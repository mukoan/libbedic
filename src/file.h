#ifndef FILE_H
#define FILE_H

extern "C" {
#include <zlib.h>
}

class File {
protected:
	int fd;

public:
	File();
	virtual ~File();

	virtual int open(const char* fname);
	virtual int close();
	virtual int size();
	virtual int read(int pos, char* buf, int buflen);
};

class DZFile : public File {
public:
	DZFile();
	virtual ~DZFile();

	virtual int open(const char* fname);
	virtual int close();
	virtual int size();
	virtual int read(int pos, char* buf, int buflen);

protected:
	z_stream zstream;
	int fsize;
	int chunkLen;
	int chunkCount;
	int* chunks;
	char* inbuf;
	int outbuflen;
	int outbufsize;
	char* outbuf;
	int cchunk;	// current chunk
};

#endif
