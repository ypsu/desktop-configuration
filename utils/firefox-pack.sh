#!/bin/bash

cd /home/rlblaster
tar cf .mozilla.tar.new .mozilla/*
mv .mozilla.tar .mozilla.tar.old
sync
mv .mozilla.tar.new .mozilla.tar
