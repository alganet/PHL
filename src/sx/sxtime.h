/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXTIME_H__
#define __SXTIME_H__

#include "sxtypes.h"

/* Time utility function prototypes */
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
PH7_PRIVATE const char *SyTimeGetDay(sxi32 iDay);
PH7_PRIVATE const char *SyTimeGetMonth(sxi32 iMonth);
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */

#endif /* __SXTIME_H__ */
