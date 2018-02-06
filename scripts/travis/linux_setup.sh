#!/bin/sh
set -ev

sudo add-apt-repository -y 'ppa:ubuntu-toolchain-r/test'
sudo add-apt-repository -y 'ppa:beineri/opt-qt562-trusty'
sudo add-apt-repository -y 'deb http://apt.llvm.org/precise/ llvm-toolchain-precise-3.8 main'
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get update -qq 
sudo apt-get install --allow-unauthenticated -y -qq libx11-dev mesa-common-dev libgl1-mesa-dev qt56base qt56svg qt56x11extras libxcb-keysyms1-dev gdb clang-format-3.8 g++-6

# check formatting matches clang-format-3.8. Since newer versions can have
# changes in formatting even without any rule changes, we have to fix on a
# single version.

clang-format-3.8 -i -style=file $(find qrenderdoc/ renderdoc/ renderdoccmd/ renderdocshim/ -type f -regex '.*\(/3rdparty/\|/official/\|resource.h\).*' -prune -o -regex '.*\.\(c\|cpp\|h\)$' -print)

git clean -f

# this swallows the exit code so we won't fail until we print the diff in
# the next line, but allows us to print a friendlier message to anyone
# looking at the build log
git diff --quiet || (
  echo "***************************************************";
  echo "*** The code is not clean against clang-format  ***";
  echo "*** Please run clang-format-3.8 and fix the     ***";
  echo "*** differences then rebase/squash them into    ***";
  echo "*** the relevant commits. Do not add a commit   ***";
  echo "*** for just formatting fixes. Thanks!          ***";
  echo "***************************************************";
  )

git diff --exit-code
