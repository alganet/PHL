/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Newlib syscall stubs for FreeRTOS on Cortex-M3.
 * _write uses ARM semihosting so printf output goes to QEMU's stdout.
 */
#include <sys/stat.h>
#include <errno.h>

/* ARM semihosting operations */
#define SYS_WRITE0  0x04
#define SYS_WRITEC  0x03

static inline int semihosting_call(int op, void *arg)
{
	int result;
	__asm__ volatile (
		"mov r0, %[op]\n"
		"mov r1, %[arg]\n"
		"bkpt 0xab\n"
		"mov %[result], r0\n"
		: [result] "=r" (result)
		: [op] "r" (op), [arg] "r" (arg)
		: "r0", "r1", "memory"
	);
	return result;
}

int _write(int fd, const char *buf, int len)
{
	(void)fd;
	/* Write character by character via semihosting SYS_WRITEC */
	for( int i = 0; i < len; i++ ){
		semihosting_call(SYS_WRITEC, (void *)&buf[i]);
	}
	return len;
}

int _read(int fd, char *buf, int len)
{
	(void)fd; (void)buf; (void)len;
	errno = ENOTSUP;
	return -1;
}

int _close(int fd)
{
	(void)fd;
	return -1;
}

int _lseek(int fd, int offset, int whence)
{
	(void)fd; (void)offset; (void)whence;
	return 0;
}

int _fstat(int fd, struct stat *st)
{
	(void)fd;
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int fd)
{
	(void)fd;
	return 1;
}

void *_sbrk(int incr)
{
	extern char _ebss;
	static char *heap_end = 0;
	char *prev_heap_end;

	if( heap_end == 0 ){
		heap_end = &_ebss;
	}
	prev_heap_end = heap_end;
	heap_end += incr;
	return prev_heap_end;
}

int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	(void)pid; (void)sig;
	errno = EINVAL;
	return -1;
}

void _exit(int status)
{
	(void)status;
	for(;;);
}
