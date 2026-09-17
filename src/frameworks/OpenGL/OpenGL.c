#include <stdlib.h>
#include <stdio.h>  // for fprintf(stderr, "unimplemented")
#include <string.h>
#include <stdint.h>

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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/gl.h>

#define APPLE
#define __APPLE__

__attribute__((used)) const char cgl_glx_window_visual_v1[] =
        "cgl_glx_window_visual_v1";

static EGLDisplay display;
static Display *g_x_display;

static EGLConfig config;
static int num_config;

static EGLint const attribute_list[] = {
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
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
    GLXContext glx_context;
    Window glx_drawable;
    int use_glx;
};

static XVisualInfo *cglVisualInfoForWindow(Display *dpy, Window win) {
    XWindowAttributes wa;
    if (!dpy || !win)
        return NULL;
    if (!XGetWindowAttributes(dpy, win, &wa))
        return NULL;
    XVisualInfo tmpl;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.visualid = XVisualIDFromVisual(wa.visual);
    int n = 0;
    XVisualInfo *info = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &n);
    if (!info || n < 1)
        return NULL;
    return info;
}

static GLXContext cglCreateGLXContextForWindow(Display *dpy, Window win,
                                              GLXContext share) {
    XVisualInfo *vi = cglVisualInfoForWindow(dpy, win);
    if (!vi) {
        fprintf(stderr, "CGL GLX: no XVisualInfo for window 0x%lx\n",
                (unsigned long) win);
        return NULL;
    }
    GLXContext ctx = glXCreateContext(dpy, vi, share, True);
    int db = -1;
    glXGetConfig(dpy, vi, GLX_DOUBLEBUFFER, &db);
    fprintf(stderr,
            "CGL glXCreateContext win=0x%lx vis=0x%lx depth=%d db=%d ctx=%p\n",
            (unsigned long) win, (unsigned long) vi->visualid, vi->depth, db,
            ctx);
    fflush(stderr);
    XFree(vi);
    return ctx;
}

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

__attribute__((constructor))
static void _CGLInitialize(void)
{
    g_displays = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
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
    g_x_display = (Display *) native_display;

    display = eglGetDisplay(native_display);

    if (display != EGL_NO_DISPLAY) {
        eglInitialize(display, NULL, NULL);
        eglChooseConfig(display, attribute_list, &config, 1, &num_config);
        eglBindAPI(EGL_OPENGL_API);
    }

    /* DCV :1 is not DRI2/DRI3 capable; EGL fails. GLX with the window's
       24-bit DefaultVisual is the drawable path. */
    if (g_x_display) {
        int ev = 0, err = 0;
        fprintf(stderr, "CGLRegisterNativeDisplay dpy=%p glX=%d vis=0x%lx depth=%d\n",
                g_x_display, glXQueryExtension(g_x_display, &ev, &err),
                (unsigned long) XVisualIDFromVisual(
                        DefaultVisual(g_x_display, DefaultScreen(g_x_display))),
                DefaultDepth(g_x_display, DefaultScreen(g_x_display)));
        fflush(stderr);
        return kCGLNoError;
    }

    if (display == EGL_NO_DISPLAY)
        return kCGLBadConnection;

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
    EGLNativeWindowType window;
    if (sid)
        window = (EGLNativeWindowType) _CGSNativeWindowForSurfaceID(cid, wid, sid);
    else
        window = (EGLNativeWindowType) _CGSNativeWindowForID(cid, wid);

    if (!window)
        return kCGLBadWindow;

    if (g_x_display) {
        Window xw = (Window)(uintptr_t) window;
        if (!gl->glx_context) {
            gl->glx_context = cglCreateGLXContextForWindow(
                    g_x_display, xw, NULL);
            if (!gl->glx_context)
                return kCGLBadContext;
        }
        gl->glx_drawable = xw;
        gl->use_glx = 1;
        if (!glXMakeCurrent(g_x_display, xw, gl->glx_context))
            return kCGLBadState;
        return kCGLNoError;
    }

    struct _CGLDisplay* disp = getCGLDisplay(cid);
    if (!disp)
        return kCGLBadConnection;

    gl->egl_surface = eglCreateWindowSurface(disp->display, disp->config, window, NULL);
    if (gl->egl_surface == EGL_NO_SURFACE)
        return kCGLBadState;
    return kCGLNoError;
}

CGLWindowRef CGLGetWindow(void *native_window) {
    /* Opaque handle is the X Window itself. EGL window surfaces require a
       config whose EGL_NATIVE_VISUAL_ID matches the X window; DCV EGL is
       also DRI-less. Callers must overlay this OpenGL with X11.backend. */
    fprintf(stderr, "CGLGetWindow native=%p vis_path=glx\n", native_window);
    fflush(stderr);
    return (CGLWindowRef) native_window;
}

CGL_EXPORT void CGLDestroyWindow(CGLWindowRef window) {
    (void) window;
}

