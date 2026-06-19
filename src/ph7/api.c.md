# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 765/1079 lines (70.90%)

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
|    6268 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       2 |   79 |  |
|    6270 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    6270 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    6270 |   83 | `	switch(nOp){` |
|    3134 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    6270 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    6270 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    6270 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    6270 |   93 | `		pConf->xErr     = xConsumer;` |
|    6270 |   94 | `		pConf->pErrData = pUserData;` |
|    6270 |   95 | `		break;` |
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
|     ! 0 |  129 | `	default:` |
|       - |  130 | `		/* Unknown configuration verb */` |
|     ! 0 |  131 | `		rc = PH7_CORRUPT;` |
|     ! 0 |  132 | `		break;` |
|       - |  133 | `	} /* Switch() */` |
|    6270 |  134 | `	return rc;` |
|       2 |  135 |  |
|       - |  136 | `/*` |
|       - |  137 | ` * Configure the PH7 library.` |
|       - |  138 | ` * return PH7_OK on success.Any other return value` |
|       - |  139 | ` * indicates failure.` |
|       - |  140 | ` * Refer to [ph7_lib_config()].` |
|       - |  141 | ` */` |
|    9432 |  142 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       2 |  143 |  |
|    9434 |  144 | `	int rc = PH7_OK;` |
|    9434 |  145 | `	switch(nOp){` |
|    1572 |  146 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  147 | `			/* Install a virtual file system */` |
|    3146 |  148 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    3146 |  149 | `			sMPGlobal.pVfs = pVfs;` |
|    3146 |  150 | `			break;` |
|       - |  151 | `								}` |
|    1572 |  152 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  153 | `			/* Use an alternative low-level memory allocation routines */` |
|    3146 |  154 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  155 | `			/* Save the memory failure callback (if available) */` |
|    3146 |  156 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    3146 |  157 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    3146 |  158 | `			if( pMethods == 0 ){` |
|       - |  159 | `				/* Use the built-in memory allocation subsystem */` |
|    3146 |  160 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|    1574 |  161 | `			}else{` |
|     ! 0 |  162 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  163 | `			}` |
|    3146 |  164 | `			break;` |
|       - |  165 | `										  }` |
|     ! 0 |  166 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  167 | `			/* Memory failure callback */` |
|     ! 0 |  168 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  169 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  170 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  171 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  172 | `			break;` |
|       - |  173 | `												 }` |
|    1572 |  174 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  175 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  176 | `			/* Use an alternative low-level mutex subsystem */` |
|    3146 |  177 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  178 | `#if defined (UNTRUST)` |
|       - |  179 | `			if( pMethods == 0 ){` |
|       - |  180 | `				rc = PH7_CORRUPT;` |
|       - |  181 | `			}` |
|       - |  182 | `#endif` |
|       - |  183 | `			/* Sanity check */` |
|    3146 |  184 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  185 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  186 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  187 | `				break;` |
|       - |  188 | `			}` |
|    3146 |  189 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  190 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  191 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  192 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  193 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  194 | `				}` |
|     ! 0 |  195 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  196 | `			}` |
|       - |  197 | `			/* Initialize and install the new mutex subsystem */` |
|    3146 |  198 | `			if( pMethods->xGlobalInit ){` |
|       2 |  199 | `				rc = pMethods->xGlobalInit();` |
|       2 |  200 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  201 | `					break;` |
|       - |  202 | `				}` |
|     ! 0 |  203 | `			}` |
|       - |  204 | `			/* Create the global mutex */` |
|    3146 |  205 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    3146 |  206 | `			if( sMPGlobal.pMutex == 0 ){` |
|       - |  207 | `				/*` |
|       - |  208 | `				 * If the supplied mutex subsystem is so sick that we are unable to` |
|       - |  209 | `				 * create a single mutex,there is no much we can do here.` |
|       - |  210 | `				 */` |
|     ! 0 |  211 | `				if( pMethods->xGlobalRelease ){` |
|     ! 0 |  212 | `					pMethods->xGlobalRelease();` |
|     ! 0 |  213 | `				}` |
|     ! 0 |  214 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  215 | `				break;` |
|       - |  216 | `			}` |
|    3146 |  217 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    3146 |  218 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  219 | `				/* Set a default threading level */` |
|    3146 |  220 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|    1572 |  221 | `			}` |
|       - |  222 | `#endif` |
|    3146 |  223 | `			break;` |
|       - |  224 | `										   }` |
|     ! 0 |  225 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:` |
|       - |  226 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  227 | `			/* Single thread mode(Only one thread is allowed to play with the library) */` |
|     ! 0 |  228 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|       - |  229 | `#endif` |
|     ! 0 |  230 | `			break;` |
|     ! 0 |  231 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:` |
|       - |  232 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  233 | `			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines` |
|       - |  234 | `			 * may be shared between multiple threads).` |
|       - |  235 | `			 */` |
|     ! 0 |  236 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|       - |  237 | `#endif` |
|     ! 0 |  238 | `			break;` |
|     ! 0 |  239 | `		default:` |
|       - |  240 | `			/* Unknown configuration option */` |
|     ! 0 |  241 | `			rc = PH7_CORRUPT;` |
|     ! 0 |  242 | `			break;` |
|       - |  243 | `	}` |
|    9434 |  244 | `	return rc;` |
|       2 |  245 |  |
|       - |  246 | `/*` |
|       - |  247 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  248 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  249 | ` */` |
|    9432 |  250 | `int ph7_lib_config(int nConfigOp,...)` |
|       2 |  251 |  |
|       - |  252 | `	va_list ap;` |
|       - |  253 | `	int rc;` |
|       - |  254 |  |
|    9434 |  255 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  256 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  257 | `		return PH7_LOOKED;` |
|       - |  258 | `	}` |
|    9434 |  259 | `	va_start(ap,nConfigOp);` |
|    9434 |  260 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    9434 |  261 | `	va_end(ap);` |
|    9434 |  262 | `	return rc;` |
|    4718 |  263 |  |
|       - |  264 | `/*` |
|       - |  265 | ` * Global library initialization` |
|       - |  266 | ` * Refer to [ph7_lib_init()]` |
|       - |  267 | ` * This routine must be called to initialize the memory allocation subsystem,the mutex` |
|       - |  268 | ` * subsystem prior to doing any serious work with the library.The first thread to call` |
|       - |  269 | ` * this routine does the initialization process and set the magic number so no body later` |
|       - |  270 | ` * can re-initialize the library.If subsequent threads call this  routine before the first` |
|       - |  271 | ` * thread have finished the initialization process, then the subsequent threads must block` |
|       - |  272 | ` * until the initialization process is done.` |
|       - |  273 | ` */` |
|    3144 |  274 | `static sxi32 PH7CoreInitialize(void)` |
|       2 |  275 |  |
|       - |  276 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  277 | `#if defined(PH7_ENABLE_THREADS)` |
|    3146 |  278 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    3146 |  279 | `	SyMutex *pMaster = 0;` |
|       - |  280 | `#endif` |
|       - |  281 | `	int rc;` |
|       - |  282 | `	/*` |
|       - |  283 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  284 | `	 * is a no-op.` |
|       - |  285 | `	 */` |
|    3146 |  286 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  287 | `		return PH7_OK; /* Already initialized */` |
|       - |  288 | `	}` |
|       - |  289 | `	/* Point to the built-in vfs */` |
|    3146 |  290 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  291 | `	/* Install it */` |
|    3146 |  292 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  293 | `#if defined(PH7_ENABLE_THREADS)` |
|    3146 |  294 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    3146 |  295 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    3146 |  296 | `		if( pMutexMethods == 0 ){` |
|       - |  297 | `			/* Use the built-in mutex subsystem */` |
|    3146 |  298 | `			pMutexMethods = SyMutexExportMethods();` |
|    3146 |  299 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  300 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  301 | `			}` |
|       - |  302 | `			/* Install the mutex subsystem */` |
|    3146 |  303 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    3146 |  304 | `			if( rc != PH7_OK ){` |
|     ! 0 |  305 | `				return rc;` |
|       - |  306 | `			}` |
|    1572 |  307 | `		}` |
|       - |  308 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    3146 |  309 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    3146 |  310 | `		if( pMaster == 0 ){` |
|     ! 0 |  311 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  312 | `		}` |
|    1572 |  313 | `	}` |
|       - |  314 | `	/* Lock the master mutex */` |
|    3146 |  315 | `	rc = PH7_OK;` |
|    3146 |  316 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    4718 |  317 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  318 | `#endif` |
|    3146 |  319 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  320 | `			/* Install a memory subsystem */` |
|    3146 |  321 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    3146 |  322 | `			if( rc != PH7_OK ){` |
|       - |  323 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  324 | `				goto End;` |
|       - |  325 | `			}` |
|    1572 |  326 | `		}` |
|       - |  327 | `#if defined(PH7_ENABLE_THREADS)` |
|    3146 |  328 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  329 | `			/* Protect the memory allocation subsystem */` |
|    3146 |  330 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    3146 |  331 | `			if( rc != PH7_OK ){` |
|     ! 0 |  332 | `				goto End;` |
|       - |  333 | `			}` |
|    1572 |  334 | `		}` |
|       - |  335 | `#endif` |
|       - |  336 | `		/* Our library is initialized,set the magic number */` |
|    3146 |  337 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    3146 |  338 | `		rc = PH7_OK;` |
|       - |  339 | `#if defined(PH7_ENABLE_THREADS)` |
|    1572 |  340 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  341 | `#endif` |
|     ! 0 |  342 | `End:` |
|       - |  343 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  344 | `	/* Unlock the master mutex */` |
|    3146 |  345 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  346 | `#endif` |
|    3146 |  347 | `	return rc;` |
|    1574 |  348 |  |
|       - |  349 | `/*` |
|       - |  350 | ` * [CAPIREF: ph7_lib_init()]` |
|       - |  351 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  352 | ` */` |
|     ! 0 |  353 | `int ph7_lib_init(void)` |
|     ! 0 |  354 |  |
|       - |  355 | `	int rc;` |
|     ! 0 |  356 | `	rc = PH7CoreInitialize();` |
|     ! 0 |  357 | `	return rc;` |
|     ! 0 |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * Release an active PH7 engine and it's associated active virtual machines.` |
|       - |  361 | ` */` |
|    3144 |  362 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       2 |  363 |  |
|       - |  364 | `	ph7_vm *pVm,*pNext;` |
|       - |  365 | `	/* Release all active VM */` |
|    3146 |  366 | `	pVm = pEngine->pVms;` |
|    1572 |  367 | `	for(;;){` |
|    3146 |  368 | `		if( pEngine->iVm <= 0 ){` |
|    3146 |  369 | `			break;` |
|       - |  370 | `		}` |
|     ! 0 |  371 | `		pNext = pVm->pNext;` |
|     ! 0 |  372 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  373 | `		pVm = pNext;` |
|     ! 0 |  374 | `		pEngine->iVm--;` |
|     ! 0 |  375 | `	}` |
|       - |  376 | `	/* Set a dummy magic number */` |
|    3146 |  377 | `	pEngine->nMagic = 0x7635;` |
|       - |  378 | `	/* Release the private memory subsystem */` |
|    3146 |  379 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    3146 |  380 | `	return PH7_OK;` |
|       2 |  381 |  |
|       - |  382 | `/*` |
|       - |  383 | ` * Release all resources consumed by the library.` |
|       - |  384 | ` * If PH7 is already shut down when this routine` |
|       - |  385 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  386 | ` * Note: This call is not thread safe.` |
|       - |  387 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  388 | ` */` |
|     314 |  389 | `static void PH7CoreShutdown(void)` |
|       1 |  390 |  |
|       - |  391 | `	ph7 *pEngine,*pNext;` |
|       - |  392 | `	/* Release all active engines first */` |
|     315 |  393 | `	pEngine = sMPGlobal.pEngines;` |
|     314 |  394 | `	for(;;){` |
|     629 |  395 | `		if( sMPGlobal.nEngine < 1 ){` |
|     315 |  396 | `			break;` |
|       - |  397 | `		}` |
|     315 |  398 | `		pNext = pEngine->pNext;` |
|     315 |  399 | `		EngineRelease(pEngine);` |
|     315 |  400 | `		pEngine = pNext;` |
|     315 |  401 | `		sMPGlobal.nEngine--;` |
|       1 |  402 | `	}` |
|       - |  403 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  404 | `	/* Release the mutex subsystem */` |
|     315 |  405 | `	if( sMPGlobal.pMutexMethods ){` |
|     315 |  406 | `		if( sMPGlobal.pMutex ){` |
|     315 |  407 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     315 |  408 | `			sMPGlobal.pMutex = 0;` |
|     157 |  409 | `		}` |
|     315 |  410 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       1 |  411 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  412 | `		}` |
|     315 |  413 | `		sMPGlobal.pMutexMethods = 0;` |
|     157 |  414 | `	}` |
|     315 |  415 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  416 | `#endif` |
|     315 |  417 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  418 | `		/* Release the memory backend */` |
|     315 |  419 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     157 |  420 | `	}` |
|     315 |  421 | `	sMPGlobal.nMagic = 0x1928;` |
|     315 |  422 |  |
|       - |  423 | `/*` |
|       - |  424 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  425 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  426 | ` */` |
|     314 |  427 | `int ph7_lib_shutdown(void)` |
|       1 |  428 |  |
|     315 |  429 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  430 | `		/* Already shut */` |
|     ! 0 |  431 | `		return PH7_OK;` |
|       - |  432 | `	}` |
|     315 |  433 | `	PH7CoreShutdown();` |
|     315 |  434 | `	return PH7_OK;` |
|     158 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * [CAPIREF: ph7_lib_is_threadsafe()]` |
|       - |  438 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  439 | ` */` |
|     ! 0 |  440 | `int ph7_lib_is_threadsafe(void)` |
|     ! 0 |  441 |  |
|     ! 0 |  442 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|     ! 0 |  443 | `		return 0;` |
|       - |  444 | `	}` |
|       - |  445 | `#if defined(PH7_ENABLE_THREADS)` |
|     ! 0 |  446 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  447 | `			/* Muli-threading support is enabled */` |
|     ! 0 |  448 | `			return 1;` |
|     ! 0 |  449 | `		}else{` |
|       - |  450 | `			/* Single-threading */` |
|     ! 0 |  451 | `			return 0;` |
|       - |  452 | `		}` |
|       - |  453 | `#else` |
|       - |  454 | `	return 0;` |
|       - |  455 | `#endif` |
|     ! 0 |  456 |  |
|       - |  457 | `/*` |
|       - |  458 | ` * [CAPIREF: ph7_lib_version()]` |
|       - |  459 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  460 | ` */` |
|      10 |  461 | `const char * ph7_lib_version(void)` |
|       2 |  462 |  |
|      12 |  463 | `	return PH7_VERSION;` |
|       2 |  464 |  |
|       - |  465 | `/*` |
|       - |  466 | ` * [CAPIREF: ph7_lib_signature()]` |
|       - |  467 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  468 | ` */` |
|      10 |  469 | `const char * ph7_lib_signature(void)` |
|       1 |  470 |  |
|      11 |  471 | `	return PH7_SIG;` |
|       1 |  472 |  |
|       - |  473 | `/*` |
|       - |  474 | ` * [CAPIREF: ph7_lib_ident()]` |
|       - |  475 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  476 | ` */` |
|       2 |  477 | `const char * ph7_lib_ident(void)` |
|       1 |  478 |  |
|       3 |  479 | `	return PH7_IDENT;` |
|       1 |  480 |  |
|       - |  481 | `/*` |
|       - |  482 | ` * [CAPIREF: ph7_lib_copyright()]` |
|       - |  483 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  484 | ` */` |
|     ! 0 |  485 | `const char * ph7_lib_copyright(void)` |
|     ! 0 |  486 |  |
|     ! 0 |  487 | `	return PH7_COPYRIGHT;` |
|     ! 0 |  488 |  |
|       - |  489 | `/*` |
|       - |  490 | ` * [CAPIREF: ph7_config()]` |
|       - |  491 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  492 | ` */` |
|    6268 |  493 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       2 |  494 |  |
|       - |  495 | `	va_list ap;` |
|       - |  496 | `	int rc;` |
|    6270 |  497 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  498 | `		return PH7_CORRUPT;` |
|       - |  499 | `	}` |
|       - |  500 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  501 | `	 /* Acquire engine mutex */` |
|    6270 |  502 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    6270 |  503 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    6268 |  504 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  505 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  506 | `	 }` |
|       - |  507 | `#endif` |
|    6270 |  508 | `	 va_start(ap,nConfigOp);` |
|    6270 |  509 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    6270 |  510 | `	 va_end(ap);` |
|       - |  511 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  512 | `	 /* Leave engine mutex */` |
|    6270 |  513 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  514 | `#endif` |
|    6270 |  515 | `	return rc;` |
|    3136 |  516 |  |
|       - |  517 | `/*` |
|       - |  518 | ` * [CAPIREF: ph7_init()]` |
|       - |  519 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  520 | ` */` |
|    3144 |  521 | `int ph7_init(ph7 **ppEngine)` |
|       2 |  522 |  |
|       - |  523 | `	ph7 *pEngine;` |
|       - |  524 | `	int rc;` |
|       - |  525 | `#if defined(UNTRUST)` |
|       - |  526 | `	if( ppEngine == 0 ){` |
|       - |  527 | `		return PH7_CORRUPT;` |
|       - |  528 | `	}` |
|       - |  529 | `#endif` |
|    3146 |  530 | `	*ppEngine = 0;` |
|       - |  531 | `	/* One-time automatic library initialization */` |
|    3146 |  532 | `	rc = PH7CoreInitialize();` |
|    3146 |  533 | `	if( rc != PH7_OK ){` |
|     ! 0 |  534 | `		return rc;` |
|       - |  535 | `	}` |
|       - |  536 | `	/* Allocate a new engine */` |
|    3146 |  537 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    3146 |  538 | `	if( pEngine == 0 ){` |
|     ! 0 |  539 | `		return PH7_NOMEM;` |
|       - |  540 | `	}` |
|       - |  541 | `	/* Zero the structure */` |
|    3146 |  542 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  543 | `	/* Initialize engine fields */` |
|    3146 |  544 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    3146 |  545 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    3146 |  546 | `	if( rc != PH7_OK ){` |
|     ! 0 |  547 | `		goto Release;` |
|       - |  548 | `	}` |
|       - |  549 | `#if defined(PH7_ENABLE_THREADS)` |
|    3146 |  550 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  551 | `#endif` |
|       - |  552 | `	/* Default configuration */` |
|    3146 |  553 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  554 | `	/* Install a default compile-time error consumer routine */` |
|    3146 |  555 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  556 | `	/* Built-in vfs */` |
|    3146 |  557 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  558 | `#if defined(PH7_ENABLE_THREADS)` |
|    3146 |  559 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  560 | `		 /* Associate a recursive mutex with this instance */` |
|    3146 |  561 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    3146 |  562 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  563 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  564 | `			 goto Release;` |
|       - |  565 | `		 }` |
|    1572 |  566 | `	 }` |
|       - |  567 | `#endif` |
|       - |  568 | `	/* Link to the list of active engines */` |
|       - |  569 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  570 | `	/* Enter the global mutex */` |
|    3146 |  571 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  572 | `#endif` |
|    3146 |  573 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    3146 |  574 | `	sMPGlobal.nEngine++;` |
|       - |  575 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  576 | `	/* Leave the global mutex */` |
|    3146 |  577 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  578 | `#endif` |
|       - |  579 | `	/* Write a pointer to the new instance */` |
|    3146 |  580 | `	*ppEngine = pEngine;` |
|    3146 |  581 | `	return PH7_OK;` |
|     ! 0 |  582 | `Release:` |
|     ! 0 |  583 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  584 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  585 | `	return rc;` |
|    1574 |  586 |  |
|       - |  587 | `/*` |
|       - |  588 | ` * [CAPIREF: ph7_release()]` |
|       - |  589 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  590 | ` */` |
|    2830 |  591 | `int ph7_release(ph7 *pEngine)` |
|       2 |  592 |  |
|       - |  593 | `	int rc;` |
|    2832 |  594 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  595 | `		return PH7_CORRUPT;` |
|       - |  596 | `	}` |
|       - |  597 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  598 | `	 /* Acquire engine mutex */` |
|    2832 |  599 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2832 |  600 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2830 |  601 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  602 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  603 | `	 }` |
|       - |  604 | `#endif` |
|       - |  605 | `	/* Release the engine */` |
|    2832 |  606 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  607 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  608 | `	 /* Leave engine mutex */` |
|    2832 |  609 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  610 | `	 /* Release engine mutex */` |
|    2832 |  611 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  612 | `#endif` |
|       - |  613 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  614 | `	/* Enter the global mutex */` |
|    2832 |  615 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  616 | `#endif` |
|       - |  617 | `	/* Unlink from the list of active engines */` |
|    2832 |  618 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    2832 |  619 | `	sMPGlobal.nEngine--;` |
|       - |  620 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  621 | `	/* Leave the global mutex */` |
|    2832 |  622 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  623 | `#endif` |
|       - |  624 | `	/* Release the memory chunk allocated to this engine */` |
|    2832 |  625 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    2832 |  626 | `	return rc;` |
|    1417 |  627 |  |
|       - |  628 | `/*` |
|       - |  629 | ` * Compile a raw PHP script.` |
|       - |  630 | ` * To execute a PHP code, it must first be compiled into a byte-code program using this routine.` |
|       - |  631 | ` * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]` |
|       - |  632 | ` * should  display the appropriate error message and this function set ppVm to null and return` |
|       - |  633 | ` * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled` |
|       - |  634 | ` * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].` |
|       - |  635 | ` * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script` |
|       - |  636 | ` * for evaluation.` |
|       - |  637 | ` */` |
|    3140 |  638 | `static sxi32 ProcessScript(` |
|       - |  639 | `	ph7 *pEngine,          /* Running PH7 engine */` |
|       - |  640 | `	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */` |
|       - |  641 | `	SyString *pScript,     /* Raw PHP script to compile */` |
|       - |  642 | `	sxi32 iFlags,          /* Compile-time flags */` |
|       - |  643 | `	const char *zFilePath  /* File path if script come from a file. NULL otherwise */` |
|       - |  644 | `	)` |
|       2 |  645 |  |
|       - |  646 | `	ph7_vm *pVm;` |
|       - |  647 | `	int rc;` |
|       - |  648 | `	/* Allocate a new virtual machine */` |
|    3142 |  649 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    3142 |  650 | `	if( pVm == 0 ){` |
|       - |  651 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  652 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  653 | `		if( ppVm ){` |
|     ! 0 |  654 | `			*ppVm = 0;` |
|     ! 0 |  655 | `		}` |
|     ! 0 |  656 | `		return PH7_NOMEM;` |
|       - |  657 | `	}` |
|    3142 |  658 | `	if( iFlags < 0 ){` |
|       - |  659 | `		/* Default compile-time flags */` |
|     ! 0 |  660 | `		iFlags = 0;` |
|     ! 0 |  661 | `	}` |
|       - |  662 | `	/* Initialize the Virtual Machine */` |
|    3142 |  663 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    3142 |  664 | `	if( rc != PH7_OK ){` |
|     ! 0 |  665 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  666 | `		if( ppVm ){` |
|     ! 0 |  667 | `			*ppVm = 0;` |
|     ! 0 |  668 | `		}` |
|     ! 0 |  669 | `		return PH7_VM_ERR;` |
|       - |  670 | `	}` |
|    3142 |  671 | `	if( zFilePath ){` |
|       - |  672 | `		/* Push processed file path */` |
|    3134 |  673 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|    1566 |  674 | `	}` |
|       - |  675 | `	/* Reset the error message consumer */` |
|    3142 |  676 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  677 | `	/* Compile the script */` |
|    3142 |  678 | `	PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    3142 |  679 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     315 |  680 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  681 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     315 |  682 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     315 |  683 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     315 |  684 | `		if( ppVm ){` |
|     315 |  685 | `			*ppVm = 0;` |
|     157 |  686 | `		}` |
|     315 |  687 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  688 | `	}` |
|       - |  689 | `	/* Prepare the virtual machine for bytecode execution */` |
|    2828 |  690 | `	rc = PH7_VmMakeReady(pVm);` |
|    2828 |  691 | `	if( rc != PH7_OK ){` |
|     ! 0 |  692 | `		goto Release;` |
|       - |  693 | `	}` |
|       - |  694 | `	/* Install local import path which is the current directory */` |
|    2828 |  695 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  696 | `#if defined(PH7_ENABLE_THREADS)` |
|    2828 |  697 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  698 | `		 /* Associate a recursive mutex with this instance */` |
|    2828 |  699 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    2828 |  700 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  701 | `			 goto Release;` |
|       - |  702 | `		 }` |
|    1413 |  703 | `	 }` |
|       - |  704 | `#endif` |
|       - |  705 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    2828 |  706 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    2828 |  707 | `	pEngine->iVm++;` |
|       - |  708 | `	/* Point to the freshly created VM */` |
|    2828 |  709 | `	*ppVm = pVm;` |
|       - |  710 | `	/* Ready to execute PH7 bytecode */` |
|    2828 |  711 | `	return PH7_OK;` |
|     ! 0 |  712 | `Release:` |
|     ! 0 |  713 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     ! 0 |  714 | `	SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  715 | `	*ppVm = 0;` |
|     ! 0 |  716 | `	return PH7_VM_ERR;` |
|    1572 |  717 |  |
|       - |  718 | `/*` |
|       - |  719 | ` * [CAPIREF: ph7_compile()]` |
|       - |  720 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  721 | ` */` |
|     ! 0 |  722 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|     ! 0 |  723 |  |
|       - |  724 | `	SyString sScript;` |
|       - |  725 | `	int rc;` |
|     ! 0 |  726 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  727 | `		return PH7_CORRUPT;` |
|       - |  728 | `	}` |
|     ! 0 |  729 | `	if( nLen < 0 ){` |
|       - |  730 | `		/* Compute input length automatically */` |
|     ! 0 |  731 | `		nLen = (int)SyStrlen(zSource);` |
|     ! 0 |  732 | `	}` |
|     ! 0 |  733 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  734 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  735 | `	 /* Acquire engine mutex */` |
|     ! 0 |  736 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  737 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  738 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  739 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  740 | `	 }` |
|       - |  741 | `#endif` |
|       - |  742 | `	/* Compile the script */` |
|     ! 0 |  743 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|       - |  744 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  745 | `	 /* Leave engine mutex */` |
|     ! 0 |  746 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  747 | `#endif` |
|       - |  748 | `	/* Compilation result */` |
|     ! 0 |  749 | `	return rc;` |
|     ! 0 |  750 |  |
|       - |  751 | `/*` |
|       - |  752 | ` * [CAPIREF: ph7_compile_v2()]` |
|       - |  753 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  754 | ` */` |
|       8 |  755 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       1 |  756 |  |
|       - |  757 | `	SyString sScript;` |
|       - |  758 | `	int rc;` |
|       9 |  759 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  760 | `		return PH7_CORRUPT;` |
|       - |  761 | `	}` |
|       9 |  762 | `	if( nLen < 0 ){` |
|       - |  763 | `		/* Compute input length automatically */` |
|       9 |  764 | `		nLen = (int)SyStrlen(zSource);` |
|       4 |  765 | `	}` |
|       9 |  766 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  767 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  768 | `	 /* Acquire engine mutex */` |
|       9 |  769 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       9 |  770 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       8 |  771 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  772 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  773 | `	 }` |
|       - |  774 | `#endif` |
|       - |  775 | `	/* Compile the script */` |
|       9 |  776 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  777 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  778 | `	 /* Leave engine mutex */` |
|       9 |  779 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  780 | `#endif` |
|       - |  781 | `	/* Compilation result */` |
|       9 |  782 | `	return rc;` |
|       5 |  783 |  |
|       - |  784 | `/*` |
|       - |  785 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  786 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  787 | ` */` |
|    3132 |  788 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       2 |  789 |  |
|       - |  790 | `	const ph7_vfs *pVfs;` |
|       - |  791 | `	int rc;` |
|    3134 |  792 | `	if( ppOutVm ){` |
|    3134 |  793 | `		*ppOutVm = 0;` |
|    1566 |  794 | `	}` |
|    3134 |  795 | `	rc = PH7_OK; /* cc warning */` |
|    3134 |  796 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  797 | `		return PH7_CORRUPT;` |
|       - |  798 | `	}` |
|       - |  799 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  800 | `	 /* Acquire engine mutex */` |
|    3134 |  801 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3134 |  802 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3132 |  803 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  804 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  805 | `	 }` |
|       - |  806 | `#endif` |
|       - |  807 | `	 /*` |
|       - |  808 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  809 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  810 | `	  */` |
|    3134 |  811 | `	 pVfs = pEngine->pVfs;` |
|    3134 |  812 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  813 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  814 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  815 | `	 }else{` |
|    3134 |  816 | `		 void *pMapView = 0; /* cc warning */` |
|    3134 |  817 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  818 | `		 SyString sScript;` |
|       - |  819 | `		 /* Try to get a memory view of the whole file */` |
|    3134 |  820 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    3134 |  821 | `		 if( rc != PH7_OK ){` |
|       - |  822 | `			 /* Assume an IO error */` |
|     ! 0 |  823 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  824 | `		 }else{` |
|       - |  825 | `			 /* Compile the file */` |
|    3134 |  826 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    3134 |  827 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  828 | `			 /* Release the memory view of the whole file */` |
|    3134 |  829 | `			 if( pVfs->xUnmap ){` |
|    3134 |  830 | `				 pVfs->xUnmap(pMapView,nSize);` |
|    1566 |  831 | `			 }` |
|       - |  832 | `		 }` |
|       - |  833 | `	 }` |
|       - |  834 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  835 | `	 /* Leave engine mutex */` |
|    3134 |  836 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  837 | `#endif` |
|       - |  838 | `	/* Compilation result */` |
|    3134 |  839 | `	return rc;` |
|    1568 |  840 |  |
|       - |  841 | `/*` |
|       - |  842 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|       - |  843 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  844 | ` */` |
|       2 |  845 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|       1 |  846 |  |
|       - |  847 | `	int rc;` |
|       - |  848 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       3 |  849 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  850 | `		return PH7_CORRUPT;` |
|       - |  851 | `	}` |
|       - |  852 | `#ifdef UNTRUST` |
|       - |  853 | `	if( xConsumer == 0 ){` |
|       - |  854 | `		return PH7_CORRUPT;` |
|       - |  855 | `	}` |
|       - |  856 | `#endif` |
|       - |  857 | `	/* Dump VM instructions */` |
|       3 |  858 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|       3 |  859 | `	return rc;` |
|       2 |  860 |  |
|       - |  861 | `/*` |
|       - |  862 | ` * [CAPIREF: ph7_vm_config()]` |
|       - |  863 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  864 | ` */` |
|   45744 |  865 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       2 |  866 |  |
|       - |  867 | `	va_list ap;` |
|       - |  868 | `	int rc;` |
|       - |  869 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   45746 |  870 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  871 | `		return PH7_CORRUPT;` |
|       - |  872 | `	}` |
|       - |  873 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  874 | `	 /* Acquire VM mutex */` |
|   45746 |  875 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   45746 |  876 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   45744 |  877 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  878 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  879 | `	 }` |
|       - |  880 | `#endif` |
|       - |  881 | `	/* Confiugure the virtual machine */` |
|   45746 |  882 | `	va_start(ap,iConfigOp);` |
|   45746 |  883 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   45746 |  884 | `	va_end(ap);` |
|       - |  885 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  886 | `	 /* Leave VM mutex */` |
|   45746 |  887 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  888 | `#endif` |
|   45746 |  889 | `	return rc;` |
|   22874 |  890 |  |
|       - |  891 | `/*` |
|       - |  892 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  893 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  894 | ` */` |
|    2832 |  895 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       2 |  896 |  |
|       - |  897 | `	int rc;` |
|       - |  898 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    2834 |  899 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  900 | `		return PH7_CORRUPT;` |
|       - |  901 | `	}` |
|       - |  902 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  903 | `	 /* Acquire VM mutex */` |
|    2834 |  904 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2834 |  905 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2832 |  906 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  907 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  908 | `	 }` |
|       - |  909 | `#endif` |
|       - |  910 | `	/* Execute PH7 byte-code */` |
|    2834 |  911 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    2834 |  912 | `	if( pExitStatus ){` |
|       - |  913 | `		/* Exit status */` |
|    2812 |  914 | `		*pExitStatus = pVm->iExitStatus;` |
|    1405 |  915 | `	}` |
|       - |  916 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  917 | `	 /* Leave VM mutex */` |
|    2834 |  918 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  919 | `#endif` |
|       - |  920 | `	/* Execution result */` |
|    2834 |  921 | `	return rc;` |
|    1418 |  922 |  |
|       - |  923 | `/*` |
|       - |  924 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  925 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  926 | ` */` |
|       6 |  927 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  928 |  |
|       - |  929 | `	int rc;` |
|       - |  930 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       6 |  931 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  932 | `		return PH7_CORRUPT;` |
|       - |  933 | `	}` |
|       - |  934 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  935 | `	 /* Acquire VM mutex */` |
|       6 |  936 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       6 |  937 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       6 |  938 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  939 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  940 | `	 }` |
|       - |  941 | `#endif` |
|       6 |  942 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  943 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  944 | `	 /* Leave VM mutex */` |
|       6 |  945 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  946 | `#endif` |
|       6 |  947 | `	return rc;` |
|       3 |  948 |  |
|       - |  949 | `/*` |
|       - |  950 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  951 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  952 | ` */` |
|    2826 |  953 | `int ph7_vm_release(ph7_vm *pVm)` |
|       2 |  954 |  |
|       - |  955 | `	ph7 *pEngine;` |
|       - |  956 | `	int rc;` |
|       - |  957 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    2828 |  958 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  959 | `		return PH7_CORRUPT;` |
|       - |  960 | `	}` |
|       - |  961 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  962 | `	 /* Acquire VM mutex */` |
|    2828 |  963 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2828 |  964 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2826 |  965 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  966 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  967 | `	 }` |
|       - |  968 | `#endif` |
|    2828 |  969 | `	pEngine = pVm->pEngine;` |
|    2828 |  970 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - |  971 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  972 | `	 /* Leave VM mutex */` |
|    2828 |  973 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  974 | `#endif` |
|    2828 |  975 | `	if( rc == PH7_OK ){` |
|       - |  976 | `		/* Unlink from the list of active VM */` |
|       - |  977 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  978 | `			/* Acquire engine mutex */` |
|    2828 |  979 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    2828 |  980 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    2826 |  981 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  982 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  983 | `			}` |
|       - |  984 | `#endif` |
|    2828 |  985 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    2828 |  986 | `		pEngine->iVm--;` |
|       - |  987 | `		/* Release the memory chunk allocated to this VM */` |
|    2828 |  988 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - |  989 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  990 | `			/* Leave engine mutex */` |
|    2828 |  991 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  992 | `#endif` |
|    1413 |  993 | `	}` |
|    2828 |  994 | `	return rc;` |
|    1415 |  995 |  |
|       - |  996 | `/*` |
|       - |  997 | ` * [CAPIREF: ph7_create_function()]` |
|       - |  998 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  999 | ` */` |
| 1393532 | 1000 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       2 | 1001 |  |
|       - | 1002 | `	SyString sName;` |
|       - | 1003 | `	int rc;` |
|       - | 1004 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 1393534 | 1005 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1006 | `		return PH7_CORRUPT;` |
|       - | 1007 | `	}` |
| 1393534 | 1008 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1009 | `	/* Remove leading and trailing white spaces */` |
| 1393534 | 1010 | `	SyStringFullTrim(&sName);` |
|       - | 1011 | `	/* Ticket 1433-003: NULL values are not allowed */` |
| 1393534 | 1012 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1013 | `		return PH7_CORRUPT;` |
|       - | 1014 | `	}` |
|       - | 1015 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1016 | `	 /* Acquire VM mutex */` |
| 1393534 | 1017 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 1393534 | 1018 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 1393532 | 1019 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1020 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1021 | `	 }` |
|       - | 1022 | `#endif` |
|       - | 1023 | `	/* Install the foreign function */` |
| 1393534 | 1024 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1025 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1026 | `	 /* Leave VM mutex */` |
| 1393534 | 1027 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1028 | `#endif` |
| 1393534 | 1029 | `	return rc;` |
|  696768 | 1030 |  |
|       - | 1031 | `/*` |
|       - | 1032 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1033 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1034 | ` */` |
|     ! 0 | 1035 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1036 |  |
|     ! 0 | 1037 | `	ph7_user_func *pFunc = 0;` |
|       - | 1038 | `	int rc;` |
|       - | 1039 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1040 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1041 | `		return PH7_CORRUPT;` |
|       - | 1042 | `	}` |
|       - | 1043 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1044 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1045 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1046 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1047 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1048 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1049 | `	 }` |
|       - | 1050 | `#endif` |
|       - | 1051 | `	/* Perform the deletion */` |
|     ! 0 | 1052 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1053 | `	if( rc == PH7_OK ){` |
|       - | 1054 | `		/* Release internal fields */` |
|     ! 0 | 1055 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1056 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1057 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1058 | `	}` |
|       - | 1059 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1060 | `	 /* Leave VM mutex */` |
|     ! 0 | 1061 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1062 | `#endif` |
|     ! 0 | 1063 | `	return rc;` |
|     ! 0 | 1064 |  |
|       - | 1065 | `/*` |
|       - | 1066 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1067 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1068 | ` */` |
|  633022 | 1069 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       2 | 1070 |  |
|       - | 1071 | `	SyString sName;` |
|       - | 1072 | `	int rc;` |
|       - | 1073 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  633024 | 1074 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1075 | `		return PH7_CORRUPT;` |
|       - | 1076 | `	}` |
|  633024 | 1077 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1078 | `	/* Remove leading and trailing white spaces */` |
|  635850 | 1079 | `	SyStringFullTrim(&sName);` |
|  633024 | 1080 | `	if( sName.nByte < 1 ){` |
|       - | 1081 | `		/* Empty constant name */` |
|     ! 0 | 1082 | `		return PH7_CORRUPT;` |
|       - | 1083 | `	}` |
|       - | 1084 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  633024 | 1085 | `	if( xExpand == 0 ){` |
|     ! 0 | 1086 | `		return PH7_CORRUPT;` |
|       - | 1087 | `	}` |
|       - | 1088 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1089 | `	 /* Acquire VM mutex */` |
|  633024 | 1090 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  633024 | 1091 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  633022 | 1092 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1093 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1094 | `	 }` |
|       - | 1095 | `#endif` |
|       - | 1096 | `	/* Perform the registration */` |
|  633024 | 1097 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1098 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1099 | `	 /* Leave VM mutex */` |
|  633024 | 1100 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1101 | `#endif` |
|  633024 | 1102 | `	 return rc;` |
|  316513 | 1103 |  |
|       - | 1104 | `/*` |
|       - | 1105 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1106 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1107 | ` */` |
|     ! 0 | 1108 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1109 |  |
|       - | 1110 | `	ph7_constant *pCons;` |
|       - | 1111 | `	int rc;` |
|       - | 1112 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1113 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1114 | `		return PH7_CORRUPT;` |
|       - | 1115 | `	}` |
|       - | 1116 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1117 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1118 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1119 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1120 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1121 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1122 | `	 }` |
|       - | 1123 | `#endif` |
|       - | 1124 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1125 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1126 | `	 if( rc == PH7_OK ){` |
|       - | 1127 | `		 /* Perform the deletion */` |
|     ! 0 | 1128 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1129 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1130 | `	 }` |
|       - | 1131 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1132 | `	 /* Leave VM mutex */` |
|     ! 0 | 1133 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1134 | `#endif` |
|     ! 0 | 1135 | `	return rc;` |
|     ! 0 | 1136 |  |
|       - | 1137 | `/*` |
|       - | 1138 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1139 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1140 | ` */` |
|    6896 | 1141 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       2 | 1142 |  |
|       - | 1143 | `	ph7_value *pObj;` |
|       - | 1144 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    6898 | 1145 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1146 | `		return 0;` |
|       - | 1147 | `	}` |
|       - | 1148 | `	/* Allocate a new scalar variable */` |
|    6898 | 1149 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    6898 | 1150 | `	if( pObj == 0 ){` |
|     ! 0 | 1151 | `		return 0;` |
|       - | 1152 | `	}` |
|       - | 1153 | `	/* Nullify the new scalar */` |
|    6898 | 1154 | `	PH7_MemObjInit(pVm,pObj);` |
|    6898 | 1155 | `	return pObj;` |
|    3450 | 1156 |  |
|       - | 1157 | `/*` |
|       - | 1158 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1159 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1160 | ` */` |
|   38008 | 1161 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       2 | 1162 |  |
|       - | 1163 | `	ph7_hashmap *pMap;` |
|       - | 1164 | `	ph7_value *pObj;` |
|       - | 1165 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   38010 | 1166 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1167 | `		return 0;` |
|       - | 1168 | `	}` |
|       - | 1169 | `	/* Create a new hashmap first */` |
|   38010 | 1170 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   38010 | 1171 | `	if( pMap == 0 ){` |
|     ! 0 | 1172 | `		return 0;` |
|       - | 1173 | `	}` |
|       - | 1174 | `	/* Associate a new ph7_value with this hashmap */` |
|   38010 | 1175 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   38010 | 1176 | `	if( pObj == 0 ){` |
|     ! 0 | 1177 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1178 | `		return 0;` |
|       - | 1179 | `	}` |
|   38010 | 1180 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   38010 | 1181 | `	return pObj;` |
|   19006 | 1182 |  |
|       - | 1183 | `/*` |
|       - | 1184 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1185 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1186 | ` */` |
|   28320 | 1187 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1188 |  |
|       - | 1189 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   28322 | 1190 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1191 | `		return PH7_CORRUPT;` |
|       - | 1192 | `	}` |
|   28322 | 1193 | `	if( pValue ){` |
|       - | 1194 | `		/* Release the value */` |
|   28322 | 1195 | `		PH7_MemObjRelease(pValue);` |
|   28322 | 1196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|   14160 | 1197 | `	}` |
|   28322 | 1198 | `	return PH7_OK;` |
|   14162 | 1199 |  |
|       - | 1200 | `/*` |
|       - | 1201 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1202 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1203 | ` */` |
|  356528 | 1204 | `int ph7_value_to_int(ph7_value *pValue)` |
|       2 | 1205 |  |
|       - | 1206 | `	int rc;` |
|  356530 | 1207 | `	rc = PH7_MemObjToInteger(pValue);` |
|  356530 | 1208 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1209 | `		return 0;` |
|       - | 1210 | `	}` |
|  356530 | 1211 | `	return (int)pValue->x.iVal;` |
|  178266 | 1212 |  |
|       - | 1213 | `/*` |
|       - | 1214 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1215 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1216 | ` */` |
|     302 | 1217 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       2 | 1218 |  |
|       - | 1219 | `	int rc;` |
|     304 | 1220 | `	rc = PH7_MemObjToBool(pValue);` |
|     304 | 1221 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1222 | `		return 0;` |
|       - | 1223 | `	}` |
|     304 | 1224 | `	return (int)pValue->x.iVal;` |
|     153 | 1225 |  |
|       - | 1226 | `/*` |
|       - | 1227 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1228 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1229 | ` */` |
|     690 | 1230 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       2 | 1231 |  |
|       - | 1232 | `	int rc;` |
|     692 | 1233 | `	rc = PH7_MemObjToInteger(pValue);` |
|     692 | 1234 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1235 | `		return 0;` |
|       - | 1236 | `	}` |
|     692 | 1237 | `	return pValue->x.iVal;` |
|     347 | 1238 |  |
|       - | 1239 | `/*` |
|       - | 1240 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1241 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1242 | ` */` |
|     480 | 1243 | `double ph7_value_to_double(ph7_value *pValue)` |
|       1 | 1244 |  |
|       - | 1245 | `	int rc;` |
|     481 | 1246 | `	rc = PH7_MemObjToReal(pValue);` |
|     481 | 1247 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1248 | `		return (double)0;` |
|       - | 1249 | `	}` |
|     481 | 1250 | `	return (double)pValue->rVal;` |
|     241 | 1251 |  |
|       - | 1252 | `/*` |
|       - | 1253 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1254 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1255 | ` */` |
|  668564 | 1256 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       2 | 1257 |  |
|  668566 | 1258 | `	PH7_MemObjToString(pValue);` |
|  668566 | 1259 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  639048 | 1260 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  639048 | 1261 | `		if( pLen ){` |
|  585066 | 1262 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  292554 | 1263 | `		}` |
|  639048 | 1264 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1265 | `	}else{` |
|       - | 1266 | `		/* Return the empty string */` |
|   29520 | 1267 | `		if( pLen ){` |
|   29510 | 1268 | `			*pLen = 0;` |
|   14754 | 1269 | `		}` |
|   29520 | 1270 | `		return "";` |
|       - | 1271 | `	}` |
|  334306 | 1272 |  |
|       - | 1273 | `/*` |
|       - | 1274 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1275 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1276 | ` */` |
|   25468 | 1277 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       2 | 1278 |  |
|   25470 | 1279 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1280 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1281 | `		return 0;` |
|       - | 1282 | `	}` |
|   25470 | 1283 | `	return pValue->x.pOther;` |
|   12736 | 1284 |  |
|       - | 1285 | `/*` |
|       - | 1286 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1287 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1288 | ` */` |
|      30 | 1289 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1290 |  |
|       - | 1291 | `	int rc;` |
|      31 | 1292 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1293 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1294 | `		return 1;` |
|       - | 1295 | `	}` |
|       - | 1296 | `	/* Perform the comparison */` |
|      31 | 1297 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1298 | `	/* Comparison result */` |
|      31 | 1299 | `	return rc;` |
|      16 | 1300 |  |
|       - | 1301 | `/*` |
|       - | 1302 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1303 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1304 | ` */` |
|    9950 | 1305 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       2 | 1306 |  |
|    9952 | 1307 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       2 | 1308 |  |
|       - | 1309 | `/*` |
|       - | 1310 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1311 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1312 | ` */` |
|   14390 | 1313 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       2 | 1314 |  |
|   14392 | 1315 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       2 | 1316 |  |
|       - | 1317 | `/*` |
|       - | 1318 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1319 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1320 | ` */` |
|  314972 | 1321 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       2 | 1322 |  |
|  314974 | 1323 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       2 | 1324 |  |
|       - | 1325 | `/*` |
|       - | 1326 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1327 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1328 | ` */` |
|     416 | 1329 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       1 | 1330 |  |
|     417 | 1331 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       1 | 1332 |  |
|       - | 1333 | `/*` |
|       - | 1334 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1335 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1336 | ` */` |
|     122 | 1337 | `int ph7_result_null(ph7_context *pCtx)` |
|       2 | 1338 |  |
|       - | 1339 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     124 | 1340 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     124 | 1341 | `	return PH7_OK;` |
|       2 | 1342 |  |
|       - | 1343 | `/*` |
|       - | 1344 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1345 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1346 | ` */` |
|  854110 | 1347 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       2 | 1348 |  |
|  854112 | 1349 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       2 | 1350 |  |
|       - | 1351 | `/*` |
|       - | 1352 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1353 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1354 | ` */` |
|     280 | 1355 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1356 |  |
|       - | 1357 | `	ph7_value *p;` |
|       - | 1358 | `	va_list ap;` |
|       - | 1359 | `	int rc;` |
|     281 | 1360 | `	p = pCtx->pRet;` |
|     281 | 1361 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1362 | `		/* Invalidate any prior representation */` |
|     141 | 1363 | `		PH7_MemObjRelease(p);` |
|     141 | 1364 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|      70 | 1365 | `	}` |
|       - | 1366 | `	/* Format the given string */` |
|     281 | 1367 | `	va_start(ap,zFormat);` |
|     281 | 1368 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|     281 | 1369 | `	va_end(ap);` |
|     281 | 1370 | `	return rc;` |
|       1 | 1371 |  |
|       - | 1372 | `/*` |
|       - | 1373 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1374 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1375 | ` */` |
|   29284 | 1376 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       2 | 1377 |  |
|   29286 | 1378 | `	int rc = PH7_OK;` |
|   29286 | 1379 | `	if( pValue == 0 ){` |
|     ! 0 | 1380 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1381 | `	}else{` |
|   29286 | 1382 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1383 | `	}` |
|   29286 | 1384 | `	return rc;` |
|       2 | 1385 |  |
|       - | 1386 | `/*` |
|       - | 1387 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1388 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1389 | ` */` |
|    4328 | 1390 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       2 | 1391 |  |
|    4330 | 1392 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       2 | 1393 |  |
|       - | 1394 | `/*` |
|       - | 1395 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1396 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1397 | ` */` |
|    6896 | 1398 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       2 | 1399 |  |
|       - | 1400 | `	ph7_value *pVal;` |
|    6898 | 1401 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|    6898 | 1402 | `	if( pVal ){` |
|       - | 1403 | `		/* Record value address so it can be freed automatically` |
|       - | 1404 | `		 * when the calling function returns.` |
|       - | 1405 | `		 */` |
|    6898 | 1406 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    3448 | 1407 | `	}` |
|    6898 | 1408 | `	return pVal;` |
|       2 | 1409 |  |
|       - | 1410 | `/*` |
|       - | 1411 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1412 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1413 | ` */` |
|    9688 | 1414 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       2 | 1415 |  |
|       - | 1416 | `	ph7_value *pVal;` |
|    9690 | 1417 | `	pVal = ph7_new_array(pCtx->pVm);` |
|    9690 | 1418 | `	if( pVal ){` |
|       - | 1419 | `		/* Record value address so it can be freed automatically` |
|       - | 1420 | `		 * when the calling function returns.` |
|       - | 1421 | `		 */` |
|    9690 | 1422 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    4844 | 1423 | `	}` |
|    9690 | 1424 | `	return pVal;` |
|       2 | 1425 |  |
|       - | 1426 | `/*` |
|       - | 1427 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1428 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1429 | ` */` |
|     382 | 1430 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       2 | 1431 |  |
|     384 | 1432 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     384 | 1433 |  |
|       - | 1434 | `/*` |
|       - | 1435 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1436 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1437 | ` */` |
|    4298 | 1438 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       2 | 1439 |  |
|       - | 1440 | `	void *pChunk;` |
|    4300 | 1441 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    4300 | 1442 | `	if( pChunk ){` |
|    4300 | 1443 | `		if( ZeroChunk ){` |
|       - | 1444 | `			/* Zero the memory chunk */` |
|    4266 | 1445 | `			SyZero(pChunk,nByte);` |
|    2132 | 1446 | `		}` |
|    4300 | 1447 | `		if( AutoRelease ){` |
|       - | 1448 | `			ph7_aux_data sAux;` |
|       - | 1449 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1450 | `			 * upon this context is destroyed.` |
|       - | 1451 | `			 */` |
|      25 | 1452 | `			sAux.pAuxData = pChunk;` |
|      25 | 1453 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|      12 | 1454 | `		}` |
|    2149 | 1455 | `	}` |
|    4300 | 1456 | `	return pChunk;` |
|       2 | 1457 |  |
|       - | 1458 | `/*` |
|       - | 1459 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1460 | ` * chunk container.` |
|       - | 1461 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1462 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1463 | ` */` |
|    4248 | 1464 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       2 | 1465 |  |
|       - | 1466 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1467 | `	sxu32 n;` |
|    4250 | 1468 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1469 | `		/* Don't bother processing,the container is empty */` |
|    4250 | 1470 | `		return 0;` |
|       - | 1471 | `	}` |
|       - | 1472 | `	/* Perform the lookup */` |
|     ! 0 | 1473 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1474 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1475 | `		pAux = &aAux[n];` |
|     ! 0 | 1476 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1477 | `			/* Chunk found */` |
|     ! 0 | 1478 | `			return pAux;` |
|       - | 1479 | `		}` |
|     ! 0 | 1480 | `	}` |
|       - | 1481 | `	/* No such allocated chunk */` |
|     ! 0 | 1482 | `	return 0;` |
|    2126 | 1483 |  |
|       - | 1484 | `/*` |
|       - | 1485 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1486 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1487 | ` */` |
|     ! 0 | 1488 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1489 |  |
|       - | 1490 | `	ph7_aux_data *pAux;` |
|       - | 1491 | `	void *pNew;` |
|     ! 0 | 1492 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1493 | `	if( pNew ){` |
|     ! 0 | 1494 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1495 | `		if( pAux ){` |
|     ! 0 | 1496 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1497 | `		}` |
|     ! 0 | 1498 | `	}` |
|     ! 0 | 1499 | `	return pNew;` |
|     ! 0 | 1500 |  |
|       - | 1501 | `/*` |
|       - | 1502 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1503 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1504 | ` */` |
|    4248 | 1505 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       2 | 1506 |  |
|       - | 1507 | `	ph7_aux_data *pAux;` |
|    4250 | 1508 | `	if( pChunk == 0 ){` |
|       - | 1509 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1510 | `		return;` |
|       - | 1511 | `	}` |
|    4250 | 1512 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    4250 | 1513 | `	if( pAux ){` |
|       - | 1514 | `		/* Mark as destroyed */` |
|     ! 0 | 1515 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1516 | `	}` |
|    4250 | 1517 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    2126 | 1518 |  |
|       - | 1519 | `/*` |
|       - | 1520 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1521 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1522 | ` */` |
|     ! 0 | 1523 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|     ! 0 | 1524 |  |
|       - | 1525 | `	ph7_hashmap_node *pNode;` |
|       - | 1526 | `	ph7_value *pValue;` |
|       - | 1527 | `	ph7_value skey;` |
|       - | 1528 | `	int rc;` |
|       - | 1529 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 1530 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1531 | `		return 0;` |
|       - | 1532 | `	}` |
|     ! 0 | 1533 | `	if( nByte < 0 ){` |
|     ! 0 | 1534 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1535 | `	}` |
|       - | 1536 | `	/* Convert the key to a ph7_value  */` |
|     ! 0 | 1537 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     ! 0 | 1538 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1539 | `	/* Perform the lookup */` |
|     ! 0 | 1540 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     ! 0 | 1541 | `	PH7_MemObjRelease(&skey);` |
|     ! 0 | 1542 | `	if( rc != PH7_OK ){` |
|       - | 1543 | `		/* No such entry */` |
|     ! 0 | 1544 | `		return 0;` |
|       - | 1545 | `	}` |
|       - | 1546 | `	/* Extract the target value */` |
|     ! 0 | 1547 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     ! 0 | 1548 | `	return pValue;` |
|     ! 0 | 1549 |  |
|       - | 1550 | `/*` |
|       - | 1551 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1552 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1553 | ` */` |
|   29854 | 1554 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       2 | 1555 |  |
|       - | 1556 | `	int rc;` |
|   29856 | 1557 | `	if( xWalk == 0 ){` |
|     ! 0 | 1558 | `		return PH7_CORRUPT;` |
|       - | 1559 | `	}` |
|       - | 1560 | `	/* Make sure we are dealing with a valid hashmap */` |
|   29856 | 1561 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1562 | `		return PH7_CORRUPT;` |
|       - | 1563 | `	}` |
|       - | 1564 | `	/* Start the walk process */` |
|   29856 | 1565 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   29856 | 1566 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   14929 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1570 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1571 | ` */` |
| 2277452 | 1572 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       2 | 1573 |  |
|       - | 1574 | `	int rc;` |
|       - | 1575 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2277454 | 1576 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1577 | `		return PH7_CORRUPT;` |
|       - | 1578 | `	}` |
|       - | 1579 | `	/* Perform the insertion */` |
| 2277454 | 1580 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
| 2277454 | 1581 | `	return rc;` |
| 1138728 | 1582 |  |
|       - | 1583 | `/*` |
|       - | 1584 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1585 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1586 | ` */` |
|    5058 | 1587 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       2 | 1588 |  |
|       - | 1589 | `	int rc;` |
|       - | 1590 | `	/* Make sure we are dealing with a valid hashmap */` |
|    5060 | 1591 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1592 | `		return PH7_CORRUPT;` |
|       - | 1593 | `	}` |
|       - | 1594 | `	/* Perform the insertion */` |
|    5060 | 1595 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1596 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1597 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1598 | `	}else{` |
|       - | 1599 | `		ph7_value sKey;` |
|    5060 | 1600 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|    5060 | 1601 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|    5060 | 1602 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|    5060 | 1603 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1604 | `	}` |
|    5060 | 1605 | `	return rc;` |
|    2531 | 1606 |  |
|       - | 1607 | `/*` |
|       - | 1608 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1609 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1610 | ` */` |
|     314 | 1611 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       2 | 1612 |  |
|       - | 1613 | `	ph7_value sKey;` |
|       - | 1614 | `	int rc;` |
|       - | 1615 | `	/* Make sure we are dealing with a valid hashmap */` |
|     316 | 1616 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1617 | `		return PH7_CORRUPT;` |
|       - | 1618 | `	}` |
|     316 | 1619 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1620 | `	/* Perform the insertion */` |
|     316 | 1621 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|     316 | 1622 | `	PH7_MemObjRelease(&sKey);` |
|     316 | 1623 | `	return rc;` |
|     159 | 1624 |  |
|       - | 1625 | `/*` |
|       - | 1626 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1627 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1628 | ` */` |
|  122470 | 1629 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       2 | 1630 |  |
|       - | 1631 | `	ph7_hashmap *pMap;` |
|       - | 1632 | `	/* Make sure we are dealing with a valid hashmap */` |
|  122472 | 1633 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1634 | `		return 0;` |
|       - | 1635 | `	}` |
|       - | 1636 | `	/* Point to the internal representation of the hashmap */` |
|  122472 | 1637 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  122472 | 1638 | `	return pMap->nEntry;` |
|   61237 | 1639 |  |
|       - | 1640 | `/*` |
|       - | 1641 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1642 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1643 | ` */` |
|       2 | 1644 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|       1 | 1645 |  |
|       - | 1646 | `	int rc;` |
|       3 | 1647 | `	if( xWalk == 0 ){` |
|     ! 0 | 1648 | `		return PH7_CORRUPT;` |
|       - | 1649 | `	}` |
|       - | 1650 | `	/* Make sure we are dealing with a valid class instance */` |
|       3 | 1651 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1652 | `		return PH7_CORRUPT;` |
|       - | 1653 | `	}` |
|       - | 1654 | `	/* Start the walk process */` |
|       3 | 1655 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|       3 | 1656 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|       2 | 1657 |  |
|       - | 1658 | `/*` |
|       - | 1659 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1660 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1661 | ` */` |
|       8 | 1662 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|       1 | 1663 |  |
|       - | 1664 | `	ph7_value *pValue;` |
|       - | 1665 | `	SyString sAttr;` |
|       - | 1666 | `	/* Make sure we are dealing with a valid class instance */` |
|       9 | 1667 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1668 | `		return 0;` |
|       - | 1669 | `	}` |
|       9 | 1670 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1671 | `	/* Extract the attribute value if available.` |
|       - | 1672 | `	 */` |
|       9 | 1673 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|       9 | 1674 | `	return pValue;` |
|       5 | 1675 |  |
|       - | 1676 | `/*` |
|       - | 1677 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1678 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1679 | ` */` |
|     ! 0 | 1680 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1681 |  |
|       - | 1682 | `	ph7_class *pClass;` |
|     ! 0 | 1683 | `	if( pLength ){` |
|     ! 0 | 1684 | `		*pLength = 0;` |
|     ! 0 | 1685 | `	}` |
|       - | 1686 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1687 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1688 | `		return 0;` |
|       - | 1689 | `	}` |
|       - | 1690 | `	/* Point to the class */` |
|     ! 0 | 1691 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1692 | `	/* Return the class name */` |
|     ! 0 | 1693 | `	if( pLength ){` |
|     ! 0 | 1694 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1695 | `	}` |
|     ! 0 | 1696 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1697 |  |
|       - | 1698 | `/*` |
|       - | 1699 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1700 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1701 | ` */` |
|     370 | 1702 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       2 | 1703 |  |
|       - | 1704 | `	SyString sData;` |
|       - | 1705 | `	int rc;` |
|     372 | 1706 | `	if( nLen < 0 ){` |
|     ! 0 | 1707 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1708 | `	}` |
|     372 | 1709 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|     372 | 1710 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|     372 | 1711 | `	return rc;` |
|       2 | 1712 |  |
|       - | 1713 | `/*` |
|       - | 1714 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1715 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1716 | ` */` |
|       2 | 1717 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1718 |  |
|       - | 1719 | `	va_list ap;` |
|       - | 1720 | `	int rc;` |
|       3 | 1721 | `	va_start(ap,zFormat);` |
|       3 | 1722 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1723 | `	va_end(ap);` |
|       3 | 1724 | `	return rc;` |
|       1 | 1725 |  |
|       - | 1726 | `/*` |
|       - | 1727 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1728 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1729 | ` */` |
|      24 | 1730 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       2 | 1731 |  |
|      26 | 1732 | `	int rc = PH7_OK;` |
|      26 | 1733 | `	if( zErr ){` |
|      26 | 1734 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      12 | 1735 | `	}` |
|      26 | 1736 | `	return rc;` |
|       2 | 1737 |  |
|       - | 1738 | `/*` |
|       - | 1739 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1740 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1741 | ` */` |
|      24 | 1742 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       2 | 1743 |  |
|       - | 1744 | `	va_list ap;` |
|       - | 1745 | `	int rc;` |
|      26 | 1746 | `	if( zFormat == 0){` |
|     ! 0 | 1747 | `		return PH7_OK;` |
|       - | 1748 | `	}` |
|      26 | 1749 | `	va_start(ap,zFormat);` |
|      26 | 1750 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      26 | 1751 | `	va_end(ap);` |
|      26 | 1752 | `	return rc;` |
|      14 | 1753 |  |
|       - | 1754 | `/*` |
|       - | 1755 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1756 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1757 | ` */` |
|      34 | 1758 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1759 |  |
|       - | 1760 | `	sxu32 n;` |
|      35 | 1761 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1762 | `	return n;` |
|       1 | 1763 |  |
|       - | 1764 | `/*` |
|       - | 1765 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1766 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1767 | ` */` |
|     ! 0 | 1768 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1769 |  |
|     ! 0 | 1770 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1771 | `		return PH7_CORRUPT;` |
|       - | 1772 | `	}` |
|     ! 0 | 1773 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1774 | `	return PH7_OK;` |
|     ! 0 | 1775 |  |
|       - | 1776 | `/*` |
|       - | 1777 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1778 | ` *` |
|       - | 1779 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1780 | ` * {` |
|       - | 1781 | ` *	return pCtx->pVm;` |
|       - | 1782 | ` * }` |
|       - | 1783 | ` */` |
|       - | 1784 | `/*` |
|       - | 1785 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1786 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1787 | ` */` |
|   54220 | 1788 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       2 | 1789 |  |
|   54222 | 1790 | `	return pCtx->pFunc->pUserData;` |
|       2 | 1791 |  |
|       - | 1792 | `/*` |
|       - | 1793 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1794 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1795 | ` */` |
|       2 | 1796 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1797 |  |
|       - | 1798 | `	ph7_aux_data sAux;` |
|       - | 1799 | `	int rc;` |
|       3 | 1800 | `	sAux.pAuxData = pUserData;` |
|       3 | 1801 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1802 | `	return rc;` |
|       1 | 1803 |  |
|       - | 1804 | `/*` |
|       - | 1805 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1806 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1807 | ` */` |
|       6 | 1808 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1809 |  |
|       - | 1810 | `	ph7_aux_data *pAux;` |
|       7 | 1811 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       7 | 1812 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1813 |  |
|       - | 1814 | `/*` |
|       - | 1815 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1816 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1817 | ` */` |
|       2 | 1818 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|       1 | 1819 |  |
|       - | 1820 | `	ph7_aux_data *pAux;` |
|       3 | 1821 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|       3 | 1822 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1823 |  |
|       - | 1824 | `/*` |
|       - | 1825 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1826 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1827 | ` */` |
|    5832 | 1828 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       2 | 1829 |  |
|    5834 | 1830 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       2 | 1831 |  |
|       - | 1832 | `/*` |
|       - | 1833 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1834 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1835 | ` */` |
|   22182 | 1836 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       2 | 1837 |  |
|       - | 1838 | `	SyString *pName;` |
|   22184 | 1839 | `	pName = &pCtx->pFunc->sName;` |
|   22184 | 1840 | `	return pName->zString;` |
|       2 | 1841 |  |
|       - | 1842 | `/*` |
|       - | 1843 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1844 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1845 | ` */` |
|   25458 | 1846 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       2 | 1847 |  |
|       - | 1848 | `	/* Invalidate any prior representation */` |
|   25460 | 1849 | `	PH7_MemObjRelease(pVal);` |
|   25460 | 1850 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|   25460 | 1851 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   25460 | 1852 | `	return PH7_OK;` |
|       2 | 1853 |  |
|       - | 1854 | `/*` |
|       - | 1855 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1856 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1857 | ` */` |
|   14476 | 1858 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       2 | 1859 |  |
|       - | 1860 | `	/* Invalidate any prior representation */` |
|   14478 | 1861 | `	PH7_MemObjRelease(pVal);` |
|   14478 | 1862 | `	pVal->x.iVal = iValue;` |
|   14478 | 1863 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   14478 | 1864 | `	return PH7_OK;` |
|       2 | 1865 |  |
|       - | 1866 | `/*` |
|       - | 1867 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1868 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1869 | ` */` |
|  314972 | 1870 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       2 | 1871 |  |
|       - | 1872 | `	/* Invalidate any prior representation */` |
|  314974 | 1873 | `	PH7_MemObjRelease(pVal);` |
|  314974 | 1874 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  314974 | 1875 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  314974 | 1876 | `	return PH7_OK;` |
|       2 | 1877 |  |
|       - | 1878 | `/*` |
|       - | 1879 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1880 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1881 | ` */` |
|       4 | 1882 | `int ph7_value_null(ph7_value *pVal)` |
|       1 | 1883 |  |
|       - | 1884 | `	/* Invalidate any prior representation and set the NULL flag */` |
|       5 | 1885 | `	PH7_MemObjRelease(pVal);` |
|       5 | 1886 | `	return PH7_OK;` |
|       1 | 1887 |  |
|       - | 1888 | `/*` |
|       - | 1889 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1890 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1891 | ` */` |
|     520 | 1892 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       1 | 1893 |  |
|       - | 1894 | `	/* Invalidate any prior representation */` |
|     521 | 1895 | `	PH7_MemObjRelease(pVal);` |
|     521 | 1896 | `	pVal->rVal = (ph7_real)Value;` |
|     521 | 1897 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1898 | `	/* Try to get an integer representation also */` |
|     521 | 1899 | `	PH7_MemObjTryInteger(pVal);` |
|     521 | 1900 | `	return PH7_OK;` |
|       1 | 1901 |  |
|       - | 1902 | `/*` |
|       - | 1903 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1904 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1905 | ` */` |
|  994836 | 1906 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       2 | 1907 |  |
|  994838 | 1908 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1909 | `		/* Invalidate any prior representation */` |
|  339760 | 1910 | `		PH7_MemObjRelease(pVal);` |
|  339760 | 1911 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  169879 | 1912 | `	}` |
|  994838 | 1913 | `	if( zString ){` |
|  993608 | 1914 | `		if( nLen < 0 ){` |
|       - | 1915 | `			/* Compute length automatically */` |
|    3672 | 1916 | `			nLen = (int)SyStrlen(zString);` |
|    1835 | 1917 | `		}` |
|       - | 1918 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|       - | 1919 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
|  993608 | 1920 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|       - | 1921 | `	}` |
|    1231 | 1922 | `	return PH7_OK;` |
|  497420 | 1923 |  |
|       - | 1924 | `/*` |
|       - | 1925 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1926 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1927 | ` */` |
|      22 | 1928 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1929 |  |
|       - | 1930 | `	va_list ap;` |
|       - | 1931 | `	int rc;` |
|      23 | 1932 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1933 | `		/* Invalidate any prior representation */` |
|      19 | 1934 | `		PH7_MemObjRelease(pVal);` |
|      19 | 1935 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       9 | 1936 | `	}` |
|      23 | 1937 | `	va_start(ap,zFormat);` |
|      23 | 1938 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      23 | 1939 | `	va_end(ap);` |
|       - | 1940 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|      23 | 1941 | `	return rc;` |
|       1 | 1942 |  |
|       - | 1943 | `/*` |
|       - | 1944 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1945 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1946 | ` */` |
|  129776 | 1947 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       2 | 1948 |  |
|       - | 1949 | `	/* Reset the string cursor */` |
|  129778 | 1950 | `	SyBlobReset(&pVal->sBlob);` |
|  129778 | 1951 | `	return PH7_OK;` |
|       2 | 1952 |  |
|       - | 1953 | `/*` |
|       - | 1954 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1955 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1956 | ` */` |
|    4418 | 1957 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       2 | 1958 |  |
|       - | 1959 | `	/* Invalidate any prior representation */` |
|    4420 | 1960 | `	PH7_MemObjRelease(pVal);` |
|       - | 1961 | `	/* Reflect the new type */` |
|    4420 | 1962 | `	pVal->x.pOther = pUserData;` |
|    4420 | 1963 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    4420 | 1964 | `	return PH7_OK;` |
|       2 | 1965 |  |
|       - | 1966 | `/*` |
|       - | 1967 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 1968 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1969 | ` */` |
|    3268 | 1970 | `int ph7_value_release(ph7_value *pVal)` |
|       2 | 1971 |  |
|    3270 | 1972 | `	PH7_MemObjRelease(pVal);` |
|    3270 | 1973 | `	return PH7_OK;` |
|       2 | 1974 |  |
|       - | 1975 | `/*` |
|       - | 1976 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 1977 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1978 | ` */` |
|   12006 | 1979 | `int ph7_value_is_int(ph7_value *pVal)` |
|       2 | 1980 |  |
|       - | 1981 | `	/* TRUE whenever an integer representation is available, including an` |
|       - | 1982 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|       - | 1983 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|       - | 1984 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|       - | 1985 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|   12008 | 1986 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       2 | 1987 |  |
|       - | 1988 | `/*` |
|       - | 1989 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 1990 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1991 | ` */` |
|    1234 | 1992 | `int ph7_value_is_float(ph7_value *pVal)` |
|       2 | 1993 |  |
|    1236 | 1994 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       2 | 1995 |  |
|       - | 1996 | `/*` |
|       - | 1997 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 1998 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1999 | ` */` |
|     486 | 2000 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       2 | 2001 |  |
|     488 | 2002 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       2 | 2003 |  |
|       - | 2004 | `/*` |
|       - | 2005 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 2006 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2007 | ` */` |
|   93492 | 2008 | `int ph7_value_is_string(ph7_value *pVal)` |
|       2 | 2009 |  |
|   93494 | 2010 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       2 | 2011 |  |
|       - | 2012 | `/*` |
|       - | 2013 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 2014 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2015 | ` */` |
|    1708 | 2016 | `int ph7_value_is_null(ph7_value *pVal)` |
|       2 | 2017 |  |
|    1710 | 2018 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       2 | 2019 |  |
|       - | 2020 | `/*` |
|       - | 2021 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2022 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2023 | ` */` |
|     368 | 2024 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       2 | 2025 |  |
|       - | 2026 | `	int rc;` |
|     370 | 2027 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|     370 | 2028 | `	return rc;` |
|       2 | 2029 |  |
|       - | 2030 | `/*` |
|       - | 2031 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2032 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2033 | ` */` |
|   23634 | 2034 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       2 | 2035 |  |
|       - | 2036 | `	int rc;` |
|   23636 | 2037 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   23636 | 2038 | `	return rc;` |
|       2 | 2039 |  |
|       - | 2040 | `/*` |
|       - | 2041 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2042 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2043 | ` */` |
|      12 | 2044 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2045 |  |
|      13 | 2046 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2047 |  |
|       - | 2048 | `/*` |
|       - | 2049 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2050 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2051 | ` */` |
|  140672 | 2052 | `int ph7_value_is_array(ph7_value *pVal)` |
|       2 | 2053 |  |
|  140674 | 2054 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       2 | 2055 |  |
|       - | 2056 | `/*` |
|       - | 2057 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2058 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2059 | ` */` |
|    2228 | 2060 | `int ph7_value_is_object(ph7_value *pVal)` |
|       2 | 2061 |  |
|    2230 | 2062 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       2 | 2063 |  |
|       - | 2064 | `/*` |
|       - | 2065 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2066 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2067 | ` */` |
|   27288 | 2068 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       2 | 2069 |  |
|   27290 | 2070 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       2 | 2071 |  |
|       - | 2072 | `/*` |
|       - | 2073 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2074 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2075 | ` */` |
|   25456 | 2076 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       2 | 2077 |  |
|       - | 2078 | `	int rc;` |
|   25458 | 2079 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   25458 | 2080 | `	return rc;` |
|       2 | 2081 |  |
|       - | 2082 | `/*` |
|       - | 2083 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|       - | 2084 | ` * Check if a value holds a Fiber instance.` |
|       - | 2085 | ` */` |
|     ! 0 | 2086 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|     ! 0 | 2087 |  |
|     ! 0 | 2088 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|     ! 0 | 2089 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|     ! 0 | 2090 |  |
|       - | 2091 | `/*` |
|       - | 2092 | ` * [CAPIREF: ph7_fiber_start()]` |
|       - | 2093 | ` * Start a Fiber, passing arguments to the callable.` |
|       - | 2094 | ` */` |
|     ! 0 | 2095 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2096 |  |
|     ! 0 | 2097 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2098 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|     ! 0 | 2099 |  |
|       - | 2100 | `/*` |
|       - | 2101 | ` * [CAPIREF: ph7_fiber_resume()]` |
|       - | 2102 | ` * Resume a suspended Fiber, optionally sending a value.` |
|       - | 2103 | ` */` |
|     ! 0 | 2104 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2105 |  |
|     ! 0 | 2106 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|     ! 0 | 2107 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|     ! 0 | 2108 |  |
|       - | 2109 | `/*` |
|       - | 2110 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|       - | 2111 | ` * Check if a Fiber is currently suspended.` |
|       - | 2112 | ` */` |
|     ! 0 | 2113 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|     ! 0 | 2114 |  |
|     ! 0 | 2115 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2116 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|     ! 0 | 2117 |  |
|       - | 2118 | `/*` |
|       - | 2119 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|       - | 2120 | ` * Check if a Fiber has completed execution.` |
|       - | 2121 | ` */` |
|     ! 0 | 2122 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|     ! 0 | 2123 |  |
|     ! 0 | 2124 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2125 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|     ! 0 | 2126 |  |
|       - | 2127 | `/*` |
|       - | 2128 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|       - | 2129 | ` * Get the return value of a terminated Fiber.` |
|       - | 2130 | ` * Returns NULL if the Fiber has not terminated.` |
|       - | 2131 | ` */` |
|     ! 0 | 2132 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|     ! 0 | 2133 |  |
|     ! 0 | 2134 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|     ! 0 | 2135 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|     ! 0 | 2136 |  |
|       - | 2137 |  |
