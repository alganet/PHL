/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/* This file implement the public interfaces presented to host-applications.
 * Routines in other files are for internal use by PH7 and should not be
 * accessed by users of the library.
 */
#define PH7_ENGINE_MAGIC 0xF874BCD7
#define PH7_ENGINE_MISUSE(ENGINE) (ENGINE == 0 || ENGINE->nMagic != PH7_ENGINE_MAGIC)
#define PH7_VM_MISUSE(VM) (VM == 0 || VM->nMagic == PH7_VM_STALE)
/* If another thread have released a working instance,the following macros
 * evaluates to true. These macros are only used when the library
 * is built with threading support enabled which is not the case in
 * the default built.
 */
#define PH7_THRD_ENGINE_RELEASE(ENGINE) (ENGINE->nMagic != PH7_ENGINE_MAGIC)
#define PH7_THRD_VM_RELEASE(VM) (VM->nMagic == PH7_VM_STALE)
/* IMPLEMENTATION: ph7@embedded@symisc 311-12-32 */
/*
 * All global variables are collected in the structure named "sMPGlobal".
 * That way it is clear in the code when we are using static variable because
 * its name start with sMPGlobal.
 */
static struct Global_Data
{
	SyMemBackend sAllocator;                /* Global low level memory allocator */
#if defined(PH7_ENABLE_THREADS)
	const SyMutexMethods *pMutexMethods;   /* Mutex methods */
	SyMutex *pMutex;                       /* Global mutex */
	sxu32 nThreadingLevel;                 /* Threading level: 0 == Single threaded/1 == Multi-Threaded 
										    * The threading level can be set using the [ph7_lib_config()]
											* interface with a configuration verb set to
											* PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE or 
											* PH7_LIB_CONFIG_THREAD_LEVEL_MULTI
											*/
#endif
	const ph7_vfs *pVfs;                    /* Underlying virtual file system */
	sxi32 nEngine;                          /* Total number of active engines */
	ph7 *pEngines;                          /* List of active engine */
	sxu32 nMagic;                           /* Sanity check against library misuse */
}sMPGlobal = {
	{0,0,0,0,0,0,0,0,{0}},
#if defined(PH7_ENABLE_THREADS)
	0,
	0,
	0,
#endif
	0,
	0,
	0,
	0
};
#define PH7_LIB_MAGIC  0xEA1495BA
#define PH7_LIB_MISUSE (sMPGlobal.nMagic != PH7_LIB_MAGIC)
/*
 * Supported threading level.
 * These options have meaning only when the library is compiled with multi-threading
 * support.That is,the PH7_ENABLE_THREADS compile time directive must be defined
 * when PH7 is built.
 * PH7_THREAD_LEVEL_SINGLE:
 * In this mode,mutexing is disabled and the library can only be used by a single thread.
 * PH7_THREAD_LEVEL_MULTI
 * In this mode, all mutexes including the recursive mutexes on [ph7] objects
 * are enabled so that the application is free to share the same engine
 * between different threads at the same time.
 */
#define PH7_THREAD_LEVEL_SINGLE 1 
#define PH7_THREAD_LEVEL_MULTI  2
/*
 * Configure a running PH7 engine instance.
 * return PH7_OK on success.Any other return
 * value indicates failure.
 * Refer to [ph7_config()].
 */
static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)
{
	ph7_conf *pConf = &pEngine->xConf;
	int rc = PH7_OK;
	/* Perform the requested operation */
	switch(nOp){									 
	case PH7_CONFIG_ERR_OUTPUT: {
		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);
		void *pUserData = va_arg(ap,void *);
		/* Compile time error consumer routine */
		if( xConsumer == 0 ){
			rc = PH7_CORRUPT;
			break;
		}
		/* Install the error consumer */
		pConf->xErr     = xConsumer;
		pConf->pErrData = pUserData;
		break;
									 }
	case PH7_CONFIG_ERR_LOG:{
		/* Extract compile-time error log if any */
		const char **pzPtr = va_arg(ap,const char **);
		int *pLen = va_arg(ap,int *);
		if( pzPtr == 0 ){
			rc = PH7_CORRUPT;
			break;
		}
		/* NULL terminate the error-log buffer */
		SyBlobNullAppend(&pConf->sErrConsumer);
		/* Point to the error-log buffer */
		*pzPtr = (const char *)SyBlobData(&pConf->sErrConsumer);
		if( pLen ){
			if( SyBlobLength(&pConf->sErrConsumer) > 1 /* NULL '\0' terminator */ ){
				*pLen = (int)SyBlobLength(&pConf->sErrConsumer);
			}else{
				*pLen = 0;
			}
		}
		break;
							}
	case PH7_CONFIG_ERR_ABORT:
		/* Reserved for future use */
		break;
	default:
		/* Unknown configuration verb */
		rc = PH7_CORRUPT;
		break;
	} /* Switch() */
	return rc;
}
/*
 * Configure the PH7 library.
 * return PH7_OK on success.Any other return value
 * indicates failure.
 * Refer to [ph7_lib_config()].
 */
