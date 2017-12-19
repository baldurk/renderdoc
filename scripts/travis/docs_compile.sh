#!/bin/sh
set -ev

cd docs/
make html SPHINXOPTS=-W
