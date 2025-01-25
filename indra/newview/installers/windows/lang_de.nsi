; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\German.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_GERMAN} "Installationssprache"
LangString SelectInstallerLanguage  ${LANG_GERMAN} "Bitte wählen Sie die Installationssprache"

; subtitle on license text caption (setup new version or update current one
LangString LicenseSubTitleUpdate ${LANG_GERMAN} " Update"
LangString LicenseSubTitleSetup ${LANG_GERMAN} " Setup"

LangString MULTIUSER_TEXT_INSTALLMODE_TITLE ${LANG_GERMAN} "Installationsmodus"
LangString MULTIUSER_TEXT_INSTALLMODE_SUBTITLE ${LANG_GERMAN} "Für alle Benutzer (erfordert Administratorrechte) oder nur für den aktuellen Benutzer installieren?"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_TOP ${LANG_GERMAN} "Wenn Sie dieses Installationsprogram mit Administratorrechten ausführen, können Sie auswählen, ob die Installation (beispielsweise) in c:\Programme oder unter AppData\Lokaler Ordner des aktuellen Benutzers erfolgen soll."
LangString MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS ${LANG_GERMAN} "Für alle Benutzer installieren"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER ${LANG_GERMAN} "Nur für den aktuellen Benutzer installieren"

; installation directory text
LangString DirectoryChooseTitle ${LANG_GERMAN} "Installations-Ordner"
LangString DirectoryChooseUpdate ${LANG_GERMAN} "Wählen Sie den Firestorm Ordner für dieses Update:"
LangString DirectoryChooseSetup ${LANG_GERMAN} "Pfad in dem Firestorm installiert werden soll:"

LangString MUI_TEXT_DIRECTORY_TITLE ${LANG_GERMAN} "Installationsverzeichnis"
LangString MUI_TEXT_DIRECTORY_SUBTITLE ${LANG_GERMAN} "Wählen Sie das Verzeichnis aus, in dem Firestorm installiert werden soll:"

LangString MUI_TEXT_INSTALLING_TITLE ${LANG_GERMAN} "Firestorm wird installiert..."
LangString MUI_TEXT_INSTALLING_SUBTITLE ${LANG_GERMAN} "Firestorm wird im Verzeichnis $INSTDIR installiert"

LangString MUI_TEXT_FINISH_TITLE ${LANG_GERMAN} "Firestorm wird installiert"
LangString MUI_TEXT_FINISH_SUBTITLE ${LANG_GERMAN} "Firestorm wurde im Verzeichnis $INSTDIR installiert."

LangString MUI_TEXT_ABORT_TITLE ${LANG_GERMAN} "Installation abgebrochen"
LangString MUI_TEXT_ABORT_SUBTITLE ${LANG_GERMAN} "Firestorm wird nicht im Verzeichnis $INSTDIR installiert."

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_GERMAN} "Konnte Programm '$INSTNAME' nicht finden. Stilles Update fehlgeschlagen."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_GERMAN} "Firestorm starten?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_GERMAN} "Überprüfe alte Version ..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_GERMAN} "Überprüfung der Windows Version ..."
LangString CheckWindowsVersionMB ${LANG_GERMAN} 'Firestorm unterstützt nur Windows Vista mit Service Pack 2 und höher.$\nEine Installation auf diesem Betriebssystem wird nicht unterstützt. Installation wird beendet...'
LangString CheckWindowsServPackMB ${LANG_GERMAN} "Es wird empfohlen, das aktuellste Service Pack des Betriebssystems für Firestorm zu verwenden.$\nEs ist hilftreich für Performance und Stabilität des Programms."
LangString UseLatestServPackDP ${LANG_GERMAN} "Bitte Windows Update benutzen, um das aktuellste Service Pack zu installieren."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_GERMAN} "Überprüfung der Installations-Berechtigungen ..."
LangString CheckAdministratorInstMB ${LANG_GERMAN} 'Sie besitzen ungenügende Berechtigungen.$\nSie müssen ein "administrator" sein, um Firestorm installieren zu können.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_GERMAN} "Überprüfung der Entfernungs-Berechtigungen ..."
LangString CheckAdministratorUnInstMB ${LANG_GERMAN} 'Sie besitzen ungenügende Berechtigungen.$\nSie müssen ein "administrator" sein, um Firestorm entfernen zu können..'

; checkcpuflags
LangString MissingSSE2 ${LANG_GERMAN} "Dieser PC besitzt möglicherweise keinen Prozessor mit SSE2-Unterstützung, die für die Ausführung von Firestorm ${VERSION_LONG} benötigt wird. Trotzdem installieren?"

