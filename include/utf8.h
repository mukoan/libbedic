/****************************************************************************
 * utf8.h
 *
 * The content of this file is based on 9libs library
 *
 * The authors of this software are Rob Pike and Howard Trickey.
 *		Copyright (c) 1998 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 ****************************************************************************/

#ifndef _UTF8_H
#define _UTF8_H

#include <string>
using namespace std;

class Utf8 {
	public:
		static int tolower(const char* s, char* buf, int buflen);
		static int toupper(const char* s, char* buf, int buflen);

		static int tolower(const string&, string&);
		static int toupper(const string&, string&);

		static unsigned int chartorune(const char** s);
		static int runetochar(char* s, int rune);
                static unsigned int runetoupper(unsigned int c);                
};

#endif
