CC = gcc
CFLAGS = --std=gnu99 -Wall
OBJECTS = statsd.o
LIBRARY = libstatsd
LIB_VERSION = 0.0.2
OS = unix

ifdef DEBUG
CFLAGS += -ggdb
else
CFLAGS += -O3
endif

#Specific instructions for Linux and Unix systems
ifeq ($(OS), unix)
CFLAGS += -fPIC

all : lib cli

lib : $(OBJECTS)
	$(CC) -shared -W1,-soname,$(LIBRARY).so.1 -o $(LIBRARY).so.$(LIB_VERSION) $(OBJECTS)
	-ln -s $(LIBRARY).so.$(LIB_VERSION) $(LIBRARY).so

cli : 
	$(CC) $(CFLAGS) statsd-cli.c -L./ -lstatsd -o statsd-cli

#Specific instructions for Windows systems
else
CFLAGS += -D ADD_EXPORTS

all: lib cli

lib: $(OBJECTS)
	$(CC) -shared -mwindows -o $(LIBRARY).dll $(OBJECTS) -Wl,--out-implib,$(LIBRARY).dll.a -LC:\Windows\System32 -lws2_32

cli :
	$(CC) --std=gnu99 -Wall statsd-cli.c -o statsd-cli.exe -L. -lstatsd -LC:\Windows\System32 -lws2_32

endif

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean : 
	-rm *.o
	-rm *.gch
	-rm $(LIBRARY).so*
	-rm statsd-cli

install : 
	install $(LIBRARY).so.$(LIB_VERSION) /usr/local/lib
	cp -d $(LIBRARY).so /usr/local/lib
	cp statsd.h /usr/local/include
	install statsd-cli /usr/local/bin
	ldconfig

uninstall : 
	rm /usr/local/lib/$(LIBRARY).so*
	rm /usr/local/bin/statsd-cli
	rm /usr/local/include/statsd.h
	ldconfig -n /usr/local/lib

.PHONY: clean install uninstall
