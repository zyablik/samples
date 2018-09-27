#!/bin/bash
docker run --net=host --pid=host --workdir=/home/user/mini_os --privileged \
           -v `realpath ..`:/home/user/mini_os \
           -v $HOME/.bash_history:/home/user/.bash_history \
           -e DISPLAY=$DISPLAY -e QT_X11_NO_MITSHM=1 -v /tmp/.X11-unix:/tmp/.X11-unix \
           -e DOCKER_IMAGE=mini_os \
           $@ \
           -i -t mini_os /bin/bash \
