#!/usr/bin/env bash

make clean\
&& make\
&&
gdb --args ./mdriver -V 
