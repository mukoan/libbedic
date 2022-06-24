/**
 * @file   xerox.cpp
 * @brief  Makes copy of a dictionary, compressing or decompressing it.
 *         During the copy creation the words are also sorted properly.
 * @author Lyndon Hill and others
 *
 * Copyright (C) 2002 Latchesar Ionkov <lionkov@yahoo.com>
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
#include <exception>
#include <iostream>
#include <getopt.h>
#include <sys/stat.h>

#include <algorithm>
#include <sstream>
#include <set>

#include <locale.h>

#include "dictionary_impl.h"
#include "utf8.h"

#include <assert.h>

#define PROG_NAME "xerox"

#define WARNING_MSG "xerox warning: "

static bool verbose = false;

class QuietException 
{
};


class XeroxException: public std::exception
{
  string message;
public:
  XeroxException( const char *message ): message( message )
  {
  }

  virtual ~XeroxException() throw()
  {
  }
  
  
  virtual const char *what () const throw ()
  {
    return message.c_str();
  }
  
};

// ==================================================================
// Structures used in xerox
// ==================================================================

struct entry_type {
  static CollationComparator *currentDict;
  string word;
  CanonizedWord canonizedWord;
  int fidx;
  int pos;
  int len;
  int offset;

  entry_type(const string &w, const CanonizedWord &canonizedWord, int i, int p):
    word( w ), canonizedWord( canonizedWord ), fidx( i ), pos( p )
  {
  }
		
  bool operator<(const entry_type& e2) const {
    int cmp = currentDict->compare( canonizedWord, e2.canonizedWord );
    return cmp < 0;    
  }
};

CollationComparator *entry_type::currentDict = NULL;

class XeroxCollationComparator: public CollationComparator
{
  set<int> usedCharacters;  
public:
  void checkIfCharsCollated( string &w )
  {    
    //Check if all letters are included in char-precedence
    if( useCharPrecedence ) {
      const char *s, *t;
      s = w.c_str();
      while (*s != 0) {
        t = s;
        int rune = Utf8::chartorune(&s);
        if( rune == 128) 
          break;
        if( usedCharacters.find( rune ) == usedCharacters.end() ) {
          usedCharacters.insert( rune );
          if( charPrecedence.find( rune ) != charPrecedence.end() )
            continue;
          if( find( ignoreChars.begin(), ignoreChars.end(), string(t, (s-t)) ) != ignoreChars.end() )
            continue;
          // Character missing both in ignoreChars and precedence list
          std::cerr << WARNING_MSG << "character '" << string(t, (s-t)) <<
            "' is missing both in search-ignore-chars and char-precedence\n";          
        }
      }
    }
  }
  
};


// ==================================================================
// Old xerox 
// ==================================================================

class XeroxDict : public DictImpl {

  /* bedic is missing exception mechanism. checkIfError() checks bedic
   * status and throws XeroxException on error.*/
  void checkIfError();  
  
public:

  XeroxDict(const char* filename) : DictImpl( filename, false )
  {
  }

  /**
   * Creates new dictionary file with the same 
   * database of words, but different compression method
   *
   * @param filename the filename of the new dictionary file
   * @param compression_method compression method
   *	currently only "none" and "shcm" are allowed as 
   * 	compression methods
   */  
  bool xerox(const string& filename, const string& compress_method, 
    bool sort = true);

  /**
   * Creates new dictionary file with the same 
   * database of words, but different compression method
   *
   * @param fd the file descriptor
   * @param compression_method compression method
   *	currently only "none" and "shcm" are allowed as 
   * 	compression methods
   */  
  bool xerox(int fd, const string& compress_method, 
    bool do_sort = true);

  vector<string> findAllCharacters( void );  
  
};

bool XeroxDict::xerox(const string& filename, const string& compress_method, 
  bool sort) {

  int fd = creat(filename.c_str(), S_IREAD | S_IWRITE);

  if (fd < 0) {
    return false;
  }

  bool ret = xerox(fd, compress_method, sort);

  close(fd);

  if (!ret) {
    unlink(filename.c_str());
  }

  return ret;
}

void XeroxDict::checkIfError() {
  string err = getError();
  if( err.size() != 0 ) {
    ostringstream msg;
    msg << "Bedic error: " << err; 
    throw XeroxException( msg.str().c_str() );  
  }
    
}

