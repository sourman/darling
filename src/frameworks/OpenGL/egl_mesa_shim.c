/* Stand-in for ANGLE's libEGL. Chromium dlopens this and asks
 * eglGetPlatformDisplay for an ANGLE platform. Those platforms have no
 * device here. Log the request, then hand back the Mesa vendor that is
 * already installed (surfaceless, or X11 on DISPLAY). */
#include <dlfcn.h>
#include <fcntl.h>
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
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_DRAW 0x3059
#define EGL_IOSURFACE_ANGLE 0x3454
#define GL_VERSION 0x1F02
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_BGRA 0x80E1
#define GL_UNSIGNED_BYTE 0x1401

static void *mesa;
static void *(*mesa_proc)(const char *);
static void ensure_external_context(void);
static void cache_mesa_extensions(void);
static void log_texturable_store(void);
static void log_texturable_flags(void);

static void patch_angle_gl_get_string(void);
static void hook_glvnd_create_context(void);
static void hook_mesa_iosurface_pbuffer(void);
EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
		void *buffer, EGLConfig config, const EGLint *attrib);
static void log_clear_branch(void);
static void log_rect_zero(void);
static void log_rect_store(void);
static void log_rect_es(void);
static volatile int shim_ready;

static int proc_is_gpu(void)
{
	char buf[512];
	int fd = open("/proc/self/cmdline", 0);
	int n, i;
	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (n < 0)
		n = 0;
	buf[n] = 0;
	for (i = 0; i < n; i++)
		if (buf[i] == 0)
			buf[i] = ' ';
	return strstr(buf, "--type=gpu") != 0;
}

__attribute__((constructor))
static void shim_loaded(void)
{
	setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "mesa libEGL shim loaded pid=%d gpu=%d\n", getpid(), proc_is_gpu());
	/* glGetString runs on this thread as soon as the loader returns. */
	ensure_external_context();
	patch_angle_gl_get_string();
	hook_glvnd_create_context();
	hook_mesa_iosurface_pbuffer();
	shim_ready = 1;
	if (proc_is_gpu()) {
		log_rect_store();
		log_rect_es();
	}
}

static void note_bgra_ext(int found)
{
	fprintf(stderr, "bgra texturable hasExtension %d\n", found);
}

static void note_bgra_skip(void)
{
	fprintf(stderr, "bgra texturable block skipped\n");
}

/* The store is movl $0x11, 0x340(%r15) at 0x3b1f9e6, reached only when the
 * byte at -0x64(%rbp) is nonzero and hasExtension("GL_EXT_texture_format_BGRA8888")
 * returns >= 0. Log both gates. Process memory only. */
static void log_texturable_store(void)
{
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	int (*mp)(void *, unsigned long, int);
	unsigned char *base = NULL, *p, *page;
	unsigned char *call, *skip;
	void *nfn = note_bgra_ext;
	void *sfn = note_bgra_skip;
	unsigned char *real, *skip_to;
	int rel;

	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		unsigned long start, end, off;
		char perm[8];
		if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
			continue;
		if (off != 0x4000 || perm[0] != 'r' || perm[2] != 'x')
			continue;
		if (end - start < 16 * 1024 * 1024)
			continue;
		if (!strstr(line, "Comet Framework"))
			continue;
		base = (unsigned char *)start;
		break;
	}
	fclose(f);
	if (!base)
		return;
	call = base + 0x3b1f9d9;
	skip = base + 0x3b1f9c3;
	if (call[0] != 0xe8 || skip[0] != 0x0f || skip[1] != 0x84) {
		fprintf(stderr, "texturable site mismatch\n");
		return;
	}
	mp = dlsym((void *)-2, "mprotect");
	page = (unsigned char *)((uintptr_t)skip & ~(uintptr_t)0xfff);
	if (!mp || mp(page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
		return;
	p = mmap(0, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1)
		return;
	memcpy(&rel, call + 1, 4);
	real = call + 5 + rel;
	memcpy(&rel, skip + 2, 4);
	skip_to = skip + 6 + rel;
	/* call stub */
	{
		unsigned char *s = p;
		rel = (int)(s - (call + 5));
		call[0] = 0xe8;
		memcpy(call + 1, &rel, 4);
		*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &real, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x50;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
		*p++ = 0x89; *p++ = 0xc7;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &nfn, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
		*p++ = 0x58;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
		*p++ = 0xc3;
		/* skip stub, entered by the je */
		s = p;
		rel = (int)(s - (skip + 6));
		memcpy(skip + 2, &rel, 4);
		*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &sfn, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &skip_to, 8); p += 8;
		*p++ = 0xff; *p++ = 0xe0;
	}
	fprintf(stderr, "texturable store logger armed\n");
}

/* isFormatTexturable entry is 13 bytes at 0x9228790. Log caps+0x340 when the
 * backend format at rsi+0x10 is 0x93A1, then replay the stolen prologue.
 * Absolute jump: a relative one from this mapping does not reach the stub. */
static void note_flags6(void *caps, unsigned flags, unsigned fmt)
{
	static int n, nb;
	if (fmt == 0x93A1) {
		if (nb++ >= 8)
			return;
	} else if (n++ >= 6) {
		return;
	}
	fprintf(stderr, "isFormatTexturable caps %p flags6 %#x fmt %#x\n",
		caps, flags, fmt);
}

