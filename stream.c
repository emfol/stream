/*
 * @file stream.c
 * @author Emanuel Fiuza de Oliveira
 * @email efiuza87@gmail.com
 * @date Sun, 6 Jan 2013 15:28 -0300
 */


#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "stream.h"


/*
 * Definitions
 */


#define PERMS (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)


/*
 * Main Data Structure
 */


struct stream {
	void *buf;  /* buffer address */
	long  off;  /* buffer offset */
	long  len;  /* buffer length */
	long  brk;  /* buffer break */
	long  blk;  /* block size */
	long  cur;  /* file cursor */
	long  end;  /* end of file position */
	long  siz;  /* file size */
	int   mod;  /* file mode */
	int   des;  /* file descriptor */
};


/*
 * Implementation
 */


stream *stream_open(const char *path, const char *mode) {

	stream *strmp, strm = { MAP_FAILED, 0, 0, 0, 0, 0, 0, 0, 0, -1 };
	struct stat inf;
	ldiv_t division;
	long lval;
	int errn, oflg;

	/* check parameters */
	if (path == NULL || (*mode != 'r' && *mode != 'w')) {
		errn = EINVAL;
		goto failure;
	}

	/* check system page size */
	strm.blk = sysconf(_SC_PAGESIZE);
	if (strm.blk < 1) {
		errn = ENOTSUP;
		goto failure;
	}

	/* determine file opening mode */
	oflg = *(mode + 1) == '+'
			? (*mode == 'w' ? O_RDWR|O_CREAT|O_TRUNC : O_RDWR)
			: (*mode == 'w' ? O_WRONLY|O_CREAT|O_TRUNC : O_RDONLY);

	/* open file */
	strm.des = open(path, oflg, PERMS);
	if (strm.des < 0) {
		errn = errno;
		goto failure;
	}

	/* set mmap protection mode */
	switch (oflg & O_ACCMODE) {
		case O_RDONLY:
			strm.mod = PROT_READ;
			break;
		case O_WRONLY:
			strm.mod = PROT_WRITE;
			break;
		default:
			strm.mod = PROT_READ|PROT_WRITE;
			break;
	}

	/* get file information */
	if (fstat(strm.des, &inf) != 0) {
		errn = errno;
		goto failure;
	}

	/* check file size */
	if (inf.st_size > LONG_MAX) {
		errn = EFBIG;
		goto failure;
	}

	/* save current file size */
	lval = (long)inf.st_size;
	strm.end = lval;
	strm.siz = lval;

	/* check file's best IO block size */
	if (inf.st_blksize < LONG_MAX) {
		lval = (long)inf.st_blksize;
		if (lval > strm.blk) {
			division = ldiv(lval, strm.blk);
			if (division.rem == 0 && division.quot > 1)
				strm.blk = lval;
		}
	}

	/* allocate space from the heap to save structure data */
	strmp = (stream *)malloc(sizeof(stream));
	if (strmp == NULL) {
		errn = errno;
		goto failure;
	}

	/* copy structure data to heap object */
	*strmp = strm;

	/* success */
	return strmp;

	failure:
		if (strm.des >= 0)
			close(strm.des);
		errno = errn;
		return NULL;

}


long stream_write(stream *s, const void *buf, long len) {

	register char *src, *dst;
	register long cnt;
	long wrl, brk;
	int errn;

	/* check parameters */
	if (s == NULL || buf == NULL || len < 0) {
		errn = EINVAL;
		goto failure;
	}

	/* check stream permissions */
	if ((s->mod & PROT_WRITE) != PROT_WRITE) {
		errn = EPERM;
		goto failure;
	}

	/* check limits */
	if (s->cur > LONG_MAX - len) {
		errn = EFBIG;
		goto failure;
	}

	/* calculate write break */
	brk = s->cur + len;

	/* initialize write counter */
	len = 0;

	/* writing loop */
	while (s->cur < brk) {

		/* check page faults */
		if (s->cur < s->off || s->cur >= s->brk || s->buf == MAP_FAILED) {
			if (s->buf != MAP_FAILED) {
				munmap(s->buf, s->len);
				s->buf = MAP_FAILED;
			}
			s->off = (s->cur / s->blk) * s->blk;
			s->len = s->blk;
			s->brk = s->off + s->len;
			if (s->brk > s->siz) {
				if (ftruncate(s->des, s->brk) != 0) {
					errn = errno;
					goto failure;
				}
				s->siz = s->brk;
			}
			s->buf = mmap(NULL, s->len, s->mod, MAP_SHARED, s->des, s->off);
			if (s->buf == MAP_FAILED) {
				errn = errno;
				goto failure;
			}
		}

		/* determine write length */
		wrl = (brk < s->brk ? brk : s->brk) - s->cur;

		/* calculate source and destination pointers */
		src = (void *)buf + len;
		dst = s->buf + (s->cur - s->off);

		/* copy bytes */
		for (cnt = wrl; cnt > 0; cnt--)
			*dst++ = *src++;

		/* update file position and counter */
		s->cur += wrl;
		len += wrl;

		/* check if end of file must be updated */
		if (s->cur > s->end)
			s->end = s->cur;

	}

	/* success */
	return len;

	failure:
		errno = errn;
		return -1;

}


