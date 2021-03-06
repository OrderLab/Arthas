#!/usr/bin/env python

import memcache

memc =  memcache.Client(['127.0.0.1:11211'], socket_timeout=100)

str1 = ""
str2 = ""

a = 1

while a < 10000000:
    str1 = "key" + str(a)
    str2 = "value" + str(a)
    memc.set(str1, str2)
    a += 1
