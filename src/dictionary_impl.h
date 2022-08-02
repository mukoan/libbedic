/**
 * @file   dictionary_impl.h
 * @brief  Dictionary class implementation
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

#pragma once
#ifndef DICTIONARY_IMPL_H
#define DICTIONARY_IMPL_H

#include <string>
#include <vector>
#include <map>

#include "dictionary.h"
#include "file.h"
#include "shcm.h"

/**
 * DictImpl class implements the abstract Dictionary class
 *
 * Format of the dictionary database
 *
 *    The database has two parts: header and data.
 *
 *    1. Header
 *       The header contains a collection of property values.
 *       All data in the header is encoded using UTF-8.
 *
 *       The format of a line of the header is:
 *       <name> '=' <value>
 *
 *       Currently defined properties:
 *       - name      required
 *           The name of the database (as will be shown to the user)
 *
 *       - search-ignore-chars  optional (default: empty)
 *           List of the characters that will be ignored while doing search
 *
 *       - max-word-length  optional (default: 50)
 *           Maximum length of the word entry
 *
 *       - max-entry-length  optional (default: 8192)
 *           Maximum length of database entry
 *
 *       The header ends with '\0' character.
 *
 *    2. Data
 *
 *       The data section of the dictionary contains variable-size entries.
 *       Every entry defines a single word and (all) its meanings. The entries
 *       in the database are sorted. The comparision while sorting ignores the
 *       character case and the characters that should be ignored (see
 *       search-ignore-chars)
 *
 *       The entry contains two values: word and sense.
 *       Both are variable-size, the delimiter between them is an '\n' character.
 *
 *       The entry ends with '\0' character.
 *
 * The class uses binary search to find a word.
 * It creates an index to improve the searches.
 *
 */

struct entry_type;

typedef std::vector<unsigned short> CanonizedWord;

class CollationComparator 
{
protected:
  std::vector<std::string> ignoreChars;
  std::map<int, int> charPrecedence;     // To build lexical precedence
  std::vector<int> precedenceGroups;
  bool useCharPrecedence;
  int charPrecedenceUnknown;

public:
  void setCollation(const std::string &collationDef, const std::string &ignoreChars);

  /**
   * Compares two words
   * The words should be put in canonical form before this method is called
   *
   * @param s1  The first word
   * @param s2  The second word
   * @return  the usual comparison value
   */
  int compare(const CanonizedWord &s1, const CanonizedWord &s2);

  /**
   * Puts a string in canonized form ready for * comparision.
   *
   * All the characters in the word are upper-cased.
   * All the characters that should be ignored are removed.
   *
   * @param s   The word to canonize
   * @return  canonical form of the word
   */
  CanonizedWord canonizeWord(const std::string &s);
};


class DictImpl : public Dictionary, public CollationComparator
{
  friend struct entry_type;

public:

  /**
   * Creates new dictionary object
   * If error occurs, error description is set to 
   * non-zero value
   *
   * @param filename name of the dictionary file
   * @param doCheckIntegrity if true, check integrity. Checking
   * integrity may be slow for large dictionaries.
   */
  DictImpl(const char *filename, bool doCheckIntegrity);
  virtual ~DictImpl();

  /**
   * Returns dictionary name
   *
   * @return  name of the dictionary as set in the dictionary properties header
   */
  virtual const std::string &getName() const;

  /**
   * Returns file name of the dictionary file
   */
  virtual const std::string &getFileName() const;

  /**
   * Looks for a word in the dictionary
   *
   * Sets the internal dictionary state to point to a word equal or greater
   * (in lexicographical terms) to the one specified as parameter.
   *
   * Before two words are compared, the are canonized. i.e. the characters
   * specified in search-ignore-chars property are removed and both words are
   * set to uppercase.
   *
   * Parameter subword is set to true if the word the dictionary points to
   * starts with the word specified as a parameter.
   *
   * @param word     Word to look for
   * @param subword  Flag if word is subword
   *
   * @return  true if exact match is found
   */
  virtual bool findEntry(const std::string &word, bool &subword);

  /**
   * Moves the internal word pointer to the next word.
   * If the pointer is set to the last word, it is not changed.
   * 
   * @return true if the pointer is moved
   */
  virtual bool nextEntry();

  /**
   * Moves the internal word pointer to the first word.
   * 
   * @return true if the word is read successfully
   */
  virtual bool firstEntry();

  /**
   * Moves the internal word pointer to the last word.
   * 
   * @return true if the word is read successfully
   */
  virtual bool lastEntry();

  /**
   * Moves the internal word pointer to randomly chosen entry.
   * 
   * @return true if the word is read successfully
   */
  virtual bool randomEntry();

