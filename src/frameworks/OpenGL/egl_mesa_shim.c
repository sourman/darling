/* Stand-in for ANGLE's libEGL. Chromium dlopens this and asks
 * eglGetPlatformDisplay for an ANGLE platform. Those platforms have no
 * device here. Log the request, then hand back the Mesa vendor that is
 * already installed (surfaceless, or X11 on DISPLAY). */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
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
static void ensure_external_context(void);

static void patch_angle_gl_get_string(void);
static void hook_glvnd_create_context(void);

__attribute__((constructor))
static void shim_loaded(void)
{
	setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "mesa libEGL shim loaded\n");
	/* glGetString runs on this thread as soon as the loader returns. */
	ensure_external_context();
	patch_angle_gl_get_string();
	hook_glvnd_create_context();
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
	if (d) {
		EGLBoolean (*init)(EGLDisplay, EGLint *, EGLint *) = M("eglInitialize");
		EGLint major = 0, minor = 0;
		if (init && !init(d, &major, &minor))
			fprintf(stderr, "mesa eglInitialize failed eglError=0x%x\n", mesa_error());
		else
			fprintf(stderr, "mesa eglInitialize %d.%d\n", major, minor);
		return d;
	}
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

/* Mesa rejects ANGLE-only context tokens with EGL_BAD_ATTRIBUTE. Keep the
 * version and the few core flags Mesa implements. */
static const EGLint *mesa_context_attribs(const EGLint *attrib, EGLint *buf, int cap)
{
	int n = 0;
	int i;
	if (!attrib)
		return NULL;
	fprintf(stderr, "eglCreateContext attribs");
	for (i = 0; attrib[i] != EGL_NONE && i < 48; i += 2)
		fprintf(stderr, " 0x%x=0x%x", attrib[i], attrib[i + 1]);
	fprintf(stderr, "\n");
	for (i = 0; attrib[i] != EGL_NONE && i < 48 && n + 2 < cap; i += 2) {
		EGLint k = attrib[i];
		/* 0x31B3 (NO_ERROR) is legal EGL but this Mesa returns BAD_ATTRIBUTE. */
		if (k == 0x3098 || k == 0x30FB) {
			buf[n++] = k;
			buf[n++] = attrib[i + 1];
		}
	}
	buf[n] = EGL_NONE;
	return buf;
}

/* GLVND's eglCreateContext is what the GPU calls. The shim export is not.
 * Jump into the function after its first 12 bytes, with the attrib pointer
 * already reduced to tokens Mesa accepts. */
void *glvnd_slot;
void *glvnd_continue;

const EGLint *filter_ctx_attribs(const EGLint *attrib)
{
	static __thread EGLint buf[40];
	if (!attrib)
		return NULL;
	return mesa_context_attribs(attrib, buf, 40);
}

__attribute__((naked)) static void glvnd_hook(void)
{
	__asm__ volatile(
		"endbr64\n"
		"push %rbp\n"
		"push %rdi\n"
		"push %rsi\n"
		"push %rdx\n"
		"push %rcx\n"
		"push %r8\n"
		"push %r9\n"
		"sub $8, %rsp\n"
		"mov %rcx, %rdi\n"
		"call _filter_ctx_attribs\n"
		"add $8, %rsp\n"
		"pop %r9\n"
		"pop %r8\n"
		"pop %rcx\n"
		"pop %rdx\n"
		"pop %rsi\n"
		"pop %rdi\n"
		"mov %rax, %rcx\n"
		"mov _glvnd_slot(%rip), %rax\n"
		"mov (%rax), %rax\n"
		"jmp *_glvnd_continue(%rip)\n"
	);
}

static int prot_page(void *page)
{
	int (*mp)(void *, unsigned long, int) = dlsym((void *)-2, "mprotect");
	if (!mp)
		return -1;
	return mp(page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);
}