static void log_texturable_flags(void)
{
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	int (*mp)(void *, unsigned long, int);
	unsigned char *bases[4], *ends[4];
	unsigned char *base, *map_end, *site, *page, *p, *gate;
	void *fn = note_flags6;
	unsigned char *cont, *je_to;
	int nbases = 0, bi;

	if (!f)
		return;
	while (fgets(line, sizeof line, f) && nbases < 4) {
		unsigned long start, end, off;
		char perm[8];
		if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
			continue;
		if (off != 0x4000 || perm[0] != 'r' || perm[2] != 'x')
			continue;
		if (end - start < 16 * 1024 * 1024)
			continue;
		if (!strstr(line, "Comet Framework"))
			continue;
		bases[nbases] = (unsigned char *)start;
		ends[nbases] = (unsigned char *)end;
		nbases++;
	}
	fclose(f);
	/* Also patch whatever address the caps vtable slot +0x20 actually holds. */
	{
		FILE *vf = fopen("/proc/self/maps", "r");
		unsigned long slot_off = 0x4000ul + 0xe01b2b0ul;
		if (vf) {
			while (fgets(line, sizeof line, vf) && nbases < 4) {
				unsigned long start, end, off;
				char perm[8];
				unsigned long fnaddr;
				if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
					continue;
				if (slot_off < off || slot_off >= off + (end - start))
					continue;
				if (perm[0] != 'r')
					continue;
				fnaddr = *(unsigned long *)(start + (slot_off - off));
				fprintf(stderr, "texturable vtable slot %p\n", (void *)fnaddr);
				if (nbases < 4 && fnaddr > 0x10000) {
					bases[nbases] = (unsigned char *)fnaddr - 0x9228790;
					ends[nbases] = bases[nbases];
					nbases++;
				}
				break;
			}
			fclose(vf);
		}
	}
	if (!nbases)
		return;
	for (bi = 0; bi < nbases; bi++) {
	base = bases[bi];
	map_end = ends[bi];
	/* Function entry. Log the first calls and every 0x93A1 call. */
	site = base + 0x9228790;
	if (site[0] != 0x55 || site[4] != 0x83) {
		fprintf(stderr, "texturable flags site mismatch %02x\n", site[0]);
		continue;
	}
	mp = dlsym((void *)-2, "mprotect");
	page = (unsigned char *)((uintptr_t)site & ~(uintptr_t)0xfff);
	if (!mp || mp(page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
		continue;
	/* The stub has to sit within 2GB of the je, and not on top of the text. */
	p = mmap(map_end, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1 ||
	    (unsigned long)p + 0x80000000ul < (unsigned long)site ||
	    (unsigned long)site + 0x80000000ul < (unsigned long)p)
		p = mmap(base - 0x100000, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1)
		continue;
	if ((unsigned long)p + 0x80000000ul < (unsigned long)site ||
	    (unsigned long)site + 0x80000000ul < (unsigned long)p) {
		fprintf(stderr, "texturable flags stub out of range %p\n", p);
		continue;
	}
	cont = site + 13;
	je_to = base + 0x9228852;
	site[0] = 0x48; site[1] = 0xb8;
	memcpy(site + 2, &p, 8);
	site[10] = 0xff; site[11] = 0xe0; site[12] = 0x90;
	*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
	*p++ = 0x57; *p++ = 0x56; *p++ = 0x52; *p++ = 0x51;
	*p++ = 0x41; *p++ = 0x50; *p++ = 0x41; *p++ = 0x51;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
	*p++ = 0x8b; *p++ = 0xb7; *p++ = 0x40; *p++ = 0x03; *p++ = 0x00; *p++ = 0x00;
	*p++ = 0x8b; *p++ = 0x56; *p++ = 0x10;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &fn, 8); p += 8;
	*p++ = 0xff; *p++ = 0xd0;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
	*p++ = 0x41; *p++ = 0x59; *p++ = 0x41; *p++ = 0x58;
	*p++ = 0x59; *p++ = 0x5a; *p++ = 0x5e; *p++ = 0x5f;
	*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
	*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
	*p++ = 0x83; *p++ = 0xfa; *p++ = 0x02;
	*p++ = 0x0f; *p++ = 0x84;
	{
		int rel = (int)(je_to - (p + 4));
		memcpy(p, &rel, 4);
		p += 4;
	}
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &cont, 8); p += 8;
	*p++ = 0xff; *p++ = 0xe0;
	(void)gate;
	fprintf(stderr, "texturable flags logger armed base %p\n", base);
	}
}

static void note_clear(unsigned site, void *caps)
{
	unsigned flags = 0;
	if (site != 0x1e3450d && caps)
		flags = *(unsigned *)((unsigned char *)caps + 0x340);
	fprintf(stderr, "clear branch %#x caps %p flags6 %#x\n", site, caps, flags);
}

static unsigned char *fw_base;

static void note_vt38(void *fn)
{
	fprintf(stderr, "vtable +0x38 offset %#lx\n",
		(unsigned long)((unsigned char *)fn - fw_base));
}

static void note_vt20(void *fn)
{
	fprintf(stderr, "vtable +0x20 offset %#lx\n",
		(unsigned long)((unsigned char *)fn - fw_base));
}

/* textureType == kRectangle && !rectangleTextureSupport() is the xor at
 * 0x922885f. Log that return-0. Process memory only. */
static void note_rect(unsigned byte)
{
	static int n;
	if (n++ >= 4)
		return;
	fprintf(stderr, "texturable rectangle gate bit14d=%#x bit0x10=%d\n",
		byte, (byte & 0x10) ? 1 : 0);
}

static void log_rect_zero(void)
{
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	int (*mp)(void *, unsigned long, int);
	unsigned char *base = NULL, *site, *page, *p;
	void *fn = note_rect;
	unsigned char *ok, *ret0;

	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		unsigned long start, end, off;
		char perm[8];
		if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
			continue;
		if (off != 0x4000 || perm[0] != 'r' || perm[2] != 'x')
			continue;
		if (end - start < 16 * 1024 * 1024)
			continue;
		if (!strstr(line, "Comet Framework"))
			continue;
		base = (unsigned char *)start;
		break;
	}
	fclose(f);
	if (!base)
		return;
	site = base + 0x9228852;
	if (site[0] != 0xf6 || site[7] != 0x0f) {
		fprintf(stderr, "rectangle gate mismatch %02x\n", site[0]);
		return;
	}
	mp = dlsym((void *)-2, "mprotect");
	page = (unsigned char *)((uintptr_t)site & ~(uintptr_t)0xfff);
	if (!mp || mp(page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
		return;
	p = mmap(base - 0x300000, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1)
		p = mmap(0, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1)
		return;
	ok = base + 0x922879d;
	ret0 = base + 0x92287f1;
	site[0] = 0x48; site[1] = 0xb8;
	memcpy(site + 2, &p, 8);
	site[10] = 0xff; site[11] = 0xe0; site[12] = 0x90;
	*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
	*p++ = 0x57;
	*p++ = 0x0f; *p++ = 0xb6; *p++ = 0x87; *p++ = 0x4d; *p++ = 0x01; *p++ = 0x00; *p++ = 0x00;
	*p++ = 0x89; *p++ = 0xc7;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &fn, 8); p += 8;
	*p++ = 0xff; *p++ = 0xd0;
	*p++ = 0x5f;
	*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
	*p++ = 0xf6; *p++ = 0x87; *p++ = 0x4d; *p++ = 0x01; *p++ = 0x00; *p++ = 0x00; *p++ = 0x10;
	*p++ = 0x74; *p++ = 0x0c;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &ok, 8); p += 8;
	*p++ = 0xff; *p++ = 0xe0;
	*p++ = 0x31; *p++ = 0xc0;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &ret0, 8); p += 8;
	*p++ = 0xff; *p++ = 0xe0;
	fprintf(stderr, "rectangle zero logger armed\n");
}