static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)
{
	int rc = PH7_OK;
	switch(nOp){
	    case PH7_LIB_CONFIG_VFS:{
			/* Install a virtual file system */
			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);
			sMPGlobal.pVfs = pVfs;
			break;
								}
		case PH7_LIB_CONFIG_USER_MALLOC: {
			/* Use an alternative low-level memory allocation routines */
			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);
			/* Save the memory failure callback (if available) */
			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;
			void *pMemErr = sMPGlobal.sAllocator.pUserData;
			if( pMethods == 0 ){
				/* Use the built-in memory allocation subsystem */
				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);
			}else{
				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);
			}
			break;
										  }
		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {
			/* Memory failure callback */
			ProcMemError xMemErr = va_arg(ap,ProcMemError);
			void *pUserData = va_arg(ap,void *);
			sMPGlobal.sAllocator.xMemError = xMemErr;
			sMPGlobal.sAllocator.pUserData = pUserData;
			break;
												 }	  
		case PH7_LIB_CONFIG_USER_MUTEX: {
#if defined(PH7_ENABLE_THREADS)
			/* Use an alternative low-level mutex subsystem */
			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);
#if defined (UNTRUST)
			if( pMethods == 0 ){
				rc = PH7_CORRUPT;
			}
#endif
			/* Sanity check */
			if( pMethods->xEnter == 0 || pMethods->xLeave == 0 || pMethods->xNew == 0){
				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */
				rc = PH7_CORRUPT;
				break;
			}
			if( sMPGlobal.pMutexMethods ){
				/* Overwrite the previous mutex subsystem */
				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);
				if( sMPGlobal.pMutexMethods->xGlobalRelease ){
					sMPGlobal.pMutexMethods->xGlobalRelease();
				}
				sMPGlobal.pMutex = 0;
			}
			/* Initialize and install the new mutex subsystem */
			if( pMethods->xGlobalInit ){
				rc = pMethods->xGlobalInit();
				if ( rc != PH7_OK ){
					break;
				}
			}
			/* Create the global mutex */
			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);
			if( sMPGlobal.pMutex == 0 ){
				/*
				 * If the supplied mutex subsystem is so sick that we are unable to
				 * create a single mutex,there is no much we can do here.
				 */
				if( pMethods->xGlobalRelease ){
					pMethods->xGlobalRelease();
				}
				rc = PH7_CORRUPT;
				break;
			}
			sMPGlobal.pMutexMethods = pMethods;			
			if( sMPGlobal.nThreadingLevel == 0 ){
				/* Set a default threading level */
				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI; 
			}
#endif
			break;
										   }
		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:
#if defined(PH7_ENABLE_THREADS)
			/* Single thread mode(Only one thread is allowed to play with the library) */
			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;
#endif
			break;
		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:
#if defined(PH7_ENABLE_THREADS)
			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines
			 * may be shared between multiple threads).
			 */
			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;
#endif
			break;
		default:
			/* Unknown configuration option */
			rc = PH7_CORRUPT;
			break;
	}
	return rc;
}
/*
 * [CAPIREF: ph7_lib_config()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_config(int nConfigOp,...)
{
	va_list ap;
	int rc;

	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){
		/* Library is already initialized,this operation is forbidden */
		return PH7_LOOKED;
	}
	va_start(ap,nConfigOp);
	rc = PH7CoreConfigure(nConfigOp,ap);
	va_end(ap);
	return rc;
}
/*
 * Global library initialization
 * Refer to [ph7_lib_init()]
 * This routine must be called to initialize the memory allocation subsystem,the mutex 
 * subsystem prior to doing any serious work with the library.The first thread to call
 * this routine does the initialization process and set the magic number so no body later
 * can re-initialize the library.If subsequent threads call this  routine before the first
 * thread have finished the initialization process, then the subsequent threads must block 
 * until the initialization process is done.
 */
static sxi32 PH7CoreInitialize(void)
{
	const ph7_vfs *pVfs; /* Built-in vfs */
#if defined(PH7_ENABLE_THREADS)
	const SyMutexMethods *pMutexMethods = 0;
	SyMutex *pMaster = 0;
#endif
	int rc;
	/*
	 * If the library is already initialized,then a call to this routine
	 * is a no-op.
	 */
	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){
		return PH7_OK; /* Already initialized */
	}
	/* Point to the built-in vfs */
	pVfs = PH7_ExportBuiltinVfs();
	/* Install it */
	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);
#if defined(PH7_ENABLE_THREADS)
	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){
		pMutexMethods = sMPGlobal.pMutexMethods;
		if( pMutexMethods == 0 ){
			/* Use the built-in mutex subsystem */
			pMutexMethods = SyMutexExportMethods();
			if( pMutexMethods == 0 ){
				return PH7_CORRUPT; /* Can't happen */
			}
			/* Install the mutex subsystem */
			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);
			if( rc != PH7_OK ){
				return rc;
			}
		}
		/* Obtain a static mutex so we can initialize the library without calling malloc() */
		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);
		if( pMaster == 0 ){
			return PH7_CORRUPT; /* Can't happen */
		}
	}
	/* Lock the master mutex */
	rc = PH7_OK;
	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){
#endif
		if( sMPGlobal.sAllocator.pMethods == 0 ){
			/* Install a memory subsystem */
			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */
			if( rc != PH7_OK ){
				/* If we are unable to initialize the memory backend,there is no much we can do here.*/
				goto End;
			}
		}
#if defined(PH7_ENABLE_THREADS)
		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
			/* Protect the memory allocation subsystem */
			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);
			if( rc != PH7_OK ){
				goto End;
			}
		}
