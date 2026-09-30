/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PHL_OPENSSL_INT_H
#define PHL_OPENSSL_INT_H
#ifdef PH7_ENABLE_OPENSSL
#include "ph7int.h"
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/ec.h>
#include <openssl/bn.h>

/*
 * Private header shared by the ext/openssl units (vm_openssl.c for the
 * library-wide surface, the keys and the ciphers; vm_openssl_x509.c for the
 * certificate containers). Nothing here is public API -- the same role
 * curl_int.h plays for ext/curl.
 *
 * MINIMUM OPENSSL. 3.0 is the floor. Everything below reads the 3.x provider
 * API (EVP_CIPHER_do_all_provided, EVP_PKEY_get_bn_param, the fetch/free
 * pairs), and the 1.1.1 shape of half of it does not exist. php's own 8.5
 * build has the same practical floor, and every box this engine is derived
 * against -- the Linux oracle, the vcpkg Windows guest, Homebrew's openssl@3
 * -- is 3.x.
 */
#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x30000000L
#error "PH7_ENABLE_OPENSSL requires OpenSSL 3.0 or later"
#endif

/*
 * php's ERROR RING, reproduced exactly. php does not hand a script OpenSSL's
 * own error queue: it DRAINS that queue into a 16-slot ring of its own after
 * an operation fails, and openssl_error_string() pops one entry per call from
 * the bottom. The capacity that produces is FIFTEEN (a full ring advances the
 * bottom with the top), and it is observable -- twenty-five failures in a row
 * answer fifteen strings, the LAST fifteen, oldest first.
 */
#define PHL_SSL_ERR_RING 16
typedef struct phl_ssl_errors phl_ssl_errors;
struct phl_ssl_errors {
	/* php's ring holds the code as an INT, and the narrowing is visible: a
	 * system error such as 0x80000002 comes back out sign-extended, which is
	 * why php prints `error:FFFFFFFF80000002:system library::No such file or
	 * directory` on a 64-bit box. Storing it as an unsigned long here would
	 * print eight hex digits where php prints sixteen. */
	int aErr[PHL_SSL_ERR_RING];
	int iTop;
	int iBottom;
};

/*
 * The three opaque handle classes are ONE record shape: php's
 * OpenSSLCertificate, OpenSSLCertificateSigningRequest and
 * OpenSSLAsymmetricKey each wrap a single library pointer and expose no
 * property, no method and no constant. What differs is which pointer, so the
 * record carries a kind tag and the free door switches on it.
 */
#define PHL_SSL_KIND_CERT 0   /* X509 *      -- OpenSSLCertificate */
#define PHL_SSL_KIND_CSR  1   /* X509_REQ *  -- OpenSSLCertificateSigningRequest */
#define PHL_SSL_KIND_KEY  2   /* EVP_PKEY *  -- OpenSSLAsymmetricKey */

typedef struct phl_ssl_obj phl_ssl_obj;
struct phl_ssl_obj {
	int iKind;                    /* PHL_SSL_KIND_* */
	void *pHandle;                /* the library object, freed with the record */
	ph7_class_instance *pOwner;   /* the instance whose __res slot points here */
	phl_ssl_obj *pNext;           /* per-VM registry chain */
};

/* Class names, spelled once. */
#define PHL_SSL_CLASS_CERT "OpenSSLCertificate"
#define PHL_SSL_CLASS_CSR  "OpenSSLCertificateSigningRequest"
#define PHL_SSL_CLASS_KEY  "OpenSSLAsymmetricKey"

/* --- vm_openssl.c, shared with the certificate unit --- */
PH7_PRIVATE void PH7_SslStoreErrors(ph7_vm *pVm);
PH7_PRIVATE phl_ssl_obj * PH7_SslNewObject(ph7_vm *pVm,int iKind,void *pHandle,
	ph7_class_instance **ppInst);
PH7_PRIVATE void * PH7_SslHandleOf(ph7_value *pVal,int iKind);
PH7_PRIVATE void PH7_SslFreeObject(phl_ssl_obj *pObj);
PH7_PRIVATE int PH7_SslResultObject(ph7_context *pCtx,int iKind,void *pHandle);
/* The four key doors, shared with the certificate unit: a `$key` argument is
 * an object, a PEM/DER string, a `file://` path or an array pair, and which of
 * those are legal differs between a PRIVATE and a PUBLIC parameter. */
PH7_PRIVATE EVP_PKEY * PH7_SslKeyOfValue(ph7_context *pCtx,ph7_value *pVal,int bPublic,int *pbOwn);
PH7_PRIVATE int PH7_SslBytesOfValue(ph7_context *pCtx,ph7_value *pVal,SyBlob *pOut,
	const char **pzData,int *pnData);
PH7_PRIVATE int PH7_SslArrayShapeError(ph7_context *pCtx);
PH7_PRIVATE EVP_PKEY * PH7_SslGenerateKey(ph7_context *pCtx,sxi64 iBits,sxi64 iType,
	const char *zCurve,int nCurve);
PH7_PRIVATE const EVP_MD * PH7_SslDigestOfValue(ph7_value *pVal,EVP_MD **ppFetched);
PH7_PRIVATE int PH7_SslReadFileArg(ph7_context *pCtx,const char *zPath,int nPath,
	SyBlob *pOut);
PH7_PRIVATE int PH7_SslWriteFileArg(ph7_context *pCtx,const char *zPath,int nPath,
	const void *pData,sxu32 nData);
PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm);
PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry);

#endif /* PH7_ENABLE_OPENSSL */
#endif /* PHL_OPENSSL_INT_H */
