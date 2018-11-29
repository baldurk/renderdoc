#!/bin/bash

# We don't have symbol servers on linux
if [ "$PLATFORM" == "Linux" ]; then
	exit;
fi

if [[ "$GITTAG" == "" ]]; then
	echo "Git tag is not valid.";
	exit 0;
fi

if [ ! -d "${REPO_ROOT}"/Win32/Release ] || [ ! -d "${REPO_ROOT}"/x64/Release ]; then
	echo "WARNING: No compiled binaries found in release folders."
	exit 0
fi

if [ ! -f "${BUILD_ROOT}"/support/pdbstr.exe ]; then
	echo "WARNING: Need pdbstr.exe from Windows Debugger folder in build root to set up source server information in symbol files."
	echo "e.g. C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\srcsrv\pdbstr.exe";
	exit 0
fi

##########################################################
# Create a source-mapping file to embed into PDBs

PDBSTR="${REPO_ROOT}"/pdbstr.txt

cat << EOF > "${PDBSTR}"
SRCSRV: ini ------------------------------------------------
VERSION=2
VERCTRL=http
SRCSRV: variables ------------------------------------------
HTTP_ALIAS=https://raw.githubusercontent.com/baldurk/renderdoc/$GITTAG/
HTTP_EXTRACT_TARGET=%HTTP_ALIAS%%var2%
SRCSRVTRG=%HTTP_EXTRACT_TARGET%
SRCSRV: source files ---------------------------------------
EOF

for I in $(find "${REPO_ROOT}" \( -path '*/3rdparty' -o -path '*/build-android*' -o -path '*/generated' \) -prune -o \( -iname '*.cpp' -o -iname '*.c' -o -iname '*.h' -o -iname '*.inl' \) -print); do
	echo $(native_path ${I})*${I};
done |
  sed -e '{s#\*'"${REPO_ROOT}"'/\?#\*#g}' |
  awk -F"*" '{gsub("/","\\",$1); print $1 "*" $2}' >> "${PDBSTR}"

echo "SRCSRV: end ------------------------------------------------" >> "${PDBSTR}"

##########################################################

# Apply the source-indexing mapping into every pdb file
for PDB in "${REPO_ROOT}"/Win32/Release/*.pdb "${REPO_ROOT}"/x64/Release/*.pdb; do
	"${BUILD_ROOT}"/support/pdbstr.exe -w -p:$(native_path $PDB) -s:srcsrv -i:$(native_path "${PDBSTR}")
done

rm "${PDBSTR}"

if [ ! -f "${BUILD_ROOT}"/support/symstore.exe ]; then
	echo "Need symstore.exe from Windows Debugger folder in build root."
	echo "e.g. C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symstore.exe";
	exit 0
fi

# check if a symbol store is specified
if [[ "$SYMSTORE" == "" ]]; then
	exit 0;
fi

TMPSTORE="${REPO_ROOT}"/symstore

echo "Storing symbols for $GITTAG in symbol store $SYMSTORE temporarily in $TMPSTORE"

mkdir -p "${SYMSTORE}"

# Store the pdbs in a symbol store
# Process Win32 and x64 separately since they'll overwrite each other otherwise.
for ARCH in Win32 x64; do

	rm -rf "${TMPSTORE}"

	# Add to local symbol store (which is rsync'd to public symbol server)
	for PDB in "${REPO_ROOT}"/$ARCH/Release/*.pdb; do
		mkdir -p "${TMPSTORE}"
		cp $PDB "${TMPSTORE}"

		EXE=${PDB%.pdb}.exe
		DLL=${PDB%.pdb}.dll
		if [ -f "$EXE" ]; then
			cp $EXE "${TMPSTORE}"
		fi
		if [ -f "$DLL" ]; then
			cp $DLL "${TMPSTORE}"
		fi
	done

	if [ -d "${TMPSTORE}" ]; then
		MSYS2_ARG_CONV_EXCL="*" "${BUILD_ROOT}"/support/symstore.exe add /s "$(native_path "${SYMSTORE}")" /compress /r /f "$(native_path "${TMPSTORE}")" /t RenderDoc /v $GITTAG
	fi

done

rm -rf "${TMPSTORE}"
