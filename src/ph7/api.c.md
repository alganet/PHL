# src/ph7/api.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 857/1204 lines (71.18%)

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
|    35110 |   78 | `static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)` |
|        5 |   79 | `{` |
|    35115 |   80 | `	ph7_conf *pConf = &pEngine->xConf;` |
|    35115 |   81 | `	int rc = PH7_OK;` |
|        - |   82 | `	/* Perform the requested operation */` |
|    35115 |   83 | `	switch(nOp){` |
|     7930 |   84 | `	case PH7_CONFIG_ERR_OUTPUT: {` |
|    15843 |   85 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|    15843 |   86 | `		void *pUserData = va_arg(ap,void *);` |
|        - |   87 | `		/* Compile time error consumer routine */` |
|    15843 |   88 | `		if( xConsumer == 0 ){` |
|      ! 0 |   89 | `			rc = PH7_CORRUPT;` |
|      ! 0 |   90 | `			break;` |
|        - |   91 | `		}` |
|        - |   92 | `		/* Install the error consumer */` |
|    15843 |   93 | `		pConf->xErr     = xConsumer;` |
|    15843 |   94 | `		pConf->pErrData = pUserData;` |
|    15843 |   95 | `		break;` |
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
|     3957 |  145 | `	case PH7_CONFIG_OUTPUT: {` |
|        - |  146 | `		/* The program-output stream a compile diagnostic's DISPLAY copy takes` |
|        - |  147 | `		 * while no VM output consumer exists yet -- the whole of the main` |
|        - |  148 | `		 * script's own compile. */` |
|     7908 |  149 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     7908 |  150 | `		void *pUserData = va_arg(ap,void *);` |
|     7908 |  151 | `		pConf->xOut     = xConsumer;` |
|     7908 |  152 | `		pConf->pOutData = pUserData;` |
|     7908 |  153 | `		break;` |
|        - |  154 | `							}` |
|     3957 |  155 | `	case PH7_CONFIG_ERR_REPORT:` |
|        - |  156 | `		/* Seed the reporting level of VMs created afterwards. The VM-level verb` |
|        - |  157 | `		 * of the same name can only reach a VM that already exists, i.e. after` |
|        - |  158 | `		 * the unit it was meant to gate has finished compiling. */` |
|     7908 |  159 | `		pConf->bErrReport = 1;` |
|     7908 |  160 | `		break;` |
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
|     1477 |  181 | `	case PH7_CONFIG_INI_ENTRY: {` |
|        - |  182 | `		/* A php.ini directive applied to every VM at birth. php reads php.ini` |
|        - |  183 | `		 * before it compiles anything, so a directive a diagnostic is gated by` |
|        - |  184 | `		 * (display_errors, log_errors, error_reporting) has to be in hand before` |
|        - |  185 | `		 * ph7_compile_file -- which is the call that CREATES the VM. Copies live` |
|        - |  186 | `		 * on the ENGINE allocator: they outlive every VM replaying them. */` |
|     2956 |  187 | `		const char *zName = va_arg(ap,const char *);` |
|     2956 |  188 | `		const char *zValue = va_arg(ap,const char *);` |
|     2956 |  189 | `		const char *zFile = va_arg(ap,const char *);` |
|     2956 |  190 | `		unsigned int nLine = va_arg(ap,unsigned int);` |
|     2956 |  191 | `		int iStop = va_arg(ap,int);` |
|        - |  192 | `		VmIniEntry sEntry;` |
|        - |  193 | `		char *zDupN,*zDupV,*zDupF;` |
|        - |  194 | `		sxu32 nName,nValue,nFile;` |
|        - |  195 | ``		/* An unclosed `[` is queued in place of a directive and carries no name:`` |
|        - |  196 | `		 * it is the source refusing itself, not an entry to apply. */` |
|     2956 |  197 | `		if( SX_EMPTY_STR(zName) && iStop < PH7_INI_STOP_SECTION ){` |
|      ! 0 |  198 | `			rc = PH7_CORRUPT;` |
|      ! 0 |  199 | `			break;` |
|        - |  200 | `		}` |
|     2956 |  201 | `		if( zName == 0 ){` |
|      ! 0 |  202 | `			zName = "";` |
|      ! 0 |  203 | `		}` |
|     2956 |  204 | `		if( zValue == 0 ){` |
|      ! 0 |  205 | `			zValue = "";` |
|      ! 0 |  206 | `		}` |
|     2956 |  207 | `		if( zFile == 0 ){` |
|      ! 0 |  208 | `			zFile = "";` |
|      ! 0 |  209 | `		}` |
|     2956 |  210 | `		nName  = (sxu32)SyStrlen(zName);` |
|     2956 |  211 | `		nValue = (sxu32)SyStrlen(zValue);` |
|     2956 |  212 | `		nFile  = (sxu32)SyStrlen(zFile);` |
|     2956 |  213 | `		zDupN = SyMemBackendStrDup(&pEngine->sAllocator,zName,nName);` |
|     2956 |  214 | `		zDupV = SyMemBackendStrDup(&pEngine->sAllocator,zValue,nValue);` |
|     2956 |  215 | `		zDupF = nFile > 0 ? SyMemBackendStrDup(&pEngine->sAllocator,zFile,nFile) : 0;` |
|     2956 |  216 | `		if( zDupN == 0 \|\| zDupV == 0 \|\| (nFile > 0 && zDupF == 0) ){` |
|      ! 0 |  217 | `			rc = PH7_NOMEM;` |
|      ! 0 |  218 | `			break;` |
|        - |  219 | `		}` |
|     2956 |  220 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|     2956 |  221 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|     2956 |  222 | `		SyStringInitFromBuf(&sEntry.sFile,zDupF,nFile);` |
|     2956 |  223 | `		sEntry.nLine = (sxu32)nLine;` |
|     2956 |  224 | `		sEntry.iStop = iStop;` |
|     2956 |  225 | `		if( SySetPut(&pConf->aIniEntry,(const void *)&sEntry) != SXRET_OK ){` |
|      ! 0 |  226 | `			rc = PH7_NOMEM;` |
|      ! 0 |  227 | `		}` |
|     2956 |  228 | `		break;` |
|        - |  229 | `								}` |
|      ! 0 |  230 | `	default:` |
|        - |  231 | `		/* Unknown configuration verb */` |
|      ! 0 |  232 | `		rc = PH7_CORRUPT;` |
|      ! 0 |  233 | `		break;` |
|        - |  234 | `	} /* Switch() */` |
|    35115 |  235 | `	return rc;` |
|        5 |  236 | `}` |
|        - |  237 | `/*` |
|        - |  238 | ` * Configure the PH7 library.` |
|        - |  239 | ` * return PH7_OK on success.Any other return value` |
|        - |  240 | ` * indicates failure.` |
|        - |  241 | ` * Refer to [ph7_lib_config()].` |
|        - |  242 | ` */` |
|    23805 |  243 | `static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)` |
|        5 |  244 | `{` |
|    23810 |  245 | `	int rc = PH7_OK;` |
|    23810 |  246 | `	switch(nOp){` |
|     3973 |  247 | `	    case PH7_LIB_CONFIG_VFS:{` |
|        - |  248 | `			/* Install a virtual file system */` |
|     7940 |  249 | `			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);` |
|     7940 |  250 | `			sMPGlobal.pVfs = pVfs;` |
|     7940 |  251 | `			break;` |
|        - |  252 | `								}` |
|     3973 |  253 | `		case PH7_LIB_CONFIG_USER_MALLOC: {` |
|        - |  254 | `			/* Use an alternative low-level memory allocation routines */` |
|     7940 |  255 | `			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);` |
|        - |  256 | `			/* Save the memory failure callback (if available) */` |
|     7940 |  257 | `			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;` |
|     7940 |  258 | `			void *pMemErr = sMPGlobal.sAllocator.pUserData;` |
|     7940 |  259 | `			if( pMethods == 0 ){` |
|        - |  260 | `				/* Use the built-in memory allocation subsystem */` |
|     7940 |  261 | `				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);` |
|     3967 |  262 | `			}else{` |
|      ! 0 |  263 | `				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);` |
|        - |  264 | `			}` |
|     7940 |  265 | `			break;` |
|        - |  266 | `										  }` |
|      ! 0 |  267 | `		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {` |
|        - |  268 | `			/* Memory failure callback */` |
|      ! 0 |  269 | `			ProcMemError xMemErr = va_arg(ap,ProcMemError);` |
|      ! 0 |  270 | `			void *pUserData = va_arg(ap,void *);` |
|      ! 0 |  271 | `			sMPGlobal.sAllocator.xMemError = xMemErr;` |
|      ! 0 |  272 | `			sMPGlobal.sAllocator.pUserData = pUserData;` |
|      ! 0 |  273 | `			break;` |
|        - |  274 | `												 }` |
|     3973 |  275 | `		case PH7_LIB_CONFIG_USER_MUTEX: {` |
|        - |  276 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  277 | `			/* Use an alternative low-level mutex subsystem */` |
|     7940 |  278 | `			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);` |
|        - |  279 | `#if defined (UNTRUST)` |
|        - |  280 | `			if( pMethods == 0 ){` |
|        - |  281 | `				rc = PH7_CORRUPT;` |
|        - |  282 | `			}` |
|        - |  283 | `#endif` |
|        - |  284 | `			/* Sanity check */` |
|     7940 |  285 | `			if( pMethods->xEnter == 0 \|\| pMethods->xLeave == 0 \|\| pMethods->xNew == 0){` |
|        - |  286 | `				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */` |
|      ! 0 |  287 | `				rc = PH7_CORRUPT;` |
|      ! 0 |  288 | `				break;` |
|        - |  289 | `			}` |
|     7940 |  290 | `			if( sMPGlobal.pMutexMethods ){` |
|        - |  291 | `				/* Overwrite the previous mutex subsystem */` |
|      ! 0 |  292 | `				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|      ! 0 |  293 | `				if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|      ! 0 |  294 | `					sMPGlobal.pMutexMethods->xGlobalRelease();` |
|      ! 0 |  295 | `				}` |
|      ! 0 |  296 | `				sMPGlobal.pMutex = 0;` |
|      ! 0 |  297 | `			}` |
|        - |  298 | `			/* Initialize and install the new mutex subsystem */` |
|     7940 |  299 | `			if( pMethods->xGlobalInit ){` |
|        5 |  300 | `				rc = pMethods->xGlobalInit();` |
|        5 |  301 | `				if ( rc != PH7_OK ){` |
|      ! 0 |  302 | `					break;` |
|        - |  303 | `				}` |
|      ! 0 |  304 | `			}` |
|        - |  305 | `			/* Create the global mutex */` |
|     7940 |  306 | `			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|     7940 |  307 | `			if( sMPGlobal.pMutex == 0 ){` |
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
|     7940 |  318 | `			sMPGlobal.pMutexMethods = pMethods;` |
|     7940 |  319 | `			if( sMPGlobal.nThreadingLevel == 0 ){` |
|        - |  320 | `				/* Set a default threading level */` |
|     7940 |  321 | `				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;` |
|     3962 |  322 | `			}` |
|        - |  323 | `#endif` |
|     7940 |  324 | `			break;` |
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
|    23810 |  345 | `	return rc;` |
|        5 |  346 | `}` |
|        - |  347 | `/*` |
|        - |  348 | ` * [CAPIREF: ph7_lib_config()]` |
|        - |  349 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  350 | ` */` |
|    23805 |  351 | `int ph7_lib_config(int nConfigOp,...)` |
|        5 |  352 | `{` |
|        - |  353 | `	va_list ap;` |
|        - |  354 | `	int rc;` |
|        - |  355 |  |
|    23810 |  356 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|        - |  357 | `		/* Library is already initialized,this operation is forbidden */` |
|      ! 0 |  358 | `		return PH7_LOOKED;` |
|        - |  359 | `	}` |
|    23810 |  360 | `	va_start(ap,nConfigOp);` |
|    23810 |  361 | `	rc = PH7CoreConfigure(nConfigOp,ap);` |
|    23810 |  362 | `	va_end(ap);` |
|    23810 |  363 | `	return rc;` |
|    11891 |  364 | `}` |
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
|     7935 |  375 | `static sxi32 PH7CoreInitialize(void)` |
|        5 |  376 | `{` |
|        - |  377 | `	const ph7_vfs *pVfs; /* Built-in vfs */` |
|        - |  378 | `#if defined(PH7_ENABLE_THREADS)` |
|     7940 |  379 | `	const SyMutexMethods *pMutexMethods = 0;` |
|     7940 |  380 | `	SyMutex *pMaster = 0;` |
|        - |  381 | `#endif` |
|        - |  382 | `	int rc;` |
|        - |  383 | `	/*` |
|        - |  384 | `	 * If the library is already initialized,then a call to this routine` |
|        - |  385 | `	 * is a no-op.` |
|        - |  386 | `	 */` |
|     7940 |  387 | `	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){` |
|      ! 0 |  388 | `		return PH7_OK; /* Already initialized */` |
|        - |  389 | `	}` |
|        - |  390 | `	/* Point to the built-in vfs */` |
|     7940 |  391 | `	pVfs = PH7_ExportBuiltinVfs();` |
|        - |  392 | `	/* Install it */` |
|     7940 |  393 | `	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);` |
|        - |  394 | `#if defined(PH7_ENABLE_THREADS)` |
|     7940 |  395 | `	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){` |
|     7940 |  396 | `		pMutexMethods = sMPGlobal.pMutexMethods;` |
|     7940 |  397 | `		if( pMutexMethods == 0 ){` |
|        - |  398 | `			/* Use the built-in mutex subsystem */` |
|     7940 |  399 | `			pMutexMethods = SyMutexExportMethods();` |
|     7940 |  400 | `			if( pMutexMethods == 0 ){` |
|      ! 0 |  401 | `				return PH7_CORRUPT; /* Can't happen */` |
|        - |  402 | `			}` |
|        - |  403 | `			/* Install the mutex subsystem */` |
|     7940 |  404 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);` |
|     7940 |  405 | `			if( rc != PH7_OK ){` |
|      ! 0 |  406 | `				return rc;` |
|        - |  407 | `			}` |
|     3962 |  408 | `		}` |
|        - |  409 | `		/* Obtain a static mutex so we can initialize the library without calling malloc() */` |
|     7940 |  410 | `		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);` |
|     7940 |  411 | `		if( pMaster == 0 ){` |
|      ! 0 |  412 | `			return PH7_CORRUPT; /* Can't happen */` |
|        - |  413 | `		}` |
|     3962 |  414 | `	}` |
|        - |  415 | `	/* Lock the master mutex */` |
|     7940 |  416 | `	rc = PH7_OK;` |
|     7940 |  417 | `	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|    11902 |  418 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|        - |  419 | `#endif` |
|     7940 |  420 | `		if( sMPGlobal.sAllocator.pMethods == 0 ){` |
|        - |  421 | `			/* Install a memory subsystem */` |
|     7940 |  422 | `			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */` |
|     7940 |  423 | `			if( rc != PH7_OK ){` |
|        - |  424 | `				/* If we are unable to initialize the memory backend,there is no much we can do here.*/` |
|      ! 0 |  425 | `				goto End;` |
|        - |  426 | `			}` |
|     3962 |  427 | `		}` |
|        - |  428 | `#if defined(PH7_ENABLE_THREADS)` |
|     7940 |  429 | `		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  430 | `			/* Protect the memory allocation subsystem */` |
|     7940 |  431 | `			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);` |
|     7940 |  432 | `			if( rc != PH7_OK ){` |
|      ! 0 |  433 | `				goto End;` |
|        - |  434 | `			}` |
|     3962 |  435 | `		}` |
|        - |  436 | `#endif` |
|        - |  437 | `		/* Our library is initialized,set the magic number */` |
|     7940 |  438 | `		sMPGlobal.nMagic = PH7_LIB_MAGIC;` |
|     7940 |  439 | `		rc = PH7_OK;` |
|        - |  440 | `#if defined(PH7_ENABLE_THREADS)` |
|     3962 |  441 | `	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */` |
|        - |  442 | `#endif` |
|      ! 0 |  443 | `End:` |
|        - |  444 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  445 | `	/* Unlock the master mutex */` |
|     7940 |  446 | `	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  447 | `#endif` |
|     7940 |  448 | `	return rc;` |
|     3967 |  449 | `}` |
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
|     7957 |  463 | `static sxi32 EngineRelease(ph7 *pEngine)` |
|        5 |  464 | `{` |
|        - |  465 | `	ph7_vm *pVm,*pNext;` |
|        - |  466 | `	/* Release all active VM */` |
|     7962 |  467 | `	pVm = pEngine->pVms;` |
|     3973 |  468 | `	for(;;){` |
|     7962 |  469 | `		if( pEngine->iVm <= 0 ){` |
|     7962 |  470 | `			break;` |
|        - |  471 | `		}` |
|      ! 0 |  472 | `		pNext = pVm->pNext;` |
|      ! 0 |  473 | `		PH7_VmRelease(pVm);` |
|      ! 0 |  474 | `		pVm = pNext;` |
|      ! 0 |  475 | `		pEngine->iVm--;` |
|      ! 0 |  476 | `	}` |
|        - |  477 | `	/* Set a dummy magic number */` |
|     7962 |  478 | `	pEngine->nMagic = 0x7635;` |
|        - |  479 | `	/* Release the private memory subsystem */` |
|     7962 |  480 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|     7962 |  481 | `	return PH7_OK;` |
|        5 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * Release all resources consumed by the library.` |
|        - |  485 | ` * If PH7 is already shut down when this routine` |
|        - |  486 | ` * is invoked then this routine is a harmless no-op.` |
|        - |  487 | ` * Note: This call is not thread safe.` |
|        - |  488 | ` * Refer to [ph7_lib_shutdown()].` |
|        - |  489 | ` */` |
|     1020 |  490 | `static void PH7CoreShutdown(void)` |
|        4 |  491 | `{` |
|        - |  492 | `	ph7 *pEngine,*pNext;` |
|        - |  493 | `	/* Release all active engines first */` |
|     1024 |  494 | `	pEngine = sMPGlobal.pEngines;` |
|      510 |  495 | `	for(;;){` |
|     1024 |  496 | `		if( sMPGlobal.nEngine < 1 ){` |
|     1024 |  497 | `			break;` |
|        - |  498 | `		}` |
|      ! 0 |  499 | `		pNext = pEngine->pNext;` |
|      ! 0 |  500 | `		EngineRelease(pEngine);` |
|      ! 0 |  501 | `		pEngine = pNext;` |
|      ! 0 |  502 | `		sMPGlobal.nEngine--;` |
|      ! 0 |  503 | `	}` |
|        - |  504 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  505 | `	/* Release the mutex subsystem */` |
|     1024 |  506 | `	if( sMPGlobal.pMutexMethods ){` |
|     1024 |  507 | `		if( sMPGlobal.pMutex ){` |
|     1024 |  508 | `			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);` |
|     1024 |  509 | `			sMPGlobal.pMutex = 0;` |
|      510 |  510 | `		}` |
|     1024 |  511 | `		if( sMPGlobal.pMutexMethods->xGlobalRelease ){` |
|        4 |  512 | `			sMPGlobal.pMutexMethods->xGlobalRelease();` |
|      ! 0 |  513 | `		}` |
|     1024 |  514 | `		sMPGlobal.pMutexMethods = 0;` |
|      510 |  515 | `	}` |
|     1024 |  516 | `	sMPGlobal.nThreadingLevel = 0;` |
|        - |  517 | `#endif` |
|     1024 |  518 | `	if( sMPGlobal.sAllocator.pMethods ){` |
|        - |  519 | `		/* Release the memory backend */` |
|     1024 |  520 | `		SyMemBackendRelease(&sMPGlobal.sAllocator);` |
|      510 |  521 | `	}` |
|     1024 |  522 | `	sMPGlobal.nMagic = 0x1928;` |
|     1024 |  523 | `}` |
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
|     1020 |  597 | `int ph7_lib_shutdown(void)` |
|        4 |  598 | `{` |
|     1024 |  599 | `	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){` |
|        - |  600 | `		/* Already shut */` |
|      ! 0 |  601 | `		return PH7_OK;` |
|        - |  602 | `	}` |
|     1024 |  603 | `	PH7CoreShutdown();` |
|     1024 |  604 | `	return PH7_OK;` |
|      514 |  605 | `}` |
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
|      226 |  639 | `const char * ph7_lib_signature(void)` |
|        3 |  640 | `{` |
|      229 |  641 | `	return PH7_SIG;` |
|        3 |  642 | `}` |
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
|    35110 |  663 | `int ph7_config(ph7 *pEngine,int nConfigOp,...)` |
|        5 |  664 | `{` |
|        - |  665 | `	va_list ap;` |
|        - |  666 | `	int rc;` |
|    35115 |  667 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|      ! 0 |  668 | `		return PH7_CORRUPT;` |
|        - |  669 | `	}` |
|        - |  670 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  671 | `	 /* Acquire engine mutex */` |
|    35115 |  672 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|    35115 |  673 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|    35110 |  674 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  675 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  676 | `	 }` |
|        - |  677 | `#endif` |
|    35115 |  678 | `	 va_start(ap,nConfigOp);` |
|    35115 |  679 | `	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);` |
|    35115 |  680 | `	 va_end(ap);` |
|        - |  681 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  682 | `	 /* Leave engine mutex */` |
|    35115 |  683 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  684 | `#endif` |
|    35115 |  685 | `	return rc;` |
|    17537 |  686 | `}` |
|        - |  687 | `/*` |
|        - |  688 | ` * [CAPIREF: ph7_init()]` |
|        - |  689 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  690 | ` */` |
|     7935 |  691 | `int ph7_init(ph7 **ppEngine)` |
|        5 |  692 | `{` |
|        - |  693 | `	ph7 *pEngine;` |
|        - |  694 | `	int rc;` |
|        - |  695 | `#if defined(UNTRUST)` |
|        - |  696 | `	if( ppEngine == 0 ){` |
|        - |  697 | `		return PH7_CORRUPT;` |
|        - |  698 | `	}` |
|        - |  699 | `#endif` |
|     7940 |  700 | `	*ppEngine = 0;` |
|        - |  701 | `	/* One-time automatic library initialization */` |
|     7940 |  702 | `	rc = PH7CoreInitialize();` |
|     7940 |  703 | `	if( rc != PH7_OK ){` |
|      ! 0 |  704 | `		return rc;` |
|        - |  705 | `	}` |
|        - |  706 | `	/* Allocate a new engine */` |
|     7940 |  707 | `	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));` |
|     7940 |  708 | `	if( pEngine == 0 ){` |
|      ! 0 |  709 | `		return PH7_NOMEM;` |
|        - |  710 | `	}` |
|        - |  711 | `	/* Zero the structure */` |
|     7940 |  712 | `	SyZero(pEngine,sizeof(ph7));` |
|        - |  713 | `	/* Initialize engine fields */` |
|     7940 |  714 | `	pEngine->nMagic = PH7_ENGINE_MAGIC;` |
|     7940 |  715 | `	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);` |
|     7940 |  716 | `	if( rc != PH7_OK ){` |
|      ! 0 |  717 | `		goto Release;` |
|        - |  718 | `	}` |
|        - |  719 | `#if defined(PH7_ENABLE_THREADS)` |
|     7940 |  720 | `	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);` |
|        - |  721 | `#endif` |
|        - |  722 | `	/* Default configuration */` |
|     7940 |  723 | `	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);` |
|     7940 |  724 | `	SySetInit(&pEngine->xConf.aIniEntry,&pEngine->sAllocator,sizeof(VmIniEntry));` |
|        - |  725 | `	/* Install a default compile-time error consumer routine */` |
|     7940 |  726 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);` |
|        - |  727 | `	/* Built-in vfs */` |
|     7940 |  728 | `	pEngine->pVfs = sMPGlobal.pVfs;` |
|        - |  729 | `#if defined(PH7_ENABLE_THREADS)` |
|     7940 |  730 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  731 | `		 /* Associate a recursive mutex with this instance */` |
|     7940 |  732 | `		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|     7940 |  733 | `		 if( pEngine->pMutex == 0 ){` |
|      ! 0 |  734 | `			 rc = PH7_NOMEM;` |
|      ! 0 |  735 | `			 goto Release;` |
|        - |  736 | `		 }` |
|     3962 |  737 | `	 }` |
|        - |  738 | `#endif` |
|        - |  739 | `	/* Link to the list of active engines */` |
|        - |  740 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  741 | `	/* Enter the global mutex */` |
|     7940 |  742 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  743 | `#endif` |
|     7940 |  744 | `	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);` |
|     7940 |  745 | `	sMPGlobal.nEngine++;` |
|        - |  746 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  747 | `	/* Leave the global mutex */` |
|     7940 |  748 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  749 | `#endif` |
|        - |  750 | `	/* Write a pointer to the new instance */` |
|     7940 |  751 | `	*ppEngine = pEngine;` |
|     7940 |  752 | `	return PH7_OK;` |
|      ! 0 |  753 | `Release:` |
|      ! 0 |  754 | `	SyMemBackendRelease(&pEngine->sAllocator);` |
|      ! 0 |  755 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|      ! 0 |  756 | `	return rc;` |
|     3967 |  757 | `}` |
|        - |  758 | `/*` |
|        - |  759 | ` * [CAPIREF: ph7_release()]` |
|        - |  760 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  761 | ` */` |
|     7957 |  762 | `int ph7_release(ph7 *pEngine)` |
|        5 |  763 | `{` |
|        - |  764 | `	int rc;` |
|     7962 |  765 | `	if( PH7_ENGINE_MISUSE(pEngine) ){` |
|      ! 0 |  766 | `		return PH7_CORRUPT;` |
|        - |  767 | `	}` |
|        - |  768 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  769 | `	 /* Acquire engine mutex */` |
|     7962 |  770 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     7962 |  771 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     7957 |  772 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  773 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  774 | `	 }` |
|        - |  775 | `#endif` |
|        - |  776 | `	/* Release the engine */` |
|     7962 |  777 | `	rc = EngineRelease(&(*pEngine));` |
|        - |  778 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  779 | `	 /* Leave engine mutex */` |
|     7962 |  780 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  781 | `	 /* Release engine mutex */` |
|     7962 |  782 | `	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  783 | `#endif` |
|        - |  784 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  785 | `	/* Enter the global mutex */` |
|     7962 |  786 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  787 | `#endif` |
|        - |  788 | `	/* Unlink from the list of active engines */` |
|     7962 |  789 | `	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);` |
|     7962 |  790 | `	sMPGlobal.nEngine--;` |
|        - |  791 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  792 | `	/* Leave the global mutex */` |
|     7962 |  793 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */` |
|        - |  794 | `#endif` |
|        - |  795 | `	/* Release the memory chunk allocated to this engine */` |
|     7962 |  796 | `	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);` |
|     7962 |  797 | `	return rc;` |
|     3978 |  798 | `}` |
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
|     7925 |  809 | `static sxi32 ProcessScript(` |
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
|     7930 |  820 | `	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));` |
|     7930 |  821 | `	if( pVm == 0 ){` |
|        - |  822 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  823 | `		 * a tiny chunk of memory, there is no much we can do here. */` |
|      ! 0 |  824 | `		if( ppVm ){` |
|      ! 0 |  825 | `			*ppVm = 0;` |
|      ! 0 |  826 | `		}` |
|      ! 0 |  827 | `		return PH7_NOMEM;` |
|        - |  828 | `	}` |
|     7930 |  829 | `	if( iFlags < 0 ){` |
|        - |  830 | `		/* Default compile-time flags */` |
|      ! 0 |  831 | `		iFlags = 0;` |
|      ! 0 |  832 | `	}` |
|        - |  833 | `	/* Initialize the Virtual Machine */` |
|     7930 |  834 | `	rc = PH7_VmInit(pVm,&(*pEngine));` |
|     7930 |  835 | `	if( rc != PH7_OK ){` |
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
|     7930 |  846 | `	PH7_VmApplyEngineIni(pVm);` |
|        - |  847 | `	/* A syntax-CHECK compile (phl -l) never runs what it compiles, which changes` |
|        - |  848 | `	 * what a class declaration whose base is missing has to do -- see the flag's` |
|        - |  849 | `	 * note in ph7int.h -- and it also changes how the unit is NAMED: php's lint` |
|        - |  850 | `	 * mode hands the argument straight to the compiler where a run expands it to` |
|        - |  851 | ``	 * an absolute path first, so `phl -l ./x.php` must say `./x.php`. Set before`` |
|        - |  852 | `	 * the path is pushed, which is what reads it. */` |
|     7930 |  853 | `	pVm->bSyntaxCheck = (iFlags & PH7_SYNTAX_CHECK) ? 1 : 0;` |
|     7930 |  854 | `	if( zFilePath ){` |
|        - |  855 | `		/* Push processed file path */` |
|     7336 |  856 | `		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);` |
|     3665 |  857 | `	}else{` |
|        - |  858 | `		/* Anonymous source (phl -r / an embedder snippet): php names it` |
|        - |  859 | `		 * "Command line code" in every diagnostic location suffix. */` |
|      597 |  860 | `		PH7_VmPushFilePath(pVm,"Command line code",-1,TRUE,0);` |
|        - |  861 | `	}` |
|        - |  862 | `	/* Reset the error message consumer */` |
|     7930 |  863 | `	SyBlobReset(&pEngine->xConf.sErrConsumer);` |
|        - |  864 | `	/* Enforce input size cap before touching the lexer/compiler */` |
|        - |  865 | `	{` |
|     7930 |  866 | `		sxu32 nLimit = pEngine->xConf.nMaxInput ? pEngine->xConf.nMaxInput : PH7_MAX_INPUT_SIZE;` |
|     7930 |  867 | `		if( SyStringLength(pScript) > nLimit ){` |
|      ! 0 |  868 | `			PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,` |
|        - |  869 | `				"Input size (%u bytes) exceeds the configured limit (%u bytes)",` |
|      ! 0 |  870 | `				SyStringLength(pScript),nLimit);` |
|      ! 0 |  871 | `		}` |
|        - |  872 | `	}` |
|        - |  873 | `	/* Compile the script */` |
|     7930 |  874 | `	if( pVm->sCodeGen.nErr == 0 ){` |
|     7930 |  875 | `		PH7_CompileScript(pVm,&(*pScript),iFlags);` |
|     3957 |  876 | `	}` |
|     7930 |  877 | `	if( pVm->sCodeGen.nErr > 0 \|\| pVm == 0){` |
|     1238 |  878 | `		sxu32 nErr = pVm->sCodeGen.nErr;` |
|        - |  879 | `		/* Compilation error or null ppVm pointer,release this VM */` |
|     1238 |  880 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|     1238 |  881 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|     1238 |  882 | `		if( ppVm ){` |
|     1238 |  883 | `			*ppVm = 0;` |
|      617 |  884 | `		}` |
|     1238 |  885 | `		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;` |
|        - |  886 | `	}` |
|        - |  887 | `	/* Prepare the virtual machine for bytecode execution */` |
|     6696 |  888 | `	rc = PH7_VmMakeReady(pVm);` |
|     6696 |  889 | `	if( rc != PH7_OK ){` |
|        8 |  890 | `		goto Release;` |
|        - |  891 | `	}` |
|        - |  892 | `	/* Install local import path which is the current directory -- unless the host` |
|        - |  893 | ``	 * already named one. IMPORT_PATH APPENDS, and a `-d include_path=` handed to`` |
|        - |  894 | `	 * the engine is applied at VM birth, i.e. before this: the two together` |
|        - |  895 | ``	 * answered `.:.` for a run started with `-d include_path=.`. */`` |
|     6690 |  896 | `	if( SySetUsed(&pVm->aPaths) < 1 ){` |
|     6682 |  897 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");` |
|     3333 |  898 | `	}` |
|        - |  899 | `#if defined(PH7_ENABLE_THREADS)` |
|     6690 |  900 | `	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){` |
|        - |  901 | `		 /* Associate a recursive mutex with this instance */` |
|     6690 |  902 | `		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);` |
|     6690 |  903 | `		 if( pVm->pMutex == 0 ){` |
|      ! 0 |  904 | `			 goto Release;` |
|        - |  905 | `		 }` |
|     3337 |  906 | `	 }` |
|        - |  907 | `#endif` |
|        - |  908 | `	/* Script successfully compiled,link to the list of active virtual machines */` |
|     6690 |  909 | `	MACRO_LD_PUSH(pEngine->pVms,pVm);` |
|     6690 |  910 | `	pEngine->iVm++;` |
|        - |  911 | `	/* Point to the freshly created VM */` |
|     6690 |  912 | `	*ppVm = pVm;` |
|        - |  913 | `	/* Ready to execute PH7 bytecode */` |
|     6690 |  914 | `	return PH7_OK;` |
|        3 |  915 | `Release:` |
|        - |  916 | `	{` |
|        - |  917 | `		/* A code-generation error raised while mounting class definitions (e.g. a` |
|        - |  918 | `		 * typed class constant whose value violates its declared type) is a compile` |
|        - |  919 | `		 * error; any other PH7_VmMakeReady failure is a genuine VM-init error.` |
|        - |  920 | `		 * Captured before the releases free the VM. */` |
|        8 |  921 | `		sxi32 rcRet = (pVm->sCodeGen.nErr > 0) ? PH7_COMPILE_ERR : PH7_VM_ERR;` |
|        8 |  922 | `		SyMemBackendRelease(&pVm->sAllocator);` |
|        8 |  923 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|        8 |  924 | `		*ppVm = 0;` |
|        8 |  925 | `		return rcRet;` |
|        - |  926 | `	}` |
|     3962 |  927 | `}` |
|        - |  928 | `/*` |
|        - |  929 | ` * [CAPIREF: ph7_compile()]` |
|        - |  930 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  931 | ` */` |
|      ! 0 |  932 | `int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)` |
|      ! 0 |  933 | `{` |
|        - |  934 | `	SyString sScript;` |
|        - |  935 | `	int rc;` |
|      ! 0 |  936 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|      ! 0 |  937 | `		return PH7_CORRUPT;` |
|        - |  938 | `	}` |
|      ! 0 |  939 | `	if( nLen < 0 ){` |
|        - |  940 | `		/* Compute input length automatically */` |
|      ! 0 |  941 | `		nLen = (int)SyStrlen(zSource);` |
|      ! 0 |  942 | `	}` |
|      ! 0 |  943 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|        - |  944 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  945 | `	 /* Acquire engine mutex */` |
|      ! 0 |  946 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 |  947 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 |  948 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  949 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  950 | `	 }` |
|        - |  951 | `#endif` |
|        - |  952 | `	/* Compile the script */` |
|      ! 0 |  953 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);` |
|        - |  954 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  955 | `	 /* Leave engine mutex */` |
|      ! 0 |  956 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  957 | `#endif` |
|        - |  958 | `	/* Compilation result */` |
|      ! 0 |  959 | `	return rc;` |
|      ! 0 |  960 | `}` |
|        - |  961 | `/*` |
|        - |  962 | ` * [CAPIREF: ph7_compile_v2()]` |
|        - |  963 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  964 | ` */` |
|      594 |  965 | `int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)` |
|        3 |  966 | `{` |
|        - |  967 | `	SyString sScript;` |
|        - |  968 | `	int rc;` |
|      597 |  969 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| zSource == 0){` |
|      ! 0 |  970 | `		return PH7_CORRUPT;` |
|        - |  971 | `	}` |
|      597 |  972 | `	if( nLen < 0 ){` |
|        - |  973 | `		/* Compute input length automatically */` |
|      585 |  974 | `		nLen = (int)SyStrlen(zSource);` |
|      291 |  975 | `	}` |
|      597 |  976 | `	SyStringInitFromBuf(&sScript,zSource,nLen);` |
|        - |  977 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  978 | `	 /* Acquire engine mutex */` |
|      597 |  979 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      597 |  980 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      594 |  981 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 |  982 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - |  983 | `	 }` |
|        - |  984 | `#endif` |
|        - |  985 | `	/* Compile the script */` |
|      597 |  986 | `	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);` |
|        - |  987 | `#if defined(PH7_ENABLE_THREADS)` |
|        - |  988 | `	 /* Leave engine mutex */` |
|      597 |  989 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - |  990 | `#endif` |
|        - |  991 | `	/* Compilation result */` |
|      597 |  992 | `	return rc;` |
|      300 |  993 | `}` |
|        - |  994 | `/*` |
|        - |  995 | ` * [CAPIREF: ph7_compile_file()]` |
|        - |  996 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - |  997 | ` */` |
|     7339 |  998 | `int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)` |
|        5 |  999 | `{` |
|        - | 1000 | `	const ph7_vfs *pVfs;` |
|        - | 1001 | `	int rc;` |
|     7344 | 1002 | `	if( ppOutVm ){` |
|     7344 | 1003 | `		*ppOutVm = 0;` |
|     3664 | 1004 | `	}` |
|     7344 | 1005 | `	rc = PH7_OK; /* cc warning */` |
|     7344 | 1006 | `	if( PH7_ENGINE_MISUSE(pEngine) \|\| SX_EMPTY_STR(zFilePath) ){` |
|      ! 0 | 1007 | `		return PH7_CORRUPT;` |
|        - | 1008 | `	}` |
|        - | 1009 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1010 | `	 /* Acquire engine mutex */` |
|     7344 | 1011 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     7344 | 1012 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     7339 | 1013 | `		 PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 | 1014 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1015 | `	 }` |
|        - | 1016 | `#endif` |
|        - | 1017 | `	 /*` |
|        - | 1018 | `	  * Check if the underlying vfs implement the memory map` |
|        - | 1019 | `	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.` |
|        - | 1020 | `	  */` |
|     7344 | 1021 | `	 pVfs = pEngine->pVfs;` |
|     7344 | 1022 | `	 if( pVfs == 0 \|\| pVfs->xMmap == 0 ){` |
|        - | 1023 | `		 /* Memory map routine not implemented */` |
|      ! 0 | 1024 | `		 rc = PH7_IO_ERR;` |
|      ! 0 | 1025 | `	 }else{` |
|     7344 | 1026 | `		 void *pMapView = 0; /* cc warning */` |
|     7344 | 1027 | `		 ph7_int64 nSize = 0; /* cc warning */` |
|        - | 1028 | `		 SyString sScript;` |
|        - | 1029 | `		 /* Try to get a memory view of the whole file */` |
|     7344 | 1030 | `		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);` |
|     7344 | 1031 | `		 if( rc != PH7_OK ){` |
|        - | 1032 | `			 /* Assume an IO error */` |
|        8 | 1033 | `			 rc = PH7_IO_ERR;` |
|        4 | 1034 | `		 }else{` |
|        - | 1035 | `			 /* Compile the file */` |
|     7336 | 1036 | `			 SyStringInitFromBuf(&sScript,pMapView,nSize);` |
|     7336 | 1037 | `			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);` |
|        - | 1038 | `			 /* Release the memory view of the whole file. A ZERO-length view is` |
|        - | 1039 | `			  * the answer for a 0-byte file -- a legal, empty PHP program -- and` |
|        - | 1040 | `			  * it is not a mapping: the vfs hands back a static empty string, so` |
|        - | 1041 | `			  * unmapping it would be a free of something it never allocated. */` |
|     7336 | 1042 | `			 if( pVfs->xUnmap && nSize > 0 ){` |
|     7332 | 1043 | `				 pVfs->xUnmap(pMapView,nSize);` |
|     3658 | 1044 | `			 }` |
|        - | 1045 | `		 }` |
|        - | 1046 | `	 }` |
|        - | 1047 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1048 | `	 /* Leave engine mutex */` |
|     7344 | 1049 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1050 | `#endif` |
|        - | 1051 | `	/* Compilation result */` |
|     7344 | 1052 | `	return rc;` |
|     3669 | 1053 | `}` |
|        - | 1054 | `/*` |
|        - | 1055 | ` * [CAPIREF: ph7_vm_dump_v2()]` |
|        - | 1056 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1057 | ` */` |
|        2 | 1058 | `int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)` |
|        1 | 1059 | `{` |
|        - | 1060 | `	int rc;` |
|        - | 1061 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|        3 | 1062 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1063 | `		return PH7_CORRUPT;` |
|        - | 1064 | `	}` |
|        - | 1065 | `#ifdef UNTRUST` |
|        - | 1066 | `	if( xConsumer == 0 ){` |
|        - | 1067 | `		return PH7_CORRUPT;` |
|        - | 1068 | `	}` |
|        - | 1069 | `#endif` |
|        - | 1070 | `	/* Dump VM instructions */` |
|        3 | 1071 | `	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);` |
|        3 | 1072 | `	return rc;` |
|        2 | 1073 | `}` |
|        - | 1074 | `/*` |
|        - | 1075 | ` * [CAPIREF: ph7_vm_config()]` |
|        - | 1076 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1077 | ` */` |
|   226086 | 1078 | `int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)` |
|        5 | 1079 | `{` |
|        - | 1080 | `	va_list ap;` |
|        - | 1081 | `	int rc;` |
|        - | 1082 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|   226091 | 1083 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1084 | `		return PH7_CORRUPT;` |
|        - | 1085 | `	}` |
|        - | 1086 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1087 | `	 /* Acquire VM mutex */` |
|   226091 | 1088 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|   226091 | 1089 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|   226086 | 1090 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1091 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1092 | `	 }` |
|        - | 1093 | `#endif` |
|        - | 1094 | `	/* Confiugure the virtual machine */` |
|   226091 | 1095 | `	va_start(ap,iConfigOp);` |
|   226091 | 1096 | `	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);` |
|   226091 | 1097 | `	va_end(ap);` |
|        - | 1098 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1099 | `	 /* Leave VM mutex */` |
|   226091 | 1100 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1101 | `#endif` |
|   226091 | 1102 | `	return rc;` |
|   112872 | 1103 | `}` |
|        - | 1104 | `/*` |
|        - | 1105 | ` * [CAPIREF: ph7_vm_exec()]` |
|        - | 1106 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1107 | ` */` |
|     6583 | 1108 | `int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)` |
|        5 | 1109 | `{` |
|        - | 1110 | `	int rc;` |
|        - | 1111 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     6588 | 1112 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       16 | 1113 | `		return PH7_CORRUPT;` |
|        - | 1114 | `	}` |
|        - | 1115 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1116 | `	 /* Acquire VM mutex */` |
|     6588 | 1117 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6588 | 1118 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6575 | 1119 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1120 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1121 | `	 }` |
|        - | 1122 | `#endif` |
|        - | 1123 | `	/* Execute PH7 byte-code */` |
|     6588 | 1124 | `	rc = PH7_VmByteCodeExec(&(*pVm));` |
|     6596 | 1125 | `	if( pExitStatus ){` |
|        - | 1126 | `		/* Exit status */` |
|     6550 | 1127 | `		*pExitStatus = pVm->iExitStatus;` |
|     3267 | 1128 | `	}` |
|        - | 1129 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1130 | `	 /* Leave VM mutex */` |
|     6596 | 1131 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1132 | `#endif` |
|        - | 1133 | `	/* Execution result */` |
|     6596 | 1134 | `	return rc;` |
|     3295 | 1135 | `}` |
|        - | 1136 | `/*` |
|        - | 1137 | ` * [CAPIREF: ph7_vm_reset()]` |
|        - | 1138 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1139 | ` */` |
|       16 | 1140 | `int ph7_vm_reset(ph7_vm *pVm)` |
|      ! 0 | 1141 | `{` |
|        - | 1142 | `	int rc;` |
|        - | 1143 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|       16 | 1144 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1145 | `		return PH7_CORRUPT;` |
|        - | 1146 | `	}` |
|        - | 1147 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1148 | `	 /* Acquire VM mutex */` |
|       16 | 1149 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|       16 | 1150 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|       16 | 1151 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1152 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1153 | `	 }` |
|        - | 1154 | `#endif` |
|       16 | 1155 | `	rc = PH7_VmReset(&(*pVm));` |
|        - | 1156 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1157 | `	 /* Leave VM mutex */` |
|       16 | 1158 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1159 | `#endif` |
|       16 | 1160 | `	return rc;` |
|        8 | 1161 | `}` |
|        - | 1162 | `/*` |
|        - | 1163 | ` * [CAPIREF: ph7_vm_release()]` |
|        - | 1164 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1165 | ` */` |
|     6701 | 1166 | `int ph7_vm_release(ph7_vm *pVm)` |
|        5 | 1167 | `{` |
|        - | 1168 | `	ph7 *pEngine;` |
|        - | 1169 | `	int rc;` |
|        - | 1170 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|     6706 | 1171 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1172 | `		return PH7_CORRUPT;` |
|        - | 1173 | `	}` |
|        - | 1174 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1175 | `	 /* Acquire VM mutex */` |
|     6706 | 1176 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6706 | 1177 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6701 | 1178 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1179 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1180 | `	 }` |
|        - | 1181 | `#endif` |
|     6706 | 1182 | `	pEngine = pVm->pEngine;` |
|     6706 | 1183 | `	rc = PH7_VmRelease(&(*pVm));` |
|        - | 1184 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1185 | `	 /* Leave VM mutex */` |
|     6706 | 1186 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6706 | 1187 | `	 if( rc == PH7_OK && pVm->pMutex ){` |
|        - | 1188 | `		 /* The per-VM mutex was allocated in ProcessScript and never freed — one` |
|        - | 1189 | `		  * leak per VM, which LeakSanitizer reports on every single run. */` |
|     6706 | 1190 | `		 SyMutexRelease(sMPGlobal.pMutexMethods,pVm->pMutex);` |
|     6706 | 1191 | `		 pVm->pMutex = 0;` |
|     3345 | 1192 | `	 }` |
|        - | 1193 | `#endif` |
|     6706 | 1194 | `	if( rc == PH7_OK ){` |
|        - | 1195 | `		/* Unlink from the list of active VM */` |
|        - | 1196 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1197 | `			/* Acquire engine mutex */` |
|     6706 | 1198 | `			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|     6706 | 1199 | `			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|     6701 | 1200 | `				PH7_THRD_ENGINE_RELEASE(pEngine) ){` |
|      ! 0 | 1201 | `					return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1202 | `			}` |
|        - | 1203 | `#endif` |
|     6706 | 1204 | `		MACRO_LD_REMOVE(pEngine->pVms,pVm);` |
|     6706 | 1205 | `		pEngine->iVm--;` |
|        - | 1206 | `		/* Release the memory chunk allocated to this VM */` |
|     6706 | 1207 | `		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);` |
|        - | 1208 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1209 | `			/* Leave engine mutex */` |
|     6706 | 1210 | `			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1211 | `#endif` |
|     3345 | 1212 | `	}` |
|     6706 | 1213 | `	return rc;` |
|     3350 | 1214 | `}` |
|        - | 1215 | `/*` |
|        - | 1216 | ` * [CAPIREF: ph7_create_function()]` |
|        - | 1217 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1218 | ` */` |
|  8931508 | 1219 | `int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)` |
|        5 | 1220 | `{` |
|        - | 1221 | `	SyString sName;` |
|        - | 1222 | `	int rc;` |
|        - | 1223 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  8931513 | 1224 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1225 | `		return PH7_CORRUPT;` |
|        - | 1226 | `	}` |
|  8931513 | 1227 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|        - | 1228 | `	/* Remove leading and trailing white spaces */` |
|  8931513 | 1229 | `	SyStringFullTrim(&sName);` |
|        - | 1230 | `	/* Ticket 1433-003: NULL values are not allowed */` |
|  8931513 | 1231 | `	if( sName.nByte < 1 \|\| xFunc == 0 ){` |
|      ! 0 | 1232 | `		return PH7_CORRUPT;` |
|        - | 1233 | `	}` |
|        - | 1234 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1235 | `	 /* Acquire VM mutex */` |
|  8931513 | 1236 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|  8931513 | 1237 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|  8931508 | 1238 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1239 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1240 | `	 }` |
|        - | 1241 | `#endif` |
|        - | 1242 | `	/* Install the foreign function */` |
|  8931513 | 1243 | `	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData);` |
|        - | 1244 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1245 | `	 /* Leave VM mutex */` |
|  8931513 | 1246 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1247 | `#endif` |
|  8931513 | 1248 | `	return rc;` |
|  4447673 | 1249 | `}` |
|        - | 1250 | `/*` |
|        - | 1251 | ` * [CAPIREF: ph7_delete_function()]` |
|        - | 1252 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1253 | ` */` |
|      ! 0 | 1254 | `int ph7_delete_function(ph7_vm *pVm,const char *zName)` |
|      ! 0 | 1255 | `{` |
|      ! 0 | 1256 | `	ph7_user_func *pFunc = 0;` |
|        - | 1257 | `	int rc;` |
|        - | 1258 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      ! 0 | 1259 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1260 | `		return PH7_CORRUPT;` |
|        - | 1261 | `	}` |
|        - | 1262 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1263 | `	 /* Acquire VM mutex */` |
|      ! 0 | 1264 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 | 1265 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 | 1266 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1267 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1268 | `	 }` |
|        - | 1269 | `#endif` |
|        - | 1270 | `	/* Perform the deletion */` |
|      ! 0 | 1271 | `	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);` |
|      ! 0 | 1272 | `	if( rc == PH7_OK ){` |
|        - | 1273 | `		/* A name that WAS callable is not any more; every call site that remembers` |
|        - | 1274 | `		 * having screened it has to ask again (OP_CALL_INIT). */` |
|      ! 0 | 1275 | `		pVm->nCallableGen++;` |
|        - | 1276 | `		/* Release internal fields */` |
|      ! 0 | 1277 | `		SySetRelease(&pFunc->aAux);` |
|      ! 0 | 1278 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 | 1279 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 | 1280 | `	}` |
|        - | 1281 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1282 | `	 /* Leave VM mutex */` |
|      ! 0 | 1283 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1284 | `#endif` |
|      ! 0 | 1285 | `	return rc;` |
|      ! 0 | 1286 | `}` |
|        - | 1287 | `/*` |
|        - | 1288 | ` * [CAPIREF: ph7_create_constant()]` |
|        - | 1289 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1290 | ` */` |
| 12993589 | 1291 | `int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)` |
|        5 | 1292 | `{` |
|        - | 1293 | `	SyString sName;` |
|        - | 1294 | `	int rc;` |
|        - | 1295 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
| 12993594 | 1296 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1297 | `		return PH7_CORRUPT;` |
|        - | 1298 | `	}` |
| 12993594 | 1299 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|        - | 1300 | `	/* Remove leading and trailing white spaces */` |
| 12993594 | 1301 | `	SyStringFullTrim(&sName);` |
| 12993594 | 1302 | `	if( sName.nByte < 1 ){` |
|        - | 1303 | `		/* Empty constant name */` |
|      ! 0 | 1304 | `		return PH7_CORRUPT;` |
|        - | 1305 | `	}` |
|        - | 1306 | `	/* TICKET 1433-003: NULL pointer harmless operation */` |
| 12993594 | 1307 | `	if( xExpand == 0 ){` |
|      ! 0 | 1308 | `		return PH7_CORRUPT;` |
|        - | 1309 | `	}` |
|        - | 1310 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1311 | `	 /* Acquire VM mutex */` |
| 12993594 | 1312 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
| 12993594 | 1313 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
| 12993589 | 1314 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1315 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1316 | `	 }` |
|        - | 1317 | `#endif` |
|        - | 1318 | `	/* Perform the registration */` |
| 12993594 | 1319 | `	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);` |
|        - | 1320 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1321 | `	 /* Leave VM mutex */` |
| 12993594 | 1322 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1323 | `#endif` |
| 12993594 | 1324 | `	 return rc;` |
|  6297139 | 1325 | `}` |
|        - | 1326 | `/*` |
|        - | 1327 | ` * [CAPIREF: ph7_delete_constant()]` |
|        - | 1328 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1329 | ` */` |
|      ! 0 | 1330 | `int ph7_delete_constant(ph7_vm *pVm,const char *zName)` |
|      ! 0 | 1331 | `{` |
|        - | 1332 | `	ph7_constant *pCons;` |
|        - | 1333 | `	int rc;` |
|        - | 1334 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|      ! 0 | 1335 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1336 | `		return PH7_CORRUPT;` |
|        - | 1337 | `	}` |
|        - | 1338 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1339 | `	 /* Acquire VM mutex */` |
|      ! 0 | 1340 | `	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|      ! 0 | 1341 | `	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE &&` |
|      ! 0 | 1342 | `		 PH7_THRD_VM_RELEASE(pVm) ){` |
|      ! 0 | 1343 | `			 return PH7_ABORT; /* Another thread have released this instance */` |
|        - | 1344 | `	 }` |
|        - | 1345 | `#endif` |
|        - | 1346 | `	 /* Query the constant hashtable */` |
|      ! 0 | 1347 | `	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);` |
|      ! 0 | 1348 | `	 if( rc == PH7_OK ){` |
|        - | 1349 | `		 /* A name that WAS a constant is not any more, and the entry every LOADC site` |
|        - | 1350 | `		  * that resolved to it remembers is about to be freed (PH7_VmConstSiteAnswer). */` |
|      ! 0 | 1351 | `		 pVm->nConstGen++;` |
|        - | 1352 | `		 /* Perform the deletion */` |
|      ! 0 | 1353 | `		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));` |
|      ! 0 | 1354 | `		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 | 1355 | `	 }` |
|        - | 1356 | `#if defined(PH7_ENABLE_THREADS)` |
|        - | 1357 | `	 /* Leave VM mutex */` |
|      ! 0 | 1358 | `	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */` |
|        - | 1359 | `#endif` |
|      ! 0 | 1360 | `	return rc;` |
|      ! 0 | 1361 | `}` |
|        - | 1362 | `/*` |
|        - | 1363 | ` * [CAPIREF: ph7_new_scalar()]` |
|        - | 1364 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1365 | ` */` |
|  1948436 | 1366 | `ph7_value * ph7_new_scalar(ph7_vm *pVm)` |
|        5 | 1367 | `{` |
|        - | 1368 | `	ph7_value *pObj;` |
|        - | 1369 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  1948441 | 1370 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1371 | `		return 0;` |
|        - | 1372 | `	}` |
|        - | 1373 | `	/* Allocate a new scalar variable */` |
|  1948441 | 1374 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  1948441 | 1375 | `	if( pObj == 0 ){` |
|      ! 0 | 1376 | `		return 0;` |
|        - | 1377 | `	}` |
|        - | 1378 | `	/* Nullify the new scalar */` |
|  1948441 | 1379 | `	PH7_MemObjInit(pVm,pObj);` |
|  1948441 | 1380 | `	return pObj;` |
|   974112 | 1381 | `}` |
|        - | 1382 | `/*` |
|        - | 1383 | ` * [CAPIREF: ph7_new_array()]` |
|        - | 1384 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1385 | ` */` |
|  3315784 | 1386 | `ph7_value * ph7_new_array(ph7_vm *pVm)` |
|        5 | 1387 | `{` |
|        - | 1388 | `	ph7_hashmap *pMap;` |
|        - | 1389 | `	ph7_value *pObj;` |
|        - | 1390 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  3315789 | 1391 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|      ! 0 | 1392 | `		return 0;` |
|        - | 1393 | `	}` |
|        - | 1394 | `	/* Create a new hashmap first */` |
|  3315789 | 1395 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  3315789 | 1396 | `	if( pMap == 0 ){` |
|      ! 0 | 1397 | `		return 0;` |
|        - | 1398 | `	}` |
|        - | 1399 | `	/* Associate a new ph7_value with this hashmap */` |
|  3315789 | 1400 | `	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  3315789 | 1401 | `	if( pObj == 0 ){` |
|      ! 0 | 1402 | `		PH7_HashmapRelease(pMap,TRUE);` |
|      ! 0 | 1403 | `		return 0;` |
|        - | 1404 | `	}` |
|  3315789 | 1405 | `	PH7_MemObjInitFromArray(pVm,pObj,pMap);` |
|  3315789 | 1406 | `	return pObj;` |
|  1657654 | 1407 | `}` |
|        - | 1408 | `/*` |
|        - | 1409 | ` * [CAPIREF: ph7_release_value()]` |
|        - | 1410 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1411 | ` */` |
|  3958258 | 1412 | `int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)` |
|        5 | 1413 | `{` |
|        - | 1414 | `	/* Ticket 1433-002: NULL VM is harmless operation */` |
|  3958263 | 1415 | `	if ( PH7_VM_MISUSE(pVm) ){` |
|       76 | 1416 | `		return PH7_CORRUPT;` |
|        - | 1417 | `	}` |
|  3958189 | 1418 | `	if( pValue ){` |
|        - | 1419 | `		/* Release the value */` |
|  3958189 | 1420 | `		PH7_MemObjRelease(pValue);` |
|  3958189 | 1421 | `		SyMemBackendPoolFree(&pVm->sAllocator,pValue);` |
|  1978881 | 1422 | `	}` |
|  3958189 | 1423 | `	return PH7_OK;` |
|  1978923 | 1424 | `}` |
|        - | 1425 | `/*` |
|        - | 1426 | ` * [CAPIREF: ph7_value_to_int()]` |
|        - | 1427 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1428 | ` */` |
|    76835 | 1429 | `int ph7_value_to_int(ph7_value *pValue)` |
|        5 | 1430 | `{` |
|        - | 1431 | `	int rc;` |
|    76840 | 1432 | `	rc = PH7_MemObjToInteger(pValue);` |
|    76840 | 1433 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1434 | `		return 0;` |
|        - | 1435 | `	}` |
|    76840 | 1436 | `	return (int)pValue->x.iVal;` |
|    38585 | 1437 | `}` |
|        - | 1438 | `/*` |
|        - | 1439 | ` * [CAPIREF: ph7_value_to_bool()]` |
|        - | 1440 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1441 | ` */` |
|    51612 | 1442 | `int ph7_value_to_bool(ph7_value *pValue)` |
|        5 | 1443 | `{` |
|        - | 1444 | `	int rc;` |
|    51617 | 1445 | `	rc = PH7_MemObjToBool(pValue);` |
|    51617 | 1446 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1447 | `		return 0;` |
|        - | 1448 | `	}` |
|    51617 | 1449 | `	return (int)pValue->x.iVal;` |
|    25566 | 1450 | `}` |
|        - | 1451 | `/*` |
|        - | 1452 | ` * [CAPIREF: ph7_value_to_int64()]` |
|        - | 1453 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1454 | ` */` |
|  4422558 | 1455 | `ph7_int64 ph7_value_to_int64(ph7_value *pValue)` |
|        5 | 1456 | `{` |
|        - | 1457 | `	int rc;` |
|  4422563 | 1458 | `	rc = PH7_MemObjToInteger(pValue);` |
|  4422563 | 1459 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1460 | `		return 0;` |
|        - | 1461 | `	}` |
|  4422563 | 1462 | `	return pValue->x.iVal;` |
|  2211890 | 1463 | `}` |
|        - | 1464 | `/*` |
|        - | 1465 | ` * [CAPIREF: ph7_value_to_double()]` |
|        - | 1466 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1467 | ` */` |
|     4335 | 1468 | `double ph7_value_to_double(ph7_value *pValue)` |
|        5 | 1469 | `{` |
|        - | 1470 | `	int rc;` |
|     4340 | 1471 | `	rc = PH7_MemObjToReal(pValue);` |
|     4340 | 1472 | `	if( rc != PH7_OK ){` |
|      ! 0 | 1473 | `		return (double)0;` |
|        - | 1474 | `	}` |
|     4340 | 1475 | `	return (double)pValue->rVal;` |
|     2176 | 1476 | `}` |
|        - | 1477 | `/*` |
|        - | 1478 | ` * [CAPIREF: ph7_value_to_string()]` |
|        - | 1479 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1480 | ` */` |
|  5404816 | 1481 | `const char * ph7_value_to_string(ph7_value *pValue,int *pLen)` |
|        5 | 1482 | `{` |
|  5404821 | 1483 | `	PH7_MemObjToString(pValue);` |
|  5404821 | 1484 | `	if( SyBlobLength(&pValue->sBlob) > 0 ){` |
|  5277749 | 1485 | `		SyBlobNullAppend(&pValue->sBlob);` |
|  5277749 | 1486 | `		if( pLen ){` |
|  5033495 | 1487 | `			*pLen = (int)SyBlobLength(&pValue->sBlob);` |
|  2517485 | 1488 | `		}` |
|  5277749 | 1489 | `		return (const char *)SyBlobData(&pValue->sBlob);` |
|      ! 0 | 1490 | `	}else{` |
|        - | 1491 | `		/* Return the empty string */` |
|   127077 | 1492 | `		if( pLen ){` |
|   126635 | 1493 | `			*pLen = 0;` |
|    63170 | 1494 | `		}` |
|   127077 | 1495 | `		return "";` |
|        - | 1496 | `	}` |
|  2702915 | 1497 | `}` |
|        - | 1498 | `/*` |
|        - | 1499 | ` * [CAPIREF: ph7_value_to_resource()]` |
|        - | 1500 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1501 | ` */` |
|   173306 | 1502 | `void * ph7_value_to_resource(ph7_value *pValue)` |
|        5 | 1503 | `{` |
|   173311 | 1504 | `	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){` |
|        - | 1505 | `		/* Not a resource,return NULL */` |
|      ! 0 | 1506 | `		return 0;` |
|        - | 1507 | `	}` |
|   173311 | 1508 | `	return pValue->x.pOther;` |
|    85959 | 1509 | `}` |
|        - | 1510 | `/*` |
|        - | 1511 | ` * [CAPIREF: ph7_value_compare()]` |
|        - | 1512 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1513 | ` */` |
|       80 | 1514 | `int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)` |
|        3 | 1515 | `{` |
|        - | 1516 | `	int rc;` |
|       83 | 1517 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|        - | 1518 | `		/* TICKET 1433-24: NULL values is harmless operation */` |
|      ! 0 | 1519 | `		return 1;` |
|        - | 1520 | `	}` |
|        - | 1521 | `	/* Perform the comparison */` |
|       83 | 1522 | `	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);` |
|        - | 1523 | `	/* A native compare handler may have REFUSED the pair (php throws comparing` |
|        - | 1524 | `	 * two different KINDS of DateTimeZone). The record is deliberately LEFT` |
|        - | 1525 | `	 * standing: array_keys() reaches the comparator through this entry point, and` |
|        - | 1526 | `	 * the host-call boundary raises what it recorded exactly as it does for the` |
|        - | 1527 | `	 * builtins that call PH7_MemObjCmp directly. A host application driving this` |
|        - | 1528 | `	 * outside any execution never sees the throw, and PH7_VmInit/PH7_VmReset` |
|        - | 1529 | `	 * clear the record before the next one begins. */` |
|        - | 1530 | `	/* Comparison result */` |
|       83 | 1531 | `	return rc;` |
|       43 | 1532 | `}` |
|        - | 1533 | `/*` |
|        - | 1534 | ` * [CAPIREF: ph7_result_int()]` |
|        - | 1535 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1536 | ` */` |
|   268177 | 1537 | `int ph7_result_int(ph7_context *pCtx,int iValue)` |
|        5 | 1538 | `{` |
|   268182 | 1539 | `	return ph7_value_int(pCtx->pRet,iValue);` |
|        5 | 1540 | `}` |
|        - | 1541 | `/*` |
|        - | 1542 | ` * [CAPIREF: ph7_result_int64()]` |
|        - | 1543 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1544 | ` */` |
|  1688751 | 1545 | `int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)` |
|        5 | 1546 | `{` |
|  1688756 | 1547 | `	return ph7_value_int64(pCtx->pRet,iValue);` |
|        5 | 1548 | `}` |
|        - | 1549 | `/*` |
|        - | 1550 | ` * [CAPIREF: ph7_result_bool()]` |
|        - | 1551 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1552 | ` */` |
|   704296 | 1553 | `int ph7_result_bool(ph7_context *pCtx,int iBool)` |
|        5 | 1554 | `{` |
|   704301 | 1555 | `	return ph7_value_bool(pCtx->pRet,iBool);` |
|        5 | 1556 | `}` |
|        - | 1557 | `/*` |
|        - | 1558 | ` * [CAPIREF: ph7_result_double()]` |
|        - | 1559 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1560 | ` */` |
|     1304 | 1561 | `int ph7_result_double(ph7_context *pCtx,double Value)` |
|        5 | 1562 | `{` |
|     1309 | 1563 | `	return ph7_value_double(pCtx->pRet,Value);` |
|        5 | 1564 | `}` |
|        - | 1565 | `/*` |
|        - | 1566 | ` * [CAPIREF: ph7_result_null()]` |
|        - | 1567 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1568 | ` */` |
|    12504 | 1569 | `int ph7_result_null(ph7_context *pCtx)` |
|        5 | 1570 | `{` |
|        - | 1571 | `	/* Invalidate any prior representation and set the NULL flag */` |
|    12509 | 1572 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    12509 | 1573 | `	return PH7_OK;` |
|        5 | 1574 | `}` |
|        - | 1575 | `/*` |
|        - | 1576 | ` * [CAPIREF: ph7_result_string()]` |
|        - | 1577 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1578 | ` */` |
|  4497762 | 1579 | `int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)` |
|        5 | 1580 | `{` |
|  4497767 | 1581 | `	return ph7_value_string(pCtx->pRet,zString,nLen);` |
|        5 | 1582 | `}` |
|        - | 1583 | `/*` |
|        - | 1584 | ` * [CAPIREF: ph7_result_string_format()]` |
|        - | 1585 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1586 | ` */` |
|   413364 | 1587 | `int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)` |
|        5 | 1588 | `{` |
|        - | 1589 | `	ph7_value *p;` |
|        - | 1590 | `	va_list ap;` |
|        - | 1591 | `	int rc;` |
|   413369 | 1592 | `	p = pCtx->pRet;` |
|   413369 | 1593 | `	if( (p->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1594 | `		/* Invalidate any prior representation */` |
|   396351 | 1595 | `		PH7_MemObjRelease(p);` |
|   396351 | 1596 | `		MemObjSetType(p,MEMOBJ_STRING);` |
|   198173 | 1597 | `	}` |
|        - | 1598 | `	/* Format the given string */` |
|   413369 | 1599 | `	va_start(ap,zFormat);` |
|   413369 | 1600 | `	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);` |
|   413369 | 1601 | `	va_end(ap);` |
|   413369 | 1602 | `	return rc;` |
|        5 | 1603 | `}` |
|        - | 1604 | `/*` |
|        - | 1605 | ` * [CAPIREF: ph7_result_value()]` |
|        - | 1606 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1607 | ` */` |
|   888648 | 1608 | `int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)` |
|        5 | 1609 | `{` |
|   888653 | 1610 | `	int rc = PH7_OK;` |
|   888653 | 1611 | `	if( pValue == 0 ){` |
|      ! 0 | 1612 | `		PH7_MemObjRelease(pCtx->pRet);` |
|      ! 0 | 1613 | `	}else{` |
|   888653 | 1614 | `		rc = PH7_MemObjStore(pValue,pCtx->pRet);` |
|        - | 1615 | `	}` |
|   888653 | 1616 | `	return rc;` |
|        5 | 1617 | `}` |
|        - | 1618 | `/*` |
|        - | 1619 | ` * [CAPIREF: ph7_result_resource()]` |
|        - | 1620 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1621 | ` */` |
|   102119 | 1622 | `int ph7_result_resource(ph7_context *pCtx,void *pUserData)` |
|        5 | 1623 | `{` |
|   102124 | 1624 | `	return ph7_value_resource(pCtx->pRet,pUserData);` |
|        5 | 1625 | `}` |
|        - | 1626 | `/*` |
|        - | 1627 | ` * [CAPIREF: ph7_context_new_scalar()]` |
|        - | 1628 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1629 | ` */` |
|   437562 | 1630 | `ph7_value * ph7_context_new_scalar(ph7_context *pCtx)` |
|        5 | 1631 | `{` |
|        - | 1632 | `	ph7_value *pVal;` |
|   437567 | 1633 | `	pVal = ph7_new_scalar(pCtx->pVm);` |
|   437567 | 1634 | `	if( pVal ){` |
|        - | 1635 | `		/* Record value address so it can be freed automatically` |
|        - | 1636 | `		 * when the calling function returns.` |
|        - | 1637 | `		 */` |
|   437567 | 1638 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   218714 | 1639 | `	}` |
|   437567 | 1640 | `	return pVal;` |
|        5 | 1641 | `}` |
|        - | 1642 | `/*` |
|        - | 1643 | ` * [CAPIREF: ph7_context_new_array()]` |
|        - | 1644 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1645 | ` */` |
|   867867 | 1646 | `ph7_value * ph7_context_new_array(ph7_context *pCtx)` |
|        5 | 1647 | `{` |
|        - | 1648 | `	ph7_value *pVal;` |
|   867872 | 1649 | `	pVal = ph7_new_array(pCtx->pVm);` |
|   867872 | 1650 | `	if( pVal ){` |
|        - | 1651 | `		/* Record value address so it can be freed automatically` |
|        - | 1652 | `		 * when the calling function returns.` |
|        - | 1653 | `		 */` |
|   867872 | 1654 | `		SySetPut(&pCtx->sVar,(const void *)&pVal);` |
|   433860 | 1655 | `	}` |
|   867872 | 1656 | `	return pVal;` |
|        5 | 1657 | `}` |
|        - | 1658 | `/*` |
|        - | 1659 | ` * [CAPIREF: ph7_context_release_value()]` |
|        - | 1660 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1661 | ` */` |
|    58561 | 1662 | `void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)` |
|        5 | 1663 | `{` |
|    58566 | 1664 | `	PH7_VmReleaseContextValue(&(*pCtx),pValue);` |
|    58566 | 1665 | `}` |
|        - | 1666 | `/*` |
|        - | 1667 | ` * [CAPIREF: ph7_context_alloc_chunk()]` |
|        - | 1668 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1669 | ` */` |
|    36128 | 1670 | `void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)` |
|        5 | 1671 | `{` |
|        - | 1672 | `	void *pChunk;` |
|    36133 | 1673 | `	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);` |
|    36133 | 1674 | `	if( pChunk ){` |
|    36133 | 1675 | `		if( ZeroChunk ){` |
|        - | 1676 | `			/* Zero the memory chunk */` |
|    24490 | 1677 | `			SyZero(pChunk,nByte);` |
|    12053 | 1678 | `		}` |
|    36133 | 1679 | `		if( AutoRelease ){` |
|        - | 1680 | `			ph7_aux_data sAux;` |
|        - | 1681 | `			/* Track the chunk so that it can be released automatically` |
|        - | 1682 | `			 * upon this context is destroyed.` |
|        - | 1683 | `			 */` |
|    23344 | 1684 | `			sAux.pAuxData = pChunk;` |
|    23344 | 1685 | `			SySetPut(&pCtx->sChunk,(const void *)&sAux);` |
|    11526 | 1686 | `		}` |
|    17864 | 1687 | `	}` |
|    36133 | 1688 | `	return pChunk;` |
|        5 | 1689 | `}` |
|        - | 1690 | `/*` |
|        - | 1691 | ` * Check if the given chunk address is registered in the call context` |
|        - | 1692 | ` * chunk container.` |
|        - | 1693 | ` * Return TRUE if registered.FALSE otherwise.` |
|        - | 1694 | ` * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].` |
|        - | 1695 | ` */` |
|     1790 | 1696 | `static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)` |
|        5 | 1697 | `{` |
|        - | 1698 | `	ph7_aux_data *aAux,*pAux;` |
|        - | 1699 | `	sxu32 n;` |
|     1795 | 1700 | `	if( SySetUsed(&pCtx->sChunk) < 1 ){` |
|        - | 1701 | `		/* Don't bother processing,the container is empty */` |
|     1091 | 1702 | `		return 0;` |
|        - | 1703 | `	}` |
|        - | 1704 | `	/* Perform the lookup */` |
|      709 | 1705 | `	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     1083 | 1706 | `	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     1083 | 1707 | `		pAux = &aAux[n];` |
|     1083 | 1708 | `		if( pAux->pAuxData == pChunk ){` |
|        - | 1709 | `			/* Chunk found */` |
|      709 | 1710 | `			return pAux;` |
|        - | 1711 | `		}` |
|      191 | 1712 | `	}` |
|        - | 1713 | `	/* No such allocated chunk */` |
|      ! 0 | 1714 | `	return 0;` |
|      890 | 1715 | `}` |
|        - | 1716 | `/*` |
|        - | 1717 | ` * [CAPIREF: ph7_context_realloc_chunk()]` |
|        - | 1718 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1719 | ` */` |
|      ! 0 | 1720 | `void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)` |
|      ! 0 | 1721 | `{` |
|        - | 1722 | `	ph7_aux_data *pAux;` |
|        - | 1723 | `	void *pNew;` |
|      ! 0 | 1724 | `	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);` |
|      ! 0 | 1725 | `	if( pNew ){` |
|      ! 0 | 1726 | `		pAux = ContextFindChunk(pCtx,pChunk);` |
|      ! 0 | 1727 | `		if( pAux ){` |
|      ! 0 | 1728 | `			pAux->pAuxData = pNew;` |
|      ! 0 | 1729 | `		}` |
|      ! 0 | 1730 | `	}` |
|      ! 0 | 1731 | `	return pNew;` |
|      ! 0 | 1732 | `}` |
|        - | 1733 | `/*` |
|        - | 1734 | ` * [CAPIREF: ph7_context_free_chunk()]` |
|        - | 1735 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1736 | ` */` |
|     1790 | 1737 | `void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)` |
|        5 | 1738 | `{` |
|        - | 1739 | `	ph7_aux_data *pAux;` |
|     1795 | 1740 | `	if( pChunk == 0 ){` |
|        - | 1741 | `		/* TICKET-1433-93: NULL chunk is a harmless operation */` |
|      ! 0 | 1742 | `		return;` |
|        - | 1743 | `	}` |
|     1795 | 1744 | `	pAux = ContextFindChunk(pCtx,pChunk);` |
|     1795 | 1745 | `	if( pAux ){` |
|        - | 1746 | `		/* Mark as destroyed */` |
|      709 | 1747 | `		pAux->pAuxData = 0;` |
|      352 | 1748 | `	}` |
|     1795 | 1749 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      890 | 1750 | `}` |
|        - | 1751 | `/*` |
|        - | 1752 | ` * [CAPIREF: ph7_array_fetch()]` |
|        - | 1753 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1754 | ` */` |
|    24162 | 1755 | `ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)` |
|        5 | 1756 | `{` |
|        - | 1757 | `	ph7_hashmap_node *pNode;` |
|        - | 1758 | `	ph7_value *pValue;` |
|        - | 1759 | `	ph7_value skey;` |
|        - | 1760 | `	int rc;` |
|        - | 1761 | `	/* Make sure we are dealing with a valid hashmap */` |
|    24167 | 1762 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1763 | `		return 0;` |
|        - | 1764 | `	}` |
|    24167 | 1765 | `	if( nByte < 0 ){` |
|    18473 | 1766 | `		nByte = (int)SyStrlen(zKey);` |
|     9234 | 1767 | `	}` |
|        - | 1768 | `	/* Convert the key to a ph7_value  */` |
|    24167 | 1769 | `	PH7_MemObjInit(pArray->pVm,&skey);` |
|    24167 | 1770 | `	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);` |
|        - | 1771 | `	/* Perform the lookup */` |
|    24167 | 1772 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);` |
|    24167 | 1773 | `	PH7_MemObjRelease(&skey);` |
|    24167 | 1774 | `	if( rc != PH7_OK ){` |
|        - | 1775 | `		/* No such entry */` |
|    10665 | 1776 | `		return 0;` |
|        - | 1777 | `	}` |
|        - | 1778 | `	/* Extract the target value */` |
|    13507 | 1779 | `	pValue = (ph7_value *)PH7_MemObjAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|    13507 | 1780 | `	return pValue;` |
|    12086 | 1781 | `}` |
|        - | 1782 | `/*` |
|        - | 1783 | ` * [CAPIREF: ph7_array_walk()]` |
|        - | 1784 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1785 | ` */` |
|    69771 | 1786 | `int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)` |
|        5 | 1787 | `{` |
|        - | 1788 | `	int rc;` |
|    69776 | 1789 | `	if( xWalk == 0 ){` |
|      ! 0 | 1790 | `		return PH7_CORRUPT;` |
|        - | 1791 | `	}` |
|        - | 1792 | `	/* Make sure we are dealing with a valid hashmap */` |
|    69776 | 1793 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1794 | `		return PH7_CORRUPT;` |
|        - | 1795 | `	}` |
|        - | 1796 | `	/* Start the walk process */` |
|    69776 | 1797 | `	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);` |
|    69776 | 1798 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|    34866 | 1799 | `}` |
|        - | 1800 | `/*` |
|        - | 1801 | ` * [CAPIREF: ph7_array_add_elem()]` |
|        - | 1802 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1803 | ` */` |
|  4276671 | 1804 | `int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)` |
|        5 | 1805 | `{` |
|        - | 1806 | `	int rc;` |
|        - | 1807 | `	/* Make sure we are dealing with a valid hashmap */` |
|  4276676 | 1808 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1809 | `		return PH7_CORRUPT;` |
|        - | 1810 | `	}` |
|        - | 1811 | `	/* Perform the insertion */` |
|  4276676 | 1812 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));` |
|  4276676 | 1813 | `	return rc;` |
|  2136082 | 1814 | `}` |
|        - | 1815 | `/*` |
|        - | 1816 | ` * [CAPIREF: ph7_array_add_strkey_elem()]` |
|        - | 1817 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1818 | ` */` |
|  3349215 | 1819 | `int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)` |
|        5 | 1820 | `{` |
|        - | 1821 | `	int rc;` |
|        - | 1822 | `	/* Make sure we are dealing with a valid hashmap */` |
|  3349220 | 1823 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1824 | `		return PH7_CORRUPT;` |
|        - | 1825 | `	}` |
|        - | 1826 | `	/* Perform the insertion */` |
|  3349220 | 1827 | `	if( SX_EMPTY_STR(zKey) ){` |
|        - | 1828 | `		/* Empty key,assign an automatic index */` |
|       17 | 1829 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));` |
|        9 | 1830 | `	}else{` |
|        - | 1831 | `		ph7_value sKey;` |
|  3349204 | 1832 | `		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);` |
|  3349204 | 1833 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|  3349204 | 1834 | `		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  3349204 | 1835 | `		PH7_MemObjRelease(&sKey);` |
|        - | 1836 | `	}` |
|  3349220 | 1837 | `	return rc;` |
|  1674048 | 1838 | `}` |
|        - | 1839 | `/*` |
|        - | 1840 | ` * [CAPIREF: ph7_array_add_intkey_elem()]` |
|        - | 1841 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1842 | ` */` |
|  2129504 | 1843 | `int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)` |
|        5 | 1844 | `{` |
|        - | 1845 | `	ph7_value sKey;` |
|        - | 1846 | `	int rc;` |
|        - | 1847 | `	/* Make sure we are dealing with a valid hashmap */` |
|  2129509 | 1848 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1849 | `		return PH7_CORRUPT;` |
|        - | 1850 | `	}` |
|  2129509 | 1851 | `	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);` |
|        - | 1852 | `	/* Perform the insertion */` |
|  2129509 | 1853 | `	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));` |
|  2129509 | 1854 | `	PH7_MemObjRelease(&sKey);` |
|  2129509 | 1855 | `	return rc;` |
|  1064757 | 1856 | `}` |
|        - | 1857 | `/*` |
|        - | 1858 | ` * [CAPIREF: ph7_array_count()]` |
|        - | 1859 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1860 | ` */` |
|  1302352 | 1861 | `unsigned int ph7_array_count(ph7_value *pArray)` |
|        5 | 1862 | `{` |
|        - | 1863 | `	ph7_hashmap *pMap;` |
|        - | 1864 | `	/* Make sure we are dealing with a valid hashmap */` |
|  1302357 | 1865 | `	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 1866 | `		return 0;` |
|        - | 1867 | `	}` |
|        - | 1868 | `	/* Point to the internal representation of the hashmap */` |
|  1302357 | 1869 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  1302357 | 1870 | `	return pMap->nEntry;` |
|   651179 | 1871 | `}` |
|        - | 1872 | `/*` |
|        - | 1873 | ` * [CAPIREF: ph7_object_walk()]` |
|        - | 1874 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1875 | ` */` |
|      ! 0 | 1876 | `int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)` |
|      ! 0 | 1877 | `{` |
|        - | 1878 | `	int rc;` |
|      ! 0 | 1879 | `	if( xWalk == 0 ){` |
|      ! 0 | 1880 | `		return PH7_CORRUPT;` |
|        - | 1881 | `	}` |
|        - | 1882 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1883 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 1884 | `		return PH7_CORRUPT;` |
|        - | 1885 | `	}` |
|        - | 1886 | `	/* Start the walk process */` |
|      ! 0 | 1887 | `	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);` |
|      ! 0 | 1888 | `	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;` |
|      ! 0 | 1889 | `}` |
|        - | 1890 | `/*` |
|        - | 1891 | ` * [CAPIREF: ph7_object_fetch_attr()]` |
|        - | 1892 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1893 | ` */` |
|      705 | 1894 | `ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)` |
|        3 | 1895 | `{` |
|        - | 1896 | `	ph7_value *pValue;` |
|        - | 1897 | `	SyString sAttr;` |
|        - | 1898 | `	/* Make sure we are dealing with a valid class instance */` |
|      708 | 1899 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 \|\| zAttr == 0 ){` |
|      ! 0 | 1900 | `		return 0;` |
|        - | 1901 | `	}` |
|      708 | 1902 | `	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));` |
|        - | 1903 | `	/* Extract the attribute value if available.` |
|        - | 1904 | `	 */` |
|      708 | 1905 | `	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);` |
|      708 | 1906 | `	return pValue;` |
|      353 | 1907 | `}` |
|        - | 1908 | `/*` |
|        - | 1909 | ` * [CAPIREF: ph7_object_get_class_name()]` |
|        - | 1910 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1911 | ` */` |
|      ! 0 | 1912 | `const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)` |
|      ! 0 | 1913 | `{` |
|        - | 1914 | `	ph7_class *pClass;` |
|      ! 0 | 1915 | `	if( pLength ){` |
|      ! 0 | 1916 | `		*pLength = 0;` |
|      ! 0 | 1917 | `	}` |
|        - | 1918 | `	/* Make sure we are dealing with a valid class instance */` |
|      ! 0 | 1919 | `	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){` |
|      ! 0 | 1920 | `		return 0;` |
|        - | 1921 | `	}` |
|        - | 1922 | `	/* Point to the class */` |
|      ! 0 | 1923 | `	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;` |
|        - | 1924 | `	/* Return the class name */` |
|      ! 0 | 1925 | `	if( pLength ){` |
|      ! 0 | 1926 | `		*pLength = (int)SyStringLength(&pClass->sName);` |
|      ! 0 | 1927 | `	}` |
|      ! 0 | 1928 | `	return SyStringData(&pClass->sName);` |
|      ! 0 | 1929 | `}` |
|        - | 1930 | `/*` |
|        - | 1931 | ` * [CAPIREF: ph7_context_output()]` |
|        - | 1932 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1933 | ` */` |
|   143065 | 1934 | `int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)` |
|        5 | 1935 | `{` |
|        - | 1936 | `	SyString sData;` |
|        - | 1937 | `	int rc;` |
|   143070 | 1938 | `	if( nLen < 0 ){` |
|      ! 0 | 1939 | `		nLen = (int)SyStrlen(zString);` |
|      ! 0 | 1940 | `	}` |
|   143070 | 1941 | `	SyStringInitFromBuf(&sData,zString,nLen);` |
|   143070 | 1942 | `	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);` |
|   143070 | 1943 | `	return rc;` |
|        5 | 1944 | `}` |
|        - | 1945 | `/*` |
|        - | 1946 | ` * [CAPIREF: ph7_context_output_format()]` |
|        - | 1947 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1948 | ` */` |
|       30 | 1949 | `int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)` |
|        2 | 1950 | `{` |
|        - | 1951 | `	va_list ap;` |
|        - | 1952 | `	int rc;` |
|       32 | 1953 | `	va_start(ap,zFormat);` |
|       32 | 1954 | `	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);` |
|       32 | 1955 | `	va_end(ap);` |
|       32 | 1956 | `	return rc;` |
|        2 | 1957 | `}` |
|        - | 1958 | `/*` |
|        - | 1959 | ` * [CAPIREF: ph7_context_throw_error()]` |
|        - | 1960 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1961 | ` */` |
|      486 | 1962 | `int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)` |
|        5 | 1963 | `{` |
|      491 | 1964 | `	int rc = PH7_OK;` |
|      491 | 1965 | `	if( zErr ){` |
|      491 | 1966 | `		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);` |
|      243 | 1967 | `	}` |
|      491 | 1968 | `	return rc;` |
|        5 | 1969 | `}` |
|        - | 1970 | `/*` |
|        - | 1971 | ` * [CAPIREF: ph7_context_throw_error_format()]` |
|        - | 1972 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1973 | ` */` |
|     1274 | 1974 | `int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)` |
|        5 | 1975 | `{` |
|        - | 1976 | `	va_list ap;` |
|        - | 1977 | `	int rc;` |
|     1279 | 1978 | `	if( zFormat == 0){` |
|      ! 0 | 1979 | `		return PH7_OK;` |
|        - | 1980 | `	}` |
|     1279 | 1981 | `	va_start(ap,zFormat);` |
|     1279 | 1982 | `	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);` |
|     1279 | 1983 | `	va_end(ap);` |
|     1279 | 1984 | `	return rc;` |
|      638 | 1985 | `}` |
|        - | 1986 | `/*` |
|        - | 1987 | ` * [CAPIREF: ph7_context_random_num()]` |
|        - | 1988 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1989 | ` */` |
|      ! 0 | 1990 | `unsigned int ph7_context_random_num(ph7_context *pCtx)` |
|      ! 0 | 1991 | `{` |
|        - | 1992 | `	sxu32 n;` |
|      ! 0 | 1993 | `	n = PH7_VmRandomNum(pCtx->pVm);` |
|      ! 0 | 1994 | `	return n;` |
|      ! 0 | 1995 | `}` |
|        - | 1996 | `/*` |
|        - | 1997 | ` * [CAPIREF: ph7_context_random_string()]` |
|        - | 1998 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 1999 | ` */` |
|      ! 0 | 2000 | `int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)` |
|      ! 0 | 2001 | `{` |
|      ! 0 | 2002 | `	if( nBuflen < 3 ){` |
|      ! 0 | 2003 | `		return PH7_CORRUPT;` |
|        - | 2004 | `	}` |
|      ! 0 | 2005 | `	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);` |
|      ! 0 | 2006 | `	return PH7_OK;` |
|      ! 0 | 2007 | `}` |
|        - | 2008 | `/*` |
|        - | 2009 | ` * IMP-12-07-2012 02:10 Experimantal public API.` |
|        - | 2010 | ` *` |
|        - | 2011 | ` * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)` |
|        - | 2012 | ` * {` |
|        - | 2013 | ` *	return pCtx->pVm;` |
|        - | 2014 | ` * }` |
|        - | 2015 | ` */` |
|        - | 2016 | `/*` |
|        - | 2017 | ` * [CAPIREF: ph7_context_user_data()]` |
|        - | 2018 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2019 | ` */` |
|   100228 | 2020 | `void * ph7_context_user_data(ph7_context *pCtx)` |
|        5 | 2021 | `{` |
|   100233 | 2022 | `	return pCtx->pFunc->pUserData;` |
|        5 | 2023 | `}` |
|        - | 2024 | `/*` |
|        - | 2025 | ` * [CAPIREF: ph7_context_push_aux_data()]` |
|        - | 2026 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2027 | ` */` |
|      212 | 2028 | `int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)` |
|        1 | 2029 | `{` |
|        - | 2030 | `	ph7_aux_data sAux;` |
|        - | 2031 | `	int rc;` |
|      213 | 2032 | `	sAux.pAuxData = pUserData;` |
|      213 | 2033 | `	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);` |
|      213 | 2034 | `	return rc;` |
|        1 | 2035 | `}` |
|        - | 2036 | `/*` |
|        - | 2037 | ` * [CAPIREF: ph7_context_peek_aux_data()]` |
|        - | 2038 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2039 | ` */` |
|        4 | 2040 | `void * ph7_context_peek_aux_data(ph7_context *pCtx)` |
|        1 | 2041 | `{` |
|        - | 2042 | `	ph7_aux_data *pAux;` |
|        5 | 2043 | `	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);` |
|        5 | 2044 | `	return pAux ? pAux->pAuxData : 0;` |
|        1 | 2045 | `}` |
|        - | 2046 | `/*` |
|        - | 2047 | ` * [CAPIREF: ph7_context_pop_aux_data()]` |
|        - | 2048 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2049 | ` */` |
|      ! 0 | 2050 | `void * ph7_context_pop_aux_data(ph7_context *pCtx)` |
|      ! 0 | 2051 | `{` |
|        - | 2052 | `	ph7_aux_data *pAux;` |
|      ! 0 | 2053 | `	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);` |
|      ! 0 | 2054 | `	return pAux ? pAux->pAuxData : 0;` |
|      ! 0 | 2055 | `}` |
|        - | 2056 | `/*` |
|        - | 2057 | ` * [CAPIREF: ph7_context_result_buf_length()]` |
|        - | 2058 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2059 | ` */` |
|    71616 | 2060 | `unsigned int ph7_context_result_buf_length(ph7_context *pCtx)` |
|        5 | 2061 | `{` |
|    71621 | 2062 | `	return SyBlobLength(&pCtx->pRet->sBlob);` |
|        5 | 2063 | `}` |
|        - | 2064 | `/*` |
|        - | 2065 | ` * [CAPIREF: ph7_function_name()]` |
|        - | 2066 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2067 | ` */` |
|   222857 | 2068 | `const char * ph7_function_name(ph7_context *pCtx)` |
|        5 | 2069 | `{` |
|        - | 2070 | `	SyString *pName;` |
|   222862 | 2071 | `	pName = &pCtx->pFunc->sName;` |
|   222862 | 2072 | `	return pName->zString;` |
|        5 | 2073 | `}` |
|        - | 2074 | `/*` |
|        - | 2075 | ` * [CAPIREF: ph7_value_int()]` |
|        - | 2076 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2077 | ` */` |
|  1160463 | 2078 | `int ph7_value_int(ph7_value *pVal,int iValue)` |
|        5 | 2079 | `{` |
|        - | 2080 | `	/* Invalidate any prior representation */` |
|  1160468 | 2081 | `	PH7_MemObjRelease(pVal);` |
|  1160468 | 2082 | `	pVal->x.iVal = (ph7_int64)iValue;` |
|  1160468 | 2083 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  1160468 | 2084 | `	return PH7_OK;` |
|        5 | 2085 | `}` |
|        - | 2086 | `/*` |
|        - | 2087 | ` * [CAPIREF: ph7_value_int64()]` |
|        - | 2088 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2089 | ` */` |
|  2206733 | 2090 | `int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)` |
|        5 | 2091 | `{` |
|        - | 2092 | `	/* Invalidate any prior representation */` |
|  2206738 | 2093 | `	PH7_MemObjRelease(pVal);` |
|  2206738 | 2094 | `	pVal->x.iVal = iValue;` |
|  2206738 | 2095 | `	MemObjSetType(pVal,MEMOBJ_INT);` |
|  2206738 | 2096 | `	return PH7_OK;` |
|        5 | 2097 | `}` |
|        - | 2098 | `/*` |
|        - | 2099 | ` * [CAPIREF: ph7_value_bool()]` |
|        - | 2100 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2101 | ` */` |
|   713152 | 2102 | `int ph7_value_bool(ph7_value *pVal,int iBool)` |
|        5 | 2103 | `{` |
|        - | 2104 | `	/* Invalidate any prior representation */` |
|   713157 | 2105 | `	PH7_MemObjRelease(pVal);` |
|   713157 | 2106 | `	pVal->x.iVal = iBool ? 1 : 0;` |
|   713157 | 2107 | `	MemObjSetType(pVal,MEMOBJ_BOOL);` |
|   713157 | 2108 | `	return PH7_OK;` |
|        5 | 2109 | `}` |
|        - | 2110 | `/*` |
|        - | 2111 | ` * [CAPIREF: ph7_value_null()]` |
|        - | 2112 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2113 | ` */` |
|     2308 | 2114 | `int ph7_value_null(ph7_value *pVal)` |
|        5 | 2115 | `{` |
|        - | 2116 | `	/* Invalidate any prior representation and set the NULL flag */` |
|     2313 | 2117 | `	PH7_MemObjRelease(pVal);` |
|     2313 | 2118 | `	return PH7_OK;` |
|        5 | 2119 | `}` |
|        - | 2120 | `/*` |
|        - | 2121 | ` * [CAPIREF: ph7_value_double()]` |
|        - | 2122 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2123 | ` */` |
|     3821 | 2124 | `int ph7_value_double(ph7_value *pVal,double Value)` |
|        5 | 2125 | `{` |
|        - | 2126 | `	/* Invalidate any prior representation */` |
|     3826 | 2127 | `	PH7_MemObjRelease(pVal);` |
|     3826 | 2128 | `	pVal->rVal = (ph7_real)Value;` |
|     3826 | 2129 | `	MemObjSetType(pVal,MEMOBJ_REAL);` |
|        - | 2130 | `	/* Try to get an integer representation also */` |
|     3826 | 2131 | `	PH7_MemObjTryInteger(pVal);` |
|     3826 | 2132 | `	return PH7_OK;` |
|        5 | 2133 | `}` |
|        - | 2134 | `/*` |
|        - | 2135 | ` * [CAPIREF: ph7_value_string()]` |
|        - | 2136 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2137 | ` */` |
|  8752212 | 2138 | `int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)` |
|        5 | 2139 | `{` |
|  8752217 | 2140 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2141 | `		/* Invalidate any prior representation */` |
|  3064176 | 2142 | `		PH7_MemObjRelease(pVal);` |
|  3064176 | 2143 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|  1532076 | 2144 | `	}` |
|  8752217 | 2145 | `	if( zString ){` |
|  8731074 | 2146 | `		if( nLen < 0 ){` |
|        - | 2147 | `			/* Compute length automatically */` |
|   131018 | 2148 | `			nLen = (int)SyStrlen(zString);` |
|    65449 | 2149 | `		}` |
|        - | 2150 | `		/* Propagate allocation failure (SXERR_MEM) instead of silently` |
|        - | 2151 | `		 * fabricating a truncated success — callers can surface an OOM fatal. */` |
|  8731074 | 2152 | `		return SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);` |
|        - | 2153 | `	}` |
|    21148 | 2154 | `	return PH7_OK;` |
|  4368204 | 2155 | `}` |
|        - | 2156 | `/*` |
|        - | 2157 | ` * [CAPIREF: ph7_value_string_format()]` |
|        - | 2158 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2159 | ` */` |
|    11052 | 2160 | `int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)` |
|        5 | 2161 | `{` |
|        - | 2162 | `	va_list ap;` |
|        - | 2163 | `	int rc;` |
|    11057 | 2164 | `	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2165 | `		/* Invalidate any prior representation */` |
|    11025 | 2166 | `		PH7_MemObjRelease(pVal);` |
|    11025 | 2167 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|     5510 | 2168 | `	}` |
|    11057 | 2169 | `	va_start(ap,zFormat);` |
|    11057 | 2170 | `	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);` |
|    11057 | 2171 | `	va_end(ap);` |
|        - | 2172 | `	/* Propagate allocation failure rather than reporting a truncated success. */` |
|    11057 | 2173 | `	return rc;` |
|        5 | 2174 | `}` |
|        - | 2175 | `/*` |
|        - | 2176 | ` * [CAPIREF: ph7_value_reset_string_cursor()]` |
|        - | 2177 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2178 | ` */` |
|  3789275 | 2179 | `int ph7_value_reset_string_cursor(ph7_value *pVal)` |
|        5 | 2180 | `{` |
|        - | 2181 | `	/* Reset the string cursor */` |
|  3789280 | 2182 | `	SyBlobReset(&pVal->sBlob);` |
|  3789280 | 2183 | `	return PH7_OK;` |
|        5 | 2184 | `}` |
|        - | 2185 | `/*` |
|        - | 2186 | ` * [CAPIREF: ph7_value_resource()]` |
|        - | 2187 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2188 | ` */` |
|   104283 | 2189 | `int ph7_value_resource(ph7_value *pVal,void *pUserData)` |
|        5 | 2190 | `{` |
|        - | 2191 | `	/* Invalidate any prior representation */` |
|   104288 | 2192 | `	PH7_MemObjRelease(pVal);` |
|        - | 2193 | `	/* Reflect the new type */` |
|   104288 | 2194 | `	pVal->x.pOther = pUserData;` |
|   104288 | 2195 | `	MemObjSetType(pVal,MEMOBJ_RES);` |
|        - | 2196 | `	/* A STREAM handle is counted, and the value carries the mark that says so --` |
|        - | 2197 | `	 * see MEMOBJ_STREAMRES. Asking the question HERE is safe because the pointer` |
|        - | 2198 | `	 * has just been handed over; asking it at release time is not. */` |
|   104288 | 2199 | `	if( PH7_StreamValueRef(pUserData) ){` |
|   103732 | 2200 | `		pVal->iFlags \|= MEMOBJ_STREAMRES;` |
|    51794 | 2201 | `	}` |
|   104288 | 2202 | `	return PH7_OK;` |
|        5 | 2203 | `}` |
|        - | 2204 | `/*` |
|        - | 2205 | ` * [CAPIREF: ph7_value_release()]` |
|        - | 2206 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2207 | ` */` |
|      ! 0 | 2208 | `int ph7_value_release(ph7_value *pVal)` |
|      ! 0 | 2209 | `{` |
|      ! 0 | 2210 | `	PH7_MemObjRelease(pVal);` |
|      ! 0 | 2211 | `	return PH7_OK;` |
|      ! 0 | 2212 | `}` |
|        - | 2213 | `/*` |
|        - | 2214 | ` * [CAPIREF: ph7_value_is_int()]` |
|        - | 2215 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2216 | ` */` |
|  1349088 | 2217 | `int ph7_value_is_int(ph7_value *pVal)` |
|        5 | 2218 | `{` |
|        - | 2219 | `	/* TRUE whenever an integer representation is available, including an` |
|        - | 2220 | `	 * integer-valued real (which caches its int in MEMOBJ_INT; see` |
|        - | 2221 | `	 * PH7_MemObjTryInteger). Internal arg-extraction relies on this lenient form to` |
|        - | 2222 | `	 * accept a float where PHP would coerce. PHP's strict is_int() — which must` |
|        - | 2223 | `	 * reject floats — lives in the is_int() builtin (PH7_builtin_is_int). */` |
|  1349093 | 2224 | `	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;` |
|        5 | 2225 | `}` |
|        - | 2226 | `/*` |
|        - | 2227 | ` * [CAPIREF: ph7_value_is_float()]` |
|        - | 2228 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2229 | ` */` |
|  1386689 | 2230 | `int ph7_value_is_float(ph7_value *pVal)` |
|        5 | 2231 | `{` |
|  1386694 | 2232 | `	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;` |
|        5 | 2233 | `}` |
|        - | 2234 | `/*` |
|        - | 2235 | ` * [CAPIREF: ph7_value_is_bool()]` |
|        - | 2236 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2237 | ` */` |
|    72174 | 2238 | `int ph7_value_is_bool(ph7_value *pVal)` |
|        5 | 2239 | `{` |
|    72179 | 2240 | `	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;` |
|        5 | 2241 | `}` |
|        - | 2242 | `/*` |
|        - | 2243 | ` * [CAPIREF: ph7_value_is_string()]` |
|        - | 2244 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2245 | ` */` |
|  1495051 | 2246 | `int ph7_value_is_string(ph7_value *pVal)` |
|        5 | 2247 | `{` |
|  1495056 | 2248 | `	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;` |
|        5 | 2249 | `}` |
|        - | 2250 | `/*` |
|        - | 2251 | ` * [CAPIREF: ph7_value_is_null()]` |
|        - | 2252 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2253 | ` */` |
|  3497706 | 2254 | `int ph7_value_is_null(ph7_value *pVal)` |
|        5 | 2255 | `{` |
|  3497711 | 2256 | `	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;` |
|        5 | 2257 | `}` |
|        - | 2258 | `/*` |
|        - | 2259 | ` * [CAPIREF: ph7_value_is_numeric()]` |
|        - | 2260 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2261 | ` */` |
|    21636 | 2262 | `int ph7_value_is_numeric(ph7_value *pVal)` |
|        5 | 2263 | `{` |
|        - | 2264 | `	int rc;` |
|    21641 | 2265 | `	rc = PH7_MemObjIsNumeric(pVal);` |
|    21641 | 2266 | `	return rc;` |
|        5 | 2267 | `}` |
|        - | 2268 | `/*` |
|        - | 2269 | ` * [CAPIREF: ph7_value_is_callable()]` |
|        - | 2270 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2271 | ` */` |
|    84688 | 2272 | `int ph7_value_is_callable(ph7_value *pVal)` |
|        5 | 2273 | `{` |
|        - | 2274 | `	int rc;` |
|    84693 | 2275 | `	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);` |
|    84693 | 2276 | `	return rc;` |
|        5 | 2277 | `}` |
|        - | 2278 | `/*` |
|        - | 2279 | ` * [CAPIREF: ph7_value_is_scalar()]` |
|        - | 2280 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2281 | ` */` |
|       30 | 2282 | `int ph7_value_is_scalar(ph7_value *pVal)` |
|        1 | 2283 | `{` |
|       31 | 2284 | `	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;` |
|        1 | 2285 | `}` |
|        - | 2286 | `/*` |
|        - | 2287 | ` * [CAPIREF: ph7_value_is_array()]` |
|        - | 2288 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2289 | ` */` |
|  1088239 | 2290 | `int ph7_value_is_array(ph7_value *pVal)` |
|        5 | 2291 | `{` |
|  1088244 | 2292 | `	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;` |
|        5 | 2293 | `}` |
|        - | 2294 | `/*` |
|        - | 2295 | ` * [CAPIREF: ph7_value_is_object()]` |
|        - | 2296 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2297 | ` */` |
|   290757 | 2298 | `int ph7_value_is_object(ph7_value *pVal)` |
|        5 | 2299 | `{` |
|   290762 | 2300 | `	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|        5 | 2301 | `}` |
|        - | 2302 | `/*` |
|        - | 2303 | ` * [CAPIREF: ph7_value_is_resource()]` |
|        - | 2304 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2305 | ` */` |
|   319785 | 2306 | `int ph7_value_is_resource(ph7_value *pVal)` |
|        5 | 2307 | `{` |
|   319790 | 2308 | `	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;` |
|        5 | 2309 | `}` |
|        - | 2310 | `/*` |
|        - | 2311 | ` * [CAPIREF: ph7_value_is_empty()]` |
|        - | 2312 | ` * Please refer to the official documentation for function purpose and expected parameters.` |
|        - | 2313 | ` */` |
|    67414 | 2314 | `int ph7_value_is_empty(ph7_value *pVal)` |
|        5 | 2315 | `{` |
|        - | 2316 | `	int rc;` |
|    67419 | 2317 | `	rc = PH7_MemObjIsEmpty(pVal);` |
|    67419 | 2318 | `	return rc;` |
|        5 | 2319 | `}` |
|        - | 2320 | `/*` |
|        - | 2321 | ` * [CAPIREF: ph7_value_is_fiber()]` |
|        - | 2322 | ` * Check if a value holds a Fiber instance.` |
|        - | 2323 | ` */` |
|      ! 0 | 2324 | `int ph7_value_is_fiber(ph7_value *pVal)` |
|      ! 0 | 2325 | `{` |
|      ! 0 | 2326 | `	if( pVal == 0 \|\| pVal->pVm == 0 ) return 0;` |
|      ! 0 | 2327 | `	return PH7_VmIsFiber(pVal->pVm, pVal);` |
|      ! 0 | 2328 | `}` |
|        - | 2329 | `/*` |
|        - | 2330 | ` * [CAPIREF: ph7_fiber_start()]` |
|        - | 2331 | ` * Start a Fiber, passing arguments to the callable.` |
|        - | 2332 | ` */` |
|      ! 0 | 2333 | `int ph7_fiber_start(ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 2334 | `{` |
|      ! 0 | 2335 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|      ! 0 | 2336 | `	return PH7_VmFiberStart(pFiber->pVm, pFiber, nArg, apArg, pResult);` |
|      ! 0 | 2337 | `}` |
|        - | 2338 | `/*` |
|        - | 2339 | ` * [CAPIREF: ph7_fiber_resume()]` |
|        - | 2340 | ` * Resume a suspended Fiber, optionally sending a value.` |
|        - | 2341 | ` */` |
|      ! 0 | 2342 | `int ph7_fiber_resume(ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 2343 | `{` |
|      ! 0 | 2344 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return SXERR_CORRUPT;` |
|      ! 0 | 2345 | `	return PH7_VmFiberResume(pFiber->pVm, pFiber, pSendValue, pResult);` |
|      ! 0 | 2346 | `}` |
|        - | 2347 | `/*` |
|        - | 2348 | ` * [CAPIREF: ph7_fiber_is_suspended()]` |
|        - | 2349 | ` * Check if a Fiber is currently suspended.` |
|        - | 2350 | ` */` |
|      ! 0 | 2351 | `int ph7_fiber_is_suspended(ph7_value *pFiber)` |
|      ! 0 | 2352 | `{` |
|      ! 0 | 2353 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2354 | `	return PH7_VmFiberIsSuspended(pFiber->pVm, pFiber);` |
|      ! 0 | 2355 | `}` |
|        - | 2356 | `/*` |
|        - | 2357 | ` * [CAPIREF: ph7_fiber_is_terminated()]` |
|        - | 2358 | ` * Check if a Fiber has completed execution.` |
|        - | 2359 | ` */` |
|      ! 0 | 2360 | `int ph7_fiber_is_terminated(ph7_value *pFiber)` |
|      ! 0 | 2361 | `{` |
|      ! 0 | 2362 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2363 | `	return PH7_VmFiberIsTerminated(pFiber->pVm, pFiber);` |
|      ! 0 | 2364 | `}` |
|        - | 2365 | `/*` |
|        - | 2366 | ` * [CAPIREF: ph7_fiber_return_value()]` |
|        - | 2367 | ` * Get the return value of a terminated Fiber.` |
|        - | 2368 | ` * Returns NULL if the Fiber has not terminated.` |
|        - | 2369 | ` */` |
|      ! 0 | 2370 | `ph7_value * ph7_fiber_return_value(ph7_value *pFiber)` |
|      ! 0 | 2371 | `{` |
|      ! 0 | 2372 | `	if( pFiber == 0 \|\| pFiber->pVm == 0 ) return 0;` |
|      ! 0 | 2373 | `	return PH7_VmFiberReturnValue(pFiber->pVm, pFiber);` |
|      ! 0 | 2374 | `}` |
|        - | 2375 |  |