#endif
		/* Our library is initialized,set the magic number */
		sMPGlobal.nMagic = PH7_LIB_MAGIC;
		rc = PH7_OK;
#if defined(PH7_ENABLE_THREADS)
	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */
#endif
End:
#if defined(PH7_ENABLE_THREADS)
	/* Unlock the master mutex */
	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_lib_init()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_init(void)
{
	int rc;
	rc = PH7CoreInitialize();
	return rc;
}
/*
 * Release an active PH7 engine and it's associated active virtual machines.
 */
static sxi32 EngineRelease(ph7 *pEngine)
{
	ph7_vm *pVm,*pNext;
	/* Release all active VM */
	pVm = pEngine->pVms;
	for(;;){
		if( pEngine->iVm <= 0 ){
			break;
		}
		pNext = pVm->pNext;
		PH7_VmRelease(pVm);
		pVm = pNext;
		pEngine->iVm--;
	}
	/* Set a dummy magic number */
	pEngine->nMagic = 0x7635;
	/* Release the private memory subsystem */
	SyMemBackendRelease(&pEngine->sAllocator); 
	return PH7_OK;
}
/*
 * Release all resources consumed by the library.
 * If PH7 is already shut down when this routine
 * is invoked then this routine is a harmless no-op.
 * Note: This call is not thread safe.
 * Refer to [ph7_lib_shutdown()].
 */
static void PH7CoreShutdown(void)
{
	ph7 *pEngine,*pNext;
	/* Release all active engines first */
	pEngine = sMPGlobal.pEngines;
	for(;;){
		if( sMPGlobal.nEngine < 1 ){
			break;
		}
		pNext = pEngine->pNext;
		EngineRelease(pEngine); 
		pEngine = pNext;
		sMPGlobal.nEngine--;
	}
#if defined(PH7_ENABLE_THREADS)
	/* Release the mutex subsystem */
	if( sMPGlobal.pMutexMethods ){
		if( sMPGlobal.pMutex ){
			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);
			sMPGlobal.pMutex = 0;
		}
		if( sMPGlobal.pMutexMethods->xGlobalRelease ){
			sMPGlobal.pMutexMethods->xGlobalRelease();
		}
		sMPGlobal.pMutexMethods = 0;
	}
	sMPGlobal.nThreadingLevel = 0;
#endif
	if( sMPGlobal.sAllocator.pMethods ){
		/* Release the memory backend */
		SyMemBackendRelease(&sMPGlobal.sAllocator);
	}
	sMPGlobal.nMagic = 0x1928;	
}
/*
 * [CAPIREF: ph7_lib_shutdown()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_shutdown(void)
{
	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){
		/* Already shut */
		return PH7_OK;
	}
	PH7CoreShutdown();
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_lib_is_threadsafe()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_is_threadsafe(void)
{
	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){
		return 0;
	}
#if defined(PH7_ENABLE_THREADS)
		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
			/* Muli-threading support is enabled */
			return 1;
		}else{
			/* Single-threading */
			return 0;
		}
#else
	return 0;
#endif
}
/*
 * [CAPIREF: ph7_lib_version()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_version(void)
{
	return PH7_VERSION;
}
/*
 * [CAPIREF: ph7_lib_signature()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_signature(void)
{
	return PH7_SIG;
}
/*
 * [CAPIREF: ph7_lib_ident()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_ident(void)
{
	return PH7_IDENT;
}
/*
 * [CAPIREF: ph7_lib_copyright()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_copyright(void)
{
	return PH7_COPYRIGHT;
}
/*
 * [CAPIREF: ph7_config()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_config(ph7 *pEngine,int nConfigOp,...)
{
	va_list ap;
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	 va_start(ap,nConfigOp);
	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);
	 va_end(ap);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_init()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_init(ph7 **ppEngine)
{
	ph7 *pEngine;
	int rc;
#if defined(UNTRUST)
	if( ppEngine == 0 ){
		return PH7_CORRUPT;
	}
#endif
	*ppEngine = 0;
	/* One-time automatic library initialization */
	rc = PH7CoreInitialize();
	if( rc != PH7_OK ){
		return rc;
	}
	/* Allocate a new engine */
	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));
	if( pEngine == 0 ){
		return PH7_NOMEM;
	}
	/* Zero the structure */
	SyZero(pEngine,sizeof(ph7));
	/* Initialize engine fields */
	pEngine->nMagic = PH7_ENGINE_MAGIC;
	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);
	if( rc != PH7_OK ){
		goto Release;
	}
