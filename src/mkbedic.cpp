/**
 * @file   mkbedic.cpp
 * @brief  Makes copy of a dictionary, compressing or decompressing it.
 *         During the copy creation the words are also sorted properly.
 * @author Lyndon Hill and others
 *
 * Copyright (C) 2005 Rafal Mantiuk <rafm@users.sourceforge.net>
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <locale.h>
#include <assert.h>

#include <exception>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <set>

#include "dictionary_impl.h"
#include "utf8.h"

#define PROG_NAME "mkbedic"
#define WARNING_MSG "mkbedic warning: "

static bool verbose = false;

class QuietException
{
};

/**
 * @class XeroxException
 * @brief General exception
 */
class XeroxException : public std::exception
{
  std::string message;

public:
  explicit XeroxException(const char *reason) : message(reason)
  {
  }

  virtual ~XeroxException() throw()
  {
  }

  virtual const char *what() const throw()
  {
    return message.c_str();
  }
};

/**
 * @class DictionarySource
 * @brief A general source of dictionary entries for xerox
 */
class DictionarySource
{
public:
  virtual ~DictionarySource()
  {
  }

  // Set the file at the first dictionary entry
  virtual bool firstEntry() = 0;

  /**
   * Read the next entry, calling readEntry
   * @return file position of an entry; -1 if end of file
   */
  virtual long nextEntry(std::string &keyWord, std::string &description) = 0;

  /**
   * Actually ingest the entry data
   * @return false if EOF
   */
  virtual bool readEntry(long pos, std::string &keyWord, std::string &description) = 0;
};

// ==================================================================
// General xerox procedure
// ==================================================================

struct entry_type {
  static CollationComparator *currentDict;
  std::string word;
  CanonizedWord canonizedWord;
  int fidx;
  int pos;
  int len;
  int offset;

  entry_type(const std::string &w, const CanonizedWord &canonizedWord, int i, int p) :
        word(w), canonizedWord(canonizedWord), fidx(i), pos(p)
  {
  }

  bool operator<(const entry_type &e2) const {
    int cmp = currentDict->compare(canonizedWord, e2.canonizedWord);
    return cmp < 0;
  }
};

CollationComparator *entry_type::currentDict = nullptr;

class XeroxCollationComparator : public CollationComparator
{
  std::set<int> usedCharacters;

public:
  void checkIfCharsCollated(std::string &w)
  {
    // Check if all letters are included in char-precedence
    if(useCharPrecedence)
    {
      const char *s;
      s = w.c_str();

      while(*s != 0) {
        const char *t = s;
        int rune = Utf8::chartorune(&s);

        if(rune == 128)
          break;

        if(usedCharacters.find(rune) == usedCharacters.end())
        {
          usedCharacters.insert(rune);
          if(charPrecedence.find(rune) != charPrecedence.end())
            continue;

          if(std::find(ignoreChars.begin(), ignoreChars.end(), std::string(t, (s-t))) != ignoreChars.end())
            continue;

          // Character missing both in ignoreChars and precedence list
          std::cerr << WARNING_MSG << "character '" << std::string(t, (s-t)) <<
            "' is missing both in search-ignore-chars and char-precedence (entry " << w << ")\n";
        }
      }
    }
  }

};

/**
 * @function processXerox
 * @brief    Process the dictionary map and write the output
 */
