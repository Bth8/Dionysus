/* note these headers are all provided by newlib - you don't need to provide them */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>
 
void _exit() { ; }
int close(int file) { return -1; }
char **environ; /* pointer to array of char * strings that define the current environment variables */
int execve(char *name, char **argv, char **env) { return -1; }
int fork() { return -1; }
int fstat(int file, struct stat *st) { return -1; }
int getpid() { return -1; }
int isatty(int file) { return -1; }
int kill(int pid, int sig) { return -1; }
int link(char *old, char *new) { return -1; }
int lseek(int file, int ptr, int dir) { return -1; }
int open(const char *name, int flags, ...) { return -1; }
int read(int file, char *ptr, int len) { return -1; }
caddr_t sbrk(int incr) { return 0; }
int stat(const char *file, struct stat *st) { return -1; }
clock_t times(struct tms *buf) { return 0; }
int unlink(char *name) { return -1; }
int wait(int *status) { return -1; }
int write(int file, char *ptr, int len) { return -1; }
int gettimeofday(struct timeval *p, void *z) { return -1; }