bool XeroxDict::xerox(int fd, const string& compress_method, 
  bool do_sort) {

  entry_type::currentDict = this;

  checkIfError();
  
  static char wdelim[] = { WORD_DELIMITER };
  static char ddelim[] = { '\0' };
  static char eql[] = { '=' };
  static char nl[] = { '\n' };

  SHCM* compr = 0;
  string s;
  string shcm_tree;

  if (compress_method == "shcm") {
    compr = SHCM::create();
  }

  // sorting
  typedef vector<entry_type> EntryList;
  EntryList entries;
  unsigned int mrl = 0;
  unsigned int mwl = 0;
  long dsize = 0;

  set<int> usedCharacters;
  
  fprintf(stderr, "Reading the entries ...\n");
  int n = 0;
  firstEntry();
  do {
    checkIfError();

    // Check if we fit into long maximum value, -2000000 to avoid overflow before the check
    if( currPos > LONG_MAX-2000000 )
      throw XeroxException( "Maximum dictionary length exceeded" );
    
    string w = getWord();
    if (mwl < w.size()) {
      mwl = w.size();
    }
//    fprintf( stderr, "%s\n", w.c_str() );

    //Check if all letters are includec in char-precedence
    if( useCharPrecedence ) {
      const char *s, *t;
      s = w.c_str();
      while (*s != 0) {
        t = s;
        int rune = Utf8::chartorune(&s);
        if( rune == 128) 
          break;
        if( usedCharacters.find( rune ) == usedCharacters.end() ) {
          usedCharacters.insert( rune );
          if( charPrecedence.find( rune ) != charPrecedence.end() )
            continue;
          if( find( ignoreChars.begin(), ignoreChars.end(), string(t, (s-t)) ) != ignoreChars.end() )
            continue;
          // Character missing both in ignoreChars and precedence list
          std::cerr << WARNING_MSG << "character '" << string(t, (s-t)) <<
            "' is missing both in search-ignore-chars and char-precedence\n";          
        }
      }
    }
    
    unsigned int d = w.size() + getSense().size() + 2;
    if (mrl < d) {
      mrl = d;
    }

    if (compr != 0) {
      compr->preencode(w);
      compr->preencode(getSense());
    }

    entries.push_back(entry_type(w, canonizeWord( w ), n, currPos));
    n++;
  } while (nextEntry());

  // calculate the length of the entries
  n = 0;
  firstEntry();
  do {
    checkIfError();    
    
    string w = getWord();
    string s = getSense();
    if (compr != 0) {
      w = escape(compr->encode(w));
      s = escape(compr->encode(s));
    }
    
    if (mwl < w.size()) {
      mwl = w.size();
    }

    unsigned int d = w.size() + s.size() + 2;
    if (mrl < d) {
      mrl = d;
    }

    dsize += d;

    entries[n].len = d;
    n++;		
  } while (nextEntry());

  // Sort entries
  if (do_sort) {
    fprintf(stderr, "Sorting ...\n");
    sort(entries.begin(), entries.end());

    //Check if there are duplicates
    EntryList::iterator it, it_previous;
    for( it = entries.begin(); it != entries.end(); it++ ) {
      if( it != entries.begin() ) {
        if( compare( it->canonizedWord, it_previous->canonizedWord )==0 )
          std::cerr << WARNING_MSG << "duplicate entry '" << it->word << "'\n";
      }
      it_previous = it;
    }    
  }

  n = 0;
  for(unsigned int i = 0; i < entries.size(); i++) {
    entries[i].offset = n;
    n += entries[i].len;
  }

  string idx;
  n = -32769;
  char* ibuf = new char[mwl+32];
  for(unsigned int i = 0; i < entries.size() - 1; i++) {
    if (n + 32768 < entries[i].offset) {
      idx += (char) 0;
      snprintf(ibuf, mwl+32, "%s\n%d", entries[i].word.c_str(), entries[i].offset);
      idx += ibuf;
      n = entries[i].offset;
    }
  }
  delete []ibuf;

  // saving the dictionary properties
  fprintf(stderr, "Saving the dictionary\n");
  map<string, string> prop(properties);
  char buf[256];

  snprintf(buf, sizeof(buf), "%d", mrl);
  prop["max-entry-length"] = buf;
  sprintf(buf, "%d", mwl);
  prop["max-word-length"] = buf;
  prop["compression-method"] = compress_method;
  if (compr != 0) {
    // this is unnecessary second escape
    // i am leaving it for now for backward compatibility
    // with already broken dictionaries 
    prop["shcm-tree"] = escape(shcm_tree); 
  } else {
    prop.erase("shcm-tree");
  }

  if (idx.size() > 0) {
    prop["index"] = idx;
  }

  snprintf(buf, sizeof(buf), "%ld", dsize);
  prop["dict-size"] = buf;

  snprintf(buf, sizeof(buf), "%ld", (long)entries.size());
  prop["items"] = buf;

  time_t currentTime;
  time( &currentTime );
  asctime_r(localtime( &currentTime ), buf );
  prop["builddate"] = buf;
  
  map<string, string>::iterator pit = prop.begin();
  while (pit != prop.end()) {
    pair<const string, string> entry = *pit;
    const char* s = escape(entry.first).c_str();
    unsigned int n = write(fd, s, strlen(s));
    if (n != strlen(s)) {
      return false;
    }
    n = write(fd, eql, sizeof(eql));
    if (n != sizeof(eql)) {
      return false;
    }

    string es = escape(entry.second);
    s = es.c_str();
    n = write(fd, s, strlen(s));
    if (n != strlen(s)) {
      return false;
    }
    n = write(fd, nl, sizeof(nl));
    if (n != sizeof(nl)) {
      return false;
    }
    ++pit;
  }

  buf[0] = 0;
  write(fd, buf, 1);

  for(unsigned int i = 0; i < entries.size(); i++) {
    readEntry(entries[i].pos);
    checkIfError();

    string w = getWord();
    string s = getSense();
    if (compr != 0) {
      w = escape(compr->encode(w));
      s = escape(compr->encode(s));
    }

    write(fd, w.c_str(), w.size());
    write(fd, wdelim, sizeof(wdelim));
    write(fd, s.c_str(), s.size());
    write(fd, ddelim, sizeof(ddelim));

    if (i % 1024 == 0) {
      fprintf(stderr, ".");
    }
  }
  fprintf(stderr, "\n");

  return true;
}


