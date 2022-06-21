#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "file.h"

#define OUT_BUFFER_SIZE 8192

File::File() {
	fd = -1;
}

File::~File() {
	close();
}

int File::open(const char* fname) {
  if( !strcmp( fname, "-" ) )
    fd = 0;                     // stdin
  else 
    fd = ::open(fname, O_RDONLY);
  return fd;
}

int File::close() {
  int ret = 0;
  if (fd != -1 && fd != 1) {
    ret = ::close(fd);
  }
  fd = -1;
  return ret;
}

int File::size() {
	int p = lseek(fd, 0, SEEK_CUR);
	int ret = lseek(fd, 0, SEEK_END);
	lseek(fd, p, SEEK_SET);
	return ret;
}

int File::read(int offset, char* buf, int buflen) {
	lseek(fd, offset, SEEK_SET);
	return ::read(fd, buf, buflen);
}

 

DZFile::DZFile() : chunks( NULL ), inbuf( NULL ), outbuf( NULL )
{
	zstream.zalloc = 0;
	zstream.zfree = 0;
	zstream.opaque = 0;
	zstream.next_in = 0;
	zstream.next_out = 0;
	zstream.avail_out = 0;
	inflateInit2(&zstream, -15);
}

DZFile::~DZFile() {
	close();
	inflateEnd(&zstream);
}

int DZFile::open(const char* fname) {
	int ret = File::open(fname);
	if (ret < 0) {
		return ret;
	}

	unsigned char buf[22];
	int flags;

	if (::read(fd, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	flags = buf[3];
	// check if it is GZIP file and if there is FEXTRA field
	if (buf[0] != 0x1f || buf[1] != 0x8b || !(flags&0x04)) {
		return -1;
	}

	// check if the extra field is the one we are looking for
	if (buf[12] != 'R' || buf[13] != 'A') {
		return -1;
	}

	// check if the subfield version is the one we support
	if (buf[16] != 1 || buf[17] != 0) {
		return -1;
	}

	chunkLen = buf[18] + (buf[19]<<8);
	chunkCount = buf[20] + (buf[21]<<8);
	unsigned char* tmp = new unsigned char[chunkCount * 2];
	chunks = new int[chunkCount + 1];
	::read(fd, tmp, chunkCount * 2);
	chunks[0] = 0;
	for(int i = 0; i < chunkCount; i++) {
		int x = tmp[i*2] | (tmp[i*2+1]<<8);
		chunks[i+1] = chunks[i] + x;
	}
	delete[] tmp;

	// FNAME
	if (flags & 0x08) {
		while (::read(fd, buf, 1)) {
			if (buf[0] == 0) {
				break;
			}
		}
	}

	// COMMENT
	if (flags & 0x10) {
		while (::read(fd, buf, 1)) {
			if (buf[0] == 0) {
				break;
			}
		}
	}

	// FHCRC
	if (flags & 0x02) {
		::read(fd, buf, 2);
	}

	// update chunk offsets
	int cstart = lseek(fd, 0, SEEK_CUR);
	for(int i = 0; i <= chunkCount; i++) {
		chunks[i] += cstart;
	}

	lseek(fd, -4, SEEK_END);
	::read(fd, buf, 4);
	fsize = buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);

	inbuf = new char[chunkLen];
	outbufsize = chunkLen + chunkLen / 9 + 12;
	outbuf = new char[outbufsize];
	cchunk = -1;

	return 0;
}

int DZFile::close() {
	if (chunks) {
		delete[] chunks;
		chunks = 0;
	}

	if (inbuf) {
		delete[] inbuf;
		inbuf = 0;
	}

	if (outbuf) {
		delete[] outbuf;
		outbuf = 0;
	}

	return File::close();
}

int DZFile::size() {
	if (fd < 0) {
		return -1;
	}

	return fsize;
}

int DZFile::read(int pos, char* buf, int buflen) {
	if (fd < 0) {
		return -1;
	}

	int cp = pos / chunkLen;
	int co = pos - cp * chunkLen;

	int n = buflen;
	while (n>0 && cp<chunkCount) {
		if (cchunk != cp) {
			lseek(fd, chunks[cp], SEEK_SET);
			::read(fd, inbuf, chunks[cp+1] - chunks[cp]);
			zstream.next_in = (Bytef *) inbuf;
			zstream.avail_in = chunks[cp+1] - chunks[cp];
			zstream.next_out = (Bytef *) outbuf;
			zstream.avail_out = outbufsize;
			if (inflate(&zstream, Z_PARTIAL_FLUSH) != Z_OK) {
				return -1;
			}

			cchunk = cp;
			outbuflen = zstream.next_out - (Bytef *) outbuf;
		}

		int len = n;
		if (co+len > outbuflen) {
			len = outbuflen-co;
		}

		memcpy(&buf[buflen - n], &outbuf[co], len);

		co = 0;
		cp++;
		n -= len;
	}

	return buflen - n;
}
