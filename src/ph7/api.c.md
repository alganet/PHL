# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 782/1102 lines (70.96%)

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
|       - |   28 | `{` |
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
|       - |   45 | `	{0,0,0,0,0,0,0,0,0,0,0,{0}},` |
|       - |   46 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |   47 | `	0,` |
|       - |   48 | `	0,` |
|       - |   49 | `	0,` |
|       - |   50 | `#endif` |
|       - |   51 | `	0,` |
|       - |   52 | `	0,` |
|       - |   53 | `	0,` |
|       - |   54 | `	0` |
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
|    7630 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       5 |   79 | `{` |
|    7635 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    7635 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    7635 |   83 | `	switch(nOp){` |
|    3815 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    7635 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    7635 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    7635 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    7635 |   93 | `		pConf->xErr     = xConsumer;` |
|    7635 |   94 | `		pConf->pErrData = pUserData;` |
|    7635 |   95 | `		break;` |
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
|     ! 0 |  139 | `	case PH7_CONFIG_MAX_INPUT: {` |
|       - |  140 | `		/* Per-compile input byte cap (0 = use PH7_MAX_INPUT_SIZE default). */` |
|     ! 0 |  141 | `		unsigned int nMax = va_arg(ap,unsigned int);` |
|     ! 0 |  142 | `		pEngine->xConf.nMaxInput = (sxu32)nMax;` |
|     ! 0 |  143 | `		break;` |
|       - |  144 | `								}` |
|     ! 0 |  145 | `	default:` |
|       - |  146 | `		/* Unknown configuration verb */` |
|     ! 0 |  147 | `		rc = PH7_CORRUPT;` |
|     ! 0 |  148 | `		break;` |
|       - |  149 | `	} /* Switch() */` |
|    7635 |  150 | `	return rc;` |
|       5 |  151 | `}` |
|       - |  152 | `/*` |
|       - |  153 | ` * Configure the PH7 library.` |
|       - |  154 | ` * return PH7_OK on success.Any other return value` |
|       - |  155 | ` * indicates failure.` |
|       - |  156 | ` * Refer to [ph7_lib_config()].` |
|       - |  157 | ` */` |
|   11484 |  158 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       5 |  159 | `{` |
|   11489 |  160 | `	int rc = PH7_OK;` |
|   11489 |  161 | `	switch(nOp){` |
|    1914 |  162 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  163 | `			/* Install a virtual file system */` |
|    3833 |  164 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    3833 |  165 | `			sMPGlobal.pVfs = pVfs;` |
|    3833 |  166 | `			break;` |
|       - |  167 | `								}` |
|    1914 |  168 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  169 | `			/* Use an alternative low-level memory allocation routines */` |
|    3833 |  170 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  171 | `			/* Save the memory failure callback (if available) */` |
|    3833 |  172 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    3833 |  173 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    3833 |  174 | `			if( pMethods == 0 ){` |
|       - |  175 | `				/* Use the built-in memory allocation subsystem */` |
|    3833 |  176 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    1919 |  177 | `			}else{` |
|     ! 0 |  178 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  179 | `			}` |
|    3833 |  180 | `			break;` |
|       - |  181 | `										  }` |
|     ! 0 |  182 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  183 | `			/* Memory failure callback */` |
|     ! 0 |  184 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  185 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  186 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  187 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  188 | `			break;` |
|       - |  189 | `												 }` |
|    1914 |  190 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  191 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  192 | `			/* Use an alternative low-level mutex subsystem */` |
|    3833 |  193 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  194 | `#if defined (UNTRUST)` |
|       - |  195 | `			if( pMethods == 0 ){` |
|       - |  196 | `				rc = PH7_CORRUPT;` |
|       - |  197 | `			}` |
|       - |  198 | `#endif` |
|       - |  199 | `			/* Sanity check */` |
|    3833 |  200 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  201 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  202 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  203 | `				break;` |
|       - |  204 | `			}` |
|    3833 |  205 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  206 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  207 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  208 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  209 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  210 | `				}` |
|     ! 0 |  211 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  212 | `			}` |
|       - |  213 | `			/* Initialize and install the new mutex subsystem */` |
|    3833 |  214 | `			if( pMethods->xGlobalInit ){` |
|       5 |  215 | `				rc = pMethods->xGlobalInit();` |
|       5 |  216 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  217 | `					break;` |
|       - |  218 | `				}` |
|     ! 0 |  219 | `			}` |
|       - |  220 | `			/* Create the global mutex */` |
|    3833 |  221 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    3833 |  222 | `			if( sMPGlobal.pMutex == 0 ){` |
|       - |  223 | `				/*` |
|       - |  224 | `				 * If the supplied mutex subsystem is so sick that we are unable to` |
|       - |  225 | `				 * create a single mutex,there is no much we can do here.` |
|       - |  226 | `				 */` |
|     ! 0 |  227 | `				if( pMethods->xGlobalRelease ){` |
|     ! 0 |  228 | `					pMethods->xGlobalRelease();` |
|     ! 0 |  229 | `				}` |
|     ! 0 |  230 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  231 | `				break;` |
|       - |  232 | `			}` |
|    3833 |  233 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    3833 |  234 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  235 | `				/* Set a default threading level */` |
|    3833 |  236 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    1914 |  237 | `			}` |
|       - |  238 | `#endif` |
|    3833 |  239 | `			break;` |
|       - |  240 | `										   }` |
|     ! 0 |  241 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:` |
|       - |  242 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  243 | `			/* Single thread mode(Only one thread is allowed to play with the library) */` |
|     ! 0 |  244 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|       - |  245 | `#endif` |
|     ! 0 |  246 | `			break;` |
|     ! 0 |  247 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:` |
|       - |  248 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  249 | `			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines` |
|       - |  250 | `			 * may be shared between multiple threads).` |
|       - |  251 | `			 */` |
|     ! 0 |  252 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|       - |  253 | `#endif` |
|     ! 0 |  254 | `			break;` |
|     ! 0 |  255 | `		default:` |
|       - |  256 | `			/* Unknown configuration option */` |
|     ! 0 |  257 | `			rc = PH7_CORRUPT;` |
|     ! 0 |  258 | `			break;` |
|       - |  259 | `	}` |
|   11489 |  260 | `	return rc;` |
|       5 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  264 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  265 | ` */` |
|   11484 |  266 | `int ph7_lib_config(int nConfigOp,...)` |
|       5 |  267 | `{` |
|       - |  268 | `	va_list ap;` |
|       - |  269 | `	int rc;` |
|       - |  270 |  |
|   11489 |  271 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  272 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  273 | `		return PH7_LOOKED;` |
|       - |  274 | `	}` |
|   11489 |  275 | `	va_start(ap,nConfigOp);` |
|   11489 |  276 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|   11489 |  277 | `	va_end(ap);` |
|   11489 |  278 | `	return rc;` |
|    5747 |  279 | `}` |
|       - |  280 | `/*` |
|       - |  281 | ` * Global library initialization` |
|       - |  282 | ` * Refer to [ph7_lib_init()]` |
|       - |  283 | ` * This routine must be called to initialize the memory allocation subsystem,the mutex` |
|       - |  284 | ` * subsystem prior to doing any serious work with the library.The first thread to call` |
|       - |  285 | ` * this routine does the initialization process and set the magic number so no body later` |
|       - |  286 | ` * can re-initialize the library.If subsequent threads call this  routine before the first` |
|       - |  287 | ` * thread have finished the initialization process, then the subsequent threads must block` |
|       - |  288 | ` * until the initialization process is done.` |
|       - |  289 | ` */` |
|    3828 |  290 | `static sxi32 PH7CoreInitialize(void)` |
|       5 |  291 | `{` |
|       - |  292 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  293 | `#if defined(PH7_ENABLE_THREADS)` |
|    3833 |  294 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    3833 |  295 | `	SyMutex *pMaster = 0;` |
|       - |  296 | `#endif` |
|       - |  297 | `	int rc;` |
|       - |  298 | `	/*` |
|       - |  299 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  300 | `	 * is a no-op.` |
|       - |  301 | `	 */` |
|    3833 |  302 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  303 | `		return PH7_OK; /* Already initialized */` |
|       - |  304 | `	}` |
|       - |  305 | `	/* Point to the built-in vfs */` |
|    3833 |  306 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  307 | `	/* Install it */` |
|    3833 |  308 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  309 | `#if defined(PH7_ENABLE_THREADS)` |
|    3833 |  310 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    3833 |  311 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    3833 |  312 | `		if( pMutexMethods == 0 ){` |
|       - |  313 | `			/* Use the built-in mutex subsystem */` |
|    3833 |  314 | `			pMutexMethods = SyMutexExportMethods();` |
|    3833 |  315 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  316 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  317 | `			}` |
|       - |  318 | `			/* Install the mutex subsystem */` |
|    3833 |  319 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    3833 |  320 | `			if( rc != PH7_OK ){` |
|     ! 0 |  321 | `				return rc;` |
|       - |  322 | `			}` |
|    1914 |  323 | `		}` |
|       - |  324 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    3833 |  325 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    3833 |  326 | `		if( pMaster == 0 ){` |
|     ! 0 |  327 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  328 | `		}` |
|    1914 |  329 | `	}` |
|       - |  330 | `	/* Lock the master mutex */` |
|    3833 |  331 | `	rc = PH7_OK;` |
|    3833 |  332 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    5747 |  333 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  334 | `#endif` |
|    3833 |  335 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  336 | `			/* Install a memory subsystem */` |
|    3833 |  337 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    3833 |  338 | `			if( rc != PH7_OK ){` |
|       - |  339 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  340 | `				goto End;` |
|       - |  341 | `			}` |
|    1914 |  342 | `		}` |
|       - |  343 | `#if defined(PH7_ENABLE_THREADS)` |
|    3833 |  344 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  345 | `			/* Protect the memory allocation subsystem */` |
|    3833 |  346 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    3833 |  347 | `			if( rc != PH7_OK ){` |
|     ! 0 |  348 | `				goto End;` |
|       - |  349 | `			}` |
|    1914 |  350 | `		}` |
|       - |  351 | `#endif` |
|       - |  352 | `		/* Our library is initialized,set the magic number */` |
|    3833 |  353 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    3833 |  354 | `		rc = PH7_OK;` |
|       - |  355 | `#if defined(PH7_ENABLE_THREADS)` |
|    1914 |  356 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  357 | `#endif` |
|     ! 0 |  358 | `End:` |
|       - |  359 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  360 | `	/* Unlock the master mutex */` |
|    3833 |  361 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  362 | `#endif` |
|    3833 |  363 | `	return rc;` |
|    1919 |  364 | `}` |
|       - |  365 | `/*` |
|       - |  366 | ` * [CAPIREF: ph7_lib_init()]` |
|       - |  367 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  368 | ` */` |
|     ! 0 |  369 | `int ph7_lib_init(void)` |
|     ! 0 |  370 | `{` |
|       - |  371 | `	int rc;` |
|     ! 0 |  372 | `	rc = PH7CoreInitialize();` |
|     ! 0 |  373 | `	return rc;` |
|     ! 0 |  374 | `}` |
|       - |  375 | `/*` |
|       - |  376 | ` * Release an active PH7 engine and it's associated active virtual machines.` |
|       - |  377 | ` */` |
|    3834 |  378 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       5 |  379 | `{` |
|       - |  380 | `	ph7_vm *pVm,*pNext;` |
|       - |  381 | `	/* Release all active VM */` |
|    3839 |  382 | `	pVm = pEngine->pVms;` |
|    1917 |  383 | `	for(;;){` |
|    3839 |  384 | `		if( pEngine->iVm <= 0 ){` |
|    3839 |  385 | `			break;` |
|       - |  386 | `		}` |
|     ! 0 |  387 | `		pNext = pVm->pNext;` |
|     ! 0 |  388 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  389 | `		pVm = pNext;` |
|     ! 0 |  390 | `		pEngine->iVm--;` |
|     ! 0 |  391 | `	}` |
|       - |  392 | `	/* Set a dummy magic number */` |
|    3839 |  393 | `	pEngine->nMagic = 0x7635;` |
|       - |  394 | `	/* Release the private memory subsystem */` |
|    3839 |  395 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    3839 |  396 | `	return PH7_OK;` |
|       5 |  397 | `}` |
|       - |  398 | `/*` |
|       - |  399 | ` * Release all resources consumed by the library.` |
|       - |  400 | ` * If PH7 is already shut down when this routine` |
|       - |  401 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  402 | ` * Note: This call is not thread safe.` |
|       - |  403 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  404 | ` */` |
|     456 |  405 | `static void PH7CoreShutdown(void)` |
|       4 |  406 | `{` |
|       - |  407 | `	ph7 *pEngine,*pNext;` |
|       - |  408 | `	/* Release all active engines first */` |
|     460 |  409 | `	pEngine = sMPGlobal.pEngines;` |
|     456 |  410 | `	for(;;){` |
|     916 |  411 | `		if( sMPGlobal.nEngine < 1 ){` |
|     460 |  412 | `			break;` |
|       - |  413 | `		}` |
|     460 |  414 | `		pNext = pEngine->pNext;` |
|     460 |  415 | `		EngineRelease(pEngine);` |
|     460 |  416 | `		pEngine = pNext;` |
|     460 |  417 | `		sMPGlobal.nEngine--;` |
|       4 |  418 | `	}` |
|       - |  419 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  420 | `	/* Release the mutex subsystem */` |
|     460 |  421 | `	if( sMPGlobal.pMutexMethods ){` |
|     460 |  422 | `		if( sMPGlobal.pMutex ){` |
|     460 |  423 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     460 |  424 | `			sMPGlobal.pMutex = 0;` |
|     228 |  425 | `		}` |
|     460 |  426 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       4 |  427 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  428 | `		}` |
|     460 |  429 | `		sMPGlobal.pMutexMethods = 0;` |
|     228 |  430 | `	}` |
|     460 |  431 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  432 | `#endif` |
|     460 |  433 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  434 | `		/* Release the memory backend */` |
|     460 |  435 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     228 |  436 | `	}` |
|     460 |  437 | `	sMPGlobal.nMagic = 0x1928;` |
|     460 |  438 | `}` |
|       - |  439 | `/*` |
|       - |  440 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  441 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  442 | ` */` |
|     456 |  443 | `int ph7_lib_shutdown(void)` |
|       4 |  444 | `{` |
|     460 |  445 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  446 | `		/* Already shut */` |
|     ! 0 |  447 | `		return PH7_OK;` |
|       - |  448 | `	}` |
|     460 |  449 | `	PH7CoreShutdown();` |
|     460 |  450 | `	return PH7_OK;` |
|     232 |  451 | `}` |
|       - |  452 | `/*` |
|       - |  453 | ` * [CAPIREF: ph7_lib_is_threadsafe()]` |
|       - |  454 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  455 | ` */` |
|     ! 0 |  456 | `int ph7_lib_is_threadsafe(void)` |
|     ! 0 |  457 | `{` |
|     ! 0 |  458 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|     ! 0 |  459 | `		return 0;` |
|       - |  460 | `	}` |
|       - |  461 | `#if defined(PH7_ENABLE_THREADS)` |
|     ! 0 |  462 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  463 | `			/* Muli-threading support is enabled */` |
|     ! 0 |  464 | `			return 1;` |
|     ! 0 |  465 | `		}else{` |
|       - |  466 | `			/* Single-threading */` |
|     ! 0 |  467 | `			return 0;` |
|       - |  468 | `		}` |
|       - |  469 | `#else` |
|       - |  470 | `	return 0;` |
|       - |  471 | `#endif` |
|     ! 0 |  472 | `}` |
|       - |  473 | `/*` |
|       - |  474 | ` * [CAPIREF: ph7_lib_version()]` |
|       - |  475 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  476 | ` */` |
|       6 |  477 | `const char * ph7_lib_version(void)` |
|       3 |  478 | `{` |
|       9 |  479 | `	return PH7_VERSION;` |
|       3 |  480 | `}` |
|       - |  481 | `/*` |
|       - |  482 | ` * [CAPIREF: ph7_lib_signature()]` |
|       - |  483 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  484 | ` */` |
|      10 |  485 | `const char * ph7_lib_signature(void)` |
|       1 |  486 | `{` |
|      11 |  487 | `	return PH7_SIG;` |
|       1 |  488 | `}` |
|       - |  489 | `/*` |
|       - |  490 | ` * [CAPIREF: ph7_lib_ident()]` |
|       - |  491 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  492 | ` */` |
|       2 |  493 | `const char * ph7_lib_ident(void)` |
|       1 |  494 | `{` |
|       3 |  495 | `	return PH7_IDENT;` |
|       1 |  496 | `}` |
|       - |  497 | `/*` |
|       - |  498 | ` * [CAPIREF: ph7_lib_copyright()]` |
|       - |  499 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  500 | ` */` |
|     ! 0 |  501 | `const char * ph7_lib_copyright(void)` |
|     ! 0 |  502 | `{` |
|     ! 0 |  503 | `	return PH7_COPYRIGHT;` |
|     ! 0 |  504 | `}` |
|       - |  505 | `/*` |
|       - |  506 | ` * [CAPIREF: ph7_config()]` |
|       - |  507 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  508 | ` */` |
|    7630 |  509 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       5 |  510 | `{` |
|       - |  511 | `	va_list ap;` |
|       - |  512 | `	int rc;` |
|    7635 |  513 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  514 | `		return PH7_CORRUPT;` |
|       - |  515 | `	}` |
|       - |  516 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  517 | `	 /* Acquire engine mutex */` |
|    7635 |  518 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    7635 |  519 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    7630 |  520 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  521 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  522 | `	 }` |
|       - |  523 | `#endif` |
|    7635 |  524 | `	 va_start(ap,nConfigOp);` |
|    7635 |  525 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    7635 |  526 | `	 va_end(ap);` |
|       - |  527 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  528 | `	 /* Leave engine mutex */` |
|    7635 |  529 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  530 | `#endif` |
|    7635 |  531 | `	return rc;` |
|    3820 |  532 | `}` |
|       - |  533 | `/*` |
|       - |  534 | ` * [CAPIREF: ph7_init()]` |
|       - |  535 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  536 | ` */` |
|    3828 |  537 | `int ph7_init(ph7 **ppEngine)` |
|       5 |  538 | `{` |
|       - |  539 | `	ph7 *pEngine;` |
|       - |  540 | `	int rc;` |
|       - |  541 | `#if defined(UNTRUST)` |
|       - |  542 | `	if( ppEngine == 0 ){` |
|       - |  543 | `		return PH7_CORRUPT;` |
|       - |  544 | `	}` |
|       - |  545 | `#endif` |
|    3833 |  546 | `	*ppEngine = 0;` |
|       - |  547 | `	/* One-time automatic library initialization */` |
|    3833 |  548 | `	rc = PH7CoreInitialize();` |
|    3833 |  549 | `	if( rc != PH7_OK ){` |
|     ! 0 |  550 | `		return rc;` |
|       - |  551 | `	}` |
|       - |  552 | `	/* Allocate a new engine */` |
|    3833 |  553 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    3833 |  554 | `	if( pEngine == 0 ){` |
|     ! 0 |  555 | `		return PH7_NOMEM;` |
|       - |  556 | `	}` |
|       - |  557 | `	/* Zero the structure */` |
|    3833 |  558 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  559 | `	/* Initialize engine fields */` |
|    3833 |  560 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    3833 |  561 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    3833 |  562 | `	if( rc != PH7_OK ){` |
|     ! 0 |  563 | `		goto Release;` |
|       - |  564 | `	}` |
|       - |  565 | `#if defined(PH7_ENABLE_THREADS)` |
|    3833 |  566 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  567 | `#endif` |
|       - |  568 | `	/* Default configuration */` |
|    3833 |  569 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  570 | `	/* Install a default compile-time error consumer routine */` |
|    3833 |  571 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  572 | `	/* Built-in vfs */` |
|    3833 |  573 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  574 | `#if defined(PH7_ENABLE_THREADS)` |
|    3833 |  575 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  576 | `		 /* Associate a recursive mutex with this instance */` |
|    3833 |  577 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3833 |  578 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  579 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  580 | `			 goto Release;` |
|       - |  581 | `		 }` |
|    1914 |  582 | `	 }` |
|       - |  583 | `#endif` |
|       - |  584 | `	/* Link to the list of active engines */` |
|       - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  586 | `	/* Enter the global mutex */` |
|    3833 |  587 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  588 | `#endif` |
|    3833 |  589 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    3833 |  590 | `	sMPGlobal.nEngine++;` |
|       - |  591 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  592 | `	/* Leave the global mutex */` |
|    3833 |  593 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  594 | `#endif` |
|       - |  595 | `	/* Write a pointer to the new instance */` |
|    3833 |  596 | `	*ppEngine = pEngine;` |
|    3833 |  597 | `	return PH7_OK;` |
|     ! 0 |  598 | `Release:` |
|     ! 0 |  599 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  600 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  601 | `	return rc;` |
|    1919 |  602 | `}` |
|       - |  603 | `/*` |
|       - |  604 | ` * [CAPIREF: ph7_release()]` |
|       - |  605 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  606 | ` */` |
|    3378 |  607 | `int ph7_release(ph7 *pEngine)` |
|       5 |  608 | `{` |
|       - |  609 | `	int rc;` |
|    3383 |  610 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  611 | `		return PH7_CORRUPT;` |
|       - |  612 | `	}` |
|       - |  613 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  614 | `	 /* Acquire engine mutex */` |
|    3383 |  615 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3383 |  616 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3378 |  617 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  618 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  619 | `	 }` |
|       - |  620 | `#endif` |
|       - |  621 | `	/* Release the engine */` |
|    3383 |  622 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  623 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  624 | `	 /* Leave engine mutex */` |
|    3383 |  625 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  626 | `	 /* Release engine mutex */` |
|    3383 |  627 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  628 | `#endif` |
|       - |  629 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  630 | `	/* Enter the global mutex */` |
|    3383 |  631 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  632 | `#endif` |
|       - |  633 | `	/* Unlink from the list of active engines */` |
|    3383 |  634 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    3383 |  635 | `	sMPGlobal.nEngine--;` |
|       - |  636 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  637 | `	/* Leave the global mutex */` |
|    3383 |  638 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  639 | `#endif` |
|       - |  640 | `	/* Release the memory chunk allocated to this engine */` |
|    3383 |  641 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    3383 |  642 | `	return rc;` |
|    1694 |  643 | `}` |
|       - |  644 | `/*` |
|       - |  645 | ` * Compile a raw PHP script.` |
|       - |  646 | ` * To execute a PHP code, it must first be compiled into a byte-code program using this routine.` |
|       - |  647 | ` * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]` |
|       - |  648 | ` * should  display the appropriate error message and this function set ppVm to null and return` |
|       - |  649 | ` * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled` |
|       - |  650 | ` * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].` |
|       - |  651 | ` * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script` |
|       - |  652 | ` * for evaluation.` |
|       - |  653 | ` */` |
|    3826 |  654 | `static sxi32 ProcessScript(` |
|       - |  655 | `	ph7 *pEngine,          /* Running PH7 engine */` |
|       - |  656 | `	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */` |
|       - |  657 | `	SyString *pScript,     /* Raw PHP script to compile */` |
|       - |  658 | `	sxi32 iFlags,          /* Compile-time flags */` |
|       - |  659 | `	const char *zFilePath  /* File path if script come from a file. NULL otherwise */` |
|       - |  660 | `	)` |
|       5 |  661 | `{` |
|       - |  662 | `	ph7_vm *pVm;` |
|       - |  663 | `	int rc;` |
|       - |  664 | `	/* Allocate a new virtual machine */` |
|    3831 |  665 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    3831 |  666 | `	if( pVm == 0 ){` |
|       - |  667 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  668 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  669 | `		if( ppVm ){` |
|     ! 0 |  670 | `			*ppVm = 0;` |
|     ! 0 |  671 | `		}` |
|     ! 0 |  672 | `		return PH7_NOMEM;` |
|       - |  673 | `	}` |
|    3831 |  674 | `	if( iFlags < 0 ){` |
|       - |  675 | `		/* Default compile-time flags */` |
|     ! 0 |  676 | `		iFlags = 0;` |
|     ! 0 |  677 | `	}` |
|       - |  678 | `	/* Initialize the Virtual Machine */` |
|    3831 |  679 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    3831 |  680 | `	if( rc != PH7_OK ){` |
|     ! 0 |  681 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  682 | `		if( ppVm ){` |
|     ! 0 |  683 | `			*ppVm = 0;` |
|     ! 0 |  684 | `		}` |
|     ! 0 |  685 | `		return PH7_VM_ERR;` |
|       - |  686 | `	}` |
|    3831 |  687 | `	if( zFilePath ){` |
|       - |  688 | `		/* Push processed file path */` |
|    3809 |  689 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    1907 |  690 | `	}else{` |
|       - |  691 | `		/* Anonymous source (phl -r / an embedder snippet): php names it` |
|       - |  692 | `		 * "Command line code" in every diagnostic location suffix. */` |
|      25 |  693 | `		PH7_VmPushFilePath(pVm,"Command line code",-1,TRUE,0);` |
|       - |  694 | `	}` |
|       - |  695 | `	/* Reset the error message consumer */` |
|    3831 |  696 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  697 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|       - |  698 | `	{` |
|    3831 |  699 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|    3831 |  700 | `		if( SyStringLength(pScript) > nLimit ){` |
|     ! 0 |  701 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|       - |  702 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|     ! 0 |  703 | `				SyStringLength(pScript),nLimit);` |
|     ! 0 |  704 | `		}` |
|       - |  705 | `	}` |
|       - |  706 | `	/* Compile the script */` |
|    3831 |  707 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|    3831 |  708 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    1913 |  709 | `	}` |
|    3831 |  710 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     460 |  711 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  712 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     460 |  713 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     460 |  714 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     460 |  715 | `		if( ppVm ){` |
|     460 |  716 | `			*ppVm = 0;` |
|     228 |  717 | `		}` |
|     460 |  718 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  719 | `	}` |
|       - |  720 | `	/* Prepare the virtual machine for bytecode execution */` |
|    3375 |  721 | `	rc = PH7_VmMakeReady(pVm);` |
|    3375 |  722 | `	if( rc != PH7_OK ){` |
|       3 |  723 | `		goto Release;` |
|       - |  724 | `	}` |
|       - |  725 | `	/* Install local import path which is the current directory */` |
|    3373 |  726 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  727 | `#if defined(PH7_ENABLE_THREADS)` |
|    3373 |  728 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  729 | `		 /* Associate a recursive mutex with this instance */` |
|    3373 |  730 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3373 |  731 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  732 | `			 goto Release;` |
|       - |  733 | `		 }` |
|    1684 |  734 | `	 }` |
|       - |  735 | `#endif` |
|       - |  736 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    3373 |  737 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    3373 |  738 | `	pEngine->iVm++;` |
|       - |  739 | `	/* Point to the freshly created VM */` |
|    3373 |  740 | `	*ppVm = pVm;` |
|       - |  741 | `	/* Ready to execute PH7 bytecode */` |
|    3373 |  742 | `	return PH7_OK;` |
|       1 |  743 | `Release:` |
|       - |  744 | `	{` |
|       - |  745 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|       - |  746 | `		 * typed class constant whose value violates its declared type) is a compile` |
|       - |  747 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|       - |  748 | `		 * Captured before the releases free the VM. */` |
|       3 |  749 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|       3 |  750 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|       3 |  751 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       3 |  752 | `		*ppVm = 0;` |
|       3 |  753 | `		return rcRet;` |
|       - |  754 | `	}` |
|    1918 |  755 | `}` |
|       - |  756 | `/*` |
|       - |  757 | ` * [CAPIREF: ph7_compile()]` |
|       - |  758 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  759 | ` */` |
|     ! 0 |  760 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|     ! 0 |  761 | `{` |
|       - |  762 | `	SyString sScript;` |
|       - |  763 | `	int rc;` |
|     ! 0 |  764 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  765 | `		return PH7_CORRUPT;` |
|       - |  766 | `	}` |
|     ! 0 |  767 | `	if( nLen < 0 ){` |
|       - |  768 | `		/* Compute input length automatically */` |
|     ! 0 |  769 | `		nLen = (int)SyStrlen(zSource);` |
|     ! 0 |  770 | `	}` |
|     ! 0 |  771 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  772 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  773 | `	 /* Acquire engine mutex */` |
|     ! 0 |  774 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  775 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  776 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  777 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  778 | `	 }` |
|       - |  779 | `#endif` |
|       - |  780 | `	/* Compile the script */` |
|     ! 0 |  781 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|       - |  782 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  783 | `	 /* Leave engine mutex */` |
|     ! 0 |  784 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  785 | `#endif` |
|       - |  786 | `	/* Compilation result */` |
|     ! 0 |  787 | `	return rc;` |
|     ! 0 |  788 | `}` |
|       - |  789 | `/*` |
|       - |  790 | ` * [CAPIREF: ph7_compile_v2()]` |
|       - |  791 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  792 | ` */` |
|      22 |  793 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       3 |  794 | `{` |
|       - |  795 | `	SyString sScript;` |
|       - |  796 | `	int rc;` |
|      25 |  797 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  798 | `		return PH7_CORRUPT;` |
|       - |  799 | `	}` |
|      25 |  800 | `	if( nLen < 0 ){` |
|       - |  801 | `		/* Compute input length automatically */` |
|      25 |  802 | `		nLen = (int)SyStrlen(zSource);` |
|      11 |  803 | `	}` |
|      25 |  804 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  805 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  806 | `	 /* Acquire engine mutex */` |
|      25 |  807 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      25 |  808 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      22 |  809 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  810 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  811 | `	 }` |
|       - |  812 | `#endif` |
|       - |  813 | `	/* Compile the script */` |
|      25 |  814 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  815 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  816 | `	 /* Leave engine mutex */` |
|      25 |  817 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  818 | `#endif` |
|       - |  819 | `	/* Compilation result */` |
|      25 |  820 | `	return rc;` |
|      14 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  824 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  825 | ` */` |
|    3804 |  826 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       5 |  827 | `{` |
|       - |  828 | `	const ph7_vfs *pVfs;` |
|       - |  829 | `	int rc;` |
|    3809 |  830 | `	if( ppOutVm ){` |
|    3809 |  831 | `		*ppOutVm = 0;` |
|    1902 |  832 | `	}` |
|    3809 |  833 | `	rc = PH7_OK; /* cc warning */` |
|    3809 |  834 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  835 | `		return PH7_CORRUPT;` |
|       - |  836 | `	}` |
|       - |  837 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  838 | `	 /* Acquire engine mutex */` |
|    3809 |  839 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3809 |  840 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3804 |  841 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  842 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  843 | `	 }` |
|       - |  844 | `#endif` |
|       - |  845 | `	 /*` |
|       - |  846 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  847 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  848 | `	  */` |
|    3809 |  849 | `	 pVfs = pEngine->pVfs;` |
|    3809 |  850 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  851 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  852 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  853 | `	 }else{` |
|    3809 |  854 | `		 void *pMapView = 0; /* cc warning */` |
|    3809 |  855 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  856 | `		 SyString sScript;` |
|       - |  857 | `		 /* Try to get a memory view of the whole file */` |
|    3809 |  858 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    3809 |  859 | `		 if( rc != PH7_OK ){` |
|       - |  860 | `			 /* Assume an IO error */` |
|     ! 0 |  861 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  862 | `		 }else{` |
|       - |  863 | `			 /* Compile the file */` |
|    3809 |  864 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    3809 |  865 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  866 | `			 /* Release the memory view of the whole file */` |
|    3809 |  867 | `			 if( pVfs->xUnmap ){` |
|    3809 |  868 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    1902 |  869 | `			 }` |
|       - |  870 | `		 }` |
|       - |  871 | `	 }` |
|       - |  872 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  873 | `	 /* Leave engine mutex */` |
|    3809 |  874 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  875 | `#endif` |
|       - |  876 | `	/* Compilation result */` |
|    3809 |  877 | `	return rc;` |
|    1907 |  878 | `}` |
|       - |  879 | `/*` |
|       - |  880 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|       - |  881 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  882 | ` */` |
|       2 |  883 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|       1 |  884 | `{` |
|       - |  885 | `	int rc;` |
|       - |  886 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       3 |  887 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  888 | `		return PH7_CORRUPT;` |
|       - |  889 | `	}` |
|       - |  890 | `#ifdef UNTRUST` |
|       - |  891 | `	if( xConsumer == 0 ){` |
|       - |  892 | `		return PH7_CORRUPT;` |
|       - |  893 | `	}` |
|       - |  894 | `#endif` |
|       - |  895 | `	/* Dump VM instructions */` |
|       3 |  896 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|       3 |  897 | `	return rc;` |
|       2 |  898 | `}` |
|       - |  899 | `/*` |
|       - |  900 | ` * [CAPIREF: ph7_vm_config()]` |
|       - |  901 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  902 | ` */` |
|   88158 |  903 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       5 |  904 | `{` |
|       - |  905 | `	va_list ap;` |
|       - |  906 | `	int rc;` |
|       - |  907 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   88163 |  908 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  909 | `		return PH7_CORRUPT;` |
|       - |  910 | `	}` |
|       - |  911 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  912 | `	 /* Acquire VM mutex */` |
|   88163 |  913 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   88163 |  914 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   88158 |  915 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  916 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  917 | `	 }` |
|       - |  918 | `#endif` |
|       - |  919 | `	/* Confiugure the virtual machine */` |
|   88163 |  920 | `	va_start(ap,iConfigOp);` |
|   88163 |  921 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   88163 |  922 | `	va_end(ap);` |
|       - |  923 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  924 | `	 /* Leave VM mutex */` |
|   88163 |  925 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  926 | `#endif` |
|   88163 |  927 | `	return rc;` |
|   44084 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  931 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  932 | ` */` |
|    3374 |  933 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       5 |  934 | `{` |
|       - |  935 | `	int rc;` |
|       - |  936 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    3379 |  937 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  938 | `		return PH7_CORRUPT;` |
|       - |  939 | `	}` |
|       - |  940 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  941 | `	 /* Acquire VM mutex */` |
|    3379 |  942 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3379 |  943 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3374 |  944 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  945 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  946 | `	 }` |
|       - |  947 | `#endif` |
|       - |  948 | `	/* Execute PH7 byte-code */` |
|    3379 |  949 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    3379 |  950 | `	if( pExitStatus ){` |
|       - |  951 | `		/* Exit status */` |
|    3347 |  952 | `		*pExitStatus = pVm->iExitStatus;` |
|    1671 |  953 | `	}` |
|       - |  954 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  955 | `	 /* Leave VM mutex */` |
|    3379 |  956 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  957 | `#endif` |
|       - |  958 | `	/* Execution result */` |
|    3379 |  959 | `	return rc;` |
|    1692 |  960 | `}` |
|       - |  961 | `/*` |
|       - |  962 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  963 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  964 | ` */` |
|       8 |  965 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  966 | `{` |
|       - |  967 | `	int rc;` |
|       - |  968 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       8 |  969 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  970 | `		return PH7_CORRUPT;` |
|       - |  971 | `	}` |
|       - |  972 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  973 | `	 /* Acquire VM mutex */` |
|       8 |  974 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       8 |  975 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       8 |  976 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  977 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  978 | `	 }` |
|       - |  979 | `#endif` |
|       8 |  980 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  981 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  982 | `	 /* Leave VM mutex */` |
|       8 |  983 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  984 | `#endif` |
|       8 |  985 | `	return rc;` |
|       4 |  986 | `}` |
|       - |  987 | `/*` |
|       - |  988 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  990 | ` */` |
|    3368 |  991 | `int ph7_vm_release(ph7_vm *pVm)` |
|       5 |  992 | `{` |
|       - |  993 | `	ph7 *pEngine;` |
|       - |  994 | `	int rc;` |
|       - |  995 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    3373 |  996 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  997 | `		return PH7_CORRUPT;` |
|       - |  998 | `	}` |
|       - |  999 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1000 | `	 /* Acquire VM mutex */` |
|    3373 | 1001 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3373 | 1002 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3368 | 1003 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1004 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1005 | `	 }` |
|       - | 1006 | `#endif` |
|    3373 | 1007 | `	pEngine = pVm->pEngine;` |
|    3373 | 1008 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - | 1009 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1010 | `	 /* Leave VM mutex */` |
|    3373 | 1011 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3373 | 1012 | `	 if( rc == PH7_OK && pVm->pMutex ){` |
|       - | 1013 | `		 /* The per-VM mutex was allocated in ProcessScript and never freed — one` |
|       - | 1014 | `		  * leak per VM, which LeakSanitizer reports on every single run. */` |
|    3373 | 1015 | `		 SyMutexRelease(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|    3373 | 1016 | `		 pVm->pMutex = 0;` |
|    1684 | 1017 | `	 }` |
|       - | 1018 | `#endif` |
|    3373 | 1019 | `	if( rc == PH7_OK ){` |
|       - | 1020 | `		/* Unlink from the list of active VM */` |
|       - | 1021 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1022 | `			/* Acquire engine mutex */` |
|    3373 | 1023 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3373 | 1024 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3368 | 1025 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 | 1026 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1027 | `			}` |
|       - | 1028 | `#endif` |
|    3373 | 1029 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    3373 | 1030 | `		pEngine->iVm--;` |
|       - | 1031 | `		/* Release the memory chunk allocated to this VM */` |
|    3373 | 1032 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - | 1033 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1034 | `			/* Leave engine mutex */` |
|    3373 | 1035 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1036 | `#endif` |
|    1684 | 1037 | `	}` |
|    3373 | 1038 | `	return rc;` |
|    1689 | 1039 | `}` |
|       - | 1040 | `/*` |
|       - | 1041 | ` * [CAPIREF: ph7_create_function()]` |
|       - | 1042 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1043 | ` */` |
| 2081236 | 1044 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       5 | 1045 | `{` |
|       - | 1046 | `	SyString sName;` |
|       - | 1047 | `	int rc;` |
|       - | 1048 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 2081241 | 1049 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1050 | `		return PH7_CORRUPT;` |
|       - | 1051 | `	}` |
| 2081241 | 1052 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1053 | `	/* Remove leading and trailing white spaces */` |
| 2081241 | 1054 | `	SyStringFullTrim(&sName);` |
|       - | 1055 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 2081241 | 1056 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1057 | `		return PH7_CORRUPT;` |
|       - | 1058 | `	}` |
|       - | 1059 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1060 | `	 /* Acquire VM mutex */` |
| 2081241 | 1061 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 2081241 | 1062 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 2081236 | 1063 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1064 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1065 | `	 }` |
|       - | 1066 | `#endif` |
|       - | 1067 | `	/* Install the foreign function */` |
| 2081241 | 1068 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1069 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1070 | `	 /* Leave VM mutex */` |
| 2081241 | 1071 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1072 | `#endif` |
| 2081241 | 1073 | `	return rc;` |
| 1040623 | 1074 | `}` |
|       - | 1075 | `/*` |
|       - | 1076 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1077 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1078 | ` */` |
|     ! 0 | 1079 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1080 | `{` |
|     ! 0 | 1081 | `	ph7_user_func *pFunc = 0;` |
|       - | 1082 | `	int rc;` |
|       - | 1083 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1084 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1085 | `		return PH7_CORRUPT;` |
|       - | 1086 | `	}` |
|       - | 1087 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1088 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1089 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1090 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1091 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1092 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1093 | `	 }` |
|       - | 1094 | `#endif` |
|       - | 1095 | `	/* Perform the deletion */` |
|     ! 0 | 1096 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1097 | `	if( rc == PH7_OK ){` |
|       - | 1098 | `		/* Release internal fields */` |
|     ! 0 | 1099 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1100 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1101 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1102 | `	}` |
|       - | 1103 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1104 | `	 /* Leave VM mutex */` |
|     ! 0 | 1105 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1106 | `#endif` |
|     ! 0 | 1107 | `	return rc;` |
|     ! 0 | 1108 | `}` |
|       - | 1109 | `/*` |
|       - | 1110 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1111 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1112 | ` */` |
|  963816 | 1113 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       5 | 1114 | `{` |
|       - | 1115 | `	SyString sName;` |
|       - | 1116 | `	int rc;` |
|       - | 1117 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  963821 | 1118 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1119 | `		return PH7_CORRUPT;` |
|       - | 1120 | `	}` |
|  963821 | 1121 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1122 | `	/* Remove leading and trailing white spaces */` |
|  967191 | 1123 | `	SyStringFullTrim(&sName);` |
|  963821 | 1124 | `	if( sName.nByte < 1 ){` |
|       - | 1125 | `		/* Empty constant name */` |
|     ! 0 | 1126 | `		return PH7_CORRUPT;` |
|       - | 1127 | `	}` |
|       - | 1128 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  963821 | 1129 | `	if( xExpand == 0 ){` |
|     ! 0 | 1130 | `		return PH7_CORRUPT;` |
|       - | 1131 | `	}` |
|       - | 1132 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1133 | `	 /* Acquire VM mutex */` |
|  963821 | 1134 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  963821 | 1135 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  963816 | 1136 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1137 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1138 | `	 }` |
|       - | 1139 | `#endif` |
|       - | 1140 | `	/* Perform the registration */` |
|  963821 | 1141 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1142 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1143 | `	 /* Leave VM mutex */` |
|  963821 | 1144 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1145 | `#endif` |
|  963821 | 1146 | `	 return rc;` |
|  481913 | 1147 | `}` |
|       - | 1148 | `/*` |
|       - | 1149 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1150 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1151 | ` */` |
|     ! 0 | 1152 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1153 | `{` |
|       - | 1154 | `	ph7_constant *pCons;` |
|       - | 1155 | `	int rc;` |
|       - | 1156 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1157 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1158 | `		return PH7_CORRUPT;` |
|       - | 1159 | `	}` |
|       - | 1160 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1161 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1162 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1163 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1164 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1165 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1166 | `	 }` |
|       - | 1167 | `#endif` |
|       - | 1168 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1169 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1170 | `	 if( rc == PH7_OK ){` |
|       - | 1171 | `		 /* Perform the deletion */` |
|     ! 0 | 1172 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1173 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1174 | `	 }` |
|       - | 1175 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1176 | `	 /* Leave VM mutex */` |
|     ! 0 | 1177 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1178 | `#endif` |
|     ! 0 | 1179 | `	return rc;` |
|     ! 0 | 1180 | `}` |
|       - | 1181 | `/*` |
|       - | 1182 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1183 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1184 | ` */` |
|   41728 | 1185 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       5 | 1186 | `{` |
|       - | 1187 | `	ph7_value *pObj;` |
|       - | 1188 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   41733 | 1189 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1190 | `		return 0;` |
|       - | 1191 | `	}` |
|       - | 1192 | `	/* Allocate a new scalar variable */` |
|   41733 | 1193 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   41733 | 1194 | `	if( pObj == 0 ){` |
|     ! 0 | 1195 | `		return 0;` |
|       - | 1196 | `	}` |
|       - | 1197 | `	/* Nullify the new scalar */` |
|   41733 | 1198 | `	PH7_MemObjInit(pVm,pObj);` |
|   41733 | 1199 | `	return pObj;` |
|   20869 | 1200 | `}` |
|       - | 1201 | `/*` |
|       - | 1202 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1203 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1204 | ` */` |
|   56344 | 1205 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       5 | 1206 | `{` |
|       - | 1207 | `	ph7_hashmap *pMap;` |
|       - | 1208 | `	ph7_value *pObj;` |
|       - | 1209 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   56349 | 1210 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1211 | `		return 0;` |
|       - | 1212 | `	}` |
|       - | 1213 | `	/* Create a new hashmap first */` |
|   56349 | 1214 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   56349 | 1215 | `	if( pMap == 0 ){` |
|     ! 0 | 1216 | `		return 0;` |
|       - | 1217 | `	}` |
|       - | 1218 | `	/* Associate a new ph7_value with this hashmap */` |
|   56349 | 1219 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   56349 | 1220 | `	if( pObj == 0 ){` |
|     ! 0 | 1221 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1222 | `		return 0;` |
|       - | 1223 | `	}` |
|   56349 | 1224 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   56349 | 1225 | `	return pObj;` |
|   28177 | 1226 | `}` |
|       - | 1227 | `/*` |
|       - | 1228 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1229 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1230 | ` */` |
|   42706 | 1231 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1232 | `{` |
|       - | 1233 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   42711 | 1234 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1235 | `		return PH7_CORRUPT;` |
|       - | 1236 | `	}` |
|   42711 | 1237 | `	if( pValue ){` |
|       - | 1238 | `		/* Release the value */` |
|   42711 | 1239 | `		PH7_MemObjRelease(pValue);` |
|   42711 | 1240 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|   21353 | 1241 | `	}` |
|   42711 | 1242 | `	return PH7_OK;` |
|   21358 | 1243 | `}` |
|       - | 1244 | `/*` |
|       - | 1245 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1246 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1247 | ` */` |
|   23320 | 1248 | `int ph7_value_to_int(ph7_value *pValue)` |
|       5 | 1249 | `{` |
|       - | 1250 | `	int rc;` |
|   23325 | 1251 | `	rc = PH7_MemObjToInteger(pValue);` |
|   23325 | 1252 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1253 | `		return 0;` |
|       - | 1254 | `	}` |
|   23325 | 1255 | `	return (int)pValue->x.iVal;` |
|   11665 | 1256 | `}` |
|       - | 1257 | `/*` |
|       - | 1258 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1259 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1260 | ` */` |
|    2206 | 1261 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       5 | 1262 | `{` |
|       - | 1263 | `	int rc;` |
|    2211 | 1264 | `	rc = PH7_MemObjToBool(pValue);` |
|    2211 | 1265 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1266 | `		return 0;` |
|       - | 1267 | `	}` |
|    2211 | 1268 | `	return (int)pValue->x.iVal;` |
|    1108 | 1269 | `}` |
|       - | 1270 | `/*` |
|       - | 1271 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1272 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1273 | ` */` |
|  502142 | 1274 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       5 | 1275 | `{` |
|       - | 1276 | `	int rc;` |
|  502147 | 1277 | `	rc = PH7_MemObjToInteger(pValue);` |
|  502147 | 1278 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1279 | `		return 0;` |
|       - | 1280 | `	}` |
|  502147 | 1281 | `	return pValue->x.iVal;` |
|  251076 | 1282 | `}` |
|       - | 1283 | `/*` |
|       - | 1284 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1285 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1286 | ` */` |
|    1298 | 1287 | `double ph7_value_to_double(ph7_value *pValue)` |
|       4 | 1288 | `{` |
|       - | 1289 | `	int rc;` |
|    1302 | 1290 | `	rc = PH7_MemObjToReal(pValue);` |
|    1302 | 1291 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1292 | `		return (double)0;` |
|       - | 1293 | `	}` |
|    1302 | 1294 | `	return (double)pValue->rVal;` |
|     653 | 1295 | `}` |
|       - | 1296 | `/*` |
|       - | 1297 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1298 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1299 | ` */` |
|  931661 | 1300 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       5 | 1301 | `{` |
|  931666 | 1302 | `	PH7_MemObjToString(pValue);` |
|  931666 | 1303 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  897490 | 1304 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  897490 | 1305 | `		if( pLen ){` |
|  833706 | 1306 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  416874 | 1307 | `		}` |
|  897490 | 1308 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1309 | `	}else{` |
|       - | 1310 | `		/* Return the empty string */` |
|   34181 | 1311 | `		if( pLen ){` |
|   34171 | 1312 | `			*pLen = 0;` |
|   17083 | 1313 | `		}` |
|   34181 | 1314 | `		return "";` |
|       - | 1315 | `	}` |
|  465859 | 1316 | `}` |
|       - | 1317 | `/*` |
|       - | 1318 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1319 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1320 | ` */` |
|   31710 | 1321 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       5 | 1322 | `{` |
|   31715 | 1323 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1324 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1325 | `		return 0;` |
|       - | 1326 | `	}` |
|   31715 | 1327 | `	return pValue->x.pOther;` |
|   15860 | 1328 | `}` |
|       - | 1329 | `/*` |
|       - | 1330 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1331 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1332 | ` */` |
|      64 | 1333 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1334 | `{` |
|       - | 1335 | `	int rc;` |
|      65 | 1336 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1337 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1338 | `		return 1;` |
|       - | 1339 | `	}` |
|       - | 1340 | `	/* Perform the comparison */` |
|      65 | 1341 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1342 | `	/* Comparison result */` |
|      65 | 1343 | `	return rc;` |
|      33 | 1344 | `}` |
|       - | 1345 | `/*` |
|       - | 1346 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1347 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1348 | ` */` |
|   82244 | 1349 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       5 | 1350 | `{` |
|   82249 | 1351 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       5 | 1352 | `}` |
|       - | 1353 | `/*` |
|       - | 1354 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1355 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1356 | ` */` |
|   20758 | 1357 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       5 | 1358 | `{` |
|   20763 | 1359 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       5 | 1360 | `}` |
|       - | 1361 | `/*` |
|       - | 1362 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1363 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1364 | ` */` |
|  372199 | 1365 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       5 | 1366 | `{` |
|  372204 | 1367 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       5 | 1368 | `}` |
|       - | 1369 | `/*` |
|       - | 1370 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1371 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1372 | ` */` |
|     650 | 1373 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       4 | 1374 | `{` |
|     654 | 1375 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       4 | 1376 | `}` |
|       - | 1377 | `/*` |
|       - | 1378 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1379 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1380 | ` */` |
|     590 | 1381 | `int ph7_result_null(ph7_context *pCtx)` |
|       5 | 1382 | `{` |
|       - | 1383 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     595 | 1384 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     595 | 1385 | `	return PH7_OK;` |
|       5 | 1386 | `}` |
|       - | 1387 | `/*` |
|       - | 1388 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1389 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1390 | ` */` |
| 1289387 | 1391 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1392 | `{` |
| 1289392 | 1393 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       5 | 1394 | `}` |
|       - | 1395 | `/*` |
|       - | 1396 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1397 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1398 | ` */` |
|    1464 | 1399 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       3 | 1400 | `{` |
|       - | 1401 | `	ph7_value *p;` |
|       - | 1402 | `	va_list ap;` |
|       - | 1403 | `	int rc;` |
|    1467 | 1404 | `	p = pCtx->pRet;` |
|    1467 | 1405 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1406 | `		/* Invalidate any prior representation */` |
|     481 | 1407 | `		PH7_MemObjRelease(p);` |
|     481 | 1408 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|     239 | 1409 | `	}` |
|       - | 1410 | `	/* Format the given string */` |
|    1467 | 1411 | `	va_start(ap,zFormat);` |
|    1467 | 1412 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|    1467 | 1413 | `	va_end(ap);` |
|    1467 | 1414 | `	return rc;` |
|       3 | 1415 | `}` |
|       - | 1416 | `/*` |
|       - | 1417 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1418 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1419 | ` */` |
|   35218 | 1420 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1421 | `{` |
|   35223 | 1422 | `	int rc = PH7_OK;` |
|   35223 | 1423 | `	if( pValue == 0 ){` |
|     ! 0 | 1424 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1425 | `	}else{` |
|   35223 | 1426 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1427 | `	}` |
|   35223 | 1428 | `	return rc;` |
|       5 | 1429 | `}` |
|       - | 1430 | `/*` |
|       - | 1431 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1432 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1433 | ` */` |
|    5304 | 1434 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       5 | 1435 | `{` |
|    5309 | 1436 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       5 | 1437 | `}` |
|       - | 1438 | `/*` |
|       - | 1439 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1440 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1441 | ` */` |
|   37400 | 1442 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       5 | 1443 | `{` |
|       - | 1444 | `	ph7_value *pVal;` |
|   37405 | 1445 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|   37405 | 1446 | `	if( pVal ){` |
|       - | 1447 | `		/* Record value address so it can be freed automatically` |
|       - | 1448 | `		 * when the calling function returns.` |
|       - | 1449 | `		 */` |
|   37405 | 1450 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   18700 | 1451 | `	}` |
|   37405 | 1452 | `	return pVal;` |
|       5 | 1453 | `}` |
|       - | 1454 | `/*` |
|       - | 1455 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1456 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1457 | ` */` |
|   17966 | 1458 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       5 | 1459 | `{` |
|       - | 1460 | `	ph7_value *pVal;` |
|   17971 | 1461 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   17971 | 1462 | `	if( pVal ){` |
|       - | 1463 | `		/* Record value address so it can be freed automatically` |
|       - | 1464 | `		 * when the calling function returns.` |
|       - | 1465 | `		 */` |
|   17971 | 1466 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    8983 | 1467 | `	}` |
|   17971 | 1468 | `	return pVal;` |
|       5 | 1469 | `}` |
|       - | 1470 | `/*` |
|       - | 1471 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1472 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1473 | ` */` |
|     828 | 1474 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1475 | `{` |
|     833 | 1476 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     833 | 1477 | `}` |
|       - | 1478 | `/*` |
|       - | 1479 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1480 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1481 | ` */` |
|    5508 | 1482 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       5 | 1483 | `{` |
|       - | 1484 | `	void *pChunk;` |
|    5513 | 1485 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    5513 | 1486 | `	if( pChunk ){` |
|    5513 | 1487 | `		if( ZeroChunk ){` |
|       - | 1488 | `			/* Zero the memory chunk */` |
|    5275 | 1489 | `			SyZero(pChunk,nByte);` |
|    2635 | 1490 | `		}` |
|    5513 | 1491 | `		if( AutoRelease ){` |
|       - | 1492 | `			ph7_aux_data sAux;` |
|       - | 1493 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1494 | `			 * upon this context is destroyed.` |
|       - | 1495 | `			 */` |
|     215 | 1496 | `			sAux.pAuxData = pChunk;` |
|     215 | 1497 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|     105 | 1498 | `		}` |
|    2754 | 1499 | `	}` |
|    5513 | 1500 | `	return pChunk;` |
|       5 | 1501 | `}` |
|       - | 1502 | `/*` |
|       - | 1503 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1504 | ` * chunk container.` |
|       - | 1505 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1506 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1507 | ` */` |
|    5252 | 1508 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1509 | `{` |
|       - | 1510 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1511 | `	sxu32 n;` |
|    5257 | 1512 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1513 | `		/* Don't bother processing,the container is empty */` |
|    5257 | 1514 | `		return 0;` |
|       - | 1515 | `	}` |
|       - | 1516 | `	/* Perform the lookup */` |
|     ! 0 | 1517 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1518 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1519 | `		pAux = &aAux[n];` |
|     ! 0 | 1520 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1521 | `			/* Chunk found */` |
|     ! 0 | 1522 | `			return pAux;` |
|       - | 1523 | `		}` |
|     ! 0 | 1524 | `	}` |
|       - | 1525 | `	/* No such allocated chunk */` |
|     ! 0 | 1526 | `	return 0;` |
|    2631 | 1527 | `}` |
|       - | 1528 | `/*` |
|       - | 1529 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1530 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1531 | ` */` |
|     ! 0 | 1532 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1533 | `{` |
|       - | 1534 | `	ph7_aux_data *pAux;` |
|       - | 1535 | `	void *pNew;` |
|     ! 0 | 1536 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1537 | `	if( pNew ){` |
|     ! 0 | 1538 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1539 | `		if( pAux ){` |
|     ! 0 | 1540 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1541 | `		}` |
|     ! 0 | 1542 | `	}` |
|     ! 0 | 1543 | `	return pNew;` |
|     ! 0 | 1544 | `}` |
|       - | 1545 | `/*` |
|       - | 1546 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1547 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1548 | ` */` |
|    5252 | 1549 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1550 | `{` |
|       - | 1551 | `	ph7_aux_data *pAux;` |
|    5257 | 1552 | `	if( pChunk == 0 ){` |
|       - | 1553 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1554 | `		return;` |
|       - | 1555 | `	}` |
|    5257 | 1556 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    5257 | 1557 | `	if( pAux ){` |
|       - | 1558 | `		/* Mark as destroyed */` |
|     ! 0 | 1559 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1560 | `	}` |
|    5257 | 1561 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    2631 | 1562 | `}` |
|       - | 1563 | `/*` |
|       - | 1564 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1565 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1566 | ` */` |
|     152 | 1567 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|       3 | 1568 | `{` |
|       - | 1569 | `	ph7_hashmap_node *pNode;` |
|       - | 1570 | `	ph7_value *pValue;` |
|       - | 1571 | `	ph7_value skey;` |
|       - | 1572 | `	int rc;` |
|       - | 1573 | `	/* Make sure we are dealing with a valid hashmap */` |
|     155 | 1574 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1575 | `		return 0;` |
|       - | 1576 | `	}` |
|     155 | 1577 | `	if( nByte < 0 ){` |
|     ! 0 | 1578 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1579 | `	}` |
|       - | 1580 | `	/* Convert the key to a ph7_value  */` |
|     155 | 1581 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     155 | 1582 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1583 | `	/* Perform the lookup */` |
|     155 | 1584 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     155 | 1585 | `	PH7_MemObjRelease(&skey);` |
|     155 | 1586 | `	if( rc != PH7_OK ){` |
|       - | 1587 | `		/* No such entry */` |
|      64 | 1588 | `		return 0;` |
|       - | 1589 | `	}` |
|       - | 1590 | `	/* Extract the target value */` |
|      93 | 1591 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|      93 | 1592 | `	return pValue;` |
|      79 | 1593 | `}` |
|       - | 1594 | `/*` |
|       - | 1595 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1596 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1597 | ` */` |
|   34734 | 1598 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       5 | 1599 | `{` |
|       - | 1600 | `	int rc;` |
|   34739 | 1601 | `	if( xWalk == 0 ){` |
|     ! 0 | 1602 | `		return PH7_CORRUPT;` |
|       - | 1603 | `	}` |
|       - | 1604 | `	/* Make sure we are dealing with a valid hashmap */` |
|   34739 | 1605 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1606 | `		return PH7_CORRUPT;` |
|       - | 1607 | `	}` |
|       - | 1608 | `	/* Start the walk process */` |
|   34739 | 1609 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   34739 | 1610 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   17372 | 1611 | `}` |
|       - | 1612 | `/*` |
|       - | 1613 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1614 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1615 | ` */` |
|  608076 | 1616 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       5 | 1617 | `{` |
|       - | 1618 | `	int rc;` |
|       - | 1619 | `	/* Make sure we are dealing with a valid hashmap */` |
|  608081 | 1620 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1621 | `		return PH7_CORRUPT;` |
|       - | 1622 | `	}` |
|       - | 1623 | `	/* Perform the insertion */` |
|  608081 | 1624 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
|  608081 | 1625 | `	return rc;` |
|  304043 | 1626 | `}` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1629 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1630 | ` */` |
|   37936 | 1631 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       5 | 1632 | `{` |
|       - | 1633 | `	int rc;` |
|       - | 1634 | `	/* Make sure we are dealing with a valid hashmap */` |
|   37941 | 1635 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1636 | `		return PH7_CORRUPT;` |
|       - | 1637 | `	}` |
|       - | 1638 | `	/* Perform the insertion */` |
|   37941 | 1639 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1640 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1641 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1642 | `	}else{` |
|       - | 1643 | `		ph7_value sKey;` |
|   37941 | 1644 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|   37941 | 1645 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   37941 | 1646 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|   37941 | 1647 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1648 | `	}` |
|   37941 | 1649 | `	return rc;` |
|   18973 | 1650 | `}` |
|       - | 1651 | `/*` |
|       - | 1652 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1653 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1654 | ` */` |
| 2118208 | 1655 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       5 | 1656 | `{` |
|       - | 1657 | `	ph7_value sKey;` |
|       - | 1658 | `	int rc;` |
|       - | 1659 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2118213 | 1660 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1661 | `		return PH7_CORRUPT;` |
|       - | 1662 | `	}` |
| 2118213 | 1663 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1664 | `	/* Perform the insertion */` |
| 2118213 | 1665 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
| 2118213 | 1666 | `	PH7_MemObjRelease(&sKey);` |
| 2118213 | 1667 | `	return rc;` |
| 1059109 | 1668 | `}` |
|       - | 1669 | `/*` |
|       - | 1670 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1671 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1672 | ` */` |
|  160298 | 1673 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       5 | 1674 | `{` |
|       - | 1675 | `	ph7_hashmap *pMap;` |
|       - | 1676 | `	/* Make sure we are dealing with a valid hashmap */` |
|  160303 | 1677 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1678 | `		return 0;` |
|       - | 1679 | `	}` |
|       - | 1680 | `	/* Point to the internal representation of the hashmap */` |
|  160303 | 1681 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  160303 | 1682 | `	return pMap->nEntry;` |
|   80154 | 1683 | `}` |
|       - | 1684 | `/*` |
|       - | 1685 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1686 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1687 | ` */` |
|     ! 0 | 1688 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|     ! 0 | 1689 | `{` |
|       - | 1690 | `	int rc;` |
|     ! 0 | 1691 | `	if( xWalk == 0 ){` |
|     ! 0 | 1692 | `		return PH7_CORRUPT;` |
|       - | 1693 | `	}` |
|       - | 1694 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1695 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1696 | `		return PH7_CORRUPT;` |
|       - | 1697 | `	}` |
|       - | 1698 | `	/* Start the walk process */` |
|     ! 0 | 1699 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|     ! 0 | 1700 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|     ! 0 | 1701 | `}` |
|       - | 1702 | `/*` |
|       - | 1703 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1704 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1705 | ` */` |
|       8 | 1706 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1707 | `{` |
|       - | 1708 | `	ph7_value *pValue;` |
|       - | 1709 | `	SyString sAttr;` |
|       - | 1710 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1711 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1712 | `		return 0;` |
|       - | 1713 | `	}` |
|       9 | 1714 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1715 | `	/* Extract the attribute value if available.` |
|       - | 1716 | `	 */` |
|       9 | 1717 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1718 | `	return pValue;` |
|       5 | 1719 | `}` |
|       - | 1720 | `/*` |
|       - | 1721 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1722 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1723 | ` */` |
|     ! 0 | 1724 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1725 | `{` |
|       - | 1726 | `	ph7_class *pClass;` |
|     ! 0 | 1727 | `	if( pLength ){` |
|     ! 0 | 1728 | `		*pLength = 0;` |
|     ! 0 | 1729 | `	}` |
|       - | 1730 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1731 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1732 | `		return 0;` |
|       - | 1733 | `	}` |
|       - | 1734 | `	/* Point to the class */` |
|     ! 0 | 1735 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1736 | `	/* Return the class name */` |
|     ! 0 | 1737 | `	if( pLength ){` |
|     ! 0 | 1738 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1739 | `	}` |
|     ! 0 | 1740 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1741 | `}` |
|       - | 1742 | `/*` |
|       - | 1743 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1744 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1745 | ` */` |
|    1898 | 1746 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1747 | `{` |
|       - | 1748 | `	SyString sData;` |
|       - | 1749 | `	int rc;` |
|    1903 | 1750 | `	if( nLen < 0 ){` |
|     ! 0 | 1751 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1752 | `	}` |
|    1903 | 1753 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|    1903 | 1754 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|    1903 | 1755 | `	return rc;` |
|       5 | 1756 | `}` |
|       - | 1757 | `/*` |
|       - | 1758 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1759 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1760 | ` */` |
|       2 | 1761 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1762 | `{` |
|       - | 1763 | `	va_list ap;` |
|       - | 1764 | `	int rc;` |
|       3 | 1765 | `	va_start(ap,zFormat);` |
|       3 | 1766 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1767 | `	va_end(ap);` |
|       3 | 1768 | `	return rc;` |
|       1 | 1769 | `}` |
|       - | 1770 | `/*` |
|       - | 1771 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1772 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1773 | ` */` |
|      14 | 1774 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       4 | 1775 | `{` |
|      18 | 1776 | `	int rc = PH7_OK;` |
|      18 | 1777 | `	if( zErr ){` |
|      18 | 1778 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|       7 | 1779 | `	}` |
|      18 | 1780 | `	return rc;` |
|       4 | 1781 | `}` |
|       - | 1782 | `/*` |
|       - | 1783 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1784 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1785 | ` */` |
|      42 | 1786 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       4 | 1787 | `{` |
|       - | 1788 | `	va_list ap;` |
|       - | 1789 | `	int rc;` |
|      46 | 1790 | `	if( zFormat == 0){` |
|     ! 0 | 1791 | `		return PH7_OK;` |
|       - | 1792 | `	}` |
|      46 | 1793 | `	va_start(ap,zFormat);` |
|      46 | 1794 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      46 | 1795 | `	va_end(ap);` |
|      46 | 1796 | `	return rc;` |
|      25 | 1797 | `}` |
|       - | 1798 | `/*` |
|       - | 1799 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1800 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1801 | ` */` |
|      34 | 1802 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1803 | `{` |
|       - | 1804 | `	sxu32 n;` |
|      35 | 1805 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1806 | `	return n;` |
|       1 | 1807 | `}` |
|       - | 1808 | `/*` |
|       - | 1809 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1810 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1811 | ` */` |
|     ! 0 | 1812 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1813 | `{` |
|     ! 0 | 1814 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1815 | `		return PH7_CORRUPT;` |
|       - | 1816 | `	}` |
|     ! 0 | 1817 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1818 | `	return PH7_OK;` |
|     ! 0 | 1819 | `}` |
|       - | 1820 | `/*` |
|       - | 1821 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1822 | ` *` |
|       - | 1823 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1824 | ` * {` |
|       - | 1825 | ` *	return pCtx->pVm;` |
|       - | 1826 | ` * }` |
|       - | 1827 | ` */` |
|       - | 1828 | `/*` |
|       - | 1829 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1830 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1831 | ` */` |
|   63348 | 1832 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       5 | 1833 | `{` |
|   63353 | 1834 | `	return pCtx->pFunc->pUserData;` |
|       5 | 1835 | `}` |
|       - | 1836 | `/*` |
|       - | 1837 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1838 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1839 | ` */` |
|       2 | 1840 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1841 | `{` |
|       - | 1842 | `	ph7_aux_data sAux;` |
|       - | 1843 | `	int rc;` |
|       3 | 1844 | `	sAux.pAuxData = pUserData;` |
|       3 | 1845 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1846 | `	return rc;` |
|       1 | 1847 | `}` |
|       - | 1848 | `/*` |
|       - | 1849 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1850 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1851 | ` */` |
|       4 | 1852 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1853 | `{` |
|       - | 1854 | `	ph7_aux_data *pAux;` |
|       5 | 1855 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       5 | 1856 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1857 | `}` |
|       - | 1858 | `/*` |
|       - | 1859 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1860 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1861 | ` */` |
|     ! 0 | 1862 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|     ! 0 | 1863 | `{` |
|       - | 1864 | `	ph7_aux_data *pAux;` |
|     ! 0 | 1865 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|     ! 0 | 1866 | `	return pAux ? pAux->pAuxData : 0;` |
|     ! 0 | 1867 | `}` |
|       - | 1868 | `/*` |
|       - | 1869 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1870 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1871 | ` */` |
|    6940 | 1872 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       5 | 1873 | `{` |
|    6945 | 1874 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       5 | 1875 | `}` |
|       - | 1876 | `/*` |
|       - | 1877 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1878 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1879 | ` */` |
|   50454 | 1880 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       5 | 1881 | `{` |
|       - | 1882 | `	SyString *pName;` |
|   50459 | 1883 | `	pName = &pCtx->pFunc->sName;` |
|   50459 | 1884 | `	return pName->zString;` |
|       5 | 1885 | `}` |
|       - | 1886 | `/*` |
|       - | 1887 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1888 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1889 | ` */` |
|  104616 | 1890 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       5 | 1891 | `{` |
|       - | 1892 | `	/* Invalidate any prior representation */` |
|  104621 | 1893 | `	PH7_MemObjRelease(pVal);` |
|  104621 | 1894 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|  104621 | 1895 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  104621 | 1896 | `	return PH7_OK;` |
|       5 | 1897 | `}` |
|       - | 1898 | `/*` |
|       - | 1899 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1900 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1901 | ` */` |
|  429728 | 1902 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       5 | 1903 | `{` |
|       - | 1904 | `	/* Invalidate any prior representation */` |
|  429733 | 1905 | `	PH7_MemObjRelease(pVal);` |
|  429733 | 1906 | `	pVal->x.iVal = iValue;` |
|  429733 | 1907 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  429733 | 1908 | `	return PH7_OK;` |
|       5 | 1909 | `}` |
|       - | 1910 | `/*` |
|       - | 1911 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1912 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1913 | ` */` |
|  389121 | 1914 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       5 | 1915 | `{` |
|       - | 1916 | `	/* Invalidate any prior representation */` |
|  389126 | 1917 | `	PH7_MemObjRelease(pVal);` |
|  389126 | 1918 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  389126 | 1919 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  389126 | 1920 | `	return PH7_OK;` |
|       5 | 1921 | `}` |
|       - | 1922 | `/*` |
|       - | 1923 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1924 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1925 | ` */` |
|    1602 | 1926 | `int ph7_value_null(ph7_value *pVal)` |
|       2 | 1927 | `{` |
|       - | 1928 | `	/* Invalidate any prior representation and set the NULL flag */` |
|    1604 | 1929 | `	PH7_MemObjRelease(pVal);` |
|    1604 | 1930 | `	return PH7_OK;` |
|       2 | 1931 | `}` |
|       - | 1932 | `/*` |
|       - | 1933 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1934 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1935 | ` */` |
|     966 | 1936 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       4 | 1937 | `{` |
|       - | 1938 | `	/* Invalidate any prior representation */` |
|     970 | 1939 | `	PH7_MemObjRelease(pVal);` |
|     970 | 1940 | `	pVal->rVal = (ph7_real)Value;` |
|     970 | 1941 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1942 | `	/* Try to get an integer representation also */` |
|     970 | 1943 | `	PH7_MemObjTryInteger(pVal);` |
|     970 | 1944 | `	return PH7_OK;` |
|       4 | 1945 | `}` |
|       - | 1946 | `/*` |
|       - | 1947 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1948 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1949 | ` */` |
| 1475653 | 1950 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       5 | 1951 | `{` |
| 1475658 | 1952 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1953 | `		/* Invalidate any prior representation */` |
|  478299 | 1954 | `		PH7_MemObjRelease(pVal);` |
|  478299 | 1955 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  239147 | 1956 | `	}` |
| 1475658 | 1957 | `	if( zString ){` |
| 1475238 | 1958 | `		if( nLen < 0 ){` |
|       - | 1959 | `			/* Compute length automatically */` |
|    4977 | 1960 | `			nLen = (int)SyStrlen(zString);` |
|    2486 | 1961 | `		}` |
|       - | 1962 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1963 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
| 1475238 | 1964 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1965 | `	}` |
|     421 | 1966 | `	return PH7_OK;` |
|  737831 | 1967 | `}` |
|       - | 1968 | `/*` |
|       - | 1969 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1970 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1971 | ` */` |
|      22 | 1972 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1973 | `{` |
|       - | 1974 | `	va_list ap;` |
|       - | 1975 | `	int rc;` |
|      23 | 1976 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1977 | `		/* Invalidate any prior representation */` |
|      19 | 1978 | `		PH7_MemObjRelease(pVal);` |
|      19 | 1979 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       9 | 1980 | `	}` |
|      23 | 1981 | `	va_start(ap,zFormat);` |
|      23 | 1982 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      23 | 1983 | `	va_end(ap);` |
|       - | 1984 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|      23 | 1985 | `	return rc;` |
|       1 | 1986 | `}` |
|       - | 1987 | `/*` |
|       - | 1988 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1990 | ` */` |
|  167548 | 1991 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       5 | 1992 | `{` |
|       - | 1993 | `	/* Reset the string cursor */` |
|  167553 | 1994 | `	SyBlobReset(&pVal->sBlob);` |
|  167553 | 1995 | `	return PH7_OK;` |
|       5 | 1996 | `}` |
|       - | 1997 | `/*` |
|       - | 1998 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1999 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2000 | ` */` |
|    5398 | 2001 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       5 | 2002 | `{` |
|       - | 2003 | `	/* Invalidate any prior representation */` |
|    5403 | 2004 | `	PH7_MemObjRelease(pVal);` |
|       - | 2005 | `	/* Reflect the new type */` |
|    5403 | 2006 | `	pVal->x.pOther = pUserData;` |
|    5403 | 2007 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    5403 | 2008 | `	return PH7_OK;` |
|       5 | 2009 | `}` |
|       - | 2010 | `/*` |
|       - | 2011 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 2012 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2013 | ` */` |
|    4112 | 2014 | `int ph7_value_release(ph7_value *pVal)` |
|       5 | 2015 | `{` |
|    4117 | 2016 | `	PH7_MemObjRelease(pVal);` |
|    4117 | 2017 | `	return PH7_OK;` |
|       5 | 2018 | `}` |
|       - | 2019 | `/*` |
|       - | 2020 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 2021 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2022 | ` */` |
|  509466 | 2023 | `int ph7_value_is_int(ph7_value *pVal)` |
|       5 | 2024 | `{` |
|       - | 2025 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 2026 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 2027 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 2028 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 2029 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|  509471 | 2030 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       5 | 2031 | `}` |
|       - | 2032 | `/*` |
|       - | 2033 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 2034 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2035 | ` */` |
|  503976 | 2036 | `int ph7_value_is_float(ph7_value *pVal)` |
|       5 | 2037 | `{` |
|  503981 | 2038 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       5 | 2039 | `}` |
|       - | 2040 | `/*` |
|       - | 2041 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 2042 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2043 | ` */` |
|    3822 | 2044 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       5 | 2045 | `{` |
|    3827 | 2046 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       5 | 2047 | `}` |
|       - | 2048 | `/*` |
|       - | 2049 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2050 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2051 | ` */` |
|  605502 | 2052 | `int ph7_value_is_string(ph7_value *pVal)` |
|       5 | 2053 | `{` |
|  605507 | 2054 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       5 | 2055 | `}` |
|       - | 2056 | `/*` |
|       - | 2057 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2058 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2059 | ` */` |
| 1098976 | 2060 | `int ph7_value_is_null(ph7_value *pVal)` |
|       5 | 2061 | `{` |
| 1098981 | 2062 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       5 | 2063 | `}` |
|       - | 2064 | `/*` |
|       - | 2065 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2066 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2067 | ` */` |
|    1382 | 2068 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       5 | 2069 | `{` |
|       - | 2070 | `	int rc;` |
|    1387 | 2071 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|    1387 | 2072 | `	return rc;` |
|       5 | 2073 | `}` |
|       - | 2074 | `/*` |
|       - | 2075 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2076 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2077 | ` */` |
|   60578 | 2078 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       5 | 2079 | `{` |
|       - | 2080 | `	int rc;` |
|   60583 | 2081 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   60583 | 2082 | `	return rc;` |
|       5 | 2083 | `}` |
|       - | 2084 | `/*` |
|       - | 2085 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2086 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2087 | ` */` |
|      30 | 2088 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2089 | `{` |
|      31 | 2090 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2091 | `}` |
|       - | 2092 | `/*` |
|       - | 2093 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2094 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2095 | ` */` |
|  213340 | 2096 | `int ph7_value_is_array(ph7_value *pVal)` |
|       5 | 2097 | `{` |
|  213345 | 2098 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       5 | 2099 | `}` |
|       - | 2100 | `/*` |
|       - | 2101 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2102 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2103 | ` */` |
|    8198 | 2104 | `int ph7_value_is_object(ph7_value *pVal)` |
|       5 | 2105 | `{` |
|    8203 | 2106 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       5 | 2107 | `}` |
|       - | 2108 | `/*` |
|       - | 2109 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2110 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2111 | ` */` |
|   36010 | 2112 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       5 | 2113 | `{` |
|   36015 | 2114 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       5 | 2115 | `}` |
|       - | 2116 | `/*` |
|       - | 2117 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2118 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2119 | ` */` |
|   40782 | 2120 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       5 | 2121 | `{` |
|       - | 2122 | `	int rc;` |
|   40787 | 2123 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   40787 | 2124 | `	return rc;` |
|       5 | 2125 | `}` |
|       - | 2126 | `/*` |
|       - | 2127 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2128 | ` * Check if a value holds a Fiber instance.` |
|       - | 2129 | ` */` |
|     ! 0 | 2130 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2131 | `{` |
|     ! 0 | 2132 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2133 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2134 | `}` |
|       - | 2135 | `/*` |
|       - | 2136 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2137 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2138 | ` */` |
|     ! 0 | 2139 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2140 | `{` |
|     ! 0 | 2141 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2142 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2143 | `}` |
|       - | 2144 | `/*` |
|       - | 2145 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2146 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2147 | ` */` |
|     ! 0 | 2148 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2149 | `{` |
|     ! 0 | 2150 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2151 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2152 | `}` |
|       - | 2153 | `/*` |
|       - | 2154 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2155 | ` * Check if a Fiber is currently suspended.` |
|       - | 2156 | ` */` |
|     ! 0 | 2157 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2158 | `{` |
|     ! 0 | 2159 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2160 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2161 | `}` |
|       - | 2162 | `/*` |
|       - | 2163 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2164 | ` * Check if a Fiber has completed execution.` |
|       - | 2165 | ` */` |
|     ! 0 | 2166 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2167 | `{` |
|     ! 0 | 2168 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2169 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2170 | `}` |
|       - | 2171 | `/*` |
|       - | 2172 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2173 | ` * Get the return value of a terminated Fiber.` |
|       - | 2174 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2175 | ` */` |
|     ! 0 | 2176 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2177 | `{` |
|     ! 0 | 2178 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2179 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2180 | `}` |
|       - | 2181 |  |
