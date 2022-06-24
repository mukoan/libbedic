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

#define PROG_NAME "mkbedic"

#define WARNING_MSG "mkbedic warning: "

static bool verbose = false;

class QuietException 
{
};


class XeroxException: public std::exception
{
  std::string message;
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

  virtual bool firstEntry() = 0;

  /**
   * @return file position of an entry; -1 if end of file
   */
  virtual long nextEntry( string &keyWord, string &description ) = 0;

  /**
   * @return false if EOF
   */
  virtual bool readEntry( long pos, string &keyWord, string &description ) = 0;
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

  entry_type(const std::string &w, const CanonizedWord &canonizedWord, int i, int p):
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
            "' is missing both in search-ignore-chars and char-precedence (entry " << w << ")\n";
        }
      }
    }
  }

};

void processXerox( XeroxCollationComparator *comparator, DictionarySource *dictSource,
  map<string, string> &properties, FILE *fhOut )
{
  entry_type::currentDict = comparator;

//  string s;
  string shcm_tree;

  // sorting
  typedef vector<entry_type> EntryList;
  EntryList entries;
  unsigned int mrl = 0;
  unsigned int mwl = 0;
  long dsize = 0;

  fprintf(stderr, "Reading the entries ...\n");
  int n = 0;
  dictSource->firstEntry();
  // calculate the length of the entries and read key-words
  while( true ) {
    string w, s;
    long currPos = dictSource->nextEntry( w, s );
    if( currPos < 0 ) break;

    if (mwl < w.size()) {
      mwl = w.size();
    }

    comparator->checkIfCharsCollated( w );
    
    unsigned int d = w.size() + s.size() + 2;
    if (mrl < d) {
      mrl = d;
    }

    dsize += d;
    
    entries.push_back(entry_type(w, comparator->canonizeWord( w ), n, currPos));
    entries[n].len = d;
    n++;
  }

  // Sort entries
  {
    fprintf(stderr, "Sorting ...\n");
    sort(entries.begin(), entries.end());

    //Check if there are duplicates
    EntryList::iterator it, it_previous;
    for( it = entries.begin(); it != entries.end(); it++ ) {
      if( it != entries.begin() ) {
        if( comparator->compare( it->canonizedWord, it_previous->canonizedWord )==0 )
          std::cerr << WARNING_MSG << "duplicate entry '" << it->word << "'\n";
      }
      it_previous = it;
    }
  }

  n = 0;
  for(unsigned int i = 0; i < entries.size(); i++) {    entries[i].offset = n;
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

  snprintf(buf, sizeof(buf), "%u", mrl);
  prop["max-entry-length"] = buf;
  sprintf(buf, "%u", mwl);
  prop["max-word-length"] = buf;

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
  for( ;pit != prop.end(); ++pit) {
    pair<const string, string> entry = *pit;
    int written = fprintf( fhOut, "%s=%s\x0a",
      DictImpl::escape(entry.first).c_str(), DictImpl::escape(entry.second).c_str() );
    if( written < 0 )
      throw XeroxException( "Cannot write properties" );
  }

  fwrite( "\x00", 1, 1, fhOut );

  for(unsigned int i = 0; i < entries.size(); i++) {
    string w, s;
    dictSource->readEntry( entries[i].pos, w, s );

    fprintf( fhOut, "%s\x0a%s", w.c_str(), s.c_str() );
    fwrite( "\x00", 1, 1, fhOut );

    if (i % 1024 == 0) {
      fprintf(stderr, ".");
    }
  }
  fprintf(stderr, "\n");

}


// ==================================================================
// Read a file line by line
// ==================================================================

class LineReader
{
  static const int lineBufSize = 5120000;
  char *lineBuf;
  int lineNo;

protected:
  FILE *fh;
  
public:
  LineReader( FILE *fh ): lineBuf( NULL ), lineNo( 0 ), fh( fh )
  {
    lineBuf = new char[lineBufSize];
  }

  ~LineReader()
  {
    delete[] lineBuf;
  }
  
  char *readLine()
  {
    char *read = fgets( lineBuf, lineBufSize, fh );
    if( read == NULL )
      return NULL;
    
    int l = strlen( read );
    if( read[l-1] == 10 || read[l-1] == 13 ) l--;
    if( read[l-1] == 10 || read[l-1] == 13 ) l--;
    read[l] = 0;

    lineNo++;

    return read;
  }

  int getLineNo()
  {
    return lineNo;
  }

};

// ==================================================================
// Read header with properties
// ==================================================================

static void readProperties( LineReader *in, map<string, string> &properties )
{
  while( true ) {
    char *read = in->readLine();

    if( read == NULL )        // EOF
      return;

    if( read[0] == 0 ) break; // Empty line - no more properties
    
    string line = read;
    int n = line.find('=');
    if (n < 0) {
      ostringstream msg;
      msg << "line " << in->getLineNo() << ": '=' missing in the property line: '" << read << "'"; 
      throw XeroxException( msg.str().c_str() );
    }
 
    string name = DictImpl::unescape(line.substr(0, n));
    string value = DictImpl::unescape(line.substr(n+1));
    properties[name] = value;
  }
}

// ==================================================================
// New xerox for simplified dictionary format
// ==================================================================
class TextDictSrc: public DictionarySource, public LineReader {

  long firstPos;

public:
  TextDictSrc( FILE *fhDic ): LineReader( fhDic )
  {
  }

  virtual ~TextDictSrc()
  {
  }


  void setFirstPos()
  {
    firstPos = ftell( fh );
  }

  bool firstEntry()
  {
    if( fseek( fh, firstPos, SEEK_SET ) != 0 )
      throw XeroxException( "Cannot read dictionary file (fseek failed)" );
    return true;
  }

  long nextEntry( string &keyWord, string &description )
  {
    long pos = ftell( fh );
    return readEntry( pos, keyWord, description ) ? pos : -1;
  }
  
  bool readEntry( long pos, string &keyWord, string &description  )
  {
    // Check if we fit into long maximum value, -2000000 to avoid overflow before the check
    if( pos > LONG_MAX-2000000 )
      throw XeroxException( "Maximum dictionary length exceeded" );

    if( fseek( fh, pos, SEEK_SET ) != 0 )
      throw XeroxException( "Cannot read dictionary file (fseek failed)" );

    do {
      char *read = readLine();
      if( read == NULL ) 
        return false;

      keyWord = read;
    } while( keyWord.empty() );

    description = "";

    while( true ) {
      char *read = readLine();
      if( read == NULL )
        break;
      if( read[0] == 0 ) break;
      if( !description.empty() )
        description += " ";
      description += read;
    }

    if( description.empty() ) {
      ostringstream msg;
      msg << "line " << getLineNo() << ": Missing description for item '" << keyWord << "'"; 
      throw XeroxException( msg.str().c_str() );
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

static void errorCheck( bool condition, char *string )  // FIXME could confuse with std::string
{
  if( !condition ) {
    throw XeroxException( string );
  }
}


int main(int argc, char** argv) {

  try {
    bool noHeader = false;
    char *headerFile = NULL;
    char *id = NULL;

    static struct option cmdLineOptions[] = {
      { "help", no_argument, NULL, 'e' },
      { "verbose", no_argument, NULL, 'v' },
      { "id", required_argument, NULL, 'i' },
      { "header-file", required_argument, NULL, 'h' },
      { "no-header", no_argument, NULL, 'n' },
      { NULL, 0, NULL, 0 }
    };

    int optionIndex = 0;
    while( 1 ) {
      int c = getopt_long(argc, argv, "vi:h:n", cmdLineOptions, &optionIndex);
      if( c == -1 ) break;
      switch( c ) {
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

    errorCheck( optind == argc - 2, "Both input and output file must be specified" );

    sourceFileName = argv[optind++];
    destFileName = argv[optind++];
    
    {
      FILE *fhDic, *fhOut;
      fhDic = fopen( sourceFileName, "r" );
      errorCheck( fhDic != NULL, "Cannot open input file for reading" );
      if( !strcmp( destFileName, "-" ) )
        fhOut = stdout;
      else
        fhOut = fopen( destFileName, "wb" );

      errorCheck( fhOut != NULL, "Cannot open output file for writing" );

      // property values
      map<string, string> properties;
      XeroxCollationComparator comparator;

      TextDictSrc source( fhDic );

      if( !noHeader )
        readProperties( &source, properties );

      source.setFirstPos();

      if( headerFile != NULL ) {
        FILE *fhHeader;
        if( !strcmp( headerFile, "-" ) )
          fhHeader = stdin;
        else
          fhHeader = fopen( headerFile, "r" );

        errorCheck( fhHeader != NULL, "Cannot open header file for reading" );
        LineReader headerLR( fhHeader );
        readProperties( &headerLR, properties );
        if( fhHeader != stdout ) fclose( fhHeader );
      }

      if( id != NULL )
        properties["id"] = id;

      {
        string precedence = properties["char-precedence"];
        string ic = properties["search-ignore-chars"];

        if (ic.size() == 0) {
          ic = !precedence.empty() ? "" : "-.";
          properties["search-ignore-chars"] = ic;
        }
        comparator.setCollation( precedence, ic );
      }

      errorCheck( properties.find( "id" ) != properties.end(),
        "missing required 'id' property in the header");

      processXerox( &comparator, &source, properties, fhOut );

      fclose( fhDic );
      if( fhOut != stdout ) fclose( fhOut );
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
