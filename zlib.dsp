# Microsoft Developer Studio Project File - Name="zlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=zlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "zlib.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "zlib.mak" CFG="zlib - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "zlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "zlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "zlib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "zlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i ".." /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF

# Begin Target

# Name "zlib - Win32 Release"
# Name "zlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\zlib-1.1.3\adler32.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\compress.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\crc32.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\deflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\gzio.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\infblock.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\infcodes.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\inffast.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\inflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\infutil.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\trees.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\zutil.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\zlib-1.1.3\deflate.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\infblock.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\infcodes.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\inffast.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\infutil.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\trees.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\zconf.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\zlib.h
# End Source File
# Begin Source File

SOURCE=.\zlib-1.1.3\zutil.h
# End Source File
# End Group
# End Target
# End Project
