#!/bin/bash

AUTOBUILD=1

if [ $# -ne 1 ] || [ $1 != "autobuild" ]; then
	AUTOBUILD=0
	echo "Have you rebuilt the documentation?"

	read;

	echo "Have you marked git commit hash in AssemblyInfo.cs and resource.h (run hash_version.sh)?"
	echo "If this is an OFFICIAL build only, mark that in the assembly info and resource.h and bump their versions."

	read;
fi

rm -rf dist
mkdir -p dist/Release{32,64}
cp -R x64/Release/* dist/Release64/
cp -R Win32/Release/* dist/Release32/
cp LICENSE.md Documentation/*.chm dist/Release64/
cp LICENSE.md Documentation/*.chm dist/Release32/
cp -R dist/Release64 dist/ReleasePDBs64
cp -R dist/Release32 dist/ReleasePDBs32
find dist/Release32/ -iname '*.pdb' -exec rm '{}' \;
find dist/Release64/ -iname '*.pdb' -exec rm '{}' \;
rm dist/Release32/*.{exp,lib,metagen,xml} dist/Release32/*.vshost.*
rm dist/Release64/*.{exp,lib,metagen,xml} dist/Release64/*.vshost.*
mkdir -p dist/Release64/x86
rm -rf dist/Release32/pdblocate/x64 dist/ReleasePDBs32/pdblocate/x64
cp -R dist/Release32/{renderdoc.dll,renderdoccmd.exe,pdblocate} dist/Release64/x86/
mkdir -p dist/ReleasePDBs64/x86
cp -R dist/ReleasePDBs32/{renderdoc.dll,renderdoc.pdb,renderdoccmd.exe,renderdoccmd.pdb,pdblocate} dist/ReleasePDBs64/x86/

if [[ $AUTOBUILD -eq 1 ]]; then
	exit;
fi

echo "Ready to make installer MSIs - make sure to bump version numbers on package."

read;

/c/Program\ Files\ \(x86\)/WiX\ Toolset\ v3.8/bin/candle.exe -o dist/Installer32.wixobj installer/Installer32.wxs
/c/Program\ Files\ \(x86\)/WiX\ Toolset\ v3.8/bin/light.exe -ext WixUIExtension -sw1076 -o dist/Installer32.msi dist/Installer32.wixobj

/c/Program\ Files\ \(x86\)/WiX\ Toolset\ v3.8/bin/candle.exe -o dist/Installer64.wixobj installer/Installer64.wxs
/c/Program\ Files\ \(x86\)/WiX\ Toolset\ v3.8/bin/light.exe -ext WixUIExtension -sw1076 -o dist/Installer64.msi dist/Installer64.wixobj

rm dist/*.wixobj dist/*.wixpdb
