//********** Vogel **********************************************************//
//*                                                                         *//
//* File:          sqlmotion.c                                              *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2014-07-20  --  2016-02-18                               *//
//* Last change:   2022-11-30 - 14:19:25                                    *//
//* Description:   Weiterverarbeitung von 'motion'-Dateien:                 *//
//*                Event ermitteln, daraus ein Verzeichnis erstellen,       *//
//*                zugehörige Dateien in dieses Verzeichnis verschieben     *//
//*                                                                         *//
//* Copyright (C) 2014-21 by Wolfgang Keuch                                 *//
//*                                                                         *//
//* Kompilation:                                                            *//
//*    make                                                                 *//
//*                                                                         *//
//* Aufruf:                                                                 *//
//*    ./SqlMotion thisPfad/*                                               *//
//*                                                                         *//
//***************************************************************************//

#define _POSIX_SOURCE
#define _DEFAULT_SOURCE
#define _MODUL0
#define __SQLMOTION_DEBUG__      true
#define __SQLMOTION_DEBUG_INIT__ true

#define __SQLMOTION_MYLOG__    true
#define __SQLMOTION_MYLOG1__   false
#define __SQLMOTION_MYLOG1a__  false
#define __SQLMOTION_MYLOG2__   false
//#define __SQLMOTION_DEBUG__    false
#define __SQLMOTION_DEBUG__d   false     /* Datenbanken */
#define __SQLMOTION_DEBUG__1   true
#define __SQLMOTION_DEBUG__2   true
#define __SQLMOTION_DEBUG__z   true


// Breakpoints
// -----------
#define BREAK1      0     /* der erste Durchlauf                  */
#define BREAK21     0     /* Beginn Phase 2                       */
#define BREAK22     0     /* nächste Datei in diesem Verzeichnis  */
#define BREAK23     0     /* ein Eventverzeichnis durchlaufen     */
#define BREAK24     0     /* alle Eventverzeichnisse durchlaufen  */


// Peripherie aktivieren
// ----------------------
#define __LCD_DISPLAY__    false   /* LCD-Display               */
#define __DS18B20__        false   /* DS18B20-Sensoren          */
#define __INTERNAL__       true    /* CPU-Temperatur            */
#define __BME280__         false   /* BME280-Sensor             */
#define __TSL2561__        false   /* TLS2561-Sensor            */
#define __GPIO__           true    /* GPIOs über 'gpio.c'       */
#define __MQTT__           false   /* Mosquitto                 */
#define __INTERRUPT__      false   /* Interrupt Geigerzähler    */
#define __DATENBANK__      true    /* Datenbank                 */


#include "./version.h"
#include "./sqlmotion.h"


#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>


//#include "../error.h"
//#include "../datetime.h"
//#include "../../sendmail/sendMail.h"

#include "/home/pi/treiber/common/gpio.h"
#include "/home/pi/treiber/common/error.h"
#include "/home/pi/treiber/common/common.h"
#include "/home/pi/treiber/common/datetime.h"
#include "/home/pi/treiber/sendmail/sendMail.h"

#include "/home/pi/treiber/dbank/createdb.h"
#include "./motion_db.h"

// statische Variable
// --------------------
static time_t ErrorFlag = 0;          // Steuerung rote LED

char MailBody[BODYLEN];


// programmweite Variable
// -----------------------
char   s_Hostname[ZEILE];                       // der Name dieses Rechners
char   s_meineIPAdr[NOTIZ];                     // die IP-Adresse dieses Rechners
uint   s_IPnmr=0;                               // letzte Stelle der IP-Adresse
long   s_pid=0;                                 // meine Prozess-ID

#if __DATENBANK__
  static MYSQL* con = NULL;                     // Verbindung zur Datenbank
  long ID_Site = -1;                            // KeyID dieses Rechners in der Datenbank
  long ID_Event = -1;                           // KeyID Ereignis-Tabelle
#endif

// gemeinsame Verzeichnisse
// ------------------------
#include "/home/pi/motion/motion.h"
	
//// gemeinsame Verzeichnisse
//// ------------------------
//#define _FOLDER       "event_"                  /* Kennzeichnung Verzeichnis */
//#define _EVENT_       "Event_"                  /* Kennzeichnung Verzeichnis */
//#define AUXDIR          AUXDIR
//#define FIFO          AUXDIR"/MOTION.FIFO"
//#define SOURCE        TOPDIR"/pix/"
//#define TEMP          TOPDIR"/tmp/"
//#define DESTINATION   "/media/Kamera/Vogel/"    /* = 'DISKSTATION/surveillance'/... */
//
//
//// Verzeichnisse
//// --------------
//#define _FOLDER       "event_"                  /* Kennzeichnung Verzeichnis */
//#define _EVENT_       "Event_"                  /* Kennzeichnung Verzeichnis */
//#define FIFO          AUXDIR"/MOTION.FIFO"
//#define SOURCE        PIXDIR/                   /* "/home/pi/motion/pix/" */
//#define TEMP          TOPDIR"/tmp/"
//#define DESTINATION   "/media/Kamera/Vogel/"    /* = 'DISKSTATION/surveillance'/... */
//

//***************************************************************************// bool MyLog(const char* pLogText)

/*
 * Define debug function.
 * ---------------------
 */

#if __SQLMOTION_DEBUG__
  #define DEBUG(...) printf(__VA_ARGS__)
#else
  #define DEBUG(...)
#endif

#if __SQLMOTION_DEBUG_INIT__
  // nur die Init-Phase
  // -------------------
  #define _DEBUG(...) printf(__VA_ARGS__)
#else
  #define _DEBUG(...)
#endif


//// Log-Ausgabe
//// -----------
#if __SQLMOTION_MYLOG__
  #define MYLOG(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
  #define MYLOG(...)
#endif

#if __SQLMOTION_MYLOG1__
  #define MYLOG1(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
  #define MYLOG1(...)
#endif

#if __SQLMOTION_MYLOG1a__
  #define MYLOG1a(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
  #define MYLOG1a(...)
#endif

#if __SQLMOTION_MYLOG2__
  #define MYLOG2(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
  #define MYLOG2(...)
#endif

// main()
// ------
#if __SQLMOTION_DEBUG__
#define DEBUG(...)  printf(__VA_ARGS__)
#define DEBUG_m(...)  printf(__VA_ARGS__)
#define SYSLOG(...)  syslog(__VA_ARGS__)
#define BREAKmain1 false
#define BREAKmain2 false
#else
#define DEBUG(...)
#define DEBUG_m(...)
#define SYSLOG(...)
#endif

// Datenbanken
// -------------
#if __SQLMOTION_DEBUG__d
#define DEBUG_d(...)  printf(__VA_ARGS__)
#else
#define DEBUG_d(...)
#endif


#if __SQLMOTION_DEBUG__1
#define DEBUG_1(...)  printf(__VA_ARGS__)
#else
#define DEBUG_1(...)
#endif

#if __SQLMOTION_DEBUG__2
int cnt = 0;
#define DEBUG_2(...)  printf(__VA_ARGS__)
#else
#define DEBUG_2(...)
#endif

#if __SQLMOTION_DEBUG__z
#define DEBUG_z(...)  printf(__VA_ARGS__)
#else
#define DEBUG_z(...)
#endif


//************************************************************************************//

// Fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_Error( char* Message, const char* Func, int Zeile)
{
  char ErrText[ERRBUFLEN];
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Message);

  printf("\n    -- Fehler --> %s\n", ErrText);    // lokale Fehlerausgabe
  syslog(LOG_NOTICE, ErrText);
  digitalWrite (M_LED_GELB,   LED_AUS);
  digitalWrite (M_LED_GRUEN,  LED_AUS);
  digitalWrite (M_LED_BLAU,   LED_AUS);
  digitalWrite (M_LED_ROT,    LED_EIN);

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<< %s: Exitosi!",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // PID-Datei wieder löschen
  // ------------------------
  killPID();

  finish_with_Error(ErrText);
}
//************************************************************************************//

