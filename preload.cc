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
 * preload.cc - redirect LANL GUFI/parallel_find fs ops to tablefs. Currently,
 * only lstat, opendir, readdir, and closedir functions are redirected, and
 * redirection is only triggered when the pathname passed to us starts with a
 * specific prefix (e.g., /tablefs). Code is ONLY TESTED ON LINUX PLATFORMS at
 * the moment. Does not work on macOS despite its POSIX compliance and
 * Unix likeness.
 *
 * Configuration:
 *
 * PRELOAD_Tablefs_path_prefix
 *   Path prefix for triggering preload.
 * PRELOAD_Tablefs_home
 *   DB home of tablefs. This is where tablefs stores namespace data.
 * PRELOAD_Tablefs_readonly
 *   Open tablefs as read only.
 * PRELOAD_Verbose
 *   Print more information.
 */
#include <tablefs/tablefs_api.h>

#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>

/*
 * Error reporting facilities...
 */
#define ABORT_FILENAME \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define ABORT(what, why) \
  msg_abort(why, what, __func__, ABORT_FILENAME, __LINE__)
static void msg_abort(const char* why, const char* what, const char* srcfcn,
                      const char* srcf, int srcln);

/*
 * next_functions: libc replacement functions (i.e., the default libc
 * implementation) we are providing to the preloader.
 */
static struct next_functions {
  int (*rmdir)(const char* path);
  int (*mkdir)(const char* path, mode_t);
  int (*__xmknod)(int ver, const char* path, mode_t, dev_t*);
  int (*__xstat)(int ver, const char* path, struct stat* buf);
  int (*__lxstat)(int ver, const char* path, struct stat* buf);
  DIR* (*opendir)(const char* path);
  struct dirent* (*readdir)(DIR* dirp);
  int (*closedir)(DIR* dirp);
  int (*access)(const char* path, int mode);
  int (*unlink)(const char* path);
} nxt = {0};

/*
 * init is done as a two-step process: in the first step, we init only the
 * preload ctx; in the second step, we open tablefs.
 */
static pthread_once_t preload_once = PTHREAD_ONCE_INIT;
static pthread_once_t tablefs_once = PTHREAD_ONCE_INIT;
static void preload_init();
static void tablefs_init();

/*
 * finalize fs.
 */
static void closefs();

/*
 * helper functions...
 */

/*
 * must_getnextdlsym: get next symbol or die.
 */
static void must_getnextdlsym(void** result, const char* symbol) {
  *result = dlsym(RTLD_NEXT, symbol);
  if (!(*result)) {
    ABORT(symbol, dlerror());
  }
}

/*
 * is_envset: return 1 if key is set.
 */
static int is_envset(const char* key) {
  const char* v = getenv(key);
  if (!v || !v[0]) {
    return 0;
  } else if (strcmp(v, "0") == 0) {
    return 0;
  } else {
    return 1;
  }
}

/*
 * end helpers
 */

/*
 * we assume that different threads do not share DIR* with each other and each
 * thread only opens one directory at a time. This allows us to use a simple
 * thread local storage to trace the directory currently opened by each
 * individual thread.
 */
static thread_local void* currdir = nullptr;

static struct preload_ctx {
  size_t path_prefixlen; /* strlen(path_prefix) */
  const char* path_prefix;
  const char* fsloc;
  tablefs_t* fs;
  int rdonly;
  int v;
} ctx = {0};

/*
 * PRELOAD_Init: init preload lib or die.
 */
static void PRELOAD_Init() {
  int rv = pthread_once(&preload_once, preload_init);
  if (rv != 0) {
    ABORT("pthread_once", strerror(rv));
  }
}

/*
 * TABLEFS_Init: init tablefs.
 */
static void TABLEFS_Init() {
  int rv = pthread_once(&tablefs_once, tablefs_init);
  if (rv != 0) {
    ABORT("pthread_once", strerror(rv));
  }
}

/*
 * preload_init: called via init_once. if this fails we are sunk, so
 * we'll abort the process....
 */
static void preload_init() {
#define MUST_GETNEXTDLSYM(x) must_getnextdlsym((void**)(&nxt.x), #x)
  MUST_GETNEXTDLSYM(access);
  MUST_GETNEXTDLSYM(unlink);
  MUST_GETNEXTDLSYM(__xmknod);
  MUST_GETNEXTDLSYM(__lxstat);
  MUST_GETNEXTDLSYM(__xstat);
  MUST_GETNEXTDLSYM(opendir);
  MUST_GETNEXTDLSYM(readdir);
  MUST_GETNEXTDLSYM(closedir);
  MUST_GETNEXTDLSYM(rmdir);
  MUST_GETNEXTDLSYM(mkdir);

#undef MUST_GETNEXTDLSYM
  ctx.v = is_envset("PRELOAD_Verbose");
  ctx.rdonly = is_envset("PRELOAD_Tablefs_readonly");
  ctx.fsloc = getenv("PRELOAD_Tablefs_home");
  if (!ctx.fsloc || !ctx.fsloc[0]) {
    ctx.fsloc = "/tmp/tablefs";
  }
  ctx.path_prefix = getenv("PRELOAD_Tablefs_path_prefix");
  if (!ctx.path_prefix || !ctx.path_prefix[0]) {
    ctx.path_prefix = "/tablefs/";
  }
  ctx.path_prefixlen = strlen(ctx.path_prefix);
  if (ctx.path_prefixlen == 1) ABORT(ctx.path_prefix, "Too short");
  if (ctx.path_prefix[ctx.path_prefixlen - 1] != '/')
    ABORT(ctx.path_prefix, "Does not end with '/'");
  if (ctx.v) {
    printf("PRELOAD_Verbose=%d\n", ctx.v);
    printf("PRELOAD_Tablefs_readonly=%d\n", ctx.rdonly);
    printf("PRELOAD_Tablefs_path_prefix=%s\n", ctx.path_prefix);
    printf("PRELOAD_Tablefs_home=%s\n", ctx.fsloc);
  }

  ctx.fs = NULL; /* initialized by tablefs_init() */
}

