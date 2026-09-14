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
/* Signed 64-bit integer overflow detection lives in ph7int.h as the shared
 * PH7_{ADD,SUB,MUL}_OVERFLOW64 macros (GCC/Clang intrinsics, MSVC fallbacks in
 * memobj.c). The executor uses them to promote an overflowing integer
 * operation to a float, matching PHP. */
/* Argument-unpacking key capture (PHP 8.1 named-parameter semantics for spreads).
 * `pMap->aNames` is COMPILE-TIME metadata indexed by compile-time argument
 * position, but a runtime spread expands its slot to a variable element count,
 * so any spread that expands to !=1 element shifts the following actual stack
 * positions out of alignment with aNames — and the element keys (which PHP 8.1
 * treats as named arguments) are otherwise discarded. OP_SPREAD records one
 * VmSpreadRun per expansion plus one VmSpreadKey per element (in order) on the
 * VM; CALL/NEW replay them (VmBuildEffectiveArgMap) into an effective map with
 * one name entry per ACTUAL slot, then let the existing named-argument resolver
 * run unchanged. The same runs give each call its own argument-count growth
 * (VmSpreadOwnExtra). This call's runs are consumed (truncated) at the CALL. */
typedef struct VmSpreadRun VmSpreadRun;
struct VmSpreadRun {
	ph7_value *pStart;   /* First stack slot the expansion wrote (the source slot) */
	sxu32 nCount;        /* Elements produced (0 for an empty array) */
	sxu32 nKeyStart;     /* aSpreadKey index of this run's first element key */
	sxu32 nBlobStart;    /* sSpreadKeyBlob length before this run's keys were appended */
};
typedef struct VmSpreadKey VmSpreadKey;
struct VmSpreadKey {
	sxu32 nOff;          /* Byte offset into pVm->sSpreadKeyBlob (valid iff nLen>0) */
	sxu32 nLen;          /* Key length; 0 == integer key == positional element */
};
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
/* VmSlot struct moved to ph7int.h */
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
/* struct VmRefObj + VM_REF_IDX_KEEP moved to ph7int.h */
/*
 * Each installed shutdown callback (registered using [register_shutdown_function()] )
 * is stored in an instance of the following structure.
 * Refer to the implementation of [register_shutdown_function(()] for more information.
 */
/* VmShutdownCB struct moved to ph7int.h */
/*
 * Each installed autoload callback (registered using [spl_autoload_register()] )
 * is stored in an instance of the following structure.
 * Refer to the implementation of [spl_autoload_register()] for more information.
 */
/* VmAutoloadCB struct moved to ph7int.h */

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
	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);
}
/*
 * Like PH7_VmRegisterConstant, additionally recording the constant's
 * origin (file/line/user-defined) for ReflectionConstant.
 */
PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(
	ph7_vm *pVm,            /* Target VM */
	const SyString *pName,  /* Constant name */
	ProcConstant xExpand,   /* Constant expansion callback */
	void *pUserData,        /* Last argument to xExpand() */
	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */
	sxu32 nLine,            /* Declaration line, 0 = unknown */
	int bUser               /* 1 when defined by user code */
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
		if( pFile ){
			SyStringDupPtr(&pCons->sFile,pFile);
		}else{
			SyStringInitFromBuf(&pCons->sFile,0,0);
		}
		pCons->nLine = nLine;
		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);
		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */
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
	SyStringInitFromBuf(&pCons->sFile,0,0);
	if( pFile ){
		SyStringDupPtr(&pCons->sFile,pFile);
	}
	pCons->nLine = nLine;
	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);
	/* Install the constant */
	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);
	pCons->xExpand = xExpand;
	pCons->pUserData = pUserData;
	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));
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
		/* A replacement implementation carries its own (unknown) arity, so drop
		 * any minimum-arity metadata stamped on the previous holder of this name
		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg
		 * custom "substr") would inherit the old ArgumentCountError threshold. */
		pFunc->nMinArg  = 0;
		pFunc->bAtLeast = 0;
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
	/* Declared #[...] attributes */
	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));
	pFunc->iFlags = iFlags;
	pFunc->pUserData = pUserData;
	/* Capture the defining file's strict_types mode. PHP scopes return-type
	 * coercion by the callee's file, so we freeze it at definition time. */
	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);
	if( pVm->bCompilingBuiltin ){
		/* Defined by an embedded builtin chunk: internal, no defining file */
		pFunc->iFlags |= VM_FUNC_INTERNAL;
	}else{
		/* Alias the VM-lifetime path dup on top of the include stack. eval()
		 * chunks push nothing, so eval-defined functions report the includer's
		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */
		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);
		if( pFile ){
			SyStringDupPtr(&pFunc->sFile,pFile);
		}
	}
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
	ph7_gen_state *pGen = &pVm->sCodeGen;
	sxi32 rc;
	/* Fill the VM instruction */
	sInstr.iOp = (sxu8)iOp;
	sInstr.iP1 = iP1;
	sInstr.iP2 = iP2;
	sInstr.p3  = p3;
	/* Stamp the source line. The node handlers point pGen->pIn at the token being
	 * compiled (that is how they read its text), so the current token IS this
	 * instruction's source position; pIn can sit one past the end of the stream
	 * between statements, hence the range check. */
	sInstr.nLine = 0;
	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){
		sInstr.nLine = pGen->pIn->nLine;
	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){
		/* Past the end (statement tail): blame the last real token. */
		sInstr.nLine = pGen->pEnd[-1].nLine;
	}
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
PH7_PRIVATE VmFrame * VmNewFrame(
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
	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */
	/* Per-frame pending catch/finally return slot (always-init so release is
	 * unconditional; bHasRet is already 0 from SyZero). */
	PH7_MemObjInit(&(*pVm),&pFrame->sRet);
	return pFrame;
}
/* Forward declaration */
static void VmSpreadCaptureReset(ph7_vm *pVm);
/*
 * Enter a VM frame.
 */
PH7_PRIVATE sxi32 VmEnterFrame(
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
	/* The line currently executing IS the call site for the frame being pushed. */
	pFrame->nCallLine = pVm->nCurLine;
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
PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)
{
	if( pVm->pResumeFrame == pFrame ){
		pVm->pResumeFrame = 0;
	}
}
/*
 * Leave the top-most active frame.
 */
PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)
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
 * Pin a memory-object slot past its owning frame: remove it from whichever
 * active frame's local-teardown set records it (walking the parent chain
 * covers by-ref argument aliases whose slot belongs to a caller), and flag
 * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.
 * Used for by-reference closure captures (`use (&$x)`), whose slot must stay
 * alive as long as the closure itself: the slot then lives until VM reset —
 * php frees it by refcount, PHL trades that for a script-lifetime pin.
 */
/*
 * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)
 * records it — walking the parent chain covers by-reference aliases whose slot is
 * owned by a caller. Returns TRUE if an entry was dropped.
 *
 * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot
 * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),
 * or returned to the free pool by unset() — and once the index is recycled for a
 * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale
 * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the
 * entry at the point the slot leaves the frame closes that use-after-free.
 */
PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)
{
	VmFrame *pFrame;
	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){
		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){
			if( aSlot[n].nIdx == nIdx ){
				/* Swap-remove: teardown order over sLocal is immaterial */
				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];
				(void)SySetPop(&pFrame->sLocal);
				return TRUE; /* Slot owned by exactly one frame */
			}
		}
	}
	return FALSE;
}
static void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)
{
	VmRefObj *pRef;
	VmDropFrameLocalSlot(&(*pVm),nIdx);
	pRef = VmRefObjExtract(&(*pVm),nIdx);
	if( pRef ){
		pRef->iFlags |= VM_REF_IDX_KEEP;
	}
}
/*
 * Skip exception frames to reach the nearest non-exception frame.
 * Exception frames are transparent wrappers pushed by try/catch and
 * should be skipped when looking for the real execution context.
 */
PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)
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
	/* The catch may have run at an OUTER try, several call-levels above the throw.
	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the
	 * THROW SITE (its pThrowSite restore), so between here and the catching try's
	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION
	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about
	 * to skip), AND — when the throw was raised in a DEEPER call than the one that
	 * declared the catching try — the dead body frames of those abandoned callees
	 * (the exception unwound past them, but the in-place-catch resume short-circuits
	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL
	 * so exactly one frame (the catching try's exception frame) is left for the
	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the
	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame
	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live
	 * caller variable reads as an uninitialised fresh one). Their handlers/finally
	 * already ran in place during VmThrowException. The loop stops on either term, both
	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==
	 * the recorded landing); or this exec's entry is reached (structural floor). Landing
	 * pads are unique per try within one bytecode array, and the function guard above
	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the
	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so
	 * leaving the catching exception frame here (rather than the body) lands cleanly. */
	while( pVm->pFrame != pEntryFrame
	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)
	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){
		VmLeaveFrame(&(*pVm));
	}
	*pResumePc = (sxi32)pVm->iResumePc - 1;
	pVm->pResumeFrame = 0; /* one-shot consume */
	/* Landing at the catch pad consumes any C-boundary parked copy of the same
	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-
	 * point router must not re-fire it after this resume. */
	pVm->nBoundaryRc = 0;
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
 * (silent wrong answers; see the try_unwind_recursive_frames
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
PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)
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
PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)
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
PH7_PRIVATE void VmClearFrameReturn(VmFrame *pFrame)
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
/*
 * Evaluate a constant/default initializer bytecode into a pool memory-object slot,
 * safely across a pool reallocation.
 *
 * VmLocalExec writes its result through the caller's pResult pointer at
 * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the
 * initializer allocates pool memobjs — a large array literal grows aMemObj via
 * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —
 * the reserved pResult pointer dangles and the final store is a heap
 * use-after-free (confirmed via ASan on a >=~227-element class-const array; it
 * is what blocked Composer's autoload class-map). Evaluate into a stable local
 * instead, then store into the slot re-fetched by its (stable) index. On return
 * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the
 * destination slot's nIdx (excluded from its memcpy), so the slot identity is
 * kept. Mirrors the enum-case backing path, which already evaluates into a local.
 */
PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)
{
	ph7_value sVal;
	sxu32 nIdx = (*ppMemObj)->nIdx;
	sxi32 rc;
	PH7_MemObjInit(&(*pVm),&sVal);
	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);
	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */
	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	if( *ppMemObj ){
		PH7_MemObjStore(&sVal,*ppMemObj);
	}
	PH7_MemObjRelease(&sVal);
	return rc;
}
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
		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)
		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0
			|| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){
			/* Untyped class constants and enum cases are evaluated LAZILY, on
			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),
			 * matching php. Eager evaluation here ran BEFORE execution for
			 * top-level classes (PH7_VmMakeReady), so an initializer error
			 * (self-reference, enum backing mismatch) could never reach a
			 * user catch, and initializers referencing constants of a class
			 * mounted later in hash order silently read NULL. TYPED constants
			 * stay eager: php validates them at DECLARATION time ("Cannot use
			 * %s as value for class constant" fatal without any access). */
			continue;
		}
		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
			ph7_value *pMemObj;
			if( pAttr->nIdx != SXU32_HIGH ){
				/* Already materialized (an attr shared with an earlier-mounted
				 * class). PH7_VmReset invalidates every nIdx before its
				 * re-mount pass, so VM reuse still re-evaluates. */
				continue;
			}
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
				/* Initialize attribute default value (any complex expression).
				 * pConstEvalClass lets self::/parent:: in the initializer
				 * resolve (VmLocalExec runs without a method frame). */
				ph7_class *pSaveCtx = pVm->pConstEvalClass;
				sxi32 rcExec;
				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
				pAttr->iFlags |= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */
				pVm->nConstEvalDepth++;
				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);
				pVm->nConstEvalDepth--;
				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;
				pVm->pConstEvalClass = pSaveCtx;
				if( rcExec == PH7_EXCEPTION || rcExec == PH7_ABORT ){
					/* The initializer raised (self-referencing constant, or a
					 * throwing enum-case reference): park it for the fetch-point
					 * router — user classes mount mid-execution, so the throw
					 * lands catchably at the declaration site. */
					VmBoundaryPark(&(*pVm),rcExec);
				}else if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){
					/* A nested evaluation detected a self-referencing constant:
					 * raise it at this, the outermost level. */
					VmBoundaryPark(&(*pVm),VmConstCycleThrow(&(*pVm)));
				}
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
				/* Initialize attribute default value (any complex expression).
				 * pConstEvalClass: self::CONST in a property default resolves
				 * against the declaring class (no method frame here). */
				ph7_class *pSaveCtx = pVm->pConstEvalClass;
				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
				VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);
				pVm->pConstEvalClass = pSaveCtx;
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
 * stdClass for now; the future general-dynamic-props work turns
 * this into a class-flag / #[AllowDynamicProperties] check at this one site.
 */
static int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)
{
	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;
}
/*
 * Whether pClass carries a #[...] attribute of the given (resolved) name —
 * band A #3b uses it for #[AllowDynamicProperties], which suppresses the
 * php 8.2 dynamic-property deprecation. Inherited attributes do not apply
 * (php: the attribute must be on the class itself... except php DOES honor
 * it on parents for AllowDynamicProperties — walk the ancestry).
 */
static int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)
{
	while( pClass ){
		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);
		sxu32 n;
		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){
			if( aAttr[n].sName.nByte == nName
			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){
				return TRUE;
			}
		}
		pClass = pClass->pBase;
	}
	return FALSE;
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
/* Forward declarations for Fiber C functions */
/* Forward declarations for Fiber/Generator infrastructure */
/* Forward declarations for Generator helpers and C functions */
/*
 * Built-in classes/interfaces and some functions that cannot be implemented
 * directly as foreign functions.
 */
/* The libxml-backed extensions (libxml/dom/xmlwriter) are reported by
 * extension_loaded()/get_loaded_extensions() only when compiled in. These
 * string fragments splice into the prelude arrays via adjacent-literal
 * concatenation; they are leading-comma so they append cleanly and vanish
 * to "" in tiny/non-libxml builds. */
