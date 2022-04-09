//******************************************************************************//
//*                                                                            *//
//* File:          bme280.c                                                    *//
//* Author:        übernommen 2018-08-03 - an LampMotion angepasst -- wkh      *//
//* Creation date: 2018-08-03;                                                 *//
//* Last change:   2021-09-20 - 09:54:47                         *//
//* Description:   BME 280 - Temperatur- und Luftdrucksensor lesen             *//
//*                                                                            *//
//*                                                                            *//
//******************************************************************************//

#define __BME280_DEBUG__   false        /* Debug-Funktion */

/*

Modified BSD License
====================

Copyright © 2016, Andrei Vainik
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the organization nor the
   names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This piece of code was combined from several sources
https://github.com/adafruit/Adafruit_BME280_Library
https://cdn-shop.adafruit.com/datasheets/BST-BME280_DS001-10.pdf
https://projects.drogon.net/raspberry-pi/wiringpi/i2c-library/

Compensation functions and altitude function originally from:
https://github.com/adafruit/Adafruit_BME280_Library/blob/master/Adafruit_BME280.cpp
***************************************************************************
  This is a library for the BME280 humidity, temperature & pressure sensor
  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650
  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface.
  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!
  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ***************************************************************************

****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <wiringPi.h>

#define EXTERN
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <mysql.h>
#include <stdbool.h>
#include <wiringPiI2C.h>

#include "./bme280.h"
#include "../error.h"
#include "../common.h"
#include "../datetime.h"

//***************************************************************************//
/*
 * Define debug function.
 * ---------------------
 */
#if __BME280_DEBUG__
#define DEBUG(...)  printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

//*********************************************************************************************//

#define BME280_ADDRESS                0x76

#define BME280_REGISTER_DIG_T1        0x88
#define BME280_REGISTER_DIG_T2        0x8A
#define BME280_REGISTER_DIG_T3        0x8C
#define BME280_REGISTER_DIG_P1        0x8E
#define BME280_REGISTER_DIG_P2        0x90
#define BME280_REGISTER_DIG_P3        0x92
#define BME280_REGISTER_DIG_P4        0x94
#define BME280_REGISTER_DIG_P5        0x96
#define BME280_REGISTER_DIG_P6        0x98
#define BME280_REGISTER_DIG_P7        0x9A
#define BME280_REGISTER_DIG_P8        0x9C
#define BME280_REGISTER_DIG_P9        0x9E
#define BME280_REGISTER_DIG_H1        0xA1
#define BME280_REGISTER_DIG_H2        0xE1
#define BME280_REGISTER_DIG_H3        0xE3
#define BME280_REGISTER_DIG_H4        0xE4
#define BME280_REGISTER_DIG_H5        0xE5
#define BME280_REGISTER_DIG_H6        0xE7
#define BME280_REGISTER_CHIPID        0xD0
#define BME280_REGISTER_VERSION       0xD1
#define BME280_REGISTER_SOFTRESET     0xE0
#define BME280_RESET                  0xB6
#define BME280_REGISTER_CAL26         0xE1
#define BME280_REGISTER_CONTROLHUMID  0xF2
#define BME280_REGISTER_CONTROL       0xF4
#define BME280_REGISTER_CONFIG        0xF5
#define BME280_REGISTER_PRESSUREDATA  0xF7
#define BME280_REGISTER_TEMPDATA      0xFA
#define BME280_REGISTER_HUMIDDATA     0xFD

#define MEAN_SEA_LEVEL_PRESSURE       1013

/*
* Immutable calibration data read from bme280
* ===========================================
*/
typedef struct
{
  uint16_t dig_T1;
  int16_t  dig_T2;
  int16_t  dig_T3;

  uint16_t dig_P1;
  int16_t  dig_P2;
  int16_t  dig_P3;
  int16_t  dig_P4;
  int16_t  dig_P5;
  int16_t  dig_P6;
  int16_t  dig_P7;
  int16_t  dig_P8;
  int16_t  dig_P9;

  uint8_t  dig_H1;
  int16_t  dig_H2;
  uint8_t  dig_H3;
  int16_t  dig_H4;
  int16_t  dig_H5;
  int8_t   dig_H6;
} bme280_calib_data;

