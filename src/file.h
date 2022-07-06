/**
 * @file   file.h
 * @brief  Handle compressed files
 * @author Lyndon Hill and others
 */

#pragma once
#ifndef FILE_H
#define FILE_H

extern "C" {
#include <zlib.h>
}

/**
 * @class File
 */
class File {
protected:
  int fd;

public:
  File();
  virtual ~File();

  virtual int open(const char *fname);
  virtual int close();
  virtual int size();
  virtual int read(int pos, char *buf, int buflen);
};

/**
 * @class DZFile
 */
class DZFile : public File {
public:
  DZFile();
  virtual ~DZFile();

  virtual int open(const char *fname) override;
  virtual int close() override;
  virtual int size() override;
  virtual int read(int pos, char *buf, int buflen) override;

protected:
  z_stream zstream;
  int   fsize;
  int   chunkLen;
  int   chunkCount;
  int  *chunks;
  char *inbuf;
  int   outbuflen;
  int   outbufsize;
  char *outbuf;
  int   cchunk;      //< current chunk
};

#endif  /* FILE_H */

