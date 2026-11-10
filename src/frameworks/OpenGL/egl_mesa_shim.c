/* Stand-in for ANGLE's libEGL. Chromium dlopens this and asks
 * eglGetPlatformDisplay for an ANGLE platform. Those platforms have no
 * device here. Log the request, then hand back the Mesa vendor that is
 * already installed (surfaceless, or X11 on DISPLAY). */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int32_t EGLint;
typedef unsigned int EGLenum;
typedef intptr_t EGLAttrib;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLSurface;
typedef unsigned int EGLBoolean;

#define EGL_NONE 0x3038
#define EGL_SUCCESS 0x3000
#define EGL_FALSE 0
#define EGL_TRUE 1
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_PLATFORM_X11_KHR 0x31D5
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API 0x30A0
#define GL_VERSION 0x1F02

static void *mesa;
static void *(*mesa_proc)(const char *);

__attribute__((constructor))
static void shim_loaded(void)
{
	setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "mesa libEGL shim loaded\n");
}
void (*eglGetProcAddress(const char *name))(void);

static void load_mesa(void)
{
	if (mesa)
		return;
	setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "mesa shim dlopen native libEGL\n");
	mkdir("/tmp/.X11-unix", 01777);
	symlink("/Volumes/SystemRoot/tmp/.X11-unix/X1", "/tmp/.X11-unix/X1");
	mesa = dlopen("/usr/lib/native/libEGL.dylib", RTLD_NOW | RTLD_LOCAL);
	if (!mesa)
		fprintf(stderr, "mesa libEGL dlopen failed: %s\n", dlerror());
	else
		mesa_proc = dlsym(mesa, "eglGetProcAddress");
}

static void *M(const char *name)
{
	void *p;
	load_mesa();
	p = mesa ? dlsym(mesa, name) : NULL;
	if (!p && mesa_proc)
		p = mesa_proc(name);
	if (!p)
		fprintf(stderr, "mesa EGL missing %s\n", name);
	return p;
}

static EGLint mesa_error(void)
{
	EGLint (*ge)(void) = M("eglGetError");
	return ge ? ge() : -1;
}

static void log_attribs(const void *attribs, int wide)
{
	if (!attribs) {
		fprintf(stderr, " attribs=null");
		return;
	}
	if (wide) {
		const EGLAttrib *a = attribs;
		int i;
		for (i = 0; a[i] != EGL_NONE && i < 24; i += 2)
			fprintf(stderr, " 0x%lx=0x%lx", (unsigned long)a[i], (unsigned long)a[i + 1]);
	} else {
		const EGLint *a = attribs;
		int i;
		for (i = 0; a[i] != EGL_NONE && i < 24; i += 2)
			fprintf(stderr, " 0x%x=0x%x", a[i], a[i + 1]);
	}
}

static EGLDisplay mesa_display(void)
{
	static EGLDisplay d;
	static int done;
	EGLDisplay (*gpd)(EGLenum, void *, const EGLAttrib *);
	EGLAttrib none = EGL_NONE;
	void *x11, *xd;
	void *(*xopen)(const char *);

	if (done)
		return d;
	done = 1;
	gpd = M("eglGetPlatformDisplay");
	if (!gpd)
		return NULL;
	d = gpd(EGL_PLATFORM_SURFACELESS_MESA, NULL, &none);
	fprintf(stderr, "mesa surfaceless display=%p eglError=0x%x\n", d, mesa_error());
	if (d)
		return d;
	x11 = dlopen("/usr/lib/native/libX11.dylib", RTLD_LAZY);
	xopen = x11 ? dlsym(x11, "XOpenDisplay") : NULL;
	xd = xopen ? xopen(NULL) : NULL;
	d = gpd(EGL_PLATFORM_X11_KHR, xd, &none);
	fprintf(stderr, "mesa X11 display=%p native=%p eglError=0x%x\n", d, xd, mesa_error());
	return d;
}

EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native, const EGLAttrib *attribs)
{
	EGLDisplay d;
	fprintf(stderr, "eglGetPlatformDisplay platform=0x%x native=%p", platform, native);
	log_attribs(attribs, 1);
	fprintf(stderr, "\n");
	d = mesa_display();
	fprintf(stderr, "eglGetPlatformDisplay -> %p eglError=0x%x\n", d, mesa_error());
	return d;
}

EGLDisplay eglGetPlatformDisplayEXT(EGLenum platform, void *native, const EGLint *attribs)
{
	EGLDisplay d;
	fprintf(stderr, "eglGetPlatformDisplayEXT platform=0x%x native=%p", platform, native);
	log_attribs(attribs, 0);
	fprintf(stderr, "\n");
	d = mesa_display();
	fprintf(stderr, "eglGetPlatformDisplayEXT -> %p eglError=0x%x\n", d, mesa_error());
	return d;
}

EGLDisplay eglGetDisplay(void *native)
{
	EGLDisplay d;
	fprintf(stderr, "eglGetDisplay native=%p\n", native);
	d = mesa_display();
	fprintf(stderr, "eglGetDisplay -> %p eglError=0x%x\n", d, mesa_error());
	return d;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
	EGLBoolean (*fn)(EGLDisplay, EGLint *, EGLint *) = M("eglInitialize");
	EGLBoolean ok;
	if (!dpy)
		dpy = mesa_display();
	ok = fn ? fn(dpy, major, minor) : EGL_FALSE;
	fprintf(stderr, "eglInitialize dpy=%p ok=%u %d.%d eglError=0x%x\n",
		dpy, ok, major ? *major : -1, minor ? *minor : -1, mesa_error());
	return ok;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib, EGLConfig *configs, EGLint n, EGLint *got)
{
	EGLBoolean (*fn)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *) = M("eglChooseConfig");
	return fn ? fn(dpy, attrib, configs, n, got) : EGL_FALSE;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share, const EGLint *attrib)
{
	EGLContext (*fn)(EGLDisplay, EGLConfig, EGLContext, const EGLint *) = M("eglCreateContext");
	EGLint es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLContext ctx;
	EGLBoolean (*bind)(EGLenum) = M("eglBindAPI");
	if (bind)
		bind(EGL_OPENGL_ES_API);
	if (!attrib)
		attrib = es2;
	ctx = fn ? fn(dpy, config, share, attrib) : NULL;
	fprintf(stderr, "eglCreateContext dpy=%p ctx=%p eglError=0x%x\n", dpy, ctx, mesa_error());
	return ctx;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
	EGLBoolean (*fn)(EGLDisplay, EGLSurface, EGLSurface, EGLContext) = M("eglMakeCurrent");
	EGLBoolean ok = fn ? fn(dpy, draw, read, ctx) : EGL_FALSE;
	const unsigned char *(*glver)(unsigned int);
	fprintf(stderr, "eglMakeCurrent dpy=%p draw=%p ctx=%p ok=%u eglError=0x%x\n",
		dpy, draw, ctx, ok, mesa_error());
	if (ok && ctx) {
		glver = (void *)eglGetProcAddress("glGetString");
		if (glver)
			fprintf(stderr, "GL_VERSION %s\n", glver(GL_VERSION) ? (const char *)glver(GL_VERSION) : "(null)");
	}
	return ok;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib)
{
	EGLSurface (*fn)(EGLDisplay, EGLConfig, const EGLint *) = M("eglCreatePbufferSurface");
	EGLSurface s = fn ? fn(dpy, config, attrib) : NULL;
	fprintf(stderr, "eglCreatePbufferSurface %p eglError=0x%x\n", s, mesa_error());
	return s;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, void *win, const EGLint *attrib)
{
	EGLSurface (*fn)(EGLDisplay, EGLConfig, void *, const EGLint *) = M("eglCreateWindowSurface");
	return fn ? fn(dpy, config, win, attrib) : NULL;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface s)
{
	EGLBoolean (*fn)(EGLDisplay, EGLSurface) = M("eglDestroySurface");
	return fn ? fn(dpy, s) : EGL_FALSE;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext c)
{
	EGLBoolean (*fn)(EGLDisplay, EGLContext) = M("eglDestroyContext");
	return fn ? fn(dpy, c) : EGL_FALSE;
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
	EGLBoolean (*fn)(EGLDisplay) = M("eglTerminate");
	return fn ? fn(dpy) : EGL_FALSE;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface s)
{
	EGLBoolean (*fn)(EGLDisplay, EGLSurface) = M("eglSwapBuffers");
	return fn ? fn(dpy, s) : EGL_FALSE;
}