/* orb $0x10, 1(%rax) at 0x3b2acab sets the rectangle bit. It is reached when
 * the version word at -0x4c(%rbp) is > 0x30000, or hasExtension of either
 * rectangle string returns >= 0. Log the version and that call's result. */
static void note_rect_ver(unsigned ver)
{
	fprintf(stderr, "rectangle store version %#x\n", ver);
	fflush(stderr);
}

static void note_rect_ext(int found, unsigned count)
{
	fprintf(stderr, "rectangle store hasExtension %d list %u\n", found, count);
	fflush(stderr);
}

static void note_rect_stored(void)
{
	fprintf(stderr, "rectangle bit store ran\n");
	fflush(stderr);
}

static void log_rect_store(void)
{
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	int (*mp)(void *, unsigned long, int);
	unsigned char *base = NULL, *ver, *call, *store, *page, *p;
	void *vn = note_rect_ver, *en = note_rect_ext, *sn = note_rect_stored;
	unsigned char *real, *after;
	int rel;

	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		unsigned long start, end, off;
		char perm[8];
		if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
			continue;
		if (off != 0x4000 || perm[0] != 'r' || perm[2] != 'x')
			continue;
		if (end - start < 16 * 1024 * 1024)
			continue;
		if (!strstr(line, "Comet Framework"))
			continue;
		base = (unsigned char *)start;
		ver = base + 0x3b2ac7f;
		call = base + 0x3b2ac9e;
		store = base + 0x3b2aca7;
		if (ver[0] != 0x81 || call[0] != 0xe8 || store[0] != 0x48)
			continue;
	mp = dlsym((void *)-2, "mprotect");
	page = (unsigned char *)((uintptr_t)ver & ~(uintptr_t)0xfff);
	if (!mp || mp(page, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
		continue;
	p = mmap(base + 0xdd20000, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1)
		p = mmap(0, 4096, 7, 0x1002, -1, 0);
	if (!p || p == (unsigned char *)-1)
		continue;
	if ((unsigned long)p + 0x80000000ul < (unsigned long)ver ||
	    (unsigned long)ver + 0x80000000ul < (unsigned long)p) {
		fprintf(stderr, "rectangle store stub out of range %p\n", p);
		continue;
	}
	/* version compare: log [rbp-0x4c], then run the cmpl and the ja */
	rel = (int)(p - (ver + 5));
	ver[0] = 0xe9;
	memcpy(ver + 1, &rel, 4);
	ver[5] = 0x90;
	ver[6] = 0x90;
	*p++ = 0x8b; *p++ = 0x7d; *p++ = 0xb4;
	*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &vn, 8); p += 8;
	*p++ = 0xff; *p++ = 0xd0;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
	*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
	*p++ = 0x81; *p++ = 0x7d; *p++ = 0xb4; *p++ = 0x00; *p++ = 0x00; *p++ = 0x03; *p++ = 0x00;
	after = ver + 7;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &after, 8); p += 8;
	*p++ = 0xff; *p++ = 0xe0;
	/* hasExtension call. rdi is the extension list, count at +8. */
	memcpy(&rel, call + 1, 4);
	real = call + 5 + rel;
	rel = (int)(p - (call + 5));
	memcpy(call + 1, &rel, 4);
	*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
	*p++ = 0x57; *p++ = 0x56;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &real, 8); p += 8;
	*p++ = 0xff; *p++ = 0xd0;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
	*p++ = 0x5e; *p++ = 0x5f;
	*p++ = 0x89; *p++ = 0xc7;
	*p++ = 0x8b; *p++ = 0x77; *p++ = 0x08;
	*p++ = 0x50;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &en, 8); p += 8;
	*p++ = 0xff; *p++ = 0xd0;
	*p++ = 0x58;
	*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
	*p++ = 0xc3;
	/* store itself */
	rel = (int)(p - (store + 5));
	store[0] = 0xe9;
	memcpy(store + 1, &rel, 4);
	/* 48 8b 45 98 80 48 01 10 is 8 bytes; e9 is 5, leave the orb to be replaced */
	store[5] = 0x90; store[6] = 0x90; store[7] = 0x90;
	*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &sn, 8); p += 8;
	*p++ = 0xff; *p++ = 0xd0;
	*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
	*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
	*p++ = 0x48; *p++ = 0x8b; *p++ = 0x45; *p++ = 0x98;
	*p++ = 0x80; *p++ = 0x48; *p++ = 0x01; *p++ = 0x10;
	after = store + 8;
	*p++ = 0x48; *p++ = 0xb8; memcpy(p, &after, 8); p += 8;
	*p++ = 0xff; *p++ = 0xe0;
		fprintf(stderr, "rectangle store logger armed\n");
	}
	fclose(f);
}


/* GLES/desktop shared check at 0x3b27eb0. hasExtension of
 * GL_ARB_texture_rectangle. A negative result falls through to the ANGLE
 * string; if that is also negative the OR mask is 0 and bit 0x10 of
 * caps+0x14d stays clear. */