// Fataler Datenbank-Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_SQL_Error( char* Label, const char* Func, int Zeile, MYSQL *con)
{
  char ErrSql[ZEILE];
  char ErrText[ERRBUFLEN];
  sprintf(ErrSql, "SQL-Error %d(%s)", mysql_errno(con), mysql_error(con));
  mysql_close(con);                             // Datenbank schließen
  sprintf(ErrText, "%s -- %s", Label, ErrSql);
  showMain_Error( ErrText, Func, Zeile);        // Meldung weiterreichen
}
//***********************************************************************************************

 // nicht-fataler Fehler
 // ------------------------
 // Lokale Fehlerbearbeitung
 // Fehler wird nur geloggt
int Error_NonFatal( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  if (errsv == 0)
    sprintf( Fehler, "%s", Message);            // kein Fehler, nur Meldung
  else
    sprintf( Fehler, "%s - Err %d-%s", Message, errsv, strerror(errsv));

  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  DEBUG("   -- Fehler -->  %s\n", ErrText);     // lokale Fehlerausgabe

  digitalWrite (M_LED_ROT,    LED_EIN);
  ErrorFlag = time(0) + BRENNDAUER;             // Steuerung rote LED

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<< %s",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  report_Error(ErrText, true);                  // Fehlermeldung immer mit Mail ausgeben

//  if (errsv == 24)                              // 'too many open files' ...
//    report_Error(ErrText, true);                // Fehlermeldung mit Mail ausgeben
//  else
//    report_Error(ErrText, false);               // Fehlermeldung ohne Mail ausgeben
  return errsv;
}
//************************************************************************************//

//************************************************************************************//

// Eventnummer ermitteln
// ---------------------
//     home/pi/motion/pix/401-20210205_134103-02.jpg
//     -- 06-20201229133023-01 -- Eventnummer steht jetzt vorn

#define EVMRK '/'      /* vordere Markierung von hinten */
#define EVEND '-'      /* hintere Markierung */

int getEventNo(char* Filename)
{
  DEBUG("=> %s()#%d %s(%s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, Filename);
  int EventNo = -1;
  char sEvent[12];

  char* evbeg = strrchr(Filename, EVMRK) + 1;
  if (evbeg != NULL)
  {
    char* evend = strchr(evbeg, EVEND);
    if (evend != NULL)
    {
      int evlen = evend-evbeg;
      strncpy(&sEvent[0], evbeg, evlen);
      sEvent[evlen] = '\0';
      EventNo = atoi(sEvent);
    }
  }

  DEBUG("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, EventNo);
  return EventNo;
}
//************************************************************************************//

// Eventnamen erzeugen
// -----------------------------------------
// ausgehend vom der Eventnummer (xx_ ... )wird die erste nicht besetzte Nummer gesucht
// Dies ist der 'unique'-Schlüssel für das Ereignis       `

#define EVSLH '_'      /* Markierung von hinten */

int getEventKey(MYSQL* con, char* FolderName, char* EventKey)
{
  DEBUG_d("=> %s()#%d: %s('%s', '%s') ======\n",
             __FUNCTION__, __LINE__, __FUNCTION__, FolderName, EventKey);
  int retval = false;
  int TempZahl = 1000;                // Default-Basis
  char textQuery[ZEILE];              // Buffer für Datenbankabfragen

  char* str = strrchr(FolderName, EVSLH) + 1;
  // dies sollte eine 4stellige Zahl sein
  if (str != NULL)
  { // ... aus Foldername
    TempZahl = atoi(str);
  }

  unsigned int num_rows = -1;         // Anzahl Zeilen in der Abfrage
  do  // ===============================================================================
  {
    char TempKey[ZEILE];
    sprintf( TempKey, "%s%04d", _FOLDER, TempZahl);
    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: TempZahl='%d' => '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, TempZahl, TempKey);
      #undef MELDUNG
    } // --------------------------------------------------------------
    sprintf( textQuery, "SELECT  %s FROM %s WHERE %s = '%s';",
                             "evKeyID", "tabEvent", "evEvent", TempKey);
    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery);
      #undef MELDUNG
    } // --------------------------------------------------------------

    if (mysql_query(con, textQuery))            // Abfrage
      showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);
    else // Abfrage erfolgreich, Rückgabedaten können verarbeitet werden
    {
      MYSQL_RES* result = mysql_store_result(con);  // Array-Buffer Ergebnismenge
      if (result)
      { // Es ist ein Ergebnis vorhanden
        // -----------------------------
        num_rows = mysql_affected_rows(con);    // Anzahl Zeilen in der Abfrage;
        { // --- Debug-Ausgaben --------------------------------------------------
          #define MELDUNG   "   %s()#%d: num_rows: <'%i'>\n"
          DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, num_rows);
          #undef MELDUNG
        } // ---------------------------------------------------------------------
      }
      mysql_free_result(result);                // Buffer freigeben
      TempZahl++;
    }
    strcpy(EventKey, TempKey);
  }
  while(num_rows > 0);  // wiederholen, bis es den Key nicht mehr gibt ===============

  retval = true;

  DEBUG_d("<- %s()#%d -<%d>-  : '%s' \n",  __FUNCTION__, __LINE__, retval, EventKey);
  return retval;
}
//************************************************************************************//

// Verzeichnis für Eventnummer anlegen
// -----------------------------------
// Rückgabe: Neu / schon vorhanden

#define MODUS  0774

