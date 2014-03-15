/*
 * restart.h
 *
 *  Created on: Feb 13, 2014
 *      Author: jrm
 *
 *  Slightly modified from code in:
 *     Unix Systems Programming: Communication, Concurrency, and Threads
 *     By Kay A. Robbins , Steven Robbins
 */
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef RESTART_H_
#define RESTART_H_

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

int r_wait_all();
struct timeval r_add2currenttime(double seconds);
int r_copyfile(int fromfd, int tofd);
int r_close(int fildes);
int r_dup2(int fildes, int fildes2);
int r_open2(const char *path, int oflag);
int r_open3(const char *path, int oflag, mode_t mode);
ssize_t r_read(int fd, void *buf, size_t size);
pid_t r_wait(int *stat_loc);
pid_t r_waitpid(pid_t pid, int *stat_loc, int options);
ssize_t r_write(int fd, void *buf, size_t size);
ssize_t r_readblock(int fd, void *buf, size_t size);
int r_readline(int fd, char *buf, int nbytes);
ssize_t r_readtimed(int fd, void *buf, size_t nbyte, double seconds);
int r_readwrite(int fromfd, int tofd);
int r_readwriteblock(int fromfd, int tofd, char *buf, int size);
int r_waitfdtimed(int fd, struct timeval end);

#endif /* RESTART_H_ */