// ==================================================================
// Generate character precedence
// ==================================================================

int collate_compare(const void *va, const void *vb)
{
  const char *a = *((const char**)va);
  const char *b = *((const char**)vb);

  return strcoll( a, b );
}

//struct CollateCmp: public binary_function< const char*, const char*, int >

struct CollateCmp : public binary_function<string,string,bool>
{
  bool operator()(const string & x, const string& y) const
  {
    return strcoll( x.c_str(), y.c_str() ) < 0;
  }
};

vector<string> XeroxDict::findAllCharacters( void )
{
  
  cerr << "Reading the entries and looking for all letters...\n";
  set<string> foundLetters;
  firstEntry();
  do {
    string w = getWord();
    checkIfError();
    const char *cBeg, *cEnd;
    cBeg = w.c_str();
//    fprintf( stderr, "%s\n", w.c_str() );
    while( *cBeg != 0 ) {
      cEnd = cBeg;
      int rune = Utf8::chartorune(&cEnd);
      if( rune == 128) 
        break;

      int len = cEnd-cBeg;
      assert( len <= 4 && len >= 1 );
      char letter[5];
      strncpy( letter, (char*)cBeg, len );
      letter[len]=0;
      if( foundLetters.find( letter ) == foundLetters.end() ) {
        foundLetters.insert( string(letter) );
//        fprintf( stderr, "%s: %s\n", w.c_str(), letter );
      }
      cBeg = cEnd;
    }
  } while (nextEntry());

  vector<string> letterVec(foundLetters.begin(), foundLetters.end());
//   map<string, bool>::iterator it;
//   for( it = foundLetters.begin(); it != foundLetters.end(); it++ )
//     letterVec.push_back( it->first );  

  return letterVec;
}



// =========== Main =============

static void printHelp() {
  std::cerr << "Usage: " PROG_NAME " [-d] [--generate-char-precedence] [--verbose] [--help] infile"
 " [outfile]\nSee the man page for more information\n";
}

static void errorCheck( bool condition, char *string )
{
  if( !condition ) {
    throw XeroxException( string );
  }
}


int main(int argc, char** argv) {

  bool generateCharPrecedence = false;
  
  try {
    const char* cmth = "none";
    char *localeForCharPrec;

    static struct option cmdLineOptions[] = {
      { "help", no_argument, NULL, 'h' },
      { "verbose", no_argument, NULL, 'v' },
      { "generate-char-precedence", required_argument, NULL, 'g' },
      { NULL, 0, NULL, 0 }
    };

    int optionIndex = 0;
    while( 1 ) {
      int c = getopt_long(argc, argv, "hvdg:s", cmdLineOptions, &optionIndex);
      if( c == -1 ) break;
      switch( c ) {
      case 'h':
        printHelp();
        throw QuietException();
      case 'v':
        verbose = true;
        break;
      case 'g':
        generateCharPrecedence = true;
        localeForCharPrec = optarg;
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

    if( generateCharPrecedence )
      errorCheck( optind == argc - 1, "A single dictionary file must be specified" );
    else
      errorCheck( optind == argc - 2, "Both input and output file must be specified" );

    sourceFileName = argv[optind++];
    destFileName = argv[optind++];
    
    XeroxDict* dict = new XeroxDict( sourceFileName ); 
    
    if (dict == 0)
      throw XeroxException( "Cannot read the dictionary" );

    if( generateCharPrecedence ) {

      // Set locale for LC_COLLATE
      char *oldloc = setlocale( LC_COLLATE, localeForCharPrec );
      if( oldloc == NULL ) {
        ostringstream msg;
        msg << "Can not set locale '" << localeForCharPrec << "'";
        throw XeroxException( msg.str().c_str() );
      }

      // Find all letters used for key-words in the dictionary
      vector<string> usedCharacters = dict->findAllCharacters();

      // Sort letters using collation
      CollateCmp cmp;
      sort( usedCharacters.begin(), usedCharacters.end(), cmp );      

      // Output the result
      cout << "char-precedence=";
      for( unsigned int i = 0; i < usedCharacters.size(); i++ )
        cout << usedCharacters[i];
      cout << "\n";
      
    } else {                    // Normal xerox mode
      if( !strcmp( destFileName, "-" ) ) { // stdout
        dict->xerox((int)1, cmth);
      } else {
        dict->xerox(destFileName, cmth);
      }
    }
    
    return EXIT_SUCCESS;
  }
  catch( QuietException  ex ) {
    return EXIT_FAILURE;
  }
  catch( XeroxException ex ) {
    std::cerr << PROG_NAME ": " << ex.what() << "\n";
  }
  

}
