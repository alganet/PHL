/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
#include <stddef.h>
#include <stdlib.h>
/*
 * The code in this file implements execution method of the PH7 Virtual Machine.
 * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program
 * which is then executed by the virtual machine implemented here to do the work of the PHP
 * statements.
 * PH7 bytecode programs are similar in form to assembly language. The program consists
 * of a linear sequence of operations .Each operation has an opcode and 3 operands.
 * Operands P1 and P2 are integers where the first is signed while the second is unsigned.
 * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually
 * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.
 * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.
 * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.
 * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.
 * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects
 * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)
 * and so on.
 * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.
 * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.
 * An implicit conversion from one type to the other occurs as necessary.
 * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does
 * the work of interpreting a PH7 bytecode program. But other routines are also provided
 * to help in building up a program instruction by instruction. Also note that sepcial
 * functions that need access to the underlying virtual machine details such as [die()],
 * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.
 */
/*
 * Each active virtual machine frame is represented by an instance 
 * of the following structure.
 * VM Frame hold local variables and other stuff related to function call.
 */
struct VmFrame
{
	VmFrame *pParent; /* Parent frame or NULL if global scope */
	void *pUserData;  /* Upper layer private data associated with this frame */
	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */
	SySet sLocal;     /* Local variables container (VmSlot instance) */
	ph7_vm *pVm;      /* VM that own this frame */
	SyHash hVar;      /* Variable hashtable for fast lookup */
	SySet sArg;       /* Function arguments container */
	SySet sRef;       /* Local reference table (VmSlot instance) */
	sxi32 iFlags;     /* Frame configuration flags (See below)*/
	sxu32 iExceptionJump; /* Exception jump destination */
};
#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */
#define VM_FRAME_THROW      0x02 /* An exception was thrown */
#define VM_FRAME_CATCH      0x04 /* Catch frame */
/*
 * When a user defined variable is released (via manual unset($x) or garbage collected)
 * memory object index is stored in an instance of the following structure and put
 * in the free object table so that it can be reused again without allocating
 * a new memory object.
 */
typedef struct VmSlot VmSlot;
struct VmSlot
{
	sxu32 nIdx;      /* Index in pVm->aMemObj[] */ 
	void *pUserData; /* Upper-layer private data */
};
/*
 * An entry in the reference table is represented by an instance of the
 * follwoing table.
 * The implementation of the reference mechanism in the PH7 engine
 * differ greatly from the one used by the zend engine. That is,
 * the reference implementation is consistent,solid and it's
 * behavior resemble the C++ reference mechanism.
 * Refer to the official for more information on this powerful
 * extension.
 */
struct VmRefObj
{
	SySet aReference;  /* Table of references to this memory object */
	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */
	sxu32 nIdx;        /* Referenced object index */
	sxi32 iFlags;      /* Configuration flags */
	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */
	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */
};
#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */
/*
 * Output control buffer entry.
 * Refer to the implementation of [ob_start()] for more information.
 */
typedef struct VmObEntry VmObEntry;
struct VmObEntry
{
	ph7_value sCallback; /* User defined callback */
	SyBlob sOB;          /* Output buffer consumer */
};
/*
 * Each installed shutdown callback (registered using [register_shutdown_function()] ) 
 * is stored in an instance of the following structure.
 * Refer to the implementation of [register_shutdown_function(()] for more information.
 */
typedef struct VmShutdownCB VmShutdownCB;
struct VmShutdownCB
{
	ph7_value sCallback; /* Shutdown callback */
	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */
	int nArg;             /* Total number of given arguments */
};
/* Uncaught exception code value */
#define PH7_EXCEPTION -255
/*
 * Each parsed URI is recorded and stored in an instance of the following structure.
 * This structure and it's related routines are taken verbatim from the xHT project
 * [A modern embeddable HTTP engine implementing all the RFC2616 methods]
 * the xHT project is developed internally by Symisc Systems.
 */
typedef struct SyhttpUri SyhttpUri;
struct SyhttpUri 
{ 
	SyString sHost;     /* Hostname or IP address */ 
	SyString sPort;     /* Port number */ 
	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */ 
	SyString sQuery;    /* Query part */	 
	SyString sFragment; /* Fragment part */ 
	SyString sScheme;   /* Scheme */ 
	SyString sUser;     /* Username */ 
	SyString sPass;     /* Password */
	SyString sRaw;      /* Raw URI */
};
/* 
 * An instance of the following structure is used to record all MIME headers seen
 * during a HTTP interaction. 
 * This structure and it's related routines are taken verbatim from the xHT project
 * [A modern embeddable HTTP engine implementing all the RFC2616 methods]
 * the xHT project is developed internally by Symisc Systems.
 */  
typedef struct SyhttpHeader SyhttpHeader;
struct SyhttpHeader 
{ 
	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */ 
	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */ 
};
/*
 * Supported HTTP methods.
 */
#define HTTP_METHOD_GET  1 /* GET */
#define HTTP_METHOD_HEAD 2 /* HEAD */
#define HTTP_METHOD_POST 3 /* POST */
#define HTTP_METHOD_PUT  4 /* PUT */
#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/
/*
 * Supported HTTP protocol version.
 */
#define HTTP_PROTO_10 1 /* HTTP/1.0 */
#define HTTP_PROTO_11 2 /* HTTP/1.1 */
/*
 * Register a constant and it's associated expansion callback so that
 * it can be expanded from the target PHP program.
 * The constant expansion mechanism under PH7 is extremely powerful yet
 * simple and work as follows:
 * Each registered constant have a C procedure associated with it.
 * This procedure known as the constant expansion callback is responsible
 * of expanding the invoked constant to the desired value,for example:
 * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).
 * The "__OS__" constant procedure expands to the name of the host Operating Systems
 * (Windows,Linux,...) and so on.
 * Please refer to the official documentation for additional information.
 */
PH7_PRIVATE sxi32 PH7_VmRegisterConstant(
	ph7_vm *pVm,            /* Target VM */
	const SyString *pName,  /* Constant name */
	ProcConstant xExpand,   /* Constant expansion callback */
	void *pUserData         /* Last argument to xExpand() */
	)
{
	ph7_constant *pCons;
	SyHashEntry *pEntry;
	char *zDupName;
	sxi32 rc;
	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);
	if( pEntry ){
		/* Overwrite the old definition and return immediately */
		pCons = (ph7_constant *)pEntry->pUserData;
		pCons->xExpand = xExpand;
		pCons->pUserData = pUserData;
		return SXRET_OK;
	}
	/* Allocate a new constant instance */
	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));
	if( pCons == 0 ){
		return 0;
	}
	/* Duplicate constant name */
	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);
	if( zDupName == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator,pCons);
		return 0;
	}
	/* Install the constant */
	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);
	pCons->xExpand = xExpand;
	pCons->pUserData = pUserData;
	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);
	if( rc != SXRET_OK ){
		SyMemBackendFree(&pVm->sAllocator,zDupName);
		SyMemBackendPoolFree(&pVm->sAllocator,pCons);
		return rc;
	}
	/* All done,constant can be invoked from PHP code */
	return SXRET_OK;
}
/*
 * Allocate a new foreign function instance.
 * This function return SXRET_OK on success. Any other
 * return value indicates failure.
 * Please refer to the official documentation for an introduction to
 * the foreign function mechanism.
 */
static sxi32 PH7_NewForeignFunction(
	ph7_vm *pVm,              /* Target VM */
	const SyString *pName,    /* Foreign function name */
	ProchHostFunction xFunc,  /* Foreign function implementation */
	void *pUserData,          /* Foreign function private data */
	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */
	)
{
	ph7_user_func *pFunc;
	char *zDup;
	/* Allocate a new user function */
	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));
	if( pFunc == 0 ){
		return SXERR_MEM;
	}
	/* Duplicate function name */
	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);
		return SXERR_MEM;
	}
	/* Zero the structure */
	SyZero(pFunc,sizeof(ph7_user_func));
	/* Initialize structure fields */
	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);
	pFunc->pVm   = pVm;
	pFunc->xFunc = xFunc;
	pFunc->pUserData = pUserData;
	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));
	/* Write a pointer to the new function */
	*ppOut = pFunc;
	return SXRET_OK;
}
/*
 * Install a foreign function and it's associated callback so that
 * it can be invoked from the target PHP code.
 * This function return SXRET_OK on successful registration. Any other
 * return value indicates failure.
 * Please refer to the official documentation for an introduction to
 * the foreign function mechanism.
 */
PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(
	ph7_vm *pVm,              /* Target VM */
	const SyString *pName,    /* Foreign function name */
	ProchHostFunction xFunc,  /* Foreign function implementation */
	void *pUserData           /* Foreign function private data */
	)
{
	ph7_user_func *pFunc;
	SyHashEntry *pEntry;
	sxi32 rc;
	/* Overwrite any previously registered function with the same name */
	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);
	if( pEntry ){
		pFunc = (ph7_user_func *)pEntry->pUserData;
		pFunc->pUserData = pUserData;
		pFunc->xFunc = xFunc;
		SySetReset(&pFunc->aAux);
		return SXRET_OK;
	}
	/* Create a new user function */
	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Install the function in the corresponding hashtable */
	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);
	if( rc != SXRET_OK ){
		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));
		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);
		return rc;
	}
	/* User function successfully installed */
	return SXRET_OK;
}
/*
 * Initialize a VM function.
 */
PH7_PRIVATE sxi32 PH7_VmInitFuncState(
	ph7_vm *pVm,        /* Target VM */
	ph7_vm_func *pFunc, /* Target Fucntion */
	const char *zName,  /* Function name */
	sxu32 nByte,        /* zName length */
	sxi32 iFlags,       /* Configuration flags */
	void *pUserData     /* Function private data */
	)
{
	/* Zero the structure */
	SyZero(pFunc,sizeof(ph7_vm_func));	
	/* Initialize structure fields */
	/* Arguments container */
	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));
	/* Static variable container */
	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));
	/* Bytecode container */
	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));
    /* Preallocate some instruction slots */
	SySetAlloc(&pFunc->aByteCode,0x10);
	/* Closure environment */
	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));
	pFunc->iFlags = iFlags;
	pFunc->pUserData = pUserData;
	SyStringInitFromBuf(&pFunc->sName,zName,nByte);
	return SXRET_OK;
}
/*
 * Install a user defined function in the corresponding VM container.
 */
PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(
	ph7_vm *pVm,        /* Target VM */
	ph7_vm_func *pFunc, /* Target function */
	SyString *pName     /* Function name */
	)
{
	SyHashEntry *pEntry;
	sxi32 rc;
	if( pName == 0 ){
		/* Use the built-in name */
		pName = &pFunc->sName;
	}
	/* Check for duplicates (functions with the same name) first */
	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);
	if( pEntry ){
		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;
		if( pLink != pFunc ){
			/* Link */
			pFunc->pNextName = pLink;
			pEntry->pUserData = pFunc;
		}
		return SXRET_OK;
	}
	/* First time seen */
	pFunc->pNextName = 0;
	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);
	return rc;
}
/*
 * Install a user defined class in the corresponding VM container.
 */
PH7_PRIVATE sxi32 PH7_VmInstallClass(
	ph7_vm *pVm,      /* Target VM  */
	ph7_class *pClass /* Target Class */
	)
{
	SyString *pName = &pClass->sName;
	SyHashEntry *pEntry;
	sxi32 rc;
	/* Check for duplicates */
	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);
	if( pEntry ){
		ph7_class *pLink = (ph7_class *)pEntry->pUserData;
		/* Link entry with the same name */
		pClass->pNextName = pLink;
		pEntry->pUserData = pClass;
		return SXRET_OK;
	}
	pClass->pNextName = 0;
	/* Perform a simple hashtable insertion */
	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);
	return rc;
}
/*
 * Instruction builder interface.
 */
PH7_PRIVATE sxi32 PH7_VmEmitInstr(
	ph7_vm *pVm,  /* Target VM */
	sxi32 iOp,    /* Operation to perform */
	sxi32 iP1,    /* First operand */
	sxu32 iP2,    /* Second operand */
	void *p3,     /* Third operand */
	sxu32 *pIndex /* Instruction index. NULL otherwise */
	)
{
	VmInstr sInstr;
	sxi32 rc;
	/* Fill the VM instruction */
	sInstr.iOp = (sxu8)iOp; 
	sInstr.iP1 = iP1; 
	sInstr.iP2 = iP2; 
	sInstr.p3  = p3;  
	if( pIndex ){
		/* Instruction index in the bytecode array */
		*pIndex = SySetUsed(pVm->pByteContainer);
	}
	/* Finally,record the instruction */
	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");
		/* Fall throw */
	}
	return rc;
}
/*
 * Swap the current bytecode container with the given one.
 */
PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)
{
	if( pContainer == 0 ){
		/* Point to the default container */
		pVm->pByteContainer = &pVm->aByteCode;
	}else{
		/* Change container */
		pVm->pByteContainer = &(*pContainer);
	}
	return SXRET_OK;
}
/*
 * Return the current bytecode container.
 */
PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)
{
	return pVm->pByteContainer;
}
/*
 * Extract the VM instruction rooted at nIndex.
 */
PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)
{
	VmInstr *pInstr;
	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);
	return pInstr;
}
/*
 * Return the total number of VM instructions recorded so far.
 */
PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)
{
	return SySetUsed(pVm->pByteContainer);
}
/*
 * Pop the last VM instruction.
 */
PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)
{
	return (VmInstr *)SySetPop(pVm->pByteContainer);
}
/*
 * Peek the last VM instruction.
 */
PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)
{
	return (VmInstr *)SySetPeek(pVm->pByteContainer);
}
PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)
{
	VmInstr *aInstr;
	sxu32 n;
	n = SySetUsed(pVm->pByteContainer);
	if( n < 2 ){
		return 0;
	}
	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);
	return &aInstr[n - 2];
}
/*
 * Allocate a new virtual machine frame.
 */
static VmFrame * VmNewFrame(
	ph7_vm *pVm,              /* Target VM */
	void *pUserData,          /* Upper-layer private data */
	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */
	)
{
	VmFrame *pFrame;
	/* Allocate a new vm frame */
	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));
	if( pFrame == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pFrame,sizeof(VmFrame));
	/* Initialize frame fields */
	pFrame->pUserData = pUserData;
	pFrame->pThis = pThis;
	pFrame->pVm = pVm;
	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);
	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));
	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));
	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));
	return pFrame;
}
/*
 * Enter a VM frame.
 */
static sxi32 VmEnterFrame(
	ph7_vm *pVm,               /* Target VM */
	void *pUserData,           /* Upper-layer private data */
	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */
	VmFrame **ppFrame          /* OUT: Top most active frame */
	)
{
	VmFrame *pFrame;
	/* Allocate a new frame */
	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);
	if( pFrame == 0 ){
		return SXERR_MEM;
	}
	/* Link to the list of active VM frame */
	pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pFrame;
	if( ppFrame ){
		/* Write a pointer to the new VM frame */
		*ppFrame = pFrame;
	}
	return SXRET_OK;
}
/*
 * Link a foreign variable with the TOP most active frame.
 * Refer to the PH7_OP_UPLINK instruction implementation for more
 * information.
 */
static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)
{
	VmFrame *pTarget,*pFrame;
	SyHashEntry *pEntry = 0;
	sxi32 rc;
	/* Point to the upper frame */
	pFrame = pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	pTarget = pFrame;
	pFrame = pTarget->pParent;
	while( pFrame ){
		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){
			/* Query the current frame */
			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);	
			if( pEntry ){
				/* Variable found */
				break;
			}
		}
		/* Point to the upper frame */
		pFrame = pFrame->pParent;
	}
	if( pEntry == 0 ){
		/* Inexistant variable */
		return SXERR_NOTFOUND;
	}
	/* Link to the current frame */
	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);
	if( rc == SXRET_OK ){
		sxu32 nIdx;
		nIdx = SX_PTR_TO_INT(pEntry->pUserData);
		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);
	}
	return rc;
}
/*
 * Leave the top-most active frame.
 */
static void VmLeaveFrame(ph7_vm *pVm)
{
	VmFrame *pFrame = pVm->pFrame;
	if( pFrame ){
		/* Unlink from the list of active VM frame */
		pVm->pFrame = pFrame->pParent;
		if( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){
			VmSlot  *aSlot;
			sxu32 n;
			/* Restore local variable to the free pool so that they can be reused again */
			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);
			for(n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){
				/* Unset the local variable */
				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);
			}
			/* Remove local reference */
			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);
			for(n = 0 ; n < SySetUsed(&pFrame->sRef) ; ++n ){
				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);
			}
		}
		/* Release internal containers */
		SyHashRelease(&pFrame->hVar);
		SySetRelease(&pFrame->sArg);
		SySetRelease(&pFrame->sLocal);
		SySetRelease(&pFrame->sRef);
		/* Release the whole structure */
		SyMemBackendPoolFree(&pVm->sAllocator,pFrame);
	}
}
/*
 * Compare two functions signature and return the comparison result.
 */
static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)
{
	const char *zSend = &pSecond->zString[pSecond->nByte];
	const char *zFend = &pFirst->zString[pFirst->nByte];
	const char *zSin = pSecond->zString;
	const char *zFin = pFirst->zString;
	const char *zPtr = zFin;
	for(;;){
		if( zFin >= zFend || zSin >= zSend ){
			break;
		}
		if( zFin[0] != zSin[0] ){
			/* mismatch */
			break;
		}
		zFin++;
		zSin++;
	}
	return (int)(zFin-zPtr);
}
/*
 * Select the appropriate VM function for the current call context.
 * This is the implementation of the powerful 'function overloading' feature
 * introduced by the version 2 of the PH7 engine.
 * Refer to the official documentation for more information.
 */
static ph7_vm_func * VmOverload(
	ph7_vm *pVm,         /* Target VM */
	ph7_vm_func *pList,  /* Linked list of candidates for overloading */
	ph7_value *aArg,     /* Array of passed arguments */
	int nArg             /* Total number of passed arguments  */
	)
{
	int iTarget,i,j,iCur,iMax;
	ph7_vm_func *apSet[10];   /* Maximum number of candidates */
	ph7_vm_func *pLink;
	SyString sArgSig;
	SyBlob sSig;

	pLink = pList;
	i = 0;
	/* Put functions expecting the same number of passed arguments */
	while( i < (int)SX_ARRAYSIZE(apSet) ){
		if( pLink == 0 ){
			break;
		}
		if( (int)SySetUsed(&pLink->aArgs) == nArg ){
			/* Candidate for overloading */
			apSet[i++] = pLink;
		}
		/* Point to the next entry */
		pLink = pLink->pNextName;
	}
	if( i < 1 ){
		/* No candidates,return the head of the list */
		return pList;
	}
	if( nArg < 1 || i < 2 ){
		/* Return the only candidate */
		return apSet[0];
	}
	/* Calculate function signature */
	SyBlobInit(&sSig,&pVm->sAllocator);
	for( j = 0 ; j < nArg ; j++ ){
		int c = 'n'; /* null */
		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){
			/* Hashmap */
			c = 'h';
		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){
			/* bool */
			c = 'b';
		}else if( aArg[j].iFlags & MEMOBJ_INT ){
			/* int */
			c = 'i';
		}else if( aArg[j].iFlags & MEMOBJ_STRING ){
			/* String */
			c = 's';
		}else if( aArg[j].iFlags & MEMOBJ_REAL ){
			/* Float */
			c = 'f';
		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){
			/* Class instance */
			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;
			SyString *pName = &pClass->sName;
			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);
			c = -1;
		}
		if( c > 0 ){
			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));
		}
	}
	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));
	iTarget = 0;
	iMax = -1;
	/* Select the appropriate function */
	for( j = 0 ; j < i ; j++ ){
		/* Compare the two signatures */
		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);
		if( iCur > iMax ){
			iMax = iCur;
			iTarget = j;
		}
	}
	SyBlobRelease(&sSig);
	/* Appropriate function for the current call context */
	return apSet[iTarget];
}
/* Forward declaration */
static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);
static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);
/*
 * Mount a compiled class into the freshly created vitual machine so that
 * it can be instanciated from the executed PHP script.
 */
static sxi32 VmMountUserClass(
	ph7_vm *pVm,      /* Target VM */
	ph7_class *pClass /* Class to be mounted */
	)
{
	ph7_class_method *pMeth;
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
	sxi32 rc;
	/* Reset the loop cursor */
	SyHashResetLoopCursor(&pClass->hAttr);
	/* Process only static and constant attribute */
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		/* Extract the current attribute */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
			ph7_value *pMemObj;
			/* Reserve a memory object for this constant/static attribute */
			pMemObj = PH7_ReserveMemObj(&(*pVm));
			if( pMemObj == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,
					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",
					&pClass->sName,&pAttr->sName
					);
				return SXERR_MEM;
			}
			if( SySetUsed(&pAttr->aByteCode) > 0 ){
				/* Initialize attribute default value (any complex expression) */ 
				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);
			}
			/* Record attribute index */
			pAttr->nIdx = pMemObj->nIdx;
			/* Install static attribute in the reference table */
			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);
		}
	}
	/* Install class methods */
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){
		/* Do not mount interface methods since they are signatures only.
		 */
		return SXRET_OK;
	}
	/* Create constructor alias if not yet done */
	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){
		/* User constructor with the same base class name */
		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));
		if( pEntry ){
			pMeth = (ph7_class_method *)pEntry->pUserData;
			/* Create the alias */
			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);
		}
	}
	/* Install the methods now */
	SyHashResetLoopCursor(&pClass->hMethod);
	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		pMeth = (ph7_class_method *)pEntry->pUserData;
		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){
			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	return SXRET_OK;
}
/*
 * Allocate a private frame for attributes of the given
 * class instance (Object in the PHP jargon).
 */
PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(
	ph7_vm *pVm, /* Target VM */
	ph7_class_instance *pObj /* Class instance */
	)
{
	ph7_class *pClass = pObj->pClass;
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
	sxi32 rc;
	/* Install class attribute in the private frame associated with this instance */
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		VmClassAttr *pVmAttr;
		/* Extract the current attribute */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));
		if( pVmAttr == 0 ){
			return SXERR_MEM;
		}
		pVmAttr->pAttr = pAttr;
		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC)) == 0 ){
			ph7_value *pMemObj;
			/* Reserve a memory object for this attribute */
			pMemObj = PH7_ReserveMemObj(&(*pVm));
			if( pMemObj == 0 ){
				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
				return SXERR_MEM;
			}
			pVmAttr->nIdx = pMemObj->nIdx;
			if( SySetUsed(&pAttr->aByteCode) > 0 ){
				/* Initialize attribute default value (any complex expression) */ 
				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);
			}
			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);
			if( rc != SXRET_OK ){
				VmSlot sSlot;
				/* Restore memory object */
				sSlot.nIdx = pMemObj->nIdx;
				sSlot.pUserData = 0;
				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);
				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
				return SXERR_MEM;
			}
			/* Install attribute in the reference table */
			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);
		}else{
			/* Install static/constant attribute */
			pVmAttr->nIdx = pAttr->nIdx;
			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);
			if( rc != SXRET_OK ){
				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
				return SXERR_MEM;
			}
		}
	}
	return SXRET_OK;
}
/* Forward declaration */
static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);
static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);
/* 
 * Dummy read-only buffer used for slot reservation.
 */
static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */ 
/*
 * Reserve a constant memory object.
 * Return a pointer to the raw ph7_value on success. NULL on failure.
 */
PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)
{
	ph7_value *pObj;
	sxi32 rc;
	if( pIndex ){
		/* Object index in the object table */
		*pIndex = SySetUsed(&pVm->aLitObj);
	}
	/* Reserve a slot for the new object */
	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);
	if( rc != SXRET_OK ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here.
		 */
		return 0;
	}
	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);
	return pObj;
}
/*
 * Reserve a memory object.
 * Return a pointer to the raw ph7_value on success. NULL on failure.
 */
PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)
{
	ph7_value *pObj;
	sxi32 rc;
	if( pIndex ){
		/* Object index in the object table */
		*pIndex = SySetUsed(&pVm->aMemObj);
	}
	/* Reserve a slot for the new object */
	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);
	if( rc != SXRET_OK ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here.
		 */
		return 0;
	}
	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);
	return pObj;
}
/* Forward declaration */
static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);
/*
 * Built-in classes/interfaces and some functions that cannot be implemented
 * directly as foreign functions.
 */
#define PH7_BUILTIN_LIB \
	"class Exception { "\
    "protected $message = 'Unknown exception';"\
    "protected $code = 0;"\
    "protected $file;"\
    "protected $line;"\
    "protected $trace;"\
    "protected $previous;"\
	"public function __construct($message = null, $code = 0, Exception $previous = null){"\
	"   if( isset($message) ){"\
	"	  $this->message = $message;"\
	"   }"\
	"   $this->code = $code;"\
	"   $this->file = __FILE__;"\
	"   $this->line = __LINE__;"\
	"   $this->trace = debug_backtrace();"\
	"   if( isset($previous) ){"\
	"     $this->previous = $previous;"\
	"   }"\
	"}"\
	"public function getMessage(){"\
	"   return $this->message;"\
	"}"\
	" public function getCode(){"\
	"  return $this->code;"\
	"}"\
	"public function getFile(){"\
	"  return $this->file;"\
	"}"\
	"public function getLine(){"\
	"  return $this->line;"\
	"}"\
	"public function getTrace(){"\
	"   return $this->trace;"\
	"}"\
	"public function getTraceAsString(){"\
	"  return debug_string_backtrace();"\
	"}"\
	"public function getPrevious(){"\
	"    return $this->previous;"\
	"}"\
	"public function __toString(){"\
	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\
    "}"\
	"}"\
	"class ErrorException extends Exception { "\
	"protected $severity;"\
	"public function __construct(string $message = null,"\
	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\
	"   if( isset($message) ){"\
	"	  $this->message = $message;"\
	"   }"\
	"   $this->severity = $severity;"\
	"   $this->code = $code;"\
	"   $this->file = $filename;"\
	"   $this->line = $lineno;"\
	"   $this->trace = debug_backtrace();"\
	"   if( isset($previous) ){"\
	"     $this->previous = $previous;"\
	"   }"\
	"}"\
	"public function getSeverity(){"\
	"   return $this->severity;"\
    "}"\
	"}"\
	"interface Iterator {"\
	"public function current();"\
	"public function key();"\
	"public function next();"\
	"public function rewind();"\
	"public function valid();"\
	"}"\
	"interface IteratorAggregate {"\
	"public function getIterator();"\
	"}"\
	"interface Serializable {"\
	"public function serialize();"\
	"public function unserialize(string $serialized);"\
	"}"\
	"/* Directory releated IO */"\
	"class Directory {"\
	"public $handle = null;"\
	"public $path  = null;"\
	"public function __construct(string $path)"\
	"{"\
	"   $this->handle = opendir($path);"\
	"   if( $this->handle !== FALSE ){"\
	"      $this->path = $path;"\
	"   }"\
	"}"\
	"public function __destruct()"\
	"{"\
	"  if( $this->handle != null ){"\
	"       closedir($this->handle);"\
	"  }"\
	"}"\
	"public function read()"\
	"{"\
	"    return readdir($this->handle);"\
	"}"\
	"public function rewind()"\
	"{"\
	"    rewinddir($this->handle);"\
	"}"\
	"public function close()"\
	"{"\
	"    closedir($this->handle);"\
	"    $this->handle = null;"\
	"}"\
	"}"\
	"class stdClass{"\
	"  public $value;"\
	" /* Magic methods */"\
	" public function __toInt(){ return (int)$this->value; }"\
	" public function __toBool(){ return (bool)$this->value; }"\
	" public function __toFloat(){ return (float)$this->value; }"\
	" public function __toString(){ return (string)$this->value; }"\
	" function __construct($v){ $this->value = $v; }"\
	"}"\
	"function dir(string $path){"\
	"   return new Directory($path);"\
	"}"\
	"function Dir(string $path){"\
	"   return new Directory($path);"\
	"}"\
	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\
    "{"\
	"  if( func_num_args() < 1 ){ return FALSE; }"\
	"  $aDir = array();"\
	"  $pHandle = opendir($directory);"\
	"  if( $pHandle == FALSE ){ return FALSE; }"\
	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\
	"      $aDir[] = $pEntry;"\
	"   }"\
	"  closedir($pHandle);"\
	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\
	"      rsort($aDir);"\
	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\
	"      sort($aDir);"\
	"  }"\
	"  return $aDir;"\
	"}"\
	"function glob(string $pattern,int $iFlags = 0){"\
	"/* Open the target directory */"\
	"$zDir = dirname($pattern);"\
	"if(!is_string($zDir) ){ $zDir = './'; }"\
	"$pHandle = opendir($zDir);"\
	"if( $pHandle == FALSE ){"\
	"   /* IO error while opening the current directory,return FALSE */"\
	"	return FALSE;"\
	"}"\
	"$pattern = basename($pattern);"\
	"$pArray = array(); /* Empty array */"\
	"/* Loop throw available entries */"\
	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\
	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\
	"	$rc = strglob($pattern,$pEntry);"\
	"	if( $rc ){"\
	"	   if( is_dir($pEntry) ){"\
	"	      if( $iFlags & GLOB_MARK ){"\
	"		     /* Adds a slash to each directory returned */"\
	"			 $pEntry .= DIRECTORY_SEPARATOR;"\
	"		  }"\
	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\
	"	     /* Not a directory,ignore */"\
	"		 continue;"\
	"	   }"\
	"	   /* Add the entry */"\
	"	   $pArray[] = $pEntry;"\
	"	}"\
	" }"\
	"/* Close the handle */"\
	"closedir($pHandle);"\
	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\
	"  /* Sort the array */"\
	"  sort($pArray);"\
	"}"\
	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\
	"  /* Return the search pattern if no files matching were found */"\
	"  $pArray[] = $pattern;"\
	"}"\
	"/* Return the created array */"\
	"return $pArray;"\
   "}"\
   "/* Creates a temporary file */"\
   "function tmpfile(){"\
   "  /* Extract the temp directory */"\
   "  $zTempDir = sys_get_temp_dir();"\
   "  if( strlen($zTempDir) < 1 ){"\
   "    /* Use the current dir */"\
   "    $zTempDir = '.';"\
   "  }"\
   "  /* Create the file */"\
   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\
   "  return $pHandle;"\
   "}"\
   "/* Creates a temporary filename */"\
   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\
   "{"\
   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\
   "}"\
   "function array_unshift(&$pArray ){"\
   " if( func_num_args() < 1 || !is_array($pArray) ){  return 0; }"\
   "/* Copy arguments */"\
   "$nArgs = func_num_args();"\
   "$pNew = array();"\
   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\
    " $pNew[] = func_get_arg($i);"\
    "}"\
   	"/* Make a copy of the old entries */"\
	"$pOld = array_copy($pArray);"\
	"/* Erase */"\
	"array_erase($pArray);"\
	"/* Unshift */"\
	"$pArray = array_merge($pNew,$pOld);"\
	"return sizeof($pArray);"\
    "}"\
	"function array_merge_recursive($array1, $array2){"\
	"if( func_num_args() < 1 ){ return NULL; }"\
    "$arrays = func_get_args();"\
    "$narrays = count($arrays);"\
    "$ret = $arrays[0];"\
    "for ($i = 1; $i < $narrays; $i++) {"\
	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\
     " foreach ($arrays[$i] as $key => $value) {"\
     "  if (((string) $key) === ((string) intval($key))) {"\
     "   $ret[] = $value;"\
     "  }else{"\
     "  if (is_array($value) && isset($ret[$key]) ) {"\
     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\
     " }else {"\
     "   $ret[$key] = $value;"\
     "  }"\
     " }"\
     " }"\
	 "}"\
	 " return $ret;"\
    "}"\
	"function max(){"\
    "  $pArgs = func_get_args();"\
    " if( sizeof($pArgs) < 1 ){"\
	"  return null;"\
    " }"\
    " if( sizeof($pArgs) < 2 ){"\
    " $pArg = $pArgs[0];"\
	" if( !is_array($pArg) ){"\
	"   return $pArg; "\
	" }"\
	" if( sizeof($pArg) < 1 ){"\
	"   return null;"\
	" }"\
	" $pArg = array_copy($pArgs[0]);"\
	" reset($pArg);"\
	" $max = current($pArg);"\
	" while( FALSE !== ($val = next($pArg)) ){"\
	"   if( $val > $max ){"\
	"     $max = $val;"\
    " }"\
	" }"\
	" return $max;"\
    " }"\
    " $max = $pArgs[0];"\
    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\
    " $val = $pArgs[$i];"\
	"if( $val > $max ){"\
	" $max = $val;"\
	"}"\
    " }"\
	" return $max;"\
    "}"\
	"function min(){"\
    "  $pArgs = func_get_args();"\
    " if( sizeof($pArgs) < 1 ){"\
	"  return null;"\
    " }"\
    " if( sizeof($pArgs) < 2 ){"\
    " $pArg = $pArgs[0];"\
	" if( !is_array($pArg) ){"\
	"   return $pArg; "\
	" }"\
	" if( sizeof($pArg) < 1 ){"\
	"   return null;"\
	" }"\
	" $pArg = array_copy($pArgs[0]);"\
	" reset($pArg);"\
	" $min = current($pArg);"\
	" while( FALSE !== ($val = next($pArg)) ){"\
	"   if( $val < $min ){"\
	"     $min = $val;"\
    " }"\
	" }"\
	" return $min;"\
    " }"\
    " $min = $pArgs[0];"\
    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\
    " $val = $pArgs[$i];"\
	"if( $val < $min ){"\
	" $min = $val;"\
	" }"\
    " }"\
	" return $min;"\
	"}"\
	"function fileowner(string $file){"\
    " $a = stat($file);"\
	" if( !is_array($a) ){"\
	"	return false;"\
	" }"\
	" return $a['uid'];"\
    "}"\
    "function filegroup(string $file){"\
	" $a = stat($file);"\
	" if( !is_array($a) ){"\
	"	return false;"\
	" }"\
	" return $a['gid'];"\
    "}"\
	 "function fileinode(string $file){"\
	" $a = stat($file);"\
	" if( !is_array($a) ){"\
	"	return false;"\
	" }"\
	" return $a['ino'];"\
    "}"

/*
 * Initialize a freshly allocated PH7 Virtual Machine so that we can
 * start compiling the target PHP program.
 */
PH7_PRIVATE sxi32 PH7_VmInit(
	 ph7_vm *pVm, /* Initialize this */
	 ph7 *pEngine /* Master engine */
	 )
{
	SyString sBuiltin;
	ph7_value *pObj;
	sxi32 rc;
	/* Zero the structure */
	SyZero(pVm,sizeof(ph7_vm));
	/* Initialize VM fields */
	pVm->pEngine = &(*pEngine);
	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);
	/* Instructions containers */
	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));
	SySetAlloc(&pVm->aByteCode,0xFF);
	pVm->pByteContainer = &pVm->aByteCode;
	/* Object containers */
	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));
	SySetAlloc(&pVm->aMemObj,0xFF);
	/* Virtual machine internal containers */
	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);
	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);
	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);
	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));
	SySetAlloc(&pVm->aLitObj,0xFF);
	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);
	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);
	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));
	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));
	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));
	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));
	/* Configuration containers */
	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));
	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));
	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));
	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));
	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));
	/* Error callbacks containers */
	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);
	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);
	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);
	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);
	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);
	/* Set a default recursion limit */
#if defined(__WINNT__) || defined(__UNIXES__)
	pVm->nMaxDepth = 32;
#else
	pVm->nMaxDepth = 16;
#endif
	/* Default assertion flags */
	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */
	/* JSON return status */
	pVm->json_rc = JSON_ERROR_NONE;
	/* PRNG context */
	SyRandomnessInit(&pVm->sPrng,0,0);
	/* Install the null constant */
	pObj = PH7_ReserveConstObj(&(*pVm),0);
	if( pObj == 0 ){
		rc = SXERR_MEM;
		goto Err;
	}
	PH7_MemObjInit(pVm,pObj);
	/* Install the boolean TRUE constant */
	pObj = PH7_ReserveConstObj(&(*pVm),0);
	if( pObj == 0 ){
		rc = SXERR_MEM;
		goto Err;
	}
	PH7_MemObjInitFromBool(pVm,pObj,1);
	/* Install the boolean FALSE constant */
	pObj = PH7_ReserveConstObj(&(*pVm),0);
	if( pObj == 0 ){
		rc = SXERR_MEM;
		goto Err;
	}
	PH7_MemObjInitFromBool(pVm,pObj,0);
	/* Create the global frame */
	rc = VmEnterFrame(&(*pVm),0,0,0);
	if( rc != SXRET_OK ){
		goto Err;
	}
	/* Initialize the code generator */
	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);
	if( rc != SXRET_OK ){
		goto Err;
	}
	/* VM correctly initialized,set the magic number */
	pVm->nMagic = PH7_VM_INIT;
	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);
	/* Compile the built-in library */
	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);
	/* Reset the code generator */
	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);
	return SXRET_OK;
Err:
	SyMemBackendRelease(&pVm->sAllocator);
	return rc;
}
/*
 * Default VM output consumer callback.That is,all VM output is redirected to this
 * routine which store the output in an internal blob.
 * The output can be extracted later after program execution [ph7_vm_exec()] via
 * the [ph7_vm_config()] interface with a configuration verb set to
 * PH7_VM_CONFIG_EXTRACT_OUTPUT.
 * Refer to the official docurmentation for additional information.
 * Note that for performance reason it's preferable to install a VM output
 * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM
 * to finish executing and extracting the output.
 */
PH7_PRIVATE sxi32 PH7_VmBlobConsumer(
	const void *pOut,   /* VM Generated output*/
	unsigned int nLen,  /* Generated output length */
	void *pUserData     /* User private data */
	)
{
	 sxi32 rc;
	 /* Store the output in an internal BLOB */
	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);
	 return rc;
}
#define VM_STACK_GUARD 16
/*
 * Allocate a new operand stack so that we can start executing
 * our compiled PHP program.
 * Return a pointer to the operand stack (array of ph7_values)
 * on success. NULL (Fatal error) on failure.
 */
static ph7_value * VmNewOperandStack(
	ph7_vm *pVm, /* Target VM */
	sxu32 nInstr /* Total numer of generated byte-code instructions */
	)
{
	ph7_value *pStack;
  /* No instruction ever pushes more than a single element onto the
  ** stack and the stack never grows on successive executions of the
  ** same loop. So the total number of instructions is an upper bound
  ** on the maximum stack depth required.
  **
  ** Allocation all the stack space we will ever need.
  */
	nInstr += VM_STACK_GUARD;
	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));
	if( pStack == 0 ){
		return 0;
	}
	/* Initialize the operand stack */
	while( nInstr > 0 ){
		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);
		--nInstr;
	}
	/* Ready for bytecode execution */
	return pStack;
}
/* Forward declaration */
static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);
static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);
static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);
/*
 * Prepare the Virtual Machine for byte-code execution.
 * This routine gets called by the PH7 engine after
 * successful compilation of the target PHP program.
 */
PH7_PRIVATE sxi32 PH7_VmMakeReady(
	ph7_vm *pVm /* Target VM */
	)
{
	SyHashEntry *pEntry;
	sxi32 rc;
	if( pVm->nMagic != PH7_VM_INIT ){
		/* Initialize your VM first */
		return SXERR_CORRUPT;
	}
	/* Mark the VM ready for byte-code execution */
	pVm->nMagic = PH7_VM_RUN; 
	/* Release the code generator now we have compiled our program */
	PH7_ResetCodeGenerator(pVm,0,0);
	/* Emit the DONE instruction */
	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);
	if( rc != SXRET_OK ){
		return SXERR_MEM;
	}
	/* Script return value */
	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */
	/* Allocate a new operand stack */	
	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));
	if( pVm->aOps == 0 ){
		return SXERR_MEM;
	}
	/* Set the default VM output consumer callback and it's 
	 * private data. */
	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;
	pVm->sVmConsumer.pUserData = &pVm->sConsumer;
	/* Allocate the reference table */
	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */
	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);
	if( pVm->apRefObj == 0 ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_MEM;
	}
	/* Zero the reference table */
	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);
	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */
	rc = VmRegisterSpecialFunction(&(*pVm));
	if( rc != SXRET_OK ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return rc;
	}
	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */
	rc = PH7_HashmapCreateSuper(&(*pVm));
	if( rc != SXRET_OK ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return rc;
	}
	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */
	PH7_RegisterBuiltInConstant(&(*pVm));
	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */
	PH7_RegisterBuiltInFunction(&(*pVm));
	/* Initialize and install static and constants class attributes */
	SyHashResetLoopCursor(&pVm->hClass);
	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){
		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	/* Random number betwwen 0 and 1023 used to generate unique ID */
	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;
	/* VM is ready for bytecode execution */
	return SXRET_OK;
}
/*
 * Reset a Virtual Machine to it's initial state.
 */
PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)
{
	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){
		return SXERR_CORRUPT;
	}
	/* TICKET 1433-003: As of this version, the VM is automatically reset */
	SyBlobReset(&pVm->sConsumer);
	PH7_MemObjRelease(&pVm->sExec);
	/* Set the ready flag */
	pVm->nMagic = PH7_VM_RUN;
	return SXRET_OK;
}
/*
 * Release a Virtual Machine.
 * Every virtual machine must be destroyed in order to avoid memory leaks.
 */
PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)
{
	/* Set the stale magic number */
	pVm->nMagic = PH7_VM_STALE;
	/* Release the private memory subsystem */
	SyMemBackendRelease(&pVm->sAllocator);
	return SXRET_OK;
}
/*
 * Initialize a foreign function call context.
 * The context in which a foreign function executes is stored in a ph7_context object.
 * A pointer to a ph7_context object is always first parameter to application-defined foreign
 * functions.
 * The application-defined foreign function implementation will pass this pointer through into
 * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),
 * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()
 * and many more. Refer to the C/C++ Interfaces documentation for additional information.
 */
static sxi32 VmInitCallContext(
	ph7_context *pOut,    /* Call Context */
	ph7_vm *pVm,          /* Target VM */
	ph7_user_func *pFunc, /* Foreign function to execute shortly */
	ph7_value *pRet,      /* Store return value here*/
	sxi32 iFlags          /* Control flags */
	)
{
	pOut->pFunc = pFunc;
	pOut->pVm   = pVm;
	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));
	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));
	/* Assume a null return value */
	MemObjSetType(pRet,MEMOBJ_NULL);
	pOut->pRet = pRet;
	pOut->iFlags = iFlags;
	return SXRET_OK;
}
/*
 * Release a foreign function call context and cleanup the mess
 * left behind.
 */
static void VmReleaseCallContext(ph7_context *pCtx)
{
	sxu32 n;
	if( SySetUsed(&pCtx->sVar) > 0 ){
		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);
		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){
			if( apObj[n] == 0 ){
				/* Already released */
				continue;
			}
			PH7_MemObjRelease(apObj[n]);
			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);
		}
		SySetRelease(&pCtx->sVar);
	}
	if( SySetUsed(&pCtx->sChunk) > 0 ){
		ph7_aux_data *aAux;
		void *pChunk;
		/* Automatic release of dynamically allocated chunk 
		 * using [ph7_context_alloc_chunk()].
		 */
		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);
		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){
			pChunk = aAux[n].pAuxData;
			/* Release the chunk */
			if( pChunk ){
				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);
			}
		}
		SySetRelease(&pCtx->sChunk);
	}
}
/*
 * Release a ph7_value allocated from the body of a foreign function.
 * Refer to [ph7_context_release_value()] for additional information.
 */
PH7_PRIVATE void PH7_VmReleaseContextValue(
	ph7_context *pCtx, /* Call context */
	ph7_value *pValue  /* Release this value */
	)
{
	if( pValue == 0 ){
		/* NULL value is a harmless operation */
		return;
	}
	if( SySetUsed(&pCtx->sVar) > 0 ){
		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){
			if( apObj[n] == pValue ){
				PH7_MemObjRelease(pValue);
				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);
				/* Mark as released */
				apObj[n] = 0;
				break;
			}
		}
	}
}
/*
 * Pop and release as many memory object from the operand stack.
 */
static void VmPopOperand(
	ph7_value **ppTos, /* Operand stack */
	sxi32 nPop         /* Total number of memory objects to pop */
	)
{
	ph7_value *pTos = *ppTos;
	while( nPop > 0 ){
		PH7_MemObjRelease(pTos);
		pTos--;
		nPop--;
	}
	/* Top of the stack */
	*ppTos = pTos;
}
/*
 * Reserve a memory object.
 * Return a pointer to the raw ph7_value on success. NULL on failure.
 */
PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)
{
	ph7_value *pObj = 0;
	VmSlot *pSlot;
	sxu32 nIdx;
	/* Check for a free slot */
	nIdx = SXU32_HIGH; /* cc warning */
	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);
	if( pSlot ){
		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);
		nIdx = pSlot->nIdx;
	}
	if( pObj == 0 ){
		/* Reserve a new memory object */
		pObj = VmReserveMemObj(&(*pVm),&nIdx);
		if( pObj == 0 ){
			return 0;
		}
	}
	/* Set a null default value */
	PH7_MemObjInit(&(*pVm),pObj);
	pObj->nIdx = nIdx;
	return pObj;
}
/*
 * Insert an entry by reference (not copy) in the given hashmap.
 */
static sxi32 VmHashmapRefInsert(
	ph7_hashmap *pMap, /* Target hashmap */
	const char *zKey,  /* Entry key */
	sxu32 nByte,       /* Key length */
	sxu32 nRefIdx      /* Entry index in the object pool */
	)
{
	ph7_value sKey;
	sxi32 rc;
	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,nByte);
	/* Perform the insertion */
	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);
	PH7_MemObjRelease(&sKey);
	return rc;
}
/*
 * Extract a variable value from the top active VM frame.
 * Return a pointer to the variable value on success. 
 * NULL otherwise (non-existent variable/Out-of-memory,...).
 */
static ph7_value * VmExtractMemObj(
	ph7_vm *pVm,           /* Target VM */
	const SyString *pName, /* Variable name */
	int bDup,              /* True to duplicate variable name */
	int bCreate            /* True to create the variable if non-existent */
	)
{
	int bNullify = FALSE;
	SyHashEntry *pEntry;
	VmFrame *pFrame;
	ph7_value *pObj;
	sxu32 nIdx;
	sxi32 rc;
	/* Point to the top active frame */
	pFrame = pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent; /* Parent frame */
	}
	/* Perform the lookup */
	if( pName == 0 || pName->nByte < 1 ){
		static const SyString sAnnon = { " " , sizeof(char) };
		pName = &sAnnon;
		/* Always nullify the object */
		bNullify = TRUE;
		bDup = FALSE;
	}
	/* Check the superglobals table first */
	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);
	if( pEntry == 0 ){
		/* Query the top active frame */
		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);
		if( pEntry == 0 ){
			char *zName = (char *)pName->zString;
			VmSlot sLocal;
			if( !bCreate ){
				/* Do not create the variable,return NULL instead */
				return 0;
			}
			/* No such variable,automatically create a new one and install
			 * it in the current frame.
			 */
			pObj = PH7_ReserveMemObj(&(*pVm));
			if( pObj == 0 ){
				return 0;
			}
			nIdx = pObj->nIdx;
			if( bDup ){
				/* Duplicate name */
				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);
				if( zName == 0 ){
					return 0;
				}
			}
			/* Link to the top active VM frame */
			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));
			if( rc != SXRET_OK ){
				/* Return the slot to the free pool */
				sLocal.nIdx = nIdx;
				sLocal.pUserData = 0;
				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);
				return 0;
			}
			if( pFrame->pParent != 0 ){
				/* Local variable */
				sLocal.nIdx = nIdx;
				SySetPut(&pFrame->sLocal,(const void *)&sLocal);
			}else{
				/* Register in the $GLOBALS array */
				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);
			}
			/* Install in the reference table */
			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);
			/* Save object index */
			pObj->nIdx = nIdx;
		}else{
			/* Extract variable contents */
			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);
			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( bNullify && pObj ){
				PH7_MemObjRelease(pObj);
			}
		}
	}else{
		/* Superglobal */
		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);
		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	}
	return pObj;
}
/*
 * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,.... 
 * Return a pointer to the variable value on success.NULL otherwise.
 */
static ph7_value * VmExtractSuper(
	ph7_vm *pVm,       /* Target VM */
	const char *zName, /* Superglobal name: NOT NULL TERMINATED */
	sxu32 nByte        /* zName length */
	)
{
	SyHashEntry *pEntry;
	ph7_value *pValue;
	sxu32 nIdx;
	/* Query the superglobal table */
	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);
	if( pEntry == 0 ){
		/* No such entry */
		return 0;
	}
	/* Extract the superglobal index in the global object pool */
	nIdx = SX_PTR_TO_INT(pEntry->pUserData);
	/* Extract the variable value  */
	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	return pValue;
}
/*
 * Perform a raw hashmap insertion.
 * Refer to the [PH7_VmConfigure()] implementation for additional information.
 */
static sxi32 VmHashmapInsert(
	ph7_hashmap *pMap,  /* Target hashmap  */
	const char *zKey,   /* Entry key */
	int nKeylen,        /* zKey length*/
	const char *zData,  /* Entry data */
	int nLen            /* zData length */
	)
{
	ph7_value sKey,sValue;
	sxi32 rc;
	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);
	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);
	if( zKey ){
		if( nKeylen < 0 ){
			nKeylen = (int)SyStrlen(zKey);
		}
		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);
	}
	if( zData ){
		if( nLen < 0 ){
			/* Compute length automatically */
			nLen = (int)SyStrlen(zData);
		}
		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);
	}
	/* Perform the insertion */
	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sValue);
	return rc;
}
/* Forward declaration */
static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);
/*
 * Configure a working virtual machine instance.
 *
 * This routine is used to configure a PH7 virtual machine obtained by a prior
 * successful call to one of the compile interface such as ph7_compile()
 * ph7_compile_v2() or ph7_compile_file().
 * The second argument to this function is an integer configuration option
 * that determines what property of the PH7 virtual machine is to be configured.
 * Subsequent arguments vary depending on the configuration option in the second
 * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,
 * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.
 * Refer to the official documentation for the list of allowed verbs.
 */
PH7_PRIVATE sxi32 PH7_VmConfigure(
	ph7_vm *pVm, /* Target VM */
	sxi32 nOp,   /* Configuration verb */
	va_list ap   /* Subsequent option arguments */
	)
{
	sxi32 rc = SXRET_OK;
	switch(nOp){
	case PH7_VM_CONFIG_OUTPUT: {
		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);
		void *pUserData = va_arg(ap,void *);
		/* VM output consumer callback */
#ifdef UNTRUST
		if( xConsumer == 0 ){
			rc = SXERR_CORRUPT;
			break;
		}
#endif
		/* Install the output consumer */
		pVm->sVmConsumer.xConsumer = xConsumer;
		pVm->sVmConsumer.pUserData = pUserData;
		break;
							   }
	case PH7_VM_CONFIG_IMPORT_PATH: {
		/* Import path */
		  const char *zPath;
		  SyString sPath;
		  zPath = va_arg(ap,const char *);
#if defined(UNTRUST)
		  if( zPath == 0 ){
			  rc = SXERR_EMPTY;
			  break;
		  }
#endif
		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));
		  /* Remove trailing slashes and backslashes */
#ifdef __WINNT__
		  SyStringTrimTrailingChar(&sPath,'\\');
#endif
		  SyStringTrimTrailingChar(&sPath,'/');
		  /* Remove leading and trailing white spaces */
		  SyStringFullTrim(&sPath);
		  if( sPath.nByte > 0 ){
			  /* Store the path in the corresponding conatiner */
			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);
		  }
		  break;
									 }
	case PH7_VM_CONFIG_ERR_REPORT:
		/* Run-Time Error report */
		pVm->bErrReport = 1;
		break;
	case PH7_VM_CONFIG_RECURSION_DEPTH:{
		/* Recursion depth */
		int nDepth = va_arg(ap,int);
		if( nDepth > 2 && nDepth < 1024 ){
			pVm->nMaxDepth = nDepth;
		}
		break;
									   }
	case PH7_VM_OUTPUT_LENGTH: {
		/* VM output length in bytes */
		sxu32 *pOut = va_arg(ap,sxu32 *);
#ifdef UNTRUST
		if( pOut == 0 ){
			rc = SXERR_CORRUPT;
			break;
		}
#endif
		*pOut = pVm->nOutputLen;
		break;
							   }

	case PH7_VM_CONFIG_CREATE_SUPER:
	case PH7_VM_CONFIG_CREATE_VAR: {
		/* Create a new superglobal/global variable */
		const char *zName = va_arg(ap,const char *);
		ph7_value *pValue = va_arg(ap,ph7_value *);
		SyHashEntry *pEntry;
		ph7_value *pObj;
		sxu32 nByte;
		sxu32 nIdx; 
#ifdef UNTRUST
		if( SX_EMPTY_STR(zName) || pValue == 0 ){
			rc = SXERR_CORRUPT;
			break;
		}
#endif
		nByte = SyStrlen(zName);
		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){
			/* Check if the superglobal is already installed */
			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);
		}else{
			/* Query the top active VM frame */
			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);
		}
		if( pEntry ){
			/* Variable already installed */
			nIdx = SX_PTR_TO_INT(pEntry->pUserData);
			/* Extract contents */
			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pObj ){
				/* Overwrite old contents */
				PH7_MemObjStore(pValue,pObj);
			}
		}else{
			/* Install a new variable */
			pObj = PH7_ReserveMemObj(&(*pVm));
			if( pObj == 0 ){
				rc = SXERR_MEM;
				break;
			}
			nIdx = pObj->nIdx;
			/* Copy value */
			PH7_MemObjStore(pValue,pObj);
			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){
				/* Install the superglobal */
				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));
			}else{
				/* Install in the current frame */
				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));
			}
			if( rc == SXRET_OK ){
				SyHashEntry *pRef;
				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){
					pRef = SyHashLastEntry(&pVm->hSuper);
				}else{
					pRef = SyHashLastEntry(&pVm->pFrame->hVar);
				}
				/* Install in the reference table */
				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);
				if( nOp == PH7_VM_CONFIG_CREATE_SUPER || pVm->pFrame->pParent == 0){
					/* Register in the $GLOBALS array */
					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);
				}
			}
		}
		break;
									}
	case PH7_VM_CONFIG_SERVER_ATTR:
	case PH7_VM_CONFIG_ENV_ATTR: 
	case PH7_VM_CONFIG_SESSION_ATTR:
	case PH7_VM_CONFIG_POST_ATTR:
	case PH7_VM_CONFIG_GET_ATTR:
	case PH7_VM_CONFIG_COOKIE_ATTR:
	case PH7_VM_CONFIG_HEADER_ATTR: {
		const char *zKey   = va_arg(ap,const char *);
		const char *zValue = va_arg(ap,const char *);
		int nLen = va_arg(ap,int);
		ph7_hashmap *pMap;
		ph7_value *pValue;
		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){
			/* Extract the $_ENV superglobal */
			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);
		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){
			/* Extract the $_POST superglobal */
			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);
		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){
			/* Extract the $_GET superglobal */
			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);
		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){
			/* Extract the $_COOKIE superglobal */
			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);
		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){
			/* Extract the $_SESSION superglobal */
			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);
		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){
			/* Extract the $_HEADER superglobale */
			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);
		}else{
			/* Extract the $_SERVER superglobal */
			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);
		}
		if( pValue == 0 || (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* No such entry */
			rc = SXERR_NOTFOUND;
			break;
		}
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pValue->x.pOther;
		/* Perform the insertion */
		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);
		break;
								   }
	case PH7_VM_CONFIG_ARGV_ENTRY:{
		/* Script arguments */
		const char *zValue = va_arg(ap,const char *);
		ph7_hashmap *pMap;
		ph7_value *pValue;
		sxu32 n;
		if( SX_EMPTY_STR(zValue) ){
			rc = SXERR_EMPTY;
			break;
		}
		/* Extract the $argv array */
		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);
		if( pValue == 0 || (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* No such entry */
			rc = SXERR_NOTFOUND;
			break;
		}
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pValue->x.pOther;
		/* Perform the insertion */
		n = (sxu32)SyStrlen(zValue);
		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);
		if( rc == SXRET_OK ){
			if( pMap->nEntry > 1 ){
				/* Append space separator first */
				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));
			}
			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);
		}
		break;
								  }
	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {
		/* error_log() consumer */
		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);
		pVm->xErrLog = xErrLog;
		break;
										}
	case PH7_VM_CONFIG_EXEC_VALUE: {
		/* Script return value */
		ph7_value **ppValue = va_arg(ap,ph7_value **);
#ifdef UNTRUST
		if( ppValue == 0 ){
			rc = SXERR_CORRUPT;
			break;
		}
#endif
		*ppValue = &pVm->sExec;
		break;
								   }
	case PH7_VM_CONFIG_IO_STREAM: {
		/* Register an IO stream device */
		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);
		/* Make sure we are dealing with a valid IO stream */
		if( pStream == 0 || pStream->zName == 0 || pStream->zName[0] == 0 ||
			pStream->xOpen == 0 || pStream->xRead == 0 ){
				/* Invalid stream */
				rc = SXERR_INVALID;
				break;
		}
		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){
			/* Make the 'file://' stream the defaut stream device */
			pVm->pDefStream = pStream;
		}
		/* Insert in the appropriate container */
		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);
		break;
								  }
	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {
		/* Point to the VM internal output consumer buffer */
		const void **ppOut = va_arg(ap,const void **);
		unsigned int *pLen = va_arg(ap,unsigned int *);
#ifdef UNTRUST
		if( ppOut == 0 || pLen == 0 ){
			rc = SXERR_CORRUPT;
			break;
		}
#endif
		*ppOut = SyBlobData(&pVm->sConsumer);
		*pLen  = SyBlobLength(&pVm->sConsumer);
		break;
									   }
	case PH7_VM_CONFIG_HTTP_REQUEST:{
		/* Raw HTTP request*/
		const char *zRequest = va_arg(ap,const char *);
		int nByte = va_arg(ap,int);
		if( SX_EMPTY_STR(zRequest) ){
			rc = SXERR_EMPTY;
			break;
		}
		if( nByte < 0 ){
			/* Compute length automatically */
			nByte = (int)SyStrlen(zRequest);
		}
		/* Process the request */
		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);
		break;
									}
	default:
		/* Unknown configuration option */
		rc = SXERR_UNKNOWN;
		break;
	}
	return rc;
}
/* Forward declaration */
static const char * VmInstrToString(sxi32 nOp);
/*
 * This routine is used to dump PH7 byte-code instructions to a human readable
 * format.
 * The dump is redirected to the given consumer callback which is responsible
 * of consuming the generated dump perhaps redirecting it to its standard output
 * (STDOUT).
 */
static sxi32 VmByteCodeDump(
	SySet *pByteCode,       /* Bytecode container */
	ProcConsumer xConsumer, /* Dump consumer callback */
	void *pUserData         /* Last argument to xConsumer() */
	)
{
	static const char zDump[] = {
		"====================================================\n"
		"PH7 VM Dump\n"
		"====================================================\n"
	};
	VmInstr *pInstr,*pEnd;
	sxi32 rc = SXRET_OK;
	sxu32 n;
	/* Point to the PH7 instructions */
	pInstr = (VmInstr *)SySetBasePtr(pByteCode);
	pEnd   = &pInstr[SySetUsed(pByteCode)];
	n = 0;
	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);
	/* Dump instructions */
	for(;;){
		if( pInstr >= pEnd ){
			/* No more instructions */
			break;
		}
		/* Format and call the consumer callback */
		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",
			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,
			SX_PTR_TO_INT(pInstr->p3),n);
		if( rc != SXRET_OK ){
			/* Consumer routine request an operation abort */
			return rc;
		}
		++n;
		pInstr++; /* Next instruction in the stream */
	}
	return rc;
}
/* Forward declaration */
static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);
static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);
static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);
/*
 * Consume a generated run-time error message by invoking the VM output
 * consumer callback.
 */
static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)
{
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	sxi32 rc = SXRET_OK;
	/* Append a new line */
#ifdef __WINNT__
	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);
#else
	SyBlobAppend(pMsg,"\n",sizeof(char));
#endif
	/* Invoke the output consumer callback */
	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);
	if( pCons->xConsumer != VmObConsumer ){
		/* Increment output length */
		pVm->nOutputLen += SyBlobLength(pMsg);
	}
	return rc;
}
/*
 * Throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error()] for additional
 * information.
 */
static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, SyString *pFile, sxi32 iLine)
{
	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){
		ph7_value apArg[4];
		ph7_value *apArgPtr[4];
		ph7_value sResult;
		SyString sErr;
		/* Prepare arguments */
		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);
		SyStringInitFromBuf(&sErr,zMessage,SyStrlen(zMessage));
		PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);
		if( pFile ){
			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);
			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);
		}else{
			PH7_MemObjInit(pVm,&apArg[2]);
		}
		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);
		PH7_MemObjInit(pVm,&sResult);
		/* Set up pointer array */
		apArgPtr[0] = &apArg[0];
		apArgPtr[1] = &apArg[1];
		apArgPtr[2] = &apArg[2];
		apArgPtr[3] = &apArg[3];
		/* Call the handler */
		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);
		/* Check return value */
		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){
			PH7_MemObjToBool(&sResult);
		}
		/* Release */
		PH7_MemObjRelease(&apArg[0]);
		PH7_MemObjRelease(&apArg[1]);
		PH7_MemObjRelease(&apArg[2]);
		PH7_MemObjRelease(&apArg[3]);
		PH7_MemObjRelease(&sResult);
		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)
		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */
		return sResult.x.iVal == 0 ? TRUE : FALSE;
	}
	/* No handler, always call error handler */
	return TRUE;
}
PH7_PRIVATE sxi32 PH7_VmThrowError(
	ph7_vm *pVm,         /* Target VM */
	SyString *pFuncName, /* Function name. NULL otherwise */
	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/
	const char *zMessage /* Null terminated error message */
	)
{
	SyBlob *pWorker = &pVm->sWorker;
	SyString *pFile;
	char *zErr;
	sxi32 rc = SXRET_OK;
	if( !pVm->bErrReport ){
		/* Don't bother reporting errors */
		return SXRET_OK;
	}
	/* Reset the working buffer */
	SyBlobReset(pWorker);
	/* Peek the processed file if available */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile ){
		/* Append file name */
		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);
		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));
	}
	zErr = "Error: ";
	switch(iErr){
	case PH7_CTX_WARNING: zErr = "Warning: "; break;
	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;
	default:
		iErr = PH7_CTX_ERR;
		break;
	}
	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));
	if( pFuncName ){
		/* Append function name first */
		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);
		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);
	}
	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));
	/* Check for user error handler */
	if( VmInvokeErrorHandler(pVm, iErr, zMessage, pFile, 0) ){
		rc = VmCallErrorHandler(&(*pVm),pWorker);
	}
	return rc;
}
/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 */
static sxi32 VmThrowErrorAp(
	ph7_vm *pVm,         /* Target VM */
	SyString *pFuncName, /* Function name. NULL otherwise */
	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */
	const char *zFormat, /* Format message */
	va_list ap           /* Variable list of arguments */
	)
{
	SyBlob *pWorker = &pVm->sWorker;
	SyBlob sMsg;
	SyString *pFile;
	char *zErr;
	sxi32 rc = SXRET_OK;
	if( !pVm->bErrReport ){
		/* Don't bother reporting errors */
		return SXRET_OK;
	}
	/* Reset the working buffer */
	SyBlobReset(pWorker);
	/* Peek the processed file if available */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile ){
		/* Append file name */
		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);
		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));
	}
	zErr = "Error: ";
	switch(iErr){
	case PH7_CTX_WARNING: zErr = "Warning: "; break;
	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;
	default:
		iErr = PH7_CTX_ERR;
		break;
	}
	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));
	if( pFuncName ){
		/* Append function name first */
		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);
		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);
	}
	/* Format the raw message */
	SyBlobInit(&sMsg, &pVm->sAllocator);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	/* Check if a user error handler is installed */
	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), pFile, 0) ){
		/* No handler or handler returned TRUE, normal processing */
		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));
		rc = VmCallErrorHandler(&(*pVm),pWorker);
	}
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 * ------------------------------------
 * Simple boring wrapper function.
 * ------------------------------------
 */
static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)
{
	va_list ap;
	sxi32 rc;
	va_start(ap,zFormat);
	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 * ------------------------------------
 * Simple boring wrapper function.
 * ------------------------------------
 */
PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)
{
	sxi32 rc;
	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);
	return rc;
}
/*
 * Execute as much of a PH7 bytecode program as we can then return.
 *
 * [PH7_VmMakeReady()] must be called before this routine in order to
 * close the program with a final OP_DONE and to set up the default
 * consumer routines and other stuff. Refer to the implementation
 * of [PH7_VmMakeReady()] for additional information.
 * If the installed VM output consumer callback ever returns PH7_ABORT
 * then the program execution is halted.
 * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]
 * should be used respectively to clean up the mess that was left behind
 * or to reset the VM to it's initial state.
 */
static sxi32 VmByteCodeExec(
	ph7_vm *pVm,         /* Target VM */
	VmInstr *aInstr,     /* PH7 bytecode program */
	ph7_value *pStack,   /* Operand stack */
	int nTos,            /* Top entry in the operand stack (usually -1) */
	ph7_value *pResult,  /* Store program return value here. NULL otherwise */
	sxu32 *pLastRef,     /* Last referenced ph7_value index */
	int is_callback      /* TRUE if we are executing a callback */
	)
{
	VmInstr *pInstr;
	ph7_value *pTos;
	SySet aArg;
	sxi32 pc;
	sxi32 rc;
	/* Argument container */
	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
	if( nTos < 0 ){
		pTos = &pStack[-1];
	}else{
		pTos = &pStack[nTos];
	}
	pc = 0;
	/* Execute as much as we can */
	for(;;){
		/* Fetch the instruction to execute */
		pInstr = &aInstr[pc];
		rc = SXRET_OK;
/*
 * What follows here is a massive switch statement where each case implements a
 * separate instruction in the virtual machine.  If we follow the usual
 * indentation convention each case should be indented by 6 spaces.  But
 * that is a lot of wasted space on the left margin.  So the code within
 * the switch statement will break with convention and be flush-left.
 */
		switch(pInstr->iOp){
/*
 * DONE: P1 * * 
 *
 * Program execution completed: Clean up the mess left behind
 * and return immediately.
 */
case PH7_OP_DONE:
	if( pInstr->iP1 ){
#ifdef UNTRUST
		if( pTos < pStack ){
			goto Abort;
		}
#endif
		if( pLastRef ){
			*pLastRef = pTos->nIdx;
		}
		if( pResult ){
			/* Execution result */
			PH7_MemObjStore(pTos,pResult);
		}		
		VmPopOperand(&pTos,1);
	}else if( pLastRef ){
		/* Nothing referenced */
		*pLastRef = SXU32_HIGH;
	}
	goto Done;
/*
 * HALT: P1 * *
 *
 * Program execution aborted: Clean up the mess left behind
 * and abort immediately.
 */
case PH7_OP_HALT:
	if( pInstr->iP1 ){
#ifdef UNTRUST
		if( pTos < pStack ){
			goto Abort;
		}
#endif
		if( pLastRef ){
			*pLastRef = pTos->nIdx;
		}
		if( pTos->iFlags & MEMOBJ_STRING ){
			if( SyBlobLength(&pTos->sBlob) > 0 ){
				/* Output the exit message */
				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),
					pVm->sVmConsumer.pUserData);
				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){
					/* Increment output length */
					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);
				}
			}
		}else if(pTos->iFlags & MEMOBJ_INT ){
			/* Record exit status */
			pVm->iExitStatus = (sxi32)pTos->x.iVal;
		}
		VmPopOperand(&pTos,1);
	}else if( pLastRef ){
		/* Nothing referenced */
		*pLastRef = SXU32_HIGH;
	}
	/* Check if we're in an included file context */
	if( SySetUsed(&pVm->aFiles) > 0 ){
		/* Terminate the entire process */
		exit(pVm->iExitStatus);
	}
	goto Abort;
/*
 * JMP: * P2 *
 *
 * Unconditional jump: The next instruction executed will be 
 * the one at index P2 from the beginning of the program.
 */
case PH7_OP_JMP:
	pc = pInstr->iP2 - 1;
	break;
/*
 * JZ: P1 P2 *
 *
 * Take the jump if the top value is zero (FALSE jump).Pop the top most
 * entry in the stack if P1 is zero. 
 */
case PH7_OP_JZ:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Get a boolean value */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if( !pTos->x.iVal ){
		/* Take the jump */
		pc = pInstr->iP2 - 1;
	}
	if( !pInstr->iP1 ){
		VmPopOperand(&pTos,1);
	}
	break;
/*
 * JNZ: P1 P2 *
 *
 * Take the jump if the top value is not zero (TRUE jump).Pop the top most
 * entry in the stack if P1 is zero.
 */
case PH7_OP_JNZ:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Get a boolean value */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if( pTos->x.iVal ){
		/* Take the jump */
		pc = pInstr->iP2 - 1;
	}
	if( !pInstr->iP1 ){
		VmPopOperand(&pTos,1);
	}
	break;
/*
 * NOOP: * * *
 *
 * Do nothing. This instruction is often useful as a jump
 * destination.
 */
case PH7_OP_NOOP:
	break;
/*
 * POP: P1 * *
 *
 * Pop P1 elements from the operand stack.
 */
case PH7_OP_POP: {
	sxi32 n = pInstr->iP1;
	if( &pTos[-n+1] < pStack ){
		/* TICKET 1433-51 Stack underflow must be handled at run-time */
		n = (sxi32)(pTos - pStack);
	}
	VmPopOperand(&pTos,n);
	break;
				 }
/*
 * CVT_INT: * * *
 *
 * Force the top of the stack to be an integer.
 */
case PH7_OP_CVT_INT:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if((pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	/* Invalidate any prior representation */
	MemObjSetType(pTos,MEMOBJ_INT);
	break;
/*
 * CVT_REAL: * * *
 *
 * Force the top of the stack to be a real.
 */
case PH7_OP_CVT_REAL:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pTos);
	}
	/* Invalidate any prior representation */
	MemObjSetType(pTos,MEMOBJ_REAL);
	break;
/*
 * CVT_STR: * * *
 *
 * Force the top of the stack to be a string.
 */
case PH7_OP_CVT_STR:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
		PH7_MemObjToString(pTos);
	}
	break;
/*
 * CVT_BOOL: * * *
 *
 * Force the top of the stack to be a boolean.
 */
case PH7_OP_CVT_BOOL:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	break;
/*
 * CVT_NULL: * * *
 *
 * Nullify the top of the stack.
 */
case PH7_OP_CVT_NULL:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	PH7_MemObjRelease(pTos);
	break;
/*
 * CVT_NUMC: * * *
 *
 * Force the top of the stack to be a numeric type (integer,real or both).
 */
case PH7_OP_CVT_NUMC:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a numeric cast */
	PH7_MemObjToNumeric(pTos);
	break;
/*
 * CVT_ARRAY: * * *
 *
 * Force the top of the stack to be a hashmap aka 'array'.
 */
case PH7_OP_CVT_ARRAY:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a hashmap cast */
	rc = PH7_MemObjToHashmap(pTos);
	if( rc != SXRET_OK ){
		/* Not so fatal,emit a simple warning */
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,
			"PH7 engine is running out of memory while performing an array cast");
	}
	break;
/*
 * CVT_OBJ: * * *
 *
 * Force the top of the stack to be a class instance (Object in the PHP jargon).
 */
case PH7_OP_CVT_OBJ:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){
		/* Force a 'stdClass()' cast */
		PH7_MemObjToObject(pTos);
	}
	break;
/*
 * ERR_CTRL * * *
 *
 * Error control operator. 
 */
case PH7_OP_ERR_CTRL:
	/* 
	 * TICKET 1433-038:
	 * As of this version ,the error control operator '@' is a no-op,simply
	 * use the public API,to control error output.
	 */
	break;
/*
 * IS_A * * *
 *
 * Pop the top two operands from the stack and check whether the first operand
 * is an object and is an instance of the second operand (which must be a string
 * holding a class name or an object).
 * Push TRUE on success. FALSE otherwise.
 */
case PH7_OP_IS_A:{
	ph7_value *pNos = &pTos[-1];
	sxi32 iRes = 0; /* assume false by default */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	if( pNos->iFlags& MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;
		ph7_class *pClass = 0;
		/* Extract the target class */
		if( pTos->iFlags & MEMOBJ_OBJ ){
			/* Instance already loaded */
			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;
		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){
			/* Perform the query */
			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),
				SyBlobLength(&pTos->sBlob),FALSE,0);
		}
		if( pClass ){
			/* Perform the query */
			iRes = VmInstanceOf(pThis->pClass,pClass);
		}
	}
	/* Push result */
	VmPopOperand(&pTos,1);
	PH7_MemObjRelease(pTos);
	pTos->x.iVal = iRes;
	MemObjSetType(pTos,MEMOBJ_BOOL);
	break;
				 }

/*
 * LOADC P1 P2 *
 *
 * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.
 * If P1 is set,then this constant is candidate for expansion via user installable callbacks.
 */
case PH7_OP_LOADC: {
	ph7_value *pObj;
	/* Reserve a room */
	pTos++;
	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){
		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){
			SyHashEntry *pEntry;
			/* Candidate for expansion via user defined callbacks */
			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));
			if( pEntry ){
				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;
				/* Set a NULL default value */
				MemObjSetType(pTos,MEMOBJ_NULL);
				SyBlobReset(&pTos->sBlob);
				/* Invoke the callback and deal with the expanded value */
				pCons->xExpand(pTos,pCons->pUserData);
				/* Mark as constant */
				pTos->nIdx = SXU32_HIGH;
				break;
			}
		}
		PH7_MemObjLoad(pObj,pTos);
	}else{
		/* Set a NULL value */
		MemObjSetType(pTos,MEMOBJ_NULL);
	}
	/* Mark as constant */
	pTos->nIdx = SXU32_HIGH;
	break;
				  }
/*
 * LOAD: P1 * P3
 *
 * Load a variable where it's name is taken from the top of the stack or
 * from the P3 operand.
 * If P1 is set,then perform a lookup only.In other words do not create
 * the variable if non existent and push the NULL constant instead.
 */
case PH7_OP_LOAD:{
	ph7_value *pObj;
	SyString sName;
	if( pInstr->p3 == 0 ){
		/* Take the variable name from the top of the stack */
#ifdef UNTRUST
		if( pTos < pStack ){
			goto Abort;
		}
#endif
		/* Force a string cast */
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(pTos);
		}
		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	}else{
		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
		/* Reserve a room for the target object */
		pTos++;
	}
	/* Extract the requested memory object */
	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);
	if( pObj == 0 ){
		if( pInstr->iP1 ){
			/* Variable not found,load NULL */
			if( !pInstr->p3 ){
				PH7_MemObjRelease(pTos);
			}else{
				MemObjSetType(pTos,MEMOBJ_NULL);
			}
			pTos->nIdx = SXU32_HIGH; /* Mark as constant */
			break;
		}else{
			/* Fatal error */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);
			goto Abort;
		}
	}
	/* Load variable contents */
	PH7_MemObjLoad(pObj,pTos);
	pTos->nIdx = pObj->nIdx;
	break;
				   }
/*
 * LOAD_MAP P1 * *
 *
 * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.
 * If the P1 operand is greater than zero then pop P1 elements from the
 * stack and insert them (key => value pair) in the new hashmap.
 */
case PH7_OP_LOAD_MAP: {
	ph7_hashmap *pMap;
	/* Allocate a new hashmap instance */
	pMap = PH7_NewHashmap(&(*pVm),0,0);
	if( pMap == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);
		goto Abort;
	}
	if( pInstr->iP1 > 0 ){
		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */
		/* Perform the insertion */
		while( pEntry < pTos ){
			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){
				/* Insertion by reference */
				PH7_HashmapInsertByRef(pMap,
					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,
					(sxu32)pEntry[1].x.iVal
					);
			}else{
				/* Standard insertion */
				PH7_HashmapInsert(pMap,
					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,
					&pEntry[1]
				);
			}
			/* Next pair on the stack */
			pEntry += 2;
		}
		/* Pop P1 elements */
		VmPopOperand(&pTos,pInstr->iP1);
	}
	/* Push the hashmap */
	pTos++;
	pTos->nIdx = SXU32_HIGH;
	pTos->x.pOther = pMap;
	MemObjSetType(pTos,MEMOBJ_HASHMAP);
	break;
					  }
/*
 * LOAD_LIST: P1 * *
 *  
 * Assign hashmap entries values to the top P1 entries.
 * This is the VM implementation of the list() PHP construct.
 * Caveats:
 *  This implementation support only a single nesting level.
 */
case PH7_OP_LOAD_LIST: {
	ph7_value *pEntry;
	if( pInstr->iP1 <= 0 ){
		/* Empty list,break immediately */
		break;
	}
	pEntry = &pTos[-pInstr->iP1+1];
#ifdef UNTRUST
	if( &pEntry[-1] < pStack ){
		goto Abort;
	}
#endif
	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;
		ph7_hashmap_node *pNode;
		ph7_value sKey,*pObj;
		/* Start Copying */
		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);
		while( pEntry <= pTos ){
			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){
				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);
				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){
					if( rc == SXRET_OK ){
						/* Store node value */
						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);
					}else{
						/* Nullify the variable */
						PH7_MemObjRelease(pObj);
					}
				}
			}
			sKey.x.iVal++; /* Next numeric index */
			pEntry++;
		}
	}
	VmPopOperand(&pTos,pInstr->iP1);
	break;
					   }
/*
 * LOAD_IDX: P1 P2 *
 *
 * Load a hasmap entry where it's index (either numeric or string) is taken
 * from the stack.
 * If the index does not refer to a valid element,then push the NULL constant
 * instead.
 */
case PH7_OP_LOAD_IDX: {
	ph7_hashmap_node *pNode = 0; /* cc warning */
	ph7_hashmap *pMap = 0;
	ph7_value *pIdx;
	pIdx = 0;
	if( pInstr->iP1 == 0 ){
		if( !pInstr->iP2){
			/* No available index,load NULL */
			if( pTos >= pStack ){
				PH7_MemObjRelease(pTos);
			}else{
				/* TICKET 1433-020: Empty stack */
				pTos++;
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
			}
			/* Emit a notice */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,
				"Array: Attempt to access an undefined index,PH7 is loading NULL");
			break;
		}
	}else{
		pIdx = pTos;
		pTos--;
	}
	if( pTos->iFlags & MEMOBJ_STRING ){
		/* String access */
		if( pIdx ){
			sxu32 nOfft;
			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){
				/* Force an int cast */
				PH7_MemObjToInteger(pIdx);
			}
			nOfft = (sxu32)pIdx->x.iVal;
			if( nOfft >= SyBlobLength(&pTos->sBlob) ){
				/* Invalid offset,load null */
				PH7_MemObjRelease(pTos);
			}else{
				const char *zData = (const char *)SyBlobData(&pTos->sBlob);
				int c = zData[nOfft];
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_STRING);
				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));
			}
		}else{
			/* No available index,load NULL */
			MemObjSetType(pTos,MEMOBJ_NULL);
		}
		break;
	}
	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				PH7_MemObjToHashmap(pObj);
				PH7_MemObjLoad(pObj,pTos);
			}
		}
	}
	rc = SXERR_NOTFOUND; /* Assume the index is invalid */
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pTos->x.pOther;
		if( pIdx ){
			/* Load the desired entry */
			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);
		}
		if( rc != SXRET_OK && pInstr->iP2 ){
			/* Create a new empty entry */
			rc = PH7_HashmapInsert(pMap,pIdx,0);
			if( rc == SXRET_OK ){
				/* Point to the last inserted entry */
				pNode = pMap->pLast;
			}
		}
	}
	if( pIdx ){
		PH7_MemObjRelease(pIdx);
	}
	if( rc == SXRET_OK ){
		/* Load entry contents */
		if( pMap->iRef < 2 ){
			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy
			 * of the entry value,rather than pointing to it.
			 */
			pTos->nIdx = SXU32_HIGH;
			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);
		}else{
			pTos->nIdx = pNode->nValIdx;
			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);
			PH7_HashmapUnref(pMap);
		}
	}else{
		/* No such entry,load NULL */
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
	}
	break;
					  }
/*
 * LOAD_CLOSURE * * P3
 *
 * Set-up closure environment described by the P3 oeprand and push the closure
 * name in the stack.
 */
case PH7_OP_LOAD_CLOSURE:{
	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;
	if( pFunc->iFlags & VM_FUNC_CLOSURE ){
		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;
		ph7_vm_func *pClosure;
		char *zName;
		sxu32 mLen;
		sxu32 n;
		/* Create a new VM function */
		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));
		/* Generate an unique closure name */
		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);
		if( pClosure == 0 || zName == 0){
			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");
			goto Abort;
		}
		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);
		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){
			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);
		}
		/* Zero the stucture */
		SyZero(pClosure,sizeof(ph7_vm_func));
		/* Perform a structure assignment on read-only items */
		pClosure->aArgs = pFunc->aArgs;
		pClosure->aByteCode = pFunc->aByteCode;
		pClosure->aStatic = pFunc->aStatic;
		pClosure->iFlags = pFunc->iFlags;
		pClosure->pUserData = pFunc->pUserData;
		pClosure->sSignature = pFunc->sSignature;
		SyStringInitFromBuf(&pClosure->sName,zName,mLen);
		/* Register the closure */
		PH7_VmInstallUserFunction(pVm,pClosure,0);
		/* Set up closure environment */
		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));
		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){
			ph7_value *pValue;
			pEnv = &aEnv[n];
			sEnv.sName  = pEnv->sName;
			sEnv.iFlags = pEnv->iFlags;
			sEnv.nIdx = SXU32_HIGH;
			PH7_MemObjInit(pVm,&sEnv.sValue);
			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){
				/* Pass by reference */
				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"
					);
			}
			/* Standard pass by value */
			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);
			if( pValue ){
				/* Copy imported value */
				PH7_MemObjStore(pValue,&sEnv.sValue);
			}
			/* Insert the imported variable */
			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);
		}
		/* Finally,load the closure name on the stack */
		pTos++;
		PH7_MemObjStringAppend(pTos,zName,mLen);
	}
	break;
						 }
/*
 * STORE * P2 P3
 *
 * Perform a store (Assignment) operation.
 */
case PH7_OP_STORE: {
	ph7_value *pObj;
	SyString sName;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( pInstr->iP2 ){
		sxu32 nIdx;
		/* Member store operation */
		nIdx = pTos->nIdx;
		VmPopOperand(&pTos,1);
		if( nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");
			pTos->nIdx = SXU32_HIGH;
		}else{
			/* Point to the desired memory object */
			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pObj ){
				/* Perform the store operation */
				PH7_MemObjStore(pTos,pObj);
			}
		}
		break;
	}else if( pInstr->p3 == 0 ){
		/* Take the variable name from the next on the stack */
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
		pTos--;
#ifdef UNTRUST
		if( pTos < pStack  ){
			goto Abort;
		}
#endif
	}else{
		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
	}
	/* Extract the desired variable and if not available dynamically create it */
	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);
	if( pObj == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);
		goto Abort;
	}
	if( !pInstr->p3 ){
		PH7_MemObjRelease(&pTos[1]);
	}
	/* Perform the store operation */
	PH7_MemObjStore(pTos,pObj);
	break;
				   }
/*
 * STORE_IDX:   P1 * P3
 * STORE_IDX_R: P1 * P3
 *
 * Perfrom a store operation an a hashmap entry.
 */
case PH7_OP_STORE_IDX:
case PH7_OP_STORE_IDX_REF: {
	ph7_hashmap *pMap = 0; /* cc  warning */
	ph7_value *pKey;
	sxu32 nIdx;
	if( pInstr->iP1 ){
		/* Key is next on stack */
		pKey = pTos;
		pTos--;
	}else{
		pKey = 0;
	}
	nIdx = pTos->nIdx;
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		/* Hashmap already loaded */
		pMap = (ph7_hashmap *)pTos->x.pOther;
		if( pMap->iRef < 2 ){
			/* TICKET 1433-48: Prevent garbage collection */
			pMap->iRef = 2;
		}
	}else{
		ph7_value *pObj;
		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
		if( pObj == 0 ){
			if( pKey ){
			  PH7_MemObjRelease(pKey);
			}
			VmPopOperand(&pTos,1);
			break;
		}
		/* Phase#1: Load the array */
		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){
			VmPopOperand(&pTos,1);
			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){
				/* Force a string cast */
				PH7_MemObjToString(pTos);
			}
			if( pKey == 0 ){
				/* Append string */
				if( SyBlobLength(&pTos->sBlob) > 0 ){
					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
				}
			}else{
				sxu32 nOfft;
				if((pKey->iFlags & MEMOBJ_INT)){
					/* Force an int cast */
					PH7_MemObjToInteger(pKey);
				}
				nOfft = (sxu32)pKey->x.iVal;
				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){
					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);
					char *zData = (char *)SyBlobData(&pObj->sBlob);
					zData[nOfft] = zBlob[0];
				}else{
					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){
						/* Perform an append operation */
						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));
					}
				}
			}
			if( pKey ){
			  PH7_MemObjRelease(pKey);
			}
			break;
		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* Force a hashmap cast  */
			rc = PH7_MemObjToHashmap(pObj);
			if( rc != SXRET_OK ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");
				goto Abort;
			}
		}
		pMap = (ph7_hashmap *)pObj->x.pOther;
	}
	VmPopOperand(&pTos,1);
	/* Phase#2: Perform the insertion */
	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){
		/* Insertion by reference */
		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);
	}else{
		PH7_HashmapInsert(pMap,pKey,pTos);
	}
	if( pKey ){
		PH7_MemObjRelease(pKey);
	}
	break;
					   }
/*
 * INCR: P1 * *
 *
 * Force a numeric cast and increment the top of the stack by 1.
 * If the P1 operand is set then perform a duplication of the top of
 * the stack and increment after that.
 */
case PH7_OP_INCR:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				/* Force a numeric cast */
				PH7_MemObjToNumeric(pObj);
				if( pObj->iFlags & MEMOBJ_REAL ){
					pObj->rVal++;
					/* Try to get an integer representation */
					PH7_MemObjTryInteger(pTos);
				}else{
					pObj->x.iVal++;
					MemObjSetType(pTos,MEMOBJ_INT);
				}
				if( pInstr->iP1 ){
					/* Pre-icrement */
					PH7_MemObjStore(pObj,pTos);
				}
			}
		}else{
			if( pInstr->iP1 ){
				/* Force a numeric cast */
				PH7_MemObjToNumeric(pTos);
				/* Pre-increment */
				if( pTos->iFlags & MEMOBJ_REAL ){
					pTos->rVal++;
					/* Try to get an integer representation */
					PH7_MemObjTryInteger(pTos);
				}else{
					pTos->x.iVal++;
					MemObjSetType(pTos,MEMOBJ_INT);
				}
			}
		}
	}
	break;
/*
 * DECR: P1 * *
 *
 * Force a numeric cast and decrement the top of the stack by 1.
 * If the P1 operand is set then perform a duplication of the top of the stack 
 * and decrement after that.
 */
case PH7_OP_DECR:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0 ){
		/* Force a numeric cast */
		PH7_MemObjToNumeric(pTos);
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				/* Force a numeric cast */
				PH7_MemObjToNumeric(pObj);
				if( pObj->iFlags & MEMOBJ_REAL ){
					pObj->rVal--;
					/* Try to get an integer representation */
					PH7_MemObjTryInteger(pTos);
				}else{
					pObj->x.iVal--;
					MemObjSetType(pTos,MEMOBJ_INT);
				}
				if( pInstr->iP1 ){
					/* Pre-icrement */
					PH7_MemObjStore(pObj,pTos);
				}
			}
		}else{
			if( pInstr->iP1 ){
				/* Pre-increment */
				if( pTos->iFlags & MEMOBJ_REAL ){
					pTos->rVal--;
					/* Try to get an integer representation */
					PH7_MemObjTryInteger(pTos);
				}else{
					pTos->x.iVal--;
					MemObjSetType(pTos,MEMOBJ_INT);
				}
			}
		}
	}
	break;
/*
 * UMINUS: * * *
 *
 * Perform a unary minus operation.
 */
case PH7_OP_UMINUS:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a numeric (integer,real or both) cast */
	PH7_MemObjToNumeric(pTos);
	if( pTos->iFlags & MEMOBJ_REAL ){
		pTos->rVal = -pTos->rVal;
	}
	if( pTos->iFlags & MEMOBJ_INT ){
		pTos->x.iVal = -pTos->x.iVal;
	}
	break;				   
/*
 * UPLUS: * * *
 *
 * Perform a unary plus operation.
 */
case PH7_OP_UPLUS:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a numeric (integer,real or both) cast */
	PH7_MemObjToNumeric(pTos);
	if( pTos->iFlags & MEMOBJ_REAL ){
		pTos->rVal = +pTos->rVal;
	}
	if( pTos->iFlags & MEMOBJ_INT ){
		pTos->x.iVal = +pTos->x.iVal;
	}
	break;
/*
 * OP_LNOT: * * *
 *
 * Interpret the top of the stack as a boolean value.  Replace it
 * with its complement.
 */
case PH7_OP_LNOT:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a boolean cast */
	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	pTos->x.iVal = !pTos->x.iVal;
	break;
/*
 * OP_BITNOT: * * *
 *
 * Interpret the top of the stack as an value.Replace it
 * with its ones-complement.
 */
case PH7_OP_BITNOT:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force an integer cast */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	pTos->x.iVal = ~pTos->x.iVal;
	break;
/* OP_MUL * * *
 * OP_MUL_STORE * * *
 *
 * Pop the top two elements from the stack, multiply them together,
 * and push the result back onto the stack.
 */
case PH7_OP_MUL:
case PH7_OP_MUL_STORE: {
	ph7_value *pNos = &pTos[-1];
	/* Force the operand to be numeric */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
	/* Perform the requested operation */
	if( MEMOBJ_REAL & (pTos->iFlags|pNos->iFlags) ){
		/* Floating point arithemic */
		ph7_real a,b,r;
		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pTos);
		}
		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pNos);
		}
		a = pNos->rVal;
		b = pTos->rVal;
		r = a * b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}else{
		/* Integer arithmetic */
		sxi64 a,b,r;
		a = pNos->x.iVal;
		b = pTos->x.iVal;
		r = a * b;
		/* Push the result */
		pNos->x.iVal = r;
		MemObjSetType(pNos,MEMOBJ_INT);
	}
	if( pInstr->iOp == PH7_OP_MUL_STORE ){
		ph7_value *pObj;
		if( pTos->nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
			PH7_MemObjStore(pNos,pObj);
		}
	}
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_ADD * * *
 *
 * Pop the top two elements from the stack, add them together,
 * and push the result back onto the stack.
 */
case PH7_OP_ADD:{
	ph7_value *pNos = &pTos[-1];
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Perform the addition */
	PH7_MemObjAdd(pNos,pTos,FALSE);
	VmPopOperand(&pTos,1);
	break;
				}
/*
 * OP_ADD_STORE * * *
 *
 * Pop the top two elements from the stack, add them together,
 * and push the result back onto the stack.
 */
case PH7_OP_ADD_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Perform the addition */
	nIdx = pTos->nIdx;
	PH7_MemObjAdd(pTos,pNos,TRUE);
	/* Peform the store operation */
	if( nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){
		PH7_MemObjStore(pTos,pObj);
	}
	/* Ticket 1433-35: Perform a stack dup */
	PH7_MemObjStore(pTos,pNos);
	VmPopOperand(&pTos,1);
	break;
				}
/* OP_SUB * * *
 *
 * Pop the top two elements from the stack, subtract the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result back onto the stack.
 */
case PH7_OP_SUB: {
	ph7_value *pNos = &pTos[-1];
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	if( MEMOBJ_REAL & (pTos->iFlags|pNos->iFlags) ){
		/* Floating point arithemic */
		ph7_real a,b,r;
		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pTos);
		}
		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pNos);
		}
		a = pNos->rVal;
		b = pTos->rVal;
		r = a - b; 
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}else{
		/* Integer arithmetic */
		sxi64 a,b,r;
		a = pNos->x.iVal;
		b = pTos->x.iVal;
		r = a - b;
		/* Push the result */
		pNos->x.iVal = r;
		MemObjSetType(pNos,MEMOBJ_INT);
	}
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_SUB_STORE * * *
 *
 * Pop the top two elements from the stack, subtract the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result back onto the stack.
 */
case PH7_OP_SUB_STORE: {
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	if( MEMOBJ_REAL & (pTos->iFlags|pNos->iFlags) ){
		/* Floating point arithemic */
		ph7_real a,b,r;
		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pTos);
		}
		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pNos);
		}
		a = pTos->rVal;
		b = pNos->rVal;
		r = a - b; 
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}else{
		/* Integer arithmetic */
		sxi64 a,b,r;
		a = pTos->x.iVal;
		b = pNos->x.iVal;
		r = a - b;
		/* Push the result */
		pNos->x.iVal = r;
		MemObjSetType(pNos,MEMOBJ_INT);
	}
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_MemObjStore(pNos,pObj);
	}
	VmPopOperand(&pTos,1);
	break;
				 }

/*
 * OP_MOD * * *
 *
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the remainder after division 
 * onto the stack.
 * Note: Only integer arithemtic is allowed.
 */
case PH7_OP_MOD:{
	ph7_value *pNos = &pTos[-1];
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pNos->x.iVal;
	b = pTos->x.iVal;
	if( b == 0 ){
		r = 0;
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);
		/* goto Abort; */
	}else{
		r = a%b;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	VmPopOperand(&pTos,1);
	break;
				}
/*
 * OP_MOD_STORE * * *
 *
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the remainder after division 
 * onto the stack.
 * Note: Only integer arithemtic is allowed.
 */
case PH7_OP_MOD_STORE: {
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pTos->x.iVal;
	b = pNos->x.iVal;
	if( b == 0 ){
		r = 0;
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);
		/* goto Abort; */
	}else{
		r = a%b;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_MemObjStore(pNos,pObj);
	}
	VmPopOperand(&pTos,1);
	break;
				}
/*
 * OP_DIV * * *
 * 
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result onto the stack.
 * Note: Only floating point arithemtic is allowed.
 */
case PH7_OP_DIV:{
	ph7_value *pNos = &pTos[-1];
	ph7_real a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be real */
	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pNos);
	}
	/* Perform the requested operation */
	a = pNos->rVal;
	b = pTos->rVal;
	if( b == 0 ){
		/* Division by zero */
		r = 0;
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");
		/* goto Abort; */
	}else{
		r = a/b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}
	VmPopOperand(&pTos,1);
	break;
				}
/*
 * OP_DIV_STORE * * *
 * 
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result onto the stack.
 * Note: Only floating point arithemtic is allowed.
 */
case PH7_OP_DIV_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	ph7_real a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be real */
	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pNos);
	}
	/* Perform the requested operation */
	a = pTos->rVal;
	b = pNos->rVal;
	if( b == 0 ){
		/* Division by zero */
		r = 0;
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);
		/* goto Abort; */
	}else{
		r = a/b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_MemObjStore(pNos,pObj);
	}
	VmPopOperand(&pTos,1);
	break;
				}
/* OP_BAND * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise AND of the
 * two elements.
*/
/* OP_BOR * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise OR of the
 * two elements.
 */
/* OP_BXOR * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise XOR of the
 * two elements.
 */
case PH7_OP_BAND:
case PH7_OP_BOR:
case PH7_OP_BXOR:{
	ph7_value *pNos = &pTos[-1];
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pNos->x.iVal;
	b = pTos->x.iVal;
	switch(pInstr->iOp){
	case PH7_OP_BOR_STORE:
	case PH7_OP_BOR:  r = a|b; break;
	case PH7_OP_BXOR_STORE:
	case PH7_OP_BXOR: r = a^b; break;
	case PH7_OP_BAND_STORE:
	case PH7_OP_BAND:
	default:          r = a&b; break;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_BAND_STORE * * * 
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise AND of the
 * two elements.
*/
/* OP_BOR_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise OR of the
 * two elements.
 */
/* OP_BXOR_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise XOR of the
 * two elements.
 */
case PH7_OP_BAND_STORE:
case PH7_OP_BOR_STORE:
case PH7_OP_BXOR_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pTos->x.iVal;
	b = pNos->x.iVal;
	switch(pInstr->iOp){
	case PH7_OP_BOR_STORE:
	case PH7_OP_BOR:  r = a|b; break;
	case PH7_OP_BXOR_STORE:
	case PH7_OP_BXOR: r = a^b; break;
	case PH7_OP_BAND_STORE:
	case PH7_OP_BAND:
	default:          r = a&b; break;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_MemObjStore(pNos,pObj);
	}
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_SHL * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * left by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
/* OP_SHR * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * right by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
case PH7_OP_SHL:
case PH7_OP_SHR: {
	ph7_value *pNos = &pTos[-1];
	sxi64 a,r;
	sxi32 b;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pNos->x.iVal;
	b = (sxi32)pTos->x.iVal;
	if( pInstr->iOp == PH7_OP_SHL ){
		r = a << b;
	}else{
		r = a >> b;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	VmPopOperand(&pTos,1);
	break;
				 }
/*  OP_SHL_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * left by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
/* OP_SHR_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * right by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
case PH7_OP_SHL_STORE:
case PH7_OP_SHR_STORE: {
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxi64 a,r;
	sxi32 b;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer */
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pTos->x.iVal;
	b = (sxi32)pNos->x.iVal;
	if( pInstr->iOp == PH7_OP_SHL_STORE ){
		r = a << b;
	}else{
		r = a >> b;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_MemObjStore(pNos,pObj);
	}
	VmPopOperand(&pTos,1);
	break;
				 }
/* CAT:  P1 * *
 *
 * Pop P1 elements from the stack. Concatenate them togeher and push the result
 * back.
 */
case PH7_OP_CAT:{
	ph7_value *pNos,*pCur;
	if( pInstr->iP1 < 1 ){
		pNos = &pTos[-1];
	}else{
		pNos = &pTos[-pInstr->iP1+1];
	}
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force a string cast */
	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){
		PH7_MemObjToString(pNos);
	}
	pCur = &pNos[1];
	while( pCur <= pTos ){
		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(pCur);
		}
		/* Perform the concatenation */
		if( SyBlobLength(&pCur->sBlob) > 0 ){
			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));
		}
		SyBlobRelease(&pCur->sBlob);
		pCur++;
	}
	pTos = pNos;
	break;
				}
/*  CAT_STORE: * * *
 *
 * Pop two elements from the stack. Concatenate them togeher and push the result
 * back.
 */
case PH7_OP_CAT_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){
		/* Force a string cast */
		PH7_MemObjToString(pTos);
	}
	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){
		/* Force a string cast */
		PH7_MemObjToString(pNos);
	}
	/* Perform the concatenation (Reverse order) */
	if( SyBlobLength(&pNos->sBlob) > 0 ){
		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));
	}
	/* Perform the store operation */
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_MemObjStore(pTos,pObj);
	}
	PH7_MemObjStore(pTos,pNos);
	VmPopOperand(&pTos,1);
	break;
				}
/* OP_AND: * * *
 *
 * Pop two values off the stack.  Take the logical AND of the
 * two values and push the resulting boolean value back onto the
 * stack. 
 */
/* OP_OR: * * *
 *
 * Pop two values off the stack.  Take the logical OR of the
 * two values and push the resulting boolean value back onto the
 * stack. 
 */
case PH7_OP_LAND:
case PH7_OP_LOR: {
	ph7_value *pNos = &pTos[-1];
	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force a boolean cast */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pNos);
	}
	v1 = pNos->x.iVal == 0 ? 1 : 0;
	v2 = pTos->x.iVal == 0 ? 1 : 0;
	if( pInstr->iOp == PH7_OP_LAND ){
		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };
		v1 = and_logic[v1*3+v2];
	}else{
		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };
		v1 = or_logic[v1*3+v2];
	}
	if( v1 == 2 ){
		v1 = 1;
	}
	VmPopOperand(&pTos,1);
	pTos->x.iVal = v1 == 0 ? 1 : 0;
	MemObjSetType(pTos,MEMOBJ_BOOL);
	break;
				 }
/* OP_LXOR: * * *
 *
 * Pop two values off the stack. Take the logical XOR of the
 * two values and push the resulting boolean value back onto the
 * stack.
 * According to the PHP language reference manual:
 *  $a xor $b is evaluated to TRUE if either $a or $b is 
 *  TRUE,but not both.
 */
case PH7_OP_LXOR:{
	ph7_value *pNos = &pTos[-1];
	sxi32 v = 0;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force a boolean cast */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pNos);
	}
	if( (pNos->x.iVal && !pTos->x.iVal) || (pTos->x.iVal && !pNos->x.iVal) ){
		v = 1;
	}
	VmPopOperand(&pTos,1);
	pTos->x.iVal = v;
	MemObjSetType(pTos,MEMOBJ_BOOL);
	break;
				 }
/* OP_EQ P1 P2 P3
 *
 * Pop the top two elements from the stack.  If they are equal, then
 * jump to instruction P2.  Otherwise, continue to the next instruction.
 * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not. 
 */
/* OP_NEQ P1 P2 P3
 *
 * Pop the top two elements from the stack. If they are not equal, then
 * jump to instruction P2. Otherwise, continue to the next instruction.
 * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 */
case PH7_OP_EQ:
case PH7_OP_NEQ: {
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	if( pInstr->iOp == PH7_OP_EQ ){
		rc = rc == 0;
	}else{
		rc = rc != 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	break;
				 }
/* OP_TEQ P1 P2 *
 *
 * Pop the top two elements from the stack. If they have the same type and are equal
 * then jump to instruction P2. Otherwise, continue to the next instruction.
 * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not. 
 */
case PH7_OP_TEQ: {
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0) == 0;
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	break;
				 }
/* OP_TNE P1 P2 *
 *
 * Pop the top two elements from the stack.If they are not equal an they are not 
 * of the same type, then jump to instruction P2. Otherwise, continue to the next 
 * instruction.
 * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 * 
 */
case PH7_OP_TNE: {
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0) != 0;
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	break;
				 }
/* OP_LT P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is less than the first (next on stack),then jump to instruction P2.Otherwise
 * continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 * 
 */
/* OP_LE P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is less than or equal to the first (next on stack),then jump to instruction P2.
 * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 * 
 */
case PH7_OP_LT:
case PH7_OP_LE: {
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	if( pInstr->iOp == PH7_OP_LE ){
		rc = rc < 1;
	}else{
		rc = rc < 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	break;
				}
/* OP_GT P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is greater than the first (next on stack),then jump to instruction P2.Otherwise
 * continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 * 
 */
/* OP_GE P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is greater than or equal to the first (next on stack),then jump to instruction P2.
 * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 * 
 */
case PH7_OP_GT:
case PH7_OP_GE: {
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	if( pInstr->iOp == PH7_OP_GE ){
		rc = rc >= 0;
	}else{
		rc = rc > 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	break;
				}
/* OP_SEQ P1 P2 *
 * Strict string comparison.
 * Pop the top two elements from the stack. If they are equal (pure text comparison)
 * then jump to instruction P2.  Otherwise, continue to the next instruction.
 * If either operand is NULL then the comparison result is FALSE.
 * The SyMemcmp() routine is used for the comparison. For a numeric comparison
 * use PH7_OP_EQ.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 */
/* OP_SNE P1 P2 *
 * Strict string comparison.
 * Pop the top two elements from the stack. If they are not equal (pure text comparison)
 * then jump to instruction P2.  Otherwise, continue to the next instruction.
 * If either operand is NULL then the comparison result is FALSE.
 * The SyMemcmp() routine is used for the comparison. For a numeric comparison
 * use PH7_OP_EQ.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 */
case PH7_OP_SEQ:
case PH7_OP_SNE: {
	ph7_value *pNos = &pTos[-1];
	SyString s1,s2;
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force a string cast */
	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){
		PH7_MemObjToString(pTos);
	}
	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){
		PH7_MemObjToString(pNos);
	}
	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));
	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	rc = SyStringCmp(&s1,&s2,SyMemcmp);
	if( pInstr->iOp == PH7_OP_NEQ ){
		rc = rc != 0;
	}else{
		rc = rc == 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	break;
				 }
/*
 * OP_LOAD_REF * * *
 * Push the index of a referenced object on the stack.
 */
case PH7_OP_LOAD_REF: {
	sxu32 nIdx;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Extract memory object index */
	nIdx = pTos->nIdx;
	if( nIdx != SXU32_HIGH /* Not a constant */ ){
		/* Nullify the object */
		PH7_MemObjRelease(pTos);
		/* Mark as constant and store the index on the top of the stack */
		pTos->x.iVal = (sxi64)nIdx;
		pTos->nIdx = SXU32_HIGH;
		pTos->iFlags = MEMOBJ_INT|MEMOBJ_REFERENCE;
	}
	break;
					  }
/*
 * OP_STORE_REF * * P3
 * Perform an assignment operation by reference.
 */
case PH7_OP_STORE_REF: {
	SyString sName = { 0 , 0 };
	SyHashEntry *pEntry;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( pInstr->p3 == 0 ){
		char *zName;
		/* Take the variable name from the Next on the stack */
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		if( SyBlobLength(&pTos->sBlob) > 0 ){
			zName = SyMemBackendStrDup(&pVm->sAllocator,
				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			if( zName ){
				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));
			}
		}
		PH7_MemObjRelease(pTos);
		pTos--;
	}else{
		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
	}
	nIdx = pTos->nIdx;
	if(nIdx == SXU32_HIGH ){
		if( (pTos->iFlags & (MEMOBJ_OBJ|MEMOBJ_HASHMAP|MEMOBJ_RES)) == 0 ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Reference operator require a variable not a constant as it's right operand");
		}else{
			ph7_value *pObj;
			/* Extract the desired variable and if not available dynamically create it */
			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);
			if( pObj == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,
					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);
				goto Abort;
			}
			/* Perform the store operation */
			PH7_MemObjStore(pTos,pObj);
			pTos->nIdx = pObj->nIdx;
		}
	}else if( sName.nByte > 0){
		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");
		}else{
			VmFrame *pFrame = pVm->pFrame;
			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
				/* Safely ignore the exception frame */
				pFrame = pFrame->pParent;
			}
			/* Query the local frame */
			pEntry = SyHashGet(&pFrame->hVar,(const void *)sName.zString,sName.nByte);
			if( pEntry ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);
			}else{
				rc = SyHashInsert(&pFrame->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));
				if( pFrame->pParent == 0 ){
					/* Insert in the $GLOBALS array */
					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);
				}
				if( rc == SXRET_OK ){
					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);
				}
			}
		}
	}
	break;
				 }
/*
 * OP_UPLINK P1 * *
 * Link a variable to the top active VM frame. 
 * This is used to implement the 'global' PHP construct.
 */
case PH7_OP_UPLINK: {
	if( pVm->pFrame->pParent ){
		ph7_value *pLink = &pTos[-pInstr->iP1+1];
		SyString sName;
		/* Perform the link */
		while( pLink <= pTos ){
			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){
				/* Force a string cast */
				PH7_MemObjToString(pLink);
			}
			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));
			if( sName.nByte > 0 ){
				VmFrameLink(&(*pVm),&sName);
			}
			pLink++;
		}
	}
	VmPopOperand(&pTos,pInstr->iP1);
	break;
					}
/*
 * OP_LOAD_EXCEPTION * P2 P3
 * Push an exception in the corresponding container so that
 * it can be thrown later by the OP_THROW instruction.
 */
case PH7_OP_LOAD_EXCEPTION: {
	ph7_exception *pException = (ph7_exception *)pInstr->p3;
	VmFrame *pFrame;
	SySetPut(&pVm->aException,(const void *)&pException);
	/* Create the exception frame */
	rc = VmEnterFrame(&(*pVm),0,0,&pFrame);
	if( rc != SXRET_OK ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		goto Abort;
	}
	/* Mark the special frame */
	pFrame->iFlags |= VM_FRAME_EXCEPTION;
	pFrame->iExceptionJump = pInstr->iP2;
	/* Point to the frame that trigger the exception */
	pFrame = pFrame->pParent;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		pFrame = pFrame->pParent;
	}
	pException->pFrame = pFrame;
	break;
							}
/*
 * OP_POP_EXCEPTION * * P3
 * Pop a previously pushed exception from the corresponding container.
 */
case PH7_OP_POP_EXCEPTION: {
	ph7_exception *pException = (ph7_exception *)pInstr->p3;
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception **apException;
		/* Pop the loaded exception */
		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);
		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){
			(void)SySetPop(&pVm->aException);
		}
	}
	pException->pFrame = 0;
	/* Leave the exception frame */
	VmLeaveFrame(&(*pVm));
	break;
							}

/*
 * OP_THROW * P2 *
 * Throw an user exception.
 */
case PH7_OP_THROW: {
	VmFrame *pFrame = pVm->pFrame;
	sxu32 nJump = pInstr->iP2;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	/* Tell the upper layer that an exception was thrown */
	pFrame->iFlags |= VM_FRAME_THROW;
	if( pTos->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
		ph7_class *pException;
		/* Make sure the loaded object is an instance of the 'Exception' base class.
		 */
		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);
		if( pException == 0 || !VmInstanceOf(pThis->pClass,pException) ){
			/* Exceptions must be valid objects derived from the Exception base class */
			rc = VmUncaughtException(&(*pVm),pThis);
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				goto Abort;
			}
		}else{
			/* Throw the exception */
			rc = VmThrowException(&(*pVm),pThis);
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				goto Abort;
			}
		}
	}else{
		/* Expecting a class instance */
		VmUncaughtException(&(*pVm),0);
		if( rc == SXERR_ABORT ){
			/* Abort processing immediately */
			goto Abort;
		}
	}
	/* Pop the top entry */
	VmPopOperand(&pTos,1);
	/* Perform an unconditional jump */
	pc = nJump - 1;
	break;
				   }
/*
 * OP_FOREACH_INIT * P2 P3
 * Prepare a foreach step.
 */
case PH7_OP_FOREACH_INIT: {
	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;
	void *pName;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( SyStringLength(&pInfo->sValue) < 1 ){
		/* Take the variable name from the top of the stack */
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		/* Duplicate name */
		if( SyBlobLength(&pTos->sBlob) > 0 ){
			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));
		}
		VmPopOperand(&pTos,1);
	}
	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		/* Duplicate name */
		if( SyBlobLength(&pTos->sBlob) > 0 ){
			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));
		}
		VmPopOperand(&pTos,1);
	}
	/* Make sure we are dealing with a hashmap aka 'array' or an object */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ)) == 0 || SyStringLength(&pInfo->sValue) < 1 ){
		/* Jump out of the loop */
		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");
		}
		pc = pInstr->iP2 - 1;
	}else{
		ph7_foreach_step *pStep;
		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));
		if( pStep == 0 ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");
			/* Jump out of the loop */
			pc = pInstr->iP2 - 1;
		}else{
			/* Zero the structure */
			SyZero(pStep,sizeof(ph7_foreach_step));
			/* Prepare the step */
			pStep->iFlags = pInfo->iFlags;
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;
				/* Reset the internal loop cursor */
				PH7_HashmapResetLoopCursor(pMap);
				/* Mark the step */
				pStep->iFlags |= PH7_4EACH_STEP_HASHMAP;
				pStep->xIter.pMap = pMap;
				pMap->iRef++;
			}else{
				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
				/* Reset the loop cursor */
				SyHashResetLoopCursor(&pThis->hAttr);
				/* Mark the step */
				pStep->iFlags |= PH7_4EACH_STEP_OBJECT;
				pStep->xIter.pThis = pThis;
				pThis->iRef++;
			}
		}
		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");
			SyMemBackendPoolFree(&pVm->sAllocator,pStep);
			/* Jump out of the loop */
			pc = pInstr->iP2 - 1;
		}
	}
	VmPopOperand(&pTos,1);
	break;
						  }
/*
 * OP_FOREACH_STEP * P2 P3
 * Perform a foreach step. Jump to P2 at the end of the step.
 */
case PH7_OP_FOREACH_STEP: {
	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;
	ph7_foreach_step **apStep,*pStep;
	ph7_value *pValue;
	VmFrame *pFrame;
	/* Peek the last step */
	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];
	pFrame = pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){
		ph7_hashmap *pMap = pStep->xIter.pMap;
		ph7_hashmap_node *pNode;
		/* Extract the current node value */
		pNode = PH7_HashmapGetNextEntry(pMap);
		if( pNode == 0 ){
			/* No more entry to process */
			pc = pInstr->iP2 - 1; /* Jump to this destination */
			if( pStep->iFlags & PH7_4EACH_STEP_REF ){
				/* Break the reference with the last element */
				SyHashDeleteEntry(&pFrame->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
			}
			/* Automatically reset the loop cursor */
			PH7_HashmapResetLoopCursor(pMap);
			/* Cleanup the mess left behind */
			SyMemBackendPoolFree(&pVm->sAllocator,pStep);
			SySetPop(&pInfo->aStep);
			PH7_HashmapUnref(pMap);
		}else{
			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){
				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);
				if( pKey ){
					PH7_HashmapExtractNodeKey(pNode,pKey);
				}
			}
			if( pStep->iFlags & PH7_4EACH_STEP_REF ){
				SyHashEntry *pEntry;
				/* Pass by reference */
				pEntry = SyHashGet(&pFrame->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));
				if( pEntry ){
					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);
				}else{
					SyHashInsert(&pFrame->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),
						SX_INT_TO_PTR(pNode->nValIdx));
				}
			}else{
				/* Make a copy of the entry value */
				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
				if( pValue ){
					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);
				}
			}
		}
	}else{
		ph7_class_instance *pThis = pStep->xIter.pThis;
		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */
		SyHashEntry *pEntry;
		/* Point to the next attribute */
		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
			pVmAttr = (VmClassAttr *)pEntry->pUserData;
			/* Check access permission */
			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,
				pVmAttr->pAttr->iProtection,FALSE) ){
					break; /* Access is granted */
			}
		}
		if( pEntry == 0 ){
			/* Clean up the mess left behind */
			pc = pInstr->iP2 - 1; /* Jump to this destination */
			if( pStep->iFlags & PH7_4EACH_STEP_REF ){
				/* Break the reference with the last element */
				SyHashDeleteEntry(&pFrame->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
			}
			SyMemBackendPoolFree(&pVm->sAllocator,pStep);
			SySetPop(&pInfo->aStep);
			PH7_ClassInstanceUnref(pThis);
		}else{
			SyString *pAttrName = &pVmAttr->pAttr->sName;
			ph7_value *pAttrValue;
			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){
				/* Fill with the current attribute name */
				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);
				if( pKey ){
					SyBlobReset(&pKey->sBlob);
					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);
					MemObjSetType(pKey,MEMOBJ_STRING);
				}
			}
			/* Extract attribute value */
			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
			if( pAttrValue ){
				if( pStep->iFlags & PH7_4EACH_STEP_REF ){
					/* Pass by reference */
					pEntry = SyHashGet(&pFrame->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));
					if( pEntry ){
						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);
					}else{
						SyHashInsert(&pFrame->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),
							SX_INT_TO_PTR(pVmAttr->nIdx));
					} 
				}else{
					/* Make a copy of the attribute value */
					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
					if( pValue ){
						PH7_MemObjStore(pAttrValue,pValue);
					}
				}
			}
		}
	}
	break;
						  }
/*
 * OP_MEMBER P1 P2
 * Load class attribute/method on the stack.
 */
case PH7_OP_MEMBER: {
	ph7_class_instance *pThis;
	ph7_value *pNos;
	SyString sName;
	if( !pInstr->iP1 ){
		pNos = &pTos[-1];
#ifdef UNTRUST
		if( pNos < pStack ){
			goto Abort;
		}
#endif
		if( pNos->iFlags & MEMOBJ_OBJ ){
			ph7_class *pClass;
			/* Class already instantiated */
			pThis = (ph7_class_instance *)pNos->x.pOther;
			/* Point to the instantiated class */
			pClass = pThis->pClass;
			/* Extract attribute name first */
			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			if( pInstr->iP2 ){
				/* Method call */
				ph7_class_method *pMeth = 0;
				if( sName.nByte > 0 ){
					/* Extract the target method */
					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);
				}
				if( pMeth == 0 ){
					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",
						&pClass->sName,&sName
						);
					/* Call the '__Call()' magic method if available */
					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);
					/* Pop the method name from the stack */
					VmPopOperand(&pTos,1);
					PH7_MemObjRelease(pTos);
				}else{
					/* Push method name on the stack */
					PH7_MemObjRelease(pTos);
					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
					MemObjSetType(pTos,MEMOBJ_STRING);
				}
				pTos->nIdx = SXU32_HIGH;
			}else{
				/* Attribute access */
				VmClassAttr *pObjAttr = 0;
				SyHashEntry *pEntry;
				/* Extract the target attribute */
				if( sName.nByte > 0 ){
					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);
					if( pEntry ){
						/* Point to the attribute value */
						pObjAttr = (VmClassAttr *)pEntry->pUserData;
					}
				}
				if( pObjAttr == 0 ){
					/* No such attribute,load null */
					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",
						&pClass->sName,&sName);
					/* Call the __get magic method if available */
					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);
				}
				VmPopOperand(&pTos,1); 
				/* TICKET 1433-49: Deffer garbage collection until attribute loading.
				 * This is due to the following case:
				 *     (new TestClass())->foo;
				 */
				pThis->iRef++;
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */
				if( pObjAttr ){
					ph7_value *pValue = 0; /* cc warning */
					/* Check attribute access */
					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){
						/* Load attribute */
						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);
						if( pValue ){
							if( pThis->iRef < 2 ){
								/* Perform a store operation,rather than a load operation since
								 * the class instance '$this' will be deleted shortly.
								 */
								PH7_MemObjStore(pValue,pTos);
							}else{
								/* Simple load */
								PH7_MemObjLoad(pValue,pTos);
							}
							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
								if( pThis->iRef > 1 ){
									/* Load attribute index */
									pTos->nIdx = pObjAttr->nIdx;
								}
							}
						}
					}
				}
				/* Safely unreference the object */
				PH7_ClassInstanceUnref(pThis);
			}
		}else{
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */
		}
	}else{
		/* Static member access using class name */
		pNos = pTos;
		pThis = 0;
		if( !pInstr->p3 ){
			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			pNos--;
#ifdef UNTRUST
			if( pNos < pStack ){
				goto Abort;
			}
#endif
		}else{
			/* Attribute name already computed */
			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
		}
		if( pNos->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ) ){
			ph7_class *pClass = 0;	
			if( pNos->iFlags & MEMOBJ_OBJ ){
				/* Class already instantiated */
				pThis = (ph7_class_instance *)pNos->x.pOther;
				pClass = pThis->pClass;
				pThis->iRef++; /* Deffer garbage collection */
			}else{
				/* Try to extract the target class */
				if( SyBlobLength(&pNos->sBlob) > 0 ){
					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),
						SyBlobLength(&pNos->sBlob),FALSE,0);
				}
			}
			if( pClass == 0 ){
				/* Undefined class */
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",
					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)
					);
				if( !pInstr->p3 ){
					VmPopOperand(&pTos,1);
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
			}else{	
				if( pInstr->iP2 ){
					/* Method call */
					ph7_class_method *pMeth = 0;
					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){
						/* Extract the target method */
						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);
					}
					if( pMeth == 0 || (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){
						if( pMeth ){
							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",
								&pClass->sName,&sName
								);
						}else{
							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",
								&pClass->sName,&sName
								);
							/* Call the '__CallStatic()' magic method if available */
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);
						}
						/* Pop the method name from the stack */
						if( !pInstr->p3 ){
							VmPopOperand(&pTos,1);
						}
						PH7_MemObjRelease(pTos);
					}else{
						/* Push method name on the stack */
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
						MemObjSetType(pTos,MEMOBJ_STRING);
					}
					pTos->nIdx = SXU32_HIGH;
				}else{
					/* Attribute access */
					ph7_class_attr *pAttr = 0;
					/* Extract the target attribute */
					if( sName.nByte > 0 ){
						pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);
					}
					if( pAttr == 0 ){
						/* No such attribute,load null */
						VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",
							&pClass->sName,&sName);
						/* Call the __get magic method if available */
						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);
					}
					/* Pop the attribute name from the stack */
					if( !pInstr->p3 ){
						VmPopOperand(&pTos,1);
					}
					PH7_MemObjRelease(pTos);
					pTos->nIdx = SXU32_HIGH;
					if( pAttr ){
						if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
							/* Access to a non static attribute */
							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",
								&pClass->sName,&pAttr->sName
								);
						}else{
							ph7_value *pValue;
							/* Check if the access to the attribute is allowed */
							if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){
								/* Load the desired attribute */
								pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);
								if( pValue ){
									PH7_MemObjLoad(pValue,pTos);
									if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){
										/* Load index number */
										pTos->nIdx = pAttr->nIdx;
									}
								}
							}
						}
					}
				}
				if( pThis ){
					/* Safely unreference the object */
					PH7_ClassInstanceUnref(pThis);
				}
			}
		}else{
			/* Pop operands */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");
			if( !pInstr->p3 ){
				VmPopOperand(&pTos,1);
			}
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH;
		}
	}
	break;
					}
/*
 * OP_NEW P1 * * *
 *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.
 */
case PH7_OP_NEW: {
	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */
	ph7_class *pClass = 0;
	ph7_class_instance *pNew;
	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){
		/* Try to extract the desired class */
		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),
			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);
	}else if( pTos->iFlags & MEMOBJ_OBJ ){
		/* Take the base class from the loaded instance */
		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;
	}
	if( pClass == 0 ){
		/* No such class */
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",
			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)
			);
		PH7_MemObjRelease(pTos);
		if( pInstr->iP1 > 0 ){
			/* Pop given arguments */
			VmPopOperand(&pTos,pInstr->iP1);
		}
	}else{
		ph7_class_method *pCons;
		/* Create a new class instance */
		pNew = PH7_NewClassInstance(&(*pVm),pClass);
		if( pNew == 0 ){
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,
				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",
				&pClass->sName
			);
			PH7_MemObjRelease(pTos);
			if( pInstr->iP1 > 0 ){
				/* Pop given arguments */
				VmPopOperand(&pTos,pInstr->iP1);
			}
			break;
		}
		/* Check if a constructor is available */
		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
		if( pCons == 0 ){
			SyString *pName = &pClass->sName;
			/* Check for a constructor with the same base class name */
			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);
		}
		if( pCons ){
			/* Call the class constructor */
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			if( pVm->bErrReport ){
				ph7_vm_func_arg *pFuncArg;
				sxu32 n;
				n = SySetUsed(&aArg);
				/* Emit a notice for missing arguments */
				while( n < SySetUsed(&pCons->sFunc.aArgs) ){
					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);
					if( pFuncArg ){
						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){
							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",
								n+1,&pFuncArg->sName,&pClass->sName);
						}
					}
					n++;
				}
			}
			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));
			/* TICKET 1433-52: Unsetting $this in the constructor body */
			if( pNew->iRef < 1 ){
				pNew->iRef = 1; 
			}
		}
		if( pInstr->iP1 > 0 ){
			/* Pop given arguments */
			VmPopOperand(&pTos,pInstr->iP1);
		}
		PH7_MemObjRelease(pTos);
		pTos->x.pOther = pNew;
		MemObjSetType(pTos,MEMOBJ_OBJ);
	}
	break;
				 }
/*
 * OP_CLONE * * *
 * Perfome a clone operation.
 */
case PH7_OP_CLONE: {
	ph7_class_instance *pSrc,*pClone;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Make sure we are dealing with a class instance */
	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");
		PH7_MemObjRelease(pTos);
		break;
	}
	/* Point to the source */
	pSrc = (ph7_class_instance *)pTos->x.pOther;
	/* Perform the clone operation */
	pClone = PH7_CloneClassInstance(pSrc);
	PH7_MemObjRelease(pTos);
	if( pClone == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");
	}else{
		/* Load the cloned object */
		pTos->x.pOther = pClone;
		MemObjSetType(pTos,MEMOBJ_OBJ);
	}
	break;
				   }
/*
 * OP_SWITCH * * P3
 *  This is the bytecode implementation of the complex switch() PHP construct.
 */
case PH7_OP_SWITCH: {
	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;
	ph7_case_expr *aCase,*pCase;
	ph7_value sValue,sCaseValue; 
	sxu32 n,nEntry;
#ifdef UNTRUST
	if( pSwitch == 0 || pTos < pStack ){
		goto Abort;
	}
#endif
	/* Point to the case table  */
	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);
	nEntry = SySetUsed(&pSwitch->aCaseExpr);
	/* Select the appropriate case block to execute */
	PH7_MemObjInit(pVm,&sValue);
	PH7_MemObjInit(pVm,&sCaseValue);
	for( n = 0 ; n < nEntry ; ++n ){
		pCase = &aCase[n];
		PH7_MemObjLoad(pTos,&sValue);
		/* Execute the case expression first */
		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);
		/* Compare the two expression */
		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);
		PH7_MemObjRelease(&sValue);
		PH7_MemObjRelease(&sCaseValue);
		if( rc == 0 ){
			/* Value match,jump to this block */
			pc = pCase->nStart - 1;
			break;
		}
	}
	VmPopOperand(&pTos,1);
	if( n >= nEntry ){
		/* No approprite case to execute,jump to the default case */
		if( pSwitch->nDefault > 0 ){
			pc = pSwitch->nDefault - 1;
		}else{
			/* No default case,jump out of this switch */
			pc = pSwitch->nOut - 1;
		}
	}
	break;
					}
/*
 * OP_CALL P1 * *
 *  Call a PHP or a foreign function and push the return value of the called
 *  function on the stack.
 */
case PH7_OP_CALL: {
	ph7_value *pArg = &pTos[-pInstr->iP1];
	SyHashEntry *pEntry;
	SyString sName;
	/* Extract function name */
	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
		if( pTos->iFlags & MEMOBJ_HASHMAP ){
			ph7_value sResult;
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			PH7_MemObjInit(pVm,&sResult);
			/* May be a class instance and it's static method */
			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);
			SySetReset(&aArg);
			/* Pop given arguments */
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			/* Copy result */
			PH7_MemObjStore(&sResult,pTos);
			PH7_MemObjRelease(&sResult);
		}else{	
			if( pTos->iFlags & MEMOBJ_OBJ ){
				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
				/* Call the magic method '__invoke' if available */
				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);
			}else{
				/* Raise exception: Invalid function name */
				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");
			}
			/* Pop given arguments */
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
		}
		break;
	}
	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	/* Check for a compiled function first */
	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);
	if( pEntry ){
		ph7_vm_func_arg *aFormalArg;
		ph7_class_instance *pThis;
		ph7_value *pFrameStack;
		ph7_vm_func *pVmFunc;
		ph7_class *pSelf;
		VmFrame *pFrame;
		ph7_value *pObj;
		VmSlot sArg;
		sxu32 n;
		/* initialize fields */
		pVmFunc = (ph7_vm_func *)pEntry->pUserData;
		pThis = 0;
		pSelf = 0;
		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){
			ph7_class_method *pMeth;
			/* Class method call */
			ph7_value *pTarget = &pTos[-1];
			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ|MEMOBJ_NULL)) ){
				/* Extract the 'this' pointer */
				if(pTarget->iFlags & MEMOBJ_OBJ ){
					/* Instance already loaded */
					pThis = (ph7_class_instance *)pTarget->x.pOther;
					pThis->iRef++;
					pSelf = pThis->pClass;
				}
				if( pSelf == 0 ){
					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){
						/* "Late Static Binding" class name */
						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),
							SyBlobLength(&pTarget->sBlob),FALSE,0);
					}
					if( pSelf == 0 ){
						pSelf = (ph7_class *)pVmFunc->pUserData;
					}
				}
				if( pThis == 0  ){
					VmFrame *pFrame = pVm->pFrame;
					while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
						/* Safely ignore the exception frame */
						pFrame = pFrame->pParent;
					}
					if( pFrame->pParent ){
						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */
						pThis = pFrame->pThis;
						if( pThis ){
							pThis->iRef++;
						}
					}
				}
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				/* Synchronize pointers */
				pArg = &pTos[-pInstr->iP1];
				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'
				 * user have already computed the random generated unique class method name 
				 * and tries to call it outside it's context [i.e: global scope]. In that 
				 * case we have to synchronize pointers to avoid stack underflow.
				 */
				while( pArg < pStack ){
					pArg++;
				}
				if( pSelf ){ /* Paranoid edition */
					/* Check if the call is allowed */
					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);
					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){
							/* Pop given arguments */
							if( pInstr->iP1 > 0 ){
								VmPopOperand(&pTos,pInstr->iP1);
							}
							/* Assume a null return value so that the program continue it's execution normally */
							PH7_MemObjRelease(pTos);
							break;
						}
					}
				}
			}
		}
		/* Check The recursion limit */
		if( pVm->nRecursionDepth > pVm->nMaxDepth ){
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,
				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",
				&pVmFunc->sName);
			/* Pop given arguments */
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
			break;
		}
		if( pVmFunc->pNextName ){
			/* Function is candidate for overloading,select the appropriate function to call */
			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));
		}
		/* Extract the formal argument set */
		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
		/* Create a new VM frame  */
		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);
		if( rc != SXRET_OK ){
			/* Raise exception: Out of memory */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,
				"PH7 is running out of memory while calling function '%z',NULL will be returned",
				&pVmFunc->sName);
			/* Pop given arguments */
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
			break;
		}
		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){
			/* Install the '$this' variable */
			static const SyString sThis = { "this" , sizeof("this") - 1 };
			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);
			if( pObj ){
				/* Reflect the change */
				pObj->x.pOther = pThis;
				MemObjSetType(pObj,MEMOBJ_OBJ);
			}
		}
		if( SySetUsed(&pVmFunc->aStatic) > 0 ){
			ph7_vm_func_static_var *pStatic,*aStatic;
			/* Install static variables */
			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);
			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){
				pStatic = &aStatic[n];
				if( pStatic->nIdx == SXU32_HIGH ){
					/* Initialize the static variables */
					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);
					if( pObj ){
						/* Assume a NULL initialization value */
						PH7_MemObjInit(&(*pVm),pObj);
						if( SySetUsed(&pStatic->aByteCode) > 0 ){
							/* Evaluate initialization expression (Any complex expression) */
							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);
						}
						pObj->nIdx = pStatic->nIdx;
					}else{
						continue;
					}
				}
				/* Install in the current frame */
				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),
					SX_INT_TO_PTR(pStatic->nIdx));
			}
		}
		/* Push arguments in the local frame */
		n = 0;
		while( pArg < pTos ){
			if( n < SySetUsed(&pVmFunc->aArgs) ){
				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
					/* NULL values are redirected to default arguments */
					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);
					if( rc == PH7_ABORT ){
						goto Abort;
					}
				}
				/* Make sure the given arguments are of the correct type */
				if( aFormalArg[n].nType > 0 ){
					if ( aFormalArg[n].nType == SXU32_HIGH ){
						/* Argument must be a class instance [i.e: object] */
						SyString *pName = &aFormalArg[n].sClass;
						ph7_class *pClass;
						/* Try to extract the desired class */
						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);
						if( pClass ){
							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){
								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){
									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",
										&pVmFunc->sName,n+1,pName);
									PH7_MemObjRelease(pArg);
								}
							}else{
								ph7_class_instance *pThis = (ph7_class_instance *)pArg->x.pOther;
								/* Make sure the object is an instance of the given class */
								if( ! VmInstanceOf(pThis->pClass,pClass) ){
									VmErrorFormat(&(*pVm),PH7_CTX_ERR,
										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",
										&pVmFunc->sName,n+1,pName);
									PH7_MemObjRelease(pArg);
								}
							}
						}
					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						/* Cast to the desired type */
						xCast(pArg);
					}
				}
				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){
					/* Pass by reference */
					if( pArg->nIdx == SXU32_HIGH ){
						/* Expecting a variable,not a constant,raise an exception */
						if((pArg->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0){
							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
								"Function '%z',%d argument: Pass by reference,expecting a variable not a "
								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);
						}
						/* Switch to pass by value */
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
					}else{
						SyHashEntry *pRefEntry;
						/* Install the referenced variable in the private function frame */
						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));
						if( pRefEntry == 0 ){
							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),
								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));
							sArg.nIdx = pArg->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
						}
						pObj = 0;
					}
				}else{
					/* Pass by value,make a copy of the given argument */
					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				}
			}else{
				char zName[32];
				SyString sName;
				/* Set a dummy name */
				sName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);
				sName.zString = zName;
				/* Annonymous argument */
				pObj = VmExtractMemObj(&(*pVm),&sName,TRUE,TRUE);
			}
			if( pObj ){
				PH7_MemObjStore(pArg,pObj);
				/* Insert argument index  */
				sArg.nIdx = pObj->nIdx;
				sArg.pUserData = 0;
				SySetPut(&pFrame->sArg,(const void *)&sArg);
			}
			PH7_MemObjRelease(pArg);
			pArg++;
			++n;
		}
		/* Set up closure environment */
		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){
			ph7_vm_func_closure_env *aEnv,*pEnv;
			ph7_value *pValue;
			sxu32 n;
			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);
			for(n = 0 ; n < SySetUsed(&pVmFunc->aClosureEnv) ; ++n ){
				pEnv = &aEnv[n];
				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){
					/* Do not install null value */
					continue;
				}
				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);
				if( pValue == 0 ){
					continue;
				}
				/* Invalidate any prior representation */
				PH7_MemObjRelease(pValue);
				/* Duplicate bound variable value */
				PH7_MemObjStore(&pEnv->sValue,pValue);
			}
		}
		/* Process default values */
		while( n < SySetUsed(&pVmFunc->aArgs) ){
			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					/* Evaluate the default value and extract it's result */
					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);
					if( rc == PH7_ABORT ){
						goto Abort;
					}
					/* Insert argument index */
					sArg.nIdx = pObj->nIdx;
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
					/* Make sure the default argument is of the correct type */
					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						/* Cast to the desired type */
						xCast(pObj);
					}
				}
			}
			++n;
		}
		/* Pop arguments,function name from the operand stack and assume the function 
		 * does not return anything.
		 */
		PH7_MemObjRelease(pTos);
		pTos = &pTos[-pInstr->iP1];
		/* Allocate a new operand stack and evaluate the function body */
		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));
		if( pFrameStack == 0 ){
			/* Raise exception: Out of memory */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",
				&pVmFunc->sName);
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			break;
		}
		if( pSelf ){
			/* Push class name */
			SySetPut(&pVm->aSelf,(const void *)&pSelf);
		}
		/* Increment nesting level */
		pVm->nRecursionDepth++;
		/* Execute function body */
		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);
		/* Decrement nesting level */
		pVm->nRecursionDepth--;
		if( pSelf ){
			/* Pop class name */
			(void)SySetPop(&pVm->aSelf);
		}
		/* Cleanup the mess left behind */
		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){
			/* Return by reference,reflect that */
			if( n != SXU32_HIGH ){
				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);
				sxu32 i;
				/* Make sure the referenced object is not a local variable */
				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){
					if( n == aSlot[i].nIdx ){
						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);
						if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_OBJ|MEMOBJ_HASHMAP|MEMOBJ_RES)) == 0 ){
							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",
								&pVmFunc->sName);
						}
						n = SXU32_HIGH;
						break;
					}
				}
			}else{
				if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_NULL|MEMOBJ_RES)) == 0 ){
					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",
						&pVmFunc->sName);
				}
			}
			pTos->nIdx = n;
		}
		/* Cleanup the mess left behind */
		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) || rc == PH7_EXCEPTION) ){
			/* An exception was throw in this frame */
			pFrame = pFrame->pParent;
			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){
				/* Pop the resutlt */
				VmPopOperand(&pTos,1);
				/* Jump to this destination */
				pc = pFrame->iExceptionJump - 1;
				rc = PH7_OK;
			}else{
				if( pFrame->pParent ){
					rc = PH7_EXCEPTION;
				}else{
					/* Continue normal execution */
					rc = PH7_OK;
				}
			}
		}
		/* Free the operand stack */
		SyMemBackendFree(&pVm->sAllocator,pFrameStack);
		/* Leave the frame */
		VmLeaveFrame(&(*pVm));
		if( rc == PH7_ABORT ){
			/* Abort processing immeditaley */
			goto Abort;
		}else if( rc == PH7_EXCEPTION ){
			goto Exception;
		}
	}else{
		ph7_user_func *pFunc; 
		ph7_context sCtx;
		ph7_value sRet;
		/* Look for an installed foreign function */
		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);
		if( pEntry == 0 ){
			/* Call to undefined function */
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);
			/* Pop given arguments */
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
			break;
		}
		pFunc = (ph7_user_func *)pEntry->pUserData;
		/* Start collecting function arguments */
		SySetReset(&aArg);
		while( pArg < pTos ){
			SySetPut(&aArg,(const void *)&pArg);
			pArg++;
		}
		/* Assume a null return value */
		PH7_MemObjInit(&(*pVm),&sRet);
		/* Init the call context */
		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);
		/* Call the foreign function */
		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));
		/* Release the call context */
		VmReleaseCallContext(&sCtx);
		if( rc == PH7_ABORT ){
			goto Abort;
		}
		if( pInstr->iP1 > 0 ){
			/* Pop function name and arguments */
			VmPopOperand(&pTos,pInstr->iP1);
		}
		/* Save foreign function return value */
		PH7_MemObjStore(&sRet,pTos);
		PH7_MemObjRelease(&sRet);
	}
	break;
				  }
/*
 * OP_CONSUME: P1 * *
 * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.
 */
case PH7_OP_CONSUME: {
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	ph7_value *pCur,*pOut = pTos;

	pOut = &pTos[-pInstr->iP1 + 1];
	pCur = pOut;
	/* Start the consume process  */
	while( pOut <= pTos ){
		/* Force a string cast */
		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(pOut);
		}
		if( SyBlobLength(&pOut->sBlob) > 0 ){
			/*SyBlobNullAppend(&pOut->sBlob);*/
			/* Invoke the output consumer callback */
			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);
			if( pCons->xConsumer != VmObConsumer ){
				/* Increment output length */
				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);
			}
			SyBlobRelease(&pOut->sBlob);
			if( rc == SXERR_ABORT ){
				/* Output consumer callback request an operation abort. */
				goto Abort;
			}
		}
		pOut++;
	}
	pTos = &pCur[-1];
	break;
					 }

		} /* Switch() */
		pc++; /* Next instruction in the stream */
	} /* For(;;) */
Done:
	SySetRelease(&aArg);
	return SXRET_OK;
Abort:
	SySetRelease(&aArg);
	while( pTos >= pStack ){
		PH7_MemObjRelease(pTos);
		pTos--;
	}
	return PH7_ABORT;
Exception:
	SySetRelease(&aArg);
	while( pTos >= pStack ){
		PH7_MemObjRelease(pTos);
		pTos--;
	}
	return PH7_EXCEPTION;
}
/*
 * Execute as much of a local PH7 bytecode program as we can then return.
 * This function is a wrapper around [VmByteCodeExec()].
 * See block-comment on that function for additional information.
 */
static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)
{
	ph7_value *pStack;
	sxi32 rc;
	/* Allocate a new operand stack */
	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));
	if( pStack == 0 ){
		return SXERR_MEM;
	}
	/* Execute the program */
	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);
	/* Free the operand stack */
	SyMemBackendFree(&pVm->sAllocator,pStack);
	/* Execution result */
	return rc;
}
/*
 * Invoke any installed shutdown callbacks.
 * Shutdown callbacks are kept in a stack and are registered using one
 * or more calls to [register_shutdown_function()].
 * These callbacks are invoked by the virtual machine when the program
 * execution ends.
 * Refer to the implementation of [register_shutdown_function()] for
 * additional information.
 */
static void VmInvokeShutdownCallbacks(ph7_vm *pVm)
{
	VmShutdownCB *pEntry;
	ph7_value *apArg[10];
	sxu32 n,nEntry;
	int i;
	/* Point to the stack of registered callbacks */
	nEntry = SySetUsed(&pVm->aShutdown);
	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){
		apArg[i] = 0;
	}
	for( n = 0 ; n < nEntry ; ++n ){
		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);
		if( pEntry ){
			/* Prepare callback arguments if any */
			for( i = 0 ; i < pEntry->nArg ; i++ ){
				if( i >= (int)SX_ARRAYSIZE(apArg) ){
					break;
				}
				apArg[i] = &pEntry->aArg[i];
			}
			/* Invoke the callback */
			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);
			/* 
			 * TICKET 1433-56: Try re-access the same entry since the invoked
			 * callback may call [register_shutdown_function()] in it's body. 
			 */
			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);
			if( pEntry ){
				PH7_MemObjRelease(&pEntry->sCallback);
				for( i = 0 ; i < pEntry->nArg ; ++i ){
					PH7_MemObjRelease(apArg[i]);
				}
			}
		}
	}
	SySetReset(&pVm->aShutdown);
}
/*
 * Execute as much of a PH7 bytecode program as we can then return.
 * This function is a wrapper around [VmByteCodeExec()].
 * See block-comment on that function for additional information.
 */
PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)
{
	/* Make sure we are ready to execute this program */
	if( pVm->nMagic != PH7_VM_RUN ){
		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */
	}
	/* Set the execution magic number  */
	pVm->nMagic = PH7_VM_EXEC;
	/* Execute the program */
	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);
	/* Invoke any shutdown callbacks */
	VmInvokeShutdownCallbacks(&(*pVm));
	/*
	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number
	 * so that any following call to [ph7_vm_exec()] without calling
	 * [ph7_vm_reset()] first would fail.
	 */
	return SXRET_OK;
}
/*
 * Invoke the installed VM output consumer callback to consume
 * the desired message.
 * Refer to the implementation of [ph7_context_output()] defined
 * in 'api.c' for additional information.
 */
PH7_PRIVATE sxi32 PH7_VmOutputConsume(
	ph7_vm *pVm,      /* Target VM */
	SyString *pString /* Message to output */
	)
{
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	sxi32 rc = SXRET_OK;
	/* Call the output consumer */
	if( pString->nByte > 0 ){
		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);
		if( pCons->xConsumer != VmObConsumer ){
			/* Increment output length */
			pVm->nOutputLen += pString->nByte;
		}
	}
	return rc;
}
/*
 * Format a message and invoke the installed VM output consumer
 * callback to consume the formatted message.
 * Refer to the implementation of [ph7_context_output_format()] defined
 * in 'api.c' for additional information.
 */
PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(
	ph7_vm *pVm,         /* Target VM */
	const char *zFormat, /* Formatted message to output */
	va_list ap           /* Variable list of arguments */ 
	)
{
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	sxi32 rc = SXRET_OK;
	SyBlob sWorker;
	/* Format the message and call the output consumer */
	SyBlobInit(&sWorker,&pVm->sAllocator);
	SyBlobFormatAp(&sWorker,zFormat,ap);
	if( SyBlobLength(&sWorker) > 0 ){
		/* Consume the formatted message */
		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);
	}
	if( pCons->xConsumer != VmObConsumer ){
		/* Increment output length */
		pVm->nOutputLen += SyBlobLength(&sWorker);
	}
	/* Release the working buffer */
	SyBlobRelease(&sWorker);
	return rc;
}
/*
 * Return a string representation of the given PH7 OP code.
 * This function never fail and always return a pointer
 * to a null terminated string.
 */
static const char * VmInstrToString(sxi32 nOp)
{
	const char *zOp = "Unknown     ";
	switch(nOp){
	case PH7_OP_DONE:       zOp = "DONE       "; break;
	case PH7_OP_HALT:       zOp = "HALT       "; break;
	case PH7_OP_LOAD:       zOp = "LOAD       "; break;
	case PH7_OP_LOADC:      zOp = "LOADC      "; break;
	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;
	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;
	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;
	case PH7_OP_LOAD_CLOSURE:
		                    zOp = "LOAD_CLOSR "; break;
	case PH7_OP_NOOP:       zOp = "NOOP       "; break;
	case PH7_OP_JMP:        zOp = "JMP        "; break;
	case PH7_OP_JZ:         zOp = "JZ         "; break;
	case PH7_OP_JNZ:        zOp = "JNZ        "; break;
	case PH7_OP_POP:        zOp = "POP        "; break;
	case PH7_OP_CAT:        zOp = "CAT        "; break;
	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;
	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;
	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;
	case PH7_OP_CALL:       zOp = "CALL       "; break;
	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;
	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;
	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;
	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;
	case PH7_OP_MUL:        zOp = "MUL        "; break;
	case PH7_OP_DIV:        zOp = "DIV        "; break;
	case PH7_OP_MOD:        zOp = "MOD        "; break;
	case PH7_OP_ADD:        zOp = "ADD        "; break;
	case PH7_OP_SUB:        zOp = "SUB        "; break;
	case PH7_OP_SHL:        zOp = "SHL        "; break;
	case PH7_OP_SHR:        zOp = "SHR        "; break;
	case PH7_OP_LT:         zOp = "LT         "; break;
	case PH7_OP_LE:         zOp = "LE         "; break;
	case PH7_OP_GT:         zOp = "GT         "; break;
	case PH7_OP_GE:         zOp = "GE         "; break;
	case PH7_OP_EQ:         zOp = "EQ         "; break;
	case PH7_OP_NEQ:        zOp = "NEQ        "; break;
	case PH7_OP_TEQ:        zOp = "TEQ        "; break;
	case PH7_OP_TNE:        zOp = "TNE        "; break;
	case PH7_OP_BAND:       zOp = "BITAND     "; break;
	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;
	case PH7_OP_BOR:        zOp = "BITOR      "; break;
	case PH7_OP_LAND:       zOp = "LOGAND     "; break;
	case PH7_OP_LOR:        zOp = "LOGOR      "; break;
	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;
	case PH7_OP_STORE:      zOp = "STORE      "; break;
	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;
	case PH7_OP_STORE_IDX_REF:
		                    zOp = "STORE_IDX_R"; break;
	case PH7_OP_PULL:       zOp = "PULL       "; break;
	case PH7_OP_SWAP:       zOp = "SWAP       "; break;
	case PH7_OP_YIELD:      zOp = "YIELD      "; break;
	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;
	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;
	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;
	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;
	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;
	case PH7_OP_INCR:       zOp = "INCR       "; break;
	case PH7_OP_DECR:       zOp = "DECR       "; break;
	case PH7_OP_SEQ:        zOp = "SEQ        "; break;
	case PH7_OP_SNE:        zOp = "SNE        "; break;
	case PH7_OP_NEW:        zOp = "NEW        "; break;
	case PH7_OP_CLONE:      zOp = "CLONE      "; break;
	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;
	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;
	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;
	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;
	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;
	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;
	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;
	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;
	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;
	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;
	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;
	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;
	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;
	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;
	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;
	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;
	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;
	case PH7_OP_IS_A:       zOp = "IS_A       "; break;
	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;
	case PH7_OP_LOAD_EXCEPTION:
		                    zOp = "LOAD_EXCEP "; break;
	case PH7_OP_POP_EXCEPTION:
		                    zOp = "POP_EXCEP  "; break;
	case PH7_OP_THROW:      zOp = "THROW      "; break;
	case PH7_OP_FOREACH_INIT:
		                    zOp = "4EACH_INIT "; break;
	case PH7_OP_FOREACH_STEP:
						    zOp = "4EACH_STEP "; break;
	default:
		break;
	}
	return zOp;
}
/*
 * Dump PH7 bytecodes instructions to a human readable format.
 * The xConsumer() callback which is an used defined function
 * is responsible of consuming the generated dump.
 */
PH7_PRIVATE sxi32 PH7_VmDump(
	ph7_vm *pVm,            /* Target VM */
	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */
	void *pUserData         /* Last argument to xConsumer() */
	)
{
	sxi32 rc;
	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);
	return rc;
}
/*
 * Default constant expansion callback used by the 'const' statement if used
 * outside a class body [i.e: global or function scope].
 * Refer to the implementation of [PH7_CompileConstant()] defined
 * in 'compile.c' for additional information.
 */
PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)
{
	SySet *pByteCode = (SySet *)pUserData;
	/* Evaluate and expand constant value */
	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);
}
/*
 * Section:
 *  Function handling functions.
 * Status:
 *    Stable.
 */
/*
 * int func_num_args(void)
 *   Returns the number of arguments passed to the function.
 * Parameters
 *   None.
 * Return
 *  Total number of arguments passed into the current user-defined function
 *  or -1 if called from the globe scope.
 */
static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmFrame *pFrame;
	ph7_vm *pVm;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Current frame */
	pFrame = pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( pFrame->pParent == 0 ){
		SXUNUSED(nArg);
		SXUNUSED(apArg);
		/* Global frame,return -1 */
		ph7_result_int(pCtx,-1);
		return SXRET_OK;
	}
	/* Total number of arguments passed to the enclosing function */
	nArg = (int)SySetUsed(&pFrame->sArg);
	ph7_result_int(pCtx,nArg);
	return SXRET_OK;
}
/*
 * value func_get_arg(int $arg_num)
 *   Return an item from the argument list.
 * Parameters
 *  Argument number(index start from zero).
 * Return
 *  Returns the specified argument or FALSE on error.
 */
static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj = 0;
	VmSlot *pSlot = 0;
	VmFrame *pFrame;
	ph7_vm *pVm;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Current frame */
	pFrame = pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( nArg < 1 || pFrame->pParent == 0 ){
		/* Global frame or Missing arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract the desired index */
	nArg = ph7_value_to_int(apArg[0]);
	if( nArg < 0 || nArg >= (int)SySetUsed(&pFrame->sArg) ){
		/* Invalid index,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract the desired argument */
	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){
		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){
			/* Return the desired argument */
			ph7_result_value(pCtx,(ph7_value *)pObj);
		}else{
			/* No such argument,return false */
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* CAN'T HAPPEN */
		ph7_result_bool(pCtx,0);
	}
	return SXRET_OK;
}
/*
 * array func_get_args_byref(void)
 *   Returns an array comprising a function's argument list.
 * Parameters
 *  None.
 * Return
 *  Returns an array in which each element is a POINTER to the corresponding
 *  member of the current user-defined function's argument list.
 *  Otherwise FALSE is returned on failure.
 * NOTE:
 *  Arguments are returned to the array by reference.
 */
static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	VmFrame *pFrame;
	VmSlot *aSlot;
	sxu32 n;
	/* Point to the current frame */
	pFrame = pCtx->pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( pFrame->pParent == 0 ){
		/* Global frame,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Start filling the array with the given arguments (Pass by reference) */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){
		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * array func_get_args(void)
 *   Returns an array comprising a copy of function's argument list.
 * Parameters
 *  None.
 * Return
 *  Returns an array in which each element is a copy of the corresponding
 *  member of the current user-defined function's argument list.
 *  Otherwise FALSE is returned on failure.
 */
static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj = 0;
	ph7_value *pArray;
	VmFrame *pFrame;
	VmSlot *aSlot;
	sxu32 n;
	/* Point to the current frame */
	pFrame = pCtx->pVm->pFrame;
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( pFrame->pParent == 0 ){
		/* Global frame,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Start filling the array with the given arguments */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){
		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);
		if( pObj ){
			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);
		}
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * bool function_exists(string $name)
 *  Return TRUE if the given function has been defined.
 * Parameters
 *  The name of the desired function.
 * Return
 *  Return TRUE if the given function has been defined.False otherwise
 */
static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	ph7_vm *pVm;
	int nLen;
	int res;
	if( nArg < 1 ){
		/* Missing argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Extract the function name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	/* Assume the function is not defined */
	res = 0;
	/* Perform the lookup */
	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 ||
		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){
			/* Function is defined */
			res = 1;
	}
	ph7_result_bool(pCtx,res);
	return SXRET_OK;
}
/* Forward declaration */
static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);
/*
 * Verify that the contents of a variable can be called as a function.
 * [i.e: Whether it is callable or not].
 * Return TRUE if callable.FALSE otherwise.
 */
PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)
{
	int res = 0;
	if( pValue->iFlags & MEMOBJ_OBJ ){
		/* Call the magic method __invoke if available */
		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;
		ph7_class_method *pMethod;
		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);
		if( pMethod && CallInvoke ){
			ph7_value sResult;
			sxi32 rc;
			/* Invoke the magic method and extract the result */
			PH7_MemObjInit(pVm,&sResult);
			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);
			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL|MEMOBJ_INT)) ){
				res = sResult.x.iVal != 0;
			}
			PH7_MemObjRelease(&sResult);
		}
	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		if( pMap->nEntry > 1 ){
			ph7_class *pClass;
			ph7_value *pV;
			/* Extract the target class */
			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);
			if( pV ){
				pClass = VmExtractClassFromValue(pVm,pV);
				if( pClass ){
					ph7_class_method *pMethod;
					/* Extract the target method */
					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);
					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){
						/* Perform the lookup */
						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));
						if( pMethod ){
							/* Method is callable */
							res = 1;
						}
					}
				}
			}
		}
	}else if( pValue->iFlags & MEMOBJ_STRING ){
		const char *zName;
		int nLen;
		/* Extract the name */
		zName = ph7_value_to_string(pValue,&nLen);
		/* Perform the lookup */
		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 ||
			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){
				/* Function is callable */
				res = 1;
		}
	}
	return res;
}
/*
 * bool is_callable(callable $name[,bool $syntax_only = false])
 * Verify that the contents of a variable can be called as a function.
 * Parameters
 * $name
 *    The callback function to check
 * $syntax_only
 *    If set to TRUE the function only verifies that name might be a function or method.
 *    It will only reject simple variables that are not strings, or an array that does
 *    not have a valid structure to be used as a callback. The valid ones are supposed
 *    to have only 2 entries, the first of which is an object or a string, and the second
 *    a string.
 * Return
 *  TRUE if name is callable, FALSE otherwise.
 */
static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm;	
	int res;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Perform the requested operation */
	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);
	ph7_result_bool(pCtx,res);
	return SXRET_OK;
}
/*
 * Hash walker callback used by the [get_defined_functions()] function
 * defined below.
 */
static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_value sName;
	sxi32 rc;
	/* Prepare the function name for insertion */
	PH7_MemObjInitFromString(pArray->pVm,&sName,0);
	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);
	/* Perform the insertion */
	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */
	PH7_MemObjRelease(&sName);
	return rc;
}
/*
 * array get_defined_functions(void)
 *  Returns an array of all defined functions.
 * Parameter
 *  None.
 * Return
 *  Returns an multidimensional array containing a list of all defined functions
 *  both built-in (internal) and user-defined.
 *  The internal functions will be accessible via $arr["internal"], and the user 
 *  defined ones using $arr["user"]. 
 * Note:
 *  NULL is returned on failure.
 */
static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pEntry;
	/* NOTE:
	 * Don't worry about freeing memory here,every allocated resource will be released
	 * automatically by the engine as soon we return from this foreign function.
	 */
	pArray = ph7_context_new_array(pCtx);
 	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	pEntry = ph7_context_new_array(pCtx);
	if( pEntry == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Fill with the appropriate information */
	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);
	/* Create the 'internal' index */
	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */
	/* Create the user-func array */
	pEntry = ph7_context_new_array(pCtx);
	if( pEntry == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Fill with the appropriate information */
	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);
	/* Create the 'user' index */
	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */
	/* Return the multi-dimensional array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * void register_shutdown_function(callable $callback[,mixed $param,...)
 *  Register a function for execution on shutdown.
 * Note
 *  Multiple calls to register_shutdown_function() can be made, and each will
 *  be called in the same order as they were registered.
 * Parameters
 *  $callback
 *   The shutdown callback to register.
 * $param
 *  One or more Parameter to pass to the registered callback.
 * Return
 *  Nothing.
 */
static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmShutdownCB sEntry;
	int i,j;
	if( nArg < 1 || (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP)) == 0 ){
		/* Missing/Invalid arguments,return immediately */
		return PH7_OK;
	}
	/* Zero the Entry */
	SyZero(&sEntry,sizeof(VmShutdownCB));
	/* Initialize fields */
	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);
	/* Save the callback name for later invocation name */
	PH7_MemObjStore(apArg[0],&sEntry.sCallback); 
	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){
		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);
	}
	/* Copy arguments */
	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){
		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){
			/* Limit reached */
			break;
		}
		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);
	}
	sEntry.nArg = j;
	/* Install the callback */
	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);
	return PH7_OK;
}
/*
 * Section:
 *  Class handling functions.
 * Status:
 *    Stable.
 */
/*
 * Extract the top active class. NULL is returned
 * if the class stack is empty.
 */
PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)
{
	SySet *pSet = &pVm->aSelf;
	ph7_class **apClass;
	if( SySetUsed(pSet) <= 0 ){
		/* Empty stack,return NULL */
		return 0;
	}
	/* Peek the last entry */
	apClass = (ph7_class **)SySetBasePtr(pSet);
	return apClass[pSet->nUsed - 1];
}
/*
 * string get_class ([ object $object = NULL ] )
 *   Returns the name of the class of an object
 * Parameters
 *  object
 *   The tested object. This parameter may be omitted when inside a class.
 * Return
 *  The name of the class of which object is an instance.
 *  Returns FALSE if object is not an object.
 *  If object is omitted when inside a class, the name of that class is returned.
 */
static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass;
	SyString *pName;
	if( nArg < 1 ){
		/* Check if we are inside a class */
		pClass = PH7_VmPeekTopClass(pCtx->pVm);
		if( pClass ){
			/* Point to the class name */
			pName = &pClass->sName;
			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
		}else{
			/* Not inside class,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* Extract the target class */
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			pName = &pClass->sName;
			/* Return the class name */
			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
		}else{
			/* Not a class instance,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}
	return PH7_OK;
}
/*
 * string get_parent_class([object $object = NULL ] )
 *   Returns the name of the parent class of an object
 * Parameters
 *  object
 *   The tested object. This parameter may be omitted when inside a class.
 * Return
 *  The name of the parent class of which object is an instance.
 *  Returns FALSE if object is not an object or if the object does
 *  not have a parent.
 *  If object is omitted when inside a class, the name of that class is returned.
 */
static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass;
	SyString *pName;
	if( nArg < 1 ){
		/* Check if we are inside a class [i.e: a method call]*/
		pClass = PH7_VmPeekTopClass(pCtx->pVm);
		if( pClass && pClass->pBase ){
			/* Point to the class name */
			pName = &pClass->pBase->sName;
			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
		}else{
			/* Not inside class,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* Extract the target class */
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			if( pClass->pBase ){
				pName = &pClass->pBase->sName;
				/* Return the parent class name */
				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
			}else{
				/* Object does not have a parent class */
				ph7_result_bool(pCtx,0);
			}
		}else{
			/* Not a class instance,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}
	return PH7_OK;
}
/*
 * string get_called_class(void)
 *   Gets the name of the class the static method is called in. 
 * Parameters
 *  None.
 * Return
 *  Returns the class name. Returns FALSE if called from outside a class. 
 */
static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass;
	/* Check if we are inside a class [i.e: a method call] */
	pClass = PH7_VmPeekTopClass(pCtx->pVm);
	if( pClass ){
		SyString *pName;
		/* Point to the class name */
		pName = &pClass->sName;
		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
	}else{
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Not inside class,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * Extract a ph7_class from the given ph7_value.
 * The given value must be of type object [i.e: class instance] or
 * string which hold the class name.
 */
static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)
{
	ph7_class *pClass = 0;
	if( ph7_value_is_object(pArg) ){
		/* Class instance already loaded,no need to perform a lookup */
		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;
	}else if( ph7_value_is_string(pArg) ){
		const char *zClass;
		int nLen;
		/* Extract class name */
		zClass = ph7_value_to_string(pArg,&nLen);
		if( nLen > 0 ){
			SyHashEntry *pEntry;
			/* Perform a lookup */
			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);
			if( pEntry ){
				/* Point to the desired class */
				pClass = (ph7_class *)pEntry->pUserData;
			}
		}
	}
	return pClass;
}
/*
 * bool property_exists(mixed $class,string $property)
 *   Checks if the object or class has a property.
 * Parameters
 *  class
 *   The class name or an object of the class to test for
 * property
 *  The name of the property
 * Return
 *   Returns TRUE if the property exists,FALSE otherwise. 
 */
static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume attribute does not exists */
	if( nArg > 1 ){
		ph7_class *pClass;
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			const char *zName; 
			int nLen;
			/* Extract attribute name */
			zName = ph7_value_to_string(apArg[1],&nLen);
			if( nLen > 0 ){
				/* Perform the lookup in the attribute and method table */
				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0 
					|| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){
						/* property exists,flag that */
						res = 1;
				}
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool method_exists(mixed $class,string $method)
 *   Checks if the given method is a class member.
 * Parameters
 *  class
 *   The class name or an object of the class to test for
 * property
 *  The name of the method
 * Return
 *   Returns TRUE if the method exists,FALSE otherwise. 
 */
static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume method does not exists */
	if( nArg > 1 ){
		ph7_class *pClass;
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			const char *zName; 
			int nLen;
			/* Extract method name */
			zName = ph7_value_to_string(apArg[1],&nLen);
			if( nLen > 0 ){
				/* Perform the lookup in the method table */
				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){
					/* method exists,flag that */
					res = 1;
				}
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool class_exists(string $class_name [, bool $autoload = true ] )
 *   Checks if the class has been defined.
 * Parameters
 *  class_name
 *   The class name. The name is matched in a case-sensitive manner 
 *   unlinke the standard PHP engine.
 *  autoload
 *   Whether or not to call __autoload by default.
 * Return
 *   TRUE if class_name is a defined class, FALSE otherwise. 
 */
static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume class does not exists */
	if( nArg > 0 ){
		const char *zName;
		int nLen;
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		/* Perform a hashlookup */
		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){
			/* class is available */
			res = 1;
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool interface_exists(string $class_name [, bool $autoload = true ] )
 *   Checks if the interface has been defined.
 * Parameters
 *  class_name
 *   The class name. The name is matched in a case-sensitive manner 
 *   unlinke the standard PHP engine.
 *  autoload
 *   Whether or not to call __autoload by default.
 * Return
 *   TRUE if class_name is a defined class, FALSE otherwise. 
 */
static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume class does not exists */
	if( nArg > 0 ){
		SyHashEntry *pEntry = 0;
		const char *zName;
		int nLen;
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		/* Perform a hashlookup */
		if( nLen > 0 ){
			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);
		}
		if( pEntry ){
			ph7_class *pClass = (ph7_class *)pEntry->pUserData;
			while( pClass ){
				if( pClass->iFlags & PH7_CLASS_INTERFACE ){
					/* interface is available */
					res = 1;
					break;
				}
				/* Next with the same name */
				pClass = pClass->pNextName;
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool class_alias([string $original[,string $alias ]])
 *   Creates an alias for a class.
 * Parameters
 *  original
 *    The original class.
 *  alias
 *   The alias name for the class.
 * Return
 *   Returns TRUE on success or FALSE on failure. 
 */
static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zOld,*zNew;
	int nOldLen,nNewLen;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	char *zDup;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract old class name */
	zOld = ph7_value_to_string(apArg[0],&nOldLen);
	/* Extract alias name */
	zNew = ph7_value_to_string(apArg[1],&nNewLen);
	if( nNewLen < 1 ){
		/* Invalid alias name,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform a hash lookup */
	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);
	if( pEntry ==  0 ){
		/* No such class,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the class */
	pClass = (ph7_class *)pEntry->pUserData;
	/* Duplicate alias name */
	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);
	if( zDup == 0 ){
		/* Out of memory,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create the alias */
	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);
	if( rc != SXRET_OK ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);
	}
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * array get_declared_classes(void)
 *   Returns an array with the name of the defined classes
 * Parameters
 *  None
 * Return
 *   Returns an array of the names of the declared classes
 *   in the current script. 
 * Note:
 *   NULL is returned on failure.
 */
static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	/* Create a new array first */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined classes */
	SyHashResetLoopCursor(&pCtx->pVm->hClass);
	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){
		ph7_class *pClass = (ph7_class *)pEntry->pUserData;
		/* Do not register classes defined as interfaces */
		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){
			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));
			/* insert class name */
			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array get_declared_interfaces(void)
 *   Returns an array with the name of the defined interfaces
 * Parameters
 *  None
 * Return
 *   Returns an array of the names of the declared interfaces
 *   in the current script. 
 * Note:
 *   NULL is returned on failure.
 */
static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	/* Create a new array first */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined classes */
	SyHashResetLoopCursor(&pCtx->pVm->hClass);
	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){
		ph7_class *pClass = (ph7_class *)pEntry->pUserData;
		/* Register classes defined as interfaces only */
		if( pClass->iFlags & PH7_CLASS_INTERFACE ){
			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));
			/* insert interface name */
			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array get_class_methods(string/object $class_name)
 *   Returns an array with the name of the class methods
 * Parameters
 *  class_name
 *  The class name or class instance
 * Return
 *  Returns an array of method names defined for the class specified by class_name.
 *  In case of an error, it returns NULL.  
 * Note:
 *   NULL is returned on failure.
 */
static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	/* Extract the target class first */
	pClass = 0;
	if( nArg > 0 ){
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
	}
	if( pClass == 0 ){
		/* No such class,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array  */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined methods */
	SyHashResetLoopCursor(&pClass->hMethod);
	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;
		/* Insert method name */
		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));
		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
		/* Reset the cursor */
		ph7_value_reset_string_cursor(pName);
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * This function return TRUE(1) if the given class attribute stored 
 * in the pAttrName parameter is visible and thus can be extracted
 * from the current scope.Otherwise FALSE is returned.
 */
static int VmClassMemberAccess(
	ph7_vm *pVm,               /* Target VM */
	ph7_class *pClass,         /* Target Class */
	const SyString *pAttrName, /* Attribute name */
	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */
	int bLog                   /* TRUE to log forbidden access. */
	)
{
	if( iProtection != PH7_CLASS_PROT_PUBLIC ){
		VmFrame *pFrame = pVm->pFrame;
		ph7_vm_func *pVmFunc;
		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH) ) ){
			/* Safely ignore the exception frame */
			pFrame = pFrame->pParent;
		}
		pVmFunc = (ph7_vm_func *)pFrame->pUserData;
		if( pVmFunc == 0 || (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){
			goto dis; /* Access is forbidden */
		}
		if( iProtection == PH7_CLASS_PROT_PRIVATE ){
			/* Must be the same instance */
			if( (ph7_class *)pVmFunc->pUserData != pClass ){
				goto dis; /* Access is forbidden */
			}
		}else{
			/* Protected */
			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;
			/* Must be a derived class */
			if( !VmInstanceOf(pClass,pBase) ){
				goto dis; /* Access is forbidden */
			}
		}
	}
	return 1; /* Access is granted */
dis:
	if( bLog ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Access to the class attribute '%z->%z' is forbidden",
			&pClass->sName,pAttrName);
	}
	return 0; /* Access is forbidden */
}
/*
 * array get_class_vars(string/object $class_name)
 *   Get the default properties of the class
 * Parameters
 *  class_name
 *   The class name or class instance
 * Return
 *  Returns an associative array of declared properties visible from the current scope
 *  with their default value. The resulting array elements are in the form 
 *  of varname => value.  
 * Note:
 *   NULL is returned on failure.
 */
static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pName,*pArray,sValue;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	/* Extract the target class first */
	pClass = 0;
	if( nArg > 0 ){
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
	}
	if( pClass == 0 ){
		/* No such class,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array  */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	PH7_MemObjInit(pCtx->pVm,&sValue);
	if( pArray == 0 || pName == 0){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined attribute visible from the current scope */
	SyHashResetLoopCursor(&pClass->hAttr);
	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		/* Check if the access is allowed */
		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
			SyString *pAttrName = &pAttr->sName;
			ph7_value *pValue = 0;
			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
				/* Extract static attribute value which is always computed */
				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);
			}else{
				if( SySetUsed(&pAttr->aByteCode) > 0 ){
					PH7_MemObjRelease(&sValue);
					/* Compute default value (any complex expression) associated with this attribute */ 
					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);
					pValue = &sValue;
				}
			}
			/* Fill in the array */
			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);
			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	PH7_MemObjRelease(&sValue);
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * array get_object_vars(object $this)
 *   Gets the properties of the given object
 * Parameters
 *  this
 *   A class instance
 * Return
 *  Returns an associative array of defined object accessible non-static properties
 *  for the specified object in scope. If a property have not been assigned a value
 *  it will be returned with a NULL value.  
 * Note:
 *   NULL is returned on failure.
 */
static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = 0;
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){
		/* Extract the target instance */
		pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	}
	if( pThis == 0 ){
		/* No such instance,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array  */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined attribute visible from the current scope */
	SyHashResetLoopCursor(&pThis->hAttr);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		SyString *pAttrName;
		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
			/* Only non-static/constant attributes are extracted */
			continue;
		}
		pAttrName = &pVmAttr->pAttr->sName;
		/* Check if the access is allowed */
		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){
			ph7_value *pValue = 0;
			/* Extract attribute */
			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
			if( pValue ){
				/* Insert attribute name in the array */
				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);
				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */
			}
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * This function returns TRUE if the given class is an implemented
 * interface.Otherwise FALSE is returned. 
 */
static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)
{
	ph7_class **apInterface;
	sxu32 n;
	if( SySetUsed(pSet) < 1 ){
		/* Empty interface container */
		return FALSE;
	}
	/* Point to the set of implemented interfaces */
	apInterface = (ph7_class **)SySetBasePtr(pSet);
	/* Perform the lookup */
	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){
		if( apInterface[n] == pClass ){
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * This function returns TRUE if the given class (first argument)
 * is an instance of the main class (second argument).
 * Otherwise FALSE is returned. 
 */
static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass) 
{
	ph7_class *pParent;
	sxi32 rc;
	if( pThis == pClass ){
		/* Instance of the same class */
		return TRUE;
	}
	/* Check implemented interfaces */
	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);
	if( rc ){
		return TRUE;
	}
	/* Check parent classes */
	pParent = pThis->pBase;
	while( pParent ){
		if( pParent == pClass ){
			/* Same instance */
			return TRUE;
		}
		/* Check the implemented interfaces */
		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);
		if( rc ){
			return TRUE;
		}
		/* Point to the parent class */
		pParent = pParent->pBase;
	}
	/* Not an instance of the the given class */
	return FALSE;
}
/*
 * This function returns TRUE if the given class (first argument)
 * is a subclass of the main class (second argument).
 * Otherwise FALSE is returned. 
 */
static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)
{
	SySet *pInterface = &pClass->aInterface;
	SyHashEntry *pEntry;
	SyString *pName;
	sxi32 rc;
	while( pClass ){
		pName = &pClass->sName;
		/* Query the derived hashtable */
		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);
		if( pEntry ){
			return TRUE;
		}
		pClass = pClass->pBase;
	}
	rc = VmQueryInterfaceSet(pBase,pInterface);
	if( rc ){
		return TRUE;
	}
	/* Not a subclass */
	return FALSE;
}
/*
 * bool is_a(object $object,string $class_name)
 *   Checks if the object is of this class or has this class as one of its parents.
 * Parameters
 *  object
 *   The tested object
 * class_name
 *  The class name
 * Return
 *   Returns TRUE if the object is of this class or has this class as one of its
 *   parents, FALSE otherwise. 
 */
static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume FALSE by default */
	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){
		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;
		ph7_class *pClass;
		/* Extract the given class */
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);
		if( pClass ){
			/* Perform the query */
			res = VmInstanceOf(pThis->pClass,pClass);
		}
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_subclass_of(object/string $object,object/string $class_name)
 *   Checks if the object has this class as one of its parents.
 * Parameters
 *  object
 *   The tested object
 * class_name
 *  The class name
 * Return
 *  This function returns TRUE if the object , belongs to a class
 *  which is a subclass of class_name, FALSE otherwise. 
 */
static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume FALSE by default */
	if( nArg > 1 ){
		ph7_class *pClass,*pMain;
		/* Extract the given classes */
		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);
		if( pClass && pMain ){
			/* Perform the query */
			res = VmSubclassOf(pClass,pMain);
		}
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * Call a class method where the name of the method is stored in the pMethod
 * parameter and the given arguments are stored in the apArg[] array.
 * Return SXRET_OK if the method was successfuly called.Any other
 * return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_VmCallClassMethod(
	ph7_vm *pVm,               /* Target VM */
	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/
	ph7_class_method *pMethod, /* Method name */
	ph7_value *pResult,        /* Store method return value here. NULL otherwise */
	int nArg,                  /* Total number of given arguments */
	ph7_value **apArg          /* Method arguments */
	)
{
	ph7_value *aStack;
	VmInstr aInstr[2];
	int iCursor;
	int i;
	/* Create a new operand stack */
	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);
	if( aStack == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"PH7 is running out of memory while invoking class method");
		return SXERR_MEM;
	}
	/* Fill the operand stack with the given arguments */
	for( i = 0 ; i < nArg ; i++ ){
		PH7_MemObjLoad(apArg[i],&aStack[i]);
		/*
		 * Symisc eXtension:
		 *  Parameters to [call_user_func()] can be passed by reference.
		 */
		aStack[i].nIdx = apArg[i]->nIdx;
	}
	iCursor = nArg + 1;
	if( pThis ){
		/* 
		 * Push the class instance so that the '$this' variable will be available.
		 */
		pThis->iRef++; /* Increment reference count */
		aStack[i].x.pOther = pThis; 
		aStack[i].iFlags = MEMOBJ_OBJ;
	}
	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */
	i++;
	/* Push method name */
	SyBlobReset(&aStack[i].sBlob);
	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));
	aStack[i].iFlags = MEMOBJ_STRING;
	aStack[i].nIdx = SXU32_HIGH;
	/* Emit the CALL istruction */
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg; /* Total number of given arguments */
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = 0;
	/* Emit the DONE instruction */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;   /* Extract method return value */
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	/* Execute the method body (if available) */
	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);
	/* Clean up the mess left behind */
	SyMemBackendFree(&pVm->sAllocator,aStack);
	return PH7_OK;
}
/*
 * Call a user defined or foreign function where the name of the function
 * is stored in the pFunc parameter and the given arguments are stored
 * in the apArg[] array.
 * Return SXRET_OK if the function was successfuly called.Any other
 * return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_VmCallUserFunction(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	int nArg,          /* Total number of given arguments */
	ph7_value **apArg, /* Callback arguments */
	ph7_value *pResult /* Store callback return value here. NULL otherwise */
	)
{
	ph7_value *aStack;
	VmInstr aInstr[2];
	int i;
	if((pFunc->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP)) == 0 ){
		/* Don't bother processing,it's invalid anyway */
		if( pResult ){
			/* Assume a null return value */
			PH7_MemObjRelease(pResult);
		}
		return SXERR_INVALID;
	}
	if( pFunc->iFlags & MEMOBJ_HASHMAP ){
		/* Class method */
		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;
		ph7_class_method *pMethod = 0;
		ph7_class_instance *pThis = 0;
		ph7_class *pClass = 0;
		ph7_value *pValue;
		sxi32 rc;
		if( pMap->nEntry < 2 /* Class name/instance + method name */){
			/* Empty hashmap,nothing to call */
			if( pResult ){
				/* Assume a null return value */
				PH7_MemObjRelease(pResult);
			}
			return SXRET_OK;
		}
		/* Extract the class name or an instance of it */
		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);
		if( pValue ){
			pClass = VmExtractClassFromValue(&(*pVm),pValue);
		}
		if( pClass == 0 ){
			/* No such class,return NULL */
			if( pResult ){
				PH7_MemObjRelease(pResult);
			}
			return SXRET_OK;
		}
		if( pValue->iFlags & MEMOBJ_OBJ ){
			/* Point to the class instance */
			pThis = (ph7_class_instance *)pValue->x.pOther;
		}
		/* Try to extract the method */
		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);
		if( pValue ){
			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){
				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),
					SyBlobLength(&pValue->sBlob));
			}
		}		
		if( pMethod == 0 ){
			/* No such method,return NULL */
			if( pResult ){
				PH7_MemObjRelease(pResult);
			}
			return SXRET_OK;
		}
		/* Call the class method */
		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);
		return rc;
	}
	/* Create a new operand stack */
	aStack = VmNewOperandStack(&(*pVm),1+nArg);
	if( aStack == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"PH7 is running out of memory while invoking user callback");
		if( pResult ){
			/* Assume a null return value */
			PH7_MemObjRelease(pResult);
		}
		return SXERR_MEM;
	}
	/* Fill the operand stack with the given arguments */
	for( i = 0 ; i < nArg ; i++ ){
		PH7_MemObjLoad(apArg[i],&aStack[i]);
		/* 
		 * Symisc eXtension:
		 *  Parameters to [call_user_func()] can be passed by reference.
		 */
		aStack[i].nIdx = apArg[i]->nIdx;
	}
	/* Push the function name */
	PH7_MemObjLoad(pFunc,&aStack[i]);
	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */
	/* Emit the CALL istruction */
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg; /* Total number of given arguments */
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = 0;
	/* Emit the DONE instruction */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;   /* Extract function return value if available */
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	/* Execute the function body (if available) */
	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);
	/* Clean up the mess left behind */
	SyMemBackendFree(&pVm->sAllocator,aStack);
	return PH7_OK;
}
/*
 * Call a user defined or foreign function whith a varibale number
 * of arguments where the name of the function is stored in the pFunc
 * parameter.
 * Return SXRET_OK if the function was successfuly called.Any other
 * return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	ph7_value *pResult,/* Store callback return value here. NULL otherwise */
	...                /* 0 (Zero) or more Callback arguments */ 
	)
{
	ph7_value *pArg;
	SySet aArg;
	va_list ap;
	sxi32 rc;
	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
	/* Copy arguments one after one */
	va_start(ap,pResult);
	for(;;){
		pArg = va_arg(ap,ph7_value *);
		if( pArg == 0 ){
			break;
		}
		SySetPut(&aArg,(const void *)&pArg);
	}
	/* Call the core routine */
	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);
	/* Cleanup */
	SySetRelease(&aArg);
	return rc;
}
/*
 * value call_user_func(callable $callback[,value $parameter[, value $... ]])
 *  Call the callback given by the first parameter.
 * Parameter
 *  $callback
 *   The callable to be called.
 *  ...
 *    Zero or more parameters to be passed to the callback. 
 * Return
 *  Th return value of the callback, or FALSE on error. 
 */
static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value sResult; /* Store callback return value here */
	sxi32 rc;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Try to invoke the callback */
	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);
	if( rc != SXRET_OK ){
		/* An error occured while invoking the given callback [i.e: not defined] */
		ph7_result_bool(pCtx,0); /* return false */
	}else{
		/* Callback result */
		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */
	}
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * value call_user_func_array(callable $callback,array $param_arr)
 *  Call a callback with an array of parameters.
 * Parameter
 *  $callback
 *   The callable to be called.
 * $param_arr
 *  The parameters to be passed to the callback, as an indexed array.
 * Return
 *  Returns the return value of the callback, or FALSE on error.
 */
static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry; /* Current hashmap entry */
	ph7_value *pValue,sResult;/* Store callback return value here */
	ph7_hashmap *pMap;        /* Target hashmap */
	SySet aArg;               /* Arguments containers */
	sxi32 rc;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Initialize the arguments container */
	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));
	/* Turn hashmap entries into callback arguments */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	pEntry = pMap->pFirst; /* First inserted entry */
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract node value */
		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){
			SySetPut(&aArg,(const void *)&pValue);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Try to invoke the callback */
	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);
	if( rc != SXRET_OK ){
		/* An error occured while invoking the given callback [i.e: not defined] */
		ph7_result_bool(pCtx,0); /* return false */
	}else{
		/* Callback result */
		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */
	}
	/* Cleanup the mess left behind */
	PH7_MemObjRelease(&sResult);
	SySetRelease(&aArg);
	return PH7_OK;
}
/*
 * bool defined(string $name)
 *  Checks whether a given named constant exists.
 * Parameter:
 *  Name of the desired constant.
 * Return
 *  TRUE if the given constant exists.FALSE otherwise.
 */
static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	int nLen = 0;
	int res = 0;
	if( nArg < 1 ){
		/* Missing constant name,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	/* Perform the lookup */
	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){
		/* Already defined */
		res = 1;
	}
	ph7_result_bool(pCtx,res);
	return SXRET_OK;
}
/*
 * Constant expansion callback used by the [define()] function defined
 * below.
 */
static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)
{
	ph7_value *pConstantValue = (ph7_value *)pUserData;
	/* Expand constant value */
	PH7_MemObjStore(pConstantValue,pVal);
}
/*
 * bool define(string $constant_name,expression value)
 *  Defines a named constant at runtime.
 * Parameter:
 *  $constant_name
 *   The name of the constant
 *  $value
 *   Constant value
 * Return:
 *   TRUE on success,FALSE on failure.
 */
static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;  /* Constant name */
	ph7_value *pValue;  /* Duplicated constant value */
	int nLen = 0;       /* Name length */
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,throw a ntoice and return false */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	if( !ph7_value_is_string(apArg[0]) ){
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Duplicate constant value */
	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));
	if( pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Initialize the memory object */
	PH7_MemObjInit(pCtx->pVm,pValue);
	/* Register the constant */
	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);
	if( rc != SXRET_OK ){
		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Duplicate constant value */
	PH7_MemObjStore(apArg[1],pValue);
	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){
		/* Lower case the constant name */
		char *zCur = (char *)zName;
		while( zCur < &zName[nLen] ){
			if( (unsigned char)zCur[0] >= 0xc0 ){
				/* UTF-8 stream */
				zCur++;
				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){
					zCur++;
				}
				continue;
			}
			if( SyisUpper(zCur[0]) ){
				int c = SyToLower(zCur[0]);
				zCur[0] = (char)c;
			}
			zCur++;
		}
		/* Finally,register the constant */
		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * value constant(string $name)
 *  Returns the value of a constant
 * Parameter
 *  $name
 *    Name of the constant.
 * Return
 *  Constant value or NULL if not defined.
 */
static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyHashEntry *pEntry;
	ph7_constant *pCons;
	const char *zName; /* Constant name */
	ph7_value sVal;    /* Constant value */
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Invallid argument,return NULL */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Extract the constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	/* Perform the query */
	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);
	if( pEntry == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sVal);
	/* Point to the structure that describe the constant */
	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);
	/* Extract constant value by calling it's associated callback */
	pCons->xExpand(&sVal,pCons->pUserData);
	/* Return that value */
	ph7_result_value(pCtx,&sVal);
	/* Cleanup */
	PH7_MemObjRelease(&sVal);
	return SXRET_OK;
}
/*
 * Hash walker callback used by the [get_defined_constants()] function
 * defined below.
 */
static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_value sName;
	sxi32 rc;
	/* Prepare the constant name for insertion */
	PH7_MemObjInitFromString(pArray->pVm,&sName,0);
	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);
	/* Perform the insertion */
	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */
	PH7_MemObjRelease(&sName);
	return rc;
}
/*
 * array get_defined_constants(void)
 *  Returns an associative array with the names of all defined
 *  constants.
 * Parameters
 *  NONE.
 * Returns
 *  Returns the names of all the constants currently defined.
 */
static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	/* Create the array first*/
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Fill the array with the defined constants */
	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * Section:
 *  Output Control (OB) functions.
 * Status:
 *    Stable.
 */
/* Forward declaration */	
static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);												
/*
 * void ob_clean(void)
 *  This function discards the contents of the output buffer.
 *  This function does not destroy the output buffer like ob_end_clean() does.
 * Parameter
 *  None
 * Return
 *  No value is returned.
 */
static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb ){
		SyBlobRelease(&pOb->sOB);
	}
	return PH7_OK;
}
/*
 * bool ob_end_clean(void)
 *  Clean (erase) the output buffer and turn off output buffering
 *  This function discards the contents of the topmost output buffer and turns
 *  off this output buffering. If you want to further process the buffer's contents
 *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents
 *  are discarded when ob_end_clean() is called. 
 * Parameter
 *  None
 * Return
 *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called
 *  the function without an active buffer or that for some reason a buffer could not be deleted
 * (possible for special buffer)
 */
static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	/* Pop the top most OB */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0){
		/* No such OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Release */
		VmObRestore(pVm,pOb);
		/* Return true */
		ph7_result_bool(pCtx,1);
	}
	return PH7_OK;
}
/*
 * string ob_get_contents(void)
 *  Gets the contents of the output buffer without clearing it.
 * Parameter
 *  None
 * Return
 *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.
 */
static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb == 0 ){
		/* No active OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Return contents */
		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));
	}	
	return PH7_OK;
}
/*
 * string ob_get_clean(void)
 * string ob_get_flush(void)
 *  Get current buffer contents and delete current output buffer.
 * Parameter
 *  None
 * Return
 *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.
 */
static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	/* Pop the top most OB */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0 ){
		/* No active OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Return contents */
		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */
		/* Release */
		VmObRestore(pVm,pOb);
	}	
	return PH7_OK;
}
/*
 * int ob_get_length(void)
 *  Return the length of the output buffer.
 * Parameter
 *  None
 * Return
 *  Returns the length of the output buffer contents or FALSE if no buffering is active.
 */
static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb == 0 ){
		/* No active OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Return OB length */
		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));
	}	
	return PH7_OK;
}
/*
 * int ob_get_level(void)
 *  Returns the nesting level of the output buffering mechanism.
 * Parameter
 *  None
 * Return
 *  Returns the level of nested output buffering handlers or zero if output buffering is not active. 
 */
static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int iNest;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Nesting level */
	iNest = (int)SySetUsed(&pVm->aOB);
	/* Return the nesting value */
	ph7_result_int(pCtx,iNest);
	return PH7_OK;
}
/*
 * Output Buffer(OB) default VM consumer routine.All VM output is now redirected
 * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].
 * Refer to the implementation of [ob_start()] for more information.
 */
static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	VmObEntry *pEntry;
	ph7_value sResult;
	/* Peek the top most entry */
	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pEntry == 0 ){
		/* CAN'T HAPPEN */
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sResult);
	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){
		ph7_value sArg,*apArg[2];
		/* Fill the first argument */
		PH7_MemObjInitFromString(pVm,&sArg,0);
		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);
		apArg[0] = &sArg;
		/* Call the 'filter' callback */
		pVm->nObDepth++;
		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);
		pVm->nObDepth--;
		if( sResult.iFlags & MEMOBJ_STRING ){
			/* Extract the function result */
			pData = SyBlobData(&sResult.sBlob);
			nDataLen = SyBlobLength(&sResult.sBlob);
		}
		PH7_MemObjRelease(&sArg);
	}
	if( nDataLen > 0 ){
		/* Redirect the VM output to the internal buffer */
		SyBlobAppend(&pEntry->sOB,pData,nDataLen);
	}
	/* Release */
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Restore the default consumer.
 * Refer to the implementation of [ob_end_clean()] for more
 * information.
 */
static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)
{
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	if( SySetUsed(&pVm->aOB) < 1 ){
		/* No more stackable OB */
		pCons->xConsumer = pCons->xDef;
		pCons->pUserData = pCons->pDefData;
	}
	/* Release OB data */
	PH7_MemObjRelease(&pEntry->sCallback);
	SyBlobRelease(&pEntry->sOB);
}
/*
 * bool ob_start([ callback $output_callback] )
 * This function will turn output buffering on. While output buffering is active no output
 *  is sent from the script (other than headers), instead the output is stored in an internal
 *  buffer. 
 * Parameter
 *  $output_callback
 *   An optional output_callback function may be specified. This function takes a string 
 *   as a parameter and should return a string. The function will be called when the output
 *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)
 *   or when the output buffer is flushed to the browser at the end of the request.
 *   When output_callback is called, it will receive the contents of the output buffer
 *   as its parameter and is expected to return a new output buffer as a result, which will
 *   be sent to the browser. If the output_callback is not a callable function, this function
 *   will return FALSE.
 *   If the callback function has two parameters, the second parameter is filled with
 *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT 
 *   and PHP_OUTPUT_HANDLER_END.
 *   If output_callback returns FALSE original input is sent to the browser.
 *   The output_callback parameter may be bypassed by passing a NULL value. 
 * Return
 *   Returns TRUE on success or FALSE on failure.
 */
static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry sOb;
	sxi32 rc;
	/* Initialize the OB entry */
	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);
	SyBlobInit(&sOb.sOB,&pVm->sAllocator);
	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP)) ){
		/* Save the callback name for later invocation */
		PH7_MemObjStore(apArg[0],&sOb.sCallback);
	}
	/* Push in the stack */
	rc = SySetPut(&pVm->aOB,(const void *)&sOb);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sOb.sCallback);
	}else{
		ph7_output_consumer *pCons = &pVm->sVmConsumer;
		/* Substitute the default VM consumer */
		if( pCons->xConsumer != VmObConsumer ){
			pCons->xDef = pCons->xConsumer;
			pCons->pDefData = pCons->pUserData;
			/* Install the new consumer */
			pCons->xConsumer = VmObConsumer;
			pCons->pUserData = pVm;
		}
	}
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * Flush Output buffer to the default VM output consumer.
 * Refer to the implementation of [ob_flush()] for more
 * information.
 */
static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)
{
	SyBlob *pBlob = &pEntry->sOB;
	sxi32 rc;
	/* Flush contents */
	rc = PH7_OK;
	if( SyBlobLength(pBlob) > 0 ){
		/* Call the VM output consumer */
		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);
		/* Increment VM output counter */
		pVm->nOutputLen += SyBlobLength(pBlob);
		if( rc != PH7_ABORT ){
			rc = PH7_OK;
		}
	}
	if( bRelease ){
		VmObRestore(&(*pVm),pEntry);
	}else{
		/* Reset the blob */
		SyBlobReset(pBlob);
	}
	return rc;
}
/*
 * void ob_flush(void)
 * void flush(void)
 *  Flush (send) the output buffer.
 * Parameter
 *  None
 * Return
 *  No return value.
 */
static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	sxi32 rc;
	/* Peek the top most OB entry */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb == 0 ){
		/* Empty stack,return immediately */
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Flush contents */
	rc = VmObFlush(pVm,pOb,FALSE);
	return rc;
}
/*
 * bool ob_end_flush(void)
 *  Flush (send) the output buffer and turn off output buffering.
 * Parameter
 *  None
 * Return
 *  Returns TRUE on success or FALSE on failure. Reasons for failure are first
 *  that you called the function without an active buffer or that for some reason
 *  a buffer could not be deleted (possible for special buffer). 
 */
static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	sxi32 rc;
	/* Pop the top most OB entry */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0 ){
		/* Empty stack,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Flush contents */
	rc = VmObFlush(pVm,pOb,TRUE);
	/* Return true */
	ph7_result_bool(pCtx,1);
	return rc;
}
/*
 * void ob_implicit_flush([int $flag = true ])
 *  ob_implicit_flush() will turn implicit flushing on or off.
 *  Implicit flushing will result in a flush operation after every
 *  output call, so that explicit calls to flush() will no longer be needed. 
 * Parameter
 *  $flag
 *   TRUE to turn implicit flushing on, FALSE otherwise.
 * Return
 *   Nothing
 */
static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* NOTE: As of this version,this function is a no-op.
	 * PH7 is smart enough to flush it's internal buffer when appropriate.
	 */
	SXUNUSED(pCtx);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * array ob_list_handlers(void)
 *  Lists all output handlers in use.
 * Parameter
 *  None
 * Return
 *  This will return an array with the output handlers in use (if any).
 */
static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;	
	ph7_value *pArray;
	VmObEntry *aEntry;
	ph7_value sVal;
	sxu32 n;
	if( SySetUsed(&pVm->aOB) < 1 ){
		/* Empty stack,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Out of memory,return NULL */
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sVal);
	/* Point to the installed OB entries */
	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);
	/* Perform the requested operation */
	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){
		VmObEntry *pEntry = &aEntry[n];
		/* Extract handler name */
		SyBlobReset(&sVal.sBlob);
		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){
			/* Callback,dup it's name */
			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);
		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){
			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);
		}else{
			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);
		}
		sVal.iFlags = MEMOBJ_STRING;
		/* Perform the insertion */
		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);
	}
	PH7_MemObjRelease(&sVal);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Section:
 *  Random numbers/string generators.
 * Status:
 *    Stable.
 */
/*
 * Generate a random 32-bit unsigned integer.
 * PH7 use it's own private PRNG which is based on the one
 * used by te SQLite3 library.
 */
PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)
{
	sxu32 iNum;
	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));
	return iNum;
}
/*
 * Generate a random string (English Alphabet) of length nLen.
 * Note that the generated string is NOT null terminated.
 * PH7 use it's own private PRNG which is based on the one used
 * by te SQLite3 library.
 */
PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)
{
	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */
	int i;
	/* Generate a binary string first */
	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);
	/* Turn the binary string into english based alphabet */
	for( i = 0 ; i < nLen ; ++i ){
		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];
	 }
}
/*
 * int rand()
 * int mt_rand() 
 * int rand(int $min,int $max)
 * int mt_rand(int $min,int $max)
 *  Generate a random (unsigned 32-bit) integer.
 * Parameter
 *  $min
 *    The lowest value to return (default: 0)
 *  $max
 *   The highest value to return (default: getrandmax())
 * Return
 *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).
 * Note:
 *  PH7 use it's own private PRNG which is based on the one used
 *  by te SQLite3 library.
 */
static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxu32 iNum;
	/* Generate the random number */
	iNum = PH7_VmRandomNum(pCtx->pVm);
	if( nArg > 1 ){
		sxu32 iMin,iMax;
		iMin = (sxu32)ph7_value_to_int(apArg[0]);
		iMax = (sxu32)ph7_value_to_int(apArg[1]);
		if( iMin < iMax ){
			sxu32 iDiv = iMax+1-iMin;
			if( iDiv > 0 ){
				iNum = (iNum % iDiv)+iMin;
			}
		}else if(iMax > 0 ){
			iNum %= iMax;
		}
	}
	/* Return the number */
	ph7_result_int64(pCtx,(ph7_int64)iNum);
	return SXRET_OK;
}
/*
 * int getrandmax(void)
 * int mt_getrandmax(void)
 * int rc4_getrandmax(void)
 *   Show largest possible random value
 * Return
 *  The largest possible random value returned by rand() which is in
 *  this implementation 0xFFFFFFFF.
 * Note:
 *  PH7 use it's own private PRNG which is based on the one used
 *  by te SQLite3 library.
 */
static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,SXU32_HIGH);
	return SXRET_OK;
}
/*
 * string rand_str()
 * string rand_str(int $len)
 *  Generate a random string (English alphabet).
 * Parameter
 *  $len
 *    Length of the desired string (default: 16,Min: 1,Max: 1024)
 * Return
 *   A pseudo random string.
 * Note:
 *  PH7 use it's own private PRNG which is based on the one used
 *  by te SQLite3 library.
 *  This function is a symisc extension.
 */
static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	char zString[1024];
	int iLen = 0x10;
	if( nArg > 0 ){
		/* Get the desired length */
		iLen = ph7_value_to_int(apArg[0]);
		if( iLen < 1 || iLen > 1024 ){
			/* Default length */
			iLen = 0x10;
		}
	}
	/* Generate the random string */
	PH7_VmRandomString(pCtx->pVm,zString,iLen);
	/* Return the generated string */
	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */
	return SXRET_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#if !defined(PH7_DISABLE_HASH_FUNC)
/* Unique ID private data */
struct unique_id_data
{
	ph7_context *pCtx; /* Call context */
	int entropy;       /* TRUE if the more_entropy flag is set */
};
/*
 * Binary to hex consumer callback.
 * This callback is the default consumer used by [uniqid()] function
 * defined below.
 */
static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;
	sxu32 nBuflen;
	/* Extract result buffer length */
	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);
	if( nBuflen > 12 && !pUniq->entropy ){
			/* 
			 * If the more_entropy flag is not set,then the returned
			 * string will be 13 characters long
			 */
		return SXERR_ABORT;
	}
	if( nBuflen > 22 ){
		return SXERR_ABORT;
	}
	/* Safely Consume the hex stream */
	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string uniqid([string $prefix = "" [, bool $more_entropy = false]])
 *  Generate a unique ID
 * Parameter
 * $prefix
 *  Append this prefix to the generated unique ID.
 *  With an empty prefix, the returned string will be 13 characters long.
 *  If more_entropy is TRUE, it will be 23 characters.
 * $more_entropy
 *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood 
 *  that the result will be unique.
 * Return
 *  Returns the unique identifier, as a string.
 */
static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct unique_id_data sUniq;
	unsigned char zDigest[20]; 
	ph7_vm *pVm = pCtx->pVm;
	const char *zPrefix;
	SHA1Context sCtx;
	char zRandom[7];
	int nPrefix;
	int entropy;
	/* Generate a random string first */
	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));
	/* Initialize fields */
	zPrefix = 0;
	nPrefix = 0;
	entropy = 0;
	if( nArg > 0 ){
		/* Append this prefix to the generated unqiue ID */
		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);
		if( nArg > 1 ){
			entropy = ph7_value_to_bool(apArg[1]);
		}
	}
	SHA1Init(&sCtx);
	/* Generate the random ID */
	if( nPrefix > 0 ){
		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);
	}
	/* Append the random ID */
	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));
	/* Append the random string */
	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));
	/* Increment the number */
	pVm->unique_id++;
	SHA1Final(&sCtx,zDigest);
	/* Hexify the digest */
	sUniq.pCtx = pCtx;
	sUniq.entropy = entropy;
	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);
	/* All done */
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Section:
 *  Language construct implementation as foreign functions.
 * Status:
 *    Stable.
 */
/*
 * void echo($string...)
 *  Output one or more messages.
 * Parameters
 *  $string
 *   Message to output.
 * Return
 *  NULL.
 */
static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData;
	int nDataLen = 0;
	ph7_vm *pVm;
	int i,rc;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Output */
	for( i = 0 ; i < nArg ; ++i ){
		zData = ph7_value_to_string(apArg[i],&nDataLen);
		if( nDataLen > 0 ){
			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);
			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){
				/* Increment output length */
				pVm->nOutputLen += nDataLen;
			}
			if( rc == SXERR_ABORT ){
				/* Output consumer callback request an operation abort */
				return PH7_ABORT;
			}
		}
	}
	return SXRET_OK;
}
/*
 * int print($string...)
 *  Output one or more messages.
 * Parameters
 *  $string
 *   Message to output.
 * Return
 *  1 always.
 */
static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData;
	int nDataLen = 0;
	ph7_vm *pVm;
	int i,rc;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Output */
	for( i = 0 ; i < nArg ; ++i ){
		zData = ph7_value_to_string(apArg[i],&nDataLen);
		if( nDataLen > 0 ){
			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);
			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){
				/* Increment output length */
				pVm->nOutputLen += nDataLen;
			}
			if( rc == SXERR_ABORT ){
				/* Output consumer callback request an operation abort */
				return PH7_ABORT;
			}
		}
	}
	/* Return 1 */
	ph7_result_int(pCtx,1);
	return SXRET_OK;
}
/*
 * void exit(string $msg)
 * void exit(int $status)
 * void die(string $ms)
 * void die(int $status)
 *   Output a message and terminate program execution.
 * Parameter
 *  If status is a string, this function prints the status just before exiting.
 *  If status is an integer, that value will be used as the exit status 
 *  and not printed
 * Return
 *  NULL
 */
static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){
		if( ph7_value_is_string(apArg[0]) ){
			const char *zData;
			int iLen = 0;
			/* Print exit message */
			zData = ph7_value_to_string(apArg[0],&iLen);
			ph7_context_output(pCtx,zData,iLen);
		}else if(ph7_value_is_int(apArg[0]) ){
			sxi32 iExitStatus;
			/* Record exit status code */
			iExitStatus = ph7_value_to_int(apArg[0]);
			pCtx->pVm->iExitStatus = iExitStatus;
		}
	}
	/* Check if we are in an included file */
	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){
		/* Exit the entire process */
		exit(pCtx->pVm->iExitStatus);
	}
	/* Abort processing immediately */
	return PH7_ABORT;
}
/*
 * bool isset($var,...)
 *  Finds out whether a variable is set.
 * Parameters
 *  One or more variable to check.
 * Return
 *  1 if var exists and has value other than NULL, 0 otherwise.
 */
static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	int res = 0;
	int i;
	if( nArg < 1 ){
		/* Missing arguments,return false */
		ph7_result_bool(pCtx,res);
		return SXRET_OK;
	}
	/* Iterate over available arguments */
	for( i = 0 ; i < nArg ; ++i ){
		pObj = apArg[i];
		if( pObj->nIdx == SXU32_HIGH ){
			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){
				/* Not so fatal,Throw a warning */
				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");
			}
		}
		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;
		if( !res ){
			/* Variable not set,return FALSE */
			ph7_result_bool(pCtx,0);
			return SXRET_OK;
		}
	}
	/* All given variable are set,return TRUE */
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * Unset a memory object [i.e: a ph7_value],remove it from the current
 * frame,the reference table and discard it's contents.
 * This function never fail and always return SXRET_OK.
 */
PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)
{
	ph7_value *pObj;
	VmRefObj *pRef;
	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);
	if( pObj ){
		/* Release the object */
		PH7_MemObjRelease(pObj);
	}
	/* Remove old reference links */
	pRef = VmRefObjExtract(&(*pVm),nObjIdx);
	if( pRef ){
		sxi32 iFlags = pRef->iFlags;
		/* Unlink from the reference table */
		VmRefObjUnlink(&(*pVm),pRef);
		if( (bForce == TRUE) || (iFlags & VM_REF_IDX_KEEP) == 0 ){
			VmSlot sFree;	
			/* Restore to the free list */
			sFree.nIdx = nObjIdx;
			sFree.pUserData = 0;
			SySetPut(&pVm->aFreeObj,(const void *)&sFree);
		}
	}	
	return SXRET_OK;
}
/*
 * void unset($var,...)
 *   Unset one or more given variable.
 * Parameters
 *  One or more variable to unset.
 * Return
 *  Nothing.
 */
static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	ph7_vm *pVm;
	int i;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Iterate and unset */
	for( i = 0 ; i < nArg ; ++i ){
		pObj = apArg[i];
		if( pObj->nIdx == SXU32_HIGH ){
			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){
				/* Throw an error */
				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");
			}
		}else{
			sxu32 nIdx = pObj->nIdx;
			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */
			if( nIdx != pVm->nGlobalIdx ){				
				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);
			}
		}
	}
	return SXRET_OK;
}
/*
 * Hash walker callback used by the [get_defined_vars()] function.
 */
static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_vm *pVm = pArray->pVm;
	ph7_value *pObj;
	sxu32 nIdx;
	/* Extract the memory object */
	nIdx = SX_PTR_TO_INT(pEntry->pUserData);
	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	if( pObj ){
		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 || (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){
			if( pEntry->nKeyLen > 0 ){
				SyString sName;
				ph7_value sKey;
				/* Perform the insertion */
				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);
				PH7_MemObjInitFromString(pVm,&sKey,&sName);
				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);
				PH7_MemObjRelease(&sKey);
			}
		}
	}
	return SXRET_OK;
}
/*
 * array get_defined_vars(void)
 *  Returns an array of all defined variables.
 * Parameter
 *  None
 * Return
 *  An array with all the variables defined in the current scope.
 */
static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
 	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Superglobals first */
	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);
	/* Then variable defined in the current frame */
	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);
	/* Finally,return the created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * bool gettype($var)
 *  Get the type of a variable
 * Parameters
 *   $var
 *    The variable being type checked.
 * Return
 *   String representation of the given variable type.
 */
static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zType = "Empty";
	if( nArg > 0 ){
		zType = PH7_MemObjTypeDump(apArg[0]);
	}
	/* Return the variable type */
	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);
	return SXRET_OK;
}
/*
 * string get_resource_type(resource $handle)
 *  This function gets the type of the given resource.
 * Parameters
 *  $handle
 *  The evaluated resource handle.
 * Return
 *  If the given handle is a resource, this function will return a string 
 *  representing its type. If the type is not identified by this function
 *  the return value will be the string Unknown.
 *  This function will return FALSE and generate an error if handle
 *  is not a resource.
 */
static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);
	return SXRET_OK;
}
/*
 * void var_dump(expression,....)
 *   var_dump � Dumps information about a variable
 * Parameters
 *   One or more expression to dump.
 * Returns
 *  Nothing.
 */
static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyBlob sDump; /* Generated dump is stored here */
	int i;
	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);
	/* Dump one or more expressions */
	for( i = 0 ; i < nArg ; i++ ){
		ph7_value *pObj = apArg[i];
		/* Reset the working buffer */
		SyBlobReset(&sDump);
		/* Dump the given expression */
		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);
		/* Output */
		if( SyBlobLength(&sDump) > 0 ){
			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
		}
	}
	/* Release the working buffer */
	SyBlobRelease(&sDump);
	return SXRET_OK;
}
/*
 * string/bool print_r(expression,[bool $return = FALSE])
 *   print-r - Prints human-readable information about a variable 
 * Parameters
 *   expression: Expression to dump
 *   return : If you would like to capture the output of print_r() use
 *            the return parameter. When this parameter is set to TRUE
 *            print_r() will return the information rather than print it.
 * Return
 *  When the return parameter is TRUE, this function will return a string.
 *  Otherwise, the return value is TRUE.
 */
static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int ret_string = 0;
	SyBlob sDump;
	if( nArg < 1 ){
		/* Nothing to output,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);
	if ( nArg > 1 ){
		/* Where to redirect output */
		ret_string = ph7_value_to_bool(apArg[1]);
	}
	/* Generate dump */
	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);
	if( !ret_string ){
		/* Output dump */
		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
		/* Return true */
		ph7_result_bool(pCtx,1);
	}else{
		/* Generated dump as return value */
		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
	}
	/* Release the working buffer */
	SyBlobRelease(&sDump);
	return SXRET_OK;
}
/*
 * string/null var_export(expression,[bool $return = FALSE])
 * Same job as print_r. (see coment above)
 */
static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int ret_string = 0;
	SyBlob sDump;      /* Dump is stored in this BLOB */
	if( nArg < 1 ){
		/* Nothing to output,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);
	if ( nArg > 1 ){
		/* Where to redirect output */
		ret_string = ph7_value_to_bool(apArg[1]);
	}
	/* Generate dump */
	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);
	if( !ret_string ){
		/* Output dump */
		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
		/* Return NULL */
		ph7_result_null(pCtx);
	}else{
		/* Generated dump as return value */
		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
	}
	/* Release the working buffer */
	SyBlobRelease(&sDump);
	return SXRET_OK;
}
/*
 * int/bool assert_options(int $what [, mixed $value ])
 *  Set/get the various assert flags.
 * Parameter
 * $what
 *   ASSERT_ACTIVE          Enable assert() evaluation
 *   ASSERT_WARNING         Issue a warning for each failed assertion
 *   ASSERT_BAIL            Terminate execution on failed assertions
 *   ASSERT_QUIET_EVAL      Not used
 *   ASSERT_CALLBACK        Callback to call on failed assertions
 * $value
 *   An optional new value for the option.
 * Return
 *  Old setting on success or FALSE on failure. 
 */
static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int iOld,iNew,iValue; 
	if( nArg < 1 || !ph7_value_is_int(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Save old assertion flags */
	iOld = pVm->iAssertFlags;
	/* Extract the new flags */
	iNew = ph7_value_to_int(apArg[0]);
	if( iNew == PH7_ASSERT_DISABLE ){
		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;
		if( nArg > 1 ){
			iValue = !ph7_value_to_bool(apArg[1]);
			if( iValue ){
				/* Disable assertion */
				pVm->iAssertFlags |= PH7_ASSERT_DISABLE;
			}
		}
	}else if( iNew == PH7_ASSERT_WARNING ){
		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;
		if( nArg > 1 ){
			iValue = ph7_value_to_bool(apArg[1]);
			if( iValue ){
				/* Issue a warning for each failed assertion */
				pVm->iAssertFlags |= PH7_ASSERT_WARNING;
			}
		}
	}else if( iNew == PH7_ASSERT_BAIL ){
		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;
		if( nArg > 1 ){
			iValue = ph7_value_to_bool(apArg[1]);
			if( iValue ){
				/* Terminate execution on failed assertions */
				pVm->iAssertFlags |= PH7_ASSERT_BAIL;
			}
		}
	}else if( iNew == PH7_ASSERT_CALLBACK ){
		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Callback to call on failed assertions */
			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);
			pVm->iAssertFlags |= PH7_ASSERT_CALLBACK;
		}
	}
	/* Return the old flags */
	ph7_result_int(pCtx,iOld);
	return PH7_OK;
}
/*
 * bool assert(mixed $assertion)
 *  Checks if assertion is FALSE.
 * Parameter
 *  $assertion
 *    The assertion to test.
 * Return
 *  FALSE if the assertion is false, TRUE otherwise.
 */
static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pAssert;
	int iFlags,iResult; 
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iFlags = pVm->iAssertFlags;
	if( iFlags & PH7_ASSERT_DISABLE ){
		/* Assertion is disabled,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pAssert = apArg[0];
	iResult = 1; /* cc warning */
	if( pAssert->iFlags & MEMOBJ_STRING ){
		SyString sChunk;
		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));
		if( sChunk.nByte > 0 ){
			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY|PH7_PHP_EXPR,FALSE);
			/* Extract evaluation result */
			iResult = ph7_value_to_bool(pCtx->pRet);
		}else{
			iResult = 0;
		}
	}else{
		/* Perform a boolean cast */
		iResult = ph7_value_to_bool(apArg[0]);
	}
	if( !iResult ){
		/* Assertion failed */
		if( iFlags & PH7_ASSERT_CALLBACK ){
			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};
			ph7_value sFile,sLine;
			ph7_value *apCbArg[3];
			SyString *pFile;
			/* Extract the processed script */
			pFile = (SyString *)SySetPeek(&pVm->aFiles);
			if( pFile == 0 ){
				pFile = (SyString *)&sFileName;
			}
			/* Invoke the callback */
			PH7_MemObjInitFromString(pVm,&sFile,pFile);
			PH7_MemObjInitFromInt(pVm,&sLine,0);
			apCbArg[0] = &sFile;
			apCbArg[1] = &sLine;
			apCbArg[2] = pAssert;
			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);
			/* Clean-up the mess left behind */
			PH7_MemObjRelease(&sFile);
			PH7_MemObjRelease(&sLine);
		}
		if( iFlags & PH7_ASSERT_WARNING ){
			/* Emit a warning */
			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");
		}
		if( iFlags & PH7_ASSERT_BAIL ){
			/* Abort VM execution immediately */
			return PH7_ABORT;
		}
	}
	/* Assertion result */
	ph7_result_bool(pCtx,iResult);
	return PH7_OK;
}
/*
 * Section:
 *  Error reporting functions.
 * Status:
 *    Stable.
 */
/*
 * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])
 *  Generates a user-level error/warning/notice message.
 * Parameters
 *  $error_msg
 *   The designated error message for this error. It's limited to 1024 characters
 *   in length. Any additional characters beyond 1024 will be truncated.
 * $error_type
 *  The designated error type for this error. It only works with the E_USER family
 *  of constants, and will default to E_USER_NOTICE.
 * Return
 *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.
 */
static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nErr = PH7_CTX_NOTICE;
	int rc = PH7_OK;
	if( nArg > 0 ){
		const char *zErr;
		int nLen;
		/* Extract the error message */
		zErr = ph7_value_to_string(apArg[0],&nLen);
		if( nArg > 1 ){
			/* Extract the error type */
			nErr = ph7_value_to_int(apArg[1]);
			switch( nErr ){
			case 1:   /* E_ERROR */
			case 16:  /* E_CORE_ERROR */
			case 64:  /* E_COMPILE_ERROR */
			case 256: /* E_USER_ERROR */
				nErr = PH7_CTX_ERR;
				rc = PH7_ABORT; /* Abort processing immediately */
				break;
			case 2:   /* E_WARNING */
			case 32:  /* E_CORE_WARNING */
			case 123: /* E_COMPILE_WARNING */
			case 512: /* E_USER_WARNING */
				nErr = PH7_CTX_WARNING;
				break;
			default:
				nErr = PH7_CTX_NOTICE;
				break;
			}
		}
		/* Report error */
		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);
		if( rc == PH7_ABORT ){
			return rc;
		}
		/* Return true */
		ph7_result_bool(pCtx,1);
	}else{
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return rc;
}
/*
 * int error_reporting([int $level])
 *  Sets which PHP errors are reported.
 * Parameters
 *  $level
 *   The new error_reporting level. It takes on either a bitmask, or named constants.
 *   Using named constants is strongly encouraged to ensure compatibility for future versions.
 *   As error levels are added, the range of integers increases, so older integer-based error
 *   levels will not always behave as expected.
 *   The available error level constants and the actual meanings of these error levels are described
 *   in the predefined constants.
 * Return
 *   Returns the old error_reporting level or the current level if no level
 *   parameter is given. 
 */
static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int nOld;
	/* Extract the old reporting level */
	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;
	if( nArg > 0 ){
		int nNew;
		/* Extract the desired error reporting level */
		nNew = ph7_value_to_int(apArg[0]);
		if( !nNew ){
			/* Do not report errors at all */
			pVm->bErrReport = 0;
		}else{
			/* Report all errors */
			pVm->bErrReport = 1;
		}
	}
	/* Return the old level */
	ph7_result_int(pCtx,nOld);
	return PH7_OK;
}
/*
 * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])
 *  Send an error message somewhere.
 * Parameter
 *  $message
 *   The error message that should be logged.
 *  $message_type
 *   Says where the error should go. The possible message types are as follows:
 *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism
 *       or a file, depending on what the error_log configuration directive is set to.
 *       This is the default option.
 *    1 message is sent by email to the address in the destination parameter.
 *      This is the only message type where the fourth parameter, extra_headers is used.
 *    2  No longer an option.
 *    3  message is appended to the file destination. A newline is not automatically added
 *       to the end of the message string.
 *    4  message is sent directly to the SAPI logging handler.
 *  $destination
 *   The destination. Its meaning depends on the message_type parameter as described above.
 *  $extra_headers
 *   The extra headers. It's used when the message_type parameter is set to 1
 * Return
 *  TRUE on success or FALSE on failure. 
 * NOTE:
 *  Actually,PH7 does not care about the given parameters,all this function does
 *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER
 *  configuration directive (refer to the official documentation for more information).
 *  Otherwise this function is no-op.
 */
static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMessage,*zDest,*zHeader;
	ph7_vm *pVm = pCtx->pVm;
	int iType = 0;
	if( nArg < 1 ){
		/* Missing log message,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pVm->xErrLog  ){
		/* Invoke the user callback */
		zMessage = ph7_value_to_string(apArg[0],0);
		zDest = zHeader = ""; /* Empty string */
		if( nArg > 1 ){
			iType = ph7_value_to_int(apArg[1]);
			if( nArg > 2 ){
				zDest = ph7_value_to_string(apArg[2],0);
				if( nArg > 3 ){
					zHeader = ph7_value_to_string(apArg[3],0);
				}
			}
		}
		pVm->xErrLog(zMessage,iType,zDest,zHeader);
	}
	/* Retun TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool restore_exception_handler(void) 
 *  Restores the previously defined exception handler function.
 * Parameter
 *  None
 * Return
 *  TRUE if the exception handler is restored.FALSE otherwise
 */
static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pOld,*pNew;
	/* Point to the old and the new handler */
	pOld = &pVm->aExceptionCB[0];
	pNew = &pVm->aExceptionCB[1];
	if( pOld->iFlags & MEMOBJ_NULL ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* No installed handler,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Copy the old handler */
	PH7_MemObjStore(pOld,pNew);
	PH7_MemObjRelease(pOld);
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * callable set_exception_handler(callable $exception_handler)
 *  Sets a user-defined exception handler function.
 *  Sets the default exception handler if an exception is not caught within a try/catch block.
 * NOTE
 *  Execution will NOT stop after the exception_handler calls for example die/exit unlike
 *  the satndard PHP engine.
 * Parameters
 *  $exception_handler
 *   Name of the function to be called when an uncaught exception occurs.
 *   This handler function needs to accept one parameter, which will be the exception object
 *   that was thrown.
 *  Note:
 *   NULL may be passed instead, to reset this handler to its default state. 
 * Return
 *  Returns the name of the previously defined exception handler, or NULL on error.
 *  If no previous handler was defined, NULL is also returned. If NULL is passed
 *  resetting the handler to its default state, TRUE is returned. 
 */
static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pOld,*pNew;
	/* Point to the old and the new handler */
	pOld = &pVm->aExceptionCB[0];
	pNew = &pVm->aExceptionCB[1];
	/* Return the old handler */
	ph7_result_value(pCtx,pOld); /* Will make it's own copy */
	if( nArg > 0 ){
		if( !ph7_value_is_callable(apArg[0])) {
			/* Not callable,return TRUE (As requested by the PHP specification) */
			PH7_MemObjRelease(pNew);
			ph7_result_bool(pCtx,1);
		}else{
			PH7_MemObjStore(pNew,pOld);
			/* Install the new handler */
			PH7_MemObjStore(apArg[0],pNew);
		}
	}
	return PH7_OK;
}
/*
 * bool restore_error_handler(void) 
 *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.
 * Parameters:
 *  None.
 * Return
 *  Always TRUE.
 */
static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pOld,*pNew;
	/* Point to the old and the new handler */
	pOld = &pVm->aErrCB[0];
	pNew = &pVm->aErrCB[1];
	if( pOld->iFlags & MEMOBJ_NULL ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* No installed callback,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Copy the old callback */
	PH7_MemObjStore(pOld,pNew);
	PH7_MemObjRelease(pOld);
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * value set_error_handler(callable $error_handler)
 *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.
 *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  Sets a user-defined error handler function.
 *  This function can be used for defining your own way of handling errors during
 *  runtime, for example in applications in which you need to do cleanup of data/files
 *  when a critical error happens, or when you need to trigger an error under certain 
 *  conditions (using trigger_error()). 
 * Parameters
 *  $error_handler
 *   The user function needs to accept two parameters: the error code, and a string
 *   describing the error.
 *   Then there are three optional parameters that may be supplied: the filename in which
 *   the error occurred, the line number in which the error occurred, and the context in which
 *   the error occurred (an array that points to the active symbol table at the point the error occurred).
 *   The function can be shown as:
 *    handler ( int $errno , string $errstr [, string $errfile])
 *     errno
 *       The first parameter, errno, contains the level of the error raised, as an integer. 
 *   errstr
 *      The second parameter, errstr, contains the error message, as a string. 
 *   errfile
 *      The third parameter is optional, errfile, which contains the filename that the error
 *     was raised in, as a string. 
 *  Note:
 *   NULL may be passed instead, to reset this handler to its default state. 
 * Return
 *  Returns the name of the previously defined error handler, or NULL on error.
 *  If no previous handler was defined, NULL is also returned. If NULL is passed
 *  resetting the handler to its default state, TRUE is returned. 
 */
static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pOld,*pNew;
	/* Point to the old and the new handler */
	pOld = &pVm->aErrCB[0];
	pNew = &pVm->aErrCB[1];
	/* Return the old handler */
	ph7_result_value(pCtx,pOld); /* Will make it's own copy */
	if( nArg > 0 ){
		if( !ph7_value_is_callable(apArg[0])) {
			/* Not callable,return TRUE (As requested by the PHP specification) */
			PH7_MemObjRelease(pNew);
			ph7_result_bool(pCtx,1);
		}else{
			PH7_MemObjStore(pNew,pOld);
			/* Install the new handler */
			PH7_MemObjStore(apArg[0],pNew);
		}
	}
	return PH7_OK;
}
/*
 * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )
 *  Generates a backtrace.
 * Paramaeter
 *  $options
 *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.
 *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus
 *   all the function/method arguments, to save memory.
 * $limit
 *   (Not Used)
 * Return
 *  An array.The possible returned elements are as follows:
 *          Possible returned elements from debug_backtrace()
 *          Name        Type      Description
 *          ------      ------     -----------
 *          function    string    The current function name. See also __FUNCTION__.
 *          line        integer   The current line number. See also __LINE__.
 *          file 	    string 	  The current file name. See also __FILE__.
 *          class       string    The current class name. See also __CLASS__
 *          object      object    The current object.
 *          args        array     If inside a function, this lists the functions arguments.
 *                                If inside an included file, this lists the included file name(s). 
 */
static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	ph7_class *pClass;
	ph7_value *pValue;
	SyString *pFile;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		/* Out of memory,return NULL */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Dump running function name and it's arguments  */
	if( pVm->pFrame->pParent ){
		VmFrame *pFrame = pVm->pFrame;
		ph7_vm_func *pFunc;
		ph7_value *pArg;
		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
			/* Safely ignore the exception frame */
			pFrame = pFrame->pParent;
		}
		pFunc = (ph7_vm_func *)pFrame->pUserData;
		if( pFrame->pParent && pFunc ){
			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);
			ph7_array_add_strkey_elem(pArray,"function",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		/* Function arguments */
		pArg = ph7_context_new_array(pCtx);
		if( pArg  ){
			ph7_value *pObj;
			VmSlot *aSlot;
			sxu32 n;
			/* Start filling the array with the given arguments */
			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){
				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);
				if( pObj ){
					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);
				}
			}
			/* Save the array */
			ph7_array_add_strkey_elem(pArray,"args",pArg);
		}
	}
	ph7_value_int(pValue,1);
	/* Append the current line (which is always 1 since PH7 does not track
	 * line numbers at run-time. )
	 */
	ph7_array_add_strkey_elem(pArray,"line",pValue);
	/* Current processed script */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile ){
		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);
		ph7_array_add_strkey_elem(pArray,"file",pValue);
		ph7_value_reset_string_cursor(pValue);
	}
	/* Top class */
	pClass = PH7_VmPeekTopClass(pVm);
	if( pClass ){
		ph7_value_reset_string_cursor(pValue);
		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);
		ph7_array_add_strkey_elem(pArray,"class",pValue);
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory, everything will be released automatically
	 * as soon we return from this function.
	 */
	return PH7_OK;
}
/* 
 * Generate a small backtrace.
 * Store the generated dump in the given BLOB
 */
static int VmMiniBacktrace(
	ph7_vm *pVm, /* Target VM */
	SyBlob *pOut /* Store Dump here */
	)
{
	VmFrame *pFrame = pVm->pFrame;
	ph7_vm_func *pFunc;
	ph7_class *pClass;
	SyString *pFile;
	/* Called function */
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	pFunc = (ph7_vm_func *)pFrame->pUserData;
	SyBlobAppend(pOut,"[",sizeof(char));
	if( pFrame->pParent && pFunc ){
		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);
		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);
	}else{
		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);
	}
	SyBlobAppend(pOut,"]",sizeof(char));
	/* Current processed script */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile ){
		SyBlobAppend(pOut,"[",sizeof(char));
		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);
		SyBlobAppend(pOut,pFile->zString,pFile->nByte);
		SyBlobAppend(pOut,"]",sizeof(char));
	}
	/* Top class */
	pClass = PH7_VmPeekTopClass(pVm);
	if( pClass ){
		SyBlobAppend(pOut,"[",sizeof(char));
		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);
		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);
		SyBlobAppend(pOut,"]",sizeof(char));
	}
	SyBlobAppend(pOut,"\n",sizeof(char));
	/* All done */
	return SXRET_OK;
}
/*
 * void debug_print_backtrace()
 *  Prints a backtrace
 * Parameters
 * None
 * Return
 * NULL
 */
static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sDump;
	SyBlobInit(&sDump,&pVm->sAllocator);
	/* Generate the backtrace */
	VmMiniBacktrace(pVm,&sDump);
	/* Output backtrace */
	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
	/* All done,cleanup */
	SyBlobRelease(&sDump);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * string debug_string_backtrace()
 *  Generate a backtrace
 * Parameters
 * None
 * Return
 *  A mini backtrace().
 * Note that this is a symisc extension.
 */
static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sDump;
	SyBlobInit(&sDump,&pVm->sAllocator);
	/* Generate the backtrace */
	VmMiniBacktrace(pVm,&sDump);
	/* Return the backtrace */
	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */
	/* All done,cleanup */
	SyBlobRelease(&sDump);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * The following routine is invoked by the engine when an uncaught
 * exception is triggered.
 */
static sxi32 VmUncaughtException(
	ph7_vm *pVm, /* Target VM */
	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */
	)
{
	ph7_value *apArg[2],sArg;
	int nArg = 1;
	sxi32 rc;
	if( pVm->nExceptDepth > 15 ){
		/* Nesting limit reached */
		return SXRET_OK;
	}
	/* Call any exception handler if available */
	PH7_MemObjInit(pVm,&sArg);
	if( pThis ){
		/* Load the exception instance */
		sArg.x.pOther = pThis;
		pThis->iRef++;
		MemObjSetType(&sArg,MEMOBJ_OBJ);
	}else{
		nArg = 0;
	}
	apArg[0] = &sArg;
	/* Call the exception handler if available */
	pVm->nExceptDepth++;
	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],1,apArg,0);
	pVm->nExceptDepth--;
	if( rc != SXRET_OK ){
		SyString sName = { "Exception" , sizeof("Exception") - 1 };
		SyString sFuncName = { "Global",sizeof("Global") - 1 };
		VmFrame *pFrame = pVm->pFrame;
		/* No available handler,generate a fatal error */ 
		if( pThis ){
			SyStringDupPtr(&sName,&pThis->pClass->sName);
		}
		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
			/* Ignore exception frames */
			pFrame = pFrame->pParent;
		}
		if( pFrame->pParent ){
			if( pFrame->iFlags & VM_FRAME_CATCH ){
				SyStringInitFromBuf(&sFuncName,"Catch_block",sizeof("Catch_block")-1);
			}else{
				ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;
				if( pFunc ){
					SyStringDupPtr(&sFuncName,&pFunc->sName);
				}
			}
		}
		/* Generate a listing */
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Uncaught exception '%z' in the '%z' frame context",
			&sName,&sFuncName);
		/* Tell the upper layer to stop VM execution immediately  */
		rc = SXERR_ABORT;
	}
	PH7_MemObjRelease(&sArg);
	return rc;
}
/*
 * Throw an user exception.
 */
static sxi32 VmThrowException(
	ph7_vm *pVm,              /* Target VM */
	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */
	)
{
	ph7_exception_block *pCatch; /* Catch block to execute */
	ph7_exception **apException;
	ph7_exception *pException;
	/* Point to the stack of loaded exceptions */
	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);
	pException = 0;
	pCatch = 0;
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception_block *aCatch;
		ph7_class *pClass;
		sxu32 j;
		/* Locate the appropriate block to execute */
		pException = apException[SySetUsed(&pVm->aException) - 1];
		(void)SySetPop(&pVm->aException);
		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);
		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){
			SyString *pName = &aCatch[j].sClass;
			/* Extract the target class */
			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);
			if( pClass == 0 ){
				/* No such class */
				continue;
			}
			if( VmInstanceOf(pThis->pClass,pClass) ){
				/* Catch block found,break immeditaley */
				pCatch = &aCatch[j];
				break;
			}
		}		
	}
	/* Execute the cached block if available */
	if( pCatch == 0 ){
		sxi32 rc;
		rc = VmUncaughtException(&(*pVm),pThis);
		if( rc == SXRET_OK && pException ){
			VmFrame *pFrame = pVm->pFrame;
			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
				/* Safely ignore the exception frame */
				pFrame = pFrame->pParent;
			}
			if( pException->pFrame == pFrame ){
				/* Tell the upper layer that the exception was caught */
				pFrame->iFlags &= ~VM_FRAME_THROW;
			}
		}
		return rc;
	}else{
		VmFrame *pFrame = pVm->pFrame;
		sxi32 rc;
		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
			/* Safely ignore the exception frame */
			pFrame = pFrame->pParent;
		}
		if( pException->pFrame == pFrame ){
			/* Tell the upper layer that the exception was caught */
			pFrame->iFlags &= ~VM_FRAME_THROW;
		}
		/* Create a private frame first */
		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);
		if( rc == SXRET_OK ){
			/* Mark as catch frame */
			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);
			pFrame->iFlags |= VM_FRAME_CATCH;
			if( pObj ){
				/* Install the exception instance */
				pThis->iRef++; /* Increment reference count */
				pObj->x.pOther = pThis;
				MemObjSetType(pObj,MEMOBJ_OBJ);
			}
			/* Exceute the block */
			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);
			/* Leave the frame */
			VmLeaveFrame(&(*pVm));
		}		
	}
	/* TICKET 1433-60: Do not release the 'pException' pointer since it may
	 * be used again if a 'goto' statement is executed.
	 */
	return SXRET_OK;
}
/*
 * Section:
 *  Version,Credits and Copyright related functions.
 * Status:
 *    Stable.
 */
/*
 * string ph7version(void)
 *  Returns the running version of the PH7 version.
 * Parameters
 *  None
 * Return
 * Current PH7 version.
 */
static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg); /* cc warning */
	/* Current engine version */
	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);
	return PH7_OK;
}
/*
 * PH7 release information HTML page used by the ph7info() and ph7credits() functions.
 */
 #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\
 "<html><head>"\
 "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\
 "<style type=\"text/css\">"\
 "div {"\
     "border: 1px solid #cccccc;"\
     "-moz-border-radius-topleft: 10px;"\
     "-moz-border-radius-bottomright: 10px;"\
     "-moz-border-radius-bottomleft: 10px;"\
     "-moz-border-radius-topright: 10px;"\
     "-webkit-border-radius: 10px;"\
     "-o-border-radius: 10px;"\
     "border-radius: 10px;"\
     "padding-left: 2em;"\
     "background-color: white;"\
     "margin-left: auto;"\
     "font-family: verdana;"\
     "padding-right: 2em;"\
     "margin-right: auto;"\
     "}"\
     "body {"\
     "padding: 0.2em;"\
     "font-style: normal;"\
     "font-size: medium;"\
     "background-color: #f2f2f2;"\
     "}"\
     "hr {"\
     "border-style: solid none none;"\
     "border-width: 1px medium medium;"\
     "border-top: 1px solid #cccccc;"\
     "height: 1px;"\
     "}"\
     "a {"\
     "color: #3366cc;"\
     "text-decoration: none;"\
     "}"\
     "a:hover {"\
     "color: #999999;"\
     "}"\
     "a:active {"\
     "color: #663399;"\
     "}"\
     "h1 {"\
     "margin: 0;"\
     "padding: 0;"\
     "font-family: Verdana;"\
     "font-weight: bold;"\
     "font-style: normal;"\
     "font-size: medium;"\
     "text-transform: capitalize;"\
     "color: #0a328c;"\
     "}"\
     "p {"\
     "margin: 0 auto;"\
     "font-size: medium;"\
     "font-style: normal;"\
     "font-family: verdana;"\
     "}"\
"</style></head><body>"\
"<div style=\"background-color: white; width: 699px;\">"\
"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\
"<hr style=\"margin-left: auto; margin-right: auto;\">"\
"<p><small><small><span style=\"font-weight: bold;\">"\
"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\
"<p style=\"text-align: left;\"><small><small>"\
"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\
"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\
"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"

#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\
 "Symisc Public License (SPL)</a>&gt;</small></small></p>"

#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\
"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\
"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\
"&nbsp;*<br>"\
"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\
"&nbsp;* modification, are permitted provided that the following conditions<br>"\
"&nbsp;* are met:<br>"\
"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\
"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\
"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\
"&nbsp;*<br>"\
"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\
"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\
"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\
"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\
"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\
"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\
"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\
"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\
"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\
"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\
"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\
"&nbsp;*/<br>"\
"</span></small></small></p>"\
"</div></body></html>"
/*
 * bool ph7credits(void)
 * bool ph7info(void)
 * bool ph7copyright(void)
 *  Prints out the credits for PH7 engine
 * Parameters
 *  None
 * Return
 *  Always TRUE
 */
static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */
	/* Expand the HTML page above*/
	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);
	ph7_context_output_format(
		pCtx,
		PH7_HTML_PAGE_FORMAT,
		ph7_lib_version(),   /* Engine version */
		ph7_lib_signature(), /* Engine signature */
		ph7_lib_ident(),     /* Engine ID */
		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",
		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */
		SyHashTotalEntry(&pVm->hClass),
#ifdef __WINNT__
		"Windows NT"
#elif defined(__UNIXES__)
		"UNIX-Like"
#else
		"Other OS"
#endif
		);
	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Return TRUE */
	//ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Section:
 *    URL related routines.
 * Status:
 *    Stable.
 */
/* Forward declaration */
static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);
/*
 * value parse_url(string $url [, int $component = -1 ])
 *  Parse a URL and return its fields.
 * Parameters
 *  $url
 *   The URL to parse.
 * $component
 *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER
 *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve
 *  just a specific URL component as a string (except when PHP_URL_PORT is given
 *  in which case the return value will be an integer).
 * Return
 *  If the component parameter is omitted, an associative array is returned.
 *  At least one element will be present within the array. Potential keys within
 *  this array are:
 *   scheme - e.g. http
 *   host
 *   port
 *   user
 *   pass
 *   path
 *   query - after the question mark ?
 *   fragment - after the hashmark #
 * Note:
 *  FALSE is returned on failure.
 *  This function work with relative URL unlike the one shipped
 *  with the standard PHP engine.
 */
static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStr; /* Input string */
	SyString *pComp;  /* Pointer to the URI component */
	SyhttpUri sURI;   /* Parse of the given URI */
	int nLen;
	sxi32 rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the given URI */
	zStr = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Get a parse */
	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);
	if( rc != SXRET_OK ){
		/* Malformed input,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		int nComponent = ph7_value_to_int(apArg[1]);
		/* Refer to constant.c for constants values */
		switch(nComponent){
		case 1: /* PHP_URL_SCHEME */
			pComp = &sURI.sScheme;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 2: /* PHP_URL_HOST */
			pComp = &sURI.sHost;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 3: /* PHP_URL_PORT */
			pComp = &sURI.sPort;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				int iPort = 0;
				/* Cast the value to integer */
				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);
				ph7_result_int(pCtx,iPort);
			}
			break;
		case 4: /* PHP_URL_USER */
			pComp = &sURI.sUser;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 5: /* PHP_URL_PASS */
			pComp = &sURI.sPass;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 7: /* PHP_URL_QUERY */
			pComp = &sURI.sQuery;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 8: /* PHP_URL_FRAGMENT */
			pComp = &sURI.sFragment;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 6: /*  PHP_URL_PATH */
			pComp = &sURI.sPath;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		default:
			/* No such entry,return NULL */
			ph7_result_null(pCtx);
			break;
		}
	}else{
		ph7_value *pArray,*pValue;
		/* Return an associative array */
		pArray = ph7_context_new_array(pCtx);  /* Empty array */
		pValue = ph7_context_new_scalar(pCtx); /* Array value */
		if( pArray == 0 || pValue == 0 ){
			/* Out of memory */
			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");
			/* Return false */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Fill the array */
		pComp = &sURI.sScheme;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sHost;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sPort;
		if( pComp->nByte > 0 ){
			int iPort = 0;/* cc warning */
			/* Convert to integer */
			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);
			ph7_value_int(pValue,iPort);
			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sUser;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sPass;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sPath;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sQuery;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sFragment;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */
		}
		/* Return the created array */
		ph7_result_value(pCtx,pArray);
		/* NOTE:
		 * Don't worry about freeing 'pValue',everything will be released
		 * automatically as soon we return from this function.
		 */
	}
	/* All done */
	return PH7_OK;
}
/*
 * Section:
 *   Array related routines.
 * Status:
 *    Stable.
 * Note 2012-5-21 01:04:15:
 *  Array related functions that need access to the underlying
 *  virtual machine are implemented here rather than 'hashmap.c'
 */
/*
 * The [compact()] function store it's state information in an instance
 * of the following structure.
 */
struct compact_data
{
	ph7_value *pArray;  /* Target array */
	int nRecCount;      /* Recursion count */
};
/*
 * Walker callback for the [compact()] function defined below.
 */
static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct compact_data *pData = (struct compact_data *)pUserData;
	ph7_value *pArray = (ph7_value *)pData->pArray;
	ph7_vm *pVm = pArray->pVm;
	/* Act according to the hashmap value */
	if( ph7_value_is_string(pValue) ){
		SyString sVar;
		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));
		if( sVar.nByte > 0 ){
			/* Query the current frame */
			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);
			/* ^
			 * | Avoid wasting variable and use 'pKey' instead 
			 */
			if( pKey ){
				/* Perform the insertion */
				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);
			}
		}
	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {
		int rc;
		/* Recursively traverse this array */
		pData->nRecCount++;
		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);
		pData->nRecCount--;
		return rc;
	}
	return SXRET_OK;
}
/*
 * array compact(mixed $varname [, mixed $... ])
 *  Create array containing variables and their values.
 *  For each of these, compact() looks for a variable with that name
 *  in the current symbol table and adds it to the output array such 
 *  that the variable name becomes the key and the contents of the variable
 *  become the value for that key. In short, it does the opposite of extract().
 *  Any strings that are not set will simply be skipped. 
 * Parameters
 *  $varname
 *   compact() takes a variable number of parameters. Each parameter can be either
 *   a string containing the name of the variable, or an array of variable names.
 *   The array can contain other arrays of variable names inside it; compact() handles
 *   it recursively.
 * Return
 *  The output array with all the variables added to it or NULL on failure
 */
static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pObj;
	ph7_vm *pVm = pCtx->pVm;
	const char *zName;
	SyString sVar;
	int i,nLen;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create the array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Out of memory */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for( i = 0 ; i < nArg ; i++ ){
		if( !ph7_value_is_string(apArg[i]) ){
			if( ph7_value_is_array(apArg[i]) ){
				struct compact_data sData;
				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Recursively walk the array */
				sData.nRecCount = 0;
				sData.pArray = pArray;
				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);
			}
		}else{
			/* Extract variable name */
			zName = ph7_value_to_string(apArg[i],&nLen);
			if( nLen > 0 ){
				SyStringInitFromBuf(&sVar,zName,nLen);
				/* Check if the variable is available in the current frame */
				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);
				if( pObj ){
					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);
				}
			}
		}
	}
	/* Return the array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * The [extract()] function store it's state information in an instance
 * of the following structure.
 */
typedef struct extract_aux_data extract_aux_data;
struct extract_aux_data
{
	ph7_vm *pVm;          /* VM that own this instance */
	int iCount;           /* Number of variables successfully imported  */
	const char *zPrefix;  /* Prefix name */
	int Prefixlen;        /* Prefix  length */
	int iFlags;           /* Control flags */
	char zWorker[1024];   /* Working buffer */
};
/* Forward declaration */
static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);
/*
 * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])
 *   Import variables into the current symbol table from an array.
 * Parameters
 * $var_array
 *  An associative array. This function treats keys as variable names and values
 *  as variable values. For each key/value pair it will create a variable in the current symbol
 *  table, subject to extract_type and prefix parameters.
 *  You must use an associative array; a numerically indexed array will not produce results
 *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.
 * $extract_type
 *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.
 *  It can be one of the following values:
 *   EXTR_OVERWRITE
 *       If there is a collision, overwrite the existing variable. 
 *   EXTR_SKIP
 *       If there is a collision, don't overwrite the existing variable. 
 *   EXTR_PREFIX_SAME
 *       If there is a collision, prefix the variable name with prefix. 
 *   EXTR_PREFIX_ALL
 *       Prefix all variable names with prefix. 
 *   EXTR_PREFIX_INVALID
 *       Only prefix invalid/numeric variable names with prefix. 
 *   EXTR_IF_EXISTS
 *       Only overwrite the variable if it already exists in the current symbol table
 *       otherwise do nothing.
 *       This is useful for defining a list of valid variables and then extracting only those
 *       variables you have defined out of $_REQUEST, for example. 
 *   EXTR_PREFIX_IF_EXISTS
 *       Only create prefixed variable names if the non-prefixed version of the same variable exists in 
 *      the current symbol table.
 * $prefix
 *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL
 *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name
 *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an
 *  underscore character.
 * Return
 *   Returns the number of variables successfully imported into the symbol table.
 */
static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	extract_aux_data sAux;
	ph7_hashmap *pMap;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Empty map,return  0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Prepare the aux data */
	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));
	if( nArg > 1 ){
		sAux.iFlags = ph7_value_to_int(apArg[1]);
		if( nArg > 2 ){
			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);
		}
	}
	sAux.pVm = pCtx->pVm;
	/* Invoke the worker callback */
	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);
	/* Number of variables successfully imported */
	ph7_result_int(pCtx,sAux.iCount);
	return PH7_OK;
}
/*
 * Worker callback for the [extract()] function defined
 * below.
 */
static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	extract_aux_data *pAux = (extract_aux_data *)pUserData;
	int iFlags = pAux->iFlags;
	ph7_vm *pVm = pAux->pVm;
	ph7_value *pObj;
	SyString sVar;
	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL|MEMOBJ_REAL))){
		iFlags |= 0x08; /*EXTR_PREFIX_ALL*/
	}
	/* Perform a string cast */
	PH7_MemObjToString(pKey);
	if( SyBlobLength(&pKey->sBlob) < 1 ){
		/* Unavailable variable name */
		return SXRET_OK;
	}
	sVar.nByte = 0; /* cc warning */
	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){
		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",
			pAux->Prefixlen,pAux->zPrefix,
			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)
			);
	}else{
		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,
			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));
	}
	sVar.zString = pAux->zWorker;
	/* Try to extract the variable */
	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);
	if( pObj ){
		/* Collision */
		if( iFlags & 0x02 /* EXTR_SKIP */ ){
			return SXRET_OK;
		}
		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){
			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) || pAux->Prefixlen < 1){
				/* Already prefixed */
				return SXRET_OK;
			}
			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",
				pAux->Prefixlen,pAux->zPrefix,
				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)
				);
			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
		}
	}else{
		/* Create the variable */
		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
	}
	if( pObj ){
		/* Overwrite the old value */
		PH7_MemObjStore(pValue,pObj);
		/* Increment counter */
		pAux->iCount++;
	}
	return SXRET_OK;
}
/*
 * Worker callback for the [import_request_variables()] function
 * defined below.
 */
static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	extract_aux_data *pAux = (extract_aux_data *)pUserData;
	ph7_vm *pVm = pAux->pVm;
	ph7_value *pObj;
	SyString sVar;
	/* Perform a string cast */
	PH7_MemObjToString(pKey);
	if( SyBlobLength(&pKey->sBlob) < 1 ){
		/* Unavailable variable name */
		return SXRET_OK;
	}
	sVar.nByte = 0; /* cc warning */
	if( pAux->Prefixlen > 0 ){
		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",
			pAux->Prefixlen,pAux->zPrefix,
			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)
			);
	}else{
		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,
			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));
	}
	sVar.zString = pAux->zWorker;
	/* Extract the variable */
	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
	if( pObj ){
		PH7_MemObjStore(pValue,pObj);
	}
	return SXRET_OK;
}
/*
 * bool import_request_variables(string $types[,string $prefix])
 *  Import GET/POST/Cookie variables into the global scope.
 * Parameters
 * $types
 *  Using the types parameter, you can specify which request variables to import.
 *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.
 *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.
 *  POST includes the POST uploaded file information.
 *  Note:
 *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite 
 *  GET variables with the same name. Any other letters than GPC are discarded.
 * $prefix
 *  Variable name prefix, prepended before all variable's name imported into the global scope.
 *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global
 *  variable named $pref_userid.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPrefix,*zEnd,*zImport;
	extract_aux_data sAux;
	int nLen,nPrefixLen;
	ph7_value *pSuper;
	ph7_vm *pVm;
	/* By default import only $_GET variables  */
	zImport = "G";
	nLen = (int)sizeof(char);
	zPrefix = 0;
	nPrefixLen = 0;
	if( nArg > 0 ){
		if( ph7_value_is_string(apArg[0]) ){
			zImport = ph7_value_to_string(apArg[0],&nLen);
		}
		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);
		}
	}
	/* Point to the underlying VM */
	pVm = pCtx->pVm;
	/* Initialize the aux data */
	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));
	sAux.zPrefix = zPrefix;
	sAux.Prefixlen = nPrefixLen;
	sAux.pVm = pVm;
	/* Extract */
	zEnd = &zImport[nLen];
	while( zImport < zEnd ){
		int c = zImport[0];
		pSuper = 0;
		if( c == 'G' || c == 'g' ){
			/* Import $_GET variables */
			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);
		}else if( c == 'P' || c == 'p' ){
			/* Import $_POST variables */
			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);
		}else if( c == 'c' || c == 'C' ){
			/* Import $_COOKIE variables */
			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);
		}
		if( pSuper ){
			/* Iterate throw array entries */
			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);
		}
		/* Advance the cursor */
		zImport++;
	}
	/* All done,return TRUE*/
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * Compile and evaluate a PHP chunk at run-time.
 * Refer to the eval() language construct implementation for more
 * information.
 */
static sxi32 VmEvalChunk(
	ph7_vm *pVm,        /* Underlying Virtual Machine */
	ph7_context *pCtx,  /* Call Context */
	SyString *pChunk,   /* PHP chunk to evaluate */ 
	int iFlags,         /* Compile flag */
	int bTrueReturn     /* TRUE to return execution result */
	)
{
	SySet *pByteCode,aByteCode;
	ProcConsumer xErr = 0;
	void *pErrData = 0;
	/* Initialize bytecode container */
	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));
	SySetAlloc(&aByteCode,0x20);
	/* Reset the code generator */
	if( bTrueReturn ){
		/* Included file,log compile-time errors */
		xErr = pVm->pEngine->xConf.xErr;
		pErrData = pVm->pEngine->xConf.pErrData;
	}
	PH7_ResetCodeGenerator(pVm,xErr,pErrData);
	/* Swap bytecode container */
	pByteCode = pVm->pByteContainer;
	pVm->pByteContainer = &aByteCode;
	/* Compile the chunk */
	PH7_CompileScript(pVm,pChunk,iFlags);
	if( pVm->sCodeGen.nErr > 0 ){
		/* Compilation error,return false */
		if( pCtx ){
			ph7_result_bool(pCtx,0);
		}
	}else{
		ph7_value sResult; /* Return value */
		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){
			/* Out of memory */
			if( pCtx ){
				ph7_result_bool(pCtx,0);
			}
			goto Cleanup;
		}
		if( bTrueReturn ){
			/* Assume a boolean true return value */
			PH7_MemObjInitFromBool(pVm,&sResult,1);
		}else{
			/* Assume a null return value */
			PH7_MemObjInit(pVm,&sResult);
		}
		/* Execute the compiled chunk */
		VmLocalExec(pVm,&aByteCode,&sResult);
		if( pCtx ){
			/* Set the execution result */
			ph7_result_value(pCtx,&sResult);
		}
		PH7_MemObjRelease(&sResult);
	}
Cleanup:
	/* Cleanup the mess left behind */
	pVm->pByteContainer = pByteCode;
	SySetRelease(&aByteCode);
	return SXRET_OK;
}
/*
 * value eval(string $code)
 *   Evaluate a string as PHP code.
 * Parameter
 *  code: PHP code to evaluate.
 * Return
 *  eval() returns NULL unless return is called in the evaluated code, in which case 
 *  the value passed to return is returned. If there is a parse error in the evaluated
 *  code, eval() returns FALSE and execution of the following code continues normally.
 */
static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sChunk;    /* Chunk to evaluate */
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Chunk to evaluate */
	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);
	if( sChunk.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Eval the chunk */
	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);
	return SXRET_OK;
}
/*
 * Check if a file path is already included.
 */
static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)
{
	SyString *aEntries;
	sxu32 n;
	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);
	/* Perform a linear search */
	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){
		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){
			/* Already included */
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * Push a file path in the appropriate VM container.
 */
PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)
{
	SyString sPath;
	char *zDup;
#ifdef __WINNT__
	char *zCur;
#endif
	sxi32 rc;
	if( nLen < 0 ){
		nLen = SyStrlen(zPath);
	}
	/* Duplicate the file path first */
	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);
	if( zDup == 0 ){
		return SXERR_MEM;
	}
#ifdef __WINNT__
	/* Normalize path on windows
	 * Example:
	 *    Path/To/File.php
	 * becomes
	 *   path\to\file.php
	 */
	zCur = zDup;
	while( zCur[0] != 0 ){
		if( zCur[0] == '/' ){
			zCur[0] = '\\';
		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){
			int c = SyToLower(zCur[0]);
			zCur[0] = (char)c; /* MSVC stupidity */
		}
		zCur++;
	}
#endif
	/* Install the file path */
	SyStringInitFromBuf(&sPath,zDup,nLen);
	if( !bMain ){
		if( VmIsIncludedFile(&(*pVm),&sPath) ){
			/* Already included */
			*pNew = 0;
		}else{
			/* Insert in the corresponding container */
			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);
			if( rc != SXRET_OK ){
				SyMemBackendFree(&pVm->sAllocator,zDup);
				return rc;
			}
			*pNew = 1;
		}
	}
	SySetPut(&pVm->aFiles,(const void *)&sPath);
	return SXRET_OK;
}
/*
 * Compile and Execute a PHP script at run-time.
 * SXRET_OK is returned on sucessful evaluation.Any other return values
 * indicates failure.
 * Note that the PHP script to evaluate can be a local or remote file.In
 * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying
 * operations.
 * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then
 * this function is a no-op.
 * Refer to the implementation of the include(),include_once() language
 * constructs for more information.
 */
static sxi32 VmExecIncludedFile(
	 ph7_context *pCtx, /* Call Context */
	 SyString *pPath,   /* Script path or URL*/
	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */
	 )
{
	sxi32 rc;
#ifndef PH7_DISABLE_BUILTIN_FUNC
	const ph7_io_stream *pStream;
	SyBlob sContents;
	void *pHandle;
	ph7_vm *pVm;
	int isNew;
	/* Initialize fields */
	pVm = pCtx->pVm;
	SyBlobInit(&sContents,&pVm->sAllocator);
	isNew = 0;
	/* Extract the associated stream */
	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);
	/*
	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"] 
	 * in a read-only mode.
	 */
	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);
	if( pHandle == 0 ){
		return SXERR_IO;
	}
	rc = SXRET_OK; /* Stupid cc warning */
	if( IncludeOnce && !isNew ){
		/* Already included */
		rc = SXERR_EXISTS;
	}else{
		/* Read the whole file contents */
		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);
		if( rc == SXRET_OK ){
			SyString sScript;
			/* Compile and execute the script */
			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));
			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);
		}
	}
	/* Pop from the set of included file */
	(void)SySetPop(&pVm->aFiles);
	/* Close the handle */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Release the working buffer */
	SyBlobRelease(&sContents);
#else
	pCtx = 0; /* cc warning */
	pPath = 0;
	IncludeOnce = 0;
	rc = SXERR_IO;
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	return rc;
}
/*
 * string get_include_path(void)
 *  Gets the current include_path configuration option.
 * Parameter
 *  None
 * Return
 *  Included paths as a string
 */
static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyString *aEntry;
	int dir_sep;
	sxu32 n;
#ifdef __WINNT__
	dir_sep = ';';
#else
	/* Assume UNIX path separator */
	dir_sep = ':';
#endif
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg); 
	/* Point to the list of import paths */
	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);
	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){
		SyString *pEntry = &aEntry[n];
		if( n > 0 ){
			/* Append dir seprator */
			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));
		}
		/* Append path */
		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);
	}
	return PH7_OK;
}
/*
 * string get_get_included_files(void)
 *  Gets the current include_path configuration option.
 * Parameter
 *  None
 * Return
 *  Included paths as a string
 */
static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SySet *pFiles = &pCtx->pVm->aFiles;
	ph7_value *pArray,*pWorker;
	SyString *pEntry;
	int c,d;
	/* Create an array and a working value */
	pArray  = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 ){
		/* Out of memory,return null */
		ph7_result_null(pCtx);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg); 
		return PH7_OK;
	}
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
	/* Iterate throw entries */
	SySetResetCursor(pFiles);
	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){
		const char *zBase,*zEnd;
		int iLen;
		/* reset the string cursor */
		ph7_value_reset_string_cursor(pWorker);
		/* Extract base name */
		zEnd = &pEntry->zString[pEntry->nByte - 1];
		/* Ignore trailing '/' */
		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c || (int)zEnd[0] == d ) ){
			zEnd--;
		}
		iLen = (int)(&zEnd[1]-pEntry->zString);
		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){
			zEnd--;
		}
		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;
		zEnd = &pEntry->zString[iLen];
		/* Copy entry name */
		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));
		/* Perform the insertion */
		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */
	}
	/* All done,return the created array */
	ph7_result_value(pCtx,pArray);
	/* Note that 'pWorker' will be automatically destroyed 
	 * by the engine as soon we return from this foreign
	 * function.
	 */
	return PH7_OK;
}
/*
 * include:
 * According to the PHP reference manual.
 *  The include() function includes and evaluates the specified file.
 *  Files are included based on the file path given or, if none is given
 *  the include_path specified.If the file isn't found in the include_path
 *  include() will finally check in the calling script's own directory
 *  and the current working directory before failing. The include()
 *  construct will emit a warning if it cannot find a file; this is different
 *  behavior from require(), which will emit a fatal error.
 *  If a path is defined � whether absolute (starting with a drive letter
 *  or \ on Windows, or / on Unix/Linux systems) or relative to the current
 *  directory (starting with . or ..) � the include_path will be ignored altogether.
 *  For example, if a filename begins with ../, the parser will look in the parent
 *  directory to find the requested file.
 *  When a file is included, the code it contains inherits the variable scope
 *  of the line on which the include occurs. Any variables available at that line
 *  in the calling file will be available within the called file, from that point forward.
 *  However, all functions and classes defined in the included file have the global scope. 
 */
static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include */
	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);
	if( rc != SXRET_OK ){
		/* Emit a warning and return false */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
	}
	return SXRET_OK;
}
/*
 * include_once:
 *  According to the PHP reference manual.
 *   The include_once() statement includes and evaluates the specified file during
 *   the execution of the script. This is a behavior similar to the include() 
 *   statement, with the only difference being that if the code from a file has already
 *   been included, it will not be included again. As the name suggests, it will be included
 *   just once.
 */
static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include */
	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);
	if( rc == SXERR_EXISTS ){
		/* File already included,return TRUE */
		ph7_result_bool(pCtx,1);
		return SXRET_OK;
	}
	if( rc != SXRET_OK ){
		/* Emit a warning and return false */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
 	}
	return SXRET_OK;
}
/*
 * require.
 *  According to the PHP reference manual.
 *   require() is identical to include() except upon failure it will
 *   also produce a fatal level error.
 *   In other words, it will halt the script whereas include() only
 *   emits a warning  which allows the script to continue.
 */
static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include */
	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);
	if( rc != SXRET_OK ){
		/* Fatal,abort VM execution immediately */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/*
 * require_once:
 *  According to the PHP reference manual.
 *   The require_once() statement is identical to require() except PHP will check
 *   if the file has already been included, and if so, not include (require) it again.
 *   See the include_once() documentation for information about the _once behaviour
 *   and how it differs from its non _once siblings. 
 */
static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include */
	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);
	if( rc == SXERR_EXISTS ){
		/* File already included,return TRUE */
		ph7_result_bool(pCtx,1);
		return SXRET_OK;
	}
	if( rc != SXRET_OK ){
		/* Fatal,abort VM execution immediately */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/*
 * Section:
 *  Command line arguments processing.
 * Status:
 *    Stable.
 */
/*
 * Check if a short option argument [i.e: -c] is available in the command
 * line string. Return a pointer to the start of the stream on success.
 * NULL otherwise.
 */
static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)
{
	while( zIn < zEnd ){
		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){
			/* Got one */
			return &zIn[1];
		}
		/* Advance the cursor */
		zIn++;
	}
	/* No such option */
	return 0;
}
/*
 * Check if a long option argument [i.e: --opt] is available in the command
 * line string. Return a pointer to the start of the stream on success.
 * NULL otherwise.
 */
static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)
{
	const char *zOpt;
	while( zIn < zEnd ){
		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){
			zIn += 2;
			zOpt = zIn;
			while( zIn < zEnd && !SyisSpace(zIn[0]) ){
				if( zIn[0] == '=' /* --opt=val */){
					break;
				}
				zIn++;
			}
			/* Test */
			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){
				/* Got one,return it's value */
				return zIn;
			}

		}else{
			zIn++;
		}
	}
	/* No such option */
	return 0;
}
/*
 * Long option [i.e: --opt] arguments private data structure.
 */
struct getopt_long_opt
{
	const char *zArgIn,*zArgEnd; /* Command line arguments */
	ph7_value *pWorker;  /* Worker variable*/
	ph7_value *pArray;   /* getopt() return value */
	ph7_context *pCtx;   /* Call Context */
};
/* Forward declaration */
static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);
/*
 * Extract short or long argument option values.
 */
static void VmExtractOptArgValue(
	ph7_value *pArray,  /* getopt() return value */
	ph7_value *pWorker, /* Worker variable */
	const char *zArg,   /* Argument stream */
	const char *zArgEnd,/* End of the argument stream  */
	int need_val,       /* TRUE to fetch option argument */
	ph7_context *pCtx,  /* Call Context */
	const char *zName   /* Option name */)
{
	ph7_value_bool(pWorker,0);
	if( !need_val ){
		/* 
		 * Option does not need arguments.
		 * Insert the option name and a boolean FALSE.
		 */
		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */
	}else{
		const char *zCur;
		/* Extract option argument */
		zArg++;
		if( zArg < zArgEnd && zArg[0] == '=' ){
			zArg++;
		}
		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){
			zArg++;
		}
		if( zArg >= zArgEnd || zArg[0] == '-' ){
			/*
			 * Argument not found.
			 * Insert the option name and a boolean FALSE.
			 */
			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */
			return;
		}
		/* Delimit the value */
		zCur = zArg;
		if( zArg[0] == '\'' || zArg[0] == '"' ){
			int d = zArg[0];
			/* Delimt the argument */
			zArg++;
			zCur = zArg;
			while( zArg < zArgEnd ){
				if( zArg[0] == d && zArg[-1] != '\\' ){
					/* Delimiter found,exit the loop  */
					break;
				}
				zArg++;
			}
			/* Save the value */
			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));
			if( zArg < zArgEnd ){ zArg++; }
		}else{
			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){
				zArg++;
			}
			/* Save the value */
			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));
		}
		/*
		 * Check if we are dealing with multiple values.
		 * If so,create an array to hold them,rather than a scalar variable.
		 */
		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){
			zArg++;
		}
		if( zArg < zArgEnd && zArg[0] != '-' ){
			ph7_value *pOptArg; /* Array of option arguments */
			pOptArg = ph7_context_new_array(pCtx);
			if( pOptArg == 0 ){
				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			}else{
				/* Insert the first value */
				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */
				for(;;){
					if( zArg >= zArgEnd || zArg[0] == '-' ){
						/* No more value */
						break;
					}
					/* Delimit the value */
					zCur = zArg;
					if( zArg < zArgEnd && zArg[0] == '\\' ){
						zArg++;
						zCur = zArg;
					}
					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){
						zArg++;
					}
					/* Reset the string cursor */
					ph7_value_reset_string_cursor(pWorker);
					/* Save the value */
					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));
					/* Insert */
					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */
					/* Jump trailing white spaces */
					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){
						zArg++;
					}
				}
				/* Insert the option arg array */
				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */
				/* Safely release */
				ph7_context_release_value(pCtx,pOptArg);
			}
		}else{
			/* Single value */
			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */
		}
	}
}
/*
 * array getopt(string $options[,array $longopts ])
 *   Gets options from the command line argument list.
 * Parameters
 *  $options
 *   Each character in this string will be used as option characters
 *   and matched against options passed to the script starting with
 *   a single hyphen (-). For example, an option string "x" recognizes
 *   an option -x. Only a-z, A-Z and 0-9 are allowed.
 *  $longopts
 *   An array of options. Each element in this array will be used as option
 *   strings and matched against options passed to the script starting with
 *   two hyphens (--). For example, an longopts element "opt" recognizes an
 *   option --opt. 
 * Return
 *  This function will return an array of option / argument pairs or FALSE
 *  on failure. 
 */
static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;
	struct getopt_long_opt sLong;
	ph7_value *pArray,*pWorker;
	SyBlob *pArg;
	int nByte;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract option arguments */
	zIn  = ph7_value_to_string(apArg[0],&nByte);
	zEnd = &zIn[nByte];
	/* Point to the string representation of the $argv[] array */
	pArg = &pCtx->pVm->sArgv;
	/* Create a new empty array and a worker variable */
	pArray = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBlobLength(pArg) < 1 ){
		/* Empty command line,return the empty array*/
		ph7_result_value(pCtx,pArray);
		/* Everything will be released automatically when we return 
		 * from this function.
		 */
		return PH7_OK;
	}
	zArgIn = (const char *)SyBlobData(pArg);
	zArgEnd = &zArgIn[SyBlobLength(pArg)];
	/* Fill the long option structure */
	sLong.pArray = pArray;
	sLong.pWorker = pWorker;
	sLong.zArgIn =  zArgIn;
	sLong.zArgEnd = zArgEnd;
	sLong.pCtx = pCtx;
	/* Start processing */
	while( zIn < zEnd ){
		int c = zIn[0];
		int need_val = 0;
		/* Advance the stream cursor */
		zIn++;
		/* Ignore non-alphanum characters */
		if( !SyisAlphaNum(c) ){
			continue;
		}
		if( zIn < zEnd && zIn[0] == ':' ){
			zIn++;
			need_val = 1;
			if( zIn < zEnd && zIn[0] == ':' ){
				zIn++;
			}
		}
		/* Find option */
		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);
		if( zArg == 0 ){
			/* No such option */
			continue;
		}
		/* Extract option argument value */
		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);	
	}
	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){
		/* Process long options */
		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);
	}
	/* Return the option array */
	ph7_result_value(pCtx,pArray);
	/* 
	 * Don't worry about freeing memory, everything will be released
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * Array walker callback used for processing long options values.
 */
static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;
	const char *zArg,*zOpt,*zEnd;
	int need_value = 0;
	int nByte;
	/* Value must be of type string */
	if( !ph7_value_is_string(pValue) ){
		/* Simply ignore */
		return PH7_OK;
	}
	zOpt = ph7_value_to_string(pValue,&nByte);
	if( nByte < 1 ){
		/* Empty string,ignore */
		return PH7_OK;
	}
	zEnd = &zOpt[nByte - 1];
	if( zEnd[0] == ':' ){
		char *zTerm;
		/* Try to extract a value */
		need_value = 1;
		while( zEnd >= zOpt && zEnd[0] == ':' ){
			zEnd--;
		}
		if( zOpt >= zEnd ){
			/* Empty string,ignore */
			SXUNUSED(pKey);
			return PH7_OK;
		}
		zEnd++;
		zTerm = (char *)zEnd;
		zTerm[0] = 0;
	}else{
		zEnd = &zOpt[nByte];
	}
	/* Find the option */
	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);
	if( zArg == 0 ){
		/* No such option,return immediately */
		return PH7_OK;
	}
	/* Try to extract a value */
	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);
	return PH7_OK;
}
/*
 * Section: 
 *  JSON encoding/decoding routines.
 * Status:
 *    Devel.
 */
/* Forward reference */
static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);
static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);
/* 
 * JSON encoder state is stored in an instance 
 * of the following structure.
 */
typedef struct json_private_data json_private_data;
struct json_private_data
{
	ph7_context *pCtx; /* Call context */
	int isFirst;       /* True if first encoded entry */
	int iFlags;        /* JSON encoding flags */
	int nRecCount;     /* Recursion count */
};
/*
 * Returns the JSON representation of a value.In other word perform a JSON encoding operation.
 * According to wikipedia
 * JSON's basic types are:
 *   Number (double precision floating-point format in JavaScript, generally depends on implementation)
 *   String (double-quoted Unicode, with backslash escaping)
 *   Boolean (true or false)
 *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values
 *    do not need to be of the same type)
 *   Object (an unordered collection of key:value pairs with the ':' character separating the key 
 *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should
 *     be distinct from each other)
 *   null (empty)
 * Non-significant white space may be added freely around the "structural characters"
 * (i.e. the brackets "[{]}", colon ":" and comma ",").
 */
static sxi32 VmJsonEncode(
	ph7_value *pIn,          /* Encode this value */
	json_private_data *pData /* Context data */
	){
		ph7_context *pCtx = pData->pCtx;
		int iFlags = pData->iFlags;
		int nByte;
		if( ph7_value_is_null(pIn) || ph7_value_is_resource(pIn)){
			/* null */
			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);
		}else if( ph7_value_is_bool(pIn) ){
			int iBool = ph7_value_to_bool(pIn);
			int iLen;
			/* true/false */
			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");
			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);
		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){
			const char *zNum;
			/* Get a string representation of the number */
			zNum = ph7_value_to_string(pIn,&nByte);
			ph7_result_string(pCtx,zNum,nByte);
		}else if( ph7_value_is_string(pIn) ){
			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){
				const char *zNum;
				/* Encodes numeric strings as numbers. */
				PH7_MemObjToReal(pIn); /* Force a numeric cast */
				/* Get a string representation of the number */
				zNum = ph7_value_to_string(pIn,&nByte);
				ph7_result_string(pCtx,zNum,nByte);
			}else{
				const char *zIn,*zEnd;
				int c;
				/* Encode the string */
				zIn = ph7_value_to_string(pIn,&nByte);
				zEnd = &zIn[nByte];
				/* Append the double quote */
				ph7_result_string(pCtx,"\"",(int)sizeof(char));
				for(;;){
					if( zIn >= zEnd ){
						/* No more input to process */
						break;
					}
					c = zIn[0];
					/* Advance the stream cursor */
					zIn++;
					if( (c == '<' || c == '>') && (iFlags & JSON_HEX_TAG) ){
						/* All < and > are converted to \u003C and \u003E */
						if( c == '<' ){
							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);
						}else{
							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);
						}
						continue;
					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){
						/* All &s are converted to \u0026.  */
						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);
						continue;
					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){
						/* All ' are converted to \u0027.   */
						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);
						continue;
					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){
						/* All " are converted to \u0022. */
						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);
						continue;
					}
					if( c == '"' || (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){
						/* Unescape the character */
						ph7_result_string(pCtx,"\\",(int)sizeof(char));
					}
					/* Append character verbatim */
					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
				}
				/* Append the double quote */
				ph7_result_string(pCtx,"\"",(int)sizeof(char));
			}
		}else if( ph7_value_is_array(pIn) ){
			int c = '[',d = ']';
			/* Encode the array */
			pData->isFirst = 1;
			if( iFlags & JSON_FORCE_OBJECT ){
				/* Outputs an object rather than an array */
				c = '{';
				d = '}';
			}
			/* Append the square bracket or curly braces */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			/* Iterate throw array entries */
			ph7_array_walk(pIn,VmJsonArrayEncode,pData);
			/* Append the closing square bracket or curly braces */
			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));
		}else if( ph7_value_is_object(pIn) ){
			/* Encode the class instance */
			pData->isFirst = 1;
			/* Append the curly braces */
			ph7_result_string(pCtx,"{",(int)sizeof(char));
			/* Iterate throw class attribute */
			ph7_object_walk(pIn,VmJsonObjectEncode,pData);
			/* Append the closing curly braces  */
			ph7_result_string(pCtx,"}",(int)sizeof(char));
		}else{
			/* Can't happen */
			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);
		}
		/* All done */
		return PH7_OK;
}
/*
 * The following walker callback is invoked each time we need
 * to encode an array to JSON.
 */
static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	json_private_data *pJson = (json_private_data *)pUserData;
	if( pJson->nRecCount > 31 ){
		/* Recursion limit reached,return immediately */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the colon first */
		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));
	}
	if( pJson->iFlags & JSON_FORCE_OBJECT ){
		/* Outputs an object rather than an array */
		const char *zKey;
		int nByte;
		/* Extract a string representation of the key */
		zKey = ph7_value_to_string(pKey,&nByte);
		/* Append the key and the double colon */
		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);
	}
	/* Encode the value */
	pJson->nRecCount++;
	VmJsonEncode(pValue,pJson);
	pJson->nRecCount--;
	pJson->isFirst = 0;
	return PH7_OK;
}
/*
 * The following walker callback is invoked each time we need to encode
 * a class instance [i.e: Object in the PHP jargon] to JSON.
 */
static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)
{
	json_private_data *pJson = (json_private_data *)pUserData;
	if( pJson->nRecCount > 31 ){
		/* Recursion limit reached,return immediately */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the colon first */
		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));
	}
	/* Append the attribute name and the double colon first */
	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);
	/* Encode the value */
	pJson->nRecCount++;
	VmJsonEncode(pValue,pJson);
	pJson->nRecCount--;
	pJson->isFirst = 0;
	return PH7_OK;
}
/*
 * string json_encode(mixed $value [, int $options = 0 ])
 *  Returns a string containing the JSON representation of value.
 * Parameters
 *  $value
 *  The value being encoded. Can be any type except a resource.
 * $options
 *  Bitmask consisting of:
 *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.
 *  JSON_HEX_AMP   All &s are converted to \u0026. 
 *  JSON_HEX_APOS  All ' are converted to \u0027.
 *  JSON_HEX_QUOT  All " are converted to \u0022.
 *  JSON_FORCE_OBJECT  Outputs an object rather than an array.
 *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.
 *  JSON_BIGINT_AS_STRING   Not used 
 *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.
 *  JSON_UNESCAPED_SLASHES  Don't escape '/' 
 *  JSON_UNESCAPED_UNICODE  Not used.
 * Return
 *  Returns a JSON encoded string on success. FALSE otherwise
 */
static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	json_private_data sJson;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Prepare the JSON data */
	sJson.nRecCount = 0;
	sJson.pCtx = pCtx;
	sJson.isFirst = 1;
	sJson.iFlags = 0;
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		/* Extract option flags */
		sJson.iFlags = ph7_value_to_int(apArg[1]);
	}
	/* Perform the encoding operation */
	VmJsonEncode(apArg[0],&sJson);
	/* All done */
	return PH7_OK;
}
/*
 * int json_last_error(void)
 *  Returns the last error (if any) occurred during the last JSON encoding/decoding.
 * Parameters
 *  None
 * Return
 *  Returns an integer, the value can be one of the following constants:
 *  JSON_ERROR_NONE            No error has occurred.
 *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded. 
 *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.
 *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.
 *  JSON_ERROR_SYNTAX          Syntax error.
 *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.
 */
static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	/* Return the error code */
	ph7_result_int(pCtx,pVm->json_rc);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/* Possible tokens from the JSON tokenization process */
#define JSON_TK_TRUE    0x001 /* Boolean true */
#define JSON_TK_FALSE   0x002 /* Boolean false */
#define JSON_TK_STR     0x004 /* String enclosed in double quotes */
#define JSON_TK_NULL    0x008 /* null */
#define JSON_TK_NUM     0x010 /* Numeric */
#define JSON_TK_OCB     0x020 /* Open curly braces '{' */
#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */
#define JSON_TK_OSB     0x080 /* Open square bracke '[' */
#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */
#define JSON_TK_COLON   0x200 /* Single colon ':' */
#define JSON_TK_COMMA   0x400 /* Single comma ',' */
#define JSON_TK_INVALID 0x800 /* Unexpected token */
/* 
 * Tokenize an entire JSON input.
 * Get a single low-level token from the input file.
 * Update the stream pointer so that it points to the first
 * character beyond the extracted token.
 */
static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)
{
	int *pJsonErr = (int *)pUserData;
	SyString *pStr;
	int c;
	/* Ignore leading white spaces */
	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){
		/* Advance the stream cursor */
		if( pStream->zText[0] == '\n' ){
			/* Update line counter */
			pStream->nLine++;
		}
		pStream->zText++;
	}
	if( pStream->zText >= pStream->zEnd ){
		/* End of input reached */
		SXUNUSED(pCtxData); /* cc warning */
		return SXERR_EOF;
	}
	/* Record token starting position and line */
	pToken->nLine = pStream->nLine;
	pToken->pUserData = 0;
	pStr = &pToken->sData;
	SyStringInitFromBuf(pStr,pStream->zText,0);
	if( pStream->zText[0] == '{' || pStream->zText[0] == '[' || pStream->zText[0] == '}' || pStream->zText[0] == ']'
		|| pStream->zText[0] == ':' || pStream->zText[0] == ',' ){
			/* Single character */
			c = pStream->zText[0];
			/* Set token type */
			switch(c){
			case '[': pToken->nType = JSON_TK_OSB;   break;
			case '{': pToken->nType = JSON_TK_OCB;   break;
			case '}': pToken->nType = JSON_TK_CCB;   break;
			case ']': pToken->nType = JSON_TK_CSB;   break;
			case ':': pToken->nType = JSON_TK_COLON; break;
			case ',': pToken->nType = JSON_TK_COMMA; break;
			default:
				break;
			}
			/* Advance the stream cursor */
			pStream->zText++;
	}else if( pStream->zText[0] == '"') {
		/* JSON string */
		pStream->zText++;
		pStr->zString++;
		/* Delimit the string */
		while( pStream->zText < pStream->zEnd ){
			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){
				break;
			}
			if( pStream->zText[0] == '\n' ){
				/* Update line counter */
				pStream->nLine++;
			}
			pStream->zText++;
		}
		if( pStream->zText >= pStream->zEnd ){
			/* Missing closing '"' */
			pToken->nType = JSON_TK_INVALID;
			*pJsonErr = JSON_ERROR_SYNTAX;
		}else{
			pToken->nType = JSON_TK_STR;
			pStream->zText++; /* Jump the closing double quotes */
		}
	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
		/* Number */
		pStream->zText++;
		pToken->nType = JSON_TK_NUM;
		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
			pStream->zText++;
		}
		if( pStream->zText < pStream->zEnd ){
			c = pStream->zText[0];
			if( c == '.' ){
					/* Real number */
					pStream->zText++;
					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
						pStream->zText++;
					}
					if( pStream->zText < pStream->zEnd ){
						c = pStream->zText[0];
						if( c=='e' || c=='E' ){
							pStream->zText++;
							if( pStream->zText < pStream->zEnd ){
								c = pStream->zText[0];
								if( c =='+' || c=='-' ){
									pStream->zText++;
								}
								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
									pStream->zText++;
								}
							}
						}
					}					
				}else if( c=='e' || c=='E' ){
					/* Real number */
					pStream->zText++;
					if( pStream->zText < pStream->zEnd ){
						c = pStream->zText[0];
						if( c =='+' || c=='-' ){
							pStream->zText++;
						}
						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
							pStream->zText++;
						}
					}					
				}
			}
	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 && 
		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){
			/* boolean true */
			pToken->nType = JSON_TK_TRUE;
			/* Advance the stream cursor */
			pStream->zText += sizeof("true")-1;
	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 && 
		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){
			/* boolean false */
			pToken->nType = JSON_TK_FALSE;
			/* Advance the stream cursor */
			pStream->zText += sizeof("false")-1;
	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 && 
		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){
			/* NULL */
			pToken->nType = JSON_TK_NULL;
			/* Advance the stream cursor */
			pStream->zText += sizeof("null")-1;
	}else{
		/* Unexpected token */
		pToken->nType = JSON_TK_INVALID;
		/* Advance the stream cursor */
		pStream->zText++;
		*pJsonErr = JSON_ERROR_SYNTAX;
		/* Abort processing immediatley */
		return SXERR_ABORT;
	}
	/* record token length */
	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
	if( pToken->nType == JSON_TK_STR ){
		pStr->nByte--;
	}
	/* Return to the lexer */
	return SXRET_OK;
}
/*
 * JSON decoded input consumer callback signature.
 */
typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *); 
/* 
 * JSON decoder state is kept in the following structure.
 */
typedef struct json_decoder json_decoder;
struct json_decoder
{
	ph7_context *pCtx; /* Call context */
	ProcJsonConsumer xConsumer; /* Consumer callback */ 
	void *pUserData;   /* Last argument to xConsumer() */
	int iFlags;        /* Configuration flags */
	SyToken *pIn;      /* Token stream */
	SyToken *pEnd;     /* End of the token stream */
	int rec_depth;     /* Recursion limit */
	int rec_count;     /* Current nesting level */
	int *pErr;         /* JSON decoding error if any */
};
#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */
/* Forward declaration */
static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);
/*
 * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store
 * the result in the given ph7_value.
 */
static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)
{
	const char *zIn = pStr->zString;
	const char *zEnd = &pStr->zString[pStr->nByte];
	const char *zCur;
	int c;
	/* Mark the value as a string */
	ph7_value_string(pWorker,"",0); /* Empty string */
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append chunk verbatim */
			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));
		}
		zIn++;
		if( zIn >= zEnd ){
			/* End of the input reached */
			break;
		}
		c = zIn[0];
		/* Unescape the character */
		switch(c){
		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;
		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;
		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;
		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;
		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;
		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;
		default:
			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));
			break;
		}
		/* Advance the stream cursor */
		zIn++;
	}
}
/*
 * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.
 * According to wikipedia
 * JSON's basic types are:
 *   Number (double precision floating-point format in JavaScript, generally depends on implementation)
 *   String (double-quoted Unicode, with backslash escaping)
 *   Boolean (true or false)
 *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values
 *    do not need to be of the same type)
 *   Object (an unordered collection of key:value pairs with the ':' character separating the key 
 *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should
 *     be distinct from each other)
 *   null (empty)
 * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").
 */
static sxi32 VmJsonDecode(
	json_decoder *pDecoder, /* JSON decoder */      
	ph7_value *pArrayKey    /* Key for the decoded array */
	){
	ph7_value *pWorker; /* Worker variable */
	sxi32 rc;
	/* Check if we do not nest to much */
	if( pDecoder->rec_count >= pDecoder->rec_depth ){
		/* Nesting limit reached,abort decoding immediately */
		*pDecoder->pErr = JSON_ERROR_DEPTH;
		return SXERR_ABORT;
	}
	if( pDecoder->pIn->nType & (JSON_TK_STR|JSON_TK_TRUE|JSON_TK_FALSE|JSON_TK_NULL|JSON_TK_NUM) ){
		/* Scalar value */
		pWorker = ph7_context_new_scalar(pDecoder->pCtx);
		if( pWorker == 0 ){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			/* Abort the decoding operation immediately */
			return SXERR_ABORT;
		}
		/* Reflect the JSON image */
		if( pDecoder->pIn->nType & JSON_TK_NULL ){
			/* Nullify the value.*/
			ph7_value_null(pWorker);
		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE|JSON_TK_FALSE) ){
			/* Boolean value */
			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );
		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){
			SyString *pStr = &pDecoder->pIn->sData;
			/* 
			 * Numeric value.
			 * Get a string representation first then try to get a numeric
			 * value.
			 */
			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);
			/* Obtain a numeric representation */
			PH7_MemObjToNumeric(pWorker);
		}else{
			/* Dequote the string */
			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);
		}
		/* Invoke the consumer callback */
		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* All done,advance the stream cursor */
		pDecoder->pIn++;
	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {
		ProcJsonConsumer xOld;
		void *pOld;
		/* Array representation*/
		pDecoder->pIn++;
		/* Create a working array */
		pWorker = ph7_context_new_array(pDecoder->pCtx);
		if( pWorker == 0 ){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			/* Abort the decoding operation immediately */
			return SXERR_ABORT;
		}
		/* Save the old consumer */
		xOld = pDecoder->xConsumer;
		pOld = pDecoder->pUserData;
		/* Set the new consumer */
		pDecoder->xConsumer = VmJsonArrayDecoder;
		pDecoder->pUserData = pWorker;
		/* Decode the array */
		for(;;){
			/* Jump trailing comma. Note that the standard PHP engine will not let you
			 * do this.
			 */
			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){
				pDecoder->pIn++;
			}
			if( pDecoder->pIn >= pDecoder->pEnd || (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){
				if( pDecoder->pIn < pDecoder->pEnd ){
					pDecoder->pIn++; /* Jump the trailing ']' */
				}
				break;
			}
			/* Recurse and decode the entry */
			pDecoder->rec_count++;
			rc = VmJsonDecode(pDecoder,0);
			pDecoder->rec_count--;
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				return SXERR_ABORT;
			}
			/*The cursor is automatically advanced by the VmJsonDecode() function */
			if( (pDecoder->pIn < pDecoder->pEnd) &&
				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/|JSON_TK_COMMA/*','*/))==0) ){
					/* Unexpected token,abort immediatley */
					*pDecoder->pErr = JSON_ERROR_SYNTAX;
					return SXERR_ABORT;
			}
		}
		/* Restore the old consumer */
		pDecoder->xConsumer = xOld;
		pDecoder->pUserData = pOld;
		/* Invoke the old consumer on the decoded array */
		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);
	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {
		ProcJsonConsumer xOld;
		ph7_value *pKey;
		void *pOld;
		/* Object representation*/
		pDecoder->pIn++;
		/* Return the object as an associative array */
		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,
				"JSON Objects are always returned as an associative array"
				);
		}
		/* Create a working array */
		pWorker = ph7_context_new_array(pDecoder->pCtx);
		pKey = ph7_context_new_scalar(pDecoder->pCtx);
		if( pWorker == 0 || pKey == 0){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			/* Abort the decoding operation immediately */
			return SXERR_ABORT;
		}
		/* Save the old consumer */
		xOld = pDecoder->xConsumer;
		pOld = pDecoder->pUserData;
		/* Set the new consumer */
		pDecoder->xConsumer = VmJsonArrayDecoder;
		pDecoder->pUserData = pWorker;
		/* Decode the object */
		for(;;){
			/* Jump trailing comma. Note that the standard PHP engine will not let you
			 * do this.
			 */
			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){
				pDecoder->pIn++;
			}
			if( pDecoder->pIn >= pDecoder->pEnd || (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){
				if( pDecoder->pIn < pDecoder->pEnd ){
					pDecoder->pIn++; /* Jump the trailing ']' */
				}
				break;
			}
			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 || &pDecoder->pIn[1] >= pDecoder->pEnd
				|| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){
					/* Syntax error,return immediately */
					*pDecoder->pErr = JSON_ERROR_SYNTAX;
					return SXERR_ABORT;
			}
			/* Dequote the key */
			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);
			/* Jump the key and the colon */
			pDecoder->pIn += 2; 
			/* Recurse and decode the value */
			pDecoder->rec_count++;
			rc = VmJsonDecode(pDecoder,pKey);
			pDecoder->rec_count--;
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				return SXERR_ABORT;
			}
			/* Reset the internal buffer of the key */
			ph7_value_reset_string_cursor(pKey);
			/*The cursor is automatically advanced by the VmJsonDecode() function */
		}
		/* Restore the old consumer */
		pDecoder->xConsumer = xOld;
		pDecoder->pUserData = pOld;
		/* Invoke the old consumer on the decoded object*/
		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);
		/* Release the key */
		ph7_context_release_value(pDecoder->pCtx,pKey);
	}else{
		/* Unexpected token */
		return SXERR_ABORT; /* Abort immediately */
	}
	/* Release the worker variable */
	ph7_context_release_value(pDecoder->pCtx,pWorker);
	return SXRET_OK;
}
/*
 * The following JSON decoder callback is invoked each time
 * a JSON array representation [i.e: [15,"hello",FALSE] ]
 * is being decoded.
 */
static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	/* Insert the entry */
	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */
	SXUNUSED(pCtx); /* cc warning */
	/* All done */
	return SXRET_OK;
}
/*
 * Standard JSON decoder callback.
 */
static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)
{
	/* Return the value directly */
	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */
	SXUNUSED(pKey); /* cc warning */
	SXUNUSED(pUserData);
	/* All done */
	return SXRET_OK;
}
/*
 * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])
 *  Takes a JSON encoded string and converts it into a PHP variable.
 * Parameters
 *  $json
 *    The json string being decoded.
 * $assoc
 *   When TRUE, returned objects will be converted into associative arrays.
 * $depth
 *   User specified recursion depth.
 * $options
 *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported 
 * (default is to cast large integers as floats)
 * Return
 *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)
 *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded
 *  or if the encoded data is deeper than the recursion limit.
 */
static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	json_decoder sDecoder;
	const char *zIn;
	SySet sToken;
	SyLex sLex;
	int nByte;
	sxi32 rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments, return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the JSON string */
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Clear JSON error code */
	pVm->json_rc = JSON_ERROR_NONE;
	/* Tokenize the input */
	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));
	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);
	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);
	if( pVm->json_rc != JSON_ERROR_NONE ){
		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */
		SyLexRelease(&sLex);
		SySetRelease(&sToken);
		/* return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the decoder */
	sDecoder.pCtx = pCtx;
	sDecoder.pErr = &pVm->json_rc;
	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);
	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];
	sDecoder.iFlags = 0;
	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){
		/* Returned objects will be converted into associative arrays */
		sDecoder.iFlags |= JSON_DECODE_ASSOC;
	}
	sDecoder.rec_depth = 32;
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		int nDepth = ph7_value_to_int(apArg[2]);
		if( nDepth > 1 && nDepth < 32 ){
			sDecoder.rec_depth = nDepth;
		}
	}
	sDecoder.rec_count = 0;
	/* Set a default consumer */
	sDecoder.xConsumer = VmJsonDefaultDecoder;
	sDecoder.pUserData = 0;
	/* Decode the raw JSON input */
	rc = VmJsonDecode(&sDecoder,0);
	if( rc == SXERR_ABORT ||  pVm->json_rc != JSON_ERROR_NONE ){
		/* 
		 * Something goes wrong while decoding JSON input.Return NULL.
		 */
		ph7_result_null(pCtx);
	}
	/* Clean-up the mess left behind */
	SyLexRelease(&sLex);
	SySetRelease(&sToken);
	/* All done */
	return PH7_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * XML processing Functions.
 * Status:
 *    Devel.
 */
enum ph7_xml_handler_id{
	PH7_XML_START_TAG = 0, /* Start element handlers ID */
	PH7_XML_END_TAG,       /* End element handler ID*/
	PH7_XML_CDATA,         /* Character data handler ID*/
	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/
	PH7_XML_DEF,           /* Default handler ID */
	PH7_XML_UNPED,         /* Unparsed entity declaration handler */
	PH7_XML_ND,            /* Notation declaration handler ID*/
	PH7_XML_EER,           /* External entity reference handler */
	PH7_XML_NS_START,      /* Start namespace declaration handler */
	PH7_XML_NS_END         /* End namespace declaration handler */
};
#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)
/* An instance of the following structure describe a working
 * XML engine instance.
 */
typedef struct ph7_xml_engine ph7_xml_engine;
struct ph7_xml_engine
{
	ph7_vm *pVm;         /* VM that own this instance */
	ph7_context *pCtx;   /* Call context */
	SyXMLParser sParser; /* Underlying XML parser */
	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */
	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded
							  * as the first argument to the user callbacks.
							  */
	int ns_sep;      /* Namespace separator */
	SyBlob sErr;     /* Error message consumer */
	sxi32 iErrCode;  /* Last error code */
	sxi32 iNest;     /* Nesting level */
	sxu32 nLine;     /* Last processed line */
	sxu32 nMagic;    /* Magic number so that we avoid misuse  */
};
#define XML_ENGINE_MAGIC 0x851EFC52
#define IS_INVALID_XML_ENGINE(XML) (XML == 0 || (XML)->nMagic != XML_ENGINE_MAGIC)
/* 
 * Allocate and initialize an XML engine.
 */
static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)
{
	ph7_xml_engine *pEngine;
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pValue;
	sxu32 n;
	/* Allocate a new instance */
	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));
	if( pEngine == 0 ){
		/* Out of memory */
		return 0;
	}
	/* Zero the structure */
	SyZero(pEngine,sizeof(ph7_xml_engine));
	/* Initialize fields */
	pEngine->pVm = pVm;
	pEngine->pCtx = 0;
	pEngine->ns_sep = ns_sep;
	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);
	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);
	PH7_MemObjInit(pVm,&pEngine->sParserValue);
	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){
		pValue = &pEngine->aCB[n];
		/* NULLIFY the array entries,until someone register an event handler */
		PH7_MemObjInit(&(*pVm),pValue);
	}
	ph7_value_resource(&pEngine->sParserValue,pEngine);
	pEngine->iErrCode = SXML_ERROR_NONE;
	/* Finally set the magic number */
	pEngine->nMagic = XML_ENGINE_MAGIC;
	return pEngine;
}
/*
 * Release an XML engine.
 */
static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)
{
	ph7_vm *pVm = pEngine->pVm;
	ph7_value *pValue;
	sxu32 n;
	/* Release fields */
	SyBlobRelease(&pEngine->sErr);
	SyXMLParserRelease(&pEngine->sParser);
	PH7_MemObjRelease(&pEngine->sParserValue);
	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){
		pValue = &pEngine->aCB[n];
		PH7_MemObjRelease(pValue);
	}
	pEngine->nMagic = 0x2621;
	/* Finally,release the whole instance */
	SyMemBackendFree(&pVm->sAllocator,pEngine);
}
/*
 * resource xml_parser_create([ string $encoding ])
 *  Create an UTF-8 XML parser.
 * Parameter
 *  $encoding
 *   (Only UTF-8 encoding is used) 
 * Return
 *  Returns a resource handle for the new XML parser.
 */
static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	/* Allocate a new instance */
	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');
	if( pEngine == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		/* Return null */
		ph7_result_null(pCtx);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Return the engine as a resource */
	ph7_result_resource(pCtx,pEngine);
	return PH7_OK;
}
/*
 * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])
 *  Create an UTF-8 XML parser with namespace support.
 * Parameter
 *  $encoding
 *   (Only UTF-8 encoding is supported) 
 *  $separtor
 *   Namespace separator (a single character)
 * Return
 *  Returns a resource handle for the new XML parser.
 */
static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	int ns_sep = ':';
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		const char *zSep = ph7_value_to_string(apArg[1],0);
		if( zSep[0] != 0 ){
			ns_sep = zSep[0];
		}
	}
	/* Allocate a new instance */
	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);
	if( pEngine == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		/* Return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Return the engine as a resource */
	ph7_result_resource(pCtx,pEngine);
	return PH7_OK;
}
/*
 * bool xml_parser_free(resource $parser)
 *  Release an XML engine.
 * Parameter
 *  $parser
 *   A reference to the XML parser to free. 
 * Return
 *  This function returns FALSE if parser does not refer
 *  to a valid parser, or else it frees the parser and returns TRUE.
 */
static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Safely release the engine */
	VmReleaseXMLEngine(pEngine);
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])
 * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler
 * are strings containing the names of functions.
 * Parameters
 *  $parser
 *   A reference to the XML parser to set up start and end element handler functions.
 *  $start_element_handler
 *    The function named by start_element_handler must accept three parameters:
 *    start_element_handler(resource $parser,string $name,array $attribs)
 *    $parser
 *      The first parameter, parser, is a reference to the XML parser calling the handler. 
 *   $name
 *      The second parameter, name, contains the name of the element for which this handler
 *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters. 
 *  $attribs
 *      The third parameter, attribs, contains an associative array with the element's attributes (if any).
 *		The keys of this array are the attribute names, the values are the attribute values.
 *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.
 *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().
 *      The first key in the array was the first attribute, and so on. 
 *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.
 * $end_element_handler
 *     The function named by end_element_handler must accept two parameters:
 *     end_element_handler(resource $parser,string $name)
 *    $parser
 *      The first parameter, parser, is a reference to the XML parser calling the handler. 
 *   $name
 *      The second parameter, name, contains the name of the element for which this handler
 *      is called.If case-folding is in effect for this parser, the element name will be in uppercase
 *      letters. 
 *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.
 * Return
 * TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the start_element_handler callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);
		if( nArg > 2 ){
			/* Save the end_element_handler callback for later invocation */
			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_character_data_handler(resource $parser,callback $handler)
 *  Sets the character data handler function for the XML parser parser. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $data
 *   The second parameter, data, contains the character data as a string. 
 *   Character data handler is called for every piece of a text in the XML document.
 *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).
 *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.
 *   Note: Instead of a function name, an array containing an object reference and a method name 
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_default_handler(resource $parser,callback $handler)
 *  Set up default handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $data
 *   The second parameter, data, contains the character data.This may be the XML declaration
 *   document type declaration, entities or other data for which no other handler exists.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)
 *  Set up end namespace declaration handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $prefix)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $prefix
 *   The prefix is a string used to reference the namespace within an XML object.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)
 *  Set up start namespace declaration handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $prefix,string $uri)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $prefix
 *   The prefix is a string used to reference the namespace within an XML object.
 *  $uri
 *    Uniform Resource Identifier (URI) of namespace. 
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)
 *  Set up processing instruction (PI) handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept three parameters:
 *   handler(resource $parser,string $target,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $target
 *   The second parameter, target, contains the PI target. 
 *  $data
     The third parameter, data, contains the PI data. 
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)
 *  Set up unparsed entity declaration handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept six parameters:
 *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $entity_name
 *   The name of the entity that is about to be defined. 
 *  $base
 *   This is the base for resolving the system identifier (systemId) of the external entity.
 *   Currently this parameter will always be set to an empty string. 
 *  $system_id
 *   System identifier for the external entity. 
 *  $public_id
 *    Public identifier for the external entity. 
 *  $notation_name
 *    Name of the notation of this entity (see xml_set_notation_decl_handler()). 
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_notation_decl_handler(resource $parser,callback $handler)
 *  Set up notation declaration handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept five parameters:
 *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $entity_name
 *   The name of the entity that is about to be defined. 
 *  $base
 *   This is the base for resolving the system identifier (systemId) of the external entity.
 *   Currently this parameter will always be set to an empty string. 
 *  $system_id
 *   System identifier for the external entity. 
 *  $public_id
 *    Public identifier for the external entity.
 *  Note: Instead of a function name, an array containing an object reference and a method name
 *  can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)
 *  Set up external entity reference handler. 
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept five parameters:
 *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $open_entity_names
 *   The second parameter, open_entity_names, is a space-separated list of the names 
 *   of the entities that are open for the parse of this entity (including the name of the referenced entity). 
 *  $base
 *   This is the base for resolving the system identifier (system_id) of the external entity.
 *   Currently this parameter will always be set to an empty string. 
 *  $system_id
 *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration. 
 *  $public_id
 *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration
 *   or an empty string if none was specified; the whitespace in the public identifier will have been
 *   normalized as required by the XML spec. 
 * Note: Instead of a function name, an array containing an object reference and a method name
 * can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int xml_get_current_line_number(resource $parser)
 *  Gets the current line number for the given XML parser.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer
 *  to a valid parser, or else it returns which line the parser
 *  is currently at in its data buffer. 
 */
static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return the line number */
	ph7_result_int(pCtx,(int)pEngine->nLine);
	return PH7_OK;
}
/*
 * int xml_get_current_byte_index(resource $parser)
 *  Gets the current byte index of the given XML parser.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer to a valid 
 *  parser, or else it returns which byte index the parser is currently
 *  at in its data buffer (starting at 0).
 */
static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	SyStream *pStream;
	SyToken *pToken;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the current processed token */
	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);
	if( pToken == 0 ){
		/* Stream not yet processed */
		ph7_result_int(pCtx,0);
		return 0;
	}
	/* Point to the input stream */
	pStream = &pEngine->sParser.sLex.sStream;
	/* Return the byte index */
	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));
	return PH7_OK;
}
/*
 * bool xml_set_object(resource $parser,object &$object)
 *  Use XML Parser within an object.
 * NOTE
 *  This function is depreceated and is a no-op.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * $object
 *  The object where to use the XML parser.
 * Return
 * Always FALSE.
 */
static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_object(apArg[1]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/*  Throw a notice and return */ 
	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."
		"In order to mimic this behaviour,you can supply instead of a function name an array "
		"containing an object reference and a method name."
		);
	/* Return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * int xml_get_current_column_number(resource $parser)
 *  Gets the current column number of the given XML parser.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer to a valid parser, or else it returns
 *  which column on the current line (as given by xml_get_current_line_number()) the parser
 *  is currently at. 
 */
static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	SyStream *pStream;
	SyToken *pToken;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the current processed token */
	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);
	if( pToken == 0 ){
		/* Stream not yet processed */
		ph7_result_int(pCtx,0);
		return 0;
	}
	/* Point to the input stream */
	pStream = &pEngine->sParser.sLex.sStream;
	/* Return the byte index */
	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);
	return PH7_OK;
}
/*
 * int xml_get_error_code(resource $parser)
 *  Get XML parser error code.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer to a valid 
 *  parser, or else it returns one of the error codes listed in the error
 *  codes section.
 */
static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return the error code if any */
	ph7_result_int(pCtx,pEngine->iErrCode);
	return PH7_OK;
}
/*
 * XML parser event callbacks
 * Each time the unserlying XML parser extract a single token
 * from the input,one of the following callbacks are invoked.
 * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]
 */
/*
 * Create a scalar ph7_value holding the value
 * of an XML tag/attribute/CDATA and so on.
 */
static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)
{
	ph7_value *pValue;
	/* Allocate a new scalar variable */
	pValue = ph7_context_new_scalar(pEngine->pCtx);
	if( pValue == 0 ){
		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		return 0;
	}
	if( pNsUri && pNsUri->nByte > 0 ){
		/* Append namespace URI and the separator */
		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);
	}
	/* Copy the tag value */
	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);
	return pValue;
}
/*
 * Create a 'ph7_value' of type array holding the values
 * of an XML tag attributes.
 */
static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)
{
	ph7_value *pArray;
	/* Create an empty array */
	pArray = ph7_context_new_array(pEngine->pCtx);
	if( pArray == 0 ){
		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		return 0;
	}
	if( nAttr > 0 ){
		ph7_value *pKey,*pValue;
		sxu32 n;
		/* Create worker variables */
		pKey = ph7_context_new_scalar(pEngine->pCtx);
		pValue = ph7_context_new_scalar(pEngine->pCtx);
		if( pKey == 0 || pValue == 0 ){
			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			return 0;
		}
		/* Copy attributes */
		for( n = 0 ; n < nAttr ; n += 2 ){
			/* Reset string cursors */
			ph7_value_reset_string_cursor(pKey);
			ph7_value_reset_string_cursor(pValue);
			/* Copy attribute name and it's associated value */
			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */
			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */
			/* Insert in the array */
			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */
		}
		/* Release the worker variables */
		ph7_context_release_value(pEngine->pCtx,pKey);
		ph7_context_release_value(pEngine->pCtx,pValue);
	}
	/* Return the freshly created array */
	return pArray;
}
/*
 * Start element handler.
 * The user defined callback must accept three parameters:
 *    start_element_handler(resource $parser,string $name,array $attribs )
 *    $parser
 *      The first parameter, parser, is a reference to the XML parser calling the handler. 
 *    $name
 *      The second parameter, name, contains the name of the element for which this handler
 *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters. 
 *    $attribs
 *      The third parameter, attribs, contains an associative array with the element's attributes (if any).
 *		The keys of this array are the attribute names, the values are the attribute values.
 *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.
 *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().
 *      The first key in the array was the first attribute, and so on. 
 *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.
 */
static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pTag,*pAttr;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_START_TAG];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Create a ph7_value holding the tag name */
	pTag = VmXMLValue(pEngine,pStart,pNS);
	/* Create a ph7_value holding the tag attributes */
	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);
	if( pTag == 0  || pAttr == 0 ){
		SXUNUSED(pNS); /* cc warning */
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pTag);
	ph7_context_release_value(pEngine->pCtx,pAttr);
	return SXRET_OK;
}
/*
 * End element handler.
 * The user defined callback must accept two parameters:
 *  end_element_handler(resource $parser,string $name)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $name
 *   The second parameter, name, contains the name of the element for which this handler is called.
 *   If case-folding is in effect for this parser, the element name will be in uppercase letters.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 */
static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pTag;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_END_TAG];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Create a ph7_value holding the tag name */
	pTag = VmXMLValue(pEngine,pEnd,pNS);
	if( pTag == 0  ){
		SXUNUSED(pNS); /* cc warning */
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pTag);
	return SXRET_OK;
}
/*
 * Character data handler.
 *  The user defined callback must accept two parameters:
 *  handler(resource $parser,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $data
 *   The second parameter, data, contains the character data as a string. 
 *   Character data handler is called for every piece of a text in the XML document.
 *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).
 *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.
 *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.
 */
static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pData;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_CDATA];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Create a ph7_value holding the data */
	pData = VmXMLValue(pEngine,&(*pText),0);
	if( pData == 0  ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pData);
	return SXRET_OK;
}
/*
 * Processing instruction (PI) handler.
 * The user defined callback must accept two parameters:
 *   handler(resource $parser,string $target,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler. 
 *  $target
 *   The second parameter, target, contains the PI target. 
 *  $data
 *    The third parameter, data, contains the PI data. 
 *    Note: Instead of a function name, an array containing an object reference
 *    and a method name can also be supplied.
 */
static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pTarget,*pData;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_PI];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Get a ph7_value holding the data */
	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);
	pData = VmXMLValue(pEngine,&(*pDataStr),0);
	if( pTarget == 0 || pData == 0  ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pTarget);
	ph7_context_release_value(pEngine->pCtx,pData);
	return SXRET_OK;
}
/*
 * Namespace declaration handler.
 * The user defined callback must accept two parameters:
 *    handler(resource $parser,string $prefix,string $uri)
 * $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler. 
 * $prefix
 *   The prefix is a string used to reference the namespace within an XML object. 
 * $uri
 *   Uniform Resource Identifier (URI) of namespace. 
 *   Note: Instead of a function name, an array containing an object reference
 *   and a method name can also be supplied.
 */
static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pUri,*pPrefix;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_NS_START];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Get a ph7_value holding the PREFIX/URI */
	pUri = VmXMLValue(pEngine,pUriStr,0);
	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);
	if( pUri == 0 || pPrefix == 0  ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pUri);
	ph7_context_release_value(pEngine->pCtx,pPrefix);
	return SXRET_OK;
}
/*
 * Namespace end declaration handler.
 * The user defined callback must accept two parameters:
 *    handler(resource $parser,string $prefix)
 * $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 * $prefix
 *  The prefix is a string used to reference the namespace within an XML object.
 *   Note: Instead of a function name, an array containing an object reference
 *   and a method name can also be supplied.
 */
static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pPrefix;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_NS_END];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Get a ph7_value holding the prefix */
	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);
	if( pPrefix == 0 ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pPrefix);
	return SXRET_OK;
}
/*
 * Error Message consumer handler.
 * Each time the XML parser encounter a syntaxt error or any other error
 * related to XML processing,the following callback is invoked by the 
 * underlying XML parser.
 */
static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	/* Save the error code */
	pEngine->iErrCode = iErrCode;
	SXUNUSED(zMessage); /* cc warning */
	if( pToken ){
		pEngine->nLine = pToken->nLine;
	}
	/* Abort XML processing immediately */
	return SXERR_ABORT;
}
/*
 * int xml_parse(resource $parser,string $data[,bool $is_final = false ])
 *  Parses an XML document. The handlers for the configured events are called
 *  as many times as necessary.
 * Parameters
 *  $parser
 *   A reference to the XML parser.
 *  $data
 *   Chunk of data to parse. A document may be parsed piece-wise by calling
 *   xml_parse() several times with new data, as long as the is_final parameter
 *   is set and TRUE when the last data is parsed.
 * $is_final
 *   NOT USED. This implementation require that all the processed input be
 *   entirely loaded in memory.
 * Return
 *  Returns 1 on success or 0 on failure.
 */
static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	SyXMLParser *pParser;
	const char *zData;
	int nByte;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Ivalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pEngine->iNest > 0 ){
		/* This can happen when the user callback call xml_parse() again 
		 * in it's body which is forbidden.
		 */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,
			"Recursive call to %s,PH7 is returning false",
			ph7_function_name(pCtx)
			);
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pEngine->pCtx = pCtx;
	/* Point to the underlying XML parser */
	pParser = &pEngine->sParser;
	/* Register elements handler */
	SyXMLParserSetEventHandler(pParser,pEngine,
		VmXMLStartElementHandler,
		VmXMLTextHandler,
		VmXMLErrorHandler,
		0,
		VmXMLEndElementHandler,
		VmXMLPIHandler,
		0,
		0,
		VmXMLNSStartHandler,
		VmXMLNSEndHandler
		);
	pEngine->iErrCode = SXML_ERROR_NONE;
	/* Extract the raw XML input */
	zData = ph7_value_to_string(apArg[1],&nByte);
	/* Start the parse process */
	pEngine->iNest++;
	SyXMLProcess(pParser,zData,(sxu32)nByte);
	pEngine->iNest--;
	/* Return the parse result */
	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);
	return PH7_OK;
}
/*
 * bool xml_parser_set_option(resource $parser,int $option,mixed $value)
 *  Sets an option in an XML parser.
 * Parameters
 *  $parser
 *   A reference to the XML parser to set an option in.
 *  $option
 *    Which option to set. See below.
 *   The following options are available:
 *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.
 *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.
 *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.
 *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.
 * $value
 *   The option's new value.
 * Return
 *  Returns 1 on success or 0 on failure.
 * Note:
 *  Well,none of these options have meaning under the built-in XML parser so a call to this
 *  function is a no-op.
 */
static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Always return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * mixed xml_parser_get_option(resource $parser,int $option)
 *  Get options from an XML parser.
 * Parameters
 *  $parser
 *   A reference to the XML parser to set an option in.
 * $option
 *   Which option to fetch.
 * Return
 *  This function returns FALSE if parser does not refer to a valid parser
 *  or if option isn't valid.Else the option's value is returned.
 */
static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	int nOp;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the option */
	nOp = ph7_value_to_int(apArg[1]);
	switch(nOp){
	case SXML_OPTION_SKIP_TAGSTART:
	case SXML_OPTION_SKIP_WHITE:
	case SXML_OPTION_CASE_FOLDING:
		ph7_result_int(pCtx,0); break;
	case SXML_OPTION_TARGET_ENCODING:
		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);
		break;
	default:
		/* Unknown option,return FALSE*/
		ph7_result_bool(pCtx,0);
		break;
	}
	return PH7_OK;
}
/*
 * string xml_error_string(int $code)
 *  Gets the XML parser error string associated with the given code.
 * Parameters
 *  $code
 *   An error code from xml_get_error_code().
 * Return
 *  Returns a string with a textual description of the error 
 *  code, or FALSE if no description was found.
 */
static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nErr = -1;
	if( nArg > 0 ){
		nErr = ph7_value_to_int(apArg[0]);
	}
	switch(nErr){
	case SXML_ERROR_DUPLICATE_ATTRIBUTE:
		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);
		break;
	case SXML_ERROR_INCORRECT_ENCODING:
		ph7_result_string(pCtx,"Incorrect encoding",-1);
		break;
	case SXML_ERROR_INVALID_TOKEN:
		ph7_result_string(pCtx,"Unexpected token",-1);
		break;
	case SXML_ERROR_MISPLACED_XML_PI:
		ph7_result_string(pCtx,"Misplaced processing instruction",-1);
		break;
	case SXML_ERROR_NO_MEMORY:
		ph7_result_string(pCtx,"Out of memory",-1);
		break;
	case SXML_ERROR_NONE:
		ph7_result_string(pCtx,"Not an error",-1);
		break;
	case SXML_ERROR_TAG_MISMATCH:
		ph7_result_string(pCtx,"Tag mismatch",-1);
		break;
	case -1:
		ph7_result_string(pCtx,"Unknown error code",-1);
		break;
	default:
		ph7_result_string(pCtx,"Syntax error",-1);
		break;
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * int utf8_encode(string $input)
 *  UTF-8 encoding.
 *  This function encodes the string data to UTF-8, and returns the encoded version.
 *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values
 * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized
 * (meaning it is possible for a program to figure out where in the bytestream characters start)
 * and can be used with normal string comparison functions for sorting and such.
 *  Notes on UTF-8 (According to SQLite3 authors):
 *  Byte-0    Byte-1    Byte-2    Byte-3    Value
 *  0xxxxxxx                                 00000000 00000000 0xxxxxxx
 *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx
 *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx
 *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx
 * Parameters
 * $input
 *   String to encode or NULL on failure.
 * Return
 *  An UTF-8 encoded string.
 */
static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nByte,c,e;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zEnd = &zIn[nByte];
	/* Start the encoding process */
	for(;;){
		if( zIn >= zEnd ){
			/* End of input */
			break;
		}
		c = zIn[0];
		/* Advance the stream cursor */
		zIn++;
		/* Encode */
		if( c<0x00080 ){
			e = (c&0xFF);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}else if( c<0x00800 ){
			e = 0xC0 + ((c>>6)&0x1F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + (c & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}else if( c<0x10000 ){
			e = 0xE0 + ((c>>12)&0x0F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + ((c>>6) & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + (c & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}else{
			e = 0xF0 + ((c>>18) & 0x07);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + ((c>>12) & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + ((c>>6) & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + (c & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		} 
	}
	/* All done */
	return PH7_OK;
}
/*
 * UTF-8 decoding routine extracted from the sqlite3 source tree.
 * Original author: D. Richard Hipp (http://www.sqlite.org)
 * Status: Public Domain
 */
/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char UtfTrans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};
/*
** Translate a single UTF-8 character.  Return the unicode value.
**
** During translation, assume that the byte that zTerm points
** is a 0x00.
**
** Write a pointer to the next unread byte back into *pzNext.
**
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts an infinite number of different UTF8 encodings
**     for unicode values 0x80 and greater.  It do not change over-length
**     encodings to 0xfffd as some systems recommend.
*/
#define READ_UTF8(zIn, zTerm, c)                           \
  c = *(zIn++);                                            \
  if( c>=0xc0 ){                                           \
    c = UtfTrans1[c-0xc0];                                 \
    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \
  }
PH7_PRIVATE int PH7_Utf8Read(
  const unsigned char *z,         /* First byte of UTF-8 character */
  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */
  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */
){
  int c;
  READ_UTF8(z, zTerm, c);
  *pzNext = z;
  return c;
}
/*
 * string utf8_decode(string $data)
 *  This function decodes data, assumed to be UTF-8 encoded, to unicode.
 * Parameters
 * data
 *  An UTF-8 encoded string.
 * Return
 *  Unicode decoded string or NULL on failure.
 */
static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nByte,c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zEnd = &zIn[nByte];
	/* Start the decoding process */
	while( zIn < zEnd ){
		c = PH7_Utf8Read(zIn,zEnd,&zIn);
		if( c == 0x0 ){
			break;
		}
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	}
	return PH7_OK;
}
/* Table of built-in VM functions. */
static const ph7_builtin_func aVmFunc[] = {
	{ "func_num_args"  , vm_builtin_func_num_args },
	{ "func_get_arg"   , vm_builtin_func_get_arg  },
	{ "func_get_args"  , vm_builtin_func_get_args },
	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },
	{ "function_exists", vm_builtin_func_exists   },
	{ "is_callable"    , vm_builtin_is_callable   }, 
	{ "get_defined_functions", vm_builtin_get_defined_func },
	{ "register_shutdown_function",vm_builtin_register_shutdown_function },
	{ "call_user_func",        vm_builtin_call_user_func   },
	{ "call_user_func_array",  vm_builtin_call_user_func_array    },
	{ "forward_static_call",   vm_builtin_call_user_func   },
	{ "forward_static_call_array",vm_builtin_call_user_func_array },
	    /* Constants management */
	{ "defined",  vm_builtin_defined              },
	{ "define",   vm_builtin_define               },
	{ "constant", vm_builtin_constant             },
	{ "get_defined_constants", vm_builtin_get_defined_constants },
	   /* Class/Object functions */
	{ "class_alias",     vm_builtin_class_alias       },
	{ "class_exists",    vm_builtin_class_exists      },
	{ "property_exists", vm_builtin_property_exists   },
	{ "method_exists",   vm_builtin_method_exists     },
	{ "interface_exists",vm_builtin_interface_exists  },
	{ "get_class",       vm_builtin_get_class         },
	{ "get_parent_class",vm_builtin_get_parent_class  },
	{ "get_called_class",vm_builtin_get_called_class  },
	{ "get_declared_classes",    vm_builtin_get_declared_classes   },
	{ "get_defined_classes",     vm_builtin_get_declared_classes    },
	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},
	{ "get_class_methods",       vm_builtin_get_class_methods },
	{ "get_class_vars",          vm_builtin_get_class_vars    },
	{ "get_object_vars",         vm_builtin_get_object_vars   },
	{ "is_subclass_of",          vm_builtin_is_subclass_of    },
	{ "is_a", vm_builtin_is_a },
	   /* Random numbers/strings generators */
	{ "rand",          vm_builtin_rand            },
	{ "mt_rand",       vm_builtin_rand            },
	{ "rand_str",      vm_builtin_rand_str        },
	{ "getrandmax",    vm_builtin_getrandmax      },
	{ "mt_getrandmax", vm_builtin_getrandmax      },
#ifndef PH7_DISABLE_BUILTIN_FUNC
#if !defined(PH7_DISABLE_HASH_FUNC)
	{ "uniqid",        vm_builtin_uniqid          },
#endif /* PH7_DISABLE_HASH_FUNC */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	   /* Language constructs functions */
	{ "echo",  vm_builtin_echo                    },
	{ "print", vm_builtin_print                   },
	{ "exit",  vm_builtin_exit                    },
	{ "die",   vm_builtin_exit                    },
	{ "eval",  vm_builtin_eval                    },
	  /* Variable handling functions */
	{ "get_defined_vars",vm_builtin_get_defined_vars},
	{ "gettype",   vm_builtin_gettype              },
	{ "get_resource_type", vm_builtin_get_resource_type},
	{ "isset",     vm_builtin_isset                },
	{ "unset",     vm_builtin_unset                },
	{ "var_dump",  vm_builtin_var_dump             },
	{ "print_r",   vm_builtin_print_r              },
	{ "var_export",vm_builtin_var_export           },
	  /* Ouput control functions */
	{ "flush",        vm_builtin_ob_flush          },
	{ "ob_clean",     vm_builtin_ob_clean          },
	{ "ob_end_clean", vm_builtin_ob_end_clean      },
	{ "ob_end_flush", vm_builtin_ob_end_flush      },
	{ "ob_flush",     vm_builtin_ob_flush          },
	{ "ob_get_clean", vm_builtin_ob_get_clean      },
	{ "ob_get_contents", vm_builtin_ob_get_contents},
	{ "ob_get_flush",    vm_builtin_ob_get_clean   },
	{ "ob_get_length",   vm_builtin_ob_get_length  },
	{ "ob_get_level",    vm_builtin_ob_get_level   },
	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},
	{ "ob_get_level",      vm_builtin_ob_get_level },
	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },
	{ "ob_start",          vm_builtin_ob_start     },
	  /* Assertion functions */
	{ "assert_options",  vm_builtin_assert_options },
	{ "assert",          vm_builtin_assert         },
	  /* Error reporting functions */
	{ "trigger_error",vm_builtin_trigger_error     },
	{ "user_error",   vm_builtin_trigger_error     },
	{ "error_reporting",vm_builtin_error_reporting },
	{ "error_log",       vm_builtin_error_log      },
	{ "restore_exception_handler", vm_builtin_restore_exception_handler },
	{ "set_exception_handler",     vm_builtin_set_exception_handler     },
	{ "restore_error_handler", vm_builtin_restore_error_handler },
	{ "set_error_handler",vm_builtin_set_error_handler },
	{ "debug_backtrace",  vm_builtin_debug_backtrace},
	{ "error_get_last" ,  vm_builtin_debug_backtrace },
	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },
	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },
	  /* Release info */
	{"ph7version",       vm_builtin_ph7_version  },
	{"ph7credits",       vm_builtin_ph7_credits  },
	{"ph7info",          vm_builtin_ph7_credits  },
	{"ph7_info",         vm_builtin_ph7_credits  },
	{"phpinfo",          vm_builtin_ph7_credits  },
	{"ph7copyright",     vm_builtin_ph7_credits  },
	  /* hashmap */
	{"compact",          vm_builtin_compact       },
	{"extract",          vm_builtin_extract       },
	{"import_request_variables", vm_builtin_import_request_variables},
	  /* URL related function */
	{"parse_url",        vm_builtin_parse_url     },
	 /* Refer to 'builtin.c' for others string processing functions. */
#ifndef PH7_DISABLE_BUILTIN_FUNC
	   /* XML processing functions */
	{"xml_parser_create",        vm_builtin_xml_parser_create   },
	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},
	{"xml_parser_free",          vm_builtin_xml_parser_free     },
	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},
	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},
	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },
	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},
	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},
	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},
	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},
	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},
	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},
	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},
	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },
	{"xml_set_object",               vm_builtin_xml_set_object},
	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},
	{"xml_get_error_code",           vm_builtin_xml_get_error_code },
	{"xml_parse",                    vm_builtin_xml_parse },
	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},
	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},
	{"xml_error_string",             vm_builtin_xml_error_string     },
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	   /* UTF-8 encoding/decoding */
	{"utf8_encode",    vm_builtin_utf8_encode},
	{"utf8_decode",    vm_builtin_utf8_decode},
	   /* Command line processing */
	{"getopt",         vm_builtin_getopt     },
	   /* JSON encoding/decoding */
	{"json_encode",    vm_builtin_json_encode },
	{"json_last_error",vm_builtin_json_last_error},
	{"json_decode",    vm_builtin_json_decode },
	{"serialize",      vm_builtin_json_encode },
	{"unserialize",    vm_builtin_json_decode },
	   /* Files/URI inclusion facility */
	{ "get_include_path",  vm_builtin_get_include_path },
	{ "get_included_files",vm_builtin_get_included_files}, 
	{ "include",      vm_builtin_include          },
	{ "include_once", vm_builtin_include_once     },
	{ "require",      vm_builtin_require          },
	{ "require_once", vm_builtin_require_once     },
};
/*
 * Register the built-in VM functions defined above.
 */
static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)
{
	sxi32 rc;
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){
		/* Note that these special functions have access
		 * to the underlying virtual machine as their
		 * private data.
		 */
		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return SXRET_OK;
}
/*
 * Check if the given name refer to an installed class.
 * Return a pointer to that class on success. NULL on failure.
 */
PH7_PRIVATE ph7_class * PH7_VmExtractClass(
	ph7_vm *pVm,        /* Target VM */
	const char *zName,  /* Name of the target class */
	sxu32 nByte,        /* zName length */
	sxi32 iLoadable,    /* TRUE to return only loadable class
						 * [i.e: no abstract classes or interfaces]
						 */
	sxi32 iNest         /* Nesting level (Not used) */
	)
{
	SyHashEntry *pEntry;
	ph7_class *pClass;
	/* Perform a hash lookup */
	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);
	
	if( pEntry == 0 ){
		/* No such entry,return NULL */
		iNest = 0; /* cc warning */
		return 0;
	}
	pClass = (ph7_class *)pEntry->pUserData;
	if( !iLoadable ){
		/* Return the first class seen */
		return pClass;
	}else{
		/* Check the collision list */
		while(pClass){
			if( (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT)) == 0 ){
				/* Class is loadable */
				return pClass;
			}
			/* Point to the next entry */
			pClass = pClass->pNextName;
		}
	}
	/* No such loadable class */
	return 0;
}
/*
 * Reference Table Implementation 
 * Status: stable <chm@symisc.net>
 * Intro
 *  The implementation of the reference mechanism in the PH7 engine
 *  differ greatly from the one used by the zend engine. That is,
 *  the reference implementation is consistent,solid and it's
 *  behavior resemble the C++ reference mechanism.
 *  Refer to the official for more information on this powerful
 *  extension.
 */
/*
 * Allocate a new reference entry.
 */
static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)
{
	VmRefObj *pRef;
	/* Allocate a new instance */
	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));
	if( pRef == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pRef,sizeof(VmRefObj));
	/* Initialize fields */
	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));
	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));
	pRef->nIdx = nIdx;
	return pRef;
}
/*
 * Default hash function used by the reference table 
 * for lookup/insertion operations.
 */
static sxu32 VmRefHash(sxu32 nIdx)
{
	/* Calculate the hash based on the memory object index */
	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);
}
/*
 * Check if a memory object [i.e: a variable] is already installed
 * in the reference table.
 * Return a pointer to the entry (VmRefObj instance) on success.NULL
 * otherwise.
 * The implementation of the reference mechanism in the PH7 engine
 * differ greatly from the one used by the zend engine. That is,
 * the reference implementation is consistent,solid and it's
 * behavior resemble the C++ reference mechanism.
 * Refer to the official for more information on this powerful
 * extension.
 */
static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)
{
	VmRefObj *pRef;
	sxu32 nBucket;
	/* Point to the appropriate bucket */
	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);
	/* Perform the lookup */
	pRef = pVm->apRefObj[nBucket];
	for(;;){
		if( pRef == 0 ){
			break;
		}
		if( pRef->nIdx == nObjIdx ){
			/* Entry found */
			return pRef;
		}
		/* Point to the next entry */
		pRef = pRef->pNextCollide;
	}
	/* No such entry,return NULL */
	return 0;
}
/*
 * Install a memory object [i.e: a variable] in the reference table.
 *
 * The implementation of the reference mechanism in the PH7 engine
 * differ greatly from the one used by the zend engine. That is,
 * the reference implementation is consistent,solid and it's
 * behavior resemble the C++ reference mechanism.
 * Refer to the official for more information on this powerful
 * extension.
 */
static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)
{
	sxu32 nBucket;
	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){
		VmRefObj **apNew;
		sxu32 nNew;
		/* Allocate a larger table */
		nNew = pVm->nRefSize << 1;
		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);
		if( apNew ){
			VmRefObj *pEntry = pVm->pRefList;
			sxu32 n;
			/* Zero the structure */
			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));
			/* Rehash all referenced entries */
			for( n = 0 ; n < pVm->nRefUsed ; ++n ){
				/* Remove old collision links */
				pEntry->pNextCollide = pEntry->pPrevCollide = 0;
				/* Point to the appropriate bucket */
				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);
				/* Insert the entry  */
				pEntry->pNextCollide = apNew[nBucket];
				if( apNew[nBucket] ){
					apNew[nBucket]->pPrevCollide = pEntry;
				}
				apNew[nBucket] = pEntry;
				/* Point to the next entry */
				pEntry = pEntry->pNext;
			}
			/* Release the old table */
			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);
			/* Install the new one */
			pVm->apRefObj = apNew;
			pVm->nRefSize = nNew;
		}
	}
	/* Point to the appropriate bucket */
	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);
	/* Insert the entry */
	pRef->pNextCollide = pVm->apRefObj[nBucket];
	if( pVm->apRefObj[nBucket] ){
		pVm->apRefObj[nBucket]->pPrevCollide = pRef;
	}
	pVm->apRefObj[nBucket] = pRef;
	MACRO_LD_PUSH(pVm->pRefList,pRef);
	pVm->nRefUsed++;
	return SXRET_OK;
}
/*
 * Destroy a memory object [i.e: a variable] and remove it from
 * the reference table.
 * This function is invoked when the user perform an unset
 * call [i.e: unset($var); ].
 * The implementation of the reference mechanism in the PH7 engine
 * differ greatly from the one used by the zend engine. That is,
 * the reference implementation is consistent,solid and it's
 * behavior resemble the C++ reference mechanism.
 * Refer to the official for more information on this powerful
 * extension.
 */
static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)
{
	ph7_hashmap_node **apNode;
	SyHashEntry **apEntry;
	sxu32 n;
	/* Point to the reference table */
	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);
	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);
	/* Unlink the entry from the reference table */
	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){
		if( apEntry[n] ){
			SyHashDeleteEntry2(apEntry[n]);
		}
	}
	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){
		if( apNode[n] ){
			PH7_HashmapUnlinkNode(apNode[n],FALSE);
		}
	}
	if( pRef->pPrevCollide ){
		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;
	}else{
		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;
	}
	if( pRef->pNextCollide ){
		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;
	}
	MACRO_LD_REMOVE(pVm->pRefList,pRef);
	/* Release the node */
	SySetRelease(&pRef->aReference);
	SySetRelease(&pRef->aArrEntries);
	SyMemBackendPoolFree(&pVm->sAllocator,pRef);
	pVm->nRefUsed--;
	return SXRET_OK;
}
/*
 * Install a memory object [i.e: a variable] in the reference table.
 * The implementation of the reference mechanism in the PH7 engine
 * differ greatly from the one used by the zend engine. That is,
 * the reference implementation is consistent,solid and it's
 * behavior resemble the C++ reference mechanism.
 * Refer to the official for more information on this powerful
 * extension.
 */
PH7_PRIVATE sxi32 PH7_VmRefObjInstall(
	ph7_vm *pVm,                 /* Target VM */
	sxu32 nIdx,                  /* Memory object index in the global object pool */
	SyHashEntry *pEntry,         /* Hash entry of this object */
	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */
	sxi32 iFlags                 /* Control flags */ 
	)
{
	VmFrame *pFrame = pVm->pFrame;
	VmRefObj *pRef;
	/* Check if the referenced object already exists */
	pRef = VmRefObjExtract(&(*pVm),nIdx);
	if( pRef == 0 ){
		/* Create a new entry */
		pRef = VmNewRefObj(&(*pVm),nIdx);
		if( pRef == 0 ){
			return SXERR_MEM;
		}
		pRef->iFlags = iFlags;
		/* Install the entry */
		VmRefObjInsert(&(*pVm),pRef);
	}
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( pFrame->pParent != 0 && pEntry ){
		VmSlot sRef;
		/* Local frame,record referenced entry so that it can
		 * be deleted when we leave this frame.
		 */
		sRef.nIdx = nIdx;
		sRef.pUserData = pEntry;
		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {
			pEntry = 0; /* Do not record this entry */
		}
	}
	if( pEntry ){
		/* Address of the hash-entry */
		SySetPut(&pRef->aReference,(const void *)&pEntry);
	}
	if( pMapEntry ){
		/* Address of the hashmap node [i.e: Array entry] */
		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);
	}
	return SXRET_OK;
}
/*
 * Remove a memory object [i.e: a variable] from the reference table.
 * The implementation of the reference mechanism in the PH7 engine
 * differ greatly from the one used by the zend engine. That is,
 * the reference implementation is consistent,solid and it's
 * behavior resemble the C++ reference mechanism.
 * Refer to the official for more information on this powerful
 * extension.
 */
PH7_PRIVATE sxi32 PH7_VmRefObjRemove(
	ph7_vm *pVm,                 /* Target VM */
	sxu32 nIdx,                  /* Memory object index in the global object pool */
	SyHashEntry *pEntry,         /* Hash entry of this object */
	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */
	)
{
	VmRefObj *pRef;
	sxu32 n;
	/* Check if the referenced object already exists */
	pRef = VmRefObjExtract(&(*pVm),nIdx);
	if( pRef == 0 ){
		/* Not such entry */
		return SXERR_NOTFOUND;
	}
	/* Remove the desired entry */
	if( pEntry ){
		SyHashEntry **apEntry;
		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);
		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){
			if( apEntry[n] == pEntry ){
				/* Nullify the entry */
				apEntry[n] = 0;
				/*
				 * NOTE:
				 * In future releases,think to add a free pool of entries,so that
				 * we avoid wasting spaces. 
				 */
			}
		}
	}
	if( pMapEntry ){
		ph7_hashmap_node **apNode;
		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);
		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){
			if( apNode[n] == pMapEntry ){
				/* nullify the entry */
				apNode[n] = 0;
			}
		}
	}
	return SXRET_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * Extract the IO stream device associated with a given scheme.
 * Return a pointer to an instance of ph7_io_stream when the scheme
 * have an associated IO stream registered with it. NULL otherwise.
 * If no scheme:// is avalilable then the file:// scheme is assumed.
 * For more information on how to register IO stream devices,please
 * refer to the official documentation.
 */
PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(
	ph7_vm *pVm,           /* Target VM */
	const char **pzDevice, /* Full path,URI,... */
	int nByte              /* *pzDevice length*/
	)
{
	const char *zIn,*zEnd,*zCur,*zNext;
	ph7_io_stream **apStream,*pStream;
	SyString sDev,sCur;
	sxu32 n,nEntry;
	int rc;
	/* Check if a scheme [i.e: file://,http://,zip://...] is available */
	zNext = zCur = zIn = *pzDevice;
	zEnd = &zIn[nByte];
	while( zIn < zEnd ){
		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){
			/* Got one */
			zNext = &zIn[sizeof("://")-1];
			break;
		}
		/* Advance the cursor */
		zIn++;
	}
	if( zIn >= zEnd ){
		/* No such scheme,return the default stream */
		return pVm->pDefStream;
	}
	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);
	/* Remove leading and trailing white spaces */
	SyStringFullTrim(&sDev);
	/* Perform a linear lookup on the installed stream devices */
	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);
	nEntry = SySetUsed(&pVm->aIOstream);
	for( n = 0 ; n < nEntry ; n++ ){
		pStream = apStream[n];
		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));
		/* Perfrom a case-insensitive comparison */
		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);
		if( rc == 0 ){
			/* Stream device found */
			*pzDevice = zNext;
			return pStream;
		}
	}
	/* No such stream,return NULL */
	return 0;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Section:
 *    HTTP/URI related routines.
 * Status:
 *    Stable.
 */ 
 /*
  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].
  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]
  * This almost, but not quite, RFC1738 URI syntax.
  * This routine is not a validator,it does not check for validity
  * nor decode URI parts,the only thing this routine does is splitting
  * the input to its fields.
  * Upper layer are responsible of decoding and validating URI parts.
  * On success,this function populate the "SyhttpUri" structure passed
  * as the first argument. Otherwise SXERR_* is returned when a malformed
  * input is encountered.
  */
 static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)
 {
	 const char *zEnd = &zUri[nLen];
	 sxu8 bHostOnly = FALSE;
	 sxu8 bIPv6 = FALSE	; 
	 const char *zCur;
	 SyString *pComp;
	 sxu32 nPos = 0;
	 sxi32 rc;
	 /* Zero the structure first */
	 SyZero(pOut,sizeof(SyhttpUri));
	 /* Remove leading and trailing white spaces  */
	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);
	 SyStringFullTrim(&pOut->sRaw);
	 /* Find the first '/' separator */
	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);
	 if( rc != SXRET_OK ){
		 /* Assume a host name only */
		 zCur = zEnd;
		 bHostOnly = TRUE;
		 goto ProcessHost;
	 }
	 zCur = &zUri[nPos];
	 if( zUri != zCur && zCur[-1] == ':' ){
		 /* Extract a scheme:
		  * Not that we can get an invalid scheme here.
		  * Fortunately the caller can discard any URI by comparing this scheme with its 
		  * registered schemes and will report the error as soon as his comparison function
		  * fail.
		  */
	 	pComp = &pOut->sScheme;
		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));
		SyStringLeftTrim(pComp);		
	 }
	 if( zCur[1] != '/' ){
		 if( zCur == zUri || zCur[-1] == ':' ){
		  /* No authority */
		  goto PathSplit;
		}
		 /* There is something here , we will assume its an authority
		  * and someone has forgot the two prefix slashes "//",
		  * sooner or later we will detect if we are dealing with a malicious
		  * user or not,but now assume we are dealing with an authority
		  * and let the caller handle all the validation process.
		  */
		 goto ProcessHost;
	 }	 
	 zUri = &zCur[2];
	 zCur = zEnd;
	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);
	 if( rc == SXRET_OK ){
		 zCur = &zUri[nPos];
	 }
 ProcessHost:
	 /* Extract user information if present */
	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);
	 if( rc == SXRET_OK ){
		 if( nPos > 0 ){
			 sxu32 nPassOfft; /* Password offset */
			 pComp = &pOut->sUser;
			 SyStringInitFromBuf(pComp,zUri,nPos);
			 /* Extract the password if available */
			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);
			 if( rc == SXRET_OK && nPassOfft < nPos){
				 pComp->nByte = nPassOfft;
				 pComp = &pOut->sPass;
				 pComp->zString = &zUri[nPassOfft+sizeof(char)];
				 pComp->nByte = nPos - nPassOfft - 1;
			 }
			 /* Update the cursor */
			 zUri = &zUri[nPos+1];
		 }else{
			 zUri++;
		 }
	 }
	 pComp = &pOut->sHost;
	 while( zUri < zCur && SyisSpace(zUri[0])){
		 zUri++;
	 }	
	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));
	 if( pComp->zString[0] == '[' ){
		 /* An IPv6 Address: Make a simple naive test
		  */
		 zUri++; pComp->zString++; pComp->nByte = 0;
		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) || zUri[0] == ':' ){
			 zUri++; pComp->nByte++;
		 }
		 if( zUri[0] != ']' ){
			 return SXERR_CORRUPT; /* Malformed IPv6 address */
		 }
		 zUri++;
		 bIPv6 = TRUE;
	 }
	 /* Extract a port number if available */
	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);
	 if( rc == SXRET_OK ){
		 if( bIPv6 == FALSE ){
			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);
		 }
		 pComp = &pOut->sPort;
		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));	
	 }
	 if( bHostOnly == TRUE ){
		 return SXRET_OK;
	 }
PathSplit:
	 zUri = zCur;
	 pComp = &pOut->sPath;
	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));
	 if( pComp->nByte == 0 ){
		 return SXRET_OK; /* Empty path */
	 }
	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){
		 pComp->nByte = nPos; /* Update path length */
		 pComp = &pOut->sQuery;
		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));
	 }
	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){
		 /* Update path or query length */
		 if( pComp == &pOut->sPath ){
			 pComp->nByte = nPos;
		 }else{
			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){
				 /* Malformed syntax : Query must be present before fragment */
				 return SXERR_SYNTAX;
			 }
			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);
		 }
		 pComp = &pOut->sFragment;
		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))
	 }
	 return SXRET_OK;
 }
 /*
 * Extract a single line from a raw HTTP request.
 * Return SXRET_OK on success,SXERR_EOF when end of input
 * and SXERR_MORE when more input is needed.
 */
static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)
{ 
  	const char *zIn;
  	sxu32 nPos; 
	/* Jump leading white spaces */
	SyStringLeftTrim(pCursor);
	if( pCursor->nByte < 1 ){
		SyStringInitFromBuf(pCurrent,0,0);
		return SXERR_EOF; /* End of input */
	}
	zIn = SyStringData(pCursor);
	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){
		/* Line not found,tell the caller to read more input from source */
		SyStringDupPtr(pCurrent,pCursor);
		return SXERR_MORE;
	}
  	pCurrent->zString = zIn;
  	pCurrent->nByte	= nPos;	
  	/* advance the cursor so we can call this routine again */
  	pCursor->zString = &zIn[nPos];
  	pCursor->nByte -= nPos;
  	return SXRET_OK;
 }
 /*
  * Split a single MIME header into a name value pair. 
  * This function return SXRET_OK,SXERR_CONTINUE on success.
  * Otherwise SXERR_NEXT is returned when a malformed header
  * is encountered.
  * Note: This function handle also mult-line headers.
  */
 static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)
 {
	 SyString *pName;
	 sxu32 nPos;
	 sxi32 rc;
	 if( nLen < 1 ){
		 return SXERR_NEXT;
	 }
	 /* Check for multi-line header */
	if( pLast && (zLine[-1] == ' ' || zLine[-1] == '\t') ){
		SyString *pTmp = &pLast->sValue;
		SyStringFullTrim(pTmp);
		if( pTmp->nByte == 0 ){
			SyStringInitFromBuf(pTmp,zLine,nLen);
		}else{
			/* Update header value length */
			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);
		}
		 /* Simply tell the caller to reset its states and get another line */
		 return SXERR_CONTINUE;
	 }
	/* Split the header */
	pName = &pHdr->sName;
	rc = SyByteFind(zLine,nLen,':',&nPos);
	if(rc != SXRET_OK ){
		return SXERR_NEXT; /* Malformed header;Check the next entry */
	}
	SyStringInitFromBuf(pName,zLine,nPos);
	SyStringFullTrim(pName);
	/* Extract a header value */
	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);
	/* Remove leading and trailing whitespaces */
	SyStringFullTrim(&pHdr->sValue);
	return SXRET_OK;
 }
 /*
  * Extract all MIME headers associated with a HTTP request.
  * After processing the first line of a HTTP request,the following
  * routine is called in order to extract MIME headers.
  * This function return SXRET_OK on success,SXERR_MORE when it needs
  * more inputs.
  * Note: Any malformed header is simply discarded.
  */
 static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)
 {
	 SyhttpHeader *pLast = 0;
	 SyString sCurrent;
	 SyhttpHeader sHdr;
	 sxu8 bEol;
	 sxi32 rc;
	 if( SySetUsed(pOut) > 0 ){
		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);
	 }
	 bEol = FALSE;
	 for(;;){
		 SyZero(&sHdr,sizeof(SyhttpHeader));
		 /* Extract a single line from the raw HTTP request */
		 rc = VmGetNextLine(pRequest,&sCurrent);
		 if(rc != SXRET_OK ){
			 if( sCurrent.nByte < 1 ){
				 break;
			 }
			 bEol = TRUE;
		 }
		 /* Process the header */
		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){
			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){
				 break;
			 }
			 /* Retrieve the last parsed header so we can handle multi-line header
			  * in case we face one of them.
			  */
			 pLast = (SyhttpHeader *)SySetPeek(pOut);
		 }
		 if( bEol ){
			 break;
		 }
	 } /* for(;;) */
	 return SXRET_OK;
 }
 /*
  * Process the first line of a HTTP request.
  * This routine perform the following operations
  *  1) Extract the HTTP method.
  *  2) Split the request URI to it's fields [ie: host,path,query,...].
  *  3) Extract the HTTP protocol version.
  */
 static sxi32 VmHttpProcessFirstLine(
	 SyString *pRequest, /* Raw HTTP request */
	 sxi32 *pMethod,     /* OUT: HTTP method */
	 SyhttpUri *pUri,    /* OUT: Parse of the URI */
	 sxi32 *pProto       /* OUT: HTTP protocol */
	 )
 {
	 static const char *azMethods[] = { "get","post","head","put"};
	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};
	 const char *zIn,*zEnd,*zPtr;
	 SyString sLine;
	 sxu32 nLen;
	 sxi32 rc;
	 /* Extract the first line and update the pointer */
	 rc = VmGetNextLine(pRequest,&sLine);
	 if( rc != SXRET_OK ){
		 return rc;
	 }
	 if ( sLine.nByte < 1 ){
		 /* Empty HTTP request */
		 return SXERR_EMPTY;
	 }
	 /* Delimit the line and ignore trailing and leading white spaces */
	 zIn = sLine.zString;
	 zEnd = &zIn[sLine.nByte];
	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 /* Extract the HTTP method */
	 zPtr = zIn;
	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 *pMethod = HTTP_METHOD_OTHR;
	 if( zIn > zPtr ){
		 sxu32 i;
		 nLen = (sxu32)(zIn-zPtr);
		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){
			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){
				 *pMethod = aMethods[i];
				 break;
			 }
		 }
	 }
	 /* Jump trailing white spaces */
	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	  /* Extract the request URI */
	 zPtr = zIn;
	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){
		 zIn++;
	 } 
	 if( zIn > zPtr ){
		 nLen = (sxu32)(zIn-zPtr);
		 /* Split raw URI to it's fields */
		 VmHttpSplitURI(pUri,zPtr,nLen);
	 }
	 /* Jump trailing white spaces */
	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 /* Extract the HTTP version */
	 zPtr = zIn;
	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */
	 rc = 1;
	 if( zIn > zPtr ){
		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));
	 }
	 if( !rc ){
		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */
	 }
	 return SXRET_OK;
 }
 /*
  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded" 
  * into a name value pair.
  * Note that this encoding is implicit in GET based requests.
  * After the tokenization process,register the decoded queries
  * in the $_GET/$_POST/$_REQUEST superglobals arrays.
  */
 static sxi32 VmHttpSplitEncodedQuery(
	 ph7_vm *pVm,       /* Target VM */
	 SyString *pQuery,  /* Raw query to decode */
	 SyBlob *pWorker,   /* Working buffer */
	 int is_post        /* TRUE if we are dealing with a POST request */
	 )
 {
	 const char *zEnd = &pQuery->zString[pQuery->nByte];
	 const char *zIn = pQuery->zString;
	 ph7_value *pGet,*pRequest;
	 SyString sName,sValue;
	 const char *zPtr;
	 sxu32 nBlobOfft;
	 /* Extract superglobals */
	 if( is_post ){
		 /* $_POST superglobal */
		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);
	 }else{
		 /* $_GET superglobal */
		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);
	 }
	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);
	 /* Split up the raw query */
	 for(;;){
		 /* Jump leading white spaces */
		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){
			 zIn++;
		 }
		 if( zIn >= zEnd ){
			 break;
		 }
		 zPtr = zIn;
		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){
			 zPtr++;
		 }
		 /* Reset the working buffer */
		 SyBlobReset(pWorker);
		 /* Decode the entry */
		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);
		 /* Save the entry */
		 sName.nByte = SyBlobLength(pWorker);
		 sValue.zString = 0;
		 sValue.nByte = 0;
		 if( zPtr < zEnd && zPtr[0] == '=' ){
			 zPtr++;
			 zIn = zPtr;
			 /* Store field value */
			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){
				 zPtr++;
			 }
			 if( zPtr > zIn ){
				 /* Decode the value */
				  nBlobOfft = SyBlobLength(pWorker);
				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);
				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);
				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;
				 
			 }
			 /* Synchronize pointers */
			 zIn = zPtr;
		 }
		 sName.zString = (const char *)SyBlobData(pWorker);
		 /* Install the decoded query in the $_GET/$_REQUEST array */
		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){
			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,
				 sName.zString,(int)sName.nByte,
				 sValue.zString,(int)sValue.nByte
				 );
		 }
		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){
			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,
				 sName.zString,(int)sName.nByte,
				 sValue.zString,(int)sValue.nByte
					 );
		 }
		 /* Advance the pointer */
		 zIn = &zPtr[1];
	 }
	/* All done*/
	return SXRET_OK;
 }
 /*
  * Extract MIME header value from the given set.
  * Return header value on success. NULL otherwise.
  */
 static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)
 {
	 SyhttpHeader *aMime,*pMime;
	 SyString sMime;
	 sxu32 n;
	 SyStringInitFromBuf(&sMime,zMime,nByte);
	 /* Point to the MIME entries */
	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);
	 /* Perform the lookup */
	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){
		 pMime = &aMime[n];
		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){
			 /* Header found,return it's associated value */
			 return &pMime->sValue;
		 }
	 }
	 /* No such MIME header */
	 return 0;
 }
 /*
  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair
  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.
  */
 static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)
 {
	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];
	 SyString sName,sValue;
	 ph7_value *pCookie;
	 sxu32 nOfft;
	 /* Make sure the $_COOKIE superglobal is available */
	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);
	 if( pCookie == 0 || (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){
		 /* $_COOKIE superglobal not available */
		 return SXERR_NOTFOUND;
	 }	
	 for(;;){
		  /* Jump leading white spaces */
		 while( zIn < zEnd && SyisSpace(zIn[0]) ){
			 zIn++;
		 }
		 if( zIn >= zEnd ){
			 break;
		 }
		  /* Reset the working buffer */
		 SyBlobReset(pWorker);
		 zDelimiter = zIn;
		 /* Delimit the name[=value]; pair */ 
		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){
			 zDelimiter++;
		 }
		 zPtr = zIn;
		 while( zPtr < zDelimiter && zPtr[0] != '=' ){
			 zPtr++;
		 }
		 /* Decode the cookie */
		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);
		 sName.nByte = SyBlobLength(pWorker);
		 zPtr++;
		 sValue.zString = 0;
		 sValue.nByte = 0;
		 if( zPtr < zDelimiter ){
			 /* Got a Cookie value */
			 nOfft = SyBlobLength(pWorker);
			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);
			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);
		 }
		 /* Synchronize pointers */
		 zIn = &zDelimiter[1];
		 /* Perform the insertion */
		 sName.zString = (const char *)SyBlobData(pWorker);
		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,
			 sName.zString,(int)sName.nByte,
			 sValue.zString,(int)sValue.nByte
			 );
	 }
	 return SXRET_OK;
 }
 /*
  * Process a full HTTP request and populate the appropriate arrays
  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information
  * extracted from the raw HTTP request. As an extension Symisc introduced 
  * the $_HEADER array which hold a copy of the processed HTTP MIME headers
  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].
  * This function return SXRET_OK on success. Any other return value indicates
  * a malformed HTTP request.
  */
 static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)
 {
	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */
	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/
	 SyhttpHeader *pHeader;            /* MIME header */
	 SyhttpUri sUri;     /* Parse of the raw URI*/
	 SyBlob sWorker;     /* General purpose working buffer */
	 SySet sHeader;      /* MIME headers set */
	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/
	 sxi32 iVer;         /* HTTP protocol version */
	 sxi32 rc;
	 SyStringInitFromBuf(&sRequest,zRequest,nByte);
	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));
	 SyBlobInit(&sWorker,&pVm->sAllocator);
	 /* Ignore leading and trailing white spaces*/
	 SyStringFullTrim(&sRequest);
	 /* Process the first line */
	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);
	 if( rc != SXRET_OK ){
		 return rc;
	 }
	 /* Process MIME headers */
	 VmHttpExtractHeaders(&sRequest,&sHeader);
	 /*
	  * Setup $_SERVER environments 
	  */
	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "SERVER_PROTOCOL",
		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",
		 sizeof("HTTP/1.1")-1
		 );
	 /* 'REQUEST_METHOD':  Which request method was used to access the page */
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "REQUEST_METHOD",
		 iMethod == HTTP_METHOD_GET ?   "GET" : 
		 (iMethod == HTTP_METHOD_POST ? "POST":
		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :
		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),
		 -1 /* Compute attribute length automatically */
		 );
	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){
		 pValue = &sUri.sQuery;
		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "QUERY_STRING",
			 pValue->zString,
			 pValue->nByte
			 );
		 /* Decoded the raw query */
		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);
	 }
	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */
	 pValue = &sUri.sRaw;
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "REQUEST_URI",
		 pValue->zString,
		 pValue->nByte
		 );
	 /*
	  * 'PATH_INFO'
	  * 'ORIG_PATH_INFO' 
      * Contains any client-provided pathname information trailing the actual script filename but preceding
	  * the query string, if available. For instance, if the current script was accessed via the URL
	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain
	  * /some/stuff. 
	  */
	 pValue = &sUri.sPath;
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "PATH_INFO",
		 pValue->zString,
		 pValue->nByte
		 );
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "ORIG_PATH_INFO",
		 pValue->zString,
		 pValue->nByte
		 );
	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT_CHARSET",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT_ENCODING",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT_LANGUAGE",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_CONNECTION",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_HOST",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_REFERER",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_USER_AGENT",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'
	   * header sent by the client (which you should then use to make the appropriate validation).
	   */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "PHP_AUTH_DIGEST",
			 pValue->zString,
			 pValue->nByte
		 );
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "PHP_AUTH",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* Install all clients HTTP headers in the $_HEADER superglobal */
	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);
	 /* Iterate throw the available MIME headers*/
	 SySetResetCursor(&sHeader);
	 pHeader = 0; /* stupid cc warning */
	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){
		 pName  = &pHeader->sName;
		 pValue = &pHeader->sValue;
		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){
			 /* Insert the MIME header and it's associated value */
			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,
				 pName->zString,(int)pName->nByte,
				 pValue->zString,(int)pValue->nByte
				 );
		 }
		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0 
			 && pValue->nByte > 0){
				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */
				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);
		 }
	 }
	 if( iMethod == HTTP_METHOD_POST ){
		 /* Extract raw POST data */
		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);
		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&
			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){
				 /* Extract POST data length */
				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);
				 if( pValue ){
					 sxi32 iLen = 0; /* POST data length */
					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);
					 if( iLen > 0 ){
						 /* Remove leading and trailing white spaces */
						 SyStringFullTrim(&sRequest);
						 if( (int)sRequest.nByte > iLen ){
							 sRequest.nByte = (sxu32)iLen;
						 }
						 /* Decode POST data now */
						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);
					 }
				 }
		 }
	 }
	 /* All done,clean-up the mess left behind */
	 SySetRelease(&sHeader);
	 SyBlobRelease(&sWorker);
	 return SXRET_OK;
 }
