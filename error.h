//*********************************************************************************************//
//*                                                                                           *//
//* File:          error.h                                                                    *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2018-07-14;                                                                *//
//* Last change:   2022-05-04 - 15:39:46                                                      *//
//* Description:   SolarControl - Überwachung und Messung des Solarmoduls                     *//
//*                Standard-Fehlerausgänge                                                    *//
//*                                                                                           *//
//* Copyright (C) 2018 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef _ERROR_H
#define _ERROR_H  1

#define ERRBUFLEN      2048
#define ZEITBUFLEN      128

#define FEHLERZEIT      12    /* Brenndauer rote LED nach Fehlern */

void finish_with_Error(char* ErrorMessage);           // Fatale Fehlermeldung
void report_Error(char* ErrorMessage, bool withMail); // Nicht-Fatale Fehlermeldung

void Fehlerliste(char* Message);

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
