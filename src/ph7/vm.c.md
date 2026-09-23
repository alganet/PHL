# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2804/3270 lines (85.75%)

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
|   2002308 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   77 | `{` |
|   2002313 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|        42 |   79 | `		return TRUE;` |
|         - |   80 | `	}` |
|   2002273 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        11 |   82 | `		return TRUE;` |
|         - |   83 | `	}` |
|   2002263 |   84 | `	return FALSE;` |
|   1002830 |   85 | `}` |
|         - |   86 | `/*` |
|         - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |   93 | ` * still go through the existing numeric coercion.` |
|         - |   94 | ` */` |
|    667387 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |   96 | `{` |
|         - |   97 | `	SyString sStr;` |
|    667392 |   98 | `	sxu8 bReal = FALSE;` |
|    667392 |   99 | `	const char *zTail = 0;` |
|         - |  100 | `	const char *zEnd;` |
|    667392 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    667374 |  102 | `		return FALSE;` |
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
|    334117 |  119 | `}` |
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
|   1879030 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  139 | `	const SyString *pName,  /* Constant name */` |
|         - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  142 | `	)` |
|         5 |  143 | `{` |
|   1879035 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  145 | `}` |
|         - |  146 | `/*` |
|         - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  149 | ` */` |
|   1879212 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|   1879217 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   1879217 |  165 | `	if( pEntry ){` |
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
|   1879215 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   1879215 |  190 | `	if( pCons == 0 ){` |
|       ! 0 |  191 | `		return 0;` |
|         - |  192 | `	}` |
|         - |  193 | `	/* Duplicate constant name */` |
|   1879215 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   1879215 |  195 | `	if( zDupName == 0 ){` |
|       ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  197 | `		return 0;` |
|         - |  198 | `	}` |
|   1879215 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|   1879215 |  200 | `	if( pFile ){` |
|       185 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|        90 |  202 | `	}` |
|   1879215 |  203 | `	pCons->nLine = nLine;` |
|   1879215 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         - |  205 | `	/* Install the constant */` |
|   1879215 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   1879215 |  207 | `	pCons->xExpand = xExpand;` |
|   1879215 |  208 | `	pCons->pUserData = pUserData;` |
|   1879215 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   1879215 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   1879215 |  211 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  214 | `		return rc;` |
|         - |  215 | `	}` |
|         - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|   1879215 |  217 | `	return SXRET_OK;` |
|    939611 |  218 | `}` |
|         - |  219 | `/*` |
|         - |  220 | ` * Allocate a new foreign function instance.` |
|         - |  221 | ` * This function return SXRET_OK on success. Any other` |
|         - |  222 | ` * return value indicates failure.` |
|         - |  223 | ` * Please refer to the official documentation for an introduction to` |
|         - |  224 | ` * the foreign function mechanism.` |
|         - |  225 | ` */` |
|   6155374 |  226 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
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
|   6155379 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   6155379 |  238 | `	if( pFunc == 0 ){` |
|       ! 0 |  239 | `		return SXERR_MEM;` |
|         - |  240 | `	}` |
|         - |  241 | `	/* Duplicate function name */` |
|   6155379 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   6155379 |  243 | `	if( zDup == 0 ){` |
|       ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  245 | `		return SXERR_MEM;` |
|         - |  246 | `	}` |
|         - |  247 | `	/* Zero the structure */` |
|   6155379 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  249 | `	/* Initialize structure fields */` |
|   6155379 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   6155379 |  251 | `	pFunc->pVm   = pVm;` |
|   6155379 |  252 | `	pFunc->xFunc = xFunc;` |
|   6155379 |  253 | `	pFunc->pUserData = pUserData;` |
|   6155379 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  255 | `	/* Write a pointer to the new function */` |
|   6155379 |  256 | `	*ppOut = pFunc;` |
|   6155379 |  257 | `	return SXRET_OK;` |
|   3077692 |  258 | `}` |
|         - |  259 | `/*` |
|         - |  260 | ` * Install a foreign function and it's associated callback so that` |
|         - |  261 | ` * it can be invoked from the target PHP code.` |
|         - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  263 | ` * return value indicates failure.` |
|         - |  264 | ` * Please refer to the official documentation for an introduction to` |
|         - |  265 | ` * the foreign function mechanism.` |
|         - |  266 | ` */` |
|   2497974 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   2497979 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   2497979 |  279 | `	if( pEntry ){` |
|      4081 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      4081 |  281 | `		pFunc->pUserData = pUserData;` |
|      4081 |  282 | `		pFunc->xFunc = xFunc;` |
|      4081 |  283 | `		SySetReset(&pFunc->aAux);` |
|         - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|      4081 |  288 | `		pFunc->nMinArg  = 0;` |
|      4081 |  289 | `		pFunc->nMaxArg  = 0;` |
|      4081 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|      4081 |  291 | `		pFunc->bAtLeast = 0;` |
|      4081 |  292 | `		return SXRET_OK;` |
|         - |  293 | `	}` |
|         - |  294 | `	/* Create a new user function */` |
|   2493903 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   2493903 |  296 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  297 | `		return rc;` |
|         - |  298 | `	}` |
|         - |  299 | `	/* Install the function in the corresponding hashtable */` |
|   2493903 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   2493903 |  301 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  304 | `		return rc;` |
|         - |  305 | `	}` |
|         - |  306 | `	/* User function successfully installed */` |
|   2493903 |  307 | `	return SXRET_OK;` |
|   1248992 |  308 | `}` |
|         - |  309 | `/*` |
|         - |  310 | ` * Initialize a VM function.` |
|         - |  311 | ` */` |
|   3813520 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  315 | `	const char *zName,  /* Function name */` |
|         - |  316 | `	sxu32 nByte,        /* zName length */` |
|         - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  318 | `	void *pUserData     /* Function private data */` |
|         - |  319 | `	)` |
|         5 |  320 | `{` |
|         - |  321 | `	/* Zero the structure */` |
|   3813525 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  323 | `	/* Initialize structure fields */` |
|         - |  324 | `	/* Arguments container */` |
|   3813525 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  326 | `	/* Static variable container */` |
|   3813525 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  328 | `	/* Bytecode container */` |
|   3813525 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  330 | `    /* Preallocate some instruction slots */` |
|   3813525 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  332 | `	/* Closure environment */` |
|   3813525 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   3813525 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  336 | `	/* Declared #[...] attributes */` |
|   3813525 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   3813525 |  338 | `	pFunc->iFlags = iFlags;` |
|   3813525 |  339 | `	pFunc->pUserData = pUserData;` |
|         - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   3813525 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   3813525 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   3801385 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   1900695 |  346 | `	}else{` |
|         - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     12145 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     12145 |  351 | `		if( pFile ){` |
|     12145 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|      6070 |  353 | `		}` |
|         - |  354 | `	}` |
|   3813525 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   3813525 |  356 | `	return SXRET_OK;` |
|         5 |  357 | `}` |
|         - |  358 | `/*` |
|         - |  359 | ` * Namespace-aware function lookup.` |
|         - |  360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  362 | ` */` |
|         - |  363 | `/*` |
|         - |  364 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  365 | ` */` |
|  14464462 |  366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  367 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  368 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  369 | `	SyString *pName     /* Function name */` |
|         - |  370 | `	)` |
|         5 |  371 | `{` |
|         - |  372 | `	SyHashEntry *pEntry;` |
|         - |  373 | `	sxi32 rc;` |
|  14464467 |  374 | `	if( pName == 0 ){` |
|         - |  375 | `		/* Use the built-in name */` |
|    149181 |  376 | `		pName = &pFunc->sName;` |
|     74588 |  377 | `	}` |
|         - |  378 | `	/* Check for duplicates (functions with the same name) first */` |
|  14464467 |  379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  14464467 |  380 | `	if( pEntry ){` |
|  10814087 |  381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  10814087 |  382 | `		if( pLink != pFunc ){` |
|         - |  383 | `			/* Link */` |
|        90 |  384 | `			pFunc->pNextName = pLink;` |
|        90 |  385 | `			pEntry->pUserData = pFunc;` |
|        43 |  386 | `		}` |
|  10814087 |  387 | `		return SXRET_OK;` |
|         - |  388 | `	}` |
|         - |  389 | `	/* First time seen */` |
|   3650385 |  390 | `	pFunc->pNextName = 0;` |
|   3650385 |  391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   3650385 |  392 | `	return rc;` |
|   7232236 |  393 | `}` |
|         - |  394 | `/*` |
|         - |  395 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  396 | ` */` |
|    629410 |  397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  398 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  399 | `	ph7_class *pClass /* Target Class */` |
|         - |  400 | `	)` |
|         5 |  401 | `{` |
|    629415 |  402 | `	SyString *pName = &pClass->sName;` |
|         - |  403 | `	SyHashEntry *pEntry;` |
|         - |  404 | `	sxi32 rc;` |
|         - |  405 | `	/* Check for duplicates */` |
|    629415 |  406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    629415 |  407 | `	if( pEntry ){` |
|         3 |  408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  409 | `		/* Link entry with the same name */` |
|         3 |  410 | `		pClass->pNextName = pLink;` |
|         3 |  411 | `		pEntry->pUserData = pClass;` |
|         3 |  412 | `		return SXRET_OK;` |
|         - |  413 | `	}` |
|    629413 |  414 | `	pClass->pNextName = 0;` |
|         - |  415 | `	/* Perform a simple hashtable insertion */` |
|    629413 |  416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    629413 |  417 | `	return rc;` |
|    314710 |  418 | `}` |
|         - |  419 | `/*` |
|         - |  420 | ` * Instruction builder interface.` |
|         - |  421 | ` */` |
|  10619428 |  422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  423 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  424 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  425 | `	sxi32 iP1,    /* First operand */` |
|         - |  426 | `	sxu32 iP2,    /* Second operand */` |
|         - |  427 | `	void *p3,     /* Third operand */` |
|         - |  428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  429 | `	)` |
|         5 |  430 | `{` |
|         - |  431 | `	VmInstr sInstr;` |
|  10619433 |  432 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  433 | `	sxi32 rc;` |
|         - |  434 | `	/* Fill the VM instruction */` |
|  10619433 |  435 | `	sInstr.iOp = (sxu8)iOp;` |
|  10619433 |  436 | `	sInstr.iP1 = iP1;` |
|  10619433 |  437 | `	sInstr.iP2 = iP2;` |
|  10619433 |  438 | `	sInstr.p3  = p3;` |
|         - |  439 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  440 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  441 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  442 | `	 * between statements, hence the range check. */` |
|  10619433 |  443 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|  10619433 |  444 | `	sInstr.nLine = 0;` |
|  10619433 |  445 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|   3851697 |  446 | `		sInstr.nLine = pGen->pIn->nLine;` |
|   8693587 |  447 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  448 | `		/* Past the end (statement tail): blame the last real token. */` |
|   6719609 |  449 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|   3359802 |  450 | `	}` |
|  10619433 |  451 | `	if( pIndex ){` |
|         - |  452 | `		/* Instruction index in the bytecode array */` |
|    850123 |  453 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    425059 |  454 | `	}` |
|         - |  455 | `	/* Finally,record the instruction */` |
|  10619433 |  456 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  10619433 |  457 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  458 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  459 | `		/* Fall throw */` |
|       ! 0 |  460 | `	}` |
|  10619433 |  461 | `	return rc;` |
|         5 |  462 | `}` |
|         - |  463 | `/*` |
|         - |  464 | ` * Swap the current bytecode container with the given one.` |
|         - |  465 | ` */` |
|    475204 |  466 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  467 | `{` |
|    475209 |  468 | `	if( pContainer == 0 ){` |
|         - |  469 | `		/* Point to the default container */` |
|       ! 0 |  470 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  471 | `	}else{` |
|         - |  472 | `		/* Change container */` |
|    475209 |  473 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  474 | `	}` |
|    475209 |  475 | `	return SXRET_OK;` |
|         5 |  476 | `}` |
|         - |  477 | `/*` |
|         - |  478 | ` * Return the current bytecode container.` |
|         - |  479 | ` */` |
|    869260 |  480 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  481 | `{` |
|    869265 |  482 | `	return pVm->pByteContainer;` |
|         5 |  483 | `}` |
|         - |  484 | `/*` |
|         - |  485 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  486 | ` */` |
|    271704 |  487 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  488 | `{` |
|         - |  489 | `	VmInstr *pInstr;` |
|    271709 |  490 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    271709 |  491 | `	return pInstr;` |
|         5 |  492 | `}` |
|         - |  493 | `/*` |
|         - |  494 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  495 | ` */` |
|   5334128 |  496 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  497 | `{` |
|   5334133 |  498 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  499 | `}` |
|         - |  500 | `/*` |
|         - |  501 | ` * Pop the last VM instruction.` |
|         - |  502 | ` */` |
|    762880 |  503 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  504 | `{` |
|    762885 |  505 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  506 | `}` |
|         - |  507 | `/*` |
|         - |  508 | ` * Peek the last VM instruction.` |
|         - |  509 | ` */` |
|   2001812 |  510 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  511 | `{` |
|   2001817 |  512 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  513 | `}` |
|     66134 |  514 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  515 | `{` |
|         - |  516 | `	VmInstr *aInstr;` |
|         - |  517 | `	sxu32 n;` |
|     66139 |  518 | `	n = SySetUsed(pVm->pByteContainer);` |
|     66139 |  519 | `	if( n < 2 ){` |
|       ! 0 |  520 | `		return 0;` |
|         - |  521 | `	}` |
|     66139 |  522 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     66139 |  523 | `	return &aInstr[n - 2];` |
|     33072 |  524 | `}` |
|         - |  525 | `/*` |
|         - |  526 | ` * Allocate a new virtual machine frame.` |
|         - |  527 | ` */` |
|   3646550 |  528 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  529 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  530 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  531 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  532 | `	)` |
|         5 |  533 | `{` |
|         - |  534 | `	VmFrame *pFrame;` |
|         - |  535 | `	/* Allocate a new vm frame */` |
|   3646555 |  536 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   3646555 |  537 | `	if( pFrame == 0 ){` |
|       ! 0 |  538 | `		return 0;` |
|         - |  539 | `	}` |
|         - |  540 | `	/* Zero the structure */` |
|   3646555 |  541 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  542 | `	/* Initialize frame fields */` |
|   3646555 |  543 | `	pFrame->pUserData = pUserData;` |
|   3646555 |  544 | `	pFrame->pThis = pThis;` |
|   3646555 |  545 | `	pFrame->pVm = pVm;` |
|   3646555 |  546 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   3646555 |  547 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   3646555 |  548 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   3646555 |  549 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   3646555 |  550 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  551 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  552 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   3646555 |  553 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   3646555 |  554 | `	return pFrame;` |
|   1823492 |  555 | `}` |
|         - |  556 | `/* Forward declaration */` |
|         - |  557 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  558 | `/*` |
|         - |  559 | ` * Enter a VM frame.` |
|         - |  560 | ` */` |
|   3645878 |  561 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  562 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  563 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  564 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  565 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  566 | `	)` |
|         5 |  567 | `{` |
|         - |  568 | `	VmFrame *pFrame;` |
|         - |  569 | `	/* Allocate a new frame */` |
|   3645883 |  570 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   3645883 |  571 | `	if( pFrame == 0 ){` |
|       ! 0 |  572 | `		return SXERR_MEM;` |
|         - |  573 | `	}` |
|         - |  574 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   3645883 |  575 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  576 | `	/* Link to the list of active VM frame */` |
|   3645883 |  577 | `	pFrame->pParent = pVm->pFrame;` |
|   3645883 |  578 | `	pVm->pFrame = pFrame;` |
|   3645883 |  579 | `	if( ppFrame ){` |
|         - |  580 | `		/* Write a pointer to the new VM frame */` |
|   3641205 |  581 | `		*ppFrame = pFrame;` |
|   1820812 |  582 | `	}` |
|   3645883 |  583 | `	return SXRET_OK;` |
|   1823156 |  584 | `}` |
|         - |  585 | `/*` |
|         - |  586 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  587 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  588 | ` * information.` |
|         - |  589 | ` */` |
|        92 |  590 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  591 | `{` |
|         - |  592 | `	VmFrame *pTarget,*pGlobal;` |
|         - |  593 | `	SyHashEntry *pEntry;` |
|         - |  594 | `	sxi32 rc;` |
|        97 |  595 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|         - |  596 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|         - |  597 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|         - |  598 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|         - |  599 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|         - |  600 | `	 * writes never reached the real global. */` |
|        97 |  601 | `	pGlobal = pTarget;` |
|       199 |  602 | `	while( pGlobal->pParent ){` |
|       107 |  603 | `		pGlobal = pGlobal->pParent;` |
|         5 |  604 | `	}` |
|        97 |  605 | `	if( pGlobal == pTarget ){` |
|         - |  606 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|       ! 0 |  607 | `		return SXRET_OK;` |
|         - |  608 | `	}` |
|         - |  609 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|         - |  610 | `	 * plain global that would shadow it. */` |
|        97 |  611 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|        97 |  612 | `	if( pEntry == 0 ){` |
|        95 |  613 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        45 |  614 | `	}` |
|        97 |  615 | `	if( pEntry == 0 ){` |
|         - |  616 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|         - |  617 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|         - |  618 | `		 * the assignment went to a local nobody could read. */` |
|        12 |  619 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|        12 |  620 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  621 | `			return rc;` |
|         - |  622 | `		}` |
|        12 |  623 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        12 |  624 | `		if( pEntry == 0 ){` |
|       ! 0 |  625 | `			return SXERR_NOTFOUND;` |
|         - |  626 | `		}` |
|         5 |  627 | `	}` |
|         - |  628 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|         - |  629 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|       143 |  630 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|        92 |  631 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|        97 |  632 | `	return SXRET_OK;` |
|        51 |  633 | `}` |
|         - |  634 | `/*` |
|         - |  635 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  636 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  637 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  638 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  639 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  640 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  641 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  642 | ` */` |
|   3641682 |  643 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  644 | `{` |
|   3641687 |  645 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 |  646 | `		pVm->pResumeFrame = 0;` |
|       ! 0 |  647 | `	}` |
|   3641687 |  648 | `}` |
|         - |  649 | `/*` |
|         - |  650 | ` * Leave the top-most active frame.` |
|         - |  651 | ` */` |
|   3640988 |  652 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  653 | `{` |
|   3640993 |  654 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   3640993 |  655 | `	if( pCurFrame ){` |
|         - |  656 | `		/* Unlink from the list of active VM frame */` |
|   3640993 |  657 | `		pVm->pFrame = pCurFrame->pParent;` |
|   3640993 |  658 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  659 | `			VmSlot  *aSlot;` |
|         - |  660 | `			sxu32 n;` |
|         - |  661 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|         - |  662 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|         - |  663 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|         - |  664 | `			 * is about to free). */` |
|    748353 |  665 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   1822325 |  666 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   1073977 |  667 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    538263 |  668 | `			}` |
|         - |  669 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    748353 |  670 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   1822143 |  671 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|   1073795 |  672 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|         - |  673 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|         - |  674 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|         - |  675 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|         - |  676 | `					 * down the slot and the reference table took the holders with it, so` |
|         - |  677 | `					 * the returned array came back one element SHORT. The last holder to` |
|         - |  678 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|        24 |  679 | `					continue;` |
|         - |  680 | `				}` |
|         - |  681 | `				/* Unset the local variable */` |
|   1073773 |  682 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    538161 |  683 | `			}` |
|    374386 |  684 | `		}` |
|         - |  685 | `		/* Release internal containers */` |
|   3640993 |  686 | `		SyHashRelease(&pCurFrame->hVar);` |
|   3640993 |  687 | `		SySetRelease(&pCurFrame->sArg);` |
|   3640993 |  688 | `		SySetRelease(&pCurFrame->sLocal);` |
|   3640993 |  689 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  690 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  691 | `		 * containers above — released for every frame, including transparent` |
|         - |  692 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   3640993 |  693 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  694 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   3640993 |  695 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  696 | `		/* Release the whole structure */` |
|   3640993 |  697 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   1820706 |  698 | `	}` |
|   3640993 |  699 | `}` |
|         - |  700 | `/*` |
|         - |  701 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  702 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  703 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  704 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  705 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  706 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  707 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  708 | ` */` |
|         - |  709 | `/*` |
|         - |  710 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  711 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  712 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  713 | ` *` |
|         - |  714 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  715 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  716 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  717 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  718 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  719 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  720 | ` */` |
|   7530158 |  721 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  722 | `{` |
|         - |  723 | `	VmFrame *pFrame;` |
|  20392945 |  724 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|  12862817 |  725 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  726 | `		sxu32 n;` |
|  26704907 |  727 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|  13842125 |  728 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  729 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|        32 |  730 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|        32 |  731 | `				(void)SySetPop(&pFrame->sLocal);` |
|        32 |  732 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  733 | `			}` |
|   6921050 |  734 | `		}` |
|   6431396 |  735 | `	}` |
|   7530133 |  736 | `	return FALSE;` |
|   3765084 |  737 | `}` |
|       200 |  738 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         4 |  739 | `{` |
|         - |  740 | `	VmRefObj *pRef;` |
|       204 |  741 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       204 |  742 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       204 |  743 | `	if( pRef ){` |
|       180 |  744 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|        92 |  745 | `	}else{` |
|         - |  746 | `		/* No record yet — a pin on a slot nothing refers to was silently a NO-OP, so the` |
|         - |  747 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|        27 |  748 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  749 | `	}` |
|       204 |  750 | `}` |
|         - |  751 | `/*` |
|         - |  752 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|         - |  753 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|         - |  754 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|         - |  755 | ` */` |
|        42 |  756 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  757 | `{` |
|         - |  758 | `	VmRefObj *pRef;` |
|        43 |  759 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|        43 |  760 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        43 |  761 | `	if( pRef ){` |
|        43 |  762 | `		pRef->nPin++;` |
|        21 |  763 | `	}` |
|        43 |  764 | `}` |
|         - |  765 | `/*` |
|         - |  766 | ` * Give back a counted pin. The slot goes when it was the last holder — without this the` |
|         - |  767 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|         - |  768 | ` * used to be a flag, so nothing could tell one holder from two).` |
|         - |  769 | ` */` |
|        36 |  770 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  771 | `{` |
|        37 |  772 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        37 |  773 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|       ! 0 |  774 | `		return;` |
|         - |  775 | `	}` |
|        37 |  776 | `	pRef->nPin--;` |
|        37 |  777 | `	if( pRef->nPin < 1 ){` |
|        31 |  778 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|        31 |  779 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        15 |  780 | `	}` |
|        19 |  781 | `}` |
|         - |  782 | `/*` |
|         - |  783 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  784 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  785 | ` * should be skipped when looking for the real execution context.` |
|         - |  786 | ` */` |
|  41708127 |  787 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  788 | `{` |
|  55146314 |  789 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  13438187 |  790 | `		pFrame = pFrame->pParent;` |
|         5 |  791 | `	}` |
|  41708132 |  792 | `	return pFrame;` |
|         5 |  793 | `}` |
|         - |  794 | `/*` |
|         - |  795 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  796 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  797 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  798 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  799 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  800 | ` *` |
|         - |  801 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  802 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  803 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  804 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  805 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  806 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  807 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  808 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  809 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  810 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  811 | ` *` |
|         - |  812 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  813 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  814 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  815 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  816 | ` */` |
|   1958088 |  817 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  818 | `{` |
|   1958093 |  819 | `	if( pVm->pResumeFrame == 0 ){` |
|        23 |  820 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  821 | `	}` |
|   1958073 |  822 | `	if( pEntryFrame == 0 ){` |
|         - |  823 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - |  824 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - |  825 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - |  826 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - |  827 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - |  828 | `		 * NULL and the process dies. */` |
|        12 |  829 | `		return FALSE;` |
|         - |  830 | `	}` |
|         - |  831 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  832 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  833 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  834 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  835 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  836 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  837 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  838 | `	 * exec's resume site match and land. */` |
|   1958058 |  839 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1656449 |  840 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1354672 |  841 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  842 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  843 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  844 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  845 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  846 | `		 * never match a real frame. */` |
|    603569 |  847 | `		return FALSE;` |
|         - |  848 | `	}` |
|         - |  849 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  850 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  851 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  852 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  853 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  854 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  855 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  856 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  857 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  858 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  859 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  860 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  861 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  862 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  863 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  864 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  865 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  866 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  867 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  868 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  869 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|   2185333 |  870 | `	while( pVm->pFrame != pEntryFrame` |
|   2389001 |  871 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1558156 |  872 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|    307189 |  873 | `		VmLeaveFrame(&(*pVm));` |
|         5 |  874 | `	}` |
|   1354499 |  875 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|   1354499 |  876 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|         - |  877 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  878 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  879 | `	 * point router must not re-fire it after this resume. */` |
|   1354499 |  880 | `	pVm->nBoundaryRc = 0;` |
|   1354499 |  881 | `	return TRUE;` |
|    979049 |  882 | `}` |
|         - |  883 | `/*` |
|         - |  884 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  885 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  886 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  887 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  888 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  889 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  890 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  891 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  892 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  893 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  894 | ` */` |
|         - |  895 | `/*` |
|         - |  896 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  897 | ` *` |
|         - |  898 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  899 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  900 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  901 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  902 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  903 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  904 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  905 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  906 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  907 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  908 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  909 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  910 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  911 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  912 | ` * activation.` |
|         - |  913 | ` */` |
|   1457980 |  914 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  915 | `{` |
|   1457985 |  916 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1457985 |  917 | `	if( pClone == 0 ){` |
|       ! 0 |  918 | `		return 0;` |
|         - |  919 | `	}` |
|   1457985 |  920 | `	*pClone = *pCompiled;` |
|   1457985 |  921 | `	pClone->pCompiled = pCompiled;` |
|   1457985 |  922 | `	pClone->iFinallyDone = 0;` |
|   1457985 |  923 | `	pClone->iInCatch = 0;` |
|   1457985 |  924 | `	pClone->pInflight = 0;` |
|   1457985 |  925 | `	pClone->pFrame = 0;` |
|   1457985 |  926 | `	return pClone;` |
|    728995 |  927 | `}` |
|   2913208 |  928 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  929 | `{` |
|   2913213 |  930 | `	if( pExc && pExc->pCompiled ){` |
|         - |  931 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  932 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - |  933 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - |  934 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1457973 |  935 | `		if( pExc->pInflight ){` |
|       ! 0 |  936 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 |  937 | `			pExc->pInflight = 0;` |
|       ! 0 |  938 | `		}` |
|   1457973 |  939 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    728984 |  940 | `	}` |
|   2913213 |  941 | `}` |
|         - |  942 | `/*` |
|         - |  943 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - |  944 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - |  945 | ` */` |
|      3280 |  946 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 |  947 | `{` |
|      3285 |  948 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 |  949 | `}` |
|         - |  950 | `/*` |
|         - |  951 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - |  952 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - |  953 | ` * handlers). The set itself is reset by the caller.` |
|         - |  954 | ` */` |
|    100926 |  955 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 |  956 | `{` |
|    100931 |  957 | `	sxu32 n = SySetUsed(pSet);` |
|    100931 |  958 | `	if( n > 0 ){` |
|       ! 0 |  959 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - |  960 | `		sxu32 i;` |
|       ! 0 |  961 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 |  962 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 |  963 | `		}` |
|       ! 0 |  964 | `	}` |
|    100931 |  965 | `}` |
|         - |  966 | `/*` |
|         - |  967 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - |  968 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - |  969 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - |  970 | ` * down a try belonging to the caller.` |
|         - |  971 | ` */` |
|        18 |  972 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         4 |  973 | `{` |
|        22 |  974 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        22 |  975 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        22 |  976 | `	if( nBase < nFloor ){` |
|       ! 0 |  977 | `		nBase = nFloor;` |
|       ! 0 |  978 | `	}` |
|        22 |  979 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         4 |  980 | `}` |
|         - |  981 | `/*` |
|         - |  982 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - |  983 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - |  984 | ` * only carries the compiled pointer.` |
|         - |  985 | ` */` |
|        76 |  986 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  987 | `{` |
|        81 |  988 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        81 |  989 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        81 |  990 | `	while( n > 0 ){` |
|        81 |  991 | `		n--;` |
|        81 |  992 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        81 |  993 | `			return ap[n];` |
|         - |  994 | `		}` |
|       ! 0 |  995 | `	}` |
|       ! 0 |  996 | `	return 0;` |
|        43 |  997 | `}` |
|   3210072 |  998 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 |  999 | `{` |
|         - | 1000 | `	sxu32 nUsed;` |
|   3210077 | 1001 | `	sxi32 rcOut = SXRET_OK;` |
|   3210157 | 1002 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|        85 | 1003 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        85 | 1004 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|        85 | 1005 | `		(void)SySetPop(&pVm->aException);` |
|        85 | 1006 | `		pExc->pFrame = 0;` |
|         - | 1007 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - | 1008 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - | 1009 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - | 1010 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - | 1011 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|        85 | 1012 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        85 | 1013 | `			VmLeaveFrame(&(*pVm));` |
|        40 | 1014 | `		}` |
|       117 | 1015 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - | 1016 | `			sxi32 rcF;` |
|        69 | 1017 | `			pExc->iFinallyDone = 1;` |
|        69 | 1018 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        69 | 1019 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        69 | 1020 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 | 1021 | `				return SXERR_ABORT;` |
|         - | 1022 | `			}` |
|        69 | 1023 | `			if( rcF == PH7_EXCEPTION ){` |
|         - | 1024 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - | 1025 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - | 1026 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 | 1027 | `				rcOut = PH7_EXCEPTION;` |
|         2 | 1028 | `			}` |
|        37 | 1029 | `		}else{` |
|        21 | 1030 | `			VmExcRelease(&(*pVm),pExc);` |
|         - | 1031 | `		}` |
|         5 | 1032 | `	}` |
|   3210077 | 1033 | `	return rcOut;` |
|   1605253 | 1034 | `}` |
|         - | 1035 | `/*` |
|         - | 1036 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - | 1037 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - | 1038 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - | 1039 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - | 1040 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - | 1041 | ` */` |
|   2640250 | 1042 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 | 1043 | `{` |
|   2640255 | 1044 | `	pFrame->bHasRet = 0;` |
|   2640255 | 1045 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   2640255 | 1046 | `	pFrame->nCatchJmpPc = 0;` |
|   2640255 | 1047 | `}` |
|         - | 1048 | `/*` |
|         - | 1049 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - | 1050 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - | 1051 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - | 1052 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - | 1053 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - | 1054 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - | 1055 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - | 1056 | ` */` |
|     20254 | 1057 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 | 1058 | `{` |
|     20259 | 1059 | `	if( pResult ){` |
|     20259 | 1060 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     10127 | 1061 | `	}` |
|     20259 | 1062 | `	VmClearFramePending(pEntryFrame);` |
|     20263 | 1063 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|         5 | 1064 | `		VmLeaveFrame(&(*pVm));` |
|         1 | 1065 | `	}` |
|     20259 | 1066 | `}` |
|         - | 1067 | `/*` |
|         - | 1068 | ` * Compare two functions signature and return the comparison result.` |
|         - | 1069 | ` */` |
|      1186 | 1070 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         2 | 1071 | `{` |
|      1188 | 1072 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      1188 | 1073 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      1188 | 1074 | `	const char *zSin = pSecond->zString;` |
|      1188 | 1075 | `	const char *zFin = pFirst->zString;` |
|      1188 | 1076 | `	const char *zPtr = zFin;` |
|       593 | 1077 | `	for(;;){` |
|      1188 | 1078 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|       595 | 1079 | `			break;` |
|         - | 1080 | `		}` |
|       ! 0 | 1081 | `		if( zFin[0] != zSin[0] ){` |
|         - | 1082 | `			/* mismatch */` |
|       ! 0 | 1083 | `			break;` |
|         - | 1084 | `		}` |
|       ! 0 | 1085 | `		zFin++;` |
|       ! 0 | 1086 | `		zSin++;` |
|       ! 0 | 1087 | `	}` |
|      1188 | 1088 | `	return (int)(zFin-zPtr);` |
|         2 | 1089 | `}` |
|         - | 1090 | `/*` |
|         - | 1091 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1092 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1093 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1094 | ` * Refer to the official documentation for more information.` |
|         - | 1095 | ` */` |
|       284 | 1096 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1097 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1098 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1099 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1100 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1101 | `	)` |
|         4 | 1102 | `{` |
|         - | 1103 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1104 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1105 | `	ph7_vm_func *pLink;` |
|         - | 1106 | `	SyString sArgSig;` |
|         - | 1107 | `	SyBlob sSig;` |
|         - | 1108 |  |
|       288 | 1109 | `	pLink = pList;` |
|       288 | 1110 | `	i = 0;` |
|         - | 1111 | `	/* Put functions expecting the same number of passed arguments */` |
|      1890 | 1112 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      1822 | 1113 | `		if( pLink == 0 ){` |
|       219 | 1114 | `			break;` |
|         - | 1115 | `		}` |
|      1606 | 1116 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1117 | `			/* Candidate for overloading */` |
|      1606 | 1118 | `			apSet[i++] = pLink;` |
|       801 | 1119 | `		}` |
|         - | 1120 | `		/* Point to the next entry */` |
|      1606 | 1121 | `		pLink = pLink->pNextName;` |
|         4 | 1122 | `	}` |
|       288 | 1123 | `	if( i < 1 ){` |
|         - | 1124 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1125 | `		return pList;` |
|         - | 1126 | `	}` |
|       288 | 1127 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1128 | `		/* Return the only candidate */` |
|        60 | 1129 | `		return apSet[0];` |
|         - | 1130 | `	}` |
|         - | 1131 | `	/* Calculate function signature */` |
|       230 | 1132 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|       458 | 1133 | `	for( j = 0 ; j < nArg ; j++ ){` |
|       230 | 1134 | `		int c = 'n'; /* null */` |
|       230 | 1135 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1136 | `			/* Hashmap */` |
|       ! 0 | 1137 | `			c = 'h';` |
|       230 | 1138 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1139 | `			/* bool */` |
|        85 | 1140 | `			c = 'b';` |
|       188 | 1141 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1142 | `			/* int */` |
|        48 | 1143 | `			c = 'i';` |
|       122 | 1144 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1145 | `			/* String */` |
|        87 | 1146 | `			c = 's';` |
|        56 | 1147 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1148 | `			/* Float */` |
|        11 | 1149 | `			c = 'f';` |
|         8 | 1150 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1151 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1152 | `			int marker = 'o';` |
|       ! 0 | 1153 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1154 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1155 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1156 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1157 | `			c = -1;` |
|       ! 0 | 1158 | `		}` |
|       230 | 1159 | `		if( c > 0 ){` |
|       230 | 1160 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       114 | 1161 | `		}` |
|       116 | 1162 | `	}` |
|       230 | 1163 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|       230 | 1164 | `	iTarget = 0;` |
|       230 | 1165 | `	iMax = -1;` |
|         - | 1166 | `	/* Select the appropriate function */` |
|      1416 | 1167 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1168 | `		/* Compare the two signatures */` |
|      1188 | 1169 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      1188 | 1170 | `		if( iCur > iMax ){` |
|       230 | 1171 | `			iMax = iCur;` |
|       230 | 1172 | `			iTarget = j;` |
|       114 | 1173 | `		}` |
|       595 | 1174 | `	}` |
|       230 | 1175 | `	SyBlobRelease(&sSig);` |
|         - | 1176 | `	/* Appropriate function for the current call context */` |
|       230 | 1177 | `	return apSet[iTarget];` |
|       146 | 1178 | `}` |
|         - | 1179 | `/* Forward declaration */` |
|         - | 1180 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1181 | `/*` |
|         - | 1182 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|         - | 1183 | ` * safely across a pool reallocation.` |
|         - | 1184 | ` *` |
|         - | 1185 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1186 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|         - | 1187 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|         - | 1188 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|         - | 1189 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|         - | 1190 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|         - | 1191 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|         - | 1192 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|         - | 1193 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|         - | 1194 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|         - | 1195 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|         - | 1196 | ` */` |
|      2736 | 1197 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1198 | `{` |
|         - | 1199 | `	ph7_value sVal;` |
|      2741 | 1200 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1201 | `	sxi32 rc;` |
|      2741 | 1202 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|      2741 | 1203 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1204 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|      2741 | 1205 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      2741 | 1206 | `	if( *ppMemObj ){` |
|      2741 | 1207 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|      1368 | 1208 | `	}` |
|      2741 | 1209 | `	PH7_MemObjRelease(&sVal);` |
|      2741 | 1210 | `	return rc;` |
|         5 | 1211 | `}` |
|         - | 1212 | `/*` |
|         - | 1213 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|         - | 1214 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|         - | 1215 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|         - | 1216 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|         - | 1217 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|         - | 1218 | ` * C caller ever regains control.` |
|         - | 1219 | ` */` |
|         - | 1220 | `typedef struct VmMuteState {` |
|         - | 1221 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|         - | 1222 | `	sxu32 nSaved;` |
|         - | 1223 | `	sxi32 iSaveStatus;` |
|         - | 1224 | `	sxi32 iSaveBoundary;` |
|         - | 1225 | `	VmFrame *pSaveResume;` |
|         - | 1226 | `	ph7_class_attr *pSaveCycleAttr;` |
|         - | 1227 | `	ph7_class *pSaveCycleClass;` |
|         - | 1228 | `} VmMuteState;` |
|       244 | 1229 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|         5 | 1230 | `{` |
|       249 | 1231 | `	pSave->apSaved = 0;` |
|       249 | 1232 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|       249 | 1233 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|       249 | 1234 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|       249 | 1235 | `	pSave->pSaveResume = pVm->pResumeFrame;` |
|       249 | 1236 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|       249 | 1237 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|       249 | 1238 | `	if( pSave->nSaved > 0 ){` |
|        34 | 1239 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        16 | 1240 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|        18 | 1241 | `		if( pSave->apSaved ){` |
|        26 | 1242 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|        16 | 1243 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|        18 | 1244 | `			SySetReset(&pVm->aException);` |
|         8 | 1245 | `		}` |
|         8 | 1246 | `	}` |
|       249 | 1247 | `	pVm->nMuteThrow++;` |
|       249 | 1248 | `}` |
|         - | 1249 | `/*` |
|         - | 1250 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|         - | 1251 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|         - | 1252 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|         - | 1253 | ` * TRUE when a throw was actually swallowed.` |
|         - | 1254 | ` */` |
|       244 | 1255 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|         5 | 1256 | `{` |
|         - | 1257 | `	VmFrame *pFrame;` |
|       249 | 1258 | `	pVm->nMuteThrow--;` |
|         - | 1259 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|         - | 1260 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|         - | 1261 | `	 * activation behind: release whatever is there whether or not anything was` |
|         - | 1262 | `	 * hidden. */` |
|       249 | 1263 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|       249 | 1264 | `	SySetReset(&pVm->aException);` |
|       249 | 1265 | `	if( pSave->apSaved ){` |
|         - | 1266 | `		sxu32 k;` |
|        34 | 1267 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|        18 | 1268 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|        10 | 1269 | `		}` |
|        18 | 1270 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|        18 | 1271 | `		pSave->apSaved = 0;` |
|         8 | 1272 | `	}` |
|       249 | 1273 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|       203 | 1274 | `		return FALSE;` |
|         - | 1275 | `	}` |
|        50 | 1276 | `	pFrame = pVm->pFrame;` |
|        50 | 1277 | `	if( pFrame ){` |
|        50 | 1278 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        50 | 1279 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        23 | 1280 | `	}` |
|        50 | 1281 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|        50 | 1282 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|        50 | 1283 | `	pVm->pResumeFrame = pSave->pSaveResume;` |
|        50 | 1284 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|        50 | 1285 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|        50 | 1286 | `	return TRUE;` |
|       127 | 1287 | `}` |
|         - | 1288 | `/*` |
|         - | 1289 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|         - | 1290 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|         - | 1291 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|         - | 1292 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|         - | 1293 | ` *` |
|         - | 1294 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|         - | 1295 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|         - | 1296 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|         - | 1297 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|         - | 1298 | ` */` |
|        12 | 1299 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|         - | 1300 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 1301 | `	ph7_class_instance *pThis,   /* Receiver */` |
|         - | 1302 | `	ph7_class_method *pMethod,   /* Method to run */` |
|         - | 1303 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|         - | 1304 | `	int nArg,                    /* Argument count */` |
|         - | 1305 | `	ph7_value **apArg,           /* Arguments */` |
|         - | 1306 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|         - | 1307 | `	)` |
|         1 | 1308 | `{` |
|         - | 1309 | `	VmMuteState sSave;` |
|         - | 1310 | `	sxi32 rc;` |
|        13 | 1311 | `	VmMuteEnter(&(*pVm),&sSave);` |
|        13 | 1312 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|        13 | 1313 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         3 | 1314 | `		if( pbThrew ){` |
|         3 | 1315 | `			*pbThrew = TRUE;` |
|         1 | 1316 | `		}` |
|         3 | 1317 | `		return SXRET_OK;` |
|         - | 1318 | `	}` |
|        11 | 1319 | `	if( pbThrew ){` |
|        11 | 1320 | `		*pbThrew = FALSE;` |
|         5 | 1321 | `	}` |
|        11 | 1322 | `	return rc;` |
|         7 | 1323 | `}` |
|         - | 1324 | `/*` |
|         - | 1325 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - | 1326 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - | 1327 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - | 1328 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - | 1329 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - | 1330 | ` *` |
|         - | 1331 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - | 1332 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - | 1333 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - | 1334 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - | 1335 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - | 1336 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - | 1337 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - | 1338 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - | 1339 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - | 1340 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 1341 | ` */` |
|       232 | 1342 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 | 1343 | `{` |
|         - | 1344 | `	VmMuteState sSave;` |
|         - | 1345 | `	sxi32 rc;` |
|       237 | 1346 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       237 | 1347 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|       237 | 1348 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|         - | 1349 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - | 1350 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - | 1351 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - | 1352 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - | 1353 | `		 * records the cycle again and raises it there. */` |
|       ! 0 | 1354 | `		rc = PH7_EXCEPTION;` |
|       ! 0 | 1355 | `	}` |
|         - | 1356 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|         - | 1357 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|         - | 1358 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|         - | 1359 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|         - | 1360 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|         - | 1361 | `	 * detects the cycle again. */` |
|       237 | 1362 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|       237 | 1363 | `	return rc;` |
|         5 | 1364 | `}` |
|         - | 1365 | `/*` |
|         - | 1366 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1367 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1368 | ` */` |
|         - | 1369 | `/*` |
|         - | 1370 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1371 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1372 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1373 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1374 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1375 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1376 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1377 | ` */` |
|   1474934 | 1378 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1379 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1380 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1381 | `	)` |
|         5 | 1382 | `{` |
|         - | 1383 | `	ph7_class_attr *pAttr;` |
|         - | 1384 | `	SyHashEntry *pEntry;` |
|         - | 1385 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1386 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1387 | `	 * over both tables. */` |
|         - | 1388 | `	SyHash *apMount[2];` |
|         - | 1389 | `	int iMount;` |
|   1474939 | 1390 | `	apMount[0] = &pClass->hAttr;` |
|   1474939 | 1391 | `	apMount[1] = &pClass->hConst;` |
|   4424801 | 1392 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1393 | `	/* Reset the loop cursor */` |
|   2949873 | 1394 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1395 | `	/* Process only static and constant attribute */` |
|   8988249 | 1396 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1397 | `		/* Extract the current attribute */` |
|   6038387 | 1398 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   6038382 | 1399 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   3799792 | 1400 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    781129 | 1401 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1402 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1403 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1404 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1405 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1406 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1407 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1408 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1409 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1410 | `			 * %s as value for class constant" fatal without any access). */` |
|   1560131 | 1411 | `			continue;` |
|         - | 1412 | `		}` |
|   4478261 | 1413 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1414 | `			ph7_value *pMemObj;` |
|      1307 | 1415 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1416 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1417 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1418 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1419 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - | 1420 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - | 1421 | `				 * access / instantiation throws like php's. */` |
|      1048 | 1422 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        26 | 1423 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - | 1424 | `						/* Its default threw at the other class's mount and is` |
|         - | 1425 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - | 1426 | `						 * both static tables). */` |
|         3 | 1427 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1428 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        11 | 1429 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|         6 | 1430 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|         8 | 1431 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1432 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|         1 | 1433 | `						}` |
|         3 | 1434 | `					}` |
|        11 | 1435 | `				}` |
|      1051 | 1436 | `				continue;` |
|         - | 1437 | `			}` |
|         - | 1438 | `			/* Reserve a memory object for this constant/static attribute */` |
|       263 | 1439 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       263 | 1440 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1441 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 1442 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 1443 | `					&pClass->sName,&pAttr->sName` |
|         - | 1444 | `					);` |
|       ! 0 | 1445 | `				return SXERR_MEM;` |
|         - | 1446 | `			}` |
|       263 | 1447 | `			if( pAttr->pNativeValue ){` |
|         - | 1448 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 | 1449 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|       263 | 1450 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1451 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1452 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 1453 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|       237 | 1454 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       237 | 1455 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - | 1456 | `				sxi32 rcExec;` |
|       237 | 1457 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       237 | 1458 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|       237 | 1459 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|       237 | 1460 | `				pVm->nConstEvalDepth++;` |
|         - | 1461 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - | 1462 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - | 1463 | `				 * not something it can see. What reaches this line is a static` |
|         - | 1464 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - | 1465 | `				 * which php does validate here, but only when the initializer` |
|         - | 1466 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - | 1467 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - | 1468 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|       237 | 1469 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|       237 | 1470 | `				pVm->nConstEvalDepth--;` |
|       237 | 1471 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       237 | 1472 | `				pVm->pConstEvalClass = pSaveCtx;` |
|       237 | 1473 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1474 | `					/* php has not reached this initializer: defer it whole to the` |
|         - | 1475 | `					 * first USE, where the throw is raised at the access site and is` |
|         - | 1476 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - | 1477 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - | 1478 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - | 1479 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - | 1480 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - | 1481 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - | 1482 | `					 * unset nIdx. */` |
|        47 | 1483 | `					if( bStaticProp ){` |
|        41 | 1484 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        41 | 1485 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1486 | `					}else{` |
|         - | 1487 | `						VmSlot sSlot;` |
|         - | 1488 | `						/* Release before recycling: PH7_ReserveMemObj re-inits a` |
|         - | 1489 | `						 * reused slot without releasing it, and a muted eval that` |
|         - | 1490 | `						 * only recorded a CYCLE still left its value here. */` |
|         8 | 1491 | `						sSlot.nIdx = pMemObj->nIdx;` |
|         8 | 1492 | `						sSlot.pUserData = 0;` |
|         8 | 1493 | `						PH7_MemObjRelease(pMemObj);` |
|         8 | 1494 | `						SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|         8 | 1495 | `						continue;` |
|         - | 1496 | `					}` |
|       210 | 1497 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|        99 | 1498 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1499 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 1500 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - | 1501 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        41 | 1502 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        41 | 1503 | `					if( rcType != SXRET_OK ){` |
|         8 | 1504 | `						return rcType;` |
|         - | 1505 | `					}` |
|        16 | 1506 | `				}` |
|       110 | 1507 | `			}` |
|         - | 1508 | `			/* Record attribute index */` |
|       251 | 1509 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 1510 | `			/* Install static attribute in the reference table */` |
|       251 | 1511 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1512 | `			/* If this is a typed static property, register the slot so the` |
|         - | 1513 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 1514 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 1515 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 1516 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 1517 | `			 * already enforced above, so they need no store-time slot. */` |
|       246 | 1518 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       176 | 1519 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        69 | 1520 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        69 | 1521 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 1522 | `					return SXERR_MEM;` |
|         - | 1523 | `				}` |
|        69 | 1524 | `				pVmAttrS->pAttr = pAttr;` |
|        69 | 1525 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        69 | 1526 | `				pVmAttrS->iState = 0;` |
|        69 | 1527 | `				pVmAttrS->pOwner = pClass;` |
|         - | 1528 | `				/* Static typed property with no default starts uninitialized` |
|         - | 1529 | `				 * (constants are already excluded by the enclosing condition). */` |
|        69 | 1530 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        18 | 1531 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        61 | 1532 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - | 1533 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 1534 | `					 * typed static default LAZILY at the first static-property` |
|         - | 1535 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 1536 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 1537 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 1538 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 1539 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 1540 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - | 1541 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - | 1542 | `					 * the materializer checks it after the re-run. */` |
|        54 | 1543 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        24 | 1544 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        24 | 1545 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        11 | 1546 | `					}` |
|        25 | 1547 | `				}` |
|        69 | 1548 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 1549 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 1550 | `					return SXERR_MEM;` |
|         - | 1551 | `				}` |
|        32 | 1552 | `			}` |
|       123 | 1553 | `		}` |
|         5 | 1554 | `	}` |
|   1474936 | 1555 | `	} /* for iMount */` |
|   1474933 | 1556 | `	return SXRET_OK;` |
|    737472 | 1557 | `}` |
|         - | 1558 | `/*` |
|         - | 1559 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - | 1560 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - | 1561 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - | 1562 | ` */` |
|   1474394 | 1563 | `static sxi32 VmMountUserClassMethods(` |
|         - | 1564 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1565 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - | 1566 | `	)` |
|         5 | 1567 | `{` |
|         - | 1568 | `	ph7_class_method *pMeth;` |
|         - | 1569 | `	SyHashEntry *pEntry;` |
|         - | 1570 | `	sxi32 rc;` |
|         - | 1571 | `	/* Install class methods */` |
|   1474399 | 1572 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 1573 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 1574 | `		 */` |
|    456761 | 1575 | `		return SXRET_OK;` |
|         - | 1576 | `	}` |
|         - | 1577 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 1578 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 1579 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 1580 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 1581 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 1582 | `	/* Install the methods now */` |
|   1017643 | 1583 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  15876778 | 1584 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  14350321 | 1585 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  14350321 | 1586 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  14315291 | 1587 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  14315291 | 1588 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1589 | `				return rc;` |
|         - | 1590 | `			}` |
|   7157643 | 1591 | `		}` |
|         5 | 1592 | `	}` |
|         - | 1593 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   1017643 | 1594 | `	pClass->bMounted = TRUE;` |
|   1017643 | 1595 | `	return SXRET_OK;` |
|    737202 | 1596 | `}` |
|    926984 | 1597 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 1598 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1599 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 1600 | `	)` |
|         5 | 1601 | `{` |
|         - | 1602 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - | 1603 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - | 1604 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - | 1605 | `	 * initializer that throws finds the exception classes ready. */` |
|    926989 | 1606 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|    926989 | 1607 | `	if( rc != SXRET_OK ){` |
|         3 | 1608 | `		return rc;` |
|         - | 1609 | `	}` |
|    926987 | 1610 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|    463497 | 1611 | `}` |
|         - | 1612 | `/*` |
|         - | 1613 | ` * Allocate a private frame for attributes of the given` |
|         - | 1614 | ` * class instance (Object in the PHP jargon).` |
|         - | 1615 | ` */` |
|   1574342 | 1616 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 1617 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 1618 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 1619 | `	)` |
|         5 | 1620 | `{` |
|   1574347 | 1621 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 1622 | `	ph7_class_attr *pAttr;` |
|         - | 1623 | `	SyHashEntry *pEntry;` |
|         - | 1624 | `	sxi32 rc;` |
|   1574347 | 1625 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 1626 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 1627 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 1628 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1574347 | 1629 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  11808651 | 1630 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 1631 | `		VmClassAttr *pVmAttr;` |
|         - | 1632 | `		/* Extract the current attribute */` |
|  10234309 | 1633 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  10234309 | 1634 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|  10234309 | 1635 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 1636 | `			return SXERR_MEM;` |
|         - | 1637 | `		}` |
|  10234309 | 1638 | `		pVmAttr->pAttr = pAttr;` |
|  10234309 | 1639 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 1640 | `			ph7_value *pMemObj;` |
|         - | 1641 | `			/* Reserve a memory object for this attribute */` |
|  10234191 | 1642 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|  10234191 | 1643 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1644 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1645 | `				return SXERR_MEM;` |
|         - | 1646 | `			}` |
|  10234191 | 1647 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|  10234191 | 1648 | `			pVmAttr->iState = 0;` |
|  10234191 | 1649 | `			pVmAttr->pOwner = pClass;` |
|  10234191 | 1650 | `			if( pAttr->pNativeValue ){` |
|         - | 1651 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - | 1652 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - | 1653 | `				 * literal cannot throw and the builder states the type itself. */` |
|   9772801 | 1654 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   5347793 | 1655 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1656 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1657 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 1658 | `				 * against the declaring class (no method frame here). */` |
|      2083 | 1659 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1660 | `				sxi32 rcExec;` |
|      2083 | 1661 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      2083 | 1662 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      2083 | 1663 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      2083 | 1664 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1665 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 1666 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 1667 | `					 * also type-check the leftover value (a spurious second` |
|         - | 1668 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 1669 | `					 * nothing further for the remaining attributes.` |
|         - | 1670 | `					 *` |
|         - | 1671 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - | 1672 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - | 1673 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - | 1674 | `					 * and the throw came back as a status nobody read: this` |
|         - | 1675 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - | 1676 | `					 * and the whole statement RESUMED after the catch — php` |
|         - | 1677 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - | 1678 | `					 * existing construction-aborted route. */` |
|        24 | 1679 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        24 | 1680 | `					bDefThrew = 1;` |
|      2072 | 1681 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1682 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 1683 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 1684 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 1685 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 1686 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 1687 | `					 * aborts construction) / the fetch-point router. */` |
|       295 | 1688 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       295 | 1689 | `					if( rcDef != SXRET_OK ){` |
|        13 | 1690 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 1691 | `						bDefThrew = 1;` |
|         6 | 1692 | `					}` |
|       150 | 1693 | `				}` |
|    460356 | 1694 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 1695 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 1696 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|    458425 | 1697 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|    229210 | 1698 | `			}` |
|  10234191 | 1699 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|  10234191 | 1700 | `			if( rc != SXRET_OK ){` |
|         - | 1701 | `				VmSlot sSlot;` |
|         - | 1702 | `				/* Restore memory object */` |
|       ! 0 | 1703 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1704 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1705 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1706 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1707 | `				return SXERR_MEM;` |
|         - | 1708 | `			}` |
|         - | 1709 | `			/* Install attribute in the reference table */` |
|  10234191 | 1710 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1711 | `			/* Register typed property slot for assignment-time enforcement.` |
|         - | 1712 | `			 * On failure roll back the just-installed hAttr entry and the` |
|         - | 1713 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|  10234191 | 1714 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|   7282005 | 1715 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|   7282005 | 1716 | `				if( rc != SXRET_OK ){` |
|         - | 1717 | `					VmSlot sSlot;` |
|       ! 0 | 1718 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1719 | `					sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1720 | `					sSlot.pUserData = 0;` |
|       ! 0 | 1721 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1722 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1723 | `					return SXERR_MEM;` |
|         - | 1724 | `				}` |
|   3641000 | 1725 | `			}` |
|   5117098 | 1726 | `		}else{` |
|         - | 1727 | `			/* Install static/constant attribute */` |
|       123 | 1728 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       123 | 1729 | `			pVmAttr->iState = 0;` |
|       123 | 1730 | `			pVmAttr->pOwner = pClass;` |
|       123 | 1731 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       123 | 1732 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1733 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1734 | `				return SXERR_MEM;` |
|         - | 1735 | `			}` |
|         - | 1736 | `		}` |
|         5 | 1737 | `	}` |
|   1574347 | 1738 | `	return SXRET_OK;` |
|    787176 | 1739 | `}` |
|         - | 1740 | `/*` |
|         - | 1741 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|         - | 1742 | ` * stdClass for now; the future general-dynamic-props work turns` |
|         - | 1743 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|         - | 1744 | ` */` |
|       160 | 1745 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         4 | 1746 | `{` |
|       164 | 1747 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|         4 | 1748 | `}` |
|         - | 1749 | `/*` |
|         - | 1750 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 1751 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 1752 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 1753 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 1754 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 1755 | ` */` |
|        22 | 1756 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         3 | 1757 | `{` |
|        35 | 1758 | `	while( pClass ){` |
|        25 | 1759 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 1760 | `		sxu32 n;` |
|        25 | 1761 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|        12 | 1762 | `			if( aAttr[n].sName.nByte == nName` |
|        13 | 1763 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|        13 | 1764 | `				return TRUE;` |
|         - | 1765 | `			}` |
|       ! 0 | 1766 | `		}` |
|        12 | 1767 | `		pClass = pClass->pBase;` |
|         2 | 1768 | `	}` |
|        12 | 1769 | `	return FALSE;` |
|        14 | 1770 | `}` |
|         - | 1771 | `/*` |
|         - | 1772 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|         - | 1773 | ` * access or method call on such an instance is php's incomplete-object` |
|         - | 1774 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|         - | 1775 | ` * (array), get_object_vars) read its attribute table freely.` |
|         - | 1776 | ` */` |
|    222389 | 1777 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1778 | `{` |
|    222394 | 1779 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|         5 | 1780 | `}` |
|         - | 1781 | `/*` |
|         - | 1782 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|         - | 1783 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|         - | 1784 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|         - | 1785 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|         - | 1786 | ` * had one says "unknown", like php.` |
|         - | 1787 | ` */` |
|        44 | 1788 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|         1 | 1789 | `{` |
|        45 | 1790 | `	const char *zName = "unknown";` |
|        45 | 1791 | `	sxu32 nName = sizeof("unknown")-1;` |
|        45 | 1792 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|         - | 1793 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|        45 | 1794 | `	if( pEntry ){` |
|        45 | 1795 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        45 | 1796 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|        45 | 1797 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|        45 | 1798 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|        45 | 1799 | `			nName = SyBlobLength(&pVal->sBlob);` |
|        22 | 1800 | `		}` |
|        22 | 1801 | `	}` |
|        67 | 1802 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|         - | 1803 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|         - | 1804 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|        22 | 1805 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|        45 | 1806 | `}` |
|         - | 1807 | `/*` |
|         - | 1808 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|         - | 1809 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|         - | 1810 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|         - | 1811 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|         - | 1812 | ` * (property_exists() passes its name in pFuncName).` |
|         - | 1813 | ` */` |
|        16 | 1814 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|         1 | 1815 | `{` |
|         - | 1816 | `	SyBlob sMsg;` |
|        17 | 1817 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        17 | 1818 | `	if( pFuncName ){` |
|       ! 0 | 1819 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|       ! 0 | 1820 | `	}else{` |
|        17 | 1821 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|        17 | 1822 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|        17 | 1823 | `		if( pFunc == 0 ){` |
|       ! 0 | 1824 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|       ! 0 | 1825 | `		}else{` |
|        17 | 1826 | `			const char *zDisp = 0;` |
|         - | 1827 | `			int nDisp;` |
|        17 | 1828 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 | 1829 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         3 | 1830 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|         3 | 1831 | `				SyBlobAppend(&sMsg,"::",2);` |
|         1 | 1832 | `			}` |
|        17 | 1833 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|        17 | 1834 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|         - | 1835 | `		}` |
|         - | 1836 | `	}` |
|        17 | 1837 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|        17 | 1838 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|        25 | 1839 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|        16 | 1840 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        17 | 1841 | `	SyBlobRelease(&sMsg);` |
|        17 | 1842 | `}` |
|         - | 1843 | `/*` |
|         - | 1844 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 1845 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 1846 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 1847 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 1848 | ` *` |
|         - | 1849 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 1850 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 1851 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 1852 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 1853 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 1854 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 1855 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 1856 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 1857 | ` */` |
|       364 | 1858 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         4 | 1859 | `{` |
|         - | 1860 | `	ph7_class_attr *pAttr;` |
|       368 | 1861 | `	VmClassAttr *pVmAttr = 0;` |
|       368 | 1862 | `	ph7_value *pMemObj = 0;` |
|         - | 1863 | `	char *zCopy;` |
|         - | 1864 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       368 | 1865 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       368 | 1866 | `	if( pAttr == 0 ){` |
|       ! 0 | 1867 | `		return 0;` |
|         - | 1868 | `	}` |
|       368 | 1869 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       368 | 1870 | `	zCopy = (char *)&pAttr[1];` |
|       368 | 1871 | `	if( nName > 0 ){` |
|       360 | 1872 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       178 | 1873 | `	}` |
|       368 | 1874 | `	zCopy[nName] = 0;` |
|       368 | 1875 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       368 | 1876 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       368 | 1877 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       368 | 1878 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 1879 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 1880 | `	 * value, never a union. */` |
|       368 | 1881 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       368 | 1882 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1883 | `		goto fail_attr;` |
|         - | 1884 | `	}` |
|       368 | 1885 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       368 | 1886 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1887 | `		goto fail_vmattr;` |
|         - | 1888 | `	}` |
|       368 | 1889 | `	pVmAttr->pAttr = pAttr;` |
|       368 | 1890 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       368 | 1891 | `	pVmAttr->iState = 0;` |
|       368 | 1892 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1893 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 1894 | `	 * property-creation order, matching PHP. */` |
|       368 | 1895 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 1896 | `		goto fail_slot;` |
|         - | 1897 | `	}` |
|         - | 1898 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       368 | 1899 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|       368 | 1900 | `	if( ppAttr ){` |
|       142 | 1901 | `		*ppAttr = pVmAttr;` |
|        69 | 1902 | `	}` |
|       368 | 1903 | `	return pMemObj;` |
|       ! 0 | 1904 | `fail_slot:` |
|         - | 1905 | `	{` |
|         - | 1906 | `		VmSlot sSlot;` |
|       ! 0 | 1907 | `		sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1908 | `		sSlot.pUserData = 0;` |
|       ! 0 | 1909 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1910 | `	}` |
|       ! 0 | 1911 | `fail_vmattr:` |
|       ! 0 | 1912 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1913 | `fail_attr:` |
|       ! 0 | 1914 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 1915 | `	return 0;` |
|       186 | 1916 | `}` |
|         - | 1917 | `/*` |
|         - | 1918 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 1919 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 1920 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 1921 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 1922 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 1923 | ` */` |
|         8 | 1924 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         1 | 1925 | `{` |
|         - | 1926 | `	VmClassAttr *pVmAttr;` |
|         - | 1927 | `	ph7_value *pMemObj;` |
|         9 | 1928 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|         9 | 1929 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1930 | `		return;` |
|         - | 1931 | `	}` |
|         9 | 1932 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|         9 | 1933 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1934 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1935 | `		return;` |
|         - | 1936 | `	}` |
|         9 | 1937 | `	pVmAttr->pAttr = pAttr;` |
|         9 | 1938 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|         9 | 1939 | `	pVmAttr->iState = 0;` |
|         9 | 1940 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1941 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 1942 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 1943 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 1944 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 1945 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|         9 | 1946 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       ! 0 | 1947 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       ! 0 | 1948 | `	}` |
|         - | 1949 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 1950 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 1951 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 1952 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|         9 | 1953 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|         - | 1954 | `		VmSlot sSlot;` |
|       ! 0 | 1955 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1956 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1957 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1958 | `		return;` |
|         - | 1959 | `	}` |
|         9 | 1960 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         9 | 1961 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       ! 0 | 1962 | `		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|         - | 1963 | `			VmSlot sSlot;` |
|       ! 0 | 1964 | `			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1965 | `			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1966 | `			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1967 | `			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1968 | `			return;` |
|         - | 1969 | `		}` |
|       ! 0 | 1970 | `	}` |
|         9 | 1971 | `	if( ppAttr ){` |
|         9 | 1972 | `		*ppAttr = pVmAttr;` |
|         4 | 1973 | `	}` |
|         5 | 1974 | `}` |
|         - | 1975 | `/* Forward declaration */` |
|         - | 1976 | `/*` |
|         - | 1977 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 1978 | ` */` |
|         - | 1979 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 1980 | `/*` |
|         - | 1981 | ` * Reserve a constant memory object.` |
|         - | 1982 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 1983 | ` */` |
|   1231154 | 1984 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 1985 | `{` |
|         - | 1986 | `	ph7_value *pObj;` |
|         - | 1987 | `	sxi32 rc;` |
|   1231159 | 1988 | `	if( pIndex ){` |
|         - | 1989 | `		/* Object index in the object table */` |
|   1217149 | 1990 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    608572 | 1991 | `	}` |
|         - | 1992 | `	/* Reserve a slot for the new object */` |
|   1231159 | 1993 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   1231159 | 1994 | `	if( rc != SXRET_OK ){` |
|         - | 1995 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1996 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1997 | `		 */` |
|       ! 0 | 1998 | `		return 0;` |
|         - | 1999 | `	}` |
|   1231159 | 2000 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   1231159 | 2001 | `	return pObj;` |
|    615582 | 2002 | `}` |
|         - | 2003 | `/*` |
|         - | 2004 | ` * Reserve a memory object.` |
|         - | 2005 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2006 | ` */` |
|   2938052 | 2007 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2008 | `{` |
|         - | 2009 | `	ph7_value *pObj;` |
|         - | 2010 | `	sxi32 rc;` |
|   2938057 | 2011 | `	if( pIndex ){` |
|         - | 2012 | `		/* Object index in the object table */` |
|   2938057 | 2013 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|   1469026 | 2014 | `	}` |
|         - | 2015 | `	/* Reserve a slot for the new object */` |
|   2938057 | 2016 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|   2938057 | 2017 | `	if( rc != SXRET_OK ){` |
|         - | 2018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2020 | `		 */` |
|       ! 0 | 2021 | `		return 0;` |
|         - | 2022 | `	}` |
|   2938057 | 2023 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|   2938057 | 2024 | `	return pObj;` |
|   1469031 | 2025 | `}` |
|         - | 2026 | `/* Forward declaration */` |
|         - | 2027 | `/* Forward declarations for Fiber C functions */` |
|         - | 2028 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 2029 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 2030 | `/*` |
|         - | 2031 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 2032 | ` * directly as foreign functions.` |
|         - | 2033 | ` */` |
|         - | 2034 |  |
|         - | 2035 | `/*` |
|         - | 2036 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 2037 | ` * start compiling the target PHP program.` |
|         - | 2038 | ` */` |
|      4670 | 2039 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 2040 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 2041 | `	 ph7 *pEngine /* Master engine */` |
|         - | 2042 | `	 )` |
|         5 | 2043 | `{` |
|         - | 2044 | `	ph7_value *pObj;` |
|         - | 2045 | `	sxi32 rc;` |
|         - | 2046 | `	/* Zero the structure */` |
|      4675 | 2047 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 2048 | `	/* Initialize VM fields */` |
|      4675 | 2049 | `	pVm->pEngine = &(*pEngine);` |
|      4675 | 2050 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|         - | 2051 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 2052 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 2053 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      4675 | 2054 | `	pVm->bDisplayErrors = 0;` |
|      4675 | 2055 | `	pVm->bLogErrors = 1;` |
|      4675 | 2056 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 2057 | `	/* Instructions containers */` |
|      4675 | 2058 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      4675 | 2059 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      4675 | 2060 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 2061 | `	/* Object containers */` |
|      4675 | 2062 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      4675 | 2063 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|         - | 2064 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      4675 | 2065 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      4675 | 2066 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      4675 | 2067 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      4675 | 2068 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 2069 | `	/* Virtual machine internal containers */` |
|      4675 | 2070 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      4675 | 2071 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      4675 | 2072 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      4675 | 2073 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|      4675 | 2074 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      4675 | 2075 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 2076 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 2077 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 2078 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 2079 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 2080 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 2081 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 2082 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 2083 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      4675 | 2084 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4675 | 2085 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4675 | 2086 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4675 | 2087 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      4675 | 2088 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      4675 | 2089 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      4675 | 2090 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|      4675 | 2091 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      4675 | 2092 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      4675 | 2093 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      4675 | 2094 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|      4675 | 2095 | `	pVm->bIniSeeded = 0;` |
|      4675 | 2096 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|      4675 | 2097 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|      4675 | 2098 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|      4675 | 2099 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|      4675 | 2100 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|      4675 | 2101 | `	pVm->bSessWired = 0;` |
|      4675 | 2102 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      4675 | 2103 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      4675 | 2104 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      4675 | 2105 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|      4675 | 2106 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      4675 | 2107 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      4675 | 2108 | `	pVm->nResourceIdNext = 1;` |
|      4675 | 2109 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      4675 | 2110 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      4675 | 2111 | `	pVm->pMagicSetThis = 0;` |
|      4675 | 2112 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      4675 | 2113 | `	pVm->pHookSetThis = 0;` |
|      4675 | 2114 | `	pVm->pHookSetAttr = 0;` |
|      4675 | 2115 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      4675 | 2116 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      4675 | 2117 | `	pVm->pMagicCallThis = 0;` |
|      4675 | 2118 | `	pVm->pMagicCallClass = 0;` |
|      4675 | 2119 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      4675 | 2120 | `	pVm->pIdleCallFrames = 0;` |
|      4675 | 2121 | `	pVm->pIdleOperandStacks = 0;` |
|      4675 | 2122 | `	pVm->nIdleOperandStacks = 0;` |
|      4675 | 2123 | `	pVm->pIdleStackNodes = 0;` |
|      4675 | 2124 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      4675 | 2125 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      4675 | 2126 | `	pVm->pPendingException = 0;` |
|      4675 | 2127 | `	pVm->pInflightException = 0;` |
|      4675 | 2128 | `	pVm->nInflightExcBase = 0;` |
|      4675 | 2129 | `	pVm->pResumeFrame = 0;` |
|      4675 | 2130 | `	pVm->iResumePc = 0;` |
|      4675 | 2131 | `	pVm->pResumeInstr = 0;` |
|      4675 | 2132 | `	pVm->iResumeStackDepth = 0;` |
|      4675 | 2133 | `	pVm->nBoundaryRc = 0;` |
|      4675 | 2134 | `	pVm->pConstEvalClass = 0;` |
|      4675 | 2135 | `	pVm->nConstEvalDepth = 0;` |
|      4675 | 2136 | `	pVm->pConstCycleAttr = 0;` |
|      4675 | 2137 | `	pVm->pConstCycleClass = 0;` |
|      4675 | 2138 | `	SySetReset(&pVm->aMagicGuard);` |
|      4675 | 2139 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2140 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2141 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2142 | `	}` |
|      4675 | 2143 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      4675 | 2144 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2145 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2146 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2147 | `	}` |
|      4675 | 2148 | `	pVm->pHookSetAttr = 0;` |
|      4675 | 2149 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      4675 | 2150 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2151 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2152 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2153 | `	}` |
|      4675 | 2154 | `	pVm->pMagicCallClass = 0;` |
|      4675 | 2155 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 2156 | `	/* Configuration containers */` |
|      4675 | 2157 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      4675 | 2158 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      4675 | 2159 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      4675 | 2160 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      4675 | 2161 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|      4675 | 2162 | `	pVm->iResponseStatus = 200;` |
|      4675 | 2163 | `	pVm->bHeadersSent = 0;` |
|      4675 | 2164 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 2165 | `	/* Error callbacks containers */` |
|      4675 | 2166 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|      4675 | 2167 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|      4675 | 2168 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|      4675 | 2169 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|      4675 | 2170 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 2171 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 2172 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 2173 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 2174 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 2175 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 2176 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 2177 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      4675 | 2178 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      4675 | 2179 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 2180 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 2181 | `#else` |
|         - | 2182 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 2183 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 2184 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 2185 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 2186 | `	pVm->nMaxDepth = 512;` |
|         - | 2187 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 2188 | `#endif` |
|         - | 2189 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 2190 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 2191 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      4675 | 2192 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 2193 | `	/* JSON return status */` |
|      4675 | 2194 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2195 | `	/* PRNG context */` |
|      4675 | 2196 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 2197 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 2198 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      4675 | 2199 | `	pVm->mtSeeded = FALSE;` |
|         - | 2200 | `	/* Install the null constant */` |
|      4675 | 2201 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4675 | 2202 | `	if( pObj == 0 ){` |
|       ! 0 | 2203 | `		rc = SXERR_MEM;` |
|       ! 0 | 2204 | `		goto Err;` |
|         - | 2205 | `	}` |
|      4675 | 2206 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 2207 | `	/* Install the boolean TRUE constant */` |
|      4675 | 2208 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4675 | 2209 | `	if( pObj == 0 ){` |
|       ! 0 | 2210 | `		rc = SXERR_MEM;` |
|       ! 0 | 2211 | `		goto Err;` |
|         - | 2212 | `	}` |
|      4675 | 2213 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 2214 | `	/* Install the boolean FALSE constant */` |
|      4675 | 2215 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4675 | 2216 | `	if( pObj == 0 ){` |
|       ! 0 | 2217 | `		rc = SXERR_MEM;` |
|       ! 0 | 2218 | `		goto Err;` |
|         - | 2219 | `	}` |
|      4675 | 2220 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 2221 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 2222 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 2223 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      4675 | 2224 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      4675 | 2225 | `	if( pObj == 0 ){` |
|       ! 0 | 2226 | `		rc = SXERR_MEM;` |
|       ! 0 | 2227 | `		goto Err;` |
|         - | 2228 | `	}` |
|      4675 | 2229 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 2230 | `	/* Create the global frame */` |
|      4675 | 2231 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      4675 | 2232 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2233 | `		goto Err;` |
|         - | 2234 | `	}` |
|         - | 2235 | `	/* Initialize the code generator */` |
|      4675 | 2236 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      4675 | 2237 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2238 | `		goto Err;` |
|         - | 2239 | `	}` |
|         - | 2240 | `	/* VM correctly initialized,set the magic number */` |
|      4675 | 2241 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 2242 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 2243 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      4675 | 2244 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 2245 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      4675 | 2246 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 2247 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 2248 | `	 * compiled — its classes are internal too. */` |
|         - | 2249 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      4675 | 2250 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      4675 | 2251 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      4675 | 2252 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      4675 | 2253 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      4675 | 2254 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 2255 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      4675 | 2256 | `	pVm->pCoalesceObj = 0;` |
|      4675 | 2257 | `	pVm->bCoalesceArmed = 0;` |
|      4675 | 2258 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 2259 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - | 2260 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - | 2261 | ``	 * segfaults the first `new Fiber`. */`` |
|      4675 | 2262 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      4675 | 2263 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 2264 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|         - | 2265 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|         - | 2266 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|         - | 2267 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|         - | 2268 | `	 * rather than being stamped on afterwards. */` |
|      4675 | 2269 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|      4675 | 2270 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      4675 | 2271 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      4675 | 2272 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 2273 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      4675 | 2274 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 2275 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|      4675 | 2276 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|         - | 2277 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|         - | 2278 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - | 2279 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - | 2280 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - | 2281 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      4675 | 2282 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      4675 | 2283 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - | 2284 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|         - | 2285 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|         - | 2286 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|      4675 | 2287 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|         - | 2288 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 2289 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 2290 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      4675 | 2291 | `	PH7_VmInstallReflection(&(*pVm));` |
|      4675 | 2292 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      4675 | 2293 | `	PH7_VmInstallSpl(&(*pVm));` |
|      4675 | 2294 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|      4675 | 2295 | `	PH7_VmInstallSession(&(*pVm));` |
|      4675 | 2296 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 2297 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2298 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|         - | 2299 | `	 * XMLWriter class libraries that build on it. */` |
|      4675 | 2300 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      4675 | 2301 | `	PH7_VmInstallDom(&(*pVm));` |
|      4675 | 2302 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 2303 | `#endif` |
|      4675 | 2304 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 2305 | `	/* Reset the code generator */` |
|      4675 | 2306 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      4675 | 2307 | `	return SXRET_OK;` |
|       ! 0 | 2308 | `Err:` |
|       ! 0 | 2309 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 2310 | `	return rc;` |
|      2340 | 2311 | `}` |
|         - | 2312 | `/*` |
|         - | 2313 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 2314 | ` * routine which store the output in an internal blob.` |
|         - | 2315 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 2316 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 2317 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 2318 | ` * Refer to the official docurmentation for additional information.` |
|         - | 2319 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 2320 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 2321 | ` * to finish executing and extracting the output.` |
|         - | 2322 | ` */` |
|        68 | 2323 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 2324 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 2325 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 2326 | `	void *pUserData     /* User private data */` |
|         - | 2327 | `	)` |
|       ! 0 | 2328 | `{` |
|         - | 2329 | `	 sxi32 rc;` |
|         - | 2330 | `	 /* Store the output in an internal BLOB */` |
|        68 | 2331 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|        68 | 2332 | `	 return rc;` |
|       ! 0 | 2333 | `}` |
|         - | 2334 | `/*` |
|         - | 2335 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 2336 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 2337 | ` */` |
|     93900 | 2338 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 2339 | `{` |
|     93905 | 2340 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|     93905 | 2341 | `	if( xCons != VmObConsumer ){` |
|     26209 | 2342 | `		pVm->nOutputLen += nLen;` |
|     26209 | 2343 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      1233 | 2344 | `			pVm->bHeadersSent = 1;` |
|       614 | 2345 | `		}` |
|     13102 | 2346 | `	}` |
|     93905 | 2347 | `}` |
|         - | 2348 | `/*` |
|         - | 2349 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 2350 | ` *` |
|         - | 2351 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 2352 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 2353 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 2354 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 2355 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 2356 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 2357 | ` *` |
|         - | 2358 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 2359 | ` * conservative BY CONSTRUCTION:` |
|         - | 2360 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 2361 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 2362 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 2363 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 2364 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 2365 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 2366 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 2367 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 2368 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 2369 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 2370 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 2371 | ` *     instruction-count bound -> fallback.` |
|         - | 2372 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 2373 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 2374 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 2375 | ` *` |
|         - | 2376 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 2377 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 2378 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 2379 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 2380 | ` *` |
|         - | 2381 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 2382 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 2383 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 2384 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 2385 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 2386 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 2387 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 2388 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 2389 | ` * entry here.` |
|         - | 2390 | ` */` |
|         - | 2391 | `/*` |
|         - | 2392 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 2393 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 2394 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 2395 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 2396 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 2397 | ` */` |
|     40636 | 2398 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 2399 | `{` |
|     40641 | 2400 | `	int push = 0, n = 0;` |
|         - | 2401 | `	sxi32 d;` |
|     40641 | 2402 | `	switch( pI->iOp ){` |
|         - | 2403 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 2404 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|      7557 | 2405 | `	case PH7_OP_LOADC:` |
|         - | 2406 | `	case PH7_OP_DUP:` |
|     15119 | 2407 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      2725 | 2408 | `	case PH7_OP_LOAD:` |
|      5455 | 2409 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|      5455 | 2410 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         9 | 2411 | `	case PH7_OP_LOAD_REF:` |
|        19 | 2412 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2413 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       518 | 2414 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 2415 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 2416 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1041 | 2417 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 2418 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 2419 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       175 | 2420 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 2421 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       355 | 2422 | `		if( pI->iP2 == 0 ){` |
|       355 | 2423 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       180 | 2424 | `		}else{` |
|       ! 0 | 2425 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 2426 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 2427 | `		}` |
|       355 | 2428 | `		break;` |
|         - | 2429 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 2430 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       142 | 2431 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 2432 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 2433 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 2434 | `	case PH7_OP_NOOP:` |
|       289 | 2435 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2436 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 2437 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|       527 | 2438 | `	case PH7_OP_STORE:` |
|      1059 | 2439 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      1059 | 2440 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 2441 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1072 | 2442 | `	case PH7_OP_POP:` |
|         - | 2443 | `	case PH7_OP_CONSUME:` |
|      2149 | 2444 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2445 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|         - | 2446 | `	 * pushed and none is popped, on any path. */` |
|       ! 0 | 2447 | `	case PH7_OP_ROT_CALLEE:` |
|       ! 0 | 2448 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2449 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 2450 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 2451 | `	 * true pop count is a runtime value — never reaches here. */` |
|       192 | 2452 | `	case PH7_OP_CALL:` |
|       389 | 2453 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2454 | `	/* Jumps. */` |
|        90 | 2455 | `	case PH7_OP_JMP:` |
|       185 | 2456 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       132 | 2457 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|       269 | 2458 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|       269 | 2459 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 2460 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      2826 | 2461 | `	case PH7_OP_DONE:` |
|      5657 | 2462 | `		n = 0; break;` |
|      4353 | 2463 | `	default:` |
|      8711 | 2464 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 2465 | `	}` |
|     31935 | 2466 | `	*pPush = push; *pN = n;` |
|     31935 | 2467 | `	return 1;` |
|     20323 | 2468 | `}` |
|         - | 2469 | `/*` |
|         - | 2470 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 2471 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 2472 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 2473 | ` */` |
|     10674 | 2474 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 2475 | `{` |
|         - | 2476 | `	void *pScratch;` |
|         - | 2477 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 2478 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 2479 | `	sxi32 iMax;` |
|         - | 2480 | `	int push, n, k;` |
|         - | 2481 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     10679 | 2482 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 2483 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 2484 | `		return VM_STACK_UNMODELED;` |
|         - | 2485 | `	}` |
|         - | 2486 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     36315 | 2487 | `	for( i = 0; i < nInstr; i++ ){` |
|     34347 | 2488 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|      8711 | 2489 | `			return VM_STACK_UNMODELED;` |
|         - | 2490 | `		}` |
|     12823 | 2491 | `	}` |
|         - | 2492 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 2493 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 2494 | `	 * first (the byte array last needs no alignment). */` |
|      1973 | 2495 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      1973 | 2496 | `	if( pScratch == 0 ){` |
|       ! 0 | 2497 | `		return VM_STACK_UNMODELED;` |
|         - | 2498 | `	}` |
|      1973 | 2499 | `	aH  = (sxi32 *)pScratch;` |
|      1973 | 2500 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      1973 | 2501 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|      9987 | 2502 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      1973 | 2503 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      1973 | 2504 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|      8267 | 2505 | `	while( nQ > 0 ){` |
|      6299 | 2506 | `		sxu32 pc = aQ[--nQ];` |
|         - | 2507 | `		sxi32 h;` |
|      6299 | 2508 | `		aIn[pc] = 0;` |
|      6299 | 2509 | `		h = aH[pc];` |
|      6299 | 2510 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|      6299 | 2511 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|      6299 | 2512 | `		if( h + push > iMax ){ iMax = h + push; }` |
|      6299 | 2513 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     10639 | 2514 | `		for( k = 0; k < n; k++ ){` |
|      4345 | 2515 | `			sxi32 hn = h + delta[k];` |
|      4345 | 2516 | `			sxu32 t = succ[k];` |
|      4345 | 2517 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      4345 | 2518 | `			if( hn > aH[t] ){` |
|      4331 | 2519 | `				aH[t] = hn;` |
|      4331 | 2520 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      2163 | 2521 | `			}` |
|      2175 | 2522 | `		}` |
|      6299 | 2523 | `		if( iMax < 0 ){ break; }` |
|         5 | 2524 | `	}` |
|      1973 | 2525 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      1973 | 2526 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|      5342 | 2527 | `}` |
|         - | 2528 | `/*` |
|         - | 2529 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 2530 | ` * our compiled PHP program.` |
|         - | 2531 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 2532 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 2533 | ` *` |
|         - | 2534 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 2535 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 2536 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 2537 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 2538 | ` */` |
|   3530498 | 2539 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 2540 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2541 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 2542 | `	)` |
|         5 | 2543 | `{` |
|         - | 2544 | `	ph7_value *pStack;` |
|         - | 2545 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 2546 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 2547 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 2548 | `  ** on the maximum stack depth required.` |
|         - | 2549 | `  **` |
|         - | 2550 | `  ** Allocation all the stack space we will ever need.` |
|         - | 2551 | `  */` |
|   3530503 | 2552 | `	nInstr += VM_STACK_GUARD;` |
|   3530503 | 2553 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   3530503 | 2554 | `	if( pStack == 0 ){` |
|       ! 0 | 2555 | `		return 0;` |
|         - | 2556 | `	}` |
|         - | 2557 | `	/* Initialize the operand stack */` |
|  72543923 | 2558 | `	while( nInstr > 0 ){` |
|  69013425 | 2559 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  69013425 | 2560 | `		--nInstr;` |
|         5 | 2561 | `	}` |
|         - | 2562 | `	/* Ready for bytecode execution */` |
|   3530503 | 2563 | `	return pStack;` |
|   1765310 | 2564 | `}` |
|         - | 2565 | `/*` |
|         - | 2566 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 2567 | ` *` |
|         - | 2568 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 2569 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 2570 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 2571 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 2572 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 2573 | ` * the per-slot PH7_MemObjInit.` |
|         - | 2574 | ` *` |
|         - | 2575 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 2576 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 2577 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 2578 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|         - | 2579 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|         - | 2580 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|         - | 2581 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|         - | 2582 | ` *` |
|         - | 2583 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|         - | 2584 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|         - | 2585 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|         - | 2586 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|         - | 2587 | ` * recursion case is the one worth the O(1) simplicity.` |
|         - | 2588 | ` */` |
|         - | 2589 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 2590 | `struct VmIdleStack {` |
|         - | 2591 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 2592 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 2593 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 2594 | `};` |
|         - | 2595 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|         - | 2596 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|         - | 2597 | `                                    * (a large fallback-sized stack recursing would` |
|         - | 2598 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|         - | 2599 | `                                    * the tight-sized hot case is far below this) */` |
|         - | 2600 | `/*` |
|         - | 2601 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 2602 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 2603 | ` */` |
|    744074 | 2604 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 2605 | `{` |
|    744079 | 2606 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    744079 | 2607 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|    744079 | 2608 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    685353 | 2609 | `		ph7_value *pStack = pIdle->pStack;` |
|    685353 | 2610 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    685353 | 2611 | `		pVm->nIdleOperandStacks--;` |
|         - | 2612 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 2613 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    685353 | 2614 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    685353 | 2615 | `		pVm->pIdleStackNodes = pIdle;` |
|    685353 | 2616 | `		return pStack; /* slots already released -> reusable without re-init */` |
|         - | 2617 | `	}` |
|     58731 | 2618 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    372254 | 2619 | `}` |
|         - | 2620 | `/*` |
|         - | 2621 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 2622 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|         - | 2623 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|         - | 2624 | ` * live value.` |
|         - | 2625 | ` */` |
|    743874 | 2626 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|         5 | 2627 | `{` |
|         - | 2628 | `	VmIdleStack *pIdle;` |
|         - | 2629 | `	sxu32 i;` |
|    743879 | 2630 | `	if( pStack == 0 ){` |
|       ! 0 | 2631 | `		return;` |
|         - | 2632 | `	}` |
|    743879 | 2633 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|     54539 | 2634 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|     54539 | 2635 | `		return;` |
|         - | 2636 | `	}` |
|         - | 2637 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 2638 | `	 * pool-allocate only when the spare list is empty. */` |
|    689345 | 2639 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    689345 | 2640 | `	if( pIdle ){` |
|    685353 | 2641 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    342835 | 2642 | `	}else{` |
|      3997 | 2643 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      3997 | 2644 | `		if( pIdle == 0 ){` |
|       ! 0 | 2645 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 2646 | `			return;` |
|         - | 2647 | `		}` |
|         - | 2648 | `	}` |
|  44473189 | 2649 | `	for( i = 0; i < nCap; i++ ){` |
|  43783849 | 2650 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 2651 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 2652 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 2653 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 2654 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 2655 | `		 * across invocations — cheap defense in depth. */` |
|  43783849 | 2656 | `		pStack[i].nIdx = SXU32_HIGH;` |
|  21957291 | 2657 | `	}` |
|    689345 | 2658 | `	pIdle->pStack = pStack;` |
|    689345 | 2659 | `	pIdle->nCap = nCap;` |
|    689345 | 2660 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    689345 | 2661 | `	pVm->pIdleOperandStacks = pIdle;` |
|    689345 | 2662 | `	pVm->nIdleOperandStacks++;` |
|    372154 | 2663 | `}` |
|         - | 2664 | `/* Forward declaration */` |
|         - | 2665 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 2666 | `/*` |
|         - | 2667 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 2668 | ` * This routine gets called by the PH7 engine after` |
|         - | 2669 | ` * successful compilation of the target PHP program.` |
|         - | 2670 | ` */` |
|      4076 | 2671 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 2672 | `	ph7_vm *pVm /* Target VM */` |
|         - | 2673 | `	)` |
|         5 | 2674 | `{` |
|         - | 2675 | `	SyHashEntry *pEntry;` |
|         - | 2676 | `	sxi32 rc;` |
|      4081 | 2677 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 2678 | `		/* Initialize your VM first */` |
|       ! 0 | 2679 | `		return SXERR_CORRUPT;` |
|         - | 2680 | `	}` |
|         - | 2681 | `	/* Mark the VM ready for byte-code execution */` |
|      4081 | 2682 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 2683 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 2684 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 2685 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 2686 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      4081 | 2687 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 2688 | `	/* Emit the DONE instruction */` |
|      4081 | 2689 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      4081 | 2690 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2691 | `		return SXERR_MEM;` |
|         - | 2692 | `	}` |
|         - | 2693 | `	/* Script return value */` |
|      4081 | 2694 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 2695 | `	/* Allocate a new operand stack */` |
|      4081 | 2696 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      4081 | 2697 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 2698 | `		return SXERR_MEM;` |
|         - | 2699 | `	}` |
|         - | 2700 | `	/* Set the default VM output consumer callback and it's` |
|         - | 2701 | `	 * private data. */` |
|      4081 | 2702 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      4081 | 2703 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 2704 | `	/* Allocate the reference table */` |
|      4081 | 2705 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      4081 | 2706 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      4081 | 2707 | `	if( pVm->apRefObj == 0 ){` |
|         - | 2708 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2709 | `		return SXERR_MEM;` |
|         - | 2710 | `	}` |
|         - | 2711 | `	/* Zero the reference table */` |
|      4081 | 2712 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|         - | 2713 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      4081 | 2714 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      4081 | 2715 | `	if( rc != SXRET_OK ){` |
|         - | 2716 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2717 | `		return rc;` |
|         - | 2718 | `	}` |
|         - | 2719 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 2720 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 2721 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 2722 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 2723 | `	 * below it is compile-time/init state that survives a reset. */` |
|      4081 | 2724 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|         - | 2725 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      4081 | 2726 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      4081 | 2727 | `	if( rc != SXRET_OK ){` |
|         - | 2728 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2729 | `		return rc;` |
|         - | 2730 | `	}` |
|         - | 2731 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      4081 | 2732 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 2733 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      4081 | 2734 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 2735 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      4081 | 2736 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 2737 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      4081 | 2738 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 2739 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 2740 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      4081 | 2741 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      4081 | 2742 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 2743 | `#endif` |
|         - | 2744 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2745 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      4081 | 2746 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 2747 | `#endif` |
|         - | 2748 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 2749 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      4081 | 2750 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 2751 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      4081 | 2752 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 2753 | `	/* Initialize and install static and constants class attributes.` |
|         - | 2754 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 2755 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 2756 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 2757 | `	 * that function in sync when changing what is reserved here. */` |
|         - | 2758 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - | 2759 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - | 2760 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - | 2761 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - | 2762 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - | 2763 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - | 2764 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - | 2765 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 2766 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - | 2767 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      4081 | 2768 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    551493 | 2769 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    547417 | 2770 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    547417 | 2771 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2772 | `			return rc;` |
|         - | 2773 | `		}` |
|         5 | 2774 | `	}` |
|      4081 | 2775 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    550951 | 2776 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    546879 | 2777 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    546879 | 2778 | `		if( rc != SXRET_OK ){` |
|         5 | 2779 | `			return rc;` |
|         - | 2780 | `		}` |
|         5 | 2781 | `	}` |
|         - | 2782 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      4077 | 2783 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2784 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      4077 | 2785 | `	pVm->nNextObjId = 1;` |
|         - | 2786 | `	/* VM is ready for bytecode execution */` |
|      4077 | 2787 | `	return SXRET_OK;` |
|      2043 | 2788 | `}` |
|         - | 2789 | `/*` |
|         - | 2790 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 2791 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 2792 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 2793 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 2794 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 2795 | ` */` |
|         8 | 2796 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 2797 | `{` |
|         - | 2798 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|         - | 2799 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|         - | 2800 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|       276 | 2801 | `	while( pVm->pRefList ){` |
|       268 | 2802 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|       ! 0 | 2803 | `	}` |
|         8 | 2804 | `}` |
|         - | 2805 | `/*` |
|         - | 2806 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 2807 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 2808 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 2809 | ` */` |
|        56 | 2810 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 2811 | `{` |
|        56 | 2812 | `	PH7_MemObjRelease(pObj);` |
|        56 | 2813 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        56 | 2814 | `}` |
|         - | 2815 | `/*` |
|         - | 2816 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 2817 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 2818 | ` * of statics).` |
|         - | 2819 | ` */` |
|      6240 | 2820 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|       ! 0 | 2821 | `{` |
|      6240 | 2822 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 2823 | `	sxu32 k;` |
|      6244 | 2824 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|         4 | 2825 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|         2 | 2826 | `	}` |
|      6240 | 2827 | `}` |
|         - | 2828 | `/*` |
|         - | 2829 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 2830 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 2831 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 2832 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 2833 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 2834 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 2835 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 2836 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 2837 | ` *    has its static sentinels reset.` |
|         - | 2838 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 2839 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 2840 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 2841 | ` */` |
|         8 | 2842 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 2843 | `{` |
|         - | 2844 | `	SyHashEntry *pEntry;` |
|         8 | 2845 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      6248 | 2846 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      6240 | 2847 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      6240 | 2848 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 2849 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|         - | 2850 | `			 * release its captured-by-value environment, then free the entry,` |
|         - | 2851 | `			 * name buffer and structure. */` |
|         4 | 2852 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|         4 | 2853 | `			const char *zName = SyStringData(&pFunc->sName);` |
|         - | 2854 | `			sxu32 k;` |
|         4 | 2855 | `			VmResetFuncStatics(pFunc);` |
|         8 | 2856 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|         4 | 2857 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|         2 | 2858 | `			}` |
|         4 | 2859 | `			SySetRelease(&pFunc->aClosureEnv);` |
|         - | 2860 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 2861 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 2862 | `			if( zName ){` |
|         4 | 2863 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|         2 | 2864 | `			}` |
|         4 | 2865 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|         4 | 2866 | `			continue;` |
|         - | 2867 | `		}` |
|         - | 2868 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     12472 | 2869 | `		while( pFunc ){` |
|      6236 | 2870 | `			VmResetFuncStatics(pFunc);` |
|      6236 | 2871 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 2872 | `		}` |
|       ! 0 | 2873 | `	}` |
|         8 | 2874 | `	pVm->closure_cnt = 0;` |
|         8 | 2875 | `}` |
|         - | 2876 | `/*` |
|         - | 2877 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 2878 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 2879 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 2880 | ` * the class re-mount registers fresh ones.` |
|         - | 2881 | ` */` |
|         8 | 2882 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 2883 | `{` |
|         - | 2884 | `	SyHashEntry *pEntry;` |
|         - | 2885 | `	/* Common case: no class static typed properties — table already empty. */` |
|         8 | 2886 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|         4 | 2887 | `		return;` |
|         - | 2888 | `	}` |
|         - | 2889 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 2890 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 2891 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 2892 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 2893 | `		if( pEntry->pUserData ){` |
|         4 | 2894 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 2895 | `		}` |
|       ! 0 | 2896 | `	}` |
|         4 | 2897 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 2898 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 | 2899 | `}` |
|         - | 2900 | `/*` |
|         - | 2901 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 2902 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 2903 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 2904 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 2905 | ` * comparing equal — both used to cast to 1.` |
|         - | 2906 | ` *` |
|         - | 2907 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 2908 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 2909 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 2910 | ` * aborting a cast.` |
|         - | 2911 | ` */` |
|        40 | 2912 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         2 | 2913 | `{` |
|         - | 2914 | `	SyHashEntry *pEntry;` |
|         - | 2915 | `	phl_res_id *pRec;` |
|        42 | 2916 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 2917 | `		return 0;` |
|         - | 2918 | `	}` |
|        42 | 2919 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|        42 | 2920 | `	if( pEntry ){` |
|        32 | 2921 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 2922 | `	}` |
|        12 | 2923 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        12 | 2924 | `	if( pRec == 0 ){` |
|       ! 0 | 2925 | `		return 0;` |
|         - | 2926 | `	}` |
|        12 | 2927 | `	pRec->pRes = pRes;` |
|        12 | 2928 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        12 | 2929 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 2930 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 2931 | `		return 0;` |
|         - | 2932 | `	}` |
|        12 | 2933 | `	return pRec->nId;` |
|        22 | 2934 | `}` |
|         - | 2935 | `/*` |
|         - | 2936 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 2937 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 2938 | ` */` |
|         8 | 2939 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 2940 | `{` |
|         - | 2941 | `	SyHashEntry *pEntry;` |
|         8 | 2942 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|         8 | 2943 | `		pVm->nResourceIdNext = 1;` |
|         8 | 2944 | `		return;` |
|         - | 2945 | `	}` |
|       ! 0 | 2946 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 2947 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 2948 | `		if( pEntry->pUserData ){` |
|       ! 0 | 2949 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 2950 | `		}` |
|       ! 0 | 2951 | `	}` |
|       ! 0 | 2952 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 2953 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 2954 | `	pVm->nResourceIdNext = 1;` |
|         4 | 2955 | `}` |
|         - | 2956 | `/*` |
|         - | 2957 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 2958 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 2959 | ` *` |
|         - | 2960 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 2961 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 2962 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 2963 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 2964 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 2965 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 2966 | ` *` |
|         - | 2967 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 2968 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 2969 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 2970 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 2971 | ` * reserved during the run.` |
|         - | 2972 | ` *` |
|         - | 2973 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 2974 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 2975 | ` * global-scope destructors never fired.` |
|         - | 2976 | ` */` |
|         8 | 2977 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 2978 | `{` |
|         - | 2979 | `	sxu32 nWater,n;` |
|         8 | 2980 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 2981 | `		return SXERR_CORRUPT;` |
|         - | 2982 | `	}` |
|         8 | 2983 | `	nWater = pVm->nSuperBaseline;` |
|         - | 2984 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 2985 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|         8 | 2986 | `	pVm->pGlobal = 0;` |
|         - | 2987 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 2988 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 2989 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 2990 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 2991 | `	 * object); unref'ing here would race the teardown below. */` |
|         8 | 2992 | `	pVm->pClosureThis = 0;` |
|         8 | 2993 | `	pVm->pClosureScope = 0;` |
|         - | 2994 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 2995 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 2996 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|         - | 2997 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|         8 | 2998 | `	pVm->bInReset = 1;` |
|         - | 2999 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|         8 | 3000 | `	VmResetRefTable(&(*pVm));` |
|         - | 3001 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 3002 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 3003 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 3004 | `	 * re-run define() overwrites the value in place). */` |
|         8 | 3005 | `	VmResetFunctionState(&(*pVm));` |
|         - | 3006 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 3007 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       288 | 3008 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|       280 | 3009 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|       280 | 3010 | `		if( pObj ){` |
|       280 | 3011 | `			PH7_MemObjRelease(pObj);` |
|       140 | 3012 | `		}` |
|       140 | 3013 | `	}` |
|         - | 3014 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 3015 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|         8 | 3016 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 3017 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 3018 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|         8 | 3019 | `	VmResetResourceIds(&(*pVm));` |
|         - | 3020 | `	/* (5) Unwind any active frames back to none. */` |
|        16 | 3021 | `	while( pVm->pFrame ){` |
|         8 | 3022 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 3023 | `	}` |
|         - | 3024 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|         8 | 3025 | `	pVm->bInReset = 0;` |
|         - | 3026 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 3027 | `	 * slots (their indices no longer exist). */` |
|         8 | 3028 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|         8 | 3029 | `	SySetReset(&pVm->aFreeObj);` |
|         - | 3030 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|         8 | 3031 | `	SyHashRelease(&pVm->hSuper);` |
|         8 | 3032 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|         - | 3033 | `	/* (8) Drain remaining per-exec containers. */` |
|         8 | 3034 | `	SySetReset(&pVm->aSelf);` |
|         - | 3035 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 3036 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 3037 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|         8 | 3038 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 3039 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 3040 | `		if( pCB ){` |
|         - | 3041 | `			int iArg;` |
|       ! 0 | 3042 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3043 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 3044 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 3045 | `			}` |
|       ! 0 | 3046 | `		}` |
|       ! 0 | 3047 | `	}` |
|         8 | 3048 | `	SySetReset(&pVm->aShutdown);` |
|         - | 3049 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 3050 | `	 * aborted program can leave entries behind). */` |
|         8 | 3051 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|         8 | 3052 | `	SySetReset(&pVm->aException);` |
|         8 | 3053 | `	SySetReset(&pVm->aFinallyAction);` |
|         8 | 3054 | `	pVm->pPendingException = 0;` |
|         8 | 3055 | `	pVm->pInflightException = 0;` |
|         8 | 3056 | `	pVm->nInflightExcBase = 0;` |
|         8 | 3057 | `	pVm->pResumeFrame = 0;` |
|         8 | 3058 | `	pVm->iResumePc = 0;` |
|         8 | 3059 | `	pVm->pResumeInstr = 0;` |
|         8 | 3060 | `	pVm->iResumeStackDepth = 0;` |
|         8 | 3061 | `	pVm->nBoundaryRc = 0;` |
|         8 | 3062 | `	pVm->pConstEvalClass = 0;` |
|         8 | 3063 | `	pVm->nConstEvalDepth = 0;` |
|         8 | 3064 | `	pVm->pConstCycleAttr = 0;` |
|         8 | 3065 | `	pVm->pConstCycleClass = 0;` |
|         8 | 3066 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 3067 | `	{` |
|         - | 3068 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 3069 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|         8 | 3070 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|         8 | 3071 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 3072 | `		sxu32 iRmw;` |
|         8 | 3073 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 3074 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 3075 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 3076 | `		}` |
|         8 | 3077 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 3078 | `	}` |
|         8 | 3079 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 3080 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 3081 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 3082 | `	}` |
|         8 | 3083 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|         8 | 3084 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 3085 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 3086 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 3087 | `	}` |
|         8 | 3088 | `	pVm->pHookSetAttr = 0;` |
|         8 | 3089 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|         8 | 3090 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 3091 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 3092 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 3093 | `	}` |
|         8 | 3094 | `	pVm->pMagicCallClass = 0;` |
|         8 | 3095 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         8 | 3096 | `	pVm->nExceptDepth = 0;` |
|         - | 3097 | `	/* spl_autoload_register() callbacks are per request */` |
|         8 | 3098 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 3099 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 3100 | `		if( pCB ){` |
|       ! 0 | 3101 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3102 | `		}` |
|       ! 0 | 3103 | `	}` |
|         8 | 3104 | `	SySetReset(&pVm->aAutoload);` |
|         - | 3105 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 3106 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|         8 | 3107 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 3108 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 3109 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 3110 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 3111 | `	}` |
|         - | 3112 | `	/* Output buffers */` |
|         8 | 3113 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 3114 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 3115 | `		if( pOb ){` |
|       ! 0 | 3116 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 3117 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 3118 | `		}` |
|       ! 0 | 3119 | `	}` |
|         8 | 3120 | `	SySetReset(&pVm->aOB);` |
|         8 | 3121 | `	pVm->nObDepth = 0;` |
|         - | 3122 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 3123 | `	{` |
|         8 | 3124 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|         8 | 3125 | `		if( rc == SXRET_OK ){` |
|         8 | 3126 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         4 | 3127 | `		}` |
|         8 | 3128 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 3129 | `			return rc;` |
|         - | 3130 | `		}` |
|         - | 3131 | `	}` |
|         - | 3132 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 3133 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 3134 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 3135 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 3136 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 3137 | `	{` |
|         - | 3138 | `		SyHashEntry *pEntry;` |
|         8 | 3139 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      1084 | 3140 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      1076 | 3141 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 3142 | `			ph7_class_attr *pAttr;` |
|         - | 3143 | `			SyHashEntry *pAttrEntry;` |
|      1076 | 3144 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|      5706 | 3145 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      4092 | 3146 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      4092 | 3147 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|         4 | 3148 | `					pAttr->nIdx = SXU32_HIGH;` |
|         4 | 3149 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|         2 | 3150 | `				}` |
|       ! 0 | 3151 | `			}` |
|         - | 3152 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 3153 | `			 * slots too so VM reuse re-evaluates them. */` |
|      1076 | 3154 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|      2356 | 3155 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|      1280 | 3156 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      1280 | 3157 | `				pAttr->nIdx = SXU32_HIGH;` |
|      1280 | 3158 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 3159 | `			}` |
|       ! 0 | 3160 | `		}` |
|         8 | 3161 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      1084 | 3162 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      1076 | 3163 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      1076 | 3164 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 3165 | `				return rc;` |
|         - | 3166 | `			}` |
|       ! 0 | 3167 | `		}` |
|         - | 3168 | `	}` |
|         - | 3169 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|         8 | 3170 | `	SyBlobReset(&pVm->sConsumer);` |
|         8 | 3171 | `	pVm->nOutputLen = 0;` |
|         8 | 3172 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|         8 | 3173 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|         8 | 3174 | `	pVm->iResponseStatus = 200;` |
|         8 | 3175 | `	pVm->bHeadersSent = 0;` |
|         8 | 3176 | `	pVm->bHttpContext = 0;` |
|         8 | 3177 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|         8 | 3178 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|         8 | 3179 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|         8 | 3180 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|         8 | 3181 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         8 | 3182 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 3183 | `#ifdef PH7_ENABLE_PCRE` |
|         8 | 3184 | `	pVm->iPcreLastError = 0;` |
|         - | 3185 | `#endif` |
|         - | 3186 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3187 | `	/* Drop the libxml error queue and the previous request's documents */` |
|         8 | 3188 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 3189 | `#endif` |
|         8 | 3190 | `	pVm->iCmpCallbackExc = 0;` |
|         8 | 3191 | `	pVm->bHaltRequested = 0;` |
|         8 | 3192 | `	pVm->iExitStatus = 0;` |
|         8 | 3193 | `	pVm->nSpreadCallBase = 0;` |
|         8 | 3194 | `	VmSpreadCaptureReset(pVm);` |
|         8 | 3195 | `	pVm->nRecursionDepth = 0;` |
|         8 | 3196 | `	pVm->pActiveCtx = 0;` |
|         8 | 3197 | `	pVm->pCoalesceObj = 0;` |
|         8 | 3198 | `	pVm->bCoalesceArmed = 0;` |
|         8 | 3199 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 3200 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|         8 | 3201 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 3202 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 3203 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|         8 | 3204 | `	pVm->nNextObjId = 1;` |
|         - | 3205 | `	/* Set the ready flag */` |
|         8 | 3206 | `	pVm->nMagic = PH7_VM_RUN;` |
|         8 | 3207 | `	return SXRET_OK;` |
|         4 | 3208 | `}` |
|         - | 3209 | `/*` |
|         - | 3210 | ` * Release a Virtual Machine.` |
|         - | 3211 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 3212 | ` */` |
|      4072 | 3213 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 3214 | `{` |
|         - | 3215 | `	/* Set the stale magic number */` |
|      4077 | 3216 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 3217 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3218 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 3219 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      4077 | 3220 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 3221 | `#endif` |
|         - | 3222 | `	/* Release the private memory subsystem */` |
|      4077 | 3223 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      4077 | 3224 | `	return SXRET_OK;` |
|         5 | 3225 | `}` |
|         - | 3226 | `/*` |
|         - | 3227 | ` * Initialize a foreign function call context.` |
|         - | 3228 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 3229 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 3230 | ` * functions.` |
|         - | 3231 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 3232 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 3233 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 3234 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 3235 | ` */` |
|   2742729 | 3236 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 3237 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 3238 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 3239 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 3240 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 3241 | `	sxi32 iFlags          /* Control flags */` |
|         - | 3242 | `	)` |
|         5 | 3243 | `{` |
|   2742734 | 3244 | `	pOut->pFunc = pFunc;` |
|   2742734 | 3245 | `	pOut->pVm   = pVm;` |
|   2742734 | 3246 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   2742734 | 3247 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 3248 | `	/* Assume a null return value */` |
|   2742734 | 3249 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   2742734 | 3250 | `	pOut->pRet = pRet;` |
|   2742734 | 3251 | `	pOut->iFlags = iFlags;` |
|   2742734 | 3252 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   2742734 | 3253 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - | 3254 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - | 3255 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - | 3256 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - | 3257 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|   2742734 | 3258 | `	pOut->pThis = 0;` |
|   2742734 | 3259 | `	pOut->pCalledClass = 0;` |
|   2742734 | 3260 | `	pOut->bThisInit = 0;` |
|   2742734 | 3261 | `	return SXRET_OK;` |
|         5 | 3262 | `}` |
|         - | 3263 | `/*` |
|         - | 3264 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 3265 | ` * left behind.` |
|         - | 3266 | ` */` |
|   2742729 | 3267 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 3268 | `{` |
|         - | 3269 | `	sxu32 n;` |
|   2742734 | 3270 | `	if( pCtx->bThisInit ){` |
|         - | 3271 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - | 3272 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - | 3273 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - | 3274 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|      5435 | 3275 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|      5435 | 3276 | `		pCtx->sThis.x.pOther = 0;` |
|      5435 | 3277 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|      5435 | 3278 | `		pCtx->bThisInit = 0;` |
|      2715 | 3279 | `	}` |
|   2742734 | 3280 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     13741 | 3281 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|     44263 | 3282 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     30527 | 3283 | `			if( apObj[n] == 0 ){` |
|         - | 3284 | `				/* Already released */` |
|      2717 | 3285 | `				continue;` |
|         - | 3286 | `			}` |
|     27815 | 3287 | `			PH7_MemObjRelease(apObj[n]);` |
|     27815 | 3288 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     13910 | 3289 | `		}` |
|     13741 | 3290 | `		SySetRelease(&pCtx->sVar);` |
|      6868 | 3291 | `	}` |
|   2742734 | 3292 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 3293 | `		ph7_aux_data *aAux;` |
|         - | 3294 | `		void *pChunk;` |
|         - | 3295 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 3296 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 3297 | `		 */` |
|       899 | 3298 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      2723 | 3299 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      1829 | 3300 | `			pChunk = aAux[n].pAuxData;` |
|         - | 3301 | `			/* Release the chunk */` |
|      1829 | 3302 | `			if( pChunk ){` |
|      1829 | 3303 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       912 | 3304 | `			}` |
|       917 | 3305 | `		}` |
|       899 | 3306 | `		SySetRelease(&pCtx->sChunk);` |
|       447 | 3307 | `	}` |
|   2742734 | 3308 | `}` |
|         - | 3309 | `/*` |
|         - | 3310 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 3311 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 3312 | ` */` |
|      2712 | 3313 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 3314 | `	ph7_context *pCtx, /* Call context */` |
|         - | 3315 | `	ph7_value *pValue  /* Release this value */` |
|         - | 3316 | `	)` |
|         5 | 3317 | `{` |
|      2717 | 3318 | `	if( pValue == 0 ){` |
|         - | 3319 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 3320 | `		return;` |
|         - | 3321 | `	}` |
|      2717 | 3322 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      2717 | 3323 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 3324 | `		sxu32 n;` |
|    366091 | 3325 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    366091 | 3326 | `			if( apObj[n] == pValue ){` |
|      2717 | 3327 | `				PH7_MemObjRelease(pValue);` |
|      2717 | 3328 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 3329 | `				/* Mark as released */` |
|      2717 | 3330 | `				apObj[n] = 0;` |
|      2717 | 3331 | `				break;` |
|         - | 3332 | `			}` |
|    181692 | 3333 | `		}` |
|      1356 | 3334 | `	}` |
|      1361 | 3335 | `}` |
|         - | 3336 | `/*` |
|         - | 3337 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 3338 | ` */` |
|  15830201 | 3339 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 3340 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 3341 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 3342 | `	)` |
|         5 | 3343 | `{` |
|  15830206 | 3344 | `	ph7_value *pTos = *ppTos;` |
|  32681405 | 3345 | `	while( nPop > 0 ){` |
|  16851204 | 3346 | `		PH7_MemObjRelease(pTos);` |
|  16851204 | 3347 | `		pTos--;` |
|  16851204 | 3348 | `		nPop--;` |
|         5 | 3349 | `	}` |
|         - | 3350 | `	/* Top of the stack */` |
|  15830206 | 3351 | `	*ppTos = pTos;` |
|  15830206 | 3352 | `}` |
|         - | 3353 | `/*` |
|         - | 3354 | ` * Reserve a memory object.` |
|         - | 3355 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 3356 | ` */` |
|  18967728 | 3357 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 3358 | `{` |
|  18967733 | 3359 | `	ph7_value *pObj = 0;` |
|         - | 3360 | `	VmSlot *pSlot;` |
|         - | 3361 | `	sxu32 nIdx;` |
|         - | 3362 | `	/* Check for a free slot */` |
|  18967733 | 3363 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  18967733 | 3364 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  18967733 | 3365 | `	if( pSlot ){` |
|  16029705 | 3366 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  16029705 | 3367 | `		nIdx = pSlot->nIdx;` |
|   8016122 | 3368 | `	}` |
|  18967733 | 3369 | `	if( pObj == 0 ){` |
|         - | 3370 | `		/* Reserve a new memory object */` |
|   2938033 | 3371 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   2938033 | 3372 | `		if( pObj == 0 ){` |
|       ! 0 | 3373 | `			return 0;` |
|         - | 3374 | `		}` |
|   1469014 | 3375 | `	}` |
|         - | 3376 | `	/* Set a null default value */` |
|  18967733 | 3377 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  18967733 | 3378 | `	pObj->nIdx = nIdx;` |
|  18967733 | 3379 | `	return pObj;` |
|   9485141 | 3380 | `}` |
|         - | 3381 | `/*` |
|         - | 3382 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 3383 | ` */` |
|     56046 | 3384 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 3385 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 3386 | `	const char *zKey,  /* Entry key */` |
|         - | 3387 | `	sxu32 nByte,       /* Key length */` |
|         - | 3388 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 3389 | `	)` |
|         5 | 3390 | `{` |
|         - | 3391 | `	ph7_value sKey;` |
|         - | 3392 | `	sxi32 rc;` |
|     56051 | 3393 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     56051 | 3394 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 3395 | `	/* Perform the insertion */` |
|     56051 | 3396 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     56051 | 3397 | `	PH7_MemObjRelease(&sKey);` |
|     56051 | 3398 | `	return rc;` |
|         5 | 3399 | `}` |
|         - | 3400 | `/*` |
|         - | 3401 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 3402 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 3403 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 3404 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 3405 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 3406 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 3407 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 3408 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 3409 | ` *     superglobal in place.` |
|         - | 3410 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 3411 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|         - | 3412 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|         - | 3413 | ` *     a plain variable.` |
|         - | 3414 | ` */` |
|       178 | 3415 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         2 | 3416 | `{` |
|       180 | 3417 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 3418 | `	SyHashEntry *pEntry;` |
|         - | 3419 | `	ph7_value *pObj;` |
|         - | 3420 | `	char *zDup;` |
|         - | 3421 | `	sxu32 nIdx;` |
|         - | 3422 | `	sxi32 rc;` |
|         - | 3423 | `	/* Walk down to the global frame */` |
|       200 | 3424 | `	while( pFrame->pParent ){` |
|        22 | 3425 | `		pFrame = pFrame->pParent;` |
|         2 | 3426 | `	}` |
|         - | 3427 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       180 | 3428 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|       180 | 3429 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 3430 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 3431 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 3432 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 3433 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 3434 | `		 * of $GLOBALS itself). */` |
|         5 | 3435 | `		pEntry = 0;` |
|         2 | 3436 | `	}` |
|       180 | 3437 | `	if( pEntry == 0 ){` |
|       180 | 3438 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|        89 | 3439 | `	}` |
|       180 | 3440 | `	if( pEntry ){` |
|         5 | 3441 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 3442 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|         - | 3443 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|         3 | 3444 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|         3 | 3445 | `			return SXRET_OK;` |
|         - | 3446 | `		}` |
|         3 | 3447 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 3448 | `		if( pObj == 0 ){` |
|       ! 0 | 3449 | `			return SXERR_NOTFOUND;` |
|         - | 3450 | `		}` |
|         3 | 3451 | `		if( pValue ){` |
|         3 | 3452 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 3453 | `		}else{` |
|       ! 0 | 3454 | `			PH7_MemObjToNull(pObj);` |
|         - | 3455 | `		}` |
|         3 | 3456 | `		return SXRET_OK;` |
|         - | 3457 | `	}` |
|       176 | 3458 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 3459 | `		/* Reserve a fresh slot for the new global */` |
|       174 | 3460 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       174 | 3461 | `		if( pObj == 0 ){` |
|       ! 0 | 3462 | `			return SXERR_MEM;` |
|         - | 3463 | `		}` |
|       174 | 3464 | `		nIdx = pObj->nIdx;` |
|        88 | 3465 | `	}else{` |
|         - | 3466 | `		/* Reference assignment: bind the name to the existing slot */` |
|         3 | 3467 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|         3 | 3468 | `		if( pObj == 0 ){` |
|       ! 0 | 3469 | `			return SXERR_NOTFOUND;` |
|         - | 3470 | `		}` |
|         3 | 3471 | `		nIdx = nRefIdx;` |
|         - | 3472 | `	}` |
|       176 | 3473 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       176 | 3474 | `	if( zDup == 0 ){` |
|       ! 0 | 3475 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3476 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 3477 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|         - | 3478 | `			VmSlot sFree;` |
|       ! 0 | 3479 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3480 | `			sFree.pUserData = 0;` |
|       ! 0 | 3481 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3482 | `		}` |
|       ! 0 | 3483 | `		return SXERR_MEM;` |
|         - | 3484 | `	}` |
|       176 | 3485 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       176 | 3486 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3487 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3488 | `			VmSlot sFree;` |
|       ! 0 | 3489 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3490 | `			sFree.pUserData = 0;` |
|       ! 0 | 3491 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3492 | `		}` |
|       ! 0 | 3493 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 3494 | `		return rc;` |
|         - | 3495 | `	}` |
|         - | 3496 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       176 | 3497 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       176 | 3498 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       176 | 3499 | `	if( nRefIdx == SXU32_HIGH ){` |
|       174 | 3500 | `		pObj->nIdx = nIdx;` |
|       174 | 3501 | `		if( pValue ){` |
|       162 | 3502 | `			PH7_MemObjStore(pValue,pObj);` |
|        80 | 3503 | `		}` |
|        86 | 3504 | `	}` |
|       176 | 3505 | `	return SXRET_OK;` |
|        91 | 3506 | `}` |
|         - | 3507 | `/*` |
|         - | 3508 | ` * Extract a variable value from the top active VM frame.` |
|         - | 3509 | ` * Return a pointer to the variable value on success.` |
|         - | 3510 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 3511 | ` */` |
|  10910935 | 3512 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 3513 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 3514 | `	const SyString *pName, /* Variable name */` |
|         - | 3515 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 3516 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 3517 | `	)` |
|         5 | 3518 | `{` |
|  10910940 | 3519 | `	int bNullify = FALSE;` |
|         - | 3520 | `	SyHashEntry *pEntry;` |
|         - | 3521 | `	VmFrame *pFrame;` |
|         - | 3522 | `	ph7_value *pObj;` |
|         - | 3523 | `	sxu32 nIdx;` |
|         - | 3524 | `	sxi32 rc;` |
|         - | 3525 | `	/* Point to the top active frame */` |
|  10910940 | 3526 | `	pFrame = pVm->pFrame;` |
|  10910940 | 3527 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 3528 | `	/* Perform the lookup */` |
|  10910940 | 3529 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 3530 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        18 | 3531 | `		pName = &sAnnon;` |
|         - | 3532 | `		/* Always nullify the object */` |
|        18 | 3533 | `		bNullify = TRUE;` |
|        18 | 3534 | `		bDup = FALSE;` |
|         8 | 3535 | `	}` |
|         - | 3536 | `	/* Check the superglobals table first */` |
|  10910940 | 3537 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  10910940 | 3538 | `	if( pEntry == 0 ){` |
|         - | 3539 | `		/* Query the top active frame */` |
|  10910388 | 3540 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  10910388 | 3541 | `		if( pEntry == 0 ){` |
|   1092261 | 3542 | `			char *zName = (char *)pName->zString;` |
|         - | 3543 | `			VmSlot sLocal;` |
|   1092261 | 3544 | `			if( !bCreate ){` |
|         - | 3545 | `				/* Do not create the variable,return NULL instead */` |
|      6735 | 3546 | `				return 0;` |
|         - | 3547 | `			}` |
|         - | 3548 | `			/* No such variable,automatically create a new one and install` |
|         - | 3549 | `			 * it in the current frame.` |
|         - | 3550 | `			 */` |
|   1085531 | 3551 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1085531 | 3552 | `			if( pObj == 0 ){` |
|       ! 0 | 3553 | `				return 0;` |
|         - | 3554 | `			}` |
|   1085531 | 3555 | `			nIdx = pObj->nIdx;` |
|   1085531 | 3556 | `			if( bDup ){` |
|         - | 3557 | `				/* Duplicate name */` |
|      1809 | 3558 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      1809 | 3559 | `				if( zName == 0 ){` |
|       ! 0 | 3560 | `					return 0;` |
|         - | 3561 | `				}` |
|       902 | 3562 | `			}` |
|         - | 3563 | `			/* Link to the top active VM frame */` |
|   1085531 | 3564 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   1085531 | 3565 | `			if( rc != SXRET_OK ){` |
|         - | 3566 | `				/* Return the slot to the free pool */` |
|       ! 0 | 3567 | `				sLocal.nIdx = nIdx;` |
|       ! 0 | 3568 | `				sLocal.pUserData = 0;` |
|       ! 0 | 3569 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|       ! 0 | 3570 | `				return 0;` |
|         - | 3571 | `			}` |
|   1085531 | 3572 | `			if( pFrame->pParent != 0 ){` |
|         - | 3573 | `				/* Local variable */` |
|   1074537 | 3574 | `				sLocal.nIdx = nIdx;` |
|   1074537 | 3575 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    549537 | 3576 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|         - | 3577 | `				/* Register in the $GLOBALS array */` |
|     10893 | 3578 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|      5444 | 3579 | `			}` |
|         - | 3580 | `			/* Install in the reference table */` |
|   1085531 | 3581 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 3582 | `			/* Save object index */` |
|   1085531 | 3583 | `			pObj->nIdx = nIdx;` |
|    544040 | 3584 | `		}else{` |
|         - | 3585 | `			/* Extract variable contents */` |
|   9818132 | 3586 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   9818132 | 3587 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   9818132 | 3588 | `			if( bNullify && pObj ){` |
|         3 | 3589 | `				PH7_MemObjRelease(pObj);` |
|         1 | 3590 | `			}` |
|         - | 3591 | `		}` |
|   5459303 | 3592 | `	}else{` |
|         - | 3593 | `		/* Superglobal */` |
|       557 | 3594 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       557 | 3595 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 3596 | `	}` |
|  10904210 | 3597 | `	return pObj;` |
|   5462944 | 3598 | `}` |
|         - | 3599 | `/*` |
|         - | 3600 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 3601 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 3602 | ` */` |
|     37124 | 3603 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 3604 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 3605 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 3606 | `	sxu32 nByte        /* zName length */` |
|         - | 3607 | `	)` |
|         5 | 3608 | `{` |
|         - | 3609 | `	SyHashEntry *pEntry;` |
|         - | 3610 | `	ph7_value *pValue;` |
|         - | 3611 | `	sxu32 nIdx;` |
|         - | 3612 | `	/* Query the superglobal table */` |
|     37129 | 3613 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     37129 | 3614 | `	if( pEntry == 0 ){` |
|         - | 3615 | `		/* No such entry */` |
|       ! 0 | 3616 | `		return 0;` |
|         - | 3617 | `	}` |
|         - | 3618 | `	/* Extract the superglobal index in the global object pool */` |
|     37129 | 3619 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3620 | `	/* Extract the variable value  */` |
|     37129 | 3621 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     37129 | 3622 | `	return pValue;` |
|     18567 | 3623 | `}` |
|         - | 3624 | `/*` |
|         - | 3625 | ` * Perform a raw hashmap insertion.` |
|         - | 3626 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 3627 | ` */` |
|     29046 | 3628 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 3629 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 3630 | `	const char *zKey,   /* Entry key */` |
|         - | 3631 | `	int nKeylen,        /* zKey length*/` |
|         - | 3632 | `	const char *zData,  /* Entry data */` |
|         - | 3633 | `	int nLen            /* zData length */` |
|         - | 3634 | `	)` |
|         5 | 3635 | `{` |
|         - | 3636 | `	ph7_value sKey,sValue;` |
|         - | 3637 | `	sxi32 rc;` |
|     29051 | 3638 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     29051 | 3639 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     29051 | 3640 | `	if( zKey ){` |
|     24929 | 3641 | `		if( nKeylen < 0 ){` |
|     24825 | 3642 | `			nKeylen = (int)SyStrlen(zKey);` |
|     12410 | 3643 | `		}` |
|     24929 | 3644 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     12462 | 3645 | `	}` |
|     29051 | 3646 | `	if( zData ){` |
|     29051 | 3647 | `		if( nLen < 0 ){` |
|         - | 3648 | `			/* Compute length automatically */` |
|     16475 | 3649 | `			nLen = (int)SyStrlen(zData);` |
|      8235 | 3650 | `		}` |
|     29051 | 3651 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     14523 | 3652 | `	}` |
|         - | 3653 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 3654 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 3655 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 3656 | `	 * every argument under "". */` |
|     29051 | 3657 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     29051 | 3658 | `	PH7_MemObjRelease(&sKey);` |
|     29051 | 3659 | `	PH7_MemObjRelease(&sValue);` |
|     29051 | 3660 | `	return rc;` |
|         5 | 3661 | `}` |
|         - | 3662 | `/*` |
|         - | 3663 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 3664 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 3665 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 3666 | ` */` |
|        34 | 3667 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 3668 | `{` |
|        38 | 3669 | `	sxi64 iVal = 0;` |
|        38 | 3670 | `	if( nValue == 0 ){` |
|       ! 0 | 3671 | `		return 0;` |
|         - | 3672 | `	}` |
|        34 | 3673 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 3674 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 3675 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 3676 | `		return 1;` |
|         - | 3677 | `	}` |
|        38 | 3678 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 3679 | `	return iVal != 0;` |
|        21 | 3680 | `}` |
|         - | 3681 | `/*` |
|         - | 3682 | ` * Configure a working virtual machine instance.` |
|         - | 3683 | ` *` |
|         - | 3684 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 3685 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 3686 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 3687 | ` * The second argument to this function is an integer configuration option` |
|         - | 3688 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 3689 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 3690 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 3691 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 3692 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 3693 | ` */` |
|    110656 | 3694 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 3695 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3696 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 3697 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 3698 | `	)` |
|         5 | 3699 | `{` |
|    110661 | 3700 | `	sxi32 rc = SXRET_OK;` |
|    110661 | 3701 | `	switch(nOp){` |
|      2023 | 3702 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      4051 | 3703 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4051 | 3704 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3705 | `		/* VM output consumer callback */` |
|         - | 3706 | `#ifdef UNTRUST` |
|         - | 3707 | `		if( xConsumer == 0 ){` |
|         - | 3708 | `			rc = SXERR_CORRUPT;` |
|         - | 3709 | `			break;` |
|         - | 3710 | `		}` |
|         - | 3711 | `#endif` |
|         - | 3712 | `		/* Install the output consumer */` |
|      4051 | 3713 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      4051 | 3714 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      4051 | 3715 | `		break;` |
|         - | 3716 | `							   }` |
|      2023 | 3717 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      4051 | 3718 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4051 | 3719 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3720 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 3721 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 3722 | `		 * instead of the program-output stream. */` |
|         - | 3723 | `#ifdef UNTRUST` |
|         - | 3724 | `		if( xConsumer == 0 ){` |
|         - | 3725 | `			rc = SXERR_CORRUPT;` |
|         - | 3726 | `			break;` |
|         - | 3727 | `		}` |
|         - | 3728 | `#endif` |
|      4051 | 3729 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      4051 | 3730 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      4051 | 3731 | `		break;` |
|         - | 3732 | `								   }` |
|      2036 | 3733 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 3734 | `		/* Import path */` |
|         - | 3735 | `		  const char *zPath;` |
|         - | 3736 | `		  SyString sPath;` |
|      4077 | 3737 | `		  zPath = va_arg(ap,const char *);` |
|         - | 3738 | `#if defined(UNTRUST)` |
|         - | 3739 | `		  if( zPath == 0 ){` |
|         - | 3740 | `			  rc = SXERR_EMPTY;` |
|         - | 3741 | `			  break;` |
|         - | 3742 | `		  }` |
|         - | 3743 | `#endif` |
|      4077 | 3744 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 3745 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 3746 | `#ifdef __WINNT__` |
|         5 | 3747 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 3748 | `#endif` |
|      8149 | 3749 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 3750 | `		  /* Remove leading and trailing white spaces */` |
|      4077 | 3751 | `		  SyStringFullTrim(&sPath);` |
|      4077 | 3752 | `		  if( sPath.nByte > 0 ){` |
|         - | 3753 | `			  /* Store the path in the corresponding conatiner */` |
|      4077 | 3754 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      2036 | 3755 | `		  }` |
|      4077 | 3756 | `		  break;` |
|         - | 3757 | `									 }` |
|      2039 | 3758 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 3759 | `		/* Run-Time Error report */` |
|      4083 | 3760 | `		pVm->bErrReport = 1;` |
|      4083 | 3761 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      4083 | 3762 | `		break;` |
|         2 | 3763 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 3764 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 3765 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 3766 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 3767 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 3768 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 3769 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 3770 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 3771 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 3772 | `		int nDepth = va_arg(ap,int);` |
|         5 | 3773 | `		if( nDepth >= 0 ){` |
|         5 | 3774 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 3775 | `		}` |
|         5 | 3776 | `		break;` |
|         - | 3777 | `									   }` |
|         5 | 3778 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 3779 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 3780 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 3781 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 3782 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 3783 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 3784 | `		 * so it is rejected as a footgun). */` |
|        12 | 3785 | `		int nDepth = va_arg(ap,int);` |
|        12 | 3786 | `		if( nDepth > 1 ){` |
|        12 | 3787 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 3788 | `		}` |
|        12 | 3789 | `		break;` |
|         - | 3790 | `									   }` |
|       ! 0 | 3791 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 3792 | `		/* VM output length in bytes */` |
|       ! 0 | 3793 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 3794 | `#ifdef UNTRUST` |
|         - | 3795 | `		if( pOut == 0 ){` |
|         - | 3796 | `			rc = SXERR_CORRUPT;` |
|         - | 3797 | `			break;` |
|         - | 3798 | `		}` |
|         - | 3799 | `#endif` |
|       ! 0 | 3800 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 3801 | `		break;` |
|         - | 3802 | `							   }` |
|         - | 3803 |  |
|     22443 | 3804 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 3805 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 3806 | `		/* Create a new superglobal/global variable */` |
|     44891 | 3807 | `		const char *zName = va_arg(ap,const char *);` |
|     44891 | 3808 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 3809 | `		SyHashEntry *pEntry;` |
|         - | 3810 | `		ph7_value *pObj;` |
|         - | 3811 | `		sxu32 nByte;` |
|         - | 3812 | `		sxu32 nIdx;` |
|         - | 3813 | `#ifdef UNTRUST` |
|         - | 3814 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 3815 | `			rc = SXERR_CORRUPT;` |
|         - | 3816 | `			break;` |
|         - | 3817 | `		}` |
|         - | 3818 | `#endif` |
|     44891 | 3819 | `		nByte = SyStrlen(zName);` |
|     44891 | 3820 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3821 | `			/* Check if the superglobal is already installed */` |
|     40845 | 3822 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     20425 | 3823 | `		}else{` |
|         - | 3824 | `			/* Query the top active VM frame */` |
|      4051 | 3825 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 3826 | `		}` |
|     44891 | 3827 | `		if( pEntry ){` |
|         - | 3828 | `			/* Variable already installed */` |
|       ! 0 | 3829 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3830 | `			/* Extract contents */` |
|       ! 0 | 3831 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 3832 | `			if( pObj ){` |
|         - | 3833 | `				/* Overwrite old contents */` |
|       ! 0 | 3834 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 3835 | `			}` |
|       ! 0 | 3836 | `		}else{` |
|         - | 3837 | `			/* Install a new variable */` |
|     44891 | 3838 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     44891 | 3839 | `			if( pObj == 0 ){` |
|       ! 0 | 3840 | `				rc = SXERR_MEM;` |
|       ! 0 | 3841 | `				break;` |
|         - | 3842 | `			}` |
|     44891 | 3843 | `			nIdx = pObj->nIdx;` |
|         - | 3844 | `			/* Copy value */` |
|     44891 | 3845 | `			PH7_MemObjStore(pValue,pObj);` |
|     44891 | 3846 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3847 | `				/* Install the superglobal */` |
|     40845 | 3848 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     20425 | 3849 | `			}else{` |
|         - | 3850 | `				/* Install in the current frame */` |
|      4051 | 3851 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 3852 | `			}` |
|     44891 | 3853 | `			if( rc == SXRET_OK ){` |
|         - | 3854 | `				SyHashEntry *pRef;` |
|     44891 | 3855 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     40845 | 3856 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     20425 | 3857 | `				}else{` |
|      4051 | 3858 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 3859 | `				}` |
|         - | 3860 | `				/* Install in the reference table */` |
|     44891 | 3861 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     44891 | 3862 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 3863 | `					/* Register in the $GLOBALS array */` |
|     44891 | 3864 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     22443 | 3865 | `				}` |
|     22443 | 3866 | `			}` |
|         - | 3867 | `		}` |
|     44891 | 3868 | `		break;` |
|         - | 3869 | `									}` |
|     12410 | 3870 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 3871 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 3872 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 3873 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 3874 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 3875 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 3876 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     24825 | 3877 | `		const char *zKey   = va_arg(ap,const char *);` |
|     24825 | 3878 | `		const char *zValue = va_arg(ap,const char *);` |
|     24825 | 3879 | `		int nLen = va_arg(ap,int);` |
|         - | 3880 | `		ph7_hashmap *pMap;` |
|         - | 3881 | `		ph7_value *pValue;` |
|     24825 | 3882 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 3883 | `			/* Extract the $_ENV superglobal */` |
|         5 | 3884 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     24823 | 3885 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 3886 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 3887 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     24821 | 3888 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 3889 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 3890 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     24821 | 3891 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 3892 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 3893 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     24821 | 3894 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 3895 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 3896 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     24821 | 3897 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 3898 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 3899 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 3900 | `		}else{` |
|         - | 3901 | `			/* Extract the $_SERVER superglobal */` |
|     24821 | 3902 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 3903 | `		}` |
|     24825 | 3904 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3905 | `			/* No such entry */` |
|       ! 0 | 3906 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3907 | `			break;` |
|         - | 3908 | `		}` |
|         - | 3909 | `		/* Point to the hashmap */` |
|     24825 | 3910 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3911 | `		/* Perform the insertion */` |
|     24825 | 3912 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     24825 | 3913 | `		break;` |
|         - | 3914 | `								   }` |
|      2061 | 3915 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 3916 | `		/* Script arguments */` |
|      4127 | 3917 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 3918 | `		ph7_hashmap *pMap;` |
|         - | 3919 | `		ph7_value *pValue;` |
|         - | 3920 | `		sxu32 n;` |
|         - | 3921 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 3922 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 3923 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 3924 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      4127 | 3925 | `		if( zValue == 0 ){` |
|       ! 0 | 3926 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3927 | `			break;` |
|         - | 3928 | `		}` |
|         - | 3929 | `		/* Extract the $argv array */` |
|      4127 | 3930 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4127 | 3931 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3932 | `			/* No such entry */` |
|       ! 0 | 3933 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3934 | `			break;` |
|         - | 3935 | `		}` |
|         - | 3936 | `		/* Point to the hashmap */` |
|      4127 | 3937 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3938 | `		/* Perform the insertion */` |
|      4127 | 3939 | `		n = (sxu32)SyStrlen(zValue);` |
|      4127 | 3940 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      4127 | 3941 | `		break;` |
|         - | 3942 | `								  }` |
|      2023 | 3943 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 3944 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 3945 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 3946 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 3947 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 3948 | `		ph7_value *pArgv,*pServer;` |
|         - | 3949 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 3950 | `		ph7_value sArgvVal,sKey,sCount;` |
|      4051 | 3951 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4051 | 3952 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      4046 | 3953 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      4051 | 3954 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 3955 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3956 | `			break;` |
|         - | 3957 | `		}` |
|      4051 | 3958 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      4051 | 3959 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 3960 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      4051 | 3961 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      4051 | 3962 | `		if( pDup == 0 ){` |
|       ! 0 | 3963 | `			rc = SXERR_MEM;` |
|       ! 0 | 3964 | `			break;` |
|         - | 3965 | `		}` |
|      4051 | 3966 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      4051 | 3967 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      4051 | 3968 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4051 | 3969 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      4051 | 3970 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      4051 | 3971 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      4051 | 3972 | `		PH7_MemObjRelease(&sKey);` |
|         - | 3973 | `		/* $_SERVER['argc'] = count($argv). */` |
|      4051 | 3974 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      4051 | 3975 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4051 | 3976 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      4051 | 3977 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      4051 | 3978 | `		PH7_MemObjRelease(&sCount);` |
|      4051 | 3979 | `		PH7_MemObjRelease(&sKey);` |
|      4051 | 3980 | `		rc = SXRET_OK;` |
|      4051 | 3981 | `		break;` |
|         - | 3982 | `								  }` |
|        46 | 3983 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 3984 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 3985 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 3986 | `		 * apply immediately so they take effect even if the script never` |
|         - | 3987 | `		 * touches the INI API. */` |
|        96 | 3988 | `		const char *zName = va_arg(ap,const char *);` |
|        96 | 3989 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 3990 | `		VmIniEntry sEntry;` |
|         - | 3991 | `		char *zDupN,*zDupV;` |
|         - | 3992 | `		sxu32 nName,nValue;` |
|        96 | 3993 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 3994 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3995 | `			break;` |
|         - | 3996 | `		}` |
|        96 | 3997 | `		if( zValue == 0 ){` |
|       ! 0 | 3998 | `			zValue = "";` |
|       ! 0 | 3999 | `		}` |
|        96 | 4000 | `		nName = (sxu32)SyStrlen(zName);` |
|        96 | 4001 | `		nValue = (sxu32)SyStrlen(zValue);` |
|        96 | 4002 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|        96 | 4003 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|        96 | 4004 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 4005 | `			rc = SXERR_MEM;` |
|       ! 0 | 4006 | `			break;` |
|         - | 4007 | `		}` |
|        96 | 4008 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|        96 | 4009 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|        96 | 4010 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|        96 | 4011 | `		if( rc == SXRET_OK ){` |
|        92 | 4012 | `			if( nName == sizeof("error_reporting")-1` |
|        70 | 4013 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         4 | 4014 | `				sxi64 iLevel = 0;` |
|         4 | 4015 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         4 | 4016 | `				pVm->bErrReport = iLevel != 0;` |
|        90 | 4017 | `			}else if( nName == sizeof("date.timezone")-1` |
|        44 | 4018 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|       ! 0 | 4019 | `			 && nValue == 3` |
|         4 | 4020 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 4021 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 4022 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 4023 | `				pVm->nDefTz = 3;` |
|        88 | 4024 | `			}else if( nName == sizeof("zend.assertions")-1` |
|        84 | 4025 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 4026 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 4027 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 4028 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 4029 | `				sxi64 iZend = 0;` |
|        40 | 4030 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 4031 | `				if( iZend >= 1 ){` |
|        40 | 4032 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 4033 | `				}else{` |
|       ! 0 | 4034 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 4035 | `				}` |
|        74 | 4036 | `			}else if( nName == sizeof("display_errors")-1` |
|        39 | 4037 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 4038 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 4039 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 4040 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 4041 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        47 | 4042 | `			}else if( nName == sizeof("log_errors")-1` |
|        29 | 4043 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 4044 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|         8 | 4045 | `			}` |
|        46 | 4046 | `		}` |
|        96 | 4047 | `		break;` |
|         - | 4048 | `								  }` |
|       ! 0 | 4049 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 4050 | `		/* error_log() consumer */` |
|       ! 0 | 4051 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 4052 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 4053 | `		break;` |
|         - | 4054 | `										}` |
|       ! 0 | 4055 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 4056 | `		/* Script return value */` |
|       ! 0 | 4057 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 4058 | `#ifdef UNTRUST` |
|         - | 4059 | `		if( ppValue == 0 ){` |
|         - | 4060 | `			rc = SXERR_CORRUPT;` |
|         - | 4061 | `			break;` |
|         - | 4062 | `		}` |
|         - | 4063 | `#endif` |
|       ! 0 | 4064 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 4065 | `		break;` |
|         - | 4066 | `								   }` |
|      8153 | 4067 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 4068 | `		/* Register an IO stream device */` |
|     16311 | 4069 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 4070 | `		/* Make sure we are dealing with a valid IO stream */` |
|     16306 | 4071 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     16311 | 4072 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|         - | 4073 | `				/* Invalid stream */` |
|       ! 0 | 4074 | `				rc = SXERR_INVALID;` |
|       ! 0 | 4075 | `				break;` |
|         - | 4076 | `		}` |
|     16311 | 4077 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 4078 | `			/* Make the 'file://' stream the defaut stream device */` |
|      4081 | 4079 | `			pVm->pDefStream = pStream;` |
|      2038 | 4080 | `		}` |
|         - | 4081 | `		/* Insert in the appropriate container */` |
|     16311 | 4082 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     16311 | 4083 | `		break;` |
|         - | 4084 | `								  }` |
|        16 | 4085 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 4086 | `		/* Point to the VM internal output consumer buffer */` |
|        32 | 4087 | `		const void **ppOut = va_arg(ap,const void **);` |
|        32 | 4088 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 4089 | `#ifdef UNTRUST` |
|         - | 4090 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 4091 | `			rc = SXERR_CORRUPT;` |
|         - | 4092 | `			break;` |
|         - | 4093 | `		}` |
|         - | 4094 | `#endif` |
|        32 | 4095 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        32 | 4096 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        32 | 4097 | `		break;` |
|         - | 4098 | `									   }` |
|        16 | 4099 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 4100 | `		/* Raw HTTP request*/` |
|        32 | 4101 | `		const char *zRequest = va_arg(ap,const char *);` |
|        32 | 4102 | `		int nByte = va_arg(ap,int);` |
|        32 | 4103 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 4104 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4105 | `			break;` |
|         - | 4106 | `		}` |
|        32 | 4107 | `		if( nByte < 0 ){` |
|         - | 4108 | `			/* Compute length automatically */` |
|       ! 0 | 4109 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 4110 | `		}` |
|         - | 4111 | `		/* Process the request */` |
|        32 | 4112 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 4113 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        32 | 4114 | `		if( rc == SXRET_OK ){` |
|        30 | 4115 | `			pVm->bHttpContext = 1;` |
|        15 | 4116 | `		}` |
|        32 | 4117 | `		break;` |
|         - | 4118 | `									}` |
|        16 | 4119 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 4120 | `		/* Extract HTTP response status code */` |
|        32 | 4121 | `		int *pStatus = va_arg(ap, int *);` |
|        32 | 4122 | `		if( pStatus ){` |
|        32 | 4123 | `			*pStatus = pVm->iResponseStatus;` |
|        16 | 4124 | `		}` |
|        32 | 4125 | `		break;` |
|         - | 4126 | `										}` |
|        16 | 4127 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 4128 | `		/* Iterate response headers via callback */` |
|         - | 4129 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        32 | 4130 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        32 | 4131 | `		void *pUserData = va_arg(ap, void *);` |
|        32 | 4132 | `		if( xCallback ){` |
|        32 | 4133 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        32 | 4134 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|        44 | 4135 | `			for( k = 0; k < nHdr; k++ ){` |
|        18 | 4136 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        12 | 4137 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|         6 | 4138 | `							   pUserData);` |
|        12 | 4139 | `				if( rc != PH7_OK ){` |
|       ! 0 | 4140 | `					break;` |
|         - | 4141 | `				}` |
|         6 | 4142 | `			}` |
|        16 | 4143 | `		}` |
|        32 | 4144 | `		break;` |
|         - | 4145 | `										 }` |
|       ! 0 | 4146 | `	default:` |
|         - | 4147 | `		/* Unknown configuration option */` |
|       ! 0 | 4148 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 4149 | `		break;` |
|         - | 4150 | `	}` |
|    110661 | 4151 | `	return rc;` |
|         5 | 4152 | `}` |
|         - | 4153 | `/* Forward declaration */` |
|         - | 4154 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 4155 | `/*` |
|         - | 4156 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 4157 | ` * format.` |
|         - | 4158 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 4159 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 4160 | ` * (STDOUT).` |
|         - | 4161 | ` */` |
|         2 | 4162 | `static sxi32 VmByteCodeDump(` |
|         - | 4163 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 4164 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 4165 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 4166 | `	)` |
|         1 | 4167 | `{` |
|         - | 4168 | `	static const char zDump[] = {` |
|         - | 4169 | `		"====================================================\n"` |
|         - | 4170 | `		"PH7 VM Dump\n"` |
|         - | 4171 | `		"====================================================\n"` |
|         - | 4172 | `	};` |
|         - | 4173 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 4174 | `	sxi32 rc = SXRET_OK;` |
|         - | 4175 | `	sxu32 n;` |
|         - | 4176 | `	/* Point to the PH7 instructions */` |
|         3 | 4177 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 4178 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 4179 | `	n = 0;` |
|         3 | 4180 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 4181 | `	/* Dump instructions */` |
|         6 | 4182 | `	for(;;){` |
|        13 | 4183 | `		if( pInstr >= pEnd ){` |
|         - | 4184 | `			/* No more instructions */` |
|         3 | 4185 | `			break;` |
|         - | 4186 | `		}` |
|         - | 4187 | `		/* Format and call the consumer callback */` |
|        16 | 4188 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 | 4189 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 | 4190 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 | 4191 | `		if( rc != SXRET_OK ){` |
|         - | 4192 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 4193 | `			return rc;` |
|         - | 4194 | `		}` |
|        11 | 4195 | `		++n;` |
|        11 | 4196 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 4197 | `	}` |
|         3 | 4198 | `	return rc;` |
|         2 | 4199 | `}` |
|         - | 4200 | `/*` |
|         - | 4201 | ` * Save the execution state of a fiber/generator context.` |
|         - | 4202 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 4203 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 4204 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 4205 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 4206 | ` * when VmByteCodeExec returns.` |
|         - | 4207 | ` */` |
|      1680 | 4208 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 4209 | `	ph7_vm *pVm,` |
|         - | 4210 | `	ph7_exec_ctx *pCtx,` |
|         - | 4211 | `	sxi32 pc,` |
|         - | 4212 | `	sxi32 nTos` |
|         - | 4213 | `	)` |
|         5 | 4214 | `{` |
|       840 | 4215 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1685 | 4216 | `	pCtx->pc = pc;` |
|      1685 | 4217 | `	pCtx->nTos = nTos;` |
|      1685 | 4218 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1685 | 4219 | `	return PH7_SUSPEND;` |
|         5 | 4220 | `}` |
|         - | 4221 | `/*` |
|         - | 4222 | ` * Resolve named-argument mapping.` |
|         - | 4223 | ` *` |
|         - | 4224 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 4225 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 4226 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 4227 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 4228 | ` * every formal parameter that received a value.` |
|         - | 4229 | ` *` |
|         - | 4230 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 4231 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 4232 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 4233 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 4234 | ` */` |
|       392 | 4235 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 4236 | `	ph7_vm *pVm,` |
|         - | 4237 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 4238 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 4239 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 4240 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 4241 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 4242 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 4243 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 4244 | `)` |
|         5 | 4245 | `{` |
|       397 | 4246 | `	sxi32 posIdx = 0;` |
|         - | 4247 | `	sxu32 i;` |
|       397 | 4248 | `	int bSeenNamed = 0;` |
|         - | 4249 | `	char zErrMsg[256];` |
|       397 | 4250 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1355 | 4251 | `	for( i = 0; i < nActual; i++ ){` |
|       963 | 4252 | `		aSlot[i] = -2;` |
|       484 | 4253 | `	}` |
|      1343 | 4254 | `	for( i = 0; i < nActual; i++ ){` |
|      1254 | 4255 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 4256 | `			/* Named argument — find formal by name */` |
|       599 | 4257 | `			int found = 0;` |
|       599 | 4258 | `			bSeenNamed = 1;` |
|         - | 4259 | `			sxu32 k;` |
|       903 | 4260 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       734 | 4261 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       714 | 4262 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       684 | 4263 | `						pMap->aNames[i].zString,` |
|      1026 | 4264 | `						pMap->aNames[i].nByte) == 0 ){` |
|       435 | 4265 | `					if( aUsed[k] ){` |
|        12 | 4266 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4267 | `							"Named parameter $%.*s overwrites previous argument",` |
|         6 | 4268 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         9 | 4269 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4270 | `					}` |
|       428 | 4271 | `					aSlot[i] = (sxi32)k;` |
|       428 | 4272 | `					aUsed[k] = 1;` |
|       428 | 4273 | `					found = 1;` |
|       428 | 4274 | `					break;` |
|         - | 4275 | `				}` |
|       157 | 4276 | `			}` |
|       593 | 4277 | `			if( !found ){` |
|       168 | 4278 | `				if( iVariadicIdx >= 0 ){` |
|       162 | 4279 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        83 | 4280 | `				}else{` |
|        11 | 4281 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4282 | `						"Unknown named parameter $%.*s",` |
|         6 | 4283 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         8 | 4284 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4285 | `				}` |
|        79 | 4286 | `			}` |
|       296 | 4287 | `		}else{` |
|         - | 4288 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 4289 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 4290 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 4291 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       368 | 4292 | `			if( bSeenNamed ){` |
|       ! 0 | 4293 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 4294 | `					"Cannot use positional argument after named argument",` |
|         - | 4295 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 4296 | `			}` |
|       368 | 4297 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        62 | 4298 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 4299 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4300 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 4301 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 4302 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4303 | `				}` |
|        62 | 4304 | `				aSlot[i] = posIdx;` |
|        62 | 4305 | `				aUsed[posIdx] = 1;` |
|       338 | 4306 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 4307 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 4308 | `			}` |
|       368 | 4309 | `			posIdx++;` |
|         - | 4310 | `		}` |
|       478 | 4311 | `	}` |
|       385 | 4312 | `	return SXRET_OK;` |
|       201 | 4313 | `}` |
|         - | 4314 | `/*` |
|         - | 4315 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 4316 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 4317 | ` */` |
|       406 | 4318 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         3 | 4319 | `{` |
|       409 | 4320 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       397 | 4321 | `		return 0;` |
|         - | 4322 | `	}` |
|        15 | 4323 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       206 | 4324 | `}` |
|         - | 4325 | `/*` |
|         - | 4326 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 4327 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 4328 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 4329 | ` */` |
|        10 | 4330 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4331 | `{` |
|        11 | 4332 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|         5 | 4333 | `	(void)pVm;` |
|        11 | 4334 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|        11 | 4335 | `	return SXRET_OK;` |
|         1 | 4336 | `}` |
|         - | 4337 | `/*` |
|         - | 4338 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 4339 | ` * collect values positionally (keys ignored) into a temp array.` |
|         - | 4340 | ` */` |
|         6 | 4341 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4342 | `{` |
|         3 | 4343 | `	(void)pVm; (void)pKey;` |
|         7 | 4344 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|         7 | 4345 | `	return SXRET_OK;` |
|         1 | 4346 | `}` |
|         - | 4347 | `/*` |
|         - | 4348 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 4349 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 4350 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 4351 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 4352 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 4353 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 4354 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 4355 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 4356 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 4357 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 4358 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 4359 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 4360 | ` */` |
|         - | 4361 | `/*` |
|         - | 4362 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 4363 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 4364 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 4365 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 4366 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 4367 | ` */` |
|       384 | 4368 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         3 | 4369 | `{` |
|         - | 4370 | `	VmSpreadRun sRun;` |
|         - | 4371 | `	ph7_hashmap_node *pNode;` |
|         - | 4372 | `	sxu32 i;` |
|       387 | 4373 | `	sRun.pStart = pFirst;` |
|       387 | 4374 | `	sRun.nCount = nCount;` |
|       387 | 4375 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|       387 | 4376 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       387 | 4377 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 4378 | `		return;` |
|         - | 4379 | `	}` |
|       387 | 4380 | `	pNode = pMap->pFirst;` |
|      2327 | 4381 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 4382 | `		VmSpreadKey sKey;` |
|      1943 | 4383 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 4384 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 4385 | `			 * the source map's release before CALL replays them. */` |
|       101 | 4386 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       101 | 4387 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       101 | 4388 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        51 | 4389 | `		}else{` |
|         - | 4390 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      1843 | 4391 | `			sKey.nOff = 0;` |
|      1843 | 4392 | `			sKey.nLen = 0;` |
|         - | 4393 | `		}` |
|      1943 | 4394 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      1943 | 4395 | `		pNode = pNode->pPrev; /* forward link */` |
|       973 | 4396 | `	}` |
|       195 | 4397 | `}` |
|         - | 4398 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 4399 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|         8 | 4400 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 4401 | `{` |
|         8 | 4402 | `	SySetReset(&pVm->aSpreadRun);` |
|         8 | 4403 | `	SySetReset(&pVm->aSpreadKey);` |
|         8 | 4404 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|         8 | 4405 | `}` |
|         - | 4406 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 4407 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 4408 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 4409 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 4410 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 4411 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 4412 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 4413 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 4414 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 4415 | ` * slot — from being consumed by that nested call. */` |
|       656 | 4416 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         3 | 4417 | `{` |
|       659 | 4418 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|       659 | 4419 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 4420 | `	VmSpreadRun *aRun;` |
|       659 | 4421 | `	if( rStart >= nRun ){` |
|       289 | 4422 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 4423 | `	}` |
|       373 | 4424 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       373 | 4425 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|       373 | 4426 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|       373 | 4427 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       185 | 4428 | `	}` |
|       373 | 4429 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|       331 | 4430 | `}` |
|         - | 4431 | `/*` |
|         - | 4432 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 4433 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 4434 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 4435 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 4436 | ` *` |
|         - | 4437 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 4438 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 4439 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 4440 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 4441 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 4442 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 4443 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 4444 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 4445 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 4446 | ` *` |
|         - | 4447 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 4448 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 4449 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 4450 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 4451 | ` */` |
|       776 | 4452 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         3 | 4453 | `{` |
|       779 | 4454 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4455 | `	VmSpreadRun *aRun;` |
|       779 | 4456 | `	ph7_value *pEnd = pTos;` |
|       779 | 4457 | `	sxi32 nPos = iP1;` |
|       779 | 4458 | `	sxi32 ri, extra = 0;` |
|       779 | 4459 | `	if( nRun == 0 ){` |
|        15 | 4460 | `		pVm->nSpreadCallBase = 0;` |
|        15 | 4461 | `		return 0;` |
|         - | 4462 | `	}` |
|       765 | 4463 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       765 | 4464 | `	ri = (sxi32)nRun - 1;` |
|      1797 | 4465 | `	while( nPos > 0 ){` |
|      1035 | 4466 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 4467 | `			/* A non-empty unpack occupying nCount slots. */` |
|       679 | 4468 | `			pEnd = aRun[ri].pStart;` |
|       679 | 4469 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|       679 | 4470 | `			ri--;` |
|       697 | 4471 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 4472 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|        95 | 4473 | `			extra -= 1;` |
|        95 | 4474 | `			ri--;` |
|        49 | 4475 | `		}else{` |
|         - | 4476 | `			/* An ordinary single-slot argument. */` |
|       267 | 4477 | `			pEnd--;` |
|         - | 4478 | `		}` |
|      1035 | 4479 | `		nPos--;` |
|         3 | 4480 | `	}` |
|         - | 4481 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 4482 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|       765 | 4483 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|       765 | 4484 | `	return extra;` |
|       391 | 4485 | `}` |
|       384 | 4486 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|         3 | 4487 | `{` |
|       387 | 4488 | `	ph7_value *pTos = *ppTos;` |
|       387 | 4489 | `	sxu32 nEntry = pMap->nEntry;` |
|       387 | 4490 | `	if( nEntry == 0 ){` |
|         - | 4491 | `		/* Nothing to unpack — remove the source from the stack */` |
|        49 | 4492 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|        49 | 4493 | `		VmPopOperand(&pTos, 1);` |
|        26 | 4494 | `	}else{` |
|         - | 4495 | `		ph7_hashmap_node *pNode;` |
|         - | 4496 | `		ph7_value *pElem;` |
|         - | 4497 | `		sxu32 i;` |
|         - | 4498 | `		int bTemp;` |
|         - | 4499 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|         - | 4500 | `		 * so a source array that other variables share has to separate first — otherwise` |
|         - | 4501 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|         - | 4502 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|         - | 4503 | `		 * ordinary spread pays nothing for this. */` |
|       338 | 4504 | `		if( bVarSource` |
|       259 | 4505 | `		 && pMap != pVm->pGlobal` |
|       180 | 4506 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|       183 | 4507 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|         - | 4508 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|         - | 4509 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|         - | 4510 | `			 * object. */` |
|       183 | 4511 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|       183 | 4512 | `			if( pSep ){` |
|       183 | 4513 | `				pMap = pSep;` |
|       183 | 4514 | `				nEntry = pMap->nEntry;` |
|        90 | 4515 | `			}` |
|        90 | 4516 | `		}` |
|       341 | 4517 | `		pMap->iRef++;` |
|       341 | 4518 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 4519 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 4520 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|       341 | 4521 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 4522 | `		/* Overwrite the source slot with the first element */` |
|       341 | 4523 | `		pNode = pMap->pFirst;` |
|       341 | 4524 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|       341 | 4525 | `		PH7_MemObjRelease(pTos);` |
|       341 | 4526 | `		if( pElem ){` |
|       341 | 4527 | `			if( bTemp ){` |
|       152 | 4528 | `				PH7_MemObjStore(pElem, pTos);` |
|        77 | 4529 | `			}else{` |
|       191 | 4530 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 4531 | `			}` |
|       169 | 4532 | `		}` |
|         - | 4533 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|         - | 4534 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|         - | 4535 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|         - | 4536 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|         - | 4537 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|         - | 4538 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|         - | 4539 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|         - | 4540 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|       341 | 4541 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|       341 | 4542 | `		if( !bVarSource \|\| bTemp ){` |
|       160 | 4543 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|        79 | 4544 | `		}` |
|         - | 4545 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 4546 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|       341 | 4547 | `		pNode = pNode->pPrev;` |
|         - | 4548 | `		/* Push the remaining elements */` |
|      1943 | 4549 | `		for( i = 1; i < nEntry; i++ ){` |
|      1605 | 4550 | `			pTos++;` |
|      1605 | 4551 | `			PH7_MemObjInit(pVm, pTos);` |
|      1605 | 4552 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1605 | 4553 | `			if( pElem ){` |
|      1605 | 4554 | `				if( bTemp ){` |
|      1289 | 4555 | `					PH7_MemObjStore(pElem, pTos);` |
|       645 | 4556 | `				}else{` |
|       317 | 4557 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 4558 | `				}` |
|       801 | 4559 | `			}` |
|      1605 | 4560 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|      1605 | 4561 | `			if( !bVarSource \|\| bTemp ){` |
|      1289 | 4562 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       644 | 4563 | `			}` |
|      1605 | 4564 | `			pNode = pNode->pPrev;` |
|       804 | 4565 | `		}` |
|       341 | 4566 | `		PH7_HashmapUnref(pMap);` |
|         - | 4567 | `	}` |
|       387 | 4568 | `	*ppTos = pTos;` |
|       387 | 4569 | `}` |
|         - | 4570 | `/*` |
|         - | 4571 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 4572 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 4573 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 4574 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 4575 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 4576 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 4577 | ` *` |
|         - | 4578 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 4579 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 4580 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 4581 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 4582 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 4583 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 4584 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 4585 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 4586 | ` * method-name slot pop shifts pArg).` |
|         - | 4587 | ` *` |
|         - | 4588 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 4589 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 4590 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 4591 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 4592 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 4593 | ` */` |
|       348 | 4594 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 4595 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         3 | 4596 | `{` |
|       351 | 4597 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4598 | `	VmSpreadRun *aRun;` |
|         - | 4599 | `	VmSpreadKey *aKey;` |
|         - | 4600 | `	const char *zKeyBase;` |
|       351 | 4601 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|       351 | 4602 | `	int bAnyNamed = 0;` |
|         - | 4603 | `	sxu32 ai, ci, ri, rStart;` |
|       351 | 4604 | `	if( nRun == 0 ){` |
|         - | 4605 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 4606 | `		return 0;` |
|         - | 4607 | `	}` |
|       351 | 4608 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       351 | 4609 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|       351 | 4610 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 4611 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 4612 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 4613 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 4614 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|       351 | 4615 | `	ri = pVm->nSpreadCallBase;` |
|       351 | 4616 | `	rStart = ri;` |
|       351 | 4617 | `	if( rStart >= nRun ){` |
|         - | 4618 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 4619 | `		return 0;` |
|         - | 4620 | `	}` |
|       351 | 4621 | `	SySetReset(&pVm->aEffArgName);` |
|       351 | 4622 | `	ci = 0;` |
|       351 | 4623 | `	ai = 0;` |
|       807 | 4624 | `	while( ai < nActual ){` |
|         - | 4625 | `		SyString sName;` |
|       459 | 4626 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 4627 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 4628 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|       471 | 4629 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 4630 | `			ci++; ri++;` |
|         1 | 4631 | `		}` |
|       459 | 4632 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 4633 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 4634 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 4635 | `			 * run never desyncs the key stream. */` |
|       341 | 4636 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      2281 | 4637 | `			for( j = 0; j < K; j++ ){` |
|      1943 | 4638 | `				SyZero(&sName, sizeof(sName));` |
|      1943 | 4639 | `				if( aKey[ks + j].nLen > 0 ){` |
|       101 | 4640 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       101 | 4641 | `					bAnyNamed = 1;` |
|        50 | 4642 | `				}` |
|      1943 | 4643 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       973 | 4644 | `			}` |
|       341 | 4645 | `			ai += K;` |
|       341 | 4646 | `			ci++; ri++;` |
|       172 | 4647 | `		}else{` |
|         - | 4648 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|       121 | 4649 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        35 | 4650 | `				sName = pCompile->aNames[ci];` |
|        35 | 4651 | `				bAnyNamed = 1;` |
|        17 | 4652 | `			}` |
|       121 | 4653 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       121 | 4654 | `			ai++;` |
|       121 | 4655 | `			ci++;` |
|         - | 4656 | `		}` |
|         3 | 4657 | `	}` |
|         - | 4658 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 4659 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 4660 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 4661 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|       351 | 4662 | `	VmSpreadConsume(pVm);` |
|       351 | 4663 | `	if( !bAnyNamed ){` |
|         - | 4664 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|       275 | 4665 | `		return 0;` |
|         - | 4666 | `	}` |
|        77 | 4667 | `	pEff->bHasNamed = 1;` |
|        77 | 4668 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|        77 | 4669 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|        77 | 4670 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|         - | 4671 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|         - | 4672 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|         - | 4673 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|         - | 4674 | `	 * is the caller's stack local. */` |
|        77 | 4675 | `	pEff->bArgShapes = 0;` |
|        77 | 4676 | `	pEff->nNonLvalMask = 0;` |
|        77 | 4677 | `	pEff->nTempCallMask = 0;` |
|        77 | 4678 | `	if( pCompile ){` |
|        37 | 4679 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        19 | 4680 | `	}else{` |
|        41 | 4681 | `		pEff->sAssertSrc.zString = 0;` |
|        41 | 4682 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 4683 | `	}` |
|        77 | 4684 | `	pEff->nTotal = nActual;` |
|        77 | 4685 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|        77 | 4686 | `	return 1;` |
|       177 | 4687 | `}` |
|         - | 4688 | `/*` |
|         - | 4689 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 4690 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 4691 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 4692 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 4693 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 4694 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 4695 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 4696 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 4697 | ` * pArg must be the site's FINAL argument base.` |
|         - | 4698 | ` */` |
|   5000813 | 4699 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 4700 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 4701 | `{` |
|   5000818 | 4702 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   5000818 | 4703 | `	if( pInstr->iP2 == 0 ){` |
|   5000434 | 4704 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 4705 | `	}` |
|       387 | 4706 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|        77 | 4707 | `		return pStorage;` |
|         - | 4708 | `	}` |
|       311 | 4709 | `	VmSpreadConsume(pVm);` |
|       311 | 4710 | `	return pCompile;` |
|   2501426 | 4711 | `}` |
|         - | 4712 | `/*` |
|         - | 4713 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 4714 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 4715 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 4716 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 4717 | ` */` |
|        14 | 4718 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         3 | 4719 | `{` |
|        17 | 4720 | `	const char *zType = "unknown";` |
|         - | 4721 | `	char zMsg[64];` |
|        17 | 4722 | `	if( iFlags & MEMOBJ_STRING ){` |
|         6 | 4723 | `		zType = "string";` |
|        14 | 4724 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 4725 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 4726 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 4727 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 4728 | `		zType = "float";` |
|        12 | 4729 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        10 | 4730 | `		zType = "int";` |
|         7 | 4731 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 4732 | `		zType = "bool";` |
|         1 | 4733 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 4734 | `		zType = "object";` |
|       ! 0 | 4735 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 4736 | `		zType = "resource";` |
|       ! 0 | 4737 | `	}` |
|        17 | 4738 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        17 | 4739 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        17 | 4740 | `}` |
|         - | 4741 | `/*` |
|         - | 4742 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - | 4743 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - | 4744 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - | 4745 | ` * per context — see VmMemberCtxWantsValue.` |
|         - | 4746 | ` */` |
|      1798 | 4747 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 4748 | `{` |
|      1803 | 4749 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 | 4750 | `}` |
|         - | 4751 | `/*` |
|         - | 4752 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - | 4753 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - | 4754 | ` * isset() stops at the truth.` |
|         - | 4755 | ` */` |
|        44 | 4756 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 | 4757 | `{` |
|        47 | 4758 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 | 4759 | `}` |
|         - | 4760 | `/*` |
|         - | 4761 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 4762 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 4763 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 4764 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 4765 | ` * reads (other names / other instances) still dispatch.` |
|         - | 4766 | ` */` |
|      1604 | 4767 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4768 | `{` |
|         - | 4769 | `	VmMagicGuard *aG;` |
|         - | 4770 | `	sxu32 nHash;` |
|         - | 4771 | `	sxu32 n;` |
|      1609 | 4772 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 4773 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 4774 | `		 * every hooked-property access consults the guard, often twice. */` |
|      1427 | 4775 | `		return FALSE;` |
|         - | 4776 | `	}` |
|       184 | 4777 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       184 | 4778 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       228 | 4779 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       184 | 4780 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       140 | 4781 | `			return TRUE;` |
|         - | 4782 | `		}` |
|        23 | 4783 | `	}` |
|        45 | 4784 | `	return FALSE;` |
|       807 | 4785 | `}` |
|       790 | 4786 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4787 | `{` |
|         - | 4788 | `	VmMagicGuard sG;` |
|       795 | 4789 | `	sG.pThis = pThis;` |
|       795 | 4790 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       795 | 4791 | `	sG.cKind = cKind;` |
|       795 | 4792 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|       795 | 4793 | `}` |
|       790 | 4794 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         5 | 4795 | `{` |
|       795 | 4796 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|       795 | 4797 | `}` |
|         - | 4798 | `/*` |
|         - | 4799 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 4800 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 4801 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 4802 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 4803 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 4804 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 4805 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 4806 | ` * One-token lookahead only.` |
|         - | 4807 | ` */` |
|      1146 | 4808 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 | 4809 | `{` |
|      1151 | 4810 | `	switch( pNext->iOp ){` |
|        44 | 4811 | `		case PH7_OP_STORE:` |
|        92 | 4812 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|         5 | 4813 | `		case PH7_OP_STORE_REF:` |
|        11 | 4814 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|        18 | 4815 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 4816 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4817 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4818 | `		case PH7_OP_CAT_STORE:` |
|         - | 4819 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4820 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|        37 | 4821 | `			return 1;` |
|       506 | 4822 | `		default:` |
|      1017 | 4823 | `			return 0;` |
|         - | 4824 | `	}` |
|       578 | 4825 | `}` |
|         - | 4826 | `/*` |
|         - | 4827 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|         - | 4828 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|         - | 4829 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|         - | 4830 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|         - | 4831 | ` * treat them as one.` |
|         - | 4832 | ` */` |
|       312 | 4833 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|         5 | 4834 | `{` |
|       444 | 4835 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|       455 | 4836 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|         5 | 4837 | `}` |
|         - | 4838 | `/*` |
|         - | 4839 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|         - | 4840 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|         - | 4841 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|         - | 4842 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|         - | 4843 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|         - | 4844 | `` * offsetSet where `$o['n']++` only notices.`` |
|         - | 4845 | ` */` |
|       514 | 4846 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|         5 | 4847 | `{` |
|       519 | 4848 | `	switch( pNext->iOp ){` |
|        52 | 4849 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4850 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4851 | `		case PH7_OP_CAT_STORE:` |
|         - | 4852 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4853 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       105 | 4854 | `			return 1;` |
|       205 | 4855 | `		default:` |
|       415 | 4856 | `			return 0;` |
|         - | 4857 | `	}` |
|       262 | 4858 | `}` |
|         - | 4859 | `/*` |
|         - | 4860 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 4861 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 4862 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 4863 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 4864 | ` */` |
|       534 | 4865 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         5 | 4866 | `{` |
|       539 | 4867 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         5 | 4868 | `}` |
|         - | 4869 | `/*` |
|         - | 4870 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 4871 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 4872 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 4873 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 4874 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 4875 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 4876 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 4877 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 4878 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 4879 | ` * abort path; SXRET_OK otherwise.` |
|         - | 4880 | ` */` |
|        76 | 4881 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         3 | 4882 | `{` |
|         - | 4883 | `	char zHName[384];` |
|         - | 4884 | `	sxu32 nHName;` |
|         - | 4885 | `	ph7_class_method *pSetHook;` |
|        79 | 4886 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 4887 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 4888 | `		SyBlob sErrMsg;` |
|         5 | 4889 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 4890 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 4891 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 4892 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 4893 | `		return SXRET_OK;` |
|         - | 4894 | `	}` |
|        75 | 4895 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 4896 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 4897 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 4898 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 4899 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 4900 | `			return SXRET_OK;` |
|         - | 4901 | `		}` |
|       ! 0 | 4902 | `	}` |
|        75 | 4903 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        75 | 4904 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        75 | 4905 | `	if( pSetHook ){` |
|         - | 4906 | `		ph7_value sHookRet;` |
|         - | 4907 | `		ph7_value *apHArg[1];` |
|        75 | 4908 | `		apHArg[0] = pValue;` |
|        75 | 4909 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        75 | 4910 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        75 | 4911 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        75 | 4912 | `		VmMagicGuardPop(pVm);` |
|        72 | 4913 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        41 | 4914 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         6 | 4915 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         6 | 4916 | `			if( rcH == SXRET_OK ){` |
|         6 | 4917 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|         6 | 4918 | `				if( pBack ){` |
|         6 | 4919 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         4 | 4920 | `				}` |
|         2 | 4921 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 4922 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 4923 | `				return PH7_ABORT;` |
|         - | 4924 | `			}` |
|         - | 4925 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 4926 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 4927 | `			 * parked throw. */` |
|         2 | 4928 | `		}` |
|        75 | 4929 | `		PH7_MemObjRelease(&sHookRet);` |
|        36 | 4930 | `	}` |
|        75 | 4931 | `	return SXRET_OK;` |
|        41 | 4932 | `}` |
|         - | 4933 | `/*` |
|         - | 4934 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 4935 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 4936 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 4937 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 4938 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 4939 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 4940 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 4941 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 4942 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 4943 | ` */` |
|         - | 4944 | `/*` |
|         - | 4945 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 4946 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 4947 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 4948 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 4949 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 4950 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 4951 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 4952 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 4953 | ` * caller reads the raw slot then.` |
|         - | 4954 | ` */` |
|       482 | 4955 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 | 4956 | `{` |
|       487 | 4957 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 4958 | `	char zHName[384];` |
|         - | 4959 | `	sxu32 nHName;` |
|         - | 4960 | `	ph7_class_method *pGetHook;` |
|         - | 4961 | `	sxi32 rc;` |
|       482 | 4962 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       331 | 4963 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       185 | 4964 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 4965 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 4966 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 4967 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 4968 | `		 * raw values whose output the routed throw then discards. */` |
|       307 | 4969 | `		return SXERR_NOTFOUND;` |
|         - | 4970 | `	}` |
|       185 | 4971 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       185 | 4972 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       185 | 4973 | `	if( pGetHook == 0 ){` |
|       ! 0 | 4974 | `		return SXERR_NOTFOUND;` |
|         - | 4975 | `	}` |
|       185 | 4976 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       185 | 4977 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       185 | 4978 | `	VmMagicGuardPop(pVm);` |
|       185 | 4979 | `	return rc;` |
|       246 | 4980 | `}` |
|         - | 4981 | `/*` |
|         - | 4982 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 4983 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 4984 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 4985 | ` */` |
|       130 | 4986 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 4987 | `{` |
|       131 | 4988 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 4989 | `	VmSlot sFree;` |
|       131 | 4990 | `	if( pScr ){` |
|       131 | 4991 | `		PH7_MemObjRelease(pScr);` |
|        65 | 4992 | `	}` |
|       131 | 4993 | `	sFree.nIdx = nIdx;` |
|       131 | 4994 | `	sFree.pUserData = 0;` |
|       131 | 4995 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       131 | 4996 | `}` |
|         - | 4997 | `/*` |
|         - | 4998 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 4999 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 5000 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 5001 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 5002 | ` * instance reference.` |
|         - | 5003 | ` */` |
|        22 | 5004 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 5005 | `{` |
|        24 | 5006 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        24 | 5007 | `	if( pEnt == 0 ){` |
|         5 | 5008 | `		return;` |
|         - | 5009 | `	}` |
|        19 | 5010 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        11 | 5011 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         5 | 5012 | `	}` |
|        19 | 5013 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|         - | 5014 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|         - | 5015 | `		 * property's backing store — this entry owns it. */` |
|         3 | 5016 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|         1 | 5017 | `	}` |
|        19 | 5018 | `	SyBlobRelease(&pEnt->sName);` |
|        19 | 5019 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        19 | 5020 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        13 | 5021 | `}` |
|        78 | 5022 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5023 | `{` |
|         - | 5024 | `	VmHookRmw sEnt;` |
|         - | 5025 | `	VmHookRmw *pEnt;` |
|         - | 5026 | `	ph7_value *pScr;` |
|         - | 5027 | `	ph7_value sVal;` |
|         - | 5028 | `	ph7_value sKey;` |
|        79 | 5029 | `	sxi32 rc = SXRET_OK;` |
|        79 | 5030 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        79 | 5031 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 5032 | `		return SXERR_NOTFOUND;` |
|         - | 5033 | `	}` |
|        79 | 5034 | `	sEnt = *pEnt;` |
|        79 | 5035 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 5036 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|         - | 5037 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|         - | 5038 | `	 * scratch index past this point). The DIM kind's KEY slot goes the same` |
|         - | 5039 | `	 * way, for the same reason: reserving relocates the aMemObj set. */` |
|        79 | 5040 | `	PH7_MemObjInit(pVm,&sVal);` |
|        79 | 5041 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        79 | 5042 | `	if( pScr ){` |
|        79 | 5043 | `		PH7_MemObjStore(pScr,&sVal);` |
|        39 | 5044 | `	}` |
|        79 | 5045 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        79 | 5046 | `	sVal.nIdx = SXU32_HIGH;` |
|        79 | 5047 | `	PH7_MemObjInit(pVm,&sKey);` |
|        79 | 5048 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|        41 | 5049 | `		ph7_value *pKeySlot = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|        41 | 5050 | `		if( pKeySlot ){` |
|        41 | 5051 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|        20 | 5052 | `		}` |
|        41 | 5053 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|        41 | 5054 | `		sKey.nIdx = SXU32_HIGH;` |
|        20 | 5055 | `	}` |
|        79 | 5056 | `	if( pVm->nBoundaryRc == 0 ){` |
|        79 | 5057 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|         - | 5058 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|         - | 5059 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|         - | 5060 | `			SyString sPropName;` |
|        25 | 5061 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|        25 | 5062 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|        67 | 5063 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|         - | 5064 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|         - | 5065 | `			 * back through offsetSet($key, $value). */` |
|        41 | 5066 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|         - | 5067 | `				"offsetSet",sizeof("offsetSet")-1);` |
|        41 | 5068 | `			if( pSet ){` |
|         - | 5069 | `				ph7_value *apArg[2];` |
|        41 | 5070 | `				apArg[0] = &sKey;` |
|        41 | 5071 | `				apArg[1] = &sVal;` |
|        41 | 5072 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|        20 | 5073 | `			}` |
|        21 | 5074 | `		}else{` |
|        15 | 5075 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         - | 5076 | `		}` |
|        39 | 5077 | `	}` |
|        79 | 5078 | `	SyBlobRelease(&sEnt.sName);` |
|        79 | 5079 | `	PH7_MemObjRelease(&sKey);` |
|        79 | 5080 | `	PH7_MemObjRelease(&sVal);` |
|        79 | 5081 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        79 | 5082 | `	return rc;` |
|        40 | 5083 | `}` |
|         - | 5084 | `/*` |
|         - | 5085 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 5086 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 5087 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 5088 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 5089 | ` */` |
|        54 | 5090 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         3 | 5091 | `{` |
|        57 | 5092 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|        57 | 5093 | `	if( pSetMeth ){` |
|         - | 5094 | `		ph7_value sNameVal;` |
|         - | 5095 | `		ph7_value *apSetArg[2];` |
|        57 | 5096 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|        57 | 5097 | `		sNameVal.nIdx = SXU32_HIGH;` |
|        57 | 5098 | `		apSetArg[0] = &sNameVal;` |
|        57 | 5099 | `		apSetArg[1] = pValue;` |
|        57 | 5100 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|        57 | 5101 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|        57 | 5102 | `		VmMagicGuardPop(pVm);` |
|        57 | 5103 | `		PH7_MemObjRelease(&sNameVal);` |
|        27 | 5104 | `	}` |
|        57 | 5105 | `}` |
|         - | 5106 | `/*` |
|         - | 5107 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 5108 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 5109 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 5110 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 5111 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 5112 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 5113 | ` * path (the SyHash-layout incident class).` |
|         - | 5114 | ` */` |
|         - | 5115 | `/*` |
|         - | 5116 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 5117 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 5118 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 5119 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 5120 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 5121 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 5122 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 5123 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 5124 | ` * never linked (INIT error path).` |
|         - | 5125 | ` */` |
|     27950 | 5126 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 5127 | `{` |
|     27955 | 5128 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     27955 | 5129 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 5130 | `	sxu32 i;` |
|     30219 | 5131 | `	for( i = 0 ; i < n ; ++i ){` |
|     30219 | 5132 | `		if( apStep[i] == pStep ){` |
|     27963 | 5133 | `			for( ; i + 1 < n ; ++i ){` |
|         9 | 5134 | `				apStep[i] = apStep[i + 1];` |
|         5 | 5135 | `			}` |
|     27955 | 5136 | `			(void)SySetPop(&pInfo->aStep);` |
|     27955 | 5137 | `			return;` |
|         - | 5138 | `		}` |
|      1137 | 5139 | `	}` |
|     13980 | 5140 | `}` |
|       366 | 5141 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 5142 | `{` |
|       371 | 5143 | `	if( pStep->pOwner ){` |
|        77 | 5144 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|        37 | 5145 | `	}` |
|       371 | 5146 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       371 | 5147 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       371 | 5148 | `	PH7_ClassInstanceUnref(pThis);` |
|       371 | 5149 | `}` |
|         - | 5150 | `/*` |
|         - | 5151 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 5152 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 5153 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 5154 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 5155 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 5156 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 5157 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 5158 | ` * map's last reference.` |
|         - | 5159 | ` */` |
|     27526 | 5160 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 5161 | `{` |
|     27531 | 5162 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     27531 | 5163 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     27531 | 5164 | `	if( bPop ){` |
|         - | 5165 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 5166 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     27531 | 5167 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     13763 | 5168 | `	}` |
|     27531 | 5169 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     27531 | 5170 | `	PH7_HashmapUnref(pMap);` |
|     27531 | 5171 | `}` |
|         - | 5172 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 5173 | `/*` |
|         - | 5174 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 5175 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5176 | ` * See block-comment on that function for additional information.` |
|         - | 5177 | ` */` |
|   1585754 | 5178 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 5179 | `{` |
|         - | 5180 | `	ph7_value *pStack;` |
|         - | 5181 | `	sxu32 nCap;` |
|         - | 5182 | `	sxi32 rc;` |
|         - | 5183 | `	/* Allocate a new operand stack */` |
|   1585759 | 5184 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   1585759 | 5185 | `	if( pStack == 0 ){` |
|       ! 0 | 5186 | `		return SXERR_MEM;` |
|         - | 5187 | `	}` |
|   1585759 | 5188 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 5189 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 5190 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   1585759 | 5191 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 5192 | `	/* Free the operand stack */` |
|   1585759 | 5193 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 5194 | `	/* Execution result */` |
|   1585759 | 5195 | `	return rc;` |
|    792882 | 5196 | `}` |
|         - | 5197 | `/*` |
|         - | 5198 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - | 5199 | ` * statement must honour?` |
|         - | 5200 | ` *` |
|         - | 5201 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - | 5202 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - | 5203 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - | 5204 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - | 5205 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - | 5206 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - | 5207 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - | 5208 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - | 5209 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - | 5210 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - | 5211 | ` * drift apart.` |
|         - | 5212 | ` *` |
|         - | 5213 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - | 5214 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - | 5215 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - | 5216 | ` * this opcode did not create as its own throw.` |
|         - | 5217 | ` *` |
|         - | 5218 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - | 5219 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - | 5220 | ` */` |
|         - | 5221 | `/*` |
|         - | 5222 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - | 5223 | ` *` |
|         - | 5224 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - | 5225 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - | 5226 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - | 5227 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - | 5228 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - | 5229 | ` * captured value writes over the superglobal's own slot: calling` |
|         - | 5230 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - | 5231 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - | 5232 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - | 5233 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - | 5234 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - | 5235 | ` *` |
|         - | 5236 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - | 5237 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - | 5238 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - | 5239 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - | 5240 | ` */` |
|      1922 | 5241 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 | 5242 | `{` |
|         - | 5243 | `	static const char *const azAuto[] = {` |
|         - | 5244 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - | 5245 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - | 5246 | `	};` |
|         - | 5247 | `	sxu32 n;` |
|     20709 | 5248 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|     18831 | 5249 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|     18831 | 5250 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|        47 | 5251 | `			return 1;` |
|         - | 5252 | `		}` |
|      9396 | 5253 | `	}` |
|      1883 | 5254 | `	return 0;` |
|       966 | 5255 | `}` |
|     15416 | 5256 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 | 5257 | `{` |
|     15401 | 5258 | `	return rc == PH7_EXCEPTION` |
|     15396 | 5259 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|     23104 | 5260 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 | 5261 | `}` |
|         - | 5262 | `/*` |
|         - | 5263 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 5264 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 5265 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 5266 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 5267 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 5268 | ` */` |
|       140 | 5269 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         1 | 5270 | `{` |
|       141 | 5271 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       141 | 5272 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 5273 | `	sxi32 rc;` |
|       141 | 5274 | `	if( pDeclCls ){` |
|       125 | 5275 | `		pVm->pConstEvalClass = pDeclCls;` |
|       125 | 5276 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        62 | 5277 | `	}` |
|       141 | 5278 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       141 | 5279 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       141 | 5280 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       141 | 5281 | `	return rc;` |
|         1 | 5282 | `}` |
|         - | 5283 | `/*` |
|         - | 5284 | ` * Invoke any installed shutdown callbacks.` |
|         - | 5285 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|         - | 5286 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|         - | 5287 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|         - | 5288 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|         - | 5289 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|         - | 5290 | ` *` |
|         - | 5291 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|         - | 5292 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|         - | 5293 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|         - | 5294 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|         - | 5295 | ` * down and restore the default consumer.` |
|         - | 5296 | ` */` |
|      4078 | 5297 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 5298 | `{` |
|      4083 | 5299 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         - | 5300 | `	sxu32 n,nUsed;` |
|      4083 | 5301 | `	nUsed = SySetUsed(&pVm->aOB);` |
|      4083 | 5302 | `	if( nUsed < 1 ){` |
|      4081 | 5303 | `		return;` |
|         - | 5304 | `	}` |
|         7 | 5305 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|         5 | 5306 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|         5 | 5307 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|         5 | 5308 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|         5 | 5309 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|         2 | 5310 | `		}` |
|         3 | 5311 | `	}` |
|         - | 5312 | `	/* Restore the default consumer and release the buffers. */` |
|         3 | 5313 | `	pCons->xConsumer = pCons->xDef;` |
|         3 | 5314 | `	pCons->pUserData = pCons->pDefData;` |
|         7 | 5315 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|         5 | 5316 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|         5 | 5317 | `		if( pOb ){` |
|         5 | 5318 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|         5 | 5319 | `			SyBlobRelease(&pOb->sOB);` |
|         2 | 5320 | `		}` |
|         3 | 5321 | `	}` |
|         3 | 5322 | `	SySetReset(&pVm->aOB);` |
|         3 | 5323 | `	pVm->nObDepth = 0;` |
|      2044 | 5324 | `}` |
|         - | 5325 | `/*` |
|         - | 5326 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 5327 | ` * or more calls to [register_shutdown_function()].` |
|         - | 5328 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 5329 | ` * execution ends.` |
|         - | 5330 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 5331 | ` * additional information.` |
|         - | 5332 | ` */` |
|      4078 | 5333 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 5334 | `{` |
|         - | 5335 | `	VmShutdownCB *pEntry;` |
|         - | 5336 | `	ph7_value *apArg[10];` |
|         - | 5337 | `	sxu32 n,nEntry;` |
|         - | 5338 | `	int i;` |
|         - | 5339 | `	/* Point to the stack of registered callbacks */` |
|      4083 | 5340 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     44863 | 5341 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     40785 | 5342 | `		apArg[i] = 0;` |
|     20395 | 5343 | `	}` |
|         - | 5344 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 5345 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 5346 | `	 * callbacks, mirroring PHP.` |
|         - | 5347 | `	 */` |
|      4083 | 5348 | `	pVm->bHaltRequested = 0;` |
|      4101 | 5349 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        23 | 5350 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        23 | 5351 | `		if( pEntry ){` |
|         - | 5352 | `			/* Prepare callback arguments if any */` |
|        23 | 5353 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 5354 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 5355 | `					break;` |
|         - | 5356 | `				}` |
|       ! 0 | 5357 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 5358 | `			}` |
|         - | 5359 | `			/* Invoke the callback */` |
|        23 | 5360 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 5361 | `			/*` |
|         - | 5362 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 5363 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 5364 | `			 */` |
|        23 | 5365 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        23 | 5366 | `			if( pEntry ){` |
|        23 | 5367 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        23 | 5368 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 5369 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 5370 | `				}` |
|         9 | 5371 | `			}` |
|        23 | 5372 | `			if( pVm->bHaltRequested ){` |
|         - | 5373 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|       ! 0 | 5374 | `				break;` |
|         - | 5375 | `			}` |
|         9 | 5376 | `		}` |
|        14 | 5377 | `	}` |
|      4083 | 5378 | `	SySetReset(&pVm->aShutdown);` |
|      4083 | 5379 | `}` |
|         - | 5380 | `/*` |
|         - | 5381 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 5382 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5383 | ` * See block-comment on that function for additional information.` |
|         - | 5384 | ` */` |
|      4078 | 5385 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 5386 | `{` |
|         - | 5387 | `	/* Make sure we are ready to execute this program */` |
|      4083 | 5388 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 5389 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 5390 | `	}` |
|         - | 5391 | `	/* Set the execution magic number  */` |
|      4083 | 5392 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 5393 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 5394 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 5395 | `	{` |
|      4083 | 5396 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|      4083 | 5397 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 5398 | `	}` |
|         - | 5399 | `	/* Invoke any shutdown callbacks */` |
|      4083 | 5400 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 5401 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 5402 | `	 * shutdown callbacks, which may still write into them. */` |
|      4083 | 5403 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 5404 | `	/*` |
|         - | 5405 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 5406 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 5407 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 5408 | `	 */` |
|      4083 | 5409 | `	return SXRET_OK;` |
|      2044 | 5410 | `}` |
|         - | 5411 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 5412 | `/*` |
|         - | 5413 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 5414 | ` * the desired message.` |
|         - | 5415 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 5416 | ` * in 'api.c' for additional information.` |
|         - | 5417 | ` */` |
|     11984 | 5418 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 5419 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 5420 | `	SyString *pString /* Message to output */` |
|         - | 5421 | `	)` |
|         5 | 5422 | `{` |
|     11989 | 5423 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     11989 | 5424 | `	sxi32 rc = SXRET_OK;` |
|         - | 5425 | `	/* Call the output consumer */` |
|     11989 | 5426 | `	if( pString->nByte > 0 ){` |
|     11989 | 5427 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     11989 | 5428 | `		VmTrackOutput(pVm, pString->nByte);` |
|      5992 | 5429 | `	}` |
|     11989 | 5430 | `	return rc;` |
|         5 | 5431 | `}` |
|         - | 5432 | `/*` |
|         - | 5433 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 5434 | ` * callback to consume the formatted message.` |
|         - | 5435 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 5436 | ` * in 'api.c' for additional information.` |
|         - | 5437 | ` */` |
|         2 | 5438 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 5439 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 5440 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 5441 | `	va_list ap           /* Variable list of arguments */` |
|         - | 5442 | `	)` |
|         1 | 5443 | `{` |
|         3 | 5444 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         3 | 5445 | `	sxi32 rc = SXRET_OK;` |
|         - | 5446 | `	SyBlob sWorker;` |
|         - | 5447 | `	/* Format the message and call the output consumer */` |
|         3 | 5448 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|         3 | 5449 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|         3 | 5450 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 5451 | `		/* Consume the formatted message */` |
|         3 | 5452 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|         1 | 5453 | `	}` |
|         3 | 5454 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 5455 | `	/* Release the working buffer */` |
|         3 | 5456 | `	SyBlobRelease(&sWorker);` |
|         3 | 5457 | `	return rc;` |
|         1 | 5458 | `}` |
|         - | 5459 | `/*` |
|         - | 5460 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 5461 | ` * This function never fail and always return a pointer` |
|         - | 5462 | ` * to a null terminated string.` |
|         - | 5463 | ` */` |
|        10 | 5464 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 5465 | `{` |
|        11 | 5466 | `	const char *zOp = "Unknown     ";` |
|        11 | 5467 | `	switch(nOp){` |
|         3 | 5468 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 5469 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 5470 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 5471 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 5472 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 5473 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 5474 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 5475 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 5476 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 5477 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 5478 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 5479 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 5480 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 5481 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 5482 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 5483 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 5484 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 5485 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 5486 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 5487 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 5488 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 5489 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|       ! 0 | 5490 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|       ! 0 | 5491 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 5492 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 5493 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 5494 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 5495 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 5496 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 5497 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 5498 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 5499 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 5500 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 5501 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 5502 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 5503 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 5504 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 5505 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 5506 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 5507 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 5508 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 5509 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 5510 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 5511 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 5512 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 5513 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 5514 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 5515 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 5516 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 5517 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 5518 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 5519 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 5520 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 5521 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 5522 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 | 5523 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 5524 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 5525 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 5526 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 5527 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 5528 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 5529 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 5530 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 5531 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 5532 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 5533 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 5534 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 5535 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 5536 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 5537 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 5538 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 5539 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 5540 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 5541 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 5542 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 5543 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 5544 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 5545 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 5546 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 5547 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 5548 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 5549 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 5550 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 5551 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 5552 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 5553 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 5554 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 5555 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 5556 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 5557 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 5558 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 5559 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 5560 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 5561 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 5562 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 5563 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 5564 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 5565 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 5566 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 5567 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 5568 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 5569 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 5570 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 5571 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 5572 | `	default:` |
|       ! 0 | 5573 | `		break;` |
|         - | 5574 | `	}` |
|        11 | 5575 | `	return zOp;` |
|         1 | 5576 | `}` |
|         - | 5577 | `/*` |
|         - | 5578 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 5579 | ` * The xConsumer() callback which is an used defined function` |
|         - | 5580 | ` * is responsible of consuming the generated dump.` |
|         - | 5581 | ` */` |
|         2 | 5582 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 5583 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 5584 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 5585 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 5586 | `	)` |
|         1 | 5587 | `{` |
|         - | 5588 | `	sxi32 rc;` |
|         3 | 5589 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 5590 | `	return rc;` |
|         1 | 5591 | `}` |
|         - | 5592 | `/*` |
|         - | 5593 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 5594 | ` * outside a class body [i.e: global or function scope].` |
|         - | 5595 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 5596 | ` * in 'compile.c' for additional information.` |
|         - | 5597 | ` */` |
|    100182 | 5598 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         5 | 5599 | `{` |
|    100187 | 5600 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 5601 | `	/* Evaluate and expand constant value */` |
|    100187 | 5602 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|    100187 | 5603 | `}` |
|         - | 5604 | `/*` |
|         - | 5605 | ` * Section:` |
|         - | 5606 | ` *  Function handling functions.` |
|         - | 5607 | ` * Status:` |
|         - | 5608 | ` *    Stable.` |
|         - | 5609 | ` */` |
|         - | 5610 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 5611 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 5612 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 5613 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 5614 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 5615 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 5616 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 5617 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 5618 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 5619 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 5620 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 5621 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 5622 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 5623 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 5624 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 5625 | `	    /* Constants management */` |
|         - | 5626 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 5627 | `	{ "define",   vm_builtin_define               },` |
|         - | 5628 | `	{ "constant", vm_builtin_constant             },` |
|         - | 5629 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 5630 | `	   /* Class/Object functions */` |
|         - | 5631 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 5632 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 5633 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 5634 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 5635 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 5636 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 5637 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 5638 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 5639 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 5640 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 5641 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 5642 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 5643 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 5644 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 5645 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 5646 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 5647 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 5648 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|         - | 5649 | `	{ "clone",           vm_builtin_clone             },` |
|         - | 5650 | `	   /* SPL object identity */` |
|         - | 5651 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 5652 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 5653 | `	   /* SPL Autoloading */` |
|         - | 5654 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 5655 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 5656 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 5657 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 5658 | `	   /* Random numbers/strings generators */` |
|         - | 5659 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 5660 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 5661 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 5662 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 5663 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 5664 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 5665 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 5666 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5667 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 5668 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 5669 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 5670 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5671 | `	   /* Language constructs functions */` |
|         - | 5672 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 5673 | `	{ "print", vm_builtin_print                   },` |
|         - | 5674 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 5675 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 5676 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 5677 | `	  /* Variable handling functions */` |
|         - | 5678 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 5679 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 5680 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|         - | 5681 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 5682 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 5683 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 5684 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 5685 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 5686 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 5687 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 5688 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 5689 | `	  /* Ouput control functions */` |
|         - | 5690 | `	{ "flush",        vm_builtin_ob_flush          },` |
|         - | 5691 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 5692 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 5693 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 5694 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 5695 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 5696 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 5697 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|         - | 5698 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 5699 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 5700 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 5701 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|         - | 5702 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 5703 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 5704 | `	  /* Assertion functions */` |
|         - | 5705 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 5706 | `	  /* Error reporting functions */` |
|         - | 5707 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 5708 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 5709 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 5710 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 5711 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 5712 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 5713 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 5714 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 5715 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 5716 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 5717 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 5718 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 5719 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 5720 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 5721 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 5722 | `	  /* Release info */` |
|         - | 5723 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 5724 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 5725 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|         - | 5726 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|         - | 5727 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 5728 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 5729 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 5730 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 5731 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 5732 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 5733 | `	  /* hashmap */` |
|         - | 5734 | `	{"compact",          vm_builtin_compact       },` |
|         - | 5735 | `	{"extract",          vm_builtin_extract       },` |
|         - | 5736 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 5737 | `	  /* URL related function */` |
|         - | 5738 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 5739 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 5740 | `	   /* Command line processing */` |
|         - | 5741 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 5742 | `	   /* JSON encoding/decoding */` |
|         - | 5743 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 5744 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 5745 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 5746 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 5747 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 5748 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 5749 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 5750 | `	   /* Files/URI inclusion facility */` |
|         - | 5751 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 5752 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 5753 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 5754 | `	{ "include",      vm_builtin_include          },` |
|         - | 5755 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 5756 | `	{ "require",      vm_builtin_require          },` |
|         - | 5757 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 5758 | `};` |
|         - | 5759 | `/*` |
|         - | 5760 | ` * Register the built-in VM functions defined above.` |
|         - | 5761 | ` */` |
|      4076 | 5762 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 5763 | `{` |
|         - | 5764 | `	sxi32 rc;` |
|         - | 5765 | `	sxu32 n;` |
|    509505 | 5766 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 5767 | `		/* Note that these special functions have access` |
|         - | 5768 | `		 * to the underlying virtual machine as their` |
|         - | 5769 | `		 * private data.` |
|         - | 5770 | `		 */` |
|    505429 | 5771 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    505429 | 5772 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 5773 | `			return rc;` |
|         - | 5774 | `		}` |
|    252717 | 5775 | `	}` |
|      4081 | 5776 | `	return SXRET_OK;` |
|      2043 | 5777 | `}` |
|         - | 5778 | `/*` |
|         - | 5779 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 5780 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 5781 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 5782 | ` */` |
|   7684220 | 5783 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 5784 | `{` |
|   7684225 | 5785 | `	if( !iLoadable ){` |
|   5116439 | 5786 | `		return pClass;` |
|         - | 5787 | `	}` |
|   2567807 | 5788 | `	while(pClass){` |
|   2567791 | 5789 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   2567775 | 5790 | `			return pClass;` |
|         - | 5791 | `		}` |
|        17 | 5792 | `		pClass = pClass->pNextName;` |
|         1 | 5793 | `	}` |
|        17 | 5794 | `	return 0;` |
|   3842115 | 5795 | `}` |
|         - | 5796 | `/*` |
|         - | 5797 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 5798 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 5799 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 5800 | ` * registered in the VM's class table.` |
|         - | 5801 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 5802 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 5803 | ` */` |
|       428 | 5804 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5805 | `{` |
|         - | 5806 | `	VmAutoloadCB *pEntry;` |
|         - | 5807 | `	ph7_value sArg,sResult;` |
|         - | 5808 | `	SyHashEntry *pHashEntry;` |
|         - | 5809 | `	ph7_class *pClass;` |
|         - | 5810 | `	sxu32 n,nEntry;` |
|       433 | 5811 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       433 | 5812 | `	if( nEntry < 1 ){` |
|       291 | 5813 | `		return 0;` |
|         - | 5814 | `	}` |
|         - | 5815 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       147 | 5816 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 5817 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 5818 | `	}` |
|         - | 5819 | `	/* Mark this class as being autoloaded */` |
|       145 | 5820 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 5821 | `	/* Prepare the class name argument */` |
|       145 | 5822 | `	PH7_MemObjInit(pVm,&sArg);` |
|       145 | 5823 | `	PH7_MemObjInit(pVm,&sResult);` |
|       145 | 5824 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       145 | 5825 | `	pClass = 0;` |
|       267 | 5826 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 5827 | `		ph7_value *apArg[1];` |
|       155 | 5828 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       155 | 5829 | `		if( pEntry == 0 ){` |
|       ! 0 | 5830 | `			continue;` |
|         - | 5831 | `		}` |
|       155 | 5832 | `		apArg[0] = &sArg;` |
|       155 | 5833 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|         - | 5834 | `			/* Callback could not be invoked — skip to next autoloader */` |
|        24 | 5835 | `			continue;` |
|         - | 5836 | `		}` |
|         - | 5837 | `		/* Check if the class is now available */` |
|       133 | 5838 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       133 | 5839 | `		if( pHashEntry ){` |
|        33 | 5840 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        33 | 5841 | `			if( pClass ){` |
|        33 | 5842 | `				break;` |
|         - | 5843 | `			}` |
|       ! 0 | 5844 | `		}` |
|        55 | 5845 | `	}` |
|       145 | 5846 | `	PH7_MemObjRelease(&sArg);` |
|       145 | 5847 | `	PH7_MemObjRelease(&sResult);` |
|         - | 5848 | `	/* Remove reentrancy guard */` |
|       145 | 5849 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       145 | 5850 | `	return pClass;` |
|       219 | 5851 | `}` |
|         - | 5852 | `/*` |
|         - | 5853 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 5854 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 5855 | ` */` |
|        48 | 5856 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         4 | 5857 | `{` |
|        52 | 5858 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         4 | 5859 | `}` |
|         - | 5860 | `/*` |
|         - | 5861 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 5862 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 5863 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 5864 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 5865 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 5866 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 5867 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 5868 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 5869 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 5870 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 5871 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 5872 | ` */` |
|   9024292 | 5873 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 5874 | `{` |
|   9024297 | 5875 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        68 | 5876 | `		(*pzName)++;` |
|        68 | 5877 | `		(*pnByte)--;` |
|        32 | 5878 | `	}` |
|   9024297 | 5879 | `}` |
|         - | 5880 | `/*` |
|         - | 5881 | ` * Check if the given name refer to an installed class.` |
|         - | 5882 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 5883 | ` */` |
|   7684576 | 5884 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 5885 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 5886 | `	const char *zName,  /* Name of the target class */` |
|         - | 5887 | `	sxu32 nByte,        /* zName length */` |
|         - | 5888 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 5889 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 5890 | `						 */` |
|         - | 5891 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 5892 | `	)` |
|         5 | 5893 | `{` |
|         - | 5894 | `	SyHashEntry *pEntry;` |
|         - | 5895 | `	ph7_class *pClass;` |
|   7684581 | 5896 | `	sxu32 nOrig = nByte;` |
|   3842288 | 5897 | `	SXUNUSED(iNest);` |
|         - | 5898 | `	/* Exact class lookup.` |
|         - | 5899 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 5900 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 5901 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 5902 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   7684581 | 5903 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 5904 | `	/* An empty stripped name never matches a stored key (none is empty); skip the` |
|         - | 5905 | `	 * hash probe. But php still fires the autoloader when the ORIGINAL name was` |
|         - | 5906 | `	 * non-empty — a lone "\" autoloads with the empty stripped name, whereas a` |
|         - | 5907 | `	 * truly empty "" does not. Gate autoload on nOrig, pass the stripped name. */` |
|   7684581 | 5908 | `	pEntry = nByte > 0 ? SyHashGet(&pVm->hClass,(const void *)zName,nByte) : 0;` |
|   7684581 | 5909 | `	if( pEntry == 0 ){` |
|         - | 5910 | `		/* Class not found in hash table — try autoload before giving up */` |
|       389 | 5911 | `		return nOrig > 0 ? VmTriggerAutoload(pVm,zName,nByte,iLoadable) : 0;` |
|         - | 5912 | `	}` |
|   7684197 | 5913 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   7684197 | 5914 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   3842293 | 5915 | `}` |
|         - | 5916 | `/*` |
|         - | 5917 | ` * Reference Table Implementation` |
|         - | 5918 | ` * Status: stable <chm@symisc.net>` |
|         - | 5919 | ` * Intro` |
|         - | 5920 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 5921 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 5922 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 5923 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 5924 | ` *  Refer to the official for more information on this powerful` |
|         - | 5925 | ` *  extension.` |
|         - | 5926 | ` */` |
|         - | 5927 | `/*` |
|         - | 5928 | ` * Allocate a new reference entry.` |
|         - | 5929 | ` */` |
|  18963368 | 5930 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 5931 | `{` |
|         - | 5932 | `	VmRefObj *pRef;` |
|         - | 5933 | `	/* Allocate a new instance */` |
|  18963373 | 5934 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  18963373 | 5935 | `	if( pRef == 0 ){` |
|       ! 0 | 5936 | `		return 0;` |
|         - | 5937 | `	}` |
|         - | 5938 | `	/* Zero the structure */` |
|  18963373 | 5939 | `	SyZero(pRef,sizeof(VmRefObj));` |
|         - | 5940 | `	/* Initialize fields */` |
|  18963373 | 5941 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  18963373 | 5942 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  18963373 | 5943 | `	pRef->nIdx = nIdx;` |
|  18963373 | 5944 | `	return pRef;` |
|   9482961 | 5945 | `}` |
|         - | 5946 | `/*` |
|         - | 5947 | ` * Default hash function used by the reference table` |
|         - | 5948 | ` * for lookup/insertion operations.` |
|         - | 5949 | ` */` |
| 102310216 | 5950 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|         5 | 5951 | `{` |
|         - | 5952 | `	/* Calculate the hash based on the memory object index */` |
| 102310221 | 5953 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|         5 | 5954 | `}` |
|         - | 5955 | `/*` |
|         - | 5956 | ` * Check if a memory object [i.e: a variable] is already installed` |
|         - | 5957 | ` * in the reference table.` |
|         - | 5958 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|         - | 5959 | ` * otherwise.` |
|         - | 5960 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5961 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5962 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5963 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5964 | ` * Refer to the official for more information on this powerful` |
|         - | 5965 | ` * extension.` |
|         - | 5966 | ` */` |
|  62717784 | 5967 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|         5 | 5968 | `{` |
|         - | 5969 | `	VmRefObj *pRef;` |
|         - | 5970 | `	sxu32 nBucket;` |
|         - | 5971 | `	/* Point to the appropriate bucket */` |
|  62717789 | 5972 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|         - | 5973 | `	/* Perform the lookup */` |
|  62717789 | 5974 | `	pRef = pVm->apRefObj[nBucket];` |
| 200487565 | 5975 | `	for(;;){` |
| 400951201 | 5976 | `		if( pRef == 0 ){` |
|  18963411 | 5977 | `			break;` |
|         - | 5978 | `		}` |
| 381987795 | 5979 | `		if( pRef->nIdx == nObjIdx ){` |
|         - | 5980 | `			/* Entry found */` |
|  43754383 | 5981 | `			return pRef;` |
|         - | 5982 | `		}` |
|         - | 5983 | `		/* Point to the next entry */` |
| 338233417 | 5984 | `		pRef = pRef->pNextCollide;` |
|         5 | 5985 | `	}` |
|         - | 5986 | `	/* No such entry,return NULL */` |
|  18963411 | 5987 | `	return 0;` |
|  31363985 | 5988 | `}` |
|         - | 5989 | `/*` |
|         - | 5990 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 5991 | ` *` |
|         - | 5992 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5993 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5994 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5995 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5996 | ` * Refer to the official for more information on this powerful` |
|         - | 5997 | ` * extension.` |
|         - | 5998 | ` */` |
|  18963368 | 5999 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6000 | `{` |
|         - | 6001 | `	sxu32 nBucket;` |
|  18963373 | 6002 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|         - | 6003 | `		VmRefObj **apNew;` |
|         - | 6004 | `		sxu32 nNew;` |
|         - | 6005 | `		/* Allocate a larger table */` |
|     10235 | 6006 | `		nNew = pVm->nRefSize << 1;` |
|     10235 | 6007 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     10235 | 6008 | `		if( apNew ){` |
|     10235 | 6009 | `			VmRefObj *pEntry = pVm->pRefList;` |
|         - | 6010 | `			sxu32 n;` |
|         - | 6011 | `			/* Zero the structure */` |
|     10235 | 6012 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|         - | 6013 | `			/* Rehash all referenced entries */` |
|   4340789 | 6014 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|         - | 6015 | `				/* Remove old collision links */` |
|   4330559 | 6016 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 6017 | `				/* Point to the appropriate bucket */` |
|   4330559 | 6018 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|         - | 6019 | `				/* Insert the entry  */` |
|   4330559 | 6020 | `				pEntry->pNextCollide = apNew[nBucket];` |
|   4330559 | 6021 | `				if( apNew[nBucket] ){` |
|   3566283 | 6022 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|   1783139 | 6023 | `				}` |
|   4330559 | 6024 | `				apNew[nBucket] = pEntry;` |
|         - | 6025 | `				/* Point to the next entry */` |
|   4330559 | 6026 | `				pEntry = pEntry->pNext;` |
|   2165282 | 6027 | `			}` |
|         - | 6028 | `			/* Release the old table */` |
|     10235 | 6029 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|         - | 6030 | `			/* Install the new one */` |
|     10235 | 6031 | `			pVm->apRefObj = apNew;` |
|     10235 | 6032 | `			pVm->nRefSize = nNew;` |
|      5115 | 6033 | `		}` |
|      5115 | 6034 | `	}` |
|         - | 6035 | `	/* Point to the appropriate bucket */` |
|  18963373 | 6036 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|         - | 6037 | `	/* Insert the entry */` |
|  18963373 | 6038 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  18963373 | 6039 | `	if( pVm->apRefObj[nBucket] ){` |
|  16852565 | 6040 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   8427217 | 6041 | `	}` |
|  18963373 | 6042 | `	pVm->apRefObj[nBucket] = pRef;` |
|  18963373 | 6043 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  18963373 | 6044 | `	pVm->nRefUsed++;` |
|  18963373 | 6045 | `	return SXRET_OK;` |
|         5 | 6046 | `}` |
|         - | 6047 | `/*` |
|         - | 6048 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|         - | 6049 | ` * the reference table.` |
|         - | 6050 | ` * This function is invoked when the user perform an unset` |
|         - | 6051 | ` * call [i.e: unset($var); ].` |
|         - | 6052 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6053 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6054 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6055 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6056 | ` * Refer to the official for more information on this powerful` |
|         - | 6057 | ` * extension.` |
|         - | 6058 | ` */` |
|  18111460 | 6059 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6060 | `{` |
|         - | 6061 | `	ph7_hashmap_node **apNode;` |
|         - | 6062 | `	SyHashEntry **apEntry;` |
|         - | 6063 | `	sxu32 n;` |
|         - | 6064 | `	/* Point to the reference table */` |
|  18111465 | 6065 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  18111465 | 6066 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|         - | 6067 | `	/* Unlink the entry from the reference table */` |
|  19195595 | 6068 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1084135 | 6069 | `		if( apEntry[n] ){` |
|        95 | 6070 | `			SyHashDeleteEntry2(apEntry[n]);` |
|        47 | 6071 | `		}` |
|    543342 | 6072 | `	}` |
|  25641919 | 6073 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   7530459 | 6074 | `		if( apNode[n] ){` |
|       253 | 6075 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|       126 | 6076 | `		}` |
|   3765232 | 6077 | `	}` |
|  18111465 | 6078 | `	if( pRef->pPrevCollide ){` |
|   1812955 | 6079 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|    906315 | 6080 | `	}else{` |
|  16298515 | 6081 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|         - | 6082 | `	}` |
|  18111465 | 6083 | `	if( pRef->pNextCollide ){` |
|  15067794 | 6084 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   7534824 | 6085 | `	}` |
|  18111465 | 6086 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|         - | 6087 | `	/* Release the node */` |
|  18111465 | 6088 | `	SySetRelease(&pRef->aReference);` |
|  18111465 | 6089 | `	SySetRelease(&pRef->aArrEntries);` |
|  18111465 | 6090 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  18111465 | 6091 | `	pVm->nRefUsed--;` |
|  18111465 | 6092 | `	return SXRET_OK;` |
|         5 | 6093 | `}` |
|         - | 6094 | `/*` |
|         - | 6095 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6096 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6097 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6098 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6099 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6100 | ` * Refer to the official for more information on this powerful` |
|         - | 6101 | ` * extension.` |
|         - | 6102 | ` */` |
|  19023270 | 6103 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 6104 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6105 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6106 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6107 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 6108 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 6109 | `	)` |
|         5 | 6110 | `{` |
|  19023275 | 6111 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 6112 | `	VmRefObj *pRef;` |
|         - | 6113 | `	/* Check if the referenced object already exists */` |
|  19023275 | 6114 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  19023275 | 6115 | `	if( pRef == 0 ){` |
|         - | 6116 | `		/* Create a new entry */` |
|  18963373 | 6117 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  18963373 | 6118 | `		if( pRef == 0 ){` |
|       ! 0 | 6119 | `			return SXERR_MEM;` |
|         - | 6120 | `		}` |
|  18963373 | 6121 | `		pRef->iFlags = iFlags;` |
|         - | 6122 | `		/* Install the entry */` |
|  18963373 | 6123 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   9482956 | 6124 | `	}` |
|  19023275 | 6125 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  19023275 | 6126 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 6127 | `		VmSlot sRef;` |
|         - | 6128 | `		/* Local frame,record referenced entry so that it can` |
|         - | 6129 | `		 * be deleted when we leave this frame.` |
|         - | 6130 | `		 */` |
|   1077811 | 6131 | `		sRef.nIdx = nIdx;` |
|   1077811 | 6132 | `		sRef.pUserData = pEntry;` |
|   1077811 | 6133 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 6134 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 6135 | `		}` |
|    540175 | 6136 | `	}` |
|  19023275 | 6137 | `	if( pEntry ){` |
|         - | 6138 | `		/* Address of the hash-entry (into a row a dead holder left behind — a name can` |
|         - | 6139 | `		 * be RE-BOUND to the same slot any number of times, and a set that only ever` |
|         - | 6140 | `		 * grew made both the install and the holder count O(rows)) */` |
|   1134061 | 6141 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   1134061 | 6142 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   1134295 | 6143 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|       239 | 6144 | `			if( apEntry[n] == pEntry ){` |
|       ! 0 | 6145 | `				nFree = SXU32_HIGH; /* already recorded: never file one holder twice */` |
|       ! 0 | 6146 | `				break;` |
|         - | 6147 | `			}` |
|       239 | 6148 | `			if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|        51 | 6149 | `				nFree = n;` |
|        24 | 6150 | `			}` |
|       122 | 6151 | `		}` |
|   1134061 | 6152 | `		if( n >= SySetUsed(&pRef->aReference) ){` |
|   1134061 | 6153 | `			if( nFree != SXU32_HIGH ){` |
|        51 | 6154 | `				apEntry[nFree] = pEntry;` |
|        27 | 6155 | `			}else{` |
|   1134013 | 6156 | `				SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|         - | 6157 | `			}` |
|    568300 | 6158 | `		}` |
|    568300 | 6159 | `	}` |
|  19023275 | 6160 | `	if( pMapEntry ){` |
|         - | 6161 | `		/* Address of the hashmap node [i.e: Array entry] — same row reuse */` |
|   7653955 | 6162 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   7653955 | 6163 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   7654635 | 6164 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|       684 | 6165 | `			if( apNode[n] == pMapEntry ){` |
|       ! 0 | 6166 | `				nFree = SXU32_HIGH;` |
|       ! 0 | 6167 | `				break;` |
|         - | 6168 | `			}` |
|       684 | 6169 | `			if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|         3 | 6170 | `				nFree = n;` |
|         1 | 6171 | `			}` |
|       344 | 6172 | `		}` |
|   7653955 | 6173 | `		if( n >= SySetUsed(&pRef->aArrEntries) ){` |
|   7653955 | 6174 | `			if( nFree != SXU32_HIGH ){` |
|         3 | 6175 | `				apNode[nFree] = pMapEntry;` |
|         2 | 6176 | `			}else{` |
|   7653953 | 6177 | `				SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|         - | 6178 | `			}` |
|   3826975 | 6179 | `		}` |
|   3826975 | 6180 | `	}` |
|  19023275 | 6181 | `	return SXRET_OK;` |
|   9512912 | 6182 | `}` |
|         - | 6183 | `/*` |
|         - | 6184 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 6185 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6186 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6187 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6188 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6189 | ` * Refer to the official for more information on this powerful` |
|         - | 6190 | ` * extension.` |
|         - | 6191 | ` */` |
|   8601726 | 6192 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 6193 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6194 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6195 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6196 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 6197 | `	)` |
|         5 | 6198 | `{` |
|         - | 6199 | `	VmRefObj *pRef;` |
|         - | 6200 | `	sxu32 n;` |
|         - | 6201 | `	/* Check if the referenced object already exists */` |
|   8601731 | 6202 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   8601731 | 6203 | `	if( pRef == 0 ){` |
|         - | 6204 | `		/* Not such entry */` |
|         5 | 6205 | `		return SXERR_NOTFOUND;` |
|         - | 6206 | `	}` |
|         - | 6207 | `	/* Remove the desired entry */` |
|   8601727 | 6208 | `	if( pEntry ){` |
|         - | 6209 | `		SyHashEntry **apEntry;` |
|   1077769 | 6210 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   2155707 | 6211 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1077943 | 6212 | `			if( apEntry[n] == pEntry ){` |
|         - | 6213 | `				/* Nullify the entry */` |
|   1077767 | 6214 | `				apEntry[n] = 0;` |
|         - | 6215 | `				/*` |
|         - | 6216 | `				 * NOTE:` |
|         - | 6217 | `				 * In future releases,think to add a free pool of entries,so that` |
|         - | 6218 | `				 * we avoid wasting spaces.` |
|         - | 6219 | `				 */` |
|    540153 | 6220 | `			}` |
|    540246 | 6221 | `		}` |
|    540154 | 6222 | `	}` |
|   8601727 | 6223 | `	if( pMapEntry ){` |
|         - | 6224 | `		ph7_hashmap_node **apNode;` |
|   7523963 | 6225 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  15048801 | 6226 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   7524843 | 6227 | `			if( apNode[n] == pMapEntry ){` |
|         - | 6228 | `				/* nullify the entry */` |
|   7523963 | 6229 | `				apNode[n] = 0;` |
|   3761979 | 6230 | `			}` |
|   3762424 | 6231 | `		}` |
|   3761979 | 6232 | `	}` |
|   8601727 | 6233 | `	return SXRET_OK;` |
|   4302140 | 6234 | `}` |
|         - | 6235 | `/*` |
|         - | 6236 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|         - | 6237 | ` * names bound to it plus the array nodes pointing at it. php refcounts a` |
|         - | 6238 | ` * reference set and keeps the VALUE alive while any holder remains, so this is` |
|         - | 6239 | ` * the count every "may I release this slot?" decision asks for.` |
|         - | 6240 | ` *` |
|         - | 6241 | ` * A row is only a holder while it is non-NULL (every holder's death nullifies` |
|         - | 6242 | ` * its own row through PH7_VmRefObjRemove) and, for a node, while it still points` |
|         - | 6243 | ` * HERE — a slot index travels through the free list, so a record can outlive the` |
|         - | 6244 | ` * node that filed the row (the same filter VmUnsetVarByName applies).` |
|         - | 6245 | ` */` |
|   9441324 | 6246 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6247 | `{` |
|         - | 6248 | `	ph7_hashmap_node **apNode;` |
|         - | 6249 | `	SyHashEntry **apEntry;` |
|         - | 6250 | `	VmRefObj *pRef;` |
|   9441329 | 6251 | `	sxu32 n, nLive = 0;` |
|   9441329 | 6252 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6253 | `		return 0;` |
|         - | 6254 | `	}` |
|   9441329 | 6255 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   9441329 | 6256 | `	if( pRef == 0 ){` |
|         3 | 6257 | `		return 0;` |
|         - | 6258 | `	}` |
|   9441327 | 6259 | `	if( pRef->nPin > 0 ){` |
|         - | 6260 | `		/* Holders the table cannot name, counted: reference-bound properties. */` |
|        13 | 6261 | `		nLive += pRef->nPin;` |
|   9441321 | 6262 | `	}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|         - | 6263 | ``		/* A permanent pin — a `use (&$x)` capture, a static, an enum case. It is the`` |
|         - | 6264 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|         - | 6265 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|         - | 6266 | `		 * php's refcount does. */` |
|         5 | 6267 | `		nLive++;` |
|         2 | 6268 | `	}` |
|   9441327 | 6269 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|  10528989 | 6270 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|   1087667 | 6271 | `		if( apEntry[n] ){` |
|       249 | 6272 | `			nLive++;` |
|       122 | 6273 | `		}` |
|    545108 | 6274 | `	}` |
|   9441327 | 6275 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  17809509 | 6276 | `	for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   8368187 | 6277 | `		if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|    837311 | 6278 | `			nLive++;` |
|    418653 | 6279 | `		}` |
|   4184096 | 6280 | `	}` |
|   9441327 | 6281 | `	return nLive;` |
|   4721939 | 6282 | `}` |
|         - | 6283 | `/*` |
|         - | 6284 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|         - | 6285 | ` * holds it (php frees the value with the last reference, not with the first one` |
|         - | 6286 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|         - | 6287 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|         - | 6288 | ` */` |
|   7533522 | 6289 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6290 | `{` |
|         - | 6291 | `	VmRefObj *pRef;` |
|   7533527 | 6292 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6293 | `		return;` |
|         - | 6294 | `	}` |
|   7533527 | 6295 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   7533527 | 6296 | `	if( pRef ){` |
|   7533523 | 6297 | `		if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|        57 | 6298 | `			return; /* pinned past its frame — its holder is not in the table */` |
|         - | 6299 | `		}` |
|   7533467 | 6300 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|      3514 | 6301 | `			return; /* somebody still holds it */` |
|         - | 6302 | `		}` |
|   3764976 | 6303 | `	}` |
|         - | 6304 | `	/* No record at all means nothing was ever registered against the slot, which is` |
|         - | 6305 | `	 * the same answer as a count of zero — release it (this is what every caller did` |
|         - | 6306 | `	 * unconditionally before the holder rule). */` |
|   7529961 | 6307 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|         - | 6308 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|         - | 6309 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
|   7529961 | 6310 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|   3766766 | 6311 | `}` |
|         - | 6312 | `/*` |
|         - | 6313 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|         - | 6314 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|         - | 6315 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|         - | 6316 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|         - | 6317 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|         - | 6318 | ` */` |
|      9626 | 6319 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|         5 | 6320 | `{` |
|         - | 6321 | `	VmFrame *pFrame;` |
|     22353 | 6322 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     12727 | 6323 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     12727 | 6324 | `		sxu32 n = 0;` |
|     25047 | 6325 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|     12322 | 6326 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|         - | 6327 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|         - | 6328 | `				 * same index — it now holds the row swapped in from the tail. */` |
|      3094 | 6329 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|      3094 | 6330 | `				(void)SySetPop(&pFrame->sRef);` |
|      3094 | 6331 | `				continue;` |
|         - | 6332 | `			}` |
|      9230 | 6333 | `			n++;` |
|         2 | 6334 | `		}` |
|      6366 | 6335 | `	}` |
|      9631 | 6336 | `}` |
|         - | 6337 | `/*` |
|         - | 6338 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|         - | 6339 | ` *` |
|         - | 6340 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|         - | 6341 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|         - | 6342 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|         - | 6343 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|         - | 6344 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|         - | 6345 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|         - | 6346 | ` */` |
|     16902 | 6347 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|         5 | 6348 | `{` |
|     16907 | 6349 | `	return nByte > 0 && zName[0] == '[';` |
|         5 | 6350 | `}` |
|         - | 6351 | `/*` |
|         - | 6352 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|         - | 6353 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|         - | 6354 | ` * its value variable on every step, and it goes through the reference table like any` |
|         - | 6355 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|         - | 6356 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|         - | 6357 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|         - | 6358 | ` */` |
|       310 | 6359 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 6360 | `	sxu32 nIdx)` |
|         5 | 6361 | `{` |
|       315 | 6362 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       315 | 6363 | `	if( pEntry ){` |
|        95 | 6364 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|        95 | 6365 | `		return;` |
|         - | 6366 | `	}` |
|       223 | 6367 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|       ! 0 | 6368 | `		return;` |
|         - | 6369 | `	}` |
|       223 | 6370 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|         - | 6371 | `		/* A global is also an entry of the $GLOBALS view */` |
|        41 | 6372 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        19 | 6373 | `	}` |
|       223 | 6374 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       160 | 6375 | `}` |
|         - | 6376 | `/*` |
|         - | 6377 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|         - | 6378 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|         - | 6379 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|         - | 6380 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|         - | 6381 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|         - | 6382 | ` * and left the OLD binding standing, so every later write through the name went to` |
|         - | 6383 | ` * the wrong variable.` |
|         - | 6384 | ` *` |
|         - | 6385 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|         - | 6386 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|         - | 6387 | ` * compares it by identity.` |
|         - | 6388 | ` */` |
|      3176 | 6389 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|         - | 6390 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 6391 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|         - | 6392 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|         - | 6393 | `	const char *zName,    /* Variable name */` |
|         - | 6394 | `	sxu32 nByte,          /* Name length */` |
|         - | 6395 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|         - | 6396 | `	)` |
|         3 | 6397 | `{` |
|      3179 | 6398 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      3179 | 6399 | `	if( nOld == nIdx ){` |
|         - | 6400 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|       ! 0 | 6401 | `		return;` |
|         - | 6402 | `	}` |
|         - | 6403 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|         - | 6404 | `	 * "release this reference at exit" set */` |
|      3179 | 6405 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|      3179 | 6406 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|      3179 | 6407 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|      3179 | 6408 | `	if( pFrame->pParent == 0 ){` |
|         - | 6409 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|         - | 6410 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|         - | 6411 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|         - | 6412 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|       105 | 6413 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|       102 | 6414 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|       105 | 6415 | `		 && pGlobalNode ){` |
|       105 | 6416 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|       105 | 6417 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|       105 | 6418 | `				pGlobalNode->nValIdx = nIdx;` |
|       105 | 6419 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|        51 | 6420 | `			}` |
|        54 | 6421 | `		}else{` |
|         - | 6422 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|       ! 0 | 6423 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|         - | 6424 | `		}` |
|        51 | 6425 | `	}` |
|      3179 | 6426 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|         - | 6427 | `	/* The old value dies with its last holder — and only then */` |
|      3179 | 6428 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|      1591 | 6429 | `}` |
|         - | 6430 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 6431 | `/*` |
|         - | 6432 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 6433 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 6434 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 6435 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 6436 | ` * For more information on how to register IO stream devices,please` |
|         - | 6437 | ` * refer to the official documentation.` |
|         - | 6438 | ` */` |
|     34322 | 6439 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 6440 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 6441 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 6442 | `	int nByte              /* *pzDevice length*/` |
|         - | 6443 | `	)` |
|         5 | 6444 | `{` |
|         - | 6445 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|         - | 6446 | `	ph7_io_stream **apStream,*pStream;` |
|         - | 6447 | `	SyString sDev,sCur;` |
|         - | 6448 | `	sxu32 n,nEntry;` |
|         - | 6449 | `	int rc;` |
|         - | 6450 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|     34327 | 6451 | `	zNext = zCur = zIn = *pzDevice;` |
|     34327 | 6452 | `	zEnd = &zIn[nByte];` |
|   2179598 | 6453 | `	while( zIn < zEnd ){` |
|   2145418 | 6454 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|         - | 6455 | `			/* Got one */` |
|       147 | 6456 | `			zNext = &zIn[sizeof("://")-1];` |
|       147 | 6457 | `			break;` |
|         - | 6458 | `		}` |
|         - | 6459 | `		/* Advance the cursor */` |
|   2145276 | 6460 | `		zIn++;` |
|         5 | 6461 | `	}` |
|     34327 | 6462 | `	if( zIn >= zEnd ){` |
|         - | 6463 | `		/* No such scheme,return the default stream */` |
|     34185 | 6464 | `		return pVm->pDefStream;` |
|         - | 6465 | `	}` |
|       147 | 6466 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|         - | 6467 | `	/* Remove leading and trailing white spaces */` |
|       147 | 6468 | `	SyStringFullTrim(&sDev);` |
|         - | 6469 | `	/* Perform a linear lookup on the installed stream devices */` |
|       147 | 6470 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       147 | 6471 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|       181 | 6472 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       181 | 6473 | `		pStream = apStream[n];` |
|       181 | 6474 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|         - | 6475 | `		/* Perfrom a case-insensitive comparison */` |
|       181 | 6476 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|       181 | 6477 | `		if( rc == 0 ){` |
|         - | 6478 | `			/* Stream device found */` |
|       147 | 6479 | `			*pzDevice = zNext;` |
|       147 | 6480 | `			return pStream;` |
|         - | 6481 | `		}` |
|        19 | 6482 | `	}` |
|         - | 6483 | `	/* No such stream,return NULL */` |
|       ! 0 | 6484 | `	return 0;` |
|     17166 | 6485 | `}` |
|         - | 6486 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 6487 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 6488 |  |