CGL_EXPORT CGLError CGLContextMakeCurrentAndAttachToWindow(CGLContextObj context, CGLWindowRef window) {
    if (!context)
        return kCGLBadContext;
    if (!window)
        return kCGLBadWindow;

    if (g_x_display) {
        Window xw = (Window)(uintptr_t) window;
        if (!context->glx_context) {
            context->glx_context = cglCreateGLXContextForWindow(
                    g_x_display, xw, NULL);
            if (!context->glx_context)
                return kCGLBadContext;
        }
        context->glx_drawable = xw;
        context->use_glx = 1;
        return CGLSetCurrentContext(context);
    }

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

    if (context != NULL && context->use_glx && g_x_display) {
        if (!glXMakeCurrent(g_x_display, context->glx_drawable,
                            context->glx_context)) {
            fprintf(stderr, "CGL glXMakeCurrent FAILED win=0x%lx\n",
                    (unsigned long) context->glx_drawable);
            fflush(stderr);
            return kCGLBadState;
        }
        static int makes = 0;
        if (makes < 8) {
            fprintf(stderr, "CGL glXMakeCurrent win=0x%lx ok n=%d\n",
                    (unsigned long) context->glx_drawable, ++makes);
            fflush(stderr);
        }
        return kCGLNoError;
    }

    if (context == NULL && g_x_display) {
        glXMakeCurrent(g_x_display, None, NULL);
        return kCGLNoError;
    }

    if (context != NULL) {
        EGLSurface surface = context->egl_surface;
        eglMakeCurrent(display, surface, surface, context->egl_context);
    } else if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    return kCGLNoError;
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
    (void) pixelFormat;

    CGLContextObj context = calloc(1, sizeof(struct _CGLContextObj));
    if (!context)
        return kCGLBadAlloc;

    context->retain_count = 1;
    pthread_mutex_init(&(context->lock), NULL);
    context->swap_interval = 1;

    if (display != EGL_NO_DISPLAY && config) {
        EGLContext egl_share = EGL_NO_CONTEXT;
        if (share != NULL)
            egl_share = share->egl_context;
        context->egl_context = eglCreateContext(display, config, egl_share, NULL);
    }

    if (share && share->glx_context)
        context->glx_context = NULL; /* created on attach against the window visual */

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

    if (context->glx_context && g_x_display)
        glXDestroyContext(g_x_display, context->glx_context);
    if (context->egl_context && display != EGL_NO_DISPLAY)
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

CGLError CGLGetDrawableSize(CGLContextObj context, GLint *width, GLint *height) {
    if (!context || !width || !height)
        return kCGLBadAddress;
    if (!context->use_glx || !g_x_display || !context->glx_drawable)
        return kCGLBadDrawable;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(g_x_display, context->glx_drawable, &wa))
        return kCGLBadDrawable;
    *width = (GLint) wa.width;
    *height = (GLint) wa.height;
    return kCGLNoError;
}

CGLError CGLPutBGRAOnDrawable(CGLContextObj context, const void *pixels,
                              int width, int height, int bytesPerRow) {
    if (!context)
        return kCGLBadContext;
    if (!pixels || width < 1 || height < 1)
        return kCGLBadAddress;
    if (!context->use_glx || !g_x_display || !context->glx_drawable)
        return kCGLBadDrawable;

    Window win = context->glx_drawable;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(g_x_display, win, &wa))
        return kCGLBadDrawable;

    int depth = wa.depth;
    Visual *visual = wa.visual;
    size_t dstBpr = (size_t) width * 4;
    char *dst = malloc(dstBpr * (size_t) height);
    if (!dst)
        return kCGLBadAlloc;

    size_t srcBpr = bytesPerRow > 0 ? (size_t) bytesPerRow : dstBpr;
    size_t copy = dstBpr;
    if (srcBpr < copy)
        copy = srcBpr;
    const unsigned char *src = (const unsigned char *) pixels;
    for (int y = 0; y < height; y++)
        memcpy(dst + (size_t) y * dstBpr, src + (size_t) y * srcBpr, copy);

    XImage *img = XCreateImage(g_x_display, visual, (unsigned int) depth,
                               ZPixmap, 0, dst, (unsigned int) width,
                               (unsigned int) height, 32, (int) dstBpr);
    if (!img) {
        free(dst);
        return kCGLBadAlloc;
    }
    img->byte_order = LSBFirst;
    img->bitmap_bit_order = LSBFirst;
    img->bits_per_pixel = 32;
    GC gc = DefaultGC(g_x_display, DefaultScreen(g_x_display));
    XPutImage(g_x_display, win, gc, img, 0, 0, 0, 0, (unsigned int) width,
              (unsigned int) height);
    XFlush(g_x_display);
    static int puts = 0;
    if (puts < 16) {
        fprintf(stderr, "CGL XPutImage win=0x%lx %dx%d depth=%d vis=0x%lx n=%d\n",
                (unsigned long) win, width, height, depth,
                (unsigned long) XVisualIDFromVisual(visual), ++puts);
        fflush(stderr);
    }
    img->data = NULL;
    XDestroyImage(img);
    free(dst);
    return kCGLNoError;
}

CGLError CGLFlushDrawable(CGLContextObj context) {
    if (!context)
        return kCGLBadContext;
    if (context->use_glx && g_x_display) {
        glFlush();
        glXSwapBuffers(g_x_display, context->glx_drawable);
        XFlush(g_x_display);
        static int swaps = 0;
        if (swaps < 32) {
            fprintf(stderr, "CGL glXSwapBuffers win=0x%lx n=%d\n",
                    (unsigned long) context->glx_drawable, ++swaps);
            fflush(stderr);
        }
        return kCGLNoError;
    }
    if (display != EGL_NO_DISPLAY && context->egl_surface)
        eglSwapBuffers(display, context->egl_surface);
    return kCGLNoError;
}

CGLError CGLSetParameter(CGLContextObj context, CGLContextParameter parameter, const GLint *value) {
    if (!value)
        return kCGLBadAddress;

    if (parameter == kCGLCPSwapInterval)
    {
        GLint v = *value;
        context->swap_interval = v;
        if (context->use_glx)
            return kCGLNoError;
        if (display != EGL_NO_DISPLAY) {
            EGLBoolean success = eglSwapInterval(display, v);
            return success ? kCGLNoError : kCGLBadValue;
        }
        return kCGLNoError;
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
    return kCGLNoError;
}

CGLError CGLQueryRendererInfo(unsigned long display_mask, CGLRendererInfoObj *rend, long *nrend) {
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend) {
    return kCGLNoError;
}