int makeEventfolder(char* thisPfad, int EvNummer, char* Verzeichnis)
{
  DEBUG_m("=> %s()#%d: %s(%s, %d, %s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, thisPfad, EvNummer, Verzeichnis);
  int retval = FOLDER_NEW;
  mode_t oldMask = umask((mode_t) 0);
  umask(oldMask & ~(S_IWGRP|S_IXGRP));

  { // --- Debug-Ausgaben --------------------------------------------------------
    #define MELDUNG     "   %s()#%d: oldMask '%on', newMask '%on'\n"
    DEBUG_m(MELDUNG, __FUNCTION__, __LINE__, oldMask, oldMask ^ S_IXGRP);
    #undef MELDUNG
  } // ---------------------------------------------------------------------------

  sprintf(Verzeichnis, "%s%s%04d", thisPfad, _FOLDER, EvNummer);
  if(mkdir(Verzeichnis, MODUS) == -1)   // Fehler aufgetreten ...
  {
    if (errno == EEXIST)                        // ... der darf!
      retval = FOLDER_EXIST;
    else
    { // -- Error
      char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
      int errsv = errno;
      sprintf(ErrText, "Error Verzeichnis '%s' - = %i(%s)",
                        Verzeichnis, errsv, strerror(errsv));
      showMain_Error( ErrText,__FUNCTION__,__LINE__);
    }
  }
  umask(oldMask);
  DEBUG_m("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//************************************************************************************//

// Datei in das neue Verzeichnis verschieben
// -----------------------------------------
// über 'rename'
bool MoveFile( const char* File, const char* Verz)
{
  DEBUG("=> %s()#%d: %s(%s, \n                            %s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, File, Verz);
  char myDir[128];
  strcpy(myDir, Verz);
  char *ptr = strrchr(File, '/');
  strcat(myDir, ptr);
  bool status = true;

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "   %s()#%d: Datei: '%s',"\
    "\n                     alt: '%s',\n                     neu: '%s'\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, ptr, File, Verz);
    #undef MELDUNG
  } // --------------------------------------------------------------

  if( (rename(File, myDir)) < 0)
  { // -- Error
//    char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
//    int errsv = errno;
//    sprintf(ErrText, "Error rename '%s' ->  '%s'- = %i(%s)",
//                     File, Verz, errsv, strerror(errsv));
//    showMain_Error( ErrText,__FUNCTION__,__LINE__);
    status = false;
  }
  DEBUG("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// Verzeichnis löschen
// ---------------------------------------------
//  Foldername: ganzer Pfad
int deleteFolder(const char* Foldername)
{
  DEBUG("===> %s()#%d: %s('%s')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Foldername);

  char ErrText[ERRBUFLEN];
  int retval = 0;

 	// Verzeichnis muss leer sein: alle enthaltenen Dateien löschen
  // ------------------------------------------------------------
 	DIR* pdir = opendir(Foldername);
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    if (((*pdirzeiger).d_type) == DT_REG)
    { // reguläre Datei
      // --------------
      char Filename[ZEILE];                       // Pfad der Datei
      sprintf(Filename,"%s/%s", Foldername, (pdirzeiger)->d_name);
      if (remove(Filename) == 0)
      { // --- Debug-Ausgaben ------------------------------------------
        #define MELDUNG   "     %s()#%d: Datei: '%s' geloescht\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, Filename);
        #undef MELDUNG
        // --- Log-Ausgabe -------------------------------------------------------------------------
        char LogText[ZEILE];
        sprintf(LogText, "           --- '%s' gelöscht", Filename);
        MYLOG1a(LogText);
      } // -----------------------------------------------------------------------------------------
      else	
  		{ // -- Error
    		sprintf(ErrText, "remove '%s'", Filename);
    		Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    	}
    }
  }
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Foldername);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }
  
  // das Verzeichnis löschen
  // ------------------------------------
  if (rmdir(Foldername) == 0)
  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: Verzeichnis: '%s' geloescht\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, Foldername);
    #undef MELDUNG
    // --- Log-Ausgabe -------------------------------------------------------------------------
    char LogText[ZEILE];
    sprintf(LogText, "           --- '%s' gelöscht", Foldername);
    MYLOG1a(LogText);
  } // -----------------------------------------------------------------------------------------
  else	
  { // -- Error
    sprintf(ErrText, "rmdir '%s'", Foldername);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }

  DEBUG("<--- %s()#%d -<%d>- \n\n",  __FUNCTION__, __LINE__ , retval);

  return retval;
}
//***********************************************************************************************
//***********************************************************************************************
//
//
//***********************************************************************************************

// Datenbank anlegen
// ==================
MYSQL* CreateDBi(MYSQL* con)
{
  DEBUG_d("===> %s()#%d: %s(%ld) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__, (long int)con);
  // Connection initialisieren
  // -------------------------
  con = mysql_init(con);
  if (con == NULL)
    showMain_SQL_Error( "Init", __FUNCTION__, __LINE__, con);

  // User einloggen
  // --------------
#if __SQLMOTION_DEBUG__d
  syslog(LOG_NOTICE, "CreateDBi(%p) connect - Host:'%s', User:'%s', Passwort:'%s' \n", con, THISHOST, THISUSER, THISPW);
#endif
  if (mysql_real_connect(con, THISHOST, THISUSER, THISPW, NULL, 0, NULL, 0) == NULL)
    showMain_SQL_Error( "Connect", __FUNCTION__, __LINE__, con);

  // Datenbank erzeugen
  // ------------------
  char textCrea[100] = "CREATE DATABASE ";
  strcat(textCrea, MYDATABASE);
  if (mysql_query(con, textCrea))
  {
    if(mysql_errno(con) == ER_DB_CREATE_EXISTS)
    {
      // Fehler: 1007 - Kann Datenbank '%s' nicht erzeugen. Datenbank existiert bereits
    }
    else
    { // sonstiger Fehler
      // ----------------
      showMain_SQL_Error( textCrea, __FUNCTION__, __LINE__, con);
    }
  }

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: CREATE DATABASE %s: %p\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__,  MYDATABASE, con);
    #undef MELDUNG
  } // --------------------------------------------------------------

  DEBUG_d("<--- %s()#%d -<%ld>- \n",  __FUNCTION__, __LINE__,  (long int)con);
  return con;
}
//***********************************************************************************************

// Tabellen anlegen
// ================
void CreateTables(MYSQL* con)
{
  DEBUG_d("===> %s()#%d: %s(%ld) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__,  (long int)con);
  MYSQL_RES *res;
  char textQuery[1000];     // Buffer für Datenbankabfragen

  // Datenbank aktivieren
  // --------------------
  sprintf( textQuery, "USE %s;", MYDATABASE);
  if (mysql_query(con, textQuery))
    showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: %s: %p\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery, con);
    #undef MELDUNG
  } // --------------------------------------------------------------

  // Tabelle für Events prüfen
  // -------------------------------
  {
    sprintf( textQuery, "SELECT * FROM %s;", EVENTTABLE);
    mysql_query(con, textQuery);
    res = mysql_use_result(con);

    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "     %s()#%d: Test TABLE: %s - Error %i\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery, mysql_errno(con));
      #undef MELDUNG
    } // --------------------------------------------------------------

    if(mysql_errno(con) == NO_ERROR)
      {} // Tabelle vorhanden

    else if(mysql_errno(con) == ER_NO_SUCH_TABLE)
      showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);

    mysql_free_result(res);
  }

  DEBUG_d("<--- %s()#%d -< >- \n",  __FUNCTION__, __LINE__ );
}
//***********************************************************************************************

// Datensatz für Event anlegen ...
// ===============================
// thisEvent - Name des Events (Unterverzeichnis)
// FaDatum   - Datum  des Events
// thisSize  - Größe des Unterverzeichnisses
// Remark    - weitere Informationen
// ... und die ID des Datensatzes zurückgeben
int AddEvent(MYSQL* con, char* thisEvent, time_t* FaDatum, long thisSize, char* Remark)
{
  DEBUG_d("=> %s()#%d: %s(%ld, %s, %ld, %ld, %s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__,
             (long int)con, thisEvent, *FaDatum, thisSize, Remark);
  Startzeit(5);                       // Zeitmessung starten

  int PriID = 0;                      // PrimaryID des Datensatzes
  char textQuery[ZEILE];              // Buffer für Datenbankabfragen
  bool makeNewSet = false;            // Flag: neuen Datensatz anlegen
  char thisDatum[20] = {'\0'};        // String Datum
  char thisZeit[20] = {'\0'};         // String Zeit
  struct tm* ts = localtime(FaDatum); // Wandlung der Ereigniszeit
  #if _DEBUGzd
    long Zeitmarke(6);
  #endif

  sprintf(thisDatum, "%04d-%02d-%02d", 1900 + ts->tm_year, 1 + ts->tm_mon, ts->tm_mday);
  sprintf(thisZeit,  "%02d:%02d:%02d", ts->tm_hour, ts->tm_min, ts->tm_sec);

  // Prüfung, ob Datensatz schon vorhanden
  // -------------------------------------
  sprintf( textQuery, "SELECT %s FROM %s WHERE %s='%s' AND %s = '%s' AND %s = '%s' ORDER BY %s",
                        EVKEYID, EVENTTABLE, EVEVENT, thisEvent,
                        EVDATE, thisDatum, EVTIME, thisZeit, EVSAVED);

//#define  EVKEYID     "evKeyID"
//#define  EVSAVED     "evSaved"
//#define  EVEVENT     "evEvent"
//#define  EVDATE      "evDate"
//#define  EVTIME      "evTime"
//#define  EVSIZE      "evSize"
//#define  EVREMARK    "evRemark"

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "   %s()#%d: textQuery: %s\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery);
    #undef MELDUNG
  } // --------------------------------------------------------------

  if (mysql_query(con, textQuery))
    showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);
  else // Anfrage erfolgreich, Rückgabedaten können verarbeitet werden
  {
    MYSQL_RES *result;                // Array Ergebnismenge
    MYSQL_ROW row=0;                  // Array Datensatz
    result = mysql_store_result(con);
    if (result)
    { // Es ist ein Ergebnis vorhanden
      // -----------------------------
      unsigned int num_rows = mysql_affected_rows(con);             // Zeile in der Abfrage

      { // --- Debug-Ausgaben -----------------------------------------------------
        #define MELDUNG   "   %s()#%d: 2.num_rows: '%i', 3.num_fields: '%i'\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, num_rows, mysql_num_fields(result));
        #undef MELDUNG
      } // -----------------------------------------------------------------------------

      if( num_rows == 0 )
      { // es gibt noch keinen Datensatz mit diesem Event
        // ----------------------------------------------
        makeNewSet = true;
      }
      else
      { // ein Datensatz bereits vorhanden
        // -------------------------------
        row = mysql_fetch_row( result );

        { // --- Debug-Ausgaben ------------------------------------------
          #define MELDUNG   "   %s()#%d: Feld[0] = '%s'"
          DEBUG(MELDUNG, __FUNCTION__, __LINE__, row[0]);
          #undef MELDUNG
        } // --------------------------------------------------------------

        PriID = atoi(row[0]);         // der zuletzt gelesene (j_ngste) Wert gilt
      }
    }
    mysql_free_result(result);
  }

  if( makeNewSet )
  { // noch kein Eintrag - Datensatz einfügen
    // --------------------------------------
    MYSQL_RES *result;
    sprintf( textQuery, "INSERT INTO %s (%s, %s, %s, %s, %s) VALUES ('%s', '%s', '%s', '%ld', '%s')",
                        EVENTTABLE, EVEVENT, EVDATE, EVTIME, EVSIZE, EVREMARK,
                                    thisEvent, thisDatum, thisZeit, thisSize, Remark);

//#define  EVKEYID     "evKeyID"
//#define  EVSAVED     "evSaved"
//#define  EVEVENT     "evEvent"
//#define  EVDATE      "evDate"
//#define  EVTIME      "evTime"
//#define  EVSIZE      "evSize"
//#define  EVREMARK    "evRemark"

    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: textQuery = '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery);
      #undef MELDUNG
    } // --------------------------------------------------------------

    if (mysql_query(con, textQuery))
      {}//showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);
    else   // Datensatz erfolgreich angelegt
    {
      if ((result = mysql_store_result(con)) == 0 &&
          mysql_field_count(con) == 0 &&
          mysql_insert_id(con) != 0)
      {
        PriID = mysql_insert_id(con);
      }
    }
    mysql_free_result(result);

    char Logtext[ZEILE];
    sprintf(Logtext, "> neues Event: KeyID '%d' = '%s' @ %s %s\n", PriID, thisEvent, thisDatum, thisZeit);
    strcat(MailBody, Logtext);

    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: KeyID '%d' = '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, PriID, thisEvent);
      #undef MELDUNG
    } // --------------------------------------------------------------
  }

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "   %s()#%d: Datensatz-ID = '%i'\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, PriID);
    #undef MELDUNG
  } // --------------------------------------------------------------

  DEBUG_d("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__ , PriID);
  return(PriID);
}
//***********************************************************************************************

