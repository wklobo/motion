//*********************************************************************************************//
//*                                                                                           *//
//* File:          error.h                                                                    *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2018-07-14;                                                                *//
//* Last change:   2022-04-18 - 16:54:51                                                      *//
//* Description:   SolarControl - Überwachung und Messung des Solarmoduls                     *//
//*                Standard-Fehlerausgänge                                                    *//
//*                                                                                           *//
//* Copyright (C) 2018 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef _ERROR_H
#define _ERROR_H  1

#define ERRBUFLEN      1024
#define ZEITBUFLEN       64

#define FEHLERZEIT      12    /* Brenndauer rote LED nach Fehlern */

//char cNow[ZEITBUFLEN];

void finish_with_Error(char* ErrorMessage);           // Fatale Fehlermeldung
void report_Error(char* ErrorMessage, bool withMail); // Nicht-Fatale Fehlermeldung

#define MAXERROR  12

#ifndef EXTERN
	#ifdef _MODUL0
  	#define EXTERN
	#else
  	#define EXTERN extern
	#endif
#endif

#define DO_FOREVER     		while(true)

#ifdef _MODUL0
	bool aborted=false;
#endif
#define UNTIL_ABORTED			while(!aborted)

#endif //_ERROR_H

//*********************************************************************************************//
