/**
 * @file   shcm.cpp
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

#include <stdio.h>
#include <vector>

#include "shcm.h"
#include "shc.h"

using namespace std;

class SHCMImpl : public SHCM
{
public:
  SHCMImpl();

  virtual void startDecode(const std::string &tree);
  virtual void endDecode();

  virtual void startPreEncode();
  virtual std::string endPreEncode();

  virtual void preencode(const std::string &s);
  virtual string encode(const std::string &s);
  virtual string decode(const std::string &s);

protected:
  uint32 *freq;
  uchar  *symb;
  uchar  *len;
  uchar  *code;
  uchar  *aux;
  uchar  *base;
  uchar  *offs;
  uchar  *cache;

  uint32 tree[256];
  int tree_len;

  bool initialized;
};

SHCM *SHCM::create()
{
  return new SHCMImpl();
}

SHCMImpl::SHCMImpl()
{
  freq  = (uint32*) calloc(256, sizeof(uint32));
  symb  = (uchar*) calloc(256, sizeof(uchar));
  len   = (uchar*) calloc(256, sizeof(uchar));
  code  = (uchar*) calloc(256, sizeof(uchar));
  aux   = (uchar*) calloc(256, sizeof(uchar));
  base  = (uchar*) calloc(SH_MAXLENGTH, sizeof(uchar));
  offs  = (uchar*) calloc(SH_MAXLENGTH, sizeof(uchar));
  cache = (uchar*) calloc(256, sizeof(uchar));

  initialized = false;
}

void SHCMImpl::startPreEncode()
{
  for(int i = 0; i < 256; i++) {
    freq[i] = 0;
  }
}

string SHCMImpl::endPreEncode()
{
  int n;

  n = sh_SortFreq(freq, symb);
  sh_CalcLen(freq, symb, len, n, 31);
  sh_SortLen(len, symb, n);
  sh_CalcCode(len, symb, code, n);

  tree_len = sh_PackTree(len, symb, aux, tree, n);

  initialized = true;

  string ret;

  for(int i = 0; i < tree_len; i++) {
    ret.push_back((char) tree[i] & 0xFF);
    ret.push_back((tree[i]>>8) & 0xFF);
    ret.push_back((tree[i]>>16) & 0xFF);
    ret.push_back((tree[i]>>24) & 0xFF);
  }

  return ret;
}

void SHCMImpl::startDecode(const std::string &Tree)
{
  unsigned char *s = (unsigned char *) Tree.c_str();

  if(Tree.size() > sizeof(tree)) {
    return;
  }

  unsigned int treelen = Tree.size();

  for(unsigned int i = 0; i < treelen; i += 4) {
    tree[i/4] = s[i] + (s[i+1]<<8) + (s[i+2]<<16) + (s[i+3]<<24);
  }

  tree_len = treelen/4 + ((treelen%4) ? 1 : 0);

  int n = sh_ExpandTree(len, symb, (uint32*) tree);
  sh_SortLen(len, symb, n);
  sh_CalcDecode(len, symb, base, offs, cache, n);
  sh_CalcCode(len, symb, code, n);

  initialized = true;
}

void SHCMImpl::endDecode()
{
}

void SHCMImpl::preencode(const std::string &s)
{
  unsigned char *t = (unsigned char *) s.c_str();

  for(size_t i = 0; i < s.size(); i++) {
    freq[t[i]]++;
  }
}

string SHCMImpl::encode(const std::string &s)
{
  unsigned int i, bits;
  uint32 bitbuf;
  unsigned char *buf = (unsigned char *) s.c_str();
  std::string ret;

  ret.push_back(0);  // will be replaced later

  bits = 31;
  bitbuf = 0;
  for(i = 0; i < s.size(); i++) {
    uint32 symblen, symbcode;
    symblen = len[buf[i]];
    symbcode = code[buf[i]];
    if(symblen <= bits) {
      bitbuf <<= symblen;
      bitbuf |= symbcode;
      bits -= symblen;
    } else {
      bitbuf <<= bits;
      bitbuf |= (symbcode >> (symblen-bits));
      ret.push_back(bitbuf & 0xFF);
      ret.push_back((bitbuf >> 8) & 0xFF);
      ret.push_back((bitbuf >> 16) & 0xFF);
      ret.push_back((bitbuf >> 24) & 0xFF);
      bitbuf = symbcode;
      bits += (32-symblen);
    }
  }

  ret[0] = bits;

  if(bits < 32) {
    ret.push_back(bitbuf & 0xFF);
  }

  if(bits < 24) {
    ret.push_back((bitbuf >> 8) & 0xFF);
  }

  if(bits < 16) {
    ret.push_back((bitbuf >> 16) & 0xFF);
  }

  if(bits < 8) {
    ret.push_back((bitbuf >> 24) & 0xFF);
  }

  return ret;
}

std::string SHCMImpl::decode(const std::string &ss)
{
  unsigned int bits, symbol;
  uint32 bitbuf, bufpos;
  std::string ret;
  std::vector<uint32> s;
  unsigned int blen;
  unsigned char *t = (unsigned char *) ss.c_str();

  unsigned int lbits = t[0];
  unsigned int j = 1;
  blen = ss.size() - 1;
  for(unsigned int i = 0; i < (blen-1) / 4; i++) {
    s.push_back(t[j] + (t[j+1] << 8) + (t[j+2] << 16) + (t[j+3] << 24));

    j += 4;
  }

  bitbuf = 0;
  bits = 0;

  if(j < blen+1) {
    while(j < blen+1) {
      bitbuf = bitbuf | (t[j++] << bits);
      bits += 8;
    }

    bitbuf <<= lbits;
    s.push_back(bitbuf);
  }

  s.push_back(0);

  bits=31;
  bufpos = 0;
  bitbuf = s[bufpos++];

  blen = s.size() - 1;
  while(bufpos <= blen) {
    uint32 frame, codelen;

    if(bufpos == blen && bits == lbits) {
      break;
    }

    if(bits) {
      frame = (bitbuf << (32-bits)) | (s[bufpos] >> bits);
    } else {
      frame = s[bufpos];
    }

    codelen = cache[frame >> (32-SH_CACHEBITS)];
    if(codelen > SH_CACHEBITS) {
      while((frame >> (32-codelen)) < base[codelen]) {
        codelen++;
      }
    }

    symbol=symb[(frame>>(32-codelen))-base[codelen]+offs[codelen]];

    if(codelen <= bits) {
      bits -= codelen;
    } else {
      bits += 32 - codelen;
      bitbuf = s[bufpos++];
    }

    ret.push_back(symbol);
  }

  return ret;
}
