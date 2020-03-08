// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "reactor-opts.h"

// The short option specifiers should be consistent with the long-options
// declaration below. It can optionally include additional short option
// specifiers that do not have a corresponding long-option. ':'
// after the character means this opt requires an argument.
#define REACTOR_ARGS "hp:t:l:n:g:a:"

// Reference:
// https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Options.html

const char *program = "reactor";  // by default it's reactor
static int show_help = 0;

static struct option long_options[] = {
    /* These options set a flag. */
    {"help", no_argument, &show_help, 1},
    /* These options don't set a flag.
       We distinguish them by their indices. */
    {"pmem-file", required_argument, 0, 'p'},
    {"pmem-layout", required_argument, 0, 't'},
    {"pmem-lib", required_argument, 0, 'l'},
    {"ver", required_argument, 0, 'n'},
    {"guid-map", required_argument, 0, 'g'},
    {"addresses", required_argument, 0, 'a'},
    {0, 0, 0, 0}};

void usage() {
  fprintf(
      stderr,
      "Usage: %s [-h] [OPTION] \n\n"
      "Options:\n"
      "  -h, --help                   : show this help\n"
      "  -p, --pmem-file <file>       : path to the target system's "
      "persistent memory file\n"
      "  -t, --pmem-layout <layout>   : the PM file's layout name\n"
      "  -l, --pmem-lib <library>     : the PMDK library: libpmem, libpmemobj\n"
      "  -n, --ver <number>           : the version number to revert for the "
      "1st \n"
      "                                 coarse-grained reversion attempt\n"
      "  -g, --guid-map <file>        : path to the static GUID map file\n"
      "  -a, --addresses <file>       : path to the dynamic address trace "
      "file\n\n",
      program);
}

bool parse_options(int argc, char *argv[], reactor_options &options) {
  // reset the options
  memset(&options, 0, sizeof(options));
  int option_index = 0;
  int c;
  while ((c = getopt_long(argc, argv, REACTOR_ARGS, long_options,
                          &option_index)) != -1) {
    switch (c) {
      case 'h':
        show_help = 1;
        break;
      case 'p':
        options.pmem_file = optarg;
        break;
      case 't':
        options.pmem_layout = optarg;
        break;
      case 'l':
        options.pmem_library = optarg;
        break;
      case 'n':
        options.version_num = atoi(optarg);
        break;
      case 'g':
        options.hook_guidfile = optarg;
        break;
      case 'a':
        options.address_file = optarg;
        break;
      case 0:
        // getopt_long sets a flagkeep going
        break;
      case '?':
      default:
        return false;
    }
  }
  if (show_help) {
    usage();
    exit(0);
  }
  // TODO: may process some arguments here
  if (optind < argc) {
    printf("Non-option argument received: ");
    while (optind < argc) printf("%s ", argv[optind++]);
    putchar('\n');
  }
  return true;
}