#if defined(PH7_ENABLE_THREADS)
	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);
#endif
	/* Default configuration */
	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);
	/* Install a default compile-time error consumer routine */
	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);
	/* Built-in vfs */
	pEngine->pVfs = sMPGlobal.pVfs;
#if defined(PH7_ENABLE_THREADS)
	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
		 /* Associate a recursive mutex with this instance */
		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);
		 if( pEngine->pMutex == 0 ){
			 rc = PH7_NOMEM;
			 goto Release;
		 }
	 }
#endif
	/* Link to the list of active engines */
#if defined(PH7_ENABLE_THREADS)
	/* Enter the global mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);
	sMPGlobal.nEngine++;
#if defined(PH7_ENABLE_THREADS)
	/* Leave the global mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	/* Write a pointer to the new instance */
	*ppEngine = pEngine;
	return PH7_OK;
Release:
	SyMemBackendRelease(&pEngine->sAllocator);
	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);
	return rc;
}
/*
 * [CAPIREF: ph7_release()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_release(ph7 *pEngine)
{
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Release the engine */
	rc = EngineRelease(&(*pEngine));
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 /* Release engine mutex */
	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
#if defined(PH7_ENABLE_THREADS)
	/* Enter the global mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	/* Unlink from the list of active engines */
	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);
	sMPGlobal.nEngine--;
#if defined(PH7_ENABLE_THREADS)
	/* Leave the global mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	/* Release the memory chunk allocated to this engine */
	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);
	return rc;
}
/*
 * Compile a raw PHP script.
 * To execute a PHP code, it must first be compiled into a byte-code program using this routine.
 * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]
 * should  display the appropriate error message and this function set ppVm to null and return
 * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled
 * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].
 * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script
 * for evaluation.
 */
static sxi32 ProcessScript(
	ph7 *pEngine,          /* Running PH7 engine */
	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */
	SyString *pScript,     /* Raw PHP script to compile */
	sxi32 iFlags,          /* Compile-time flags */
	const char *zFilePath  /* File path if script come from a file. NULL otherwise */
	)
{
	ph7_vm *pVm;
	int rc;
	/* Allocate a new virtual machine */
	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));
	if( pVm == 0 ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here. */
		if( ppVm ){
			*ppVm = 0;
		}
		return PH7_NOMEM;
	}
	if( iFlags < 0 ){
		/* Default compile-time flags */
		iFlags = 0;
	}
	/* Initialize the Virtual Machine */
	rc = PH7_VmInit(pVm,&(*pEngine));
	if( rc != PH7_OK ){
		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
		if( ppVm ){
			*ppVm = 0;
		}
		return PH7_VM_ERR;
	}
	if( zFilePath ){
		/* Push processed file path */
		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);
	}
	/* Reset the error message consumer */
	SyBlobReset(&pEngine->xConf.sErrConsumer);
	/* Compile the script */
	PH7_CompileScript(pVm,&(*pScript),iFlags);
	if( pVm->sCodeGen.nErr > 0 || pVm == 0){
		sxu32 nErr = pVm->sCodeGen.nErr;
		/* Compilation error or null ppVm pointer,release this VM */
		SyMemBackendRelease(&pVm->sAllocator);
		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
		if( ppVm ){
			*ppVm = 0;
		}
		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;
	}
	/* Prepare the virtual machine for bytecode execution */
	rc = PH7_VmMakeReady(pVm);
	if( rc != PH7_OK ){
		goto Release;
	}
	/* Install local import path which is the current directory */
	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");
#if defined(PH7_ENABLE_THREADS)
	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
		 /* Associate a recursive mutex with this instance */
		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);
		 if( pVm->pMutex == 0 ){
			 goto Release;
		 }
	 }
#endif
	/* Script successfully compiled,link to the list of active virtual machines */
	MACRO_LD_PUSH(pEngine->pVms,pVm);
	pEngine->iVm++;
	/* Point to the freshly created VM */
	*ppVm = pVm;
	/* Ready to execute PH7 bytecode */
	return PH7_OK;
