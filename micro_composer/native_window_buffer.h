#pragma once

#include <hardware/gralloc.h>
#include <system/window.h>

// <cutils/native_handle.h>:
// typedef struct native_handle {
//     int version;        /* sizeof(native_handle_t) */
//     int numFds;         /* number of file-descriptors at &data[0] */
//     int numInts;        /* number of ints at &data[numFds] */
//     int data[0];        /* numFds + numInts ints */
// } native_handle_t;

// <system/window.h>:
//typedef const native_handle_t* buffer_handle_t;

// <system/window.h>:
// typedef struct android_native_base_t {
//     /* a magic value defined by the actual EGL native type */
//     int magic;
// 
//     /* the sizeof() of the actual EGL native type */
//     int version;
// 
//     void* reserved[4];
// 
//     /* reference-counting interface */
//     void (*incRef)(struct android_native_base_t* base);
//     void (*decRef)(struct android_native_base_t* base);
// } android_native_base_t;

// <system/window.h>:
// typedef struct ANativeWindowBuffer {
//     struct android_native_base_t common;
// 
//     int width;
//     int height;
//     int stride;
//     int format;
//     int usage;
// 
//     void* reserved[2];
// 
//     buffer_handle_t handle;
// 
//     void* reserved_proc[8];
// } ANativeWindowBuffer_t;

class NativeWindowBuffer: public ANativeWindowBuffer {
public:
    NativeWindowBuffer();
    NativeWindowBuffer(int width, int height, int format, int usage);

    static int closeGrDev();

    int fenceFd;

private:
    virtual ~NativeWindowBuffer();

    static void _incRef(struct android_native_base_t * base);
    static void _decRef(struct android_native_base_t * base);

    static alloc_device_t * gr_dev;
    static hw_module_t * gr_module;

    volatile int mRefCount;
};
