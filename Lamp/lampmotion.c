//******** Vogel ************************************************************//
//*                                                                         *//
//* File:          lampmotion.c                                             *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2021-04-05;                                              *//
//* Last change:   2022-11-28 - 15:32:03                                    *//
//* Description:   Nistkastenprogramm - ergänzt 'fifomotion':               *//
//*                Steuerung der Infrarot-Lampen                            *//
//*                Verwaltung der Umwelt-Sensoren                           *//
//*                Kommunikation per MQTT                                   *//
//*                                                                         *//
//* 12. November 2022 - Umstellung auf Hilfsfunktionen in 'treiber'         *//
//*                                                                         *//
//* Copyright (C) 2014-23 by Wolfgang Keuch                                 *//
//*                                                                         *//
//* Aufruf:                                                                 *//
//*    ./LampMotion &    (Daemon)                                           *//
//*                                                                         *//
//***************************************************************************//

#define _MODUL0
#define __LAMPMOTION_DEBUG__      false
#define __LAMPMOTION_DEBUG_INIT__ true
#define __LAMPMOTION_DEBUG_MQTT__ false
#define __LAMPMOTION_DEBUG_1__    false
#define __LAMPMOTION_DEBUG_2__    false


// Peripherie aktivieren
// ----------------------
#define __LCD_DISPLAY__    false   /* LCD-Display               */
#define __DS18B20__        true    /* DS18B20-Sensoren          */
#define __INTERNAL__       true    /* CPU-Temperatur            */
#define __BME280__         true    /* BME280-Sensor             */
#define __TSL2561__        true    /* TLS2561-Sensor            */
#define __GPIO__           true    /* GPIOs über 'gpio.c'       */
#define __MQTT__           true    /* Mosquitto                 */
#define __INTERRUPT__      false   /* Interrupt Geigerzähler    */
#define __DATENBANK__      true    /* Datenbank                 */


// Einstellungen für Sensoren
// --------------------------
#define CPU_TAKT      45              // Abfragetakt CPU-Temperatur
#define DS1820_TAKT   60              // Abfragetakt Innen-Temperatur


//***************************************************************************//

#include "./version.h"
#include "./lampmotion.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <mysql.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "/home/pi/treiber/common/gpio.h"
#include "/home/pi/treiber/common/mqtt.h"
#include "/home/pi/treiber/common/error.h"
#include "/home/pi/treiber/common/common.h"
#include "/home/pi/treiber/sensor/bme280.h"
#include "/home/pi/treiber/sensor/ds18b20.h"
#include "/home/pi/treiber/sensor/tsl2561.h"
#include "/home/pi/treiber/common/datetime.h"
#include "/home/pi/treiber/common/mqtthelp.h"
#include "/home/pi/treiber/sendmail/sendMail.h"

#include "/home/pi/treiber/dbank/createdb.h"
#include "/home/pi/treiber/dbank/sensor_db.h"


// statische Variable
// --------------------
static time_t ErrorFlag = 0;          // Steuerung rote LED
static bool Automatic = false;        // Steuerung IR-Lampem

//// Message-Erwiderung
//// --------------------
//enum RESPONSE { RESP_KEINEANTWORT,
//                RESP_QUITTUNG=200,
//                RESP_NOSUPPORT,
//                RESP_FEHLER};

#define SENDETAKT      20   // für Sensoren [sec]
#define ABFRAGETAKT   250   // für MQTT [msec]

//***************************************************************************//
/*
 * Define debug function.
 * ---------------------
 */

#if __LAMPMOTION_DEBUG__
  #define DEBUG(...) printf(__VA_ARGS__)
#else
  #define DEBUG(...)
#endif

#if __LAMPMOTION_DEBUG_INIT__
  // nur die Init-Phase
  // -------------------
  #define _DEBUG(...) printf(__VA_ARGS__)
#else
  #define _DEBUG(...)
#endif

#if __LAMPMOTION_DEBUG_MQTT__
  #define DEBUG_MQTT(...) printf(__VA_ARGS__)
#else
  #define DEBUG_MQTT(...)
#endif

#if __LAMPMOTION_DEBUG_1__
#define DEBUG_1(...)  printf(__VA_ARGS__)
#else
#define DEBUG_1(...)
#endif

#if __LAMPMOTION_DEBUG_2__
#define DEBUG_2(...)  printf(__VA_ARGS__)
#else
#define DEBUG_2(...)
#endif

//**************************************************************************************//

// programmweite Variable
// -----------------------
char   s_Hostname[ZEILE];                       // der Name dieses Rechners
char   s_meineIPAdr[NOTIZ];                     // die IP-Adresse dieses Rechners
uint   s_IPnmr=0;                               // letzte Stelle der IP-Adresse
long   s_pid=0;                                 // meine Prozess-ID

