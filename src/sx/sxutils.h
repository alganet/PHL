/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXUTILS_H__
#define __SXUTILS_H__

#include "sxtypes.h"

/* Numeric parsing function prototypes */
PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char **pzTail);
PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyHexToint(sxi32 c);
PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);

#endif /* __SXUTILS_H__ */
