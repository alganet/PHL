/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_CURL
#include "curl_int.h"

/*
 * Section:
 *    ext/curl -- php's binding of libcurl.
 * Status:
 *    In progress. This unit currently carries the library-wide surface:
 *    curl_version() and the three strerror() families. The handle classes,
 *    the option table and the transfer verbs follow.
 *
 * WHY A BINDING AND NOT A REIMPLEMENTATION. php's ext/curl is a thin shell
 * over libcurl, so the contract a program depends on is the LIBRARY's, not
 * the extension's -- the same relationship ext/pdo_sqlite has with
 * libsqlite3. Every answer here is therefore derived by asking php 8.5 and
 * libcurl 8.5 the same question, never by reading php's source: see the
 * per-function notes below for the ones a careful reading would have got
 * wrong.
 *
 * MEMORY MODEL. libcurl stays on its own (system) allocator, like libxml2 and
 * sqlite3 before it. Routing it through SyMemBackend would subject library
 * internals to the PHL_MAX_ALLOC fault injection the stress tier uses, and
 * libcurl does not tolerate mid-transfer OOM injection the way engine code
 * does.
 */

/*
 * curl_global_init() once per process. libcurl's own documentation calls this
 * not thread-safe, so it must not be left to the first curl_easy_init() on
 * whichever thread gets there first (the -S server pre-forks, and
 * PH7_ENABLE_THREADS builds share the process).
 */
PH7_PRIVATE void PH7_CurlGlobalInit(void)
{
	static int bInit = 0;
	if( !bInit ){
		curl_global_init(CURL_GLOBAL_DEFAULT);
		bInit = 1;
	}
}

/* ===== curl_version() ===== */

/*
 * php's feature-name table: the bit each name reports, in php's own order.
 * The names are php's spelling, not libcurl's constant tails ("GSS-Negotiate",
 * "krb4", "TLS-SRP", "NTLMWB", "CharConv"), and all 29 were verified against
 * both the oracle's feature_list and curl_version_info()'s features word.
 */
static const struct CurlFeatureName {
	const char *zName;
	unsigned int iBit;
} aCurlFeature[] = {
	{ "AsynchDNS",     CURL_VERSION_ASYNCHDNS     },
	{ "CharConv",      CURL_VERSION_CONV          },
	{ "Debug",         CURL_VERSION_DEBUG         },
	{ "GSS-Negotiate", CURL_VERSION_GSSNEGOTIATE  },
	{ "IDN",           CURL_VERSION_IDN           },
	{ "IPv6",          CURL_VERSION_IPV6          },
	{ "krb4",          CURL_VERSION_KERBEROS4     },
	{ "Largefile",     CURL_VERSION_LARGEFILE     },
	{ "libz",          CURL_VERSION_LIBZ          },
	{ "NTLM",          CURL_VERSION_NTLM          },
	{ "NTLMWB",        CURL_VERSION_NTLM_WB       },
	{ "SPNEGO",        CURL_VERSION_SPNEGO        },
	{ "SSL",           CURL_VERSION_SSL           },
	{ "SSPI",          CURL_VERSION_SSPI          },
	{ "TLS-SRP",       CURL_VERSION_TLSAUTH_SRP   },
	{ "HTTP2",         CURL_VERSION_HTTP2         },
	{ "GSSAPI",        CURL_VERSION_GSSAPI        },
	{ "KERBEROS5",     CURL_VERSION_KERBEROS5     },
	{ "UNIX_SOCKETS",  CURL_VERSION_UNIX_SOCKETS  },
	{ "PSL",           CURL_VERSION_PSL           },
	{ "HTTPS_PROXY",   CURL_VERSION_HTTPS_PROXY   },
	{ "MULTI_SSL",     CURL_VERSION_MULTI_SSL     },
	{ "BROTLI",        CURL_VERSION_BROTLI        },
	{ "ALTSVC",        CURL_VERSION_ALTSVC        },
	{ "HTTP3",         CURL_VERSION_HTTP3         },
	{ "UNICODE",       CURL_VERSION_UNICODE       },
	{ "ZSTD",          CURL_VERSION_ZSTD          },
	{ "HSTS",          CURL_VERSION_HSTS          },
	{ "GSASL",         CURL_VERSION_GSASL         }
};

