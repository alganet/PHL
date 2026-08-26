# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 777/1097 lines (70.83%)

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
|       - |   45 | `	{0,0,0,0,0,0,0,0,0,{0}},` |
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
|    7754 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       5 |   79 | `{` |
|    7759 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    7759 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    7759 |   83 | `	switch(nOp){` |
|    3877 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    7759 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    7759 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    7759 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    7759 |   93 | `		pConf->xErr     = xConsumer;` |
|    7759 |   94 | `		pConf->pErrData = pUserData;` |
|    7759 |   95 | `		break;` |
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
|    7759 |  150 | `	return rc;` |
|       5 |  151 | `}` |
|       - |  152 | `/*` |
|       - |  153 | ` * Configure the PH7 library.` |
|       - |  154 | ` * return PH7_OK on success.Any other return value` |
|       - |  155 | ` * indicates failure.` |
|       - |  156 | ` * Refer to [ph7_lib_config()].` |
|       - |  157 | ` */` |
|   11664 |  158 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       5 |  159 | `{` |
|   11669 |  160 | `	int rc = PH7_OK;` |
|   11669 |  161 | `	switch(nOp){` |
|    1944 |  162 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  163 | `			/* Install a virtual file system */` |
|    3893 |  164 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    3893 |  165 | `			sMPGlobal.pVfs = pVfs;` |
|    3893 |  166 | `			break;` |
|       - |  167 | `								}` |
|    1944 |  168 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  169 | `			/* Use an alternative low-level memory allocation routines */` |
|    3893 |  170 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  171 | `			/* Save the memory failure callback (if available) */` |
|    3893 |  172 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    3893 |  173 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    3893 |  174 | `			if( pMethods == 0 ){` |
|       - |  175 | `				/* Use the built-in memory allocation subsystem */` |
|    3893 |  176 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    1949 |  177 | `			}else{` |
|     ! 0 |  178 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  179 | `			}` |
|    3893 |  180 | `			break;` |
|       - |  181 | `										  }` |
|     ! 0 |  182 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  183 | `			/* Memory failure callback */` |
|     ! 0 |  184 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  185 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  186 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  187 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  188 | `			break;` |
|       - |  189 | `												 }` |
|    1944 |  190 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  191 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  192 | `			/* Use an alternative low-level mutex subsystem */` |
|    3893 |  193 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  194 | `#if defined (UNTRUST)` |
|       - |  195 | `			if( pMethods == 0 ){` |
|       - |  196 | `				rc = PH7_CORRUPT;` |
|       - |  197 | `			}` |
|       - |  198 | `#endif` |
|       - |  199 | `			/* Sanity check */` |
|    3893 |  200 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  201 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  202 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  203 | `				break;` |
|       - |  204 | `			}` |
|    3893 |  205 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  206 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  207 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  208 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  209 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  210 | `				}` |
|     ! 0 |  211 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  212 | `			}` |
|       - |  213 | `			/* Initialize and install the new mutex subsystem */` |
|    3893 |  214 | `			if( pMethods->xGlobalInit ){` |
|       5 |  215 | `				rc = pMethods->xGlobalInit();` |
|       5 |  216 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  217 | `					break;` |
|       - |  218 | `				}` |
|     ! 0 |  219 | `			}` |
|       - |  220 | `			/* Create the global mutex */` |
|    3893 |  221 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    3893 |  222 | `			if( sMPGlobal.pMutex == 0 ){` |
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
|    3893 |  233 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    3893 |  234 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  235 | `				/* Set a default threading level */` |
|    3893 |  236 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    1944 |  237 | `			}` |
|       - |  238 | `#endif` |
|    3893 |  239 | `			break;` |
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
|   11669 |  260 | `	return rc;` |
|       5 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  264 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  265 | ` */` |
|   11664 |  266 | `int ph7_lib_config(int nConfigOp,...)` |
|       5 |  267 | `{` |
|       - |  268 | `	va_list ap;` |
|       - |  269 | `	int rc;` |
|       - |  270 |  |
|   11669 |  271 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  272 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  273 | `		return PH7_LOOKED;` |
|       - |  274 | `	}` |
|   11669 |  275 | `	va_start(ap,nConfigOp);` |
|   11669 |  276 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|   11669 |  277 | `	va_end(ap);` |
|   11669 |  278 | `	return rc;` |
|    5837 |  279 | `}` |
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
|    3888 |  290 | `static sxi32 PH7CoreInitialize(void)` |
|       5 |  291 | `{` |
|       - |  292 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  293 | `#if defined(PH7_ENABLE_THREADS)` |
|    3893 |  294 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    3893 |  295 | `	SyMutex *pMaster = 0;` |
|       - |  296 | `#endif` |
|       - |  297 | `	int rc;` |
|       - |  298 | `	/*` |
|       - |  299 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  300 | `	 * is a no-op.` |
|       - |  301 | `	 */` |
|    3893 |  302 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  303 | `		return PH7_OK; /* Already initialized */` |
|       - |  304 | `	}` |
|       - |  305 | `	/* Point to the built-in vfs */` |
|    3893 |  306 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  307 | `	/* Install it */` |
|    3893 |  308 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  309 | `#if defined(PH7_ENABLE_THREADS)` |
|    3893 |  310 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    3893 |  311 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    3893 |  312 | `		if( pMutexMethods == 0 ){` |
|       - |  313 | `			/* Use the built-in mutex subsystem */` |
|    3893 |  314 | `			pMutexMethods = SyMutexExportMethods();` |
|    3893 |  315 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  316 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  317 | `			}` |
|       - |  318 | `			/* Install the mutex subsystem */` |
|    3893 |  319 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    3893 |  320 | `			if( rc != PH7_OK ){` |
|     ! 0 |  321 | `				return rc;` |
|       - |  322 | `			}` |
|    1944 |  323 | `		}` |
|       - |  324 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    3893 |  325 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    3893 |  326 | `		if( pMaster == 0 ){` |
|     ! 0 |  327 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  328 | `		}` |
|    1944 |  329 | `	}` |
|       - |  330 | `	/* Lock the master mutex */` |
|    3893 |  331 | `	rc = PH7_OK;` |
|    3893 |  332 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    5837 |  333 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  334 | `#endif` |
|    3893 |  335 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  336 | `			/* Install a memory subsystem */` |
|    3893 |  337 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    3893 |  338 | `			if( rc != PH7_OK ){` |
|       - |  339 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  340 | `				goto End;` |
|       - |  341 | `			}` |
|    1944 |  342 | `		}` |
|       - |  343 | `#if defined(PH7_ENABLE_THREADS)` |
|    3893 |  344 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  345 | `			/* Protect the memory allocation subsystem */` |
|    3893 |  346 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    3893 |  347 | `			if( rc != PH7_OK ){` |
|     ! 0 |  348 | `				goto End;` |
|       - |  349 | `			}` |
|    1944 |  350 | `		}` |
|       - |  351 | `#endif` |
|       - |  352 | `		/* Our library is initialized,set the magic number */` |
|    3893 |  353 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    3893 |  354 | `		rc = PH7_OK;` |
|       - |  355 | `#if defined(PH7_ENABLE_THREADS)` |
|    1944 |  356 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  357 | `#endif` |
|     ! 0 |  358 | `End:` |
|       - |  359 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  360 | `	/* Unlock the master mutex */` |
|    3893 |  361 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  362 | `#endif` |
|    3893 |  363 | `	return rc;` |
|    1949 |  364 | `}` |
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
|    3888 |  378 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       5 |  379 | `{` |
|       - |  380 | `	ph7_vm *pVm,*pNext;` |
|       - |  381 | `	/* Release all active VM */` |
|    3893 |  382 | `	pVm = pEngine->pVms;` |
|    1944 |  383 | `	for(;;){` |
|    3893 |  384 | `		if( pEngine->iVm <= 0 ){` |
|    3893 |  385 | `			break;` |
|       - |  386 | `		}` |
|     ! 0 |  387 | `		pNext = pVm->pNext;` |
|     ! 0 |  388 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  389 | `		pVm = pNext;` |
|     ! 0 |  390 | `		pEngine->iVm--;` |
|     ! 0 |  391 | `	}` |
|       - |  392 | `	/* Set a dummy magic number */` |
|    3893 |  393 | `	pEngine->nMagic = 0x7635;` |
|       - |  394 | `	/* Release the private memory subsystem */` |
|    3893 |  395 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    3893 |  396 | `	return PH7_OK;` |
|       5 |  397 | `}` |
|       - |  398 | `/*` |
|       - |  399 | ` * Release all resources consumed by the library.` |
|       - |  400 | ` * If PH7 is already shut down when this routine` |
|       - |  401 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  402 | ` * Note: This call is not thread safe.` |
|       - |  403 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  404 | ` */` |
|     380 |  405 | `static void PH7CoreShutdown(void)` |
|       4 |  406 | `{` |
|       - |  407 | `	ph7 *pEngine,*pNext;` |
|       - |  408 | `	/* Release all active engines first */` |
|     384 |  409 | `	pEngine = sMPGlobal.pEngines;` |
|     380 |  410 | `	for(;;){` |
|     764 |  411 | `		if( sMPGlobal.nEngine < 1 ){` |
|     384 |  412 | `			break;` |
|       - |  413 | `		}` |
|     384 |  414 | `		pNext = pEngine->pNext;` |
|     384 |  415 | `		EngineRelease(pEngine);` |
|     384 |  416 | `		pEngine = pNext;` |
|     384 |  417 | `		sMPGlobal.nEngine--;` |
|       4 |  418 | `	}` |
|       - |  419 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  420 | `	/* Release the mutex subsystem */` |
|     384 |  421 | `	if( sMPGlobal.pMutexMethods ){` |
|     384 |  422 | `		if( sMPGlobal.pMutex ){` |
|     384 |  423 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     384 |  424 | `			sMPGlobal.pMutex = 0;` |
|     190 |  425 | `		}` |
|     384 |  426 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       4 |  427 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  428 | `		}` |
|     384 |  429 | `		sMPGlobal.pMutexMethods = 0;` |
|     190 |  430 | `	}` |
|     384 |  431 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  432 | `#endif` |
|     384 |  433 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  434 | `		/* Release the memory backend */` |
|     384 |  435 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     190 |  436 | `	}` |
|     384 |  437 | `	sMPGlobal.nMagic = 0x1928;` |
|     384 |  438 | `}` |
|       - |  439 | `/*` |
|       - |  440 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  441 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  442 | ` */` |
|     380 |  443 | `int ph7_lib_shutdown(void)` |
|       4 |  444 | `{` |
|     384 |  445 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  446 | `		/* Already shut */` |
|     ! 0 |  447 | `		return PH7_OK;` |
|       - |  448 | `	}` |
|     384 |  449 | `	PH7CoreShutdown();` |
|     384 |  450 | `	return PH7_OK;` |
|     194 |  451 | `}` |
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
|      10 |  477 | `const char * ph7_lib_version(void)` |
|       5 |  478 | `{` |
|      15 |  479 | `	return PH7_VERSION;` |
|       5 |  480 | `}` |
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
|    7754 |  509 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       5 |  510 | `{` |
|       - |  511 | `	va_list ap;` |
|       - |  512 | `	int rc;` |
|    7759 |  513 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  514 | `		return PH7_CORRUPT;` |
|       - |  515 | `	}` |
|       - |  516 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  517 | `	 /* Acquire engine mutex */` |
|    7759 |  518 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    7759 |  519 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    7754 |  520 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  521 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  522 | `	 }` |
|       - |  523 | `#endif` |
|    7759 |  524 | `	 va_start(ap,nConfigOp);` |
|    7759 |  525 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    7759 |  526 | `	 va_end(ap);` |
|       - |  527 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  528 | `	 /* Leave engine mutex */` |
|    7759 |  529 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  530 | `#endif` |
|    7759 |  531 | `	return rc;` |
|    3882 |  532 | `}` |
|       - |  533 | `/*` |
|       - |  534 | ` * [CAPIREF: ph7_init()]` |
|       - |  535 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  536 | ` */` |
|    3888 |  537 | `int ph7_init(ph7 **ppEngine)` |
|       5 |  538 | `{` |
|       - |  539 | `	ph7 *pEngine;` |
|       - |  540 | `	int rc;` |
|       - |  541 | `#if defined(UNTRUST)` |
|       - |  542 | `	if( ppEngine == 0 ){` |
|       - |  543 | `		return PH7_CORRUPT;` |
|       - |  544 | `	}` |
|       - |  545 | `#endif` |
|    3893 |  546 | `	*ppEngine = 0;` |
|       - |  547 | `	/* One-time automatic library initialization */` |
|    3893 |  548 | `	rc = PH7CoreInitialize();` |
|    3893 |  549 | `	if( rc != PH7_OK ){` |
|     ! 0 |  550 | `		return rc;` |
|       - |  551 | `	}` |
|       - |  552 | `	/* Allocate a new engine */` |
|    3893 |  553 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    3893 |  554 | `	if( pEngine == 0 ){` |
|     ! 0 |  555 | `		return PH7_NOMEM;` |
|       - |  556 | `	}` |
|       - |  557 | `	/* Zero the structure */` |
|    3893 |  558 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  559 | `	/* Initialize engine fields */` |
|    3893 |  560 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    3893 |  561 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    3893 |  562 | `	if( rc != PH7_OK ){` |
|     ! 0 |  563 | `		goto Release;` |
|       - |  564 | `	}` |
|       - |  565 | `#if defined(PH7_ENABLE_THREADS)` |
|    3893 |  566 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  567 | `#endif` |
|       - |  568 | `	/* Default configuration */` |
|    3893 |  569 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  570 | `	/* Install a default compile-time error consumer routine */` |
|    3893 |  571 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  572 | `	/* Built-in vfs */` |
|    3893 |  573 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  574 | `#if defined(PH7_ENABLE_THREADS)` |
|    3893 |  575 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  576 | `		 /* Associate a recursive mutex with this instance */` |
|    3893 |  577 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3893 |  578 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  579 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  580 | `			 goto Release;` |
|       - |  581 | `		 }` |
|    1944 |  582 | `	 }` |
|       - |  583 | `#endif` |
|       - |  584 | `	/* Link to the list of active engines */` |
|       - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  586 | `	/* Enter the global mutex */` |
|    3893 |  587 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  588 | `#endif` |
|    3893 |  589 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    3893 |  590 | `	sMPGlobal.nEngine++;` |
|       - |  591 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  592 | `	/* Leave the global mutex */` |
|    3893 |  593 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  594 | `#endif` |
|       - |  595 | `	/* Write a pointer to the new instance */` |
|    3893 |  596 | `	*ppEngine = pEngine;` |
|    3893 |  597 | `	return PH7_OK;` |
|     ! 0 |  598 | `Release:` |
|     ! 0 |  599 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  600 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  601 | `	return rc;` |
|    1949 |  602 | `}` |
|       - |  603 | `/*` |
|       - |  604 | ` * [CAPIREF: ph7_release()]` |
|       - |  605 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  606 | ` */` |
|    3508 |  607 | `int ph7_release(ph7 *pEngine)` |
|       5 |  608 | `{` |
|       - |  609 | `	int rc;` |
|    3513 |  610 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  611 | `		return PH7_CORRUPT;` |
|       - |  612 | `	}` |
|       - |  613 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  614 | `	 /* Acquire engine mutex */` |
|    3513 |  615 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3513 |  616 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3508 |  617 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  618 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  619 | `	 }` |
|       - |  620 | `#endif` |
|       - |  621 | `	/* Release the engine */` |
|    3513 |  622 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  623 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  624 | `	 /* Leave engine mutex */` |
|    3513 |  625 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  626 | `	 /* Release engine mutex */` |
|    3513 |  627 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  628 | `#endif` |
|       - |  629 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  630 | `	/* Enter the global mutex */` |
|    3513 |  631 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  632 | `#endif` |
|       - |  633 | `	/* Unlink from the list of active engines */` |
|    3513 |  634 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    3513 |  635 | `	sMPGlobal.nEngine--;` |
|       - |  636 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  637 | `	/* Leave the global mutex */` |
|    3513 |  638 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  639 | `#endif` |
|       - |  640 | `	/* Release the memory chunk allocated to this engine */` |
|    3513 |  641 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    3513 |  642 | `	return rc;` |
|    1759 |  643 | `}` |
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
|    3884 |  654 | `static sxi32 ProcessScript(` |
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
|    3889 |  665 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    3889 |  666 | `	if( pVm == 0 ){` |
|       - |  667 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  668 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  669 | `		if( ppVm ){` |
|     ! 0 |  670 | `			*ppVm = 0;` |
|     ! 0 |  671 | `		}` |
|     ! 0 |  672 | `		return PH7_NOMEM;` |
|       - |  673 | `	}` |
|    3889 |  674 | `	if( iFlags < 0 ){` |
|       - |  675 | `		/* Default compile-time flags */` |
|     ! 0 |  676 | `		iFlags = 0;` |
|     ! 0 |  677 | `	}` |
|       - |  678 | `	/* Initialize the Virtual Machine */` |
|    3889 |  679 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    3889 |  680 | `	if( rc != PH7_OK ){` |
|     ! 0 |  681 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  682 | `		if( ppVm ){` |
|     ! 0 |  683 | `			*ppVm = 0;` |
|     ! 0 |  684 | `		}` |
|     ! 0 |  685 | `		return PH7_VM_ERR;` |
|       - |  686 | `	}` |
|    3889 |  687 | `	if( zFilePath ){` |
|       - |  688 | `		/* Push processed file path */` |
|    3881 |  689 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    1938 |  690 | `	}` |
|       - |  691 | `	/* Reset the error message consumer */` |
|    3889 |  692 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  693 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|       - |  694 | `	{` |
|    3889 |  695 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|    3889 |  696 | `		if( SyStringLength(pScript) > nLimit ){` |
|     ! 0 |  697 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|       - |  698 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|     ! 0 |  699 | `				SyStringLength(pScript),nLimit);` |
|     ! 0 |  700 | `		}` |
|       - |  701 | `	}` |
|       - |  702 | `	/* Compile the script */` |
|    3889 |  703 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|    3889 |  704 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    1942 |  705 | `	}` |
|    3889 |  706 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     384 |  707 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  708 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     384 |  709 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     384 |  710 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     384 |  711 | `		if( ppVm ){` |
|     384 |  712 | `			*ppVm = 0;` |
|     190 |  713 | `		}` |
|     384 |  714 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  715 | `	}` |
|       - |  716 | `	/* Prepare the virtual machine for bytecode execution */` |
|    3509 |  717 | `	rc = PH7_VmMakeReady(pVm);` |
|    3509 |  718 | `	if( rc != PH7_OK ){` |
|       3 |  719 | `		goto Release;` |
|       - |  720 | `	}` |
|       - |  721 | `	/* Install local import path which is the current directory */` |
|    3507 |  722 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  723 | `#if defined(PH7_ENABLE_THREADS)` |
|    3507 |  724 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  725 | `		 /* Associate a recursive mutex with this instance */` |
|    3507 |  726 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3507 |  727 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  728 | `			 goto Release;` |
|       - |  729 | `		 }` |
|    1751 |  730 | `	 }` |
|       - |  731 | `#endif` |
|       - |  732 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    3507 |  733 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    3507 |  734 | `	pEngine->iVm++;` |
|       - |  735 | `	/* Point to the freshly created VM */` |
|    3507 |  736 | `	*ppVm = pVm;` |
|       - |  737 | `	/* Ready to execute PH7 bytecode */` |
|    3507 |  738 | `	return PH7_OK;` |
|       1 |  739 | `Release:` |
|       - |  740 | `	{` |
|       - |  741 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|       - |  742 | `		 * typed class constant whose value violates its declared type) is a compile` |
|       - |  743 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|       - |  744 | `		 * Captured before the releases free the VM. */` |
|       3 |  745 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|       3 |  746 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|       3 |  747 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       3 |  748 | `		*ppVm = 0;` |
|       3 |  749 | `		return rcRet;` |
|       - |  750 | `	}` |
|    1947 |  751 | `}` |
|       - |  752 | `/*` |
|       - |  753 | ` * [CAPIREF: ph7_compile()]` |
|       - |  754 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  755 | ` */` |
|     ! 0 |  756 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|     ! 0 |  757 | `{` |
|       - |  758 | `	SyString sScript;` |
|       - |  759 | `	int rc;` |
|     ! 0 |  760 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  761 | `		return PH7_CORRUPT;` |
|       - |  762 | `	}` |
|     ! 0 |  763 | `	if( nLen < 0 ){` |
|       - |  764 | `		/* Compute input length automatically */` |
|     ! 0 |  765 | `		nLen = (int)SyStrlen(zSource);` |
|     ! 0 |  766 | `	}` |
|     ! 0 |  767 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  768 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  769 | `	 /* Acquire engine mutex */` |
|     ! 0 |  770 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  771 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  772 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  773 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  774 | `	 }` |
|       - |  775 | `#endif` |
|       - |  776 | `	/* Compile the script */` |
|     ! 0 |  777 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|       - |  778 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  779 | `	 /* Leave engine mutex */` |
|     ! 0 |  780 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  781 | `#endif` |
|       - |  782 | `	/* Compilation result */` |
|     ! 0 |  783 | `	return rc;` |
|     ! 0 |  784 | `}` |
|       - |  785 | `/*` |
|       - |  786 | ` * [CAPIREF: ph7_compile_v2()]` |
|       - |  787 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  788 | ` */` |
|       8 |  789 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       2 |  790 | `{` |
|       - |  791 | `	SyString sScript;` |
|       - |  792 | `	int rc;` |
|      10 |  793 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  794 | `		return PH7_CORRUPT;` |
|       - |  795 | `	}` |
|      10 |  796 | `	if( nLen < 0 ){` |
|       - |  797 | `		/* Compute input length automatically */` |
|      10 |  798 | `		nLen = (int)SyStrlen(zSource);` |
|       4 |  799 | `	}` |
|      10 |  800 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  801 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  802 | `	 /* Acquire engine mutex */` |
|      10 |  803 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      10 |  804 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       8 |  805 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  806 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  807 | `	 }` |
|       - |  808 | `#endif` |
|       - |  809 | `	/* Compile the script */` |
|      10 |  810 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  811 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  812 | `	 /* Leave engine mutex */` |
|      10 |  813 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  814 | `#endif` |
|       - |  815 | `	/* Compilation result */` |
|      10 |  816 | `	return rc;` |
|       6 |  817 | `}` |
|       - |  818 | `/*` |
|       - |  819 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  820 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  821 | ` */` |
|    3876 |  822 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       5 |  823 | `{` |
|       - |  824 | `	const ph7_vfs *pVfs;` |
|       - |  825 | `	int rc;` |
|    3881 |  826 | `	if( ppOutVm ){` |
|    3881 |  827 | `		*ppOutVm = 0;` |
|    1938 |  828 | `	}` |
|    3881 |  829 | `	rc = PH7_OK; /* cc warning */` |
|    3881 |  830 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  831 | `		return PH7_CORRUPT;` |
|       - |  832 | `	}` |
|       - |  833 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  834 | `	 /* Acquire engine mutex */` |
|    3881 |  835 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3881 |  836 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3876 |  837 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  838 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  839 | `	 }` |
|       - |  840 | `#endif` |
|       - |  841 | `	 /*` |
|       - |  842 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  843 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  844 | `	  */` |
|    3881 |  845 | `	 pVfs = pEngine->pVfs;` |
|    3881 |  846 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  847 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  848 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  849 | `	 }else{` |
|    3881 |  850 | `		 void *pMapView = 0; /* cc warning */` |
|    3881 |  851 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  852 | `		 SyString sScript;` |
|       - |  853 | `		 /* Try to get a memory view of the whole file */` |
|    3881 |  854 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    3881 |  855 | `		 if( rc != PH7_OK ){` |
|       - |  856 | `			 /* Assume an IO error */` |
|     ! 0 |  857 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  858 | `		 }else{` |
|       - |  859 | `			 /* Compile the file */` |
|    3881 |  860 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    3881 |  861 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  862 | `			 /* Release the memory view of the whole file */` |
|    3881 |  863 | `			 if( pVfs->xUnmap ){` |
|    3881 |  864 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    1938 |  865 | `			 }` |
|       - |  866 | `		 }` |
|       - |  867 | `	 }` |
|       - |  868 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  869 | `	 /* Leave engine mutex */` |
|    3881 |  870 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  871 | `#endif` |
|       - |  872 | `	/* Compilation result */` |
|    3881 |  873 | `	return rc;` |
|    1943 |  874 | `}` |
|       - |  875 | `/*` |
|       - |  876 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|       - |  877 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  878 | ` */` |
|       2 |  879 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|       1 |  880 | `{` |
|       - |  881 | `	int rc;` |
|       - |  882 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       3 |  883 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  884 | `		return PH7_CORRUPT;` |
|       - |  885 | `	}` |
|       - |  886 | `#ifdef UNTRUST` |
|       - |  887 | `	if( xConsumer == 0 ){` |
|       - |  888 | `		return PH7_CORRUPT;` |
|       - |  889 | `	}` |
|       - |  890 | `#endif` |
|       - |  891 | `	/* Dump VM instructions */` |
|       3 |  892 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|       3 |  893 | `	return rc;` |
|       2 |  894 | `}` |
|       - |  895 | `/*` |
|       - |  896 | ` * [CAPIREF: ph7_vm_config()]` |
|       - |  897 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  898 | ` */` |
|   81016 |  899 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       5 |  900 | `{` |
|       - |  901 | `	va_list ap;` |
|       - |  902 | `	int rc;` |
|       - |  903 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   81021 |  904 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  905 | `		return PH7_CORRUPT;` |
|       - |  906 | `	}` |
|       - |  907 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  908 | `	 /* Acquire VM mutex */` |
|   81021 |  909 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   81021 |  910 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   81016 |  911 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  912 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  913 | `	 }` |
|       - |  914 | `#endif` |
|       - |  915 | `	/* Confiugure the virtual machine */` |
|   81021 |  916 | `	va_start(ap,iConfigOp);` |
|   81021 |  917 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   81021 |  918 | `	va_end(ap);` |
|       - |  919 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  920 | `	 /* Leave VM mutex */` |
|   81021 |  921 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  922 | `#endif` |
|   81021 |  923 | `	return rc;` |
|   40513 |  924 | `}` |
|       - |  925 | `/*` |
|       - |  926 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  927 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  928 | ` */` |
|    3506 |  929 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       5 |  930 | `{` |
|       - |  931 | `	int rc;` |
|       - |  932 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    3511 |  933 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  934 | `		return PH7_CORRUPT;` |
|       - |  935 | `	}` |
|       - |  936 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  937 | `	 /* Acquire VM mutex */` |
|    3511 |  938 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3511 |  939 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3506 |  940 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  941 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  942 | `	 }` |
|       - |  943 | `#endif` |
|       - |  944 | `	/* Execute PH7 byte-code */` |
|    3511 |  945 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    3511 |  946 | `	if( pExitStatus ){` |
|       - |  947 | `		/* Exit status */` |
|    3487 |  948 | `		*pExitStatus = pVm->iExitStatus;` |
|    1741 |  949 | `	}` |
|       - |  950 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  951 | `	 /* Leave VM mutex */` |
|    3511 |  952 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  953 | `#endif` |
|       - |  954 | `	/* Execution result */` |
|    3511 |  955 | `	return rc;` |
|    1758 |  956 | `}` |
|       - |  957 | `/*` |
|       - |  958 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  959 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  960 | ` */` |
|       6 |  961 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  962 | `{` |
|       - |  963 | `	int rc;` |
|       - |  964 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       6 |  965 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  966 | `		return PH7_CORRUPT;` |
|       - |  967 | `	}` |
|       - |  968 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  969 | `	 /* Acquire VM mutex */` |
|       6 |  970 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       6 |  971 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       6 |  972 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  973 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  974 | `	 }` |
|       - |  975 | `#endif` |
|       6 |  976 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  977 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  978 | `	 /* Leave VM mutex */` |
|       6 |  979 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  980 | `#endif` |
|       6 |  981 | `	return rc;` |
|       3 |  982 | `}` |
|       - |  983 | `/*` |
|       - |  984 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  985 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  986 | ` */` |
|    3502 |  987 | `int ph7_vm_release(ph7_vm *pVm)` |
|       5 |  988 | `{` |
|       - |  989 | `	ph7 *pEngine;` |
|       - |  990 | `	int rc;` |
|       - |  991 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    3507 |  992 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  993 | `		return PH7_CORRUPT;` |
|       - |  994 | `	}` |
|       - |  995 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  996 | `	 /* Acquire VM mutex */` |
|    3507 |  997 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3507 |  998 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3502 |  999 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1000 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1001 | `	 }` |
|       - | 1002 | `#endif` |
|    3507 | 1003 | `	pEngine = pVm->pEngine;` |
|    3507 | 1004 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - | 1005 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1006 | `	 /* Leave VM mutex */` |
|    3507 | 1007 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1008 | `#endif` |
|    3507 | 1009 | `	if( rc == PH7_OK ){` |
|       - | 1010 | `		/* Unlink from the list of active VM */` |
|       - | 1011 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1012 | `			/* Acquire engine mutex */` |
|    3507 | 1013 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3507 | 1014 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3502 | 1015 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 | 1016 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1017 | `			}` |
|       - | 1018 | `#endif` |
|    3507 | 1019 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    3507 | 1020 | `		pEngine->iVm--;` |
|       - | 1021 | `		/* Release the memory chunk allocated to this VM */` |
|    3507 | 1022 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - | 1023 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1024 | `			/* Leave engine mutex */` |
|    3507 | 1025 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1026 | `#endif` |
|    1751 | 1027 | `	}` |
|    3507 | 1028 | `	return rc;` |
|    1756 | 1029 | `}` |
|       - | 1030 | `/*` |
|       - | 1031 | ` * [CAPIREF: ph7_create_function()]` |
|       - | 1032 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1033 | ` */` |
| 1915128 | 1034 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       5 | 1035 | `{` |
|       - | 1036 | `	SyString sName;` |
|       - | 1037 | `	int rc;` |
|       - | 1038 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 1915133 | 1039 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1040 | `		return PH7_CORRUPT;` |
|       - | 1041 | `	}` |
| 1915133 | 1042 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1043 | `	/* Remove leading and trailing white spaces */` |
| 1915133 | 1044 | `	SyStringFullTrim(&sName);` |
|       - | 1045 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 1915133 | 1046 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1047 | `		return PH7_CORRUPT;` |
|       - | 1048 | `	}` |
|       - | 1049 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1050 | `	 /* Acquire VM mutex */` |
| 1915133 | 1051 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 1915133 | 1052 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 1915128 | 1053 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1054 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1055 | `	 }` |
|       - | 1056 | `#endif` |
|       - | 1057 | `	/* Install the foreign function */` |
| 1915133 | 1058 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1059 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1060 | `	 /* Leave VM mutex */` |
| 1915133 | 1061 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1062 | `#endif` |
| 1915133 | 1063 | `	return rc;` |
|  957569 | 1064 | `}` |
|       - | 1065 | `/*` |
|       - | 1066 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1067 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1068 | ` */` |
|     ! 0 | 1069 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1070 | `{` |
|     ! 0 | 1071 | `	ph7_user_func *pFunc = 0;` |
|       - | 1072 | `	int rc;` |
|       - | 1073 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1074 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1075 | `		return PH7_CORRUPT;` |
|       - | 1076 | `	}` |
|       - | 1077 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1078 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1079 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1080 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1081 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1082 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1083 | `	 }` |
|       - | 1084 | `#endif` |
|       - | 1085 | `	/* Perform the deletion */` |
|     ! 0 | 1086 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1087 | `	if( rc == PH7_OK ){` |
|       - | 1088 | `		/* Release internal fields */` |
|     ! 0 | 1089 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1090 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1091 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1092 | `	}` |
|       - | 1093 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1094 | `	 /* Leave VM mutex */` |
|     ! 0 | 1095 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1096 | `#endif` |
|     ! 0 | 1097 | `	return rc;` |
|     ! 0 | 1098 | `}` |
|       - | 1099 | `/*` |
|       - | 1100 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1101 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1102 | ` */` |
|  949580 | 1103 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       5 | 1104 | `{` |
|       - | 1105 | `	SyString sName;` |
|       - | 1106 | `	int rc;` |
|       - | 1107 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  949585 | 1108 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1109 | `		return PH7_CORRUPT;` |
|       - | 1110 | `	}` |
|  949585 | 1111 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1112 | `	/* Remove leading and trailing white spaces */` |
|  953089 | 1113 | `	SyStringFullTrim(&sName);` |
|  949585 | 1114 | `	if( sName.nByte < 1 ){` |
|       - | 1115 | `		/* Empty constant name */` |
|     ! 0 | 1116 | `		return PH7_CORRUPT;` |
|       - | 1117 | `	}` |
|       - | 1118 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  949585 | 1119 | `	if( xExpand == 0 ){` |
|     ! 0 | 1120 | `		return PH7_CORRUPT;` |
|       - | 1121 | `	}` |
|       - | 1122 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1123 | `	 /* Acquire VM mutex */` |
|  949585 | 1124 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  949585 | 1125 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  949580 | 1126 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1127 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1128 | `	 }` |
|       - | 1129 | `#endif` |
|       - | 1130 | `	/* Perform the registration */` |
|  949585 | 1131 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1132 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1133 | `	 /* Leave VM mutex */` |
|  949585 | 1134 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1135 | `#endif` |
|  949585 | 1136 | `	 return rc;` |
|  474795 | 1137 | `}` |
|       - | 1138 | `/*` |
|       - | 1139 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1140 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1141 | ` */` |
|     ! 0 | 1142 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1143 | `{` |
|       - | 1144 | `	ph7_constant *pCons;` |
|       - | 1145 | `	int rc;` |
|       - | 1146 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1147 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1148 | `		return PH7_CORRUPT;` |
|       - | 1149 | `	}` |
|       - | 1150 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1151 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1152 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1153 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1154 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1155 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1156 | `	 }` |
|       - | 1157 | `#endif` |
|       - | 1158 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1159 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1160 | `	 if( rc == PH7_OK ){` |
|       - | 1161 | `		 /* Perform the deletion */` |
|     ! 0 | 1162 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1163 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1164 | `	 }` |
|       - | 1165 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1166 | `	 /* Leave VM mutex */` |
|     ! 0 | 1167 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1168 | `#endif` |
|     ! 0 | 1169 | `	return rc;` |
|     ! 0 | 1170 | `}` |
|       - | 1171 | `/*` |
|       - | 1172 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1173 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1174 | ` */` |
|   99866 | 1175 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       5 | 1176 | `{` |
|       - | 1177 | `	ph7_value *pObj;` |
|       - | 1178 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   99871 | 1179 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1180 | `		return 0;` |
|       - | 1181 | `	}` |
|       - | 1182 | `	/* Allocate a new scalar variable */` |
|   99871 | 1183 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   99871 | 1184 | `	if( pObj == 0 ){` |
|     ! 0 | 1185 | `		return 0;` |
|       - | 1186 | `	}` |
|       - | 1187 | `	/* Nullify the new scalar */` |
|   99871 | 1188 | `	PH7_MemObjInit(pVm,pObj);` |
|   99871 | 1189 | `	return pObj;` |
|   49938 | 1190 | `}` |
|       - | 1191 | `/*` |
|       - | 1192 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1193 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1194 | ` */` |
|   70324 | 1195 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       5 | 1196 | `{` |
|       - | 1197 | `	ph7_hashmap *pMap;` |
|       - | 1198 | `	ph7_value *pObj;` |
|       - | 1199 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   70329 | 1200 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1201 | `		return 0;` |
|       - | 1202 | `	}` |
|       - | 1203 | `	/* Create a new hashmap first */` |
|   70329 | 1204 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   70329 | 1205 | `	if( pMap == 0 ){` |
|     ! 0 | 1206 | `		return 0;` |
|       - | 1207 | `	}` |
|       - | 1208 | `	/* Associate a new ph7_value with this hashmap */` |
|   70329 | 1209 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   70329 | 1210 | `	if( pObj == 0 ){` |
|     ! 0 | 1211 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1212 | `		return 0;` |
|       - | 1213 | `	}` |
|   70329 | 1214 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   70329 | 1215 | `	return pObj;` |
|   35167 | 1216 | `}` |
|       - | 1217 | `/*` |
|       - | 1218 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1219 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1220 | ` */` |
|   38582 | 1221 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1222 | `{` |
|       - | 1223 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   38587 | 1224 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1225 | `		return PH7_CORRUPT;` |
|       - | 1226 | `	}` |
|   38587 | 1227 | `	if( pValue ){` |
|       - | 1228 | `		/* Release the value */` |
|   38587 | 1229 | `		PH7_MemObjRelease(pValue);` |
|   38587 | 1230 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|   19291 | 1231 | `	}` |
|   38587 | 1232 | `	return PH7_OK;` |
|   19296 | 1233 | `}` |
|       - | 1234 | `/*` |
|       - | 1235 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1236 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1237 | ` */` |
|  415112 | 1238 | `int ph7_value_to_int(ph7_value *pValue)` |
|       5 | 1239 | `{` |
|       - | 1240 | `	int rc;` |
|  415117 | 1241 | `	rc = PH7_MemObjToInteger(pValue);` |
|  415117 | 1242 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1243 | `		return 0;` |
|       - | 1244 | `	}` |
|  415117 | 1245 | `	return (int)pValue->x.iVal;` |
|  207561 | 1246 | `}` |
|       - | 1247 | `/*` |
|       - | 1248 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1249 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1250 | ` */` |
|    1160 | 1251 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       5 | 1252 | `{` |
|       - | 1253 | `	int rc;` |
|    1165 | 1254 | `	rc = PH7_MemObjToBool(pValue);` |
|    1165 | 1255 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1256 | `		return 0;` |
|       - | 1257 | `	}` |
|    1165 | 1258 | `	return (int)pValue->x.iVal;` |
|     585 | 1259 | `}` |
|       - | 1260 | `/*` |
|       - | 1261 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1262 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1263 | ` */` |
|   25956 | 1264 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       5 | 1265 | `{` |
|       - | 1266 | `	int rc;` |
|   25961 | 1267 | `	rc = PH7_MemObjToInteger(pValue);` |
|   25961 | 1268 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1269 | `		return 0;` |
|       - | 1270 | `	}` |
|   25961 | 1271 | `	return pValue->x.iVal;` |
|   12983 | 1272 | `}` |
|       - | 1273 | `/*` |
|       - | 1274 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1275 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1276 | ` */` |
|    1188 | 1277 | `double ph7_value_to_double(ph7_value *pValue)` |
|       1 | 1278 | `{` |
|       - | 1279 | `	int rc;` |
|    1189 | 1280 | `	rc = PH7_MemObjToReal(pValue);` |
|    1189 | 1281 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1282 | `		return (double)0;` |
|       - | 1283 | `	}` |
|    1189 | 1284 | `	return (double)pValue->rVal;` |
|     595 | 1285 | `}` |
|       - | 1286 | `/*` |
|       - | 1287 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1288 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1289 | ` */` |
|  799247 | 1290 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       5 | 1291 | `{` |
|  799252 | 1292 | `	PH7_MemObjToString(pValue);` |
|  799252 | 1293 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  766422 | 1294 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  766422 | 1295 | `		if( pLen ){` |
|  705306 | 1296 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  352693 | 1297 | `		}` |
|  766422 | 1298 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1299 | `	}else{` |
|       - | 1300 | `		/* Return the empty string */` |
|   32835 | 1301 | `		if( pLen ){` |
|   32825 | 1302 | `			*pLen = 0;` |
|   16410 | 1303 | `		}` |
|   32835 | 1304 | `		return "";` |
|       - | 1305 | `	}` |
|  399671 | 1306 | `}` |
|       - | 1307 | `/*` |
|       - | 1308 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1309 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1310 | ` */` |
|   30842 | 1311 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       5 | 1312 | `{` |
|   30847 | 1313 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1314 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1315 | `		return 0;` |
|       - | 1316 | `	}` |
|   30847 | 1317 | `	return pValue->x.pOther;` |
|   15426 | 1318 | `}` |
|       - | 1319 | `/*` |
|       - | 1320 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1321 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1322 | ` */` |
|      64 | 1323 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1324 | `{` |
|       - | 1325 | `	int rc;` |
|      65 | 1326 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1327 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1328 | `		return 1;` |
|       - | 1329 | `	}` |
|       - | 1330 | `	/* Perform the comparison */` |
|      65 | 1331 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1332 | `	/* Comparison result */` |
|      65 | 1333 | `	return rc;` |
|      33 | 1334 | `}` |
|       - | 1335 | `/*` |
|       - | 1336 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1337 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1338 | ` */` |
|   17424 | 1339 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       5 | 1340 | `{` |
|   17429 | 1341 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       5 | 1342 | `}` |
|       - | 1343 | `/*` |
|       - | 1344 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1345 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1346 | ` */` |
|   18888 | 1347 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       5 | 1348 | `{` |
|   18893 | 1349 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       5 | 1350 | `}` |
|       - | 1351 | `/*` |
|       - | 1352 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1353 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1354 | ` */` |
|  364871 | 1355 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       5 | 1356 | `{` |
|  364876 | 1357 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       5 | 1358 | `}` |
|       - | 1359 | `/*` |
|       - | 1360 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1361 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1362 | ` */` |
|     612 | 1363 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       1 | 1364 | `{` |
|     613 | 1365 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       1 | 1366 | `}` |
|       - | 1367 | `/*` |
|       - | 1368 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1369 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1370 | ` */` |
|     360 | 1371 | `int ph7_result_null(ph7_context *pCtx)` |
|       5 | 1372 | `{` |
|       - | 1373 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     365 | 1374 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     365 | 1375 | `	return PH7_OK;` |
|       5 | 1376 | `}` |
|       - | 1377 | `/*` |
|       - | 1378 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1379 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1380 | ` */` |
| 1196430 | 1381 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1382 | `{` |
| 1196435 | 1383 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       5 | 1384 | `}` |
|       - | 1385 | `/*` |
|       - | 1386 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1387 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1388 | ` */` |
|     384 | 1389 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       4 | 1390 | `{` |
|       - | 1391 | `	ph7_value *p;` |
|       - | 1392 | `	va_list ap;` |
|       - | 1393 | `	int rc;` |
|     388 | 1394 | `	p = pCtx->pRet;` |
|     388 | 1395 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1396 | `		/* Invalidate any prior representation */` |
|     165 | 1397 | `		PH7_MemObjRelease(p);` |
|     165 | 1398 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|      81 | 1399 | `	}` |
|       - | 1400 | `	/* Format the given string */` |
|     388 | 1401 | `	va_start(ap,zFormat);` |
|     388 | 1402 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|     388 | 1403 | `	va_end(ap);` |
|     388 | 1404 | `	return rc;` |
|       4 | 1405 | `}` |
|       - | 1406 | `/*` |
|       - | 1407 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1408 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1409 | ` */` |
|   36412 | 1410 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1411 | `{` |
|   36417 | 1412 | `	int rc = PH7_OK;` |
|   36417 | 1413 | `	if( pValue == 0 ){` |
|     ! 0 | 1414 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1415 | `	}else{` |
|   36417 | 1416 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1417 | `	}` |
|   36417 | 1418 | `	return rc;` |
|       5 | 1419 | `}` |
|       - | 1420 | `/*` |
|       - | 1421 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1422 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1423 | ` */` |
|    5134 | 1424 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       5 | 1425 | `{` |
|    5139 | 1426 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       5 | 1427 | `}` |
|       - | 1428 | `/*` |
|       - | 1429 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1430 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1431 | ` */` |
|   96384 | 1432 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       5 | 1433 | `{` |
|       - | 1434 | `	ph7_value *pVal;` |
|   96389 | 1435 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|   96389 | 1436 | `	if( pVal ){` |
|       - | 1437 | `		/* Record value address so it can be freed automatically` |
|       - | 1438 | `		 * when the calling function returns.` |
|       - | 1439 | `		 */` |
|   96389 | 1440 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   48192 | 1441 | `	}` |
|   96389 | 1442 | `	return pVal;` |
|       5 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1446 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1447 | ` */` |
|   35224 | 1448 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       5 | 1449 | `{` |
|       - | 1450 | `	ph7_value *pVal;` |
|   35229 | 1451 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   35229 | 1452 | `	if( pVal ){` |
|       - | 1453 | `		/* Record value address so it can be freed automatically` |
|       - | 1454 | `		 * when the calling function returns.` |
|       - | 1455 | `		 */` |
|   35229 | 1456 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   17612 | 1457 | `	}` |
|   35229 | 1458 | `	return pVal;` |
|       5 | 1459 | `}` |
|       - | 1460 | `/*` |
|       - | 1461 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1462 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1463 | ` */` |
|     486 | 1464 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1465 | `{` |
|     491 | 1466 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     491 | 1467 | `}` |
|       - | 1468 | `/*` |
|       - | 1469 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1470 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1471 | ` */` |
|    5156 | 1472 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       5 | 1473 | `{` |
|       - | 1474 | `	void *pChunk;` |
|    5161 | 1475 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    5161 | 1476 | `	if( pChunk ){` |
|    5161 | 1477 | `		if( ZeroChunk ){` |
|       - | 1478 | `			/* Zero the memory chunk */` |
|    5077 | 1479 | `			SyZero(pChunk,nByte);` |
|    2536 | 1480 | `		}` |
|    5161 | 1481 | `		if( AutoRelease ){` |
|       - | 1482 | `			ph7_aux_data sAux;` |
|       - | 1483 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1484 | `			 * upon this context is destroyed.` |
|       - | 1485 | `			 */` |
|      75 | 1486 | `			sAux.pAuxData = pChunk;` |
|      75 | 1487 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|      37 | 1488 | `		}` |
|    2578 | 1489 | `	}` |
|    5161 | 1490 | `	return pChunk;` |
|       5 | 1491 | `}` |
|       - | 1492 | `/*` |
|       - | 1493 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1494 | ` * chunk container.` |
|       - | 1495 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1496 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1497 | ` */` |
|    5050 | 1498 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1499 | `{` |
|       - | 1500 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1501 | `	sxu32 n;` |
|    5055 | 1502 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1503 | `		/* Don't bother processing,the container is empty */` |
|    5055 | 1504 | `		return 0;` |
|       - | 1505 | `	}` |
|       - | 1506 | `	/* Perform the lookup */` |
|     ! 0 | 1507 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1508 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1509 | `		pAux = &aAux[n];` |
|     ! 0 | 1510 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1511 | `			/* Chunk found */` |
|     ! 0 | 1512 | `			return pAux;` |
|       - | 1513 | `		}` |
|     ! 0 | 1514 | `	}` |
|       - | 1515 | `	/* No such allocated chunk */` |
|     ! 0 | 1516 | `	return 0;` |
|    2530 | 1517 | `}` |
|       - | 1518 | `/*` |
|       - | 1519 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1520 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1521 | ` */` |
|     ! 0 | 1522 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1523 | `{` |
|       - | 1524 | `	ph7_aux_data *pAux;` |
|       - | 1525 | `	void *pNew;` |
|     ! 0 | 1526 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1527 | `	if( pNew ){` |
|     ! 0 | 1528 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1529 | `		if( pAux ){` |
|     ! 0 | 1530 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1531 | `		}` |
|     ! 0 | 1532 | `	}` |
|     ! 0 | 1533 | `	return pNew;` |
|     ! 0 | 1534 | `}` |
|       - | 1535 | `/*` |
|       - | 1536 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1537 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1538 | ` */` |
|    5050 | 1539 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1540 | `{` |
|       - | 1541 | `	ph7_aux_data *pAux;` |
|    5055 | 1542 | `	if( pChunk == 0 ){` |
|       - | 1543 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1544 | `		return;` |
|       - | 1545 | `	}` |
|    5055 | 1546 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    5055 | 1547 | `	if( pAux ){` |
|       - | 1548 | `		/* Mark as destroyed */` |
|     ! 0 | 1549 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1550 | `	}` |
|    5055 | 1551 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    2530 | 1552 | `}` |
|       - | 1553 | `/*` |
|       - | 1554 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1555 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1556 | ` */` |
|     152 | 1557 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|       3 | 1558 | `{` |
|       - | 1559 | `	ph7_hashmap_node *pNode;` |
|       - | 1560 | `	ph7_value *pValue;` |
|       - | 1561 | `	ph7_value skey;` |
|       - | 1562 | `	int rc;` |
|       - | 1563 | `	/* Make sure we are dealing with a valid hashmap */` |
|     155 | 1564 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1565 | `		return 0;` |
|       - | 1566 | `	}` |
|     155 | 1567 | `	if( nByte < 0 ){` |
|     ! 0 | 1568 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1569 | `	}` |
|       - | 1570 | `	/* Convert the key to a ph7_value  */` |
|     155 | 1571 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     155 | 1572 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1573 | `	/* Perform the lookup */` |
|     155 | 1574 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     155 | 1575 | `	PH7_MemObjRelease(&skey);` |
|     155 | 1576 | `	if( rc != PH7_OK ){` |
|       - | 1577 | `		/* No such entry */` |
|      64 | 1578 | `		return 0;` |
|       - | 1579 | `	}` |
|       - | 1580 | `	/* Extract the target value */` |
|      93 | 1581 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|      93 | 1582 | `	return pValue;` |
|      79 | 1583 | `}` |
|       - | 1584 | `/*` |
|       - | 1585 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1586 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1587 | ` */` |
|   33850 | 1588 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       5 | 1589 | `{` |
|       - | 1590 | `	int rc;` |
|   33855 | 1591 | `	if( xWalk == 0 ){` |
|     ! 0 | 1592 | `		return PH7_CORRUPT;` |
|       - | 1593 | `	}` |
|       - | 1594 | `	/* Make sure we are dealing with a valid hashmap */` |
|   33855 | 1595 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1596 | `		return PH7_CORRUPT;` |
|       - | 1597 | `	}` |
|       - | 1598 | `	/* Start the walk process */` |
|   33855 | 1599 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   33855 | 1600 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   16930 | 1601 | `}` |
|       - | 1602 | `/*` |
|       - | 1603 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1604 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1605 | ` */` |
|  205510 | 1606 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       5 | 1607 | `{` |
|       - | 1608 | `	int rc;` |
|       - | 1609 | `	/* Make sure we are dealing with a valid hashmap */` |
|  205515 | 1610 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1611 | `		return PH7_CORRUPT;` |
|       - | 1612 | `	}` |
|       - | 1613 | `	/* Perform the insertion */` |
|  205515 | 1614 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
|  205515 | 1615 | `	return rc;` |
|  102760 | 1616 | `}` |
|       - | 1617 | `/*` |
|       - | 1618 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1619 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1620 | ` */` |
|  106334 | 1621 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       5 | 1622 | `{` |
|       - | 1623 | `	int rc;` |
|       - | 1624 | `	/* Make sure we are dealing with a valid hashmap */` |
|  106339 | 1625 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1626 | `		return PH7_CORRUPT;` |
|       - | 1627 | `	}` |
|       - | 1628 | `	/* Perform the insertion */` |
|  106339 | 1629 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1630 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1631 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1632 | `	}else{` |
|       - | 1633 | `		ph7_value sKey;` |
|  106339 | 1634 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|  106339 | 1635 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|  106339 | 1636 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  106339 | 1637 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1638 | `	}` |
|  106339 | 1639 | `	return rc;` |
|   53172 | 1640 | `}` |
|       - | 1641 | `/*` |
|       - | 1642 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1643 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1644 | ` */` |
| 2117756 | 1645 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       5 | 1646 | `{` |
|       - | 1647 | `	ph7_value sKey;` |
|       - | 1648 | `	int rc;` |
|       - | 1649 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2117761 | 1650 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1651 | `		return PH7_CORRUPT;` |
|       - | 1652 | `	}` |
| 2117761 | 1653 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1654 | `	/* Perform the insertion */` |
| 2117761 | 1655 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
| 2117761 | 1656 | `	PH7_MemObjRelease(&sKey);` |
| 2117761 | 1657 | `	return rc;` |
| 1058883 | 1658 | `}` |
|       - | 1659 | `/*` |
|       - | 1660 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1661 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1662 | ` */` |
|  150804 | 1663 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       5 | 1664 | `{` |
|       - | 1665 | `	ph7_hashmap *pMap;` |
|       - | 1666 | `	/* Make sure we are dealing with a valid hashmap */` |
|  150809 | 1667 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1668 | `		return 0;` |
|       - | 1669 | `	}` |
|       - | 1670 | `	/* Point to the internal representation of the hashmap */` |
|  150809 | 1671 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  150809 | 1672 | `	return pMap->nEntry;` |
|   75407 | 1673 | `}` |
|       - | 1674 | `/*` |
|       - | 1675 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1676 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1677 | ` */` |
|     ! 0 | 1678 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|     ! 0 | 1679 | `{` |
|       - | 1680 | `	int rc;` |
|     ! 0 | 1681 | `	if( xWalk == 0 ){` |
|     ! 0 | 1682 | `		return PH7_CORRUPT;` |
|       - | 1683 | `	}` |
|       - | 1684 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1685 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1686 | `		return PH7_CORRUPT;` |
|       - | 1687 | `	}` |
|       - | 1688 | `	/* Start the walk process */` |
|     ! 0 | 1689 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|     ! 0 | 1690 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|     ! 0 | 1691 | `}` |
|       - | 1692 | `/*` |
|       - | 1693 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1694 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1695 | ` */` |
|       8 | 1696 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1697 | `{` |
|       - | 1698 | `	ph7_value *pValue;` |
|       - | 1699 | `	SyString sAttr;` |
|       - | 1700 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1701 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1702 | `		return 0;` |
|       - | 1703 | `	}` |
|       9 | 1704 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1705 | `	/* Extract the attribute value if available.` |
|       - | 1706 | `	 */` |
|       9 | 1707 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1708 | `	return pValue;` |
|       5 | 1709 | `}` |
|       - | 1710 | `/*` |
|       - | 1711 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1712 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1713 | ` */` |
|     ! 0 | 1714 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1715 | `{` |
|       - | 1716 | `	ph7_class *pClass;` |
|     ! 0 | 1717 | `	if( pLength ){` |
|     ! 0 | 1718 | `		*pLength = 0;` |
|     ! 0 | 1719 | `	}` |
|       - | 1720 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1721 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1722 | `		return 0;` |
|       - | 1723 | `	}` |
|       - | 1724 | `	/* Point to the class */` |
|     ! 0 | 1725 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1726 | `	/* Return the class name */` |
|     ! 0 | 1727 | `	if( pLength ){` |
|     ! 0 | 1728 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1729 | `	}` |
|     ! 0 | 1730 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1731 | `}` |
|       - | 1732 | `/*` |
|       - | 1733 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1734 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1735 | ` */` |
|    1656 | 1736 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1737 | `{` |
|       - | 1738 | `	SyString sData;` |
|       - | 1739 | `	int rc;` |
|    1661 | 1740 | `	if( nLen < 0 ){` |
|     ! 0 | 1741 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1742 | `	}` |
|    1661 | 1743 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|    1661 | 1744 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|    1661 | 1745 | `	return rc;` |
|       5 | 1746 | `}` |
|       - | 1747 | `/*` |
|       - | 1748 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1749 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1750 | ` */` |
|       2 | 1751 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1752 | `{` |
|       - | 1753 | `	va_list ap;` |
|       - | 1754 | `	int rc;` |
|       3 | 1755 | `	va_start(ap,zFormat);` |
|       3 | 1756 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1757 | `	va_end(ap);` |
|       3 | 1758 | `	return rc;` |
|       1 | 1759 | `}` |
|       - | 1760 | `/*` |
|       - | 1761 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1762 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1763 | ` */` |
|      22 | 1764 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       5 | 1765 | `{` |
|      27 | 1766 | `	int rc = PH7_OK;` |
|      27 | 1767 | `	if( zErr ){` |
|      27 | 1768 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      11 | 1769 | `	}` |
|      27 | 1770 | `	return rc;` |
|       5 | 1771 | `}` |
|       - | 1772 | `/*` |
|       - | 1773 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1774 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1775 | ` */` |
|      40 | 1776 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       4 | 1777 | `{` |
|       - | 1778 | `	va_list ap;` |
|       - | 1779 | `	int rc;` |
|      44 | 1780 | `	if( zFormat == 0){` |
|     ! 0 | 1781 | `		return PH7_OK;` |
|       - | 1782 | `	}` |
|      44 | 1783 | `	va_start(ap,zFormat);` |
|      44 | 1784 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      44 | 1785 | `	va_end(ap);` |
|      44 | 1786 | `	return rc;` |
|      24 | 1787 | `}` |
|       - | 1788 | `/*` |
|       - | 1789 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1790 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1791 | ` */` |
|      34 | 1792 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1793 | `{` |
|       - | 1794 | `	sxu32 n;` |
|      35 | 1795 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1796 | `	return n;` |
|       1 | 1797 | `}` |
|       - | 1798 | `/*` |
|       - | 1799 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1800 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1801 | ` */` |
|     ! 0 | 1802 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1803 | `{` |
|     ! 0 | 1804 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1805 | `		return PH7_CORRUPT;` |
|       - | 1806 | `	}` |
|     ! 0 | 1807 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1808 | `	return PH7_OK;` |
|     ! 0 | 1809 | `}` |
|       - | 1810 | `/*` |
|       - | 1811 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1812 | ` *` |
|       - | 1813 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1814 | ` * {` |
|       - | 1815 | ` *	return pCtx->pVm;` |
|       - | 1816 | ` * }` |
|       - | 1817 | ` */` |
|       - | 1818 | `/*` |
|       - | 1819 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1820 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1821 | ` */` |
|   61394 | 1822 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       5 | 1823 | `{` |
|   61399 | 1824 | `	return pCtx->pFunc->pUserData;` |
|       5 | 1825 | `}` |
|       - | 1826 | `/*` |
|       - | 1827 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1828 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1829 | ` */` |
|       2 | 1830 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1831 | `{` |
|       - | 1832 | `	ph7_aux_data sAux;` |
|       - | 1833 | `	int rc;` |
|       3 | 1834 | `	sAux.pAuxData = pUserData;` |
|       3 | 1835 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1836 | `	return rc;` |
|       1 | 1837 | `}` |
|       - | 1838 | `/*` |
|       - | 1839 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1840 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1841 | ` */` |
|       4 | 1842 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1843 | `{` |
|       - | 1844 | `	ph7_aux_data *pAux;` |
|       5 | 1845 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       5 | 1846 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1847 | `}` |
|       - | 1848 | `/*` |
|       - | 1849 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1850 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1851 | ` */` |
|     ! 0 | 1852 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|     ! 0 | 1853 | `{` |
|       - | 1854 | `	ph7_aux_data *pAux;` |
|     ! 0 | 1855 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|     ! 0 | 1856 | `	return pAux ? pAux->pAuxData : 0;` |
|     ! 0 | 1857 | `}` |
|       - | 1858 | `/*` |
|       - | 1859 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1860 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1861 | ` */` |
|    6704 | 1862 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       5 | 1863 | `{` |
|    6709 | 1864 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       5 | 1865 | `}` |
|       - | 1866 | `/*` |
|       - | 1867 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1868 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1869 | ` */` |
|   29590 | 1870 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       5 | 1871 | `{` |
|       - | 1872 | `	SyString *pName;` |
|   29595 | 1873 | `	pName = &pCtx->pFunc->sName;` |
|   29595 | 1874 | `	return pName->zString;` |
|       5 | 1875 | `}` |
|       - | 1876 | `/*` |
|       - | 1877 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1878 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1879 | ` */` |
|   40256 | 1880 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       5 | 1881 | `{` |
|       - | 1882 | `	/* Invalidate any prior representation */` |
|   40261 | 1883 | `	PH7_MemObjRelease(pVal);` |
|   40261 | 1884 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|   40261 | 1885 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   40261 | 1886 | `	return PH7_OK;` |
|       5 | 1887 | `}` |
|       - | 1888 | `/*` |
|       - | 1889 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1890 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1891 | ` */` |
|   38530 | 1892 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       5 | 1893 | `{` |
|       - | 1894 | `	/* Invalidate any prior representation */` |
|   38535 | 1895 | `	PH7_MemObjRelease(pVal);` |
|   38535 | 1896 | `	pVal->x.iVal = iValue;` |
|   38535 | 1897 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   38535 | 1898 | `	return PH7_OK;` |
|       5 | 1899 | `}` |
|       - | 1900 | `/*` |
|       - | 1901 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1902 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1903 | ` */` |
|  410963 | 1904 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       5 | 1905 | `{` |
|       - | 1906 | `	/* Invalidate any prior representation */` |
|  410968 | 1907 | `	PH7_MemObjRelease(pVal);` |
|  410968 | 1908 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  410968 | 1909 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  410968 | 1910 | `	return PH7_OK;` |
|       5 | 1911 | `}` |
|       - | 1912 | `/*` |
|       - | 1913 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1914 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1915 | ` */` |
|    4358 | 1916 | `int ph7_value_null(ph7_value *pVal)` |
|       1 | 1917 | `{` |
|       - | 1918 | `	/* Invalidate any prior representation and set the NULL flag */` |
|    4359 | 1919 | `	PH7_MemObjRelease(pVal);` |
|    4359 | 1920 | `	return PH7_OK;` |
|       1 | 1921 | `}` |
|       - | 1922 | `/*` |
|       - | 1923 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1924 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1925 | ` */` |
|     894 | 1926 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       1 | 1927 | `{` |
|       - | 1928 | `	/* Invalidate any prior representation */` |
|     895 | 1929 | `	PH7_MemObjRelease(pVal);` |
|     895 | 1930 | `	pVal->rVal = (ph7_real)Value;` |
|     895 | 1931 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1932 | `	/* Try to get an integer representation also */` |
|     895 | 1933 | `	PH7_MemObjTryInteger(pVal);` |
|     895 | 1934 | `	return PH7_OK;` |
|       1 | 1935 | `}` |
|       - | 1936 | `/*` |
|       - | 1937 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1938 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1939 | ` */` |
| 1392390 | 1940 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       5 | 1941 | `{` |
| 1392395 | 1942 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1943 | `		/* Invalidate any prior representation */` |
|  433383 | 1944 | `		PH7_MemObjRelease(pVal);` |
|  433383 | 1945 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  216689 | 1946 | `	}` |
| 1392395 | 1947 | `	if( zString ){` |
| 1391591 | 1948 | `		if( nLen < 0 ){` |
|       - | 1949 | `			/* Compute length automatically */` |
|    5139 | 1950 | `			nLen = (int)SyStrlen(zString);` |
|    2567 | 1951 | `		}` |
|       - | 1952 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1953 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
| 1391591 | 1954 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1955 | `	}` |
|     805 | 1956 | `	return PH7_OK;` |
|  696200 | 1957 | `}` |
|       - | 1958 | `/*` |
|       - | 1959 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1960 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1961 | ` */` |
|      22 | 1962 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1963 | `{` |
|       - | 1964 | `	va_list ap;` |
|       - | 1965 | `	int rc;` |
|      23 | 1966 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1967 | `		/* Invalidate any prior representation */` |
|      19 | 1968 | `		PH7_MemObjRelease(pVal);` |
|      19 | 1969 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       9 | 1970 | `	}` |
|      23 | 1971 | `	va_start(ap,zFormat);` |
|      23 | 1972 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      23 | 1973 | `	va_end(ap);` |
|       - | 1974 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|      23 | 1975 | `	return rc;` |
|       1 | 1976 | `}` |
|       - | 1977 | `/*` |
|       - | 1978 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1979 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1980 | ` */` |
|  163168 | 1981 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       5 | 1982 | `{` |
|       - | 1983 | `	/* Reset the string cursor */` |
|  163173 | 1984 | `	SyBlobReset(&pVal->sBlob);` |
|  163173 | 1985 | `	return PH7_OK;` |
|       5 | 1986 | `}` |
|       - | 1987 | `/*` |
|       - | 1988 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1990 | ` */` |
|    5224 | 1991 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       5 | 1992 | `{` |
|       - | 1993 | `	/* Invalidate any prior representation */` |
|    5229 | 1994 | `	PH7_MemObjRelease(pVal);` |
|       - | 1995 | `	/* Reflect the new type */` |
|    5229 | 1996 | `	pVal->x.pOther = pUserData;` |
|    5229 | 1997 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    5229 | 1998 | `	return PH7_OK;` |
|       5 | 1999 | `}` |
|       - | 2000 | `/*` |
|       - | 2001 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 2002 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2003 | ` */` |
|    4018 | 2004 | `int ph7_value_release(ph7_value *pVal)` |
|       5 | 2005 | `{` |
|    4023 | 2006 | `	PH7_MemObjRelease(pVal);` |
|    4023 | 2007 | `	return PH7_OK;` |
|       5 | 2008 | `}` |
|       - | 2009 | `/*` |
|       - | 2010 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 2011 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2012 | ` */` |
|   15390 | 2013 | `int ph7_value_is_int(ph7_value *pVal)` |
|       5 | 2014 | `{` |
|       - | 2015 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 2016 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 2017 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 2018 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 2019 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|   15395 | 2020 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       5 | 2021 | `}` |
|       - | 2022 | `/*` |
|       - | 2023 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 2024 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2025 | ` */` |
|    9820 | 2026 | `int ph7_value_is_float(ph7_value *pVal)` |
|       5 | 2027 | `{` |
|    9825 | 2028 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       5 | 2029 | `}` |
|       - | 2030 | `/*` |
|       - | 2031 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 2032 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2033 | ` */` |
|    2668 | 2034 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       5 | 2035 | `{` |
|    2673 | 2036 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       5 | 2037 | `}` |
|       - | 2038 | `/*` |
|       - | 2039 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2040 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2041 | ` */` |
|  109388 | 2042 | `int ph7_value_is_string(ph7_value *pVal)` |
|       5 | 2043 | `{` |
|  109393 | 2044 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       5 | 2045 | `}` |
|       - | 2046 | `/*` |
|       - | 2047 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2048 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2049 | ` */` |
|    4594 | 2050 | `int ph7_value_is_null(ph7_value *pVal)` |
|       5 | 2051 | `{` |
|    4599 | 2052 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       5 | 2053 | `}` |
|       - | 2054 | `/*` |
|       - | 2055 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2056 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2057 | ` */` |
|    1078 | 2058 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       5 | 2059 | `{` |
|       - | 2060 | `	int rc;` |
|    1083 | 2061 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|    1083 | 2062 | `	return rc;` |
|       5 | 2063 | `}` |
|       - | 2064 | `/*` |
|       - | 2065 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2066 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2067 | ` */` |
|   35924 | 2068 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       5 | 2069 | `{` |
|       - | 2070 | `	int rc;` |
|   35929 | 2071 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   35929 | 2072 | `	return rc;` |
|       5 | 2073 | `}` |
|       - | 2074 | `/*` |
|       - | 2075 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2076 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2077 | ` */` |
|      12 | 2078 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2079 | `{` |
|      13 | 2080 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2081 | `}` |
|       - | 2082 | `/*` |
|       - | 2083 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2084 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2085 | ` */` |
|  171198 | 2086 | `int ph7_value_is_array(ph7_value *pVal)` |
|       5 | 2087 | `{` |
|  171203 | 2088 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       5 | 2089 | `}` |
|       - | 2090 | `/*` |
|       - | 2091 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2092 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2093 | ` */` |
|    6736 | 2094 | `int ph7_value_is_object(ph7_value *pVal)` |
|       5 | 2095 | `{` |
|    6741 | 2096 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       5 | 2097 | `}` |
|       - | 2098 | `/*` |
|       - | 2099 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2100 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2101 | ` */` |
|   34316 | 2102 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       5 | 2103 | `{` |
|   34321 | 2104 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       5 | 2105 | `}` |
|       - | 2106 | `/*` |
|       - | 2107 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2108 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2109 | ` */` |
|   34186 | 2110 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       5 | 2111 | `{` |
|       - | 2112 | `	int rc;` |
|   34191 | 2113 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   34191 | 2114 | `	return rc;` |
|       5 | 2115 | `}` |
|       - | 2116 | `/*` |
|       - | 2117 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2118 | ` * Check if a value holds a Fiber instance.` |
|       - | 2119 | ` */` |
|     ! 0 | 2120 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2121 | `{` |
|     ! 0 | 2122 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2123 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2124 | `}` |
|       - | 2125 | `/*` |
|       - | 2126 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2127 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2128 | ` */` |
|     ! 0 | 2129 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2130 | `{` |
|     ! 0 | 2131 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2132 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2133 | `}` |
|       - | 2134 | `/*` |
|       - | 2135 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2136 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2137 | ` */` |
|     ! 0 | 2138 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2139 | `{` |
|     ! 0 | 2140 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2141 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2142 | `}` |
|       - | 2143 | `/*` |
|       - | 2144 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2145 | ` * Check if a Fiber is currently suspended.` |
|       - | 2146 | ` */` |
|     ! 0 | 2147 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2148 | `{` |
|     ! 0 | 2149 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2150 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2151 | `}` |
|       - | 2152 | `/*` |
|       - | 2153 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2154 | ` * Check if a Fiber has completed execution.` |
|       - | 2155 | ` */` |
|     ! 0 | 2156 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2157 | `{` |
|     ! 0 | 2158 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2159 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2160 | `}` |
|       - | 2161 | `/*` |
|       - | 2162 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2163 | ` * Get the return value of a terminated Fiber.` |
|       - | 2164 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2165 | ` */` |
|     ! 0 | 2166 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2167 | `{` |
|     ! 0 | 2168 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2169 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2170 | `}` |
|       - | 2171 |  |