Release:
	SyMemBackendRelease(&pVm->sAllocator);
	SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
	*ppVm = 0;
	return PH7_VM_ERR;
}
/*
 * [CAPIREF: ph7_compile()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)
{
	SyString sScript;
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) || zSource == 0){
		return PH7_CORRUPT;
	}
	if( nLen < 0 ){
		/* Compute input length automatically */
		nLen = (int)SyStrlen(zSource);
	}
	SyStringInitFromBuf(&sScript,zSource,nLen);
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Compile the script */
	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Compilation result */
	return rc;
}
/*
 * [CAPIREF: ph7_compile_v2()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)
{
	SyString sScript;
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) || zSource == 0){
		return PH7_CORRUPT;
	}
	if( nLen < 0 ){
		/* Compute input length automatically */
		nLen = (int)SyStrlen(zSource);
	}
	SyStringInitFromBuf(&sScript,zSource,nLen);
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Compile the script */
	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Compilation result */
	return rc;
}
/*
 * [CAPIREF: ph7_compile_file()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)
{
	const ph7_vfs *pVfs;
	int rc;
	if( ppOutVm ){
		*ppOutVm = 0;
	}
	rc = PH7_OK; /* cc warning */
	if( PH7_ENGINE_MISUSE(pEngine) || SX_EMPTY_STR(zFilePath) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	 /*
	  * Check if the underlying vfs implement the memory map
	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.
	  */
	 pVfs = pEngine->pVfs;
	 if( pVfs == 0 || pVfs->xMmap == 0 ){
		 /* Memory map routine not implemented */
		 rc = PH7_IO_ERR;
	 }else{
		 void *pMapView = 0; /* cc warning */
		 ph7_int64 nSize = 0; /* cc warning */
		 SyString sScript;
		 /* Try to get a memory view of the whole file */
		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);
		 if( rc != PH7_OK ){
			 /* Assume an IO error */
			 rc = PH7_IO_ERR;
		 }else{
			 /* Compile the file */
			 SyStringInitFromBuf(&sScript,pMapView,nSize);
			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);
			 /* Release the memory view of the whole file */
			 if( pVfs->xUnmap ){
				 pVfs->xUnmap(pMapView,nSize);
			 }
		 }
	 }
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Compilation result */
	return rc;
}
/*
 * [CAPIREF: ph7_vm_dump_v2()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)
{
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#ifdef UNTRUST
	if( xConsumer == 0 ){
		return PH7_CORRUPT;
	}
#endif
	/* Dump VM instructions */
	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);
	return rc;
}
/*
 * [CAPIREF: ph7_vm_config()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)
{
	va_list ap;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Confiugure the virtual machine */
	va_start(ap,iConfigOp);
	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);
	va_end(ap);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_vm_exec()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)
{
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Execute PH7 byte-code */
	rc = PH7_VmByteCodeExec(&(*pVm));
	if( pExitStatus ){
		/* Exit status */
		*pExitStatus = pVm->iExitStatus;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Execution result */
	return rc;
}
/*
 * [CAPIREF: ph7_vm_reset()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_reset(ph7_vm *pVm)
{
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	rc = PH7_VmReset(&(*pVm));
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_vm_release()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_release(ph7_vm *pVm)
{
	ph7 *pEngine;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	pEngine = pVm->pEngine;
	rc = PH7_VmRelease(&(*pVm));
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	if( rc == PH7_OK ){
		/* Unlink from the list of active VM */
#if defined(PH7_ENABLE_THREADS)
			/* Acquire engine mutex */
			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
				PH7_THRD_ENGINE_RELEASE(pEngine) ){
					return PH7_ABORT; /* Another thread have released this instance */
			}
#endif
		MACRO_LD_REMOVE(pEngine->pVms,pVm);
		pEngine->iVm--;
		/* Release the memory chunk allocated to this VM */
		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
