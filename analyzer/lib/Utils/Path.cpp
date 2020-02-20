// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//
// A set of file path related functions

#include "Utils/Path.h"
#include "Utils/String.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __set_errno(e) (errno = (e))
#define MAX_PATH 512
#define ISSLASH(C) ((C) == '/')

static char PBUF[MAX_PATH];

/*
 * Strip N components from a file path
 */
const char *stripname(const char *name, int strips) {
  if (strips == 0) return name;
  const char *str = name;
  const char *p = name;
  int left = strips;
  while (*str) {
    if (ISSLASH(*str)) {
      while (ISSLASH(str[1]))  // care, e,g, a//b/c//
        str++;
      if (strips < 0 || --left >= 0) {
        p = str + 1;
      }
      if (left == 0) break;
    }
    str++;
  }
  return p;
}

/* Return the canonical absolute name of file NAME.  A canonical name
   does not contain any `.', `..' components nor any repeated path
   separators ('/').  
   Differences from the libc's realpath: 
   1). do not expand symlink
   2). do not add pwd to the name
   3). do not check for existence of each component

   The following implementation is modified from glibc 2.9(canonicalize.c)
*/
char *canonpath(const char *name, char *resolved) {
  char *rpath, *dest;
  const char *start, *end, *rpath_limit;
  long int path_max;

  if (name == NULL)
  {
    /* As per Single Unix Specification V2 we must return an error if
       either parameter is a null pointer.  We extend this to allow
       the RESOLVED parameter to be NULL in case the we are expected to
       allocate the room for the return value.  */
    __set_errno(EINVAL);
    return NULL;
  }

  if (name[0] == '\0')
  {
    /* As per Single Unix Specification V2 we must return an error if
       the name argument points to an empty string.  */
    __set_errno (ENOENT);
    return NULL;
  }

  path_max = MAX_PATH;

  if (resolved == NULL)
  {
    rpath = (char *) xmalloc (path_max);
  }
  else
    rpath = resolved;
  rpath_limit = rpath + path_max;

  if (name[0] == '/') {
    rpath[0] = '/';
    dest = rpath + 1;
  }
  else {
    // here the `correct' implementation should be the 
    // following commented one
    // but we especially are interested when name is not
    // rooted at `/'
    // This is dangerous if name is illegal such as '../gcc/xxx'
    // For now we just check the simple error. 
    // TODO
    /*
       if (!getcwd(rpath, path_max)) {
       rpath[0] = '\0';
       goto error;
       }
       dest = strchr(rpath, '\0');
       */
    if (name[0] == '.' && name[1] == '.')
      goto error;
    dest = rpath;
  }

  for (start = end = name; *start; start = end)
  {
    /* Skip sequence of multiple path-separators.  */
    while (*start == '/')
      ++start;
    /* Find end of path component.  */
    for (end = start; *end && *end != '/'; ++end)
      /* Nothing.  */;
    if (end - start == 0)
      break;
    else if (end - start == 1 && start[0] == '.')
      /* nothing */;
    else if (end - start == 2 && start[0] == '.' && start[1] == '.')
    {
      /* Back up to previous component, ignore if at root already.  */
      while ((dest > rpath + 1) && (--dest)[-1] != '/');
      if (dest[-1] != '/')
        --dest; //move to the beginning
    }
    else
    {
      size_t new_size;
      if (dest > rpath && dest[-1] != '/')
        *dest++ = '/';
      if (dest + (end - start) >= rpath_limit)
      {
        off_t dest_offset = dest - rpath;
        char *new_rpath;
        if (resolved)
        {
          __set_errno (ENAMETOOLONG);
          if (dest > rpath + 1)
            dest--;
          *dest = '\0';
          goto error;
        }
        new_size = rpath_limit - rpath;
        if (end - start + 1 > path_max)
          new_size += end - start + 1;
        else
          new_size += path_max;
        new_rpath = (char *) realloc (rpath, new_size);
        if (new_rpath == NULL)
          goto error;
        rpath = new_rpath;
        rpath_limit = rpath + new_size;
        dest = rpath + dest_offset;
      }

      mempcpy (dest, start, end - start);
      dest += (end - start);
      *dest = '\0';
    }
  }
  if (dest > rpath + 1 && dest[-1] == '/')
    --dest;
  *dest = '\0';

  assert (resolved == NULL || resolved == rpath);
  return rpath;

error:
  assert (resolved == NULL || resolved == rpath);
  if (resolved == NULL)
    free (rpath);
  fprintf(stderr, "fatal: %s\n", name);
  return NULL;
}

/*
 * Check if path1 ends with path2
 */
bool pathendswith(const char *path1, const char *path2) {
  if (canonpath(path1, PBUF)) {
    int len1 = strlen(PBUF);
    int len2 = strlen(path2);
    if (len1 < len2)
      return false;
    int i = len1 - len2;
    int j = 0;
    if (PBUF[0] == '/' && i > 0 && path2[j] != '/' && PBUF[i - 1] != '/') {
      // A: /tmp/atest.c
      // B: test.c
      return false;
    }
    while (i < len1 && j < len2 && PBUF[i] == path2[j]) {
      i++;
      j++;
    }
    if (i != len1) {
      return false;
    }
    return true;
  }
  else
    return false;
}

/*
 * If two paths are equal for up to n components
 */
bool pathneq(const char *path1, const char *path2, int n) {
  if (canonpath(path1, PBUF)) {
    return strcmp(stripname(PBUF, n), path2) == 0;
  }
  else
    return false;
}

