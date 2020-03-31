#!/bin/bash
perl -i -ne 'print if ! $x{$_}++' $1
