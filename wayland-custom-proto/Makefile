
all: wayland-client wayland-server

gen/wcp-client-protocol.h: wcp.xml
	wayland-scanner client-header < $< > $@

gen/wcp-server-protocol.h: wcp.xml
	wayland-scanner server-header < $< > $@

gen/wcp-protocol.c: wcp.xml gen/wcp-client-protocol.h
	wayland-scanner code < $< > $@

gen/wcp-protocol.o: gen/wcp-protocol.c
	gcc gen/wcp-protocol.c -c -o gen/wcp-protocol.o

wayland-client: client.cpp Makefile gen/wcp-client-protocol.h gen/wcp-protocol.o
	g++ client.cpp -Igen -I/usr/local/include gen/wcp-protocol.o /usr/local/lib/libwayland-client.a -lffi -O0 -g3 -o wayland-client

wayland-server: server.cpp Makefile gen/wcp-server-protocol.h gen/wcp-protocol.o
	g++ server.cpp -Igen -I/usr/local/include gen/wcp-protocol.o /usr/local/lib/libwayland-server.a -lffi -pthread -O0 -g3 -o wayland-server

clean:
	rm -rf ./wayland-client ./wayland-server ./gen/*.c ./gen/*.h
	find . -type f -name '*.o' -delete
