; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\TradChinese.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_TRADCHINESE} "安裝語言"
LangString SelectInstallerLanguage  ${LANG_TRADCHINESE} "請選擇安裝時使用的語言。"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_TRADCHINESE} "更新"
LangString LicenseSubTitleSetup ${LANG_TRADCHINESE} "設置"

LangString MULTIUSER_TEXT_INSTALLMODE_TITLE ${LANG_TRADCHINESE} "安裝模式"
LangString MULTIUSER_TEXT_INSTALLMODE_SUBTITLE ${LANG_TRADCHINESE} "為所有使用者安裝（需要管理員權限）或僅為當前使用者安裝？"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_TOP ${LANG_TRADCHINESE} "使用管理員權限執行本安裝程式時，你可以選擇是否安裝到例如 c:\Program Files 或當前使用者的 AppData\Local 等資料夾。"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS ${LANG_TRADCHINESE} "為所有使用者安裝"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER ${LANG_TRADCHINESE} "僅為當前使用者安裝"

; installation directory text
LangString DirectoryChooseTitle ${LANG_TRADCHINESE} "安裝目錄"
LangString DirectoryChooseUpdate ${LANG_TRADCHINESE} "請選擇 Firestorm 的安裝目錄，以便於將軟體更新成 ${VERSION_LONG} 版本（XXX）:"
LangString DirectoryChooseSetup ${LANG_TRADCHINESE} "請選擇安裝 Firestorm 的目錄："

LangString MUI_TEXT_DIRECTORY_TITLE ${LANG_TRADCHINESE} "安裝目錄"
LangString MUI_TEXT_DIRECTORY_SUBTITLE ${LANG_TRADCHINESE} "選擇要安裝 Firestorm 的目錄："

LangString MUI_TEXT_INSTALLING_TITLE ${LANG_TRADCHINESE} "正在安裝 Firestorm…"
LangString MUI_TEXT_INSTALLING_SUBTITLE ${LANG_TRADCHINESE} "正在安裝 Firestorm 瀏覽器到 $INSTDIR"

LangString MUI_TEXT_FINISH_TITLE ${LANG_TRADCHINESE} "正在安裝 Firestorm…"
LangString MUI_TEXT_FINISH_SUBTITLE ${LANG_TRADCHINESE} "已安裝 Firestorm 瀏覽器到 $INSTDIR"

LangString MUI_TEXT_ABORT_TITLE ${LANG_TRADCHINESE} "安裝過程中止"
LangString MUI_TEXT_ABORT_SUBTITLE ${LANG_TRADCHINESE} "將上安裝 Firestorm 瀏覽器到 $INSTDIR。"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_TRADCHINESE} "找不到 '$INSTNAME' 程式。自動更新失敗。"

; installation success dialog
LangString InstSuccesssQuestion ${LANG_TRADCHINESE} "現在要啟動 Firestorm 嗎？"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_TRADCHINESE} "檢查是否在使用舊版本…"

; check windows version
LangString CheckWindowsVersionDP ${LANG_TRADCHINESE} "檢查 Windows 版本…"
LangString CheckWindowsVersionMB ${LANG_TRADCHINESE} "Firestorm 只支援 Windows Vista SP2。"
LangString CheckWindowsServPackMB ${LANG_TRADCHINESE} "建議在操作系統的最新服務包上運行 Firestorm。$\n這將有助於提高程式的效能和穩定性。"
LangString UseLatestServPackDP ${LANG_TRADCHINESE} "請使用 Windows 更新安裝最新的服務包。"

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_TRADCHINESE} "檢查安裝所需的權限..."
LangString CheckAdministratorInstMB ${LANG_TRADCHINESE} "您的帳戶似乎是「受限的帳戶」。$\n您必須有「管理員」權限才可以安裝 Firestorm。"

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_TRADCHINESE} "檢查卸載所需的權限..."
LangString CheckAdministratorUnInstMB ${LANG_TRADCHINESE} "您的帳戶似乎是「受限的帳戶」。$\n您必須有「管理員」權限才可以卸載 Firestorm。"

; checkcpuflags
LangString MissingSSE2 ${LANG_TRADCHINESE} "此電腦可能沒有支援 SSE2 的 CPU，而這是運行 Firestorm ${VERSION_LONG} 所必須的。您要繼續嗎？"

