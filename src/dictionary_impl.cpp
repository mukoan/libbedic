/**
 * @file   dictionary_impl.cpp
 * @brief  
 * @author Lyndon Hill and others
 *
 * Copyright (C) 2002 Latchesar Ionkov <lionkov@yahoo.com>
 * Copyright (C) 2005 Rafal Mantiuk <rafm@users.sourceforge.net>
 *
 * This program is based on the kbedic dictionary by
 * Radostin Radnev <radnev@yahoo.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>

#include <algorithm>

#include <time.h>

#include <sstream>

#include "dictionary_impl.h"

#include "utf8.h"

unsigned char terminal_keyword[] = { 0xc2, 0xb6 };


/***************************************************************************
 *
 * Dictionary class implementation
 *
 **************************************************************************/
Dictionary *Dictionary::create(const char *filename, bool doCheckIntegrity)
{
  DictImpl *dict = new DictImpl(filename, doCheckIntegrity);

  return dict;
}

Dictionary::~Dictionary()
{
}

/***************************************************************************
 *
 * Dictionary class implementation
 *
 **************************************************************************/

const char DictImpl::DATA_DELIMITER = '\x00';
const char DictImpl::WORD_DELIMITER = '\n';

DictImpl::DictImpl(const char *filename, bool doCheckIntegrity): buf(nullptr)
{
  fileName = filename;

  compressor = nullptr;

  if(strlen(filename) > 3 && strcmp(&filename[strlen(filename) - 3], ".dz") == 0)
  {
    fdata = new DZFile();
  }
  else
  {
    fdata = new File();
  }

  if(fdata->open(filename) < 0) {
    setError(strerror(errno));
    return;
  }

  // find and set the position of the last word
  firstEntryPos = 0;
  lastEntryPos = fdata->size() - 2;
  lastEntryPos = findPrev(lastEntryPos);

  // In case the file ends with 0x00 0x10
  {
    char lastBytes[2] =
    {
      1, 1
    };
    fdata->read(fdata->size() - 2, (char*)&lastBytes, 2);

    if(lastBytes[0]==DATA_DELIMITER && lastBytes[1] == 10) {
//      fprintf( stderr, "ends with EOL; la: %d\n", lastEntryPos );
      lastEntryPos = findPrev(lastEntryPos-2);
//      fprintf( stderr, "la: %d\n", lastEntryPos );
    }
  }

  // read dictionary header
  // set the position of the first word
  firstEntryPos = readProperties();
  currPos = firstEntryPos;

  buf = new char[maxEntryLength];

  // fix the indices
  for(std::vector<IndexEntry>::iterator it = index.begin(); it != index.end(); ++it) {
    (*it).pos += firstEntryPos;
  }

  // check the integrity
  if(doCheckIntegrity) {
    checkIntegrity();
  }

  nextPos = -1;
}

DictImpl::~DictImpl()
{
  if(buf) {
    delete []buf;
  }

  delete fdata;
}

const std::string &DictImpl::getName() const
{
  return name;
}

const std::string &DictImpl::getFileName() const
{
  return fileName;
}


