#!/bin/bash

BUILD=$PWD/build
mkdir -p $BUILD && cd $BUILD

export LANG=C

export SDK_PREFIX="/opt/KasperskyOS-SDK-Mobile-1.0.0.13"
export PATH="$SDK_PREFIX/toolchain/bin:$PATH"

cmake -G "Unix Makefiles" \
      -D CMAKE_BUILD_TYPE:STRING=Debug \
      -D CMAKE_TOOLCHAIN_FILE=$SDK_PREFIX/toolchain/share/toolchain-x86_64-pc-kos.cmake \
      ../ && make kos-image  #&& sudo qemu-system-x86_64 -cpu host -serial stdio -m 256   -device vfio-pci,host=00:02.0  --enable-kvm -kernel ./einit/kos-qemu-image
