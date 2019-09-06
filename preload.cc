/*
 * Copyright (c) 2019 Carnegie Mellon University,
 * Copyright (c) 2019 Triad National Security, LLC, as operator of
 *     Los Alamos National Laboratory.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of CMU, TRIAD, Los Alamos National Laboratory, LANL, the
 *    U.S. Government, nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * preload.cc - redirect LANL pfind ops to tablefs
 */
#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * logging facilities and helpers
 */
#define ABORT_FILENAME \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define ABORT(msg) msg_abort(errno, msg, __func__, ABORT_FILENAME, __LINE__)

/* abort with an error message */
static void msg_abort(int err, const char* msg, const char* srcfcn,
                      const char* srcf, int srcln);

/*
 * next_functions: libc replacement functions we are providing to the preloader.
 */
static struct next_functions {
  int (*lstat)(const char* path, struct stat* buf);
  DIR* (*opendir)(const char* path);
  struct dirent* (*readdir)(DIR* dirp);
  int (*closedir)(DIR* dirp);
} nxt = {0};

/*
 * this once is used to trigger the init of the preload library.
 */
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

/* helper: must_getnextdlsym: get next symbol or die */
static void must_getnextdlsym(void** result, const char* symbol) {
  *result = dlsym(RTLD_NEXT, symbol);
  if (!(*result)) ABORT(symbol);
}

/*
 * preload_init: called via init_once. if this fails we are sunk, so
 * we'll abort the process....
 */
static void preload_init() {
#define MUST_GETNEXTDLSYM(x) must_getnextdlsym((void**)(&nxt.x), #x)
  MUST_GETNEXTDLSYM(opendir);
  MUST_GETNEXTDLSYM(readdir);
  MUST_GETNEXTDLSYM(closedir);
  MUST_GETNEXTDLSYM(lstat);
}

/*
 * here are the actual override functions from libc.
 */
extern "C" {

int lstat(const char* path, struct stat* const buf) {
  int rv = pthread_once(&init_once, preload_init);
  if (rv != 0) ABORT("pthread_once");
  fprintf(stderr, "lstat(%s)\n", path);
  return nxt.lstat(path, buf);
}

DIR* opendir(const char* path) {
  int rv = pthread_once(&init_once, preload_init);
  if (rv != 0) ABORT("pthread_once");
  fprintf(stderr, "opendir(%s)\n", path);
  return nxt.opendir(path);
}

struct dirent* readdir(DIR* dirp) {
  int rv = pthread_once(&init_once, preload_init);
  if (rv != 0) ABORT("pthread_once");
  fprintf(stderr, "readdir(%p)\n", dirp);
  return nxt.readdir(dirp);
}

int closedir(DIR* dirp) {
  int rv = pthread_once(&init_once, preload_init);
  if (rv != 0) ABORT("pthread_once");
  fprintf(stderr, "closedir(%p)\n", dirp);
  return nxt.closedir(dirp);
}

} /* extern "C" */

static void msg_abort(int err, const char* msg, const char* srcfcn,
                      const char* srcf, int srcln) {
  fputs("*** ABORT *** ", stderr);
  fprintf(stderr, "@@ %s:%d @@ %s] ", srcf, srcln, srcfcn);
  fputs(msg, stderr);
  if (err != 0) fprintf(stderr, ": %s (errno=%d)", strerror(err), err);
  fputc('\n', stderr);
  abort();
}
