/**
 * @file   bedic.h
 * @brief  This is a new dictionary interface, which should replace Dictionary
 *         class.
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

#pragma once
#ifndef _BEDIC_H
#define _BEDIC_H

#include <string.h>
#include <stdio.h>

#include <string>

class CollationComparator;

/**
 * A slightly modified simple smart pointer from Yonat Sharon.
 */
template <class X> class OwnedPtr
{
public:
    typedef X element_type;

    explicit OwnedPtr(X* p=0)       : itsOwn(p!=0), itsPtr(p) {}
    ~OwnedPtr() {
      if (itsOwn) {
        delete itsPtr;
      }
    }
    OwnedPtr(const OwnedPtr& r) 
        : itsOwn(r.itsOwn), itsPtr(r.release()) {}
    OwnedPtr& operator=(const OwnedPtr& r) 
    {
        if (&r != this) {
            if (itsPtr != r.itsPtr) {
                if (itsOwn) delete itsPtr;
                itsOwn = r.itsOwn;
            }
            else if (r.itsOwn) itsOwn = true;
            itsPtr = r.release();
        }
        return *this;
    }

    bool operator==( const OwnedPtr &x ) const
      {
        return *(itsPtr) == *(x.itsPtr);
      }

    bool operator!=( const OwnedPtr &x ) const
      {
        return *(itsPtr) != *(x.itsPtr);
      }

    bool isValid() const
      {
        return itsPtr != NULL;
      }

    X& operator*()  const            {return *itsPtr;}
    X* operator->() const            {return itsPtr;}
    X* get()        const            {return itsPtr;}
    X* release()    const  {itsOwn = false; return itsPtr;}

private:
    mutable bool itsOwn;
    X* itsPtr;
};

class DictionaryIterator
{
public:
  virtual ~DictionaryIterator()
  {
  }

  virtual const char *getKeyword() = 0;
  virtual const char *getDescription() = 0;

  virtual bool nextEntry() = 0;
  virtual bool previousEntry() = 0;

  bool operator==( DictionaryIterator &x ) 
  {
    return strcmp( getKeyword(), x.getKeyword() ) == 0;
  }

  bool operator!=( DictionaryIterator &x ) 
  {
    return strcmp( getKeyword(), x.getKeyword() ) != 0;
  }
  
};

typedef OwnedPtr<DictionaryIterator> DictionaryIteratorPtr;

class StaticDictionary
{
public:
  virtual ~StaticDictionary()
  {
  }

  virtual DictionaryIteratorPtr begin() = 0;
  virtual DictionaryIteratorPtr end() = 0;

  virtual DictionaryIteratorPtr findEntry( const char *keyword, bool &matches ) = 0;

  virtual const char *getName() = 0;
  virtual const char *getFileName() = 0;

  virtual bool getProperty( const char *propertyName, std::string &propertyValue ) = 0;

  virtual const char *getErrorMessage() = 0;

  virtual bool checkIntegrity()
  {
    return true;
  }

  virtual CollationComparator* getCollationComparator()
  {
    return NULL;
  }  
  
  /**
   * Check if the dictionary can be casted to DynamicDictionary
   * interface (qtopia does not allow for RTT).
   */
  virtual bool isDynamic()
  {
    return false;
  }

  /**
   * Check if the dictionary allows to edit its properties. Must be also
   * DynamicDictionary.
   */
  virtual bool isMetaEditable()
  {
    return false;
  }
  
  // Static members

  static StaticDictionary *loadDictionary( const char* filename, bool doCheckIntegrity, std::string &errorMessage );
  
};

class DynamicDictionary: public StaticDictionary
{
public:
  virtual ~DynamicDictionary()
  { 
  }

  virtual DictionaryIteratorPtr insertEntry( const char *keyword ) = 0;
  virtual bool updateEntry( const DictionaryIteratorPtr &entry, const char *description ) = 0;
  virtual bool removeEntry( const DictionaryIteratorPtr &entry ) = 0;  

  virtual bool setProperty( const char *propertyName, const char *propertyValue ) = 0;

  virtual bool isDynamic()
  {
    return true;
  }
  
};

DynamicDictionary *createSQLiteDictionary( const char *fileName, const char *name, std::string &errorMessage );
DynamicDictionary *createHybridDictionary( const char *fileName, StaticDictionary *static_dic,
  std::string &errorMessage );

std::string formatDicEntry( std::string entry );


#endif  /* _BEDIC_H */