// Nachricht über FIFO senden
// --------------------------
bool toFIFO (char* inhalt)
{
  DEBUG_1("=> %s()#%d: %s(%s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, inhalt);
  int fd=0;
  int status=0;

  { // --- Debug-Ausgaben --------------------------------------------------------
    #define MELDUNG   "%s()#%d: open(%s, >%c<)\n"
    DEBUG_1(MELDUNG, __FUNCTION__, __LINE__, FIFO, (O_WRONLY | O_NONBLOCK));
    #undef MELDUNG
  } // ---------------------------------------------------------------------------

  fd = open(FIFO, O_WRONLY | O_NONBLOCK);       // Sender: Write Only
  if (fd < 0)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    if (errsv == 6)                             // 'No such device or address'
    { // 'FifoMotion' noch nicht bereit !
      // --------------------------------
      // nur Blinksignal
      #define ANZEIT  333 /* msec */
      for (int ix = 12; ix <=0; ix--)
      {
        digitalWrite (M_LED_ROT, LED_EIN);
        delay(ANZEIT);
        digitalWrite (M_LED_ROT, LED_AUS);
        delay(ANZEIT);
      }
      return false;
      #undef ANZEIT
    }
    else
    {
      sprintf(ErrText,"Error open('%s') = %i(%s)",
                    FIFO, errsv, strerror(errsv));
      showMain_Error( ErrText,__FUNCTION__,__LINE__);
    }
  }

  { // --- Debug-Ausgaben --------------------------------------------------------
    #define MELDUNG   "%s()#%d: '%s'"
    DEBUG_1(MELDUNG, __FUNCTION__, __LINE__, inhalt);
    #undef MELDUNG
  } // ---------------------------------------------------------------------------

  int lng = strlen(inhalt);
  status = write (fd, inhalt, lng + 1);     // <<< Übertragung
  if (lng > 0)
  { // --- Log-Ausgabe ----------------------------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "   ----->>> in FIFO übertragen: %d Bytes: \"%s\"",
                                                                                   lng, inhalt);
    MYLOG(LogText);
  } // --------------------------------------------------------------------------------------------

  if (status < 0)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText,"Error write('%s') = %i(%s)",
                    FIFO, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }
  DEBUG_1("<- %s()#%d -<0>- \n",  __FUNCTION__, __LINE__);
  return 0;
}
//***********************************************************************************************
//                                                                                              *
//                                    main()                                                    *
//                                                                                              *
//***********************************************************************************************

// Hilfsfunktion für Quicksort
// ---------------------------
struct Files{char Name[ZEILE]; int Lng;};

int compare(const void *lhs, const void *rhs)
{
  struct Files* left  = ((struct Files*) lhs);
  struct Files* right = ((struct Files*) rhs);
  
//  { // --- Log-Ausgabe -----------------------------------------------------------
//    char LogText[ZEILE];
//    sprintf(LogText, "              -- %s <--> %s", left->Name, right->Name );
//    MYLOG1(LogText);
//    sprintf(LogText, "                 %d <--> %d", left->Lng,  right->Lng );
//    MYLOG1(LogText);
//  } // ---------------------------------------------------------------------------

  if( left->Lng < right->Lng ) return  1;
  if( left->Lng > right->Lng ) return -1;
 
  return 0;
}
// ===============================================================================================