bool DictImpl::findEntry(const std::string &w, bool &subword)
{
  long b, e, m;
  bool found;
  CanonizedWord cw;

  b = firstEntryPos;
  e = lastEntryPos;

  CanonizedWord word = canonizeWord(w);

//  struct timeval tv;
//  gettimeofday(&tv, NULL);
//  fprintf(stderr, "findEntry: > %s %015ld %015ld\n", word.c_str(), tv.tv_sec, tv.tv_usec);

  // First search the index
  bsearchIndex(word, b, e);
//  printf("findEntry: b=%ld, e=%ld\n", b, e);

  // If index is dense, no need to search further
  if(b >= e)
  {
    readEntry(b);
    cw = canonizeWord(currWord);
    found = compare(word, cw) == 0;
  }
  else
  {
    found = false;
  }

  // Binary search on dictionary
  while(b < e)
  {
    // operation on unsiged numbers to save one more bit
    m = (long)(((unsigned long)b+(unsigned long)e)/2);
    m = findPrev(m);
    if((m < 0) || !readEntry(m))
    {
      currWord  = string();
      currSense = string();
      senseCompressed = false;
      currPos = firstEntryPos;
      return false;
    }

    cw = canonizeWord(currWord);
//  printf("findEntry: compare %s:%s\n", word.c_str(), cw.c_str());
    int cmp = compare(word, cw);
    if(cmp == 0)
    {
      found = true;
      break;
    }
    else if(cmp < 0)
    {
      e = currPos;
    }
    else
    {
      b = findNext(m+1);
    }
  }

  if(!found)
  {           // findNext(m+1) can move position to the matching word
    readEntry(b);
    cw = canonizeWord(currWord);
    int cmp = compare(word, cw);
    found = cmp == 0;
    // Fix disabled because it was rather counterintuitive
//           if (cmp < 0) {        // always show entry which is lexically smaller then 'word'
//             long pos;
//             pos = findPrev(currPos-2);
//             readEntry(pos);
//             cw = canonizeWord(currWord);
//           }
  }

  subword = false; //TODO: fix it: cw.substr(0, word.size()) == word;
// printf("findEntry: meaning=%s\n", currSense.c_str());

// gettimeofday(&tv, NULL);
// fprintf(stderr, "findEntry: < %015ld %015ld\n", tv.tv_sec, tv.tv_usec);
  return found;
}

bool DictImpl::nextEntry()
{
  long pos;

  if(nextPos > 0) {
    pos = nextPos;
  } else {
    pos = findNext(currPos+1);
  }

  if(pos > lastEntryPos) {
    pos = lastEntryPos;
  }

  if(pos == currPos) {
    return false;
  }

  return readEntry(pos);
}

bool DictImpl::firstEntry()
{
  return readEntry(firstEntryPos);
}

bool DictImpl::lastEntry()
{
  return readEntry(lastEntryPos);
}

bool DictImpl::randomEntry()
{
  return readEntry(findNext(firstEntryPos + (long)
                     ((((double) lastEntryPos) * rand()) /
                       (RAND_MAX + (double) firstEntryPos))));
}

const std::string &DictImpl::getWord() const
{
  return currWord;
}

const std::string &DictImpl::getSense() const
{
  if(senseCompressed) {
    currSense = unescape(currSense);
    currSense = compressor->decode(currSense);
    senseCompressed = false;
  }

  return currSense;
}

void DictImpl::bsearchIndex(const CanonizedWord &s, long &b, long &e)
{
  int ib, ie, m;

  ib = m = 0;
  ie = index.size() - 1;

  if(ib >= ie) {
    return;
  }

  while(ib < ie) {
    m = (ib+ie) / 2;
    int cmp = compare(s, index[m].word);
//  printf("bsearchIndex: compare %s:%s\n", (const char*) s.utf8(),
//         (const char*) index[m]->word.utf8());

    if(cmp == 0) {
      break;
    } else if(cmp < 0) {
      ie = m;
    } else {
      ib = m+1;
      m++;
    }
  }

// Can not happen
  if((int)index.size() <= m) {
    assert(0);
          
//  b = lastEntryPos;
//  e = lastEntryPos;
  } else {
//  printf("bsearchIndex: compare %s:%s\n", (const char *) s.utf8(), 
//         (const char *) index[m]->word.utf8());
    if(compare(s, index[m].word) < 0 && m > 0) {
      m--;
    }

    b = index[m].pos;
    if((m+1) < (int) index.size()) {
      e = index[m+1].pos;
    } else {
      e = lastEntryPos;
    }
  }
}

