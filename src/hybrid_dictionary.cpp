/****************************************************************************
* hybrid_dictionary.cpp
*
* Copyright (C) 2005 Rafal Mantiuk <rafm@users.sourceforge.net>
*
* This is an implementation of editable - dynamic dictionary
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

#include "bedic.h"
#include "dictionary_impl.h"


class HybridDictionaryIterator;

/** Hybrid dictionary consists of a large static (bedic) dictionary
 * and a small dynamic (sqlite) dictionary, which are searched
 * together. All aditing is done on the dynamic dictionary.
 */
class HybridDictionary: public DynamicDictionary
{
  friend DynamicDictionary *createHybridDictionary(const char *fileName, StaticDictionary *static_dic,
                                                   std::string &errorMessage);
  friend DynamicDictionary *loadHybridDictionary(const char *fileName, std::string &errorMessage);
  friend class HybridDictionaryIterator;

protected:
  std::string errorString;

  StaticDictionary *static_dic;
  DynamicDictionary *dynamic_dic;

  explicit HybridDictionary(const char *fileName);
  HybridDictionary(StaticDictionary *static_dic, DynamicDictionary *dynamic_dic);

public:
  ~HybridDictionary();

  virtual DictionaryIteratorPtr begin();
  virtual DictionaryIteratorPtr end();

  virtual DictionaryIteratorPtr findEntry(const char *keyword, bool &matches);

  virtual const char *getName();
  virtual const char *getFileName();

  virtual bool getProperty(const char *propertyName, std::string &propertyValue);
  virtual bool setProperty(const char *propertyName, const char *propertyValue);

  virtual const char *getErrorMessage();

  virtual bool isMetaEditable()
  {
    return false;
  }

  DictionaryIteratorPtr insertEntry(const char *keyword);
  bool updateEntry(const DictionaryIteratorPtr &entry, const char *description);
  bool removeEntry(const DictionaryIteratorPtr &entry);
};

//============== Iterator ==============

class HybridDictionaryIterator : public DictionaryIterator
{
//  HybridDictionary *dic;

  DictionaryIterator *static_it, *dynamic_it;
  CollationComparator* cmp;

//  std::string keyword;

  enum { NoOrder = 0, StaticFirst, DynamicFirst, BothSame } order;

  DictionaryIterator *getFirstIterator()
  {
    switch( order ) {
    case NoOrder:
    {
//      printf( "%s - %s\n", static_it->getKeyword(), dynamic_it->getKeyword() );
      CanonizedWord word_s = cmp->canonizeWord(static_it->getKeyword());
      CanonizedWord word_d = cmp->canonizeWord(dynamic_it->getKeyword());
      int res = cmp->compare( word_s, word_d );
      if(res == 0) {
        order = BothSame;
        return dynamic_it;
      } else if(res < 0) {
        order = StaticFirst;
        return static_it;
      } else {
        order = DynamicFirst;
        return dynamic_it;
      }
    }
    case DynamicFirst:
      return dynamic_it;
    case StaticFirst:
      return static_it;
    case BothSame:
      return dynamic_it;
    } 
    return static_it;
  }

public:
  HybridDictionaryIterator(DictionaryIteratorPtr static_it, DictionaryIteratorPtr dynamic_it,
                           CollationComparator *cmp) : static_it(static_it.release()),
                           dynamic_it(dynamic_it.release()), cmp(cmp), order(NoOrder)
  {
  }

  ~HybridDictionaryIterator()
  {
    delete static_it; 
    delete dynamic_it;
  }

  const char *getKeyword()
  {
    return getFirstIterator()->getKeyword();
  }

  virtual const char *getDescription()
  {
    return getFirstIterator()->getDescription();
  }

  bool nextEntry()
  {
    DictionaryIterator *firstIt = getFirstIterator();

    bool res = firstIt->nextEntry();
    if(!res) return false;

    if(order == BothSame)
      res = static_it->nextEntry();

    order = NoOrder;

    return res;
  }

  bool previousEntry()
  {
    return false;
  }
};

//============== Search ==============

DictionaryIteratorPtr HybridDictionary::begin()
{
  return DictionaryIteratorPtr(new HybridDictionaryIterator(
                                static_dic->begin(), dynamic_dic->begin(),
                                dynamic_dic->getCollationComparator()));
}

DictionaryIteratorPtr HybridDictionary::end()
{
  return DictionaryIteratorPtr(new HybridDictionaryIterator(
                                static_dic->end(), dynamic_dic->end(),
                                dynamic_dic->getCollationComparator()));
}

DictionaryIteratorPtr HybridDictionary::findEntry(const char *keyword, bool &matches)
{
  bool matches_static, matches_dynamic;
  DictionaryIteratorPtr it =  DictionaryIteratorPtr(new HybridDictionaryIterator(
                                  static_dic->findEntry(keyword, matches_static),
                                  dynamic_dic->findEntry(keyword, matches_dynamic),
                                  dynamic_dic->getCollationComparator()));
  matches = matches_static || matches_dynamic;
  return it;
}
// ============= Constructors ==============

