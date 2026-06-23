# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 772/1086 lines (71.09%)

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
|    6578 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       5 |   79 |  |
|    6583 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    6583 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    6583 |   83 | `	switch(nOp){` |
|    3289 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    6583 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    6583 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    6583 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    6583 |   93 | `		pConf->xErr     = xConsumer;` |
|    6583 |   94 | `		pConf->pErrData = pUserData;` |
|    6583 |   95 | `		break;` |
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
|    6583 |  144 | `	return rc;` |
|       5 |  145 |  |
|       - |  146 | `/*` |
|       - |  147 | ` * Configure the PH7 library.` |
|       - |  148 | ` * return PH7_OK on success.Any other return value` |
|       - |  149 | ` * indicates failure.` |
|       - |  150 | ` * Refer to [ph7_lib_config()].` |
|       - |  151 | ` */` |
|    9900 |  152 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       5 |  153 |  |
|    9905 |  154 | `	int rc = PH7_OK;` |
|    9905 |  155 | `	switch(nOp){` |
|    1650 |  156 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  157 | `			/* Install a virtual file system */` |
|    3305 |  158 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    3305 |  159 | `			sMPGlobal.pVfs = pVfs;` |
|    3305 |  160 | `			break;` |
|       - |  161 | `								}` |
|    1650 |  162 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  163 | `			/* Use an alternative low-level memory allocation routines */` |
|    3305 |  164 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  165 | `			/* Save the memory failure callback (if available) */` |
|    3305 |  166 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    3305 |  167 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    3305 |  168 | `			if( pMethods == 0 ){` |
|       - |  169 | `				/* Use the built-in memory allocation subsystem */` |
|    3305 |  170 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    1655 |  171 | `			}else{` |
|     ! 0 |  172 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  173 | `			}` |
|    3305 |  174 | `			break;` |
|       - |  175 | `										  }` |
|     ! 0 |  176 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  177 | `			/* Memory failure callback */` |
|     ! 0 |  178 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  179 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  180 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  181 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  182 | `			break;` |
|       - |  183 | `												 }` |
|    1650 |  184 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  185 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  186 | `			/* Use an alternative low-level mutex subsystem */` |
|    3305 |  187 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  188 | `#if defined (UNTRUST)` |
|       - |  189 | `			if( pMethods == 0 ){` |
|       - |  190 | `				rc = PH7_CORRUPT;` |
|       - |  191 | `			}` |
|       - |  192 | `#endif` |
|       - |  193 | `			/* Sanity check */` |
|    3305 |  194 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  195 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  196 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  197 | `				break;` |
|       - |  198 | `			}` |
|    3305 |  199 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  200 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  201 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  202 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  203 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  204 | `				}` |
|     ! 0 |  205 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  206 | `			}` |
|       - |  207 | `			/* Initialize and install the new mutex subsystem */` |
|    3305 |  208 | `			if( pMethods->xGlobalInit ){` |
|       5 |  209 | `				rc = pMethods->xGlobalInit();` |
|       5 |  210 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  211 | `					break;` |
|       - |  212 | `				}` |
|     ! 0 |  213 | `			}` |
|       - |  214 | `			/* Create the global mutex */` |
|    3305 |  215 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    3305 |  216 | `			if( sMPGlobal.pMutex == 0 ){` |
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
|    3305 |  227 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    3305 |  228 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  229 | `				/* Set a default threading level */` |
|    3305 |  230 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    1650 |  231 | `			}` |
|       - |  232 | `#endif` |
|    3305 |  233 | `			break;` |
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
|    9905 |  254 | `	return rc;` |
|       5 |  255 |  |
|       - |  256 | `/*` |
|       - |  257 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  258 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  259 | ` */` |
|    9900 |  260 | `int ph7_lib_config(int nConfigOp,...)` |
|       5 |  261 |  |
|       - |  262 | `	va_list ap;` |
|       - |  263 | `	int rc;` |
|       - |  264 |  |
|    9905 |  265 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  266 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  267 | `		return PH7_LOOKED;` |
|       - |  268 | `	}` |
|    9905 |  269 | `	va_start(ap,nConfigOp);` |
|    9905 |  270 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    9905 |  271 | `	va_end(ap);` |
|    9905 |  272 | `	return rc;` |
|    4955 |  273 |  |
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
|    3300 |  284 | `static sxi32 PH7CoreInitialize(void)` |
|       5 |  285 |  |
|       - |  286 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  287 | `#if defined(PH7_ENABLE_THREADS)` |
|    3305 |  288 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    3305 |  289 | `	SyMutex *pMaster = 0;` |
|       - |  290 | `#endif` |
|       - |  291 | `	int rc;` |
|       - |  292 | `	/*` |
|       - |  293 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  294 | `	 * is a no-op.` |
|       - |  295 | `	 */` |
|    3305 |  296 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  297 | `		return PH7_OK; /* Already initialized */` |
|       - |  298 | `	}` |
|       - |  299 | `	/* Point to the built-in vfs */` |
|    3305 |  300 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  301 | `	/* Install it */` |
|    3305 |  302 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  303 | `#if defined(PH7_ENABLE_THREADS)` |
|    3305 |  304 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    3305 |  305 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    3305 |  306 | `		if( pMutexMethods == 0 ){` |
|       - |  307 | `			/* Use the built-in mutex subsystem */` |
|    3305 |  308 | `			pMutexMethods = SyMutexExportMethods();` |
|    3305 |  309 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  310 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  311 | `			}` |
|       - |  312 | `			/* Install the mutex subsystem */` |
|    3305 |  313 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    3305 |  314 | `			if( rc != PH7_OK ){` |
|     ! 0 |  315 | `				return rc;` |
|       - |  316 | `			}` |
|    1650 |  317 | `		}` |
|       - |  318 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    3305 |  319 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    3305 |  320 | `		if( pMaster == 0 ){` |
|     ! 0 |  321 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  322 | `		}` |
|    1650 |  323 | `	}` |
|       - |  324 | `	/* Lock the master mutex */` |
|    3305 |  325 | `	rc = PH7_OK;` |
|    3305 |  326 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    4955 |  327 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  328 | `#endif` |
|    3305 |  329 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  330 | `			/* Install a memory subsystem */` |
|    3305 |  331 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    3305 |  332 | `			if( rc != PH7_OK ){` |
|       - |  333 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  334 | `				goto End;` |
|       - |  335 | `			}` |
|    1650 |  336 | `		}` |
|       - |  337 | `#if defined(PH7_ENABLE_THREADS)` |
|    3305 |  338 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  339 | `			/* Protect the memory allocation subsystem */` |
|    3305 |  340 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    3305 |  341 | `			if( rc != PH7_OK ){` |
|     ! 0 |  342 | `				goto End;` |
|       - |  343 | `			}` |
|    1650 |  344 | `		}` |
|       - |  345 | `#endif` |
|       - |  346 | `		/* Our library is initialized,set the magic number */` |
|    3305 |  347 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    3305 |  348 | `		rc = PH7_OK;` |
|       - |  349 | `#if defined(PH7_ENABLE_THREADS)` |
|    1650 |  350 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  351 | `#endif` |
|     ! 0 |  352 | `End:` |
|       - |  353 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  354 | `	/* Unlock the master mutex */` |
|    3305 |  355 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  356 | `#endif` |
|    3305 |  357 | `	return rc;` |
|    1655 |  358 |  |
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
|    3300 |  372 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       5 |  373 |  |
|       - |  374 | `	ph7_vm *pVm,*pNext;` |
|       - |  375 | `	/* Release all active VM */` |
|    3305 |  376 | `	pVm = pEngine->pVms;` |
|    1650 |  377 | `	for(;;){` |
|    3305 |  378 | `		if( pEngine->iVm <= 0 ){` |
|    3305 |  379 | `			break;` |
|       - |  380 | `		}` |
|     ! 0 |  381 | `		pNext = pVm->pNext;` |
|     ! 0 |  382 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  383 | `		pVm = pNext;` |
|     ! 0 |  384 | `		pEngine->iVm--;` |
|     ! 0 |  385 | `	}` |
|       - |  386 | `	/* Set a dummy magic number */` |
|    3305 |  387 | `	pEngine->nMagic = 0x7635;` |
|       - |  388 | `	/* Release the private memory subsystem */` |
|    3305 |  389 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    3305 |  390 | `	return PH7_OK;` |
|       5 |  391 |  |
|       - |  392 | `/*` |
|       - |  393 | ` * Release all resources consumed by the library.` |
|       - |  394 | ` * If PH7 is already shut down when this routine` |
|       - |  395 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  396 | ` * Note: This call is not thread safe.` |
|       - |  397 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  398 | ` */` |
|     340 |  399 | `static void PH7CoreShutdown(void)` |
|       4 |  400 |  |
|       - |  401 | `	ph7 *pEngine,*pNext;` |
|       - |  402 | `	/* Release all active engines first */` |
|     344 |  403 | `	pEngine = sMPGlobal.pEngines;` |
|     340 |  404 | `	for(;;){` |
|     684 |  405 | `		if( sMPGlobal.nEngine < 1 ){` |
|     344 |  406 | `			break;` |
|       - |  407 | `		}` |
|     344 |  408 | `		pNext = pEngine->pNext;` |
|     344 |  409 | `		EngineRelease(pEngine);` |
|     344 |  410 | `		pEngine = pNext;` |
|     344 |  411 | `		sMPGlobal.nEngine--;` |
|       4 |  412 | `	}` |
|       - |  413 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  414 | `	/* Release the mutex subsystem */` |
|     344 |  415 | `	if( sMPGlobal.pMutexMethods ){` |
|     344 |  416 | `		if( sMPGlobal.pMutex ){` |
|     344 |  417 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     344 |  418 | `			sMPGlobal.pMutex = 0;` |
|     170 |  419 | `		}` |
|     344 |  420 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       4 |  421 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  422 | `		}` |
|     344 |  423 | `		sMPGlobal.pMutexMethods = 0;` |
|     170 |  424 | `	}` |
|     344 |  425 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  426 | `#endif` |
|     344 |  427 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  428 | `		/* Release the memory backend */` |
|     344 |  429 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     170 |  430 | `	}` |
|     344 |  431 | `	sMPGlobal.nMagic = 0x1928;` |
|     344 |  432 |  |
|       - |  433 | `/*` |
|       - |  434 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  435 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  436 | ` */` |
|     340 |  437 | `int ph7_lib_shutdown(void)` |
|       4 |  438 |  |
|     344 |  439 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  440 | `		/* Already shut */` |
|     ! 0 |  441 | `		return PH7_OK;` |
|       - |  442 | `	}` |
|     344 |  443 | `	PH7CoreShutdown();` |
|     344 |  444 | `	return PH7_OK;` |
|     174 |  445 |  |
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
|    6578 |  503 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       5 |  504 |  |
|       - |  505 | `	va_list ap;` |
|       - |  506 | `	int rc;` |
|    6583 |  507 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  508 | `		return PH7_CORRUPT;` |
|       - |  509 | `	}` |
|       - |  510 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  511 | `	 /* Acquire engine mutex */` |
|    6583 |  512 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    6583 |  513 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    6578 |  514 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  515 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  516 | `	 }` |
|       - |  517 | `#endif` |
|    6583 |  518 | `	 va_start(ap,nConfigOp);` |
|    6583 |  519 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    6583 |  520 | `	 va_end(ap);` |
|       - |  521 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  522 | `	 /* Leave engine mutex */` |
|    6583 |  523 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  524 | `#endif` |
|    6583 |  525 | `	return rc;` |
|    3294 |  526 |  |
|       - |  527 | `/*` |
|       - |  528 | ` * [CAPIREF: ph7_init()]` |
|       - |  529 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  530 | ` */` |
|    3300 |  531 | `int ph7_init(ph7 **ppEngine)` |
|       5 |  532 |  |
|       - |  533 | `	ph7 *pEngine;` |
|       - |  534 | `	int rc;` |
|       - |  535 | `#if defined(UNTRUST)` |
|       - |  536 | `	if( ppEngine == 0 ){` |
|       - |  537 | `		return PH7_CORRUPT;` |
|       - |  538 | `	}` |
|       - |  539 | `#endif` |
|    3305 |  540 | `	*ppEngine = 0;` |
|       - |  541 | `	/* One-time automatic library initialization */` |
|    3305 |  542 | `	rc = PH7CoreInitialize();` |
|    3305 |  543 | `	if( rc != PH7_OK ){` |
|     ! 0 |  544 | `		return rc;` |
|       - |  545 | `	}` |
|       - |  546 | `	/* Allocate a new engine */` |
|    3305 |  547 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    3305 |  548 | `	if( pEngine == 0 ){` |
|     ! 0 |  549 | `		return PH7_NOMEM;` |
|       - |  550 | `	}` |
|       - |  551 | `	/* Zero the structure */` |
|    3305 |  552 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  553 | `	/* Initialize engine fields */` |
|    3305 |  554 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    3305 |  555 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    3305 |  556 | `	if( rc != PH7_OK ){` |
|     ! 0 |  557 | `		goto Release;` |
|       - |  558 | `	}` |
|       - |  559 | `#if defined(PH7_ENABLE_THREADS)` |
|    3305 |  560 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  561 | `#endif` |
|       - |  562 | `	/* Default configuration */` |
|    3305 |  563 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  564 | `	/* Install a default compile-time error consumer routine */` |
|    3305 |  565 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  566 | `	/* Built-in vfs */` |
|    3305 |  567 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  568 | `#if defined(PH7_ENABLE_THREADS)` |
|    3305 |  569 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  570 | `		 /* Associate a recursive mutex with this instance */` |
|    3305 |  571 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3305 |  572 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  573 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  574 | `			 goto Release;` |
|       - |  575 | `		 }` |
|    1650 |  576 | `	 }` |
|       - |  577 | `#endif` |
|       - |  578 | `	/* Link to the list of active engines */` |
|       - |  579 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  580 | `	/* Enter the global mutex */` |
|    3305 |  581 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  582 | `#endif` |
|    3305 |  583 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    3305 |  584 | `	sMPGlobal.nEngine++;` |
|       - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  586 | `	/* Leave the global mutex */` |
|    3305 |  587 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  588 | `#endif` |
|       - |  589 | `	/* Write a pointer to the new instance */` |
|    3305 |  590 | `	*ppEngine = pEngine;` |
|    3305 |  591 | `	return PH7_OK;` |
|     ! 0 |  592 | `Release:` |
|     ! 0 |  593 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  594 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  595 | `	return rc;` |
|    1655 |  596 |  |
|       - |  597 | `/*` |
|       - |  598 | ` * [CAPIREF: ph7_release()]` |
|       - |  599 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  600 | ` */` |
|    2960 |  601 | `int ph7_release(ph7 *pEngine)` |
|       5 |  602 |  |
|       - |  603 | `	int rc;` |
|    2965 |  604 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  605 | `		return PH7_CORRUPT;` |
|       - |  606 | `	}` |
|       - |  607 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  608 | `	 /* Acquire engine mutex */` |
|    2965 |  609 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2965 |  610 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2960 |  611 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  612 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  613 | `	 }` |
|       - |  614 | `#endif` |
|       - |  615 | `	/* Release the engine */` |
|    2965 |  616 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  617 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  618 | `	 /* Leave engine mutex */` |
|    2965 |  619 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  620 | `	 /* Release engine mutex */` |
|    2965 |  621 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  622 | `#endif` |
|       - |  623 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  624 | `	/* Enter the global mutex */` |
|    2965 |  625 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  626 | `#endif` |
|       - |  627 | `	/* Unlink from the list of active engines */` |
|    2965 |  628 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    2965 |  629 | `	sMPGlobal.nEngine--;` |
|       - |  630 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  631 | `	/* Leave the global mutex */` |
|    2965 |  632 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  633 | `#endif` |
|       - |  634 | `	/* Release the memory chunk allocated to this engine */` |
|    2965 |  635 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    2965 |  636 | `	return rc;` |
|    1485 |  637 |  |
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
|    3296 |  648 | `static sxi32 ProcessScript(` |
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
|    3301 |  659 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    3301 |  660 | `	if( pVm == 0 ){` |
|       - |  661 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  662 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  663 | `		if( ppVm ){` |
|     ! 0 |  664 | `			*ppVm = 0;` |
|     ! 0 |  665 | `		}` |
|     ! 0 |  666 | `		return PH7_NOMEM;` |
|       - |  667 | `	}` |
|    3301 |  668 | `	if( iFlags < 0 ){` |
|       - |  669 | `		/* Default compile-time flags */` |
|     ! 0 |  670 | `		iFlags = 0;` |
|     ! 0 |  671 | `	}` |
|       - |  672 | `	/* Initialize the Virtual Machine */` |
|    3301 |  673 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    3301 |  674 | `	if( rc != PH7_OK ){` |
|     ! 0 |  675 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  676 | `		if( ppVm ){` |
|     ! 0 |  677 | `			*ppVm = 0;` |
|     ! 0 |  678 | `		}` |
|     ! 0 |  679 | `		return PH7_VM_ERR;` |
|       - |  680 | `	}` |
|    3301 |  681 | `	if( zFilePath ){` |
|       - |  682 | `		/* Push processed file path */` |
|    3293 |  683 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    1644 |  684 | `	}` |
|       - |  685 | `	/* Reset the error message consumer */` |
|    3301 |  686 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  687 | `	/* Compile the script */` |
|    3301 |  688 | `	PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    3301 |  689 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     344 |  690 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  691 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     344 |  692 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     344 |  693 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     344 |  694 | `		if( ppVm ){` |
|     344 |  695 | `			*ppVm = 0;` |
|     170 |  696 | `		}` |
|     344 |  697 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  698 | `	}` |
|       - |  699 | `	/* Prepare the virtual machine for bytecode execution */` |
|    2961 |  700 | `	rc = PH7_VmMakeReady(pVm);` |
|    2961 |  701 | `	if( rc != PH7_OK ){` |
|       3 |  702 | `		goto Release;` |
|       - |  703 | `	}` |
|       - |  704 | `	/* Install local import path which is the current directory */` |
|    2959 |  705 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  706 | `#if defined(PH7_ENABLE_THREADS)` |
|    2959 |  707 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  708 | `		 /* Associate a recursive mutex with this instance */` |
|    2959 |  709 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    2959 |  710 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  711 | `			 goto Release;` |
|       - |  712 | `		 }` |
|    1477 |  713 | `	 }` |
|       - |  714 | `#endif` |
|       - |  715 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    2959 |  716 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    2959 |  717 | `	pEngine->iVm++;` |
|       - |  718 | `	/* Point to the freshly created VM */` |
|    2959 |  719 | `	*ppVm = pVm;` |
|       - |  720 | `	/* Ready to execute PH7 bytecode */` |
|    2959 |  721 | `	return PH7_OK;` |
|       1 |  722 | `Release:` |
|       - |  723 | `	{` |
|       - |  724 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|       - |  725 | `		 * typed class constant whose value violates its declared type) is a compile` |
|       - |  726 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|       - |  727 | `		 * Captured before the releases free the VM. */` |
|       3 |  728 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|       3 |  729 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|       3 |  730 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       3 |  731 | `		*ppVm = 0;` |
|       3 |  732 | `		return rcRet;` |
|       - |  733 | `	}` |
|    1653 |  734 |  |
|       - |  735 | `/*` |
|       - |  736 | ` * [CAPIREF: ph7_compile()]` |
|       - |  737 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  738 | ` */` |
|     ! 0 |  739 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|     ! 0 |  740 |  |
|       - |  741 | `	SyString sScript;` |
|       - |  742 | `	int rc;` |
|     ! 0 |  743 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  744 | `		return PH7_CORRUPT;` |
|       - |  745 | `	}` |
|     ! 0 |  746 | `	if( nLen < 0 ){` |
|       - |  747 | `		/* Compute input length automatically */` |
|     ! 0 |  748 | `		nLen = (int)SyStrlen(zSource);` |
|     ! 0 |  749 | `	}` |
|     ! 0 |  750 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  751 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  752 | `	 /* Acquire engine mutex */` |
|     ! 0 |  753 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  754 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  755 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  756 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  757 | `	 }` |
|       - |  758 | `#endif` |
|       - |  759 | `	/* Compile the script */` |
|     ! 0 |  760 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|       - |  761 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  762 | `	 /* Leave engine mutex */` |
|     ! 0 |  763 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  764 | `#endif` |
|       - |  765 | `	/* Compilation result */` |
|     ! 0 |  766 | `	return rc;` |
|     ! 0 |  767 |  |
|       - |  768 | `/*` |
|       - |  769 | ` * [CAPIREF: ph7_compile_v2()]` |
|       - |  770 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  771 | ` */` |
|       8 |  772 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       2 |  773 |  |
|       - |  774 | `	SyString sScript;` |
|       - |  775 | `	int rc;` |
|      10 |  776 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  777 | `		return PH7_CORRUPT;` |
|       - |  778 | `	}` |
|      10 |  779 | `	if( nLen < 0 ){` |
|       - |  780 | `		/* Compute input length automatically */` |
|      10 |  781 | `		nLen = (int)SyStrlen(zSource);` |
|       4 |  782 | `	}` |
|      10 |  783 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  784 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  785 | `	 /* Acquire engine mutex */` |
|      10 |  786 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      10 |  787 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       8 |  788 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  789 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  790 | `	 }` |
|       - |  791 | `#endif` |
|       - |  792 | `	/* Compile the script */` |
|      10 |  793 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  794 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  795 | `	 /* Leave engine mutex */` |
|      10 |  796 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  797 | `#endif` |
|       - |  798 | `	/* Compilation result */` |
|      10 |  799 | `	return rc;` |
|       6 |  800 |  |
|       - |  801 | `/*` |
|       - |  802 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  803 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  804 | ` */` |
|    3288 |  805 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       5 |  806 |  |
|       - |  807 | `	const ph7_vfs *pVfs;` |
|       - |  808 | `	int rc;` |
|    3293 |  809 | `	if( ppOutVm ){` |
|    3293 |  810 | `		*ppOutVm = 0;` |
|    1644 |  811 | `	}` |
|    3293 |  812 | `	rc = PH7_OK; /* cc warning */` |
|    3293 |  813 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  814 | `		return PH7_CORRUPT;` |
|       - |  815 | `	}` |
|       - |  816 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  817 | `	 /* Acquire engine mutex */` |
|    3293 |  818 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3293 |  819 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3288 |  820 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  821 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  822 | `	 }` |
|       - |  823 | `#endif` |
|       - |  824 | `	 /*` |
|       - |  825 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  826 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  827 | `	  */` |
|    3293 |  828 | `	 pVfs = pEngine->pVfs;` |
|    3293 |  829 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  830 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  831 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  832 | `	 }else{` |
|    3293 |  833 | `		 void *pMapView = 0; /* cc warning */` |
|    3293 |  834 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  835 | `		 SyString sScript;` |
|       - |  836 | `		 /* Try to get a memory view of the whole file */` |
|    3293 |  837 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    3293 |  838 | `		 if( rc != PH7_OK ){` |
|       - |  839 | `			 /* Assume an IO error */` |
|     ! 0 |  840 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  841 | `		 }else{` |
|       - |  842 | `			 /* Compile the file */` |
|    3293 |  843 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    3293 |  844 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  845 | `			 /* Release the memory view of the whole file */` |
|    3293 |  846 | `			 if( pVfs->xUnmap ){` |
|    3293 |  847 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    1644 |  848 | `			 }` |
|       - |  849 | `		 }` |
|       - |  850 | `	 }` |
|       - |  851 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  852 | `	 /* Leave engine mutex */` |
|    3293 |  853 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  854 | `#endif` |
|       - |  855 | `	/* Compilation result */` |
|    3293 |  856 | `	return rc;` |
|    1649 |  857 |  |
|       - |  858 | `/*` |
|       - |  859 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|       - |  860 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  861 | ` */` |
|       2 |  862 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|       1 |  863 |  |
|       - |  864 | `	int rc;` |
|       - |  865 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       3 |  866 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  867 | `		return PH7_CORRUPT;` |
|       - |  868 | `	}` |
|       - |  869 | `#ifdef UNTRUST` |
|       - |  870 | `	if( xConsumer == 0 ){` |
|       - |  871 | `		return PH7_CORRUPT;` |
|       - |  872 | `	}` |
|       - |  873 | `#endif` |
|       - |  874 | `	/* Dump VM instructions */` |
|       3 |  875 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|       3 |  876 | `	return rc;` |
|       2 |  877 |  |
|       - |  878 | `/*` |
|       - |  879 | ` * [CAPIREF: ph7_vm_config()]` |
|       - |  880 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  881 | ` */` |
|   68398 |  882 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       5 |  883 |  |
|       - |  884 | `	va_list ap;` |
|       - |  885 | `	int rc;` |
|       - |  886 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   68403 |  887 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  888 | `		return PH7_CORRUPT;` |
|       - |  889 | `	}` |
|       - |  890 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  891 | `	 /* Acquire VM mutex */` |
|   68403 |  892 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   68403 |  893 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   68398 |  894 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  895 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  896 | `	 }` |
|       - |  897 | `#endif` |
|       - |  898 | `	/* Confiugure the virtual machine */` |
|   68403 |  899 | `	va_start(ap,iConfigOp);` |
|   68403 |  900 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   68403 |  901 | `	va_end(ap);` |
|       - |  902 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  903 | `	 /* Leave VM mutex */` |
|   68403 |  904 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  905 | `#endif` |
|   68403 |  906 | `	return rc;` |
|   34204 |  907 |  |
|       - |  908 | `/*` |
|       - |  909 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  910 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  911 | ` */` |
|    2958 |  912 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       5 |  913 |  |
|       - |  914 | `	int rc;` |
|       - |  915 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    2963 |  916 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  917 | `		return PH7_CORRUPT;` |
|       - |  918 | `	}` |
|       - |  919 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  920 | `	 /* Acquire VM mutex */` |
|    2963 |  921 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2963 |  922 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2958 |  923 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  924 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  925 | `	 }` |
|       - |  926 | `#endif` |
|       - |  927 | `	/* Execute PH7 byte-code */` |
|    2963 |  928 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    2963 |  929 | `	if( pExitStatus ){` |
|       - |  930 | `		/* Exit status */` |
|    2939 |  931 | `		*pExitStatus = pVm->iExitStatus;` |
|    1467 |  932 | `	}` |
|       - |  933 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  934 | `	 /* Leave VM mutex */` |
|    2963 |  935 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  936 | `#endif` |
|       - |  937 | `	/* Execution result */` |
|    2963 |  938 | `	return rc;` |
|    1484 |  939 |  |
|       - |  940 | `/*` |
|       - |  941 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  942 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  943 | ` */` |
|       6 |  944 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  945 |  |
|       - |  946 | `	int rc;` |
|       - |  947 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       6 |  948 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  949 | `		return PH7_CORRUPT;` |
|       - |  950 | `	}` |
|       - |  951 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  952 | `	 /* Acquire VM mutex */` |
|       6 |  953 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       6 |  954 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       6 |  955 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  956 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  957 | `	 }` |
|       - |  958 | `#endif` |
|       6 |  959 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  960 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  961 | `	 /* Leave VM mutex */` |
|       6 |  962 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  963 | `#endif` |
|       6 |  964 | `	return rc;` |
|       3 |  965 |  |
|       - |  966 | `/*` |
|       - |  967 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  968 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  969 | ` */` |
|    2954 |  970 | `int ph7_vm_release(ph7_vm *pVm)` |
|       5 |  971 |  |
|       - |  972 | `	ph7 *pEngine;` |
|       - |  973 | `	int rc;` |
|       - |  974 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    2959 |  975 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  976 | `		return PH7_CORRUPT;` |
|       - |  977 | `	}` |
|       - |  978 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  979 | `	 /* Acquire VM mutex */` |
|    2959 |  980 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2959 |  981 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2954 |  982 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  983 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  984 | `	 }` |
|       - |  985 | `#endif` |
|    2959 |  986 | `	pEngine = pVm->pEngine;` |
|    2959 |  987 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - |  988 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  989 | `	 /* Leave VM mutex */` |
|    2959 |  990 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  991 | `#endif` |
|    2959 |  992 | `	if( rc == PH7_OK ){` |
|       - |  993 | `		/* Unlink from the list of active VM */` |
|       - |  994 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  995 | `			/* Acquire engine mutex */` |
|    2959 |  996 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2959 |  997 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2954 |  998 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  999 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1000 | `			}` |
|       - | 1001 | `#endif` |
|    2959 | 1002 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    2959 | 1003 | `		pEngine->iVm--;` |
|       - | 1004 | `		/* Release the memory chunk allocated to this VM */` |
|    2959 | 1005 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - | 1006 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1007 | `			/* Leave engine mutex */` |
|    2959 | 1008 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1009 | `#endif` |
|    1477 | 1010 | `	}` |
|    2959 | 1011 | `	return rc;` |
|    1482 | 1012 |  |
|       - | 1013 | `/*` |
|       - | 1014 | ` * [CAPIREF: ph7_create_function()]` |
|       - | 1015 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1016 | ` */` |
| 1478548 | 1017 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       5 | 1018 |  |
|       - | 1019 | `	SyString sName;` |
|       - | 1020 | `	int rc;` |
|       - | 1021 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 1478553 | 1022 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1023 | `		return PH7_CORRUPT;` |
|       - | 1024 | `	}` |
| 1478553 | 1025 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1026 | `	/* Remove leading and trailing white spaces */` |
| 1478553 | 1027 | `	SyStringFullTrim(&sName);` |
|       - | 1028 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 1478553 | 1029 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1030 | `		return PH7_CORRUPT;` |
|       - | 1031 | `	}` |
|       - | 1032 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1033 | `	 /* Acquire VM mutex */` |
| 1478553 | 1034 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 1478553 | 1035 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 1478548 | 1036 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1037 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1038 | `	 }` |
|       - | 1039 | `#endif` |
|       - | 1040 | `	/* Install the foreign function */` |
| 1478553 | 1041 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1042 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1043 | `	 /* Leave VM mutex */` |
| 1478553 | 1044 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1045 | `#endif` |
| 1478553 | 1046 | `	return rc;` |
|  739279 | 1047 |  |
|       - | 1048 | `/*` |
|       - | 1049 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1050 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1051 | ` */` |
|     ! 0 | 1052 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1053 |  |
|     ! 0 | 1054 | `	ph7_user_func *pFunc = 0;` |
|       - | 1055 | `	int rc;` |
|       - | 1056 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1057 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1058 | `		return PH7_CORRUPT;` |
|       - | 1059 | `	}` |
|       - | 1060 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1061 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1062 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1063 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1064 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1065 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1066 | `	 }` |
|       - | 1067 | `#endif` |
|       - | 1068 | `	/* Perform the deletion */` |
|     ! 0 | 1069 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1070 | `	if( rc == PH7_OK ){` |
|       - | 1071 | `		/* Release internal fields */` |
|     ! 0 | 1072 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1073 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1074 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1075 | `	}` |
|       - | 1076 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1077 | `	 /* Leave VM mutex */` |
|     ! 0 | 1078 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1079 | `#endif` |
|     ! 0 | 1080 | `	return rc;` |
|     ! 0 | 1081 |  |
|       - | 1082 | `/*` |
|       - | 1083 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1084 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1085 | ` */` |
|  662154 | 1086 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       5 | 1087 |  |
|       - | 1088 | `	SyString sName;` |
|       - | 1089 | `	int rc;` |
|       - | 1090 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  662159 | 1091 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1092 | `		return PH7_CORRUPT;` |
|       - | 1093 | `	}` |
|  662159 | 1094 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1095 | `	/* Remove leading and trailing white spaces */` |
|  665115 | 1096 | `	SyStringFullTrim(&sName);` |
|  662159 | 1097 | `	if( sName.nByte < 1 ){` |
|       - | 1098 | `		/* Empty constant name */` |
|     ! 0 | 1099 | `		return PH7_CORRUPT;` |
|       - | 1100 | `	}` |
|       - | 1101 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  662159 | 1102 | `	if( xExpand == 0 ){` |
|     ! 0 | 1103 | `		return PH7_CORRUPT;` |
|       - | 1104 | `	}` |
|       - | 1105 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1106 | `	 /* Acquire VM mutex */` |
|  662159 | 1107 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  662159 | 1108 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  662154 | 1109 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1110 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1111 | `	 }` |
|       - | 1112 | `#endif` |
|       - | 1113 | `	/* Perform the registration */` |
|  662159 | 1114 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1115 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1116 | `	 /* Leave VM mutex */` |
|  662159 | 1117 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1118 | `#endif` |
|  662159 | 1119 | `	 return rc;` |
|  331082 | 1120 |  |
|       - | 1121 | `/*` |
|       - | 1122 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1123 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1124 | ` */` |
|     ! 0 | 1125 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1126 |  |
|       - | 1127 | `	ph7_constant *pCons;` |
|       - | 1128 | `	int rc;` |
|       - | 1129 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1130 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1131 | `		return PH7_CORRUPT;` |
|       - | 1132 | `	}` |
|       - | 1133 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1134 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1135 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1136 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1137 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1138 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1139 | `	 }` |
|       - | 1140 | `#endif` |
|       - | 1141 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1142 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1143 | `	 if( rc == PH7_OK ){` |
|       - | 1144 | `		 /* Perform the deletion */` |
|     ! 0 | 1145 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1146 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1147 | `	 }` |
|       - | 1148 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1149 | `	 /* Leave VM mutex */` |
|     ! 0 | 1150 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1151 | `#endif` |
|     ! 0 | 1152 | `	return rc;` |
|     ! 0 | 1153 |  |
|       - | 1154 | `/*` |
|       - | 1155 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1156 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1157 | ` */` |
|   10106 | 1158 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       5 | 1159 |  |
|       - | 1160 | `	ph7_value *pObj;` |
|       - | 1161 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   10111 | 1162 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1163 | `		return 0;` |
|       - | 1164 | `	}` |
|       - | 1165 | `	/* Allocate a new scalar variable */` |
|   10111 | 1166 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   10111 | 1167 | `	if( pObj == 0 ){` |
|     ! 0 | 1168 | `		return 0;` |
|       - | 1169 | `	}` |
|       - | 1170 | `	/* Nullify the new scalar */` |
|   10111 | 1171 | `	PH7_MemObjInit(pVm,pObj);` |
|   10111 | 1172 | `	return pObj;` |
|    5058 | 1173 |  |
|       - | 1174 | `/*` |
|       - | 1175 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1176 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1177 | ` */` |
|   39720 | 1178 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       5 | 1179 |  |
|       - | 1180 | `	ph7_hashmap *pMap;` |
|       - | 1181 | `	ph7_value *pObj;` |
|       - | 1182 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   39725 | 1183 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1184 | `		return 0;` |
|       - | 1185 | `	}` |
|       - | 1186 | `	/* Create a new hashmap first */` |
|   39725 | 1187 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   39725 | 1188 | `	if( pMap == 0 ){` |
|     ! 0 | 1189 | `		return 0;` |
|       - | 1190 | `	}` |
|       - | 1191 | `	/* Associate a new ph7_value with this hashmap */` |
|   39725 | 1192 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   39725 | 1193 | `	if( pObj == 0 ){` |
|     ! 0 | 1194 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1195 | `		return 0;` |
|       - | 1196 | `	}` |
|   39725 | 1197 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   39725 | 1198 | `	return pObj;` |
|   19865 | 1199 |  |
|       - | 1200 | `/*` |
|       - | 1201 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1202 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1203 | ` */` |
|   32554 | 1204 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1205 |  |
|       - | 1206 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   32559 | 1207 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1208 | `		return PH7_CORRUPT;` |
|       - | 1209 | `	}` |
|   32559 | 1210 | `	if( pValue ){` |
|       - | 1211 | `		/* Release the value */` |
|   32559 | 1212 | `		PH7_MemObjRelease(pValue);` |
|   32559 | 1213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|   16277 | 1214 | `	}` |
|   32559 | 1215 | `	return PH7_OK;` |
|   16282 | 1216 |  |
|       - | 1217 | `/*` |
|       - | 1218 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1219 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1220 | ` */` |
|  367480 | 1221 | `int ph7_value_to_int(ph7_value *pValue)` |
|       5 | 1222 |  |
|       - | 1223 | `	int rc;` |
|  367485 | 1224 | `	rc = PH7_MemObjToInteger(pValue);` |
|  367485 | 1225 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1226 | `		return 0;` |
|       - | 1227 | `	}` |
|  367485 | 1228 | `	return (int)pValue->x.iVal;` |
|  183745 | 1229 |  |
|       - | 1230 | `/*` |
|       - | 1231 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1232 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1233 | ` */` |
|     336 | 1234 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       5 | 1235 |  |
|       - | 1236 | `	int rc;` |
|     341 | 1237 | `	rc = PH7_MemObjToBool(pValue);` |
|     341 | 1238 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1239 | `		return 0;` |
|       - | 1240 | `	}` |
|     341 | 1241 | `	return (int)pValue->x.iVal;` |
|     173 | 1242 |  |
|       - | 1243 | `/*` |
|       - | 1244 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1245 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1246 | ` */` |
|     690 | 1247 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       3 | 1248 |  |
|       - | 1249 | `	int rc;` |
|     693 | 1250 | `	rc = PH7_MemObjToInteger(pValue);` |
|     693 | 1251 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1252 | `		return 0;` |
|       - | 1253 | `	}` |
|     693 | 1254 | `	return pValue->x.iVal;` |
|     348 | 1255 |  |
|       - | 1256 | `/*` |
|       - | 1257 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1258 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1259 | ` */` |
|     486 | 1260 | `double ph7_value_to_double(ph7_value *pValue)` |
|       1 | 1261 |  |
|       - | 1262 | `	int rc;` |
|     487 | 1263 | `	rc = PH7_MemObjToReal(pValue);` |
|     487 | 1264 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1265 | `		return (double)0;` |
|       - | 1266 | `	}` |
|     487 | 1267 | `	return (double)pValue->rVal;` |
|     244 | 1268 |  |
|       - | 1269 | `/*` |
|       - | 1270 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1271 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1272 | ` */` |
|  687627 | 1273 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       5 | 1274 |  |
|  687632 | 1275 | `	PH7_MemObjToString(pValue);` |
|  687632 | 1276 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  657434 | 1277 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  657434 | 1278 | `		if( pLen ){` |
|  601854 | 1279 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  300967 | 1280 | `		}` |
|  657434 | 1281 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1282 | `	}else{` |
|       - | 1283 | `		/* Return the empty string */` |
|   30203 | 1284 | `		if( pLen ){` |
|   30193 | 1285 | `			*pLen = 0;` |
|   15094 | 1286 | `		}` |
|   30203 | 1287 | `		return "";` |
|       - | 1288 | `	}` |
|  343861 | 1289 |  |
|       - | 1290 | `/*` |
|       - | 1291 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1292 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1293 | ` */` |
|   26578 | 1294 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       5 | 1295 |  |
|   26583 | 1296 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1297 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1298 | `		return 0;` |
|       - | 1299 | `	}` |
|   26583 | 1300 | `	return pValue->x.pOther;` |
|   13294 | 1301 |  |
|       - | 1302 | `/*` |
|       - | 1303 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1304 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1305 | ` */` |
|      30 | 1306 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1307 |  |
|       - | 1308 | `	int rc;` |
|      31 | 1309 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1310 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1311 | `		return 1;` |
|       - | 1312 | `	}` |
|       - | 1313 | `	/* Perform the comparison */` |
|      31 | 1314 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1315 | `	/* Comparison result */` |
|      31 | 1316 | `	return rc;` |
|      16 | 1317 |  |
|       - | 1318 | `/*` |
|       - | 1319 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1320 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1321 | ` */` |
|   10898 | 1322 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       5 | 1323 |  |
|   10903 | 1324 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       5 | 1325 |  |
|       - | 1326 | `/*` |
|       - | 1327 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1328 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1329 | ` */` |
|   14786 | 1330 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       5 | 1331 |  |
|   14791 | 1332 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       5 | 1333 |  |
|       - | 1334 | `/*` |
|       - | 1335 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1336 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1337 | ` */` |
|  323277 | 1338 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       5 | 1339 |  |
|  323282 | 1340 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       5 | 1341 |  |
|       - | 1342 | `/*` |
|       - | 1343 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1344 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1345 | ` */` |
|     428 | 1346 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       1 | 1347 |  |
|     429 | 1348 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       1 | 1349 |  |
|       - | 1350 | `/*` |
|       - | 1351 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1352 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1353 | ` */` |
|     122 | 1354 | `int ph7_result_null(ph7_context *pCtx)` |
|       4 | 1355 |  |
|       - | 1356 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     126 | 1357 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     126 | 1358 | `	return PH7_OK;` |
|       4 | 1359 |  |
|       - | 1360 | `/*` |
|       - | 1361 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1362 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1363 | ` */` |
|  875832 | 1364 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1365 |  |
|  875837 | 1366 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       5 | 1367 |  |
|       - | 1368 | `/*` |
|       - | 1369 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1370 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1371 | ` */` |
|     282 | 1372 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1373 |  |
|       - | 1374 | `	ph7_value *p;` |
|       - | 1375 | `	va_list ap;` |
|       - | 1376 | `	int rc;` |
|     283 | 1377 | `	p = pCtx->pRet;` |
|     283 | 1378 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1379 | `		/* Invalidate any prior representation */` |
|     143 | 1380 | `		PH7_MemObjRelease(p);` |
|     143 | 1381 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|      71 | 1382 | `	}` |
|       - | 1383 | `	/* Format the given string */` |
|     283 | 1384 | `	va_start(ap,zFormat);` |
|     283 | 1385 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|     283 | 1386 | `	va_end(ap);` |
|     283 | 1387 | `	return rc;` |
|       1 | 1388 |  |
|       - | 1389 | `/*` |
|       - | 1390 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1391 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1392 | ` */` |
|   30152 | 1393 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1394 |  |
|   30157 | 1395 | `	int rc = PH7_OK;` |
|   30157 | 1396 | `	if( pValue == 0 ){` |
|     ! 0 | 1397 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1398 | `	}else{` |
|   30157 | 1399 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1400 | `	}` |
|   30157 | 1401 | `	return rc;` |
|       5 | 1402 |  |
|       - | 1403 | `/*` |
|       - | 1404 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1405 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1406 | ` */` |
|    4512 | 1407 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       5 | 1408 |  |
|    4517 | 1409 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       5 | 1410 |  |
|       - | 1411 | `/*` |
|       - | 1412 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1413 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1414 | ` */` |
|    7172 | 1415 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       5 | 1416 |  |
|       - | 1417 | `	ph7_value *pVal;` |
|    7177 | 1418 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|    7177 | 1419 | `	if( pVal ){` |
|       - | 1420 | `		/* Record value address so it can be freed automatically` |
|       - | 1421 | `		 * when the calling function returns.` |
|       - | 1422 | `		 */` |
|    7177 | 1423 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    3586 | 1424 | `	}` |
|    7177 | 1425 | `	return pVal;` |
|       5 | 1426 |  |
|       - | 1427 | `/*` |
|       - | 1428 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1429 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1430 | ` */` |
|   10100 | 1431 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       5 | 1432 |  |
|       - | 1433 | `	ph7_value *pVal;` |
|   10105 | 1434 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   10105 | 1435 | `	if( pVal ){` |
|       - | 1436 | `		/* Record value address so it can be freed automatically` |
|       - | 1437 | `		 * when the calling function returns.` |
|       - | 1438 | `		 */` |
|   10105 | 1439 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    5050 | 1440 | `	}` |
|   10105 | 1441 | `	return pVal;` |
|       5 | 1442 |  |
|       - | 1443 | `/*` |
|       - | 1444 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1445 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1446 | ` */` |
|     382 | 1447 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1448 |  |
|     387 | 1449 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     387 | 1450 |  |
|       - | 1451 | `/*` |
|       - | 1452 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1453 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1454 | ` */` |
|    4482 | 1455 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       5 | 1456 |  |
|       - | 1457 | `	void *pChunk;` |
|    4487 | 1458 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    4487 | 1459 | `	if( pChunk ){` |
|    4487 | 1460 | `		if( ZeroChunk ){` |
|       - | 1461 | `			/* Zero the memory chunk */` |
|    4453 | 1462 | `			SyZero(pChunk,nByte);` |
|    2224 | 1463 | `		}` |
|    4487 | 1464 | `		if( AutoRelease ){` |
|       - | 1465 | `			ph7_aux_data sAux;` |
|       - | 1466 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1467 | `			 * upon this context is destroyed.` |
|       - | 1468 | `			 */` |
|      25 | 1469 | `			sAux.pAuxData = pChunk;` |
|      25 | 1470 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|      12 | 1471 | `		}` |
|    2241 | 1472 | `	}` |
|    4487 | 1473 | `	return pChunk;` |
|       5 | 1474 |  |
|       - | 1475 | `/*` |
|       - | 1476 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1477 | ` * chunk container.` |
|       - | 1478 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1479 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1480 | ` */` |
|    4430 | 1481 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1482 |  |
|       - | 1483 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1484 | `	sxu32 n;` |
|    4435 | 1485 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1486 | `		/* Don't bother processing,the container is empty */` |
|    4435 | 1487 | `		return 0;` |
|       - | 1488 | `	}` |
|       - | 1489 | `	/* Perform the lookup */` |
|     ! 0 | 1490 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1491 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1492 | `		pAux = &aAux[n];` |
|     ! 0 | 1493 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1494 | `			/* Chunk found */` |
|     ! 0 | 1495 | `			return pAux;` |
|       - | 1496 | `		}` |
|     ! 0 | 1497 | `	}` |
|       - | 1498 | `	/* No such allocated chunk */` |
|     ! 0 | 1499 | `	return 0;` |
|    2220 | 1500 |  |
|       - | 1501 | `/*` |
|       - | 1502 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1503 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1504 | ` */` |
|     ! 0 | 1505 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1506 |  |
|       - | 1507 | `	ph7_aux_data *pAux;` |
|       - | 1508 | `	void *pNew;` |
|     ! 0 | 1509 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1510 | `	if( pNew ){` |
|     ! 0 | 1511 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1512 | `		if( pAux ){` |
|     ! 0 | 1513 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1514 | `		}` |
|     ! 0 | 1515 | `	}` |
|     ! 0 | 1516 | `	return pNew;` |
|     ! 0 | 1517 |  |
|       - | 1518 | `/*` |
|       - | 1519 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1520 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1521 | ` */` |
|    4430 | 1522 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1523 |  |
|       - | 1524 | `	ph7_aux_data *pAux;` |
|    4435 | 1525 | `	if( pChunk == 0 ){` |
|       - | 1526 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1527 | `		return;` |
|       - | 1528 | `	}` |
|    4435 | 1529 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    4435 | 1530 | `	if( pAux ){` |
|       - | 1531 | `		/* Mark as destroyed */` |
|     ! 0 | 1532 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1533 | `	}` |
|    4435 | 1534 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    2220 | 1535 |  |
|       - | 1536 | `/*` |
|       - | 1537 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1538 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1539 | ` */` |
|     ! 0 | 1540 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|     ! 0 | 1541 |  |
|       - | 1542 | `	ph7_hashmap_node *pNode;` |
|       - | 1543 | `	ph7_value *pValue;` |
|       - | 1544 | `	ph7_value skey;` |
|       - | 1545 | `	int rc;` |
|       - | 1546 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 1547 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1548 | `		return 0;` |
|       - | 1549 | `	}` |
|     ! 0 | 1550 | `	if( nByte < 0 ){` |
|     ! 0 | 1551 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1552 | `	}` |
|       - | 1553 | `	/* Convert the key to a ph7_value  */` |
|     ! 0 | 1554 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     ! 0 | 1555 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1556 | `	/* Perform the lookup */` |
|     ! 0 | 1557 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     ! 0 | 1558 | `	PH7_MemObjRelease(&skey);` |
|     ! 0 | 1559 | `	if( rc != PH7_OK ){` |
|       - | 1560 | `		/* No such entry */` |
|     ! 0 | 1561 | `		return 0;` |
|       - | 1562 | `	}` |
|       - | 1563 | `	/* Extract the target value */` |
|     ! 0 | 1564 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     ! 0 | 1565 | `	return pValue;` |
|     ! 0 | 1566 |  |
|       - | 1567 | `/*` |
|       - | 1568 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1569 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1570 | ` */` |
|   30728 | 1571 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       5 | 1572 |  |
|       - | 1573 | `	int rc;` |
|   30733 | 1574 | `	if( xWalk == 0 ){` |
|     ! 0 | 1575 | `		return PH7_CORRUPT;` |
|       - | 1576 | `	}` |
|       - | 1577 | `	/* Make sure we are dealing with a valid hashmap */` |
|   30733 | 1578 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1579 | `		return PH7_CORRUPT;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Start the walk process */` |
|   30733 | 1582 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   30733 | 1583 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   15369 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1587 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1588 | ` */` |
| 2281846 | 1589 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       5 | 1590 |  |
|       - | 1591 | `	int rc;` |
|       - | 1592 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2281851 | 1593 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1594 | `		return PH7_CORRUPT;` |
|       - | 1595 | `	}` |
|       - | 1596 | `	/* Perform the insertion */` |
| 2281851 | 1597 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
| 2281851 | 1598 | `	return rc;` |
| 1140928 | 1599 |  |
|       - | 1600 | `/*` |
|       - | 1601 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1602 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1603 | ` */` |
|    5562 | 1604 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       5 | 1605 |  |
|       - | 1606 | `	int rc;` |
|       - | 1607 | `	/* Make sure we are dealing with a valid hashmap */` |
|    5567 | 1608 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1609 | `		return PH7_CORRUPT;` |
|       - | 1610 | `	}` |
|       - | 1611 | `	/* Perform the insertion */` |
|    5567 | 1612 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1613 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1614 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1615 | `	}else{` |
|       - | 1616 | `		ph7_value sKey;` |
|    5567 | 1617 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|    5567 | 1618 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|    5567 | 1619 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|    5567 | 1620 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1621 | `	}` |
|    5567 | 1622 | `	return rc;` |
|    2786 | 1623 |  |
|       - | 1624 | `/*` |
|       - | 1625 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1626 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1627 | ` */` |
|     314 | 1628 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       5 | 1629 |  |
|       - | 1630 | `	ph7_value sKey;` |
|       - | 1631 | `	int rc;` |
|       - | 1632 | `	/* Make sure we are dealing with a valid hashmap */` |
|     319 | 1633 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1634 | `		return PH7_CORRUPT;` |
|       - | 1635 | `	}` |
|     319 | 1636 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1637 | `	/* Perform the insertion */` |
|     319 | 1638 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|     319 | 1639 | `	PH7_MemObjRelease(&sKey);` |
|     319 | 1640 | `	return rc;` |
|     162 | 1641 |  |
|       - | 1642 | `/*` |
|       - | 1643 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1644 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1645 | ` */` |
|  126334 | 1646 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       5 | 1647 |  |
|       - | 1648 | `	ph7_hashmap *pMap;` |
|       - | 1649 | `	/* Make sure we are dealing with a valid hashmap */` |
|  126339 | 1650 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1651 | `		return 0;` |
|       - | 1652 | `	}` |
|       - | 1653 | `	/* Point to the internal representation of the hashmap */` |
|  126339 | 1654 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  126339 | 1655 | `	return pMap->nEntry;` |
|   63172 | 1656 |  |
|       - | 1657 | `/*` |
|       - | 1658 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1659 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1660 | ` */` |
|       2 | 1661 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|       1 | 1662 |  |
|       - | 1663 | `	int rc;` |
|       3 | 1664 | `	if( xWalk == 0 ){` |
|     ! 0 | 1665 | `		return PH7_CORRUPT;` |
|       - | 1666 | `	}` |
|       - | 1667 | `	/* Make sure we are dealing with a valid class instance */` |
|       3 | 1668 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1669 | `		return PH7_CORRUPT;` |
|       - | 1670 | `	}` |
|       - | 1671 | `	/* Start the walk process */` |
|       3 | 1672 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|       3 | 1673 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|       2 | 1674 |  |
|       - | 1675 | `/*` |
|       - | 1676 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1677 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1678 | ` */` |
|       8 | 1679 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1680 |  |
|       - | 1681 | `	ph7_value *pValue;` |
|       - | 1682 | `	SyString sAttr;` |
|       - | 1683 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1684 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1685 | `		return 0;` |
|       - | 1686 | `	}` |
|       9 | 1687 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1688 | `	/* Extract the attribute value if available.` |
|       - | 1689 | `	 */` |
|       9 | 1690 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1691 | `	return pValue;` |
|       5 | 1692 |  |
|       - | 1693 | `/*` |
|       - | 1694 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1695 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1696 | ` */` |
|     ! 0 | 1697 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1698 |  |
|       - | 1699 | `	ph7_class *pClass;` |
|     ! 0 | 1700 | `	if( pLength ){` |
|     ! 0 | 1701 | `		*pLength = 0;` |
|     ! 0 | 1702 | `	}` |
|       - | 1703 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1704 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1705 | `		return 0;` |
|       - | 1706 | `	}` |
|       - | 1707 | `	/* Point to the class */` |
|     ! 0 | 1708 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1709 | `	/* Return the class name */` |
|     ! 0 | 1710 | `	if( pLength ){` |
|     ! 0 | 1711 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1712 | `	}` |
|     ! 0 | 1713 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1714 |  |
|       - | 1715 | `/*` |
|       - | 1716 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1717 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1718 | ` */` |
|     370 | 1719 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       3 | 1720 |  |
|       - | 1721 | `	SyString sData;` |
|       - | 1722 | `	int rc;` |
|     373 | 1723 | `	if( nLen < 0 ){` |
|     ! 0 | 1724 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1725 | `	}` |
|     373 | 1726 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|     373 | 1727 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|     373 | 1728 | `	return rc;` |
|       3 | 1729 |  |
|       - | 1730 | `/*` |
|       - | 1731 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1732 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1733 | ` */` |
|       2 | 1734 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1735 |  |
|       - | 1736 | `	va_list ap;` |
|       - | 1737 | `	int rc;` |
|       3 | 1738 | `	va_start(ap,zFormat);` |
|       3 | 1739 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1740 | `	va_end(ap);` |
|       3 | 1741 | `	return rc;` |
|       1 | 1742 |  |
|       - | 1743 | `/*` |
|       - | 1744 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1745 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1746 | ` */` |
|      24 | 1747 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       3 | 1748 |  |
|      27 | 1749 | `	int rc = PH7_OK;` |
|      27 | 1750 | `	if( zErr ){` |
|      27 | 1751 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      12 | 1752 | `	}` |
|      27 | 1753 | `	return rc;` |
|       3 | 1754 |  |
|       - | 1755 | `/*` |
|       - | 1756 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1757 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1758 | ` */` |
|      24 | 1759 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       4 | 1760 |  |
|       - | 1761 | `	va_list ap;` |
|       - | 1762 | `	int rc;` |
|      28 | 1763 | `	if( zFormat == 0){` |
|     ! 0 | 1764 | `		return PH7_OK;` |
|       - | 1765 | `	}` |
|      28 | 1766 | `	va_start(ap,zFormat);` |
|      28 | 1767 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      28 | 1768 | `	va_end(ap);` |
|      28 | 1769 | `	return rc;` |
|      16 | 1770 |  |
|       - | 1771 | `/*` |
|       - | 1772 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1773 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1774 | ` */` |
|      34 | 1775 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1776 |  |
|       - | 1777 | `	sxu32 n;` |
|      35 | 1778 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1779 | `	return n;` |
|       1 | 1780 |  |
|       - | 1781 | `/*` |
|       - | 1782 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1783 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1784 | ` */` |
|     ! 0 | 1785 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1786 |  |
|     ! 0 | 1787 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1788 | `		return PH7_CORRUPT;` |
|       - | 1789 | `	}` |
|     ! 0 | 1790 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1791 | `	return PH7_OK;` |
|     ! 0 | 1792 |  |
|       - | 1793 | `/*` |
|       - | 1794 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1795 | ` *` |
|       - | 1796 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1797 | ` * {` |
|       - | 1798 | ` *	return pCtx->pVm;` |
|       - | 1799 | ` * }` |
|       - | 1800 | ` */` |
|       - | 1801 | `/*` |
|       - | 1802 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1803 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1804 | ` */` |
|   55868 | 1805 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       5 | 1806 |  |
|   55873 | 1807 | `	return pCtx->pFunc->pUserData;` |
|       5 | 1808 |  |
|       - | 1809 | `/*` |
|       - | 1810 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1811 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1812 | ` */` |
|       2 | 1813 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1814 |  |
|       - | 1815 | `	ph7_aux_data sAux;` |
|       - | 1816 | `	int rc;` |
|       3 | 1817 | `	sAux.pAuxData = pUserData;` |
|       3 | 1818 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1819 | `	return rc;` |
|       1 | 1820 |  |
|       - | 1821 | `/*` |
|       - | 1822 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1823 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1824 | ` */` |
|       6 | 1825 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1826 |  |
|       - | 1827 | `	ph7_aux_data *pAux;` |
|       7 | 1828 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       7 | 1829 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1830 |  |
|       - | 1831 | `/*` |
|       - | 1832 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1833 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1834 | ` */` |
|       2 | 1835 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|       1 | 1836 |  |
|       - | 1837 | `	ph7_aux_data *pAux;` |
|       3 | 1838 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|       3 | 1839 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1840 |  |
|       - | 1841 | `/*` |
|       - | 1842 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1843 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1844 | ` */` |
|    6002 | 1845 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       5 | 1846 |  |
|    6007 | 1847 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       5 | 1848 |  |
|       - | 1849 | `/*` |
|       - | 1850 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1851 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1852 | ` */` |
|   22862 | 1853 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       5 | 1854 |  |
|       - | 1855 | `	SyString *pName;` |
|   22867 | 1856 | `	pName = &pCtx->pFunc->sName;` |
|   22867 | 1857 | `	return pName->zString;` |
|       5 | 1858 |  |
|       - | 1859 | `/*` |
|       - | 1860 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1861 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1862 | ` */` |
|   29840 | 1863 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       5 | 1864 |  |
|       - | 1865 | `	/* Invalidate any prior representation */` |
|   29845 | 1866 | `	PH7_MemObjRelease(pVal);` |
|   29845 | 1867 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|   29845 | 1868 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   29845 | 1869 | `	return PH7_OK;` |
|       5 | 1870 |  |
|       - | 1871 | `/*` |
|       - | 1872 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1873 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1874 | ` */` |
|   14876 | 1875 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       5 | 1876 |  |
|       - | 1877 | `	/* Invalidate any prior representation */` |
|   14881 | 1878 | `	PH7_MemObjRelease(pVal);` |
|   14881 | 1879 | `	pVal->x.iVal = iValue;` |
|   14881 | 1880 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   14881 | 1881 | `	return PH7_OK;` |
|       5 | 1882 |  |
|       - | 1883 | `/*` |
|       - | 1884 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1885 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1886 | ` */` |
|  323277 | 1887 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       5 | 1888 |  |
|       - | 1889 | `	/* Invalidate any prior representation */` |
|  323282 | 1890 | `	PH7_MemObjRelease(pVal);` |
|  323282 | 1891 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  323282 | 1892 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  323282 | 1893 | `	return PH7_OK;` |
|       5 | 1894 |  |
|       - | 1895 | `/*` |
|       - | 1896 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1897 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1898 | ` */` |
|       4 | 1899 | `int ph7_value_null(ph7_value *pVal)` |
|       1 | 1900 |  |
|       - | 1901 | `	/* Invalidate any prior representation and set the NULL flag */` |
|       5 | 1902 | `	PH7_MemObjRelease(pVal);` |
|       5 | 1903 | `	return PH7_OK;` |
|       1 | 1904 |  |
|       - | 1905 | `/*` |
|       - | 1906 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1907 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1908 | ` */` |
|     532 | 1909 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       1 | 1910 |  |
|       - | 1911 | `	/* Invalidate any prior representation */` |
|     533 | 1912 | `	PH7_MemObjRelease(pVal);` |
|     533 | 1913 | `	pVal->rVal = (ph7_real)Value;` |
|     533 | 1914 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1915 | `	/* Try to get an integer representation also */` |
|     533 | 1916 | `	PH7_MemObjTryInteger(pVal);` |
|     533 | 1917 | `	return PH7_OK;` |
|       1 | 1918 |  |
|       - | 1919 | `/*` |
|       - | 1920 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1921 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1922 | ` */` |
| 1021166 | 1923 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       5 | 1924 |  |
| 1021171 | 1925 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1926 | `		/* Invalidate any prior representation */` |
|  350241 | 1927 | `		PH7_MemObjRelease(pVal);` |
|  350241 | 1928 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  175118 | 1929 | `	}` |
| 1021171 | 1930 | `	if( zString ){` |
| 1019939 | 1931 | `		if( nLen < 0 ){` |
|       - | 1932 | `			/* Compute length automatically */` |
|    3875 | 1933 | `			nLen = (int)SyStrlen(zString);` |
|    1935 | 1934 | `		}` |
|       - | 1935 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1936 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
| 1019939 | 1937 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1938 | `	}` |
|    1233 | 1939 | `	return PH7_OK;` |
|  510588 | 1940 |  |
|       - | 1941 | `/*` |
|       - | 1942 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1943 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1944 | ` */` |
|      22 | 1945 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1946 |  |
|       - | 1947 | `	va_list ap;` |
|       - | 1948 | `	int rc;` |
|      23 | 1949 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1950 | `		/* Invalidate any prior representation */` |
|      19 | 1951 | `		PH7_MemObjRelease(pVal);` |
|      19 | 1952 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       9 | 1953 | `	}` |
|      23 | 1954 | `	va_start(ap,zFormat);` |
|      23 | 1955 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      23 | 1956 | `	va_end(ap);` |
|       - | 1957 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|      23 | 1958 | `	return rc;` |
|       1 | 1959 |  |
|       - | 1960 | `/*` |
|       - | 1961 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1962 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1963 | ` */` |
|  133952 | 1964 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       5 | 1965 |  |
|       - | 1966 | `	/* Reset the string cursor */` |
|  133957 | 1967 | `	SyBlobReset(&pVal->sBlob);` |
|  133957 | 1968 | `	return PH7_OK;` |
|       5 | 1969 |  |
|       - | 1970 | `/*` |
|       - | 1971 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1972 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1973 | ` */` |
|    4602 | 1974 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       5 | 1975 |  |
|       - | 1976 | `	/* Invalidate any prior representation */` |
|    4607 | 1977 | `	PH7_MemObjRelease(pVal);` |
|       - | 1978 | `	/* Reflect the new type */` |
|    4607 | 1979 | `	pVal->x.pOther = pUserData;` |
|    4607 | 1980 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    4607 | 1981 | `	return PH7_OK;` |
|       5 | 1982 |  |
|       - | 1983 | `/*` |
|       - | 1984 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 1985 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1986 | ` */` |
|    3430 | 1987 | `int ph7_value_release(ph7_value *pVal)` |
|       5 | 1988 |  |
|    3435 | 1989 | `	PH7_MemObjRelease(pVal);` |
|    3435 | 1990 | `	return PH7_OK;` |
|       5 | 1991 |  |
|       - | 1992 | `/*` |
|       - | 1993 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 1994 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1995 | ` */` |
|   12376 | 1996 | `int ph7_value_is_int(ph7_value *pVal)` |
|       5 | 1997 |  |
|       - | 1998 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 1999 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 2000 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 2001 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 2002 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|   12381 | 2003 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       5 | 2004 |  |
|       - | 2005 | `/*` |
|       - | 2006 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 2007 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2008 | ` */` |
|    1250 | 2009 | `int ph7_value_is_float(ph7_value *pVal)` |
|       5 | 2010 |  |
|    1255 | 2011 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       5 | 2012 |  |
|       - | 2013 | `/*` |
|       - | 2014 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 2015 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2016 | ` */` |
|     508 | 2017 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       5 | 2018 |  |
|     513 | 2019 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       5 | 2020 |  |
|       - | 2021 | `/*` |
|       - | 2022 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2023 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2024 | ` */` |
|   96360 | 2025 | `int ph7_value_is_string(ph7_value *pVal)` |
|       5 | 2026 |  |
|   96365 | 2027 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       5 | 2028 |  |
|       - | 2029 | `/*` |
|       - | 2030 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2031 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2032 | ` */` |
|    1726 | 2033 | `int ph7_value_is_null(ph7_value *pVal)` |
|       5 | 2034 |  |
|    1731 | 2035 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       5 | 2036 |  |
|       - | 2037 | `/*` |
|       - | 2038 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2039 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2040 | ` */` |
|     368 | 2041 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       5 | 2042 |  |
|       - | 2043 | `	int rc;` |
|     373 | 2044 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|     373 | 2045 | `	return rc;` |
|       5 | 2046 |  |
|       - | 2047 | `/*` |
|       - | 2048 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2049 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2050 | ` */` |
|   24406 | 2051 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       5 | 2052 |  |
|       - | 2053 | `	int rc;` |
|   24411 | 2054 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   24411 | 2055 | `	return rc;` |
|       5 | 2056 |  |
|       - | 2057 | `/*` |
|       - | 2058 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2059 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2060 | ` */` |
|      12 | 2061 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2062 |  |
|      13 | 2063 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2064 |  |
|       - | 2065 | `/*` |
|       - | 2066 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2067 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2068 | ` */` |
|  144764 | 2069 | `int ph7_value_is_array(ph7_value *pVal)` |
|       5 | 2070 |  |
|  144769 | 2071 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       5 | 2072 |  |
|       - | 2073 | `/*` |
|       - | 2074 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2075 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2076 | ` */` |
|    2238 | 2077 | `int ph7_value_is_object(ph7_value *pVal)` |
|       5 | 2078 |  |
|    2243 | 2079 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       5 | 2080 |  |
|       - | 2081 | `/*` |
|       - | 2082 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2083 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2084 | ` */` |
|   28406 | 2085 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       5 | 2086 |  |
|   28411 | 2087 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       5 | 2088 |  |
|       - | 2089 | `/*` |
|       - | 2090 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2091 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2092 | ` */` |
|   26164 | 2093 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       5 | 2094 |  |
|       - | 2095 | `	int rc;` |
|   26169 | 2096 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   26169 | 2097 | `	return rc;` |
|       5 | 2098 |  |
|       - | 2099 | `/*` |
|       - | 2100 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2101 | ` * Check if a value holds a Fiber instance.` |
|       - | 2102 | ` */` |
|     ! 0 | 2103 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2104 |  |
|     ! 0 | 2105 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2106 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2107 |  |
|       - | 2108 | `/*` |
|       - | 2109 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2110 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2111 | ` */` |
|     ! 0 | 2112 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2113 |  |
|     ! 0 | 2114 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2115 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2116 |  |
|       - | 2117 | `/*` |
|       - | 2118 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2119 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2120 | ` */` |
|     ! 0 | 2121 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2122 |  |
|     ! 0 | 2123 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2124 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2125 |  |
|       - | 2126 | `/*` |
|       - | 2127 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2128 | ` * Check if a Fiber is currently suspended.` |
|       - | 2129 | ` */` |
|     ! 0 | 2130 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2131 |  |
|     ! 0 | 2132 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2133 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2134 |  |
|       - | 2135 | `/*` |
|       - | 2136 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2137 | ` * Check if a Fiber has completed execution.` |
|       - | 2138 | ` */` |
|     ! 0 | 2139 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2140 |  |
|     ! 0 | 2141 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2142 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2143 |  |
|       - | 2144 | `/*` |
|       - | 2145 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2146 | ` * Get the return value of a terminated Fiber.` |
|       - | 2147 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2148 | ` */` |
|     ! 0 | 2149 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2150 |  |
|     ! 0 | 2151 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2152 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2153 |  |
|       - | 2154 |  |
