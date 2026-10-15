/* SPDX-License-Identifier: GPL-3.0-only
 * ESTAMPIE: WASI has no processes. tests/hostsim.c's own test runners (its main, unused in the browser build) fork
 * and wait; these stubs fail the way a libc without processes does (ENOSYS), so the shared source compiles
 * unchanged. Found first on the include path (-Iweb/emu/wasi) by web/emu/build.sh. */
#ifndef ESTAMPIE_WASI_SYS_WAIT_H
#define ESTAMPIE_WASI_SYS_WAIT_H
#include <errno.h>
#include <unistd.h>

static inline pid_t fork(void) { errno = ENOSYS; return -1; }
static inline pid_t waitpid(pid_t pid, int *status, int options) { (void)pid; (void)status; (void)options; errno = ENOSYS; return -1; }
int pipe(int fd[2]);                     /* (wasi-libc declares it in unistd.h, without a definition) */
int pipe(int fd[2]) { (void)fd; errno = ENOSYS; return -1; }

#define WIFEXITED(s) 0
#define WEXITSTATUS(s) 0
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
#endif
