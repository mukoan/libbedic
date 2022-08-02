/**
 * @file   dynamic_dictionary.cpp
 * @brief  This is an implementation of editable - dynamic dictionary
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

#include <string>
#include <exception>

#include <sqlite3.h>

#include "bedic.h"
#include "dictionary_impl.h"    // Collation

#include "utf8.h"

static int compare_callback(void *collationPtr, int len1, const void *s1, int len2, const void *s2);
//static int strlenUTF8( const char *s ) ;

#include "default_collation.h"

enum StmtID { S_GET_PROPERTY = 0, S_SET_PROPERTY, S_INSERT_ENTRY, S_FIND_NEXT,
              S_UPDATE_ENTRY, S_REMOVE_ENTRY, S_GET_DESCRIPTION, S_FIND_NEXT_OR_SAME,
              S_COUNT };


class SQLiteDictionaryIterator;

class SQLiteDictionary : public DynamicDictionary
{
  friend DynamicDictionary *createSQLiteDictionary(const char *fileName, const char *name,
                                                   std::string &errorMessage);
  friend DynamicDictionary *loadSQLiteDictionary(const char *fileName, std::string &errorMessage);
  friend class SQLiteDictionaryIterator;

  std::string name, fileName, errorString;

  sqlite3 *_db;
  sqlite3_stmt *statement[S_COUNT];

  sqlite3_stmt *getStmt(StmtID stmt_id);
  sqlite3 *getDB();

  CollationComparator collationComparator;

protected:
  /**
   * Constructor
   * @param fname    Filename
   */
  explicit SQLiteDictionary(const char *fname);

  /**
   * A dictionary can only be bound if it has id and collation properties,
   * search-ignore-chars also needs to be set
   */
  bool bind();

  bool findNext(const char *keyword, std::string &next, bool or_same);

public:
  ~SQLiteDictionary();

  virtual DictionaryIteratorPtr begin();
  virtual DictionaryIteratorPtr end();

  virtual DictionaryIteratorPtr findEntry(const char *keyword, bool &matches);

  virtual CollationComparator   *getCollationComparator()
  {
    return &collationComparator;
  }

  virtual const char *getName();
  virtual const char *getFileName();

  virtual bool getProperty(const char *propertyName, std::string &propertyValue);
  virtual bool setProperty(const char *propertyName, const char *propertyValue);

  virtual const char *getErrorMessage();  

  virtual bool isMetaEditable()
  {
    return true;
  }

  DictionaryIteratorPtr insertEntry(const char *keyword);
  bool updateEntry(const DictionaryIteratorPtr &entry, const char *description);
  bool removeEntry(const DictionaryIteratorPtr &entry);  
};

//============== Iterator ==============

class SQLiteDictionaryIterator : public DictionaryIterator
{
  SQLiteDictionary *dic;
  std::string keyword, description;

public:
  SQLiteDictionaryIterator(SQLiteDictionary *dic, const char *kword) : dic(dic),
                                                                       keyword(kword)
  {
  }

  ~SQLiteDictionaryIterator()
  {
  }

  const char *getKeyword()
  {
    return keyword.c_str();
  }

  virtual const char *getDescription()
  {
    if(!description.empty()) 
      return description.c_str();

    sqlite3 *db = dic->getDB();
    if(db == nullptr) return nullptr;

    sqlite3_stmt *stmt = dic->getStmt(S_GET_DESCRIPTION);
    if(stmt == nullptr) return nullptr;

    sqlite3_bind_text(stmt, 1, keyword.c_str(), keyword.size(), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW)
    {
      const char *str = (const char *)sqlite3_column_text(stmt, 0);
      description = str != nullptr ? str : "";
    }
    else
    {
      dic->errorString = std::string(sqlite3_errmsg(db));
      sqlite3_reset( stmt );
      return nullptr;
    }

    sqlite3_reset(stmt);

    return description.c_str();
  }

  bool nextEntry()
  {
    std::string next;
    if(!dic->findNext(keyword.c_str(), next, false))
      return false;

    keyword = next;
    description = "";

    return true;
  }

  bool previousEntry()
  {
    return false;
  }
};


// Constructor
SQLiteDictionary::SQLiteDictionary(const char *fname) : fileName(fname), _db(nullptr)
{
  memset(statement, 0, sizeof(sqlite3_stmt*)*S_COUNT);
}

