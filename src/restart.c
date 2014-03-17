/*
 * restart.c
 *
 *  Created on: Feb 13, 2014
 *      Author: jrm
 *
 *  Slightly modified from code in:
 *     Unix Systems Programming: Communication, Concurrency, and Threads
 *     By Kay A. Robbins , Steven Robbins
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include "restart.h"

#ifndef BLKSIZE
#define BLKSIZE PIPE_BUF
#endif

#ifndef MILLION
#define MILLION 1000000L
#endif

#ifndef D_MILLION
#define D_MILLION 1000000.0
#endif

/**
 * The following code segment waits for all children that have finished but avoids blocking if there
 * are no children whose status is available. It restarts waitpid if that function is interrupted by a
 * signal or if it successfully waited for a child.
 */
pid_t r_wait_all() {
	pid_t childpid;
	while (childpid = waitpid(-1, NULL, WNOHANG)) {
		if ((childpid == -1) && (errno != EINTR)) {
			break;
		}
	}
	return childpid;
}


/* Private functions */
static int gettimeout(struct timeval end,struct timeval *timeoutp) {
	gettimeofday(timeoutp, NULL);
	timeoutp->tv_sec = end.tv_sec - timeoutp->tv_sec;
	timeoutp->tv_usec = end.tv_usec - timeoutp->tv_usec;if (timeoutp->tv_usec >= MILLION) {
		timeoutp->tv_sec++;
		timeoutp->tv_usec -= MILLION;
	}
	if (timeoutp->tv_usec < 0) {
		timeoutp->tv_sec--;
		timeoutp->tv_usec += MILLION;
	}
	if ((timeoutp->tv_sec < 0) ||
			((timeoutp->tv_sec == 0) && (timeoutp->tv_usec == 0))) {
		errno = ETIME;
		return -1;
	}
	return 0;
}

/* Restart versions of traditional functions */

/**
 * closes fildes. If successful, r_close returns 0. If unsuccessful, r_close returns
 * –1 and sets errno. The implementation calls close in a loop, restarting if close
 * returns –1 with errno set to EINTR.
 */
int r_close(int fildes) {
	int retval;
	while (retval = close(fildes), retval == -1 && errno == EINTR) ;
	return retval;
}

/**
 * closes fildes2 if it was open and causes fildes2 to refer to the same file as
 * fildes. If successful, r_dup2 returns fildes2. If unsuccessful, r_dup2 returns –
 * 1 and sets errno. The implementation calls dup2 in a loop, restarting if dup2
 * returns –1 with errno set to EINTR.
 */
int r_dup2(int fildes, int fildes2) {
	int retval;
	while (retval = dup2(fildes, fildes2), retval == -1 && errno == EINTR) ;
	return retval;
}

/**
 * opens a file descriptor for path. The oflag should not have the O_CREAT bit set.
 * If successful, r_open2 returns an open file descriptor. If unsuccessful, r_open2
 * returns –1 and sets errno. The implementation calls open in a loop, restarting if
 * open returns –1 with errno set to EINTR.
 */
int r_open2(const char *path, int oflag) {
	int retval;
	while (retval = open(path, oflag), retval == -1 && errno == EINTR) ;
	return retval;
}

/**
 * opens a file descriptor for path. The oflag should have the O_CREAT bit set. If
 * successful, r_open3 returns an open file descriptor. If unsuccessful, r_open3
 * returns –1 and sets errno. The implementation calls open in a loop, restarting if
 * open returns –1 with errno set to EINTR.
 */
int r_open3(const char *path, int oflag, mode_t mode) {
	int retval;
	while (retval = open(path, oflag, mode), retval == -1 && errno == EINTR) ;
	return retval;
}

/**
 * reads at most size bytes from the open file descriptor fd into buf. If successful,
 * r_read returns the number of bytes read. If unsuccessful, r_read returns –1 and
 * sets errno. The implementation calls read in a loop, restarting if read returns –1
 * with errno set to EINTR.
 */
ssize_t r_read(int fd, void *buf, size_t size) {
	ssize_t retval;
	while (retval = read(fd, buf, size), retval == -1 && errno == EINTR) ;
	return retval;
}

/**
 * suspends execution of the calling thread until status information for one of its
 * terminated children is available. If successful, r_wait returns the process ID of a
 * terminated child process. If unsuccessful, r_wait returns –1 and sets errno. The
 * implementation calls wait in a loop, restarting if wait returns –1 with errno set
 * to EINTR.
 */
