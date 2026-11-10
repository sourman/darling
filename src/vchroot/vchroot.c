#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

extern int lkm_call(int nr, ...);
extern int __darling_vchroot(int dfd);
extern void __darling_vchroot_force_root(void);

int main(int argc, const char** argv)
{
    if (argc < 3)
	{
		fprintf(stderr, "vchroot <dir> <binary> [args...]\n");
		return 1;
	}

	char buf[4096];
	sprintf(buf, "%s%s", argv[1], argv[2]);

	if (access(buf, F_OK) != 0)
	{
		fprintf(stderr, "Target executable not found: %s\n", buf);
		return 5;
	}

	int dfd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (dfd == -1)
	{
		perror("open");
		return 1;
	}

	if (fchdir(dfd) == -1)
	{
		perror("fchdir");
		return 2;
	}

	if (__darling_vchroot(dfd) < 0)
	{
		perror("vchroot");
		return 3;
	}

	/* Already chrooted into the prefix: argv[2] exists at the chroot path.
	   The fd readlink is the host prefix and must not be prepended again. */
	if (access(argv[2], F_OK) == 0)
		__darling_vchroot_force_root();

	close(dfd);

	// This is only needed for this binary and shouldn't be passed down
	unsetenv("DYLD_ROOT_PATH");

	// printf("Will execv %s\n", argv[2]);
	execv(argv[2], (char * const *) argv+2);
	perror("execv");

	return 4;
}

