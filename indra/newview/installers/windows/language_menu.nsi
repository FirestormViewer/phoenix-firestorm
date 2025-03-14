# language_menu.nsi
# Prepare for language selection menu with native language names
# In a separate file so the main script can remain ASCII text, whereas this
# file can be UTF-16LE

# Order doesn't appear to matter, NSIS sorts the menu by itself.
Push ${LANG_ENGLISH}
Push "English"
##Push ${LANG_DANISH}
##Push "Dansk"
Push ${LANG_GERMAN}
Push "Deutsch"
Push ${LANG_SPANISH}
Push "Español"
Push ${LANG_FRENCH}
Push "Français"
Push ${LANG_ITALIAN}
Push "Italiano"
Push ${LANG_POLISH}
Push "Polski"
##Push ${LANG_PORTUGUESEBR}
##Push "Português do Brasil"
Push ${LANG_JAPANESE}
Push "日本語"
Push ${LANG_TRADCHINESE}
Push "中文（正體）"
Push ${LANG_RUSSIAN}
Push "Русский"
##Push ${LANG_TURKISH}
##Push "Türkçe"
