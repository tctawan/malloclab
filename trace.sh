#!/usr/bin/env bash

make clean\
&& make\
&&
gdb --args ./mdriver -f short2-bal.rep -V
