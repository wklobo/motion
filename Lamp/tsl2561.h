//***************************************************************************//
//*                                                                         *//
//* File:          tls2561.h                                                *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2021-04-12;                                              *//
//* Last change:   2021-09-10 - 13:49:15: neuer Ansatz                      *//
//* Description:   Treiber für Helligkeits-Sensor TSL2561                   *//
//*                Treiber für CPU-Temperatur                               *//
//*                                                                         *//
//* angepasst für 'Buster' -- 2021-04-12 -- Wolfgang Keuch                  *//
//*                                                                         *//
//*                                                                         *//
//***************************************************************************//
/*
/*
 * @author  Alexander Rüedlinger <a.rueedlinger@gmail.com>
 * @date  22.02.2015
 *
 * A driver written in C for the sensor TSL2561.
 *
 * This driver is a part of the Adafruit TSL2561 Light Sensor Driver.
 *
 * The original driver is written by Kevin (KTOWN) Townsend
 * for Adafruit Industries.
 *
 * source: https://github.com/adafruit/Adafruit_TSL2561
 *
 */


#ifndef _TLS2561_H
#define _TLS2561_H  1


#define TSL2561_I2C_ADDR_LOW     0x29
#define TSL2561_I2C_ADDR_DEFAULT 0x39
#define TSL2561_I2C_ADDR_HIGH    0x49


#define TSL2561_INTEGRATION_TIME_13MS  0x00
#define TSL2561_INTEGRATION_TIME_101MS 0x01
#define TSL2561_INTEGRATION_TIME_402MS 0x02


#define TSL2561_GAIN_0X  0x00
#define TSL2561_GAIN_16X 0x10

int tsl2561_enable(void *_tsl);
int tsl2561_disable(void *_tsl);

void* tsl2561_init();                 // Initialisierung des Treibers
void tsl2561_close(void* _tsl);       // Schließen des Treibers

void tsl2561_set_timing(void *_tsl, int integration_time, int gain);
void tsl2561_set_gain(void *_tsl, int gain);
void tsl2561_set_integration_time(void *_tsl, int ingeration_time);
void tsl2561_set_type(void *_tsl, int type);

void tsl2561_read(void *_tsl, int *visible, int *ir);
long tsl2561_lux(void* _tsl);         // Abfrage Lux-Wert
void tsl2561_luminosity(void *_tsl, int *visible, int *ir);

void tsl2561_enable_autogain(void *_tsl);
void tsl2561_disable_autogain(void *_tsl);

#endif //_TLS2561_H

//***************************************************************************//
