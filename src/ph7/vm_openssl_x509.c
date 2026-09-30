/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_OPENSSL
#include "openssl_int.h"

/*
 * Section:
 *    ext/openssl -- the certificate containers: X.509 certificates, signing
 *    requests, PKCS#12 bundles and the PKCS#7 / CMS message families. The
 *    library-wide surface, the keys and the ciphers are in vm_openssl.c.
 * Status:
 *    In progress -- the two OpenSSLCertificate* classes are mounted by the
 *    other unit; the functions over them land next.
 */

PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm)
{
	SXUNUSED(pVm);
	return SXRET_OK;
}
PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry)
{
	*pnEntry = 0;
	return 0;
}

#else
/* Ensure non-empty translation unit when openssl is disabled (MSVC C4206) */
typedef int vm_openssl_x509_unused;
#endif /* PH7_ENABLE_OPENSSL */
