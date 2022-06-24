/**
 * @file   shcm.h
 * @brief  
 * @author Lyndon Hill and others
 *
 * Copyright (C) 2002 Latchesar Ionkov <lionkov@yahoo.com>
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
#ifndef SHCM_H
#define SHCM_H

#include <string>

using namespace std;

class SHCM {
public:
  virtual void startDecode(const string& tree) = 0;
  virtual void endDecode() = 0;

  virtual void startPreEncode() = 0;
  virtual string endPreEncode() = 0;

  virtual void preencode(const string& s) = 0;
  virtual string encode(const string& s) = 0;
  virtual string decode(const string& s) = 0;

  static SHCM* create();
};

#endif  /* SHCM_H */

