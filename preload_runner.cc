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
 * preload_runner.cc - a simple test program for executing a set of
 *     lstat and readdir ops against a given directory.
 */

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * helper/utility functions, included inline here so we are self-contained
 * in one single source file...
 */
static char* argv0; /* argv[0], program name */

/*
 * default values
 */
#define DEF_TIMEOUT 120 /* alarm timeout */

/*
 * gs: shared global data (e.g. from the command line)
 */
static struct gs { int timeout; /* alarm timeout */ } g;

/*
 * alarm signal handler
 */
static void sigalarm(int foo) {
  fprintf(stderr, "SIGALRM detected\n");
  fprintf(stderr, "Alarm clock\n");
  exit(EXIT_FAILURE);
}

/*
 * usage
 */
static void usage(const char* msg) {
  if (msg) fprintf(stderr, "%s: %s\n", argv0, msg);
  fprintf(stderr, "usage: %s [opts] path_to_dir\n", argv0);
  fprintf(stderr, "\nopts:\n");
  fprintf(stderr, "\t-t sec      timeout (alarm), in seconds\n");

  exit(EXIT_FAILURE);
}

static uint64_t timeval_to_micros(const struct timeval* tv) {
  uint64_t t;
  t = static_cast<uint64_t>(tv->tv_sec) * 1000000;
  t += tv->tv_usec;
  return t;
}

/*
 * forward prototype decls.
 */
static void listdir(const char* dirpath);

/*
 * main program.
 */
int main(int argc, char* argv[]) {
  int ch;
  argv0 = argv[0];

  /* we want lines, even if we are writing to a pipe */
  setlinebuf(stdout);

  /* setup default to zero/null */
  memset(&g, 0, sizeof(g));
  g.timeout = DEF_TIMEOUT;

  while ((ch = getopt(argc, argv, "t:")) != -1) {
    switch (ch) {
      case 't':
        g.timeout = atoi(optarg);
        if (g.timeout < 0) usage("bad timeout");
        break;
      default:
        usage(NULL);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 1) {
    usage("missing dir path");
  }

  signal(SIGALRM, sigalarm);
  alarm(g.timeout);

  printf("Listing dir %s (timeout=%d)\n", argv[0], g.timeout);
  listdir(argv[0]);
  printf("Done!\n");

  return 0;
}

static void listdir(const char* dirpath) {
  DIR* dir = opendir(dirpath);
  if (!dir) {
    fprintf(stderr, "cannot open dir %s: %s\n", dirpath, strerror(errno));
    return;
  }
  const size_t pathlen = strlen(dirpath);
  char* pathbuf = (char*)malloc(PATH_MAX + pathlen + 1);
  strcpy(pathbuf, dirpath);
  struct dirent* ent;
  struct stat stat;
  int r;
  while ((ent = readdir(dir))) {
    printf("%c] %s\n", S_ISDIR(DTTOIF(ent->d_type)) ? 'D' : 'F', ent->d_name);
    sprintf(pathbuf + pathlen + 1, "%s", ent->d_name);
    r = lstat(pathbuf, &stat);
    if (r == -1) {
      fprintf(stderr, "cannot stat %s: %s\n", pathbuf, strerror(errno));
    } else {
      // OK!
    }
  }
  free(pathbuf);
  closedir(dir);
}
