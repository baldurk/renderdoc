#!/bin/bash
# Script to sign a file using the key.pfx certificate
if [ $# -ne 1 ] ; then
   echo Usage: $0 file
   exit 1
fi

# Make sure the file exists
if [ ! -f $1 ] ; then
   echo File $1 does not exist
   exit 1
fi

if [ ! -f "${BUILD_ROOT}"/support/key.pass ] || [ ! -f "${BUILD_ROOT}"/support/key.pfx ] ; then
   echo Key key.pfx / key.pass does not exist
   exit 1
fi

PASS=$(cat "${BUILD_ROOT}"/support/key.pass)

# First check to see if it is already signed.
# An exit value of 1 from signtool indicates it is not signed.
signtool verify //pa $1 >/dev/null 2>&1
if [ $? -eq 1 ] ; then

    # This is the list of timestamp servers to try.
    # Sometime the signing operation fails because we can't contact the
    # timestamp server, so we try several servers.
    TSSLIST=(
        http://timestamp.comodoca.com/rfc3161
        http://timestamp.digicert.com
        http://tsa.starfieldtech.com
        http://timestamp.geotrust.com/tsa)

    TSS=${TSSLIST[0]}
    echo Signing $1 using timestamp server $TSS ...
    sleep 1
    signtool sign //f "${BUILD_ROOT}"/support/key.pfx //fd sha256 //p $PASS //tr $TSS //td sha256 $1
    if [ $? -eq 0 ] ; then
       # Successfully signed, return success
       exit 0
    fi

    for RETRY in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ; do

        # Sometimes signtool returns failure, but the file was already signed.
        # Not sure why that happens. Since the file is now signed, return successs.
        sleep 1
        signtool verify //pa $1 >/dev/null 2>&1
        if [ $? -eq 0 ] ; then
           echo Signing returned failure, but file was signed. Returning success.
           exit 0
        fi

        # Retry with a different timestamp server.
        TSS=${TSSLIST[`expr $RETRY % ${#TSSLIST[@]}`]}
        echo Signing failed, retry $RETRY. Using timestamp server $TSS ...
        sleep 4
        echo Retrying signing of $1
        signtool sign //f "${BUILD_ROOT}"/support/key.pfx //p $PASS //tr $TSS  $1
        if [ $? -eq 0 ] ; then
           # Successfully signed, return success
           exit 0
        fi
    done
    # We didn't sign the file succesfully
    exit 1
else
    echo Signing of $1 skipped, already signed...
    exit 0
fi
