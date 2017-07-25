#!/bin/sh
VERSION=$1
PLIST=$2

# Delete the key if it already exists
/usr/libexec/PlistBuddy -c "Delete :CFBundleShortVersionString" "$PLIST" >/dev/null 2>&1

# Now add with the right value
/usr/libexec/PlistBuddy -c "Add :CFBundleShortVersionString string $VERSION" "$PLIST" || exit 1

# Set identifier
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier org.renderdoc.qrenderdoc" "$PLIST" || exit 1
