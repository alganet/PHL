# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 765/1085 lines (70.51%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/* This file implement the public interfaces presented to host-applications.` |
|       - |    8 | ` * Routines in other files are for internal use by PH7 and should not be` |
|       - |    9 | ` * accessed by users of the library.` |
|       - |   10 | ` */` |
|       - |   11 | `#define PH7_ENGINE_MAGIC 0xF874BCD7` |
|       - |   12 | `#define PH7_ENGINE_MISUSE(ENGINE) (ENGINE == 0 \|\| ENGINE->nMagic != PH7_ENGINE_MAGIC)` |
|       - |   13 | `#define PH7_VM_MISUSE(VM) (VM == 0 \|\| VM->nMagic == PH7_VM_STALE)` |
|       - |   14 | `/* If another thread have released a working instance,the following macros` |
|       - |   15 | ` * evaluates to true. These macros are only used when the library` |
|       - |   16 | ` * is built with threading support enabled which is not the case in` |
|       - |   17 | ` * the default built.` |
|       - |   18 | ` */` |
|       - |   19 | `#define PH7_THRD_ENGINE_RELEASE(ENGINE) (ENGINE->nMagic != PH7_ENGINE_MAGIC)` |
|       - |   20 | `#define PH7_THRD_VM_RELEASE(VM) (VM->nMagic == PH7_VM_STALE)` |
|       - |   21 | `/* IMPLEMENTATION: ph7@embedded@symisc 311-12-32 */` |
|       - |   22 | `/*` |
|       - |   23 | ` * All global variables are collected in the structure named "sMPGlobal".` |
|       - |   24 | ` * That way it is clear in the code when we are using static variable because` |
|       - |   25 | ` * its name start with sMPGlobal.` |
|       - |   26 | ` */` |
|       - |   27 | `static struct Global_Data` |
|       - |   28 |  |
|       - |   29 | `	SyMemBackend sAllocator;                /* Global low level memory allocator */` |
|       - |   30 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |   31 | `	const SyMutexMethods *pMutexMethods;   /* Mutex methods */` |
|       - |   32 | `	SyMutex *pMutex;                       /* Global mutex */` |
|       - |   33 | `	sxu32 nThreadingLevel;                 /* Threading level: 0 == Single threaded/1 == Multi-Threaded` |
|       - |   34 | `										    * The threading level can be set using the [ph7_lib_config()]` |
|       - |   35 | `											* interface with a configuration verb set to` |
|       - |   36 | `											* PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE or` |
|       - |   37 | `											* PH7_LIB_CONFIG_THREAD_LEVEL_MULTI` |
|       - |   38 | `											*/` |
|       - |   39 | `#endif` |
|       - |   40 | `	const ph7_vfs *pVfs;                    /* Underlying virtual file system */` |
|       - |   41 | `	sxi32 nEngine;                          /* Total number of active engines */` |
|       - |   42 | `	ph7 *pEngines;                          /* List of active engine */` |
|       - |   43 | `	sxu32 nMagic;                           /* Sanity check against library misuse */` |
|       - |   44 | `}sMPGlobal = {` |
|       - |   45 | `	{0,0,0,0,0,0,0,0,0,{0}},` |
|       - |   46 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |   47 |  |
|       - |   48 |  |
|       - |   49 |  |
|       - |   50 | `#endif` |
|       - |   51 |  |
|       - |   52 |  |
|       - |   53 |  |
|       - |   54 |  |
|       - |   55 | `};` |
|       - |   56 | `#define PH7_LIB_MAGIC  0xEA1495BA` |
|       - |   57 | `#define PH7_LIB_MISUSE (sMPGlobal.nMagic != PH7_LIB_MAGIC)` |
|       - |   58 | `/*` |
|       - |   59 | ` * Supported threading level.` |
|       - |   60 | ` * These options have meaning only when the library is compiled with multi-threading` |
|       - |   61 | ` * support.That is,the PH7_ENABLE_THREADS compile time directive must be defined` |
|       - |   62 | ` * when PH7 is built.` |
|       - |   63 | ` * PH7_THREAD_LEVEL_SINGLE:` |
|       - |   64 | ` * In this mode,mutexing is disabled and the library can only be used by a single thread.` |
|       - |   65 | ` * PH7_THREAD_LEVEL_MULTI` |
|       - |   66 | ` * In this mode, all mutexes including the recursive mutexes on [ph7] objects` |
|       - |   67 | ` * are enabled so that the application is free to share the same engine` |
|       - |   68 | ` * between different threads at the same time.` |
|       - |   69 | ` */` |
|       - |   70 | `#define PH7_THREAD_LEVEL_SINGLE 1` |
|       - |   71 | `#define PH7_THREAD_LEVEL_MULTI  2` |
|       - |   72 | `/*` |
|       - |   73 | ` * Configure a running PH7 engine instance.` |
|       - |   74 | ` * return PH7_OK on success.Any other return` |
|       - |   75 | ` * value indicates failure.` |
|       - |   76 | ` * Refer to [ph7_config()].` |
|       - |   77 | ` */` |
|    6284 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       5 |   79 |  |
|    6289 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    6289 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    6289 |   83 | `	switch(nOp){` |
|    3142 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    6289 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    6289 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    6289 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    6289 |   93 | `		pConf->xErr     = xConsumer;` |
|    6289 |   94 | `		pConf->pErrData = pUserData;` |
|    6289 |   95 | `		break;` |
|       - |   96 | `									 }` |
|     ! 0 |   97 | `	case PH7_CONFIG_ERR_LOG:{` |
|       - |   98 | `		/* Extract compile-time error log if any */` |
|     ! 0 |   99 | `		const char **pzPtr = va_arg(ap,const char **);` |
|     ! 0 |  100 | `		int *pLen = va_arg(ap,int *);` |
|     ! 0 |  101 | `		if( pzPtr == 0 ){` |
|     ! 0 |  102 | `			rc = PH7_CORRUPT;` |
|     ! 0 |  103 | `			break;` |
|       - |  104 | `		}` |
|       - |  105 | `		/* NULL terminate the error-log buffer */` |
|     ! 0 |  106 | `		SyBlobNullAppend(&pConf->sErrConsumer);` |
|       - |  107 | `		/* Point to the error-log buffer */` |
|     ! 0 |  108 | `		*pzPtr = (const char *)SyBlobData(&pConf->sErrConsumer);` |
|     ! 0 |  109 | `		if( pLen ){` |
|     ! 0 |  110 | `			if( SyBlobLength(&pConf->sErrConsumer) > 1 /* NULL '\0' terminator */ ){` |
|     ! 0 |  111 | `				*pLen = (int)SyBlobLength(&pConf->sErrConsumer);` |
|     ! 0 |  112 | `			}else{` |
|     ! 0 |  113 | `				*pLen = 0;` |
|       - |  114 | `			}` |
|     ! 0 |  115 | `		}` |
|     ! 0 |  116 | `		break;` |
|       - |  117 | `							}` |
|     ! 0 |  118 | `	case PH7_CONFIG_ERR_ABORT:` |
|       - |  119 | `		/* Reserved for future use */` |
|     ! 0 |  120 | `		break;` |
|     ! 0 |  121 | `	case PH7_CONFIG_MAX_ALLOC: {` |
|       - |  122 | `		/* Per-allocation cap in bytes (0 = unlimited). VMs created afterwards` |
|       - |  123 | `		 * inherit it via SyMemBackendInitFromParent. Primarily a test/embedding` |
|       - |  124 | `		 * knob to exercise out-of-memory paths deterministically. */` |
|     ! 0 |  125 | `		unsigned int nMax = va_arg(ap,unsigned int);` |
|     ! 0 |  126 | `		pEngine->sAllocator.nMaxRequest = (sxu32)nMax;` |
|     ! 0 |  127 | `		break;` |
|       - |  128 | `								}` |
|     ! 0 |  129 | `	case PH7_CONFIG_CLOCK: {` |
|       - |  130 | `		/* Optional embedder clock used by microtime()/gettimeofday(). The` |
|       - |  131 | `		 * callback fills epoch seconds + microseconds; NULL restores the` |
|       - |  132 | `		 * platform default. Inherited by VMs created afterwards. */` |
|     ! 0 |  133 | `		ph7_clock xClock = va_arg(ap,ph7_clock);` |
|     ! 0 |  134 | `		void *pUserData  = va_arg(ap,void *);` |
|     ! 0 |  135 | `		pEngine->xConf.xClock     = xClock;` |
|     ! 0 |  136 | `		pEngine->xConf.pClockData = pUserData;` |
|     ! 0 |  137 | `		break;` |
|       - |  138 | `							}` |
|     ! 0 |  139 | `	default:` |
|       - |  140 | `		/* Unknown configuration verb */` |
|     ! 0 |  141 | `		rc = PH7_CORRUPT;` |
|     ! 0 |  142 | `		break;` |
|       - |  143 | `	} /* Switch() */` |
|    6289 |  144 | `	return rc;` |
|       5 |  145 |  |
|       - |  146 | `/*` |
|       - |  147 | ` * Configure the PH7 library.` |
|       - |  148 | ` * return PH7_OK on success.Any other return value` |
|       - |  149 | ` * indicates failure.` |
|       - |  150 | ` * Refer to [ph7_lib_config()].` |
|       - |  151 | ` */` |
|    9456 |  152 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       5 |  153 |  |
|    9461 |  154 | `	int rc = PH7_OK;` |
|    9461 |  155 | `	switch(nOp){` |
|    1576 |  156 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  157 | `			/* Install a virtual file system */` |
|    3157 |  158 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    3157 |  159 | `			sMPGlobal.pVfs = pVfs;` |
|    3157 |  160 | `			break;` |
|       - |  161 | `								}` |
|    1576 |  162 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  163 | `			/* Use an alternative low-level memory allocation routines */` |
|    3157 |  164 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  165 | `			/* Save the memory failure callback (if available) */` |
|    3157 |  166 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    3157 |  167 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    3157 |  168 | `			if( pMethods == 0 ){` |
|       - |  169 | `				/* Use the built-in memory allocation subsystem */` |
|    3157 |  170 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    1581 |  171 | `			}else{` |
|     ! 0 |  172 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  173 | `			}` |
|    3157 |  174 | `			break;` |
|       - |  175 | `										  }` |
|     ! 0 |  176 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  177 | `			/* Memory failure callback */` |
|     ! 0 |  178 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  179 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  180 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  181 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  182 | `			break;` |
|       - |  183 | `												 }` |
|    1576 |  184 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  185 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  186 | `			/* Use an alternative low-level mutex subsystem */` |
|    3157 |  187 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  188 | `#if defined (UNTRUST)` |
|       - |  189 | `			if( pMethods == 0 ){` |
|       - |  190 | `				rc = PH7_CORRUPT;` |
|       - |  191 | `			}` |
|       - |  192 | `#endif` |
|       - |  193 | `			/* Sanity check */` |
|    3157 |  194 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  195 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  196 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  197 | `				break;` |
|       - |  198 | `			}` |
|    3157 |  199 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  200 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  201 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  202 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  203 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  204 | `				}` |
|     ! 0 |  205 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  206 | `			}` |
|       - |  207 | `			/* Initialize and install the new mutex subsystem */` |
|    3157 |  208 | `			if( pMethods->xGlobalInit ){` |
|       5 |  209 | `				rc = pMethods->xGlobalInit();` |
|       5 |  210 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  211 | `					break;` |
|       - |  212 | `				}` |
|     ! 0 |  213 | `			}` |
|       - |  214 | `			/* Create the global mutex */` |
|    3157 |  215 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    3157 |  216 | `			if( sMPGlobal.pMutex == 0 ){` |
|       - |  217 | `				/*` |
|       - |  218 | `				 * If the supplied mutex subsystem is so sick that we are unable to` |
|       - |  219 | `				 * create a single mutex,there is no much we can do here.` |
|       - |  220 | `				 */` |
|     ! 0 |  221 | `				if( pMethods->xGlobalRelease ){` |
|     ! 0 |  222 | `					pMethods->xGlobalRelease();` |
|     ! 0 |  223 | `				}` |
|     ! 0 |  224 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  225 | `				break;` |
|       - |  226 | `			}` |
|    3157 |  227 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    3157 |  228 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  229 | `				/* Set a default threading level */` |
|    3157 |  230 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    1576 |  231 | `			}` |
|       - |  232 | `#endif` |
|    3157 |  233 | `			break;` |
|       - |  234 | `										   }` |
|     ! 0 |  235 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:` |
|       - |  236 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  237 | `			/* Single thread mode(Only one thread is allowed to play with the library) */` |
|     ! 0 |  238 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|       - |  239 | `#endif` |
|     ! 0 |  240 | `			break;` |
|     ! 0 |  241 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:` |
|       - |  242 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  243 | `			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines` |
|       - |  244 | `			 * may be shared between multiple threads).` |
|       - |  245 | `			 */` |
|     ! 0 |  246 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|       - |  247 | `#endif` |
|     ! 0 |  248 | `			break;` |
|     ! 0 |  249 | `		default:` |
|       - |  250 | `			/* Unknown configuration option */` |
|     ! 0 |  251 | `			rc = PH7_CORRUPT;` |
|     ! 0 |  252 | `			break;` |
|       - |  253 | `	}` |
|    9461 |  254 | `	return rc;` |
|       5 |  255 |  |
|       - |  256 | `/*` |
|       - |  257 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  258 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  259 | ` */` |
|    9456 |  260 | `int ph7_lib_config(int nConfigOp,...)` |
|       5 |  261 |  |
|       - |  262 | `	va_list ap;` |
|       - |  263 | `	int rc;` |
|       - |  264 |  |
|    9461 |  265 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  266 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  267 | `		return PH7_LOOKED;` |
|       - |  268 | `	}` |
|    9461 |  269 | `	va_start(ap,nConfigOp);` |
|    9461 |  270 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    9461 |  271 | `	va_end(ap);` |
|    9461 |  272 | `	return rc;` |
|    4733 |  273 |  |
|       - |  274 | `/*` |
|       - |  275 | ` * Global library initialization` |
|       - |  276 | ` * Refer to [ph7_lib_init()]` |
|       - |  277 | ` * This routine must be called to initialize the memory allocation subsystem,the mutex` |
|       - |  278 | ` * subsystem prior to doing any serious work with the library.The first thread to call` |
|       - |  279 | ` * this routine does the initialization process and set the magic number so no body later` |
|       - |  280 | ` * can re-initialize the library.If subsequent threads call this  routine before the first` |
|       - |  281 | ` * thread have finished the initialization process, then the subsequent threads must block` |
|       - |  282 | ` * until the initialization process is done.` |
|       - |  283 | ` */` |
|    3152 |  284 | `static sxi32 PH7CoreInitialize(void)` |
|       5 |  285 |  |
|       - |  286 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  287 | `#if defined(PH7_ENABLE_THREADS)` |
|    3157 |  288 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    3157 |  289 | `	SyMutex *pMaster = 0;` |
|       - |  290 | `#endif` |
|       - |  291 | `	int rc;` |
|       - |  292 | `	/*` |
|       - |  293 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  294 | `	 * is a no-op.` |
|       - |  295 | `	 */` |
|    3157 |  296 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  297 | `		return PH7_OK; /* Already initialized */` |
|       - |  298 | `	}` |
|       - |  299 | `	/* Point to the built-in vfs */` |
|    3157 |  300 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  301 | `	/* Install it */` |
|    3157 |  302 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  303 | `#if defined(PH7_ENABLE_THREADS)` |
|    3157 |  304 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    3157 |  305 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    3157 |  306 | `		if( pMutexMethods == 0 ){` |
|       - |  307 | `			/* Use the built-in mutex subsystem */` |
|    3157 |  308 | `			pMutexMethods = SyMutexExportMethods();` |
|    3157 |  309 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  310 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  311 | `			}` |
|       - |  312 | `			/* Install the mutex subsystem */` |
|    3157 |  313 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    3157 |  314 | `			if( rc != PH7_OK ){` |
|     ! 0 |  315 | `				return rc;` |
|       - |  316 | `			}` |
|    1576 |  317 | `		}` |
|       - |  318 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    3157 |  319 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    3157 |  320 | `		if( pMaster == 0 ){` |
|     ! 0 |  321 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  322 | `		}` |
|    1576 |  323 | `	}` |
|       - |  324 | `	/* Lock the master mutex */` |
|    3157 |  325 | `	rc = PH7_OK;` |
|    3157 |  326 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    4733 |  327 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  328 | `#endif` |
|    3157 |  329 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  330 | `			/* Install a memory subsystem */` |
|    3157 |  331 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    3157 |  332 | `			if( rc != PH7_OK ){` |
|       - |  333 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  334 | `				goto End;` |
|       - |  335 | `			}` |
|    1576 |  336 | `		}` |
|       - |  337 | `#if defined(PH7_ENABLE_THREADS)` |
|    3157 |  338 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  339 | `			/* Protect the memory allocation subsystem */` |
|    3157 |  340 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    3157 |  341 | `			if( rc != PH7_OK ){` |
|     ! 0 |  342 | `				goto End;` |
|       - |  343 | `			}` |
|    1576 |  344 | `		}` |
|       - |  345 | `#endif` |
|       - |  346 | `		/* Our library is initialized,set the magic number */` |
|    3157 |  347 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    3157 |  348 | `		rc = PH7_OK;` |
|       - |  349 | `#if defined(PH7_ENABLE_THREADS)` |
|    1576 |  350 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  351 | `#endif` |
|     ! 0 |  352 | `End:` |
|       - |  353 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  354 | `	/* Unlock the master mutex */` |
|    3157 |  355 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  356 | `#endif` |
|    3157 |  357 | `	return rc;` |
|    1581 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * [CAPIREF: ph7_lib_init()]` |
|       - |  361 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  362 | ` */` |
|     ! 0 |  363 | `int ph7_lib_init(void)` |
|     ! 0 |  364 |  |
|       - |  365 | `	int rc;` |
|     ! 0 |  366 | `	rc = PH7CoreInitialize();` |
|     ! 0 |  367 | `	return rc;` |
|     ! 0 |  368 |  |
|       - |  369 | `/*` |
|       - |  370 | ` * Release an active PH7 engine and it's associated active virtual machines.` |
|       - |  371 | ` */` |
|    3152 |  372 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       5 |  373 |  |
|       - |  374 | `	ph7_vm *pVm,*pNext;` |
|       - |  375 | `	/* Release all active VM */` |
|    3157 |  376 | `	pVm = pEngine->pVms;` |
|    1576 |  377 | `	for(;;){` |
|    3157 |  378 | `		if( pEngine->iVm <= 0 ){` |
|    3157 |  379 | `			break;` |
|       - |  380 | `		}` |
|     ! 0 |  381 | `		pNext = pVm->pNext;` |
|     ! 0 |  382 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  383 | `		pVm = pNext;` |
|     ! 0 |  384 | `		pEngine->iVm--;` |
|     ! 0 |  385 | `	}` |
|       - |  386 | `	/* Set a dummy magic number */` |
|    3157 |  387 | `	pEngine->nMagic = 0x7635;` |
|       - |  388 | `	/* Release the private memory subsystem */` |
|    3157 |  389 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    3157 |  390 | `	return PH7_OK;` |
|       5 |  391 |  |
|       - |  392 | `/*` |
|       - |  393 | ` * Release all resources consumed by the library.` |
|       - |  394 | ` * If PH7 is already shut down when this routine` |
|       - |  395 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  396 | ` * Note: This call is not thread safe.` |
|       - |  397 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  398 | ` */` |
|     314 |  399 | `static void PH7CoreShutdown(void)` |
|       4 |  400 |  |
|       - |  401 | `	ph7 *pEngine,*pNext;` |
|       - |  402 | `	/* Release all active engines first */` |
|     318 |  403 | `	pEngine = sMPGlobal.pEngines;` |
|     314 |  404 | `	for(;;){` |
|     632 |  405 | `		if( sMPGlobal.nEngine < 1 ){` |
|     318 |  406 | `			break;` |
|       - |  407 | `		}` |
|     318 |  408 | `		pNext = pEngine->pNext;` |
|     318 |  409 | `		EngineRelease(pEngine);` |
|     318 |  410 | `		pEngine = pNext;` |
|     318 |  411 | `		sMPGlobal.nEngine--;` |
|       4 |  412 | `	}` |
|       - |  413 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  414 | `	/* Release the mutex subsystem */` |
|     318 |  415 | `	if( sMPGlobal.pMutexMethods ){` |
|     318 |  416 | `		if( sMPGlobal.pMutex ){` |
|     318 |  417 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     318 |  418 | `			sMPGlobal.pMutex = 0;` |
|     157 |  419 | `		}` |
|     318 |  420 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       4 |  421 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  422 | `		}` |
|     318 |  423 | `		sMPGlobal.pMutexMethods = 0;` |
|     157 |  424 | `	}` |
|     318 |  425 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  426 | `#endif` |
|     318 |  427 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  428 | `		/* Release the memory backend */` |
|     318 |  429 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     157 |  430 | `	}` |
|     318 |  431 | `	sMPGlobal.nMagic = 0x1928;` |
|     318 |  432 |  |
|       - |  433 | `/*` |
|       - |  434 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  435 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  436 | ` */` |
|     314 |  437 | `int ph7_lib_shutdown(void)` |
|       4 |  438 |  |
|     318 |  439 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  440 | `		/* Already shut */` |
|     ! 0 |  441 | `		return PH7_OK;` |
|       - |  442 | `	}` |
|     318 |  443 | `	PH7CoreShutdown();` |
|     318 |  444 | `	return PH7_OK;` |
|     161 |  445 |  |
|       - |  446 | `/*` |
|       - |  447 | ` * [CAPIREF: ph7_lib_is_threadsafe()]` |
|       - |  448 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  449 | ` */` |
|     ! 0 |  450 | `int ph7_lib_is_threadsafe(void)` |
|     ! 0 |  451 |  |
|     ! 0 |  452 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|     ! 0 |  453 | `		return 0;` |
|       - |  454 | `	}` |
|       - |  455 | `#if defined(PH7_ENABLE_THREADS)` |
|     ! 0 |  456 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  457 | `			/* Muli-threading support is enabled */` |
|     ! 0 |  458 | `			return 1;` |
|     ! 0 |  459 | `		}else{` |
|       - |  460 | `			/* Single-threading */` |
|     ! 0 |  461 | `			return 0;` |
|       - |  462 | `		}` |
|       - |  463 | `#else` |
|       - |  464 | `	return 0;` |
|       - |  465 | `#endif` |
|     ! 0 |  466 |  |
|       - |  467 | `/*` |
|       - |  468 | ` * [CAPIREF: ph7_lib_version()]` |
|       - |  469 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  470 | ` */` |
|      10 |  471 | `const char * ph7_lib_version(void)` |
|       5 |  472 |  |
|      15 |  473 | `	return PH7_VERSION;` |
|       5 |  474 |  |
|       - |  475 | `/*` |
|       - |  476 | ` * [CAPIREF: ph7_lib_signature()]` |
|       - |  477 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  478 | ` */` |
|      10 |  479 | `const char * ph7_lib_signature(void)` |
|       1 |  480 |  |
|      11 |  481 | `	return PH7_SIG;` |
|       1 |  482 |  |
|       - |  483 | `/*` |
|       - |  484 | ` * [CAPIREF: ph7_lib_ident()]` |
|       - |  485 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  486 | ` */` |
|       2 |  487 | `const char * ph7_lib_ident(void)` |
|       1 |  488 |  |
|       3 |  489 | `	return PH7_IDENT;` |
|       1 |  490 |  |
|       - |  491 | `/*` |
|       - |  492 | ` * [CAPIREF: ph7_lib_copyright()]` |
|       - |  493 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  494 | ` */` |
|     ! 0 |  495 | `const char * ph7_lib_copyright(void)` |
|     ! 0 |  496 |  |
|     ! 0 |  497 | `	return PH7_COPYRIGHT;` |
|     ! 0 |  498 |  |
|       - |  499 | `/*` |
|       - |  500 | ` * [CAPIREF: ph7_config()]` |
|       - |  501 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  502 | ` */` |
|    6284 |  503 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       5 |  504 |  |
|       - |  505 | `	va_list ap;` |
|       - |  506 | `	int rc;` |
|    6289 |  507 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  508 | `		return PH7_CORRUPT;` |
|       - |  509 | `	}` |
|       - |  510 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  511 | `	 /* Acquire engine mutex */` |
|    6289 |  512 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    6289 |  513 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    6284 |  514 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  515 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  516 | `	 }` |
|       - |  517 | `#endif` |
|    6289 |  518 | `	 va_start(ap,nConfigOp);` |
|    6289 |  519 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    6289 |  520 | `	 va_end(ap);` |
|       - |  521 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  522 | `	 /* Leave engine mutex */` |
|    6289 |  523 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  524 | `#endif` |
|    6289 |  525 | `	return rc;` |
|    3147 |  526 |  |
|       - |  527 | `/*` |
|       - |  528 | ` * [CAPIREF: ph7_init()]` |
|       - |  529 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  530 | ` */` |
|    3152 |  531 | `int ph7_init(ph7 **ppEngine)` |
|       5 |  532 |  |
|       - |  533 | `	ph7 *pEngine;` |
|       - |  534 | `	int rc;` |
|       - |  535 | `#if defined(UNTRUST)` |
|       - |  536 | `	if( ppEngine == 0 ){` |
|       - |  537 | `		return PH7_CORRUPT;` |
|       - |  538 | `	}` |
|       - |  539 | `#endif` |
|    3157 |  540 | `	*ppEngine = 0;` |
|       - |  541 | `	/* One-time automatic library initialization */` |
|    3157 |  542 | `	rc = PH7CoreInitialize();` |
|    3157 |  543 | `	if( rc != PH7_OK ){` |
|     ! 0 |  544 | `		return rc;` |
|       - |  545 | `	}` |
|       - |  546 | `	/* Allocate a new engine */` |
|    3157 |  547 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    3157 |  548 | `	if( pEngine == 0 ){` |
|     ! 0 |  549 | `		return PH7_NOMEM;` |
|       - |  550 | `	}` |
|       - |  551 | `	/* Zero the structure */` |
|    3157 |  552 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  553 | `	/* Initialize engine fields */` |
|    3157 |  554 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    3157 |  555 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    3157 |  556 | `	if( rc != PH7_OK ){` |
|     ! 0 |  557 | `		goto Release;` |
|       - |  558 | `	}` |
|       - |  559 | `#if defined(PH7_ENABLE_THREADS)` |
|    3157 |  560 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  561 | `#endif` |
|       - |  562 | `	/* Default configuration */` |
|    3157 |  563 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  564 | `	/* Install a default compile-time error consumer routine */` |
|    3157 |  565 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  566 | `	/* Built-in vfs */` |
|    3157 |  567 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  568 | `#if defined(PH7_ENABLE_THREADS)` |
|    3157 |  569 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  570 | `		 /* Associate a recursive mutex with this instance */` |
|    3157 |  571 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3157 |  572 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  573 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  574 | `			 goto Release;` |
|       - |  575 | `		 }` |
|    1576 |  576 | `	 }` |
|       - |  577 | `#endif` |
|       - |  578 | `	/* Link to the list of active engines */` |
|       - |  579 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  580 | `	/* Enter the global mutex */` |
|    3157 |  581 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  582 | `#endif` |
|    3157 |  583 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    3157 |  584 | `	sMPGlobal.nEngine++;` |
|       - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  586 | `	/* Leave the global mutex */` |
|    3157 |  587 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  588 | `#endif` |
|       - |  589 | `	/* Write a pointer to the new instance */` |
|    3157 |  590 | `	*ppEngine = pEngine;` |
|    3157 |  591 | `	return PH7_OK;` |
|     ! 0 |  592 | `Release:` |
|     ! 0 |  593 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  594 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  595 | `	return rc;` |
|    1581 |  596 |  |
|       - |  597 | `/*` |
|       - |  598 | ` * [CAPIREF: ph7_release()]` |
|       - |  599 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  600 | ` */` |
|    2838 |  601 | `int ph7_release(ph7 *pEngine)` |
|       5 |  602 |  |
|       - |  603 | `	int rc;` |
|    2843 |  604 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  605 | `		return PH7_CORRUPT;` |
|       - |  606 | `	}` |
|       - |  607 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  608 | `	 /* Acquire engine mutex */` |
|    2843 |  609 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2843 |  610 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2838 |  611 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  612 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  613 | `	 }` |
|       - |  614 | `#endif` |
|       - |  615 | `	/* Release the engine */` |
|    2843 |  616 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  617 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  618 | `	 /* Leave engine mutex */` |
|    2843 |  619 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  620 | `	 /* Release engine mutex */` |
|    2843 |  621 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  622 | `#endif` |
|       - |  623 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  624 | `	/* Enter the global mutex */` |
|    2843 |  625 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  626 | `#endif` |
|       - |  627 | `	/* Unlink from the list of active engines */` |
|    2843 |  628 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    2843 |  629 | `	sMPGlobal.nEngine--;` |
|       - |  630 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  631 | `	/* Leave the global mutex */` |
|    2843 |  632 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  633 | `#endif` |
|       - |  634 | `	/* Release the memory chunk allocated to this engine */` |
|    2843 |  635 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    2843 |  636 | `	return rc;` |
|    1424 |  637 |  |
|       - |  638 | `/*` |
|       - |  639 | ` * Compile a raw PHP script.` |
|       - |  640 | ` * To execute a PHP code, it must first be compiled into a byte-code program using this routine.` |
|       - |  641 | ` * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]` |
|       - |  642 | ` * should  display the appropriate error message and this function set ppVm to null and return` |
|       - |  643 | ` * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled` |
|       - |  644 | ` * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].` |
|       - |  645 | ` * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script` |
|       - |  646 | ` * for evaluation.` |
|       - |  647 | ` */` |
|    3148 |  648 | `static sxi32 ProcessScript(` |
|       - |  649 | `	ph7 *pEngine,          /* Running PH7 engine */` |
|       - |  650 | `	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */` |
|       - |  651 | `	SyString *pScript,     /* Raw PHP script to compile */` |
|       - |  652 | `	sxi32 iFlags,          /* Compile-time flags */` |
|       - |  653 | `	const char *zFilePath  /* File path if script come from a file. NULL otherwise */` |
|       - |  654 | `	)` |
|       5 |  655 |  |
|       - |  656 | `	ph7_vm *pVm;` |
|       - |  657 | `	int rc;` |
|       - |  658 | `	/* Allocate a new virtual machine */` |
|    3153 |  659 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    3153 |  660 | `	if( pVm == 0 ){` |
|       - |  661 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  662 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  663 | `		if( ppVm ){` |
|     ! 0 |  664 | `			*ppVm = 0;` |
|     ! 0 |  665 | `		}` |
|     ! 0 |  666 | `		return PH7_NOMEM;` |
|       - |  667 | `	}` |
|    3153 |  668 | `	if( iFlags < 0 ){` |
|       - |  669 | `		/* Default compile-time flags */` |
|     ! 0 |  670 | `		iFlags = 0;` |
|     ! 0 |  671 | `	}` |
|       - |  672 | `	/* Initialize the Virtual Machine */` |
|    3153 |  673 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    3153 |  674 | `	if( rc != PH7_OK ){` |
|     ! 0 |  675 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  676 | `		if( ppVm ){` |
|     ! 0 |  677 | `			*ppVm = 0;` |
|     ! 0 |  678 | `		}` |
|     ! 0 |  679 | `		return PH7_VM_ERR;` |
|       - |  680 | `	}` |
|    3153 |  681 | `	if( zFilePath ){` |
|       - |  682 | `		/* Push processed file path */` |
|    3145 |  683 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    1570 |  684 | `	}` |
|       - |  685 | `	/* Reset the error message consumer */` |
|    3153 |  686 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  687 | `	/* Compile the script */` |
|    3153 |  688 | `	PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    3153 |  689 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     318 |  690 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  691 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     318 |  692 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     318 |  693 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     318 |  694 | `		if( ppVm ){` |
|     318 |  695 | `			*ppVm = 0;` |
|     157 |  696 | `		}` |
|     318 |  697 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  698 | `	}` |
|       - |  699 | `	/* Prepare the virtual machine for bytecode execution */` |
|    2839 |  700 | `	rc = PH7_VmMakeReady(pVm);` |
|    2839 |  701 | `	if( rc != PH7_OK ){` |
|     ! 0 |  702 | `		goto Release;` |
|       - |  703 | `	}` |
|       - |  704 | `	/* Install local import path which is the current directory */` |
|    2839 |  705 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  706 | `#if defined(PH7_ENABLE_THREADS)` |
|    2839 |  707 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  708 | `		 /* Associate a recursive mutex with this instance */` |
|    2839 |  709 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    2839 |  710 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  711 | `			 goto Release;` |
|       - |  712 | `		 }` |
|    1417 |  713 | `	 }` |
|       - |  714 | `#endif` |
|       - |  715 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    2839 |  716 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    2839 |  717 | `	pEngine->iVm++;` |
|       - |  718 | `	/* Point to the freshly created VM */` |
|    2839 |  719 | `	*ppVm = pVm;` |
|       - |  720 | `	/* Ready to execute PH7 bytecode */` |
|    2839 |  721 | `	return PH7_OK;` |
|     ! 0 |  722 | `Release:` |
|     ! 0 |  723 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     ! 0 |  724 | `	SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  725 | `	*ppVm = 0;` |
|     ! 0 |  726 | `	return PH7_VM_ERR;` |
|    1579 |  727 |  |
|       - |  728 | `/*` |
|       - |  729 | ` * [CAPIREF: ph7_compile()]` |
|       - |  730 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  731 | ` */` |
|     ! 0 |  732 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|     ! 0 |  733 |  |
|       - |  734 | `	SyString sScript;` |
|       - |  735 | `	int rc;` |
|     ! 0 |  736 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  737 | `		return PH7_CORRUPT;` |
|       - |  738 | `	}` |
|     ! 0 |  739 | `	if( nLen < 0 ){` |
|       - |  740 | `		/* Compute input length automatically */` |
|     ! 0 |  741 | `		nLen = (int)SyStrlen(zSource);` |
|     ! 0 |  742 | `	}` |
|     ! 0 |  743 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  744 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  745 | `	 /* Acquire engine mutex */` |
|     ! 0 |  746 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  747 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  748 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  749 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  750 | `	 }` |
|       - |  751 | `#endif` |
|       - |  752 | `	/* Compile the script */` |
|     ! 0 |  753 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|       - |  754 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  755 | `	 /* Leave engine mutex */` |
|     ! 0 |  756 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  757 | `#endif` |
|       - |  758 | `	/* Compilation result */` |
|     ! 0 |  759 | `	return rc;` |
|     ! 0 |  760 |  |
|       - |  761 | `/*` |
|       - |  762 | ` * [CAPIREF: ph7_compile_v2()]` |
|       - |  763 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  764 | ` */` |
|       8 |  765 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       2 |  766 |  |
|       - |  767 | `	SyString sScript;` |
|       - |  768 | `	int rc;` |
|      10 |  769 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  770 | `		return PH7_CORRUPT;` |
|       - |  771 | `	}` |
|      10 |  772 | `	if( nLen < 0 ){` |
|       - |  773 | `		/* Compute input length automatically */` |
|      10 |  774 | `		nLen = (int)SyStrlen(zSource);` |
|       4 |  775 | `	}` |
|      10 |  776 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  777 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  778 | `	 /* Acquire engine mutex */` |
|      10 |  779 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      10 |  780 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       8 |  781 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  782 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  783 | `	 }` |
|       - |  784 | `#endif` |
|       - |  785 | `	/* Compile the script */` |
|      10 |  786 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  787 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  788 | `	 /* Leave engine mutex */` |
|      10 |  789 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  790 | `#endif` |
|       - |  791 | `	/* Compilation result */` |
|      10 |  792 | `	return rc;` |
|       6 |  793 |  |
|       - |  794 | `/*` |
|       - |  795 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  796 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  797 | ` */` |
|    3140 |  798 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       5 |  799 |  |
|       - |  800 | `	const ph7_vfs *pVfs;` |
|       - |  801 | `	int rc;` |
|    3145 |  802 | `	if( ppOutVm ){` |
|    3145 |  803 | `		*ppOutVm = 0;` |
|    1570 |  804 | `	}` |
|    3145 |  805 | `	rc = PH7_OK; /* cc warning */` |
|    3145 |  806 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  807 | `		return PH7_CORRUPT;` |
|       - |  808 | `	}` |
|       - |  809 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  810 | `	 /* Acquire engine mutex */` |
|    3145 |  811 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3145 |  812 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3140 |  813 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  814 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  815 | `	 }` |
|       - |  816 | `#endif` |
|       - |  817 | `	 /*` |
|       - |  818 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  819 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  820 | `	  */` |
|    3145 |  821 | `	 pVfs = pEngine->pVfs;` |
|    3145 |  822 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  823 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  824 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  825 | `	 }else{` |
|    3145 |  826 | `		 void *pMapView = 0; /* cc warning */` |
|    3145 |  827 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  828 | `		 SyString sScript;` |
|       - |  829 | `		 /* Try to get a memory view of the whole file */` |
|    3145 |  830 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    3145 |  831 | `		 if( rc != PH7_OK ){` |
|       - |  832 | `			 /* Assume an IO error */` |
|     ! 0 |  833 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  834 | `		 }else{` |
|       - |  835 | `			 /* Compile the file */` |
|    3145 |  836 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    3145 |  837 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  838 | `			 /* Release the memory view of the whole file */` |
|    3145 |  839 | `			 if( pVfs->xUnmap ){` |
|    3145 |  840 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    1570 |  841 | `			 }` |
|       - |  842 | `		 }` |
|       - |  843 | `	 }` |
|       - |  844 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  845 | `	 /* Leave engine mutex */` |
|    3145 |  846 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  847 | `#endif` |
|       - |  848 | `	/* Compilation result */` |
|    3145 |  849 | `	return rc;` |
|    1575 |  850 |  |
|       - |  851 | `/*` |
|       - |  852 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|       - |  853 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  854 | ` */` |
|       2 |  855 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|       1 |  856 |  |
|       - |  857 | `	int rc;` |
|       - |  858 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       3 |  859 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  860 | `		return PH7_CORRUPT;` |
|       - |  861 | `	}` |
|       - |  862 | `#ifdef UNTRUST` |
|       - |  863 | `	if( xConsumer == 0 ){` |
|       - |  864 | `		return PH7_CORRUPT;` |
|       - |  865 | `	}` |
|       - |  866 | `#endif` |
|       - |  867 | `	/* Dump VM instructions */` |
|       3 |  868 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|       3 |  869 | `	return rc;` |
|       2 |  870 |  |
|       - |  871 | `/*` |
|       - |  872 | ` * [CAPIREF: ph7_vm_config()]` |
|       - |  873 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  874 | ` */` |
|   45872 |  875 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       5 |  876 |  |
|       - |  877 | `	va_list ap;` |
|       - |  878 | `	int rc;` |
|       - |  879 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   45877 |  880 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  881 | `		return PH7_CORRUPT;` |
|       - |  882 | `	}` |
|       - |  883 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  884 | `	 /* Acquire VM mutex */` |
|   45877 |  885 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   45877 |  886 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   45872 |  887 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  888 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  889 | `	 }` |
|       - |  890 | `#endif` |
|       - |  891 | `	/* Confiugure the virtual machine */` |
|   45877 |  892 | `	va_start(ap,iConfigOp);` |
|   45877 |  893 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   45877 |  894 | `	va_end(ap);` |
|       - |  895 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  896 | `	 /* Leave VM mutex */` |
|   45877 |  897 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  898 | `#endif` |
|   45877 |  899 | `	return rc;` |
|   22941 |  900 |  |
|       - |  901 | `/*` |
|       - |  902 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  903 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  904 | ` */` |
|    2840 |  905 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       5 |  906 |  |
|       - |  907 | `	int rc;` |
|       - |  908 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    2845 |  909 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  910 | `		return PH7_CORRUPT;` |
|       - |  911 | `	}` |
|       - |  912 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  913 | `	 /* Acquire VM mutex */` |
|    2845 |  914 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2845 |  915 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2840 |  916 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  917 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  918 | `	 }` |
|       - |  919 | `#endif` |
|       - |  920 | `	/* Execute PH7 byte-code */` |
|    2845 |  921 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    2845 |  922 | `	if( pExitStatus ){` |
|       - |  923 | `		/* Exit status */` |
|    2823 |  924 | `		*pExitStatus = pVm->iExitStatus;` |
|    1409 |  925 | `	}` |
|       - |  926 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  927 | `	 /* Leave VM mutex */` |
|    2845 |  928 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  929 | `#endif` |
|       - |  930 | `	/* Execution result */` |
|    2845 |  931 | `	return rc;` |
|    1425 |  932 |  |
|       - |  933 | `/*` |
|       - |  934 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  935 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  936 | ` */` |
|       6 |  937 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  938 |  |
|       - |  939 | `	int rc;` |
|       - |  940 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       6 |  941 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  942 | `		return PH7_CORRUPT;` |
|       - |  943 | `	}` |
|       - |  944 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  945 | `	 /* Acquire VM mutex */` |
|       6 |  946 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       6 |  947 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       6 |  948 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  949 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  950 | `	 }` |
|       - |  951 | `#endif` |
|       6 |  952 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  953 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  954 | `	 /* Leave VM mutex */` |
|       6 |  955 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  956 | `#endif` |
|       6 |  957 | `	return rc;` |
|       3 |  958 |  |
|       - |  959 | `/*` |
|       - |  960 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  961 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  962 | ` */` |
|    2834 |  963 | `int ph7_vm_release(ph7_vm *pVm)` |
|       5 |  964 |  |
|       - |  965 | `	ph7 *pEngine;` |
|       - |  966 | `	int rc;` |
|       - |  967 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    2839 |  968 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  969 | `		return PH7_CORRUPT;` |
|       - |  970 | `	}` |
|       - |  971 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  972 | `	 /* Acquire VM mutex */` |
|    2839 |  973 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2839 |  974 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2834 |  975 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  976 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  977 | `	 }` |
|       - |  978 | `#endif` |
|    2839 |  979 | `	pEngine = pVm->pEngine;` |
|    2839 |  980 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - |  981 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  982 | `	 /* Leave VM mutex */` |
|    2839 |  983 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  984 | `#endif` |
|    2839 |  985 | `	if( rc == PH7_OK ){` |
|       - |  986 | `		/* Unlink from the list of active VM */` |
|       - |  987 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  988 | `			/* Acquire engine mutex */` |
|    2839 |  989 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2839 |  990 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2834 |  991 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  992 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  993 | `			}` |
|       - |  994 | `#endif` |
|    2839 |  995 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    2839 |  996 | `		pEngine->iVm--;` |
|       - |  997 | `		/* Release the memory chunk allocated to this VM */` |
|    2839 |  998 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - |  999 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1000 | `			/* Leave engine mutex */` |
|    2839 | 1001 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1002 | `#endif` |
|    1417 | 1003 | `	}` |
|    2839 | 1004 | `	return rc;` |
|    1422 | 1005 |  |
|       - | 1006 | `/*` |
|       - | 1007 | ` * [CAPIREF: ph7_create_function()]` |
|       - | 1008 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1009 | ` */` |
| 1405962 | 1010 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       5 | 1011 |  |
|       - | 1012 | `	SyString sName;` |
|       - | 1013 | `	int rc;` |
|       - | 1014 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 1405967 | 1015 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1016 | `		return PH7_CORRUPT;` |
|       - | 1017 | `	}` |
| 1405967 | 1018 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1019 | `	/* Remove leading and trailing white spaces */` |
| 1405967 | 1020 | `	SyStringFullTrim(&sName);` |
|       - | 1021 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 1405967 | 1022 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1023 | `		return PH7_CORRUPT;` |
|       - | 1024 | `	}` |
|       - | 1025 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1026 | `	 /* Acquire VM mutex */` |
| 1405967 | 1027 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 1405967 | 1028 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 1405962 | 1029 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1030 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1031 | `	 }` |
|       - | 1032 | `#endif` |
|       - | 1033 | `	/* Install the foreign function */` |
| 1405967 | 1034 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1035 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1036 | `	 /* Leave VM mutex */` |
| 1405967 | 1037 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1038 | `#endif` |
| 1405967 | 1039 | `	return rc;` |
|  702986 | 1040 |  |
|       - | 1041 | `/*` |
|       - | 1042 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1043 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1044 | ` */` |
|     ! 0 | 1045 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1046 |  |
|     ! 0 | 1047 | `	ph7_user_func *pFunc = 0;` |
|       - | 1048 | `	int rc;` |
|       - | 1049 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1050 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1051 | `		return PH7_CORRUPT;` |
|       - | 1052 | `	}` |
|       - | 1053 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1054 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1055 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1056 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1057 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1058 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1059 | `	 }` |
|       - | 1060 | `#endif` |
|       - | 1061 | `	/* Perform the deletion */` |
|     ! 0 | 1062 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1063 | `	if( rc == PH7_OK ){` |
|       - | 1064 | `		/* Release internal fields */` |
|     ! 0 | 1065 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1066 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1067 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1068 | `	}` |
|       - | 1069 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1070 | `	 /* Leave VM mutex */` |
|     ! 0 | 1071 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1072 | `#endif` |
|     ! 0 | 1073 | `	return rc;` |
|     ! 0 | 1074 |  |
|       - | 1075 | `/*` |
|       - | 1076 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1077 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1078 | ` */` |
|  634814 | 1079 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       5 | 1080 |  |
|       - | 1081 | `	SyString sName;` |
|       - | 1082 | `	int rc;` |
|       - | 1083 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  634819 | 1084 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1085 | `		return PH7_CORRUPT;` |
|       - | 1086 | `	}` |
|  634819 | 1087 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1088 | `	/* Remove leading and trailing white spaces */` |
|  637653 | 1089 | `	SyStringFullTrim(&sName);` |
|  634819 | 1090 | `	if( sName.nByte < 1 ){` |
|       - | 1091 | `		/* Empty constant name */` |
|     ! 0 | 1092 | `		return PH7_CORRUPT;` |
|       - | 1093 | `	}` |
|       - | 1094 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  634819 | 1095 | `	if( xExpand == 0 ){` |
|     ! 0 | 1096 | `		return PH7_CORRUPT;` |
|       - | 1097 | `	}` |
|       - | 1098 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1099 | `	 /* Acquire VM mutex */` |
|  634819 | 1100 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  634819 | 1101 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  634814 | 1102 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1103 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1104 | `	 }` |
|       - | 1105 | `#endif` |
|       - | 1106 | `	/* Perform the registration */` |
|  634819 | 1107 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1108 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1109 | `	 /* Leave VM mutex */` |
|  634819 | 1110 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1111 | `#endif` |
|  634819 | 1112 | `	 return rc;` |
|  317412 | 1113 |  |
|       - | 1114 | `/*` |
|       - | 1115 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1116 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1117 | ` */` |
|     ! 0 | 1118 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1119 |  |
|       - | 1120 | `	ph7_constant *pCons;` |
|       - | 1121 | `	int rc;` |
|       - | 1122 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1123 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1124 | `		return PH7_CORRUPT;` |
|       - | 1125 | `	}` |
|       - | 1126 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1127 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1128 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1129 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1130 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1131 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1132 | `	 }` |
|       - | 1133 | `#endif` |
|       - | 1134 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1135 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1136 | `	 if( rc == PH7_OK ){` |
|       - | 1137 | `		 /* Perform the deletion */` |
|     ! 0 | 1138 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1139 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1140 | `	 }` |
|       - | 1141 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1142 | `	 /* Leave VM mutex */` |
|     ! 0 | 1143 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1144 | `#endif` |
|     ! 0 | 1145 | `	return rc;` |
|     ! 0 | 1146 |  |
|       - | 1147 | `/*` |
|       - | 1148 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1149 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1150 | ` */` |
|    6956 | 1151 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       5 | 1152 |  |
|       - | 1153 | `	ph7_value *pObj;` |
|       - | 1154 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    6961 | 1155 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1156 | `		return 0;` |
|       - | 1157 | `	}` |
|       - | 1158 | `	/* Allocate a new scalar variable */` |
|    6961 | 1159 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    6961 | 1160 | `	if( pObj == 0 ){` |
|     ! 0 | 1161 | `		return 0;` |
|       - | 1162 | `	}` |
|       - | 1163 | `	/* Nullify the new scalar */` |
|    6961 | 1164 | `	PH7_MemObjInit(pVm,pObj);` |
|    6961 | 1165 | `	return pObj;` |
|    3483 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1169 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1170 | ` */` |
|   38186 | 1171 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       5 | 1172 |  |
|       - | 1173 | `	ph7_hashmap *pMap;` |
|       - | 1174 | `	ph7_value *pObj;` |
|       - | 1175 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   38191 | 1176 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1177 | `		return 0;` |
|       - | 1178 | `	}` |
|       - | 1179 | `	/* Create a new hashmap first */` |
|   38191 | 1180 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   38191 | 1181 | `	if( pMap == 0 ){` |
|     ! 0 | 1182 | `		return 0;` |
|       - | 1183 | `	}` |
|       - | 1184 | `	/* Associate a new ph7_value with this hashmap */` |
|   38191 | 1185 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   38191 | 1186 | `	if( pObj == 0 ){` |
|     ! 0 | 1187 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1188 | `		return 0;` |
|       - | 1189 | `	}` |
|   38191 | 1190 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   38191 | 1191 | `	return pObj;` |
|   19098 | 1192 |  |
|       - | 1193 | `/*` |
|       - | 1194 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1195 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1196 | ` */` |
|   28400 | 1197 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1198 |  |
|       - | 1199 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   28405 | 1200 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1201 | `		return PH7_CORRUPT;` |
|       - | 1202 | `	}` |
|   28405 | 1203 | `	if( pValue ){` |
|       - | 1204 | `		/* Release the value */` |
|   28405 | 1205 | `		PH7_MemObjRelease(pValue);` |
|   28405 | 1206 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|   14200 | 1207 | `	}` |
|   28405 | 1208 | `	return PH7_OK;` |
|   14205 | 1209 |  |
|       - | 1210 | `/*` |
|       - | 1211 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1212 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1213 | ` */` |
|  358774 | 1214 | `int ph7_value_to_int(ph7_value *pValue)` |
|       5 | 1215 |  |
|       - | 1216 | `	int rc;` |
|  358779 | 1217 | `	rc = PH7_MemObjToInteger(pValue);` |
|  358779 | 1218 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1219 | `		return 0;` |
|       - | 1220 | `	}` |
|  358779 | 1221 | `	return (int)pValue->x.iVal;` |
|  179392 | 1222 |  |
|       - | 1223 | `/*` |
|       - | 1224 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1225 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1226 | ` */` |
|     310 | 1227 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       5 | 1228 |  |
|       - | 1229 | `	int rc;` |
|     315 | 1230 | `	rc = PH7_MemObjToBool(pValue);` |
|     315 | 1231 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1232 | `		return 0;` |
|       - | 1233 | `	}` |
|     315 | 1234 | `	return (int)pValue->x.iVal;` |
|     160 | 1235 |  |
|       - | 1236 | `/*` |
|       - | 1237 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1238 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1239 | ` */` |
|     690 | 1240 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       3 | 1241 |  |
|       - | 1242 | `	int rc;` |
|     693 | 1243 | `	rc = PH7_MemObjToInteger(pValue);` |
|     693 | 1244 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1245 | `		return 0;` |
|       - | 1246 | `	}` |
|     693 | 1247 | `	return pValue->x.iVal;` |
|     348 | 1248 |  |
|       - | 1249 | `/*` |
|       - | 1250 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1251 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1252 | ` */` |
|     486 | 1253 | `double ph7_value_to_double(ph7_value *pValue)` |
|       1 | 1254 |  |
|       - | 1255 | `	int rc;` |
|     487 | 1256 | `	rc = PH7_MemObjToReal(pValue);` |
|     487 | 1257 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1258 | `		return (double)0;` |
|       - | 1259 | `	}` |
|     487 | 1260 | `	return (double)pValue->rVal;` |
|     244 | 1261 |  |
|       - | 1262 | `/*` |
|       - | 1263 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1264 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1265 | ` */` |
|  672520 | 1266 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       5 | 1267 |  |
|  672525 | 1268 | `	PH7_MemObjToString(pValue);` |
|  672525 | 1269 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  642865 | 1270 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  642865 | 1271 | `		if( pLen ){` |
|  588545 | 1272 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  294292 | 1273 | `		}` |
|  642865 | 1274 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1275 | `	}else{` |
|       - | 1276 | `		/* Return the empty string */` |
|   29665 | 1277 | `		if( pLen ){` |
|   29655 | 1278 | `			*pLen = 0;` |
|   14825 | 1279 | `		}` |
|   29665 | 1280 | `		return "";` |
|       - | 1281 | `	}` |
|  336287 | 1282 |  |
|       - | 1283 | `/*` |
|       - | 1284 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1285 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1286 | ` */` |
|   25550 | 1287 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       5 | 1288 |  |
|   25555 | 1289 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1290 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1291 | `		return 0;` |
|       - | 1292 | `	}` |
|   25555 | 1293 | `	return pValue->x.pOther;` |
|   12780 | 1294 |  |
|       - | 1295 | `/*` |
|       - | 1296 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1297 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1298 | ` */` |
|      30 | 1299 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1300 |  |
|       - | 1301 | `	int rc;` |
|      31 | 1302 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1303 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1304 | `		return 1;` |
|       - | 1305 | `	}` |
|       - | 1306 | `	/* Perform the comparison */` |
|      31 | 1307 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1308 | `	/* Comparison result */` |
|      31 | 1309 | `	return rc;` |
|      16 | 1310 |  |
|       - | 1311 | `/*` |
|       - | 1312 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1313 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1314 | ` */` |
|    9974 | 1315 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       5 | 1316 |  |
|    9979 | 1317 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       5 | 1318 |  |
|       - | 1319 | `/*` |
|       - | 1320 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1321 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1322 | ` */` |
|   14478 | 1323 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       5 | 1324 |  |
|   14483 | 1325 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       5 | 1326 |  |
|       - | 1327 | `/*` |
|       - | 1328 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1329 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1330 | ` */` |
|  316888 | 1331 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       5 | 1332 |  |
|  316893 | 1333 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       5 | 1334 |  |
|       - | 1335 | `/*` |
|       - | 1336 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1337 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1338 | ` */` |
|     428 | 1339 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       1 | 1340 |  |
|     429 | 1341 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       1 | 1342 |  |
|       - | 1343 | `/*` |
|       - | 1344 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1345 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1346 | ` */` |
|     122 | 1347 | `int ph7_result_null(ph7_context *pCtx)` |
|       4 | 1348 |  |
|       - | 1349 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     126 | 1350 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     126 | 1351 | `	return PH7_OK;` |
|       4 | 1352 |  |
|       - | 1353 | `/*` |
|       - | 1354 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1355 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1356 | ` */` |
|  858036 | 1357 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1358 |  |
|  858041 | 1359 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       5 | 1360 |  |
|       - | 1361 | `/*` |
|       - | 1362 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1363 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1364 | ` */` |
|     282 | 1365 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1366 |  |
|       - | 1367 | `	ph7_value *p;` |
|       - | 1368 | `	va_list ap;` |
|       - | 1369 | `	int rc;` |
|     283 | 1370 | `	p = pCtx->pRet;` |
|     283 | 1371 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1372 | `		/* Invalidate any prior representation */` |
|     143 | 1373 | `		PH7_MemObjRelease(p);` |
|     143 | 1374 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|      71 | 1375 | `	}` |
|       - | 1376 | `	/* Format the given string */` |
|     283 | 1377 | `	va_start(ap,zFormat);` |
|     283 | 1378 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|     283 | 1379 | `	va_end(ap);` |
|     283 | 1380 | `	return rc;` |
|       1 | 1381 |  |
|       - | 1382 | `/*` |
|       - | 1383 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1384 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1385 | ` */` |
|   29638 | 1386 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1387 |  |
|   29643 | 1388 | `	int rc = PH7_OK;` |
|   29643 | 1389 | `	if( pValue == 0 ){` |
|     ! 0 | 1390 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1391 | `	}else{` |
|   29643 | 1392 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1393 | `	}` |
|   29643 | 1394 | `	return rc;` |
|       5 | 1395 |  |
|       - | 1396 | `/*` |
|       - | 1397 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1398 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1399 | ` */` |
|    4342 | 1400 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       5 | 1401 |  |
|    4347 | 1402 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       5 | 1403 |  |
|       - | 1404 | `/*` |
|       - | 1405 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1406 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1407 | ` */` |
|    6956 | 1408 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       5 | 1409 |  |
|       - | 1410 | `	ph7_value *pVal;` |
|    6961 | 1411 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|    6961 | 1412 | `	if( pVal ){` |
|       - | 1413 | `		/* Record value address so it can be freed automatically` |
|       - | 1414 | `		 * when the calling function returns.` |
|       - | 1415 | `		 */` |
|    6961 | 1416 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    3478 | 1417 | `	}` |
|    6961 | 1418 | `	return pVal;` |
|       5 | 1419 |  |
|       - | 1420 | `/*` |
|       - | 1421 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1422 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1423 | ` */` |
|    9786 | 1424 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       5 | 1425 |  |
|       - | 1426 | `	ph7_value *pVal;` |
|    9791 | 1427 | `	pVal = ph7_new_array(pCtx->pVm);` |
|    9791 | 1428 | `	if( pVal ){` |
|       - | 1429 | `		/* Record value address so it can be freed automatically` |
|       - | 1430 | `		 * when the calling function returns.` |
|       - | 1431 | `		 */` |
|    9791 | 1432 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    4893 | 1433 | `	}` |
|    9791 | 1434 | `	return pVal;` |
|       5 | 1435 |  |
|       - | 1436 | `/*` |
|       - | 1437 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1438 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1439 | ` */` |
|     382 | 1440 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1441 |  |
|     387 | 1442 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     387 | 1443 |  |
|       - | 1444 | `/*` |
|       - | 1445 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1446 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1447 | ` */` |
|    4312 | 1448 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       5 | 1449 |  |
|       - | 1450 | `	void *pChunk;` |
|    4317 | 1451 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    4317 | 1452 | `	if( pChunk ){` |
|    4317 | 1453 | `		if( ZeroChunk ){` |
|       - | 1454 | `			/* Zero the memory chunk */` |
|    4283 | 1455 | `			SyZero(pChunk,nByte);` |
|    2139 | 1456 | `		}` |
|    4317 | 1457 | `		if( AutoRelease ){` |
|       - | 1458 | `			ph7_aux_data sAux;` |
|       - | 1459 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1460 | `			 * upon this context is destroyed.` |
|       - | 1461 | `			 */` |
|      25 | 1462 | `			sAux.pAuxData = pChunk;` |
|      25 | 1463 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|      12 | 1464 | `		}` |
|    2156 | 1465 | `	}` |
|    4317 | 1466 | `	return pChunk;` |
|       5 | 1467 |  |
|       - | 1468 | `/*` |
|       - | 1469 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1470 | ` * chunk container.` |
|       - | 1471 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1472 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1473 | ` */` |
|    4262 | 1474 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1475 |  |
|       - | 1476 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1477 | `	sxu32 n;` |
|    4267 | 1478 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1479 | `		/* Don't bother processing,the container is empty */` |
|    4267 | 1480 | `		return 0;` |
|       - | 1481 | `	}` |
|       - | 1482 | `	/* Perform the lookup */` |
|     ! 0 | 1483 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1484 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1485 | `		pAux = &aAux[n];` |
|     ! 0 | 1486 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1487 | `			/* Chunk found */` |
|     ! 0 | 1488 | `			return pAux;` |
|       - | 1489 | `		}` |
|     ! 0 | 1490 | `	}` |
|       - | 1491 | `	/* No such allocated chunk */` |
|     ! 0 | 1492 | `	return 0;` |
|    2136 | 1493 |  |
|       - | 1494 | `/*` |
|       - | 1495 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1496 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1497 | ` */` |
|     ! 0 | 1498 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1499 |  |
|       - | 1500 | `	ph7_aux_data *pAux;` |
|       - | 1501 | `	void *pNew;` |
|     ! 0 | 1502 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1503 | `	if( pNew ){` |
|     ! 0 | 1504 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1505 | `		if( pAux ){` |
|     ! 0 | 1506 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1507 | `		}` |
|     ! 0 | 1508 | `	}` |
|     ! 0 | 1509 | `	return pNew;` |
|     ! 0 | 1510 |  |
|       - | 1511 | `/*` |
|       - | 1512 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1513 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1514 | ` */` |
|    4262 | 1515 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1516 |  |
|       - | 1517 | `	ph7_aux_data *pAux;` |
|    4267 | 1518 | `	if( pChunk == 0 ){` |
|       - | 1519 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1520 | `		return;` |
|       - | 1521 | `	}` |
|    4267 | 1522 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    4267 | 1523 | `	if( pAux ){` |
|       - | 1524 | `		/* Mark as destroyed */` |
|     ! 0 | 1525 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1526 | `	}` |
|    4267 | 1527 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    2136 | 1528 |  |
|       - | 1529 | `/*` |
|       - | 1530 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1531 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1532 | ` */` |
|     ! 0 | 1533 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|     ! 0 | 1534 |  |
|       - | 1535 | `	ph7_hashmap_node *pNode;` |
|       - | 1536 | `	ph7_value *pValue;` |
|       - | 1537 | `	ph7_value skey;` |
|       - | 1538 | `	int rc;` |
|       - | 1539 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 1540 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1541 | `		return 0;` |
|       - | 1542 | `	}` |
|     ! 0 | 1543 | `	if( nByte < 0 ){` |
|     ! 0 | 1544 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1545 | `	}` |
|       - | 1546 | `	/* Convert the key to a ph7_value  */` |
|     ! 0 | 1547 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     ! 0 | 1548 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1549 | `	/* Perform the lookup */` |
|     ! 0 | 1550 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     ! 0 | 1551 | `	PH7_MemObjRelease(&skey);` |
|     ! 0 | 1552 | `	if( rc != PH7_OK ){` |
|       - | 1553 | `		/* No such entry */` |
|     ! 0 | 1554 | `		return 0;` |
|       - | 1555 | `	}` |
|       - | 1556 | `	/* Extract the target value */` |
|     ! 0 | 1557 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     ! 0 | 1558 | `	return pValue;` |
|     ! 0 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1562 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1563 | ` */` |
|   30046 | 1564 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       5 | 1565 |  |
|       - | 1566 | `	int rc;` |
|   30051 | 1567 | `	if( xWalk == 0 ){` |
|     ! 0 | 1568 | `		return PH7_CORRUPT;` |
|       - | 1569 | `	}` |
|       - | 1570 | `	/* Make sure we are dealing with a valid hashmap */` |
|   30051 | 1571 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1572 | `		return PH7_CORRUPT;` |
|       - | 1573 | `	}` |
|       - | 1574 | `	/* Start the walk process */` |
|   30051 | 1575 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   30051 | 1576 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   15028 | 1577 |  |
|       - | 1578 | `/*` |
|       - | 1579 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1580 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1581 | ` */` |
| 2278416 | 1582 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       5 | 1583 |  |
|       - | 1584 | `	int rc;` |
|       - | 1585 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2278421 | 1586 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1587 | `		return PH7_CORRUPT;` |
|       - | 1588 | `	}` |
|       - | 1589 | `	/* Perform the insertion */` |
| 2278421 | 1590 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
| 2278421 | 1591 | `	return rc;` |
| 1139213 | 1592 |  |
|       - | 1593 | `/*` |
|       - | 1594 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1595 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1596 | ` */` |
|    5162 | 1597 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       5 | 1598 |  |
|       - | 1599 | `	int rc;` |
|       - | 1600 | `	/* Make sure we are dealing with a valid hashmap */` |
|    5167 | 1601 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1602 | `		return PH7_CORRUPT;` |
|       - | 1603 | `	}` |
|       - | 1604 | `	/* Perform the insertion */` |
|    5167 | 1605 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1606 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1607 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1608 | `	}else{` |
|       - | 1609 | `		ph7_value sKey;` |
|    5167 | 1610 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|    5167 | 1611 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|    5167 | 1612 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|    5167 | 1613 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1614 | `	}` |
|    5167 | 1615 | `	return rc;` |
|    2586 | 1616 |  |
|       - | 1617 | `/*` |
|       - | 1618 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1619 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1620 | ` */` |
|     314 | 1621 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       5 | 1622 |  |
|       - | 1623 | `	ph7_value sKey;` |
|       - | 1624 | `	int rc;` |
|       - | 1625 | `	/* Make sure we are dealing with a valid hashmap */` |
|     319 | 1626 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1627 | `		return PH7_CORRUPT;` |
|       - | 1628 | `	}` |
|     319 | 1629 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1630 | `	/* Perform the insertion */` |
|     319 | 1631 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|     319 | 1632 | `	PH7_MemObjRelease(&sKey);` |
|     319 | 1633 | `	return rc;` |
|     162 | 1634 |  |
|       - | 1635 | `/*` |
|       - | 1636 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1637 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1638 | ` */` |
|  123300 | 1639 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       5 | 1640 |  |
|       - | 1641 | `	ph7_hashmap *pMap;` |
|       - | 1642 | `	/* Make sure we are dealing with a valid hashmap */` |
|  123305 | 1643 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1644 | `		return 0;` |
|       - | 1645 | `	}` |
|       - | 1646 | `	/* Point to the internal representation of the hashmap */` |
|  123305 | 1647 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  123305 | 1648 | `	return pMap->nEntry;` |
|   61655 | 1649 |  |
|       - | 1650 | `/*` |
|       - | 1651 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1652 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1653 | ` */` |
|       2 | 1654 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|       1 | 1655 |  |
|       - | 1656 | `	int rc;` |
|       3 | 1657 | `	if( xWalk == 0 ){` |
|     ! 0 | 1658 | `		return PH7_CORRUPT;` |
|       - | 1659 | `	}` |
|       - | 1660 | `	/* Make sure we are dealing with a valid class instance */` |
|       3 | 1661 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1662 | `		return PH7_CORRUPT;` |
|       - | 1663 | `	}` |
|       - | 1664 | `	/* Start the walk process */` |
|       3 | 1665 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|       3 | 1666 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|       2 | 1667 |  |
|       - | 1668 | `/*` |
|       - | 1669 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1670 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1671 | ` */` |
|       8 | 1672 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1673 |  |
|       - | 1674 | `	ph7_value *pValue;` |
|       - | 1675 | `	SyString sAttr;` |
|       - | 1676 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1677 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1678 | `		return 0;` |
|       - | 1679 | `	}` |
|       9 | 1680 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1681 | `	/* Extract the attribute value if available.` |
|       - | 1682 | `	 */` |
|       9 | 1683 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1684 | `	return pValue;` |
|       5 | 1685 |  |
|       - | 1686 | `/*` |
|       - | 1687 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1688 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1689 | ` */` |
|     ! 0 | 1690 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1691 |  |
|       - | 1692 | `	ph7_class *pClass;` |
|     ! 0 | 1693 | `	if( pLength ){` |
|     ! 0 | 1694 | `		*pLength = 0;` |
|     ! 0 | 1695 | `	}` |
|       - | 1696 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1697 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1698 | `		return 0;` |
|       - | 1699 | `	}` |
|       - | 1700 | `	/* Point to the class */` |
|     ! 0 | 1701 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1702 | `	/* Return the class name */` |
|     ! 0 | 1703 | `	if( pLength ){` |
|     ! 0 | 1704 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1705 | `	}` |
|     ! 0 | 1706 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1707 |  |
|       - | 1708 | `/*` |
|       - | 1709 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1710 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1711 | ` */` |
|     370 | 1712 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       3 | 1713 |  |
|       - | 1714 | `	SyString sData;` |
|       - | 1715 | `	int rc;` |
|     373 | 1716 | `	if( nLen < 0 ){` |
|     ! 0 | 1717 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1718 | `	}` |
|     373 | 1719 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|     373 | 1720 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|     373 | 1721 | `	return rc;` |
|       3 | 1722 |  |
|       - | 1723 | `/*` |
|       - | 1724 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1725 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1726 | ` */` |
|       2 | 1727 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1728 |  |
|       - | 1729 | `	va_list ap;` |
|       - | 1730 | `	int rc;` |
|       3 | 1731 | `	va_start(ap,zFormat);` |
|       3 | 1732 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1733 | `	va_end(ap);` |
|       3 | 1734 | `	return rc;` |
|       1 | 1735 |  |
|       - | 1736 | `/*` |
|       - | 1737 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1738 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1739 | ` */` |
|      24 | 1740 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       3 | 1741 |  |
|      27 | 1742 | `	int rc = PH7_OK;` |
|      27 | 1743 | `	if( zErr ){` |
|      27 | 1744 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      12 | 1745 | `	}` |
|      27 | 1746 | `	return rc;` |
|       3 | 1747 |  |
|       - | 1748 | `/*` |
|       - | 1749 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1750 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1751 | ` */` |
|      24 | 1752 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       4 | 1753 |  |
|       - | 1754 | `	va_list ap;` |
|       - | 1755 | `	int rc;` |
|      28 | 1756 | `	if( zFormat == 0){` |
|     ! 0 | 1757 | `		return PH7_OK;` |
|       - | 1758 | `	}` |
|      28 | 1759 | `	va_start(ap,zFormat);` |
|      28 | 1760 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      28 | 1761 | `	va_end(ap);` |
|      28 | 1762 | `	return rc;` |
|      16 | 1763 |  |
|       - | 1764 | `/*` |
|       - | 1765 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1766 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1767 | ` */` |
|      34 | 1768 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1769 |  |
|       - | 1770 | `	sxu32 n;` |
|      35 | 1771 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1772 | `	return n;` |
|       1 | 1773 |  |
|       - | 1774 | `/*` |
|       - | 1775 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1776 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1777 | ` */` |
|     ! 0 | 1778 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1779 |  |
|     ! 0 | 1780 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1781 | `		return PH7_CORRUPT;` |
|       - | 1782 | `	}` |
|     ! 0 | 1783 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1784 | `	return PH7_OK;` |
|     ! 0 | 1785 |  |
|       - | 1786 | `/*` |
|       - | 1787 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1788 | ` *` |
|       - | 1789 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1790 | ` * {` |
|       - | 1791 | ` *	return pCtx->pVm;` |
|       - | 1792 | ` * }` |
|       - | 1793 | ` */` |
|       - | 1794 | `/*` |
|       - | 1795 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1796 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1797 | ` */` |
|   54558 | 1798 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       5 | 1799 |  |
|   54563 | 1800 | `	return pCtx->pFunc->pUserData;` |
|       5 | 1801 |  |
|       - | 1802 | `/*` |
|       - | 1803 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1804 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1805 | ` */` |
|       2 | 1806 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1807 |  |
|       - | 1808 | `	ph7_aux_data sAux;` |
|       - | 1809 | `	int rc;` |
|       3 | 1810 | `	sAux.pAuxData = pUserData;` |
|       3 | 1811 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1812 | `	return rc;` |
|       1 | 1813 |  |
|       - | 1814 | `/*` |
|       - | 1815 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1816 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1817 | ` */` |
|       6 | 1818 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1819 |  |
|       - | 1820 | `	ph7_aux_data *pAux;` |
|       7 | 1821 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       7 | 1822 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1823 |  |
|       - | 1824 | `/*` |
|       - | 1825 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1826 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1827 | ` */` |
|       2 | 1828 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|       1 | 1829 |  |
|       - | 1830 | `	ph7_aux_data *pAux;` |
|       3 | 1831 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|       3 | 1832 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1833 |  |
|       - | 1834 | `/*` |
|       - | 1835 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1836 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1837 | ` */` |
|    5868 | 1838 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       5 | 1839 |  |
|    5873 | 1840 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       5 | 1841 |  |
|       - | 1842 | `/*` |
|       - | 1843 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1844 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1845 | ` */` |
|   22326 | 1846 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       5 | 1847 |  |
|       - | 1848 | `	SyString *pName;` |
|   22331 | 1849 | `	pName = &pCtx->pFunc->sName;` |
|   22331 | 1850 | `	return pName->zString;` |
|       5 | 1851 |  |
|       - | 1852 | `/*` |
|       - | 1853 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1854 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1855 | ` */` |
|   25592 | 1856 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       5 | 1857 |  |
|       - | 1858 | `	/* Invalidate any prior representation */` |
|   25597 | 1859 | `	PH7_MemObjRelease(pVal);` |
|   25597 | 1860 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|   25597 | 1861 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   25597 | 1862 | `	return PH7_OK;` |
|       5 | 1863 |  |
|       - | 1864 | `/*` |
|       - | 1865 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1866 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1867 | ` */` |
|   14568 | 1868 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       5 | 1869 |  |
|       - | 1870 | `	/* Invalidate any prior representation */` |
|   14573 | 1871 | `	PH7_MemObjRelease(pVal);` |
|   14573 | 1872 | `	pVal->x.iVal = iValue;` |
|   14573 | 1873 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   14573 | 1874 | `	return PH7_OK;` |
|       5 | 1875 |  |
|       - | 1876 | `/*` |
|       - | 1877 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1878 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1879 | ` */` |
|  316888 | 1880 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       5 | 1881 |  |
|       - | 1882 | `	/* Invalidate any prior representation */` |
|  316893 | 1883 | `	PH7_MemObjRelease(pVal);` |
|  316893 | 1884 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  316893 | 1885 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  316893 | 1886 | `	return PH7_OK;` |
|       5 | 1887 |  |
|       - | 1888 | `/*` |
|       - | 1889 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1890 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1891 | ` */` |
|       4 | 1892 | `int ph7_value_null(ph7_value *pVal)` |
|       1 | 1893 |  |
|       - | 1894 | `	/* Invalidate any prior representation and set the NULL flag */` |
|       5 | 1895 | `	PH7_MemObjRelease(pVal);` |
|       5 | 1896 | `	return PH7_OK;` |
|       1 | 1897 |  |
|       - | 1898 | `/*` |
|       - | 1899 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1900 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1901 | ` */` |
|     532 | 1902 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       1 | 1903 |  |
|       - | 1904 | `	/* Invalidate any prior representation */` |
|     533 | 1905 | `	PH7_MemObjRelease(pVal);` |
|     533 | 1906 | `	pVal->rVal = (ph7_real)Value;` |
|     533 | 1907 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1908 | `	/* Try to get an integer representation also */` |
|     533 | 1909 | `	PH7_MemObjTryInteger(pVal);` |
|     533 | 1910 | `	return PH7_OK;` |
|       1 | 1911 |  |
|       - | 1912 | `/*` |
|       - | 1913 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1914 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1915 | ` */` |
|  999718 | 1916 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       5 | 1917 |  |
|  999723 | 1918 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1919 | `		/* Invalidate any prior representation */` |
|  341835 | 1920 | `		PH7_MemObjRelease(pVal);` |
|  341835 | 1921 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  170915 | 1922 | `	}` |
|  999723 | 1923 | `	if( zString ){` |
|  998491 | 1924 | `		if( nLen < 0 ){` |
|       - | 1925 | `			/* Compute length automatically */` |
|    3683 | 1926 | `			nLen = (int)SyStrlen(zString);` |
|    1839 | 1927 | `		}` |
|       - | 1928 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1929 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
|  998491 | 1930 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1931 | `	}` |
|    1233 | 1932 | `	return PH7_OK;` |
|  499864 | 1933 |  |
|       - | 1934 | `/*` |
|       - | 1935 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1936 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1937 | ` */` |
|      22 | 1938 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1939 |  |
|       - | 1940 | `	va_list ap;` |
|       - | 1941 | `	int rc;` |
|      23 | 1942 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1943 | `		/* Invalidate any prior representation */` |
|      19 | 1944 | `		PH7_MemObjRelease(pVal);` |
|      19 | 1945 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       9 | 1946 | `	}` |
|      23 | 1947 | `	va_start(ap,zFormat);` |
|      23 | 1948 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      23 | 1949 | `	va_end(ap);` |
|       - | 1950 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|      23 | 1951 | `	return rc;` |
|       1 | 1952 |  |
|       - | 1953 | `/*` |
|       - | 1954 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1955 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1956 | ` */` |
|  130666 | 1957 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       5 | 1958 |  |
|       - | 1959 | `	/* Reset the string cursor */` |
|  130671 | 1960 | `	SyBlobReset(&pVal->sBlob);` |
|  130671 | 1961 | `	return PH7_OK;` |
|       5 | 1962 |  |
|       - | 1963 | `/*` |
|       - | 1964 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1965 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1966 | ` */` |
|    4432 | 1967 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       5 | 1968 |  |
|       - | 1969 | `	/* Invalidate any prior representation */` |
|    4437 | 1970 | `	PH7_MemObjRelease(pVal);` |
|       - | 1971 | `	/* Reflect the new type */` |
|    4437 | 1972 | `	pVal->x.pOther = pUserData;` |
|    4437 | 1973 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    4437 | 1974 | `	return PH7_OK;` |
|       5 | 1975 |  |
|       - | 1976 | `/*` |
|       - | 1977 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 1978 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1979 | ` */` |
|    3276 | 1980 | `int ph7_value_release(ph7_value *pVal)` |
|       5 | 1981 |  |
|    3281 | 1982 | `	PH7_MemObjRelease(pVal);` |
|    3281 | 1983 | `	return PH7_OK;` |
|       5 | 1984 |  |
|       - | 1985 | `/*` |
|       - | 1986 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 1987 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1988 | ` */` |
|   12094 | 1989 | `int ph7_value_is_int(ph7_value *pVal)` |
|       5 | 1990 |  |
|       - | 1991 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 1992 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 1993 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 1994 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 1995 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|   12099 | 1996 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       5 | 1997 |  |
|       - | 1998 | `/*` |
|       - | 1999 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 2000 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2001 | ` */` |
|    1242 | 2002 | `int ph7_value_is_float(ph7_value *pVal)` |
|       5 | 2003 |  |
|    1247 | 2004 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       5 | 2005 |  |
|       - | 2006 | `/*` |
|       - | 2007 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 2008 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2009 | ` */` |
|     496 | 2010 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       5 | 2011 |  |
|     501 | 2012 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       5 | 2013 |  |
|       - | 2014 | `/*` |
|       - | 2015 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2016 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2017 | ` */` |
|   94036 | 2018 | `int ph7_value_is_string(ph7_value *pVal)` |
|       5 | 2019 |  |
|   94041 | 2020 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       5 | 2021 |  |
|       - | 2022 | `/*` |
|       - | 2023 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2024 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2025 | ` */` |
|    1718 | 2026 | `int ph7_value_is_null(ph7_value *pVal)` |
|       5 | 2027 |  |
|    1723 | 2028 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       5 | 2029 |  |
|       - | 2030 | `/*` |
|       - | 2031 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2032 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2033 | ` */` |
|     368 | 2034 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       5 | 2035 |  |
|       - | 2036 | `	int rc;` |
|     373 | 2037 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|     373 | 2038 | `	return rc;` |
|       5 | 2039 |  |
|       - | 2040 | `/*` |
|       - | 2041 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2042 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2043 | ` */` |
|   23856 | 2044 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       5 | 2045 |  |
|       - | 2046 | `	int rc;` |
|   23861 | 2047 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   23861 | 2048 | `	return rc;` |
|       5 | 2049 |  |
|       - | 2050 | `/*` |
|       - | 2051 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2052 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2053 | ` */` |
|      12 | 2054 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2055 |  |
|      13 | 2056 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2057 |  |
|       - | 2058 | `/*` |
|       - | 2059 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2060 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2061 | ` */` |
|  141574 | 2062 | `int ph7_value_is_array(ph7_value *pVal)` |
|       5 | 2063 |  |
|  141579 | 2064 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       5 | 2065 |  |
|       - | 2066 | `/*` |
|       - | 2067 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2068 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2069 | ` */` |
|    2230 | 2070 | `int ph7_value_is_object(ph7_value *pVal)` |
|       5 | 2071 |  |
|    2235 | 2072 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       5 | 2073 |  |
|       - | 2074 | `/*` |
|       - | 2075 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2076 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2077 | ` */` |
|   27370 | 2078 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       5 | 2079 |  |
|   27375 | 2080 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       5 | 2081 |  |
|       - | 2082 | `/*` |
|       - | 2083 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2084 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2085 | ` */` |
|   25608 | 2086 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       5 | 2087 |  |
|       - | 2088 | `	int rc;` |
|   25613 | 2089 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   25613 | 2090 | `	return rc;` |
|       5 | 2091 |  |
|       - | 2092 | `/*` |
|       - | 2093 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2094 | ` * Check if a value holds a Fiber instance.` |
|       - | 2095 | ` */` |
|     ! 0 | 2096 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2097 |  |
|     ! 0 | 2098 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2099 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2100 |  |
|       - | 2101 | `/*` |
|       - | 2102 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2103 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2104 | ` */` |
|     ! 0 | 2105 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2106 |  |
|     ! 0 | 2107 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2108 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2109 |  |
|       - | 2110 | `/*` |
|       - | 2111 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2112 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2113 | ` */` |
|     ! 0 | 2114 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2115 |  |
|     ! 0 | 2116 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2117 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2118 |  |
|       - | 2119 | `/*` |
|       - | 2120 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2121 | ` * Check if a Fiber is currently suspended.` |
|       - | 2122 | ` */` |
|     ! 0 | 2123 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2124 |  |
|     ! 0 | 2125 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2126 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2127 |  |
|       - | 2128 | `/*` |
|       - | 2129 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2130 | ` * Check if a Fiber has completed execution.` |
|       - | 2131 | ` */` |
|     ! 0 | 2132 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2133 |  |
|     ! 0 | 2134 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2135 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2136 |  |
|       - | 2137 | `/*` |
|       - | 2138 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2139 | ` * Get the return value of a terminated Fiber.` |
|       - | 2140 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2141 | ` */` |
|     ! 0 | 2142 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2143 |  |
|     ! 0 | 2144 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2145 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2146 |  |
|       - | 2147 |  |
