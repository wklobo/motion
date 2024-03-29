//***************************************************************************//
//*                                                                         *//
//* File:          fifomotion.c                                             *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2014-08-23                                               *//
//* Last change:   2022-11-28 - 16:25:10                                    *//
//* Description:   Weiterverarbeitung von 'motion'-Dateien:                 *//
//*                kopieren auf einen anderen Rechner                       *//
//*                                                                         *//
//* Copyright (C) 2014-21 by Wolfgang Keuch                                 *//
//*                                                                         *//
//* Kompilation:                                                            *//
//*    gcc -o Fifo fifomotion.c                                             *//
//*    make fifo                                                            *//
//*                                                                         *//
//* Aufruf:                                                                 *//
//*    ./FifoMotion &    (Daemon)                                           *//
//*                                                                         *//
//***************************************************************************//

#define _MODUL0
#define __FIFOMOTION_MYLOG__         false
#define __FIFOMOTION_DEBUG_INIT__     true
#define __FIFOMOTION_MYLOG1__        false
#define __FIFOMOTION_DEBUG__         false
#define __FIFOMOTION_DEBUG__c__      false     // calcSize
#define __FIFOMOTION_DEBUG__t__      false     // FileTransfer
#define __FIFOMOTION_DEBUG__d__      false     // delOldest

// Peripherie aktivieren
// ----------------------
#define __LCD_DISPLAY__    false   /* LCD-Display               */
#define __DS18B20__        false   /* DS18B20-Sensoren          */
#define __INTERNAL__       true    /* CPU-Temperatur            */
#define __BME280__         false   /* BME280-Sensor             */
#define __TSL2561__        false   /* TLS2561-Sensor            */
#define __GPIO__           true    /* GPIOs �ber 'gpio.c'       */
#define __FIFO__           true    /* named pipe(Fifo)          */
#define __MQTT__           false   /* Mosquitto                 */
#define __INTERRUPT__      false   /* Interrupt Geigerz�hler    */
#define __DATENBANK__      false   /* Datenbank                 */


//***************************************************************************//


#include "./version.h"
#include "./fifomotion.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>


#include "/home/pi/treiber/common/gpio.h"
#include "/home/pi/treiber/common/error.h"
#include "/home/pi/treiber/common/common.h"
#include "/home/pi/treiber/common/datetime.h"
#include "/home/pi/treiber/sendmail/sendMail.h"

#define FLAGS   O_WRONLY|O_CREAT|O_EXCL
#define MODE    S_IRWXU|S_IRWXG|S_IROTH
#define MODUS  0774


// statische Variable
// ----------------
static time_t ErrorFlag = 0;          // Steuerung rote LED

static int newFiles   = 0;            // neue Dateien
static int newFolders = 0;            // neue Verzeichnisse
static int delFiles   = 0;            // gel�schte Dateien
static int delFolders = 0;            // gel�schte Verzeichnisse

#define BRENNDAUER     18             /* [sec]...rote Fehler-LED                    */

// programmweite Variable
// -----------------------
char   s_Hostname[ZEILE];                       // der Name dieses Rechners
char   s_meineIPAdr[NOTIZ];                     // die IP-Adresse dieses Rechners
uint   s_IPnmr=0;                               // letzte Stelle der IP-Adresse
long   s_pid=0;                                 // meine Prozess-ID

#define SNAPSHOT         "/%s/lastsnap.jpg"
char   Snapshot[ZEILE];													// Schnappschuss-Datei


// Verzeichnisse
// --------------
#define _FOLDER       "event_"                  /* Kennzeichnung Verzeichnis */
#define _EVENT_       "Event_"                  /* Kennzeichnung Verzeichnis */
#define FIFO          AUXDIR"/MOTION.FIFO"
#define SOURCE        PIXDIR/                   /* "/home/pi/motion/pix/" */
#define TEMP          TOPDIR"/tmp/"
#define DESTINATION   "/media/Kamera/Vogel/"    /* = 'DISKSTATION/surveillance'/... */


//***************************************************************************//

// main()
// ------
#if __FIFOMOTION_DEBUG__
 #define DEBUG(...)  printf(__VA_ARGS__)
 #define BREAKmain   true /* alle Dateien bearbeitet */
 #define SYSLOG(...)  syslog(__VA_ARGS__)
#else
 #define DEBUG(...)
 #define SYSLOG(...)
#endif

#if __FIFOMOTION_DEBUG_INIT__
  // nur die Init-Phase
  // -------------------
  #define _DEBUG(...) printf(__VA_ARGS__)
#else
  #define _DEBUG(...)
#endif


#if __FIFOMOTION_DEBUG__1__
#define DEBUG_1(...)  printf(__VA_ARGS__)
#else
#define DEBUG_1(...)
#endif

#if __FIFOMOTION_DEBUG__c__
#define DEBUG_c(...)  printf(__VA_ARGS__)
#else
#define DEBUG_c(...)
#endif

// FileTransfer
// -------------
#if __FIFOMOTION_DEBUG__t__
#define DEBUG_t(...)  printf(__VA_ARGS__)
#define BREAK_FileTransfer1   false   /* FileTransfer  */
#define BREAK_FileTransfer2   false   /* FileTransfer  */
#else
#define DEBUG_t(...)
#endif

// delOldest
// ---------
#if __FIFOMOTION_DEBUG__d__
#define DEBUG_d(...)  printf(__VA_ARGS__)
#define BREAK_delOldest1   false      /* delOldest  */
#define BREAK_delOldest2   false      /* delOldest  */
#else
#define DEBUG_d(...)
#endif

//***********************************************************************************************//


//***********************************************************************************************//

// fataler Fehler
// ------------------------
// f�gt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_Error( char* Message, const char* Func, int Zeile)
{
  int  errsave = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  sprintf( Fehler, "%s - Err %d-%s", Message,  errsave, strerror( errsave));
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  printf("    -- Fehler -->  %s\n", ErrText);   // lokale Fehlerausgabe
  digitalWrite (M_LED_GELB,   LED_AUS);
  digitalWrite (M_LED_GRUEN,  LED_AUS);
  digitalWrite (M_LED_BLAU,   LED_AUS);
  digitalWrite (M_LED_ROT,    LED_EIN);

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<< %s: Exit!",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // PID-Datei wieder l�schen
  // ------------------------
  killPID();

  finish_with_Error(ErrText);                   // Fehlermeldung ausgeben
}
//***********************************************************************************************

 // nicht-fataler Fehler
 // ------------------------
 // Lokale Fehlerbearbeitung
 // Fehler wird nur geloggt
int Error_NonFatal( char* Message, const char* Func, int Zeile)
{
  int  errsave = errno;                         // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  if ( errsave == 0)
    sprintf( Fehler, "%s", Message);            // kein Fehler, nur Meldung
  else
    sprintf( Fehler, "%s - Err %d-%s", Message,  errsave, strerror( errsave));

  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  DEBUG("   -- Fehler -->  %s\n", ErrText);     // lokale Fehlerausgabe

  digitalWrite (M_LED_ROT,    LED_EIN);
  ErrorFlag = time(0) + BRENNDAUER;             // Steuerung rote LED

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<*** %s",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  report_Error(ErrText, true);                	// Fehlermeldung immer mit Mail ausgeben   

//  if ( errsave == 24)                           // 'too many open files' ...
//    report_Error(ErrText, true);                // Fehlermeldung mit Mail ausgeben   
//  else
//    report_Error(ErrText, false);               // Fehlermeldung ohne Mail ausgeben

  return  errsave;
}
//***********************************************************************************************

