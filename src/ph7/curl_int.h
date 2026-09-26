/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PHL_CURL_INT_H
#define PHL_CURL_INT_H
#ifdef PH7_ENABLE_CURL
#include "ph7int.h"
/*
 * libcurl marks sixteen of the enum members php still EXPOSES as deprecated
 * (CURLOPT_PROGRESSFUNCTION, CURLOPT_PROTOCOLS, the seven pre-7.55 CURLINFO_*
 * doubles, ...). Naming one then costs a -Wdeprecated-declarations, which this
 * build turns into an error under -Werror / /WX -- and php defines every one of
 * them, so the table cannot simply drop them: they are the LIBRARY's
 * deprecations, not php's, and §10 only removes what PHP deprecates. This is
 * libcurl's own documented opt-out, and it is portable across gcc, clang and
 * MSVC in a way a per-compiler pragma is not.
 */
#define CURL_DISABLE_DEPRECATION 1
#include <curl/curl.h>

/*
 * Private header shared by the ext/curl units (vm_curl.c today; the multi and
 * share halves join it later). Nothing here is public API -- the same role
 * pdo_int.h plays for ext/pdo.
 *
 * MINIMUM LIBCURL. php's ext/curl exposes a surface that grows with the
 * library: options, info selectors and error codes are ENUM MEMBERS, not
 * macros, so `#ifdef CURLOPT_X` can never gate one and every version test has
 * to read LIBCURL_VERSION_NUM. Rather than carry a per-symbol version map for
 * the whole 679-constant table, PHL declares a floor and gates only what
 * postdates it. 8.5.0 is what this engine's oracle (php 8.5.9), the CI Ubuntu
 * image and the derivation tables were all built against; a build below it
 * would silently define a smaller surface than the tests assert.
 */
#if !defined(LIBCURL_VERSION_NUM) || LIBCURL_VERSION_NUM < 0x080500
#error "PH7_ENABLE_CURL requires libcurl 8.5.0 or later"
#endif

/* Shared one-time curl_global_init() (vm_curl.c). */
PH7_PRIVATE void PH7_CurlGlobalInit(void);

/*
 * One easy handle, and the state php keeps BESIDE it.
 *
 * libcurl remembers no "last error" of its own, so the two reporters
 * (curl_errno/curl_error) read what php stored: the CURLcode the last transfer
 * returned, and the text libcurl wrote into the handle's CURLOPT_ERRORBUFFER,
 * which is more specific than curl_easy_strerror() of the same code ("Failed
 * to connect to 127.0.0.1 port 1 after 0 ms: ..." against "Couldn't connect to
 * server"). The buffer belongs to the record because libcurl writes into it
 * during a transfer and keeps the pointer until the option is cleared.
 *
 * The record is chained on the per-VM registry (pVm->pCurlHandles) for the
 * reason ext/pdo's connections are: a CURL* lives outside SyMemBackend, so the
 * wholesale release at VM teardown would leak it along with its sockets.
 * pOwner is the CurlHandle instance the record backs, so the sweep can blank
 * the instance's slot before freeing what it points at.
 */
/*
 * One curl_slist the HANDLE owns, keyed by the option it was set on.
 *
 * libcurl does not copy a slist: curl_easy_setopt stores the pointer and reads
 * it during the transfer, so the list has to outlive the call and be freed by
 * whoever built it. php keeps exactly this -- a per-handle list of lists --
 * and duplicates them into a copied handle, which is also the evidence that
 * curl_easy_duphandle does not deep-copy them either.
 */
typedef struct phl_curl_slist phl_curl_slist;
struct phl_curl_slist {
	sxi64 iOpt;                 /* the CURLOPT_* this list is set on */
	struct curl_slist *pList;   /* the list itself, or 0 for an empty array */
	phl_curl_slist *pNext;
};

typedef struct phl_curl phl_curl;
struct phl_curl {
	CURL *pEasy;                    /* the libcurl easy handle (never 0 while live) */
	ph7_class_instance *pOwner;     /* the CurlHandle this record backs */
	ph7_vm *pVm;
	int iLastErr;                   /* CURLcode of the last transfer (php's ch->err.no) */
	char zErrBuf[CURL_ERROR_SIZE];  /* libcurl's CURLOPT_ERRORBUFFER target */
	phl_curl_slist *pSlists;        /* the curl_slists this handle owns */
	/*
	 * The php callables libcurl may call back into, owned by the handle: the
	 * library keeps the pointer and may call long after curl_setopt returned,
	 * so the value cannot be the caller's temporary.
	 */
	ph7_value *pWriteCb;
	ph7_value *pHeaderCb;
	ph7_value *pXferCb;             /* XFERINFOFUNCTION, or PROGRESSFUNCTION */
	int bXferIsProgress;            /* the older option's argument shape */
	sxi32 iCbExc;                   /* a callback threw: parked until the verb unwinds */
	int bReturnTransfer;            /* CURLOPT_RETURNTRANSFER: php's own option, no
	                                 * libcurl equivalent -- it picks where the body
	                                 * goes, so it lives here rather than on the
	                                 * easy handle */
	phl_curl *pNext;                /* per-VM registry chain */
};

#endif /* PH7_ENABLE_CURL */
#endif /* PHL_CURL_INT_H */
