# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 780/1102 lines (70.78%)

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
|   11456 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       5 |   79 | `{` |
|   11461 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|   11461 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|   11461 |   83 | `	switch(nOp){` |
|    5726 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|   11461 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|   11461 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|   11461 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|   11461 |   93 | `		pConf->xErr     = xConsumer;` |
|   11461 |   94 | `		pConf->pErrData = pUserData;` |
|   11461 |   95 | `		break;` |
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
|   11461 |  150 | `	return rc;` |
|       5 |  151 | `}` |
|       - |  152 | `/*` |
|       - |  153 | ` * Configure the PH7 library.` |
|       - |  154 | ` * return PH7_OK on success.Any other return value` |
|       - |  155 | ` * indicates failure.` |
|       - |  156 | ` * Refer to [ph7_lib_config()].` |
|       - |  157 | ` */` |
|   17232 |  158 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       5 |  159 | `{` |
|   17237 |  160 | `	int rc = PH7_OK;` |
|   17237 |  161 | `	switch(nOp){` |
|    2871 |  162 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  163 | `			/* Install a virtual file system */` |
|    5749 |  164 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    5749 |  165 | `			sMPGlobal.pVfs = pVfs;` |
|    5749 |  166 | `			break;` |
|       - |  167 | `								}` |
|    2871 |  168 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  169 | `			/* Use an alternative low-level memory allocation routines */` |
|    5749 |  170 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  171 | `			/* Save the memory failure callback (if available) */` |
|    5749 |  172 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    5749 |  173 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    5749 |  174 | `			if( pMethods == 0 ){` |
|       - |  175 | `				/* Use the built-in memory allocation subsystem */` |
|    5749 |  176 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    2878 |  177 | `			}else{` |
|     ! 0 |  178 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  179 | `			}` |
|    5749 |  180 | `			break;` |
|       - |  181 | `										  }` |
|     ! 0 |  182 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  183 | `			/* Memory failure callback */` |
|     ! 0 |  184 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  185 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  186 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  187 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  188 | `			break;` |
|       - |  189 | `												 }` |
|    2871 |  190 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  191 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  192 | `			/* Use an alternative low-level mutex subsystem */` |
|    5749 |  193 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  194 | `#if defined (UNTRUST)` |
|       - |  195 | `			if( pMethods == 0 ){` |
|       - |  196 | `				rc = PH7_CORRUPT;` |
|       - |  197 | `			}` |
|       - |  198 | `#endif` |
|       - |  199 | `			/* Sanity check */` |
|    5749 |  200 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  201 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  202 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  203 | `				break;` |
|       - |  204 | `			}` |
|    5749 |  205 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  206 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  207 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  208 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  209 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  210 | `				}` |
|     ! 0 |  211 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  212 | `			}` |
|       - |  213 | `			/* Initialize and install the new mutex subsystem */` |
|    5749 |  214 | `			if( pMethods->xGlobalInit ){` |
|       5 |  215 | `				rc = pMethods->xGlobalInit();` |
|       5 |  216 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  217 | `					break;` |
|       - |  218 | `				}` |
|     ! 0 |  219 | `			}` |
|       - |  220 | `			/* Create the global mutex */` |
|    5749 |  221 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    5749 |  222 | `			if( sMPGlobal.pMutex == 0 ){` |
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
|    5749 |  233 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    5749 |  234 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  235 | `				/* Set a default threading level */` |
|    5749 |  236 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    2873 |  237 | `			}` |
|       - |  238 | `#endif` |
|    5749 |  239 | `			break;` |
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
|   17237 |  260 | `	return rc;` |
|       5 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  264 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  265 | ` */` |
|   17232 |  266 | `int ph7_lib_config(int nConfigOp,...)` |
|       5 |  267 | `{` |
|       - |  268 | `	va_list ap;` |
|       - |  269 | `	int rc;` |
|       - |  270 |  |
|   17237 |  271 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  272 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  273 | `		return PH7_LOOKED;` |
|       - |  274 | `	}` |
|   17237 |  275 | `	va_start(ap,nConfigOp);` |
|   17237 |  276 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|   17237 |  277 | `	va_end(ap);` |
|   17237 |  278 | `	return rc;` |
|    8624 |  279 | `}` |
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
|    5744 |  290 | `static sxi32 PH7CoreInitialize(void)` |
|       5 |  291 | `{` |
|       - |  292 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  293 | `#if defined(PH7_ENABLE_THREADS)` |
|    5749 |  294 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    5749 |  295 | `	SyMutex *pMaster = 0;` |
|       - |  296 | `#endif` |
|       - |  297 | `	int rc;` |
|       - |  298 | `	/*` |
|       - |  299 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  300 | `	 * is a no-op.` |
|       - |  301 | `	 */` |
|    5749 |  302 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  303 | `		return PH7_OK; /* Already initialized */` |
|       - |  304 | `	}` |
|       - |  305 | `	/* Point to the built-in vfs */` |
|    5749 |  306 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  307 | `	/* Install it */` |
|    5749 |  308 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  309 | `#if defined(PH7_ENABLE_THREADS)` |
|    5749 |  310 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    5749 |  311 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    5749 |  312 | `		if( pMutexMethods == 0 ){` |
|       - |  313 | `			/* Use the built-in mutex subsystem */` |
|    5749 |  314 | `			pMutexMethods = SyMutexExportMethods();` |
|    5749 |  315 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  316 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  317 | `			}` |
|       - |  318 | `			/* Install the mutex subsystem */` |
|    5749 |  319 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    5749 |  320 | `			if( rc != PH7_OK ){` |
|     ! 0 |  321 | `				return rc;` |
|       - |  322 | `			}` |
|    2873 |  323 | `		}` |
|       - |  324 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    5749 |  325 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    5749 |  326 | `		if( pMaster == 0 ){` |
|     ! 0 |  327 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  328 | `		}` |
|    2873 |  329 | `	}` |
|       - |  330 | `	/* Lock the master mutex */` |
|    5749 |  331 | `	rc = PH7_OK;` |
|    5749 |  332 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    8622 |  333 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  334 | `#endif` |
|    5749 |  335 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  336 | `			/* Install a memory subsystem */` |
|    5749 |  337 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    5749 |  338 | `			if( rc != PH7_OK ){` |
|       - |  339 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  340 | `				goto End;` |
|       - |  341 | `			}` |
|    2873 |  342 | `		}` |
|       - |  343 | `#if defined(PH7_ENABLE_THREADS)` |
|    5749 |  344 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  345 | `			/* Protect the memory allocation subsystem */` |
|    5749 |  346 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    5749 |  347 | `			if( rc != PH7_OK ){` |
|     ! 0 |  348 | `				goto End;` |
|       - |  349 | `			}` |
|    2873 |  350 | `		}` |
|       - |  351 | `#endif` |
|       - |  352 | `		/* Our library is initialized,set the magic number */` |
|    5749 |  353 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    5749 |  354 | `		rc = PH7_OK;` |
|       - |  355 | `#if defined(PH7_ENABLE_THREADS)` |
|    2873 |  356 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  357 | `#endif` |
|     ! 0 |  358 | `End:` |
|       - |  359 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  360 | `	/* Unlock the master mutex */` |
|    5749 |  361 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  362 | `#endif` |
|    5749 |  363 | `	return rc;` |
|    2878 |  364 | `}` |
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
|    5750 |  378 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       5 |  379 | `{` |
|       - |  380 | `	ph7_vm *pVm,*pNext;` |
|       - |  381 | `	/* Release all active VM */` |
|    5755 |  382 | `	pVm = pEngine->pVms;` |
|    2876 |  383 | `	for(;;){` |
|    5755 |  384 | `		if( pEngine->iVm <= 0 ){` |
|    5755 |  385 | `			break;` |
|       - |  386 | `		}` |
|     ! 0 |  387 | `		pNext = pVm->pNext;` |
|     ! 0 |  388 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  389 | `		pVm = pNext;` |
|     ! 0 |  390 | `		pEngine->iVm--;` |
|     ! 0 |  391 | `	}` |
|       - |  392 | `	/* Set a dummy magic number */` |
|    5755 |  393 | `	pEngine->nMagic = 0x7635;` |
|       - |  394 | `	/* Release the private memory subsystem */` |
|    5755 |  395 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    5755 |  396 | `	return PH7_OK;` |
|       5 |  397 | `}` |
|       - |  398 | `/*` |
|       - |  399 | ` * Release all resources consumed by the library.` |
|       - |  400 | ` * If PH7 is already shut down when this routine` |
|       - |  401 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  402 | ` * Note: This call is not thread safe.` |
|       - |  403 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  404 | ` */` |
|     780 |  405 | `static void PH7CoreShutdown(void)` |
|       4 |  406 | `{` |
|       - |  407 | `	ph7 *pEngine,*pNext;` |
|       - |  408 | `	/* Release all active engines first */` |
|     784 |  409 | `	pEngine = sMPGlobal.pEngines;` |
|     390 |  410 | `	for(;;){` |
|     784 |  411 | `		if( sMPGlobal.nEngine < 1 ){` |
|     784 |  412 | `			break;` |
|       - |  413 | `		}` |
|     ! 0 |  414 | `		pNext = pEngine->pNext;` |
|     ! 0 |  415 | `		EngineRelease(pEngine);` |
|     ! 0 |  416 | `		pEngine = pNext;` |
|     ! 0 |  417 | `		sMPGlobal.nEngine--;` |
|     ! 0 |  418 | `	}` |
|       - |  419 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  420 | `	/* Release the mutex subsystem */` |
|     784 |  421 | `	if( sMPGlobal.pMutexMethods ){` |
|     784 |  422 | `		if( sMPGlobal.pMutex ){` |
|     784 |  423 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     784 |  424 | `			sMPGlobal.pMutex = 0;` |
|     390 |  425 | `		}` |
|     784 |  426 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       4 |  427 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  428 | `		}` |
|     784 |  429 | `		sMPGlobal.pMutexMethods = 0;` |
|     390 |  430 | `	}` |
|     784 |  431 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  432 | `#endif` |
|     784 |  433 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  434 | `		/* Release the memory backend */` |
|     784 |  435 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     390 |  436 | `	}` |
|     784 |  437 | `	sMPGlobal.nMagic = 0x1928;` |
|     784 |  438 | `}` |
|       - |  439 | `/*` |
|       - |  440 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  441 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  442 | ` */` |
|     780 |  443 | `int ph7_lib_shutdown(void)` |
|       4 |  444 | `{` |
|     784 |  445 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  446 | `		/* Already shut */` |
|     ! 0 |  447 | `		return PH7_OK;` |
|       - |  448 | `	}` |
|     784 |  449 | `	PH7CoreShutdown();` |
|     784 |  450 | `	return PH7_OK;` |
|     394 |  451 | `}` |
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
|       2 |  477 | `const char * ph7_lib_version(void)` |
|       1 |  478 | `{` |
|       3 |  479 | `	return PH7_VERSION;` |
|       1 |  480 | `}` |
|       - |  481 | `/*` |
|       - |  482 | ` * [CAPIREF: ph7_lib_signature()]` |
|       - |  483 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  484 | ` */` |
|     196 |  485 | `const char * ph7_lib_signature(void)` |
|       3 |  486 | `{` |
|     199 |  487 | `	return PH7_SIG;` |
|       3 |  488 | `}` |
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
|   11456 |  509 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       5 |  510 | `{` |
|       - |  511 | `	va_list ap;` |
|       - |  512 | `	int rc;` |
|   11461 |  513 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  514 | `		return PH7_CORRUPT;` |
|       - |  515 | `	}` |
|       - |  516 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  517 | `	 /* Acquire engine mutex */` |
|   11461 |  518 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   11461 |  519 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   11456 |  520 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  521 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  522 | `	 }` |
|       - |  523 | `#endif` |
|   11461 |  524 | `	 va_start(ap,nConfigOp);` |
|   11461 |  525 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|   11461 |  526 | `	 va_end(ap);` |
|       - |  527 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  528 | `	 /* Leave engine mutex */` |
|   11461 |  529 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  530 | `#endif` |
|   11461 |  531 | `	return rc;` |
|    5735 |  532 | `}` |
|       - |  533 | `/*` |
|       - |  534 | ` * [CAPIREF: ph7_init()]` |
|       - |  535 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  536 | ` */` |
|    5744 |  537 | `int ph7_init(ph7 **ppEngine)` |
|       5 |  538 | `{` |
|       - |  539 | `	ph7 *pEngine;` |
|       - |  540 | `	int rc;` |
|       - |  541 | `#if defined(UNTRUST)` |
|       - |  542 | `	if( ppEngine == 0 ){` |
|       - |  543 | `		return PH7_CORRUPT;` |
|       - |  544 | `	}` |
|       - |  545 | `#endif` |
|    5749 |  546 | `	*ppEngine = 0;` |
|       - |  547 | `	/* One-time automatic library initialization */` |
|    5749 |  548 | `	rc = PH7CoreInitialize();` |
|    5749 |  549 | `	if( rc != PH7_OK ){` |
|     ! 0 |  550 | `		return rc;` |
|       - |  551 | `	}` |
|       - |  552 | `	/* Allocate a new engine */` |
|    5749 |  553 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    5749 |  554 | `	if( pEngine == 0 ){` |
|     ! 0 |  555 | `		return PH7_NOMEM;` |
|       - |  556 | `	}` |
|       - |  557 | `	/* Zero the structure */` |
|    5749 |  558 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  559 | `	/* Initialize engine fields */` |
|    5749 |  560 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    5749 |  561 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    5749 |  562 | `	if( rc != PH7_OK ){` |
|     ! 0 |  563 | `		goto Release;` |
|       - |  564 | `	}` |
|       - |  565 | `#if defined(PH7_ENABLE_THREADS)` |
|    5749 |  566 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  567 | `#endif` |
|       - |  568 | `	/* Default configuration */` |
|    5749 |  569 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  570 | `	/* Install a default compile-time error consumer routine */` |
|    5749 |  571 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  572 | `	/* Built-in vfs */` |
|    5749 |  573 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  574 | `#if defined(PH7_ENABLE_THREADS)` |
|    5749 |  575 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  576 | `		 /* Associate a recursive mutex with this instance */` |
|    5749 |  577 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    5749 |  578 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  579 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  580 | `			 goto Release;` |
|       - |  581 | `		 }` |
|    2873 |  582 | `	 }` |
|       - |  583 | `#endif` |
|       - |  584 | `	/* Link to the list of active engines */` |
|       - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  586 | `	/* Enter the global mutex */` |
|    5749 |  587 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  588 | `#endif` |
|    5749 |  589 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    5749 |  590 | `	sMPGlobal.nEngine++;` |
|       - |  591 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  592 | `	/* Leave the global mutex */` |
|    5749 |  593 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  594 | `#endif` |
|       - |  595 | `	/* Write a pointer to the new instance */` |
|    5749 |  596 | `	*ppEngine = pEngine;` |
|    5749 |  597 | `	return PH7_OK;` |
|     ! 0 |  598 | `Release:` |
|     ! 0 |  599 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  600 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  601 | `	return rc;` |
|    2878 |  602 | `}` |
|       - |  603 | `/*` |
|       - |  604 | ` * [CAPIREF: ph7_release()]` |
|       - |  605 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  606 | ` */` |
|    5750 |  607 | `int ph7_release(ph7 *pEngine)` |
|       5 |  608 | `{` |
|       - |  609 | `	int rc;` |
|    5755 |  610 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  611 | `		return PH7_CORRUPT;` |
|       - |  612 | `	}` |
|       - |  613 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  614 | `	 /* Acquire engine mutex */` |
|    5755 |  615 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    5755 |  616 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    5750 |  617 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  618 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  619 | `	 }` |
|       - |  620 | `#endif` |
|       - |  621 | `	/* Release the engine */` |
|    5755 |  622 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  623 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  624 | `	 /* Leave engine mutex */` |
|    5755 |  625 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  626 | `	 /* Release engine mutex */` |
|    5755 |  627 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  628 | `#endif` |
|       - |  629 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  630 | `	/* Enter the global mutex */` |
|    5755 |  631 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  632 | `#endif` |
|       - |  633 | `	/* Unlink from the list of active engines */` |
|    5755 |  634 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    5755 |  635 | `	sMPGlobal.nEngine--;` |
|       - |  636 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  637 | `	/* Leave the global mutex */` |
|    5755 |  638 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  639 | `#endif` |
|       - |  640 | `	/* Release the memory chunk allocated to this engine */` |
|    5755 |  641 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    5755 |  642 | `	return rc;` |
|    2881 |  643 | `}` |
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
|    5742 |  654 | `static sxi32 ProcessScript(` |
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
|    5747 |  665 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    5747 |  666 | `	if( pVm == 0 ){` |
|       - |  667 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  668 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  669 | `		if( ppVm ){` |
|     ! 0 |  670 | `			*ppVm = 0;` |
|     ! 0 |  671 | `		}` |
|     ! 0 |  672 | `		return PH7_NOMEM;` |
|       - |  673 | `	}` |
|    5747 |  674 | `	if( iFlags < 0 ){` |
|       - |  675 | `		/* Default compile-time flags */` |
|     ! 0 |  676 | `		iFlags = 0;` |
|     ! 0 |  677 | `	}` |
|       - |  678 | `	/* Initialize the Virtual Machine */` |
|    5747 |  679 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    5747 |  680 | `	if( rc != PH7_OK ){` |
|     ! 0 |  681 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  682 | `		if( ppVm ){` |
|     ! 0 |  683 | `			*ppVm = 0;` |
|     ! 0 |  684 | `		}` |
|     ! 0 |  685 | `		return PH7_VM_ERR;` |
|       - |  686 | `	}` |
|    5747 |  687 | `	if( zFilePath ){` |
|       - |  688 | `		/* Push processed file path */` |
|    5707 |  689 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    2857 |  690 | `	}else{` |
|       - |  691 | `		/* Anonymous source (phl -r / an embedder snippet): php names it` |
|       - |  692 | `		 * "Command line code" in every diagnostic location suffix. */` |
|      42 |  693 | `		PH7_VmPushFilePath(pVm,"Command line code",-1,TRUE,0);` |
|       - |  694 | `	}` |
|       - |  695 | `	/* Reset the error message consumer */` |
|    5747 |  696 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  697 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|       - |  698 | `	{` |
|    5747 |  699 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|    5747 |  700 | `		if( SyStringLength(pScript) > nLimit ){` |
|     ! 0 |  701 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|       - |  702 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|     ! 0 |  703 | `				SyStringLength(pScript),nLimit);` |
|     ! 0 |  704 | `		}` |
|       - |  705 | `	}` |
|       - |  706 | `	/* Compile the script */` |
|    5747 |  707 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|    5747 |  708 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    2872 |  709 | `	}` |
|    5747 |  710 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     782 |  711 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  712 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     782 |  713 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     782 |  714 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     782 |  715 | `		if( ppVm ){` |
|     782 |  716 | `			*ppVm = 0;` |
|     389 |  717 | `		}` |
|     782 |  718 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  719 | `	}` |
|       - |  720 | `	/* Prepare the virtual machine for bytecode execution */` |
|    4969 |  721 | `	rc = PH7_VmMakeReady(pVm);` |
|    4969 |  722 | `	if( rc != PH7_OK ){` |
|       6 |  723 | `		goto Release;` |
|       - |  724 | `	}` |
|       - |  725 | `	/* Install local import path which is the current directory */` |
|    4965 |  726 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  727 | `#if defined(PH7_ENABLE_THREADS)` |
|    4965 |  728 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  729 | `		 /* Associate a recursive mutex with this instance */` |
|    4965 |  730 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    4965 |  731 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  732 | `			 goto Release;` |
|       - |  733 | `		 }` |
|    2481 |  734 | `	 }` |
|       - |  735 | `#endif` |
|       - |  736 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    4965 |  737 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    4965 |  738 | `	pEngine->iVm++;` |
|       - |  739 | `	/* Point to the freshly created VM */` |
|    4965 |  740 | `	*ppVm = pVm;` |
|       - |  741 | `	/* Ready to execute PH7 bytecode */` |
|    4965 |  742 | `	return PH7_OK;` |
|       2 |  743 | `Release:` |
|       - |  744 | `	{` |
|       - |  745 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|       - |  746 | `		 * typed class constant whose value violates its declared type) is a compile` |
|       - |  747 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|       - |  748 | `		 * Captured before the releases free the VM. */` |
|       6 |  749 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|       6 |  750 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|       6 |  751 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       6 |  752 | `		*ppVm = 0;` |
|       6 |  753 | `		return rcRet;` |
|       - |  754 | `	}` |
|    2877 |  755 | `}` |
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
|      40 |  793 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       2 |  794 | `{` |
|       - |  795 | `	SyString sScript;` |
|       - |  796 | `	int rc;` |
|      42 |  797 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  798 | `		return PH7_CORRUPT;` |
|       - |  799 | `	}` |
|      42 |  800 | `	if( nLen < 0 ){` |
|       - |  801 | `		/* Compute input length automatically */` |
|      30 |  802 | `		nLen = (int)SyStrlen(zSource);` |
|      14 |  803 | `	}` |
|      42 |  804 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  805 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  806 | `	 /* Acquire engine mutex */` |
|      42 |  807 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      42 |  808 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      40 |  809 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  810 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  811 | `	 }` |
|       - |  812 | `#endif` |
|       - |  813 | `	/* Compile the script */` |
|      42 |  814 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  815 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  816 | `	 /* Leave engine mutex */` |
|      42 |  817 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  818 | `#endif` |
|       - |  819 | `	/* Compilation result */` |
|      42 |  820 | `	return rc;` |
|      22 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  824 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  825 | ` */` |
|    5702 |  826 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       5 |  827 | `{` |
|       - |  828 | `	const ph7_vfs *pVfs;` |
|       - |  829 | `	int rc;` |
|    5707 |  830 | `	if( ppOutVm ){` |
|    5707 |  831 | `		*ppOutVm = 0;` |
|    2852 |  832 | `	}` |
|    5707 |  833 | `	rc = PH7_OK; /* cc warning */` |
|    5707 |  834 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  835 | `		return PH7_CORRUPT;` |
|       - |  836 | `	}` |
|       - |  837 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  838 | `	 /* Acquire engine mutex */` |
|    5707 |  839 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    5707 |  840 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    5702 |  841 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  842 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  843 | `	 }` |
|       - |  844 | `#endif` |
|       - |  845 | `	 /*` |
|       - |  846 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  847 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  848 | `	  */` |
|    5707 |  849 | `	 pVfs = pEngine->pVfs;` |
|    5707 |  850 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  851 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  852 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  853 | `	 }else{` |
|    5707 |  854 | `		 void *pMapView = 0; /* cc warning */` |
|    5707 |  855 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  856 | `		 SyString sScript;` |
|       - |  857 | `		 /* Try to get a memory view of the whole file */` |
|    5707 |  858 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    5707 |  859 | `		 if( rc != PH7_OK ){` |
|       - |  860 | `			 /* Assume an IO error */` |
|     ! 0 |  861 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  862 | `		 }else{` |
|       - |  863 | `			 /* Compile the file */` |
|    5707 |  864 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    5707 |  865 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  866 | `			 /* Release the memory view of the whole file */` |
|    5707 |  867 | `			 if( pVfs->xUnmap ){` |
|    5707 |  868 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    2852 |  869 | `			 }` |
|       - |  870 | `		 }` |
|       - |  871 | `	 }` |
|       - |  872 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  873 | `	 /* Leave engine mutex */` |
|    5707 |  874 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  875 | `#endif` |
|       - |  876 | `	/* Compilation result */` |
|    5707 |  877 | `	return rc;` |
|    2857 |  878 | `}` |
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
|  139958 |  903 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       5 |  904 | `{` |
|       - |  905 | `	va_list ap;` |
|       - |  906 | `	int rc;` |
|       - |  907 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  139963 |  908 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  909 | `		return PH7_CORRUPT;` |
|       - |  910 | `	}` |
|       - |  911 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  912 | `	 /* Acquire VM mutex */` |
|  139963 |  913 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  139963 |  914 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  139958 |  915 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  916 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  917 | `	 }` |
|       - |  918 | `#endif` |
|       - |  919 | `	/* Confiugure the virtual machine */` |
|  139963 |  920 | `	va_start(ap,iConfigOp);` |
|  139963 |  921 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|  139963 |  922 | `	va_end(ap);` |
|       - |  923 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  924 | `	 /* Leave VM mutex */` |
|  139963 |  925 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  926 | `#endif` |
|  139963 |  927 | `	return rc;` |
|   70014 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  931 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  932 | ` */` |
|    4974 |  933 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       5 |  934 | `{` |
|       - |  935 | `	int rc;` |
|       - |  936 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    4979 |  937 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  938 | `		return PH7_CORRUPT;` |
|       - |  939 | `	}` |
|       - |  940 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  941 | `	 /* Acquire VM mutex */` |
|    4979 |  942 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    4979 |  943 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    4974 |  944 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  945 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  946 | `	 }` |
|       - |  947 | `#endif` |
|       - |  948 | `	/* Execute PH7 byte-code */` |
|    4979 |  949 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    4979 |  950 | `	if( pExitStatus ){` |
|       - |  951 | `		/* Exit status */` |
|    4933 |  952 | `		*pExitStatus = pVm->iExitStatus;` |
|    2465 |  953 | `	}` |
|       - |  954 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  955 | `	 /* Leave VM mutex */` |
|    4979 |  956 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  957 | `#endif` |
|       - |  958 | `	/* Execution result */` |
|    4979 |  959 | `	return rc;` |
|    2493 |  960 | `}` |
|       - |  961 | `/*` |
|       - |  962 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  963 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  964 | ` */` |
|      16 |  965 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  966 | `{` |
|       - |  967 | `	int rc;` |
|       - |  968 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      16 |  969 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  970 | `		return PH7_CORRUPT;` |
|       - |  971 | `	}` |
|       - |  972 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  973 | `	 /* Acquire VM mutex */` |
|      16 |  974 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      16 |  975 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      16 |  976 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  977 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  978 | `	 }` |
|       - |  979 | `#endif` |
|      16 |  980 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  981 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  982 | `	 /* Leave VM mutex */` |
|      16 |  983 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  984 | `#endif` |
|      16 |  985 | `	return rc;` |
|       8 |  986 | `}` |
|       - |  987 | `/*` |
|       - |  988 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  990 | ` */` |
|    4960 |  991 | `int ph7_vm_release(ph7_vm *pVm)` |
|       5 |  992 | `{` |
|       - |  993 | `	ph7 *pEngine;` |
|       - |  994 | `	int rc;` |
|       - |  995 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    4965 |  996 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  997 | `		return PH7_CORRUPT;` |
|       - |  998 | `	}` |
|       - |  999 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1000 | `	 /* Acquire VM mutex */` |
|    4965 | 1001 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    4965 | 1002 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    4960 | 1003 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1004 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1005 | `	 }` |
|       - | 1006 | `#endif` |
|    4965 | 1007 | `	pEngine = pVm->pEngine;` |
|    4965 | 1008 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - | 1009 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1010 | `	 /* Leave VM mutex */` |
|    4965 | 1011 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    4965 | 1012 | `	 if( rc == PH7_OK && pVm->pMutex ){` |
|       - | 1013 | `		 /* The per-VM mutex was allocated in ProcessScript and never freed — one` |
|       - | 1014 | `		  * leak per VM, which LeakSanitizer reports on every single run. */` |
|    4965 | 1015 | `		 SyMutexRelease(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|    4965 | 1016 | `		 pVm->pMutex = 0;` |
|    2481 | 1017 | `	 }` |
|       - | 1018 | `#endif` |
|    4965 | 1019 | `	if( rc == PH7_OK ){` |
|       - | 1020 | `		/* Unlink from the list of active VM */` |
|       - | 1021 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1022 | `			/* Acquire engine mutex */` |
|    4965 | 1023 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    4965 | 1024 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    4960 | 1025 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 | 1026 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1027 | `			}` |
|       - | 1028 | `#endif` |
|    4965 | 1029 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    4965 | 1030 | `		pEngine->iVm--;` |
|       - | 1031 | `		/* Release the memory chunk allocated to this VM */` |
|    4965 | 1032 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - | 1033 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1034 | `			/* Leave engine mutex */` |
|    4965 | 1035 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1036 | `#endif` |
|    2481 | 1037 | `	}` |
|    4965 | 1038 | `	return rc;` |
|    2486 | 1039 | `}` |
|       - | 1040 | `/*` |
|       - | 1041 | ` * [CAPIREF: ph7_create_function()]` |
|       - | 1042 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1043 | ` */` |
| 4275162 | 1044 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       5 | 1045 | `{` |
|       - | 1046 | `	SyString sName;` |
|       - | 1047 | `	int rc;` |
|       - | 1048 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 4275167 | 1049 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1050 | `		return PH7_CORRUPT;` |
|       - | 1051 | `	}` |
| 4275167 | 1052 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1053 | `	/* Remove leading and trailing white spaces */` |
| 4275167 | 1054 | `	SyStringFullTrim(&sName);` |
|       - | 1055 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 4275167 | 1056 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1057 | `		return PH7_CORRUPT;` |
|       - | 1058 | `	}` |
|       - | 1059 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1060 | `	 /* Acquire VM mutex */` |
| 4275167 | 1061 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 4275167 | 1062 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 4275162 | 1063 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1064 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1065 | `	 }` |
|       - | 1066 | `#endif` |
|       - | 1067 | `	/* Install the foreign function */` |
| 4275167 | 1068 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1069 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1070 | `	 /* Leave VM mutex */` |
| 4275167 | 1071 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1072 | `#endif` |
| 4275167 | 1073 | `	return rc;` |
| 2138422 | 1074 | `}` |
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
| 6529582 | 1113 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       5 | 1114 | `{` |
|       - | 1115 | `	SyString sName;` |
|       - | 1116 | `	int rc;` |
|       - | 1117 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 6529587 | 1118 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1119 | `		return PH7_CORRUPT;` |
|       - | 1120 | `	}` |
| 6529587 | 1121 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1122 | `	/* Remove leading and trailing white spaces */` |
| 6534551 | 1123 | `	SyStringFullTrim(&sName);` |
| 6529587 | 1124 | `	if( sName.nByte < 1 ){` |
|       - | 1125 | `		/* Empty constant name */` |
|     ! 0 | 1126 | `		return PH7_CORRUPT;` |
|       - | 1127 | `	}` |
|       - | 1128 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
| 6529587 | 1129 | `	if( xExpand == 0 ){` |
|     ! 0 | 1130 | `		return PH7_CORRUPT;` |
|       - | 1131 | `	}` |
|       - | 1132 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1133 | `	 /* Acquire VM mutex */` |
| 6529587 | 1134 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 6529587 | 1135 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 6529582 | 1136 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1137 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1138 | `	 }` |
|       - | 1139 | `#endif` |
|       - | 1140 | `	/* Perform the registration */` |
| 6529587 | 1141 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1142 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1143 | `	 /* Leave VM mutex */` |
| 6529587 | 1144 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1145 | `#endif` |
| 6529587 | 1146 | `	 return rc;` |
| 3266107 | 1147 | `}` |
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
| 1892925 | 1185 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       5 | 1186 | `{` |
|       - | 1187 | `	ph7_value *pObj;` |
|       - | 1188 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 1892930 | 1189 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1190 | `		return 0;` |
|       - | 1191 | `	}` |
|       - | 1192 | `	/* Allocate a new scalar variable */` |
| 1892930 | 1193 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
| 1892930 | 1194 | `	if( pObj == 0 ){` |
|     ! 0 | 1195 | `		return 0;` |
|       - | 1196 | `	}` |
|       - | 1197 | `	/* Nullify the new scalar */` |
| 1892930 | 1198 | `	PH7_MemObjInit(pVm,pObj);` |
| 1892930 | 1199 | `	return pObj;` |
|  946494 | 1200 | `}` |
|       - | 1201 | `/*` |
|       - | 1202 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1203 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1204 | ` */` |
| 2969423 | 1205 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       5 | 1206 | `{` |
|       - | 1207 | `	ph7_hashmap *pMap;` |
|       - | 1208 | `	ph7_value *pObj;` |
|       - | 1209 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 2969428 | 1210 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1211 | `		return 0;` |
|       - | 1212 | `	}` |
|       - | 1213 | `	/* Create a new hashmap first */` |
| 2969428 | 1214 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
| 2969428 | 1215 | `	if( pMap == 0 ){` |
|     ! 0 | 1216 | `		return 0;` |
|       - | 1217 | `	}` |
|       - | 1218 | `	/* Associate a new ph7_value with this hashmap */` |
| 2969428 | 1219 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
| 2969428 | 1220 | `	if( pObj == 0 ){` |
|     ! 0 | 1221 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1222 | `		return 0;` |
|       - | 1223 | `	}` |
| 2969428 | 1224 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
| 2969428 | 1225 | `	return pObj;` |
| 1484752 | 1226 | `}` |
|       - | 1227 | `/*` |
|       - | 1228 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1229 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1230 | ` */` |
| 3622260 | 1231 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1232 | `{` |
|       - | 1233 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 3622265 | 1234 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     130 | 1235 | `		return PH7_CORRUPT;` |
|       - | 1236 | `	}` |
| 3622137 | 1237 | `	if( pValue ){` |
|       - | 1238 | `		/* Release the value */` |
| 3622137 | 1239 | `		PH7_MemObjRelease(pValue);` |
| 3622137 | 1240 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
| 1811076 | 1241 | `	}` |
| 3622137 | 1242 | `	return PH7_OK;` |
| 1811145 | 1243 | `}` |
|       - | 1244 | `/*` |
|       - | 1245 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1246 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1247 | ` */` |
|   41836 | 1248 | `int ph7_value_to_int(ph7_value *pValue)` |
|       5 | 1249 | `{` |
|       - | 1250 | `	int rc;` |
|   41841 | 1251 | `	rc = PH7_MemObjToInteger(pValue);` |
|   41841 | 1252 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1253 | `		return 0;` |
|       - | 1254 | `	}` |
|   41841 | 1255 | `	return (int)pValue->x.iVal;` |
|   21100 | 1256 | `}` |
|       - | 1257 | `/*` |
|       - | 1258 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1259 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1260 | ` */` |
|   35003 | 1261 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       5 | 1262 | `{` |
|       - | 1263 | `	int rc;` |
|   35008 | 1264 | `	rc = PH7_MemObjToBool(pValue);` |
|   35008 | 1265 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1266 | `		return 0;` |
|       - | 1267 | `	}` |
|   35008 | 1268 | `	return (int)pValue->x.iVal;` |
|   17514 | 1269 | `}` |
|       - | 1270 | `/*` |
|       - | 1271 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1272 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1273 | ` */` |
| 4095041 | 1274 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       5 | 1275 | `{` |
|       - | 1276 | `	int rc;` |
| 4095046 | 1277 | `	rc = PH7_MemObjToInteger(pValue);` |
| 4095046 | 1278 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1279 | `		return 0;` |
|       - | 1280 | `	}` |
| 4095046 | 1281 | `	return pValue->x.iVal;` |
| 2048174 | 1282 | `}` |
|       - | 1283 | `/*` |
|       - | 1284 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1285 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1286 | ` */` |
|    3420 | 1287 | `double ph7_value_to_double(ph7_value *pValue)` |
|       5 | 1288 | `{` |
|       - | 1289 | `	int rc;` |
|    3425 | 1290 | `	rc = PH7_MemObjToReal(pValue);` |
|    3425 | 1291 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1292 | `		return (double)0;` |
|       - | 1293 | `	}` |
|    3425 | 1294 | `	return (double)pValue->rVal;` |
|    1729 | 1295 | `}` |
|       - | 1296 | `/*` |
|       - | 1297 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1298 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1299 | ` */` |
| 7930370 | 1300 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       5 | 1301 | `{` |
| 7930375 | 1302 | `	PH7_MemObjToString(pValue);` |
| 7930375 | 1303 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
| 7833380 | 1304 | `		SyBlobNullAppend(&pValue->sBlob);` |
| 7833380 | 1305 | `		if( pLen ){` |
| 7731729 | 1306 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
| 3869853 | 1307 | `		}` |
| 7833380 | 1308 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1309 | `	}else{` |
|       - | 1310 | `		/* Return the empty string */` |
|   97000 | 1311 | `		if( pLen ){` |
|   96706 | 1312 | `			*pLen = 0;` |
|   48432 | 1313 | `		}` |
|   97000 | 1314 | `		return "";` |
|       - | 1315 | `	}` |
| 3969263 | 1316 | `}` |
|       - | 1317 | `/*` |
|       - | 1318 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1319 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1320 | ` */` |
|   82507 | 1321 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       5 | 1322 | `{` |
|   82512 | 1323 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1324 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1325 | `		return 0;` |
|       - | 1326 | `	}` |
|   82512 | 1327 | `	return pValue->x.pOther;` |
|   41365 | 1328 | `}` |
|       - | 1329 | `/*` |
|       - | 1330 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1331 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1332 | ` */` |
|      76 | 1333 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       3 | 1334 | `{` |
|       - | 1335 | `	int rc;` |
|      79 | 1336 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1337 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1338 | `		return 1;` |
|       - | 1339 | `	}` |
|       - | 1340 | `	/* Perform the comparison */` |
|      79 | 1341 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1342 | `	/* A native compare handler may have REFUSED the pair (php throws comparing` |
|       - | 1343 | `	 * two different KINDS of DateTimeZone). The record is deliberately LEFT` |
|       - | 1344 | `	 * standing: array_keys() reaches the comparator through this entry point, and` |
|       - | 1345 | `	 * the host-call boundary raises what it recorded exactly as it does for the` |
|       - | 1346 | `	 * builtins that call PH7_MemObjCmp directly. A host application driving this` |
|       - | 1347 | `	 * outside any execution never sees the throw, and PH7_VmInit/PH7_VmReset` |
|       - | 1348 | `	 * clear the record before the next one begins. */` |
|       - | 1349 | `	/* Comparison result */` |
|      79 | 1350 | `	return rc;` |
|      41 | 1351 | `}` |
|       - | 1352 | `/*` |
|       - | 1353 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1354 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1355 | ` */` |
|  181192 | 1356 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       5 | 1357 | `{` |
|  181197 | 1358 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       5 | 1359 | `}` |
|       - | 1360 | `/*` |
|       - | 1361 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1362 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1363 | ` */` |
| 1661599 | 1364 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       5 | 1365 | `{` |
| 1661604 | 1366 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       5 | 1367 | `}` |
|       - | 1368 | `/*` |
|       - | 1369 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1370 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1371 | ` */` |
|  519647 | 1372 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       5 | 1373 | `{` |
|  519652 | 1374 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       5 | 1375 | `}` |
|       - | 1376 | `/*` |
|       - | 1377 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1378 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1379 | ` */` |
|    1172 | 1380 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       4 | 1381 | `{` |
|    1176 | 1382 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       4 | 1383 | `}` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1386 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1387 | ` */` |
|   10582 | 1388 | `int ph7_result_null(ph7_context *pCtx)` |
|       5 | 1389 | `{` |
|       - | 1390 | `	/* Invalidate any prior representation and set the NULL flag */` |
|   10587 | 1391 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   10587 | 1392 | `	return PH7_OK;` |
|       5 | 1393 | `}` |
|       - | 1394 | `/*` |
|       - | 1395 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1396 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1397 | ` */` |
| 3791907 | 1398 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1399 | `{` |
| 3791912 | 1400 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       5 | 1401 | `}` |
|       - | 1402 | `/*` |
|       - | 1403 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1404 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1405 | ` */` |
|  406348 | 1406 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       5 | 1407 | `{` |
|       - | 1408 | `	ph7_value *p;` |
|       - | 1409 | `	va_list ap;` |
|       - | 1410 | `	int rc;` |
|  406353 | 1411 | `	p = pCtx->pRet;` |
|  406353 | 1412 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1413 | `		/* Invalidate any prior representation */` |
|  394190 | 1414 | `		PH7_MemObjRelease(p);` |
|  394190 | 1415 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|  197093 | 1416 | `	}` |
|       - | 1417 | `	/* Format the given string */` |
|  406353 | 1418 | `	va_start(ap,zFormat);` |
|  406353 | 1419 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|  406353 | 1420 | `	va_end(ap);` |
|  406353 | 1421 | `	return rc;` |
|       5 | 1422 | `}` |
|       - | 1423 | `/*` |
|       - | 1424 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1425 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1426 | ` */` |
|  873176 | 1427 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1428 | `{` |
|  873181 | 1429 | `	int rc = PH7_OK;` |
|  873181 | 1430 | `	if( pValue == 0 ){` |
|     ! 0 | 1431 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1432 | `	}else{` |
|  873181 | 1433 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1434 | `	}` |
|  873181 | 1435 | `	return rc;` |
|       5 | 1436 | `}` |
|       - | 1437 | `/*` |
|       - | 1438 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1439 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1440 | ` */` |
|    8882 | 1441 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       5 | 1442 | `{` |
|    8887 | 1443 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       5 | 1444 | `}` |
|       - | 1445 | `/*` |
|       - | 1446 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1447 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1448 | ` */` |
|  423070 | 1449 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       5 | 1450 | `{` |
|       - | 1451 | `	ph7_value *pVal;` |
|  423075 | 1452 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|  423075 | 1453 | `	if( pVal ){` |
|       - | 1454 | `		/* Record value address so it can be freed automatically` |
|       - | 1455 | `		 * when the calling function returns.` |
|       - | 1456 | `		 */` |
|  423075 | 1457 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|  211561 | 1458 | `	}` |
|  423075 | 1459 | `	return pVal;` |
|       5 | 1460 | `}` |
|       - | 1461 | `/*` |
|       - | 1462 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1463 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1464 | ` */` |
|  816708 | 1465 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       5 | 1466 | `{` |
|       - | 1467 | `	ph7_value *pVal;` |
|  816713 | 1468 | `	pVal = ph7_new_array(pCtx->pVm);` |
|  816713 | 1469 | `	if( pVal ){` |
|       - | 1470 | `		/* Record value address so it can be freed automatically` |
|       - | 1471 | `		 * when the calling function returns.` |
|       - | 1472 | `		 */` |
|  816713 | 1473 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|  408379 | 1474 | `	}` |
|  816713 | 1475 | `	return pVal;` |
|       5 | 1476 | `}` |
|       - | 1477 | `/*` |
|       - | 1478 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1479 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1480 | ` */` |
|   15708 | 1481 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1482 | `{` |
|   15713 | 1483 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|   15713 | 1484 | `}` |
|       - | 1485 | `/*` |
|       - | 1486 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1487 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1488 | ` */` |
|   19804 | 1489 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       5 | 1490 | `{` |
|       - | 1491 | `	void *pChunk;` |
|   19809 | 1492 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|   19809 | 1493 | `	if( pChunk ){` |
|   19809 | 1494 | `		if( ZeroChunk ){` |
|       - | 1495 | `			/* Zero the memory chunk */` |
|   11213 | 1496 | `			SyZero(pChunk,nByte);` |
|    5617 | 1497 | `		}` |
|   19809 | 1498 | `		if( AutoRelease ){` |
|       - | 1499 | `			ph7_aux_data sAux;` |
|       - | 1500 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1501 | `			 * upon this context is destroyed.` |
|       - | 1502 | `			 */` |
|   10563 | 1503 | `			sAux.pAuxData = pChunk;` |
|   10563 | 1504 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|    5279 | 1505 | `		}` |
|    9919 | 1506 | `	}` |
|   19809 | 1507 | `	return pChunk;` |
|       5 | 1508 | `}` |
|       - | 1509 | `/*` |
|       - | 1510 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1511 | ` * chunk container.` |
|       - | 1512 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1513 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1514 | ` */` |
|    1128 | 1515 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1516 | `{` |
|       - | 1517 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1518 | `	sxu32 n;` |
|    1133 | 1519 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1520 | `		/* Don't bother processing,the container is empty */` |
|     457 | 1521 | `		return 0;` |
|       - | 1522 | `	}` |
|       - | 1523 | `	/* Perform the lookup */` |
|     680 | 1524 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|    1052 | 1525 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|    1052 | 1526 | `		pAux = &aAux[n];` |
|    1052 | 1527 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1528 | `			/* Chunk found */` |
|     680 | 1529 | `			return pAux;` |
|       - | 1530 | `		}` |
|     189 | 1531 | `	}` |
|       - | 1532 | `	/* No such allocated chunk */` |
|     ! 0 | 1533 | `	return 0;` |
|     573 | 1534 | `}` |
|       - | 1535 | `/*` |
|       - | 1536 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1537 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1538 | ` */` |
|     ! 0 | 1539 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1540 | `{` |
|       - | 1541 | `	ph7_aux_data *pAux;` |
|       - | 1542 | `	void *pNew;` |
|     ! 0 | 1543 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1544 | `	if( pNew ){` |
|     ! 0 | 1545 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1546 | `		if( pAux ){` |
|     ! 0 | 1547 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1548 | `		}` |
|     ! 0 | 1549 | `	}` |
|     ! 0 | 1550 | `	return pNew;` |
|     ! 0 | 1551 | `}` |
|       - | 1552 | `/*` |
|       - | 1553 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1554 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1555 | ` */` |
|    1128 | 1556 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1557 | `{` |
|       - | 1558 | `	ph7_aux_data *pAux;` |
|    1133 | 1559 | `	if( pChunk == 0 ){` |
|       - | 1560 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1561 | `		return;` |
|       - | 1562 | `	}` |
|    1133 | 1563 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    1133 | 1564 | `	if( pAux ){` |
|       - | 1565 | `		/* Mark as destroyed */` |
|     680 | 1566 | `		pAux->pAuxData = 0;` |
|     338 | 1567 | `	}` |
|    1133 | 1568 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|     573 | 1569 | `}` |
|       - | 1570 | `/*` |
|       - | 1571 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1572 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1573 | ` */` |
|    4476 | 1574 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|       5 | 1575 | `{` |
|       - | 1576 | `	ph7_hashmap_node *pNode;` |
|       - | 1577 | `	ph7_value *pValue;` |
|       - | 1578 | `	ph7_value skey;` |
|       - | 1579 | `	int rc;` |
|       - | 1580 | `	/* Make sure we are dealing with a valid hashmap */` |
|    4481 | 1581 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1582 | `		return 0;` |
|       - | 1583 | `	}` |
|    4481 | 1584 | `	if( nByte < 0 ){` |
|    1967 | 1585 | `		nByte = (int)SyStrlen(zKey);` |
|     985 | 1586 | `	}` |
|       - | 1587 | `	/* Convert the key to a ph7_value  */` |
|    4481 | 1588 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|    4481 | 1589 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1590 | `	/* Perform the lookup */` |
|    4481 | 1591 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|    4481 | 1592 | `	PH7_MemObjRelease(&skey);` |
|    4481 | 1593 | `	if( rc != PH7_OK ){` |
|       - | 1594 | `		/* No such entry */` |
|    1627 | 1595 | `		return 0;` |
|       - | 1596 | `	}` |
|       - | 1597 | `	/* Extract the target value */` |
|    2859 | 1598 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|    2859 | 1599 | `	return pValue;` |
|    2247 | 1600 | `}` |
|       - | 1601 | `/*` |
|       - | 1602 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1603 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1604 | ` */` |
|   58832 | 1605 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       5 | 1606 | `{` |
|       - | 1607 | `	int rc;` |
|   58837 | 1608 | `	if( xWalk == 0 ){` |
|     ! 0 | 1609 | `		return PH7_CORRUPT;` |
|       - | 1610 | `	}` |
|       - | 1611 | `	/* Make sure we are dealing with a valid hashmap */` |
|   58837 | 1612 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1613 | `		return PH7_CORRUPT;` |
|       - | 1614 | `	}` |
|       - | 1615 | `	/* Start the walk process */` |
|   58837 | 1616 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   58837 | 1617 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   29421 | 1618 | `}` |
|       - | 1619 | `/*` |
|       - | 1620 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1621 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1622 | ` */` |
| 3934335 | 1623 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       5 | 1624 | `{` |
|       - | 1625 | `	int rc;` |
|       - | 1626 | `	/* Make sure we are dealing with a valid hashmap */` |
| 3934340 | 1627 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1628 | `		return PH7_CORRUPT;` |
|       - | 1629 | `	}` |
|       - | 1630 | `	/* Perform the insertion */` |
| 3934340 | 1631 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
| 3934340 | 1632 | `	return rc;` |
| 1967236 | 1633 | `}` |
|       - | 1634 | `/*` |
|       - | 1635 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1636 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1637 | ` */` |
| 3027698 | 1638 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       5 | 1639 | `{` |
|       - | 1640 | `	int rc;` |
|       - | 1641 | `	/* Make sure we are dealing with a valid hashmap */` |
| 3027703 | 1642 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1643 | `		return PH7_CORRUPT;` |
|       - | 1644 | `	}` |
|       - | 1645 | `	/* Perform the insertion */` |
| 3027703 | 1646 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1647 | `		/* Empty key,assign an automatic index */` |
|      17 | 1648 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|       9 | 1649 | `	}else{` |
|       - | 1650 | `		ph7_value sKey;` |
| 3027687 | 1651 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
| 3027687 | 1652 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
| 3027687 | 1653 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
| 3027687 | 1654 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1655 | `	}` |
| 3027703 | 1656 | `	return rc;` |
| 1513874 | 1657 | `}` |
|       - | 1658 | `/*` |
|       - | 1659 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1660 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1661 | ` */` |
| 2125458 | 1662 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       5 | 1663 | `{` |
|       - | 1664 | `	ph7_value sKey;` |
|       - | 1665 | `	int rc;` |
|       - | 1666 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2125463 | 1667 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1668 | `		return PH7_CORRUPT;` |
|       - | 1669 | `	}` |
| 2125463 | 1670 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1671 | `	/* Perform the insertion */` |
| 2125463 | 1672 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
| 2125463 | 1673 | `	PH7_MemObjRelease(&sKey);` |
| 2125463 | 1674 | `	return rc;` |
| 1062762 | 1675 | `}` |
|       - | 1676 | `/*` |
|       - | 1677 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1678 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1679 | ` */` |
| 1117492 | 1680 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       5 | 1681 | `{` |
|       - | 1682 | `	ph7_hashmap *pMap;` |
|       - | 1683 | `	/* Make sure we are dealing with a valid hashmap */` |
| 1117497 | 1684 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1685 | `		return 0;` |
|       - | 1686 | `	}` |
|       - | 1687 | `	/* Point to the internal representation of the hashmap */` |
| 1117497 | 1688 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
| 1117497 | 1689 | `	return pMap->nEntry;` |
|  558807 | 1690 | `}` |
|       - | 1691 | `/*` |
|       - | 1692 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1693 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1694 | ` */` |
|     ! 0 | 1695 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|     ! 0 | 1696 | `{` |
|       - | 1697 | `	int rc;` |
|     ! 0 | 1698 | `	if( xWalk == 0 ){` |
|     ! 0 | 1699 | `		return PH7_CORRUPT;` |
|       - | 1700 | `	}` |
|       - | 1701 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1702 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1703 | `		return PH7_CORRUPT;` |
|       - | 1704 | `	}` |
|       - | 1705 | `	/* Start the walk process */` |
|     ! 0 | 1706 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|     ! 0 | 1707 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|     ! 0 | 1708 | `}` |
|       - | 1709 | `/*` |
|       - | 1710 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1711 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1712 | ` */` |
|       8 | 1713 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1714 | `{` |
|       - | 1715 | `	ph7_value *pValue;` |
|       - | 1716 | `	SyString sAttr;` |
|       - | 1717 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1718 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1719 | `		return 0;` |
|       - | 1720 | `	}` |
|       9 | 1721 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1722 | `	/* Extract the attribute value if available.` |
|       - | 1723 | `	 */` |
|       9 | 1724 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1725 | `	return pValue;` |
|       5 | 1726 | `}` |
|       - | 1727 | `/*` |
|       - | 1728 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1729 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1730 | ` */` |
|     ! 0 | 1731 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1732 | `{` |
|       - | 1733 | `	ph7_class *pClass;` |
|     ! 0 | 1734 | `	if( pLength ){` |
|     ! 0 | 1735 | `		*pLength = 0;` |
|     ! 0 | 1736 | `	}` |
|       - | 1737 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1738 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1739 | `		return 0;` |
|       - | 1740 | `	}` |
|       - | 1741 | `	/* Point to the class */` |
|     ! 0 | 1742 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1743 | `	/* Return the class name */` |
|     ! 0 | 1744 | `	if( pLength ){` |
|     ! 0 | 1745 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1746 | `	}` |
|     ! 0 | 1747 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1748 | `}` |
|       - | 1749 | `/*` |
|       - | 1750 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1751 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1752 | ` */` |
|   87958 | 1753 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1754 | `{` |
|       - | 1755 | `	SyString sData;` |
|       - | 1756 | `	int rc;` |
|   87963 | 1757 | `	if( nLen < 0 ){` |
|     ! 0 | 1758 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1759 | `	}` |
|   87963 | 1760 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|   87963 | 1761 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|   87963 | 1762 | `	return rc;` |
|       5 | 1763 | `}` |
|       - | 1764 | `/*` |
|       - | 1765 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1766 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1767 | ` */` |
|      30 | 1768 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       2 | 1769 | `{` |
|       - | 1770 | `	va_list ap;` |
|       - | 1771 | `	int rc;` |
|      32 | 1772 | `	va_start(ap,zFormat);` |
|      32 | 1773 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|      32 | 1774 | `	va_end(ap);` |
|      32 | 1775 | `	return rc;` |
|       2 | 1776 | `}` |
|       - | 1777 | `/*` |
|       - | 1778 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1779 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1780 | ` */` |
|     254 | 1781 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       4 | 1782 | `{` |
|     258 | 1783 | `	int rc = PH7_OK;` |
|     258 | 1784 | `	if( zErr ){` |
|     258 | 1785 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|     127 | 1786 | `	}` |
|     258 | 1787 | `	return rc;` |
|       4 | 1788 | `}` |
|       - | 1789 | `/*` |
|       - | 1790 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1791 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1792 | ` */` |
|     730 | 1793 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       5 | 1794 | `{` |
|       - | 1795 | `	va_list ap;` |
|       - | 1796 | `	int rc;` |
|     735 | 1797 | `	if( zFormat == 0){` |
|     ! 0 | 1798 | `		return PH7_OK;` |
|       - | 1799 | `	}` |
|     735 | 1800 | `	va_start(ap,zFormat);` |
|     735 | 1801 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|     735 | 1802 | `	va_end(ap);` |
|     735 | 1803 | `	return rc;` |
|     370 | 1804 | `}` |
|       - | 1805 | `/*` |
|       - | 1806 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1807 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1808 | ` */` |
|     ! 0 | 1809 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|     ! 0 | 1810 | `{` |
|       - | 1811 | `	sxu32 n;` |
|     ! 0 | 1812 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|     ! 0 | 1813 | `	return n;` |
|     ! 0 | 1814 | `}` |
|       - | 1815 | `/*` |
|       - | 1816 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1817 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1818 | ` */` |
|     ! 0 | 1819 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1820 | `{` |
|     ! 0 | 1821 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1822 | `		return PH7_CORRUPT;` |
|       - | 1823 | `	}` |
|     ! 0 | 1824 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1825 | `	return PH7_OK;` |
|     ! 0 | 1826 | `}` |
|       - | 1827 | `/*` |
|       - | 1828 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1829 | ` *` |
|       - | 1830 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1831 | ` * {` |
|       - | 1832 | ` *	return pCtx->pVm;` |
|       - | 1833 | ` * }` |
|       - | 1834 | ` */` |
|       - | 1835 | `/*` |
|       - | 1836 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1837 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1838 | ` */` |
|   85258 | 1839 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       5 | 1840 | `{` |
|   85263 | 1841 | `	return pCtx->pFunc->pUserData;` |
|       5 | 1842 | `}` |
|       - | 1843 | `/*` |
|       - | 1844 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1845 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1846 | ` */` |
|      14 | 1847 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1848 | `{` |
|       - | 1849 | `	ph7_aux_data sAux;` |
|       - | 1850 | `	int rc;` |
|      15 | 1851 | `	sAux.pAuxData = pUserData;` |
|      15 | 1852 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|      15 | 1853 | `	return rc;` |
|       1 | 1854 | `}` |
|       - | 1855 | `/*` |
|       - | 1856 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1857 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1858 | ` */` |
|       4 | 1859 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1860 | `{` |
|       - | 1861 | `	ph7_aux_data *pAux;` |
|       5 | 1862 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       5 | 1863 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1864 | `}` |
|       - | 1865 | `/*` |
|       - | 1866 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1867 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1868 | ` */` |
|     ! 0 | 1869 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|     ! 0 | 1870 | `{` |
|       - | 1871 | `	ph7_aux_data *pAux;` |
|     ! 0 | 1872 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|     ! 0 | 1873 | `	return pAux ? pAux->pAuxData : 0;` |
|     ! 0 | 1874 | `}` |
|       - | 1875 | `/*` |
|       - | 1876 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1877 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1878 | ` */` |
|   42517 | 1879 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       5 | 1880 | `{` |
|   42522 | 1881 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       5 | 1882 | `}` |
|       - | 1883 | `/*` |
|       - | 1884 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1885 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1886 | ` */` |
|  181730 | 1887 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       5 | 1888 | `{` |
|       - | 1889 | `	SyString *pName;` |
|  181735 | 1890 | `	pName = &pCtx->pFunc->sName;` |
|  181735 | 1891 | `	return pName->zString;` |
|       5 | 1892 | `}` |
|       - | 1893 | `/*` |
|       - | 1894 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1895 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1896 | ` */` |
|  929664 | 1897 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       5 | 1898 | `{` |
|       - | 1899 | `	/* Invalidate any prior representation */` |
|  929669 | 1900 | `	PH7_MemObjRelease(pVal);` |
|  929669 | 1901 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|  929669 | 1902 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  929669 | 1903 | `	return PH7_OK;` |
|       5 | 1904 | `}` |
|       - | 1905 | `/*` |
|       - | 1906 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1907 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1908 | ` */` |
| 2146072 | 1909 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       5 | 1910 | `{` |
|       - | 1911 | `	/* Invalidate any prior representation */` |
| 2146077 | 1912 | `	PH7_MemObjRelease(pVal);` |
| 2146077 | 1913 | `	pVal->x.iVal = iValue;` |
| 2146077 | 1914 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
| 2146077 | 1915 | `	return PH7_OK;` |
|       5 | 1916 | `}` |
|       - | 1917 | `/*` |
|       - | 1918 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1919 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1920 | ` */` |
|  521533 | 1921 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       5 | 1922 | `{` |
|       - | 1923 | `	/* Invalidate any prior representation */` |
|  521538 | 1924 | `	PH7_MemObjRelease(pVal);` |
|  521538 | 1925 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  521538 | 1926 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  521538 | 1927 | `	return PH7_OK;` |
|       5 | 1928 | `}` |
|       - | 1929 | `/*` |
|       - | 1930 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1931 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1932 | ` */` |
|     672 | 1933 | `int ph7_value_null(ph7_value *pVal)` |
|       5 | 1934 | `{` |
|       - | 1935 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     677 | 1936 | `	PH7_MemObjRelease(pVal);` |
|     677 | 1937 | `	return PH7_OK;` |
|       5 | 1938 | `}` |
|       - | 1939 | `/*` |
|       - | 1940 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1941 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1942 | ` */` |
|    3344 | 1943 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       5 | 1944 | `{` |
|       - | 1945 | `	/* Invalidate any prior representation */` |
|    3349 | 1946 | `	PH7_MemObjRelease(pVal);` |
|    3349 | 1947 | `	pVal->rVal = (ph7_real)Value;` |
|    3349 | 1948 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1949 | `	/* Try to get an integer representation also */` |
|    3349 | 1950 | `	PH7_MemObjTryInteger(pVal);` |
|    3349 | 1951 | `	return PH7_OK;` |
|       5 | 1952 | `}` |
|       - | 1953 | `/*` |
|       - | 1954 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1955 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1956 | ` */` |
| 7674020 | 1957 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       5 | 1958 | `{` |
| 7674025 | 1959 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1960 | `		/* Invalidate any prior representation */` |
| 2617163 | 1961 | `		PH7_MemObjRelease(pVal);` |
| 2617163 | 1962 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
| 1309263 | 1963 | `	}` |
| 7674025 | 1964 | `	if( zString ){` |
| 7656141 | 1965 | `		if( nLen < 0 ){` |
|       - | 1966 | `			/* Compute length automatically */` |
|   61487 | 1967 | `			nLen = (int)SyStrlen(zString);` |
|   30715 | 1968 | `		}` |
|       - | 1969 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1970 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
| 7656141 | 1971 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1972 | `	}` |
|   17889 | 1973 | `	return PH7_OK;` |
| 3838590 | 1974 | `}` |
|       - | 1975 | `/*` |
|       - | 1976 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1977 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1978 | ` */` |
|   11000 | 1979 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       3 | 1980 | `{` |
|       - | 1981 | `	va_list ap;` |
|       - | 1982 | `	int rc;` |
|   11003 | 1983 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1984 | `		/* Invalidate any prior representation */` |
|   10971 | 1985 | `		PH7_MemObjRelease(pVal);` |
|   10971 | 1986 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|    5484 | 1987 | `	}` |
|   11003 | 1988 | `	va_start(ap,zFormat);` |
|   11003 | 1989 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|   11003 | 1990 | `	va_end(ap);` |
|       - | 1991 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|   11003 | 1992 | `	return rc;` |
|       3 | 1993 | `}` |
|       - | 1994 | `/*` |
|       - | 1995 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1996 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1997 | ` */` |
| 3462407 | 1998 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       5 | 1999 | `{` |
|       - | 2000 | `	/* Reset the string cursor */` |
| 3462412 | 2001 | `	SyBlobReset(&pVal->sBlob);` |
| 3462412 | 2002 | `	return PH7_OK;` |
|       5 | 2003 | `}` |
|       - | 2004 | `/*` |
|       - | 2005 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 2006 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2007 | ` */` |
|    9616 | 2008 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       5 | 2009 | `{` |
|       - | 2010 | `	/* Invalidate any prior representation */` |
|    9621 | 2011 | `	PH7_MemObjRelease(pVal);` |
|       - | 2012 | `	/* Reflect the new type */` |
|    9621 | 2013 | `	pVal->x.pOther = pUserData;` |
|    9621 | 2014 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    9621 | 2015 | `	return PH7_OK;` |
|       5 | 2016 | `}` |
|       - | 2017 | `/*` |
|       - | 2018 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 2019 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2020 | ` */` |
|     ! 0 | 2021 | `int ph7_value_release(ph7_value *pVal)` |
|     ! 0 | 2022 | `{` |
|     ! 0 | 2023 | `	PH7_MemObjRelease(pVal);` |
|     ! 0 | 2024 | `	return PH7_OK;` |
|     ! 0 | 2025 | `}` |
|       - | 2026 | `/*` |
|       - | 2027 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 2028 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2029 | ` */` |
| 1036155 | 2030 | `int ph7_value_is_int(ph7_value *pVal)` |
|       5 | 2031 | `{` |
|       - | 2032 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 2033 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 2034 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 2035 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 2036 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
| 1036160 | 2037 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       5 | 2038 | `}` |
|       - | 2039 | `/*` |
|       - | 2040 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 2041 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2042 | ` */` |
| 1058313 | 2043 | `int ph7_value_is_float(ph7_value *pVal)` |
|       5 | 2044 | `{` |
| 1058318 | 2045 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       5 | 2046 | `}` |
|       - | 2047 | `/*` |
|       - | 2048 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 2049 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2050 | ` */` |
|   60344 | 2051 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       5 | 2052 | `{` |
|   60349 | 2053 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       5 | 2054 | `}` |
|       - | 2055 | `/*` |
|       - | 2056 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2057 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2058 | ` */` |
| 1150370 | 2059 | `int ph7_value_is_string(ph7_value *pVal)` |
|       5 | 2060 | `{` |
| 1150375 | 2061 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       5 | 2062 | `}` |
|       - | 2063 | `/*` |
|       - | 2064 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2065 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2066 | ` */` |
| 2759911 | 2067 | `int ph7_value_is_null(ph7_value *pVal)` |
|       5 | 2068 | `{` |
| 2759916 | 2069 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       5 | 2070 | `}` |
|       - | 2071 | `/*` |
|       - | 2072 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2073 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2074 | ` */` |
|   14928 | 2075 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       5 | 2076 | `{` |
|       - | 2077 | `	int rc;` |
|   14933 | 2078 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|   14933 | 2079 | `	return rc;` |
|       5 | 2080 | `}` |
|       - | 2081 | `/*` |
|       - | 2082 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2083 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2084 | ` */` |
|   72680 | 2085 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       5 | 2086 | `{` |
|       - | 2087 | `	int rc;` |
|   72685 | 2088 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   72685 | 2089 | `	return rc;` |
|       5 | 2090 | `}` |
|       - | 2091 | `/*` |
|       - | 2092 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2093 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2094 | ` */` |
|      30 | 2095 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2096 | `{` |
|      31 | 2097 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2098 | `}` |
|       - | 2099 | `/*` |
|       - | 2100 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2101 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2102 | ` */` |
|  879919 | 2103 | `int ph7_value_is_array(ph7_value *pVal)` |
|       5 | 2104 | `{` |
|  879924 | 2105 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       5 | 2106 | `}` |
|       - | 2107 | `/*` |
|       - | 2108 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2109 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2110 | ` */` |
|  301880 | 2111 | `int ph7_value_is_object(ph7_value *pVal)` |
|       5 | 2112 | `{` |
|  301885 | 2113 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       5 | 2114 | `}` |
|       - | 2115 | `/*` |
|       - | 2116 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2117 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2118 | ` */` |
|  158597 | 2119 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       5 | 2120 | `{` |
|  158602 | 2121 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       5 | 2122 | `}` |
|       - | 2123 | `/*` |
|       - | 2124 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2125 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2126 | ` */` |
|   51600 | 2127 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       5 | 2128 | `{` |
|       - | 2129 | `	int rc;` |
|   51605 | 2130 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   51605 | 2131 | `	return rc;` |
|       5 | 2132 | `}` |
|       - | 2133 | `/*` |
|       - | 2134 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2135 | ` * Check if a value holds a Fiber instance.` |
|       - | 2136 | ` */` |
|     ! 0 | 2137 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2138 | `{` |
|     ! 0 | 2139 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2140 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2141 | `}` |
|       - | 2142 | `/*` |
|       - | 2143 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2144 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2145 | ` */` |
|     ! 0 | 2146 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2147 | `{` |
|     ! 0 | 2148 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2149 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2150 | `}` |
|       - | 2151 | `/*` |
|       - | 2152 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2153 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2154 | ` */` |
|     ! 0 | 2155 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2156 | `{` |
|     ! 0 | 2157 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2158 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2159 | `}` |
|       - | 2160 | `/*` |
|       - | 2161 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2162 | ` * Check if a Fiber is currently suspended.` |
|       - | 2163 | ` */` |
|     ! 0 | 2164 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2165 | `{` |
|     ! 0 | 2166 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2167 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2168 | `}` |
|       - | 2169 | `/*` |
|       - | 2170 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2171 | ` * Check if a Fiber has completed execution.` |
|       - | 2172 | ` */` |
|     ! 0 | 2173 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2174 | `{` |
|     ! 0 | 2175 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2176 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2177 | `}` |
|       - | 2178 | `/*` |
|       - | 2179 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2180 | ` * Get the return value of a terminated Fiber.` |
|       - | 2181 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2182 | ` */` |
|     ! 0 | 2183 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2184 | `{` |
|     ! 0 | 2185 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2186 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2187 | `}` |
|       - | 2188 |  |
