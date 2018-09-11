#ifndef __runtime_h
#define __runtime_h

#include <EGL/egl.h>
#include <stdarg.h>

#if defined(__arm__)
#	define WINDOW_RED_SIZE		5
#	define WINDOW_GREEN_SIZE	6
#	define WINDOW_BLUE_SIZE		5
#	define WINDOW_BUFFER_SIZE	16
#else
#	define WINDOW_RED_SIZE		8
#	define WINDOW_GREEN_SIZE	8
#	define WINDOW_BLUE_SIZE		8
#	define WINDOW_BUFFER_SIZE	32
#endif

#define EGL_CHECK(x) \
	x; \
	{ \
		EGLint eglError = eglGetError(); \
		if(eglError != EGL_SUCCESS) { \
			LOGD("eglGetError() = %i (0x%.8x) at %s:%i\n", (signed int)eglError, (unsigned int)eglError, __FILE__, __LINE__); \
			exit(1); \
		} \
	}

#define GL_CHECK(x) \
	x; \
	{ \
		GLenum glError = glGetError(); \
		if(glError != GL_NO_ERROR) { \
			LOGD("glGetError() = %i (0x%.8x) at %s:%i\n", glError, glError, __FILE__, __LINE__); \
			exit(1); \
		} \
	}

extern EGLint g_aEGLAttributes[];

typedef struct {
  unsigned short x;           /* image position */
  unsigned short y;
  unsigned short width;       /* input display size*/
  unsigned short height;
  unsigned char bStretchToDisplay; /* 0 : input display size , 1 : stretch display size*/
} fbdev_window;

int initializeEGL();
int terminateEGL();

int createWindow(int uiWidth, int uiHeight);
int destroyWindow(void);

void linuxLOG(const char* format, ...);

#define LOGI linuxLOG
#define LOGE linuxLOG
#define LOGD linuxLOG

#endif