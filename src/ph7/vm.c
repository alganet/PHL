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
PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)
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
PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)
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
		pFunc->nMaxArg  = 0;
		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */
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
PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)
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
PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)
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
PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)
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
PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)
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
PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)
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
PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)
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
PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)
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
PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)
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
PH7_PRIVATE ph7_vm_func * VmOverload(
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
	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —
	 * separate php member namespaces. Both need their slots reserved, so mount
	 * over both tables. */
	SyHash *apMount[2];
	int iMount;
	apMount[0] = &pClass->hAttr;
	apMount[1] = &pClass->hConst;
	for( iMount = 0 ; iMount < 2 ; iMount++ ){
	/* Reset the loop cursor */
	SyHashResetLoopCursor(apMount[iMount]);
	/* Process only static and constant attribute */
	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){
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
				 * re-mount pass, so VM reuse still re-evaluates. Propagate a
				 * deferred static-default type failure to THIS class too, so a
				 * subclass's static access / instantiation throws like php's. */
				if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)
				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
					SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,
						(const void *)&pAttr->nIdx,sizeof(sxu32));
					if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){
						pClass->iFlags |= PH7_CLASS_STATIC_TYPE_DEFER;
					}
				}
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
				}else{
					/* The default was evaluated EAGERLY above, but php validates a
					 * typed static default LAZILY at the first static-property
					 * access / instantiation (a never-touched bad default is
					 * silent). Check now WITHOUT throwing — a pass coerces in
					 * place (int -> float widening, whole-real materialization,
					 * matching php's access-time value) and a failure is DEFERRED:
					 * the slot and the class are flagged, and the access sites
					 * throw via VmThrowDeferredStaticType. */
					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){
						pVmAttrS->iState |= VM_CLASS_ATTR_TYPE_DEFER;
						pClass->iFlags |= PH7_CLASS_STATIC_TYPE_DEFER;
					}
				}
				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){
					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);
					return SXERR_MEM;
				}
			}
		}
	}
	} /* for iMount */
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
	/* PHP-4-style constructors (a method named like the class) were REMOVED in
	 * PHP 8.0: such a method is now a plain method, never the constructor. We used
	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}`
	 * invoke c() as the ctor (and a required param there fataled at construction).
	 * No alias now — only an explicit __construct is the constructor. */
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
	int bDefThrew = 0; /* one throw per instantiation: php aborts construction
	                    * at the FIRST bad default; a second registered throw
	                    * would escape the catch as an uncaught fatal. */
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
				sxi32 rcExec;
				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);
				pVm->pConstEvalClass = pSaveCtx;
				if( rcExec == PH7_EXCEPTION || rcExec == PH7_ABORT ){
					/* The initializer itself threw (undefined constant, throwing
					 * enum case): its exception is already registered — do NOT
					 * also type-check the leftover value (a spurious second
					 * TypeError would escape the user's catch), and throw
					 * nothing further for the remaining attributes. */
					bDefThrew = 1;
				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){
					/* Typed property DEFAULT: php validates the computed value
					 * with the typed-CONSTANT rule (exact match or int->float
					 * widening — no weak coercion, `public int $p = "5"` throws)
					 * as a CATCHABLE TypeError when the default materializes,
					 * i.e. here at `new`. Park the throw for OP_NEW (which
					 * aborts construction) / the fetch-point router. */
					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);
					if( rcDef != SXRET_OK ){
						VmBoundaryPark(&(*pVm),rcDef);
						bDefThrew = 1;
					}
				}
			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
				/* Typed property without a default: mark uninitialized. Reading
				 * it before the first write is an Error in PHP 7.4+. */
				pVmAttr->iState |= VM_CLASS_ATTR_UNINIT;
			}
			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);
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
			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);
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
PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)
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
PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)
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
PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)
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

/*
 * Initialize a freshly allocated PH7 Virtual Machine so that we can
 * start compiling the target PHP program.
 */
