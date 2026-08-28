# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 778/1098 lines (70.86%)

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
|    7738 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       5 |   79 | `{` |
|    7743 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    7743 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    7743 |   83 | `	switch(nOp){` |
|    3869 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    7743 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    7743 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    7743 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    7743 |   93 | `		pConf->xErr     = xConsumer;` |
|    7743 |   94 | `		pConf->pErrData = pUserData;` |
|    7743 |   95 | `		break;` |
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
|    7743 |  150 | `	return rc;` |
|       5 |  151 | `}` |
|       - |  152 | `/*` |
|       - |  153 | ` * Configure the PH7 library.` |
|       - |  154 | ` * return PH7_OK on success.Any other return value` |
|       - |  155 | ` * indicates failure.` |
|       - |  156 | ` * Refer to [ph7_lib_config()].` |
|       - |  157 | ` */` |
|   11640 |  158 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       5 |  159 | `{` |
|   11645 |  160 | `	int rc = PH7_OK;` |
|   11645 |  161 | `	switch(nOp){` |
|    1940 |  162 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  163 | `			/* Install a virtual file system */` |
|    3885 |  164 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    3885 |  165 | `			sMPGlobal.pVfs = pVfs;` |
|    3885 |  166 | `			break;` |
|       - |  167 | `								}` |
|    1940 |  168 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  169 | `			/* Use an alternative low-level memory allocation routines */` |
|    3885 |  170 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  171 | `			/* Save the memory failure callback (if available) */` |
|    3885 |  172 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    3885 |  173 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    3885 |  174 | `			if( pMethods == 0 ){` |
|       - |  175 | `				/* Use the built-in memory allocation subsystem */` |
|    3885 |  176 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    1945 |  177 | `			}else{` |
|     ! 0 |  178 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  179 | `			}` |
|    3885 |  180 | `			break;` |
|       - |  181 | `										  }` |
|     ! 0 |  182 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  183 | `			/* Memory failure callback */` |
|     ! 0 |  184 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  185 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  186 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  187 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  188 | `			break;` |
|       - |  189 | `												 }` |
|    1940 |  190 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  191 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  192 | `			/* Use an alternative low-level mutex subsystem */` |
|    3885 |  193 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  194 | `#if defined (UNTRUST)` |
|       - |  195 | `			if( pMethods == 0 ){` |
|       - |  196 | `				rc = PH7_CORRUPT;` |
|       - |  197 | `			}` |
|       - |  198 | `#endif` |
|       - |  199 | `			/* Sanity check */` |
|    3885 |  200 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  201 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  202 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  203 | `				break;` |
|       - |  204 | `			}` |
|    3885 |  205 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  206 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  207 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  208 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  209 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  210 | `				}` |
|     ! 0 |  211 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  212 | `			}` |
|       - |  213 | `			/* Initialize and install the new mutex subsystem */` |
|    3885 |  214 | `			if( pMethods->xGlobalInit ){` |
|       5 |  215 | `				rc = pMethods->xGlobalInit();` |
|       5 |  216 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  217 | `					break;` |
|       - |  218 | `				}` |
|     ! 0 |  219 | `			}` |
|       - |  220 | `			/* Create the global mutex */` |
|    3885 |  221 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    3885 |  222 | `			if( sMPGlobal.pMutex == 0 ){` |
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
|    3885 |  233 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    3885 |  234 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  235 | `				/* Set a default threading level */` |
|    3885 |  236 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    1940 |  237 | `			}` |
|       - |  238 | `#endif` |
|    3885 |  239 | `			break;` |
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
|   11645 |  260 | `	return rc;` |
|       5 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  264 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  265 | ` */` |
|   11640 |  266 | `int ph7_lib_config(int nConfigOp,...)` |
|       5 |  267 | `{` |
|       - |  268 | `	va_list ap;` |
|       - |  269 | `	int rc;` |
|       - |  270 |  |
|   11645 |  271 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  272 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  273 | `		return PH7_LOOKED;` |
|       - |  274 | `	}` |
|   11645 |  275 | `	va_start(ap,nConfigOp);` |
|   11645 |  276 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|   11645 |  277 | `	va_end(ap);` |
|   11645 |  278 | `	return rc;` |
|    5825 |  279 | `}` |
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
|    3880 |  290 | `static sxi32 PH7CoreInitialize(void)` |
|       5 |  291 | `{` |
|       - |  292 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  293 | `#if defined(PH7_ENABLE_THREADS)` |
|    3885 |  294 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    3885 |  295 | `	SyMutex *pMaster = 0;` |
|       - |  296 | `#endif` |
|       - |  297 | `	int rc;` |
|       - |  298 | `	/*` |
|       - |  299 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  300 | `	 * is a no-op.` |
|       - |  301 | `	 */` |
|    3885 |  302 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  303 | `		return PH7_OK; /* Already initialized */` |
|       - |  304 | `	}` |
|       - |  305 | `	/* Point to the built-in vfs */` |
|    3885 |  306 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  307 | `	/* Install it */` |
|    3885 |  308 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  309 | `#if defined(PH7_ENABLE_THREADS)` |
|    3885 |  310 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    3885 |  311 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    3885 |  312 | `		if( pMutexMethods == 0 ){` |
|       - |  313 | `			/* Use the built-in mutex subsystem */` |
|    3885 |  314 | `			pMutexMethods = SyMutexExportMethods();` |
|    3885 |  315 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  316 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  317 | `			}` |
|       - |  318 | `			/* Install the mutex subsystem */` |
|    3885 |  319 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    3885 |  320 | `			if( rc != PH7_OK ){` |
|     ! 0 |  321 | `				return rc;` |
|       - |  322 | `			}` |
|    1940 |  323 | `		}` |
|       - |  324 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    3885 |  325 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    3885 |  326 | `		if( pMaster == 0 ){` |
|     ! 0 |  327 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  328 | `		}` |
|    1940 |  329 | `	}` |
|       - |  330 | `	/* Lock the master mutex */` |
|    3885 |  331 | `	rc = PH7_OK;` |
|    3885 |  332 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    5825 |  333 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  334 | `#endif` |
|    3885 |  335 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  336 | `			/* Install a memory subsystem */` |
|    3885 |  337 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    3885 |  338 | `			if( rc != PH7_OK ){` |
|       - |  339 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  340 | `				goto End;` |
|       - |  341 | `			}` |
|    1940 |  342 | `		}` |
|       - |  343 | `#if defined(PH7_ENABLE_THREADS)` |
|    3885 |  344 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  345 | `			/* Protect the memory allocation subsystem */` |
|    3885 |  346 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    3885 |  347 | `			if( rc != PH7_OK ){` |
|     ! 0 |  348 | `				goto End;` |
|       - |  349 | `			}` |
|    1940 |  350 | `		}` |
|       - |  351 | `#endif` |
|       - |  352 | `		/* Our library is initialized,set the magic number */` |
|    3885 |  353 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    3885 |  354 | `		rc = PH7_OK;` |
|       - |  355 | `#if defined(PH7_ENABLE_THREADS)` |
|    1940 |  356 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  357 | `#endif` |
|     ! 0 |  358 | `End:` |
|       - |  359 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  360 | `	/* Unlock the master mutex */` |
|    3885 |  361 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  362 | `#endif` |
|    3885 |  363 | `	return rc;` |
|    1945 |  364 | `}` |
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
|    3880 |  378 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       5 |  379 | `{` |
|       - |  380 | `	ph7_vm *pVm,*pNext;` |
|       - |  381 | `	/* Release all active VM */` |
|    3885 |  382 | `	pVm = pEngine->pVms;` |
|    1940 |  383 | `	for(;;){` |
|    3885 |  384 | `		if( pEngine->iVm <= 0 ){` |
|    3885 |  385 | `			break;` |
|       - |  386 | `		}` |
|     ! 0 |  387 | `		pNext = pVm->pNext;` |
|     ! 0 |  388 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  389 | `		pVm = pNext;` |
|     ! 0 |  390 | `		pEngine->iVm--;` |
|     ! 0 |  391 | `	}` |
|       - |  392 | `	/* Set a dummy magic number */` |
|    3885 |  393 | `	pEngine->nMagic = 0x7635;` |
|       - |  394 | `	/* Release the private memory subsystem */` |
|    3885 |  395 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    3885 |  396 | `	return PH7_OK;` |
|       5 |  397 | `}` |
|       - |  398 | `/*` |
|       - |  399 | ` * Release all resources consumed by the library.` |
|       - |  400 | ` * If PH7 is already shut down when this routine` |
|       - |  401 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  402 | ` * Note: This call is not thread safe.` |
|       - |  403 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  404 | ` */` |
|     388 |  405 | `static void PH7CoreShutdown(void)` |
|       4 |  406 | `{` |
|       - |  407 | `	ph7 *pEngine,*pNext;` |
|       - |  408 | `	/* Release all active engines first */` |
|     392 |  409 | `	pEngine = sMPGlobal.pEngines;` |
|     388 |  410 | `	for(;;){` |
|     780 |  411 | `		if( sMPGlobal.nEngine < 1 ){` |
|     392 |  412 | `			break;` |
|       - |  413 | `		}` |
|     392 |  414 | `		pNext = pEngine->pNext;` |
|     392 |  415 | `		EngineRelease(pEngine);` |
|     392 |  416 | `		pEngine = pNext;` |
|     392 |  417 | `		sMPGlobal.nEngine--;` |
|       4 |  418 | `	}` |
|       - |  419 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  420 | `	/* Release the mutex subsystem */` |
|     392 |  421 | `	if( sMPGlobal.pMutexMethods ){` |
|     392 |  422 | `		if( sMPGlobal.pMutex ){` |
|     392 |  423 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     392 |  424 | `			sMPGlobal.pMutex = 0;` |
|     194 |  425 | `		}` |
|     392 |  426 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       4 |  427 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  428 | `		}` |
|     392 |  429 | `		sMPGlobal.pMutexMethods = 0;` |
|     194 |  430 | `	}` |
|     392 |  431 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  432 | `#endif` |
|     392 |  433 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  434 | `		/* Release the memory backend */` |
|     392 |  435 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     194 |  436 | `	}` |
|     392 |  437 | `	sMPGlobal.nMagic = 0x1928;` |
|     392 |  438 | `}` |
|       - |  439 | `/*` |
|       - |  440 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  441 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  442 | ` */` |
|     388 |  443 | `int ph7_lib_shutdown(void)` |
|       4 |  444 | `{` |
|     392 |  445 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  446 | `		/* Already shut */` |
|     ! 0 |  447 | `		return PH7_OK;` |
|       - |  448 | `	}` |
|     392 |  449 | `	PH7CoreShutdown();` |
|     392 |  450 | `	return PH7_OK;` |
|     198 |  451 | `}` |
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
|       4 |  478 | `{` |
|      14 |  479 | `	return PH7_VERSION;` |
|       4 |  480 | `}` |
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
|    7738 |  509 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       5 |  510 | `{` |
|       - |  511 | `	va_list ap;` |
|       - |  512 | `	int rc;` |
|    7743 |  513 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  514 | `		return PH7_CORRUPT;` |
|       - |  515 | `	}` |
|       - |  516 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  517 | `	 /* Acquire engine mutex */` |
|    7743 |  518 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    7743 |  519 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    7738 |  520 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  521 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  522 | `	 }` |
|       - |  523 | `#endif` |
|    7743 |  524 | `	 va_start(ap,nConfigOp);` |
|    7743 |  525 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    7743 |  526 | `	 va_end(ap);` |
|       - |  527 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  528 | `	 /* Leave engine mutex */` |
|    7743 |  529 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  530 | `#endif` |
|    7743 |  531 | `	return rc;` |
|    3874 |  532 | `}` |
|       - |  533 | `/*` |
|       - |  534 | ` * [CAPIREF: ph7_init()]` |
|       - |  535 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  536 | ` */` |
|    3880 |  537 | `int ph7_init(ph7 **ppEngine)` |
|       5 |  538 | `{` |
|       - |  539 | `	ph7 *pEngine;` |
|       - |  540 | `	int rc;` |
|       - |  541 | `#if defined(UNTRUST)` |
|       - |  542 | `	if( ppEngine == 0 ){` |
|       - |  543 | `		return PH7_CORRUPT;` |
|       - |  544 | `	}` |
|       - |  545 | `#endif` |
|    3885 |  546 | `	*ppEngine = 0;` |
|       - |  547 | `	/* One-time automatic library initialization */` |
|    3885 |  548 | `	rc = PH7CoreInitialize();` |
|    3885 |  549 | `	if( rc != PH7_OK ){` |
|     ! 0 |  550 | `		return rc;` |
|       - |  551 | `	}` |
|       - |  552 | `	/* Allocate a new engine */` |
|    3885 |  553 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    3885 |  554 | `	if( pEngine == 0 ){` |
|     ! 0 |  555 | `		return PH7_NOMEM;` |
|       - |  556 | `	}` |
|       - |  557 | `	/* Zero the structure */` |
|    3885 |  558 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  559 | `	/* Initialize engine fields */` |
|    3885 |  560 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    3885 |  561 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    3885 |  562 | `	if( rc != PH7_OK ){` |
|     ! 0 |  563 | `		goto Release;` |
|       - |  564 | `	}` |
|       - |  565 | `#if defined(PH7_ENABLE_THREADS)` |
|    3885 |  566 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  567 | `#endif` |
|       - |  568 | `	/* Default configuration */` |
|    3885 |  569 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  570 | `	/* Install a default compile-time error consumer routine */` |
|    3885 |  571 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  572 | `	/* Built-in vfs */` |
|    3885 |  573 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  574 | `#if defined(PH7_ENABLE_THREADS)` |
|    3885 |  575 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  576 | `		 /* Associate a recursive mutex with this instance */` |
|    3885 |  577 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3885 |  578 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  579 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  580 | `			 goto Release;` |
|       - |  581 | `		 }` |
|    1940 |  582 | `	 }` |
|       - |  583 | `#endif` |
|       - |  584 | `	/* Link to the list of active engines */` |
|       - |  585 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  586 | `	/* Enter the global mutex */` |
|    3885 |  587 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  588 | `#endif` |
|    3885 |  589 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    3885 |  590 | `	sMPGlobal.nEngine++;` |
|       - |  591 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  592 | `	/* Leave the global mutex */` |
|    3885 |  593 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  594 | `#endif` |
|       - |  595 | `	/* Write a pointer to the new instance */` |
|    3885 |  596 | `	*ppEngine = pEngine;` |
|    3885 |  597 | `	return PH7_OK;` |
|     ! 0 |  598 | `Release:` |
|     ! 0 |  599 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  600 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  601 | `	return rc;` |
|    1945 |  602 | `}` |
|       - |  603 | `/*` |
|       - |  604 | ` * [CAPIREF: ph7_release()]` |
|       - |  605 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  606 | ` */` |
|    3492 |  607 | `int ph7_release(ph7 *pEngine)` |
|       5 |  608 | `{` |
|       - |  609 | `	int rc;` |
|    3497 |  610 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  611 | `		return PH7_CORRUPT;` |
|       - |  612 | `	}` |
|       - |  613 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  614 | `	 /* Acquire engine mutex */` |
|    3497 |  615 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3497 |  616 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3492 |  617 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  618 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  619 | `	 }` |
|       - |  620 | `#endif` |
|       - |  621 | `	/* Release the engine */` |
|    3497 |  622 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  623 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  624 | `	 /* Leave engine mutex */` |
|    3497 |  625 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  626 | `	 /* Release engine mutex */` |
|    3497 |  627 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  628 | `#endif` |
|       - |  629 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  630 | `	/* Enter the global mutex */` |
|    3497 |  631 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  632 | `#endif` |
|       - |  633 | `	/* Unlink from the list of active engines */` |
|    3497 |  634 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    3497 |  635 | `	sMPGlobal.nEngine--;` |
|       - |  636 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  637 | `	/* Leave the global mutex */` |
|    3497 |  638 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  639 | `#endif` |
|       - |  640 | `	/* Release the memory chunk allocated to this engine */` |
|    3497 |  641 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    3497 |  642 | `	return rc;` |
|    1751 |  643 | `}` |
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
|    3876 |  654 | `static sxi32 ProcessScript(` |
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
|    3881 |  665 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    3881 |  666 | `	if( pVm == 0 ){` |
|       - |  667 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  668 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  669 | `		if( ppVm ){` |
|     ! 0 |  670 | `			*ppVm = 0;` |
|     ! 0 |  671 | `		}` |
|     ! 0 |  672 | `		return PH7_NOMEM;` |
|       - |  673 | `	}` |
|    3881 |  674 | `	if( iFlags < 0 ){` |
|       - |  675 | `		/* Default compile-time flags */` |
|     ! 0 |  676 | `		iFlags = 0;` |
|     ! 0 |  677 | `	}` |
|       - |  678 | `	/* Initialize the Virtual Machine */` |
|    3881 |  679 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    3881 |  680 | `	if( rc != PH7_OK ){` |
|     ! 0 |  681 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  682 | `		if( ppVm ){` |
|     ! 0 |  683 | `			*ppVm = 0;` |
|     ! 0 |  684 | `		}` |
|     ! 0 |  685 | `		return PH7_VM_ERR;` |
|       - |  686 | `	}` |
|    3881 |  687 | `	if( zFilePath ){` |
|       - |  688 | `		/* Push processed file path */` |
|    3869 |  689 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    1937 |  690 | `	}else{` |
|       - |  691 | `		/* Anonymous source (phl -r / an embedder snippet): php names it` |
|       - |  692 | `		 * "Command line code" in every diagnostic location suffix. */` |
|      15 |  693 | `		PH7_VmPushFilePath(pVm,"Command line code",-1,TRUE,0);` |
|       - |  694 | `	}` |
|       - |  695 | `	/* Reset the error message consumer */` |
|    3881 |  696 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  697 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|       - |  698 | `	{` |
|    3881 |  699 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|    3881 |  700 | `		if( SyStringLength(pScript) > nLimit ){` |
|     ! 0 |  701 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|       - |  702 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|     ! 0 |  703 | `				SyStringLength(pScript),nLimit);` |
|     ! 0 |  704 | `		}` |
|       - |  705 | `	}` |
|       - |  706 | `	/* Compile the script */` |
|    3881 |  707 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|    3881 |  708 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    1938 |  709 | `	}` |
|    3881 |  710 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     392 |  711 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  712 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     392 |  713 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     392 |  714 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     392 |  715 | `		if( ppVm ){` |
|     392 |  716 | `			*ppVm = 0;` |
|     194 |  717 | `		}` |
|     392 |  718 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  719 | `	}` |
|       - |  720 | `	/* Prepare the virtual machine for bytecode execution */` |
|    3493 |  721 | `	rc = PH7_VmMakeReady(pVm);` |
|    3493 |  722 | `	if( rc != PH7_OK ){` |
|       3 |  723 | `		goto Release;` |
|       - |  724 | `	}` |
|       - |  725 | `	/* Install local import path which is the current directory */` |
|    3491 |  726 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  727 | `#if defined(PH7_ENABLE_THREADS)` |
|    3491 |  728 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  729 | `		 /* Associate a recursive mutex with this instance */` |
|    3491 |  730 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3491 |  731 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  732 | `			 goto Release;` |
|       - |  733 | `		 }` |
|    1743 |  734 | `	 }` |
|       - |  735 | `#endif` |
|       - |  736 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    3491 |  737 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    3491 |  738 | `	pEngine->iVm++;` |
|       - |  739 | `	/* Point to the freshly created VM */` |
|    3491 |  740 | `	*ppVm = pVm;` |
|       - |  741 | `	/* Ready to execute PH7 bytecode */` |
|    3491 |  742 | `	return PH7_OK;` |
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
|    1943 |  755 | `}` |
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
|      12 |  793 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       3 |  794 | `{` |
|       - |  795 | `	SyString sScript;` |
|       - |  796 | `	int rc;` |
|      15 |  797 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  798 | `		return PH7_CORRUPT;` |
|       - |  799 | `	}` |
|      15 |  800 | `	if( nLen < 0 ){` |
|       - |  801 | `		/* Compute input length automatically */` |
|      15 |  802 | `		nLen = (int)SyStrlen(zSource);` |
|       6 |  803 | `	}` |
|      15 |  804 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  805 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  806 | `	 /* Acquire engine mutex */` |
|      15 |  807 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      15 |  808 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      12 |  809 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  810 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  811 | `	 }` |
|       - |  812 | `#endif` |
|       - |  813 | `	/* Compile the script */` |
|      15 |  814 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  815 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  816 | `	 /* Leave engine mutex */` |
|      15 |  817 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  818 | `#endif` |
|       - |  819 | `	/* Compilation result */` |
|      15 |  820 | `	return rc;` |
|       9 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  824 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  825 | ` */` |
|    3864 |  826 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       5 |  827 | `{` |
|       - |  828 | `	const ph7_vfs *pVfs;` |
|       - |  829 | `	int rc;` |
|    3869 |  830 | `	if( ppOutVm ){` |
|    3869 |  831 | `		*ppOutVm = 0;` |
|    1932 |  832 | `	}` |
|    3869 |  833 | `	rc = PH7_OK; /* cc warning */` |
|    3869 |  834 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  835 | `		return PH7_CORRUPT;` |
|       - |  836 | `	}` |
|       - |  837 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  838 | `	 /* Acquire engine mutex */` |
|    3869 |  839 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3869 |  840 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3864 |  841 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  842 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  843 | `	 }` |
|       - |  844 | `#endif` |
|       - |  845 | `	 /*` |
|       - |  846 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  847 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  848 | `	  */` |
|    3869 |  849 | `	 pVfs = pEngine->pVfs;` |
|    3869 |  850 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  851 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  852 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  853 | `	 }else{` |
|    3869 |  854 | `		 void *pMapView = 0; /* cc warning */` |
|    3869 |  855 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  856 | `		 SyString sScript;` |
|       - |  857 | `		 /* Try to get a memory view of the whole file */` |
|    3869 |  858 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    3869 |  859 | `		 if( rc != PH7_OK ){` |
|       - |  860 | `			 /* Assume an IO error */` |
|     ! 0 |  861 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  862 | `		 }else{` |
|       - |  863 | `			 /* Compile the file */` |
|    3869 |  864 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    3869 |  865 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  866 | `			 /* Release the memory view of the whole file */` |
|    3869 |  867 | `			 if( pVfs->xUnmap ){` |
|    3869 |  868 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    1932 |  869 | `			 }` |
|       - |  870 | `		 }` |
|       - |  871 | `	 }` |
|       - |  872 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  873 | `	 /* Leave engine mutex */` |
|    3869 |  874 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  875 | `#endif` |
|       - |  876 | `	/* Compilation result */` |
|    3869 |  877 | `	return rc;` |
|    1937 |  878 | `}` |
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
|   80648 |  903 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       5 |  904 | `{` |
|       - |  905 | `	va_list ap;` |
|       - |  906 | `	int rc;` |
|       - |  907 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   80653 |  908 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  909 | `		return PH7_CORRUPT;` |
|       - |  910 | `	}` |
|       - |  911 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  912 | `	 /* Acquire VM mutex */` |
|   80653 |  913 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   80653 |  914 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   80648 |  915 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  916 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  917 | `	 }` |
|       - |  918 | `#endif` |
|       - |  919 | `	/* Confiugure the virtual machine */` |
|   80653 |  920 | `	va_start(ap,iConfigOp);` |
|   80653 |  921 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   80653 |  922 | `	va_end(ap);` |
|       - |  923 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  924 | `	 /* Leave VM mutex */` |
|   80653 |  925 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  926 | `#endif` |
|   80653 |  927 | `	return rc;` |
|   40329 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  931 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  932 | ` */` |
|    3490 |  933 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       5 |  934 | `{` |
|       - |  935 | `	int rc;` |
|       - |  936 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    3495 |  937 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  938 | `		return PH7_CORRUPT;` |
|       - |  939 | `	}` |
|       - |  940 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  941 | `	 /* Acquire VM mutex */` |
|    3495 |  942 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3495 |  943 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3490 |  944 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  945 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  946 | `	 }` |
|       - |  947 | `#endif` |
|       - |  948 | `	/* Execute PH7 byte-code */` |
|    3495 |  949 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    3495 |  950 | `	if( pExitStatus ){` |
|       - |  951 | `		/* Exit status */` |
|    3471 |  952 | `		*pExitStatus = pVm->iExitStatus;` |
|    1733 |  953 | `	}` |
|       - |  954 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  955 | `	 /* Leave VM mutex */` |
|    3495 |  956 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  957 | `#endif` |
|       - |  958 | `	/* Execution result */` |
|    3495 |  959 | `	return rc;` |
|    1750 |  960 | `}` |
|       - |  961 | `/*` |
|       - |  962 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  963 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  964 | ` */` |
|       6 |  965 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  966 | `{` |
|       - |  967 | `	int rc;` |
|       - |  968 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       6 |  969 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  970 | `		return PH7_CORRUPT;` |
|       - |  971 | `	}` |
|       - |  972 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  973 | `	 /* Acquire VM mutex */` |
|       6 |  974 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       6 |  975 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       6 |  976 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  977 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  978 | `	 }` |
|       - |  979 | `#endif` |
|       6 |  980 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  981 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  982 | `	 /* Leave VM mutex */` |
|       6 |  983 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  984 | `#endif` |
|       6 |  985 | `	return rc;` |
|       3 |  986 | `}` |
|       - |  987 | `/*` |
|       - |  988 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  990 | ` */` |
|    3486 |  991 | `int ph7_vm_release(ph7_vm *pVm)` |
|       5 |  992 | `{` |
|       - |  993 | `	ph7 *pEngine;` |
|       - |  994 | `	int rc;` |
|       - |  995 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    3491 |  996 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  997 | `		return PH7_CORRUPT;` |
|       - |  998 | `	}` |
|       - |  999 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1000 | `	 /* Acquire VM mutex */` |
|    3491 | 1001 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3491 | 1002 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3486 | 1003 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1004 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1005 | `	 }` |
|       - | 1006 | `#endif` |
|    3491 | 1007 | `	pEngine = pVm->pEngine;` |
|    3491 | 1008 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - | 1009 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1010 | `	 /* Leave VM mutex */` |
|    3491 | 1011 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1012 | `#endif` |
|    3491 | 1013 | `	if( rc == PH7_OK ){` |
|       - | 1014 | `		/* Unlink from the list of active VM */` |
|       - | 1015 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1016 | `			/* Acquire engine mutex */` |
|    3491 | 1017 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3491 | 1018 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3486 | 1019 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 | 1020 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1021 | `			}` |
|       - | 1022 | `#endif` |
|    3491 | 1023 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    3491 | 1024 | `		pEngine->iVm--;` |
|       - | 1025 | `		/* Release the memory chunk allocated to this VM */` |
|    3491 | 1026 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - | 1027 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1028 | `			/* Leave engine mutex */` |
|    3491 | 1029 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1030 | `#endif` |
|    1743 | 1031 | `	}` |
|    3491 | 1032 | `	return rc;` |
|    1748 | 1033 | `}` |
|       - | 1034 | `/*` |
|       - | 1035 | ` * [CAPIREF: ph7_create_function()]` |
|       - | 1036 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1037 | ` */` |
| 1948652 | 1038 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       5 | 1039 | `{` |
|       - | 1040 | `	SyString sName;` |
|       - | 1041 | `	int rc;` |
|       - | 1042 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 1948657 | 1043 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1044 | `		return PH7_CORRUPT;` |
|       - | 1045 | `	}` |
| 1948657 | 1046 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1047 | `	/* Remove leading and trailing white spaces */` |
| 1948657 | 1048 | `	SyStringFullTrim(&sName);` |
|       - | 1049 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 1948657 | 1050 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1051 | `		return PH7_CORRUPT;` |
|       - | 1052 | `	}` |
|       - | 1053 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1054 | `	 /* Acquire VM mutex */` |
| 1948657 | 1055 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 1948657 | 1056 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 1948652 | 1057 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1058 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1059 | `	 }` |
|       - | 1060 | `#endif` |
|       - | 1061 | `	/* Install the foreign function */` |
| 1948657 | 1062 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1063 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1064 | `	 /* Leave VM mutex */` |
| 1948657 | 1065 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1066 | `#endif` |
| 1948657 | 1067 | `	return rc;` |
|  974331 | 1068 | `}` |
|       - | 1069 | `/*` |
|       - | 1070 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1071 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1072 | ` */` |
|     ! 0 | 1073 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1074 | `{` |
|     ! 0 | 1075 | `	ph7_user_func *pFunc = 0;` |
|       - | 1076 | `	int rc;` |
|       - | 1077 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1078 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1079 | `		return PH7_CORRUPT;` |
|       - | 1080 | `	}` |
|       - | 1081 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1082 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1083 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1084 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1085 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1086 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1087 | `	 }` |
|       - | 1088 | `#endif` |
|       - | 1089 | `	/* Perform the deletion */` |
|     ! 0 | 1090 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1091 | `	if( rc == PH7_OK ){` |
|       - | 1092 | `		/* Release internal fields */` |
|     ! 0 | 1093 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1094 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1095 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1096 | `	}` |
|       - | 1097 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1098 | `	 /* Leave VM mutex */` |
|     ! 0 | 1099 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1100 | `#endif` |
|     ! 0 | 1101 | `	return rc;` |
|     ! 0 | 1102 | `}` |
|       - | 1103 | `/*` |
|       - | 1104 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1105 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1106 | ` */` |
|  945244 | 1107 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       5 | 1108 | `{` |
|       - | 1109 | `	SyString sName;` |
|       - | 1110 | `	int rc;` |
|       - | 1111 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  945249 | 1112 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1113 | `		return PH7_CORRUPT;` |
|       - | 1114 | `	}` |
|  945249 | 1115 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1116 | `	/* Remove leading and trailing white spaces */` |
|  948737 | 1117 | `	SyStringFullTrim(&sName);` |
|  945249 | 1118 | `	if( sName.nByte < 1 ){` |
|       - | 1119 | `		/* Empty constant name */` |
|     ! 0 | 1120 | `		return PH7_CORRUPT;` |
|       - | 1121 | `	}` |
|       - | 1122 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  945249 | 1123 | `	if( xExpand == 0 ){` |
|     ! 0 | 1124 | `		return PH7_CORRUPT;` |
|       - | 1125 | `	}` |
|       - | 1126 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1127 | `	 /* Acquire VM mutex */` |
|  945249 | 1128 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  945249 | 1129 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  945244 | 1130 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1131 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1132 | `	 }` |
|       - | 1133 | `#endif` |
|       - | 1134 | `	/* Perform the registration */` |
|  945249 | 1135 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1136 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1137 | `	 /* Leave VM mutex */` |
|  945249 | 1138 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1139 | `#endif` |
|  945249 | 1140 | `	 return rc;` |
|  472627 | 1141 | `}` |
|       - | 1142 | `/*` |
|       - | 1143 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1144 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1145 | ` */` |
|     ! 0 | 1146 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1147 | `{` |
|       - | 1148 | `	ph7_constant *pCons;` |
|       - | 1149 | `	int rc;` |
|       - | 1150 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1151 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1152 | `		return PH7_CORRUPT;` |
|       - | 1153 | `	}` |
|       - | 1154 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1155 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1156 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1157 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1158 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1159 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1160 | `	 }` |
|       - | 1161 | `#endif` |
|       - | 1162 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1163 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1164 | `	 if( rc == PH7_OK ){` |
|       - | 1165 | `		 /* Perform the deletion */` |
|     ! 0 | 1166 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1167 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1168 | `	 }` |
|       - | 1169 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1170 | `	 /* Leave VM mutex */` |
|     ! 0 | 1171 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1172 | `#endif` |
|     ! 0 | 1173 | `	return rc;` |
|     ! 0 | 1174 | `}` |
|       - | 1175 | `/*` |
|       - | 1176 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1177 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1178 | ` */` |
|  110204 | 1179 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       5 | 1180 | `{` |
|       - | 1181 | `	ph7_value *pObj;` |
|       - | 1182 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  110209 | 1183 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1184 | `		return 0;` |
|       - | 1185 | `	}` |
|       - | 1186 | `	/* Allocate a new scalar variable */` |
|  110209 | 1187 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  110209 | 1188 | `	if( pObj == 0 ){` |
|     ! 0 | 1189 | `		return 0;` |
|       - | 1190 | `	}` |
|       - | 1191 | `	/* Nullify the new scalar */` |
|  110209 | 1192 | `	PH7_MemObjInit(pVm,pObj);` |
|  110209 | 1193 | `	return pObj;` |
|   55107 | 1194 | `}` |
|       - | 1195 | `/*` |
|       - | 1196 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1197 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1198 | ` */` |
|   71330 | 1199 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       5 | 1200 | `{` |
|       - | 1201 | `	ph7_hashmap *pMap;` |
|       - | 1202 | `	ph7_value *pObj;` |
|       - | 1203 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   71335 | 1204 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1205 | `		return 0;` |
|       - | 1206 | `	}` |
|       - | 1207 | `	/* Create a new hashmap first */` |
|   71335 | 1208 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   71335 | 1209 | `	if( pMap == 0 ){` |
|     ! 0 | 1210 | `		return 0;` |
|       - | 1211 | `	}` |
|       - | 1212 | `	/* Associate a new ph7_value with this hashmap */` |
|   71335 | 1213 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   71335 | 1214 | `	if( pObj == 0 ){` |
|     ! 0 | 1215 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1216 | `		return 0;` |
|       - | 1217 | `	}` |
|   71335 | 1218 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   71335 | 1219 | `	return pObj;` |
|   35670 | 1220 | `}` |
|       - | 1221 | `/*` |
|       - | 1222 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1223 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1224 | ` */` |
|   38406 | 1225 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1226 | `{` |
|       - | 1227 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   38411 | 1228 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1229 | `		return PH7_CORRUPT;` |
|       - | 1230 | `	}` |
|   38411 | 1231 | `	if( pValue ){` |
|       - | 1232 | `		/* Release the value */` |
|   38411 | 1233 | `		PH7_MemObjRelease(pValue);` |
|   38411 | 1234 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|   19203 | 1235 | `	}` |
|   38411 | 1236 | `	return PH7_OK;` |
|   19208 | 1237 | `}` |
|       - | 1238 | `/*` |
|       - | 1239 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1240 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1241 | ` */` |
|  419812 | 1242 | `int ph7_value_to_int(ph7_value *pValue)` |
|       5 | 1243 | `{` |
|       - | 1244 | `	int rc;` |
|  419817 | 1245 | `	rc = PH7_MemObjToInteger(pValue);` |
|  419817 | 1246 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1247 | `		return 0;` |
|       - | 1248 | `	}` |
|  419817 | 1249 | `	return (int)pValue->x.iVal;` |
|  209911 | 1250 | `}` |
|       - | 1251 | `/*` |
|       - | 1252 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1253 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1254 | ` */` |
|    1174 | 1255 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       5 | 1256 | `{` |
|       - | 1257 | `	int rc;` |
|    1179 | 1258 | `	rc = PH7_MemObjToBool(pValue);` |
|    1179 | 1259 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1260 | `		return 0;` |
|       - | 1261 | `	}` |
|    1179 | 1262 | `	return (int)pValue->x.iVal;` |
|     592 | 1263 | `}` |
|       - | 1264 | `/*` |
|       - | 1265 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1266 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1267 | ` */` |
|   27710 | 1268 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       5 | 1269 | `{` |
|       - | 1270 | `	int rc;` |
|   27715 | 1271 | `	rc = PH7_MemObjToInteger(pValue);` |
|   27715 | 1272 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1273 | `		return 0;` |
|       - | 1274 | `	}` |
|   27715 | 1275 | `	return pValue->x.iVal;` |
|   13860 | 1276 | `}` |
|       - | 1277 | `/*` |
|       - | 1278 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1279 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1280 | ` */` |
|    1192 | 1281 | `double ph7_value_to_double(ph7_value *pValue)` |
|       1 | 1282 | `{` |
|       - | 1283 | `	int rc;` |
|    1193 | 1284 | `	rc = PH7_MemObjToReal(pValue);` |
|    1193 | 1285 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1286 | `		return (double)0;` |
|       - | 1287 | `	}` |
|    1193 | 1288 | `	return (double)pValue->rVal;` |
|     597 | 1289 | `}` |
|       - | 1290 | `/*` |
|       - | 1291 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1292 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1293 | ` */` |
|  806513 | 1294 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       5 | 1295 | `{` |
|  806518 | 1296 | `	PH7_MemObjToString(pValue);` |
|  806518 | 1297 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  773474 | 1298 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  773474 | 1299 | `		if( pLen ){` |
|  712096 | 1300 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  356088 | 1301 | `		}` |
|  773474 | 1302 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1303 | `	}else{` |
|       - | 1304 | `		/* Return the empty string */` |
|   33049 | 1305 | `		if( pLen ){` |
|   33039 | 1306 | `			*pLen = 0;` |
|   16517 | 1307 | `		}` |
|   33049 | 1308 | `		return "";` |
|       - | 1309 | `	}` |
|  403304 | 1310 | `}` |
|       - | 1311 | `/*` |
|       - | 1312 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1313 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1314 | ` */` |
|   30926 | 1315 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       5 | 1316 | `{` |
|   30931 | 1317 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1318 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1319 | `		return 0;` |
|       - | 1320 | `	}` |
|   30931 | 1321 | `	return pValue->x.pOther;` |
|   15468 | 1322 | `}` |
|       - | 1323 | `/*` |
|       - | 1324 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1325 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1326 | ` */` |
|      64 | 1327 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1328 | `{` |
|       - | 1329 | `	int rc;` |
|      65 | 1330 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1331 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1332 | `		return 1;` |
|       - | 1333 | `	}` |
|       - | 1334 | `	/* Perform the comparison */` |
|      65 | 1335 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1336 | `	/* Comparison result */` |
|      65 | 1337 | `	return rc;` |
|      33 | 1338 | `}` |
|       - | 1339 | `/*` |
|       - | 1340 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1341 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1342 | ` */` |
|   18702 | 1343 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       5 | 1344 | `{` |
|   18707 | 1345 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       5 | 1346 | `}` |
|       - | 1347 | `/*` |
|       - | 1348 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1349 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1350 | ` */` |
|   19364 | 1351 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       5 | 1352 | `{` |
|   19369 | 1353 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       5 | 1354 | `}` |
|       - | 1355 | `/*` |
|       - | 1356 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1357 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1358 | ` */` |
|  366149 | 1359 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       5 | 1360 | `{` |
|  366154 | 1361 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       5 | 1362 | `}` |
|       - | 1363 | `/*` |
|       - | 1364 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1365 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1366 | ` */` |
|     616 | 1367 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       1 | 1368 | `{` |
|     617 | 1369 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       1 | 1370 | `}` |
|       - | 1371 | `/*` |
|       - | 1372 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1373 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1374 | ` */` |
|     374 | 1375 | `int ph7_result_null(ph7_context *pCtx)` |
|       4 | 1376 | `{` |
|       - | 1377 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     378 | 1378 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     378 | 1379 | `	return PH7_OK;` |
|       4 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1383 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1384 | ` */` |
| 1204178 | 1385 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       5 | 1386 | `{` |
| 1204183 | 1387 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       5 | 1388 | `}` |
|       - | 1389 | `/*` |
|       - | 1390 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1391 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1392 | ` */` |
|     818 | 1393 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       3 | 1394 | `{` |
|       - | 1395 | `	ph7_value *p;` |
|       - | 1396 | `	va_list ap;` |
|       - | 1397 | `	int rc;` |
|     821 | 1398 | `	p = pCtx->pRet;` |
|     821 | 1399 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1400 | `		/* Invalidate any prior representation */` |
|     330 | 1401 | `		PH7_MemObjRelease(p);` |
|     330 | 1402 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|     164 | 1403 | `	}` |
|       - | 1404 | `	/* Format the given string */` |
|     821 | 1405 | `	va_start(ap,zFormat);` |
|     821 | 1406 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|     821 | 1407 | `	va_end(ap);` |
|     821 | 1408 | `	return rc;` |
|       3 | 1409 | `}` |
|       - | 1410 | `/*` |
|       - | 1411 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1412 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1413 | ` */` |
|   36810 | 1414 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1415 | `{` |
|   36815 | 1416 | `	int rc = PH7_OK;` |
|   36815 | 1417 | `	if( pValue == 0 ){` |
|     ! 0 | 1418 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1419 | `	}else{` |
|   36815 | 1420 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1421 | `	}` |
|   36815 | 1422 | `	return rc;` |
|       5 | 1423 | `}` |
|       - | 1424 | `/*` |
|       - | 1425 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1426 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1427 | ` */` |
|    5126 | 1428 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       5 | 1429 | `{` |
|    5131 | 1430 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       5 | 1431 | `}` |
|       - | 1432 | `/*` |
|       - | 1433 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1434 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1435 | ` */` |
|  106738 | 1436 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       5 | 1437 | `{` |
|       - | 1438 | `	ph7_value *pVal;` |
|  106743 | 1439 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|  106743 | 1440 | `	if( pVal ){` |
|       - | 1441 | `		/* Record value address so it can be freed automatically` |
|       - | 1442 | `		 * when the calling function returns.` |
|       - | 1443 | `		 */` |
|  106743 | 1444 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   53369 | 1445 | `	}` |
|  106743 | 1446 | `	return pVal;` |
|       5 | 1447 | `}` |
|       - | 1448 | `/*` |
|       - | 1449 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1450 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1451 | ` */` |
|   36390 | 1452 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       5 | 1453 | `{` |
|       - | 1454 | `	ph7_value *pVal;` |
|   36395 | 1455 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   36395 | 1456 | `	if( pVal ){` |
|       - | 1457 | `		/* Record value address so it can be freed automatically` |
|       - | 1458 | `		 * when the calling function returns.` |
|       - | 1459 | `		 */` |
|   36395 | 1460 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   18195 | 1461 | `	}` |
|   36395 | 1462 | `	return pVal;` |
|       5 | 1463 | `}` |
|       - | 1464 | `/*` |
|       - | 1465 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1466 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1467 | ` */` |
|     658 | 1468 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       5 | 1469 | `{` |
|     663 | 1470 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     663 | 1471 | `}` |
|       - | 1472 | `/*` |
|       - | 1473 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1474 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1475 | ` */` |
|    5216 | 1476 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       5 | 1477 | `{` |
|       - | 1478 | `	void *pChunk;` |
|    5221 | 1479 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    5221 | 1480 | `	if( pChunk ){` |
|    5221 | 1481 | `		if( ZeroChunk ){` |
|       - | 1482 | `			/* Zero the memory chunk */` |
|    5069 | 1483 | `			SyZero(pChunk,nByte);` |
|    2532 | 1484 | `		}` |
|    5221 | 1485 | `		if( AutoRelease ){` |
|       - | 1486 | `			ph7_aux_data sAux;` |
|       - | 1487 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1488 | `			 * upon this context is destroyed.` |
|       - | 1489 | `			 */` |
|     147 | 1490 | `			sAux.pAuxData = pChunk;` |
|     147 | 1491 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|      71 | 1492 | `		}` |
|    2608 | 1493 | `	}` |
|    5221 | 1494 | `	return pChunk;` |
|       5 | 1495 | `}` |
|       - | 1496 | `/*` |
|       - | 1497 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1498 | ` * chunk container.` |
|       - | 1499 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1500 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1501 | ` */` |
|    5042 | 1502 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1503 | `{` |
|       - | 1504 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1505 | `	sxu32 n;` |
|    5047 | 1506 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1507 | `		/* Don't bother processing,the container is empty */` |
|    5047 | 1508 | `		return 0;` |
|       - | 1509 | `	}` |
|       - | 1510 | `	/* Perform the lookup */` |
|     ! 0 | 1511 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1512 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1513 | `		pAux = &aAux[n];` |
|     ! 0 | 1514 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1515 | `			/* Chunk found */` |
|     ! 0 | 1516 | `			return pAux;` |
|       - | 1517 | `		}` |
|     ! 0 | 1518 | `	}` |
|       - | 1519 | `	/* No such allocated chunk */` |
|     ! 0 | 1520 | `	return 0;` |
|    2526 | 1521 | `}` |
|       - | 1522 | `/*` |
|       - | 1523 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1524 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1525 | ` */` |
|     ! 0 | 1526 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1527 | `{` |
|       - | 1528 | `	ph7_aux_data *pAux;` |
|       - | 1529 | `	void *pNew;` |
|     ! 0 | 1530 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1531 | `	if( pNew ){` |
|     ! 0 | 1532 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1533 | `		if( pAux ){` |
|     ! 0 | 1534 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1535 | `		}` |
|     ! 0 | 1536 | `	}` |
|     ! 0 | 1537 | `	return pNew;` |
|     ! 0 | 1538 | `}` |
|       - | 1539 | `/*` |
|       - | 1540 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1541 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1542 | ` */` |
|    5042 | 1543 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       5 | 1544 | `{` |
|       - | 1545 | `	ph7_aux_data *pAux;` |
|    5047 | 1546 | `	if( pChunk == 0 ){` |
|       - | 1547 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1548 | `		return;` |
|       - | 1549 | `	}` |
|    5047 | 1550 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    5047 | 1551 | `	if( pAux ){` |
|       - | 1552 | `		/* Mark as destroyed */` |
|     ! 0 | 1553 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1554 | `	}` |
|    5047 | 1555 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    2526 | 1556 | `}` |
|       - | 1557 | `/*` |
|       - | 1558 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1559 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1560 | ` */` |
|     152 | 1561 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|       3 | 1562 | `{` |
|       - | 1563 | `	ph7_hashmap_node *pNode;` |
|       - | 1564 | `	ph7_value *pValue;` |
|       - | 1565 | `	ph7_value skey;` |
|       - | 1566 | `	int rc;` |
|       - | 1567 | `	/* Make sure we are dealing with a valid hashmap */` |
|     155 | 1568 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1569 | `		return 0;` |
|       - | 1570 | `	}` |
|     155 | 1571 | `	if( nByte < 0 ){` |
|     ! 0 | 1572 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1573 | `	}` |
|       - | 1574 | `	/* Convert the key to a ph7_value  */` |
|     155 | 1575 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     155 | 1576 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1577 | `	/* Perform the lookup */` |
|     155 | 1578 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     155 | 1579 | `	PH7_MemObjRelease(&skey);` |
|     155 | 1580 | `	if( rc != PH7_OK ){` |
|       - | 1581 | `		/* No such entry */` |
|      64 | 1582 | `		return 0;` |
|       - | 1583 | `	}` |
|       - | 1584 | `	/* Extract the target value */` |
|      93 | 1585 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|      93 | 1586 | `	return pValue;` |
|      79 | 1587 | `}` |
|       - | 1588 | `/*` |
|       - | 1589 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1590 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1591 | ` */` |
|   34002 | 1592 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       5 | 1593 | `{` |
|       - | 1594 | `	int rc;` |
|   34007 | 1595 | `	if( xWalk == 0 ){` |
|     ! 0 | 1596 | `		return PH7_CORRUPT;` |
|       - | 1597 | `	}` |
|       - | 1598 | `	/* Make sure we are dealing with a valid hashmap */` |
|   34007 | 1599 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1600 | `		return PH7_CORRUPT;` |
|       - | 1601 | `	}` |
|       - | 1602 | `	/* Start the walk process */` |
|   34007 | 1603 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   34007 | 1604 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   17006 | 1605 | `}` |
|       - | 1606 | `/*` |
|       - | 1607 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1608 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1609 | ` */` |
|  208450 | 1610 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       5 | 1611 | `{` |
|       - | 1612 | `	int rc;` |
|       - | 1613 | `	/* Make sure we are dealing with a valid hashmap */` |
|  208455 | 1614 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1615 | `		return PH7_CORRUPT;` |
|       - | 1616 | `	}` |
|       - | 1617 | `	/* Perform the insertion */` |
|  208455 | 1618 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
|  208455 | 1619 | `	return rc;` |
|  104230 | 1620 | `}` |
|       - | 1621 | `/*` |
|       - | 1622 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1623 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1624 | ` */` |
|  116806 | 1625 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       5 | 1626 | `{` |
|       - | 1627 | `	int rc;` |
|       - | 1628 | `	/* Make sure we are dealing with a valid hashmap */` |
|  116811 | 1629 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1630 | `		return PH7_CORRUPT;` |
|       - | 1631 | `	}` |
|       - | 1632 | `	/* Perform the insertion */` |
|  116811 | 1633 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1634 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1635 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1636 | `	}else{` |
|       - | 1637 | `		ph7_value sKey;` |
|  116811 | 1638 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|  116811 | 1639 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|  116811 | 1640 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  116811 | 1641 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1642 | `	}` |
|  116811 | 1643 | `	return rc;` |
|   58408 | 1644 | `}` |
|       - | 1645 | `/*` |
|       - | 1646 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1647 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1648 | ` */` |
| 2118122 | 1649 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       5 | 1650 | `{` |
|       - | 1651 | `	ph7_value sKey;` |
|       - | 1652 | `	int rc;` |
|       - | 1653 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2118127 | 1654 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1655 | `		return PH7_CORRUPT;` |
|       - | 1656 | `	}` |
| 2118127 | 1657 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1658 | `	/* Perform the insertion */` |
| 2118127 | 1659 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
| 2118127 | 1660 | `	PH7_MemObjRelease(&sKey);` |
| 2118127 | 1661 | `	return rc;` |
| 1059066 | 1662 | `}` |
|       - | 1663 | `/*` |
|       - | 1664 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1665 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1666 | ` */` |
|  152416 | 1667 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       5 | 1668 | `{` |
|       - | 1669 | `	ph7_hashmap *pMap;` |
|       - | 1670 | `	/* Make sure we are dealing with a valid hashmap */` |
|  152421 | 1671 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1672 | `		return 0;` |
|       - | 1673 | `	}` |
|       - | 1674 | `	/* Point to the internal representation of the hashmap */` |
|  152421 | 1675 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  152421 | 1676 | `	return pMap->nEntry;` |
|   76213 | 1677 | `}` |
|       - | 1678 | `/*` |
|       - | 1679 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1680 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1681 | ` */` |
|     ! 0 | 1682 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|     ! 0 | 1683 | `{` |
|       - | 1684 | `	int rc;` |
|     ! 0 | 1685 | `	if( xWalk == 0 ){` |
|     ! 0 | 1686 | `		return PH7_CORRUPT;` |
|       - | 1687 | `	}` |
|       - | 1688 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1689 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1690 | `		return PH7_CORRUPT;` |
|       - | 1691 | `	}` |
|       - | 1692 | `	/* Start the walk process */` |
|     ! 0 | 1693 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|     ! 0 | 1694 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|     ! 0 | 1695 | `}` |
|       - | 1696 | `/*` |
|       - | 1697 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1698 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1699 | ` */` |
|       8 | 1700 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1701 | `{` |
|       - | 1702 | `	ph7_value *pValue;` |
|       - | 1703 | `	SyString sAttr;` |
|       - | 1704 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1705 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1706 | `		return 0;` |
|       - | 1707 | `	}` |
|       9 | 1708 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1709 | `	/* Extract the attribute value if available.` |
|       - | 1710 | `	 */` |
|       9 | 1711 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1712 | `	return pValue;` |
|       5 | 1713 | `}` |
|       - | 1714 | `/*` |
|       - | 1715 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1716 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1717 | ` */` |
|     ! 0 | 1718 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1719 | `{` |
|       - | 1720 | `	ph7_class *pClass;` |
|     ! 0 | 1721 | `	if( pLength ){` |
|     ! 0 | 1722 | `		*pLength = 0;` |
|     ! 0 | 1723 | `	}` |
|       - | 1724 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1725 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1726 | `		return 0;` |
|       - | 1727 | `	}` |
|       - | 1728 | `	/* Point to the class */` |
|     ! 0 | 1729 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1730 | `	/* Return the class name */` |
|     ! 0 | 1731 | `	if( pLength ){` |
|     ! 0 | 1732 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1733 | `	}` |
|     ! 0 | 1734 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1735 | `}` |
|       - | 1736 | `/*` |
|       - | 1737 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1738 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1739 | ` */` |
|    1680 | 1740 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       4 | 1741 | `{` |
|       - | 1742 | `	SyString sData;` |
|       - | 1743 | `	int rc;` |
|    1684 | 1744 | `	if( nLen < 0 ){` |
|     ! 0 | 1745 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1746 | `	}` |
|    1684 | 1747 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|    1684 | 1748 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|    1684 | 1749 | `	return rc;` |
|       4 | 1750 | `}` |
|       - | 1751 | `/*` |
|       - | 1752 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1753 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1754 | ` */` |
|       2 | 1755 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1756 | `{` |
|       - | 1757 | `	va_list ap;` |
|       - | 1758 | `	int rc;` |
|       3 | 1759 | `	va_start(ap,zFormat);` |
|       3 | 1760 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1761 | `	va_end(ap);` |
|       3 | 1762 | `	return rc;` |
|       1 | 1763 | `}` |
|       - | 1764 | `/*` |
|       - | 1765 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1766 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1767 | ` */` |
|      22 | 1768 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       4 | 1769 | `{` |
|      26 | 1770 | `	int rc = PH7_OK;` |
|      26 | 1771 | `	if( zErr ){` |
|      26 | 1772 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      11 | 1773 | `	}` |
|      26 | 1774 | `	return rc;` |
|       4 | 1775 | `}` |
|       - | 1776 | `/*` |
|       - | 1777 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1778 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1779 | ` */` |
|      40 | 1780 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       4 | 1781 | `{` |
|       - | 1782 | `	va_list ap;` |
|       - | 1783 | `	int rc;` |
|      44 | 1784 | `	if( zFormat == 0){` |
|     ! 0 | 1785 | `		return PH7_OK;` |
|       - | 1786 | `	}` |
|      44 | 1787 | `	va_start(ap,zFormat);` |
|      44 | 1788 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      44 | 1789 | `	va_end(ap);` |
|      44 | 1790 | `	return rc;` |
|      24 | 1791 | `}` |
|       - | 1792 | `/*` |
|       - | 1793 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1794 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1795 | ` */` |
|      34 | 1796 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1797 | `{` |
|       - | 1798 | `	sxu32 n;` |
|      35 | 1799 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1800 | `	return n;` |
|       1 | 1801 | `}` |
|       - | 1802 | `/*` |
|       - | 1803 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1804 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1805 | ` */` |
|     ! 0 | 1806 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1807 | `{` |
|     ! 0 | 1808 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1809 | `		return PH7_CORRUPT;` |
|       - | 1810 | `	}` |
|     ! 0 | 1811 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1812 | `	return PH7_OK;` |
|     ! 0 | 1813 | `}` |
|       - | 1814 | `/*` |
|       - | 1815 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1816 | ` *` |
|       - | 1817 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1818 | ` * {` |
|       - | 1819 | ` *	return pCtx->pVm;` |
|       - | 1820 | ` * }` |
|       - | 1821 | ` */` |
|       - | 1822 | `/*` |
|       - | 1823 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1824 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1825 | ` */` |
|   61588 | 1826 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       5 | 1827 | `{` |
|   61593 | 1828 | `	return pCtx->pFunc->pUserData;` |
|       5 | 1829 | `}` |
|       - | 1830 | `/*` |
|       - | 1831 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1832 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1833 | ` */` |
|       2 | 1834 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1835 | `{` |
|       - | 1836 | `	ph7_aux_data sAux;` |
|       - | 1837 | `	int rc;` |
|       3 | 1838 | `	sAux.pAuxData = pUserData;` |
|       3 | 1839 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1840 | `	return rc;` |
|       1 | 1841 | `}` |
|       - | 1842 | `/*` |
|       - | 1843 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1844 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1845 | ` */` |
|       4 | 1846 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1847 | `{` |
|       - | 1848 | `	ph7_aux_data *pAux;` |
|       5 | 1849 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       5 | 1850 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1851 | `}` |
|       - | 1852 | `/*` |
|       - | 1853 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1854 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1855 | ` */` |
|     ! 0 | 1856 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|     ! 0 | 1857 | `{` |
|       - | 1858 | `	ph7_aux_data *pAux;` |
|     ! 0 | 1859 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|     ! 0 | 1860 | `	return pAux ? pAux->pAuxData : 0;` |
|     ! 0 | 1861 | `}` |
|       - | 1862 | `/*` |
|       - | 1863 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1864 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1865 | ` */` |
|    6734 | 1866 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       5 | 1867 | `{` |
|    6739 | 1868 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       5 | 1869 | `}` |
|       - | 1870 | `/*` |
|       - | 1871 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1872 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1873 | ` */` |
|   29726 | 1874 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       5 | 1875 | `{` |
|       - | 1876 | `	SyString *pName;` |
|   29731 | 1877 | `	pName = &pCtx->pFunc->sName;` |
|   29731 | 1878 | `	return pName->zString;` |
|       5 | 1879 | `}` |
|       - | 1880 | `/*` |
|       - | 1881 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1882 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1883 | ` */` |
|   41630 | 1884 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       5 | 1885 | `{` |
|       - | 1886 | `	/* Invalidate any prior representation */` |
|   41635 | 1887 | `	PH7_MemObjRelease(pVal);` |
|   41635 | 1888 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|   41635 | 1889 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   41635 | 1890 | `	return PH7_OK;` |
|       5 | 1891 | `}` |
|       - | 1892 | `/*` |
|       - | 1893 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1894 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1895 | ` */` |
|   40318 | 1896 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       5 | 1897 | `{` |
|       - | 1898 | `	/* Invalidate any prior representation */` |
|   40323 | 1899 | `	PH7_MemObjRelease(pVal);` |
|   40323 | 1900 | `	pVal->x.iVal = iValue;` |
|   40323 | 1901 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   40323 | 1902 | `	return PH7_OK;` |
|       5 | 1903 | `}` |
|       - | 1904 | `/*` |
|       - | 1905 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1906 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1907 | ` */` |
|  420667 | 1908 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       5 | 1909 | `{` |
|       - | 1910 | `	/* Invalidate any prior representation */` |
|  420672 | 1911 | `	PH7_MemObjRelease(pVal);` |
|  420672 | 1912 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  420672 | 1913 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  420672 | 1914 | `	return PH7_OK;` |
|       5 | 1915 | `}` |
|       - | 1916 | `/*` |
|       - | 1917 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1918 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1919 | ` */` |
|    4396 | 1920 | `int ph7_value_null(ph7_value *pVal)` |
|       1 | 1921 | `{` |
|       - | 1922 | `	/* Invalidate any prior representation and set the NULL flag */` |
|    4397 | 1923 | `	PH7_MemObjRelease(pVal);` |
|    4397 | 1924 | `	return PH7_OK;` |
|       1 | 1925 | `}` |
|       - | 1926 | `/*` |
|       - | 1927 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1928 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1929 | ` */` |
|     898 | 1930 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       1 | 1931 | `{` |
|       - | 1932 | `	/* Invalidate any prior representation */` |
|     899 | 1933 | `	PH7_MemObjRelease(pVal);` |
|     899 | 1934 | `	pVal->rVal = (ph7_real)Value;` |
|     899 | 1935 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1936 | `	/* Try to get an integer representation also */` |
|     899 | 1937 | `	PH7_MemObjTryInteger(pVal);` |
|     899 | 1938 | `	return PH7_OK;` |
|       1 | 1939 | `}` |
|       - | 1940 | `/*` |
|       - | 1941 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1942 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1943 | ` */` |
| 1403276 | 1944 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       5 | 1945 | `{` |
| 1403281 | 1946 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1947 | `		/* Invalidate any prior representation */` |
|  438373 | 1948 | `		PH7_MemObjRelease(pVal);` |
|  438373 | 1949 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  219184 | 1950 | `	}` |
| 1403281 | 1951 | `	if( zString ){` |
| 1402507 | 1952 | `		if( nLen < 0 ){` |
|       - | 1953 | `			/* Compute length automatically */` |
|    5191 | 1954 | `			nLen = (int)SyStrlen(zString);` |
|    2593 | 1955 | `		}` |
|       - | 1956 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1957 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
| 1402507 | 1958 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1959 | `	}` |
|     775 | 1960 | `	return PH7_OK;` |
|  701643 | 1961 | `}` |
|       - | 1962 | `/*` |
|       - | 1963 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1964 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1965 | ` */` |
|      22 | 1966 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1967 | `{` |
|       - | 1968 | `	va_list ap;` |
|       - | 1969 | `	int rc;` |
|      23 | 1970 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1971 | `		/* Invalidate any prior representation */` |
|      19 | 1972 | `		PH7_MemObjRelease(pVal);` |
|      19 | 1973 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       9 | 1974 | `	}` |
|      23 | 1975 | `	va_start(ap,zFormat);` |
|      23 | 1976 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      23 | 1977 | `	va_end(ap);` |
|       - | 1978 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|      23 | 1979 | `	return rc;` |
|       1 | 1980 | `}` |
|       - | 1981 | `/*` |
|       - | 1982 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1983 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1984 | ` */` |
|  165374 | 1985 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       5 | 1986 | `{` |
|       - | 1987 | `	/* Reset the string cursor */` |
|  165379 | 1988 | `	SyBlobReset(&pVal->sBlob);` |
|  165379 | 1989 | `	return PH7_OK;` |
|       5 | 1990 | `}` |
|       - | 1991 | `/*` |
|       - | 1992 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1993 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1994 | ` */` |
|    5216 | 1995 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       5 | 1996 | `{` |
|       - | 1997 | `	/* Invalidate any prior representation */` |
|    5221 | 1998 | `	PH7_MemObjRelease(pVal);` |
|       - | 1999 | `	/* Reflect the new type */` |
|    5221 | 2000 | `	pVal->x.pOther = pUserData;` |
|    5221 | 2001 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    5221 | 2002 | `	return PH7_OK;` |
|       5 | 2003 | `}` |
|       - | 2004 | `/*` |
|       - | 2005 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 2006 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2007 | ` */` |
|    4010 | 2008 | `int ph7_value_release(ph7_value *pVal)` |
|       5 | 2009 | `{` |
|    4015 | 2010 | `	PH7_MemObjRelease(pVal);` |
|    4015 | 2011 | `	return PH7_OK;` |
|       5 | 2012 | `}` |
|       - | 2013 | `/*` |
|       - | 2014 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 2015 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2016 | ` */` |
|   15492 | 2017 | `int ph7_value_is_int(ph7_value *pVal)` |
|       5 | 2018 | `{` |
|       - | 2019 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 2020 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 2021 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 2022 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 2023 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|   15497 | 2024 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       5 | 2025 | `}` |
|       - | 2026 | `/*` |
|       - | 2027 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 2028 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2029 | ` */` |
|   10494 | 2030 | `int ph7_value_is_float(ph7_value *pVal)` |
|       5 | 2031 | `{` |
|   10499 | 2032 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       5 | 2033 | `}` |
|       - | 2034 | `/*` |
|       - | 2035 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 2036 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2037 | ` */` |
|    2732 | 2038 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       5 | 2039 | `{` |
|    2737 | 2040 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       5 | 2041 | `}` |
|       - | 2042 | `/*` |
|       - | 2043 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2044 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2045 | ` */` |
|  110232 | 2046 | `int ph7_value_is_string(ph7_value *pVal)` |
|       5 | 2047 | `{` |
|  110237 | 2048 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       5 | 2049 | `}` |
|       - | 2050 | `/*` |
|       - | 2051 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2052 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2053 | ` */` |
|  299404 | 2054 | `int ph7_value_is_null(ph7_value *pVal)` |
|       5 | 2055 | `{` |
|  299409 | 2056 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       5 | 2057 | `}` |
|       - | 2058 | `/*` |
|       - | 2059 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2060 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2061 | ` */` |
|    1116 | 2062 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       5 | 2063 | `{` |
|       - | 2064 | `	int rc;` |
|    1121 | 2065 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|    1121 | 2066 | `	return rc;` |
|       5 | 2067 | `}` |
|       - | 2068 | `/*` |
|       - | 2069 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2070 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2071 | ` */` |
|   36760 | 2072 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       5 | 2073 | `{` |
|       - | 2074 | `	int rc;` |
|   36765 | 2075 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   36765 | 2076 | `	return rc;` |
|       5 | 2077 | `}` |
|       - | 2078 | `/*` |
|       - | 2079 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2080 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2081 | ` */` |
|      12 | 2082 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2083 | `{` |
|      13 | 2084 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2085 | `}` |
|       - | 2086 | `/*` |
|       - | 2087 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2088 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2089 | ` */` |
|  172126 | 2090 | `int ph7_value_is_array(ph7_value *pVal)` |
|       5 | 2091 | `{` |
|  172131 | 2092 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       5 | 2093 | `}` |
|       - | 2094 | `/*` |
|       - | 2095 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2096 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2097 | ` */` |
|    7158 | 2098 | `int ph7_value_is_object(ph7_value *pVal)` |
|       5 | 2099 | `{` |
|    7163 | 2100 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       5 | 2101 | `}` |
|       - | 2102 | `/*` |
|       - | 2103 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2104 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2105 | ` */` |
|   34772 | 2106 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       5 | 2107 | `{` |
|   34777 | 2108 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       5 | 2109 | `}` |
|       - | 2110 | `/*` |
|       - | 2111 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2112 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2113 | ` */` |
|   34226 | 2114 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       5 | 2115 | `{` |
|       - | 2116 | `	int rc;` |
|   34231 | 2117 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   34231 | 2118 | `	return rc;` |
|       5 | 2119 | `}` |
|       - | 2120 | `/*` |
|       - | 2121 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2122 | ` * Check if a value holds a Fiber instance.` |
|       - | 2123 | ` */` |
|     ! 0 | 2124 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2125 | `{` |
|     ! 0 | 2126 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2127 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2128 | `}` |
|       - | 2129 | `/*` |
|       - | 2130 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2131 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2132 | ` */` |
|     ! 0 | 2133 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2134 | `{` |
|     ! 0 | 2135 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2136 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2137 | `}` |
|       - | 2138 | `/*` |
|       - | 2139 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2140 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2141 | ` */` |
|     ! 0 | 2142 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2143 | `{` |
|     ! 0 | 2144 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2145 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2146 | `}` |
|       - | 2147 | `/*` |
|       - | 2148 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2149 | ` * Check if a Fiber is currently suspended.` |
|       - | 2150 | ` */` |
|     ! 0 | 2151 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2152 | `{` |
|     ! 0 | 2153 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2154 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2155 | `}` |
|       - | 2156 | `/*` |
|       - | 2157 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2158 | ` * Check if a Fiber has completed execution.` |
|       - | 2159 | ` */` |
|     ! 0 | 2160 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2161 | `{` |
|     ! 0 | 2162 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2163 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2164 | `}` |
|       - | 2165 | `/*` |
|       - | 2166 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2167 | ` * Get the return value of a terminated Fiber.` |
|       - | 2168 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2169 | ` */` |
|     ! 0 | 2170 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2171 | `{` |
|     ! 0 | 2172 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2173 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2174 | `}` |
|       - | 2175 |  |