static void hook_glvnd_create_context(void)
{
	FILE *maps;
	char line[512];
	unsigned long start = 0, off = 0;
	unsigned char *fn;
	int32_t disp;

	maps = fopen("/proc/self/maps", "r");
	if (!maps)
		return;
	while (fgets(line, sizeof line, maps)) {
		if (!strstr(line, "libEGL.so.1") || !strstr(line, "r-xp"))
			continue;
		sscanf(line, "%lx-%*x %*s %lx", &start, &off);
		break;
	}
	fclose(maps);
	if (!start) {
		fprintf(stderr, "glvnd eglCreateContext not mapped\n");
		return;
	}
	fn = (unsigned char *)(start + (0x8b20 - off));
	if (fn[0] != 0xf3 || fn[5] != 0x48) {
		fprintf(stderr, "glvnd prologue mismatch %02x %02x\n", fn[0], fn[5]);
		return;
	}
	disp = *(int32_t *)(fn + 8);
	glvnd_slot = fn + 12 + disp;
	glvnd_continue = fn + 12;
	if (prot_page((void *)((uintptr_t)fn & ~(uintptr_t)0xfff)) != 0) {
		fprintf(stderr, "glvnd mprotect failed\n");
		return;
	}
	fn[0] = 0x48;
	fn[1] = 0xb8;
	{
		void *hook = (void *)glvnd_hook;
		memcpy(fn + 2, &hook, sizeof hook);
	}
	fn[10] = 0xff;
	fn[11] = 0xe0;
	fprintf(stderr, "glvnd eglCreateContext hooked at %p\n", fn);
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share, const EGLint *attrib)
{
	EGLContext (*fn)(EGLDisplay, EGLConfig, EGLContext, const EGLint *) = M("eglCreateContext");
	EGLint es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLint filtered[32];
	EGLContext ctx;
	EGLBoolean (*bind)(EGLenum) = M("eglBindAPI");
	const EGLint *use = attrib;
	if (bind)
		bind(EGL_OPENGL_ES_API);
	if (attrib)
		use = mesa_context_attribs(attrib, filtered, 32);
	if (!use)
		use = es2;
	ctx = fn ? fn(dpy, config, share, use) : NULL;
	if (!ctx && attrib) {
		ctx = fn ? fn(dpy, config, share, es2) : NULL;
		fprintf(stderr, "eglCreateContext retry ES2 ctx=%p eglError=0x%x\n", ctx, mesa_error());
	}
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

static EGLDisplay external_dpy;
static EGLSurface external_surf;
static EGLContext external_ctx;

static void ensure_external_context(void)
{
	EGLint cfgattr[] = {
		0x3033, 0x0001, /* EGL_SURFACE_TYPE, EGL_PBUFFER_BIT */
		0x3040, 0x0004, /* EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT */
		0x3024, 8, 0x3023, 8, 0x3022, 8, /* RGB */
		EGL_NONE
	};
	EGLConfig config;
	EGLint got = 0;
	const unsigned char *(*glver)(unsigned int);

	if (external_ctx)
		return;
	external_dpy = mesa_display();
	eglBindAPI(EGL_OPENGL_ES_API);
	if (!eglChooseConfig(external_dpy, cfgattr, &config, 1, &got) || got < 1) {
		fprintf(stderr, "external context: choose config failed eglError=0x%x\n", eglGetError());
		return;
	}
	external_ctx = eglCreateContext(external_dpy, config, NULL,
		(const EGLint[]){ 0x3098, 3, EGL_NONE }); /* EGL_CONTEXT_CLIENT_VERSION */
	external_surf = eglCreatePbufferSurface(external_dpy, config,
		(const EGLint[]){ 0x3057, 16, 0x3056, 16, EGL_NONE }); /* WIDTH, HEIGHT */
	fprintf(stderr, "external context ctx=%p surf=%p\n", external_ctx, external_surf);
	if (external_ctx && external_surf &&
	    eglMakeCurrent(external_dpy, external_surf, external_surf, external_ctx)) {
		glver = (void *)eglGetProcAddress("glGetString");
		fprintf(stderr, "external GL_VERSION %s\n",
			(glver && glver(GL_VERSION)) ? (const char *)glver(GL_VERSION) : "(null)");
	}
}

/* Mesa's ES internalformat query rejects GL_BGRA8_EXT (0x93A1). Skia then
 * records no sample counts for that format, and the output-surface
 * characterization stays zero at the color-type word. Report BGRA8 as
 * color-renderable with one sample, and allocate it as RGBA8. */
static int bgra8(unsigned fmt)
{
	return fmt == 0x93A1;
}

void shim_glGetInternalformativ(unsigned target, unsigned fmt, unsigned pname,
		int count, int *params)
{
	typedef void (*fn_t)(unsigned, unsigned, unsigned, int, int *);
	static fn_t real;
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glGetInternalformativ");
	if (bgra8(fmt)) {
		if ((pname == 0x826F || pname == 0x8286 || pname == 0x8289) &&
		    params && count > 0) {
			params[0] = 1;
			return;
		}
		if (pname == 0x9380 && params && count > 0) {
			params[0] = 1;
			return;
		}
		if (pname == 0x80A9 && params && count > 0) {
			params[0] = 1;
			return;
		}
		fmt = 0x8058;
	}
	if (real)
		real(target, fmt, pname, count, params);
}

void shim_glRenderbufferStorage(unsigned target, unsigned fmt, int w, int h)
{
	typedef void (*fn_t)(unsigned, unsigned, int, int);
	static fn_t real;
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glRenderbufferStorage");
	if (bgra8(fmt))
		fmt = 0x8058;
	if (real)
		real(target, fmt, w, h);
}

void shim_glRenderbufferStorageMultisample(unsigned target, int samples,
		unsigned fmt, int w, int h)
{
	typedef void (*fn_t)(unsigned, int, unsigned, int, int);
	static fn_t real;
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glRenderbufferStorageMultisample");
	if (bgra8(fmt))
		fmt = 0x8058;
	if (real)
		real(target, samples, fmt, w, h);
}

void shim_glTexImage2D(unsigned target, int level, int internal,
		int w, int h, int border, unsigned format, unsigned type,
		const void *pixels)
{
	typedef void (*fn_t)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
	static fn_t real;
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glTexImage2D");
	if (bgra8((unsigned)internal))
		internal = 0x8058;
	if (real)
		real(target, level, internal, w, h, border, format, type, pixels);
}

void shim_glTexStorage2D(unsigned target, int levels, unsigned fmt, int w, int h)
{
	typedef void (*fn_t)(unsigned, int, unsigned, int, int);
	static fn_t real;
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glTexStorage2D");
	if (bgra8(fmt))
		fmt = 0x8058;
	if (real)
		real(target, levels, fmt, w, h);
}

static void *shim_gl(const char *name)
{
	if (!strcmp(name, "glGetInternalformativ"))
		return (void *)shim_glGetInternalformativ;
	if (!strcmp(name, "glRenderbufferStorage"))
		return (void *)shim_glRenderbufferStorage;
	if (!strcmp(name, "glRenderbufferStorageMultisample"))
		return (void *)shim_glRenderbufferStorageMultisample;
	if (!strcmp(name, "glTexImage2D"))
		return (void *)shim_glTexImage2D;
	if (!strcmp(name, "glTexStorage2D"))
		return (void *)shim_glTexStorage2D;
	return NULL;
}

/* ANGLE's gl* stubs read an empty TLS context, so glGetIntegerv returns 0
 * and shared GL init rejects the context. Point each exported stub at Mesa. */
static const char *const k_gl_names[] = {
#include "egl_mesa_gl_names.inc"
};

static void patch_angle_gl_get_string(void)
{
	const char *path = "/Applications/Comet.app/Contents/Frameworks/Comet Framework.framework/Versions/145.2.7632.4581/Libraries/libGLESv2.dylib";
	void *gles;
	int (*mp)(void *, unsigned long, int);
	uintptr_t last_page = 0;
	unsigned i;
	int patched = 0;

	gles = dlopen(path, 0x10 /* RTLD_NOLOAD */);
	if (!gles)
		gles = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!gles || !mesa_proc) {
		fprintf(stderr, "angle libGLESv2 not loaded: %s\n", dlerror());
		return;
	}
	mp = dlsym((void *)-2, "mprotect");
	for (i = 0; i < sizeof k_gl_names / sizeof k_gl_names[0]; i++) {
		unsigned char *stub = dlsym(gles, k_gl_names[i]);
		void *fn = shim_gl(k_gl_names[i]);
		if (!fn)
			fn = mesa_proc(k_gl_names[i]);
		uintptr_t page;
		if (!stub || !fn || stub[0] != 0x55)
			continue;
		page = (uintptr_t)stub & ~(uintptr_t)0xfff;
		if (page != last_page) {
			if (!mp || mp((void *)page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
				fprintf(stderr, "angle gl mprotect failed %s\n", k_gl_names[i]);
				return;
			}
			last_page = page;
		}
		stub[0] = 0x48;
		stub[1] = 0xb8;
		memcpy(stub + 2, &fn, sizeof fn);
		stub[10] = 0xff;
		stub[11] = 0xe0;
		patched++;
	}
	fprintf(stderr, "angle gl stubs patched %d including glGetIntegerv\n", patched);
}

/* ANGLE looks this up and, while the proc is null, never sees the Mesa context.
 * The header declares it void; Chromium uses the return as the external context. */
EGLContext eglAcquireExternalContextANGLE(EGLDisplay dpy, EGLSurface drawAndRead)
{
	EGLSurface surf;
	EGLContext current;

	fprintf(stderr, "eglAcquireExternalContextANGLE dpy=%p surface=%p\n", dpy, drawAndRead);
	ensure_external_context();
	current = eglGetCurrentContext();
	if (!current)
		current = external_ctx;
	surf = drawAndRead ? drawAndRead : external_surf;
	if (current && surf)
		eglMakeCurrent(external_dpy, surf, surf, current);
	fprintf(stderr, "eglAcquireExternalContextANGLE -> %p\n", current);
	return current;
}

void eglReleaseExternalContextANGLE(EGLDisplay dpy)
{
	fprintf(stderr, "eglReleaseExternalContextANGLE dpy=%p\n", dpy);
	if (external_dpy)
		eglMakeCurrent(external_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void (*eglGetProcAddress(const char *name))(void)
{
	static int primed;
	fprintf(stderr, "eglGetProcAddress %s\n", name ? name : "(null)");
	load_mesa();
	if (!primed) {
		primed = 1;
		ensure_external_context();
	}
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
		if (!strcmp(name, "eglAcquireExternalContextANGLE"))
			return (void (*)(void))eglAcquireExternalContextANGLE;
		if (!strcmp(name, "eglReleaseExternalContextANGLE"))
			return (void (*)(void))eglReleaseExternalContextANGLE;
		if (shim_gl(name))
			return (void (*)(void))shim_gl(name);
	}
	if (!mesa_proc)
		return NULL;
	return (void (*)(void))mesa_proc(name);
}
