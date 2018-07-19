/*
 * Handle resume from suspend-to-disk
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "kinit.h"
#include "do_mounts.h"
#include "resume.h"

#ifndef CONFIG_PM_STD_PARTITION
# define CONFIG_PM_STD_PARTITION ""
#endif

int do_resume(int argc, char *argv[])
{
	const char *resume_file = CONFIG_PM_STD_PARTITION;
	const char *resume_arg;
	unsigned long long resume_offset;

	resume_arg = get_arg(argc, argv, "resume=");
	resume_file = resume_arg ? resume_arg : resume_file;
	/* No resume device specified */
	if (!resume_file[0])
		return 0;

	resume_arg = get_arg(argc, argv, "resume_offset=");
	resume_offset = resume_arg ? strtoull(resume_arg, NULL, 0) : 0ULL;

	/* Fix: we either should consider reverting the device back to
	   ordinary swap, or (better) put that code into swapon */
	/* Noresume requested */
	if (get_flag(argc, argv, "noresume"))
		return 0;
	return resume(resume_file, resume_offset);
}

/*
 * Get the default resume_offset set on the kernel command line.
 * Return 0 (built-in default) if it is not set, or -1 on failure.
 */
static unsigned long long default_resume_offset(void)
{
	static const char str_hibernate_noresume[] = "hibernate=noresume";
	static const char str_resume_offset[] = "resume_offset=";
	unsigned long long offset = -1;
	/*
	 * Max length of the kernel command line is arch-dependent,
	 * but currently no more than 4K.
	 */
	char buf[4096], *param;
	ssize_t len;
	int fd;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0)
		goto out;

	len = read(fd, buf, sizeof buf - 1);
	if (len < 0)
		goto out;
	buf[len] = 0;

	offset = 0;
	param = buf;
	for (;;) {
		/* Skip white space and check for end of string */
		param += strspn(param, " \t\r\n");
		if (!*param)
			break;

		/* Get param length */
		len = strcspn(param, " \t\r\n");

		/*
		 * Check for hibernate=(noresume|no) which inhibits
		 * parsing of the resume_offset parameter
		 */
		if ((len == sizeof str_hibernate_noresume - 1 ||
		     len == sizeof str_hibernate_noresume - 1 - 6) &&
		    strncmp(param, str_hibernate_noresume, len) == 0) {
			offset = 0;
			break;
		}

		/* Check for resume_offset=... */
		if (strncmp(param, str_resume_offset,
			    sizeof str_resume_offset - 1) == 0)
			sscanf(param + sizeof str_resume_offset - 1,
			       "%llu", &offset);

		/* Advance over param */
		param += len;
	}

out:
	if (fd >= 0)
		close(fd);
	return offset;
}

int resume(const char *resume_file, unsigned long long resume_offset)
{
	dev_t resume_device;
	int attr_fd = -1;
	char attr_value[64];
	int len;

	resume_device = name_to_dev_t(resume_file);

	if (major(resume_device) == 0) {
		fprintf(stderr, "Invalid resume device: %s\n", resume_file);
		goto failure;
	}

	if ((attr_fd = open("/sys/power/resume_offset", O_WRONLY)) < 0) {
		if (errno == ENOENT) {
			/*
			 * We can't change the offset, but maybe we don't
			 * need to.  In that case, continue.
			 */
			unsigned long long default_offset =
				default_resume_offset();

			if (default_offset != (unsigned long long)(-1) &&
			    default_offset == resume_offset)
				goto skip_offset;
		}
		goto fail_offset;
	}

	len = snprintf(attr_value, sizeof attr_value,
		       "%llu",
		       resume_offset);

	/* This should never happen */
	if (len >= sizeof attr_value)
		goto fail_offset;

	if (write(attr_fd, attr_value, len) != len)
		goto fail_offset;

	close(attr_fd);

skip_offset:
	if ((attr_fd = open("/sys/power/resume", O_WRONLY)) < 0)
		goto fail_r;

	len = snprintf(attr_value, sizeof attr_value,
		       "%u:%u",
		       major(resume_device), minor(resume_device));

	/* This should never happen */
	if (len >= sizeof attr_value)
		goto fail_r;

	dprintf("kinit: trying to resume from %s\n", resume_file);

	if (write(attr_fd, attr_value, len) != len)
		goto fail_r;

	/* Okay, what are we still doing alive... */
failure:
	if (attr_fd >= 0)
		close(attr_fd);
	dprintf("kinit: No resume image, doing normal boot...\n");
	return -1;

fail_offset:
	fprintf(stderr, "Cannot write /sys/power/resume_offset "
			"(no software suspend kernel support, or old kernel version?)\n");
	goto failure;

fail_r:
	fprintf(stderr, "Cannot write /sys/power/resume "
			"(no software suspend kernel support?)\n");
	goto failure;
}
