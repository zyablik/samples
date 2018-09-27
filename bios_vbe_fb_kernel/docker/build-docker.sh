#!/bin/bash

set -x
docker build --build-arg GID=`id -g` --build-arg UID=`id -u` -t mini_os $@ .
