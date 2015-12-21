ifeq ($(MAKECMDGOALS),)
    $(error No target selected. Please type "make palm", "make linux", "make win32" or "make wince")
    exit
endif

## ------------------------------------------ ##
## Main target options:                       ##
## ------------------------------------------ ##

ifeq ($(MAKECMDGOALS),palm)
    MCC = m68k-palmos-gcc $(CFLAGS) -Wno-multichar
    CC = arm-elf-gcc $(CFLAGS) -Wno-multichar
    TARGET_DEFINES = \
	-DARCH_ARM \
	-DPALMOS
endif
ifeq ($(MAKECMDGOALS),linux)
    CC = gcc $(CFLAGS) -Wno-multichar -malign-double
    CXX = gcc $(CFLAGS) -Wno-multichar -malign-double
    TARGET_DEFINES = \
	-DARCH_X86 \
	-DLINUX
endif
ifeq ($(MAKECMDGOALS),win32)
    CC = i686-pc-mingw32-gcc $(CFLAGS) -fno-exceptions -Wno-multichar -malign-double
    CXX = i686-pc-mingw32-gcc $(CFLAGS) -fno-exceptions -Wno-multichar -malign-double
    TARGET_DEFINES = \
	-DARCH_X86 \
	-DWIN
endif
ifeq ($(MAKECMDGOALS),wince)
    CC = arm-mingw32ce-gcc $(CFLAGS) -fno-exceptions -Wno-multichar
    CXX = arm-mingw32ce-gcc $(CFLAGS) -fno-exceptions -Wno-multichar
    TARGET_DEFINES = \
	-DARCH_ARM \
	-DWINCE
endif

## ------------------------------------------ ##
## Directories:                               ##
## ------------------------------------------ ##

MAIN = $(SUNDOG)main/
CMAIN = $(MAIN)code/
MEM = $(SUNDOG)memory/
CMEM = $(MEM)code/
SOUND = $(SUNDOG)sound/
CSOUND = $(SOUND)code/
CORE = $(SUNDOG)core/
CCORE = $(CORE)code/
FS = $(SUNDOG)filesystem/
CFS = $(FS)code/
TM = $(SUNDOG)time/
CTM = $(TM)code/
WM = $(SUNDOG)window_manager/
CWM = $(WM)code/
UTILS = $(SUNDOG)utils/
CUTILS = $(UTILS)code/

## ------------------------------------------ ##
## Object files and includes:                 ##
## ------------------------------------------ ##

# SunDog engine:
STD_INCS = $(MEM)memory.h $(CORE)core.h $(CORE)debug.h
DEBUG_OBJS = debug.o
MEMORY_OBJS = memory.o
FS_OBJS = v3nus_fs.o
TIME_OBJS = timemanager.o
UTILS_OBJS = utils.o
SOUND_INCS = \
    $(SOUND)sound.h \
    $(CSOUND)sound_sndout_linux.h \
    $(CSOUND)sound_sndout_palmos.h \
    $(CSOUND)sound_sndout_win.h
ifneq ($(SUNDOG_NOSOUND),true)
    SOUND_OBJS = sound_player.o sound_sndout.o
endif
WM_INCS = \
    $(CWM)wm_win32.h \
    $(CWM)wm_wince.h \
    $(CWM)wm_opengl.h \
    $(CWM)wm_directx.h \
    $(CWM)wm_framebuffer.h \
    $(CWM)wm_palmos.h \
    $(CWM)wm_unixgraphics.h \
    $(CWM)wm_unixgraphics_sdl.h \
    $(WM)wmanager.h \
    $(WM)struct.h
ifneq ($(SUNDOG_NOWINDOWS),true)
    WM_OBJS = wmanager.o image.o wbd.o handlers.o devrgn.o
endif
ifneq ($(SUNDOG_NOMAIN),true)
    MAIN_OBJS = main.o
