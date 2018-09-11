#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <gbm.h>

GLuint loadShader(GLenum shaderType, const char * source);
GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr);
static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE);
static void checkGlError(const char* op);

int main(int argc, char ** argv) {
    int dri_fd = open("/dev/dri/card0", O_RDWR);

    struct drmModeRes * resources = drmModeGetResources(dri_fd);
    struct drmModeConnector * connector = NULL;

    for(int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector * connector = drmModeGetConnector(dri_fd, resources->connectors[i]);
        // pick the first connected connector
        if (connector->connection == DRM_MODE_CONNECTED) {
            break;
        }
        drmModeFreeConnector(connector);
    }

    if (!connector) {
        printf("no connector found\n");
        exit(EXIT_FAILURE);
    }

    // save the connector_id
    uint32_t connector_id = connector->connector_id;
    // save the first mode
    struct drmModeModeInfo mode_info = connector->modes[0];

    printf ("resolution: %ix%i\n", mode_info.hdisplay, mode_info.vdisplay);
    // find an encoder
	drmModeEncoder * encoder = drmModeGetEncoder(dri_fd, connector->encoder_id);

    if (!encoder) {
        printf("no encoder found\n");
        exit(EXIT_FAILURE);
    }

    // find a CRTC
    struct drmModeCrtc * crtc = NULL;
    if (encoder->crtc_id)
        crtc = drmModeGetCrtc(dri_fd, encoder->crtc_id);

    if (!encoder) {
        printf("no crtc found\n");
        exit(EXIT_FAILURE);
    }

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    struct gbm_device * gbm_dev = gbm_create_device(dri_fd);

    EGLDisplay egl_display = eglGetDisplay(gbm);

    if (egl_display == EGL_NO_DISPLAY) {
        printf("eglGetDisplay() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    int major, minor;
    int rc = eglInitialize(egl_display, &major, &minor);
    if (rc == EGL_FALSE) {
        printf("eglInitialize() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   0x8,
        EGL_GREEN_SIZE, 0x8,
        EGL_BLUE_SIZE,  0x8,
        EGL_ALPHA_SIZE, 0x8,
        EGL_NONE
    };

    int n;
    EGLConfig egl_config;
    if (eglChooseConfig(egl_display, attribs, &egl_config, 1, &n) == EGL_FALSE) {
        printf("eglChooseConfig() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    if(n == 0) {
        printf("eglChooseConfig() no appropriate configs found\n");
        exit(EXIT_FAILURE);
    }

    EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };

    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);

    EGLint window_attribs[] = { EGL_NONE };

    struct gbm_surface * gbm_surf = gbm_surface_create(gbm_dev, mode_info.hdisplay, mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);

    EGLSurface offscreen_egl_surface = eglCreateWindowSurface(egl_display, egl_config, gbm_surf, window_attribs);

    eglMakeCurrent(egl_display, offscreen_egl_surface, offscreen_egl_surface, egl_context);

    const GLfloat triangle[] = {
        -0.125f, -0.125f, 0.0f, 0.5f,
        0.0f,  0.125f, 0.0f, 0.5f,
        0.125f, -0.125f, 0.0f, 0.5f
    };

    const GLfloat color_triangle[] = {
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f
    };

    GLint num_vertex = 3;

    static const char triangleVertexShaderStr[] =
        "attribute vec4 vPosition;\n"
        "attribute vec4 vColor;\n"
        "uniform float angle;\n"
        "varying vec4 colorinfo;\n"
        "void main() {\n"
        "  mat3 rot_z = mat3( vec3( cos(angle),  sin(angle), 0.0),\n"
        "                     vec3(-sin(angle),  cos(angle), 0.0),\n"
        "                     vec3(       0.0,         0.0, 1.0));\n"
        "  gl_Position = vec4(rot_z * vPosition.xyz, 1.0);\n"
        "  colorinfo = vColor;\n"
        "}\n";

    static const char triangleFragmentShaderStr[] =
        "precision mediump float;\n"
        "varying vec4 colorinfo;\n"
        "void main() {\n"
        "  gl_FragColor = colorinfo;\n"
        "}\n";

    GLuint triangleProgram = createProgram(triangleVertexShaderStr, triangleFragmentShaderStr);

    GLuint positionHandle = glGetAttribLocation(triangleProgram, "vPosition");
    GLuint colorHandle = glGetAttribLocation(triangleProgram, "vColor");
    GLuint rotation_uniform = glGetUniformLocation(triangleProgram, "angle");
    
    glClearColor(0.1, 0.5, 0.0, 0.0);
    glUseProgram(triangleProgram);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    GLfloat rotation_angle = 0.0f;

    struct gbm_bo * previous_bo = NULL;
    uint32_t previous_fb;

    while(true) {
        rotation_angle += 0.3;

        glUniform1fv(rotation_uniform, 1, &rotation_angle);

        glVertexAttribPointer(positionHandle, num_vertex, GL_FLOAT, GL_FALSE, 0, triangle);
        glEnableVertexAttribArray(positionHandle);
        
        glVertexAttribPointer(colorHandle, num_vertex, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, color_triangle);
        glEnableVertexAttribArray(colorHandle);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, num_vertex);

        glDisableVertexAttribArray(positionHandle);
        glDisableVertexAttribArray(colorHandle);

// swap
        eglSwapBuffers(egl_display, offscreen_egl_surface);

        struct gbm_bo * bo = gbm_surface_lock_front_buffer(gbm_surf);
        uint32_t handle = gbm_bo_get_handle(bo).u32;
        uint32_t pitch = gbm_bo_get_stride(bo);
        uint32_t fb;

        drmModeAddFB(device, mode_info.hdisplay, mode_info.vdisplay, 24, 32, pitch, handle, &fb);
        drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info);

        if (previous_bo) {
            drmModeRmFB(device, previous_fb);
            gbm_surface_release_buffer(gbm_surface, previous_bo);
        }
        previous_bo = bo;
        previous_fb = fb;

        printf("press enter to draw next frame..\n");
    }
    return 0;
}

