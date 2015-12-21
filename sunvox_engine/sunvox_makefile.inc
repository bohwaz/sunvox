DEFINES += -I$(SUNVOX)

LIB_SUNVOX = \
    sunvox_engine.o \
    psynth_net.o \
    psynths_echo.o \
    psynths_filter.o \
    psynths_generator.o \
    psynths_kicker.o \
    psynths_sampler.o \
    psynths_distortion.o \
    psynths_flanger.o \
    psynths_spectravoice.o \
    psynths_loop.o \
    psynths_delay.o \
    psynths_fm.o \
    psynths_lfo.o \
    psynths_reverb.o \
    psynths_vocal_filter.o

sunvox_engine.o: $(SUNVOX)sunvox_engine.cpp $(SUNVOX)sunvox_engine.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)sunvox_engine.cpp

psynth_net.o: $(SUNVOX)psynth/psynth_net.cpp $(SUNVOX)psynth/psynth_net.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynth_net.cpp

psynths_echo.o: $(SUNVOX)psynth/psynths_echo.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_echo.cpp

psynths_flanger.o: $(SUNVOX)psynth/psynths_flanger.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_flanger.cpp

psynths_filter.o: $(SUNVOX)psynth/psynths_filter.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_filter.cpp

psynths_generator.o: $(SUNVOX)psynth/psynths_generator.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_generator.cpp

psynths_spectravoice.o: $(SUNVOX)psynth/psynths_spectravoice.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_spectravoice.cpp

psynths_kicker.o: $(SUNVOX)psynth/psynths_kicker.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_kicker.cpp

psynths_sampler.o: $(SUNVOX)psynth/psynths_sampler.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_sampler.cpp

psynths_distortion.o: $(SUNVOX)psynth/psynths_distortion.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_distortion.cpp

psynths_loop.o: $(SUNVOX)psynth/psynths_loop.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_loop.cpp

psynths_delay.o: $(SUNVOX)psynth/psynths_delay.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_delay.cpp

psynths_fm.o: $(SUNVOX)psynth/psynths_fm.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_fm.cpp

psynths_lfo.o: $(SUNVOX)psynth/psynths_lfo.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_lfo.cpp

psynths_reverb.o: $(SUNVOX)psynth/psynths_reverb.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_reverb.cpp

psynths_vocal_filter.o: $(SUNVOX)psynth/psynths_vocal_filter.cpp $(SUNVOX)psynth/psynth.h $(STD_INCS)
	$(CC) $(DEFINES) -c $(SUNVOX)psynth/psynths_vocal_filter.cpp
