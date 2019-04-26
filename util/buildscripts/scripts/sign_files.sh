#!/bin/bash

# We don't sign files on linux
if [ "$PLATFORM" == "Linux" ]; then
	exit;
fi

# Skip silently if no key is present
if [ ! -f "${BUILD_ROOT}"/support/key.pass ] || [ ! -f "${BUILD_ROOT}"/support/key.pfx ] ; then
	exit;
fi

# Kill any running processes
for EXE in "${REPO_ROOT}"/Win32/Release/*.exe; do
	taskkill.exe /F /IM $(basename "${EXE}");
done

# sign all of our files
for PDB in "${REPO_ROOT}"/Win32/Release/*.pdb "${REPO_ROOT}"/x64/Release/*.pdb; do
	EXE=${PDB%.pdb}.exe
	DLL=${PDB%.pdb}.dll

	if [ -f "$EXE" ]; then
		bash "${BUILD_ROOT}"/scripts/sign.sh $EXE
	fi
	if [ -f "$DLL" ]; then
		bash "${BUILD_ROOT}"/scripts/sign.sh $DLL
	fi
done