bool DictImpl::readEntry(long pos)
{
  int clen = maxEntryLength / 4;
  int n = 0;

  // find the start position of the entry
  if(pos > lastEntryPos) {
    pos = lastEntryPos;
  } 

  currPos = pos;
  nextPos = -1;
  char *pp = 0;

  // read the entry
  while(n < maxEntryLength) {
    if(n+clen > maxEntryLength) {
      clen = maxEntryLength - n;
    }

    int i = fdata->read(currPos + n, &buf[n], clen);
    if(i < 0) {
      setError(strerror(errno));
      return false;
    } else if(i == 0) {
      break;
    }

    pp = (char *) memchr(&buf[n], DATA_DELIMITER, i);
    if(pp != 0) {
      break;
    }
    n += i;
  }

  if(pp == 0) {
    setError("entry too long");
    return false;
  }

  char *p = (char *) memchr(buf, WORD_DELIMITER, pp-buf);
  if(p == 0) {
    std::stringstream s;
    s << "readEntry: invalid entry format. Entry content: '"
      << buf
      << "'";
    setError(s.str());
    return false;
  }
  *p = '\0';
  currWord = string(buf);
  if(compressor) {
    currWord = unescape(currWord);
    currWord = compressor->decode(currWord);
  }

  *pp = '\0';
  nextPos = currPos + (pp-buf) + 1;

  currSense=string(p+1);
  senseCompressed=false;
  if(compressor) {
    senseCompressed=true;
//  currSense = unescape(currSense);
//  currSense = compressor->decode(currSense);
  }

//  printf("readEntry: currPos=%ld, nextPos=%ld, currSense=%s, currWord=%s\n",
//         currPos, nextPos, currSense.c_str(), currWord.c_str());

// printf(">>> %s\n", currWord.c_str());

  return true;
}

long DictImpl::findPrev(long pos)
{
  char s[256];

  if(pos < firstEntryPos) {
    return firstEntryPos;
  }

  if(pos > lastEntryPos) {
    return lastEntryPos;
  }

  long n = pos;

  while(n > firstEntryPos) {
    int len = sizeof(s);
    if((n-len) < firstEntryPos) {
      len = n - firstEntryPos + 1;
    }

    int k = fdata->read(n - len + 1, s, len);
    if(k != len) {
      setError(strerror(errno));
      return -1;
    }

    for(int i = len - 1; i >= 0; i--) {
      if(s[i] == DATA_DELIMITER) {
        return (n-len) + i + 2;
      }
    }

    n -= len;
  }

  return firstEntryPos;
}

long DictImpl::findNext(long pos)
{
  char s[256];

  if(pos < firstEntryPos) {
    return firstEntryPos;
  }

  if(pos > lastEntryPos) {
    return lastEntryPos;
  }

  while(1) {
    int n = fdata->read(pos, s, sizeof(s));

    if(n < 0) {
      setError(strerror(errno));
      return false;
    }

    if(n == 0) {
      setError("internal error");
      return -1;
    }

    char *p = (char *) memchr(s, DATA_DELIMITER, n);
    if(p != 0) {
      pos += (p - s) + 1;
      break;
    }

    pos += n;
  }

  return pos;
}



int DictImpl::readProperties()
{
  properties.clear();
  int pos = 0;

  while(1) {
    std::string line;

    int lineSize = getLine(line, pos);
    if(lineSize <= 0 || line[0] == DATA_DELIMITER) {
      break;
    }

    int n = line.find('=');
    if(n < 0) {
      continue;
    }

    std::string name = unescape(line.substr(0, n));
    std::string value = unescape(line.substr(n+1));
    properties[name] = value;
  }

  // get dictionary name
  name = properties["id"];
  
  // get the character precedence used for searching and sorting
  std::string precedence = properties["char-precedence"];
  
  // get the chars that are ignored while searching
  std::string ic = properties["search-ignore-chars"];  

  if(ic.size() == 0) {
    ic = !precedence.empty() ? "" : "-.";
    properties["search-ignore-chars"] = ic;
  }

  setCollation(precedence, ic);
        
  // get the maximum length of a word
  maxWordLength = 50;
  string ns = properties["max-word-length"];
  if(ns.size() != 0) {
    char *eptr;
    int n = strtol(ns.c_str(), &eptr, 0);
    if(*eptr == '\0') {
      maxWordLength = n + 5;
    }
  }

  // get the maximum length of word record
  // (word and sense including)
  maxEntryLength = 16384;
  ns = properties["max-entry-length"];
  if(ns.size() != 0) {
    char *eptr;
    int n = strtol(ns.c_str(), &eptr, 0);
    if(*eptr == '\0') {
      maxEntryLength = n + 10;
    }
  }

  // read compression method
  ns = properties["compression-method"];
  if(ns.size() == 0) {
    ns = "none";
  }

  if(ns == "shcm") {
    compressor = SHCM::create();
    ns = properties["shcm-tree"];
    if(ns.size() == 0) {
      setError("no shcm tree");
      return 0;
    }

    // this is unnecessary second unescape
    // i am leaving it for now for backward compatibility
    // with already broken dictionaries 
    ns = unescape(ns);
    compressor->startDecode(ns);
  }

  // read the index
  ns = properties["index"];
// printf("index=%s\n", ns.c_str() + 1);

  int i, n;
  n = 0;
  do {
    i = ns.find((char) 0, n+1);
    if(i < 0) {
      i = ns.size();
    }

    if (i == n) {
      break;
    }

    std::string idx(ns, n+1, i-n);
    int k = idx.find('\n');
    std::string word(idx, 0, k);
    std::string spos(idx, k+1);
    char *eptr;

    long l = strtol(spos.c_str(), &eptr, 0);
    if(*eptr != '\0') {
//  printf("error\n");
      index.clear();
      break;
    }

    index.push_back(IndexEntry(canonizeWord(word), l));
//  printf("%s:%ld\n", word.c_str(), l);

    n = i;
  } while (n < (int) ns.size());

  properties.erase("index");
  return pos;
}

