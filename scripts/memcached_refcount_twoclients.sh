#!/bin/bash
((echo -e "get dd" | nc 127.0.0.1 11211) | grep 'END' 1>&2;  )3>&1 & X=$!
(echo -e "get dd" | nc 127.0.0.1 11211) 
Y=$!
wait $X
retn_code=$?
echo 'Return val is ' 
echo $retn_code
echo $X
wait
exit $retn_code