/*
* Raw sensor measurement data from bme280
* =======================================
*/
typedef struct
{
  uint8_t pmsb;
  uint8_t plsb;
  uint8_t pxsb;

  uint8_t tmsb;
  uint8_t tlsb;
  uint8_t txsb;

  uint8_t hmsb;
  uint8_t hlsb;

  uint32_t temperature;
  uint32_t pressure;
  uint32_t humidity;

} bme280_raw_data;

//*********************************************************************************************//

// Funktionsübergreifende Variable
// --------------------------------
static int i2cfd;
bme280_calib_data cal;

// lokale Funktionen
// --------------------
void readCalibrationData(int fd, bme280_calib_data *cal);
int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T);
float compensateTemperature(int32_t t_fine);
float compensatePressure(int32_t adc_P, bme280_calib_data *cal, int32_t t_fine);
float compensateHumidity(int32_t adc_H, bme280_calib_data *cal, int32_t t_fine);
void getRawData(int fd, bme280_raw_data *raw);
float getAltitude(float pressure);

//*********************************************************************************************//

// fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showbme280_Error( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  sprintf( Fehler, "%s - Err %d-%s", Message, errsv, strerror(errsv));
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  printf("    -- Fehler -->  %s\n", ErrText);   // lokale Fehlerausgabe
  digitalWrite (LED_GELB,   LED_AUS);
  digitalWrite (LED_GRUEN,  LED_AUS);
  digitalWrite (LED_BLAU,   LED_AUS);
  digitalWrite (LED_ROT,    LED_EIN);

  finish_with_Error(ErrText);                   // Fehlermeldung ausgeben
}
//*********************************************************************************************//

bool bme280_Init()                                               // Initialisierung des Sensors
{
  i2cfd = wiringPiI2CSetup(BME280_ADDRESS);
  DEBUG(stdout, "%s()#%d: wiringPiI2CSetup(%d): %d\n",
                __FUNCTION__,__LINE__, BME280_ADDRESS, i2cfd);
  if(i2cfd < 0)
  {
    return false;
  }

  readCalibrationData(i2cfd, &cal);

  wiringPiI2CWriteReg8(i2cfd, 0xf2, 0x01);   // humidity oversampling x 1
  wiringPiI2CWriteReg8(i2cfd, 0xf4, 0x25);   // pressure and temperature oversampling x 1, mode normal

  return (true);
}
//*********************************************************************************************//

int bme280_Read(float* Pressure, float* Temperature, float* Humidity)         // Werte lesen
{
  bme280_raw_data raw;
  getRawData(i2cfd, &raw);

  int32_t t_fine = getTemperatureCalibration(&cal, raw.temperature);
  float t = compensateTemperature(t_fine);                        // °C
  float p = compensatePressure(raw.pressure, &cal, t_fine) / 100; // hPa
  float h = compensateHumidity(raw.humidity, &cal, t_fine);       // %
//  float a = getAltitude(p);                                       // meters

  DEBUG(stdout, "%s()#%d: Sensor BME280 - {\"sensor\":\"bme280\", \"humidity\":%.2f, \"pressure\":%.2f,"
    " \"temperature\":%.2f, \"altitude\":%.2f, \"timestamp\":%d}\n",
    __FUNCTION__,__LINE__, h, p, t, a, (int)time(NULL));

  // Rückgabe: die Ergebnisse
  // ------------------------
  *Pressure = p;
  *Temperature = t;
  *Humidity = h;

  return true;
}
//*********************************************************************************************//

int bme280_Close()
{
  return(0);
}
//*********************************************************************************************//

int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T)
{
  int32_t var1  = ((((adc_T>>3) - ((int32_t)cal->dig_T1 <<1))) *
     ((int32_t)cal->dig_T2)) >> 11;

  int32_t var2  = (((((adc_T>>4) - ((int32_t)cal->dig_T1)) *
       ((adc_T>>4) - ((int32_t)cal->dig_T1))) >> 12) *
     ((int32_t)cal->dig_T3)) >> 14;

  return var1 + var2;
}
//*********************************************************************************************//

