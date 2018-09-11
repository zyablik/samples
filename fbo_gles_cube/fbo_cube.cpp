#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

  #include "shaders.h"
  #include "matrix.h"
  #include "runtime.h"
  #include "timer.h"

#include <EGL/egl.h>
#include <assert.h>
#include <cutils/properties.h>

#include <system/window.h>

#define SAMPLE_TITLE "fbo_cube"

#define RESOURCES_PATH "assets/"
#define VERTEX_SHADER_FILE "cube.vert"
#define FRAGMENT_SHADER_FILE "cube.frag"


char RESOURCES_DIR[]   = RESOURCES_PATH;
char VERTEX_SHADER_PATH[]   = RESOURCES_PATH  SAMPLE_TITLE  "_" VERTEX_SHADER_FILE;
char FRAGMENT_SHADER_PATH[] = RESOURCES_PATH  SAMPLE_TITLE  "_" FRAGMENT_SHADER_FILE;

GLint iLocPosition, iLocTexCoord, iLocMVP, iSamplerLoc;

GLuint uiProgram, uiFragShader, uiVertShader;

ANativeWindow * sWindow;

EGLDisplay sEGLDisplay;
EGLContext sEGLOpenGLES2Context;
EGLSurface sEGLSurface;
EGLConfig sEGLConfig;

int iXangle = 0, iYangle = 0, iZangle = 0;
float aRotate[16], aModelView[16], aPerspective[16], aMVP[16];
int i;

int g_iWindowW = 800;
int g_iWindowH = 600;

GLfloat vertices2[] = {
  -1,-1,-1,   1,-1,-1,   1,-1, 1,  -1,-1, 1,
  1, 1, 1,  -1, 1, 1,  -1,-1, 1,   1,-1, 1,
};

GLfloat tex_coords2[] = {
  0, 0,        0, 1,      1, 0.5,      0.5, 0,
  1, 1,        0.5, 1,      0.5,0,      1, 0,
};

GLubyte indices2[]  = {
  0, 2, 1,  0, 3, 2, // back
  4, 5, 6,  4, 6, 7  // front
};

GLubyte pixels[4 * 3] =
{  
  255,   0,   0, // Red
  0, 255,   0, // Green
  0,   0, 255, // Blue
  255, 255,   0  // Yellow
};

GLuint framebuffer, fbo_texture, cube_texture;
GLint fboWidth = 512, fboHeight = 512;

void initGraphics(){
	processShader(&uiVertShader, VERTEX_SHADER_PATH, GL_VERTEX_SHADER);
	processShader(&uiFragShader, FRAGMENT_SHADER_PATH, GL_FRAGMENT_SHADER);
	uiProgram = GL_CHECK(glCreateProgram());
	GL_CHECK(glAttachShader(uiProgram, uiVertShader));
	GL_CHECK(glAttachShader(uiProgram, uiFragShader));
	GL_CHECK(glLinkProgram(uiProgram));

  GLint maxRenderbufferSize;
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
  printf("GLint maxRenderbufferSize = %d\n", maxRenderbufferSize);
  
  
  GL_CHECK(glGenFramebuffers(1, &framebuffer));
  GL_CHECK(glGenTextures(1, &fbo_texture));
  
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, fbo_texture));
  GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  
  GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
  GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0));
  
	GLenum iResult = GL_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER));
	if(iResult != GL_FRAMEBUFFER_COMPLETE) {
	  exit(1);
	}

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
	iLocPosition = GL_CHECK(glGetAttribLocation(uiProgram, "av4position"));
	iLocTexCoord = GL_CHECK(glGetAttribLocation(uiProgram, "a_texCoord"));
	LOGI("iLocPosition = %i\n", iLocPosition);
	LOGI("iLocTexCoord   = %i\n", iLocTexCoord);
	iLocMVP = GL_CHECK(glGetUniformLocation(uiProgram, "mvp"));
  iSamplerLoc = GL_CHECK(glGetUniformLocation(uiProgram, "s_texture"));

 	LOGI("iLocMVP      = %i\n", iLocMVP);
  LOGI("iSamplerLoc      = %i\n", iSamplerLoc);

 	GL_CHECK(glUseProgram(uiProgram));
	GL_CHECK(glEnableVertexAttribArray(iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(iLocTexCoord));
  
	GL_CHECK(glEnable(GL_CULL_FACE));
	GL_CHECK(glEnable(GL_DEPTH_TEST));
  
  
  GLuint vboIds[3];
  GL_CHECK(glGenBuffers(3, vboIds));
  
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_STATIC_DRAW));
	GL_CHECK(glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, NULL));

  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vboIds[1]));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords2), tex_coords2, GL_STATIC_DRAW));
	GL_CHECK(glVertexAttribPointer(iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, NULL));
  
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIds[2]));
  GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices2), indices2, GL_STATIC_DRAW));
  

  GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
  GL_CHECK(glGenTextures(1, &cube_texture));
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, cube_texture));
  GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  
  GL_CHECK(glActiveTexture(GL_TEXTURE0));
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, cube_texture));
}