static void log_rect_es(void)
{
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	int (*mp)(void *, unsigned long, int);
	unsigned char *call, *page, *p, *after;
	void *en = note_rect_ext;
	int rel;

	if (!f)
		return;
	mp = dlsym((void *)-2, "mprotect");
	while (fgets(line, sizeof line, f)) {
		unsigned long start, end, off;
		char perm[8];
		if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
			continue;
		if (off != 0x4000 || perm[0] != 'r' || perm[2] != 'x')
			continue;
		if (end - start < 16 * 1024 * 1024)
			continue;
		if (!strstr(line, "Comet Framework"))
			continue;
		/* orq site. eax is 0x1000 when the search hit, else 0. */
		call = (unsigned char *)start + 0x3b27eda;
		if (call[0] != 0x48 || call[1] != 0xc7)
			continue;
		page = (unsigned char *)((uintptr_t)call & ~(uintptr_t)0xfff);
		if (!mp || mp(page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
			continue;
		p = mmap((unsigned char *)start + 0xdd21000, 4096, 7, 0x1002, -1, 0);
		if (!p || p == (unsigned char *)-1)
			p = mmap(0, 4096, 7, 0x1002, -1, 0);
		if (!p || p == (unsigned char *)-1)
			continue;
		if ((unsigned long)p + 0x80000000ul < (unsigned long)call ||
		    (unsigned long)call + 0x80000000ul < (unsigned long)p) {
			fprintf(stderr, "rectangle es stub out of range\n");
			continue;
		}
		rel = (int)(p - (call + 5));
		call[0] = 0xe9;
		memcpy(call + 1, &rel, 4);
		call[5] = 0x90;
		call[6] = 0x90;
		*p++ = 0x50;
		*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
		*p++ = 0x8b; *p++ = 0x7d; *p++ = 0x08;
		*p++ = 0x31; *p++ = 0xf6;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &en, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
		*p++ = 0x58;
		*p++ = 0x48; *p++ = 0xc7; *p++ = 0xc1; *p++ = 0xff; *p++ = 0xef; *p++ = 0xff; *p++ = 0xff;
		after = call + 7;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &after, 8); p += 8;
		*p++ = 0xff; *p++ = 0xe0;
		fprintf(stderr, "rectangle es logger armed\n");
	}
	fclose(f);
}

/* Retarget every conditional jump into the clear at 0x1e3473b onto a stub
 * that records the jump, then runs the clear. The bytes at the clear itself
 * record a fall-through. */
static void log_clear_branch(void)
{
	static const unsigned sites[] = {
		0x1e3450d, 0x1e3452b, 0x1e34542, 0x1e3454d, 0x1e3455f,
		0x1e3456a, 0x1e34572, 0x1e345b6, 0x1e345db, 0x1e34604, 0x1e3462b
	};
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	int (*mp)(void *, unsigned long, int);
	unsigned char *base = NULL, *page, *stub, *p;
	unsigned i;
	unsigned char *clear, *cont, *call_target;
	void *fn = note_clear;

	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		unsigned long start, end, off;
		char perm[8];
		if (sscanf(line, "%lx-%lx %7s %lx", &start, &end, perm, &off) != 4)
			continue;
		if (off != 0x4000 || perm[0] != 'r' || perm[2] != 'x')
			continue;
		if (end - start < 16 * 1024 * 1024)
			continue;
		if (!strstr(line, "Comet Framework"))
			continue;
		base = (unsigned char *)start;
		fw_base = base;
		break;
	}
	fclose(f);
	if (!base) {
		fprintf(stderr, "clear branch framework missing\n");
		return;
	}
	clear = base + 0x1e3473b;
	if (clear[0] != 0x48 || clear[1] != 0x89 || clear[2] != 0xdf || clear[3] != 0xe8) {
		fprintf(stderr, "clear prologue mismatch %p\n", clear);
		return;
	}
	mp = dlsym((void *)-2, "mprotect");
	page = (unsigned char *)((uintptr_t)(base + 0x1e3450d) & ~(uintptr_t)0xfff);
	if (!mp || mp(page, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
		fprintf(stderr, "clear branch mprotect failed\n");
		return;
	}
	stub = mmap(base - 0x200000, 4096, 7, 0x1002, -1, 0);
	if (!stub || stub == (unsigned char *)-1 ||
	    (unsigned long)stub + 0x80000000ul < (unsigned long)base ||
	    (unsigned long)base + 0x80000000ul < (unsigned long)stub)
		stub = mmap(0, 4096, 7, 0x1002, -1, 0);
	if (!stub || stub == (unsigned char *)-1)
		return;
	if ((unsigned long)stub + 0x80000000ul < (unsigned long)(base + 0x1e3462b) ||
	    (unsigned long)(base + 0x1e3462b) + 0x80000000ul < (unsigned long)stub) {
		fprintf(stderr, "clear branch stub out of range %p\n", stub);
		return;
	}
	p = stub;
	cont = base + 0x1e34743;
	call_target = base + 0x811b880;
	for (i = 0; i < sizeof sites / sizeof sites[0]; i++) {
		unsigned char *j = base + sites[i];
		int rel;
		unsigned char *target;
		if (j[0] != 0x0f || (j[1] != 0x84 && j[1] != 0x8e && j[1] != 0x8f))
			continue;
		memcpy(&rel, j + 2, 4);
		target = j + 6 + rel;
		if (target != clear)
			continue;
		rel = (int)(p - (j + 6));
		memcpy(j + 2, &rel, 4);
		/* stack is 16-byte aligned at the jump. */
		*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
		*p++ = 0xbf; memcpy(p, &sites[i], 4); p += 4;
		*p++ = 0x49; *p++ = 0x8b; *p++ = 0xb5; *p++ = 0x30; *p++ = 0x01; *p++ = 0x00; *p++ = 0x00;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &fn, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xdf;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &call_target, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &cont, 8); p += 8;
		*p++ = 0xff; *p++ = 0xe0;
	}
	fprintf(stderr, "clear branch logger armed\n");
	if (0) {
		unsigned char *fall = p;
		int rel = (int)(fall - (clear + 5));
		unsigned site = 0x1e3473b;
		*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
		*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
		*p++ = 0xbf; memcpy(p, &site, 4); p += 4;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &fn, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
		*p++ = 0x48; *p++ = 0x89; *p++ = 0xdf;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &call_target, 8); p += 8;
		*p++ = 0xff; *p++ = 0xd0;
		*p++ = 0x48; *p++ = 0xb8; memcpy(p, &cont, 8); p += 8;
		*p++ = 0xff; *p++ = 0xe0;
		clear[0] = 0xe9;
		memcpy(clear + 1, &rel, 4);
	}
	/* callq *0x38(%rax); testb %al,%al  is the jump that cleared last time.
	 * Replace those five bytes with a call that logs the method, then the
	 * existing je still sees al. */
	{
		unsigned char *c = base + 0x1e345d6;
		unsigned char *s = p;
		int rel;
		void *nfn = note_vt38;
		if (c[0] == 0xff && c[1] == 0x50 && c[2] == 0x38 && c[3] == 0x84 && c[4] == 0xc0) {
			rel = (int)(s - (c + 5));
			c[0] = 0xe8;
			memcpy(c + 1, &rel, 4);
			*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
			*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
			*p++ = 0x50;
			*p++ = 0x57; *p++ = 0x56; *p++ = 0x52; *p++ = 0x51;
			*p++ = 0x41; *p++ = 0x50; *p++ = 0x41; *p++ = 0x51;
			*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
			*p++ = 0x48; *p++ = 0x8b; *p++ = 0x78; *p++ = 0x38;
			*p++ = 0x48; *p++ = 0xb8; memcpy(p, &nfn, 8); p += 8;
			*p++ = 0xff; *p++ = 0xd0;
			*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
			*p++ = 0x41; *p++ = 0x59; *p++ = 0x41; *p++ = 0x58;
			*p++ = 0x59; *p++ = 0x5a; *p++ = 0x5e; *p++ = 0x5f;
			*p++ = 0x58;
			*p++ = 0xff; *p++ = 0x50; *p++ = 0x38;
			*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
			*p++ = 0xc3;
		}
		c = base + 0x1e34626;
		if (c[0] == 0xff && c[1] == 0x50 && c[2] == 0x20 && c[3] == 0x84 && c[4] == 0xc0) {
			unsigned char *s = p;
			void *nfn = note_vt20;
			rel = (int)(s - (c + 5));
			c[0] = 0xe8;
			memcpy(c + 1, &rel, 4);
			*p++ = 0x55; *p++ = 0x48; *p++ = 0x89; *p++ = 0xe5;
			*p++ = 0x48; *p++ = 0x83; *p++ = 0xe4; *p++ = 0xf0;
			*p++ = 0x50;
			*p++ = 0x57; *p++ = 0x56; *p++ = 0x52; *p++ = 0x51;
			*p++ = 0x41; *p++ = 0x50; *p++ = 0x41; *p++ = 0x51;
			*p++ = 0x48; *p++ = 0x83; *p++ = 0xec; *p++ = 0x08;
			*p++ = 0x48; *p++ = 0x8b; *p++ = 0x78; *p++ = 0x20;
			*p++ = 0x48; *p++ = 0xb8; memcpy(p, &nfn, 8); p += 8;
			*p++ = 0xff; *p++ = 0xd0;
			*p++ = 0x48; *p++ = 0x83; *p++ = 0xc4; *p++ = 0x08;
			*p++ = 0x41; *p++ = 0x59; *p++ = 0x41; *p++ = 0x58;
			*p++ = 0x59; *p++ = 0x5a; *p++ = 0x5e; *p++ = 0x5f;
			*p++ = 0x58;
			*p++ = 0xff; *p++ = 0x50; *p++ = 0x20;
			*p++ = 0x48; *p++ = 0x89; *p++ = 0xec; *p++ = 0x5d;
			*p++ = 0xc3;
		}
	}
	fprintf(stderr, "clear branch logger armed\n");
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
		"mov %rcx, %rdi\n"
		"call _filter_ctx_attribs\n"
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
	char line[512];
	unsigned long start = 0, off = 0;
	unsigned char *fn;
	int32_t disp;
	int fd, n, used = 0;

	/* fopen is remapped to fopen$UNIX2003, which this flat namespace
	 * does not provide. Read the map with open. */
	fd = open("/proc/self/maps", 0);
	if (fd < 0)
		return;
	while ((n = read(fd, line + used, sizeof line - 1 - used)) > 0) {
		char *nl;
		used += n;
		line[used] = 0;
		while ((nl = strchr(line, '\n'))) {
			*nl = 0;
			if (strstr(line, "libEGL.so.1") && strstr(line, "r-xp"))
				sscanf(line, "%lx-%*x %*s %lx", &start, &off);
			used = (int)strlen(nl + 1);
			memmove(line, nl + 1, used);
			line[used] = 0;
			if (start)
				break;
		}
		if (start || used >= (int)sizeof line - 1)
			break;
	}
	close(fd);
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

