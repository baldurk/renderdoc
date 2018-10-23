#!/bin/sh

brew update
brew install qt5
brew link qt5 --force
brew upgrade python

echo "Setup complete"
