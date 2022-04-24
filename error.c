//*********************************************************************************************//
//*                                                                                           *//
//* File:          error.c                                                                    *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2018-07-14;                                                                *//
//* Last change:   2022-04-23 - 15:52:15                                                     *//
//* Description:   Nistkastenprogramm: Standard-Fehlerausgänge                                *//
//*                                                                                           *//
//* Copyright (C) 2018-21 by Wolfgang Keuch                                                   *//
//*                                                                                           *//
//*      2022-03-22; 'sendmail' für Inbetriebnahme abgeschaltet                               *//
//*                                                                                           *//
//*********************************************************************************************//

#include <stdio.h>
#include <mysql.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <stdbool.h>

#include <fcntl.h>           /* Definition of AT_* constants */
#include <sys/stat.h>

#include "./error.h"
#include "./datetime.h"
#include "../sendmail/sendMail.h"

EXTERN bool DBError_Flag;
EXTERN int  DBError_Count;

char Uhrzeitbuffer[TIMLEN];

#undef      MIT_DISPLAY

//#define _DEBUG

//*********************************************************************************************//

// Meldung in Fehlerliste eintragen
// ---------------------------------
// 
void Fehlerliste(char* Message)
{
  #define  FEHLER_LISTE "/home/pi/motion/aux/errors"
  #define   LZA  "\n ---------- %s ---------------------------- \n"
  #define   LZE  "\n ---------- %s - %s --------------- \n"
  char buf[256] = {'\0'};
  char lza[256] = {'\0'};
  char lze[256] = {'\0'};
  
  mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;  
  int fd_list = open(FEHLER_LISTE, O_CREAT | O_APPEND | O_RDWR, mode);
  if(fd_list == -1) 
  { // --- Debug-Ausgaben ---------------------------------------------------
    perror("open FEHLER_LISTE");
  } // ----------------------------------------------------------------------
  else
  { // Attribute setzen
    // -----------------
    if(fchmod(fd_list, mode) == -1) 
    { // --- Debug-Ausgaben ---------------------------------------------------
      perror("fchmod FEHLER_LISTE");
    } // ----------------------------------------------------------------------
    else
    { // Inhalte schreiben
      // -----------------
      char today[12]; sprintf(today, "%s", __HEUTE__);
      char   now[12]; sprintf(  now, "%s", __NOW__);
      sprintf(lza, LZA, PROGNAME);
      sprintf(lze, LZE, today, now);
      sprintf(buf, "%s(%d)-%s%s", lza, strlen(Message), Message, lze);
      
      if(write(fd_list, buf, sizeof(buf)) != sizeof(buf))
      { // --- Debug-Ausgaben ---------------------------------------------------
        perror("write FEHLER_LISTE");
      } // ----------------------------------------------------------------------
      // Log-Datei schließen
      // -------------------
      close(fd_list);    
    }
  }
}
//*********************************************************************************************//

#define FATAL_MESSAGE        "Programm-Abbruch in '%s':\n%s\n-wkh-\n"
#define NONFATAL_MESSAGE     "Fehler in '%s':\n%s\n-wkh-\n"

//*********************************************************************************************//

// Fehlermeldung ausgeben
// ------------------------
void show_Error(char* ErrorMessage, char* MailMaske)
{
  char Header[SUBJECT] = {'\0'};
  char MailBody[BODYLEN] = {'\0'};

  char ErrText[ERRBUFLEN]={'\0'};
  sprintf(ErrText, "Error! <%s>", ErrorMessage);
  sprintf(ErrText, "%s<%s>", strlen(MailMaske) == 0 ? "" : "Error! ", ErrorMessage);
  syslog(LOG_NOTICE, ErrText);                            // im Log vermerken

  if (strlen(MailMaske) > 0)
  {
    // Fehler-Mail abschicken
    // ----------------------
    sprintf(Header, "Error in >%s<", PROGNAME);
    Header[SUBJECT-1]='\0';                               // Begrenzung
    #ifdef _DEBUG                                         // Fehler ausgeben
    fprintf(stdout, "-- %s()#%d - Header: '%s'\n", __FUNCTION__, __LINE__, Header);
    #endif

    sprintf(MailBody, MailMaske, PROGNAME, ErrorMessage);
    MailBody[BODYLEN-1] = '\0';                           // Begrenzung
    #ifdef _DEBUG                                         // Fehler ausgeben
    fprintf(stdout, "-- %s()#%d - Body: >>>\n%s\n<<<\n", __FUNCTION__, __LINE__, MailBody);
    fprintf(stdout, "-- ----------------------------------------------\n");
    #endif

    sendmail(Header, MailBody);                           // Mail-Message absetzen
  }
}
//*********************************************************************************************//

// Fatale Fehlermeldung
// ------------------------
// Programm wird beendet

void finish_with_Error(char* ErrorMessage)
{

#ifdef _DEBUG                                             // Fehler ausgeben
  fprintf(stdout, "\n-- ---------- finish_with_Error() ---------------\n");
  fprintf(stdout, "-- %s()#%d: '%s'\n", __FUNCTION__, __LINE__, ErrorMessage);
#endif

  // in Fehlerliste eintragen
  // ------------------------
  Fehlerliste(ErrorMessage);

  // Fehlerausgabe
  // --------------
  show_Error(ErrorMessage, FATAL_MESSAGE);
  syslog(LOG_NOTICE, "Exit!");                            // im Log vermerken
  closelog();                                             // Verbindung zum Dämon Syslog schließen

#ifdef MIT_DISPLAY
  // Damit die Anzeige erhalten bleibt: Dauerschleife
  // ------------------------------------------------
  DO_FOREVER
  {
    sleep(1000);
  }
#endif

  exit(EXIT_FAILURE);
}
//*********************************************************************************************//

// Nicht-Fatale Fehlermeldung
// ------------------------
// Fehler wird gemeldet, Programm läuft weiter

void report_Error(char* ErrorMessage, bool withMail)
{
  // in Fehlerliste eintragen
  // ------------------------
  Fehlerliste(ErrorMessage);

  // Fehlerausgabe
  // --------------
  {
    if (withMail)
      show_Error(ErrorMessage, NONFATAL_MESSAGE);
    else
      show_Error(ErrorMessage, "");
  }
}
//*********************************************************************************************//