#if defined(PH7_ENABLE_THREADS)
			/* Leave engine mutex */
			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif	
	}
	return rc;
}
/*
 * [CAPIREF: ph7_create_function()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)
{
	SyString sName;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	/* Remove leading and trailing white spaces */
	SyStringFullTrim(&sName);
	/* Ticket 1433-003: NULL values are not allowed */
	if( sName.nByte < 1 || xFunc == 0 ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Install the foreign function */
	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData); 
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_delete_function()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_delete_function(ph7_vm *pVm,const char *zName)
{
	ph7_user_func *pFunc = 0;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Perform the deletion */
	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);
	if( rc == PH7_OK ){
		/* Release internal fields */
		SySetRelease(&pFunc->aAux);
		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));
		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_create_constant()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)
{
	SyString sName;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	/* Remove leading and trailing white spaces */
	SyStringFullTrim(&sName);
	if( sName.nByte < 1 ){
		/* Empty constant name */
		return PH7_CORRUPT;
	}
	/* TICKET 1433-003: NULL pointer harmless operation */
	if( xExpand == 0 ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Perform the registration */
	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	 return rc;
}
/*
 * [CAPIREF: ph7_delete_constant()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_delete_constant(ph7_vm *pVm,const char *zName)
{
	ph7_constant *pCons;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	 /* Query the constant hashtable */
	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);
	 if( rc == PH7_OK ){
		 /* Perform the deletion */
		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));
		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);
	 }
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_new_scalar()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_new_scalar(ph7_vm *pVm)
{
	ph7_value *pObj;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return 0;
	}
	/* Allocate a new scalar variable */
	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));
	if( pObj == 0 ){
		return 0;
	}
	/* Nullify the new scalar */
	PH7_MemObjInit(pVm,pObj);
	return pObj;
}
/*
 * [CAPIREF: ph7_new_array()] 
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_new_array(ph7_vm *pVm)
{
	ph7_hashmap *pMap;
	ph7_value *pObj;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return 0;
	}
	/* Create a new hashmap first */
	pMap = PH7_NewHashmap(&(*pVm),0,0);
	if( pMap == 0 ){
		return 0;
	}
	/* Associate a new ph7_value with this hashmap */
	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));
	if( pObj == 0 ){
		PH7_HashmapRelease(pMap,TRUE);
		return 0;
	}
	PH7_MemObjInitFromArray(pVm,pObj,pMap);
	return pObj;
}
/*
 * [CAPIREF: ph7_release_value()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)
{
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
	if( pValue ){
		/* Release the value */
		PH7_MemObjRelease(pValue);
		SyMemBackendPoolFree(&pVm->sAllocator,pValue);
	}
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_to_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_to_int(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToInteger(pValue);
	if( rc != PH7_OK ){
		return 0;
	}
	return (int)pValue->x.iVal;
}
/*
 * [CAPIREF: ph7_value_to_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_to_bool(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToBool(pValue);
	if( rc != PH7_OK ){
		return 0;
	}
	return (int)pValue->x.iVal;
}
/*
 * [CAPIREF: ph7_value_to_int64()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_int64 ph7_value_to_int64(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToInteger(pValue);
	if( rc != PH7_OK ){
		return 0;
	}
	return pValue->x.iVal;
}
/*
 * [CAPIREF: ph7_value_to_double()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
double ph7_value_to_double(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToReal(pValue);
	if( rc != PH7_OK ){
		return (double)0;
	}
	return (double)pValue->rVal;
}
/*
 * [CAPIREF: ph7_value_to_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_value_to_string(ph7_value *pValue,int *pLen)
{
	PH7_MemObjToString(pValue);
	if( SyBlobLength(&pValue->sBlob) > 0 ){
		SyBlobNullAppend(&pValue->sBlob);
		if( pLen ){
			*pLen = (int)SyBlobLength(&pValue->sBlob);
		}
		return (const char *)SyBlobData(&pValue->sBlob);
	}else{
		/* Return the empty string */
		if( pLen ){
			*pLen = 0;
		}
		return "";
	}
}
/*
 * [CAPIREF: ph7_value_to_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_value_to_resource(ph7_value *pValue)
{
	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){
		/* Not a resource,return NULL */
		return 0;
	}
	return pValue->x.pOther;
}
/*
 * [CAPIREF: ph7_value_compare()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)
{
	int rc;
	if( pLeft == 0 || pRight == 0 ){
		/* TICKET 1433-24: NULL values is harmless operation */
		return 1;
	}
	/* Perform the comparison */
	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);
	/* Comparison result */
	return rc;
}
/*
 * [CAPIREF: ph7_result_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_int(ph7_context *pCtx,int iValue)
{
	return ph7_value_int(pCtx->pRet,iValue);
}
/*
 * [CAPIREF: ph7_result_int64()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)
{
	return ph7_value_int64(pCtx->pRet,iValue);
}
/*
 * [CAPIREF: ph7_result_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_bool(ph7_context *pCtx,int iBool)
{
	return ph7_value_bool(pCtx->pRet,iBool);
}
/*
 * [CAPIREF: ph7_result_double()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_double(ph7_context *pCtx,double Value)
{
	return ph7_value_double(pCtx->pRet,Value);
}
/*
 * [CAPIREF: ph7_result_null()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_null(ph7_context *pCtx)
{
	/* Invalidate any prior representation and set the NULL flag */
	PH7_MemObjRelease(pCtx->pRet);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_result_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)
{
	return ph7_value_string(pCtx->pRet,zString,nLen);
}
/*
 * [CAPIREF: ph7_result_string_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)
{
	ph7_value *p;
	va_list ap;
	int rc;
	p = pCtx->pRet;
	if( (p->iFlags & MEMOBJ_STRING) == 0 ){
		/* Invalidate any prior representation */
		PH7_MemObjRelease(p);
		MemObjSetType(p,MEMOBJ_STRING);
	}
	/* Format the given string */
	va_start(ap,zFormat);
	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * [CAPIREF: ph7_result_value()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)
{
	int rc = PH7_OK;
	if( pValue == 0 ){
		PH7_MemObjRelease(pCtx->pRet);
	}else{
		rc = PH7_MemObjStore(pValue,pCtx->pRet);
	}
	return rc;
}
/*
 * [CAPIREF: ph7_result_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_resource(ph7_context *pCtx,void *pUserData)
{
	return ph7_value_resource(pCtx->pRet,pUserData);
}
/*
 * [CAPIREF: ph7_context_new_scalar()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_context_new_scalar(ph7_context *pCtx)
{
	ph7_value *pVal;
	pVal = ph7_new_scalar(pCtx->pVm);
	if( pVal ){
		/* Record value address so it can be freed automatically
		 * when the calling function returns. 
		 */
		SySetPut(&pCtx->sVar,(const void *)&pVal);
	}
	return pVal;
}
/*
 * [CAPIREF: ph7_context_new_array()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_context_new_array(ph7_context *pCtx)
{
	ph7_value *pVal;
	pVal = ph7_new_array(pCtx->pVm);
	if( pVal ){
		/* Record value address so it can be freed automatically
		 * when the calling function returns. 
		 */
		SySetPut(&pCtx->sVar,(const void *)&pVal);
	}
	return pVal;
}
/*
 * [CAPIREF: ph7_context_release_value()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)
{
	PH7_VmReleaseContextValue(&(*pCtx),pValue);
}
/*
 * [CAPIREF: ph7_context_alloc_chunk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)
{
	void *pChunk;
	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);
	if( pChunk ){
		if( ZeroChunk ){
			/* Zero the memory chunk */
			SyZero(pChunk,nByte);
		}
		if( AutoRelease ){
			ph7_aux_data sAux;
			/* Track the chunk so that it can be released automatically 
			 * upon this context is destroyed.
			 */
			sAux.pAuxData = pChunk;
			SySetPut(&pCtx->sChunk,(const void *)&sAux);
		}
	}
	return pChunk;
}
/*
 * Check if the given chunk address is registered in the call context
 * chunk container.
 * Return TRUE if registered.FALSE otherwise.
 * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].
 */