int main(int argc, char* argv[])
{
  char MailBody[4*ABSATZ] = {'\0'};
	char thisPfad[ZEILE] = {'\0'};                  // aktueller Verzeichnispfad

  sprintf (Version, "Vers. %d.%d.%d", MAXVERS, MINVERS, BUILD);
  printf("   %s %s von %s\n\n", PROGNAME, Version, __DATE__);

  // Verbindung zum Dämon Syslog aufbauen
  // -----------------------------------
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 );
  syslog(LOG_NOTICE, ">>>>> %s - %s/%s - PID %d - User %d, Group %d <<<<<<",
                  PROGNAME, Version, __DATE__, getpid(), geteuid(), getegid());
                  
  setAuxFolder(AUXDIR, TOPNAME);         		// Info an 'common'
  setAuxFolder_Err(AUXDIR);                	// Info an 'error'


  #include "/home/pi/treiber/snippets/get_progname.snip"


	// Aufrufparameter prüfen
	// -------------------------
  if (argc <= 1)
  {     // -- Error
    printf("   - Anzahl Argumente '%d'\n", argc);
    printf("   - Aufruf: \"%s\": Exit !\n", *argv);
    syslog(LOG_NOTICE, ">> %s()#%d -- Anzahl Argumente '%d': Exit!",
                                      __FUNCTION__, __LINE__, argc);
    {// --- Log-Ausgabe ------------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText,
         "<<< Anzahl Argumente '%d': Exit!",  argc);
      MYLOG(LogText);
    } // ---------------------------------------------------------------------------
    exit (EXIT_FAILURE);
  }

 
 	{	// Verzeichnispfad ermitteln
    // -------------------------
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-value"
    { 
      *argv++;
      char* pfad = thisPfad;
      char* ptr = (char*) *argv;
      char* end = strrchr(ptr, '/');
      if (end != NULL)
      {
        while (ptr <= end)
        {
          *pfad++ = *ptr++;
        }
      }
    	_DEBUG(">>> %s  %s()#%d: Daten-Verzeichnis: '%s'\n",  __Si__, thisPfad);
      *argv--;
    }
    #pragma GCC diagnostic pop
		_DEBUG(">>>\n");
	}
 
	{	// Argumentliste 
		// --------------
    for (int ix=0; ix < argc; ix++)
    {
      _DEBUG(">>> %s  %s()#%d: Argument %d: %s\n",  __Si__, ix, argv[ix]);
    }
    _DEBUG(">>> %s  %s()#%d: ------ Argumentlist done ------\n",  __Si__);
  	_DEBUG(">>>\n");
	}

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
  {	// LED-Mäusekino
    #define ANZEIT  111 /* msec */
    pinMode (M_LED_ROT,    OUTPUT);
    pinMode (M_LED_GELB,   OUTPUT);
    pinMode (M_LED_GRUEN,  OUTPUT);
    pinMode (M_LED_BLAU,   OUTPUT);
    pullUpDnControl (M_LED_ROT,   PUD_UP) ;
    pullUpDnControl (M_LED_GELB,  PUD_UP) ;
    pullUpDnControl (M_LED_GRUEN, PUD_UP) ;
    pullUpDnControl (M_LED_BLAU,  PUD_UP) ;
    digitalWrite (M_LED_GRUEN,  LED_EIN);
    digitalWrite (M_LED_BLAU,   LED_EIN);
    for (int ix=3; ix > 0; ix--)
    {
      digitalWrite (M_LED_ROT,  LED_EIN);
      digitalWrite (M_LED_GELB, LED_EIN);
      delay(ANZEIT);
      digitalWrite (M_LED_ROT,  LED_AUS);
      digitalWrite (M_LED_GELB, LED_AUS);
      delay(ANZEIT);
    }
    digitalWrite (M_LED_GRUEN,  LED_AUS);
    digitalWrite (M_LED_BLAU,   LED_AUS);
    #undef ANZEIT
	}

  // wenn keine Daten: hier beenden
  // -------------------------------
  if (argc <= 2)
  { // -- Error
    printf("   - Anzahl Argumente '%d': Exit!\n", argc);
    printf("   - Aufruf: %s'\n", *argv);
  	_DEBUG(">>> %s  %s()#%d: Anzahl Argumente '%d': Exit!\n",  __Si__, argc);   
    {// --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText,
         "<<< Anzahl Argumente '%d': Exit!",  argc);
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
    closelog();
    return EXIT_FAILURE;
  }


  // Zugriffsrechte
  // --------------
  int bits[] =
  {
    S_IRUSR,S_IWUSR,S_IXUSR,   /* Zugriffsrechte User    */
    S_IRGRP,S_IWGRP,S_IXGRP,   /* Zugriffsrechte Gruppe  */
    S_IROTH,S_IWOTH,S_IXOTH    /* Zugriffsrechte der Rest */
  };
  destroyInt(*bits);


  // Initialisierung abgeschlossen
  // ------------------------------
  #include "/home/pi/treiber/snippets/init_mail.snip"
  #include "/home/pi/treiber/snippets/init_done.snip"
  goto Phase_1;



Phase_1:
//* ===== Phase 1 =================================================================================
//*
//* alle Dateien im Verzeichnis nach Events sortieren und in Event-Verzeichnisse verschieben:
//*  while Dateizahl größer 0
//*  {
//*    avi-Datei finden
//*    daraus Event machen / Verzeichnis
//*    wenn bereits Event-Verzeichnis
//*       Event + 1
//*    alle jpg-Dateien dieses Events in das Verzeichnis
//*  }

  digitalWrite (M_LED_GELB, LED_AUS);
  digitalWrite (M_LED_GRUEN, LED_EIN);
  int newFolder = 0;                  // Verzeichniszähler
  int newFiles = 0;                   // Dateizähler

  { // --- Debug-Ausgaben ----------------------------------------------------------------------
    #define MELDUNG "\n%s()#%d:  ==== Phase 1: Dateien in '%s' ordnen ================================\n\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    #undef MELDUNG
  }// -------------------------------------------------------------------------------------------

  Startzeit(T_GESAMT);                // Zeitmessung über alles starten
  Startzeit(T_ABSCHNITT);             // Zeitmessung Phase 1 starten

  // alle Elemente im Verzeichnis durchlaufen
  // ----------------------------------------
  { // --- Debug-Ausgaben ------------------------------------------------------------------------
    #define MELDUNG   "%s()#%d: ---- alle Elemente im '%s' durchlaufen -----\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    #undef MELDUNG
  } // -------------------------------------------------------------------------------------------
  { // --- Log-Ausgabe ---------------------------------------------------------------------------
    char LogText[ZEILE];
    sprintf(LogText, "      --- Phase  1: alle Elemente im '%s' durchlaufen", thisPfad);
    MYLOG1(LogText);
  } // -------------------------------------------------------------------------------------------
  
  char** DateiListe = argv;                     // die übergebene Dateiliste

  while(*++DateiListe)
  {
    char* thisDatei = *DateiListe;              // eine einzelne Datei
    int thisFiletype = getFiletyp(thisDatei);

    { // --- Log-Ausgabe -------------------------------------------------------------------------
      char LogText[ZEILE];
      sprintf(LogText, "         --- %s", thisDatei);
      MYLOG1a(LogText);
    } // -----------------------------------------------------------------------------------------

    // um Konflikte zwischen 'pi' und 'motion' zu vermeiden
    // ----------------------------------------------------
    if ((thisFiletype == AVI) || (thisFiletype == MKV) || (thisFiletype == JPG))
    {
      chmod(thisDatei, S_IRUSR+S_IWUSR+S_IXUSR+S_IRGRP+S_IWGRP+S_IROTH);
    }

    // nächste Filmdatei suchen (gibt Ereignis-Nummer vor)
    // ----------------------------------------------------
    if ((thisFiletype == AVI) || (thisFiletype == MKV))
    {
      // Dies ist ein Film: Ereignis-Nummer ermitteln
      // --------------------------------------------
      int thisEventnummer = getEventNo(thisDatei);

      { // -----------------------------------------------------------
        #define MELDUNG   "\n%s()#%d: - Eventnummer '%s': %d\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisDatei, thisEventnummer);
        #undef MELDUNG
      } // -----------------------------------------------------------
      { // --- Log-Ausgabe -----------------------------------------------------------------------
        char LogText[ZEILE];
        sprintf(LogText, "         --- Eventnummer '%d'", thisEventnummer);
        MYLOG1(LogText);
      } // ---------------------------------------------------------------------------------------

      // ein neues Verzeichnis anlegen
      // -----------------------------
      char thisFolder[ZEILE];                   // neues Verzeichnis
      int Eventnummer = thisEventnummer;
      while (makeEventfolder(thisPfad, Eventnummer, thisFolder) == FOLDER_EXIST)
      {
        Eventnummer += 1;
      }
      newFolder++;
      // alle Dateien mit diesem Ereignis in dieses Verzeichnis verschieben
      // -------------------------------------------------------------------
      int FCnt = 0;
      char** DateiListe = argv;                 // nochmals die übergebene Dateiliste
      while(*++DateiListe)
      {
        char* myDatei = *DateiListe;            // eine einzelne Datei
        int myEventnummer = getEventNo(myDatei);
        if (myEventnummer == thisEventnummer)
        {
          
          { // -----------------------------------------------------------
            #define MELDUNG   "%s()#%d: '%s' -> '%s'\n"
            DEBUG(MELDUNG, __FUNCTION__, __LINE__, myDatei, thisFolder);
            #undef MELDUNG
          } // -----------------------------------------------------------
          { // --- Log-Ausgabe -----------------------------------------------------------------------
            char LogText[ZEILE];
            sprintf(LogText, "            --- '%s' -> '%s'", myDatei, thisFolder);
            MYLOG1a(LogText);
          }

          errno = 0;
          if (MoveFile( myDatei, thisFolder))                   // Datei verschieben
          { // --- Log-Ausgabe -----------------------------------------------------------------------
            newFiles++;
            FCnt++;
          }
          else
          { // --- Log-Ausgabe ----------------------------------------------------------------------
            char LogText[ZEILE];
            sprintf(LogText, "            --- '%s' -> '%s': Error %d(%s)",
                                              myDatei, thisFolder, errno, strerror(errno));
            MYLOG(LogText);
          } // ----------------------------------------------------------------------------------------
        }
      }
      
      // alle Dateien dieses Ereignisses stehen jetzt in einem Verzeichnis
      // -----------------------------------------------------------------
      // Sie werden nach Länge sortiert und nur die  'n' größten behalten
      {
        { // --- Log-Ausgabe -----------------------------------------------------------------------
          char LogText[ZEILE];
          sprintf(LogText, "            --- %d Dateien in '%s'", FCnt, thisFolder);
          MYLOG1(LogText);
        }
        struct Files Liste[FCnt+2];
        int files = 0;
        DIR* udir = opendir(thisFolder);                  // das Eventverzeichnis öffnen
        struct dirent* udirzeiger;
        while((udirzeiger=readdir(udir)) != NULL)         // Zeiger auf den Inhalt diese Unterverzeichnisses
        {
          char LogText[ZEILE];
          char myFilename[ZEILE];
          char longFilename[ZEILE];
          unsigned long myFileSize = 0;                   // Dateilänge
          struct stat myAttribut;
          strcpy(myFilename, (*udirzeiger).d_name);
          sprintf(longFilename,"%s/%s", thisFolder, myFilename);
          if (stat(longFilename, &myAttribut) == -1)      // Datei-Attribute holen
          { // -- Error
            sprintf(LogText,"Lesefehler Datei '%s'!",  myFilename);
            MYLOG1(LogText);
            continue;
          }
          if (S_ISREG(myAttribut.st_mode))
          {
            myFileSize = myAttribut.st_size;              // Dateilänge
            { // --- Log-Ausgabe -----------------------------------------------------------
              sprintf(LogText, "              --- '%s': Länge %ld", myFilename, myFileSize);
              MYLOG1a(LogText);
            } // ---------------------------------------------------------------------------
            strcpy(Liste[files].Name, longFilename);
            Liste[files].Lng = myFileSize;
            files++;
          }
          else
          { // --- Log-Ausgabe -----------------------------------------------------------
            sprintf(LogText, "              --- '%s' is not a file", myFilename);
            MYLOG1a(LogText);
          } // ---------------------------------------------------------------------------
        }
        
        { // --- Log-Ausgabe -----------------------------------------------------------
          char LogText[ZEILE];
          sprintf(LogText, "                -- nach Dateilänge sortieren --");
          MYLOG1(LogText);
        } // ---------------------------------------------------------------------------
        
        qsort(Liste, files, sizeof(struct Files), compare); // nach Dateilänge sortieren
        
        for (int ix=0; ix < files; ix++)
        { // --- Log-Ausgabe -----------------------------------------------------------
          char LogText[ZEILE];
          sprintf(LogText, "                -- %2d: -- '%s': Länge %d", ix, Liste[ix].Name, Liste[ix].Lng);
          MYLOG1a(LogText);
        } // ---------------------------------------------------------------------------

        { // --- Log-Ausgabe -----------------------------------------------------------
          char LogText[ZEILE];
          sprintf(LogText, "                -- Überschuss löschen --");
          MYLOG1(LogText);
        } // ---------------------------------------------------------------------------
        
        for (int ix=5; ix < files; ix++)
        { 
          remove(Liste[ix].Name);
          
          // --- Log-Ausgabe -----------------------------------------------------------
          char LogText[ZEILE];
          sprintf(LogText, "                -- %2d: -- '%s': gelöscht", ix, Liste[ix].Name);
          MYLOG1a(LogText);
        } // ---------------------------------------------------------------------------       
      }
      { // -----------------------------------------------------------
        #define MELDUNG   "%s()#%d: --  Eventnummer '%d' fertig --'\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisEventnummer);
        #undef MELDUNG
      } // -----------------------------------------------------------
    }
  }
