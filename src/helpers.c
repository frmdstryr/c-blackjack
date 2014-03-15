/*
 * helpers.c
 *
 *  Created on: Feb 13, 2014
 *      Author: jrm
 *
 *  Slightly modified from code in:
 *     Unix Systems Programming: Communication, Concurrency, and Threads
 *     By Kay A. Robbins , Steven Robbins
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "restart.h"
#include "helpers.h"

/**
 * Splits a string into an array of arguments
 * @see
 */
int h_mk_argv(const char *s, const char *delimiters, char ***argvp) {
	int error;
	int i;
	int numtokens;
	const char *snew;
	char *t;
	if ((s == NULL) || (delimiters == NULL) || (argvp == NULL)) {
		errno = EINVAL;
		return -1;
	}
	*argvp = NULL;
	snew = s + strspn(s, delimiters);
	/* snew is real start of string */
	if ((t = malloc(strlen(snew) + 1)) == NULL)
		return -1;
	strcpy(t, snew);
	numtokens = 0;
	if (strtok(t, delimiters) != NULL)
		/* count the number of tokens in s */
		for (numtokens = 1; strtok(NULL, delimiters) != NULL; numtokens++) ;
	/* create argument array for ptrs to the tokens */
	if ((*argvp = malloc((numtokens + 1)*sizeof(char *))) == NULL) {
		error = errno;
		free(t);
		errno = error;
		return -1;
	}
	/* insert pointers to tokens into the argument array */
	if (numtokens == 0)free(t);
	else {
		strcpy(t, snew);
		**argvp = strtok(t, delimiters);
		for (i = 1; i < numtokens; i++)
			*((*argvp) + i) = strtok(NULL, delimiters);
	}
	*((*argvp) + numtokens) = NULL;
	/* put in final NULL pointer */
	return numtokens;
}



/**
 * Creates a child process to execute a command.
 * @return pid_t of the process
 */
pid_t h_run_cmd(const char *cmd,const bool wait) {
	pid_t childpid;
	char **args;

	// If invalid args, return invalid
	if (cmd == NULL) {
		errno = EINVAL;
		return -1;
	}

	// Parse the cmd into args
	h_mk_argv(cmd," \t\n",&args);

	// Spawn the new process
	childpid = fork();
	if (childpid == -1) {
		perror("Failed to fork");
		return childpid;
	}

	// If child process, execute the cmd
	if (childpid == 0) {
		execvp(args[0], &args[0]);
		perror("Child failed to execvp the command");
		exit(-1);
	}

	// Free args
	if (args == NULL) {
		if (*args != NULL) {
			free(*args);
		}
		free(args);
	}

	// Wait for it to finish (if requested)
	if (wait) {
		if (childpid != r_wait(NULL)) {
			perror("Failed to wait");
		}
	}
	return childpid;
}

int h_len(const char** array) {
  int count = 0;
  while(array[count]) count++;
  return count;
}

/**
 * Returns TRUE if the string is in the given array.
 */
bool h_str_in(const char *s, const char *a[]) {
	int l = h_len(a);
	int i=0;
	for (i=0;i<l;i++) {
		if (strcmp(s,a[i])==0) {
			return TRUE;
		}
	}
	return FALSE;
}

///**
// * print to a string by appending the two
// */
//char *h_sprintf(char *str, char *format,...) {
//	va_list args;
//	va_start(args, format);
//	char *buf;
//	vsprintf(buf,format, args);
//	va_end(args);
//	return h_strcat(str,buf);
//}
//
///**
// * Append two strings
// */
//char *h_strcat(char *str1, char *str2) {
//	char * new_str ;
//	if((new_str = malloc(strlen(str1)+strlen(str2)+1)) != NULL){
//		new_str[0] = '\0';   // ensures the memory is an empty string
//		strcat(new_str,str1);
//		strcat(new_str,str2);
//		return new_str;
//	}
//	return NULL;
//}
#if defined(__SVR4) && defined(__sun)
int vasprintf(char **strp, const char *fmt, va_list ap) {
	size_t size = vsnprintf(NULL, 0, fmt, ap) + 1;
	char *buffer = calloc(1, size);
	if (!buffer) {
		return -1;
	}
	return vsnprintf(buffer, size, fmt, ap);
}

int asprintf(char **strp, const char *fmt, ...){
	int error;
	va_list ap;
	va_start(ap, fmt);
	error = vasprintf(strp, fmt, ap);
	va_end(ap);
	return error;
}

#define _GETLINE_BUFLEN 255

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
	int c;
	size_t alloced = 0;
	char *linebuf;

	if (*lineptr == NULL) {
		linebuf = malloc(sizeof(char) * (_GETLINE_BUFLEN + 1));
		alloced = _GETLINE_BUFLEN + 1;
	} else {
		linebuf = *lineptr;
		alloced = *n;
	}
	ssize_t linelen = 0;

	do {
		c = fgetc(stream);
		if (c == EOF) {
			break;
		}
		if (linelen >= alloced) {
			linebuf = realloc(linebuf, sizeof(char) * (alloced + _GETLINE_BUFLEN + 1));
			alloced += (_GETLINE_BUFLEN + 1);
		}
		*(linebuf + linelen) = (unsigned char)c;
		linelen++;
	} while (c != '\n');

	/* empty line means EOF or some other error */
	if (linelen == 0) {
		if (linebuf != NULL && *lineptr == NULL) {
			free(linebuf);
			linebuf = NULL;
		}
		linelen = -1;
		*n = alloced;
	} else {
		if (linebuf != NULL) {
			linebuf[linelen] = '\0';
		}
		*n = alloced;
		*lineptr = linebuf;
	}

	return linelen;
}
#endif
