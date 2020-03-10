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
    {"rxcmd", required_argument, 0, 'r'},
    {"guid-map", required_argument, 0, 'g'},
    {"addresses", required_argument, 0, 'a'},
    {"file-lines", required_argument, 0, 'z'},
    {"inst-str", required_argument, 0, 'i'},
    {"func-name", required_argument, 0, 'f'},
    {"inst-no", required_argument, 0, 's'},
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
      "  -r, --rxcmd <command>        : command string to re-execute the\n"
      "                                 target program with reverted context\n"
      "  -g, --guid-map <file>        : path to the static GUID map file\n"
      "  -a, --addresses <file>       : path to the dynamic address trace file\n"
      "  -z  --file-lines <lines>     : comma separated list of slicing criterion\n"
      "  -i  --inst-str <inst-string> : instruction to start slicing\n"
      "  -f  --func-name <func>       : func name \n"
      "  -s  --inst-no <inst>         : Nth instruction in a function to start slicing "
      "\n\n",
      program);
}

bool parse_options(int argc, char *argv[], reactor_options &options) {
  // reset the options
  memset(&options, 0, sizeof(options));
  int option_index = 0;
  int c;
  char *pend;
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
        options.version_num = strtol(optarg, &pend, 10);
        if (pend == optarg || *pend != '\0') {
          fprintf(stderr, "version number must be an integer\n");
          return false;
        }
        break;
      case 'g':
        options.hook_guid_file = optarg;
        break;
      case 'a':
        options.address_file = optarg;
        break;
      case 'r':
        options.reexecute_cmd = optarg;
        break;
      case 'z':
        options.file_lines = optarg;
        break;
      case 'i':
        options.inst = optarg;
        break;
      case 'f':
        options.func = optarg;
        break;
      case 's':
        options.inst_no = strtol(optarg, &pend, 10);
        if (pend == optarg || *pend != '\0') {
          fprintf(stderr, "version number must be an integer\n");
          return false;
        }
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
  return check_options(options);
}

bool check_options(reactor_options &options) {
  if (!options.pmem_file) {
    fprintf(stderr,
            "pmem file option is not set, specify it with -p or --pmem-file\n");
    return false;
  }
  if (access((options.pmem_file), 0) != 0) {
    fprintf(stderr, "pmem file %s does not exist\n", options.pmem_file);
    return false;
  }
  if (!options.pmem_layout) {
    fprintf(
        stderr,
        "pmem file layout is not set, specify it with -t or --pmem-layout\n");
    return false;
  }
  if (!options.pmem_library) {
    fprintf(stderr,
            "pmem library is not set, specify it with -l or --pmem-lib\n");
    return false;
  }
  if (!(strcmp(options.pmem_library, "libpmem") == 0 ||
        strcmp(options.pmem_library, "libpmemobj") == 0)) {
    fprintf(stderr, "Unrecognized pmem library %s\n", options.pmem_library);
    return false;
  }
  if (!options.hook_guid_file) {
    fprintf(stderr,
            "pmem library is not set, specify it with -g or --guid-map\n");
    return false;
  }
  if (options.version_num <= 0) {
    fprintf(stderr, "version number must be positive\n");
    return false;
  }
  if (!options.reexecute_cmd) {
    fprintf(stderr,
            "re-execution command is not set, specify it with -r or --rxcmd\n");
    return false;
  }
  if (options.file_lines.empty()) {
    fprintf(stderr,
            "file_lines not set, specify it with -z or --file-lines\n");
    return false;
  }
  if (options.inst.empty()) {
    fprintf(stderr,
            "instruction string is not set, specify it with -i or --inst-str\n");
    return false;
  }
  if (options.func.empty()) {
    fprintf(stderr,
            "function name is not set, specify it with -f or --func-name\n");
    return false;
  }
  if (!options.inst_no){
    fprintf(stderr,
            "instruction num is not set, specify it with -s or --inst-no\n");
    return false;
  }

  return true;
}