/* An int entry on the answer. */
static void CurlVersionAddInt(ph7_value *pArray,ph7_value *pWorker,
	const char *zKey,sxi64 iVal)
{
	ph7_value_int64(pWorker,iVal);
	ph7_array_add_strkey_elem(pArray,zKey,pWorker);
}

/*
 * A string entry on the answer. php prints the EMPTY STRING for a field
 * libcurl left NULL -- `ares` is NULL in every build without c-ares and the
 * oracle still answers string(0) "" for it, never null.
 */
static void CurlVersionAddStr(ph7_value *pArray,ph7_value *pWorker,
	const char *zKey,const char *zVal)
{
	ph7_value_reset_string_cursor(pWorker);
	ph7_value_string(pWorker,zVal ? zVal : "",-1);
	ph7_array_add_strkey_elem(pArray,zKey,pWorker);
	ph7_value_reset_string_cursor(pWorker);
}

/* array|false curl_version() */
static int vm_builtin_curl_version(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	curl_version_info_data *pInfo;
	ph7_value *pArray,*pWorker,*pFeature,*pProto;
	const char * const *pzProto;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_CurlGlobalInit();
	pInfo = curl_version_info(CURLVERSION_NOW);
	if( pInfo == 0 ){
		/* php's own guard: the library answered nothing, so neither do we. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray   = ph7_context_new_array(pCtx);
	pWorker  = ph7_context_new_scalar(pCtx);
	pFeature = ph7_context_new_array(pCtx);
	pProto   = ph7_context_new_array(pCtx);
	if( pArray == 0 || pWorker == 0 || pFeature == 0 || pProto == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* The key ORDER is php's and is user-visible through print_r/var_dump and
	 * a foreach; it is not libcurl's struct order. */
	CurlVersionAddInt(pArray,pWorker,"version_number",(sxi64)pInfo->version_num);
	CurlVersionAddInt(pArray,pWorker,"age",(sxi64)pInfo->age);
	CurlVersionAddInt(pArray,pWorker,"features",(sxi64)pInfo->features);
	for( n = 0 ; n < SX_ARRAYSIZE(aCurlFeature) ; ++n ){
		ph7_value_bool(pWorker,(pInfo->features & aCurlFeature[n].iBit) ? 1 : 0);
		ph7_array_add_strkey_elem(pFeature,aCurlFeature[n].zName,pWorker);
	}
	ph7_array_add_strkey_elem(pArray,"feature_list",pFeature);
	CurlVersionAddInt(pArray,pWorker,"ssl_version_number",(sxi64)pInfo->ssl_version_num);
	CurlVersionAddStr(pArray,pWorker,"version",pInfo->version);
	CurlVersionAddStr(pArray,pWorker,"host",pInfo->host);
	CurlVersionAddStr(pArray,pWorker,"ssl_version",pInfo->ssl_version);
	CurlVersionAddStr(pArray,pWorker,"libz_version",pInfo->libz_version);
	for( pzProto = pInfo->protocols ; pzProto && *pzProto ; ++pzProto ){
		ph7_value_reset_string_cursor(pWorker);
		ph7_value_string(pWorker,*pzProto,-1);
		ph7_array_add_elem(pProto,0,pWorker);
	}
	ph7_value_reset_string_cursor(pWorker);
	ph7_array_add_strkey_elem(pArray,"protocols",pProto);
	/*
	 * Everything below is gated on the struct's OWN age, which is what php
	 * gates on -- and php stops at the brotli block even against a library
	 * reporting age 10, so zstd_version and the fields after it are absent
	 * from the answer even where libcurl reports them. Reproducing php means
	 * stopping here too, not exposing what the library happens to know.
	 */
	if( pInfo->age >= CURLVERSION_SECOND ){
		CurlVersionAddStr(pArray,pWorker,"ares",pInfo->ares);
		CurlVersionAddInt(pArray,pWorker,"ares_num",(sxi64)pInfo->ares_num);
	}
	if( pInfo->age >= CURLVERSION_THIRD ){
		CurlVersionAddStr(pArray,pWorker,"libidn",pInfo->libidn);
	}
	if( pInfo->age >= CURLVERSION_FOURTH ){
		CurlVersionAddInt(pArray,pWorker,"iconv_ver_num",(sxi64)pInfo->iconv_ver_num);
		CurlVersionAddStr(pArray,pWorker,"libssh_version",pInfo->libssh_version);
	}
	if( pInfo->age >= CURLVERSION_FIFTH ){
		CurlVersionAddInt(pArray,pWorker,"brotli_ver_num",(sxi64)pInfo->brotli_ver_num);
		CurlVersionAddStr(pArray,pWorker,"brotli_version",pInfo->brotli_version);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}

/* ===== The three strerror families ===== */

/*
 * All three are pass-throughs, and the pass-through is the point: the codes
 * are C enums, so php narrows its zend_long argument to the enum's width
 * before the library ever sees it. That truncation is user-visible --
 * curl_multi_strerror(PHP_INT_MAX) answers "Please call curl_multi_perform()
 * soon" because the value arrives as -1 (CURLM_CALL_MULTI_PERFORM), not
 * because PHP_INT_MAX means anything. Casting to int here reproduces it; a
 * range check would not.
 *
 * The declared return type is ?string for the same reason php's is: the
 * answer is whatever the library's pointer says, including NULL.
 */
static int CurlStrError(ph7_context *pCtx,int nArg,ph7_value **apArg,
	const char *(*xStr)(int))
{
	int iCode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 0;
	const char *zMsg = xStr(iCode);
	if( zMsg == 0 ){
		ph7_result_null(pCtx);
	}else{
		ph7_result_string(pCtx,zMsg,-1);
	}
	return PH7_OK;
}
static const char * CurlEasyStrErrorTrampoline(int iCode)
{
	return curl_easy_strerror((CURLcode)iCode);
}
static const char * CurlMultiStrErrorTrampoline(int iCode)
{
	return curl_multi_strerror((CURLMcode)iCode);
}
static const char * CurlShareStrErrorTrampoline(int iCode)
{
	return curl_share_strerror((CURLSHcode)iCode);
}
/* ?string curl_strerror(int $error_code) */
static int vm_builtin_curl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CurlStrError(pCtx,nArg,apArg,CurlEasyStrErrorTrampoline);
}
/* ?string curl_multi_strerror(int $error_code) */
static int vm_builtin_curl_multi_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CurlStrError(pCtx,nArg,apArg,CurlMultiStrErrorTrampoline);
}
/* ?string curl_share_strerror(int $error_code) */
static int vm_builtin_curl_share_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CurlStrError(pCtx,nArg,apArg,CurlShareStrErrorTrampoline);
}

/* ===== Installation ===== */

PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "curl_version",        vm_builtin_curl_version        },
		{ "curl_strerror",       vm_builtin_curl_strerror       },
		{ "curl_multi_strerror", vm_builtin_curl_multi_strerror },
		{ "curl_share_strerror", vm_builtin_curl_share_strerror }
	};
	sxu32 n;
	PH7_CurlGlobalInit();
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; ++n ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	return SXRET_OK;
}

#else
/* Ensure non-empty translation unit when curl is disabled (MSVC C4206) */
typedef int vm_curl_unused;
#endif /* PH7_ENABLE_CURL */
