;;
;;  german.nsh
;;
;;  German language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Bjoern Voigt <bjoern@cs.tu-berlin.de>, 2003.
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Die GTK+ Runtime Umgebung ist entweder nicht vorhanden oder sollte aktualisiert werden.$\rBitte installieren Sie v${GTK_VERSION} oder h�her der GTK+ Runtime"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (erforderlich)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Umgebung (erforderlich)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Themen"
!define GTK_NOTHEME_SECTION_TITLE		"Kein Thema"
!define GTK_WIMP_SECTION_TITLE		"Wimp Thema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve Thema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue Thema"
!define GAIM_SECTION_DESCRIPTION		"Gaim Basis-Dateien und -DLLs"
!define GTK_SECTION_DESCRIPTION		"Ein Multi-Plattform GUI Toolkit, verwendet von Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ Themen k�nnen Aussehen und Bedienung von GTK+ Anwendungen ver�ndern."
!define GTK_NO_THEME_DESC			"Installiere kein GTK+ Thema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows Imitator) ist ein GTK Theme, da� sich besonders gut in den Windows Desktop integriert."
!define GTK_BLUECURVE_THEME_DESC		"Das Bluecurve Thema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Das Lighthouseblue Thema."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Eine alte Version der GTK+ Runtime wurde gefunden. M�chten Sie aktualisieren?$\rHinweis: Gaim funktioniert evtl. nicht, wenn Sie nicht aktualisieren."

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (nur entfernen)"
!define GAIM_PROMPT_WIPEOUT			"Ihre altes Gaim-Verzeichnis soll gel�scht werden. M�chten Sie fortfahren?$\r$\rHinweis: Alle nicht-Standard Plugins, die Sie evtl. installiert haben werden$\rgel�scht. Gaim-Benutzereinstellungen sind nicht betroffen."
!define GAIM_PROMPT_DIR_EXISTS		"Das Installationsverzeichnis, da� Sie angegeben haben, existiert schon. Der Verzeichnisinhalt$\rwird gel�scht. M�chten Sie fortfahren?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fehler beim Installieren der GTK+ Runtime."
!define GTK_BAD_INSTALL_PATH			"Der Pfad, den Sie eingegeben haben, existiert nicht und kann nicht erstellt werden."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Sie haben keine Berechtigung, um ein GTK+ Theme zu installieren."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Der Deinstaller konnte keine Registrierungsshl�ssel f�r Gaim finden.$\rEs ist wahrscheinlich, da� ein anderer Benutzer diese Anwendunng installiert hat."
!define un.GAIM_UNINSTALL_ERROR_2         "Sie haben keine Berechtigung, diese Anwendung zu deinstallieren."
