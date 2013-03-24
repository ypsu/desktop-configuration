#!/bin/bash

cd /home/rlblaster
rm -rf $(readlink .mozilla)
mkdir -p $(readlink .mozilla)
tar xf .mozilla.tar
