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
 * Where a transfer's BODY goes.
 *
 * php keeps one field for this, not a flag per option: CURLOPT_RETURNTRANSFER
 * and CURLOPT_WRITEFUNCTION both write it, so the LAST of the two to be set
 * decides, and setting the callback back to null does not restore the other --
 * it lands on the default. Modelling them as two independent settings answers
 * differently in three places (a handle carrying both, a null that follows a
 * RETURNTRANSFER, and the copy of either), so the destination is one value.
 */
#define PHL_CURL_DEST_STDOUT 0   /* the script's own output -- php's default */
#define PHL_CURL_DEST_RETURN 1   /* CURLOPT_RETURNTRANSFER: curl_exec answers it */
#define PHL_CURL_DEST_USER   2   /* CURLOPT_WRITEFUNCTION: the callback is the sink */

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

/*
 * One FILE part of a multipart body, and the stream it reads from.
 *
 * php does not hand libcurl a path: it opens the CURLFile's name through the
 * STREAM layer at setopt time and gives the mime part read/seek callbacks over
 * that open stream. Three answers only that model produces, all measured
 * against php 8.5.9: `php://temp` and `data://text/plain,hi` are legal upload
 * sources, a file UNLINKED between setopt and exec still uploads (the handle is
 * already open), and a file that cannot be opened is CURLE_ABORTED_BY_CALLBACK
 * at exec rather than a refusal at setopt.
 *
 * The record outlives the setopt call and is owned by the handle, because
 * libcurl reads it during the transfer. It is NOT freed through libcurl's own
 * mime free callback: curl_easy_duphandle copies a callback part by copying the
 * callback ARGUMENT, so a freeing duplicate would tear down the source's
 * stream. The handle frees its own list instead, and a duplicate rebuilds the
 * whole mime from the array php keeps for exactly that purpose.
 */
typedef struct phl_curl phl_curl;
typedef struct phl_curl_part phl_curl_part;
struct phl_curl_part {
	const ph7_io_stream *pStream;  /* the device the handle was opened on */
	void *pHandle;                 /* the open stream, or 0 when the open failed */
	curl_off_t nSize;              /* what the part declares, taken at setopt time */
	int bNoPath;                   /* the CURLFile named nothing at all */
	phl_curl *pOwner;              /* for the diagnostics a read raises */
	phl_curl_part *pNext;
};

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
	int iWriteDest;                 /* where the body goes: one of PHL_CURL_DEST_* */
	/*
	 * The multipart body CURLOPT_POSTFIELDS built from an array: the mime
	 * libcurl reads, the streams its file parts read from, and the ARRAY
	 * itself -- which php keeps so that a copied handle can rebuild the whole
	 * structure rather than share one that cannot be duplicated.
	 */
	curl_mime *pMime;
	phl_curl_part *pParts;
	ph7_value *pPostArray;
	ph7_context *pExecCtx;          /* the running curl_exec, for a diagnostic an
	                                 * upload read raises from inside libcurl */
	int bNoPathRead;                /* an upload part with no source was read: the
	                                 * refusal is raised once the library unwinds */
	phl_curl *pNext;                /* per-VM registry chain */
};

#endif /* PH7_ENABLE_CURL */
#endif /* PHL_CURL_INT_H */
