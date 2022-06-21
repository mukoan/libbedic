/****************************************************************************
* dictionary.h
*
* Copyright (C) 2002 Latchesar Ionkov <lionkov@yahoo.com>
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
****************************************************************************/
#ifndef _DICTIONARY_H
#define _DICTIONARY_H

#include <string>

using namespace std;
/**
 * This is an abstract class that represent Dictionary
 *
 */
class Dictionary {
public:
	/**
	 * Create dictionary instance
	 *
	 * @param filename name of the file that contains 
	 * the dictionary database
	 * @param doCheckIntegrity if true, check integrity. Checking
	 * integrity may be slow for large dictionaries.
	 *
	 * @return pointer to dictionary object or null if error
	 */
	static Dictionary* create(const char* filename, bool doCheckIntegrity = true);

	virtual ~Dictionary();

	/**
	 * Returns dictionary name (as shown to the user
	 */
	virtual const string& getName() const =0;

	/**
	 * Returns file name of the dictionary file
	 */
	virtual const string& getFileName() const =0;
        
	/**
	 * Looks for a word in the dictionary
	 *
	 * Sets the internal dictionary state to point to a
	 * word equal or greater (in lexicographical terms)
	 * to the one specified as parameter.
	 *
	 * Before two words are compared, the are canonized.
	 * I.e. the characters specified in search-ignore-chars 
	 * property are removed and both words are set 
	 * to uppercase.
	 *
	 * Parameter subword is set to true if the word the
	 * dictionary points to starts with the word specified
	 * as a parameter.
	 *
	 * @param word word to look for
	 * @param subword flag if word is subword
	 *
	 * @return true if exact match is found
	 */
	virtual bool findEntry(const string& word, bool& subword)=0;

	/**
	 * Moves the internal word pointer to the next word.
	 *
	 * If the pointer is set to the last word, it is 
	 * not changed.
	 * 
	 * @return true if the pointer is moved
	 */
	virtual bool nextEntry()=0;

	/**
	 * Moves the internal word pointer to the first word.
	 * 
	 * @return true if the word is read successfully
	 */
	virtual bool firstEntry()=0;

	/**
	 * Moves the internal word pointer to the last word.
	 * 
	 * @return true if the word is read successfully
	 */
	virtual bool lastEntry()=0;

	/**
	 * Moves the internal word pointer to randomly chosen
	 * entry.
	 * 
	 * @return true if the word is read successfully
	 */
	virtual bool randomEntry()=0;

	/**
	 * Returns the word pointed by the internal word pointer
	 *
	 * @return current word
	 */
	virtual const string& getWord() const =0;

	/**
	 * Returns the sense of the word pointer by the 
	 * internal word pointer
	 *
	 * @return sense
	 */
	virtual const string& getSense() const =0;

	/**
	 * Returns error description or zero if no error 
	 *
	 * @return error description
	 */
	virtual const string& getError() const =0;


        /**
         * Returns property from the header of the dictionary file. See
         * bedic-format.txt for the description of available properties.
         *
         * @return property value
         */
        virtual const string& getProperty( const char *name ) =0;
        
        /**
	 * Check integrity of the dictionary file.
	 *
	 * @return true if integrity check is succesfull -
	 * dictionary file is not corrupted.
	 */
	virtual bool checkIntegrity() =0;
};

#endif
