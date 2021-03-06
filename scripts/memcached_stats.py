#!/usr/bin/env python

import sys
import memcache

memc =  memcache.Client(['127.0.0.1:11211'], socket_timeout=100)
stats = memc.get_stats()
if not stats:
    print("Did not get stats from Memcached server. It's probably not running.")
    sys.exit(1)

first_level = stats[0]
second_level = first_level[1]
print("total items is ")
print(second_level.get("total_items"))