/* Chromium resolves eglCreatePbufferFromClientBuffer from Mesa, which
 * returns EGL_BAD_ALLOC for an IOSurface. Point that export at our pbuffer. */
static void hook_mesa_iosurface_pbuffer(void)
{
	unsigned char *fn;
	void *mesa_egl;
	void *sym;

	load_mesa();
	mesa_egl = dlopen("/usr/lib/native/libEGL.dylib", RTLD_NOLOAD);
	sym = mesa_egl ? dlsym(mesa_egl, "eglCreatePbufferFromClientBuffer") : NULL;
	if (!sym || sym == (void *)eglCreatePbufferFromClientBuffer) {
		fprintf(stderr, "mesa pbuffer hook missing sym=%p\n", sym);
		return;
	}
	fn = sym;
	if (prot_page((void *)((uintptr_t)fn & ~(uintptr_t)0xfff)) != 0) {
		fprintf(stderr, "mesa pbuffer mprotect failed %p\n", fn);
		return;
	}
	/*
	 * Chromium tail-calls this export, so RSP is 16-byte aligned on
	 * entry. A bare jmp into the C function makes every call it makes
	 * land in dyld_stub_binder's stack_not_16_byte_aligned_error.
	 * Align, call, and return.
	 *
	 *   push rbp; mov rbp, rsp; and rsp, -16
	 *   movabs rax, hook; call rax
	 *   mov rsp, rbp; pop rbp; ret
	 */
	{
		void *hook = (void *)eglCreatePbufferFromClientBuffer;
		unsigned char tramp[] = {
			0x55,
			0x48, 0x89, 0xe5,
			0x48, 0x83, 0xe4, 0xf0,
			0x48, 0xb8,
			0, 0, 0, 0, 0, 0, 0, 0,
			0xff, 0xd0,
			0x48, 0x89, 0xec,
			0x5d,
			0xc3
		};
		memcpy(tramp + 10, &hook, sizeof hook);
		memcpy(fn, tramp, sizeof tramp);
	}
	fprintf(stderr, "mesa eglCreatePbufferFromClientBuffer hooked at %p\n", fn);
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

/*
 * Chromium draws the compositor frame into an IOSurface-backed pbuffer
 * (EGL_IOSURFACE_ANGLE). Mesa has no such buffer. A plain pbuffer of the
 * same size lets Skia initialize; the pixels are copied into the
 * IOSurface on flush so X11CopyLayerImage can see them.
 */
#define IOSURF_PB_MAX 8
static struct {
	EGLSurface surface;
	void *iosurface;
	int w, h;
} g_iosurf_pb[IOSURF_PB_MAX];

static void *io_sym(const char *name)
{
	static void *lib;
	if (!lib)
		lib = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_LAZY);
	return lib ? dlsym(lib, name) : NULL;
}

