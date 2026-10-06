# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 851/1215 lines (70.04%)

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
|    37590 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|        5 |   79 | `{` |
|    37595 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    37595 |   81 | `	int rc = PH7_OK;` |
|        - |   82 | `	/* Perform the requested operation */` |
|    37595 |   83 | `	switch(nOp){` |
|     8450 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    16883 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    16883 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|        - |   87 | `		/* Compile time error consumer routine */` |
|    16883 |   88 | `		if( xConsumer == 0 ){` |
|      ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|      ! 0 |   90 | `			break;` |
|        - |   91 | `		}` |
|        - |   92 | `		/* Install the error consumer */` |
|    16883 |   93 | `		pConf->xErr     = xConsumer;` |
|    16883 |   94 | `		pConf->pErrData = pUserData;` |
|    16883 |   95 | `		break;` |
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
|     4217 |  145 | `	case PH7_CONFIG_OUTPUT: {` |
|        - |  146 | `		/* The program-output stream a compile diagnostic's DISPLAY copy takes` |
|        - |  147 | `		 * while no VM output consumer exists yet -- the whole of the main` |
|        - |  148 | `		 * script's own compile. */` |
|     8428 |  149 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     8428 |  150 | `		void *pUserData = va_arg(ap,void *);` |
|     8428 |  151 | `		pConf->xOut     = xConsumer;` |
|     8428 |  152 | `		pConf->pOutData = pUserData;` |
|     8428 |  153 | `		break;` |
|        - |  154 | `							}` |
|     4217 |  155 | `	case PH7_CONFIG_ERR_REPORT:` |
|        - |  156 | `		/* Seed the reporting level of VMs created afterwards. The VM-level verb` |
|        - |  157 | `		 * of the same name can only reach a VM that already exists, i.e. after` |
|        - |  158 | `		 * the unit it was meant to gate has finished compiling. */` |
|     8428 |  159 | `		pConf->bErrReport = 1;` |
|     8428 |  160 | `		break;` |
|      257 |  161 | `	case PH7_CONFIG_INI_FILE: {` |
|        - |  162 | `		/* Which php.ini file the host actually read, under the name php quotes` |
|        - |  163 | `		 * for it. The host resolves it; the engine only remembers the string, so` |
|        - |  164 | `		 * php_ini_loaded_file() can answer with it. Copied onto the ENGINE` |
|        - |  165 | `		 * allocator: it outlives every VM that asks. */` |
|      514 |  166 | `		const char *zPath = va_arg(ap,const char *);` |
|      514 |  167 | `		sxu32 nPath = zPath ? (sxu32)SyStrlen(zPath) : 0;` |
|        - |  168 | `		char *zDup;` |
|      514 |  169 | `		if( nPath < 1 ){` |
|      ! 0 |  170 | `			SyStringInitFromBuf(&pConf->sIniFile,0,0);` |
|      ! 0 |  171 | `			break;` |
|        - |  172 | `		}` |
|      514 |  173 | `		zDup = SyMemBackendStrDup(&pEngine->sAllocator,zPath,nPath);` |
|      514 |  174 | `		if( zDup == 0 ){` |
|      ! 0 |  175 | `			rc = PH7_NOMEM;` |
|      ! 0 |  176 | `			break;` |
|        - |  177 | `		}` |
|      514 |  178 | `		SyStringInitFromBuf(&pConf->sIniFile,zDup,nPath);` |
|      514 |  179 | `		break;` |
|        - |  180 | `							  }` |
|     1677 |  181 | `	case PH7_CONFIG_INI_ENTRY: {` |
|        - |  182 | `		/* A php.ini directive applied to every VM at birth. php reads php.ini` |
|        - |  183 | `		 * before it compiles anything, so a directive a diagnostic is gated by` |
|        - |  184 | `		 * (display_errors, log_errors, error_reporting) has to be in hand before` |
|        - |  185 | `		 * ph7_compile_file -- which is the call that CREATES the VM. Copies live` |
|        - |  186 | `		 * on the ENGINE allocator: they outlive every VM replaying them. */` |
|     3356 |  187 | `		const char *zName = va_arg(ap,const char *);` |
|     3356 |  188 | `		const char *zValue = va_arg(ap,const char *);` |
|     3356 |  189 | `		const char *zFile = va_arg(ap,const char *);` |
|     3356 |  190 | `		unsigned int nLine = va_arg(ap,unsigned int);` |
|     3356 |  191 | `		int iStop = va_arg(ap,int);` |
|        - |  192 | `		VmIniEntry sEntry;` |
|        - |  193 | `		char *zDupN,*zDupV,*zDupF;` |
|        - |  194 | `		sxu32 nName,nValue,nFile;` |
|        - |  195 | ``		/* An unclosed `[` is queued in place of a directive and carries no name:`` |
|        - |  196 | `		 * it is the source refusing itself, not an entry to apply. */` |
|     3356 |  197 | `		if( SX_EMPTY_STR(zName) && iStop < PH7_INI_STOP_SECTION ){` |
|      ! 0 |  198 | `			rc = PH7_CORRUPT;` |
|      ! 0 |  199 | `			break;` |
|        - |  200 | `		}` |
|     3356 |  201 | `		if( zName == 0 ){` |
|      ! 0 |  202 | `			zName = "";` |
|      ! 0 |  203 | `		}` |
|     3356 |  204 | `		if( zValue == 0 ){` |
|      ! 0 |  205 | `			zValue = "";` |
|      ! 0 |  206 | `		}` |
|     3356 |  207 | `		if( zFile == 0 ){` |
|      ! 0 |  208 | `			zFile = "";` |
|      ! 0 |  209 | `		}` |
|     3356 |  210 | `		nName  = (sxu32)SyStrlen(zName);` |
|     3356 |  211 | `		nValue = (sxu32)SyStrlen(zValue);` |
|     3356 |  212 | `		nFile  = (sxu32)SyStrlen(zFile);` |
|     3356 |  213 | `		zDupN = SyMemBackendStrDup(&pEngine->sAllocator,zName,nName);` |
|     3356 |  214 | `		zDupV = SyMemBackendStrDup(&pEngine->sAllocator,zValue,nValue);` |
|     3356 |  215 | `		zDupF = nFile > 0 ? SyMemBackendStrDup(&pEngine->sAllocator,zFile,nFile) : 0;` |
|     3356 |  216 | `		if( zDupN == 0 \|\| zDupV == 0 \|\| (nFile > 0 && zDupF == 0) ){` |
|      ! 0 |  217 | `			rc = PH7_NOMEM;` |
|      ! 0 |  218 | `			break;` |
|        - |  219 | `		}` |
|     3356 |  220 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|     3356 |  221 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|     3356 |  222 | `		SyStringInitFromBuf(&sEntry.sFile,zDupF,nFile);` |
|     3356 |  223 | `		sEntry.nLine = (sxu32)nLine;` |
|     3356 |  224 | `		sEntry.iStop = iStop;` |
|     3356 |  225 | `		if( SySetPut(&pConf->aIniEntry,(const void *)&sEntry) != SXRET_OK ){` |
|      ! 0 |  226 | `			rc = PH7_NOMEM;` |
|      ! 0 |  227 | `		}` |
|     3356 |  228 | `		break;` |
|        - |  229 | `								}` |
|      ! 0 |  230 | `	default:` |
|        - |  231 | `		/* Unknown configuration verb */` |
|      ! 0 |  232 | `		rc = PH7_CORRUPT;` |
|      ! 0 |  233 | `		break;` |
|        - |  234 | `	} /* Switch() */` |
|    37595 |  235 | `	return rc;` |
|        5 |  236 | `}` |
|        - |  237 | `/*` |
|        - |  238 | ` * Configure the PH7 library.` |
|        - |  239 | ` * return PH7_OK on success.Any other return value` |
|        - |  240 | ` * indicates failure.` |
|        - |  241 | ` * Refer to [ph7_lib_config()].` |
|        - |  242 | ` */` |
|    25365 |  243 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|        5 |  244 | `{` |
|    25370 |  245 | `	int rc = PH7_OK;` |
|    25370 |  246 | `	switch(nOp){` |
|     4233 |  247 | `	    case PH7_LIB_CONFIG_VFS:{` |
|        - |  248 | `			/* Install a virtual file system */` |
|     8460 |  249 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|     8460 |  250 | `			sMPGlobal.pVfs = pVfs;` |
|     8460 |  251 | `			break;` |
|        - |  252 | `								}` |
|     4233 |  253 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|        - |  254 | `			/* Use an alternative low-level memory allocation routines */` |
|     8460 |  255 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|        - |  256 | `			/* Save the memory failure callback (if available) */` |
|     8460 |  257 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|     8460 |  258 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|     8460 |  259 | `			if( pMethods == 0 ){` |
|        - |  260 | `				/* Use the built-in memory allocation subsystem */` |
|     8460 |  261 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|     4227 |  262 | `			}else{` |
|      ! 0 |  263 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|        - |  264 | `			}` |
|     8460 |  265 | `			break;` |
|        - |  266 | `										  }` |
|      ! 0 |  267 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|        - |  268 | `			/* Memory failure callback */` |
|      ! 0 |  269 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|      ! 0 |  270 | `			void *pUserData = va_arg(ap,void *);` |
|      ! 0 |  271 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|      ! 0 |  272 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|      ! 0 |  273 | `			break;` |
|        - |  274 | `												 }` |
|     4233 |  275 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|        - |  276 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  277 | `			/* Use an alternative low-level mutex subsystem */` |
|     8460 |  278 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|        - |  279 | `#if defined (UNTRUST)` |
|        - |  280 | `			if( pMethods == 0 ){` |
|        - |  281 | `				rc = PH7_CORRUPT;` |
|        - |  282 | `			}` |
|        - |  283 | `#endif` |
|        - |  284 | `			/* Sanity check */` |
|     8460 |  285 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|        - |  286 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|      ! 0 |  287 | `				rc = PH7_CORRUPT;` |
|      ! 0 |  288 | `				break;` |
|        - |  289 | `			}` |
|     8460 |  290 | `			if( sMPGlobal.pMutexMethods ){` |
|        - |  291 | `				/* Overwrite the previous mutex subsystem */` |
|      ! 0 |  292 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|      ! 0 |  293 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|      ! 0 |  294 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|      ! 0 |  295 | `				}` |
|      ! 0 |  296 | `				sMPGlobal.pMutex = 0;` |
|      ! 0 |  297 | `			}` |
|        - |  298 | `			/* Initialize and install the new mutex subsystem */` |
|     8460 |  299 | `			if( pMethods->xGlobalInit ){` |
|        5 |  300 | `				rc = pMethods->xGlobalInit();` |
|        5 |  301 | `				if ( rc != PH7_OK ){` |
|      ! 0 |  302 | `					break;` |
|        - |  303 | `				}` |
|      ! 0 |  304 | `			}` |
|        - |  305 | `			/* Create the global mutex */` |
|     8460 |  306 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|     8460 |  307 | `			if( sMPGlobal.pMutex == 0 ){` |
|        - |  308 | `				/*` |
|        - |  309 | `				 * If the supplied mutex subsystem is so sick that we are unable to` |
|        - |  310 | `				 * create a single mutex,there is no much we can do here.` |
|        - |  311 | `				 */` |
|      ! 0 |  312 | `				if( pMethods->xGlobalRelease ){` |
|      ! 0 |  313 | `					pMethods->xGlobalRelease();` |
|      ! 0 |  314 | `				}` |
|      ! 0 |  315 | `				rc = PH7_CORRUPT;` |
|      ! 0 |  316 | `				break;` |
|        - |  317 | `			}` |
|     8460 |  318 | `			sMPGlobal.pMutexMethods = pMethods;` |
|     8460 |  319 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|        - |  320 | `				/* Set a default threading level */` |
|     8460 |  321 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|     4222 |  322 | `			}` |
|        - |  323 | `#endif` |
|     8460 |  324 | `			break;` |
|        - |  325 | `										   }` |
|      ! 0 |  326 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:` |
|        - |  327 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  328 | `			/* Single thread mode(Only one thread is allowed to play with the library) */` |
|      ! 0 |  329 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|        - |  330 | `#endif` |
|      ! 0 |  331 | `			break;` |
|      ! 0 |  332 | `		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:` |
|        - |  333 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  334 | `			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines` |
|        - |  335 | `			 * may be shared between multiple threads).` |
|        - |  336 | `			 */` |
|      ! 0 |  337 | `			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|        - |  338 | `#endif` |
|      ! 0 |  339 | `			break;` |
|      ! 0 |  340 | `		default:` |
|        - |  341 | `			/* Unknown configuration option */` |
|      ! 0 |  342 | `			rc = PH7_CORRUPT;` |
|      ! 0 |  343 | `			break;` |
|        - |  344 | `	}` |
|    25370 |  345 | `	return rc;` |
|        5 |  346 | `}` |
|        - |  347 | `/*` |
|        - |  348 | ` * [CAPIREF: ph7_lib_config()]` |
|        - |  349 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  350 | ` */` |
|    25365 |  351 | `int ph7_lib_config(int nConfigOp,...)` |
|        5 |  352 | `{` |
|        - |  353 | `	va_list ap;` |
|        - |  354 | `	int rc;` |
|        - |  355 |  |
|    25370 |  356 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|        - |  357 | `		/* Library is already initialized,this operation is forbidden */` |
|      ! 0 |  358 | `		return PH7_LOOKED;` |
|        - |  359 | `	}` |
|    25370 |  360 | `	va_start(ap,nConfigOp);` |
|    25370 |  361 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    25370 |  362 | `	va_end(ap);` |
|    25370 |  363 | `	return rc;` |
|    12671 |  364 | `}` |
|        - |  365 | `/*` |
|        - |  366 | ` * Global library initialization` |
|        - |  367 | ` * Refer to [ph7_lib_init()]` |
|        - |  368 | ` * This routine must be called to initialize the memory allocation subsystem,the mutex` |
|        - |  369 | ` * subsystem prior to doing any serious work with the library.The first thread to call` |
|        - |  370 | ` * this routine does the initialization process and set the magic number so no body later` |
|        - |  371 | ` * can re-initialize the library.If subsequent threads call this  routine before the first` |
|        - |  372 | ` * thread have finished the initialization process, then the subsequent threads must block` |
|        - |  373 | ` * until the initialization process is done.` |
|        - |  374 | ` */` |
|     8455 |  375 | `static sxi32 PH7CoreInitialize(void)` |
|        5 |  376 | `{` |
|        - |  377 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|        - |  378 | `#if defined(PH7_ENABLE_THREADS)` |
|     8460 |  379 | `	const SyMutexMethods *pMutexMethods = 0;` |
|     8460 |  380 | `	SyMutex *pMaster = 0;` |
|        - |  381 | `#endif` |
|        - |  382 | `	int rc;` |
|        - |  383 | `	/*` |
|        - |  384 | `	 * If the library is already initialized,then a call to this routine` |
|        - |  385 | `	 * is a no-op.` |
|        - |  386 | `	 */` |
|     8460 |  387 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|      ! 0 |  388 | `		return PH7_OK; /* Already initialized */` |
|        - |  389 | `	}` |
|        - |  390 | `	/* Point to the built-in vfs */` |
|     8460 |  391 | `	pVfs = PH7_ExportBuiltinVfs();` |
|        - |  392 | `	/* Install it */` |
|     8460 |  393 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|        - |  394 | `#if defined(PH7_ENABLE_THREADS)` |
|     8460 |  395 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|     8460 |  396 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|     8460 |  397 | `		if( pMutexMethods == 0 ){` |
|        - |  398 | `			/* Use the built-in mutex subsystem */` |
|     8460 |  399 | `			pMutexMethods = SyMutexExportMethods();` |
|     8460 |  400 | `			if( pMutexMethods == 0 ){` |
|      ! 0 |  401 | `				return PH7_CORRUPT; /* Can't happen */` |
|        - |  402 | `			}` |
|        - |  403 | `			/* Install the mutex subsystem */` |
|     8460 |  404 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|     8460 |  405 | `			if( rc != PH7_OK ){` |
|      ! 0 |  406 | `				return rc;` |
|        - |  407 | `			}` |
|     4222 |  408 | `		}` |
|        - |  409 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|     8460 |  410 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|     8460 |  411 | `		if( pMaster == 0 ){` |
|      ! 0 |  412 | `			return PH7_CORRUPT; /* Can't happen */` |
|        - |  413 | `		}` |
|     4222 |  414 | `	}` |
|        - |  415 | `	/* Lock the master mutex */` |
|     8460 |  416 | `	rc = PH7_OK;` |
|     8460 |  417 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    12682 |  418 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|        - |  419 | `#endif` |
|     8460 |  420 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|        - |  421 | `			/* Install a memory subsystem */` |
|     8460 |  422 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|     8460 |  423 | `			if( rc != PH7_OK ){` |
|        - |  424 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|      ! 0 |  425 | `				goto End;` |
|        - |  426 | `			}` |
|     4222 |  427 | `		}` |
|        - |  428 | `#if defined(PH7_ENABLE_THREADS)` |
|     8460 |  429 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  430 | `			/* Protect the memory allocation subsystem */` |
|     8460 |  431 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|     8460 |  432 | `			if( rc != PH7_OK ){` |
|      ! 0 |  433 | `				goto End;` |
|        - |  434 | `			}` |
|     4222 |  435 | `		}` |
|        - |  436 | `#endif` |
|        - |  437 | `		/* Our library is initialized,set the magic number */` |
|     8460 |  438 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|     8460 |  439 | `		rc = PH7_OK;` |
|        - |  440 | `#if defined(PH7_ENABLE_THREADS)` |
|     4222 |  441 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|        - |  442 | `#endif` |
|      ! 0 |  443 | `End:` |
|        - |  444 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  445 | `	/* Unlock the master mutex */` |
|     8460 |  446 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  447 | `#endif` |
|     8460 |  448 | `	return rc;` |
|     4227 |  449 | `}` |
|        - |  450 | `/*` |
|        - |  451 | ` * [CAPIREF: ph7_lib_init()]` |
|        - |  452 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  453 | ` */` |
|      ! 0 |  454 | `int ph7_lib_init(void)` |
|      ! 0 |  455 | `{` |
|        - |  456 | `	int rc;` |
|      ! 0 |  457 | `	rc = PH7CoreInitialize();` |
|      ! 0 |  458 | `	return rc;` |
|      ! 0 |  459 | `}` |
|        - |  460 | `/*` |
|        - |  461 | ` * Release an active PH7 engine and it's associated active virtual machines.` |
|        - |  462 | ` */` |
|     8477 |  463 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|        5 |  464 | `{` |
|        - |  465 | `	ph7_vm *pVm,*pNext;` |
|        - |  466 | `	/* Release all active VM */` |
|     8482 |  467 | `	pVm = pEngine->pVms;` |
|     4233 |  468 | `	for(;;){` |
|     8482 |  469 | `		if( pEngine->iVm <= 0 ){` |
|     8482 |  470 | `			break;` |
|        - |  471 | `		}` |
|      ! 0 |  472 | `		pNext = pVm->pNext;` |
|      ! 0 |  473 | `		PH7_VmRelease(pVm);` |
|      ! 0 |  474 | `		pVm = pNext;` |
|      ! 0 |  475 | `		pEngine->iVm--;` |
|      ! 0 |  476 | `	}` |
|        - |  477 | `	/* Set a dummy magic number */` |
|     8482 |  478 | `	pEngine->nMagic = 0x7635;` |
|        - |  479 | `	/* Release the private memory subsystem */` |
|     8482 |  480 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     8482 |  481 | `	return PH7_OK;` |
|        5 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * Release all resources consumed by the library.` |
|        - |  485 | ` * If PH7 is already shut down when this routine` |
|        - |  486 | ` * is invoked then this routine is a harmless no-op.` |
|        - |  487 | ` * Note: This call is not thread safe.` |
|        - |  488 | ` * Refer to [ph7_lib_shutdown()].` |
|        - |  489 | ` */` |
|     1246 |  490 | `static void PH7CoreShutdown(void)` |
|        4 |  491 | `{` |
|        - |  492 | `	ph7 *pEngine,*pNext;` |
|        - |  493 | `	/* Release all active engines first */` |
|     1250 |  494 | `	pEngine = sMPGlobal.pEngines;` |
|      623 |  495 | `	for(;;){` |
|     1250 |  496 | `		if( sMPGlobal.nEngine < 1 ){` |
|     1250 |  497 | `			break;` |
|        - |  498 | `		}` |
|      ! 0 |  499 | `		pNext = pEngine->pNext;` |
|      ! 0 |  500 | `		EngineRelease(pEngine);` |
|      ! 0 |  501 | `		pEngine = pNext;` |
|      ! 0 |  502 | `		sMPGlobal.nEngine--;` |
|      ! 0 |  503 | `	}` |
|        - |  504 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  505 | `	/* Release the mutex subsystem */` |
|     1250 |  506 | `	if( sMPGlobal.pMutexMethods ){` |
|     1250 |  507 | `		if( sMPGlobal.pMutex ){` |
|     1250 |  508 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     1250 |  509 | `			sMPGlobal.pMutex = 0;` |
|      623 |  510 | `		}` |
|     1250 |  511 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|        4 |  512 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|      ! 0 |  513 | `		}` |
|     1250 |  514 | `		sMPGlobal.pMutexMethods = 0;` |
|      623 |  515 | `	}` |
|     1250 |  516 | `	sMPGlobal.nThreadingLevel = 0;` |
|        - |  517 | `#endif` |
|     1250 |  518 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|        - |  519 | `		/* Release the memory backend */` |
|     1250 |  520 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|      623 |  521 | `	}` |
|     1250 |  522 | `	sMPGlobal.nMagic = 0x1928;` |
|     1250 |  523 | `}` |
|        - |  524 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  525 | `/*` |
|        - |  526 | ` * Drop THIS process to single-threaded mode, called in the CHILD of a fork().` |
|        - |  527 | ` *` |
|        - |  528 | ` * Only one thread survives fork(), and every mutex the library holds is a` |
|        - |  529 | ` * pthread RECURSIVE one whose owner field still names the PARENT's thread id.` |
|        - |  530 | ` * The child's single thread is therefore not the owner of anything -- so the` |
|        - |  531 | ` * first SyMutexEnter it reaches (the one ph7_vm_config takes, which every exit` |
|        - |  532 | ` * path goes through) blocks on a futex nobody will ever post. A forked child` |
|        - |  533 | ` * that merely called exit() hung forever, which is what made this necessary.` |
|        - |  534 | ` *` |
|        - |  535 | ` * The fix is to RE-INITIALIZE every one of them, which is exactly what` |
|        - |  536 | ` * pthread_atfork's child handler exists for and the only defined way out:` |
|        - |  537 | ` * destroying a locked mutex is undefined, and unlocking one this thread does` |
|        - |  538 | ` * not own does nothing. After this nobody holds anything and the child's single` |
|        - |  539 | ` * thread can lock and unlock normally; a balancing SyMutexLeave the interrupted` |
|        - |  540 | ` * call still owes answers EPERM and changes nothing, which is the right answer.` |
|        - |  541 | ` *` |
|        - |  542 | ` * An embedder that installed its OWN mutex subsystem is a different matter --` |
|        - |  543 | ` * this cannot know what its locks are made of. There the mutex POINTERS are` |
|        - |  544 | ` * dropped instead (SyMutexEnter/Leave are no-ops on a null one) and the level` |
|        - |  545 | ` * goes to SINGLE, which is safe for the same reason: a child has one thread` |
|        - |  546 | ` * until it makes another, so the locks protect nothing.` |
|        - |  547 | ` */` |
|       16 |  548 | `PH7_PRIVATE void PH7_LibForkChild(void)` |
|      ! 0 |  549 | `{` |
|        - |  550 | `	ph7 *pEngine;` |
|        - |  551 | `	sxi32 i;` |
|        - |  552 | `	int bReset;` |
|       16 |  553 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC` |
|       16 |  554 | `	 \|\| sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE ){` |
|      ! 0 |  555 | `		return;` |
|        - |  556 | `	}` |
|        - |  557 | `	/* One probe decides which of the two answers this build gets. */` |
|       16 |  558 | `	bReset = SyMutexResetAfterFork(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|       16 |  559 | `	pEngine = sMPGlobal.pEngines;` |
|       32 |  560 | `	for( i = 0 ; i < sMPGlobal.nEngine && pEngine ; ++i ){` |
|       16 |  561 | `		ph7_vm *pVm = pEngine->pVms;` |
|        - |  562 | `		sxi32 j;` |
|       32 |  563 | `		for( j = 0 ; j < pEngine->iVm && pVm ; ++j ){` |
|       16 |  564 | `			if( bReset ){` |
|       16 |  565 | `				SyMutexResetAfterFork(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|        8 |  566 | `			}else{` |
|      ! 0 |  567 | `				pVm->pMutex = 0;` |
|        - |  568 | `			}` |
|       16 |  569 | `			pVm = pVm->pNext;` |
|        8 |  570 | `		}` |
|       16 |  571 | `		if( bReset ){` |
|       16 |  572 | `			SyMutexResetAfterFork(sMPGlobal.pMutexMethods,pEngine->pMutex);` |
|       24 |  573 | `			SyMutexResetAfterFork(pEngine->sAllocator.pMutexMethods,` |
|        8 |  574 | `				pEngine->sAllocator.pMutex);` |
|        8 |  575 | `		}else{` |
|      ! 0 |  576 | `			pEngine->pMutex = 0;` |
|      ! 0 |  577 | `			pEngine->sAllocator.pMutex = 0;` |
|      ! 0 |  578 | `			pEngine->sAllocator.pMutexMethods = 0;` |
|        - |  579 | `		}` |
|       16 |  580 | `		pEngine = pEngine->pNext;` |
|        8 |  581 | `	}` |
|       16 |  582 | `	if( bReset ){` |
|       24 |  583 | `		SyMutexResetAfterFork(sMPGlobal.sAllocator.pMutexMethods,` |
|        8 |  584 | `			sMPGlobal.sAllocator.pMutex);` |
|       16 |  585 | `		return;` |
|        - |  586 | `	}` |
|      ! 0 |  587 | `	sMPGlobal.pMutex = 0;` |
|      ! 0 |  588 | `	sMPGlobal.sAllocator.pMutex = 0;` |
|      ! 0 |  589 | `	sMPGlobal.sAllocator.pMutexMethods = 0;` |
|      ! 0 |  590 | `	sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;` |
|        8 |  591 | `}` |
|        - |  592 | `#endif /* PH7_ENABLE_THREADS */` |
|        - |  593 | `/*` |
|        - |  594 | ` * [CAPIREF: ph7_lib_shutdown()]` |
|        - |  595 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  596 | ` */` |
|     1246 |  597 | `int ph7_lib_shutdown(void)` |
|        4 |  598 | `{` |
|     1250 |  599 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|        - |  600 | `		/* Already shut */` |
|      ! 0 |  601 | `		return PH7_OK;` |
|        - |  602 | `	}` |
|     1250 |  603 | `	PH7CoreShutdown();` |
|     1250 |  604 | `	return PH7_OK;` |
|      627 |  605 | `}` |
|        - |  606 | `/*` |
|        - |  607 | ` * [CAPIREF: ph7_lib_is_threadsafe()]` |
|        - |  608 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  609 | ` */` |
|      ! 0 |  610 | `int ph7_lib_is_threadsafe(void)` |
|      ! 0 |  611 | `{` |
|      ! 0 |  612 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|      ! 0 |  613 | `		return 0;` |
|        - |  614 | `	}` |
|        - |  615 | `#if defined(PH7_ENABLE_THREADS)` |
|      ! 0 |  616 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  617 | `			/* Muli-threading support is enabled */` |
|      ! 0 |  618 | `			return 1;` |
|      ! 0 |  619 | `		}else{` |
|        - |  620 | `			/* Single-threading */` |
|      ! 0 |  621 | `			return 0;` |
|        - |  622 | `		}` |
|        - |  623 | `#else` |
|        - |  624 | `	return 0;` |
|        - |  625 | `#endif` |
|      ! 0 |  626 | `}` |
|        - |  627 | `/*` |
|        - |  628 | ` * [CAPIREF: ph7_lib_version()]` |
|        - |  629 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  630 | ` */` |
|        2 |  631 | `const char * ph7_lib_version(void)` |
|        1 |  632 | `{` |
|        3 |  633 | `	return PH7_VERSION;` |
|        1 |  634 | `}` |
|        - |  635 | `/*` |
|        - |  636 | ` * [CAPIREF: ph7_lib_signature()]` |
|        - |  637 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  638 | ` */` |
|      256 |  639 | `const char * ph7_lib_signature(void)` |
|        5 |  640 | `{` |
|      261 |  641 | `	return PH7_SIG;` |
|        5 |  642 | `}` |
|        - |  643 | `/*` |
|        - |  644 | ` * [CAPIREF: ph7_lib_ident()]` |
|        - |  645 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  646 | ` */` |
|        2 |  647 | `const char * ph7_lib_ident(void)` |
|        1 |  648 | `{` |
|        3 |  649 | `	return PH7_IDENT;` |
|        1 |  650 | `}` |
|        - |  651 | `/*` |
|        - |  652 | ` * [CAPIREF: ph7_lib_copyright()]` |
|        - |  653 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  654 | ` */` |
|      ! 0 |  655 | `const char * ph7_lib_copyright(void)` |
|      ! 0 |  656 | `{` |
|      ! 0 |  657 | `	return PH7_COPYRIGHT;` |
|      ! 0 |  658 | `}` |
|        - |  659 | `/*` |
|        - |  660 | ` * [CAPIREF: ph7_config()]` |
|        - |  661 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  662 | ` */` |
|    37590 |  663 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|        5 |  664 | `{` |
|        - |  665 | `	va_list ap;` |
|        - |  666 | `	int rc;` |
|    37595 |  667 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|      ! 0 |  668 | `		return PH7_CORRUPT;` |
|        - |  669 | `	}` |
|        - |  670 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  671 | `	 /* Acquire engine mutex */` |
|    37595 |  672 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    37595 |  673 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    37590 |  674 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  675 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  676 | `	 }` |
|        - |  677 | `#endif` |
|    37595 |  678 | `	 va_start(ap,nConfigOp);` |
|    37595 |  679 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    37595 |  680 | `	 va_end(ap);` |
|        - |  681 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  682 | `	 /* Leave engine mutex */` |
|    37595 |  683 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  684 | `#endif` |
|    37595 |  685 | `	return rc;` |
|    18777 |  686 | `}` |
|        - |  687 | `/*` |
|        - |  688 | ` * [CAPIREF: ph7_init()]` |
|        - |  689 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  690 | ` */` |
|     8455 |  691 | `int ph7_init(ph7 **ppEngine)` |
|        5 |  692 | `{` |
|        - |  693 | `	ph7 *pEngine;` |
|        - |  694 | `	int rc;` |
|        - |  695 | `#if defined(UNTRUST)` |
|        - |  696 | `	if( ppEngine == 0 ){` |
|        - |  697 | `		return PH7_CORRUPT;` |
|        - |  698 | `	}` |
|        - |  699 | `#endif` |
|     8460 |  700 | `	*ppEngine = 0;` |
|        - |  701 | `	/* One-time automatic library initialization */` |
|     8460 |  702 | `	rc = PH7CoreInitialize();` |
|     8460 |  703 | `	if( rc != PH7_OK ){` |
|      ! 0 |  704 | `		return rc;` |
|        - |  705 | `	}` |
|        - |  706 | `	/* Allocate a new engine */` |
|     8460 |  707 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|     8460 |  708 | `	if( pEngine == 0 ){` |
|      ! 0 |  709 | `		return PH7_NOMEM;` |
|        - |  710 | `	}` |
|        - |  711 | `	/* Zero the structure */` |
|     8460 |  712 | `	SyZero(pEngine,sizeof(ph7));` |
|        - |  713 | `	/* Initialize engine fields */` |
|     8460 |  714 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|     8460 |  715 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|     8460 |  716 | `	if( rc != PH7_OK ){` |
|      ! 0 |  717 | `		goto Release;` |
|        - |  718 | `	}` |
|        - |  719 | `#if defined(PH7_ENABLE_THREADS)` |
|     8460 |  720 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|        - |  721 | `#endif` |
|        - |  722 | `	/* Default configuration */` |
|     8460 |  723 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|     8460 |  724 | `	SySetInit(&pEngine->xConf.aIniEntry,&pEngine->sAllocator,sizeof(VmIniEntry));` |
|        - |  725 | `	/* Install a default compile-time error consumer routine */` |
|     8460 |  726 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|        - |  727 | `	/* Built-in vfs */` |
|     8460 |  728 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|        - |  729 | `#if defined(PH7_ENABLE_THREADS)` |
|     8460 |  730 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  731 | `		 /* Associate a recursive mutex with this instance */` |
|     8460 |  732 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|     8460 |  733 | `		 if( pEngine->pMutex == 0 ){` |
|      ! 0 |  734 | `			 rc = PH7_NOMEM;` |
|      ! 0 |  735 | `			 goto Release;` |
|        - |  736 | `		 }` |
|     4222 |  737 | `	 }` |
|        - |  738 | `#endif` |
|        - |  739 | `	/* Link to the list of active engines */` |
|        - |  740 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  741 | `	/* Enter the global mutex */` |
|     8460 |  742 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  743 | `#endif` |
|     8460 |  744 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|     8460 |  745 | `	sMPGlobal.nEngine++;` |
|        - |  746 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  747 | `	/* Leave the global mutex */` |
|     8460 |  748 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  749 | `#endif` |
|        - |  750 | `	/* Write a pointer to the new instance */` |
|     8460 |  751 | `	*ppEngine = pEngine;` |
|     8460 |  752 | `	return PH7_OK;` |
|      ! 0 |  753 | `Release:` |
|      ! 0 |  754 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|      ! 0 |  755 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|      ! 0 |  756 | `	return rc;` |
|     4227 |  757 | `}` |
|        - |  758 | `/*` |
|        - |  759 | ` * [CAPIREF: ph7_release()]` |
|        - |  760 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  761 | ` */` |
|     8477 |  762 | `int ph7_release(ph7 *pEngine)` |
|        5 |  763 | `{` |
|        - |  764 | `	int rc;` |
|     8482 |  765 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|      ! 0 |  766 | `		return PH7_CORRUPT;` |
|        - |  767 | `	}` |
|        - |  768 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  769 | `	 /* Acquire engine mutex */` |
|     8482 |  770 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     8482 |  771 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     8477 |  772 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  773 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  774 | `	 }` |
|        - |  775 | `#endif` |
|        - |  776 | `	/* Release the engine */` |
|     8482 |  777 | `	rc = EngineRelease(&(*pEngine));` |
|        - |  778 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  779 | `	 /* Leave engine mutex */` |
|     8482 |  780 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  781 | `	 /* Release engine mutex */` |
|     8482 |  782 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  783 | `#endif` |
|        - |  784 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  785 | `	/* Enter the global mutex */` |
|     8482 |  786 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  787 | `#endif` |
|        - |  788 | `	/* Unlink from the list of active engines */` |
|     8482 |  789 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|     8482 |  790 | `	sMPGlobal.nEngine--;` |
|        - |  791 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  792 | `	/* Leave the global mutex */` |
|     8482 |  793 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  794 | `#endif` |
|        - |  795 | `	/* Release the memory chunk allocated to this engine */` |
|     8482 |  796 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     8482 |  797 | `	return rc;` |
|     4238 |  798 | `}` |
|        - |  799 | `/*` |
|        - |  800 | ` * Compile a raw PHP script.` |
|        - |  801 | ` * To execute a PHP code, it must first be compiled into a byte-code program using this routine.` |
|        - |  802 | ` * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]` |
|        - |  803 | ` * should  display the appropriate error message and this function set ppVm to null and return` |
|        - |  804 | ` * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled` |
|        - |  805 | ` * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].` |
|        - |  806 | ` * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script` |
|        - |  807 | ` * for evaluation.` |
|        - |  808 | ` */` |
|     8445 |  809 | `static sxi32 ProcessScript(` |
|        - |  810 | `	ph7 *pEngine,          /* Running PH7 engine */` |
|        - |  811 | `	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */` |
|        - |  812 | `	SyString *pScript,     /* Raw PHP script to compile */` |
|        - |  813 | `	sxi32 iFlags,          /* Compile-time flags */` |
|        - |  814 | `	const char *zFilePath  /* File path if script come from a file. NULL otherwise */` |
|        - |  815 | `	)` |
|        5 |  816 | `{` |
|        - |  817 | `	ph7_vm *pVm;` |
|        - |  818 | `	int rc;` |
|        - |  819 | `	/* Allocate a new virtual machine */` |
|     8450 |  820 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|     8450 |  821 | `	if( pVm == 0 ){` |
|        - |  822 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  823 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|      ! 0 |  824 | `		if( ppVm ){` |
|      ! 0 |  825 | `			*ppVm = 0;` |
|      ! 0 |  826 | `		}` |
|      ! 0 |  827 | `		return PH7_NOMEM;` |
|        - |  828 | `	}` |
|     8450 |  829 | `	if( iFlags < 0 ){` |
|        - |  830 | `		/* Default compile-time flags */` |
|      ! 0 |  831 | `		iFlags = 0;` |
|      ! 0 |  832 | `	}` |
|        - |  833 | `	/* Initialize the Virtual Machine */` |
|     8450 |  834 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|     8450 |  835 | `	if( rc != PH7_OK ){` |
|      ! 0 |  836 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|      ! 0 |  837 | `		if( ppVm ){` |
|      ! 0 |  838 | `			*ppVm = 0;` |
|      ! 0 |  839 | `		}` |
|      ! 0 |  840 | `		return PH7_VM_ERR;` |
|        - |  841 | `	}` |
|        - |  842 | `	/* The host's php.ini directives, BEFORE a line of the unit is compiled: a` |
|        - |  843 | `	 * compile diagnostic is gated by display_errors/log_errors/error_reporting` |
|        - |  844 | `	 * exactly as a runtime one is, and this is the only window in which they can` |
|        - |  845 | `	 * still be in hand for the main script's own compile. */` |
|     8450 |  846 | `	PH7_VmApplyEngineIni(pVm);` |
|        - |  847 | `	/* A syntax-CHECK compile (phl -l) never runs what it compiles, which changes` |
|        - |  848 | `	 * what a class declaration whose base is missing has to do -- see the flag's` |
|        - |  849 | `	 * note in ph7int.h -- and it also changes how the unit is NAMED: php's lint` |
|        - |  850 | `	 * mode hands the argument straight to the compiler where a run expands it to` |
|        - |  851 | ``	 * an absolute path first, so `phl -l ./x.php` must say `./x.php`. Set before`` |
|        - |  852 | `	 * the path is pushed, which is what reads it. */` |
|     8450 |  853 | `	pVm->bSyntaxCheck = (iFlags & PH7_SYNTAX_CHECK) ? 1 : 0;` |
|     8450 |  854 | `	if( zFilePath ){` |
|        - |  855 | `		/* Push processed file path */` |
|     7856 |  856 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|     3925 |  857 | `	}else{` |
|        - |  858 | `		/* Anonymous source (phl -r / an embedder snippet): php names it` |
|        - |  859 | `		 * "Command line code" in every diagnostic location suffix. */` |
|      598 |  860 | `		PH7_VmPushFilePath(pVm,"Command line code",-1,TRUE,0);` |
|        - |  861 | `	}` |
|        - |  862 | `	/* Reset the error message consumer */` |
|     8450 |  863 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|        - |  864 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|        - |  865 | `	{` |
|     8450 |  866 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|     8450 |  867 | `		if( SyStringLength(pScript) > nLimit ){` |
|      ! 0 |  868 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|        - |  869 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|      ! 0 |  870 | `				SyStringLength(pScript),nLimit);` |
|      ! 0 |  871 | `		}` |
|        - |  872 | `	}` |
|        - |  873 | `	/* Compile the script */` |
|     8450 |  874 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|     8450 |  875 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|     4217 |  876 | `	}` |
|     8450 |  877 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     1464 |  878 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|        - |  879 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     1464 |  880 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     1464 |  881 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     1464 |  882 | `		if( ppVm ){` |
|     1464 |  883 | `			*ppVm = 0;` |
|      730 |  884 | `		}` |
|     1464 |  885 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|        - |  886 | `	}` |
|        - |  887 | `	/* Prepare the virtual machine for bytecode execution */` |
|     6990 |  888 | `	rc = PH7_VmMakeReady(pVm);` |
|     6990 |  889 | `	if( rc != PH7_OK ){` |
|        8 |  890 | `		goto Release;` |
|        - |  891 | `	}` |
|        - |  892 | `	/* A class php does not early-bind waits for its statement. */` |
|     6984 |  893 | `	PH7_VmHideClasses(pVm,0);` |
|        - |  894 | `	/* Install local import path which is the current directory -- unless the host` |
|        - |  895 | ``	 * already named one. IMPORT_PATH APPENDS, and a `-d include_path=` handed to`` |
|        - |  896 | `	 * the engine is applied at VM birth, i.e. before this: the two together` |
|        - |  897 | ``	 * answered `.:.` for a run started with `-d include_path=.`. */`` |
|     6984 |  898 | `	if( SySetUsed(&pVm->aPaths) < 1 ){` |
|     6976 |  899 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|     3480 |  900 | `	}` |
|        - |  901 | `#if defined(PH7_ENABLE_THREADS)` |
|     6984 |  902 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  903 | `		 /* Associate a recursive mutex with this instance */` |
|     6984 |  904 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|     6984 |  905 | `		 if( pVm->pMutex == 0 ){` |
|      ! 0 |  906 | `			 goto Release;` |
|        - |  907 | `		 }` |
|     3484 |  908 | `	 }` |
|        - |  909 | `#endif` |
|        - |  910 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|     6984 |  911 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|     6984 |  912 | `	pEngine->iVm++;` |
|        - |  913 | `	/* Point to the freshly created VM */` |
|     6984 |  914 | `	*ppVm = pVm;` |
|        - |  915 | `	/* Ready to execute PH7 bytecode */` |
|     6984 |  916 | `	return PH7_OK;` |
|        3 |  917 | `Release:` |
|        - |  918 | `	{` |
|        - |  919 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|        - |  920 | `		 * typed class constant whose value violates its declared type) is a compile` |
|        - |  921 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|        - |  922 | `		 * Captured before the releases free the VM. */` |
|        8 |  923 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|        8 |  924 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|        8 |  925 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|        8 |  926 | `		*ppVm = 0;` |
|        8 |  927 | `		return rcRet;` |
|        - |  928 | `	}` |
|     4222 |  929 | `}` |
|        - |  930 | `/*` |
|        - |  931 | ` * [CAPIREF: ph7_compile()]` |
|        - |  932 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  933 | ` */` |
|      ! 0 |  934 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|      ! 0 |  935 | `{` |
|        - |  936 | `	SyString sScript;` |
|        - |  937 | `	int rc;` |
|      ! 0 |  938 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|      ! 0 |  939 | `		return PH7_CORRUPT;` |
|        - |  940 | `	}` |
|      ! 0 |  941 | `	if( nLen < 0 ){` |
|        - |  942 | `		/* Compute input length automatically */` |
|      ! 0 |  943 | `		nLen = (int)SyStrlen(zSource);` |
|      ! 0 |  944 | `	}` |
|      ! 0 |  945 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|        - |  946 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  947 | `	 /* Acquire engine mutex */` |
|      ! 0 |  948 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 |  949 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 |  950 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  951 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  952 | `	 }` |
|        - |  953 | `#endif` |
|        - |  954 | `	/* Compile the script */` |
|      ! 0 |  955 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|        - |  956 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  957 | `	 /* Leave engine mutex */` |
|      ! 0 |  958 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  959 | `#endif` |
|        - |  960 | `	/* Compilation result */` |
|      ! 0 |  961 | `	return rc;` |
|      ! 0 |  962 | `}` |
|        - |  963 | `/*` |
|        - |  964 | ` * [CAPIREF: ph7_compile_v2()]` |
|        - |  965 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  966 | ` */` |
|      594 |  967 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|        4 |  968 | `{` |
|        - |  969 | `	SyString sScript;` |
|        - |  970 | `	int rc;` |
|      598 |  971 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|      ! 0 |  972 | `		return PH7_CORRUPT;` |
|        - |  973 | `	}` |
|      598 |  974 | `	if( nLen < 0 ){` |
|        - |  975 | `		/* Compute input length automatically */` |
|      586 |  976 | `		nLen = (int)SyStrlen(zSource);` |
|      291 |  977 | `	}` |
|      598 |  978 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|        - |  979 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  980 | `	 /* Acquire engine mutex */` |
|      598 |  981 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      598 |  982 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      594 |  983 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  984 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  985 | `	 }` |
|        - |  986 | `#endif` |
|        - |  987 | `	/* Compile the script */` |
|      598 |  988 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|        - |  989 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  990 | `	 /* Leave engine mutex */` |
|      598 |  991 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  992 | `#endif` |
|        - |  993 | `	/* Compilation result */` |
|      598 |  994 | `	return rc;` |
|      301 |  995 | `}` |
|        - |  996 | `/*` |
|        - |  997 | ` * [CAPIREF: ph7_compile_file()]` |
|        - |  998 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  999 | ` */` |
|     7859 | 1000 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|        5 | 1001 | `{` |
|        - | 1002 | `	const ph7_vfs *pVfs;` |
|        - | 1003 | `	int rc;` |
|     7864 | 1004 | `	if( ppOutVm ){` |
|     7864 | 1005 | `		*ppOutVm = 0;` |
|     3924 | 1006 | `	}` |
|     7864 | 1007 | `	rc = PH7_OK; /* cc warning */` |
|     7864 | 1008 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|      ! 0 | 1009 | `		return PH7_CORRUPT;` |
|        - | 1010 | `	}` |
|        - | 1011 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1012 | `	 /* Acquire engine mutex */` |
|     7864 | 1013 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     7864 | 1014 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     7859 | 1015 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 | 1016 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1017 | `	 }` |
|        - | 1018 | `#endif` |
|        - | 1019 | `	 /*` |
|        - | 1020 | `	  * Check if the underlying vfs implement the memory map` |
|        - | 1021 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|        - | 1022 | `	  */` |
|     7864 | 1023 | `	 pVfs = pEngine->pVfs;` |
|     7864 | 1024 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|        - | 1025 | `		 /* Memory map routine not implemented */` |
|      ! 0 | 1026 | `		 rc = PH7_IO_ERR;` |
|      ! 0 | 1027 | `	 }else{` |
|     7864 | 1028 | `		 void *pMapView = 0; /* cc warning */` |
|     7864 | 1029 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|        - | 1030 | `		 SyString sScript;` |
|        - | 1031 | `		 /* Try to get a memory view of the whole file */` |
|     7864 | 1032 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|     7864 | 1033 | `		 if( rc != PH7_OK ){` |
|        - | 1034 | `			 /* Assume an IO error */` |
|        8 | 1035 | `			 rc = PH7_IO_ERR;` |
|        4 | 1036 | `		 }else{` |
|        - | 1037 | `			 /* Compile the file */` |
|     7856 | 1038 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|     7856 | 1039 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|        - | 1040 | `			 /* Release the memory view of the whole file. A ZERO-length view is` |
|        - | 1041 | `			  * the answer for a 0-byte file -- a legal, empty PHP program -- and` |
|        - | 1042 | `			  * it is not a mapping: the vfs hands back a static empty string, so` |
|        - | 1043 | `			  * unmapping it would be a free of something it never allocated. */` |
|     7856 | 1044 | `			 if( pVfs->xUnmap && nSize > 0 ){` |
|     7852 | 1045 | `				 pVfs->xUnmap(pMapView,nSize);` |
|     3918 | 1046 | `			 }` |
|        - | 1047 | `		 }` |
|        - | 1048 | `	 }` |
|        - | 1049 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1050 | `	 /* Leave engine mutex */` |
|     7864 | 1051 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1052 | `#endif` |
|        - | 1053 | `	/* Compilation result */` |
|     7864 | 1054 | `	return rc;` |
|     3929 | 1055 | `}` |
|        - | 1056 | `/*` |
|        - | 1057 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|        - | 1058 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1059 | ` */` |
|        2 | 1060 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|        1 | 1061 | `{` |
|        - | 1062 | `	int rc;` |
|        - | 1063 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|        3 | 1064 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1065 | `		return PH7_CORRUPT;` |
|        - | 1066 | `	}` |
|        - | 1067 | `#ifdef UNTRUST` |
|        - | 1068 | `	if( xConsumer == 0 ){` |
|        - | 1069 | `		return PH7_CORRUPT;` |
|        - | 1070 | `	}` |
|        - | 1071 | `#endif` |
|        - | 1072 | `	/* Dump VM instructions */` |
|        3 | 1073 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|        3 | 1074 | `	return rc;` |
|        2 | 1075 | `}` |
|        - | 1076 | `/*` |
|        - | 1077 | ` * [CAPIREF: ph7_vm_config()]` |
|        - | 1078 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1079 | ` */` |
|   237754 | 1080 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|        5 | 1081 | `{` |
|        - | 1082 | `	va_list ap;` |
|        - | 1083 | `	int rc;` |
|        - | 1084 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   237759 | 1085 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1086 | `		return PH7_CORRUPT;` |
|        - | 1087 | `	}` |
|        - | 1088 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1089 | `	 /* Acquire VM mutex */` |
|   237759 | 1090 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   237759 | 1091 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   237754 | 1092 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1093 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1094 | `	 }` |
|        - | 1095 | `#endif` |
|        - | 1096 | `	/* Confiugure the virtual machine */` |
|   237759 | 1097 | `	va_start(ap,iConfigOp);` |
|   237759 | 1098 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   237759 | 1099 | `	va_end(ap);` |
|        - | 1100 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1101 | `	 /* Leave VM mutex */` |
|   237759 | 1102 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1103 | `#endif` |
|   237759 | 1104 | `	return rc;` |
|   118706 | 1105 | `}` |
|        - | 1106 | `/*` |
|        - | 1107 | ` * [CAPIREF: ph7_vm_exec()]` |
|        - | 1108 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1109 | ` */` |
|     6877 | 1110 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|        5 | 1111 | `{` |
|        - | 1112 | `	int rc;` |
|        - | 1113 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     6882 | 1114 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       16 | 1115 | `		return PH7_CORRUPT;` |
|        - | 1116 | `	}` |
|        - | 1117 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1118 | `	 /* Acquire VM mutex */` |
|     6882 | 1119 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6882 | 1120 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6869 | 1121 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1122 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1123 | `	 }` |
|        - | 1124 | `#endif` |
|        - | 1125 | `	/* Execute PH7 byte-code */` |
|     6882 | 1126 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|     6890 | 1127 | `	if( pExitStatus ){` |
|        - | 1128 | `		/* Exit status */` |
|     6844 | 1129 | `		*pExitStatus = pVm->iExitStatus;` |
|     3414 | 1130 | `	}` |
|        - | 1131 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1132 | `	 /* Leave VM mutex */` |
|     6890 | 1133 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1134 | `#endif` |
|        - | 1135 | `	/* Execution result */` |
|     6890 | 1136 | `	return rc;` |
|     3442 | 1137 | `}` |
|        - | 1138 | `/*` |
|        - | 1139 | ` * [CAPIREF: ph7_vm_reset()]` |
|        - | 1140 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1141 | ` */` |
|       16 | 1142 | `int ph7_vm_reset(ph7_vm *pVm)` |
|      ! 0 | 1143 | `{` |
|        - | 1144 | `	int rc;` |
|        - | 1145 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       16 | 1146 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1147 | `		return PH7_CORRUPT;` |
|        - | 1148 | `	}` |
|        - | 1149 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1150 | `	 /* Acquire VM mutex */` |
|       16 | 1151 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       16 | 1152 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       16 | 1153 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1154 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1155 | `	 }` |
|        - | 1156 | `#endif` |
|       16 | 1157 | `	rc = PH7_VmReset(&(*pVm));` |
|        - | 1158 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1159 | `	 /* Leave VM mutex */` |
|       16 | 1160 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1161 | `#endif` |
|       16 | 1162 | `	return rc;` |
|        8 | 1163 | `}` |
|        - | 1164 | `/*` |
|        - | 1165 | ` * [CAPIREF: ph7_vm_release()]` |
|        - | 1166 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1167 | ` */` |
|     6995 | 1168 | `int ph7_vm_release(ph7_vm *pVm)` |
|        5 | 1169 | `{` |
|        - | 1170 | `	ph7 *pEngine;` |
|        - | 1171 | `	int rc;` |
|        - | 1172 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     7000 | 1173 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1174 | `		return PH7_CORRUPT;` |
|        - | 1175 | `	}` |
|        - | 1176 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1177 | `	 /* Acquire VM mutex */` |
|     7000 | 1178 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     7000 | 1179 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6995 | 1180 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1181 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1182 | `	 }` |
|        - | 1183 | `#endif` |
|     7000 | 1184 | `	pEngine = pVm->pEngine;` |
|     7000 | 1185 | `	rc = PH7_VmRelease(&(*pVm));` |
|        - | 1186 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1187 | `	 /* Leave VM mutex */` |
|     7000 | 1188 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     7000 | 1189 | `	 if( rc == PH7_OK && pVm->pMutex ){` |
|        - | 1190 | `		 /* The per-VM mutex was allocated in ProcessScript and never freed — one` |
|        - | 1191 | `		  * leak per VM, which LeakSanitizer reports on every single run. */` |
|     7000 | 1192 | `		 SyMutexRelease(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|     7000 | 1193 | `		 pVm->pMutex = 0;` |
|     3492 | 1194 | `	 }` |
|        - | 1195 | `#endif` |
|     7000 | 1196 | `	if( rc == PH7_OK ){` |
|        - | 1197 | `		/* Unlink from the list of active VM */` |
|        - | 1198 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1199 | `			/* Acquire engine mutex */` |
|     7000 | 1200 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     7000 | 1201 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6995 | 1202 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 | 1203 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1204 | `			}` |
|        - | 1205 | `#endif` |
|     7000 | 1206 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|     7000 | 1207 | `		pEngine->iVm--;` |
|        - | 1208 | `		/* Release the memory chunk allocated to this VM */` |
|     7000 | 1209 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|        - | 1210 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1211 | `			/* Leave engine mutex */` |
|     7000 | 1212 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1213 | `#endif` |
|     3492 | 1214 | `	}` |
|     7000 | 1215 | `	return rc;` |
|     3497 | 1216 | `}` |
|        - | 1217 | `/*` |
|        - | 1218 | ` * [CAPIREF: ph7_create_function()]` |
|        - | 1219 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1220 | ` */` |
|  9534438 | 1221 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|        5 | 1222 | `{` |
|        - | 1223 | `	SyString sName;` |
|        - | 1224 | `	int rc;` |
|        - | 1225 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  9534443 | 1226 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1227 | `		return PH7_CORRUPT;` |
|        - | 1228 | `	}` |
|  9534443 | 1229 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|        - | 1230 | `	/* Remove leading and trailing white spaces */` |
|  9534443 | 1231 | `	SyStringFullTrim(&sName);` |
|        - | 1232 | `	/* Ticket 1433-003: NULL values are not allowed */` |
|  9534443 | 1233 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|      ! 0 | 1234 | `		return PH7_CORRUPT;` |
|        - | 1235 | `	}` |
|        - | 1236 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1237 | `	 /* Acquire VM mutex */` |
|  9534443 | 1238 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  9534443 | 1239 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  9534438 | 1240 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1241 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1242 | `	 }` |
|        - | 1243 | `#endif` |
|        - | 1244 | `	/* Install the foreign function */` |
|  9534443 | 1245 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|        - | 1246 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1247 | `	 /* Leave VM mutex */` |
|  9534443 | 1248 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1249 | `#endif` |
|  9534443 | 1250 | `	return rc;` |
|  4748347 | 1251 | `}` |
|        - | 1252 | `/*` |
|        - | 1253 | ` * [CAPIREF: ph7_delete_function()]` |
|        - | 1254 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1255 | ` */` |
|      ! 0 | 1256 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|      ! 0 | 1257 | `{` |
|      ! 0 | 1258 | `	ph7_user_func *pFunc = 0;` |
|        - | 1259 | `	int rc;` |
|        - | 1260 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      ! 0 | 1261 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1262 | `		return PH7_CORRUPT;` |
|        - | 1263 | `	}` |
|        - | 1264 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1265 | `	 /* Acquire VM mutex */` |
|      ! 0 | 1266 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 | 1267 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 | 1268 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1269 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1270 | `	 }` |
|        - | 1271 | `#endif` |
|        - | 1272 | `	/* Perform the deletion */` |
|      ! 0 | 1273 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|      ! 0 | 1274 | `	if( rc == PH7_OK ){` |
|        - | 1275 | `		/* A name that WAS callable is not any more; every call site that remembers` |
|        - | 1276 | `		 * having screened it has to ask again (OP_CALL_INIT). */` |
|      ! 0 | 1277 | `		pVm->nCallableGen++;` |
|        - | 1278 | `		/* Release internal fields */` |
|      ! 0 | 1279 | `		SySetRelease(&pFunc->aAux);` |
|      ! 0 | 1280 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 | 1281 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 | 1282 | `	}` |
|        - | 1283 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1284 | `	 /* Leave VM mutex */` |
|      ! 0 | 1285 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1286 | `#endif` |
|      ! 0 | 1287 | `	return rc;` |
|      ! 0 | 1288 | `}` |
|        - | 1289 | `/*` |
|        - | 1290 | ` * [CAPIREF: ph7_create_constant()]` |
|        - | 1291 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1292 | ` */` |
| 13683132 | 1293 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|        5 | 1294 | `{` |
|        - | 1295 | `	SyString sName;` |
|        - | 1296 | `	int rc;` |
|        - | 1297 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 13683137 | 1298 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1299 | `		return PH7_CORRUPT;` |
|        - | 1300 | `	}` |
| 13683137 | 1301 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|        - | 1302 | `	/* Remove leading and trailing white spaces */` |
| 13683137 | 1303 | `	SyStringFullTrim(&sName);` |
| 13683137 | 1304 | `	if( sName.nByte < 1 ){` |
|        - | 1305 | `		/* Empty constant name */` |
|      ! 0 | 1306 | `		return PH7_CORRUPT;` |
|        - | 1307 | `	}` |
|        - | 1308 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
| 13683137 | 1309 | `	if( xExpand == 0 ){` |
|      ! 0 | 1310 | `		return PH7_CORRUPT;` |
|        - | 1311 | `	}` |
|        - | 1312 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1313 | `	 /* Acquire VM mutex */` |
| 13683137 | 1314 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 13683137 | 1315 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 13683132 | 1316 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1317 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1318 | `	 }` |
|        - | 1319 | `#endif` |
|        - | 1320 | `	/* Perform the registration */` |
| 13683137 | 1321 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|        - | 1322 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1323 | `	 /* Leave VM mutex */` |
| 13683137 | 1324 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1325 | `#endif` |
| 13683137 | 1326 | `	 return rc;` |
|  6633517 | 1327 | `}` |
|        - | 1328 | `/*` |
|        - | 1329 | ` * [CAPIREF: ph7_delete_constant()]` |
|        - | 1330 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1331 | ` */` |
|      ! 0 | 1332 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|      ! 0 | 1333 | `{` |
|      ! 0 | 1334 | `	ph7_constant *pCons = 0;   /* cl cannot see that rc == PH7_OK implies it was set */` |
|        - | 1335 | `	int rc;` |
|        - | 1336 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      ! 0 | 1337 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1338 | `		return PH7_CORRUPT;` |
|        - | 1339 | `	}` |
|        - | 1340 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1341 | `	 /* Acquire VM mutex */` |
|      ! 0 | 1342 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 | 1343 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 | 1344 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1345 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1346 | `	 }` |
|        - | 1347 | `#endif` |
|        - | 1348 | `	 /* Query the constant hashtable -- under php's matching rule, so a namespaced` |
|        - | 1349 | `	  * name reaches the entry it was folded into (PH7_VmConstantFetch). */` |
|        - | 1350 | `	 {` |
|      ! 0 | 1351 | `		 SyHashEntry *pEntry = PH7_VmConstantFetch(pVm,zName,SyStrlen(zName),1);` |
|      ! 0 | 1352 | `		 if( pEntry == 0 ){` |
|      ! 0 | 1353 | `			 rc = SXERR_NOTFOUND;` |
|      ! 0 | 1354 | `		 }else{` |
|      ! 0 | 1355 | `			 pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|      ! 0 | 1356 | `			 SyHashDeleteEntry2(pEntry);` |
|      ! 0 | 1357 | `			 rc = PH7_OK;` |
|        - | 1358 | `		 }` |
|        - | 1359 | `	 }` |
|      ! 0 | 1360 | `	 if( rc == PH7_OK ){` |
|        - | 1361 | `		 /* A name that WAS a constant is not any more, and the entry every LOADC site` |
|        - | 1362 | `		  * that resolved to it remembers is about to be freed (PH7_VmConstSiteAnswer). */` |
|      ! 0 | 1363 | `		 pVm->nConstGen++;` |
|        - | 1364 | `		 /* Perform the deletion */` |
|      ! 0 | 1365 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|      ! 0 | 1366 | `		 if( pCons->zKey ){` |
|      ! 0 | 1367 | `			 SyMemBackendFree(&pVm->sAllocator,pCons->zKey);` |
|      ! 0 | 1368 | `		 }` |
|      ! 0 | 1369 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 | 1370 | `	 }` |
|        - | 1371 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1372 | `	 /* Leave VM mutex */` |
|      ! 0 | 1373 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1374 | `#endif` |
|      ! 0 | 1375 | `	return rc;` |
|      ! 0 | 1376 | `}` |
|        - | 1377 | `/*` |
|        - | 1378 | ` * [CAPIREF: ph7_new_scalar()]` |
|        - | 1379 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1380 | ` */` |
|  1957615 | 1381 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|        5 | 1382 | `{` |
|        - | 1383 | `	ph7_value *pObj;` |
|        - | 1384 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  1957620 | 1385 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1386 | `		return 0;` |
|        - | 1387 | `	}` |
|        - | 1388 | `	/* Allocate a new scalar variable */` |
|  1957620 | 1389 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  1957620 | 1390 | `	if( pObj == 0 ){` |
|      ! 0 | 1391 | `		return 0;` |
|        - | 1392 | `	}` |
|        - | 1393 | `	/* Nullify the new scalar */` |
|  1957620 | 1394 | `	PH7_MemObjInit(pVm,pObj);` |
|  1957620 | 1395 | `	return pObj;` |
|   978700 | 1396 | `}` |
|        - | 1397 | `/*` |
|        - | 1398 | ` * [CAPIREF: ph7_new_array()]` |
|        - | 1399 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1400 | ` */` |
|  3339002 | 1401 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|        5 | 1402 | `{` |
|        - | 1403 | `	ph7_hashmap *pMap;` |
|        - | 1404 | `	ph7_value *pObj;` |
|        - | 1405 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  3339007 | 1406 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1407 | `		return 0;` |
|        - | 1408 | `	}` |
|        - | 1409 | `	/* Create a new hashmap first */` |
|  3339007 | 1410 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  3339007 | 1411 | `	if( pMap == 0 ){` |
|      ! 0 | 1412 | `		return 0;` |
|        - | 1413 | `	}` |
|        - | 1414 | `	/* Associate a new ph7_value with this hashmap */` |
|  3339007 | 1415 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  3339007 | 1416 | `	if( pObj == 0 ){` |
|      ! 0 | 1417 | `		PH7_HashmapRelease(pMap,TRUE);` |
|      ! 0 | 1418 | `		return 0;` |
|        - | 1419 | `	}` |
|  3339007 | 1420 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|  3339007 | 1421 | `	return pObj;` |
|  1669262 | 1422 | `}` |
|        - | 1423 | `/*` |
|        - | 1424 | ` * [CAPIREF: ph7_release_value()]` |
|        - | 1425 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1426 | ` */` |
|  3984322 | 1427 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|        5 | 1428 | `{` |
|        - | 1429 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  3984327 | 1430 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       76 | 1431 | `		return PH7_CORRUPT;` |
|        - | 1432 | `	}` |
|  3984253 | 1433 | `	if( pValue ){` |
|        - | 1434 | `		/* Release the value */` |
|  3984253 | 1435 | `		PH7_MemObjRelease(pValue);` |
|  3984253 | 1436 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|  1991913 | 1437 | `	}` |
|  3984253 | 1438 | `	return PH7_OK;` |
|  1991955 | 1439 | `}` |
|        - | 1440 | `/*` |
|        - | 1441 | ` * [CAPIREF: ph7_value_to_int()]` |
|        - | 1442 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1443 | ` */` |
|   755619 | 1444 | `int ph7_value_to_int(ph7_value *pValue)` |
|        5 | 1445 | `{` |
|        - | 1446 | `	int rc;` |
|   755624 | 1447 | `	rc = PH7_MemObjToInteger(pValue);` |
|   755624 | 1448 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1449 | `		return 0;` |
|        - | 1450 | `	}` |
|   755624 | 1451 | `	return (int)pValue->x.iVal;` |
|   377973 | 1452 | `}` |
|        - | 1453 | `/*` |
|        - | 1454 | ` * [CAPIREF: ph7_value_to_bool()]` |
|        - | 1455 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1456 | ` */` |
|    62777 | 1457 | `int ph7_value_to_bool(ph7_value *pValue)` |
|        5 | 1458 | `{` |
|        - | 1459 | `	int rc;` |
|    62782 | 1460 | `	rc = PH7_MemObjToBool(pValue);` |
|    62782 | 1461 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1462 | `		return 0;` |
|        - | 1463 | `	}` |
|    62782 | 1464 | `	return (int)pValue->x.iVal;` |
|    31092 | 1465 | `}` |
|        - | 1466 | `/*` |
|        - | 1467 | ` * [CAPIREF: ph7_value_to_int64()]` |
|        - | 1468 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1469 | ` */` |
| 11628671 | 1470 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|        5 | 1471 | `{` |
|        - | 1472 | `	int rc;` |
| 11628676 | 1473 | `	rc = PH7_MemObjToInteger(pValue);` |
| 11628676 | 1474 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1475 | `		return 0;` |
|        - | 1476 | `	}` |
| 11628676 | 1477 | `	return pValue->x.iVal;` |
|  5814993 | 1478 | `}` |
|        - | 1479 | `/*` |
|        - | 1480 | ` * [CAPIREF: ph7_value_to_double()]` |
|        - | 1481 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1482 | ` */` |
|     4356 | 1483 | `double ph7_value_to_double(ph7_value *pValue)` |
|        5 | 1484 | `{` |
|        - | 1485 | `	int rc;` |
|     4361 | 1486 | `	rc = PH7_MemObjToReal(pValue);` |
|     4361 | 1487 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1488 | `		return (double)0;` |
|        - | 1489 | `	}` |
|     4361 | 1490 | `	return (double)pValue->rVal;` |
|     2185 | 1491 | `}` |
|        - | 1492 | `/*` |
|        - | 1493 | ` * [CAPIREF: ph7_value_to_string()]` |
|        - | 1494 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1495 | ` */` |
| 17273165 | 1496 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|        5 | 1497 | `{` |
| 17273170 | 1498 | `	PH7_MemObjToString(pValue);` |
| 17273170 | 1499 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
| 17138088 | 1500 | `		SyBlobNullAppend(&pValue->sBlob);` |
| 17138088 | 1501 | `		if( pLen ){` |
| 16871759 | 1502 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  8436665 | 1503 | `		}` |
| 17138088 | 1504 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|      ! 0 | 1505 | `	}else{` |
|        - | 1506 | `		/* Return the empty string */` |
|   135087 | 1507 | `		if( pLen ){` |
|   134285 | 1508 | `			*pLen = 0;` |
|    66995 | 1509 | `		}` |
|   135087 | 1510 | `		return "";` |
|        - | 1511 | `	}` |
|  8637143 | 1512 | `}` |
|        - | 1513 | `/*` |
|        - | 1514 | ` * [CAPIREF: ph7_value_to_resource()]` |
|        - | 1515 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1516 | ` */` |
|   193325 | 1517 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|        5 | 1518 | `{` |
|   193330 | 1519 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|        - | 1520 | `		/* Not a resource,return NULL */` |
|      ! 0 | 1521 | `		return 0;` |
|        - | 1522 | `	}` |
|   193330 | 1523 | `	return pValue->x.pOther;` |
|    95972 | 1524 | `}` |
|        - | 1525 | `/*` |
|        - | 1526 | ` * [CAPIREF: ph7_value_compare()]` |
|        - | 1527 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1528 | ` */` |
|       80 | 1529 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|        3 | 1530 | `{` |
|        - | 1531 | `	int rc;` |
|       83 | 1532 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|        - | 1533 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|      ! 0 | 1534 | `		return 1;` |
|        - | 1535 | `	}` |
|        - | 1536 | `	/* Perform the comparison */` |
|       83 | 1537 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|        - | 1538 | `	/* A native compare handler may have REFUSED the pair (php throws comparing` |
|        - | 1539 | `	 * two different KINDS of DateTimeZone). The record is deliberately LEFT` |
|        - | 1540 | `	 * standing: array_keys() reaches the comparator through this entry point, and` |
|        - | 1541 | `	 * the host-call boundary raises what it recorded exactly as it does for the` |
|        - | 1542 | `	 * builtins that call PH7_MemObjCmp directly. A host application driving this` |
|        - | 1543 | `	 * outside any execution never sees the throw, and PH7_VmInit/PH7_VmReset` |
|        - | 1544 | `	 * clear the record before the next one begins. */` |
|        - | 1545 | `	/* Comparison result */` |
|       83 | 1546 | `	return rc;` |
|       43 | 1547 | `}` |
|        - | 1548 | `/*` |
|        - | 1549 | ` * [CAPIREF: ph7_result_int()]` |
|        - | 1550 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1551 | ` */` |
|  4763223 | 1552 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|        5 | 1553 | `{` |
|  4763228 | 1554 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|        5 | 1555 | `}` |
|        - | 1556 | `/*` |
|        - | 1557 | ` * [CAPIREF: ph7_result_int64()]` |
|        - | 1558 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1559 | ` */` |
|  1696252 | 1560 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|        5 | 1561 | `{` |
|  1696257 | 1562 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|        5 | 1563 | `}` |
|        - | 1564 | `/*` |
|        - | 1565 | ` * [CAPIREF: ph7_result_bool()]` |
|        - | 1566 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1567 | ` */` |
|   772898 | 1568 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|        5 | 1569 | `{` |
|   772903 | 1570 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|        5 | 1571 | `}` |
|        - | 1572 | `/*` |
|        - | 1573 | ` * [CAPIREF: ph7_result_double()]` |
|        - | 1574 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1575 | ` */` |
|     1328 | 1576 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|        5 | 1577 | `{` |
|     1333 | 1578 | `	return ph7_value_double(pCtx->pRet,Value);` |
|        5 | 1579 | `}` |
|        - | 1580 | `/*` |
|        - | 1581 | ` * [CAPIREF: ph7_result_null()]` |
|        - | 1582 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1583 | ` */` |
|    14518 | 1584 | `int ph7_result_null(ph7_context *pCtx)` |
|        5 | 1585 | `{` |
|        - | 1586 | `	/* Invalidate any prior representation and set the NULL flag */` |
|    14523 | 1587 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    14523 | 1588 | `	return PH7_OK;` |
|        5 | 1589 | `}` |
|        - | 1590 | `/*` |
|        - | 1591 | ` * [CAPIREF: ph7_result_string()]` |
|        - | 1592 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1593 | ` */` |
| 13140456 | 1594 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|        5 | 1595 | `{` |
| 13140461 | 1596 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|        5 | 1597 | `}` |
|        - | 1598 | `/*` |
|        - | 1599 | ` * [CAPIREF: ph7_result_string_format()]` |
|        - | 1600 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1601 | ` */` |
|   413690 | 1602 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|        5 | 1603 | `{` |
|        - | 1604 | `	ph7_value *p;` |
|        - | 1605 | `	va_list ap;` |
|        - | 1606 | `	int rc;` |
|   413695 | 1607 | `	p = pCtx->pRet;` |
|   413695 | 1608 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1609 | `		/* Invalidate any prior representation */` |
|   396667 | 1610 | `		PH7_MemObjRelease(p);` |
|   396667 | 1611 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|   198331 | 1612 | `	}` |
|        - | 1613 | `	/* Format the given string */` |
|   413695 | 1614 | `	va_start(ap,zFormat);` |
|   413695 | 1615 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|   413695 | 1616 | `	va_end(ap);` |
|   413695 | 1617 | `	return rc;` |
|        5 | 1618 | `}` |
|        - | 1619 | `/*` |
|        - | 1620 | ` * [CAPIREF: ph7_result_value()]` |
|        - | 1621 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1622 | ` */` |
|   935212 | 1623 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|        5 | 1624 | `{` |
|   935217 | 1625 | `	int rc = PH7_OK;` |
|   935217 | 1626 | `	if( pValue == 0 ){` |
|      ! 0 | 1627 | `		PH7_MemObjRelease(pCtx->pRet);` |
|      ! 0 | 1628 | `	}else{` |
|   935217 | 1629 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|        - | 1630 | `	}` |
|   935217 | 1631 | `	return rc;` |
|        5 | 1632 | `}` |
|        - | 1633 | `/*` |
|        - | 1634 | ` * [CAPIREF: ph7_result_resource()]` |
|        - | 1635 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1636 | ` */` |
|   102779 | 1637 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|        5 | 1638 | `{` |
|   102784 | 1639 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|        5 | 1640 | `}` |
|        - | 1641 | `/*` |
|        - | 1642 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|        - | 1643 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1644 | ` */` |
|   440311 | 1645 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|        5 | 1646 | `{` |
|        - | 1647 | `	ph7_value *pVal;` |
|   440316 | 1648 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|   440316 | 1649 | `	if( pVal ){` |
|        - | 1650 | `		/* Record value address so it can be freed automatically` |
|        - | 1651 | `		 * when the calling function returns.` |
|        - | 1652 | `		 */` |
|   440316 | 1653 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   220087 | 1654 | `	}` |
|   440316 | 1655 | `	return pVal;` |
|        5 | 1656 | `}` |
|        - | 1657 | `/*` |
|        - | 1658 | ` * [CAPIREF: ph7_context_new_array()]` |
|        - | 1659 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1660 | ` */` |
|   871427 | 1661 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|        5 | 1662 | `{` |
|        - | 1663 | `	ph7_value *pVal;` |
|   871432 | 1664 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   871432 | 1665 | `	if( pVal ){` |
|        - | 1666 | `		/* Record value address so it can be freed automatically` |
|        - | 1667 | `		 * when the calling function returns.` |
|        - | 1668 | `		 */` |
|   871432 | 1669 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   435639 | 1670 | `	}` |
|   871432 | 1671 | `	return pVal;` |
|        5 | 1672 | `}` |
|        - | 1673 | `/*` |
|        - | 1674 | ` * [CAPIREF: ph7_context_release_value()]` |
|        - | 1675 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1676 | ` */` |
|    59323 | 1677 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|        5 | 1678 | `{` |
|    59328 | 1679 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|    59328 | 1680 | `}` |
|        - | 1681 | `/*` |
|        - | 1682 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|        - | 1683 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1684 | ` */` |
|    38830 | 1685 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|        5 | 1686 | `{` |
|        - | 1687 | `	void *pChunk;` |
|    38835 | 1688 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    38835 | 1689 | `	if( pChunk ){` |
|    38835 | 1690 | `		if( ZeroChunk ){` |
|        - | 1691 | `			/* Zero the memory chunk */` |
|    25204 | 1692 | `			SyZero(pChunk,nByte);` |
|    12410 | 1693 | `		}` |
|    38835 | 1694 | `		if( AutoRelease ){` |
|        - | 1695 | `			ph7_aux_data sAux;` |
|        - | 1696 | `			/* Track the chunk so that it can be released automatically` |
|        - | 1697 | `			 * upon this context is destroyed.` |
|        - | 1698 | `			 */` |
|    25372 | 1699 | `			sAux.pAuxData = pChunk;` |
|    25372 | 1700 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|    12540 | 1701 | `		}` |
|    19218 | 1702 | `	}` |
|    38835 | 1703 | `	return pChunk;` |
|        5 | 1704 | `}` |
|        - | 1705 | `/*` |
|        - | 1706 | ` * Check if the given chunk address is registered in the call context` |
|        - | 1707 | ` * chunk container.` |
|        - | 1708 | ` * Return TRUE if registered.FALSE otherwise.` |
|        - | 1709 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|        - | 1710 | ` */` |
|     1796 | 1711 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|        5 | 1712 | `{` |
|        - | 1713 | `	ph7_aux_data *aAux,*pAux;` |
|        - | 1714 | `	sxu32 n;` |
|     1801 | 1715 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|        - | 1716 | `		/* Don't bother processing,the container is empty */` |
|     1097 | 1717 | `		return 0;` |
|        - | 1718 | `	}` |
|        - | 1719 | `	/* Perform the lookup */` |
|      709 | 1720 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     1083 | 1721 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     1083 | 1722 | `		pAux = &aAux[n];` |
|     1083 | 1723 | `		if( pAux->pAuxData == pChunk ){` |
|        - | 1724 | `			/* Chunk found */` |
|      709 | 1725 | `			return pAux;` |
|        - | 1726 | `		}` |
|      191 | 1727 | `	}` |
|        - | 1728 | `	/* No such allocated chunk */` |
|      ! 0 | 1729 | `	return 0;` |
|      896 | 1730 | `}` |
|        - | 1731 | `/*` |
|        - | 1732 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|        - | 1733 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1734 | ` */` |
|      ! 0 | 1735 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|      ! 0 | 1736 | `{` |
|        - | 1737 | `	ph7_aux_data *pAux;` |
|        - | 1738 | `	void *pNew;` |
|      ! 0 | 1739 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|      ! 0 | 1740 | `	if( pNew ){` |
|      ! 0 | 1741 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|      ! 0 | 1742 | `		if( pAux ){` |
|      ! 0 | 1743 | `			pAux->pAuxData = pNew;` |
|      ! 0 | 1744 | `		}` |
|      ! 0 | 1745 | `	}` |
|      ! 0 | 1746 | `	return pNew;` |
|      ! 0 | 1747 | `}` |
|        - | 1748 | `/*` |
|        - | 1749 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|        - | 1750 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1751 | ` */` |
|     1796 | 1752 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|        5 | 1753 | `{` |
|        - | 1754 | `	ph7_aux_data *pAux;` |
|     1801 | 1755 | `	if( pChunk == 0 ){` |
|        - | 1756 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|      ! 0 | 1757 | `		return;` |
|        - | 1758 | `	}` |
|     1801 | 1759 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|     1801 | 1760 | `	if( pAux ){` |
|        - | 1761 | `		/* Mark as destroyed */` |
|      709 | 1762 | `		pAux->pAuxData = 0;` |
|      352 | 1763 | `	}` |
|     1801 | 1764 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      896 | 1765 | `}` |
|        - | 1766 | `/*` |
|        - | 1767 | ` * [CAPIREF: ph7_array_fetch()]` |
|        - | 1768 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1769 | ` */` |
|    24650 | 1770 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|        5 | 1771 | `{` |
|        - | 1772 | `	ph7_hashmap_node *pNode;` |
|        - | 1773 | `	ph7_value *pValue;` |
|        - | 1774 | `	ph7_value skey;` |
|        - | 1775 | `	int rc;` |
|        - | 1776 | `	/* Make sure we are dealing with a valid hashmap */` |
|    24655 | 1777 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1778 | `		return 0;` |
|        - | 1779 | `	}` |
|    24655 | 1780 | `	if( nByte < 0 ){` |
|    18577 | 1781 | `		nByte = (int)SyStrlen(zKey);` |
|     9286 | 1782 | `	}` |
|        - | 1783 | `	/* Convert the key to a ph7_value  */` |
|    24655 | 1784 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|    24655 | 1785 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|        - | 1786 | `	/* Perform the lookup */` |
|    24655 | 1787 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|    24655 | 1788 | `	PH7_MemObjRelease(&skey);` |
|    24655 | 1789 | `	if( rc != PH7_OK ){` |
|        - | 1790 | `		/* No such entry */` |
|    10665 | 1791 | `		return 0;` |
|        - | 1792 | `	}` |
|        - | 1793 | `	/* Extract the target value */` |
|    13995 | 1794 | `	pValue = (ph7_value *)PH7_MemObjAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|    13995 | 1795 | `	return pValue;` |
|    12330 | 1796 | `}` |
|        - | 1797 | `/*` |
|        - | 1798 | ` * [CAPIREF: ph7_array_walk()]` |
|        - | 1799 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1800 | ` */` |
|    75031 | 1801 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|        5 | 1802 | `{` |
|        - | 1803 | `	int rc;` |
|    75036 | 1804 | `	if( xWalk == 0 ){` |
|      ! 0 | 1805 | `		return PH7_CORRUPT;` |
|        - | 1806 | `	}` |
|        - | 1807 | `	/* Make sure we are dealing with a valid hashmap */` |
|    75036 | 1808 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1809 | `		return PH7_CORRUPT;` |
|        - | 1810 | `	}` |
|        - | 1811 | `	/* Start the walk process */` |
|    75036 | 1812 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|    75036 | 1813 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|    37496 | 1814 | `}` |
|        - | 1815 | `/*` |
|        - | 1816 | ` * [CAPIREF: ph7_array_add_elem()]` |
|        - | 1817 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1818 | ` */` |
|  8836818 | 1819 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|        5 | 1820 | `{` |
|        - | 1821 | `	int rc;` |
|        - | 1822 | `	/* Make sure we are dealing with a valid hashmap */` |
|  8836823 | 1823 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1824 | `		return PH7_CORRUPT;` |
|        - | 1825 | `	}` |
|        - | 1826 | `	/* Perform the insertion */` |
|  8836823 | 1827 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
|  8836823 | 1828 | `	return rc;` |
|  4415760 | 1829 | `}` |
|        - | 1830 | `/*` |
|        - | 1831 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|        - | 1832 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1833 | ` */` |
|  3387583 | 1834 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|        5 | 1835 | `{` |
|        - | 1836 | `	int rc;` |
|        - | 1837 | `	/* Make sure we are dealing with a valid hashmap */` |
|  3387588 | 1838 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1839 | `		return PH7_CORRUPT;` |
|        - | 1840 | `	}` |
|        - | 1841 | `	/* Perform the insertion */` |
|  3387588 | 1842 | `	if( SX_EMPTY_STR(zKey) ){` |
|        - | 1843 | `		/* Empty key,assign an automatic index */` |
|       17 | 1844 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|        9 | 1845 | `	}else{` |
|        - | 1846 | `		ph7_value sKey;` |
|  3387572 | 1847 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|  3387572 | 1848 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|  3387572 | 1849 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  3387572 | 1850 | `		PH7_MemObjRelease(&sKey);` |
|        - | 1851 | `	}` |
|  3387588 | 1852 | `	return rc;` |
|  1693228 | 1853 | `}` |
|        - | 1854 | `/*` |
|        - | 1855 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|        - | 1856 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1857 | ` */` |
|  2129584 | 1858 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|        5 | 1859 | `{` |
|        - | 1860 | `	ph7_value sKey;` |
|        - | 1861 | `	int rc;` |
|        - | 1862 | `	/* Make sure we are dealing with a valid hashmap */` |
|  2129589 | 1863 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1864 | `		return PH7_CORRUPT;` |
|        - | 1865 | `	}` |
|  2129589 | 1866 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|        - | 1867 | `	/* Perform the insertion */` |
|  2129589 | 1868 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  2129589 | 1869 | `	PH7_MemObjRelease(&sKey);` |
|  2129589 | 1870 | `	return rc;` |
|  1064797 | 1871 | `}` |
|        - | 1872 | `/*` |
|        - | 1873 | ` * [CAPIREF: ph7_array_count()]` |
|        - | 1874 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1875 | ` */` |
|  5802334 | 1876 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|        5 | 1877 | `{` |
|        - | 1878 | `	ph7_hashmap *pMap;` |
|        - | 1879 | `	/* Make sure we are dealing with a valid hashmap */` |
|  5802339 | 1880 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1881 | `		return 0;` |
|        - | 1882 | `	}` |
|        - | 1883 | `	/* Point to the internal representation of the hashmap */` |
|  5802339 | 1884 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  5802339 | 1885 | `	return pMap->nEntry;` |
|  2901170 | 1886 | `}` |
|        - | 1887 | `/*` |
|        - | 1888 | ` * [CAPIREF: ph7_object_walk()]` |
|        - | 1889 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1890 | ` */` |
|      ! 0 | 1891 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|      ! 0 | 1892 | `{` |
|        - | 1893 | `	int rc;` |
|      ! 0 | 1894 | `	if( xWalk == 0 ){` |
|      ! 0 | 1895 | `		return PH7_CORRUPT;` |
|        - | 1896 | `	}` |
|        - | 1897 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1898 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 1899 | `		return PH7_CORRUPT;` |
|        - | 1900 | `	}` |
|        - | 1901 | `	/* Start the walk process */` |
|      ! 0 | 1902 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|      ! 0 | 1903 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|      ! 0 | 1904 | `}` |
|        - | 1905 | `/*` |
|        - | 1906 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|        - | 1907 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1908 | ` */` |
|      ! 0 | 1909 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|      ! 0 | 1910 | `{` |
|        - | 1911 | `	ph7_value *pValue;` |
|        - | 1912 | `	SyString sAttr;` |
|        - | 1913 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1914 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|      ! 0 | 1915 | `		return 0;` |
|        - | 1916 | `	}` |
|      ! 0 | 1917 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|        - | 1918 | `	/* Extract the attribute value if available.` |
|        - | 1919 | `	 */` |
|      ! 0 | 1920 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|      ! 0 | 1921 | `	return pValue;` |
|      ! 0 | 1922 | `}` |
|        - | 1923 | `/*` |
|        - | 1924 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|        - | 1925 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1926 | ` */` |
|      ! 0 | 1927 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|      ! 0 | 1928 | `{` |
|        - | 1929 | `	ph7_class *pClass;` |
|      ! 0 | 1930 | `	if( pLength ){` |
|      ! 0 | 1931 | `		*pLength = 0;` |
|      ! 0 | 1932 | `	}` |
|        - | 1933 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1934 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|      ! 0 | 1935 | `		return 0;` |
|        - | 1936 | `	}` |
|        - | 1937 | `	/* Point to the class */` |
|      ! 0 | 1938 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|        - | 1939 | `	/* Return the class name */` |
|      ! 0 | 1940 | `	if( pLength ){` |
|      ! 0 | 1941 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|      ! 0 | 1942 | `	}` |
|      ! 0 | 1943 | `	return SyStringData(&pClass->sName);` |
|      ! 0 | 1944 | `}` |
|        - | 1945 | `/*` |
|        - | 1946 | ` * [CAPIREF: ph7_context_output()]` |
|        - | 1947 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1948 | ` */` |
|   178777 | 1949 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|        5 | 1950 | `{` |
|        - | 1951 | `	SyString sData;` |
|        - | 1952 | `	int rc;` |
|   178782 | 1953 | `	if( nLen < 0 ){` |
|      ! 0 | 1954 | `		nLen = (int)SyStrlen(zString);` |
|      ! 0 | 1955 | `	}` |
|   178782 | 1956 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|   178782 | 1957 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|   178782 | 1958 | `	return rc;` |
|        5 | 1959 | `}` |
|        - | 1960 | `/*` |
|        - | 1961 | ` * [CAPIREF: ph7_context_output_format()]` |
|        - | 1962 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1963 | ` */` |
|       30 | 1964 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|        2 | 1965 | `{` |
|        - | 1966 | `	va_list ap;` |
|        - | 1967 | `	int rc;` |
|       32 | 1968 | `	va_start(ap,zFormat);` |
|       32 | 1969 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       32 | 1970 | `	va_end(ap);` |
|       32 | 1971 | `	return rc;` |
|        2 | 1972 | `}` |
|        - | 1973 | `/*` |
|        - | 1974 | ` * [CAPIREF: ph7_context_throw_error()]` |
|        - | 1975 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1976 | ` */` |
|      580 | 1977 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|        5 | 1978 | `{` |
|      585 | 1979 | `	int rc = PH7_OK;` |
|      585 | 1980 | `	if( zErr ){` |
|      585 | 1981 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      290 | 1982 | `	}` |
|      585 | 1983 | `	return rc;` |
|        5 | 1984 | `}` |
|        - | 1985 | `/*` |
|        - | 1986 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|        - | 1987 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1988 | ` */` |
|     1275 | 1989 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|        5 | 1990 | `{` |
|        - | 1991 | `	va_list ap;` |
|        - | 1992 | `	int rc;` |
|     1280 | 1993 | `	if( zFormat == 0){` |
|      ! 0 | 1994 | `		return PH7_OK;` |
|        - | 1995 | `	}` |
|     1280 | 1996 | `	va_start(ap,zFormat);` |
|     1280 | 1997 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|     1280 | 1998 | `	va_end(ap);` |
|     1280 | 1999 | `	return rc;` |
|      637 | 2000 | `}` |
|        - | 2001 | `/*` |
|        - | 2002 | ` * [CAPIREF: ph7_context_random_num()]` |
|        - | 2003 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2004 | ` */` |
|      ! 0 | 2005 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|      ! 0 | 2006 | `{` |
|        - | 2007 | `	sxu32 n;` |
|      ! 0 | 2008 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      ! 0 | 2009 | `	return n;` |
|      ! 0 | 2010 | `}` |
|        - | 2011 | `/*` |
|        - | 2012 | ` * [CAPIREF: ph7_context_random_string()]` |
|        - | 2013 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2014 | ` */` |
|      ! 0 | 2015 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|      ! 0 | 2016 | `{` |
|      ! 0 | 2017 | `	if( nBuflen < 3 ){` |
|      ! 0 | 2018 | `		return PH7_CORRUPT;` |
|        - | 2019 | `	}` |
|      ! 0 | 2020 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|      ! 0 | 2021 | `	return PH7_OK;` |
|      ! 0 | 2022 | `}` |
|        - | 2023 | `/*` |
|        - | 2024 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|        - | 2025 | ` *` |
|        - | 2026 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|        - | 2027 | ` * {` |
|        - | 2028 | ` *	return pCtx->pVm;` |
|        - | 2029 | ` * }` |
|        - | 2030 | ` */` |
|        - | 2031 | `/*` |
|        - | 2032 | ` * [CAPIREF: ph7_context_user_data()]` |
|        - | 2033 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2034 | ` */` |
|   106064 | 2035 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|        5 | 2036 | `{` |
|   106069 | 2037 | `	return pCtx->pFunc->pUserData;` |
|        5 | 2038 | `}` |
|        - | 2039 | `/*` |
|        - | 2040 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|        - | 2041 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2042 | ` */` |
|      216 | 2043 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|        2 | 2044 | `{` |
|        - | 2045 | `	ph7_aux_data sAux;` |
|        - | 2046 | `	int rc;` |
|      218 | 2047 | `	sAux.pAuxData = pUserData;` |
|      218 | 2048 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|      218 | 2049 | `	return rc;` |
|        2 | 2050 | `}` |
|        - | 2051 | `/*` |
|        - | 2052 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|        - | 2053 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2054 | ` */` |
|        4 | 2055 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|        1 | 2056 | `{` |
|        - | 2057 | `	ph7_aux_data *pAux;` |
|        5 | 2058 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|        5 | 2059 | `	return pAux ? pAux->pAuxData : 0;` |
|        1 | 2060 | `}` |
|        - | 2061 | `/*` |
|        - | 2062 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|        - | 2063 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2064 | ` */` |
|      ! 0 | 2065 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|      ! 0 | 2066 | `{` |
|        - | 2067 | `	ph7_aux_data *pAux;` |
|      ! 0 | 2068 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|      ! 0 | 2069 | `	return pAux ? pAux->pAuxData : 0;` |
|      ! 0 | 2070 | `}` |
|        - | 2071 | `/*` |
|        - | 2072 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|        - | 2073 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2074 | ` */` |
|    78529 | 2075 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|        5 | 2076 | `{` |
|    78534 | 2077 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|        5 | 2078 | `}` |
|        - | 2079 | `/*` |
|        - | 2080 | ` * [CAPIREF: ph7_function_name()]` |
|        - | 2081 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2082 | ` */` |
|   237148 | 2083 | `const char * ph7_function_name(ph7_context *pCtx)` |
|        5 | 2084 | `{` |
|        - | 2085 | `	SyString *pName;` |
|   237153 | 2086 | `	pName = &pCtx->pFunc->sName;` |
|   237153 | 2087 | `	return pName->zString;` |
|        5 | 2088 | `}` |
|        - | 2089 | `/*` |
|        - | 2090 | ` * [CAPIREF: ph7_value_int()]` |
|        - | 2091 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2092 | ` */` |
|  5676527 | 2093 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|        5 | 2094 | `{` |
|        - | 2095 | `	/* Invalidate any prior representation */` |
|  5676532 | 2096 | `	PH7_MemObjRelease(pVal);` |
|  5676532 | 2097 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|  5676532 | 2098 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  5676532 | 2099 | `	return PH7_OK;` |
|        5 | 2100 | `}` |
|        - | 2101 | `/*` |
|        - | 2102 | ` * [CAPIREF: ph7_value_int64()]` |
|        - | 2103 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2104 | ` */` |
|  2239884 | 2105 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|        5 | 2106 | `{` |
|        - | 2107 | `	/* Invalidate any prior representation */` |
|  2239889 | 2108 | `	PH7_MemObjRelease(pVal);` |
|  2239889 | 2109 | `	pVal->x.iVal = iValue;` |
|  2239889 | 2110 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  2239889 | 2111 | `	return PH7_OK;` |
|        5 | 2112 | `}` |
|        - | 2113 | `/*` |
|        - | 2114 | ` * [CAPIREF: ph7_value_bool()]` |
|        - | 2115 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2116 | ` */` |
|   782360 | 2117 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|        5 | 2118 | `{` |
|        - | 2119 | `	/* Invalidate any prior representation */` |
|   782365 | 2120 | `	PH7_MemObjRelease(pVal);` |
|   782365 | 2121 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|   782365 | 2122 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|   782365 | 2123 | `	return PH7_OK;` |
|        5 | 2124 | `}` |
|        - | 2125 | `/*` |
|        - | 2126 | ` * [CAPIREF: ph7_value_null()]` |
|        - | 2127 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2128 | ` */` |
|     2362 | 2129 | `int ph7_value_null(ph7_value *pVal)` |
|        5 | 2130 | `{` |
|        - | 2131 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     2367 | 2132 | `	PH7_MemObjRelease(pVal);` |
|     2367 | 2133 | `	return PH7_OK;` |
|        5 | 2134 | `}` |
|        - | 2135 | `/*` |
|        - | 2136 | ` * [CAPIREF: ph7_value_double()]` |
|        - | 2137 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2138 | ` */` |
|     4079 | 2139 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|        5 | 2140 | `{` |
|        - | 2141 | `	/* Invalidate any prior representation */` |
|     4084 | 2142 | `	PH7_MemObjRelease(pVal);` |
|     4084 | 2143 | `	pVal->rVal = (ph7_real)Value;` |
|     4084 | 2144 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|        - | 2145 | `	/* Try to get an integer representation also */` |
|     4084 | 2146 | `	PH7_MemObjTryInteger(pVal);` |
|     4084 | 2147 | `	return PH7_OK;` |
|        5 | 2148 | `}` |
|        - | 2149 | `/*` |
|        - | 2150 | ` * [CAPIREF: ph7_value_string()]` |
|        - | 2151 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2152 | ` */` |
| 21931799 | 2153 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|        5 | 2154 | `{` |
| 21931804 | 2155 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2156 | `		/* Invalidate any prior representation */` |
| 10970336 | 2157 | `		PH7_MemObjRelease(pVal);` |
| 10970336 | 2158 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  5485202 | 2159 | `	}` |
| 21931804 | 2160 | `	if( zString ){` |
| 21909465 | 2161 | `		if( nLen < 0 ){` |
|        - | 2162 | `			/* Compute length automatically */` |
|   152654 | 2163 | `			nLen = (int)SyStrlen(zString);` |
|    76266 | 2164 | `		}` |
|        - | 2165 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|        - | 2166 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
| 21909465 | 2167 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|        - | 2168 | `	}` |
|    22344 | 2169 | `	return PH7_OK;` |
| 10958043 | 2170 | `}` |
|        - | 2171 | `/*` |
|        - | 2172 | ` * [CAPIREF: ph7_value_string_format()]` |
|        - | 2173 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2174 | ` */` |
|    11084 | 2175 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|        5 | 2176 | `{` |
|        - | 2177 | `	va_list ap;` |
|        - | 2178 | `	int rc;` |
|    11089 | 2179 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2180 | `		/* Invalidate any prior representation */` |
|    11053 | 2181 | `		PH7_MemObjRelease(pVal);` |
|    11053 | 2182 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|     5524 | 2183 | `	}` |
|    11089 | 2184 | `	va_start(ap,zFormat);` |
|    11089 | 2185 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|    11089 | 2186 | `	va_end(ap);` |
|        - | 2187 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|    11089 | 2188 | `	return rc;` |
|        5 | 2189 | `}` |
|        - | 2190 | `/*` |
|        - | 2191 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|        - | 2192 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2193 | ` */` |
|  8316361 | 2194 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|        5 | 2195 | `{` |
|        - | 2196 | `	/* Reset the string cursor */` |
|  8316366 | 2197 | `	SyBlobReset(&pVal->sBlob);` |
|  8316366 | 2198 | `	return PH7_OK;` |
|        5 | 2199 | `}` |
|        - | 2200 | `/*` |
|        - | 2201 | ` * [CAPIREF: ph7_value_resource()]` |
|        - | 2202 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2203 | ` */` |
|   105347 | 2204 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|        5 | 2205 | `{` |
|        - | 2206 | `	/* Invalidate any prior representation */` |
|   105352 | 2207 | `	PH7_MemObjRelease(pVal);` |
|        - | 2208 | `	/* Reflect the new type */` |
|   105352 | 2209 | `	pVal->x.pOther = pUserData;` |
|   105352 | 2210 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|        - | 2211 | `	/* A STREAM handle is counted, and the value carries the mark that says so --` |
|        - | 2212 | `	 * see MEMOBJ_STREAMRES. Asking the question HERE is safe because the pointer` |
|        - | 2213 | `	 * has just been handed over; asking it at release time is not. */` |
|   105352 | 2214 | `	if( PH7_StreamValueRef(pUserData) ){` |
|   104454 | 2215 | `		pVal->iFlags \|= MEMOBJ_STREAMRES;` |
|    52155 | 2216 | `	}` |
|   105352 | 2217 | `	return PH7_OK;` |
|        5 | 2218 | `}` |
|        - | 2219 | `/*` |
|        - | 2220 | ` * [CAPIREF: ph7_value_release()]` |
|        - | 2221 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2222 | ` */` |
|      ! 0 | 2223 | `int ph7_value_release(ph7_value *pVal)` |
|      ! 0 | 2224 | `{` |
|      ! 0 | 2225 | `	PH7_MemObjRelease(pVal);` |
|      ! 0 | 2226 | `	return PH7_OK;` |
|      ! 0 | 2227 | `}` |
|        - | 2228 | `/*` |
|        - | 2229 | ` * [CAPIREF: ph7_value_is_int()]` |
|        - | 2230 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2231 | ` */` |
|  1487603 | 2232 | `int ph7_value_is_int(ph7_value *pVal)` |
|        5 | 2233 | `{` |
|        - | 2234 | `	/* TRUE whenever an integer representation is available, including an` |
|        - | 2235 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|        - | 2236 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|        - | 2237 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|        - | 2238 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|  1487608 | 2239 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|        5 | 2240 | `}` |
|        - | 2241 | `/*` |
|        - | 2242 | ` * [CAPIREF: ph7_value_is_float()]` |
|        - | 2243 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2244 | ` */` |
|  2202505 | 2245 | `int ph7_value_is_float(ph7_value *pVal)` |
|        5 | 2246 | `{` |
|  2202510 | 2247 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|        5 | 2248 | `}` |
|        - | 2249 | `/*` |
|        - | 2250 | ` * [CAPIREF: ph7_value_is_bool()]` |
|        - | 2251 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2252 | ` */` |
|    81256 | 2253 | `int ph7_value_is_bool(ph7_value *pVal)` |
|        5 | 2254 | `{` |
|    81261 | 2255 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|        5 | 2256 | `}` |
|        - | 2257 | `/*` |
|        - | 2258 | ` * [CAPIREF: ph7_value_is_string()]` |
|        - | 2259 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2260 | ` */` |
|  1647829 | 2261 | `int ph7_value_is_string(ph7_value *pVal)` |
|        5 | 2262 | `{` |
|  1647834 | 2263 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|        5 | 2264 | `}` |
|        - | 2265 | `/*` |
|        - | 2266 | ` * [CAPIREF: ph7_value_is_null()]` |
|        - | 2267 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2268 | ` */` |
| 15418966 | 2269 | `int ph7_value_is_null(ph7_value *pVal)` |
|        5 | 2270 | `{` |
| 15418971 | 2271 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|        5 | 2272 | `}` |
|        - | 2273 | `/*` |
|        - | 2274 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|        - | 2275 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2276 | ` */` |
|    23964 | 2277 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|        5 | 2278 | `{` |
|        - | 2279 | `	int rc;` |
|    23969 | 2280 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|    23969 | 2281 | `	return rc;` |
|        5 | 2282 | `}` |
|        - | 2283 | `/*` |
|        - | 2284 | ` * [CAPIREF: ph7_value_is_callable()]` |
|        - | 2285 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2286 | ` */` |
|    26290 | 2287 | `int ph7_value_is_callable(ph7_value *pVal)` |
|        5 | 2288 | `{` |
|        - | 2289 | `	int rc;` |
|    26295 | 2290 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|    26295 | 2291 | `	return rc;` |
|        5 | 2292 | `}` |
|        - | 2293 | `/*` |
|        - | 2294 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|        - | 2295 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2296 | ` */` |
|       30 | 2297 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|        1 | 2298 | `{` |
|       31 | 2299 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|        1 | 2300 | `}` |
|        - | 2301 | `/*` |
|        - | 2302 | ` * [CAPIREF: ph7_value_is_array()]` |
|        - | 2303 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2304 | ` */` |
|  1148661 | 2305 | `int ph7_value_is_array(ph7_value *pVal)` |
|        5 | 2306 | `{` |
|  1148666 | 2307 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|        5 | 2308 | `}` |
|        - | 2309 | `/*` |
|        - | 2310 | ` * [CAPIREF: ph7_value_is_object()]` |
|        - | 2311 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2312 | ` */` |
|   324213 | 2313 | `int ph7_value_is_object(ph7_value *pVal)` |
|        5 | 2314 | `{` |
|   324218 | 2315 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|        5 | 2316 | `}` |
|        - | 2317 | `/*` |
|        - | 2318 | ` * [CAPIREF: ph7_value_is_resource()]` |
|        - | 2319 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2320 | ` */` |
|   367610 | 2321 | `int ph7_value_is_resource(ph7_value *pVal)` |
|        5 | 2322 | `{` |
|   367615 | 2323 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|        5 | 2324 | `}` |
|        - | 2325 | `/*` |
|        - | 2326 | ` * [CAPIREF: ph7_value_is_empty()]` |
|        - | 2327 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2328 | ` */` |
|    70974 | 2329 | `int ph7_value_is_empty(ph7_value *pVal)` |
|        5 | 2330 | `{` |
|        - | 2331 | `	int rc;` |
|    70979 | 2332 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|    70979 | 2333 | `	return rc;` |
|        5 | 2334 | `}` |
|        - | 2335 | `/*` |
|        - | 2336 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|        - | 2337 | ` * Check if a value holds a Fiber instance.` |
|        - | 2338 | ` */` |
|      ! 0 | 2339 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|      ! 0 | 2340 | `{` |
|      ! 0 | 2341 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|      ! 0 | 2342 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|      ! 0 | 2343 | `}` |
|        - | 2344 | `/*` |
|        - | 2345 | ` * [CAPIREF: ph7_fiber_start()]` |
|        - | 2346 | ` * Start a Fiber, passing arguments to the callable.` |
|        - | 2347 | ` */` |
|      ! 0 | 2348 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 2349 | `{` |
|      ! 0 | 2350 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|      ! 0 | 2351 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|      ! 0 | 2352 | `}` |
|        - | 2353 | `/*` |
|        - | 2354 | ` * [CAPIREF: ph7_fiber_resume()]` |
|        - | 2355 | ` * Resume a suspended Fiber, optionally sending a value.` |
|        - | 2356 | ` */` |
|      ! 0 | 2357 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 2358 | `{` |
|      ! 0 | 2359 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|      ! 0 | 2360 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|      ! 0 | 2361 | `}` |
|        - | 2362 | `/*` |
|        - | 2363 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|        - | 2364 | ` * Check if a Fiber is currently suspended.` |
|        - | 2365 | ` */` |
|      ! 0 | 2366 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|      ! 0 | 2367 | `{` |
|      ! 0 | 2368 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2369 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|      ! 0 | 2370 | `}` |
|        - | 2371 | `/*` |
|        - | 2372 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|        - | 2373 | ` * Check if a Fiber has completed execution.` |
|        - | 2374 | ` */` |
|      ! 0 | 2375 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|      ! 0 | 2376 | `{` |
|      ! 0 | 2377 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2378 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|      ! 0 | 2379 | `}` |
|        - | 2380 | `/*` |
|        - | 2381 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|        - | 2382 | ` * Get the return value of a terminated Fiber.` |
|        - | 2383 | ` * Returns NULL if the Fiber has not terminated.` |
|        - | 2384 | ` */` |
|      ! 0 | 2385 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|      ! 0 | 2386 | `{` |
|      ! 0 | 2387 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2388 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|      ! 0 | 2389 | `}` |
|        - | 2390 |  |
