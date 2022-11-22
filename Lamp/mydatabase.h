//*                                                                                           *//
//* File:          mydatabase.h                                                               *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2022-07-09                                                                 *//
//* Last change:   2022-10-20 - 17:19:59                                                      *//
//* Description:   Individuelle Beschreibung der Datenbanken                                  *//
//*                wird in 'createdb' umgesetzt                                               *//
//*                                                                                           *//
//* Copyright (C) 2022 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef MYDATABASE_H
#define MYDATABASE_H 1

//***************************************************************************************************************

//        mosquitto_sub -h Broker -v -t /Geiger/#


// Datenbank
// =========
//#define THISUSER            "Heizer"
//#define THISPW              "warm"
#define THISUSER            "root"
#define THISPW              "geheim"
#define THISHOST            "localhost"
#define MYDATABASE          "db_Messwerte"

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
                         `Adresse` char(16)  NOT NULL  UNIQUE     COMMENT 'die IP-Adresse' , \
                         `Bemerkung` tinytext   , \
                         PRIMARY KEY (`keyID`) \
                       ) ENGINE=InnoDB                            COMMENT 'die vorhandenen Messstellen';"

//***************************************************************************************************************

// Typen-Tabelle
// ====================
#ifdef _MODUL0
void Typetable(){}
#endif
// speichert die vorhandenen Messwert-Typen
#define TYPETABLE    "mw_Type"

#define TYPEFIELDS   " ( `keyID` int(12) NOT NULL AUTO_INCREMENT COMMENT 'Primärindex',\
                         `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                         `TypName` char(32)  NOT NULL  UNIQUE    COMMENT 'der Sensortyp', \
                         `Zweck` char(64)    DEFAULT NULL        COMMENT 'Verwendung', \
                         `Maske`   char(16)  DEFAULT '%2.2f'     COMMENT 'die Darstellung', \
                         `Einheit` char(16)  DEFAULT NULL        COMMENT 'die MaÃŸeinheit', \
                         `Bemerkung` tinytext  , \
                         PRIMARY KEY (`keyID`) \
                       ) ENGINE=InnoDB                           COMMENT 'die vorhandenen Sensor-Typen';"

                  //  Typ                       Zweck                  Einheit
                  //  ----------------          ---------------        -----------
#define TYPEVALUES  "'UNDEFINED'/               'unbestimmt'/          '-'/        \
                     'Infrarot-Lampe'/          'Beleuchtung'/         '-'/        \
                     'Leuchtdiode rot'/         'Signal'/              '-'/        \
                     'Leuchtdiode gelb'/        'Signal'/              '-'/        \
                     'Leuchtdiode gruen'/       'Signal'/              '-'/        \
                     'Leuchtdiode blau'/        'Signal'/              '-'/        \
                     'Leuchtdiode weiss'/       'Signal'/              '-'/        \
                     'Leuchtdiode bunt'/        'Signal'/              '-'/        \
                     'Leuchtdiode Blitz'/       'Signal'/              '-'/        \
                     'Leuchtdiode Alarm'/       'Alarm'/               '-'/        \
                     'DS18B20'/                 'Temperatur'/          '°C'/       \
                     'BME280 Temp'/             'Lufttemperatur'/      '°C'/       \
                     'BME280 Press'/            'Luftdruck'/           'hPa'/      \
                     'BME280 Humi'/             'Luftfeuchtigkeit'/    'rel%'/     \
                     'TLS2561'/                 'Helligkeit'/          'lm'/       \
                     'Geigerzaehler'/           'Äquivalentdosis'/     'µSv-h'/    \
                     'CPU-Temperatur'/          'CPU-Temperatur'/      '°C'/       \
                     'CPU_Temperatur'/          'CPU_Temperatur'/      '°C'/       \
                     'Geigerklicks'/            'Radoaktivität'/       'Ticks'/    \
                     ''/                        ''/                    ''; "

// Anm.: '/' ist Trennzeichen bei MQTT und darf auf keinen Fall im Text vorkommen !

#define TYPE_TNAME_QUERY   "SELECT TypName FROM mw_Type WHERE keyID=%d"

//***************************************************************************************************************

// Namens-Tabelle
// ====================
#ifdef _MODUL0
void Nametable(){}
#endif
// von MQTT-Namen --> Typnamen
#define NAMETABLE    "mw_Name"

#define NAMEFIELDS   " ( `keyID` int(12) NOT NULL AUTO_INCREMENT  COMMENT 'Primärindex',\
                         `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                         `NamID` int(12)   NOT NULL   UNIQUE      COMMENT 'die Names-ID', \
                         `MQTTName` char(32)   NOT NULL           COMMENT 'der Sensor-Name aus MQTT', \
                         `TypName` char(32)    NOT NULL           COMMENT 'der Sensortyp', \
                         `Bemerkung` tinytext   , \
                         PRIMARY KEY (`keyID`) \
                       ) ENGINE=InnoDB                            COMMENT 'Umsetzung Typenbezeichnung';"

                   //   MQTT-Name                          Typ-Name
                   //   -------------------                --------------------