static int iosurf_index(EGLSurface s)
{
	int i;
	for (i = 0; i < IOSURF_PB_MAX; i++) {
		if (g_iosurf_pb[i].surface == s)
			return i;
	}
	return -1;
}

static void copy_pb_to_iosurface(EGLSurface s)
{
	int i = iosurf_index(s);
	typedef void *(*base_fn)(void *);
	typedef size_t (*sz_fn)(void *);
	typedef void (*read_fn)(int, int, int, int, unsigned, unsigned, void *);
	typedef void (*store_fn)(unsigned, int);
	base_fn base;
	sz_fn bytes_per_row;
	read_fn read_pixels;
	store_fn pixel_store;
	void *dst;
	size_t stride;
	int w, h, y;
	static uint8_t *tmp;
	static size_t tmp_bytes;
	static int copies;
	size_t need;
	uint32_t first;

	if (i < 0)
		return;
	base = io_sym("IOSurfaceGetBaseAddress");
	bytes_per_row = io_sym("IOSurfaceGetBytesPerRow");
	if (!base || !bytes_per_row || !mesa_proc)
		return;
	dst = base(g_iosurf_pb[i].iosurface);
	stride = bytes_per_row(g_iosurf_pb[i].iosurface);
	w = g_iosurf_pb[i].w;
	h = g_iosurf_pb[i].h;
	if (!dst || w < 1 || h < 1 || stride < (size_t)w * 4)
		return;
	need = (size_t)w * (size_t)h * 4;
	if (tmp_bytes < need) {
		uint8_t *n = realloc(tmp, need);
		if (!n)
			return;
		tmp = n;
		tmp_bytes = need;
	}
	read_pixels = (read_fn)mesa_proc("glReadPixels");
	pixel_store = (store_fn)mesa_proc("glPixelStorei");
	if (!read_pixels)
		return;
	if (pixel_store)
		pixel_store(GL_PACK_ALIGNMENT, 4);
	read_pixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, tmp);
	for (y = 0; y < h; y++)
		memcpy((uint8_t *)dst + (size_t)(h - 1 - y) * stride,
			tmp + (size_t)y * (size_t)w * 4, (size_t)w * 4);
	first = *(uint32_t *)dst;
	if (copies < 4) {
		copies++;
		fprintf(stderr, "IOSurface pbuffer copy %p %dx%d first %08x\n",
			s, w, h, first);
	}
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
		void *buffer, EGLConfig config, const EGLint *attrib)
{
	EGLSurface (*fn)(EGLDisplay, EGLConfig, const EGLint *) = M("eglCreatePbufferSurface");
	EGLint pb[8];
	int w = 0, h = 0, i;
	EGLSurface s;
	typedef size_t (*sz_fn)(void *);

	if (attrib) {
		for (i = 0; attrib[i] != EGL_NONE && i < 64; i += 2) {
			if (attrib[i] == EGL_WIDTH)
				w = attrib[i + 1];
			else if (attrib[i] == EGL_HEIGHT)
				h = attrib[i + 1];
		}
	}
	if ((w < 1 || h < 1) && buffer) {
		sz_fn width = io_sym("IOSurfaceGetWidth");
		sz_fn height = io_sym("IOSurfaceGetHeight");
		if (width && height) {
			w = (int)width(buffer);
			h = (int)height(buffer);
		}
	}
	fprintf(stderr, "eglCreatePbufferFromClientBuffer type=0x%x buffer=%p %dx%d\n",
		buftype, buffer, w, h);
	if (buftype != EGL_IOSURFACE_ANGLE || w < 1 || h < 1 || !fn)
		return EGL_NO_SURFACE;
	pb[0] = EGL_WIDTH;
	pb[1] = w;
	pb[2] = EGL_HEIGHT;
	pb[3] = h;
	pb[4] = EGL_NONE;
	s = fn(dpy, config, pb);
	fprintf(stderr, "eglCreatePbufferFromClientBuffer surface=%p eglError=0x%x\n",
		s, mesa_error());
	if (!s)
		return EGL_NO_SURFACE;
	for (i = 0; i < IOSURF_PB_MAX; i++) {
		if (g_iosurf_pb[i].surface == NULL || g_iosurf_pb[i].surface == s) {
			g_iosurf_pb[i].surface = s;
			g_iosurf_pb[i].iosurface = buffer;
			g_iosurf_pb[i].w = w;
			g_iosurf_pb[i].h = h;
			break;
		}
	}
	return s;
}