// ===== Phase 1 beendet===========================================================================
  {
    char LogText[ZEILE];
    sprintf(LogText, "      --- Phase 1: fertig %d Dateien -> %d neue Verzeichnisse in %ld msec",
                                               newFiles, newFolder, Zwischenzeit(T_ABSCHNITT));
    MYLOG(LogText);
  }


  #if BREAKmain1
  { // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n\n%s()#%d:                           ------< weiter mit ENTER >------\n", __FUNCTION__, __LINE__);
    char dummy;
    scanf ("%c", &dummy);

  }
  #endif
  goto Phase_2;




Phase_2:
//* ===== Phase 2 ==================================================================================
//*
//* jetzt nochmal alle Dateien in allen Verzeichnissen durchsuchen:
//*
//*  alle Verzeichnisse
//*  {
//*    alle Dateien
//*    {
//*      zu kleine Dateien löschen
//*      Dateien zählen
//*      Speicher aufsummieren
//*    }
//*    neue Eventnummer = PrimID Datenbank
//*    Verzeichnis als Event in Datenbank
//*  }

  Startzeit(T_ABSCHNITT);             // Zeitmessung Phase 2 starten
  float SizeTotal = 0;                // gesamte Speicherbelegung

  { // --- Debug-Ausgaben ------------------------------------------------------------------------
    #define MELDUNG "\n%s()#%d: ==== Phase 2: nochmal alle Verzeichnisse in '%s' durchsuchen ========\n"
    DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    #undef MELDUNG
  }
  // -------------------------------------------------------------------------------------------
  { // --- Log-Ausgabe ---------------------------------------------------------------------------
    char LogText[ZEILE];
    sprintf(LogText, "      --- Phase 2: alle Verzeichnisse im '%s' durchsuchen", thisPfad);
    MYLOG2(LogText);
  } // -------------------------------------------------------------------------------------------

  DIR* pdir = opendir(thisPfad);                // das oberste Verzeichnis
  struct dirent* pdirzeiger;
  if (pdir == NULL)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText,"Error Verzeichnis '%s': %i(%s)",
                    thisPfad, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }

  // alle Unter-Verzeichnisse durchsuchen
  // ------------------------------------
  // int cnt = 0; --> siehe '#if __SQLMOTION_DEBUG__2'
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char myPath[ZEILE];                                   // Name des aktuellen Unterverzeichnisses
    struct stat myAttribut;
    sprintf(myPath, "%s%s", thisPfad, (*pdirzeiger).d_name);
    if (stat(myPath, &myAttribut) == -1)                  // Datei-Attribute holen
    { // -- Error
      if (errno != 2)                                     // Err 2-No such file or directory
      {
        char ErrText[ERRBUFLEN];                          // Buffer für Fehlermeldungen
        sprintf(ErrText,"Attribut '%s'", myPath);
        Error_NonFatal( ErrText,__FUNCTION__,__LINE__);
      }
      continue;
    }
    unsigned long myFileSize = myAttribut.st_size;        // Dateilänge
    { // --- Debug-Ausgaben ----------------------------------------------
      #define MELDUNG "\n%s()#%d: %d. ---  '%s': %ld Bytes ---\n"
      DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, cnt++, myPath, myFileSize);
      #undef MELDUNG
      // -----------------------------------------------------------------
    }