GLuint loadShader(GLenum shaderType, const char * source) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        printf("glCreateShader() shaderType = 0x%x error: 0x%x\n", shaderType, glGetError());
        exit(EXIT_FAILURE);
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        printf("glCompileShader() shaderType = 0x%x error: ", shaderType);
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen) {
            char buf[infoLen];
            glGetShaderInfoLog(shader, infoLen, NULL, buf);
            printf("%s\n", buf);
        } else {
            printf("\n");
        }
        exit(EXIT_FAILURE);
    }
    return shader;
}

GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr) {
    GLuint program = glCreateProgram();
    if (!program) {
        printf("glCreateProgram() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderStr);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderStr);

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    if(linkStatus != GL_TRUE) {
        printf("glLinkProgram() link error: ");
        GLint bufLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
        if (bufLength) {
             char buf[bufLength];
             glGetProgramInfoLog(program, bufLength, NULL, buf);
             printf("%s\n", buf);
        } else {
            printf("\n");
        }
        exit(EXIT_FAILURE);
    }
    return program;
}

const char * egl_strerror(EGLint err) {
    switch (err){
        case EGL_SUCCESS:           return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:   return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:        return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:         return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:     return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONFIG:        return "EGL_BAD_CONFIG";
        case EGL_BAD_CONTEXT:       return "EGL_BAD_CONTEXT";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:       return "EGL_BAD_DISPLAY";
        case EGL_BAD_MATCH:         return "EGL_BAD_MATCH";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_BAD_PARAMETER:     return "EGL_BAD_PARAMETER";
        case EGL_BAD_SURFACE:       return "EGL_BAD_SURFACE";
        case EGL_CONTEXT_LOST:      return "EGL_CONTEXT_LOST";
        default: return "UNKNOWN";
    }
}

static void checkEglError(const char* op, EGLBoolean returnVal) {
    if (returnVal != EGL_TRUE)
        fprintf(stderr, "%s() returned %d\n", op, returnVal);

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError()) {
        fprintf(stderr, "after %s() eglError %s (0x%x)\n", op, egl_strerror(error), error);
        exit(EXIT_FAILURE);
    }
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        fprintf(stderr, "after %s() glError (0x%x)\n", op, error);
        exit(EXIT_FAILURE);
    }
}