#ifdef PH7_ENABLE_LIBXML
#define PHL_EXT_LOADED_LIBXML ", 'libxml' => 1, 'dom' => 1, 'xmlwriter' => 1"
#define PHL_EXT_LIST_LIBXML   ",'libxml','dom','xmlwriter'"
#else
#define PHL_EXT_LOADED_LIBXML ""
#define PHL_EXT_LIST_LIBXML   ""
#endif
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
    "private $trace;"\
    "private $previous;"\
	"public function __construct($message = null, $code = 0, ?Throwable $previous = null){"\
	"   if( isset($message) ){"\
	"	  $this->message = $message;"\
	"   }"\
	"   $this->code = $code;"\
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
	"  $s = ''; $i = 0;"\
	"  if( is_array($this->trace) ){"\
	"    foreach( $this->trace as $f ){"\
	"      $a = array();"\
	"      if( isset($f['args']) && is_array($f['args']) ){"\
	"        foreach( $f['args'] as $v ){"\
	"          if( is_string($v) ){"\
	"            $a[] = strlen($v) > 15 ? \"'\" . substr($v, 0, 15) . \"...'\" : \"'\" . $v . \"'\";"\
	"          } elseif( is_array($v) ){ $a[] = 'Array'; }"\
	"          elseif( is_object($v) ){ $a[] = 'Object(' . get_class($v) . ')'; }"\
	"          elseif( is_null($v) ){ $a[] = 'NULL'; }"\
	"          elseif( is_bool($v) ){ $a[] = $v ? 'true' : 'false'; }"\
	"          else { $a[] = (string)$v; }"\
	"        }"\
	"      }"\
	"      $s .= '#' . $i . ' ' . $f['file'] . '(' . $f['line'] . '): '"\
	"         . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function']"\
	"         . '(' . implode(', ', $a) . \")\\n\";"\
	"      $i++;"\
	"    }"\
	"  }"\
	"  return $s . '#' . $i . ' {main}';"\
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
    "private $trace;"\
    "private $previous;"\
	"public function __construct($message = null, $code = 0, ?Throwable $previous = null){"\
	"   if( isset($message) ){"\
	"	  $this->message = $message;"\
	"   }"\
	"   $this->code = $code;"\
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
	"  $s = ''; $i = 0;"\
	"  if( is_array($this->trace) ){"\
	"    foreach( $this->trace as $f ){"\
	"      $a = array();"\
	"      if( isset($f['args']) && is_array($f['args']) ){"\
	"        foreach( $f['args'] as $v ){"\
	"          if( is_string($v) ){"\
	"            $a[] = strlen($v) > 15 ? \"'\" . substr($v, 0, 15) . \"...'\" : \"'\" . $v . \"'\";"\
	"          } elseif( is_array($v) ){ $a[] = 'Array'; }"\
	"          elseif( is_object($v) ){ $a[] = 'Object(' . get_class($v) . ')'; }"\
	"          elseif( is_null($v) ){ $a[] = 'NULL'; }"\
	"          elseif( is_bool($v) ){ $a[] = $v ? 'true' : 'false'; }"\
	"          else { $a[] = (string)$v; }"\
	"        }"\
	"      }"\
	"      $s .= '#' . $i . ' ' . $f['file'] . '(' . $f['line'] . '): '"\
	"         . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function']"\
	"         . '(' . implode(', ', $a) . \")\\n\";"\
	"      $i++;"\
	"    }"\
	"  }"\
	"  return $s . '#' . $i . ' {main}';"\
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
	"public function __construct(?string $message = null,"\
	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,?Throwable $previous = null){"\
	"   /* message/code/previous belong to Exception (trace/previous are private"\
	"    * to it); delegate, then set our own severity plus the caller-supplied"\
	"    * file/line, which are protected and stay writable here. */"\
	"   parent::__construct($message, $code, $previous);"\
	"   $this->severity = $severity;"\
	"   $this->file = $filename;"\
	"   $this->line = $lineno;"\
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
	"class JsonException extends Exception { }"\
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
	"#[Attribute(Attribute::TARGET_CLASS)]"\
	"final class Attribute {"\
	"  const TARGET_CLASS = 1;"\
	"  const TARGET_FUNCTION = 2;"\
	"  const TARGET_METHOD = 4;"\
	"  const TARGET_PROPERTY = 8;"\
	"  const TARGET_CLASS_CONSTANT = 16;"\
	"  const TARGET_PARAMETER = 32;"\
	"  const TARGET_CONSTANT = 64;"\
	"  const TARGET_ALL = 127;"\
	"  const IS_REPEATABLE = 128;"\
	"  public $flags;"\
	"  public function __construct($flags = 127){ $this->flags = $flags; }"\
	"}"\
	"#[Attribute(Attribute::TARGET_METHOD | Attribute::TARGET_FUNCTION | Attribute::TARGET_CLASS_CONSTANT | Attribute::TARGET_CONSTANT)]"\
	"final class Deprecated {"\
	"  public $message;"\
	"  public $since;"\
	"  public function __construct($message = null, $since = null){"\
	"    $this->message = $message;"\
	"    $this->since = $since;"\
	"  }"\
	"}"\
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
	"/* php keeps the literal directory portion of the pattern in every result;"\
	"   split off everything up to and including the last '/' as the prefix. */"\
	"$slash = strrpos($pattern,'/');"\
	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\
	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\
	"$pHandle = opendir($zDir);"\
	"if( $pHandle == FALSE ){"\
	"   /* IO error while opening the target directory,return FALSE */"\
	"	return FALSE;"\
	"}"\
	"$pArray = array(); /* Empty array */"\
	"/* Loop throw available entries */"\
	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\
	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\
	"    the pattern itself starts with a dot */"\
	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 || $pat[0] !== '.') ){ continue; }"\
	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\
	"	$rc = strglob($pat,$pEntry);"\
	"	if( $rc ){"\
	"	   $zFull = $prefix . $pEntry;"\
	"	   if( is_dir($zDir . '/' . $pEntry) ){"\
	"	      if( $iFlags & GLOB_MARK ){"\
	"		     /* Adds a slash to each directory returned */"\
	"			 $zFull .= DIRECTORY_SEPARATOR;"\
	"		  }"\
	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\
	"	     /* Not a directory,ignore */"\
	"		 continue;"\
	"	   }"\
	"	   /* Add the entry (with its literal directory prefix, php-style) */"\
	"	   $pArray[] = $zFull;"\
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
   "/* php's number_format(): missing entirely from PH7. */"\
   "function number_format($num, $decimals = 0, $dec_point = '.', $thousands_sep = ','){"\
   "  $num = (float)$num;"\
   "  $decimals = (int)$decimals;"\
   "  if( $decimals < 0 ){ $decimals = 0; }"\
   "  if( $dec_point === null ){ $dec_point = '.'; }"\
   "  if( $thousands_sep === null ){ $thousands_sep = ','; }"\
   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\
   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). */"\
   "  $num = round($num, $decimals);"\
   "  $s = sprintf('%.' . $decimals . 'f', $num);"\
   "  $neg = false;"\
   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\
   "  $parts = explode('.', $s);"\
   "  $int = $parts[0];"\
   "  $frac = count($parts) > 1 ? $parts[1] : '';"\
   "  $out = '';"\
   "  $len = strlen($int);"\
   "  $c = 0;"\
   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\
   "    $out = $int[$i] . $out;"\
   "    $c++;"\
   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_sep . $out; }"\
   "  }"\
   "  if( $decimals > 0 ){ $out = $out . $dec_point . $frac; }"\
   "  if( $neg ){ $out = '-' . $out; }"\
   "  return $out;"\
   "}"\
   "function is_nan($v){ $v = (float)$v; return $v != $v; }"\
   "function is_infinite($v){ $v = (float)$v; return $v == INF || $v == -INF; }"\
   "function is_finite($v){ $v = (float)$v; return !is_nan($v) && !is_infinite($v); }"\
   "/* php's version_compare: canonicalise (separators + digit/alpha boundaries all"\
   " * become '.'), then compare parts with the special dev<alpha<beta<RC<#<pl ordering. */"\
   "function __phl_vcanon($v){"\
   "  $v = (string)$v; $len = strlen($v); $out = '';"\
   "  for( $i = 0; $i < $len; $i++ ){"\
   "   $c = $v[$i]; $rp = $i + 1 < $len ? $v[$i + 1] : '';"\
   "   $cd = ($c >= '0' && $c <= '9');"\
   "   $ca = $cd || ($c >= 'a' && $c <= 'z') || ($c >= 'A' && $c <= 'Z');"\
   "   if( !$ca ){"\
   "    /* any non-alphanumeric (., -, _, +, ...) is a separator: emit one '.' */"\
   "    if( $out !== '' && substr($out, -1) !== '.' ){ $out .= '.'; }"\
   "   }else{"\
   "    $out .= $c;"\
   "    $rd = ($rp >= '0' && $rp <= '9');"\
   "    $ra = $rd || ($rp >= 'a' && $rp <= 'z') || ($rp >= 'A' && $rp <= 'Z');"\
   "    if( $rp !== '' && $ra && ($cd !== $rd) ){ $out .= '.'; }"\
   "   }"\
   "  }"\
   "  return explode('.', $out);"\
   "}"\
   "function __phl_vform($s){"\
   "  if( $s === '' ){ return -1; }"\
   "  if( ctype_digit($s) ){ return 4; }"\
   "  $f = array('dev' => 0, 'alpha' => 1, 'a' => 1, 'beta' => 2, 'b' => 2, 'RC' => 3, 'rc' => 3, 'pl' => 5, 'p' => 5);"\
   "  foreach( $f as $name => $ord ){ if( strncmp($s, $name, strlen($name)) === 0 ){ return $ord; } }"\
   "  return -1;"\
   "}"\
   "function version_compare($version1, $version2, $operator = null){"\
   "  $v1 = __phl_vcanon($version1); $v2 = __phl_vcanon($version2);"\
   "  $n1 = count($v1); $n2 = count($v2); $n = $n1 > $n2 ? $n1 : $n2; $cmp = 0;"\
   "  for( $i = 0; $i < $n; $i++ ){"\
   "   $a = $i < $n1 ? $v1[$i] : null; $b = $i < $n2 ? $v2[$i] : null;"\
   "   if( $a === null ){ $cmp = ctype_digit($b) ? -1 : (4 <=> __phl_vform($b)); }"\
   "   elseif( $b === null ){ $cmp = ctype_digit($a) ? 1 : (__phl_vform($a) <=> 4); }"\
   "   elseif( ctype_digit($a) && ctype_digit($b) ){ $cmp = (int)$a <=> (int)$b; }"\
   "   else{ $cmp = __phl_vform($a) <=> __phl_vform($b); }"\
   "   if( $cmp !== 0 ){ break; }"\
   "  }"\
   "  if( $operator === null ){ return $cmp; }"\
   "  switch( (string)$operator ){"\
   "   case '<': case 'lt': return $cmp < 0;"\
   "   case '<=': case 'le': return $cmp <= 0;"\
   "   case '>': case 'gt': return $cmp > 0;"\
   "   case '>=': case 'ge': return $cmp >= 0;"\
   "   case '==': case '=': case 'eq': return $cmp === 0;"\
   "   case '!=': case '<>': case 'ne': return $cmp !== 0;"\
   "  }"\
   "  return null;"\
   "}"\
   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\
   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\
   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\
   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\
   "function __phl_stub_exts(){"\
   "  $s = ini_get('phl.stub_extensions');"\
   "  if( $s === false || $s === '' ){ return array(); }"\
   "  $out = array();"\
   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\
   "  return $out;"\
   "}"\
   "function extension_loaded($name){"\
   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\
   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\
   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\
   "  $n = strtolower((string)$name);"\
   "  if( isset($ext[$n]) ){ return true; }"\
   "  $stub = __phl_stub_exts();"\
   "  return isset($stub[$n]);"\
   "}"\
   "function get_loaded_extensions($zend_extensions = false){"\
   "  if( $zend_extensions ){ return array(); }"\
   "  $base = array('Core','date','pcre','SPL','json','standard',"\
   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\
   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\
   "  return $base;"\
   "}"\
   "/* Inverse of bin2hex() */"\
   "function hex2bin($str){"\
   "  $str = (string)$str;"\
   "  $len = strlen($str);"\
   "  if( $len % 2 !== 0 ){"\
   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\
   "    return false;"\
   "  }"\
   "  $out = '';"\
   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\
   "    $pair = substr($str, $i, 2);"\
   "    if( !ctype_xdigit($pair) ){"\
   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\
   "      return false;"\
   "    }"\
   "    $out = $out . chr(hexdec($pair));"\
   "  }"\
   "  return $out;"\
   "}"\
   "/* Division that never throws: INF/-INF/NAN like php */"\
   "function fdiv($a, $b){"\
   "  $a = (float)$a;"\
   "  $b = (float)$b;"\
   "  if( $b == 0.0 ){"\
   "    if( $a == 0.0 || is_nan($a) ){ return NAN; }"\
   "    return $a > 0 ? INF : -INF;"\
   "  }"\
   "  return $a / $b;"\
   "}"\
   "function checkdate($month, $day, $year){"\
   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\
   "  if( $month < 1 || $month > 12 || $year < 1 || $year > 32767 || $day < 1 ){ return false; }"\
   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\
   "  $max = $days[$month - 1];"\
   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) || ($year % 400 === 0)) ){"\
   "    $max = 29;"\
   "  }"\
   "  return $day <= $max;"\
   "}"\
   "function is_iterable($v){ return is_array($v) || ($v instanceof Traversable); }"\
   "function is_countable($v){ return is_array($v) || ($v instanceof Countable); }"\
   "function key_exists($key, $array){ return array_key_exists($key, $array); }"\
   "function doubleval($v){ return (float)$v; }"\
   "function array_count_values($array){"\
   "  $out = array();"\
   "  foreach( $array as $v ){"\
   "    if( !is_int($v) && !is_string($v) ){"\
   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\
   "      continue;"\
   "    }"\
   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\
   "  }"\
   "  return $out;"\
   "}"\
   "function array_change_key_case($array, $case = CASE_LOWER){"\
   "  $out = array();"\
   "  foreach( $array as $k => $v ){"\
   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\
   "    $out[$k] = $v;"\
   "  }"\
   "  return $out;"\
   "}"\
   "function array_replace_recursive($array, ...$others){"\
   "  foreach( $others as $o ){"\
   "    foreach( $o as $k => $v ){"\
   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\
   "        $array[$k] = array_replace_recursive($array[$k], $v);"\
   "      }else{"\
   "        $array[$k] = $v;"\
   "      }"\
   "    }"\
   "  }"\
   "  return $array;"\
   "}"\
   "function class_uses($what, $autoload = true){"\
   "  $c = is_object($what) ? get_class($what) : (string)$what;"\
   "  if( !class_exists($c) ){ return false; }"\
   "  return array();  /* PHL has no traits yet -- always the empty set */"\
   "}"\
   "function count_chars($str, $mode = 0){"\
   "  $str = (string)$str;"\
   "  $counts = array();"\
   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\
   "  $len = strlen($str);"\
   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($str[$i]); $counts[$b] = $counts[$b] + 1; }"\
   "  if( $mode == 1 ){"\
   "    $out = array();"\
   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out[$b] = $n; } }"\
   "    return $out;"\
   "  }"\
   "  if( $mode == 3 ){"\
   "    $out = '';"\
   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out = $out . chr($b); } }"\
   "    return $out;"\
   "  }"\
   "  return $counts;"\
   "}"\
   "function ip2long($ip){"\
   "  $p = explode('.', (string)$ip);"\
   "  if( count($p) !== 4 ){ return false; }"\
   "  $n = 0;"\
   "  foreach( $p as $o ){"\
   "    if( !ctype_digit($o) || (int)$o < 0 || (int)$o > 255 ){ return false; }"\
   "    $n = $n * 256 + (int)$o;"\
   "  }"\
   "  return $n;"\
   "}"\
   "function long2ip($n){"\
   "  $n = (int)$n;"\
   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\
   "}"\
   "function preg_filter($pattern, $replacement, $subject, $limit = -1){"\
   "  if( is_array($subject) ){"\
   "    $out = array();"\
   "    foreach( $subject as $k => $v ){"\
   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\
   "      if( $cnt > 0 ){ $out[$k] = $r; }"\
   "    }"\
   "    return $out;"\
   "  }"\
   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\
   "  return $cnt > 0 ? $r : null;"\
   "}"\
   "function preg_replace_callback_array($patterns, $subject, $limit = -1){"\
   "  foreach( $patterns as $pat => $cb ){"\
   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit);"\
   "  }"\
   "  return $subject;"\
   "}"\
   "function cal_days_in_month($calendar, $month, $year){"\
   "  $month = (int)$month; $year = (int)$year;"\
   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\
   "  if( $month < 1 || $month > 12 ){"\
   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\
   "  }"\
   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) || ($year % 400 === 0)) ){"\
   "    return 29;"\
   "  }"\
   "  return $days[$month - 1];"\
   "}"\
   "function preg_grep($pattern, $array, $flags = 0){"\
   "  $out = array();"\
   "  foreach( $array as $k => $v ){"\
   "    $m = preg_match($pattern, (string)$v);"\
   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\
   "    if( $m ){ $out[$k] = $v; }"\
   "  }"\
   "  return $out;"\
   "}"\
   "function class_implements($what, $autoload = true){"\
   "  $c = is_object($what) ? get_class($what) : (string)$what;"\
   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\
   "  $out = array();"\
   "  $r = new ReflectionClass($c);"\
   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\
   "  return $out;"\
   "}"\
   "function class_parents($what, $autoload = true){"\
   "  $c = is_object($what) ? get_class($what) : (string)$what;"\
   "  if( !class_exists($c) ){ return false; }"\
   "  $out = array();"\
   "  $r = new ReflectionClass($c);"\
   "  while( ($p = $r->getParentClass()) ){"\
   "    $n = $p->getName();"\
   "    $out[$n] = $n;"\
   "    $r = $p;"\
   "  }"\
   "  return $out;"\
   "}"\
   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\
   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\
   "function __phl_hbq_enc($s, $enc){"\
   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\
   "}"\
   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\
   "  foreach( $data as $k => $v ){"\
   "    if( $v === null ){ continue; }"\
   "    if( $key_prefix === '' ){"\
   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\
   "    } else {"\
   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\
   "    }"\
   "    if( is_array($v) ){"\
   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\
   "    } elseif( is_object($v) ){"\
   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\
   "    } else {"\
   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\
   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\
   "    }"\
   "  }"\
   "}"\
   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\
   "  if( !is_array($data) && !is_object($data) ){"\
   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array|object, ' . gettype($data) . ' given');"\
   "  }"\
   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\
   "  $pairs = array();"\
   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\
   "  return implode($arg_separator, $pairs);"\
   "}"\
   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\
   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\
   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\
   "  $seg = $segments[$i];"\
   "  $last = ($i === count($segments) - 1);"\
   "  if( $seg === '' ){"\
   "    if( $last ){ $arr[] = $val; return; }"\
   "    $arr[] = array();"\
   "    $k = array_key_last($arr);"\
   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\
   "  } else {"\
   "    if( $last ){ $arr[$seg] = $val; return; }"\
   "    if( !isset($arr[$seg]) || !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\
   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\
   "  }"\
   "}"\
   "function parse_str($string, &$result){"\
   "  $result = array();"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return; }"\
   "  foreach( explode('&', $string) as $pair ){"\
   "    if( $pair === '' ){ continue; }"\
   "    $eq = strpos($pair, '=');"\
   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\
   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\
   "    if( $rawkey === '' ){ continue; }"\
   "    $bpos = strpos($rawkey, '[');"\
   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\
   "    else {"\
   "      $base = substr($rawkey, 0, $bpos);"\
   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\
   "      $subs = $m[1];"\
   "    }"\
   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\
   "    if( $base === '' ){ continue; }"\
   "    $segs = array($base);"\
   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\
   "    __phl_parsestr_assign($result, $segs, 0, $val);"\
   "  }"\
   "}"\
   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\
   "function str_increment($string){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\
   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\
   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\
   "    $c = $string[$i];"\
   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\
   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\
   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\
   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\
   "  }"\
   "  $first = $string[0];"\
   "  if( $first === '0' ){ return '1' . $string; }"\
   "  if( $first === 'a' ){ return 'a' . $string; }"\
   "  return 'A' . $string;"\
   "}"\
   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\
   " * at the bottom of the counting sequence. */"\
   "function str_decrement($string){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\
   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\
   "  $orig = $string;"\
   "  $borrowed = false;"\
   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\
   "    $c = $string[$i];"\
   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\
   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\
   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\
   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\
   "    if( $i === 0 ){ $borrowed = true; }"\
   "  }"\
   "  if( $borrowed ){"\
   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\
   "    $string = substr($string, 1);"\
   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\
   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\
   "    $string = substr($string, 1);"\
   "  }"\
   "  return $string;"\
   "}"\
   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\
   "function fileperms($filename){"\
   "  $s = @stat($filename);"\
   "  if( $s === false ){"\
   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\
   "    return false;"\
   "  }"\
   "  return $s['mode'];"\
   "}"\
   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\
   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\
   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\
   "function mb_ucfirst($string, $encoding = null){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return ''; }"\
   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\
   "}"\
   "function mb_lcfirst($string, $encoding = null){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return ''; }"\
   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\
   "}"\
   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\
   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\
   " * whitespace set. */"\
   "function __phl_mb_ws(){"\
   "  static $set = null;"\
   "  if( $set === null ){"\
   "    $set = array();"\
   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\
   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\
   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\
   "      $set[mb_chr($cp)] = true;"\
   "    }"\
   "  }"\
   "  return $set;"\
   "}"\
   "function __phl_mb_trim($string, $characters, $left, $right){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return ''; }"\
   "  if( $characters === null ){"\
   "    $set = __phl_mb_ws();"\
   "  } else {"\
   "    $set = array();"\
   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\
   "  }"\
   "  $chars = mb_str_split($string);"\
   "  $n = count($chars);"\
   "  $i = 0; $j = $n;"\
   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\
   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\
   "  return implode('', array_slice($chars, $i, $j - $i));"\
   "}"\
   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\
   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\
   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\
   "/* Creates a temporary file and returns its name */"\
   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\
   "{"\
   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\
   "    * returning a bare name left the caller with a path that does not exist, so"\
   "    * file_exists() was false and unlink() failed on it. */"\
   "   $zDir = rtrim($zDir, DIRECTORY_SEPARATOR);"\
   "   for( $i = 0 ; $i < 64 ; ++$i ){"\
   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\
   "     if( file_exists($zPath) ){ continue; }"\
   "     $pHandle = @fopen($zPath,'x');"\
   "     if( $pHandle === false ){ continue; }"\
   "     fclose($pHandle);"\
   "     @chmod($zPath, 0600);"\
   "     return $zPath;"\
   "   }"\
   "   return false;"\
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
	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\
	"function __php_zpp_type($v){"\
	" if( is_object($v) ){ return get_class($v); }"\
	" if( is_int($v) ){ return 'int'; }"\
	" if( is_float($v) ){ return 'float'; }"\
	" if( is_string($v) ){ return 'string'; }"\
	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\
	" if( is_null($v) ){ return 'null'; }"\
	" if( is_array($v) ){ return 'array'; }"\
	" if( is_resource($v) ){ return 'resource'; }"\
	" return 'mixed';"\
	"}"\
	"function max(){"\
    "  $pArgs = func_get_args();"\
    " if( sizeof($pArgs) < 1 ){"\
	"  throw new ArgumentCountError('max() expects at least 1 argument, 0 given');"\
    " }"\
    " if( sizeof($pArgs) < 2 ){"\
    " $pArg = $pArgs[0];"\
	" if( !is_array($pArg) ){"\
	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\
	" }"\
	" if( sizeof($pArg) < 1 ){"\
	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\
	" }"\
	" $max = null; $first = true;"\
	" foreach( $pArgs[0] as $val ){"\
	"   if( $first ){ $max = $val; $first = false; }"\
	"   else if( $val > $max ){ $max = $val; }"\
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
	"  throw new ArgumentCountError('min() expects at least 1 argument, 0 given');"\
    " }"\
    " if( sizeof($pArgs) < 2 ){"\
    " $pArg = $pArgs[0];"\
	" if( !is_array($pArg) ){"\
	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\
	" }"\
	" if( sizeof($pArg) < 1 ){"\
	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\
	" }"\
	" $min = null; $first = true;"\
	" foreach( $pArgs[0] as $val ){"\
	"   if( $first ){ $min = $val; $first = false; }"\
	"   else if( $val < $min ){ $min = $val; }"\
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
	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */
	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);
	/* Instructions containers */
	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));
	SySetAlloc(&pVm->aByteCode,0xFF);
	pVm->pByteContainer = &pVm->aByteCode;
	/* Object containers */
	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));
	SySetAlloc(&pVm->aMemObj,0xFF);
	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */
	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));
	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));
	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);
	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));
	/* Virtual machine internal containers */
	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);
	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);
	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);
	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);
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
	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));
	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));
	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);
	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);
	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));
	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));
	pVm->pMagicSetThis = 0;
	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);
	pVm->pHookSetThis = 0;
	pVm->pHookSetAttr = 0;
	pVm->nHookSetIdx = SXU32_HIGH;
	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));
	pVm->pMagicCallThis = 0;
	pVm->pMagicCallClass = 0;
	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);
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
	pVm->nBoundaryRc = 0;
	pVm->pConstEvalClass = 0;
	pVm->nConstEvalDepth = 0;
	pVm->pConstCycleAttr = 0;
	pVm->pConstCycleClass = 0;
	SySetReset(&pVm->aMagicGuard);
	if( pVm->pMagicSetThis ){
		PH7_ClassInstanceUnref(pVm->pMagicSetThis);
		pVm->pMagicSetThis = 0;
	}
	SyBlobRelease(&pVm->sMagicSetName);
	if( pVm->pHookSetThis ){
		PH7_ClassInstanceUnref(pVm->pHookSetThis);
		pVm->pHookSetThis = 0;
	}
	pVm->pHookSetAttr = 0;
	pVm->nHookSetIdx = SXU32_HIGH;
	if( pVm->pMagicCallThis ){
		PH7_ClassInstanceUnref(pVm->pMagicCallThis);
		pVm->pMagicCallThis = 0;
	}
	pVm->pMagicCallClass = 0;
	SyBlobRelease(&pVm->sMagicCallName);
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
	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so
	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.
	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */
	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;
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
	/* Classes/functions defined by the embedded builtin chunks below are
	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */
	pVm->bCompilingBuiltin = 1;
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
	/* bCompilingBuiltin stays set until the Reflection library below has
	 * compiled — its classes are internal too. */
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
	/* Install the Reflection library (embedded classes + __reflect_* thunks).
	 * Still inside the bCompilingBuiltin window so its classes are flagged
	 * internal; the Traversable pointer above must already be cached. */
	PH7_VmInstallReflection(&(*pVm));
	PH7_VmInstallDateTime(&(*pVm));
	PH7_VmInstallSpl(&(*pVm));
	PH7_VmInstallTokenizer(&(*pVm));
	PH7_VmInstallSession(&(*pVm));
	PH7_VmInstallIni(&(*pVm));
#ifdef PH7_ENABLE_LIBXML
	/* libxml2-backed surfaces: shared plumbing first, then the DOM and
	 * XMLWriter class libraries that build on it. */
	PH7_VmInstallLibxml(&(*pVm));
	PH7_VmInstallDom(&(*pVm));
	PH7_VmInstallXmlWriter(&(*pVm));
#endif
	pVm->bCompilingBuiltin = 0;
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
PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)
{
	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;
	if( xCons != VmObConsumer ){
		pVm->nOutputLen += nLen;
		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){
			pVm->bHeadersSent = 1;
		}
	}
}
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
PH7_PRIVATE ph7_value * VmNewOperandStack(
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
	/* Register the tokenizer T_* / TOKEN_PARSE constants */
	PH7_RegisterTokenizerConstants(&(*pVm));
	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */
	PH7_RegisterBuiltInFunction(&(*pVm));
	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */
	PH7_RegisterHttpResponseFunctions(&(*pVm));
#ifdef PH7_ENABLE_PCRE
	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */
	PH7_RegisterPcreFunctions(&(*pVm));
	PH7_RegisterPcreConstants(&(*pVm));
#endif
#ifdef PH7_ENABLE_LIBXML
	/* Register the LIBXML_* / XML_*_NODE constants */
	PH7_RegisterLibxmlConstants(&(*pVm));
#endif
	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the
	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */
	VmSetBuiltinArity(&(*pVm));
	/* Attach PHP-style parameter signatures for reflection over builtins */
	VmSetBuiltinSignatures(&(*pVm));
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
	pVm->nBoundaryRc = 0;
	pVm->pConstEvalClass = 0;
	pVm->nConstEvalDepth = 0;
	pVm->pConstCycleAttr = 0;
	pVm->pConstCycleClass = 0;
	SySetReset(&pVm->aMagicGuard);
	{
		/* Drop any pending write-back entries (each owns one instance ref;
		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */
		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);
		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);
		sxu32 iRmw;
		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){
			SyBlobRelease(&aRmw[iRmw].sName);
			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);
		}
		SySetReset(&pVm->aHookRmw);
	}
	if( pVm->pMagicSetThis ){
		PH7_ClassInstanceUnref(pVm->pMagicSetThis);
		pVm->pMagicSetThis = 0;
	}
	SyBlobRelease(&pVm->sMagicSetName);
	if( pVm->pHookSetThis ){
		PH7_ClassInstanceUnref(pVm->pHookSetThis);
		pVm->pHookSetThis = 0;
	}
	pVm->pHookSetAttr = 0;
	pVm->nHookSetIdx = SXU32_HIGH;
	if( pVm->pMagicCallThis ){
		PH7_ClassInstanceUnref(pVm->pMagicCallThis);
		pVm->pMagicCallThis = 0;
	}
	pVm->pMagicCallClass = 0;
	SyBlobRelease(&pVm->sMagicCallName);
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
	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);
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
	/* (10) Re-mount the static/const attribute slots of every class. First
	 * invalidate every const/static slot index across ALL classes: the object
	 * pool was truncated, so the old indexes are stale, and the mount loop
	 * (plus the on-demand constant evaluator it can trigger) skips attributes
	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */
	{
		SyHashEntry *pEntry;
		SyHashResetLoopCursor(&pVm->hClass);
		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){
			ph7_class *pClass = (ph7_class *)pEntry->pUserData;
			ph7_class_attr *pAttr;
			SyHashEntry *pAttrEntry;
			SyHashResetLoopCursor(&pClass->hAttr);
			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;
				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
					pAttr->nIdx = SXU32_HIGH;
					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;
				}
			}
		}
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
#ifdef PH7_ENABLE_LIBXML
	/* Drop the libxml error queue and the previous request's documents */
	PH7_LibxmlVmReset(&(*pVm));
#endif
	pVm->iCmpCallbackExc = 0;
	pVm->bHaltRequested = 0;
	pVm->iExitStatus = 0;
	pVm->nSpreadCallBase = 0;
	VmSpreadCaptureReset(pVm);
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
#ifdef PH7_ENABLE_LIBXML
	/* Free the libxml document registry (libxml2 allocations live outside
	 * SyMemBackend, so the wholesale release below would leak them). */
	PH7_LibxmlVmRelease(pVm);
#endif
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
	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */
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
 * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or
 * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new
 * key must create a real global variable — linked into the bottom frame's
 * hVar and registered by reference in the $GLOBALS hashmap, exactly like a
 * variable created by top-level code — so later reads and writes alias one
 * slot. Called from the hashmap layer when an insertion targets pGlobal.
 *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy
 *     of pValue (NULL pValue nullifies), overwriting an existing global or
 *     superglobal in place.
 *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that
 *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING
 *     name is rejected with the engine's usual "already exists" diagnostic
 *     (the same limitation OP_STORE_REF has for plain variables).
 */
PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)
{
	VmFrame *pFrame = pVm->pFrame;
	SyHashEntry *pEntry;
	ph7_value *pObj;
	char *zDup;
	sxu32 nIdx;
	sxi32 rc;
	/* Walk down to the global frame */
	while( pFrame->pParent ){
		pFrame = pFrame->pParent;
	}
	/* An existing global (or superglobal) is overwritten in place */
	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);
	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){
		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:
		 * php creates an ordinary symbol-table entry named GLOBALS while the
		 * auto-global keeps resolving to the array. Fall through to the
		 * create-a-real-entry path (the hSuper lookup still wins for reads
		 * of $GLOBALS itself). */
		pEntry = 0;
	}
	if( pEntry == 0 ){
		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);
	}
	if( pEntry ){
		if( nRefIdx != SXU32_HIGH ){
			SyString sName;
			SyStringInitFromBuf(&sName,zName,nByte);
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);
			return SXRET_OK;
		}
		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));
		if( pObj == 0 ){
			return SXERR_NOTFOUND;
		}
		if( pValue ){
			PH7_MemObjStore(pValue,pObj);
		}else{
			PH7_MemObjToNull(pObj);
		}
		return SXRET_OK;
	}
	if( nRefIdx == SXU32_HIGH ){
		/* Reserve a fresh slot for the new global */
		pObj = PH7_ReserveMemObj(&(*pVm));
		if( pObj == 0 ){
			return SXERR_MEM;
		}
		nIdx = pObj->nIdx;
	}else{
		/* Reference assignment: bind the name to the existing slot */
		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);
		if( pObj == 0 ){
			return SXERR_NOTFOUND;
		}
		nIdx = nRefIdx;
	}
	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);
	if( zDup == 0 ){
		if( nRefIdx == SXU32_HIGH ){
			/* Return the reserved slot to the free pool (as VmExtractMemObj
			 * does) so an OOM here doesn't burn aMemObj slots. */
			VmSlot sFree;
			sFree.nIdx = nIdx;
			sFree.pUserData = 0;
			SySetPut(&pVm->aFreeObj,(const void *)&sFree);
		}
		return SXERR_MEM;
	}
	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));
	if( rc != SXRET_OK ){
		if( nRefIdx == SXU32_HIGH ){
			VmSlot sFree;
			sFree.nIdx = nIdx;
			sFree.pUserData = 0;
			SySetPut(&pVm->aFreeObj,(const void *)&sFree);
		}
		SyMemBackendFree(&pVm->sAllocator,zDup);
		return rc;
	}
	/* Register in the $GLOBALS array (by reference, like any global) */
	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);
	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);
	if( nRefIdx == SXU32_HIGH ){
		pObj->nIdx = nIdx;
		if( pValue ){
			PH7_MemObjStore(pValue,pObj);
		}
	}
	return SXRET_OK;
}
/*
 * Extract a variable value from the top active VM frame.
 * Return a pointer to the variable value on success.
 * NULL otherwise (non-existent variable/Out-of-memory,...).
 */