endif
OBJECTS = \
    $(MEMORY_OBJS) \
    $(DEBUG_OBJS) \
    $(FS_OBJS) \
    $(TIME_OBJS) \
    $(UTILS_OBJS) \
    $(MAIN_OBJS) \
    $(SOUND_OBJS) \
    $(WM_OBJS)

# Global defines:
DEFINES = $(TARGET_DEFINES) $(SUNDOG_DEFINES) -I$(SUNDOG)

# PalmOS defines:
ifeq ($(MAKECMDGOALS),palm)
    ARM_PALM_INCS = -I$(SUNDOG)various/palmos/ -I$(SDK) -I$(SDK)libraries -I$(SDK)Extensions/ExpansionMgr -I$(SDK)Dynamic -I$(SDK)Core -I$(SDK)Core/Hardware -I$(SDK)Core/System -I$(SDK)Core/System/Unix -I$(SDK)Core/ui -I$(SDK)Core/UI -I$(SDK)Libraries
    ARM_PALM_FLAGS = -DNATIVE -mcpu=arm9tdmi -nostartfiles -ffixed-r9 -mpic-register=r10 -msingle-pic-base -fPIC
    DEFINES += $(ARM_PALM_INCS) $(ARM_PALM_FLAGS) $(ARM_PALM_DEFINES)
    ARM_PALM_FILES = $(SUNDOG)various/palmos/palm_functions.cpp $(SUNDOG)various/palmos/PalmTypes.h $(SUNDOG)various/palmos/DebugMgr.h $(SUNDOG)various/palmos/palm_functions.h
    ARM_PALM_OBJS = palm_functions.o
    OBJECTS += $(ARM_PALM_OBJS)
endif

# Win32 defines:
ifeq ($(MAKECMDGOALS),win32)
    DEFINES += -Iresources/
endif

# WinCE defines:
ifeq ($(MAKECMDGOALS),wince)
    DEFINES += -Iresources/ -I$(SUNDOG)various/wince/
endif

## ------------------------------------------ ##
## Main rules for different targets:          ##
## ------------------------------------------ ##

.SECONDEXPANSION:
linux: $$(OBJECTS)
	$(CC) $(LDFLAGS) -o $(APPNAME) $(OBJECTS) $(LIBS)

.SECONDEXPANSION:
palm: $$(OBJECTS)
#Link main code ( with special ARM script - armscript.ld ) :
	$(CC) $(DEFINES) -o main_palm.o $(OBJECTS) -Xlinker --script=$(SUNDOG)various/palmos/armscript.ld
#Create BIN files from the program_palm.o :
	arm-elf-objcopy -j .text -O binary main_palm.o armcode
	split -b 64000 armcode armc
	if [[ -a armcaa ]]; then mv armcaa armc0000.bin; fi
	if [[ -a armcab ]]; then mv armcab armc0001.bin; fi
	if [[ -a armcac ]]; then mv armcac armc0002.bin; fi
	if [[ -a armcad ]]; then mv armcad armc0003.bin; fi
	if [[ -a armcae ]]; then mv armcae armc0004.bin; fi
	if [[ -a armcaf ]]; then mv armcaf armc0005.bin; fi
	arm-elf-objcopy -j .got -O binary main_palm.o armg0000.bin
	arm-elf-objcopy -j .alldata -O binary main_palm.o armdata
	split -b 64000 armdata armd
	if [[ -a armdaa ]]; then mv armdaa armd0000.bin; fi
	if [[ -a armdab ]]; then mv armdab armd0001.bin; fi
	if [[ -a armdac ]]; then mv armdac armd0002.bin; fi
	if [[ -a armdad ]]; then mv armdad armd0003.bin; fi
	if [[ -a armdae ]]; then mv armdae armd0004.bin; fi
	if [[ -a armdaf ]]; then mv armdaf armd0005.bin; fi
	arm-elf-objcopy -j .rel -O binary main_palm.o armr0000.bin
#Create Palm resources ( *.BIN files ) :
	pilrc resources/palmos_res.rcp
