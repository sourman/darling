#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include <pthread.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLInternal.h>
#include <CoreFoundation/CFDictionary.h>
#include <pthread.h>

// Try to get the right (generic) type definitions.
// In particular, we really want EGLNativeDisplayType to be void *,
// not int as it is if __APPLE__ is defined.
#undef APPLE
#undef __APPLE__

#define __unix__
#define EGL_NO_X11

#include <EGL/egl.h>

#define APPLE
#define __APPLE__

static EGLDisplay display;

static EGLConfig config;
static int num_config;

static EGLint const attribute_list[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_NONE
};

struct _CGLDisplay
{
    EGLDisplay display;
    EGLConfig config;
    int num_config;
};

static CFMutableDictionaryRef g_displays;
static pthread_mutex_t g_displaysMutex = PTHREAD_MUTEX_INITIALIZER;

struct _CGLContextObj {
    GLuint retain_count;
    pthread_mutex_t lock;
    EGLContext egl_context;
    EGLSurface egl_surface;
    // EGL has no function for getting the current swap interval,
    // so we need to save the last set value. The default is 1.
    int swap_interval;
};

struct _CGLPixelFormatObj {
    GLuint retain_count;
    CGLPixelFormatAttribute *attributes;
};

static inline int attribute_has_argument(CGLPixelFormatAttribute attr) {
    switch (attr) {
    case kCGLPFAAuxBuffers:
    case kCGLPFAColorSize:
    case kCGLPFAAlphaSize:
    case kCGLPFADepthSize:
    case kCGLPFAStencilSize:
    case kCGLPFAAccumSize:
    case kCGLPFARendererID:
    case kCGLPFADisplayMask:
        return 1;
    default:
        return 0;
   }
}

static int ensure_software_display(void);

__attribute__((constructor))
static void _CGLInitialize(void)
{
    g_displays = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    setvbuf(stderr, NULL, _IONBF, 0);
    /* Mesa opens DISPLAY via /tmp/.X11-unix, which the prefix does not have.
     * The host socket is visible at /Volumes/SystemRoot. */
    mkdir("/tmp/.X11-unix", 01777);
    symlink("/Volumes/SystemRoot/tmp/.X11-unix/X1", "/tmp/.X11-unix/X1");
    fprintf(stderr, "CGL framework loaded pid=%d\n", getpid());
    /* Chromium reads GL_VERSION before it asks CGL for a context. A current
     * Mesa ES context makes that string non-empty. */
    if (ensure_software_display()) {
        EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        EGLint pbattribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        typedef const unsigned char *(*glgetstring_fn)(unsigned int);
        EGLContext ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxattr);
        EGLSurface surf = ctx ? eglCreatePbufferSurface(display, config, pbattribs) : EGL_NO_SURFACE;
        void *gl;
        glgetstring_fn glGetString;
        if (ctx && surf && eglMakeCurrent(display, surf, surf, ctx)) {
            glGetString = (glgetstring_fn)eglGetProcAddress("glGetString");
            fprintf(stderr, "CGL startup GL_VERSION %s eglError=0x%x\n",
                (glGetString && glGetString(0x1F02)) ? (const char *)glGetString(0x1F02) : "(null)",
                eglGetError());
        } else {
            fprintf(stderr, "CGL startup context failed ctx=%p surf=%p eglError=0x%x\n",
                (void *)ctx, (void *)surf, eglGetError());
        }
    }
}

static int ensure_software_display(void)
{
    void *native = NULL;
    void *x11;

    if (display != EGL_NO_DISPLAY && num_config > 0)
        return 1;

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        x11 = dlopen("/usr/lib/native/libX11.dylib", RTLD_LAZY);
        if (x11) {
            void *(*xopen)(const char *) = dlsym(x11, "XOpenDisplay");
            if (xopen)
                native = xopen(NULL);
        }
        fprintf(stderr, "CGL: default display failed 0x%x, XOpenDisplay %p\n", eglGetError(), native);
        if (native)
            display = eglGetDisplay(native);
    }
    if (display == EGL_NO_DISPLAY) {
        fprintf(stderr, "CGL: eglGetDisplay failed 0x%x\n", eglGetError());
        return 0;
    }
    if (!eglInitialize(display, NULL, NULL)) {
        fprintf(stderr, "CGL: eglInitialize failed 0x%x\n", eglGetError());
        display = EGL_NO_DISPLAY;
        return 0;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    if (!eglChooseConfig(display, attribute_list, &config, 1, &num_config) || num_config < 1) {
        fprintf(stderr, "CGL: eglChooseConfig failed 0x%x n=%d\n", eglGetError(), num_config);
        return 0;
    }
    fprintf(stderr, "CGL: software EGL display ready\n");
    return 1;
}

