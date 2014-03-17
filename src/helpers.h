/*
 * helpers.h
 *
 *  Created on: Feb 13, 2014
 *      Author: jrm
 */
#ifndef bool
typedef enum { FALSE, TRUE } bool;
#endif

#ifndef HELPERS_H_
#define HELPERS_H_

int h_mk_argv(const char *s, const char *delimiters, char ***argvp);
pid_t h_run_cmd(const char *cmd,const bool wait);
int h_len(const char** array);


#if defined(__SVR4) && defined(__sun)
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

#endif /* HELPERS_H_ */
