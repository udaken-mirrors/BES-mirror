# Microsoft Developer Studio Project File - Name="BES" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=BES - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BES.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BES.mak" CFG="BES - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BES - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "BES - Win32 Release Unicode" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BES - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\vc6\Non-Unicode"
# PROP Intermediate_Dir ".\vc6\Non-Unicode"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W4 /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D DPSAPI_VERSION=1 /D _WIN32_WINNT=0x0500 /U "_UNICODE" /U "UNICODE" /U "_MBCS" /U "MBCS" /U "min" /U "max" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 odbc32.lib odbccp32.lib comctl32.lib Msimg32.lib Psapi.lib Winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Advapi32.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "BES - Win32 Release Unicode"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BES___Win32_Release_Unicode"
# PROP BASE Intermediate_Dir "BES___Win32_Release_Unicode"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\vc6\"
# PROP Intermediate_Dir ".\vc6\"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /Ob0 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W4 /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /D DPSAPI_VERSION=1 /D _WIN32_WINNT=0x0500 /U "_MBCS" /U "MBCS" /U "min" /U "max" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT BASE LINK32 /incremental:yes /debug
# ADD LINK32 odbc32.lib odbccp32.lib comctl32.lib Msimg32.lib Psapi.lib Winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Advapi32.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /verbose /pdb:none

!ENDIF 

# Begin Target

# Name "BES - Win32 Release"
# Name "BES - Win32 Release Unicode"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\hack.cpp
# ADD CPP /MT
# SUBTRACT CPP /O<none>
# End Source File
# Begin Source File

SOURCE=.\ini.cpp
# End Source File
# Begin Source File

SOURCE=.\list.cpp
# ADD CPP /MT
# SUBTRACT CPP /O<none>
# End Source File
# Begin Source File

SOURCE=.\log.cpp
# ADD CPP /MT
# SUBTRACT CPP /O<none>
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# ADD CPP /MT
# SUBTRACT CPP /O<none>
# End Source File
# Begin Source File

SOURCE=.\misc.cpp
# End Source File
# Begin Source File

SOURCE=.\NotifyIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\skin.cpp
# End Source File
# Begin Source File

SOURCE=.\sstp.cpp
# End Source File
# Begin Source File

SOURCE=.\tooltips.cpp
# End Source File
# Begin Source File

SOURCE=.\watch.cpp
# End Source File
# Begin Source File

SOURCE=.\wndproc.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BattleEnc.h
# End Source File
# Begin Source File

SOURCE=.\BES.manifest
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\sstp.sjis.h
# End Source File
# Begin Source File

SOURCE=.\strings.utf8.h
# End Source File
# Begin Source File

SOURCE=.\Trackbar.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=".\active.ico"
# End Source File
# Begin Source File

SOURCE=.\BattleEnc.rc
# End Source File
# Begin Source File

SOURCE=".\idle.ico"
# End Source File
# End Group
# Begin Source File

SOURCE=.\history.txt
# End Source File
# End Target
# End Project