int DictImpl::getLine(std::string &line, int &pos)
{
  char buf[90];
  int i, n;
  int p;

  line.erase();
  p = pos;
  while(1) {
    n = fdata->read(p, buf, sizeof(buf));
    if(n < 0) {
      return 0;
    } else if (n == 0) {
      break;
    }

    for(i = 0; i < n; i++) {
      if(buf[i] == 0) {
        pos += line.size() + i + 1;
        return 0;
      }

      if(buf[i] == '\n') {
        break;
      }
    }

    line.append(buf, i);
    if(i < n) {
      break;
    }

    p += n;
  }

  pos += line.size() + 1;
  return line.size();
}

bool DictImpl::checkIntegrity() {

  // first check if the last character in the file is zero or
  // the last two characters are 0 and 10. This is because most
  // of the text editors insert EOL at the end of the file
  char lastBytes[2] = 
  {
    1, 1
  };
  fdata->read(fdata->size() - 2, (char*)&lastBytes, 2);

  if(lastBytes[1] != 0 && (lastBytes[0] !=0 || lastBytes[1] != 10)) {
    setError("Integrity failure");
    return false;
  }

  // check the file size (if specified in the header)
/*  string s = properties["dict-size"];
  if (s.size() != 0) {
        const char* ss = s.c_str();
        char* sptr;
        printf(">>> %s\n", ss);
        long n = strtol(ss, &sptr, 0);
        if (*sptr == '\0') {
        if (n != (pos - firstEntryPos + 1)) {
        return false;
        }
        }
  }
*/

  // check if the index entry positions point to correct places
  int step = index.size() / 7;
  if(step <= 0) {
    step = 1;
  }

  for(unsigned int i = 0; i < index.size(); i += step) {
    char c = 12;
    IndexEntry ie = index[i];
    fdata->read(ie.pos - 1, &c, 1);
    if(c != 0) {
      setError("Integrity failure: index corrupted");
      return false;
    }
  }

  return true;
}

std::string DictImpl::escape(const std::string &str)
{
  const char *s = str.c_str();
  std::string ret;

  for(unsigned int i = 0; i < str.size(); i++) {
    switch(s[i]) {
    case WORD_DELIMITER:
      ret.push_back(27);
      ret.push_back('n');
      break;

    case 27:
      ret.push_back(27);
      ret.push_back('e');
      break;

    default:
      if(s[i] == DATA_DELIMITER) {
        ret.push_back(27);
        ret.push_back('0');
      } else
        ret.push_back(s[i]);
    }
  }

  return ret;
}

std::string DictImpl::unescape(const std::string &str) {
  const char *s = str.c_str();
  std::string ret;

  for(unsigned int i = 0; i < str.size(); i++) {
    if(s[i] == 27) {
      i++;
      switch (s[i]) {
      case '0':
        ret.push_back(DATA_DELIMITER);
        break;

      case 'n':
        ret.push_back(WORD_DELIMITER);
        break;

      case 'e':
        ret.push_back(27);
        break;
      }
    } else {
      ret.push_back(s[i]);
    }
  }

  return ret;
}

