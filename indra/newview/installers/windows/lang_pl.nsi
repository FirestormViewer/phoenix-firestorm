; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Polish.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_POLISH} "Język instalatora"
LangString SelectInstallerLanguage  ${LANG_POLISH} "Proszę wybrać język instalatora"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_POLISH} " - Aktualizacja"
LangString LicenseSubTitleSetup ${LANG_POLISH} " - Instalacja"

LangString MULTIUSER_TEXT_INSTALLMODE_TITLE ${LANG_POLISH} "Tryb instalacji"
LangString MULTIUSER_TEXT_INSTALLMODE_SUBTITLE ${LANG_POLISH} "Instalować dla wszystkich użytkowników (wymaga administratora) czy tylko dla bieżącego?"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_TOP ${LANG_POLISH} "Po uruchomieniu tego instalatora z uprawnieniami administratora można wybrać instalację w (np.) C:\Program Files lub w folderze AppData\Local bieżącego użytkownika."
LangString MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS ${LANG_POLISH} "Instaluj dla wszystkich użytkowników"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER ${LANG_POLISH} "Instaluj tylko dla bieżącego użytkownika"

; installation directory text
LangString DirectoryChooseTitle ${LANG_POLISH} "Katalog instalacji" 
LangString DirectoryChooseUpdate ${LANG_POLISH} "Wybierz katalog Firestorma, aby uaktualnić do wersji ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_POLISH} "Wybierz gdzie zainstalować Firestorma:"

LangString MUI_TEXT_DIRECTORY_TITLE ${LANG_POLISH} "Katalog instalacyjny"
LangString MUI_TEXT_DIRECTORY_SUBTITLE ${LANG_POLISH} "Wybierz katalog, w którym chcesz zainstalować Firestorma:"

LangString MUI_TEXT_INSTALLING_TITLE ${LANG_POLISH} "Instalowanie Firestorma..."
LangString MUI_TEXT_INSTALLING_SUBTITLE ${LANG_POLISH} "Instalowanie przeglądarki Firestorm w $INSTDIR"

LangString MUI_TEXT_FINISH_TITLE ${LANG_POLISH} "Instalowanie Firestorma"
LangString MUI_TEXT_FINISH_SUBTITLE ${LANG_POLISH} "Zainstalowano przeglądarkę Firestorm w $INSTDIR."

LangString MUI_TEXT_ABORT_TITLE ${LANG_POLISH} "Instalacja przerwana"
LangString MUI_TEXT_ABORT_SUBTITLE ${LANG_POLISH} "Przeglądarka Firestorm nie została zainstalowana w $INSTDIR."

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_POLISH} "Nie można odnaleźć programu '$INSTNAME'. Cicha aktualizacja zakończyła się niepowodzeniem."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_POLISH} "Czy uruchomić teraz Firestorma?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_POLISH} "Poszukiwanie starszej wersji..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_POLISH} "Sprawdzanie wersji Windows..."
LangString CheckWindowsVersionMB ${LANG_POLISH} "Firestorm obsługuje tylko Windows Vista z Service Pack 2 lub nowszym.$\nInstalacja na obecnym systemie operacyjnym nie jest wspierana."
LangString CheckWindowsServPackMB ${LANG_POLISH} "Zalecane jest uruchamianie Firestorma z najnowszym dostępnym Service Packiem zainstalowanym w systemie.$\nPomaga on w podniesieniu wydajności i stabilności programu."
LangString UseLatestServPackDP ${LANG_POLISH} "Użyj usługi Windows Update, aby zainstalować najnowszy Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_POLISH} "Sprawdzam uprawnienia do instalacji..."
LangString CheckAdministratorInstMB ${LANG_POLISH} 'Używasz konta z ograniczeniami.$\nMusisz być zalogowany jako "administrator" aby zainstalować Firestorma.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_POLISH} "Sprawdzam uprawnienia do dezinstalacji..."
LangString CheckAdministratorUnInstMB ${LANG_POLISH} 'Używasz konta z ograniczeniami.$\nMusisz być być zalogowany jako "administrator" aby odinstalować Firestorma.'

; checkcpuflags
LangString MissingSSE2 ${LANG_POLISH} "Ten komputer może nie mieć procesora z obsługą SSE2, który jest wymagany aby uruchomić Firestorma w wersji ${VERSION_LONG}. Chcesz kontynuować?"