EGLBoolean eglBindAPI(EGLenum api)
{
	EGLBoolean (*fn)(EGLenum) = M("eglBindAPI");
	return fn ? fn(api) : EGL_FALSE;
}

EGLint eglGetError(void)
{
	EGLint (*fn)(void) = M("eglGetError");
	return fn ? fn() : EGL_SUCCESS;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attr, EGLint *value)
{
	EGLBoolean (*fn)(EGLDisplay, EGLConfig, EGLint, EGLint *) = M("eglGetConfigAttrib");
	return fn ? fn(dpy, config, attr, value) : EGL_FALSE;
}

const char *eglQueryString(EGLDisplay dpy, EGLint name)
{
	const char *(*fn)(EGLDisplay, EGLint) = M("eglQueryString");
	const char *s = fn ? fn(dpy, name) : NULL;
	fprintf(stderr, "eglQueryString name=0x%x %s\n", name, s ? s : "(null)");
	return s;
}

EGLDisplay eglGetCurrentDisplay(void)
{
	EGLDisplay (*fn)(void) = M("eglGetCurrentDisplay");
	return fn ? fn() : NULL;
}

EGLContext eglGetCurrentContext(void)
{
	EGLContext (*fn)(void) = M("eglGetCurrentContext");
	return fn ? fn() : NULL;
}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
{
	EGLSurface (*fn)(EGLint) = M("eglGetCurrentSurface");
	return fn ? fn(readdraw) : NULL;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface s, EGLint attr, EGLint *value)
{
	EGLBoolean (*fn)(EGLDisplay, EGLSurface, EGLint, EGLint *) = M("eglQuerySurface");
	return fn ? fn(dpy, s, attr, value) : EGL_FALSE;
}

EGLBoolean eglReleaseThread(void)
{
	EGLBoolean (*fn)(void) = M("eglReleaseThread");
	return fn ? fn() : EGL_TRUE;
}

void (*eglGetProcAddress(const char *name))(void)
{
	fprintf(stderr, "eglGetProcAddress %s\n", name ? name : "(null)");
	load_mesa();
	if (name) {
		if (!strcmp(name, "eglGetPlatformDisplay"))
			return (void (*)(void))eglGetPlatformDisplay;
		if (!strcmp(name, "eglGetPlatformDisplayEXT"))
			return (void (*)(void))eglGetPlatformDisplayEXT;
		if (!strcmp(name, "eglGetDisplay"))
			return (void (*)(void))eglGetDisplay;
		if (!strcmp(name, "eglInitialize"))
			return (void (*)(void))eglInitialize;
		if (!strcmp(name, "eglCreateContext"))
			return (void (*)(void))eglCreateContext;
		if (!strcmp(name, "eglMakeCurrent"))
			return (void (*)(void))eglMakeCurrent;
		if (!strcmp(name, "eglChooseConfig"))
			return (void (*)(void))eglChooseConfig;
		if (!strcmp(name, "eglCreatePbufferSurface"))
			return (void (*)(void))eglCreatePbufferSurface;
		if (!strcmp(name, "eglQueryString"))
			return (void (*)(void))eglQueryString;
		if (!strcmp(name, "eglGetError"))
			return (void (*)(void))eglGetError;
		if (!strcmp(name, "eglBindAPI"))
			return (void (*)(void))eglBindAPI;
		if (!strcmp(name, "eglGetProcAddress"))
			return (void (*)(void))eglGetProcAddress;
	}
	if (!mesa_proc)
		return NULL;
	return (void (*)(void))mesa_proc(name);
}
