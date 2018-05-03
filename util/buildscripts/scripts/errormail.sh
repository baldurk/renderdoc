#!/bin/bash

EMAIL_BODY_FILE=$1

PLATFORM=Windows

if [[ "$ERRORMAIL" == "" ]]; then
	exit 0;
fi

if [ "$(uname)" == "Linux" ]; then
	PLATFORM=Linux
fi

SUBJECT="[renderdoc] Compile error on $PLATFORM"

EMAILHOST="${BUILD_ROOT}"/support/emailhost

if [ -f "${EMAILHOST}" ]; then
	ssh $(cat "${EMAILHOST}") "mail -s \"$SUBJECT\" $ERRORMAIL" < $EMAIL_BODY_FILE
	exit;
else
	mail -s \"$SUBJECT\" $ERRORMAIL < $EMAIL_BODY_FILE
fi