; Extended cpu checks (AVX2)
LangString MissingAVX2 ${LANG_TRADCHINESE} "您的 CPU 不支援 AVX2 指令。請從 %DLURL%-legacy-cpus/ 下載舊版 CPU 版本。"
LangString AVX2Available ${LANG_TRADCHINESE} "您的 CPU 支援 AVX2 指令。您可以從 %DLURL% 下載 AVX2 優化版本以獲得更好的效能。要立即下載嗎？"
LangString AVX2OverrideConfirmation ${LANG_TRADCHINESE} "如果您認為您的電腦可以支援AVX2優化，您仍然可以繼續安裝。是否要繼續？"
LangString AVX2OverrideNote ${LANG_TRADCHINESE} "透過覆蓋安裝程式，您將安裝一個可能會在啟動後立即崩潰的版本。如果發生此情況，請立即安裝標準CPU版本。"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_TRADCHINESE} "等待 Firestorm 停止運行…"
LangString CloseSecondLifeInstMB ${LANG_TRADCHINESE} "如果 Firestorm 仍在運行，將無法進行安裝。$\n$\n請結束您在 Firestorm 內的活動，然後選擇確定，將 Firestorm 關閉，以繼續安裝。$\n選擇「取消」，取消安裝。"
LangString CloseSecondLifeInstRM ${LANG_TRADCHINESE} "Firestorm 未能刪除某些先前安裝的文件。"

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_TRADCHINESE} "等待 Firestorm 停止運行…"
LangString CloseSecondLifeUnInstMB ${LANG_TRADCHINESE} "如果 Firestorm 仍在運行，將無法進行卸載。$\n$\n請結束您在 Firestorm 內的活動，然後選擇確定，將 Firestorm 關閉，以繼續卸載。$\n選擇「取消」，取消卸載。"

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_TRADCHINESE} "正在檢查網路連接..."

; error during installation
LangString ErrorSecondLifeInstallRetry ${LANG_TRADCHINESE} "Firestorm 安裝程式在安裝過程中遇到問題。某些文件可能沒有正確複製。"
LangString ErrorSecondLifeInstallSupport ${LANG_TRADCHINESE} "請從 https://www.firestormviewer.org/downloads/ 重新安裝查看器，如果重新安裝後問題仍然存在，請訪問 https://www.firestormviewer.org/support/ 尋求支援。"

; ask to remove user's data files
LangString RemoveDataFilesMB ${LANG_TRADCHINESE} "刪除文檔和設置文件夾中的暫存文件？"

; delete program files
LangString DeleteProgramFilesMB ${LANG_TRADCHINESE} "在您的 Firestorm 程式目錄裡仍存有一些文件。$\n$\n這些文件可能是您新建或移動到 $\n$INSTDIR 文件夾中的。$\n $\n您還想要加以刪除嗎？"

; uninstall text
LangString UninstallTextMsg ${LANG_TRADCHINESE} "將從您的系統中卸載 Firestorm ${VERSION_LONG}。"

; ask to remove protocol handler registry entries registry keys that still might be needed by other viewers that are installed
LangString DeleteRegistryKeysMB ${LANG_TRADCHINESE} "您想要移除 Firestorm 作為虛擬世界協議的默認處理程序嗎？$\n$\n如果您安裝了其他版本的 Firestorm，建議保留註冊表。"

; <FS:Ansariel> Ask to create protocol handler registry entries
LangString CreateUrlRegistryEntries ${LANG_TRADCHINESE} "您想要將 Firestorm 註冊為虛擬世界協議的默認處理程序嗎？$\n$\n如果您安裝了其他版本的 Firestorm，這將覆蓋現有的註冊表。"

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_TRADCHINESE} "在開始菜單中創建一個條目？"

; <FS:Ansariel> Application name suffix for OpenSim variant
LangString ForOpenSimSuffix ${LANG_TRADCHINESE} "for OpenSimulator"

LangString DeleteDocumentAndSettingsDP ${LANG_TRADCHINESE} "正在刪除文檔和設置文件夾中的文件。"
LangString UnChatlogsNoticeMB ${LANG_TRADCHINESE} "此卸載不會刪除您的 Firestorm 聊天記錄和其他私人文件。如果您想自行刪除，請刪除用戶應用程式數據文件夾中的 Firestorm 文件夾。"
LangString UnRemovePasswordsDP ${LANG_TRADCHINESE} "正在移除 Firestorm 保存的密碼。"

LangString MUI_TEXT_LICENSE_TITLE ${LANG_TRADCHINESE} "Vivox 語音系統許可協議"
LangString MUI_TEXT_LICENSE_SUBTITLE ${LANG_TRADCHINESE} "Vivox 語音系統組件的附加許可協議。"
LangString MUI_INNERTEXT_LICENSE_TOP ${LANG_TRADCHINESE} "在繼續安裝之前，請仔細閱讀以下許可協議："
LangString MUI_INNERTEXT_LICENSE_BOTTOM ${LANG_TRADCHINESE} "您必須同意許可條款才能繼續安裝。"