static int attributes_count(const CGLPixelFormatAttribute *attrs) {
    int result;
    for (result = 0; attrs[result] != 0; result++) {
        if (attribute_has_argument(attrs[result])) {
            result++;
        }
    }
    return result;
}

CGLError CGLRegisterNativeDisplay(void *native_display) {

    display = eglGetDisplay(native_display);

    if (display == EGL_NO_DISPLAY) {
        return kCGLBadConnection;
    }

    eglInitialize(display, NULL, NULL);
    eglChooseConfig(display, attribute_list, &config, 1, &num_config);

    eglBindAPI(EGL_OPENGL_API);

    return kCGLNoError;
}

static struct _CGLDisplay* getCGLDisplay(CGSConnectionID cid)
{
    struct _CGLDisplay* rv;

    pthread_mutex_lock(&g_displaysMutex);
    rv = (struct _CGLDisplay*) CFDictionaryGetValue(g_displays, (const void*)(unsigned long) cid);
    pthread_mutex_unlock(&g_displaysMutex);

    if (!rv)
    {
        EGLDisplay disp = eglGetDisplay(_CGSNativeDisplay(cid));
        if (disp == EGL_NO_DISPLAY)
            return NULL;

        rv = (struct _CGLDisplay*) malloc(sizeof(*rv));
        rv->display = disp;

        eglInitialize(rv->display, NULL, NULL);
        eglChooseConfig(rv->display, attribute_list, &rv->config, 1, &rv->num_config);

        eglBindAPI(EGL_OPENGL_API);

        pthread_mutex_lock(&g_displaysMutex);
        CFDictionaryAddValue(g_displays, (const void*)(unsigned long) cid, rv);
        pthread_mutex_unlock(&g_displaysMutex);
    }

    return rv;
}

CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid)
{
    struct _CGLDisplay* disp = getCGLDisplay(cid);
    if (!disp)
        return kCGLBadConnection;

    EGLNativeWindowType window;
    if (sid)
        window = (EGLNativeWindowType) _CGSNativeWindowForSurfaceID(cid, wid, sid);
    else
        window = (EGLNativeWindowType) _CGSNativeWindowForID(cid, wid);

    if (!window)
        return kCGLBadWindow;

    gl->egl_surface = eglCreateWindowSurface(disp->display, disp->config, window, NULL);
    if (gl->egl_surface == EGL_NO_SURFACE)
        return kCGLBadState;
    return kCGLNoError;
}

CGLWindowRef CGLGetWindow(void *native_window) {

    EGLNativeWindowType window = (EGLNativeWindowType) native_window;
    EGLSurface surface = eglCreateWindowSurface(display, config, window, NULL);

    if (surface == EGL_NO_SURFACE) {
        return NULL;
    }

    return (CGLWindowRef) surface;
}

CGL_EXPORT void CGLDestroyWindow(CGLWindowRef window) {
    eglDestroySurface(display, (EGLSurface) window);
}

CGL_EXPORT CGLError CGLContextMakeCurrentAndAttachToWindow(CGLContextObj context, CGLWindowRef window) {
    if (!context)
        return kCGLBadContext;
    context->egl_surface = (EGLSurface) window;
    return CGLSetCurrentContext(context);
}

static pthread_key_t current_context_key;

static void make_key() {
    pthread_key_create(&current_context_key, NULL);
}

static pthread_key_t get_current_context_key() {
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    pthread_once(&key_once, make_key);
    return current_context_key;
}

CGLContextObj CGLGetCurrentContext(void) {
    pthread_key_t key = get_current_context_key();
    return pthread_getspecific(key);
}

