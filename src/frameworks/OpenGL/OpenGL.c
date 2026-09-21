#include <stdlib.h>
#include <stdio.h>  // for fprintf(stderr, "unimplemented")
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <pthread.h>
#include <dlfcn.h>

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
__attribute__((used)) const char cgl_offscreen_glx_v1[] =
        "cgl_offscreen_glx_v1";
__attribute__((used)) const char cgl_keep_glx_for_angle_v1[] =
        "cgl_keep_glx_for_angle_v1";
__attribute__((used)) const char cgl_procaddr_string_v1[] =
        "cgl_procaddr_string_v1";

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
    case kCGLPFAOpenGLProfile:
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

/* GPU helper never calls CGLRegisterNativeDisplay (that's AppKit/X11).
   Bind offscreen GLX at load so glGetString(GL_VERSION) is a real C string
   before Chrome's gl_version_info CHECK. */
__attribute__((constructor(200)))
static void cglBootOffscreenContext(void)
{
    static CGLContextObj boot;
    CGLError err;

    if (boot)
        return;
    err = CGLCreateContext(NULL, NULL, &boot);
    fprintf(stderr, "cgl_offscreen_glx_v1 boot CreateContext err=%d ctx=%p\n",
            (int) err, (void *) boot);
    fflush(stderr);
    if (err == kCGLNoError && boot) {
        CGLSetCurrentContext(boot);
        /* Keep the boot GLX context current. Unbinding it (cgl_unbind_for_egl_v1)
           let Mesa EGL eglMakeCurrent, but ANGLE-GL does not load Darling
           libEGL — it dlsyms CGL then libGL. Empty GL_VERSION was the unbound
           native context on that path. */
        fprintf(stderr, "cgl_keep_glx_for_angle_v1 boot current\n");
        fflush(stderr);
    }
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
static int cglBindOffscreenGLX(CGLContextObj context, GLXContext share);

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

    if (context != NULL && !context->glx_context)
        (void) cglBindOffscreenGLX(context, NULL);

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
        /* Leave the last GLX context bound so glGetString(GL_VERSION) stays a
           real C string for ANGLE FunctionsGLCGL / Chrome gl_version_info. */
        fprintf(stderr, "cgl_keep_glx_for_angle_v1 skip unbind\n");
        fflush(stderr);
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
    format->attributes = malloc(sizeof(CGLPixelFormatAttribute) * (count + 1));
    for (int i = 0; i < count; i++) {
        format->attributes[i] = attrs[i];
    }
    format->attributes[count] = 0;

    *result = format;
    *number_of_screens = 1;
    {
        unsigned a0 = count > 0 ? (unsigned) attrs[0] : 0;
        unsigned a1 = count > 1 ? (unsigned) attrs[1] : 0;
        unsigned a2 = count > 2 ? (unsigned) attrs[2] : 0;
        fprintf(stderr,
                "cgl_keep_glx_for_angle_v1 ChoosePixelFormat pid=%d n=%d "
                "a0=%u a1=0x%x a2=%u fmt=%p\n",
                (int) getpid(), count, a0, a1, a2, (void *) format);
        fflush(stderr);
    }

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
    if (!format || !value)
        return kCGLBadAddress;
    (void) sreen_num;
    if (attr == kCGLPFAVirtualScreenCount) {
        *value = 1;
        return kCGLNoError;
    }
    if (attr == kCGLPFADisplayMask) {
        *value = (GLint) 0xffffffff;
        return kCGLNoError;
    }
    if (attr == kCGLPFAAccelerated) {
        *value = 0;
        return kCGLNoError;
    }
    if (attr == kCGLPFAOpenGLProfile) {
        *value = (GLint) kCGLOGLPVersion_3_2_Core;
        return kCGLNoError;
    }
    if (format->attributes) {
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

static int cglEnsureXDisplay(void)
{
    if (g_x_display)
        return 1;
    const char *d = getenv("DISPLAY");
    if (!d || !d[0])
        d = ":1";
    g_x_display = XOpenDisplay(d);
    fprintf(stderr, "cgl_offscreen_glx_v1 XOpenDisplay %s -> %p\n", d,
            (void *) g_x_display);
    fflush(stderr);
    return g_x_display != NULL;
}

static int cglBindOffscreenGLX(CGLContextObj context, GLXContext share)
{
    int scr;
    int attrs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None };
    int attrs2[] = { GLX_RGBA, None };
    XVisualInfo *vi;
    Window root;
    Window win;

    if (!context)
        return 0;
    if (!cglEnsureXDisplay())
        return 0;
    if (context->glx_context && context->glx_drawable)
        return 1;

    scr = DefaultScreen(g_x_display);
    vi = glXChooseVisual(g_x_display, scr, attrs);
    if (!vi)
        vi = glXChooseVisual(g_x_display, scr, attrs2);
    if (!vi) {
        fprintf(stderr, "cgl_offscreen_glx_v1 glXChooseVisual failed\n");
        fflush(stderr);
        return 0;
    }
    root = RootWindow(g_x_display, scr);
    win = XCreateSimpleWindow(g_x_display, root, -64, -64, 8, 8, 0,
                              BlackPixel(g_x_display, scr),
                              BlackPixel(g_x_display, scr));
    context->glx_context = glXCreateContext(g_x_display, vi, share, True);
    context->glx_drawable = win;
    context->use_glx = 1;
    fprintf(stderr, "cgl_offscreen_glx_v1 ctx=%p win=0x%lx vis=0x%lx\n",
            context->glx_context, (unsigned long) win,
            (unsigned long) vi->visualid);
    fflush(stderr);
    XFree(vi);
    if (context->glx_context) {
        if (glXMakeCurrent(g_x_display, win, context->glx_context)) {
            const GLubyte *ver = glGetString(GL_VERSION);
            fprintf(stderr, "cgl_offscreen_glx_v1 MakeCurrent ok VERSION=%s\n",
                    ver ? (const char *) ver : "(null)");
        } else {
            fprintf(stderr, "cgl_offscreen_glx_v1 MakeCurrent FAILED\n");
        }
        fflush(stderr);
    }
    return context->glx_context != NULL;
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

    /* DCV EGL can hand back a non-NULL context that cannot make current.
       Always bind offscreen GLX so glGetString returns a real C string
       instead of an unslid/sentinel pointer (0x501004). */
    if (!context->glx_context) {
        GLXContext share_glx = (share && share->glx_context) ? share->glx_context : NULL;
        (void) cglBindOffscreenGLX(context, share_glx);
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

struct _CGLRendererInfoObj {
    int dummy;
};

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, long rend_num, CGLRendererProperty prop, long *value) {
    if (!value)
        return kCGLBadValue;
    (void) rend;
    (void) rend_num;
    switch (prop) {
    case kCGLRPAccelerated:
        *value = 0;
        break;
    case kCGLRPWindow:
    case kCGLRPCompliant:
    case kCGLRPBackingStore:
        *value = 1;
        break;
    case kCGLRPOffScreen:
    case kCGLRPFullScreen:
    case kCGLRPMultiScreen:
        *value = 0;
        break;
    case kCGLRPDisplayMask:
        *value = 0xffffffff;
        break;
    case kCGLRPRendererID:
        *value = 0x00020400; /* Apple Software Renderer-ish */
        break;
    case kCGLRPVideoMemory:
    case kCGLRPTextureMemory:
        *value = 256 * 1024 * 1024;
        break;
    case kCGLRPRendererCount:
        *value = 1;
        break;
    default:
        *value = 0;
        break;
    }
    return kCGLNoError;
}

CGLError CGLQueryRendererInfo(unsigned long display_mask, CGLRendererInfoObj *rend, long *nrend) {
    static struct _CGLRendererInfoObj dummy;
    (void) display_mask;
    if (rend)
        *rend = &dummy;
    if (nrend)
        *nrend = 1;
    fprintf(stderr, "CGLQueryRendererInfo mask=0x%lx n=1 dummy\n", display_mask);
    fflush(stderr);
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend) {
    (void) rend;
    return kCGLNoError;
}

CGL_EXPORT CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target,
                                           GLenum internal_format, GLsizei width,
                                           GLsizei height, GLenum format,
                                           GLenum type, void *ioSurface,
                                           GLuint plane);

CGL_EXPORT void *CGLGetProcAddress(const char *name) {
    if (name && strcmp(name, "glGetString") == 0) {
        fprintf(stderr, "cgl_procaddr_string_v1 -> wrap glGetString\n");
        fflush(stderr);
        return (void *) glGetString;
    }
    if (name && strcmp(name, "CGLTexImageIOSurface2D") == 0) {
        fprintf(stderr, "cgl_procaddr_iosurface_v1 -> CGLTexImageIOSurface2D\n");
        fflush(stderr);
        return (void *) CGLTexImageIOSurface2D;
    }
    return (void *) glXGetProcAddress((const GLubyte *) (name ? name : ""));
}

CGL_EXPORT const char *CGLErrorString(CGLError error) {
    return error == kCGLNoError ? "kCGLNoError" : "CGL error";
}

CGL_EXPORT CGLError CGLSetVirtualScreen(CGLContextObj ctx, GLint screen) {
    (void) ctx;
    (void) screen;
    return kCGLNoError;
}

CGL_EXPORT CGLError CGLUpdateContext(CGLContextObj ctx) {
    (void) ctx;
    return kCGLNoError;
}

__attribute__((used)) const char cgl_iosurface_teximage_v1[] =
        "cgl_iosurface_teximage_v1";
__attribute__((used)) const char cgl_procaddr_iosurface_v1[] =
        "cgl_procaddr_iosurface_v1";

static void *cgl_iosurface_sym(const char *name)
{
    static void *h;
    if (!h)
        h = dlopen("/System/Library/Frameworks/IOSurface.framework/Versions/A/IOSurface",
                   RTLD_LAZY);
    return h ? dlsym(h, name) : NULL;
}

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif

CGL_EXPORT CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target,
                                           GLenum internal_format, GLsizei width,
                                           GLsizei height, GLenum format,
                                           GLenum type, void *ioSurface,
                                           GLuint plane) {
    typedef void *(*ios_base_fn)(void *);
    typedef void *(*ios_plane_base_fn)(void *, size_t);
    typedef size_t (*ios_sz_fn)(void *);
    typedef size_t (*ios_plane_sz_fn)(void *, size_t);
    static ios_base_fn get_base;
    static ios_plane_base_fn get_plane_base;
    static ios_sz_fn get_bpr;
    static ios_plane_sz_fn get_plane_bpr;
    static ios_sz_fn get_bpe;
    void *pixels;
    size_t bpr = 0, bpe = 4;
    GLint unpack_row;
    GLenum glerr;
    CGLError cerr;
    GLenum tex_target = target;
    GLenum up_format = format;
    GLenum up_type = type;
    GLint up_internal = (GLint) internal_format;
    const char *ver;
    int is_es = 0;

    fprintf(stderr,
            "cgl_iosurface_teximage_v1 enter ctx=%p tgt=0x%x ifmt=0x%x %dx%d "
            "fmt=0x%x type=0x%x plane=%u surf=%p\n",
            (void *) ctx, (unsigned) target, (unsigned) internal_format,
            (int) width, (int) height, (unsigned) format, (unsigned) type,
            (unsigned) plane, ioSurface);
    fflush(stderr);

    if (!ioSurface)
        return kCGLBadAddress;
    if (!target)
        target = GL_TEXTURE_2D;
    tex_target = target;
    if (ctx) {
        cerr = CGLSetCurrentContext(ctx);
        if (cerr != kCGLNoError)
            return cerr;
    }
    ver = (const char *) glGetString(GL_VERSION);
    is_es = ver && strstr(ver, "OpenGL ES");
    if (is_es && (tex_target == GL_TEXTURE_RECTANGLE || tex_target == 0x84F5))
        tex_target = GL_TEXTURE_2D;
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
    if (up_internal == (GLint) GL_RGBA || up_internal == (GLint) GL_BGRA ||
        up_internal == (GLint) 0x80E1)
        up_internal = (GLint) GL_RGBA8;
    if (is_es && up_type == GL_UNSIGNED_INT_8_8_8_8_REV) {
        up_type = GL_UNSIGNED_BYTE;
        if (up_format == GL_BGRA || up_format == 0x80E1)
            up_format = GL_BGRA;
    }
    if (!get_base) {
        get_base = (ios_base_fn) cgl_iosurface_sym("IOSurfaceGetBaseAddress");
        get_plane_base = (ios_plane_base_fn) cgl_iosurface_sym("IOSurfaceGetBaseAddressOfPlane");
        get_bpr = (ios_sz_fn) cgl_iosurface_sym("IOSurfaceGetBytesPerRow");
        get_plane_bpr = (ios_plane_sz_fn) cgl_iosurface_sym("IOSurfaceGetBytesPerRowOfPlane");
        get_bpe = (ios_sz_fn) cgl_iosurface_sym("IOSurfaceGetBytesPerElement");
    }
    pixels = NULL;
    if (get_plane_base)
        pixels = get_plane_base(ioSurface, (size_t) plane);
    if (!pixels && get_base)
        pixels = get_base(ioSurface);
    if (!pixels)
        return kCGLBadAddress;
    if (get_plane_bpr)
        bpr = get_plane_bpr(ioSurface, (size_t) plane);
    if (!bpr && get_bpr)
        bpr = get_bpr(ioSurface);
    if (get_bpe) {
        size_t e = get_bpe(ioSurface);
        if (e)
            bpe = e;
    }
    if (bpr < (size_t) width * bpe)
        bpr = (size_t) width * bpe;
    unpack_row = (GLint) (bpe ? (bpr / bpe) : width);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row);
