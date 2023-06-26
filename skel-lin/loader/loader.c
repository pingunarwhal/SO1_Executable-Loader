/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "exec_parser.h"

static so_exec_t *exec;

// used for accessing old handler
static struct sigaction old;
static void *old_seg;

// file descriptor
static int fd;

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	/* TODO - actual loader implementation */

	// address at which segfault occured
	void *segfault = info->si_addr;
	if (segfault == old_seg) {
		old.sa_sigaction(signum, info, context);
		return;
	}

	// iterating through program segments until the problematic one is found
	int segm_count = 0;
	
	while (segm_count < exec->segments_no) {

		// memorizing the current segment in a separate variable
		so_seg_t its_me = exec->segments[segm_count];

		if ((uintptr_t)segfault >= its_me.vaddr && (uintptr_t)segfault < its_me.vaddr + its_me.mem_size) {

			// calculating page index
			int page_idx = ((uintptr_t)segfault - its_me.vaddr) / getpagesize();

			// address of page beginning where segfault occured
			uintptr_t alloc_page = its_me.vaddr + getpagesize() * page_idx;

			// address of allocated page
			void *place = mmap((void *)alloc_page, getpagesize(), PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANON, fd, 0);

			// calculating the page offset in the file
			int offset = its_me.offset + page_idx * getpagesize();
			
			// verifying that the page is not in bss
			if (its_me.file_size > getpagesize() * page_idx) {

				// setting the cursor
				lseek(fd, offset, SEEK_SET);

				// verifying if the whole page should be copied
				if (its_me.file_size >= getpagesize() * (page_idx + 1)) {

					// copying the entire page
					read(fd, place, getpagesize());

				} else {

					//copying only a part of the page
					read(fd, place, its_me.file_size - getpagesize() * page_idx);
					
				}

			}

			mprotect(place, getpagesize(), its_me.perm);

			old_seg = segfault;
			return;
		}

		segm_count++;
	}

	old.sa_sigaction(signum, info, context);
	
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, &old);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	// opening each file
	fd = open(path, O_RDONLY);

	so_start_exec(exec, argv);

	return -1;
}
