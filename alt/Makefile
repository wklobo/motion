##*********************************************************************************************//
##*                                                                                           *//
##* File:          Makefile                                                                   *//
##* Author:        Wolfgang Keuch                                                             *//
##* Creation date: 2020-12-30;                                                                *//
##* Last change:   2020-12-30;                                                                *//
##* Description:   Makefile f�r 'FifoMotion'                                                  *//
##*                                                                                           *//
##*********************************************************************************************//
 
DIR_MAIL = ../../sendmail
DIR_OBJ = ./../obj
DIR_BIN = ./../bin

OBJ_C = $(wildcard ${DIR_FONTS}/*.c ${DIR_OBJ}/*.c ${DIR_MAIL}/*.c ./*.c )
#OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))
OBJ_O = ${DIR_BIN}/%.o

TARGET  = FifoMotion
VERSION = ./../version.h
#BIN_TARGET = ${DIR_BIN}/${TARGET}

CC = gcc

# -I Pfad f�r die Headerdateien (include)
INCLUDE	= -I/usr/include -I/usr/local/include -I/usr/include/mysql -I$(DIR_MAIL)
# -g Symboltabelle f�r Debugger an Code anf�gen
DEBUG = -g -O0 -Wall '-D PROGNAME="${TARGET}"'
# -l bindet zus�tzliche Programm-Bibliotheken (Libraries) ein
CFLAGS += $(DEBUG) -lmysqlclient -lmosquitto
# -W Warnings

LIB = -lwiringPi

# neue Versionsnummer
# --------------------
&(VERSION) : ${TARGET} 
	./../version.py

# die OBJs zusammenbinden:
# ------------------------
# -o der nachfolgende Name soll als Programmname verwendet werden
# -l bindet zus�tzliche Programm-Bibliotheken (Libraries) ein
# -I Pfad f�r die Headerdateien (include) 
# -g Symboltabelle f�r Debugger an Code anf�gen
${TARGET} : ${OBJ_O}
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIB) $(INCLUDE)

# Mail:
# -----
${DIR_BIN}/%.o : $(DIR_MAIL)/%.c
	$(CC) $(CFLAGS) -c  $< -o $@

# meine Dateien:
# --------------
${DIR_BIN}/%.o : ./%.c  ./%.h  Makefile
	$(CC) $(CFLAGS) -c  $< -o $@  $(LIB)  $(INCLUDE)

#${DIR_BIN}/fifomotion.o : ./fifomotion.c  ./../%.h  Makefile
${DIR_BIN}/fifomotion.o : ./fifomotion.c   Makefile
#	$(CC) $(CFLAGS) -c  $< -o $@  $(LIB)  $(INCLUDE)
	$(CC) $(CFLAGS) -c  $< -o $(LIB)  $(INCLUDE)

# saubermachen:
# -------------
clean :
	$(RM) $(DIR_BIN)/*.* $(TARGET) 
#	/home/pi/Sicherung.sh

##*********************************************************************************************//
#make: *** Keine Regel vorhanden, um das Ziel �../bin/fifomotion.o�,
#  ben�tigt von �FifoMotion�, zu erstellen.  Schlu