static void shim_glFlush(void)
{
	typedef void (*fn_t)(void);
	typedef EGLSurface (*cur_fn)(EGLint);
	fn_t real = mesa_proc ? (fn_t)mesa_proc("glFlush") : NULL;
	cur_fn current = M("eglGetCurrentSurface");
	if (real)
		real();
	if (current)
		copy_pb_to_iosurface(current(EGL_DRAW));
}

static void shim_glFinish(void)
{
	typedef void (*fn_t)(void);
	typedef EGLSurface (*cur_fn)(EGLint);
	fn_t real = mesa_proc ? (fn_t)mesa_proc("glFinish") : NULL;
	cur_fn current = M("eglGetCurrentSurface");
	if (real)
		real();
	if (current)
		copy_pb_to_iosurface(current(EGL_DRAW));
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
	EGLBoolean ok = fn ? fn(dpy, s) : EGL_FALSE;
	copy_pb_to_iosurface(s);
	return ok;
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
		cache_mesa_extensions();
	}
}

/* Skia on GLES 3 reads extensions with glGetStringi, after ANGLE has
 * replaced the current context. Serve the list captured while the Mesa
 * context was current, so BGRA8 gets FormatInfo::kTexturable_Flag. */
static const char **g_exts;
static int g_ext_count;
static const char *g_version;
static const char *g_vendor;
static const char *g_renderer;