  /**
   * Returns the word pointed by the internal word pointer
   *
   * @return current word
   */
  virtual const std::string &getWord() const;

  /**
   * Returns the sense of the word pointer by the internal word pointer
   *
   * @return sense
   */
  virtual const std::string &getSense() const;

  /**
   * Returns error description or zero if no error 
   *
   * @return error description
   */
  virtual const std::string &getError() const {
    return errorDescr; 
  }

  /**
   * Returns property from the header of the dictionary file. See
   * bedic-format.txt for the description of available properties.
   *
   * @param key  Name of property
   * @return  property value
   */
  virtual const std::string &getProperty(const char *key) {
    return properties[key];
  }

  /**
   * Check integrity of the dictionary file.
   *
   * @return  true if integrity check is succesfull - dictionary file is not
   *          corrupted.
   */
  bool checkIntegrity();

protected:

  /**
   * This structure represent an index entry
   *
   * It contains canonized word value and the position of that value
   */
  struct IndexEntry
  {
    CanonizedWord word;
    long pos;

    IndexEntry(const CanonizedWord &w, long p) : word(w), pos(p) { }
  };

  /// File descriptor to the dictionary file
  File *fdata;

  /// Position of the first entry
  long firstEntryPos;

  /// Position of the last entry
  long lastEntryPos;

  /// Error description
  std::string errorDescr;

  /// Properties (see the class overall documentation)
  std::string name;
  std::string fileName;
  int maxWordLength;
  int maxEntryLength;

  /// General purpose buffer
  char *buf;

  /// Current word
  std::string currWord;

  /// The sense of the current word
  mutable std::string currSense;

  /// Senses are compressed using the SHC compressor
  mutable bool senseCompressed;

  /// Current position
  long currPos;

  /// Next position, or -1 if not defined
  long nextPos;

  /// Index table
  std::vector<IndexEntry> index;

  /// Property values
  std::map<std::string, std::string> properties;

  /// The SHC compressor
  SHCM *compressor;

  /// Set an error description
  void setError(const std::string &err) {
    errorDescr = err; 
  }

  /**
   * Reads the properties of the dictionary.
   *
   * The file pointer should point to the start of the file
   * If the execution was successful, on return the file
   * pointer points to the first character after '\0'
   * end-of-header marker. If error occured, the file pointer
   * position is undefined.
   *
   * Updates properties field with the values of the
   * properties read from the header.
   */
  int readProperties();

  /**
   * Reads a line from the header.
   *
   * The read start from the current file pointer position.
   * If the read is successful, the file pointer is updated.
   *
   * If the line contains '\0' character, null string 
   * is returned to inform for the end of the header
   *
   * @return next line
   */
  int getLine(std::string &, int &);

  /**
   * Reads an entry starting from the specified position.
   *
   * Updates currWord, currSense, currPos and nextPos fields.
   *
   * The method expects that pos points to the start of an
   * entry. If not, the results are undefined.
   *
   * @param pos start of the entry to read
   *
   * @return true if read was succesful
   */
  bool readEntry(long pos);

  /**
   * Looks backward for a start of an entry.
   *
   * There are no restrictions for the value of the pos
   * parameter. If the value is less than the position of
   * the first entry, the position of the first entry is
   * returned. If the value is greater than the position of
   * the last entry, the position of the last entry is
   * returned.
   *
   * @param pos   position to start scanning backward from
   *
   * @return start position of an entry
   */
  long findPrev(long pos);

  /** 
   * Looks forward for a start of an entry.
   *
   * There are no restrictions for the value of the pos
   * parameter. If the value is less than the position of
   * the first entry, the position of the first entry is
   * returned. If the value is greater than the position of
   * the last entry, the position of the last entry is
   * returned.
   *
   * @param pos
   *
   * @return start position of an entry
   */
  long findNext(long pos);

  /**
   * Index lookup
   *
   * Using the index, finds the region in the file where
   * the specified word is defined.
   *
   * @param s word to look for
   * @param b output param. sets the start of the region
   * @param e output param. sets the end of the region
   */
  void bsearchIndex(const CanonizedWord &s, long &b, long &e);

  // Entry delimiter character
  static const char DATA_DELIMITER;

  // Word delimiter character
  static const char WORD_DELIMITER;

public:
  /// Convert the string s to escape codes
  static std::string escape(const std::string &s);

  /// Convert the string s to use delimiter characters
  static std::string unescape(const std::string &s);
};

extern unsigned char terminal_keyword[];

#endif  /* DICTIONARY_IMPL_H */

