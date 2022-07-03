/**
 * @file   format_entry.cpp
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

#include <string>
#include <sstream>
#include <iostream>

#include <ctype.h>

// FIXME this is probably meant to be insertIndent but where is it used?
void insertIdent(std::ostringstream &out, int level, bool insertNewLine);
bool isFormatted(const std::string &text, int pos);

/**
 * Apply formating to a dictionary entry, including idents and new
 * lines.
 */
std::string formatDicEntry(std::string entry)
{
  std::ostringstream out;
  bool firstSubstr = true;

  int level = 0;
  int p = 0, ns = 0;
  while(ns < (int)entry.size()) {
    int np = entry.find('{', ns);
    if(np == -1) break;

    bool doFormat = false, incLevel = false;

//    std::cerr << "tag: " << entry.substr( np, 5 ) << "\n";

    // string::compare seems to malfunction in older libg++
#ifdef ARCH_ARM
    if(strcmp(entry.substr(np, 3).c_str(), "{s}") == 0 || strcmp(entry.substr(np, 4).c_str(), "{ss}" ) == 0) {
#else
    if(entry.compare(np, 3, "{s}") == 0 || entry.compare(np, 4, "{ss}") == 0) {
#endif
      incLevel = true;
      doFormat = true;
    }
#ifdef ARCH_ARM
    if(strcmp(entry.substr(np, 4).c_str(), "{/s}") == 0) {
#else
    if(entry.compare(np, 4, "{/s}") == 0) {
#endif
      level--;
      doFormat = true;
    }
#ifdef ARCH_ARM
    if(strcmp(entry.substr(np, 5).c_str(), "{/ss}") == 0) {
#else
    if(entry.compare(np, 5, "{/ss}") == 0) {
#endif
      level--;
    }

    int lp = np;
    if(doFormat) {
      while(lp > 0 && isspace(entry[lp-1]))
        lp--;
    }

    if(lp-1 > p) {
      out << entry.substr(p, lp-p);
      firstSubstr = false;
    }

    if(doFormat) {
      if(!firstSubstr) out << "\n";
      for(int i = 0; i < level; i++)
        out << "  ";
    }

    if(incLevel)
      level++;

    p = np;
    ns = p+1;
  }

  if(p < (int)entry.size())
    out << entry.substr(p);

  return out.str();
}

bool isFormatted(const std::string &text, int pos)
{
  int np = text.rfind( "\n", pos-1 );
  if(np == -1 || pos - np > 10)
    return false;
  return true;
}

void insertIdent(std::ostringstream &out, int level, bool insertNewLine)
{
  if(insertNewLine) out << "\n";
  for(int i = 0; i < level; i++)
    out << "  ";
}
