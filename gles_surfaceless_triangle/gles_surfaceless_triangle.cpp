#define EGL_NO_X11

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <gbm.h>
#include <algorithm>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <sstream>
#include <vector>

GLuint loadShader(GLenum shaderType, const char * source);
GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr);
static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE);
static void checkGlError(const char* op);
static void write_ppm(const char * file, uint32_t width, uint32_t height, uint32_t * argb);


struct Rgb32Pixel {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t padding;
};

struct Rgb32Image {
    uint32_t width;
    uint32_t height;
    Rgb32Pixel * data;
};

bool downscale_image(const Rgb32Image& source_img, Rgb32Image& target_img);
void print_ascii(const Rgb32Image& img);

std::string egl_surface_type_2_str(EGLint surface_type);

#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 1440

int main(int argc, char ** argv) {
    EGLDisplay egl_display = EGL_NO_DISPLAY;

    const char * egl_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("egl extensions: %s\n", egl_extensions);

    if(!strstr(egl_extensions, "EGL_EXT_platform_base")) {
        printf("extension EGL_EXT_platform_base is required. exit()\n");
        exit(EXIT_FAILURE);
    }

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);

    printf("egl_display = %p\n", egl_display);
    
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
    printf("eglInitialize(): major = %d minor = %d\n", major, minor);
    
    EGLint num_configs;
    EGLBoolean result = eglGetConfigs(egl_display, NULL, 0, &num_configs);
    printf("eglGetConfigs() result = %d num_configs = %d\n", result, num_configs);
    
    EGLConfig configs[num_configs];
    result = eglGetConfigs(egl_display, configs, num_configs, &num_configs);
    
    for(int i = 0; i < num_configs; i++) {
        EGLint config_id, surface_type, r, g, b, a, buffer_size, depth, stencil, alpha_mask;
        eglGetConfigAttrib(egl_display, configs[i], EGL_CONFIG_ID, &config_id);
        eglGetConfigAttrib(egl_display, configs[i], EGL_SURFACE_TYPE, &surface_type);
        eglGetConfigAttrib(egl_display, configs[i], EGL_RED_SIZE, &r);
        eglGetConfigAttrib(egl_display, configs[i], EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(egl_display, configs[i], EGL_BLUE_SIZE, &b);
        eglGetConfigAttrib(egl_display, configs[i], EGL_ALPHA_SIZE, &a);
        eglGetConfigAttrib(egl_display, configs[i], EGL_BUFFER_SIZE, &buffer_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_DEPTH_SIZE, &depth);
        eglGetConfigAttrib(egl_display, configs[i], EGL_STENCIL_SIZE, &stencil);
        printf("config[%d]: id: %d surface_type: 0x%x (%s) r: %d g: %d b: %d a: %d (size: %d) depth: %2d stencil: %d\n",
               i, config_id, surface_type, egl_surface_type_2_str(surface_type).c_str(), r, g, b, a, buffer_size, depth, stencil);
    }
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   0x8,
        EGL_GREEN_SIZE, 0x8,
        EGL_BLUE_SIZE,  0x8,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE
    };

    int n;
    EGLConfig egl_config;
    if (eglChooseConfig(egl_display, attribs, &egl_config, 1, &n) == EGL_FALSE) {
        printf("eglChooseConfig() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    EGLint config_id;
    eglGetConfigAttrib(egl_display, egl_config, EGL_CONFIG_ID, &config_id);
    printf("eglChooseConfig() n = %d egl_config id = %d\n", n, config_id);

    if(n == 0) {
        printf("eglChooseConfig() no appropriate configs found\n");
        exit(EXIT_FAILURE);
    }

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);

    EGLint pbuffer_attribs[] = { 
        EGL_HEIGHT, SCREEN_HEIGHT,
        EGL_WIDTH, SCREEN_WIDTH,
        EGL_NONE
    };

    EGLSurface pbuffer_surface = eglCreatePbufferSurface(egl_display, egl_config, pbuffer_attribs);

    if(pbuffer_surface == EGL_NO_SURFACE) {
        printf("eglChooseConfig() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    result = eglMakeCurrent(egl_display, pbuffer_surface, pbuffer_surface, egl_context);
    if(result == false) {
        printf("eglMakeCurrent() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    const GLfloat triangle[] = {
        -0.5f, -0.5f, 0.0f, 0.5f,
        0.0f,  0.125f, 0.0f, 0.5f,
        0.5f, -0.5f, 0.0f, 0.5f
    };

    GLint num_vertex = 3;

    static const char triangleVertexShaderStr[] = R"(
        attribute vec4 vPosition;
        void main() {
          gl_Position.x = vPosition.x;
          gl_Position.y = vPosition.y * 1.0;
          gl_Position.z = vPosition.z;
          gl_Position.w = vPosition.w;
        };
    )";

    static const char triangleFragmentShaderStr[] = R"(
        void main() {
          gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);
        };
    )";

    GLuint triangleProgram = createProgram(triangleVertexShaderStr, triangleFragmentShaderStr);

    GLuint positionHandle = glGetAttribLocation(triangleProgram, "vPosition");

    printf("triangleProgram = %d positionHandle = %d rotation_uniform = %d", triangleProgram, positionHandle);

    glUseProgram(triangleProgram);
    checkGlError("glUseProgram()");

    // glClearColor(0.1, 0.5, 0.0, 0.0);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear( GL_COLOR_BUFFER_BIT); // GL_DEPTH_BUFFER_BIT | 

    glVertexAttribPointer(positionHandle, num_vertex, GL_FLOAT, GL_FALSE, 0, triangle);
    glEnableVertexAttribArray(positionHandle);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, num_vertex);

    checkGlError("glDrawArrays(GL_TRIANGLE_STRIP)");

    // glDisableVertexAttribArray(positionHandle);