CGLError CGLSetCurrentContext(CGLContextObj context) {
    pthread_key_t key = get_current_context_key();
    pthread_setspecific(key, context);

    if (context != NULL && context->egl_context != EGL_NO_CONTEXT) {
        if (!context->egl_surface && display != EGL_NO_DISPLAY) {
            EGLint pbattribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
            context->egl_surface = eglCreatePbufferSurface(display, config, pbattribs);
            if (context->egl_surface == EGL_NO_SURFACE)
                fprintf(stderr, "CGL: pbuffer failed 0x%x\n", eglGetError());
        }
        if (context->egl_surface &&
            !eglMakeCurrent(display, context->egl_surface, context->egl_surface, context->egl_context))
            fprintf(stderr, "CGL: eglMakeCurrent failed 0x%x\n", eglGetError());
    } else if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    return kCGLNoError; // FIXME
}

CGLError CGLSetFullScreen(CGLContextObj ctx) {
    printf("STUB: CGLSetFullScreen\n");

    return kCGLNoError;
}

CGLError CGLChoosePixelFormat(
    const CGLPixelFormatAttribute *attrs,
    CGLPixelFormatObj *result,
    GLint *number_of_screens
) {
    CGLPixelFormatObj format = malloc(sizeof(struct _CGLPixelFormatObj));
    int count = attributes_count(attrs);

    format->retain_count = 1;
    format->attributes = malloc(sizeof(CGLPixelFormatAttribute) * count);
    for (int i = 0; i < count; i++) {
        format->attributes[i] = attrs[i];
    }

    *result = format;
    *number_of_screens = 1;

    return kCGLNoError;
}

CGLError CGLClearDrawable(CGLContextObj ctx)
{
    printf("STUB: CGLClearDrawable\n");

    return kCGLNoError;
}

CGLError CGLDescribePixelFormat(
    CGLPixelFormatObj format,
    GLint sreen_num,
    CGLPixelFormatAttribute attr,
    GLint *value
) {
    for (int i = 0; format->attributes[i] != 0; i++) {
        int has_arg = attribute_has_argument(format->attributes[i]);

        if (format->attributes[i] == attr) {
            if (has_arg) {
                *value = format->attributes[i + 1];
            } else {
                *value = 1;
            }
            return kCGLNoError;
        }

        if (has_arg) {
            i++;
        }
    }

    *value = 0;
    return kCGLNoError;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj format) {
    if (format == NULL) {
        return NULL;
    }

    format->retain_count++;
    return format;
}

void CGLReleasePixelFormat(CGLPixelFormatObj format) {
    if (format == NULL) {
        return;
    }

    format->retain_count--;

    if (format->retain_count == 0) {
        free(format->attributes);
        free(format);
    }
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pixelFormat) {
    CGLReleasePixelFormat(pixelFormat);
    return kCGLNoError;
}

GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pixelFormat) {
    return pixelFormat->retain_count;
}

CGLError CGLCreateContext(CGLPixelFormatObj pixelFormat, CGLContextObj share, CGLContextObj *resultp) {

    EGLContext egl_share = EGL_NO_CONTEXT;
    if (share != NULL) {
        egl_share = share->egl_context;
    }
    EGLContext egl_context = EGL_NO_CONTEXT;
    if (ensure_software_display()) {
        EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        egl_context = eglCreateContext(display, config, egl_share, ctxattr);
    }

    if (egl_context == EGL_NO_CONTEXT)
        fprintf(stderr, "CGLCreateContext: eglCreateContext failed 0x%x\n", eglGetError());
    else
        fprintf(stderr, "CGLCreateContext: software context %p\n", (void *) egl_context);

    CGLContextObj context = malloc(sizeof(struct _CGLContextObj));

    context->retain_count = 1;
    pthread_mutex_init(&(context->lock), NULL);
    context->egl_context = egl_context;
    context->egl_surface = NULL;
    context->swap_interval = 1;
    if (egl_context != EGL_NO_CONTEXT && display != EGL_NO_DISPLAY) {
        EGLint pbattribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        typedef const unsigned char *(*glgetstring_fn)(unsigned int);
        void *gl;
        glgetstring_fn glGetString;
        context->egl_surface = eglCreatePbufferSurface(display, config, pbattribs);
        if (context->egl_surface == EGL_NO_SURFACE)
            fprintf(stderr, "CGLCreateContext: pbuffer failed 0x%x\n", eglGetError());
        else if (!eglMakeCurrent(display, context->egl_surface, context->egl_surface, egl_context))
            fprintf(stderr, "CGLCreateContext: eglMakeCurrent failed 0x%x\n", eglGetError());
        else {
            gl = dlopen("/usr/lib/native/libGL.dylib", RTLD_LAZY);
            glGetString = gl ? dlsym(gl, "glGetString") : NULL;
            fprintf(stderr, "CGLCreateContext: GL_VERSION %s\n",
                (glGetString && glGetString(0x1F02)) ? (const char *)glGetString(0x1F02) : "(null)");
        }
    }

    *resultp = context;

    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj context) {
    if (context == NULL) {
        return NULL;
    }

    context->retain_count++;
    return context;
}

