# Microsoft Developer Studio Project File - Name="apc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=apc - Win32 Debug_TS
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "apc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "apc.mak" CFG="apc - Win32 Debug_TS"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "apc - Win32 Debug_TS" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "apc - Win32 Release_TS" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "apc - Win32 Debug_TS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_TS"
# PROP BASE Intermediate_Dir "Debug_TS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_TS"
# PROP Intermediate_Dir "Debug_TS"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APC_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\..\php4" /I "..\..\..\php4\main" /I "..\..\..\php4\Zend" /I "..\..\..\php4\TSRM" /I "..\..\..\php4\win32" /I "..\..\..\php4\regex" /D "TSRM_LOCKS" /D HAVE_APC=1 /D "COMPILE_DL_APC" /D ZEND_DEBUG=1 /D ZTS=1 /D "ZEND_WIN32" /D "PHP_WIN32" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APC_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 php4ts_debug.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"Debug_TS/php_apc.dll" /pdbtype:sept /libpath:"..\..\..\php4\Debug_TS"

!ELSEIF  "$(CFG)" == "apc - Win32 Release_TS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_TS"
# PROP BASE Intermediate_Dir "Release_TS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_TS"
# PROP Intermediate_Dir "Release_TS"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\php4" /I "..\..\..\php4\main" /I "..\..\..\php4\Zend" /I "..\..\..\php4\TSRM" /I "..\..\..\php4\win32" /I "..\..\..\php4\regex" /D "TSRM_LOCKS" /D HAVE_APC=1 /D "COMPILE_DL_APC" /D ZEND_DEBUG=0 /D ZTS=1 /D "ZEND_WIN32" /D "PHP_WIN32" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APC_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 php4ts.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"../../Release_TS/php_apc.dll" /libpath:"..\..\..\php4\Release_TS" /libpath:"..\..\..\php4\Release_TS_Inline"

!ENDIF 

# Begin Target

# Name "apc - Win32 Debug_TS"
# Name "apc - Win32 Release_TS"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\apc.c
# End Source File
# Begin Source File

SOURCE=.\apc_cache.c
# End Source File
# Begin Source File

SOURCE=.\apc_compile.c
# End Source File
# Begin Source File

SOURCE=.\apc_debug.c
# End Source File
# Begin Source File

SOURCE=.\apc_fcntl_win32.c
# End Source File
# Begin Source File

SOURCE=.\apc_main.c
# End Source File
# Begin Source File

SOURCE=.\apc_rfc1867.c
# End Source File
# Begin Source File

SOURCE=.\apc_shm.c
# End Source File
# Begin Source File

SOURCE=.\apc_sma.c
# End Source File
# Begin Source File

SOURCE=.\apc_stack.c
# End Source File
# Begin Source File

SOURCE=.\apc_zend.c
# End Source File
# Begin Source File

SOURCE=.\php_apc.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\apc.h
# End Source File
# Begin Source File

SOURCE=.\apc_cache.h
# End Source File
# Begin Source File

SOURCE=.\apc_compile.h
# End Source File
# Begin Source File

SOURCE=.\apc_debug.h
# End Source File
# Begin Source File

SOURCE=.\apc_fcntl.h
# End Source File
# Begin Source File

SOURCE=.\apc_globals.h
# End Source File
# Begin Source File

SOURCE=.\apc_lock.h
# End Source File
# Begin Source File

SOURCE=.\apc_main.h
# End Source File
# Begin Source File

SOURCE=.\apc_php.h
# End Source File
# Begin Source File

SOURCE=.\apc_shm.h
# End Source File
# Begin Source File

SOURCE=.\apc_sma.h
# End Source File
# Begin Source File

SOURCE=.\apc_stack.h
# End Source File
# Begin Source File

SOURCE=.\apc_zend.h
# End Source File
# Begin Source File

SOURCE=.\php_apc.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