#define NAMEVALUES  "  'D_IRLAMPE_RECHTS'/                'Infrarot-Lampe'/      \
                       'D_IRLAMPE_LINKS'/                 'Infrarot-Lampe'/      \
                       'D_IRLAMPEN'/                      'Infrarot-Lampe'/      \
                       'D_LED_ROT'/                       'Leuchtdiode rot'/     \
                       'D_LED_GELB'/                      'Leuchtdiode gelb'/    \
                       'D_LED_GRUEN'/                     'Leuchtdiode gruen'/   \
                       'D_LED_BLAU'/                      'Leuchtdiode blau'/    \
                       'D_LED_GRUEN1'/                    'Leuchtdiode weiss'/   \
                       'D_LED_BLAU1'/                     'Leuchtdiode bunt'/    \
                       'D_DS18B20_0'/                     'DS18B20'/             \
                       'D_DS18B20_1'/                     'DS18B20'/             \
                       'D_DS18B20_2'/                     'DS18B20'/             \
                       'D_DS18B20_3'/                     'DS18B20'/             \
                       'D_DS18B20_4'/                     'DS18B20'/             \
                       'D_DS18B20_5'/                     'DS18B20'/             \
                       'D_DS18B20_6'/                     'DS18B20'/             \
                       'D_DS18B20_7'/                     'DS18B20'/             \
                       'D_DS18B20_8'/                     'DS18B20'/             \
                       'D_DS18B20_9'/                     'DS18B20'/             \
                       'D_BME280_T'/                      'BME280 Temp'/         \
                       'D_BME280_P'/                      'BME280 Press'/        \
                       'D_BME280_H'/                      'BME280 Humi'/         \
                       'D_TLS2561'/                       'TLS2561'/             \
                       'D_INTERN'/                        'CPU-Temperatur'/      \
                       'D_GEIGER'/                        'Geigerzähler'/        \
                       'D_TICKS'/                         'Geigerklicks'/        \
                       'D_UNDEFINED'/                     'UNDEFINED'/           \
                       'D_LED_BLITZ'/                     'Leuchtdiode Blitz'/   \
                       'D_CPU_TEMP'/                      'CPU_Temperatur'/      \
                       'D_LED_ALARM'/                     'Leuchtdiode Alarm'/   \
                       ''/                                ''; "

// Constraints der Tabelle `mw_Name`
// ----------------------------------
// Constraints definieren Bedingungen, die beim Einfügen, Ändern und Löchen von Datensätzen in der Datenbank
// erfüllt werden müssen.
#define NAMECONSTRAINT     "`FK_TName` FOREIGN KEY (`TypName`) REFERENCES `mw_Type` (`TypName`)"

//***************************************************************************************************************

// Sensor-Tabelle
// ==========================
#ifdef _MODUL0
void Sensortable(){}
#endif
// speichert die vorhandenen Messwert-Sensoren
#define SENSORTABLE    "mw_Sensor"

