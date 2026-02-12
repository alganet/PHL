/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXURI_H__
#define __SXURI_H__

#include "sxtypes.h"

/* URI encoding/decoding function prototypes */
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
#endif
PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bUTF8);

#endif /* __SXURI_H__ */
