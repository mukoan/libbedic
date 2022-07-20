/**
 * @file   bedic_wrapper.cpp
 * @brief  This file contains a wrapper for the original bedic Dictionary class
 *         (file dictionary.cpp), so that it can be interfaced as a
 *         StaticDictionary. This dictionary does not implement
 *         DictionaryIterator::previous().
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

#include "bedic.h"
#include "dictionary.h"

extern char terminal_keyword[];

class BedicDictionaryIterator;

/**
 * Wrapper for the Dictionary class
 */
class BedicDictionary : public StaticDictionary
{
  friend class BedicDictionaryIterator;
  friend StaticDictionary *loadBedicDictionary(const char *filename, bool doCheckIntegrity,
                                               std::string &errorMessage);

protected:
  Dictionary *dic;

protected:
  explicit BedicDictionary(Dictionary *dic) : dic(dic)
  {
  }

public:
  ~BedicDictionary();

  virtual DictionaryIteratorPtr begin();
  virtual DictionaryIteratorPtr end();

  virtual DictionaryIteratorPtr findEntry(const char *keyword, bool &matches);

  virtual const char *getName();
  virtual const char *getFileName();

  virtual bool getProperty(const char *propertyName, std::string &propertyValue);

  virtual const char *getErrorMessage();

  virtual bool checkIntegrity();
};

//============== Iterator ==============

class BedicDictionaryIterator : public DictionaryIterator
{
  Dictionary *dic;
  bool lastEntry;

public:
  BedicDictionaryIterator(Dictionary *dic, bool lastEntry) :
                                           dic(dic), lastEntry(lastEntry)
  {
  }

  ~BedicDictionaryIterator()
  {
  }

  const char *getKeyword() 
  {
    if(lastEntry) return terminal_keyword;
    return dic->getWord().c_str();
  }

  virtual const char *getDescription()
  {
    if(lastEntry) return nullptr;
    return dic->getSense().c_str();
  }

  bool nextEntry()
  {
    if(lastEntry)
      return false;

    bool moved = dic->nextEntry();
    if(!moved) {
      lastEntry = true;
    }

    if(dic->getError() != "")
      return false;
    else
      return true;
  }

  bool previousEntry()
  {
    return false;
  }
};

// =======================================

BedicDictionary::~BedicDictionary()
{
  delete dic;
}

DictionaryIteratorPtr BedicDictionary::begin()
{
  bool success = dic->firstEntry();
  if(!success)
    return DictionaryIteratorPtr(nullptr);

  return DictionaryIteratorPtr(new BedicDictionaryIterator(dic, false));
}

DictionaryIteratorPtr BedicDictionary::end()
{
  return DictionaryIteratorPtr(new BedicDictionaryIterator(dic, true));
}

DictionaryIteratorPtr BedicDictionary::findEntry(const char *keyword, bool &matches)
{
  bool subword;
  matches = dic->findEntry(keyword, subword);

  if(dic->getError() != "") {
    return DictionaryIteratorPtr(nullptr);
  }

  return DictionaryIteratorPtr(new BedicDictionaryIterator(dic, false));
}

const char *BedicDictionary::getName()
{
  return dic->getName().c_str();
}

const char *BedicDictionary::getFileName()
{
  return dic->getFileName().c_str();
}

bool BedicDictionary::getProperty(const char *propertyName, std::string &propertyValue)
{
  propertyValue = dic->getProperty(propertyName);
  return true;
}

const char *BedicDictionary::getErrorMessage()
{
  return dic->getError().c_str();
}

bool BedicDictionary::checkIntegrity()
{
  return dic->checkIntegrity();
}

StaticDictionary *loadBedicDictionary(const char *filename, bool doCheckIntegrity,
                                      std::string &errorMessage)
{
  Dictionary *dic = Dictionary::create(filename, doCheckIntegrity);
  errorMessage = dic->getError();
  if(errorMessage != "") {
    delete dic;
    return nullptr;
  }

  return new BedicDictionary(dic);
}
