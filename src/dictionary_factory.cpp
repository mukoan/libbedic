/**
 * @file   dictionary_factory.cpp
 * @brief  Create dynamic or static dictionary from a file. Recognize format.
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

#include "bedic.h"

#include <string.h>
#include <stdio.h>

StaticDictionary *loadBedicDictionary( const char* filename, bool doCheckIntegrity, std::string &errorMessage );
DynamicDictionary *loadSQLiteDictionary( const char *fileName, std::string &errorMessage );
DynamicDictionary *loadHybridDictionary( const char *fileName, std::string &errorMessage );


StaticDictionary *StaticDictionary::loadDictionary( const char* filename, bool doCheckIntegrity, std::string &errorMessage )
{
  printf( "Loading dictionary: %s \n", filename );
  if(strlen(filename)>5 && strcmp(&filename[strlen(filename) - 5], ".edic")==0) {
    StaticDictionary *dic = loadSQLiteDictionary( filename, errorMessage );
    return dic;
  } if(strlen(filename)>5 && strcmp(&filename[strlen(filename) - 5], ".hdic")==0) {
    DynamicDictionary *dic = loadHybridDictionary( filename, errorMessage );
    return dic;
  } else {
    return loadBedicDictionary( filename, doCheckIntegrity, errorMessage );
  }
}

