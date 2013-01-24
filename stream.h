/*
 * @file stream.h
 * @author Emanuel Fiuza de Oliveira
 * @email efiuza87@gmail.com
 * @date Sun, 6 Jan 2013 15:28 -0300
 */


#ifndef _STREAM_H
#define _STREAM_H


/*
 * Stream opaque type definition.
 */
typedef struct stream stream;


/*
 * This function opens a stream for reading, writing or both.
 * On success, returns a pointer to a successfully opened stream. On error,
 * a NULL pointer is returned and "errno" is set to indicate the error.
 * Available modes:
 * "w"  : Write only. If file does not exist, create it. If it does, truncate
 *        its size to zero.
 * "w+" : Update mode. If file does not exist, create it. If it does, truncate
 *        its size to zero.
 * "r"  : Read only. If file does not exist, the function will fail.
 * "r+" : Update mode. If file does not exist, the function will fail.
 */
stream *stream_open(const char *path, const char *mode);


/*
 * This function writes "len" chars from the buffer pointed by "buf" to
 * the stream. On success, it returns the number of chars succcessfully written.
 * On error, returns -1 and "errno" is set to indicate the error.
 */
long stream_write(stream *strm, const void *buf, long len);


/*
 * This function reads at most "len" chars from the stream and writes then to
 * the buffer pointed by "buf". On success, returns the number of chars
 * successfully read, which might be less then "len" when the stream cursor
 * reaches the end of the stream. On error, return -1 and "errno" is set.
 */
long stream_read(stream *strm, void *buf, long len);


/*
 * This function updates the stream cursor. On success, returns the new value
 * of the stream cursor. On error, returns -1 and sets "errno" to indicate
 * the error. This function accepts "pos" to be negative. In such cases it is
 * interpreted as a position relative to the end of the stream.
 */
long stream_seek(stream *strm, long pos);


/*
 * This function returns the current stream cursor position on success.
 * On error, it returns -1 and sets "errno" to indicate the error.
 */
long stream_tell(stream *strm);


/*
 * This function returns the end-of-stream position on success.
 * On error, it returns -1 and sets "errno" to indicate the error.
 */
long stream_end(stream *strm);


/*
 * This function syncs the contents of the stream buffer with the undelying
 * file and returns 0 to indicate success. On error, it returns -1 and sets
 * "errno" to indicate the error.
 */
int stream_sync(stream *strm);


/*
 * This function syncs finalizes the stream.
 */
void stream_close(stream *strm);


#endif