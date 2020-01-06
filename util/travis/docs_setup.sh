#!/bin/sh
set -ev

sudo apt-get update -qq
sudo apt-get install --allow-unauthenticated -y -qq libx11-dev mesa-common-dev libgl1-mesa-dev qtbase5-dev libxcb-keysyms1-dev gdb g++-6 python3-pip

sudo pip3 install Sphinx sphinx-rtd-theme
