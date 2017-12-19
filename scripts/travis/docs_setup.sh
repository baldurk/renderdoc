#!/bin/sh
set -ev

sudo add-apt-repository -y 'ppa:ubuntu-toolchain-r/test'
sudo add-apt-repository -y 'ppa:beineri/opt-qt562-trusty'
sudo add-apt-repository -y 'deb http://apt.llvm.org/precise/ llvm-toolchain-precise-3.8 main'
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get update -qq
sudo apt-get install --allow-unauthenticated -y -qq libx11-dev mesa-common-dev libgl1-mesa-dev qt56base libxcb-keysyms1-dev gdb g++-6 python3-pip

sudo pip3 install --upgrade pip setuptools
sudo pip3 install Sphinx sphinx-rtd-theme
