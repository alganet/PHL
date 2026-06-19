/*
 * Embedding example: overriding the engine clock via PH7_CONFIG_CLOCK.
 *
 * microtime()/gettimeofday() normally read the platform wall clock
 * (gettimeofday() on Unix). An embedder with no usable sub-second system
 * clock — e.g. the ESP32 port, which has esp_timer but builds with
 * -DOS_OTHER — can install a callback that supplies "now" instead, so
 * standard PHP timing works without re-registering microtime() in PHP.
 *
 * Build (from the repo root), linking against the split engine sources in
 * src/sx and src/ph7 (everything except net.c), e.g.:
 *   cc -Isrc/sx -Isrc/ph7 -D__UNIXES__ -o /tmp/ph7_clock_hook \
 *      examples/ph7_clock_hook.c src/sx/[a-z]*.c \
 *      $(ls src/ph7/[a-z]*.c | grep -v net.c) -lm -lpthread
 *   then run /tmp/ph7_clock_hook  -> 1700000000.123456 / usec=123456 / OK
 */
#include <stdio.h>
#include "ph7.h"

/* Deterministic clock: a fixed instant, so the test output is stable. */
static int fixed_clock(void *pUserData, ph7_int64 *pSec, ph7_int64 *pUsec)
{
	(void)pUserData;
	*pSec  = 1700000000;
	*pUsec = 123456;
	return PH7_OK;
}

static int Consumer(const void *pOut, unsigned int nLen, void *pUserData)
{
	(void)pUserData;
	fwrite(pOut, 1, nLen, stdout);
	return PH7_OK;
}

static const char *zScript =
	"<?php\n"
	"printf(\"%.6f\\n\", microtime(true));\n"
	"$g = gettimeofday();\n"
	"echo 'usec=', $g['usec'], \"\\n\";\n"
	"echo (abs(microtime(true) - 1700000000.123456) < 1e-3) ? \"OK\\n\" : \"FAIL\\n\";\n";

int main(void)
{
	ph7 *pEngine;
	ph7_vm *pVm;
	if (ph7_init(&pEngine) != PH7_OK) {
		fprintf(stderr, "ph7_init failed\n");
		return 1;
	}
	/* Install the clock override before compiling/running. */
	if (ph7_config(pEngine, PH7_CONFIG_CLOCK, fixed_clock, (void *)0) != PH7_OK) {
		fprintf(stderr, "PH7_CONFIG_CLOCK failed\n");
		return 1;
	}
	if (ph7_compile_v2(pEngine, zScript, -1, &pVm, 0) != PH7_OK) {
		fprintf(stderr, "compile failed\n");
		return 1;
	}
	ph7_vm_config(pVm, PH7_VM_CONFIG_OUTPUT, Consumer, (void *)0);
	ph7_vm_exec(pVm, 0);
	ph7_vm_release(pVm);
	ph7_release(pEngine);
	return 0;
}
