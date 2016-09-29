#!/bin/bash

AUTOBUILD=1

if [ ! -f LICENSE.md ]; then
	echo "This script should be run from the root of the checkout.";
	echo "e.g. ./scripts/dist.sh";
	exit;
fi

if [ $# -ne 1 ] || [ $1 != "autobuild" ]; then
	AUTOBUILD=0
	echo "=== Building standalone folders. Hit enter when each prompt is satisfied"

	echo "Have you rebuilt the documentation? (cd docs/ && ./make.sh htmlhelp)"
	read;

	echo "Have you built the python libraries? (cd renderdocui/3rdparty/ironpython/ && ./compilelibs.sh /path/to/IronPython)"
	read;

	echo "Have you marked the git commit hash in version info? (./scripts/hash_version.sh)"
	read;

	echo "Now compile 32-bit and 64-bit Release builds."
	read;

	echo "=== Building folders"
fi

# clean any old files lying around and make new structure
rm -rf dist
mkdir -p dist/Release{32,64}

# Copy files from release build in
cp -R x64/Release/* dist/Release64/
cp -R Win32/Release/* dist/Release32/

cp renderdoc/api/app/renderdoc_app.h dist/Release64/
cp renderdoc/api/app/renderdoc_app.h dist/Release32/

cp renderdocui/3rdparty/ironpython/pythonlibs.zip dist/Release64/
cp renderdocui/3rdparty/ironpython/pythonlibs.zip dist/Release32/

# Copy in d3dcompiler from windows kit 8.1
cp /c/Program\ Files\ \(x86\)/Windows\ Kits/8.1/Redist/D3D/x64/d3dcompiler_47.dll dist/Release64/
cp /c/Program\ Files\ \(x86\)/Windows\ Kits/8.1/Redist/D3D/x86/d3dcompiler_47.dll dist/Release32/

# Copy associated files that should be included with the distribution
cp LICENSE.md Documentation/htmlhelp/*.chm dist/Release64/
cp LICENSE.md Documentation/htmlhelp/*.chm dist/Release32/

# Delete new VS2015 incremental pdb files, these are just build artifacts
# and aren't needed for later symbol resolution etc
find dist/Release{32,64}/ -iname '*.ipdb' -exec rm '{}' \;
find dist/Release{32,64}/ -iname '*.iobj' -exec rm '{}' \;

# Make a copy of the main distribution folder that has PDBs
cp -R dist/Release64 dist/ReleasePDBs64
cp -R dist/Release32 dist/ReleasePDBs32

# Remove all pdbs except renderdocui.pdb (which we keep so we can get callstack crashes)
find dist/Release{32,64}/ -iname '*.pdb' -exec rm '{}' \;
cp dist/ReleasePDBs32/renderdocui.pdb dist/Release32/
cp dist/ReleasePDBs64/renderdocui.pdb dist/Release64/

# Remove any build associated files that might have gotten dumped in the folders
rm -f dist/Release{32,64}/*.{exp,lib,metagen,xml} dist/Release{32,64}/*.vshost.*

# Delete all but xml files from PDB folder as well (large files, and not useful)
rm -f dist/ReleasePDBs{32,64}/*.{exp,lib,metagen} dist/Release{32,64}/*.vshost.*

# In the 64bit release folder, make an x86 subfolder and copy in renderdoc 32bit
mkdir -p dist/Release64/x86
rm -rf dist/Release32/pdblocate/x64 dist/ReleasePDBs32/pdblocate/x64
cp -R dist/Release32/{d3dcompiler_47.dll,renderdoc.dll,renderdoc.json,renderdocshim32.dll,renderdoccmd.exe,pdblocate} dist/Release64/x86/
mkdir -p dist/ReleasePDBs64/x86
cp -R dist/ReleasePDBs32/{d3dcompiler_47.dll,renderdoc.dll,renderdoc.json,renderdoc.pdb,renderdocshim32.dll,renderdocshim32.pdb,renderdoccmd.exe,renderdoccmd.pdb,pdblocate} dist/ReleasePDBs64/x86/

if [[ $AUTOBUILD -eq 0 ]]; then
	echo "=== Folders built. Ready to make installer MSIs."
	echo
	echo "Hit enter when ready"
	read;
fi

VERSION=`grep -E "#define RENDERDOC_VERSION_(MAJOR|MINOR)" renderdoc/data/version.h | tr -dc '[0-9\n]' | tr '\n' '.' | grep -Eo '[0-9]+\.[0-9]+'`

export RENDERDOC_VERSION="${VERSION}.0"

"$WIX/bin/candle.exe" -o dist/Installer32.wixobj installer/Installer32.wxs
"$WIX/bin/light.exe" -ext WixUIExtension -sw1076 -loc installer/customtext.wxl -o dist/Installer32.msi dist/Installer32.wixobj

"$WIX/bin/candle.exe" -o dist/Installer64.wixobj installer/Installer64.wxs
"$WIX/bin/light.exe" -ext WixUIExtension -sw1076 -loc installer/customtext.wxl -o dist/Installer64.msi dist/Installer64.wixobj

rm dist/*.wixobj dist/*.wixpdb