//static bool   s_aborted = false;                // Status Signale SIGTERM und SIGKILL

#if __INTERRUPT__
  extern int clickCounter;                      // the event counter
#endif


#if __DS18B20__
  static uint ds18b20anz = 0;                   // Anzahl DS18B20-Sensoren
  static uint ds18b20takt = 0;                  // Abfragetakt gesamt
  static uint ds18b20gap = 0;                   // Abfragetakt
#endif


#if __BME280__
  static int bme280 = 0;                        // Anzahl BME280-Sensoren
#endif


#if __TSL2561__
  static void* tsl = NULL;                      // das TLS2561-Objekt !
#endif


#if __INTERNAL__
  float CPUTemp = 0;                            // interne CPU-Temperatur lesen
#endif


static struct MqttInfo* mqtt = NULL;            // das MQTT-Objekt
#if __MQTT__
  #undef  __DATENBANK__
  #define __DATENBANK__      true               /* Datenbank wird benötigt */
#endif


#if __DATENBANK__
  static MYSQL* con = NULL;                     // Verbindung zur Datenbank
  long ID_Site = -1;                            // KeyID dieses Rechners in der Datenbank
  long ID_ds18B20[MAXDS18B20] = {-1};           // KeyIDs ds18B20-Sensoren
  long ID_bme280[DREI] = {-1};                  // alle BME28-Sensoren
  long ID_tsl2561 = -1;                         // KeyID TSL2561
#endif


#if __LCD_DISPLAY__
  static int  LCDi2cfd = 0;                     // LCD-Display
#endif

//**************************************************************************************//

// Signal-Handler
// --------------
// Signale SIGTERM und SIGKILL werden meist durch den Watchdog ausgelöst

void sigfunc(int sig)
{
 if(sig == SIGTERM)
 {
    { // --- Log-Ausgabe ------------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<< ----- SIGTERM %s -------", PROGNAME);
    MYLOG(LogText);
    } // ----------------------------------------------------------------------------
 }
 else if(sig == SIGKILL)
 {
    { // --- Log-Ausgabe ------------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<< ----- SIGKILL %s -------", PROGNAME);
    MYLOG(LogText);
    } // ----------------------------------------------------------------------------
//***  	aborted = true;
 }
 else
    return;
}
//***********************************************************************************************

// fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_Error( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  sprintf( Fehler, "%s - Err %d-%s", Message, errsv, strerror(errsv));
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  printf("    -- Fehler -->  %s\n", ErrText);   // lokale Fehlerausgabe
  digitalWrite (M_LED_GRUEN1,  LED_AUS);
  digitalWrite (M_LED_BLAU1,   LED_AUS);
  digitalWrite (M_LED_ROT,     LED_EIN);

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<< %s: Exit!",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // PID-Datei wieder löschen
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
//***  ErrorFlag = time(0) + BRENNDAUER;             // Steuerung rote LED

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "<<*** %s",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  if (errsv == 24)                              // 'too many open files' ...
    report_Error(ErrText, true);                // Fehlermeldung mit Mail ausgeben
  else
    report_Error(ErrText, false);               // Fehlermeldung ohne Mail ausgeben

  return errsv;
}
//***********************************************************************************************
//***********************************************************************************************

// auf eine MQTT-Message antworten
// -------------------------------
// mit Log-Möglichkeit
int ReplyMessage(struct MqttInfo* info, char* topic, char* payload)
{
#ifdef __LAMPMOTION_MYLOG__
  { // --- Log-Ausgabe ----------------------------------------------------------------
    // zum Debuggen Klartext einfügen
    char Messagetyp[ZEILE] = "";
    char Messageinfo[ZEILE] = "";
    char Topic[ZEILE];
    char LogText[ZEILE];

    strcpy(Topic, topic);
    DEBUG(">> %s---%s()#%d: Topic = '%s'\n", __NOW__, __FUNCTION__, __LINE__, Topic);
    if (matchn(Topic, TPOS_TYP, MSG_ERROR))
      sprintf(Messagetyp, "%5d(\"%s\")", MSG_ERROR, "Fehlermeldung");
    else if (matchn(Topic, TPOS_TYP, MSG_VALUE))
      sprintf(Messagetyp, "%5d(\"%s\")", MSG_VALUE, "Messwert");
    DEBUG("   %s   %s()#%d: Messagetyp = '%s'\n", __NOW__, __FUNCTION__, __LINE__, Messagetyp);

    if (matchn(Topic, TPOS_INF, INF_FEHLERHAFT))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_FEHLERHAFT, "Message war fehlerhaft");
    else if (matchn(Topic, TPOS_INF, INF_NOSUPPORT))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_NOSUPPORT, "wird nicht unterstuetzt");
    else if (matchn(Topic, TPOS_INF, INF_ACK))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_ACK, "positive Quittung");
    else if (matchn(Topic, TPOS_INF, INF_NAK))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_NAK, "negative Quittung");
    DEBUG("   %s   %s()#%d: Messageinfo = '%s'\n", __NOW__, __FUNCTION__, __LINE__, Messageinfo);
    if (strlen(Messagetyp) > 0)
      setStr(Topic, TPOS_TYP, Messagetyp);
    if (strlen(Messageinfo) > 0)
      setStr(Topic, TPOS_INF, Messageinfo);
    sprintf(LogText, " Topic: \"%s\" -- Payload: \"%s\"", Topic, payload);
    DEBUG(">> %s---%s()#%d: Topic = '%s' - Payload: \"%s\"\n",
                                        __NOW__, __FUNCTION__, __LINE__, Topic, payload);
    MYLOG(LogText);
  } // ---------------------------------------------------------------------------------
