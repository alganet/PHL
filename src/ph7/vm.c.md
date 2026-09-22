# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2522/2980 lines (84.63%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `#include <stddef.h>` |
|         - |    8 | `#include <stdlib.h>` |
|         - |    9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |   10 | `#include <math.h>` |
|         - |   11 | `#endif` |
|         - |   12 | `/* Signed 64-bit integer overflow detection lives in ph7int.h as the shared` |
|         - |   13 | ` * PH7_{ADD,SUB,MUL}_OVERFLOW64 macros (GCC/Clang intrinsics, MSVC fallbacks in` |
|         - |   14 | ` * memobj.c). The executor uses them to promote an overflowing integer` |
|         - |   15 | ` * operation to a float, matching PHP. */` |
|         - |   16 | `/*` |
|         - |   17 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|         - |   18 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|         - |   19 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|         - |   20 | ` * statements.` |
|         - |   21 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|         - |   22 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|         - |   23 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|         - |   24 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|         - |   25 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|         - |   26 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|         - |   27 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|         - |   28 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|         - |   29 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|         - |   30 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|         - |   31 | ` * and so on.` |
|         - |   32 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|         - |   33 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|         - |   34 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|         - |   35 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|         - |   36 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|         - |   37 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|         - |   38 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|         - |   39 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|         - |   40 | ` */` |
|         - |   41 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|         - |   42 | `/*` |
|         - |   43 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|         - |   44 | ` * memory object index is stored in an instance of the following structure and put` |
|         - |   45 | ` * in the free object table so that it can be reused again without allocating` |
|         - |   46 | ` * a new memory object.` |
|         - |   47 | ` */` |
|         - |   48 | `/* VmSlot struct moved to ph7int.h */` |
|         - |   49 | `/*` |
|         - |   50 | ` * An entry in the reference table is represented by an instance of the` |
|         - |   51 | ` * follwoing table.` |
|         - |   52 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - |   53 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - |   54 | ` * the reference implementation is consistent,solid and it's` |
|         - |   55 | ` * behavior resemble the C++ reference mechanism.` |
|         - |   56 | ` * Refer to the official for more information on this powerful` |
|         - |   57 | ` * extension.` |
|         - |   58 | ` */` |
|         - |   59 | `/* struct VmRefObj + VM_REF_IDX_KEEP moved to ph7int.h */` |
|         - |   60 | `/*` |
|         - |   61 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|         - |   62 | ` * is stored in an instance of the following structure.` |
|         - |   63 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|         - |   64 | ` */` |
|         - |   65 | `/* VmShutdownCB struct moved to ph7int.h */` |
|         - |   66 | `/*` |
|         - |   67 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|         - |   68 | ` * is stored in an instance of the following structure.` |
|         - |   69 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|         - |   70 | ` */` |
|         - |   71 | `/* VmAutoloadCB struct moved to ph7int.h */` |
|         - |   72 |  |
|         - |   73 | `/*` |
|         - |   74 | ` * Return TRUE if either operand is a NaN real value.` |
|         - |   75 | ` */` |
|   1888982 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   77 | `{` |
|   1888987 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|        42 |   79 | `		return TRUE;` |
|         - |   80 | `	}` |
|   1888947 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        11 |   82 | `		return TRUE;` |
|         - |   83 | `	}` |
|   1888937 |   84 | `	return FALSE;` |
|    945502 |   85 | `}` |
|         - |   86 | `/*` |
|         - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |   93 | ` * still go through the existing numeric coercion.` |
|         - |   94 | ` */` |
|    637712 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |   96 | `{` |
|         - |   97 | `	SyString sStr;` |
|    637717 |   98 | `	sxu8 bReal = FALSE;` |
|    637717 |   99 | `	const char *zTail = 0;` |
|         - |  100 | `	const char *zEnd;` |
|    637717 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    637699 |  102 | `		return FALSE;` |
|         - |  103 | `	}` |
|        21 |  104 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        21 |  105 | `	if( sStr.nByte == 0 ){` |
|       ! 0 |  106 | `		return TRUE;` |
|         - |  107 | `	}` |
|        21 |  108 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|         5 |  109 | `		return TRUE;` |
|         - |  110 | `	}` |
|         - |  111 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|         - |  112 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|         - |  113 | `	 * still takes the Perl path. */` |
|        17 |  114 | `	zEnd = sStr.zString + sStr.nByte;` |
|        17 |  115 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|       ! 0 |  116 | `		zTail++;` |
|       ! 0 |  117 | `	}` |
|        17 |  118 | `	return zTail < zEnd;` |
|    319067 |  119 | `}` |
|         - |  120 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|         - |  121 | `/* Constant expander used by define(); used below to recognise user-defined` |
|         - |  122 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|         - |  123 | ` * a define() overwrites them. */` |
|         - |  124 | `/*` |
|         - |  125 | ` * Register a constant and it's associated expansion callback so that` |
|         - |  126 | ` * it can be expanded from the target PHP program.` |
|         - |  127 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|         - |  128 | ` * simple and work as follows:` |
|         - |  129 | ` * Each registered constant have a C procedure associated with it.` |
|         - |  130 | ` * This procedure known as the constant expansion callback is responsible` |
|         - |  131 | ` * of expanding the invoked constant to the desired value,for example:` |
|         - |  132 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|         - |  133 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|         - |  134 | ` * (Windows,Linux,...) and so on.` |
|         - |  135 | ` * Please refer to the official documentation for additional information.` |
|         - |  136 | ` */` |
|   1784150 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  139 | `	const SyString *pName,  /* Constant name */` |
|         - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  142 | `	)` |
|         5 |  143 | `{` |
|   1784155 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  145 | `}` |
|         - |  146 | `/*` |
|         - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  149 | ` */` |
|   1784310 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
|         - |  151 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  152 | `	const SyString *pName,  /* Constant name */` |
|         - |  153 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  154 | `	void *pUserData,        /* Last argument to xExpand() */` |
|         - |  155 | `	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */` |
|         - |  156 | `	sxu32 nLine,            /* Declaration line, 0 = unknown */` |
|         - |  157 | `	int bUser               /* 1 when defined by user code */` |
|         - |  158 | `	)` |
|         5 |  159 | `{` |
|         - |  160 | `	ph7_constant *pCons;` |
|         - |  161 | `	SyHashEntry *pEntry;` |
|         - |  162 | `	char *zDupName;` |
|         - |  163 | `	sxi32 rc;` |
|   1784315 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   1784315 |  165 | `	if( pEntry ){` |
|         - |  166 | `		/* Overwrite the old definition and return immediately */` |
|         3 |  167 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  168 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|         - |  169 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|         - |  170 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|         2 |  171 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|         3 |  172 | `		 && pCons->pUserData != pUserData ){` |
|         3 |  173 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|         3 |  174 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|         1 |  175 | `		}` |
|         3 |  176 | `		pCons->xExpand = xExpand;` |
|         3 |  177 | `		pCons->pUserData = pUserData;` |
|         3 |  178 | `		if( pFile ){` |
|         3 |  179 | `			SyStringDupPtr(&pCons->sFile,pFile);` |
|         2 |  180 | `		}else{` |
|       ! 0 |  181 | `			SyStringInitFromBuf(&pCons->sFile,0,0);` |
|         - |  182 | `		}` |
|         3 |  183 | `		pCons->nLine = nLine;` |
|         3 |  184 | `		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         3 |  185 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|         3 |  186 | `		return SXRET_OK;` |
|         - |  187 | `	}` |
|         - |  188 | `	/* Allocate a new constant instance */` |
|   1784313 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   1784313 |  190 | `	if( pCons == 0 ){` |
|       ! 0 |  191 | `		return 0;` |
|         - |  192 | `	}` |
|         - |  193 | `	/* Duplicate constant name */` |
|   1784313 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   1784313 |  195 | `	if( zDupName == 0 ){` |
|       ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  197 | `		return 0;` |
|         - |  198 | `	}` |
|   1784313 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|   1784313 |  200 | `	if( pFile ){` |
|       163 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|        79 |  202 | `	}` |
|   1784313 |  203 | `	pCons->nLine = nLine;` |
|   1784313 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         - |  205 | `	/* Install the constant */` |
|   1784313 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   1784313 |  207 | `	pCons->xExpand = xExpand;` |
|   1784313 |  208 | `	pCons->pUserData = pUserData;` |
|   1784313 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   1784313 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   1784313 |  211 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  214 | `		return rc;` |
|         - |  215 | `	}` |
|         - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|   1784313 |  217 | `	return SXRET_OK;` |
|    892160 |  218 | `}` |
|         - |  219 | `/*` |
|         - |  220 | ` * Allocate a new foreign function instance.` |
|         - |  221 | ` * This function return SXRET_OK on success. Any other` |
|         - |  222 | ` * return value indicates failure.` |
|         - |  223 | ` * Please refer to the official documentation for an introduction to` |
|         - |  224 | ` * the foreign function mechanism.` |
|         - |  225 | ` */` |
|   2607820 |  226 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
|         - |  227 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  228 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  229 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  230 | `	void *pUserData,          /* Foreign function private data */` |
|         - |  231 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|         - |  232 | `	)` |
|         5 |  233 | `{` |
|         - |  234 | `	ph7_user_func *pFunc;` |
|         - |  235 | `	char *zDup;` |
|         - |  236 | `	/* Allocate a new user function */` |
|   2607825 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   2607825 |  238 | `	if( pFunc == 0 ){` |
|       ! 0 |  239 | `		return SXERR_MEM;` |
|         - |  240 | `	}` |
|         - |  241 | `	/* Duplicate function name */` |
|   2607825 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   2607825 |  243 | `	if( zDup == 0 ){` |
|       ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  245 | `		return SXERR_MEM;` |
|         - |  246 | `	}` |
|         - |  247 | `	/* Zero the structure */` |
|   2607825 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  249 | `	/* Initialize structure fields */` |
|   2607825 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   2607825 |  251 | `	pFunc->pVm   = pVm;` |
|   2607825 |  252 | `	pFunc->xFunc = xFunc;` |
|   2607825 |  253 | `	pFunc->pUserData = pUserData;` |
|   2607825 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  255 | `	/* Write a pointer to the new function */` |
|   2607825 |  256 | `	*ppOut = pFunc;` |
|   2607825 |  257 | `	return SXRET_OK;` |
|   1303915 |  258 | `}` |
|         - |  259 | `/*` |
|         - |  260 | ` * Install a foreign function and it's associated callback so that` |
|         - |  261 | ` * it can be invoked from the target PHP code.` |
|         - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  263 | ` * return value indicates failure.` |
|         - |  264 | ` * Please refer to the official documentation for an introduction to` |
|         - |  265 | ` * the foreign function mechanism.` |
|         - |  266 | ` */` |
|   2512160 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|         - |  268 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  269 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  270 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  271 | `	void *pUserData           /* Foreign function private data */` |
|         - |  272 | `	)` |
|         5 |  273 | `{` |
|         - |  274 | `	ph7_user_func *pFunc;` |
|         - |  275 | `	SyHashEntry *pEntry;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Overwrite any previously registered function with the same name */` |
|   2512165 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   2512165 |  279 | `	if( pEntry ){` |
|      3961 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      3961 |  281 | `		pFunc->pUserData = pUserData;` |
|      3961 |  282 | `		pFunc->xFunc = xFunc;` |
|      3961 |  283 | `		SySetReset(&pFunc->aAux);` |
|         - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|      3961 |  288 | `		pFunc->nMinArg  = 0;` |
|      3961 |  289 | `		pFunc->nMaxArg  = 0;` |
|      3961 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|      3961 |  291 | `		pFunc->bAtLeast = 0;` |
|      3961 |  292 | `		return SXRET_OK;` |
|         - |  293 | `	}` |
|         - |  294 | `	/* Create a new user function */` |
|   2508209 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   2508209 |  296 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  297 | `		return rc;` |
|         - |  298 | `	}` |
|         - |  299 | `	/* Install the function in the corresponding hashtable */` |
|   2508209 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   2508209 |  301 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  304 | `		return rc;` |
|         - |  305 | `	}` |
|         - |  306 | `	/* User function successfully installed */` |
|   2508209 |  307 | `	return SXRET_OK;` |
|   1256085 |  308 | `}` |
|         - |  309 | `/*` |
|         - |  310 | ` * Initialize a VM function.` |
|         - |  311 | ` */` |
|   3979000 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  315 | `	const char *zName,  /* Function name */` |
|         - |  316 | `	sxu32 nByte,        /* zName length */` |
|         - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  318 | `	void *pUserData     /* Function private data */` |
|         - |  319 | `	)` |
|         5 |  320 | `{` |
|         - |  321 | `	/* Zero the structure */` |
|   3979005 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  323 | `	/* Initialize structure fields */` |
|         - |  324 | `	/* Arguments container */` |
|   3979005 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  326 | `	/* Static variable container */` |
|   3979005 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  328 | `	/* Bytecode container */` |
|   3979005 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  330 | `    /* Preallocate some instruction slots */` |
|   3979005 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  332 | `	/* Closure environment */` |
|   3979005 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   3979005 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  336 | `	/* Declared #[...] attributes */` |
|   3979005 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   3979005 |  338 | `	pFunc->iFlags = iFlags;` |
|   3979005 |  339 | `	pFunc->pUserData = pUserData;` |
|         - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   3979005 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   3979005 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   3971061 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   1985533 |  346 | `	}else{` |
|         - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|      7949 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      7949 |  351 | `		if( pFile ){` |
|      7949 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|      3972 |  353 | `		}` |
|         - |  354 | `	}` |
|   3979005 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   3979005 |  356 | `	return SXRET_OK;` |
|         5 |  357 | `}` |
|         - |  358 | `/*` |
|         - |  359 | ` * Namespace-aware function lookup.` |
|         - |  360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  362 | ` */` |
|         - |  363 | `/*` |
|         - |  364 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  365 | ` */` |
|   6887370 |  366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  367 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  368 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  369 | `	SyString *pName     /* Function name */` |
|         - |  370 | `	)` |
|         5 |  371 | `{` |
|         - |  372 | `	SyHashEntry *pEntry;` |
|         - |  373 | `	sxi32 rc;` |
|   6887375 |  374 | `	if( pName == 0 ){` |
|         - |  375 | `		/* Use the built-in name */` |
|    598637 |  376 | `		pName = &pFunc->sName;` |
|    299316 |  377 | `	}` |
|         - |  378 | `	/* Check for duplicates (functions with the same name) first */` |
|   6887375 |  379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   6887375 |  380 | `	if( pEntry ){` |
|   3466625 |  381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   3466625 |  382 | `		if( pLink != pFunc ){` |
|         - |  383 | `			/* Link */` |
|        50 |  384 | `			pFunc->pNextName = pLink;` |
|        50 |  385 | `			pEntry->pUserData = pFunc;` |
|        23 |  386 | `		}` |
|   3466625 |  387 | `		return SXRET_OK;` |
|         - |  388 | `	}` |
|         - |  389 | `	/* First time seen */` |
|   3420755 |  390 | `	pFunc->pNextName = 0;` |
|   3420755 |  391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   3420755 |  392 | `	return rc;` |
|   3443690 |  393 | `}` |
|         - |  394 | `/*` |
|         - |  395 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  396 | ` */` |
|    618814 |  397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  398 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  399 | `	ph7_class *pClass /* Target Class */` |
|         - |  400 | `	)` |
|         5 |  401 | `{` |
|    618819 |  402 | `	SyString *pName = &pClass->sName;` |
|         - |  403 | `	SyHashEntry *pEntry;` |
|         - |  404 | `	sxi32 rc;` |
|         - |  405 | `	/* Check for duplicates */` |
|    618819 |  406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    618819 |  407 | `	if( pEntry ){` |
|         3 |  408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  409 | `		/* Link entry with the same name */` |
|         3 |  410 | `		pClass->pNextName = pLink;` |
|         3 |  411 | `		pEntry->pUserData = pClass;` |
|         3 |  412 | `		return SXRET_OK;` |
|         - |  413 | `	}` |
|    618817 |  414 | `	pClass->pNextName = 0;` |
|         - |  415 | `	/* Perform a simple hashtable insertion */` |
|    618817 |  416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    618817 |  417 | `	return rc;` |
|    309412 |  418 | `}` |
|         - |  419 | `/*` |
|         - |  420 | ` * Instruction builder interface.` |
|         - |  421 | ` */` |
| 115489474 |  422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  423 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  424 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  425 | `	sxi32 iP1,    /* First operand */` |
|         - |  426 | `	sxu32 iP2,    /* Second operand */` |
|         - |  427 | `	void *p3,     /* Third operand */` |
|         - |  428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  429 | `	)` |
|         5 |  430 | `{` |
|         - |  431 | `	VmInstr sInstr;` |
| 115489479 |  432 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  433 | `	sxi32 rc;` |
|         - |  434 | `	/* Fill the VM instruction */` |
| 115489479 |  435 | `	sInstr.iOp = (sxu8)iOp;` |
| 115489479 |  436 | `	sInstr.iP1 = iP1;` |
| 115489479 |  437 | `	sInstr.iP2 = iP2;` |
| 115489479 |  438 | `	sInstr.p3  = p3;` |
|         - |  439 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  440 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  441 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  442 | `	 * between statements, hence the range check. */` |
| 115489479 |  443 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
| 115489479 |  444 | `	sInstr.nLine = 0;` |
| 115489479 |  445 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|  49016201 |  446 | `		sInstr.nLine = pGen->pIn->nLine;` |
|  90981381 |  447 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  448 | `		/* Past the end (statement tail): blame the last real token. */` |
|  66336941 |  449 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|  33168468 |  450 | `	}` |
| 115489479 |  451 | `	if( pIndex ){` |
|         - |  452 | `		/* Instruction index in the bytecode array */` |
|   7954593 |  453 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   3977294 |  454 | `	}` |
|         - |  455 | `	/* Finally,record the instruction */` |
| 115489479 |  456 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 115489479 |  457 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  458 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  459 | `		/* Fall throw */` |
|       ! 0 |  460 | `	}` |
| 115489479 |  461 | `	return rc;` |
|         5 |  462 | `}` |
|         - |  463 | `/*` |
|         - |  464 | ` * Swap the current bytecode container with the given one.` |
|         - |  465 | ` */` |
|  11183056 |  466 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  467 | `{` |
|  11183061 |  468 | `	if( pContainer == 0 ){` |
|         - |  469 | `		/* Point to the default container */` |
|       ! 0 |  470 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  471 | `	}else{` |
|         - |  472 | `		/* Change container */` |
|  11183061 |  473 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  474 | `	}` |
|  11183061 |  475 | `	return SXRET_OK;` |
|         5 |  476 | `}` |
|         - |  477 | `/*` |
|         - |  478 | ` * Return the current bytecode container.` |
|         - |  479 | ` */` |
|  11169902 |  480 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  481 | `{` |
|  11169907 |  482 | `	return pVm->pByteContainer;` |
|         5 |  483 | `}` |
|         - |  484 | `/*` |
|         - |  485 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  486 | ` */` |
|   2612654 |  487 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  488 | `{` |
|         - |  489 | `	VmInstr *pInstr;` |
|   2612659 |  490 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   2612659 |  491 | `	return pInstr;` |
|         5 |  492 | `}` |
|         - |  493 | `/*` |
|         - |  494 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  495 | ` */` |
|  67638212 |  496 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  497 | `{` |
|  67638217 |  498 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  499 | `}` |
|         - |  500 | `/*` |
|         - |  501 | ` * Pop the last VM instruction.` |
|         - |  502 | ` */` |
|   6344794 |  503 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  504 | `{` |
|   6344799 |  505 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  506 | `}` |
|         - |  507 | `/*` |
|         - |  508 | ` * Peek the last VM instruction.` |
|         - |  509 | ` */` |
|  23988378 |  510 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  511 | `{` |
|  23988383 |  512 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  513 | `}` |
|   2026808 |  514 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  515 | `{` |
|         - |  516 | `	VmInstr *aInstr;` |
|         - |  517 | `	sxu32 n;` |
|   2026813 |  518 | `	n = SySetUsed(pVm->pByteContainer);` |
|   2026813 |  519 | `	if( n < 2 ){` |
|       ! 0 |  520 | `		return 0;` |
|         - |  521 | `	}` |
|   2026813 |  522 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|   2026813 |  523 | `	return &aInstr[n - 2];` |
|   1013409 |  524 | `}` |
|         - |  525 | `/*` |
|         - |  526 | ` * Allocate a new virtual machine frame.` |
|         - |  527 | ` */` |
|   5086654 |  528 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  529 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  530 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  531 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  532 | `	)` |
|         5 |  533 | `{` |
|         - |  534 | `	VmFrame *pFrame;` |
|         - |  535 | `	/* Allocate a new vm frame */` |
|   5086659 |  536 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   5086659 |  537 | `	if( pFrame == 0 ){` |
|       ! 0 |  538 | `		return 0;` |
|         - |  539 | `	}` |
|         - |  540 | `	/* Zero the structure */` |
|   5086659 |  541 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  542 | `	/* Initialize frame fields */` |
|   5086659 |  543 | `	pFrame->pUserData = pUserData;` |
|   5086659 |  544 | `	pFrame->pThis = pThis;` |
|   5086659 |  545 | `	pFrame->pVm = pVm;` |
|   5086659 |  546 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   5086659 |  547 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   5086659 |  548 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   5086659 |  549 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   5086659 |  550 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  551 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  552 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   5086659 |  553 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   5086659 |  554 | `	return pFrame;` |
|   2543530 |  555 | `}` |
|         - |  556 | `/* Forward declaration */` |
|         - |  557 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  558 | `/*` |
|         - |  559 | ` * Enter a VM frame.` |
|         - |  560 | ` */` |
|   5086012 |  561 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  562 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  563 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  564 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  565 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  566 | `	)` |
|         5 |  567 | `{` |
|         - |  568 | `	VmFrame *pFrame;` |
|         - |  569 | `	/* Allocate a new frame */` |
|   5086017 |  570 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   5086017 |  571 | `	if( pFrame == 0 ){` |
|       ! 0 |  572 | `		return SXERR_MEM;` |
|         - |  573 | `	}` |
|         - |  574 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   5086017 |  575 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  576 | `	/* Link to the list of active VM frame */` |
|   5086017 |  577 | `	pFrame->pParent = pVm->pFrame;` |
|   5086017 |  578 | `	pVm->pFrame = pFrame;` |
|   5086017 |  579 | `	if( ppFrame ){` |
|         - |  580 | `		/* Write a pointer to the new VM frame */` |
|   5081481 |  581 | `		*ppFrame = pFrame;` |
|   2540936 |  582 | `	}` |
|   5086017 |  583 | `	return SXRET_OK;` |
|   2543209 |  584 | `}` |
|         - |  585 | `/*` |
|         - |  586 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  587 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  588 | ` * information.` |
|         - |  589 | ` */` |
|        70 |  590 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  591 | `{` |
|         - |  592 | `	VmFrame *pTarget,*pFrame;` |
|        75 |  593 | `	SyHashEntry *pEntry = 0;` |
|         - |  594 | `	sxi32 rc;` |
|         - |  595 | `	/* Point to the upper frame */` |
|        75 |  596 | `	pFrame = pVm->pFrame;` |
|        75 |  597 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        75 |  598 | `	pTarget = pFrame;` |
|        75 |  599 | `	pFrame = pTarget->pParent;` |
|        83 |  600 | `	while( pFrame ){` |
|        81 |  601 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  602 | `			/* Query the current frame */` |
|        79 |  603 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|        79 |  604 | `			if( pEntry ){` |
|         - |  605 | `				/* Variable found */` |
|        73 |  606 | `				break;` |
|         - |  607 | `			}` |
|         3 |  608 | `		}` |
|         - |  609 | `		/* Point to the upper frame */` |
|         9 |  610 | `		pFrame = pFrame->pParent;` |
|         1 |  611 | `	}` |
|        75 |  612 | `	if( pEntry == 0 ){` |
|         - |  613 | `		/* Inexistant variable */` |
|         3 |  614 | `		return SXERR_NOTFOUND;` |
|         - |  615 | `	}` |
|         - |  616 | `	/* Link to the current frame */` |
|        73 |  617 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|        73 |  618 | `	if( rc == SXRET_OK ){` |
|         - |  619 | `		sxu32 nIdx;` |
|        73 |  620 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        73 |  621 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|        34 |  622 | `	}` |
|        73 |  623 | `	return rc;` |
|        40 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  627 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  628 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  629 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  630 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  631 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  632 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  633 | ` */` |
|   5081944 |  634 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  635 | `{` |
|   5081949 |  636 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 |  637 | `		pVm->pResumeFrame = 0;` |
|       ! 0 |  638 | `	}` |
|   5081949 |  639 | `}` |
|         - |  640 | `/*` |
|         - |  641 | ` * Leave the top-most active frame.` |
|         - |  642 | ` */` |
|   5081262 |  643 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  644 | `{` |
|   5081267 |  645 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   5081267 |  646 | `	if( pCurFrame ){` |
|         - |  647 | `		/* Unlink from the list of active VM frame */` |
|   5081267 |  648 | `		pVm->pFrame = pCurFrame->pParent;` |
|   5081267 |  649 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  650 | `			VmSlot  *aSlot;` |
|         - |  651 | `			sxu32 n;` |
|         - |  652 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   2202065 |  653 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   9074297 |  654 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|         - |  655 | `				/* Unset the local variable */` |
|   6872237 |  656 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   3437309 |  657 | `			}` |
|         - |  658 | `			/* Remove local reference */` |
|   2202065 |  659 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   9074399 |  660 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   6872339 |  661 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   3437360 |  662 | `			}` |
|   1101228 |  663 | `		}` |
|         - |  664 | `		/* Release internal containers */` |
|   5081267 |  665 | `		SyHashRelease(&pCurFrame->hVar);` |
|   5081267 |  666 | `		SySetRelease(&pCurFrame->sArg);` |
|   5081267 |  667 | `		SySetRelease(&pCurFrame->sLocal);` |
|   5081267 |  668 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  669 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  670 | `		 * containers above — released for every frame, including transparent` |
|         - |  671 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   5081267 |  672 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  673 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   5081267 |  674 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  675 | `		/* Release the whole structure */` |
|   5081267 |  676 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   2540829 |  677 | `	}` |
|   5081267 |  678 | `}` |
|         - |  679 | `/*` |
|         - |  680 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  681 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  682 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  683 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  684 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  685 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  686 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  687 | ` */` |
|         - |  688 | `/*` |
|         - |  689 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  690 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  691 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  692 | ` *` |
|         - |  693 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  694 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  695 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  696 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  697 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  698 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  699 | ` */` |
|      6896 |  700 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  701 | `{` |
|         - |  702 | `	VmFrame *pFrame;` |
|     13933 |  703 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|      7059 |  704 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  705 | `		sxu32 n;` |
|      7813 |  706 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|       781 |  707 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  708 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|        24 |  709 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|        24 |  710 | `				(void)SySetPop(&pFrame->sLocal);` |
|        24 |  711 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  712 | `			}` |
|       382 |  713 | `		}` |
|      3521 |  714 | `	}` |
|      6879 |  715 | `	return FALSE;` |
|      3453 |  716 | `}` |
|       110 |  717 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         3 |  718 | `{` |
|         - |  719 | `	VmRefObj *pRef;` |
|       113 |  720 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       113 |  721 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       113 |  722 | `	if( pRef ){` |
|       113 |  723 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|        55 |  724 | `	}` |
|       113 |  725 | `}` |
|         - |  726 | `/*` |
|         - |  727 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  728 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  729 | ` * should be skipped when looking for the real execution context.` |
|         - |  730 | ` */` |
|  59864890 |  731 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  732 | `{` |
|  72314121 |  733 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  12449231 |  734 | `		pFrame = pFrame->pParent;` |
|         5 |  735 | `	}` |
|  59864895 |  736 | `	return pFrame;` |
|         5 |  737 | `}` |
|         - |  738 | `/*` |
|         - |  739 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  740 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  741 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  742 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  743 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  744 | ` *` |
|         - |  745 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  746 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  747 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  748 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  749 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  750 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  751 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  752 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  753 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  754 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  755 | ` *` |
|         - |  756 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  757 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  758 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  759 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  760 | ` */` |
|   1950946 |  761 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  762 | `{` |
|   1950951 |  763 | `	if( pVm->pResumeFrame == 0 ){` |
|        23 |  764 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  765 | `	}` |
|   1950931 |  766 | `	if( pEntryFrame == 0 ){` |
|         - |  767 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - |  768 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - |  769 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - |  770 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - |  771 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - |  772 | `		 * NULL and the process dies. */` |
|        12 |  773 | `		return FALSE;` |
|         - |  774 | `	}` |
|         - |  775 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  776 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  777 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  778 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  779 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  780 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  781 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  782 | `	 * exec's resume site match and land. */` |
|   1950916 |  783 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1649826 |  784 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1348668 |  785 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  786 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  787 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  788 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  789 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  790 | `		 * never match a real frame. */` |
|    602331 |  791 | `		return FALSE;` |
|         - |  792 | `	}` |
|         - |  793 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  794 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  795 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  796 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  797 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  798 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  799 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  800 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  801 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  802 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  803 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  804 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  805 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  806 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  807 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  808 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  809 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  810 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  811 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  812 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  813 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|   2173929 |  814 | `	while( pVm->pFrame != pEntryFrame` |
|   2375048 |  815 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1549703 |  816 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|    302093 |  817 | `		VmLeaveFrame(&(*pVm));` |
|         5 |  818 | `	}` |
|   1348595 |  819 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|   1348595 |  820 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|         - |  821 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  822 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  823 | `	 * point router must not re-fire it after this resume. */` |
|   1348595 |  824 | `	pVm->nBoundaryRc = 0;` |
|   1348595 |  825 | `	return TRUE;` |
|    975478 |  826 | `}` |
|         - |  827 | `/*` |
|         - |  828 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  829 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  830 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  831 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  832 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  833 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  834 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  835 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  836 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  837 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  838 | ` */` |
|         - |  839 | `/*` |
|         - |  840 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  841 | ` *` |
|         - |  842 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  843 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  844 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  845 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  846 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  847 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  848 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  849 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  850 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  851 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  852 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  853 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  854 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  855 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  856 | ` * activation.` |
|         - |  857 | ` */` |
|   1450448 |  858 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  859 | `{` |
|   1450453 |  860 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1450453 |  861 | `	if( pClone == 0 ){` |
|       ! 0 |  862 | `		return 0;` |
|         - |  863 | `	}` |
|   1450453 |  864 | `	*pClone = *pCompiled;` |
|   1450453 |  865 | `	pClone->pCompiled = pCompiled;` |
|   1450453 |  866 | `	pClone->iFinallyDone = 0;` |
|   1450453 |  867 | `	pClone->iInCatch = 0;` |
|   1450453 |  868 | `	pClone->pInflight = 0;` |
|   1450453 |  869 | `	pClone->pFrame = 0;` |
|   1450453 |  870 | `	return pClone;` |
|    725229 |  871 | `}` |
|   2899768 |  872 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  873 | `{` |
|   2899773 |  874 | `	if( pExc && pExc->pCompiled ){` |
|         - |  875 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  876 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - |  877 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - |  878 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1450441 |  879 | `		if( pExc->pInflight ){` |
|       ! 0 |  880 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 |  881 | `			pExc->pInflight = 0;` |
|       ! 0 |  882 | `		}` |
|   1450441 |  883 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    725218 |  884 | `	}` |
|   2899773 |  885 | `}` |
|         - |  886 | `/*` |
|         - |  887 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - |  888 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - |  889 | ` */` |
|      1578 |  890 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 |  891 | `{` |
|      1583 |  892 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 |  893 | `}` |
|         - |  894 | `/*` |
|         - |  895 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - |  896 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - |  897 | ` * handlers). The set itself is reset by the caller.` |
|         - |  898 | ` */` |
|    128516 |  899 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 |  900 | `{` |
|    128521 |  901 | `	sxu32 n = SySetUsed(pSet);` |
|    128521 |  902 | `	if( n > 0 ){` |
|       ! 0 |  903 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - |  904 | `		sxu32 i;` |
|       ! 0 |  905 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 |  906 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 |  907 | `		}` |
|       ! 0 |  908 | `	}` |
|    128521 |  909 | `}` |
|         - |  910 | `/*` |
|         - |  911 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - |  912 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - |  913 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - |  914 | ` * down a try belonging to the caller.` |
|         - |  915 | ` */` |
|        18 |  916 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         4 |  917 | `{` |
|        22 |  918 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        22 |  919 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        22 |  920 | `	if( nBase < nFloor ){` |
|       ! 0 |  921 | `		nBase = nFloor;` |
|       ! 0 |  922 | `	}` |
|        22 |  923 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         4 |  924 | `}` |
|         - |  925 | `/*` |
|         - |  926 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - |  927 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - |  928 | ` * only carries the compiled pointer.` |
|         - |  929 | ` */` |
|        76 |  930 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  931 | `{` |
|        81 |  932 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        81 |  933 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        81 |  934 | `	while( n > 0 ){` |
|        81 |  935 | `		n--;` |
|        81 |  936 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        81 |  937 | `			return ap[n];` |
|         - |  938 | `		}` |
|       ! 0 |  939 | `	}` |
|       ! 0 |  940 | `	return 0;` |
|        43 |  941 | `}` |
|  10567388 |  942 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 |  943 | `{` |
|         - |  944 | `	sxu32 nUsed;` |
|  10567393 |  945 | `	sxi32 rcOut = SXRET_OK;` |
|  10567479 |  946 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|        91 |  947 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        91 |  948 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|        91 |  949 | `		(void)SySetPop(&pVm->aException);` |
|        91 |  950 | `		pExc->pFrame = 0;` |
|         - |  951 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - |  952 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - |  953 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - |  954 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - |  955 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|        91 |  956 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        91 |  957 | `			VmLeaveFrame(&(*pVm));` |
|        43 |  958 | `		}` |
|       123 |  959 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - |  960 | `			sxi32 rcF;` |
|        69 |  961 | `			pExc->iFinallyDone = 1;` |
|        69 |  962 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        69 |  963 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        69 |  964 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 |  965 | `				return SXERR_ABORT;` |
|         - |  966 | `			}` |
|        69 |  967 | `			if( rcF == PH7_EXCEPTION ){` |
|         - |  968 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - |  969 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - |  970 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 |  971 | `				rcOut = PH7_EXCEPTION;` |
|         2 |  972 | `			}` |
|        37 |  973 | `		}else{` |
|        27 |  974 | `			VmExcRelease(&(*pVm),pExc);` |
|         - |  975 | `		}` |
|         5 |  976 | `	}` |
|  10567393 |  977 | `	return rcOut;` |
|   5283897 |  978 | `}` |
|         - |  979 | `/*` |
|         - |  980 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - |  981 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - |  982 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - |  983 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - |  984 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - |  985 | ` */` |
|   8543624 |  986 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 |  987 | `{` |
|   8543629 |  988 | `	pFrame->bHasRet = 0;` |
|   8543629 |  989 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   8543629 |  990 | `	pFrame->nCatchJmpPc = 0;` |
|   8543629 |  991 | `}` |
|         - |  992 | `/*` |
|         - |  993 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - |  994 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - |  995 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - |  996 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - |  997 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - |  998 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - |  999 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - | 1000 | ` */` |
|     20194 | 1001 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 | 1002 | `{` |
|     20199 | 1003 | `	if( pResult ){` |
|     20199 | 1004 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     10097 | 1005 | `	}` |
|     20199 | 1006 | `	VmClearFramePending(pEntryFrame);` |
|     20199 | 1007 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|       ! 0 | 1008 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 1009 | `	}` |
|     20199 | 1010 | `}` |
|         - | 1011 | `/*` |
|         - | 1012 | ` * Compare two functions signature and return the comparison result.` |
|         - | 1013 | ` */` |
|      1186 | 1014 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         2 | 1015 | `{` |
|      1188 | 1016 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      1188 | 1017 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      1188 | 1018 | `	const char *zSin = pSecond->zString;` |
|      1188 | 1019 | `	const char *zFin = pFirst->zString;` |
|      1188 | 1020 | `	const char *zPtr = zFin;` |
|       593 | 1021 | `	for(;;){` |
|      1188 | 1022 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|       595 | 1023 | `			break;` |
|         - | 1024 | `		}` |
|       ! 0 | 1025 | `		if( zFin[0] != zSin[0] ){` |
|         - | 1026 | `			/* mismatch */` |
|       ! 0 | 1027 | `			break;` |
|         - | 1028 | `		}` |
|       ! 0 | 1029 | `		zFin++;` |
|       ! 0 | 1030 | `		zSin++;` |
|       ! 0 | 1031 | `	}` |
|      1188 | 1032 | `	return (int)(zFin-zPtr);` |
|         2 | 1033 | `}` |
|         - | 1034 | `/*` |
|         - | 1035 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1036 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1037 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1038 | ` * Refer to the official documentation for more information.` |
|         - | 1039 | ` */` |
|       264 | 1040 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1041 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1042 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1043 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1044 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1045 | `	)` |
|         4 | 1046 | `{` |
|         - | 1047 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1048 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1049 | `	ph7_vm_func *pLink;` |
|         - | 1050 | `	SyString sArgSig;` |
|         - | 1051 | `	SyBlob sSig;` |
|         - | 1052 |  |
|       268 | 1053 | `	pLink = pList;` |
|       268 | 1054 | `	i = 0;` |
|         - | 1055 | `	/* Put functions expecting the same number of passed arguments */` |
|      1670 | 1056 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      1622 | 1057 | `		if( pLink == 0 ){` |
|       219 | 1058 | `			break;` |
|         - | 1059 | `		}` |
|      1406 | 1060 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1061 | `			/* Candidate for overloading */` |
|      1406 | 1062 | `			apSet[i++] = pLink;` |
|       701 | 1063 | `		}` |
|         - | 1064 | `		/* Point to the next entry */` |
|      1406 | 1065 | `		pLink = pLink->pNextName;` |
|         4 | 1066 | `	}` |
|       268 | 1067 | `	if( i < 1 ){` |
|         - | 1068 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1069 | `		return pList;` |
|         - | 1070 | `	}` |
|       268 | 1071 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1072 | `		/* Return the only candidate */` |
|        40 | 1073 | `		return apSet[0];` |
|         - | 1074 | `	}` |
|         - | 1075 | `	/* Calculate function signature */` |
|       230 | 1076 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|       458 | 1077 | `	for( j = 0 ; j < nArg ; j++ ){` |
|       230 | 1078 | `		int c = 'n'; /* null */` |
|       230 | 1079 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1080 | `			/* Hashmap */` |
|       ! 0 | 1081 | `			c = 'h';` |
|       230 | 1082 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1083 | `			/* bool */` |
|        85 | 1084 | `			c = 'b';` |
|       188 | 1085 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1086 | `			/* int */` |
|        48 | 1087 | `			c = 'i';` |
|       122 | 1088 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1089 | `			/* String */` |
|        87 | 1090 | `			c = 's';` |
|        56 | 1091 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1092 | `			/* Float */` |
|        11 | 1093 | `			c = 'f';` |
|         8 | 1094 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1095 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1096 | `			int marker = 'o';` |
|       ! 0 | 1097 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1098 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1099 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1100 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1101 | `			c = -1;` |
|       ! 0 | 1102 | `		}` |
|       230 | 1103 | `		if( c > 0 ){` |
|       230 | 1104 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       114 | 1105 | `		}` |
|       116 | 1106 | `	}` |
|       230 | 1107 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|       230 | 1108 | `	iTarget = 0;` |
|       230 | 1109 | `	iMax = -1;` |
|         - | 1110 | `	/* Select the appropriate function */` |
|      1416 | 1111 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1112 | `		/* Compare the two signatures */` |
|      1188 | 1113 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      1188 | 1114 | `		if( iCur > iMax ){` |
|       230 | 1115 | `			iMax = iCur;` |
|       230 | 1116 | `			iTarget = j;` |
|       114 | 1117 | `		}` |
|       595 | 1118 | `	}` |
|       230 | 1119 | `	SyBlobRelease(&sSig);` |
|         - | 1120 | `	/* Appropriate function for the current call context */` |
|       230 | 1121 | `	return apSet[iTarget];` |
|       136 | 1122 | `}` |
|         - | 1123 | `/* Forward declaration */` |
|         - | 1124 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1125 | `/*` |
|         - | 1126 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|         - | 1127 | ` * safely across a pool reallocation.` |
|         - | 1128 | ` *` |
|         - | 1129 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1130 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|         - | 1131 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|         - | 1132 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|         - | 1133 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|         - | 1134 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|         - | 1135 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|         - | 1136 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|         - | 1137 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|         - | 1138 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|         - | 1139 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|         - | 1140 | ` */` |
|   2934226 | 1141 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1142 | `{` |
|         - | 1143 | `	ph7_value sVal;` |
|   2934231 | 1144 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1145 | `	sxi32 rc;` |
|   2934231 | 1146 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   2934231 | 1147 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1148 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|   2934231 | 1149 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   2934231 | 1150 | `	if( *ppMemObj ){` |
|   2934231 | 1151 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|   1467113 | 1152 | `	}` |
|   2934231 | 1153 | `	PH7_MemObjRelease(&sVal);` |
|   2934231 | 1154 | `	return rc;` |
|         5 | 1155 | `}` |
|         - | 1156 | `/*` |
|         - | 1157 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - | 1158 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - | 1159 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - | 1160 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - | 1161 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - | 1162 | ` *` |
|         - | 1163 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - | 1164 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - | 1165 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - | 1166 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - | 1167 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - | 1168 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - | 1169 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - | 1170 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - | 1171 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - | 1172 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 1173 | ` */` |
|     27920 | 1174 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 | 1175 | `{` |
|     27925 | 1176 | `	ph7_exception **apSaved = 0;             /* try activations hidden for the eval */` |
|     27925 | 1177 | `	sxu32 nSaved = SySetUsed(&pVm->aException);` |
|     27925 | 1178 | `	sxi32 iSaveStatus = pVm->iExitStatus;` |
|     27925 | 1179 | `	sxi32 iSaveBoundary = pVm->nBoundaryRc;` |
|     27925 | 1180 | `	VmFrame *pSaveResume = pVm->pResumeFrame;` |
|     27925 | 1181 | `	ph7_class_attr *pSaveCycleAttr = pVm->pConstCycleAttr;` |
|     27925 | 1182 | `	ph7_class *pSaveCycleClass = pVm->pConstCycleClass;` |
|         - | 1183 | `	VmFrame *pFrame;` |
|         - | 1184 | `	sxi32 rc;` |
|     27925 | 1185 | `	if( nSaved > 0 ){` |
|         3 | 1186 | `		apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,nSaved * sizeof(ph7_exception *));` |
|         3 | 1187 | `		if( apSaved ){` |
|         3 | 1188 | `			SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,nSaved * sizeof(ph7_exception *));` |
|         3 | 1189 | `			SySetReset(&pVm->aException);` |
|         1 | 1190 | `		}` |
|         1 | 1191 | `	}` |
|     27925 | 1192 | `	pVm->nMuteThrow++;` |
|     27925 | 1193 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|     27925 | 1194 | `	pVm->nMuteThrow--;` |
|     27925 | 1195 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != pSaveCycleAttr ){` |
|         - | 1196 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - | 1197 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - | 1198 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - | 1199 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - | 1200 | `		 * records the cycle again and raises it there. */` |
|       ! 0 | 1201 | `		rc = PH7_EXCEPTION;` |
|       ! 0 | 1202 | `	}` |
|         - | 1203 | `	/* Nothing may push onto the hidden stack (an initializer has no try of its` |
|         - | 1204 | `	 * own), but a muted throw unwinding out of one would leave an activation` |
|         - | 1205 | `	 * behind: release whatever is there whether or not anything was hidden. */` |
|     27925 | 1206 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|     27925 | 1207 | `	SySetReset(&pVm->aException);` |
|     27925 | 1208 | `	if( apSaved ){` |
|         - | 1209 | `		sxu32 k;` |
|         5 | 1210 | `		for( k = 0 ; k < nSaved ; ++k ){` |
|         3 | 1211 | `			SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|         2 | 1212 | `		}` |
|         3 | 1213 | `		SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|         1 | 1214 | `	}` |
|     27925 | 1215 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 1216 | `		/* Roll the attempt back whole: the throw stamped the frame (which the` |
|         - | 1217 | `		 * enclosing execution would read as an unwind in progress), the uncaught` |
|         - | 1218 | `		 * exit status, and possibly a C-boundary park / resume target. */` |
|        47 | 1219 | `		pFrame = pVm->pFrame;` |
|        47 | 1220 | `		if( pFrame ){` |
|        47 | 1221 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|        47 | 1222 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        22 | 1223 | `		}` |
|        47 | 1224 | `		pVm->iExitStatus = iSaveStatus;` |
|        47 | 1225 | `		pVm->nBoundaryRc = iSaveBoundary;` |
|        47 | 1226 | `		pVm->pResumeFrame = pSaveResume;` |
|         - | 1227 | `		/* A self-referencing constant reached by the abandoned initializer only` |
|         - | 1228 | `		 * RECORDS itself here (VmClassConstEvalOnDemand) for an outer level to` |
|         - | 1229 | `		 * raise. Left standing it would be raised, unmuted, by the next attribute` |
|         - | 1230 | `		 * whose default happens to succeed — at the declaration site, and blamed` |
|         - | 1231 | `		 * on the wrong member. The deferred re-run detects the cycle again. */` |
|        47 | 1232 | `		pVm->pConstCycleAttr = pSaveCycleAttr;` |
|        47 | 1233 | `		pVm->pConstCycleClass = pSaveCycleClass;` |
|        22 | 1234 | `	}` |
|     27925 | 1235 | `	return rc;` |
|         5 | 1236 | `}` |
|         - | 1237 | `/*` |
|         - | 1238 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1239 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1240 | ` */` |
|         - | 1241 | `/*` |
|         - | 1242 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1243 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1244 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1245 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1246 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1247 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1248 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1249 | ` */` |
|    824698 | 1250 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1251 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1252 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1253 | `	)` |
|         5 | 1254 | `{` |
|         - | 1255 | `	ph7_class_attr *pAttr;` |
|         - | 1256 | `	SyHashEntry *pEntry;` |
|         - | 1257 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1258 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1259 | `	 * over both tables. */` |
|         - | 1260 | `	SyHash *apMount[2];` |
|         - | 1261 | `	int iMount;` |
|    824703 | 1262 | `	apMount[0] = &pClass->hAttr;` |
|    824703 | 1263 | `	apMount[1] = &pClass->hConst;` |
|   2474093 | 1264 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1265 | `	/* Reset the loop cursor */` |
|   1649401 | 1266 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1267 | `	/* Process only static and constant attribute */` |
|   4194747 | 1268 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1269 | `		/* Extract the current attribute */` |
|   2545357 | 1270 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   2545352 | 1271 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   1664549 | 1272 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    392320 | 1273 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1274 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1275 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1276 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1277 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1278 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1279 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1280 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1281 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1282 | `			 * %s as value for class constant" fatal without any access). */` |
|    782837 | 1283 | `			continue;` |
|         - | 1284 | `		}` |
|   1762525 | 1285 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1286 | `			ph7_value *pMemObj;` |
|     28815 | 1287 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1288 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1289 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1290 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1291 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - | 1292 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - | 1293 | `				 * access / instantiation throws like php's. */` |
|       879 | 1294 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        17 | 1295 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - | 1296 | `						/* Its default threw at the other class's mount and is` |
|         - | 1297 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - | 1298 | `						 * both static tables). */` |
|         3 | 1299 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        16 | 1300 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         4 | 1301 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|         2 | 1302 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|         3 | 1303 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1304 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|         1 | 1305 | `						}` |
|         1 | 1306 | `					}` |
|         7 | 1307 | `				}` |
|       882 | 1308 | `				continue;` |
|         - | 1309 | `			}` |
|         - | 1310 | `			/* Reserve a memory object for this constant/static attribute */` |
|     27939 | 1311 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     27939 | 1312 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1313 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 1314 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 1315 | `					&pClass->sName,&pAttr->sName` |
|         - | 1316 | `					);` |
|       ! 0 | 1317 | `				return SXERR_MEM;` |
|         - | 1318 | `			}` |
|     27939 | 1319 | `			if( pAttr->pNativeValue ){` |
|         - | 1320 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 | 1321 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|     27939 | 1322 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1323 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1324 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 1325 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|     27925 | 1326 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|     27925 | 1327 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - | 1328 | `				sxi32 rcExec;` |
|     27925 | 1329 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|     27925 | 1330 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|     27925 | 1331 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|     27925 | 1332 | `				pVm->nConstEvalDepth++;` |
|         - | 1333 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - | 1334 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - | 1335 | `				 * not something it can see. What reaches this line is a static` |
|         - | 1336 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - | 1337 | `				 * which php does validate here, but only when the initializer` |
|         - | 1338 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - | 1339 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - | 1340 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|     27925 | 1341 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|     27925 | 1342 | `				pVm->nConstEvalDepth--;` |
|     27925 | 1343 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|     27925 | 1344 | `				pVm->pConstEvalClass = pSaveCtx;` |
|     27925 | 1345 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1346 | `					/* php has not reached this initializer: defer it whole to the` |
|         - | 1347 | `					 * first USE, where the throw is raised at the access site and is` |
|         - | 1348 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - | 1349 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - | 1350 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - | 1351 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - | 1352 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - | 1353 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - | 1354 | `					 * unset nIdx. */` |
|        47 | 1355 | `					if( bStaticProp ){` |
|        41 | 1356 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        41 | 1357 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1358 | `					}else{` |
|         - | 1359 | `						VmSlot sSlot;` |
|         - | 1360 | `						/* Release before recycling: PH7_ReserveMemObj re-inits a` |
|         - | 1361 | `						 * reused slot without releasing it, and a muted eval that` |
|         - | 1362 | `						 * only recorded a CYCLE still left its value here. */` |
|         8 | 1363 | `						sSlot.nIdx = pMemObj->nIdx;` |
|         8 | 1364 | `						sSlot.pUserData = 0;` |
|         8 | 1365 | `						PH7_MemObjRelease(pMemObj);` |
|         8 | 1366 | `						SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|         8 | 1367 | `						continue;` |
|         - | 1368 | `					}` |
|     27898 | 1369 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|     13943 | 1370 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1371 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 1372 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - | 1373 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        39 | 1374 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        39 | 1375 | `					if( rcType != SXRET_OK ){` |
|         8 | 1376 | `						return rcType;` |
|         - | 1377 | `					}` |
|        15 | 1378 | `				}` |
|     13954 | 1379 | `			}` |
|         - | 1380 | `			/* Record attribute index */` |
|     27927 | 1381 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 1382 | `			/* Install static attribute in the reference table */` |
|     27927 | 1383 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1384 | `			/* If this is a typed static property, register the slot so the` |
|         - | 1385 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 1386 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 1387 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 1388 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 1389 | `			 * already enforced above, so they need no store-time slot. */` |
|     27922 | 1390 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     14008 | 1391 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        59 | 1392 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        59 | 1393 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 1394 | `					return SXERR_MEM;` |
|         - | 1395 | `				}` |
|        59 | 1396 | `				pVmAttrS->pAttr = pAttr;` |
|        59 | 1397 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        59 | 1398 | `				pVmAttrS->iState = 0;` |
|        59 | 1399 | `				pVmAttrS->pOwner = pClass;` |
|         - | 1400 | `				/* Static typed property with no default starts uninitialized` |
|         - | 1401 | `				 * (constants are already excluded by the enclosing condition). */` |
|        59 | 1402 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        14 | 1403 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        53 | 1404 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - | 1405 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 1406 | `					 * typed static default LAZILY at the first static-property` |
|         - | 1407 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 1408 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 1409 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 1410 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 1411 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 1412 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - | 1413 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - | 1414 | `					 * the materializer checks it after the re-run. */` |
|        48 | 1415 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        24 | 1416 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        24 | 1417 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        11 | 1418 | `					}` |
|        22 | 1419 | `				}` |
|        59 | 1420 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 1421 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 1422 | `					return SXERR_MEM;` |
|         - | 1423 | `				}` |
|        27 | 1424 | `			}` |
|     13961 | 1425 | `		}` |
|         5 | 1426 | `	}` |
|    824700 | 1427 | `	} /* for iMount */` |
|    824697 | 1428 | `	return SXRET_OK;` |
|    412354 | 1429 | `}` |
|         - | 1430 | `/*` |
|         - | 1431 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - | 1432 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - | 1433 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - | 1434 | ` */` |
|    824150 | 1435 | `static sxi32 VmMountUserClassMethods(` |
|         - | 1436 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1437 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - | 1438 | `	)` |
|         5 | 1439 | `{` |
|         - | 1440 | `	ph7_class_method *pMeth;` |
|         - | 1441 | `	SyHashEntry *pEntry;` |
|         - | 1442 | `	sxi32 rc;` |
|         - | 1443 | `	/* Install class methods */` |
|    824155 | 1444 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 1445 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 1446 | `		 */` |
|    353675 | 1447 | `		return SXRET_OK;` |
|         - | 1448 | `	}` |
|         - | 1449 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 1450 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 1451 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 1452 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 1453 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 1454 | `	/* Install the methods now */` |
|    470485 | 1455 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   7006371 | 1456 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   6300651 | 1457 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   6300651 | 1458 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   6288743 | 1459 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   6288743 | 1460 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1461 | `				return rc;` |
|         - | 1462 | `			}` |
|   3144369 | 1463 | `		}` |
|         5 | 1464 | `	}` |
|         - | 1465 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    470485 | 1466 | `	pClass->bMounted = TRUE;` |
|    470485 | 1467 | `	return SXRET_OK;` |
|    412080 | 1468 | `}` |
|    284946 | 1469 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 1470 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1471 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 1472 | `	)` |
|         5 | 1473 | `{` |
|         - | 1474 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - | 1475 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - | 1476 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - | 1477 | `	 * initializer that throws finds the exception classes ready. */` |
|    284951 | 1478 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|    284951 | 1479 | `	if( rc != SXRET_OK ){` |
|         3 | 1480 | `		return rc;` |
|         - | 1481 | `	}` |
|    284949 | 1482 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|    142478 | 1483 | `}` |
|         - | 1484 | `/*` |
|         - | 1485 | ` * Allocate a private frame for attributes of the given` |
|         - | 1486 | ` * class instance (Object in the PHP jargon).` |
|         - | 1487 | ` */` |
|   1560104 | 1488 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 1489 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 1490 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 1491 | `	)` |
|         5 | 1492 | `{` |
|   1560109 | 1493 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 1494 | `	ph7_class_attr *pAttr;` |
|         - | 1495 | `	SyHashEntry *pEntry;` |
|         - | 1496 | `	sxi32 rc;` |
|   1560109 | 1497 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 1498 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 1499 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 1500 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1560109 | 1501 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  10279361 | 1502 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 1503 | `		VmClassAttr *pVmAttr;` |
|         - | 1504 | `		/* Extract the current attribute */` |
|   8719257 | 1505 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   8719257 | 1506 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|   8719257 | 1507 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 1508 | `			return SXERR_MEM;` |
|         - | 1509 | `		}` |
|   8719257 | 1510 | `		pVmAttr->pAttr = pAttr;` |
|   8719257 | 1511 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 1512 | `			ph7_value *pMemObj;` |
|         - | 1513 | `			/* Reserve a memory object for this attribute */` |
|   8718965 | 1514 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|   8718965 | 1515 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1516 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1517 | `				return SXERR_MEM;` |
|         - | 1518 | `			}` |
|   8718965 | 1519 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|   8718965 | 1520 | `			pVmAttr->iState = 0;` |
|   8718965 | 1521 | `			pVmAttr->pOwner = pClass;` |
|   8718965 | 1522 | `			if( pAttr->pNativeValue ){` |
|         - | 1523 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - | 1524 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - | 1525 | `				 * literal cannot throw and the builder states the type itself. */` |
|       903 | 1526 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   8718516 | 1527 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1528 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1529 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 1530 | `				 * against the declaring class (no method frame here). */` |
|   2905825 | 1531 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1532 | `				sxi32 rcExec;` |
|   2905825 | 1533 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   2905825 | 1534 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|   2905825 | 1535 | `				pVm->pConstEvalClass = pSaveCtx;` |
|   2905825 | 1536 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1537 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 1538 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 1539 | `					 * also type-check the leftover value (a spurious second` |
|         - | 1540 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 1541 | `					 * nothing further for the remaining attributes.` |
|         - | 1542 | `					 *` |
|         - | 1543 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - | 1544 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - | 1545 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - | 1546 | `					 * and the throw came back as a status nobody read: this` |
|         - | 1547 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - | 1548 | `					 * and the whole statement RESUMED after the catch — php` |
|         - | 1549 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - | 1550 | `					 * existing construction-aborted route. */` |
|        23 | 1551 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        23 | 1552 | `					bDefThrew = 1;` |
|   2905814 | 1553 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1554 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 1555 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 1556 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 1557 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 1558 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 1559 | `					 * aborts construction) / the fetch-point router. */` |
|       269 | 1560 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       269 | 1561 | `					if( rcDef != SXRET_OK ){` |
|        13 | 1562 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 1563 | `						bDefThrew = 1;` |
|         6 | 1564 | `					}` |
|       137 | 1565 | `				}` |
|   7265157 | 1566 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 1567 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 1568 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       529 | 1569 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       262 | 1570 | `			}` |
|   8718965 | 1571 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|   8718965 | 1572 | `			if( rc != SXRET_OK ){` |
|         - | 1573 | `				VmSlot sSlot;` |
|         - | 1574 | `				/* Restore memory object */` |
|       ! 0 | 1575 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1576 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1577 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1578 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1579 | `				return SXERR_MEM;` |
|         - | 1580 | `			}` |
|         - | 1581 | `			/* Install attribute in the reference table */` |
|   8718965 | 1582 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1583 | `			/* Register typed property slot for assignment-time enforcement.` |
|         - | 1584 | `			 * On failure roll back the just-installed hAttr entry and the` |
|         - | 1585 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|   8718965 | 1586 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       797 | 1587 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|       797 | 1588 | `				if( rc != SXRET_OK ){` |
|         - | 1589 | `					VmSlot sSlot;` |
|       ! 0 | 1590 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1591 | `					sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1592 | `					sSlot.pUserData = 0;` |
|       ! 0 | 1593 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1594 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1595 | `					return SXERR_MEM;` |
|         - | 1596 | `				}` |
|       396 | 1597 | `			}` |
|   4359485 | 1598 | `		}else{` |
|         - | 1599 | `			/* Install static/constant attribute */` |
|       296 | 1600 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       296 | 1601 | `			pVmAttr->iState = 0;` |
|       296 | 1602 | `			pVmAttr->pOwner = pClass;` |
|       296 | 1603 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       296 | 1604 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1605 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1606 | `				return SXERR_MEM;` |
|         - | 1607 | `			}` |
|         - | 1608 | `		}` |
|         5 | 1609 | `	}` |
|   1560109 | 1610 | `	return SXRET_OK;` |
|    780057 | 1611 | `}` |
|         - | 1612 | `/*` |
|         - | 1613 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|         - | 1614 | ` * stdClass for now; the future general-dynamic-props work turns` |
|         - | 1615 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|         - | 1616 | ` */` |
|        90 | 1617 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1618 | `{` |
|        95 | 1619 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|         5 | 1620 | `}` |
|         - | 1621 | `/*` |
|         - | 1622 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 1623 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 1624 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 1625 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 1626 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 1627 | ` */` |
|         6 | 1628 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         2 | 1629 | `{` |
|        14 | 1630 | `	while( pClass ){` |
|         8 | 1631 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 1632 | `		sxu32 n;` |
|         8 | 1633 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|       ! 0 | 1634 | `			if( aAttr[n].sName.nByte == nName` |
|       ! 0 | 1635 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|       ! 0 | 1636 | `				return TRUE;` |
|         - | 1637 | `			}` |
|       ! 0 | 1638 | `		}` |
|         8 | 1639 | `		pClass = pClass->pBase;` |
|         2 | 1640 | `	}` |
|         8 | 1641 | `	return FALSE;` |
|         5 | 1642 | `}` |
|         - | 1643 | `/*` |
|         - | 1644 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 1645 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 1646 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 1647 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 1648 | ` *` |
|         - | 1649 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 1650 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 1651 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 1652 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 1653 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 1654 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 1655 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 1656 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 1657 | ` */` |
|       180 | 1658 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         5 | 1659 | `{` |
|         - | 1660 | `	ph7_class_attr *pAttr;` |
|       185 | 1661 | `	VmClassAttr *pVmAttr = 0;` |
|       185 | 1662 | `	ph7_value *pMemObj = 0;` |
|         - | 1663 | `	char *zCopy;` |
|         - | 1664 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       185 | 1665 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       185 | 1666 | `	if( pAttr == 0 ){` |
|       ! 0 | 1667 | `		return 0;` |
|         - | 1668 | `	}` |
|       185 | 1669 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       185 | 1670 | `	zCopy = (char *)&pAttr[1];` |
|       185 | 1671 | `	if( nName > 0 ){` |
|       185 | 1672 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|        90 | 1673 | `	}` |
|       185 | 1674 | `	zCopy[nName] = 0;` |
|       185 | 1675 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       185 | 1676 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       185 | 1677 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       185 | 1678 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 1679 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 1680 | `	 * value, never a union. */` |
|       185 | 1681 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       185 | 1682 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1683 | `		goto fail_attr;` |
|         - | 1684 | `	}` |
|       185 | 1685 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       185 | 1686 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1687 | `		goto fail_vmattr;` |
|         - | 1688 | `	}` |
|       185 | 1689 | `	pVmAttr->pAttr = pAttr;` |
|       185 | 1690 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       185 | 1691 | `	pVmAttr->iState = 0;` |
|       185 | 1692 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1693 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 1694 | `	 * property-creation order, matching PHP. */` |
|       185 | 1695 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 1696 | `		goto fail_slot;` |
|         - | 1697 | `	}` |
|         - | 1698 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       185 | 1699 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|       185 | 1700 | `	if( ppAttr ){` |
|       101 | 1701 | `		*ppAttr = pVmAttr;` |
|        48 | 1702 | `	}` |
|       185 | 1703 | `	return pMemObj;` |
|       ! 0 | 1704 | `fail_slot:` |
|         - | 1705 | `	{` |
|         - | 1706 | `		VmSlot sSlot;` |
|       ! 0 | 1707 | `		sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1708 | `		sSlot.pUserData = 0;` |
|       ! 0 | 1709 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1710 | `	}` |
|       ! 0 | 1711 | `fail_vmattr:` |
|       ! 0 | 1712 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1713 | `fail_attr:` |
|       ! 0 | 1714 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 1715 | `	return 0;` |
|        95 | 1716 | `}` |
|         - | 1717 | `/*` |
|         - | 1718 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 1719 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 1720 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 1721 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 1722 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 1723 | ` */` |
|         6 | 1724 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         1 | 1725 | `{` |
|         - | 1726 | `	VmClassAttr *pVmAttr;` |
|         - | 1727 | `	ph7_value *pMemObj;` |
|         7 | 1728 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|         7 | 1729 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1730 | `		return;` |
|         - | 1731 | `	}` |
|         7 | 1732 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|         7 | 1733 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1734 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1735 | `		return;` |
|         - | 1736 | `	}` |
|         7 | 1737 | `	pVmAttr->pAttr = pAttr;` |
|         7 | 1738 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|         7 | 1739 | `	pVmAttr->iState = 0;` |
|         7 | 1740 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1741 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 1742 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 1743 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 1744 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 1745 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|         7 | 1746 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       ! 0 | 1747 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       ! 0 | 1748 | `	}` |
|         - | 1749 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 1750 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 1751 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 1752 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|         7 | 1753 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|         - | 1754 | `		VmSlot sSlot;` |
|       ! 0 | 1755 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1756 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1757 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1758 | `		return;` |
|         - | 1759 | `	}` |
|         7 | 1760 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         7 | 1761 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       ! 0 | 1762 | `		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|         - | 1763 | `			VmSlot sSlot;` |
|       ! 0 | 1764 | `			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1765 | `			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1766 | `			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1767 | `			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1768 | `			return;` |
|         - | 1769 | `		}` |
|       ! 0 | 1770 | `	}` |
|         7 | 1771 | `	if( ppAttr ){` |
|         7 | 1772 | `		*ppAttr = pVmAttr;` |
|         3 | 1773 | `	}` |
|         4 | 1774 | `}` |
|         - | 1775 | `/* Forward declaration */` |
|         - | 1776 | `/*` |
|         - | 1777 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 1778 | ` */` |
|         - | 1779 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 1780 | `/*` |
|         - | 1781 | ` * Reserve a constant memory object.` |
|         - | 1782 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 1783 | ` */` |
|  10007000 | 1784 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 1785 | `{` |
|         - | 1786 | `	ph7_value *pObj;` |
|         - | 1787 | `	sxi32 rc;` |
|  10007005 | 1788 | `	if( pIndex ){` |
|         - | 1789 | `		/* Object index in the object table */` |
|   9993421 | 1790 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   4996708 | 1791 | `	}` |
|         - | 1792 | `	/* Reserve a slot for the new object */` |
|  10007005 | 1793 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|  10007005 | 1794 | `	if( rc != SXRET_OK ){` |
|         - | 1795 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1796 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1797 | `		 */` |
|       ! 0 | 1798 | `		return 0;` |
|         - | 1799 | `	}` |
|  10007005 | 1800 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|  10007005 | 1801 | `	return pObj;` |
|   5003505 | 1802 | `}` |
|         - | 1803 | `/*` |
|         - | 1804 | ` * Reserve a memory object.` |
|         - | 1805 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 1806 | ` */` |
|   2868964 | 1807 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 1808 | `{` |
|         - | 1809 | `	ph7_value *pObj;` |
|         - | 1810 | `	sxi32 rc;` |
|   2868969 | 1811 | `	if( pIndex ){` |
|         - | 1812 | `		/* Object index in the object table */` |
|   2868969 | 1813 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|   1434482 | 1814 | `	}` |
|         - | 1815 | `	/* Reserve a slot for the new object */` |
|   2868969 | 1816 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|   2868969 | 1817 | `	if( rc != SXRET_OK ){` |
|         - | 1818 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1819 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1820 | `		 */` |
|       ! 0 | 1821 | `		return 0;` |
|         - | 1822 | `	}` |
|   2868969 | 1823 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|   2868969 | 1824 | `	return pObj;` |
|   1434487 | 1825 | `}` |
|         - | 1826 | `/* Forward declaration */` |
|         - | 1827 | `/* Forward declarations for Fiber C functions */` |
|         - | 1828 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 1829 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 1830 | `/*` |
|         - | 1831 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 1832 | ` * directly as foreign functions.` |
|         - | 1833 | ` */` |
|         - | 1834 |  |
|         - | 1835 | `/*` |
|         - | 1836 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 1837 | ` * start compiling the target PHP program.` |
|         - | 1838 | ` */` |
|      4528 | 1839 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 1840 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 1841 | `	 ph7 *pEngine /* Master engine */` |
|         - | 1842 | `	 )` |
|         5 | 1843 | `{` |
|         - | 1844 | `	ph7_value *pObj;` |
|         - | 1845 | `	sxi32 rc;` |
|         - | 1846 | `	/* Zero the structure */` |
|      4533 | 1847 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 1848 | `	/* Initialize VM fields */` |
|      4533 | 1849 | `	pVm->pEngine = &(*pEngine);` |
|      4533 | 1850 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|         - | 1851 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 1852 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 1853 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      4533 | 1854 | `	pVm->bDisplayErrors = 0;` |
|      4533 | 1855 | `	pVm->bLogErrors = 1;` |
|      4533 | 1856 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 1857 | `	/* Instructions containers */` |
|      4533 | 1858 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      4533 | 1859 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      4533 | 1860 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 1861 | `	/* Object containers */` |
|      4533 | 1862 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      4533 | 1863 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|         - | 1864 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      4533 | 1865 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      4533 | 1866 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      4533 | 1867 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      4533 | 1868 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 1869 | `	/* Virtual machine internal containers */` |
|      4533 | 1870 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      4533 | 1871 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      4533 | 1872 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      4533 | 1873 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|      4533 | 1874 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      4533 | 1875 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 1876 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 1877 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 1878 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 1879 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 1880 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 1881 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 1882 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 1883 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      4533 | 1884 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4533 | 1885 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4533 | 1886 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4533 | 1887 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      4533 | 1888 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      4533 | 1889 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      4533 | 1890 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|      4533 | 1891 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      4533 | 1892 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      4533 | 1893 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      4533 | 1894 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      4533 | 1895 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      4533 | 1896 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      4533 | 1897 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      4533 | 1898 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      4533 | 1899 | `	pVm->nResourceIdNext = 1;` |
|      4533 | 1900 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      4533 | 1901 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      4533 | 1902 | `	pVm->pMagicSetThis = 0;` |
|      4533 | 1903 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      4533 | 1904 | `	pVm->pHookSetThis = 0;` |
|      4533 | 1905 | `	pVm->pHookSetAttr = 0;` |
|      4533 | 1906 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      4533 | 1907 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      4533 | 1908 | `	pVm->pMagicCallThis = 0;` |
|      4533 | 1909 | `	pVm->pMagicCallClass = 0;` |
|      4533 | 1910 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      4533 | 1911 | `	pVm->pIdleCallFrames = 0;` |
|      4533 | 1912 | `	pVm->pIdleOperandStacks = 0;` |
|      4533 | 1913 | `	pVm->nIdleOperandStacks = 0;` |
|      4533 | 1914 | `	pVm->pIdleStackNodes = 0;` |
|      4533 | 1915 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      4533 | 1916 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      4533 | 1917 | `	pVm->pPendingException = 0;` |
|      4533 | 1918 | `	pVm->pInflightException = 0;` |
|      4533 | 1919 | `	pVm->nInflightExcBase = 0;` |
|      4533 | 1920 | `	pVm->pResumeFrame = 0;` |
|      4533 | 1921 | `	pVm->iResumePc = 0;` |
|      4533 | 1922 | `	pVm->pResumeInstr = 0;` |
|      4533 | 1923 | `	pVm->iResumeStackDepth = 0;` |
|      4533 | 1924 | `	pVm->nBoundaryRc = 0;` |
|      4533 | 1925 | `	pVm->pConstEvalClass = 0;` |
|      4533 | 1926 | `	pVm->nConstEvalDepth = 0;` |
|      4533 | 1927 | `	pVm->pConstCycleAttr = 0;` |
|      4533 | 1928 | `	pVm->pConstCycleClass = 0;` |
|      4533 | 1929 | `	SySetReset(&pVm->aMagicGuard);` |
|      4533 | 1930 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 1931 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 1932 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 1933 | `	}` |
|      4533 | 1934 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      4533 | 1935 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 1936 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 1937 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 1938 | `	}` |
|      4533 | 1939 | `	pVm->pHookSetAttr = 0;` |
|      4533 | 1940 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      4533 | 1941 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 1942 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 1943 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 1944 | `	}` |
|      4533 | 1945 | `	pVm->pMagicCallClass = 0;` |
|      4533 | 1946 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 1947 | `	/* Configuration containers */` |
|      4533 | 1948 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      4533 | 1949 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      4533 | 1950 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      4533 | 1951 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      4533 | 1952 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|      4533 | 1953 | `	pVm->iResponseStatus = 200;` |
|      4533 | 1954 | `	pVm->bHeadersSent = 0;` |
|      4533 | 1955 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 1956 | `	/* Error callbacks containers */` |
|      4533 | 1957 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|      4533 | 1958 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|      4533 | 1959 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|      4533 | 1960 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|      4533 | 1961 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 1962 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 1963 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 1964 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 1965 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 1966 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 1967 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 1968 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      4533 | 1969 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      4533 | 1970 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 1971 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 1972 | `#else` |
|         - | 1973 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 1974 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 1975 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 1976 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 1977 | `	pVm->nMaxDepth = 512;` |
|         - | 1978 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 1979 | `#endif` |
|         - | 1980 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 1981 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 1982 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      4533 | 1983 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 1984 | `	/* JSON return status */` |
|      4533 | 1985 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 1986 | `	/* PRNG context */` |
|      4533 | 1987 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 1988 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 1989 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      4533 | 1990 | `	pVm->mtSeeded = FALSE;` |
|         - | 1991 | `	/* Install the null constant */` |
|      4533 | 1992 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4533 | 1993 | `	if( pObj == 0 ){` |
|       ! 0 | 1994 | `		rc = SXERR_MEM;` |
|       ! 0 | 1995 | `		goto Err;` |
|         - | 1996 | `	}` |
|      4533 | 1997 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 1998 | `	/* Install the boolean TRUE constant */` |
|      4533 | 1999 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4533 | 2000 | `	if( pObj == 0 ){` |
|       ! 0 | 2001 | `		rc = SXERR_MEM;` |
|       ! 0 | 2002 | `		goto Err;` |
|         - | 2003 | `	}` |
|      4533 | 2004 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 2005 | `	/* Install the boolean FALSE constant */` |
|      4533 | 2006 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4533 | 2007 | `	if( pObj == 0 ){` |
|       ! 0 | 2008 | `		rc = SXERR_MEM;` |
|       ! 0 | 2009 | `		goto Err;` |
|         - | 2010 | `	}` |
|      4533 | 2011 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 2012 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 2013 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 2014 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      4533 | 2015 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      4533 | 2016 | `	if( pObj == 0 ){` |
|       ! 0 | 2017 | `		rc = SXERR_MEM;` |
|       ! 0 | 2018 | `		goto Err;` |
|         - | 2019 | `	}` |
|      4533 | 2020 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 2021 | `	/* Create the global frame */` |
|      4533 | 2022 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      4533 | 2023 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2024 | `		goto Err;` |
|         - | 2025 | `	}` |
|         - | 2026 | `	/* Initialize the code generator */` |
|      4533 | 2027 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      4533 | 2028 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2029 | `		goto Err;` |
|         - | 2030 | `	}` |
|         - | 2031 | `	/* VM correctly initialized,set the magic number */` |
|      4533 | 2032 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 2033 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 2034 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      4533 | 2035 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 2036 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      4533 | 2037 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 2038 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 2039 | `	 * compiled — its classes are internal too. */` |
|         - | 2040 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      4533 | 2041 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      4533 | 2042 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      4533 | 2043 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      4533 | 2044 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      4533 | 2045 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 2046 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      4533 | 2047 | `	pVm->pCoalesceObj = 0;` |
|      4533 | 2048 | `	pVm->bCoalesceArmed = 0;` |
|      4533 | 2049 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 2050 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - | 2051 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - | 2052 | ``	 * segfaults the first `new Fiber`. */`` |
|      4533 | 2053 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      4533 | 2054 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 2055 | `	/* Cache the Closure class pointer (closures are instances of it) */` |
|      4533 | 2056 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      4533 | 2057 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      4533 | 2058 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 2059 | `	/* Closure::bindTo/bind/fromCallable, as real C-bodied METHODS rather than the` |
|         - | 2060 | `	 * global __closure_* thunks a prelude method used to forward to. */` |
|      4533 | 2061 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|         - | 2062 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      4533 | 2063 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 2064 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - | 2065 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - | 2066 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - | 2067 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      4533 | 2068 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      4533 | 2069 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - | 2070 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 2071 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 2072 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      4533 | 2073 | `	PH7_VmInstallReflection(&(*pVm));` |
|      4533 | 2074 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      4533 | 2075 | `	PH7_VmInstallSpl(&(*pVm));` |
|      4533 | 2076 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|      4533 | 2077 | `	PH7_VmInstallSession(&(*pVm));` |
|      4533 | 2078 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 2079 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2080 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|         - | 2081 | `	 * XMLWriter class libraries that build on it. */` |
|      4533 | 2082 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      4533 | 2083 | `	PH7_VmInstallDom(&(*pVm));` |
|      4533 | 2084 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 2085 | `#endif` |
|      4533 | 2086 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 2087 | `	/* Reset the code generator */` |
|      4533 | 2088 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      4533 | 2089 | `	return SXRET_OK;` |
|       ! 0 | 2090 | `Err:` |
|       ! 0 | 2091 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 2092 | `	return rc;` |
|      2269 | 2093 | `}` |
|         - | 2094 | `/*` |
|         - | 2095 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 2096 | ` * routine which store the output in an internal blob.` |
|         - | 2097 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 2098 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 2099 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 2100 | ` * Refer to the official docurmentation for additional information.` |
|         - | 2101 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 2102 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 2103 | ` * to finish executing and extracting the output.` |
|         - | 2104 | ` */` |
|        68 | 2105 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 2106 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 2107 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 2108 | `	void *pUserData     /* User private data */` |
|         - | 2109 | `	)` |
|       ! 0 | 2110 | `{` |
|         - | 2111 | `	 sxi32 rc;` |
|         - | 2112 | `	 /* Store the output in an internal BLOB */` |
|        68 | 2113 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|        68 | 2114 | `	 return rc;` |
|       ! 0 | 2115 | `}` |
|         - | 2116 | `/*` |
|         - | 2117 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 2118 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 2119 | ` */` |
|     64558 | 2120 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 2121 | `{` |
|     64563 | 2122 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|     64563 | 2123 | `	if( xCons != VmObConsumer ){` |
|     23479 | 2124 | `		pVm->nOutputLen += nLen;` |
|     23479 | 2125 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      1145 | 2126 | `			pVm->bHeadersSent = 1;` |
|       570 | 2127 | `		}` |
|     11737 | 2128 | `	}` |
|     64563 | 2129 | `}` |
|         - | 2130 | `/*` |
|         - | 2131 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 2132 | ` *` |
|         - | 2133 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 2134 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 2135 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 2136 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 2137 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 2138 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 2139 | ` *` |
|         - | 2140 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 2141 | ` * conservative BY CONSTRUCTION:` |
|         - | 2142 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 2143 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 2144 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 2145 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 2146 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 2147 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 2148 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 2149 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 2150 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 2151 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 2152 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 2153 | ` *     instruction-count bound -> fallback.` |
|         - | 2154 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 2155 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 2156 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 2157 | ` *` |
|         - | 2158 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 2159 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 2160 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 2161 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 2162 | ` *` |
|         - | 2163 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 2164 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 2165 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 2166 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 2167 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 2168 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 2169 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 2170 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 2171 | ` * entry here.` |
|         - | 2172 | ` */` |
|         - | 2173 | `/*` |
|         - | 2174 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 2175 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 2176 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 2177 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 2178 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 2179 | ` */` |
|     68636 | 2180 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 2181 | `{` |
|     68641 | 2182 | `	int push = 0, n = 0;` |
|         - | 2183 | `	sxi32 d;` |
|     68641 | 2184 | `	switch( pI->iOp ){` |
|         - | 2185 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 2186 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     12010 | 2187 | `	case PH7_OP_LOADC:` |
|         - | 2188 | `	case PH7_OP_DUP:` |
|     24025 | 2189 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      6745 | 2190 | `	case PH7_OP_LOAD:` |
|     13495 | 2191 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|     13495 | 2192 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|       ! 0 | 2193 | `	case PH7_OP_LOAD_REF:` |
|       ! 0 | 2194 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2195 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       538 | 2196 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 2197 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 2198 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1081 | 2199 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 2200 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 2201 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       341 | 2202 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 2203 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       687 | 2204 | `		if( pI->iP2 == 0 ){` |
|       687 | 2205 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       346 | 2206 | `		}else{` |
|       ! 0 | 2207 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 2208 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 2209 | `		}` |
|       687 | 2210 | `		break;` |
|         - | 2211 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 2212 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       331 | 2213 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 2214 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 2215 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 2216 | `	case PH7_OP_NOOP:` |
|       667 | 2217 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2218 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 2219 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      1164 | 2220 | `	case PH7_OP_STORE:` |
|      2333 | 2221 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      2333 | 2222 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 2223 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1631 | 2224 | `	case PH7_OP_POP:` |
|         - | 2225 | `	case PH7_OP_CONSUME:` |
|      3267 | 2226 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2227 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 2228 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 2229 | `	 * true pop count is a runtime value — never reaches here. */` |
|      2053 | 2230 | `	case PH7_OP_CALL:` |
|      4111 | 2231 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2232 | `	/* Jumps. */` |
|       229 | 2233 | `	case PH7_OP_JMP:` |
|       463 | 2234 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|      1085 | 2235 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|      2175 | 2236 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|      2175 | 2237 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 2238 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      3689 | 2239 | `	case PH7_OP_DONE:` |
|      7383 | 2240 | `		n = 0; break;` |
|      4502 | 2241 | `	default:` |
|      9009 | 2242 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 2243 | `	}` |
|     59637 | 2244 | `	*pPush = push; *pN = n;` |
|     59637 | 2245 | `	return 1;` |
|     34323 | 2246 | `}` |
|         - | 2247 | `/*` |
|         - | 2248 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 2249 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 2250 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 2251 | ` */` |
|     11468 | 2252 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 2253 | `{` |
|         - | 2254 | `	void *pScratch;` |
|         - | 2255 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 2256 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 2257 | `	sxi32 iMax;` |
|         - | 2258 | `	int push, n, k;` |
|         - | 2259 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     11473 | 2260 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 2261 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 2262 | `		return VM_STACK_UNMODELED;` |
|         - | 2263 | `	}` |
|         - | 2264 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     60173 | 2265 | `	for( i = 0; i < nInstr; i++ ){` |
|     57709 | 2266 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|      9009 | 2267 | `			return VM_STACK_UNMODELED;` |
|         - | 2268 | `		}` |
|     24355 | 2269 | `	}` |
|         - | 2270 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 2271 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 2272 | `	 * first (the byte array last needs no alignment). */` |
|      2469 | 2273 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      2469 | 2274 | `	if( pScratch == 0 ){` |
|       ! 0 | 2275 | `		return VM_STACK_UNMODELED;` |
|         - | 2276 | `	}` |
|      2469 | 2277 | `	aH  = (sxi32 *)pScratch;` |
|      2469 | 2278 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      2469 | 2279 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     15559 | 2280 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      2469 | 2281 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      2469 | 2282 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|     13401 | 2283 | `	while( nQ > 0 ){` |
|     10937 | 2284 | `		sxu32 pc = aQ[--nQ];` |
|         - | 2285 | `		sxi32 h;` |
|     10937 | 2286 | `		aIn[pc] = 0;` |
|     10937 | 2287 | `		h = aH[pc];` |
|     10937 | 2288 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     10937 | 2289 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     10937 | 2290 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     10937 | 2291 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     19533 | 2292 | `		for( k = 0; k < n; k++ ){` |
|      8601 | 2293 | `			sxi32 hn = h + delta[k];` |
|      8601 | 2294 | `			sxu32 t = succ[k];` |
|      8601 | 2295 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      8601 | 2296 | `			if( hn > aH[t] ){` |
|      8473 | 2297 | `				aH[t] = hn;` |
|      8473 | 2298 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      4234 | 2299 | `			}` |
|      4303 | 2300 | `		}` |
|     10937 | 2301 | `		if( iMax < 0 ){ break; }` |
|         5 | 2302 | `	}` |
|      2469 | 2303 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      2469 | 2304 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|      5739 | 2305 | `}` |
|         - | 2306 | `/*` |
|         - | 2307 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 2308 | ` * our compiled PHP program.` |
|         - | 2309 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 2310 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 2311 | ` *` |
|         - | 2312 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 2313 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 2314 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 2315 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 2316 | ` */` |
|  10254145 | 2317 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 2318 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2319 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 2320 | `	)` |
|         5 | 2321 | `{` |
|         - | 2322 | `	ph7_value *pStack;` |
|         - | 2323 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 2324 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 2325 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 2326 | `  ** on the maximum stack depth required.` |
|         - | 2327 | `  **` |
|         - | 2328 | `  ** Allocation all the stack space we will ever need.` |
|         - | 2329 | `  */` |
|  10254150 | 2330 | `	nInstr += VM_STACK_GUARD;` |
|  10254150 | 2331 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|  10254150 | 2332 | `	if( pStack == 0 ){` |
|       ! 0 | 2333 | `		return 0;` |
|         - | 2334 | `	}` |
|         - | 2335 | `	/* Initialize the operand stack */` |
| 220822817 | 2336 | `	while( nInstr > 0 ){` |
| 210568672 | 2337 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
| 210568672 | 2338 | `		--nInstr;` |
|         5 | 2339 | `	}` |
|         - | 2340 | `	/* Ready for bytecode execution */` |
|  10254150 | 2341 | `	return pStack;` |
|   5127132 | 2342 | `}` |
|         - | 2343 | `/*` |
|         - | 2344 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 2345 | ` *` |
|         - | 2346 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 2347 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 2348 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 2349 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 2350 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 2351 | ` * the per-slot PH7_MemObjInit.` |
|         - | 2352 | ` *` |
|         - | 2353 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 2354 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 2355 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 2356 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|         - | 2357 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|         - | 2358 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|         - | 2359 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|         - | 2360 | ` *` |
|         - | 2361 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|         - | 2362 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|         - | 2363 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|         - | 2364 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|         - | 2365 | ` * recursion case is the one worth the O(1) simplicity.` |
|         - | 2366 | ` */` |
|         - | 2367 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 2368 | `struct VmIdleStack {` |
|         - | 2369 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 2370 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 2371 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 2372 | `};` |
|         - | 2373 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|         - | 2374 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|         - | 2375 | `                                    * (a large fallback-sized stack recursing would` |
|         - | 2376 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|         - | 2377 | `                                    * the tight-sized hot case is far below this) */` |
|         - | 2378 | `/*` |
|         - | 2379 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 2380 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 2381 | ` */` |
|   2201786 | 2382 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 2383 | `{` |
|   2201791 | 2384 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   2201791 | 2385 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|   2201791 | 2386 | `	if( pIdle && pIdle->nCap == nCap ){` |
|   1317830 | 2387 | `		ph7_value *pStack = pIdle->pStack;` |
|   1317830 | 2388 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|   1317830 | 2389 | `		pVm->nIdleOperandStacks--;` |
|         - | 2390 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 2391 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|   1317830 | 2392 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|   1317830 | 2393 | `		pVm->pIdleStackNodes = pIdle;` |
|   1317830 | 2394 | `		return pStack; /* slots already released -> reusable without re-init */` |
|         - | 2395 | `	}` |
|    883966 | 2396 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|   1101096 | 2397 | `}` |
|         - | 2398 | `/*` |
|         - | 2399 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 2400 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|         - | 2401 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|         - | 2402 | ` * live value.` |
|         - | 2403 | ` */` |
|   2201586 | 2404 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|         5 | 2405 | `{` |
|         - | 2406 | `	VmIdleStack *pIdle;` |
|         - | 2407 | `	sxu32 i;` |
|   2201591 | 2408 | `	if( pStack == 0 ){` |
|       ! 0 | 2409 | `		return;` |
|         - | 2410 | `	}` |
|   2201591 | 2411 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|    874282 | 2412 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|    874282 | 2413 | `		return;` |
|         - | 2414 | `	}` |
|         - | 2415 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 2416 | `	 * pool-allocate only when the spare list is empty. */` |
|   1327314 | 2417 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|   1327314 | 2418 | `	if( pIdle ){` |
|   1317830 | 2419 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    659061 | 2420 | `	}else{` |
|      9489 | 2421 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      9489 | 2422 | `		if( pIdle == 0 ){` |
|       ! 0 | 2423 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 2424 | `			return;` |
|         - | 2425 | `		}` |
|         - | 2426 | `	}` |
|  66910451 | 2427 | `	for( i = 0; i < nCap; i++ ){` |
|  65583142 | 2428 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 2429 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 2430 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 2431 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 2432 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 2433 | `		 * across invocations — cheap defense in depth. */` |
|  65583142 | 2434 | `		pStack[i].nIdx = SXU32_HIGH;` |
|  32847108 | 2435 | `	}` |
|   1327314 | 2436 | `	pIdle->pStack = pStack;` |
|   1327314 | 2437 | `	pIdle->nCap = nCap;` |
|   1327314 | 2438 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   1327314 | 2439 | `	pVm->pIdleOperandStacks = pIdle;` |
|   1327314 | 2440 | `	pVm->nIdleOperandStacks++;` |
|   1100996 | 2441 | `}` |
|         - | 2442 | `/* Forward declaration */` |
|         - | 2443 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 2444 | `/*` |
|         - | 2445 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 2446 | ` * This routine gets called by the PH7 engine after` |
|         - | 2447 | ` * successful compilation of the target PHP program.` |
|         - | 2448 | ` */` |
|      3956 | 2449 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 2450 | `	ph7_vm *pVm /* Target VM */` |
|         - | 2451 | `	)` |
|         5 | 2452 | `{` |
|         - | 2453 | `	SyHashEntry *pEntry;` |
|         - | 2454 | `	sxi32 rc;` |
|      3961 | 2455 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 2456 | `		/* Initialize your VM first */` |
|       ! 0 | 2457 | `		return SXERR_CORRUPT;` |
|         - | 2458 | `	}` |
|         - | 2459 | `	/* Mark the VM ready for byte-code execution */` |
|      3961 | 2460 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 2461 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 2462 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 2463 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 2464 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      3961 | 2465 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 2466 | `	/* Emit the DONE instruction */` |
|      3961 | 2467 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      3961 | 2468 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2469 | `		return SXERR_MEM;` |
|         - | 2470 | `	}` |
|         - | 2471 | `	/* Script return value */` |
|      3961 | 2472 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 2473 | `	/* Allocate a new operand stack */` |
|      3961 | 2474 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      3961 | 2475 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 2476 | `		return SXERR_MEM;` |
|         - | 2477 | `	}` |
|         - | 2478 | `	/* Set the default VM output consumer callback and it's` |
|         - | 2479 | `	 * private data. */` |
|      3961 | 2480 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      3961 | 2481 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 2482 | `	/* Allocate the reference table */` |
|      3961 | 2483 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      3961 | 2484 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      3961 | 2485 | `	if( pVm->apRefObj == 0 ){` |
|         - | 2486 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2487 | `		return SXERR_MEM;` |
|         - | 2488 | `	}` |
|         - | 2489 | `	/* Zero the reference table */` |
|      3961 | 2490 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|         - | 2491 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      3961 | 2492 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      3961 | 2493 | `	if( rc != SXRET_OK ){` |
|         - | 2494 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2495 | `		return rc;` |
|         - | 2496 | `	}` |
|         - | 2497 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 2498 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 2499 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 2500 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 2501 | `	 * below it is compile-time/init state that survives a reset. */` |
|      3961 | 2502 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|         - | 2503 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      3961 | 2504 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      3961 | 2505 | `	if( rc != SXRET_OK ){` |
|         - | 2506 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2507 | `		return rc;` |
|         - | 2508 | `	}` |
|         - | 2509 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      3961 | 2510 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 2511 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      3961 | 2512 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 2513 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      3961 | 2514 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 2515 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      3961 | 2516 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 2517 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 2518 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      3961 | 2519 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      3961 | 2520 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 2521 | `#endif` |
|         - | 2522 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2523 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      3961 | 2524 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 2525 | `#endif` |
|         - | 2526 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 2527 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      3961 | 2528 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 2529 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      3961 | 2530 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 2531 | `	/* Initialize and install static and constants class attributes.` |
|         - | 2532 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 2533 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 2534 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 2535 | `	 * that function in sync when changing what is reserved here. */` |
|         - | 2536 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - | 2537 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - | 2538 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - | 2539 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - | 2540 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - | 2541 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - | 2542 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - | 2543 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 2544 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - | 2545 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      3961 | 2546 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    543167 | 2547 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    539211 | 2548 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    539211 | 2549 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2550 | `			return rc;` |
|         - | 2551 | `		}` |
|         5 | 2552 | `	}` |
|      3961 | 2553 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    542617 | 2554 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    538665 | 2555 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    538665 | 2556 | `		if( rc != SXRET_OK ){` |
|         6 | 2557 | `			return rc;` |
|         - | 2558 | `		}` |
|         5 | 2559 | `	}` |
|         - | 2560 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      3957 | 2561 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2562 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      3957 | 2563 | `	pVm->nNextObjId = 1;` |
|         - | 2564 | `	/* VM is ready for bytecode execution */` |
|      3957 | 2565 | `	return SXRET_OK;` |
|      1983 | 2566 | `}` |
|         - | 2567 | `/*` |
|         - | 2568 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 2569 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 2570 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 2571 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 2572 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 2573 | ` */` |
|         8 | 2574 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 2575 | `{` |
|         - | 2576 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|         - | 2577 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|         - | 2578 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|       328 | 2579 | `	while( pVm->pRefList ){` |
|       320 | 2580 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|       ! 0 | 2581 | `	}` |
|         8 | 2582 | `}` |
|         - | 2583 | `/*` |
|         - | 2584 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 2585 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 2586 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 2587 | ` */` |
|        56 | 2588 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 2589 | `{` |
|        56 | 2590 | `	PH7_MemObjRelease(pObj);` |
|        56 | 2591 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        56 | 2592 | `}` |
|         - | 2593 | `/*` |
|         - | 2594 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 2595 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 2596 | ` * of statics).` |
|         - | 2597 | ` */` |
|      6736 | 2598 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|       ! 0 | 2599 | `{` |
|      6736 | 2600 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 2601 | `	sxu32 k;` |
|      6764 | 2602 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        28 | 2603 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        14 | 2604 | `	}` |
|      6736 | 2605 | `}` |
|         - | 2606 | `/*` |
|         - | 2607 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 2608 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 2609 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 2610 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 2611 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 2612 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 2613 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 2614 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 2615 | ` *    has its static sentinels reset.` |
|         - | 2616 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 2617 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 2618 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 2619 | ` */` |
|         8 | 2620 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 2621 | `{` |
|         - | 2622 | `	SyHashEntry *pEntry;` |
|         8 | 2623 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      6744 | 2624 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      6736 | 2625 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      6736 | 2626 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 2627 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|         - | 2628 | `			 * release its captured-by-value environment, then free the entry,` |
|         - | 2629 | `			 * name buffer and structure. */` |
|         4 | 2630 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|         4 | 2631 | `			const char *zName = SyStringData(&pFunc->sName);` |
|         - | 2632 | `			sxu32 k;` |
|         4 | 2633 | `			VmResetFuncStatics(pFunc);` |
|         8 | 2634 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|         4 | 2635 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|         2 | 2636 | `			}` |
|         4 | 2637 | `			SySetRelease(&pFunc->aClosureEnv);` |
|         - | 2638 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 2639 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 2640 | `			if( zName ){` |
|         4 | 2641 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|         2 | 2642 | `			}` |
|         4 | 2643 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|         4 | 2644 | `			continue;` |
|         - | 2645 | `		}` |
|         - | 2646 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     13464 | 2647 | `		while( pFunc ){` |
|      6732 | 2648 | `			VmResetFuncStatics(pFunc);` |
|      6732 | 2649 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 2650 | `		}` |
|       ! 0 | 2651 | `	}` |
|         8 | 2652 | `	pVm->closure_cnt = 0;` |
|         8 | 2653 | `}` |
|         - | 2654 | `/*` |
|         - | 2655 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 2656 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 2657 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 2658 | ` * the class re-mount registers fresh ones.` |
|         - | 2659 | ` */` |
|         8 | 2660 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 2661 | `{` |
|         - | 2662 | `	SyHashEntry *pEntry;` |
|         - | 2663 | `	/* Common case: no class static typed properties — table already empty. */` |
|         8 | 2664 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|         4 | 2665 | `		return;` |
|         - | 2666 | `	}` |
|         - | 2667 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 2668 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 2669 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 2670 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 2671 | `		if( pEntry->pUserData ){` |
|         4 | 2672 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 2673 | `		}` |
|       ! 0 | 2674 | `	}` |
|         4 | 2675 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 2676 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 | 2677 | `}` |
|         - | 2678 | `/*` |
|         - | 2679 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 2680 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 2681 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 2682 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 2683 | ` * comparing equal — both used to cast to 1.` |
|         - | 2684 | ` *` |
|         - | 2685 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 2686 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 2687 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 2688 | ` * aborting a cast.` |
|         - | 2689 | ` */` |
|        40 | 2690 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         2 | 2691 | `{` |
|         - | 2692 | `	SyHashEntry *pEntry;` |
|         - | 2693 | `	phl_res_id *pRec;` |
|        42 | 2694 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 2695 | `		return 0;` |
|         - | 2696 | `	}` |
|        42 | 2697 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|        42 | 2698 | `	if( pEntry ){` |
|        32 | 2699 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 2700 | `	}` |
|        12 | 2701 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        12 | 2702 | `	if( pRec == 0 ){` |
|       ! 0 | 2703 | `		return 0;` |
|         - | 2704 | `	}` |
|        12 | 2705 | `	pRec->pRes = pRes;` |
|        12 | 2706 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        12 | 2707 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 2708 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 2709 | `		return 0;` |
|         - | 2710 | `	}` |
|        12 | 2711 | `	return pRec->nId;` |
|        22 | 2712 | `}` |
|         - | 2713 | `/*` |
|         - | 2714 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 2715 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 2716 | ` */` |
|         8 | 2717 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 2718 | `{` |
|         - | 2719 | `	SyHashEntry *pEntry;` |
|         8 | 2720 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|         8 | 2721 | `		pVm->nResourceIdNext = 1;` |
|         8 | 2722 | `		return;` |
|         - | 2723 | `	}` |
|       ! 0 | 2724 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 2725 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 2726 | `		if( pEntry->pUserData ){` |
|       ! 0 | 2727 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 2728 | `		}` |
|       ! 0 | 2729 | `	}` |
|       ! 0 | 2730 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 2731 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 2732 | `	pVm->nResourceIdNext = 1;` |
|         4 | 2733 | `}` |
|         - | 2734 | `/*` |
|         - | 2735 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 2736 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 2737 | ` *` |
|         - | 2738 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 2739 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 2740 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 2741 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 2742 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 2743 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 2744 | ` *` |
|         - | 2745 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 2746 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 2747 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 2748 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 2749 | ` * reserved during the run.` |
|         - | 2750 | ` *` |
|         - | 2751 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 2752 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 2753 | ` * global-scope destructors never fired.` |
|         - | 2754 | ` */` |
|         8 | 2755 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 2756 | `{` |
|         - | 2757 | `	sxu32 nWater,n;` |
|         8 | 2758 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 2759 | `		return SXERR_CORRUPT;` |
|         - | 2760 | `	}` |
|         8 | 2761 | `	nWater = pVm->nSuperBaseline;` |
|         - | 2762 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 2763 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|         8 | 2764 | `	pVm->pGlobal = 0;` |
|         - | 2765 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 2766 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 2767 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 2768 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 2769 | `	 * object); unref'ing here would race the teardown below. */` |
|         8 | 2770 | `	pVm->pClosureThis = 0;` |
|         8 | 2771 | `	pVm->pClosureScope = 0;` |
|         - | 2772 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 2773 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 2774 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|         - | 2775 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|         8 | 2776 | `	pVm->bInReset = 1;` |
|         - | 2777 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|         8 | 2778 | `	VmResetRefTable(&(*pVm));` |
|         - | 2779 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 2780 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 2781 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 2782 | `	 * re-run define() overwrites the value in place). */` |
|         8 | 2783 | `	VmResetFunctionState(&(*pVm));` |
|         - | 2784 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 2785 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       344 | 2786 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|       336 | 2787 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|       336 | 2788 | `		if( pObj ){` |
|       336 | 2789 | `			PH7_MemObjRelease(pObj);` |
|       168 | 2790 | `		}` |
|       168 | 2791 | `	}` |
|         - | 2792 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 2793 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|         8 | 2794 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 2795 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 2796 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|         8 | 2797 | `	VmResetResourceIds(&(*pVm));` |
|         - | 2798 | `	/* (5) Unwind any active frames back to none. */` |
|        16 | 2799 | `	while( pVm->pFrame ){` |
|         8 | 2800 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 2801 | `	}` |
|         - | 2802 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|         8 | 2803 | `	pVm->bInReset = 0;` |
|         - | 2804 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 2805 | `	 * slots (their indices no longer exist). */` |
|         8 | 2806 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|         8 | 2807 | `	SySetReset(&pVm->aFreeObj);` |
|         - | 2808 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|         8 | 2809 | `	SyHashRelease(&pVm->hSuper);` |
|         8 | 2810 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|         - | 2811 | `	/* (8) Drain remaining per-exec containers. */` |
|         8 | 2812 | `	SySetReset(&pVm->aSelf);` |
|         - | 2813 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 2814 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 2815 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|         8 | 2816 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 2817 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 2818 | `		if( pCB ){` |
|         - | 2819 | `			int iArg;` |
|       ! 0 | 2820 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 2821 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 2822 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 2823 | `			}` |
|       ! 0 | 2824 | `		}` |
|       ! 0 | 2825 | `	}` |
|         8 | 2826 | `	SySetReset(&pVm->aShutdown);` |
|         - | 2827 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 2828 | `	 * aborted program can leave entries behind). */` |
|         8 | 2829 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|         8 | 2830 | `	SySetReset(&pVm->aException);` |
|         8 | 2831 | `	SySetReset(&pVm->aFinallyAction);` |
|         8 | 2832 | `	pVm->pPendingException = 0;` |
|         8 | 2833 | `	pVm->pInflightException = 0;` |
|         8 | 2834 | `	pVm->nInflightExcBase = 0;` |
|         8 | 2835 | `	pVm->pResumeFrame = 0;` |
|         8 | 2836 | `	pVm->iResumePc = 0;` |
|         8 | 2837 | `	pVm->pResumeInstr = 0;` |
|         8 | 2838 | `	pVm->iResumeStackDepth = 0;` |
|         8 | 2839 | `	pVm->nBoundaryRc = 0;` |
|         8 | 2840 | `	pVm->pConstEvalClass = 0;` |
|         8 | 2841 | `	pVm->nConstEvalDepth = 0;` |
|         8 | 2842 | `	pVm->pConstCycleAttr = 0;` |
|         8 | 2843 | `	pVm->pConstCycleClass = 0;` |
|         8 | 2844 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 2845 | `	{` |
|         - | 2846 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 2847 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|         8 | 2848 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|         8 | 2849 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 2850 | `		sxu32 iRmw;` |
|         8 | 2851 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 2852 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 2853 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 2854 | `		}` |
|         8 | 2855 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 2856 | `	}` |
|         8 | 2857 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2858 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2859 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2860 | `	}` |
|         8 | 2861 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|         8 | 2862 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2863 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2864 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2865 | `	}` |
|         8 | 2866 | `	pVm->pHookSetAttr = 0;` |
|         8 | 2867 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|         8 | 2868 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2869 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2870 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2871 | `	}` |
|         8 | 2872 | `	pVm->pMagicCallClass = 0;` |
|         8 | 2873 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         8 | 2874 | `	pVm->nExceptDepth = 0;` |
|         - | 2875 | `	/* spl_autoload_register() callbacks are per request */` |
|         8 | 2876 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 2877 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 2878 | `		if( pCB ){` |
|       ! 0 | 2879 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 2880 | `		}` |
|       ! 0 | 2881 | `	}` |
|         8 | 2882 | `	SySetReset(&pVm->aAutoload);` |
|         - | 2883 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 2884 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|         8 | 2885 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 2886 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 2887 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 2888 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 2889 | `	}` |
|         - | 2890 | `	/* Output buffers */` |
|         8 | 2891 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 2892 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 2893 | `		if( pOb ){` |
|       ! 0 | 2894 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 2895 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 2896 | `		}` |
|       ! 0 | 2897 | `	}` |
|         8 | 2898 | `	SySetReset(&pVm->aOB);` |
|         8 | 2899 | `	pVm->nObDepth = 0;` |
|         - | 2900 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 2901 | `	{` |
|         8 | 2902 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|         8 | 2903 | `		if( rc == SXRET_OK ){` |
|         8 | 2904 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         4 | 2905 | `		}` |
|         8 | 2906 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2907 | `			return rc;` |
|         - | 2908 | `		}` |
|         - | 2909 | `	}` |
|         - | 2910 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 2911 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 2912 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 2913 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 2914 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 2915 | `	{` |
|         - | 2916 | `		SyHashEntry *pEntry;` |
|         8 | 2917 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      1100 | 2918 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      1092 | 2919 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 2920 | `			ph7_class_attr *pAttr;` |
|         - | 2921 | `			SyHashEntry *pAttrEntry;` |
|      1092 | 2922 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|      5050 | 2923 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      3412 | 2924 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      3412 | 2925 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        60 | 2926 | `					pAttr->nIdx = SXU32_HIGH;` |
|        60 | 2927 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|        30 | 2928 | `				}` |
|       ! 0 | 2929 | `			}` |
|         - | 2930 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 2931 | `			 * slots too so VM reuse re-evaluates them. */` |
|      1092 | 2932 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|      2372 | 2933 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|      1280 | 2934 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      1280 | 2935 | `				pAttr->nIdx = SXU32_HIGH;` |
|      1280 | 2936 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 2937 | `			}` |
|       ! 0 | 2938 | `		}` |
|         8 | 2939 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      1100 | 2940 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      1092 | 2941 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      1092 | 2942 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2943 | `				return rc;` |
|         - | 2944 | `			}` |
|       ! 0 | 2945 | `		}` |
|         - | 2946 | `	}` |
|         - | 2947 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|         8 | 2948 | `	SyBlobReset(&pVm->sConsumer);` |
|         8 | 2949 | `	pVm->nOutputLen = 0;` |
|         8 | 2950 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|         8 | 2951 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|         8 | 2952 | `	pVm->iResponseStatus = 200;` |
|         8 | 2953 | `	pVm->bHeadersSent = 0;` |
|         8 | 2954 | `	pVm->bHttpContext = 0;` |
|         8 | 2955 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|         8 | 2956 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|         8 | 2957 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|         8 | 2958 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|         8 | 2959 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         8 | 2960 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2961 | `#ifdef PH7_ENABLE_PCRE` |
|         8 | 2962 | `	pVm->iPcreLastError = 0;` |
|         - | 2963 | `#endif` |
|         - | 2964 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2965 | `	/* Drop the libxml error queue and the previous request's documents */` |
|         8 | 2966 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 2967 | `#endif` |
|         8 | 2968 | `	pVm->iCmpCallbackExc = 0;` |
|         8 | 2969 | `	pVm->bHaltRequested = 0;` |
|         8 | 2970 | `	pVm->iExitStatus = 0;` |
|         8 | 2971 | `	pVm->nSpreadCallBase = 0;` |
|         8 | 2972 | `	VmSpreadCaptureReset(pVm);` |
|         8 | 2973 | `	pVm->nRecursionDepth = 0;` |
|         8 | 2974 | `	pVm->pActiveCtx = 0;` |
|         8 | 2975 | `	pVm->pCoalesceObj = 0;` |
|         8 | 2976 | `	pVm->bCoalesceArmed = 0;` |
|         8 | 2977 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 2978 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|         8 | 2979 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2980 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 2981 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|         8 | 2982 | `	pVm->nNextObjId = 1;` |
|         - | 2983 | `	/* Set the ready flag */` |
|         8 | 2984 | `	pVm->nMagic = PH7_VM_RUN;` |
|         8 | 2985 | `	return SXRET_OK;` |
|         4 | 2986 | `}` |
|         - | 2987 | `/*` |
|         - | 2988 | ` * Release a Virtual Machine.` |
|         - | 2989 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 2990 | ` */` |
|      3952 | 2991 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 2992 | `{` |
|         - | 2993 | `	/* Set the stale magic number */` |
|      3957 | 2994 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 2995 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2996 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 2997 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      3957 | 2998 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 2999 | `#endif` |
|         - | 3000 | `	/* Release the private memory subsystem */` |
|      3957 | 3001 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      3957 | 3002 | `	return SXRET_OK;` |
|         5 | 3003 | `}` |
|         - | 3004 | `/*` |
|         - | 3005 | ` * Initialize a foreign function call context.` |
|         - | 3006 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 3007 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 3008 | ` * functions.` |
|         - | 3009 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 3010 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 3011 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 3012 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 3013 | ` */` |
|   4071603 | 3014 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 3015 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 3016 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 3017 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 3018 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 3019 | `	sxi32 iFlags          /* Control flags */` |
|         - | 3020 | `	)` |
|         5 | 3021 | `{` |
|   4071608 | 3022 | `	pOut->pFunc = pFunc;` |
|   4071608 | 3023 | `	pOut->pVm   = pVm;` |
|   4071608 | 3024 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   4071608 | 3025 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 3026 | `	/* Assume a null return value */` |
|   4071608 | 3027 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   4071608 | 3028 | `	pOut->pRet = pRet;` |
|   4071608 | 3029 | `	pOut->iFlags = iFlags;` |
|   4071608 | 3030 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   4071608 | 3031 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - | 3032 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - | 3033 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - | 3034 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - | 3035 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|   4071608 | 3036 | `	pOut->pThis = 0;` |
|   4071608 | 3037 | `	pOut->pCalledClass = 0;` |
|   4071608 | 3038 | `	pOut->bThisInit = 0;` |
|   4071608 | 3039 | `	return SXRET_OK;` |
|         5 | 3040 | `}` |
|         - | 3041 | `/*` |
|         - | 3042 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 3043 | ` * left behind.` |
|         - | 3044 | ` */` |
|   4071603 | 3045 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 3046 | `{` |
|         - | 3047 | `	sxu32 n;` |
|   4071608 | 3048 | `	if( pCtx->bThisInit ){` |
|         - | 3049 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - | 3050 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - | 3051 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - | 3052 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|      5473 | 3053 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|      5473 | 3054 | `		pCtx->sThis.x.pOther = 0;` |
|      5473 | 3055 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|      5473 | 3056 | `		pCtx->bThisInit = 0;` |
|      2734 | 3057 | `	}` |
|   4071608 | 3058 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     13769 | 3059 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|     92333 | 3060 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     78569 | 3061 | `			if( apObj[n] == 0 ){` |
|         - | 3062 | `				/* Already released */` |
|      1091 | 3063 | `				continue;` |
|         - | 3064 | `			}` |
|     77483 | 3065 | `			PH7_MemObjRelease(apObj[n]);` |
|     77483 | 3066 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     38744 | 3067 | `		}` |
|     13769 | 3068 | `		SySetRelease(&pCtx->sVar);` |
|      6882 | 3069 | `	}` |
|   4071608 | 3070 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 3071 | `		ph7_aux_data *aAux;` |
|         - | 3072 | `		void *pChunk;` |
|         - | 3073 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 3074 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 3075 | `		 */` |
|       163 | 3076 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       491 | 3077 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       333 | 3078 | `			pChunk = aAux[n].pAuxData;` |
|         - | 3079 | `			/* Release the chunk */` |
|       333 | 3080 | `			if( pChunk ){` |
|       333 | 3081 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       164 | 3082 | `			}` |
|       169 | 3083 | `		}` |
|       163 | 3084 | `		SySetRelease(&pCtx->sChunk);` |
|        79 | 3085 | `	}` |
|   4071608 | 3086 | `}` |
|         - | 3087 | `/*` |
|         - | 3088 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 3089 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 3090 | ` */` |
|      1086 | 3091 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 3092 | `	ph7_context *pCtx, /* Call context */` |
|         - | 3093 | `	ph7_value *pValue  /* Release this value */` |
|         - | 3094 | `	)` |
|         5 | 3095 | `{` |
|      1091 | 3096 | `	if( pValue == 0 ){` |
|         - | 3097 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 3098 | `		return;` |
|         - | 3099 | `	}` |
|      1091 | 3100 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      1091 | 3101 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 3102 | `		sxu32 n;` |
|      2717 | 3103 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      2717 | 3104 | `			if( apObj[n] == pValue ){` |
|      1091 | 3105 | `				PH7_MemObjRelease(pValue);` |
|      1091 | 3106 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 3107 | `				/* Mark as released */` |
|      1091 | 3108 | `				apObj[n] = 0;` |
|      1091 | 3109 | `				break;` |
|         - | 3110 | `			}` |
|       818 | 3111 | `		}` |
|       543 | 3112 | `	}` |
|       548 | 3113 | `}` |
|         - | 3114 | `/*` |
|         - | 3115 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 3116 | ` */` |
|  34246894 | 3117 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 3118 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 3119 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 3120 | `	)` |
|         5 | 3121 | `{` |
|  34246899 | 3122 | `	ph7_value *pTos = *ppTos;` |
|  69400082 | 3123 | `	while( nPop > 0 ){` |
|  35153188 | 3124 | `		PH7_MemObjRelease(pTos);` |
|  35153188 | 3125 | `		pTos--;` |
|  35153188 | 3126 | `		nPop--;` |
|         5 | 3127 | `	}` |
|         - | 3128 | `	/* Top of the stack */` |
|  34246899 | 3129 | `	*ppTos = pTos;` |
|  34246899 | 3130 | `}` |
|         - | 3131 | `/*` |
|         - | 3132 | ` * Reserve a memory object.` |
|         - | 3133 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 3134 | ` */` |
|  23128438 | 3135 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 3136 | `{` |
|  23128443 | 3137 | `	ph7_value *pObj = 0;` |
|         - | 3138 | `	VmSlot *pSlot;` |
|         - | 3139 | `	sxu32 nIdx;` |
|         - | 3140 | `	/* Check for a free slot */` |
|  23128443 | 3141 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  23128443 | 3142 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  23128443 | 3143 | `	if( pSlot ){` |
|  20259527 | 3144 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  20259527 | 3145 | `		nIdx = pSlot->nIdx;` |
|  10130949 | 3146 | `	}` |
|  23128443 | 3147 | `	if( pObj == 0 ){` |
|         - | 3148 | `		/* Reserve a new memory object */` |
|   2868921 | 3149 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   2868921 | 3150 | `		if( pObj == 0 ){` |
|       ! 0 | 3151 | `			return 0;` |
|         - | 3152 | `		}` |
|   1434458 | 3153 | `	}` |
|         - | 3154 | `	/* Set a null default value */` |
|  23128443 | 3155 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  23128443 | 3156 | `	pObj->nIdx = nIdx;` |
|  23128443 | 3157 | `	return pObj;` |
|  11565412 | 3158 | `}` |
|         - | 3159 | `/*` |
|         - | 3160 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 3161 | ` */` |
|     53644 | 3162 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 3163 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 3164 | `	const char *zKey,  /* Entry key */` |
|         - | 3165 | `	sxu32 nByte,       /* Key length */` |
|         - | 3166 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 3167 | `	)` |
|         5 | 3168 | `{` |
|         - | 3169 | `	ph7_value sKey;` |
|         - | 3170 | `	sxi32 rc;` |
|     53649 | 3171 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     53649 | 3172 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 3173 | `	/* Perform the insertion */` |
|     53649 | 3174 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     53649 | 3175 | `	PH7_MemObjRelease(&sKey);` |
|     53649 | 3176 | `	return rc;` |
|         5 | 3177 | `}` |
|         - | 3178 | `/*` |
|         - | 3179 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 3180 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 3181 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 3182 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 3183 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 3184 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 3185 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 3186 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 3187 | ` *     superglobal in place.` |
|         - | 3188 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 3189 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING` |
|         - | 3190 | ` *     name is rejected with the engine's usual "already exists" diagnostic` |
|         - | 3191 | ` *     (the same limitation OP_STORE_REF has for plain variables).` |
|         - | 3192 | ` */` |
|       168 | 3193 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         3 | 3194 | `{` |
|       171 | 3195 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 3196 | `	SyHashEntry *pEntry;` |
|         - | 3197 | `	ph7_value *pObj;` |
|         - | 3198 | `	char *zDup;` |
|         - | 3199 | `	sxu32 nIdx;` |
|         - | 3200 | `	sxi32 rc;` |
|         - | 3201 | `	/* Walk down to the global frame */` |
|       175 | 3202 | `	while( pFrame->pParent ){` |
|         5 | 3203 | `		pFrame = pFrame->pParent;` |
|         1 | 3204 | `	}` |
|         - | 3205 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       171 | 3206 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|       171 | 3207 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 3208 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 3209 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 3210 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 3211 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 3212 | `		 * of $GLOBALS itself). */` |
|         5 | 3213 | `		pEntry = 0;` |
|         2 | 3214 | `	}` |
|       171 | 3215 | `	if( pEntry == 0 ){` |
|       171 | 3216 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|        84 | 3217 | `	}` |
|       171 | 3218 | `	if( pEntry ){` |
|         3 | 3219 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 3220 | `			SyString sName;` |
|       ! 0 | 3221 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|       ! 0 | 3222 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|       ! 0 | 3223 | `			return SXRET_OK;` |
|         - | 3224 | `		}` |
|         3 | 3225 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 3226 | `		if( pObj == 0 ){` |
|       ! 0 | 3227 | `			return SXERR_NOTFOUND;` |
|         - | 3228 | `		}` |
|         3 | 3229 | `		if( pValue ){` |
|         3 | 3230 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 3231 | `		}else{` |
|       ! 0 | 3232 | `			PH7_MemObjToNull(pObj);` |
|         - | 3233 | `		}` |
|         3 | 3234 | `		return SXRET_OK;` |
|         - | 3235 | `	}` |
|       169 | 3236 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 3237 | `		/* Reserve a fresh slot for the new global */` |
|       167 | 3238 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       167 | 3239 | `		if( pObj == 0 ){` |
|       ! 0 | 3240 | `			return SXERR_MEM;` |
|         - | 3241 | `		}` |
|       167 | 3242 | `		nIdx = pObj->nIdx;` |
|        85 | 3243 | `	}else{` |
|         - | 3244 | `		/* Reference assignment: bind the name to the existing slot */` |
|         3 | 3245 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|         3 | 3246 | `		if( pObj == 0 ){` |
|       ! 0 | 3247 | `			return SXERR_NOTFOUND;` |
|         - | 3248 | `		}` |
|         3 | 3249 | `		nIdx = nRefIdx;` |
|         - | 3250 | `	}` |
|       169 | 3251 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       169 | 3252 | `	if( zDup == 0 ){` |
|       ! 0 | 3253 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3254 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 3255 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|         - | 3256 | `			VmSlot sFree;` |
|       ! 0 | 3257 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3258 | `			sFree.pUserData = 0;` |
|       ! 0 | 3259 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3260 | `		}` |
|       ! 0 | 3261 | `		return SXERR_MEM;` |
|         - | 3262 | `	}` |
|       169 | 3263 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       169 | 3264 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3265 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3266 | `			VmSlot sFree;` |
|       ! 0 | 3267 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3268 | `			sFree.pUserData = 0;` |
|       ! 0 | 3269 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3270 | `		}` |
|       ! 0 | 3271 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 3272 | `		return rc;` |
|         - | 3273 | `	}` |
|         - | 3274 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       169 | 3275 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       169 | 3276 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       169 | 3277 | `	if( nRefIdx == SXU32_HIGH ){` |
|       167 | 3278 | `		pObj->nIdx = nIdx;` |
|       167 | 3279 | `		if( pValue ){` |
|       164 | 3280 | `			PH7_MemObjStore(pValue,pObj);` |
|        81 | 3281 | `		}` |
|        82 | 3282 | `	}` |
|       169 | 3283 | `	return SXRET_OK;` |
|        87 | 3284 | `}` |
|         - | 3285 | `/*` |
|         - | 3286 | ` * Extract a variable value from the top active VM frame.` |
|         - | 3287 | ` * Return a pointer to the variable value on success.` |
|         - | 3288 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 3289 | ` */` |
|  25008908 | 3290 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 3291 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 3292 | `	const SyString *pName, /* Variable name */` |
|         - | 3293 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 3294 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 3295 | `	)` |
|         5 | 3296 | `{` |
|  25008913 | 3297 | `	int bNullify = FALSE;` |
|         - | 3298 | `	SyHashEntry *pEntry;` |
|         - | 3299 | `	VmFrame *pFrame;` |
|         - | 3300 | `	ph7_value *pObj;` |
|         - | 3301 | `	sxu32 nIdx;` |
|         - | 3302 | `	sxi32 rc;` |
|         - | 3303 | `	/* Point to the top active frame */` |
|  25008913 | 3304 | `	pFrame = pVm->pFrame;` |
|  25008913 | 3305 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 3306 | `	/* Perform the lookup */` |
|  25008913 | 3307 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 3308 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        16 | 3309 | `		pName = &sAnnon;` |
|         - | 3310 | `		/* Always nullify the object */` |
|        16 | 3311 | `		bNullify = TRUE;` |
|        16 | 3312 | `		bDup = FALSE;` |
|         7 | 3313 | `	}` |
|         - | 3314 | `	/* Check the superglobals table first */` |
|  25008913 | 3315 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  25008913 | 3316 | `	if( pEntry == 0 ){` |
|         - | 3317 | `		/* Query the top active frame */` |
|  25008391 | 3318 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  25008391 | 3319 | `		if( pEntry == 0 ){` |
|   6886517 | 3320 | `			char *zName = (char *)pName->zString;` |
|         - | 3321 | `			VmSlot sLocal;` |
|   6886517 | 3322 | `			if( !bCreate ){` |
|         - | 3323 | `				/* Do not create the variable,return NULL instead */` |
|      3591 | 3324 | `				return 0;` |
|         - | 3325 | `			}` |
|         - | 3326 | `			/* No such variable,automatically create a new one and install` |
|         - | 3327 | `			 * it in the current frame.` |
|         - | 3328 | `			 */` |
|   6882931 | 3329 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   6882931 | 3330 | `			if( pObj == 0 ){` |
|       ! 0 | 3331 | `				return 0;` |
|         - | 3332 | `			}` |
|   6882931 | 3333 | `			nIdx = pObj->nIdx;` |
|   6882931 | 3334 | `			if( bDup ){` |
|         - | 3335 | `				/* Duplicate name */` |
|      1137 | 3336 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      1137 | 3337 | `				if( zName == 0 ){` |
|       ! 0 | 3338 | `					return 0;` |
|         - | 3339 | `				}` |
|       566 | 3340 | `			}` |
|         - | 3341 | `			/* Link to the top active VM frame */` |
|   6882931 | 3342 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   6882931 | 3343 | `			if( rc != SXRET_OK ){` |
|         - | 3344 | `				/* Return the slot to the free pool */` |
|       ! 0 | 3345 | `				sLocal.nIdx = nIdx;` |
|       ! 0 | 3346 | `				sLocal.pUserData = 0;` |
|       ! 0 | 3347 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|       ! 0 | 3348 | `				return 0;` |
|         - | 3349 | `			}` |
|   6882931 | 3350 | `			if( pFrame->pParent != 0 ){` |
|         - | 3351 | `				/* Local variable */` |
|   6873055 | 3352 | `				sLocal.nIdx = nIdx;` |
|   6873055 | 3353 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   3437718 | 3354 | `			}else{` |
|         - | 3355 | `				/* Register in the $GLOBALS array */` |
|      9881 | 3356 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|         - | 3357 | `			}` |
|         - | 3358 | `			/* Install in the reference table */` |
|   6882931 | 3359 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 3360 | `			/* Save object index */` |
|   6882931 | 3361 | `			pObj->nIdx = nIdx;` |
|   3442656 | 3362 | `		}else{` |
|         - | 3363 | `			/* Extract variable contents */` |
|  18121879 | 3364 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  18121879 | 3365 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  18121879 | 3366 | `			if( bNullify && pObj ){` |
|         3 | 3367 | `				PH7_MemObjRelease(pObj);` |
|         1 | 3368 | `			}` |
|         - | 3369 | `		}` |
|  12507710 | 3370 | `	}else{` |
|         - | 3371 | `		/* Superglobal */` |
|       527 | 3372 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       527 | 3373 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 3374 | `	}` |
|  25005327 | 3375 | `	return pObj;` |
|  12509764 | 3376 | `}` |
|         - | 3377 | `/*` |
|         - | 3378 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 3379 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 3380 | ` */` |
|     36034 | 3381 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 3382 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 3383 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 3384 | `	sxu32 nByte        /* zName length */` |
|         - | 3385 | `	)` |
|         5 | 3386 | `{` |
|         - | 3387 | `	SyHashEntry *pEntry;` |
|         - | 3388 | `	ph7_value *pValue;` |
|         - | 3389 | `	sxu32 nIdx;` |
|         - | 3390 | `	/* Query the superglobal table */` |
|     36039 | 3391 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     36039 | 3392 | `	if( pEntry == 0 ){` |
|         - | 3393 | `		/* No such entry */` |
|       ! 0 | 3394 | `		return 0;` |
|         - | 3395 | `	}` |
|         - | 3396 | `	/* Extract the superglobal index in the global object pool */` |
|     36039 | 3397 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3398 | `	/* Extract the variable value  */` |
|     36039 | 3399 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     36039 | 3400 | `	return pValue;` |
|     18022 | 3401 | `}` |
|         - | 3402 | `/*` |
|         - | 3403 | ` * Perform a raw hashmap insertion.` |
|         - | 3404 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 3405 | ` */` |
|     28204 | 3406 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 3407 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 3408 | `	const char *zKey,   /* Entry key */` |
|         - | 3409 | `	int nKeylen,        /* zKey length*/` |
|         - | 3410 | `	const char *zData,  /* Entry data */` |
|         - | 3411 | `	int nLen            /* zData length */` |
|         - | 3412 | `	)` |
|         5 | 3413 | `{` |
|         - | 3414 | `	ph7_value sKey,sValue;` |
|         - | 3415 | `	sxi32 rc;` |
|     28209 | 3416 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     28209 | 3417 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     28209 | 3418 | `	if( zKey ){` |
|     24207 | 3419 | `		if( nKeylen < 0 ){` |
|     24103 | 3420 | `			nKeylen = (int)SyStrlen(zKey);` |
|     12049 | 3421 | `		}` |
|     24207 | 3422 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     12101 | 3423 | `	}` |
|     28209 | 3424 | `	if( zData ){` |
|     28209 | 3425 | `		if( nLen < 0 ){` |
|         - | 3426 | `			/* Compute length automatically */` |
|     15995 | 3427 | `			nLen = (int)SyStrlen(zData);` |
|      7995 | 3428 | `		}` |
|     28209 | 3429 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     14102 | 3430 | `	}` |
|         - | 3431 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 3432 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 3433 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 3434 | `	 * every argument under "". */` |
|     28209 | 3435 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     28209 | 3436 | `	PH7_MemObjRelease(&sKey);` |
|     28209 | 3437 | `	PH7_MemObjRelease(&sValue);` |
|     28209 | 3438 | `	return rc;` |
|         5 | 3439 | `}` |
|         - | 3440 | `/*` |
|         - | 3441 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 3442 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 3443 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 3444 | ` */` |
|        34 | 3445 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 3446 | `{` |
|        38 | 3447 | `	sxi64 iVal = 0;` |
|        38 | 3448 | `	if( nValue == 0 ){` |
|       ! 0 | 3449 | `		return 0;` |
|         - | 3450 | `	}` |
|        34 | 3451 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 3452 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 3453 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 3454 | `		return 1;` |
|         - | 3455 | `	}` |
|        38 | 3456 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 3457 | `	return iVal != 0;` |
|        21 | 3458 | `}` |
|         - | 3459 | `/*` |
|         - | 3460 | ` * Configure a working virtual machine instance.` |
|         - | 3461 | ` *` |
|         - | 3462 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 3463 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 3464 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 3465 | ` * The second argument to this function is an integer configuration option` |
|         - | 3466 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 3467 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 3468 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 3469 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 3470 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 3471 | ` */` |
|    107412 | 3472 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 3473 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3474 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 3475 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 3476 | `	)` |
|         5 | 3477 | `{` |
|    107417 | 3478 | `	sxi32 rc = SXRET_OK;` |
|    107417 | 3479 | `	switch(nOp){` |
|      1963 | 3480 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      3931 | 3481 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      3931 | 3482 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3483 | `		/* VM output consumer callback */` |
|         - | 3484 | `#ifdef UNTRUST` |
|         - | 3485 | `		if( xConsumer == 0 ){` |
|         - | 3486 | `			rc = SXERR_CORRUPT;` |
|         - | 3487 | `			break;` |
|         - | 3488 | `		}` |
|         - | 3489 | `#endif` |
|         - | 3490 | `		/* Install the output consumer */` |
|      3931 | 3491 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      3931 | 3492 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      3931 | 3493 | `		break;` |
|         - | 3494 | `							   }` |
|      1963 | 3495 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      3931 | 3496 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      3931 | 3497 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3498 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 3499 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 3500 | `		 * instead of the program-output stream. */` |
|         - | 3501 | `#ifdef UNTRUST` |
|         - | 3502 | `		if( xConsumer == 0 ){` |
|         - | 3503 | `			rc = SXERR_CORRUPT;` |
|         - | 3504 | `			break;` |
|         - | 3505 | `		}` |
|         - | 3506 | `#endif` |
|      3931 | 3507 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      3931 | 3508 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      3931 | 3509 | `		break;` |
|         - | 3510 | `								   }` |
|      1976 | 3511 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 3512 | `		/* Import path */` |
|         - | 3513 | `		  const char *zPath;` |
|         - | 3514 | `		  SyString sPath;` |
|      3957 | 3515 | `		  zPath = va_arg(ap,const char *);` |
|         - | 3516 | `#if defined(UNTRUST)` |
|         - | 3517 | `		  if( zPath == 0 ){` |
|         - | 3518 | `			  rc = SXERR_EMPTY;` |
|         - | 3519 | `			  break;` |
|         - | 3520 | `		  }` |
|         - | 3521 | `#endif` |
|      3957 | 3522 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 3523 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 3524 | `#ifdef __WINNT__` |
|         5 | 3525 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 3526 | `#endif` |
|      7909 | 3527 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 3528 | `		  /* Remove leading and trailing white spaces */` |
|      3957 | 3529 | `		  SyStringFullTrim(&sPath);` |
|      3957 | 3530 | `		  if( sPath.nByte > 0 ){` |
|         - | 3531 | `			  /* Store the path in the corresponding conatiner */` |
|      3957 | 3532 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      1976 | 3533 | `		  }` |
|      3957 | 3534 | `		  break;` |
|         - | 3535 | `									 }` |
|      1979 | 3536 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 3537 | `		/* Run-Time Error report */` |
|      3963 | 3538 | `		pVm->bErrReport = 1;` |
|      3963 | 3539 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      3963 | 3540 | `		break;` |
|         2 | 3541 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 3542 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 3543 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 3544 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 3545 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 3546 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 3547 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 3548 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 3549 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 3550 | `		int nDepth = va_arg(ap,int);` |
|         5 | 3551 | `		if( nDepth >= 0 ){` |
|         5 | 3552 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 3553 | `		}` |
|         5 | 3554 | `		break;` |
|         - | 3555 | `									   }` |
|         5 | 3556 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 3557 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 3558 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 3559 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 3560 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 3561 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 3562 | `		 * so it is rejected as a footgun). */` |
|        12 | 3563 | `		int nDepth = va_arg(ap,int);` |
|        12 | 3564 | `		if( nDepth > 1 ){` |
|        12 | 3565 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 3566 | `		}` |
|        12 | 3567 | `		break;` |
|         - | 3568 | `									   }` |
|       ! 0 | 3569 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 3570 | `		/* VM output length in bytes */` |
|       ! 0 | 3571 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 3572 | `#ifdef UNTRUST` |
|         - | 3573 | `		if( pOut == 0 ){` |
|         - | 3574 | `			rc = SXERR_CORRUPT;` |
|         - | 3575 | `			break;` |
|         - | 3576 | `		}` |
|         - | 3577 | `#endif` |
|       ! 0 | 3578 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 3579 | `		break;` |
|         - | 3580 | `							   }` |
|         - | 3581 |  |
|     21783 | 3582 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 3583 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 3584 | `		/* Create a new superglobal/global variable */` |
|     43571 | 3585 | `		const char *zName = va_arg(ap,const char *);` |
|     43571 | 3586 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 3587 | `		SyHashEntry *pEntry;` |
|         - | 3588 | `		ph7_value *pObj;` |
|         - | 3589 | `		sxu32 nByte;` |
|         - | 3590 | `		sxu32 nIdx;` |
|         - | 3591 | `#ifdef UNTRUST` |
|         - | 3592 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 3593 | `			rc = SXERR_CORRUPT;` |
|         - | 3594 | `			break;` |
|         - | 3595 | `		}` |
|         - | 3596 | `#endif` |
|     43571 | 3597 | `		nByte = SyStrlen(zName);` |
|     43571 | 3598 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3599 | `			/* Check if the superglobal is already installed */` |
|     39645 | 3600 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     19825 | 3601 | `		}else{` |
|         - | 3602 | `			/* Query the top active VM frame */` |
|      3931 | 3603 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 3604 | `		}` |
|     43571 | 3605 | `		if( pEntry ){` |
|         - | 3606 | `			/* Variable already installed */` |
|       ! 0 | 3607 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3608 | `			/* Extract contents */` |
|       ! 0 | 3609 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 3610 | `			if( pObj ){` |
|         - | 3611 | `				/* Overwrite old contents */` |
|       ! 0 | 3612 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 3613 | `			}` |
|       ! 0 | 3614 | `		}else{` |
|         - | 3615 | `			/* Install a new variable */` |
|     43571 | 3616 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     43571 | 3617 | `			if( pObj == 0 ){` |
|       ! 0 | 3618 | `				rc = SXERR_MEM;` |
|       ! 0 | 3619 | `				break;` |
|         - | 3620 | `			}` |
|     43571 | 3621 | `			nIdx = pObj->nIdx;` |
|         - | 3622 | `			/* Copy value */` |
|     43571 | 3623 | `			PH7_MemObjStore(pValue,pObj);` |
|     43571 | 3624 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3625 | `				/* Install the superglobal */` |
|     39645 | 3626 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     19825 | 3627 | `			}else{` |
|         - | 3628 | `				/* Install in the current frame */` |
|      3931 | 3629 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 3630 | `			}` |
|     43571 | 3631 | `			if( rc == SXRET_OK ){` |
|         - | 3632 | `				SyHashEntry *pRef;` |
|     43571 | 3633 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     39645 | 3634 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     19825 | 3635 | `				}else{` |
|      3931 | 3636 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 3637 | `				}` |
|         - | 3638 | `				/* Install in the reference table */` |
|     43571 | 3639 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     43571 | 3640 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 3641 | `					/* Register in the $GLOBALS array */` |
|     43571 | 3642 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     21783 | 3643 | `				}` |
|     21783 | 3644 | `			}` |
|         - | 3645 | `		}` |
|     43571 | 3646 | `		break;` |
|         - | 3647 | `									}` |
|     12049 | 3648 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 3649 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 3650 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 3651 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 3652 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 3653 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 3654 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     24103 | 3655 | `		const char *zKey   = va_arg(ap,const char *);` |
|     24103 | 3656 | `		const char *zValue = va_arg(ap,const char *);` |
|     24103 | 3657 | `		int nLen = va_arg(ap,int);` |
|         - | 3658 | `		ph7_hashmap *pMap;` |
|         - | 3659 | `		ph7_value *pValue;` |
|     24103 | 3660 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 3661 | `			/* Extract the $_ENV superglobal */` |
|         3 | 3662 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     24102 | 3663 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 3664 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 3665 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     24101 | 3666 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 3667 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 3668 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     24101 | 3669 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 3670 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 3671 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     24101 | 3672 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 3673 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 3674 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     24101 | 3675 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 3676 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 3677 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 3678 | `		}else{` |
|         - | 3679 | `			/* Extract the $_SERVER superglobal */` |
|     24101 | 3680 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 3681 | `		}` |
|     24103 | 3682 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3683 | `			/* No such entry */` |
|       ! 0 | 3684 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3685 | `			break;` |
|         - | 3686 | `		}` |
|         - | 3687 | `		/* Point to the hashmap */` |
|     24103 | 3688 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3689 | `		/* Perform the insertion */` |
|     24103 | 3690 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     24103 | 3691 | `		break;` |
|         - | 3692 | `								   }` |
|      2001 | 3693 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 3694 | `		/* Script arguments */` |
|      4007 | 3695 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 3696 | `		ph7_hashmap *pMap;` |
|         - | 3697 | `		ph7_value *pValue;` |
|         - | 3698 | `		sxu32 n;` |
|         - | 3699 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 3700 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 3701 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 3702 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      4007 | 3703 | `		if( zValue == 0 ){` |
|       ! 0 | 3704 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3705 | `			break;` |
|         - | 3706 | `		}` |
|         - | 3707 | `		/* Extract the $argv array */` |
|      4007 | 3708 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4007 | 3709 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3710 | `			/* No such entry */` |
|       ! 0 | 3711 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3712 | `			break;` |
|         - | 3713 | `		}` |
|         - | 3714 | `		/* Point to the hashmap */` |
|      4007 | 3715 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3716 | `		/* Perform the insertion */` |
|      4007 | 3717 | `		n = (sxu32)SyStrlen(zValue);` |
|      4007 | 3718 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      4007 | 3719 | `		break;` |
|         - | 3720 | `								  }` |
|      1963 | 3721 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 3722 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 3723 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 3724 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 3725 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 3726 | `		ph7_value *pArgv,*pServer;` |
|         - | 3727 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 3728 | `		ph7_value sArgvVal,sKey,sCount;` |
|      3931 | 3729 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      3931 | 3730 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      3926 | 3731 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      3931 | 3732 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 3733 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3734 | `			break;` |
|         - | 3735 | `		}` |
|      3931 | 3736 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      3931 | 3737 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 3738 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      3931 | 3739 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      3931 | 3740 | `		if( pDup == 0 ){` |
|       ! 0 | 3741 | `			rc = SXERR_MEM;` |
|       ! 0 | 3742 | `			break;` |
|         - | 3743 | `		}` |
|      3931 | 3744 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      3931 | 3745 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      3931 | 3746 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      3931 | 3747 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      3931 | 3748 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      3931 | 3749 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      3931 | 3750 | `		PH7_MemObjRelease(&sKey);` |
|         - | 3751 | `		/* $_SERVER['argc'] = count($argv). */` |
|      3931 | 3752 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      3931 | 3753 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      3931 | 3754 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      3931 | 3755 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      3931 | 3756 | `		PH7_MemObjRelease(&sCount);` |
|      3931 | 3757 | `		PH7_MemObjRelease(&sKey);` |
|      3931 | 3758 | `		rc = SXRET_OK;` |
|      3931 | 3759 | `		break;` |
|         - | 3760 | `								  }` |
|        45 | 3761 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 3762 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 3763 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 3764 | `		 * apply immediately so they take effect even if the script never` |
|         - | 3765 | `		 * touches the INI API. */` |
|        94 | 3766 | `		const char *zName = va_arg(ap,const char *);` |
|        94 | 3767 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 3768 | `		VmIniEntry sEntry;` |
|         - | 3769 | `		char *zDupN,*zDupV;` |
|         - | 3770 | `		sxu32 nName,nValue;` |
|        94 | 3771 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 3772 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3773 | `			break;` |
|         - | 3774 | `		}` |
|        94 | 3775 | `		if( zValue == 0 ){` |
|       ! 0 | 3776 | `			zValue = "";` |
|       ! 0 | 3777 | `		}` |
|        94 | 3778 | `		nName = (sxu32)SyStrlen(zName);` |
|        94 | 3779 | `		nValue = (sxu32)SyStrlen(zValue);` |
|        94 | 3780 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|        94 | 3781 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|        94 | 3782 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 3783 | `			rc = SXERR_MEM;` |
|       ! 0 | 3784 | `			break;` |
|         - | 3785 | `		}` |
|        94 | 3786 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|        94 | 3787 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|        94 | 3788 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|        94 | 3789 | `		if( rc == SXRET_OK ){` |
|        90 | 3790 | `			if( nName == sizeof("error_reporting")-1` |
|        68 | 3791 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         2 | 3792 | `				sxi64 iLevel = 0;` |
|         2 | 3793 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         2 | 3794 | `				pVm->bErrReport = iLevel != 0;` |
|        89 | 3795 | `			}else if( nName == sizeof("date.timezone")-1` |
|        44 | 3796 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|       ! 0 | 3797 | `			 && nValue == 3` |
|         4 | 3798 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 3799 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 3800 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 3801 | `				pVm->nDefTz = 3;` |
|        88 | 3802 | `			}else if( nName == sizeof("zend.assertions")-1` |
|        84 | 3803 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 3804 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 3805 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 3806 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 3807 | `				sxi64 iZend = 0;` |
|        40 | 3808 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 3809 | `				if( iZend >= 1 ){` |
|        40 | 3810 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 3811 | `				}else{` |
|       ! 0 | 3812 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 3813 | `				}` |
|        74 | 3814 | `			}else if( nName == sizeof("display_errors")-1` |
|        39 | 3815 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 3816 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 3817 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 3818 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 3819 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        47 | 3820 | `			}else if( nName == sizeof("log_errors")-1` |
|        29 | 3821 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 3822 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|         8 | 3823 | `			}` |
|        45 | 3824 | `		}` |
|        94 | 3825 | `		break;` |
|         - | 3826 | `								  }` |
|       ! 0 | 3827 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 3828 | `		/* error_log() consumer */` |
|       ! 0 | 3829 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 3830 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 3831 | `		break;` |
|         - | 3832 | `										}` |
|       ! 0 | 3833 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 3834 | `		/* Script return value */` |
|       ! 0 | 3835 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 3836 | `#ifdef UNTRUST` |
|         - | 3837 | `		if( ppValue == 0 ){` |
|         - | 3838 | `			rc = SXERR_CORRUPT;` |
|         - | 3839 | `			break;` |
|         - | 3840 | `		}` |
|         - | 3841 | `#endif` |
|       ! 0 | 3842 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 3843 | `		break;` |
|         - | 3844 | `								   }` |
|      7913 | 3845 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 3846 | `		/* Register an IO stream device */` |
|     15831 | 3847 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 3848 | `		/* Make sure we are dealing with a valid IO stream */` |
|     15826 | 3849 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     15831 | 3850 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|         - | 3851 | `				/* Invalid stream */` |
|       ! 0 | 3852 | `				rc = SXERR_INVALID;` |
|       ! 0 | 3853 | `				break;` |
|         - | 3854 | `		}` |
|     15831 | 3855 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 3856 | `			/* Make the 'file://' stream the defaut stream device */` |
|      3961 | 3857 | `			pVm->pDefStream = pStream;` |
|      1978 | 3858 | `		}` |
|         - | 3859 | `		/* Insert in the appropriate container */` |
|     15831 | 3860 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     15831 | 3861 | `		break;` |
|         - | 3862 | `								  }` |
|        16 | 3863 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 3864 | `		/* Point to the VM internal output consumer buffer */` |
|        32 | 3865 | `		const void **ppOut = va_arg(ap,const void **);` |
|        32 | 3866 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 3867 | `#ifdef UNTRUST` |
|         - | 3868 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 3869 | `			rc = SXERR_CORRUPT;` |
|         - | 3870 | `			break;` |
|         - | 3871 | `		}` |
|         - | 3872 | `#endif` |
|        32 | 3873 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        32 | 3874 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        32 | 3875 | `		break;` |
|         - | 3876 | `									   }` |
|        16 | 3877 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 3878 | `		/* Raw HTTP request*/` |
|        32 | 3879 | `		const char *zRequest = va_arg(ap,const char *);` |
|        32 | 3880 | `		int nByte = va_arg(ap,int);` |
|        32 | 3881 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 3882 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3883 | `			break;` |
|         - | 3884 | `		}` |
|        32 | 3885 | `		if( nByte < 0 ){` |
|         - | 3886 | `			/* Compute length automatically */` |
|       ! 0 | 3887 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 3888 | `		}` |
|         - | 3889 | `		/* Process the request */` |
|        32 | 3890 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 3891 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        32 | 3892 | `		if( rc == SXRET_OK ){` |
|        30 | 3893 | `			pVm->bHttpContext = 1;` |
|        15 | 3894 | `		}` |
|        32 | 3895 | `		break;` |
|         - | 3896 | `									}` |
|        16 | 3897 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 3898 | `		/* Extract HTTP response status code */` |
|        32 | 3899 | `		int *pStatus = va_arg(ap, int *);` |
|        32 | 3900 | `		if( pStatus ){` |
|        32 | 3901 | `			*pStatus = pVm->iResponseStatus;` |
|        16 | 3902 | `		}` |
|        32 | 3903 | `		break;` |
|         - | 3904 | `										}` |
|        16 | 3905 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 3906 | `		/* Iterate response headers via callback */` |
|         - | 3907 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        32 | 3908 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        32 | 3909 | `		void *pUserData = va_arg(ap, void *);` |
|        32 | 3910 | `		if( xCallback ){` |
|        32 | 3911 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        32 | 3912 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|        44 | 3913 | `			for( k = 0; k < nHdr; k++ ){` |
|        18 | 3914 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        12 | 3915 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|         6 | 3916 | `							   pUserData);` |
|        12 | 3917 | `				if( rc != PH7_OK ){` |
|       ! 0 | 3918 | `					break;` |
|         - | 3919 | `				}` |
|         6 | 3920 | `			}` |
|        16 | 3921 | `		}` |
|        32 | 3922 | `		break;` |
|         - | 3923 | `										 }` |
|       ! 0 | 3924 | `	default:` |
|         - | 3925 | `		/* Unknown configuration option */` |
|       ! 0 | 3926 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 3927 | `		break;` |
|         - | 3928 | `	}` |
|    107417 | 3929 | `	return rc;` |
|         5 | 3930 | `}` |
|         - | 3931 | `/* Forward declaration */` |
|         - | 3932 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 3933 | `/*` |
|         - | 3934 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 3935 | ` * format.` |
|         - | 3936 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 3937 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 3938 | ` * (STDOUT).` |
|         - | 3939 | ` */` |
|         2 | 3940 | `static sxi32 VmByteCodeDump(` |
|         - | 3941 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 3942 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 3943 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 3944 | `	)` |
|         1 | 3945 | `{` |
|         - | 3946 | `	static const char zDump[] = {` |
|         - | 3947 | `		"====================================================\n"` |
|         - | 3948 | `		"PH7 VM Dump\n"` |
|         - | 3949 | `		"====================================================\n"` |
|         - | 3950 | `	};` |
|         - | 3951 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 3952 | `	sxi32 rc = SXRET_OK;` |
|         - | 3953 | `	sxu32 n;` |
|         - | 3954 | `	/* Point to the PH7 instructions */` |
|         3 | 3955 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 3956 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 3957 | `	n = 0;` |
|         3 | 3958 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 3959 | `	/* Dump instructions */` |
|         6 | 3960 | `	for(;;){` |
|        13 | 3961 | `		if( pInstr >= pEnd ){` |
|         - | 3962 | `			/* No more instructions */` |
|         3 | 3963 | `			break;` |
|         - | 3964 | `		}` |
|         - | 3965 | `		/* Format and call the consumer callback */` |
|        16 | 3966 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 | 3967 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 | 3968 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 | 3969 | `		if( rc != SXRET_OK ){` |
|         - | 3970 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 3971 | `			return rc;` |
|         - | 3972 | `		}` |
|        11 | 3973 | `		++n;` |
|        11 | 3974 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 3975 | `	}` |
|         3 | 3976 | `	return rc;` |
|         2 | 3977 | `}` |
|         - | 3978 | `/*` |
|         - | 3979 | ` * Save the execution state of a fiber/generator context.` |
|         - | 3980 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 3981 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 3982 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 3983 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 3984 | ` * when VmByteCodeExec returns.` |
|         - | 3985 | ` */` |
|      1692 | 3986 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 3987 | `	ph7_vm *pVm,` |
|         - | 3988 | `	ph7_exec_ctx *pCtx,` |
|         - | 3989 | `	sxi32 pc,` |
|         - | 3990 | `	sxi32 nTos` |
|         - | 3991 | `	)` |
|         5 | 3992 | `{` |
|       846 | 3993 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1697 | 3994 | `	pCtx->pc = pc;` |
|      1697 | 3995 | `	pCtx->nTos = nTos;` |
|      1697 | 3996 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1697 | 3997 | `	return PH7_SUSPEND;` |
|         5 | 3998 | `}` |
|         - | 3999 | `/*` |
|         - | 4000 | ` * Resolve named-argument mapping.` |
|         - | 4001 | ` *` |
|         - | 4002 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 4003 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 4004 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 4005 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 4006 | ` * every formal parameter that received a value.` |
|         - | 4007 | ` *` |
|         - | 4008 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 4009 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 4010 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 4011 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 4012 | ` */` |
|       360 | 4013 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 4014 | `	ph7_vm *pVm,` |
|         - | 4015 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 4016 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 4017 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 4018 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 4019 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 4020 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 4021 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 4022 | `)` |
|         5 | 4023 | `{` |
|       365 | 4024 | `	sxi32 posIdx = 0;` |
|         - | 4025 | `	sxu32 i;` |
|       365 | 4026 | `	int bSeenNamed = 0;` |
|         - | 4027 | `	char zErrMsg[256];` |
|       365 | 4028 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1293 | 4029 | `	for( i = 0; i < nActual; i++ ){` |
|       933 | 4030 | `		aSlot[i] = -2;` |
|       469 | 4031 | `	}` |
|      1281 | 4032 | `	for( i = 0; i < nActual; i++ ){` |
|      1210 | 4033 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 4034 | `			/* Named argument — find formal by name */` |
|       571 | 4035 | `			int found = 0;` |
|       571 | 4036 | `			bSeenNamed = 1;` |
|         - | 4037 | `			sxu32 k;` |
|       863 | 4038 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       700 | 4039 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       673 | 4040 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       636 | 4041 | `						pMap->aNames[i].zString,` |
|       954 | 4042 | `						pMap->aNames[i].nByte) == 0 ){` |
|       413 | 4043 | `					if( aUsed[k] ){` |
|        12 | 4044 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4045 | `							"Named parameter $%.*s overwrites previous argument",` |
|         6 | 4046 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         9 | 4047 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4048 | `					}` |
|       407 | 4049 | `					aSlot[i] = (sxi32)k;` |
|       407 | 4050 | `					aUsed[k] = 1;` |
|       407 | 4051 | `					found = 1;` |
|       407 | 4052 | `					break;` |
|         - | 4053 | `				}` |
|       150 | 4054 | `			}` |
|       565 | 4055 | `			if( !found ){` |
|       162 | 4056 | `				if( iVariadicIdx >= 0 ){` |
|       156 | 4057 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        80 | 4058 | `				}else{` |
|        11 | 4059 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4060 | `						"Unknown named parameter $%.*s",` |
|         6 | 4061 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         8 | 4062 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4063 | `				}` |
|        76 | 4064 | `			}` |
|       282 | 4065 | `		}else{` |
|         - | 4066 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 4067 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 4068 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 4069 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       366 | 4070 | `			if( bSeenNamed ){` |
|       ! 0 | 4071 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 4072 | `					"Cannot use positional argument after named argument",` |
|         - | 4073 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 4074 | `			}` |
|       366 | 4075 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        60 | 4076 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 4077 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4078 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 4079 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 4080 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4081 | `				}` |
|        60 | 4082 | `				aSlot[i] = posIdx;` |
|        60 | 4083 | `				aUsed[posIdx] = 1;` |
|       337 | 4084 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 4085 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 4086 | `			}` |
|       366 | 4087 | `			posIdx++;` |
|         - | 4088 | `		}` |
|       463 | 4089 | `	}` |
|       352 | 4090 | `	return SXRET_OK;` |
|       185 | 4091 | `}` |
|         - | 4092 | `/*` |
|         - | 4093 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 4094 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 4095 | ` */` |
|       326 | 4096 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         4 | 4097 | `{` |
|       330 | 4098 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       317 | 4099 | `		return 0;` |
|         - | 4100 | `	}` |
|        15 | 4101 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       167 | 4102 | `}` |
|         - | 4103 | `/*` |
|         - | 4104 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 4105 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 4106 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 4107 | ` */` |
|        10 | 4108 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4109 | `{` |
|        11 | 4110 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|         5 | 4111 | `	(void)pVm;` |
|        11 | 4112 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|        11 | 4113 | `	return SXRET_OK;` |
|         1 | 4114 | `}` |
|         - | 4115 | `/*` |
|         - | 4116 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 4117 | ` * collect values positionally (keys ignored) into a temp array.` |
|         - | 4118 | ` */` |
|         6 | 4119 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4120 | `{` |
|         3 | 4121 | `	(void)pVm; (void)pKey;` |
|         7 | 4122 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|         7 | 4123 | `	return SXRET_OK;` |
|         1 | 4124 | `}` |
|         - | 4125 | `/*` |
|         - | 4126 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 4127 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 4128 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 4129 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 4130 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 4131 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 4132 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 4133 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 4134 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 4135 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 4136 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 4137 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 4138 | ` */` |
|         - | 4139 | `/*` |
|         - | 4140 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 4141 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 4142 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 4143 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 4144 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 4145 | ` */` |
|       304 | 4146 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         3 | 4147 | `{` |
|         - | 4148 | `	VmSpreadRun sRun;` |
|         - | 4149 | `	ph7_hashmap_node *pNode;` |
|         - | 4150 | `	sxu32 i;` |
|       307 | 4151 | `	sRun.pStart = pFirst;` |
|       307 | 4152 | `	sRun.nCount = nCount;` |
|       307 | 4153 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|       307 | 4154 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       307 | 4155 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 4156 | `		return;` |
|         - | 4157 | `	}` |
|       307 | 4158 | `	pNode = pMap->pFirst;` |
|      2105 | 4159 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 4160 | `		VmSpreadKey sKey;` |
|      1801 | 4161 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 4162 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 4163 | `			 * the source map's release before CALL replays them. */` |
|        95 | 4164 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|        95 | 4165 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|        95 | 4166 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        48 | 4167 | `		}else{` |
|         - | 4168 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      1707 | 4169 | `			sKey.nOff = 0;` |
|      1707 | 4170 | `			sKey.nLen = 0;` |
|         - | 4171 | `		}` |
|      1801 | 4172 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      1801 | 4173 | `		pNode = pNode->pPrev; /* forward link */` |
|       902 | 4174 | `	}` |
|       155 | 4175 | `}` |
|         - | 4176 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 4177 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|         8 | 4178 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 4179 | `{` |
|         8 | 4180 | `	SySetReset(&pVm->aSpreadRun);` |
|         8 | 4181 | `	SySetReset(&pVm->aSpreadKey);` |
|         8 | 4182 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|         8 | 4183 | `}` |
|         - | 4184 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 4185 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 4186 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 4187 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 4188 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 4189 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 4190 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 4191 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 4192 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 4193 | ` * slot — from being consumed by that nested call. */` |
|       512 | 4194 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         3 | 4195 | `{` |
|       515 | 4196 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|       515 | 4197 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 4198 | `	VmSpreadRun *aRun;` |
|       515 | 4199 | `	if( rStart >= nRun ){` |
|       223 | 4200 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 4201 | `	}` |
|       295 | 4202 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       295 | 4203 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|       295 | 4204 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|       295 | 4205 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       146 | 4206 | `	}` |
|       295 | 4207 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|       259 | 4208 | `}` |
|         - | 4209 | `/*` |
|         - | 4210 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 4211 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 4212 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 4213 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 4214 | ` *` |
|         - | 4215 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 4216 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 4217 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 4218 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 4219 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 4220 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 4221 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 4222 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 4223 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 4224 | ` *` |
|         - | 4225 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 4226 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 4227 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 4228 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 4229 | ` */` |
|       320 | 4230 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         3 | 4231 | `{` |
|       323 | 4232 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4233 | `	VmSpreadRun *aRun;` |
|       323 | 4234 | `	ph7_value *pEnd = pTos;` |
|       323 | 4235 | `	sxi32 nPos = iP1;` |
|       323 | 4236 | `	sxi32 ri, extra = 0;` |
|       323 | 4237 | `	if( nRun == 0 ){` |
|        15 | 4238 | `		pVm->nSpreadCallBase = 0;` |
|        15 | 4239 | `		return 0;` |
|         - | 4240 | `	}` |
|       309 | 4241 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       309 | 4242 | `	ri = (sxi32)nRun - 1;` |
|       743 | 4243 | `	while( nPos > 0 ){` |
|       437 | 4244 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 4245 | `			/* A non-empty unpack occupying nCount slots. */` |
|       269 | 4246 | `			pEnd = aRun[ri].pStart;` |
|       269 | 4247 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|       269 | 4248 | `			ri--;` |
|       304 | 4249 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 4250 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|        41 | 4251 | `			extra -= 1;` |
|        41 | 4252 | `			ri--;` |
|        22 | 4253 | `		}else{` |
|         - | 4254 | `			/* An ordinary single-slot argument. */` |
|       133 | 4255 | `			pEnd--;` |
|         - | 4256 | `		}` |
|       437 | 4257 | `		nPos--;` |
|         3 | 4258 | `	}` |
|         - | 4259 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 4260 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|       309 | 4261 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|       309 | 4262 | `	return extra;` |
|       163 | 4263 | `}` |
|       304 | 4264 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)` |
|         3 | 4265 | `{` |
|       307 | 4266 | `	ph7_value *pTos = *ppTos;` |
|       307 | 4267 | `	sxu32 nEntry = pMap->nEntry;` |
|       307 | 4268 | `	if( nEntry == 0 ){` |
|         - | 4269 | `		/* Nothing to unpack — remove the source from the stack */` |
|        41 | 4270 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|        41 | 4271 | `		VmPopOperand(&pTos, 1);` |
|        22 | 4272 | `	}else{` |
|         - | 4273 | `		ph7_hashmap_node *pNode;` |
|         - | 4274 | `		ph7_value *pElem;` |
|         - | 4275 | `		sxu32 i;` |
|         - | 4276 | `		int bTemp;` |
|       269 | 4277 | `		pMap->iRef++;` |
|       269 | 4278 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 4279 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 4280 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|       269 | 4281 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 4282 | `		/* Overwrite the source slot with the first element */` |
|       269 | 4283 | `		pNode = pMap->pFirst;` |
|       269 | 4284 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|       269 | 4285 | `		PH7_MemObjRelease(pTos);` |
|       269 | 4286 | `		if( pElem ){` |
|       269 | 4287 | `			if( bTemp ){` |
|       137 | 4288 | `				PH7_MemObjStore(pElem, pTos);` |
|        69 | 4289 | `			}else{` |
|       133 | 4290 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 4291 | `			}` |
|       133 | 4292 | `		}` |
|       269 | 4293 | `		pTos->nIdx = SXU32_HIGH;` |
|         - | 4294 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 4295 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|       269 | 4296 | `		pNode = pNode->pPrev;` |
|         - | 4297 | `		/* Push the remaining elements */` |
|      1801 | 4298 | `		for( i = 1; i < nEntry; i++ ){` |
|      1535 | 4299 | `			pTos++;` |
|      1535 | 4300 | `			PH7_MemObjInit(pVm, pTos);` |
|      1535 | 4301 | `			pTos->nIdx = SXU32_HIGH;` |
|      1535 | 4302 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1535 | 4303 | `			if( pElem ){` |
|      1535 | 4304 | `				if( bTemp ){` |
|      1261 | 4305 | `					PH7_MemObjStore(pElem, pTos);` |
|       631 | 4306 | `				}else{` |
|       275 | 4307 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 4308 | `				}` |
|       766 | 4309 | `			}` |
|      1535 | 4310 | `			pNode = pNode->pPrev;` |
|       769 | 4311 | `		}` |
|       269 | 4312 | `		PH7_HashmapUnref(pMap);` |
|         - | 4313 | `	}` |
|       307 | 4314 | `	*ppTos = pTos;` |
|       307 | 4315 | `}` |
|         - | 4316 | `/*` |
|         - | 4317 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 4318 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 4319 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 4320 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 4321 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 4322 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 4323 | ` *` |
|         - | 4324 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 4325 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 4326 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 4327 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 4328 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 4329 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 4330 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 4331 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 4332 | ` * method-name slot pop shifts pArg).` |
|         - | 4333 | ` *` |
|         - | 4334 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 4335 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 4336 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 4337 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 4338 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 4339 | ` */` |
|       276 | 4340 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 4341 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         3 | 4342 | `{` |
|       279 | 4343 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4344 | `	VmSpreadRun *aRun;` |
|         - | 4345 | `	VmSpreadKey *aKey;` |
|         - | 4346 | `	const char *zKeyBase;` |
|       279 | 4347 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|       279 | 4348 | `	int bAnyNamed = 0;` |
|         - | 4349 | `	sxu32 ai, ci, ri, rStart;` |
|       279 | 4350 | `	if( nRun == 0 ){` |
|         - | 4351 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 4352 | `		return 0;` |
|         - | 4353 | `	}` |
|       279 | 4354 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       279 | 4355 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|       279 | 4356 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 4357 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 4358 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 4359 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 4360 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|       279 | 4361 | `	ri = pVm->nSpreadCallBase;` |
|       279 | 4362 | `	rStart = ri;` |
|       279 | 4363 | `	if( rStart >= nRun ){` |
|         - | 4364 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 4365 | `		return 0;` |
|         - | 4366 | `	}` |
|       279 | 4367 | `	SySetReset(&pVm->aEffArgName);` |
|       279 | 4368 | `	ci = 0;` |
|       279 | 4369 | `	ai = 0;` |
|       653 | 4370 | `	while( ai < nActual ){` |
|         - | 4371 | `		SyString sName;` |
|       377 | 4372 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 4373 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 4374 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|       389 | 4375 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 4376 | `			ci++; ri++;` |
|         1 | 4377 | `		}` |
|       377 | 4378 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 4379 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 4380 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 4381 | `			 * run never desyncs the key stream. */` |
|       267 | 4382 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      2061 | 4383 | `			for( j = 0; j < K; j++ ){` |
|      1797 | 4384 | `				SyZero(&sName, sizeof(sName));` |
|      1797 | 4385 | `				if( aKey[ks + j].nLen > 0 ){` |
|        95 | 4386 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|        95 | 4387 | `					bAnyNamed = 1;` |
|        47 | 4388 | `				}` |
|      1797 | 4389 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       900 | 4390 | `			}` |
|       267 | 4391 | `			ai += K;` |
|       267 | 4392 | `			ci++; ri++;` |
|       135 | 4393 | `		}else{` |
|         - | 4394 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|       113 | 4395 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        33 | 4396 | `				sName = pCompile->aNames[ci];` |
|        33 | 4397 | `				bAnyNamed = 1;` |
|        16 | 4398 | `			}` |
|       113 | 4399 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       113 | 4400 | `			ai++;` |
|       113 | 4401 | `			ci++;` |
|         - | 4402 | `		}` |
|         3 | 4403 | `	}` |
|         - | 4404 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 4405 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 4406 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 4407 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|       279 | 4408 | `	VmSpreadConsume(pVm);` |
|       279 | 4409 | `	if( !bAnyNamed ){` |
|         - | 4410 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|       209 | 4411 | `		return 0;` |
|         - | 4412 | `	}` |
|        71 | 4413 | `	pEff->bHasNamed = 1;` |
|        71 | 4414 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|        71 | 4415 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|        71 | 4416 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|        71 | 4417 | `	if( pCompile ){` |
|        33 | 4418 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        17 | 4419 | `	}else{` |
|        39 | 4420 | `		pEff->sAssertSrc.zString = 0;` |
|        39 | 4421 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 4422 | `	}` |
|        71 | 4423 | `	pEff->nTotal = nActual;` |
|        71 | 4424 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|        71 | 4425 | `	return 1;` |
|       141 | 4426 | `}` |
|         - | 4427 | `/*` |
|         - | 4428 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 4429 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 4430 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 4431 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 4432 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 4433 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 4434 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 4435 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 4436 | ` * pArg must be the site's FINAL argument base.` |
|         - | 4437 | ` */` |
|   7781767 | 4438 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 4439 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 4440 | `{` |
|   7781772 | 4441 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   7781772 | 4442 | `	if( pInstr->iP2 == 0 ){` |
|   7781468 | 4443 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 4444 | `	}` |
|       307 | 4445 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|        71 | 4446 | `		return pStorage;` |
|         - | 4447 | `	}` |
|       237 | 4448 | `	VmSpreadConsume(pVm);` |
|       237 | 4449 | `	return pCompile;` |
|   3891830 | 4450 | `}` |
|         - | 4451 | `/*` |
|         - | 4452 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 4453 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 4454 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 4455 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 4456 | ` */` |
|        14 | 4457 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         2 | 4458 | `{` |
|        16 | 4459 | `	const char *zType = "unknown";` |
|         - | 4460 | `	char zMsg[64];` |
|        16 | 4461 | `	if( iFlags & MEMOBJ_STRING ){` |
|         6 | 4462 | `		zType = "string";` |
|        13 | 4463 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 4464 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 4465 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 4466 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 4467 | `		zType = "float";` |
|        11 | 4468 | `	}else if( iFlags & MEMOBJ_INT ){` |
|         9 | 4469 | `		zType = "int";` |
|         7 | 4470 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 4471 | `		zType = "bool";` |
|         1 | 4472 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 4473 | `		zType = "object";` |
|       ! 0 | 4474 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 4475 | `		zType = "resource";` |
|       ! 0 | 4476 | `	}` |
|        16 | 4477 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        16 | 4478 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        16 | 4479 | `}` |
|         - | 4480 | `/*` |
|         - | 4481 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - | 4482 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - | 4483 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - | 4484 | ` * per context — see VmMemberCtxWantsValue.` |
|         - | 4485 | ` */` |
|      1348 | 4486 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 4487 | `{` |
|      1353 | 4488 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 | 4489 | `}` |
|         - | 4490 | `/*` |
|         - | 4491 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - | 4492 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - | 4493 | ` * isset() stops at the truth.` |
|         - | 4494 | ` */` |
|        38 | 4495 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 | 4496 | `{` |
|        41 | 4497 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 | 4498 | `}` |
|         - | 4499 | `/*` |
|         - | 4500 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 4501 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 4502 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 4503 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 4504 | ` * reads (other names / other instances) still dispatch.` |
|         - | 4505 | ` */` |
|      1306 | 4506 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4507 | `{` |
|         - | 4508 | `	VmMagicGuard *aG;` |
|         - | 4509 | `	sxu32 nHash;` |
|         - | 4510 | `	sxu32 n;` |
|      1311 | 4511 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 4512 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 4513 | `		 * every hooked-property access consults the guard, often twice. */` |
|      1129 | 4514 | `		return FALSE;` |
|         - | 4515 | `	}` |
|       184 | 4516 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       184 | 4517 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       228 | 4518 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       184 | 4519 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       140 | 4520 | `			return TRUE;` |
|         - | 4521 | `		}` |
|        23 | 4522 | `	}` |
|        45 | 4523 | `	return FALSE;` |
|       658 | 4524 | `}` |
|       606 | 4525 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4526 | `{` |
|         - | 4527 | `	VmMagicGuard sG;` |
|       611 | 4528 | `	sG.pThis = pThis;` |
|       611 | 4529 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       611 | 4530 | `	sG.cKind = cKind;` |
|       611 | 4531 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|       611 | 4532 | `}` |
|       606 | 4533 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         5 | 4534 | `{` |
|       611 | 4535 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|       611 | 4536 | `}` |
|         - | 4537 | `/*` |
|         - | 4538 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 4539 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 4540 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 4541 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 4542 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 4543 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 4544 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 4545 | ` * One-token lookahead only.` |
|         - | 4546 | ` */` |
|       916 | 4547 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 | 4548 | `{` |
|       921 | 4549 | `	switch( pNext->iOp ){` |
|        22 | 4550 | `		case PH7_OP_STORE:` |
|        48 | 4551 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|       ! 0 | 4552 | `		case PH7_OP_STORE_REF:` |
|       ! 0 | 4553 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|        11 | 4554 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 4555 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4556 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4557 | `		case PH7_OP_CAT_STORE:` |
|         - | 4558 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4559 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|        23 | 4560 | `			return 1;` |
|       425 | 4561 | `		default:` |
|       855 | 4562 | `			return 0;` |
|         - | 4563 | `	}` |
|       463 | 4564 | `}` |
|         - | 4565 | `/*` |
|         - | 4566 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 4567 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 4568 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 4569 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 4570 | ` */` |
|       516 | 4571 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         5 | 4572 | `{` |
|       521 | 4573 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         5 | 4574 | `}` |
|         - | 4575 | `/*` |
|         - | 4576 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 4577 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 4578 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 4579 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 4580 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 4581 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 4582 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 4583 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 4584 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 4585 | ` * abort path; SXRET_OK otherwise.` |
|         - | 4586 | ` */` |
|        76 | 4587 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         3 | 4588 | `{` |
|         - | 4589 | `	char zHName[384];` |
|         - | 4590 | `	sxu32 nHName;` |
|         - | 4591 | `	ph7_class_method *pSetHook;` |
|        79 | 4592 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 4593 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 4594 | `		SyBlob sErrMsg;` |
|         5 | 4595 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 4596 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 4597 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 4598 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 4599 | `		return SXRET_OK;` |
|         - | 4600 | `	}` |
|        75 | 4601 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 4602 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 4603 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 4604 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 4605 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 4606 | `			return SXRET_OK;` |
|         - | 4607 | `		}` |
|       ! 0 | 4608 | `	}` |
|        75 | 4609 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        75 | 4610 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        75 | 4611 | `	if( pSetHook ){` |
|         - | 4612 | `		ph7_value sHookRet;` |
|         - | 4613 | `		ph7_value *apHArg[1];` |
|        75 | 4614 | `		apHArg[0] = pValue;` |
|        75 | 4615 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        75 | 4616 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        75 | 4617 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        75 | 4618 | `		VmMagicGuardPop(pVm);` |
|        72 | 4619 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        41 | 4620 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         6 | 4621 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         6 | 4622 | `			if( rcH == SXRET_OK ){` |
|         6 | 4623 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|         6 | 4624 | `				if( pBack ){` |
|         6 | 4625 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         4 | 4626 | `				}` |
|         2 | 4627 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 4628 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 4629 | `				return PH7_ABORT;` |
|         - | 4630 | `			}` |
|         - | 4631 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 4632 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 4633 | `			 * parked throw. */` |
|         2 | 4634 | `		}` |
|        75 | 4635 | `		PH7_MemObjRelease(&sHookRet);` |
|        36 | 4636 | `	}` |
|        75 | 4637 | `	return SXRET_OK;` |
|        41 | 4638 | `}` |
|         - | 4639 | `/*` |
|         - | 4640 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 4641 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 4642 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 4643 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 4644 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 4645 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 4646 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 4647 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 4648 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 4649 | ` */` |
|         - | 4650 | `/*` |
|         - | 4651 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 4652 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 4653 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 4654 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 4655 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 4656 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 4657 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 4658 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 4659 | ` * caller reads the raw slot then.` |
|         - | 4660 | ` */` |
|       428 | 4661 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 | 4662 | `{` |
|       433 | 4663 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 4664 | `	char zHName[384];` |
|         - | 4665 | `	sxu32 nHName;` |
|         - | 4666 | `	ph7_class_method *pGetHook;` |
|         - | 4667 | `	sxi32 rc;` |
|       428 | 4668 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       300 | 4669 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       177 | 4670 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 4671 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 4672 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 4673 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 4674 | `		 * raw values whose output the routed throw then discards. */` |
|       261 | 4675 | `		return SXERR_NOTFOUND;` |
|         - | 4676 | `	}` |
|       177 | 4677 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       177 | 4678 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       177 | 4679 | `	if( pGetHook == 0 ){` |
|       ! 0 | 4680 | `		return SXERR_NOTFOUND;` |
|         - | 4681 | `	}` |
|       177 | 4682 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       177 | 4683 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       177 | 4684 | `	VmMagicGuardPop(pVm);` |
|       177 | 4685 | `	return rc;` |
|       219 | 4686 | `}` |
|         - | 4687 | `/*` |
|         - | 4688 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 4689 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 4690 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 4691 | ` */` |
|        20 | 4692 | `static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 4693 | `{` |
|        21 | 4694 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 4695 | `	VmSlot sFree;` |
|        21 | 4696 | `	if( pScr ){` |
|        21 | 4697 | `		PH7_MemObjRelease(pScr);` |
|        10 | 4698 | `	}` |
|        21 | 4699 | `	sFree.nIdx = nIdx;` |
|        21 | 4700 | `	sFree.pUserData = 0;` |
|        21 | 4701 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|        21 | 4702 | `}` |
|         - | 4703 | `/*` |
|         - | 4704 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 4705 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 4706 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 4707 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 4708 | ` * instance reference.` |
|         - | 4709 | ` */` |
|        18 | 4710 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 4711 | `{` |
|        20 | 4712 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        20 | 4713 | `	if( pEnt == 0 ){` |
|         5 | 4714 | `		return;` |
|         - | 4715 | `	}` |
|        15 | 4716 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|         7 | 4717 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         3 | 4718 | `	}` |
|        15 | 4719 | `	SyBlobRelease(&pEnt->sName);` |
|        15 | 4720 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        15 | 4721 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        11 | 4722 | `}` |
|        14 | 4723 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 4724 | `{` |
|         - | 4725 | `	VmHookRmw sEnt;` |
|         - | 4726 | `	VmHookRmw *pEnt;` |
|         - | 4727 | `	ph7_value *pScr;` |
|         - | 4728 | `	ph7_value sVal;` |
|        15 | 4729 | `	sxi32 rc = SXRET_OK;` |
|        15 | 4730 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        15 | 4731 | `	if( pEnt == 0 \|\| pEnt->iKind != VM_HOOK_PEND_RMW \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 4732 | `		return SXERR_NOTFOUND;` |
|         - | 4733 | `	}` |
|        15 | 4734 | `	sEnt = *pEnt;` |
|        15 | 4735 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 4736 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|         - | 4737 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|         - | 4738 | `	 * scratch index past this point). */` |
|        15 | 4739 | `	PH7_MemObjInit(pVm,&sVal);` |
|        15 | 4740 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        15 | 4741 | `	if( pScr ){` |
|        15 | 4742 | `		PH7_MemObjStore(pScr,&sVal);` |
|         7 | 4743 | `	}` |
|        15 | 4744 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        15 | 4745 | `	sVal.nIdx = SXU32_HIGH;` |
|        15 | 4746 | `	if( pVm->nBoundaryRc == 0 ){` |
|        15 | 4747 | `		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         7 | 4748 | `	}` |
|        15 | 4749 | `	PH7_MemObjRelease(&sVal);` |
|        15 | 4750 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        15 | 4751 | `	return rc;` |
|         8 | 4752 | `}` |
|         - | 4753 | `/*` |
|         - | 4754 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 4755 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 4756 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 4757 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 4758 | ` */` |
|        16 | 4759 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         3 | 4760 | `{` |
|        19 | 4761 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|        19 | 4762 | `	if( pSetMeth ){` |
|         - | 4763 | `		ph7_value sNameVal;` |
|         - | 4764 | `		ph7_value *apSetArg[2];` |
|        19 | 4765 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|        19 | 4766 | `		sNameVal.nIdx = SXU32_HIGH;` |
|        19 | 4767 | `		apSetArg[0] = &sNameVal;` |
|        19 | 4768 | `		apSetArg[1] = pValue;` |
|        19 | 4769 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|        19 | 4770 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|        19 | 4771 | `		VmMagicGuardPop(pVm);` |
|        19 | 4772 | `		PH7_MemObjRelease(&sNameVal);` |
|         8 | 4773 | `	}` |
|        19 | 4774 | `}` |
|         - | 4775 | `/*` |
|         - | 4776 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 4777 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 4778 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 4779 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 4780 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 4781 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 4782 | ` * path (the SyHash-layout incident class).` |
|         - | 4783 | ` */` |
|         - | 4784 | `/*` |
|         - | 4785 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 4786 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 4787 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 4788 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 4789 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 4790 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 4791 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 4792 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 4793 | ` * never linked (INIT error path).` |
|         - | 4794 | ` */` |
|     27252 | 4795 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 4796 | `{` |
|     27257 | 4797 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     27257 | 4798 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 4799 | `	sxu32 i;` |
|     29537 | 4800 | `	for( i = 0 ; i < n ; ++i ){` |
|     29537 | 4801 | `		if( apStep[i] == pStep ){` |
|     27265 | 4802 | `			for( ; i + 1 < n ; ++i ){` |
|         9 | 4803 | `				apStep[i] = apStep[i + 1];` |
|         5 | 4804 | `			}` |
|     27257 | 4805 | `			(void)SySetPop(&pInfo->aStep);` |
|     27257 | 4806 | `			return;` |
|         - | 4807 | `		}` |
|      1145 | 4808 | `	}` |
|     13631 | 4809 | `}` |
|       246 | 4810 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 4811 | `{` |
|       251 | 4812 | `	if( pStep->pOwner ){` |
|        24 | 4813 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|        11 | 4814 | `	}` |
|       251 | 4815 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       251 | 4816 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       251 | 4817 | `	PH7_ClassInstanceUnref(pThis);` |
|       251 | 4818 | `}` |
|         - | 4819 | `/*` |
|         - | 4820 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 4821 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 4822 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 4823 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 4824 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 4825 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 4826 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 4827 | ` * map's last reference.` |
|         - | 4828 | ` */` |
|     26978 | 4829 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 4830 | `{` |
|     26983 | 4831 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     26983 | 4832 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     26983 | 4833 | `	if( bPop ){` |
|         - | 4834 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 4835 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     26983 | 4836 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     13489 | 4837 | `	}` |
|     26983 | 4838 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     26983 | 4839 | `	PH7_HashmapUnref(pMap);` |
|     26983 | 4840 | `}` |
|         - | 4841 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 4842 | `/*` |
|         - | 4843 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 4844 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 4845 | ` * See block-comment on that function for additional information.` |
|         - | 4846 | ` */` |
|   7501796 | 4847 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 4848 | `{` |
|         - | 4849 | `	ph7_value *pStack;` |
|         - | 4850 | `	sxu32 nCap;` |
|         - | 4851 | `	sxi32 rc;` |
|         - | 4852 | `	/* Allocate a new operand stack */` |
|   7501801 | 4853 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   7501801 | 4854 | `	if( pStack == 0 ){` |
|       ! 0 | 4855 | `		return SXERR_MEM;` |
|         - | 4856 | `	}` |
|   7501801 | 4857 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 4858 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 4859 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   7501801 | 4860 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 4861 | `	/* Free the operand stack */` |
|   7501801 | 4862 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 4863 | `	/* Execution result */` |
|   7501801 | 4864 | `	return rc;` |
|   3750903 | 4865 | `}` |
|         - | 4866 | `/*` |
|         - | 4867 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - | 4868 | ` * statement must honour?` |
|         - | 4869 | ` *` |
|         - | 4870 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - | 4871 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - | 4872 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - | 4873 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - | 4874 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - | 4875 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - | 4876 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - | 4877 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - | 4878 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - | 4879 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - | 4880 | ` * drift apart.` |
|         - | 4881 | ` *` |
|         - | 4882 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - | 4883 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - | 4884 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - | 4885 | ` * this opcode did not create as its own throw.` |
|         - | 4886 | ` *` |
|         - | 4887 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - | 4888 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - | 4889 | ` */` |
|         - | 4890 | `/*` |
|         - | 4891 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - | 4892 | ` *` |
|         - | 4893 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - | 4894 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - | 4895 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - | 4896 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - | 4897 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - | 4898 | ` * captured value writes over the superglobal's own slot: calling` |
|         - | 4899 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - | 4900 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - | 4901 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - | 4902 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - | 4903 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - | 4904 | ` *` |
|         - | 4905 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - | 4906 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - | 4907 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - | 4908 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - | 4909 | ` */` |
|       820 | 4910 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 | 4911 | `{` |
|         - | 4912 | `	static const char *const azAuto[] = {` |
|         - | 4913 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - | 4914 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - | 4915 | `	};` |
|         - | 4916 | `	sxu32 n;` |
|      8947 | 4917 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|      8135 | 4918 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|      8135 | 4919 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|         9 | 4920 | `			return 1;` |
|         - | 4921 | `		}` |
|      4066 | 4922 | `	}` |
|       817 | 4923 | `	return 0;` |
|       415 | 4924 | `}` |
|      1688 | 4925 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 | 4926 | `{` |
|      1681 | 4927 | `	return rc == PH7_EXCEPTION` |
|      1676 | 4928 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|      2520 | 4929 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 | 4930 | `}` |
|         - | 4931 | `/*` |
|         - | 4932 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 4933 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 4934 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 4935 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 4936 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 4937 | ` */` |
|        54 | 4938 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         1 | 4939 | `{` |
|        55 | 4940 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        55 | 4941 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 4942 | `	sxi32 rc;` |
|        55 | 4943 | `	if( pDeclCls ){` |
|        39 | 4944 | `		pVm->pConstEvalClass = pDeclCls;` |
|        39 | 4945 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        19 | 4946 | `	}` |
|        55 | 4947 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|        55 | 4948 | `	pVm->pConstEvalClass = pSaveCtx;` |
|        55 | 4949 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|        55 | 4950 | `	return rc;` |
|         1 | 4951 | `}` |
|         - | 4952 | `/*` |
|         - | 4953 | ` * Invoke any installed shutdown callbacks.` |
|         - | 4954 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|         - | 4955 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|         - | 4956 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|         - | 4957 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|         - | 4958 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|         - | 4959 | ` *` |
|         - | 4960 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|         - | 4961 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|         - | 4962 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|         - | 4963 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|         - | 4964 | ` * down and restore the default consumer.` |
|         - | 4965 | ` */` |
|      3958 | 4966 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 4967 | `{` |
|      3963 | 4968 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         - | 4969 | `	sxu32 n,nUsed;` |
|      3963 | 4970 | `	nUsed = SySetUsed(&pVm->aOB);` |
|      3963 | 4971 | `	if( nUsed < 1 ){` |
|      3961 | 4972 | `		return;` |
|         - | 4973 | `	}` |
|         7 | 4974 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|         5 | 4975 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|         5 | 4976 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|         5 | 4977 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|         5 | 4978 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|         2 | 4979 | `		}` |
|         3 | 4980 | `	}` |
|         - | 4981 | `	/* Restore the default consumer and release the buffers. */` |
|         3 | 4982 | `	pCons->xConsumer = pCons->xDef;` |
|         3 | 4983 | `	pCons->pUserData = pCons->pDefData;` |
|         7 | 4984 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|         5 | 4985 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|         5 | 4986 | `		if( pOb ){` |
|         5 | 4987 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|         5 | 4988 | `			SyBlobRelease(&pOb->sOB);` |
|         2 | 4989 | `		}` |
|         3 | 4990 | `	}` |
|         3 | 4991 | `	SySetReset(&pVm->aOB);` |
|         3 | 4992 | `	pVm->nObDepth = 0;` |
|      1984 | 4993 | `}` |
|         - | 4994 | `/*` |
|         - | 4995 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 4996 | ` * or more calls to [register_shutdown_function()].` |
|         - | 4997 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 4998 | ` * execution ends.` |
|         - | 4999 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 5000 | ` * additional information.` |
|         - | 5001 | ` */` |
|      3958 | 5002 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 5003 | `{` |
|         - | 5004 | `	VmShutdownCB *pEntry;` |
|         - | 5005 | `	ph7_value *apArg[10];` |
|         - | 5006 | `	sxu32 n,nEntry;` |
|         - | 5007 | `	int i;` |
|         - | 5008 | `	/* Point to the stack of registered callbacks */` |
|      3963 | 5009 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     43543 | 5010 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     39585 | 5011 | `		apArg[i] = 0;` |
|     19795 | 5012 | `	}` |
|         - | 5013 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 5014 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 5015 | `	 * callbacks, mirroring PHP.` |
|         - | 5016 | `	 */` |
|      3963 | 5017 | `	pVm->bHaltRequested = 0;` |
|      3981 | 5018 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        23 | 5019 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        23 | 5020 | `		if( pEntry ){` |
|         - | 5021 | `			/* Prepare callback arguments if any */` |
|        23 | 5022 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 5023 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 5024 | `					break;` |
|         - | 5025 | `				}` |
|       ! 0 | 5026 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 5027 | `			}` |
|         - | 5028 | `			/* Invoke the callback */` |
|        23 | 5029 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 5030 | `			/*` |
|         - | 5031 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 5032 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 5033 | `			 */` |
|        23 | 5034 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        23 | 5035 | `			if( pEntry ){` |
|        23 | 5036 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        23 | 5037 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 5038 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 5039 | `				}` |
|         9 | 5040 | `			}` |
|        23 | 5041 | `			if( pVm->bHaltRequested ){` |
|         - | 5042 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|       ! 0 | 5043 | `				break;` |
|         - | 5044 | `			}` |
|         9 | 5045 | `		}` |
|        14 | 5046 | `	}` |
|      3963 | 5047 | `	SySetReset(&pVm->aShutdown);` |
|      3963 | 5048 | `}` |
|         - | 5049 | `/*` |
|         - | 5050 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 5051 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5052 | ` * See block-comment on that function for additional information.` |
|         - | 5053 | ` */` |
|      3958 | 5054 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 5055 | `{` |
|         - | 5056 | `	/* Make sure we are ready to execute this program */` |
|      3963 | 5057 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 5058 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 5059 | `	}` |
|         - | 5060 | `	/* Set the execution magic number  */` |
|      3963 | 5061 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 5062 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 5063 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 5064 | `	{` |
|      3963 | 5065 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|      3963 | 5066 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 5067 | `	}` |
|         - | 5068 | `	/* Invoke any shutdown callbacks */` |
|      3963 | 5069 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 5070 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 5071 | `	 * shutdown callbacks, which may still write into them. */` |
|      3963 | 5072 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 5073 | `	/*` |
|         - | 5074 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 5075 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 5076 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 5077 | `	 */` |
|      3963 | 5078 | `	return SXRET_OK;` |
|      1984 | 5079 | `}` |
|         - | 5080 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 5081 | `/*` |
|         - | 5082 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 5083 | ` * the desired message.` |
|         - | 5084 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 5085 | ` * in 'api.c' for additional information.` |
|         - | 5086 | ` */` |
|      8170 | 5087 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 5088 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 5089 | `	SyString *pString /* Message to output */` |
|         - | 5090 | `	)` |
|         5 | 5091 | `{` |
|      8175 | 5092 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      8175 | 5093 | `	sxi32 rc = SXRET_OK;` |
|         - | 5094 | `	/* Call the output consumer */` |
|      8175 | 5095 | `	if( pString->nByte > 0 ){` |
|      8175 | 5096 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      8175 | 5097 | `		VmTrackOutput(pVm, pString->nByte);` |
|      4085 | 5098 | `	}` |
|      8175 | 5099 | `	return rc;` |
|         5 | 5100 | `}` |
|         - | 5101 | `/*` |
|         - | 5102 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 5103 | ` * callback to consume the formatted message.` |
|         - | 5104 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 5105 | ` * in 'api.c' for additional information.` |
|         - | 5106 | ` */` |
|         2 | 5107 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 5108 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 5109 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 5110 | `	va_list ap           /* Variable list of arguments */` |
|         - | 5111 | `	)` |
|         1 | 5112 | `{` |
|         3 | 5113 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         3 | 5114 | `	sxi32 rc = SXRET_OK;` |
|         - | 5115 | `	SyBlob sWorker;` |
|         - | 5116 | `	/* Format the message and call the output consumer */` |
|         3 | 5117 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|         3 | 5118 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|         3 | 5119 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 5120 | `		/* Consume the formatted message */` |
|         3 | 5121 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|         1 | 5122 | `	}` |
|         3 | 5123 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 5124 | `	/* Release the working buffer */` |
|         3 | 5125 | `	SyBlobRelease(&sWorker);` |
|         3 | 5126 | `	return rc;` |
|         1 | 5127 | `}` |
|         - | 5128 | `/*` |
|         - | 5129 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 5130 | ` * This function never fail and always return a pointer` |
|         - | 5131 | ` * to a null terminated string.` |
|         - | 5132 | ` */` |
|        10 | 5133 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 5134 | `{` |
|        11 | 5135 | `	const char *zOp = "Unknown     ";` |
|        11 | 5136 | `	switch(nOp){` |
|         3 | 5137 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 5138 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 5139 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 5140 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 5141 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 5142 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 5143 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 5144 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 5145 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 5146 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 5147 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 5148 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 5149 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 5150 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 5151 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 5152 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 5153 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 5154 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 5155 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 5156 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 5157 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 5158 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 5159 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 5160 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 5161 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 5162 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 5163 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 5164 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 5165 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 5166 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 5167 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 5168 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 5169 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 5170 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 5171 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 5172 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 5173 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 5174 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 5175 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 5176 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 5177 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 5178 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 5179 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 5180 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 5181 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 5182 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 5183 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 5184 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 5185 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 5186 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 5187 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 5188 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 5189 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 | 5190 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 5191 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 5192 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 5193 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 5194 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 5195 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 5196 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 5197 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 5198 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 5199 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 5200 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 5201 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 5202 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 5203 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 5204 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 5205 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 5206 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 5207 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 5208 | `	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;` |
|       ! 0 | 5209 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 5210 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 5211 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 5212 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 5213 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 5214 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 5215 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 5216 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 5217 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 5218 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 5219 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 5220 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 5221 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 5222 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 5223 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 5224 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 5225 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 5226 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 5227 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 5228 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 5229 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 5230 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 5231 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 5232 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 5233 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 5234 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 5235 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 5236 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 5237 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 5238 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 5239 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 5240 | `	default:` |
|       ! 0 | 5241 | `		break;` |
|         - | 5242 | `	}` |
|        11 | 5243 | `	return zOp;` |
|         1 | 5244 | `}` |
|         - | 5245 | `/*` |
|         - | 5246 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 5247 | ` * The xConsumer() callback which is an used defined function` |
|         - | 5248 | ` * is responsible of consuming the generated dump.` |
|         - | 5249 | ` */` |
|         2 | 5250 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 5251 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 5252 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 5253 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 5254 | `	)` |
|         1 | 5255 | `{` |
|         - | 5256 | `	sxi32 rc;` |
|         3 | 5257 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 5258 | `	return rc;` |
|         1 | 5259 | `}` |
|         - | 5260 | `/*` |
|         - | 5261 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 5262 | ` * outside a class body [i.e: global or function scope].` |
|         - | 5263 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 5264 | ` * in 'compile.c' for additional information.` |
|         - | 5265 | ` */` |
|    100174 | 5266 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         5 | 5267 | `{` |
|    100179 | 5268 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 5269 | `	/* Evaluate and expand constant value */` |
|    100179 | 5270 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|    100179 | 5271 | `}` |
|         - | 5272 | `/*` |
|         - | 5273 | ` * Section:` |
|         - | 5274 | ` *  Function handling functions.` |
|         - | 5275 | ` * Status:` |
|         - | 5276 | ` *    Stable.` |
|         - | 5277 | ` */` |
|         - | 5278 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 5279 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 5280 | `	{ "__phl_magic_call", vm_builtin_magic_call },` |
|         - | 5281 | `	{ "__phl_enum_cases",   vm_builtin_enum_cases },` |
|         - | 5282 | `	{ "__phl_enum_from",    vm_builtin_enum_from },` |
|         - | 5283 | `	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },` |
|         - | 5284 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 5285 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 5286 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 5287 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 5288 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 5289 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 5290 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 5291 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 5292 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 5293 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 5294 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 5295 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 5296 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 5297 | `	    /* Constants management */` |
|         - | 5298 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 5299 | `	{ "define",   vm_builtin_define               },` |
|         - | 5300 | `	{ "constant", vm_builtin_constant             },` |
|         - | 5301 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 5302 | `	   /* Class/Object functions */` |
|         - | 5303 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 5304 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 5305 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 5306 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 5307 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 5308 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 5309 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 5310 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 5311 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 5312 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 5313 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 5314 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 5315 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 5316 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 5317 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 5318 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 5319 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 5320 | `	   /* SPL object identity */` |
|         - | 5321 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 5322 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 5323 | `	   /* SPL Autoloading */` |
|         - | 5324 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 5325 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 5326 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 5327 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 5328 | `	   /* Random numbers/strings generators */` |
|         - | 5329 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 5330 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 5331 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 5332 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 5333 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 5334 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 5335 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 5336 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5337 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 5338 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 5339 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 5340 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5341 | `	   /* Language constructs functions */` |
|         - | 5342 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 5343 | `	{ "print", vm_builtin_print                   },` |
|         - | 5344 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 5345 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 5346 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 5347 | `	  /* Variable handling functions */` |
|         - | 5348 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 5349 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 5350 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 5351 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 5352 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 5353 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 5354 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 5355 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 5356 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 5357 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 5358 | `	  /* Ouput control functions */` |
|         - | 5359 | `	{ "flush",        vm_builtin_ob_flush          },` |
|         - | 5360 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 5361 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 5362 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 5363 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 5364 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 5365 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 5366 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|         - | 5367 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 5368 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 5369 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 5370 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|         - | 5371 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 5372 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 5373 | `	  /* Assertion functions */` |
|         - | 5374 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 5375 | `	  /* Error reporting functions */` |
|         - | 5376 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 5377 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 5378 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 5379 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 5380 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 5381 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 5382 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 5383 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 5384 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 5385 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 5386 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 5387 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 5388 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 5389 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 5390 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 5391 | `	  /* Release info */` |
|         - | 5392 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 5393 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 5394 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 5395 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 5396 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 5397 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 5398 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 5399 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 5400 | `	  /* hashmap */` |
|         - | 5401 | `	{"compact",          vm_builtin_compact       },` |
|         - | 5402 | `	{"extract",          vm_builtin_extract       },` |
|         - | 5403 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 5404 | `	  /* URL related function */` |
|         - | 5405 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 5406 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 5407 | `	   /* Command line processing */` |
|         - | 5408 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 5409 | `	   /* JSON encoding/decoding */` |
|         - | 5410 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 5411 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 5412 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 5413 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 5414 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 5415 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 5416 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 5417 | `	   /* Files/URI inclusion facility */` |
|         - | 5418 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 5419 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 5420 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 5421 | `	{ "include",      vm_builtin_include          },` |
|         - | 5422 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 5423 | `	{ "require",      vm_builtin_require          },` |
|         - | 5424 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 5425 | `};` |
|         - | 5426 | `/*` |
|         - | 5427 | ` * Register the built-in VM functions defined above.` |
|         - | 5428 | ` */` |
|      3956 | 5429 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 5430 | `{` |
|         - | 5431 | `	sxi32 rc;` |
|         - | 5432 | `	sxu32 n;` |
|    494505 | 5433 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 5434 | `		/* Note that these special functions have access` |
|         - | 5435 | `		 * to the underlying virtual machine as their` |
|         - | 5436 | `		 * private data.` |
|         - | 5437 | `		 */` |
|    490549 | 5438 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    490549 | 5439 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 5440 | `			return rc;` |
|         - | 5441 | `		}` |
|    245277 | 5442 | `	}` |
|      3961 | 5443 | `	return SXRET_OK;` |
|      1983 | 5444 | `}` |
|         - | 5445 | `/*` |
|         - | 5446 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 5447 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 5448 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 5449 | ` */` |
|   7316076 | 5450 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 5451 | `{` |
|   7316081 | 5452 | `	if( !iLoadable ){` |
|   5760501 | 5453 | `		return pClass;` |
|         - | 5454 | `	}` |
|   1555589 | 5455 | `	while(pClass){` |
|   1555585 | 5456 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   1555581 | 5457 | `			return pClass;` |
|         - | 5458 | `		}` |
|         5 | 5459 | `		pClass = pClass->pNextName;` |
|         1 | 5460 | `	}` |
|         5 | 5461 | `	return 0;` |
|   3658043 | 5462 | `}` |
|         - | 5463 | `/*` |
|         - | 5464 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 5465 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 5466 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 5467 | ` * registered in the VM's class table.` |
|         - | 5468 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 5469 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 5470 | ` */` |
|       340 | 5471 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5472 | `{` |
|         - | 5473 | `	VmAutoloadCB *pEntry;` |
|         - | 5474 | `	ph7_value sArg,sResult;` |
|         - | 5475 | `	SyHashEntry *pHashEntry;` |
|         - | 5476 | `	ph7_class *pClass;` |
|         - | 5477 | `	sxu32 n,nEntry;` |
|       345 | 5478 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       345 | 5479 | `	if( nEntry < 1 ){` |
|       245 | 5480 | `		return 0;` |
|         - | 5481 | `	}` |
|         - | 5482 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       105 | 5483 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 5484 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 5485 | `	}` |
|         - | 5486 | `	/* Mark this class as being autoloaded */` |
|       103 | 5487 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 5488 | `	/* Prepare the class name argument */` |
|       103 | 5489 | `	PH7_MemObjInit(pVm,&sArg);` |
|       103 | 5490 | `	PH7_MemObjInit(pVm,&sResult);` |
|       103 | 5491 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       103 | 5492 | `	pClass = 0;` |
|       183 | 5493 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 5494 | `		ph7_value *apArg[1];` |
|       113 | 5495 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       113 | 5496 | `		if( pEntry == 0 ){` |
|       ! 0 | 5497 | `			continue;` |
|         - | 5498 | `		}` |
|       113 | 5499 | `		apArg[0] = &sArg;` |
|       113 | 5500 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|         - | 5501 | `			/* Callback could not be invoked — skip to next autoloader */` |
|        24 | 5502 | `			continue;` |
|         - | 5503 | `		}` |
|         - | 5504 | `		/* Check if the class is now available */` |
|        91 | 5505 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        91 | 5506 | `		if( pHashEntry ){` |
|        33 | 5507 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        33 | 5508 | `			if( pClass ){` |
|        33 | 5509 | `				break;` |
|         - | 5510 | `			}` |
|       ! 0 | 5511 | `		}` |
|        34 | 5512 | `	}` |
|       103 | 5513 | `	PH7_MemObjRelease(&sArg);` |
|       103 | 5514 | `	PH7_MemObjRelease(&sResult);` |
|         - | 5515 | `	/* Remove reentrancy guard */` |
|       103 | 5516 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       103 | 5517 | `	return pClass;` |
|       175 | 5518 | `}` |
|         - | 5519 | `/*` |
|         - | 5520 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 5521 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 5522 | ` */` |
|        42 | 5523 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         4 | 5524 | `{` |
|        46 | 5525 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         4 | 5526 | `}` |
|         - | 5527 | `/*` |
|         - | 5528 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 5529 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 5530 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 5531 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 5532 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 5533 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 5534 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 5535 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 5536 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 5537 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 5538 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 5539 | ` */` |
|   7322064 | 5540 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 5541 | `{` |
|   7322069 | 5542 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        66 | 5543 | `		(*pzName)++;` |
|        66 | 5544 | `		(*pnByte)--;` |
|        31 | 5545 | `	}` |
|   7322069 | 5546 | `}` |
|         - | 5547 | `/*` |
|         - | 5548 | ` * Check if the given name refer to an installed class.` |
|         - | 5549 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 5550 | ` */` |
|   7316350 | 5551 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 5552 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 5553 | `	const char *zName,  /* Name of the target class */` |
|         - | 5554 | `	sxu32 nByte,        /* zName length */` |
|         - | 5555 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 5556 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 5557 | `						 */` |
|         - | 5558 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 5559 | `	)` |
|         5 | 5560 | `{` |
|         - | 5561 | `	SyHashEntry *pEntry;` |
|         - | 5562 | `	ph7_class *pClass;` |
|   7316355 | 5563 | `	sxu32 nOrig = nByte;` |
|   3658175 | 5564 | `	SXUNUSED(iNest);` |
|         - | 5565 | `	/* Exact class lookup.` |
|         - | 5566 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 5567 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 5568 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 5569 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   7316355 | 5570 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 5571 | `	/* An empty stripped name never matches a stored key (none is empty); skip the` |
|         - | 5572 | `	 * hash probe. But php still fires the autoloader when the ORIGINAL name was` |
|         - | 5573 | `	 * non-empty — a lone "\" autoloads with the empty stripped name, whereas a` |
|         - | 5574 | `	 * truly empty "" does not. Gate autoload on nOrig, pass the stripped name. */` |
|   7316355 | 5575 | `	pEntry = nByte > 0 ? SyHashGet(&pVm->hClass,(const void *)zName,nByte) : 0;` |
|   7316355 | 5576 | `	if( pEntry == 0 ){` |
|         - | 5577 | `		/* Class not found in hash table — try autoload before giving up */` |
|       307 | 5578 | `		return nOrig > 0 ? VmTriggerAutoload(pVm,zName,nByte,iLoadable) : 0;` |
|         - | 5579 | `	}` |
|   7316053 | 5580 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   7316053 | 5581 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   3658180 | 5582 | `}` |
|         - | 5583 | `/*` |
|         - | 5584 | ` * Reference Table Implementation` |
|         - | 5585 | ` * Status: stable <chm@symisc.net>` |
|         - | 5586 | ` * Intro` |
|         - | 5587 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 5588 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 5589 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 5590 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 5591 | ` *  Refer to the official for more information on this powerful` |
|         - | 5592 | ` *  extension.` |
|         - | 5593 | ` */` |
|         - | 5594 | `/*` |
|         - | 5595 | ` * Allocate a new reference entry.` |
|         - | 5596 | ` */` |
|  23123674 | 5597 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 5598 | `{` |
|         - | 5599 | `	VmRefObj *pRef;` |
|         - | 5600 | `	/* Allocate a new instance */` |
|  23123679 | 5601 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  23123679 | 5602 | `	if( pRef == 0 ){` |
|       ! 0 | 5603 | `		return 0;` |
|         - | 5604 | `	}` |
|         - | 5605 | `	/* Zero the structure */` |
|  23123679 | 5606 | `	SyZero(pRef,sizeof(VmRefObj));` |
|         - | 5607 | `	/* Initialize fields */` |
|  23123679 | 5608 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  23123679 | 5609 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  23123679 | 5610 | `	pRef->nIdx = nIdx;` |
|  23123679 | 5611 | `	return pRef;` |
|  11563030 | 5612 | `}` |
|         - | 5613 | `/*` |
|         - | 5614 | ` * Default hash function used by the reference table` |
|         - | 5615 | ` * for lookup/insertion operations.` |
|         - | 5616 | ` */` |
| 107994775 | 5617 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|         5 | 5618 | `{` |
|         - | 5619 | `	/* Calculate the hash based on the memory object index */` |
| 107994780 | 5620 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|         5 | 5621 | `}` |
|         - | 5622 | `/*` |
|         - | 5623 | ` * Check if a memory object [i.e: a variable] is already installed` |
|         - | 5624 | ` * in the reference table.` |
|         - | 5625 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|         - | 5626 | ` * otherwise.` |
|         - | 5627 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5628 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5629 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5630 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5631 | ` * Refer to the official for more information on this powerful` |
|         - | 5632 | ` * extension.` |
|         - | 5633 | ` */` |
|  60559476 | 5634 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|         5 | 5635 | `{` |
|         - | 5636 | `	VmRefObj *pRef;` |
|         - | 5637 | `	sxu32 nBucket;` |
|         - | 5638 | `	/* Point to the appropriate bucket */` |
|  60559481 | 5639 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|         - | 5640 | `	/* Perform the lookup */` |
|  60559481 | 5641 | `	pRef = pVm->apRefObj[nBucket];` |
| 311746760 | 5642 | `	for(;;){` |
| 623446763 | 5643 | `		if( pRef == 0 ){` |
|  29997297 | 5644 | `			break;` |
|         - | 5645 | `		}` |
| 593449471 | 5646 | `		if( pRef->nIdx == nObjIdx ){` |
|         - | 5647 | `			/* Entry found */` |
|  30562189 | 5648 | `			return pRef;` |
|         - | 5649 | `		}` |
|         - | 5650 | `		/* Point to the next entry */` |
| 562887287 | 5651 | `		pRef = pRef->pNextCollide;` |
|         5 | 5652 | `	}` |
|         - | 5653 | `	/* No such entry,return NULL */` |
|  29997297 | 5654 | `	return 0;` |
|  30283307 | 5655 | `}` |
|         - | 5656 | `/*` |
|         - | 5657 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 5658 | ` *` |
|         - | 5659 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5660 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5661 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5662 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5663 | ` * Refer to the official for more information on this powerful` |
|         - | 5664 | ` * extension.` |
|         - | 5665 | ` */` |
|  23123674 | 5666 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 5667 | `{` |
|         - | 5668 | `	sxu32 nBucket;` |
|  23123679 | 5669 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|         - | 5670 | `		VmRefObj **apNew;` |
|         - | 5671 | `		sxu32 nNew;` |
|         - | 5672 | `		/* Allocate a larger table */` |
|     12497 | 5673 | `		nNew = pVm->nRefSize << 1;` |
|     12497 | 5674 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     12497 | 5675 | `		if( apNew ){` |
|     12497 | 5676 | `			VmRefObj *pEntry = pVm->pRefList;` |
|         - | 5677 | `			sxu32 n;` |
|         - | 5678 | `			/* Zero the structure */` |
|     12497 | 5679 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|         - | 5680 | `			/* Rehash all referenced entries */` |
|   3707255 | 5681 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|         - | 5682 | `				/* Remove old collision links */` |
|   3694763 | 5683 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 5684 | `				/* Point to the appropriate bucket */` |
|   3694763 | 5685 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|         - | 5686 | `				/* Insert the entry  */` |
|   3694763 | 5687 | `				pEntry->pNextCollide = apNew[nBucket];` |
|   3694763 | 5688 | `				if( apNew[nBucket] ){` |
|   2965707 | 5689 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|   1482851 | 5690 | `				}` |
|   3694763 | 5691 | `				apNew[nBucket] = pEntry;` |
|         - | 5692 | `				/* Point to the next entry */` |
|   3694763 | 5693 | `				pEntry = pEntry->pNext;` |
|   1847384 | 5694 | `			}` |
|         - | 5695 | `			/* Release the old table */` |
|     12497 | 5696 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|         - | 5697 | `			/* Install the new one */` |
|     12497 | 5698 | `			pVm->apRefObj = apNew;` |
|     12497 | 5699 | `			pVm->nRefSize = nNew;` |
|      6246 | 5700 | `		}` |
|      6246 | 5701 | `	}` |
|         - | 5702 | `	/* Point to the appropriate bucket */` |
|  23123679 | 5703 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|         - | 5704 | `	/* Insert the entry */` |
|  23123679 | 5705 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  23123679 | 5706 | `	if( pVm->apRefObj[nBucket] ){` |
|  20844663 | 5707 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  10423193 | 5708 | `	}` |
|  23123679 | 5709 | `	pVm->apRefObj[nBucket] = pRef;` |
|  23123679 | 5710 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  23123679 | 5711 | `	pVm->nRefUsed++;` |
|  23123679 | 5712 | `	return SXRET_OK;` |
|         5 | 5713 | `}` |
|         - | 5714 | `/*` |
|         - | 5715 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|         - | 5716 | ` * the reference table.` |
|         - | 5717 | ` * This function is invoked when the user perform an unset` |
|         - | 5718 | ` * call [i.e: unset($var); ].` |
|         - | 5719 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5720 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5721 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5722 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5723 | ` * Refer to the official for more information on this powerful` |
|         - | 5724 | ` * extension.` |
|         - | 5725 | ` */` |
|  22357362 | 5726 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 5727 | `{` |
|         - | 5728 | `	ph7_hashmap_node **apNode;` |
|         - | 5729 | `	SyHashEntry **apEntry;` |
|         - | 5730 | `	sxu32 n;` |
|         - | 5731 | `	/* Point to the reference table */` |
|  22357367 | 5732 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  22357367 | 5733 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|         - | 5734 | `	/* Unlink the entry from the reference table */` |
|  29236487 | 5735 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   6879125 | 5736 | `		if( apEntry[n] ){` |
|   6873169 | 5737 | `			SyHashDeleteEntry2(apEntry[n]);` |
|   3437770 | 5738 | `		}` |
|   3440753 | 5739 | `	}` |
|  29739143 | 5740 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   7381781 | 5741 | `		if( apNode[n] ){` |
|      1245 | 5742 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|       620 | 5743 | `		}` |
|   3690893 | 5744 | `	}` |
|  22357367 | 5745 | `	if( pRef->pPrevCollide ){` |
|   1740500 | 5746 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|    870638 | 5747 | `	}else{` |
|  20616872 | 5748 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|         - | 5749 | `	}` |
|  22357367 | 5750 | `	if( pRef->pNextCollide ){` |
|  19150861 | 5751 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   9576282 | 5752 | `	}` |
|  22357367 | 5753 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|         - | 5754 | `	/* Release the node */` |
|  22357367 | 5755 | `	SySetRelease(&pRef->aReference);` |
|  22357367 | 5756 | `	SySetRelease(&pRef->aArrEntries);` |
|  22357367 | 5757 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  22357367 | 5758 | `	pVm->nRefUsed--;` |
|  22357367 | 5759 | `	return SXRET_OK;` |
|         5 | 5760 | `}` |
|         - | 5761 | `/*` |
|         - | 5762 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 5763 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5764 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5765 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5766 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5767 | ` * Refer to the official for more information on this powerful` |
|         - | 5768 | ` * extension.` |
|         - | 5769 | ` */` |
|  23178322 | 5770 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 5771 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 5772 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 5773 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 5774 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 5775 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 5776 | `	)` |
|         5 | 5777 | `{` |
|  23178327 | 5778 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 5779 | `	VmRefObj *pRef;` |
|         - | 5780 | `	/* Check if the referenced object already exists */` |
|  23178327 | 5781 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  23178327 | 5782 | `	if( pRef == 0 ){` |
|         - | 5783 | `		/* Create a new entry */` |
|  23123679 | 5784 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  23123679 | 5785 | `		if( pRef == 0 ){` |
|       ! 0 | 5786 | `			return SXERR_MEM;` |
|         - | 5787 | `		}` |
|  23123679 | 5788 | `		pRef->iFlags = iFlags;` |
|         - | 5789 | `		/* Install the entry */` |
|  23123679 | 5790 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  11563025 | 5791 | `	}` |
|  23178327 | 5792 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  23178327 | 5793 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 5794 | `		VmSlot sRef;` |
|         - | 5795 | `		/* Local frame,record referenced entry so that it can` |
|         - | 5796 | `		 * be deleted when we leave this frame.` |
|         - | 5797 | `		 */` |
|   6873135 | 5798 | `		sRef.nIdx = nIdx;` |
|   6873135 | 5799 | `		sRef.pUserData = pEntry;` |
|   6873135 | 5800 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 5801 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 5802 | `		}` |
|   3437753 | 5803 | `	}` |
|  23178327 | 5804 | `	if( pEntry ){` |
|         - | 5805 | `		/* Address of the hash-entry */` |
|   6926775 | 5806 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|   3464573 | 5807 | `	}` |
|  23178327 | 5808 | `	if( pMapEntry ){` |
|         - | 5809 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|   7504031 | 5810 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|   3752013 | 5811 | `	}` |
|  23178327 | 5812 | `	return SXRET_OK;` |
|  11590354 | 5813 | `}` |
|         - | 5814 | `/*` |
|         - | 5815 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 5816 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5817 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5818 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5819 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5820 | ` * Refer to the official for more information on this powerful` |
|         - | 5821 | ` * extension.` |
|         - | 5822 | ` */` |
|  14247696 | 5823 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 5824 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 5825 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 5826 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 5827 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 5828 | `	)` |
|         5 | 5829 | `{` |
|         - | 5830 | `	VmRefObj *pRef;` |
|         - | 5831 | `	sxu32 n;` |
|         - | 5832 | `	/* Check if the referenced object already exists */` |
|  14247701 | 5833 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  14247701 | 5834 | `	if( pRef == 0 ){` |
|         - | 5835 | `		/* Not such entry */` |
|   6872931 | 5836 | `		return SXERR_NOTFOUND;` |
|         - | 5837 | `	}` |
|         - | 5838 | `	/* Remove the desired entry */` |
|   7374775 | 5839 | `	if( pEntry ){` |
|         - | 5840 | `		SyHashEntry **apEntry;` |
|        93 | 5841 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|       283 | 5842 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       195 | 5843 | `			if( apEntry[n] == pEntry ){` |
|         - | 5844 | `				/* Nullify the entry */` |
|        91 | 5845 | `				apEntry[n] = 0;` |
|         - | 5846 | `				/*` |
|         - | 5847 | `				 * NOTE:` |
|         - | 5848 | `				 * In future releases,think to add a free pool of entries,so that` |
|         - | 5849 | `				 * we avoid wasting spaces.` |
|         - | 5850 | `				 */` |
|        43 | 5851 | `			}` |
|       100 | 5852 | `		}` |
|        44 | 5853 | `	}` |
|   7374775 | 5854 | `	if( pMapEntry ){` |
|         - | 5855 | `		ph7_hashmap_node **apNode;` |
|   7374687 | 5856 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  14749633 | 5857 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   7374951 | 5858 | `			if( apNode[n] == pMapEntry ){` |
|         - | 5859 | `				/* nullify the entry */` |
|   7374687 | 5860 | `				apNode[n] = 0;` |
|   3687341 | 5861 | `			}` |
|   3687478 | 5862 | `		}` |
|   3687341 | 5863 | `	}` |
|   7374775 | 5864 | `	return SXRET_OK;` |
|   7125041 | 5865 | `}` |
|         - | 5866 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 5867 | `/*` |
|         - | 5868 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 5869 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 5870 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 5871 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 5872 | ` * For more information on how to register IO stream devices,please` |
|         - | 5873 | ` * refer to the official documentation.` |
|         - | 5874 | ` */` |
|     32226 | 5875 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 5876 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 5877 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 5878 | `	int nByte              /* *pzDevice length*/` |
|         - | 5879 | `	)` |
|         5 | 5880 | `{` |
|         - | 5881 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|         - | 5882 | `	ph7_io_stream **apStream,*pStream;` |
|         - | 5883 | `	SyString sDev,sCur;` |
|         - | 5884 | `	sxu32 n,nEntry;` |
|         - | 5885 | `	int rc;` |
|         - | 5886 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|     32231 | 5887 | `	zNext = zCur = zIn = *pzDevice;` |
|     32231 | 5888 | `	zEnd = &zIn[nByte];` |
|   2049383 | 5889 | `	while( zIn < zEnd ){` |
|   2017251 | 5890 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|         - | 5891 | `			/* Got one */` |
|        99 | 5892 | `			zNext = &zIn[sizeof("://")-1];` |
|        99 | 5893 | `			break;` |
|         - | 5894 | `		}` |
|         - | 5895 | `		/* Advance the cursor */` |
|   2017157 | 5896 | `		zIn++;` |
|         5 | 5897 | `	}` |
|     32231 | 5898 | `	if( zIn >= zEnd ){` |
|         - | 5899 | `		/* No such scheme,return the default stream */` |
|     32137 | 5900 | `		return pVm->pDefStream;` |
|         - | 5901 | `	}` |
|        99 | 5902 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|         - | 5903 | `	/* Remove leading and trailing white spaces */` |
|        99 | 5904 | `	SyStringFullTrim(&sDev);` |
|         - | 5905 | `	/* Perform a linear lookup on the installed stream devices */` |
|        99 | 5906 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        99 | 5907 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|       133 | 5908 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       133 | 5909 | `		pStream = apStream[n];` |
|       133 | 5910 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|         - | 5911 | `		/* Perfrom a case-insensitive comparison */` |
|       133 | 5912 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|       133 | 5913 | `		if( rc == 0 ){` |
|         - | 5914 | `			/* Stream device found */` |
|        99 | 5915 | `			*pzDevice = zNext;` |
|        99 | 5916 | `			return pStream;` |
|         - | 5917 | `		}` |
|        19 | 5918 | `	}` |
|         - | 5919 | `	/* No such stream,return NULL */` |
|       ! 0 | 5920 | `	return 0;` |
|     16118 | 5921 | `}` |
|         - | 5922 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 5923 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 5924 |  |