// Destructor
SQLiteDictionary::~SQLiteDictionary()
{
  if(_db != nullptr) sqlite3_close(_db);
}

//============== DB Utils ==============

sqlite3 *SQLiteDictionary::getDB()
{
  if(_db == nullptr) {
    int rc;
    rc = sqlite3_open(fileName.c_str(), &_db);
    if(rc != SQLITE_OK) {
      if(_db != nullptr)
        errorString = sqlite3_errmsg(_db);
      return nullptr;
    }
    sqlite3_create_collation(_db, "bedic", SQLITE_UTF8, &(this->collationComparator), compare_callback);
  }
  return _db;
}

static const char *statement_sql[S_COUNT] = {
  // S_GET_PROPERTY
  "select value from properties where tag=?",
  // S_SET_PROPERTY
  "insert or replace into properties (tag, value) values( ?1, ?2)",
  //S_INSERT_ENTRY
  "insert or fail into entries (keyword, create_date, modif_date) values( ?1, ?2, ?2)",
  //S_FIND_NEXT
  "select keyword from entries where keyword > ?1 limit 1",
  //S_UPDATE_ENTRY
  "update entries set description=?2, modif_date=?3 where keyword=?1",
  //S_REMOVE_ENTRY
  "delete from entries where keyword=?1",
  //S_GET_DESCRIPTION
  "select description from entries where keyword=?1",
  //S_FIND_NEXT_OR_SAME
  "select keyword from entries where keyword >= ?1 limit 1"
};

sqlite3_stmt *SQLiteDictionary::getStmt(StmtID stmt_id)
{
  sqlite3 *db = getDB();
  if(db == nullptr) return nullptr;
  
  if(statement[stmt_id] != nullptr) {
    return statement[stmt_id];
  } else {
    sqlite3_stmt *new_stmt;
    int rc = sqlite3_prepare(db, statement_sql[stmt_id], strlen(statement_sql[stmt_id]),
                             &new_stmt, nullptr);
    statement[stmt_id] = new_stmt;
    if(rc != SQLITE_OK) {
      errorString = std::string(sqlite3_errmsg(db)) + " (SQL: " + statement_sql[stmt_id] + ")";
      return nullptr;
    }
    return new_stmt;
  }
}

bool SQLiteDictionary::bind()
{
  bool success = getProperty("id", name);
  if(!success || name.empty()) return false;

  std::string collationString, ignoreChars;
  success = getProperty("collation", collationString);
  if(!success) return false;
  getProperty("search-ignore-chars", ignoreChars);
  collationComparator.setCollation(collationString, ignoreChars);

//  sqlite3_exec( _db, "reindex bedic", NULL, NULL, NULL );

  return true;   // FIXME search-ignore-chars is optional, but appears to 
                 //       be non-optional as you can't set the collation without it
}

//=============================

const char database_schema[] = 
"create table entries ("
"  keyword varchar(200) PRIMARY KEY COLLATE bedic,"
"  description varchar(1024000),"
"  create_date int,"
"  modif_date int );"
""
"create table properties ("
"  tag varchar(200) PRIMARY KEY,"
"  value varchar(1024000) );";

