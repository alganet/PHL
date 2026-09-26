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

#endif /* PH7_ENABLE_CURL */
#endif /* PHL_CURL_INT_H */