; Extended cpu checks (AVX2)
LangString MissingAVX2 ${LANG_GERMAN} "Ihre CPU unterstützt keine AVX2-Anweisungen. Bitte laden Sie die Version für ältere CPUs von %DLURL%-legacy-cpus/ herunter."
LangString AVX2Available ${LANG_GERMAN} "Ihre CPU unterstützt AVX2-Anweisungen. Sie können die für AVX2 optimierte Version für eine bessere Leistung unter %DLURL%/ herunterladen. Möchten Sie sie jetzt herunterladen?"
LangString AVX2OverrideConfirmation ${LANG_GERMAN} "Falls Sie glauben, dass Ihr PC AVX2-Anweisungen unterstützt, können Sie die Installation dennoch durchführen. Möchten Sie fortfahren?"
LangString AVX2OverrideNote ${LANG_GERMAN} "Durch das Übersteuern des Installers installieren Sie möglicherweise eine Version, die beim Starten direkt abstürzt. In diesem Fall installieren Sie bitte stattdessen die Standard-Version."

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_GERMAN} "Warten auf die Beendigung von Firestorm ..."
LangString CloseSecondLifeInstMB ${LANG_GERMAN} "Firestorm kann nicht installiert oder ersetzt werden, wenn es bereits läuft.$\n$\nBeenden Sie, was Sie gerade tun und klicken Sie OK, um Firestorm zu beenden.$\nKlicken Sie ABBRECHEN, um die Installation abzubrechen."
LangString CloseSecondLifeInstRM ${LANG_GERMAN} "Firestorm konnte einige Dateien einer vorherigen Installation nicht entfernen."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_GERMAN} "Warten auf die Beendigung von Firestorm ..."
LangString CloseSecondLifeUnInstMB ${LANG_GERMAN} "Firestorm kann nicht entfernt werden, wenn es bereits läuft.$\n$\nBeenden Sie, was Sie gerade tun und klicken Sie OK, um Firestorm zu beenden.$\nKlicken Sie CANCEL, um abzubrechen."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_GERMAN} "Prüfe Netzwerkverbindung..."

; error during installation
LangString ErrorSecondLifeInstallRetry ${LANG_GERMAN} "Firestorm konnte nicht korrekt installiert werden, einige Dateien wurden eventuell nicht korrekt von der Installationroutine kopiert."
LangString ErrorSecondLifeInstallSupport ${LANG_GERMAN} "Bitte laden Sie den Viewer erneut von https://www.firestormviewer.org/downloads/ und versuchen Sie die Installation erneut. Sollte das Problem weiterhin bestehen, dann kontaktieren Sie unseren Support unter https://www.firestormviewer.org/support/."

; ask to remove user's data files
LangString RemoveDataFilesMB ${LANG_GERMAN} "Einstellungs- und Cache-Dateien in Dokumente und Einstellungen löschen?"

; delete program files
LangString DeleteProgramFilesMB ${LANG_GERMAN} "Es existieren weiterhin Dateien in Ihrem Firestorm Programm-Ordner.$\n$\nDies sind möglicherweise Dateien, die sie modifiziert oder bewegt haben:$\n$INSTDIR$\n$\nMöchten Sie diese ebenfalls löschen?"

; uninstall text
LangString UninstallTextMsg ${LANG_GERMAN} "Dies wird Firestorm ${VERSION_LONG} von Ihrem System entfernen."

; ask to remove protocol handler registry entries registry keys that still might be needed by other viewers that are installed
LangString DeleteRegistryKeysMB ${LANG_GERMAN} "Möchten Sie Firestorm als Standardverknüpfung zum Öffnen von Virtuelle-Welten-Protokollen entfernen?$\n$\nEs wird empfohlen, diese zu behalten, falls Sie noch andere Versionen von Firestorm installiert haben."

; <FS:Ansariel> Ask to create protocol handler registry entries
LangString CreateUrlRegistryEntries ${LANG_GERMAN} "Möchten Sie Firestorm als Standardverknüpfung zum Öffnen von Virtuelle-Welten-Protokollen festlegen?$\n$\nFalls Sie andere Versionen von Firestorm installiert haben, werden bereits bestehenden Verknüpfungen überschrieben."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_GERMAN} "Eintrag im Startmenü erstellen?"

; <FS:Ansariel> Application name suffix for OpenSim variant
LangString ForOpenSimSuffix ${LANG_GERMAN} "für OpenSimulator"

LangString DeleteDocumentAndSettingsDP ${LANG_GERMAN} 'Dateien unterhalb von "Dokumente und Einstellungen werden gelöscht.'
LangString UnChatlogsNoticeMB ${LANG_GERMAN} "Diese Deinstallation löscht NICHT die Firestorm-Chatprotokolle und andere private Dateien. Sollen diese gelöscht werden, muss das Firestorm-Verzeichnis im Anwendungsdaten-Verzeichnis manuell gelöscht werden."
LangString UnRemovePasswordsDP ${LANG_GERMAN} "Lösche gespeicherte Firestorm-Passwörter."

LangString MUI_TEXT_LICENSE_TITLE ${LANG_GERMAN} "VivoxVoice System Lizenz-Vereinbarung"
LangString MUI_TEXT_LICENSE_SUBTITLE ${LANG_GERMAN} "Zusätzliche Lizenz-Vereinbarung für die Vivox-Voice-System-Komponenten."
LangString MUI_INNERTEXT_LICENSE_TOP ${LANG_GERMAN} "Bitte lesen Sie die folgende Lizenz-Vereinbarung aufmerksam durch, bevor Sie mit der Installation fortfahren:"
LangString MUI_INNERTEXT_LICENSE_BOTTOM ${LANG_GERMAN} "Sie müssen den Lizenzbestimmungen zustimmen, um mit der Installation fortfahren zu können."