static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)
{
	ph7_aux_data *aAux,*pAux;
	sxu32 n;
	if( SySetUsed(&pCtx->sChunk) < 1 ){
		/* Don't bother processing,the container is empty */
		return 0;
	}
	/* Perform the lookup */
	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);
	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){
		pAux = &aAux[n];
		if( pAux->pAuxData == pChunk ){
			/* Chunk found */
			return pAux;
		}
	}
	/* No such allocated chunk */
	return 0;
}
/*
 * [CAPIREF: ph7_context_realloc_chunk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)
{
	ph7_aux_data *pAux;
	void *pNew;
	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);
	if( pNew ){
		pAux = ContextFindChunk(pCtx,pChunk);
		if( pAux ){
			pAux->pAuxData = pNew;
		}
	}
	return pNew;
}
/*
 * [CAPIREF: ph7_context_free_chunk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)
{
	ph7_aux_data *pAux;
	if( pChunk == 0 ){
		/* TICKET-1433-93: NULL chunk is a harmless operation */
		return;
	}
	pAux = ContextFindChunk(pCtx,pChunk);
	if( pAux ){
		/* Mark as destroyed */
		pAux->pAuxData = 0;
	}
	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);
}
/*
 * [CAPIREF: ph7_array_fetch()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)
{
	ph7_hashmap_node *pNode;
	ph7_value *pValue;
	ph7_value skey;
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	if( nByte < 0 ){
		nByte = (int)SyStrlen(zKey);
	}
	/* Convert the key to a ph7_value  */
	PH7_MemObjInit(pArray->pVm,&skey);
	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);
	/* Perform the lookup */
	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);
	PH7_MemObjRelease(&skey);
	if( rc != PH7_OK ){
		/* No such entry */
		return 0;
	}
	/* Extract the target value */
	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);
	return pValue;
}
/*
 * [CAPIREF: ph7_array_walk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)
{
	int rc;
	if( xWalk == 0 ){
		return PH7_CORRUPT;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	/* Start the walk process */
	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);
	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;
}
/*
 * [CAPIREF: ph7_array_add_elem()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)
{
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	/* Perform the insertion */
	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));
	return rc;
}
/*
 * [CAPIREF: ph7_array_add_strkey_elem()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)
{
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	/* Perform the insertion */
	if( SX_EMPTY_STR(zKey) ){
		/* Empty key,assign an automatic index */
		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));
	}else{
		ph7_value sKey;
		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));
		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));
		PH7_MemObjRelease(&sKey);
	}
	return rc;
}
/*
 * [CAPIREF: ph7_array_add_intkey_elem()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)
{
	ph7_value sKey;
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);
	/* Perform the insertion */
	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));
	PH7_MemObjRelease(&sKey);
	return rc;
}
/*
 * [CAPIREF: ph7_array_count()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
unsigned int ph7_array_count(ph7_value *pArray)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	return pMap->nEntry;
}
/*
 * [CAPIREF: ph7_object_walk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)
{
	int rc;
	if( xWalk == 0 ){
		return PH7_CORRUPT;
	}
	/* Make sure we are dealing with a valid class instance */
	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_CORRUPT;
	}
	/* Start the walk process */
	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);
	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;
}
/*
 * [CAPIREF: ph7_object_fetch_attr()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)
{
	ph7_value *pValue;
	SyString sAttr;
	/* Make sure we are dealing with a valid class instance */
	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 || zAttr == 0 ){
		return 0; 
	}
	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));
	/* Extract the attribute value if available.
	 */
	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);
	return pValue;
}
/*
 * [CAPIREF: ph7_object_get_class_name()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)
{
	ph7_class *pClass;
	if( pLength ){
		*pLength = 0;
	}
	/* Make sure we are dealing with a valid class instance */
	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){
		return 0; 
	}
	/* Point to the class */
	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;
	/* Return the class name */
	if( pLength ){
		*pLength = (int)SyStringLength(&pClass->sName);
	}
	return SyStringData(&pClass->sName);
}
/*
 * [CAPIREF: ph7_context_output()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)
{
	SyString sData;
	int rc;
	if( nLen < 0 ){
		nLen = (int)SyStrlen(zString);
	}
	SyStringInitFromBuf(&sData,zString,nLen);
	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);
	return rc;
}
/*
 * [CAPIREF: ph7_context_output_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)
{
	va_list ap;
	int rc;
	va_start(ap,zFormat);
	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * [CAPIREF: ph7_context_throw_error()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)
{
	int rc = PH7_OK;
	if( zErr ){
		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);
	}
	return rc;
}
/*
 * [CAPIREF: ph7_context_throw_error_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)
{
	va_list ap;
	int rc;
	if( zFormat == 0){
		return PH7_OK;
	}
	va_start(ap,zFormat);
	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * [CAPIREF: ph7_context_random_num()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
unsigned int ph7_context_random_num(ph7_context *pCtx)
{
	sxu32 n;
	n = PH7_VmRandomNum(pCtx->pVm);
	return n;
}
/*
 * [CAPIREF: ph7_context_random_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)
{
	if( nBuflen < 3 ){
		return PH7_CORRUPT;
	}
	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);
	return PH7_OK;
}
/*
 * IMP-12-07-2012 02:10 Experimantal public API.
 *
 * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)
 * {
 *	return pCtx->pVm;
 * }
 */
