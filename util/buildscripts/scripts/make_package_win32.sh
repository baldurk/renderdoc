#!/bin/bash

FILENAME="$1"

if [ $# -ne 1 ]; then
	echo "Usage: $0 FILENAME";
	exit;
fi

if [ ! -d "${REPO_ROOT}"/x64/Release ] || [ ! -d "${REPO_ROOT}"/Win32/Release ]; then
	echo "ERROR: Missing one of Win32 or x64 Release builds";
	exit 1;
fi

pushd "${REPO_ROOT}"

# clean any old files lying around and make new structure
rm -rf dist
mkdir -p dist/Release{32,64}

# Copy files from release build in, without copying obj/
pushd x64/Release
find * -not -path 'obj*' -and -not -path '*.lib' -and -not -path 'pymodules*' -exec cp -r --parents '{}' ../../dist/Release64/ \;
popd

pushd Win32/Release
find * -not -path 'obj*' -and -not -path '*.lib' -and -not -path 'pymodules*' -exec cp -r --parents '{}' ../../dist/Release32/ \;
popd

# Copy in d3dcompiler from windows kit. Prefer 10 over 8.1 but either works
if [ -f "${WIN_ROOT}c/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/d3dcompiler_47.dll" ]; then
	cp "${WIN_ROOT}c/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/d3dcompiler_47.dll" dist/Release64/
	cp "${WIN_ROOT}c/Program Files (x86)/Windows Kits/10/Redist/D3D/x86/d3dcompiler_47.dll" dist/Release32/
elif [ -f "${WIN_ROOT}c/Program Files (x86)/Windows Kits/8.1/Redist/D3D/x64/d3dcompiler_47.dll" ]; then 
	cp "${WIN_ROOT}c/Program Files (x86)/Windows Kits/8.1/Redist/D3D/x64/d3dcompiler_47.dll" dist/Release64/
	cp "${WIN_ROOT}c/Program Files (x86)/Windows Kits/8.1/Redist/D3D/x86/d3dcompiler_47.dll" dist/Release32/
else
	echo "WARNING: Couldn't find d3dcompiler_47.dll from Windows Kits redist.";
fi

# Copy associated files that should be included with the distribution
cp LICENSE.md Documentation/htmlhelp/*.chm dist/Release64/
cp LICENSE.md Documentation/htmlhelp/*.chm dist/Release32/

# Copy in appropriate plugins folder if they exist
if [ -d plugins-win64 ]; then
	cp -R plugins-win64/ dist/Release64/plugins
else
	echo "WARNING: x64 plugins missing, download and extract https://renderdoc.org/plugins.zip in root";
fi;

if [ -d plugins-win32 ]; then
	cp -R plugins-win32/ dist/Release32/plugins
else
	echo "WARNING: x86 plugins missing, download and extract https://renderdoc.org/plugins.zip in root";
fi

# Delete new VS2015 incremental pdb files, these are just build artifacts
# and aren't needed for later symbol resolution etc
find dist/Release{32,64}/ -iname '*.ipdb' -exec rm '{}' \;
find dist/Release{32,64}/ -iname '*.iobj' -exec rm '{}' \;

# Copy in any android APKs that were built
mkdir -p dist/Release64/plugins/android/
if ls build-android-* > /dev/null; then
	find build-android-* -iname 'org.renderdoc.renderdoccmd.*.apk' -exec cp '{}' dist/Release64/plugins/android ';'
else
	echo "WARNING: No android builds found, expected build-android-arm32 and build-android-arm64";
fi

# Copy in android adb and patching requirements
if [ -f plugins-android/adb.exe ]; then
	cp -R plugins-android/* dist/Release64/plugins/android/;
else
	echo "WARNING: Couldn't find android dependency (adb.exe and friends)";
	echo "         These files must be in plugins-android/ in the root and";
	echo "         MUST be built locally from an AOSP checkout, not from a";
	echo "         distributed android SDK due to licensing concerns.";
fi

# Generate a debug key for signing purposes
if [ -f "$JAVA_HOME/bin/keytool.exe" ] && [ -d dist/Release64/plugins/android ]; then
	"$JAVA_HOME/bin/keytool.exe" -genkey -keystore dist/Release64/plugins/android/renderdoc.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=, OU=, O=, L=,  S=, C="
elif [ -f "$JAVA_HOME/bin/keytool" ] && [ -d dist/Release64/plugins/android ]; then
	"$JAVA_HOME/bin/keytool" -genkey -keystore dist/Release64/plugins/android/renderdoc.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=, OU=, O=, L=,  S=, C="
fi

if [ -d dist/Release64/plugins/android ]; then
	cp -R dist/Release64/plugins/android dist/Release32/plugins/
fi

# Make a copy of the main distribution folder that has PDBs
cp -R dist/Release64 dist/ReleasePDBs64
cp -R dist/Release32 dist/ReleasePDBs32

# Remove all pdbs
find dist/Release{32,64}/ -iname '*.pdb' -exec rm '{}' \;

# Remove any build associated files that might have gotten dumped in the folders
rm -f dist/Release{32,64}/*.{exp,lib,metagen,xml} dist/Release{32,64}/*.vshost.*

# Delete all but xml files from PDB folder as well (large files, and not useful)
rm -f dist/ReleasePDBs{32,64}/*.{exp,lib,metagen} dist/Release{32,64}/*.vshost.*

# In the 64bit release folder, make an x86 subfolder and copy in renderdoc 32bit
mkdir -p dist/Release64/x86
cp -R dist/Release32/{d3dcompiler_47.dll,renderdoc.dll,renderdoc.json,renderdocshim32.dll,renderdoccmd.exe,dbghelp.dll,symsrv.dll,symsrv.yes} dist/Release64/x86/
mkdir -p dist/ReleasePDBs64/x86
cp -R dist/ReleasePDBs32/{d3dcompiler_47.dll,renderdoc.dll,renderdoc.json,renderdoc.pdb,renderdocshim32.dll,renderdocshim32.pdb,renderdoccmd.exe,renderdoccmd.pdb,dbghelp.dll,symsrv.dll,symsrv.yes} dist/ReleasePDBs64/x86/

VERSION=`grep -E "#define RENDERDOC_VERSION_(MAJOR|MINOR)" renderdoc/api/replay/version.h | tr -dc '[0-9\n]' | tr '\n' '.' | grep -Eo '[0-9]+\.[0-9]+'`

export RENDERDOC_VERSION="${VERSION}"

# Ensure this variable passes through to windows on WSL
export WSLENV=$WSLENV:RENDERDOC_VERSION

"$WIX/bin/candle.exe" -o dist/Installer32.wixobj util/installer/Installer32.wxs
"$WIX/bin/light.exe" -ext WixUIExtension -sw1076 -loc util/installer/customtext.wxl -o dist/Installer32.msi dist/Installer32.wixobj

"$WIX/bin/candle.exe" -o dist/Installer64.wixobj util/installer/Installer64.wxs
"$WIX/bin/light.exe" -ext WixUIExtension -sw1076 -loc util/installer/customtext.wxl -o dist/Installer64.msi dist/Installer64.wixobj

rm -f dist/*.wixobj dist/*.wixpdb

popd # $REPO_ROOT

rm -rf "${REPO_ROOT}"/package
mkdir "${REPO_ROOT}"/package
pushd "${REPO_ROOT}"/package

cp -R "${REPO_ROOT}"/dist/Release32 ${FILENAME}_32
cp -R "${REPO_ROOT}"/dist/Release64 ${FILENAME}_64
zip -r ${FILENAME}_32.zip ${FILENAME}_32/
zip -r ${FILENAME}_64.zip ${FILENAME}_64/
gpg -o ${FILENAME}_32.zip.sig --detach-sign --armor ${FILENAME}_32.zip
gpg -o ${FILENAME}_64.zip.sig --detach-sign --armor ${FILENAME}_64.zip

if [ -f "${REPO_ROOT}"/dist/Installer32.msi ]; then

	cp "${REPO_ROOT}"/dist/Installer32.msi ${FILENAME}_32.msi
	cp "${REPO_ROOT}"/dist/Installer64.msi ${FILENAME}_64.msi

	gpg -o ${FILENAME}_32.msi.sig --detach-sign --armor ${FILENAME}_32.msi
	gpg -o ${FILENAME}_64.msi.sig --detach-sign --armor ${FILENAME}_64.msi

	# On windows, also sign the installers
	"${BUILD_ROOT}"/scripts/sign.sh ${FILENAME}_32.msi
	"${BUILD_ROOT}"/scripts/sign.sh ${FILENAME}_64.msi

fi;

rm -rf ${FILENAME}_32/ ${FILENAME}_64/

popd # $REPO_ROOT/package