void processXerox(XeroxCollationComparator *comparator, DictionarySource *dictSource,
                  std::map<std::string, std::string> &properties, FILE *fhOut)
{
  entry_type::currentDict = comparator;

  // Sorting
  typedef std::vector<entry_type> EntryList;
  EntryList entries;
  unsigned int mrl = 0;    // maximum entry length
  unsigned int mwl = 0;    // maximum word length
  long dsize = 0;          // dictionary size

  std::cerr << "Reading the entries ...\n";

  int n = 0;
  dictSource->firstEntry();

  // Calculate the length of the entries and read keywords

  while(true)
  {
    std::string w, s;
    long currPos = dictSource->nextEntry(w, s);
    if(currPos < 0) break;

    if(mwl < w.size()) {
      mwl = w.size();
    }

    comparator->checkIfCharsCollated(w);
    
    unsigned int d = w.size() + s.size() + 2;
    if(mrl < d) {
      mrl = d;
    }

    dsize += d;

    entries.push_back(entry_type(w, comparator->canonizeWord(w), n, currPos));
    entries[n].len = d;
    n++;
  }

  // Sort entries

  std::cerr << "Sorting ...\n";
  std::sort(entries.begin(), entries.end());

  // Check if there are duplicates
  EntryList::iterator it, it_previous;
  it_previous = entries.begin();  // silence static analysis warning
  for(it = entries.begin(); it != entries.end(); ++it) {
    if(it != entries.begin()) {
      if(comparator->compare(it->canonizedWord, it_previous->canonizedWord) == 0)
        std::cerr << WARNING_MSG << "duplicate entry '" << it->word << "'\n";
    }
    it_previous = it;
  }

  n = 0;
  for(unsigned int i = 0; i < entries.size(); i++) {
    entries[i].offset = n;
    n += entries[i].len;
  }

  std::string idx;
  n = -32769;
  char *ibuf = new char [mwl+32];
  for(unsigned int i = 0; i < entries.size() - 1; i++) {
    if(n + 32768 < entries[i].offset) {
      idx += (char) 0;
      snprintf(ibuf, mwl+32, "%s\n%d", entries[i].word.c_str(), entries[i].offset);
      idx += ibuf;
      n = entries[i].offset;
    }
  }
  delete [] ibuf;

  // Save the dictionary properties

  std::cerr << "Saving the dictionary\n";
  std::map<std::string, std::string> prop(properties);
  char buf[256];

  snprintf(buf, sizeof(buf), "%u", mrl);
  prop["max-entry-length"] = buf;
  sprintf(buf, "%u", mwl);
  prop["max-word-length"] = buf;

  if(idx.size() > 0) {
    prop["index"] = idx;
  }

  snprintf(buf, sizeof(buf), "%ld", dsize);
  prop["dict-size"] = buf;

  snprintf(buf, sizeof(buf), "%ld", (long)entries.size());
  prop["items"] = buf;

  time_t currentTime;
  time(&currentTime);
  asctime_r(localtime(&currentTime), buf);
  prop["builddate"] = buf;

  std::map<std::string, std::string>::iterator pit = prop.begin();
  for( ;pit != prop.end(); ++pit) {
    std::pair<const std::string, std::string> entry = *pit;
    int written = fprintf(fhOut, "%s=%s\x0a",
                          DictImpl::escape(entry.first).c_str(),
                          DictImpl::escape(entry.second).c_str());

    if(written < 0)
      throw XeroxException("Cannot write properties");
  }

  fwrite("\x00", 1, 1, fhOut);

  for(unsigned int i = 0; i < entries.size(); i++) {
    std::string w, s;
    dictSource->readEntry(entries[i].pos, w, s);

    fprintf(fhOut, "%s\x0a%s", w.c_str(), s.c_str());
    fwrite("\x00", 1, 1, fhOut);

    if(i % 1024 == 0) {
      std::cerr << ".";
    }
  }

  std::cerr << "\n";
}


/**
 * @class LineReader
 * @brief Read a file line by line
 */
class LineReader
{
  /// Size of line buffer
  static const int lineBufSize = 5120000;

  /// Line buffer
  char *lineBuf;

  /// Number of lines read
  int lineNo;

protected:
  /// Handle to the file
  FILE *fh;
  
public:
  /// Constructor
  explicit LineReader(FILE *fh) : lineBuf(nullptr), lineNo(0), fh(fh)
  {
    lineBuf = new char [lineBufSize];
  }

  /// Destructor
  ~LineReader()
  {
    delete [] lineBuf;
  }

  /**
   * @function readLine
   * @return   The line read
   */
  char *readLine()
  {
    char *read = fgets(lineBuf, lineBufSize, fh);
    if(read == nullptr)
      return nullptr;

    // Replace end of line (LF or CR) with Null
    int l = strlen(read);
    if(read[l-1] == 10 || read[l-1] == 13) l--;
    if(read[l-1] == 10 || read[l-1] == 13) l--;
    read[l] = 0;

    lineNo++;

    return read;
  }

  /// Get the number of lines read
  int getLineNo() const
  {
    return lineNo;
  }
};

/**
 * @function readProperties
 * @brief    Read header with properties
 */
static void readProperties(LineReader *in, std::map<std::string, std::string> &properties)
{
  while(true)
  {
    char *read = in->readLine();

    if(read == nullptr)        // EOF
      return;

    if(read[0] == 0) break;    // Empty line - no more properties

    std::string line = read;

    // Validate the line, must be of the format "name=value"
    int n = line.find('=');
    if(n < 0)
    {
      std::ostringstream msg;
      msg << "line " << in->getLineNo()
          << ": '=' missing in the property line: '"
          << read << "'";

      throw XeroxException(msg.str().c_str());
    }

    std::string name  = DictImpl::unescape(line.substr(0, n));
    std::string value = DictImpl::unescape(line.substr(n+1));
    properties[name]  = value;
  }
}

/**
 * @class TextDictSrc
 * @brief New xerox for simplified dictionary format
 */ 
class TextDictSrc : public DictionarySource, public LineReader
{
  long firstPos;

public:
  /// Constructor
  explicit TextDictSrc(FILE *fhDic) : LineReader(fhDic)
  {
  }

  /// Destructor
  virtual ~TextDictSrc()
  {
  }

  /// Bookmark the start of the dictionary entries
  void setFirstPos()
  {
    firstPos = ftell(fh);
  }

  /**
   * Set the file at the first dictionary entry
   * @return  true if successful, throws an exception if failed
   */
  bool firstEntry() override
  {
    if(fseek(fh, firstPos, SEEK_SET) != 0)
      throw XeroxException("Cannot read dictionary file (fseek failed)");

    return true;
  }

