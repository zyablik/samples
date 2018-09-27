FROM ubuntu:16.04

ARG GID
ARG UID

RUN apt-get update
RUN apt-get -y install sudo make gcc bc dosfstools grub qemu

RUN groupadd -g $GID user && useradd -m -G sudo -d /home/user -g $GID -u $UID user
RUN chown -R user:user /home/user
RUN sed -i 's/sudo.*ALL=(ALL:ALL) ALL/sudo ALL=(ALL:ALL) NOPASSWD:ALL/g' /etc/sudoers
# keep user PATH when sudoing
RUN sed -i "s/^Defaults.*secure_path/#Defaults        secure_path/g" /etc/sudoers


USER user
ENV USER user
RUN echo "shopt -s histappend" >> ~/.bashrc 

ENV HISTSIZE 100000
ENV HISTFILESIZE 100000
ENV HISTCONTROL ignoredups
ENV PROMPT_COMMAND 'history -a'

RUN echo "\
export PS1='\u@\[\033[01;32m\]\${mini_os}\[\033[00m\]:\w\$ '\
" >> ~/.bashrc
