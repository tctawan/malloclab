#!/usr/bin/env bash

make clean\
&& make\
&&
gdb --args ./mdriver -f short1-bal.rep -V