/*
 * [CAPIREF: ph7_context_user_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_user_data(ph7_context *pCtx)
{
	return pCtx->pFunc->pUserData;
}
/*
 * [CAPIREF: ph7_context_push_aux_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)
{
	ph7_aux_data sAux;
	int rc;
	sAux.pAuxData = pUserData;
	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);
	return rc;
}
/*
 * [CAPIREF: ph7_context_peek_aux_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_peek_aux_data(ph7_context *pCtx)
{
	ph7_aux_data *pAux;
	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);
	return pAux ? pAux->pAuxData : 0;
}
/*
 * [CAPIREF: ph7_context_pop_aux_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_pop_aux_data(ph7_context *pCtx)
{
	ph7_aux_data *pAux;
	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);
	return pAux ? pAux->pAuxData : 0;
}
/*
 * [CAPIREF: ph7_context_result_buf_length()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
unsigned int ph7_context_result_buf_length(ph7_context *pCtx)
{
	return SyBlobLength(&pCtx->pRet->sBlob);
}
/*
 * [CAPIREF: ph7_function_name()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_function_name(ph7_context *pCtx)
{
	SyString *pName;
	pName = &pCtx->pFunc->sName;
	return pName->zString;
}
/*
 * [CAPIREF: ph7_value_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_int(ph7_value *pVal,int iValue)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->x.iVal = (ph7_int64)iValue;
	MemObjSetType(pVal,MEMOBJ_INT);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_int64()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->x.iVal = iValue;
	MemObjSetType(pVal,MEMOBJ_INT);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_bool(ph7_value *pVal,int iBool)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->x.iVal = iBool ? 1 : 0;
	MemObjSetType(pVal,MEMOBJ_BOOL);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_null()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_null(ph7_value *pVal)
{
	/* Invalidate any prior representation and set the NULL flag */
	PH7_MemObjRelease(pVal);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_double()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_double(ph7_value *pVal,double Value)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->rVal = (ph7_real)Value;
	MemObjSetType(pVal,MEMOBJ_REAL);
	/* Try to get an integer representation also */
	PH7_MemObjTryInteger(pVal);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)
{
	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){
		/* Invalidate any prior representation */
		PH7_MemObjRelease(pVal);
		MemObjSetType(pVal,MEMOBJ_STRING);
	}
	if( zString ){
		if( nLen < 0 ){
			/* Compute length automatically */
			nLen = (int)SyStrlen(zString);
		}
		SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);
	}
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_string_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)
{
	va_list ap;
	int rc;
	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){
		/* Invalidate any prior representation */
		PH7_MemObjRelease(pVal);
		MemObjSetType(pVal,MEMOBJ_STRING);
	}
	va_start(ap,zFormat);
	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);
	va_end(ap);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_reset_string_cursor()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_reset_string_cursor(ph7_value *pVal)
{
	/* Reset the string cursor */
	SyBlobReset(&pVal->sBlob);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_resource(ph7_value *pVal,void *pUserData)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	/* Reflect the new type */
	pVal->x.pOther = pUserData;
	MemObjSetType(pVal,MEMOBJ_RES);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_release()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_release(ph7_value *pVal)
{
	PH7_MemObjRelease(pVal);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_is_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_int(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_float()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_float(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_bool(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_string(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_null()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_null(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_numeric()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_numeric(ph7_value *pVal)
{
	int rc;
	rc = PH7_MemObjIsNumeric(pVal);
	return rc;
}
/*
 * [CAPIREF: ph7_value_is_callable()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_callable(ph7_value *pVal)
{
	int rc;
	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);
	return rc;
}
/*
 * [CAPIREF: ph7_value_is_scalar()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_scalar(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_array()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_array(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_object()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_object(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_resource(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_empty()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_empty(ph7_value *pVal)
{
	int rc;
	rc = PH7_MemObjIsEmpty(pVal);
	return rc;
}
