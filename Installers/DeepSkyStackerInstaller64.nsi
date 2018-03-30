##!include "MUI2.nsh"

!define DSS_ICON           "..\DeepSkyStacker\Icon\DSS.ico"

!define DSS_HELP_FR        "..\Help\Aide DeepSkyStacker.chm"
!define DSS_HELP_ES        "..\Help\Ayuda DeepSkyStacker.chm"
!define DSS_HELP_EN        "..\Help\DeepSkyStacker Help.chm"
!define DSS_HELP_DE        "..\Help\DeepSkyStacker Hilfe.chm"
!define DSS_HELP_PT        "..\Help\DeepSkyStacker Ajuda.chm"


!define DSS_PRODUCT        "DeepSkyStacker64"

!define DSS_NAME           "Deep Sky Stacker (64 bit)"
!define DSS_FILE           "DeepSkyStacker"

!define DSSCL_NAME         "Deep Sky Stacker Command Line (64 bit)"
!define DSSCL_FILE         "DeepSkyStackerCL"

!define DSS_UNINSTALL_FILE "DeepSkyStackerUninstaller"

CRCCheck On


# define installer name

OutFile "DeepSkyStacker64Installer.exe"
 
# set the install directory - the programs are 32 bit versions

InstallDir "$PROGRAMFILES64\${DSS_PRODUCT}"

# 

RequestExecutionLevel admin

# Enable/disable UI features we do/dont want

ShowInstDetails       nevershow
ShowUninstDetails     nevershow

Name                  "${DSS_NAME}"
Icon                  "${DSS_ICON}"



# default installer section start

Section

  # Modify UI behaviours
  
  SetDetailsPrint     none

  # define output path

  SetOutPath $INSTDIR
 
  # Automatically uninstall previous version
  
  Exec "$INSTDIR\${DSS_UNINSTALL_FILE}.exe"


  # specify files to go in the output path

  File "..\x64\Release\${DSS_FILE}.exe"
  File "..\x64\Release\${DSSCL_FILE}.exe"
  File "${DSS_HELP_FR}"
  File "${DSS_HELP_ES}"
  File "${DSS_HELP_EN}"
  File "${DSS_HELP_DE}"
  File "${DSS_HELP_PT}"

 
  # define uninstaller name

  WriteUninstaller "$INSTDIR\${DSS_UNINSTALL_FILE}.exe"
  

  # create desktop shortcut

  CreateShortCut "$DESKTOP\${DSS_PRODUCT}.lnk" "$INSTDIR\${DSS_FILE}.exe" ""
 
  # create start-menu items

  CreateDirectory "$SMPROGRAMS\${DSS_PRODUCT}"
  CreateShortCut  "$SMPROGRAMS\${DSS_PRODUCT}\${DSS_PRODUCT}.lnk" "$INSTDIR\${DSS_FILE}.exe" "" "$INSTDIR\${DSS_FILE}.exe" 0
 
  CreateShortCut  "$SMPROGRAMS\${DSS_PRODUCT}\${DSS_UNINSTALL_FILE}.lnk" "$INSTDIR\${DSS_UNINSTALL_FILE}.exe" "" "$INSTDIR\${DSS_UNINSTALL_FILE}.exe" 0

  # write uninstall information to the registry
 
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${DSS_PRODUCT}" "DisplayName" "${DSS_PRODUCT} (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${DSS_PRODUCT}" "UninstallString" "$INSTDIR\${DSS_UNINSTALL_FILE}.exe"

  # Create the uninstaller program
  
  WriteUninstaller "$INSTDIR\${DSS_UNINSTALL_FILE}.exe"

  # default section end

SectionEnd


 
# create a section to define what the uninstaller does.
# the section will always be named "Uninstall"

Section "Uninstall"
 
  # Modify UI behaviours
  
  SetDetailsPrint     none


  # Always delete uninstaller first

  Delete "$INSTDIR\${DSS_UNINSTALL_FILE}.exe" 

  # now delete installed files

  Delete "$INSTDIR\${DSS_FILE}.exe"
 
  # Delete Start Menu Shortcuts
   
  Delete "$DESKTOP\${DSS_PRODUCT}.lnk"
  Delete "$SMPROGRAMS\${DSS_PRODUCT}\*.*"
  RmDir  "$SMPROGRAMS\${DSS_PRODUCT}"
  
  # Delete Uninstaller And Uninstall Registry Entries
  
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\${DSS_PRODUCT}"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${DSS_PRODUCT}" 
  

SectionEnd
