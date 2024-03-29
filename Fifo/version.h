//**************************************************************************//
//*                                                                        *//
//* File:          version.h                                               *//
//* Author:        Wolfgang Keuch                                          *//
//* Creation date: 2014-03-31;                                             *//
//* Last change:   2022-11-29 - 10:22:43                                   *//
//* Description:   Versions-Verwaltung                                     *//
//*                Die 'BUILD'-Nummer wird über ein Python-Programm erhöht *//
//* Copyright (C) 2014-2922 by Wolfgang Keuch                              *//
//*                                                                        *//
//* nach UTF-8 konvertiert                                                 *//
//*                                                                        *//
//**************************************************************************//

#ifndef _VERSION_H
#define _VERSION_H  1

#ifdef _MODUL0
  #define EXTERN static
#else
  #define EXTERN extern
#endif

EXTERN char Version[50];

#define MAXVERS 4
#define MINVERS 3
#define BUILD 203

#endif //_VERSION_H

//***************************************************************************//
