MESA=/home/user/mesa-static
LIBDRM=/home/user/libdrm

all: gles-gbm-rot-cube-shared gles-gbm-rot-cube-static

gles-gbm-rot-cube-shared: gles_gbm_rot_cube.cpp Makefile
	g++ gles_gbm_rot_cube.cpp -O0 -ggdb3 -I/usr/local/include/libdrm -I${MESA}/include -I${MESA}/src/gbm/main -ldrm -L${MESA}/out/lib -lgbm -lEGL -lGLESv2 -o gles-gbm-rot-cube-shared

gles-gbm-rot-cube-static: gles_gbm_rot_cube.cpp Makefile
	g++ gles_gbm_rot_cube.cpp -DSTATIC_BUILD -O0 -g3 -I${MESA}/include -I${MESA}/src/gbm/main -I${LIBDRM} -I${LIBDRM}/include/drm ${MESA}/out/src/gbm/.libs/libgbm.a ${LIBDRM}/out/.libs/libdrm.a ${MESA}/out/src/egl/.libs/libEGL_common.a ${MESA}/out/src/mapi/es2api/.libs/libGLESv2.a ${MESA}/out/src/mapi/shared-glapi/.libs/libglapi.a ${MESA}/out/src/mesa/drivers/dri/i965/.libs/libi965_dri.a -u __driDriverGetExtensions_i965 ${MESA}/out/src/mesa/.libs/libmesa.a ${MESA}/out/src/mesa/drivers/dri/common/.libs/libdricommon.a ${MESA}/out/src/mesa/drivers/dri/common/.libs/libmegadriver_stub.a -lwayland-client -lwayland-server -lpthread -lexpat -lz -o gles-gbm-rot-cube-static

clean:
	rm gles-gbm-rot-cube-shared gles-gbm-rot-cube-static 2>/dev/null || echo done
