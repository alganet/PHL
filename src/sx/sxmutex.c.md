# src/sx/sxmutex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 93/109 lines (85.32%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "sxtypes.h"` |
|      - |    7 | `#include "sxmutex.h"` |
|      - |    8 | `#if defined(PH7_ENABLE_THREADS)` |
|      - |    9 | `#if defined(__WINNT__)` |
|      - |   10 | `#include <Windows.h>` |
|      - |   11 | `struct SyMutex` |
|      - |   12 | `{` |
|      - |   13 | `	CRITICAL_SECTION sMutex;` |
|      - |   14 | `	sxu32 nType; /* Mutex type,one of SXMUTEX_TYPE_* */` |
|      - |   15 | `};` |
|      - |   16 | `/* Preallocated static mutex */` |
|      - |   17 | `static SyMutex aStaticMutexes[] = {` |
|      - |   18 | `		{{0},SXMUTEX_TYPE_STATIC_1},` |
|      - |   19 | `		{{0},SXMUTEX_TYPE_STATIC_2},` |
|      - |   20 | `		{{0},SXMUTEX_TYPE_STATIC_3},` |
|      - |   21 | `		{{0},SXMUTEX_TYPE_STATIC_4},` |
|      - |   22 | `		{{0},SXMUTEX_TYPE_STATIC_5},` |
|      - |   23 | `		{{0},SXMUTEX_TYPE_STATIC_6}` |
|      - |   24 | `};` |
|      - |   25 | `static BOOL winMutexInit = FALSE;` |
|      - |   26 | `static LONG winMutexLock = 0;` |
|      - |   27 |  |
|      - |   28 | `static sxi32 WinMutexGlobaInit(void)` |
|      5 |   29 | `{` |
|      - |   30 | `	LONG rc;` |
|      5 |   31 | `	rc = InterlockedCompareExchange(&winMutexLock,1,0);` |
|      5 |   32 | `	if ( rc == 0 ){` |
|      - |   33 | `		sxu32 n;` |
|      5 |   34 | `		for( n = 0 ; n < SX_ARRAYSIZE(aStaticMutexes) ; ++n ){` |
|      5 |   35 | `			InitializeCriticalSection(&aStaticMutexes[n].sMutex);` |
|      5 |   36 | `		}` |
|      5 |   37 | `		winMutexInit = TRUE;` |
|      5 |   38 | `	}else{` |
|      - |   39 | `		/* Someone else is doing this for us */` |
|    ! 0 |   40 | `		while( winMutexInit == FALSE ){` |
|    ! 0 |   41 | `			Sleep(1);` |
|    ! 0 |   42 | `		}` |
|      - |   43 | `	}` |
|      5 |   44 | `	return SXRET_OK;` |
|      5 |   45 | `}` |
|      - |   46 | `static void WinMutexGlobalRelease(void)` |
|      4 |   47 | `{` |
|      - |   48 | `	LONG rc;` |
|      4 |   49 | `	rc = InterlockedCompareExchange(&winMutexLock,0,1);` |
|      4 |   50 | `	if( rc == 1 ){` |
|      - |   51 | `		/* The first to decrement to zero does the actual global release */` |
|      4 |   52 | `		if( winMutexInit == TRUE ){` |
|      - |   53 | `			sxu32 n;` |
|      4 |   54 | `			for( n = 0 ; n < SX_ARRAYSIZE(aStaticMutexes) ; ++n ){` |
|      4 |   55 | `				DeleteCriticalSection(&aStaticMutexes[n].sMutex);` |
|      4 |   56 | `			}` |
|      4 |   57 | `			winMutexInit = FALSE;` |
|      - |   58 | `		}` |
|      - |   59 | `	}` |
|      4 |   60 | `}` |
|      - |   61 | `static SyMutex * WinMutexNew(int nType)` |
|      5 |   62 | `{` |
|      5 |   63 | `	SyMutex *pMutex = 0;` |
|      5 |   64 | `	if( nType == SXMUTEX_TYPE_FAST \|\| nType == SXMUTEX_TYPE_RECURSIVE ){` |
|      - |   65 | `		/* Allocate a new mutex */` |
|      5 |   66 | `		pMutex = (SyMutex *)HeapAlloc(GetProcessHeap(),0,sizeof(SyMutex));` |
|      5 |   67 | `		if( pMutex == 0 ){` |
|    ! 0 |   68 | `			return 0;` |
|      - |   69 | `		}` |
|      5 |   70 | `		InitializeCriticalSection(&pMutex->sMutex);` |
|      5 |   71 | `	}else{` |
|      - |   72 | `		/* Use a pre-allocated static mutex */` |
|      5 |   73 | `		if( nType > SXMUTEX_TYPE_STATIC_6 ){` |
|    ! 0 |   74 | `			nType = SXMUTEX_TYPE_STATIC_6;` |
|      - |   75 | `		}` |
|      5 |   76 | `		pMutex = &aStaticMutexes[nType - 3];` |
|      - |   77 | `	}` |
|      5 |   78 | `	pMutex->nType = nType;` |
|      5 |   79 | `	return pMutex;` |
|      5 |   80 | `}` |
|      - |   81 | `static void WinMutexRelease(SyMutex *pMutex)` |
|      5 |   82 | `{` |
|      5 |   83 | `	if( pMutex->nType == SXMUTEX_TYPE_FAST \|\| pMutex->nType == SXMUTEX_TYPE_RECURSIVE ){` |
|      5 |   84 | `		DeleteCriticalSection(&pMutex->sMutex);` |
|      5 |   85 | `		HeapFree(GetProcessHeap(),0,pMutex);` |
|      - |   86 | `	}` |
|      5 |   87 | `}` |
|      - |   88 | `static void WinMutexEnter(SyMutex *pMutex)` |
|      5 |   89 | `{` |
|      5 |   90 | `	EnterCriticalSection(&pMutex->sMutex);` |
|      5 |   91 | `}` |
|      - |   92 | `static sxi32 WinMutexTryEnter(SyMutex *pMutex)` |
|    ! 0 |   93 | `{` |
|      - |   94 | `#ifdef _WIN32_WINNT` |
|      - |   95 | `	BOOL rc;` |
|      - |   96 | `	/* Only WindowsNT platforms */` |
|    ! 0 |   97 | `	rc = TryEnterCriticalSection(&pMutex->sMutex);` |
|    ! 0 |   98 | `	if( rc ){` |
|    ! 0 |   99 | `		return SXRET_OK;` |
|    ! 0 |  100 | `	}else{` |
|    ! 0 |  101 | `		return SXERR_BUSY;` |
|      - |  102 | `	}` |
|      - |  103 | `#else` |
|      - |  104 | `	return SXERR_NOTIMPLEMENTED;` |
|      - |  105 | `#endif` |
|    ! 0 |  106 | `}` |
|      - |  107 | `static void WinMutexLeave(SyMutex *pMutex)` |
|      5 |  108 | `{` |
|      5 |  109 | `	LeaveCriticalSection(&pMutex->sMutex);` |
|      5 |  110 | `}` |
|      - |  111 | `/* Export Windows mutex interfaces */` |
|      - |  112 | `static const SyMutexMethods sWinMutexMethods = {` |
|      - |  113 | `	WinMutexGlobaInit,  /* xGlobalInit() */` |
|      - |  114 | `	WinMutexGlobalRelease, /* xGlobalRelease() */` |
|      - |  115 | `	WinMutexNew,     /* xNew() */` |
|      - |  116 | `	WinMutexRelease, /* xRelease() */` |
|      - |  117 | `	WinMutexEnter,   /* xEnter() */` |
|      - |  118 | `	WinMutexTryEnter, /* xTryEnter() */` |
|      - |  119 | `	WinMutexLeave     /* xLeave() */` |
|      - |  120 | `};` |
|      - |  121 | `PH7_PRIVATE const SyMutexMethods * SyMutexExportMethods(void)` |
|      5 |  122 | `{` |
|      5 |  123 | `	return &sWinMutexMethods;` |
|      5 |  124 | `}` |
|      - |  125 | `#elif defined(__UNIXES__)` |
|      - |  126 | `#include <pthread.h>` |
|      - |  127 | `#include <stdlib.h>` |
|      - |  128 | `struct SyMutex` |
|      - |  129 | `{` |
|      - |  130 | `	pthread_mutex_t sMutex;` |
|      - |  131 | `	sxu32 nType;` |
|      - |  132 | `};` |
|  39268 |  133 | `static SyMutex * UnixMutexNew(int nType)` |
|      - |  134 | `{` |
|      - |  135 | `	static SyMutex aStaticMutexes[] = {` |
|      - |  136 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_1},` |
|      - |  137 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_2},` |
|      - |  138 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_3},` |
|      - |  139 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_4},` |
|      - |  140 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_5},` |
|      - |  141 | `		{PTHREAD_MUTEX_INITIALIZER,SXMUTEX_TYPE_STATIC_6}` |
|      - |  142 | `	};` |
|      - |  143 | `	SyMutex *pMutex;` |
|      - |  144 |  |
|  55559 |  145 | `	if( nType == SXMUTEX_TYPE_FAST \|\| nType == SXMUTEX_TYPE_RECURSIVE ){` |
|      - |  146 | `		pthread_mutexattr_t sRecursiveAttr;` |
|      - |  147 | `  		/* Allocate a new mutex */` |
|  32537 |  148 | `  		pMutex = (SyMutex *)malloc(sizeof(SyMutex));` |
|  32537 |  149 | `  		if( pMutex == 0 ){` |
|    ! 0 |  150 | `  			return 0;` |
|      - |  151 | `  		}` |
|  32537 |  152 | `  		if( nType == SXMUTEX_TYPE_RECURSIVE ){` |
|  12344 |  153 | `  			pthread_mutexattr_init(&sRecursiveAttr);` |
|  12344 |  154 | `  			pthread_mutexattr_settype(&sRecursiveAttr,PTHREAD_MUTEX_RECURSIVE);` |
|   6163 |  155 | `  		}` |
|  32537 |  156 | `  		pthread_mutex_init(&pMutex->sMutex,nType == SXMUTEX_TYPE_RECURSIVE ? &sRecursiveAttr : 0 );` |
|  32537 |  157 | `		if(	nType == SXMUTEX_TYPE_RECURSIVE ){` |
|  12344 |  158 | `   			pthread_mutexattr_destroy(&sRecursiveAttr);` |
|   6163 |  159 | `		}` |
|  16246 |  160 | `	}else{` |
|      - |  161 | `		/* Use a pre-allocated static mutex */` |
|   6731 |  162 | `		if( nType > SXMUTEX_TYPE_STATIC_6 ){` |
|    ! 0 |  163 | `			nType = SXMUTEX_TYPE_STATIC_6;` |
|    ! 0 |  164 | `		}` |
|   6731 |  165 | `		pMutex = &aStaticMutexes[nType - 3];` |
|      - |  166 | `	}` |
|  39268 |  167 | `  pMutex->nType = nType;` |
|      - |  168 |  |
|  39268 |  169 | `  return pMutex;` |
|  19607 |  170 | `}` |
|  21005 |  171 | `static void UnixMutexRelease(SyMutex *pMutex)` |
|      - |  172 | `{` |
|  21005 |  173 | `	if( pMutex->nType == SXMUTEX_TYPE_FAST \|\| pMutex->nType == SXMUTEX_TYPE_RECURSIVE ){` |
|  21005 |  174 | `		pthread_mutex_destroy(&pMutex->sMutex);` |
|  21005 |  175 | `		free(pMutex);` |
|  10489 |  176 | `	}` |
|  21005 |  177 | `}` |
|      - |  178 | `/*` |
|      - |  179 | ` * Re-initialize a mutex in the CHILD of a fork(). Only one thread survives a` |
|      - |  180 | ` * fork, and every lock the others held is frozen -- worse, a lock this thread` |
|      - |  181 | ` * held is now owned by a thread id that no longer exists, so the child cannot` |
|      - |  182 | ` * even take it again. Re-initializing in the child is what pthread_atfork's` |
|      - |  183 | ` * child handler is FOR, and it is the only defined way out: destroying a locked` |
|      - |  184 | ` * mutex is not, and unlocking one you do not own does nothing.` |
|      - |  185 | ` *` |
|      - |  186 | ` * The lock COUNT is dropped with the lock, so a balancing SyMutexLeave the child` |
|      - |  187 | ` * still owes answers EPERM and changes nothing -- which is the right answer,` |
|      - |  188 | ` * because after this nobody holds it.` |
|      - |  189 | ` */` |
|     64 |  190 | `static void UnixMutexResetAfterFork(SyMutex *pMutex)` |
|      - |  191 | `{` |
|      - |  192 | `	pthread_mutexattr_t sAttr;` |
|     64 |  193 | `	if( pMutex->nType == SXMUTEX_TYPE_RECURSIVE ){` |
|     32 |  194 | `		pthread_mutexattr_init(&sAttr);` |
|     32 |  195 | `		pthread_mutexattr_settype(&sAttr,PTHREAD_MUTEX_RECURSIVE);` |
|     32 |  196 | `		pthread_mutex_init(&pMutex->sMutex,&sAttr);` |
|     32 |  197 | `		pthread_mutexattr_destroy(&sAttr);` |
|     16 |  198 | `	}else{` |
|     32 |  199 | `		pthread_mutex_init(&pMutex->sMutex,0);` |
|      - |  200 | `	}` |
|     64 |  201 | `}` |
| 139164 |  202 | `static void UnixMutexEnter(SyMutex *pMutex)` |
|      - |  203 | `{` |
| 139164 |  204 | `	pthread_mutex_lock(&pMutex->sMutex);` |
| 139164 |  205 | `}` |
| 139180 |  206 | `static void UnixMutexLeave(SyMutex *pMutex)` |
|      - |  207 | `{` |
| 139180 |  208 | `	pthread_mutex_unlock(&pMutex->sMutex);` |
| 139180 |  209 | `}` |
|      - |  210 | `/* Export pthread mutex interfaces */` |
|      - |  211 | `static const SyMutexMethods sPthreadMutexMethods = {` |
|      - |  212 | `	0, /* xGlobalInit() */` |
|      - |  213 | `	0, /* xGlobalRelease() */` |
|      - |  214 | `	UnixMutexNew,      /* xNew() */` |
|      - |  215 | `	UnixMutexRelease,  /* xRelease() */` |
|      - |  216 | `	UnixMutexEnter,    /* xEnter() */` |
|      - |  217 | `	0,                 /* xTryEnter() */` |
|      - |  218 | `	UnixMutexLeave     /* xLeave() */` |
|      - |  219 | `};` |
|   6811 |  220 | `PH7_PRIVATE const SyMutexMethods * SyMutexExportMethods(void)` |
|      - |  221 | `{` |
|   6811 |  222 | `	return &sPthreadMutexMethods;` |
|      - |  223 | `}` |
|      - |  224 | `#else` |
|      - |  225 | `/* Host application must register their own mutex subsystem if the target` |
|      - |  226 | ` * platform is not an UNIX-like or windows systems.` |
|      - |  227 | ` */` |
|      - |  228 | `struct SyMutex` |
|      - |  229 | `{` |
|      - |  230 | `	sxu32 nType;` |
|      - |  231 | `};` |
|      - |  232 | `static SyMutex * DummyMutexNew(int nType)` |
|      - |  233 | `{` |
|      - |  234 | `	static SyMutex sMutex;` |
|      - |  235 | `	SXUNUSED(nType);` |
|      - |  236 | `	return &sMutex;` |
|      - |  237 | `}` |
|      - |  238 | `static void DummyMutexRelease(SyMutex *pMutex)` |
|      - |  239 | `{` |
|      - |  240 | `	SXUNUSED(pMutex);` |
|      - |  241 | `}` |
|      - |  242 | `static void DummyMutexEnter(SyMutex *pMutex)` |
|      - |  243 | `{` |
|      - |  244 | `	SXUNUSED(pMutex);` |
|      - |  245 | `}` |
|      - |  246 | `static void DummyMutexLeave(SyMutex *pMutex)` |
|      - |  247 | `{` |
|      - |  248 | `	SXUNUSED(pMutex);` |
|      - |  249 | `}` |
|      - |  250 | `/* Export the dummy mutex interfaces */` |
|      - |  251 | `static const SyMutexMethods sDummyMutexMethods = {` |
|      - |  252 | `	0, /* xGlobalInit() */` |
|      - |  253 | `	0, /* xGlobalRelease() */` |
|      - |  254 | `	DummyMutexNew,      /* xNew() */` |
|      - |  255 | `	DummyMutexRelease,  /* xRelease() */` |
|      - |  256 | `	DummyMutexEnter,    /* xEnter() */` |
|      - |  257 | `	0,                  /* xTryEnter() */` |
|      - |  258 | `	DummyMutexLeave     /* xLeave() */` |
|      - |  259 | `};` |
|      - |  260 | `PH7_PRIVATE const SyMutexMethods * SyMutexExportMethods(void)` |
|      - |  261 | `{` |
|      - |  262 | `	return &sDummyMutexMethods;` |
|      - |  263 | `}` |
|      - |  264 | `#endif /* __WINNT__ */` |
|      - |  265 | `/*` |
|      - |  266 | ` * Put one mutex back into an unlocked, usable state in the CHILD of a fork().` |
|      - |  267 | ` * Answers 1 when it could, 0 when the mutex is not one of ours -- an embedder` |
|      - |  268 | ` * that installed its OWN mutex subsystem (PH7_LIB_CONFIG_USER_MUTEX) knows what` |
|      - |  269 | ` * its locks are made of and this cannot speak for them, so the caller falls back` |
|      - |  270 | ` * to abandoning the pointer instead.` |
|      - |  271 | ` *` |
|      - |  272 | ` * Only a POSIX build has anything to do here: Windows has no fork().` |
|      - |  273 | ` */` |
|     80 |  274 | `PH7_PRIVATE int SyMutexResetAfterFork(const SyMutexMethods *pMethods,SyMutex *pMutex)` |
|    ! 0 |  275 | `{` |
|      - |  276 | `#if !defined(__WINNT__) && defined(__UNIXES__)` |
|     80 |  277 | `	if( pMethods == SyMutexExportMethods() && pMutex ){` |
|     64 |  278 | `		UnixMutexResetAfterFork(pMutex);` |
|     64 |  279 | `		return 1;` |
|      - |  280 | `	}` |
|      - |  281 | `#endif` |
|      8 |  282 | `	SXUNUSED(pMethods); SXUNUSED(pMutex);` |
|     16 |  283 | `	return 0;` |
|     40 |  284 | `}` |
|      - |  285 | `#endif /* PH7_ENABLE_THREADS */` |
|      - |  286 |  |
