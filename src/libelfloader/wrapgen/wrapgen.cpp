
#include <string>
#include <iostream>
#include <set>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <link.h>

#ifndef PATH_MAX
#	define PATH_MAX	4096
#endif

// This is a generator of assembly code for Mach-O
// libraries wrapping ELF libraries.

// TODO: use wrapgen32 to generate 32-bit wrappers.

void parse_elf(const char* elf, std::string& soname_out, std::set<std::string>& functions_out, std::set<std::string>& vars_out);
void generate_wrapper(std::ofstream& output, const char* soname, const std::set<std::string>& symbols);
void generate_var_wrappers(std::ofstream& output, std::ofstream& outputHeader, const std::set<std::string>& vars);
Elf64_Off vaddr_to_offset(const Elf64_Ehdr* ehdr, Elf64_Addr vaddr);

int main(int argc, const char** argv)
{
	std::string elfLibrary;
	std::set<std::string> functions, vars;
	std::string soname;
	std::ofstream output;

	if (argc != 4)
	{
		std::cerr << "Usage: " << argv[0] << " <library-name> <output-file> <var-access-header>\n";
		return 1;
	}

	elfLibrary = argv[1];
	output.open(argv[2]);

	try
	{
		if (!output.is_open())
			throw std::runtime_error("Cannot open output file");

		if (access(elfLibrary.c_str(), R_OK) == -1)
		{
			// Try loading the library and then ask the loader where it found the library.
			// It is simpler than parsing /etc/ld.so.conf.

			void* handle = dlopen(elfLibrary.c_str(), RTLD_LAZY | RTLD_LOCAL);
			struct link_map* lm = NULL;

			if (!handle)
			{
				std::stringstream ss;
				ss << "Cannot load " << elfLibrary << ": " << dlerror();
				throw std::runtime_error(ss.str());
			}

			if (dlinfo(handle, RTLD_DI_LINKMAP, &lm) == 0)
			{
				elfLibrary = lm->l_name;
			}
			else
			{
				std::stringstream ss;
				std::cerr << "Cannot locate " << elfLibrary << ": " << dlerror();
				throw std::runtime_error(ss.str());
			}

			dlclose(handle);
		}

		parse_elf(elfLibrary.c_str(), soname, functions, vars);
		generate_wrapper(output, soname.c_str(), functions);

		if (!vars.empty())
		{
			std::ofstream outputHeader(argv[3]);
			if (!outputHeader.is_open())
				throw std::runtime_error("Cannot open output macro header file");

			generate_var_wrappers(output, outputHeader, vars);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}

void parse_elf(const char* elf, std::string& soname, std::set<std::string>& symbols, std::set<std::string>& vars)
{
	int fd;
	const Elf64_Ehdr* ehdr;
	const char* strings = NULL;
	uint64_t length;

	fd = open(elf, O_RDONLY);
	if (fd < 0)
	{
		std::stringstream ss;
		ss << "Error opening " << elf << ": " << strerror(errno);
		throw std::runtime_error(ss.str());
	}

	length = lseek(fd, 0, SEEK_END);

	ehdr = (Elf64_Ehdr*) mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
	if (ehdr == MAP_FAILED)
	{
		std::stringstream ss;
		ss << "Cannot mmap ELF file: " << strerror(errno);
		throw std::runtime_error(ss.str());
	}

	close(fd);

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 || ehdr->e_ident[EI_CLASS] != ELFCLASS64)
	{
		std::stringstream ss;
		ss << elf << " is not a 64-bit ELF";
		throw std::runtime_error(ss.str());
	}

	if (ehdr->e_type != ET_DYN)
	{
		std::stringstream ss;
		ss << elf << " is not a dynamic library ELF";
		throw std::runtime_error(ss.str());
	}

	if (ehdr->e_machine != EM_X86_64)
	{
		std::stringstream ss;
		ss << elf << " is not an ELF for x86-64";
		throw std::runtime_error(ss.str());
	}

	// Now read program headers, find a PT_DYNAMIC segment type
	for (int i = 0; i < ehdr->e_phnum; i++)
	{
		const Elf64_Phdr* phdr;

		phdr = (const Elf64_Phdr*) (((char*) ehdr) + ehdr->e_phoff + (i * ehdr->e_phentsize));

		if (phdr->p_type == PT_DYNAMIC)
		{
			Elf64_Xword off_strtab, off_soname;

			off_strtab = off_soname = 0;

			for (int j = 0; j < phdr->p_filesz; j += sizeof(Elf64_Dyn))
			{
				const Elf64_Dyn* dyn;

				dyn = (const Elf64_Dyn*) (((char*) ehdr) + phdr->p_offset + j);
				
				switch (dyn->d_tag)
				{
					case DT_STRTAB:
						off_strtab = vaddr_to_offset(ehdr, dyn->d_un.d_val);
						break;
					case DT_SONAME:
						off_soname = vaddr_to_offset(ehdr, dyn->d_un.d_val);
						break;
					case DT_NULL:
						goto end_dyn;
				}
			}
end_dyn:

			if (off_strtab != 0)
			{
				strings = (const char*) ((char*) ehdr) + off_strtab;

				if (off_soname != 0)
				{
					// Read DT_SONAME value from the string table
					soname = strings + off_soname;
				}
			}

			break;
		}
	}

	if (strings != NULL)
	{
		// Load symbol list
		const Elf64_Shdr* shdr;

		for (int i = 0; i < ehdr->e_shnum; i++)
		{
			shdr = (const Elf64_Shdr*) (((char*) ehdr) + ehdr->e_shoff + (i * ehdr->e_shentsize));
			if (shdr->sh_type == SHT_DYNSYM)
			{
				for (int j = 0; j < shdr->sh_size; j += sizeof(Elf64_Sym))
				{
					const Elf64_Sym* sym;

					sym = (const Elf64_Sym*) (((char*) ehdr) + shdr->sh_offset + j);

					if (ELF64_ST_TYPE(sym->st_info) != STT_OBJECT && ELF64_ST_TYPE(sym->st_info) != STT_FUNC)
						continue;
					if (ELF64_ST_BIND(sym->st_info) != STB_GLOBAL)
						continue;
					if (sym->st_shndx == SHN_UNDEF || sym->st_value == 0)
						continue;
					if (ELF64_ST_VISIBILITY(sym->st_other) != STV_DEFAULT)
						continue;

					if (ELF64_ST_TYPE(sym->st_info) == STT_FUNC)
						symbols.insert(strings + sym->st_name);
					else
						vars.insert(strings + sym->st_name);
				}

				break;
			}
		}
	}

	if (soname.empty())
	{
		std::cerr << "WARNING: No DT_SONAME in " << elf << ".\n";

		const char* slash = strrchr(elf, '/');
		if (slash != NULL)
			soname = slash + 1;
		else
			soname = elf;
	}
	if (symbols.empty())
	{
		std::stringstream ss;
		ss << "No symbols found in " << elf;
		throw std::runtime_error(ss.str());
	}

	munmap((void*) ehdr, length);
}

void generate_wrapper(std::ofstream& output, const char* soname, const std::set<std::string>& symbols)
{
	const bool is_gl = symbols.count("glGetString") > 0;

	output << "#include <elfcalls.h>\n";
	if (is_gl)
		output << "#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n";
	output << "extern struct elf_calls* _elfcalls;\n\n"
		"extern const char __elfname[];\n\n";

	if (is_gl) {
		output <<
			"__attribute__((used)) const char gl_string_guard_v1[] = \"gl_string_guard_v1\";\n"
			"__attribute__((used)) const char gl_procaddr_string_v1[] = \"gl_procaddr_string_v1\";\n"
			"__attribute__((used)) const char gl_es_from_desktop_v1[] = \"gl_es_from_desktop_v1\";\n"
			"__attribute__((used)) const char gl_desktop_native_v1[] = \"gl_desktop_native_v1\";\n"
			"__attribute__((used)) const char gl_es_bgra_ext_v1[] = \"gl_es_bgra_ext_v1\";\n"
			"\n"
			"static int gl_string_ok(const char *s)\n"
			"{\n"
			"\tuintptr_t p = (uintptr_t)s;\n"
			"\tint i;\n"
			"\tif (!s || !s[0])\n"
			"\t\treturn 0;\n"
			"\t/* 0x501004-class: unslid Mach-O / non-C-string sentinel. */\n"
			"\tif (p < 0x1000000ul)\n"
			"\t\treturn 0;\n"
			"\tfor (i = 0; i < 4096; i++) {\n"
			"\t\tif (s[i] == '\\0')\n"
			"\t\t\treturn 1;\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n"
			"\n"
			"static const char *const gl_extra_exts[] = {\n"
			"\t\"GL_EXT_texture_format_BGRA8888\",\n"
			"\t\"GL_APPLE_texture_format_BGRA8888\",\n"
			"\t\"GL_EXT_bgra\",\n"
			"\t\"GL_ARB_texture_rectangle\",\n"
			"\t\"GL_EXT_texture_rectangle\",\n"
			"\t\"GL_APPLE_ycbcr_422\",\n"
			"\tNULL\n"
			"};\n"
			"\n"
			"static int gl_extra_ext_count(void)\n"
			"{\n"
			"\tint n = 0;\n"
			"\twhile (gl_extra_exts[n])\n"
			"\t\tn++;\n"
			"\treturn n;\n"
			"}\n"
			"\n"
			"static const unsigned char *gl_es_extensions(const char *src)\n"
			"{\n"
			"\tstatic char buf[16384];\n"
			"\tint i;\n"
			"\tsize_t len;\n"
			"\tif (!src)\n"
			"\t\tsrc = \"\";\n"
			"\tsnprintf(buf, sizeof(buf), \"%s\", src);\n"
			"\tfor (i = 0; gl_extra_exts[i]; i++) {\n"
			"\t\tif (strstr(buf, gl_extra_exts[i]))\n"
			"\t\t\tcontinue;\n"
			"\t\tlen = strlen(buf);\n"
			"\t\tif (len + 2 + strlen(gl_extra_exts[i]) >= sizeof(buf))\n"
			"\t\t\tbreak;\n"
			"\t\tif (len && buf[len - 1] != ' ')\n"
			"\t\t\tbuf[len++] = ' ', buf[len] = '\\0';\n"
			"\t\tstrncat(buf, gl_extra_exts[i], sizeof(buf) - strlen(buf) - 1);\n"
			"\t}\n"
			"\treturn (const unsigned char *)buf;\n"
			"}\n"
			"\n"
			"/* Empty/sentinel GL_VERSION only. A live desktop string must stay desktop so\n"
			"   ANGLE FunctionsGLCGL takes the GL-desktop backend (BGRA RT + TEXTURE_RECTANGLE).\n"
			"   Rewriting 4.5 Compatibility -> GLES 3.2 made Skia CHECK on 1x1 BGRA\n"
			"   skia_output_surface_impl.cc:1291 (VizBufferQueue Scanout). Chrome's\n"
			"   gl_version_info ES CHECK reads ANGLE's GLES frontend, not this native string. */\n"
			"static const unsigned char *gl_string_fallback(unsigned int name)\n"
			"{\n"
			"\tswitch (name) {\n"
			"\tcase 0x1F00: return (const unsigned char *)\"Mesa\";\n"
			"\tcase 0x1F01: return (const unsigned char *)\"llvmpipe (LLVM 18, 256 bits)\";\n"
			"\tcase 0x1F02: return (const unsigned char *)\"OpenGL ES 3.2 Mesa 24.2.6\";\n"
			"\tcase 0x1F03: return gl_es_extensions(\"GL_ARB_vertex_array_object GL_ARB_framebuffer_object GL_OES_rgb8_rgba8 GL_EXT_texture_format_BGRA8888 GL_APPLE_texture_format_BGRA8888 GL_EXT_bgra\");\n"
			"\tcase 0x8B8C: return (const unsigned char *)\"OpenGL ES GLSL ES 3.20\";\n"
			"\tdefault: return (const unsigned char *)\"\";\n"
			"\t}\n"
			"}\n"
			"\n"
			"const unsigned char* glGetString(unsigned int name);\n"
			"const unsigned char *glGetStringi(unsigned int name, unsigned int index);\n"
			"void glGetIntegerv(unsigned int pname, int *params);\n"
			"\n";
	}

	output << "static void* lib_handle;\n"
		"__attribute__((constructor)) static void initializer() {\n"
		"\tlib_handle = _elfcalls->dlopen_fatal(__elfname);\n"
		"}\n\n";

	output << "__attribute__((destructor)) static void destructor() {\n"
		"\t_elfcalls->dlclose_fatal(lib_handle);\n"
		"}\n\n";
	
	for (const std::string& sym : symbols)
	{
		if (is_gl && sym == "glGetString") {
			output <<
				"const unsigned char* glGetString(unsigned int name) {\n"
				"\tstatic const unsigned char* (*real)(unsigned int);\n"
				"\tconst unsigned char* s;\n"
				"\tif (!real)\n"
				"\t\treal = (const unsigned char*(*)(unsigned int))_elfcalls->dlsym_fatal(lib_handle, \"glGetString\");\n"
				"\ts = real(name);\n"
				"\tif (!gl_string_ok((const char *)s)) {\n"
				"\t\tfprintf(stderr, \"gl_string_guard_v1 name=0x%x ptr=%p -> fallback\\n\",\n"
				"\t\t\tname, (const void *)s);\n"
				"\t\tfflush(stderr);\n"
				"\t\ts = gl_string_fallback(name);\n"
				"\t}\n"
				"\tif (name == 0x1F02) {\n"
				"\t\tfprintf(stderr, \"gl_desktop_native_v1 GL_VERSION='%s'\\n\",\n"
				"\t\t\t(const char *)s);\n"
				"\t\tfflush(stderr);\n"
				"\t}\n"
				"\tif (name == 0x1F03)\n"
				"\t\treturn gl_es_extensions((const char *)s);\n"
				"\treturn s;\n"
				"}\n\n";
			continue;
		}
		if (is_gl && sym == "glGetStringi") {
			output <<
				"const unsigned char *glGetStringi(unsigned int name, unsigned int index)\n"
				"{\n"
				"\tstatic const unsigned char *(*real)(unsigned int, unsigned int);\n"
				"\tstatic void (*giv)(unsigned int, int *);\n"
				"\tstatic int native_n = -1;\n"
				"\tif (!real)\n"
				"\t\treal = (const unsigned char *(*)(unsigned int, unsigned int))\n"
				"\t\t\t_elfcalls->dlsym_fatal(lib_handle, \"glGetStringi\");\n"
				"\tif (name != 0x1F03)\n"
				"\t\treturn real(name, index);\n"
				"\tif (native_n < 0) {\n"
				"\t\tif (!giv)\n"
				"\t\t\tgiv = (void (*)(unsigned int, int *))\n"
				"\t\t\t\t_elfcalls->dlsym_fatal(lib_handle, \"glGetIntegerv\");\n"
				"\t\tnative_n = 0;\n"
				"\t\tgiv(0x821D, &native_n);\n"
				"\t}\n"
				"\tif ((int) index < native_n)\n"
				"\t\treturn real(name, index);\n"
				"\tindex -= (unsigned) native_n;\n"
				"\tif ((int) index < gl_extra_ext_count())\n"
				"\t\treturn (const unsigned char *) gl_extra_exts[index];\n"
				"\treturn NULL;\n"
				"}\n\n";
			continue;
		}
		if (is_gl && sym == "glGetIntegerv") {
			output <<
				"void glGetIntegerv(unsigned int pname, int *params)\n"
				"{\n"
				"\tstatic void (*real)(unsigned int, int *);\n"
				"\tif (!real)\n"
				"\t\treal = (void (*)(unsigned int, int *))\n"
				"\t\t\t_elfcalls->dlsym_fatal(lib_handle, \"glGetIntegerv\");\n"
				"\treal(pname, params);\n"
				"\tif (pname == 0x821D && params)\n"
				"\t\t*params += gl_extra_ext_count();\n"
				"}\n\n";
			continue;
		}
		if (is_gl && (sym == "glXGetProcAddress" || sym == "glXGetProcAddressARB")) {
			output <<
				"void* " << sym << "(const unsigned char *procName) {\n"
				"\tstatic void* (*real)(const unsigned char *);\n"
				"\tif (procName && strcmp((const char *)procName, \"glGetString\") == 0) {\n"
				"\t\tfprintf(stderr, \"gl_procaddr_string_v1 " << sym << " -> wrap\\n\");\n"
				"\t\tfflush(stderr);\n"
				"\t\treturn (void *)glGetString;\n"
				"\t}\n"
				"\tif (procName && strcmp((const char *)procName, \"glGetStringi\") == 0)\n"
				"\t\treturn (void *)glGetStringi;\n"
				"\tif (procName && strcmp((const char *)procName, \"glGetIntegerv\") == 0)\n"
				"\t\treturn (void *)glGetIntegerv;\n"
				"\tif (!real)\n"
				"\t\treal = (void*(*)(const unsigned char *))_elfcalls->dlsym_fatal(lib_handle, \"" << sym << "\");\n"
				"\treturn real(procName);\n"
				"}\n\n";
			continue;
		}
		output << "void* " << sym << "() {\n"
			"\t__asm__(\".symbol_resolver _" << sym << "\");\n"
			"\treturn _elfcalls->dlsym_fatal(lib_handle, \"" << sym << "\");\n"
			"}\n\n";
	}
	output << "asm(\".section __TEXT,__elfname\\n"
		".private_extern ___elfname\\n"
		"___elfname: .asciz \\\"" << soname << "\\\"\");\n";
}

void generate_var_wrappers(std::ofstream& output, std::ofstream& outputHeader, const std::set<std::string>& vars)
{
	outputHeader << "#pragma once\n\n";
	outputHeader << "#ifdef __cplusplus\n"
		"extern \"C\" {\n"
		"#endif\n\n";

	for (const std::string& sym : vars)
	{
		output << "void* __elf_get_" << sym << "(void) {\n"
			"\treturn _elfcalls->dlsym_fatal(lib_handle, \"" << sym << "\");\n"
			"}\n\n";
		
		outputHeader << "extern __typeof(" << sym << ")* __elf_get_" << sym << "(void);\n"
			"#define " << sym << " (*__elf_get_" << sym << "())\n\n";
	}

	outputHeader << "\n\n#ifdef __cplusplus\n"
		"}\n"
		"#endif\n\n";
}

Elf64_Off vaddr_to_offset(const Elf64_Ehdr* ehdr, Elf64_Xword vaddr)
{
	for (int i = 0; i < ehdr->e_phnum; i++)
	{
		const Elf64_Phdr* phdr;
		Elf64_Addr vstart, vend;

		phdr = (const Elf64_Phdr*) (((char*) ehdr) + ehdr->e_phoff + (i * ehdr->e_phentsize));
		vstart = phdr->p_vaddr;
		vend = phdr->p_vaddr + phdr->p_memsz;
		if (vstart <= vaddr && vaddr < vend) {
			return phdr->p_offset + vaddr - vstart;
		}
	}

	return 0;
}