DynamicDictionary *createSQLiteDictionary(const char *fileName, const char *name,
                                          std::string &errorMessage)
{
  // Check if file exists, if it does - return an error
  {
    FILE *fh = fopen(fileName, "rb");
    if(fh != nullptr) {
      errorMessage = "File exists";
      fclose(fh);
      return nullptr;
    }
  }

  // Create DB
  int rc;
  sqlite3 *db;
  rc = sqlite3_open(fileName, &db);
  if(rc != SQLITE_OK) {
    if(db != nullptr) {
      errorMessage = sqlite3_errmsg(db);
      sqlite3_close(db);
    }
    return nullptr;
  }

  // initialize collation, so create table does not fail
  sqlite3_create_collation(db, "bedic", SQLITE_UTF8, nullptr, compare_callback);

  // create tables
  char *errmsg = nullptr;
  rc = sqlite3_exec(db, database_schema, nullptr, nullptr, &errmsg);
  if(rc != SQLITE_OK) {
    if(errmsg != nullptr) {
      errorMessage = errmsg;
      sqlite3_free( errmsg );
    }
    sqlite3_close(db);
    return nullptr;
  }

  sqlite3_close(db);
    
//   // Copy an empty database to fileName
//   FILE *in = fopen( templateFileName, "rb" );
//   if( in == NULL ) {
//     errorMessage = std::string("Can not open ") + templateFileName;
//     return NULL;
//   }


//   FILE *out = fopen( fileName, "wb" );
//   if( out == NULL ) {
//     fclose( in );
//     errorMessage =  "Can not open dictionary file for writing" ;
//     return NULL;
//   }

//   const int bufSize = 1024;
//   char buf[bufSize];

//   do {
//     int read = fread( buf, sizeof( char ), bufSize, in );
//     fwrite( buf, sizeof( char ), read, out );
//   } while( !feof( in ) );

//   fclose( in );
//   fclose( out );

  SQLiteDictionary *dic = new SQLiteDictionary(fileName);

  if(!dic->setProperty("id", name) ||
     !dic->setProperty("collation", default_collation) ||
     !dic->setProperty("bedic-version", VERSION) ||
     !dic->bind()) {
    errorMessage = dic->getErrorMessage();
    delete dic;
    return nullptr;
  }

  return dic;
}

DynamicDictionary *loadSQLiteDictionary(const char *fileName, std::string &errorMessage)
{
  // Check if file exists, if it does - return an error
  {
    FILE *fh = fopen(fileName, "rb");

    if(fh == nullptr) {
      errorMessage = "File does not exists";
      return nullptr;
    }

    fclose(fh);
  }

  SQLiteDictionary *dic = new SQLiteDictionary(fileName);

  if(!dic->bind())
  {
    errorMessage = dic->getErrorMessage();
    delete dic;
    return nullptr;
  }

  return dic;
}

//============== Search ==============

bool SQLiteDictionary::findNext(const char *keyword, std::string &next, bool or_same)
{
  sqlite3 *db = getDB();
  if(db == nullptr) return false;

  sqlite3_stmt *stmt = getStmt(or_same ? S_FIND_NEXT_OR_SAME : S_FIND_NEXT);
  if(stmt == nullptr) return false;

  sqlite3_bind_text(stmt, 1, keyword, strlen( keyword ), SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);

  if(rc == SQLITE_ROW) {
    next = (const char *)sqlite3_column_text(stmt, 0);
  } else if(rc == SQLITE_DONE) {
    next = (char *)(terminal_keyword);  // FIXME do proper C++ cast
  } else {
    errorString = std::string(sqlite3_errmsg(db));
    sqlite3_reset(stmt);
    return false;
  }

  sqlite3_reset(stmt);

  return true;
}

DictionaryIteratorPtr SQLiteDictionary::begin()
{
  std::string first;
  if(!findNext("", first, false)) {
    return DictionaryIteratorPtr(nullptr);
  }

  return DictionaryIteratorPtr(new SQLiteDictionaryIterator(this, first.c_str()));
}

DictionaryIteratorPtr SQLiteDictionary::end()
{
  return DictionaryIteratorPtr(new SQLiteDictionaryIterator(this, (char *)(terminal_keyword)));  // FIXME do proper C++ cast
}

DictionaryIteratorPtr SQLiteDictionary::findEntry(const char *keyword, bool &matches)
{
  std::string result;
  if(!findNext(keyword, result, true)) {
    return DictionaryIteratorPtr(nullptr);
  }

  matches = (result == keyword);
  return DictionaryIteratorPtr(new SQLiteDictionaryIterator(this, result.c_str()));
}

//============== State ==============


const char *SQLiteDictionary::getName()
{
  return name.c_str();
}

const char *SQLiteDictionary::getFileName()
{
  return fileName.c_str();
}

const char *SQLiteDictionary::getErrorMessage()
{
  return errorString.c_str();
}

//============== Properties ==============

bool SQLiteDictionary::getProperty(const char *propertyName, std::string &propertyValue)
{
  sqlite3 *db = getDB();
  if(db == nullptr) return false;

  sqlite3_stmt *stmt = getStmt(S_GET_PROPERTY);
  if(stmt == nullptr) return false;
  
  sqlite3_bind_text(stmt, 1, propertyName, strlen(propertyName), SQLITE_TRANSIENT);
  if(sqlite3_step( stmt ) == SQLITE_ROW) {
    propertyValue = (const char *)sqlite3_column_text(stmt, 0);
  }
  sqlite3_reset(stmt);

  return true;
}

