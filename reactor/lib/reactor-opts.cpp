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
#include <algorithm>
#include <iostream>

#include "reactor-opts.h"

// The short option specifiers should be consistent with the long-options
// declaration below. It can optionally include additional short option
// specifiers that do not have a corresponding long-option. ':'
// after the character means this opt requires an argument.
#define REACTOR_ARGS "hp:t:l:n:r:g:a:i:c:b:z:"

// Reference:
// https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Options.html

const char *program = "reactor";  // by default it's reactor
static int show_help = 0;
static int skip_check = 0;

static int enable_pta = 1;  // by default PTA is enabled
static int enable_control = 0;
// see comments in reactor-opts.h
// for the difference between enable_control
// and slice_control
static int slice_control = 0;
static int support_thread = 0;
static int intra_procedural = 0;
static int inter_procedural = 1;  // by default inter-procedural
static int entry_only = 0;

static struct option long_options[] = {
    /* These options set a flag. */
    {"help", no_argument, &show_help, 1},
    {"skip-check", no_argument, &skip_check, 1},
    {"pta", no_argument, &enable_pta, 1},
    {"no-pta", no_argument, &enable_pta, 0},
    {"ctrl", no_argument, &enable_control, 1},
    {"no-ctrl", no_argument, &enable_control, 0},
    {"slice-ctrl", no_argument, &slice_control, 1},
    {"thd", no_argument, &support_thread, 1},
    {"no-thd", no_argument, &support_thread, 0},
    {"intra", no_argument, &intra_procedural, 1},
    {"inter", no_argument, &inter_procedural, 1},
    {"entry-only", no_argument, &entry_only, 1},
    /* These options don't set a flag.
       We distinguish them by their indices. */
    {"pmem-file", required_argument, 0, 'p'},
    {"pmem-layout", required_argument, 0, 't'},
    {"pmem-lib", required_argument, 0, 'l'},
    {"ver", required_argument, 0, 'n'},
    {"rxcmd", required_argument, 0, 'r'},
    {"guid-map", required_argument, 0, 'g'},
    {"addresses", required_argument, 0, 'a'},
    {"fault-inst", required_argument, 0, 'i'},
    {"fault-loc", required_argument, 0, 'c'},
    {"bc-file", required_argument, 0, 'b'},
    {"batch-threshold", required_argument, 0, 'e'},
    {"arckpt", required_argument, 0, 'z'},
    {0, 0, 0, 0}};

void usage() {
  fprintf(
      stderr,
      "Usage: %s [-h] [OPTION] \n\n"
      "Options:\n"
      "  -h, --help                   : show this help\n"
      "      --skip-check             : do not validate options\n"
      "                                 useful for testing\n"
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
      "  -a, --addresses <file>       : path to the dynamic address trace "
      "file\n"
      "  -i  --fault-inst <string>    : the fault instruction\n"
      "  -c  --fault-loc  <file:line\n"
      "                   [:func]>    : location of the fault instruction \n"
      "  -b  --bc-file <file>         : bytecode file \n"
      "  -z  --arckpt                 : arckpt\n"
      "  -e  --batch-threshold        : number of items to batch in a reversion\n"
      "\nSlicer Options:\n"
      "      --pta                    : enable pointer analysis\n"
      "      --no-pta                 : disable pointer analysis\n"
      "      --ctrl                   : enable control dependencies\n"
      "      --no-ctrl                : disable control dependencies\n"
      "      --slice-ctrl             : include control dependencies\n"
      "                                 in the slice graph\n"
      "      --thd                    : analyze thread operations\n"
      "      --no-thd                 : do not analyze thread operations\n"
      "      --intra                  : intra-procedural analysis\n"
      "      --inter                  : inter-procedural analysis\n"
      "      --entry-only             : only analyze the function that a\n"
      "                                 fault instruction belongs to\n"
      "\n\n",
      program);
}

bool parse_options(int argc, char *argv[], reactor_options_t &options) {
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
      case 'z':
        options.arckpt = optarg;
        break;
      case 'e':
        options.batch_threshold = strtol(optarg, &pend, 10);
        break;
      case 'a':
        options.address_file = optarg;
        break;
      case 'r':
        options.reexecute_cmd = optarg;
        break;
      case 'c':
        options.fault_loc = optarg;
        break;
      case 'i':
        options.fault_instr = optarg;
        break;
      case 'b':
        options.bc_file = optarg;
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
  options.dg_options.entry_only = entry_only != 0;
  options.dg_options.enable_pta = enable_pta != 0;
  options.dg_options.enable_ctrl = enable_control != 0;
  options.dg_options.slice_ctrl = slice_control != 0;
  options.dg_options.support_thread = support_thread != 0;
  options.dg_options.inter_procedural = inter_procedural != 0;
  options.dg_options.intra_procedural = intra_procedural != 0;
  // defaults
  if (!options.pmem_library) options.pmem_library = "libpmem";
  if (skip_check != 0) return true;
  // only check options if skip_check is not specified
  return check_options(options);
}

bool check_options(reactor_options_t &options) {
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
        strcmp(options.pmem_library, "libpmemobj") == 0 || 
        strcmp(options.pmem_library, "mmap") == 0 )) {
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
  if (!options.fault_loc.empty()) {
    size_t n =
        std::count(options.fault_loc.begin(), options.fault_loc.end(), ':');
    if (n < 1 || n > 2) {
      fprintf(stderr,
              "invalid location specifier %s, it must be <file:line[:func]> "
              "format\n",
              options.fault_loc.c_str());
      return false;
    }
  }
  if (options.bc_file.empty()) {
    fprintf(stderr,
            "bitcode file is not set, specify it with -b or --bc-file\n");
    return false;
  }
  return true;
}
