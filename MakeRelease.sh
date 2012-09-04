#!/bin/sh

if [ "$1" = "win7" ] ; then
	DSTVERSION="7"
elif [ "$1" = "winxp" ] ; then
	DSTVERSION="XP"
else
	echo "Usage: MakeRelease.sh [win7|winxp] [chk]"
	exit
fi

if [ "$2" = "chk" ] ; then
	FRECHK="chk"
else
	FRECHK="fre"
fi

DSTDIR="Release $DSTVERSION"
ARCHIVE="ghost-win$DSTVERSION.zip"
if [ "$DSTVERSION" = "7" ] ; then
	OBJDIR="win7"
elif [ "$DSTVERSION" = "XP" ] ; then
	OBJDIR="wxp"
fi

if [ -d "$DSTDIR" ] ; then
	rm -rf "${DSTDIR}/*"
else
	mkdir "$DSTDIR"
fi

if [ -f "$ARCHIVE" ] ; then
	rm "$ARCHIVE"
fi

cp AUTHORS "$DSTDIR"
cp CHANGELOG "$DSTDIR"
cp COPYING "$DSTDIR"
cp INSTALL "$DSTDIR"
cp README "$DSTDIR"

cp "ghostbus${DSTVERSION}.inf" "${DSTDIR}/ghostbus.inf"
cp "ghostdrive${DSTVERSION}.inf" "${DSTDIR}/ghostdrive.inf"
cp GhostBus/sys/obj${FRECHK}_${OBJDIR}_x86/i386/GhostBus.sys "$DSTDIR"
cp GhostBus/exe/obj${FRECHK}_${OBJDIR}_x86/i386/GhostTool.exe "$DSTDIR"
cp GhostDrive/obj${FRECHK}_${OBJDIR}_x86/i386/GhostDrive.sys "$DSTDIR"
cp Test/obj${FRECHK}_${OBJDIR}_x86/i386/Test.exe "$DSTDIR"
cp GhostLib/obj${FRECHK}_${OBJDIR}_x86/i386/GhostLib.dll "$DSTDIR"
cp GhostLib/obj${FRECHK}_${OBJDIR}_x86/i386/GhostLib.lib "$DSTDIR"
cp GhostLib/ghostlib.h "$DSTDIR"
cp GhostGUI/bin/Release/GhostGUI.exe "$DSTDIR"
cp Installer/obj${FRECHK}_${OBJDIR}_x86/i386/Setup.exe "$DSTDIR"

if [ "$DSTVERSION" = "XP" ] ; then
	if [ -f WdfCoInstaller01009.dll ] ; then
		cp WdfCoInstaller01009.dll "$DSTDIR"
	fi
	if [ -f WUDFUpdate_01009.dll ] ; then
		cp WUDFUpdate_01009.dll "$DSTDIR"
	fi
fi

cd "$DSTDIR"
zip "../$ARCHIVE" ghostbus.inf ghostdrive.inf GhostBus.sys GhostTool.exe GhostDrive.sys ghostlib.h \
	Test.exe GhostLib.dll GhostLib.lib GhostGUI.exe Setup.exe AUTHORS CHANGELOG COPYING INSTALL README
cd ..