void readCalibrationData(int fd, bme280_calib_data *data)
{
  data->dig_T1 = (uint16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T1);
  data->dig_T2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T2);
  data->dig_T3 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T3);

  data->dig_P1 = (uint16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P1);
  data->dig_P2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P2);
  data->dig_P3 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P3);
  data->dig_P4 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P4);
  data->dig_P5 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P5);
  data->dig_P6 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P6);
  data->dig_P7 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P7);
  data->dig_P8 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P8);
  data->dig_P9 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P9);

  data->dig_H1 = (uint8_t)wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H1);
  data->dig_H2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_H2);
  data->dig_H3 = (uint8_t)wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H3);
  data->dig_H4 = (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H4) << 4) | (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H4+1) & 0xF);
  data->dig_H5 = (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H5+1) << 4) | (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H5) >> 4);
  data->dig_H6 = (int8_t)wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H6);
}
//*********************************************************************************************//

float compensateTemperature(int32_t t_fine) {
  float T  = (t_fine * 5 + 128) >> 8;
  return T/100;
}
//*********************************************************************************************//

float compensatePressure(int32_t adc_P, bme280_calib_data *cal, int32_t t_fine) {
  int64_t var1, var2, p;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)cal->dig_P6;
  var2 = var2 + ((var1*(int64_t)cal->dig_P5)<<17);
  var2 = var2 + (((int64_t)cal->dig_P4)<<35);
  var1 = ((var1 * var1 * (int64_t)cal->dig_P3)>>8) +
    ((var1 * (int64_t)cal->dig_P2)<<12);
  var1 = (((((int64_t)1)<<47)+var1))*((int64_t)cal->dig_P1)>>33;

  if (var1 == 0) {
    return 0;  // avoid exception caused by division by zero
  }
  p = 1048576 - adc_P;
  p = (((p<<31) - var2)*3125) / var1;
  var1 = (((int64_t)cal->dig_P9) * (p>>13) * (p>>13)) >> 25;
  var2 = (((int64_t)cal->dig_P8) * p) >> 19;

  p = ((p + var1 + var2) >> 8) + (((int64_t)cal->dig_P7)<<4);
  return (float)p/256;
}
//*********************************************************************************************//

float compensateHumidity(int32_t adc_H, bme280_calib_data *cal, int32_t t_fine)
{
  int32_t v_x1_u32r;

  v_x1_u32r = (t_fine - ((int32_t)76800));

  v_x1_u32r = (((((adc_H << 14) - (((int32_t)cal->dig_H4) << 20) -
      (((int32_t)cal->dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
         (((((((v_x1_u32r * ((int32_t)cal->dig_H6)) >> 10) *
        (((v_x1_u32r * ((int32_t)cal->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
      ((int32_t)2097152)) * ((int32_t)cal->dig_H2) + 8192) >> 14));

  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
           ((int32_t)cal->dig_H1)) >> 4));

  v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
  v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
  float h = (v_x1_u32r>>12);
  return  h / 1024.0;
}
//*********************************************************************************************//

void getRawData(int fd, bme280_raw_data *raw)
{
  wiringPiI2CWrite(fd, 0xf7);

  raw->pmsb = wiringPiI2CRead(fd);
  raw->plsb = wiringPiI2CRead(fd);
  raw->pxsb = wiringPiI2CRead(fd);

  raw->tmsb = wiringPiI2CRead(fd);
  raw->tlsb = wiringPiI2CRead(fd);
  raw->txsb = wiringPiI2CRead(fd);

  raw->hmsb = wiringPiI2CRead(fd);
  raw->hlsb = wiringPiI2CRead(fd);

  raw->temperature = 0;
  raw->temperature = (raw->temperature | raw->tmsb) << 8;
  raw->temperature = (raw->temperature | raw->tlsb) << 8;
  raw->temperature = (raw->temperature | raw->txsb) >> 4;

  raw->pressure = 0;
  raw->pressure = (raw->pressure | raw->pmsb) << 8;
  raw->pressure = (raw->pressure | raw->plsb) << 8;
  raw->pressure = (raw->pressure | raw->pxsb) >> 4;

  raw->humidity = 0;
  raw->humidity = (raw->humidity | raw->hmsb) << 8;
  raw->humidity = (raw->humidity | raw->hlsb);
}
//*********************************************************************************************//

float getAltitude(float pressure)
{
  // Equation taken from BMP180 datasheet (page 16):
  //  http://www.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf

  // Note that using the equation from wikipedia can give bad results
  // at high altitude.  See this thread for more information:
  //  http://forums.adafruit.com/viewtopic.php?f=22&t=58064

  return 44330.0 * (1.0 - pow(pressure / MEAN_SEA_LEVEL_PRESSURE, 0.190294957));
}
//*********************************************************************************************//
