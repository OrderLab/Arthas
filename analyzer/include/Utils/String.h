// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _UTILS_STRING_H_
#define _UTILS_STRING_H_

#include <stddef.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

void *xmalloc(size_t size);

// split the string by a single-char delimeter
int splitList(const std::string &str, char sep, std::vector<std::string> &ret);

// split the string by a string delimeter
int splitList(const std::string& str, const char* sep,
              std::vector<std::string>& result);

bool splitUntilN(const std::string& str, const char* delimeters, int n,
                 std::vector<std::string>& result, size_t* last_pos);

template <typename T>
T str2fmt(const std::string& s, bool hex = false) {
  std::stringstream iss;
  if (hex) {
    iss << std::hex << s;
  } else {
    iss << s;
  }
  T result;
  iss >> result;
  if (iss.fail()) {
    throw std::invalid_argument("invalid format");
  }
  return result;
}

inline std::string& ltrim(std::string& str, const char* chars) {
  str.erase(0, str.find_first_not_of(chars));
  return str;
}

inline std::string& ltrim(std::string& str) {
  str.erase(str.begin(), std::find_if(str.begin(), str.end(),
        std::not1(std::ptr_fun<int, int>(std::isspace))));
  return str;
}

inline std::string& rtrim(std::string& str) {
  str.erase(std::find_if(str.rbegin(), str.rend(),
        std::not1(std::ptr_fun<int, int>(std::isspace))).base(), str.end());
  return str;
}

inline std::string& trim(std::string& str) {
  return ltrim(rtrim(str));
}

#endif /* _UTILS_STRING_H_ */