#define SENSORFIELDS   " ( `keyID` int(12) NOT NULL AUTO_INCREMENT    COMMENT 'Primärindex', \
                           `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                           `Name`  char(32)   NOT NULL                COMMENT 'vergebener Name bzw. Name aus MQTT', \
                           `Serial` char(32)                          COMMENT 'optionale Seriennummer', \
                           `rType` int(12)   NOT NULL                 COMMENT 'Link zur Typ-Tabelle', \
                           `rSite` int(12)   NOT NULL                 COMMENT 'Link zur Standort-Tabelle', \
                           `Funktion` char(32) DEFAULT NULL           COMMENT 'steuert die Auswertung', \
                           `aktiv` tinyint(1)  DEFAULT NULL , \
                           `Standort`  tinytext                       COMMENT 'wo sitzt dieser Sensor', \
                           `Bemerkung` tinytext  , \
                           PRIMARY KEY (`keyID`) \
                         ) ENGINE=InnoDB                              COMMENT 'die vorhandenen Mess-Sensoren';"

// Constraints der Tabelle `mw_Sensor`
// ----------------------------------
// Constraints definieren Bedingungen, die beim Einfügen, Ändern und Löchen von Datensätzen in der Datenbank
// erfüllt werden müssen.
#define SENSORCONSTRAINT1    "`FK_TSensor1` FOREIGN KEY (`rSite`) \
                               REFERENCES `mw_Site` (`keyID`) ON DELETE CASCADE ON UPDATE CASCADE"
#define SENSORCONSTRAINT2    "`FK_TSensor2` FOREIGN KEY (`rType`) \
                               REFERENCES `mw_Type` (`keyID`) ON DELETE CASCADE ON UPDATE CASCADE"
#define SENSORCONSTRAINT3    "`FK_TSensor3` UNIQUE (Name, rType, rSite)"

#define SENSOR_RTYPE_QUERY   "SELECT rType FROM mw_Sensor WHERE keyID=%d"
#define SENSOR_RFUNK_QUERY   "SELECT rType FROM mw_Sensor WHERE Funktion='%s'"

//***************************************************************************************************************

// Messwert-Tabelle
// ============================
#ifdef _MODUL0

void Valuetable(){}
#endif
// speichert die Ã¼ber MQTT empfangenen Messwerte
#define VALUETABLE    "mw_Value"

#define VALUEFIELDS   " (  `keyID` int(12) NOT NULL AUTO_INCREMENT  COMMENT 'Primärindex', \
                           `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                           `rSensor` int(12) NOT NULL               COMMENT 'Link zur Sensor-Tabelle', \
                           `Messwert` decimal(12,6) NOT NULL        COMMENT 'eben der Messwert', \
                           `Maske`    char(16) DEFAULT NULL         COMMENT 'die Darstellung', \
                           `Einheit`  char(16) DEFAULT NULL         COMMENT 'die Masseinheit', \
                           `Bemerkung` tinytext, \
                           PRIMARY KEY (`keyID`) \
                         ) ENGINE=InnoDB                            COMMENT='die erfassten Messwerte';"

// Constraints der Tabelle `mw_Value`
// ----------------------------------
// Constraints definieren Bedingungen, die beim Einfügen, Ändern und Löchen von Datensätzen in der Datenbank
// erfüllt werden müssen.
#define VALUECONSTRAINT    "`FK_TValues` FOREIGN KEY (`rSensor`) \
                             REFERENCES `mw_Sensor` (`keyID`) ON DELETE CASCADE ON UPDATE CASCADE"

#define VALUE_SLOPE_QUERY   "SELECT Datum, Messwert FROM mw_Value WHERE rSensor=%ld ORDER BY Datum DESC LIMIT %d"
#define VALUE_MWERT_QUERY   "SELECT Messwert FROM mw_Value WHERE rSensor=%ld ORDER BY Datum DESC LIMIT 1"

// Abfrage auf 'MIN/MAX/AVG'-Wert
// -------------------------------
#define VALUE_MINMAX_QUERY  "SELECT %s(`Messwert`) FROM `mw_Value` " \
                            "WHERE rSensor=%ld AND DAY(`Datum`)=DAY(SUBDATE(DATE(NOW()),INTERVAL 1 DAY)) LIMIT 1"

