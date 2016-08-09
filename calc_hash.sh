#!/bin/sh
git status > /dev/null 2>&1 && git rev-parse HEAD || echo NO_GIT_COMMIT_HASH_DEFINED
