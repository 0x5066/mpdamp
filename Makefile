CFLAGS += -Wall -O0 -g3
TBCFLAGS = -c
LIBS += -lSDL2 -lSDL2_image -lfftw3 -lm -lmpdclient -lX11

OBJS = mpdamppl.o

.PHONY: all
all: mpdamp

mpdamp: mpdvz.c ${OBJS}
	$(CXX) -o $@ $< $(CFLAGS) $(OBJS) $(LIBS)

mpdamppl.o: mpdamppl.cpp
	$(CXX) $(CFLAGS) $(TBCFLAGS) -o $@ $<

mpdamp_debug: mpdvz.c ${OBJS}
	$(CXX) -o $@ mpdvz.c $(OBJS) $(CFLAGS) $(LIBS) -DDEBUG

clean:
	rm -rf mpdamp mpdamp_debug mpdamppl.o
