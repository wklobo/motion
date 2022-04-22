//**************************************************************************//
//*                                                                        *//
//* File:          version.h                                               *//
//* Author:        Wolfgang Keuch                                          *//
//* Creation date: 2014-03-31;                                             *//
//* Last change:   2021-09-18 - 15:02:52                                   *//
//* Description:   Versions-Verwaltung                                     *//
//*                Die 'BUILD'-Nummer wird �ber ein Python-Programm erh�ht *//
//* Copyright (C) 2014-2922 by Wolfgang Keuch                              *//
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
#define BUILD 100

#endif //_VERSION_H

//***************************************************************************//
