# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 809/1140 lines (70.96%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/* This file implement the public interfaces presented to host-applications.` |
|        - |    8 | ` * Routines in other files are for internal use by PH7 and should not be` |
|        - |    9 | ` * accessed by users of the library.` |
|        - |   10 | ` */` |
|        - |   11 | `#define PH7_ENGINE_MAGIC 0xF874BCD7` |
|        - |   12 | `#define PH7_ENGINE_MISUSE(ENGINE) (ENGINE == 0 \|\| ENGINE->nMagic != PH7_ENGINE_MAGIC)` |
|        - |   13 | `#define PH7_VM_MISUSE(VM) (VM == 0 \|\| VM->nMagic == PH7_VM_STALE)` |
|        - |   14 | `/* If another thread have released a working instance,the following macros` |
|        - |   15 | ` * evaluates to true. These macros are only used when the library` |
|        - |   16 | ` * is built with threading support enabled which is not the case in` |
|        - |   17 | ` * the default built.` |
|        - |   18 | ` */` |
|        - |   19 | `#define PH7_THRD_ENGINE_RELEASE(ENGINE) (ENGINE->nMagic != PH7_ENGINE_MAGIC)` |
|        - |   20 | `#define PH7_THRD_VM_RELEASE(VM) (VM->nMagic == PH7_VM_STALE)` |
|        - |   21 | `/* IMPLEMENTATION: ph7@embedded@symisc 311-12-32 */` |
|        - |   22 | `/*` |
|        - |   23 | ` * All global variables are collected in the structure named "sMPGlobal".` |
|        - |   24 | ` * That way it is clear in the code when we are using static variable because` |
|        - |   25 | ` * its name start with sMPGlobal.` |
|        - |   26 | ` */` |
|        - |   27 | `static struct Global_Data` |
|        - |   28 | `{` |
|        - |   29 | `	SyMemBackend sAllocator;                /* Global low level memory allocator */` |
|        - |   30 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |   31 | `	const SyMutexMethods *pMutexMethods;   /* Mutex methods */` |
|        - |   32 | `	SyMutex *pMutex;                       /* Global mutex */` |
|        - |   33 | `	sxu32 nThreadingLevel;                 /* Threading level: 0 == Single threaded/1 == Multi-Threaded` |
|        - |   34 | `										    * The threading level can be set using the [ph7_lib_config()]` |
|        - |   35 | `											* interface with a configuration verb set to` |
|        - |   36 | `											* PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE or` |
|        - |   37 | `											* PH7_LIB_CONFIG_THREAD_LEVEL_MULTI` |
|        - |   38 | `											*/` |
|        - |   39 | `#endif` |
|        - |   40 | `	const ph7_vfs *pVfs;                    /* Underlying virtual file system */` |
|        - |   41 | `	sxi32 nEngine;                          /* Total number of active engines */` |
|        - |   42 | `	ph7 *pEngines;                          /* List of active engine */` |
|        - |   43 | `	sxu32 nMagic;                           /* Sanity check against library misuse */` |
|        - |   44 | `}sMPGlobal = {` |
|        - |   45 | `	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,{0}}, /* SyMemBackend: +nMemLimit/nMemLimitHit/nMemTried */` |
|        - |   46 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |   47 | `	0,` |
|        - |   48 | `	0,` |
|        - |   49 | `	0,` |
|        - |   50 | `#endif` |
|        - |   51 | `	0,` |
|        - |   52 | `	0,` |
|        - |   53 | `	0,` |
|        - |   54 | `	0` |
|        - |   55 | `};` |
|        - |   56 | `#define PH7_LIB_MAGIC  0xEA1495BA` |
|        - |   57 | `#define PH7_LIB_MISUSE (sMPGlobal.nMagic != PH7_LIB_MAGIC)` |
|        - |   58 | `/*` |
|        - |   59 | ` * Supported threading level.` |
|        - |   60 | ` * These options have meaning only when the library is compiled with multi-threading` |
|        - |   61 | ` * support.That is,the PH7_ENABLE_THREADS compile time directive must be defined` |
|        - |   62 | ` * when PH7 is built.` |
|        - |   63 | ` * PH7_THREAD_LEVEL_SINGLE:` |
|        - |   64 | ` * In this mode,mutexing is disabled and the library can only be used by a single thread.` |
|        - |   65 | ` * PH7_THREAD_LEVEL_MULTI` |
|        - |   66 | ` * In this mode, all mutexes including the recursive mutexes on [ph7] objects` |
|        - |   67 | ` * are enabled so that the application is free to share the same engine` |
|        - |   68 | ` * between different threads at the same time.` |
|        - |   69 | ` */` |
|        - |   70 | `#define PH7_THREAD_LEVEL_SINGLE 1` |
|        - |   71 | `#define PH7_THREAD_LEVEL_MULTI  2` |
|        - |   72 | `/*` |
|        - |   73 | ` * Configure a running PH7 engine instance.` |
|        - |   74 | ` * return PH7_OK on success.Any other return` |
|        - |   75 | ` * value indicates failure.` |
|        - |   76 | ` * Refer to [ph7_config()].` |
|        - |   77 | ` */` |
|    13430 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|        5 |   79 | `{` |
|    13435 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    13435 |   81 | `	int rc = PH7_OK;` |
|        - |   82 | `	/* Perform the requested operation */` |
|    13435 |   83 | `	switch(nOp){` |
|     6724 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    13435 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    13435 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|        - |   87 | `		/* Compile time error consumer routine */` |
|    13435 |   88 | `		if( xConsumer == 0 ){` |
|      ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|      ! 0 |   90 | `			break;` |
|        - |   91 | `		}` |
|        - |   92 | `		/* Install the error consumer */` |
|    13435 |   93 | `		pConf->xErr     = xConsumer;` |
|    13435 |   94 | `		pConf->pErrData = pUserData;` |
|    13435 |   95 | `		break;` |
|        - |   96 | `									 }` |
|      ! 0 |   97 | `	case PH7_CONFIG_ERR_LOG:{` |
|        - |   98 | `		/* Extract compile-time error log if any */` |
|      ! 0 |   99 | `		const char **pzPtr = va_arg(ap,const char **);` |
|      ! 0 |  100 | `		int *pLen = va_arg(ap,int *);` |
|      ! 0 |  101 | `		if( pzPtr == 0 ){` |
|      ! 0 |  102 | `			rc = PH7_CORRUPT;` |
|      ! 0 |  103 | `			break;` |
|        - |  104 | `		}` |
|        - |  105 | `		/* NULL terminate the error-log buffer */` |
|      ! 0 |  106 | `		SyBlobNullAppend(&pConf->sErrConsumer);` |
|        - |  107 | `		/* Point to the error-log buffer */` |
|      ! 0 |  108 | `		*pzPtr = (const char *)SyBlobData(&pConf->sErrConsumer);` |
|      ! 0 |  109 | `		if( pLen ){` |
|      ! 0 |  110 | `			if( SyBlobLength(&pConf->sErrConsumer) > 1 /* NULL '\0' terminator */ ){` |
|      ! 0 |  111 | `				*pLen = (int)SyBlobLength(&pConf->sErrConsumer);` |
|      ! 0 |  112 | `			}else{` |
|      ! 0 |  113 | `				*pLen = 0;` |
|        - |  114 | `			}` |
|      ! 0 |  115 | `		}` |
|      ! 0 |  116 | `		break;` |
|        - |  117 | `							}` |
|      ! 0 |  118 | `	case PH7_CONFIG_ERR_ABORT:` |
|        - |  119 | `		/* Reserved for future use */` |
|      ! 0 |  120 | `		break;` |
|      ! 0 |  121 | `	case PH7_CONFIG_MAX_ALLOC: {` |
|        - |  122 | `		/* Per-allocation cap in bytes (0 = unlimited). VMs created afterwards` |
|        - |  123 | `		 * inherit it via SyMemBackendInitFromParent. Primarily a test/embedding` |
|        - |  124 | `		 * knob to exercise out-of-memory paths deterministically. */` |
|      ! 0 |  125 | `		unsigned int nMax = va_arg(ap,unsigned int);` |
|      ! 0 |  126 | `		pEngine->sAllocator.nMaxRequest = (sxu32)nMax;` |
|      ! 0 |  127 | `		break;` |
|        - |  128 | `								}` |
|      ! 0 |  129 | `	case PH7_CONFIG_CLOCK: {` |
|        - |  130 | `		/* Optional embedder clock used by microtime()/gettimeofday(). The` |
|        - |  131 | `		 * callback fills epoch seconds + microseconds; NULL restores the` |
|        - |  132 | `		 * platform default. Inherited by VMs created afterwards. */` |
|      ! 0 |  133 | `		ph7_clock xClock = va_arg(ap,ph7_clock);` |
|      ! 0 |  134 | `		void *pUserData  = va_arg(ap,void *);` |
|      ! 0 |  135 | `		pEngine->xConf.xClock     = xClock;` |
|      ! 0 |  136 | `		pEngine->xConf.pClockData = pUserData;` |
|      ! 0 |  137 | `		break;` |
|        - |  138 | `							}` |
|      ! 0 |  139 | `	case PH7_CONFIG_MAX_INPUT: {` |
|        - |  140 | `		/* Per-compile input byte cap (0 = use PH7_MAX_INPUT_SIZE default). */` |
|      ! 0 |  141 | `		unsigned int nMax = va_arg(ap,unsigned int);` |
|      ! 0 |  142 | `		pEngine->xConf.nMaxInput = (sxu32)nMax;` |
|      ! 0 |  143 | `		break;` |
|        - |  144 | `								}` |
|      ! 0 |  145 | `	default:` |
|        - |  146 | `		/* Unknown configuration verb */` |
|      ! 0 |  147 | `		rc = PH7_CORRUPT;` |
|      ! 0 |  148 | `		break;` |
|        - |  149 | `	} /* Switch() */` |
|    13435 |  150 | `	return rc;` |
|        5 |  151 | `}` |
|        - |  152 | `/*` |
|        - |  153 | ` * Configure the PH7 library.` |
|        - |  154 | ` * return PH7_OK on success.Any other return value` |
|        - |  155 | ` * indicates failure.` |
|        - |  156 | ` * Refer to [ph7_lib_config()].` |
|        - |  157 | ` */` |
|    20193 |  158 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|        5 |  159 | `{` |
|    20198 |  160 | `	int rc = PH7_OK;` |
|    20198 |  161 | `	switch(nOp){` |
|     3370 |  162 | `	    case PH7_LIB_CONFIG_VFS:{` |
|        - |  163 | `			/* Install a virtual file system */` |
|     6736 |  164 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|     6736 |  165 | `			sMPGlobal.pVfs = pVfs;` |
|     6736 |  166 | `			break;` |
|        - |  167 | `								}` |
|     3370 |  168 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|        - |  169 | `			/* Use an alternative low-level memory allocation routines */` |
|     6736 |  170 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|        - |  171 | `			/* Save the memory failure callback (if available) */` |
|     6736 |  172 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|     6736 |  173 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|     6736 |  174 | `			if( pMethods == 0 ){` |
|        - |  175 | `				/* Use the built-in memory allocation subsystem */` |
|     6736 |  176 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|     3366 |  177 | `			}else{` |
|      ! 0 |  178 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|        - |  179 | `			}` |
|     6736 |  180 | `			break;` |
|        - |  181 | `										  }` |
|      ! 0 |  182 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|        - |  183 | `			/* Memory failure callback */` |
|      ! 0 |  184 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|      ! 0 |  185 | `			void *pUserData = va_arg(ap,void *);` |
|      ! 0 |  186 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|      ! 0 |  187 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|      ! 0 |  188 | `			break;` |
|        - |  189 | `												 }` |
|     3370 |  190 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|        - |  191 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  192 | `			/* Use an alternative low-level mutex subsystem */` |
|     6736 |  193 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|        - |  194 | `#if defined (UNTRUST)` |
|        - |  195 | `			if( pMethods == 0 ){` |
|        - |  196 | `				rc = PH7_CORRUPT;` |
|        - |  197 | `			}` |
|        - |  198 | `#endif` |
|        - |  199 | `			/* Sanity check */` |
|     6736 |  200 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|        - |  201 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|      ! 0 |  202 | `				rc = PH7_CORRUPT;` |
|      ! 0 |  203 | `				break;` |
|        - |  204 | `			}` |
|     6736 |  205 | `			if( sMPGlobal.pMutexMethods ){` |
|        - |  206 | `				/* Overwrite the previous mutex subsystem */` |
|      ! 0 |  207 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|      ! 0 |  208 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|      ! 0 |  209 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|      ! 0 |  210 | `				}` |
|      ! 0 |  211 | `				sMPGlobal.pMutex = 0;` |
|      ! 0 |  212 | `			}` |
|        - |  213 | `			/* Initialize and install the new mutex subsystem */` |
|     6736 |  214 | `			if( pMethods->xGlobalInit ){` |
|        5 |  215 | `				rc = pMethods->xGlobalInit();` |
|        5 |  216 | `				if ( rc != PH7_OK ){` |
|      ! 0 |  217 | `					break;` |
|        - |  218 | `				}` |
|      ! 0 |  219 | `			}` |
|        - |  220 | `			/* Create the global mutex */` |
|     6736 |  221 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|     6736 |  222 | `			if( sMPGlobal.pMutex == 0 ){` |
|        - |  223 | `				/*` |
|        - |  224 | `				 * If the supplied mutex subsystem is so sick that we are unable to` |
|        - |  225 | `				 * create a single mutex,there is no much we can do here.` |
|        - |  226 | `				 */` |
|      ! 0 |  227 | `				if( pMethods->xGlobalRelease ){` |
|      ! 0 |  228 | `					pMethods->xGlobalRelease();` |
|      ! 0 |  229 | `				}` |
|      ! 0 |  230 | `				rc = PH7_CORRUPT;` |
|      ! 0 |  231 | `				break;` |
|        - |  232 | `			}` |
|     6736 |  233 | `			sMPGlobal.pMutexMethods = pMethods;` |
|     6736 |  234 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|        - |  235 | `				/* Set a default threading level */` |
|     6736 |  236 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|     3361 |  237 | `			}` |
|        - |  238 | `#endif` |
|     6736 |  239 | `			break;` |
|        - |  240 | `										   }` |
|      ! 0 |  241 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:` |
|        - |  242 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  243 | `			/* Single thread mode(Only one thread is allowed to play with the library) */` |
|      ! 0 |  244 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|        - |  245 | `#endif` |
|      ! 0 |  246 | `			break;` |
|      ! 0 |  247 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:` |
|        - |  248 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  249 | `			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines` |
|        - |  250 | `			 * may be shared between multiple threads).` |
|        - |  251 | `			 */` |
|      ! 0 |  252 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|        - |  253 | `#endif` |
|      ! 0 |  254 | `			break;` |
|      ! 0 |  255 | `		default:` |
|        - |  256 | `			/* Unknown configuration option */` |
|      ! 0 |  257 | `			rc = PH7_CORRUPT;` |
|      ! 0 |  258 | `			break;` |
|        - |  259 | `	}` |
|    20198 |  260 | `	return rc;` |
|        5 |  261 | `}` |
|        - |  262 | `/*` |
|        - |  263 | ` * [CAPIREF: ph7_lib_config()]` |
|        - |  264 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  265 | ` */` |
|    20193 |  266 | `int ph7_lib_config(int nConfigOp,...)` |
|        5 |  267 | `{` |
|        - |  268 | `	va_list ap;` |
|        - |  269 | `	int rc;` |
|        - |  270 |  |
|    20198 |  271 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|        - |  272 | `		/* Library is already initialized,this operation is forbidden */` |
|      ! 0 |  273 | `		return PH7_LOOKED;` |
|        - |  274 | `	}` |
|    20198 |  275 | `	va_start(ap,nConfigOp);` |
|    20198 |  276 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    20198 |  277 | `	va_end(ap);` |
|    20198 |  278 | `	return rc;` |
|    10088 |  279 | `}` |
|        - |  280 | `/*` |
|        - |  281 | ` * Global library initialization` |
|        - |  282 | ` * Refer to [ph7_lib_init()]` |
|        - |  283 | ` * This routine must be called to initialize the memory allocation subsystem,the mutex` |
|        - |  284 | ` * subsystem prior to doing any serious work with the library.The first thread to call` |
|        - |  285 | ` * this routine does the initialization process and set the magic number so no body later` |
|        - |  286 | ` * can re-initialize the library.If subsequent threads call this  routine before the first` |
|        - |  287 | ` * thread have finished the initialization process, then the subsequent threads must block` |
|        - |  288 | ` * until the initialization process is done.` |
|        - |  289 | ` */` |
|     6731 |  290 | `static sxi32 PH7CoreInitialize(void)` |
|        5 |  291 | `{` |
|        - |  292 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|        - |  293 | `#if defined(PH7_ENABLE_THREADS)` |
|     6736 |  294 | `	const SyMutexMethods *pMutexMethods = 0;` |
|     6736 |  295 | `	SyMutex *pMaster = 0;` |
|        - |  296 | `#endif` |
|        - |  297 | `	int rc;` |
|        - |  298 | `	/*` |
|        - |  299 | `	 * If the library is already initialized,then a call to this routine` |
|        - |  300 | `	 * is a no-op.` |
|        - |  301 | `	 */` |
|     6736 |  302 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|      ! 0 |  303 | `		return PH7_OK; /* Already initialized */` |
|        - |  304 | `	}` |
|        - |  305 | `	/* Point to the built-in vfs */` |
|     6736 |  306 | `	pVfs = PH7_ExportBuiltinVfs();` |
|        - |  307 | `	/* Install it */` |
|     6736 |  308 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|        - |  309 | `#if defined(PH7_ENABLE_THREADS)` |
|     6736 |  310 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|     6736 |  311 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|     6736 |  312 | `		if( pMutexMethods == 0 ){` |
|        - |  313 | `			/* Use the built-in mutex subsystem */` |
|     6736 |  314 | `			pMutexMethods = SyMutexExportMethods();` |
|     6736 |  315 | `			if( pMutexMethods == 0 ){` |
|      ! 0 |  316 | `				return PH7_CORRUPT; /* Can't happen */` |
|        - |  317 | `			}` |
|        - |  318 | `			/* Install the mutex subsystem */` |
|     6736 |  319 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|     6736 |  320 | `			if( rc != PH7_OK ){` |
|      ! 0 |  321 | `				return rc;` |
|        - |  322 | `			}` |
|     3361 |  323 | `		}` |
|        - |  324 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|     6736 |  325 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|     6736 |  326 | `		if( pMaster == 0 ){` |
|      ! 0 |  327 | `			return PH7_CORRUPT; /* Can't happen */` |
|        - |  328 | `		}` |
|     3361 |  329 | `	}` |
|        - |  330 | `	/* Lock the master mutex */` |
|     6736 |  331 | `	rc = PH7_OK;` |
|     6736 |  332 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    10097 |  333 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|        - |  334 | `#endif` |
|     6736 |  335 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|        - |  336 | `			/* Install a memory subsystem */` |
|     6736 |  337 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|     6736 |  338 | `			if( rc != PH7_OK ){` |
|        - |  339 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|      ! 0 |  340 | `				goto End;` |
|        - |  341 | `			}` |
|     3361 |  342 | `		}` |
|        - |  343 | `#if defined(PH7_ENABLE_THREADS)` |
|     6736 |  344 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  345 | `			/* Protect the memory allocation subsystem */` |
|     6736 |  346 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|     6736 |  347 | `			if( rc != PH7_OK ){` |
|      ! 0 |  348 | `				goto End;` |
|        - |  349 | `			}` |
|     3361 |  350 | `		}` |
|        - |  351 | `#endif` |
|        - |  352 | `		/* Our library is initialized,set the magic number */` |
|     6736 |  353 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|     6736 |  354 | `		rc = PH7_OK;` |
|        - |  355 | `#if defined(PH7_ENABLE_THREADS)` |
|     3361 |  356 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|        - |  357 | `#endif` |
|      ! 0 |  358 | `End:` |
|        - |  359 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  360 | `	/* Unlock the master mutex */` |
|     6736 |  361 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  362 | `#endif` |
|     6736 |  363 | `	return rc;` |
|     3366 |  364 | `}` |
|        - |  365 | `/*` |
|        - |  366 | ` * [CAPIREF: ph7_lib_init()]` |
|        - |  367 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  368 | ` */` |
|      ! 0 |  369 | `int ph7_lib_init(void)` |
|      ! 0 |  370 | `{` |
|        - |  371 | `	int rc;` |
|      ! 0 |  372 | `	rc = PH7CoreInitialize();` |
|      ! 0 |  373 | `	return rc;` |
|      ! 0 |  374 | `}` |
|        - |  375 | `/*` |
|        - |  376 | ` * Release an active PH7 engine and it's associated active virtual machines.` |
|        - |  377 | ` */` |
|     6753 |  378 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|        5 |  379 | `{` |
|        - |  380 | `	ph7_vm *pVm,*pNext;` |
|        - |  381 | `	/* Release all active VM */` |
|     6758 |  382 | `	pVm = pEngine->pVms;` |
|     3372 |  383 | `	for(;;){` |
|     6758 |  384 | `		if( pEngine->iVm <= 0 ){` |
|     6758 |  385 | `			break;` |
|        - |  386 | `		}` |
|      ! 0 |  387 | `		pNext = pVm->pNext;` |
|      ! 0 |  388 | `		PH7_VmRelease(pVm);` |
|      ! 0 |  389 | `		pVm = pNext;` |
|      ! 0 |  390 | `		pEngine->iVm--;` |
|      ! 0 |  391 | `	}` |
|        - |  392 | `	/* Set a dummy magic number */` |
|     6758 |  393 | `	pEngine->nMagic = 0x7635;` |
|        - |  394 | `	/* Release the private memory subsystem */` |
|     6758 |  395 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     6758 |  396 | `	return PH7_OK;` |
|        5 |  397 | `}` |
|        - |  398 | `/*` |
|        - |  399 | ` * Release all resources consumed by the library.` |
|        - |  400 | ` * If PH7 is already shut down when this routine` |
|        - |  401 | ` * is invoked then this routine is a harmless no-op.` |
|        - |  402 | ` * Note: This call is not thread safe.` |
|        - |  403 | ` * Refer to [ph7_lib_shutdown()].` |
|        - |  404 | ` */` |
|      946 |  405 | `static void PH7CoreShutdown(void)` |
|        4 |  406 | `{` |
|        - |  407 | `	ph7 *pEngine,*pNext;` |
|        - |  408 | `	/* Release all active engines first */` |
|      950 |  409 | `	pEngine = sMPGlobal.pEngines;` |
|      473 |  410 | `	for(;;){` |
|      950 |  411 | `		if( sMPGlobal.nEngine < 1 ){` |
|      950 |  412 | `			break;` |
|        - |  413 | `		}` |
|      ! 0 |  414 | `		pNext = pEngine->pNext;` |
|      ! 0 |  415 | `		EngineRelease(pEngine);` |
|      ! 0 |  416 | `		pEngine = pNext;` |
|      ! 0 |  417 | `		sMPGlobal.nEngine--;` |
|      ! 0 |  418 | `	}` |
|        - |  419 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  420 | `	/* Release the mutex subsystem */` |
|      950 |  421 | `	if( sMPGlobal.pMutexMethods ){` |
|      950 |  422 | `		if( sMPGlobal.pMutex ){` |
|      950 |  423 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|      950 |  424 | `			sMPGlobal.pMutex = 0;` |
|      473 |  425 | `		}` |
|      950 |  426 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|        4 |  427 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|      ! 0 |  428 | `		}` |
|      950 |  429 | `		sMPGlobal.pMutexMethods = 0;` |
|      473 |  430 | `	}` |
|      950 |  431 | `	sMPGlobal.nThreadingLevel = 0;` |
|        - |  432 | `#endif` |
|      950 |  433 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|        - |  434 | `		/* Release the memory backend */` |
|      950 |  435 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|      473 |  436 | `	}` |
|      950 |  437 | `	sMPGlobal.nMagic = 0x1928;` |
|      950 |  438 | `}` |
|        - |  439 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  440 | `/*` |
|        - |  441 | ` * Drop THIS process to single-threaded mode, called in the CHILD of a fork().` |
|        - |  442 | ` *` |
|        - |  443 | ` * Only one thread survives fork(), and every mutex the library holds is a` |
|        - |  444 | ` * pthread RECURSIVE one whose owner field still names the PARENT's thread id.` |
|        - |  445 | ` * The child's single thread is therefore not the owner of anything -- so the` |
|        - |  446 | ` * first SyMutexEnter it reaches (the one ph7_vm_config takes, which every exit` |
|        - |  447 | ` * path goes through) blocks on a futex nobody will ever post. A forked child` |
|        - |  448 | ` * that merely called exit() hung forever, which is what made this necessary.` |
|        - |  449 | ` *` |
|        - |  450 | ` * The fix is to RE-INITIALIZE every one of them, which is exactly what` |
|        - |  451 | ` * pthread_atfork's child handler exists for and the only defined way out:` |
|        - |  452 | ` * destroying a locked mutex is undefined, and unlocking one this thread does` |
|        - |  453 | ` * not own does nothing. After this nobody holds anything and the child's single` |
|        - |  454 | ` * thread can lock and unlock normally; a balancing SyMutexLeave the interrupted` |
|        - |  455 | ` * call still owes answers EPERM and changes nothing, which is the right answer.` |
|        - |  456 | ` *` |
|        - |  457 | ` * An embedder that installed its OWN mutex subsystem is a different matter --` |
|        - |  458 | ` * this cannot know what its locks are made of. There the mutex POINTERS are` |
|        - |  459 | ` * dropped instead (SyMutexEnter/Leave are no-ops on a null one) and the level` |
|        - |  460 | ` * goes to SINGLE, which is safe for the same reason: a child has one thread` |
|        - |  461 | ` * until it makes another, so the locks protect nothing.` |
|        - |  462 | ` */` |
|       16 |  463 | `PH7_PRIVATE void PH7_LibForkChild(void)` |
|      ! 0 |  464 | `{` |
|        - |  465 | `	ph7 *pEngine;` |
|        - |  466 | `	sxi32 i;` |
|        - |  467 | `	int bReset;` |
|       16 |  468 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC` |
|       16 |  469 | `	 \|\| sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE ){` |
|      ! 0 |  470 | `		return;` |
|        - |  471 | `	}` |
|        - |  472 | `	/* One probe decides which of the two answers this build gets. */` |
|       16 |  473 | `	bReset = SyMutexResetAfterFork(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|       16 |  474 | `	pEngine = sMPGlobal.pEngines;` |
|       32 |  475 | `	for( i = 0 ; i < sMPGlobal.nEngine && pEngine ; ++i ){` |
|       16 |  476 | `		ph7_vm *pVm = pEngine->pVms;` |
|        - |  477 | `		sxi32 j;` |
|       32 |  478 | `		for( j = 0 ; j < pEngine->iVm && pVm ; ++j ){` |
|       16 |  479 | `			if( bReset ){` |
|       16 |  480 | `				SyMutexResetAfterFork(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|        8 |  481 | `			}else{` |
|      ! 0 |  482 | `				pVm->pMutex = 0;` |
|        - |  483 | `			}` |
|       16 |  484 | `			pVm = pVm->pNext;` |
|        8 |  485 | `		}` |
|       16 |  486 | `		if( bReset ){` |
|       16 |  487 | `			SyMutexResetAfterFork(sMPGlobal.pMutexMethods,pEngine->pMutex);` |
|       24 |  488 | `			SyMutexResetAfterFork(pEngine->sAllocator.pMutexMethods,` |
|        8 |  489 | `				pEngine->sAllocator.pMutex);` |
|        8 |  490 | `		}else{` |
|      ! 0 |  491 | `			pEngine->pMutex = 0;` |
|      ! 0 |  492 | `			pEngine->sAllocator.pMutex = 0;` |
|      ! 0 |  493 | `			pEngine->sAllocator.pMutexMethods = 0;` |
|        - |  494 | `		}` |
|       16 |  495 | `		pEngine = pEngine->pNext;` |
|        8 |  496 | `	}` |
|       16 |  497 | `	if( bReset ){` |
|       24 |  498 | `		SyMutexResetAfterFork(sMPGlobal.sAllocator.pMutexMethods,` |
|        8 |  499 | `			sMPGlobal.sAllocator.pMutex);` |
|       16 |  500 | `		return;` |
|        - |  501 | `	}` |
|      ! 0 |  502 | `	sMPGlobal.pMutex = 0;` |
|      ! 0 |  503 | `	sMPGlobal.sAllocator.pMutex = 0;` |
|      ! 0 |  504 | `	sMPGlobal.sAllocator.pMutexMethods = 0;` |
|      ! 0 |  505 | `	sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|        8 |  506 | `}` |
|        - |  507 | `#endif /* PH7_ENABLE_THREADS */` |
|        - |  508 | `/*` |
|        - |  509 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|        - |  510 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  511 | ` */` |
|      946 |  512 | `int ph7_lib_shutdown(void)` |
|        4 |  513 | `{` |
|      950 |  514 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|        - |  515 | `		/* Already shut */` |
|      ! 0 |  516 | `		return PH7_OK;` |
|        - |  517 | `	}` |
|      950 |  518 | `	PH7CoreShutdown();` |
|      950 |  519 | `	return PH7_OK;` |
|      477 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * [CAPIREF: ph7_lib_is_threadsafe()]` |
|        - |  523 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  524 | ` */` |
|      ! 0 |  525 | `int ph7_lib_is_threadsafe(void)` |
|      ! 0 |  526 | `{` |
|      ! 0 |  527 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|      ! 0 |  528 | `		return 0;` |
|        - |  529 | `	}` |
|        - |  530 | `#if defined(PH7_ENABLE_THREADS)` |
|      ! 0 |  531 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  532 | `			/* Muli-threading support is enabled */` |
|      ! 0 |  533 | `			return 1;` |
|      ! 0 |  534 | `		}else{` |
|        - |  535 | `			/* Single-threading */` |
|      ! 0 |  536 | `			return 0;` |
|        - |  537 | `		}` |
|        - |  538 | `#else` |
|        - |  539 | `	return 0;` |
|        - |  540 | `#endif` |
|      ! 0 |  541 | `}` |
|        - |  542 | `/*` |
|        - |  543 | ` * [CAPIREF: ph7_lib_version()]` |
|        - |  544 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  545 | ` */` |
|        2 |  546 | `const char * ph7_lib_version(void)` |
|        1 |  547 | `{` |
|        3 |  548 | `	return PH7_VERSION;` |
|        1 |  549 | `}` |
|        - |  550 | `/*` |
|        - |  551 | ` * [CAPIREF: ph7_lib_signature()]` |
|        - |  552 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  553 | ` */` |
|      220 |  554 | `const char * ph7_lib_signature(void)` |
|        3 |  555 | `{` |
|      223 |  556 | `	return PH7_SIG;` |
|        3 |  557 | `}` |
|        - |  558 | `/*` |
|        - |  559 | ` * [CAPIREF: ph7_lib_ident()]` |
|        - |  560 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  561 | ` */` |
|        2 |  562 | `const char * ph7_lib_ident(void)` |
|        1 |  563 | `{` |
|        3 |  564 | `	return PH7_IDENT;` |
|        1 |  565 | `}` |
|        - |  566 | `/*` |
|        - |  567 | ` * [CAPIREF: ph7_lib_copyright()]` |
|        - |  568 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  569 | ` */` |
|      ! 0 |  570 | `const char * ph7_lib_copyright(void)` |
|      ! 0 |  571 | `{` |
|      ! 0 |  572 | `	return PH7_COPYRIGHT;` |
|      ! 0 |  573 | `}` |
|        - |  574 | `/*` |
|        - |  575 | ` * [CAPIREF: ph7_config()]` |
|        - |  576 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  577 | ` */` |
|    13430 |  578 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|        5 |  579 | `{` |
|        - |  580 | `	va_list ap;` |
|        - |  581 | `	int rc;` |
|    13435 |  582 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|      ! 0 |  583 | `		return PH7_CORRUPT;` |
|        - |  584 | `	}` |
|        - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  586 | `	 /* Acquire engine mutex */` |
|    13435 |  587 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    13435 |  588 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    13430 |  589 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  590 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  591 | `	 }` |
|        - |  592 | `#endif` |
|    13435 |  593 | `	 va_start(ap,nConfigOp);` |
|    13435 |  594 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    13435 |  595 | `	 va_end(ap);` |
|        - |  596 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  597 | `	 /* Leave engine mutex */` |
|    13435 |  598 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  599 | `#endif` |
|    13435 |  600 | `	return rc;` |
|     6711 |  601 | `}` |
|        - |  602 | `/*` |
|        - |  603 | ` * [CAPIREF: ph7_init()]` |
|        - |  604 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  605 | ` */` |
|     6731 |  606 | `int ph7_init(ph7 **ppEngine)` |
|        5 |  607 | `{` |
|        - |  608 | `	ph7 *pEngine;` |
|        - |  609 | `	int rc;` |
|        - |  610 | `#if defined(UNTRUST)` |
|        - |  611 | `	if( ppEngine == 0 ){` |
|        - |  612 | `		return PH7_CORRUPT;` |
|        - |  613 | `	}` |
|        - |  614 | `#endif` |
|     6736 |  615 | `	*ppEngine = 0;` |
|        - |  616 | `	/* One-time automatic library initialization */` |
|     6736 |  617 | `	rc = PH7CoreInitialize();` |
|     6736 |  618 | `	if( rc != PH7_OK ){` |
|      ! 0 |  619 | `		return rc;` |
|        - |  620 | `	}` |
|        - |  621 | `	/* Allocate a new engine */` |
|     6736 |  622 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|     6736 |  623 | `	if( pEngine == 0 ){` |
|      ! 0 |  624 | `		return PH7_NOMEM;` |
|        - |  625 | `	}` |
|        - |  626 | `	/* Zero the structure */` |
|     6736 |  627 | `	SyZero(pEngine,sizeof(ph7));` |
|        - |  628 | `	/* Initialize engine fields */` |
|     6736 |  629 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|     6736 |  630 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|     6736 |  631 | `	if( rc != PH7_OK ){` |
|      ! 0 |  632 | `		goto Release;` |
|        - |  633 | `	}` |
|        - |  634 | `#if defined(PH7_ENABLE_THREADS)` |
|     6736 |  635 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|        - |  636 | `#endif` |
|        - |  637 | `	/* Default configuration */` |
|     6736 |  638 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|        - |  639 | `	/* Install a default compile-time error consumer routine */` |
|     6736 |  640 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|        - |  641 | `	/* Built-in vfs */` |
|     6736 |  642 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|        - |  643 | `#if defined(PH7_ENABLE_THREADS)` |
|     6736 |  644 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  645 | `		 /* Associate a recursive mutex with this instance */` |
|     6736 |  646 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|     6736 |  647 | `		 if( pEngine->pMutex == 0 ){` |
|      ! 0 |  648 | `			 rc = PH7_NOMEM;` |
|      ! 0 |  649 | `			 goto Release;` |
|        - |  650 | `		 }` |
|     3361 |  651 | `	 }` |
|        - |  652 | `#endif` |
|        - |  653 | `	/* Link to the list of active engines */` |
|        - |  654 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  655 | `	/* Enter the global mutex */` |
|     6736 |  656 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  657 | `#endif` |
|     6736 |  658 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|     6736 |  659 | `	sMPGlobal.nEngine++;` |
|        - |  660 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  661 | `	/* Leave the global mutex */` |
|     6736 |  662 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  663 | `#endif` |
|        - |  664 | `	/* Write a pointer to the new instance */` |
|     6736 |  665 | `	*ppEngine = pEngine;` |
|     6736 |  666 | `	return PH7_OK;` |
|      ! 0 |  667 | `Release:` |
|      ! 0 |  668 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|      ! 0 |  669 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|      ! 0 |  670 | `	return rc;` |
|     3366 |  671 | `}` |
|        - |  672 | `/*` |
|        - |  673 | ` * [CAPIREF: ph7_release()]` |
|        - |  674 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  675 | ` */` |
|     6753 |  676 | `int ph7_release(ph7 *pEngine)` |
|        5 |  677 | `{` |
|        - |  678 | `	int rc;` |
|     6758 |  679 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|      ! 0 |  680 | `		return PH7_CORRUPT;` |
|        - |  681 | `	}` |
|        - |  682 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  683 | `	 /* Acquire engine mutex */` |
|     6758 |  684 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6758 |  685 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6753 |  686 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  687 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  688 | `	 }` |
|        - |  689 | `#endif` |
|        - |  690 | `	/* Release the engine */` |
|     6758 |  691 | `	rc = EngineRelease(&(*pEngine));` |
|        - |  692 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  693 | `	 /* Leave engine mutex */` |
|     6758 |  694 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  695 | `	 /* Release engine mutex */` |
|     6758 |  696 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  697 | `#endif` |
|        - |  698 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  699 | `	/* Enter the global mutex */` |
|     6758 |  700 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  701 | `#endif` |
|        - |  702 | `	/* Unlink from the list of active engines */` |
|     6758 |  703 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|     6758 |  704 | `	sMPGlobal.nEngine--;` |
|        - |  705 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  706 | `	/* Leave the global mutex */` |
|     6758 |  707 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  708 | `#endif` |
|        - |  709 | `	/* Release the memory chunk allocated to this engine */` |
|     6758 |  710 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     6758 |  711 | `	return rc;` |
|     3377 |  712 | `}` |
|        - |  713 | `/*` |
|        - |  714 | ` * Compile a raw PHP script.` |
|        - |  715 | ` * To execute a PHP code, it must first be compiled into a byte-code program using this routine.` |
|        - |  716 | ` * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]` |
|        - |  717 | ` * should  display the appropriate error message and this function set ppVm to null and return` |
|        - |  718 | ` * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled` |
|        - |  719 | ` * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].` |
|        - |  720 | ` * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script` |
|        - |  721 | ` * for evaluation.` |
|        - |  722 | ` */` |
|     6721 |  723 | `static sxi32 ProcessScript(` |
|        - |  724 | `	ph7 *pEngine,          /* Running PH7 engine */` |
|        - |  725 | `	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */` |
|        - |  726 | `	SyString *pScript,     /* Raw PHP script to compile */` |
|        - |  727 | `	sxi32 iFlags,          /* Compile-time flags */` |
|        - |  728 | `	const char *zFilePath  /* File path if script come from a file. NULL otherwise */` |
|        - |  729 | `	)` |
|        5 |  730 | `{` |
|        - |  731 | `	ph7_vm *pVm;` |
|        - |  732 | `	int rc;` |
|        - |  733 | `	/* Allocate a new virtual machine */` |
|     6726 |  734 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|     6726 |  735 | `	if( pVm == 0 ){` |
|        - |  736 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  737 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|      ! 0 |  738 | `		if( ppVm ){` |
|      ! 0 |  739 | `			*ppVm = 0;` |
|      ! 0 |  740 | `		}` |
|      ! 0 |  741 | `		return PH7_NOMEM;` |
|        - |  742 | `	}` |
|     6726 |  743 | `	if( iFlags < 0 ){` |
|        - |  744 | `		/* Default compile-time flags */` |
|      ! 0 |  745 | `		iFlags = 0;` |
|      ! 0 |  746 | `	}` |
|        - |  747 | `	/* Initialize the Virtual Machine */` |
|     6726 |  748 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|     6726 |  749 | `	if( rc != PH7_OK ){` |
|      ! 0 |  750 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|      ! 0 |  751 | `		if( ppVm ){` |
|      ! 0 |  752 | `			*ppVm = 0;` |
|      ! 0 |  753 | `		}` |
|      ! 0 |  754 | `		return PH7_VM_ERR;` |
|        - |  755 | `	}` |
|     6726 |  756 | `	if( zFilePath ){` |
|        - |  757 | `		/* Push processed file path */` |
|     6672 |  758 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|     3334 |  759 | `	}else{` |
|        - |  760 | `		/* Anonymous source (phl -r / an embedder snippet): php names it` |
|        - |  761 | `		 * "Command line code" in every diagnostic location suffix. */` |
|       57 |  762 | `		PH7_VmPushFilePath(pVm,"Command line code",-1,TRUE,0);` |
|        - |  763 | `	}` |
|        - |  764 | `	/* Reset the error message consumer */` |
|     6726 |  765 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|        - |  766 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|        - |  767 | `	{` |
|     6726 |  768 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|     6726 |  769 | `		if( SyStringLength(pScript) > nLimit ){` |
|      ! 0 |  770 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|        - |  771 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|      ! 0 |  772 | `				SyStringLength(pScript),nLimit);` |
|      ! 0 |  773 | `		}` |
|        - |  774 | `	}` |
|        - |  775 | `	/* A syntax-CHECK compile (phl -l) never runs what it compiles, which changes` |
|        - |  776 | `	 * what a class declaration whose base is missing has to do -- see the flag's` |
|        - |  777 | `	 * note in ph7int.h. */` |
|     6726 |  778 | `	pVm->bSyntaxCheck = (iFlags & PH7_SYNTAX_CHECK) ? 1 : 0;` |
|        - |  779 | `	/* Compile the script */` |
|     6726 |  780 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|     6726 |  781 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|     3356 |  782 | `	}` |
|     6726 |  783 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     1106 |  784 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|        - |  785 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     1106 |  786 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     1106 |  787 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     1106 |  788 | `		if( ppVm ){` |
|     1106 |  789 | `			*ppVm = 0;` |
|      551 |  790 | `		}` |
|     1106 |  791 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|        - |  792 | `	}` |
|        - |  793 | `	/* Prepare the virtual machine for bytecode execution */` |
|     5624 |  794 | `	rc = PH7_VmMakeReady(pVm);` |
|     5624 |  795 | `	if( rc != PH7_OK ){` |
|        9 |  796 | `		goto Release;` |
|        - |  797 | `	}` |
|        - |  798 | `	/* Install local import path which is the current directory */` |
|     5618 |  799 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|        - |  800 | `#if defined(PH7_ENABLE_THREADS)` |
|     5618 |  801 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  802 | `		 /* Associate a recursive mutex with this instance */` |
|     5618 |  803 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|     5618 |  804 | `		 if( pVm->pMutex == 0 ){` |
|      ! 0 |  805 | `			 goto Release;` |
|        - |  806 | `		 }` |
|     2802 |  807 | `	 }` |
|        - |  808 | `#endif` |
|        - |  809 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|     5618 |  810 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|     5618 |  811 | `	pEngine->iVm++;` |
|        - |  812 | `	/* Point to the freshly created VM */` |
|     5618 |  813 | `	*ppVm = pVm;` |
|        - |  814 | `	/* Ready to execute PH7 bytecode */` |
|     5618 |  815 | `	return PH7_OK;` |
|        3 |  816 | `Release:` |
|        - |  817 | `	{` |
|        - |  818 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|        - |  819 | `		 * typed class constant whose value violates its declared type) is a compile` |
|        - |  820 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|        - |  821 | `		 * Captured before the releases free the VM. */` |
|        9 |  822 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|        9 |  823 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|        9 |  824 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|        9 |  825 | `		*ppVm = 0;` |
|        9 |  826 | `		return rcRet;` |
|        - |  827 | `	}` |
|     3361 |  828 | `}` |
|        - |  829 | `/*` |
|        - |  830 | ` * [CAPIREF: ph7_compile()]` |
|        - |  831 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  832 | ` */` |
|      ! 0 |  833 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|      ! 0 |  834 | `{` |
|        - |  835 | `	SyString sScript;` |
|        - |  836 | `	int rc;` |
|      ! 0 |  837 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|      ! 0 |  838 | `		return PH7_CORRUPT;` |
|        - |  839 | `	}` |
|      ! 0 |  840 | `	if( nLen < 0 ){` |
|        - |  841 | `		/* Compute input length automatically */` |
|      ! 0 |  842 | `		nLen = (int)SyStrlen(zSource);` |
|      ! 0 |  843 | `	}` |
|      ! 0 |  844 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|        - |  845 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  846 | `	 /* Acquire engine mutex */` |
|      ! 0 |  847 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 |  848 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 |  849 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  850 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  851 | `	 }` |
|        - |  852 | `#endif` |
|        - |  853 | `	/* Compile the script */` |
|      ! 0 |  854 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|        - |  855 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  856 | `	 /* Leave engine mutex */` |
|      ! 0 |  857 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  858 | `#endif` |
|        - |  859 | `	/* Compilation result */` |
|      ! 0 |  860 | `	return rc;` |
|      ! 0 |  861 | `}` |
|        - |  862 | `/*` |
|        - |  863 | ` * [CAPIREF: ph7_compile_v2()]` |
|        - |  864 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  865 | ` */` |
|       54 |  866 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|        3 |  867 | `{` |
|        - |  868 | `	SyString sScript;` |
|        - |  869 | `	int rc;` |
|       57 |  870 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|      ! 0 |  871 | `		return PH7_CORRUPT;` |
|        - |  872 | `	}` |
|       57 |  873 | `	if( nLen < 0 ){` |
|        - |  874 | `		/* Compute input length automatically */` |
|       45 |  875 | `		nLen = (int)SyStrlen(zSource);` |
|       21 |  876 | `	}` |
|       57 |  877 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|        - |  878 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  879 | `	 /* Acquire engine mutex */` |
|       57 |  880 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       57 |  881 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       54 |  882 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  883 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  884 | `	 }` |
|        - |  885 | `#endif` |
|        - |  886 | `	/* Compile the script */` |
|       57 |  887 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|        - |  888 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  889 | `	 /* Leave engine mutex */` |
|       57 |  890 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  891 | `#endif` |
|        - |  892 | `	/* Compilation result */` |
|       57 |  893 | `	return rc;` |
|       30 |  894 | `}` |
|        - |  895 | `/*` |
|        - |  896 | ` * [CAPIREF: ph7_compile_file()]` |
|        - |  897 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  898 | ` */` |
|     6675 |  899 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|        5 |  900 | `{` |
|        - |  901 | `	const ph7_vfs *pVfs;` |
|        - |  902 | `	int rc;` |
|     6680 |  903 | `	if( ppOutVm ){` |
|     6680 |  904 | `		*ppOutVm = 0;` |
|     3333 |  905 | `	}` |
|     6680 |  906 | `	rc = PH7_OK; /* cc warning */` |
|     6680 |  907 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|      ! 0 |  908 | `		return PH7_CORRUPT;` |
|        - |  909 | `	}` |
|        - |  910 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  911 | `	 /* Acquire engine mutex */` |
|     6680 |  912 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6680 |  913 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6675 |  914 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  915 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  916 | `	 }` |
|        - |  917 | `#endif` |
|        - |  918 | `	 /*` |
|        - |  919 | `	  * Check if the underlying vfs implement the memory map` |
|        - |  920 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|        - |  921 | `	  */` |
|     6680 |  922 | `	 pVfs = pEngine->pVfs;` |
|     6680 |  923 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|        - |  924 | `		 /* Memory map routine not implemented */` |
|      ! 0 |  925 | `		 rc = PH7_IO_ERR;` |
|      ! 0 |  926 | `	 }else{` |
|     6680 |  927 | `		 void *pMapView = 0; /* cc warning */` |
|     6680 |  928 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|        - |  929 | `		 SyString sScript;` |
|        - |  930 | `		 /* Try to get a memory view of the whole file */` |
|     6680 |  931 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|     6680 |  932 | `		 if( rc != PH7_OK ){` |
|        - |  933 | `			 /* Assume an IO error */` |
|        8 |  934 | `			 rc = PH7_IO_ERR;` |
|        4 |  935 | `		 }else{` |
|        - |  936 | `			 /* Compile the file */` |
|     6672 |  937 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|     6672 |  938 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|        - |  939 | `			 /* Release the memory view of the whole file. A ZERO-length view is` |
|        - |  940 | `			  * the answer for a 0-byte file -- a legal, empty PHP program -- and` |
|        - |  941 | `			  * it is not a mapping: the vfs hands back a static empty string, so` |
|        - |  942 | `			  * unmapping it would be a free of something it never allocated. */` |
|     6672 |  943 | `			 if( pVfs->xUnmap && nSize > 0 ){` |
|     6668 |  944 | `				 pVfs->xUnmap(pMapView,nSize);` |
|     3327 |  945 | `			 }` |
|        - |  946 | `		 }` |
|        - |  947 | `	 }` |
|        - |  948 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  949 | `	 /* Leave engine mutex */` |
|     6680 |  950 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  951 | `#endif` |
|        - |  952 | `	/* Compilation result */` |
|     6680 |  953 | `	return rc;` |
|     3338 |  954 | `}` |
|        - |  955 | `/*` |
|        - |  956 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|        - |  957 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  958 | ` */` |
|        2 |  959 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|        1 |  960 | `{` |
|        - |  961 | `	int rc;` |
|        - |  962 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|        3 |  963 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 |  964 | `		return PH7_CORRUPT;` |
|        - |  965 | `	}` |
|        - |  966 | `#ifdef UNTRUST` |
|        - |  967 | `	if( xConsumer == 0 ){` |
|        - |  968 | `		return PH7_CORRUPT;` |
|        - |  969 | `	}` |
|        - |  970 | `#endif` |
|        - |  971 | `	/* Dump VM instructions */` |
|        3 |  972 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|        3 |  973 | `	return rc;` |
|        2 |  974 | `}` |
|        - |  975 | `/*` |
|        - |  976 | ` * [CAPIREF: ph7_vm_config()]` |
|        - |  977 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  978 | ` */` |
|   179967 |  979 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|        5 |  980 | `{` |
|        - |  981 | `	va_list ap;` |
|        - |  982 | `	int rc;` |
|        - |  983 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   179972 |  984 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 |  985 | `		return PH7_CORRUPT;` |
|        - |  986 | `	}` |
|        - |  987 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  988 | `	 /* Acquire VM mutex */` |
|   179972 |  989 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   179972 |  990 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   179967 |  991 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 |  992 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  993 | `	 }` |
|        - |  994 | `#endif` |
|        - |  995 | `	/* Confiugure the virtual machine */` |
|   179972 |  996 | `	va_start(ap,iConfigOp);` |
|   179972 |  997 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   179972 |  998 | `	va_end(ap);` |
|        - |  999 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1000 | `	 /* Leave VM mutex */` |
|   179972 | 1001 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1002 | `#endif` |
|   179972 | 1003 | `	return rc;` |
|    89844 | 1004 | `}` |
|        - | 1005 | `/*` |
|        - | 1006 | ` * [CAPIREF: ph7_vm_exec()]` |
|        - | 1007 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1008 | ` */` |
|     5561 | 1009 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|        5 | 1010 | `{` |
|        - | 1011 | `	int rc;` |
|        - | 1012 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     5566 | 1013 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       16 | 1014 | `		return PH7_CORRUPT;` |
|        - | 1015 | `	}` |
|        - | 1016 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1017 | `	 /* Acquire VM mutex */` |
|     5566 | 1018 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     5566 | 1019 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     5553 | 1020 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1021 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1022 | `	 }` |
|        - | 1023 | `#endif` |
|        - | 1024 | `	/* Execute PH7 byte-code */` |
|     5566 | 1025 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|     5574 | 1026 | `	if( pExitStatus ){` |
|        - | 1027 | `		/* Exit status */` |
|     5528 | 1028 | `		*pExitStatus = pVm->iExitStatus;` |
|     2757 | 1029 | `	}` |
|        - | 1030 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1031 | `	 /* Leave VM mutex */` |
|     5574 | 1032 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1033 | `#endif` |
|        - | 1034 | `	/* Execution result */` |
|     5574 | 1035 | `	return rc;` |
|     2785 | 1036 | `}` |
|        - | 1037 | `/*` |
|        - | 1038 | ` * [CAPIREF: ph7_vm_reset()]` |
|        - | 1039 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1040 | ` */` |
|       16 | 1041 | `int ph7_vm_reset(ph7_vm *pVm)` |
|      ! 0 | 1042 | `{` |
|        - | 1043 | `	int rc;` |
|        - | 1044 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       16 | 1045 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1046 | `		return PH7_CORRUPT;` |
|        - | 1047 | `	}` |
|        - | 1048 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1049 | `	 /* Acquire VM mutex */` |
|       16 | 1050 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       16 | 1051 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       16 | 1052 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1053 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1054 | `	 }` |
|        - | 1055 | `#endif` |
|       16 | 1056 | `	rc = PH7_VmReset(&(*pVm));` |
|        - | 1057 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1058 | `	 /* Leave VM mutex */` |
|       16 | 1059 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1060 | `#endif` |
|       16 | 1061 | `	return rc;` |
|        8 | 1062 | `}` |
|        - | 1063 | `/*` |
|        - | 1064 | ` * [CAPIREF: ph7_vm_release()]` |
|        - | 1065 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1066 | ` */` |
|     5629 | 1067 | `int ph7_vm_release(ph7_vm *pVm)` |
|        5 | 1068 | `{` |
|        - | 1069 | `	ph7 *pEngine;` |
|        - | 1070 | `	int rc;` |
|        - | 1071 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     5634 | 1072 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1073 | `		return PH7_CORRUPT;` |
|        - | 1074 | `	}` |
|        - | 1075 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1076 | `	 /* Acquire VM mutex */` |
|     5634 | 1077 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     5634 | 1078 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     5629 | 1079 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1080 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1081 | `	 }` |
|        - | 1082 | `#endif` |
|     5634 | 1083 | `	pEngine = pVm->pEngine;` |
|     5634 | 1084 | `	rc = PH7_VmRelease(&(*pVm));` |
|        - | 1085 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1086 | `	 /* Leave VM mutex */` |
|     5634 | 1087 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     5634 | 1088 | `	 if( rc == PH7_OK && pVm->pMutex ){` |
|        - | 1089 | `		 /* The per-VM mutex was allocated in ProcessScript and never freed — one` |
|        - | 1090 | `		  * leak per VM, which LeakSanitizer reports on every single run. */` |
|     5634 | 1091 | `		 SyMutexRelease(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|     5634 | 1092 | `		 pVm->pMutex = 0;` |
|     2810 | 1093 | `	 }` |
|        - | 1094 | `#endif` |
|     5634 | 1095 | `	if( rc == PH7_OK ){` |
|        - | 1096 | `		/* Unlink from the list of active VM */` |
|        - | 1097 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1098 | `			/* Acquire engine mutex */` |
|     5634 | 1099 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     5634 | 1100 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     5629 | 1101 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 | 1102 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1103 | `			}` |
|        - | 1104 | `#endif` |
|     5634 | 1105 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|     5634 | 1106 | `		pEngine->iVm--;` |
|        - | 1107 | `		/* Release the memory chunk allocated to this VM */` |
|     5634 | 1108 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|        - | 1109 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1110 | `			/* Leave engine mutex */` |
|     5634 | 1111 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1112 | `#endif` |
|     2810 | 1113 | `	}` |
|     5634 | 1114 | `	return rc;` |
|     2815 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * [CAPIREF: ph7_create_function()]` |
|        - | 1118 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1119 | ` */` |
|  6510534 | 1120 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|        5 | 1121 | `{` |
|        - | 1122 | `	SyString sName;` |
|        - | 1123 | `	int rc;` |
|        - | 1124 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  6510539 | 1125 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1126 | `		return PH7_CORRUPT;` |
|        - | 1127 | `	}` |
|  6510539 | 1128 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|        - | 1129 | `	/* Remove leading and trailing white spaces */` |
|  6510539 | 1130 | `	SyStringFullTrim(&sName);` |
|        - | 1131 | `	/* Ticket 1433-003: NULL values are not allowed */` |
|  6510539 | 1132 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|      ! 0 | 1133 | `		return PH7_CORRUPT;` |
|        - | 1134 | `	}` |
|        - | 1135 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1136 | `	 /* Acquire VM mutex */` |
|  6510539 | 1137 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  6510539 | 1138 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  6510534 | 1139 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1140 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1141 | `	 }` |
|        - | 1142 | `#endif` |
|        - | 1143 | `	/* Install the foreign function */` |
|  6510539 | 1144 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|        - | 1145 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1146 | `	 /* Leave VM mutex */` |
|  6510539 | 1147 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1148 | `#endif` |
|  6510539 | 1149 | `	return rc;` |
|  3241889 | 1150 | `}` |
|        - | 1151 | `/*` |
|        - | 1152 | ` * [CAPIREF: ph7_delete_function()]` |
|        - | 1153 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1154 | ` */` |
|      ! 0 | 1155 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|      ! 0 | 1156 | `{` |
|      ! 0 | 1157 | `	ph7_user_func *pFunc = 0;` |
|        - | 1158 | `	int rc;` |
|        - | 1159 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      ! 0 | 1160 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1161 | `		return PH7_CORRUPT;` |
|        - | 1162 | `	}` |
|        - | 1163 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1164 | `	 /* Acquire VM mutex */` |
|      ! 0 | 1165 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 | 1166 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 | 1167 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1168 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1169 | `	 }` |
|        - | 1170 | `#endif` |
|        - | 1171 | `	/* Perform the deletion */` |
|      ! 0 | 1172 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|      ! 0 | 1173 | `	if( rc == PH7_OK ){` |
|        - | 1174 | `		/* A name that WAS callable is not any more; every call site that remembers` |
|        - | 1175 | `		 * having screened it has to ask again (OP_CALL_INIT). */` |
|      ! 0 | 1176 | `		pVm->nCallableGen++;` |
|        - | 1177 | `		/* Release internal fields */` |
|      ! 0 | 1178 | `		SySetRelease(&pFunc->aAux);` |
|      ! 0 | 1179 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 | 1180 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 | 1181 | `	}` |
|        - | 1182 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1183 | `	 /* Leave VM mutex */` |
|      ! 0 | 1184 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1185 | `#endif` |
|      ! 0 | 1186 | `	return rc;` |
|      ! 0 | 1187 | `}` |
|        - | 1188 | `/*` |
|        - | 1189 | ` * [CAPIREF: ph7_create_constant()]` |
|        - | 1190 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1191 | ` */` |
| 10762749 | 1192 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|        5 | 1193 | `{` |
|        - | 1194 | `	SyString sName;` |
|        - | 1195 | `	int rc;` |
|        - | 1196 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 10762754 | 1197 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1198 | `		return PH7_CORRUPT;` |
|        - | 1199 | `	}` |
| 10762754 | 1200 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|        - | 1201 | `	/* Remove leading and trailing white spaces */` |
| 10768373 | 1202 | `	SyStringFullTrim(&sName);` |
| 10762754 | 1203 | `	if( sName.nByte < 1 ){` |
|        - | 1204 | `		/* Empty constant name */` |
|      ! 0 | 1205 | `		return PH7_CORRUPT;` |
|        - | 1206 | `	}` |
|        - | 1207 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
| 10762754 | 1208 | `	if( xExpand == 0 ){` |
|      ! 0 | 1209 | `		return PH7_CORRUPT;` |
|        - | 1210 | `	}` |
|        - | 1211 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1212 | `	 /* Acquire VM mutex */` |
| 10762754 | 1213 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 10762754 | 1214 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 10762749 | 1215 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1216 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1217 | `	 }` |
|        - | 1218 | `#endif` |
|        - | 1219 | `	/* Perform the registration */` |
| 10762754 | 1220 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|        - | 1221 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1222 | `	 /* Leave VM mutex */` |
| 10762754 | 1223 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1224 | `#endif` |
| 10762754 | 1225 | `	 return rc;` |
|  5214059 | 1226 | `}` |
|        - | 1227 | `/*` |
|        - | 1228 | ` * [CAPIREF: ph7_delete_constant()]` |
|        - | 1229 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1230 | ` */` |
|      ! 0 | 1231 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|      ! 0 | 1232 | `{` |
|        - | 1233 | `	ph7_constant *pCons;` |
|        - | 1234 | `	int rc;` |
|        - | 1235 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      ! 0 | 1236 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1237 | `		return PH7_CORRUPT;` |
|        - | 1238 | `	}` |
|        - | 1239 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1240 | `	 /* Acquire VM mutex */` |
|      ! 0 | 1241 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 | 1242 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 | 1243 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1244 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1245 | `	 }` |
|        - | 1246 | `#endif` |
|        - | 1247 | `	 /* Query the constant hashtable */` |
|      ! 0 | 1248 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|      ! 0 | 1249 | `	 if( rc == PH7_OK ){` |
|        - | 1250 | `		 /* A name that WAS a constant is not any more, and the entry every LOADC site` |
|        - | 1251 | `		  * that resolved to it remembers is about to be freed (PH7_VmConstSiteAnswer). */` |
|      ! 0 | 1252 | `		 pVm->nConstGen++;` |
|        - | 1253 | `		 /* Perform the deletion */` |
|      ! 0 | 1254 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|      ! 0 | 1255 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 | 1256 | `	 }` |
|        - | 1257 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1258 | `	 /* Leave VM mutex */` |
|      ! 0 | 1259 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1260 | `#endif` |
|      ! 0 | 1261 | `	return rc;` |
|      ! 0 | 1262 | `}` |
|        - | 1263 | `/*` |
|        - | 1264 | ` * [CAPIREF: ph7_new_scalar()]` |
|        - | 1265 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1266 | ` */` |
|  1935395 | 1267 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|        5 | 1268 | `{` |
|        - | 1269 | `	ph7_value *pObj;` |
|        - | 1270 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  1935400 | 1271 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1272 | `		return 0;` |
|        - | 1273 | `	}` |
|        - | 1274 | `	/* Allocate a new scalar variable */` |
|  1935400 | 1275 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  1935400 | 1276 | `	if( pObj == 0 ){` |
|      ! 0 | 1277 | `		return 0;` |
|        - | 1278 | `	}` |
|        - | 1279 | `	/* Nullify the new scalar */` |
|  1935400 | 1280 | `	PH7_MemObjInit(pVm,pObj);` |
|  1935400 | 1281 | `	return pObj;` |
|   967591 | 1282 | `}` |
|        - | 1283 | `/*` |
|        - | 1284 | ` * [CAPIREF: ph7_new_array()]` |
|        - | 1285 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1286 | ` */` |
|  3112970 | 1287 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|        5 | 1288 | `{` |
|        - | 1289 | `	ph7_hashmap *pMap;` |
|        - | 1290 | `	ph7_value *pObj;` |
|        - | 1291 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  3112975 | 1292 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1293 | `		return 0;` |
|        - | 1294 | `	}` |
|        - | 1295 | `	/* Create a new hashmap first */` |
|  3112975 | 1296 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  3112975 | 1297 | `	if( pMap == 0 ){` |
|      ! 0 | 1298 | `		return 0;` |
|        - | 1299 | `	}` |
|        - | 1300 | `	/* Associate a new ph7_value with this hashmap */` |
|  3112975 | 1301 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  3112975 | 1302 | `	if( pObj == 0 ){` |
|      ! 0 | 1303 | `		PH7_HashmapRelease(pMap,TRUE);` |
|      ! 0 | 1304 | `		return 0;` |
|        - | 1305 | `	}` |
|  3112975 | 1306 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|  3112975 | 1307 | `	return pObj;` |
|  1556286 | 1308 | `}` |
|        - | 1309 | `/*` |
|        - | 1310 | ` * [CAPIREF: ph7_release_value()]` |
|        - | 1311 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1312 | ` */` |
|  3755659 | 1313 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|        5 | 1314 | `{` |
|        - | 1315 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  3755664 | 1316 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       74 | 1317 | `		return PH7_CORRUPT;` |
|        - | 1318 | `	}` |
|  3755592 | 1319 | `	if( pValue ){` |
|        - | 1320 | `		/* Release the value */` |
|  3755592 | 1321 | `		PH7_MemObjRelease(pValue);` |
|  3755592 | 1322 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|  1877620 | 1323 | `	}` |
|  3755592 | 1324 | `	return PH7_OK;` |
|  1877661 | 1325 | `}` |
|        - | 1326 | `/*` |
|        - | 1327 | ` * [CAPIREF: ph7_value_to_int()]` |
|        - | 1328 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1329 | ` */` |
|    61829 | 1330 | `int ph7_value_to_int(ph7_value *pValue)` |
|        5 | 1331 | `{` |
|        - | 1332 | `	int rc;` |
|    61834 | 1333 | `	rc = PH7_MemObjToInteger(pValue);` |
|    61834 | 1334 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1335 | `		return 0;` |
|        - | 1336 | `	}` |
|    61834 | 1337 | `	return (int)pValue->x.iVal;` |
|    31085 | 1338 | `}` |
|        - | 1339 | `/*` |
|        - | 1340 | ` * [CAPIREF: ph7_value_to_bool()]` |
|        - | 1341 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1342 | ` */` |
|    48712 | 1343 | `int ph7_value_to_bool(ph7_value *pValue)` |
|        5 | 1344 | `{` |
|        - | 1345 | `	int rc;` |
|    48717 | 1346 | `	rc = PH7_MemObjToBool(pValue);` |
|    48717 | 1347 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1348 | `		return 0;` |
|        - | 1349 | `	}` |
|    48717 | 1350 | `	return (int)pValue->x.iVal;` |
|    24116 | 1351 | `}` |
|        - | 1352 | `/*` |
|        - | 1353 | ` * [CAPIREF: ph7_value_to_int64()]` |
|        - | 1354 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1355 | ` */` |
|  4345930 | 1356 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|        5 | 1357 | `{` |
|        - | 1358 | `	int rc;` |
|  4345935 | 1359 | `	rc = PH7_MemObjToInteger(pValue);` |
|  4345935 | 1360 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1361 | `		return 0;` |
|        - | 1362 | `	}` |
|  4345935 | 1363 | `	return pValue->x.iVal;` |
|  2173261 | 1364 | `}` |
|        - | 1365 | `/*` |
|        - | 1366 | ` * [CAPIREF: ph7_value_to_double()]` |
|        - | 1367 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1368 | ` */` |
|     3522 | 1369 | `double ph7_value_to_double(ph7_value *pValue)` |
|        5 | 1370 | `{` |
|        - | 1371 | `	int rc;` |
|     3527 | 1372 | `	rc = PH7_MemObjToReal(pValue);` |
|     3527 | 1373 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1374 | `		return (double)0;` |
|        - | 1375 | `	}` |
|     3527 | 1376 | `	return (double)pValue->rVal;` |
|     1768 | 1377 | `}` |
|        - | 1378 | `/*` |
|        - | 1379 | ` * [CAPIREF: ph7_value_to_string()]` |
|        - | 1380 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1381 | ` */` |
|  5231032 | 1382 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|        5 | 1383 | `{` |
|  5231037 | 1384 | `	PH7_MemObjToString(pValue);` |
|  5231037 | 1385 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  5109159 | 1386 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  5109159 | 1387 | `		if( pLen ){` |
|  4875723 | 1388 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  2437434 | 1389 | `		}` |
|  5109159 | 1390 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|      ! 0 | 1391 | `	}else{` |
|        - | 1392 | `		/* Return the empty string */` |
|   121883 | 1393 | `		if( pLen ){` |
|   121441 | 1394 | `			*pLen = 0;` |
|    60566 | 1395 | `		}` |
|   121883 | 1396 | `		return "";` |
|        - | 1397 | `	}` |
|  2614858 | 1398 | `}` |
|        - | 1399 | `/*` |
|        - | 1400 | ` * [CAPIREF: ph7_value_to_resource()]` |
|        - | 1401 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1402 | ` */` |
|   127230 | 1403 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|        5 | 1404 | `{` |
|   127235 | 1405 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|        - | 1406 | `		/* Not a resource,return NULL */` |
|      ! 0 | 1407 | `		return 0;` |
|        - | 1408 | `	}` |
|   127235 | 1409 | `	return pValue->x.pOther;` |
|    62958 | 1410 | `}` |
|        - | 1411 | `/*` |
|        - | 1412 | ` * [CAPIREF: ph7_value_compare()]` |
|        - | 1413 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1414 | ` */` |
|       80 | 1415 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|        2 | 1416 | `{` |
|        - | 1417 | `	int rc;` |
|       82 | 1418 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|        - | 1419 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|      ! 0 | 1420 | `		return 1;` |
|        - | 1421 | `	}` |
|        - | 1422 | `	/* Perform the comparison */` |
|       82 | 1423 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|        - | 1424 | `	/* A native compare handler may have REFUSED the pair (php throws comparing` |
|        - | 1425 | `	 * two different KINDS of DateTimeZone). The record is deliberately LEFT` |
|        - | 1426 | `	 * standing: array_keys() reaches the comparator through this entry point, and` |
|        - | 1427 | `	 * the host-call boundary raises what it recorded exactly as it does for the` |
|        - | 1428 | `	 * builtins that call PH7_MemObjCmp directly. A host application driving this` |
|        - | 1429 | `	 * outside any execution never sees the throw, and PH7_VmInit/PH7_VmReset` |
|        - | 1430 | `	 * clear the record before the next one begins. */` |
|        - | 1431 | `	/* Comparison result */` |
|       82 | 1432 | `	return rc;` |
|       42 | 1433 | `}` |
|        - | 1434 | `/*` |
|        - | 1435 | ` * [CAPIREF: ph7_result_int()]` |
|        - | 1436 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1437 | ` */` |
|   241016 | 1438 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|        5 | 1439 | `{` |
|   241021 | 1440 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|        5 | 1441 | `}` |
|        - | 1442 | `/*` |
|        - | 1443 | ` * [CAPIREF: ph7_result_int64()]` |
|        - | 1444 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1445 | ` */` |
|  1680583 | 1446 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|        5 | 1447 | `{` |
|  1680588 | 1448 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|        5 | 1449 | `}` |
|        - | 1450 | `/*` |
|        - | 1451 | ` * [CAPIREF: ph7_result_bool()]` |
|        - | 1452 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1453 | ` */` |
|   639617 | 1454 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|        5 | 1455 | `{` |
|   639622 | 1456 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|        5 | 1457 | `}` |
|        - | 1458 | `/*` |
|        - | 1459 | ` * [CAPIREF: ph7_result_double()]` |
|        - | 1460 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1461 | ` */` |
|     1178 | 1462 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|        4 | 1463 | `{` |
|     1182 | 1464 | `	return ph7_value_double(pCtx->pRet,Value);` |
|        4 | 1465 | `}` |
|        - | 1466 | `/*` |
|        - | 1467 | ` * [CAPIREF: ph7_result_null()]` |
|        - | 1468 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1469 | ` */` |
|    12096 | 1470 | `int ph7_result_null(ph7_context *pCtx)` |
|        5 | 1471 | `{` |
|        - | 1472 | `	/* Invalidate any prior representation and set the NULL flag */` |
|    12101 | 1473 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    12101 | 1474 | `	return PH7_OK;` |
|        5 | 1475 | `}` |
|        - | 1476 | `/*` |
|        - | 1477 | ` * [CAPIREF: ph7_result_string()]` |
|        - | 1478 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1479 | ` */` |
|  4335430 | 1480 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|        5 | 1481 | `{` |
|  4335435 | 1482 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|        5 | 1483 | `}` |
|        - | 1484 | `/*` |
|        - | 1485 | ` * [CAPIREF: ph7_result_string_format()]` |
|        - | 1486 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1487 | ` */` |
|   409310 | 1488 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|        5 | 1489 | `{` |
|        - | 1490 | `	ph7_value *p;` |
|        - | 1491 | `	va_list ap;` |
|        - | 1492 | `	int rc;` |
|   409315 | 1493 | `	p = pCtx->pRet;` |
|   409315 | 1494 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1495 | `		/* Invalidate any prior representation */` |
|   395473 | 1496 | `		PH7_MemObjRelease(p);` |
|   395473 | 1497 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|   197734 | 1498 | `	}` |
|        - | 1499 | `	/* Format the given string */` |
|   409315 | 1500 | `	va_start(ap,zFormat);` |
|   409315 | 1501 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|   409315 | 1502 | `	va_end(ap);` |
|   409315 | 1503 | `	return rc;` |
|        5 | 1504 | `}` |
|        - | 1505 | `/*` |
|        - | 1506 | ` * [CAPIREF: ph7_result_value()]` |
|        - | 1507 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1508 | ` */` |
|   884808 | 1509 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|        5 | 1510 | `{` |
|   884813 | 1511 | `	int rc = PH7_OK;` |
|   884813 | 1512 | `	if( pValue == 0 ){` |
|      ! 0 | 1513 | `		PH7_MemObjRelease(pCtx->pRet);` |
|      ! 0 | 1514 | `	}else{` |
|   884813 | 1515 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|        - | 1516 | `	}` |
|   884813 | 1517 | `	return rc;` |
|        5 | 1518 | `}` |
|        - | 1519 | `/*` |
|        - | 1520 | ` * [CAPIREF: ph7_result_resource()]` |
|        - | 1521 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1522 | ` */` |
|    10589 | 1523 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|        5 | 1524 | `{` |
|    10594 | 1525 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|        5 | 1526 | `}` |
|        - | 1527 | `/*` |
|        - | 1528 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|        - | 1529 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1530 | ` */` |
|   433621 | 1531 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|        5 | 1532 | `{` |
|        - | 1533 | `	ph7_value *pVal;` |
|   433626 | 1534 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|   433626 | 1535 | `	if( pVal ){` |
|        - | 1536 | `		/* Record value address so it can be freed automatically` |
|        - | 1537 | `		 * when the calling function returns.` |
|        - | 1538 | `		 */` |
|   433626 | 1539 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   216742 | 1540 | `	}` |
|   433626 | 1541 | `	return pVal;` |
|        5 | 1542 | `}` |
|        - | 1543 | `/*` |
|        - | 1544 | ` * [CAPIREF: ph7_context_new_array()]` |
|        - | 1545 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1546 | ` */` |
|   858423 | 1547 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|        5 | 1548 | `{` |
|        - | 1549 | `	ph7_value *pVal;` |
|   858428 | 1550 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   858428 | 1551 | `	if( pVal ){` |
|        - | 1552 | `		/* Record value address so it can be freed automatically` |
|        - | 1553 | `		 * when the calling function returns.` |
|        - | 1554 | `		 */` |
|   858428 | 1555 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   429140 | 1556 | `	}` |
|   858428 | 1557 | `	return pVal;` |
|        5 | 1558 | `}` |
|        - | 1559 | `/*` |
|        - | 1560 | ` * [CAPIREF: ph7_context_release_value()]` |
|        - | 1561 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1562 | ` */` |
|    55288 | 1563 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|        5 | 1564 | `{` |
|    55293 | 1565 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|    55293 | 1566 | `}` |
|        - | 1567 | `/*` |
|        - | 1568 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|        - | 1569 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1570 | ` */` |
|    33983 | 1571 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|        5 | 1572 | `{` |
|        - | 1573 | `	void *pChunk;` |
|    33988 | 1574 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    33988 | 1575 | `	if( pChunk ){` |
|    33988 | 1576 | `		if( ZeroChunk ){` |
|        - | 1577 | `			/* Zero the memory chunk */` |
|    23234 | 1578 | `			SyZero(pChunk,nByte);` |
|    11465 | 1579 | `		}` |
|    33988 | 1580 | `		if( AutoRelease ){` |
|        - | 1581 | `			ph7_aux_data sAux;` |
|        - | 1582 | `			/* Track the chunk so that it can be released automatically` |
|        - | 1583 | `			 * upon this context is destroyed.` |
|        - | 1584 | `			 */` |
|    22588 | 1585 | `			sAux.pAuxData = pChunk;` |
|    22588 | 1586 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|    11147 | 1587 | `		}` |
|    16833 | 1588 | `	}` |
|    33988 | 1589 | `	return pChunk;` |
|        5 | 1590 | `}` |
|        - | 1591 | `/*` |
|        - | 1592 | ` * Check if the given chunk address is registered in the call context` |
|        - | 1593 | ` * chunk container.` |
|        - | 1594 | ` * Return TRUE if registered.FALSE otherwise.` |
|        - | 1595 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|        - | 1596 | ` */` |
|     1599 | 1597 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|        5 | 1598 | `{` |
|        - | 1599 | `	ph7_aux_data *aAux,*pAux;` |
|        - | 1600 | `	sxu32 n;` |
|     1604 | 1601 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|        - | 1602 | `		/* Don't bother processing,the container is empty */` |
|      912 | 1603 | `		return 0;` |
|        - | 1604 | `	}` |
|        - | 1605 | `	/* Perform the lookup */` |
|      696 | 1606 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     1068 | 1607 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     1068 | 1608 | `		pAux = &aAux[n];` |
|     1068 | 1609 | `		if( pAux->pAuxData == pChunk ){` |
|        - | 1610 | `			/* Chunk found */` |
|      696 | 1611 | `			return pAux;` |
|        - | 1612 | `		}` |
|      189 | 1613 | `	}` |
|        - | 1614 | `	/* No such allocated chunk */` |
|      ! 0 | 1615 | `	return 0;` |
|      797 | 1616 | `}` |
|        - | 1617 | `/*` |
|        - | 1618 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|        - | 1619 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1620 | ` */` |
|      ! 0 | 1621 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|      ! 0 | 1622 | `{` |
|        - | 1623 | `	ph7_aux_data *pAux;` |
|        - | 1624 | `	void *pNew;` |
|      ! 0 | 1625 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|      ! 0 | 1626 | `	if( pNew ){` |
|      ! 0 | 1627 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|      ! 0 | 1628 | `		if( pAux ){` |
|      ! 0 | 1629 | `			pAux->pAuxData = pNew;` |
|      ! 0 | 1630 | `		}` |
|      ! 0 | 1631 | `	}` |
|      ! 0 | 1632 | `	return pNew;` |
|      ! 0 | 1633 | `}` |
|        - | 1634 | `/*` |
|        - | 1635 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|        - | 1636 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1637 | ` */` |
|     1599 | 1638 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|        5 | 1639 | `{` |
|        - | 1640 | `	ph7_aux_data *pAux;` |
|     1604 | 1641 | `	if( pChunk == 0 ){` |
|        - | 1642 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|      ! 0 | 1643 | `		return;` |
|        - | 1644 | `	}` |
|     1604 | 1645 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|     1604 | 1646 | `	if( pAux ){` |
|        - | 1647 | `		/* Mark as destroyed */` |
|      696 | 1648 | `		pAux->pAuxData = 0;` |
|      346 | 1649 | `	}` |
|     1604 | 1650 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      797 | 1651 | `}` |
|        - | 1652 | `/*` |
|        - | 1653 | ` * [CAPIREF: ph7_array_fetch()]` |
|        - | 1654 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1655 | ` */` |
|    19146 | 1656 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|        5 | 1657 | `{` |
|        - | 1658 | `	ph7_hashmap_node *pNode;` |
|        - | 1659 | `	ph7_value *pValue;` |
|        - | 1660 | `	ph7_value skey;` |
|        - | 1661 | `	int rc;` |
|        - | 1662 | `	/* Make sure we are dealing with a valid hashmap */` |
|    19151 | 1663 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1664 | `		return 0;` |
|        - | 1665 | `	}` |
|    19151 | 1666 | `	if( nByte < 0 ){` |
|    14787 | 1667 | `		nByte = (int)SyStrlen(zKey);` |
|     7391 | 1668 | `	}` |
|        - | 1669 | `	/* Convert the key to a ph7_value  */` |
|    19151 | 1670 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|    19151 | 1671 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|        - | 1672 | `	/* Perform the lookup */` |
|    19151 | 1673 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|    19151 | 1674 | `	PH7_MemObjRelease(&skey);` |
|    19151 | 1675 | `	if( rc != PH7_OK ){` |
|        - | 1676 | `		/* No such entry */` |
|     8647 | 1677 | `		return 0;` |
|        - | 1678 | `	}` |
|        - | 1679 | `	/* Extract the target value */` |
|    10509 | 1680 | `	pValue = (ph7_value *)PH7_MemObjAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|    10509 | 1681 | `	return pValue;` |
|     9578 | 1682 | `}` |
|        - | 1683 | `/*` |
|        - | 1684 | ` * [CAPIREF: ph7_array_walk()]` |
|        - | 1685 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1686 | ` */` |
|    67336 | 1687 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|        5 | 1688 | `{` |
|        - | 1689 | `	int rc;` |
|    67341 | 1690 | `	if( xWalk == 0 ){` |
|      ! 0 | 1691 | `		return PH7_CORRUPT;` |
|        - | 1692 | `	}` |
|        - | 1693 | `	/* Make sure we are dealing with a valid hashmap */` |
|    67341 | 1694 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1695 | `		return PH7_CORRUPT;` |
|        - | 1696 | `	}` |
|        - | 1697 | `	/* Start the walk process */` |
|    67341 | 1698 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|    67341 | 1699 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|    33648 | 1700 | `}` |
|        - | 1701 | `/*` |
|        - | 1702 | ` * [CAPIREF: ph7_array_add_elem()]` |
|        - | 1703 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1704 | ` */` |
|  4200762 | 1705 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|        5 | 1706 | `{` |
|        - | 1707 | `	int rc;` |
|        - | 1708 | `	/* Make sure we are dealing with a valid hashmap */` |
|  4200767 | 1709 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1710 | `		return PH7_CORRUPT;` |
|        - | 1711 | `	}` |
|        - | 1712 | `	/* Perform the insertion */` |
|  4200767 | 1713 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
|  4200767 | 1714 | `	return rc;` |
|  2098215 | 1715 | `}` |
|        - | 1716 | `/*` |
|        - | 1717 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|        - | 1718 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1719 | ` */` |
|  3268924 | 1720 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|        5 | 1721 | `{` |
|        - | 1722 | `	int rc;` |
|        - | 1723 | `	/* Make sure we are dealing with a valid hashmap */` |
|  3268929 | 1724 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1725 | `		return PH7_CORRUPT;` |
|        - | 1726 | `	}` |
|        - | 1727 | `	/* Perform the insertion */` |
|  3268929 | 1728 | `	if( SX_EMPTY_STR(zKey) ){` |
|        - | 1729 | `		/* Empty key,assign an automatic index */` |
|       17 | 1730 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|        9 | 1731 | `	}else{` |
|        - | 1732 | `		ph7_value sKey;` |
|  3268913 | 1733 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|  3268913 | 1734 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|  3268913 | 1735 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  3268913 | 1736 | `		PH7_MemObjRelease(&sKey);` |
|        - | 1737 | `	}` |
|  3268929 | 1738 | `	return rc;` |
|  1633989 | 1739 | `}` |
|        - | 1740 | `/*` |
|        - | 1741 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|        - | 1742 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1743 | ` */` |
|  2128630 | 1744 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|        5 | 1745 | `{` |
|        - | 1746 | `	ph7_value sKey;` |
|        - | 1747 | `	int rc;` |
|        - | 1748 | `	/* Make sure we are dealing with a valid hashmap */` |
|  2128635 | 1749 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1750 | `		return PH7_CORRUPT;` |
|        - | 1751 | `	}` |
|  2128635 | 1752 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|        - | 1753 | `	/* Perform the insertion */` |
|  2128635 | 1754 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  2128635 | 1755 | `	PH7_MemObjRelease(&sKey);` |
|  2128635 | 1756 | `	return rc;` |
|  1064320 | 1757 | `}` |
|        - | 1758 | `/*` |
|        - | 1759 | ` * [CAPIREF: ph7_array_count()]` |
|        - | 1760 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1761 | ` */` |
|  1275688 | 1762 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|        5 | 1763 | `{` |
|        - | 1764 | `	ph7_hashmap *pMap;` |
|        - | 1765 | `	/* Make sure we are dealing with a valid hashmap */` |
|  1275693 | 1766 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1767 | `		return 0;` |
|        - | 1768 | `	}` |
|        - | 1769 | `	/* Point to the internal representation of the hashmap */` |
|  1275693 | 1770 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  1275693 | 1771 | `	return pMap->nEntry;` |
|   637847 | 1772 | `}` |
|        - | 1773 | `/*` |
|        - | 1774 | ` * [CAPIREF: ph7_object_walk()]` |
|        - | 1775 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1776 | ` */` |
|      ! 0 | 1777 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|      ! 0 | 1778 | `{` |
|        - | 1779 | `	int rc;` |
|      ! 0 | 1780 | `	if( xWalk == 0 ){` |
|      ! 0 | 1781 | `		return PH7_CORRUPT;` |
|        - | 1782 | `	}` |
|        - | 1783 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1784 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 1785 | `		return PH7_CORRUPT;` |
|        - | 1786 | `	}` |
|        - | 1787 | `	/* Start the walk process */` |
|      ! 0 | 1788 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|      ! 0 | 1789 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|      ! 0 | 1790 | `}` |
|        - | 1791 | `/*` |
|        - | 1792 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|        - | 1793 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1794 | ` */` |
|       17 | 1795 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|        2 | 1796 | `{` |
|        - | 1797 | `	ph7_value *pValue;` |
|        - | 1798 | `	SyString sAttr;` |
|        - | 1799 | `	/* Make sure we are dealing with a valid class instance */` |
|       19 | 1800 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|      ! 0 | 1801 | `		return 0;` |
|        - | 1802 | `	}` |
|       19 | 1803 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|        - | 1804 | `	/* Extract the attribute value if available.` |
|        - | 1805 | `	 */` |
|       19 | 1806 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       19 | 1807 | `	return pValue;` |
|        8 | 1808 | `}` |
|        - | 1809 | `/*` |
|        - | 1810 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|        - | 1811 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1812 | ` */` |
|      ! 0 | 1813 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|      ! 0 | 1814 | `{` |
|        - | 1815 | `	ph7_class *pClass;` |
|      ! 0 | 1816 | `	if( pLength ){` |
|      ! 0 | 1817 | `		*pLength = 0;` |
|      ! 0 | 1818 | `	}` |
|        - | 1819 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1820 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|      ! 0 | 1821 | `		return 0;` |
|        - | 1822 | `	}` |
|        - | 1823 | `	/* Point to the class */` |
|      ! 0 | 1824 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|        - | 1825 | `	/* Return the class name */` |
|      ! 0 | 1826 | `	if( pLength ){` |
|      ! 0 | 1827 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|      ! 0 | 1828 | `	}` |
|      ! 0 | 1829 | `	return SyStringData(&pClass->sName);` |
|      ! 0 | 1830 | `}` |
|        - | 1831 | `/*` |
|        - | 1832 | ` * [CAPIREF: ph7_context_output()]` |
|        - | 1833 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1834 | ` */` |
|   125938 | 1835 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|        5 | 1836 | `{` |
|        - | 1837 | `	SyString sData;` |
|        - | 1838 | `	int rc;` |
|   125943 | 1839 | `	if( nLen < 0 ){` |
|      ! 0 | 1840 | `		nLen = (int)SyStrlen(zString);` |
|      ! 0 | 1841 | `	}` |
|   125943 | 1842 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|   125943 | 1843 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|   125943 | 1844 | `	return rc;` |
|        5 | 1845 | `}` |
|        - | 1846 | `/*` |
|        - | 1847 | ` * [CAPIREF: ph7_context_output_format()]` |
|        - | 1848 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1849 | ` */` |
|       30 | 1850 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|        2 | 1851 | `{` |
|        - | 1852 | `	va_list ap;` |
|        - | 1853 | `	int rc;` |
|       32 | 1854 | `	va_start(ap,zFormat);` |
|       32 | 1855 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       32 | 1856 | `	va_end(ap);` |
|       32 | 1857 | `	return rc;` |
|        2 | 1858 | `}` |
|        - | 1859 | `/*` |
|        - | 1860 | ` * [CAPIREF: ph7_context_throw_error()]` |
|        - | 1861 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1862 | ` */` |
|      404 | 1863 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|        5 | 1864 | `{` |
|      409 | 1865 | `	int rc = PH7_OK;` |
|      409 | 1866 | `	if( zErr ){` |
|      409 | 1867 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      202 | 1868 | `	}` |
|      409 | 1869 | `	return rc;` |
|        5 | 1870 | `}` |
|        - | 1871 | `/*` |
|        - | 1872 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|        - | 1873 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1874 | ` */` |
|     1109 | 1875 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|        5 | 1876 | `{` |
|        - | 1877 | `	va_list ap;` |
|        - | 1878 | `	int rc;` |
|     1114 | 1879 | `	if( zFormat == 0){` |
|      ! 0 | 1880 | `		return PH7_OK;` |
|        - | 1881 | `	}` |
|     1114 | 1882 | `	va_start(ap,zFormat);` |
|     1114 | 1883 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|     1114 | 1884 | `	va_end(ap);` |
|     1114 | 1885 | `	return rc;` |
|      554 | 1886 | `}` |
|        - | 1887 | `/*` |
|        - | 1888 | ` * [CAPIREF: ph7_context_random_num()]` |
|        - | 1889 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1890 | ` */` |
|      ! 0 | 1891 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|      ! 0 | 1892 | `{` |
|        - | 1893 | `	sxu32 n;` |
|      ! 0 | 1894 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      ! 0 | 1895 | `	return n;` |
|      ! 0 | 1896 | `}` |
|        - | 1897 | `/*` |
|        - | 1898 | ` * [CAPIREF: ph7_context_random_string()]` |
|        - | 1899 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1900 | ` */` |
|      ! 0 | 1901 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|      ! 0 | 1902 | `{` |
|      ! 0 | 1903 | `	if( nBuflen < 3 ){` |
|      ! 0 | 1904 | `		return PH7_CORRUPT;` |
|        - | 1905 | `	}` |
|      ! 0 | 1906 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|      ! 0 | 1907 | `	return PH7_OK;` |
|      ! 0 | 1908 | `}` |
|        - | 1909 | `/*` |
|        - | 1910 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|        - | 1911 | ` *` |
|        - | 1912 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|        - | 1913 | ` * {` |
|        - | 1914 | ` *	return pCtx->pVm;` |
|        - | 1915 | ` * }` |
|        - | 1916 | ` */` |
|        - | 1917 | `/*` |
|        - | 1918 | ` * [CAPIREF: ph7_context_user_data()]` |
|        - | 1919 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1920 | ` */` |
|    95539 | 1921 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|        5 | 1922 | `{` |
|    95544 | 1923 | `	return pCtx->pFunc->pUserData;` |
|        5 | 1924 | `}` |
|        - | 1925 | `/*` |
|        - | 1926 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|        - | 1927 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1928 | ` */` |
|      212 | 1929 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|        1 | 1930 | `{` |
|        - | 1931 | `	ph7_aux_data sAux;` |
|        - | 1932 | `	int rc;` |
|      213 | 1933 | `	sAux.pAuxData = pUserData;` |
|      213 | 1934 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|      213 | 1935 | `	return rc;` |
|        1 | 1936 | `}` |
|        - | 1937 | `/*` |
|        - | 1938 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|        - | 1939 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1940 | ` */` |
|        4 | 1941 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|        1 | 1942 | `{` |
|        - | 1943 | `	ph7_aux_data *pAux;` |
|        5 | 1944 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|        5 | 1945 | `	return pAux ? pAux->pAuxData : 0;` |
|        1 | 1946 | `}` |
|        - | 1947 | `/*` |
|        - | 1948 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|        - | 1949 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1950 | ` */` |
|      ! 0 | 1951 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|      ! 0 | 1952 | `{` |
|        - | 1953 | `	ph7_aux_data *pAux;` |
|      ! 0 | 1954 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|      ! 0 | 1955 | `	return pAux ? pAux->pAuxData : 0;` |
|      ! 0 | 1956 | `}` |
|        - | 1957 | `/*` |
|        - | 1958 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|        - | 1959 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1960 | ` */` |
|    67548 | 1961 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|        5 | 1962 | `{` |
|    67553 | 1963 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|        5 | 1964 | `}` |
|        - | 1965 | `/*` |
|        - | 1966 | ` * [CAPIREF: ph7_function_name()]` |
|        - | 1967 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1968 | ` */` |
|   214088 | 1969 | `const char * ph7_function_name(ph7_context *pCtx)` |
|        5 | 1970 | `{` |
|        - | 1971 | `	SyString *pName;` |
|   214093 | 1972 | `	pName = &pCtx->pFunc->sName;` |
|   214093 | 1973 | `	return pName->zString;` |
|        5 | 1974 | `}` |
|        - | 1975 | `/*` |
|        - | 1976 | ` * [CAPIREF: ph7_value_int()]` |
|        - | 1977 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1978 | ` */` |
|  1102205 | 1979 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|        5 | 1980 | `{` |
|        - | 1981 | `	/* Invalidate any prior representation */` |
|  1102210 | 1982 | `	PH7_MemObjRelease(pVal);` |
|  1102210 | 1983 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|  1102210 | 1984 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  1102210 | 1985 | `	return PH7_OK;` |
|        5 | 1986 | `}` |
|        - | 1987 | `/*` |
|        - | 1988 | ` * [CAPIREF: ph7_value_int64()]` |
|        - | 1989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1990 | ` */` |
|  2187085 | 1991 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|        5 | 1992 | `{` |
|        - | 1993 | `	/* Invalidate any prior representation */` |
|  2187090 | 1994 | `	PH7_MemObjRelease(pVal);` |
|  2187090 | 1995 | `	pVal->x.iVal = iValue;` |
|  2187090 | 1996 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  2187090 | 1997 | `	return PH7_OK;` |
|        5 | 1998 | `}` |
|        - | 1999 | `/*` |
|        - | 2000 | ` * [CAPIREF: ph7_value_bool()]` |
|        - | 2001 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2002 | ` */` |
|   642501 | 2003 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|        5 | 2004 | `{` |
|        - | 2005 | `	/* Invalidate any prior representation */` |
|   642506 | 2006 | `	PH7_MemObjRelease(pVal);` |
|   642506 | 2007 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|   642506 | 2008 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|   642506 | 2009 | `	return PH7_OK;` |
|        5 | 2010 | `}` |
|        - | 2011 | `/*` |
|        - | 2012 | ` * [CAPIREF: ph7_value_null()]` |
|        - | 2013 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2014 | ` */` |
|     2126 | 2015 | `int ph7_value_null(ph7_value *pVal)` |
|        5 | 2016 | `{` |
|        - | 2017 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     2131 | 2018 | `	PH7_MemObjRelease(pVal);` |
|     2131 | 2019 | `	return PH7_OK;` |
|        5 | 2020 | `}` |
|        - | 2021 | `/*` |
|        - | 2022 | ` * [CAPIREF: ph7_value_double()]` |
|        - | 2023 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2024 | ` */` |
|     3551 | 2025 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|        5 | 2026 | `{` |
|        - | 2027 | `	/* Invalidate any prior representation */` |
|     3556 | 2028 | `	PH7_MemObjRelease(pVal);` |
|     3556 | 2029 | `	pVal->rVal = (ph7_real)Value;` |
|     3556 | 2030 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|        - | 2031 | `	/* Try to get an integer representation also */` |
|     3556 | 2032 | `	PH7_MemObjTryInteger(pVal);` |
|     3556 | 2033 | `	return PH7_OK;` |
|        5 | 2034 | `}` |
|        - | 2035 | `/*` |
|        - | 2036 | ` * [CAPIREF: ph7_value_string()]` |
|        - | 2037 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2038 | ` */` |
|  8499403 | 2039 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|        5 | 2040 | `{` |
|  8499408 | 2041 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2042 | `		/* Invalidate any prior representation */` |
|  2954004 | 2043 | `		PH7_MemObjRelease(pVal);` |
|  2954004 | 2044 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  1476742 | 2045 | `	}` |
|  8499408 | 2046 | `	if( zString ){` |
|  8479169 | 2047 | `		if( nLen < 0 ){` |
|        - | 2048 | `			/* Compute length automatically */` |
|   128855 | 2049 | `			nLen = (int)SyStrlen(zString);` |
|    64368 | 2050 | `		}` |
|        - | 2051 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|        - | 2052 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
|  8479169 | 2053 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|        - | 2054 | `	}` |
|    20244 | 2055 | `	return PH7_OK;` |
|  4241622 | 2056 | `}` |
|        - | 2057 | `/*` |
|        - | 2058 | ` * [CAPIREF: ph7_value_string_format()]` |
|        - | 2059 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2060 | ` */` |
|    11048 | 2061 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|        5 | 2062 | `{` |
|        - | 2063 | `	va_list ap;` |
|        - | 2064 | `	int rc;` |
|    11053 | 2065 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2066 | `		/* Invalidate any prior representation */` |
|    11021 | 2067 | `		PH7_MemObjRelease(pVal);` |
|    11021 | 2068 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|     5508 | 2069 | `	}` |
|    11053 | 2070 | `	va_start(ap,zFormat);` |
|    11053 | 2071 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|    11053 | 2072 | `	va_end(ap);` |
|        - | 2073 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|    11053 | 2074 | `	return rc;` |
|        5 | 2075 | `}` |
|        - | 2076 | `/*` |
|        - | 2077 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|        - | 2078 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2079 | ` */` |
|  3708598 | 2080 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|        5 | 2081 | `{` |
|        - | 2082 | `	/* Reset the string cursor */` |
|  3708603 | 2083 | `	SyBlobReset(&pVal->sBlob);` |
|  3708603 | 2084 | `	return PH7_OK;` |
|        5 | 2085 | `}` |
|        - | 2086 | `/*` |
|        - | 2087 | ` * [CAPIREF: ph7_value_resource()]` |
|        - | 2088 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2089 | ` */` |
|    12078 | 2090 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|        5 | 2091 | `{` |
|        - | 2092 | `	/* Invalidate any prior representation */` |
|    12083 | 2093 | `	PH7_MemObjRelease(pVal);` |
|        - | 2094 | `	/* Reflect the new type */` |
|    12083 | 2095 | `	pVal->x.pOther = pUserData;` |
|    12083 | 2096 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    12083 | 2097 | `	return PH7_OK;` |
|        5 | 2098 | `}` |
|        - | 2099 | `/*` |
|        - | 2100 | ` * [CAPIREF: ph7_value_release()]` |
|        - | 2101 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2102 | ` */` |
|      ! 0 | 2103 | `int ph7_value_release(ph7_value *pVal)` |
|      ! 0 | 2104 | `{` |
|      ! 0 | 2105 | `	PH7_MemObjRelease(pVal);` |
|      ! 0 | 2106 | `	return PH7_OK;` |
|      ! 0 | 2107 | `}` |
|        - | 2108 | `/*` |
|        - | 2109 | ` * [CAPIREF: ph7_value_is_int()]` |
|        - | 2110 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2111 | ` */` |
|  1274538 | 2112 | `int ph7_value_is_int(ph7_value *pVal)` |
|        5 | 2113 | `{` |
|        - | 2114 | `	/* TRUE whenever an integer representation is available, including an` |
|        - | 2115 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|        - | 2116 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|        - | 2117 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|        - | 2118 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|  1274543 | 2119 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|        5 | 2120 | `}` |
|        - | 2121 | `/*` |
|        - | 2122 | ` * [CAPIREF: ph7_value_is_float()]` |
|        - | 2123 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2124 | ` */` |
|  1310224 | 2125 | `int ph7_value_is_float(ph7_value *pVal)` |
|        5 | 2126 | `{` |
|  1310229 | 2127 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|        5 | 2128 | `}` |
|        - | 2129 | `/*` |
|        - | 2130 | ` * [CAPIREF: ph7_value_is_bool()]` |
|        - | 2131 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2132 | ` */` |
|    74284 | 2133 | `int ph7_value_is_bool(ph7_value *pVal)` |
|        5 | 2134 | `{` |
|    74289 | 2135 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|        5 | 2136 | `}` |
|        - | 2137 | `/*` |
|        - | 2138 | ` * [CAPIREF: ph7_value_is_string()]` |
|        - | 2139 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2140 | ` */` |
|  1409395 | 2141 | `int ph7_value_is_string(ph7_value *pVal)` |
|        5 | 2142 | `{` |
|  1409400 | 2143 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|        5 | 2144 | `}` |
|        - | 2145 | `/*` |
|        - | 2146 | ` * [CAPIREF: ph7_value_is_null()]` |
|        - | 2147 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2148 | ` */` |
|  3310679 | 2149 | `int ph7_value_is_null(ph7_value *pVal)` |
|        5 | 2150 | `{` |
|  3310684 | 2151 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|        5 | 2152 | `}` |
|        - | 2153 | `/*` |
|        - | 2154 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|        - | 2155 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2156 | ` */` |
|    21032 | 2157 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|        5 | 2158 | `{` |
|        - | 2159 | `	int rc;` |
|    21037 | 2160 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|    21037 | 2161 | `	return rc;` |
|        5 | 2162 | `}` |
|        - | 2163 | `/*` |
|        - | 2164 | ` * [CAPIREF: ph7_value_is_callable()]` |
|        - | 2165 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2166 | ` */` |
|    81573 | 2167 | `int ph7_value_is_callable(ph7_value *pVal)` |
|        5 | 2168 | `{` |
|        - | 2169 | `	int rc;` |
|    81578 | 2170 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|    81578 | 2171 | `	return rc;` |
|        5 | 2172 | `}` |
|        - | 2173 | `/*` |
|        - | 2174 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|        - | 2175 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2176 | ` */` |
|       30 | 2177 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|        1 | 2178 | `{` |
|       31 | 2179 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|        1 | 2180 | `}` |
|        - | 2181 | `/*` |
|        - | 2182 | ` * [CAPIREF: ph7_value_is_array()]` |
|        - | 2183 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2184 | ` */` |
|   982473 | 2185 | `int ph7_value_is_array(ph7_value *pVal)` |
|        5 | 2186 | `{` |
|   982478 | 2187 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|        5 | 2188 | `}` |
|        - | 2189 | `/*` |
|        - | 2190 | ` * [CAPIREF: ph7_value_is_object()]` |
|        - | 2191 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2192 | ` */` |
|   337192 | 2193 | `int ph7_value_is_object(ph7_value *pVal)` |
|        5 | 2194 | `{` |
|   337197 | 2195 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|        5 | 2196 | `}` |
|        - | 2197 | `/*` |
|        - | 2198 | ` * [CAPIREF: ph7_value_is_resource()]` |
|        - | 2199 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2200 | ` */` |
|   254460 | 2201 | `int ph7_value_is_resource(ph7_value *pVal)` |
|        5 | 2202 | `{` |
|   254465 | 2203 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|        5 | 2204 | `}` |
|        - | 2205 | `/*` |
|        - | 2206 | ` * [CAPIREF: ph7_value_is_empty()]` |
|        - | 2207 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2208 | ` */` |
|    55843 | 2209 | `int ph7_value_is_empty(ph7_value *pVal)` |
|        5 | 2210 | `{` |
|        - | 2211 | `	int rc;` |
|    55848 | 2212 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|    55848 | 2213 | `	return rc;` |
|        5 | 2214 | `}` |
|        - | 2215 | `/*` |
|        - | 2216 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|        - | 2217 | ` * Check if a value holds a Fiber instance.` |
|        - | 2218 | ` */` |
|      ! 0 | 2219 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|      ! 0 | 2220 | `{` |
|      ! 0 | 2221 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|      ! 0 | 2222 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|      ! 0 | 2223 | `}` |
|        - | 2224 | `/*` |
|        - | 2225 | ` * [CAPIREF: ph7_fiber_start()]` |
|        - | 2226 | ` * Start a Fiber, passing arguments to the callable.` |
|        - | 2227 | ` */` |
|      ! 0 | 2228 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 2229 | `{` |
|      ! 0 | 2230 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|      ! 0 | 2231 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|      ! 0 | 2232 | `}` |
|        - | 2233 | `/*` |
|        - | 2234 | ` * [CAPIREF: ph7_fiber_resume()]` |
|        - | 2235 | ` * Resume a suspended Fiber, optionally sending a value.` |
|        - | 2236 | ` */` |
|      ! 0 | 2237 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 2238 | `{` |
|      ! 0 | 2239 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|      ! 0 | 2240 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|      ! 0 | 2241 | `}` |
|        - | 2242 | `/*` |
|        - | 2243 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|        - | 2244 | ` * Check if a Fiber is currently suspended.` |
|        - | 2245 | ` */` |
|      ! 0 | 2246 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|      ! 0 | 2247 | `{` |
|      ! 0 | 2248 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2249 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|      ! 0 | 2250 | `}` |
|        - | 2251 | `/*` |
|        - | 2252 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|        - | 2253 | ` * Check if a Fiber has completed execution.` |
|        - | 2254 | ` */` |
|      ! 0 | 2255 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|      ! 0 | 2256 | `{` |
|      ! 0 | 2257 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2258 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|      ! 0 | 2259 | `}` |
|        - | 2260 | `/*` |
|        - | 2261 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|        - | 2262 | ` * Get the return value of a terminated Fiber.` |
|        - | 2263 | ` * Returns NULL if the Fiber has not terminated.` |
|        - | 2264 | ` */` |
|      ! 0 | 2265 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|      ! 0 | 2266 | `{` |
|      ! 0 | 2267 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2268 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|      ! 0 | 2269 | `}` |
|        - | 2270 |  |
