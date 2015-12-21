SUNDOG = ../../../sundog_engine/
SUNVOX = ../../../sunvox_engine/

APPNAME = sunvox_test

include $(SUNDOG)various/sundog_makefile.inc
include $(SUNVOX)sunvox_makefile.inc

## ------------------------------------------ ##
## Main target options:                       ##
## ------------------------------------------ ##

CFLAGS = -O3 -s
#CFLAGS = -g

ifeq ($(MAKECMDGOALS),palm)
    SDK = /usr/local/share/palmdev/sdk-5r3/include/
    SUNDOG_DEFINES = \
	-DCOLOR8BITS \
	-DGRAYSCALE \
	-DDIRECTDRAW \
	-DNOSTORAGE \
	-DSTYPE_INT16
endif
ifeq ($(MAKECMDGOALS),linux)
    #(remove -lrt on FreeBSD and Cygwin)
    # X11 (OpenGL) libs:
    #LIBS = -L/usr/X11R6/lib -lpthread -lrt -lstdc++ -lX11 -lGL -lXext -lasound
    # X11 libs:
    #LIBS = -L/usr/X11R6/lib -lpthread -lrt -lstdc++ -lX11 -lasound
    # SDL:
    LIBS = -L/usr/X11R6/lib -lpthread -lrt -lstdc++ -lSDL -lasound
    SUNDOG_DEFINES = \
	-DDIRECTDRAW \
	-DCOLOR32BITS \
	-DSTYPE_FLOAT
endif
ifeq ($(MAKECMDGOALS),win32)
    SUNDOG_DEFINES = \
	-DCOLOR32BITS \
	-DGDI \
	-DSTYPE_FLOAT
endif
ifeq ($(MAKECMDGOALS),wince)
    SUNDOG_DEFINES = \
	-DCOLOR16BITS \
	-DDIRECTDRAW \
	-DRGB565 \
	-DGDI \
	-DSTYPE_INT16
endif

## ------------------------------------------ ##
## Object files and includes:                 ##
## ------------------------------------------ ##

# User application:
ADD_LIBS = \
    $(LIB_SUNVOX)
OBJECTS += user_code.o $(ADD_LIBS)

## ------------------------------------------ ##
## User application rules:                    ##
## ------------------------------------------ ##

user_code.o: ../main/user_code.cpp $(STD_INCS)
	$(CC) $(DEFINES) -c ../main/user_code.cpp
