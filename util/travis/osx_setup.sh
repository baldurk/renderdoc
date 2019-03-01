#!/bin/sh

brew update
brew install qt5 lftp
brew link qt5 --force
brew upgrade python

echo '|1|DQR5DTWgBz2JwdQs1G6KpjppcIE=|oxLDo2zhfkFZ+/gsTcnXI/vC8qs= ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBObosfTSrCa11pDrmPxJ6zzNltDJls3Vc0AMVrqX0hAGFoFWbGvdDm3wpDBYHpkL9LmG6bJNHqWmO59oUJZYl9E=' >> $HOME/.ssh/known_hosts

echo "Setup complete"