bool SQLiteDictionary::setProperty(const char *propertyName, const char *propertyValue)
{
  sqlite3 *db = getDB();
  if(db == nullptr) return false;

  sqlite3_stmt *stmt = getStmt(S_SET_PROPERTY);
  if(stmt == nullptr) return false;

  int rc;
  sqlite3_bind_text(stmt, 1, propertyName, strlen(propertyName), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, propertyValue, strlen(propertyValue), SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt);
  if(rc != SQLITE_DONE) {
    errorString = std::string(sqlite3_errmsg(db));
    sqlite3_reset(stmt);
    return false;
  }
  sqlite3_reset(stmt);

  return true;
}

//============== Edit ==============

DictionaryIteratorPtr SQLiteDictionary::insertEntry(const char *keyword)
{
  sqlite3 *db = getDB();
  if(db == nullptr) return DictionaryIteratorPtr(nullptr);

  sqlite3_stmt *stmt = getStmt(S_INSERT_ENTRY);
  if(stmt == nullptr) return DictionaryIteratorPtr(nullptr);

  sqlite3_bind_text(stmt, 1, keyword, strlen(keyword), SQLITE_TRANSIENT);
  int create_time = (int)time(nullptr);
  sqlite3_bind_int(stmt, 2, create_time);
  if(sqlite3_step(stmt) != SQLITE_DONE) {
    errorString = std::string(sqlite3_errmsg(db));
    sqlite3_reset(stmt);
    return DictionaryIteratorPtr(nullptr);
  }
  sqlite3_reset(stmt);

  return DictionaryIteratorPtr(new SQLiteDictionaryIterator(this, keyword));
}

bool SQLiteDictionary::updateEntry(const DictionaryIteratorPtr &entry, const char *description)
{
  if(!entry.isValid()) return false;

  sqlite3 *db = getDB();
  if(db == nullptr) return false;

  sqlite3_stmt *stmt = getStmt(S_UPDATE_ENTRY);
  if(stmt == nullptr) return false;

  sqlite3_bind_text(stmt, 1, entry->getKeyword(), strlen(entry->getKeyword()), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, description, strlen(description), SQLITE_TRANSIENT);
  int modif_time = (int)time(nullptr);
  sqlite3_bind_int(stmt, 3, modif_time);
  if(sqlite3_step(stmt) != SQLITE_DONE) {
    errorString = std::string(sqlite3_errmsg(db));
    sqlite3_reset(stmt);
    return false;
  }
  sqlite3_reset(stmt);

  return true;
}

bool SQLiteDictionary::removeEntry(const DictionaryIteratorPtr &entry)
{
  if(!entry.isValid()) return false;

  sqlite3 *db = getDB();
  if(db == nullptr) return false;

  sqlite3_stmt *stmt = getStmt(S_REMOVE_ENTRY);
  if(stmt == nullptr) return false;

  sqlite3_bind_text(stmt, 1, entry->getKeyword(), strlen(entry->getKeyword()), SQLITE_TRANSIENT);
  if(sqlite3_step(stmt) != SQLITE_DONE) {
    errorString = std::string(sqlite3_errmsg(db));
    sqlite3_reset(stmt);
    return false;
  }
  sqlite3_reset(stmt);

  return true;
}


//================ SQLite Collation Callback ===============


static int compare_callback(void *collationPtr, int len1, const void *s1, int len2, const void *s2)
{
  CanonizedWord w1(15), w2(15);
  CollationComparator *comparator = static_cast<CollationComparator *>(collationPtr);
  w1 = comparator->canonizeWord(std::string((char *)s1, len1));
  w2 = comparator->canonizeWord(std::string((char *)s2, len2));
  return comparator->compare(w1, w2);
}

//================ Utils ===============

// static int strlenUTF8( const char *s ) 
// {
//   int len = 0;
//   while( *s != 0 ) {
//     if (Utf8::chartorune(&s) == 128) {
//       break;
//     }
//     len++;
//   }
//   return len;
// }
