all: module-systray.so

CFLAGS := -fPIC $(shell pkg-config --cflags gmodule-2.0 evolution-shell-3.0 xapp)
LIBS := $(shell pkg-config --libs evolution-mail-3.0 xapp)

module-systray.so: systray.o
	gcc -shared systray.o -o module-systray.so $(LIBS)

systray.o: systray.c
	gcc $(CFLAGS) -c systray.c -o systray.o

.PHONY: install clean

install: all
	sudo cp module-systray.so /usr/lib64/evolution/modules/

clean:
	-rm -f systray.o module-systray.so
