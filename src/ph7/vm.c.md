# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2332/2794 lines (83.46%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include <stddef.h>` |
|        - |    8 | `#include <stdlib.h>` |
|        - |    9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |   10 | `#include <math.h>` |
|        - |   11 | `#endif` |
|        - |   12 | `/* Signed 64-bit integer overflow detection lives in ph7int.h as the shared` |
|        - |   13 | ` * PH7_{ADD,SUB,MUL}_OVERFLOW64 macros (GCC/Clang intrinsics, MSVC fallbacks in` |
|        - |   14 | ` * memobj.c). The executor uses them to promote an overflowing integer` |
|        - |   15 | ` * operation to a float, matching PHP. */` |
|        - |   16 | `/*` |
|        - |   17 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |   18 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |   19 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |   20 | ` * statements.` |
|        - |   21 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |   22 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |   23 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |   24 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |   25 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |   26 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |   27 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |   28 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |   29 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |   30 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |   31 | ` * and so on.` |
|        - |   32 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |   33 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |   34 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |   35 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |   36 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |   37 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |   38 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |   39 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |   40 | ` */` |
|        - |   41 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |   42 | `/*` |
|        - |   43 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |   44 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |   45 | ` * in the free object table so that it can be reused again without allocating` |
|        - |   46 | ` * a new memory object.` |
|        - |   47 | ` */` |
|        - |   48 | `/* VmSlot struct moved to ph7int.h */` |
|        - |   49 | `/*` |
|        - |   50 | ` * An entry in the reference table is represented by an instance of the` |
|        - |   51 | ` * follwoing table.` |
|        - |   52 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |   53 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |   54 | ` * the reference implementation is consistent,solid and it's` |
|        - |   55 | ` * behavior resemble the C++ reference mechanism.` |
|        - |   56 | ` * Refer to the official for more information on this powerful` |
|        - |   57 | ` * extension.` |
|        - |   58 | ` */` |
|        - |   59 | `/* struct VmRefObj + VM_REF_IDX_KEEP moved to ph7int.h */` |
|        - |   60 | `/*` |
|        - |   61 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |   62 | ` * is stored in an instance of the following structure.` |
|        - |   63 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   64 | ` */` |
|        - |   65 | `/* VmShutdownCB struct moved to ph7int.h */` |
|        - |   66 | `/*` |
|        - |   67 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |   68 | ` * is stored in an instance of the following structure.` |
|        - |   69 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |   70 | ` */` |
|        - |   71 | `/* VmAutoloadCB struct moved to ph7int.h */` |
|        - |   72 |  |
|        - |   73 | `/*` |
|        - |   74 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   75 | ` */` |
|  1278095 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   77 | `{` |
|  1278100 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       40 |   79 | `		return TRUE;` |
|        - |   80 | `	}` |
|  1278062 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   82 | `		return TRUE;` |
|        - |   83 | `	}` |
|  1278052 |   84 | `	return FALSE;` |
|   639560 |   85 | `}` |
|        - |   86 | `/*` |
|        - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   93 | ` * still go through the existing numeric coercion.` |
|        - |   94 | ` */` |
|   366353 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   96 | `{` |
|        - |   97 | `	SyString sStr;` |
|   366358 |   98 | `	sxu8 bReal = FALSE;` |
|   366358 |   99 | `	const char *zTail = 0;` |
|        - |  100 | `	const char *zEnd;` |
|   366358 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   366344 |  102 | `		return FALSE;` |
|        - |  103 | `	}` |
|       17 |  104 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       17 |  105 | `	if( sStr.nByte == 0 ){` |
|      ! 0 |  106 | `		return TRUE;` |
|        - |  107 | `	}` |
|       17 |  108 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|        5 |  109 | `		return TRUE;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |  112 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |  113 | `	 * still takes the Perl path. */` |
|       13 |  114 | `	zEnd = sStr.zString + sStr.nByte;` |
|       13 |  115 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|      ! 0 |  116 | `		zTail++;` |
|      ! 0 |  117 | `	}` |
|       13 |  118 | `	return zTail < zEnd;` |
|   183283 |  119 | `}` |
|        - |  120 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |  121 | `/* Constant expander used by define(); used below to recognise user-defined` |
|        - |  122 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|        - |  123 | ` * a define() overwrites them. */` |
|        - |  124 | `/*` |
|        - |  125 | ` * Register a constant and it's associated expansion callback so that` |
|        - |  126 | ` * it can be expanded from the target PHP program.` |
|        - |  127 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |  128 | ` * simple and work as follows:` |
|        - |  129 | ` * Each registered constant have a C procedure associated with it.` |
|        - |  130 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |  131 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |  132 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |  133 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |  134 | ` * (Windows,Linux,...) and so on.` |
|        - |  135 | ` * Please refer to the official documentation for additional information.` |
|        - |  136 | ` */` |
|  1522640 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  139 | `	const SyString *pName,  /* Constant name */` |
|        - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |  142 | `	)` |
|        5 |  143 | `{` |
|  1522645 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|        5 |  145 | `}` |
|        - |  146 | `/*` |
|        - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|        - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|        - |  149 | ` */` |
|  1522692 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
|        - |  151 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  152 | `	const SyString *pName,  /* Constant name */` |
|        - |  153 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |  154 | `	void *pUserData,        /* Last argument to xExpand() */` |
|        - |  155 | `	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */` |
|        - |  156 | `	sxu32 nLine,            /* Declaration line, 0 = unknown */` |
|        - |  157 | `	int bUser               /* 1 when defined by user code */` |
|        - |  158 | `	)` |
|        5 |  159 | `{` |
|        - |  160 | `	ph7_constant *pCons;` |
|        - |  161 | `	SyHashEntry *pEntry;` |
|        - |  162 | `	char *zDupName;` |
|        - |  163 | `	sxi32 rc;` |
|  1522697 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|  1522697 |  165 | `	if( pEntry ){` |
|        - |  166 | `		/* Overwrite the old definition and return immediately */` |
|        3 |  167 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  168 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|        - |  169 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|        - |  170 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|        2 |  171 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|        3 |  172 | `		 && pCons->pUserData != pUserData ){` |
|        3 |  173 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|        3 |  174 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|        1 |  175 | `		}` |
|        3 |  176 | `		pCons->xExpand = xExpand;` |
|        3 |  177 | `		pCons->pUserData = pUserData;` |
|        3 |  178 | `		if( pFile ){` |
|        3 |  179 | `			SyStringDupPtr(&pCons->sFile,pFile);` |
|        2 |  180 | `		}else{` |
|      ! 0 |  181 | `			SyStringInitFromBuf(&pCons->sFile,0,0);` |
|        - |  182 | `		}` |
|        3 |  183 | `		pCons->nLine = nLine;` |
|        3 |  184 | `		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|        3 |  185 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|        3 |  186 | `		return SXRET_OK;` |
|        - |  187 | `	}` |
|        - |  188 | `	/* Allocate a new constant instance */` |
|  1522695 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|  1522695 |  190 | `	if( pCons == 0 ){` |
|      ! 0 |  191 | `		return 0;` |
|        - |  192 | `	}` |
|        - |  193 | `	/* Duplicate constant name */` |
|  1522695 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1522695 |  195 | `	if( zDupName == 0 ){` |
|      ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  197 | `		return 0;` |
|        - |  198 | `	}` |
|  1522695 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|  1522695 |  200 | `	if( pFile ){` |
|       53 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       25 |  202 | `	}` |
|  1522695 |  203 | `	pCons->nLine = nLine;` |
|  1522695 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|        - |  205 | `	/* Install the constant */` |
|  1522695 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|  1522695 |  207 | `	pCons->xExpand = xExpand;` |
|  1522695 |  208 | `	pCons->pUserData = pUserData;` |
|  1522695 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1522695 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|  1522695 |  211 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  214 | `		return rc;` |
|        - |  215 | `	}` |
|        - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|  1522695 |  217 | `	return SXRET_OK;` |
|   761351 |  218 | `}` |
|        - |  219 | `/*` |
|        - |  220 | ` * Allocate a new foreign function instance.` |
|        - |  221 | ` * This function return SXRET_OK on success. Any other` |
|        - |  222 | ` * return value indicates failure.` |
|        - |  223 | ` * Please refer to the official documentation for an introduction to` |
|        - |  224 | ` * the foreign function mechanism.` |
|        - |  225 | ` */` |
|  2229006 |  226 | `static sxi32 PH7_NewForeignFunction(` |
|        - |  227 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  228 | `	const SyString *pName,    /* Foreign function name */` |
|        - |  229 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |  230 | `	void *pUserData,          /* Foreign function private data */` |
|        - |  231 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |  232 | `	)` |
|        5 |  233 | `{` |
|        - |  234 | `	ph7_user_func *pFunc;` |
|        - |  235 | `	char *zDup;` |
|        - |  236 | `	/* Allocate a new user function */` |
|  2229011 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  2229011 |  238 | `	if( pFunc == 0 ){` |
|      ! 0 |  239 | `		return SXERR_MEM;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate function name */` |
|  2229011 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2229011 |  243 | `	if( zDup == 0 ){` |
|      ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  245 | `		return SXERR_MEM;` |
|        - |  246 | `	}` |
|        - |  247 | `	/* Zero the structure */` |
|  2229011 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |  249 | `	/* Initialize structure fields */` |
|  2229011 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  2229011 |  251 | `	pFunc->pVm   = pVm;` |
|  2229011 |  252 | `	pFunc->xFunc = xFunc;` |
|  2229011 |  253 | `	pFunc->pUserData = pUserData;` |
|  2229011 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  255 | `	/* Write a pointer to the new function */` |
|  2229011 |  256 | `	*ppOut = pFunc;` |
|  2229011 |  257 | `	return SXRET_OK;` |
|  1114508 |  258 | `}` |
|        - |  259 | `/*` |
|        - |  260 | ` * Install a foreign function and it's associated callback so that` |
|        - |  261 | ` * it can be invoked from the target PHP code.` |
|        - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |  263 | ` * return value indicates failure.` |
|        - |  264 | ` * Please refer to the official documentation for an introduction to` |
|        - |  265 | ` * the foreign function mechanism.` |
|        - |  266 | ` */` |
|  2232420 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |  268 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  269 | `	const SyString *pName,    /* Foreign function name */` |
|        - |  270 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |  271 | `	void *pUserData           /* Foreign function private data */` |
|        - |  272 | `	)` |
|        5 |  273 | `{` |
|        - |  274 | `	ph7_user_func *pFunc;` |
|        - |  275 | `	SyHashEntry *pEntry;` |
|        - |  276 | `	sxi32 rc;` |
|        - |  277 | `	/* Overwrite any previously registered function with the same name */` |
|  2232425 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  2232425 |  279 | `	if( pEntry ){` |
|     3419 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     3419 |  281 | `		pFunc->pUserData = pUserData;` |
|     3419 |  282 | `		pFunc->xFunc = xFunc;` |
|     3419 |  283 | `		SySetReset(&pFunc->aAux);` |
|        - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|        - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|        - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|        - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|     3419 |  288 | `		pFunc->nMinArg  = 0;` |
|     3419 |  289 | `		pFunc->bAtLeast = 0;` |
|     3419 |  290 | `		return SXRET_OK;` |
|        - |  291 | `	}` |
|        - |  292 | `	/* Create a new user function */` |
|  2229011 |  293 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  2229011 |  294 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  295 | `		return rc;` |
|        - |  296 | `	}` |
|        - |  297 | `	/* Install the function in the corresponding hashtable */` |
|  2229011 |  298 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  2229011 |  299 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  300 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |  301 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  302 | `		return rc;` |
|        - |  303 | `	}` |
|        - |  304 | `	/* User function successfully installed */` |
|  2229011 |  305 | `	return SXRET_OK;` |
|  1116215 |  306 | `}` |
|        - |  307 | `/*` |
|        - |  308 | ` * Initialize a VM function.` |
|        - |  309 | ` */` |
|  3420694 |  310 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |  311 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  312 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |  313 | `	const char *zName,  /* Function name */` |
|        - |  314 | `	sxu32 nByte,        /* zName length */` |
|        - |  315 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |  316 | `	void *pUserData     /* Function private data */` |
|        - |  317 | `	)` |
|        5 |  318 | `{` |
|        - |  319 | `	/* Zero the structure */` |
|  3420699 |  320 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |  321 | `	/* Initialize structure fields */` |
|        - |  322 | `	/* Arguments container */` |
|  3420699 |  323 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |  324 | `	/* Static variable container */` |
|  3420699 |  325 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |  326 | `	/* Bytecode container */` |
|  3420699 |  327 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |  328 | `    /* Preallocate some instruction slots */` |
|  3420699 |  329 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |  330 | `	/* Closure environment */` |
|  3420699 |  331 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |  332 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|  3420699 |  333 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  334 | `	/* Declared #[...] attributes */` |
|  3420699 |  335 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  3420699 |  336 | `	pFunc->iFlags = iFlags;` |
|  3420699 |  337 | `	pFunc->pUserData = pUserData;` |
|        - |  338 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |  339 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|  3420699 |  340 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|  3420699 |  341 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  342 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|  3416165 |  343 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|  1708085 |  344 | `	}else{` |
|        - |  345 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|        - |  346 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|        - |  347 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     4539 |  348 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     4539 |  349 | `		if( pFile ){` |
|     4539 |  350 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|     2267 |  351 | `		}` |
|        - |  352 | `	}` |
|  3420699 |  353 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|  3420699 |  354 | `	return SXRET_OK;` |
|        5 |  355 | `}` |
|        - |  356 | `/*` |
|        - |  357 | ` * Namespace-aware function lookup.` |
|        - |  358 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |  359 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |  360 | ` */` |
|        - |  361 | `/*` |
|        - |  362 | ` * Install a user defined function in the corresponding VM container.` |
|        - |  363 | ` */` |
|  5810676 |  364 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |  365 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  366 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |  367 | `	SyString *pName     /* Function name */` |
|        - |  368 | `	)` |
|        5 |  369 | `{` |
|        - |  370 | `	SyHashEntry *pEntry;` |
|        - |  371 | `	sxi32 rc;` |
|  5810681 |  372 | `	if( pName == 0 ){` |
|        - |  373 | `		/* Use the built-in name */` |
|   522721 |  374 | `		pName = &pFunc->sName;` |
|   261358 |  375 | `	}` |
|        - |  376 | `	/* Check for duplicates (functions with the same name) first */` |
|  5810681 |  377 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  5810681 |  378 | `	if( pEntry ){` |
|  2863603 |  379 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  2863603 |  380 | `		if( pLink != pFunc ){` |
|        - |  381 | `			/* Link */` |
|       35 |  382 | `			pFunc->pNextName = pLink;` |
|       35 |  383 | `			pEntry->pUserData = pFunc;` |
|       16 |  384 | `		}` |
|  2863603 |  385 | `		return SXRET_OK;` |
|        - |  386 | `	}` |
|        - |  387 | `	/* First time seen */` |
|  2947083 |  388 | `	pFunc->pNextName = 0;` |
|  2947083 |  389 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|  2947083 |  390 | `	return rc;` |
|  2905343 |  391 | `}` |
|        - |  392 | `/*` |
|        - |  393 | ` * Install a user defined class in the corresponding VM container.` |
|        - |  394 | ` */` |
|   522308 |  395 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |  396 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |  397 | `	ph7_class *pClass /* Target Class */` |
|        - |  398 | `	)` |
|        5 |  399 | `{` |
|   522313 |  400 | `	SyString *pName = &pClass->sName;` |
|        - |  401 | `	SyHashEntry *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `	/* Check for duplicates */` |
|   522313 |  404 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   522313 |  405 | `	if( pEntry ){` |
|        3 |  406 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |  407 | `		/* Link entry with the same name */` |
|        3 |  408 | `		pClass->pNextName = pLink;` |
|        3 |  409 | `		pEntry->pUserData = pClass;` |
|        3 |  410 | `		return SXRET_OK;` |
|        - |  411 | `	}` |
|   522311 |  412 | `	pClass->pNextName = 0;` |
|        - |  413 | `	/* Perform a simple hashtable insertion */` |
|   522311 |  414 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   522311 |  415 | `	return rc;` |
|   261159 |  416 | `}` |
|        - |  417 | `/*` |
|        - |  418 | ` * Instruction builder interface.` |
|        - |  419 | ` */` |
| 98449296 |  420 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |  421 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |  422 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |  423 | `	sxi32 iP1,    /* First operand */` |
|        - |  424 | `	sxu32 iP2,    /* Second operand */` |
|        - |  425 | `	void *p3,     /* Third operand */` |
|        - |  426 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |  427 | `	)` |
|        5 |  428 | `{` |
|        - |  429 | `	VmInstr sInstr;` |
| 98449301 |  430 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - |  431 | `	sxi32 rc;` |
|        - |  432 | `	/* Fill the VM instruction */` |
| 98449301 |  433 | `	sInstr.iOp = (sxu8)iOp;` |
| 98449301 |  434 | `	sInstr.iP1 = iP1;` |
| 98449301 |  435 | `	sInstr.iP2 = iP2;` |
| 98449301 |  436 | `	sInstr.p3  = p3;` |
|        - |  437 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|        - |  438 | `	 * compiled (that is how they read its text), so the current token IS this` |
|        - |  439 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|        - |  440 | `	 * between statements, hence the range check. */` |
| 98449301 |  441 | `	sInstr.nLine = 0;` |
| 98449301 |  442 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
| 41944415 |  443 | `		sInstr.nLine = pGen->pIn->nLine;` |
| 77477096 |  444 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|        - |  445 | `		/* Past the end (statement tail): blame the last real token. */` |
| 56290055 |  446 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
| 28145025 |  447 | `	}` |
| 98449301 |  448 | `	if( pIndex ){` |
|        - |  449 | `		/* Instruction index in the bytecode array */` |
|  6664517 |  450 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|  3332256 |  451 | `	}` |
|        - |  452 | `	/* Finally,record the instruction */` |
| 98449301 |  453 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 98449301 |  454 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  455 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |  456 | `		/* Fall throw */` |
|      ! 0 |  457 | `	}` |
| 98449301 |  458 | `	return rc;` |
|        5 |  459 | `}` |
|        - |  460 | `/*` |
|        - |  461 | ` * Swap the current bytecode container with the given one.` |
|        - |  462 | ` */` |
|  9790524 |  463 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |  464 | `{` |
|  9790529 |  465 | `	if( pContainer == 0 ){` |
|        - |  466 | `		/* Point to the default container */` |
|      ! 0 |  467 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |  468 | `	}else{` |
|        - |  469 | `		/* Change container */` |
|  9790529 |  470 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |  471 | `	}` |
|  9790529 |  472 | `	return SXRET_OK;` |
|        5 |  473 | `}` |
|        - |  474 | `/*` |
|        - |  475 | ` * Return the current bytecode container.` |
|        - |  476 | ` */` |
|  4895262 |  477 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |  478 | `{` |
|  4895267 |  479 | `	return pVm->pByteContainer;` |
|        5 |  480 | `}` |
|        - |  481 | `/*` |
|        - |  482 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |  483 | ` */` |
|  6602318 |  484 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |  485 | `{` |
|        - |  486 | `	VmInstr *pInstr;` |
|  6602323 |  487 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|  6602323 |  488 | `	return pInstr;` |
|        5 |  489 | `}` |
|        - |  490 | `/*` |
|        - |  491 | ` * Return the total number of VM instructions recorded so far.` |
|        - |  492 | ` */` |
| 24541180 |  493 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |  494 | `{` |
| 24541185 |  495 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |  496 | `}` |
|        - |  497 | `/*` |
|        - |  498 | ` * Pop the last VM instruction.` |
|        - |  499 | ` */` |
|  5403188 |  500 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |  501 | `{` |
|  5403193 |  502 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |  503 | `}` |
|        - |  504 | `/*` |
|        - |  505 | ` * Peek the last VM instruction.` |
|        - |  506 | ` */` |
| 20467432 |  507 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |  508 | `{` |
| 20467437 |  509 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |  510 | `}` |
|  1713616 |  511 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |  512 | `{` |
|        - |  513 | `	VmInstr *aInstr;` |
|        - |  514 | `	sxu32 n;` |
|  1713621 |  515 | `	n = SySetUsed(pVm->pByteContainer);` |
|  1713621 |  516 | `	if( n < 2 ){` |
|      ! 0 |  517 | `		return 0;` |
|        - |  518 | `	}` |
|  1713621 |  519 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|  1713621 |  520 | `	return &aInstr[n - 2];` |
|   856813 |  521 | `}` |
|        - |  522 | `/*` |
|        - |  523 | ` * Allocate a new virtual machine frame.` |
|        - |  524 | ` */` |
|   113057 |  525 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|        - |  526 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  527 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |  528 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  529 | `	)` |
|        5 |  530 | `{` |
|        - |  531 | `	VmFrame *pFrame;` |
|        - |  532 | `	/* Allocate a new vm frame */` |
|   113062 |  533 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   113062 |  534 | `	if( pFrame == 0 ){` |
|      ! 0 |  535 | `		return 0;` |
|        - |  536 | `	}` |
|        - |  537 | `	/* Zero the structure */` |
|   113062 |  538 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |  539 | `	/* Initialize frame fields */` |
|   113062 |  540 | `	pFrame->pUserData = pUserData;` |
|   113062 |  541 | `	pFrame->pThis = pThis;` |
|   113062 |  542 | `	pFrame->pVm = pVm;` |
|   113062 |  543 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   113062 |  544 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   113062 |  545 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   113062 |  546 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   113062 |  547 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|        - |  548 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|        - |  549 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   113062 |  550 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   113062 |  551 | `	return pFrame;` |
|    56635 |  552 | `}` |
|        - |  553 | `/* Forward declaration */` |
|        - |  554 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|        - |  555 | `/*` |
|        - |  556 | ` * Enter a VM frame.` |
|        - |  557 | ` */` |
|   112461 |  558 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|        - |  559 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  560 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |  561 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  562 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |  563 | `	)` |
|        5 |  564 | `{` |
|        - |  565 | `	VmFrame *pFrame;` |
|        - |  566 | `	/* Allocate a new frame */` |
|   112466 |  567 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   112466 |  568 | `	if( pFrame == 0 ){` |
|      ! 0 |  569 | `		return SXERR_MEM;` |
|        - |  570 | `	}` |
|        - |  571 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   112466 |  572 | `	pFrame->nCallLine = pVm->nCurLine;` |
|        - |  573 | `	/* Link to the list of active VM frame */` |
|   112466 |  574 | `	pFrame->pParent = pVm->pFrame;` |
|   112466 |  575 | `	pVm->pFrame = pFrame;` |
|   112466 |  576 | `	if( ppFrame ){` |
|        - |  577 | `		/* Write a pointer to the new VM frame */` |
|   108576 |  578 | `		*ppFrame = pFrame;` |
|    54387 |  579 | `	}` |
|   112466 |  580 | `	return SXRET_OK;` |
|    56337 |  581 | `}` |
|        - |  582 | `/*` |
|        - |  583 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |  584 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |  585 | ` * information.` |
|        - |  586 | ` */` |
|       64 |  587 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        5 |  588 | `{` |
|        - |  589 | `	VmFrame *pTarget,*pFrame;` |
|       69 |  590 | `	SyHashEntry *pEntry = 0;` |
|        - |  591 | `	sxi32 rc;` |
|        - |  592 | `	/* Point to the upper frame */` |
|       69 |  593 | `	pFrame = pVm->pFrame;` |
|       69 |  594 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       69 |  595 | `	pTarget = pFrame;` |
|       69 |  596 | `	pFrame = pTarget->pParent;` |
|       69 |  597 | `	while( pFrame ){` |
|       69 |  598 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  599 | `			/* Query the current frame */` |
|       69 |  600 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       69 |  601 | `			if( pEntry ){` |
|        - |  602 | `				/* Variable found */` |
|       69 |  603 | `				break;` |
|        - |  604 | `			}` |
|      ! 0 |  605 | `		}` |
|        - |  606 | `		/* Point to the upper frame */` |
|      ! 0 |  607 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  608 | `	}` |
|       69 |  609 | `	if( pEntry == 0 ){` |
|        - |  610 | `		/* Inexistant variable */` |
|      ! 0 |  611 | `		return SXERR_NOTFOUND;` |
|        - |  612 | `	}` |
|        - |  613 | `	/* Link to the current frame */` |
|       69 |  614 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       69 |  615 | `	if( rc == SXRET_OK ){` |
|        - |  616 | `		sxu32 nIdx;` |
|       69 |  617 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       69 |  618 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       32 |  619 | `	}` |
|       69 |  620 | `	return rc;` |
|       37 |  621 | `}` |
|        - |  622 | `/*` |
|        - |  623 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|        - |  624 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|        - |  625 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|        - |  626 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|        - |  627 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|        - |  628 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|        - |  629 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|        - |  630 | ` */` |
|   109001 |  631 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 |  632 | `{` |
|   109006 |  633 | `	if( pVm->pResumeFrame == pFrame ){` |
|        3 |  634 | `		pVm->pResumeFrame = 0;` |
|        1 |  635 | `	}` |
|   109006 |  636 | `}` |
|        - |  637 | `/*` |
|        - |  638 | ` * Leave the top-most active frame.` |
|        - |  639 | ` */` |
|   108149 |  640 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |  641 | `{` |
|   108154 |  642 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   108154 |  643 | `	if( pCurFrame ){` |
|        - |  644 | `		/* Unlink from the list of active VM frame */` |
|   108154 |  645 | `		pVm->pFrame = pCurFrame->pParent;` |
|   108154 |  646 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  647 | `			VmSlot  *aSlot;` |
|        - |  648 | `			sxu32 n;` |
|        - |  649 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   103310 |  650 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   587692 |  651 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |  652 | `				/* Unset the local variable */` |
|   484387 |  653 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   242805 |  654 | `			}` |
|        - |  655 | `			/* Remove local reference */` |
|   103310 |  656 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   587776 |  657 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   484471 |  658 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   242847 |  659 | `			}` |
|    51754 |  660 | `		}` |
|        - |  661 | `		/* Release internal containers */` |
|   108154 |  662 | `		SyHashRelease(&pCurFrame->hVar);` |
|   108154 |  663 | `		SySetRelease(&pCurFrame->sArg);` |
|   108154 |  664 | `		SySetRelease(&pCurFrame->sLocal);` |
|   108154 |  665 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |  666 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|        - |  667 | `		 * containers above — released for every frame, including transparent` |
|        - |  668 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   108154 |  669 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|        - |  670 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   108154 |  671 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|        - |  672 | `		/* Release the whole structure */` |
|   108154 |  673 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    54176 |  674 | `	}` |
|   108154 |  675 | `}` |
|        - |  676 | `/*` |
|        - |  677 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|        - |  678 | ` * active frame's local-teardown set records it (walking the parent chain` |
|        - |  679 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|        - |  680 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|        - |  681 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|        - |  682 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|        - |  683 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|        - |  684 | ` */` |
|        - |  685 | `/*` |
|        - |  686 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|        - |  687 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|        - |  688 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|        - |  689 | ` *` |
|        - |  690 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|        - |  691 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|        - |  692 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|        - |  693 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|        - |  694 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|        - |  695 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|        - |  696 | ` */` |
|     6926 |  697 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  698 | `{` |
|        - |  699 | `	VmFrame *pFrame;` |
|    13887 |  700 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     6973 |  701 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  702 | `		sxu32 n;` |
|     7033 |  703 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|       76 |  704 | `			if( aSlot[n].nIdx == nIdx ){` |
|        - |  705 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       13 |  706 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       13 |  707 | `				(void)SySetPop(&pFrame->sLocal);` |
|       13 |  708 | `				return TRUE; /* Slot owned by exactly one frame */` |
|        - |  709 | `			}` |
|       34 |  710 | `		}` |
|     3483 |  711 | `	}` |
|     6919 |  712 | `	return FALSE;` |
|     3468 |  713 | `}` |
|       78 |  714 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        2 |  715 | `{` |
|        - |  716 | `	VmRefObj *pRef;` |
|       80 |  717 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       80 |  718 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       80 |  719 | `	if( pRef ){` |
|       80 |  720 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       39 |  721 | `	}` |
|       80 |  722 | `}` |
|        - |  723 | `/*` |
|        - |  724 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |  725 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |  726 | ` * should be skipped when looking for the real execution context.` |
|        - |  727 | ` */` |
| 10133523 |  728 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |  729 | `{` |
| 10158726 |  730 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|    25203 |  731 | `		pFrame = pFrame->pParent;` |
|        5 |  732 | `	}` |
| 10133528 |  733 | `	return pFrame;` |
|        5 |  734 | `}` |
|        - |  735 | `/*` |
|        - |  736 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|        - |  737 | ` * may be several frames below the frame that caught the exception — must resume at` |
|        - |  738 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|        - |  739 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|        - |  740 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|        - |  741 | ` *` |
|        - |  742 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|        - |  743 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|        - |  744 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|        - |  745 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|        - |  746 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|        - |  747 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|        - |  748 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|        - |  749 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|        - |  750 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|        - |  751 | ` * VmSkipExceptionFrames yields its real body frame.` |
|        - |  752 | ` *` |
|        - |  753 | ` * This replaces the older "is there a resumable try frame here" test` |
|        - |  754 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|        - |  755 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|        - |  756 | ` * catching frame was not the nearest try (ROOT B).` |
|        - |  757 | ` */` |
|     2944 |  758 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|        5 |  759 | `{` |
|     2949 |  760 | `	if( pVm->pResumeFrame == 0 ){` |
|       17 |  761 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|        - |  762 | `	}` |
|        - |  763 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|        - |  764 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|        - |  765 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|        - |  766 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|        - |  767 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|        - |  768 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|        - |  769 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|        - |  770 | `	 * exec's resume site match and land. */` |
|     2930 |  771 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|     2456 |  772 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|     1974 |  773 | `	 \|\| pVm->iResumePc == 0 ){` |
|        - |  774 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|        - |  775 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|        - |  776 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|        - |  777 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|        - |  778 | `		 * never match a real frame. */` |
|      979 |  779 | `		return FALSE;` |
|        - |  780 | `	}` |
|        - |  781 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|        - |  782 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|        - |  783 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|        - |  784 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|        - |  785 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|        - |  786 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|        - |  787 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|        - |  788 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|        - |  789 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|        - |  790 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|        - |  791 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|        - |  792 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|        - |  793 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|        - |  794 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|        - |  795 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|        - |  796 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|        - |  797 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|        - |  798 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|        - |  799 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|        - |  800 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|        - |  801 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|     3339 |  802 | `	while( pVm->pFrame != pEntryFrame` |
|     3810 |  803 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|     2421 |  804 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|      815 |  805 | `		VmLeaveFrame(&(*pVm));` |
|        5 |  806 | `	}` |
|     1961 |  807 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|     1961 |  808 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|        - |  809 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|        - |  810 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|        - |  811 | `	 * point router must not re-fire it after this resume. */` |
|     1961 |  812 | `	pVm->nBoundaryRc = 0;` |
|     1961 |  813 | `	return TRUE;` |
|     1477 |  814 | `}` |
|        - |  815 | `/*` |
|        - |  816 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|        - |  817 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|        - |  818 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|        - |  819 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|        - |  820 | ` * nested try/finally inside a catch body). Each finally runs with` |
|        - |  821 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|        - |  822 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|        - |  823 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|        - |  824 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|        - |  825 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|        - |  826 | ` */` |
|        - |  827 | `/*` |
|        - |  828 | ` * BYTECODE stage 2b — per-activation try state.` |
|        - |  829 | ` *` |
|        - |  830 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|        - |  831 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|        - |  832 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|        - |  833 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|        - |  834 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|        - |  835 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|        - |  836 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|        - |  837 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|        - |  838 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|        - |  839 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|        - |  840 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|        - |  841 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|        - |  842 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|        - |  843 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|        - |  844 | ` * activation.` |
|        - |  845 | ` */` |
|     2692 |  846 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  847 | `{` |
|     2697 |  848 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|     2697 |  849 | `	if( pClone == 0 ){` |
|      ! 0 |  850 | `		return 0;` |
|        - |  851 | `	}` |
|     2697 |  852 | `	*pClone = *pCompiled;` |
|     2697 |  853 | `	pClone->pCompiled = pCompiled;` |
|     2697 |  854 | `	pClone->iFinallyDone = 0;` |
|     2697 |  855 | `	pClone->iInCatch = 0;` |
|     2697 |  856 | `	pClone->pInflight = 0;` |
|     2697 |  857 | `	pClone->pFrame = 0;` |
|     2697 |  858 | `	return pClone;` |
|     1351 |  859 | `}` |
|     5304 |  860 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|        5 |  861 | `{` |
|     5309 |  862 | `	if( pExc && pExc->pCompiled ){` |
|        - |  863 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|        - |  864 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|        - |  865 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|        - |  866 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|     2685 |  867 | `		if( pExc->pInflight ){` |
|      ! 0 |  868 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|      ! 0 |  869 | `			pExc->pInflight = 0;` |
|      ! 0 |  870 | `		}` |
|     2685 |  871 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|     1340 |  872 | `	}` |
|     5309 |  873 | `}` |
|        - |  874 | `/*` |
|        - |  875 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|        - |  876 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|        - |  877 | ` */` |
|      466 |  878 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|        5 |  879 | `{` |
|      471 |  880 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|        5 |  881 | `}` |
|        - |  882 | `/*` |
|        - |  883 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|        - |  884 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|        - |  885 | ` * handlers). The set itself is reset by the caller.` |
|        - |  886 | ` */` |
|      546 |  887 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|        5 |  888 | `{` |
|      551 |  889 | `	sxu32 n = SySetUsed(pSet);` |
|      551 |  890 | `	if( n > 0 ){` |
|      ! 0 |  891 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|        - |  892 | `		sxu32 i;` |
|      ! 0 |  893 | `		for( i = 0; i < n; i++ ){` |
|      ! 0 |  894 | `			VmExcRelease(pVm,ap[i]);` |
|      ! 0 |  895 | `		}` |
|      ! 0 |  896 | `	}` |
|      551 |  897 | `}` |
|        - |  898 | `/*` |
|        - |  899 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|        - |  900 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|        - |  901 | ` * only carries the compiled pointer.` |
|        - |  902 | ` */` |
|       66 |  903 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  904 | `{` |
|       71 |  905 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       71 |  906 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|       71 |  907 | `	while( n > 0 ){` |
|       71 |  908 | `		n--;` |
|       71 |  909 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|       71 |  910 | `			return ap[n];` |
|        - |  911 | `		}` |
|      ! 0 |  912 | `	}` |
|      ! 0 |  913 | `	return 0;` |
|       38 |  914 | `}` |
|   255875 |  915 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |  916 | `{` |
|        - |  917 | `	sxu32 nUsed;` |
|   255880 |  918 | `	sxi32 rcOut = SXRET_OK;` |
|   255944 |  919 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|       69 |  920 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       69 |  921 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|       69 |  922 | `		(void)SySetPop(&pVm->aException);` |
|       69 |  923 | `		pExc->pFrame = 0;` |
|        - |  924 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|        - |  925 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|        - |  926 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|        - |  927 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|        - |  928 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|       69 |  929 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       69 |  930 | `			VmLeaveFrame(&(*pVm));` |
|       32 |  931 | `		}` |
|       93 |  932 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        - |  933 | `			sxi32 rcF;` |
|       52 |  934 | `			pExc->iFinallyDone = 1;` |
|       52 |  935 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|       52 |  936 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|       52 |  937 | `			if( rcF == SXERR_ABORT ){` |
|      ! 0 |  938 | `				return SXERR_ABORT;` |
|        - |  939 | `			}` |
|       52 |  940 | `			if( rcF == PH7_EXCEPTION ){` |
|        - |  941 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|        - |  942 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|        - |  943 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|        5 |  944 | `				rcOut = PH7_EXCEPTION;` |
|        2 |  945 | `			}` |
|       28 |  946 | `		}else{` |
|       19 |  947 | `			VmExcRelease(&(*pVm),pExc);` |
|        - |  948 | `		}` |
|        5 |  949 | `	}` |
|   255880 |  950 | `	return rcOut;` |
|   128044 |  951 | `}` |
|        - |  952 | `/*` |
|        - |  953 | ` * Drop a body frame's pending catch/finally return: clear the flag and release` |
|        - |  954 | ` * the slot value. Safe on a frame with no pending return (the slot is then an` |
|        - |  955 | ` * empty MEMOBJ_NULL value and the release is a no-op).` |
|        - |  956 | ` */` |
|   154158 |  957 | `PH7_PRIVATE void VmClearFrameReturn(VmFrame *pFrame)` |
|        5 |  958 | `{` |
|   154163 |  959 | `	pFrame->bHasRet = 0;` |
|   154163 |  960 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   154163 |  961 | `}` |
|        - |  962 | `/*` |
|        - |  963 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|        - |  964 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|        - |  965 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|        - |  966 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|        - |  967 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|        - |  968 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|        - |  969 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|        - |  970 | ` */` |
|      162 |  971 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|        5 |  972 | `{` |
|      167 |  973 | `	if( pResult ){` |
|      167 |  974 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|       81 |  975 | `	}` |
|      167 |  976 | `	VmClearFrameReturn(pEntryFrame);` |
|      167 |  977 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|      ! 0 |  978 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  979 | `	}` |
|      167 |  980 | `}` |
|        - |  981 | `/*` |
|        - |  982 | ` * Compare two functions signature and return the comparison result.` |
|        - |  983 | ` */` |
|     1182 |  984 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |  985 | `{` |
|     1183 |  986 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|     1183 |  987 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|     1183 |  988 | `	const char *zSin = pSecond->zString;` |
|     1183 |  989 | `	const char *zFin = pFirst->zString;` |
|     1183 |  990 | `	const char *zPtr = zFin;` |
|      591 |  991 | `	for(;;){` |
|     1183 |  992 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      592 |  993 | `			break;` |
|        - |  994 | `		}` |
|      ! 0 |  995 | `		if( zFin[0] != zSin[0] ){` |
|        - |  996 | `			/* mismatch */` |
|      ! 0 |  997 | `			break;` |
|        - |  998 | `		}` |
|      ! 0 |  999 | `		zFin++;` |
|      ! 0 | 1000 | `		zSin++;` |
|      ! 0 | 1001 | `	}` |
|     1183 | 1002 | `	return (int)(zFin-zPtr);` |
|        1 | 1003 | `}` |
|        - | 1004 | `/*` |
|        - | 1005 | ` * Select the appropriate VM function for the current call context.` |
|        - | 1006 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - | 1007 | ` * introduced by the version 2 of the PH7 engine.` |
|        - | 1008 | ` * Refer to the official documentation for more information.` |
|        - | 1009 | ` */` |
|      242 | 1010 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|        - | 1011 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1012 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - | 1013 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - | 1014 | `	int nArg             /* Total number of passed arguments  */` |
|        - | 1015 | `	)` |
|        3 | 1016 | `{` |
|        - | 1017 | `	int iTarget,i,j,iCur,iMax;` |
|        - | 1018 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - | 1019 | `	ph7_vm_func *pLink;` |
|        - | 1020 | `	SyString sArgSig;` |
|        - | 1021 | `	SyBlob sSig;` |
|        - | 1022 |  |
|      245 | 1023 | `	pLink = pList;` |
|      245 | 1024 | `	i = 0;` |
|        - | 1025 | `	/* Put functions expecting the same number of passed arguments */` |
|     1459 | 1026 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1429 | 1027 | `		if( pLink == 0 ){` |
|      215 | 1028 | `			break;` |
|        - | 1029 | `		}` |
|     1217 | 1030 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - | 1031 | `			/* Candidate for overloading */` |
|     1217 | 1032 | `			apSet[i++] = pLink;` |
|      607 | 1033 | `		}` |
|        - | 1034 | `		/* Point to the next entry */` |
|     1217 | 1035 | `		pLink = pLink->pNextName;` |
|        3 | 1036 | `	}` |
|      245 | 1037 | `	if( i < 1 ){` |
|        - | 1038 | `		/* No candidates,return the head of the list */` |
|      ! 0 | 1039 | `		return pList;` |
|        - | 1040 | `	}` |
|      245 | 1041 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - | 1042 | `		/* Return the only candidate */` |
|       19 | 1043 | `		return apSet[0];` |
|        - | 1044 | `	}` |
|        - | 1045 | `	/* Calculate function signature */` |
|      227 | 1046 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      453 | 1047 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      227 | 1048 | `		int c = 'n'; /* null */` |
|      227 | 1049 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1050 | `			/* Hashmap */` |
|      ! 0 | 1051 | `			c = 'h';` |
|      227 | 1052 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - | 1053 | `			/* bool */` |
|       85 | 1054 | `			c = 'b';` |
|      185 | 1055 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - | 1056 | `			/* int */` |
|       45 | 1057 | `			c = 'i';` |
|      121 | 1058 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - | 1059 | `			/* String */` |
|       87 | 1060 | `			c = 's';` |
|       56 | 1061 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - | 1062 | `			/* Float */` |
|       11 | 1063 | `			c = 'f';` |
|        8 | 1064 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - | 1065 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|      ! 0 | 1066 | `			int marker = 'o';` |
|      ! 0 | 1067 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 | 1068 | `			SyString *pName = &pClass->sName;` |
|      ! 0 | 1069 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|      ! 0 | 1070 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 | 1071 | `			c = -1;` |
|      ! 0 | 1072 | `		}` |
|      227 | 1073 | `		if( c > 0 ){` |
|      227 | 1074 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      113 | 1075 | `		}` |
|      114 | 1076 | `	}` |
|      227 | 1077 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      227 | 1078 | `	iTarget = 0;` |
|      227 | 1079 | `	iMax = -1;` |
|        - | 1080 | `	/* Select the appropriate function */` |
|     1409 | 1081 | `	for( j = 0 ; j < i ; j++ ){` |
|        - | 1082 | `		/* Compare the two signatures */` |
|     1183 | 1083 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|     1183 | 1084 | `		if( iCur > iMax ){` |
|      227 | 1085 | `			iMax = iCur;` |
|      227 | 1086 | `			iTarget = j;` |
|      113 | 1087 | `		}` |
|      592 | 1088 | `	}` |
|      227 | 1089 | `	SyBlobRelease(&sSig);` |
|        - | 1090 | `	/* Appropriate function for the current call context */` |
|      227 | 1091 | `	return apSet[iTarget];` |
|      124 | 1092 | `}` |
|        - | 1093 | `/* Forward declaration */` |
|        - | 1094 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - | 1095 | `/*` |
|        - | 1096 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|        - | 1097 | ` * safely across a pool reallocation.` |
|        - | 1098 | ` *` |
|        - | 1099 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|        - | 1100 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|        - | 1101 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|        - | 1102 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|        - | 1103 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|        - | 1104 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|        - | 1105 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|        - | 1106 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|        - | 1107 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|        - | 1108 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|        - | 1109 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|        - | 1110 | ` */` |
|    34742 | 1111 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|        5 | 1112 | `{` |
|        - | 1113 | `	ph7_value sVal;` |
|    34747 | 1114 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|        - | 1115 | `	sxi32 rc;` |
|    34747 | 1116 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    34747 | 1117 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|        - | 1118 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|    34747 | 1119 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    34747 | 1120 | `	if( *ppMemObj ){` |
|    34747 | 1121 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|    17371 | 1122 | `	}` |
|    34747 | 1123 | `	PH7_MemObjRelease(&sVal);` |
|    34747 | 1124 | `	return rc;` |
|        5 | 1125 | `}` |
|        - | 1126 | `/*` |
|        - | 1127 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - | 1128 | ` * it can be instanciated from the executed PHP script.` |
|        - | 1129 | ` */` |
|        - | 1130 | `/*` |
|        - | 1131 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - | 1132 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - | 1133 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - | 1134 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - | 1135 | ` * properties register their enforcement slot. It is factored out of` |
|        - | 1136 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - | 1137 | ` * reuse without re-installing the (compile-time) methods.` |
|        - | 1138 | ` */` |
|   704196 | 1139 | `static sxi32 VmMountUserClassAttrs(` |
|        - | 1140 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1141 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - | 1142 | `	)` |
|        5 | 1143 | `{` |
|        - | 1144 | `	ph7_class_attr *pAttr;` |
|        - | 1145 | `	SyHashEntry *pEntry;` |
|        - | 1146 | `	/* Reset the loop cursor */` |
|   704201 | 1147 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - | 1148 | `	/* Process only static and constant attribute */` |
|  2866193 | 1149 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1150 | `		/* Extract the current attribute */` |
|  2162001 | 1151 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2161996 | 1152 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  1424746 | 1153 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   344192 | 1154 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|        - | 1155 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|        - | 1156 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|        - | 1157 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|        - | 1158 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|        - | 1159 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|        - | 1160 | `			 * user catch, and initializers referencing constants of a class` |
|        - | 1161 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|        - | 1162 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|        - | 1163 | `			 * %s as value for class constant" fatal without any access). */` |
|   686593 | 1164 | `			continue;` |
|        - | 1165 | `		}` |
|  1475413 | 1166 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 1167 | `			ph7_value *pMemObj;` |
|    24903 | 1168 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|        - | 1169 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|        - | 1170 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|        - | 1171 | `				 * re-mount pass, so VM reuse still re-evaluates. */` |
|      872 | 1172 | `				continue;` |
|        - | 1173 | `			}` |
|        - | 1174 | `			/* Reserve a memory object for this constant/static attribute */` |
|    24033 | 1175 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    24033 | 1176 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1177 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1178 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 | 1179 | `					&pClass->sName,&pAttr->sName` |
|        - | 1180 | `					);` |
|      ! 0 | 1181 | `				return SXERR_MEM;` |
|        - | 1182 | `			}` |
|    24033 | 1183 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1184 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1185 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|        - | 1186 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|    24025 | 1187 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        - | 1188 | `				sxi32 rcExec;` |
|    24025 | 1189 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    24025 | 1190 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|    24025 | 1191 | `				pVm->nConstEvalDepth++;` |
|    24025 | 1192 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    24025 | 1193 | `				pVm->nConstEvalDepth--;` |
|    24025 | 1194 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|    24025 | 1195 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    24025 | 1196 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1197 | `					/* The initializer raised (self-referencing constant, or a` |
|        - | 1198 | `					 * throwing enum-case reference): park it for the fetch-point` |
|        - | 1199 | `					 * router — user classes mount mid-execution, so the throw` |
|        - | 1200 | `					 * lands catchably at the declaration site. */` |
|      ! 0 | 1201 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|    24025 | 1202 | `				}else if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 1203 | `					/* A nested evaluation detected a self-referencing constant:` |
|        - | 1204 | `					 * raise it at this, the outermost level. */` |
|      ! 0 | 1205 | `					VmBoundaryPark(&(*pVm),VmConstCycleThrow(&(*pVm)));` |
|      ! 0 | 1206 | `				}` |
|        - | 1207 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - | 1208 | `				 * against the declared type. A mismatch is a non-catchable` |
|        - | 1209 | `				 * fatal, raised here at definition time (matching PHP). */` |
|    24020 | 1210 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|    12015 | 1211 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|       35 | 1212 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|       35 | 1213 | `					if( rcType != SXRET_OK ){` |
|        6 | 1214 | `						return rcType;` |
|        - | 1215 | `					}` |
|       14 | 1216 | `				}` |
|    12008 | 1217 | `			}` |
|        - | 1218 | `			/* Record attribute index */` |
|    24029 | 1219 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - | 1220 | `			/* Install static attribute in the reference table */` |
|    24029 | 1221 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1222 | `			/* If this is a typed static property, register the slot so the` |
|        - | 1223 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - | 1224 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - | 1225 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - | 1226 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - | 1227 | `			 * already enforced above, so they need no store-time slot. */` |
|    24024 | 1228 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    12040 | 1229 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       21 | 1230 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       21 | 1231 | `				if( pVmAttrS == 0 ){` |
|      ! 0 | 1232 | `					return SXERR_MEM;` |
|        - | 1233 | `				}` |
|       21 | 1234 | `				pVmAttrS->pAttr = pAttr;` |
|       21 | 1235 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       21 | 1236 | `				pVmAttrS->iState = 0;` |
|       21 | 1237 | `				pVmAttrS->pOwner = pClass;` |
|        - | 1238 | `				/* Static typed property with no default starts uninitialized` |
|        - | 1239 | `				 * (constants are already excluded by the enclosing condition). */` |
|       21 | 1240 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        6 | 1241 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 | 1242 | `				}` |
|       21 | 1243 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 | 1244 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 | 1245 | `					return SXERR_MEM;` |
|        - | 1246 | `				}` |
|        9 | 1247 | `			}` |
|    12012 | 1248 | `		}` |
|        5 | 1249 | `	}` |
|   704197 | 1250 | `	return SXRET_OK;` |
|   352103 | 1251 | `}` |
|   703120 | 1252 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - | 1253 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1254 | `	ph7_class *pClass /* Class to be mounted */` |
|        - | 1255 | `	)` |
|        5 | 1256 | `{` |
|        - | 1257 | `	ph7_class_method *pMeth;` |
|        - | 1258 | `	SyHashEntry *pEntry;` |
|        - | 1259 | `	sxi32 rc;` |
|        - | 1260 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   703125 | 1261 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   703125 | 1262 | `	if( rc != SXRET_OK ){` |
|        6 | 1263 | `		return rc;` |
|        - | 1264 | `	}` |
|        - | 1265 | `	/* Install class methods */` |
|   703121 | 1266 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - | 1267 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - | 1268 | `		 */` |
|   312373 | 1269 | `		return SXRET_OK;` |
|        - | 1270 | `	}` |
|        - | 1271 | `	/* Create constructor alias if not yet done */` |
|   390753 | 1272 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - | 1273 | `		/* User constructor with the same base class name */` |
|    59497 | 1274 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|    59497 | 1275 | `		if( pEntry ){` |
|      ! 0 | 1276 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 1277 | `			/* Create the alias */` |
|      ! 0 | 1278 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 | 1279 | `		}` |
|    29746 | 1280 | `	}` |
|        - | 1281 | `	/* Install the methods now */` |
|   390753 | 1282 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  5884351 | 1283 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5298229 | 1284 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5298229 | 1285 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  5287965 | 1286 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  5287965 | 1287 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1288 | `				return rc;` |
|        - | 1289 | `			}` |
|  2643980 | 1290 | `		}` |
|        5 | 1291 | `	}` |
|        - | 1292 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   390753 | 1293 | `	pClass->bMounted = TRUE;` |
|   390753 | 1294 | `	return SXRET_OK;` |
|   351565 | 1295 | `}` |
|        - | 1296 | `/*` |
|        - | 1297 | ` * Allocate a private frame for attributes of the given` |
|        - | 1298 | ` * class instance (Object in the PHP jargon).` |
|        - | 1299 | ` */` |
|     8658 | 1300 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - | 1301 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 1302 | `	ph7_class_instance *pObj /* Class instance */` |
|        - | 1303 | `	)` |
|        5 | 1304 | `{` |
|     8663 | 1305 | `	ph7_class *pClass = pObj->pClass;` |
|        - | 1306 | `	ph7_class_attr *pAttr;` |
|        - | 1307 | `	SyHashEntry *pEntry;` |
|        - | 1308 | `	sxi32 rc;` |
|        - | 1309 | `	/* Install class attribute in the private frame associated with this instance */` |
|     8663 | 1310 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    45563 | 1311 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1312 | `		VmClassAttr *pVmAttr;` |
|        - | 1313 | `		/* Extract the current attribute */` |
|    36905 | 1314 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    36905 | 1315 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|    36905 | 1316 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1317 | `			return SXERR_MEM;` |
|        - | 1318 | `		}` |
|    36905 | 1319 | `		pVmAttr->pAttr = pAttr;` |
|    36905 | 1320 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - | 1321 | `			ph7_value *pMemObj;` |
|        - | 1322 | `			/* Reserve a memory object for this attribute */` |
|    28851 | 1323 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    28851 | 1324 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1325 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1326 | `				return SXERR_MEM;` |
|        - | 1327 | `			}` |
|    28851 | 1328 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|    28851 | 1329 | `			pVmAttr->iState = 0;` |
|    28851 | 1330 | `			pVmAttr->pOwner = pClass;` |
|    28851 | 1331 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1332 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1333 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|        - | 1334 | `				 * against the declaring class (no method frame here). */` |
|    10515 | 1335 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|    10515 | 1336 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    10515 | 1337 | `				VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    10515 | 1338 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    23596 | 1339 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1340 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - | 1341 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|      403 | 1342 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      199 | 1343 | `			}` |
|    28851 | 1344 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|    28851 | 1345 | `			if( rc != SXRET_OK ){` |
|        - | 1346 | `				VmSlot sSlot;` |
|        - | 1347 | `				/* Restore memory object */` |
|      ! 0 | 1348 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1349 | `				sSlot.pUserData = 0;` |
|      ! 0 | 1350 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1351 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1352 | `				return SXERR_MEM;` |
|        - | 1353 | `			}` |
|        - | 1354 | `			/* Install attribute in the reference table */` |
|    28851 | 1355 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1356 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - | 1357 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - | 1358 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|    28851 | 1359 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      617 | 1360 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      617 | 1361 | `				if( rc != SXRET_OK ){` |
|        - | 1362 | `					VmSlot sSlot;` |
|      ! 0 | 1363 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 | 1364 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1365 | `					sSlot.pUserData = 0;` |
|      ! 0 | 1366 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1367 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1368 | `					return SXERR_MEM;` |
|        - | 1369 | `				}` |
|      306 | 1370 | `			}` |
|    14428 | 1371 | `		}else{` |
|        - | 1372 | `			/* Install static/constant attribute */` |
|     8058 | 1373 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|     8058 | 1374 | `			pVmAttr->iState = 0;` |
|     8058 | 1375 | `			pVmAttr->pOwner = pClass;` |
|     8058 | 1376 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     8058 | 1377 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1378 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1379 | `				return SXERR_MEM;` |
|        - | 1380 | `			}` |
|        - | 1381 | `		}` |
|        5 | 1382 | `	}` |
|     8663 | 1383 | `	return SXRET_OK;` |
|     4334 | 1384 | `}` |
|        - | 1385 | `/*` |
|        - | 1386 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|        - | 1387 | ` * stdClass for now; the future general-dynamic-props work turns` |
|        - | 1388 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|        - | 1389 | ` */` |
|       62 | 1390 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 1391 | `{` |
|       65 | 1392 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|        3 | 1393 | `}` |
|        - | 1394 | `/*` |
|        - | 1395 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|        - | 1396 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|        - | 1397 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|        - | 1398 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|        - | 1399 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|        - | 1400 | ` */` |
|        2 | 1401 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|        1 | 1402 | `{` |
|        5 | 1403 | `	while( pClass ){` |
|        3 | 1404 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|        - | 1405 | `		sxu32 n;` |
|        3 | 1406 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|      ! 0 | 1407 | `			if( aAttr[n].sName.nByte == nName` |
|      ! 0 | 1408 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|      ! 0 | 1409 | `				return TRUE;` |
|        - | 1410 | `			}` |
|      ! 0 | 1411 | `		}` |
|        3 | 1412 | `		pClass = pClass->pBase;` |
|        1 | 1413 | `	}` |
|        3 | 1414 | `	return FALSE;` |
|        2 | 1415 | `}` |
|        - | 1416 | `/*` |
|        - | 1417 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|        - | 1418 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|        - | 1419 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|        - | 1420 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|        - | 1421 | ` *` |
|        - | 1422 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|        - | 1423 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|        - | 1424 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|        - | 1425 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|        - | 1426 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|        - | 1427 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|        - | 1428 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|        - | 1429 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|        - | 1430 | ` */` |
|      140 | 1431 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|        4 | 1432 | `{` |
|        - | 1433 | `	ph7_class_attr *pAttr;` |
|      144 | 1434 | `	VmClassAttr *pVmAttr = 0;` |
|      144 | 1435 | `	ph7_value *pMemObj = 0;` |
|        - | 1436 | `	char *zCopy;` |
|        - | 1437 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|      144 | 1438 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|      144 | 1439 | `	if( pAttr == 0 ){` |
|      ! 0 | 1440 | `		return 0;` |
|        - | 1441 | `	}` |
|      144 | 1442 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      144 | 1443 | `	zCopy = (char *)&pAttr[1];` |
|      144 | 1444 | `	if( nName > 0 ){` |
|      144 | 1445 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       70 | 1446 | `	}` |
|      144 | 1447 | `	zCopy[nName] = 0;` |
|      144 | 1448 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|      144 | 1449 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|      144 | 1450 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      144 | 1451 | `	pAttr->pDeclClass = pThis->pClass;` |
|        - | 1452 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|        - | 1453 | `	 * value, never a union. */` |
|      144 | 1454 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      144 | 1455 | `	if( pVmAttr == 0 ){` |
|      ! 0 | 1456 | `		goto fail_attr;` |
|        - | 1457 | `	}` |
|      144 | 1458 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      144 | 1459 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1460 | `		goto fail_vmattr;` |
|        - | 1461 | `	}` |
|      144 | 1462 | `	pVmAttr->pAttr = pAttr;` |
|      144 | 1463 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|      144 | 1464 | `	pVmAttr->iState = 0;` |
|      144 | 1465 | `	pVmAttr->pOwner = pThis->pClass;` |
|        - | 1466 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|        - | 1467 | `	 * property-creation order, matching PHP. */` |
|      144 | 1468 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 1469 | `		goto fail_slot;` |
|        - | 1470 | `	}` |
|        - | 1471 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|      144 | 1472 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      144 | 1473 | `	if( ppAttr ){` |
|       77 | 1474 | `		*ppAttr = pVmAttr;` |
|       37 | 1475 | `	}` |
|      144 | 1476 | `	return pMemObj;` |
|      ! 0 | 1477 | `fail_slot:` |
|        - | 1478 | `	{` |
|        - | 1479 | `		VmSlot sSlot;` |
|      ! 0 | 1480 | `		sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1481 | `		sSlot.pUserData = 0;` |
|      ! 0 | 1482 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1483 | `	}` |
|      ! 0 | 1484 | `fail_vmattr:` |
|      ! 0 | 1485 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1486 | `fail_attr:` |
|      ! 0 | 1487 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|      ! 0 | 1488 | `	return 0;` |
|       74 | 1489 | `}` |
|        - | 1490 | `/*` |
|        - | 1491 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|        - | 1492 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|        - | 1493 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|        - | 1494 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|        - | 1495 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|        - | 1496 | ` */` |
|        6 | 1497 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|        1 | 1498 | `{` |
|        - | 1499 | `	VmClassAttr *pVmAttr;` |
|        - | 1500 | `	ph7_value *pMemObj;` |
|        7 | 1501 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        7 | 1502 | `	if( pVmAttr == 0 ){` |
|      ! 0 | 1503 | `		return;` |
|        - | 1504 | `	}` |
|        7 | 1505 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|        7 | 1506 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1507 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1508 | `		return;` |
|        - | 1509 | `	}` |
|        7 | 1510 | `	pVmAttr->pAttr = pAttr;` |
|        7 | 1511 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|        7 | 1512 | `	pVmAttr->iState = 0;` |
|        7 | 1513 | `	pVmAttr->pOwner = pThis->pClass;` |
|        - | 1514 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|        - | 1515 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|        - | 1516 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|        - | 1517 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|        - | 1518 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|        7 | 1519 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1520 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      ! 0 | 1521 | `	}` |
|        - | 1522 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|        - | 1523 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|        - | 1524 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|        - | 1525 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|        7 | 1526 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|        - | 1527 | `		VmSlot sSlot;` |
|      ! 0 | 1528 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|      ! 0 | 1529 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1530 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1531 | `		return;` |
|        - | 1532 | `	}` |
|        7 | 1533 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        7 | 1534 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1535 | `		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|        - | 1536 | `			VmSlot sSlot;` |
|      ! 0 | 1537 | `			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 | 1538 | `			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|      ! 0 | 1539 | `			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1540 | `			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1541 | `			return;` |
|        - | 1542 | `		}` |
|      ! 0 | 1543 | `	}` |
|        7 | 1544 | `	if( ppAttr ){` |
|        7 | 1545 | `		*ppAttr = pVmAttr;` |
|        3 | 1546 | `	}` |
|        4 | 1547 | `}` |
|        - | 1548 | `/* Forward declaration */` |
|        - | 1549 | `/*` |
|        - | 1550 | ` * Dummy read-only buffer used for slot reservation.` |
|        - | 1551 | ` */` |
|        - | 1552 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - | 1553 | `/*` |
|        - | 1554 | ` * Reserve a constant memory object.` |
|        - | 1555 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 1556 | ` */` |
|  8574352 | 1557 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1558 | `{` |
|        - | 1559 | `	ph7_value *pObj;` |
|        - | 1560 | `	sxi32 rc;` |
|  8574357 | 1561 | `	if( pIndex ){` |
|        - | 1562 | `		/* Object index in the object table */` |
|  8562711 | 1563 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|  4281353 | 1564 | `	}` |
|        - | 1565 | `	/* Reserve a slot for the new object */` |
|  8574357 | 1566 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|  8574357 | 1567 | `	if( rc != SXRET_OK ){` |
|        - | 1568 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1569 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1570 | `		 */` |
|      ! 0 | 1571 | `		return 0;` |
|        - | 1572 | `	}` |
|  8574357 | 1573 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|  8574357 | 1574 | `	return pObj;` |
|  4287181 | 1575 | `}` |
|        - | 1576 | `/*` |
|        - | 1577 | ` * Reserve a memory object.` |
|        - | 1578 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 1579 | ` */` |
|  2237484 | 1580 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1581 | `{` |
|        - | 1582 | `	ph7_value *pObj;` |
|        - | 1583 | `	sxi32 rc;` |
|  2237489 | 1584 | `	if( pIndex ){` |
|        - | 1585 | `		/* Object index in the object table */` |
|  2237489 | 1586 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1118742 | 1587 | `	}` |
|        - | 1588 | `	/* Reserve a slot for the new object */` |
|  2237489 | 1589 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2237489 | 1590 | `	if( rc != SXRET_OK ){` |
|        - | 1591 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1592 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1593 | `		 */` |
|      ! 0 | 1594 | `		return 0;` |
|        - | 1595 | `	}` |
|  2237489 | 1596 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2237489 | 1597 | `	return pObj;` |
|  1118747 | 1598 | `}` |
|        - | 1599 | `/* Forward declaration */` |
|        - | 1600 | `/* Forward declarations for Fiber C functions */` |
|        - | 1601 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - | 1602 | `/* Forward declarations for Generator helpers and C functions */` |
|        - | 1603 | `/*` |
|        - | 1604 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - | 1605 | ` * directly as foreign functions.` |
|        - | 1606 | ` */` |
|        - | 1607 |  |
|        - | 1608 | `/*` |
|        - | 1609 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - | 1610 | ` * start compiling the target PHP program.` |
|        - | 1611 | ` */` |
|     3882 | 1612 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - | 1613 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - | 1614 | `	 ph7 *pEngine /* Master engine */` |
|        - | 1615 | `	 )` |
|        5 | 1616 | `{` |
|        - | 1617 | `	ph7_value *pObj;` |
|        - | 1618 | `	sxi32 rc;` |
|        - | 1619 | `	/* Zero the structure */` |
|     3887 | 1620 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - | 1621 | `	/* Initialize VM fields */` |
|     3887 | 1622 | `	pVm->pEngine = &(*pEngine);` |
|     3887 | 1623 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|     3887 | 1624 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - | 1625 | `	/* Instructions containers */` |
|     3887 | 1626 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3887 | 1627 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3887 | 1628 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - | 1629 | `	/* Object containers */` |
|     3887 | 1630 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3887 | 1631 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - | 1632 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|     3887 | 1633 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|     3887 | 1634 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|     3887 | 1635 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|     3887 | 1636 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|        - | 1637 | `	/* Virtual machine internal containers */` |
|     3887 | 1638 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3887 | 1639 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3887 | 1640 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|     3887 | 1641 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|     3887 | 1642 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3887 | 1643 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3887 | 1644 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3887 | 1645 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3887 | 1646 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3887 | 1647 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3887 | 1648 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3887 | 1649 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3887 | 1650 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3887 | 1651 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3887 | 1652 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3887 | 1653 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3887 | 1654 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3887 | 1655 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3887 | 1656 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3887 | 1657 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|     3887 | 1658 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3887 | 1659 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3887 | 1660 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|     3887 | 1661 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3887 | 1662 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3887 | 1663 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|     3887 | 1664 | `	pVm->pMagicSetThis = 0;` |
|     3887 | 1665 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|     3887 | 1666 | `	pVm->pHookSetThis = 0;` |
|     3887 | 1667 | `	pVm->pHookSetAttr = 0;` |
|     3887 | 1668 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3887 | 1669 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|     3887 | 1670 | `	pVm->pMagicCallThis = 0;` |
|     3887 | 1671 | `	pVm->pMagicCallClass = 0;` |
|     3887 | 1672 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|     3887 | 1673 | `	pVm->pIdleCallFrames = 0;` |
|     3887 | 1674 | `	pVm->pIdleOperandStacks = 0;` |
|     3887 | 1675 | `	pVm->nIdleOperandStacks = 0;` |
|     3887 | 1676 | `	pVm->pIdleStackNodes = 0;` |
|     3887 | 1677 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|     3887 | 1678 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|     3887 | 1679 | `	pVm->pPendingException = 0;` |
|     3887 | 1680 | `	pVm->pInflightException = 0;` |
|     3887 | 1681 | `	pVm->nInflightExcBase = 0;` |
|     3887 | 1682 | `	pVm->pResumeFrame = 0;` |
|     3887 | 1683 | `	pVm->iResumePc = 0;` |
|     3887 | 1684 | `	pVm->pResumeInstr = 0;` |
|     3887 | 1685 | `	pVm->iResumeStackDepth = 0;` |
|     3887 | 1686 | `	pVm->nBoundaryRc = 0;` |
|     3887 | 1687 | `	pVm->pConstEvalClass = 0;` |
|     3887 | 1688 | `	pVm->nConstEvalDepth = 0;` |
|     3887 | 1689 | `	pVm->pConstCycleAttr = 0;` |
|     3887 | 1690 | `	pVm->pConstCycleClass = 0;` |
|     3887 | 1691 | `	SySetReset(&pVm->aMagicGuard);` |
|     3887 | 1692 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 1693 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 1694 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 1695 | `	}` |
|     3887 | 1696 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|     3887 | 1697 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 1698 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 1699 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 1700 | `	}` |
|     3887 | 1701 | `	pVm->pHookSetAttr = 0;` |
|     3887 | 1702 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3887 | 1703 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 1704 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 1705 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 1706 | `	}` |
|     3887 | 1707 | `	pVm->pMagicCallClass = 0;` |
|     3887 | 1708 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        - | 1709 | `	/* Configuration containers */` |
|     3887 | 1710 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3887 | 1711 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3887 | 1712 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3887 | 1713 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3887 | 1714 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3887 | 1715 | `	pVm->iResponseStatus = 200;` |
|     3887 | 1716 | `	pVm->bHeadersSent = 0;` |
|     3887 | 1717 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - | 1718 | `	/* Error callbacks containers */` |
|     3887 | 1719 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3887 | 1720 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3887 | 1721 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3887 | 1722 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3887 | 1723 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - | 1724 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|        - | 1725 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|        - | 1726 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|        - | 1727 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|        - | 1728 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|        - | 1729 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|        - | 1730 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3887 | 1731 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|     3887 | 1732 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|        - | 1733 | `	                             * usort-in-comparator path overflows at 1024) */` |
|        - | 1734 | `#else` |
|        - | 1735 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|        - | 1736 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|        - | 1737 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|        - | 1738 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|        - | 1739 | `	pVm->nMaxDepth = 512;` |
|        - | 1740 | `	pVm->nMaxNativeDepth = 16;` |
|        - | 1741 | `#endif` |
|        - | 1742 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|        - | 1743 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|        - | 1744 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|     3887 | 1745 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|        - | 1746 | `	/* JSON return status */` |
|     3887 | 1747 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 1748 | `	/* PRNG context */` |
|     3887 | 1749 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - | 1750 | `	/* Install the null constant */` |
|     3887 | 1751 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3887 | 1752 | `	if( pObj == 0 ){` |
|      ! 0 | 1753 | `		rc = SXERR_MEM;` |
|      ! 0 | 1754 | `		goto Err;` |
|        - | 1755 | `	}` |
|     3887 | 1756 | `	PH7_MemObjInit(pVm,pObj);` |
|        - | 1757 | `	/* Install the boolean TRUE constant */` |
|     3887 | 1758 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3887 | 1759 | `	if( pObj == 0 ){` |
|      ! 0 | 1760 | `		rc = SXERR_MEM;` |
|      ! 0 | 1761 | `		goto Err;` |
|        - | 1762 | `	}` |
|     3887 | 1763 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - | 1764 | `	/* Install the boolean FALSE constant */` |
|     3887 | 1765 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3887 | 1766 | `	if( pObj == 0 ){` |
|      ! 0 | 1767 | `		rc = SXERR_MEM;` |
|      ! 0 | 1768 | `		goto Err;` |
|        - | 1769 | `	}` |
|     3887 | 1770 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - | 1771 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - | 1772 | `	 * reuse the same slot rather than allocating a new one.` |
|        - | 1773 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3887 | 1774 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3887 | 1775 | `	if( pObj == 0 ){` |
|      ! 0 | 1776 | `		rc = SXERR_MEM;` |
|      ! 0 | 1777 | `		goto Err;` |
|        - | 1778 | `	}` |
|     3887 | 1779 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - | 1780 | `	/* Create the global frame */` |
|     3887 | 1781 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3887 | 1782 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1783 | `		goto Err;` |
|        - | 1784 | `	}` |
|        - | 1785 | `	/* Initialize the code generator */` |
|     3887 | 1786 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3887 | 1787 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1788 | `		goto Err;` |
|        - | 1789 | `	}` |
|        - | 1790 | `	/* VM correctly initialized,set the magic number */` |
|     3887 | 1791 | `	pVm->nMagic = PH7_VM_INIT;` |
|        - | 1792 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|        - | 1793 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|     3887 | 1794 | `	pVm->bCompilingBuiltin = 1;` |
|        - | 1795 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|     3887 | 1796 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|        - | 1797 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|        - | 1798 | `	 * compiled — its classes are internal too. */` |
|        - | 1799 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3887 | 1800 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - | 1801 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3887 | 1802 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3887 | 1803 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3887 | 1804 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3887 | 1805 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3887 | 1806 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - | 1807 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3887 | 1808 | `	pVm->pCoalesceObj = 0;` |
|     3887 | 1809 | `	pVm->bCoalesceArmed = 0;` |
|     3887 | 1810 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - | 1811 | `	/* Register Fiber internal C functions */` |
|     3887 | 1812 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3887 | 1813 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3887 | 1814 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3887 | 1815 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3887 | 1816 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3887 | 1817 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3887 | 1818 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3887 | 1819 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3887 | 1820 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3887 | 1821 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - | 1822 | `	/* Cache the Closure class pointer (closures are instances of it) */` |
|     3887 | 1823 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|     3887 | 1824 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|     3887 | 1825 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|        - | 1826 | `	/* Closure::bind/bindTo/call/fromCallable native delegates (Increment 2) */` |
|     3887 | 1827 | `	ph7_create_function(pVm,"__closure_bindTo",vm_builtin_Closure_bindTo,0);` |
|     3887 | 1828 | `	ph7_create_function(pVm,"__closure_fromCallable",vm_builtin_Closure_fromCallable,0);` |
|        - | 1829 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|     3887 | 1830 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        - | 1831 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3887 | 1832 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3887 | 1833 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3887 | 1834 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3887 | 1835 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3887 | 1836 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3887 | 1837 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3887 | 1838 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3887 | 1839 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3887 | 1840 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3887 | 1841 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - | 1842 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|        - | 1843 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|        - | 1844 | `	 * internal; the Traversable pointer above must already be cached. */` |
|     3887 | 1845 | `	PH7_VmInstallReflection(&(*pVm));` |
|     3887 | 1846 | `	PH7_VmInstallDateTime(&(*pVm));` |
|     3887 | 1847 | `	PH7_VmInstallSpl(&(*pVm));` |
|     3887 | 1848 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|     3887 | 1849 | `	PH7_VmInstallSession(&(*pVm));` |
|     3887 | 1850 | `	PH7_VmInstallIni(&(*pVm));` |
|        - | 1851 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1852 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|        - | 1853 | `	 * XMLWriter class libraries that build on it. */` |
|     3887 | 1854 | `	PH7_VmInstallLibxml(&(*pVm));` |
|     3887 | 1855 | `	PH7_VmInstallDom(&(*pVm));` |
|     3887 | 1856 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|        - | 1857 | `#endif` |
|     3887 | 1858 | `	pVm->bCompilingBuiltin = 0;` |
|        - | 1859 | `	/* Reset the code generator */` |
|     3887 | 1860 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3887 | 1861 | `	return SXRET_OK;` |
|      ! 0 | 1862 | `Err:` |
|      ! 0 | 1863 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 | 1864 | `	return rc;` |
|     1946 | 1865 | `}` |
|        - | 1866 | `/*` |
|        - | 1867 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - | 1868 | ` * routine which store the output in an internal blob.` |
|        - | 1869 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - | 1870 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - | 1871 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - | 1872 | ` * Refer to the official docurmentation for additional information.` |
|        - | 1873 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - | 1874 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - | 1875 | ` * to finish executing and extracting the output.` |
|        - | 1876 | ` */` |
|       66 | 1877 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - | 1878 | `	const void *pOut,   /* VM Generated output*/` |
|        - | 1879 | `	unsigned int nLen,  /* Generated output length */` |
|        - | 1880 | `	void *pUserData     /* User private data */` |
|        - | 1881 | `	)` |
|      ! 0 | 1882 | `{` |
|        - | 1883 | `	 sxi32 rc;` |
|        - | 1884 | `	 /* Store the output in an internal BLOB */` |
|       66 | 1885 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       66 | 1886 | `	 return rc;` |
|      ! 0 | 1887 | `}` |
|        - | 1888 | `/*` |
|        - | 1889 | ` * Track output length and mark headers as sent when output reaches` |
|        - | 1890 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - | 1891 | ` */` |
|    41234 | 1892 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 | 1893 | `{` |
|    41239 | 1894 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    41239 | 1895 | `	if( xCons != VmObConsumer ){` |
|    12453 | 1896 | `		pVm->nOutputLen += nLen;` |
|    12453 | 1897 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1385 | 1898 | `			pVm->bHeadersSent = 1;` |
|      690 | 1899 | `		}` |
|     6224 | 1900 | `	}` |
|    41239 | 1901 | `}` |
|        - | 1902 | `/*` |
|        - | 1903 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|        - | 1904 | ` *` |
|        - | 1905 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|        - | 1906 | ` * (no instruction pushes more than one net slot), and that is what` |
|        - | 1907 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|        - | 1908 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|        - | 1909 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|        - | 1910 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|        - | 1911 | ` *` |
|        - | 1912 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|        - | 1913 | ` * conservative BY CONSTRUCTION:` |
|        - | 1914 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|        - | 1915 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|        - | 1916 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|        - | 1917 | ` *     pop — makes height go negative, which triggers fallback.` |
|        - | 1918 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|        - | 1919 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|        - | 1920 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|        - | 1921 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|        - | 1922 | ` *     bound. There is no partial/unsafe middle.` |
|        - | 1923 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|        - | 1924 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|        - | 1925 | ` *     instruction-count bound -> fallback.` |
|        - | 1926 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|        - | 1927 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|        - | 1928 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|        - | 1929 | ` *` |
|        - | 1930 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|        - | 1931 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|        - | 1932 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|        - | 1933 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|        - | 1934 | ` *` |
|        - | 1935 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|        - | 1936 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|        - | 1937 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|        - | 1938 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|        - | 1939 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|        - | 1940 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|        - | 1941 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|        - | 1942 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|        - | 1943 | ` * entry here.` |
|        - | 1944 | ` */` |
|        - | 1945 | `/*` |
|        - | 1946 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|        - | 1947 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|        - | 1948 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|        - | 1949 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|        - | 1950 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|        - | 1951 | ` */` |
|    45868 | 1952 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|        5 | 1953 | `{` |
|    45873 | 1954 | `	int push = 0, n = 0;` |
|        - | 1955 | `	sxi32 d;` |
|    45873 | 1956 | `	switch( pI->iOp ){` |
|        - | 1957 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|        - | 1958 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     7202 | 1959 | `	case PH7_OP_LOADC:` |
|        - | 1960 | `	case PH7_OP_DUP:` |
|    14409 | 1961 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|     5443 | 1962 | `	case PH7_OP_LOAD:` |
|    10891 | 1963 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|    10891 | 1964 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|      ! 0 | 1965 | `	case PH7_OP_LOAD_REF:` |
|      ! 0 | 1966 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1967 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|      340 | 1968 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|        - | 1969 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|        - | 1970 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      685 | 1971 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|        - | 1972 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|        - | 1973 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|      269 | 1974 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - | 1975 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|      543 | 1976 | `		if( pI->iP2 == 0 ){` |
|      543 | 1977 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|      274 | 1978 | `		}else{` |
|      ! 0 | 1979 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|      ! 0 | 1980 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|        - | 1981 | `		}` |
|      543 | 1982 | `		break;` |
|        - | 1983 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|        - | 1984 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|      155 | 1985 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|        - | 1986 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|        - | 1987 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|        - | 1988 | `	case PH7_OP_NOOP:` |
|      314 | 1989 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1990 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|        - | 1991 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      255 | 1992 | `	case PH7_OP_STORE:` |
|      515 | 1993 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      515 | 1994 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|        - | 1995 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      696 | 1996 | `	case PH7_OP_POP:` |
|        - | 1997 | `	case PH7_OP_CONSUME:` |
|     1397 | 1998 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 1999 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|        - | 2000 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|        - | 2001 | `	 * true pop count is a runtime value — never reaches here. */` |
|     1899 | 2002 | `	case PH7_OP_CALL:` |
|     3803 | 2003 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2004 | `	/* Jumps. */` |
|      147 | 2005 | `	case PH7_OP_JMP:` |
|      299 | 2006 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|      766 | 2007 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|     1537 | 2008 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|     1537 | 2009 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|        - | 2010 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|     2954 | 2011 | `	case PH7_OP_DONE:` |
|     5913 | 2012 | `		n = 0; break;` |
|     2808 | 2013 | `	default:` |
|     5621 | 2014 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|        - | 2015 | `	}` |
|    40257 | 2016 | `	*pPush = push; *pN = n;` |
|    40257 | 2017 | `	return 1;` |
|    22939 | 2018 | `}` |
|        - | 2019 | `/*` |
|        - | 2020 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|        - | 2021 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|        - | 2022 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|        - | 2023 | ` */` |
|     7636 | 2024 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|        5 | 2025 | `{` |
|        - | 2026 | `	void *pScratch;` |
|        - | 2027 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|        - | 2028 | `	sxu32 nQ, i, nIter, nCap;` |
|        - | 2029 | `	sxi32 iMax;` |
|        - | 2030 | `	int push, n, k;` |
|        - | 2031 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     7641 | 2032 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|        - | 2033 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|      ! 0 | 2034 | `		return VM_STACK_UNMODELED;` |
|        - | 2035 | `	}` |
|        - | 2036 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|    39327 | 2037 | `	for( i = 0; i < nInstr; i++ ){` |
|    37307 | 2038 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     5621 | 2039 | `			return VM_STACK_UNMODELED;` |
|        - | 2040 | `		}` |
|    15848 | 2041 | `	}` |
|        - | 2042 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|        - | 2043 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|        - | 2044 | `	 * first (the byte array last needs no alignment). */` |
|     2025 | 2045 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|     2025 | 2046 | `	if( pScratch == 0 ){` |
|      ! 0 | 2047 | `		return VM_STACK_UNMODELED;` |
|        - | 2048 | `	}` |
|     2025 | 2049 | `	aH  = (sxi32 *)pScratch;` |
|     2025 | 2050 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|     2025 | 2051 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|    12309 | 2052 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|     2025 | 2053 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|     2025 | 2054 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|    10591 | 2055 | `	while( nQ > 0 ){` |
|     8571 | 2056 | `		sxu32 pc = aQ[--nQ];` |
|        - | 2057 | `		sxi32 h;` |
|     8571 | 2058 | `		aIn[pc] = 0;` |
|     8571 | 2059 | `		h = aH[pc];` |
|     8571 | 2060 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     8571 | 2061 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     8571 | 2062 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     8571 | 2063 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|    15195 | 2064 | `		for( k = 0; k < n; k++ ){` |
|     6629 | 2065 | `			sxi32 hn = h + delta[k];` |
|     6629 | 2066 | `			sxu32 t = succ[k];` |
|     6629 | 2067 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|     6629 | 2068 | `			if( hn > aH[t] ){` |
|     6551 | 2069 | `				aH[t] = hn;` |
|     6551 | 2070 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|     3273 | 2071 | `			}` |
|     3317 | 2072 | `		}` |
|     8571 | 2073 | `		if( iMax < 0 ){ break; }` |
|        5 | 2074 | `	}` |
|     2025 | 2075 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|     2025 | 2076 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|     3823 | 2077 | `}` |
|        - | 2078 | `/*` |
|        - | 2079 | ` * Allocate a new operand stack so that we can start executing` |
|        - | 2080 | ` * our compiled PHP program.` |
|        - | 2081 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - | 2082 | ` * on success. NULL (Fatal error) on failure.` |
|        - | 2083 | ` *` |
|        - | 2084 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|        - | 2085 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|        - | 2086 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|        - | 2087 | ` * eval, coroutine, callbacks) call this directly.` |
|        - | 2088 | ` */` |
|   240426 | 2089 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|        - | 2090 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 2091 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - | 2092 | `	)` |
|        5 | 2093 | `{` |
|        - | 2094 | `	ph7_value *pStack;` |
|        - | 2095 | `  /* No instruction ever pushes more than a single element onto the` |
|        - | 2096 | `  ** stack and the stack never grows on successive executions of the` |
|        - | 2097 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - | 2098 | `  ** on the maximum stack depth required.` |
|        - | 2099 | `  **` |
|        - | 2100 | `  ** Allocation all the stack space we will ever need.` |
|        - | 2101 | `  */` |
|   240431 | 2102 | `	nInstr += VM_STACK_GUARD;` |
|   240431 | 2103 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   240431 | 2104 | `	if( pStack == 0 ){` |
|      ! 0 | 2105 | `		return 0;` |
|        - | 2106 | `	}` |
|        - | 2107 | `	/* Initialize the operand stack */` |
| 22841217 | 2108 | `	while( nInstr > 0 ){` |
| 22600791 | 2109 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
| 22600791 | 2110 | `		--nInstr;` |
|        5 | 2111 | `	}` |
|        - | 2112 | `	/* Ready for bytecode execution */` |
|   240431 | 2113 | `	return pStack;` |
|   120319 | 2114 | `}` |
|        - | 2115 | `/*` |
|        - | 2116 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|        - | 2117 | ` *` |
|        - | 2118 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|        - | 2119 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|        - | 2120 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|        - | 2121 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|        - | 2122 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|        - | 2123 | ` * the per-slot PH7_MemObjInit.` |
|        - | 2124 | ` *` |
|        - | 2125 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|        - | 2126 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|        - | 2127 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|        - | 2128 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|        - | 2129 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|        - | 2130 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|        - | 2131 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|        - | 2132 | ` *` |
|        - | 2133 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|        - | 2134 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|        - | 2135 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|        - | 2136 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|        - | 2137 | ` * recursion case is the one worth the O(1) simplicity.` |
|        - | 2138 | ` */` |
|        - | 2139 | `typedef struct VmIdleStack VmIdleStack;` |
|        - | 2140 | `struct VmIdleStack {` |
|        - | 2141 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|        - | 2142 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|        - | 2143 | `	VmIdleStack *pNext;  /* LIFO link */` |
|        - | 2144 | `};` |
|        - | 2145 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|        - | 2146 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|        - | 2147 | `                                    * (a large fallback-sized stack recursing would` |
|        - | 2148 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|        - | 2149 | `                                    * the tight-sized hot case is far below this) */` |
|        - | 2150 | `/*` |
|        - | 2151 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|        - | 2152 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|        - | 2153 | ` */` |
|   103581 | 2154 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|        5 | 2155 | `{` |
|   103586 | 2156 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   103586 | 2157 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|   103586 | 2158 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    18518 | 2159 | `		ph7_value *pStack = pIdle->pStack;` |
|    18518 | 2160 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    18518 | 2161 | `		pVm->nIdleOperandStacks--;` |
|        - | 2162 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|        - | 2163 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    18518 | 2164 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    18518 | 2165 | `		pVm->pIdleStackNodes = pIdle;` |
|    18518 | 2166 | `		return pStack; /* slots already released -> reusable without re-init */` |
|        - | 2167 | `	}` |
|    85073 | 2168 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    51897 | 2169 | `}` |
|        - | 2170 | `/*` |
|        - | 2171 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|        - | 2172 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|        - | 2173 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|        - | 2174 | ` * live value.` |
|        - | 2175 | ` */` |
|   103171 | 2176 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|        5 | 2177 | `{` |
|        - | 2178 | `	VmIdleStack *pIdle;` |
|        - | 2179 | `	sxu32 i;` |
|   103176 | 2180 | `	if( pStack == 0 ){` |
|      ! 0 | 2181 | `		return;` |
|        - | 2182 | `	}` |
|   103176 | 2183 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|    79447 | 2184 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|    79447 | 2185 | `		return;` |
|        - | 2186 | `	}` |
|        - | 2187 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|        - | 2188 | `	 * pool-allocate only when the spare list is empty. */` |
|    23734 | 2189 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    23734 | 2190 | `	if( pIdle ){` |
|    18518 | 2191 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|     9262 | 2192 | `	}else{` |
|     5221 | 2193 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|     5221 | 2194 | `		if( pIdle == 0 ){` |
|      ! 0 | 2195 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|      ! 0 | 2196 | `			return;` |
|        - | 2197 | `		}` |
|        - | 2198 | `	}` |
|  1565961 | 2199 | `	for( i = 0; i < nCap; i++ ){` |
|  1542232 | 2200 | `		PH7_MemObjRelease(&pStack[i]);` |
|        - | 2201 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|        - | 2202 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|        - | 2203 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|        - | 2204 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|        - | 2205 | `		 * across invocations — cheap defense in depth. */` |
|  1542232 | 2206 | `		pStack[i].nIdx = SXU32_HIGH;` |
|   771312 | 2207 | `	}` |
|    23734 | 2208 | `	pIdle->pStack = pStack;` |
|    23734 | 2209 | `	pIdle->nCap = nCap;` |
|    23734 | 2210 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    23734 | 2211 | `	pVm->pIdleOperandStacks = pIdle;` |
|    23734 | 2212 | `	pVm->nIdleOperandStacks++;` |
|    51692 | 2213 | `}` |
|        - | 2214 | `/* Forward declaration */` |
|        - | 2215 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - | 2216 | `/*` |
|        - | 2217 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - | 2218 | ` * This routine gets called by the PH7 engine after` |
|        - | 2219 | ` * successful compilation of the target PHP program.` |
|        - | 2220 | ` */` |
|     3414 | 2221 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - | 2222 | `	ph7_vm *pVm /* Target VM */` |
|        - | 2223 | `	)` |
|        5 | 2224 | `{` |
|        - | 2225 | `	SyHashEntry *pEntry;` |
|        - | 2226 | `	sxi32 rc;` |
|     3419 | 2227 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - | 2228 | `		/* Initialize your VM first */` |
|      ! 0 | 2229 | `		return SXERR_CORRUPT;` |
|        - | 2230 | `	}` |
|        - | 2231 | `	/* Mark the VM ready for byte-code execution */` |
|     3419 | 2232 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - | 2233 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - | 2234 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - | 2235 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - | 2236 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     3419 | 2237 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - | 2238 | `	/* Emit the DONE instruction */` |
|     3419 | 2239 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     3419 | 2240 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2241 | `		return SXERR_MEM;` |
|        - | 2242 | `	}` |
|        - | 2243 | `	/* Script return value */` |
|     3419 | 2244 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - | 2245 | `	/* Allocate a new operand stack */` |
|     3419 | 2246 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     3419 | 2247 | `	if( pVm->aOps == 0 ){` |
|      ! 0 | 2248 | `		return SXERR_MEM;` |
|        - | 2249 | `	}` |
|        - | 2250 | `	/* Set the default VM output consumer callback and it's` |
|        - | 2251 | `	 * private data. */` |
|     3419 | 2252 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     3419 | 2253 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - | 2254 | `	/* Allocate the reference table */` |
|     3419 | 2255 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     3419 | 2256 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     3419 | 2257 | `	if( pVm->apRefObj == 0 ){` |
|        - | 2258 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2259 | `		return SXERR_MEM;` |
|        - | 2260 | `	}` |
|        - | 2261 | `	/* Zero the reference table */` |
|     3419 | 2262 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - | 2263 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     3419 | 2264 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     3419 | 2265 | `	if( rc != SXRET_OK ){` |
|        - | 2266 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2267 | `		return rc;` |
|        - | 2268 | `	}` |
|        - | 2269 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - | 2270 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - | 2271 | `	 * every object/variable created during execution) is per-exec state that` |
|        - | 2272 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - | 2273 | `	 * below it is compile-time/init state that survives a reset. */` |
|     3419 | 2274 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - | 2275 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     3419 | 2276 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     3419 | 2277 | `	if( rc != SXRET_OK ){` |
|        - | 2278 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2279 | `		return rc;` |
|        - | 2280 | `	}` |
|        - | 2281 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     3419 | 2282 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - | 2283 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|     3419 | 2284 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|        - | 2285 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     3419 | 2286 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - | 2287 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     3419 | 2288 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - | 2289 | `#ifdef PH7_ENABLE_PCRE` |
|        - | 2290 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     3419 | 2291 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     3419 | 2292 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - | 2293 | `#endif` |
|        - | 2294 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2295 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|     3419 | 2296 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|        - | 2297 | `#endif` |
|        - | 2298 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|        - | 2299 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|     3419 | 2300 | `	VmSetBuiltinArity(&(*pVm));` |
|        - | 2301 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|     3419 | 2302 | `	VmSetBuiltinSignatures(&(*pVm));` |
|        - | 2303 | `	/* Initialize and install static and constants class attributes.` |
|        - | 2304 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - | 2305 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - | 2306 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - | 2307 | `	 * that function in sync when changing what is reserved here. */` |
|     3419 | 2308 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   461131 | 2309 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   457719 | 2310 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   457719 | 2311 | `		if( rc != SXRET_OK ){` |
|        3 | 2312 | `			return rc;` |
|        - | 2313 | `		}` |
|        5 | 2314 | `	}` |
|        - | 2315 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     3417 | 2316 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2317 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|     3417 | 2318 | `	pVm->nNextObjId = 1;` |
|        - | 2319 | `	/* VM is ready for bytecode execution */` |
|     3417 | 2320 | `	return SXRET_OK;` |
|     1712 | 2321 | `}` |
|        - | 2322 | `/*` |
|        - | 2323 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - | 2324 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - | 2325 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - | 2326 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - | 2327 | ` * a dangling node pointer in some other object's reference record.` |
|        - | 2328 | ` */` |
|        8 | 2329 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 | 2330 | `{` |
|        - | 2331 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - | 2332 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - | 2333 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      328 | 2334 | `	while( pVm->pRefList ){` |
|      320 | 2335 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 | 2336 | `	}` |
|        8 | 2337 | `}` |
|        - | 2338 | `/*` |
|        - | 2339 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - | 2340 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - | 2341 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - | 2342 | ` */` |
|       56 | 2343 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 | 2344 | `{` |
|       56 | 2345 | `	PH7_MemObjRelease(pObj);` |
|       56 | 2346 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       56 | 2347 | `}` |
|        - | 2348 | `/*` |
|        - | 2349 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - | 2350 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - | 2351 | ` * of statics).` |
|        - | 2352 | ` */` |
|     6760 | 2353 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 | 2354 | `{` |
|     6760 | 2355 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - | 2356 | `	sxu32 k;` |
|     6788 | 2357 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|       28 | 2358 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|       14 | 2359 | `	}` |
|     6760 | 2360 | `}` |
|        - | 2361 | `/*` |
|        - | 2362 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - | 2363 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - | 2364 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - | 2365 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - | 2366 | ` *    captured environment values, its name buffer and its structure (the` |
|        - | 2367 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - | 2368 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - | 2369 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - | 2370 | ` *    has its static sentinels reset.` |
|        - | 2371 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - | 2372 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - | 2373 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - | 2374 | ` */` |
|        8 | 2375 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 | 2376 | `{` |
|        - | 2377 | `	SyHashEntry *pEntry;` |
|        8 | 2378 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     6768 | 2379 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     6760 | 2380 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     6760 | 2381 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 2382 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - | 2383 | `			 * release its captured-by-value environment, then free the entry,` |
|        - | 2384 | `			 * name buffer and structure. */` |
|        4 | 2385 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 | 2386 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - | 2387 | `			sxu32 k;` |
|        4 | 2388 | `			VmResetFuncStatics(pFunc);` |
|        8 | 2389 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 | 2390 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 | 2391 | `			}` |
|        4 | 2392 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - | 2393 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 | 2394 | `			SyHashDeleteEntry2(pEntry);` |
|        4 | 2395 | `			if( zName ){` |
|        4 | 2396 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 | 2397 | `			}` |
|        4 | 2398 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 | 2399 | `			continue;` |
|        - | 2400 | `		}` |
|        - | 2401 | `		/* Named function: reset statics for every overload sharing this name. */` |
|    13512 | 2402 | `		while( pFunc ){` |
|     6756 | 2403 | `			VmResetFuncStatics(pFunc);` |
|     6756 | 2404 | `			pFunc = pFunc->pNextName;` |
|      ! 0 | 2405 | `		}` |
|      ! 0 | 2406 | `	}` |
|        8 | 2407 | `	pVm->closure_cnt = 0;` |
|        8 | 2408 | `}` |
|        - | 2409 | `/*` |
|        - | 2410 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - | 2411 | ` * are already gone (each object's destructor removed its own during the object` |
|        - | 2412 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - | 2413 | ` * the class re-mount registers fresh ones.` |
|        - | 2414 | ` */` |
|        8 | 2415 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 | 2416 | `{` |
|        - | 2417 | `	SyHashEntry *pEntry;` |
|        - | 2418 | `	/* Common case: no class static typed properties — table already empty. */` |
|        8 | 2419 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        4 | 2420 | `		return;` |
|        - | 2421 | `	}` |
|        - | 2422 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - | 2423 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 | 2424 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 | 2425 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 | 2426 | `		if( pEntry->pUserData ){` |
|        4 | 2427 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 | 2428 | `		}` |
|      ! 0 | 2429 | `	}` |
|        4 | 2430 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 | 2431 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        4 | 2432 | `}` |
|        - | 2433 | `/*` |
|        - | 2434 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - | 2435 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - | 2436 | ` *` |
|        - | 2437 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - | 2438 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - | 2439 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - | 2440 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - | 2441 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - | 2442 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - | 2443 | ` *` |
|        - | 2444 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - | 2445 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - | 2446 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - | 2447 | ` * exception/error-handler state, the reference table and every object/array` |
|        - | 2448 | ` * reserved during the run.` |
|        - | 2449 | ` *` |
|        - | 2450 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - | 2451 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - | 2452 | ` * global-scope destructors never fired.` |
|        - | 2453 | ` */` |
|        8 | 2454 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 | 2455 | `{` |
|        - | 2456 | `	sxu32 nWater,n;` |
|        8 | 2457 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 | 2458 | `		return SXERR_CORRUPT;` |
|        - | 2459 | `	}` |
|        8 | 2460 | `	nWater = pVm->nSuperBaseline;` |
|        - | 2461 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - | 2462 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        8 | 2463 | `	pVm->pGlobal = 0;` |
|        - | 2464 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|        - | 2465 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|        - | 2466 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|        - | 2467 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|        - | 2468 | `	 * object); unref'ing here would race the teardown below. */` |
|        8 | 2469 | `	pVm->pClosureThis = 0;` |
|        8 | 2470 | `	pVm->pClosureScope = 0;` |
|        - | 2471 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - | 2472 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - | 2473 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - | 2474 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        8 | 2475 | `	pVm->bInReset = 1;` |
|        - | 2476 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        8 | 2477 | `	VmResetRefTable(&(*pVm));` |
|        - | 2478 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - | 2479 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - | 2480 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - | 2481 | `	 * re-run define() overwrites the value in place). */` |
|        8 | 2482 | `	VmResetFunctionState(&(*pVm));` |
|        - | 2483 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - | 2484 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      344 | 2485 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      336 | 2486 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      336 | 2487 | `		if( pObj ){` |
|      336 | 2488 | `			PH7_MemObjRelease(pObj);` |
|      168 | 2489 | `		}` |
|      168 | 2490 | `	}` |
|        - | 2491 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - | 2492 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        8 | 2493 | `	VmResetTypedSlots(&(*pVm));` |
|        - | 2494 | `	/* (5) Unwind any active frames back to none. */` |
|       16 | 2495 | `	while( pVm->pFrame ){` |
|        8 | 2496 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 2497 | `	}` |
|        - | 2498 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        8 | 2499 | `	pVm->bInReset = 0;` |
|        - | 2500 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - | 2501 | `	 * slots (their indices no longer exist). */` |
|        8 | 2502 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        8 | 2503 | `	SySetReset(&pVm->aFreeObj);` |
|        - | 2504 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        8 | 2505 | `	SyHashRelease(&pVm->hSuper);` |
|        8 | 2506 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - | 2507 | `	/* (8) Drain remaining per-exec containers. */` |
|        8 | 2508 | `	SySetReset(&pVm->aSelf);` |
|        - | 2509 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - | 2510 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - | 2511 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        8 | 2512 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 | 2513 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 | 2514 | `		if( pCB ){` |
|        - | 2515 | `			int iArg;` |
|      ! 0 | 2516 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2517 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 | 2518 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 | 2519 | `			}` |
|      ! 0 | 2520 | `		}` |
|      ! 0 | 2521 | `	}` |
|        8 | 2522 | `	SySetReset(&pVm->aShutdown);` |
|        - | 2523 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|        - | 2524 | `	 * aborted program can leave entries behind). */` |
|        8 | 2525 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        8 | 2526 | `	SySetReset(&pVm->aException);` |
|        8 | 2527 | `	SySetReset(&pVm->aFinallyAction);` |
|        8 | 2528 | `	pVm->pPendingException = 0;` |
|        8 | 2529 | `	pVm->pInflightException = 0;` |
|        8 | 2530 | `	pVm->nInflightExcBase = 0;` |
|        8 | 2531 | `	pVm->pResumeFrame = 0;` |
|        8 | 2532 | `	pVm->iResumePc = 0;` |
|        8 | 2533 | `	pVm->pResumeInstr = 0;` |
|        8 | 2534 | `	pVm->iResumeStackDepth = 0;` |
|        8 | 2535 | `	pVm->nBoundaryRc = 0;` |
|        8 | 2536 | `	pVm->pConstEvalClass = 0;` |
|        8 | 2537 | `	pVm->nConstEvalDepth = 0;` |
|        8 | 2538 | `	pVm->pConstCycleAttr = 0;` |
|        8 | 2539 | `	pVm->pConstCycleClass = 0;` |
|        8 | 2540 | `	SySetReset(&pVm->aMagicGuard);` |
|        - | 2541 | `	{` |
|        - | 2542 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|        - | 2543 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        8 | 2544 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        8 | 2545 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|        - | 2546 | `		sxu32 iRmw;` |
|        8 | 2547 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|      ! 0 | 2548 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|      ! 0 | 2549 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|      ! 0 | 2550 | `		}` |
|        8 | 2551 | `		SySetReset(&pVm->aHookRmw);` |
|        - | 2552 | `	}` |
|        8 | 2553 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 2554 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 2555 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 2556 | `	}` |
|        8 | 2557 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        8 | 2558 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 2559 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 2560 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 2561 | `	}` |
|        8 | 2562 | `	pVm->pHookSetAttr = 0;` |
|        8 | 2563 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        8 | 2564 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 2565 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 2566 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 2567 | `	}` |
|        8 | 2568 | `	pVm->pMagicCallClass = 0;` |
|        8 | 2569 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        8 | 2570 | `	pVm->nExceptDepth = 0;` |
|        - | 2571 | `	/* spl_autoload_register() callbacks are per request */` |
|        8 | 2572 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 | 2573 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 | 2574 | `		if( pCB ){` |
|      ! 0 | 2575 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2576 | `		}` |
|      ! 0 | 2577 | `	}` |
|        8 | 2578 | `	SySetReset(&pVm->aAutoload);` |
|        - | 2579 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - | 2580 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        8 | 2581 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 | 2582 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 | 2583 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 | 2584 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      ! 0 | 2585 | `	}` |
|        - | 2586 | `	/* Output buffers */` |
|        8 | 2587 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 | 2588 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 | 2589 | `		if( pOb ){` |
|      ! 0 | 2590 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 | 2591 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 | 2592 | `		}` |
|      ! 0 | 2593 | `	}` |
|        8 | 2594 | `	SySetReset(&pVm->aOB);` |
|        8 | 2595 | `	pVm->nObDepth = 0;` |
|        - | 2596 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - | 2597 | `	{` |
|        8 | 2598 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        8 | 2599 | `		if( rc == SXRET_OK ){` |
|        8 | 2600 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        4 | 2601 | `		}` |
|        8 | 2602 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2603 | `			return rc;` |
|        - | 2604 | `		}` |
|        - | 2605 | `	}` |
|        - | 2606 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|        - | 2607 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|        - | 2608 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|        - | 2609 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|        - | 2610 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|        - | 2611 | `	{` |
|        - | 2612 | `		SyHashEntry *pEntry;` |
|        8 | 2613 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1084 | 2614 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1076 | 2615 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 2616 | `			ph7_class_attr *pAttr;` |
|        - | 2617 | `			SyHashEntry *pAttrEntry;` |
|     1076 | 2618 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     6210 | 2619 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     4596 | 2620 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     4596 | 2621 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     1340 | 2622 | `					pAttr->nIdx = SXU32_HIGH;` |
|     1340 | 2623 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      670 | 2624 | `				}` |
|      ! 0 | 2625 | `			}` |
|      ! 0 | 2626 | `		}` |
|        8 | 2627 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1084 | 2628 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1076 | 2629 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     1076 | 2630 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2631 | `				return rc;` |
|        - | 2632 | `			}` |
|      ! 0 | 2633 | `		}` |
|        - | 2634 | `	}` |
|        - | 2635 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        8 | 2636 | `	SyBlobReset(&pVm->sConsumer);` |
|        8 | 2637 | `	pVm->nOutputLen = 0;` |
|        8 | 2638 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        8 | 2639 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        8 | 2640 | `	pVm->iResponseStatus = 200;` |
|        8 | 2641 | `	pVm->bHeadersSent = 0;` |
|        8 | 2642 | `	pVm->bHttpContext = 0;` |
|        8 | 2643 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        8 | 2644 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        8 | 2645 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        8 | 2646 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        8 | 2647 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        8 | 2648 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 2649 | `#ifdef PH7_ENABLE_PCRE` |
|        8 | 2650 | `	pVm->iPcreLastError = 0;` |
|        - | 2651 | `#endif` |
|        - | 2652 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2653 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        8 | 2654 | `	PH7_LibxmlVmReset(&(*pVm));` |
|        - | 2655 | `#endif` |
|        8 | 2656 | `	pVm->iCmpCallbackExc = 0;` |
|        8 | 2657 | `	pVm->bHaltRequested = 0;` |
|        8 | 2658 | `	pVm->iExitStatus = 0;` |
|        8 | 2659 | `	pVm->nSpreadCallBase = 0;` |
|        8 | 2660 | `	VmSpreadCaptureReset(pVm);` |
|        8 | 2661 | `	pVm->nRecursionDepth = 0;` |
|        8 | 2662 | `	pVm->pActiveCtx = 0;` |
|        8 | 2663 | `	pVm->pCoalesceObj = 0;` |
|        8 | 2664 | `	pVm->bCoalesceArmed = 0;` |
|        8 | 2665 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - | 2666 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        8 | 2667 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2668 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|        - | 2669 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        8 | 2670 | `	pVm->nNextObjId = 1;` |
|        - | 2671 | `	/* Set the ready flag */` |
|        8 | 2672 | `	pVm->nMagic = PH7_VM_RUN;` |
|        8 | 2673 | `	return SXRET_OK;` |
|        4 | 2674 | `}` |
|        - | 2675 | `/*` |
|        - | 2676 | ` * Release a Virtual Machine.` |
|        - | 2677 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - | 2678 | ` */` |
|     3412 | 2679 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 | 2680 | `{` |
|        - | 2681 | `	/* Set the stale magic number */` |
|     3417 | 2682 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - | 2683 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2684 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|        - | 2685 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|     3417 | 2686 | `	PH7_LibxmlVmRelease(pVm);` |
|        - | 2687 | `#endif` |
|        - | 2688 | `	/* Release the private memory subsystem */` |
|     3417 | 2689 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     3417 | 2690 | `	return SXRET_OK;` |
|        5 | 2691 | `}` |
|        - | 2692 | `/*` |
|        - | 2693 | ` * Initialize a foreign function call context.` |
|        - | 2694 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - | 2695 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - | 2696 | ` * functions.` |
|        - | 2697 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - | 2698 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - | 2699 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - | 2700 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - | 2701 | ` */` |
|   930916 | 2702 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|        - | 2703 | `	ph7_context *pOut,    /* Call Context */` |
|        - | 2704 | `	ph7_vm *pVm,          /* Target VM */` |
|        - | 2705 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - | 2706 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - | 2707 | `	sxi32 iFlags          /* Control flags */` |
|        - | 2708 | `	)` |
|        5 | 2709 | `{` |
|   930921 | 2710 | `	pOut->pFunc = pFunc;` |
|   930921 | 2711 | `	pOut->pVm   = pVm;` |
|   930921 | 2712 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   930921 | 2713 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - | 2714 | `	/* Assume a null return value */` |
|   930921 | 2715 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   930921 | 2716 | `	pOut->pRet = pRet;` |
|   930921 | 2717 | `	pOut->iFlags = iFlags;` |
|   930921 | 2718 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|   930921 | 2719 | `	return SXRET_OK;` |
|        5 | 2720 | `}` |
|        - | 2721 | `/*` |
|        - | 2722 | ` * Release a foreign function call context and cleanup the mess` |
|        - | 2723 | ` * left behind.` |
|        - | 2724 | ` */` |
|   930916 | 2725 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 | 2726 | `{` |
|        - | 2727 | `	sxu32 n;` |
|   930921 | 2728 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    11655 | 2729 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    68212 | 2730 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    56562 | 2731 | `			if( apObj[n] == 0 ){` |
|        - | 2732 | `				/* Already released */` |
|      682 | 2733 | `				continue;` |
|        - | 2734 | `			}` |
|    55885 | 2735 | `			PH7_MemObjRelease(apObj[n]);` |
|    55885 | 2736 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    27945 | 2737 | `		}` |
|    11655 | 2738 | `		SySetRelease(&pCtx->sVar);` |
|     5825 | 2739 | `	}` |
|   930921 | 2740 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - | 2741 | `		ph7_aux_data *aAux;` |
|        - | 2742 | `		void *pChunk;` |
|        - | 2743 | `		/* Automatic release of dynamically allocated chunk` |
|        - | 2744 | `		 * using [ph7_context_alloc_chunk()].` |
|        - | 2745 | `		 */` |
|      115 | 2746 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      337 | 2747 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      227 | 2748 | `			pChunk = aAux[n].pAuxData;` |
|        - | 2749 | `			/* Release the chunk */` |
|      227 | 2750 | `			if( pChunk ){` |
|      227 | 2751 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      111 | 2752 | `			}` |
|      116 | 2753 | `		}` |
|      115 | 2754 | `		SySetRelease(&pCtx->sChunk);` |
|       55 | 2755 | `	}` |
|   930921 | 2756 | `}` |
|        - | 2757 | `/*` |
|        - | 2758 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - | 2759 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - | 2760 | ` */` |
|      677 | 2761 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - | 2762 | `	ph7_context *pCtx, /* Call context */` |
|        - | 2763 | `	ph7_value *pValue  /* Release this value */` |
|        - | 2764 | `	)` |
|        5 | 2765 | `{` |
|      682 | 2766 | `	if( pValue == 0 ){` |
|        - | 2767 | `		/* NULL value is a harmless operation */` |
|      ! 0 | 2768 | `		return;` |
|        - | 2769 | `	}` |
|      682 | 2770 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      682 | 2771 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - | 2772 | `		sxu32 n;` |
|     1536 | 2773 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1536 | 2774 | `			if( apObj[n] == pValue ){` |
|      682 | 2775 | `				PH7_MemObjRelease(pValue);` |
|      682 | 2776 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - | 2777 | `				/* Mark as released */` |
|      682 | 2778 | `				apObj[n] = 0;` |
|      682 | 2779 | `				break;` |
|        - | 2780 | `			}` |
|      431 | 2781 | `		}` |
|      338 | 2782 | `	}` |
|      343 | 2783 | `}` |
|        - | 2784 | `/*` |
|        - | 2785 | ` * Pop and release as many memory object from the operand stack.` |
|        - | 2786 | ` */` |
|  5657027 | 2787 | `PH7_PRIVATE void VmPopOperand(` |
|        - | 2788 | `	ph7_value **ppTos, /* Operand stack */` |
|        - | 2789 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - | 2790 | `	)` |
|        5 | 2791 | `{` |
|  5657032 | 2792 | `	ph7_value *pTos = *ppTos;` |
| 12040382 | 2793 | `	while( nPop > 0 ){` |
|  6383355 | 2794 | `		PH7_MemObjRelease(pTos);` |
|  6383355 | 2795 | `		pTos--;` |
|  6383355 | 2796 | `		nPop--;` |
|        5 | 2797 | `	}` |
|        - | 2798 | `	/* Top of the stack */` |
|  5657032 | 2799 | `	*ppTos = pTos;` |
|  5657032 | 2800 | `}` |
|        - | 2801 | `/*` |
|        - | 2802 | ` * Reserve a memory object.` |
|        - | 2803 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 2804 | ` */` |
|  4362883 | 2805 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 | 2806 | `{` |
|  4362888 | 2807 | `	ph7_value *pObj = 0;` |
|        - | 2808 | `	VmSlot *pSlot;` |
|        - | 2809 | `	sxu32 nIdx;` |
|        - | 2810 | `	/* Check for a free slot */` |
|  4362888 | 2811 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  4362888 | 2812 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  4362888 | 2813 | `	if( pSlot ){` |
|  2125430 | 2814 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  2125430 | 2815 | `		nIdx = pSlot->nIdx;` |
|  1063321 | 2816 | `	}` |
|  4362888 | 2817 | `	if( pObj == 0 ){` |
|        - | 2818 | `		/* Reserve a new memory object */` |
|  2237463 | 2819 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2237463 | 2820 | `		if( pObj == 0 ){` |
|      ! 0 | 2821 | `			return 0;` |
|        - | 2822 | `		}` |
|  1118729 | 2823 | `	}` |
|        - | 2824 | `	/* Set a null default value */` |
|  4362888 | 2825 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  4362888 | 2826 | `	pObj->nIdx = nIdx;` |
|  4362888 | 2827 | `	return pObj;` |
|  2182055 | 2828 | `}` |
|        - | 2829 | `/*` |
|        - | 2830 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - | 2831 | ` */` |
|    46300 | 2832 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|        - | 2833 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - | 2834 | `	const char *zKey,  /* Entry key */` |
|        - | 2835 | `	sxu32 nByte,       /* Key length */` |
|        - | 2836 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - | 2837 | `	)` |
|        5 | 2838 | `{` |
|        - | 2839 | `	ph7_value sKey;` |
|        - | 2840 | `	sxi32 rc;` |
|    46305 | 2841 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    46305 | 2842 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - | 2843 | `	/* Perform the insertion */` |
|    46305 | 2844 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    46305 | 2845 | `	PH7_MemObjRelease(&sKey);` |
|    46305 | 2846 | `	return rc;` |
|        5 | 2847 | `}` |
|        - | 2848 | `/*` |
|        - | 2849 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|        - | 2850 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|        - | 2851 | ` * key must create a real global variable — linked into the bottom frame's` |
|        - | 2852 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|        - | 2853 | ` * variable created by top-level code — so later reads and writes alias one` |
|        - | 2854 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|        - | 2855 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|        - | 2856 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|        - | 2857 | ` *     superglobal in place.` |
|        - | 2858 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|        - | 2859 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING` |
|        - | 2860 | ` *     name is rejected with the engine's usual "already exists" diagnostic` |
|        - | 2861 | ` *     (the same limitation OP_STORE_REF has for plain variables).` |
|        - | 2862 | ` */` |
|      142 | 2863 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|        2 | 2864 | `{` |
|      144 | 2865 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 2866 | `	SyHashEntry *pEntry;` |
|        - | 2867 | `	ph7_value *pObj;` |
|        - | 2868 | `	char *zDup;` |
|        - | 2869 | `	sxu32 nIdx;` |
|        - | 2870 | `	sxi32 rc;` |
|        - | 2871 | `	/* Walk down to the global frame */` |
|      148 | 2872 | `	while( pFrame->pParent ){` |
|        5 | 2873 | `		pFrame = pFrame->pParent;` |
|        1 | 2874 | `	}` |
|        - | 2875 | `	/* An existing global (or superglobal) is overwritten in place */` |
|      144 | 2876 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      144 | 2877 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|        - | 2878 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|        - | 2879 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|        - | 2880 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|        - | 2881 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|        - | 2882 | `		 * of $GLOBALS itself). */` |
|        5 | 2883 | `		pEntry = 0;` |
|        2 | 2884 | `	}` |
|      144 | 2885 | `	if( pEntry == 0 ){` |
|      144 | 2886 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       71 | 2887 | `	}` |
|      144 | 2888 | `	if( pEntry ){` |
|        3 | 2889 | `		if( nRefIdx != SXU32_HIGH ){` |
|        - | 2890 | `			SyString sName;` |
|      ! 0 | 2891 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|      ! 0 | 2892 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 | 2893 | `			return SXRET_OK;` |
|        - | 2894 | `		}` |
|        3 | 2895 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|        3 | 2896 | `		if( pObj == 0 ){` |
|      ! 0 | 2897 | `			return SXERR_NOTFOUND;` |
|        - | 2898 | `		}` |
|        3 | 2899 | `		if( pValue ){` |
|        3 | 2900 | `			PH7_MemObjStore(pValue,pObj);` |
|        2 | 2901 | `		}else{` |
|      ! 0 | 2902 | `			PH7_MemObjToNull(pObj);` |
|        - | 2903 | `		}` |
|        3 | 2904 | `		return SXRET_OK;` |
|        - | 2905 | `	}` |
|      142 | 2906 | `	if( nRefIdx == SXU32_HIGH ){` |
|        - | 2907 | `		/* Reserve a fresh slot for the new global */` |
|      140 | 2908 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|      140 | 2909 | `		if( pObj == 0 ){` |
|      ! 0 | 2910 | `			return SXERR_MEM;` |
|        - | 2911 | `		}` |
|      140 | 2912 | `		nIdx = pObj->nIdx;` |
|       71 | 2913 | `	}else{` |
|        - | 2914 | `		/* Reference assignment: bind the name to the existing slot */` |
|        3 | 2915 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|        3 | 2916 | `		if( pObj == 0 ){` |
|      ! 0 | 2917 | `			return SXERR_NOTFOUND;` |
|        - | 2918 | `		}` |
|        3 | 2919 | `		nIdx = nRefIdx;` |
|        - | 2920 | `	}` |
|      142 | 2921 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|      142 | 2922 | `	if( zDup == 0 ){` |
|      ! 0 | 2923 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2924 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|        - | 2925 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|        - | 2926 | `			VmSlot sFree;` |
|      ! 0 | 2927 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 2928 | `			sFree.pUserData = 0;` |
|      ! 0 | 2929 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 2930 | `		}` |
|      ! 0 | 2931 | `		return SXERR_MEM;` |
|        - | 2932 | `	}` |
|      142 | 2933 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|      142 | 2934 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2935 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2936 | `			VmSlot sFree;` |
|      ! 0 | 2937 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 2938 | `			sFree.pUserData = 0;` |
|      ! 0 | 2939 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 2940 | `		}` |
|      ! 0 | 2941 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 2942 | `		return rc;` |
|        - | 2943 | `	}` |
|        - | 2944 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|      142 | 2945 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|      142 | 2946 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      142 | 2947 | `	if( nRefIdx == SXU32_HIGH ){` |
|      140 | 2948 | `		pObj->nIdx = nIdx;` |
|      140 | 2949 | `		if( pValue ){` |
|      140 | 2950 | `			PH7_MemObjStore(pValue,pObj);` |
|       69 | 2951 | `		}` |
|       69 | 2952 | `	}` |
|      142 | 2953 | `	return SXRET_OK;` |
|       73 | 2954 | `}` |
|        - | 2955 | `/*` |
|        - | 2956 | ` * Extract a variable value from the top active VM frame.` |
|        - | 2957 | ` * Return a pointer to the variable value on success.` |
|        - | 2958 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - | 2959 | ` */` |
|  5401462 | 2960 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|        - | 2961 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 2962 | `	const SyString *pName, /* Variable name */` |
|        - | 2963 | `	int bDup,              /* True to duplicate variable name */` |
|        - | 2964 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - | 2965 | `	)` |
|        5 | 2966 | `{` |
|  5401467 | 2967 | `	int bNullify = FALSE;` |
|        - | 2968 | `	SyHashEntry *pEntry;` |
|        - | 2969 | `	VmFrame *pFrame;` |
|        - | 2970 | `	ph7_value *pObj;` |
|        - | 2971 | `	sxu32 nIdx;` |
|        - | 2972 | `	sxi32 rc;` |
|        - | 2973 | `	/* Point to the top active frame */` |
|  5401467 | 2974 | `	pFrame = pVm->pFrame;` |
|  5401467 | 2975 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 2976 | `	/* Perform the lookup */` |
|  5401467 | 2977 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - | 2978 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 | 2979 | `		pName = &sAnnon;` |
|        - | 2980 | `		/* Always nullify the object */` |
|      ! 0 | 2981 | `		bNullify = TRUE;` |
|      ! 0 | 2982 | `		bDup = FALSE;` |
|      ! 0 | 2983 | `	}` |
|        - | 2984 | `	/* Check the superglobals table first */` |
|  5401467 | 2985 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  5401467 | 2986 | `	if( pEntry == 0 ){` |
|        - | 2987 | `		/* Query the top active frame */` |
|  5401065 | 2988 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  5401065 | 2989 | `		if( pEntry == 0 ){` |
|   494997 | 2990 | `			char *zName = (char *)pName->zString;` |
|        - | 2991 | `			VmSlot sLocal;` |
|   494997 | 2992 | `			if( !bCreate ){` |
|        - | 2993 | `				/* Do not create the variable,return NULL instead */` |
|     1117 | 2994 | `				return 0;` |
|        - | 2995 | `			}` |
|        - | 2996 | `			/* No such variable,automatically create a new one and install` |
|        - | 2997 | `			 * it in the current frame.` |
|        - | 2998 | `			 */` |
|   493885 | 2999 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   493885 | 3000 | `			if( pObj == 0 ){` |
|      ! 0 | 3001 | `				return 0;` |
|        - | 3002 | `			}` |
|   493885 | 3003 | `			nIdx = pObj->nIdx;` |
|   493885 | 3004 | `			if( bDup ){` |
|        - | 3005 | `				/* Duplicate name */` |
|      487 | 3006 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      487 | 3007 | `				if( zName == 0 ){` |
|      ! 0 | 3008 | `					return 0;` |
|        - | 3009 | `				}` |
|      242 | 3010 | `			}` |
|        - | 3011 | `			/* Link to the top active VM frame */` |
|   493885 | 3012 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   493885 | 3013 | `			if( rc != SXRET_OK ){` |
|        - | 3014 | `				/* Return the slot to the free pool */` |
|      ! 0 | 3015 | `				sLocal.nIdx = nIdx;` |
|      ! 0 | 3016 | `				sLocal.pUserData = 0;` |
|      ! 0 | 3017 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 | 3018 | `				return 0;` |
|        - | 3019 | `			}` |
|   493885 | 3020 | `			if( pFrame->pParent != 0 ){` |
|        - | 3021 | `				/* Local variable */` |
|   485367 | 3022 | `				sLocal.nIdx = nIdx;` |
|   485367 | 3023 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   243295 | 3024 | `			}else{` |
|        - | 3025 | `				/* Register in the $GLOBALS array */` |
|     8523 | 3026 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - | 3027 | `			}` |
|        - | 3028 | `			/* Install in the reference table */` |
|   493885 | 3029 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - | 3030 | `			/* Save object index */` |
|   493885 | 3031 | `			pObj->nIdx = nIdx;` |
|   247554 | 3032 | `		}else{` |
|        - | 3033 | `			/* Extract variable contents */` |
|  4906073 | 3034 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  4906073 | 3035 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  4906073 | 3036 | `			if( bNullify && pObj ){` |
|      ! 0 | 3037 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 | 3038 | `			}` |
|        - | 3039 | `		}` |
|  2702520 | 3040 | `	}else{` |
|        - | 3041 | `		/* Superglobal */` |
|      407 | 3042 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      407 | 3043 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 3044 | `	}` |
|  5400355 | 3045 | `	return pObj;` |
|  2703277 | 3046 | `}` |
|        - | 3047 | `/*` |
|        - | 3048 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - | 3049 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - | 3050 | ` */` |
|    31116 | 3051 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - | 3052 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3053 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - | 3054 | `	sxu32 nByte        /* zName length */` |
|        - | 3055 | `	)` |
|        5 | 3056 | `{` |
|        - | 3057 | `	SyHashEntry *pEntry;` |
|        - | 3058 | `	ph7_value *pValue;` |
|        - | 3059 | `	sxu32 nIdx;` |
|        - | 3060 | `	/* Query the superglobal table */` |
|    31121 | 3061 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    31121 | 3062 | `	if( pEntry == 0 ){` |
|        - | 3063 | `		/* No such entry */` |
|      ! 0 | 3064 | `		return 0;` |
|        - | 3065 | `	}` |
|        - | 3066 | `	/* Extract the superglobal index in the global object pool */` |
|    31121 | 3067 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3068 | `	/* Extract the variable value  */` |
|    31121 | 3069 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    31121 | 3070 | `	return pValue;` |
|    15563 | 3071 | `}` |
|        - | 3072 | `/*` |
|        - | 3073 | ` * Perform a raw hashmap insertion.` |
|        - | 3074 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - | 3075 | ` */` |
|    24378 | 3076 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - | 3077 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - | 3078 | `	const char *zKey,   /* Entry key */` |
|        - | 3079 | `	int nKeylen,        /* zKey length*/` |
|        - | 3080 | `	const char *zData,  /* Entry data */` |
|        - | 3081 | `	int nLen            /* zData length */` |
|        - | 3082 | `	)` |
|        5 | 3083 | `{` |
|        - | 3084 | `	ph7_value sKey,sValue;` |
|        - | 3085 | `	sxi32 rc;` |
|    24383 | 3086 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    24383 | 3087 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|    24383 | 3088 | `	if( zKey ){` |
|    20965 | 3089 | `		if( nKeylen < 0 ){` |
|    20861 | 3090 | `			nKeylen = (int)SyStrlen(zKey);` |
|    10428 | 3091 | `		}` |
|    20965 | 3092 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|    10480 | 3093 | `	}` |
|    24383 | 3094 | `	if( zData ){` |
|    24383 | 3095 | `		if( nLen < 0 ){` |
|        - | 3096 | `			/* Compute length automatically */` |
|    13835 | 3097 | `			nLen = (int)SyStrlen(zData);` |
|     6915 | 3098 | `		}` |
|    24383 | 3099 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|    12189 | 3100 | `	}` |
|        - | 3101 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|        - | 3102 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|        - | 3103 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|        - | 3104 | `	 * every argument under "". */` |
|    24383 | 3105 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|    24383 | 3106 | `	PH7_MemObjRelease(&sKey);` |
|    24383 | 3107 | `	PH7_MemObjRelease(&sValue);` |
|    24383 | 3108 | `	return rc;` |
|        5 | 3109 | `}` |
|        - | 3110 | `/*` |
|        - | 3111 | ` * Configure a working virtual machine instance.` |
|        - | 3112 | ` *` |
|        - | 3113 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - | 3114 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - | 3115 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - | 3116 | ` * The second argument to this function is an integer configuration option` |
|        - | 3117 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - | 3118 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - | 3119 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - | 3120 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - | 3121 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - | 3122 | ` */` |
|    89342 | 3123 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - | 3124 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 3125 | `	sxi32 nOp,   /* Configuration verb */` |
|        - | 3126 | `	va_list ap   /* Subsequent option arguments */` |
|        - | 3127 | `	)` |
|        5 | 3128 | `{` |
|    89347 | 3129 | `	sxi32 rc = SXRET_OK;` |
|    89347 | 3130 | `	switch(nOp){` |
|     1693 | 3131 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     3391 | 3132 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     3391 | 3133 | `		void *pUserData = va_arg(ap,void *);` |
|        - | 3134 | `		/* VM output consumer callback */` |
|        - | 3135 | `#ifdef UNTRUST` |
|        - | 3136 | `		if( xConsumer == 0 ){` |
|        - | 3137 | `			rc = SXERR_CORRUPT;` |
|        - | 3138 | `			break;` |
|        - | 3139 | `		}` |
|        - | 3140 | `#endif` |
|        - | 3141 | `		/* Install the output consumer */` |
|     3391 | 3142 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     3391 | 3143 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     3391 | 3144 | `		break;` |
|        - | 3145 | `							   }` |
|     1706 | 3146 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - | 3147 | `		/* Import path */` |
|        - | 3148 | `		  const char *zPath;` |
|        - | 3149 | `		  SyString sPath;` |
|     3417 | 3150 | `		  zPath = va_arg(ap,const char *);` |
|        - | 3151 | `#if defined(UNTRUST)` |
|        - | 3152 | `		  if( zPath == 0 ){` |
|        - | 3153 | `			  rc = SXERR_EMPTY;` |
|        - | 3154 | `			  break;` |
|        - | 3155 | `		  }` |
|        - | 3156 | `#endif` |
|     3417 | 3157 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - | 3158 | `		  /* Remove trailing slashes and backslashes */` |
|        - | 3159 | `#ifdef __WINNT__` |
|        5 | 3160 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - | 3161 | `#endif` |
|     6829 | 3162 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - | 3163 | `		  /* Remove leading and trailing white spaces */` |
|     3417 | 3164 | `		  SyStringFullTrim(&sPath);` |
|     3417 | 3165 | `		  if( sPath.nByte > 0 ){` |
|        - | 3166 | `			  /* Store the path in the corresponding conatiner */` |
|     3417 | 3167 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1706 | 3168 | `		  }` |
|     3417 | 3169 | `		  break;` |
|        - | 3170 | `									 }` |
|     1709 | 3171 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - | 3172 | `		/* Run-Time Error report */` |
|     3423 | 3173 | `		pVm->bErrReport = 1;` |
|     3423 | 3174 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|     3423 | 3175 | `		break;` |
|        2 | 3176 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - | 3177 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|        - | 3178 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|        - | 3179 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|        - | 3180 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|        - | 3181 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|        - | 3182 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|        - | 3183 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|        - | 3184 | `		 * would otherwise read as an enormous positive cap). */` |
|        5 | 3185 | `		int nDepth = va_arg(ap,int);` |
|        5 | 3186 | `		if( nDepth >= 0 ){` |
|        5 | 3187 | `			pVm->nMaxDepth = nDepth;` |
|        2 | 3188 | `		}` |
|        5 | 3189 | `		break;` |
|        - | 3190 | `									   }` |
|        5 | 3191 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|        - | 3192 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|        - | 3193 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|        - | 3194 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|        - | 3195 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|        - | 3196 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|        - | 3197 | `		 * so it is rejected as a footgun). */` |
|       12 | 3198 | `		int nDepth = va_arg(ap,int);` |
|       12 | 3199 | `		if( nDepth > 1 ){` |
|       12 | 3200 | `			pVm->nMaxNativeDepth = nDepth;` |
|        5 | 3201 | `		}` |
|       12 | 3202 | `		break;` |
|        - | 3203 | `									   }` |
|      ! 0 | 3204 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - | 3205 | `		/* VM output length in bytes */` |
|      ! 0 | 3206 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - | 3207 | `#ifdef UNTRUST` |
|        - | 3208 | `		if( pOut == 0 ){` |
|        - | 3209 | `			rc = SXERR_CORRUPT;` |
|        - | 3210 | `			break;` |
|        - | 3211 | `		}` |
|        - | 3212 | `#endif` |
|      ! 0 | 3213 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 | 3214 | `		break;` |
|        - | 3215 | `							   }` |
|        - | 3216 |  |
|    18803 | 3217 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - | 3218 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - | 3219 | `		/* Create a new superglobal/global variable */` |
|    37611 | 3220 | `		const char *zName = va_arg(ap,const char *);` |
|    37611 | 3221 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - | 3222 | `		SyHashEntry *pEntry;` |
|        - | 3223 | `		ph7_value *pObj;` |
|        - | 3224 | `		sxu32 nByte;` |
|        - | 3225 | `		sxu32 nIdx;` |
|        - | 3226 | `#ifdef UNTRUST` |
|        - | 3227 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - | 3228 | `			rc = SXERR_CORRUPT;` |
|        - | 3229 | `			break;` |
|        - | 3230 | `		}` |
|        - | 3231 | `#endif` |
|    37611 | 3232 | `		nByte = SyStrlen(zName);` |
|    37611 | 3233 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3234 | `			/* Check if the superglobal is already installed */` |
|    34225 | 3235 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    17115 | 3236 | `		}else{` |
|        - | 3237 | `			/* Query the top active VM frame */` |
|     3391 | 3238 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - | 3239 | `		}` |
|    37611 | 3240 | `		if( pEntry ){` |
|        - | 3241 | `			/* Variable already installed */` |
|      ! 0 | 3242 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3243 | `			/* Extract contents */` |
|      ! 0 | 3244 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 | 3245 | `			if( pObj ){` |
|        - | 3246 | `				/* Overwrite old contents */` |
|      ! 0 | 3247 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 | 3248 | `			}` |
|      ! 0 | 3249 | `		}else{` |
|        - | 3250 | `			/* Install a new variable */` |
|    37611 | 3251 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    37611 | 3252 | `			if( pObj == 0 ){` |
|      ! 0 | 3253 | `				rc = SXERR_MEM;` |
|      ! 0 | 3254 | `				break;` |
|        - | 3255 | `			}` |
|    37611 | 3256 | `			nIdx = pObj->nIdx;` |
|        - | 3257 | `			/* Copy value */` |
|    37611 | 3258 | `			PH7_MemObjStore(pValue,pObj);` |
|    37611 | 3259 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3260 | `				/* Install the superglobal */` |
|    34225 | 3261 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    17115 | 3262 | `			}else{` |
|        - | 3263 | `				/* Install in the current frame */` |
|     3391 | 3264 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - | 3265 | `			}` |
|    37611 | 3266 | `			if( rc == SXRET_OK ){` |
|        - | 3267 | `				SyHashEntry *pRef;` |
|    37611 | 3268 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    34225 | 3269 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    17115 | 3270 | `				}else{` |
|     3391 | 3271 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - | 3272 | `				}` |
|        - | 3273 | `				/* Install in the reference table */` |
|    37611 | 3274 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    37611 | 3275 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - | 3276 | `					/* Register in the $GLOBALS array */` |
|    37611 | 3277 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    18803 | 3278 | `				}` |
|    18803 | 3279 | `			}` |
|        - | 3280 | `		}` |
|    37611 | 3281 | `		break;` |
|        - | 3282 | `									}` |
|    10428 | 3283 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - | 3284 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - | 3285 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - | 3286 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - | 3287 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - | 3288 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - | 3289 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|    20861 | 3290 | `		const char *zKey   = va_arg(ap,const char *);` |
|    20861 | 3291 | `		const char *zValue = va_arg(ap,const char *);` |
|    20861 | 3292 | `		int nLen = va_arg(ap,int);` |
|        - | 3293 | `		ph7_hashmap *pMap;` |
|        - | 3294 | `		ph7_value *pValue;` |
|    20861 | 3295 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - | 3296 | `			/* Extract the $_ENV superglobal */` |
|        3 | 3297 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|    20860 | 3298 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - | 3299 | `			/* Extract the $_POST superglobal */` |
|      ! 0 | 3300 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|    20859 | 3301 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - | 3302 | `			/* Extract the $_GET superglobal */` |
|      ! 0 | 3303 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    20859 | 3304 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - | 3305 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 | 3306 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    20859 | 3307 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - | 3308 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 | 3309 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|    20859 | 3310 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - | 3311 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 | 3312 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 | 3313 | `		}else{` |
|        - | 3314 | `			/* Extract the $_SERVER superglobal */` |
|    20859 | 3315 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - | 3316 | `		}` |
|    20861 | 3317 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3318 | `			/* No such entry */` |
|      ! 0 | 3319 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3320 | `			break;` |
|        - | 3321 | `		}` |
|        - | 3322 | `		/* Point to the hashmap */` |
|    20861 | 3323 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3324 | `		/* Perform the insertion */` |
|    20861 | 3325 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|    20861 | 3326 | `		break;` |
|        - | 3327 | `								   }` |
|     1710 | 3328 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - | 3329 | `		/* Script arguments */` |
|     3425 | 3330 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3331 | `		ph7_hashmap *pMap;` |
|        - | 3332 | `		ph7_value *pValue;` |
|        - | 3333 | `		sxu32 n;` |
|     3425 | 3334 | `		if( SX_EMPTY_STR(zValue) ){` |
|        2 | 3335 | `			rc = SXERR_EMPTY;` |
|        2 | 3336 | `			break;` |
|        - | 3337 | `		}` |
|        - | 3338 | `		/* Extract the $argv array */` |
|     3423 | 3339 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3423 | 3340 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3341 | `			/* No such entry */` |
|      ! 0 | 3342 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3343 | `			break;` |
|        - | 3344 | `		}` |
|        - | 3345 | `		/* Point to the hashmap */` |
|     3423 | 3346 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3347 | `		/* Perform the insertion */` |
|     3423 | 3348 | `		n = (sxu32)SyStrlen(zValue);` |
|     3423 | 3349 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|     3423 | 3350 | `		if( rc == SXRET_OK ){` |
|     3423 | 3351 | `			if( pMap->nEntry > 1 ){` |
|        - | 3352 | `				/* Append space separator first */` |
|       37 | 3353 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|       16 | 3354 | `			}` |
|     3423 | 3355 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|     1709 | 3356 | `		}` |
|     3423 | 3357 | `		break;` |
|        - | 3358 | `								  }` |
|     1693 | 3359 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|        - | 3360 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|        - | 3361 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|        - | 3362 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|        - | 3363 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|        - | 3364 | `		ph7_value *pArgv,*pServer;` |
|        - | 3365 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|        - | 3366 | `		ph7_value sArgvVal,sKey,sCount;` |
|     3391 | 3367 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3391 | 3368 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|     3386 | 3369 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|     3391 | 3370 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 3371 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3372 | `			break;` |
|        - | 3373 | `		}` |
|     3391 | 3374 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|     3391 | 3375 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|        - | 3376 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|     3391 | 3377 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|     3391 | 3378 | `		if( pDup == 0 ){` |
|      ! 0 | 3379 | `			rc = SXERR_MEM;` |
|      ! 0 | 3380 | `			break;` |
|        - | 3381 | `		}` |
|     3391 | 3382 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|     3391 | 3383 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|     3391 | 3384 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3391 | 3385 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|     3391 | 3386 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|     3391 | 3387 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|     3391 | 3388 | `		PH7_MemObjRelease(&sKey);` |
|        - | 3389 | `		/* $_SERVER['argc'] = count($argv). */` |
|     3391 | 3390 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|     3391 | 3391 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3391 | 3392 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|     3391 | 3393 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|     3391 | 3394 | `		PH7_MemObjRelease(&sCount);` |
|     3391 | 3395 | `		PH7_MemObjRelease(&sKey);` |
|     3391 | 3396 | `		rc = SXRET_OK;` |
|     3391 | 3397 | `		break;` |
|        - | 3398 | `								  }` |
|       29 | 3399 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|        - | 3400 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|        - | 3401 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|        - | 3402 | `		 * apply immediately so they take effect even if the script never` |
|        - | 3403 | `		 * touches the INI API. */` |
|       62 | 3404 | `		const char *zName = va_arg(ap,const char *);` |
|       62 | 3405 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3406 | `		VmIniEntry sEntry;` |
|        - | 3407 | `		char *zDupN,*zDupV;` |
|        - | 3408 | `		sxu32 nName,nValue;` |
|       62 | 3409 | `		if( SX_EMPTY_STR(zName) ){` |
|      ! 0 | 3410 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3411 | `			break;` |
|        - | 3412 | `		}` |
|       62 | 3413 | `		if( zValue == 0 ){` |
|      ! 0 | 3414 | `			zValue = "";` |
|      ! 0 | 3415 | `		}` |
|       62 | 3416 | `		nName = (sxu32)SyStrlen(zName);` |
|       62 | 3417 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       62 | 3418 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       62 | 3419 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       62 | 3420 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|      ! 0 | 3421 | `			rc = SXERR_MEM;` |
|      ! 0 | 3422 | `			break;` |
|        - | 3423 | `		}` |
|       62 | 3424 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       62 | 3425 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       62 | 3426 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       62 | 3427 | `		if( rc == SXRET_OK ){` |
|       58 | 3428 | `			if( nName == sizeof("error_reporting")-1` |
|       52 | 3429 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|        2 | 3430 | `				sxi64 iLevel = 0;` |
|        2 | 3431 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|        2 | 3432 | `				pVm->bErrReport = iLevel != 0;` |
|       57 | 3433 | `			}else if( nName == sizeof("date.timezone")-1` |
|       28 | 3434 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|      ! 0 | 3435 | `			 && nValue == 3` |
|        4 | 3436 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|      ! 0 | 3437 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|      ! 0 | 3438 | `				pVm->zDefTz[3] = 0;` |
|      ! 0 | 3439 | `				pVm->nDefTz = 3;` |
|       56 | 3440 | `			}else if( nName == sizeof("zend.assertions")-1` |
|       50 | 3441 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|        - | 3442 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|        - | 3443 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|        - | 3444 | `				 * effect even before the INI chunk is seeded. */` |
|       40 | 3445 | `				sxi64 iZend = 0;` |
|       40 | 3446 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|       40 | 3447 | `				if( iZend >= 1 ){` |
|       40 | 3448 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|       22 | 3449 | `				}else{` |
|      ! 0 | 3450 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|        - | 3451 | `				}` |
|       18 | 3452 | `			}` |
|       29 | 3453 | `		}` |
|       62 | 3454 | `		break;` |
|        - | 3455 | `								  }` |
|      ! 0 | 3456 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - | 3457 | `		/* error_log() consumer */` |
|      ! 0 | 3458 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 | 3459 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 | 3460 | `		break;` |
|        - | 3461 | `										}` |
|      ! 0 | 3462 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - | 3463 | `		/* Script return value */` |
|      ! 0 | 3464 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - | 3465 | `#ifdef UNTRUST` |
|        - | 3466 | `		if( ppValue == 0 ){` |
|        - | 3467 | `			rc = SXERR_CORRUPT;` |
|        - | 3468 | `			break;` |
|        - | 3469 | `		}` |
|        - | 3470 | `#endif` |
|      ! 0 | 3471 | `		*ppValue = &pVm->sExec;` |
|      ! 0 | 3472 | `		break;` |
|        - | 3473 | `								   }` |
|     6829 | 3474 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - | 3475 | `		/* Register an IO stream device */` |
|    13663 | 3476 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - | 3477 | `		/* Make sure we are dealing with a valid IO stream */` |
|    13658 | 3478 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|    13663 | 3479 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - | 3480 | `				/* Invalid stream */` |
|      ! 0 | 3481 | `				rc = SXERR_INVALID;` |
|      ! 0 | 3482 | `				break;` |
|        - | 3483 | `		}` |
|    13663 | 3484 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - | 3485 | `			/* Make the 'file://' stream the defaut stream device */` |
|     3419 | 3486 | `			pVm->pDefStream = pStream;` |
|     1707 | 3487 | `		}` |
|        - | 3488 | `		/* Insert in the appropriate container */` |
|    13663 | 3489 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|    13663 | 3490 | `		break;` |
|        - | 3491 | `								  }` |
|       16 | 3492 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - | 3493 | `		/* Point to the VM internal output consumer buffer */` |
|       32 | 3494 | `		const void **ppOut = va_arg(ap,const void **);` |
|       32 | 3495 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - | 3496 | `#ifdef UNTRUST` |
|        - | 3497 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - | 3498 | `			rc = SXERR_CORRUPT;` |
|        - | 3499 | `			break;` |
|        - | 3500 | `		}` |
|        - | 3501 | `#endif` |
|       32 | 3502 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       32 | 3503 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       32 | 3504 | `		break;` |
|        - | 3505 | `									   }` |
|       16 | 3506 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - | 3507 | `		/* Raw HTTP request*/` |
|       32 | 3508 | `		const char *zRequest = va_arg(ap,const char *);` |
|       32 | 3509 | `		int nByte = va_arg(ap,int);` |
|       32 | 3510 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 | 3511 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3512 | `			break;` |
|        - | 3513 | `		}` |
|       32 | 3514 | `		if( nByte < 0 ){` |
|        - | 3515 | `			/* Compute length automatically */` |
|      ! 0 | 3516 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 | 3517 | `		}` |
|        - | 3518 | `		/* Process the request */` |
|       32 | 3519 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - | 3520 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       32 | 3521 | `		if( rc == SXRET_OK ){` |
|       30 | 3522 | `			pVm->bHttpContext = 1;` |
|       15 | 3523 | `		}` |
|       32 | 3524 | `		break;` |
|        - | 3525 | `									}` |
|       16 | 3526 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - | 3527 | `		/* Extract HTTP response status code */` |
|       32 | 3528 | `		int *pStatus = va_arg(ap, int *);` |
|       32 | 3529 | `		if( pStatus ){` |
|       32 | 3530 | `			*pStatus = pVm->iResponseStatus;` |
|       16 | 3531 | `		}` |
|       32 | 3532 | `		break;` |
|        - | 3533 | `										}` |
|       16 | 3534 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - | 3535 | `		/* Iterate response headers via callback */` |
|        - | 3536 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       32 | 3537 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       32 | 3538 | `		void *pUserData = va_arg(ap, void *);` |
|       32 | 3539 | `		if( xCallback ){` |
|       32 | 3540 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       32 | 3541 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       44 | 3542 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 | 3543 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 | 3544 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 | 3545 | `							   pUserData);` |
|       12 | 3546 | `				if( rc != PH7_OK ){` |
|      ! 0 | 3547 | `					break;` |
|        - | 3548 | `				}` |
|        6 | 3549 | `			}` |
|       16 | 3550 | `		}` |
|       32 | 3551 | `		break;` |
|        - | 3552 | `										 }` |
|      ! 0 | 3553 | `	default:` |
|        - | 3554 | `		/* Unknown configuration option */` |
|      ! 0 | 3555 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 | 3556 | `		break;` |
|        - | 3557 | `	}` |
|    89347 | 3558 | `	return rc;` |
|        5 | 3559 | `}` |
|        - | 3560 | `/* Forward declaration */` |
|        - | 3561 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - | 3562 | `/*` |
|        - | 3563 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - | 3564 | ` * format.` |
|        - | 3565 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - | 3566 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - | 3567 | ` * (STDOUT).` |
|        - | 3568 | ` */` |
|        2 | 3569 | `static sxi32 VmByteCodeDump(` |
|        - | 3570 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - | 3571 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - | 3572 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 3573 | `	)` |
|        1 | 3574 | `{` |
|        - | 3575 | `	static const char zDump[] = {` |
|        - | 3576 | `		"====================================================\n"` |
|        - | 3577 | `		"PH7 VM Dump\n"` |
|        - | 3578 | `		"====================================================\n"` |
|        - | 3579 | `	};` |
|        - | 3580 | `	VmInstr *pInstr,*pEnd;` |
|        3 | 3581 | `	sxi32 rc = SXRET_OK;` |
|        - | 3582 | `	sxu32 n;` |
|        - | 3583 | `	/* Point to the PH7 instructions */` |
|        3 | 3584 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 | 3585 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 | 3586 | `	n = 0;` |
|        3 | 3587 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - | 3588 | `	/* Dump instructions */` |
|        7 | 3589 | `	for(;;){` |
|       15 | 3590 | `		if( pInstr >= pEnd ){` |
|        - | 3591 | `			/* No more instructions */` |
|        3 | 3592 | `			break;` |
|        - | 3593 | `		}` |
|        - | 3594 | `		/* Format and call the consumer callback */` |
|       19 | 3595 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|       12 | 3596 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 | 3597 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|       13 | 3598 | `		if( rc != SXRET_OK ){` |
|        - | 3599 | `			/* Consumer routine request an operation abort */` |
|      ! 0 | 3600 | `			return rc;` |
|        - | 3601 | `		}` |
|       13 | 3602 | `		++n;` |
|       13 | 3603 | `		pInstr++; /* Next instruction in the stream */` |
|        1 | 3604 | `	}` |
|        3 | 3605 | `	return rc;` |
|        2 | 3606 | `}` |
|        - | 3607 | `/*` |
|        - | 3608 | ` * Save the execution state of a fiber/generator context.` |
|        - | 3609 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - | 3610 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - | 3611 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - | 3612 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - | 3613 | ` * when VmByteCodeExec returns.` |
|        - | 3614 | ` */` |
|     1646 | 3615 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|        - | 3616 | `	ph7_vm *pVm,` |
|        - | 3617 | `	ph7_exec_ctx *pCtx,` |
|        - | 3618 | `	sxi32 pc,` |
|        - | 3619 | `	sxi32 nTos` |
|        - | 3620 | `	)` |
|        5 | 3621 | `{` |
|      823 | 3622 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|     1651 | 3623 | `	pCtx->pc = pc;` |
|     1651 | 3624 | `	pCtx->nTos = nTos;` |
|     1651 | 3625 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|     1651 | 3626 | `	return PH7_SUSPEND;` |
|        5 | 3627 | `}` |
|        - | 3628 | `/*` |
|        - | 3629 | ` * Resolve named-argument mapping.` |
|        - | 3630 | ` *` |
|        - | 3631 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - | 3632 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - | 3633 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - | 3634 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - | 3635 | ` * every formal parameter that received a value.` |
|        - | 3636 | ` *` |
|        - | 3637 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - | 3638 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|        - | 3639 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|        - | 3640 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|        - | 3641 | ` */` |
|      276 | 3642 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|        - | 3643 | `	ph7_vm *pVm,` |
|        - | 3644 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - | 3645 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - | 3646 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - | 3647 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - | 3648 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - | 3649 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - | 3650 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - | 3651 | `)` |
|        4 | 3652 | `{` |
|      280 | 3653 | `	sxi32 posIdx = 0;` |
|        - | 3654 | `	sxu32 i;` |
|      280 | 3655 | `	int bSeenNamed = 0;` |
|        - | 3656 | `	char zErrMsg[256];` |
|      280 | 3657 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|     1056 | 3658 | `	for( i = 0; i < nActual; i++ ){` |
|      780 | 3659 | `		aSlot[i] = -2;` |
|      392 | 3660 | `	}` |
|     1044 | 3661 | `	for( i = 0; i < nActual; i++ ){` |
|      999 | 3662 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - | 3663 | `			/* Named argument — find formal by name */` |
|      454 | 3664 | `			int found = 0;` |
|      454 | 3665 | `			bSeenNamed = 1;` |
|        - | 3666 | `			sxu32 k;` |
|      704 | 3667 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      602 | 3668 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      585 | 3669 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      560 | 3670 | `						pMap->aNames[i].zString,` |
|      840 | 3671 | `						pMap->aNames[i].nByte) == 0 ){` |
|      356 | 3672 | `					if( aUsed[k] ){` |
|       12 | 3673 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3674 | `							"Named parameter $%.*s overwrites previous argument",` |
|        6 | 3675 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        9 | 3676 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3677 | `					}` |
|      349 | 3678 | `					aSlot[i] = (sxi32)k;` |
|      349 | 3679 | `					aUsed[k] = 1;` |
|      349 | 3680 | `					found = 1;` |
|      349 | 3681 | `					break;` |
|        - | 3682 | `				}` |
|      128 | 3683 | `			}` |
|      448 | 3684 | `			if( !found ){` |
|      101 | 3685 | `				if( iVariadicIdx >= 0 ){` |
|       93 | 3686 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|       47 | 3687 | `				}else{` |
|       11 | 3688 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3689 | `						"Unknown named parameter $%.*s",` |
|        6 | 3690 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        8 | 3691 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3692 | `				}` |
|       46 | 3693 | `			}` |
|      222 | 3694 | `		}else{` |
|        - | 3695 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|        - | 3696 | `			 * named arg (the parser rejects it at compile time), but a call` |
|        - | 3697 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|        - | 3698 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|      329 | 3699 | `			if( bSeenNamed ){` |
|      ! 0 | 3700 | `				return VmThrowNamedArgError(&(*pVm),` |
|        - | 3701 | `					"Cannot use positional argument after named argument",` |
|        - | 3702 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|        - | 3703 | `			}` |
|      329 | 3704 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       51 | 3705 | `				if( aUsed[posIdx] ){` |
|      ! 0 | 3706 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3707 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 | 3708 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 | 3709 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3710 | `				}` |
|       51 | 3711 | `				aSlot[i] = posIdx;` |
|       51 | 3712 | `				aUsed[posIdx] = 1;` |
|      303 | 3713 | `			}else if( iVariadicIdx >= 0 ){` |
|      279 | 3714 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      139 | 3715 | `			}` |
|      329 | 3716 | `			posIdx++;` |
|        - | 3717 | `		}` |
|      386 | 3718 | `	}` |
|      267 | 3719 | `	return SXRET_OK;` |
|      142 | 3720 | `}` |
|        - | 3721 | `/*` |
|        - | 3722 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - | 3723 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - | 3724 | ` */` |
|      302 | 3725 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        5 | 3726 | `{` |
|      307 | 3727 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      295 | 3728 | `		return 0;` |
|        - | 3729 | `	}` |
|       15 | 3730 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|      156 | 3731 | `}` |
|        - | 3732 | `/*` |
|        - | 3733 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - | 3734 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - | 3735 | ` * preserved (later wins), integer keys are renumbered.` |
|        - | 3736 | ` */` |
|       10 | 3737 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3738 | `{` |
|       11 | 3739 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 | 3740 | `	(void)pVm;` |
|       11 | 3741 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 | 3742 | `	return SXRET_OK;` |
|        1 | 3743 | `}` |
|        - | 3744 | `/*` |
|        - | 3745 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - | 3746 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - | 3747 | ` */` |
|        6 | 3748 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3749 | `{` |
|        3 | 3750 | `	(void)pVm; (void)pKey;` |
|        7 | 3751 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 | 3752 | `	return SXRET_OK;` |
|        1 | 3753 | `}` |
|        - | 3754 | `/*` |
|        - | 3755 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|        - | 3756 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|        - | 3757 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|        - | 3758 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|        - | 3759 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|        - | 3760 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|        - | 3761 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|        - | 3762 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|        - | 3763 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|        - | 3764 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|        - | 3765 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|        - | 3766 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|        - | 3767 | ` */` |
|        - | 3768 | `/*` |
|        - | 3769 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|        - | 3770 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|        - | 3771 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|        - | 3772 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|        - | 3773 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|        - | 3774 | ` */` |
|      280 | 3775 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|        4 | 3776 | `{` |
|        - | 3777 | `	VmSpreadRun sRun;` |
|        - | 3778 | `	ph7_hashmap_node *pNode;` |
|        - | 3779 | `	sxu32 i;` |
|      284 | 3780 | `	sRun.pStart = pFirst;` |
|      284 | 3781 | `	sRun.nCount = nCount;` |
|      284 | 3782 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      284 | 3783 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      284 | 3784 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|      ! 0 | 3785 | `		return;` |
|        - | 3786 | `	}` |
|      284 | 3787 | `	pNode = pMap->pFirst;` |
|     2040 | 3788 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|        - | 3789 | `		VmSpreadKey sKey;` |
|     1760 | 3790 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|        - | 3791 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|        - | 3792 | `			 * the source map's release before CALL replays them. */` |
|       95 | 3793 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       95 | 3794 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       95 | 3795 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|       48 | 3796 | `		}else{` |
|        - | 3797 | `			/* Integer key (or empty-string key, treated positionally) */` |
|     1666 | 3798 | `			sKey.nOff = 0;` |
|     1666 | 3799 | `			sKey.nLen = 0;` |
|        - | 3800 | `		}` |
|     1760 | 3801 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|     1760 | 3802 | `		pNode = pNode->pPrev; /* forward link */` |
|      882 | 3803 | `	}` |
|      144 | 3804 | `}` |
|        - | 3805 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|        - | 3806 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        8 | 3807 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|      ! 0 | 3808 | `{` |
|        8 | 3809 | `	SySetReset(&pVm->aSpreadRun);` |
|        8 | 3810 | `	SySetReset(&pVm->aSpreadKey);` |
|        8 | 3811 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        8 | 3812 | `}` |
|        - | 3813 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|        - | 3814 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|        - | 3815 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|        - | 3816 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|        - | 3817 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|        - | 3818 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|        - | 3819 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|        - | 3820 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|        - | 3821 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|        - | 3822 | ` * slot — from being consumed by that nested call. */` |
|      468 | 3823 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|        4 | 3824 | `{` |
|      472 | 3825 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      472 | 3826 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|        - | 3827 | `	VmSpreadRun *aRun;` |
|      472 | 3828 | `	if( rStart >= nRun ){` |
|      204 | 3829 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|        - | 3830 | `	}` |
|      272 | 3831 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      272 | 3832 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      272 | 3833 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      272 | 3834 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|      134 | 3835 | `	}` |
|      272 | 3836 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|      238 | 3837 | `}` |
|        - | 3838 | `/*` |
|        - | 3839 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|        - | 3840 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|        - | 3841 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|        - | 3842 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|        - | 3843 | ` *` |
|        - | 3844 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|        - | 3845 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|        - | 3846 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|        - | 3847 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|        - | 3848 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|        - | 3849 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|        - | 3850 | ` * they are counted only by that call. This replaces the old shared` |
|        - | 3851 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|        - | 3852 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|        - | 3853 | ` *` |
|        - | 3854 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|        - | 3855 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|        - | 3856 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|        - | 3857 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|        - | 3858 | ` */` |
|      296 | 3859 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|        4 | 3860 | `{` |
|      300 | 3861 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 3862 | `	VmSpreadRun *aRun;` |
|      300 | 3863 | `	ph7_value *pEnd = pTos;` |
|      300 | 3864 | `	sxi32 nPos = iP1;` |
|      300 | 3865 | `	sxi32 ri, extra = 0;` |
|      300 | 3866 | `	if( nRun == 0 ){` |
|       15 | 3867 | `		pVm->nSpreadCallBase = 0;` |
|       15 | 3868 | `		return 0;` |
|        - | 3869 | `	}` |
|      286 | 3870 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      286 | 3871 | `	ri = (sxi32)nRun - 1;` |
|      696 | 3872 | `	while( nPos > 0 ){` |
|      414 | 3873 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|        - | 3874 | `			/* A non-empty unpack occupying nCount slots. */` |
|      248 | 3875 | `			pEnd = aRun[ri].pStart;` |
|      248 | 3876 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      248 | 3877 | `			ri--;` |
|      292 | 3878 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|        - | 3879 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       39 | 3880 | `			extra -= 1;` |
|       39 | 3881 | `			ri--;` |
|       21 | 3882 | `		}else{` |
|        - | 3883 | `			/* An ordinary single-slot argument. */` |
|      134 | 3884 | `			pEnd--;` |
|        - | 3885 | `		}` |
|      414 | 3886 | `		nPos--;` |
|        4 | 3887 | `	}` |
|        - | 3888 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|        - | 3889 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      286 | 3890 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      286 | 3891 | `	return extra;` |
|      152 | 3892 | `}` |
|      280 | 3893 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)` |
|        4 | 3894 | `{` |
|      284 | 3895 | `	ph7_value *pTos = *ppTos;` |
|      284 | 3896 | `	sxu32 nEntry = pMap->nEntry;` |
|      284 | 3897 | `	if( nEntry == 0 ){` |
|        - | 3898 | `		/* Nothing to unpack — remove the source from the stack */` |
|       39 | 3899 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       39 | 3900 | `		VmPopOperand(&pTos, 1);` |
|       21 | 3901 | `	}else{` |
|        - | 3902 | `		ph7_hashmap_node *pNode;` |
|        - | 3903 | `		ph7_value *pElem;` |
|        - | 3904 | `		sxu32 i;` |
|        - | 3905 | `		int bTemp;` |
|      248 | 3906 | `		pMap->iRef++;` |
|      248 | 3907 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|        - | 3908 | `		/* Record the run + element keys before any release (nodes still alive).` |
|        - | 3909 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|      248 | 3910 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|        - | 3911 | `		/* Overwrite the source slot with the first element */` |
|      248 | 3912 | `		pNode = pMap->pFirst;` |
|      248 | 3913 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      248 | 3914 | `		PH7_MemObjRelease(pTos);` |
|      248 | 3915 | `		if( pElem ){` |
|      248 | 3916 | `			if( bTemp ){` |
|      135 | 3917 | `				PH7_MemObjStore(pElem, pTos);` |
|       68 | 3918 | `			}else{` |
|      114 | 3919 | `				PH7_MemObjLoad(pElem, pTos);` |
|        - | 3920 | `			}` |
|      122 | 3921 | `		}` |
|      248 | 3922 | `		pTos->nIdx = SXU32_HIGH;` |
|        - | 3923 | `		/* Traverse in insertion order (pPrev is the forward link` |
|        - | 3924 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|      248 | 3925 | `		pNode = pNode->pPrev;` |
|        - | 3926 | `		/* Push the remaining elements */` |
|     1760 | 3927 | `		for( i = 1; i < nEntry; i++ ){` |
|     1516 | 3928 | `			pTos++;` |
|     1516 | 3929 | `			PH7_MemObjInit(pVm, pTos);` |
|     1516 | 3930 | `			pTos->nIdx = SXU32_HIGH;` |
|     1516 | 3931 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|     1516 | 3932 | `			if( pElem ){` |
|     1516 | 3933 | `				if( bTemp ){` |
|     1259 | 3934 | `					PH7_MemObjStore(pElem, pTos);` |
|      630 | 3935 | `				}else{` |
|      258 | 3936 | `					PH7_MemObjLoad(pElem, pTos);` |
|        - | 3937 | `				}` |
|      756 | 3938 | `			}` |
|     1516 | 3939 | `			pNode = pNode->pPrev;` |
|      760 | 3940 | `		}` |
|      248 | 3941 | `		PH7_HashmapUnref(pMap);` |
|        - | 3942 | `	}` |
|      284 | 3943 | `	*ppTos = pTos;` |
|      284 | 3944 | `}` |
|        - | 3945 | `/*` |
|        - | 3946 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|        - | 3947 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|        - | 3948 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|        - | 3949 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|        - | 3950 | ` * element keys, interleaving them with the compile-time names at their real` |
|        - | 3951 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|        - | 3952 | ` *` |
|        - | 3953 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|        - | 3954 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|        - | 3955 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|        - | 3956 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|        - | 3957 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|        - | 3958 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|        - | 3959 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|        - | 3960 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|        - | 3961 | ` * method-name slot pop shifts pArg).` |
|        - | 3962 | ` *` |
|        - | 3963 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|        - | 3964 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|        - | 3965 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|        - | 3966 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|        - | 3967 | ` * which is after this call's synchronous named-arg resolution.` |
|        - | 3968 | ` */` |
|      256 | 3969 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|        - | 3970 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|        4 | 3971 | `{` |
|      260 | 3972 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 3973 | `	VmSpreadRun *aRun;` |
|        - | 3974 | `	VmSpreadKey *aKey;` |
|        - | 3975 | `	const char *zKeyBase;` |
|      260 | 3976 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|      260 | 3977 | `	int bAnyNamed = 0;` |
|        - | 3978 | `	sxu32 ai, ci, ri, rStart;` |
|      260 | 3979 | `	if( nRun == 0 ){` |
|        - | 3980 | `		/* No spread captured at all — the compile map is already aligned. */` |
|      ! 0 | 3981 | `		return 0;` |
|        - | 3982 | `	}` |
|      260 | 3983 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      260 | 3984 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      260 | 3985 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|        - | 3986 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|        - | 3987 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|        - | 3988 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|        - | 3989 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      260 | 3990 | `	ri = pVm->nSpreadCallBase;` |
|      260 | 3991 | `	rStart = ri;` |
|      260 | 3992 | `	if( rStart >= nRun ){` |
|        - | 3993 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|      ! 0 | 3994 | `		return 0;` |
|        - | 3995 | `	}` |
|      260 | 3996 | `	SySetReset(&pVm->aEffArgName);` |
|      260 | 3997 | `	ci = 0;` |
|      260 | 3998 | `	ai = 0;` |
|      614 | 3999 | `	while( ai < nActual ){` |
|        - | 4000 | `		SyString sName;` |
|      358 | 4001 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|        - | 4002 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|        - | 4003 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      370 | 4004 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|       13 | 4005 | `			ci++; ri++;` |
|        1 | 4006 | `		}` |
|      358 | 4007 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|        - | 4008 | `			/* A run of spread elements: one name per element from its key. Keys` |
|        - | 4009 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|        - | 4010 | `			 * run never desyncs the key stream. */` |
|      248 | 4011 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|     2004 | 4012 | `			for( j = 0; j < K; j++ ){` |
|     1760 | 4013 | `				SyZero(&sName, sizeof(sName));` |
|     1760 | 4014 | `				if( aKey[ks + j].nLen > 0 ){` |
|       95 | 4015 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       95 | 4016 | `					bAnyNamed = 1;` |
|       47 | 4017 | `				}` |
|     1760 | 4018 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      882 | 4019 | `			}` |
|      248 | 4020 | `			ai += K;` |
|      248 | 4021 | `			ci++; ri++;` |
|      126 | 4022 | `		}else{` |
|        - | 4023 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      114 | 4024 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|       33 | 4025 | `				sName = pCompile->aNames[ci];` |
|       33 | 4026 | `				bAnyNamed = 1;` |
|       16 | 4027 | `			}` |
|      114 | 4028 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      114 | 4029 | `			ai++;` |
|      114 | 4030 | `			ci++;` |
|        - | 4031 | `		}` |
|        4 | 4032 | `	}` |
|        - | 4033 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|        - | 4034 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|        - | 4035 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|        - | 4036 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      260 | 4037 | `	VmSpreadConsume(pVm);` |
|      260 | 4038 | `	if( !bAnyNamed ){` |
|        - | 4039 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      190 | 4040 | `		return 0;` |
|        - | 4041 | `	}` |
|       71 | 4042 | `	pEff->bHasNamed = 1;` |
|       71 | 4043 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|       71 | 4044 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|       71 | 4045 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|       71 | 4046 | `	pEff->nTotal = nActual;` |
|       71 | 4047 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|       71 | 4048 | `	return 1;` |
|      132 | 4049 | `}` |
|        - | 4050 | `/*` |
|        - | 4051 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|        - | 4052 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|        - | 4053 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|        - | 4054 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|        - | 4055 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|        - | 4056 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|        - | 4057 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|        - | 4058 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|        - | 4059 | ` * pArg must be the site's FINAL argument base.` |
|        - | 4060 | ` */` |
|  1039899 | 4061 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|        - | 4062 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|        5 | 4063 | `{` |
|  1039904 | 4064 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|  1039904 | 4065 | `	if( pInstr->iP2 == 0 ){` |
|  1039622 | 4066 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|        - | 4067 | `	}` |
|      286 | 4068 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|       71 | 4069 | `		return pStorage;` |
|        - | 4070 | `	}` |
|      216 | 4071 | `	VmSpreadConsume(pVm);` |
|      216 | 4072 | `	return pCompile;` |
|   520413 | 4073 | `}` |
|        - | 4074 | `/*` |
|        - | 4075 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|        - | 4076 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|        - | 4077 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — the two paths` |
|        - | 4078 | ` * disagree on which scalar types are silent (positional silences null+bool; keyed silences` |
|        - | 4079 | ` * only null, warning for bool to match PHP) — this only maps the type name and emits.` |
|        - | 4080 | ` */` |
|      ! 0 | 4081 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|      ! 0 | 4082 | `{` |
|      ! 0 | 4083 | `	const char *zType = "unknown";` |
|        - | 4084 | `	char zMsg[64];` |
|      ! 0 | 4085 | `	if( iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 4086 | `		zType = "string";` |
|      ! 0 | 4087 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        - | 4088 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|        - | 4089 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|        - | 4090 | `		 * REAL flag, so it still falls through to the int arm. */` |
|      ! 0 | 4091 | `		zType = "float";` |
|      ! 0 | 4092 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 | 4093 | `		zType = "int";` |
|      ! 0 | 4094 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 4095 | `		zType = "bool";` |
|      ! 0 | 4096 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4097 | `		zType = "object";` |
|      ! 0 | 4098 | `	}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 | 4099 | `		zType = "resource";` |
|      ! 0 | 4100 | `	}` |
|      ! 0 | 4101 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|      ! 0 | 4102 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 | 4103 | `}` |
|        - | 4104 | `/*` |
|        - | 4105 | ` * A member access in isset()/empty() context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY) is a silent` |
|        - | 4106 | ` * lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class instance"` |
|        - | 4107 | ` * warnings, mirroring the array isset/empty path.` |
|        - | 4108 | ` */` |
|     1102 | 4109 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|        5 | 4110 | `{` |
|     1107 | 4111 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY;` |
|        5 | 4112 | `}` |
|        - | 4113 | `/*` |
|        - | 4114 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|        - | 4115 | ` * A __get body reading the SAME property of the SAME instance must not` |
|        - | 4116 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|        - | 4117 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|        - | 4118 | ` * reads (other names / other instances) still dispatch.` |
|        - | 4119 | ` */` |
|     1060 | 4120 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4121 | `{` |
|        - | 4122 | `	VmMagicGuard *aG;` |
|        - | 4123 | `	sxu32 nHash;` |
|        - | 4124 | `	sxu32 n;` |
|     1061 | 4125 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|        - | 4126 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|        - | 4127 | `		 * every hooked-property access consults the guard, often twice. */` |
|      881 | 4128 | `		return FALSE;` |
|        - | 4129 | `	}` |
|      181 | 4130 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|      181 | 4131 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      225 | 4132 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|      181 | 4133 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|      137 | 4134 | `			return TRUE;` |
|        - | 4135 | `		}` |
|       23 | 4136 | `	}` |
|       45 | 4137 | `	return FALSE;` |
|      531 | 4138 | `}` |
|      486 | 4139 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4140 | `{` |
|        - | 4141 | `	VmMagicGuard sG;` |
|      487 | 4142 | `	sG.pThis = pThis;` |
|      487 | 4143 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      487 | 4144 | `	sG.cKind = cKind;` |
|      487 | 4145 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      487 | 4146 | `}` |
|      486 | 4147 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|        1 | 4148 | `{` |
|      487 | 4149 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      487 | 4150 | `}` |
|        - | 4151 | `/*` |
|        - | 4152 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|        - | 4153 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|        - | 4154 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|        - | 4155 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|        - | 4156 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|        - | 4157 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|        - | 4158 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|        - | 4159 | ` * One-token lookahead only.` |
|        - | 4160 | ` */` |
|      708 | 4161 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|        2 | 4162 | `{` |
|      710 | 4163 | `	switch( pNext->iOp ){` |
|       17 | 4164 | `		case PH7_OP_STORE:` |
|       36 | 4165 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|      ! 0 | 4166 | `		case PH7_OP_STORE_REF:` |
|      ! 0 | 4167 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|       11 | 4168 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|        - | 4169 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|        - | 4170 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|        - | 4171 | `		case PH7_OP_CAT_STORE:` |
|        - | 4172 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|        - | 4173 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       23 | 4174 | `			return 1;` |
|      326 | 4175 | `		default:` |
|      654 | 4176 | `			return 0;` |
|        - | 4177 | `	}` |
|      356 | 4178 | `}` |
|        - | 4179 | `/*` |
|        - | 4180 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|        - | 4181 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|        - | 4182 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|        - | 4183 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|        - | 4184 | ` */` |
|      424 | 4185 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|        1 | 4186 | `{` |
|      425 | 4187 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|        1 | 4188 | `}` |
|        - | 4189 | `/*` |
|        - | 4190 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|        - | 4191 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|        - | 4192 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|        - | 4193 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|        - | 4194 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|        - | 4195 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|        - | 4196 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|        - | 4197 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|        - | 4198 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|        - | 4199 | ` * abort path; SXRET_OK otherwise.` |
|        - | 4200 | ` */` |
|       54 | 4201 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|        1 | 4202 | `{` |
|        - | 4203 | `	char zHName[384];` |
|        - | 4204 | `	sxu32 nHName;` |
|        - | 4205 | `	ph7_class_method *pSetHook;` |
|       55 | 4206 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|        - | 4207 | `		/* get-only hooked property: php's read-only Error */` |
|        - | 4208 | `		SyBlob sErrMsg;` |
|        5 | 4209 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 4210 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|        4 | 4211 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|        5 | 4212 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        5 | 4213 | `		return SXRET_OK;` |
|        - | 4214 | `	}` |
|       51 | 4215 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 4216 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|      ! 0 | 4217 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|      ! 0 | 4218 | `		if( rcVis != SXRET_OK ){` |
|      ! 0 | 4219 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|      ! 0 | 4220 | `			return SXRET_OK;` |
|        - | 4221 | `		}` |
|      ! 0 | 4222 | `	}` |
|       51 | 4223 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|       51 | 4224 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|       51 | 4225 | `	if( pSetHook ){` |
|        - | 4226 | `		ph7_value sHookRet;` |
|        - | 4227 | `		ph7_value *apHArg[1];` |
|       51 | 4228 | `		apHArg[0] = pValue;` |
|       51 | 4229 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|       51 | 4230 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|       51 | 4231 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|       51 | 4232 | `		VmMagicGuardPop(pVm);` |
|       50 | 4233 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|       27 | 4234 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|        3 | 4235 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|        3 | 4236 | `			if( rcH == SXRET_OK ){` |
|        3 | 4237 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|        3 | 4238 | `				if( pBack ){` |
|        3 | 4239 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|        2 | 4240 | `				}` |
|        1 | 4241 | `			}else if( rcH == PH7_ABORT ){` |
|      ! 0 | 4242 | `				PH7_MemObjRelease(&sHookRet);` |
|      ! 0 | 4243 | `				return PH7_ABORT;` |
|        - | 4244 | `			}` |
|        - | 4245 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|        - | 4246 | `			 * the store is skipped, execution lands at the fetch point like any` |
|        - | 4247 | `			 * parked throw. */` |
|        1 | 4248 | `		}` |
|       51 | 4249 | `		PH7_MemObjRelease(&sHookRet);` |
|       25 | 4250 | `	}` |
|       51 | 4251 | `	return SXRET_OK;` |
|       28 | 4252 | `}` |
|        - | 4253 | `/*` |
|        - | 4254 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|        - | 4255 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|        - | 4256 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|        - | 4257 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|        - | 4258 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|        - | 4259 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|        - | 4260 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|        - | 4261 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|        - | 4262 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|        - | 4263 | ` */` |
|        - | 4264 | `/*` |
|        - | 4265 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|        - | 4266 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|        - | 4267 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|        - | 4268 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|        - | 4269 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|        - | 4270 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|        - | 4271 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|        - | 4272 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|        - | 4273 | ` * caller reads the raw slot then.` |
|        - | 4274 | ` */` |
|      332 | 4275 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|        3 | 4276 | `{` |
|      335 | 4277 | `	ph7_vm *pVm = pThis->pVm;` |
|        - | 4278 | `	char zHName[384];` |
|        - | 4279 | `	sxu32 nHName;` |
|        - | 4280 | `	ph7_class_method *pGetHook;` |
|        - | 4281 | `	sxi32 rc;` |
|      332 | 4282 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|      235 | 4283 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|      141 | 4284 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|        - | 4285 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|        - | 4286 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|        - | 4287 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|        - | 4288 | `		 * raw values whose output the routed throw then discards. */` |
|      197 | 4289 | `		return SXERR_NOTFOUND;` |
|        - | 4290 | `	}` |
|      139 | 4291 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|      139 | 4292 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|      139 | 4293 | `	if( pGetHook == 0 ){` |
|      ! 0 | 4294 | `		return SXERR_NOTFOUND;` |
|        - | 4295 | `	}` |
|      139 | 4296 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|      139 | 4297 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|      139 | 4298 | `	VmMagicGuardPop(pVm);` |
|      139 | 4299 | `	return rc;` |
|      169 | 4300 | `}` |
|        - | 4301 | `/*` |
|        - | 4302 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|        - | 4303 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|        - | 4304 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|        - | 4305 | ` */` |
|       20 | 4306 | `static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4307 | `{` |
|       21 | 4308 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 4309 | `	VmSlot sFree;` |
|       21 | 4310 | `	if( pScr ){` |
|       21 | 4311 | `		PH7_MemObjRelease(pScr);` |
|       10 | 4312 | `	}` |
|       21 | 4313 | `	sFree.nIdx = nIdx;` |
|       21 | 4314 | `	sFree.pUserData = 0;` |
|       21 | 4315 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       21 | 4316 | `}` |
|        - | 4317 | `/*` |
|        - | 4318 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|        - | 4319 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|        - | 4320 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|        - | 4321 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|        - | 4322 | ` * instance reference.` |
|        - | 4323 | ` */` |
|       16 | 4324 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|        2 | 4325 | `{` |
|       18 | 4326 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       18 | 4327 | `	if( pEnt == 0 ){` |
|        5 | 4328 | `		return;` |
|        - | 4329 | `	}` |
|       13 | 4330 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        5 | 4331 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|        2 | 4332 | `	}` |
|       13 | 4333 | `	SyBlobRelease(&pEnt->sName);` |
|       13 | 4334 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|       13 | 4335 | `	(void)SySetPop(&pVm->aHookRmw);` |
|       10 | 4336 | `}` |
|       16 | 4337 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4338 | `{` |
|        - | 4339 | `	VmHookRmw sEnt;` |
|        - | 4340 | `	VmHookRmw *pEnt;` |
|        - | 4341 | `	ph7_value *pScr;` |
|        - | 4342 | `	ph7_value sVal;` |
|       17 | 4343 | `	sxi32 rc = SXRET_OK;` |
|       17 | 4344 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       17 | 4345 | `	if( pEnt == 0 \|\| pEnt->iKind != VM_HOOK_PEND_RMW \|\| pEnt->nScratchIdx != nIdx ){` |
|      ! 0 | 4346 | `		return SXERR_NOTFOUND;` |
|        - | 4347 | `	}` |
|       17 | 4348 | `	sEnt = *pEnt;` |
|       17 | 4349 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        - | 4350 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|        - | 4351 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|        - | 4352 | `	 * scratch index past this point). */` |
|       17 | 4353 | `	PH7_MemObjInit(pVm,&sVal);` |
|       17 | 4354 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|       17 | 4355 | `	if( pScr ){` |
|       17 | 4356 | `		PH7_MemObjStore(pScr,&sVal);` |
|        8 | 4357 | `	}` |
|       17 | 4358 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|       17 | 4359 | `	sVal.nIdx = SXU32_HIGH;` |
|       17 | 4360 | `	if( pVm->nBoundaryRc == 0 ){` |
|       15 | 4361 | `		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|        7 | 4362 | `	}` |
|       17 | 4363 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 4364 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|       17 | 4365 | `	return rc;` |
|        9 | 4366 | `}` |
|        - | 4367 | `/*` |
|        - | 4368 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|        - | 4369 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|        - | 4370 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|        - | 4371 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|        - | 4372 | ` */` |
|       12 | 4373 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|        1 | 4374 | `{` |
|       13 | 4375 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       13 | 4376 | `	if( pSetMeth ){` |
|        - | 4377 | `		ph7_value sNameVal;` |
|        - | 4378 | `		ph7_value *apSetArg[2];` |
|       13 | 4379 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       13 | 4380 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       13 | 4381 | `		apSetArg[0] = &sNameVal;` |
|       13 | 4382 | `		apSetArg[1] = pValue;` |
|       13 | 4383 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       13 | 4384 | `		PH7_VmCallClassMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       13 | 4385 | `		VmMagicGuardPop(pVm);` |
|       13 | 4386 | `		PH7_MemObjRelease(&sNameVal);` |
|        6 | 4387 | `	}` |
|       13 | 4388 | `}` |
|        - | 4389 | `/*` |
|        - | 4390 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|        - | 4391 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|        - | 4392 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|        - | 4393 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|        - | 4394 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|        - | 4395 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|        - | 4396 | ` * path (the SyHash-layout incident class).` |
|        - | 4397 | ` */` |
|        - | 4398 | `/*` |
|        - | 4399 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|        - | 4400 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|        - | 4401 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|        - | 4402 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|        - | 4403 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|        - | 4404 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|        - | 4405 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|        - | 4406 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|        - | 4407 | ` * never linked (INIT error path).` |
|        - | 4408 | ` */` |
|    23206 | 4409 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|        5 | 4410 | `{` |
|    23211 | 4411 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|    23211 | 4412 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|        - | 4413 | `	sxu32 i;` |
|    25387 | 4414 | `	for( i = 0 ; i < n ; ++i ){` |
|    25387 | 4415 | `		if( apStep[i] == pStep ){` |
|    23219 | 4416 | `			for( ; i + 1 < n ; ++i ){` |
|        9 | 4417 | `				apStep[i] = apStep[i + 1];` |
|        5 | 4418 | `			}` |
|    23211 | 4419 | `			(void)SySetPop(&pInfo->aStep);` |
|    23211 | 4420 | `			return;` |
|        - | 4421 | `		}` |
|     1093 | 4422 | `	}` |
|    11608 | 4423 | `}` |
|      210 | 4424 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|        5 | 4425 | `{` |
|      215 | 4426 | `	if( pStep->pOwner ){` |
|       24 | 4427 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       11 | 4428 | `	}` |
|      215 | 4429 | `	VmForeachStepUnlink(pInfo,pStep);` |
|      215 | 4430 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      215 | 4431 | `	PH7_ClassInstanceUnref(pThis);` |
|      215 | 4432 | `}` |
|        - | 4433 | `/*` |
|        - | 4434 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|        - | 4435 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|        - | 4436 | ` * step stack, then drop the step's map reference. The single home for this` |
|        - | 4437 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|        - | 4438 | ` * load-bearing: a step freed while still registered is walked by the next` |
|        - | 4439 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|        - | 4440 | ` * class), and the unregister must precede the unref in case the step held the` |
|        - | 4441 | ` * map's last reference.` |
|        - | 4442 | ` */` |
|    22974 | 4443 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|        5 | 4444 | `{` |
|    22979 | 4445 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|    22979 | 4446 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|    22979 | 4447 | `	if( bPop ){` |
|        - | 4448 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|        - | 4449 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|    22979 | 4450 | `		VmForeachStepUnlink(pInfo,pStep);` |
|    11487 | 4451 | `	}` |
|    22979 | 4452 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    22979 | 4453 | `	PH7_HashmapUnref(pMap);` |
|    22979 | 4454 | `}` |
|        - | 4455 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|        - | 4456 | `/*` |
|        - | 4457 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 4458 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4459 | ` * See block-comment on that function for additional information.` |
|        - | 4460 | ` */` |
|   137648 | 4461 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 4462 | `{` |
|        - | 4463 | `	ph7_value *pStack;` |
|        - | 4464 | `	sxu32 nCap;` |
|        - | 4465 | `	sxi32 rc;` |
|        - | 4466 | `	/* Allocate a new operand stack */` |
|   137653 | 4467 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   137653 | 4468 | `	if( pStack == 0 ){` |
|      ! 0 | 4469 | `		return SXERR_MEM;` |
|        - | 4470 | `	}` |
|   137653 | 4471 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|        - | 4472 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|        - | 4473 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   137653 | 4474 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|        - | 4475 | `	/* Free the operand stack */` |
|   137653 | 4476 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 4477 | `	/* Execution result */` |
|   137653 | 4478 | `	return rc;` |
|    68829 | 4479 | `}` |
|        - | 4480 | `/*` |
|        - | 4481 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|        - | 4482 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|        - | 4483 | ` * the argument resolve against that class (like php) rather than the reflection` |
|        - | 4484 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|        - | 4485 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|        - | 4486 | ` */` |
|       54 | 4487 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|        1 | 4488 | `{` |
|       55 | 4489 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       55 | 4490 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - | 4491 | `	sxi32 rc;` |
|       55 | 4492 | `	if( pDeclCls ){` |
|       39 | 4493 | `		pVm->pConstEvalClass = pDeclCls;` |
|       39 | 4494 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       19 | 4495 | `	}` |
|       55 | 4496 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       55 | 4497 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       55 | 4498 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       55 | 4499 | `	return rc;` |
|        1 | 4500 | `}` |
|        - | 4501 | `/*` |
|        - | 4502 | ` * Invoke any installed shutdown callbacks.` |
|        - | 4503 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|        - | 4504 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|        - | 4505 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|        - | 4506 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|        - | 4507 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|        - | 4508 | ` *` |
|        - | 4509 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|        - | 4510 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|        - | 4511 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|        - | 4512 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|        - | 4513 | ` * down and restore the default consumer.` |
|        - | 4514 | ` */` |
|     3418 | 4515 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|        5 | 4516 | `{` |
|     3423 | 4517 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - | 4518 | `	sxu32 n,nUsed;` |
|     3423 | 4519 | `	nUsed = SySetUsed(&pVm->aOB);` |
|     3423 | 4520 | `	if( nUsed < 1 ){` |
|     3421 | 4521 | `		return;` |
|        - | 4522 | `	}` |
|        7 | 4523 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4524 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4525 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|        5 | 4526 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|        5 | 4527 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|        2 | 4528 | `		}` |
|        3 | 4529 | `	}` |
|        - | 4530 | `	/* Restore the default consumer and release the buffers. */` |
|        3 | 4531 | `	pCons->xConsumer = pCons->xDef;` |
|        3 | 4532 | `	pCons->pUserData = pCons->pDefData;` |
|        7 | 4533 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4534 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4535 | `		if( pOb ){` |
|        5 | 4536 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|        5 | 4537 | `			SyBlobRelease(&pOb->sOB);` |
|        2 | 4538 | `		}` |
|        3 | 4539 | `	}` |
|        3 | 4540 | `	SySetReset(&pVm->aOB);` |
|        3 | 4541 | `	pVm->nObDepth = 0;` |
|     1714 | 4542 | `}` |
|        - | 4543 | `/*` |
|        - | 4544 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 4545 | ` * or more calls to [register_shutdown_function()].` |
|        - | 4546 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 4547 | ` * execution ends.` |
|        - | 4548 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 4549 | ` * additional information.` |
|        - | 4550 | ` */` |
|     3418 | 4551 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 4552 | `{` |
|        - | 4553 | `	VmShutdownCB *pEntry;` |
|        - | 4554 | `	ph7_value *apArg[10];` |
|        - | 4555 | `	sxu32 n,nEntry;` |
|        - | 4556 | `	int i;` |
|        - | 4557 | `	/* Point to the stack of registered callbacks */` |
|     3423 | 4558 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    37603 | 4559 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    34185 | 4560 | `		apArg[i] = 0;` |
|    17095 | 4561 | `	}` |
|        - | 4562 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 4563 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 4564 | `	 * callbacks, mirroring PHP.` |
|        - | 4565 | `	 */` |
|     3423 | 4566 | `	pVm->bHaltRequested = 0;` |
|     3441 | 4567 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       23 | 4568 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4569 | `		if( pEntry ){` |
|        - | 4570 | `			/* Prepare callback arguments if any */` |
|       23 | 4571 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 4572 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 4573 | `					break;` |
|        - | 4574 | `				}` |
|      ! 0 | 4575 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 4576 | `			}` |
|        - | 4577 | `			/* Invoke the callback */` |
|       23 | 4578 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 4579 | `			/*` |
|        - | 4580 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 4581 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 4582 | `			 */` |
|       23 | 4583 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4584 | `			if( pEntry ){` |
|       23 | 4585 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       23 | 4586 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 4587 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 4588 | `				}` |
|        9 | 4589 | `			}` |
|       23 | 4590 | `			if( pVm->bHaltRequested ){` |
|        - | 4591 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 4592 | `				break;` |
|        - | 4593 | `			}` |
|        9 | 4594 | `		}` |
|       14 | 4595 | `	}` |
|     3423 | 4596 | `	SySetReset(&pVm->aShutdown);` |
|     3423 | 4597 | `}` |
|        - | 4598 | `/*` |
|        - | 4599 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 4600 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4601 | ` * See block-comment on that function for additional information.` |
|        - | 4602 | ` */` |
|     3418 | 4603 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 4604 | `{` |
|        - | 4605 | `	/* Make sure we are ready to execute this program */` |
|     3423 | 4606 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 4607 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 4608 | `	}` |
|        - | 4609 | `	/* Set the execution magic number  */` |
|     3423 | 4610 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 4611 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|        - | 4612 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|        - | 4613 | `	{` |
|     3423 | 4614 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|     3423 | 4615 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|        - | 4616 | `	}` |
|        - | 4617 | `	/* Invoke any shutdown callbacks */` |
|     3423 | 4618 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 4619 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|        - | 4620 | `	 * shutdown callbacks, which may still write into them. */` |
|     3423 | 4621 | `	VmFlushOutputBuffers(&(*pVm));` |
|        - | 4622 | `	/*` |
|        - | 4623 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 4624 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 4625 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 4626 | `	 */` |
|     3423 | 4627 | `	return SXRET_OK;` |
|     1714 | 4628 | `}` |
|        - | 4629 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 4630 | `/*` |
|        - | 4631 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 4632 | ` * the desired message.` |
|        - | 4633 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 4634 | ` * in 'api.c' for additional information.` |
|        - | 4635 | ` */` |
|     2180 | 4636 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 4637 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 4638 | `	SyString *pString /* Message to output */` |
|        - | 4639 | `	)` |
|        5 | 4640 | `{` |
|     2185 | 4641 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     2185 | 4642 | `	sxi32 rc = SXRET_OK;` |
|        - | 4643 | `	/* Call the output consumer */` |
|     2185 | 4644 | `	if( pString->nByte > 0 ){` |
|     2185 | 4645 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     2185 | 4646 | `		VmTrackOutput(pVm, pString->nByte);` |
|     1090 | 4647 | `	}` |
|     2185 | 4648 | `	return rc;` |
|        5 | 4649 | `}` |
|        - | 4650 | `/*` |
|        - | 4651 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 4652 | ` * callback to consume the formatted message.` |
|        - | 4653 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 4654 | ` * in 'api.c' for additional information.` |
|        - | 4655 | ` */` |
|        2 | 4656 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 4657 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 4658 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 4659 | `	va_list ap           /* Variable list of arguments */` |
|        - | 4660 | `	)` |
|        1 | 4661 | `{` |
|        3 | 4662 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 4663 | `	sxi32 rc = SXRET_OK;` |
|        - | 4664 | `	SyBlob sWorker;` |
|        - | 4665 | `	/* Format the message and call the output consumer */` |
|        3 | 4666 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 4667 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 4668 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 4669 | `		/* Consume the formatted message */` |
|        3 | 4670 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 4671 | `	}` |
|        3 | 4672 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 4673 | `	/* Release the working buffer */` |
|        3 | 4674 | `	SyBlobRelease(&sWorker);` |
|        3 | 4675 | `	return rc;` |
|        1 | 4676 | `}` |
|        - | 4677 | `/*` |
|        - | 4678 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 4679 | ` * This function never fail and always return a pointer` |
|        - | 4680 | ` * to a null terminated string.` |
|        - | 4681 | ` */` |
|       12 | 4682 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 4683 | `{` |
|       13 | 4684 | `	const char *zOp = "Unknown     ";` |
|       13 | 4685 | `	switch(nOp){` |
|        3 | 4686 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 4687 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 4688 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 4689 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 4690 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 4691 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 4692 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 4693 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 4694 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 4695 | `	case PH7_OP_LOAD_FCC:` |
|      ! 0 | 4696 | `		                    zOp = "LOAD_FCC   "; break;` |
|      ! 0 | 4697 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 4698 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 4699 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 4700 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 4701 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 4702 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 4703 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 4704 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 4705 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 4706 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 4707 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 4708 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 4709 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 4710 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 4711 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 4712 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 4713 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 4714 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 4715 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 4716 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 4717 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 4718 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 4719 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 4720 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 4721 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 4722 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 4723 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 4724 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 4725 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 4726 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 4727 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 4728 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 4729 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 4730 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 4731 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 4732 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 4733 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 4734 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 4735 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 4736 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 4737 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 4738 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 4739 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 4740 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 4741 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 4742 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 4743 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|      ! 0 | 4744 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 4745 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 4746 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 4747 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 4748 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 4749 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 4750 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 4751 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 4752 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 4753 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 4754 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 4755 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 4756 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 4757 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 4758 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 4759 | `	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;` |
|      ! 0 | 4760 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 4761 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 4762 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 4763 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 4764 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 4765 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 4766 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 4767 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 4768 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 4769 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 4770 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 4771 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 4772 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 4773 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 4774 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 4775 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 4776 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 4777 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|      ! 0 | 4778 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 4779 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 4780 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 4781 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 4782 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 4783 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 4784 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 4785 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 4786 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 4787 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 4788 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 4789 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 4790 | `	default:` |
|      ! 0 | 4791 | `		break;` |
|        - | 4792 | `	}` |
|       13 | 4793 | `	return zOp;` |
|        1 | 4794 | `}` |
|        - | 4795 | `/*` |
|        - | 4796 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 4797 | ` * The xConsumer() callback which is an used defined function` |
|        - | 4798 | ` * is responsible of consuming the generated dump.` |
|        - | 4799 | ` */` |
|        2 | 4800 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 4801 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 4802 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 4803 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 4804 | `	)` |
|        1 | 4805 | `{` |
|        - | 4806 | `	sxi32 rc;` |
|        3 | 4807 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 4808 | `	return rc;` |
|        1 | 4809 | `}` |
|        - | 4810 | `/*` |
|        - | 4811 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 4812 | ` * outside a class body [i.e: global or function scope].` |
|        - | 4813 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 4814 | ` * in 'compile.c' for additional information.` |
|        - | 4815 | ` */` |
|       42 | 4816 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        2 | 4817 | `{` |
|       44 | 4818 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 4819 | `	/* Evaluate and expand constant value */` |
|       44 | 4820 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       44 | 4821 | `}` |
|        - | 4822 | `/*` |
|        - | 4823 | ` * Section:` |
|        - | 4824 | ` *  Function handling functions.` |
|        - | 4825 | ` * Status:` |
|        - | 4826 | ` *    Stable.` |
|        - | 4827 | ` */` |
|        - | 4828 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 4829 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 4830 | `	{ "__phl_magic_call", vm_builtin_magic_call },` |
|        - | 4831 | `	{ "__phl_enum_cases",   vm_builtin_enum_cases },` |
|        - | 4832 | `	{ "__phl_enum_from",    vm_builtin_enum_from },` |
|        - | 4833 | `	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },` |
|        - | 4834 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|        - | 4835 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 4836 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 4837 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 4838 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 4839 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 4840 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 4841 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 4842 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 4843 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 4844 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 4845 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 4846 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 4847 | `	    /* Constants management */` |
|        - | 4848 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 4849 | `	{ "define",   vm_builtin_define               },` |
|        - | 4850 | `	{ "constant", vm_builtin_constant             },` |
|        - | 4851 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 4852 | `	   /* Class/Object functions */` |
|        - | 4853 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 4854 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 4855 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 4856 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 4857 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 4858 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|        - | 4859 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 4860 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 4861 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 4862 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 4863 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 4864 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 4865 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 4866 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 4867 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 4868 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 4869 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 4870 | `	   /* SPL object identity */` |
|        - | 4871 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|        - | 4872 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|        - | 4873 | `	   /* SPL Autoloading */` |
|        - | 4874 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 4875 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 4876 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 4877 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 4878 | `	   /* Random numbers/strings generators */` |
|        - | 4879 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 4880 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 4881 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 4882 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 4883 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 4884 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 4885 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 4886 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 4887 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 4888 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 4889 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 4890 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 4891 | `	   /* Language constructs functions */` |
|        - | 4892 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 4893 | `	{ "print", vm_builtin_print                   },` |
|        - | 4894 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 4895 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 4896 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 4897 | `	  /* Variable handling functions */` |
|        - | 4898 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 4899 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 4900 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 4901 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 4902 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 4903 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 4904 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 4905 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 4906 | `	  /* Ouput control functions */` |
|        - | 4907 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 4908 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 4909 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 4910 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 4911 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 4912 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 4913 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 4914 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 4915 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 4916 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 4917 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 4918 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 4919 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 4920 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 4921 | `	  /* Assertion functions */` |
|        - | 4922 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 4923 | `	  /* Error reporting functions */` |
|        - | 4924 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 4925 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 4926 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 4927 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 4928 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 4929 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 4930 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 4931 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 4932 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|        - | 4933 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|        - | 4934 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 4935 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|        - | 4936 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|        - | 4937 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 4938 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 4939 | `	  /* Release info */` |
|        - | 4940 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 4941 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 4942 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 4943 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 4944 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 4945 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 4946 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 4947 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 4948 | `	  /* hashmap */` |
|        - | 4949 | `	{"compact",          vm_builtin_compact       },` |
|        - | 4950 | `	{"extract",          vm_builtin_extract       },` |
|        - | 4951 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 4952 | `	  /* URL related function */` |
|        - | 4953 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 4954 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 4955 | `	   /* UTF-8 encoding/decoding */` |
|        - | 4956 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 4957 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 4958 | `	   /* Command line processing */` |
|        - | 4959 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 4960 | `	   /* JSON encoding/decoding */` |
|        - | 4961 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 4962 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 4963 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 4964 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 4965 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 4966 | `	{"serialize",      vm_builtin_serialize },` |
|        - | 4967 | `	{"unserialize",    vm_builtin_unserialize },` |
|        - | 4968 | `	   /* Files/URI inclusion facility */` |
|        - | 4969 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 4970 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|        - | 4971 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 4972 | `	{ "include",      vm_builtin_include          },` |
|        - | 4973 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 4974 | `	{ "require",      vm_builtin_require          },` |
|        - | 4975 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 4976 | `};` |
|        - | 4977 | `/*` |
|        - | 4978 | ` * Register the built-in VM functions defined above.` |
|        - | 4979 | ` */` |
|     3414 | 4980 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 4981 | `{` |
|        - | 4982 | `	sxi32 rc;` |
|        - | 4983 | `	sxu32 n;` |
|   426755 | 4984 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 4985 | `		/* Note that these special functions have access` |
|        - | 4986 | `		 * to the underlying virtual machine as their` |
|        - | 4987 | `		 * private data.` |
|        - | 4988 | `		 */` |
|   423341 | 4989 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   423341 | 4990 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 4991 | `			return rc;` |
|        - | 4992 | `		}` |
|   211673 | 4993 | `	}` |
|     3419 | 4994 | `	return SXRET_OK;` |
|     1712 | 4995 | `}` |
|        - | 4996 | `/*` |
|        - | 4997 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 4998 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 4999 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 5000 | ` */` |
|   769042 | 5001 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 5002 | `{` |
|   769047 | 5003 | `	if( !iLoadable ){` |
|   762417 | 5004 | `		return pClass;` |
|        - | 5005 | `	}` |
|     6639 | 5006 | `	while(pClass){` |
|     6635 | 5007 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     6631 | 5008 | `			return pClass;` |
|        - | 5009 | `		}` |
|        5 | 5010 | `		pClass = pClass->pNextName;` |
|        1 | 5011 | `	}` |
|        5 | 5012 | `	return 0;` |
|   384526 | 5013 | `}` |
|        - | 5014 | `/*` |
|        - | 5015 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 5016 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 5017 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 5018 | ` * registered in the VM's class table.` |
|        - | 5019 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 5020 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 5021 | ` */` |
|      300 | 5022 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5023 | `{` |
|        - | 5024 | `	VmAutoloadCB *pEntry;` |
|        - | 5025 | `	ph7_value sArg,sResult;` |
|        - | 5026 | `	SyHashEntry *pHashEntry;` |
|        - | 5027 | `	ph7_class *pClass;` |
|        - | 5028 | `	sxu32 n,nEntry;` |
|      305 | 5029 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|      305 | 5030 | `	if( nEntry < 1 ){` |
|      233 | 5031 | `		return 0;` |
|        - | 5032 | `	}` |
|        - | 5033 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       77 | 5034 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 5035 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 5036 | `	}` |
|        - | 5037 | `	/* Mark this class as being autoloaded */` |
|       75 | 5038 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 5039 | `	/* Prepare the class name argument */` |
|       75 | 5040 | `	PH7_MemObjInit(pVm,&sArg);` |
|       75 | 5041 | `	PH7_MemObjInit(pVm,&sResult);` |
|       75 | 5042 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       75 | 5043 | `	pClass = 0;` |
|      139 | 5044 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 5045 | `		ph7_value *apArg[1];` |
|       85 | 5046 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       85 | 5047 | `		if( pEntry == 0 ){` |
|      ! 0 | 5048 | `			continue;` |
|        - | 5049 | `		}` |
|       85 | 5050 | `		apArg[0] = &sArg;` |
|       85 | 5051 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 5052 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 5053 | `			continue;` |
|        - | 5054 | `		}` |
|        - | 5055 | `		/* Check if the class is now available */` |
|       85 | 5056 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       85 | 5057 | `		if( pHashEntry ){` |
|       20 | 5058 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       20 | 5059 | `			if( pClass ){` |
|       20 | 5060 | `				break;` |
|        - | 5061 | `			}` |
|      ! 0 | 5062 | `		}` |
|       35 | 5063 | `	}` |
|       75 | 5064 | `	PH7_MemObjRelease(&sArg);` |
|       75 | 5065 | `	PH7_MemObjRelease(&sResult);` |
|        - | 5066 | `	/* Remove reentrancy guard */` |
|       75 | 5067 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       75 | 5068 | `	return pClass;` |
|      155 | 5069 | `}` |
|        - | 5070 | `/*` |
|        - | 5071 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 5072 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 5073 | ` */` |
|       38 | 5074 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5075 | `{` |
|       43 | 5076 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 | 5077 | `}` |
|        - | 5078 | `/*` |
|        - | 5079 | ` * Check if the given name refer to an installed class.` |
|        - | 5080 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 5081 | ` */` |
|   769288 | 5082 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 5083 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 5084 | `	const char *zName,  /* Name of the target class */` |
|        - | 5085 | `	sxu32 nByte,        /* zName length */` |
|        - | 5086 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 5087 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 5088 | `						 */` |
|        - | 5089 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 5090 | `	)` |
|        5 | 5091 | `{` |
|        - | 5092 | `	SyHashEntry *pEntry;` |
|        - | 5093 | `	ph7_class *pClass;` |
|   384644 | 5094 | `	SXUNUSED(iNest);` |
|        - | 5095 | `	/* Exact class lookup.` |
|        - | 5096 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 5097 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   769293 | 5098 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   769293 | 5099 | `	if( pEntry == 0 ){` |
|        - | 5100 | `		/* Class not found in hash table — try autoload before giving up */` |
|      267 | 5101 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 5102 | `	}` |
|   769031 | 5103 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   769031 | 5104 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   384649 | 5105 | `}` |
|        - | 5106 | `/*` |
|        - | 5107 | ` * Reference Table Implementation` |
|        - | 5108 | ` * Status: stable <chm@symisc.net>` |
|        - | 5109 | ` * Intro` |
|        - | 5110 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 5111 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 5112 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 5113 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 5114 | ` *  Refer to the official for more information on this powerful` |
|        - | 5115 | ` *  extension.` |
|        - | 5116 | ` */` |
|        - | 5117 | `/*` |
|        - | 5118 | ` * Allocate a new reference entry.` |
|        - | 5119 | ` */` |
|  4359431 | 5120 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 5121 | `{` |
|        - | 5122 | `	VmRefObj *pRef;` |
|        - | 5123 | `	/* Allocate a new instance */` |
|  4359436 | 5124 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  4359436 | 5125 | `	if( pRef == 0 ){` |
|      ! 0 | 5126 | `		return 0;` |
|        - | 5127 | `	}` |
|        - | 5128 | `	/* Zero the structure */` |
|  4359436 | 5129 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 5130 | `	/* Initialize fields */` |
|  4359436 | 5131 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  4359436 | 5132 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  4359436 | 5133 | `	pRef->nIdx = nIdx;` |
|  4359436 | 5134 | `	return pRef;` |
|  2180329 | 5135 | `}` |
|        - | 5136 | `/*` |
|        - | 5137 | ` * Default hash function used by the reference table` |
|        - | 5138 | ` * for lookup/insertion operations.` |
|        - | 5139 | ` */` |
| 23422175 | 5140 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 5141 | `{` |
|        - | 5142 | `	/* Calculate the hash based on the memory object index */` |
| 23422180 | 5143 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 5144 | `}` |
|        - | 5145 | `/*` |
|        - | 5146 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 5147 | ` * in the reference table.` |
|        - | 5148 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 5149 | ` * otherwise.` |
|        - | 5150 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5151 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5152 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5153 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5154 | ` * Refer to the official for more information on this powerful` |
|        - | 5155 | ` * extension.` |
|        - | 5156 | ` */` |
| 13544373 | 5157 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 5158 | `{` |
|        - | 5159 | `	VmRefObj *pRef;` |
|        - | 5160 | `	sxu32 nBucket;` |
|        - | 5161 | `	/* Point to the appropriate bucket */` |
| 13544378 | 5162 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 5163 | `	/* Perform the lookup */` |
| 13544378 | 5164 | `	pRef = pVm->apRefObj[nBucket];` |
| 29851782 | 5165 | `	for(;;){` |
| 59694631 | 5166 | `		if( pRef == 0 ){` |
|  4844672 | 5167 | `			break;` |
|        - | 5168 | `		}` |
| 54849964 | 5169 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 5170 | `			/* Entry found */` |
|  8699711 | 5171 | `			return pRef;` |
|        - | 5172 | `		}` |
|        - | 5173 | `		/* Point to the next entry */` |
| 46150258 | 5174 | `		pRef = pRef->pNextCollide;` |
|        5 | 5175 | `	}` |
|        - | 5176 | `	/* No such entry,return NULL */` |
|  4844672 | 5177 | `	return 0;` |
|  6774017 | 5178 | `}` |
|        - | 5179 | `/*` |
|        - | 5180 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5181 | ` *` |
|        - | 5182 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5183 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5184 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5185 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5186 | ` * Refer to the official for more information on this powerful` |
|        - | 5187 | ` * extension.` |
|        - | 5188 | ` */` |
|  4359431 | 5189 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5190 | `{` |
|        - | 5191 | `	sxu32 nBucket;` |
|  4359436 | 5192 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 5193 | `		VmRefObj **apNew;` |
|        - | 5194 | `		sxu32 nNew;` |
|        - | 5195 | `		/* Allocate a larger table */` |
|    10563 | 5196 | `		nNew = pVm->nRefSize << 1;` |
|    10563 | 5197 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|    10563 | 5198 | `		if( apNew ){` |
|    10563 | 5199 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 5200 | `			sxu32 n;` |
|        - | 5201 | `			/* Zero the structure */` |
|    10563 | 5202 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 5203 | `			/* Rehash all referenced entries */` |
|  2966465 | 5204 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 5205 | `				/* Remove old collision links */` |
|  2955907 | 5206 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 5207 | `				/* Point to the appropriate bucket */` |
|  2955907 | 5208 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 5209 | `				/* Insert the entry  */` |
|  2955907 | 5210 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2955907 | 5211 | `				if( apNew[nBucket] ){` |
|  2306069 | 5212 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1153032 | 5213 | `				}` |
|  2955907 | 5214 | `				apNew[nBucket] = pEntry;` |
|        - | 5215 | `				/* Point to the next entry */` |
|  2955907 | 5216 | `				pEntry = pEntry->pNext;` |
|  1477956 | 5217 | `			}` |
|        - | 5218 | `			/* Release the old table */` |
|    10563 | 5219 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 5220 | `			/* Install the new one */` |
|    10563 | 5221 | `			pVm->apRefObj = apNew;` |
|    10563 | 5222 | `			pVm->nRefSize = nNew;` |
|     5279 | 5223 | `		}` |
|     5279 | 5224 | `	}` |
|        - | 5225 | `	/* Point to the appropriate bucket */` |
|  4359436 | 5226 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 5227 | `	/* Insert the entry */` |
|  4359436 | 5228 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  4359436 | 5229 | `	if( pVm->apRefObj[nBucket] ){` |
|  3357190 | 5230 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1679125 | 5231 | `	}` |
|  4359436 | 5232 | `	pVm->apRefObj[nBucket] = pRef;` |
|  4359436 | 5233 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  4359436 | 5234 | `	pVm->nRefUsed++;` |
|  4359436 | 5235 | `	return SXRET_OK;` |
|        5 | 5236 | `}` |
|        - | 5237 | `/*` |
|        - | 5238 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 5239 | ` * the reference table.` |
|        - | 5240 | ` * This function is invoked when the user perform an unset` |
|        - | 5241 | ` * call [i.e: unset($var); ].` |
|        - | 5242 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5243 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5244 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5245 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5246 | ` * Refer to the official for more information on this powerful` |
|        - | 5247 | ` * extension.` |
|        - | 5248 | ` */` |
|  4225761 | 5249 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5250 | `{` |
|        - | 5251 | `	ph7_hashmap_node **apNode;` |
|        - | 5252 | `	SyHashEntry **apEntry;` |
|        - | 5253 | `	sxu32 n;` |
|        - | 5254 | `	/* Point to the reference table */` |
|  4225766 | 5255 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  4225766 | 5256 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 5257 | `	/* Unlink the entry from the reference table */` |
|  4717106 | 5258 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   491345 | 5259 | `		if( apEntry[n] ){` |
|   485465 | 5260 | `			SyHashDeleteEntry2(apEntry[n]);` |
|   243339 | 5261 | `		}` |
|   246284 | 5262 | `	}` |
|  7944825 | 5263 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3719064 | 5264 | `		if( apNode[n] ){` |
|     1380 | 5265 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|      688 | 5266 | `		}` |
|  1859534 | 5267 | `	}` |
|  4225766 | 5268 | `	if( pRef->pPrevCollide ){` |
|  1663297 | 5269 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   831291 | 5270 | `	}else{` |
|  2562474 | 5271 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 5272 | `	}` |
|  4225766 | 5273 | `	if( pRef->pNextCollide ){` |
|  2244837 | 5274 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|  1122960 | 5275 | `	}` |
|  4225766 | 5276 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 5277 | `	/* Release the node */` |
|  4225766 | 5278 | `	SySetRelease(&pRef->aReference);` |
|  4225766 | 5279 | `	SySetRelease(&pRef->aArrEntries);` |
|  4225766 | 5280 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  4225766 | 5281 | `	pVm->nRefUsed--;` |
|  4225766 | 5282 | `	return SXRET_OK;` |
|        5 | 5283 | `}` |
|        - | 5284 | `/*` |
|        - | 5285 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5286 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5287 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5288 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5289 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5290 | ` * Refer to the official for more information on this powerful` |
|        - | 5291 | ` * extension.` |
|        - | 5292 | ` */` |
|  4405985 | 5293 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 5294 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5295 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5296 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5297 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 5298 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 5299 | `	)` |
|        5 | 5300 | `{` |
|  4405990 | 5301 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 5302 | `	VmRefObj *pRef;` |
|        - | 5303 | `	/* Check if the referenced object already exists */` |
|  4405990 | 5304 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4405990 | 5305 | `	if( pRef == 0 ){` |
|        - | 5306 | `		/* Create a new entry */` |
|  4359436 | 5307 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  4359436 | 5308 | `		if( pRef == 0 ){` |
|      ! 0 | 5309 | `			return SXERR_MEM;` |
|        - | 5310 | `		}` |
|  4359436 | 5311 | `		pRef->iFlags = iFlags;` |
|        - | 5312 | `		/* Install the entry */` |
|  4359436 | 5313 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  2180324 | 5314 | `	}` |
|  4405990 | 5315 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  4405990 | 5316 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 5317 | `		VmSlot sRef;` |
|        - | 5318 | `		/* Local frame,record referenced entry so that it can` |
|        - | 5319 | `		 * be deleted when we leave this frame.` |
|        - | 5320 | `		 */` |
|   485439 | 5321 | `		sRef.nIdx = nIdx;` |
|   485439 | 5322 | `		sRef.pUserData = pEntry;` |
|   485439 | 5323 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 5324 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 5325 | `		}` |
|   243326 | 5326 | `	}` |
|  4405990 | 5327 | `	if( pEntry ){` |
|        - | 5328 | `		/* Address of the hash-entry */` |
|   531735 | 5329 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|   266474 | 5330 | `	}` |
|  4405990 | 5331 | `	if( pMapEntry ){` |
|        - | 5332 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3820986 | 5333 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1910490 | 5334 | `	}` |
|  4405990 | 5335 | `	return SXRET_OK;` |
|  2203606 | 5336 | `}` |
|        - | 5337 | `/*` |
|        - | 5338 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 5339 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5340 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5341 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5342 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5343 | ` * Refer to the official for more information on this powerful` |
|        - | 5344 | ` * extension.` |
|        - | 5345 | ` */` |
|  4197207 | 5346 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 5347 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5348 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5349 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5350 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 5351 | `	)` |
|        5 | 5352 | `{` |
|        - | 5353 | `	VmRefObj *pRef;` |
|        - | 5354 | `	sxu32 n;` |
|        - | 5355 | `	/* Check if the referenced object already exists */` |
|  4197212 | 5356 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4197212 | 5357 | `	if( pRef == 0 ){` |
|        - | 5358 | `		/* Not such entry */` |
|   485241 | 5359 | `		return SXERR_NOTFOUND;` |
|        - | 5360 | `	}` |
|        - | 5361 | `	/* Remove the desired entry */` |
|  3711976 | 5362 | `	if( pEntry ){` |
|        - | 5363 | `		SyHashEntry **apEntry;` |
|       87 | 5364 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 5365 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      185 | 5366 | `			if( apEntry[n] == pEntry ){` |
|        - | 5367 | `				/* Nullify the entry */` |
|       85 | 5368 | `				apEntry[n] = 0;` |
|        - | 5369 | `				/*` |
|        - | 5370 | `				 * NOTE:` |
|        - | 5371 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 5372 | `				 * we avoid wasting spaces.` |
|        - | 5373 | `				 */` |
|       40 | 5374 | `			}` |
|       95 | 5375 | `		}` |
|       41 | 5376 | `	}` |
|  3711976 | 5377 | `	if( pMapEntry ){` |
|        - | 5378 | `		ph7_hashmap_node **apNode;` |
|  3711894 | 5379 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  7424047 | 5380 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3712158 | 5381 | `			if( apNode[n] == pMapEntry ){` |
|        - | 5382 | `				/* nullify the entry */` |
|  3711894 | 5383 | `				apNode[n] = 0;` |
|  1855944 | 5384 | `			}` |
|  1856081 | 5385 | `		}` |
|  1855944 | 5386 | `	}` |
|  3711976 | 5387 | `	return SXRET_OK;` |
|  2099217 | 5388 | `}` |
|        - | 5389 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 5390 | `/*` |
|        - | 5391 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 5392 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 5393 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 5394 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 5395 | ` * For more information on how to register IO stream devices,please` |
|        - | 5396 | ` * refer to the official documentation.` |
|        - | 5397 | ` */` |
|    30180 | 5398 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 5399 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 5400 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 5401 | `	int nByte              /* *pzDevice length*/` |
|        - | 5402 | `	)` |
|        5 | 5403 | `{` |
|        - | 5404 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 5405 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 5406 | `	SyString sDev,sCur;` |
|        - | 5407 | `	sxu32 n,nEntry;` |
|        - | 5408 | `	int rc;` |
|        - | 5409 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    30185 | 5410 | `	zNext = zCur = zIn = *pzDevice;` |
|    30185 | 5411 | `	zEnd = &zIn[nByte];` |
|  1913598 | 5412 | `	while( zIn < zEnd ){` |
|  1883448 | 5413 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 5414 | `			/* Got one */` |
|       32 | 5415 | `			zNext = &zIn[sizeof("://")-1];` |
|       32 | 5416 | `			break;` |
|        - | 5417 | `		}` |
|        - | 5418 | `		/* Advance the cursor */` |
|  1883418 | 5419 | `		zIn++;` |
|        5 | 5420 | `	}` |
|    30185 | 5421 | `	if( zIn >= zEnd ){` |
|        - | 5422 | `		/* No such scheme,return the default stream */` |
|    30155 | 5423 | `		return pVm->pDefStream;` |
|        - | 5424 | `	}` |
|       32 | 5425 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 5426 | `	/* Remove leading and trailing white spaces */` |
|       32 | 5427 | `	SyStringFullTrim(&sDev);` |
|        - | 5428 | `	/* Perform a linear lookup on the installed stream devices */` |
|       32 | 5429 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       32 | 5430 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|       66 | 5431 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       66 | 5432 | `		pStream = apStream[n];` |
|       66 | 5433 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 5434 | `		/* Perfrom a case-insensitive comparison */` |
|       66 | 5435 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|       66 | 5436 | `		if( rc == 0 ){` |
|        - | 5437 | `			/* Stream device found */` |
|       32 | 5438 | `			*pzDevice = zNext;` |
|       32 | 5439 | `			return pStream;` |
|        - | 5440 | `		}` |
|       19 | 5441 | `	}` |
|        - | 5442 | `	/* No such stream,return NULL */` |
|      ! 0 | 5443 | `	return 0;` |
|    15095 | 5444 | `}` |
|        - | 5445 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 5446 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 5447 |  |
