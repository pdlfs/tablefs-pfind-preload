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
 * fsmaker.cc - populate tablefs with a very simple namespace
 *   for development and testing purposes.
 */

#include <tablefs/tablefs_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * helper/utility functions, included inline here so we are self-contained
 * in one single source file...
 */
static char *argv0; /* argv[0], program name */
static char *argv1; /* argv[1], tablefs home */

/*
 * Error reporting facilities...
 */
#define ABORT_FILENAME \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define ABORT(what, why) \
  msg_abort(why, what, __func__, ABORT_FILENAME, __LINE__)
static void msg_abort(const char *why, const char *what, const char *srcfcn,
                      const char *srcf, int srcln);

/*
 * usage
 */
static void usage(const char *msg) {
  if (msg) fprintf(stderr, "%s: %s\n", argv0, msg);
  fprintf(stderr, "usage: %s <tablefs_db_home>\n", argv0);
  exit(EXIT_FAILURE);
}

static void mkfs(const char *fsloc) {
  tablefs_t *fs = tablefs_newfshdl();
  if (!fs) ABORT("Cannot create fs handle", strerror(errno));
  int r = tablefs_openfs(fs, fsloc);
  if (r == -1) {
    ABORT("Cannot open fs", strerror(errno));
  }

  tablefs_mkdir(fs, "/1", 0755);
  tablefs_mkdir(fs, "/2", 0755);
  tablefs_mkdir(fs, "/3", 0755);

  tablefs_mkfile(fs, "/1/a", 0644);
  tablefs_mkfile(fs, "/1/b", 0644);
  tablefs_mkfile(fs, "/1/c", 0644);
  tablefs_mkfile(fs, "/2/a", 0644);
  tablefs_mkfile(fs, "/2/b", 0644);
  tablefs_mkfile(fs, "/2/c", 0644);
  tablefs_mkfile(fs, "/3/a", 0644);
  tablefs_mkfile(fs, "/3/b", 0644);
  tablefs_mkfile(fs, "/3/c", 0644);

  tablefs_closefs(fs);
}

/*
 * main program.
 */
int main(int argc, char *argv[]) {
  argv0 = argv[0];

  /* we want lines, even if we are writing to a pipe */
  setlinebuf(stdout);
  if (argc < 2) {
    usage("missing tablefs db home");
  }

  argv1 = argv[1];
  mkfs(argv1);
  puts("Done!");
  return 0;
}

/*
 * abort with what, why, and where
 */
static void msg_abort(const char *why, const char *what, const char *srcfcn,
                      const char *srcf, int srcln) {
  fputs("*** ABORT *** ", stderr);
  fprintf(stderr, "@@ %s:%d @@ %s] ", srcf, srcln, srcfcn);
  fputs(what, stderr);
  if (why) fprintf(stderr, ": %s", why);
  fputc('\n', stderr);
  abort();
}