; Extended cpu checks (AVX2)
LangString MissingAVX2 ${LANG_POLISH} "Twój procesor nie obsługuje instrukcji AVX2. Pobierz wersję dla starszych procesorów z: %DLURL%-legacy-cpus/"
LangString AVX2Available ${LANG_POLISH} "Twój procesor obsługuje instrukcje AVX2. Możesz pobrać zoptymalizowaną wersję AVX2, aby uzyskać lepszą wydajność z: %DLURL%/. Czy chcesz pobrać ją teraz?"
LangString AVX2OverrideConfirmation ${LANG_POLISH} "Jeśli uważasz, że Twój komputer obsługuje optymalizację AVX2, możesz zainstalować mimo to. Czy chcesz kontynuować?"
LangString AVX2OverrideNote ${LANG_POLISH} "Zamierzasz zainstalować wersję, która może ulec awarii natychmiast po uruchomieniu. Jeśli tak się stanie zainstaluj wersję dla standardowych CPU."

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_POLISH} "Oczekiwanie na zamknięcie Firestorma..."
LangString CloseSecondLifeInstMB ${LANG_POLISH} "Firestorm nie może zostać zainstalowany, gdy jest już włączony.$\n$\nZakończ to, co obecnie robisz i wybierz OK aby zamknąć Firestorma i kontynuować.$\nWybierz ANULUJ aby anulować instalację."
LangString CloseSecondLifeInstRM ${LANG_POLISH} "Firestorm failed to remove some files from a previous install."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_POLISH} "Oczekiwanie na zamknięcie Firestorma..."
LangString CloseSecondLifeUnInstMB ${LANG_POLISH} "Firestorm nie może zostać odinstalowany, gdy jest włączony.$\n$\nZakończ to, co obecnie robisz i wybierz OK aby zamknąć Firestorma i kontynuować.$\nWybierz ANULUJ aby anulować."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_POLISH} "Sprawdzanie połączenia sieciowego..."

; error during installation
LangString ErrorSecondLifeInstallRetry ${LANG_POLISH} "Instalator Firestorma napotkał problemy podczas instalacji. Niektóre pliki mogły nie zostać poprawnie skopiowane."
LangString ErrorSecondLifeInstallSupport ${LANG_POLISH} "Zainstaluj ponownie przeglądarkę z https://www.firestormviewer.org/downloads/ i skontaktuj się z https://www.firestormviewer.org/support/ jeśli problem będzie się powtarzał."

; ask to remove user's data files
LangString RemoveDataFilesMB ${LANG_POLISH} "Usunąć ustawienia i pliki pamięci podręcznej (cache) z folderu Documents and Settings?"

; delete program files
LangString DeleteProgramFilesMB ${LANG_POLISH} "Nadal istnieją pliki w katalogu instalacyjnym Firestorma.$\n$\nMożliwe, że są to pliki, które stworzyłeś/aś lub przeniosłeś/aś do:$\n$INSTDIR$\n$\nCzy chcesz je usunąć?"

; uninstall text
LangString UninstallTextMsg ${LANG_POLISH} "To spowoduje odinstalowanie Firestorma ${VERSION_LONG} z Twojego systemu."

; ask to remove protocol handler registry entries registry keys that still might be needed by other viewers that are installed
LangString DeleteRegistryKeysMB ${LANG_POLISH} "Czy chcesz wyrejestrować Firestorma jako domyślny program do obsługi protokołów wirtualnych światów?$\n$\nZalecane jest, aby zachować te klucze, jeśli wciąż są zainstalowane inne wersje Firestorma."

; <FS:Ansariel> Ask to create protocol handler registry entries
LangString CreateUrlRegistryEntries ${LANG_POLISH} "Czy chcesz zarejestrować Firestorma jako domyślny program do obsługi protokołów wirtualnych światów?$\n$\nJeśli masz zainstalowane inne wersje Firestorma, to spowoduje to zastąpienie istniejących kluczy rejestru."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_POLISH} "Utworzyć skrót w Menu Start?"

; <FS:Ansariel> Application name suffix for OpenSim variant
LangString ForOpenSimSuffix ${LANG_POLISH} "dla OpenSimulatora"

LangString DeleteDocumentAndSettingsDP ${LANG_POLISH} "Usuwanie plików z katalogu Documents and Settings."
LangString UnChatlogsNoticeMB ${LANG_POLISH} "Ta dezinstalacja NIE usunie logów czatów Firestorma ani innych prywatnych plików. Jeśli chcesz zrobić to samodzielnie, to usuń katalog Firestorm wewnątrz folderu Dane Aplikacji użytkownika."
LangString UnRemovePasswordsDP ${LANG_POLISH} "Usuwanie zapisanych przez Firestorma haseł."

LangString MUI_TEXT_LICENSE_TITLE ${LANG_POLISH} "Umowa licencyjna Vivox Voice System"
LangString MUI_TEXT_LICENSE_SUBTITLE ${LANG_POLISH} "Dodatkowa umowa licencyjna na komponenty Vivox Voice System."
LangString MUI_INNERTEXT_LICENSE_TOP ${LANG_POLISH} "Przed przystąpieniem do instalacji uważnie przeczytaj poniższą umowę licencyjną:"
LangString MUI_INNERTEXT_LICENSE_BOTTOM ${LANG_POLISH} "Musisz zaakceptować warunki licencji, aby kontynuować instalację."
