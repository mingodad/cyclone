#ifndef _UNISTD_H
#define _UNISTD_H

#include <core.h>
#include <sys/types.h>
#include <getopt.h> // Since ordinary getopt() belongs in unistd.h
// This means you need "using Getopt" if you're "using Unistd"

namespace Unistd {

#ifndef SEEK_SET
#define SEEK_SET        0       // set file offset to offset 
#endif
#ifndef SEEK_CUR
#define SEEK_CUR        1       // set file offset to current plus offset 
#endif
#ifndef SEEK_END
#define SEEK_END        2       // set file offset to EOF plus offset 
#endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

  extern "C" {
    unsigned alarm(unsigned seconds);
    int close(int);
    pid_t getpid(void);
    pid_t getppid(void);
    pid_t fork(void);
    int fchdir(int);
    int dup(int);
    int dup2(int, int);
    uid_t getuid(void);  int setuid(uid_t uid);
    uid_t geteuid(void); int seteuid(uid_t euid);
    gid_t getgid(void);  int setgid(gid_t gid);
    gid_t getegid(void); int setegid(gid_t egid);
    int select(int n, fd_set *readfds, fd_set *writefds,
               fd_set *exceptfds, struct timeval *timeout);
    int pipe(int @{2}`r filedes);
    off_t lseek(int filedes, off_t offset, int whence);
  }

  int chdir(string_t);
  char ?`r getcwd(char ?`r buf, size_t size);
  int execl(string_t path, string_t arg0, ...`r string_t argv);
  int execlp(string_t file, string_t arg0, ...`r string_t argv);
  //int execv(string_t path, string_t const ?argv);
  //int execp(string_t file, string_t const ?argv);
  int execve(string_t filename, string_t const ?argv, string_t const ?envp);
  ssize_t read(int fd, mstring_t buf, size_t count);
  ssize_t write(int fd, string_t buf, size_t count);
  int unlink(string_t pathname);
}

#endif
