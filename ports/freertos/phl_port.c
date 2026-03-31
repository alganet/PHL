/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "phl_port.h"
#include "ph7.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>
#include <stdio.h>

/* Output buffer used by phl_port_exec */
typedef struct {
	char *zBuf;
	unsigned int nMax;
	unsigned int nUsed;
} OutputCtx;

/*
 * FreeRTOS memory methods for PHL.
 * FreeRTOS heap_4 does not provide realloc, so we implement it
 * by prefixing each allocation with its size.
 */
static void freertos_free(void *p);

static void *freertos_alloc(unsigned int nByte)
{
	unsigned int *p = (unsigned int *)pvPortMalloc(nByte + sizeof(unsigned int));
	if( p ){
		*p = nByte;
		return (char *)p + sizeof(unsigned int);
	}
	return 0;
}

static void *freertos_realloc(void *pOld, unsigned int nByte)
{
	void *pNew;
	unsigned int nOld = 0;
	if( pOld ){
		nOld = *((unsigned int *)pOld - 1);
	}
	pNew = freertos_alloc(nByte);
	if( pNew && pOld ){
		memcpy(pNew, pOld, nOld < nByte ? nOld : nByte);
		freertos_free(pOld);
	}
	return pNew;
}

static void freertos_free(void *p)
{
	if( p ){
		vPortFree((char *)p - sizeof(unsigned int));
	}
}

static unsigned int freertos_chunk_size(void *p)
{
	if( p ){
		return *((unsigned int *)p - 1);
	}
	return 0;
}

static const SyMemMethods sFreertosMem = {
	freertos_alloc,      /* xAlloc */
	freertos_realloc,    /* xRealloc */
	freertos_free,       /* xFree */
	freertos_chunk_size, /* xChunkSize */
	0,                   /* xInit */
	0,                   /* xRelease */
	0                    /* pUserData */
};

int phl_port_init(void)
{
	return ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC, &sFreertosMem);
}

int phl_port_output_consumer(const void *pOutput, unsigned int nOutputLen, void *pUserData)
{
	OutputCtx *pCtx = (OutputCtx *)pUserData;
	unsigned int nCopy;
	if( pCtx == 0 ){
		return PH7_OK;
	}
	nCopy = nOutputLen;
	if( pCtx->nUsed + nCopy > pCtx->nMax ){
		nCopy = pCtx->nMax - pCtx->nUsed;
	}
	if( nCopy > 0 ){
		memcpy(&pCtx->zBuf[pCtx->nUsed], pOutput, nCopy);
		pCtx->nUsed += nCopy;
	}
	return PH7_OK;
}

int phl_port_exec(const char *zScript, char *zOut, unsigned int nOut, unsigned int *pOutLen)
{
	ph7 *pEngine = 0;
	ph7_vm *pVm = 0;
	OutputCtx sCtx;
	int rc;

	sCtx.zBuf = zOut;
	sCtx.nMax = nOut > 0 ? nOut - 1 : 0; /* reserve space for NUL */
	sCtx.nUsed = 0;

	rc = ph7_init(&pEngine);
	if( rc != PH7_OK ){
		return rc;
	}
	rc = ph7_compile_v2(pEngine, zScript, -1, &pVm, 0);
	if( rc != PH7_OK ){
		ph7_release(pEngine);
		return rc;
	}
	rc = ph7_vm_config(pVm, PH7_VM_CONFIG_OUTPUT, phl_port_output_consumer, &sCtx);
	if( rc != PH7_OK ){
		ph7_vm_release(pVm);
		ph7_release(pEngine);
		return rc;
	}
	rc = ph7_vm_exec(pVm, 0);
	ph7_vm_release(pVm);
	ph7_release(pEngine);

	/* NUL-terminate */
	if( nOut > 0 ){
		zOut[sCtx.nUsed] = '\0';
	}
	if( pOutLen ){
		*pOutLen = sCtx.nUsed;
	}
	return rc;
}