PH7_PRIVATE ph7_value * VmExtractMemObj(
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
	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty
	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses
	 * into an automatic index. $argv is built through here, so getting this wrong files
	 * every argument under "". */
	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);
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
		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */
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
	case PH7_VM_CONFIG_SERVER_ARGV: {
		/* php CLI exposes the script arguments in $_SERVER['argv'] and their
		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.
		 * Mirror the already-populated $argv array into $_SERVER once, after
		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */
		ph7_value *pArgv,*pServer;
		ph7_hashmap *pServerMap,*pArgvMap,*pDup;
		ph7_value sArgvVal,sKey,sCount;
		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);
		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);
		if( pArgv == 0 || (pArgv->iFlags & MEMOBJ_HASHMAP) == 0
		 || pServer == 0 || (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){
			rc = SXERR_NOTFOUND;
			break;
		}
		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;
		pServerMap = (ph7_hashmap *)pServer->x.pOther;
		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */
		pDup = PH7_NewHashmap(&(*pVm),0,0);
		if( pDup == 0 ){
			rc = SXERR_MEM;
			break;
		}
		PH7_HashmapDup(pArgvMap,pDup);
		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);
		PH7_MemObjInitFromString(&(*pVm),&sKey,0);
		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);
		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);
		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */
		PH7_MemObjRelease(&sKey);
		/* $_SERVER['argc'] = count($argv). */
		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);
		PH7_MemObjInitFromString(&(*pVm),&sKey,0);
		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);
		PH7_HashmapInsert(pServerMap,&sKey,&sCount);
		PH7_MemObjRelease(&sCount);
		PH7_MemObjRelease(&sKey);
		rc = SXRET_OK;
		break;
								  }
	case PH7_VM_CONFIG_INI_ENTRY: {
		/* A php.ini directive from the CLI (-d name=value or a -c file line).
		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs
		 * apply immediately so they take effect even if the script never
		 * touches the INI API. */
		const char *zName = va_arg(ap,const char *);
		const char *zValue = va_arg(ap,const char *);
		VmIniEntry sEntry;
		char *zDupN,*zDupV;
		sxu32 nName,nValue;
		if( SX_EMPTY_STR(zName) ){
			rc = SXERR_EMPTY;
			break;
		}
		if( zValue == 0 ){
			zValue = "";
		}
		nName = (sxu32)SyStrlen(zName);
		nValue = (sxu32)SyStrlen(zValue);
		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);
		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);
		if( zDupN == 0 || zDupV == 0 ){
			rc = SXERR_MEM;
			break;
		}
		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);
		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);
		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);
		if( rc == SXRET_OK ){
			if( nName == sizeof("error_reporting")-1
			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){
				sxi64 iLevel = 0;
				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);
				pVm->bErrReport = iLevel != 0;
			}else if( nName == sizeof("date.timezone")-1
			 && SyMemcmp(zName,"date.timezone",nName) == 0
			 && nValue == 3
			 && (SyStrnicmp(zValue,"UTC",3) == 0 || SyStrnicmp(zValue,"GMT",3) == 0) ){
				SyMemcpy(zValue,pVm->zDefTz,3);
				pVm->zDefTz[3] = 0;
				pVm->nDefTz = 3;
			}else if( nName == sizeof("zend.assertions")-1
			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){
				/* zend.assertions is a compile-time switch: 1 makes assert()
				 * active, 0 or -1 makes it a no-op. Applied here so it takes
				 * effect even before the INI chunk is seeded. */
				sxi64 iZend = 0;
				SyStrToInt64(zValue,nValue,(void *)&iZend,0);
				if( iZend >= 1 ){
					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;
				}else{
					pVm->iAssertFlags |= PH7_ASSERT_ZEND_OFF;
				}
			}
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
		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",
			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,
			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);
		if( rc != SXRET_OK ){
			/* Consumer routine request an operation abort */
			return rc;
		}
		++n;
		pInstr++; /* Next instruction in the stream */
	}
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
	int bSeenNamed = 0;
	char zErrMsg[256];
	SyZero(aUsed, nNonVariadic * sizeof(sxu8));
	for( i = 0; i < nActual; i++ ){
		aSlot[i] = -2;
	}
	for( i = 0; i < nActual; i++ ){
		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){
			/* Named argument — find formal by name */
			int found = 0;
			bSeenNamed = 1;
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
			/* Positional argument. Source-syntax calls can't reach here after a
			 * named arg (the parser rejects it at compile time), but a call
			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —
			 * can, so enforce PHP's rule at this shared choke point. */
			if( bSeenNamed ){
				VmThrowNamedArgError(&(*pVm),
					"Cannot use positional argument after named argument",
					sizeof("Cannot use positional argument after named argument") - 1);
				return PH7_ABORT;
			}
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
 * Shared OP_SPREAD expansion tail: replace the stack slot holding an
 * array-to-unpack with the map's elements in insertion order, capturing one
 * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both
 * the plain-array path (*ppTos holds the map value) and the materialized-Traversable
 * path (*ppTos holds the iterator object; pMap is the temp).
 * The map is kept alive across the walk — the stack slot may hold its ONLY
 * reference (literal / call-result temp), and releasing it mid-walk restores
 * the nodes' value slots to the freelist (the old open-coded copy then read
 * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A
 * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested
 * containers and blobs survive the trailing unref; a shared map keeps the
 * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).
 */
/*
 * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at
 * the first stack slot written (pFirst) plus one key entry per element, walked in
 * insertion order (string key -> named, integer key -> positional). Best-effort —
 * on OOM the capture is skipped and the call falls back to positional binding
 * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).
 */
static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)
{
	VmSpreadRun sRun;
	ph7_hashmap_node *pNode;
	sxu32 i;
	sRun.pStart = pFirst;
	sRun.nCount = nCount;
	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);
	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);
	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){
		return;
	}
	pNode = pMap->pFirst;
	for( i = 0; i < nCount && pNode; i++ ){
		VmSpreadKey sKey;
		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){
			/* String key -> named argument. Copy the bytes so the name survives
			 * the source map's release before CALL replays them. */
			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);
			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);
			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);
		}else{
			/* Integer key (or empty-string key, treated positionally) */
			sKey.nOff = 0;
			sKey.nLen = 0;
		}
		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);
		pNode = pNode->pPrev; /* forward link */
	}
}
/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left
 * intact — it is rebuilt (and its blob dependency retired) at the next build. */
static void VmSpreadCaptureReset(ph7_vm *pVm)
{
	SySetReset(&pVm->aSpreadRun);
	SySetReset(&pVm->aSpreadKey);
	SyBlobReset(&pVm->sSpreadKeyBlob);
}
/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)
 * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the
 * buffers to the enclosing call's state. A no-op when this call owns no run.
 * Every spread-bearing CALL/NEW MUST invoke this (directly, or via
 * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run
 * outlives the call in the VM-global buffers and corrupts a later call in the
 * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the
 * pre-computed run base (not a pStart scan) is what keeps an enclosing empty
 * `...[]` run — which shares its zero-width anchor with a nested call's base
 * slot — from being consumed by that nested call. */
static void VmSpreadConsume(ph7_vm *pVm)
{
	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);
	sxu32 rStart = pVm->nSpreadCallBase;
	VmSpreadRun *aRun;
	if( rStart >= nRun ){
		return; /* this call owns no run (none captured, or already consumed) */
	}
	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);
	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);
	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){
		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;
	}
	SySetTruncate(&pVm->aSpreadRun, rStart);
}
/*
 * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —
 * the captured spread runs anchored in this call's argument region at the top of
 * the stack. iP1 is the compile-time argument count; pTos points one past the
 * last pushed argument (the callable slot for CALL, the class-name slot for NEW).
 *
 * Walks the compile-time argument positions right-to-left, matching each against
 * the captured runs top-down: a position whose slots end at the current top is a
 * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the
 * current boundary is a `...[]` (one compile position, zero slots, -1 net); any
 * other position is a single ordinary slot. Stopping after iP1 positions leaves
 * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so
 * they are counted only by that call. This replaces the old shared
 * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another
 * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.
 *
 * ALSO records pVm->nSpreadCallBase — the index of the first run this walk
 * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by
 * that exact boundary rather than re-deriving it from pStart (which is ambiguous
 * for a zero-width `...[]` run that shares a nested call's base slot).
 */
static sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)
{
	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);
	VmSpreadRun *aRun;
	ph7_value *pEnd = pTos;
	sxi32 nPos = iP1;
	sxi32 ri, extra = 0;
	if( nRun == 0 ){
		pVm->nSpreadCallBase = 0;
		return 0;
	}
	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);
	ri = (sxi32)nRun - 1;
	while( nPos > 0 ){
		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){
			/* A non-empty unpack occupying nCount slots. */
			pEnd = aRun[ri].pStart;
			extra += (sxi32)aRun[ri].nCount - 1;
			ri--;
		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){
			/* An empty unpack (`...[]`): one compile position, zero slots. */
			extra -= 1;
			ri--;
		}else{
			/* An ordinary single-slot argument. */
			pEnd--;
		}
		nPos--;
	}
	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an
	 * enclosing call (or -1). This call's runs begin at ri+1. */
	pVm->nSpreadCallBase = (sxu32)(ri + 1);
	return extra;
}
static void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)
{
	ph7_value *pTos = *ppTos;
	sxu32 nEntry = pMap->nEntry;
	if( nEntry == 0 ){
		/* Nothing to unpack — remove the source from the stack */
		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */
		VmPopOperand(&pTos, 1);
	}else{
		ph7_hashmap_node *pNode;
		ph7_value *pElem;
		sxu32 i;
		int bTemp;
		pMap->iRef++;
		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */
		/* Record the run + element keys before any release (nodes still alive).
		 * pTos is the source slot, which becomes the first element's slot. */
		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);
		/* Overwrite the source slot with the first element */
		pNode = pMap->pFirst;
		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);
		PH7_MemObjRelease(pTos);
		if( pElem ){
			if( bTemp ){
				PH7_MemObjStore(pElem, pTos);
			}else{
				PH7_MemObjLoad(pElem, pTos);
			}
		}
		pTos->nIdx = SXU32_HIGH;
		/* Traverse in insertion order (pPrev is the forward link
		 * in PHL's circular doubly-linked hashmap node list). */
		pNode = pNode->pPrev;
		/* Push the remaining elements */
		for( i = 1; i < nEntry; i++ ){
			pTos++;
			PH7_MemObjInit(pVm, pTos);
			pTos->nIdx = SXU32_HIGH;
			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);
			if( pElem ){
				if( bTemp ){
					PH7_MemObjStore(pElem, pTos);
				}else{
					PH7_MemObjLoad(pElem, pTos);
				}
			}
			pNode = pNode->pPrev;
		}
		PH7_HashmapUnref(pMap);
	}
	*ppTos = pTos;
}
/*
 * Build the effective per-actual-slot argument-name map for a CALL/NEW whose
 * argument list contained an unpack (spread). `pCompile` is the compile-time
 * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe
 * the flattened actual arguments on the stack. Replays the captured spread runs +
 * element keys, interleaving them with the compile-time names at their real
 * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).
 *
 * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call
 * (an inner spread-bearing call runs between two of an outer call's spreads), so a
 * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs
 * below pArg belong to an enclosing, not-yet-built call and are skipped. Before
 * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,
 * restoring the buffers to the enclosing call's state — this is the per-call reset
 * (there is no global lazy flag). MUST run once per spread call, hence the caller
 * invokes it against the FINAL argument base (methods rebuild after their
 * method-name slot pop shifts pArg).
 *
 * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,
 * bHasNamed set) when at least one actual slot is named; returns 0 to keep the
 * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)
 * and the compile map (compile names); both stay valid until the next OP_SPREAD,
 * which is after this call's synchronous named-arg resolution.
 */
static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,
	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)
{
	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);
	VmSpreadRun *aRun;
	VmSpreadKey *aKey;
	const char *zKeyBase;
	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;
	int bAnyNamed = 0;
	sxu32 ai, ci, ri, rStart;
	if( nRun == 0 ){
		/* No spread captured at all — the compile map is already aligned. */
		return 0;
	}
	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);
	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);
	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);
	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below
	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —
	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run
	 * that shares this call's base slot from being mis-attributed here. */
	ri = pVm->nSpreadCallBase;
	rStart = ri;
	if( rStart >= nRun ){
		/* No run anchored in this call's argument region — nothing to realign. */
		return 0;
	}
	SySetReset(&pVm->aEffArgName);
	ci = 0;
	ai = 0;
	while( ai < nActual ){
		SyString sName;
		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */
		/* Empty runs (spread of []) anchored here consumed a compile arg but no
		 * slot — skip past their compile-name entry to keep alignment. */
		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){
			ci++; ri++;
		}
		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){
			/* A run of spread elements: one name per element from its key. Keys
			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)
			 * run never desyncs the key stream. */
			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;
			for( j = 0; j < K; j++ ){
				SyZero(&sName, sizeof(sName));
				if( aKey[ks + j].nLen > 0 ){
					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);
					bAnyNamed = 1;
				}
				SySetPut(&pVm->aEffArgName, (const void *)&sName);
			}
			ai += K;
			ci++; ri++;
		}else{
			/* Non-spread compile argument: carry its compile-time name (if any). */
			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){
				sName = pCompile->aNames[ci];
				bAnyNamed = 1;
			}
			SySetPut(&pVm->aEffArgName, (const void *)&sName);
			ai++;
			ci++;
		}
	}
	/* Consume this call's runs, restoring the buffers to the enclosing call's
	 * state. The just-built names still alias the (now logically-truncated) blob
	 * bytes until this call's synchronous resolution completes, before the next
	 * spread — the truncation only lowers the length, it does not free. */
	VmSpreadConsume(pVm);
	if( !bAnyNamed ){
		/* Every actual slot is positional — keep the fast positional path. */
		return 0;
	}
	pEff->bHasNamed = 1;
	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;
	pEff->bStrict = pCompile ? pCompile->bStrict : 0;
	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;
	pEff->nTotal = nActual;
	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);
	return 1;
}
/*
 * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument
 * name map (the built spread-key map when it contributes names, else the compile
 * map) AND guarantees this call's captured spread runs are consumed. The build is
 * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])`
 * — one compile arg, zero elements) still captured an empty run that MUST be
 * truncated, else it desyncs a later call in the VM-global buffers. So consume
 * unconditionally for a spread call when the build didn't run (after a build that
 * ran, VmSpreadConsume already fired and the second call is a harmless no-op).
 * pArg must be the site's FINAL argument base.
 */
static VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,
	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)
{
	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;
	if( pInstr->iP2 == 0 ){
		return pCompile; /* no spread: compile map already aligned, nothing captured */
	}
	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){
		return pStorage;
	}
	VmSpreadConsume(pVm);
	return pCompile;
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
 * In-flight magic-accessor guard (band A #3a) — php's property guard.
 * A __get body reading the SAME property of the SAME instance must not
 * re-enter __get (php falls back to the undefined-property path); entries
 * are pushed around the dispatch and popped after, so unrelated nested
 * reads (other names / other instances) still dispatch.
 */
static int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)
{
	VmMagicGuard *aG;
	sxu32 nHash;
	sxu32 n;
	if( SySetUsed(&pVm->aMagicGuard) == 0 ){
		/* Common case (no accessor in flight): skip the name hash entirely —
		 * every hooked-property access consults the guard, often twice. */
		return FALSE;
	}
	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);
	nHash = SyBinHash((const void *)pName->zString,pName->nByte);
	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){
		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){
			return TRUE;
		}
	}
	return FALSE;
}
static void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)
{
	VmMagicGuard sG;
	sG.pThis = pThis;
	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);
	sG.cKind = cKind;
	SySetPut(&pVm->aMagicGuard,(const void *)&sG);
}
static void VmMagicGuardPop(ph7_vm *pVm)
{
	(void)SySetPop(&pVm->aMagicGuard);
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
		case PH7_OP_STORE_REF:
			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */
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
 * Whether execution is currently INSIDE one of pName's own hook bodies on this
 * instance. php's rule: within ANY hook of property x (get or set alike),
 * `$this->x` addresses the raw backing store for BOTH reads and writes — so
 * every hook-dispatch decision checks both guard kinds, not just its own.
 */
static int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)
{
	return VmMagicGuardHeld(pVm,pThis,pName,'G') || VmMagicGuardHeld(pVm,pThis,pName,'S');
}
/*
 * Dispatch the SET side of a hooked-property write: php's read-only Error when
 * the property has no set hook, the asymmetric set-visibility check (php checks
 * it before the hook runs), then __phl_hook_set_NAME with pValue; a
 * `set => expr` hook's return value is stored into the BACKING slot through the
 * ordinary typed enforcement. Errors park on the boundary rail (the caller's
 * opcode completes benignly; the fetch-point router lands them). Shared by the
 * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)
 * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the
 * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's
 * abort path; SXRET_OK otherwise.
 */
static sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)
{
	char zHName[384];
	sxu32 nHName;
	ph7_class_method *pSetHook;
	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){
		/* get-only hooked property: php's read-only Error */
		SyBlob sErrMsg;
		SyBlobInit(&sErrMsg,&pVm->sAllocator);
		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",
			&pHThis->pClass->sName,&pHAttr->sName);
		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
		return SXRET_OK;
	}
	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET|PH7_CLASS_ATTR_PROTECTED_SET) ){
		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */
		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);
		if( rcVis != SXRET_OK ){
			VmBoundaryPark(&(*pVm),rcVis);
			return SXRET_OK;
		}
	}
	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);
	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);
	if( pSetHook ){
		ph7_value sHookRet;
		ph7_value *apHArg[1];
		apHArg[0] = pValue;
		PH7_MemObjInit(pVm,&sHookRet);
		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');
		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);
		VmMagicGuardPop(pVm);
		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)
		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){
			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);
			if( rcH == SXRET_OK ){
				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);
				if( pBack ){
					PH7_MemObjStore(&sHookRet,pBack);
				}
			}else if( rcH == PH7_ABORT ){
				PH7_MemObjRelease(&sHookRet);
				return PH7_ABORT;
			}
			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —
			 * the store is skipped, execution lands at the fetch point like any
			 * parked throw. */
		}
		PH7_MemObjRelease(&sHookRet);
	}
	return SXRET_OK;
}
/*
 * Consume the top pending hook-RMW entry if it targets slot nIdx — called from
 * the tail of every read-modify-write opcode (++/--/compound-assign) with the
 * slot it just wrote. A non-matching top (an ordinary variable RMW running
 * inside a nested exec while an outer write-back is pending) is left alone.
 * On match: copy the computed value out of the scratch slot, return the slot
 * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a
 * throw parked during the modify op, which wins (php: the exception discards
 * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to
 * propagate the enforcement abort, SXRET_OK otherwise.
 */
/*
 * Hook-aware attribute read for the C-side object walks — foreach over an
 * object, get_object_vars(), json_encode(), var_export(): php dispatches the
 * GET hook on these surfaces, while var_dump / (array) / print_r / serialize
 * read the raw backing store. Fills pOut (an initialized ph7_value the caller
 * owns) with the hook's return value and returns SXRET_OK — or the call's
 * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary
 * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property
 * has no get hook (or execution is inside one of its own hook bodies): the
 * caller reads the raw slot then.
 */
PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)
{
	ph7_vm *pVm = pThis->pVm;
	char zHName[384];
	sxu32 nHName;
	ph7_class_method *pGetHook;
	sxi32 rc;
	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0
	 || VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)
	 || pVm->nBoundaryRc != 0 ){
		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/
		 * foreach) from running FURTHER hooks after one already threw — php
		 * aborts the whole builtin at the first throw; the walk falls back to
		 * raw values whose output the routed throw then discards. */
		return SXERR_NOTFOUND;
	}
	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);
	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);
	if( pGetHook == 0 ){
		return SXERR_NOTFOUND;
	}
	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');
	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);
	VmMagicGuardPop(pVm);
	return rc;
}
/*
 * Release a hook-RMW SCRATCH slot: drop its contents and return the index to
 * the free pool. Scratch slots come from PH7_ReserveMemObj and are never
 * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.
 */
static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)
{
	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	VmSlot sFree;
	if( pScr ){
		PH7_MemObjRelease(pScr);
	}
	sFree.nIdx = nIdx;
	sFree.pUserData = 0;
	SySetPut(&pVm->aFreeObj,(const void *)&sFree);
}
/*
 * Drop the top pending write-back entry without dispatching its set side: the
 * arming statement was abandoned by a throw, or a ??= short-circuit jump
 * skipped its assign (php: the throw/skip discards the write). Releases the
 * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's
 * instance reference.
 */
static void VmHookRmwDropTop(ph7_vm *pVm)
{
	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
	if( pEnt == 0 ){
		return;
	}
	if( pEnt->nScratchIdx != SXU32_HIGH ){
		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);
	}
	SyBlobRelease(&pEnt->sName);
	PH7_ClassInstanceUnref(pEnt->pThis);
	(void)SySetPop(&pVm->aHookRmw);
}
static sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)
{
	VmHookRmw sEnt;
	VmHookRmw *pEnt;
	ph7_value *pScr;
	ph7_value sVal;
	sxi32 rc = SXRET_OK;
	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
	if( pEnt == 0 || pEnt->iKind != VM_HOOK_PEND_RMW || pEnt->nScratchIdx != nIdx ){
		return SXERR_NOTFOUND;
	}
	sEnt = *pEnt;
	(void)SySetPop(&pVm->aHookRmw);
	/* Copy the computed value out of the scratch slot, then free the slot
	 * (the set dispatch below may reserve slots — nothing may read the
	 * scratch index past this point). */
	PH7_MemObjInit(pVm,&sVal);
	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);
	if( pScr ){
		PH7_MemObjStore(pScr,&sVal);
	}
	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);
	sVal.nIdx = SXU32_HIGH;
	if( pVm->nBoundaryRc == 0 ){
		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);
	}
	PH7_MemObjRelease(&sVal);
	PH7_ClassInstanceUnref(sEnt.pThis);
	return rc;
}
/*
 * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending
 * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce
 * entry both funnel here. The guard makes a same-name write inside __set fall
 * through to creation, like php. Does NOT release the caller's reference.
 */
static void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)
{
	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);
	if( pSetMeth ){
		ph7_value sNameVal;
		ph7_value *apSetArg[2];
		PH7_MemObjInitFromString(pVm,&sNameVal,pName);
		sNameVal.nIdx = SXU32_HIGH;
		apSetArg[0] = &sNameVal;
		apSetArg[1] = pValue;
		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');
		PH7_VmCallClassMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);
		VmMagicGuardPop(pVm);
		PH7_MemObjRelease(&sNameVal);
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
/*
 * Remove pStep's pointer from the per-statement aStep set. The step being torn
 * down is not necessarily the last one pushed — two generator/fiber instances
 * of the same foreach suspend and finish out of LIFO order — so find it by
 * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's
 * top-down scan relies on the running activation's step (always the most-recent
 * push for its statement) sitting ABOVE any leaked older step that may share a
 * recycled frame address. A swap-with-last would move a newer step below such a
 * leaked step and let the scan match the stale one. A no-op if the step was
 * never linked (INIT error path).
 */