long stream_read(stream *s, void *buf, long len) {

	register char *src, *dst;
	register long cnt;
	long rdl, brk;
	int errn;

	/* check parameters */
	if (s == NULL || buf == NULL || len < 0) {
		errn = EINVAL;
		goto failure;
	}

	/* check stream permissions */
	if ((s->mod & PROT_READ) != PROT_READ) {
		errn = EPERM;
		goto failure;
	}

	/* check limits */
	if (s->cur > s->end - len)
		len = s->end - s->cur;

	/* calculate read break */
	brk = s->cur + len;

	/* initialize write counter */
	len = 0;

	/* writing loop */
	while (s->cur < brk) {

		/* check page faults */
		if (s->cur < s->off || s->cur >= s->brk || s->buf == MAP_FAILED) {
			if (s->buf != MAP_FAILED) {
				munmap(s->buf, s->len);
				s->buf = MAP_FAILED;
			}
			s->off = (s->cur / s->blk) * s->blk;
			s->len = s->blk;
			s->brk = s->off + s->len;
			if (s->brk > s->siz) {
				s->len = s->siz - s->off;
				s->brk = s->off + s->len;
			}
			s->buf = mmap(NULL, s->len, s->mod, MAP_SHARED, s->des, s->off);
			if (s->buf == MAP_FAILED) {
				errn = errno;
				goto failure;
			}
		}

		/* determine write length */
		rdl = (brk < s->brk ? brk : s->brk) - s->cur;

		/* calculate source and destination pointers */
		src = s->buf + (s->cur - s->off);
		dst = buf + len;

		/* copy bytes */
		for (cnt = rdl; cnt > 0; cnt--)
			*dst++ = *src++;

		/* update file position and counter */
		s->cur += rdl;
		len += rdl;

	}

	/* success */
	return len;

	failure:
		errno = errn;
		return -1;

}


long stream_seek(stream *s, long pos) {

	int errn;

	if (s == NULL) {
		errn = EINVAL;
		goto failure;
	}

	if (s->cur != pos) {
		if (pos < 0)
			pos += s->end;
		if (pos < 0 || pos > s->end) {
			errn = ERANGE;
			goto failure;
		}
		s->cur = pos;
	}

	return pos;

	failure:
		errno = errn;
		return -1;

}


long stream_tell(stream *s) {
	if (s == NULL) {
		errno = EINVAL;
		return -1;
	}
	return s->cur;
}


long stream_end(stream *s) {
	if (s == NULL) {
		errno = EINVAL;
		return -1;
	}
	return s->end;
}


int stream_sync(stream *s) {

	int errn;

	if (s == NULL) {
		errn = EINVAL;
		goto failure;
	}

	if (s->buf != MAP_FAILED) {
		if (msync(s->buf, s->len, MS_SYNC) != 0) {
			errn = errno;
			goto failure;
		}
	}

	/* success */
	return 0;

	failure:
		errno = errn;
		return -1;

}


void stream_close(stream *s) {
	if (s != NULL) {
		if (s->buf != MAP_FAILED)
			munmap(s->buf, s->len);
		if (s->end < s->siz)
			ftruncate(s->des, s->end);
		close(s->des);
		free(s);
	}
}

