SUNDOG = ../../../sundog_engine/
SUNVOX = ../../../sunvox_engine/
PSYTEXX = ../main/lib_psytexx/
PSYCODE = $(PSYTEXX)code/

APPNAME = mod2sunvox

SUNDOG_NOWINDOWS = true
SUNDOG_NOMAIN = true
include $(SUNDOG)various/sundog_makefile.inc
include $(SUNVOX)sunvox_makefile.inc

## ------------------------------------------ ##
## Main target options:                       ##
## ------------------------------------------ ##

CFLAGS = -O3 -s
#CFLAGS = -g

ifeq ($(MAKECMDGOALS),linux)
    LIBS = -lpthread -lrt -lstdc++
    SUNDOG_DEFINES = \
	-DNOSOUND \
	-DLIB_SUNVOX \
	-DSTYPE_FLOAT
endif
ifeq ($(MAKECMDGOALS),win32)
    SUNDOG_DEFINES = \
	-DNOSOUND \
	-DLIB_SUNVOX \
	-DSTYPE_FLOAT
endif

## ------------------------------------------ ##
## Object files and includes:                 ##
## ------------------------------------------ ##

LIB_PSYTEXX = \
    xm_channels.o \
    xm_instrum.o \
    xm_main.o \
    xm_pattern.o \
    xm_play.o \
    xm_sample.o \
    xm_song.o \
    xm_tables.o
ADD_LIBS = \
    $(LIB_SUNVOX) \
    $(LIB_PSYTEXX)
OBJECTS += mod2sunvox.o $(ADD_LIBS)

## ------------------------------------------ ##
## User application rules:                    ##
## ------------------------------------------ ##

mod2sunvox.o: ../main/mod2sunvox.cpp
	$(CC) $(DEFINES) -c ../main/mod2sunvox.cpp

xm_channels.o: $(PSYCODE)xm_channels.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_channels.cpp

xm_instrum.o: $(PSYCODE)xm_instrum.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_instrum.cpp

xm_main.o: $(PSYCODE)xm_main.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_main.cpp

xm_pattern.o: $(PSYCODE)xm_pattern.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_pattern.cpp

xm_play.o: $(PSYCODE)xm_play.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_play.cpp

xm_sample.o: $(PSYCODE)xm_sample.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_sample.cpp

xm_song.o: $(PSYCODE)xm_song.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_song.cpp

xm_tables.o: $(PSYCODE)xm_tables.cpp
	$(CC) $(DEFINES) -c $(PSYCODE)xm_tables.cpp
