MESA=/home/user/mesa-static


all: wayland-client wayland-compositor

wayland-client: client.cpp Makefile
	g++ client.cpp -I${MESA}/include -L${MESA}/out/lib -lEGL -lGLESv2 -lwayland-egl -lwayland-client -lffi -O0 -g3 -o wayland-client

wayland-compositor: compositor.cpp Makefile
	g++ compositor.cpp -I/usr/local/include/libdrm -I${MESA}/include -L${MESA}/out/lib -lgbm -ldrm -lEGL -lGLESv2 -lwayland-server -lffi -pthread -O0 -g3 -o wayland-compositor

clean:
	rm -rf ./wayland-client ./wayland-compositor