HybridDictionary::HybridDictionary(StaticDictionary *static_dic, DynamicDictionary *dynamic_dic) :
                                              static_dic(static_dic), dynamic_dic( dynamic_dic )
{
}

HybridDictionary::~HybridDictionary()
{
  delete static_dic;
  delete dynamic_dic;
}




// ============= Create hybrid dictionary ==============

DynamicDictionary *createHybridDictionary(const char *fileName, StaticDictionary *static_dic,
                                          std::string &errorMessage)
{

  DynamicDictionary *dynamic_dic = createSQLiteDictionary(fileName, static_dic->getName(), errorMessage);
  if(dynamic_dic == NULL)
    return NULL;

  {
    string collation_def;
    bool success = static_dic->getProperty( "char-precedence", collation_def );

    if(success) {
      success = dynamic_dic->setProperty("collation", collation_def.c_str());
      if(!success) {
        errorMessage = dynamic_dic->getErrorMessage();
        delete dynamic_dic;
        return NULL;
      }
    }
  }


  {
    string ignore_def;
    bool success = static_dic->getProperty("search-ignore-chars", ignore_def);

    if(success) {
      if(ignore_def.empty())
        ignore_def = "-.";
      success = dynamic_dic->setProperty("search-ignore-chars", ignore_def.c_str());
      if(!success) {
        errorMessage = dynamic_dic->getErrorMessage();
        delete dynamic_dic;
        return NULL;
      }
    }
  }


//   success = dynamic_dic->setProperty( "static-dic", static_dic->getFileName() );
//   if( !success ) {
//     errorMessage = dynamic_dic->getErrorMessage();
//     delete dynamic_dic;
//     return NULL;
//   }

  return new HybridDictionary(static_dic, dynamic_dic);

}


StaticDictionary *loadBedicDictionary(const char* filename, bool doCheckIntegrity, std::string &errorMessage);
DynamicDictionary *loadSQLiteDictionary(const char *fileName, std::string &errorMessage);

DynamicDictionary *loadHybridDictionary(const char *fileName, std::string &errorMessage)
{
  std::string static_file_name = fileName;
  int len = static_file_name.size();
  if(len < 6) return NULL;
  if(static_file_name.substr(len - 5) != ".hdic") {
    errorMessage = "Invalid hybrid dictionary extension";
    return NULL;
  }
  static_file_name.replace(len-5, 5, ".dic.dz");

  DynamicDictionary *dynamic_dic = loadSQLiteDictionary(fileName, errorMessage);
  if(dynamic_dic == NULL)
    return NULL;

//   std::string static_file_name;
//   bool success = dynamic_dic->getProperty( "static-dic", static_file_name );
//   if( !success ) {
//     errorMessage = dynamic_dic->getErrorMessage();
//     delete dynamic_dic;
//     return NULL;
//   }

  StaticDictionary *static_dic = loadBedicDictionary(static_file_name.c_str(), false, errorMessage);
  if(static_dic == NULL) {
    delete dynamic_dic;
    return NULL;
  }

  return new HybridDictionary(static_dic, dynamic_dic);
}

// ============= Properties ==============

const char *HybridDictionary::getName()
{
  return static_dic->getName();
}

const char *HybridDictionary::getFileName()
{
  return dynamic_dic->getFileName();
}


bool HybridDictionary::getProperty(const char *propertyName, std::string &propertyValue)
{
  bool res = dynamic_dic->getProperty(propertyName, propertyValue);
  if(!res) return false;
  if(propertyValue != "") return true;
  return static_dic->getProperty(propertyName, propertyValue);
}

bool HybridDictionary::setProperty(const char *propertyName, const char *propertyValue)
{
  return dynamic_dic->setProperty(propertyName, propertyValue);
}

const char *HybridDictionary::getErrorMessage()
{
  if(strlen(static_dic->getErrorMessage()) != 0)
    return static_dic->getErrorMessage();
  return dynamic_dic->getErrorMessage();
}

// ============= Editing ==============

DictionaryIteratorPtr HybridDictionary::insertEntry(const char *keyword)
{
  return dynamic_dic->insertEntry(keyword);
}

bool HybridDictionary::updateEntry(const DictionaryIteratorPtr &entry, const char *description)
{
  bool matches;
  DictionaryIteratorPtr placeHolder = dynamic_dic->findEntry(entry->getKeyword(), matches);
  if(!matches) {
    placeHolder = dynamic_dic->insertEntry(entry->getKeyword());
    if(!placeHolder.isValid())
      return false;
  }

  return dynamic_dic->updateEntry(placeHolder, description);
}

bool HybridDictionary::removeEntry(const DictionaryIteratorPtr &entry)
{
  return dynamic_dic->removeEntry(entry);
}