static void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)
{
	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
	sxu32 n = SySetUsed(&pInfo->aStep);
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		if( apStep[i] == pStep ){
			for( ; i + 1 < n ; ++i ){
				apStep[i] = apStep[i + 1];
			}
			(void)SySetPop(&pInfo->aStep);
			return;
		}
	}
}
static void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)
{
	if( pStep->pOwner ){
		PH7_ClassInstanceUnref(pStep->pOwner);
	}
	VmForeachStepUnlink(pInfo,pStep);
	SyMemBackendPoolFree(&pVm->sAllocator,pStep);
	PH7_ClassInstanceUnref(pThis);
}
/*
 * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the
 * map's active-step registry, free the step, optionally pop it off the info's
 * step stack, then drop the step's map reference. The single home for this
 * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is
 * load-bearing: a step freed while still registered is walked by the next
 * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident
 * class), and the unregister must precede the unref in case the step held the
 * map's last reference.
 */
static void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)
{
	ph7_hashmap *pMap = pStep->xIter.pMap;
	PH7_HashmapUnregisterForeachStep(pMap,pStep);
	if( bPop ){
		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber
		 * teardown may leave this step below newer ones on the shared aStep. */
		VmForeachStepUnlink(pInfo,pStep);
	}
	SyMemBackendPoolFree(&pVm->sAllocator,pStep);
	PH7_HashmapUnref(pMap);
}
/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */
/*
 * OP_SPREAD stack growth (removes the old VM_STACK_GUARD expansion cap).
 *
 * The operand stack of the CURRENTLY-RUNNING activation is about to receive more
 * spread elements than its remaining slack holds. Realloc the buffer so the
 * expansion — and the rest of the body's normal pushes — fit, then fix up every
 * pointer that aimed into the old buffer. Returns 1 on success (the caller may
 * proceed with the expansion), 0 on OOM (the caller keeps the old buffer and
 * raises the historical guard error, preserving pre-growth soundness).
 *
 * nNeed is the live-slot count the stack must hold after the pending expansion;
 * the grown capacity adds VM_STACK_GUARD back on top so the remainder of the body
 * keeps its slack. ONLY this activation's references move, and they are all here:
 *   - the dispatch locals pStack/pTos (via *ppStack / *ppTos) and the boundary
 *     copies in sState (pState->pStack/pTos + the tracked capacity nStackCap)
 *   - the owner slot: for a trampoline callee, its record's pFrameStack and the
 *     recycle capacity nStackCap; for the base activation (top-level, mini-program,
 *     coroutine body, callback), *ppBaseOwner / *pnBaseCap — whatever storage the
 *     native entry frees (a local, pVm->aOps, or pCtx->pStack/nStackCap)
 *   - aSpreadRun[].pStart entries anchored in the OLD buffer (this call's earlier
 *     spreads) — an unfixed pStart would desync PHP 8.1 named-arg replay after the
 *     realloc. Enclosing activations own DISTINCT buffers, so their runs (pStart
 *     outside [pOld, pOld+nOldCap)) are deliberately left untouched.
 */
static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,
	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,
	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)
{
	ph7_value *pOld = *ppStack;
	sxu32 nOldCap = pState->nStackCap;
	sxu32 nNewCap, nReq, nMaxCap, i, nRun;
	ph7_value *pNew;
	VmSpreadRun *aRun;
	/* Size the grown buffer to the post-expansion live depth (nNeed) PLUS the
	 * activation's original full budget (nStackOrig = nMaxStack + VM_STACK_GUARD) as
	 * headroom. That headroom is essential: only OP_SPREAD re-checks capacity, so any
	 * ordinary arg pushes that FOLLOW this spread in the same call (e.g.
	 * `foo(...$big, a1..aN)`) must fit — and the rest of the body adds at most
	 * nMaxStack above the current point. Crucially the headroom is relative to the
	 * ORIGINAL capacity, NOT the grown nOldCap: basing it on nOldCap would ratchet
	 * capacity up on every spread (nOldCap already includes prior growth), leaking
	 * without bound across statements that share one operand stack until it pins at
	 * nMaxCap. nNeed resets between statements (the stack pops back), so this does not. */
	nReq = nNeed + pState->nStackOrig;
	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */
		nReq = SXU32_HIGH;
	}
	/* SyMemBackendRealloc's size argument is sxu32, so the byte count
	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack
	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and
	 * the init loop below would run off it. If the true requirement exceeds the
	 * representable cap, treat it as OOM (the caller raises the guard error). */
	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);
	if( nReq > nMaxCap ){
		return 0;
	}
	if( nReq <= nOldCap ){
		return 1; /* already fits — no growth needed */
	}
	nNewCap = nReq;
	/* Amortize repeated spreads in one argument list: at least double, but never
	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't
	 * itself overflow.) */
	if( nNewCap < nOldCap * 2 ){
		sxu32 nDbl = nOldCap * 2;
		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }
		if( nNewCap < nDbl ){ nNewCap = nDbl; }
	}
	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,
		nNewCap * sizeof(ph7_value));
	if( pNew == 0 ){
		return 0; /* OOM: caller keeps pOld and raises the guard error */
	}
	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */
	for( i = nOldCap; i < nNewCap; i++ ){
		PH7_MemObjInit(pVm, &pNew[i]);
		pNew[i].nIdx = SXU32_HIGH;
	}
	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */
	*ppTos = pNew + (*ppTos - pOld);
	*ppStack = pNew;
	pState->pStack = pNew + (pState->pStack - pOld);
	pState->pTos = pNew + (pState->pTos - pOld);
	pState->nStackCap = nNewCap;
	if( pCallTop ){
		/* This activation is a trampoline callee: its record owns the buffer. */
		pCallTop->sCall.pFrameStack = pNew;
		pCallTop->sCall.nStackCap = nNewCap;
	}else{
		/* Base activation: the native entry frees *ppBaseOwner (and, for a
		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */
		if( ppBaseOwner ){ *ppBaseOwner = pNew; }
		if( pnBaseCap ){ *pnBaseCap = nNewCap; }
	}
	/* Re-anchor this activation's captured spread runs (pStart into the old
	 * buffer). Enclosing-activation runs live in other buffers — leave them. */
	nRun = SySetUsed(&pVm->aSpreadRun);
	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);
	for( i = 0; i < nRun; i++ ){
		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){
			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);
		}
	}
	return 1;
}
/*
 * Ensure the running activation's operand stack can hold a spread of nEntry
 * elements pushed at *ppTos (the source slot becomes the first element, so the
 * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.
 * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.
 */
static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,
	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,
	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)
{
	sxu32 nNeed;
	if( nEntry == 0 ){
		return 1; /* empty spread never grows the stack */
	}
	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);
	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,
		pCallTop, ppBaseOwner, pnBaseCap);
}
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
	 * or free it if the pool is full. Its allocated size is tracked in
	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew
	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)
	 *
	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,
	 * where a deep suspend parks its whole record segment before reaching here)
	 * would leave the callee stack owned by the suspended ctx, so recycling it
	 * would hand a live fiber's operand stack to the next call. The guard keeps
	 * that invariant explicit and robust to future coroutine changes. */
	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){
		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee
		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly
		 * the allocated slot count either way. */
		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);
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
	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,
	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);
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
PH7_PRIVATE sxi32 VmByteCodeExec(
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
	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */
	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */
	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */
	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */
	)
{
	sxi32 rc;
	sxi32 nSavedBrc;
	sxu32 nSavedLine;
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	/* A fresh native exec entered while a C-boundary throw is parked
	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the
	 * very opcode that swallowed the throw) must run CLEAN: the parked status
	 * belongs to the interrupted outer exec's fetch-point router, not to this
	 * one. Save+clear on entry, merge back on exit — the outer status is
	 * restored unless this exec parked its own unconsumed (newer) one, with
	 * PH7_ABORT dominating either way. */
	nSavedBrc = pVm->nBoundaryRc;
	pVm->nBoundaryRc = 0;
	/* The executing source line belongs to the ACTIVATION. A nested body -- a called
	 * function, but equally an attribute-default or default-argument mini-program --
	 * runs its own bytecode with its own lines, so it must not leave the caller
	 * reporting the callee's position: `new Exception` stamped line 1 because the
	 * class's `protected $message = '';` default ran (from the embedded chunk) between
	 * OP_NEW and the stamp. Save on entry, restore on exit. */
	nSavedLine = pVm->nCurLine;
	pVm->nVmExecDepth++;
	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,
		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);
	pVm->nVmExecDepth--;
	pVm->nCurLine = nSavedLine;
	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT || pVm->nBoundaryRc == 0) ){
		pVm->nBoundaryRc = nSavedBrc;
	}
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
	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */
	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */
	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */
	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */
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
	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */
	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */
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
/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the
 * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */
#define VM_EXIT_BREAK break
#define VM_EXIT_ABORT goto Abort
#define VM_EXIT_EXCEPTION goto Exception
#include "vm_dispatch.h"
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
	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.
	 *
	 * Exception: when this body is suspended mid `yield from` over an inner Generator
	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded
	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected
	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected
	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame
	 && pVm->pActiveCtx->iDelegateState != 3 ){
		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;
		VmFrame *pThrowFrame;
		sxi32 iResumePc;
		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */
		/* Raising the injection at the yield-from point abandons any array/Iterator
		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).
		 * Tear the delegate down so that if the generator's own try catches this and
		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh
		 * instead of resuming this now-stale delegate cursor. */
		if( pVm->pActiveCtx->iDelegateState != 0 ){
			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);
			pVm->pActiveCtx->pDelegateNode = 0;
			pVm->pActiveCtx->iDelegateState = 0;
		}
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
	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its
	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight
	 * into the innermost open try's finally as if a `return` had crossed every
	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the
	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the
	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing
	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a
	 * yield reached inside one of these finallys. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing
	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame
	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){
		VmFinallyAction sAct;
		sxu32 iFpc = 0;
		int nCross = -1; /* cross every enclosing finally of this body */
		SyZero(&sAct,sizeof(sAct));
		sAct.eKind = PH7_FA_RETURN;
		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);
		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */
		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
			sAct.nCross = nCross;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */
		}else{
			/* No open try had a finally: nothing to run, complete the body. */
			PH7_MemObjRelease(&sAct.sRet);
			goto Done;
		}
	}
	/* Execute as much as we can */
	for(;;){
VmLoopFetch:
		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C
		 * site with no status channel — a __toString/__toInt cast, __get/__set/
		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a
		 * builtin — raised, and the site continued with a fallback value; the
		 * invocation boundary parked the status here (VmBoundaryPark). Route it
		 * exactly as the throw site's dispatch macro would have: abort, land at
		 * an inline-try redirect, resume at a recorded in-place catch (draining
		 * the abandoned mid-expression operands to the catching try's base), or
		 * propagate out of this exec. Checked at the fetch point, so a swallowed
		 * throw outlives at most the C remainder of ONE opcode instead of
		 * silently resuming the surrounding PHP code with a bogus value.
		 * The pending write-back sweep shares this one guard so the hot
		 * no-hooks path pays a single predicted branch per fetch. */
		if( pVm->nBoundaryRc != 0 || SySetUsed(&pVm->aHookRmw) > 0 ){
			if( pVm->nBoundaryRc != 0 ){
				sxi32 rcBr = pVm->nBoundaryRc;
				pVm->nBoundaryRc = 0;
				if( rcBr == PH7_ABORT ){
					goto Abort;
				}
				if( pVm->pInlineInstr == (void *)aInstr ){
					/* Caught by an inline try (generator body) THIS exec owns: drain
					 * and land (pre-fetch path: pc is used directly, no trailing ++). */
					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){
						PH7_MemObjRelease(pTos);
						pTos--;
					}
					pc = (sxi32)pVm->iInlinePc;
					pVm->pInlineInstr = 0;
				}else{
					sxi32 iBrPc;
					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){
						/* Caught in place by a try THIS exec owns: drain the abandoned
						 * operands to the catching try's base and land at its pad
						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;
						 * pre-fetch here, so +1 lands on the pad itself). */
						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){
							PH7_MemObjRelease(pTos);
							pTos--;
						}
						pc = iBrPc + 1;
					}else{
						/* Caught by an outer exec (or uncaught-with-report pending):
						 * unwind out of this exec; the owner's macros/router land it. */
						goto Exception;
					}
				}
			}
			/* Stale write-back sweep: a pending entry whose OWNING activation is
			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for
			 * an RMW entry (window = the modify op alone) a routed throw abandoned
			 * the arming statement mid-flight; for a ??= entry either a throw
			 * abandoned the RHS or the short-circuit jump landed past the
			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch
			 * (php: the throw/skip discards the write). A nested exec (even a
			 * recursive one over the same bytecode) has a different operand-stack
			 * base and leaves enclosing entries alone; entries below a live top
			 * are reached as the drops expose them. */
			while( SySetUsed(&pVm->aHookRmw) > 0 ){
				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
				if( pTopRmw->pOwnerStack != (void *)pStack || pTopRmw->pInstrs != (void *)aInstr
				 || ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){
					break; /* not ours, or legitimately in flight */
				}
				VmHookRmwDropTop(&(*pVm));
			}
		}
		/* Fetch the instruction to execute */
		pInstr = &aInstr[pc];
		if( pInstr->nLine ){
			/* Publish the source position for diagnostics, debug_backtrace() and
			 * Throwable. Instructions the compiler could not attribute (nLine 0)
			 * leave the last known line standing rather than reporting line 0. */
			pVm->nCurLine = pInstr->nLine;
		}
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
case PH7_OP_UNSET_VAR: {
	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */
	SyString *pName = (SyString *)pInstr->p3;
	if( pName && pVm->pFrame ){
		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body
		 * frame below it, so skip past it exactly as every other variable path does.
		 * Without this, unset($x) inside a try silently found nothing and did nothing. */
		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);
		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);
		if( rcU == PH7_ABORT ){
			goto Abort;
		}
		/* Releasing the last holder can run a __destruct(), and that destructor may
		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it
		 * here and route it, or the catch runs and execution resumes inside the try
		 * ("resumed-dtor" instead of php's "caught-dtor"). */
		if( pVm->nBoundaryRc != 0 ){
			rc = pVm->nBoundaryRc;
			pVm->nBoundaryRc = 0;
			if( rc == PH7_ABORT ){
				goto Abort;
			}
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
	}
	break;
					   }
case PH7_OP_ERR_CTRL:
	/*
	 * Error-control operator '@'. Emitted as a PAIR around the suppressed
	 * expression: iP1=1 opens the window (before the operand is evaluated),
	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),
	 * which went unnoticed only because the engine raised so few diagnostics;
	 * every warning/notice/deprecation the parity work added leaked straight
	 * through `@`. The window nests, and php 8 does NOT let '@' swallow
	 * fatals or exceptions — those unwind past the closing instruction, so
	 * the depth is reset on the exception path rather than decremented here.
	 */
	if( pInstr->iP1 ){
		pVm->nErrSuppress++;
	}else if( pVm->nErrSuppress > 0 ){
		pVm->nErrSuppress--;
	}
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
				pClass = PH7_VmResolveParentClass(&(*pVm));
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
	if( pInstr->iP1 & PH7_LOADC_NOKEY ){
		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */
		MemObjSetType(pTos,MEMOBJ_NULL);
		SyBlobReset(&pTos->sBlob);
		pTos->iFlags |= MEMOBJ_AUX_NOKEY;
		pTos->nIdx = SXU32_HIGH;
		break;
	}
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
						VmExpandConstantWithNotice(&(*pVm),pCons,pTos);
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
				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);
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
						VmExpandConstantWithNotice(&(*pVm),pCons,pTos);
						pTos->nIdx = SXU32_HIGH;
						break;
					}
					/* Not in current namespace either — fall through to global/string */
				}
				{
					/*
					 * php 8 has no bare-word fallback: an unresolved constant is a catchable
					 * Error, not its own name as a string. PH7 answered "X" for an unknown
					 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —
					 * silently became a string and flowed on.
					 *
					 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call
					 * boundary, so neither a bare `break` nor `goto Exception` is correct
					 * here (see the macro).
					 */
					SyBlob sMsg;
					SyBlobInit(&sMsg,&pVm->sAllocator);
					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);
					MemObjSetType(pTos,MEMOBJ_NULL);
					SyBlobReset(&pTos->sBlob);
					pTos->nIdx = SXU32_HIGH;
					rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
						SyBlobLength(&sMsg));
					SyBlobRelease(&sMsg);
					if( rc == SXERR_ABORT ){
						goto Abort;
					}
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
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
				/* An explicit key in an array LITERAL gets the same php diagnostics a
				 * subscript does — a float key that truncates deprecates, and so does an
				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to
				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that
				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the
				 * MEMOBJ_NULL check below is what tells the two apart. */
					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){
						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */
						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;
						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0
							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;
						if( bNull || bLossyFloat ){
							const char *zErr = bNull ? "Cannot access offset of type null on array"
							                         : "Cannot access offset of type float on array";
							SyBlob sErrMsg;
							SyBlobInit(&sErrMsg,&pVm->sAllocator);
							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
						}
					}
				/* Standard insertion */
				PH7_HashmapInsert(pMap,
					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,
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
			sxi64 iOfft;
			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);
			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){
				/* Force an int cast */
				PH7_MemObjToInteger(pIdx);
			}
			iOfft = pIdx->x.iVal;
			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last
			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge
			 * number, ran past the end and quietly produced NULL. */
			if( iOfft < 0 ){
				iOfft += nLen;
			}
			if( iOfft < 0 || iOfft >= nLen ){
				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load
				 * NULL there; everywhere else it WARNS and yields the empty string (PH7
				 * silently produced NULL in both cases). */
				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the
				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are
				 * lookups and must stay silent. */
				int bQuiet = pInstr->iP2 == 4 || pInstr->iP2 == 5 || pInstr->iP2 == 6;
				PH7_MemObjRelease(pTos);
				if( bQuiet ){
					MemObjSetType(pTos,MEMOBJ_NULL);
				}else{
					MemObjSetType(pTos,MEMOBJ_STRING);
					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",
						pIdx->x.iVal);
				}
			}else{
				const char *zData = (const char *)SyBlobData(&pTos->sBlob);
				int c = zData[iOfft];
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
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			if( rc == SXERR_ABORT ){ goto Abort; }
			/* `break` used to resume at the NEXT instruction: the catch ran and then
			 * execution carried on inside the try block. */
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
	}
	if( (pInstr->iP2 == 1 || pInstr->iP2 == 3 || pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				/* php 8 write-context auto-vivify rules: NULL converts to array
				 * silently; FALSE converts with the 8.1 deprecation; any other
				 * scalar base — int/float/true/resource — is php's catchable
				 * "Cannot use a scalar value as an array" Error and the variable
				 * stays untouched (pre-fix the base was silently CONVERTED,
				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string
				 * bases were intercepted by the string-offset paths above). */
				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL
				 * rejects it like any other scalar base (null still auto-vivifies —
				 * it is not a bool). */
				if( (pObj->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_RES|MEMOBJ_BOOL)) != 0 ){
					SyBlob sErrMsg;
					SyBlobInit(&sErrMsg,&pVm->sAllocator);
					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",
						sizeof("Cannot use a scalar value as an array")-1);
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
					if( pIdx ){
						PH7_MemObjRelease(pIdx);
					}
					PH7_MemObjRelease(pTos);
					pTos->nIdx = SXU32_HIGH;
					break;
				}
				PH7_MemObjToHashmap(pObj);
				PH7_MemObjLoad(pObj,pTos);
			}
		}
	}
	rc = SXERR_NOTFOUND; /* Assume the index is invalid */
	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /
	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays
	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */
	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx
	 && (pInstr->iP2 == 0 || pInstr->iP2 == 1) ){
		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;
		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0
			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;
		if( bNull || bLossyFloat ){
			SyBlob sErrMsg;
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			SyBlobAppend(&sErrMsg,
				bNull ? "Cannot access offset of type null on array"
				      : "Cannot access offset of type float on array",
				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"
				                      : "Cannot access offset of type float on array"));
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
			PH7_MemObjRelease(pIdx);
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH;
			break;
		}
	}
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
	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 || pInstr->iP2 == 0)
	 && (pTos->iFlags & MEMOBJ_HASHMAP)
	 && !((pInstr+1)->iOp == PH7_OP_NULLC || (pInstr+1)->iOp == PH7_OP_NULLC_JMP) ){
		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed
		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay
		 * silent (same guard the magic-accessor read path uses). */
		/* php warns when a missing key is READ (iP2 == 0) or destructured
		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context
		 * vivification (iP2 == 1) stay silent, as does a read on a non-array
		 * base (already diagnosed above). php prints an INT key bare and a
		 * STRING key quoted. */
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		if( pIdx->iFlags & (MEMOBJ_STRING|MEMOBJ_BOOL|MEMOBJ_NULL) ){
			SyString sKey;
			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){
				PH7_MemObjToString(pIdx);
			}
			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));
			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);
		}else{
			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){
				PH7_MemObjToInteger(pIdx);
			}
			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);
		}
		SyBlobNullAppend(&sMsg);
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_STRING|MEMOBJ_OBJ)) == 0
	 && (pInstr->iP2 == 0 || pInstr->iP2 == 2) ){
		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset
		 * on int") that yields NULL. PH7 yielded NULL in silence. */
		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",
			VmArithTypeName(pTos));
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
	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE
	 * (its env is empty), yet php still binds the creation-site class as its scope
	 * so `self::`/private access inside the body works. Detect the enclosing class
	 * here and route such a lambda through the per-instance closure path (a global
	 * no-capture lambda peeks NULL and stays a shared function). */
	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);
	if( (pFunc->iFlags & VM_FUNC_CLOSURE) || pLoadScope ){
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
		/* An in-class no-capture lambda routed here (pLoadScope set) must be a
		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */
		pClosure->iFlags |= VM_FUNC_CLOSURE;
		pClosure->pUserData = pFunc->pUserData;
		pClosure->sSignature = pFunc->sSignature;
		pClosure->nReturnType = pFunc->nReturnType;
		pClosure->sReturnClass = pFunc->sReturnClass;
		pClosure->aReturnUnion = pFunc->aReturnUnion;
		pClosure->sReturnTypeName = pFunc->sReturnTypeName;
		pClosure->bStrictTypes = pFunc->bStrictTypes;
		pClosure->nMaxStack = pFunc->nMaxStack;
		if( pClosure->pUserData == 0 ){
			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):
			 * a closure made in a method — or in another closure, whose own stamp
			 * the peek reads — resolves self::/parent:: against it like php. */
			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);
		}
		/* Capture the creation-site late-static-binding class so `static::` inside
		 * the closure body resolves like php (the "called class", which may differ
		 * from the declaring scope stamped above — e.g. a closure made in an
		 * inherited method). A closure made outside any class captures NULL. */
		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);
		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/
		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */
		pClosure->aAttrs = pFunc->aAttrs;
		pClosure->sDoc = pFunc->sDoc;
		pClosure->sFile = pFunc->sFile;
		pClosure->nLine = pFunc->nLine;
		pClosure->nEndLine = pFunc->nEndLine;
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
			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF
			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1
				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){
				/* Capture by reference: bind the env entry to the variable's
				 * memory slot — creating a fresh null variable when missing,
				 * as php does (`use (&$f)` before $f is assigned) — and pin
				 * the slot past the creating frame's teardown so the closure
				 * can outlive its birth scope. The call-time env install
				 * aliases the name to this slot instead of copying a value. */
				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);
				if( pValue ){
					sEnv.nIdx = pValue->nIdx;
					VmPinMemObjSlot(pVm,pValue->nIdx);
				}
			}else{
				/* Standard pass by value */
				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);
				if( pValue ){
					/* Copy imported value */
					PH7_MemObjStore(pValue,&sEnv.sValue);
				}
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
		if( pVm->pMagicSetThis ){
			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain
			 * store to a missing/inaccessible property on a class declaring __set.
			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on
			 * the stack as the assignment expression's result — php's semantics
			 * (no property is created; a throw rides the boundary rail). */
			ph7_class_instance *pSetThis = pVm->pMagicSetThis;
			SyString sSetName;
			pVm->pMagicSetThis = 0;
			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));
			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);
			PH7_ClassInstanceUnref(pSetThis);
			SyBlobReset(&pVm->sMagicSetName);
			break;
		}
		if( pVm->pHookSetThis ){
			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a
			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with
			 * the rvalue (which stays on the stack as the assignment expression's
			 * result); a `set => expr` hook's return value is stored into the
			 * BACKING slot with the ordinary typed enforcement. A property with
			 * only a get hook is php's catchable "is read-only" Error. */
			ph7_class_instance *pHThis = pVm->pHookSetThis;
			ph7_class_attr *pHAttr = pVm->pHookSetAttr;
			sxu32 nBackIdx = pVm->nHookSetIdx;
			sxi32 rcHs;
			pVm->pHookSetThis = 0;
			pVm->pHookSetAttr = 0;
			pVm->nHookSetIdx = SXU32_HIGH;
			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);
			PH7_ClassInstanceUnref(pHThis);
			if( rcHs == PH7_ABORT ){
				goto Abort;
			}
			break;
		}
		if( nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");
			pTos->nIdx = SXU32_HIGH;
		}else{
			/* Enforce typed property declaration if any. May coerce the
			 * incoming value in place (weak mode) or throw TypeError. */
			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);
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
	if( sName.nByte == sizeof("GLOBALS")-1
	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){
		if( pInstr->p3 ){
			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time
			 * fatal there; raised at the store site here with the same
			 * message and the same non-catchable outcome). Element writes
			 * are unaffected. */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
			pVm->iExitStatus = 255;
			pVm->bHaltRequested = 1;
			goto Abort;
		}
		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's
		 * compile-time case: php quietly creates an ordinary symbol-table
		 * entry named GLOBALS, leaving the auto-global view intact. */
		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);
		PH7_MemObjRelease(&pTos[1]);
		break;
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
		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /
		 * normalizes to ""); PHL rejects it. */
		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){
			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;
			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0
				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;
			if( bNull || bLossyFloat ){
				sxi32 rcSc;
				const char *zErr = bNull ? "Cannot access offset of type null on array"
				                         : "Cannot access offset of type float on array";
				PH7_MemObjRelease(pKey);
				VmPopOperand(&pTos,1);
				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));
				if( rcSc == SXERR_ABORT ){ goto Abort; }
				rc = rcSc;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
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
				PH7_THROW_ROUTE_MIDEXPR(rc)
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
				sxi64 iOfft;
				sxi64 nLen;
				if((pKey->iFlags & MEMOBJ_INT) == 0 ){
					/* Force an int cast */
					PH7_MemObjToInteger(pKey);
				}
				iOfft = pKey->x.iVal;
				nLen = (sxi64)SyBlobLength(&pObj->sBlob);
				if( iOfft < 0 ){
					/* php 7.1: a negative offset writes back from the end. */
					iOfft += nLen;
					if( iOfft < 0 ){
						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",
							pKey->x.iVal);
						PH7_MemObjRelease(pKey);
						break;
					}
				}
				if( SyBlobLength(&pTos->sBlob) > 0 ){
					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);
					if( SyBlobLength(&pTos->sBlob) > 1 ){
						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
							"Only the first byte will be assigned to the string offset");
					}
					if( iOfft >= nLen ){
						/* php PADS WITH SPACES up to the offset. PH7 simply appended the
						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"
						 * -- a silently wrong string. */
						sxi64 nPad;
						for( nPad = nLen ; nPad < iOfft ; ++nPad ){
							SyBlobAppend(&pObj->sBlob," ",sizeof(char));
						}
						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));
					}else{
						char *zData = (char *)SyBlobData(&pObj->sBlob);
						zData[iOfft] = zBlob[0];
					}
				}
			}
			if( pKey ){
			  PH7_MemObjRelease(pKey);
			}
			break;
		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an
			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value
			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */
			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL
			 * rejects any scalar base, false included (null still auto-vivifies). */
			int bScalar = (pObj->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_RES|MEMOBJ_BOOL)) != 0;
			if( bScalar ){
				sxi32 rcSc;
				if( pKey ){
					PH7_MemObjRelease(pKey);
				}
				VmPopOperand(&pTos,1);
				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",
					sizeof("Cannot use a scalar value as an array")-1);
				if( rcSc == SXERR_ABORT ){ goto Abort; }
				rc = rcSc;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
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
		if( pMap == pVm->pGlobal ){
			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's
			 * slot; an append has no name to bind (catchable Error). */
			if( pKey == 0 ){
				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));
			}else{
				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
					PH7_MemObjToString(pKey);
				}
				if( SyBlobLength(&pKey->sBlob) < 1 ){
					/* Pathological empty name: keep the legacy diagnostic */
					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,
						"$GLOBALS is a read-only array,insertion is forbidden");
					rc = SXRET_OK;
				}else{
					rc = PH7_VmInstallGlobalVar(&(*pVm),
						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),
						0,pTos->nIdx);
				}
			}
		}else{
			/* Insertion by reference */
			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);
		}
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
	/* A hooked property whose get hook returned an array/object/resource:
	 * php's TypeError, raised BEFORE any set dispatch (the type guard below
	 * would skip the mutation and the tail write-back would otherwise call
	 * the set hook with the unchanged value). */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) != 0
	 && SySetUsed(&pVm->aHookRmw) > 0 ){
		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
		if( pTopInc->iKind == VM_HOOK_PEND_RMW && pTopInc->nScratchIdx == pTos->nIdx ){
			SyBlob sErrMsg;
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				SyBlobAppend(&sErrMsg,"Cannot increment array",sizeof("Cannot increment array")-1);
			}else if( pTos->iFlags & MEMOBJ_OBJ ){
				SyBlobFormat(&sErrMsg,"Cannot increment %z",
					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);
			}else{
				SyBlobAppend(&sErrMsg,"Cannot increment resource",sizeof("Cannot increment resource")-1);
			}
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
			VmHookRmwDropTop(&(*pVm));
			pTos->nIdx = SXU32_HIGH;
			break;
		}
	}
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				if( VmStringWantsPerlIncr(pObj) ){
					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric
					 * string (it points at str_increment() instead); PHL rejects it. */
					SyBlob sErrMsg;
					SyBlobInit(&sErrMsg,&pVm->sAllocator);
					SyBlobAppend(&sErrMsg,
						"Increment on a non-numeric string is not supported, use str_increment() instead",
						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
					VmHookRmwDropTop(&(*pVm));
					pTos->nIdx = SXU32_HIGH;
					break;
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
						/* PHP promotes PHP_INT_MAX++ to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;
							MemObjSetType(pObj,MEMOBJ_REAL);
#else
							pObj->x.iVal = r;
#endif
						}else{
							pObj->x.iVal = r;
						}
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
						/* PHP promotes PHP_INT_MAX++ to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;
							MemObjSetType(pTos,MEMOBJ_REAL);
#else
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
#endif
						}else{
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
						}
					}
				}
			}
		}
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);
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
	/* A hooked property whose get hook returned an array/object/resource:
	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —
	 * `--` on null is php's no-op and its write-back still dispatches set). */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) != 0
	 && SySetUsed(&pVm->aHookRmw) > 0 ){
		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
		if( pTopDec->iKind == VM_HOOK_PEND_RMW && pTopDec->nScratchIdx == pTos->nIdx ){
			SyBlob sErrMsg;
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				SyBlobAppend(&sErrMsg,"Cannot decrement array",sizeof("Cannot decrement array")-1);
			}else if( pTos->iFlags & MEMOBJ_OBJ ){
				SyBlobFormat(&sErrMsg,"Cannot decrement %z",
					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);
			}else{
				SyBlobAppend(&sErrMsg,"Cannot decrement resource",sizeof("Cannot decrement resource")-1);
			}
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
			VmHookRmwDropTop(&(*pVm));
			pTos->nIdx = SXU32_HIGH;
			break;
		}
	}
	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op) -- but 8.3
	 * deprecates that no-op, same as the non-numeric-string one below. */
	if( pTos->iFlags & MEMOBJ_NULL ){
		/* E_WARNING, not E_DEPRECATED -- php reports this one at errno 2. */
		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
			"Decrement on type null has no effect, this will change in the next major version of PHP");
	}
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				if( VmStringWantsPerlIncr(pObj) ){
					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string
					 * (php has no string decrement); PHL rejects it. */
					SyBlob sErrMsg;
					SyBlobInit(&sErrMsg,&pVm->sAllocator);
					SyBlobAppend(&sErrMsg,
						"Decrement on a non-numeric string is not supported",
						sizeof("Decrement on a non-numeric string is not supported")-1);
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
					VmHookRmwDropTop(&(*pVm));
					pTos->nIdx = SXU32_HIGH;
					break;
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
						/* PHP promotes PHP_INT_MIN-- to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;
							MemObjSetType(pObj,MEMOBJ_REAL);
#else
							pObj->x.iVal = r;
#endif
						}else{
							pObj->x.iVal = r;
						}
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
						/* PHP promotes PHP_INT_MIN-- to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;
							MemObjSetType(pTos,MEMOBJ_REAL);
#else
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
#endif
						}else{
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
						}
					}
				}
			}
		}
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);
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
		if( pTos->x.iVal == SMALLEST_INT64 ){
			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a
			 * REAL representation is already present it is the negated
			 * authoritative value, so just drop the now-stale cached int. The
			 * integer-only build has no float type, so it wraps (two's
			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */
#ifndef PH7_OMIT_FLOATING_POINT
			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
				pTos->rVal = -(ph7_real)pTos->x.iVal;
				MemObjSetType(pTos,MEMOBJ_REAL);
			}else{
				pTos->iFlags &= ~MEMOBJ_INT;
			}
