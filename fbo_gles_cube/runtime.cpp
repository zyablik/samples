/* vim: set sts=4 ts=4 noexpandtab: */
/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <stdio.h>
#include <stdlib.h>

#include "runtime.h"

#include <ui/FramebufferNativeWindow.h>
#include <gui/Surface.h>
#include <android/native_window.h>

extern ANativeWindow * sWindow;

extern EGLDisplay sEGLDisplay;
extern EGLContext sEGLOpenGLES2Context;
extern EGLSurface sEGLSurface;
extern EGLConfig sEGLConfig;

EGLint g_aEGLAttributes[] =
{
	EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
//	EGL_RECORDABLE_ANDROID, 1,
//	EGL_FRAMEBUFFER_TARGET_ANDROID, 1,
	
	EGL_RED_SIZE,			0x8,
	EGL_GREEN_SIZE,			0x8,
	EGL_BLUE_SIZE,			0x8,
	EGL_ALPHA_SIZE,         	0x8,
	EGL_NONE
};

static const EGLint g_aEGLOpenGLES2ContextAttributes[] =
{
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

static const EGLint k_aWindowAttributes[] =
{
	EGL_NONE
	/*
	EGL_RENDER_BUFFER,	EGL_BACK_BUFFER,
	EGL_COLORSPACE,		EGL_COLORSPACE_sRGB,
	EGL_ALPHA_FORMAT,	EGL_ALPHA_FORMAT_PRE,
	EGL_NONE
	*/
};

struct DummyConsumer : public android::BnConsumerListener {
        virtual void onFrameAvailable() {
	    printf("%s\n", __PRETTY_FUNCTION__);
	}
        virtual void onBuffersReleased() {
            printf("%s\n", __PRETTY_FUNCTION__);
	}
	virtual ~DummyConsumer() {};
    };

int initializeEGL()
{
	EGLConfig *pEGLConfig = NULL;
	EGLint cEGLConfigs = 0;
	int iEGLConfig = 0;
	EGLBoolean bResult = EGL_FALSE;

	sEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if(sEGLDisplay == EGL_NO_DISPLAY)
	{
		EGLint iError = eglGetError();
		LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		LOGD("Error: No EGL Display available at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	/* Initialize EGL. */
    bResult = eglInitialize(sEGLDisplay, NULL, NULL);
	if(bResult != EGL_TRUE)
	{
		EGLint iError = eglGetError();
		LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		LOGD("Error: Failed to initialize EGL at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	/* Enumerate available EGL configurations which match or exceed our required attribute list. */
	bResult = eglChooseConfig(sEGLDisplay, g_aEGLAttributes, NULL, 0, &cEGLConfigs);
	if(bResult != EGL_TRUE)
	{
		EGLint iError = eglGetError();
		LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		LOGD("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	printf("cEGLConfigs is %d\n", cEGLConfigs);

	/* Allocate space for all EGL configs available and get them. */
	pEGLConfig = (EGLConfig *)calloc(cEGLConfigs, sizeof(EGLConfig));
	if(pEGLConfig == NULL)
	{
		LOGD("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}
	bResult = eglChooseConfig(sEGLDisplay, g_aEGLAttributes, pEGLConfig, cEGLConfigs, &cEGLConfigs);
	if(bResult != EGL_TRUE)
	{
		EGLint iError = eglGetError();
		LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		LOGD("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	/* Loop through the EGL configs to find a color depth match.
	 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
	 * even though this may force the driver to use a slow color conversion blitting routine. */
	for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
	{
		EGLint iEGLValue = 0;

		bResult = eglGetConfigAttrib(sEGLDisplay, pEGLConfig[iEGLConfig], EGL_RED_SIZE, &iEGLValue);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			LOGD("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);
			exit(1);
		}

		if(iEGLValue == WINDOW_RED_SIZE)
		{
			bResult = eglGetConfigAttrib(sEGLDisplay, pEGLConfig[iEGLConfig], EGL_GREEN_SIZE, &iEGLValue);
			if(bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				LOGD("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);
				exit(1);
			}

			if(iEGLValue == WINDOW_GREEN_SIZE)
			{
				bResult = eglGetConfigAttrib(sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BLUE_SIZE, &iEGLValue);
				if(bResult != EGL_TRUE)
				{
					EGLint iError = eglGetError();
					LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
					LOGD("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);
					exit(1);
				}

				if(iEGLValue == WINDOW_BLUE_SIZE) break;
			}
		}
	}

	if(iEGLConfig >= cEGLConfigs)
	{
		LOGD("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
		LOGD("Use first eglConfig");
		iEGLConfig = 0;
	}
	/* Copy the EGLConfig to our static variables. */
	sEGLConfig = pEGLConfig[iEGLConfig];

#if defined(__linux__) && !defined(__arm__)
	createX11Window();
#endif
        printf("iEGLConfig is %d\n", iEGLConfig);


//        sWindow = new android::FramebufferNativeWindow();

	android::sp<android::BufferQueue> bq = new android::BufferQueue();
	bq->consumerConnect(new DummyConsumer, false);
	sWindow = new android::Surface(static_cast<android::sp<android::IGraphicBufferProducer> >(bq));

	sEGLSurface = eglCreateWindowSurface(sEGLDisplay, pEGLConfig[iEGLConfig], (EGLNativeWindowType)(sWindow), k_aWindowAttributes);
	if(sEGLSurface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		LOGD("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	/* Unconditionally bind to OpenGL-ES API as we exit this function, since it's the default. */
	eglBindAPI(EGL_OPENGL_ES_API);
		/* Select API and create OpenGL-ES 2.0 context, sharing with OpenVG context. */
	sEGLOpenGLES2Context = eglCreateContext(sEGLDisplay, pEGLConfig[iEGLConfig], EGL_NO_CONTEXT, g_aEGLOpenGLES2ContextAttributes);
	if(sEGLOpenGLES2Context == EGL_NO_CONTEXT)
	{
		EGLint iError = eglGetError();
		LOGE("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		LOGD("Error: Failed to create EGL context at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	free(pEGLConfig);
	pEGLConfig = NULL;
  
	return 0;
}

void linuxLOG(const char* format, ...)
{
	va_list ap;
    va_start (ap, format);
    vfprintf (stderr, format, ap);
    fprintf (stderr, "\n");
    va_end (ap);
}