#Build PRC file :
	$(MCC) $(SUNDOG)various/palmos/arm_starter.cpp resources/palmos_filetypes.cpp $(SUNDOG_DEFINES) -o arm_starter.o
	m68k-palmos-obj-res arm_starter.o
	build-prc -v 1.0 resources/palmos_info.def -o $(APPNAME).prc *.grc *.bin
palm_functions.o: $(ARM_PALM_FILES) 
	$(CC) $(DEFINES) -c $(SUNDOG)various/palmos/palm_functions.cpp

.SECONDEXPANSION:
win32: $$(OBJECTS)
ifneq ($(SUNDOG_NOWINDOWS),true)
	i686-pc-mingw32-windres resources/win_res.rc win_res.o
	$(CC) $(LDFLAGS) -mwindows -o $(APPNAME).exe $(OBJECTS) win_res.o $(LIBS)
else
	$(CC) $(LDFLAGS) -o $(APPNAME).exe $(OBJECTS) $(LIBS)
endif

.SECONDEXPANSION:
wince: $$(OBJECTS)
	arm-mingw32ce-windres resources/win_res.rc win_res.o
	$(CC) $(LDFLAGS) -o $(APPNAME).exe $(OBJECTS) win_res.o $(LIBS)

## ------------------------------------------ ##
## SunDog engine rules: (main)                ##
## ------------------------------------------ ##

main.o: $(CMAIN)main.cpp $(STD_INCS)
	$(CC) $(DEFINES) -c $(CMAIN)main.cpp

## ------------------------------------------ ##
## SunDog engine rules: (sound manager)       ##
## ------------------------------------------ ##

sound_player.o: $(CSOUND)sound_player.cpp $(SOUND)sound.h $(SOUND_INCS)
	$(CC) $(DEFINES) -c $(CSOUND)sound_player.cpp

sound_sndout.o: $(CSOUND)sound_sndout.cpp $(SOUND)sound.h $(SOUND_INCS)
	$(CC) $(DEFINES) -c $(CSOUND)sound_sndout.cpp

## ------------------------------------------ ##
## SunDog engine rules: (...)                 ##
## ------------------------------------------ ##

timemanager.o: $(CTM)timemanager.cpp $(TM)timemanager.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(CTM)timemanager.cpp

v3nus_fs.o: $(CFS)v3nus_fs.cpp $(FS)v3nus_fs.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(CFS)v3nus_fs.cpp

utils.o: $(CUTILS)utils.cpp $(UTILS)utils.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(CUTILS)utils.cpp

debug.o: $(CCORE)debug.cpp
	$(CC) $(DEFINES) -c $(CCORE)debug.cpp

memory.o: $(CMEM)memory.cpp $(MEM)memory.h $(CORE)core.h
	$(CC) $(DEFINES) -c $(CMEM)memory.cpp

## ------------------------------------------ ##
## SunDog engine rules: (window manager)      ##
## ------------------------------------------ ##

wmanager.o: $(CWM)wmanager.cpp $(WM_INCS) $(STD_INCS)
	$(CC) $(DEFINES) -c $(CWM)wmanager.cpp

image.o: $(CWM)image.cpp $(WM_INCS) $(STD_INCS)
	$(CC) $(DEFINES) -c $(CWM)image.cpp

wbd.o: $(CWM)wbd.cpp $(WM_INCS) $(STD_INCS)
	$(CC) $(DEFINES) -c $(CWM)wbd.cpp

handlers.o: $(CWM)handlers.cpp $(WM_INCS) $(STD_INCS)
	$(CC) $(DEFINES) -c $(CWM)handlers.cpp

devrgn.o: $(WM)regions/devrgn.cpp $(STD_INCS)
	$(CC) $(DEFINES) -c $(WM)regions/devrgn.cpp

## ------------------------------------------ ##
##                                            ##
## ------------------------------------------ ##

clean:
	rm *.o *.grc *.bin armcode armdata