// swap
    eglSwapBuffers(egl_display, pbuffer_surface);

    uint32_t result_img[SCREEN_WIDTH * SCREEN_HEIGHT] = { 0 };
    glReadPixels(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result_img);
    if(glGetError() != GL_NO_ERROR) {
        printf("glReadPixels() error: 0x%x\n", glGetError());
        exit(EXIT_FAILURE);
    }
    write_ppm("./scrn.ppm", SCREEN_WIDTH, SCREEN_HEIGHT, (uint32_t *) result_img);
    
    uint32_t downscaled_width = 40;
    uint32_t downscaled_height = SCREEN_HEIGHT * downscaled_width / SCREEN_WIDTH;

    void * target_data = malloc((uint32_t) sizeof(Rgb32Pixel) * downscaled_height * downscaled_width);

    Rgb32Image source_img = {
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
        .data = (Rgb32Pixel *) result_img
    };

    Rgb32Image target_img = {
        .width = downscaled_width,
        .height = downscaled_height,
        .data = (Rgb32Pixel *) target_data
    };

    downscale_image(source_img, target_img);
    print_ascii(target_img);

    free(target_data);

//    getchar();
    sleep(1);

    return 0;
}

std::string egl_surface_type_2_str(EGLint surface_type) {
    std::stringstream result;
    EGLint control = 0;
    if(surface_type & EGL_PBUFFER_BIT) {
        result << "| EGL_PBUFFER_BIT ";
        control |= EGL_PBUFFER_BIT;
    }

    if(surface_type & EGL_PIXMAP_BIT) {
        result << "| EGL_PIXMAP_BIT ";
        control |= EGL_PIXMAP_BIT;
    }
    
    if(surface_type & EGL_WINDOW_BIT) {
        result << "| EGL_WINDOW_BIT ";
        control |= EGL_WINDOW_BIT;
    }
    
    if(surface_type != control) {
        result << " unparsed " << std::hex << (surface_type & !control);
    }
    
    if(result.str().empty())
        return "| NONE ";

    result << "|";
    return result.str();
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

void write_ppm(const char * file, uint32_t width, uint32_t height, uint32_t * argb) {
    printf("write_ppm file: %s width = %d height = %d, argb = %p...\n", file, width, height, argb);
    FILE * fp = fopen(file, "wb");
    if(fp == NULL) {
        printf("error while open file '%s': %d:%s\n", file, errno, strerror(errno));
        return;
    }

    fprintf(fp, "P3\n%d %d\n255\n", width, height);
    for (size_t j = 0; j < height; ++j) {
        for (size_t i = 0; i < width; ++i) {
            uint8_t red = (argb[j * width + i] >> 16) & 0x000000ff;
            uint8_t green = (argb[j * width + i] >> 8) & 0x000000ff;
            uint8_t blue = (argb[j * width + i] >> 0) & 0x000000ff;
           fprintf(fp, "%d %d %d  ", red, green, blue);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    printf("... done\n");
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

bool downscale_image(const Rgb32Image& source_img, Rgb32Image& target_img) {
    printf("👾 source_img: %u x %u, data = %p target_img: %u x %u, data = %p",
                    source_img.width, source_img.height, source_img.data, target_img.width, target_img.height, target_img.data);
    if(source_img.width < target_img.width) {
        printf("source_img dimensions (%u x %u) must be >= than target_img ones [%u, %u]", source_img.width, source_img.height, target_img.width, target_img.height);
        return false;
    }

    uint32_t x_step = source_img.width / target_img.width;
    uint32_t y_step = source_img.height / target_img.height;

    // printf("1 downscale_image target_img %ux%u", target_img.width, target_img.height);

    for (uint32_t y = 0, idx_y = 0; y < source_img.height; y += y_step, idx_y++) {
        // printf("2 downscale_image target_img %ux%u", target_img.width, target_img.height);

        for (uint32_t x = 0, idx_x = 0; x < source_img.width; x += x_step, idx_x++) {
            uint32_t y1 = std::min(y + y_step, source_img.height);
            uint32_t x1 = std::min(x + x_step, source_img.width);

            uint32_t sum_r = 0;
            uint32_t sum_g = 0;
            uint32_t sum_b = 0;
            for (uint32_t i = 0; i < (y1 - y); i++) {
                for (uint32_t j = 0; j < (x1 - x); j++) {
                    uint32_t idx = (y + i) * source_img.width + (x + j);
                    sum_r += source_img.data[idx].r;
                    sum_g += source_img.data[idx].g;
                    sum_b += source_img.data[idx].b;
                }
            }

            uint32_t n_pixels = (y1 - y) * (x1 - x);

            target_img.data[idx_y * target_img.width + idx_x] = {
               .b = (uint8_t) (sum_b / n_pixels),
               .g = (uint8_t) (sum_g / n_pixels),
               .r = (uint8_t) (sum_r / n_pixels),
               .padding = 0
            };
        }
    }
    printf("downscale_image target_img %ux%u", target_img.width, target_img.height);
    return true;
}

void print_ascii(const Rgb32Image& img) {
    printf("img %ux%u data = %p", img.width, img.height, img.data);
    char palette[] = ".,:;i1tfLCG08@";

    for (uint32_t y = 0; y < img.height; y++) {
        for (uint32_t x = 0; x < img.width; x++) {
            const Rgb32Pixel& pixel = img.data[y * img.width + x];
            uint32_t brightness =  pixel.r + pixel.g + pixel.b;
            uint32_t max_pos = ((uint32_t) sizeof(palette) - 1) - 1;
            uint32_t pos = max_pos * brightness / (255 * 3);
            printf("\x1b[38;2;%u;%u;%um%c%c",                               // double char to compensate rectangular nature of terminal symbols
                   pixel.r, pixel.g, pixel.b, palette[pos], palette[pos]);
        }
        printf("\r\n");
    }
    printf("\x1b[0m\r\n");
}