// Protokoll gel�schte Dateien schreiben
// --------------------------------------
int Deleted(const char* ItemName)
{
  DEBUG_d("=======> %s()#%d: %s('%s')\n", __FUNCTION__, __LINE__, __FUNCTION__, ItemName);
  int status = 1;
	Startzeit(T_PRINT);                     								// Zeitmessung starten

  { // --- Log-Ausgabe --------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "    gel�scht: '%s'",  ItemName);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

	status = (int)Zwischenzeit(T_PRINT);
  DEBUG_d("<------- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// Protokoll neue Dateien schreiben
// --------------------------------
int Added(const char* ItemName)
{
  DEBUG_d("=======> %s()#%d: %s('%s')\n", __FUNCTION__, __LINE__, __FUNCTION__, ItemName);
  int status = 1;
	Startzeit(T_PRINT);                     								// Zeitmessung starten

  { // --- Log-Ausgabe --------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "           neu: '%s'",  ItemName);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

	status = (int)Zwischenzeit(T_PRINT);
  DEBUG_d("<------- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// aus Dateinamen einen Datums-Verzeichnisnamen erstellen
// ------------------------------------------------------
// R�ckgabe: FolderName 'YYYY-MM-DD'

int makeDatumsFoldername(const time_t Zeit, char* FolderName)
{
  DEBUG_t("===> %s()#%d:  %s('%ld',\n"\
         "                                                   ----> '%s')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Zeit, FolderName);
  int status = 0;
  struct tm *tmnow;
  tmnow = localtime(&Zeit);
  sprintf(FolderName, "%04d-%02d-%02d",
                      tmnow->tm_year + 1900, tmnow->tm_mon + 1, tmnow->tm_mday);

  { // --- Debug-Ausgaben ------------------------------------------
    DEBUG_t("     %s()#%d: Name '%s' erstellt!\n",
                         __FUNCTION__, __LINE__, FolderName);
  } // --------------------------------------------------------------

  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// aus Dateinamen einen Event-Verzeichnisnamen erstellen
// ------------------------------------------------------------
//  '.../09-20210107_132755.mkv' --> 'Kamera_Event-09'

int makeEventFoldername(const char* FileName, const char* Vorspann, char* FolderName)
{
  DEBUG_t("===> %s()#%d:  %s('%s',\n"\
         "                                                       '%s',\n"\
         "                                                 ----> '%s')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, FileName, Vorspann, FolderName);

  strcat(FolderName, Vorspann);
  strcat(FolderName, "_");
  strcat(FolderName, "Event-");
  char* beg = strrchr(FileName, '/');           // Event-Markierung suchen ...
  strncat(FolderName, beg+1, 2);                // Event-Nummer
  strcat(FolderName, "\0");

  { // --- Debug-Ausgaben ------------------------------------------
    DEBUG_t("     %s()#%d: Name '%s' erstellt!\n",
                         __FUNCTION__, __LINE__, FolderName);
  } // --------------------------------------------------------------

  int retval = 1;
  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//***********************************************************************************************

// Verzeichnis erstellen
// ----------------------
// neues Verzeichnis erstellen, wenn noch nicht vorhanden
int makeFolder(const char* Folder)
{
  DEBUG_t("===> %s()#%d: %s(%s)\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Folder);
  int errsave = 0;
  char ErrText[ERRBUFLEN];
  int verz = mkdir(Folder, MODUS);              // Datumsverzeichnis erstellen
  if (verz != 0)
  {  // -- Error
    errsave = errno;
    if (errsave != EEXIST)                      // ... der darf!
    { // -- Error
      sprintf(ErrText, "mkdir('%s')", Folder);
      showMain_Error(ErrText, __FUNCTION__, __LINE__);
    }
  }

  // --- Debug-Ausgaben ------------------------------------------
  if (errsave == 0)
  {
    DEBUG_t("     %s()#%d: Verzeichnis '%s' angelegt!\n",
                         __FUNCTION__, __LINE__, Folder);
    char TmpText[NOTIZ];
    sprintf(TmpText, "%s/", Folder);            // als Verzeichnis kennzeichnen
    newFolders += Added(TmpText);             // im Log vermerken
  }
  else
    DEBUG_t("     %s()#%d: Verzeichnis '%s' existiert!\n",
                         __FUNCTION__, __LINE__, Folder);
  // --------------------------------------------------------------


  int retval = (errsave == 0) ? 1 : 0;
  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//***********************************************************************************************

// Datei auf Alter p�fen und ggf. l�schen
// ------------------------------------------
//  Dateiname: ganzer Pfad
//  maxAlter:  Dateialter in Stunden
int remFile(const char* Dateiname, int maxAlter)
{
  DEBUG_d("=====> %s()#%d: %s('%s', '%d')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Dateiname, maxAlter);
  int deleted = 0;                              // Datei-Status
  struct stat Attribut;                         // Attribute der Datei
  char ErrText[ERRBUFLEN];

  if (stat(Dateiname, &Attribut) == -1)         // Attribute auslesen
  { // -- Error
    if (errno != 17)                            // ' Err 17-File exists': falscher Fehler
    {
      sprintf(ErrText, "Error read attribut('%s'): %d", Dateiname, errno);
      return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
    }
  }

  if(Attribut.st_mode & S_IFREG)
  {
    // Regul�re Datei
    // --------------
    time_t FcDatum = Attribut.st_mtime;         // Dateidatum
    long Alter = (time(NULL) - FcDatum);        // Alter der Datei [sec]

    { // --- Debug-Ausgaben ------------------------------------------
      int std = Alter / 3600;
      int min = (Alter % 3600) / 60;
      int sec = ((Alter % 3600) % 60);
      #define MELDUNG   "       %s()#%d: '%s':\n           "\
      "                                   Alter %3d:%02d:%02d Std - max: %d Std\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__,
                                   Dateiname, std, min, sec, maxAlter );
      #undef MELDUNG
      destroyInt(std);
      destroyInt(min);
      destroyInt(sec);
    } // --------------------------------------------------------------

    if (Alter > (maxAlter*3600))
    { // Datei nach Verfalldatum l�schen
      // -------------------------------
      remove(Dateiname);
      { // --- Debug-Ausgaben ------------------------------------------
        DEBUG_d("       %s()#%d: --- '%s' gel�scht!\n",
                                      __FUNCTION__, __LINE__, Dateiname);
      } // --------------------------------------------------------------
      { // --- Log-Ausgabe --------------------------------------------------------
//        char LogText[ZEILE];  sprintf(LogText, "         err: '%d'",  9999);
        char LogText[ZEILE];  sprintf(LogText, "      gel�scht: '%s'",  Dateiname);
        MYLOG(LogText);
      } // ------------------------------------------------------------------------
      delFiles += Deleted(Dateiname);       // im Log vermerken
      deleted++;
    }
  }
  DEBUG_d("<----- %s()#%d -<%d>- \n\n",  __FUNCTION__, __LINE__ , deleted);

  return deleted;
}
//***********************************************************************************************

// Verzeichnis  auf Alter p�fen und ggf. l�schen
// ---------------------------------------------
//  Foldername: ganzer Pfad
//  maxAlter:  Dateialter in Stunden
int remFolder(const char* Foldername, int maxAlter)
{
  DEBUG_d("===> %s()#%d: %s('%s', '%d')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Foldername, maxAlter);

  int deleted = 0;                              // Verzeichnis-Status
  int delFiles= 0;                              // Anzahl der gel�schten Dateien
  struct stat Attribut;                         // Attribute des Verzeichnisses
  char ErrText[ERRBUFLEN];
  char TmpText[ERRBUFLEN];

  // Pr�fung auf Sytemdateien ( '.  ...' usw.)
  // ------------------------------------
  char* nam = strrchr(Foldername, '/')+1;       // letztes Glied im Namen
  if (*nam == '.')
  {
    DEBUG_d("<--- %s()#%d -!<%s>- \n",  __FUNCTION__, __LINE__ , nam);
    return deleted;
  }

  if (stat(Foldername, &Attribut) == -1)        // Attribute auslesen
  {
    if (errno != 17)                            // ' Err 17-File exists': falscher Fehler
    { // -- Error
      #if _DEBUGd
        sprintf(ErrText, "read attribut '%s'", Foldername);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      #else
        return(0);
      #endif
    }
  }

  if(Attribut.st_mode & S_IFDIR)
  {
    // Verzeichnis
    // --------------
    time_t FcDatum = Attribut.st_mtime;       // Dateidatum
    long Alter = (time(NULL) - FcDatum);      // Alter der Datei [sec]

    { // --- Debug-Ausgaben -------------------------------------------
      #define MELDUNG   "     %s()#%d: '%s':\n              "\
      "                       Alter %3ld:%02ld:%02ld Std - max: %d Std\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, Foldername,
                                      Alter / 3600,
                                     (Alter % 3600) / 60,
                                    ((Alter % 3600) % 60), maxAlter );
      #undef MELDUNG
    } // ---------------------------------------------------------------

    if (Alter > (maxAlter*3600))
    { // Verzeichnis hat Verfalldatum erreicht: l�schen
      // ==============================================
      DIR* pdir = opendir(Foldername);
      if (pdir == NULL)
      { // -- Error
        sprintf(ErrText, "Error opendir(%s): %d", Foldername, errno);
        Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
  			DEBUG_d("<--- %s()#%d -<%d>- \n\n",  __FUNCTION__, __LINE__ , 9999);
        { // --- Log-Ausgabe --------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "         err: '%d'",  9999);
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
//        closedir(pdir);
  			return 9999;
      }
      // Verzeichnis muss leer sein: alle enthaltenen Dateien l�schen
      // ------------------------------------------------------------
      struct dirent* pdirzeiger;
      while((pdirzeiger=readdir(pdir)) != NULL)
      {
        if (((*pdirzeiger).d_type) == DT_REG)
        { // regul�re Datei
          // --------------
          char Filename[ZEILE];                       // Pfad der Datei
          sprintf(Filename,"%s/%s", Foldername, (pdirzeiger)->d_name);
          delFiles += remFile(Filename, MAXALTER);
        }
      }
      if (closedir(pdir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", Foldername);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }

      // nun auch noch das Verzeichnis l�schen
      // ------------------------------------
      if (rmdir(Foldername) == 0)
      { // gel�scht!
        deleted = 1;
        sprintf(TmpText, "%s/", Foldername);    // als Verzeichnis kennzeichnen
        delFolders += Deleted(TmpText);         // im Log vermerken
      }
//      else
//      { // -- Error
//        sprintf(ErrText, "rmdir('%s'):"\
//        "              Err %d - '%s'", Foldername, errno, strerror(errno));
//        // keine Fehlerausgabe, kann '..'-Verzeichnis sein
//        // return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
//      }

      if (deleted > 0)
      { // ------------------------------------------------------------
        #define MELDUNG   "     %s()#%d: ------ '%s' gel�scht!\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, Foldername);
        #undef MELDUNG
      } // ------------------------------------------------------------
    }
  }
  int retval = FFAKTOR*delFiles + deleted; // um beide Werte zu�ckzugeben k�nnen

  DEBUG_d("<--- %s()#%d -<%d>- \n\n",  __FUNCTION__, __LINE__ , retval);

  return retval;
}
//***********************************************************************************************

// Datei kopieren
// --------------
// bestehende Dateien werden nicht �berschrieben
// R�ckgabe: kopierter Datei-Typ
enum Filetype copyFile(char* destination, const char* source)
{
  DEBUG_t("===> %s()#%d: %s(%s,\n                       <----- %s) ========\n",
          __FUNCTION__, __LINE__, __FUNCTION__, destination, source);

  int in_fd;
  int out_fd;
  int n_chars;
  char buf[BUFFER];
  char ErrText[ERRBUFLEN];

	Startzeit(T_COPY);                     									// Zeitmessung starten

  // Quell- und Zieldatei �ffnen
  // ---------------------------
  if((out_fd = open(destination, FLAGS, MODE)) == -1 )    // Ziel-Datei �ffnen
  { // -- Error
    if (errno == EEXIST)                                  // - Datei ist schon vorhanden
    {
      { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "     %s()#%d: '%s' schon vorhanden!\n"
      DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, destination);
      #undef MELDUNG
      } // --------------------------------------------------------------

      // die Quelldatei l�schen
      // ----------------------
      remFile(source, SOFORT_h);

      DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__ , EXIT_SUCCESS);
      return OHNE;
    }
    else if (errno = 21)  // '.' und '..'
      return OHNE;

    sprintf(ErrText, "create('%s')", destination);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  if((in_fd = open(source, O_RDONLY)) == -1 )             // Quell-Datei �ffnen
  { // -- Error
    sprintf(ErrText, "open '%s'", source);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  // Inhalt �bertragen
  // =====================
  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: "
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__);
    #undef MELDUNG
  } // --------------------------------------------------------------
  long lng = 0;
  while( (n_chars = read(in_fd, buf, BUFFER)) > 0 )       // Quell-Datei lesen ...
  {                                                       // ... und in Ziel-Datei schreiben
    if( write(out_fd, buf, n_chars) != n_chars )
    { // -- Error
      sprintf(ErrText, "write '%s'", destination);
      return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
    }
    printf(".");
    lng += n_chars;
  } // =============== fertig =====================
  printf("\n");
  { // --- Debug-Ausgaben -------------------------------------------------------
    #define MELDUNG   "     %s()#%d: '%ld' chars in %d msec kopiert!\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, lng, (int)Zwischenzeit(T_COPY));
    #undef MELDUNG
  } // --------------------------------------------------------------------------

  if( n_chars == -1 )                                     // Lesefehler
  { // -- Error
    sprintf(ErrText, "read '%s'", source);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  if( close(in_fd) == -1)                                 // Quell-Datei schlie�en
  { // -- Error
    sprintf(ErrText, "close '%s'", destination);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }
  { // --- Debug-Ausgaben -------------------------------------------------------
    #define MELDUNG   "     %s()#%d: nach %d msec: '%s' geschlossen\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, (int)Zwischenzeit(T_COPY), destination);
    #undef MELDUNG
  } // --------------------------------------------------------------------------

  if( close(out_fd) == -1 )                               // Ziel-Datei schlie�en
  { // -- Error
    sprintf(ErrText, "close '%s'", source);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }
  { // --- Debug-Ausgaben -------------------------------------------------------
    #define MELDUNG   "     %s()#%d: nach %d msec: '%s' geschlossen\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, (int)Zwischenzeit(T_COPY), source);
    #undef MELDUNG
  } // --------------------------------------------------------------------------
  
  
  char tmpbuf[NOTIZ];
  sprintf(tmpbuf, "%s(%ld)", destination, lng);
  newFiles = Added(tmpbuf);

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: Datei kopiert!\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__);
    #undef MELDUNG
  } // --------------------------------------------------------------

  if(getFiletyp(source) == AVI)
  {
    // Namen f�r Mail-Message merken
    // -----------------------------
    char datum[ZEILE/8];
    char event[ZEILE/8];
    char* datbeg = strrchr(destination, '/');             // Dateinamen suchen
    strcpy(datum, datbeg+1);
    char* datend = strchr(datum, '-');                    // Trenner suchen
    *datend = '\0';
    replace_character(datum, '.', ':');
    strcpy(event, datend+1);
    char* eventend = strrchr(event, '.');
    *eventend = '\0';
    sprintf("newItem", "Event %s - %s", event, datum);
  }

  DEBUG_t("<--- %s()#%d -<%d>- in %d msec\n",
           __FUNCTION__, __LINE__ , getFiletyp(source), (int)Zwischenzeit(T_COPY));

  return getFiletyp(source);
}
//***********************************************************************************************

//// Dateien mit externen Buffer synchronisieren
//// -------------------------------------------
static int SyncoFiles (const char *Pfad)
{
//  DEBUG(" ==> %s()#%d: function %s(%s) ======================\n",
//             __FUNCTION__, __LINE__, __FUNCTION__, Pfad);
//  char Logtext[ZEILE];
//  time_t Start;
//  int Dauer;
//  int Status = 0;
  int retval = EXIT_SUCCESS;
//
//  Start = time(NULL);
//  Status = copyFile(target, Pfad);
//  Dauer = time(NULL) - Start;
//
//  sprintf(Logtext, ">> Save-Command done in %d sec (%d)\n", Dauer, Status);
//  syslog(LOG_NOTICE, "%s(): %s",__FUNCTION__, Logtext);
//
//  if (Status != 0)
//  { // �bertragung hat nicht geklappt !
//    // --------------------------------
//    sprintf(Logtext, ">> Save-Command failed: Status %d\n", Status);
//    syslog(LOG_NOTICE, "%s(): %s",__FUNCTION__, Logtext);
//    retval = EXIT_FAILURE;
//  }
//  DEBUG("<- %s()#%d  \n",  __FUNCTION__, __LINE__);
  return retval;
}
//***********************************************************************************************

// belegten Speicherplatz ermitteln
// ---------------------------------
unsigned long calcSize(char* Pfad)
{
  DEBUG_c("=> %s()#%d: %s(%s) =============\n",
                              __FUNCTION__, __LINE__, __FUNCTION__, Pfad);

  unsigned long SizeTotal = 0;                          // gesamte Speicherbelegung
  char ErrText[ERRBUFLEN];

  // alle Dateien in allen Verzeichnisse durchsuchen
  // =============================================================
  DIR *pdir = opendir(Pfad);                    // '.../pics' �ffnen
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }

  // das komplette Verzeichnis auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    // Datumsverzeichnis-Ebene: '2021-05-09' !
    // ---------------------------------------
    char dDirname[ZEILE];                       // Pfad des Datumsverzeichnisses
    sprintf(dDirname,"%s%s", Pfad, (*pdirzeiger).d_name);

    char datdir[ZEILE];                         // Name der Datumsverzeichnisses
    strcpy(datdir, strrchr(dDirname, '/')+1);   // ... das letzte Glied
    if (isDatum(datdir))                        // soll diese Verzeichnis angesehen werden?
    {
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n== %s()#%d: '%s'\n"
        DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, dDirname);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
      DIR* edir = opendir(dDirname);            // Unterverzeichnis �ffnen
      if (edir == NULL)
      { // -- Error
        sprintf(ErrText, "opendir '%s'", dDirname);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      // das komplette Verzeichnis auslesen
      // ----------------------------------
      struct dirent* edirzeiger;
      while((edirzeiger=readdir(edir)) != NULL)
      {
        // Eventverzeichnis-Ebene!
        // -----------------------
        char eDirname[ZEILE];                   // Pfad des Datumsverzeichnisses
        sprintf(eDirname,"%s/%s", dDirname, (*edirzeiger).d_name);

        if (strstr(eDirname, _EVENT_) != NULL)  // soll diese Verzeichnis angesehen werden?
        {
          { // --- Debug-Ausgaben ----------------------------------------------------------------
            #define MELDUNG   "== %s()#%d: '%s'\n"
            DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, eDirname);
            #undef MELDUNG
          } // ------------------------------------------------------------------------------------

          DIR* udir = opendir(eDirname);            // Unterverzeichnis �ffnen
          if (udir == NULL)
          { // -- Error
            sprintf(ErrText, "opendir '%s'", eDirname);
            return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
          }
          // hier stehen alle Dateien eines Unterverzeichnisses zur Verf�gung
          // ----------------------------------------------------------------
          unsigned long cSizeofall=0;           // Speicherbelegung dieses Unterverzeichnisses
          struct dirent* udirzeiger;
          while((udirzeiger=readdir(udir)) != NULL) // Zeiger auf den Inhalt diese Unterverzeichnisses
          {
            // jede Datei ansehen
            // ------------------
            if (((*udirzeiger).d_type) == DT_REG)
            { // regul�re Datei
              // --------------
              struct stat cAttribut;
              char Filename[ZEILE+8];
              sprintf(Filename,"%s/%s", eDirname, (*udirzeiger).d_name);
              { // --- Debug-Ausgaben ----------------------------------------------------------------
                #define MELDUNG   "== %s()#%d: '%s'\n"
                DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, Filename);
                #undef MELDUNG
              } // ------------------------------------------------------------------------------------
              if(stat(Filename, &cAttribut) == -1)            // Datei-Attribute holen
              { // -- Error
                sprintf(ErrText, "read attribut '%s'", Filename);
                return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
              }
              // hier stehen die Attribute jeder Datei einzeln zur Verf�gung
              // -----------------------------------------------------------
              unsigned long cFSize = cAttribut.st_size;       // Dateil�nge
              cSizeofall += cFSize;                           // Gesamtl�nge [kB]
              { // --- Debug-Ausgaben ----------------------------------------------------------------
                #define MELDUNG   "== %s()#%d:   cSizeofall = '%lu' chars += '%lu' chars\n"
                DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, cSizeofall, cFSize);
                #undef MELDUNG
              } // ------------------------------------------------------------------------------------
            }
          }
          if (closedir(udir) != 0)
          { // -- Error
            sprintf(ErrText, "closedir '%s'", eDirname);
            return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
          }
          SizeTotal += cSizeofall;                            // gesamte Speicherbelegung
          { // --- Debug-Ausgaben ----------------------------------------------------------------
            #define MELDUNG   "== %s()#%d:   SizeTotal = '%lu' chars\n\n"
            DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, SizeTotal);
            #undef MELDUNG
          } // ------------------------------------------------------------------------------------
        }
      }
      if (closedir(edir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", dDirname);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
    }
  }
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }
#if BREAK22
  { // // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n   %s()#%d:   <--- Funktion '%s' fertig! -- weiter mit ENTER -->\n",
                __FUNCTION__, __LINE__, __FUNCTION__);
    char dummy;
    scanf ("%c", &dummy);
  }
#endif

//        {
//          char LogText[ZEILE];
//          sprintf(LogText, "       --- belegter Speicher: %lu Bytes", SizeTotal);
//          MYLOG(LogText);
//        }

  DEBUG_c("<- %s()#%d -(%lu)-\n",  __FUNCTION__, __LINE__, SizeTotal);
  return SizeTotal;
}
//***********************************************************************************************
//
// On Linux, the dirent structure is defined as follows:
//
//    struct dirent {
//        ino_t          d_ino;       /* inode number */
//        off_t          d_off;       /* offset to the next dirent */
//        unsigned short d_reclen;    /* length of this record */
//        unsigned char  d_type;      /* type of file; not supported
//                                       by all file system types */
//        char           d_name[256]; /* filename */
//    };

// ----------------------------------------------------
// alles, was �lter als 'MAXALTER_h' ist, wird gel�scht
// ----------------------------------------------------

int delOldest(char* Pfad)
{
  DEBUG_d("=> %s()#%d: function %s('%s') ====\n",
                __FUNCTION__, __LINE__, __FUNCTION__, Pfad);

  int FileKill = 0;                             // Z�hler gel�schte Dateien
  int FoldKill = 0;                             // Z�hler gel�schte Verzeichnisse
  char ErrText[ERRBUFLEN];

  // alle Dateien in allen Verzeichnissen durchsuchen
  // =============================================================
  DIR* pdir = opendir(Pfad);
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }

  // alle Datumsverzeichnisse auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char kurzname[NOTIZ];                       // Name des Datumsverzeichnisses
    char cDirname[ZEILE];                       // Pfad des Datumsverzeichnisses
    strcpy(kurzname, (pdirzeiger)->d_name);
    sprintf(cDirname,"%s/%s", Pfad, kurzname);

    { // --- Debug-Ausgaben -----------------------------------------------------------------------
      #define MELDUNG   "\n   %s()#%d: === Datums-Verzeichnis '%s' ===\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, cDirname);
      #undef MELDUNG
    } // ------------------------------------------------------------------------------------------

    if (isDatum(kurzname))                      // Format 'YYYY-MM-DD' ?
    // dies sind die Datumsverzeichnisse !
    // ---------------------------------
    {
      DIR* cdir = opendir(cDirname);            // Datumsverzeichnis �ffnen
      if (cdir == NULL)
      { // -- Error
        sprintf(ErrText, "Error opendir(%s): %d", cDirname, errno);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      // hier stehen alle Dateien eines Datumsverzeichnisses zur Verf�gung
      // ----------------------------------------------------------------
      struct dirent* cdirzeiger;
      while((cdirzeiger=readdir(cdir)) != NULL) // Zeiger auf den Inhalt
      {
        if (((*cdirzeiger).d_type) == DT_DIR)
        {   // ------- Verzeichnis -------
          { // --- Debug-Ausgaben -------------------------------------------------------
            #define MELDUNG   "   %s()#%d: --- Verzeichnis '%s' ---\n"
            DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, (cdirzeiger)->d_name);
            #undef MELDUNG
          } // --------------------------------------------------------------------------
          char Folder[NOTIZ];                             // Name des Verzeichnisses
          sprintf(Folder,"%s/%s", cDirname, (cdirzeiger)->d_name);
          int Killed = remFolder(Folder, MAXALTER);       // beide Werte enthalten
          FileKill += Killed / FFAKTOR;
          FoldKill += Killed % FFAKTOR;

        }
        else  if (((*cdirzeiger).d_type) == DT_REG)
        {   // ----- regul�re Datei (Film) ------
          { // --- Debug-Ausgaben -------------------------------------------------------
            #define MELDUNG   "   %s()#%d: --- Datei '%s' ------\n"
            DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, (cdirzeiger)->d_name);
            #undef MELDUNG
          } // --------------------------------------------------------------------------
          char Datei[NOTIZ];                    // Name der Datei
          sprintf(Datei,"%s/%s", cDirname, (cdirzeiger)->d_name);
          FileKill += remFile(Datei, MAXALTER);
        }
      }
      if (closedir(cdir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", cDirname);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }

    }

    // Versuch, das Datumsverzeichnis zu l�schen (wenn leer)
    // -----------------------------------------------------
    FoldKill += remFolder(cDirname, SOFORT_h);

    #define MELDUNG   "   %s()#%d: FoldKill=%d, FileKill=%d!\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, FoldKill, FileKill);
    #undef MELDUNG

    #if BREAK_delOldest1
    { // STOP! -- weiter mit ENTER
      // -------------------------------
      printf("\n   %s()#%d:   <--- das war Datums-Verzeichnis '%s'! -- weiter mit ENTER -->\n",
                  __FUNCTION__, __LINE__ , cDirname);
      char dummy;
      scanf ("%c", &dummy);
    }
    #endif

  } //  ----------- das waren die Datums-Verzeichnisse -----------

  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }


  // jetzt noch die Statistik ausgeben
  // =====================================
  if ((FileKill == 0) && (FoldKill == 0))
  {
    { // --- Debug-Ausgaben ----------------------------------------------------------------
      #define MELDUNG   "   %s()#%d: keine Dateien gel�scht !\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__);
      #undef MELDUNG
    } // ------------------------------------------------------------------------------------
  }
  else
  {
    if (FileKill > 0)
    {
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "   %s()#%d: %d Dateien gel�scht !\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, FileKill);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
    }
    if (FoldKill > 0)
    {
      if (FoldKill == 1)
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "   %s()#%d: LED_EIN Verzeichnis gel�scht !\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
      else if (FoldKill > 1)
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "   %s()#%d: %d Verzeichnisse gel�scht !\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, FoldKill);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
    }
  }
  int retval = FFAKTOR*FileKill + FoldKill;

#if BREAK_delOldest2
  { // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n   %s()#%d:   <--- Funktion '%s' fertig! -- weiter mit ENTER -->\n",
                __FUNCTION__, __LINE__, __FUNCTION__);
    char dummy;
    scanf ("%c", &dummy);
  }
#endif

  DEBUG_d("<- %s()#%d--<%d>--\n",  __FUNCTION__, __LINE__, retval);
  return retval;
} // ------------ delOldest
//***********************************************************************************************

// ---------------------------------------------------
// Dateien �bertragen
// ---------------------------------------------------
// Pfad: kompletter Quell-Verzeichnispfad
// Ziel: kompletter Ziel-Verzeichnispfad

long FileTransfer(const char* Pfad, const char* Ziel)
{
  DEBUG_t("=> %s()#%d: >%s('%s',\n"\
     "                               ===>  '%s')\n",
                 __FUNCTION__, __LINE__, __FUNCTION__, Pfad, Ziel);
  int fldCount=0;
  int aviCount=0;
  int jpgCount=0;

  // alle Dateien in allen Verzeichnisse durchsuchen
  // =============================================================
  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG  "   %s()#%d: Verzeichnis '%s' untersuchen\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, Pfad);
    #undef MELDUNG
  } // --------------------------------------------------------------

  DIR *pdir = opendir(Pfad);                                        // '.../pix' �ffnen
//  if (pdir == NULL)
//  { // -- Error
//    char ErrText[ZEILE];
//    sprintf(ErrText, "opendir('%s')", Pfad);
//    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
//  }

  // das komplette Quell-Verzeichnis auslesen
  // ----------------------------------------
  bool Toggle=0;
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
	{
    Toggle = !Toggle;

    char QuellPfad[ZEILE];                                          // Gesamt-Pfad des Unterverzeichnisses
    char QuellVerzeichnis[ZEILE];                                   // Name des Unterverzeichnisses
    strcpy(QuellVerzeichnis, (*pdirzeiger).d_name);                 // Name des Unterverzeichnisses
      { // --- Debug-Ausgabe ------------------------------------------
        #define MELDUNG   "   %s()#%d: - Unterverzeichnis '%s' untersuchen\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, QuellVerzeichnis);
        #undef MELDUNG
      } // --------------------------------------------------------------
    sprintf(QuellPfad,"%s%s", Pfad, QuellVerzeichnis);
    if (strstr(QuellVerzeichnis, _EVENT_) != NULL)                  // soll diese Verzeichnis angesehen werden?
    {

      { // --- Debug-Ausgabe ------------------------------------------
        #define MELDUNG   "   %s()#%d: - Unterverzeichnis '%s' untersuchen\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, QuellVerzeichnis);
        #undef MELDUNG
      } // --------------------------------------------------------------

  		// zun�chst untersuchen, ob leeres Verzeichnis  
      // ------------------------------------
      // ... l�schen, wenn das Verzeichnis leer ist
      if (rmdir(QuellPfad) == 0)
      { // gel�scht!
      	char TmpText[ZEILE];
        sprintf(TmpText, "%s/", QuellPfad);     // als Verzeichnis kennzeichnen
        delFolders += Deleted(TmpText);         // im Log vermerken
      }
      else
      { // --- Debug-Ausgabe ------------------------------------------
        #define MELDUNG   "   %s()#%d: -   '%s' nicht geloescht: Err %d - '%s'\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, QuellPfad, errno, strerror(errno));
        #undef MELDUNG
      } // --------------------------------------------------------------

			
			
			
//*                       !------------- QuellPfad -----------------!
//*                                              !-QuellVerzeichnis-!
//*      SOURCE           /home/pi/motion/pix/ - Event_2879/*
//*
//*      DESTINATION            /media/kamera/ - 2021-02-08/Event_2879/*
//*                                              !--Datum--!---Event---!
//*                                              !-- ZielVerzeichnis --!
//*                             !------------- ZielPfad ---------------!
//*        =         DISKSTATION/surveillance/ -  .....................
//*

      // Datum der Quelle ermitteln
      // ---------------------------
      struct stat attribut;
      if(stat(QuellPfad, &attribut) == -1)
      { // -- Error
        char ErrText[ZEILE];
        sprintf(ErrText, "attribut('%s')", QuellPfad);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      time_t ZeitStempel = attribut.st_atime;

      // Zielverzeichnis gleichen Namens anlegen
      // ----------------------------------------
      char ZielPfad[ZEILE]={'\0'};                                  // Datums-Zielpfad
      char ZielVerzeichnis[ZEILE]={'\0'};                           // Datums-Zielverzeichnis
      makeDatumsFoldername(ZeitStempel, ZielVerzeichnis);           // Namen f�r Datums-Zielverzeichnis
      strcpy(ZielPfad, Ziel);                                       // Ziel-Verzeichnis im Raspi: 'media/kamera/'
      strcat(ZielPfad, ZielVerzeichnis);                            // an Pfad dranh�ngen
      fldCount += makeFolder(ZielPfad);                             // Datums-Zielverzeichnis erstellen

      { // --- Debug-Ausgabe --------------------------------------------
        #define MELDUNG   "   %s()#%d: --> ZielPfad '%s' (No.%d)\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, ZielPfad, fldCount);
        #undef MELDUNG
      } // --------------------------------------------------------------

      // Unterverzeichnis f�r das Ereignis anlegen
      // -----------------------------------------
      char EventVerzeichnis[ZEILE]={'\0'};                          // Ereignis-Zielverzeichnis
      strcpy(EventVerzeichnis, ZielPfad);                           // DatumsVerzeichnis als Vorspann
      strcat(EventVerzeichnis, "/");
      char* Originalname=strrchr(QuellPfad,'/') + 1;                // Originalname vm QuellVerzeichnis
      strcat(EventVerzeichnis, Originalname);                       // Ereignis-Zielverzeichnis erg�nzen
      fldCount += makeFolder(EventVerzeichnis);                     // Ereignis-Zielverzeichnis erstellen

      { // --- Debug-Ausgabe -----------------------------------------------
        #define MELDUNG   "   %s()#%d: EventVerzeichnis '%s' (No.%d)\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, EventVerzeichnis, fldCount);
        #undef MELDUNG
      } // ------------------------------------------------------------------

      { // Dateien �bertragen
        // ---------------------
        DIR* QuellDir = opendir(QuellPfad);                         // QuellVerzeichnis �ffnen
        if (QuellDir == NULL)
        { // -- Error
          char ErrText[ZEILE];
          sprintf(ErrText, "opendir('%s')", QuellPfad);
          return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
        }

        struct dirent* QuellZeiger;
        while((QuellZeiger=readdir(QuellDir)) != NULL)              	// alle Dateien dieses Verzeichnisses
        {
          char Dateiname[ZEILE];
          strcpy(Dateiname, (*QuellZeiger).d_name);                   // zu kopierende Datei
          char QuellDateiname[ZEILE];
          sprintf(QuellDateiname,"%s/%s", QuellPfad, Dateiname);      // Quell-Datei
          char ZielDateiname[ZEILE];
          sprintf(ZielDateiname,"%s/%s", EventVerzeichnis, Dateiname);// Ziel-Datei

          switch(copyFile(ZielDateiname, QuellDateiname))             // -- Datei �bertragen --
          {
            case OHNE:
               break;
            case JPG:
              jpgCount++;
              break;
            case AVI:
              aviCount++;
              break;
            case MKV:
              aviCount++;
              break;
            default:
              break;
          }
//          newFiles = Added(ZielDateiname);
        } // --- alle Dateien

        // QuellVerzeichnis wieder schlie�en
        // ----------------------------------
        if (closedir(QuellDir) != 0)
        { // -- Error
          char ErrText[ZEILE];
          sprintf(ErrText, "closedir '%s'", QuellPfad);
          return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
        }
      }

      #if BREAK_FileTransfer1
      { // STOP! -- Verzeichnis fertig -- weiter mit ENTER
        // ------------------------------------------------------------
        printf("\n   %s()#%d:   --- Verzeichnis '%s' fertig! -- weiter mit ENTER --\n\n",
                    __FUNCTION__, __LINE__, QuellPfad);
        char dummy;
        scanf ("%c", &dummy);
      }
      #endif

      // das QuellVerzeichnis l�schen
      // ----------------------------
      remFolder(QuellPfad, SOFORT_h);

    }  // --- dieses Eventverzeichnis


    // nun auch noch das Verzeichnis l�schen
    // ------------------------------------
    // ... wenn das Verzeichnis leer ist
    if (rmdir(QuellPfad) == 0)
    { // gel�scht!
    	char TmpText[ZEILE];
      sprintf(TmpText, "%s/", QuellPfad);     // als Verzeichnis kennzeichnen
      delFolders += Deleted(TmpText);         // im Log vermerken
    }
//    else
//    { // -- Error
//     char ErrText[ZEILE];
//     sprintf(ErrText, "rmdir('%s'):"\
//      "              Err %d - '%s'", QuellPfad, errno, strerror(errno));
//      Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
//    }



  } // --. Gesamt-Verzeichnis --
  // Gesamt-Verzeichnis schlie�en
  // ------------------------------
  if (closedir(pdir) != 0)
  { // -- Error
    char ErrText[ZEILE];
    DEBUG(ErrText, "closedir '%s'", Pfad);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
  }

  #if BREAK_FileTransfer2
  { // STOP! -- Funktionsende -- weiter mit ENTER
    // ------------------------------------------------
    printf("\n   %s()#%d:   <--- Funktion '%s' fertig! -- weiter mit ENTER -->\n",
                __FUNCTION__, __LINE__, __FUNCTION__);
    char dummy;
    scanf ("%c", &dummy);
  }
  #endif

  int retval = (jpgCount*DFAKTOR) + (aviCount*FFAKTOR) + fldCount;

  DEBUG_t("<- %s()#%d --<%d>-- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//***********************************************************************************************
//                                                                                              *
//                                    main()                                                    *
//                                                                                              *
//***********************************************************************************************

int main(int argc, char *argv[])
{
	char MailBody[4*ABSATZ] = {'\0'};
  char puffer[BUFFER];
  int fd;

  sprintf (Version, "Vers. %d.%d.%d", MAXVERS, MINVERS, BUILD);
  printf("   %s %s von %s\n\n", PROGNAME, Version, __DATE__);

  // Verbindung zum D�mon Syslog aufbauen
  // -----------------------------------
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 );
  syslog(LOG_NOTICE, ">>>>> %s - %s/%s - PID %d - User %d, Group %d <<<<<<",
                  PROGNAME, Version, __DATE__, getpid(), geteuid(), getegid());
  setAuxFolder(AUXDIR, TOPNAME);         		// Info an 'common'
  setAuxFolder_Err(AUXDIR);                	// Info an 'error'

  #include "/home/pi/treiber/snippets/get_progname.snip"


//  // Signale registrieren
//  // --------------------
//  signal(SIGTERM, sigfunc);
//  signal(SIGKILL, sigfunc);


  // schon mal den Watchdog f�ttern ...
  // -----------------------------------
  #include "/home/pi/treiber/snippets/set_watchdog.snip"
  

  // Host ermitteln
  // ---------------
  #include "/home/pi/treiber/snippets/get_host.snip"


  // Prozess-ID ablegen
  // ------------------
  #include "/home/pi/treiber/snippets/get_mypid.snip"


  // IP-Adresse ermitteln
  // ----------------------
  #include "/home/pi/treiber/snippets/get_myip.snip"


  // Datenbank-Initialisierung
  // ---------------------------
  #include "/home/pi/treiber/snippets/createdb_init.snip"


  // Ist GPIO klar?
  // --------------
  #include "/home/pi/treiber/snippets/gpio_init.snip"
  {
    pinMode (M_LED_ROT,    OUTPUT);
    pinMode (M_LED_GELB,   OUTPUT);
    pinMode (M_LED_GRUEN,  OUTPUT);
    pinMode (M_LED_BLAU,   OUTPUT);
    pullUpDnControl (M_LED_ROT, PUD_UP) ;
    pullUpDnControl (M_LED_GELB, PUD_UP) ;
    pullUpDnControl (M_LED_GRUEN, PUD_UP) ;
    pullUpDnControl (M_LED_BLAU, PUD_UP) ;
    #define ANZEIT  44 /* msec */
    digitalWrite (M_LED_ROT,   LED_EIN);
    delay(3*ANZEIT);
    digitalWrite (M_LED_ROT,   LED_AUS);
    for (int ix=0; ix < 12; ix++)
    {
      digitalWrite (M_LED_GELB,   LED_EIN);
      delay(ANZEIT);
      digitalWrite (M_LED_GELB,   LED_AUS);
      digitalWrite (M_LED_GRUEN,  LED_EIN);
      delay(ANZEIT);
      digitalWrite (M_LED_GRUEN,  LED_AUS);
      digitalWrite (M_LED_BLAU,   LED_EIN);
      delay(ANZEIT);
      digitalWrite (M_LED_BLAU,   LED_AUS);
    }
	}

  // named pipe(Fifo) erstellen
  // --------------------------
  #include "/home/pi/treiber/snippets/init_fifo.snip"


  // CPU-Temperatur aktivieren
  // -------------------------------
  #include "/home/pi/treiber/snippets/init_intern.snip"


  // LCD-Display aktivieren
  // -----------------------
  #include "/home/pi/treiber/snippets/lcddisplay_init.snip"


  // alle DS18B20-Sensoren einlesen
  // -------------------------------
  #include "/home/pi/treiber/snippets/ds18b20_init.snip"


  // Initialisierung des BME280-Sensors
  // -----------------------------------
  #include "/home/pi/treiber/snippets/bme280_init.snip"


  // Initialisierung des TLS2561-Sensors
  // -----------------------------------
  #include "/home/pi/treiber/snippets/tls2561_init.snip"


  // MQTT starten
  // --------------
  #include "/home/pi/treiber/snippets/mqtt_init.snip"


  // Snapshot vorsichtshalber erneuern
  // -----------------------------------------
//	char Snapshot[ZEILE];													// Schnappschuss-Datei
	sprintf(Snapshot, SNAPSHOT, AUXDIR);
  FILE* lastsnap = fopen(Snapshot, "w+");
  if(NULL == lastsnap)
    perror("open SNAPSHOT");
   else
    fclose(lastsnap);

//char   Snapshot[ZEILE];													// Schnappschuss-Datei
//
//#define SNAPSHOT         "/%s/lastsnap.jpg"


  // Initialisierung abgeschlossen
  // ------------------------------
  #include "/home/pi/treiber/snippets/init_mail.snip"
  #include "/home/pi/treiber/snippets/init_done.snip"


  // Fifo aktivieren und auf ersten Datenblock warten
  // ------------------------------------------------
  {
    DEBUG(">> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);
    digitalWrite (M_LED_GRUEN, LED_EIN);
    fd = open (FIFO, O_RDONLY);                   // Empf�nger liest nur aus dem FIFO
    if (fd == -1)
    {
      char LogText[ZEILE];
      sprintf(LogText, ">> %s()#%d: Error Open Fifo !",  __FUNCTION__, __LINE__);
      DEBUG(LogText);
      showMain_Error(LogText, __FUNCTION__, __LINE__);
      exit (EXIT_FAILURE);
    }
    else
    {// --- Log-Ausgabe -----------------------------------------------------------------
      char LogText[ZEILE];
      sprintf(LogText, ">>> %s()#%d: FIFO '%s' open !",  __FUNCTION__, __LINE__ , FIFO);
      syslog(LOG_NOTICE, "%s", LogText);
      DEBUG(LogText);
      DEBUG(">>> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);
    } // --------------------------------------------------------------------------------


//    char Betreff[ZEILE] = {'\0'};
//    char Zeitbuf[NOTIZ];
//    sprintf( Betreff, "FIFO open >%s< @ %s",  FIFO, mkdatum(time(0), Zeitbuf));
//    DEBUG( "Betreff: %s\n", Betreff);
//    DEBUG(">> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);

  }
 
  
  bool ShowReady = true;
  DO_FOREVER // *********************** Endlosschleife ********************************************
  {
    feedWatchdog(PROGNAME);
    if (ShowReady)    // nur einmalig anzeigen
    { // --- Debug-Ausgaben -----------------------------------------------------------------------
      #define MELDUNG   "\n>>> %s()#%d: -------------------- bereit  --  @ %s ---------------------\n"
      DEBUG(MELDUNG, __FUNCTION__, __LINE__, __NOW__);
      #undef MELDUNG
    } // ------------------------------------------------------------------------------------------
    ShowReady = false;
    digitalWrite (M_LED_BLAU, LED_AUS);
    digitalWrite (M_LED_GRUEN, LED_EIN);

    if ( read(fd, puffer, BUFFER) )             // == > auf Auftr�ge von 'motion' warten
    {
      ShowReady = true;
      Startzeit(T_GESAMT);                      // Zeitmessung starten
      digitalWrite (M_LED_BLAU, LED_EIN);
      digitalWrite (M_LED_GRUEN, LED_AUS);
      newFiles   = 0;                           // neue Dateien
      newFolders = 0;                           // neue Verzeichnisse
      delFiles   = 0;                           // gel�schte Dateien
      delFolders = 0;                           // gel�schte Verzeichnisse



      // === Dateien �bertragen, im Raspi aufr�umen  ==============================================
      {                  
        #define MELDUNG   "\n>>> %s()#%d: ################ neuer Auftrag @ %s ###############\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, __NOW__);
        #undef MELDUNG
      }
      { // --- Log-Ausgabe ------------------------------------------------------------------------
        char LogText[ZEILE];  
        sprintf(LogText, "    >>>----- neuer Auftrag: '%s' ----->>>", puffer);
        MYLOG(LogText);
        sprintf(LogText, "     ----< FileTransfer(FIFO --> '%s') >----", DESTINATION);
        MYLOG(LogText);
      } // ---------------------------------------------------------------------------------------

      Startzeit(T_FOLDER);                                          // Zeitmessung starten
      long Items = FileTransfer(puffer, DESTINATION);               // ***** Dateien �bertragen *****
      double trZeit = (float)Zwischenzeit(T_FOLDER) / 1000.0;       // [msec] --> [sec]
      
      { // --- Debug-Ausgaben ---------------------------------------------------------------------
        #define MELDUNG   "\n>>> %s()#%d: Items=%ld\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__,Items);
        #undef MELDUNG
        int fldCount  = (Items % DFAKTOR) % FFAKTOR;      // Verzeicnisse Filmdateien
        int aviCount  = (Items % DFAKTOR) / FFAKTOR;      // Z�hler Filmdateien
        int jpgCount  = Items / DFAKTOR;                  // Z�hler Bilddateien
        #define MELDUNG   "    %s()#%d: -- %d Verzeichnis%s, %d Film%s, %d Bild%s kopiert\n"\
               "                    von '%s' ---> '%s' in %2f3 sec --"
        DEBUG( MELDUNG, __FUNCTION__, __LINE__, fldCount,(fldCount>1 ? "se" : ""),
                                                aviCount,(aviCount>1 ? "e" : ""),
                                                jpgCount,(jpgCount>1 ? "er" : ""),
                                                SOURCE, puffer, trZeit);
        #undef MELDUNG
        { // --- Log-Ausgabe ------------------------------------------------------------------------
          char LogText[ZEILE];  
          sprintf(LogText, "     --- %d Verzeichnis%s, %d Film%s, %d Bild%s kopiert ...",
                                    fldCount,(fldCount>1 ? "se" : ""),
                                    aviCount,(aviCount>1 ? "e" : ""),
                                    jpgCount,(jpgCount>1 ? "er" : ""));
          MYLOG(LogText);
          sprintf(LogText, "     ... und %d Verzeichnis%s, %d Datei%s gel�scht in %2.3f sec! ---", 
                                    delFolders,(delFolders!=1 ? "se" : ""),
                                    delFiles,(delFiles!=1 ? "en" : ""),
                                    trZeit);
          MYLOG(LogText);
        } // ---------------------------------------------------------------------------------------
        delFolders=0;
        delFiles=0;
        UNUSED (fldCount);
        UNUSED (aviCount);
        UNUSED (jpgCount);
      } // ----------------------------------------------------------------------------------------
      UNUSED(Items);
      UNUSED(trZeit);



      // === aufr�umen ============================================================================
      { // --- Debug-Ausgaben ---------------------------------------------------------------------
        #define MELDUNG   "\n\n>>> %s()#%d: ################## es wird aufgeraeumt ##################\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
      } // ----------------------------------------------------------------------------------------

      { // --- Log-Ausgabe ------------------------------------------------------------------------
        char LogText[ZEILE];  sprintf(LogText, "     ----< delOldest(in '%s') >----", DESTINATION);
        MYLOG(LogText);
      } // ---------------------------------------------------------------------------------------
      
      Startzeit(T_FOLDER);                                // Zeitmessung starten
      delOldest(DESTINATION);                             // aufr�umen
      double delZeit = (float)Zwischenzeit(T_FOLDER)/1000;
      
      { // --- Debug-Ausgaben --------------------------------------------------------------------
        #define MELDUNG   "\n    %s()#%d: -- %d Verzeichnis%s, %d Datei%s in %f msec geloescht! --\n"
        DEBUG(MELDUNG,  __FUNCTION__, __LINE__, delFolders,(delFolders!=1 ? "se" : ""),
                                                delFiles,(delFiles!=1 ? "en" : ""),
                                                delZeit);
        #undef MELDUNG
      } // ---------------------------------------------------------------------------------------
      { // --- Log-Ausgabe -----------------------------------------------------------------------
        char LogText[ZEILE];  
        sprintf(LogText, "     --- %d Verzeichnis%s, %d Datei%s gel�scht in %2.3f sec! ---", 
                                  delFolders,(delFolders!=1 ? "se" : ""),
                                  delFiles,(delFiles!=1 ? "en" : ""),
                                  delZeit);
        MYLOG(LogText);
      } // ---------------------------------------------------------------------------------------
      UNUSED(delZeit);



      // === Berechnungen =========================================================================
      { // --- Debug-Ausgaben ---------------------------------------------------------------------
        #define MELDUNG   "\n\n>>> %s()#%d: ################ es wird berechnet #################\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
      } // ----------------------------------------------------------------------------------------


// wg. �berlastung vorerst stillgelegt
// ------------------------------------
//      Startzeit(T_FOLDER);                                // Zeitmessung starten
//      double dbSize = (double)calcSize(DESTINATION) * 1.0;
//      double usedSize = dbSize/(double)((unsigned long)GBYTES);
//      double calcZeit = (double)Zwischenzeit(T_FOLDER) / 1000.0;
//      
//      
//      { // --- Debug-Ausgaben ---------------------------------------------------------------------
//        #define MELDUNG   "\n    %s()#%d: -- Berechnung Speicherplatz: %2.3f MBytes in %2.3f sec --\n"
//        DEBUG(MELDUNG, __FUNCTION__, __LINE__, usedSize, calcZeit);
//        #undef MELDUNG
//        {
//          char LogText[ZEILE];
//          sprintf(LogText, "     --- belegter Speicher: %2.3f GB (in %2.3f sec)", usedSize, calcZeit);
//          MYLOG(LogText);
//        }
//      } // ----------------------------------------------------------------------------------------
//      UNUSED(usedSize);
//      UNUSED(calcZeit);



      // === Abschluss ============================================================================
      double Gesamtzeit = (float)Zwischenzeit(T_GESAMT)/1000.0;   // in [sec]

      { // --- Debug-Ausgaben ---------------------------------------------------------------------
        #define MELDUNG   "\n\n>>> %s()#%d: ###### done in %5.3f sec ####################\n\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, Gesamtzeit);
        #undef MELDUNG
      } // ----------------------------------------------------------------------------------------

      syslog(LOG_NOTICE, ">>> %s()#%d: <###---  done in %2.3f sec  ---###>",
                                                      __FUNCTION__, __LINE__, Gesamtzeit);
      { // --- Log-Ausgabe ------------------------------------------------------------------------
        char LogText[ZEILE];  
        sprintf(LogText, "    <<<----- fertig in %2.1f sec -----<<<", Gesamtzeit);
        MYLOG(LogText);
      } // ----------------------------------------------------------------------------------------

      digitalWrite (M_LED_GELB, LED_AUS);             // wurde von SqlMotion eingeschaltet!
    } // ======== Auftrag erledigt =================================================================




    // === �berwachung, ob 'motion' noch l�uft ====================================================
    // ---------------------------------------
    {
      // 'motion' erzeugt in regelm��igen Abst�nden ein 'snapshot'.
      static bool ganzneu = true;
      {
        struct stat attribut;
        static bool zualt = true;
        if(stat(Snapshot, &attribut) == -1)
        {
          if (ErrorFlag == 0)
          {
            { // --- Log-Ausgabe ------------------------------------------------------------------------
              char LogText[ZEILE];  sprintf(LogText, "    ----- '%s' missing! -----", Snapshot);
              MYLOG(LogText);
            } // ----------------------------------------------------------------------------------------
            zualt = true;
          }
        }
        else
        {
          errno = 0;
          time_t Alter = time(0) - attribut.st_mtime;
          if (Alter > 4*REFRESH)
          { // Meldung nur einmal anzeigen
            // ----------------------------
            if (!zualt)
            {
              char ErrText[ERRBUFLEN];
              sprintf(ErrText, "'%s' fehlt seit %ld sec!", Snapshot, Alter);
              Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
              zualt = true;
            }
          }
          else
          {
            if (zualt)
            {
              if (!ganzneu)
              {
                { // --- Log-Ausgabe ------------------------------------------------------------------------
                  char LogText[ZEILE];  sprintf(LogText, "    ----- '%s' wieder da! -----", Snapshot);
                  MYLOG(LogText);
                } // ----------------------------------------------------------------------------------------
              }
              ganzneu = false;
              zualt = false;
            }
          }
        }
      }
    }
    // === Ende �berwachung =======================================================================



    // Blinken gr�ne LED als Lebenszeichen! <
    // ------------------------------------
    {
      static int blink = 0;
      blink++;
      if (blink % 7 == 0)
      {
        digitalWrite (M_LED_GRUEN, LED_AUS);
        delay(100);
        digitalWrite (M_LED_GRUEN, LED_EIN);
      }
    }

    // nach einem Fehler ...
    // -----------------------------
    if (ErrorFlag > 0)
    { // ... rote LED wieder ausschalten
      // ---------------------------
      if (time(0) > ErrorFlag)
      { // wenn Zeit abgelaufen
        // --------------------
        ErrorFlag = 0;
        digitalWrite (M_LED_ROT, LED_AUS);
      }
    }

    // kleine Pause
    // -------------
    delay(100);

  } // =================================================================================
  // PID-Datei wieder l�schen 
  // ------------------------
  killPID();

  { // --- Log-Ausgabe ------------------------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "    <<<----- Programm beendet ----->>>");
    MYLOG(LogText);
  } // ----------------------------------------------------------------------------------------


  // Fehler-Mail abschicken (hier nutzlos)
  // -------------------------------------
  digitalWrite (M_LED_ROT, LED_EIN);
  { // --- Log-Ausgabe ------------------------------------------------------------------------
    char Logtext[ZEILE];  sprintf(Logtext, ">> %s()#%d: Error %s ---> '%s' OK\n",__FUNCTION__, __LINE__, PROGNAME, "lastItem");
//    syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);

    char MailBody[BODYLEN] = {'\0'};
    strcat(MailBody, Logtext);
    char Betreff[ERRBUFLEN];
    sprintf(Betreff, "Error-Message von %s: >>%s<<", PROGNAME, "lastItem");
    sendmail(Betreff, MailBody);                  // Mail-Message absetzen
  } // ----------------------------------------------------------------------------------------
}
//***********************************************************************************************
