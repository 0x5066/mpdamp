CFLAGS += -Wall -O0 -g3
TBCFLAGS = -c -Wall -O2
LIBS += -lSDL2 -lSDL2_image -lfftw3 -lm -lmpdclient

OBJS =

.PHONY: all
all: mpdamp

mpdamp: mpdvz.c ${OBJS}
	$(CXX) -o $@ $< $(CFLAGS) $(OBJS) $(LIBS)

mpdamp_debug: mpdvz.c ${OBJS}
	$(CXX) -o $@ mpdvz.c $(CFLAGS) $(OBJS) $(LIBS) -DDEBUG

clean:
	rm -rf mpdamp mpdamp_debug *.o

