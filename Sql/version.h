//**************************************************************************//
//*                                                                        *//
//* File:          version.h                                               *//
//* Author:        Wolfgang Keuch                                          *//
//* Creation date: 2014-03-31;                                             *//
//* Last change:   2022-04-11 - 16:19:57                                   *//
//* Description:   Versions-Verwaltung                                     *//
//*                Die 'BUILD'-Nummer wird über ein Python-Programm erhöht *//
//* Copyright (C) 2014-2022 by Wolfgang Keuch                              *//
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
#define BUILD 121

#endif //_VERSION_H

//***************************************************************************//