PH7_PRIVATE sxi32 PH7_VmInit(
	 ph7_vm *pVm, /* Initialize this */
	 ph7 *pEngine /* Master engine */
	 )
{
	ph7_value *pObj;
	sxi32 rc;
	/* Zero the structure */
	SyZero(pVm,sizeof(ph7_vm));
	/* Initialize VM fields */
	pVm->pEngine = &(*pEngine);
	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */
	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays
	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()
	 * override these; bErrReport is the separate master gate installed by the CLI. */
	pVm->bDisplayErrors = 0;
	pVm->bLogErrors = 1;
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
	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);
	pVm->nResourceIdNext = 1;
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
	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by
	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */
	pVm->mtSeeded = FALSE;
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
	/* Classes/functions defined by the embedded builtin chunks below are
	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */
	pVm->bCompilingBuiltin = 1;
	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */
	PH7_VmInstallBuiltinLib(&(*pVm));
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
PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)
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
PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)
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
PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)
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
 * php-visible id of a resource. PHL's resource value is a bare void*, so the
 * mapping lives in a per-VM registry: the first time a pointer is asked about it
 * takes the next id, and every later lookup returns the same one. That is what
 * makes (int)$res the id php prints, and what keeps two live resources from
 * comparing equal — both used to cast to 1.
 *
 * The record's own `pRes` field is the hash key: SyHash stores the key POINTER
 * (it does not copy), so the key has to outlive the entry. Returns 0 when the
 * registry cannot grow, which renders as php's "closed/unknown" id rather than
 * aborting a cast.
 */
PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)
{
	SyHashEntry *pEntry;
	phl_res_id *pRec;
	if( pVm == 0 || pRes == 0 ){
		return 0;
	}
	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));
	if( pEntry ){
		return ((phl_res_id *)pEntry->pUserData)->nId;
	}
	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));
	if( pRec == 0 ){
		return 0;
	}
	pRec->pRes = pRes;
	pRec->nId = pVm->nResourceIdNext++;
	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){
		SyMemBackendPoolFree(&pVm->sAllocator,pRec);
		return 0;
	}
	return pRec->nId;
}
/*
 * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at
 * 1 for the next run, matching a fresh php process.
 */
static void VmResetResourceIds(ph7_vm *pVm)
{
	SyHashEntry *pEntry;
	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){
		pVm->nResourceIdNext = 1;
		return;
	}
	SyHashResetLoopCursor(&pVm->hResourceId);
	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){
		if( pEntry->pUserData ){
			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);
		}
	}
	SyHashRelease(&pVm->hResourceId);
	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);
	pVm->nResourceIdNext = 1;
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
	/* (4b) Drop the resource-id registry: the resources it named are gone with
	 * the object pool, and a re-executed program should number from 1 again. */
	VmResetResourceIds(&(*pVm));
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
			/* Constants live in the separate hConst namespace; invalidate their
			 * slots too so VM reuse re-evaluates them. */
			SyHashResetLoopCursor(&pClass->hConst);
			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){
				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;
				pAttr->nIdx = SXU32_HIGH;
				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;
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
PH7_PRIVATE sxi32 VmInitCallContext(
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
PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)
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
PH7_PRIVATE void VmPopOperand(
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
PH7_PRIVATE sxi32 VmHashmapRefInsert(
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
 * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"
 * (case-insensitive) are true, any other string is true iff it parses as a
 * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).
 */
static int VmIniBool(const char *zValue,sxu32 nValue)
{
	sxi64 iVal = 0;
	if( nValue == 0 ){
		return 0;
	}
	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)
	 || (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)
	 || (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){
		return 1;
	}
	SyStrToInt64(zValue,nValue,(void *)&iVal,0);
	return iVal != 0;
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
	case PH7_VM_CONFIG_ERR_STREAM: {
		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);
		void *pUserData = va_arg(ap,void *);
		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and
		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)
		 * instead of the program-output stream. */
#ifdef UNTRUST
		if( xConsumer == 0 ){
			rc = SXERR_CORRUPT;
			break;
		}
#endif
		pVm->sVmErrConsumer.xConsumer = xConsumer;
		pVm->sVmErrConsumer.pUserData = pUserData;
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
			}else if( nName == sizeof("display_errors")-1
			 && SyMemcmp(zName,"display_errors",nName) == 0 ){
				/* Mirror the display_errors gate C-side so it takes effect even
				 * if the script never touches the INI API (ini_set keeps it in
				 * sync at runtime via __ini_apply_err). */
				pVm->bDisplayErrors = VmIniBool(zValue,nValue);
			}else if( nName == sizeof("log_errors")-1
			 && SyMemcmp(zName,"log_errors",nName) == 0 ){
				pVm->bLogErrors = VmIniBool(zValue,nValue);
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
PH7_PRIVATE sxi32 VmSuspendCtx(
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
 * positional-overlaps-named) it raises php's CATCHABLE \Error via
 * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the
 * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.
 */
PH7_PRIVATE sxi32 VmResolveNamedArgs(
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
						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));
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
					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));
				}
			}
		}else{
			/* Positional argument. Source-syntax calls can't reach here after a
			 * named arg (the parser rejects it at compile time), but a call
			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —
			 * can, so enforce PHP's rule at this shared choke point. */
			if( bSeenNamed ){
				return VmThrowNamedArgError(&(*pVm),
					"Cannot use positional argument after named argument",
					sizeof("Cannot use positional argument after named argument") - 1);
			}
			if( (sxu32)posIdx < nNonVariadic ){
				if( aUsed[posIdx] ){
					SyBufferFormat(zErrMsg,sizeof(zErrMsg),
						"Named parameter $%.*s overwrites previous argument",
						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);
					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));
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
PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)
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
PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)
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
PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)
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
PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)
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
PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)
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
PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)
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
PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,
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
 * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths
 * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.
 */
PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)
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
PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)
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
PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)
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
PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)
{
	VmMagicGuard sG;
	sG.pThis = pThis;
	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);
	sG.cKind = cKind;
	SySetPut(&pVm->aMagicGuard,(const void *)&sG);
}
PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)
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
PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)
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
PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)
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
PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)
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
PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)
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
PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)
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
PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)
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
PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)
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
PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)
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
PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)
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
	{ "settype",   vm_builtin_settype              },
	{ "get_resource_type", vm_builtin_get_resource_type},
	{ "get_resource_id", vm_builtin_get_resource_id},
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
 * php: a leading '\' anchors a class/interface/trait/enum name to the global
 * namespace root. A stored name never carries one (the compiler qualifies to
 * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE
 * leading backslash before a hClass lookup — only one, since a literal
 * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a
 * lookup key is universally safe: no stored key begins with '\', so it can
 * only turn a failing lookup into a match, never break an existing one.
 * Shared by PH7_VmExtractClass (the central resolver — string `new`,
 * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,
 * enum_exists via VmExtractEnumClass, instanceof over a string) and the
 * *_exists()/class_alias() builtins that hash hClass directly.
 */
PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)
{
	if( *pnByte > 0 && (*pzName)[0] == '\\' ){
		(*pzName)++;
		(*pnByte)--;
	}
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
	sxu32 nOrig = nByte;
	SXUNUSED(iNest);
	/* Exact class lookup.
	 * Static names are already namespace-qualified by the compiler.
	 * Dynamic names (from variables) use exact match only, matching PHP behavior.
	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip
	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */
	PH7_VmClassNameAnchor(&zName,&nByte);
	/* An empty stripped name never matches a stored key (none is empty); skip the
	 * hash probe. But php still fires the autoloader when the ORIGINAL name was
	 * non-empty — a lone "\" autoloads with the empty stripped name, whereas a
	 * truly empty "" does not. Gate autoload on nOrig, pass the stripped name. */
	pEntry = nByte > 0 ? SyHashGet(&pVm->hClass,(const void *)zName,nByte) : 0;
	if( pEntry == 0 ){
		/* Class not found in hash table — try autoload before giving up */
		return nOrig > 0 ? VmTriggerAutoload(pVm,zName,nByte,iLoadable) : 0;
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