//    { // --- Log-Ausgabe ---------------------------------------------------------------------------
//      char LogText[ZEILE];
//      sprintf(LogText, "        --- '%s': %ld Bytes", myPath, myFileSize);
//      MYLOG2(LogText);
//    } // -------------------------------------------------------------------------------------------
    SizeTotal += myFileSize;

    if (strstr(myPath, _FOLDER) != NULL)                  // ist dies ein ("event_")Eventverzeichnis?
    { // -- ja! Dies ist ein Eventverzeichnis
      // -------------------------------------

      { // --- Debug-Ausgaben ----------------------------------------------
        #define MELDUNG "\n%s()#%d: --- Verzeichnis '%s' durchsuchen ---\n"
        DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, myPath);
        #undef MELDUNG
      // ------------------------------------------------------------------
      }
      { // --- Log-Ausgabe ---------------------------------------------------------------------------
        char LogText[ZEILE];
        sprintf(LogText, "          --- Eventverzeichnis '%s' durchsuchen", myPath);
        MYLOG2(LogText);
      } // -------------------------------------------------------------------------------------------

      Startzeit(T_FOLDER);            // Zeitmessung starten
      DIR* udir = opendir(myPath);    // das Eventverzeichnis öffnen
      struct dirent* udirzeiger;
      if (udir == NULL)
      { // -- Error
        char ErrText[ERRBUFLEN];      // Buffer für Fehlermeldungen
        int errsv = errno;
        sprintf(ErrText,"Error Verzeichnis '%s': %i(%s)",
                        myPath, errsv, strerror(errsv));
        showMain_Error( ErrText,__FUNCTION__,__LINE__);
      }
      // alle Dateien in diesem Eventverzeichnis durchsuchen
      // -----------------------------------------------------
      unsigned long SizeFolder=0;               // Speicherbelegung dieses Unterverzeichnisses
      int cntJPGs = 0;                          // Anzahl Bilder
      int cntAVIs = 0;                          // Anzahl Filme (AVI und MKV)
      time_t EventDatum = -99999999;            // Ereignisdatum: das höchst Dateidatum
      while((udirzeiger=readdir(udir)) != NULL) // Zeiger auf den Inhalt diese Unterverzeichnisses
      {
        Startzeit(T_FILES);                     // Zeitmessung starten
        char myFilename[ZEILE];
        strcpy(myFilename, (*udirzeiger).d_name);
        int myType   = (*udirzeiger).d_type;

        { // --- Debug-Ausgaben ------------------------------------------------------------------------
          #define MELDUNG   "%s()#%d: Datei '%s' untersuchen\n"
          DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, myFilename);
          #undef MELDUNG
          DEBUG_2("             reclen    %d\n",  (*udirzeiger).d_reclen);
          DEBUG_2("             type      %d\n",  myType);
          DEBUG_2("             offset    %d\n",  (int)(*udirzeiger).d_off);
          DEBUG_2("             inode     %d\n",  (int)(*udirzeiger).d_ino);
          // file attributes such as size, modification times etc., are part of the file itself,
          // not of any particular directory entry. See File Attributes.
        } // ----------------------------------------------------------------------

//        { // --- Log-Ausgabe ---------------------------------------------------------------------------
//          char LogText[ZEILE];
//          sprintf(LogText, "          --- '.../%s' untersuchen", myFilename);
//          MYLOG2(LogText);
//        } // -------------------------------------------------------------------------------------------
//
        if (myType == DT_REG)           //  struct dirent ('dirent.h'): A regular file.
        { // reguläre Datei
          // --------------
          struct stat myAttribut;
          char longFilename[ZEILE];
          sprintf(longFilename,"%s/%s", myPath, myFilename);
          if (stat(longFilename, &myAttribut) == -1)      // Datei-Attribute holen
          { // -- Error
            char ErrText[ERRBUFLEN];                      // Buffer für Fehlermeldungen
            sprintf(ErrText,"Lesefehler Datei '%s'!",
                            longFilename);
            showMain_Error( ErrText,__FUNCTION__,__LINE__);
          }

          time_t myFileDatum = myAttribut.st_mtime;       // Datum der Datei
            DEBUG_2("             FileDate  %ld\n",  myFileDatum);
          unsigned long myFileSize = myAttribut.st_size;  // Dateilänge
            DEBUG_2("             FileSize  %ld\n",  myFileSize);

          // etwas Statistik
          // ---------------------------------------
          int myType = getFiletyp(longFilename);
            DEBUG_2("             FileType  %d\n",  myType);
            DEBUG_2("              minimum  %d\n",  MIN_FILESIZE);
            DEBUG_2("                 JPG?  %d\n",  JPG);
          EventDatum = myFileDatum > EventDatum ? myFileDatum : EventDatum;
          SizeFolder += myFileSize;
          if (myType == JPG) cntJPGs++;
          else if (myType == AVI) cntAVIs++;
          else if (myType == MKV) cntAVIs++;


          // zu kurze Dateien aussortieren
          // ---------------------------------------
          if ((myFileSize < MIN_FILESIZE) && (myType == JPG))

          { // Bilddatei zu klein
            // ------------------
            { // --- Debug-Ausgaben ---------------------------------------------------
              #define MELDUNG   "%s()#%d: -- Datei '%s' zu kurz: %ld! \n"
              DEBUG_2(MELDUNG, __FUNCTION__, __LINE__,  longFilename, myFileSize);
              #undef MELDUNG
            } // ----------------------------------------------------------------------
            { // --- Log-Ausgabe ---------------------------------------------------------------------------
              char LogText[ZEILE];
              sprintf(LogText, "           --- '%s' zu kurz: %ld (min: %d)",
                                                  longFilename, myFileSize, MIN_FILESIZE);
              MYLOG2(LogText);
            } // -------------------------------------------------------------------------------------------

            // Datei löschen
            // ------------*
            remove(longFilename);
          }


          else if ((myFileSize < MIN_FILMSIZE) && (myType == MKV))
          { // Filmdatei zu klein
            // ------------------
            { // --- Debug-Ausgaben ---------------------------------------------------
              char ErrText[ERRBUFLEN];                      // Buffer für Fehlermeldungen
              #define MELDUNG   "%s()#%d: -- Datei '%s' zu kurz: %ld!\n"
              DEBUG_2(MELDUNG, __FUNCTION__, __LINE__,  longFilename, myFileSize);
              sprintf(ErrText, MELDUNG, __FUNCTION__, __LINE__,  longFilename, myFileSize);
              syslog(LOG_NOTICE, ErrText);
              #undef MELDUNG
            } // ----------------------------------------------------------------------
            { // --- Log-Ausgabe ---------------------------------------------------------------------------
              char LogText[ZEILE];
              sprintf(LogText, "             --- '%s'  zu kurz: %ld (min: %d)",
                                                  longFilename, myFileSize, MIN_FILMSIZE);
              MYLOG2(LogText);
            } // -------------------------------------------------------------------------------------------

            // Datei löschen
            // ------------*
            remove(longFilename);
          }
          else
          {
            { // --- Log-Ausgabe ---------------------------------------------------------------------------
              char LogText[ZEILE];
              sprintf(LogText, "           --- '%s'    Lng.:%ld - OK", longFilename, myFileSize);
              MYLOG2(LogText);
            } // -------------------------------------------------------------------------------------------
            SizeTotal += myFileSize;
          }
        }
        else
        { // keine reguläre Datei
          // ---------------------
          SizeTotal += myFileSize;
          DEBUG_2("             .........%s\n", myFilename);
        } // ---- Ende reguläre Datei
      }  // --- Ende Eventverzeichnis durchsuchen T_FILES


      { // --- Zeitmessung Dateien prüfen ---------------------------------------
        #define MELDUNG   "%s()#%d: ------ Zwischenzeit >Dateien pruefen<: '%ld' msec ------\n"
        DEBUG_z(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_FILES));
        #undef MELDUNG
      } // ---------------------------------------------------------------

      if (cntAVIs == 1)
      {
        // das Eventverzeichnis in der Datenbank vermerken -----------------------------------
        // ------------------------------------------------
        // es enthält einen Film und 0...n Bilder
        struct stat dAttribut;
        char EventKey[ZEILE] = {'\0'};                              // Eventname
        char Remark[ZEILE] = {'\0'};                                // Bemerkung

        Startzeit(T_DBASE);                                         // Zeitmessung starten

        if(stat(myPath, &dAttribut) == -1)
        { // -- Error
          char ErrText[ERRBUFLEN];                                  // Buffer für Fehlermeldungen
          sprintf(ErrText,"Verzeichnis '%s' nicht lesbar", myPath);
          showMain_Error( ErrText,__FUNCTION__,__LINE__);
        }

        int PrimaryID = -1;                                         // PrimärID des Datensatzes
        sprintf(Remark, "JPGs=%i - AVIs=%i",                        // Bemerkungs-Text
                        cntJPGs,  cntAVIs);
        { // --- Debug-Ausgaben ----------------------------------------------
          #define MELDUNG "%s()#%d: \n"
          DEBUG_2(MELDUNG, __FUNCTION__, __LINE__);
          #undef MELDUNG
          // -----------------------------------------------------------------
        }
        if (getEventKey(con, myPath, EventKey) == true)             // Eventnamen für Schlüssel erzeugen
        { // in die Datenbank eintragen
          // ----------------------------
          { // --- Debug-Ausgaben ----------------------------------------------
            #define MELDUNG "%s()#%d: === in die Datenbank ===> EventKey: '%s'\n"
            DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, EventKey);
            #undef MELDUNG
            // -----------------------------------------------------------------
          }
          PrimaryID = AddEvent(con, EventKey, &EventDatum, SizeFolder, Remark);
          { // Eventkey als .info-Datei
            // -------------------------
            FILE *Datei;
            char KeyFile[ZEILE];
            sprintf(KeyFile,"%s/%s.info", myPath, EventKey);
            Datei = fopen (KeyFile, "w");
            fprintf(Datei, "%s %s\n", PROGNAME, Version);
            fprintf(Datei, "   Source = %s\r\n", argv[1]);
            fprintf(Datei, "PrimaryID = %d\r\n", PrimaryID);
            fprintf(Datei, " EventKey = '%s'\r\n", EventKey);
            fclose (Datei);
          }
          { // --- Log-Ausgabe ---------------------------------------------------------------------------
            char LogText[ZEILE];
            sprintf(LogText, "          --- '%s' --> %s#%d ", EventKey, MYDATABASE, PrimaryID);
            MYLOG2(LogText);
          } // -------------------------------------------------------------------------------------------

        }
        else
        { // -- Error
          char ErrText[ERRBUFLEN];                                    // Buffer für Fehlermeldungen
          sprintf(ErrText,"Error '%s' --> '%s'", myPath, EventKey);
          showMain_Error( ErrText,__FUNCTION__,__LINE__);
        }



        { // --- Debug-Ausgaben ----------------------------------------------
          #define MELDUNG "%s()#%d: \n"
          DEBUG_2(MELDUNG, __FUNCTION__, __LINE__);
          #undef MELDUNG
          // -----------------------------------------------------------------
        }

        // aus der PrimärID einen neuen Event-Namen erzeugen
        // -------------------------------------------------
        // ... und damit 'myPath' verschieben
        {
          char neuerEventName[ZEILE] = {'\0'};
          sprintf(neuerEventName, "%s/%s%d", thisPfad, _EVENT_, PrimaryID);
          if( (rename(myPath, neuerEventName)) < 0)
          { // -- Error
            char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
            int errsv = errno;
            sprintf(ErrText, "Error rename '%s' ->  '%s'- = %i(%s)",
                             myPath, neuerEventName, errsv, strerror(errsv));
            showMain_Error( ErrText,__FUNCTION__,__LINE__);
          }
          { // --- Log-Ausgabe ---------------------------------------------------------------------------
            char LogText[ZEILE];
            sprintf(LogText, "          --- '%s' ===> '%s", myPath, neuerEventName);
            MYLOG2(LogText);
          } // -------------------------------------------------------------------------------------------
        }
      }  // ---<<< ist dies ein Eventverzeichnis? >>>-------------------
      else
      {	// kein vollständiges Eventverzeichnis: Löschen!
      	// ----------------------------------------------
	 			deleteFolder(myPath);
      }
    }     
  }

  // Lesezeiger wieder schließen
  // ---------------------------
  if(closedir(pdir) == -1)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText,"Error Verzeichnis '%s' - = %i(%s)",
                    thisPfad, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }

//  { // --- Debug-Ausgaben ---------------------------------------
//    #define MELDUNG   "\n%s()#%d: ==== Phase 2 fertig in %ld msec ---------------------------------------------------------\n"
//    DEBUG_z(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_ABSCHNITT));
//    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_ABSCHNITT));
//    #undef MELDUNG
//    { // --- Log-Ausgabe ---------------------------------------------------------
//      char LogText[ZEILE];  sprintf(LogText,
//         "    Data ready in %ld msec!", Zwischenzeit(T_ABSCHNITT));
//      MYLOG(LogText);
//    } // ------------------------------------------------------------------------
//  } // ----------------------------------------------------------

  mysql_close(con);                             // Datenbank schließen

// ===== Phase 2 beendet===========================================================================
  double calcZeit = (double)Zwischenzeit(T_ABSCHNITT) / 1000.0;
  {
    char LogText[ZEILE];
    sprintf(LogText, "      --- Phase 2: fertig in %2.3f sec", calcZeit);
    MYLOG(LogText);
  }
  double dbSize = (double)SizeTotal * 1.0;
  double usedSize = dbSize/(double)((unsigned long)GBYTE);

  { // --- Debug-Ausgaben ---------------------------------------------------------------------
    #define MELDUNG   "\n    %s()#%d: -- belegter Speicher: %2.3f MBytes in %2.3f sec --\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, usedSize, calcZeit);
    #undef MELDUNG
    {
      char LogText[ZEILE];
      sprintf(LogText, "      --- belegter Speicher: %2.3f GB ", usedSize);
      MYLOG(LogText);
    }
  } // ----------------------------------------------------------------------------------------
  UNUSED(usedSize);
  UNUSED(calcZeit);



  char Logtext[2*ZEILE];
	char sTotal[50] = {'\0'};
  sprintf(sTotal, "%3.1f MB", (SizeTotal+((1024*1024)/2))/(1024*1024));
  sprintf(Logtext, ">>> %s()#%d: Speicherbelegung '%s': %s\n", __FUNCTION__,__LINE__, thisPfad, sTotal);
  SYSLOG(LOG_NOTICE, Logtext);
  strcat(MailBody, Logtext);

  { // --- Debug-Ausgaben ---------------------------------------
    #define MELDUNG   "%s()#%d: Speicherbelegung '%s': %s\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad, sTotal);
    #undef MELDUNG
  } // ----------------------------------------------------------

  #if BREAKmain2
  { // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n%s()#%d:                           ------< weiter mit ENTER >------\n", __FUNCTION__, __LINE__);
    char dummy;
    scanf ("%c", &dummy);
  }
  #endif

//* ===== Phase 3 ==================================================================================
//*
//* die Ereignisse in den externen Buffer übertragen:
//*  die Daten werden über einen Fifo weitergereicht.
//*

  { // --- Debug-Ausgaben ------------------------------------------------------------------------
    #define MELDUNG "\n%s()#%d: ==== Phase 3: Dateien per Fifo übertragen =============================================\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__);
    #undef MELDUNG
  }

  Startzeit(T_ABSCHNITT);             // Zeitmessung Phase 3 starten

  digitalWrite (M_LED_GRUEN, LED_AUS);
  digitalWrite (M_LED_BLAU, LED_EIN);

  toFIFO(thisPfad);                   // >>>>>>>>> über FIFO (named pipe) senden >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  digitalWrite (M_LED_ROT, LED_AUS);
  SYSLOG(LOG_NOTICE, ">>>>>> %s()#%d: %s --- fertig in %ld msec   ------------------------ <<<<<<", 
                     __FUNCTION__,__LINE__, PROGNAME, Zwischenzeit(T_GESAMT));
  { // --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText,
       "<<< fertig in %ld msec!", Zwischenzeit(T_GESAMT));
    MYLOG(LogText);
  } // ------------------------------------------------------------------------


  { // --- Debug-Ausgaben ---------------------------------------
    #define MELDUNG   "%s()#%d: ---- Phase 3 fertig in %ld msec ---------------------------------------------------------\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_ABSCHNITT));
    #undef MELDUNG
    #define MELDUNG   "%s()#%d: ---- Gesamtzeit: %ld msec -----\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_GESAMT));
    #undef MELDUNG
  } // ----------------------------------------------------------


  // den Watchdog füttern
  // ------------------------
  feedWatchdog(PROGNAME);

  // PID-Datei wieder löschen
  // ------------------------
  killPID();

  closelog();
  return EXIT_SUCCESS;
}
//************************************************************************************//