void renderFrame(void)
{
  
	rotate_matrix(iXangle, 1.0, 0.0, 0.0, aModelView);
	rotate_matrix(iYangle, 0.0, 1.0, 0.0, aRotate);
	multiply_matrix(aRotate, aModelView, aModelView);
  rotate_matrix(iZangle, 0.0, 1.0, 0.0, aRotate);
	multiply_matrix(aRotate, aModelView, aModelView);

	/* Pull the camera back from the cube */
	aModelView[14] -= 5;

	perspective_matrix(45.0, (double)g_iWindowW/(double)g_iWindowH, 0.01, 100.0, aPerspective);
	multiply_matrix(aPerspective, aModelView, aMVP);

	iXangle += 3;
	iYangle += 2;
	iZangle += 1;

	if(iXangle >= 360) iXangle -= 360;
	if(iXangle < 0) iXangle += 360;
	if(iYangle >= 360) iYangle -= 360;
	if(iYangle < 0) iYangle += 360;
	if(iZangle >= 360) iZangle -= 360;
	if(iZangle < 0) iZangle += 360;
	
  GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
	GL_CHECK(glViewport(0, 0, fboWidth, fboHeight));
	GL_CHECK(glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, aMVP));
	GL_CHECK(glActiveTexture(GL_TEXTURE0));
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, cube_texture));
  GL_CHECK(glUniform1i(iSamplerLoc, 0.0));

	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

  GL_CHECK(glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, NULL));

	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER,0));
	GL_CHECK(glViewport(0, 0, 1200, 700));
	GL_CHECK(glActiveTexture(GL_TEXTURE0));
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, fbo_texture));
  GL_CHECK(glUniform1i(iSamplerLoc, 0.0));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
  GL_CHECK(glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, NULL));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define GL_CHECK(x) x;
void asciiScreenshot(int width, int height){
  GLint readType, readFormat;
  GL_CHECK(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &readType));
  GL_CHECK(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readFormat));
  printf("readType = 0x%x, readFormat = 0x%x\n", readType, readFormat);

  unsigned int bytesPerPixel = 0;
  switch(readType) {
    case GL_UNSIGNED_BYTE:
      switch(readFormat) {
        case GL_RGBA:            bytesPerPixel = 4; break;
        case GL_RGB:             bytesPerPixel = 3; break;
        case GL_LUMINANCE_ALPHA: bytesPerPixel = 2; break;
        case GL_ALPHA:
        case GL_LUMINANCE:       bytesPerPixel = 1; break;
      }
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4: // GL_RGBA format
    case GL_UNSIGNED_SHORT_5_5_5_1: // GL_RGBA format
    case GL_UNSIGNED_SHORT_5_6_5: // GL_RGB format
      bytesPerPixel = 2;
      break;
  }
  printf("bytesPerPixel = %d\n", bytesPerPixel);
  
  GLubyte *pixels = (GLubyte*) malloc(width * height * 4);
  GL_CHECK(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
  int i, j;
  unsigned k;
  for(i = height; i >= 0; i-=20){
    for(j = 0; j < width; j+= 10){
      int color = 0;
      for(k = 0; k < bytesPerPixel; k++){
        color += pixels[(i * width + j) * bytesPerPixel + k];
      }
      if(color == 0)
        printf(" ");
      else if(color == 765)
        printf("=");
      else if(color > 0 && color < 128)
        printf(".");
      else if(color >= 128 && color < 256)
        printf("*");
      else if(color >= 256 && color < 256 + 128)
        printf("#");
      else if(color >= 256 + 128 && color < 512)
        printf("@");
      else if(color >= 512 && color < 512  + 128)
        printf("k");
      else if(color >= 512 + 128)
        printf("d");
    }
    printf("\n");
  }
  free(pixels);
}

int main(void)
{
  Timer fpsTimer;
  fpsTimer.reset();

	property_set("debug.gralloc.map_fb_memory", "1");
//	property_set("debug.gr.calcfps", "4");

	initializeEGL();
//        eglSwapInterval(sEGLDisplay, 0); // disable vsync

	EGL_CHECK(eglMakeCurrent(sEGLDisplay, sEGLSurface, sEGLSurface, sEGLOpenGLES2Context));
  initGraphics();

  printf("GL_EXTENSIONS = %s\n", glGetString(GL_EXTENSIONS));
  while(true)	{
    float fFPS = fpsTimer.getFPS();
    if(fpsTimer.isTimePassed(1.0f))
      LOGI("FPS:\t%.1f\n", fFPS);

    renderFrame();
    asciiScreenshot(1920, 1080);
    eglSwapBuffers(sEGLDisplay, sEGLSurface);

  }

	return 0;
}