  /**
   * Read the next entry, calling readEntry
   * @param keyWord      The keyword
   * @param description  The description
   * @return  The position of the entry read or -1 if fail (end of file)
   */
  long nextEntry(std::string &keyWord, std::string &description) override
  {
    long pos = ftell(fh);
    return readEntry(pos, keyWord, description) ? pos : -1;
  }

  /**
   * Actually ingest the entry data
   * @param pos         Position in file to start reading
   * @param keyWord     The keyword (output)
   * @param description The description (output)
   * @return  true if successful, throws an exception if dictionary too long, file
   *          seek failed or a description was missing
   */
  bool readEntry(long pos, std::string &keyWord, std::string &description) override
  {
    // Check if we fit into long maximum value, -2000000 to avoid overflow before the check
    if(pos > LONG_MAX-2000000)
      throw XeroxException("Maximum dictionary length exceeded");

    if(fseek(fh, pos, SEEK_SET) != 0)
      throw XeroxException("Cannot read dictionary file (fseek failed)");

    do {
      char *read = readLine();
      if(read == nullptr)
        return false;

      keyWord = read;
    } while(keyWord.empty());

    description = "";

    while(true) {
      char *read = readLine();
      if(read == nullptr)
        break;
      if(read[0] == 0) break;
      if(!description.empty())
        description += " ";
      description += read;
    }

    if(description.empty()) {
      std::ostringstream msg;
      msg << "line " << getLineNo() << ": Missing description for item '" << keyWord << "'";
      throw XeroxException(msg.str().c_str());
    }

    return true;
  }
};


// =========== Main =============

static void printHelp() {
  std::cerr << "Usage: " PROG_NAME " [--no-header] [--header-file] [--id] "
            << "[--verbose] [--help] infile outfile\n"
            << "See the man page for more information\n";
}

static void errorCheck(bool condition, const char *description)
{
  if(!condition) {
    throw XeroxException(description);
  }
}


int main(int argc, char **argv)
{
  try {
    // Get options

    bool noHeader = false;
    char *headerFile = nullptr;
    char *id = nullptr;

    static struct option cmdLineOptions[] = {
      { "help", no_argument, nullptr, 'e' },
      { "verbose", no_argument, nullptr, 'v' },
      { "id", required_argument, nullptr, 'i' },
      { "header-file", required_argument, nullptr, 'h' },
      { "no-header", no_argument, nullptr, 'n' },
      { nullptr, 0, nullptr, 0 }
    };

    int optionIndex = 0;

    while(1) {
      int c = getopt_long(argc, argv, "vi:h:n", cmdLineOptions, &optionIndex);
      if(c == -1) break;

      switch(c) {
      case 'e':
        printHelp();
        throw QuietException();
      case 'v':
        verbose = true;
        break;
      case 'i':
        id = optarg;
        break;
      case 'h':
        headerFile = optarg;
        break;
      case 'n':
        noHeader = true;
        break;
      case 'd':                 // Ignore
        break;
      case '?':
        throw QuietException();
      case ':':
        throw QuietException();
      }
    }

    const char *sourceFileName, *destFileName;

    errorCheck(optind == argc - 2, "Both input and output file must be specified");

    sourceFileName = argv[optind++];
    destFileName = argv[optind++];

    {
      // Set up input and output

      FILE *fhDic, *fhOut;
      fhDic = fopen(sourceFileName, "r");
      errorCheck(fhDic != nullptr, "Cannot open input file for reading");
      if(!strcmp(destFileName, "-"))
        fhOut = stdout;
      else
        fhOut = fopen(destFileName, "wb");

      errorCheck(fhOut != nullptr, "Cannot open output file for writing");

      // Read properties

      std::map<std::string, std::string> properties;
      XeroxCollationComparator comparator;

      TextDictSrc source(fhDic);

      if(!noHeader)
        readProperties(&source, properties);

      source.setFirstPos();

      if(headerFile != nullptr) {
        FILE *fhHeader;
        if(!strcmp( headerFile, "-"))
          fhHeader = stdin;
        else
          fhHeader = fopen(headerFile, "r");

        errorCheck(fhHeader != nullptr, "Cannot open header file for reading");
        LineReader headerLR(fhHeader);
        readProperties(&headerLR, properties);
        if(fhHeader != stdout) fclose(fhHeader);
      }

      if(id != nullptr)
        properties["id"] = id;

      std::string precedence = properties["char-precedence"];
      std::string ic = properties["search-ignore-chars"];

      if(ic.size() == 0) {
        ic = !precedence.empty() ? "" : "-.";
        properties["search-ignore-chars"] = ic;
      }
      comparator.setCollation(precedence, ic);

      errorCheck(properties.find("id") != properties.end(),
                 "missing required 'id' property in the header");

      // Build, sort and output dictionary
      processXerox(&comparator, &source, properties, fhOut);

      // Clean up

      fclose(fhDic);
      if(fhOut != stdout) fclose(fhOut);
    }

    return EXIT_SUCCESS;
  }

  catch(QuietException &ex) {
    return EXIT_FAILURE;
  }

  catch(XeroxException &ex) {
    std::cerr << PROG_NAME ": " << ex.what() << "\n";
  }
}