#else
			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);
#endif
		}else{
			pTos->x.iVal = -pTos->x.iVal;
		}
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
	/* Force an integer cast (php deprecates a lossy float here too) */
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"*",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
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
		/* Integer arithmetic; PHP promotes an overflowing product to float.
		 * The integer-only build wraps like OP_POW's OMIT path. */
		sxi64 a,b,r;
		a = pNos->x.iVal;
		b = pTos->x.iVal;
		if( PH7_MUL_OVERFLOW64(a,b,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
			pNos->rVal = (ph7_real)a * (ph7_real)b;
			MemObjSetType(pNos,MEMOBJ_REAL);
#else
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
#endif
		}else{
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
		}
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"**",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
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
				if( PH7_MUL_OVERFLOW64(result_i, cur_base, &result_i) ){
					overflow = 1;
					break;
				}
			}
			cur_exp >>= 1;
			if( cur_exp > 0 ){
				if( PH7_MUL_OVERFLOW64(cur_base, cur_base, &cur_base) ){
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
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
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Perform the addition */
	nIdx = pTos->nIdx;
	if( nIdx == pVm->nGlobalIdx ){
		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment
		 * (a compile-time fatal in php; raised here, same message). */
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
		pVm->iExitStatus = 255;
		pVm->bHaltRequested = 1;
		goto Abort;
	}
	PH7_MemObjAdd(pTos,pNos,TRUE);
	/* Peform the store operation */
	if( nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);
		PH7_MemObjStore(pTos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(nIdx,0);
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
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"-",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Force the operands to be numeric. Without this a string operand fell through
	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"
	 * quietly evaluated to 0. */
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
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
		/* Integer arithmetic; PHP promotes an overflowing difference to float.
		 * The integer-only build wraps like OP_POW's OMIT path. */
		sxi64 a,b,r;
		a = pNos->x.iVal;
		b = pTos->x.iVal;
		if( PH7_SUB_OVERFLOW64(a,b,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
			pNos->rVal = (ph7_real)a - (ph7_real)b;
			MemObjSetType(pNos,MEMOBJ_REAL);
#else
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
#endif
		}else{
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
		}
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
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"-",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Force the operands to be numeric (see OP_SUB) */
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
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
		/* Integer arithmetic; PHP promotes an overflowing difference to float.
		 * The integer-only build wraps like OP_POW's OMIT path. */
		sxi64 a,b,r;
		a = pTos->x.iVal;
		b = pNos->x.iVal;
		if( PH7_SUB_OVERFLOW64(a,b,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
			pNos->rVal = (ph7_real)a - (ph7_real)b;
			MemObjSetType(pNos,MEMOBJ_REAL);
#else
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
#endif
		}else{
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
		}
	}
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
		PH7_MemObjStore(pNos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"%",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */
	}else if( b == -1 ){
		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed
		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU
		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the
		 * remainder. php's result here is 0. */
		r = 0;
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
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"%",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */
	}else if( b == -1 ){
		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap
		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */
		r = 0;
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"/",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	ph7_real a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* php's `/`: an int/int division whose remainder is 0 yields an *int*
	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact
	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.
	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which
	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
	if( ((pTos->iFlags|pNos->iFlags) & MEMOBJ_REAL) == 0 ){
		sxi64 ia = pNos->x.iVal;
		sxi64 ib = pTos->x.iVal;
		if( ib == 0 ){
			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");
			PH7_DISPATCH_ENFORCE_RC(rc)
		}else if( ia % ib == 0 && !(ib == -1 && ia == SMALLEST_INT64) ){
			pNos->x.iVal = ia / ib;
			MemObjSetType(pNos,MEMOBJ_INT);
			VmPopOperand(&pTos,1);
			break;
		}
	}
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
		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
	}else{
		r = a/b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
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
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
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
		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
		/* A hooked lvalue's scratch slot was appended in place — dispatch the
		 * set side now (the consume reads the computed value from the slot). */
		PH7_HOOK_RMW_WRITEBACK(nIdx,0);
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
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
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
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —
		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is
		 * dropped by the fetch-point sweep (the landing pc is past its
		 * OP_NULLC_STORE window): the skipped assign never dispatches. */
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
	if( SySetUsed(&pVm->aHookRmw) > 0 ){
		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a
		 * COAL entry targeting exactly THIS store (owner + pc identity — a
		 * stale entry from an abandoned statement can never match, and nested
		 * arms in the RHS were consumed/dropped above this one). Dispatch the
		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=
		 * residual: pre-fix the assign bypassed __set through the normal slot
		 * path). The RHS stays as the expression result. */
		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr
		 && pTop->nPc == (sxu32)pc
		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK || pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){
			VmHookRmw sPend = *pTop;
			(void)SySetPop(&pVm->aHookRmw);
			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){
				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);
				if( rcHs == PH7_ABORT ){
					SyBlobRelease(&sPend.sName);
					PH7_ClassInstanceUnref(sPend.pThis);
					goto Abort;
				}
			}else{
				SyString sSetName;
				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));
				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);
			}
			SyBlobRelease(&sPend.sName);
			PH7_ClassInstanceUnref(sPend.pThis);
			PH7_MemObjStore(pTos,pNos);
			pNos->nIdx = SXU32_HIGH;
			VmPopOperand(&pTos,1);
			break;
		}
	}
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
 * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this
 * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —
 * the CALL may not be the next instruction, and may be an inner call whose own
 * spreads must stay scoped to it.
 * The expansion tail is shared between the plain-array and the materialized
 * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.
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
		if( pTmpMap == 0 ){ goto Abort; }
		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);
		if( rcW == PH7_EXCEPTION || rcW == PH7_ABORT ){
			PH7_HashmapRelease(pTmpMap,TRUE);
			if( rcW == PH7_ABORT ){ goto Abort; }
			goto Exception;
		}
		/* Grow the operand stack if this expansion would overflow it (no longer a
		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a
		 * single argument — the historical bounded-fallback, now only under OOM. */
		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,
		                            pCallTop, ppBaseOwner, pnBaseCap) ){
			PH7_HashmapRelease(pTmpMap,TRUE);
			VmErrorFormat(&(*pVm), PH7_CTX_ERR,
				"Argument unpacking: out of memory while expanding %u elements",
				pTmpMap->nEntry);
			break;
		}
		VmSpreadExpandMap(pVm, &pTos, pTmpMap);
		PH7_HashmapRelease(pTmpMap,TRUE);
		break;
	}
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;
		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,
		                            pCallTop, ppBaseOwner, pnBaseCap) ){
			VmErrorFormat(&(*pVm), PH7_CTX_ERR,
				"Argument unpacking: out of memory while expanding %u elements",
				pMap->nEntry);
			break;
		}
		VmSpreadExpandMap(pVm, &pTos, pMap);
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
	if( pInstr->iP2 == 1 ){
		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The
		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot
		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the
		 * property's nIdx to alias the source variable's slot and pin that slot
		 * past its owning frame (like a use(&$x) capture) so neither frame
		 * teardown nor a later unset recycles it while the property aliases it. */
		ph7_value *pSrc = &pTos[-1];
		sxu32 nSrcIdx = pSrc->nIdx;
		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;
		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;
		if( nSrcIdx == SXU32_HIGH ){
			/* php: the RHS of `=&` must be a variable, not a constant expression.
			 * (The compiler already rejects the obvious literal forms.) */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Reference operator require a variable not a constant as it's right operand");
		}else if( pVmAttr ){
			sxu32 nOldIdx = pVmAttr->nIdx;
			if( nOldIdx != nSrcIdx ){
				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){
					/* Release this property's own (unshared) slot before repointing.
					 * A reference-bound property bypasses typed coercion in php, so
					 * drop any typed-slot enforcement entry too. */
					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);
					}
					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);
				}
				pVmAttr->nIdx = nSrcIdx;
				pVmAttr->iState |= VM_CLASS_ATTR_REFBOUND;
				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
				VmPinMemObjSlot(&(*pVm),nSrcIdx);
			}
		}else if( pStAttr ){
			if( pStAttr->nIdx != nSrcIdx ){
				pStAttr->nIdx = nSrcIdx;
				VmPinMemObjSlot(&(*pVm),nSrcIdx);
			}
		}
		if( pVm->pRefTargetThis ){
			PH7_ClassInstanceUnref(pVm->pRefTargetThis);
		}
		pVm->pRefTargetAttr = 0;
		pVm->pRefTargetStaticAttr = 0;
		pVm->pRefTargetThis = 0;
		/* Pop the member-result; leave the source as the expression value. */
		VmPopOperand(&pTos,1);
		break;
	}
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
			/* php 8.1's non-catchable fatal (compile-time there) */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");
			pVm->iExitStatus = 255;
			pVm->bHaltRequested = 1;
			goto Abort;
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
	/* '@' depth at try entry — see ph7_exception.iErrSuppress */
	pException->iErrSuppress = pVm->nErrSuppress;
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
	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){
		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the
		 * exception is caught but not bound to any variable. */
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
		if( SyStringLength(&pInfo->sValue) > 0 ){
			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
				"foreach() argument must be of type array|object, %s given",
				VmArithTypeName(pTos));
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
			/* Record the owning activation so OP_FOREACH_STEP can pick THIS
			 * activation's step out of the per-statement stack — two suspended
			 * generator/fiber instances (or a recursive call) paused in the same
			 * textual foreach otherwise resume onto each other's cursor. */
			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				ph7_hashmap *pMap,*pIterMap;
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
				pIterMap = pMap;
				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){
					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a
					 * SNAPSHOT of the symbol table — globals created inside
					 * the loop body must not be visited (the live map would
					 * grow under the cursor). By-ref foreach keeps the live
					 * map, like php. On OOM fall back to the live map. */
					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);
					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){
						/* The step consumes the snapshot's initial reference */
						pIterMap = pSnap;
					}else if( pSnap ){
						PH7_HashmapUnref(pSnap);
					}
				}
				pStep->iFlags |= PH7_4EACH_STEP_HASHMAP;
				pStep->xIter.pMap = pIterMap;
				if( pIterMap == pMap ){
					pMap->iRef++;
				}
				/* Private cursor + registry (php: nested foreach over one
				 * array are independent; foreach never moves the internal
				 * pointer — see PH7_HashmapRegisterForeachStep) */
				PH7_HashmapRegisterForeachStep(pIterMap,pStep);
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
				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){
					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);
				}else{
					SyMemBackendPoolFree(&pVm->sAllocator,pStep);
				}
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
	sxu32 nStep;
	pFrameLocal = pVm->pFrame;
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every
	 * activation, so peeking the last entry resumes onto a sibling's cursor when
	 * two instances of one generator/fiber are suspended in the same textual
	 * foreach. Scan from the top (most-recent push) for the step whose owning
	 * frame matches the running activation; top-down makes the current push win
	 * over any leaked older step that happens to share a recycled frame address. */
	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
	nStep = SySetUsed(&pInfo->aStep);
	if( nStep < 1 ){
		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before
		 * STEP runs (and jumps past the loop when the push fails), so an empty
		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */
		pc = pInstr->iP2 - 1;
		break;
	}
	pStep = apStep[nStep - 1];
	while( nStep > 0 ){
		if( apStep[nStep - 1]->pFrame == pFrameLocal ){
			pStep = apStep[nStep - 1];
			break;
		}
		nStep--;
	}
	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){
		ph7_hashmap_node *pNode;
		/* Extract the current node via this loop's PRIVATE cursor (php:
		 * nested foreach over the same array are independent iterations) */
		pNode = pStep->pCursor;
		if( pNode == 0 ){
			/* No more entry to process */
			pc = pInstr->iP2 - 1; /* Jump to this destination */
			if( pStep->iFlags & PH7_4EACH_STEP_REF ){
				/* Break the reference with the last element */
				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
			}
			/* Cleanup the mess left behind */
			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);
		}else{
			/* Advance the private cursor */
			pStep->pCursor = pNode->pPrev; /* Reverse link */
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
			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
				continue; /* virtual set-only property: iteration skips it (php) */
			}
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
			VmForeachStepUnlink(pInfo,pStep);
			SyMemBackendPoolFree(&pVm->sAllocator,pStep);
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
			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET))
			 && (pStep->iFlags & PH7_4EACH_STEP_REF)
			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){
				/* php: a hooked property (virtual or backed) cannot be iterated
				 * by reference — catchable Error. Tear the step down like the
				 * exhausted-iteration path (break the by-ref binding, unlink,
				 * free, drop the instance retain) so nothing leaks and a
				 * re-entered foreach starts fresh; the fetch-point router lands
				 * the parked throw right after this op. Inside the property's
				 * own hook body the guard keeps raw semantics (no Error). */
				SyBlob sErrMsg;
				SyBlobInit(&sErrMsg,&pVm->sAllocator);
				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",
					&pThis->pClass->sName,&pVmAttr->pAttr->sName);
				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
				VmForeachStepUnlink(pInfo,pStep);
				SyMemBackendPoolFree(&pVm->sAllocator,pStep);
				PH7_ClassInstanceUnref(pThis);
				break;
			}
			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0
			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){
				/* PHP 8.4 property hooks: object iteration reads through the
				 * get hook (virtual properties included; the flag gate keeps
				 * hook-free classes on the raw zero-copy path below). The
				 * object step walks hAttr with the hash's single EMBEDDED
				 * cursor — save/restore it around the dispatch so a hook that
				 * re-enters an hAttr walk on this instance (get_object_vars,
				 * json_encode of $this) can't truncate THIS iteration. A hook
				 * that unset()s the property the saved cursor points at stays
				 * a recorded hazard (php's own semantics there are murky). */
				ph7_value sHookVal;
				sxi32 rcHk;
				SyHashEntry_Pr *pSavedCur = pThis->hAttr.pCurrent;
				PH7_MemObjInit(pVm,&sHookVal);
				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);
				pThis->hAttr.pCurrent = pSavedCur;
				if( rcHk != SXERR_NOTFOUND ){
					if( rcHk == SXRET_OK ){
						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
						if( pValue ){
							PH7_MemObjStore(&sHookVal,pValue);
						}
					}
					/* a throw parked on the boundary rail: the fetch-point
					 * router lands it right after this op */
					PH7_MemObjRelease(&sHookVal);
					break;
				}
				PH7_MemObjRelease(&sHookVal);
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
					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);
					if( pCallMagic ){
						/* php: a missing method dispatches __call($name, $args) — the
						 * args are only collected by the following OP_CALL, so stash the
						 * receiver + class + original name and redirect the callee to
						 * the hidden packing trampoline (band A #3b; pre-fix the name-
						 * only call discarded everything and the call site failed with
						 * "Invalid function name"). Stack: pop the method name, then
						 * the receiver slot becomes the trampoline's callee name. */
						SyBlobReset(&pVm->sMagicCallName);
						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
						pThis->iRef++;
						pVm->pMagicCallThis = pThis;
						pVm->pMagicCallClass = pClass;
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);
						MemObjSetType(pTos,MEMOBJ_STRING);
					}else{
						{
							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON
							 * with NULL, so the call silently produced nothing. */
							SyBlob sErrM;
							sxi32 rcErr;
							SyBlobInit(&sErrM,&pVm->sAllocator);
							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);
							VmPopOperand(&pTos,1);
							PH7_MemObjRelease(pTos);
							pTos->nIdx = SXU32_HIGH;
							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
								SyBlobLength(&sErrM));
							SyBlobRelease(&sErrM);
							if( rcErr == SXERR_ABORT ){ goto Abort; }
							rc = rcErr;
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}
					}
				}else{
					ph7_class_method *pDeniedCall = 0;
					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC
					 && !PH7_VmClassMemberAccess(&(*pVm),
						pMeth->sFunc.pUserData ? (ph7_class *)pMeth->sFunc.pUserData : pClass,
						&sName,pMeth->iProtection,FALSE) ){
						int bRebound = 0;
						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){
							/* php: when the instance's class SHADOWS the caller's
							 * private method (child redeclares private m), code in
							 * the caller's class dispatches its OWN private m, not
							 * the shadow. Rebind to the calling scope's method when
							 * that scope declares a private one of this name. */
							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);
							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;
							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){
								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;
								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);
								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE
								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){
									pMeth = pOwn;
									bRebound = 1;
								}
							}
						}
						if( !bRebound ){
							/* Inaccessible from this scope: php routes through __call when
							 * declared (band A #3b); without it OP_CALL raises its
							 * "Call to private/protected method" Error as before. */
							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);
						}
					}
					if( pDeniedCall ){
						SyBlobReset(&pVm->sMagicCallName);
						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
						pThis->iRef++;
						pVm->pMagicCallThis = pThis;
						pVm->pMagicCallClass = pClass;
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);
						MemObjSetType(pTos,MEMOBJ_STRING);
					}else{
						/* Push method name on the stack */
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
						MemObjSetType(pTos,MEMOBJ_STRING);
					}
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
					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).
					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property
					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from
					 * outside the class); without __unset, an inaccessible unset is php's
					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */
					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;
					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){
						/* php 8.4: a hooked property (virtual or backed) can never be
						 * unset — catchable Error, even from inside its own hook body
						 * (probe-verified). */
						SyBlob sErrMsg;
						SyBlobInit(&sErrMsg,&pVm->sAllocator);
						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",
							&pThis->pClass->sName,&pObjAttr->pAttr->sName);
						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
					}else if( pEntry && bUnsAccessible ){
						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);
						SyHashDeleteEntry2(pEntry);
					}else{
						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);
						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);
							VmMagicGuardPop(pVm);
						}else if( pEntry ){
							/* Inaccessible and no __unset: php's catchable Error. Parked on
							 * the boundary rail; the op completes benignly and the
							 * fetch-point router lands it. */
							SyBlob sErrMsg;
							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
							SyBlobInit(&sErrMsg,&pVm->sAllocator);
							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
						}
						/* Missing property without __unset: silent no-op (php). */
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
						}else{
							/* php 8 semantics (band A #3b): a PLAIN store to a missing
							 * property dispatches __set($name,$value) when declared —
							 * the value only exists at the following OP_STORE, so park
							 * the receiver+name for it (one-instruction lifetime; the
							 * guard makes a same-name write inside __set fall through
							 * to dynamic creation, like php). Without __set — or for a
							 * subscript-write base / read-modify-write, which php does
							 * NOT route through __set — create a dynamic property on
							 * ANY class with the 8.2 deprecation (suppressed for
							 * stdClass and #[AllowDynamicProperties]); a readonly
							 * class raises php's catchable Error instead. */
							ph7_class_method *pSetMagic = 0;
							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;
							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);
							if( bPlainStore ){
								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
							}
							if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){
								pThis->iRef++;
								pVm->pMagicSetThis = pThis;
								SyBlobReset(&pVm->sMagicSetName);
								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);
								/* pObjAttr stays NULL; the miss path below stays silent. */
							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE
							 && pNext->iOp == PH7_OP_NULLC_JMP
							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0
							  || (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0
							  || (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){
								/* `$o->p ??= v` on a missing property with magic accessors:
								 * php consults __isset first when declared — false means
								 * assign directly through __set with NO __get call (the
								 * test value stays null); true (or no __isset) means __get
								 * provides the test value. A COAL_MAGIC entry is pushed
								 * for the OP_NULLC_STORE at the jump target; the
								 * fetch-point sweep drops it when the short-circuit jump
								 * skips the assign or a throw abandons the RHS. Without
								 * __set the assign side keeps the pre-existing loud
								 * "Cannot perform assignment" path (php would create a
								 * dynamic property there — recorded residual). Note the
								 * condition's short-circuit assignment chain: a hit on an
								 * earlier method leaves the later pointers unresolved, so
								 * re-resolve the leftovers here (each is one hash probe,
								 * only on this ??=-miss path). */
								ph7_value sTest;
								int bMiss = 0; /* __isset said false: skip __get, test value stays null */
								if( pCoalIsset != 0 ){
									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
								}
								if( pCoalIsset != 0 || pCoalGet != 0 ){
									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
								}
								PH7_MemObjInit(pVm,&sTest);
								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){
									ph7_value sIssetRet;
									PH7_MemObjInit(pVm,&sIssetRet);
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);
									VmMagicGuardPop(pVm);
									PH7_MemObjToBool(&sIssetRet);
									bMiss = sIssetRet.x.iVal == 0;
									PH7_MemObjRelease(&sIssetRet);
								}
								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);
									VmMagicGuardPop(pVm);
								}
								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')
								 && pVm->nBoundaryRc == 0 ){
									VmHookRmw sPend;
									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;
									sPend.pThis = pThis;
									sPend.pAttr = 0;
									sPend.nBackIdx = SXU32_HIGH;
									sPend.nScratchIdx = SXU32_HIGH;
									SyBlobInit(&sPend.sName,&pVm->sAllocator);
									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);
									sPend.pOwnerStack = (void *)pStack;
									sPend.pInstrs = (void *)aInstr;
									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */
									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */
									pThis->iRef++;
									SySetPut(&pVm->aHookRmw,(const void *)&sPend);
								}
								/* Pop the attribute name; the test value becomes the
								 * expression slot (a temp, not an lvalue). */
								VmPopOperand(&pTos,1);
								pThis->iRef++;
								PH7_MemObjRelease(pTos);
								PH7_MemObjStore(&sTest,pTos);
								pTos->nIdx = SXU32_HIGH;
								PH7_MemObjRelease(&sTest);
								PH7_ClassInstanceUnref(pThis);
								break;
							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE
							 && !VmMemberNextIsWrite(pNext)
							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){
								/* Subscript-write base on a class with __get: php reads
								 * through the magic layer (the write lands on the temp and
								 * is lost, like php's indirect-modification case). A PLAIN
								 * store (bPlainStore — the compiler tags those
								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through
								 * to dynamic creation. A direct ++/--/compound-assign
								 * (VmMemberNextIsWrite — the compiler now tags those
								 * PH7_MEMBER_WRITE as well) is NOT this case either: it
								 * falls through to dynamic creation like before (the
								 * recorded RMW-vivifies-instead-of-__get residual, §7).
								 * Leave the miss path — the read gate below dispatches
								 * __get. */
							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",
									&pThis->pClass->sName,&sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)
							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){
								/* php 8.2 only DEPRECATES creating a dynamic property on a
								 * class without #[AllowDynamicProperties]; PHL rejects it.
								 * stdClass / __set / declared props are unaffected. */
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",
									&pThis->pClass->sName,&sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
							}else{
								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);
							}
						}
					}
				}
				if( pObjAttr == 0 ){
					/* Missing property. On a plain READ, php dispatches __get($name) and the
					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was
					 * discarded and a PHL-native warn fired even when __get existed). isset/
					 * empty context consults __isset first (then __get for empty()'s value
					 * test); a plain-store write context parked a pending __set above. A
					 * self-recursive read of the same property falls back to the
					 * undefined-property path via the guard, like php's property guard. */
					ph7_class_method *pGetMagic = 0;
					if( VmMemberCtxIsLookup(pInstr->iP2) ){
						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);
						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){
							ph7_value sIssetRet;
							int bSet;
							PH7_MemObjInit(pVm,&sIssetRet);
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);
							VmMagicGuardPop(pVm);
							PH7_MemObjToBool(&sIssetRet);
							bSet = sIssetRet.x.iVal != 0;
							PH7_MemObjRelease(&sIssetRet);
							if( bSet && pInstr->iP2 == PH7_MEMBER_EMPTY ){
								/* empty(): __isset said set — fetch the value via __get
								 * (php) so emptiness is judged on the real value. */
								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
								ph7_value sEmptyVal;
								PH7_MemObjInit(pVm,&sEmptyVal);
								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);
									VmMagicGuardPop(pVm);
								}
								VmPopOperand(&pTos,1);
								pThis->iRef++;
								PH7_MemObjRelease(pTos);
								PH7_MemObjStore(&sEmptyVal,pTos);
								pTos->nIdx = SXU32_HIGH;
								PH7_MemObjRelease(&sEmptyVal);
								PH7_ClassInstanceUnref(pThis);
								break;
							}
							/* isset(): the truth of __isset IS the answer — push a non-null
							 * marker for true, NULL for false (isset only tests null-ness). */
							VmPopOperand(&pTos,1);
							pThis->iRef++;
							PH7_MemObjRelease(pTos);
							if( bSet ){
								pTos->x.iVal = 1;
								MemObjSetType(pTos,MEMOBJ_BOOL);
							}
							pTos->nIdx = SXU32_HIGH;
							PH7_ClassInstanceUnref(pThis);
							break;
						}
					}
					if( !VmMemberCtxIsLookup(pInstr->iP2) && !VmMemberNextIsWrite(pInstr + 1) ){
						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,
						 * which php reads through __get); read-modify-write forms are
						 * excluded (they vivified above — approximate, recorded). */
						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
					}
					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
						ph7_value sMagicRet;
						PH7_MemObjInit(pVm,&sMagicRet);
						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);
						VmMagicGuardPop(pVm);
						/* Pop the attribute name, replace the object slot with the magic
						 * result (a temp, not an lvalue — nIdx stays constant). A throw
						 * from __get parked at the boundary; the fetch-point router lands
						 * it and abandons this slot. */
						VmPopOperand(&pTos,1);
						pThis->iRef++;
						PH7_MemObjRelease(pTos);
						PH7_MemObjStore(&sMagicRet,pTos);
						pTos->nIdx = SXU32_HIGH;
						PH7_MemObjRelease(&sMagicRet);
						PH7_ClassInstanceUnref(pThis);
						break;
					}
					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)
					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there
					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a
					 * pending __set was parked above (the following OP_STORE dispatches it —
					 * nothing is "undefined" about that write) or a throw is parked on the
					 * boundary rail (e.g. the readonly-class dynamic-property Error — the
					 * fetch-point router lands it right after this op). */
					if( !VmMemberCtxIsLookup(pInstr->iP2) && pVm->pMagicSetThis == 0
					 && pVm->nBoundaryRc == 0 ){
						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",
							&pClass->sName,&sName);
					}
				}
				VmPopOperand(&pTos,1);
				/* TICKET 1433-49: Deffer garbage collection until attribute loading.
				 * This is due to the following case:
				 *     (new TestClass())->foo;
				 */
				pThis->iRef++;
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */
				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){
					/* `$o->p =& $x`: stash the resolved instance property slot for the
					 * following member-marked OP_STORE_REF, which rebinds it to alias the
					 * source variable. Do NOT run the read/hook/magic/uninit machinery
					 * below — a reference bind neither reads the value nor triggers
					 * get/set hooks or an uninitialized-typed Error. pThis stays retained
					 * (the iRef++ above); OP_STORE_REF releases it. */
					if( pObjAttr ){
						pVm->pRefTargetAttr = pObjAttr;
						pVm->pRefTargetThis = pThis;
						pVm->pRefTargetStaticAttr = 0;
						pTos->nIdx = pObjAttr->nIdx;
					}else{
						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF
						 * no-ops. Balance the retain. */
						pVm->pRefTargetAttr = 0;
						pVm->pRefTargetThis = 0;
						pVm->pRefTargetStaticAttr = 0;
						PH7_ClassInstanceUnref(pThis);
					}
					break;
				}
				if( pObjAttr ){
					ph7_value *pValue = 0; /* cc warning */
					/* Check attribute access */
					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){
						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET))
						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){
							/* PHP 8.4 property hooks: route reads, writes, and the
							 * read-modify-write forms through the synthesized hook
							 * methods. A held guard (either kind) means we are INSIDE
							 * one of this property's own hook bodies — fall through to
							 * the raw backing slot for BOTH directions (php: `$this->x`
							 * within any of x's hooks addresses the backing store). */
							VmInstr *pNextH = pInstr + 1;
							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);
							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE
								&& pNextH->iOp == PH7_OP_NULLC_JMP;
							/* ++/--/compound-assign: the modify op FOLLOWS the member
							 * directly (the compiler tags compound-assign members
							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;
							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite
							 * returns 1 for a member OP_STORE too) */
							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);
							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext
								&& pInstr->iP2 == PH7_MEMBER_WRITE;
							if( bPlainStore ){
								/* Arm the pending hook-set for the following OP_STORE — it
								 * dispatches the set hook, or throws the read-only Error
								 * when the property only has a get hook. (The scalar
								 * transient is safe here: its window is exactly one
								 * instruction, MEMBER -> STORE, nothing runs in between.) */
								pThis->iRef++;
								pVm->pHookSetThis = pThis;
								pVm->pHookSetAttr = pObjAttr->pAttr;
								pVm->nHookSetIdx = pObjAttr->nIdx;
								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */
								PH7_ClassInstanceUnref(pThis);
								break;
							}
							if( bSubscriptW ){
								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):
								 * php's catchable Error, with or without a set hook
								 * (only a by-ref `&get` hook would allow it — a loud
								 * compile error in PHL). The op completes benignly with
								 * a null temp; the fetch-point router lands the throw. */
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",
									&pThis->pClass->sName,&pObjAttr->pAttr->sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
								PH7_ClassInstanceUnref(pThis);
								break;
							}
							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
								/* VIRTUAL set-only property: there is no backing store to
								 * fall back to, so EVERY read context — plain read,
								 * isset()/empty() (php throws there too, probe-verified),
								 * the ??= test, an RMW read — is php's catchable
								 * write-only Error. */
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",
									&pThis->pClass->sName,&pObjAttr->pAttr->sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
								PH7_ClassInstanceUnref(pThis);
								break;
							}
							if( VmMemberCtxIsLookup(pInstr->iP2) ){
								/* isset()/empty() on a hooked property calls the get hook
								 * (php): isset is "get() !== null", empty tests the value. */
								ph7_value sHookRet;
								PH7_MemObjInit(pVm,&sHookRet);
								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){
									if( pInstr->iP2 == PH7_MEMBER_ISSET ){
										pTos->x.iVal = (sHookRet.iFlags & MEMOBJ_NULL) == 0;
										MemObjSetType(pTos,MEMOBJ_BOOL);
									}else{
										/* empty(): hand the value to the truthiness test */
										PH7_MemObjStore(&sHookRet,pTos);
									}
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sHookRet);
									PH7_ClassInstanceUnref(pThis);
									break;
								}
								PH7_MemObjRelease(&sHookRet);
							}
							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){
								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */
								ph7_value sHookRet;
								PH7_MemObjInit(pVm,&sHookRet);
								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){
									PH7_MemObjStore(&sHookRet,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sHookRet);
									PH7_ClassInstanceUnref(pThis);
									break;
								}
								PH7_MemObjRelease(&sHookRet);
							}
							if( bCoalesceW || bRmwNext ){
								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write
								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads
								 * through the get hook (raw backing when the property is
								 * set-only) and writes through the set hook.
								 *   - ??=: the test value goes on the stack and a
								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE
								 *     at the jump target; the fetch-point sweep drops it
								 *     when the short-circuit jump skips the assign or a
								 *     throw abandons the RHS (the entry is NOT a scalar
								 *     transient — nested stores/coalesces in the RHS
								 *     stack their own entries above it).
								 *   - RMW: the value goes into a fresh SCRATCH slot the
								 *     modify op mutates in place; the entry makes the
								 *     op's tail dispatch the set side with the computed
								 *     value (PH7_HOOK_RMW_WRITEBACK). */
								ph7_value sCur;
								sxi32 rcCur;
								PH7_MemObjInit(pVm,&sCur);
								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);
								if( rcCur == SXERR_NOTFOUND ){
									/* set-only hook (or guard edge): the read side is the
									 * raw backing store (php) */
									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);
									if( pBack ){
										PH7_MemObjStore(pBack,&sCur);
									}
								}else if( pVm->nBoundaryRc != 0 ){
									/* the get hook threw: leave the null temp; the
									 * fetch-point router lands the parked throw —
									 * nothing is armed. */
									PH7_MemObjRelease(&sCur);
									PH7_ClassInstanceUnref(pThis);
									break;
								}
								if( bCoalesceW ){
									VmHookRmw sPend;
									PH7_MemObjStore(&sCur,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sCur);
									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;
									sPend.pThis = pThis;
									sPend.pAttr = pObjAttr->pAttr;
									sPend.nBackIdx = pObjAttr->nIdx;
									sPend.nScratchIdx = SXU32_HIGH;
									SyBlobInit(&sPend.sName,&pVm->sAllocator);
									sPend.pOwnerStack = (void *)pStack;
									sPend.pInstrs = (void *)aInstr;
									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */
									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */
									pThis->iRef++;
									SySetPut(&pVm->aHookRmw,(const void *)&sPend);
									PH7_ClassInstanceUnref(pThis);
									break;
								}
								{
									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));
									if( pScr ){
										VmHookRmw sRmw;
										PH7_MemObjStore(&sCur,pScr);
										PH7_MemObjStore(&sCur,pTos);
										pTos->nIdx = pScr->nIdx;
										sRmw.iKind = VM_HOOK_PEND_RMW;
										sRmw.pThis = pThis;
										sRmw.pAttr = pObjAttr->pAttr;
										sRmw.nBackIdx = pObjAttr->nIdx;
										sRmw.nScratchIdx = pScr->nIdx;
										SyBlobInit(&sRmw.sName,&pVm->sAllocator);
										sRmw.pOwnerStack = (void *)pStack;
										sRmw.pInstrs = (void *)aInstr;
										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */
										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */
										pThis->iRef++;
										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);
									}
									/* OOM: pScr == 0 — leave the null temp (loud allocator
									 * diagnostics already fired) */
									PH7_MemObjRelease(&sCur);
									PH7_ClassInstanceUnref(pThis);
									break;
								}
							}
						}
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
							/* isset()/empty()/`??` read an uninitialized typed property
							 * as "not set" — a silent miss, NOT the Error a plain read
							 * raises (php). The compiler tags such an access iP2 =
							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall
							 * through to load the slot's NULL. */
							if( VmMemberCtxIsLookup(pInstr->iP2) ){
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
						/* Inaccessible (private/protected) from this scope. php consults
						 * __get here exactly like a missing property (band A #3a) before
						 * erroring; the guard keeps a self-recursive read from looping. */
						ph7_class_method *pGetMagic = 0;
						if( pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberCtxIsLookup(pInstr->iP2)
						 && !VmMemberNextIsWrite(pInstr + 1) ){
							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
						}
						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
							ph7_value sMagicRet;
							PH7_MemObjInit(pVm,&sMagicRet);
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);
							VmMagicGuardPop(pVm);
							/* The name was already popped and pTos released above; just
							 * take the magic result as the expression value. */
							PH7_MemObjStore(&sMagicRet,pTos);
							pTos->nIdx = SXU32_HIGH;
							PH7_MemObjRelease(&sMagicRet);
							PH7_ClassInstanceUnref(pThis);
							break;
						}
						if( VmMemberCtxIsLookup(pInstr->iP2) ){
							/* isset/empty on an inaccessible property: php consults __isset
							 * (band A #3b), and is silently false without it — never an
							 * Error (pre-fix PHL fataled here). */
							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);
							int bSet = 0;
							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){
								ph7_value sIssetRet;
								PH7_MemObjInit(pVm,&sIssetRet);
								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');
								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);
								VmMagicGuardPop(pVm);
								PH7_MemObjToBool(&sIssetRet);
								bSet = sIssetRet.x.iVal != 0;
								PH7_MemObjRelease(&sIssetRet);
							}
							if( bSet ){
								if( pInstr->iP2 == PH7_MEMBER_EMPTY ){
									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
									ph7_value sEmptyVal;
									PH7_MemObjInit(pVm,&sEmptyVal);
									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);
										VmMagicGuardPop(pVm);
									}
									PH7_MemObjStore(&sEmptyVal,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sEmptyVal);
								}else{
									pTos->x.iVal = 1;
									MemObjSetType(pTos,MEMOBJ_BOOL);
									pTos->nIdx = SXU32_HIGH;
								}
							}
							PH7_ClassInstanceUnref(pThis);
							break;
						}
						{
							/* A plain store to an inaccessible property dispatches __set
							 * (band A #3b): park the receiver+name for the following
							 * OP_STORE, exactly like the missing-property case. */
							VmInstr *pNextW = pInstr + 1;
							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){
								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){
									pThis->iRef++;
									pVm->pMagicSetThis = pThis;
									SyBlobReset(&pVm->sMagicSetName);
									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);
									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */
									PH7_ClassInstanceUnref(pThis);
									break;
								}
							}
						}
						if( ((pInstr + 1)->iOp == PH7_OP_NULLC || (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)
						 && pInstr->iP2 != PH7_MEMBER_WRITE ){
							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=`
							 * pre-test): php treats `??` as an isset-style lookup — an
							 * inaccessible property yields null SILENTLY (the
							 * __isset/__get consults already ran above), letting the
							 * coalesce pick the default. */
							PH7_ClassInstanceUnref(pThis);
							break; /* pTos is already the null temp */
						}
						/* A subclass reading a PARENT's PRIVATE property: php treats it as
						 * an UNDEFINED property (the base-private is invisible to the
						 * subclass scope) — a Warning + null, NOT an access Error. Only
						 * this exact shape warns; every other denied read is the catchable
						 * "Cannot access" Error below. */
						{
						ph7_class *pSelf = VmCurrentSelf(&(*pVm));
						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;
						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){
							if( !VmMemberCtxIsLookup(pInstr->iP2) ){
								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",
									&pClass->sName,&sName);
							}
							PH7_ClassInstanceUnref(pThis);
							break; /* pTos is already the null temp */
						}
						}
						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the
						 * boundary rail; the fetch-point router lands it — it was an
						 * uncatchable VmReportUncaughtException+Abort before). */
						{
						SyBlob sErrMsg;
						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
						SyBlobInit(&sErrMsg,&pVm->sAllocator);
						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",
							zVis,&pClass->sName,&sName);
						PH7_ClassInstanceUnref(pThis);
						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
						SyBlobRelease(&sErrMsg);
						break;
						}
					}
				}
				/* Safely unreference the object */
				PH7_ClassInstanceUnref(pThis);
			}
		}else{
			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()
			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */
			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){
				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a
				 * non-object is a catchable Error, while READING a property off one is a
				 * Warning that yields NULL. PH7 raised the same notice for both and
				 * carried on with NULL, so `$null->m()` silently did nothing. */
				SyString sMemb;
				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
				if( pInstr->iP2 == PH7_MEMBER_METHOD ){
					SyBlob sErrM;
					sxi32 rcErr;
					SyBlobInit(&sErrM,&pVm->sAllocator);
					SyBlobFormat(&sErrM,"Call to a member function %z() on %s",
						&sMemb,VmArithTypeName(pNos));
					VmPopOperand(&pTos,1);
					PH7_MemObjRelease(pTos);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
						SyBlobLength(&sErrM));
					SyBlobRelease(&sErrM);
					if( rcErr == SXERR_ABORT ){ goto Abort; }
					rc = rcErr;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",
					&sMemb,VmArithTypeName(pNos));
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
			/* A FORWARDING static call — self::/parent::/static:: — preserves the
			 * caller's late-static-binding class (php); a non-forwarding C::m() resets
			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL
			 * can't resolve, so it would fall back to the callee's DECLARING class and
			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver
			 * after the method name is pushed. */
			int bForwardingCall = 0;
			ph7_class *pForwardLsb = 0;
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
						bForwardingCall = 1;
						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));
					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
						pClass = PH7_VmPeekTopClass(&(*pVm));
						bForwardingCall = 1;
						pForwardLsb = pClass;
					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
						pClass = PH7_VmResolveParentClass(&(*pVm));
						bForwardingCall = 1;
						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));
					}else{
						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
					}
				}
			}
			if( pClass == 0 ){
				/* Undefined class: php throws a catchable Error */
				SyBlob sErrM;
				sxi32 rcErr;
				SyBlobInit(&sErrM,&pVm->sAllocator);
				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",
					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));
				if( !pInstr->p3 ){
					VmPopOperand(&pTos,1);
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
					SyBlobLength(&sErrM));
				SyBlobRelease(&sErrM);
				if( rcErr == SXERR_ABORT ){ goto Abort; }
				rc = rcErr;
				PH7_THROW_ROUTE_MIDEXPR(rc)
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
							SyBlob sErrM;
							sxi32 rcErr;
							SyBlobInit(&sErrM,&pVm->sAllocator);
							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",
								&pClass->sName,&sName);
							if( !pInstr->p3 ){
								VmPopOperand(&pTos,1);
							}
							PH7_MemObjRelease(pTos);
							pTos->nIdx = SXU32_HIGH;
							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
								SyBlobLength(&sErrM));
							SyBlobRelease(&sErrM);
							if( rcErr == SXERR_ABORT ){ goto Abort; }
							rc = rcErr;
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}else{
							ph7_class_method *pCallStaticMagic = PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);
							if( pCallStaticMagic ){
								/* php: C::missing(...) dispatches __callStatic($name,$args)
								 * via the packing trampoline (see the instance twin). */
								SyBlobReset(&pVm->sMagicCallName);
								SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
								pVm->pMagicCallThis = 0;
								pVm->pMagicCallClass = pClass;
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);
								MemObjSetType(pTos,MEMOBJ_STRING);
								pTos->nIdx = SXU32_HIGH;
								break;
							}
							{
								/* php: the STATIC form reports the same "Call to undefined
								 * method C::m()" as the instance one. */
								SyBlob sErrM;
								sxi32 rcErr;
								SyBlobInit(&sErrM,&pVm->sAllocator);
								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",
									&pClass->sName,&sName);
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								pTos->nIdx = SXU32_HIGH;
								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
									SyBlobLength(&sErrM));
								SyBlobRelease(&sErrM);
								if( rcErr == SXERR_ABORT ){ goto Abort; }
								rc = rcErr;
								PH7_THROW_ROUTE_MIDEXPR(rc)
							}
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
					/* Forwarding call (self::/parent::/static::): overwrite the receiver
					 * slot (pNos, one below the method name) with the live LSB class name
					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the
					 * callee. Without this the literal keyword falls through to the
					 * callee's declaring class. Only when an LSB class is actually in
					 * scope (a static call from global scope has none). */
					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){
						SyBlobReset(&pNos->sBlob);
						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);
					}
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
							/* No such STATIC attribute. php raises a catchable Error
							 * ("Access to undeclared static property") — instance magic
							 * (__get) is never consulted for statics (band A #3b; the old
							 * path warned + called __get with a null $this and discarded
							 * it). isset()/empty() context stays silently false. Parked on
							 * the boundary rail; the op completes benignly with NULL and
							 * the fetch-point router lands the throw. */
							if( !VmMemberCtxIsLookup(pInstr->iP2) ){
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",
									&pClass->sName,&sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
							}
						}
						/* Pop the attribute name from the stack */
						if( !pInstr->p3 ){
							VmPopOperand(&pTos,1);
						}
						PH7_MemObjRelease(pTos);
						pTos->nIdx = SXU32_HIGH;
						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){
							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot
							 * for the following member-marked OP_STORE_REF (class-level, shared
							 * across instances — matches php). Skip the read machinery below. */
							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)
							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
								pVm->pRefTargetStaticAttr = pAttr;
								pVm->pRefTargetAttr = 0;
								pVm->pRefTargetThis = 0;
								pTos->nIdx = pAttr->nIdx;
							}else{
								pVm->pRefTargetStaticAttr = 0;
								pVm->pRefTargetAttr = 0;
								pVm->pRefTargetThis = 0;
							}
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
							}
							break;
						}
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
									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)
									 && SySetUsed(&pAttr->aAttrs) > 0 ){
										/* php 8.4 #[\Deprecated] on a class constant:
										 * every access re-warns. */
										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);
									}
									if( pAttr->nIdx == SXU32_HIGH ){
										/* Unmaterialized slot. Enum case: first access
										 * materializes ALL the singletons. Plain constant: its
										 * initializer hasn't run yet (mount-order-dependent
										 * cross-constant reference) — evaluate on demand. A
										 * raised TypeError/Error (backing mismatch, duplicate
										 * value, self-reference) parks on the boundary rail;
										 * the op completes benignly with NULL and the
										 * fetch-point router lands the throw. */
										sxi32 rcEnum;
										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){
											/* php: a DIRECT static access evaluates every case
											 * of the enum (whole-class constant update) — a
											 * broken sibling case throws here too. A reference
											 * from inside another constant's initializer
											 * (nConstEvalDepth > 0) evaluates only the
											 * requested case. */
											if( pVm->nConstEvalDepth > 0 ){
												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);
											}else{
												rcEnum = VmEnumMaterialize(&(*pVm),pClass);
											}
										}else{
											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);
										}
										if( rcEnum != SXRET_OK ){
											VmBoundaryPark(&(*pVm),rcEnum);
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
	/* Constructor arg count: compile-time args plus THIS new's own unpack
	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —
	 * `new C(...$args)` used to ignore the extras, leaving the expanded
	 * elements ABOVE the class-name slot and fataling "Class ' ' is not
	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested
	 * spread call in the ctor arg list stays scoped to itself. */
	sxi32 nCtorArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);
	ph7_value *pArg;
	ph7_class *pClass = 0;
	ph7_class_instance *pNew;
	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */
	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name
	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the
	 * class-name slot at pTos (above the args), so pArg is already the correct base;
	 * the build also truncates this call's captured runs. */
	VmCallArgMap sEffNewMap;
	VmCallArgMap *pEffNewMap = VmEffCallArgMap(pVm,pInstr,pArg,
		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);
	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){
		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);
		sxu32 nCls = SyBlobLength(&pTos->sBlob);
		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)
		 || (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)
		 || (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){
			/* new self() / new static() / new parent(): resolve against the live
			 * class context (LSB for static), sharing the FCC resolver. */
			pClass = VmFccResolveScope(&(*pVm),pTos);
			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT)) ){
				/* Not new-able (the named path's iLoadable extract excludes these
				 * before it ever gets here): php's wording, with the RESOLVED name. */
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot instantiate %s %z",
					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",
					&pClass->sName);
				PH7_MemObjRelease(pTos);
				if( nCtorArgs > 0 ){
					VmPopOperand(&pTos,nCtorArgs);
				}
				goto Abort;
			}
		}else{
			/* Try to extract the desired class */
			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,
				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);
		}
	}else if( pTos->iFlags & MEMOBJ_OBJ ){
		/* Take the base class from the loaded instance */
		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;
	}
	if( pClass == 0 ){
		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a
		 * hard stop. Aborting here killed the script outright -- and with exit status 0,
		 * so a caller could not even tell it had failed. */
		SyBlob sErrM;
		sxi32 rcErr;
		ph7_class *pNotNew = 0;
		SyBlobInit(&sErrM,&pVm->sAllocator);
		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){
			/* The extract above only accepts NEW-able classes, so an interface or an
			 * abstract class comes back as 0 and used to be reported as "not found".
			 * Look again without that filter so php's real message can be given. */
			pNotNew = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),
				SyBlobLength(&pTos->sBlob),FALSE,0);
		}
		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT)) ){
			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",
				(pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",
				&pNotNew->sName);
		}else{
			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",
				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));
		}
		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)
		 * expression result and the ctor arguments go. */
		if( nCtorArgs > 0 ){
			VmPopOperand(&pTos,nCtorArgs);
		}
		PH7_MemObjRelease(pTos);
		MemObjSetType(pTos,MEMOBJ_NULL);
		pTos->nIdx = SXU32_HIGH;
		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
			SyBlobLength(&sErrM));
		SyBlobRelease(&sErrM);
		if( rcErr == SXERR_ABORT ){ goto Abort; }
		rc = rcErr;
		PH7_THROW_ROUTE_MIDEXPR(rc)
	}else if( pClass->iFlags & PH7_CLASS_ENUM ){
		/* php 8.1: enums cannot be instantiated — a catchable Error, raised
		 * BEFORE any construction (no instance, no __destruct). */
		SyBlob sErrMsg;
		SyBlobInit(&sErrMsg,&pVm->sAllocator);
		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);
		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
		if( nCtorArgs > 0 ){
			VmPopOperand(&pTos,nCtorArgs);
		}
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		break;
	}else{
		ph7_class_method *pCons;
		/* Check if a constructor is available — BEFORE instantiation: a
		 * visibility-denied `new` must not construct (nor later destruct)
		 * the object (band A #4). */
		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
		if( pCons == 0 ){
			SyString *pName = &pClass->sName;
			/* Check for a constructor with the same base class name */
			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);
		}
		/* Constructor visibility (band A #4): __construct now KEEPS its
		 * declared protection (PH7_NewClassMethod no longer forces it
		 * public), so `new C()` on a private/protected constructor from
		 * the wrong scope is php's catchable Error. Reflection's
		 * newInstance path sets bReflectBypass like method invoke. */
		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){
			/* php binds non-public access by the member's DECLARING class, not the
			 * instantiated class: a private constructor declared in a base is
			 * reachable from that base's own methods even when instantiating a
			 * subclass (`new Child()` inside Base::factory()). __construct is a
			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the
			 * method-call visibility check — rather than pClass, whose hAttr lookup
			 * for a method name always misses and falls to a wrong exact-class test. */
			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;
			if( pVm->bReflectBypass ){
				pVm->bReflectBypass = 0;
			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){
				SyBlob sErrMsg;
				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
				SyBlobInit(&sErrMsg,&pVm->sAllocator);
				SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",
					zVis,&pClass->sName);
				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
				/* Pop ctor args + release the class-name operand, leave NULL
				 * as the expression value; the fetch-point router lands the
				 * throw. No instance was created. */
				if( nCtorArgs > 0 ){
					VmPopOperand(&pTos,nCtorArgs);
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				break;
			}
		}
		/* Create a new class instance */
		pNew = PH7_NewClassInstance(&(*pVm),pClass);
		if( pNew == 0 ){
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,
				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",
				&pClass->sName
			);
			PH7_MemObjRelease(pTos);
			if( nCtorArgs > 0 ){
				/* Pop given arguments */
				VmPopOperand(&pTos,nCtorArgs);
			}
			break;
		}
		if( pCons ){
			/* Call the class constructor.  Collect args in stack order and
			 * forward any VmCallArgMap from the NEW instruction so the
			 * receiving OP_CALL path runs its named-argument matching
			 * (including variadic string-key packing). */
			VmCallArgMap *pNewMap = pEffNewMap;
			sxi32 rcCons;
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			/* Too-few-arguments is php's catchable ArgumentCountError, raised
			 * by the shared OP_CALL install path this ctor call routes through
			 * (was a PHL-only notice here). */
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
					if( nCtorArgs > 0 ){
						VmPopOperand(&pTos,nCtorArgs);
					}
					PH7_MemObjRelease(pTos);
					pc = iResumePc;
					break;
				}
				goto Exception;
			}
		}
		if( nCtorArgs > 0 ){
			/* Pop given arguments */
			VmPopOperand(&pTos,nCtorArgs);
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
	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable
	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */
	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",
			ph7_type_name(pTos));
		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		if( rc == PH7_ABORT ){
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
	/* Point to the source */
	pSrc = (ph7_class_instance *)pTos->x.pOther;
	/* Enum cases are not cloneable — php's catchable Error (the singleton
	 * identity would break). */
	if( pSrc->pClass->iFlags & PH7_CLASS_ENUM ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",
			&pSrc->pClass->sName);
		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		if( rc == PH7_ABORT ){
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
 * OP_CLONE_APPLY * * *
 *  Apply the PHP 8.5 clone($obj, $withProperties) property updates. The updates
 *  array is on the stack top and the freshly-cloned object (from OP_CLONE) is
 *  directly below it. Each entry is applied as a scope-aware property write
 *  (AFTER __clone() has already run); the array is then popped, leaving the
 *  clone as the result.
 */
case PH7_OP_CLONE_APPLY: {
	ph7_value *pUpdates,*pObj;
	ph7_class_instance *pClone;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	sxi32 rcApply = SXRET_OK;
	sxu32 n;
#ifdef UNTRUST
	if( pTos < &pStack[1] ){
		goto Abort;
	}
#endif
	pUpdates = pTos;
	pObj = &pTos[-1];
	/* $withProperties must be an array (PHP: TypeError otherwise). */
	if( (pUpdates->iFlags & MEMOBJ_HASHMAP) == 0 ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"clone(): Argument #2 ($withProperties) must be of type array, %s given",
			ph7_type_name(pUpdates));
		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
		if( rc == PH7_ABORT ){
			goto Abort;
		}
		/* Pop the (bad) updates argument and dispatch to the nearest catch. */
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
	/* The clone must be an object; OP_CLONE leaves NULL only on a prior failure
	 * (already reported) — in that case just drop the updates and carry the NULL. */
	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){
		VmPopOperand(&pTos,1);
		break;
	}
	pClone = (ph7_class_instance *)pObj->x.pOther;
	pMap = (ph7_hashmap *)pUpdates->x.pOther;
	/* Apply each update in insertion order (pFirst -> pPrev is the forward link). */
	pNode = pMap->pFirst;
	for( n = pMap->nEntry ; n > 0 && rcApply == SXRET_OK ; --n ){
		ph7_value *pVal;
		ph7_value sVal;
		const char *zName;
		sxu32 nName;
		char zKeyBuf[64];
		if( pNode == 0 ){
			break;
		}
		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
		if( pNode->iType == HASHMAP_INT_NODE ){
			/* An int key becomes the property name (PHP: `$5`). */
			nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);
			zName = zKeyBuf;
		}else{
			zName = (const char *)SyBlobData(&pNode->xKey.sKey);
			nName = SyBlobLength(&pNode->xKey.sKey);
		}
		if( pVal ){
			/* Snapshot the update value into a stack local FIRST: applying it may
			 * create a dynamic property, whose slot reservation can reallocate
			 * pVm->aMemObj and dangle pVal (a pointer into it). The name is safe
			 * (it lives in the node's key blob / zKeyBuf, not in aMemObj). */
			PH7_MemObjInit(pVm,&sVal);
			PH7_MemObjLoad(pVal,&sVal);
			rcApply = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);
			PH7_MemObjRelease(&sVal);
		}
		pNode = pNode->pPrev;
	}
	if( rcApply == PH7_ABORT ){
		goto Abort;
	}
	if( rcApply == PH7_EXCEPTION ){
		/* An update threw (visibility / readonly / type). Pop the updates array
		 * and hand control to the nearest catch, else propagate out of the loop. */
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
	/* Success: drop the updates array, leaving the clone on the stack. */
	VmPopOperand(&pTos,1);
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
	if( pVm->pActiveCtx->bClosing ){
		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's
		 * pending finallys tried to yield — PHP forbids it. */
		if( VmThrowFixedError(&(*pVm), "Error",
			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){
			goto Abort;
		}
		goto Exception;
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
	if( pVm->pActiveCtx->bClosing ){
		/* A `yield from` reached while VmCloseCtx force-drives this destroyed
		 * generator's pending finallys — PHP forbids it (distinct message from a
		 * bare `yield`, mirroring the OP_YIELD guard above). */
		if( VmThrowFixedError(&(*pVm), "Error",
			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){
			goto Abort;
		}
		goto Exception;
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
		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a
		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()
		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)
		 * ignore send() and just advance with next(). */
#ifdef UNTRUST
		if( pTos < pStack ){ goto Abort; }
#endif
		if( pCtxFrom->iDelegateState == 3 ){
			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);
			/* A pending Generator::throw() on the outer was parked on pInjected by the
			 * body-entry gate (which skips its own raise for state 3); forward it as a
			 * throw into the delegate, else forward the sent value (pTos, which
			 * VmResumeCtx copies into the inner's own stack). */
			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;
			pCtxFrom->pInjected = 0;
			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
				if( pInjFwd ){
					/* Borrowed ref: the outer's Generator::throw() holds it across this
					 * whole resume, so the inner inject path must not unref it. */
					pInner->pCtx->pInjected = pInjFwd;
					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);
					pInner->pCtx->pInjected = 0;
				}else{
					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);
				}
			}else if( pInjFwd ){
				/* No live delegate to receive the throw (inner already finished/closed):
				 * raise it at the yield-from in the outer generator's own frame. */
				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);
				pTF->iFlags |= VM_FRAME_THROW;
				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
			}
			PH7_MemObjRelease(pTos);
			pTos--;
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}else{
			PH7_MemObjRelease(pTos);
			pTos--;
			if( pCtxFrom->iDelegateState >= 2 ){
				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,
					"next",sizeof("next")-1,0);
				if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
			}
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
	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack
	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the
	 * stack) — an INNER spread-bearing call evaluated inside this argument list
	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not
	 * be conflated, which a single shared accumulator could not express. */
	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);
	ph7_value *pArg;
	pArg = &pTos[-nCallArgs];
	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED
	 * arguments, and a spread expanding to !=1 element shifts the actual positions
	 * of any following compile-time named args. The effective per-actual-slot name
	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)
	 * is built PER PATH against that path's finalized arg base — a method call pops
	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch
	 * site rather than built once here. */
	VmCallArgMap sEffMap;
	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;
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
			{
				/* php validates the SHAPE of an array callable first: it must hold exactly
				 * two elements. PH7 handed any array to the dispatcher, which failed
				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */
				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;
				char zCbMsg[192];
				const char *zCbErr = 0;
				if( pCbMap && pCbMap->nEntry == 2 ){
					/* Shape is right; now check it actually RESOLVES. The shared dispatcher
					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for
					 * an unresolvable [class,method] pair -- silence a caller cannot detect --
					 * and its contract is relied on by call_user_func/usort, so the throw
					 * belongs here at the call site. */
					ph7_value *pCbCls = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->nValIdx);
					ph7_value *pCbMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->pPrev->nValIdx);
					ph7_class *pCbClass = pCbCls ? PH7_VmExtractClassFromValue(&(*pVm),pCbCls) : 0;
					if( pCbClass == 0 ){
						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Class \"%.*s\" not found",
							pCbCls ? (int)SyBlobLength(&pCbCls->sBlob) : 0,
							pCbCls ? (const char *)SyBlobData(&pCbCls->sBlob) : "");
						zCbErr = zCbMsg;
					}else if( pCbMeth == 0 || (pCbMeth->iFlags & MEMOBJ_STRING) == 0
						|| PH7_ClassExtractMethod(pCbClass,(const char *)SyBlobData(&pCbMeth->sBlob),
							SyBlobLength(&pCbMeth->sBlob)) == 0 ){
						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Call to undefined method %z::%.*s()",
							&pCbClass->sName,
							pCbMeth ? (int)SyBlobLength(&pCbMeth->sBlob) : 0,
							pCbMeth ? (const char *)SyBlobData(&pCbMeth->sBlob) : "");
						zCbErr = zCbMsg;
					}
				}
				if( pCbMap == 0 || pCbMap->nEntry != 2 || zCbErr ){
					sxi32 rcCb;
					if( pInstr->iP2 ){
						VmSpreadConsume(pVm);
					}
					if( nCallArgs > 0 ){
						VmPopOperand(&pTos,nCallArgs);
					}
					PH7_MemObjRelease(pTos);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					if( zCbErr == 0 ){
						zCbErr = "Array callback must have exactly two elements";
					}
					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));
					if( rcCb == SXERR_ABORT ){ goto Abort; }
					rc = rcCb;
					PH7_DISPATCH_ENFORCE_RC(rc)
				}
			}
			/* Build the effective spread-key map (and consume this call's runs)
			 * against this path's arg base (the array-callable slot isn't popped). */
			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			PH7_MemObjInit(pVm,&sResult);
			/* May be a class instance and it's static method. Forward this call's named-arg map
			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —
			 * mirroring the __invoke-object branch below. */
			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);
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
			/* __invoke object callable: the object slot isn't popped, so pArg is
			 * already this call's arg base — build the map + consume the runs. */
			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
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
				pEffCallMap);
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
			/* php: calling a non-callable is a catchable Error naming the type
			 * ("Value of type int is not callable"), or -- for an array -- the shape it
			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so
			 * `$x()` on a number quietly evaluated to nothing. */
			sxi32 rcNc;
			char zMsg[128];
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");
			}else{
				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",
					VmArithTypeName(pTos));
			}
			/* Consume this call's captured spread runs — a non-callable target
			 * (int/float/bool/null) reaches no dispatch build site. */
			if( pInstr->iP2 ){
				VmSpreadConsume(pVm);
			}
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			/* Settle the call's result slot BEFORE throwing. */
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));
			if( rcNc == SXERR_ABORT ){ goto Abort; }
			rc = rcNc;
			PH7_DISPATCH_ENFORCE_RC(rc)
		}
		break;
	}
	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just
	 * anchors it to the global namespace — strip it before resolving so a
	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */
	if( sName.nByte > 0 && sName.zString[0] == '\\' ){
		sName.zString++;
		sName.nByte--;
	}
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
	VmCallArgMap *pCallMap = pEffCallMap;
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
		}
		if( pVm->pClosureScope ){
			/* May ride alongside a bound $this, or stand alone for a
			 * scope-only rebind (`bindTo(null, Scope::class)`). */
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
				/* Synchronize pointers. The method-name slot popped above sat BETWEEN
				 * this call's arguments and the (already-removed) target — so only now
				 * is pTos one past the last actual argument. Re-derive the unpack
				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs
				 * from run ends, which the extra target slot would otherwise offset,
				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */
				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);
				pArg = &pTos[-nCallArgs];
				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'
				 * user have already computed the random generated unique class method name
				 * and tries to call it outside it's context [i.e: global scope]. In that
				 * case we have to synchronize pointers to avoid stack underflow.
				 */
				while( pArg < pStack ){
					pArg++;
				}
				if( pSelf && pVm->bReflectBypass ){
					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection
					 * ignores visibility. Consume-once so nested calls made by the
					 * invoked body are checked normally. */
					pVm->bReflectBypass = 0;
				}else
				if( pSelf ){ /* Paranoid edition */
					/* Check if the call is allowed. php binds non-public method
					 * access by the DECLARING class (pVmFunc->pUserData — the class
					 * the callee was compiled in), NOT the instance's class: an
					 * inherited base method calling $this->priv() on a child
					 * instance passes, a child's private SHADOW doesn't hijack the
					 * check for a parent callee, and the denial message names the
					 * declaring class like php. */
					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;
					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);
					if( pMeth == 0 && pDeclClass != pSelf ){
						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);
					}
					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
						if( !PH7_VmClassMemberAccess(&(*pVm),pDeclClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){
							/* php throws a CATCHABLE Error here. The old code merely PRINTED an
							 * uncaught-exception report and aborted, so `try { $o->priv(); }
							 * catch (Error $e)` never caught it and the script died. */
							char zMsg[256];
							sxi32 rcVis;
							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",
								zVis,(int)pDeclClass->sName.nByte,pDeclClass->sName.zString,
								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);
							/* Consume this call's captured spread runs — this visibility
							 * error exits before the pVmFunc build below. */
							if( pInstr->iP2 ){
								VmSpreadConsume(pVm);
							}
							/* Pop given arguments, and leave the call's NULL result behind. */
							if( nCallArgs > 0 ){
								VmPopOperand(&pTos,nCallArgs);
							}
							PH7_MemObjRelease(pTos);
							MemObjSetType(pTos,MEMOBJ_NULL);
							pTos->nIdx = SXU32_HIGH;
							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));
							if( rcVis == SXERR_ABORT ){ goto Abort; }
							rc = rcVis;
							PH7_DISPATCH_ENFORCE_RC(rc)
						}
					}
				}
			}
		}
		/* pArg is now finalized for every pVmFunc callee (a method call popped its
		 * method-name slot above; functions/closures/generators keep the top base).
		 * Build the PHP 8.1 effective spread-key map here so the generator and the
		 * install path below both see it — and so this call's captured runs are
		 * consumed exactly once, against the correct base. */
		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
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
		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){
			/* Push the closure's called-class as this frame's LSB class so
			 * `static::` inside the body resolves like php. An explicit
			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use
			 * the class captured at the closure's creation site. self::/parent::
			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done
			 * after pSelfHint so a `self`-typed param is unaffected. */
			if( pClosureScope ){
				pSelf = pClosureScope;
			}else if( pVmFunc->pLsbClass ){
				pSelf = (ph7_class *)pVmFunc->pLsbClass;
			}
		}
		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){
			/* php 8.4 #[\Deprecated] runtime notice — once per call, before
			 * execution (generators: at the g(...) call site, like php). */
			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,
				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);
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
					VmCallArgMap *pGenMap = pEffCallMap;
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
							if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){
								/* php's named-hole ArgumentCountError, checked BEFORE
								 * hole compaction: compacting first would report the
								 * positional wording with a fictitious count (g(b:2)
								 * must be `g(): Argument #1 ($a) not passed`, not
								 * "1 passed"). Implicit-required watermark, like the
								 * plain-call named path; a hole with NOTHING filled
								 * above it keeps php's count wording — fall through
								 * to VmFiberSetupFrame's check (the compacted count
								 * equals php's num_args there). */
								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;
								sxi32 iHole = -1;
								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);
								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){
									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){
										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);
									}
								}
								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){
									sxu32 gj;
									int bFound = 0;
									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){
										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }
									}
									if( !bFound && gHole + 1 <= nMaxFilledG ){
										iHole = (sxi32)gHole;
									}
								}
								if( iHole >= 0 ){
									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,
										(sxu32)iHole+1,&aFA[iHole].sName);
									SyMemBackendFree(&pVm->sAllocator, aGSlot);
									SyMemBackendFree(&pVm->sAllocator, apCallArgs);
									if( rc == PH7_ABORT ){
										goto Abort;
									}
									/* Route like the VmFiberSetupFrame throw below */
									PH7_INLINE_RESUME_BREAK()
									VmPopOperand(&pTos,nCallArgs + 1);
									{
										sxi32 iRpH;
										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){
											pc = iRpH;
											break;
										}
									}
									goto Exception;
								}
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
			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,
				(pEffCallMap && pEffCallMap->bStrict) ? 1 : 0, pSelfHint,
				TRUE/*generator: the g(...) call site is in the message*/);
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
				if( rc == PH7_EXCEPTION ){
					/* A declared-type TypeError thrown while binding the
					 * generator's arguments (php binds + type-checks eagerly at
					 * the g(...) call site, before any resume — band A #2). If
					 * an inline try THIS exec owns caught it, land at its
					 * redirect (the drain subsumes the operand pops); else pop
					 * the args + function name and route like the other
					 * OP_CALL throw paths. */
					PH7_INLINE_RESUME_BREAK()
					VmPopOperand(&pTos,nCallArgs + 1);
					{
						sxi32 iRpG;
						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){
							pc = iRpG;
							break;
						}
					}
					goto Exception;
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
			/* Pop args and function name, push Generator object. PH7_NewClassInstance
			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do
			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope
			 * and its __destruct (which runs pending `finally` blocks and frees the
			 * exec context) never fires. */
			PH7_MemObjRelease(pTos);
			pTos = &pTos[-nCallArgs];
			pTos->x.pOther = pGenObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
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
		if( pClosureScope ){
			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /
			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected
			 * access inside the body resolves against it. */
			pFrame->pBoundScope = pClosureScope;
		}
		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band
		 * A #4): sArg over-counts (defaulted params installed, variadic packed
		 * as one entry) so php's answers can't be derived from it. */
		pFrame->nActualArgs = (int)(pTos - pArg);
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
		VmCallArgMap *pCallMap3 = pEffCallMap;
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
			{
			/* php's required watermark for the hole check below (0 disables it
			 * for hosted builtin FUNCTIONS, which self-manage — hosted-class
			 * methods and all user code get php's named-hole error), plus the
			 * highest formal slot an actual resolved to: php words a hole
			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole
			 * with nothing filled above it gets the positional count message
			 * (zend's RECV arg_num > EX(num_args) distinction). */
			sxu32 nReqNamed = 0;
			sxu32 nNVNamed = 0;
			sxu32 nMaxFilled = 0;
			if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){
				nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);
				for( i = 0; i < nActual; i++ ){
					if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){
						nMaxFilled = (sxu32)(aSlot[i] + 1);
					}
				}
			}
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
					/* An explicit null is NOT redirected to the default: PHP applies a
					 * default only for an OMITTED argument. An explicit null falls through
					 * to the type check below (TypeError for a non-nullable typed param,
					 * kept as null for a typeless one). An implicitly-nullable `Type $x =
					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */
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
								/* Reaching here means the param is non-nullable (the guard
								 * above skips nullable+null; a `Type $x = null` default is
								 * marked implicitly nullable at compile time). So ANY
								 * non-object — including an explicit null — is a TypeError,
								 * matching PHP (&& below short-circuits so instanceof only
								 * derefs a real object). */
								int bBad = !((pVal->iFlags & MEMOBJ_OBJ)
									&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass));
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
						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){
							/* php 8.1: $GLOBALS cannot be passed by reference */
							SyBlob sMsg;
							SyBlobInit(&sMsg,&pVm->sAllocator);
							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
								&pVmFunc->sName,n+1,&aFormalArg[n].sName);
							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);
							if( rc == PH7_ABORT ){
								goto Abort;
							}
							SyMemBackendFree(&pVm->sAllocator, aSlot);
							PH7_MemObjRelease(pTos);
							pTos = &pTos[-nCallArgs];
							pFrameStack = 0;
							rc = PH7_EXCEPTION;
							goto SkipFuncBody;
						}
						if( pVal->nIdx == SXU32_HIGH ){
							if( (pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0
							 && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){
								/* A non-lvalue bound to a by-ref parameter is a catchable Error in
								 * php — f(5) where f(&$x). PH7 only warned and quietly passed by
								 * value, so the call ran with a copy and the caller never knew.
								 * The one legitimate copy is call_user_func()'s (MEMOBJ_AUX_CUFVAL),
								 * which php also permits, with its own warning. */
								SyBlob sMsg;
								sxi32 rcRef;
								SyBlobInit(&sMsg,&pVm->sAllocator);
								SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
									&pVmFunc->sName,n+1,&aFormalArg[n].sName);
								rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
									SyBlobLength(&sMsg));
								SyBlobRelease(&sMsg);
								if( rcRef == SXERR_ABORT ){
									pFrameStack = 0;
									rc = PH7_ABORT;
									goto SkipFuncBody;
								}
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
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
					}else if( n < nReqNamed ){
						/* php's implicit-required rule applies to named calls
						 * too: a hole below the required watermark throws even
						 * when the formal carries a default — f($a,$b=2,$c)
						 * called as f(a:1,c:3) is `Argument #2 ($b) not
						 * passed` (a hole with NO default is always below the
						 * watermark, so this subsumes the no-default case).
						 * A hole with nothing filled ABOVE it uses php's
						 * positional count wording instead. The passed stack
						 * args were not released yet on this path (that loop
						 * runs after Pass 2) — release them before the exit. */
						if( n + 1 > nMaxFilled ){
							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)
								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
									nMaxFilled,nReqNamed,nNVNamed)
								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
									nMaxFilled,nReqNamed,nNVNamed,TRUE);
						}else{
							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);
						}
						SyMemBackendFree(&pVm->sAllocator, aSlot);
						for( i = 0; i < nActual; i++ ){
							PH7_MemObjRelease(&pArg[i]);
						}
						if( rc == PH7_ABORT ){
							goto Abort;
						}
						PH7_MemObjRelease(pTos);
						pTos = &pTos[-nCallArgs];
						pFrameStack = 0;
						rc = PH7_EXCEPTION;
						goto SkipFuncBody;
					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
						if( pObj ){
							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);
							if( rc == PH7_ABORT ) goto Abort;
							sArg.nIdx = pObj->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
							/* A null default on an implicitly-nullable param must stay null
							 * (see the positional-path note above). */
							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ
								&& (pObj->iFlags & aFormalArg[n].nType) == 0
								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){
								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
								if( xCast ) xCast(pObj);
							}
						}
					}
				}
			}
			} /* end nReqNamed scope */
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
				/* An explicit null is NOT redirected to the default (PHP applies a
				 * default only for an omitted arg); it falls through to the type check
				 * below — TypeError for a non-nullable typed param, kept as null for a
				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE
				 * at compile time so its check accepts null. */
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
							/* Reaching here means the param is non-nullable (the guard above
							 * skips nullable+null; a `Type $x = null` default is marked
							 * implicitly nullable at compile time). So ANY non-object —
							 * including an explicit null — is a TypeError, matching PHP
							 * (&& below short-circuits so instanceof only derefs an object). */
							int bBad = !((pArg->iFlags & MEMOBJ_OBJ)
								&& PH7_VmInstanceOf(((ph7_class_instance *)pArg->x.pOther)->pClass,pClass));
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
					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){
						/* php 8.1: $GLOBALS cannot be passed by reference —
						 * a catchable Error with php's exact wording. */
						SyBlob sMsg;
						SyBlobInit(&sMsg,&pVm->sAllocator);
						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
							&pVmFunc->sName,n+1,&aFormalArg[n].sName);
						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);
						if( rc == PH7_ABORT ){
							goto Abort;
						}
						PH7_MemObjRelease(pTos);
						pTos = &pTos[-nCallArgs];
						pFrameStack = 0;
						rc = PH7_EXCEPTION;
						goto SkipFuncBody;
					}
					if( pArg->nIdx == SXU32_HIGH ){
						if((pArg->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0
						 && (pArg->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){
							/* php: a non-lvalue bound to a by-ref parameter is a catchable Error.
							 * PH7 warned and silently passed by value (same site as the other
							 * binder above). call_user_func()'s deliberate copy is exempt. */
							SyBlob sMsg;
							sxi32 rcRef;
							SyBlobInit(&sMsg,&pVm->sAllocator);
							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
								&pVmFunc->sName,n+1,&aFormalArg[n].sName);
							rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
								SyBlobLength(&sMsg));
							SyBlobRelease(&sMsg);
							return (rcRef == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
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
				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1
				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){
					/* The Closure instance carries an explicit bound $this
					 * (bindTo/bind/call): it wins over the creation-time
					 * captured $this, php-exact. */
					continue;
				}
				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){
					/* Captured by reference: link the name to the shared slot
					 * (no copy), mirroring the by-ref argument install above. */
					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){
						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),
							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));
					}
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
		/* Too-few-arguments check, placed AFTER the passed arguments were
		 * installed and type-checked: php's RECV order means a type error on
		 * a PASSED argument beats the count error (`f(int $x,$y)` called
		 * f("str") is a TypeError, not ArgumentCountError). The passed args
		 * were already released by the install loop, so the standard throw
		 * exit leaks nothing. The named path never fires this (its per-hole
		 * check ran in-loop; n == nNonVariadic >= nRequired here). Hosted
		 * builtin FUNCTIONS (VM_FUNC_INTERNAL) are exempt — their PHL
		 * signatures don't always mirror php's true arity and their in-body
		 * self-checks own php's wording (stage-2 family); hosted-class
		 * METHODS get php's ZPP wording via VmThrowBuiltinTooFewArgs. */
		if( n < SySetUsed(&pVmFunc->aArgs)
		 && (pVmFunc->iFlags & (VM_FUNC_INTERNAL|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){
			sxu32 nNonVar,nReq;
			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);
			if( n < nReq ){
				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;
				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){
					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
						nPassed,nReq,nNonVar);
				}else{
					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
						nPassed,nReq,nNonVar,TRUE);
				}
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
					/* Make sure the default argument is of the correct type.
					 * A null default on an implicitly-nullable param (`int $x = null`)
					 * must stay null — casting it to 0/""/false would diverge from PHP
					 * and contradict the explicit-null path, which now keeps it null. */
					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ
						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)
						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){
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
			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;
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
			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;
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
			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */
			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */
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
		VmCallArgMap *pCallMap2 = pEffCallMap;
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
			/* php accepts the "Class::method" STATIC-callable string everywhere a
			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).
			 * Split on the first "::" and route through the shared array-callable
			 * machinery ([class-name, method-name]) instead of warning undefined. */
			sxu32 iSep;
			int bScoped = 0;
			for( iSep = 1 ; iSep + 2 < sName.nByte ; ++iSep ){
				if( sName.zString[iSep] == ':' && sName.zString[iSep+1] == ':' ){
					bScoped = 1;
					break;
				}
			}
			if( bScoped ){
				ph7_hashmap *pCbMap = PH7_NewHashmap(&(*pVm),0,0);
				if( pCbMap ){
					ph7_value sCallable,sElem,sResult;
					sxi32 rcSm;
					pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
						nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
					SySetReset(&aArg);
					while( pArg < pTos ){
						SySetPut(&aArg,(const void *)&pArg);
						pArg++;
					}
					PH7_MemObjInit(pVm,&sElem);
					PH7_MemObjStringAppend(&sElem,sName.zString,iSep);
					PH7_HashmapInsert(pCbMap,0,&sElem);
					PH7_MemObjRelease(&sElem);
					PH7_MemObjInit(pVm,&sElem);
					PH7_MemObjStringAppend(&sElem,&sName.zString[iSep+2],sName.nByte-(iSep+2));
					PH7_HashmapInsert(pCbMap,0,&sElem);
					PH7_MemObjRelease(&sElem);
					PH7_MemObjInit(pVm,&sCallable);
					sCallable.x.pOther = pCbMap;
					MemObjSetType(&sCallable,MEMOBJ_HASHMAP);
					PH7_MemObjInit(pVm,&sResult);
					rcSm = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,(int)SySetUsed(&aArg),
						(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);
					SySetReset(&aArg);
					PH7_MemObjRelease(&sCallable);
					if( nCallArgs > 0 ){
						VmPopOperand(&pTos,nCallArgs);
					}
					if( rcSm == PH7_ABORT ){
						PH7_MemObjRelease(&sResult);
						goto Abort;
					}
					if( rcSm == PH7_EXCEPTION ){
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
					break;
				}
			}
			/* Call to an undefined function is a catchable Error in php 8 — it does
			 * NOT warn and hand back null and carry on, which is what PH7 did (and
			 * which quietly turned a typo into a null-propagating program). */
			{
			SyBlob sMsg;
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);
			/* Consume this call's captured spread runs so they don't leak into a
			 * later call (this path never reaches VmBuildEffectiveArgMap). */
			if( pInstr->iP2 ){
				VmSpreadConsume(pVm);
			}
			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the
			 * compile-time arg count on the stack, and this early exit skips the
			 * arg-building loop that the normal path uses, so popping only iP1 would
			 * strand the expanded elements and corrupt the enclosing expression. */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			PH7_MemObjRelease(pTos);
			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
				SyBlobLength(&sMsg));
			SyBlobRelease(&sMsg);
			if( rc == SXERR_ABORT ){
				goto Abort;
			}
			goto Exception;
			}
		}
		pFunc = (ph7_user_func *)pEntry->pUserData;
		/* Host function (builtin): build the effective spread-key map so the
		 * name-forwarding builtins (call_user_func & friends) relay string keys as
		 * named args, and — critically — so this call's captured runs are consumed.
		 * pArg is the top base here (a builtin call pops no method-name slot). */
		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
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
		/* Hand the call-site named-argument map to the builtin so name-forwarding
		 * helpers (call_user_func & friends) can relay name: arguments — and the
		 * caller's strict_types mode — to the inner callback. Forwarded whole (not
		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even
		 * when its own call site is purely positional; only the two forwarding
		 * builtins read pArgMap, so this is inert for every other host function. */
		sCtx.pArgMap = pEffCallMap;
		{
		int nGiven = (int)SySetUsed(&aArg);
		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum
		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError
		 * before the C routine runs when called with too few arguments — instead
		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""
		 * return. The message wording matches php's ZPP output byte-for-byte. */
		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){
			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",
				"%z() expects %s %d argument%s, %d given",
				&pFunc->sName,
				pFunc->bAtLeast ? "at least" : "exactly",
				(int)pFunc->nMinArg,
				pFunc->nMinArg == 1 ? "" : "s",
				nGiven);
		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,
			(ph7_value **)SySetBasePtr(&aArg))) ){
			/* TypeError thrown: rc carries the caught/uncaught status */
		}else{
			/* Call the foreign function */
			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));
		}
		}
		/* Release the call context */
		VmReleaseCallContext(&sCtx);
		if( rc == PH7_ABORT ){
			/* Release the (possibly partially-built) result slot before unwinding;
			 * the Abort: label only frees the operand stack, not this local
			 * (mirrors the PH7_EXCEPTION branch below). */
			PH7_MemObjRelease(&sRet);
			goto Abort;
		}
		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){
			/* A throw raised inside this host function — directly
			 * (PH7_VmThrowException) or by a PHP callback it invoked — was
			 * caught by an INLINE try (generator body) THIS exec owns.
			 * VmThrowInline records only a pc-redirect: a direct builtin throw
			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION
			 * with the redirect pending, so the rc branches below never land
			 * it (pre-existing hole: explode("") or a throwing usort
			 * comparator inside a generator's try lost the catch AND the
			 * yield). Land at the redirect now — its drain to the try's
			 * operand base subsumes the args + name pops. */
			PH7_MemObjRelease(&sRet);
			PH7_INLINE_RESUME_BREAK()
		}
		if( rc == PH7_EXCEPTION ){
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
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */
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
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */
			}
			/* Save fiber state: pc+1 is the instruction after this CALL.
			 * nTos is one below pTos so resume pushes at the return-value slot. */
			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);
			goto Suspend;
		}
		if( nCallArgs > 0 ){
			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack
			 * expanded the compile-time arg count on the stack, so popping iP1
			 * strands the extra elements (or, for an unpack that expanded to fewer
			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,
			 * reading a bogus value whose stray flags sent MemObjStore into the
			 * hashmap-release path and hung). Mirrors every other CALL exit. The
			 * function-name slot (pTos) receives the return value below. */
			VmPopOperand(&pTos,nCallArgs);
		}
		/* Save foreign function return value into the (now top) function-name slot */
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
		if( rc == PH7_ABORT || rc == PH7_EXCEPTION ){
			/* Drop any pending hook-RMW write-backs this activation armed — its
			 * statement is abandoned (only the innermost activation at throw time
			 * can own entries: the armed window spans exactly one instruction, so
			 * no OP_CALL record ever intervenes). */
			while( SySetUsed(&pVm->aHookRmw) > 0
			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){
				VmHookRmwDropTop(&(*pVm));
			}
		}
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
	sxu32 nCap;
	sxi32 rc;
	/* Allocate a new operand stack */
	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));
	if( pStack == 0 ){
		return SXERR_MEM;
	}
	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */
	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +
	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */
	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);
	/* Free the operand stack */
	SyMemBackendFree(&pVm->sAllocator,pStack);
	/* Execution result */
	return rc;
}
/*
 * Evaluate an attribute-argument bytecode with the attribute's DECLARING class
 * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside
 * the argument resolve against that class (like php) rather than the reflection
 * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves
 * the ambient scope untouched. Mirrors the property/const-initializer path.
 */
PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)
{
	ph7_class *pSaveCtx = pVm->pConstEvalClass;
	void *pSaveFrame = pVm->pConstEvalFrame;
	sxi32 rc;
	if( pDeclCls ){
		pVm->pConstEvalClass = pDeclCls;
		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);
	}
	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);
	pVm->pConstEvalClass = pSaveCtx;
	pVm->pConstEvalFrame = pSaveFrame;
	return rc;
}
/*
 * Invoke any installed shutdown callbacks.
 * Flush every still-open output buffer to the real output consumer at the end
 * of execution. php implicitly ends+flushes all ob_start() levels on shutdown
 * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script
 * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result
 * summary and then exit()s with a non-zero status — lost that output entirely.
 *
 * Buffer content is already callback-transformed (VmObConsumer applies handlers
 * at write time), and new output always lands in the topmost buffer, so the
 * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in
 * that order to the default consumer (sVmConsumer.xDef), then tear the stack
 * down and restore the default consumer.
 */
static void VmFlushOutputBuffers(ph7_vm *pVm)
{
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	sxu32 n,nUsed;
	nUsed = SySetUsed(&pVm->aOB);
	if( nUsed < 1 ){
		return;
	}
	for( n = 0 ; n < nUsed ; ++n ){
		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);
		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){
			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);
			pVm->nOutputLen += SyBlobLength(&pOb->sOB);
		}
	}
	/* Restore the default consumer and release the buffers. */
	pCons->xConsumer = pCons->xDef;
	pCons->pUserData = pCons->pDefData;
	for( n = 0 ; n < nUsed ; ++n ){
		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);
		if( pOb ){
			PH7_MemObjRelease(&pOb->sCallback);
			SyBlobRelease(&pOb->sOB);
		}
	}
	SySetReset(&pVm->aOB);
	pVm->nObDepth = 0;
}
/*
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
	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;
	 * pass &pVm->aOps so the growth updates the field that VM release frees. */
	{
		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;
		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);
	}
	/* Invoke any shutdown callbacks */
	VmInvokeShutdownCallbacks(&(*pVm));
	/* php flushes every still-open output buffer on shutdown — after the
	 * shutdown callbacks, which may still write into them. */
	VmFlushOutputBuffers(&(*pVm));
	/*
	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number
	 * so that any following call to [ph7_vm_exec()] without calling
	 * [ph7_vm_reset()] first would fail.
	 */
	return SXRET_OK;
}
/* ======================== Fiber Infrastructure ======================== */
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
	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;
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
	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;
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
/* call_user_func and call_user_func_array moved to vm_builtin_class.c */
static const ph7_builtin_func aVmFunc[] = {
	{ "__phl_magic_call", vm_builtin_magic_call },
	{ "__phl_enum_cases",   vm_builtin_enum_cases },
	{ "__phl_enum_from",    vm_builtin_enum_from },
	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },
	{ "enum_exists",        vm_builtin_enum_exists },
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
	{ "trait_exists",    vm_builtin_trait_exists      },
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
	{ "get_error_handler", vm_builtin_get_error_handler },
	{ "get_exception_handler", vm_builtin_get_exception_handler },
	{ "debug_backtrace",  vm_builtin_debug_backtrace},
	{ "error_get_last" ,  vm_builtin_error_get_last },
	{ "error_clear_last", vm_builtin_error_clear_last },
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
	{ "set_include_path",  vm_builtin_set_include_path },
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
PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)
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
PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)
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
