# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 742/1046 lines (70.94%)

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
|       - |   45 | `	{0,0,0,0,0,0,0,0,{0}},` |
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
|    3960 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|       2 |   79 |  |
|    3962 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    3962 |   81 | `	int rc = PH7_OK;` |
|       - |   82 | `	/* Perform the requested operation */` |
|    3962 |   83 | `	switch(nOp){` |
|    1980 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    3962 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    3962 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|       - |   87 | `		/* Compile time error consumer routine */` |
|    3962 |   88 | `		if( xConsumer == 0 ){` |
|     ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|     ! 0 |   90 | `			break;` |
|       - |   91 | `		}` |
|       - |   92 | `		/* Install the error consumer */` |
|    3962 |   93 | `		pConf->xErr     = xConsumer;` |
|    3962 |   94 | `		pConf->pErrData = pUserData;` |
|    3962 |   95 | `		break;` |
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
|     ! 0 |  121 | `	default:` |
|       - |  122 | `		/* Unknown configuration verb */` |
|     ! 0 |  123 | `		rc = PH7_CORRUPT;` |
|     ! 0 |  124 | `		break;` |
|       - |  125 | `	} /* Switch() */` |
|    3962 |  126 | `	return rc;` |
|       2 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * Configure the PH7 library.` |
|       - |  130 | ` * return PH7_OK on success.Any other return value` |
|       - |  131 | ` * indicates failure.` |
|       - |  132 | ` * Refer to [ph7_lib_config()].` |
|       - |  133 | ` */` |
|    5940 |  134 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|       2 |  135 |  |
|    5942 |  136 | `	int rc = PH7_OK;` |
|    5942 |  137 | `	switch(nOp){` |
|     990 |  138 | `	    case PH7_LIB_CONFIG_VFS:{` |
|       - |  139 | `			/* Install a virtual file system */` |
|    1982 |  140 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|    1982 |  141 | `			sMPGlobal.pVfs = pVfs;` |
|    1982 |  142 | `			break;` |
|       - |  143 | `								}` |
|     990 |  144 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|       - |  145 | `			/* Use an alternative low-level memory allocation routines */` |
|    1982 |  146 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|       - |  147 | `			/* Save the memory failure callback (if available) */` |
|    1982 |  148 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|    1982 |  149 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|    1982 |  150 | `			if( pMethods == 0 ){` |
|       - |  151 | `				/* Use the built-in memory allocation subsystem */` |
|    1982 |  152 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|     992 |  153 | `			}else{` |
|     ! 0 |  154 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|       - |  155 | `			}` |
|    1982 |  156 | `			break;` |
|       - |  157 | `										  }` |
|     ! 0 |  158 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|       - |  159 | `			/* Memory failure callback */` |
|     ! 0 |  160 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|     ! 0 |  161 | `			void *pUserData = va_arg(ap,void *);` |
|     ! 0 |  162 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|     ! 0 |  163 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|     ! 0 |  164 | `			break;` |
|       - |  165 | `												 }` |
|     990 |  166 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|       - |  167 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  168 | `			/* Use an alternative low-level mutex subsystem */` |
|    1982 |  169 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|       - |  170 | `#if defined (UNTRUST)` |
|       - |  171 | `			if( pMethods == 0 ){` |
|       - |  172 | `				rc = PH7_CORRUPT;` |
|       - |  173 | `			}` |
|       - |  174 | `#endif` |
|       - |  175 | `			/* Sanity check */` |
|    1982 |  176 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|       - |  177 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|     ! 0 |  178 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  179 | `				break;` |
|       - |  180 | `			}` |
|    1982 |  181 | `			if( sMPGlobal.pMutexMethods ){` |
|       - |  182 | `				/* Overwrite the previous mutex subsystem */` |
|     ! 0 |  183 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     ! 0 |  184 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|     ! 0 |  185 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  186 | `				}` |
|     ! 0 |  187 | `				sMPGlobal.pMutex = 0;` |
|     ! 0 |  188 | `			}` |
|       - |  189 | `			/* Initialize and install the new mutex subsystem */` |
|    1982 |  190 | `			if( pMethods->xGlobalInit ){` |
|       2 |  191 | `				rc = pMethods->xGlobalInit();` |
|       2 |  192 | `				if ( rc != PH7_OK ){` |
|     ! 0 |  193 | `					break;` |
|       - |  194 | `				}` |
|     ! 0 |  195 | `			}` |
|       - |  196 | `			/* Create the global mutex */` |
|    1982 |  197 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|    1982 |  198 | `			if( sMPGlobal.pMutex == 0 ){` |
|       - |  199 | `				/*` |
|       - |  200 | `				 * If the supplied mutex subsystem is so sick that we are unable to` |
|       - |  201 | `				 * create a single mutex,there is no much we can do here.` |
|       - |  202 | `				 */` |
|     ! 0 |  203 | `				if( pMethods->xGlobalRelease ){` |
|     ! 0 |  204 | `					pMethods->xGlobalRelease();` |
|     ! 0 |  205 | `				}` |
|     ! 0 |  206 | `				rc = PH7_CORRUPT;` |
|     ! 0 |  207 | `				break;` |
|       - |  208 | `			}` |
|    1982 |  209 | `			sMPGlobal.pMutexMethods = pMethods;` |
|    1982 |  210 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|       - |  211 | `				/* Set a default threading level */` |
|    1982 |  212 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|     990 |  213 | `			}` |
|       - |  214 | `#endif` |
|    1982 |  215 | `			break;` |
|       - |  216 | `										   }` |
|     ! 0 |  217 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:` |
|       - |  218 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  219 | `			/* Single thread mode(Only one thread is allowed to play with the library) */` |
|     ! 0 |  220 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|       - |  221 | `#endif` |
|     ! 0 |  222 | `			break;` |
|     ! 0 |  223 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:` |
|       - |  224 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  225 | `			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines` |
|       - |  226 | `			 * may be shared between multiple threads).` |
|       - |  227 | `			 */` |
|     ! 0 |  228 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|       - |  229 | `#endif` |
|     ! 0 |  230 | `			break;` |
|     ! 0 |  231 | `		default:` |
|       - |  232 | `			/* Unknown configuration option */` |
|     ! 0 |  233 | `			rc = PH7_CORRUPT;` |
|     ! 0 |  234 | `			break;` |
|       - |  235 | `	}` |
|    5942 |  236 | `	return rc;` |
|       2 |  237 |  |
|       - |  238 | `/*` |
|       - |  239 | ` * [CAPIREF: ph7_lib_config()]` |
|       - |  240 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  241 | ` */` |
|    5940 |  242 | `int ph7_lib_config(int nConfigOp,...)` |
|       2 |  243 |  |
|       - |  244 | `	va_list ap;` |
|       - |  245 | `	int rc;` |
|       - |  246 |  |
|    5942 |  247 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|       - |  248 | `		/* Library is already initialized,this operation is forbidden */` |
|     ! 0 |  249 | `		return PH7_LOOKED;` |
|       - |  250 | `	}` |
|    5942 |  251 | `	va_start(ap,nConfigOp);` |
|    5942 |  252 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    5942 |  253 | `	va_end(ap);` |
|    5942 |  254 | `	return rc;` |
|    2972 |  255 |  |
|       - |  256 | `/*` |
|       - |  257 | ` * Global library initialization` |
|       - |  258 | ` * Refer to [ph7_lib_init()]` |
|       - |  259 | ` * This routine must be called to initialize the memory allocation subsystem,the mutex` |
|       - |  260 | ` * subsystem prior to doing any serious work with the library.The first thread to call` |
|       - |  261 | ` * this routine does the initialization process and set the magic number so no body later` |
|       - |  262 | ` * can re-initialize the library.If subsequent threads call this  routine before the first` |
|       - |  263 | ` * thread have finished the initialization process, then the subsequent threads must block` |
|       - |  264 | ` * until the initialization process is done.` |
|       - |  265 | ` */` |
|    1980 |  266 | `static sxi32 PH7CoreInitialize(void)` |
|       2 |  267 |  |
|       - |  268 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|       - |  269 | `#if defined(PH7_ENABLE_THREADS)` |
|    1982 |  270 | `	const SyMutexMethods *pMutexMethods = 0;` |
|    1982 |  271 | `	SyMutex *pMaster = 0;` |
|       - |  272 | `#endif` |
|       - |  273 | `	int rc;` |
|       - |  274 | `	/*` |
|       - |  275 | `	 * If the library is already initialized,then a call to this routine` |
|       - |  276 | `	 * is a no-op.` |
|       - |  277 | `	 */` |
|    1982 |  278 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|     ! 0 |  279 | `		return PH7_OK; /* Already initialized */` |
|       - |  280 | `	}` |
|       - |  281 | `	/* Point to the built-in vfs */` |
|    1982 |  282 | `	pVfs = PH7_ExportBuiltinVfs();` |
|       - |  283 | `	/* Install it */` |
|    1982 |  284 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|       - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|    1982 |  286 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|    1982 |  287 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|    1982 |  288 | `		if( pMutexMethods == 0 ){` |
|       - |  289 | `			/* Use the built-in mutex subsystem */` |
|    1982 |  290 | `			pMutexMethods = SyMutexExportMethods();` |
|    1982 |  291 | `			if( pMutexMethods == 0 ){` |
|     ! 0 |  292 | `				return PH7_CORRUPT; /* Can't happen */` |
|       - |  293 | `			}` |
|       - |  294 | `			/* Install the mutex subsystem */` |
|    1982 |  295 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|    1982 |  296 | `			if( rc != PH7_OK ){` |
|     ! 0 |  297 | `				return rc;` |
|       - |  298 | `			}` |
|     990 |  299 | `		}` |
|       - |  300 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|    1982 |  301 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|    1982 |  302 | `		if( pMaster == 0 ){` |
|     ! 0 |  303 | `			return PH7_CORRUPT; /* Can't happen */` |
|       - |  304 | `		}` |
|     990 |  305 | `	}` |
|       - |  306 | `	/* Lock the master mutex */` |
|    1982 |  307 | `	rc = PH7_OK;` |
|    1982 |  308 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    2972 |  309 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  310 | `#endif` |
|    1982 |  311 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|       - |  312 | `			/* Install a memory subsystem */` |
|    1982 |  313 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|    1982 |  314 | `			if( rc != PH7_OK ){` |
|       - |  315 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|     ! 0 |  316 | `				goto End;` |
|       - |  317 | `			}` |
|     990 |  318 | `		}` |
|       - |  319 | `#if defined(PH7_ENABLE_THREADS)` |
|    1982 |  320 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  321 | `			/* Protect the memory allocation subsystem */` |
|    1982 |  322 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|    1982 |  323 | `			if( rc != PH7_OK ){` |
|     ! 0 |  324 | `				goto End;` |
|       - |  325 | `			}` |
|     990 |  326 | `		}` |
|       - |  327 | `#endif` |
|       - |  328 | `		/* Our library is initialized,set the magic number */` |
|    1982 |  329 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|    1982 |  330 | `		rc = PH7_OK;` |
|       - |  331 | `#if defined(PH7_ENABLE_THREADS)` |
|     990 |  332 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|       - |  333 | `#endif` |
|     ! 0 |  334 | `End:` |
|       - |  335 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  336 | `	/* Unlock the master mutex */` |
|    1982 |  337 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  338 | `#endif` |
|    1982 |  339 | `	return rc;` |
|     992 |  340 |  |
|       - |  341 | `/*` |
|       - |  342 | ` * [CAPIREF: ph7_lib_init()]` |
|       - |  343 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  344 | ` */` |
|     ! 0 |  345 | `int ph7_lib_init(void)` |
|     ! 0 |  346 |  |
|       - |  347 | `	int rc;` |
|     ! 0 |  348 | `	rc = PH7CoreInitialize();` |
|     ! 0 |  349 | `	return rc;` |
|     ! 0 |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Release an active PH7 engine and it's associated active virtual machines.` |
|       - |  353 | ` */` |
|    1972 |  354 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|       2 |  355 |  |
|       - |  356 | `	ph7_vm *pVm,*pNext;` |
|       - |  357 | `	/* Release all active VM */` |
|    1974 |  358 | `	pVm = pEngine->pVms;` |
|     986 |  359 | `	for(;;){` |
|    1974 |  360 | `		if( pEngine->iVm <= 0 ){` |
|    1974 |  361 | `			break;` |
|       - |  362 | `		}` |
|     ! 0 |  363 | `		pNext = pVm->pNext;` |
|     ! 0 |  364 | `		PH7_VmRelease(pVm);` |
|     ! 0 |  365 | `		pVm = pNext;` |
|     ! 0 |  366 | `		pEngine->iVm--;` |
|     ! 0 |  367 | `	}` |
|       - |  368 | `	/* Set a dummy magic number */` |
|    1974 |  369 | `	pEngine->nMagic = 0x7635;` |
|       - |  370 | `	/* Release the private memory subsystem */` |
|    1974 |  371 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|    1974 |  372 | `	return PH7_OK;` |
|       2 |  373 |  |
|       - |  374 | `/*` |
|       - |  375 | ` * Release all resources consumed by the library.` |
|       - |  376 | ` * If PH7 is already shut down when this routine` |
|       - |  377 | ` * is invoked then this routine is a harmless no-op.` |
|       - |  378 | ` * Note: This call is not thread safe.` |
|       - |  379 | ` * Refer to [ph7_lib_shutdown()].` |
|       - |  380 | ` */` |
|     238 |  381 | `static void PH7CoreShutdown(void)` |
|       1 |  382 |  |
|       - |  383 | `	ph7 *pEngine,*pNext;` |
|       - |  384 | `	/* Release all active engines first */` |
|     239 |  385 | `	pEngine = sMPGlobal.pEngines;` |
|     238 |  386 | `	for(;;){` |
|     477 |  387 | `		if( sMPGlobal.nEngine < 1 ){` |
|     239 |  388 | `			break;` |
|       - |  389 | `		}` |
|     239 |  390 | `		pNext = pEngine->pNext;` |
|     239 |  391 | `		EngineRelease(pEngine);` |
|     239 |  392 | `		pEngine = pNext;` |
|     239 |  393 | `		sMPGlobal.nEngine--;` |
|       1 |  394 | `	}` |
|       - |  395 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  396 | `	/* Release the mutex subsystem */` |
|     239 |  397 | `	if( sMPGlobal.pMutexMethods ){` |
|     239 |  398 | `		if( sMPGlobal.pMutex ){` |
|     239 |  399 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     239 |  400 | `			sMPGlobal.pMutex = 0;` |
|     119 |  401 | `		}` |
|     239 |  402 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|       1 |  403 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|     ! 0 |  404 | `		}` |
|     239 |  405 | `		sMPGlobal.pMutexMethods = 0;` |
|     119 |  406 | `	}` |
|     239 |  407 | `	sMPGlobal.nThreadingLevel = 0;` |
|       - |  408 | `#endif` |
|     239 |  409 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|       - |  410 | `		/* Release the memory backend */` |
|     239 |  411 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|     119 |  412 | `	}` |
|     239 |  413 | `	sMPGlobal.nMagic = 0x1928;` |
|     239 |  414 |  |
|       - |  415 | `/*` |
|       - |  416 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|       - |  417 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  418 | ` */` |
|     238 |  419 | `int ph7_lib_shutdown(void)` |
|       1 |  420 |  |
|     239 |  421 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|       - |  422 | `		/* Already shut */` |
|     ! 0 |  423 | `		return PH7_OK;` |
|       - |  424 | `	}` |
|     239 |  425 | `	PH7CoreShutdown();` |
|     239 |  426 | `	return PH7_OK;` |
|     120 |  427 |  |
|       - |  428 | `/*` |
|       - |  429 | ` * [CAPIREF: ph7_lib_is_threadsafe()]` |
|       - |  430 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  431 | ` */` |
|     ! 0 |  432 | `int ph7_lib_is_threadsafe(void)` |
|     ! 0 |  433 |  |
|     ! 0 |  434 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|     ! 0 |  435 | `		return 0;` |
|       - |  436 | `	}` |
|       - |  437 | `#if defined(PH7_ENABLE_THREADS)` |
|     ! 0 |  438 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  439 | `			/* Muli-threading support is enabled */` |
|     ! 0 |  440 | `			return 1;` |
|     ! 0 |  441 | `		}else{` |
|       - |  442 | `			/* Single-threading */` |
|     ! 0 |  443 | `			return 0;` |
|       - |  444 | `		}` |
|       - |  445 | `#else` |
|       - |  446 | `	return 0;` |
|       - |  447 | `#endif` |
|     ! 0 |  448 |  |
|       - |  449 | `/*` |
|       - |  450 | ` * [CAPIREF: ph7_lib_version()]` |
|       - |  451 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  452 | ` */` |
|      26 |  453 | `const char * ph7_lib_version(void)` |
|       2 |  454 |  |
|      28 |  455 | `	return PH7_VERSION;` |
|       2 |  456 |  |
|       - |  457 | `/*` |
|       - |  458 | ` * [CAPIREF: ph7_lib_signature()]` |
|       - |  459 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  460 | ` */` |
|      10 |  461 | `const char * ph7_lib_signature(void)` |
|       1 |  462 |  |
|      11 |  463 | `	return PH7_SIG;` |
|       1 |  464 |  |
|       - |  465 | `/*` |
|       - |  466 | ` * [CAPIREF: ph7_lib_ident()]` |
|       - |  467 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  468 | ` */` |
|       2 |  469 | `const char * ph7_lib_ident(void)` |
|       1 |  470 |  |
|       3 |  471 | `	return PH7_IDENT;` |
|       1 |  472 |  |
|       - |  473 | `/*` |
|       - |  474 | ` * [CAPIREF: ph7_lib_copyright()]` |
|       - |  475 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  476 | ` */` |
|     ! 0 |  477 | `const char * ph7_lib_copyright(void)` |
|     ! 0 |  478 |  |
|     ! 0 |  479 | `	return PH7_COPYRIGHT;` |
|     ! 0 |  480 |  |
|       - |  481 | `/*` |
|       - |  482 | ` * [CAPIREF: ph7_config()]` |
|       - |  483 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  484 | ` */` |
|    3960 |  485 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|       2 |  486 |  |
|       - |  487 | `	va_list ap;` |
|       - |  488 | `	int rc;` |
|    3962 |  489 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  490 | `		return PH7_CORRUPT;` |
|       - |  491 | `	}` |
|       - |  492 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  493 | `	 /* Acquire engine mutex */` |
|    3962 |  494 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    3962 |  495 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    3960 |  496 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  497 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  498 | `	 }` |
|       - |  499 | `#endif` |
|    3962 |  500 | `	 va_start(ap,nConfigOp);` |
|    3962 |  501 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    3962 |  502 | `	 va_end(ap);` |
|       - |  503 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  504 | `	 /* Leave engine mutex */` |
|    3962 |  505 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  506 | `#endif` |
|    3962 |  507 | `	return rc;` |
|    1982 |  508 |  |
|       - |  509 | `/*` |
|       - |  510 | ` * [CAPIREF: ph7_init()]` |
|       - |  511 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  512 | ` */` |
|    1980 |  513 | `int ph7_init(ph7 **ppEngine)` |
|       2 |  514 |  |
|       - |  515 | `	ph7 *pEngine;` |
|       - |  516 | `	int rc;` |
|       - |  517 | `#if defined(UNTRUST)` |
|       - |  518 | `	if( ppEngine == 0 ){` |
|       - |  519 | `		return PH7_CORRUPT;` |
|       - |  520 | `	}` |
|       - |  521 | `#endif` |
|    1982 |  522 | `	*ppEngine = 0;` |
|       - |  523 | `	/* One-time automatic library initialization */` |
|    1982 |  524 | `	rc = PH7CoreInitialize();` |
|    1982 |  525 | `	if( rc != PH7_OK ){` |
|     ! 0 |  526 | `		return rc;` |
|       - |  527 | `	}` |
|       - |  528 | `	/* Allocate a new engine */` |
|    1982 |  529 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|    1982 |  530 | `	if( pEngine == 0 ){` |
|     ! 0 |  531 | `		return PH7_NOMEM;` |
|       - |  532 | `	}` |
|       - |  533 | `	/* Zero the structure */` |
|    1982 |  534 | `	SyZero(pEngine,sizeof(ph7));` |
|       - |  535 | `	/* Initialize engine fields */` |
|    1982 |  536 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|    1982 |  537 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|    1982 |  538 | `	if( rc != PH7_OK ){` |
|     ! 0 |  539 | `		goto Release;` |
|       - |  540 | `	}` |
|       - |  541 | `#if defined(PH7_ENABLE_THREADS)` |
|    1982 |  542 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|       - |  543 | `#endif` |
|       - |  544 | `	/* Default configuration */` |
|    1982 |  545 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|       - |  546 | `	/* Install a default compile-time error consumer routine */` |
|    1982 |  547 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|       - |  548 | `	/* Built-in vfs */` |
|    1982 |  549 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|       - |  550 | `#if defined(PH7_ENABLE_THREADS)` |
|    1982 |  551 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  552 | `		 /* Associate a recursive mutex with this instance */` |
|    1982 |  553 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    1982 |  554 | `		 if( pEngine->pMutex == 0 ){` |
|     ! 0 |  555 | `			 rc = PH7_NOMEM;` |
|     ! 0 |  556 | `			 goto Release;` |
|       - |  557 | `		 }` |
|     990 |  558 | `	 }` |
|       - |  559 | `#endif` |
|       - |  560 | `	/* Link to the list of active engines */` |
|       - |  561 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  562 | `	/* Enter the global mutex */` |
|    1982 |  563 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  564 | `#endif` |
|    1982 |  565 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|    1982 |  566 | `	sMPGlobal.nEngine++;` |
|       - |  567 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  568 | `	/* Leave the global mutex */` |
|    1982 |  569 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  570 | `#endif` |
|       - |  571 | `	/* Write a pointer to the new instance */` |
|    1982 |  572 | `	*ppEngine = pEngine;` |
|    1982 |  573 | `	return PH7_OK;` |
|     ! 0 |  574 | `Release:` |
|     ! 0 |  575 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     ! 0 |  576 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     ! 0 |  577 | `	return rc;` |
|     992 |  578 |  |
|       - |  579 | `/*` |
|       - |  580 | ` * [CAPIREF: ph7_release()]` |
|       - |  581 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  582 | ` */` |
|    1734 |  583 | `int ph7_release(ph7 *pEngine)` |
|       2 |  584 |  |
|       - |  585 | `	int rc;` |
|    1736 |  586 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|     ! 0 |  587 | `		return PH7_CORRUPT;` |
|       - |  588 | `	}` |
|       - |  589 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  590 | `	 /* Acquire engine mutex */` |
|    1736 |  591 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    1736 |  592 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    1734 |  593 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  594 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  595 | `	 }` |
|       - |  596 | `#endif` |
|       - |  597 | `	/* Release the engine */` |
|    1736 |  598 | `	rc = EngineRelease(&(*pEngine));` |
|       - |  599 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  600 | `	 /* Leave engine mutex */` |
|    1736 |  601 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  602 | `	 /* Release engine mutex */` |
|    1736 |  603 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  604 | `#endif` |
|       - |  605 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  606 | `	/* Enter the global mutex */` |
|    1736 |  607 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  608 | `#endif` |
|       - |  609 | `	/* Unlink from the list of active engines */` |
|    1736 |  610 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|    1736 |  611 | `	sMPGlobal.nEngine--;` |
|       - |  612 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  613 | `	/* Leave the global mutex */` |
|    1736 |  614 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|       - |  615 | `#endif` |
|       - |  616 | `	/* Release the memory chunk allocated to this engine */` |
|    1736 |  617 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|    1736 |  618 | `	return rc;` |
|     869 |  619 |  |
|       - |  620 | `/*` |
|       - |  621 | ` * Compile a raw PHP script.` |
|       - |  622 | ` * To execute a PHP code, it must first be compiled into a byte-code program using this routine.` |
|       - |  623 | ` * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]` |
|       - |  624 | ` * should  display the appropriate error message and this function set ppVm to null and return` |
|       - |  625 | ` * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled` |
|       - |  626 | ` * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].` |
|       - |  627 | ` * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script` |
|       - |  628 | ` * for evaluation.` |
|       - |  629 | ` */` |
|    1980 |  630 | `static sxi32 ProcessScript(` |
|       - |  631 | `	ph7 *pEngine,          /* Running PH7 engine */` |
|       - |  632 | `	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */` |
|       - |  633 | `	SyString *pScript,     /* Raw PHP script to compile */` |
|       - |  634 | `	sxi32 iFlags,          /* Compile-time flags */` |
|       - |  635 | `	const char *zFilePath  /* File path if script come from a file. NULL otherwise */` |
|       - |  636 | `	)` |
|       2 |  637 |  |
|       - |  638 | `	ph7_vm *pVm;` |
|       - |  639 | `	int rc;` |
|       - |  640 | `	/* Allocate a new virtual machine */` |
|    1982 |  641 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|    1982 |  642 | `	if( pVm == 0 ){` |
|       - |  643 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  644 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|     ! 0 |  645 | `		if( ppVm ){` |
|     ! 0 |  646 | `			*ppVm = 0;` |
|     ! 0 |  647 | `		}` |
|     ! 0 |  648 | `		return PH7_NOMEM;` |
|       - |  649 | `	}` |
|    1982 |  650 | `	if( iFlags < 0 ){` |
|       - |  651 | `		/* Default compile-time flags */` |
|     ! 0 |  652 | `		iFlags = 0;` |
|     ! 0 |  653 | `	}` |
|       - |  654 | `	/* Initialize the Virtual Machine */` |
|    1982 |  655 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|    1982 |  656 | `	if( rc != PH7_OK ){` |
|     ! 0 |  657 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  658 | `		if( ppVm ){` |
|     ! 0 |  659 | `			*ppVm = 0;` |
|     ! 0 |  660 | `		}` |
|     ! 0 |  661 | `		return PH7_VM_ERR;` |
|       - |  662 | `	}` |
|    1982 |  663 | `	if( zFilePath ){` |
|       - |  664 | `		/* Push processed file path */` |
|    1974 |  665 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|     986 |  666 | `	}` |
|       - |  667 | `	/* Reset the error message consumer */` |
|    1982 |  668 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|       - |  669 | `	/* Compile the script */` |
|    1982 |  670 | `	PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|    1982 |  671 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     239 |  672 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|       - |  673 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     239 |  674 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     239 |  675 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     239 |  676 | `		if( ppVm ){` |
|     239 |  677 | `			*ppVm = 0;` |
|     119 |  678 | `		}` |
|     239 |  679 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|       - |  680 | `	}` |
|       - |  681 | `	/* Prepare the virtual machine for bytecode execution */` |
|    1744 |  682 | `	rc = PH7_VmMakeReady(pVm);` |
|    1744 |  683 | `	if( rc != PH7_OK ){` |
|     ! 0 |  684 | `		goto Release;` |
|       - |  685 | `	}` |
|       - |  686 | `	/* Install local import path which is the current directory */` |
|    1744 |  687 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|       - |  688 | `#if defined(PH7_ENABLE_THREADS)` |
|    1744 |  689 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|       - |  690 | `		 /* Associate a recursive mutex with this instance */` |
|    1744 |  691 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|    1744 |  692 | `		 if( pVm->pMutex == 0 ){` |
|     ! 0 |  693 | `			 goto Release;` |
|       - |  694 | `		 }` |
|     871 |  695 | `	 }` |
|       - |  696 | `#endif` |
|       - |  697 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|    1744 |  698 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|    1744 |  699 | `	pEngine->iVm++;` |
|       - |  700 | `	/* Point to the freshly created VM */` |
|    1744 |  701 | `	*ppVm = pVm;` |
|       - |  702 | `	/* Ready to execute PH7 bytecode */` |
|    1744 |  703 | `	return PH7_OK;` |
|     ! 0 |  704 | `Release:` |
|     ! 0 |  705 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     ! 0 |  706 | `	SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     ! 0 |  707 | `	*ppVm = 0;` |
|     ! 0 |  708 | `	return PH7_VM_ERR;` |
|     992 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * [CAPIREF: ph7_compile()]` |
|       - |  712 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  713 | ` */` |
|     ! 0 |  714 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|     ! 0 |  715 |  |
|       - |  716 | `	SyString sScript;` |
|       - |  717 | `	int rc;` |
|     ! 0 |  718 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  719 | `		return PH7_CORRUPT;` |
|       - |  720 | `	}` |
|     ! 0 |  721 | `	if( nLen < 0 ){` |
|       - |  722 | `		/* Compute input length automatically */` |
|     ! 0 |  723 | `		nLen = (int)SyStrlen(zSource);` |
|     ! 0 |  724 | `	}` |
|     ! 0 |  725 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  726 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  727 | `	 /* Acquire engine mutex */` |
|     ! 0 |  728 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  729 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  730 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  731 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  732 | `	 }` |
|       - |  733 | `#endif` |
|       - |  734 | `	/* Compile the script */` |
|     ! 0 |  735 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|       - |  736 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  737 | `	 /* Leave engine mutex */` |
|     ! 0 |  738 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  739 | `#endif` |
|       - |  740 | `	/* Compilation result */` |
|     ! 0 |  741 | `	return rc;` |
|     ! 0 |  742 |  |
|       - |  743 | `/*` |
|       - |  744 | ` * [CAPIREF: ph7_compile_v2()]` |
|       - |  745 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  746 | ` */` |
|       8 |  747 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|       1 |  748 |  |
|       - |  749 | `	SyString sScript;` |
|       - |  750 | `	int rc;` |
|       9 |  751 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|     ! 0 |  752 | `		return PH7_CORRUPT;` |
|       - |  753 | `	}` |
|       9 |  754 | `	if( nLen < 0 ){` |
|       - |  755 | `		/* Compute input length automatically */` |
|       9 |  756 | `		nLen = (int)SyStrlen(zSource);` |
|       4 |  757 | `	}` |
|       9 |  758 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|       - |  759 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  760 | `	 /* Acquire engine mutex */` |
|       9 |  761 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       9 |  762 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       8 |  763 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  764 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  765 | `	 }` |
|       - |  766 | `#endif` |
|       - |  767 | `	/* Compile the script */` |
|       9 |  768 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|       - |  769 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  770 | `	 /* Leave engine mutex */` |
|       9 |  771 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  772 | `#endif` |
|       - |  773 | `	/* Compilation result */` |
|       9 |  774 | `	return rc;` |
|       5 |  775 |  |
|       - |  776 | `/*` |
|       - |  777 | ` * [CAPIREF: ph7_compile_file()]` |
|       - |  778 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  779 | ` */` |
|    1972 |  780 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|       2 |  781 |  |
|       - |  782 | `	const ph7_vfs *pVfs;` |
|       - |  783 | `	int rc;` |
|    1974 |  784 | `	if( ppOutVm ){` |
|    1974 |  785 | `		*ppOutVm = 0;` |
|     986 |  786 | `	}` |
|    1974 |  787 | `	rc = PH7_OK; /* cc warning */` |
|    1974 |  788 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|     ! 0 |  789 | `		return PH7_CORRUPT;` |
|       - |  790 | `	}` |
|       - |  791 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  792 | `	 /* Acquire engine mutex */` |
|    1974 |  793 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    1974 |  794 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    1972 |  795 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  796 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  797 | `	 }` |
|       - |  798 | `#endif` |
|       - |  799 | `	 /*` |
|       - |  800 | `	  * Check if the underlying vfs implement the memory map` |
|       - |  801 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|       - |  802 | `	  */` |
|    1974 |  803 | `	 pVfs = pEngine->pVfs;` |
|    1974 |  804 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|       - |  805 | `		 /* Memory map routine not implemented */` |
|     ! 0 |  806 | `		 rc = PH7_IO_ERR;` |
|     ! 0 |  807 | `	 }else{` |
|    1974 |  808 | `		 void *pMapView = 0; /* cc warning */` |
|    1974 |  809 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|       - |  810 | `		 SyString sScript;` |
|       - |  811 | `		 /* Try to get a memory view of the whole file */` |
|    1974 |  812 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|    1974 |  813 | `		 if( rc != PH7_OK ){` |
|       - |  814 | `			 /* Assume an IO error */` |
|     ! 0 |  815 | `			 rc = PH7_IO_ERR;` |
|     ! 0 |  816 | `		 }else{` |
|       - |  817 | `			 /* Compile the file */` |
|    1974 |  818 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|    1974 |  819 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|       - |  820 | `			 /* Release the memory view of the whole file */` |
|    1974 |  821 | `			 if( pVfs->xUnmap ){` |
|    1974 |  822 | `				 pVfs->xUnmap(pMapView,nSize);` |
|     986 |  823 | `			 }` |
|       - |  824 | `		 }` |
|       - |  825 | `	 }` |
|       - |  826 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  827 | `	 /* Leave engine mutex */` |
|    1974 |  828 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  829 | `#endif` |
|       - |  830 | `	/* Compilation result */` |
|    1974 |  831 | `	return rc;` |
|     988 |  832 |  |
|       - |  833 | `/*` |
|       - |  834 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|       - |  835 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  836 | ` */` |
|       2 |  837 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|       1 |  838 |  |
|       - |  839 | `	int rc;` |
|       - |  840 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       3 |  841 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  842 | `		return PH7_CORRUPT;` |
|       - |  843 | `	}` |
|       - |  844 | `#ifdef UNTRUST` |
|       - |  845 | `	if( xConsumer == 0 ){` |
|       - |  846 | `		return PH7_CORRUPT;` |
|       - |  847 | `	}` |
|       - |  848 | `#endif` |
|       - |  849 | `	/* Dump VM instructions */` |
|       3 |  850 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|       3 |  851 | `	return rc;` |
|       2 |  852 |  |
|       - |  853 | `/*` |
|       - |  854 | ` * [CAPIREF: ph7_vm_config()]` |
|       - |  855 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  856 | ` */` |
|   27896 |  857 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|       2 |  858 |  |
|       - |  859 | `	va_list ap;` |
|       - |  860 | `	int rc;` |
|       - |  861 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   27898 |  862 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  863 | `		return PH7_CORRUPT;` |
|       - |  864 | `	}` |
|       - |  865 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  866 | `	 /* Acquire VM mutex */` |
|   27898 |  867 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   27898 |  868 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   27896 |  869 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  870 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  871 | `	 }` |
|       - |  872 | `#endif` |
|       - |  873 | `	/* Confiugure the virtual machine */` |
|   27898 |  874 | `	va_start(ap,iConfigOp);` |
|   27898 |  875 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   27898 |  876 | `	va_end(ap);` |
|       - |  877 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  878 | `	 /* Leave VM mutex */` |
|   27898 |  879 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  880 | `#endif` |
|   27898 |  881 | `	return rc;` |
|   13950 |  882 |  |
|       - |  883 | `/*` |
|       - |  884 | ` * [CAPIREF: ph7_vm_exec()]` |
|       - |  885 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  886 | ` */` |
|    1746 |  887 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|       2 |  888 |  |
|       - |  889 | `	int rc;` |
|       - |  890 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    1748 |  891 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       8 |  892 | `		return PH7_CORRUPT;` |
|       - |  893 | `	}` |
|       - |  894 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  895 | `	 /* Acquire VM mutex */` |
|    1748 |  896 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    1748 |  897 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    1742 |  898 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  899 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  900 | `	 }` |
|       - |  901 | `#endif` |
|       - |  902 | `	/* Execute PH7 byte-code */` |
|    1748 |  903 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|    1744 |  904 | `	if( pExitStatus ){` |
|       - |  905 | `		/* Exit status */` |
|     ! 0 |  906 | `		*pExitStatus = pVm->iExitStatus;` |
|     ! 0 |  907 | `	}` |
|       - |  908 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  909 | `	 /* Leave VM mutex */` |
|    1736 |  910 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  911 | `#endif` |
|       - |  912 | `	/* Execution result */` |
|    1736 |  913 | `	return rc;` |
|     869 |  914 |  |
|       - |  915 | `/*` |
|       - |  916 | ` * [CAPIREF: ph7_vm_reset()]` |
|       - |  917 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  918 | ` */` |
|     ! 0 |  919 | `int ph7_vm_reset(ph7_vm *pVm)` |
|     ! 0 |  920 |  |
|       - |  921 | `	int rc;` |
|       - |  922 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 |  923 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  924 | `		return PH7_CORRUPT;` |
|       - |  925 | `	}` |
|       - |  926 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  927 | `	 /* Acquire VM mutex */` |
|     ! 0 |  928 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 |  929 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 |  930 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  931 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  932 | `	 }` |
|       - |  933 | `#endif` |
|     ! 0 |  934 | `	rc = PH7_VmReset(&(*pVm));` |
|       - |  935 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  936 | `	 /* Leave VM mutex */` |
|     ! 0 |  937 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  938 | `#endif` |
|     ! 0 |  939 | `	return rc;` |
|     ! 0 |  940 |  |
|       - |  941 | `/*` |
|       - |  942 | ` * [CAPIREF: ph7_vm_release()]` |
|       - |  943 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  944 | ` */` |
|    1734 |  945 | `int ph7_vm_release(ph7_vm *pVm)` |
|       2 |  946 |  |
|       - |  947 | `	ph7 *pEngine;` |
|       - |  948 | `	int rc;` |
|       - |  949 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    1736 |  950 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  951 | `		return PH7_CORRUPT;` |
|       - |  952 | `	}` |
|       - |  953 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  954 | `	 /* Acquire VM mutex */` |
|    1736 |  955 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    1736 |  956 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    1734 |  957 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 |  958 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  959 | `	 }` |
|       - |  960 | `#endif` |
|    1736 |  961 | `	pEngine = pVm->pEngine;` |
|    1736 |  962 | `	rc = PH7_VmRelease(&(*pVm));` |
|       - |  963 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  964 | `	 /* Leave VM mutex */` |
|    1736 |  965 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  966 | `#endif` |
|    1736 |  967 | `	if( rc == PH7_OK ){` |
|       - |  968 | `		/* Unlink from the list of active VM */` |
|       - |  969 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  970 | `			/* Acquire engine mutex */` |
|    1736 |  971 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    1736 |  972 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    1734 |  973 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|     ! 0 |  974 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|       - |  975 | `			}` |
|       - |  976 | `#endif` |
|    1736 |  977 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|    1736 |  978 | `		pEngine->iVm--;` |
|       - |  979 | `		/* Release the memory chunk allocated to this VM */` |
|    1736 |  980 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|       - |  981 | `#if defined(PH7_ENABLE_THREADS)` |
|       - |  982 | `			/* Leave engine mutex */` |
|    1736 |  983 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - |  984 | `#endif` |
|     867 |  985 | `	}` |
|    1736 |  986 | `	return rc;` |
|     869 |  987 |  |
|       - |  988 | `/*` |
|       - |  989 | ` * [CAPIREF: ph7_create_function()]` |
|       - |  990 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - |  991 | ` */` |
|  759512 |  992 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|       2 |  993 |  |
|       - |  994 | `	SyString sName;` |
|       - |  995 | `	int rc;` |
|       - |  996 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  759514 |  997 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 |  998 | `		return PH7_CORRUPT;` |
|       - |  999 | `	}` |
|  759514 | 1000 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1001 | `	/* Remove leading and trailing white spaces */` |
|  759514 | 1002 | `	SyStringFullTrim(&sName);` |
|       - | 1003 | `	/* Ticket 1433-003: NULL values are not allowed */` |
|  759514 | 1004 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|     ! 0 | 1005 | `		return PH7_CORRUPT;` |
|       - | 1006 | `	}` |
|       - | 1007 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1008 | `	 /* Acquire VM mutex */` |
|  759514 | 1009 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  759514 | 1010 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  759512 | 1011 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1012 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1013 | `	 }` |
|       - | 1014 | `#endif` |
|       - | 1015 | `	/* Install the foreign function */` |
|  759514 | 1016 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|       - | 1017 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1018 | `	 /* Leave VM mutex */` |
|  759514 | 1019 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1020 | `#endif` |
|  759514 | 1021 | `	return rc;` |
|  379758 | 1022 |  |
|       - | 1023 | `/*` |
|       - | 1024 | ` * [CAPIREF: ph7_delete_function()]` |
|       - | 1025 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1026 | ` */` |
|     ! 0 | 1027 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1028 |  |
|     ! 0 | 1029 | `	ph7_user_func *pFunc = 0;` |
|       - | 1030 | `	int rc;` |
|       - | 1031 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1032 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1033 | `		return PH7_CORRUPT;` |
|       - | 1034 | `	}` |
|       - | 1035 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1036 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1037 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1038 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1039 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1040 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1041 | `	 }` |
|       - | 1042 | `#endif` |
|       - | 1043 | `	/* Perform the deletion */` |
|     ! 0 | 1044 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|     ! 0 | 1045 | `	if( rc == PH7_OK ){` |
|       - | 1046 | `		/* Release internal fields */` |
|     ! 0 | 1047 | `		SySetRelease(&pFunc->aAux);` |
|     ! 0 | 1048 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|     ! 0 | 1049 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     ! 0 | 1050 | `	}` |
|       - | 1051 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1052 | `	 /* Leave VM mutex */` |
|     ! 0 | 1053 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1054 | `#endif` |
|     ! 0 | 1055 | `	return rc;` |
|     ! 0 | 1056 |  |
|       - | 1057 | `/*` |
|       - | 1058 | ` * [CAPIREF: ph7_create_constant()]` |
|       - | 1059 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1060 | ` */` |
|  351894 | 1061 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|       2 | 1062 |  |
|       - | 1063 | `	SyString sName;` |
|       - | 1064 | `	int rc;` |
|       - | 1065 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  351896 | 1066 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1067 | `		return PH7_CORRUPT;` |
|       - | 1068 | `	}` |
|  351896 | 1069 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|       - | 1070 | `	/* Remove leading and trailing white spaces */` |
|  353638 | 1071 | `	SyStringFullTrim(&sName);` |
|  351896 | 1072 | `	if( sName.nByte < 1 ){` |
|       - | 1073 | `		/* Empty constant name */` |
|     ! 0 | 1074 | `		return PH7_CORRUPT;` |
|       - | 1075 | `	}` |
|       - | 1076 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
|  351896 | 1077 | `	if( xExpand == 0 ){` |
|     ! 0 | 1078 | `		return PH7_CORRUPT;` |
|       - | 1079 | `	}` |
|       - | 1080 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1081 | `	 /* Acquire VM mutex */` |
|  351896 | 1082 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  351896 | 1083 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  351894 | 1084 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1085 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1086 | `	 }` |
|       - | 1087 | `#endif` |
|       - | 1088 | `	/* Perform the registration */` |
|  351896 | 1089 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|       - | 1090 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1091 | `	 /* Leave VM mutex */` |
|  351896 | 1092 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1093 | `#endif` |
|  351896 | 1094 | `	 return rc;` |
|  175949 | 1095 |  |
|       - | 1096 | `/*` |
|       - | 1097 | ` * [CAPIREF: ph7_delete_constant()]` |
|       - | 1098 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1099 | ` */` |
|     ! 0 | 1100 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|     ! 0 | 1101 |  |
|       - | 1102 | `	ph7_constant *pCons;` |
|       - | 1103 | `	int rc;` |
|       - | 1104 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     ! 0 | 1105 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1106 | `		return PH7_CORRUPT;` |
|       - | 1107 | `	}` |
|       - | 1108 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1109 | `	 /* Acquire VM mutex */` |
|     ! 0 | 1110 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     ! 0 | 1111 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     ! 0 | 1112 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|     ! 0 | 1113 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|       - | 1114 | `	 }` |
|       - | 1115 | `#endif` |
|       - | 1116 | `	 /* Query the constant hashtable */` |
|     ! 0 | 1117 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|     ! 0 | 1118 | `	 if( rc == PH7_OK ){` |
|       - | 1119 | `		 /* Perform the deletion */` |
|     ! 0 | 1120 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|     ! 0 | 1121 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|     ! 0 | 1122 | `	 }` |
|       - | 1123 | `#if defined(PH7_ENABLE_THREADS)` |
|       - | 1124 | `	 /* Leave VM mutex */` |
|     ! 0 | 1125 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       - | 1126 | `#endif` |
|     ! 0 | 1127 | `	return rc;` |
|     ! 0 | 1128 |  |
|       - | 1129 | `/*` |
|       - | 1130 | ` * [CAPIREF: ph7_new_scalar()]` |
|       - | 1131 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1132 | ` */` |
|    4704 | 1133 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|       2 | 1134 |  |
|       - | 1135 | `	ph7_value *pObj;` |
|       - | 1136 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|    4706 | 1137 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1138 | `		return 0;` |
|       - | 1139 | `	}` |
|       - | 1140 | `	/* Allocate a new scalar variable */` |
|    4706 | 1141 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    4706 | 1142 | `	if( pObj == 0 ){` |
|     ! 0 | 1143 | `		return 0;` |
|       - | 1144 | `	}` |
|       - | 1145 | `	/* Nullify the new scalar */` |
|    4706 | 1146 | `	PH7_MemObjInit(pVm,pObj);` |
|    4706 | 1147 | `	return pObj;` |
|    2354 | 1148 |  |
|       - | 1149 | `/*` |
|       - | 1150 | ` * [CAPIREF: ph7_new_array()]` |
|       - | 1151 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1152 | ` */` |
|   24206 | 1153 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|       2 | 1154 |  |
|       - | 1155 | `	ph7_hashmap *pMap;` |
|       - | 1156 | `	ph7_value *pObj;` |
|       - | 1157 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   24208 | 1158 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1159 | `		return 0;` |
|       - | 1160 | `	}` |
|       - | 1161 | `	/* Create a new hashmap first */` |
|   24208 | 1162 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   24208 | 1163 | `	if( pMap == 0 ){` |
|     ! 0 | 1164 | `		return 0;` |
|       - | 1165 | `	}` |
|       - | 1166 | `	/* Associate a new ph7_value with this hashmap */` |
|   24208 | 1167 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   24208 | 1168 | `	if( pObj == 0 ){` |
|     ! 0 | 1169 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     ! 0 | 1170 | `		return 0;` |
|       - | 1171 | `	}` |
|   24208 | 1172 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|   24208 | 1173 | `	return pObj;` |
|   12105 | 1174 |  |
|       - | 1175 | `/*` |
|       - | 1176 | ` * [CAPIREF: ph7_release_value()]` |
|       - | 1177 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1178 | ` */` |
|   17420 | 1179 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|       2 | 1180 |  |
|       - | 1181 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   17422 | 1182 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|     ! 0 | 1183 | `		return PH7_CORRUPT;` |
|       - | 1184 | `	}` |
|   17422 | 1185 | `	if( pValue ){` |
|       - | 1186 | `		/* Release the value */` |
|   17422 | 1187 | `		PH7_MemObjRelease(pValue);` |
|   17422 | 1188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|    8710 | 1189 | `	}` |
|   17422 | 1190 | `	return PH7_OK;` |
|    8712 | 1191 |  |
|       - | 1192 | `/*` |
|       - | 1193 | ` * [CAPIREF: ph7_value_to_int()]` |
|       - | 1194 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1195 | ` */` |
|  257724 | 1196 | `int ph7_value_to_int(ph7_value *pValue)` |
|       2 | 1197 |  |
|       - | 1198 | `	int rc;` |
|  257726 | 1199 | `	rc = PH7_MemObjToInteger(pValue);` |
|  257726 | 1200 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1201 | `		return 0;` |
|       - | 1202 | `	}` |
|  257726 | 1203 | `	return (int)pValue->x.iVal;` |
|  128864 | 1204 |  |
|       - | 1205 | `/*` |
|       - | 1206 | ` * [CAPIREF: ph7_value_to_bool()]` |
|       - | 1207 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1208 | ` */` |
|     144 | 1209 | `int ph7_value_to_bool(ph7_value *pValue)` |
|       2 | 1210 |  |
|       - | 1211 | `	int rc;` |
|     146 | 1212 | `	rc = PH7_MemObjToBool(pValue);` |
|     146 | 1213 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1214 | `		return 0;` |
|       - | 1215 | `	}` |
|     146 | 1216 | `	return (int)pValue->x.iVal;` |
|      74 | 1217 |  |
|       - | 1218 | `/*` |
|       - | 1219 | ` * [CAPIREF: ph7_value_to_int64()]` |
|       - | 1220 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1221 | ` */` |
|     150 | 1222 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|       1 | 1223 |  |
|       - | 1224 | `	int rc;` |
|     151 | 1225 | `	rc = PH7_MemObjToInteger(pValue);` |
|     151 | 1226 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1227 | `		return 0;` |
|       - | 1228 | `	}` |
|     151 | 1229 | `	return pValue->x.iVal;` |
|      76 | 1230 |  |
|       - | 1231 | `/*` |
|       - | 1232 | ` * [CAPIREF: ph7_value_to_double()]` |
|       - | 1233 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1234 | ` */` |
|     370 | 1235 | `double ph7_value_to_double(ph7_value *pValue)` |
|       1 | 1236 |  |
|       - | 1237 | `	int rc;` |
|     371 | 1238 | `	rc = PH7_MemObjToReal(pValue);` |
|     371 | 1239 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1240 | `		return (double)0;` |
|       - | 1241 | `	}` |
|     371 | 1242 | `	return (double)pValue->rVal;` |
|     186 | 1243 |  |
|       - | 1244 | `/*` |
|       - | 1245 | ` * [CAPIREF: ph7_value_to_string()]` |
|       - | 1246 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1247 | ` */` |
|  521282 | 1248 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|       2 | 1249 |  |
|  521284 | 1250 | `	PH7_MemObjToString(pValue);` |
|  521284 | 1251 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  499268 | 1252 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  499268 | 1253 | `		if( pLen ){` |
|  459888 | 1254 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  229965 | 1255 | `		}` |
|  499268 | 1256 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|     ! 0 | 1257 | `	}else{` |
|       - | 1258 | `		/* Return the empty string */` |
|   22018 | 1259 | `		if( pLen ){` |
|   22008 | 1260 | `			*pLen = 0;` |
|   11003 | 1261 | `		}` |
|   22018 | 1262 | `		return "";` |
|       - | 1263 | `	}` |
|  260665 | 1264 |  |
|       - | 1265 | `/*` |
|       - | 1266 | ` * [CAPIREF: ph7_value_to_resource()]` |
|       - | 1267 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1268 | ` */` |
|   17086 | 1269 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|       2 | 1270 |  |
|   17088 | 1271 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|       - | 1272 | `		/* Not a resource,return NULL */` |
|     ! 0 | 1273 | `		return 0;` |
|       - | 1274 | `	}` |
|   17088 | 1275 | `	return pValue->x.pOther;` |
|    8545 | 1276 |  |
|       - | 1277 | `/*` |
|       - | 1278 | ` * [CAPIREF: ph7_value_compare()]` |
|       - | 1279 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1280 | ` */` |
|      30 | 1281 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|       1 | 1282 |  |
|       - | 1283 | `	int rc;` |
|      31 | 1284 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|       - | 1285 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|     ! 0 | 1286 | `		return 1;` |
|       - | 1287 | `	}` |
|       - | 1288 | `	/* Perform the comparison */` |
|      31 | 1289 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|       - | 1290 | `	/* Comparison result */` |
|      31 | 1291 | `	return rc;` |
|      16 | 1292 |  |
|       - | 1293 | `/*` |
|       - | 1294 | ` * [CAPIREF: ph7_result_int()]` |
|       - | 1295 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1296 | ` */` |
|    5778 | 1297 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|       2 | 1298 |  |
|    5780 | 1299 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|       2 | 1300 |  |
|       - | 1301 | `/*` |
|       - | 1302 | ` * [CAPIREF: ph7_result_int64()]` |
|       - | 1303 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1304 | ` */` |
|   10678 | 1305 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|       2 | 1306 |  |
|   10680 | 1307 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|       2 | 1308 |  |
|       - | 1309 | `/*` |
|       - | 1310 | ` * [CAPIREF: ph7_result_bool()]` |
|       - | 1311 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1312 | ` */` |
|  261114 | 1313 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|       2 | 1314 |  |
|  261116 | 1315 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|       2 | 1316 |  |
|       - | 1317 | `/*` |
|       - | 1318 | ` * [CAPIREF: ph7_result_double()]` |
|       - | 1319 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1320 | ` */` |
|     350 | 1321 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|       1 | 1322 |  |
|     351 | 1323 | `	return ph7_value_double(pCtx->pRet,Value);` |
|       1 | 1324 |  |
|       - | 1325 | `/*` |
|       - | 1326 | ` * [CAPIREF: ph7_result_null()]` |
|       - | 1327 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1328 | ` */` |
|     114 | 1329 | `int ph7_result_null(ph7_context *pCtx)` |
|       2 | 1330 |  |
|       - | 1331 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     116 | 1332 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     116 | 1333 | `	return PH7_OK;` |
|       2 | 1334 |  |
|       - | 1335 | `/*` |
|       - | 1336 | ` * [CAPIREF: ph7_result_string()]` |
|       - | 1337 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1338 | ` */` |
|  682076 | 1339 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|       2 | 1340 |  |
|  682078 | 1341 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|       2 | 1342 |  |
|       - | 1343 | `/*` |
|       - | 1344 | ` * [CAPIREF: ph7_result_string_format()]` |
|       - | 1345 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1346 | ` */` |
|     222 | 1347 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1348 |  |
|       - | 1349 | `	ph7_value *p;` |
|       - | 1350 | `	va_list ap;` |
|       - | 1351 | `	int rc;` |
|     223 | 1352 | `	p = pCtx->pRet;` |
|     223 | 1353 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1354 | `		/* Invalidate any prior representation */` |
|     137 | 1355 | `		PH7_MemObjRelease(p);` |
|     137 | 1356 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|      68 | 1357 | `	}` |
|       - | 1358 | `	/* Format the given string */` |
|     223 | 1359 | `	va_start(ap,zFormat);` |
|     223 | 1360 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|     223 | 1361 | `	va_end(ap);` |
|     223 | 1362 | `	return rc;` |
|       1 | 1363 |  |
|       - | 1364 | `/*` |
|       - | 1365 | ` * [CAPIREF: ph7_result_value()]` |
|       - | 1366 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1367 | ` */` |
|   22584 | 1368 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|       2 | 1369 |  |
|   22586 | 1370 | `	int rc = PH7_OK;` |
|   22586 | 1371 | `	if( pValue == 0 ){` |
|     ! 0 | 1372 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     ! 0 | 1373 | `	}else{` |
|   22586 | 1374 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|       - | 1375 | `	}` |
|   22586 | 1376 | `	return rc;` |
|       2 | 1377 |  |
|       - | 1378 | `/*` |
|       - | 1379 | ` * [CAPIREF: ph7_result_resource()]` |
|       - | 1380 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1381 | ` */` |
|    3004 | 1382 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|       2 | 1383 |  |
|    3006 | 1384 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|       2 | 1385 |  |
|       - | 1386 | `/*` |
|       - | 1387 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|       - | 1388 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1389 | ` */` |
|    4704 | 1390 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|       2 | 1391 |  |
|       - | 1392 | `	ph7_value *pVal;` |
|    4706 | 1393 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|    4706 | 1394 | `	if( pVal ){` |
|       - | 1395 | `		/* Record value address so it can be freed automatically` |
|       - | 1396 | `		 * when the calling function returns.` |
|       - | 1397 | `		 */` |
|    4706 | 1398 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    2352 | 1399 | `	}` |
|    4706 | 1400 | `	return pVal;` |
|       2 | 1401 |  |
|       - | 1402 | `/*` |
|       - | 1403 | ` * [CAPIREF: ph7_context_new_array()]` |
|       - | 1404 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1405 | ` */` |
|    6786 | 1406 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|       2 | 1407 |  |
|       - | 1408 | `	ph7_value *pVal;` |
|    6788 | 1409 | `	pVal = ph7_new_array(pCtx->pVm);` |
|    6788 | 1410 | `	if( pVal ){` |
|       - | 1411 | `		/* Record value address so it can be freed automatically` |
|       - | 1412 | `		 * when the calling function returns.` |
|       - | 1413 | `		 */` |
|    6788 | 1414 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|    3393 | 1415 | `	}` |
|    6788 | 1416 | `	return pVal;` |
|       2 | 1417 |  |
|       - | 1418 | `/*` |
|       - | 1419 | ` * [CAPIREF: ph7_context_release_value()]` |
|       - | 1420 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1421 | ` */` |
|     248 | 1422 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|       2 | 1423 |  |
|     250 | 1424 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|     250 | 1425 |  |
|       - | 1426 | `/*` |
|       - | 1427 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|       - | 1428 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1429 | ` */` |
|    2970 | 1430 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|       2 | 1431 |  |
|       - | 1432 | `	void *pChunk;` |
|    2972 | 1433 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    2972 | 1434 | `	if( pChunk ){` |
|    2972 | 1435 | `		if( ZeroChunk ){` |
|       - | 1436 | `			/* Zero the memory chunk */` |
|    2938 | 1437 | `			SyZero(pChunk,nByte);` |
|    1468 | 1438 | `		}` |
|    2972 | 1439 | `		if( AutoRelease ){` |
|       - | 1440 | `			ph7_aux_data sAux;` |
|       - | 1441 | `			/* Track the chunk so that it can be released automatically` |
|       - | 1442 | `			 * upon this context is destroyed.` |
|       - | 1443 | `			 */` |
|      25 | 1444 | `			sAux.pAuxData = pChunk;` |
|      25 | 1445 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|      12 | 1446 | `		}` |
|    1485 | 1447 | `	}` |
|    2972 | 1448 | `	return pChunk;` |
|       2 | 1449 |  |
|       - | 1450 | `/*` |
|       - | 1451 | ` * Check if the given chunk address is registered in the call context` |
|       - | 1452 | ` * chunk container.` |
|       - | 1453 | ` * Return TRUE if registered.FALSE otherwise.` |
|       - | 1454 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|       - | 1455 | ` */` |
|    2942 | 1456 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|       2 | 1457 |  |
|       - | 1458 | `	ph7_aux_data *aAux,*pAux;` |
|       - | 1459 | `	sxu32 n;` |
|    2944 | 1460 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|       - | 1461 | `		/* Don't bother processing,the container is empty */` |
|    2944 | 1462 | `		return 0;` |
|       - | 1463 | `	}` |
|       - | 1464 | `	/* Perform the lookup */` |
|     ! 0 | 1465 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     ! 0 | 1466 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     ! 0 | 1467 | `		pAux = &aAux[n];` |
|     ! 0 | 1468 | `		if( pAux->pAuxData == pChunk ){` |
|       - | 1469 | `			/* Chunk found */` |
|     ! 0 | 1470 | `			return pAux;` |
|       - | 1471 | `		}` |
|     ! 0 | 1472 | `	}` |
|       - | 1473 | `	/* No such allocated chunk */` |
|     ! 0 | 1474 | `	return 0;` |
|    1473 | 1475 |  |
|       - | 1476 | `/*` |
|       - | 1477 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|       - | 1478 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1479 | ` */` |
|     ! 0 | 1480 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|     ! 0 | 1481 |  |
|       - | 1482 | `	ph7_aux_data *pAux;` |
|       - | 1483 | `	void *pNew;` |
|     ! 0 | 1484 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|     ! 0 | 1485 | `	if( pNew ){` |
|     ! 0 | 1486 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|     ! 0 | 1487 | `		if( pAux ){` |
|     ! 0 | 1488 | `			pAux->pAuxData = pNew;` |
|     ! 0 | 1489 | `		}` |
|     ! 0 | 1490 | `	}` |
|     ! 0 | 1491 | `	return pNew;` |
|     ! 0 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|       - | 1495 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1496 | ` */` |
|    2942 | 1497 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|       2 | 1498 |  |
|       - | 1499 | `	ph7_aux_data *pAux;` |
|    2944 | 1500 | `	if( pChunk == 0 ){` |
|       - | 1501 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|     ! 0 | 1502 | `		return;` |
|       - | 1503 | `	}` |
|    2944 | 1504 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|    2944 | 1505 | `	if( pAux ){` |
|       - | 1506 | `		/* Mark as destroyed */` |
|     ! 0 | 1507 | `		pAux->pAuxData = 0;` |
|     ! 0 | 1508 | `	}` |
|    2944 | 1509 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    1473 | 1510 |  |
|       - | 1511 | `/*` |
|       - | 1512 | ` * [CAPIREF: ph7_array_fetch()]` |
|       - | 1513 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1514 | ` */` |
|     ! 0 | 1515 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|     ! 0 | 1516 |  |
|       - | 1517 | `	ph7_hashmap_node *pNode;` |
|       - | 1518 | `	ph7_value *pValue;` |
|       - | 1519 | `	ph7_value skey;` |
|       - | 1520 | `	int rc;` |
|       - | 1521 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 1522 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1523 | `		return 0;` |
|       - | 1524 | `	}` |
|     ! 0 | 1525 | `	if( nByte < 0 ){` |
|     ! 0 | 1526 | `		nByte = (int)SyStrlen(zKey);` |
|     ! 0 | 1527 | `	}` |
|       - | 1528 | `	/* Convert the key to a ph7_value  */` |
|     ! 0 | 1529 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|     ! 0 | 1530 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|       - | 1531 | `	/* Perform the lookup */` |
|     ! 0 | 1532 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|     ! 0 | 1533 | `	PH7_MemObjRelease(&skey);` |
|     ! 0 | 1534 | `	if( rc != PH7_OK ){` |
|       - | 1535 | `		/* No such entry */` |
|     ! 0 | 1536 | `		return 0;` |
|       - | 1537 | `	}` |
|       - | 1538 | `	/* Extract the target value */` |
|     ! 0 | 1539 | `	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     ! 0 | 1540 | `	return pValue;` |
|     ! 0 | 1541 |  |
|       - | 1542 | `/*` |
|       - | 1543 | ` * [CAPIREF: ph7_array_walk()]` |
|       - | 1544 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1545 | ` */` |
|   21716 | 1546 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|       2 | 1547 |  |
|       - | 1548 | `	int rc;` |
|   21718 | 1549 | `	if( xWalk == 0 ){` |
|     ! 0 | 1550 | `		return PH7_CORRUPT;` |
|       - | 1551 | `	}` |
|       - | 1552 | `	/* Make sure we are dealing with a valid hashmap */` |
|   21718 | 1553 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1554 | `		return PH7_CORRUPT;` |
|       - | 1555 | `	}` |
|       - | 1556 | `	/* Start the walk process */` |
|   21718 | 1557 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|   21718 | 1558 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|   10860 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * [CAPIREF: ph7_array_add_elem()]` |
|       - | 1562 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1563 | ` */` |
| 2237206 | 1564 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|       2 | 1565 |  |
|       - | 1566 | `	int rc;` |
|       - | 1567 | `	/* Make sure we are dealing with a valid hashmap */` |
| 2237208 | 1568 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1569 | `		return PH7_CORRUPT;` |
|       - | 1570 | `	}` |
|       - | 1571 | `	/* Perform the insertion */` |
| 2237208 | 1572 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
| 2237208 | 1573 | `	return rc;` |
| 1118605 | 1574 |  |
|       - | 1575 | `/*` |
|       - | 1576 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|       - | 1577 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1578 | ` */` |
|    2460 | 1579 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|       2 | 1580 |  |
|       - | 1581 | `	int rc;` |
|       - | 1582 | `	/* Make sure we are dealing with a valid hashmap */` |
|    2462 | 1583 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1584 | `		return PH7_CORRUPT;` |
|       - | 1585 | `	}` |
|       - | 1586 | `	/* Perform the insertion */` |
|    2462 | 1587 | `	if( SX_EMPTY_STR(zKey) ){` |
|       - | 1588 | `		/* Empty key,assign an automatic index */` |
|     ! 0 | 1589 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|     ! 0 | 1590 | `	}else{` |
|       - | 1591 | `		ph7_value sKey;` |
|    2462 | 1592 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|    2462 | 1593 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|    2462 | 1594 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|    2462 | 1595 | `		PH7_MemObjRelease(&sKey);` |
|       - | 1596 | `	}` |
|    2462 | 1597 | `	return rc;` |
|    1232 | 1598 |  |
|       - | 1599 | `/*` |
|       - | 1600 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|       - | 1601 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1602 | ` */` |
|     248 | 1603 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|       1 | 1604 |  |
|       - | 1605 | `	ph7_value sKey;` |
|       - | 1606 | `	int rc;` |
|       - | 1607 | `	/* Make sure we are dealing with a valid hashmap */` |
|     249 | 1608 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1609 | `		return PH7_CORRUPT;` |
|       - | 1610 | `	}` |
|     249 | 1611 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|       - | 1612 | `	/* Perform the insertion */` |
|     249 | 1613 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|     249 | 1614 | `	PH7_MemObjRelease(&sKey);` |
|     249 | 1615 | `	return rc;` |
|     125 | 1616 |  |
|       - | 1617 | `/*` |
|       - | 1618 | ` * [CAPIREF: ph7_array_count()]` |
|       - | 1619 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1620 | ` */` |
|   86128 | 1621 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|       2 | 1622 |  |
|       - | 1623 | `	ph7_hashmap *pMap;` |
|       - | 1624 | `	/* Make sure we are dealing with a valid hashmap */` |
|   86130 | 1625 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 1626 | `		return 0;` |
|       - | 1627 | `	}` |
|       - | 1628 | `	/* Point to the internal representation of the hashmap */` |
|   86130 | 1629 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|   86130 | 1630 | `	return pMap->nEntry;` |
|   43066 | 1631 |  |
|       - | 1632 | `/*` |
|       - | 1633 | ` * [CAPIREF: ph7_object_walk()]` |
|       - | 1634 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1635 | ` */` |
|     ! 0 | 1636 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|     ! 0 | 1637 |  |
|       - | 1638 | `	int rc;` |
|     ! 0 | 1639 | `	if( xWalk == 0 ){` |
|     ! 0 | 1640 | `		return PH7_CORRUPT;` |
|       - | 1641 | `	}` |
|       - | 1642 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1643 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1644 | `		return PH7_CORRUPT;` |
|       - | 1645 | `	}` |
|       - | 1646 | `	/* Start the walk process */` |
|     ! 0 | 1647 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|     ! 0 | 1648 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|     ! 0 | 1649 |  |
|       - | 1650 | `/*` |
|       - | 1651 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|       - | 1652 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1653 | ` */` |
|     ! 0 | 1654 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|     ! 0 | 1655 |  |
|       - | 1656 | `	ph7_value *pValue;` |
|       - | 1657 | `	SyString sAttr;` |
|       - | 1658 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1659 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|     ! 0 | 1660 | `		return 0;` |
|       - | 1661 | `	}` |
|     ! 0 | 1662 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|       - | 1663 | `	/* Extract the attribute value if available.` |
|       - | 1664 | `	 */` |
|     ! 0 | 1665 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|     ! 0 | 1666 | `	return pValue;` |
|     ! 0 | 1667 |  |
|       - | 1668 | `/*` |
|       - | 1669 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|       - | 1670 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1671 | ` */` |
|     ! 0 | 1672 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|     ! 0 | 1673 |  |
|       - | 1674 | `	ph7_class *pClass;` |
|     ! 0 | 1675 | `	if( pLength ){` |
|     ! 0 | 1676 | `		*pLength = 0;` |
|     ! 0 | 1677 | `	}` |
|       - | 1678 | `	/* Make sure we are dealing with a valid class instance */` |
|     ! 0 | 1679 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|     ! 0 | 1680 | `		return 0;` |
|       - | 1681 | `	}` |
|       - | 1682 | `	/* Point to the class */` |
|     ! 0 | 1683 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|       - | 1684 | `	/* Return the class name */` |
|     ! 0 | 1685 | `	if( pLength ){` |
|     ! 0 | 1686 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|     ! 0 | 1687 | `	}` |
|     ! 0 | 1688 | `	return SyStringData(&pClass->sName);` |
|     ! 0 | 1689 |  |
|       - | 1690 | `/*` |
|       - | 1691 | ` * [CAPIREF: ph7_context_output()]` |
|       - | 1692 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1693 | ` */` |
|     350 | 1694 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|       2 | 1695 |  |
|       - | 1696 | `	SyString sData;` |
|       - | 1697 | `	int rc;` |
|     352 | 1698 | `	if( nLen < 0 ){` |
|     ! 0 | 1699 | `		nLen = (int)SyStrlen(zString);` |
|     ! 0 | 1700 | `	}` |
|     352 | 1701 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|     352 | 1702 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|     352 | 1703 | `	return rc;` |
|       2 | 1704 |  |
|       - | 1705 | `/*` |
|       - | 1706 | ` * [CAPIREF: ph7_context_output_format()]` |
|       - | 1707 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1708 | ` */` |
|       2 | 1709 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|       1 | 1710 |  |
|       - | 1711 | `	va_list ap;` |
|       - | 1712 | `	int rc;` |
|       3 | 1713 | `	va_start(ap,zFormat);` |
|       3 | 1714 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       3 | 1715 | `	va_end(ap);` |
|       3 | 1716 | `	return rc;` |
|       1 | 1717 |  |
|       - | 1718 | `/*` |
|       - | 1719 | ` * [CAPIREF: ph7_context_throw_error()]` |
|       - | 1720 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1721 | ` */` |
|      30 | 1722 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|       2 | 1723 |  |
|      32 | 1724 | `	int rc = PH7_OK;` |
|      32 | 1725 | `	if( zErr ){` |
|      32 | 1726 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      15 | 1727 | `	}` |
|      32 | 1728 | `	return rc;` |
|       2 | 1729 |  |
|       - | 1730 | `/*` |
|       - | 1731 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|       - | 1732 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1733 | ` */` |
|      24 | 1734 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|       2 | 1735 |  |
|       - | 1736 | `	va_list ap;` |
|       - | 1737 | `	int rc;` |
|      26 | 1738 | `	if( zFormat == 0){` |
|     ! 0 | 1739 | `		return PH7_OK;` |
|       - | 1740 | `	}` |
|      26 | 1741 | `	va_start(ap,zFormat);` |
|      26 | 1742 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|      26 | 1743 | `	va_end(ap);` |
|      26 | 1744 | `	return rc;` |
|      14 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * [CAPIREF: ph7_context_random_num()]` |
|       - | 1748 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1749 | ` */` |
|      34 | 1750 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|       1 | 1751 |  |
|       - | 1752 | `	sxu32 n;` |
|      35 | 1753 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      35 | 1754 | `	return n;` |
|       1 | 1755 |  |
|       - | 1756 | `/*` |
|       - | 1757 | ` * [CAPIREF: ph7_context_random_string()]` |
|       - | 1758 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1759 | ` */` |
|     ! 0 | 1760 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|     ! 0 | 1761 |  |
|     ! 0 | 1762 | `	if( nBuflen < 3 ){` |
|     ! 0 | 1763 | `		return PH7_CORRUPT;` |
|       - | 1764 | `	}` |
|     ! 0 | 1765 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|     ! 0 | 1766 | `	return PH7_OK;` |
|     ! 0 | 1767 |  |
|       - | 1768 | `/*` |
|       - | 1769 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|       - | 1770 | ` *` |
|       - | 1771 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|       - | 1772 | ` * {` |
|       - | 1773 | ` *	return pCtx->pVm;` |
|       - | 1774 | ` * }` |
|       - | 1775 | ` */` |
|       - | 1776 | `/*` |
|       - | 1777 | ` * [CAPIREF: ph7_context_user_data()]` |
|       - | 1778 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1779 | ` */` |
|   39508 | 1780 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|       2 | 1781 |  |
|   39510 | 1782 | `	return pCtx->pFunc->pUserData;` |
|       2 | 1783 |  |
|       - | 1784 | `/*` |
|       - | 1785 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|       - | 1786 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1787 | ` */` |
|       2 | 1788 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|       1 | 1789 |  |
|       - | 1790 | `	ph7_aux_data sAux;` |
|       - | 1791 | `	int rc;` |
|       3 | 1792 | `	sAux.pAuxData = pUserData;` |
|       3 | 1793 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|       3 | 1794 | `	return rc;` |
|       1 | 1795 |  |
|       - | 1796 | `/*` |
|       - | 1797 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|       - | 1798 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1799 | ` */` |
|       6 | 1800 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|       1 | 1801 |  |
|       - | 1802 | `	ph7_aux_data *pAux;` |
|       7 | 1803 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|       7 | 1804 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1805 |  |
|       - | 1806 | `/*` |
|       - | 1807 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|       - | 1808 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1809 | ` */` |
|       2 | 1810 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|       1 | 1811 |  |
|       - | 1812 | `	ph7_aux_data *pAux;` |
|       3 | 1813 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|       3 | 1814 | `	return pAux ? pAux->pAuxData : 0;` |
|       1 | 1815 |  |
|       - | 1816 | `/*` |
|       - | 1817 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|       - | 1818 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1819 | ` */` |
|    4240 | 1820 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|       2 | 1821 |  |
|    4242 | 1822 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|       2 | 1823 |  |
|       - | 1824 | `/*` |
|       - | 1825 | ` * [CAPIREF: ph7_function_name()]` |
|       - | 1826 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1827 | ` */` |
|   15814 | 1828 | `const char * ph7_function_name(ph7_context *pCtx)` |
|       2 | 1829 |  |
|       - | 1830 | `	SyString *pName;` |
|   15816 | 1831 | `	pName = &pCtx->pFunc->sName;` |
|   15816 | 1832 | `	return pName->zString;` |
|       2 | 1833 |  |
|       - | 1834 | `/*` |
|       - | 1835 | ` * [CAPIREF: ph7_value_int()]` |
|       - | 1836 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1837 | ` */` |
|   17220 | 1838 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|       2 | 1839 |  |
|       - | 1840 | `	/* Invalidate any prior representation */` |
|   17222 | 1841 | `	PH7_MemObjRelease(pVal);` |
|   17222 | 1842 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|   17222 | 1843 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   17222 | 1844 | `	return PH7_OK;` |
|       2 | 1845 |  |
|       - | 1846 | `/*` |
|       - | 1847 | ` * [CAPIREF: ph7_value_int64()]` |
|       - | 1848 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1849 | ` */` |
|   10734 | 1850 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|       2 | 1851 |  |
|       - | 1852 | `	/* Invalidate any prior representation */` |
|   10736 | 1853 | `	PH7_MemObjRelease(pVal);` |
|   10736 | 1854 | `	pVal->x.iVal = iValue;` |
|   10736 | 1855 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|   10736 | 1856 | `	return PH7_OK;` |
|       2 | 1857 |  |
|       - | 1858 | `/*` |
|       - | 1859 | ` * [CAPIREF: ph7_value_bool()]` |
|       - | 1860 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1861 | ` */` |
|  261114 | 1862 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|       2 | 1863 |  |
|       - | 1864 | `	/* Invalidate any prior representation */` |
|  261116 | 1865 | `	PH7_MemObjRelease(pVal);` |
|  261116 | 1866 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|  261116 | 1867 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|  261116 | 1868 | `	return PH7_OK;` |
|       2 | 1869 |  |
|       - | 1870 | `/*` |
|       - | 1871 | ` * [CAPIREF: ph7_value_null()]` |
|       - | 1872 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1873 | ` */` |
|       4 | 1874 | `int ph7_value_null(ph7_value *pVal)` |
|       1 | 1875 |  |
|       - | 1876 | `	/* Invalidate any prior representation and set the NULL flag */` |
|       5 | 1877 | `	PH7_MemObjRelease(pVal);` |
|       5 | 1878 | `	return PH7_OK;` |
|       1 | 1879 |  |
|       - | 1880 | `/*` |
|       - | 1881 | ` * [CAPIREF: ph7_value_double()]` |
|       - | 1882 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1883 | ` */` |
|     426 | 1884 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|       1 | 1885 |  |
|       - | 1886 | `	/* Invalidate any prior representation */` |
|     427 | 1887 | `	PH7_MemObjRelease(pVal);` |
|     427 | 1888 | `	pVal->rVal = (ph7_real)Value;` |
|     427 | 1889 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|       - | 1890 | `	/* Try to get an integer representation also */` |
|     427 | 1891 | `	PH7_MemObjTryInteger(pVal);` |
|     427 | 1892 | `	return PH7_OK;` |
|       1 | 1893 |  |
|       - | 1894 | `/*` |
|       - | 1895 | ` * [CAPIREF: ph7_value_string()]` |
|       - | 1896 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1897 | ` */` |
|  781132 | 1898 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|       2 | 1899 |  |
|  781134 | 1900 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1901 | `		/* Invalidate any prior representation */` |
|  247406 | 1902 | `		PH7_MemObjRelease(pVal);` |
|  247406 | 1903 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  123702 | 1904 | `	}` |
|  781134 | 1905 | `	if( zString ){` |
|  779950 | 1906 | `		if( nLen < 0 ){` |
|       - | 1907 | `			/* Compute length automatically */` |
|    2480 | 1908 | `			nLen = (int)SyStrlen(zString);` |
|    1239 | 1909 | `		}` |
|  779950 | 1910 | `		SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|  389974 | 1911 | `	}` |
|  781134 | 1912 | `	return PH7_OK;` |
|       2 | 1913 |  |
|       - | 1914 | `/*` |
|       - | 1915 | ` * [CAPIREF: ph7_value_string_format()]` |
|       - | 1916 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1917 | ` */` |
|      12 | 1918 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|       1 | 1919 |  |
|       - | 1920 | `	va_list ap;` |
|      13 | 1921 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 1922 | `		/* Invalidate any prior representation */` |
|      13 | 1923 | `		PH7_MemObjRelease(pVal);` |
|      13 | 1924 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|       6 | 1925 | `	}` |
|      13 | 1926 | `	va_start(ap,zFormat);` |
|      13 | 1927 | `	(void)SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|      13 | 1928 | `	va_end(ap);` |
|      13 | 1929 | `	return PH7_OK;` |
|       1 | 1930 |  |
|       - | 1931 | `/*` |
|       - | 1932 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|       - | 1933 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1934 | ` */` |
|   91462 | 1935 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|       2 | 1936 |  |
|       - | 1937 | `	/* Reset the string cursor */` |
|   91464 | 1938 | `	SyBlobReset(&pVal->sBlob);` |
|   91464 | 1939 | `	return PH7_OK;` |
|       2 | 1940 |  |
|       - | 1941 | `/*` |
|       - | 1942 | ` * [CAPIREF: ph7_value_resource()]` |
|       - | 1943 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1944 | ` */` |
|    3094 | 1945 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|       2 | 1946 |  |
|       - | 1947 | `	/* Invalidate any prior representation */` |
|    3096 | 1948 | `	PH7_MemObjRelease(pVal);` |
|       - | 1949 | `	/* Reflect the new type */` |
|    3096 | 1950 | `	pVal->x.pOther = pUserData;` |
|    3096 | 1951 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|    3096 | 1952 | `	return PH7_OK;` |
|       2 | 1953 |  |
|       - | 1954 | `/*` |
|       - | 1955 | ` * [CAPIREF: ph7_value_release()]` |
|       - | 1956 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1957 | ` */` |
|    2056 | 1958 | `int ph7_value_release(ph7_value *pVal)` |
|       2 | 1959 |  |
|    2058 | 1960 | `	PH7_MemObjRelease(pVal);` |
|    2058 | 1961 | `	return PH7_OK;` |
|       2 | 1962 |  |
|       - | 1963 | `/*` |
|       - | 1964 | ` * [CAPIREF: ph7_value_is_int()]` |
|       - | 1965 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1966 | ` */` |
|    8282 | 1967 | `int ph7_value_is_int(ph7_value *pVal)` |
|       2 | 1968 |  |
|    8284 | 1969 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|       2 | 1970 |  |
|       - | 1971 | `/*` |
|       - | 1972 | ` * [CAPIREF: ph7_value_is_float()]` |
|       - | 1973 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1974 | ` */` |
|     614 | 1975 | `int ph7_value_is_float(ph7_value *pVal)` |
|       2 | 1976 |  |
|     616 | 1977 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|       2 | 1978 |  |
|       - | 1979 | `/*` |
|       - | 1980 | ` * [CAPIREF: ph7_value_is_bool()]` |
|       - | 1981 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1982 | ` */` |
|     218 | 1983 | `int ph7_value_is_bool(ph7_value *pVal)` |
|       2 | 1984 |  |
|     220 | 1985 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|       2 | 1986 |  |
|       - | 1987 | `/*` |
|       - | 1988 | ` * [CAPIREF: ph7_value_is_string()]` |
|       - | 1989 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1990 | ` */` |
|   67422 | 1991 | `int ph7_value_is_string(ph7_value *pVal)` |
|       2 | 1992 |  |
|   67424 | 1993 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|       2 | 1994 |  |
|       - | 1995 | `/*` |
|       - | 1996 | ` * [CAPIREF: ph7_value_is_null()]` |
|       - | 1997 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 1998 | ` */` |
|     940 | 1999 | `int ph7_value_is_null(ph7_value *pVal)` |
|       2 | 2000 |  |
|     942 | 2001 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|       2 | 2002 |  |
|       - | 2003 | `/*` |
|       - | 2004 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|       - | 2005 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2006 | ` */` |
|      58 | 2007 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|       2 | 2008 |  |
|       - | 2009 | `	int rc;` |
|      60 | 2010 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|      60 | 2011 | `	return rc;` |
|       2 | 2012 |  |
|       - | 2013 | `/*` |
|       - | 2014 | ` * [CAPIREF: ph7_value_is_callable()]` |
|       - | 2015 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2016 | ` */` |
|   15746 | 2017 | `int ph7_value_is_callable(ph7_value *pVal)` |
|       2 | 2018 |  |
|       - | 2019 | `	int rc;` |
|   15748 | 2020 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|   15748 | 2021 | `	return rc;` |
|       2 | 2022 |  |
|       - | 2023 | `/*` |
|       - | 2024 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|       - | 2025 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2026 | ` */` |
|      12 | 2027 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|       1 | 2028 |  |
|      13 | 2029 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|       1 | 2030 |  |
|       - | 2031 | `/*` |
|       - | 2032 | ` * [CAPIREF: ph7_value_is_array()]` |
|       - | 2033 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2034 | ` */` |
|  101916 | 2035 | `int ph7_value_is_array(ph7_value *pVal)` |
|       2 | 2036 |  |
|  101918 | 2037 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|       2 | 2038 |  |
|       - | 2039 | `/*` |
|       - | 2040 | ` * [CAPIREF: ph7_value_is_object()]` |
|       - | 2041 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2042 | ` */` |
|     794 | 2043 | `int ph7_value_is_object(ph7_value *pVal)` |
|       2 | 2044 |  |
|     796 | 2045 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|       2 | 2046 |  |
|       - | 2047 | `/*` |
|       - | 2048 | ` * [CAPIREF: ph7_value_is_resource()]` |
|       - | 2049 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2050 | ` */` |
|   17840 | 2051 | `int ph7_value_is_resource(ph7_value *pVal)` |
|       2 | 2052 |  |
|   17842 | 2053 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|       2 | 2054 |  |
|       - | 2055 | `/*` |
|       - | 2056 | ` * [CAPIREF: ph7_value_is_empty()]` |
|       - | 2057 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|       - | 2058 | ` */` |
|   18984 | 2059 | `int ph7_value_is_empty(ph7_value *pVal)` |
|       2 | 2060 |  |
|       - | 2061 | `	int rc;` |
|   18986 | 2062 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|   18986 | 2063 | `	return rc;` |
|       2 | 2064 |  |
|       - | 2065 |  |