#endif
  return(MQTT_Publish(info, topic, payload));
}
//***********************************************************************************************

// interne CPU-Temperatur lesen
// ---------------------------------------------------------------------
int readCPUtemp(void* mqtt)
{
	long myKeyID = ID_Site;
  float Messwert = InternalTemperatur();
  DEBUG(">> %s---%s()#%d: CPU-Temperatur = '%.1f'\n",__S__, Messwert);

	#include "/home/pi/treiber/snippets/send_messwert.snip"
  return 0;
}
//***********************************************************************************************

// alle DS18B20-Sensoren einlesen
// -------------------------------
int readds18b20(int sensoren, void* mqtt)
{
  for (int ix=0; ix < sensoren; ix++)
  {
		long myKeyID = ID_ds18B20[ix];
  	float Messwert = ds18b20_Value(ix);

		#include "/home/pi/treiber/snippets/send_messwert.snip"
	}
  return 0;
}
//***********************************************************************************************

// BME280-Sensor (Temperatur, Luftdruck, Feuchtigkeit) lesen und senden
// ---------------------------------------------------------------------
int readbme280(void* mqtt)
{
  float Temperatur  = '\0';
  float Luftdruck   = '\0';
  float Feuchtigkeit = '\0';
  if (bme280_Read(&Luftdruck, &Temperatur, &Feuchtigkeit))
  {
    DEBUG(">> %s---%s()#%d: BME280-Temperatur = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Temperatur);
    DEBUG(">> %s---%s()#%d: BME280-Luftdruck = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Luftdruck);
    DEBUG(">> %s---%s()#%d: BME280-Feuchtigkeit = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Feuchtigkeit);
  }
  // -- MQTT --
  for (int ix=0; ix < DREI; ix++)
  {
  	char myName[NOTIZ];  
  	sprintf(myName, "D_%s_%d", DS18B20TYP, ix);          
		long myKeyID = ID_bme280[ix];
  	float Messwert = ds18b20_Value(ix);

		#include "/home/pi/treiber/snippets/send_messwert.snip"	
	}
  return 0;
}
//***********************************************************************************************

// tsl2561-Sensor (Helligkeit) lesen und senden
// --------------------------------------------
int readtsl2561(void* tsl, void* mqtt)
{
	long myKeyID = ID_tsl2561;
  float Messwert = 1.0 * tsl2561_lux(tsl);
  DEBUG(">> %s---%s()#%d: Helligkeit = '%f'\n", __S__, Messwert);
 
	#include "/home/pi/treiber/snippets/send_messwert.snip"
  return 0;        
}
//***********************************************************************************************

// angeforderte Quittung senden
// ------------------------------------------
int ReqAcknowledge(char* Topic, char* Payload)
{
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  //  Topic:
  //  0.Raspi-Name (erforderlich)       - neu
  //  1.IP-letzte Stelle (erforderlich) - neu
  //  2.Sensor-ID (erforderlich)        - alt
  //  3.Sensor-Name (oder "-")          - alt
  //  4.Datum/Uhrzeit - als timestamp   - neu
  //  5.Messagetyp (Art der Message)    - alt
  //  6.Messageart (Textteil Message)   - neu

  setStr(Topic, TPOS_RAS, s_Hostname);            // eigener Name
  setInt(Topic, TPOS_IPA, s_IPnmr);         // eigene IP
  setTim(Topic, TPOS_TIM);                      // aktuelle Zeit
  setInt(Topic, TPOS_TYP, MSG_COMMAND);         // Status
  setInt(Topic, TPOS_INF, INF_ACK);             // Antwort auf Anfrage

  //  Payload: (wenn Topic[5] = MSG_COMMAND)
  //  0.Steuerbefehl   - neu
  //  1.Schaltwert     - alt
  //  2.Analogwert     - alt
  //  3.Anzeigewert    - alt

  setStr(Payload, PPOS_STT, "Status");            // eigener Name

  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  return RESP_QUITTUNG;
}
//***********************************************************************************************

// Anforderung wird nicht unterstützt
// ------------------------------------------
int ReqNotSupported(char* Topic, char* Payload)
{
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  //  Topic:
  //  0.Raspi-Name (erforderlich)       - neu
  //  1.IP-letzte Stelle (erforderlich) - neu
  //  2.Sensor-ID (erforderlich)        - alt
  //  3.Sensor-Name (oder "-")          - alt
  //  4.Datum/Uhrzeit - als timestamp   - neu
  //  5.Messagetyp (Art der Message)    - neu
  //  6.Messageart (Textteil Message)   - neu

  setStr(Topic, TPOS_RAS, s_Hostname);            // eigener Name
  setInt(Topic, TPOS_IPA, s_IPnmr);         // eigene IP
  setTim(Topic, TPOS_TIM);                      // aktuelle Zeit
  setInt(Topic, TPOS_TYP, MSG_ERROR);           // Status
  setInt(Topic, TPOS_INF, INF_NOSUPPORT);       // Antwort auf Anfrage

  //  Payload: (wenn Topic[5] = MSG_COMMAND)
  //  0.Steuerbefehl   - alt
  //  1.Schaltwert     - alt
  //  2.Analogwert     - alt
  //  3.Anzeigewert    - alt

  DEBUG(">> %s---%s()#%d ->  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  return RESP_NOSUPPORT;
}
//***********************************************************************************************

// Anforderung war fehlerhaft
// ------------------------------------------
int ReqFehlerhaft(char* Topic, char* Payload, char* Fehlertext)
{
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  //  Topic:
  //  0.Raspi-Name (erforderlich)       - neu
  //  1.IP-letzte Stelle (erforderlich) - neu
  //  2.Sensor-ID (erforderlich)        - alt
  //  3.Sensor-Name (oder "-")          - alt
  //  4.Datum/Uhrzeit - als timestamp   - neu
  //  5.Messagetyp (Art der Message)    - neu
  //  6.Messageart (Textteil Message)   - neu

  setStr(Topic, TPOS_RAS, s_Hostname);          // eigener Name
  setInt(Topic, TPOS_IPA, s_IPnmr);             // eigene IP
  setTim(Topic, TPOS_TIM);                      // aktuelle Zeit
  setInt(Topic, TPOS_TYP, MSG_ERROR);           // Status
  setInt(Topic, TPOS_INF, INF_FEHLERHAFT);      // war fehlerhaft

  //  Payload: (wenn Topic[5] = MSG_COMMAND)
  //  0.Steuerbefehl   - alt
  //  1.Schaltwert     - alt
  //  2.Analogwert     - alt
  //  3.Anzeigewert    - alt

  sprintf (Payload, MQTT_STATE, Fehlertext);

  DEBUG(">> %s---%s()#%d ->  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  return RESP_NOSUPPORT;
}
//***********************************************************************************************

// Bearbeitung der MODE_COMMAND-Message
// ------------------------------------
// Topic enthält einen Schaltbefehl
int CommandMessage(char* Topic, char* Payload)
{
  DEBUG(">> %s--%s()#%d: Topic: \"%s\" -- Payload: \"%s\"\n",
                                   __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  int response = RESP_KEINEANTWORT;

  // Auswertung von Topic
  // --------------------
  char CompName[NOTIZ] = "Sendername";                    // Name des Senders
  int CompIP = 0;                                         // letzte Stelle IP des Senders
  int DeviceID = 0;                                       // Device-(Sensor-)-ID
  char DeviceName[NOTIZ] = "Devicename";                  // Name des Geräts (Sensor, Schalter,...)
  char MsgZeit[NOTIZ] = "Sendezeitpunkt";                 // Zeitpunkt des Sendens
  char MsgInfo[NOTIZ] = "---123456789";                   // Zusatzinformation

  getStr(Topic, CompName, TPOS_RAS);                      // Name des Senders
  CompIP = getInt(Topic, "IP-Nummer", TPOS_IPA);          // letzte Stelle IP des Senders
  DeviceID = getInt(Topic, "Sensor-ID", TPOS_SID);        // Device-(Sensor-)-ID
  getStr(Topic, DeviceName, TPOS_NAM);                    // Name des Sensors
  getTim(Topic, MsgZeit, TPOS_TIM);                       // Zeitpunkt des Sendens

  bool AckRequest = false;
  if (partn(Topic, TPOS_INF, MsgInfo))                    // wenn es Zusatzinfos gibt ...
  {
    // ... ist es eine Quittungsanforderung ?
    char Inf_ReqfAck[NOTIZ];
    sprintf(Inf_ReqfAck, "%d", INF_REQFACK);
    AckRequest = (bool)strstr( MsgInfo, Inf_ReqfAck );
    DEBUG(">> %s---%s()#%d ->  MsgInfo='%s' -- AckRequest: %i\n",
                                 __NOW__, __FUNCTION__, __LINE__, MsgInfo, (int)AckRequest);
  }

  // Auswertung von Payload
  // ----------------------
  char Steuerbefehl[NOTIZ] = "---";                       // Analogwert (oder '-')
  char Analogwert[NOTIZ] = "Analogwert";                  // Analogwert (oder '-')
  char Anzeigewert[NOTIZ] = "Anzeigewert";                // Anzeigewert (oder "")

  Ctrl Command = Str2Ctrl( toUpper( getStr(Payload, Steuerbefehl, PPOS_CMD)));
  getStr(Payload, Analogwert, PPOS_ANA);                  // Analogwert (oder '---')
  getStr(Payload, Anzeigewert, PPOS_DIS);                 // Anzeigewert (oder "")


  if (Command == SCHALTEN)
  {
    // Schaltwert in Einheitsform
    // --------------------------
    int Schalter;
    char Schaltwert[NOTIZ] = "Schaltwert";
    Ctrl SValue = Str2Ctrl( toUpper( getStr(Payload, Schaltwert, PPOS_VAL)));
    switch ( SValue)
    {
      case EIN:
      case HELL:
        Schalter = LED_HELL;
        break;
      case AUS:
      case DUNKEL:
        Schalter = LED_DUNKEL;
        break;
      case AUTO:
        Schalter = LED_HELL;
        break;
      default:
        DeviceID = INF_FEHLERHAFT;
        break;
    }

    // welches Gerät?
    // --------------
    switch (DeviceID)
    {
      case D_IRLAMPE_RECHTS:
        digitalWrite (M_LAMP_IRRIGHT, Schalter);
        Automatic = false;
        DEBUG(">> %s---%s()#%d: M_LAMP_IRRIGHT = %s(%d); AckRequest=%d\n",
                         __NOW__, __FUNCTION__, __LINE__, Schaltwert, Schalter, (int)AckRequest);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    M_LAMP_IRRIGHT = %s(%d) !", Schaltwert, Schalter );
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        if (AckRequest)                       // Bitte um Quittung
        { // Quittung senden
          response = ReqAcknowledge( Topic, Payload);
        }
        break;

      case D_IRLAMPE_LINKS:
        digitalWrite (M_LAMP_IRLEFT, Schalter);
        Automatic = false;
        DEBUG(">> %s---%s()#%d: M_LAMP_IRLEFT = %s(%d); AckRequest=%d\n",
                         __NOW__, __FUNCTION__, __LINE__, Schaltwert, Schalter, (int)AckRequest);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    M_LAMP_IRLEFT = %s(%d) !", Schaltwert, Schalter );
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        if (AckRequest)                       // Bitte um Quittung
        { // Quittung senden
          response = ReqAcknowledge( Topic, Payload);
        }
        break;

      case D_IRLAMPEN: /* beide betreffend */
        Automatic = true;
        DEBUG(">> %s---%s()#%d: Automatic = %d; AckRequest=%d\n",
                         __NOW__, __FUNCTION__, __LINE__, Automatic, (int)AckRequest);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    Automatic = %d !", Automatic );
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        if (AckRequest)                       // Bitte um Quittung
        { // Quittung senden
          response = ReqAcknowledge( Topic, Payload);
        }
        break;

      default:                  // alles andere
        response = RESP_NOSUPPORT;
        break;
    } // --- ende switch ---
  } // ---- ende Command == CMD_SCHALTEN
  else
    // Befehl nicht unterstützt
    // ------------------------
    response = RESP_NOSUPPORT;



  // um 'not used'-Warnings zu vermeiden:
  // ------------------------------------
  {
    UNUSED(DeviceID);
    UNUSED(CompIP);
    UNUSED(CompName);
    UNUSED(DeviceName);
    UNUSED(MsgZeit);
    UNUSED(Command);
    UNUSED(Analogwert);
    UNUSED(MsgZeit);
//    UNUSED(MsgInfo);
  }

  DEBUG(">> %s---%s()#%d: response = %d\n", __NOW__, __FUNCTION__, __LINE__, response);
  return response;
}
//***********************************************************************************************
//***********************************************************************************************
//**                                                                                           **
//**                                  main()                                                   **
//**                                                                                           **
//***********************************************************************************************
//***********************************************************************************************

int main(int argc, char *argv[])
{
  char MailBody[4*ABSATZ] = {'\0'};

  sprintf (Version, "Vers. %d.%d.%d", MAXVERS, MINVERS, BUILD);
  printf("   %s %s vonu %s\n\n", PROGNAME, Version, __DATE__);

  // Verbindung zum Dämon Syslog aufbauen
  // -----------------------------------
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 );
  syslog(LOG_NOTICE, ">>>>> %s - %s/%s - PID %d - User %d, Group %d <<<<<<",
                  PROGNAME, Version, __DATE__, getpid(), geteuid(), getegid());
                  
  setAuxFolder(AUXDIR, TOPNAME);         		// Info an 'common'
  setAuxFolder_Err(AUXDIR);                	// Info an 'error'

  #include "/home/pi/treiber/snippets/get_progname.snip"

 
  // Signale registrieren
  // --------------------
  signal(SIGTERM, sigfunc);
  signal(SIGKILL, sigfunc);

  printf(">>> %s  %s()#%d: ----------<<< init done >>>----------\n\n",  __Si__);

  // schon mal den Watchdog füttern
  // ------------------------------
  #include "/home/pi/treiber/snippets/set_watchdog.snip"

  printf(">>> %s  %s()#%d: ----------<<< init done >>>----------\n\n",  __Si__);

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
    pinMode (M_LED_GRUEN1,   OUTPUT);
    pinMode (M_LED_BLAU1,    OUTPUT);
    pinMode (M_LAMP_IRRIGHT, OUTPUT);
    pinMode (M_LAMP_IRLEFT,  OUTPUT);
    #define ANZEIT  44 /* msec */
    for (int ix=0; ix < 12; ix++)
    {
      digitalWrite (M_LED_GRUEN1,   LED_EIN);
      digitalWrite (M_LAMP_IRRIGHT, LED_HELL);
      delay(ANZEIT);
      digitalWrite (M_LED_GRUEN1,   LED_AUS);
      digitalWrite (M_LAMP_IRRIGHT, LED_DUNKEL);
      digitalWrite (M_LED_BLAU1,    LED_EIN);
      digitalWrite (M_LAMP_IRLEFT,  LED_HELL);
      delay(ANZEIT);
      digitalWrite (M_LED_BLAU1,    LED_AUS);
      digitalWrite (M_LAMP_IRLEFT,  LED_DUNKEL);
    }
    digitalWrite (M_LAMP_IRRIGHT,   LED_DUNKEL);
    digitalWrite (M_LAMP_IRLEFT,    LED_DUNKEL);
  }


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


  digitalWrite (M_LED_BLAU1, LED_EIN);


  // MQTT starten
  // --------------
  #include "/home/pi/treiber/snippets/mqtt_init.snip"


  // Initialisierung abgeschlossen
  // ------------------------------
  #include "/home/pi/treiber/snippets/init_mail.snip"
  #include "/home/pi/treiber/snippets/init_done.snip"

  digitalWrite (M_LED_BLAU1, LED_AUS);
  digitalWrite (M_LED_GRUEN1, LED_EIN);
  digitalWrite (M_LAMP_IRRIGHT, LED_HELL);
  digitalWrite (M_LAMP_IRLEFT,  LED_HELL);
  Automatic = true;

//  UNTIL_ABORTED // ********************** Schleife bis externes Signal ***************************
//                // *******************************************************************************

  time_t AbfrageStart = time(0);
  DO_FOREVER  // >>>>>>>>>>>>>>>>---- Arbeitsschleife ---->>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  {

    feedWatchdog(PROGNAME);

    // *+++++* Sensoren bedienen **++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*
    // ------------------------------------------------------------------------------------------
    static bool done=false;
    static bool ldone=false;
    static int lcnt = 5;
    long Zeitpunkt = (time(0)-AbfrageStart) % SENDETAKT;
    switch (Zeitpunkt)
    {
      case SENDETAKT/5*0:
        if (!done)
          DEBUG("\n");
        done=true;
        break;
        
      case SENDETAKT/5*1:     // interne CPU-Temperatur lesen
        if (!done)
          readCPUtemp(mqtt);
        done=true;
        break;

      case SENDETAKT/5*2:     // ds18b20-Sensoren lesen
        if ((ds18b20anz > 0) && !done)
          readds18b20(ds18b20anz, mqtt);
        done=true;
        break;

      case SENDETAKT/5*3:     // BME280-Sensor lesen
        if ((bme280) && !done)
          readbme280(mqtt);
        done=true;
        break;

      case SENDETAKT/5*4:     // tsl2561-Sensor (Helligkeit) lesen
        if ((tsl != NULL) && !done)
          readtsl2561(tsl, mqtt);
        done=true;
        break;

      default:                // nichts oder sonstwas
        done = false;
        break;
    } // --- end switch ---


    // *+++++* MQTT bedienen **++++++++++++++++++++++++++++++++++++++++++++++++++++*
    // -----------------------------------------------------------------------------
    static bool msg=false;
    static bool lmsg=false;
    {
      char Topic[ZEILE]={'\0'};
      char Payload[ZEILE]={'\0'};
      int antwort = RESP_KEINEANTWORT;


      // auf empfangene Messages testen
      // ================================
      enum TOPICMODE Status = MQTT_Loop(mqtt, Topic, Payload);
      DEBUG_MQTT("\n>> %s  %s()#%d - neue Message: CompID '%d' == s_IPnmr '%d'?\n",
                                        __S__, getInt(Topic, "IP-Nummer", TPOS_IPA), s_IPnmr);

      // empfangene Messages holen
      // -------------------------
      msg=false;

      // Test auf eigene Message
      // -----------------------
      int CompIP = getInt(Topic, "IP-Nummer", TPOS_IPA);  // letzte Stelle IP des Senders
 DEBUG("- %d -- '%d' --\n", __LINE__, CompIP);
 DEBUG("- %d -- '%d' --\n", __LINE__, s_IPnmr);
      if (CompIP == s_IPnmr)
      { // überspringen
        // -------------
        DEBUG("\n>> %s-%s()#%d - neue Message: CompID '%d' == s_IPnmr '%d'!\n\n",
                                                                      __S__, CompIP, s_IPnmr);
        Status = MODE_NOTHING;
      }
 DEBUG("- %d -\n", __LINE__);

      Status = MODE_NOTHING;

      if (Status > MODE_NOTHING)
      { // eine Message ist eingetroffen !!
        // =======================================================================
        DEBUG("\n>> %s-%s()#%d: Status: %d -- Topic: \"%s\" -- Payload: \"%s\"\n",
                                                               __S__, Status, Topic, Payload);

#ifdef __LAMPMOTION_MYLOG__
        { // --- Log-Ausgabe ----------------------------------------------------------------
          // zum Debuggen Klartext einfügen
          char LogText[ZEILE];
          char Messagetyp[NOTIZ] = "";
          char Messageinfo[NOTIZ] = "";
          switch (Status)
          {
            case MODE_ADMIN:    // Verwaltung
              sprintf(Messagetyp, "%5d(\"%s\")", MSG_ADMIN,   "Verwaltung");
              break;

            case MODE_COMMAND:  // Schaltbefehl
              sprintf(Messagetyp, "%5d(\"%s\")", MSG_COMMAND, "Schaltbefehl");
              if (matchn(Topic, TPOS_INF, INF_REQFACK))
                sprintf(Messageinfo, "%5d(\"%s\")", INF_REQFACK, "Bitte um Quittung");
              break;

            default:            // sonstwas
              antwort = RESP_NOSUPPORT;
              break;

          } // --- end switch ---
          if (strlen(Messagetyp) > 0)
            setStr(Topic, TPOS_TYP, Messagetyp);
          if (strlen(Messageinfo) > 0)
            setStr(Topic, TPOS_INF, Messageinfo);
          sprintf(LogText, "%d - Topic: \"%s\" - Payload: \"%s\"", Status, Topic, Payload);
          MYLOG(LogText);
        } // ---------------------------------------------------------------------------------
#endif

        // in 'Status' steht, wie es weitergeht
        // ------------------------------------
        // die übermittelten Infos sind in Topic und Payload

        switch (Status)
        {
          case MODE_ADMIN:    // Verwaltung
            antwort = RESP_NOSUPPORT;
            break;

          case MODE_COMMAND:  // Schaltbefehl
            antwort = CommandMessage(Topic, Payload);
            break;

          default:            // sonstwas
            antwort = RESP_NOSUPPORT;
            break;
        } // --- end switch ---
        msg=true;
      }

      DEBUG("\n>> %s-%s()#%d: antwort = %d\n",
                                   __NOW__, __FUNCTION__, __LINE__, antwort);

      switch (antwort)
      { // ggf. die empfangene Message beantworten
        // ---------------------------------------
        case RESP_QUITTUNG:
          // die Message ist schon fertig zusammengestellt
          // ---------------------------------------------
          break;

        case RESP_NOSUPPORT:
          // Die Anforderung wird nicht unterstützt
          // --------------------------------------
          ReqNotSupported(Topic, Payload);
          ReplyMessage(mqtt, Topic, Payload);
          antwort = RESP_KEINEANTWORT;
          break;

        case RESP_FEHLER:
          // die Anforderung enthält Fehler
          // ------------------------------
          // weitere Info ggf. in Payload
          ReqFehlerhaft(Topic, Payload, "das war nix");
          ReplyMessage(mqtt, Topic, Payload);
          antwort = RESP_KEINEANTWORT;
          break;

        default:                  // alles andere
          DEBUG("\n>> %s-%s()#%d: antwort = default\n",
                                   __NOW__, __FUNCTION__, __LINE__);
          break;
      } // --- end switch ---

      if ( antwort != RESP_KEINEANTWORT)
        ReplyMessage(mqtt, Topic, Payload);

    }   // -- end MQTT ----------------------------------------------------------------------

 DEBUG("- %d -\n", __LINE__);

    // LED-Steuerung
    // -------------
    if (done != ldone)
    {
      ldone = done;
      if (done)
        lcnt = 3;
    }
    if (msg != lmsg)
    {
      lmsg = msg;
      if (msg)
        lcnt = 3;
    }
    digitalWrite (M_LED_BLAU1, (--lcnt > 0) ? LED_EIN : LED_AUS);
    static int blink = (STD*4)-20;
    blink++;
    if (blink % 5 == 0)
    {
      digitalWrite (M_LED_GRUEN1, LED_AUS);
      delay(100);
      digitalWrite (M_LED_GRUEN1, LED_EIN);
    }

    if (!Automatic)
    { // Handsteuerung
      // -------------
    }

    else if (Zeitfenster(GUTENMORGEN, GUTENACHT))
    { // Normalbetrieb (Automatic!)
      // --------------------------
      DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
      digitalWrite (M_LAMP_IRRIGHT, LED_HELL);
      digitalWrite (M_LAMP_IRLEFT,  LED_HELL);
//      if (((blink % (STD*4)) == 0))     // entspricht ca. 1:07 Stunden

      time_t tnow;
      time(&tnow);
      if ((tnow % (2*STD)) == 0)              // alle 2 Stunden zur vollen Stunde
      { // mit IR-Lampen Photo auslösen
        // ----------------------------
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "      IR-Lampen Aus/Ein");
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
        digitalWrite (M_LAMP_IRRIGHT, LED_DUNKEL);
        digitalWrite (M_LAMP_IRLEFT,  LED_DUNKEL);
        delay(10*SEC);
        digitalWrite (M_LAMP_IRRIGHT, LED_HELL);
        digitalWrite (M_LAMP_IRLEFT,  LED_HELL);
        DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
      }
    }
    else
    { // Nachts ist Ruhe!
      // -----------------
      DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
      digitalWrite (M_LAMP_IRRIGHT, LED_DUNKEL);
      digitalWrite (M_LAMP_IRLEFT,  LED_DUNKEL);
    }

    if (ErrorFlag > 0)
    { // rote LED wieder ausschalten
      // ---------------------------
      if (time(0) > ErrorFlag)
      { // wenn Zeit abgelaufen
        // --------------------
        ErrorFlag = 0;
        digitalWrite (M_LED_ROT, LED_AUS);
      }
    }

    delay(ABFRAGETAKT);

  } // ===========================================================================================

  tsl2561_close(tsl);

  // PID-Datei wieder löschen
  // ------------------------
  killPID();

  { // --- Log-Ausgabe ------------------------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "    <<<----- Programm beendet ----->>>");
    MYLOG(LogText);
  } // ----------------------------------------------------------------------------------------


  // Fehler-Mail abschicken
  // ----------------------
//***  char MailBody[BODYLEN] = {'\0'};
  char Logtext[ZEILE];
  digitalWrite (M_LED_ROT, LED_EIN);
  sprintf(Logtext, ">> %s()#%d: Error %s ---> '%s' OK\n",__FUNCTION__, __LINE__, PROGNAME, "lastItem");
  syslog(LOG_NOTICE, "%s: %s", __LAMP__, Logtext);

  strcat(MailBody, Logtext);
  char Betreff[ERRBUFLEN];
  DEBUG(Betreff, "Error-Message von %s: >>%s<<", PROGNAME, "lastItem");
  sendmail(Betreff, MailBody);                  // Mail-Message absetzen
}
//************************************************************************************************