#endif
    if (is_es && tex_target != target) {
        GLint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        if (bound)
            glBindTexture(GL_TEXTURE_2D, (GLuint) bound);
    }
    glTexImage2D(tex_target, 0, up_internal, width, height, 0,
                 up_format, up_type, pixels);
    glerr = glGetError();
    if (glerr == 0x502) {
        /* Immutable / already-specified texture: upload in place. */
        glTexSubImage2D(tex_target, 0, 0, 0, width, height, up_format,
                        up_type, pixels);
        glerr = glGetError();
        fprintf(stderr, "cgl_iosurface_teximage_v1 subimage %dx%d glerr=0x%x\n",
                (int) width, (int) height, (unsigned) glerr);
        fflush(stderr);
    }
#ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    fprintf(stderr,
            "cgl_iosurface_teximage_v1 ctx=%p tgt=0x%x->0x%x ifmt=0x%x %dx%d "
            "fmt=0x%x->0x%x type=0x%x->0x%x plane=%u ptr=%p bpr=%zu es=%d ver=%s "
            "glerr=0x%x\n",
            (void *) ctx, (unsigned) target, (unsigned) tex_target,
            (unsigned) internal_format, (int) width, (int) height,
            (unsigned) format, (unsigned) up_format, (unsigned) type,
            (unsigned) up_type, (unsigned) plane, pixels, bpr, is_es,
            ver ? ver : "?", (unsigned) glerr);
    fflush(stderr);
    return kCGLNoError;
}