// ==================================================================
// Collation comparator
// ==================================================================

CanonizedWord CollationComparator::canonizeWord(const std::string &word)
{
  std::string s = word;
  for(unsigned int i = 0; i < ignoreChars.size(); i++) {
    int n;
    while((n = s.find(ignoreChars[i])) >= 0) {
      s.erase(n, ignoreChars[i].size());
    }
  }

  CanonizedWord ss;
//  ss.reserve( maxWordLength );
  const char *sPtr = s.c_str();
  while(*sPtr != 0) {
    int rune = Utf8::chartorune(&sPtr);
    if(rune == 128) break;

    if(useCharPrecedence) {
      std::map<int, int>::iterator itcol = charPrecedence.find(rune);
      if(itcol == charPrecedence.end())
        ss.push_back((unsigned short)(charPrecedenceUnknown + rune));
      else
        ss.push_back(itcol->second);
    }
    else
      ss.push_back(Utf8::runetoupper(rune));
  }
  return ss;
}

int CollationComparator::compare(const CanonizedWord &s1, const CanonizedWord &s2)
{
  CanonizedWord::const_iterator it1 = s1.begin();
  CanonizedWord::const_iterator it2 = s2.begin();
  if(useCharPrecedence) {
    for( ;it1 != s1.end() && it2 != s2.end(); ++it1, ++it2) {
      // handle characters that are not defined in the collation string
      int ind1 = *it1 >= charPrecedenceUnknown ? charPrecedenceUnknown : *it1;
      int ind2 = *it2 >= charPrecedenceUnknown ? charPrecedenceUnknown : *it2;
      if(precedenceGroups[ind1] < precedenceGroups[ind2]) return -1;
      if(precedenceGroups[ind1] > precedenceGroups[ind2]) return 1;
    }
    if(it1 == s1.end() && it2 == s2.end()) {
      for(it1 = s1.begin(), it2 = s2.begin(); it1 != s1.end() && it2 != s2.end(); ++it1, ++it2) {
        if(*it1 < *it2) return -1;
        if(*it1 > *it2) return 1;
      }
      return 0;
    }
    if(it1 == s1.end()) return -1;
    return 1;
  } else {                      // No char precedence
    for( ;it1 != s1.end() && it2 != s2.end(); ++it1, ++it2) {
      if(*it1 < *it2) return -1;
      if(*it1 > *it2) return 1;
    }
    if(it1 == s1.end() && it2 == s2.end()) return 0;
    if(it1 == s1.end()) return -1;
    return 1;
  }
}

void CollationComparator::setCollation(const std::string &collationDef,
                                       const std::string &ic)
{
  int order = 0;
  if(collationDef.size() != 0) {
    const char *s;
    s = collationDef.c_str();
    bool isGroup = false;
    int precGroup = 1;
    while(*s != 0) {
      int rune = Utf8::chartorune(&s);
      if(rune == 128) 
        break;
      if(rune == '{') {
        isGroup = true;
        continue;
      }
      if(rune == '}') {
        isGroup = false;
        precGroup++;
        continue;
      }
      charPrecedence[rune] = order++;
      precedenceGroups.push_back(precGroup);
      if(!isGroup)
        precGroup++;
    }
    precedenceGroups.push_back(precGroup++);
    charPrecedenceUnknown = order++;
    { // Add terminal keyword
      const char *tk = (char *)(terminal_keyword);  // FIXME do a proper C++ cast here
      int rune = Utf8::chartorune(&tk);
      precedenceGroups.push_back(precGroup);
      charPrecedence[rune] = order;
    }
    useCharPrecedence = true;

//     map<int,int>::iterator it = charPrecedence.begin();
//     for( ; it != charPrecedence.end(); it++ ) {
//       std::cout << it->first << " - " << it->second << " gr.: " << precedenceGroups[it->second] << "\n";
//     }
  } else
    useCharPrecedence = false;
  
  const char *s, *t;
  s = ic.c_str();

  while(*s != 0) {
    t = s;
    if(Utf8::chartorune(&s) == 128) {
      break;
    }

    ignoreChars.push_back(string(t, (s-t)));
  }
  
}