void CGLReleaseContext(CGLContextObj context) {
    if (context == NULL) {
        return;
    }

    context->retain_count--;

    if (context->retain_count != 0) {
        return;
    }

    if (CGLGetCurrentContext() == context) {
        CGLSetCurrentContext(NULL);
    }

    pthread_mutex_destroy(&(context->lock));

    eglDestroyContext(display, context->egl_context);

    free(context);
}

GLuint CGLGetContextRetainCount(CGLContextObj context) {
    if (context == NULL) {
        return 0;
    }

    return context->retain_count;
}

CGLError CGLDestroyContext(CGLContextObj context) {
    CGLReleaseContext(context);

    return kCGLNoError;
}

CGLError CGLLockContext(CGLContextObj context) {
    pthread_mutex_lock(&(context->lock));
    return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj context) {
    pthread_mutex_unlock(&(context->lock));
    return kCGLNoError;
}

CGLError CGLFlushDrawable(CGLContextObj context) {
    if (context == NULL || context->egl_surface == NULL)
        return kCGLBadContext;
    eglSwapBuffers(display, context->egl_surface);
    return kCGLNoError;
}

CGLError CGLSetParameter(CGLContextObj context, CGLContextParameter parameter, const GLint *value) {
    if (!value)
        return kCGLBadAddress;

    if (parameter == kCGLCPSwapInterval)
    {
        GLint v = *value;
        EGLBoolean success = eglSwapInterval(display, v);
        if (success)
            context->swap_interval = v;
        return success ? kCGLNoError : kCGLBadValue;
    }
    fprintf(stderr, "CGLSetParameter unimplemented for parameter %d\n", parameter);
    return kCGLNoError;
}

CGLError CGLGetParameter(CGLContextObj context, CGLContextParameter parameter, GLint *value) {
    if (!value)
        return kCGLBadAddress;

    if (parameter == kCGLCPSwapInterval)
    {
        *value = context->swap_interval;
        return kCGLNoError;
    }
    fprintf(stderr, "CGLGetParameter unimplemented for parameter %d\n", parameter);
    return kCGLNoError;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, long rend_num, CGLRendererProperty prop, long *value) {
    if (!value)
        return kCGLBadAddress;
    switch (prop) {
    case kCGLRPAccelerated:
    case kCGLRPWindow:
    case kCGLRPCompliant:
    case kCGLRPBackingStore:
    case kCGLRPMPSafe:
        *value = 1;
        break;
    case kCGLRPRendererID:
        *value = 0x00020400;
        break;
    case kCGLRPDisplayMask:
        *value = (long) 0xffffffffu;
        break;
    case kCGLRPVideoMemory:
    case kCGLRPTextureMemory:
        *value = 256;
        break;
    default:
        *value = 0;
        break;
    }
    return kCGLNoError;
}

CGLError CGLQueryRendererInfo(unsigned long display_mask, CGLRendererInfoObj *rend, long *nrend) {
    if (!rend || !nrend)
        return kCGLBadAddress;
    *rend = (CGLRendererInfoObj) calloc(1, sizeof(void *));
    *nrend = 1;
    fprintf(stderr, "CGLQueryRendererInfo software\n");
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend) {
    free(rend);
    return kCGLNoError;
}