pid_t r_wait(int *stat_loc) {
	pid_t retval;
	while (((retval = wait(stat_loc)) == -1) && (errno == EINTR)) ;
	return retval;
}

/**
 * suspends execution of the calling thread until status information is available for a
 * specified child process. If successful, r_waitpid returns the process ID of a child
 * process. If unsuccessful, r_waitpid returns –1 and sets errno. The
 * implementation calls waitpid in a loop, restarting if waitpid returns –1 with
 * errno set to EINTR.
 */
pid_t r_waitpid(pid_t pid, int *stat_loc, int options) {
	pid_t retval;while (((retval = waitpid(pid, stat_loc, options)) == -1) &&
			(errno == EINTR)) ;
	return retval;
}

/**
 * attempts to write exactly size bytes from buf to the open file descriptor fd. If
 * successful, r_write returns size. If unsuccessful, r_write returns –1 and sets
 * errno. The only possible return values are size and –1. The implementation
 * calls write in a loop, restarting if write returns –1 with errno set to EINTR. If
 * write does not output all the requested bytes, r_write continues to call write
 * until all the bytes have been written or an error occurs.
 */
ssize_t r_write(int fd, void *buf, size_t size) {
	char *bufp;
	size_t bytestowrite;
	ssize_t byteswritten;
	size_t totalbytes;
	for (bufp = buf, bytestowrite = size, totalbytes = 0;
			bytestowrite > 0;
			bufp += byteswritten, bytestowrite -= byteswritten) {
		byteswritten = write(fd, bufp, bytestowrite);
		if ((byteswritten) == -1 && (errno != EINTR))
			return -1;
		if (byteswritten == -1)
			byteswritten = 0;
		totalbytes += byteswritten;
	}
	return totalbytes;
}
/* Utility functions */

/**
 * returns a struct timeval corresponding to the current time plus seconds
 * seconds. The implementation calls gettimeofday to get the current time,
 * converts the seconds parameter to integer values representing seconds and
 * microseconds, and adds these values to the current time.
 */
struct timeval r_add2currenttime(double seconds) {
	struct timeval newtime;
	gettimeofday(&newtime, NULL);
	newtime.tv_sec += (int)seconds;
	newtime.tv_usec += (int)((seconds - (int)seconds)*D_MILLION + 0.5);
	if (newtime.tv_usec >= MILLION) {
		newtime.tv_sec++;
		newtime.tv_usec -= MILLION;
	}
	return newtime;
}

/**
 * copies bytes from open file descriptor fromfd to open file descriptor tofd until
 * either end-of-file or an error occurs. If successful, r_copyfile returns the number
 * of bytes copied. If unsuccessful, r_copyfile returns –1 and sets errno. The
 * r_copyfile function does not return an error if any bytes are successfully copied,
 * even if an error occurs on a subsequent write that follows a successful read.
 */
int r_copyfile(int fromfd, int tofd) {
	int bytesread;
	int totalbytes = 0;
	while ((bytesread = r_readwrite(fromfd, tofd)) > 0)
		totalbytes += bytesread;
	return totalbytes;
}

/**
 * attempts to read exactly size bytes from the open file descriptor fd into the
 * buf. If r_readblock reaches end-of-file before reading any bytes, it returns 0. If
 * exactly size bytes are read, r_readblock returns size. If unsuccessful, r_readblock
 * returns –1 and sets errno. If r_readblock encounters end-of-file after some but
 * not all of the needed bytes, the function returns –1 and sets errno to EINVAL.
 */
ssize_t r_readblock(int fd, void *buf, size_t size) {
	char *bufp;
	ssize_t bytesread;
	size_t bytestoread;
	size_t totalbytes;
	for (bufp = buf, bytestoread = size, totalbytes = 0;
			bytestoread > 0;
			bufp += bytesread, bytestoread -= bytesread) {bytesread = read(fd, bufp, bytestoread);
			if ((bytesread == 0) && (totalbytes == 0))
				return 0;
			if (bytesread == 0) {
				errno = EINVAL;
				return -1;
			}
			if ((bytesread) == -1 && (errno != EINTR))
				return -1;
			if (bytesread == -1)
				bytesread = 0;
			totalbytes += bytesread;
	}
	return totalbytes;
}

