#!/bin/bash

# https://habr.com/company/neobit/blog/176707/

make clean
sudo losetup --detach-all

make

qemu-system-x86_64 -hda ./hdd.img