static void cache_mesa_extensions(void)
{
	typedef const unsigned char *(*fn_t)(unsigned);
	fn_t gs;
	const char *s;
	char *copy, *p;
	int n = 0, i, has = 0;

	if (g_exts || !mesa_proc)
		return;
	gs = (fn_t)mesa_proc("glGetString");
	if (!gs)
		return;
	s = (const char *)gs(0x1F03);
	if (!s || !s[0])
		return;
	copy = malloc(strlen(s) + 1);
	if (!copy)
		return;
	memcpy(copy, s, strlen(s) + 1);
	for (p = copy; *p; p++)
		if (*p == ' ')
			n++;
	g_exts = calloc((size_t)n + 8, sizeof *g_exts);
	if (!g_exts)
		return;
	i = 0;
	p = copy;
	while (p && *p) {
		char *sp = strchr(p, ' ');
		if (sp)
			*sp = 0;
		if (*p) {
			if (!strcmp(p, "GL_EXT_texture_format_BGRA8888"))
				has = 1;
			g_exts[i++] = p;
		}
		p = sp ? sp + 1 : 0;
	}
	{
		int k;
		static const char *add[] = {
			"GL_ARB_texture_rectangle",
			"GL_ANGLE_texture_rectangle",
		};
		for (k = 0; k < 2; k++) {
			int j, seen = 0;
			for (j = 0; j < i; j++)
				if (!strcmp(g_exts[j], add[k]))
					seen = 1;
			if (!seen) {
				memmove(g_exts + 1, g_exts, (size_t)i * sizeof *g_exts);
				g_exts[0] = add[k];
				i++;
			}
		}
	}
	g_ext_count = i;
	s = (const char *)gs(0x1F02);
	g_version = s ? strdup(s) : 0;
	s = (const char *)gs(0x1F00);
	g_vendor = s ? strdup(s) : 0;
	s = (const char *)gs(0x1F01);
	g_renderer = s ? strdup(s) : 0;
	fprintf(stderr, "mesa extension cache %d bgra=%s version=%s\n",
		i, has ? "yes" : "no", g_version ? g_version : "(null)");
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
	if (bgra8(fmt) || fmt == 0x80E1) {
		if (fmt == 0x93A1) {
			static int told;
			if (told < 4) {
				fprintf(stderr, "shim internalformat 0x93A1 pid=%d gpu=%d pname=%#x\n",
					getpid(), proc_is_gpu(), pname);
				told++;
			}
		}
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

static const unsigned char *(*angle_get_string)(unsigned);

static const unsigned char *shim_glGetString(unsigned name)
{
	typedef const unsigned char *(*fn_t)(unsigned);
	fn_t mesa = NULL;
	const unsigned char *s;
	static int logged;
	static char *built;
	static volatile int hook_lock;

	/* Not during the constructor: that glGetString is still building the
	 * extension cache, and a second thread was patching the same sites. */
	if (shim_ready && proc_is_gpu() &&
	    __sync_bool_compare_and_swap(&hook_lock, 0, 1)) {
		log_rect_zero();
		log_rect_store();
		log_clear_branch();
	}
	if (mesa_proc)
		mesa = (fn_t)mesa_proc("glGetString");
	if (mesa)
		s = mesa(name);
	if (!s && angle_get_string)
		s = angle_get_string(name);
	if (!s && name == 0x1F02)
		s = (const unsigned char *)g_version;
	if (!s && name == 0x1F00)
		s = (const unsigned char *)g_vendor;
	if (!s && name == 0x1F01)
		s = (const unsigned char *)g_renderer;
	if (name != 0x1F03)
		return s;
	if (!logged) {
		int has = s && strstr((const char *)s, "GL_EXT_texture_format_BGRA8888");
		fprintf(stderr, "shim glGetString GL_EXTENSIONS pid=%d gpu=%d mesa=%s bgra=%s\n",
			getpid(), proc_is_gpu(), s ? "present" : "null", has ? "yes" : "no");
		logged = 1;
	}
	/* GrGLExtensions::init splits this string at 0x81abe30 and sorts it.
	 * The glGetStringi cache is a different list and is not what
	 * hasExtension at 0x3b27eb0 searches. */
	if (!built) {
		const char *extra[3];
		int ne = 0, e;
		size_t n = s ? strlen((const char *)s) : 0;
		size_t add = 0;
		if (!s || !strstr((const char *)s, "GL_EXT_texture_format_BGRA8888"))
			extra[ne++] = "GL_EXT_texture_format_BGRA8888";
		if (!s || !strstr((const char *)s, "GL_ARB_texture_rectangle"))
			extra[ne++] = "GL_ARB_texture_rectangle";
		if (!s || !strstr((const char *)s, "GL_ANGLE_texture_rectangle"))
			extra[ne++] = "GL_ANGLE_texture_rectangle";
		if (!ne)
			return s;
		for (e = 0; e < ne; e++)
			add += strlen(extra[e]) + 1;
		built = malloc(n + add + 1);
		if (!built)
			return s;
		if (s)
			memcpy(built, s, n);
		for (e = 0; e < ne; e++) {
			if (n && built[n - 1] != ' ')
				built[n++] = ' ';
			memcpy(built + n, extra[e], strlen(extra[e]));
			n += strlen(extra[e]);
		}
		built[n] = 0;
		fprintf(stderr, "shim extensions string added rectangle\n");
	}
	if (logged == 1) {
		fprintf(stderr, "shim advertised GL_EXT_texture_format_BGRA8888\n");
		logged = 2;
	}
	return (const unsigned char *)built;
}

static const unsigned char *shim_glGetStringi(unsigned name, unsigned index)
{
	typedef const unsigned char *(*fn_t)(unsigned, unsigned);
	static fn_t real;
	if (name == 0x1F03 && g_exts && index < (unsigned)g_ext_count) {
		static int saw_rect;
		if (!saw_rect && g_exts[index] &&
		    strstr(g_exts[index], "texture_rectangle")) {
			fprintf(stderr, "getstringi rectangle idx %u %s\n",
				index, g_exts[index]);
			saw_rect = 1;
		}
		return (const unsigned char *)g_exts[index];
	}
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glGetStringi");
	return real ? real(name, index) : 0;
}

void shim_glGetIntegerv(unsigned pname, int *params)
{
	typedef void (*fn_t)(unsigned, int *);
	static fn_t real;
	if (!real && mesa_proc)
		real = (fn_t)mesa_proc("glGetIntegerv");
	if (real)
		real(pname, params);
	if (pname == 0x821D && params && g_ext_count > 0)
		params[0] = g_ext_count;
}

static void *shim_gl(const char *name)
{
	if (!strcmp(name, "glGetInternalformativ"))
		return (void *)shim_glGetInternalformativ;
	if (!strcmp(name, "glGetString"))
		return (void *)shim_glGetString;
	if (!strcmp(name, "glGetStringi"))
		return (void *)shim_glGetStringi;
	if (!strcmp(name, "glGetIntegerv"))
		return (void *)shim_glGetIntegerv;
	if (!strcmp(name, "glRenderbufferStorage"))
		return (void *)shim_glRenderbufferStorage;
	if (!strcmp(name, "glRenderbufferStorageMultisample"))
		return (void *)shim_glRenderbufferStorageMultisample;
	if (!strcmp(name, "glTexImage2D"))
		return (void *)shim_glTexImage2D;
	if (!strcmp(name, "glTexStorage2D"))
		return (void *)shim_glTexStorage2D;
	if (!strcmp(name, "glFlush"))
		return (void *)shim_glFlush;
	if (!strcmp(name, "glFinish"))
		return (void *)shim_glFinish;
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
		if (!strcmp(k_gl_names[i], "glGetStringi") && stub[5] == 0xe9) {
			int rel;
			unsigned char *inner;
			uintptr_t ipage;
			memcpy(&rel, stub + 6, 4);
			inner = stub + 10 + rel;
			ipage = (uintptr_t)inner & ~(uintptr_t)0xfff;
			if (inner[0] == 0x55 && inner[1] == 0x48 && mp &&
			    mp((void *)ipage, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
				inner[0] = 0x48;
				inner[1] = 0xb8;
				memcpy(inner + 2, &fn, sizeof fn);
				inner[10] = 0xff;
				inner[11] = 0xe0;
				fprintf(stderr, "angle inner glGetStringi patched %p\n", inner);
			}
		}
		if (!strcmp(k_gl_names[i], "glGetString") && stub[5] == 0xe9) {
			int rel, crel;
			unsigned char *inner, *t, *target, *cont, *p;
			uintptr_t ipage;
			memcpy(&rel, stub + 6, 4);
			inner = stub + 10 + rel;
			ipage = (uintptr_t)inner & ~(uintptr_t)0xfff;
			if (!mp || mp((void *)ipage, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
				fprintf(stderr, "angle inner mprotect failed\n");
			} else if (inner[9] == 0xe8) {
				t = mmap(0, 4096, 7, 0x1002, -1, 0);
				if (t && t != (unsigned char *)-1) {
					static const unsigned char pro[] = {
						0x55, 0x48, 0x89, 0xe5, 0x41, 0x56, 0x53, 0x89, 0xfb
					};
					memcpy(&crel, inner + 10, 4);
					target = inner + 14 + crel;
					cont = inner + 14;
					p = t;
					memcpy(p, pro, sizeof pro);
					p += sizeof pro;
					p[0] = 0x49; p[1] = 0xbb;
					memcpy(p + 2, &target, 8);
					p[10] = 0x41; p[11] = 0xff; p[12] = 0xd3;
					p += 13;
					p[0] = 0x49; p[1] = 0xbb;
					memcpy(p + 2, &cont, 8);
					p[10] = 0x41; p[11] = 0xff; p[12] = 0xe3;
					angle_get_string = (const unsigned char *(*)(unsigned))t;
					fprintf(stderr, "angle glGetString trampoline %p target %p cont %p\n",
						t, target, cont);
				}
				inner[0] = 0x48;
				inner[1] = 0xb8;
				memcpy(inner + 2, &fn, sizeof fn);
				inner[10] = 0xff;
				inner[11] = 0xe0;
				inner[12] = 0x90;
				inner[13] = 0x90;
				fprintf(stderr, "angle inner glGetString patched\n");
			}
		}
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
		if (!strcmp(name, "eglCreatePbufferFromClientBuffer"))
			return (void (*)(void))eglCreatePbufferFromClientBuffer;
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
	/*
	 * Mesa's eglGetProcAddress strstr()s the name. A NULL name is the
	 * needle that killed VizCompositorThread in ld-linux. EGL returns
	 * NULL for a NULL name.
	 */
	if (!name || !mesa_proc)
		return NULL;
	return (void (*)(void))mesa_proc(name);
}
