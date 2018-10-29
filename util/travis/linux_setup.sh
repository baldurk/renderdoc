#!/bin/sh
set -ev

sudo add-apt-repository -y 'ppa:ubuntu-toolchain-r/test'
sudo add-apt-repository -y 'ppa:beineri/opt-qt562-trusty'
sudo add-apt-repository -y 'deb http://apt.llvm.org/precise/ llvm-toolchain-precise-3.8 main'
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get update -qq 
sudo apt-get install --allow-unauthenticated -y -qq libx11-dev mesa-common-dev libgl1-mesa-dev qt56base qt56svg qt56x11extras libxcb-keysyms1-dev gdb clang-format-3.8 g++-6

# check last 100 commits are all correctly sized. First line must be no
# longer than 72 characters, so it fits in git log and github history
# We don't check the first commit since in pull requests this is an invisible 'merge' commit.
if git log --oneline | tail -n +2 | head -n 100 | cut -d ' ' -f2- | grep -q '.\{73\}'; then
  echo "***************************************************";
  echo "*** Some of your commit messages summaries are  ***";
  echo "*** longer than 72 characters.                  ***";
  echo "*** Please shorten them so they fit <= 72 chars ***";
  echo "*** on the first line, with a longer summary in ***";
  echo "*** the body after a blank line.                ***";
  echo "*** For more information see                    ***";
  echo "*** docs/CONTRIBUTING.md.                       ***";
  echo "*** Thanks!                                     ***";
  echo "***                                             ***";
  echo "*** Commit messages:                            ***";
  echo;
  git log --oneline | tail -n +2 | head -n 100 | cut -d ' ' -f2- | grep '.\{73\}'
  echo;
  echo "***************************************************";
  exit 1;
fi

# check formatting matches clang-format-3.8. Since newer versions can have
# changes in formatting even without any rule changes, we have to fix on a
# single version.
. ./util/clang_format_all.sh

git clean -f

# Print any diff here, so the error message below is the last thing
git diff

git diff --quiet || (
  echo "***************************************************";
  echo "*** The code is not clean against clang-format  ***";
  echo "*** Please run clang-format-3.8 and fix the     ***";
  echo "*** differences then rebase/squash them into    ***";
  echo "*** the relevant commits. Do not add a commit   ***";
  echo "*** for just formatting fixes. Thanks!          ***";
  echo "***************************************************";
  exit 1;
  )
