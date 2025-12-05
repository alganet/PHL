/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXMUTEX_H__
#define __SXMUTEX_H__

#include "sxtypes.h"

/* Mutex types */
#define SXMUTEX_TYPE_FAST      1
#define SXMUTEX_TYPE_RECURSIVE 2
#define SXMUTEX_TYPE_STATIC_1  3
#define SXMUTEX_TYPE_STATIC_2  4
#define SXMUTEX_TYPE_STATIC_3  5
#define SXMUTEX_TYPE_STATIC_4  6
#define SXMUTEX_TYPE_STATIC_5  7
#define SXMUTEX_TYPE_STATIC_6  8

/* Mutex operation macros */
#define SyMutexGlobalInit(METHOD){\
	if( (METHOD)->xGlobalInit ){\
	(METHOD)->xGlobalInit();\
	}\
}
#define SyMutexGlobalRelease(METHOD){\
	if( (METHOD)->xGlobalRelease ){\
	(METHOD)->xGlobalRelease();\
	}\
}
#define SyMutexNew(METHOD,TYPE)        (METHOD)->xNew(TYPE)
#define SyMutexRelease(METHOD,MUTEX){\
	if( MUTEX && (METHOD)->xRelease ){\
		(METHOD)->xRelease(MUTEX);\
	}\
}
#define SyMutexEnter(METHOD,MUTEX){\
	if( MUTEX ){\
	(METHOD)->xEnter(MUTEX);\
	}\
}
#define SyMutexTryEnter(METHOD,MUTEX){\
	if( MUTEX && (METHOD)->xTryEnter ){\
	(METHOD)->xTryEnter(MUTEX);\
	}\
}
#define SyMutexLeave(METHOD,MUTEX){\
	if( MUTEX ){\
	(METHOD)->xLeave(MUTEX);\
	}\
}

/* Mutex function prototypes */
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE const SyMutexMethods *SyMutexExportMethods(void);
#endif

#endif /* __SXMUTEX_H__ */
