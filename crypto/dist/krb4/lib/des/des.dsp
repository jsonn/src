# Microsoft Developer Studio Project File - Name="des" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=des - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "des.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "des.mak" CFG="des - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "des - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "des - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "des - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\roken" /I "." /I "..\..\include" /I "..\..\include\win32" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 ..\roken\Release\roken.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386

!ELSEIF  "$(CFG)" == "des - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\roken" /I "." /I "..\..\include" /I "..\..\include\win32" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 ..\roken\Debug\roken.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /debug /machine:I386

!ENDIF 

# Begin Target

# Name "des - Win32 Release"
# Name "des - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\cbc3_enc.c
# End Source File
# Begin Source File

SOURCE=.\cbc_cksm.c
# End Source File
# Begin Source File

SOURCE=.\cbc_enc.c
# End Source File
# Begin Source File

SOURCE=.\cfb64ede.c
# End Source File
# Begin Source File

SOURCE=.\cfb64enc.c
# End Source File
# Begin Source File

SOURCE=.\cfb_enc.c
# End Source File
# Begin Source File

SOURCE=.\des.def
# End Source File
# Begin Source File

SOURCE=.\des_enc.c
# End Source File
# Begin Source File

SOURCE=.\dllmain.c
# End Source File
# Begin Source File

SOURCE=.\ecb3_enc.c
# End Source File
# Begin Source File

SOURCE=.\ecb_enc.c
# End Source File
# Begin Source File

SOURCE=.\ede_enc.c
# End Source File
# Begin Source File

SOURCE=.\enc_read.c
# End Source File
# Begin Source File

SOURCE=.\enc_writ.c
# End Source File
# Begin Source File

SOURCE=.\fcrypt.c
# End Source File
# Begin Source File

SOURCE=.\key_par.c
# End Source File
# Begin Source File

SOURCE=.\ncbc_enc.c
# End Source File
# Begin Source File

SOURCE=.\ofb64ede.c
# End Source File
# Begin Source File

SOURCE=.\ofb64enc.c
# End Source File
# Begin Source File

SOURCE=.\ofb_enc.c
# End Source File
# Begin Source File

SOURCE=.\passwd_dlg.c
# End Source File
# Begin Source File

SOURCE=.\pcbc_enc.c
# End Source File
# Begin Source File

SOURCE=.\qud_cksm.c
# End Source File
# Begin Source File

SOURCE=.\read_pwd.c
# End Source File
# Begin Source File

SOURCE=.\rnd_keys.c
# End Source File
# Begin Source File

SOURCE=.\rpc_enc.c
# End Source File
# Begin Source File

SOURCE=.\set_key.c
# End Source File
# Begin Source File

SOURCE=.\str2key.c
# End Source File
# Begin Source File

SOURCE=.\supp.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\des.h
# End Source File
# Begin Source File

SOURCE=.\des_locl.h
# End Source File
# Begin Source File

SOURCE=.\des_ver.h
# End Source File
# Begin Source File

SOURCE=.\md5.h
# End Source File
# Begin Source File

SOURCE=.\passwd_dlg.h
# End Source File
# Begin Source File

SOURCE=.\podd.h
# End Source File
# Begin Source File

SOURCE=.\rpc_des.h
# End Source File
# Begin Source File

SOURCE=.\sk.h
# End Source File
# Begin Source File

SOURCE=.\spr.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\passwd_dialog.rc
# End Source File
# End Group
# End Target
# End Project
