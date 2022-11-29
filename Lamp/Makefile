##*********************************************************************************************//
##*                                                                                           *//
##* File:          Makefile                                                                   *//
##* Author:        Wolfgang Keuch                                                             *//
##* Creation date: 2021-04-05                                                                 *//
##* Last change:   2022-11-29 - 18:25:42                                                      *//
##* Description:   Makefile für 'LampMotion'                                                  *//
##*                                                                                           *//
##*********************************************************************************************//

SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .o
CC = gcc


TOPNAME  = motion
MAINFILE = lampmotion
TARGET   = LampMotion

DIR_BASE = /home/pi
DIR_TOP  = $(DIR_BASE)/$(TOPNAME)


DIR_AUX =     $(DIR_TOP)/aux
DIR_PIX =     $(DIR_TOP)/pix
DIR_HOME =    $(DIR_TOP)/Lamp
DIR_BIN =     ${DIR_HOME}/obj

DIR_TREIBER = $(DIR_BASE)/treiber
DIR_MAIL    =	$(DIR_TREIBER)/sendmail
DIR_COMMON  = $(DIR_TREIBER)/common
DIR_SENSOR  = $(DIR_TREIBER)/sensor
DIR_DBANK   = $(DIR_TREIBER)/dbank
DIR_SNIPPET = $(DIR_TREIBER)/snippets

VERSION     = ${DIR_HOME}/version.h


OBJ_C = $(wildcard  ${DIR_HOME}/*.c  ${DIR_MAIL}/*.c    ${DIR_COMMON}/*.c  ${DIR_SENSOR}/*.c  ${DIR_DBANK}/*.c   )
OBJ_H = $(wildcard  ${DIR_MAIL}/*.h  ${DIR_COMMON}/*.h  ${DIR_SENSOR}/*.h  ${DIR_DBANK}/*.h   $(DIR_SNIPPET)/*.snip   )
OBJ_O = $(patsubst %.c, ${DIR_BIN}/%.o, $(notdir ${OBJ_C}))


DEBUG = -g -O0  -Wall -Wno-format-overflow -Wno-unused-function  -Wno-unused-value -Wno-comment
PARAM1 =  '-D PROGNAME="${TARGET}"' '-D VOGEL=1'  
PARAM2 =  '-D TOPDIR="${DIR_TOP}"'  '-D AUXDIR="${DIR_AUX}"' '-D PIXDIR="${DIR_PIX}"' '-D TOPNAME="${TOPNAME}"'
CFLAGS += $(DEBUG) $(PARAM1)  $(PARAM2) -lmysqlclient  -lmosquitto  -li2c


LIBS = -lwiringPi -lmysqlclient  -lmosquitto  -lm  -li2c
INCLUDE	= -I/usr/include -I/usr/local/include -I/usr/include/mysql  


# die OBJs zusammenbinden:
# ------------------------
# Listen zum Debuggen!
$(TARGET) : ${OBJ_O}
	@echo
#	@echo   ----- C-Liste: ${OBJ_C} -----
#	@echo
#	@echo   ----- H-Liste: ${OBJ_H} -----
#	@echo
#	@echo   ----- Objekt-Liste: ${OBJ_O} -----
#	@echo
	@echo --- Making ${TARGET} ---
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIBS) $(INCLUDE)  
	./version.py


### Hauptdatei:
### --------------
${DIR_BIN}/$(MAINFILE).o : $(MAINFILE).c  $(MAINFILE).h  Makefile   $(OBJ_C)  $(OBJ_H) # sensordb.h    
	@echo
	@echo -- T -- Compiling ${srcdir}/$(notdir $<)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


## Hauptverzeichnis:
## ------------------
${DIR_BIN}/%.o : $(DIR_HOME)/%.c $(DIR_HOME)/%.h  $(DIR_HOME)/Makefile # $(DIR_HOME)/sensordb.h  
	@echo
	@echo -- H -- Compiling $(DIR_HOME)/$(notdir $<)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


## Mail:
## -----
${DIR_BIN}/%.o : $(DIR_MAIL)/%.c $(DIR_MAIL)/%.h  Makefile $(DIR_MAIL)/sendMail.py
	@echo
	@echo -- M -- Compiling $(DIR_MAIL)/$(notdir $<)
	$(CC) $(CFLAGS) -c	$< -o $@


## Sensor:
## --------
${DIR_BIN}/%.o : $(DIR_SENSOR)/%.c $(DIR_SENSOR)/%.h  Makefile
	@echo
	@echo -- S -- Compiling ${DIR_SENSOR}/$(notdir $<)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


## Common:
## --------
${DIR_BIN}/%.o : $(DIR_COMMON)/%.c	$(DIR_COMMON)/%.h  Makefile
	@echo
	@echo -- C -- Compiling ${DIR_COMMON}/$(notdir $<)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


## DBank:
## --------
${DIR_BIN}/%.o : $(DIR_DBANK)/%.c	$(DIR_DBANK)/%.h	$(DIR_DBANK)/sensor_db.h   Makefile
	@echo
	@echo -- D -- Compiling ${DIR_DBANK}/$(notdir $<)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


# saubermachen:
# -------------
clean :
	$(RM) $(DIR_BIN)/*.* $(TARGET) $(DIR_OBJ)/.*.sw?
#	/home/pi/Sicherung.sh

##*********************************************************************************************//
