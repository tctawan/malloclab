#!/usr/bin/env bash

make clean\
&& make\
&&
./mdriver -f short1-bal.rep -V