/**
 * attempts to read a line from the open file descriptor fd into buf, a buffer of size
 * size. If r_readline reaches end-of-file before reading any bytes, it returns 0. If
 * successful, buf contains a string ending with a newline. The r_readline function
 * returns the length of the string. If unsuccessful, r_readline returns –1 and sets
 * errno. Two errors are possible other than an error reading from fd: end-of-file
 * before newline or size-1 bytes read before newline. Both errors cause r_readline
 * to set errno to EINVAL.
 */
int r_readline(int fd, char *buf, int nbytes) {
	int numread = 0;
	int returnval;
	while (numread < nbytes - 1) {
		returnval = read(fd, buf + numread, 1);
		if ((returnval == -1) && (errno == EINTR))
			continue;
		if ((returnval == 0) && (numread == 0))
			return 0;
		if (returnval == 0)
			break;
		if (returnval == -1)
			return -1;
		numread++;
		if (buf[numread-1] == '\n') {
			buf[numread] = '\0';
			return numread;
		}
	}
	errno = EINVAL;
	return -1;
}

/**
 * attempts to read at most nbyte bytes from the open file descriptor fd into the
 * buffer buf. The r_readtimed function behaves the same as r_read unless no bytes
 * are available in a number of seconds given by seconds. If no bytes are available
 * within the timeout period, r_readtimed returns –1 and sets errno to ETIME. If
 * interrupted by a signal, r_readtimed restarts but maintains the original ending
 * timeout.
 */
ssize_t r_readtimed(int fd, void *buf, size_t nbyte, double seconds) {
	struct timeval timedone;
	timedone = r_add2currenttime(seconds);
	if (r_waitfdtimed(fd, timedone) == -1)
		return (ssize_t)(-1);
	return r_read(fd, buf, nbyte);
}

/**
 * reads at most PIPE_BUF bytes from open file descriptor fromfd and writes the
 * bytes read to the open file descriptor tofd. If successful, r_readwrite returns the
 * number of bytes copied. If r_readwrite reaches end-of-file on fromfd, it returns
 * 0. If unsuccessful, r_readwrite returns –1 and sets errno.
 */
int r_readwrite(int fromfd, int tofd) {
	char buf[BLKSIZE];
	int bytesread;
	if ((bytesread = r_read(fromfd, buf, BLKSIZE)) < 0)
		return -1;
	if (bytesread == 0)
		return 0;if (r_write(tofd, buf, bytesread) < 0)
			return -1;
		return bytesread;
}

/**
 * reads exactly size bytes from the open file descriptor fromfd and writes them to
 * the open file descriptor tofd. The buf parameter is a buffer of size size. If
 * successful, r_readwriteblock returns size and the bytes read are in buf. If
 * r_readwriteblock reaches end-of-file on fromfd before any bytes are read, it
 * returns 0. If unsuccessful, r_readwriteblock returns –1 and sets errno.
 */
int r_readwriteblock(int fromfd, int tofd, char *buf, int size) {
	int bytesread;
	bytesread = r_readblock(fromfd, buf, size);
	if (bytesread != size)
		/* can only be 0 or -1 */
		return bytesread;
	return r_write(tofd, buf, size);
}

/**
 * waits until data is available to be read from file descriptor fd or until the current
 * time is later than the time in end. If a read on fd will not block, r_waitfdtimed
 * returns 0. If unsuccessful, r_waitfdtimed returns –1 and sets errno. If fd will still
 * block when time end occurs, r_waitfdtimed sets errno to ETIME. If fd is negative
 * or greater than or equal to FD_SETSIZE, r_waitfdtimed sets errno to EINVAL.
 */
int r_waitfdtimed(int fd, struct timeval end) {
	fd_set readset;
	int retval;
	struct timeval timeout;
	if ((fd < 0) || (fd >= FD_SETSIZE)) {
		errno = EINVAL;
		return -1;
	}
	FD_ZERO(&readset);
	FD_SET(fd, &readset);
	if (gettimeout(end, &timeout) == -1)
		return -1;
	while (((retval = select(fd+1, &readset, NULL, NULL, &timeout)) == -1)
			&& (errno == EINTR)) {
		if (gettimeout(end, &timeout) == -1)
			return -1;
		FD_ZERO(&readset);
		FD_SET(fd, &readset);
	}
	if (retval == 0) {
		errno = ETIME;
		return -1;
	}
	if (retval == -1)
		return -1;
	return 0;
}
