//*                                                                                           *//
//* File:          motion_db.h   ex DB_Motion                                                 *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2022-11-30                                                                  *//
//* Last change:   2022-11-30 - 16:09:47                                                      *//
//* Description:   Beschreibung der Sensor-Datenbanken                                        *//
//*                wird in 'createdb' umgesetzt                                               *//
//*                                                                                           *//
//* Copyright (C) 2022 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef MOTIONDB_H
#define MOTIONDB_H 1

//***************************************************************************************************************

//        mosquitto_sub -h Broker -v -t /Geiger/#


// Datenbank
// =========
#define THISUSER            "root"
#define THISPW              "geheim"
#define THISHOST            "localhost"
#define MYDATABASE          "db_Motion"

#define CREATEDATABASE      "CREATE DATABASE IF NOT EXISTS  `%s`" \
                            " DEFAULT   CHARACTER SET utf8      " \
                            " COLLATE   utf8_german2_ci;"
                            // damit funktionieren Umlaute !!!

//***************************************************************************************************************

// Standort-Tabelle
// ====================
#ifdef _MODUL0
void Sitetable(){}    // um in UltraEdit sichtbar zu sein
#endif
// speichert die vorhandenen Messstellen bzw. Rechner
#define SITETABLE    "mw_Site"

#define SITEFIELDS   " ( `keyID` int(12) NOT NULL AUTO_INCREMENT  COMMENT 'Primärindex',\
                         `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                         `Name` char(32)   NOT NULL               COMMENT 'der Rechner-Name', \
                         `Ort` char(64)    DEFAULT NULL           COMMENT 'wo steht dieser Rechner', \
                         `Typ` char(32)    DEFAULT NULL           COMMENT 'der Rechnertyp', \
                         `Adresse` char(32)  NOT NULL  UNIQUE     COMMENT 'die IP-Adresse' , \
                         `Bemerkung` tinytext   , \
                         PRIMARY KEY (`keyID`) \
                       ) ENGINE=InnoDB                            COMMENT 'die vorhandenen Messstellen';"

//***************************************************************************************************************

// Ereignis-Tabelle
// ====================
#ifdef _MODUL0
void Eventtable(){}    // um in UltraEdit sichtbar zu sein
#endif
// speichert Ereignisse
#define EVENTTABLE    "sq_Event"

#define EVENTFIELDS   " ( `keyID` int(12) NOT NULL AUTO_INCREMENT     COMMENT 'Primärindex', \
                          `evSaved` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                          `evEvent` tinytext NOT NULL                 COMMENT 'Eventnummer', \
                          `evDate` date NOT NULL DEFAULT '0000-00-00' COMMENT 'Ereignis-Datum', \
                          `evTime` time NOT NULL DEFAULT '00:00:00'   COMMENT 'Ereignis-Uhrzeit', \
                          `evSize` int(11) NOT NULL                   COMMENT 'Speicherbelegung', \
                          `evRemark` tinytext   , \
                          PRIMARY KEY (`keyID`) \
                        ) ENGINE=InnoDB                               COMMENT 'Ereignis-Tabelle';"

//***************************************************************************************************************

#endif //MOTIONDB_H