// Abfrage auf 'keyID' von 'MIN/MAX/AVG'-Wert
// ------------------------------------------
#define VALUE_KEYID_QUERY   "SELECT `keyID` FROM mw_Value "\
                            "WHERE DAY(`Datum`)=DAY(SUBDATE(DATE(NOW()),INTERVAL 1 DAY)) " \
                            "AND `Messwert` = (SELECT %s(`Messwert`) FROM mw_Value WHERE `rSensor`=%ld AND " \
                            "DAY(`Datum`)=DAY(SUBDATE(DATE(NOW()),INTERVAL 1 DAY))) LIMIT 1"

//***************************************************************************************************************

// Ereignis-Tabelle
// ============================
#ifdef _MODUL0
void Eventtable(){}
#endif
// speichert erkannte Ereignisse wie z.B. TemperatursprÃ¼nge
#define EVENTTABLE    "mw_Event"

#define EVENTFIELDS   " (  `keyID` int(12) NOT NULL AUTO_INCREMENT   COMMENT 'Primärindex', \
                           `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                           `rSensor` int(12) NOT NULL                COMMENT 'Link zur Sensor-Tabelle', \
                           `Messwert` decimal(12,6) NOT NULL         COMMENT 'Messwert in °C', \
                           `Steigung` decimal(12,6) NOT NULL         COMMENT 'Änderung in °C/h', \
                           `Ereignis`  tinytext, \
                           `Bemerkung` tinytext, \
                           PRIMARY KEY (`keyID`) \
                         ) ENGINE=InnoDB                             COMMENT='die erkannte Ereignisse';"

// Constraints der Tabelle `mw_Event`
// ----------------------------------
// Constraints definieren Bedingungen, die beim Einfügen, Ändern und Löchen von Datensätzen in der Datenbank
// erfüllt werden müssen.
#define EVENTCONSTRAINT    "`FK_TEvents` FOREIGN KEY (`rSensor`) " \
                           "REFERENCES `mw_Sensor` (`keyID`) ON DELETE CASCADE ON UPDATE CASCADE"

//***************************************************************************************************************

// Statistik-Tabelle
// ============================
#ifdef _MODUL0
void Stattable(){}
#endif
// speichert spezielle Werte wie z.B. min/max-Werte
#define STATTABLE    "mw_Statistik"

#define STATFIELDS   " (   `keyID` int(12) NOT NULL AUTO_INCREMENT  COMMENT 'Primärindex', \
                           `Datum` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
                           `rSensor` int(12) NOT NULL               COMMENT 'Link zur Sensor-Tabelle', \
                           `Wertdatum` date NOT NULL                COMMENT 'Datum der Messwerte', \
                           `MinZeit` time NOT NULL                  COMMENT 'Zeitpunkt des Minimalwerts', \
                           `MinWert` decimal(12,6) NOT NULL         COMMENT 'Minimalwert des Tages', \
                           `MaxZeit` time NOT NULL                  COMMENT 'Zeitpunkt des Maximalwerts', \
                           `MaxWert` decimal(12,6) NOT NULL         COMMENT 'Maximalwert des Tages', \
                           `Durchschnitt` decimal(12,6) NOT NULL    COMMENT 'Durchschnittswert des Tages', \
                           `Bemerkung` tinytext, \
                            PRIMARY KEY (`keyID`), \
                            KEY `idxDatum` (`Datum`)  \
                        ) ENGINE=InnoDB                             COMMENT='Statistik-Werte';"

// Constraints der Tabelle `mw_Statistik`
// -------------------------------------
// Constraints definieren Bedingungen, die beim Einfügen, Ändern und Löchen von Datensätzen in der Datenbank
// erfüllt werden müssen.
#define STATCONSTRAINT    "`FK_TStats` FOREIGN KEY (`rSensor`) " \
                          "REFERENCES `mw_Sensor` (`keyID`) ON DELETE CASCADE ON UPDATE CASCADE"

#define STAT_FILL_QUERY   "INSERT INTO mw_Statistik" \
                          " (rSensor, Wertdatum, MinZeit, MinWert, MaxZeit, MaxWert, Durchschnitt, Bemerkung)" \
                          " VALUES (%ld, '%s', '%s', %2.3f, '%s', %2.3f, %2.3f, '%s')"

//****************************************************************************************************************

#endif //MYDATABASE_H

