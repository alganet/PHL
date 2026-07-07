/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stddef.h>
#include <stdlib.h>
#ifndef PH7_OMIT_FLOATING_POINT
#include <math.h>
#endif
/* Signed 64-bit multiplication with overflow detection. GCC/Clang expose
 * __builtin_mul_overflow; MSVC does not, so we fall back to a portable
 * UB-free bound-check implementation. Sets *pR to the wrapped product and
 * returns non-zero on overflow. The fallback never divides a potentially-
 * overflowed intermediate: all divisions are of compile-time constants
 * (LARGEST_INT64/SMALLEST_INT64) by a factor already proven not to be -1,
 * and the product itself is computed via unsigned multiplication to avoid
 * signed-overflow UB. */
#if defined(__GNUC__) || defined(__clang__)
#define VmMulOverflow64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))
#else
static int VmMulOverflow64(sxi64 a, sxi64 b, sxi64 *pR)
{
	*pR = (sxi64)((sxu64)a * (sxu64)b);
	/* Factors of 0 or ±1 never overflow; handle up front so the divisions
	 * below are guaranteed safe (no SMALLEST_INT64 / -1, no /0). */
	if( a == 0 || b == 0 || a == 1 || b == 1 ){
		return 0;
	}
	if( a == -1 ){
		return b == SMALLEST_INT64;
	}
	if( b == -1 ){
		return a == SMALLEST_INT64;
	}
	/* |a|,|b| >= 2 and neither is -1.  Bound check against the MAX/MIN
	 * thresholds.  No division by -1 is possible here, and the quotients
	 * of compile-time constants by {a,b} always fit in sxi64. */
	if( a > 0 ){
		if( b > 0 ){
			return a > LARGEST_INT64 / b;
		}else{
			return b < SMALLEST_INT64 / a;
		}
	}else{
		if( b > 0 ){
			return a < SMALLEST_INT64 / b;
		}else{
			return b < LARGEST_INT64 / a;
		}
	}
}
#endif
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
/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */
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
/* VmObEntry struct moved to ph7int.h */
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
/*
 * Each installed autoload callback (registered using [spl_autoload_register()] )
 * is stored in an instance of the following structure.
 * Refer to the implementation of [spl_autoload_register()] for more information.
 */
typedef struct VmAutoloadCB VmAutoloadCB;
struct VmAutoloadCB
{
	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */
};

/*
 * Return TRUE if either operand is a NaN real value.
 */
static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)
{
	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){
		return TRUE;
	}
	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){
		return TRUE;
	}
	return FALSE;
}
/*
 * Return TRUE if the value should take the Perl-style string-increment path:
 * any MEMOBJ_STRING that is empty, or whose contents are not a complete
 * number (matching PHP's is_numeric semantics — the whole string must parse
 * as a number, with optional surrounding whitespace).  Strings with a
 * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the
 * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")
 * still go through the existing numeric coercion.
 */
static int VmStringWantsPerlIncr(ph7_value *pVal)
{
	SyString sStr;
	sxu8 bReal = FALSE;
	const char *zTail = 0;
	const char *zEnd;
	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){
		return FALSE;
	}
	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));
	if( sStr.nByte == 0 ){
		return TRUE;
	}
	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){
		return TRUE;
	}
	/* SyStrIsNumeric accepts a leading numeric prefix; require the
	 * remainder to be whitespace only so leading-numeric junk like "5foo"
	 * still takes the Perl path. */
	zEnd = sStr.zString + sStr.nByte;
	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){
		zTail++;
	}
	return zTail < zEnd;
}
/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */
/* Constant expander used by define(); used below to recognise user-defined
 * (vs. host/built-in) constants so their owned value object can be freed when
 * a define() overwrites them. */
static void VmExpandUserConstant(ph7_value *pVal,void *pUserData);
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
		/* A user-defined (define()) constant owns a heap ph7_value as its
		 * pUserData; free it before overwriting so repeated define()s — e.g.
		 * the same script re-run on a reused VM — don't leak the old value. */
		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData
		 && pCons->pUserData != pUserData ){
			PH7_MemObjRelease((ph7_value *)pCons->pUserData);
			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);
		}
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
	/* Return-type union alternatives (empty unless declared as a union) */
	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));
	pFunc->iFlags = iFlags;
	pFunc->pUserData = pUserData;
	/* Capture the defining file's strict_types mode. PHP scopes return-type
	 * coercion by the callee's file, so we freeze it at definition time. */
	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);
	SyStringInitFromBuf(&pFunc->sName,zName,nByte);
	return SXRET_OK;
}
/*
 * Namespace-aware function lookup.
 * Resolution order: exact name -> use imports -> current NS\name -> global fallback.
 * For functions (unlike classes), PHP falls back to global if not found in current NS.
 */
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
	/* Per-frame pending catch/finally return slot (always-init so release is
	 * unconditional; bHasRet is already 0 from SyZero). */
	PH7_MemObjInit(&(*pVm),&pFrame->sRet);
	return pFrame;
}
/* Forward declaration */
static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
 * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at
 * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)
 * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the
 * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is
 * consumed before its catching body unwinds (the body is a pinned ancestor), so this
 * is defensive; it never fires for an intermediate exception wrapper popped during a
 * resume — pVm->pResumeFrame is always a real body, never a wrapper.
 */
static void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)
{
	if( pVm->pResumeFrame == pFrame ){
		pVm->pResumeFrame = 0;
	}
}
/*
 * Leave the top-most active frame.
 */
static void VmLeaveFrame(ph7_vm *pVm)
{
		VmFrame *pCurFrame = pVm->pFrame;
	if( pCurFrame ){
		/* Unlink from the list of active VM frame */
		pVm->pFrame = pCurFrame->pParent;
		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){
			VmSlot  *aSlot;
			sxu32 n;
			/* Restore local variable to the free pool so that they can be reused again */
			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);
			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){
				/* Unset the local variable */
				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);
			}
			/* Remove local reference */
			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);
			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){
				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);
			}
		}
		/* Release internal containers */
		SyHashRelease(&pCurFrame->hVar);
		SySetRelease(&pCurFrame->sArg);
		SySetRelease(&pCurFrame->sLocal);
		SySetRelease(&pCurFrame->sRef);
		/* Release the per-frame pending-return slot (a frame-level resource like the
		 * containers above — released for every frame, including transparent
		 * exception/catch wrappers, which never own a return so it is empty there). */
		PH7_MemObjRelease(&pCurFrame->sRet);
		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */
		VmDropResumeTarget(pVm,pCurFrame);
		/* Release the whole structure */
		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);
	}
}
/*
 * Skip exception frames to reach the nearest non-exception frame.
 * Exception frames are transparent wrappers pushed by try/catch and
 * should be skipped when looking for the real execution context.
 */
static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)
{
	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		pFrame = pFrame->pParent;
	}
	return pFrame;
}
/*
 * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which
 * may be several frames below the frame that caught the exception — must resume at
 * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that
 * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and
 * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.
 *
 * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc
 * when the CURRENT exec is the one that owns the catching frame — so the landing pc
 * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"
 * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it
 * reaches the exec that actually caught it, which then matches and lands. A
 * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's
 * landing pad indexes the enclosing function's bytecode, never the mini-program's
 * small array — so it always propagates to its enclosing body. The recorded target
 * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;
 * VmSkipExceptionFrames yields its real body frame.
 *
 * This replaces the older "is there a resumable try frame here" test
 * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather
 * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the
 * catching frame was not the nearest try (ROOT B).
 */
static int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)
{
	if( pVm->pResumeFrame == 0 ){
		return FALSE; /* no in-place catch recorded for this in-flight throw */
	}
	/* Resume here only when THIS exec is the one that owns the catching try: same
	 * body frame AND same bytecode array. The bytecode-array check is essential
	 * because a catch/finally mini-program runs in its enclosing body's frame (no
	 * new frame), so the body-frame test alone cannot tell a mini-program apart from
	 * the body that shares it — and the recorded landing pad indexes only the array
	 * the try was compiled into. A mismatch on either means the exception was caught
	 * in a different exec, so we propagate (return/goto Exception) and let the owning
	 * exec's resume site match and land. */
	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame
	 || (void *)aInstr != pVm->pResumeInstr
	 || pVm->iResumePc == 0 ){
		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),
		 * always >= 1 in practice; the ==0 guard keeps a malformed record from
		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++
		 * would turn into a re-run from index 0) and from making the pop loop below
		 * never match a real frame. */
		return FALSE;
	}
	/* The catch may have run at an OUTER try, several try-levels above the throw.
	 * Each intervening try pushed its own VM_FRAME_EXCEPTION frame (at
	 * OP_LOAD_EXCEPTION) that its OWN OP_POP_EXCEPTION would tear down — but we are
	 * about to jump straight to the catching try's landing pad, skipping those inner
	 * OP_POP_EXCEPTIONs. Pop the intermediate exception frames here so exactly one
	 * frame (the catching try's) is left for the OP_POP_EXCEPTION we land on; without
	 * this each skipped inner try leaks a frame, corrupting the stack for later code.
	 * Their handlers/finally already ran in place during VmThrowException. The loop
	 * stops on any of three terms, all load-bearing: the catching try is reached (its
	 * iExceptionJump == the recorded landing); a non-exception (body) frame is reached
	 * (structural floor — never pop a real body); or this exec's entry is reached.
	 * Landing pads are unique per try within one bytecode array, and the function guard
	 * above pins (frame,array) to this exec, so the iExceptionJump match cannot stop at
	 * the wrong try. */
	while( (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)
	    && pVm->pFrame->iExceptionJump != pVm->iResumePc
	    && pVm->pFrame != pEntryFrame ){
		VmLeaveFrame(&(*pVm));
	}
	*pResumePc = (sxi32)pVm->iResumePc - 1;
	pVm->pResumeFrame = 0; /* one-shot consume */
	return TRUE;
}
/*
 * Drain pending finally blocks for the try/catch contexts pushed during the
 * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when
 * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued
 * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a
 * nested try/finally inside a catch body). Each finally runs with
 * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on
 * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a
 * finally threw an exception that escaped it (the caller must then unwind as an
 * exception rather than return its pending value — PHP: a throwing finally
 * discards the in-flight return), SXRET_OK otherwise.
 */
/*
 * BYTECODE stage 2b — per-activation try state.
 *
 * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that
 * object itself onto pVm->aException meant every recursive activation of the
 * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a
 * deep throw then ran every level's catch/finally against the deepest frame
 * (silent wrong answers; see PLAN.md §3.1 and the try_unwind_recursive_frames
 * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow
 * copy of the compiled object (sEntry/sFinally share the read-only compiled
 * containers) with fresh mutable state and pCompiled pointing at the origin.
 * Opcodes that only know the compiled p3 find their live activation with
 * VmExcLive. Activations are freed at the sites that discard an entry for
 * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,
 * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an
 * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's
 * "never free" constraint: a `goto` re-entering the try simply mints a fresh
 * activation.
 */
static ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)
{
	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));
	if( pClone == 0 ){
		return 0;
	}
	*pClone = *pCompiled;
	pClone->pCompiled = pCompiled;
	pClone->iFinallyDone = 0;
	pClone->iInCatch = 0;
	pClone->pInflight = 0;
	pClone->pFrame = 0;
	return pClone;
}
static void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)
{
	if( pExc && pExc->pCompiled ){
		/* Only activations are freed; the compiled object is compiler-owned.
		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH
		 * never ran to release — abort/reset between the pc-redirect and the
		 * catch) is dropped here so the exception instance cannot leak. */
		if( pExc->pInflight ){
			PH7_ClassInstanceUnref(pExc->pInflight);
			pExc->pInflight = 0;
		}
		SyMemBackendPoolFree(&pVm->sAllocator,pExc);
	}
}
/*
 * TRUE when the aException entry pExc is (an activation of) the compiled try
 * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).
 */
static int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)
{
	return pExc == pCompiled || pExc->pCompiled == pCompiled;
}
/*
 * Free every activation held in an exception-entry container (leftovers at VM
 * reset, a discarded hide/restore set, an abandoned coroutine's parked
 * handlers). The set itself is reset by the caller.
 */
static void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)
{
	sxu32 n = SySetUsed(pSet);
	if( n > 0 ){
		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);
		sxu32 i;
		for( i = 0; i < n; i++ ){
			VmExcRelease(pVm,ap[i]);
		}
	}
}
/*
 * The live activation of a lexical try: the topmost aException entry cloned
 * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction
 * only carries the compiled pointer.
 */
static ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)
{
	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);
	sxu32 n = SySetUsed(&pVm->aException);
	while( n > 0 ){
		n--;
		if( VmExcMatches(ap[n],pCompiled) ){
			return ap[n];
		}
	}
	return 0;
}
static sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)
{
	sxu32 nUsed;
	sxi32 rcOut = SXRET_OK;
	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){
		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);
		ph7_exception *pExc = apExc[nUsed - 1];
		(void)SySetPop(&pVm->aException);
		pExc->pFrame = 0;
		/* Leave the try's exception frame — but only a genuine one. For a RESUMED
		 * generator/fiber body the handler was restored from the parked ctx and its
		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine
		 * body itself; popping it would free the entry frame OP_DONE still reads
		 * (same guard as OP_POP_EXCEPTION's). */
		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
			VmLeaveFrame(&(*pVm));
		}
		if( pExc->iHasFinally && !pExc->iFinallyDone ){
			sxi32 rcF;
			pExc->iFinallyDone = 1;
			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);
			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */
			if( rcF == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rcF == PH7_EXCEPTION ){
				/* The finally threw past itself (its catch, if any, ran in place).
				 * Remember it so the caller unwinds as an exception; keep draining
				 * the remaining outer finallys so the frame stack stays balanced. */
				rcOut = PH7_EXCEPTION;
			}
		}else{
			VmExcRelease(&(*pVm),pExc);
		}
	}
	return rcOut;
}
/*
 * Drop a body frame's pending catch/finally return: clear the flag and release
 * the slot value. Safe on a frame with no pending return (the slot is then an
 * empty MEMOBJ_NULL value and the release is a no-op).
 */
static void VmClearFrameReturn(VmFrame *pFrame)
{
	pFrame->bHasRet = 0;
	PH7_MemObjRelease(&pFrame->sRet);
}
/*
 * Materialize a `return` issued inside a catch/finally mini-program: copy the
 * value deferred on the enclosing body frame (pEntryFrame->sRet) into the
 * function's result, clear the per-frame slot, and tear down any try frames left
 * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by
 * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the
 * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program
 * leaves the slot set so it materializes at its own enclosing body.
 */
static void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)
{
	if( pResult ){
		PH7_MemObjStore(&pEntryFrame->sRet,pResult);
	}
	VmClearFrameReturn(pEntryFrame);
	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){
		VmLeaveFrame(&(*pVm));
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
			/* Class instance — prefix with 'o' to match formal object/class signatures */
			int marker = 'o';
			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;
			SyString *pName = &pClass->sName;
			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));
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
/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */
static sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);
/*
 * Mount a compiled class into the freshly created vitual machine so that
 * it can be instanciated from the executed PHP script.
 */
/*
 * Reserve and initialize the static/constant attribute slots of a class.
 * This is the per-execution part of mounting a class: every static/const
 * attribute gets a fresh memory object, its default initializer is run, the
 * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static
 * properties register their enforcement slot. It is factored out of
 * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM
 * reuse without re-installing the (compile-time) methods.
 */
static sxi32 VmMountUserClassAttrs(
	ph7_vm *pVm,      /* Target VM */
	ph7_class *pClass /* Class whose static/const attributes are mounted */
	)
{
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
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
				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);
				/* Typed class constant (PHP 8.3): enforce the computed value
				 * against the declared type. A mismatch is a non-catchable
				 * fatal, raised here at definition time (matching PHP). */
				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_TYPED))
					== (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_TYPED) ){
					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);
					if( rcType != SXRET_OK ){
						return rcType;
					}
				}
			}
			/* Record attribute index */
			pAttr->nIdx = pMemObj->nIdx;
			/* Install static attribute in the reference table */
			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);
			/* If this is a typed static property, register the slot so the
			 * STORE path can enforce the declared type. We allocate a tiny
			 * VmClassAttr to uniformize with instance properties; the key
			 * points at its own nIdx field (stable for the VM lifetime).
			 * Typed *constants* are excluded — they are immutable and were
			 * already enforced above, so they need no store-time slot. */
			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)
				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));
				if( pVmAttrS == 0 ){
					return SXERR_MEM;
				}
				pVmAttrS->pAttr = pAttr;
				pVmAttrS->nIdx = pMemObj->nIdx;
				pVmAttrS->iState = 0;
				pVmAttrS->pOwner = pClass;
				/* Static typed property with no default starts uninitialized
				 * (constants are already excluded by the enclosing condition). */
				if( SySetUsed(&pAttr->aByteCode) == 0 ){
					pVmAttrS->iState |= VM_CLASS_ATTR_UNINIT;
				}
				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){
					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);
					return SXERR_MEM;
				}
			}
		}
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 VmMountUserClass(
	ph7_vm *pVm,      /* Target VM */
	ph7_class *pClass /* Class to be mounted */
	)
{
	ph7_class_method *pMeth;
	SyHashEntry *pEntry;
	sxi32 rc;
	/* Reserve/initialize the static and constant attribute slots */
	rc = VmMountUserClassAttrs(&(*pVm),pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Install class methods */
	if( pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT) ){
		/* Do not mount interface/trait methods since they are not directly invocable.
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
	/* Mark class as mounted to avoid redundant mounting */
	pClass->bMounted = TRUE;
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
			pVmAttr->iState = 0;
			pVmAttr->pOwner = pClass;
			if( SySetUsed(&pAttr->aByteCode) > 0 ){
				/* Initialize attribute default value (any complex expression) */
				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);
			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
				/* Typed property without a default: mark uninitialized. Reading
				 * it before the first write is an Error in PHP 7.4+. */
				pVmAttr->iState |= VM_CLASS_ATTR_UNINIT;
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
			/* Register typed property slot for assignment-time enforcement.
			 * On failure roll back the just-installed hAttr entry and the
			 * reserved memobj so the caller sees a consistent instance. */
			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);
				if( rc != SXRET_OK ){
					VmSlot sSlot;
					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);
					sSlot.nIdx = pMemObj->nIdx;
					sSlot.pUserData = 0;
					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);
					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
					return SXERR_MEM;
				}
			}
		}else{
			/* Install static/constant attribute */
			pVmAttr->nIdx = pAttr->nIdx;
			pVmAttr->iState = 0;
			pVmAttr->pOwner = pClass;
			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);
			if( rc != SXRET_OK ){
				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
				return SXERR_MEM;
			}
		}
	}
	return SXRET_OK;
}
/*
 * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to
 * stdClass for now; the future general-dynamic-props work (PLAN.md §3.1) turns
 * this into a class-flag / #[AllowDynamicProperties] check at this one site.
 */
static int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)
{
	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;
}
/*
 * Create a dynamic (runtime-added) property named [zName:nName] on a class
 * instance and return its freshly reserved value slot (the caller stores the
 * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new
 * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.
 *
 * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,
 * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation
 * holds the attr struct followed by the name bytes (so the SyHash key, which
 * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The
 * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its
 * inline name) on that flag — the only place a per-instance pAttr is freed.
 * The property is public + untyped, so the iFlags/sName dereferences in the
 * member-read, type-enforcement and destruction paths all behave normally.
 */
PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)
{
	ph7_class_attr *pAttr;
	VmClassAttr *pVmAttr = 0;
	ph7_value *pMemObj = 0;
	char *zCopy;
	/* One block: ph7_class_attr struct + inline NUL-terminated name. */
	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);
	if( pAttr == 0 ){
		return 0;
	}
	SyZero(pAttr,sizeof(ph7_class_attr));
	zCopy = (char *)&pAttr[1];
	if( nName > 0 ){
		SyMemcpy((const void *)zName,(void *)zCopy,nName);
	}
	zCopy[nName] = 0;
	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);
	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;
	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;
	pAttr->pDeclClass = pThis->pClass;
	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default
	 * value, never a union. */
	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));
	if( pVmAttr == 0 ){
		goto fail_attr;
	}
	pMemObj = PH7_ReserveMemObj(&(*pVm));
	if( pMemObj == 0 ){
		goto fail_vmattr;
	}
	pVmAttr->pAttr = pAttr;
	pVmAttr->nIdx = pMemObj->nIdx;
	pVmAttr->iState = 0;
	pVmAttr->pOwner = pThis->pClass;
	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows
	 * property-creation order, matching PHP. */
	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){
		goto fail_slot;
	}
	/* Install in the reference table so COW/refcount tracks the slot. */
	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);
	if( ppAttr ){
		*ppAttr = pVmAttr;
	}
	return pMemObj;
fail_slot:
	{
		VmSlot sSlot;
		sSlot.nIdx = pMemObj->nIdx;
		sSlot.pUserData = 0;
		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);
	}
fail_vmattr:
	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
fail_attr:
	SyMemBackendFree(&pVm->sAllocator,pAttr);
	return 0;
}
/*
 * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is
 * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic
 * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no
 * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr
 * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).
 */
static void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)
{
	VmClassAttr *pVmAttr;
	ph7_value *pMemObj;
	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));
	if( pVmAttr == 0 ){
		return;
	}
	pMemObj = PH7_ReserveMemObj(&(*pVm));
	if( pMemObj == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
		return;
	}
	pVmAttr->pAttr = pAttr;
	pVmAttr->nIdx = pMemObj->nIdx;
	pVmAttr->iState = 0;
	pVmAttr->pOwner = pThis->pClass;
	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh
	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The
	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)
	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,
	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */
	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
		pVmAttr->iState |= VM_CLASS_ATTR_UNINIT;
	}
	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.
	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that
	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value
	 * is always correct; only the relative order of a declared prop re-added after unset differs. */
	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){
		VmSlot sSlot;
		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;
		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);
		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
		return;
	}
	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);
	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){
			VmSlot sSlot;
			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);
			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;
			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);
			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
			return;
		}
	}
	if( ppAttr ){
		*ppAttr = pVmAttr;
	}
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
/* Forward declarations for Fiber C functions */
static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);
/* Forward declarations for Fiber/Generator infrastructure */
static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);
static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);
static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj);
static int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal);
static sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut);
static ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,
	ph7_class_instance *pBoundThis, const SyString *pScope);
static ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget);
static ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue);
static sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult);
static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,
	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);
static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,
	ph7_class_method *pMethod, ph7_value *pResult, int nArg,
	ph7_value **apArg, VmCallArgMap *pMap);
static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,
	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);
static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);
/* Forward declarations for Generator helpers and C functions */
static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);
static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);
static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg);
static int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg);
/*
 * Built-in classes/interfaces and some functions that cannot be implemented
 * directly as foreign functions.
 */
#define PH7_BUILTIN_LIB \
	"interface Throwable {"\
	"public function getMessage();"\
	"public function getCode();"\
	"public function getFile();"\
	"public function getLine();"\
	"public function getTrace();"\
	"public function getTraceAsString();"\
	"public function getPrevious();"\
	"public function __toString();"\
	"}"\
	"interface Traversable {}"\
	"interface ArrayAccess {"\
	"public function offsetExists($offset);"\
	"public function offsetGet($offset);"\
	"public function offsetSet($offset, $value);"\
	"public function offsetUnset($offset);"\
	"}"\
	"interface Countable {"\
	"public function count();"\
	"}"\
	"interface Stringable {"\
	"public function __toString();"\
	"}"\
	"interface JsonSerializable {"\
	"public function jsonSerialize();"\
	"}"\
	"interface UnitEnum {"\
	"public static function cases();"\
	"}"\
	"interface BackedEnum extends UnitEnum {"\
	"public static function from($value);"\
	"public static function tryFrom($value);"\
	"}"\
	"class Exception implements Throwable { "\
    "protected $message = '';"\
    "protected $code = 0;"\
    "protected $file;"\
    "protected $line;"\
    "protected $trace;"\
    "protected $previous;"\
	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\
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
	"class Error implements Throwable { "\
    "protected $message = '';"\
    "protected $code = 0;"\
    "protected $file;"\
    "protected $line;"\
    "protected $trace;"\
    "protected $previous;"\
	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\
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
	"public function getCode(){"\
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
	"class TypeError extends Error { }"\
	"class ArgumentCountError extends TypeError { }"\
	"class ValueError extends Error { }"\
	"class FiberError extends Error { }"\
	"class AssertionError extends Error { }"\
	"class ArithmeticError extends Error { }"\
	"class DivisionByZeroError extends ArithmeticError { }"\
	"class ErrorException extends Exception { "\
	"protected $severity;"\
	"public function __construct(string $message = null,"\
	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\
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
	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\
	"class LogicException extends Exception { }"\
	"class RuntimeException extends Exception { }"\
	"class BadFunctionCallException extends LogicException { }"\
	"class BadMethodCallException extends BadFunctionCallException { }"\
	"class DomainException extends LogicException { }"\
	"class InvalidArgumentException extends LogicException { }"\
	"class LengthException extends LogicException { }"\
	"class OutOfRangeException extends LogicException { }"\
	"class OutOfBoundsException extends RuntimeException { }"\
	"class OverflowException extends RuntimeException { }"\
	"class RangeException extends RuntimeException { }"\
	"class UnderflowException extends RuntimeException { }"\
	"class UnexpectedValueException extends RuntimeException { }"\
	"interface Iterator extends Traversable {"\
	"public function current();"\
	"public function key();"\
	"public function next();"\
	"public function rewind();"\
	"public function valid();"\
	"}"\
	"interface IteratorAggregate extends Traversable {"\
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
	"class Fiber {"\
	"  private $__ctx;"\
	"  private $__callable;"\
	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\
	"  public function start(){ return __fiber_start($this, func_get_args()); }"\
	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\
	"  public function getReturn(){ return __fiber_getReturn($this); }"\
	"  public function isStarted(){ return __fiber_isStarted($this); }"\
	"  public function isRunning(){ return __fiber_isRunning($this); }"\
	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\
	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\
	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\
	"  public function __destruct(){ __fiber_destruct($this); }"\
	"}"\
	"class Generator implements Iterator {"\
	"  private $__ctx;"\
	"  public function current(){ return __gen_current($this); }"\
	"  public function key(){ return __gen_key($this); }"\
	"  public function next(){ return __gen_next($this); }"\
	"  public function rewind(){ return __gen_rewind($this); }"\
	"  public function valid(){ return __gen_valid($this); }"\
	"  public function send($value = null){ return __gen_send($this,$value); }"\
	"  public function throw(Throwable $exception){ return __gen_throw($this,$exception); }"\
	"  public function getReturn(){ return __gen_getReturn($this); }"\
	"  public function __destruct(){ __gen_destruct($this); }"\
	"}"\
	"final class Closure {"\
	"  private $__fn;"\
	"  private $__this;"\
	"  private $__scope;"\
	"  public function __construct(){ throw new \\Error('Instantiation of class Closure is not allowed'); }"\
	"  public function bindTo($newThis, $scope = 'static'){ return __closure_bindTo($this, $newThis, $scope); }"\
	"  public function call($newThis, ...$args){ $bound = __closure_bindTo($this, $newThis, get_class($newThis)); return $bound(...$args); }"\
	"  public static function bind($closure, $newThis, $scope = 'static'){ return __closure_bindTo($closure, $newThis, $scope); }"\
	"  public static function fromCallable($callable){ return __closure_fromCallable($callable); }"\
	"}"\
	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\
	"class stdClass{"\
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
   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\
   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\
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
	"function array_merge_recursive(){"\
	" if( func_num_args() < 1 ){ return array(); }"\
    "$arrays = func_get_args();"\
    "$narrays = count($arrays);"\
    "$ret = array();"\
    "for( $i = 0; $i < $narrays; $i++ ){"\
	 " if( !is_array($arrays[$i]) ){"\
	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\
	 " }"\
     " foreach ($arrays[$i] as $key => $value) {"\
     "  $keyIsInt = is_int($key) || (is_string($key) && (string)intval($key) === $key);"\
     "  if( $keyIsInt ) {"\
     "   $ret[] = $value;"\
     "  } else {"\
     "   if (array_key_exists($key, $ret)) {"\
     "    $cur = $ret[$key];"\
     "    if (is_array($cur) && is_array($value)) {"\
     "     $ret[$key] = array_merge_recursive($cur, $value);"\
     "    } elseif (is_array($cur)) {"\
     "     $ret[$key] = array_merge_recursive($cur, array($value));"\
     "    } elseif (is_array($value)) {"\
     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\
     "    } else {"\
     "     $ret[$key] = array($cur, $value);"\
     "    }"\
     "   } else {"\
     "    $ret[$key] = $value;"\
     "   }"\
     "  }"\
     " }"\
	 " }"\
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
	SyString sRandom;
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
	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);
	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);
	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);
	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));
	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));
	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));
	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));
	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);
	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));
	pVm->pIdleCallFrames = 0;
	pVm->pIdleOperandStacks = 0;
	pVm->nIdleOperandStacks = 0;
	pVm->pIdleStackNodes = 0;
	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));
	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */
	pVm->pPendingException = 0;
	pVm->pInflightException = 0;
	pVm->nInflightExcBase = 0;
	pVm->pResumeFrame = 0;
	pVm->iResumePc = 0;
	pVm->pResumeInstr = 0;
	pVm->iResumeStackDepth = 0;
	/* Configuration containers */
	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));
	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));
	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));
	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));
	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));
	pVm->iResponseStatus = 200;
	pVm->bHeadersSent = 0;
	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));
	/* Error callbacks containers */
	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);
	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);
	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);
	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);
	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);
	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since
	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs
	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts
	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding
	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume
	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */
#if defined(__WINNT__) || defined(__UNIXES__)
	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */
	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the
	                             * usort-in-comparator path overflows at 1024) */
#else
	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is
	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion
	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM
	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */
	pVm->nMaxDepth = 512;
	pVm->nMaxNativeDepth = 16;
#endif
	/* Default assertion flags */
	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */
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
	/* Install a shared empty string constant so that every "" literal can
	 * reuse the same slot rather than allocating a new one.
	 * This mirrors the NULL/TRUE/FALSE handling above. */
	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);
	if( pObj == 0 ){
		rc = SXERR_MEM;
		goto Err;
	}
	PH7_MemObjInitFromString(pVm,pObj,0);
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
	/* Register the Random\RandomException namespaced class (PHP 8.2+).
	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace
	 * declaration is NOT reset at the block's closing brace in this engine, so
	 * anything following it in the same chunk would leak into the Random
	 * namespace. Isolation instead comes from VmEvalChunk saving/restoring
	 * pVm->sNamespace (and PH7_ResetCodeGenerator clearing the compiler
	 * namespace) per chunk, so this lands as Random\RandomException while later
	 * user code still compiles in the global namespace. */
	{
		static const char zRandomLib[] =
			"namespace Random { class RandomException extends \\Exception { } }";
		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);
		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);
	}
	/* Cache the Fiber class pointer for fast dispatch */
	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);
	/* Cache built-in interface pointers used on hot dispatch paths */
	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);
	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);
	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);
	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);
	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);
	/* Initialize null-coalesce-assign scratch slot */
	pVm->pCoalesceObj = 0;
	pVm->bCoalesceArmed = 0;
	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);
	/* Register Fiber internal C functions */
	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);
	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);
	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);
	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);
	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);
	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);
	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);
	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);
	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);
	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);
	/* Cache the Closure class pointer (closures are instances of it) */
	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);
	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */
	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */
	/* Closure::bind/bindTo/call/fromCallable native delegates (Increment 2) */
	ph7_create_function(pVm,"__closure_bindTo",vm_builtin_Closure_bindTo,0);
	ph7_create_function(pVm,"__closure_fromCallable",vm_builtin_Closure_fromCallable,0);
	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */
	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);
	/* Cache the Generator class pointer and register generator functions */
	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);
	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);
	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);
	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);
	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);
	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);
	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);
	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);
	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);
	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);
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
/*
 * Track output length and mark headers as sent when output reaches
 * a real external consumer (not the internal blob or OB buffer).
 */
static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)
{
	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;
	if( xCons != VmObConsumer ){
		pVm->nOutputLen += nLen;
		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){
			pVm->bHeadersSent = 1;
		}
	}
}
#define VM_STACK_GUARD 16
/*
 * Static operand-stack depth analysis (BYTECODE.md stage 7).
 *
 * The safe upper bound on a body's operand-stack depth is its instruction count
 * (no instruction pushes more than one net slot), and that is what
 * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates
 * badly — one operand stack per live frame, each sized to the whole body — so
 * this pass computes a TIGHT bound (typically single digits) for the common
 * shape of a recursive function, letting the OP_CALL path allocate small stacks.
 *
 * Undersizing an operand stack is a heap overflow, so the analysis is
 * conservative BY CONSTRUCTION:
 *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin
 *     that never exceeds its real pop on any path (verified per handler). Over-
 *     estimating height is safe; the only unsafe direction — over-crediting a
 *     pop — makes height go negative, which triggers fallback.
 *   - A body is sized by this analysis only if EVERY instruction is in the
 *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,
 *     yield, foreach, switch/match, spread, string/array builders, …) returns
 *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count
 *     bound. There is no partial/unsafe middle.
 *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch
 *     forms). An out-of-range jump, a negative height, or a height exceeding the
 *     instruction-count bound -> fallback.
 *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top
 *     of the returned depth, and the full corpus runs under ASan (which catches
 *     any undersize as a heap-buffer-overflow) as the standing validation.
 *
 * The exception-resume `pc =` reassignments inside some modeled handlers
 * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch
 * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and
 * forces fallback — so they are dead in analyzed bodies and need no edge.
 *
 * Drift safety (for whoever adds an opcode or changes a handler's stack effect):
 * VmInstrStackEffect is a hand-maintained model that must stay in sync with the
 * real handlers. Two things keep a drift from becoming a silent undersize: a NEW
 * opcode is unmodeled by default (its `default:` return forces the safe
 * instruction-count bound), so only *changing a modeled opcode's real pop count*
 * to exceed its popmin can undersize — and that is caught deterministically by
 * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on
 * the operand stack). When touching a modeled handler's push/pop, re-check its
 * entry here.
 */
#define VM_STACK_UNMODELED SXU32_HIGH
/*
 * Fill the stack effect of one modeled instruction: *pPush is its transient
 * push (0/1, added to the height for the peak), and the *pN successor edges
 * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns
 * 1 if modeled, 0 if the opcode is outside the verified set (whole-body
 * fallback). pc is this instruction's own index (fall-through = pc+1).
 */
static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])
{
	int push = 0, n = 0;
	sxi32 d;
	switch( pI->iOp ){
	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with
	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */
	case PH7_OP_LOADC:
	case PH7_OP_DUP:
		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;
	case PH7_OP_LOAD:
		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }
		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;
	case PH7_OP_LOAD_REF:
		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;
	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */
	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:
	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:
	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:
		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;
	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form
	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */
	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:
	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:
		if( pI->iP2 == 0 ){
			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;
		}else{
			aSucc[0] = pc + 1; aDelta[0] = -1;
			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;
		}
		break;
	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not
	 * verified here -> all three fall through to the unmodeled default.) */
	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:
	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:
	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:
	case PH7_OP_NOOP:
		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;
	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name
	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */
	case PH7_OP_STORE:
		d = ( pI->iP2 || pI->p3 == 0 ) ? -1 : 0;
		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;
	/* Explicit multi-slot pops (operand-encoded count). */
	case PH7_OP_POP:
	case PH7_OP_CONSUME:
		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;
	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).
	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose
	 * true pop count is a runtime value — never reaches here. */
	case PH7_OP_CALL:
		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;
	/* Jumps. */
	case PH7_OP_JMP:
		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;
	case PH7_OP_JZ: case PH7_OP_JNZ:
		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */
		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;
	/* Terminal: ends the path (its optional result pop does not propagate). */
	case PH7_OP_DONE:
		n = 0; break;
	default:
		return 0; /* unmodeled opcode -> whole-body fallback */
	}
	*pPush = push; *pN = n;
	return 1;
}
/*
 * Compute a tight upper bound on the operand-stack depth of a compiled body, or
 * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block
 * comment above. Never underestimates a modelable body's true peak depth.
 */
static sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)
{
	void *pScratch;
	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;
	sxu32 nQ, i, nIter, nCap;
	sxi32 iMax;
	int push, n, k;
	sxu32 succ[2]; sxi32 delta[2];
	if( nInstr == 0 || nInstr > 8192 ){
		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */
		return VM_STACK_UNMODELED;
	}
	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */
	for( i = 0; i < nInstr; i++ ){
		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){
			return VM_STACK_UNMODELED;
		}
	}
	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime
	 * and count -> one allocation, carved into three regions with the 4-byte arrays
	 * first (the byte array last needs no alignment). */
	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));
	if( pScratch == 0 ){
		return VM_STACK_UNMODELED;
	}
	aH  = (sxi32 *)pScratch;
	aQ  = (sxu32 *)(aH + nInstr);
	aIn = (unsigned char *)(aQ + nInstr);
	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }
	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;
	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */
	while( nQ > 0 ){
		sxu32 pc = aQ[--nQ];
		sxi32 h;
		aIn[pc] = 0;
		h = aH[pc];
		if( ++nIter > nCap ){ iMax = -1; break; }
		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);
		if( h + push > iMax ){ iMax = h + push; }
		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */
		for( k = 0; k < n; k++ ){
			sxi32 hn = h + delta[k];
			sxu32 t = succ[k];
			if( t >= nInstr || hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */
			if( hn > aH[t] ){
				aH[t] = hn;
				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }
			}
		}
		if( iMax < 0 ){ break; }
	}
	SyMemBackendFree(&pVm->sAllocator, pScratch);
	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;
}
/*
 * Allocate a new operand stack so that we can start executing
 * our compiled PHP program.
 * Return a pointer to the operand stack (array of ph7_values)
 * on success. NULL (Fatal error) on failure.
 *
 * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD
 * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a
 * parked buffer when it can and falls back to this; the other entries (top-level,
 * eval, coroutine, callbacks) call this directly.
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
/*
 * Operand-stack recycling (BYTECODE.md stage 7).
 *
 * After tight sizing, a PHP call still allocates + inits an operand stack on the
 * way in and frees it on the way out. For recursion and hot call loops the freed
 * stack is exactly the size the next call needs, so instead of freeing it at the
 * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and
 * hand it back to the next same-size call — skipping the buffer allocation and
 * the per-slot PH7_MemObjInit.
 *
 * The freelist holds plain allocator blocks (no header): a parked buffer is just
 * a ph7_value array whose slots were all released at recycle time, so it is
 * clean to reuse, cannot leak a stale value, and can still be raw-freed by the
 * cold/suspend/abort paths that never route through here. Head-only exact-size
 * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather
 * than over-allocating — deep recursion, whose freelist is empty during descent,
 * is unaffected). Length is capped so the pool can't grow without bound.
 *
 * The head-only match is tuned for the design target (recursion / a hot loop
 * calling one function — one size, ~total reuse). An alternating-size pattern
 * (a() then b() with different depths, repeatedly) never matches the head, so it
 * degrades to a fresh allocation every call — same as no pool, never worse; the
 * recursion case is the one worth the O(1) simplicity.
 */
typedef struct VmIdleStack VmIdleStack;
struct VmIdleStack {
	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */
	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */
	VmIdleStack *pNext;  /* LIFO link */
};
#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */
#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory
                                    * (a large fallback-sized stack recursing would
                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;
                                    * the tight-sized hot case is far below this) */
/*
 * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a
 * parked same-size buffer when one is available (its slots are already clean).
 */
static ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)
{
	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;
	sxu32 nCap = nSlots + VM_STACK_GUARD;
	if( pIdle && pIdle->nCap == nCap ){
		ph7_value *pStack = pIdle->pStack;
		pVm->pIdleOperandStacks = pIdle->pNext;
		pVm->nIdleOperandStacks--;
		/* Keep the wrapper node on the spare-node freelist for the next recycle
		 * instead of returning it to the pool (mirrors pIdleCallFrames). */
		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;
		pVm->pIdleStackNodes = pIdle;
		return pStack; /* slots already released -> reusable without re-init */
	}
	return VmNewOperandStack(&(*pVm),nSlots);
}
/*
 * Return an operand stack to the freelist (or free it if the pool is full).
 * nCap is its full allocated slot count (== the VmNewOperandStack size). Every
 * slot is released so the parked buffer is clean for reuse and never retains a
 * live value.
 */
static void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)
{
	VmIdleStack *pIdle;
	sxu32 i;
	if( pStack == 0 ){
		return;
	}
	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX || nCap > VM_STACK_POOL_MAXSLOTS ){
		SyMemBackendFree(&pVm->sAllocator,pStack);
		return;
	}
	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);
	 * pool-allocate only when the spare list is empty. */
	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;
	if( pIdle ){
		pVm->pIdleStackNodes = pIdle->pNext;
	}else{
		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));
		if( pIdle == 0 ){
			SyMemBackendFree(&pVm->sAllocator,pStack);
			return;
		}
	}
	for( i = 0; i < nCap; i++ ){
		PH7_MemObjRelease(&pStack[i]);
		/* Reset the global-slot index to the "temporary / not a variable" marker.
		 * A released slot is already reusable (the dispatch reuses released slots
		 * mid-call, and every push sets nIdx before the slot is read), but marking
		 * it here means a stale index can never masquerade as a live variable slot
		 * across invocations — cheap defense in depth. */
		pStack[i].nIdx = SXU32_HIGH;
	}
	pIdle->pStack = pStack;
	pIdle->nCap = nCap;
	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;
	pVm->pIdleOperandStacks = pIdle;
	pVm->nIdleOperandStacks++;
}
/* Forward declaration */
static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);
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
	/* Release the code generator now we have compiled our program, but keep its
	 * error consumer wired to the engine's: class mounting below (e.g. typed
	 * class-constant enforcement) still reports definition-time fatals through
	 * it, and the host VM output consumer is not installed until afterwards. */
	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);
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
	/* Snapshot the runtime object-pool watermark. Everything reserved from this
	 * index up (the $GLOBALS array, the superglobals, class static/const slots and
	 * every object/variable created during execution) is per-exec state that
	 * ph7_vm_reset() releases and truncates away before rebuilding; everything
	 * below it is compile-time/init state that survives a reset. */
	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);
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
	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */
	PH7_RegisterHttpResponseFunctions(&(*pVm));
#ifdef PH7_ENABLE_PCRE
	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */
	PH7_RegisterPcreFunctions(&(*pVm));
	PH7_RegisterPcreConstants(&(*pVm));
#endif
	/* Initialize and install static and constants class attributes.
	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the
	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and
	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep
	 * that function in sync when changing what is reserved here. */
	SyHashResetLoopCursor(&pVm->hClass);
	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){
		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	/* Random number betwwen 0 and 1023 used to generate unique ID */
	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;
	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */
	pVm->nNextObjId = 1;
	/* VM is ready for bytecode execution */
	return SXRET_OK;
}
/*
 * Tear down the whole reference table. Unlinks every referenced object,
 * deleting the hash entries (frame variables) and array nodes it points at.
 * Called by ph7_vm_reset() while the frames and the object pool are still
 * intact: doing it first means a later release of a by-ref array does not leave
 * a dangling node pointer in some other object's reference record.
 */
static void VmResetRefTable(ph7_vm *pVm)
{
	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements
	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed
	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */
	while( pVm->pRefList ){
		VmRefObjUnlink(&(*pVm),pVm->pRefList);
	}
}
/*
 * Release a standing per-exec ph7_value slot and re-initialise it to NULL.
 * The reset idiom for the VM's long-lived value fields (return value, the
 * error/exception handler callbacks, the assertion callback, the coalesce key).
 */
static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)
{
	PH7_MemObjRelease(pObj);
	PH7_MemObjInit(&(*pVm),pObj);
}
/*
 * Reset a function's static-variable sentinels to SXU32_HIGH so the next call
 * re-reserves their slots and re-runs the initializers (PHP's per-request reset
 * of statics).
 */
static void VmResetFuncStatics(ph7_vm_func *pFunc)
{
	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);
	sxu32 k;
	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){
		aStatic[k].nIdx = SXU32_HIGH;
	}
}
/*
 * Reset per-execution function-table state in a single pass over hFunction:
 *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never
 *    installed in hFunction (see compile.c) and closure names are unique, so any
 *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its
 *    captured environment values, its name buffer and its structure (the
 *    bytecode/args/static sets are shared with the template and must NOT be
 *    freed). Its template-shared static sentinels are reset too.
 *  - every other function (and its pNextName overloads, including class methods)
 *    has its static sentinels reset.
 * The head flag of each entry fully classifies it, so one walk handles both.
 * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances
 * the cursor past it before returning and the delete never touches the cursor.
 */
static void VmResetFunctionState(ph7_vm *pVm)
{
	SyHashEntry *pEntry;
	SyHashResetLoopCursor(&pVm->hFunction);
	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){
		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;
		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){
			/* Standalone run-time closure: reset its (template-shared) statics,
			 * release its captured-by-value environment, then free the entry,
			 * name buffer and structure. */
			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
			const char *zName = SyStringData(&pFunc->sName);
			sxu32 k;
			VmResetFuncStatics(pFunc);
			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){
				PH7_MemObjRelease(&aEnv[k].sValue);
			}
			SySetRelease(&pFunc->aClosureEnv);
			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */
			SyHashDeleteEntry2(pEntry);
			if( zName ){
				SyMemBackendFree(&pVm->sAllocator,(void *)zName);
			}
			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);
			continue;
		}
		/* Named function: reset statics for every overload sharing this name. */
		while( pFunc ){
			VmResetFuncStatics(pFunc);
			pFunc = pFunc->pNextName;
		}
	}
	pVm->closure_cnt = 0;
}
/*
 * Free the typed-property enforcement slots left in hTypedSlot. Instance slots
 * are already gone (each object's destructor removed its own during the object
 * pool release above), so only the class *static* typed-property slots remain;
 * the class re-mount registers fresh ones.
 */
static void VmResetTypedSlots(ph7_vm *pVm)
{
	SyHashEntry *pEntry;
	/* Common case: no class static typed properties — table already empty. */
	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){
		return;
	}
	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then
	 * drop and re-init the table — SyHashRelease frees the entries themselves. */
	SyHashResetLoopCursor(&pVm->hTypedSlot);
	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){
		if( pEntry->pUserData ){
			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);
		}
	}
	SyHashRelease(&pVm->hTypedSlot);
	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);
}
/*
 * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the
 * same compiled program can be executed again (compile-once / execute-many).
 *
 * Definitions are preserved (treated like compile-time state): the bytecode,
 * the operand stack, the function/class/interface tables, user-defined constants
 * (a re-run define() overwrites the value in place), included-file markers
 * (so include_once/require_once stay satisfied — definitions and their
 * define()s survive without re-compiling), the literal pool, the cached
 * interface pointers, the output-consumer configuration and the IO streams.
 *
 * Per-execution state is cleared: global variables and the global frame, the
 * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and
 * class statics, run-time closures, the output buffers and response headers, the
 * exception/error-handler state, the reference table and every object/array
 * reserved during the run.
 *
 * Object __destruct methods are NOT run during reset (see bInReset) — releasing
 * the pool runs engine-level teardown only, matching PH7's prior behaviour where
 * global-scope destructors never fired.
 */
PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)
{
	sxu32 nWater,n;
	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){
		return SXERR_CORRUPT;
	}
	nWater = pVm->nSuperBaseline;
	/* The $GLOBALS array is normally protected from deletion; drop the guard so
	 * its hashmap is actually released below, then rebuilt by CreateSuper. */
	pVm->pGlobal = 0;
	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,
	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,
	 * a stale pointer must not survive into the next reused (-S server) request — the object pool
	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the
	 * object); unref'ing here would race the teardown below. */
	pVm->pClosureThis = 0;
	pVm->pClosureScope = 0;
	/* Suppress user __destruct while we tear down the per-exec object pool: the
	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP
	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is
	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */
	pVm->bInReset = 1;
	/* (1) Unlink the whole reference table while frames and objects are intact. */
	VmResetRefTable(&(*pVm));
	/* (2) Free run-time closures and reset every function/method static sentinel
	 * in a single pass over hFunction. User-defined constants are treated like
	 * function/class registrations and intentionally persist across reuse (a
	 * re-run define() overwrites the value in place). */
	VmResetFunctionState(&(*pVm));
	/* (3) Release every object/variable reserved during the run. Re-reading the
	 * used count each iteration tolerates a destructor reserving a fresh slot. */
	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){
		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);
		if( pObj ){
			PH7_MemObjRelease(pObj);
		}
	}
	/* (4) Free the class static typed-property slots (instance ones are already
	 * gone — object release in step 3 removes each instance's own slot). */
	VmResetTypedSlots(&(*pVm));
	/* (5) Unwind any active frames back to none. */
	while( pVm->pFrame ){
		VmLeaveFrame(&(*pVm));
	}
	/* Object teardown is complete; user __destruct may run normally again. */
	pVm->bInReset = 0;
	/* (6) Truncate the object pool back to the watermark and forget stale free
	 * slots (their indices no longer exist). */
	SySetTruncate(&pVm->aMemObj,nWater);
	SySetReset(&pVm->aFreeObj);
	/* (7) Reset the superglobal name table and namespace scratch. */
	SyHashRelease(&pVm->hSuper);
	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);
	/* (8) Drain remaining per-exec containers. */
	SySetReset(&pVm->aSelf);
	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks
	 * at the end of exec; release any that survived an abandoned run (e.g. exit()
	 * inside a shutdown callback) so their owned callback/arg values don't leak. */
	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){
		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);
		if( pCB ){
			int iArg;
			PH7_MemObjRelease(&pCB->sCallback);
			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){
				PH7_MemObjRelease(&pCB->aArg[iArg]);
			}
		}
	}
	SySetReset(&pVm->aShutdown);
	/* Stage 2b: free any leftover per-activation exception clones (an
	 * aborted program can leave entries behind). */
	VmExcReleaseAll(&(*pVm),&pVm->aException);
	SySetReset(&pVm->aException);
	SySetReset(&pVm->aFinallyAction);
	pVm->pPendingException = 0;
	pVm->pInflightException = 0;
	pVm->nInflightExcBase = 0;
	pVm->pResumeFrame = 0;
	pVm->iResumePc = 0;
	pVm->pResumeInstr = 0;
	pVm->iResumeStackDepth = 0;
	pVm->nExceptDepth = 0;
	/* spl_autoload_register() callbacks are per request */
	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){
		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pCB ){
			PH7_MemObjRelease(&pCB->sCallback);
		}
	}
	SySetReset(&pVm->aAutoload);
	/* The reentrancy guard is empty outside an active autoload (the common case);
	 * only rebuild the table when an aborted autoload left entries behind. */
	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){
		SyHashRelease(&pVm->hAutoloadActive);
		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);
	}
	/* Output buffers */
	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){
		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);
		if( pOb ){
			PH7_MemObjRelease(&pOb->sCallback);
			SyBlobRelease(&pOb->sOB);
		}
	}
	SySetReset(&pVm->aOB);
	pVm->nObDepth = 0;
	/* (9) Rebuild the global frame and the superglobals. */
	{
		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);
		if( rc == SXRET_OK ){
			rc = PH7_HashmapCreateSuper(&(*pVm));
		}
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	/* (10) Re-mount the static/const attribute slots of every class. */
	{
		SyHashEntry *pEntry;
		SyHashResetLoopCursor(&pVm->hClass);
		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){
			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	/* (11) Reset the remaining scalar/per-exec fields. */
	SyBlobReset(&pVm->sConsumer);
	pVm->nOutputLen = 0;
	VmReinitMemObj(&(*pVm),&pVm->sExec);
	PH7_VmReleaseResponseHeaders(pVm);
	pVm->iResponseStatus = 200;
	pVm->bHeadersSent = 0;
	pVm->bHttpContext = 0;
	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);
	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);
	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);
	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);
	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);
	pVm->json_rc = JSON_ERROR_NONE;
#ifdef PH7_ENABLE_PCRE
	pVm->iPcreLastError = 0;
#endif
	pVm->iCmpCallbackExc = 0;
	pVm->bHaltRequested = 0;
	pVm->iExitStatus = 0;
	pVm->iSpreadExtra = 0;
	pVm->nRecursionDepth = 0;
	pVm->pActiveCtx = 0;
	pVm->pCoalesceObj = 0;
	pVm->bCoalesceArmed = 0;
	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);
	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */
	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;
	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)
	 * looks like a fresh process, matching PH7_VmMakeReady(). */
	pVm->nNextObjId = 1;
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
PH7_PRIVATE ph7_value * PH7_VmExtractSuper(
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
PH7_PRIVATE sxi32 PH7_VmHashmapInsert(
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
		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED
		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative
		 * executor, so recursion is limited by memory like the main PHP engine.
		 * This is an embedder opt-in: any non-negative value installs a cap of that
		 * many frames; 0 restores the unbounded default. No upper clamp (the old
		 * <1024 clamp guarded the native stack the recursion no longer grows — that
		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it
		 * would otherwise read as an enormous positive cap). */
		int nDepth = va_arg(ap,int);
		if( nDepth >= 0 ){
			pVm->nMaxDepth = nDepth;
		}
		break;
									   }
	case PH7_VM_CONFIG_NATIVE_DEPTH:{
		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry
		 * classes the trampoline does not flatten (eval/include towers, nested
		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the
		 * platform stack; the default (256 host / 16 small-stack embedders) is
		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,
		 * so it is rejected as a footgun). */
		int nDepth = va_arg(ap,int);
		if( nDepth > 1 ){
			pVm->nMaxNativeDepth = nDepth;
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
			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);
		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){
			/* Extract the $_POST superglobal */
			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);
		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){
			/* Extract the $_GET superglobal */
			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);
		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){
			/* Extract the $_COOKIE superglobal */
			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);
		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){
			/* Extract the $_SESSION superglobal */
			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);
		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){
			/* Extract the $_HEADER superglobale */
			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);
		}else{
			/* Extract the $_SERVER superglobal */
			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);
		}
		if( pValue == 0 || (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* No such entry */
			rc = SXERR_NOTFOUND;
			break;
		}
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pValue->x.pOther;
		/* Perform the insertion */
		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);
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
		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);
		if( pValue == 0 || (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* No such entry */
			rc = SXERR_NOTFOUND;
			break;
		}
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pValue->x.pOther;
		/* Perform the insertion */
		n = (sxu32)SyStrlen(zValue);
		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);
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
		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);
		/* Mark this VM as operating in HTTP context only on success */
		if( rc == SXRET_OK ){
			pVm->bHttpContext = 1;
		}
		break;
									}
	case PH7_VM_CONFIG_RESPONSE_STATUS: {
		/* Extract HTTP response status code */
		int *pStatus = va_arg(ap, int *);
		if( pStatus ){
			*pStatus = pVm->iResponseStatus;
		}
		break;
										}
	case PH7_VM_CONFIG_RESPONSE_HEADERS: {
		/* Iterate response headers via callback */
		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);
		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);
		void *pUserData = va_arg(ap, void *);
		if( xCallback ){
			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);
			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);
			for( k = 0; k < nHdr; k++ ){
				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,
							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,
							   pUserData);
				if( rc != PH7_OK ){
					break;
				}
			}
		}
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
static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);
static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);
static int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc);
static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);
static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);
static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);
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
	VmTrackOutput(pVm, SyBlobLength(pMsg));
	return rc;
}
/*
 * Throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error()] for additional
 * information.
 */
static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)
{
	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){
		ph7_value apArg[4];
		ph7_value *apArgPtr[4];
		ph7_value sResult;
		SyString sErr;
		/* Prepare arguments */
		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);
			/* use explicit message length to avoid reading past buffer */
			SyStringInitFromBuf(&sErr,zMessage,nLen);
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
	/* Default prefix is "Error:".  Only the built-in warning/notice
	 * severities adjust the textual prefix.  Do not modify the raw error
	 * code; user handlers rely on seeing the original value (e.g. 8192 for
	 * E_DEPRECATED). */
	zErr = "Error:  ";
	switch(iErr){
	case PH7_CTX_WARNING:
		zErr = "Warning:  ";
		break;
	case PH7_CTX_NOTICE:
		zErr = "Notice:  ";
		break;
	default:
		/* keep iErr unchanged */
		break;
	}
	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));
	if( pFuncName ){
		/* Append function name first */
		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);
		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);
	}
	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));
	/* Check for user error handler.  compute length of C string */
	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){
		rc = VmCallErrorHandler(&(*pVm),pWorker);
	}
	return rc;
}
/*
 * Raise an out-of-memory fatal and request a clean VM halt.
 *
 * This is the single choke point for surfacing an allocation failure that would
 * otherwise produce a silently-wrong result (a truncated string/array returned
 * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a
 * fatal-level diagnostic, sets a nonzero process exit status, and requests a
 * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs
 * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers
 * return the value of this function (PH7_ABORT) directly, or `goto Abort` after
 * calling it from a VM op.
 */
PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)
{
	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");
	/* Non-catchable, terminate with a PHP-like fatal exit status */
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	return PH7_ABORT;
}
/*
 * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.
 */
PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)
{
	return PH7_VmMemoryError(pCtx->pVm);
}
/*
 * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage
 * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows
 * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,
 * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the
 * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.
 */
static int VmRecursionExceeded(ph7_vm *pVm)
{
	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is
	 * heap-bound, so only an embedder-configured cap can trip. */
	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;
}
/*
 * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack
 * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the
 * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine
 * start/resume entries (which splice frames in before that wrapper runs). Always
 * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to
 * keep native re-entries off a finite C stack).
 */
static int VmNativeNestingExceeded(ph7_vm *pVm)
{
	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;
}
/*
 * Raise the recursion-limit fatal and request a clean VM halt. Mirrors
 * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size
 * reached": a catchable Error can't be used here because PH7 runs the catch
 * body (and renders an uncaught exception) inline at the throw-site depth —
 * which is already over the cap, so getMessage()/__toString()/the catch body
 * would re-trip the limit and recurse forever. A clean fatal removes the old
 * silent "return NULL and continue" hazard while keeping the promise that deep
 * recursion never panics: it unwinds via the abort path and still runs
 * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole
 * site testing the PHP call-depth cap); native nesting has its own fatal
 * (VmNativeNestingFatal).
 *
 * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes
 * this idempotent, so an error handler that itself recurses past the cap can't
 * re-enter and loop.
 */
static sxi32 VmRecursionFatal(ph7_vm *pVm)
{
	if( pVm->bHaltRequested ){
		return PH7_ABORT;
	}
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);
	return PH7_ABORT;
}
/*
 * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the
 * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the
 * two limits are distinguishable. Non-catchable for the same at-depth reason.
 */
static sxi32 VmNativeNestingFatal(ph7_vm *pVm)
{
	if( pVm->bHaltRequested ){
		return PH7_ABORT;
	}
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");
	return PH7_ABORT;
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
	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special
	 * prefix; leave other error codes untouched so the handler receives
	 * the correct errno value. */
	zErr = "Error:  ";
	switch(iErr){
	case PH7_CTX_WARNING:
		zErr = "Warning:  ";
		break;
	case PH7_CTX_NOTICE:
		zErr = "Notice:  ";
		break;
	default:
		/* do not change iErr */
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
	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){
		/* No handler or handler returned TRUE, normal processing */
		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));
		rc = VmCallErrorHandler(&(*pVm),pWorker);
	}
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Return the class currently active on the self-stack (the innermost `self`
 * scope), or NULL when executing outside any class context.
 */
static ph7_class * VmCurrentSelf(ph7_vm *pVm)
{
	if( SySetUsed(&pVm->aSelf) > 0 ){
		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);
		return apSelf[SySetUsed(&pVm->aSelf)-1];
	}
	return 0;
}
/*
 * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it
 * with the message held in *pMsg, and throw it from the current frame. Consumes
 * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the
 * class is unavailable or the engine is aborting. Shared scaffolding for the
 * typed-property / uninitialized-property / readonly error throwers.
 */
static sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)
{
	ph7_class *pErrClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	VmFrame *pFrame;
	sxi32 rc;
	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);
	if( pErrClass == 0 ){
		SyBlobRelease(pMsg);
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);
	if( pThis == 0 ){
		SyBlobRelease(pMsg);
		return PH7_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		ph7_value sArg;
		ph7_value *apArg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(pMsg);
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Throw php's catchable Error for an append (`$a[] = v`) whose saturated
 * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap
 * layer; the store opcodes route the returned PH7_EXCEPTION through the
 * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.
 */
PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)
{
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Throw a PHP-compatible TypeError whose message describes a failed typed
 * property assignment. Called from the STORE path when coercion is not
 * possible.
 */
static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)
{
	ph7_class_attr *pAttr = pVmAttr->pAttr;
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	/* Prefer the declaring class over the runtime instance class so that an
	 * inherited typed property reports its original owner, matching PHP. */
	if( pOwner ){
		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",
			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);
	}else{
		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",
			zGiven,&pAttr->sName,&pAttr->sTypeName);
	}
	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
}
/*
 * Throw a PHP-compatible Error for reading an uninitialized typed property.
 */
static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",
		zKind,&pOwner->sName,&pAttr->sName);
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Throw the PHP-compatible Error raised on an illegal write to a readonly
 * property (PHP 8.1). bModify TRUE → a write to an already-initialized property
 * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from
 * a scope that cannot satisfy the readonly set-scope ("Cannot modify
 * protected(set) readonly property C::$x from {global scope|scope X}").
 */
static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( bModify ){
		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);
	}else{
		ph7_class *pActive = VmCurrentSelf(pVm);
		if( pActive ){
			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",
				&pOwner->sName,&pAttr->sName,&pActive->sName);
		}else{
			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",
				&pOwner->sName,&pAttr->sName);
		}
	}
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment
 * and decrement opcodes mutate the per-instance slot directly, bypassing
 * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A
 * readonly property reached by `++`/`--` is necessarily already initialized (an
 * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always
 * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the
 * PH7_EXCEPTION/PH7_ABORT produced by the throw.
 */
static sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)
{
	SyHashEntry *pSlot;
	VmClassAttr *pVmAttr;
	if( nIdx == SXU32_HIGH || SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){
		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */
	}
	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));
	if( pSlot == 0 ){
		return SXRET_OK; /* Not a typed slot */
	}
	pVmAttr = (VmClassAttr *)pSlot->pUserData;
	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){
		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);
	}
	return SXRET_OK;
}
/*
 * Enforce a typed-property assignment. On entry pValue holds the incoming
 * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).
 * For class types, instanceof is verified.
 *
 * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION
 * after throwing TypeError, or PH7_ABORT on fatal error.
 */
/*
 * PHP-strict numeric-string check used by typed-property enforcement.
 * Returns TRUE only if the entire string (optionally surrounded by
 * whitespace, with optional sign) is a valid numeric literal. Unlike the
 * permissive is_numeric() implementation which accepts leading-numeric
 * strings like "43x", this mirrors PHP's rules for coercing to int/float.
 */
static int VmStringIsStrictNumeric(ph7_value *pValue)
{
	const char *z, *zEnd, *zTail;
	sxu32 n;
	sxu8 bReal;
	sxi32 rc;
	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){
		return 0;
	}
	z = (const char *)SyBlobData(&pValue->sBlob);
	n = SyBlobLength(&pValue->sBlob);
	zEnd = z + n;
	if( n == 0 ){
		return 0;
	}
	zTail = 0;
	rc = SyStrIsNumeric(z,n,&bReal,&zTail);
	if( rc != SXRET_OK || zTail == 0 ){
		return 0;
	}
	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */
	while( zTail < zEnd && SyisSpace(zTail[0]) ){
		zTail++;
	}
	return zTail == zEnd ? 1 : 0;
}

/*
 * Numeric-string classification used by union weak-mode coercion. Returns:
 *   1 if the string is a strictly-numeric integer (no fraction, no exponent)
 *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)
 *   0 if it's not strictly numeric.
 */
static int VmStringNumericKind(ph7_value *pValue)
{
	const char *z, *zEnd, *zTail;
	sxu32 n;
	sxu8 bReal = 0;
	sxi32 rc;
	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){
		return 0;
	}
	z = (const char *)SyBlobData(&pValue->sBlob);
	n = SyBlobLength(&pValue->sBlob);
	zEnd = z + n;
	if( n == 0 ) return 0;
	zTail = 0;
	rc = SyStrIsNumeric(z,n,&bReal,&zTail);
	if( rc != SXRET_OK || zTail == 0 ) return 0;
	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;
	if( zTail != zEnd ) return 0;
	return bReal ? 2 : 1;
}

/*
 * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.
 * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not
 * scalar keywords), so without this every enforcement site — return, parameter,
 * property, union alternative — would have to string-match the name itself.
 * Centralising it here keeps the four sites consistent and is the single place
 * to extend when another literal/pseudo type is added.
 *   returns  1 : recognised pseudo-type AND the value satisfies it
 *            0 : recognised pseudo-type AND the value does NOT satisfy it
 *           -1 : not a pseudo-type (caller should treat sClass as a real class)
 */
static int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)
{
	const char *z = pClass->zString;
	sxu32 n = pClass->nByte;
	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){
		return 1; /* `mixed` accepts any value, including null */
	}
	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){
		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;
	}
	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){
		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;
	}
	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){
		/* iterable === array | Traversable */
		if( pValue->iFlags & MEMOBJ_HASHMAP ){
			return 1;
		}
		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){
			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){
				return 1;
			}
		}
		return 0;
	}
	return -1;
}
/*
 * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When
 * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive
 * scalar coercion). When bStrict is non-zero, only exact type matches are
 * accepted, plus the single implicit widening int -> float (so an int value
 * against a `float|X` union succeeds; string -> int does not).
 * Returns SXRET_OK on accept (pValue may have been mutated by the cast),
 * SXERR_INVALID on reject. Caller is responsible for the actual TypeError
 * throw.
 *
 * The class match for object values consults the active VM self-stack to
 * resolve `self`/`parent` aliases when present.
 */
/*
 * Resolve a class/interface name from a type declaration to its ph7_class*,
 * handling the `self`/`parent` aliases against the supplied scope class pSelf
 * (the active self for params/returns/properties, or the declaring class for a
 * class constant). Used by every type-enforcement site so the resolution rule —
 * including the iLoadable flag — lives in one place.
 *
 * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-
 * compatibility target, where the type may legitimately be an interface or
 * abstract class (TRUE would filter those out → the check is skipped → any
 * object wrongly accepted). Centralizing FALSE here keeps a future caller from
 * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly
 * with TRUE; it does not go through this helper.)
 */
static ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)
{
	if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){
		return pSelf;
	}
	if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){
		return pSelf ? pSelf->pBase : 0;
	}
	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);
}
static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)
{
	sxu32 i;
	sxu32 nAlts;
	ph7_type_alt *aAlts;
	int bHasArray, bHasObjAlt, bHasClassAlt;
	int bHasInt, bHasFloat, bHasString, bHasBool;
	int bHasIntersection = 0;
	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];
	if( pValue->iFlags & MEMOBJ_NULL ){
		return bNullable ? SXRET_OK : SXERR_INVALID;
	}
	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);
	nAlts = SySetUsed(pAlts);
	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the
	 * value must match ALL its members); singleton groups are ordinary union
	 * alternatives (match ANY). Group ids are NOT dense in the stored set —
	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be
	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */
	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;
	for( i = 0; i < nAlts; i++ ){
		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){
			bHasIntersection = 1;
		}
	}
	/* Intersection phase: an object satisfies an intersection group iff it is
	 * instanceof every member. Members are always class types (enforced at parse),
	 * so a non-object value can never satisfy a group. Skipped entirely for a pure
	 * union (the common case), which then pays nothing for the group machinery. */
	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){
		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
		ph7_class *pSelfNow = VmCurrentSelf(pVm);
		sxu32 g;
		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){
			int bAll;
			if( aGroupCount[g] < 2 ) continue;
			bAll = 1;
			for( i = 0; i < nAlts; i++ ){
				ph7_class *pExpected;
				if( aAlts[i].nGroup != g ) continue;
				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }
				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelfNow);
				if( pExpected == 0 || !PH7_VmInstanceOf(pInst->pClass,pExpected) ){
					bAll = 0;
					break;
				}
			}
			if( bAll ) return SXRET_OK;
		}
	}
	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are
	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.
	 * A match on any one accepts the value (handles e.g. `true|int`, `?true`,
	 * `iterable|Foo`). Only singleton-group (ordinary union) atoms apply here. */
	for( i = 0; i < nAlts; i++ ){
		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;
		if( aAlts[i].nType == SXU32_HIGH
		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){
			return SXRET_OK;
		}
	}
	bHasArray = bHasObjAlt = bHasClassAlt = 0;
	bHasInt = bHasFloat = bHasString = bHasBool = 0;
	for( i = 0; i < nAlts; i++ ){
		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;
		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;
		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;
		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;
		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;
		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;
		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;
		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;
	}
	/* Object handling */
	if( pValue->iFlags & MEMOBJ_OBJ ){
		if( bHasObjAlt ) return SXRET_OK;
		if( bHasClassAlt ){
			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
			ph7_class *pSelfNow = VmCurrentSelf(pVm);
			for( i = 0; i < nAlts; i++ ){
				ph7_class *pExpected;
				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;
				if( aAlts[i].nType != SXU32_HIGH ) continue;
				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelfNow);
				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){
					return SXRET_OK;
				}
			}
		}
		return SXERR_INVALID;
	}
	/* Array handling */
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		return bHasArray ? SXRET_OK : SXERR_INVALID;
	}
	/* Scalar handling — exact match first */
	if( pValue->iFlags & MEMOBJ_INT ){
		if( bHasInt ) return SXRET_OK;
	}
	if( pValue->iFlags & MEMOBJ_REAL ){
		if( bHasFloat ) return SXRET_OK;
	}
	if( pValue->iFlags & MEMOBJ_STRING ){
		if( bHasString ) return SXRET_OK;
	}
	if( pValue->iFlags & MEMOBJ_BOOL ){
		if( bHasBool ) return SXRET_OK;
	}
	if( bStrict ){
		/* Strict mode: only int -> float widening is allowed implicitly. */
		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){
			PH7_MemObjToReal(pValue);
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	/* Weak coercion preference order: int > float > string > bool.
	 * Numeric-string handling distinguishes integer-shaped from float-shaped
	 * to match PHP's union RFC. */
	{
		int kind = VmStringNumericKind(pValue);
		if( bHasInt ){
			/* int target accepts: bool, int (already exact), float w/o fraction,
			 * numeric-string-int. Float→int with fraction loses info → skip. */
			if( pValue->iFlags & MEMOBJ_BOOL ){
				PH7_MemObjToInteger(pValue);
				return SXRET_OK;
			}
			if( pValue->iFlags & MEMOBJ_REAL ){
				ph7_real r = pValue->rVal;
				if( r == (ph7_real)(sxi64)r ){
					PH7_MemObjToInteger(pValue);
					return SXRET_OK;
				}
			}
			if( kind == 1 ){
				PH7_MemObjToInteger(pValue);
				return SXRET_OK;
			}
		}
		if( bHasFloat ){
			if( pValue->iFlags & (MEMOBJ_BOOL|MEMOBJ_INT) ){
				PH7_MemObjToReal(pValue);
				return SXRET_OK;
			}
			if( kind == 1 || kind == 2 ){
				PH7_MemObjToReal(pValue);
				return SXRET_OK;
			}
		}
		if( bHasString ){
			if( pValue->iFlags & (MEMOBJ_BOOL|MEMOBJ_INT|MEMOBJ_REAL) ){
				PH7_MemObjToString(pValue);
				return SXRET_OK;
			}
		}
		if( bHasBool ){
			if( pValue->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_STRING) ){
				PH7_MemObjToBool(pValue);
				return SXRET_OK;
			}
		}
	}
	return SXERR_INVALID;
}

/*
 * Enforce a scalar type hint on a single argument/return value under the
 * current strict-types mode. Pre: *pVal* does not already match *nType*,
 * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).
 * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict
 * mode rejects the value. Callers throw the TypeError on rejection.
 */
static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)
{
	/* A standalone `null` type is not a weak-coercion target: only an actual
	 * null value satisfies it (and a null value matches via the flag test
	 * before this is ever called, so pVal is non-null here). Reject rather than
	 * casting the value to null — otherwise a `null`-typed parameter would
	 * silently swallow any argument. */
	if( nType == MEMOBJ_NULL ){
		return SXERR_INVALID;
	}
	if( bStrict ){
		/* Only int -> float widening is allowed implicitly. */
		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){
			PH7_MemObjToReal(pVal);
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	{
		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);
		if( xCast ) xCast(pVal);
	}
	return SXRET_OK;
}

/*
 * Render a scalar-type name suitable for the "Argument ... must be of type X"
 * TypeError message. Prefers the declared textual form when available.
 *
 * The declared SyString is length-delimited, not necessarily NUL-terminated,
 * so we bounded-copy it into the caller's *zBuf* before returning it as a
 * C string safe for "%s" formatting. If no declared text is present we fall
 * back to a static literal and ignore zBuf entirely.
 */
static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)
{
	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){
		sxu32 nCopy = SyStringLength(pDeclared);
		if( nCopy >= nBuf ) nCopy = nBuf - 1;
		if( pDeclared->zString && nCopy > 0 ){
			SyMemcpy(pDeclared->zString, zBuf, nCopy);
		}
		zBuf[nCopy] = 0;
		return zBuf;
	}
	switch( nType ){
		case MEMOBJ_INT:     return "int";
		case MEMOBJ_REAL:    return "float";
		case MEMOBJ_STRING:  return "string";
		case MEMOBJ_BOOL:    return "bool";
		case MEMOBJ_HASHMAP: return "array";
		case MEMOBJ_OBJ:     return "object";
		default:             return "scalar";
	}
}

/*
 * Format the class name of an object-typed ph7_value into a small caller
 * buffer, for use in TypeError messages. Returns the buffer pointer.
 */
static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)
{
	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
	SyBufferFormat(zBuf,nBuf,"%.*s",
		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);
	return zBuf;
}

static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)
{
	SyHashEntry *pSlot;
	VmClassAttr *pVmAttr;
	ph7_class_attr *pAttr;
	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));
	if( pSlot == 0 ){
		return SXRET_OK; /* Not a typed slot */
	}
	pVmAttr = (VmClassAttr *)pSlot->pUserData;
	pAttr = pVmAttr->pAttr;
	if( pAttr == 0 || (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
		return SXRET_OK;
	}
	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly
	 * property may be written exactly once and only from within the declaring
	 * class scope (its set-scope is protected). */
	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){
		/* A readonly property is always typed and default-less, so it starts
		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*
		 * write below — making it the write-once latch (a type-rejected write
		 * leaves it set, so a later valid initialization still works). */
		if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){
			/* Already initialized: any further write is forbidden, any scope. */
			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);
		}
		{
			/* First write must come from within the declaring class scope
			 * (readonly's set-scope is protected — a subclass may initialize). */
			ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;
			ph7_class *pActive = VmCurrentSelf(pVm);
			if( pActive == 0 || pDecl == 0 || !PH7_VmInstanceOf(pActive,pDecl) ){
				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);
			}
		}
	}
	/* Union type: dispatch to the shared coercion helper. Typed properties
	 * are always evaluated in weak mode regardless of declare(strict_types),
	 * matching PHP's documented behavior. */
	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,
			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,
			0 /* bStrict: properties never apply strict_types */);
		if( rc == SXRET_OK ){
			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
			return SXRET_OK;
		}
		if( pValue->iFlags & MEMOBJ_OBJ ){
			char zBuf[128];
			return VmThrowPropertyTypeError(pVm,pVmAttr,
				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));
		}
		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
	}
	/* NULL handling: allowed if the type is nullable, or is `mixed` (which
	 * includes null). */
	if( pValue->iFlags & MEMOBJ_NULL ){
		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)
		 || (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5
		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){
			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
			return SXRET_OK;
		}
		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");
	}
	/* standalone `null` property type (PHP 8.2): a null value was already
	 * accepted by the nullable check above, so any non-null value here is a
	 * type error. */
	if( pAttr->nType == MEMOBJ_NULL ){
		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
	}
	/* Bare 'object' type hint: accept any class instance, reject non-objects.
	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is
	 * otherwise treated as "scalar, not array" and would be rejected. */
	if( pAttr->nType == MEMOBJ_OBJ ){
		if( pValue->iFlags & MEMOBJ_OBJ ){
			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
			return SXRET_OK;
		}
		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
	}
	/* Pseudo-types stored as class-name atoms: `iterable` (array|Traversable),
	 * `true`/`false` (matching bool), `mixed` (any value — its null case is
	 * handled by the nullable check above). Checked by value before the generic
	 * class-instanceof branch, which would resolve no such class and then
	 * wrongly accept any object / reject arrays. */
	if( pAttr->nType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);
		if( rcPseudo == 1 ){
			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
		}
		/* rcPseudo == -1: real class — fall through to the instanceof branch. */
	}
	if( pAttr->nType == SXU32_HIGH ){
		/* Class / interface type. Resolve self/parent relative to the class
		 * currently active on the self-stack. */
		ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,VmCurrentSelf(pVm));
		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){
			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
		}
		if( pExpected ){
			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){
				char zBuf[128];
				return VmThrowPropertyTypeError(pVm,pVmAttr,
					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));
			}
		}
		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
		return SXRET_OK;
	}
	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast
	 * helpers used by function-argument hints. Reject object→scalar. */
	if( pValue->iFlags & MEMOBJ_OBJ ){
		char zBuf[128];
		return VmThrowPropertyTypeError(pVm,pVmAttr,
			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));
	}
	if( (pValue->iFlags & pAttr->nType) == 0 ){
		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);
		if( xCast ){
			/* Reject array<->scalar coercion to match PHP strictness */
			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){
				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
			}
			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){
				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));
			}
			/* PHP weak mode: reject string->int/float unless the string is
			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43
			 * would hide bugs and diverges from PHP's TypeError. */
			if( (pAttr->nType == MEMOBJ_INT || pAttr->nType == MEMOBJ_REAL)
			 && (pValue->iFlags & MEMOBJ_STRING)
			 && !VmStringIsStrictNumeric(pValue) ){
				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");
			}
			xCast(pValue);
		}
	}
	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
	return SXRET_OK;
}
/*
 * Raise the non-catchable fatal PHP emits when a typed class constant is given
 * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it
 * prints the diagnostic, sets a nonzero exit status, requests a clean halt and
 * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).
 */
static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	char zBuf[128];
	const char *zGiven;
	if( pValue->iFlags & MEMOBJ_OBJ ){
		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
	}else{
		zGiven = ph7_type_name(pValue);
	}
	/* A class is normally mounted during the compile/VmMakeReady phase, where the
	 * code-generator's error consumer is active but the host VM output consumer is
	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,
	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").
	 * A class declared at runtime inside plain eval() reaches here with the codegen
	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer
	 * so the fatal is still reported rather than the program halting silently. */
	if( pVm->sCodeGen.xErr ){
		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,
			"Cannot use %s as value for class constant %z::%z of type %z",
			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);
	}else{
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Cannot use %s as value for class constant %z::%z of type %z",
			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);
	}
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	return SXERR_ABORT;
}
/*
 * Enforce a typed class constant's value against its declared type (PHP 8.3).
 * Unlike typed properties (weak mode), constants are checked strictly: the only
 * implicit coercion allowed is int -> float widening (so `const float X = 1` is
 * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds
 * the computed constant value (it may be widened in place). Returns SXRET_OK on
 * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.
 */
static sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)
{
	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;
	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */
	if( pValue->iFlags & MEMOBJ_NULL ){
		if( bNullable || pAttr->nType == MEMOBJ_NULL ){
			return SXRET_OK;
		}
		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5
			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){
			return SXRET_OK;
		}
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
	}
	/* Union type: reuse the shared coercion helper in strict mode. */
	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */) == SXRET_OK ){
			return SXRET_OK;
		}
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
	}
	/* standalone `null` type: a non-null value is a mismatch. */
	if( pAttr->nType == MEMOBJ_NULL ){
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
	}
	/* Bare `object` type: any class instance, nothing else. */
	if( pAttr->nType == MEMOBJ_OBJ ){
		if( pValue->iFlags & MEMOBJ_OBJ ){
			return SXRET_OK;
		}
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
	}
	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else
	 * a real class/interface verified by instanceof. */
	if( pAttr->nType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);
		if( rcPseudo == 1 ){
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
		}
		/* rcPseudo == -1: a real class/interface type. */
		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){
			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
		}
		{
			/* A class constant's self/parent resolve against the declaring class. */
			ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,pClass);
			if( pExpected ){
				ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
				if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){
					return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
				}
			}
		}
		return SXRET_OK;
	}
	/* Scalar type, strict: an exact flag match, or the single int -> float
	 * implicit widening. Everything else is a type error.
	 *
	 * Known lenient divergence: PHL's number model leaves a whole-valued real
	 * flagged MEMOBJ_REAL|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,
	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int`
	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)
	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so
	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal
	 * case `const int X = 1.0` is caught earlier, at definition time, by the
	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal
	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)
	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed
	 * residual needs PHL's float-identity/division model, which is out of scope. */
	if( pValue->iFlags & pAttr->nType ){
		return SXRET_OK;
	}
	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){
		PH7_MemObjToReal(pValue);
		return SXRET_OK;
	}
	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);
}

/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 * ------------------------------------
 * Simple boring wrapper function.
 * ------------------------------------
 */
PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)
{
	va_list ap;
	sxi32 rc;
	va_start(ap,zFormat);
	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * Throw a TypeError exception from within the VM execution loop.
 * Used for user-defined function type hint violations (e.g. object type hint).
 */
static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyBlob sMsg;
	SyString sMsgStr;
	VmFrame *pFrame;
	sxi32 rc;
	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);
	if( pClass == 0 ){
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ABORT;
	}
	SyBlobInit(&sMsg,&pVm->sAllocator);
	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free
	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */
	if( pOwnerClass ){
		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",
			&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);
	}else{
		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",
			pFuncName,nArg,pArgName,zExpected,zGiven);
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(&sMsg);
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Throw a PHP-compatible TypeError describing a return-value type mismatch.
 * Message format: "funcname(): Return value must be of type X, Y returned".
 */
/* Build a catchable TypeError from a pre-formatted message blob and throw it.
 * The message is copied into the instance by __construct, so the caller owns
 * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */
static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyString sMsgStr;
	VmFrame *pFrame;
	sxi32 rc;
	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);
	if( pClass == 0 ){
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)
{
	SyBlob sMsg;
	sxi32 rc;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",
		pFuncName,zExpected,zGiven);
	rc = VmThrowTypeErrorMsg(pVm,&sMsg);
	SyBlobRelease(&sMsg);
	return rc;
}
/* A never-returning function that returned normally (fall-off). PHP bans an
 * explicit `return` at compile time, so this fires only for an implicit return. */
static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,SyString *pFuncName)
{
	SyBlob sMsg;
	sxi32 rc;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"%z(): never-returning function must not implicitly return",
		pFuncName);
	rc = VmThrowTypeErrorMsg(pVm,&sMsg);
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Format the "X given" portion of error messages following PHP's value-name
 * convention: "true"/"false" for booleans, class name for objects, otherwise
 * the bare type name. zBuf must hold at least 64 bytes.
 */
static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)
{
	if( pVal->iFlags & MEMOBJ_BOOL ){
		return pVal->x.iVal ? "true" : "false";
	}
	if( pVal->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;
		if( pThis && pThis->pClass ){
			SyString *pName = &pThis->pClass->sName;
			sxu32 n = pName->nByte;
			if( n >= nBuf ){
				n = nBuf - 1;
			}
			SyMemcpy(pName->zString,zBuf,n);
			zBuf[n] = 0;
			return zBuf;
		}
		return "object";
	}
	return ph7_type_name(pVal);
}
/*
 * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a
 * non-array value at runtime. Matches the message and class PHP raises
 * ("Only arrays and Traversables can be unpacked, X given"). The class is
 * \TypeError for objects, \Error otherwise — matching PHP's distinction.
 */
static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyBlob sMsg;
	SyString sMsgStr;
	VmFrame *pFrame;
	sxi32 rc;
	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";
	char zNameBuf[64];
	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));
	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);
	if( pClass == 0 ){
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ABORT;
	}
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(&sMsg);
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Enforce the declared return type of *pFunc* against the value returned
 * (or NULL if the function returned without a value). Mutates *pValue* to
 * perform allowed widening (int->float) or weak-mode coercion. On
 * violation, throws TypeError and returns PH7_EXCEPTION.
 */
/*
 * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The
 * caller's buffer is then safe to pass through "%s" formatters. An empty or
 * null SyString yields an empty C string. Returns zBuf.
 */
static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)
{
	sxu32 nCopy;
	if( nBuf == 0 ) return "";
	if( pStr == 0 || pStr->zString == 0 ){
		zBuf[0] = 0;
		return zBuf;
	}
	nCopy = SyStringLength(pStr);
	if( nCopy >= nBuf ) nCopy = nBuf - 1;
	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);
	zBuf[nCopy] = 0;
	return zBuf;
}

/*
 * TRUE if a function declares a return type that must be enforced — a single
 * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is
 * left 0). The return-enforcement gates must consult both, not just the single
 * type field.
 */
static int VmFuncHasReturnType(ph7_vm_func *pFunc)
{
	return pFunc->nReturnType > 0 || SySetUsed(&pFunc->aReturnUnion) > 0;
}
static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)
{
	int bStrict = pFunc->bStrictTypes ? 1 : 0;
	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;
	const char *zGiven;
	char zBuf[128];
	char zTypeBuf[128];
	/* Untyped function: no enforcement (no single type and no union/intersection). */
	if( !VmFuncHasReturnType(pFunc) ){
		return SXRET_OK;
	}
	/* never return type: the function must not return at all. An explicit
	 * `return` is a compile error, so reaching here means the function ran off
	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at
	 * the call site). */
	if( pFunc->nReturnType == MEMOBJ_NEVER ){
		return VmThrowNeverReturnError(pVm,&pFunc->sName);
	}
	/* void return type: the function must not produce a value. */
	if( pFunc->nReturnType == MEMOBJ_VOID ){
		if( pValue == 0 ){
			return SXRET_OK;
		}
		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL
		 * still counts as "returned a value" here. */
		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);
	}
	/* Fell off the end or a bare `return;` with no value: PHP requires any typed
	 * return (even a nullable one) to return a value explicitly — only an explicit
	 * `return null;` satisfies a nullable type, which is handled below. */
	if( pValue == 0 ){
		const char *zExpected = "value";
		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){
			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));
		}
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");
	}
	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a
	 * TypeError. (Falling off the end is handled by the generic check above,
	 * matching how every other typed return reports a missing value.) */
	if( pFunc->nReturnType == MEMOBJ_NULL ){
		if( pValue->iFlags & MEMOBJ_NULL ){
			return SXRET_OK;
		}
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",
			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));
	}
	/* An explicit `return null` satisfies any nullable return type (`?T`, `T|null`,
	 * `A|B|null`) uniformly — handle it before the per-shape branches below, none
	 * of which (scalar/class/union) carry their own nullable check. */
	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){
		return SXRET_OK;
	}
	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),
	 * `true`/`false` (the matching bool literal), `iterable` (array|Traversable).
	 * Check by value before the real-class instanceof branch below. */
	if( pFunc->nReturnType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);
		if( rcPseudo == 1 ){
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,
				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));
		}
		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */
	}
	/* Union/intersection return type — delegate. A null alternative is not stored
	 * in aReturnUnion (dropped at parse), so nullability comes from the func's
	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */
	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){
		sxi32 rcU;
		const char *zExpected = "union";
		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);
		if( rcU == SXRET_OK ){
			return SXRET_OK;
		}
		if( pValue->iFlags & MEMOBJ_OBJ ){
			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
		}else if( pValue->iFlags & MEMOBJ_NULL ){
			zGiven = "null";
		}else{
			zGiven = ph7_type_name(pValue);
		}
		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){
			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));
		}
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);
	}
	/* Class return type — instanceof check. The class name is a length-
	 * delimited SyString; copy it into a local buffer before formatting
	 * it into the TypeError message. */
	if( pFunc->nReturnType == SXU32_HIGH ){
		SyString *pClassName = &pFunc->sReturnClass;
		const char *zExpected;
		ph7_class *pExpected = VmResolveTypeClass(pVm,pClassName,VmCurrentSelf(pVm));
		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));
		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){
			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);
			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);
		}
		if( pExpected ){
			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){
				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);
			}
		}
		return SXRET_OK;
	}
	/* Scalar return type. A nullable scalar accepting null was already handled by
	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a
	 * non-nullable scalar return — a TypeError. */
	if( pValue->iFlags & MEMOBJ_NULL ){
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			"null");
	}
	/* Exact match? Done. */
	if( pValue->iFlags & pFunc->nReturnType ){
		return SXRET_OK;
	}
	/* Object->scalar is never compatible. */
	if( pValue->iFlags & MEMOBJ_OBJ ){
		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			zGiven);
	}
	/* Array <-> scalar is never compatible. */
	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			ph7_type_name(pValue));
	}
	/* PHP's weak-mode rule: string -> int/float is allowed only if the
	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43
	 * would hide the bug and diverges from PHP. Strict mode falls through
	 * to VmEnforceScalarType below which rejects string->int outright. */
	if( !bStrict
	 && (pFunc->nReturnType == MEMOBJ_INT || pFunc->nReturnType == MEMOBJ_REAL)
	 && (pValue->iFlags & MEMOBJ_STRING)
	 && !VmStringIsStrictNumeric(pValue) ){
		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			"string");
	}
	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){
		return SXRET_OK;
	}
	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,
		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
		ph7_type_name(pValue));
}
/*
 * Report a fatal named-argument error.
 * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.
 */
static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)
{
	const char *zFunc = 0;
	int nFunc = 0;
	VmGetFrameContext(pVm,&zFunc,&nFunc);
	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);
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
 * Resolve function context from the current frame.
 */
static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)
{
	VmFrame *pFrame;
	ph7_vm_func *pFunc;
	*pzFuncName = 0;
	*pnFuncLen = 0;
	pFrame = pVm->pFrame;
	if( pFrame == 0 ){
		return;
	}
	pFrame = VmSkipExceptionFrames(pFrame);
	if( pFrame->pParent == 0 ){
		return;
	}
	pFunc = (ph7_vm_func *)pFrame->pUserData;
	if( pFunc == 0 ){
		return;
	}
	*pzFuncName = pFunc->sName.zString;
	*pnFuncLen = (int)pFunc->sName.nByte;
}
/*
 * Render one exception entry of an uncaught-exception report into pOut.
 *
 * The output is the single-exception PHP format, factored so a `$previous`
 * chain can emit several entries into one blob (see VmReportUncaughtChain):
 *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a
 *             chained entry is prefixed "\n\nNext " (blank-line separated).
 *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"
 *             trailer.
 * A single entry (bFirst && bLast) is byte-identical to the historical output.
 * The caller owns the blob lifecycle (init/release) and the output consumer
 * call; this routine only appends.
 */
static void VmRenderUncaughtEntry(
	ph7_vm *pVm,SyBlob *pOut,
	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,
	const char *zFuncName,int nFuncLen,int bFirst,int bLast)
{
	SyString *pFile;
	if( zClass == 0 || nClass == 0 ){
		zClass = "Exception";
		nClass = (sxu32)sizeof("Exception") - 1;
	}
	if( zFuncName == 0 || nFuncLen <= 0 ){
		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);
	}
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( bFirst ){
		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);
	}else{
		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);
	}
	SyBlobAppend(pOut,zClass,nClass);
	if( zMsg && nMsg > 0 ){
		SyBlobAppend(pOut,": ",sizeof(": ")-1);
		SyBlobAppend(pOut,zMsg,nMsg);
	}
	if( pFile ){
		SyBlobAppend(pOut," in ",sizeof(" in ")-1);
		SyBlobAppend(pOut,pFile->zString,pFile->nByte);
		SyBlobAppend(pOut,":1",sizeof(":1")-1);
	}
	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);
	if( pFile ){
		SyBlobAppend(pOut,"#0 ",sizeof("#0 ")-1);
		SyBlobAppend(pOut,pFile->zString,pFile->nByte);
		if( zFuncName && nFuncLen > 0 ){
			SyBlobFormat(pOut,"(1): %.*s()\n",nFuncLen,zFuncName);
		}else{
			SyBlobAppend(pOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);
		}
	}else if( zFuncName && nFuncLen > 0 ){
		SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);
	}else{
		SyBlobAppend(pOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);
	}
	SyBlobAppend(pOut,"#1 {main}",sizeof("#1 {main}")-1);
	if( bLast && pFile ){
		SyBlobAppend(pOut,"\n",sizeof("\n")-1);
		SyBlobAppend(pOut,"  thrown in ",sizeof("  thrown in ")-1);
		SyBlobAppend(pOut,pFile->zString,pFile->nByte);
		SyBlobAppend(pOut," on line 1",sizeof(" on line 1")-1);
	}
}
/*
 * Emit a PHP-compatible uncaught exception message and stack trace for a
 * single exception (no `$previous` chain). Used by the callers that have no
 * exception instance to walk (internal Error reports, type errors, ...).
 */
static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)
{
	SyBlob sOut;
	/* An uncaught exception is a fatal: php exits 255 whether or not the
	 * report is displayed. Set the status before the bErrReport gate. */
	pVm->iExitStatus = 255;
	if( !pVm->bErrReport ){
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	VmRenderUncaughtEntry(pVm,&sOut,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE);
	VmCallErrorHandler(pVm,&sOut);
	SyBlobRelease(&sOut);
	return PH7_ABORT;
}
/*
 * Return the `$previous` exception linked on pThis (the Throwable data model:
 * a protected $previous property, inherited by every user subclass), or NULL
 * if there is none / it isn't an object. Reads the per-instance attribute
 * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.
 */
static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };
static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)
{
	ph7_value *pValue;
	ph7_class_instance *pPrev;
	ph7_class *pThrowable;
	if( pThis == 0 ){
		return 0;
	}
	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);
	if( pValue == 0 || (pValue->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pPrev = (ph7_class_instance *)pValue->x.pOther;
	/* PHP's $previous is always a Throwable; a PHL subclass could assign a
	 * non-Throwable to the (untyped, protected) slot — ignore it so the report
	 * never renders a stray object as an exception entry. */
	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);
	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){
		return 0;
	}
	return pPrev;
}
/*
 * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous
 * yet (PHP keeps an explicitly-constructed previous and never overrides it).
 * pThis takes a ref on pPrev so it outlives the superseded exception; the
 * slot is released by the normal instance teardown. No-op on any miss.
 */
static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)
{
	ph7_value *pValue;
	if( pThis == 0 || pPrev == 0 || pThis == pPrev ){
		return;
	}
	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);
	if( pValue == 0 || (pValue->iFlags & MEMOBJ_OBJ) != 0 ){
		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */
	}
	pPrev->iRef++;
	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a
	 * non-Throwable); release frees any such buffer before the overwrite. */
	PH7_MemObjRelease(pValue);
	pValue->x.pOther = pPrev;
	MemObjSetType(pValue,MEMOBJ_OBJ);
}
/*
 * Append the message of the exception instance pThis to pOut by invoking its
 * getMessage() (so a user override is honored). A no-op if the method is
 * absent or yields an empty string.
 */
static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)
{
	ph7_class_method *pGetMessage;
	ph7_value sMsg;
	const char *zTmp;
	int nTmp;
	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);
	if( pGetMessage == 0 ){
		return;
	}
	PH7_MemObjInit(pVm,&sMsg);
	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){
		zTmp = ph7_value_to_string(&sMsg,&nTmp);
		if( zTmp && nTmp > 0 ){
			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);
		}
	}
	PH7_MemObjRelease(&sMsg);
}
/*
 * Emit a PHP-compatible uncaught-exception report for pThis, walking its
 * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each
 * outer one as "Next ...", with a single "thrown in ..." trailer after the
 * outermost (the actually-uncaught) exception.
 *
 * The walk starts at pThis (outermost) and follows $previous inward; entries
 * are rendered in reverse (deepest first). A cyclic $previous (e.g.
 * $a->previous = $a, or A<->B) is broken on the first already-seen instance so
 * it is not duplicated, and the depth is hard-capped as a final backstop.
 */
#define VM_EXCEPTION_CHAIN_MAX 64
static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)
{
	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];
	int nChain = 0;
	int i;
	SyBlob sOut;
	/* Same rule as VmReportUncaughtException: an uncaught exception is a
	 * fatal — php exits 255 whether or not the report is displayed. One rule
	 * per report entry point, so a future direct caller can't miss it. */
	pVm->iExitStatus = 255;
	if( !pVm->bErrReport ){
		return PH7_OK;
	}
	/* Collect outermost -> deepest, stopping on a cycle (an instance already
	 * collected) or the hard cap. */
	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){
		for( i = 0 ; i < nChain ; ++i ){
			if( apChain[i] == pThis ){
				pThis = 0; /* cycle: stop the walk */
				break;
			}
		}
		if( pThis == 0 ){
			break;
		}
		apChain[nChain++] = pThis;
		pThis = VmExceptionGetPrevious(pThis);
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),
	 * index 0 is the outermost (gets the "thrown in" trailer). */
	for( i = nChain - 1 ; i >= 0 ; --i ){
		ph7_class_instance *pEnt = apChain[i];
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		VmExtractExceptionMessage(pVm,pEnt,&sMsg);
		VmRenderUncaughtEntry(pVm,&sOut,
			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,
			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),
			zFuncName,nFuncLen,
			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */
			(i == 0) ? TRUE : FALSE);           /* bLast: outermost entry */
		SyBlobRelease(&sMsg);
	}
	VmCallErrorHandler(pVm,&sOut);
	SyBlobRelease(&sOut);
	return PH7_ABORT;
}
/*
 * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.
 *
 * Arming holds a ref on the cached ArrayAccess instance so it survives the
 * intervening RHS evaluation until NULLC_STORE consumes it. Anything that
 * abandons that store path before NULLC_STORE runs — an exception thrown
 * while evaluating the RHS, a re-arm for a different target — must disarm
 * here, both to release the leaked instance ref/key and to stop a later
 * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.
 */
static void VmCoalesceDisarm(ph7_vm *pVm)
{
	if( pVm->bCoalesceArmed ){
		if( pVm->pCoalesceObj ){
			PH7_ClassInstanceUnref(pVm->pCoalesceObj);
		}
		PH7_MemObjRelease(&pVm->sCoalesceKey);
		pVm->pCoalesceObj = 0;
		pVm->bCoalesceArmed = 0;
	}
}
/*
 * Throw a PHP-compatible exception of the named class from inside the VM
 * bytecode dispatch loop (where no ph7_context is available). The message
 * is a literal, non-formatted string; callers that need formatting should
 * build the SyBlob themselves and pass its data + length.
 *
 * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on
 * successful throw (the caller should typically `goto Abort` afterwards if
 * the surrounding opcode cannot continue). Mirrors the inline pattern in
 * PH7_OP_THROW (see "case PH7_OP_THROW").
 */
static sxi32 VmThrowFromVm(
	ph7_vm *pVm,
	const char *zClass,
	const char *zMsg,
	sxu32 nMsg
){
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	VmFrame *pFrame;
	sxi32 rc;
	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);
	if( pClass == 0 ){
		return SXERR_ABORT;
	}
	pThis = PH7_NewClassInstance(pVm,pClass);
	if( pThis == 0 ){
		return SXERR_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		ph7_value sArg;
		ph7_value *apArg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);
		PH7_MemObjInit(pVm,&sArg);
		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pVm,pThis);
	PH7_ClassInstanceUnref(pThis);
	return rc;
}
/*
 * Throw an internal exception instance that can be intercepted by try/catch.
 */
PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)
{
	ph7_vm *pVm;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyBlob sMsg;
	SyString sMsgStr;
	VmFrame *pFrame;
	va_list ap;
	sxi32 rc;

	if( pCtx == 0 || pCtx->pVm == 0 ){
		return PH7_ABORT;
	}
	pVm = pCtx->pVm;
	if( zClass == 0 || zClass[0] == 0 ){
		zClass = "Error";
	}
	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);
	if( pClass == 0 ){
		return PH7_VmThrowExceptionTrace(pCtx,zClass,
			"Cannot throw internal exception, class '%s' is not available",
			zClass
			);
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_VmThrowExceptionTrace(pCtx,zClass,
			"Cannot throw internal exception, PH7 is running out of memory"
			);
	}

	SyBlobInit(&sMsg,&pVm->sAllocator);
	va_start(ap,zFormat);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	va_end(ap);

	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(&sMsg);

	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Throw an internal error as a PHP-like uncaught exception message with stack trace.
 * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.
 */
PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)
{
	ph7_vm *pVm;
	SyBlob sMsg;
	const char *zFuncName = 0;
	int nFuncLen = 0;
	va_list ap;
	sxi32 rc;

	if( pCtx == 0 || pCtx->pVm == 0 ){
		return PH7_OK;
	}
	pVm = pCtx->pVm;
	if( zClass == 0 || zClass[0] == 0 ){
		zClass = "Error";
	}

	SyBlobInit(&sMsg,&pVm->sAllocator);

	va_start(ap,zFormat);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	va_end(ap);

	if( pCtx->pFunc ){
		zFuncName = pCtx->pFunc->sName.zString;
		nFuncLen = (int)pCtx->pFunc->sName.nByte;
	}
	if( zFuncName == 0 || nFuncLen <= 0 ){
		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);
	}
	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),
		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Save the execution state of a fiber/generator context.
 * This may be called multiple times as PH7_SUSPEND propagates up through
 * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own
 * values, so the last (outermost) call wins — which is the fiber's own level.
 * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx
 * when VmByteCodeExec returns.
 */
static sxi32 VmSuspendCtx(
	ph7_vm *pVm,
	ph7_exec_ctx *pCtx,
	sxi32 pc,
	sxi32 nTos
	)
{
	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */
	pCtx->pc = pc;
	pCtx->nTos = nTos;
	pCtx->iState = PH7_CTX_STATE_SUSPENDED;
	return PH7_SUSPEND;
}
/*
 * Resolve named-argument mapping.
 *
 * For each actual argument in the call, determine which formal parameter it
 * maps to (by name or by position).  On success, aSlot[i] contains the
 * formal-parameter index for actual arg i, -1 if it overflows into the
 * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for
 * every formal parameter that received a value.
 *
 * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,
 * positional-overlaps-named) it calls VmThrowNamedArgError and returns
 * PH7_ABORT so the caller can jump to its Abort label.
 */
static sxi32 VmResolveNamedArgs(
	ph7_vm *pVm,
	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */
	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */
	sxu32 nNonVariadic,           /* Number of non-variadic formal params */
	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */
	sxu32 nActual,                /* Number of actual arguments on the stack */
	sxi32 *aSlot,                 /* OUT: mapping actual->formal */
	sxu8  *aUsed                  /* OUT: which formals are used */
)
{
	sxi32 posIdx = 0;
	sxu32 i;
	char zErrMsg[256];
	SyZero(aUsed, nNonVariadic * sizeof(sxu8));
	for( i = 0; i < nActual; i++ ){
		aSlot[i] = -2;
	}
	for( i = 0; i < nActual; i++ ){
		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){
			/* Named argument — find formal by name */
			int found = 0;
			sxu32 k;
			for( k = 0; k < nNonVariadic; k++ ){
				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte
					&& SyMemcmp(aFormalArg[k].sName.zString,
						pMap->aNames[i].zString,
						pMap->aNames[i].nByte) == 0 ){
					if( aUsed[k] ){
						SyBufferFormat(zErrMsg,sizeof(zErrMsg),
							"Named parameter $%.*s overwrites previous argument",
							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);
						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));
						return PH7_ABORT;
					}
					aSlot[i] = (sxi32)k;
					aUsed[k] = 1;
					found = 1;
					break;
				}
			}
			if( !found ){
				if( iVariadicIdx >= 0 ){
					aSlot[i] = -1; /* goes to variadic with string key */
				}else{
					SyBufferFormat(zErrMsg,sizeof(zErrMsg),
						"Unknown named parameter $%.*s",
						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);
					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));
					return PH7_ABORT;
				}
			}
		}else{
			/* Positional argument */
			if( (sxu32)posIdx < nNonVariadic ){
				if( aUsed[posIdx] ){
					SyBufferFormat(zErrMsg,sizeof(zErrMsg),
						"Named parameter $%.*s overwrites previous argument",
						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);
					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));
					return PH7_ABORT;
				}
				aSlot[i] = posIdx;
				aUsed[posIdx] = 1;
			}else if( iVariadicIdx >= 0 ){
				aSlot[i] = -1; /* overflow to variadic */
			}
			posIdx++;
		}
	}
	return SXRET_OK;
}
/*
 * Is this value an object implementing Traversable (Iterator / IteratorAggregate
 * / Generator)? Used by the spread sites to decide whether to unpack it.
 */
static int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)
{
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 || pVal->x.pOther == 0 || pVm->pTraversableClass == 0 ){
		return 0;
	}
	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);
}
/*
 * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:
 * merge each element with PHP 8.1 array-unpack key rules — string keys are
 * preserved (later wins), integer keys are renumbered.
 */
static sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;
	(void)pVm;
	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);
	return SXRET_OK;
}
/*
 * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:
 * collect values positionally (keys ignored) into a temp array.
 */
static sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	(void)pVm; (void)pKey;
	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);
	return SXRET_OK;
}
/*
 * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a
 * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the
 * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — the two paths
 * disagree on which scalar types are silent (positional silences null+bool; keyed silences
 * only null, warning for bool to match PHP) — this only maps the type name and emits.
 */
static void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)
{
	const char *zType = "unknown";
	char zMsg[64];
	if( iFlags & MEMOBJ_STRING ){
		zType = "string";
	}else if( iFlags & MEMOBJ_REAL ){
		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL|MEMOBJ_INT (see the
		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no
		 * REAL flag, so it still falls through to the int arm. */
		zType = "float";
	}else if( iFlags & MEMOBJ_INT ){
		zType = "int";
	}else if( iFlags & MEMOBJ_BOOL ){
		zType = "bool";
	}else if( iFlags & MEMOBJ_OBJ ){
		zType = "object";
	}else if( iFlags & MEMOBJ_RES ){
		zType = "resource";
	}
	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);
	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);
}
/*
 * A member access in isset()/empty() context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY) is a silent
 * lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class instance"
 * warnings, mirroring the array isset/empty path.
 */
static int VmMemberCtxIsLookup(sxi32 iP2)
{
	return iP2 == PH7_MEMBER_ISSET || iP2 == PH7_MEMBER_EMPTY;
}
/*
 * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a
 * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be
 * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms
 * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family
 * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes
 * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER
 * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).
 * One-token lookahead only.
 */
static int VmMemberNextIsWrite(const VmInstr *pNext)
{
	switch( pNext->iOp ){
		case PH7_OP_STORE:
			return pNext->iP2 != 0;                          /* member store ($o->p = v) */
		case PH7_OP_INCR: case PH7_OP_DECR:
		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:
		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:
		case PH7_OP_CAT_STORE:
		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:
		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:
			return 1;
		default:
			return 0;
	}
}
/*
 * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this
 * was an IteratorAggregate foreach), free the step, pop it off the info's
 * step stack and drop the step's retain on the iterator instance. The single
 * home for this teardown — it runs on iterator exhaustion AND on every
 * iterator-protocol throw path (next/valid/current/key); a per-site copy that
 * drifts produces a leak or pool-masked use-after-free on exactly one throw
 * path (the SyHash-layout incident class).
 */
static void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)
{
	if( pStep->pOwner ){
		PH7_ClassInstanceUnref(pStep->pOwner);
	}
	SyMemBackendPoolFree(&pVm->sAllocator,pStep);
	SySetPop(&pInfo->aStep);
	PH7_ClassInstanceUnref(pThis);
}
/*
 * Boundary state of one VmByteCodeExec activation (BYTECODE.md stage 1):
 * everything the executor must restore to continue an activation after a
 * nested call returns. pc/pTos are authoritative here only at activation
 * boundaries — the dispatch loop keeps them in locals for the hot path and
 * syncs around the call epilogue and the terminal labels. Stage 2 stacks
 * these records to replace the native recursion.
 */
typedef struct VmExecState VmExecState;
struct VmExecState
{
	VmInstr *aInstr;        /* Bytecode of this activation */
	ph7_value *pStack;      /* Operand-stack base (owned by this activation) */
	ph7_value *pTos;        /* Top-of-stack (synced at boundaries) */
	sxi32 pc;               /* Program counter (synced at boundaries) */
	sxu32 nExceptionBase;   /* Exception-stack depth at entry (finally-drain floor) */
	VmFrame *pEntryFrame;   /* Active frame at entry (exec identity for VmRecordedResume) */
	ph7_value *pResult;     /* Where the terminal OP_DONE stores the result (or NULL) */
	sxu32 *pLastRef;        /* By-ref return out-param (or NULL) */
	ph7_vm_func *pEnforceRetFunc; /* Return-type enforcement target (user-fn bodies only) */
	sxu8 is_callback;       /* TRUE only for a C->PHP callback trampoline activation */
	sxu8 bReturnPropagates; /* TRUE only for a catch/finally mini-program */
};
/*
 * One in-flight user-function call: what the caller's OP_CALL set up and the
 * pop boundary (VmCallFinish) must tear down.
 */
typedef struct VmCallRecord VmCallRecord;
struct VmCallRecord
{
	ph7_vm_func *pVmFunc;   /* Callee */
	VmFrame *pFrame;        /* Callee's VM frame (entered by the OP_CALL setup) */
	ph7_value *pFrameStack; /* Callee's operand stack (owned; freed here). NULL when the body was skipped */
	sxu32 nLastRef;         /* Callee body's last-referenced slot (by-ref return) */
	sxu8 bSelfPushed;       /* TRUE when the setup pushed onto pVm->aSelf */
};
/*
 * One node of the in-loop call-record stack (BYTECODE stage 2): the caller's
 * activation to restore plus the in-flight call to finish, linked to the
 * next-outer record. Nodes are pool-allocated individually so pointers into
 * them (sState.pLastRef aims at sCall.nLastRef while the callee runs) stay
 * stable — a growable array would invalidate them on realloc. The stack is a
 * LOCAL of each native VmByteCodeExec invocation: an inner native entry
 * (mini-program, C->PHP callback, ctx resume) can never unwind records that
 * belong to an outer invocation, preserving the old nesting isolation by
 * construction.
 */
typedef struct VmCallFrame VmCallFrame;
struct VmCallFrame
{
	VmExecState sCaller;   /* Caller activation, restored on pop */
	VmCallRecord sCall;    /* The in-flight call, finished (VmCallFinish) on pop */
	VmCallFrame *pPrev;    /* Next-outer record, or NULL at this invocation's base */
};
/*
 * BYTECODE stage 4: a Fiber::suspend() from inside a nested PHP call parks the
 * whole trampoline record segment here instead of unwinding it. The records,
 * their VmFrames and operand stacks all stay alive on the heap (that IS what a
 * suspended fiber is); only the dispatch loop's pointers move into the ctx.
 * Resume re-pushes the chain and continues INSIDE the innermost callee.
 */
typedef struct VmParkedSegment VmParkedSegment;
struct VmParkedSegment
{
	VmExecState sState;    /* Innermost activation — resume re-enters here (pTos synced) */
	VmCallFrame *pCallTop; /* Parked record chain (caller activations toward the body) */
	VmFrame *pTopFrame;    /* pVm->pFrame at suspend (innermost callee / open-try frame) */
	sxu32 nOldExcBase;     /* pCtx->nExceptionBase at park — resume rebases the segment's
	                        * absolute nExceptionBase floors by (newBase - nOldExcBase) */
	int nRecords;          /* Chain length: each record contributed one nRecursionDepth++
	                        * (and, if bSelfPushed, one aSelf push) that VmCallFinish never
	                        * ran. Deactivate that accounting while parked, reactivate on
	                        * resume; an abandoned segment stays deactivated. */
};
/*
 * Terminal teardown of one VmByteCodeExec activation — the former
 * Done/Suspend/Abort/Exception label bodies, one home (BYTECODE.md stage 1).
 *
 * SXRET_OK (Done): whenever the REAL body returns, its pending-return slot
 * must be empty — the materialize at OP_DONE/OP_POP_EXCEPTION already moved
 * the value into pResult and cleared bHasRet, so the clear is normally a
 * no-op; it only fires on a path that reached Done with a stale slot,
 * preventing a leak. The !bReturnPropagates guard is essential: a
 * catch/finally MINI-PROGRAM runs in its body's own frame (VmLocalExec adds
 * no frame), so pEntryFrame is that body — wiping its slot would destroy the
 * return the body is about to take.
 * PH7_SUSPEND: a generator/fiber body never suspends mid-completion of a
 * catch/finally return, so its frame's slot is empty (nothing to clear) and
 * its operand stack is preserved in place — the ctx owns it.
 * PH7_ABORT / PH7_EXCEPTION: abnormal unwind — discard the body's pending
 * return (an escaping exception supersedes it, per PHP) and release every
 * live operand slot down to the activation's stack base.
 */
static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)
{
	SXUNUSED(pVm);
	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){
		VmClearFrameReturn(pState->pEntryFrame);
	}
	SySetRelease(pArg);
	if( rcTerm == PH7_ABORT || rcTerm == PH7_EXCEPTION ){
		while( pTos >= pState->pStack ){
			PH7_MemObjRelease(pTos);
			pTos--;
		}
	}
	return rcTerm;
}
/*
 * Finish one user-function call at the "pop" boundary of the callee's
 * activation: pop-time accounting (recursion depth, aSelf), by-ref-return
 * fixup, callee-threw routing (inline resume / recorded resume / propagate),
 * operand-stack free and frame teardown. Extracted verbatim from the OP_CALL
 * epilogue (BYTECODE.md stage 1) so the stage-2 trampoline can run the same
 * code when a record is popped at OP_DONE instead of after a native return.
 * pCaller->pc / pCaller->pTos are authoritative across this boundary; the
 * dispatch loop syncs its locals around the call. Returns PH7_OK (continue
 * the caller, possibly at a redirected pc), PH7_ABORT, PH7_SUSPEND (the ctx
 * state was re-saved at the caller's level) or PH7_EXCEPTION.
 */
static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)
{
	ph7_value *pObj;
	/* Decrement nesting level */
	pVm->nRecursionDepth--;
	if( pCallee->bSelfPushed ){
		/* Pop class name */
		(void)SySetPop(&pVm->aSelf);
	}
	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){
		/* Return by reference,reflect that */
		if( pCallee->nLastRef != SXU32_HIGH ){
			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);
			sxu32 i;
			/* Make sure the referenced object is not a local variable */
			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){
				if( pCallee->nLastRef == aSlot[i].nIdx ){
					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pCallee->nLastRef);
					if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_OBJ|MEMOBJ_HASHMAP|MEMOBJ_RES)) == 0 ){
						VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
							"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",
							&pCallee->pVmFunc->sName);
					}
					pCallee->nLastRef = SXU32_HIGH;
					break;
				}
			}
		}else{
			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_NULL|MEMOBJ_RES)) == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",
					&pCallee->pVmFunc->sName);
			}
		}
		pCaller->pTos->nIdx = pCallee->nLastRef;
	}
	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) || rc == PH7_EXCEPTION) ){
		/* The callee threw (or its finally threw past it). If an in-place catch
		 * recorded a resume target owned by THIS caller's body, resume there and
		 * consume the target (VmRecordedResume); when the catcher is an outer exec
		 * — or this is a callback with no bytecode to resume into — propagate so
		 * the owning exec lands. This replaces the old "is the caller's parent a
		 * resumable try frame" test, which resumed at the caller's OWN try even
		 * when the finally's throw was caught further out, losing that catch's
		 * return (ROOT B, face c). */
		sxi32 iResumePc;
		VmFrame *pParentFrame = pCallee->pFrame->pParent;
		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){
			/* ROOT C: the callee's throw was caught by an inline try in THIS caller
			 * (generator body). Drain the operand stack (incl. the unwritten result
			 * slot) to the try's base and land at its catch/finally. */
			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){
				PH7_MemObjRelease(pCaller->pTos);
				pCaller->pTos--;
			}
			pCaller->pc = (sxi32)pVm->iInlinePc - 1;
			pVm->pInlineInstr = 0;
			rc = PH7_OK;
		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){
			/* Pop the result */
			VmPopOperand(&pCaller->pTos,1);
			pCaller->pc = iResumePc;
			rc = PH7_OK;
		}else{
			if( pParentFrame->pParent ){
				rc = PH7_EXCEPTION;
			}else{
				/* Continue normal execution */
				rc = PH7_OK;
			}
		}
	}
	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),
	 * or free it if the pool is full. Its allocated size is the callee's cached
	 * nMaxStack + VM_STACK_GUARD — exactly what VmOperandStackAlloc handed out.
	 * (NULL when the function body was skipped.)
	 *
	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,
	 * where a deep suspend parks its whole record segment before reaching here)
	 * would leave the callee stack owned by the suspended ctx, so recycling it
	 * would hand a live fiber's operand stack to the next call. The guard keeps
	 * that invariant explicit and robust to future coroutine changes. */
	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){
		VmOperandStackRecycle(pVm,pCallee->pFrameStack,
			pCallee->pVmFunc->nMaxStack + VM_STACK_GUARD);
	}
	/* Leave the frame */
	VmLeaveFrame(&(*pVm));
	if( rc == PH7_ABORT ){
		return PH7_ABORT;
	}
	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){
		/* A Fiber::suspend() was called somewhere inside this function.
		 * Re-save the fiber's state at THIS level (the fiber's body),
		 * overwriting the state saved by the inner level.
		 * pTos points to the result slot (not yet written).
		 * Save nTos one below so resume pushes at the result slot. */
		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);
		return PH7_SUSPEND;
	}
	if( rc == PH7_EXCEPTION ){
		return PH7_EXCEPTION;
	}
	return PH7_OK;
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
static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,
	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,
	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment);
/*
 * Native-nesting guard around the executor. PHP->PHP calls run iteratively
 * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,
 * C->PHP callbacks, ctx start/resume, eval/include — is still one real C
 * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is
 * PHP call depth, raisable to memory-bound values since the clamp removal),
 * so this counter is what actually protects the C stack: recursive
 * eval/include towers, nested coroutine-resume chains and self-recursive
 * C-callback compositions hit a clean fatal instead of overflowing. The limit
 * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-
 * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is
 * still a coarse frame-count net rather than php's stack-byte measurement, so
 * the host default is conservative — well below the old config clamp's <1024
 * ceiling so it holds on the fattest frames (the callback path drags in
 * usort/mergesort/trampoline C frames per re-entry, and instrumented builds
 * inflate every frame), while far beyond any realistic eval/include/callback
 * nesting.
 */
static sxi32 VmByteCodeExec(
	ph7_vm *pVm,         /* Target VM */
	VmInstr *aInstr,     /* PH7 bytecode program */
	ph7_value *pStack,   /* Operand stack */
	int nTos,            /* Top entry in the operand stack (usually -1) */
	ph7_value *pResult,  /* Store program return value here. NULL otherwise */
	sxu32 *pLastRef,     /* Last referenced ph7_value index */
	int is_callback,     /* TRUE if we are executing a callback */
	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */
	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */
	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */
	VmParkedSegment *pAdoptSegment /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */
	)
{
	sxi32 rc;
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	pVm->nVmExecDepth++;
	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,
		pEnforceRetFunc,bReturnPropagates,pAdoptSegment);
	pVm->nVmExecDepth--;
	return rc;
}
static sxi32 VmByteCodeExecBody(
	ph7_vm *pVm,         /* Target VM */
	VmInstr *aInstr,     /* PH7 bytecode program */
	ph7_value *pStack,   /* Operand stack */
	int nTos,            /* Top entry in the operand stack (usually -1) */
	ph7_value *pResult,  /* Store program return value here. NULL otherwise */
	sxu32 *pLastRef,     /* Last referenced ph7_value index */
	int is_callback,     /* TRUE if we are executing a callback */
	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */
	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */
	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */
	VmParkedSegment *pAdoptSegment /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */
	)
{
	VmInstr *pInstr;
	ph7_value *pTos;
	SySet aArg;
	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record
	                            * stack (BYTECODE stage 2); NULL = executing the
	                            * bottom activation. */
	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):
	                     * everything a suspended/nested activation must restore.
	                     * pc/pTos stay in locals for the hot loop and are synced
	                     * into sState only around the call epilogue (stage 2 turns
	                     * that boundary into an explicit record push/pop). */
	sxi32 pc;
	sxi32 rc;
	sState.aInstr = aInstr;
	sState.pStack = pStack;
	sState.pResult = pResult;
	sState.pLastRef = pLastRef;
	sState.pEnforceRetFunc = pEnforceRetFunc;
	sState.is_callback = (sxu8)(is_callback ? 1 : 0);
	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);
	/* Argument container */
	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
	if( nTos < 0 ){
		pTos = &pStack[-1];
	}else{
		pTos = &pStack[nTos];
	}
	sState.pTos = pTos;
	sState.pc = nPc;
	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own
	 * exception handlers were just re-published above the caller depth
	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the
	 * caller-depth base recorded on the ctx instead.
	 *
	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on
	 * the ctx's own pStack, whereas every nested frame-less mini-program run within
	 * it — a default-argument / constructor trampoline, or a catch/finally body via
	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those
	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires
	 * for such a mini-program and hands it the generator's low caller-base; its
	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a
	 * live try that the surrounding finally had just opened (e.g. `finally { try {
	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's
	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped
	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed
	 * body itself; nested mini-programs fall through to the correct live depth. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame
	 && pStack == pVm->pActiveCtx->pStack ){
		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;
	}else{
		sState.nExceptionBase = SySetUsed(&pVm->aException);
	}
	sState.pEntryFrame = pVm->pFrame;
	pc = nPc;
	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call
	 * adopts its parked record segment here. VmResumeCtx hands the segment in
	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a
	 * nested mini-program/callback run inside the resumed body — after it rebased
	 * the segment's exception floors and pushed the resume value into the innermost
	 * stack. Locals switch to the innermost activation so the dispatch loop
	 * continues inside the callee; the record chain is restored so its completion
	 * unwinds back through the body. */
	if( pAdoptSegment ){
		VmParkedSegment *pSeg = pAdoptSegment;
		pCallTop = pSeg->pCallTop;
		sState = pSeg->sState;
		aInstr = sState.aInstr;
		pStack = sState.pStack;
		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the
		 * init above; only the stack/top move to the innermost activation. */
		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */
		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */
	}
/*
 * Route an enforcement helper's (or yield-from delegate's) return code from inside
 * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION
 * resume at the landing pad of the body that actually caught the exception in place
 * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM
 * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran
 * the statement after the try even when the catch was at an enclosing frame (ROOT B,
 * face b — `yield from` over a throwing sub-generator).
 */
/*
 * ROOT C: if a throw was caught by an INLINE try owned by THIS exec, drain the
 * abandoned operand slots back to the try's stack base and jump to the catch/finally
 * body; the throw runs in this same dispatch loop (so a yield inside it suspends).
 * pInlineInstr != aInstr means an outer exec owns the handler → fall through and
 * propagate. Must be used inside a case of the main switch (uses break/pc/pTos).
 */
#define PH7_INLINE_RESUME_BREAK() \
	if( pVm->pInlineInstr == (void *)aInstr ){ \
		while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){ PH7_MemObjRelease(pTos); pTos--; } \
		pc = (sxi32)pVm->iInlinePc - 1; \
		pVm->pInlineInstr = 0; \
		break; \
	}
#define PH7_DISPATCH_ENFORCE_RC(rcVar) \
	if( (rcVar) == PH7_ABORT ){ goto Abort; } \
	if( (rcVar) == PH7_EXCEPTION || pVm->pInlineInstr ){ \
		sxi32 _iRpE; \
		PH7_INLINE_RESUME_BREAK() \
		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ \
			pc = _iRpE; \
			break; \
		} \
		if( (rcVar) == PH7_EXCEPTION ){ goto Exception; } \
	}
/*
 * Route the status of an iterator-protocol method call (foreach over an
 * Iterator/IteratorAggregate/Generator: rewind/valid/current/next/key/
 * getIterator) from inside the OP_FOREACH_INIT / OP_FOREACH_STEP cases.
 * Before invoking this, the site must have released its transients and
 * detached/freed its foreach step (each site's teardown differs) — these
 * calls used to IGNORE their status entirely, so an exception thrown by a
 * generator body (or a userland Iterator method) during foreach silently
 * ended the loop and execution continued after it (php: the exception
 * propagates; uncaught → fatal + exit 255). nPopOnResume operands are
 * consumed when an in-place catch resumes this body, mirroring the op's
 * normal stack effect. Must be used directly inside a case of the main
 * switch (uses break/pc/pTos, like PH7_DISPATCH_ENFORCE_RC).
 */
#define PH7_DISPATCH_ITER_RC(rcVar,nPopOnResume) \
	if( (rcVar) == PH7_ABORT ){ goto Abort; } \
	{ \
		sxi32 _iRpI; \
		PH7_INLINE_RESUME_BREAK() \
		if( VmRecordedResume(pVm,&_iRpI,sState.pEntryFrame,aInstr) ){ \
			if( (nPopOnResume) > 0 ){ VmPopOperand(&pTos,(nPopOnResume)); } \
			pc = _iRpI; \
			break; \
		} \
		if( (rcVar) == PH7_EXCEPTION ){ goto Exception; } \
	}
/*
 * The routing condition for the above: the iterator-protocol call aborted,
 * threw past this exec, or was caught by an inline try THIS exec owns
 * (pInlineInstr identity). Named once so the seven foreach sites cannot
 * drift; a missed edit here would silently re-swallow exceptions.
 */
#define VmIterCallThrew(rcVar) \
	((rcVar) == PH7_ABORT || (rcVar) == PH7_EXCEPTION || pVm->pInlineInstr == (void *)aInstr)
/*
 * Typed-property enforcement helper for compound stores. Called before
 * PH7_MemObjStore writes into a member memobj slot. On failure throws a
 * PHP TypeError and either jumps to the nearest catch block or propagates
 * out of the VM loop. Must be used inside a case of the main switch.
 */
#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \
	{ \
		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \
		PH7_DISPATCH_ENFORCE_RC(_rcT) \
	}
/*
 * Readonly enforcement helper for the in-place mutation opcodes (`++`/`--`),
 * which bypass the typed-store path. Throws "Cannot modify readonly property"
 * when the lvalue slot is a readonly property, otherwise proceeds. Must be used
 * inside a case of the main switch.
 */
#define PH7_ENFORCE_READONLY_MUTATE(nIdxArg) \
	{ \
		sxi32 _rcR = VmCheckReadonlyMutate(&(*pVm),(nIdxArg)); \
		PH7_DISPATCH_ENFORCE_RC(_rcR) \
	}
	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber
	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch
	 * loop (pc is already at the resume point) — so the existing OP_THROW route
	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing
	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because
	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why
	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally
	 * identical to a `throw` executed at the yield point: if the generator's own try catches
	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands
	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at
	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot
	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch
	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected
	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame ){
		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;
		VmFrame *pThrowFrame;
		sxi32 iResumePc;
		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */
		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);
		pThrowFrame->iFlags |= VM_FRAME_THROW;
		rc = VmThrowException(&(*pVm),pInj);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		if( pVm->pInlineInstr == (void *)aInstr ){
			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the
			 * abandoned mid-expression operands and land at the catch/finally body. This is
			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */
			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			pc = (sxi32)pVm->iInlinePc;
			pVm->pInlineInstr = 0;
		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a
			 * catch recorded a resume target for this exec, so rc need not be pre-checked;
			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just
			 * propagates.) Drain the abandoned mid-expression operand slots back to the
			 * catching try's base, then land at its pad (iResumePc is landing-1 for the
			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,
			 * so add 1 to land on the pad itself). */
			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			pc = iResumePc + 1;
		}else{
			/* Not caught in this generator (no match, or caught by an outer/caller frame
			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx
			 * closes and the caller sees the exception. */
			goto Exception;
		}
	}
	/* Execute as much as we can */
	for(;;){
VmLoopFetch:
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
	if( pInstr->iP2 && sState.bReturnPropagates ){
		/* Explicit `return` inside a catch/finally mini-program. Defer the value
		 * onto the body frame this catch/finally returns from (skip the transparent
		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION
		 * materializes it into sState.pResult. Drain any finally opened within this body
		 * first (nested try/finally inside the catch), which may overwrite the same
		 * frame's slot (finally-over-catch). */
		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);
		if( pInstr->iP1 && pTos >= pStack ){
			PH7_MemObjStore(pTos,&pTgt->sRet);
			VmPopOperand(&pTos,1);
		}else{
			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */
		}
		pTgt->bHasRet = 1;
		pTgt->nRetGen++;
		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		if( rc == PH7_EXCEPTION ){
			/* A drained finally threw past itself — it discards this return. */
			goto Exception;
		}
		goto Done;
	}
	/* Return-type enforcement: only the user-function CALL handler (and
	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is
	 * skipped for default-value bytecode, class-method mini-programs,
	 * callback trampolines, and the main script. */
	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)
	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){
		/* The VM_FRAME_THROW guard skips enforcement when the function is
		 * unwinding because an exception was thrown (the compiler routes an
		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a
		 * value the function never actually returned, so enforcing here would
		 * raise a spurious "Return value must be of type X" over the real
		 * exception. */
		ph7_value *pRetVal = 0;
		if( pInstr->iP1 && pTos >= pStack ){
			pRetVal = pTos;
		}
		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);
		if( rc == PH7_ABORT ) goto Abort;
		if( rc == PH7_EXCEPTION ){
			if( pInstr->iP1 && pTos >= pStack ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			goto Exception;
		}
		/* Don't enforce twice if the function loops through multiple
		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but
		 * defensively we clear the pointer after a successful check). */
		sState.pEnforceRetFunc = 0;
	}
	if( pInstr->iP1 && pTos >= pStack ){
		if( sState.pLastRef ){
			*sState.pLastRef = pTos->nIdx;
		}
		if( sState.pResult ){
			/* Execution result */
			PH7_MemObjStore(pTos,sState.pResult);
		}
		VmPopOperand(&pTos,1);
	}else if( sState.pLastRef ){
		/* Nothing referenced — also the throw-unwind path: the compiler routes
		 * an uncaught exception to this terminal OP_DONE with iP1 set but an
		 * empty operand stack (pTos == pStack-1), so there is no return value to
		 * store. Guarding on pTos >= pStack (matching the two sibling branches
		 * above) avoids the below-base read that crashed under glibc/ASan. */
		*sState.pLastRef = SXU32_HIGH;
	}
	/* Execute pending finally blocks for any try/catch contexts pushed during
	 * this execution. When 'return' is used inside a try block,
	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before
	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering
	 * with exception contexts from an outer VmByteCodeExec invocation.
	 * This runs AFTER storing the return value so that 'return' in a finally
	 * block can override it (the finally writes this body frame's sRet slot,
	 * materialized below).
	 */
	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);
	if( rc == SXERR_ABORT ){
		goto Abort;
	}
	if( rc == PH7_EXCEPTION ){
		/* A drained finally threw past itself, discarding the value this OP_DONE
		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new
		 * exception in place, resume at its landing pad; otherwise unwind (the
		 * caller's exception-resume pops the stored result). */
		sxi32 iResumePc;
		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
			pc = iResumePc;
			break;
		}
		goto Exception;
	}
	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){
		/* A catch/finally issued a 'return' targeting THIS body. If the body is
		 * actually unwinding because an exception escaped it (terminal OP_DONE on
		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type
		 * enforcement above), that exception supersedes the return: discard it.
		 * Otherwise materialize it as this function's result. */
		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){
			VmClearFrameReturn(sState.pEntryFrame);
		}else{
			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);
		}
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
		if( sState.pLastRef ){
			*sState.pLastRef = pTos->nIdx;
		}
		if( pTos->iFlags & MEMOBJ_STRING ){
			if( SyBlobLength(&pTos->sBlob) > 0 ){
				/* Output the exit message */
				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),
					pVm->sVmConsumer.pUserData);
				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));
			}
		}else if(pTos->iFlags & MEMOBJ_INT ){
			/* Record exit status */
			pVm->iExitStatus = (sxi32)pTos->x.iVal;
		}
		VmPopOperand(&pTos,1);
	}else if( sState.pLastRef ){
		/* Nothing referenced */
		*sState.pLastRef = SXU32_HIGH;
	}
	/* Request a VM-wide halt so the abort cascades out of any enclosing
	 * include/require/eval execution unit; shutdown callbacks then run
	 * at the top level (PHP semantics) instead of hard-exiting here.
	 */
	pVm->bHaltRequested = 1;
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
 * DUP: * * *
 *
 * Duplicate the top of the stack.
 */
case PH7_OP_DUP:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	pTos++;
	PH7_MemObjInit(pVm,pTos);
	PH7_MemObjStore(pTos - 1,pTos);
	break;
/*
 * NSSWITCH: * * P3
 *
 * Switch the active namespace at runtime.
 * P3 points to the namespace string (pool-allocated, NULL for global).
 */
case PH7_OP_NSSWITCH:
	SyBlobReset(&pVm->sNamespace);
	if( pInstr->p3 ){
		const char *zNs = (const char *)pInstr->p3;
		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));
	}
	/* Clear namespace-scoped use-const imports */
	SyHashRelease(&pVm->hUseConstImports);
	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);
	break;
/* OP_USECONST P1 * P3
 * Register a use-const import at runtime. P1 is the alias length,
 * P3 points to a two-pointer array: [0]=alias, [1]=FQN.
 * This is namespace-scoped: NSSWITCH clears all imports.
 */
case PH7_OP_USECONST: {
	char **azPair = (char **)pInstr->p3;
	if( azPair ){
		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);
	}
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
/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it
 * always raises "The (unset) cast is no longer supported" (php 8 removed
 * the cast), so a program containing it never compiles. The switch has no
 * default arm, so abort loudly rather than fall through as a silent no-op
 * if a future emitter ever produces one without the compile error. */
case PH7_OP_CVT_NULL:
	goto Abort;
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
			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);
			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);
			/* Handle self/static/parent keywords */
			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){
				pClass = PH7_VmPeekDeclaringClass(&(*pVm));
			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
				pClass = PH7_VmPeekTopClass(&(*pVm));
			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));
				if( pSelf && pSelf->pBase ){
					pClass = pSelf->pBase;
				}
			}else{
				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
			}
		}
		if( pClass ){
			/* Perform the query */
			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);
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
		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){
			SyHashEntry *pEntry;
			/* Check use const imports first — imports take precedence */
			{
				SyHashEntry *pConstImport;
				pConstImport = SyHashGet(&pVm->hUseConstImports,
					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));
				if( pConstImport ){
					const char *zFQN = (const char *)pConstImport->pUserData;
					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));
					if( pEntry ){
						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;
						MemObjSetType(pTos,MEMOBJ_NULL);
						SyBlobReset(&pTos->sBlob);
						pCons->xExpand(pTos,pCons->pUserData);
						pTos->nIdx = SXU32_HIGH;
						break;
					}
					/* Import found but constant not defined — fall through */
				}
			}
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
			/* Constant not found by bare name.  If a namespace is active and
			 * the name is unqualified, try namespace\name (PHP resolution order:
			 * use-const imports → current NS → global → string fallback).
			 * Absolute references (\NAME) skip the NS fallback too. */
			{
				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);
				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);
				sxu32 j;
				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;
				for( j = 0; !isQualified && j < nLit; j++ ){
					if( zLit[j] == '\\' ){ isQualified = 1; break; }
				}
				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){
					/* Try current_namespace\name */
					SyBlobReset(&pVm->sWorker);
					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));
					SyBlobAppend(&pVm->sWorker,"\\",1);
					SyBlobAppend(&pVm->sWorker,zLit,nLit);
					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));
					if( pEntry ){
						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;
						MemObjSetType(pTos,MEMOBJ_NULL);
						SyBlobReset(&pTos->sBlob);
						pCons->xExpand(pTos,pCons->pUserData);
						pTos->nIdx = SXU32_HIGH;
						break;
					}
					/* Not in current namespace either — fall through to global/string */
				}
				if( isQualified ){
					/* Qualified name: must be a real constant. */
					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);
					SyBlob sErr;
					SyBlobInit(&sErr,&pVm->sAllocator);
					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);
					if( pErrFile ){
						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);
					}
					SyBlobAppend(&sErr,"\n",1);
					VmCallErrorHandler(&(*pVm),&sErr);
					SyBlobRelease(&sErr);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					goto LoadC_Done;
				}
			}
		}
		PH7_MemObjLoad(pObj,pTos);
	}else{
		/* Set a NULL value */
		MemObjSetType(pTos,MEMOBJ_NULL);
	}
LoadC_Done:
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
		sxi32 rcSpread = SXRET_OK;
		/* Perform the insertion */
		while( pEntry < pTos ){
			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){
				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1
				 * semantics — string keys preserved (later wins), int keys
				 * renumbered. Same routine that backs array_merge. */
				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){
					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);
					if( rcMerge != SXRET_OK ){
						/* Merge failure (OOM): match the PH7_NewHashmap OOM
						 * path — emit fatal and abort, leaving no partial
						 * map dangling. */
						VmErrorFormat(&(*pVm),PH7_CTX_ERR,
							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);
						rcSpread = PH7_ABORT;
						break;
					}
				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){
					/* Traversable unpacking (PHP 8.1): walk it into the map using the
					 * same key rules as array spread (string keys kept, int renumbered). */
					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);
					if( rcW == PH7_EXCEPTION || rcW == PH7_ABORT ){
						rcSpread = rcW;
						break;
					}
				}else{
					/* Throw a catchable Error matching PHP semantics. */
					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);
					break;
				}
			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){
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
		if( rcSpread != SXRET_OK ){
			/* Discard the partially-built map and propagate the exception. */
			PH7_HashmapRelease(pMap,TRUE);
			if( rcSpread == PH7_ABORT ){
				goto Abort;
			}
			{
				sxi32 iRp;
				if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
					pc = iRp;
					break;
				}
			}
			goto Exception;
		}
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
						/* Undefined array key */
						char zMsg[128];
						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);
						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);
						PH7_MemObjRelease(pObj);
					}
				}
			}
			sKey.x.iVal++; /* Next numeric index */
			pEntry++;
		}
	}else{
		/* Source is not an array */
		ph7_value *pObj;
		while( pEntry <= pTos ){
			if( pEntry->nIdx != SXU32_HIGH ){
				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){
					PH7_MemObjRelease(pObj);
				}
			}
			pEntry++;
		}
		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL|MEMOBJ_BOOL)) == 0 ){
			/* Positional list destructuring silences null+bool; warn for the rest. */
			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);
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
	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){
		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL
		 * (never char-index a string), warning once per key — matching PHP, which warns per
		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool
		 * source DOES warn (PHP warns for bool in keyed destructuring). */
		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);
		}
		if( pIdx ){
			/* Release the key (a string literal for keyed destructuring), like the
			 * normal hashmap-read exit below — otherwise its blob is orphaned. */
			PH7_MemObjRelease(pIdx);
		}
		PH7_MemObjRelease(pTos);
		MemObjSetType(pTos,MEMOBJ_NULL);
		break;
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
	if( pTos->iFlags & MEMOBJ_OBJ ){
		/* Object subscript: ArrayAccess dispatch.
		 * iP2 codes:
		 *   0 = read       → offsetGet
		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce
		 *                    target on miss for the upcoming NULLC_STORE
		 *   4 = isset()    → offsetExists
		 *   5 = unset()    → offsetUnset
		 *   6 = empty()    → offsetExists, then offsetGet on hit */
		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;
		ph7_class *pArrayAccess = pVm->pArrayAccessClass;
		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){
			ph7_class_method *pMeth;
			ph7_value sResult;
			ph7_value *apArg[1];
			if( (pInstr->iP2 == 0 || pInstr->iP2 == 3) && pIdx == 0 ){
				/* `$obj[]` read — PHP rejects this. */
				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,
					"Cannot use [] for reading");
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				break;
			}
			PH7_MemObjInit(&(*pVm),&sResult);
			if( pInstr->iP2 == 4 || pInstr->iP2 == 6 || pInstr->iP2 == 3 ){
				/* isset, empty, and ??= all start with offsetExists. */
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetExists",sizeof("offsetExists")-1);
				apArg[0] = pIdx;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);
				}
			}else if( pInstr->iP2 == 5 ){
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetUnset",sizeof("offsetUnset")-1);
				apArg[0] = pIdx;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);
				}
			}else{
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetGet",sizeof("offsetGet")-1);
				apArg[0] = pIdx;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);
				}
			}
			if( pInstr->iP2 == 4 ){
				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the
				 * right truth value AND skips its "Expecting a variable not
				 * a constant" warning (keyed on MEMOBJ_BOOL). */
				int bExists = ph7_value_to_bool(&sResult);
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				if( bExists ){
					MemObjSetType(pTos,MEMOBJ_BOOL);
					pTos->x.iVal = 1;
				}else{
					MemObjSetType(pTos,MEMOBJ_NULL);
				}
			}else if( pInstr->iP2 == 5 ){
				/* offsetUnset return is discarded; push NULL so the trailing
				 * vm_builtin_unset is a harmless no-op. */
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				MemObjSetType(pTos,MEMOBJ_NULL);
			}else if( pInstr->iP2 == 6 ){
				/* empty: if offsetExists is false, push NULL so empty=true
				 * without calling offsetGet. If true, call offsetGet and
				 * push the value so PH7_builtin_empty evaluates emptiness. */
				int bExists = ph7_value_to_bool(&sResult);
				PH7_MemObjRelease(&sResult);
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				if( !bExists ){
					MemObjSetType(pTos,MEMOBJ_NULL);
				}else{
					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,
						"offsetGet",sizeof("offsetGet")-1);
					ph7_value sValue;
					PH7_MemObjInit(&(*pVm),&sValue);
					apArg[0] = pIdx;
					if( pGet ){
						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);
					}
					PH7_MemObjStore(&sValue,pTos);
					PH7_MemObjRelease(&sValue);
				}
				if( pIdx ){ PH7_MemObjRelease(pIdx); }
				break; /* skip the duplicate sResult release below */
			}else if( pInstr->iP2 == 3 ){
				/* ?? null-coalesce peek: emulate PHP semantics —
				 *   if !offsetExists OR offsetGet() === null → arm
				 *     coalesce slot (NULLC_STORE will call offsetSet)
				 *     and push NULL.
				 *   else → push offsetGet's value (NULLC_JMP skips). */
				int bExists = ph7_value_to_bool(&sResult);
				int bShouldArm = !bExists;
				ph7_value sValue;
				PH7_MemObjRelease(&sResult);
				/* Reset any prior arming defensively */
				VmCoalesceDisarm(pVm);
				PH7_MemObjInit(&(*pVm),&sValue);
				if( bExists ){
					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,
						"offsetGet",sizeof("offsetGet")-1);
					apArg[0] = pIdx;
					if( pGet ){
						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);
					}
					if( sValue.iFlags & MEMOBJ_NULL ){
						bShouldArm = 1;
					}
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				if( bShouldArm ){
					/* Arm: remember (object, key) so NULLC_STORE dispatches
					 * to offsetSet. Hold a ref on the instance to survive
					 * intervening expression evaluation. */
					MemObjSetType(pTos,MEMOBJ_NULL);
					if( pIdx ){
						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);
					}
					pVm->pCoalesceObj = pInst;
					pInst->iRef++;
					pVm->bCoalesceArmed = 1;
				}else{
					PH7_MemObjStore(&sValue,pTos);
				}
				PH7_MemObjRelease(&sValue);
				if( pIdx ){ PH7_MemObjRelease(pIdx); }
				break;
			}else{
				/* offsetGet: replace pTos with the returned value. */
				PH7_MemObjRelease(pTos);
				PH7_MemObjStore(&sResult,pTos);
				pTos->nIdx = SXU32_HIGH;
			}
			PH7_MemObjRelease(&sResult);
			if( pIdx ){
				PH7_MemObjRelease(pIdx);
			}
			break;
		}
		/* Object without ArrayAccess: PHP throws fatal Error in all subscript
		 * contexts (read, isset, unset, empty). Match it. */
		if( pInst ){
			char zMsg[256];
			SyString *pName = &pInst->pClass->sName;
			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
				"Cannot use object of type %.*s as array",
				(int)pName->nByte,pName->zString);
			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);
			if( pIdx ){ PH7_MemObjRelease(pIdx); }
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH;
			if( rc == SXERR_ABORT ){ goto Abort; }
			break;
		}
	}
	if( (pInstr->iP2 == 1 || pInstr->iP2 == 3 || pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){
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
		if( pInstr->iP2 == 1 || pInstr->iP2 == 5 ){
			/* Write-context access (iP2 = create-if-missing).  COW-separate
			 * the parent so nested writes like $b[0][0] = 99 don't leak
			 * through shared outer arrays.  Read-only loads (iP2 == 0) must
			 * NOT separate — that would defeat COW on every element read.
			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the
			 * trailing unset() builtin can drop the slot via pTos->nIdx. */
			PH7_HashmapCowSeparate(&(*pVm),pTos);
		}
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pTos->x.pOther;
		if( pIdx ){
			/* Load the desired entry */
			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);
		}
		if( pInstr->iP2 == 3 ){
			/* Null coalescing assign peek mode: separate only when we will
			 * actually write back. If the looked-up value is non-null, the
			 * caller's NULLC_JMP will short-circuit and no store happens, so
			 * the parent can stay shared. If the value is null or the key is
			 * missing, separate and re-lookup so the upcoming NULLC_STORE
			 * writes into our own copy. Inner levels of a nested LHS still
			 * use iP2 == 1 (eager separation), which keeps the cascade
			 * correct for the outermost write. */
			int needWrite = (rc != SXRET_OK);
			if( !needWrite && pNode ){
				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
				if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
					needWrite = 1;
				}
			}
			if( needWrite ){
				PH7_HashmapCowSeparate(&(*pVm),pTos);
				if( pMap != (ph7_hashmap *)pTos->x.pOther ){
					/* The map was actually copied — re-lookup so pNode points
					 * into the new map's storage. */
					pMap = (ph7_hashmap *)pTos->x.pOther;
					if( pIdx ){
						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);
					}
				}
			}
		}
		if( rc != SXRET_OK && (pInstr->iP2 == 1 || pInstr->iP2 == 3 || pInstr->iP2 == 5) ){
			/* Create a new empty entry */
			rc = PH7_HashmapInsert(pMap,pIdx,0);
			if( rc == SXRET_OK ){
				/* Point to the last inserted entry */
				pNode = pMap->pLast;
			}else{
				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index
				 * is occupied threw php's catchable Error. Dispatch it here —
				 * falling through with a stale pMap->pLast is what silently
				 * overwrote $a[PHP_INT_MAX]. */
				PH7_DISPATCH_ENFORCE_RC(rc)
			}
		}
	}
	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){
		/* List destructuring context: emit PHP-compatible warning for missing key */
		char zMsg[128];
		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){
			PH7_MemObjToInteger(pIdx);
		}
		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);
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
	/* The function whose name the Closure object will wrap: a fresh per-instantiation
	 * copy for a real closure (built below), or the shared lambda function itself for a
	 * plain anonymous function with no captured environment. */
	ph7_vm_func *pTarget = pFunc;
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
		pClosure->nReturnType = pFunc->nReturnType;
		pClosure->sReturnClass = pFunc->sReturnClass;
		pClosure->aReturnUnion = pFunc->aReturnUnion;
		pClosure->sReturnTypeName = pFunc->sReturnTypeName;
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
		pTarget = pClosure;
	}
	/* Wrap the target function in a Closure object and push it. Its captured environment
	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call
	 * path when the closure is dispatched by name. */
	pTos++;
	{
		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);
		if( pCloObj ){
			pCloObj->iRef++;
			pTos->x.pOther = pCloObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
		}else{
			/* OOM fallback: the name string is still a usable callable. */
			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);
		}
	}
	break;
						 }
/*
 * LOAD_FCC P1 * *
 *
 * First-class callable: wrap the callee in a Closure object instead of calling it.
 *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS
 *           (from the callee's OP_LOADC). Replace it in place with a Closure whose
 *           $__fn is that name; the existing string-callable dispatch resolves it.
 *           (OOM degrades to leaving the name string on the stack — still callable.)
 *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name
 *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds
 *           $this (scope = its class); a class-name-string target is a static callable
 *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —
 *           the popped target leaves no name string to keep.)
 */
case PH7_OP_LOAD_FCC:{
	if( pInstr->iP1 == 1 ){
		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)
		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —
		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,
		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a
		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is
		 * (graceful degradation), and so is the original on OOM — still whatever it was. */
		ph7_class_instance *pCloObj;
		if( VmValueIsClosure(pVm, pTos) ){
			break;
		}
		pCloObj = VmFccWrapValue(pVm, pTos);
		if( pCloObj ){
			PH7_MemObjRelease(pTos);
			pCloObj->iRef++;
			pTos->x.pOther = pCloObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
		}
	}else{
		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]
		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its
		 * class); a class-name string target is a static callable (scope = that class). */
		ph7_value *pTarget = &pTos[-1];
		SyString sName;
		ph7_class_instance *pCloObj;
		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;
			pCloObj = VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);
		}else if( pTarget->iFlags & MEMOBJ_STRING ){
			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class
			 * now, so the closure binds the concrete scope (matching PHP). */
			ph7_class *pScopeCls = VmFccResolveScope(pVm, pTarget);
			pCloObj = pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;
		}else{
			pCloObj = 0;
		}
		/* Pop the method name and the target, push the Closure. */
		PH7_MemObjRelease(pTos);
		pTos--;
		PH7_MemObjRelease(pTos);
		if( pCloObj ){
			pCloObj->iRef++;
			pTos->x.pOther = pCloObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
		}else{
			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */
		}
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
		sxi32 rcT;
		/* Member store operation */
		nIdx = pTos->nIdx;
		VmPopOperand(&pTos,1);
		if( nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");
			pTos->nIdx = SXU32_HIGH;
		}else{
			/* Enforce typed property declaration if any. May coerce the
			 * incoming value in place (weak mode) or throw TypeError. */
			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);
			if( rcT == PH7_ABORT ){
				goto Abort;
			}
			if( rcT == PH7_EXCEPTION ){
				/* TypeError was thrown. Pop the rejected rvalue and hand
				 * control to the nearest catch block if any, otherwise
				 * propagate out of the VM loop. */
				VmPopOperand(&pTos,1);
				{
					sxi32 iRp;
					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
						pc = iRp;
						break;
					}
				}
				goto Exception;
			}
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
	{
		/* ArrayAccess::offsetSet dispatch.
		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via
		 * the backing variable slot at nIdx. */
		ph7_class_instance *pInst = 0;
		if( pTos->iFlags & MEMOBJ_OBJ ){
			pInst = (ph7_class_instance *)pTos->x.pOther;
		}else if( nIdx != SXU32_HIGH ){
			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){
				pInst = (ph7_class_instance *)pBacking->x.pOther;
			}
		}
		if( pInst ){
			ph7_class *pArrayAccess = pVm->pArrayAccessClass;
			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){
				ph7_class_method *pMeth;
				ph7_value sNullKey;
				ph7_value *apArg[2];
				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){
					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
						"Cannot assign by reference to overloaded object");
					if( pKey ){ PH7_MemObjRelease(pKey); }
					VmPopOperand(&pTos,2); /* container + value */
					break;
				}
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetSet",sizeof("offsetSet")-1);
				/* Pop container; pTos now points to the value */
				VmPopOperand(&pTos,1);
				if( pKey == 0 ){
					PH7_MemObjInit(&(*pVm),&sNullKey);
					apArg[0] = &sNullKey;
				}else{
					apArg[0] = pKey;
				}
				apArg[1] = pTos;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);
				}
				if( pKey ){
					PH7_MemObjRelease(pKey);
				}else{
					PH7_MemObjRelease(&sNullKey);
				}
				/* Pop the value */
				VmPopOperand(&pTos,1);
				break;
			}
			/* Object without ArrayAccess: PHP throws a fatal Error rather
			 * than silently coercing the object into a hashmap (which is
			 * what the legacy PH7 fall-through would do via MemObjToHashmap
			 * a few lines below). Match PHP. */
			{
				char zMsg[256];
				SyString *pName = &pInst->pClass->sName;
				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
					"Cannot use object of type %.*s as array",
					(int)pName->nByte,pName->zString);
				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);
				if( pKey ){ PH7_MemObjRelease(pKey); }
				VmPopOperand(&pTos,2); /* container + value */
				if( rc == SXERR_ABORT ){ goto Abort; }
				break;
			}
		}
	}
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		/* Hashmap already loaded on stack — COW separate the backing variable.
		 * The stack holds a temporary ref (from LOAD), so undo it before
		 * checking true sharing count, then re-add after separation. */
		if( nIdx != SXU32_HIGH ){
			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){
				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;
				/* Only adjust refcount / perform COW if the backing variable
				 * is still sharing the same hashmap instance. This mirrors
				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting
				 * refcounts if the backing array was already separated. */
				if( pBacking->x.pOther == (void *)pCur ){
					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */
					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);
					pMap->iRef++;  /* Re-add stack ref */
					pTos->x.pOther = pMap;
				}else{
					/* Backing variable no longer points at pCur: skip COW here
					 * and operate on the hashmap currently on the stack. */
					pMap = pCur;
				}
			}else{
				pMap = (ph7_hashmap *)pTos->x.pOther;
			}
		}else{
			pMap = (ph7_hashmap *)pTos->x.pOther;
		}
		if( pMap->iRef < 2 ){
			/* TICKET 1433-48: Prevent garbage collection during insertion.
			 * This inflation is safe with COW: VmPopOperand below will call
			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,
			 * no code checks iRef for COW decisions. */
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
		/* COW separate the backing variable before mutation */
		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);
	}
	VmPopOperand(&pTos,1);
	/* Phase#2: Perform the insertion */
	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){
		/* Insertion by reference */
		rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);
	}else{
		rc = PH7_HashmapInsert(pMap,pKey,pTos);
	}
	if( pKey ){
		PH7_MemObjRelease(pKey);
	}
	/* An append onto the occupied saturated auto-index threw php's catchable
	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other
	 * store-path throw. Plain failures (OOM) keep their existing routes. */
	PH7_DISPATCH_ENFORCE_RC(rc)
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
	/* `++` on a readonly property is forbidden regardless of the current value's
	 * type (it bypasses the store path), so enforce before the type guard below
	 * — which otherwise skips object/array/resource operands. */
	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				if( VmStringWantsPerlIncr(pObj) ){
					/* Perl-style string increment.
					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY
					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming
					 * mutation of pObj doesn't bleed into pTos's old-value view. */
					if( pInstr->iP1 == 0 ){
						SyBlobNullAppend(&pTos->sBlob);
					}
					PH7_MemObjStringIncrement(pObj);
					if( pInstr->iP1 ){
						/* Pre-increment: deep-copy pObj into pTos. */
						PH7_MemObjStore(pObj,pTos);
					}
				}else{
					/* Numeric coercion. Post-increment must preserve pTos's
					 * original value: pTos may alias pObj's blob via
					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and
					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING
					 * pObj. Force pTos to take ownership of its blob first
					 * so its old-value view survives the coercion. */
					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){
						SyBlobNullAppend(&pTos->sBlob);
					}
					/* Force a numeric cast on the variable */
					PH7_MemObjToNumeric(pObj);
					if( pObj->iFlags & MEMOBJ_REAL ){
						pObj->rVal++;
						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it
						 * stays consistent with the new rVal; otherwise (int)$a,
						 * ===, intdiv() etc. read a stale int for an
						 * integer-valued real. */
						PH7_MemObjTryInteger(pObj);
					}else{
						pObj->x.iVal++;
					}
					if( pInstr->iP1 ){
						/* Pre-increment: result is the new value. */
						PH7_MemObjStore(pObj,pTos);
					}
					/* Post-increment: pTos retains the old value (a string
					 * for "5"++, an int/float for direct numeric operands). */
				}
			}
		}else{
			if( pInstr->iP1 ){
				if( VmStringWantsPerlIncr(pTos) ){
					PH7_MemObjStringIncrement(pTos);
				}else{
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
	/* `--` on a readonly property is forbidden regardless of the current value's
	 * type (it bypasses the store path), so enforce before the type guard below
	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly
	 * property currently holding null). */
	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);
	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				if( VmStringWantsPerlIncr(pObj) ){
					/* PHP has no string decrement: `--` on a non-numeric string
					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj
					 * unchanged; the result is simply that unchanged value. */
					if( pInstr->iP1 ){
						/* Pre-decrement: result is the (unchanged) value. */
						PH7_MemObjStore(pObj,pTos);
					}
					/* Post-decrement: pTos already holds the old value. */
				}else{
					/* Numeric coercion. Mirror INCR's aliasing care: a
					 * post-decrement must preserve pTos's original value, which
					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).
					 * Force pTos to own its blob before coercing pObj. */
					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){
						SyBlobNullAppend(&pTos->sBlob);
					}
					PH7_MemObjToNumeric(pObj);
					if( pObj->iFlags & MEMOBJ_REAL ){
						pObj->rVal--;
						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it
						 * stays consistent with the new rVal; otherwise (int)$a,
						 * ===, intdiv() etc. read a stale int for an
						 * integer-valued real. */
						PH7_MemObjTryInteger(pObj);
					}else{
						pObj->x.iVal--;
					}
					if( pInstr->iP1 ){
						/* Pre-decrement: result is the new value. */
						PH7_MemObjStore(pObj,pTos);
					}
					/* Post-decrement: pTos retains the old value. */
				}
			}
		}else{
			if( pInstr->iP1 ){
				if( VmStringWantsPerlIncr(pTos) ){
					/* Non-numeric string, no lvalue: no-op (value unchanged). */
				}else{
					/* Force a numeric cast */
					PH7_MemObjToNumeric(pTos);
					/* Pre-decrement */
					if( pTos->iFlags & MEMOBJ_REAL ){
						pTos->rVal--;
						/* Keep the cached int consistent with the new rVal. */
						PH7_MemObjTryInteger(pTos);
					}else{
						pTos->x.iVal--;
						MemObjSetType(pTos,MEMOBJ_INT);
					}
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
			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
			PH7_MemObjStore(pNos,pObj);
		}
	}
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_POW * * *
 * OP_POW_STORE * * *
 *
 * Pop the top two elements from the stack, raise the second to the
 * power of the first, and push the result. PHP semantics: int**int
 * stays integer iff the exponent is non-negative and the exact result
 * fits in sxi64; otherwise the result is a double.
 */
case PH7_OP_POW:
case PH7_OP_POW_STORE: {
	ph7_value *pNos = &pTos[-1];
	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);
	/* Operand order convention (matches DIV/SUB_STORE):
	 *   POW:       base = pNos (evaluated first),   exp = pTos
	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos
	 */
	ph7_value *pBase = bStore ? pTos : pNos;
	ph7_value *pExp  = bStore ? pNos : pTos;
#ifndef PH7_OMIT_FLOATING_POINT
	int bBothInt;
	int usedInt = 0;
	ph7_real a, b, r;
#endif
	sxi64 base_i = 0, exp_i = 0;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
#ifndef PH7_OMIT_FLOATING_POINT
	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&
	           ((pNos->iFlags & MEMOBJ_REAL) == 0);
	if( bBothInt ){
		base_i = pBase->x.iVal;
		exp_i  = pExp->x.iVal;
	}
	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pBase);
	}
	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pExp);
	}
	a = pBase->rVal;
	b = pExp->rVal;
	r = pow(a, b);
	/* Match PHP: int**non-negative-int stays int when the exact result
	 * fits in sxi64. Use exponentiation by squaring with overflow checks
	 * rather than casting the double back, because the boundary 2^63 is
	 * representable as double but not as signed int64. */
	if( bBothInt && exp_i >= 0 ){
		sxi64 result_i = 1;
		sxi64 cur_base = base_i;
		sxi64 cur_exp  = exp_i;
		int overflow = 0;
		while( cur_exp > 0 ){
			if( cur_exp & 1 ){
				if( VmMulOverflow64(result_i, cur_base, &result_i) ){
					overflow = 1;
					break;
				}
			}
			cur_exp >>= 1;
			if( cur_exp > 0 ){
				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){
					overflow = 1;
					break;
				}
			}
		}
		if( !overflow ){
			pNos->x.iVal = result_i;
			MemObjSetType(pNos, MEMOBJ_INT);
			usedInt = 1;
		}
	}
	if( !usedInt ){
		pNos->rVal = r;
		MemObjSetType(pNos, MEMOBJ_REAL);
	}
#else
	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().
	 * Exponentiation by squaring with silent wrap on overflow, matching
	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.
	 * Negative exponents yield 0 since fractional results cannot be
	 * represented. */
	base_i = pBase->x.iVal;
	exp_i  = pExp->x.iVal;
	{
		sxi64 result_i = 1;
		sxi64 cur_base = base_i;
		sxi64 cur_exp  = exp_i;
		if( cur_exp < 0 ){
			result_i = 0;
		}else{
			while( cur_exp > 0 ){
				if( cur_exp & 1 ){
					result_i *= cur_base;
				}
				cur_exp >>= 1;
				if( cur_exp > 0 ){
					cur_base *= cur_base;
				}
			}
		}
		pNos->x.iVal = result_i;
		MemObjSetType(pNos, MEMOBJ_INT);
	}
#endif /* PH7_OMIT_FLOATING_POINT */
	if( bStore ){
		ph7_value *pObj;
		if( pTos->nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
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
		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);
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
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
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
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
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
		pNos->rVal = 0;
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
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
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
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
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
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
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
			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){
				/* Allocation failure: raise a fatal instead of a truncated concat */
				PH7_VmMemoryError(&(*pVm));
				goto Abort;
			}
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
	sxu32 nIdx;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* The right operand must be a string to append it */
	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){
		PH7_MemObjToString(pNos);
	}
	nIdx = pTos->nIdx;
	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer
	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then
	 * storing the whole buffer back twice. This turns `$s .= ...` (and the
	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).
	 * Guards: a real owned slot; the right operand must NOT alias that same slot
	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under
	 * the source we copy from — references share the slot index, so one check
	 * covers both); and not a typed property, whose store-time type check/coercion
	 * must run before any mutation (left to the slow path).
	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here
	 * and remains O(n^2) by design. */
	if( nIdx != SXU32_HIGH
	 && nIdx != pNos->nIdx
	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0
	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0
	     || SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){
		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){
			/* e.g. $x = 5; $x .= "a";  ->  "5a" */
			PH7_MemObjToString(pObj);
		}
		if( SyBlobLength(&pNos->sBlob) > 0 ){
			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){
				/* Allocation failure: the grow happens before the copy, so pObj
				 * keeps its prior valid contents — raise the fatal uncorrupted. */
				PH7_VmMemoryError(&(*pVm));
				goto Abort;
			}
		}
		/* Produce the expression result. A `.=` result is a temporary, never an
		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a
		 * by-ref param, or `&($s .= "x")`, would alias the live variable).
		 * In the dominant statement form `$s .= "x";` the result is discarded by the
		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)
		 * RHS operand for the POP to drop — keeping the hot path allocation-free.
		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy
		 * of the updated value: a read-only alias into pObj's buffer would dangle if
		 * the same slot is appended to again later in the statement
		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result
		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a
		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */
		if( (pInstr+1)->iOp != PH7_OP_POP ){
			PH7_MemObjStore(pObj,pNos);
		}
		pNos->nIdx = SXU32_HIGH;
		VmPopOperand(&pTos,1);
		break;
	}
	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */
	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){
		/* Force a string cast */
		PH7_MemObjToString(pTos);
	}
	/* Perform the concatenation (Reverse order) */
	if( SyBlobLength(&pNos->sBlob) > 0 ){
		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){
			/* Allocation failure: raise a fatal before committing the store so
			 * no partially-concatenated value is written to the lvalue. */
			PH7_VmMemoryError(&(*pVm));
			goto Abort;
		}
	}
	/* Perform the store operation */
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);
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
/*
 * OP_NULLC: * * *
 * Null coalescing operator '??'.
 * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.
 * Otherwise push right. This is equivalent to: isset($a) ? $a : $b
 */
/*
 * OP_NULLC: * P2 *
 * Short-circuit null coalescing '??'.
 * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).
 * If TOS IS null, pop it and fall through to evaluate the RHS.
 */
case PH7_OP_NULLC: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
		/* Left is not null — keep it and skip the RHS */
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */
	}else{
		/* Left is null — discard it, fall through to evaluate RHS */
		VmPopOperand(&pTos, 1);
	}
	break;
}
/*
 * OP_NULLC_JMP: * P2 *
 * Null coalescing assignment short-circuit.
 * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).
 * If TOS IS null, fall through with TOS retained — it carries the LHS's
 * nIdx so the upcoming NULLC_STORE can write back into the variable slot.
 */
case PH7_OP_NULLC_JMP: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */
	}
	break;
}
/*
 * OP_NULLC_STORE: * * *
 * Null coalescing assignment store.
 * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],
 * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the
 * expression result.
 */
/*
 * OP_NULLSAFE_JMP: * P2 *
 * Nullsafe object operator short-circuit (PHP 8.0 `?->`).
 * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL
 * on the stack as the result of the entire containing postfix chain. If
 * non-null, fall through without modifying the stack so the following
 * PH7_OP_MEMBER can consume the object as usual.
 */
case PH7_OP_NULLSAFE_JMP: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_NULL) || pTos->iFlags == 0 ){
		/* Object operand is NULL (or uninitialized) — short-circuit. The
		 * NULL slot already on TOS becomes the chain's final value. */
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */
	}
	break;
}
case PH7_OP_NULLC_STORE: {
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3
	 * armed pVm with the (object, key) on a missing key. Dispatch to
	 * offsetSet instead of writing through the synthetic pNos->nIdx. */
	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){
		ph7_class_instance *pInst = pVm->pCoalesceObj;
		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,
			"offsetSet",sizeof("offsetSet")-1);
		ph7_value *apArg[2];
		apArg[0] = &pVm->sCoalesceKey;
		apArg[1] = pTos;
		if( pSet ){
			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);
		}
		/* Leave RHS as the expression result (replace pNos with pTos). */
		PH7_MemObjStore(pTos,pNos);
		VmPopOperand(&pTos,1);
		/* Disarm and release the cached instance ref + key. */
		VmCoalesceDisarm(pVm);
		break;
	}
	nIdx = pNos->nIdx;
	if( nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);
		PH7_MemObjStore(pTos,pObj);
	}
	PH7_MemObjStore(pTos,pNos);
	VmPopOperand(&pTos,1);
	break;
}
/*
 * OP_SPREAD: * * *
 * Argument unpacking.  TOS must be an array (hashmap).
 * Replace TOS with the array's individual elements pushed onto the stack.
 * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL
 * can adjust its argument count (the CALL may not be the next instruction).
 */
case PH7_OP_SPREAD: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Traversable argument unpacking f(...$it): materialize the iterator into a
	 * temp array (positional values), then expand it onto the operand stack
	 * like an array. Materialising first leaves the stack untouched until the
	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can
	 * be freed immediately. */
	if( VmValueIsTraversable(pVm,pTos) ){
		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);
		sxi32 rcW;
		sxu32 nEnt;
		if( pTmpMap == 0 ){ goto Abort; }
		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);
		if( rcW == PH7_EXCEPTION || rcW == PH7_ABORT ){
			PH7_HashmapRelease(pTmpMap,TRUE);
			if( rcW == PH7_ABORT ){ goto Abort; }
			goto Exception;
		}
		nEnt = pTmpMap->nEntry;
		if( nEnt == 0 ){
			VmPopOperand(&pTos,1);
			pVm->iSpreadExtra--;
		}else if( pVm->iSpreadExtra + (sxi32)(nEnt - 1) >= VM_STACK_GUARD ){
			VmErrorFormat(&(*pVm), PH7_CTX_ERR,
				"Argument unpacking: cumulative expansion exceeds stack guard (%d)", VM_STACK_GUARD);
		}else{
			ph7_hashmap_node *pNodeT = pTmpMap->pFirst;
			ph7_value *pElemT;
			sxu32 iT;
			pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);
			if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }else{ PH7_MemObjRelease(pTos); }
			pTos->nIdx = SXU32_HIGH;
			pNodeT = pNodeT->pPrev;
			for( iT = 1; iT < nEnt; iT++ ){
				pTos++;
				PH7_MemObjInit(pVm, pTos);
				pTos->nIdx = SXU32_HIGH;
				pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);
				if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }
				pNodeT = pNodeT->pPrev;
			}
			pVm->iSpreadExtra += (sxi32)(nEnt - 1);
		}
		PH7_HashmapRelease(pTmpMap,TRUE);
		break;
	}
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;
		sxu32 nEntry = pMap->nEntry;
		if( nEntry == 0 ){
			/* Empty array — remove from stack */
			VmPopOperand(&pTos, 1);
			pVm->iSpreadExtra--; /* One expression produced zero args */
		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){
			/* Safety: refuse to expand beyond the stack guard margin */
			VmErrorFormat(&(*pVm), PH7_CTX_ERR,
				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",
				VM_STACK_GUARD);
		}else{
			ph7_hashmap_node *pNode2;
			ph7_value *pElem;
			sxu32 i;
			/* Overwrite TOS with first element */
			pNode2 = pMap->pFirst;
			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);
			PH7_MemObjRelease(pTos);
			if( pElem ){
				PH7_MemObjLoad(pElem, pTos);
			}
			pTos->nIdx = SXU32_HIGH;
			/* Traverse in insertion order (pPrev is the forward link
			 * in PHL's circular doubly-linked hashmap node list). */
			pNode2 = pNode2->pPrev;
			/* Push remaining elements */
			for( i = 1; i < nEntry; i++ ){
				pTos++;
				PH7_MemObjInit(pVm, pTos);
				pTos->nIdx = SXU32_HIGH;
				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);
				if( pElem ){
					PH7_MemObjLoad(pElem, pTos);
				}
				pNode2 = pNode2->pPrev;
			}
			pVm->iSpreadExtra += (sxi32)(nEntry - 1);
		}
	}
	/* else: not an array — leave as-is (single arg) */
	break;
}
/*
 * OP_FLAG_SPREAD: * * *
 * Mark the value at TOS as a spread source for the next LOAD_MAP.
 * Used by array literal unpacking '[...$arr]'.
 */
case PH7_OP_FLAG_SPREAD: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	pTos->iFlags |= MEMOBJ_AUX_SPREAD;
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
	if( VmIsUnorderedCmp(pNos,pTos) ){
		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;
	}else if( pInstr->iOp == PH7_OP_EQ ){
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
	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);
	if( VmIsUnorderedCmp(pNos,pTos) ){
		rc = 0;
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
	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);
	if( VmIsUnorderedCmp(pNos,pTos) ){
		rc = 1;
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
	if( VmIsUnorderedCmp(pNos,pTos) ){
		rc = 0;
	}else if( pInstr->iOp == PH7_OP_LE ){
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
	if( VmIsUnorderedCmp(pNos,pTos) ){
		rc = 0;
	}else if( pInstr->iOp == PH7_OP_GE ){
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
/* OP_SPACESHIP * * *
 *
 * Pop the top two elements from the stack. Push an integer result:
 *   -1 if left < right
 *    0 if left == right
 *    1 if left > right
 * Uses loose comparison (type juggling), same as <, >, ==.
 */
case PH7_OP_SPACESHIP: {
	ph7_value *pNos = &pTos[-1];
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	if( VmIsUnorderedCmp(pNos,pTos) ){
		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */
		rc = 1;
	}else{
		/* Normalize to exactly -1, 0, or 1 */
		rc = (rc > 0) - (rc < 0);
	}
	VmPopOperand(&pTos,1);
	PH7_MemObjRelease(pTos);
	pTos->x.iVal = rc;
	MemObjSetType(pTos,MEMOBJ_INT);
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
	 VmFrame *pFrameLocal;
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
			pFrameLocal = pVm->pFrame;
			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
			/* Query the local frame */
			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);
			if( pEntry ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);
			}else{
				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));
				if( pFrameLocal->pParent == 0 ){
					/* Insert in the $GLOBALS array */
					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);
				}
				if( rc == SXRET_OK ){
					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);
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
	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own
	 * mutable state per entry — see VmExcActivate), never the shared
	 * compiled object. */
	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);
	VmFrame *pFrameLocal;
	if( pException == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		goto Abort;
	}
	/* Create the exception frame BEFORE publishing the activation, so an OOM
	 * abort cannot orphan a pushed entry with no frame behind it. */
	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);
	if( rc != SXRET_OK ){
		VmExcRelease(&(*pVm),pException);
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		goto Abort;
	}
	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){
		VmExcRelease(&(*pVm),pException);
		VmLeaveFrame(&(*pVm));
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		goto Abort;
	}
	/* Mark the special frame */
	pFrameLocal->iFlags |= VM_FRAME_EXCEPTION;
	pFrameLocal->iExceptionJump = pInstr->iP2;
	/* Record the landing pad on the exception too, so an in-place catch can resume
	 * the throwing site at THIS try (survives the exception frame's teardown), plus
	 * the bytecode array it indexes — the resume only fires in the exec running that
	 * array, so a mini-program (inline try in a catch/finally) and the body that
	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the
	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */
	pException->iLandingPc = pFrameLocal->iExceptionJump;
	pException->pOwnerInstr = (void *)aInstr;
	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try
	 * landing pad is reached with the stack back at this depth; Generator::throw()
	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the
	 * abandoned expression's operands above this base). Normal throws are already here. */
	pException->iStackDepth = (sxi32)(pTos - pStack);
	/* Point to the frame that trigger the exception */
	pFrameLocal = pFrameLocal->pParent;
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
	pException->pFrame = pFrameLocal;
	break;
							}
/*
 * OP_POP_EXCEPTION * * P3
 * Pop a previously pushed exception from the corresponding container.
 */
case PH7_OP_POP_EXCEPTION: {
	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;
	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on
	 * top (matched by compiled origin). pException == NULL means this try's
	 * activation was already consumed (an in-place catch handled a throw and
	 * ran the finally itself); compiled fields keep coming from p3. */
	ph7_exception *pException = 0;
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);
		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];
		/* Same compiled origin is NOT enough: under recursion, when THIS
		 * level's activation was already consumed by an in-place catch (the
		 * resume lands right here), the top can be an OUTER level's activation
		 * of the same lexical try — popping it would run that level's finally
		 * early and orphan its handler (probed: recursive try/catch/finally
		 * lost the outer catch entirely). The activation must also belong to
		 * the CURRENT body frame. */
		if( VmExcMatches(pTop,pCompiledExc)
		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){
			pException = pTop;
			(void)SySetPop(&pVm->aException);
		}
	}
	if( pCompiledExc->iInlined ){
		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.
		 * Pop this try's handler off aException so a throw in the finally propagates to
		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the
		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the
		 * compiler-emitted JMP falls into the finally. Without a finally, this is the
		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */
		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */
		if( pCompiledExc->iHasFinally ){
			VmFinallyAction sAct;
			SyZero(&sAct,sizeof(sAct));
			sAct.eKind = PH7_FA_FALLTHROUGH;
			sAct.iNextPc = pCompiledExc->iEndCatchPc;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			/* keep the transparent frame for OP_END_FINALLY */
		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
			VmLeaveFrame(&(*pVm));
		}
		break;
	}
	/* Leave the exception frame. It is normally on top here (a try that fell through
	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a
	 * RESUMED generator/fiber body whose yield was inside this try, that exception
	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),
	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the
	 * coroutine (and defeat the bHasRet materialization just below, which must see the
	 * body). Only leave a genuine exception frame. */
	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
		VmLeaveFrame(&(*pVm));
	}
	/* Execute the finally block if present and not already executed by the
	 * catch path. No live activation (pException == NULL) means an in-place
	 * catch consumed it — and that path runs the finally itself — so skip. */
	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){
		sxi32 rcFinally;
		VmExcRelease(&(*pVm),pException);
		pException = 0;
		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);
		if( rcFinally == SXERR_ABORT ){
			goto Abort;
		}
		if( rcFinally == PH7_EXCEPTION ){
			/* The finally threw past itself. If an enclosing try IN THIS function
			 * caught the new exception in place, resume at its landing pad;
			 * otherwise it was caught at an outer frame, so unwind this function. */
			sxi32 iResumePc;
			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
				pc = iResumePc;
				break;
			}
			goto Exception;
		}
	}
	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */
	if( VmSkipExceptionFrames(pVm->pFrame)->bHasRet ){
		/* `return` inside the finally (normal try completion) returns from the
		 * function. The return targets the body frame this try belongs to. Drain
		 * outer finally blocks first, then — only in the real function body
		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline
		 * try within a catch/finally) propagate outward so the owning body returns. */
		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		if( rc == PH7_EXCEPTION ){
			goto Exception;
		}
		if( !sState.bReturnPropagates ){
			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);
		}
		goto Done;
	}
	break;
							}
/*
 * OP_CATCH iP1(catch-index) * P3(ph7_exception)
 * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on
 * pException->pInflight by VmThrowInline) into the catch variable, resolved in the
 * enclosing body's scope (PHP: a catch shares the surrounding variable scope).
 */
case PH7_OP_CATCH: {
	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation
	 * of this try, not the compiled p3 (which VmThrowInline kept on aException
	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */
	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;
	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);
	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);
	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;
	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);
	pBody->iFlags &= ~VM_FRAME_THROW;
	if( pCatch && pBind ){
		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);
		if( pObj ){
			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,
			 * free the slot's prior contents, then rebind. */
			pBind->iRef++;
			PH7_MemObjRelease(pObj);
			pObj->x.pOther = pBind;
			MemObjSetType(pObj,MEMOBJ_OBJ);
		}
	}
	if( pBind ){
		/* Drop the hold VmThrowInline took across the redirect. */
		PH7_ClassInstanceUnref(pBind);
	}
	if( pExc ){
		pExc->pInflight = 0;
	}
	break;
						 }
/*
 * OP_END_FINALLY * P3(ph7_exception)
 * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch
 * the pending action queued when the finally was entered (fall-through / re-throw /
 * return / break-continue). A return/break threads out through each enclosing finally
 * via pException->iNextFinallyPc.
 */
case PH7_OP_END_FINALLY: {
	ph7_exception *pExc = (ph7_exception *)pInstr->p3;
	VmFinallyAction sAct;
	int eKind = PH7_FA_FALLTHROUGH;
	/* Leave the try's transparent frame kept alive across the finally. */
	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
		VmLeaveFrame(&(*pVm));
	}
	if( SySetUsed(&pVm->aFinallyAction) > 0 ){
		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);
		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];
		(void)SySetPop(&pVm->aFinallyAction);
		eKind = sAct.eKind;
	}else{
		SyZero(&sAct,sizeof(sAct));
		sAct.iNextPc = pExc->iEndCatchPc;
	}
	if( eKind == PH7_FA_FALLTHROUGH ){
		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;
		break;
	}else if( eKind == PH7_FA_JMP ){
		/* break/continue: run the remaining crossed finallys, then take the jump. */
		sxu32 iFpc = 0;
		int nCross = sAct.nCross;
		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
			sAct.nCross = nCross;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			pc = (sxi32)iFpc - 1;
			break;
		}
		pc = (sxi32)sAct.iNextPc - 1;
		break;
	}else if( eKind == PH7_FA_RETHROW ){
		ph7_class_instance *pRe = sAct.pExc;
		sxi32 _iRpE;
		rc = VmThrowException(&(*pVm),pRe);
		if( pRe ){ PH7_ClassInstanceUnref(pRe); }
		if( rc == SXERR_ABORT ){ goto Abort; }
		PH7_INLINE_RESUME_BREAK()
		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }
		goto Exception;
	}else{ /* PH7_FA_RETURN */
		sxu32 iFpc = 0;
		int nCross = sAct.nCross;
		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
			/* Thread the return through the next enclosing finally. */
			sAct.nCross = nCross;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			pc = (sxi32)iFpc - 1;
			break;
		}
		/* No enclosing finally left: materialize the return from this body. */
		if( sAct.bHasRetVal && sState.pResult ){
			PH7_MemObjStore(&sAct.sRet,sState.pResult);
		}
		PH7_MemObjRelease(&sAct.sRet);
		goto Done;
	}
						 }
/*
 * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)
 * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a
 * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);
 * OP_END_FINALLY threads it out through the finally chain and then returns.
 */
case PH7_OP_SET_FINALLY_RET: {
	VmFinallyAction sAct;
	sxu32 iFpc = 0;
	int nCross = -1; /* a return crosses every enclosing finally in this function */
	SyZero(&sAct,sizeof(sAct));
	sAct.eKind = PH7_FA_RETURN;
	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);
	PH7_MemObjInit(pVm,&sAct.sRet);
	if( pInstr->iP1 && pTos >= pStack ){
		PH7_MemObjStore(pTos,&sAct.sRet);
		sAct.bHasRetVal = 1;
		VmPopOperand(&pTos,1);
	}
	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
		sAct.nCross = nCross;
		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
		pc = (sxi32)iFpc - 1;
		break;
	}
	/* No enclosing finally left: return now. */
	if( sAct.bHasRetVal && sState.pResult ){
		PH7_MemObjStore(&sAct.sRet,sState.pResult);
	}
	PH7_MemObjRelease(&sAct.sRet);
	goto Done;
						 }
/*
 * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)
 * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action
 * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.
 */
case PH7_OP_SET_FINALLY_JMP: {
	VmFinallyAction sAct;
	sxu32 iFpc = 0;
	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */
	SyZero(&sAct,sizeof(sAct));
	sAct.eKind = PH7_FA_JMP;
	sAct.iNextPc = pInstr->iP2;
	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
		sAct.nCross = nCross;
		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
		pc = (sxi32)iFpc - 1;
		break;
	}
	/* No finally among the crossed trys: just take the break/continue jump. */
	pc = (sxi32)sAct.iNextPc - 1;
	break;
						 }
/*
 * OP_THROW * P2 *
 * Throw an user exception.
 */
case PH7_OP_THROW: {
	VmFrame *pFrameLocal = pVm->pFrame;
	sxu32 nJump = pInstr->iP2;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
	/* Tell the upper layer that an exception was thrown */
	pFrameLocal->iFlags |= VM_FRAME_THROW;
	if( pTos->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
		ph7_class *pThrowable;
		/* Thrown object must implement the Throwable interface (PHP 7+). */
		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);
		if( pThrowable == 0 || !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){
			/* Not a Throwable: replace with Error(msg) matching PHP behavior.
			 * Error::__construct is defined in the built-in library and
			 * cannot realistically fail, so we do not check its return. */
			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);
			ph7_class_instance *pErrInst = 0;
			if( pErrorClass ){
				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);
			}
			if( pErrInst ){
				ph7_class_method *pCons;
				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);
				if( pCons ){
					ph7_value sArg;
					ph7_value *apArg[1];
					SyString sMsgStr;
					static const char zErrMsg[] =
						"Cannot throw objects that do not implement Throwable";
					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);
					PH7_MemObjInit(pVm,&sArg);
					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
					apArg[0] = &sArg;
					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);
					PH7_MemObjRelease(&sArg);
				}
				rc = VmThrowException(&(*pVm),pErrInst);
				PH7_ClassInstanceUnref(pErrInst);
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
			}else{
				/* Bootstrap failure — fall back to uncaught reporting */
				rc = VmUncaughtException(&(*pVm),pThis);
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
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
	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally
	 * (draining any mid-expression operands back to the try's base). */
	PH7_INLINE_RESUME_BREAK()
	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a
	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */
	if( rc == PH7_EXCEPTION || pVm->pResumeFrame || pVm->pInlineInstr ){
		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own
		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch
		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the
		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else
		 * unwind so the owning exec lands. Without this a throw caught at an enclosing
		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run
		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw
		 * (face c). */
		sxi32 iResumePc;
		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
			pc = iResumePc;
			break;
		}
		goto Exception;
	}
	/* No in-place catch recorded: this throw's own enclosing try caught it (the
	 * common case; its landing pad is exactly nJump). Perform an unconditional jump
	 * to the try's OP_POP_EXCEPTION landing pad, which tears down the try frame, runs
	 * finally, and (when a catch/finally issued a `return`) materializes the body
	 * frame's pending return. Routing the return through OP_POP_EXCEPTION keeps the
	 * frame stack balanced. */
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
				ph7_hashmap *pMap;
				/* COW: For by-reference foreach, eagerly separate the
				 * source array so mutations don't affect other sharers. */
				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){
					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);
					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){
						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;
						/* Only adjust refcounts/separate if the backing
						 * variable still points at the same hashmap as
						 * the stack value. */
						if( pBacking->x.pOther == (void *)pCur ){
							pCur->iRef--;
							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup
							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave
							 * pBacking dangling. The return value is the post-separation map. */
							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);
							((ph7_hashmap *)pTos->x.pOther)->iRef++;
						}
					}
				}
				pMap = (ph7_hashmap *)pTos->x.pOther;
				/* Reset the internal loop cursor */
				PH7_HashmapResetLoopCursor(pMap);
				/* Mark the step */
				pStep->iFlags |= PH7_4EACH_STEP_HASHMAP;
				pStep->xIter.pMap = pMap;
				pMap->iRef++;
			}else{
				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
				ph7_class *pIteratorClass;
				/* Check if the object implements Iterator */
				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);
				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){
					/* Iterator-based iteration: call rewind() */
					ph7_class_method *pRewind;
					pStep->iFlags |= PH7_4EACH_STEP_ITERATOR|PH7_4EACH_STEP_FIRST;
					pStep->xIter.pThis = pThis;
					pThis->iRef++;
					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);
					if( pRewind ){
						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);
						if( VmIterCallThrew(rc) ){
							/* rewind() threw (a generator body or userland Iterator):
							 * undo this step's retain, drop the step, and route the
							 * exception instead of silently starting the loop. */
							pThis->iRef--;
							SyMemBackendPoolFree(&pVm->sAllocator,pStep);
							pStep = 0;
							PH7_DISPATCH_ITER_RC(rc,1)
						}
					}
				}else{
					/* Check if the object implements IteratorAggregate */
					ph7_class *pIterAggClass;
					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",
						sizeof("IteratorAggregate")-1,FALSE,0);
					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){
						/* Call getIterator() and use the returned Iterator object */
						ph7_class_method *pGetIter;
						int iterAggOk = 0;
						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);
						if( pGetIter ){
							ph7_value sResult;
							PH7_MemObjInit(&(*pVm),&sResult);
							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);
							if( VmIterCallThrew(rc) ){
								/* getIterator() threw: drop the step and route the
								 * exception (don't pile the "must implement Iterator"
								 * error on top of it). */
								PH7_MemObjRelease(&sResult);
								SyMemBackendPoolFree(&pVm->sAllocator,pStep);
								pStep = 0;
								PH7_DISPATCH_ITER_RC(rc,1)
							}
							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){
								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;
								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){
									ph7_class_method *pRewind;
									pStep->iFlags |= PH7_4EACH_STEP_ITERATOR|PH7_4EACH_STEP_FIRST;
									pStep->xIter.pThis = pIterObj;
									pIterObj->iRef++;
									/* Retain the aggregate so it lives for the duration of the foreach */
									pStep->pOwner = pThis;
									pThis->iRef++;
									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);
									if( pRewind ){
										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);
										if( VmIterCallThrew(rc) ){
											/* The aggregate's iterator rewind() threw: undo
											 * both retains, drop the step, route the exception. */
											pIterObj->iRef--;
											pThis->iRef--;
											PH7_MemObjRelease(&sResult);
											SyMemBackendPoolFree(&pVm->sAllocator,pStep);
											pStep = 0;
											PH7_DISPATCH_ITER_RC(rc,1)
										}
									}
									iterAggOk = 1;
								}
							}
							PH7_MemObjRelease(&sResult);
						}
						if( !iterAggOk ){
							/* getIterator() failed or returned non-Iterator: abort this foreach */
							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
								"Object returned by getIterator() must implement Iterator");
							SyMemBackendPoolFree(&pVm->sAllocator,pStep);
							pStep = 0; /* Signal: do not store this step */
							pc = pInstr->iP2 - 1;
						}
					}else{
						/* Plain object iteration via hAttr */
						SyHashResetLoopCursor(&pThis->hAttr);
						pStep->iFlags |= PH7_4EACH_STEP_OBJECT;
						pStep->xIter.pThis = pThis;
						pThis->iRef++;
					}
				}
			}
		}
		if( pStep ){
			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){
				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");
				SyMemBackendPoolFree(&pVm->sAllocator,pStep);
				/* Jump out of the loop */
				pc = pInstr->iP2 - 1;
			}
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
	VmFrame *pFrameLocal;
	/* Peek the last step */
	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];
	pFrameLocal = pVm->pFrame;
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
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
				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
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
				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));
				if( pEntry ){
					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);
				}else{
					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),
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
	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){
		/* Iterator-based iteration.
		 * Sequence: on first call just check valid/current/key.
		 * On subsequent calls, advance with next() first, then check.
		 */
		ph7_class_instance *pThis = pStep->xIter.pThis;
		ph7_class_method *pMethod;
		ph7_value sResult;
		int isValid = 0;
		/* Call next() to advance — but skip on the first iteration */
		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){
			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;
		}else{
			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);
			if( pMethod ){
				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);
				if( VmIterCallThrew(rc) ){
					/* next() threw (generator body / userland Iterator): tear the
					 * step down like exhaustion does, then route the exception —
					 * the loop must not silently end with execution continuing. */
					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
					PH7_DISPATCH_ITER_RC(rc,0)
				}
			}
		}
		/* Call valid() */
		PH7_MemObjInit(pVm,&sResult);
		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);
		if( pMethod ){
			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);
			if( VmIterCallThrew(rc) ){
				/* valid() threw: same teardown-and-route as next() above. */
				PH7_MemObjRelease(&sResult);
				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
				PH7_DISPATCH_ITER_RC(rc,0)
			}
			PH7_MemObjToBool(&sResult);
			isValid = (sResult.x.iVal != 0);
		}
		PH7_MemObjRelease(&sResult);
		if( !isValid ){
			/* Iterator exhausted */
			pc = pInstr->iP2 - 1;
			/* Release the aggregate owner if this was an IteratorAggregate foreach */
			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
		}else{
			/* Call current() to get value */
			PH7_MemObjInit(pVm,&sResult);
			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);
			if( pMethod ){
				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);
				if( VmIterCallThrew(rc) ){
					/* current() threw: same teardown-and-route as next() above. */
					PH7_MemObjRelease(&sResult);
					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
					PH7_DISPATCH_ITER_RC(rc,0)
				}
			}
			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
			if( pValue ){
				PH7_MemObjStore(&sResult,pValue);
			}
			PH7_MemObjRelease(&sResult);
			/* Call key() if needed */
			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){
				ph7_value sKey;
				PH7_MemObjInit(pVm,&sKey);
				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);
				if( pMethod ){
					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);
					if( VmIterCallThrew(rc) ){
						/* key() threw: same teardown-and-route as next() above. */
						PH7_MemObjRelease(&sKey);
						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
						PH7_DISPATCH_ITER_RC(rc,0)
					}
				}
				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);
				if( pValue ){
					PH7_MemObjStore(&sKey,pValue);
				}
				PH7_MemObjRelease(&sKey);
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
			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,
				pVmAttr->pAttr->iProtection,FALSE) ){
					break; /* Access is granted */
			}
		}
		if( pEntry == 0 ){
			/* Clean up the mess left behind */
			pc = pInstr->iP2 - 1; /* Jump to this destination */
			if( pStep->iFlags & PH7_4EACH_STEP_REF ){
				/* Break the reference with the last element */
				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
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
					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));
					if( pEntry ){
						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);
					}else{
						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),
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
			if( pInstr->iP2 == PH7_MEMBER_METHOD ){
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
				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */
				VmClassAttr *pObjAttr = 0;
				SyHashEntry *pEntry = 0;
				/* Extract the target attribute */
				if( sName.nByte > 0 ){
					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);
					if( pEntry ){
						/* Point to the attribute value */
						pObjAttr = (VmClassAttr *)pEntry->pUserData;
					}
				}
				if( pInstr->iP2 == PH7_MEMBER_UNSET ){
					/* unset($o->prop): remove the property entirely so it disappears from
					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only
					 * release would leave a zombie null entry). Leave a NULL constant on the stack so
					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5). */
					if( pEntry ){
						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);
						SyHashDeleteEntry2(pEntry);
					}
					VmPopOperand(&pTos,1);    /* pop the attribute name */
					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */
					pTos->nIdx = SXU32_HIGH;  /* NULL constant */
					break;
				}
				if( pObjAttr == 0 && sName.nByte > 0 ){
					/* Member not present on the instance and the next instruction writes/modifies it
					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see
					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so
					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot
					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):
					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it
					 *     (PHP re-appends it at the end), OR
					 *   - a dynamic prop on a dynamic-allowing class (stdClass).
					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies
					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.
					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is
					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next
					 * instruction there), and for ++/--/compound-assign/store the next opcode is the
					 * modify-op directly (VmMemberNextIsWrite). */
					VmInstr *pNext = pInstr + 1;
					if( pInstr->iP2 == PH7_MEMBER_WRITE || VmMemberNextIsWrite(pNext) ){
						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);
						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);
						}else if( VmClassAllowsDynamicProps(pVm,pThis->pClass) ){
							PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);
						}
					}
				}
				if( pObjAttr == 0 ){
					/* No such attribute,load null. In isset()/empty() context (iP2 3/4) PHP returns
					 * false/true SILENTLY, so suppress the read-miss warning there (mirrors the array
					 * LOAD_IDX iP2=4/6 suppression). */
					if( !VmMemberCtxIsLookup(pInstr->iP2) ){
						VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",
							&pClass->sName,&sName);
					}
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
					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){
						/* PHP 7.4+: reading an uninitialized typed property is an Error.
						 * We can only raise it on a real read, not when the slot is the
						 * LHS of an assignment — peek at the next instruction to decide.
						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so
						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */
						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)
						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){
							VmInstr *pNext = pInstr + 1;
							int bIsLhs = 0;
							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){
								bIsLhs = 1;
							}
							if( !bIsLhs ){
								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);
								PH7_ClassInstanceUnref(pThis);
								if( rcU == PH7_ABORT ){
									goto Abort;
								}
								{
									sxi32 iRp;
									if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
										pc = iRp;
										break;
									}
								}
								goto Exception;
							}
						}
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
					}else{
						/* Throw Error exception (PHP-compatible).
						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */
						char zMsg[256];
						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",
							zVis,(int)pClass->sName.nByte,pClass->sName.zString,
							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);
						PH7_ClassInstanceUnref(pThis);
						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);
						goto Abort;
					}
				}
				/* Safely unreference the object */
				PH7_ClassInstanceUnref(pThis);
			}
		}else{
			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()
			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */
			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");
			}
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
					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);
					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);
					/* Handle self/static/parent keywords */
					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){
						pClass = PH7_VmPeekDeclaringClass(&(*pVm));
						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){
							/* In a trait method, self:: resolves to the using class */
							pClass = PH7_VmPeekTopClass(&(*pVm));
						}
					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
						pClass = PH7_VmPeekTopClass(&(*pVm));
					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));
						if( pSelf && pSelf->pBase ){
							pClass = pSelf->pBase;
						}
					}else{
						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
					}
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
				if( pInstr->iP2 == PH7_MEMBER_METHOD ){
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
					if( pInstr->iP2 == PH7_MEMBER_UNSET ){
						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.
						 * Without this the iP2=unset tag falls through to a normal static read and the
						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */
						char zMsg[256];
						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",
							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);
						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);
						goto Abort;
					}
					/* Check for special ::class pseudo-constant */
					if( sName.nByte == sizeof("class")-1 &&
					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){
						/* ::class returns the fully qualified class name */
						/* Pop the attribute name from the stack */
						if( !pInstr->p3 ){
							VmPopOperand(&pTos,1);
						}
						PH7_MemObjRelease(pTos);
						/* Load the class name */
						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);
						pTos->nIdx = SXU32_HIGH;
					}else{
						/* Extract the target attribute */
						if( sName.nByte > 0 ){
							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);
						}
						if( pAttr == 0 ){
							/* No such attribute,load null. isset()/empty() context is silent. */
							if( !VmMemberCtxIsLookup(pInstr->iP2) ){
								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",
									&pClass->sName,&sName);
							}
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
								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
									/* PHP 7.4+: uninitialized typed static read.
									 * Same LHS-of-store peek as the instance path. */
									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0
									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,
											(const void *)&pAttr->nIdx,sizeof(sxu32));
										if( pS ){
											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;
											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){
												VmInstr *pNext = pInstr + 1;
												int bIsLhs = 0;
												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){
													bIsLhs = 1;
												}
												if( !bIsLhs ){
													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);
													if( pThis ){
														PH7_ClassInstanceUnref(pThis);
													}
													if( rcU == PH7_ABORT ){
														goto Abort;
													}
													{
														sxi32 iRp;
														if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
															pc = iRp;
															break;
														}
													}
													goto Exception;
												}
											}
										}
									}
									/* Load the desired attribute */
									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);
									if( pValue ){
										PH7_MemObjLoad(pValue,pTos);
										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){
											/* Load index number */
											pTos->nIdx = pAttr->nIdx;
										}
									}
								}else{
									/* Throw Error exception (PHP-compatible) */
									char zMsg[256];
									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){
										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",
											zVis,(int)pClass->sName.nByte,pClass->sName.zString,
											(int)pAttr->sName.nByte,pAttr->sName.zString);
									}else{
										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",
											zVis,(int)pClass->sName.nByte,pClass->sName.zString,
											(int)pAttr->sName.nByte,pAttr->sName.zString);
									}
									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);
									goto Abort;
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
		/* No such class — fatal error, stop execution (matches PHP behavior) */
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",
			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)
			);
		/* Release the class operand and any constructor arguments, then abort */
		PH7_MemObjRelease(pTos);
		if( pInstr->iP1 > 0 ){
			/* Pop given arguments */
			VmPopOperand(&pTos,pInstr->iP1);
		}
		goto Abort;
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
			/* Call the class constructor.  Collect args in stack order and
			 * forward any VmCallArgMap from the NEW instruction so the
			 * receiving OP_CALL path runs its named-argument matching
			 * (including variadic string-key packing). */
			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;
			sxi32 rcCons;
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){
				ph7_vm_func_arg *pFuncArg;
				sxu32 n;
				n = SySetUsed(&aArg);
				/* Emit a notice for missing arguments (positional-only:
				 * for named args the missing-arg check happens downstream
				 * after resolution). */
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
			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);
			/* TICKET 1433-52: Unsetting $this in the constructor body */
			if( pNew->iRef < 1 ){
				pNew->iRef = 1;
			}
			if( rcCons == PH7_ABORT || rcCons == PH7_EXCEPTION ){
				/* The constructor raised: the half-constructed object must not
				 * become the NEW result. Drop our reference so it is destroyed.
				 * The class-name operand (and any leftover args) are released by
				 * the Abort/Exception unwind, or explicitly on the resume path. */
				sxi32 iResumePc;
				PH7_ClassInstanceUnref(pNew);
				if( rcCons == PH7_ABORT ){
					goto Abort;
				}
				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
					/* This frame's own try caught it in-place: tidy the stack
					 * (pop ctor args + release the class-name slot) and resume. */
					if( pInstr->iP1 > 0 ){
						VmPopOperand(&pTos,pInstr->iP1);
					}
					PH7_MemObjRelease(pTos);
					pc = iResumePc;
					break;
				}
				goto Exception;
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
	/* Generator and Fiber objects are not cloneable (matches PHP) */
	if( pSrc->pClass == pVm->pGeneratorClass || pSrc->pClass == pVm->pFiberClass ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Trying to clone an uncloneable object of class '%z'",
			&pSrc->pClass->sName);
		PH7_MemObjRelease(pTos);
		break;
	}
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
		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);
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
 * OP_MATCH * * P3
 *  PHP 8.0 match expression. P3 points to a ph7_match struct holding
 *  the compiled arms. On entry, the subject is on top of the stack.
 *  On exit, the stack slot holds the matched arm's result value.
 *  Comparison is strict (===). No fallthrough. When no arm matches and
 *  no default is present, a fatal UnhandledMatchError is raised.
 */
case PH7_OP_MATCH: {
	ph7_match *pMatch = (ph7_match *)pInstr->p3;
	ph7_match_arm *aArm,*pArm,*pDefault = 0;
	ph7_value sSubject,sCond,sResult;
	sxu32 i,j,nArm,nCond;
	int matched = 0;
#ifdef UNTRUST
	if( pMatch == 0 || pTos < pStack ){
		goto Abort;
	}
#endif
	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);
	nArm = SySetUsed(&pMatch->aArms);
	PH7_MemObjInit(pVm,&sSubject);
	PH7_MemObjInit(pVm,&sCond);
	PH7_MemObjInit(pVm,&sResult);
	PH7_MemObjLoad(pTos,&sSubject);
	for( i = 0; i < nArm && !matched; ++i ){
		pArm = &aArm[i];
		if( pArm->bDefault ){
			pDefault = pArm;
			continue;
		}
		nCond = SySetUsed(&pArm->aConds);
		for( j = 0; j < nCond; ++j ){
			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);
			if( pCondBc == 0 ){
				continue;
			}
			VmLocalExec(pVm,pCondBc,&sCond,FALSE);
			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);
			PH7_MemObjRelease(&sCond);
			if( rc == 0 ){
				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);
				matched = 1;
				break;
			}
		}
	}
	if( !matched && pDefault ){
		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);
		matched = 1;
	}
	if( !matched ){
		const char *zType = "unknown";
		char zMsg[128];
		sxu32 nMsg;
		switch(sSubject.iFlags & MEMOBJ_ALL){
		case MEMOBJ_NULL:   zType = "null";   break;
		case MEMOBJ_BOOL:   zType = "bool";   break;
		case MEMOBJ_INT:    zType = "int";    break;
		case MEMOBJ_REAL:   zType = "float";  break;
		case MEMOBJ_STRING: zType = "string"; break;
		case MEMOBJ_HASHMAP:zType = "array";  break;
		case MEMOBJ_OBJ:    zType = "object"; break;
		case MEMOBJ_RES:    zType = "resource"; break;
		default: break;
		}
		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
			"Unhandled match case of type %s",zType);
		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",
			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);
		PH7_MemObjRelease(&sSubject);
		PH7_MemObjRelease(&sResult);
		goto Abort;
	}
	PH7_MemObjRelease(&sSubject);
	/* Replace subject on TOS with the arm result */
	PH7_MemObjStore(&sResult,pTos);
	PH7_MemObjRelease(&sResult);
	break;
					}
/*
 * OP_YIELD P1 P2 *
 *  Yield a value from a generator function.
 *  P1=1 if value on stack, P1=0 for bare yield.
 *  P2=1 if key=>value syntax (key below value on stack).
 */
case PH7_OP_YIELD: {
	ph7_generator *pGen;
	if( pVm->pActiveCtx == 0 || pVm->pActiveCtx->pPrivate == 0 ){
		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");
		goto Abort;
	}
	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;
	if( pInstr->iP2 ){
		/* yield $key => $value: value on top, key below */
#ifdef UNTRUST
		if( pTos < &pStack[1] ) goto Abort;
#endif
		PH7_MemObjStore(pTos, &pGen->sYieldValue);
		VmPopOperand(&pTos, 1);
		PH7_MemObjStore(pTos, &pGen->sYieldKey);
		VmPopOperand(&pTos, 1);
		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */
		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){
			sxi64 nKey = pGen->sYieldKey.x.iVal;
			if( nKey >= pGen->iImplicitKey ){
				pGen->iImplicitKey = nKey + 1;
			}
		}
	}else if( pInstr->iP1 ){
		/* yield $value */
#ifdef UNTRUST
		if( pTos < pStack ) goto Abort;
#endif
		PH7_MemObjStore(pTos, &pGen->sYieldValue);
		VmPopOperand(&pTos, 1);
		/* Auto-increment key */
		PH7_MemObjRelease(&pGen->sYieldKey);
		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;
		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);
	}else{
		/* Bare yield — null value, auto-increment key */
		PH7_MemObjRelease(&pGen->sYieldValue);
		PH7_MemObjRelease(&pGen->sYieldKey);
		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;
		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);
	}
	/* Suspend execution — resume will push the send() value as the yield result */
	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));
	goto Suspend;
}
/*
 * OP_YIELD_FROM * * *
 *
 * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an
 * array/Traversable/Generator from the OUTER generator, preserving the inner
 * keys; the expression evaluates to the inner Generator's return value (or NULL).
 *
 * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,
 * advances the per-instance delegate cursor stored on the exec context (never the
 * shared foreach aStep, so independent generator instances cannot clash). The
 * iterable operand is consumed on first entry; the expression result is pushed at
 * exhaustion — net stack effect +1, identical to OP_YIELD.
 */
case PH7_OP_YIELD_FROM: {
	ph7_generator *pGenFrom;
	ph7_exec_ctx *pCtxFrom;
	ph7_value sKey,sVal;
	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */
	int bExhausted = 0;
	if( pVm->pActiveCtx == 0 || pVm->pActiveCtx->pPrivate == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");
		goto Abort;
	}
	pCtxFrom = pVm->pActiveCtx;
	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjInit(pVm,&sVal);
	if( pCtxFrom->iDelegateState == 0 ){
		/* First entry: classify the iterable on the stack top. */
		int bIterable = 1;
#ifdef UNTRUST
		if( pTos < pStack ){ goto Abort; }
#endif
		if( pTos->iFlags & MEMOBJ_HASHMAP ){
			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);
			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;
			pCtxFrom->iDelegateState = 1;
		}else if( pTos->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);
			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){
				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);
				pCtxFrom->iDelegateState = 3;
			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){
				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);
				pCtxFrom->iDelegateState = 2;
			}else{
				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",
					sizeof("IteratorAggregate")-1,FALSE,0);
				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){
					/* Delegate to the Iterator returned by getIterator() */
					ph7_value sIt;
					PH7_MemObjInit(pVm,&sIt);
					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);
					if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){
						/* getIterator() threw/aborted: drop it, consume the
						 * operand, and propagate. */
						PH7_MemObjRelease(&sIt);
						VmPopOperand(&pTos,1);
						goto yf_propagate;
					}
					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls
						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){
						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);
						pCtxFrom->iDelegateState = 2;
					}else{
						bIterable = 0;
					}
					PH7_MemObjRelease(&sIt);
				}else{
					bIterable = 0;
				}
			}
		}else{
			bIterable = 0;
		}
		VmPopOperand(&pTos,1); /* Consume the iterable operand */
		if( !bIterable ){
			/* Non-iterable source: throw a catchable Error (PHP 8.5), then
			 * funnel through the shared teardown/route path. */
			rc = VmThrowFromVm(&(*pVm),"Error",
				"Can use \"yield from\" only with arrays and Traversables",
				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);
			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
			goto yf_propagate;
		}
		if( pCtxFrom->iDelegateState >= 2 ){
			/* rewind() the delegate (also starts a fresh generator) */
			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,
				"rewind",sizeof("rewind")-1,0);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}
	}else{
		/* Resume entry: discard the value VmResumeCtx pushed. Forwarding send()
		 * into the delegated generator is deferred (see the Generator::throw TODO);
		 * arrays/Traversables ignore send() anyway. */
#ifdef UNTRUST
		if( pTos < pStack ){ goto Abort; }
#endif
		PH7_MemObjRelease(pTos);
		pTos--;
		if( pCtxFrom->iDelegateState >= 2 ){
			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,
				"next",sizeof("next")-1,0);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}
	}
	/* Fetch the current (key,value) of the delegate, or mark exhausted. */
	if( pCtxFrom->iDelegateState == 1 ){
		if( pCtxFrom->pDelegateNode == 0 ){
			bExhausted = 1;
		}else{
			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);
			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);
			/* Forward traversal follows pPrev (the hashmap's "reverse link",
			 * matching PH7_HashmapGetNextEntry). */
			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;
		}
	}else{
		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;
		ph7_value sValid;
		int isValid;
		PH7_MemObjInit(pVm,&sValid);
		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);
		PH7_MemObjToBool(&sValid);
		isValid = (sValid.x.iVal != 0);
		PH7_MemObjRelease(&sValid);
		if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		if( !isValid ){
			bExhausted = 1;
		}else{
			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}
	}
	if( bExhausted ){
		/* Expression value: inner Generator's return value (state 3) or NULL. */
		ph7_value sResult;
		PH7_MemObjInit(pVm,&sResult);
		if( pCtxFrom->iDelegateState == 3 ){
			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);
			if( pInner && pInner->pCtx ){
				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);
			}
		}
		PH7_MemObjRelease(&pCtxFrom->sDelegate);
		pCtxFrom->pDelegateNode = 0;
		pCtxFrom->iDelegateState = 0;
		pTos++;
		PH7_MemObjStore(&sResult,pTos);
		PH7_MemObjRelease(&sResult);
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sVal);
		break; /* fall through to pc+1 with the result on the stack top */
	}
	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.
	 * The outer generator's implicit auto-key counter is NOT advanced by the
	 * delegated keys — PHP keeps it independent across `yield from`, so a later
	 * plain `yield` continues from the outer's own counter. */
	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);
	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */
	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));
	goto Suspend;
yf_propagate:
	/* A delegate iterator method threw/aborted (or the source was non-iterable):
	 * tear down the delegation, then route via the shared dispatch macro. rcm is
	 * always PH7_EXCEPTION or PH7_ABORT here. */
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjRelease(&pCtxFrom->sDelegate);
	pCtxFrom->pDelegateNode = 0;
	pCtxFrom->iDelegateState = 0;
	PH7_DISPATCH_ENFORCE_RC(rcm)
	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */
}
/*
 * OP_CALL P1 * *
 *  Call a PHP or a foreign function and push the return value of the called
 *  function on the stack.
 */
case PH7_OP_CALL: {
	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;
	ph7_value *pArg;
	pVm->iSpreadExtra = 0; /* Always reset, even if zero */
	pArg = &pTos[-nCallArgs];
	SyHashEntry *pEntry;
	SyString sName;
	/* A Closure object is callable: unwrap it to its underlying string callable so the
	 * dispatch below handles it (rather than treating it as a generic object and looking
	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure
	 * so a plain __invoke object skips the temp-value work entirely. */
	if( VmValueIsClosure(pVm,pTos) ){
		ph7_value sCallable;
		PH7_MemObjInit(pVm,&sCallable);
		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){
			PH7_MemObjRelease(pTos);
			PH7_MemObjStore(&sCallable,pTos);
		}
		PH7_MemObjRelease(&sCallable);
	}
	/* Extract function name */
	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
		if( pTos->iFlags & MEMOBJ_HASHMAP ){
			ph7_value sResult;
			sxi32 rcArr;
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			PH7_MemObjInit(pVm,&sResult);
			/* May be a class instance and it's static method. Forward this call's named-arg map
			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —
			 * mirroring the __invoke-object branch below. */
			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,(VmCallArgMap *)pInstr->p3);
			SySetReset(&aArg);
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			if( rcArr == PH7_ABORT ){
				PH7_MemObjRelease(&sResult);
				goto Abort;
			}
			if( rcArr == PH7_EXCEPTION ){
				/* An array callable ([$obj,'m']()) raised: resume after this frame's
				 * try if it caught the exception in-place, otherwise propagate. */
				sxi32 iResumePc;
				PH7_MemObjRelease(&sResult);
				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
					PH7_MemObjRelease(pTos);
					pc = iResumePc;
					break;
				}
				goto Exception;
			}
			/* Copy result */
			PH7_MemObjStore(&sResult,pTos);
			PH7_MemObjRelease(&sResult);
		}else if( pTos->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
			ph7_value sResult;
			sxi32 rcInv;
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			PH7_MemObjInit(pVm,&sResult);
			rcInv = VmCallObjectInvoke(&(*pVm),pThis,
				(int)SySetUsed(&aArg),
				(ph7_value **)SySetBasePtr(&aArg),
				&sResult,
				(VmCallArgMap *)pInstr->p3);
			SySetReset(&aArg);
			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable
			 * slot itself (it pops top-down and the callable IS pTos), which for a
			 * temporary like (new Plain())(...) holds the only reference — popping it
			 * would free pThis before VmRaiseNotCallable reads its class name below.
			 * Only the not-callable branch dereferences pThis afterwards, so pin just
			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying
			 * the temporary). The other branches let the pop free the temp as before. */
			if( rcInv == SXERR_INVALID ){
				pThis->iRef++;
			}
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			if( rcInv == SXERR_INVALID ){
				/* No __invoke: raise a catchable Error and route through try/catch.
				 * sResult was already released by VmCallObjectInvoke. */
				PH7_MemObjRelease(pTos);
				rc = VmRaiseNotCallable(&(*pVm),pThis);
				PH7_ClassInstanceUnref(pThis);
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
				{
					sxi32 iRp;
					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
						pc = iRp;
						break;
					}
				}
				goto Exception;
			}
			if( rcInv == PH7_ABORT ){
				PH7_MemObjRelease(&sResult);
				goto Abort;
			}
			if( rcInv == PH7_EXCEPTION ){
				/* __invoke raised. The catch body (if any) already ran in-place
				 * inside VmThrowException. If THIS frame's own try caught it,
				 * resume after the try/catch; otherwise propagate so the
				 * exception unwinds through intermediate frames with no handler. */
				sxi32 iResumePc;
				PH7_MemObjRelease(&sResult);
				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
					PH7_MemObjRelease(pTos);
					pc = iResumePc;
					break;
				}
				goto Exception;
			}
			PH7_MemObjStore(&sResult,pTos);
			PH7_MemObjRelease(&sResult);
		}else{
			/* Raise exception: Invalid function name */
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
		}
		break;
	}
	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	/* Check for a compiled function first.
	 * Static names are already namespace-qualified by the compiler.
	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */
	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);
	/* If the compiler qualified this call with a namespace, and the namespaced
	 * function is not found, retry with the global name (strip the namespace
	 * prefix up to the last backslash) before falling back to host functions.
	 * This mirrors PHP's lookup order for unqualified function calls inside
	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */
	{
	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;
	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){
		const char *zFunc;
		const char *zEnd;
		const char *z;
		SyString sGlobal;
		zFunc = sName.zString;
		zEnd  = zFunc + sName.nByte;
		z = zEnd;
		/* Find last namespace separator */
		while( z > zFunc ){
			if( z[-1] == '\\' ){
				break;
			}
			z--;
		}
		if( z > zFunc && z < zEnd ){
			/* Retry lookup using the unqualified/global function name */
			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));
			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);
		}
	}
	} /* end VmCallArgMap namespace scope */
	if( pEntry ){
		ph7_vm_func_arg *aFormalArg;
		ph7_class_instance *pThis;
		ph7_value *pFrameStack;
		ph7_vm_func *pVmFunc;
		ph7_class *pSelf;
		ph7_class *pSelfHint;
		VmFrame *pFrame;
		ph7_value *pObj;
		VmSlot sArg;
		sxu32 n;
		int bClosureThis = 0;
		ph7_class *pClosureScope = 0;
		/* initialize fields */
		pVmFunc = (ph7_vm_func *)pEntry->pUserData;
		pThis = 0;
		pSelf = 0;
		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it
		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever
		 * set for a bound plain closure, which dispatches as a function, so the method branch below
		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */
		if( pVm->pClosureThis ){
			pThis = pVm->pClosureThis;
			pVm->pClosureThis = 0;
			bClosureThis = 1;
			pClosureScope = pVm->pClosureScope;
			pVm->pClosureScope = 0;
		}
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
					VmFrame *pFrameLocal = pVm->pFrame;
					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
					if( pFrameLocal->pParent ){
						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */
						pThis = pFrameLocal->pThis;
						if( pThis ){
							pThis->iRef++;
						}
					}
				}
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				/* Synchronize pointers */
				pArg = &pTos[-nCallArgs];
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
						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){
							/* Throw Error exception (PHP-compatible) */
							char zMsg[256];
							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",
								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,
								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);
							/* Pop given arguments */
							if( nCallArgs > 0 ){
								VmPopOperand(&pTos,nCallArgs);
							}
							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);
							goto Abort;
						}
					}
				}
			}
		}
		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).
		 * Default is unbounded (heap-bound recursion, decoupled from the C stack
		 * by the stage-2 trampoline); the C stack is guarded separately by
		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and
		 * then raises a clean non-catchable fatal (was: silently set NULL and
		 * continue) and halts. */
		if( VmRecursionExceeded(pVm) ){
			/* Args and the function-name slot are released by the Abort label,
			 * which walks the whole operand stack — don't release them here. */
			VmRecursionFatal(&(*pVm));
			goto Abort;
		}
		if( pVmFunc->pNextName ){
			/* Function is candidate for overloading,select the appropriate function to call */
			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));
		}
		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's
		 * `Class::method`). Computed after overload resolution so it reflects the selected method.
		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed
		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method
		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the
		 * USING class; we don't carry the using class on the shared struct, so fall back to the
		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for
		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */
		pSelfHint = pSelf;
		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){
			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;
			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){
				pSelfHint = pDecl;
			}
		}
		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){
			/* Generator function: return a Generator object instead of executing */
			ph7_exec_ctx *pExecCtx;
			ph7_generator *pGenerator;
			ph7_class_instance *pGenObj;
			ph7_value *pCtxAttr;
			SyString sAttrName;
			ph7_value **apCallArgs;
			int nGenArgs, iArg;
			/* Collect arguments from the operand stack */
			nGenArgs = (int)(pTos - pArg);
			apCallArgs = 0;
			if( nGenArgs > 0 ){
				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,
					nGenArgs * sizeof(ph7_value *));
				if( apCallArgs == 0 ){
					/* OOM: fall back to zero args rather than NULL-deref */
					nGenArgs = 0;
				}else{
					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;
					int didReorder = 0;
					if( pGenMap && pGenMap->bHasNamed ){
						/* Named-argument reordering for generator */
						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
						sxu32 nF = SySetUsed(&pVmFunc->aArgs);
						sxu32 nNV = nF;
						sxi32 iVIdx = -1;
						sxi32 *aGSlot;
						sxu8 *aGUsed;
						sxu32 gi;
						for( gi = 0; gi < nF; gi++ ){
							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }
						}
						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,
							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));
						if( aGSlot ){
							aGUsed = (sxu8 *)&aGSlot[nGenArgs];
							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,
								(sxu32)nGenArgs,aGSlot,aGUsed);
							if( rc == PH7_ABORT ){
								SyMemBackendFree(&pVm->sAllocator, aGSlot);
								SyMemBackendFree(&pVm->sAllocator, apCallArgs);
								goto Abort;
							}
							/* Build apCallArgs in formal-parameter order, then
							 * append overflow (variadic / positional beyond
							 * formals) so downstream sees every argument. */
							{
								int nOut = 0;
								for( gi = 0; gi < nNV; gi++ ){
									sxu32 gj;
									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){
										if( aGSlot[gj] == (sxi32)gi ){
											apCallArgs[nOut++] = &pArg[gj];
											break;
										}
									}
								}
								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){
									if( aGSlot[gi] == -1 || aGSlot[gi] == -2 ){
										apCallArgs[nOut++] = &pArg[gi];
									}
								}
								nGenArgs = nOut;
							}
							SyMemBackendFree(&pVm->sAllocator, aGSlot);
							didReorder = 1;
						}
						/* If aGSlot allocation failed, fall through to
						 * positional fill below — preserves arg order rather
						 * than passing an uninitialized apCallArgs. */
					}
					if( !didReorder ){
						for( iArg = 0; iArg < nGenArgs; iArg++ ){
							apCallArgs[iArg] = &pArg[iArg];
						}
					}
				}
			}
			/* Create execution context and generator wrapper */
			pExecCtx = VmNewExecCtx(pVm, pVmFunc);
			if( pExecCtx == 0 ){
				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);
				VmErrorFormat(&(*pVm), PH7_CTX_ERR,
					"Out of memory while creating generator for '%z'", &pVmFunc->sName);
				break;
			}
			pGenerator = VmNewGenerator(pVm, pExecCtx);
			if( pGenerator == 0 ){
				VmReleaseExecCtx(pVm, pExecCtx);
				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);
				VmErrorFormat(&(*pVm), PH7_CTX_ERR,
					"Out of memory while creating generator for '%z'", &pVmFunc->sName);
				break;
			}
			/* Set up the frame with arguments, closure env, $this */
			pExecCtx->pFrame->pParent = pVm->pFrame;
			pVm->pFrame = pExecCtx->pFrame;
			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);
			pVm->pFrame = pExecCtx->pFrame->pParent;
			pExecCtx->pFrame->pParent = 0;
			if( apCallArgs ){
				SyMemBackendFree(&pVm->sAllocator, apCallArgs);
			}
			if( rc != SXRET_OK ){
				VmReleaseGenerator(pVm, pGenerator);
				if( pThis ){
					PH7_ClassInstanceUnref(pThis);
				}
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
				break;
			}
			/* Create Generator class instance */
			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);
			if( pGenObj == 0 ){
				VmReleaseGenerator(pVm, pGenerator);
				break;
			}
			/* Store generator in __ctx attribute */
			SyStringInitFromBuf(&sAttrName, "__ctx", 5);
			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);
			if( pCtxAttr ){
				pCtxAttr->x.pOther = pGenerator;
				MemObjSetType(pCtxAttr, MEMOBJ_RES);
			}
			/* Pop args and function name, push Generator object */
			PH7_MemObjRelease(pTos);
			pTos = &pTos[-nCallArgs];
			pTos->x.pOther = pGenObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
			pGenObj->iRef++;
			if( pThis ){
				PH7_ClassInstanceUnref(pThis);
			}
			break;
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
			/* The frame that would own (and later release) $this never got created; for a bound
			 * plain closure the consumed transient is the object's ONLY ref, so release it here
			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */
			if( bClosureThis && pThis ){
				PH7_ClassInstanceUnref(pThis);
			}
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
			break;
		}
		if( bClosureThis && pClosureScope ){
			/* Bound plain closure with an explicit scope: private/protected access inside the body
			 * resolves against it (Closure::bindTo($o, Scope::class) / call($o)). */
			pFrame->pBoundScope = pClosureScope;
		}
		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) || bClosureThis) ){
			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */
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
							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);
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
		{
		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;
		/* Caller file's strict_types mode — governs parameter coercion
		 * (but NOT return coercion, which uses the callee's file). */
		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;
		if( pCallMap3 && pCallMap3->bHasNamed ){
			/* ============================================================
			 * Named-argument matching path (PHP 8.0)
			 *
			 * Resolve each actual argument to its formal parameter by name
			 * or position, then install them in the frame.
			 * ============================================================ */
			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);
			sxu32 nActual = (sxu32)(pTos - pArg);
			sxi32 iVariadicIdx = -1;
			sxu32 nNonVariadic;
			sxi32 *aSlot;
			sxu8  *aUsed;
			sxu32 i;
			/* Find variadic parameter index */
			for( i = 0; i < nFormal; i++ ){
				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){
					iVariadicIdx = (sxi32)i;
					break;
				}
			}
			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;
			/* Allocate mapping arrays */
			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,
				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));
			if( aSlot == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");
				goto Abort;
			}
			aUsed = (sxu8 *)&aSlot[nActual];
			/* Resolve named arguments to formal parameters */
			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,
				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);
			if( rc == PH7_ABORT ){
				SyMemBackendFree(&pVm->sAllocator, aSlot);
				goto Abort;
			}
			/* Pass 2: install arguments into the frame by formal parameter order */
			for( n = 0; n < nNonVariadic; n++ ){
				/* Find the stack arg mapped to formal n */
				sxi32 iSrc = -1;
				for( i = 0; i < nActual; i++ ){
					if( aSlot[i] == (sxi32)n ){
						iSrc = (sxi32)i;
						break;
					}
				}
				if( iSrc >= 0 ){
					/* Argument was provided — install with type checking */
					ph7_value *pVal = &pArg[iSrc];
					/* NULL-to-default redirect (existing behavior) */
					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0
						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){
						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal,FALSE);
						if( rc == PH7_ABORT ) goto Abort;
					}
					/* Type checking: union types */
					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){
						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,
							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,
							bCallIsStrict);
						if( rcU != SXRET_OK ){
							const char *zGiven;
							const char *zExpected = "union";
							char zBuf[128];
							char zTypeBuf[128];
							if( pVal->iFlags & MEMOBJ_OBJ ){
								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));
							}else if( pVal->iFlags & MEMOBJ_NULL ){
								zGiven = "null";
							}else{
								zGiven = ph7_type_name(pVal);
							}
							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){
								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));
							}
							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
								&aFormalArg[n].sName, zExpected, zGiven);
							if( rc == PH7_ABORT ) goto Abort;
							SyMemBackendFree(&pVm->sAllocator, aSlot);
							PH7_MemObjRelease(pTos);
							pTos = &pTos[-nCallArgs];
							pFrameStack = 0;
							rc = PH7_EXCEPTION;
							goto SkipFuncBody;
						}
					}else if( aFormalArg[n].nType > 0
						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){
						/* Scalar/class type checking */
						if( aFormalArg[n].nType == SXU32_HIGH ){
							SyString *pName = &aFormalArg[n].sClass;
							ph7_class *pClass;
							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);
							if( rcPseudo == 0 ){
								/* Recognised pseudo-type (true/false/iterable); value mismatches */
								char zTypeBuf[128],zGivenBuf[128];
								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
									&aFormalArg[n].sName,
									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),
									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));
								if( rc == PH7_ABORT ) goto Abort;
								SyMemBackendFree(&pVm->sAllocator, aSlot);
								PH7_MemObjRelease(pTos);
								pTos = &pTos[-nCallArgs];
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
							}
							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class.
							 * Resolve via VmResolveTypeClass so `self`/`parent` resolve and
							 * interface/abstract hints are included (iLoadable=FALSE), then throw a
							 * catchable TypeError on mismatch — matching PHP — instead of the legacy
							 * warn + NULL-coerce (which silently ran the body with a corrupted arg). */
							pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);
							if( pClass ){
								/* An explicit null to a non-nullable class param stays a lenient
								 * pass-through (PHP throws; that needs the param nullable/default
								 * model — out of scope here). */
								int bBad = (pVal->iFlags & MEMOBJ_OBJ) == 0
									? ((pVal->iFlags & MEMOBJ_NULL) == 0)
									: !PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass);
								if( bBad ){
									char zTypeBuf[128],zGivenBuf[128];
									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
										&aFormalArg[n].sName,
										VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),
										VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));
									if( rc == PH7_ABORT ) goto Abort;
									SyMemBackendFree(&pVm->sAllocator, aSlot);
									PH7_MemObjRelease(pTos);
									pTos = &pTos[-nCallArgs];
									pFrameStack = 0;
									rc = PH7_EXCEPTION;
									goto SkipFuncBody;
								}
							}
						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){
							if( aFormalArg[n].nType == MEMOBJ_OBJ ){
								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
									&aFormalArg[n].sName,"object",ph7_type_name(pVal));
								if( rc == PH7_ABORT ) goto Abort;
								SyMemBackendFree(&pVm->sAllocator, aSlot);
								PH7_MemObjRelease(pTos);
								pTos = &pTos[-nCallArgs];
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){
								char zTypeBuf[128];
								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
									&aFormalArg[n].sName,
									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),
									ph7_type_name(pVal));
								if( rc == PH7_ABORT ) goto Abort;
								SyMemBackendFree(&pVm->sAllocator, aSlot);
								PH7_MemObjRelease(pTos);
								pTos = &pTos[-nCallArgs];
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
							}
						}
					}
					/* Install: by reference or by value */
					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){
						if( pVal->nIdx == SXU32_HIGH ){
							if( (pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0 ){
								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
									"Function '%z',%d argument: Pass by reference,expecting a variable not a "
									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);
							}
							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
						}else{
							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,
								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));
							if( pRefEntry == 0 ){
								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),
									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));
								sArg.nIdx = pVal->nIdx;
								sArg.pUserData = 0;
								SySetPut(&pFrame->sArg,(const void *)&sArg);
							}
							pObj = 0;
						}
					}else{
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
					}
					if( pObj ){
						PH7_MemObjStore(pVal,pObj);
						sArg.nIdx = pObj->nIdx;
						sArg.pUserData = 0;
						SySetPut(&pFrame->sArg,(const void *)&sArg);
					}
				}else{
					/* Argument was NOT provided — use default or leave unset */
					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){
						/* Should not reach here; variadic handled separately below */
					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
						if( pObj ){
							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);
							if( rc == PH7_ABORT ) goto Abort;
							sArg.nIdx = pObj->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ
								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){
								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
								if( xCast ) xCast(pObj);
							}
						}
					}
					/* else: required param missing — leave unset (matches existing behavior) */
				}
			}
			/* Handle variadic parameter */
			if( iVariadicIdx >= 0 ){
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);
				if( pObj ){
					PH7_MemObjToHashmap(pObj);
					{
						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;
						for( i = 0; i < nActual; i++ ){
							if( aSlot[i] == -1 ){
								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){
									/* Named variadic entry: insert with string key */
									ph7_value sKey;
									PH7_MemObjInit(pVm, &sKey);
									PH7_MemObjStringAppend(&sKey,
										pCallMap3->aNames[i].zString,
										(sxu32)pCallMap3->aNames[i].nByte);
									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);
									PH7_MemObjRelease(&sKey);
								}else{
									/* Positional variadic entry */
									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);
								}
							}
						}
					}
					sArg.nIdx = pObj->nIdx;
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
				}
			}else{
				/* No variadic — preserve unresolved positional overflow
				 * (aSlot[i] == -2) as anonymous frame args so
				 * func_get_args() / func_num_args() still see them, matching
				 * the positional-only path's behavior. */
				sxu32 nAnon = nNonVariadic;
				for( i = 0; i < nActual; i++ ){
					if( aSlot[i] == -2 ){
						char zAnonBuf[32];
						SyString sAnonName;
						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),
							"[%u]apArg",nAnon);
						sAnonName.zString = zAnonBuf;
						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);
						if( pObj ){
							PH7_MemObjStore(&pArg[i],pObj);
							sArg.nIdx = pObj->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
						}
						nAnon++;
					}
				}
			}
			/* Release all stack arguments */
			for( i = 0; i < nActual; i++ ){
				PH7_MemObjRelease(&pArg[i]);
			}
			SyMemBackendFree(&pVm->sAllocator, aSlot);
			/* Set n to nFormal so the defaults loop below is skipped */
			n = nFormal;
		}else{
		/* ============================================================
		 * Positional-only matching path (original)
		 * ============================================================ */
		n = 0;
		while( pArg < pTos ){
			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){
				/* Variadic parameter: collect all remaining args into an array */
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,
					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after
					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a
					 * separately-allocated hashmap and stays valid across the realloc. */
					sxu32 nVariadicIdx;
					/* Initialize as empty array */
					PH7_MemObjToHashmap(pObj);
					nVariadicIdx = pObj->nIdx;
					{
						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;
						while( pArg < pTos ){
							/* Variadic union type: per-element coercion via the shared helper.
							 *
							 * TODO: PHP reports the runtime element index here
							 * ("Argument #3 must be...") but we report the formal-arg
							 * index (always n+1, the position of the variadic). The
							 * non-union variadic path below has the same limitation;
							 * fixing both wants a separate counter for elements
							 * already packed into the variadic array. */
							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){
								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,
									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,
									bCallIsStrict);
								if( rcU != SXRET_OK ){
									const char *zGiven;
									const char *zExpected = "union";
									char zBuf[128];
									char zTypeBuf[128];
									if( pArg->iFlags & MEMOBJ_OBJ ){
										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));
									}else if( pArg->iFlags & MEMOBJ_NULL ){
										zGiven = "null";
									}else{
										zGiven = ph7_type_name(pArg);
									}
									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){
										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));
									}
									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
										&aFormalArg[n].sName, zExpected, zGiven);
									if( rc == PH7_ABORT ){
										goto Abort;
									}
									PH7_MemObjRelease(pTos);
									pTos = &pTos[-nCallArgs];
									pFrameStack = 0;
									rc = PH7_EXCEPTION;
									goto SkipFuncBody;
								}
								PH7_HashmapInsert(pMap, 0, pArg);
								pArg++;
								continue;
							}
							/* Apply type coercion to each element if the variadic has a type hint.
							 * Nullable types (?type) allow null through without coercion. */
							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH
								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))
								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){
								if( aFormalArg[n].nType == MEMOBJ_OBJ ){
									/* object type hint on variadic: reject non-objects with TypeError */
									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
										&aFormalArg[n].sName,"object",ph7_type_name(pArg));
									if( rc == PH7_ABORT ){
										goto Abort;
									}
									/* Skip function body, route through normal cleanup */
									PH7_MemObjRelease(pTos);
									pTos = &pTos[-nCallArgs];
									pFrameStack = 0;
									rc = PH7_EXCEPTION;
									goto SkipFuncBody;
								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){
									char zTypeBuf[128];
									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
										&aFormalArg[n].sName,
										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),
										ph7_type_name(pArg));
									if( rc == PH7_ABORT ){
										goto Abort;
									}
									PH7_MemObjRelease(pTos);
									pTos = &pTos[-nCallArgs];
									pFrameStack = 0;
									rc = PH7_EXCEPTION;
									goto SkipFuncBody;
								}
							}
							PH7_HashmapInsert(pMap, 0, pArg);
							pArg++;
						}
					}
					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
				}
				break; /* All remaining args consumed */
			}
			if( n < SySetUsed(&pVmFunc->aArgs) ){
				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0
					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){
					/* NULL values are redirected to default arguments (but not for nullable types) */
					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg,FALSE);
					if( rc == PH7_ABORT ){
						goto Abort;
					}
				}
				/* Union type: dispatch to the shared coercion helper. */
				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){
					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,
						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,
						bCallIsStrict);
					if( rcU != SXRET_OK ){
						const char *zGiven;
						const char *zExpected = "union";
						char zBuf[128];
						char zTypeBuf[128];
						if( pArg->iFlags & MEMOBJ_OBJ ){
							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));
						}else if( pArg->iFlags & MEMOBJ_NULL ){
							zGiven = "null";
						}else{
							zGiven = ph7_type_name(pArg);
						}
						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){
							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));
						}
						rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
							&aFormalArg[n].sName, zExpected, zGiven);
						if( rc == PH7_ABORT ){
							goto Abort;
						}
						PH7_MemObjRelease(pTos);
						pTos = &pTos[-nCallArgs];
						pFrameStack = 0;
						rc = PH7_EXCEPTION;
						goto SkipFuncBody;
					}
				}else
				/* Make sure the given arguments are of the correct type.
				 * Nullable types (?type) allow null through without coercion. */
				if( aFormalArg[n].nType > 0
					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){
					if ( aFormalArg[n].nType == SXU32_HIGH ){
						/* Argument must be a class instance [i.e: object] */
						SyString *pName = &aFormalArg[n].sClass;
						ph7_class *pClass;
						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);
						if( rcPseudo == 0 ){
							/* Recognised pseudo-type (true/false/iterable); value mismatches */
							char zTypeBuf[128],zGivenBuf[128];
							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
								&aFormalArg[n].sName,
								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),
								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));
							if( rc == PH7_ABORT ) goto Abort;
							PH7_MemObjRelease(pTos);
							pTos = &pTos[-nCallArgs];
							pFrameStack = 0;
							rc = PH7_EXCEPTION;
							goto SkipFuncBody;
						}
						/* rcPseudo==1 accepts a pseudo-type; -1 real class. Resolve via
						 * VmResolveTypeClass (self/parent + interface/abstract, iLoadable=FALSE)
						 * and throw a catchable TypeError on mismatch — matching PHP — instead of
						 * the legacy warn + NULL-coerce. (Symmetric with the positional path.) */
						pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);
						if( pClass ){
							/* An explicit null to a non-nullable class param stays a lenient
							 * pass-through (PHP throws; needs the param nullable/default model). */
							int bBad = (pArg->iFlags & MEMOBJ_OBJ) == 0
								? ((pArg->iFlags & MEMOBJ_NULL) == 0)
								: !PH7_VmInstanceOf(((ph7_class_instance *)pArg->x.pOther)->pClass,pClass);
							if( bBad ){
								char zTypeBuf[128],zGivenBuf[128];
								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
									&aFormalArg[n].sName,
									VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),
									VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));
								if( rc == PH7_ABORT ) goto Abort;
								PH7_MemObjRelease(pTos);
								pTos = &pTos[-nCallArgs];
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
							}
						}
					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){
						if( aFormalArg[n].nType == MEMOBJ_OBJ ){
							/* object type hint: reject non-objects with TypeError */
							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
								&aFormalArg[n].sName,"object",ph7_type_name(pArg));
							if( rc == PH7_ABORT ){
								goto Abort;
							}
							/* Skip function body, route through normal cleanup */
							PH7_MemObjRelease(pTos);
							pTos = &pTos[-nCallArgs];
							pFrameStack = 0;
							rc = PH7_EXCEPTION;
							goto SkipFuncBody;
						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){
							char zTypeBuf[128];
							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,
								&aFormalArg[n].sName,
								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),
								ph7_type_name(pArg));
							if( rc == PH7_ABORT ){
								goto Abort;
							}
							PH7_MemObjRelease(pTos);
							pTos = &pTos[-nCallArgs];
							pFrameStack = 0;
							rc = PH7_EXCEPTION;
							goto SkipFuncBody;
						}
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
				SyString sArgName;
				/* Set a dummy name */
				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);
				sArgName.zString = zName;
				/* Annonymous argument */
				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);
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
		} /* end named vs positional branch */
		/* Set up closure environment */
		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){
			ph7_vm_func_closure_env *aEnv,*pEnv;
			ph7_value *pValue;
			sxu32 iEnv;
			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);
			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){
				pEnv = &aEnv[iEnv];
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
		/* Process default values for remaining formal parameters */
		while( n < SySetUsed(&pVmFunc->aArgs) ){
			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){
				/* Variadic parameter with no extra args — create empty array */
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					PH7_MemObjToHashmap(pObj);
					sArg.nIdx = pObj->nIdx;
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
				}
				n++;
				break; /* Variadic is always last */
			}
			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					/* Evaluate the default value and extract it's result */
					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);
					if( rc == PH7_ABORT ){
						goto Abort;
					}
					/* Insert argument index */
					sArg.nIdx = pObj->nIdx;
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
					/* Make sure the default argument is of the correct type */
					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ
						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						/* Cast to the desired type */
						xCast(pObj);
					}
				}
			}
			++n;
		}
		} /* end VmCallArgMap scope */
		/* Pop arguments,function name from the operand stack and assume the function
		 * does not return anything.
		 */
		PH7_MemObjRelease(pTos);
		pTos = &pTos[-nCallArgs];
		/* Allocate an operand stack (via the recycling allocator) and evaluate the
		 * function body. Size it to a tight static bound when the body is statically
		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,
		 * where one such stack lives per frame — falling back to the safe
		 * instruction-count bound otherwise.
		 *
		 * The bound is computed LAZILY on the first call and cached on the func
		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:
		 * self-computing on first use is fail-safe against any body-creation path
		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),
		 * where undersizing is a heap overflow; the amortized cost is one analysis
		 * per function. */
		{
			sxu32 nSlots = pVmFunc->nMaxStack;
			if( nSlots == 0 ){
				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);
				sxu32 nTight = VmComputeMaxStack(&(*pVm),
					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);
				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;
				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */
				pVmFunc->nMaxStack = nSlots;
			}
			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);
		}
		if( pFrameStack == 0 ){
			/* Raise exception: Out of memory */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",
				&pVmFunc->sName);
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			break;
		}
SkipFuncBody:
		if( pSelf ){
			/* Push class name */
			SySetPut(&pVm->aSelf,(const void *)&pSelf);
		}
		/* Increment nesting level */
		pVm->nRecursionDepth++;
		if( rc == PH7_EXCEPTION ){
			/* Arg-binding threw: there is no body to run — finish the call
			 * immediately (no record is pushed). */
			VmCallRecord sCallee;
			sCallee.pVmFunc = pVmFunc;
			sCallee.pFrame = pFrame;
			sCallee.pFrameStack = pFrameStack;
			sCallee.nLastRef = SXU32_HIGH;
			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);
			sState.pTos = pTos;
			sState.pc = pc;
			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);
			pTos = sState.pTos;
			pc = sState.pc;
			if( rc == PH7_ABORT ){
				/* Abort processing immeditaley */
				goto Abort;
			}else if( rc == PH7_SUSPEND ){
				goto Suspend;
			}else if( rc == PH7_EXCEPTION ){
				goto Exception;
			}
		}else{
			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a
			 * call record (caller activation + in-flight call) and switch the
			 * loop's locals to the callee — a PHP->PHP call no longer grows
			 * the native stack. The record node is pool-allocated so
			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */
			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;
			if( pRec ){
				pVm->pIdleCallFrames = (void *)pRec->pPrev;
			}else{
				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));
			}
			if( pRec == 0 ){
				/* OOM: undo the push-time accounting, tear the call down and
				 * raise the non-catchable fatal (the §3.1 OOM convention —
				 * never a silent NULL). */
				pVm->nRecursionDepth--;
				if( pSelf ){
					(void)SySetPop(&pVm->aSelf);
				}
				SyMemBackendFree(&pVm->sAllocator,pFrameStack);
				VmLeaveFrame(&(*pVm));
				PH7_VmMemoryError(&(*pVm));
				goto Abort;
			}
			sState.pTos = pTos;
			sState.pc = pc;
			pRec->sCaller = sState;
			pRec->sCall.pVmFunc = pVmFunc;
			pRec->sCall.pFrame = pFrame;
			pRec->sCall.pFrameStack = pFrameStack;
			pRec->sCall.nLastRef = SXU32_HIGH;
			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);
			pRec->pPrev = pCallTop;
			pCallTop = pRec;
			/* Switch to the callee activation (what the recursive
			 * VmByteCodeExec entry used to set up). */
			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);
			pStack = pFrameStack;
			pTos = &pStack[-1];
			pc = 0;
			sState.aInstr = aInstr;
			sState.pStack = pStack;
			sState.pTos = pTos;
			sState.pc = 0;
			sState.nExceptionBase = SySetUsed(&pVm->aException);
			sState.pEntryFrame = pVm->pFrame;
			sState.pResult = pRec->sCaller.pTos;
			sState.pLastRef = &pRec->sCall.nLastRef;
			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;
			sState.is_callback = 0;
			sState.bReturnPropagates = 0;
			goto VmLoopFetch;
		}
	}else{
		ph7_user_func *pFunc;
		ph7_context sCtx;
		ph7_value sRet;
		/* Look for an installed foreign function.
		 * Host functions are registered with short names (strlen, etc.).
		 * If the compiler namespace-qualified the name, extract the short
		 * name (last component after \) and try that. This implements PHP's
		 * global fallback for unqualified function calls in namespaces. */
		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);
		{
		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;
		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){
			/* Compiler-qualified: try short name as global fallback */
			const char *zShort = sName.zString;
			sxu32 i;
			for( i = 0; i < sName.nByte; i++ ){
				if( sName.zString[i] == '\\' ){
					zShort = &sName.zString[i + 1];
				}
			}
			if( zShort != sName.zString ){
				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));
				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);
			}
		}
		} /* end VmCallArgMap namespace scope */
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
			/* Release the (possibly partially-built) result slot before unwinding;
			 * the Abort: label only frees the operand stack, not this local
			 * (mirrors the PH7_EXCEPTION branch below). */
			PH7_MemObjRelease(&sRet);
			goto Abort;
		}else if( rc == PH7_EXCEPTION ){
			/* A callback invoked by this host function threw. If an in-place catch
			 * recorded a resume target owned by THIS body, resume at its landing pad
			 * (consuming the target); otherwise the exception was caught by an outer
			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW
			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which
			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */
			sxi32 iResumePc;
			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
				/* Caught by an outer exec, or not caught here: propagate. */
				goto Exception;
			}
			/* Exception was caught in place by THIS body's try: pop args and the
			 * result slot to restore the pre-try stack, then resume. */
			PH7_MemObjRelease(&sRet);
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			VmPopOperand(&pTos,1);
			pc = iResumePc;
			break;
		}
		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){
			/* Fiber::suspend() was called from within a fiber.
			 * Pop arguments (like normal path) but don't push a return value.
			 * Propagate PH7_SUSPEND up. If this is the fiber's own
			 * VmByteCodeExec, the CALL was to a foreign function directly
			 * and we need to save state here. If it's a nested call (method
			 * body), the user-function path above will handle re-saving. */
			PH7_MemObjRelease(&sRet);
			if( pInstr->iP1 > 0 ){
				VmPopOperand(&pTos,pInstr->iP1);
			}
			/* Save fiber state: pc+1 is the instruction after this CALL.
			 * nTos is one below pTos so resume pushes at the return-value slot. */
			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);
			goto Suspend;
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
			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));
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
	/* A stacked callee completing lands here too (its result is already in
	 * sState.pResult — the caller's operand slot); Unwind's first iteration
	 * bottoms out identically for the record-less case. */
	rc = SXRET_OK;
	goto Unwind;
Suspend:
	rc = PH7_SUSPEND;
	if( pCallTop != 0 ){
		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment
		 * instead of the lossy unwind. pc/nTos of the innermost activation were
		 * already saved into the ctx by VmSuspendCtx; capture the rest (the
		 * record chain, the innermost activation, the suspend-time top frame)
		 * so resume re-enters HERE, inside the innermost callee, like php. The
		 * records / frames / operand stacks stay alive — nothing is freed. Only
		 * fibers reach this (generators yield only at their body level, pCallTop
		 * == 0); a suspend inside a C->PHP callback was already rejected with a
		 * FiberError before it could arrive here. */
		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));
		if( pSeg == 0 ){
			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST
			 * pc/nTos into the ctx, so the lossy body-level fallback would resume
			 * the callee's pc against the body stack — silent corruption, exactly
			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the
			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */
			PH7_VmMemoryError(&(*pVm));
			rc = PH7_ABORT;
			goto Unwind;
		}
		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */
		pSeg->sState = sState;
		pSeg->pCallTop = pCallTop;
		pSeg->pTopFrame = pVm->pFrame;
		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;
		{
			VmCallFrame *pRec;
			pSeg->nRecords = 0;
			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){
				pSeg->nRecords++;
			}
		}
		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;
		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */
		SySetRelease(&aArg);
		return PH7_SUSPEND;
	}
	goto Unwind;
Abort:
	rc = PH7_ABORT;
	goto Unwind;
Exception:
	rc = PH7_EXCEPTION;
	goto Unwind;
Unwind:
	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration
	 * finishes the top record exactly as the old per-level native return did:
	 * for ABORT/EXCEPTION, first run what the popped activation's own
	 * Abort/Exception label used to do (clear its pending return, release its
	 * operands — a stacked activation never has bReturnPropagates set), then
	 * VmCallFinish routes in the restored caller (an in-place catch there
	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —
	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible
	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).
	 * At the bottom, VmExecFinalize hands the status to the native caller. */
	for(;;){
		if( pCallTop == 0 ){
			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);
		}
		if( rc == PH7_ABORT || rc == PH7_EXCEPTION ){
			VmClearFrameReturn(sState.pEntryFrame);
			while( pTos >= pStack ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
		}
		{
			VmCallFrame *pRec = pCallTop;
			sState = pRec->sCaller;
			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);
			pCallTop = pRec->pPrev;
			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;
			pVm->pIdleCallFrames = (void *)pRec;
			aInstr = sState.aInstr;
			pStack = sState.pStack;
			pTos = sState.pTos;
			pc = sState.pc;
		}
		if( rc == PH7_OK ){
			pc++; /* the loop-bottom increment this OP_CALL missed */
			goto VmLoopFetch;
		}
	}
}
/*
 * Execute as much of a local PH7 bytecode program as we can then return.
 * This function is a wrapper around [VmByteCodeExec()].
 * See block-comment on that function for additional information.
 */
PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)
{
	ph7_value *pStack;
	sxi32 rc;
	/* Allocate a new operand stack */
	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));
	if( pStack == 0 ){
		return SXERR_MEM;
	}
	/* Execute the program */
	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0);
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
	/* A halt that led us here is consumed; a fresh one set by a callback
	 * (i.e. exit() inside a shutdown function) skips the remaining
	 * callbacks, mirroring PHP.
	 */
	pVm->bHaltRequested = 0;
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
			if( pVm->bHaltRequested ){
				/* exit() inside the callback: skip the remaining callbacks */
				break;
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
	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0);
	/* Invoke any shutdown callbacks */
	VmInvokeShutdownCallbacks(&(*pVm));
	/*
	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number
	 * so that any following call to [ph7_vm_exec()] without calling
	 * [ph7_vm_reset()] first would fail.
	 */
	return SXRET_OK;
}
/* ======================== Fiber Infrastructure ======================== */
/*
 * Allocate and initialize a new execution context for a fiber.
 * The context is in CREATED state and ready to be started.
 */
static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)
{
	ph7_exec_ctx *pCtx;
	ph7_value *pStack;
	VmFrame *pFrame;
	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));
	if( pCtx == 0 ){
		return 0;
	}
	SyZero(pCtx, sizeof(ph7_exec_ctx));
	pCtx->pVm = pVm;
	pCtx->pFunc = pFunc;
	pCtx->iState = PH7_CTX_STATE_CREATED;
	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */
	pCtx->pc = 0;
	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);
	PH7_MemObjInit(pVm, &pCtx->sRetValue);
	PH7_MemObjInit(pVm, &pCtx->sDelegate);
	/* Container for this body's own exception handlers while suspended (borrowed
	 * ph7_exception* pointers — never freed here, owned by the compiled func). */
	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));
	/* ROOT C: this body's own pending finally actions while suspended. */
	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));
	pCtx->nFinallyBase = 0;
	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context
	 * pushed by nested method calls still open at suspend) parked while suspended,
	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */
	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));
	pCtx->nSelfBase = 0;
	pCtx->pParkedSegment = 0;
	pCtx->nBodyExecDepth = 0;
	/* Allocate a private operand stack */
	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));
	if( pStack == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
		return 0;
	}
	pCtx->pStack = pStack;
	/* Create a detached frame for the fiber */
	pFrame = VmNewFrame(pVm, pFunc, 0);
	if( pFrame == 0 ){
		SyMemBackendFree(&pVm->sAllocator, pStack);
		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
		return 0;
	}
	pCtx->pFrame = pFrame;
	return pCtx;
}
/*
 * A suspended coroutine must not leave its own slices of the VM's shared stacks
 * sitting above the caller's depth. Three stacks are affected, identically:
 *   - pVm->aException: its exception handlers — else a generator/fiber suspended
 *     inside a try leaves handlers referencing its now-detached frame on the
 *     global stack, corrupting the caller's try/catch.
 *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield
 *     inside a finally (reached by return/break/rethrow) leaves a record where an
 *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.
 *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open
 *     nested method calls — else they sit on the resumer's aSelf and corrupt its
 *     self:: resolution.
 * Each is the same operation: on suspend move the slice above a captured base
 * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller
 * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type
 * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.
 *
 * Stage 4: the whole suspended segment stays alive, so a parked handler's owner
 * frame is never freed underneath it — the parked pointer stays valid and is kept
 * (the old stage-2b lossy-path invalidation is gone with the discard). A
 * body-level suspend only ever has body-owned handlers here, and its finally/self
 * slices are empty (all nested calls already returned) — so those are no-ops.
 */
static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)
{
	sxu32 nUsed = SySetUsed(pFrom);
	if( nUsed > nBase ){
		const char *aBase = (const char *)SySetBasePtr(pFrom);
		sxu32 i;
		for( i = nBase; i < nUsed; i++ ){
			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));
		}
		SySetTruncate(pFrom, nBase);
	}
}
static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)
{
	sxu32 i, n = SySetUsed(pSaved);
	if( n > 0 ){
		const char *aSaved = (const char *)SySetBasePtr(pSaved);
		for( i = 0; i < n; i++ ){
			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));
		}
		SySetReset(pSaved);
	}
}
static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);
	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);
	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);
}
static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);
	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);
	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);
}
/*
 * On suspend, free the exception (try) frames the yield was nested in. They were
 * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and
 * the current suspend-point top frame. The generator/fiber frame model saves only
 * the body frame, so these transparent wrappers would otherwise be orphaned and
 * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping
 * with a yield in a try). Freeing them loses nothing the resume needs: this body's
 * exception HANDLERS are parked separately (VmParkCtxState) and each
 * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION
 * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and
 * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must
 * run while pVm->pFrame still points at the suspend-time top (before the detach).
 */
static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		VmLeaveFrame(&(*pVm));
	}
}
/*
 * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended
 * coroutine from the live VM chain and park its exception handlers. Two forms:
 *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending
 *     directly in its body. The try wrappers the yield sat in are transient —
 *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the
 *     body frame alone.
 *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body
 *     frame + the nested call/try frames above it) stays alive and is detached
 *     as a unit; nothing is freed, so resume can continue inside the innermost
 *     callee. Its handlers are parked the same way and rebased on resume.
 */
static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)
{
	if( pCtx->pParkedSegment == 0 ){
		VmFreeSuspendedExceptionFrames(pVm, pCtx);
	}else{
		/* The parked records' push-time accounting (one nRecursionDepth++ each,
		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment
		 * counted as active while the fiber is suspended — deactivate it. aSelf
		 * is parked wholesale below (base-relative), so drop only the depth. */
		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;
	}
	pVm->pFrame = pCtx->pFrame->pParent;
	pCtx->pFrame->pParent = 0;
	VmParkCtxState(pVm, pCtx);
	if( pResult ){
		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);
	}
}
/*
 * Start executing a fiber context for the first time.
 */
static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)
{
	ph7_exec_ctx *pOldCtx;
	sxi32 rc;
	if( pCtx->iState != PH7_CTX_STATE_CREATED ){
		return SXERR_INVALID;
	}
	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by
	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM
	 * state, so the abort is clean — the wrapper's own check fires only after
	 * this function has spliced the coroutine into the frame chain, which its
	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to
	 * OP_CALL only (BYTECODE.md stage 5). */
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	/* Attach the fiber's frame to the VM frame chain */
	pCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pCtx->pFrame;
	/* Save and set the active context */
	pOldCtx = pVm->pActiveCtx;
	pVm->pActiveCtx = pCtx;
	pCtx->iState = PH7_CTX_STATE_RUNNING;
	pCtx->nExceptionBase = SySetUsed(&pVm->aException);
	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);
	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);
	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a
	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a
	 * FiberError instead of parking across the native frame (stage 4). */
	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;
	/* Execute from the beginning. A GENERATOR FUNCTION's declared return type
	 * belongs to the call site (always a Generator object, validated at compile
	 * time — "must be a supertype of Generator"); the body's own return value
	 * feeds getReturn() and is never type-checked. Gate on the function's
	 * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-
	 * linkage fact): a Fiber given a generator-flagged callable runs the body
	 * with pPrivate == 0 and must not enforce either. Ordinary fiber callables
	 * keep enforcement (php enforces their return type). */
	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),
		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,
		((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc)) ? pCtx->pFunc : 0, FALSE, 0);
	/* Restore the previous context */
	pVm->pActiveCtx = pOldCtx;
	if( rc == PH7_SUSPEND ){
		VmSuspendCtxDetach(pVm, pCtx, pResult);
		return SXRET_OK;
	}
	/* Detach frame */
	if( pVm->pFrame == pCtx->pFrame ){
		pVm->pFrame = pCtx->pFrame->pParent;
		pCtx->pFrame->pParent = 0;
	}
	if( rc == PH7_ABORT ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		return PH7_EXCEPTION;
	}
	/* Normal completion. The final return value belongs to getReturn() only —
	 * start()/resume() return the NEXT suspend value, which is null when the
	 * coroutine completes instead of suspending (php parity). Leave pResult at
	 * its caller-initialized null; sRetValue is read separately by getReturn().
	 * (Generators pass pResult == NULL and are unaffected.) */
	pCtx->iState = PH7_CTX_STATE_COMPLETED;
	return SXRET_OK;
}
/*
 * Resume a suspended fiber context.
 */
static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)
{
	ph7_exec_ctx *pOldCtx;
	VmParkedSegment *pSeg;
	sxi32 rc;
	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		return SXERR_INVALID;
	}
	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.
	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the
	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level
	 * abort past those mutations would leave pVm->pFrame pointing into the parked
	 * callee and the depth accounting un-reverted. The PHP call-depth cap is
	 * OP_CALL-only (BYTECODE.md stage 5). */
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	/* Push the resume value onto the SUSPENDED activation's operand stack so it
	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)
	 * that stack is the innermost callee's — parked in the segment — not the
	 * body's. nTos was saved one below the return-value slot. */
	{
		ph7_value *pResumeStack;
		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;
		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;
		if( pResumeValue ){
			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);
		}else{
			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);
		}
		pCtx->nTos++;
		/* Refresh the caller-depth base and re-publish this body's own exception
		 * handlers on top of pVm->aException at that depth, so the resumed body's
		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base
		 * override). Must run before VmByteCodeExec recaptures its local base. */
		pCtx->nExceptionBase = SySetUsed(&pVm->aException);
		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);
		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);
		VmRestoreCtxState(pVm, pCtx);
		if( pSeg ){
			/* Reactivate the parked records' recursion accounting (mirror of the
			 * deactivate at suspend); aSelf was just restored above. */
			pVm->nRecursionDepth += pSeg->nRecords;
			/* Rebase the parked segment's absolute exception-floor indices: the
			 * fiber may resume at a different caller depth than it suspended at,
			 * so every activation's nExceptionBase shifts by the same delta the
			 * republished handlers moved (newBase - the park-time base). */
			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;
			if( iDelta != 0 ){
				VmCallFrame *pRec;
				pSeg->sState.nExceptionBase =
					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);
				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){
					pRec->sCaller.nExceptionBase =
						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);
				}
			}
		}
		/* Re-attach the coroutine to the live VM frame chain: the body frame's
		 * parent becomes the resumer's current frame. For a deep segment the
		 * suspend-time top frame (the innermost callee / open-try wrapper) then
		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the
		 * callee; body-level resumes make the body frame current. */
		pCtx->pFrame->pParent = pVm->pFrame;
		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;
	}
	/* The segment (if any) is handed to the body invocation explicitly below; it
	 * is no longer part of the suspended ctx state once resume owns it. */
	pCtx->pParkedSegment = 0;
	/* Save and set the active context */
	pOldCtx = pVm->pActiveCtx;
	pVm->pActiveCtx = pCtx;
	pCtx->iState = PH7_CTX_STATE_RUNNING;
	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */
	/* Resume execution from saved PC. Generator-function bodies skip
	 * return-type enforcement — see the block comment in VmStartCtx. pSeg, when
	 * non-NULL, makes this body re-enter inside the innermost parked callee. */
	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),
		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,
		((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc)) ? pCtx->pFunc : 0, FALSE, pSeg);
	/* Restore the previous context */
	pVm->pActiveCtx = pOldCtx;
	if( rc == PH7_SUSPEND ){
		/* Suspended again — same detach as VmStartCtx. Crucially this handles a
		 * deep RE-suspend (a resumed segment that parks a fresh segment): the
		 * shared helper deactivates the new segment's recursion accounting, parks
		 * its aSelf, and (for a segment) skips VmFreeSuspendedExceptionFrames so
		 * it can't free the still-live parked try wrappers. */
		VmSuspendCtxDetach(pVm, pCtx, pResult);
		return SXRET_OK;
	}
	/* Detach frame */
	if( pVm->pFrame == pCtx->pFrame ){
		pVm->pFrame = pCtx->pFrame->pParent;
		pCtx->pFrame->pParent = 0;
	}
	if( rc == PH7_ABORT ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		return PH7_EXCEPTION;
	}
	/* Normal completion. The final return value belongs to getReturn() only —
	 * start()/resume() return the NEXT suspend value, which is null when the
	 * coroutine completes instead of suspending (php parity). Leave pResult at
	 * its caller-initialized null; sRetValue is read separately by getReturn().
	 * (Generators pass pResult == NULL and are unaffected.) */
	pCtx->iState = PH7_CTX_STATE_COMPLETED;
	return SXRET_OK;
}
/*
 * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a
 * suspended coroutine's body, or of a segment activation abandoned mid-call.
 * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop
 * from). Factored so the body-frame free and the stage-4 segment free share it.
 */
static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)
{
	VmSlot *aSlot;
	sxu32 n;
	if( pFrame == 0 ){
		return;
	}
	/* Free local variables */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);
	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){
		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);
	}
	/* Remove local references */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);
	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){
		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);
	}
	SyHashRelease(&pFrame->hVar);
	SySetRelease(&pFrame->sArg);
	SySetRelease(&pFrame->sLocal);
	SySetRelease(&pFrame->sRef);
	PH7_MemObjRelease(&pFrame->sRet);
	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */
	VmDropResumeTarget(pVm,pFrame);
	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);
}
/*
 * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while
 * suspended. Every record holds a callee's operand stack and VmFrame (the
 * topmost record's callee is the innermost activation, running on sState); walk
 * the chain releasing each callee stack's live entries then the stack and frame.
 * The body frame/stack are NOT here — they are freed by the caller
 * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.
 */
static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)
{
	/* Live top-of-stack of the activation running on the current record's callee
	 * stack: the innermost (sState) for the topmost record, then each caller. */
	ph7_value *pTosAbove = pSeg->sState.pTos;
	VmCallFrame *pRec = pSeg->pCallTop, *pNext;
	while( pRec ){
		ph7_value *pStk = pRec->sCall.pFrameStack;
		if( pStk ){
			ph7_value *pTos = pTosAbove;
			while( pTos >= pStk ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			SyMemBackendFree(&pVm->sAllocator, pStk);
		}
		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);
		/* The caller recorded here runs on the NEXT-lower callee stack; grab its
		 * live tos before freeing this node. */
		pTosAbove = pRec->sCaller.pTos;
		pNext = pRec->pPrev;
		SyMemBackendPoolFree(&pVm->sAllocator, pRec);
		pRec = pNext;
	}
	/* pTosAbove now points at the BODY activation's live top (the bottom record's
	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST
	 * index (VmSuspendCtx saved it), which would over-index the body stack in
	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */
	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);
	SyMemBackendFree(&pVm->sAllocator, pSeg);
}
/*
 * Release an execution context and all its resources.
 */
static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	if( pCtx == 0 ){
		return;
	}
	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){
		/* Cannot destroy a fiber that is currently executing */
		return;
	}
	pCtx->iState = PH7_CTX_STATE_CLOSED;
	/* Release values */
	PH7_MemObjRelease(&pCtx->sSuspendValue);
	PH7_MemObjRelease(&pCtx->sRetValue);
	PH7_MemObjRelease(&pCtx->sDelegate);
	/* Stage 2b: the parked entries are per-activation clones now — free them
	 * (an abandoned suspended coroutine is their last holder). */
	VmExcReleaseAll(pVm,&pCtx->aSavedException);
	SySetRelease(&pCtx->aSavedException);
	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked
	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).
	 * Release them so the abandon path leaks nothing. */
	{
		sxu32 n = SySetUsed(&pCtx->aSavedFinally);
		if( n > 0 ){
			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);
			sxu32 i;
			for( i = 0; i < n; i++ ){
				if( aA[i].eKind == PH7_FA_RETURN ){
					PH7_MemObjRelease(&aA[i].sRet);
				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){
					PH7_ClassInstanceUnref(aA[i].pExc);
				}
			}
		}
		SySetRelease(&pCtx->aSavedFinally);
	}
	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */
	SySetRelease(&pCtx->aSavedSelf);
	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while
	 * suspended inside a nested call, so its record chain / frames / operand
	 * stacks are still alive and only this holder references them. Must run
	 * before the body frame/stack below (they are the segment's floor). */
	if( pCtx->pParkedSegment ){
		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);
		pCtx->pParkedSegment = 0;
	}
	/* Release the frame if it's detached (not in the VM chain) */
	if( pCtx->pFrame ){
		VmFreeDetachedFrame(pVm, pCtx->pFrame);
		pCtx->pFrame = 0;
	}
	/* Release individual operand stack entries (decrement refcounts,
	 * free string buffers, etc.) before bulk-freeing the stack memory.
	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */
	if( pCtx->pStack ){
		if( pCtx->nTos >= 0 ){
			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];
			while( pTos >= pCtx->pStack ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
		}
		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);
		pCtx->pStack = 0;
	}
	/* Free the context itself */
	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
}
/*
 * Helper: extract the ph7_exec_ctx from a Fiber class instance.
 * Returns NULL if the object is not a Fiber or has no context.
 */
static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)
{
	ph7_class_instance *pThis;
	SyString sAttr;
	ph7_value *pAttr;
	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pFiberObj->x.pOther;
	if( pThis->pClass != pVm->pFiberClass ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__ctx", 5);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pAttr == 0 || (pAttr->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (ph7_exec_ctx *)pAttr->x.pOther;
}
/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and
 * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.
 * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */
#define VM_INSTANCE_FCC_BOUND 0x004
/*
 * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of
 * the built-in final `Closure` class carrying its underlying callable in a private
 * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a
 * user/host function name) — plus, for a method/static first-class callable, a bound
 * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes
 * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the
 * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is
 * no extra free path.
 *
 * Returns non-zero iff pVal is a Closure instance.
 */
static int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)
{
	ph7_class_instance *pThis;
	/* Flag test first: a non-object call target (the hot common case) bails before any
	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 || pVal->x.pOther == 0 || pVm->pClosureClass == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pVal->x.pOther;
	/* Closure is final, so an exact class match is correct (no subclasses possible). */
	return pThis->pClass == pVm->pClosureClass;
}
/*
 * Unwrap a Closure value into the simple callable the existing dispatch machinery
 * already understands, written into pOut (which the caller must have initialised):
 *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable
 *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable
 *   - neither (plain function / real closure)          -> the `$__fn` name string
 * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this
 * for an object first element and resolves the class for a class-name-string first element.
 * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.
 */
static sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)
{
	ph7_class_instance *pThis;
	ph7_value *pFn;
	SyString sAttr;
	if( !VmValueIsClosure(pVm, pVal) ){
		return SXERR_NOTFOUND;
	}
	pThis = (ph7_class_instance *)pVal->x.pOther;
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pFn == 0 || (pFn->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pFn->sBlob) == 0 ){
		return SXERR_NOTFOUND; /* malformed/uninitialised closure */
	}
	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the
	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path
	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */
	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){
		ph7_value *pBound, *pScope;
		int bBoundObj, bScope;
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);
		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;
		if( bBoundObj || bScope ){
			/* Method/static first-class callable -> [ target, "method" ] array callable. */
			ph7_hashmap *pMap;
			ph7_value sTarget, sMeth;
			sxi32 rc;
			if( bBoundObj ){
				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;
				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a
				 * method of the bound object's class, so a [obj,method] array callable would fail
				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function
				 * frame setup injects it as $this, and return the plain $__fn string for a normal
				 * function dispatch. */
				if( PH7_ClassExtractMethod(pBoundObj->pClass,
						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){
					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and
					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST
					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and
					 * dispatches via the host path which never consumes the transient — setting it
					 * there would leak the ref and inject a stale $this into the next call. */
					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),
							SyBlobLength(&pFn->sBlob)) != 0 ){
						pBoundObj->iRef++;
						pVm->pClosureThis = pBoundObj;
						/* Carry the bound scope (if set) so private/protected member access inside
						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */
						if( bScope ){
							pVm->pClosureScope = PH7_VmExtractClass(pVm,
								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);
						}
					}
					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
					return SXRET_OK;
				}
			}
			pMap = PH7_NewHashmap(&(*pVm), 0, 0);
			if( pMap == 0 ){
				return SXERR_NOTFOUND;
			}
			PH7_MemObjInit(pVm, &sTarget);
			PH7_MemObjInit(pVm, &sMeth);
			if( bBoundObj ){
				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */
			}else{
				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));
			}
			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
			rc = PH7_HashmapInsert(pMap, 0, &sTarget);
			if( rc == SXRET_OK ){
				rc = PH7_HashmapInsert(pMap, 0, &sMeth);
			}
			PH7_MemObjRelease(&sTarget);
			PH7_MemObjRelease(&sMeth);
			if( rc != SXRET_OK ){
				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */
				return SXERR_NOTFOUND;
			}
			pOut->x.pOther = pMap;
			MemObjSetType(pOut, MEMOBJ_HASHMAP);
			return SXRET_OK;
		}
	}
	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
	return SXRET_OK;
}
/*
 * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a
 * class-name STRING value: handles the self/static/parent keywords against the live class
 * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to
 * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the
 * class cannot be resolved.
 */
static ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)
{
	const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);
	sxu32 nCls = (sxu32)SyBlobLength(&pTarget->sBlob);
	ph7_class *pClass;
	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){
		pClass = PH7_VmPeekDeclaringClass(&(*pVm));
		if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){
			pClass = PH7_VmPeekTopClass(&(*pVm)); /* self:: in a trait -> using class */
		}
	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */
	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
		ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));
		pClass = (pSelf && pSelf->pBase) ? pSelf->pBase : 0;
	}else{
		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
	}
	return pClass;
}
/*
 * Create a Closure object wrapping a callable name (+ optional bound $this object and/or
 * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).
 * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.
 * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.
 */
static ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,
	ph7_class_instance *pBoundThis, const SyString *pScope)
{
	ph7_class_instance *pObj;
	ph7_value *pAttr;
	SyString sAttr;
	if( pVm->pClosureClass == 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);
	if( pObj == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);
	if( pAttr ){
		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);
	}
	if( pBoundThis ){
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);
		if( pAttr ){
			pAttr->x.pOther = pBoundThis;
			MemObjSetType(pAttr, MEMOBJ_OBJ);
			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */
		}
	}
	if( pScope && pScope->nByte ){
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);
		if( pAttr ){
			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);
		}
	}
	if( pBoundThis || (pScope && pScope->nByte) ){
		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope
		 * lookups on the hot plain-closure dispatch path. */
		pObj->iFlags |= VM_INSTANCE_FCC_BOUND;
	}
	return pObj;
}
/*
 * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.
 * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same
 * object the method/static first-class-callable paths mint, so dispatch round-trips identically
 * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:
 *   - a function-NAME string          -> plain closure ($__fn = name)
 *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name
 *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable
 *   - an __invoke object               -> closure bound to the object's __invoke
 * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this
 * stays idempotent even for a direct caller. Returns the fresh instance (iRef == 0; caller takes the
 * reference) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in
 * which case the caller leaves the value untouched (graceful degradation). This is the generic
 * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future
 * Closure::bind/fromCallable work (Increment 2) can call it directly.
 */
static ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)
{
	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch
	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a
	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */
	if( VmValueIsClosure(pVm, pValue) ){
		return 0;
	}
	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){
		return 0;
	}
	if( pValue->iFlags & MEMOBJ_STRING ){
		SyString sName;
		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));
		return VmCreateClosure(pVm, &sName, 0, 0);
	}
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		/* [target, method] — same two-slot decode PH7_VmIsCallable uses to validate it. */
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		ph7_value *pTarget, *pMeth;
		SyString sName;
		if( pMap->nEntry != 2 ){
			return 0;
		}
		pTarget = (ph7_value *)SySetAt(&pVm->aMemObj, pMap->pFirst->nValIdx);
		pMeth   = (ph7_value *)SySetAt(&pVm->aMemObj, pMap->pFirst->pPrev->nValIdx);
		if( pTarget == 0 || pMeth == 0 || (pMeth->iFlags & MEMOBJ_STRING) == 0
			|| SyBlobLength(&pMeth->sBlob) == 0 ){
			return 0;
		}
		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;
			return VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);
		}else{
			/* [class-name, method] static callable -> bind the resolved scope. A runtime array
			 * callable carries a concrete class name (never self/static/parent), so a plain class
			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve
			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */
			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);
			return pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;
		}
	}
	if( pValue->iFlags & MEMOBJ_OBJ ){
		/* __invoke object (a real Closure is intercepted by the caller before this point). */
		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;
		SyString sInvoke;
		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);
		return VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);
	}
	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all
	 * handled above; kept to satisfy the non-void return path. */
	return 0;
}
/*
 * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the
 * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.
 */
static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)
{
	if( pClosure == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjRelease(pCtx->pRet);
	pClosure->iRef++;
	pCtx->pRet->x.pOther = pClosure;
	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);
	return PH7_OK;
}
/*
 * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its
 * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the
 * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.
 * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);
 * this drops that and takes one on pNewThis.
 */
static void VmClosureRebind(ph7_class_instance *pClone,
	ph7_class_instance *pNewThis, const SyString *pScope)
{
	SyString sAttr;
	ph7_value *pThisAttr, *pScopeAttr;
	int bBound = 0;
	SyStringInitFromBuf(&sAttr, "__this", 6);
	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
	if( pThisAttr ){
		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —
		 * do NOT also decrement by hand (that double-frees the original bound object). */
		PH7_MemObjRelease(pThisAttr);
		if( pNewThis ){
			pThisAttr->x.pOther = pNewThis;
			MemObjSetType(pThisAttr, MEMOBJ_OBJ);
			pNewThis->iRef++;
		}
	}
	if( pScope ){
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
		if( pScopeAttr ){
			PH7_MemObjRelease(pScopeAttr);
			if( pScope->nByte ){
				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);
			}
		}
	}
	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already
	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it
	 * just for pScope != 0. */
	SyStringInitFromBuf(&sAttr, "__scope", 7);
	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))
		|| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){
		bBound = 1;
	}
	if( bBound ){
		pClone->iFlags |= VM_INSTANCE_FCC_BOUND;
	}else{
		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;
	}
}
/*
 * Resolve the bindTo/bind/call $scope argument to a class-name SyString.
 * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");
 * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).
 */
static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)
{
	if( pScopeArg == 0 ){
		return 0; /* keep */
	}
	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6
		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){
		return 0; /* "static" -> keep current scope */
	}
	if( pScopeArg->iFlags & MEMOBJ_NULL ){
		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */
		return 1;
	}
	if( pScopeArg->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;
		*pOut = pScopeObj->pClass->sName;
		return 1;
	}
	if( pScopeArg->iFlags & MEMOBJ_STRING ){
		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));
		return 1;
	}
	return 0;
}
/*
 * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').
 * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure
 * receiver, matching PHP's failure mode).
 */
static int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pClosure, *pNewThis, *pClone;
	ph7_value *pNewThisArg;
	SyString sScope;
	const SyString *pScopePtr = 0;
	if( nArg < 2 || !VmValueIsClosure(pVm, apArg[0]) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClosure = (ph7_class_instance *)apArg[0]->x.pOther;
	pNewThisArg = apArg[1];
	if( pNewThisArg->iFlags & MEMOBJ_NULL ){
		pNewThis = 0;
	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){
		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;
	}else{
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");
	}
	if( VmClosureResolveScope((nArg > 2) ? apArg[2] : 0, &sScope) ){
		pScopePtr = &sScope;
	}
	pClone = PH7_CloneClassInstance(pClosure);
	if( pClone == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	VmClosureRebind(pClone, pNewThis, pScopePtr);
	return VmClosureResult(pCtx, pClone);
}
/*
 * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the
 * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.
 */
static int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pClosure;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::fromCallable() expects exactly 1 argument, 0 given");
	}
	if( VmValueIsClosure(pVm, apArg[0]) ){
		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */
		return PH7_OK;
	}
	pClosure = VmFccWrapValue(pVm, apArg[0]);
	if( pClosure == 0 ){
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::fromCallable(): Argument #1 ($callback) is not a valid callback");
	}
	return VmClosureResult(pCtx, pClosure);
}
/*
 * Fiber::suspend($value = null) — static method.
 * Suspends the currently running fiber and passes $value to the caller.
 */
static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( pVm->pActiveCtx == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot suspend outside of a fiber");
	}
	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs
	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec
	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across
	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback
	 * (usort/array_map/preg_replace_callback comparator), and — because fibers
	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a
	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d
	 * code, all of which run via VmLocalExec. php does all of these via full
	 * native-stack switching; PHL raises a catchable FiberError instead of the
	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)
	 * runs in the main dispatch loop and parks normally. Recorded in PLAN.md
	 * §3.9; making the catch/finally case work needs fibers on the inline
	 * try machinery (the generator ROOT C path), a follow-up. */
	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot suspend across an internal call boundary");
	}
	if( nArg > 0 ){
		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);
	}else{
		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);
	}
	return PH7_SUSPEND;
}
/*
 * __fiber_construct($this, $callable) — validate and store the callable.
 * Actual resolution is deferred to start() so that overload selection
 * and closure-environment binding happen with the correct argument context.
 */
static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis;
	ph7_value *pAttr;
	SyString sAttrName;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct() expects a callable argument");
	}
	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct(): invalid $this");
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	if( pThis->pClass != pCtx->pVm->pFiberClass ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct(): $this is not a Fiber instance");
	}
	/* Basic validation: callable must be a string or closure (object) */
	if( (apArg[1]->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ)) == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct() expects a callable (string or closure)");
	}
	/* Store callable in $this->__callable for deferred resolution at start() */
	SyStringInitFromBuf(&sAttrName, "__callable", 10);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pAttr ){
		PH7_MemObjStore(apArg[1], pAttr);
	}
	return PH7_OK;
}
/*
 * Resolve the callable stored in a Fiber's $__callable attribute.
 * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).
 * If the callable is a closure (object), *ppThis is set to the closure instance
 * so that start() can bind it as $this for the closure environment.
 */
static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,
	ph7_class_instance **ppThis)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pCallable;
	SyString sAttrName;
	*ppThis = 0;
	SyStringInitFromBuf(&sAttrName, "__callable", 10);
	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);
	if( pCallable == 0 || (pCallable->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ)) == 0 ){
		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");
		return 0;
	}
	if( pCallable->iFlags & MEMOBJ_STRING ){
		/* String callable — look up in user functions with overload support */
		SyString sName;
		SyHashEntry *pEntry;
		ph7_vm_func *pFunc;
		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));
		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);
		if( pEntry == 0 ){
			PH7_VmThrowException(pCtx, "FiberError",
				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);
			return 0;
		}
		pFunc = (ph7_vm_func *)pEntry->pUserData;
		return pFunc;
	}else{
		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;
		ph7_class_method *pMethod;
		if( VmValueIsClosure(pVm, pCallable) ){
			/* A real Closure object: unwrap to its underlying callable name (the single
			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured
			 * environment (including any `$this`) rides along in the named function's
			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */
			ph7_value sName;
			SyHashEntry *pEntry = 0;
			PH7_MemObjInit(pVm, &sName);
			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){
				pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&sName.sBlob), SyBlobLength(&sName.sBlob));
			}
			PH7_MemObjRelease(&sName);
			if( pEntry ){
				return (ph7_vm_func *)pEntry->pUserData;
			}
			PH7_VmThrowException(pCtx, "FiberError", "Fiber callable closure could not be resolved");
			return 0;
		}
		/* Object callable — resolve __invoke method */
		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",
			sizeof("__invoke") - 1);
		if( pMethod == 0 ){
			PH7_VmThrowException(pCtx, "FiberError",
				"Fiber callable object has no __invoke method");
			return 0;
		}
		*ppThis = pClosure;
		return &pMethod->sFunc;
	}
}
/*
 * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:
 * type casting, pass-by-reference handling, default values, and closure environment.
 * The fiber's frame must be at the top of pVm->pFrame when this is called.
 */
static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,
	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)
{
	ph7_vm_func *pFunc = pExecCtx->pFunc;
	ph7_vm_func_arg *aFormalArg;
	sxu32 nFormal, n;
	VmSlot sSlot;
	sxi32 rc;
	/* Install $this for closure/method callables */
	if( pClosureThis ){
		static const SyString sThis = { "this", sizeof("this") - 1 };
		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);
		if( pObj ){
			pObj->x.pOther = pClosureThis;
			MemObjSetType(pObj, MEMOBJ_OBJ);
			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */
		}
	}
	/* Install static variables */
	if( SySetUsed(&pFunc->aStatic) > 0 ){
		ph7_vm_func_static_var *aStatic;
		ph7_value *pVal;
		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);
		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){
			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);
			if( pVal ){
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);
				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,
					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));
				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){
					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);
				}
			}
		}
	}
	/* Install arguments with type casting and default values (matching OP_CALL) */
	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	nFormal = SySetUsed(&pFunc->aArgs);
	for( n = 0; n < nFormal; n++ ){
		ph7_value *pObj;
		if( n < (sxu32)nArg ){
			/* Argument provided — install with type casting */
			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);
			if( pObj ){
				PH7_MemObjStore(apArg[n], pObj);
				/* Type casting */
				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){
					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						if( xCast ){
							xCast(pObj);
						}
					}
				}
				sSlot.nIdx = pObj->nIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
			}
		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
			/* Default value */
			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);
			if( pObj ){
				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);
				if( rc == SXERR_ABORT ){
					return rc;
				}
				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){
					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						if( xCast ){
							xCast(pObj);
						}
					}
				}
				sSlot.nIdx = pObj->nIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
			}
		}
	}
	/* Install closure environment (captured variables) */
	if( pFunc->iFlags & VM_FUNC_CLOSURE ){
		ph7_vm_func_closure_env *aEnv, *pEnv;
		ph7_value *pValue;
		sxu32 iEnv;
		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){
			pEnv = &aEnv[iEnv];
			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){
				continue;
			}
			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);
			if( pValue == 0 ){
				continue;
			}
			PH7_MemObjRelease(pValue);
			PH7_MemObjStore(&pEnv->sValue, pValue);
		}
	}
	return SXRET_OK;
}
/*
 * Fiber->start(...$args) — resolve callable, create exec context, install
 * arguments/closure-env/$this (matching OP_CALL semantics), and start.
 * apArg[0] = $this, apArg[1] = func_get_args() array
 */
static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis;
	ph7_class_instance *pClosureThis;
	ph7_exec_ctx *pExecCtx;
	ph7_vm_func *pFunc;
	ph7_value sResult;
	ph7_value *pCtxAttr;
	SyString sAttrName;
	sxi32 rc;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	/* Check if already started (has a __ctx) */
	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);
	if( pExecCtx != 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot start a fiber that has already been started");
	}
	/* Resolve callable */
	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);
	if( pFunc == 0 ){
		return PH7_EXCEPTION;
	}
	/* Create execution context now that we know the function */
	pExecCtx = VmNewExecCtx(pVm, pFunc);
	if( pExecCtx == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::start(): out of memory");
	}
	/* Store context in $this->__ctx */
	SyStringInitFromBuf(&sAttrName, "__ctx", 5);
	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pCtxAttr ){
		pCtxAttr->x.pOther = pExecCtx;
		MemObjSetType(pCtxAttr, MEMOBJ_RES);
	}
	/* Temporarily attach the fiber's frame to the VM chain so that
	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables
	 * into the fiber's frame, not the caller's. */
	pExecCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pExecCtx->pFrame;
	/* Unpack the args array and install into the frame */
	{
		ph7_value **apValues = 0;
		ph7_value *aStore = 0;
		int nActual = 0;
		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){
			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;
			ph7_hashmap_node *pNode;
			sxu32 nCount = pMap->nEntry;
			if( nCount > 0 ){
				sxu32 idx = 0;
				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,
					nCount * sizeof(ph7_value *));
				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,
					nCount * sizeof(ph7_value));
				if( apValues && aStore ){
					pNode = pMap->pFirst;
					while( pNode && idx < nCount ){
						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves
						 * memory objects (VmExtractMemObj) before reading the args, which can
						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A
						 * shallow copy is a safe source — the referent and the heap-resident
						 * blob data survive the move (same sSafeVal idiom the hashmap inserters
						 * use); it owns nothing independently, so it needs no release. */
						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);
						if( pSrc ){
							aStore[idx] = *pSrc;
						}else{
							PH7_MemObjInit(pVm, &aStore[idx]);
						}
						apValues[idx] = &aStore[idx];
						idx++;
						pNode = pNode->pPrev;
					}
					nActual = (int)idx;
				}
			}
		}
		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);
		if( aStore ){
			SyMemBackendFree(&pVm->sAllocator, aStore);
		}
		if( apValues ){
			SyMemBackendFree(&pVm->sAllocator, apValues);
		}
	}
	/* Detach the frame — VmStartCtx will re-attach it */
	pVm->pFrame = pExecCtx->pFrame->pParent;
	pExecCtx->pFrame->pParent = 0;
	if( rc != SXRET_OK ){
		return PH7_ABORT;
	}
	PH7_MemObjInit(pVm, &sResult);
	rc = VmStartCtx(pVm, pExecCtx, &sResult);
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
		return PH7_EXCEPTION;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Fiber->resume($value = null) — resume a suspended fiber.
 */
static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	ph7_value sResult;
	ph7_value *pResumeVal;
	sxi32 rc;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");
		return PH7_OK;
	}
	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);
	if( pExecCtx == 0 ){
		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");
		return PH7_OK;
	}
	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot resume a fiber that is not suspended");
	}
	pResumeVal = (nArg > 1) ? apArg[1] : 0;
	PH7_MemObjInit(pVm, &sResult);
	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
		return PH7_EXCEPTION;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Fiber->getReturn() — get the fiber's return value after it has terminated.
 */
static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);
	if( pExecCtx == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){
		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){
			return PH7_VmThrowException(pCtx, "FiberError",
				"Cannot get fiber return value: The fiber has not been started");
		}
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot get fiber return value: The fiber has not returned");
	}
	ph7_result_value(pCtx, &pExecCtx->sRetValue);
	return PH7_OK;
}
/*
 * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()
 */
static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);
	return PH7_OK;
}
static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);
	return PH7_OK;
}
static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);
	return PH7_OK;
}
static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);
	return PH7_OK;
}
/*
 * Fiber->__destruct() — clean up the execution context.
 */
static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	if( nArg < 1 ){
		return PH7_OK;
	}
	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);
	if( pExecCtx ){
		VmReleaseExecCtx(pVm, pExecCtx);
		/* Clear the attribute so double-free is prevented */
		if( apArg[0]->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;
			SyString sAttrName;
			ph7_value *pAttr;
			SyStringInitFromBuf(&sAttrName, "__ctx", 5);
			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
			if( pAttr ){
				PH7_MemObjRelease(pAttr);
			}
		}
	}
	return PH7_OK;
}
/* ======================== Fiber Public API Helpers ======================== */
PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)
{
	ph7_class_instance *pThis;
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;
	pThis = (ph7_class_instance *)pVal->x.pOther;
	return pThis->pClass == pVm->pFiberClass;
}
PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)
{
	ph7_class_instance *pThis;
	ph7_class_instance *pClosureThis = 0;
	ph7_exec_ctx *pCtx;
	ph7_vm_func *pFunc;
	ph7_value *pCallable;
	ph7_value *pCtxAttr;
	SyString sAttrName;
	/* Must not already be started */
	pCtx = VmFiberExtractCtx(pVm, pFiber);
	if( pCtx != 0 ){
		return SXERR_INVALID;
	}
	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){
		return SXERR_INVALID;
	}
	pThis = (ph7_class_instance *)pFiber->x.pOther;
	/* Get the callable */
	SyStringInitFromBuf(&sAttrName, "__callable", 10);
	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pCallable == 0 ){
		return SXERR_INVALID;
	}
	/* Resolve callable */
	if( pCallable->iFlags & MEMOBJ_STRING ){
		SyString sName;
		SyHashEntry *pEntry;
		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));
		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);
		if( pEntry == 0 ){
			return SXERR_NOTFOUND;
		}
		pFunc = (ph7_vm_func *)pEntry->pUserData;
	}else if( pCallable->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;
		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",
			sizeof("__invoke") - 1);
		if( pMethod == 0 ){
			return SXERR_INVALID;
		}
		pClosureThis = pClosure;
		pFunc = &pMethod->sFunc;
	}else{
		return SXERR_INVALID;
	}
	/* Create context */
	pCtx = VmNewExecCtx(pVm, pFunc);
	if( pCtx == 0 ){
		return SXERR_MEM;
	}
	/* Store in __ctx */
	SyStringInitFromBuf(&sAttrName, "__ctx", 5);
	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pCtxAttr ){
		pCtxAttr->x.pOther = pCtx;
		MemObjSetType(pCtxAttr, MEMOBJ_RES);
	}
	/* Set up frame with args */
	pCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pCtx->pFrame;
	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);
	pVm->pFrame = pCtx->pFrame->pParent;
	pCtx->pFrame->pParent = 0;
	return VmStartCtx(pVm, pCtx, pResult);
}
PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	if( pCtx == 0 ) return SXERR_INVALID;
	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);
}
PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;
}
PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;
}
PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	if( pCtx == 0 || pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;
	return &pCtx->sRetValue;
}
/* ======================== Generator Infrastructure ======================== */
/*
 * Allocate a new generator wrapper around an execution context.
 */
static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	ph7_generator *pGen;
	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));
	if( pGen == 0 ){
		return 0;
	}
	SyZero(pGen, sizeof(ph7_generator));
	pGen->pCtx = pCtx;
	pGen->iImplicitKey = 0;
	PH7_MemObjInit(pVm, &pGen->sYieldValue);
	PH7_MemObjInit(pVm, &pGen->sYieldKey);
	/* Link the generator back to the exec context */
	pCtx->pPrivate = pGen;
	return pGen;
}
/*
 * Release a generator and its execution context.
 */
static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)
{
	if( pGen == 0 ){
		return;
	}
	PH7_MemObjRelease(&pGen->sYieldValue);
	PH7_MemObjRelease(&pGen->sYieldKey);
	if( pGen->pCtx ){
		pGen->pCtx->pPrivate = 0;
		VmReleaseExecCtx(pVm, pGen->pCtx);
		pGen->pCtx = 0;
	}
	SyMemBackendPoolFree(&pVm->sAllocator, pGen);
}
/*
 * Extract ph7_generator from a Generator class instance.
 */
static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)
{
	ph7_class_instance *pThis;
	SyString sAttr;
	ph7_value *pAttr;
	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pGenObj->x.pOther;
	if( pThis->pClass != pVm->pGeneratorClass ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__ctx", 5);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pAttr == 0 || (pAttr->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (ph7_generator *)pAttr->x.pOther;
}
/*
 * Generator::rewind() — start if CREATED, no-op otherwise.
 */
static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	if( nArg < 1 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ) return PH7_OK;
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
	return PH7_OK;
}
/*
 * Generator::valid() — true if suspended at a yield point.
 */
static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);
	return PH7_OK;
}
/*
 * Generator::current() — return the last yielded value.
 * Auto-starts the generator on first access (like PHP).
 */
static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		ph7_result_value(pCtx, &pGen->sYieldValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Generator::key() — return the last yielded key.
 * Auto-starts the generator on first access (like PHP).
 */
static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		ph7_result_value(pCtx, &pGen->sYieldKey);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Generator::next() — advance to the next yield point.
 */
static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	if( nArg < 1 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ) return PH7_OK;
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);
	}else{
		return PH7_OK;
	}
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	return PH7_OK;
}
/*
 * Generator::send($value) — resume and send a value into the generator.
 */
static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	ph7_value *pSendVal;
	sxi32 rc;
	if( nArg < 1 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	pSendVal = (nArg > 1) ? apArg[1] : 0;
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		/* First send starts the generator; sent value is ignored per PHP semantics */
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);
	}else{
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		ph7_result_value(pCtx, &pGen->sYieldValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Generator::throw($exception) — throw an exception into the generator.
 *
 * PHP semantics: the exception is injected AT the suspended yield point so the
 * generator's OWN try/catch (if any wraps the yield) can handle it and the body
 * resumes after the try; otherwise it propagates to the throw() caller and the
 * generator closes. We implement this by resuming the body with a pending
 * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via
 * the existing OP_THROW / ROOT B resume route — no exception frame is
 * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.
 * A never-started generator is first run to its first yield, then injected there;
 * a finished/closed generator has no suspend point, so the exception is simply
 * propagated to the caller (its return value stays readable via getReturn()).
 *
 * PHL does not enforce the Throwable parameter hint (interface/class hints are
 * not checked on call), so the Throwable validation — and its PHP TypeError —
 * are done here.
 */
static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	ph7_class_instance *pInj;
	ph7_class *pThrowable;
	VmFrame *pFrame;
	sxi32 rc;
	if( nArg < 2 ) return PH7_OK;
	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming
	 * the given type (class name for objects, "null"/"string"/... for scalars). */
	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);
	if( (apArg[1]->iFlags & MEMOBJ_OBJ) == 0
	 || (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[1]->x.pOther)->pClass, pThrowable)) ){
		char zCls[128];
		const char *zGiven = (apArg[1]->iFlags & MEMOBJ_OBJ)
			? VmFormatValueClassName(apArg[1], zCls, sizeof(zCls))
			: ((apArg[1]->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(apArg[1]));
		return PH7_VmThrowException(pCtx, "TypeError",
			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);
	}
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ) return PH7_OK;
	/* PHP forbids resuming/throwing into a generator that is currently executing. */
	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){
		return PH7_VmThrowException(pCtx, "Error",
			"Cannot resume an already running generator");
	}
	/* Hold a reference to the injected instance for the whole operation: the VM loop
	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind
	 * and later release it. Dropped on every return path below. */
	pInj = (ph7_class_instance *)apArg[1]->x.pOther;
	pInj->iRef++;
	/* A never-started generator runs to its first yield, then the exception is injected
	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if
	 * it ran to completion without yielding, drop through to the propagate path below. */
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }
		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }
	}
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		/* Inject at the suspended yield: the resume loop raises it in the body's own
		 * frame so the generator's try/catch can catch it and resume (path 2). */
		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */
		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);
		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it
		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the
		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */
		pGen->pCtx->pInjected = 0;
		PH7_ClassInstanceUnref(pInj);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
		/* Caught inside the generator and it resumed: return the next yielded value (or
		 * null if it then completed) — symmetric with Generator::send(). */
		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
			ph7_result_value(pCtx, &pGen->sYieldValue);
		}else{
			ph7_result_null(pCtx);
		}
		return PH7_OK;
	}
	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a
	 * yield): no suspend point to inject at. Propagate the real object to the throw()
	 * caller through the normal dispatch path (class/message/trace preserved); its
	 * terminal state — and thus getReturn() — is left intact. */
	pFrame = pCtx->pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pCtx->pVm, pInj);
	PH7_ClassInstanceUnref(pInj);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Generator::getReturn() — get the return value after the generator has finished.
 */
static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){
		return PH7_VmThrowException(pCtx, "Error",
			"Cannot get return value of a generator that hasn't returned");
	}
	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);
	return PH7_OK;
}
/*
 * Generator::__destruct() — clean up.
 */
static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	if( nArg < 1 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);
	if( pGen ){
		VmReleaseGenerator(pCtx->pVm, pGen);
		if( apArg[0]->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;
			SyString sAttrName;
			ph7_value *pAttr;
			SyStringInitFromBuf(&sAttrName, "__ctx", 5);
			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
			if( pAttr ){
				PH7_MemObjRelease(pAttr);
			}
		}
	}
	return PH7_OK;
}
/* ======================== End Generator Infrastructure ======================== */
/* ======================== End Fiber Infrastructure ======================== */
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
		VmTrackOutput(pVm, pString->nByte);
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
	VmTrackOutput(pVm, SyBlobLength(&sWorker));
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
	case PH7_OP_LOAD_FCC:
		                    zOp = "LOAD_FCC   "; break;
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
	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;
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
	case PH7_OP_DUP:        zOp = "DUP        "; break;
	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;
	case PH7_OP_USECONST:   zOp = "USECONST   "; break;
	case PH7_OP_SWAP:       zOp = "SWAP       "; break;
	case PH7_OP_YIELD:      zOp = "YIELD      "; break;
	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;
	case PH7_OP_NULLC:      zOp = "NULLC      "; break;
	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;
	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;
	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;
	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;
	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;
	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;
	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;
	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;
	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;
	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;
	case PH7_OP_INCR:       zOp = "INCR       "; break;
	case PH7_OP_DECR:       zOp = "DECR       "; break;
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
	case PH7_OP_MATCH:      zOp = "MATCH      "; break;
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
	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
/*
 * Verify that the contents of a variable can be called as a function.
 * [i.e: Whether it is callable or not].
 * Return TRUE if callable.FALSE otherwise.
 */
PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)
{
	int res = 0;
	if( pValue->iFlags & MEMOBJ_OBJ ){
		/* PHP semantics: an object is callable iff its class declares __invoke
		 * (inherited methods count). The CallInvoke flag is unused — it
		 * formerly invoked __invoke as a runtime predicate, which is not
		 * standard PHP behavior. */
		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;
		if( VmValueIsClosure(pVm,pValue) ){
			/* A Closure (incl. a first-class callable) is always callable. */
			res = 1;
		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){
			res = 1;
		}
		(void)CallInvoke;
	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		if( pMap->nEntry == 2 ){
			ph7_class *pClass;
			ph7_value *pV;
			/* Extract the target class */
			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);
			if( pV ){
				pClass = PH7_VmExtractClassFromValue(pVm,pV);
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
	if( nArg < 1 || (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ)) == 0 ){
		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and
		 * any __invoke object) callback; it is resolved/validated at shutdown. */
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
 * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)
 *   Get the class that declared the currently executing method.
 *   This is used for resolving the 'self::' constant.
 *
 * Parameters
 *   pVm: Target VM
 *
 * Return
 *   The declaring class of the current method, or NULL if:
 *   - Not executing within a class method
 *
 * Note
 *   This differs from PH7_VmPeekTopClass() which returns the runtime class
 *   from the 'self' stack. For self::, we need the class that declared the
 *   currently executing method, not the runtime class (use static:: for that).
 *   This is found by walking the call frames to locate the method's
 *   declaring class.
 */
PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)
{
	VmFrame *pFrame = pVm->pFrame;
	ph7_vm_func *pVmFunc;

	/* Skip exception frames to find the actual method frame */
	pFrame = VmSkipExceptionFrames(pFrame);

	/* Check if we're in a method context */
	if( pFrame->pParent ){
		pVmFunc = (ph7_vm_func *)pFrame->pUserData;
		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){
			/* Return the declaring class */
			return (ph7_class *)pVmFunc->pUserData;
		}
	}

	return 0;
}

/* Class/OOP builtin functions moved to vm_builtin_class.c */
/*
 * Call a class method where the name of the method is stored in the pMethod
 * parameter and the given arguments are stored in the apArg[] array.
 * Return SXRET_OK if the method was successfuly called.Any other
 * return value indicates failure.
 */
/*
 * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap
 * through to the synthetic CALL instruction.  Used by the NEW handler so
 * that constructor calls with named arguments reach the named-arg path
 * (with variadic string-key packing) rather than the positional path.
 */
static sxi32 VmCallClassMethodWithMap(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	ph7_class_method *pMethod,
	ph7_value *pResult,
	int nArg,
	ph7_value **apArg,
	VmCallArgMap *pMap
	)
{
	ph7_value *aStack;
	VmInstr aInstr[2];
	int iCursor;
	int i;
	sxi32 rc;
	aStack = VmNewOperandStack(&(*pVm),2+nArg);
	if( aStack == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"PH7 is running out of memory while invoking class method");
		return SXERR_MEM;
	}
	for( i = 0 ; i < nArg ; i++ ){
		PH7_MemObjLoad(apArg[i],&aStack[i]);
		aStack[i].nIdx = apArg[i]->nIdx;
	}
	iCursor = nArg + 1;
	if( pThis ){
		pThis->iRef++;
		aStack[i].x.pOther = pThis;
		aStack[i].iFlags = MEMOBJ_OBJ;
	}
	aStack[i].nIdx = SXU32_HIGH;
	i++;
	SyBlobReset(&aStack[i].sBlob);
	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));
	aStack[i].iFlags = MEMOBJ_STRING;
	aStack[i].nIdx = SXU32_HIGH;
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg;
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0);
	SyMemBackendFree(&pVm->sAllocator,aStack);
	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers
	 * can unwind instead of continuing past a method that raised. */
	return rc;
}
PH7_PRIVATE sxi32 PH7_VmCallClassMethod(
	ph7_vm *pVm,               /* Target VM */
	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/
	ph7_class_method *pMethod, /* Method name */
	ph7_value *pResult,        /* Store method return value here. NULL otherwise */
	int nArg,                  /* Total number of given arguments */
	ph7_value **apArg          /* Method arguments */
	)
{
	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);
}
/*
 * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,
 * returning its result. Returns the exec status so a method that throws
 * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach
 * opcode, which discards it.
 */
static sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)
{
	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);
	if( pMethod == 0 ){
		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */
	}
	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);
}
/*
 * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep
 * for each (key,value) pair. This is the reusable form of the Iterator protocol
 * that the foreach opcode drives inline; it is consumed by iterator_to_array /
 * iterator_count / iterator_apply and by Traversable spread.
 *
 * Returns:
 *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)
 *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)
 *   PH7_EXCEPTION       an iterator method or the step threw
 *   PH7_ABORT           an iterator method or the step requested a VM halt
 *
 * pKey/pValue handed to xStep are owned by the walk (released after the step
 * returns); xStep must copy what it needs.
 */
PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)
{
	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */
	ph7_class_instance *pAggregate = 0;
	ph7_class *pIteratorClass;
	sxi32 rc = SXRET_OK;
	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 || pObj->x.pOther == 0 ){
		return SXERR_NOTIMPLEMENTED;
	}
	pThis = (ph7_class_instance *)pObj->x.pOther;
	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);
	if( pIteratorClass == 0 ){
		return SXERR_NOTIMPLEMENTED;
	}
	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){
		pThis->iRef++; /* keep the iterator alive across the walk */
	}else{
		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */
		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);
		ph7_value sInner;
		int bOk = 0;
		if( pAggClass == 0 || !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){
			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */
		}
		PH7_MemObjInit(&(*pVm),&sInner);
		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_MemObjRelease(&sInner);
			return rc;
		}
		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){
			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;
			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){
				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */
				pThis = pIter; pThis->iRef++;           /* survive release of sInner */
				bOk = 1;
			}
		}
		PH7_MemObjRelease(&sInner);
		if( !bOk ){
			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */
			return SXERR_NOTIMPLEMENTED;
		}
	}
	/* Drive rewind / valid / current / key / step / next */
	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ goto done; }
	for(;;){
		ph7_value sValid,sValue,sKey;
		int isValid;
		PH7_MemObjInit(&(*pVm),&sValid);
		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }
		PH7_MemObjToBool(&sValid);
		isValid = (sValid.x.iVal != 0);
		PH7_MemObjRelease(&sValid);
		if( !isValid ){ rc = SXRET_OK; break; }
		PH7_MemObjInit(&(*pVm),&sValue);
		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }
		PH7_MemObjInit(&(*pVm),&sKey);
		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }
		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);
		PH7_MemObjRelease(&sValue);
		PH7_MemObjRelease(&sKey);
		if( rc != SXRET_OK ){
			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */
			goto done;
		}
		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ goto done; }
	}
done:
	PH7_ClassInstanceUnref(pThis);
	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }
	return rc;
}
/*
 * Dispatch a call to an object's __invoke magic method, forwarding arguments
 * and the return value. Used by the PH7_OP_CALL object-callable branch and by
 * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and
 * call_user_func_array($obj, [...]) all reach __invoke uniformly.
 *
 * Visibility is intentionally not checked: PHP allows private/protected
 * __invoke to be invoked via $obj() from any scope, and PHL's existing
 * is_callable / closure-invoke paths follow the same rule.
 *
 * pMap forwards the call-site VmCallArgMap so named-argument resolution and
 * strict_types coercion work for $obj(...) the same way they do for normal
 * function calls. Pass 0 from C-API call sites (call_user_func and friends),
 * which receive arguments positionally and don't carry a strict-types context.
 *
 * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.
 */
static sxi32 VmCallObjectInvoke(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	int nArg,
	ph7_value **apArg,
	ph7_value *pResult,
	VmCallArgMap *pMap
	)
{
	ph7_class_method *pMethod;
	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);
	if( pMethod == 0 ){
		if( pResult ){
			PH7_MemObjRelease(pResult);
		}
		return SXERR_INVALID;
	}
	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);
}
/*
 * Raise a catchable Error("Object of type X is not callable") when an object
 * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern
 * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as
 * throwing, dispatch via VmThrowException so the nearest try/catch can handle
 * it. Caller is responsible for the post-throw control flow (iExceptionJump
 * lookup or 'goto Exception').
 *
 * Returns the result of VmThrowException (SXRET_OK on handled exception,
 * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot
 * be bootstrapped — in which case an uncaught fatal has already been
 * reported.
 */
static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)
{
	ph7_class *pErrorClass;
	ph7_class_instance *pErrInst = 0;
	ph7_class_method *pCons;
	VmFrame *pThrowFrame;
	char zMsg[256];
	int nMsg;
	sxi32 rc;
	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
		"Object of type %.*s is not callable",
		(int)pThis->pClass->sName.nByte,
		pThis->pClass->sName.zString);
	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);
	if( pErrorClass ){
		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);
	}
	if( pErrInst == 0 ){
		/* Bootstrap failure: Error class is part of the built-in library and
		 * should always be available, so this branch is effectively unreachable.
		 * Degrade to an uncaught fatal report so the failure is at least
		 * visible to the user. */
		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);
		return SXERR_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		ph7_value sArg;
		ph7_value *apMsg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);
		PH7_MemObjInit(pVm,&sArg);
		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
		apMsg[0] = &sArg;
		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);
		PH7_MemObjRelease(&sArg);
	}
	/* Else: Error::__construct is part of the built-in library and should
	 * always be present; if it isn't, the thrown exception still surfaces
	 * with an empty getMessage() rather than crashing. */
	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);
	if( pThrowFrame ){
		pThrowFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pVm,pErrInst);
	PH7_ClassInstanceUnref(pErrInst);
	return rc;
}
/*
 * Call a user defined or foreign function where the name of the function
 * is stored in the pFunc parameter and the given arguments are stored
 * in the apArg[] array.
 * Return SXRET_OK if the function was successfuly called.Any other
 * return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	int nArg,          /* Total number of given arguments */
	ph7_value **apArg, /* Callback arguments */
	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */
	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */
	)
{
	ph7_value *aStack;
	VmInstr aInstr[2];
	int i;
	if( VmValueIsClosure(pVm,pFunc) ){
		/* A Closure object: unwrap to its underlying string/array callable and dispatch
		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the
		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */
		ph7_value sCallable;
		sxi32 rcClo;
		PH7_MemObjInit(pVm,&sCallable);
		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){
			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);
			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL
			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack
			 * OOM), the transient is still set — release its owned ref and clear it so it neither
			 * leaks nor poisons the next call's frame with a stale $this. */
			if( pVm->pClosureThis ){
				PH7_ClassInstanceUnref(pVm->pClosureThis);
				pVm->pClosureThis = 0;
				pVm->pClosureScope = 0;
			}
			PH7_MemObjRelease(&sCallable);
			return rcClo;
		}
		PH7_MemObjRelease(&sCallable);
	}
	if( pFunc->iFlags & MEMOBJ_OBJ ){
		/* Object callable: dispatch through __invoke when available (Closures were already
		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the
		 * positional callers (call_user_func / array_map / usort / C API) and carries the
		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */
		return VmCallObjectInvoke(&(*pVm),
			(ph7_class_instance *)pFunc->x.pOther,
			nArg,apArg,pResult,pArgMap);
	}
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
			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);
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
		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]`
		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */
		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);
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
	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */
	/* Emit the DONE instruction */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;   /* Extract function return value if available */
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	/* Execute the function body (if available) */
	{
		sxi32 rcExec;
		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0);
		/* Clean up the mess left behind */
		SyMemBackendFree(&pVm->sAllocator,aStack);
		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */
		return rcExec;
	}
}
/*
 * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the
 * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,
 * array_map, usort and the whole C API funnel here and pass arguments by
 * position, so they need no named-argument map.
 */
PH7_PRIVATE sxi32 PH7_VmCallUserFunction(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	int nArg,          /* Total number of given arguments */
	ph7_value **apArg, /* Callback arguments */
	ph7_value *pResult /* Store callback return value here. NULL otherwise */
	)
{
	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);
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
/* call_user_func and call_user_func_array moved to vm_builtin_class.c */
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
		/* Register the lowercase alias with its OWN value copy (not the same
		 * pValue) so the two entries don't share one object — otherwise freeing
		 * one on a later overwrite would dangle the other. */
		{
			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));
			if( pAlias ){
				PH7_MemObjInit(pCtx->pVm,pAlias);
				PH7_MemObjStore(apArg[1],pAlias);
				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);
			}
		}
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
/* Output buffering builtins moved to vm_builtin_ob.c */
/*
 * Section:
 *  Random numbers/string generators.
 * Status:
 *    Stable.
 */
/*
 * Generate a random 32-bit unsigned integer.
 * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator
 * implemented in src/sx/sxrand.c).
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
 * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator
 * implemented in src/sx/sxrand.c).
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
/*
 * Reject non-numeric values (array/object/resource and non-numeric strings)
 * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as
 * an int (PHP coerces float and numeric string silently).
 */
static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)
{
	if( ph7_value_is_array(pArg) || ph7_value_is_object(pArg)
		|| ph7_value_is_resource(pArg) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #%d (%s) must be of type int, %s given",
			zFunc,iArgPos,zParamName,
			ph7_type_name(pArg)
			);
	}
	if( ph7_value_is_string(pArg) ){
		int len;
		const char *zStr = ph7_value_to_string(pArg, &len);
		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type int, string given",
				zFunc,iArgPos,zParamName
				);
		}
	}
	return SXRET_OK;
}
/*
 * int random_int(int $min, int $max)
 *  Generate a cryptographically secure pseudo-random integer in [$min, $max].
 *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().
 *  Distribution is uniform via rejection sampling against the smallest
 *  power-of-two mask covering the range.
 */
static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iMin,iMax;
	sxu64 uRange,uMask,uResult;
	unsigned int nAttempt;
	int rc;
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"random_int() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");
	if( rc != SXRET_OK ){ return rc; }
	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");
	if( rc != SXRET_OK ){ return rc; }
	iMin = ph7_value_to_int64(apArg[0]);
	iMax = ph7_value_to_int64(apArg[1]);
	if( iMin > iMax ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"
			);
	}
	if( iMin == iMax ){
		ph7_result_int64(pCtx,iMin);
		return SXRET_OK;
	}
	uRange = (sxu64)iMax - (sxu64)iMin;
	uMask = uRange;
	uMask |= uMask >> 1;
	uMask |= uMask >> 2;
	uMask |= uMask >> 4;
	uMask |= uMask >> 8;
	uMask |= uMask >> 16;
	uMask |= uMask >> 32;
	uResult = 0;
	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){
		/* Always draw a full 8 bytes so endianness of the cast doesn't matter
		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian
		 * and the low-half mask would always read 0). */
		sxu64 uDraw;
		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"Random\\RandomException",
				"Cannot gather sufficient random data"
				);
		}
		uDraw &= uMask;
		if( uDraw <= uRange ){
			uResult = uDraw;
			break;
		}
	}
	if( nAttempt >= 50 ){
		return PH7_VmThrowException(pCtx,
			"Random\\RandomException",
			"Cannot gather sufficient random data"
			);
	}
	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));
	return SXRET_OK;
}
/*
 * string random_bytes(int $length)
 *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().
 *  Mirrors PHP 7.0+ random_bytes().
 */
static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iLen;
	unsigned char zStack[256];
	void *pBuf;
	int rc;
	int bHeap = 0;
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"random_bytes() expects exactly 1 argument, %d given",
			nArg
			);
	}
	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");
	if( rc != SXRET_OK ){ return rc; }
	iLen = ph7_value_to_int64(apArg[0]);
	if( iLen < 1 ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"random_bytes(): Argument #1 ($length) must be greater than 0"
			);
	}
	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,
	 * so we can't honor a length above 2 GiB. Reject early rather than
	 * silently truncating via the (sxu32) cast below. */
	if( iLen > 0x7FFFFFFF ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"random_bytes(): Argument #1 ($length) is too large"
			);
	}
	if( iLen <= (sxi64)sizeof(zStack) ){
		pBuf = zStack;
	}else{
		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);
		if( pBuf == 0 ){
			return PH7_VmThrowException(pCtx,
				"Exception",
				"random_bytes(): Failed to allocate %qd bytes",
				iLen
				);
		}
		bHeap = 1;
	}
	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){
		if( bHeap ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);
		}
		return PH7_VmThrowException(pCtx,
			"Random\\RandomException",
			"Cannot gather sufficient random data"
			);
	}
	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);
	if( bHeap ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);
	}
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
			VmTrackOutput(pVm, (sxu32)nDataLen);
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
			VmTrackOutput(pVm, (sxu32)nDataLen);
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
	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing
	 * immediately; the abort unwinds enclosing frames and execution units.
	 */
	pCtx->pVm->bHaltRequested = 1;
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
			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —
			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and
			 * by anyone passing a bool literal (rare, harmless). */
			if( (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_BOOL)) == 0 ){
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
				/* Perform the insertion (pObj may point into pVm->aMemObj; the
				 * inserter snapshots the source before reserving, so the pool may
				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */
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
 * var_export() — PHP-exact evaluable representation of a value.
 *
 * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits
 * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),
 * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky
 * (2-space array entries; 3-space object property lines; a composite value always
 * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.
 */
/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an
 * object currently on the var_export recursion stack, for cycle detection. */
#define VM_INSTANCE_DUMPING 0x002
typedef struct VmExportCtx VmExportCtx;
struct VmExportCtx
{
	SyBlob *pOut;
	int nIndent;  /* indentation of the container emitting this entry */
	int depth;    /* recursion guard */
};
static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);
/* Append nIndent spaces. */
static void VmExportIndent(SyBlob *pOut, int nIndent)
{
	int i;
	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }
}
/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs
 * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits
 * it out as ' . "\0" . ' — match that. */
static void VmExportQuoted(SyBlob *pOut, const char *z, int n)
{
	int i, run = 0;
	SyBlobAppend(pOut,"'",1);
	for( i = 0; i < n; i++ ){
		char c = z[i];
		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */
		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }
		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }
		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }
		run = i+1;
	}
	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }
	SyBlobAppend(pOut,"'",1);
}
/* True if the array/object is already on the var_export recursion stack. */
static int VmExportIsCycle(ph7_value *pVal)
{
	if( ph7_value_is_array(pVal) ){
		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;
	}
	if( ph7_value_is_object(pVal) ){
		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;
	}
	return 0;
}
/* Emit " => " then the value: a scalar inline, a composite on its own line at
 * containerIndent+2. A circular reference renders inline as NULL (like PHP). */
static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)
{
	SyBlobAppend(pOut," => ",4);
	if( VmExportIsCycle(pVal) ){
		SyBlobAppend(pOut,"NULL",4);
	}else if( ph7_value_is_array(pVal) || ph7_value_is_object(pVal) ){
		SyBlobAppend(pOut,"\n",1);
		VmExportIndent(pOut,nContainerIndent+2);
		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);
	}else{
		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);
	}
	SyBlobAppend(pOut,",\n",2);
}
/* Array walker: "<indent+2>key => value,\n" for each entry. */
static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	VmExportCtx *pC = (VmExportCtx *)pUserData;
	VmExportIndent(pC->pOut,pC->nIndent+2);
	if( ph7_value_is_string(pKey) ){
		int n;
		const char *z = ph7_value_to_string(pKey,&n);
		VmExportQuoted(pC->pOut,z,n);
	}else{
		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));
	}
	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);
	return PH7_OK;
}
static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)
{
	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */
	if( ph7_value_is_null(pVal) ){
		SyBlobAppend(pOut,"NULL",4);
	}else if( ph7_value_is_bool(pVal) ){
		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }
		else { SyBlobAppend(pOut,"false",5); }
	}else if( ph7_value_is_float(pVal) ){
		/* float before int: ph7_value_is_int is lenient (true for integer reals). */
		sxu32 before = SyBlobLength(pOut), i, after;
		const char *z;
		int plain = 1;
		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));
		z = (const char *)SyBlobData(pOut);
		after = SyBlobLength(pOut);
		for( i = before; i < after; i++ ){
			if( !((z[i]>='0'&&z[i]<='9')||z[i]=='-') ){ plain = 0; break; }
		}
		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */
	}else if( ph7_value_is_int(pVal) ){
		SyBlobFormat(pOut,"%qd",ph7_value_to_int64(pVal));
	}else if( ph7_value_is_string(pVal) ){
		int n;
		const char *z = ph7_value_to_string(pVal,&n);
		VmExportQuoted(pOut,z,n);
	}else if( ph7_value_is_array(pVal) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
		if( pMap->iFlags & HASHMAP_DUMPING ){
			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */
		}else{
			VmExportCtx ctx;
			pMap->iFlags |= HASHMAP_DUMPING;
			SyBlobAppend(pOut,"array (\n",8);
			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;
			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);
			VmExportIndent(pOut,nIndent);
			SyBlobAppend(pOut,")",1);
			pMap->iFlags &= ~HASHMAP_DUMPING;
		}
	}else if( ph7_value_is_object(pVal) ){
		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;
		if( pThis->iFlags & VM_INSTANCE_DUMPING ){
			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */
		}else{
			SyString *pClassName = &pThis->pClass->sName;
			SyHashEntry *pEntry;
			pThis->iFlags |= VM_INSTANCE_DUMPING;
			SyBlobAppend(pOut,"\\",1);
			SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);
			SyBlobAppend(pOut,"::__set_state(array(\n",21);
			SyHashResetLoopCursor(&pThis->hAttr);
			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
				SyString *pAName = &pVmAttr->pAttr->sName;
				ph7_value *pAttrVal;
				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){ continue; }
				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */
				VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);
				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }
				else { SyBlobAppend(pOut," => NULL,\n",10); }
			}
			VmExportIndent(pOut,nIndent);
			SyBlobAppend(pOut,"))",2);
			pThis->iFlags &= ~VM_INSTANCE_DUMPING;
		}
	}else{
		/* resource / other -> PHP emits NULL (with a warning we omit) */
		SyBlobAppend(pOut,"NULL",4);
	}
}
/*
 * string/null var_export(expression,[bool $return = FALSE])
 *  PHP-exact evaluable representation (see VmExportValue).
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
	/* Generate the PHP-exact evaluable representation */
	VmExportValue(&sDump,apArg[0],0,0);
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
 *   ASSERT_WARNING         Deprecated, accepted as no-op
 *   ASSERT_BAIL            Terminate execution on failed assertions
 *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)
 *   ASSERT_CALLBACK        Callback to call on failed assertions
 *   ASSERT_EXCEPTION       Always enabled in PHP 8
 * $value
 *   An optional new value for the option.
 * Return
 *  Old setting on success or FALSE on failure.
 */
static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int iOption;
	/* PHP 8: ArgumentCountError if no arguments */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"assert_options() expects at least 1 argument, 0 given"
			);
	}
	/* PHP 8: TypeError for non-scalar option types */
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_object(apArg[0])
		|| ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"assert_options(): Argument #1 ($option) must be of type int, %s given",
			ph7_value_is_array(apArg[0]) ? "array" :
			ph7_value_is_object(apArg[0]) ? "object" : "resource"
			);
	}
	iOption = ph7_value_to_int(apArg[0]);
	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,
	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,
	 * ASSERT_QUIET_EVAL=6 (deprecated) */
	switch( iOption ){
	case 1: /* ASSERT_ACTIVE */
		/* Return old value: 1 if active (not disabled), 0 if disabled */
		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);
		if( nArg > 1 ){
			if( ph7_value_to_bool(apArg[1]) ){
				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;
			}else{
				pVm->iAssertFlags |= PH7_ASSERT_DISABLE;
			}
		}
		break;
	case 2: /* ASSERT_CALLBACK */
		/* Return old callback or null */
		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){
			ph7_result_value(pCtx, &pVm->sAssertCallback);
		}else{
			ph7_result_null(pCtx);
		}
		if( nArg > 1 ){
			if( ph7_value_is_callable(apArg[1]) ){
				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);
				pVm->iAssertFlags |= PH7_ASSERT_CALLBACK;
			}else{
				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;
			}
		}
		break;
	case 3: /* ASSERT_BAIL */
		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);
		if( nArg > 1 ){
			if( ph7_value_to_bool(apArg[1]) ){
				pVm->iAssertFlags |= PH7_ASSERT_BAIL;
			}else{
				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;
			}
		}
		break;
	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */
		ph7_result_int(pCtx, 0);
		break;
	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */
		ph7_result_int(pCtx, 1);
		break;
	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */
		ph7_result_int(pCtx, 0);
		break;
	default:
		/* PHP 8: ValueError for invalid option */
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"
			);
	}
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
	int iFlags,iResult;
	const char *zDesc;
	/* PHP 8: ArgumentCountError if no arguments */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"assert() expects at least 1 argument, 0 given"
			);
	}
	iFlags = pVm->iAssertFlags;
	if( iFlags & PH7_ASSERT_DISABLE ){
		/* Assertion is disabled,return TRUE (PHP 8 behavior) */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* PHP 8: No string evaluation.  All values are cast to boolean. */
	iResult = ph7_value_to_bool(apArg[0]);
	if( !iResult ){
		/* Assertion failed */
		/* Extract optional description */
		zDesc = 0;
		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
			zDesc = ph7_value_to_string(apArg[1],0);
		}
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
			apCbArg[2] = apArg[0];
			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);
			/* Clean-up the mess left behind */
			PH7_MemObjRelease(&sFile);
			PH7_MemObjRelease(&sLine);
		}
		if( iFlags & PH7_ASSERT_BAIL ){
			/* Abort VM execution immediately */
			return PH7_ABORT;
		}
		/* PHP 8: throw AssertionError by default */
		if( zDesc && zDesc[0] != '\0' ){
			return PH7_VmThrowException(pCtx,
				"AssertionError",
				"%s",
				zDesc
				);
		}else{
			return PH7_VmThrowException(pCtx,
				"AssertionError",
				"assert(false)"
				);
		}
	}
	/* Assertion passed */
	ph7_result_bool(pCtx,1);
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
		pFrame = VmSkipExceptionFrames(pFrame);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);
	pVm->nExceptDepth--;
	if( rc != SXRET_OK ){
		const char *zFuncName;
		int nFuncLen;
		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);
		/* Both report entry points below stamp iExitStatus = 255 themselves. */
		if( pThis ){
			/* Walk the $previous chain: deepest is "Uncaught", each outer one
			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and
			 * renders byte-identically to the historical single-entry report. */
			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);
		}else{
			/* No instance (internal report path) — default-class single entry. */
			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);
		}
		/* Tell the upper layer to stop VM execution immediately  */
		rc = SXERR_ABORT;
	}
	PH7_MemObjRelease(&sArg);
	return rc;
}
/*
 * Throw a user exception.
 *
 * Exception dispatch follows this sequence:
 *
 * 1. Walk the exception stack (pVm->aException) from top to find a
 *    try/catch whose catch block matches the exception class.
 *
 * 2. If NO catch matches:
 *    a. Run finally (if present) for the current try block.
 *    b. If outer handlers exist on the stack, re-throw recursively.
 *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)
 *       whose outer handlers were temporarily hidden, DEFER the
 *       exception in pVm->pPendingException instead of reporting it
 *       uncaught. It will be re-thrown after finally runs (step 3d).
 *    d. Otherwise, report as truly uncaught.
 *
 * 3. If a catch DOES match:
 *    a. Temporarily HIDE all outer exception handlers by saving the
 *       aException stack and resetting it. This prevents a re-throw
 *       inside the catch body from immediately propagating past our
 *       finally block.
 *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH
 *       frame. If the catch body throws, dispatch recurses but finds
 *       no handlers (they're hidden), so the exception is deferred
 *       in pPendingException (step 2c).
 *    c. Restore outer handlers from the saved copy.
 *    d. Run finally (if present).
 *    e. If pPendingException is set (catch re-threw), re-throw it now
 *       that handlers are restored and finally has run.
 */
/*
 * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.
 * Pops each enclosing try's handler (this function's inline trys, innermost first) off
 * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the
 * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the
 * finally runs). A try without a finally is torn down (handler + transparent frame) and
 * the walk continues. Returns 0 when no more of this function's inline trys remain (the
 * action is terminal: materialize the return / take the break jump). A try WITH a finally
 * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.
 * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).
 */
static int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)
{
	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){
		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);
		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];
		if( !pT->iInlined || pT->pOwnerInstr != (void *)aInstr ){
			break; /* reached an outer exec's / legacy handler */
		}
		(void)SySetPop(&pVm->aException);
		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */
		if( pT->iHasFinally ){
			*pPc = pT->iFinallyPc;
			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */
			return 1;
		}
		/* No finally: tear the try's transparent frame down now. */
		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
			VmLeaveFrame(&(*pVm));
		}
		VmExcRelease(&(*pVm),pT);
	}
	return 0;
}
/*
 * ROOT C: classify a throw against an INLINE try (generator body) and set up a
 * pc-redirect for the throw site — never runs bytecode itself. pException has been
 * popped off aException by the caller; pCatch is the matching catch block or 0.
 *
 *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch
 *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,
 *    and redirect to the catch body (iHandlerPc).
 *  - no catch but finally: queue a RETHROW action and redirect to the finally
 *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.
 *  - no catch, no finally: leave this try's transparent frame and propagate to the
 *    next handler (inline or legacy) by re-entering VmThrowException.
 * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).
 */
/*
 * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope
 * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented
 * onto pOwner, exactly the catch body's mechanism (see the re-parent note in
 * VmThrowException's catch path). Before this, a finally reached by a throw
 * from a NESTED call ran against the throw-site frame: it read the wrong
 * function's variables and a `return` inside it parked on the wrong body
 * (the finally_return_cross_frame twin). Same-frame execution (the common
 * case) is unchanged: no wrapper.
 */
static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)
{
	VmFrame *pWrap = 0;
	VmFrame *pThrowSite;
	sxi32 rc;
	if( pOwner == 0 || pOwner == VmSkipExceptionFrames(pVm->pFrame) ){
		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);
	}
	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){
		/* OOM: degrade to in-place execution rather than losing the finally. */
		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);
	}
	pThrowSite = pWrap->pParent;
	pWrap->pParent = pOwner;
	pWrap->iFlags |= VM_FRAME_EXCEPTION;
	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);
	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then
	 * restore the real throw site so the unwind continues normally. Guarded:
	 * a finally that suspends/aborts mid-mini-program can leave the frame
	 * chain unbalanced — never pop somebody else's frame. */
	if( pVm->pFrame == pWrap ){
		VmLeaveFrame(&(*pVm));
	}
	pVm->pFrame = pThrowSite;
	return rc;
}
/*
 * VmThrowInline -> VmThrowException private protocol: "no catch/finally in
 * this inline try — keep unwinding outward" (the caller loops back to its
 * Rethrow label). Aliased so the generic retry code's other contract (the
 * sxmem xMemError release-and-retry callback) is not confused with this one.
 */
#define VM_THROW_KEEP_UNWINDING SXERR_RETRY
static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,
	ph7_exception *pException, ph7_exception_block *pCatch)
{
	if( pCatch ){
		pException->iInCatch = 1;
		SySetPut(&pVm->aException,(const void *)&pException);
		if( pThis ){ pThis->iRef++; }
		pException->pInflight = pThis;
		pVm->pInlineInstr = pException->pOwnerInstr;
		pVm->iInlinePc = pCatch->iHandlerPc;
		pVm->iInlineDrain = pException->iStackDepth;
		return SXRET_OK;
	}
	if( pException->iHasFinally ){
		VmFinallyAction sAct;
		SyZero(&sAct,sizeof(sAct));
		sAct.eKind = PH7_FA_RETHROW;
		if( pThis ){ pThis->iRef++; }
		sAct.pExc = pThis;
		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
		pVm->pInlineInstr = pException->pOwnerInstr;
		pVm->iInlinePc = pException->iFinallyPc;
		pVm->iInlineDrain = pException->iStackDepth;
		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */
		return SXRET_OK;
	}
	/* No catch, no finally: drop this try's frame and continue unwinding —
	 * flat native stack instead of mutual recursion. */
	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
		VmLeaveFrame(&(*pVm));
	}
	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */
	return VM_THROW_KEEP_UNWINDING;
}
static sxi32 VmThrowException(
	ph7_vm *pVm,              /* Target VM */
	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */
	)
{
	ph7_exception_block *pCatch; /* Catch block to execute */
	ph7_exception **apException;
	ph7_exception *pException;
Rethrow:
	/* Unwinding to the next outer handler loops back here instead of the old
	 * self tail-call: a throw from N frames deep runs N finallys at constant
	 * native depth (the ASan deep-tier stress overflowed the C stack on the
	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,
	 * so the throw path must be too). */
	/* An in-flight throw abandons any pending null-coalesce-assign store:
	 * disarm so the RHS-evaluation throw can't leave the slot live for a
	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */
	VmCoalesceDisarm(pVm);
	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight
	 * exception (pInflightException) and a NEW exception thrown by that finally
	 * is leaving it — i.e. the exception stack has unwound to/below the depth it
	 * had when the finally started (nInflightExcBase), so no finally-local catch
	 * will handle it — link the in-flight one as its $previous. A finally
	 * exception caught locally (a try/catch inside the finally) sits ABOVE the
	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op
	 * if pThis already carries a previous, so re-entry across propagation is safe. */
	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException
	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){
		VmExceptionLinkPrevious(pThis,pVm->pInflightException);
	}
	/* A throw supersedes a pending catch/finally `return` ONLY when it actually
	 * unwinds past that return's body frame. We do NOT clear anything here: each
	 * body's pending return lives on its own frame and is discarded at that body's
	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught
	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body
	 * that owns the pending return, so it must leave that return intact. */
	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):
	 * the previous catch's landing is no longer where control should resume. Cleared
	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish
	 * records last (on its SXRET_OK return below) and therefore owns the resume. */
	pVm->pResumeFrame = 0;
	/* Point to the stack of loaded exceptions */
	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);
	pException = 0;
	pCatch = 0;
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception_block *aCatch;
		ph7_class *pClass;
		SyString *aNames;
		sxu32 nNames;
		int matched;
		sxu32 j,k;
		/* Locate the appropriate block to execute */
		pException = apException[SySetUsed(&pVm->aException) - 1];
		(void)SySetPop(&pVm->aException);
		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);
		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does
		 * NOT re-match its own catches — a throw inside the catch runs the finally then
		 * propagates. Skip the class scan so pCatch stays 0. */
		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){
			/* Iterate over all class names in this catch block (multi-catch support) */
			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);
			nNames = SySetUsed(&aCatch[j].aClasses);
			matched = 0;
			for( k = 0 ; k < nNames ; ++k ){
				/* Extract the target class or interface (iLoadable=FALSE so
				 * interfaces like Throwable are resolvable as catch targets).
				 * Traits are never instance-compatible, so skip them explicitly. */
				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);
				if( pClass == 0 || (pClass->iFlags & PH7_CLASS_TRAIT) ){
					/* No such class, or trait — cannot match */
					continue;
				}
				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){
					matched = 1;
					break;
				}
			}
			if( matched ){
				/* Catch block found,break immediately */
				pCatch = &aCatch[j];
				break;
			}
		}
	}
	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException
	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the
	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */
	if( pException && pException->iInlined ){
		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);
		if( rcInline == VM_THROW_KEEP_UNWINDING ){
			/* No catch/finally in that inline try: keep unwinding outward. */
			goto Rethrow;
		}
		return rcInline;
	}
	/* Execute the cached block if available */
	if( pCatch == 0 ){
		sxi32 rc;
		/* No catch matched. Execute finally, then propagate to outer try/catch. */
		if( pException && pException->iHasFinally ){
			sxu32 nExcBefore = SySetUsed(&pVm->aException);
			ph7_class_instance *pSaveInflight = pVm->pInflightException;
			sxu32 nSaveBase = pVm->nInflightExcBase;
			pException->iFinallyDone = 1;
			/* Mark pThis in-flight (base = current exception-stack depth) so a throw
			 * from the finally that leaves it chains pThis as $previous; restore after. */
			pVm->pInflightException = pThis;
			pVm->nInflightExcBase = nExcBefore;
			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own
			 * variables; a `return` parks on the owning body), like the catch. */
			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);
			pVm->pInflightException = pSaveInflight;
			pVm->nInflightExcBase = nSaveBase;
			if( rc == SXERR_ABORT ){
				VmExcRelease(&(*pVm),pException);
				return SXERR_ABORT;
			}
			/* A `return` inside the finally swallows the in-flight exception (PHP
			 * semantics). The finally stored it on the body frame it returns from
			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the
			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:
			 * clear VM_FRAME_THROW (this body now returns normally, so its caller
			 * takes the value instead of unwinding) and resume in place.
			 * Cross-frame: record the OWNER's landing pad as the resume target —
			 * the same transport an in-place catch uses — and unwind as an
			 * exception; the owner's activation consumes the resume
			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and
			 * its bHasRet tail materializes the return. */
			{
				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);
				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;
				if( pOwnerFrame->bHasRet ){
					if( pOwnerFrame == pThrowFrame ){
						pThrowFrame->iFlags &= ~VM_FRAME_THROW;
						VmExcRelease(&(*pVm),pException);
						return SXRET_OK;
					}
					pVm->pResumeFrame = pOwnerFrame;
					pVm->iResumePc = pException->iLandingPc;
					pVm->pResumeInstr = pException->pOwnerInstr;
					pVm->iResumeStackDepth = pException->iStackDepth;
					VmExcRelease(&(*pVm),pException);
					return PH7_EXCEPTION;
				}
			}
			/* The finally threw an exception that superseded pThis — it either
			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler
			 * (which consumed an entry from the exception stack). Either way the
			 * original pThis is discarded; unwind with the finally's exception (the
			 * OP_THROW caller resumes at the catching frame or propagates). */
			if( rc == PH7_EXCEPTION || SySetUsed(&pVm->aException) < nExcBefore ){
				VmExcRelease(&(*pVm),pException);
				return PH7_EXCEPTION;
			}
		}
		/* Check if there is an outer exception handler on the stack */
		if( SySetUsed(&pVm->aException) > 0 ){
			/* Re-throw to the outer handler — flat loop (see Rethrow), one
			 * iteration per unwound level instead of one native frame. */
			VmExcRelease(&(*pVm),pException);
			goto Rethrow;
		}
		/* No outer handler. If the handlers were temporarily hidden
		 * (catch body re-throw with finally pending), defer the
		 * exception instead of reporting it uncaught.
		 */
		if( pVm->pPendingException == 0 && pThis ){
			/* Check if we are inside a catch execution with hidden handlers
			 * by looking for a catch frame on the stack.
			 */
			VmFrame *pF = pVm->pFrame;
			int inCatch = 0;
			while( pF ){
				if( pF->iFlags & VM_FRAME_CATCH ){
					inCatch = 1;
					break;
				}
				pF = pF->pParent;
			}
			if( inCatch ){
				/* Defer — will be re-thrown after finally runs */
				pThis->iRef++;
				pVm->pPendingException = pThis;
				VmExcRelease(&(*pVm),pException);
				return SXRET_OK;
			}
		}
		/* Truly uncaught */
		rc = VmUncaughtException(&(*pVm),pThis);
		if( rc == SXRET_OK && pException ){
			VmFrame *pFrame = pVm->pFrame;
			pFrame = VmSkipExceptionFrames(pFrame);
			if( pException->pFrame == pFrame ){
				pFrame->iFlags &= ~VM_FRAME_THROW;
			}
		}
		VmExcRelease(&(*pVm),pException);
		return rc;
	}else{
		VmFrame *pFrame = pVm->pFrame;
		ph7_exception **apSaved = 0;
		sxu32 nSavedCount;
		sxi32 rc;
		/* Snapshot the resume target BEFORE running the catch/finally mini-programs
		 * (which may push/pop nested exceptions): the body frame that owns this
		 * matching try, and its post-try landing pad. Recorded onto the VM only on
		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at
		 * THIS catching body rather than the lexically-nearest try (ROOT B). */
		VmFrame *pCatchBody = pException->pFrame;
		sxu32 iCatchPc = pException->iLandingPc;
		void *pCatchInstr = pException->pOwnerInstr;
		pFrame = VmSkipExceptionFrames(pFrame);
		if( pException->pFrame == pFrame ){
			pFrame->iFlags &= ~VM_FRAME_THROW;
		}
		/* Temporarily hide outer exception handlers so that if the catch
		 * body re-throws, the exception does not immediately propagate past
		 * our finally block. We save the stack contents and restore after.
		 */
		nSavedCount = SySetUsed(&pVm->aException);
		if( nSavedCount > 0 ){
			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,
				nSavedCount * sizeof(ph7_exception *));
			if( apSaved ){
				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,
					nSavedCount * sizeof(ph7_exception *));
				SySetReset(&pVm->aException);
			}
		}
		/* Create the catch frame (made transparent below) */
		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);
		if( rc == SXRET_OK ){
			ph7_value *pObj;
			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a
			 * throw raised in a nested call, is the deeper throw-site frame, not the
			 * body that declared the try. Re-parent onto the try-owning body
			 * (pCatchBody = pException->pFrame) and remember the throw site to restore
			 * after. This makes the transparent wrapper resolve the catch's variable
			 * scope AND its `return` target against the body that owns the try, so a
			 * `return` parks on that body — matching the recorded resume target. Before
			 * this, an in-place catch for a deep throw parked its return on the callee's
			 * frame, which the unwind then discarded (ROOT B, face c). */
			VmFrame *pThrowSite = pFrame->pParent;
			/* A NULL pCatchBody (owner invalidated at park — its frame died with
			 * a lossy deep suspend) keeps the natural parent: the catch then runs
			 * against the live current scope rather than a freed frame. */
			if( pCatchBody ){
				pFrame->pParent = pCatchBody;
			}
			/* Transparent wrapper: the catch body shares the enclosing variable
			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames
			 * resolve variables — and bind $e — against the real enclosing frame, so
			 * outer locals, $this and a closure held in a variable are all visible
			 * inside the catch (and $e/any var written there persists afterwards).
			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump
			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)
			 * are unaffected. Must be set BEFORE binding $e below. */
			pFrame->iFlags |= VM_FRAME_CATCH | VM_FRAME_EXCEPTION;
			pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);
			if( pObj ){
				/* The catch variable now resolves in the (shared) enclosing frame,
				 * so it may already hold a value from a prior catch or assignment.
				 * Pin the new instance, then release the slot's prior contents
				 * (runs its __destruct / frees the old value) before rebinding —
				 * iRef++ first keeps a re-thrown same exception alive across the
				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */
				pThis->iRef++;
				PH7_MemObjRelease(pObj);
				pObj->x.pOther = pThis;
				MemObjSetType(pObj,MEMOBJ_OBJ);
			}
			/* Execute the catch block */
			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);
			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then
			 * restore the real throw-site frame so the unwind continues normally.
			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body
			 * that suspends/aborts mid-mini-program can leave the frame chain
			 * unbalanced — never pop somebody else's frame. */
			if( pVm->pFrame == pFrame ){
				VmLeaveFrame(&(*pVm));
			}
			pVm->pFrame = pThrowSite;
		}
		/* Restore the outer exception handlers */
		if( apSaved ){
			sxu32 k;
			/* Entries pushed during catch execution (nested try blocks inside
			 * the catch body) are normally already consumed; on an abnormal
			 * mini-program exit (e.g. a suspend escaping the catch) they can
			 * linger — release those activations before discarding the set. */
			VmExcReleaseAll(&(*pVm),&pVm->aException);
			SySetReset(&pVm->aException);
			for(k = 0; k < nSavedCount; k++){
				SySetPut(&pVm->aException,(const void *)&apSaved[k]);
			}
			SyMemBackendFree(&pVm->sAllocator,apSaved);
		}
		/* Execute the finally block after catch */
		if( pException->iHasFinally ){
			sxi32 rcf;
			/* Snapshot, before the finally runs: the body frame this try returns
			 * from, its pending-return write generation (set if the catch above
			 * returned), and the exception-stack depth. After the finally we use
			 * these to decide whether the finally's throw superseded THIS try's
			 * catch-return. */
			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep
			 * throw the current frame is the THROW SITE, and both the catch's
			 * return (parked via the re-parented wrapper) and the finally's
			 * supersede decision belong to the owner, not the thrower. */
			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);
			sxu32 nGenBefore = pBody->nRetGen;
			sxu32 nExcBefore = SySetUsed(&pVm->aException);
			/* The exception in flight while this finally runs is the catch body's
			 * re-throw (deferred in pPendingException), if any — NOT the original
			 * pThis, which the catch already handled. A finally throw chains to that
			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;
			 * a normally-handled catch leaves nothing in flight => B->previous null). */
			ph7_class_instance *pSaveInflight = pVm->pInflightException;
			sxu32 nSaveBase = pVm->nInflightExcBase;
			pException->iFinallyDone = 1;
			pVm->pInflightException = pVm->pPendingException;
			pVm->nInflightExcBase = nExcBefore;
			/* Stage 2b: the finally runs in the owner's scope, like the catch. */
			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);
			pVm->pInflightException = pSaveInflight;
			pVm->nInflightExcBase = nSaveBase;
			if( rcf == SXERR_ABORT ){
				VmExcRelease(&(*pVm),pException);
				return SXERR_ABORT;
			}
			/* Did the finally throw an exception that escaped THIS try? Two shapes:
			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by
			 * a handler that lived BELOW this try (the exception stack shrank). In
			 * either case that exception supersedes this try's catch-return — but
			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch
			 * (same body frame) ran during the finally and OVERWROTE the slot with
			 * its own return, nRetGen advanced and that return must survive. */
			if( (rcf == PH7_EXCEPTION || SySetUsed(&pVm->aException) < nExcBefore)
			 && pBody->bHasRet && pBody->nRetGen == nGenBefore ){
				VmClearFrameReturn(pBody);
			}
			if( rcf == PH7_EXCEPTION ){
				/* The finally's exception propagated past this try; drop any deferred
				 * re-throw and signal the OP_THROW site to unwind THIS function so it
				 * reaches the frame that caught the finally's throw. */
				if( pVm->pPendingException ){
					PH7_ClassInstanceUnref(pVm->pPendingException);
					pVm->pPendingException = 0;
				}
				VmExcRelease(&(*pVm),pException);
				return PH7_EXCEPTION;
			}
		}
		if( rc == SXERR_ABORT ){
			VmExcRelease(&(*pVm),pException);
			return SXERR_ABORT;
		}
		/* If the catch body re-threw, the exception was deferred in
		 * pPendingException (because outer handlers were hidden).
		 * Now that finally has run and handlers are restored, re-throw —
		 * unless the catch/finally issued a `return` (parked on this body frame,
		 * the catch frame having been left above), which swallows the in-flight
		 * exception (PHP semantics).
		 */
		if( pVm->pPendingException ){
			/* Stage 2b: the swallow decision reads the OWNER's parked return. */
			if( !(pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame))->bHasRet ){
				ph7_class_instance *pReThrow = pVm->pPendingException;
				pVm->pPendingException = 0;
				VmExcRelease(&(*pVm),pException);
				/* Continue unwinding with the re-thrown exception (flat loop) */
				pThis = pReThrow;
				goto Rethrow;
			}
			/* Swallowed by the catch/finally's return: drop the deferred exception. */
			PH7_ClassInstanceUnref(pVm->pPendingException);
			pVm->pPendingException = 0;
		}
		/* The catch (and finally) ran in place and control did NOT unwind past this
		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the
		 * throwing site — which may be several frames below — resumes at this catching
		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */
		pVm->pResumeFrame = pCatchBody;
		pVm->iResumePc = iCatchPc;
		pVm->pResumeInstr = pCatchInstr;
		pVm->iResumeStackDepth = pException->iStackDepth;
		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry
		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at
		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */
		VmExcRelease(&(*pVm),pException);
	}
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
 * string phpversion([ string $extension ])
 *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).
 * Parameters
 *  $extension (optional): an extension name. PHL has no extension registry, so any
 *  argument yields NULL (PHP returns FALSE for an unknown extension).
 * Return
 *  The PHP-compat version string, or NULL when called with an extension argument.
 */
static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(apArg); /* cc warning */
	if( nArg > 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);
	return PH7_OK;
}
/*
 * string php_sapi_name(void)
 *  Returns the type of interface (SAPI) PHL is running under.
 * Parameters
 *  None
 * Return
 *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.
 */
static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";
	SXUNUSED(nArg);
	SXUNUSED(apArg); /* cc warning */
	ph7_result_string(pCtx,zSapi,-1);
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
	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);
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
			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);
		}else if( c == 'P' || c == 'p' ){
			/* Import $_POST variables */
			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);
		}else if( c == 'c' || c == 'C' ){
			/* Import $_COOKIE variables */
			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);
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
	SyBlob sSavedNs;
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
	/* Save and reset VM namespace state for the new compilation unit.
	 * Each included file has its own namespace scope; after execution,
	 * the caller's namespace is restored. */
	SyBlobInit(&sSavedNs,&pVm->sAllocator);
	SyBlobDup(&pVm->sNamespace,&sSavedNs);
	if( bTrueReturn ){
		/* Include/require: start in a fresh (global) namespace scope. */
		SyBlobReset(&pVm->sNamespace);
	}
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
		/* Mount any newly defined classes */
		SyHashEntry *pEntry;
		ph7_class *pClass;
		ph7_value sResult; /* Return value */
		sxi32 rc;
		SyHashResetLoopCursor(&pVm->hClass);
		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){
			pClass = (ph7_class *)pEntry->pUserData;
			/* Only mount classes that haven't been mounted yet */
			if( !pClass->bMounted ){
				rc = VmMountUserClass(pVm,pClass);
				if( rc != SXRET_OK ){
					/* Mount failure (likely memory error) */
					if( pCtx ){
						ph7_result_bool(pCtx,0);
					}
					goto Cleanup;
				}
			}
		}
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
		/* Execute the compiled chunk. eval()/include/require recurse in C here
		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by
		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the
		 * native-nesting fatal instead of overflowing the C stack. The PHP
		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5). */
		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);
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
	/* Restore caller's namespace state */
	SyBlobReset(&pVm->sNamespace);
	SyBlobDup(&sSavedNs,&pVm->sNamespace);
	SyBlobRelease(&sSavedNs);
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
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the evaluated chunk: cascade the halt */
		return PH7_ABORT;
	}
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
	SXUNUSED(pCtx); /* cc warning */
	SXUNUSED(pPath);
	SXUNUSED(IncludeOnce);
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
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
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
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
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
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
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
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/* Getopt builtins moved to vm_builtin_getopt.c */
/* JSON encoding/decoding routines moved to vm_json.c */
/* XML processing and UTF-8 routines moved to vm_xml.c */
/*
 * Section:
 *  SPL Autoloading functions.
 * Status:
 *  Stable.
 */
/*
 * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])
 *  Register given function as __autoload() implementation.
 * Parameters
 *  callback
 *   The autoload function being registered. If no parameter is provided,
 *   then the default implementation of spl_autoload() will be registered.
 *  throw
 *   This parameter specifies whether spl_autoload_register() should throw
 *   exceptions on error. (Ignored in this implementation — always succeeds.)
 *  prepend
 *   If true, spl_autoload_register() will prepend the autoloader on the
 *   autoload stack instead of appending it.
 * Return
 *  TRUE on success, FALSE on failure.
 */
static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmAutoloadCB sEntry;
	ph7_vm *pVm = pCtx->pVm;
	int iPrepend = 0;
	sxu32 n;
	if( nArg < 1 ){
		/* No callback provided — register default spl_autoload.
		 * Store the string "spl_autoload" as the callback. */
		/* Check for duplicates first */
		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){
			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)
				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1
				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){
				ph7_result_bool(pCtx,1);
				return SXRET_OK;
			}
		}
		SyZero(&sEntry,sizeof(VmAutoloadCB));
		PH7_MemObjInit(pVm,&sEntry.sCallback);
		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);
		SySetPut(&pVm->aAutoload,(const void *)&sEntry);
		ph7_result_bool(pCtx,1);
		return SXRET_OK;
	}
	/* Validate that the callback is callable */
	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){
		int iThrow = 1; /* Default: throw on error */
		if( nArg >= 2 ){
			iThrow = ph7_value_to_bool(apArg[1]);
		}
		if( iThrow ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Argument is not callable");
		}
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Check for duplicates */
	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){
		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){
			/* Already registered */
			ph7_result_bool(pCtx,1);
			return SXRET_OK;
		}
	}
	/* Check prepend flag */
	if( nArg >= 3 ){
		iPrepend = ph7_value_to_bool(apArg[2]);
	}
	/* Store the callback */
	SyZero(&sEntry,sizeof(VmAutoloadCB));
	PH7_MemObjInit(pVm,&sEntry.sCallback);
	PH7_MemObjStore(apArg[0],&sEntry.sCallback);
	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){
		/* Prepend: shift existing entries and insert at position 0.
		 * We do this by appending first, then rotating the array. */
		sxu32 nTotal = SySetUsed(&pVm->aAutoload);
		VmAutoloadCB *aBase;
		SySetPut(&pVm->aAutoload,(const void *)&sEntry);
		/* Rotate: move last entry to front */
		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);
		if( aBase ){
			VmAutoloadCB sTemp;
			sxu32 i;
			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));
			for( i = nTotal ; i > 0 ; i-- ){
				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));
			}
			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));
		}
	}else{
		SySetPut(&pVm->aAutoload,(const void *)&sEntry);
	}
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * bool spl_autoload_unregister(callable $callback)
 *  Unregister a given function as __autoload() implementation.
 * Parameters
 *  callback
 *   The autoload function being unregistered.
 * Return
 *  TRUE on success, FALSE on failure.
 */
static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 n,nEntry;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	nEntry = SySetUsed(&pVm->aAutoload);
	for( n = 0 ; n < nEntry ; ++n ){
		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){
			/* Found — remove by shifting remaining entries down */
			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);
			sxu32 i;
			PH7_MemObjRelease(&pEntry->sCallback);
			for( i = n ; i + 1 < nEntry ; i++ ){
				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));
			}
			/* Pop the now-duplicate tail entry via the SySet API */
			SySetPop(&pVm->aAutoload);
			ph7_result_bool(pCtx,1);
			return SXRET_OK;
		}
	}
	ph7_result_bool(pCtx,0);
	return SXRET_OK;
}
/*
 * array spl_autoload_functions(void)
 *  Return all registered __autoload() functions.
 * Return
 *  An array of all registered autoload functions. If no function is registered,
 *  an empty array is returned.
 */
static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	sxu32 n,nEntry;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	nEntry = SySetUsed(&pVm->aAutoload);
	for( n = 0 ; n < nEntry ; ++n ){
		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pEntry ){
			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);
		}
	}
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])
 *  Default implementation of __autoload().
 *  Converts namespace separators to directory separators, lowercases the class
 *  name, and tries to include a file with each of the given extensions.
 * Parameters
 *  class
 *   The class name being searched.
 *  file_extensions
 *   Comma-separated list of file extensions to try.
 */
static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zClass,*zExt,*zEnd,*zCur;
	SyBlob sPath;
	int nClass;
	sxi32 rc;
	if( nArg < 1 ){
		return SXRET_OK;
	}
	zClass = ph7_value_to_string(apArg[0],&nClass);
	if( nClass < 1 ){
		return SXRET_OK;
	}
	/* Default extensions */
	zExt = ".php,.inc";
	if( nArg >= 2 ){
		int nExt;
		zExt = ph7_value_to_string(apArg[1],&nExt);
		if( nExt < 1 ){
			zExt = ".php,.inc";
		}
	}
	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
	/* Iterate over comma-separated extensions */
	zEnd = zExt + SyStrlen(zExt);
	zCur = zExt;
	while( zCur < zEnd ){
		const char *zComma;
		SyString sFile;
		int i;
		/* Find next comma or end */
		zComma = zCur;
		while( zComma < zEnd && *zComma != ',' ){
			zComma++;
		}
		/* Build path: lowercase class name with \ -> / , then append extension */
		SyBlobReset(&sPath);
		for( i = 0 ; i < nClass ; i++ ){
			char c = zClass[i];
			if( c == '\\' ){
				c = '/';
			}else if( c >= 'A' && c <= 'Z' ){
				c = c + ('a' - 'A');
			}
			SyBlobAppend(&sPath,(const void *)&c,1);
		}
		/* Append extension */
		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));
		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,
		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a
		 * terminator, so without this the strlen reads past the buffer (a
		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL
		 * is not counted in SyBlobLength(), so the SyString length stays correct. */
		SyBlobNullAppend(&sPath);
		/* Try to include the file */
		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));
		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);
		if( rc == SXRET_OK ){
			/* File included successfully */
			SyBlobRelease(&sPath);
			return SXRET_OK;
		}
		/* Move past the comma */
		zCur = zComma;
		if( zCur < zEnd && *zCur == ',' ){
			zCur++;
		}
	}
	SyBlobRelease(&sPath);
	return SXRET_OK;
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
	   /* SPL object identity */
	{ "spl_object_id",   vm_builtin_spl_object_id   },
	{ "spl_object_hash", vm_builtin_spl_object_hash },
	   /* SPL Autoloading */
	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },
	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },
	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },
	{ "spl_autoload",            vm_builtin_spl_autoload            },
	   /* Random numbers/strings generators */
	{ "rand",          vm_builtin_rand            },
	{ "mt_rand",       vm_builtin_rand            },
	{ "rand_str",      vm_builtin_rand_str        },
	{ "getrandmax",    vm_builtin_getrandmax      },
	{ "mt_getrandmax", vm_builtin_getrandmax      },
	{ "random_int",    vm_builtin_random_int      },
	{ "random_bytes",  vm_builtin_random_bytes    },
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
	{"phpversion",       vm_builtin_phpversion    },
	{"php_sapi_name",    vm_builtin_php_sapi_name },
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
	{"json_last_error_msg",vm_builtin_json_last_error_msg},
	{"json_decode",    vm_builtin_json_decode },
	{"json_validate",  vm_builtin_json_validate },
	{"serialize",      vm_builtin_serialize },
	{"unserialize",    vm_builtin_unserialize },
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
 * Helper: Apply loadable filter to a class pointer.
 * Returns the first concrete (non-interface, non-abstract, non-trait) class
 * in the name collision chain, or NULL if none qualifies.
 */
static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)
{
	if( !iLoadable ){
		return pClass;
	}
	while(pClass){
		if( (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT|PH7_CLASS_TRAIT)) == 0 ){
			return pClass;
		}
		pClass = pClass->pNextName;
	}
	return 0;
}
/*
 * Trigger the autoload mechanism for a class that was not found.
 * Iterates through registered spl_autoload callbacks, calling each one
 * with the class name. After each callback, checks if the class is now
 * registered in the VM's class table.
 * Returns a pointer to the class on success, NULL on failure.
 * Uses hAutoloadActive to prevent infinite recursion.
 */
static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)
{
	VmAutoloadCB *pEntry;
	ph7_value sArg,sResult;
	SyHashEntry *pHashEntry;
	ph7_class *pClass;
	sxu32 n,nEntry;
	nEntry = SySetUsed(&pVm->aAutoload);
	if( nEntry < 1 ){
		return 0;
	}
	/* Reentrancy guard: check if this class is already being autoloaded */
	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){
		return 0; /* Already in progress, prevent infinite recursion */
	}
	/* Mark this class as being autoloaded */
	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);
	/* Prepare the class name argument */
	PH7_MemObjInit(pVm,&sArg);
	PH7_MemObjInit(pVm,&sResult);
	PH7_MemObjStringAppend(&sArg,zName,nByte);
	pClass = 0;
	for( n = 0 ; n < nEntry ; ++n ){
		ph7_value *apArg[1];
		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pEntry == 0 ){
			continue;
		}
		apArg[0] = &sArg;
		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){
			/* Callback could not be invoked — skip to next autoloader */
			continue;
		}
		/* Check if the class is now available */
		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);
		if( pHashEntry ){
			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);
			if( pClass ){
				break;
			}
		}
	}
	PH7_MemObjRelease(&sArg);
	PH7_MemObjRelease(&sResult);
	/* Remove reentrancy guard */
	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);
	return pClass;
}
/*
 * Trigger autoload for external callers (e.g. class_exists).
 * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.
 */
PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)
{
	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);
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
	SXUNUSED(iNest);
	/* Exact class lookup.
	 * Static names are already namespace-qualified by the compiler.
	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */
	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);
	if( pEntry == 0 ){
		/* Class not found in hash table — try autoload before giving up */
		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);
	}
	pClass = (ph7_class *)pEntry->pUserData;
	return VmFilterLoadableClass(pClass,iLoadable);
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
	pFrame = VmSkipExceptionFrames(pFrame);
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
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
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
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
/* HTTP/URI routines moved to vm_http.c */