static void tablefs_init() {
  assert(!ctx.fs);
  ctx.fs = tablefs_newfshdl();
  if (ctx.rdonly) tablefs_set_readonly(ctx.fs, 1);
  int r = tablefs_openfs(ctx.fs, ctx.fsloc);
  if (r == -1) {
    ABORT("tablefs_openfs", strerror(errno));
  } else {
    if (ctx.v) printf("== Fs opened!\n");
    atexit(closefs);
  }
}

static void closefs() {
  assert(ctx.fs);
  tablefs_closefs(ctx.fs);
  if (ctx.v) printf("== Fs closed!\n");
  printf("Bye\n");
}

/*
 * is_tablefs: return NULL if the input path is not in tablefs so one of the
 * functions in nxt should be called to handle it. Return the input path's
 * corresponding tablefs path otherwise.
 */
static const char* is_tablefs(const char* input) {
  size_t prefixlen_1 = ctx.path_prefixlen - 1;
  if (strncmp(input, ctx.path_prefix, prefixlen_1) != 0) {
    return NULL;
  } else if (input[prefixlen_1] && input[prefixlen_1] != '/') {
    return NULL;
  } else if (input[prefixlen_1]) {
    return input + prefixlen_1;
  } else {
    return "/";
  }
}

/*
 * here are the actual override functions from libc.
 */
extern "C" {

int rmdir(const char* path) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    return tablefs_rmdir(ctx.fs, newpath);
  }

  return nxt.rmdir(path);
}

int mkdir(const char* path, mode_t mode) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    return tablefs_mkdir(ctx.fs, newpath, mode);
  }

  return nxt.mkdir(path, mode);
}

int __xmknod(int ver, const char* path, mode_t mode, dev_t* dev) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    return tablefs_mkfile(ctx.fs, newpath, mode);
  }

  return nxt.__xmknod(ver, path, mode, dev);
}

int __xstat(int ver, const char* path, struct stat* buf) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    return tablefs_lstat(ctx.fs, newpath, buf);
  }

  return nxt.__xstat(ver, path, buf);
}

/*
 * lstat is bound to __lxstat in libc on Linux... We don't know if there is a
 * solution that works for all platforms.
 */
int __lxstat(int ver, const char* path, struct stat* buf) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    return tablefs_lstat(ctx.fs, newpath, buf);
  }

  return nxt.__lxstat(ver, path, buf);
}

DIR* opendir(const char* path) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    if (currdir) {
      ABORT("opendir", "Too many open dirs");
    }
    DIR* dirp = reinterpret_cast<DIR*>(tablefs_opendir(ctx.fs, newpath));
    currdir = dirp;
    return dirp;
  }

  return nxt.opendir(path);
}

struct dirent* readdir(DIR* dirp) {
  PRELOAD_Init();
  if (currdir == dirp)
    return tablefs_readdir(reinterpret_cast<tablefs_dir_t*>(dirp));

  return nxt.readdir(dirp);
}

int closedir(DIR* dirp) {
  PRELOAD_Init();
  if (currdir == dirp) {
    int rv = tablefs_closedir(reinterpret_cast<tablefs_dir_t*>(dirp));
    currdir = nullptr;
    return rv;
  }

  return nxt.closedir(dirp);
}

int access(const char* path, int mode) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    struct stat buf;
    TABLEFS_Init();
    return tablefs_lstat(ctx.fs, newpath, &buf);
  }

  return nxt.access(path, mode);
}

int unlink(const char* path) {
  PRELOAD_Init();
  const char* newpath = is_tablefs(path);
  if (newpath) {
    TABLEFS_Init();
    return tablefs_unlink(ctx.fs, newpath);
  }

  return nxt.unlink(path);
}

int statvfs(const char* path, struct statvfs* buf) {
  memset(buf, 0, sizeof(struct statvfs));
  return 0;
}

} /* extern "C" */

/*
 * abort with what, why, and where
 */
static void msg_abort(const char* why, const char* what, const char* srcfcn,
                      const char* srcf, int srcln) {
  fputs("*** ABORT *** ", stderr);
  fprintf(stderr, "@@ %s:%d @@ %s] ", srcf, srcln, srcfcn);
  fputs(what, stderr);
  if (why) fprintf(stderr, ": %s", why);
  fputc('\n', stderr);
  abort();
}
