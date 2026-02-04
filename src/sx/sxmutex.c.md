# src/sx/sxmutex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 77/92 lines (83.70%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "sxtypes.h"` |
|     - |    7 | `#include "sxmutex.h"` |
|     - |    8 | `#if defined(PH7_ENABLE_THREADS)` |
|     - |    9 | `#if defined(__WINNT__)` |
|     - |   10 | `#include <Windows.h>` |
|     - |   11 | `struct SyMutex` |
|     - |   12 |  |
|     - |   13 | `	CRITICAL_SECTION sMutex;` |
|     - |   14 | `	sxu32 nType; /* Mutex type,one of SXMUTEX_TYPE_* */` |
|     - |   15 | `};` |
|     - |   16 | `/* Preallocated static mutex */` |
|     - |   17 | `static SyMutex aStaticMutexes[] = {` |
|     - |   18 | `		{{0},SXMUTEX_TYPE_STATIC_1},` |
|     - |   19 | `		{{0},SXMUTEX_TYPE_STATIC_2},` |
|     - |   20 | `		{{0},SXMUTEX_TYPE_STATIC_3},` |
|     - |   21 | `		{{0},SXMUTEX_TYPE_STATIC_4},` |
|     - |   22 | `		{{0},SXMUTEX_TYPE_STATIC_5},` |
|     - |   23 | `		{{0},SXMUTEX_TYPE_STATIC_6}` |
|     - |   24 | `};` |
|     - |   25 | `static BOOL winMutexInit = FALSE;` |
|     - |   26 | `static LONG winMutexLock = 0;` |
|     - |   27 |  |
|     - |   28 | `static sxi32 WinMutexGlobaInit(void)` |
|     1 |   29 |  |
|     - |   30 | `	LONG rc;` |
|     1 |   31 | `	rc = InterlockedCompareExchange(&winMutexLock,1,0);` |
|     1 |   32 | `	if ( rc == 0 ){` |
|     - |   33 | `		sxu32 n;` |
|     1 |   34 | `		for( n = 0 ; n < SX_ARRAYSIZE(aStaticMutexes) ; ++n ){` |
|     1 |   35 | `			InitializeCriticalSection(&aStaticMutexes[n].sMutex);` |
|     1 |   36 | `		}` |
|     1 |   37 | `		winMutexInit = TRUE;` |
|     1 |   38 | `	}else{` |
|     - |   39 | `		/* Someone else is doing this for us */` |
|   ! 0 |   40 | `		while( winMutexInit == FALSE ){` |
|   ! 0 |   41 | `			Sleep(1);` |
|   ! 0 |   42 | `		}` |
|     - |   43 | `	}` |
|     1 |   44 | `	return SXRET_OK;` |
|     1 |   45 |  |
|     - |   46 | `static void WinMutexGlobalRelease(void)` |
|     1 |   47 |  |
|     - |   48 | `	LONG rc;` |
|     1 |   49 | `	rc = InterlockedCompareExchange(&winMutexLock,0,1);` |
|     1 |   50 | `	if( rc == 1 ){` |
|     - |   51 | `		/* The first to decrement to zero does the actual global release */` |
|     1 |   52 | `		if( winMutexInit == TRUE ){` |
|     - |   53 | `			sxu32 n;` |
|     1 |   54 | `			for( n = 0 ; n < SX_ARRAYSIZE(aStaticMutexes) ; ++n ){` |
|     1 |   55 | `				DeleteCriticalSection(&aStaticMutexes[n].sMutex);` |
|     1 |   56 | `			}` |
|     1 |   57 | `			winMutexInit = FALSE;` |
|     - |   58 | `		}` |
|     - |   59 | `	}` |
|     1 |   60 |  |
|     - |   61 | `static SyMutex * WinMutexNew(int nType)` |
|     1 |   62 |  |
|     1 |   63 | `	SyMutex *pMutex = 0;` |
|     1 |   64 | `	if( nType == SXMUTEX_TYPE_FAST \|\| nType == SXMUTEX_TYPE_RECURSIVE ){` |
|     - |   65 | `		/* Allocate a new mutex */` |
|     1 |   66 | `		pMutex = (SyMutex *)HeapAlloc(GetProcessHeap(),0,sizeof(SyMutex));` |
|     1 |   67 | `		if( pMutex == 0 ){` |
|   ! 0 |   68 | `			return 0;` |
|     - |   69 | `		}` |
|     1 |   70 | `		InitializeCriticalSection(&pMutex->sMutex);` |
|     1 |   71 | `	}else{` |
|     - |   72 | `		/* Use a pre-allocated static mutex */` |
|     1 |   73 | `		if( nType > SXMUTEX_TYPE_STATIC_6 ){` |
|   ! 0 |   74 | `			nType = SXMUTEX_TYPE_STATIC_6;` |
|     - |   75 | `		}` |
|     1 |   76 | `		pMutex = &aStaticMutexes[nType - 3];` |
|     - |   77 | `	}` |
|     1 |   78 | `	pMutex->nType = nType;` |
|     1 |   79 | `	return pMutex;` |
|     1 |   80 |  |
|     - |   81 | `static void WinMutexRelease(SyMutex *pMutex)` |
|     1 |   82 |  |
|     1 |   83 | `	if( pMutex->nType == SXMUTEX_TYPE_FAST \|\| pMutex->nType == SXMUTEX_TYPE_RECURSIVE ){` |
|     1 |   84 | `		DeleteCriticalSection(&pMutex->sMutex);` |
|     1 |   85 | `		HeapFree(GetProcessHeap(),0,pMutex);` |
|     - |   86 | `	}` |
|     1 |   87 |  |
|     - |   88 | `static void WinMutexEnter(SyMutex *pMutex)` |
|     1 |   89 |  |
|     1 |   90 | `	EnterCriticalSection(&pMutex->sMutex);` |
|     1 |   91 |  |
|     - |   92 | `static sxi32 WinMutexTryEnter(SyMutex *pMutex)` |
|   ! 0 |   93 |  |
|     - |   94 | `#ifdef _WIN32_WINNT` |
|     - |   95 | `	BOOL rc;` |
|     - |   96 | `	/* Only WindowsNT platforms */` |
|   ! 0 |   97 | `	rc = TryEnterCriticalSection(&pMutex->sMutex);` |
|   ! 0 |   98 | `	if( rc ){` |
|   ! 0 |   99 | `		return SXRET_OK;` |
|   ! 0 |  100 | `	}else{` |
|   ! 0 |  101 | `		return SXERR_BUSY;` |
|     - |  102 | `	}` |
|     - |  103 | `#else` |
|     - |  104 | `	return SXERR_NOTIMPLEMENTED;` |
|     - |  105 | `#endif` |
|   ! 0 |  106 |  |
|     - |  107 | `static void WinMutexLeave(SyMutex *pMutex)` |
|     1 |  108 |  |
|     1 |  109 | `	LeaveCriticalSection(&pMutex->sMutex);` |
|     1 |  110 |  |
|     - |  111 | `/* Export Windows mutex interfaces */` |
|     - |  112 | `static const SyMutexMethods sWinMutexMethods = {` |
|     - |  113 | `	WinMutexGlobaInit,  /* xGlobalInit() */` |
|     - |  114 | `	WinMutexGlobalRelease, /* xGlobalRelease() */` |
|     - |  115 | `	WinMutexNew,     /* xNew() */` |
|     - |  116 | `	WinMutexRelease, /* xRelease() */` |
|     - |  117 | `	WinMutexEnter,   /* xEnter() */` |
|     - |  118 | `	WinMutexTryEnter, /* xTryEnter() */` |
|     - |  119 | `	WinMutexLeave     /* xLeave() */` |
|     - |  120 | `};` |
|     - |  121 | `PH7_PRIVATE const SyMutexMethods * SyMutexExportMethods(void)` |
|     1 |  122 |  |
|     1 |  123 | `	return &sWinMutexMethods;` |
|     1 |  124 |  |
|     - |  125 | `#elif defined(__UNIXES__)` |
|     - |  126 | `#include <pthread.h>` |
|     - |  127 | `#include <stdlib.h>` |
|     - |  128 | `struct SyMutex` |
|     - |  129 |  |
|     - |  130 | `	pthread_mutex_t sMutex;` |
|     - |  131 | `	sxu32 nType;` |
|     - |  132 | `};` |
| 31762 |  133 | `static SyMutex * UnixMutexNew(int nType)` |
|     - |  134 |  |
|     - |  135 | `	static SyMutex aStaticMutexes[] = {` |
|     - |  136 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_1},` |
|     - |  137 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_2},` |
|     - |  138 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_3},` |
|     - |  139 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_4},` |
|     - |  140 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_5},` |
|     - |  141 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_6}` |
|     - |  142 | `	};` |
|     - |  143 | `	SyMutex *pMutex;` |
|     - |  144 |  |
| 44976 |  145 | `	if( nType == SXMUTEX_TYPE_FAST \|\| nType == SXMUTEX_TYPE_RECURSIVE ){` |
|     - |  146 | `		pthread_mutexattr_t sRecursiveAttr;` |
|     - |  147 | `  		/* Allocate a new mutex */` |
| 26428 |  148 | `  		pMutex = (SyMutex *)malloc(sizeof(SyMutex));` |
| 26428 |  149 | `  		if( pMutex == 0 ){` |
|   ! 0 |  150 | `  			return 0;` |
|     - |  151 | `  		}` |
| 26428 |  152 | `  		if( nType == SXMUTEX_TYPE_RECURSIVE ){` |
| 10426 |  153 | `  			pthread_mutexattr_init(&sRecursiveAttr);` |
| 10426 |  154 | `  			pthread_mutexattr_settype(&sRecursiveAttr,PTHREAD_MUTEX_RECURSIVE);` |
|  5213 |  155 | `  		}` |
| 26428 |  156 | `  		pthread_mutex_init(&pMutex->sMutex,nType == SXMUTEX_TYPE_RECURSIVE ? &sRecursiveAttr : 0 );` |
| 26428 |  157 | `		if(	nType == SXMUTEX_TYPE_RECURSIVE ){` |
| 10426 |  158 | `   			pthread_mutexattr_destroy(&sRecursiveAttr);` |
|  5213 |  159 | `		}` |
| 13214 |  160 | `	}else{` |
|     - |  161 | `		/* Use a pre-allocated static mutex */` |
|  5334 |  162 | `		if( nType > SXMUTEX_TYPE_STATIC_6 ){` |
|   ! 0 |  163 | `			nType = SXMUTEX_TYPE_STATIC_6;` |
|   ! 0 |  164 | `		}` |
|  5334 |  165 | `		pMutex = &aStaticMutexes[nType - 3];` |
|     - |  166 | `	}` |
| 31762 |  167 | `  pMutex->nType = nType;` |
|     - |  168 |  |
| 31762 |  169 | `  return pMutex;` |
| 15881 |  170 |  |
| 10902 |  171 | `static void UnixMutexRelease(SyMutex *pMutex)` |
|     - |  172 |  |
| 10902 |  173 | `	if( pMutex->nType == SXMUTEX_TYPE_FAST \|\| pMutex->nType == SXMUTEX_TYPE_RECURSIVE ){` |
| 10902 |  174 | `		pthread_mutex_destroy(&pMutex->sMutex);` |
| 10902 |  175 | `		free(pMutex);` |
|  5451 |  176 | `	}` |
| 10902 |  177 |  |
| 67624 |  178 | `static void UnixMutexEnter(SyMutex *pMutex)` |
|     - |  179 |  |
| 67624 |  180 | `	pthread_mutex_lock(&pMutex->sMutex);` |
| 67624 |  181 |  |
| 67616 |  182 | `static void UnixMutexLeave(SyMutex *pMutex)` |
|     - |  183 |  |
| 67616 |  184 | `	pthread_mutex_unlock(&pMutex->sMutex);` |
| 67616 |  185 |  |
|     - |  186 | `/* Export pthread mutex interfaces */` |
|     - |  187 | `static const SyMutexMethods sPthreadMutexMethods = {` |
|     - |  188 | `	0, /* xGlobalInit() */` |
|     - |  189 | `	0, /* xGlobalRelease() */` |
|     - |  190 | `	UnixMutexNew,      /* xNew() */` |
|     - |  191 | `	UnixMutexRelease,  /* xRelease() */` |
|     - |  192 | `	UnixMutexEnter,    /* xEnter() */` |
|     - |  193 | `	0,                 /* xTryEnter() */` |
|     - |  194 | `	UnixMutexLeave     /* xLeave() */` |
|     - |  195 | `};` |
|  5334 |  196 | `PH7_PRIVATE const SyMutexMethods * SyMutexExportMethods(void)` |
|     - |  197 |  |
|  5334 |  198 | `	return &sPthreadMutexMethods;` |
|     - |  199 |  |
|     - |  200 | `#else` |
|     - |  201 | `/* Host application must register their own mutex subsystem if the target` |
|     - |  202 | ` * platform is not an UNIX-like or windows systems.` |
|     - |  203 | ` */` |
|     - |  204 | `struct SyMutex` |
|     - |  205 |  |
|     - |  206 | `	sxu32 nType;` |
|     - |  207 | `};` |
|     - |  208 | `static SyMutex * DummyMutexNew(int nType)` |
|     - |  209 |  |
|     - |  210 | `	static SyMutex sMutex;` |
|     - |  211 | `	SXUNUSED(nType);` |
|     - |  212 | `	return &sMutex;` |
|     - |  213 |  |
|     - |  214 | `static void DummyMutexRelease(SyMutex *pMutex)` |
|     - |  215 |  |
|     - |  216 | `	SXUNUSED(pMutex);` |
|     - |  217 |  |
|     - |  218 | `static void DummyMutexEnter(SyMutex *pMutex)` |
|     - |  219 |  |
|     - |  220 | `	SXUNUSED(pMutex);` |
|     - |  221 |  |
|     - |  222 | `static void DummyMutexLeave(SyMutex *pMutex)` |
|     - |  223 |  |
|     - |  224 | `	SXUNUSED(pMutex);` |
|     - |  225 |  |
|     - |  226 | `/* Export the dummy mutex interfaces */` |
|     - |  227 | `static const SyMutexMethods sDummyMutexMethods = {` |
|     - |  228 | `	0, /* xGlobalInit() */` |
|     - |  229 | `	0, /* xGlobalRelease() */` |
|     - |  230 | `	DummyMutexNew,      /* xNew() */` |
|     - |  231 | `	DummyMutexRelease,  /* xRelease() */` |
|     - |  232 | `	DummyMutexEnter,    /* xEnter() */` |
|     - |  233 | `	0,                  /* xTryEnter() */` |
|     - |  234 | `	DummyMutexLeave     /* xLeave() */` |
|     - |  235 | `};` |
|     - |  236 | `PH7_PRIVATE const SyMutexMethods * SyMutexExportMethods(void)` |
|     - |  237 |  |
|     - |  238 | `	return &sDummyMutexMethods;` |
|     - |  239 |  |
|     - |  240 | `#endif /* __WINNT__ */` |
|     - |  241 | `#endif /* PH7_ENABLE_THREADS */` |
|     - |  242 |  |
