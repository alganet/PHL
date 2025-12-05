/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXFMT_H__
#define __SXFMT_H__

#include "sxtypes.h"
#include "sxset.h"

/* Formatting function prototypes */
PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...);
PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap);
PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...);
PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...);

#endif /* __SXFMT_H__ */
