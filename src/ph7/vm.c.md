# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2355/2831 lines (83.19%)

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
|  1330105 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   77 | `{` |
|  1330110 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       40 |   79 | `		return TRUE;` |
|        - |   80 | `	}` |
|  1330072 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   82 | `		return TRUE;` |
|        - |   83 | `	}` |
|  1330062 |   84 | `	return FALSE;` |
|   665575 |   85 | `}` |
|        - |   86 | `/*` |
|        - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   93 | ` * still go through the existing numeric coercion.` |
|        - |   94 | ` */` |
|   373877 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   96 | `{` |
|        - |   97 | `	SyString sStr;` |
|   373882 |   98 | `	sxu8 bReal = FALSE;` |
|   373882 |   99 | `	const char *zTail = 0;` |
|        - |  100 | `	const char *zEnd;` |
|   373882 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   373868 |  102 | `		return FALSE;` |
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
|   187047 |  119 | `}` |
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
|  1518180 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  139 | `	const SyString *pName,  /* Constant name */` |
|        - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |  142 | `	)` |
|        5 |  143 | `{` |
|  1518185 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|        5 |  145 | `}` |
|        - |  146 | `/*` |
|        - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|        - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|        - |  149 | ` */` |
|  1518250 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|  1518255 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|  1518255 |  165 | `	if( pEntry ){` |
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
|  1518253 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|  1518253 |  190 | `	if( pCons == 0 ){` |
|      ! 0 |  191 | `		return 0;` |
|        - |  192 | `	}` |
|        - |  193 | `	/* Duplicate constant name */` |
|  1518253 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1518253 |  195 | `	if( zDupName == 0 ){` |
|      ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  197 | `		return 0;` |
|        - |  198 | `	}` |
|  1518253 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|  1518253 |  200 | `	if( pFile ){` |
|       71 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       34 |  202 | `	}` |
|  1518253 |  203 | `	pCons->nLine = nLine;` |
|  1518253 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|        - |  205 | `	/* Install the constant */` |
|  1518253 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|  1518253 |  207 | `	pCons->xExpand = xExpand;` |
|  1518253 |  208 | `	pCons->pUserData = pUserData;` |
|  1518253 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1518253 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|  1518253 |  211 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  214 | `		return rc;` |
|        - |  215 | `	}` |
|        - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|  1518253 |  217 | `	return SXRET_OK;` |
|   759130 |  218 | `}` |
|        - |  219 | `/*` |
|        - |  220 | ` * Allocate a new foreign function instance.` |
|        - |  221 | ` * This function return SXRET_OK on success. Any other` |
|        - |  222 | ` * return value indicates failure.` |
|        - |  223 | ` * Please refer to the official documentation for an introduction to` |
|        - |  224 | ` * the foreign function mechanism.` |
|        - |  225 | ` */` |
|  2227896 |  226 | `static sxi32 PH7_NewForeignFunction(` |
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
|  2227901 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  2227901 |  238 | `	if( pFunc == 0 ){` |
|      ! 0 |  239 | `		return SXERR_MEM;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate function name */` |
|  2227901 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2227901 |  243 | `	if( zDup == 0 ){` |
|      ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  245 | `		return SXERR_MEM;` |
|        - |  246 | `	}` |
|        - |  247 | `	/* Zero the structure */` |
|  2227901 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |  249 | `	/* Initialize structure fields */` |
|  2227901 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  2227901 |  251 | `	pFunc->pVm   = pVm;` |
|  2227901 |  252 | `	pFunc->xFunc = xFunc;` |
|  2227901 |  253 | `	pFunc->pUserData = pUserData;` |
|  2227901 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  255 | `	/* Write a pointer to the new function */` |
|  2227901 |  256 | `	*ppOut = pFunc;` |
|  2227901 |  257 | `	return SXRET_OK;` |
|  1113953 |  258 | `}` |
|        - |  259 | `/*` |
|        - |  260 | ` * Install a foreign function and it's associated callback so that` |
|        - |  261 | ` * it can be invoked from the target PHP code.` |
|        - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |  263 | ` * return value indicates failure.` |
|        - |  264 | ` * Please refer to the official documentation for an introduction to` |
|        - |  265 | ` * the foreign function mechanism.` |
|        - |  266 | ` */` |
|  2231300 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  2231305 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  2231305 |  279 | `	if( pEntry ){` |
|     3409 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     3409 |  281 | `		pFunc->pUserData = pUserData;` |
|     3409 |  282 | `		pFunc->xFunc = xFunc;` |
|     3409 |  283 | `		SySetReset(&pFunc->aAux);` |
|        - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|        - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|        - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|        - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|     3409 |  288 | `		pFunc->nMinArg  = 0;` |
|     3409 |  289 | `		pFunc->nMaxArg  = 0;` |
|     3409 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|     3409 |  291 | `		pFunc->bAtLeast = 0;` |
|     3409 |  292 | `		return SXRET_OK;` |
|        - |  293 | `	}` |
|        - |  294 | `	/* Create a new user function */` |
|  2227901 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  2227901 |  296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  297 | `		return rc;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* Install the function in the corresponding hashtable */` |
|  2227901 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  2227901 |  301 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  304 | `		return rc;` |
|        - |  305 | `	}` |
|        - |  306 | `	/* User function successfully installed */` |
|  2227901 |  307 | `	return SXRET_OK;` |
|  1115655 |  308 | `}` |
|        - |  309 | `/*` |
|        - |  310 | ` * Initialize a VM function.` |
|        - |  311 | ` */` |
|  3426038 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |  315 | `	const char *zName,  /* Function name */` |
|        - |  316 | `	sxu32 nByte,        /* zName length */` |
|        - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |  318 | `	void *pUserData     /* Function private data */` |
|        - |  319 | `	)` |
|        5 |  320 | `{` |
|        - |  321 | `	/* Zero the structure */` |
|  3426043 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |  323 | `	/* Initialize structure fields */` |
|        - |  324 | `	/* Arguments container */` |
|  3426043 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |  326 | `	/* Static variable container */` |
|  3426043 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |  328 | `	/* Bytecode container */` |
|  3426043 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |  330 | `    /* Preallocate some instruction slots */` |
|  3426043 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |  332 | `	/* Closure environment */` |
|  3426043 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|  3426043 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  336 | `	/* Declared #[...] attributes */` |
|  3426043 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  3426043 |  338 | `	pFunc->iFlags = iFlags;` |
|  3426043 |  339 | `	pFunc->pUserData = pUserData;` |
|        - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|  3426043 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|  3426043 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|  3421445 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|  1710725 |  346 | `	}else{` |
|        - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|        - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|        - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     4603 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     4603 |  351 | `		if( pFile ){` |
|     4603 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|     2299 |  353 | `		}` |
|        - |  354 | `	}` |
|  3426043 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|  3426043 |  356 | `	return SXRET_OK;` |
|        5 |  357 | `}` |
|        - |  358 | `/*` |
|        - |  359 | ` * Namespace-aware function lookup.` |
|        - |  360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |  361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |  362 | ` */` |
|        - |  363 | `/*` |
|        - |  364 | ` * Install a user defined function in the corresponding VM container.` |
|        - |  365 | ` */` |
|  5857286 |  366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |  367 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  368 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |  369 | `	SyString *pName     /* Function name */` |
|        - |  370 | `	)` |
|        5 |  371 | `{` |
|        - |  372 | `	SyHashEntry *pEntry;` |
|        - |  373 | `	sxi32 rc;` |
|  5857291 |  374 | `	if( pName == 0 ){` |
|        - |  375 | `		/* Use the built-in name */` |
|   523581 |  376 | `		pName = &pFunc->sName;` |
|   261788 |  377 | `	}` |
|        - |  378 | `	/* Check for duplicates (functions with the same name) first */` |
|  5857291 |  379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  5857291 |  380 | `	if( pEntry ){` |
|  2916449 |  381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  2916449 |  382 | `		if( pLink != pFunc ){` |
|        - |  383 | `			/* Link */` |
|       32 |  384 | `			pFunc->pNextName = pLink;` |
|       32 |  385 | `			pEntry->pUserData = pFunc;` |
|       15 |  386 | `		}` |
|  2916449 |  387 | `		return SXRET_OK;` |
|        - |  388 | `	}` |
|        - |  389 | `	/* First time seen */` |
|  2940847 |  390 | `	pFunc->pNextName = 0;` |
|  2940847 |  391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|  2940847 |  392 | `	return rc;` |
|  2928648 |  393 | `}` |
|        - |  394 | `/*` |
|        - |  395 | ` * Install a user defined class in the corresponding VM container.` |
|        - |  396 | ` */` |
|   530930 |  397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |  398 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |  399 | `	ph7_class *pClass /* Target Class */` |
|        - |  400 | `	)` |
|        5 |  401 | `{` |
|   530935 |  402 | `	SyString *pName = &pClass->sName;` |
|        - |  403 | `	SyHashEntry *pEntry;` |
|        - |  404 | `	sxi32 rc;` |
|        - |  405 | `	/* Check for duplicates */` |
|   530935 |  406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   530935 |  407 | `	if( pEntry ){` |
|        3 |  408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |  409 | `		/* Link entry with the same name */` |
|        3 |  410 | `		pClass->pNextName = pLink;` |
|        3 |  411 | `		pEntry->pUserData = pClass;` |
|        3 |  412 | `		return SXRET_OK;` |
|        - |  413 | `	}` |
|   530933 |  414 | `	pClass->pNextName = 0;` |
|        - |  415 | `	/* Perform a simple hashtable insertion */` |
|   530933 |  416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   530933 |  417 | `	return rc;` |
|   265470 |  418 | `}` |
|        - |  419 | `/*` |
|        - |  420 | ` * Instruction builder interface.` |
|        - |  421 | ` */` |
| 98604916 |  422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |  423 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |  424 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |  425 | `	sxi32 iP1,    /* First operand */` |
|        - |  426 | `	sxu32 iP2,    /* Second operand */` |
|        - |  427 | `	void *p3,     /* Third operand */` |
|        - |  428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |  429 | `	)` |
|        5 |  430 | `{` |
|        - |  431 | `	VmInstr sInstr;` |
| 98604921 |  432 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - |  433 | `	sxi32 rc;` |
|        - |  434 | `	/* Fill the VM instruction */` |
| 98604921 |  435 | `	sInstr.iOp = (sxu8)iOp;` |
| 98604921 |  436 | `	sInstr.iP1 = iP1;` |
| 98604921 |  437 | `	sInstr.iP2 = iP2;` |
| 98604921 |  438 | `	sInstr.p3  = p3;` |
|        - |  439 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|        - |  440 | `	 * compiled (that is how they read its text), so the current token IS this` |
|        - |  441 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|        - |  442 | `	 * between statements, hence the range check. */` |
| 98604921 |  443 | `	sInstr.nLine = 0;` |
| 98604921 |  444 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
| 42011183 |  445 | `		sInstr.nLine = pGen->pIn->nLine;` |
| 77599332 |  446 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|        - |  447 | `		/* Past the end (statement tail): blame the last real token. */` |
| 56379021 |  448 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
| 28189508 |  449 | `	}` |
| 98604921 |  450 | `	if( pIndex ){` |
|        - |  451 | `		/* Instruction index in the bytecode array */` |
|  6674719 |  452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|  3337357 |  453 | `	}` |
|        - |  454 | `	/* Finally,record the instruction */` |
| 98604921 |  455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 98604921 |  456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |  458 | `		/* Fall throw */` |
|      ! 0 |  459 | `	}` |
| 98604921 |  460 | `	return rc;` |
|        5 |  461 | `}` |
|        - |  462 | `/*` |
|        - |  463 | ` * Swap the current bytecode container with the given one.` |
|        - |  464 | ` */` |
|  9806032 |  465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |  466 | `{` |
|  9806037 |  467 | `	if( pContainer == 0 ){` |
|        - |  468 | `		/* Point to the default container */` |
|      ! 0 |  469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |  470 | `	}else{` |
|        - |  471 | `		/* Change container */` |
|  9806037 |  472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |  473 | `	}` |
|  9806037 |  474 | `	return SXRET_OK;` |
|        5 |  475 | `}` |
|        - |  476 | `/*` |
|        - |  477 | ` * Return the current bytecode container.` |
|        - |  478 | ` */` |
|  4903016 |  479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |  480 | `{` |
|  4903021 |  481 | `	return pVm->pByteContainer;` |
|        5 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |  485 | ` */` |
|  6881258 |  486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |  487 | `{` |
|        - |  488 | `	VmInstr *pInstr;` |
|  6881263 |  489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|  6881263 |  490 | `	return pInstr;` |
|        5 |  491 | `}` |
|        - |  492 | `/*` |
|        - |  493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |  494 | ` */` |
| 54300056 |  495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |  496 | `{` |
| 54300061 |  497 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |  498 | `}` |
|        - |  499 | `/*` |
|        - |  500 | ` * Pop the last VM instruction.` |
|        - |  501 | ` */` |
|  5411684 |  502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |  503 | `{` |
|  5411689 |  504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |  505 | `}` |
|        - |  506 | `/*` |
|        - |  507 | ` * Peek the last VM instruction.` |
|        - |  508 | ` */` |
| 20499848 |  509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |  510 | `{` |
| 20499853 |  511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |  512 | `}` |
|  1716324 |  513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |  514 | `{` |
|        - |  515 | `	VmInstr *aInstr;` |
|        - |  516 | `	sxu32 n;` |
|  1716329 |  517 | `	n = SySetUsed(pVm->pByteContainer);` |
|  1716329 |  518 | `	if( n < 2 ){` |
|      ! 0 |  519 | `		return 0;` |
|        - |  520 | `	}` |
|  1716329 |  521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|  1716329 |  522 | `	return &aInstr[n - 2];` |
|   858167 |  523 | `}` |
|        - |  524 | `/*` |
|        - |  525 | ` * Allocate a new virtual machine frame.` |
|        - |  526 | ` */` |
|   115225 |  527 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|        - |  528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |  530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  531 | `	)` |
|        5 |  532 | `{` |
|        - |  533 | `	VmFrame *pFrame;` |
|        - |  534 | `	/* Allocate a new vm frame */` |
|   115230 |  535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   115230 |  536 | `	if( pFrame == 0 ){` |
|      ! 0 |  537 | `		return 0;` |
|        - |  538 | `	}` |
|        - |  539 | `	/* Zero the structure */` |
|   115230 |  540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |  541 | `	/* Initialize frame fields */` |
|   115230 |  542 | `	pFrame->pUserData = pUserData;` |
|   115230 |  543 | `	pFrame->pThis = pThis;` |
|   115230 |  544 | `	pFrame->pVm = pVm;` |
|   115230 |  545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   115230 |  546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   115230 |  547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   115230 |  548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   115230 |  549 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|        - |  550 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|        - |  551 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   115230 |  552 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   115230 |  553 | `	return pFrame;` |
|    57721 |  554 | `}` |
|        - |  555 | `/* Forward declaration */` |
|        - |  556 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|        - |  557 | `/*` |
|        - |  558 | ` * Enter a VM frame.` |
|        - |  559 | ` */` |
|   114629 |  560 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|        - |  561 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  562 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |  563 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  564 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |  565 | `	)` |
|        5 |  566 | `{` |
|        - |  567 | `	VmFrame *pFrame;` |
|        - |  568 | `	/* Allocate a new frame */` |
|   114634 |  569 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   114634 |  570 | `	if( pFrame == 0 ){` |
|      ! 0 |  571 | `		return SXERR_MEM;` |
|        - |  572 | `	}` |
|        - |  573 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   114634 |  574 | `	pFrame->nCallLine = pVm->nCurLine;` |
|        - |  575 | `	/* Link to the list of active VM frame */` |
|   114634 |  576 | `	pFrame->pParent = pVm->pFrame;` |
|   114634 |  577 | `	pVm->pFrame = pFrame;` |
|   114634 |  578 | `	if( ppFrame ){` |
|        - |  579 | `		/* Write a pointer to the new VM frame */` |
|   110738 |  580 | `		*ppFrame = pFrame;` |
|    55470 |  581 | `	}` |
|   114634 |  582 | `	return SXRET_OK;` |
|    57423 |  583 | `}` |
|        - |  584 | `/*` |
|        - |  585 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |  586 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |  587 | ` * information.` |
|        - |  588 | ` */` |
|       64 |  589 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        5 |  590 | `{` |
|        - |  591 | `	VmFrame *pTarget,*pFrame;` |
|       69 |  592 | `	SyHashEntry *pEntry = 0;` |
|        - |  593 | `	sxi32 rc;` |
|        - |  594 | `	/* Point to the upper frame */` |
|       69 |  595 | `	pFrame = pVm->pFrame;` |
|       69 |  596 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       69 |  597 | `	pTarget = pFrame;` |
|       69 |  598 | `	pFrame = pTarget->pParent;` |
|       69 |  599 | `	while( pFrame ){` |
|       69 |  600 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  601 | `			/* Query the current frame */` |
|       69 |  602 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       69 |  603 | `			if( pEntry ){` |
|        - |  604 | `				/* Variable found */` |
|       69 |  605 | `				break;` |
|        - |  606 | `			}` |
|      ! 0 |  607 | `		}` |
|        - |  608 | `		/* Point to the upper frame */` |
|      ! 0 |  609 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  610 | `	}` |
|       69 |  611 | `	if( pEntry == 0 ){` |
|        - |  612 | `		/* Inexistant variable */` |
|      ! 0 |  613 | `		return SXERR_NOTFOUND;` |
|        - |  614 | `	}` |
|        - |  615 | `	/* Link to the current frame */` |
|       69 |  616 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       69 |  617 | `	if( rc == SXRET_OK ){` |
|        - |  618 | `		sxu32 nIdx;` |
|       69 |  619 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       69 |  620 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       32 |  621 | `	}` |
|       69 |  622 | `	return rc;` |
|       37 |  623 | `}` |
|        - |  624 | `/*` |
|        - |  625 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|        - |  626 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|        - |  627 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|        - |  628 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|        - |  629 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|        - |  630 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|        - |  631 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|        - |  632 | ` */` |
|   111163 |  633 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 |  634 | `{` |
|   111168 |  635 | `	if( pVm->pResumeFrame == pFrame ){` |
|        3 |  636 | `		pVm->pResumeFrame = 0;` |
|        1 |  637 | `	}` |
|   111168 |  638 | `}` |
|        - |  639 | `/*` |
|        - |  640 | ` * Leave the top-most active frame.` |
|        - |  641 | ` */` |
|   110311 |  642 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |  643 | `{` |
|   110316 |  644 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   110316 |  645 | `	if( pCurFrame ){` |
|        - |  646 | `		/* Unlink from the list of active VM frame */` |
|   110316 |  647 | `		pVm->pFrame = pCurFrame->pParent;` |
|   110316 |  648 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  649 | `			VmSlot  *aSlot;` |
|        - |  650 | `			sxu32 n;` |
|        - |  651 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   105306 |  652 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   610752 |  653 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |  654 | `				/* Unset the local variable */` |
|   505451 |  655 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   253349 |  656 | `			}` |
|        - |  657 | `			/* Remove local reference */` |
|   105306 |  658 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   610836 |  659 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   505535 |  660 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   253391 |  661 | `			}` |
|    52754 |  662 | `		}` |
|        - |  663 | `		/* Release internal containers */` |
|   110316 |  664 | `		SyHashRelease(&pCurFrame->hVar);` |
|   110316 |  665 | `		SySetRelease(&pCurFrame->sArg);` |
|   110316 |  666 | `		SySetRelease(&pCurFrame->sLocal);` |
|   110316 |  667 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |  668 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|        - |  669 | `		 * containers above — released for every frame, including transparent` |
|        - |  670 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   110316 |  671 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|        - |  672 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   110316 |  673 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|        - |  674 | `		/* Release the whole structure */` |
|   110316 |  675 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    55259 |  676 | `	}` |
|   110316 |  677 | `}` |
|        - |  678 | `/*` |
|        - |  679 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|        - |  680 | ` * active frame's local-teardown set records it (walking the parent chain` |
|        - |  681 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|        - |  682 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|        - |  683 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|        - |  684 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|        - |  685 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|        - |  686 | ` */` |
|        - |  687 | `/*` |
|        - |  688 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|        - |  689 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|        - |  690 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|        - |  691 | ` *` |
|        - |  692 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|        - |  693 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|        - |  694 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|        - |  695 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|        - |  696 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|        - |  697 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|        - |  698 | ` */` |
|     6916 |  699 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        4 |  700 | `{` |
|        - |  701 | `	VmFrame *pFrame;` |
|    13866 |  702 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     6962 |  703 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  704 | `		sxu32 n;` |
|     7022 |  705 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|       76 |  706 | `			if( aSlot[n].nIdx == nIdx ){` |
|        - |  707 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       13 |  708 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       13 |  709 | `				(void)SySetPop(&pFrame->sLocal);` |
|       13 |  710 | `				return TRUE; /* Slot owned by exactly one frame */` |
|        - |  711 | `			}` |
|       34 |  712 | `		}` |
|     3477 |  713 | `	}` |
|     6908 |  714 | `	return FALSE;` |
|     3462 |  715 | `}` |
|      102 |  716 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        3 |  717 | `{` |
|        - |  718 | `	VmRefObj *pRef;` |
|      105 |  719 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|      105 |  720 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|      105 |  721 | `	if( pRef ){` |
|      105 |  722 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       51 |  723 | `	}` |
|      105 |  724 | `}` |
|        - |  725 | `/*` |
|        - |  726 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |  727 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |  728 | ` * should be skipped when looking for the real execution context.` |
|        - |  729 | ` */` |
| 10688328 |  730 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |  731 | `{` |
| 10714603 |  732 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|    26275 |  733 | `		pFrame = pFrame->pParent;` |
|        5 |  734 | `	}` |
| 10688333 |  735 | `	return pFrame;` |
|        5 |  736 | `}` |
|        - |  737 | `/*` |
|        - |  738 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|        - |  739 | ` * may be several frames below the frame that caught the exception — must resume at` |
|        - |  740 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|        - |  741 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|        - |  742 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|        - |  743 | ` *` |
|        - |  744 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|        - |  745 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|        - |  746 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|        - |  747 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|        - |  748 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|        - |  749 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|        - |  750 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|        - |  751 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|        - |  752 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|        - |  753 | ` * VmSkipExceptionFrames yields its real body frame.` |
|        - |  754 | ` *` |
|        - |  755 | ` * This replaces the older "is there a resumable try frame here" test` |
|        - |  756 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|        - |  757 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|        - |  758 | ` * catching frame was not the nearest try (ROOT B).` |
|        - |  759 | ` */` |
|     3054 |  760 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|        5 |  761 | `{` |
|     3059 |  762 | `	if( pVm->pResumeFrame == 0 ){` |
|       17 |  763 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|        - |  764 | `	}` |
|        - |  765 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|        - |  766 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|        - |  767 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|        - |  768 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|        - |  769 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|        - |  770 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|        - |  771 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|        - |  772 | `	 * exec's resume site match and land. */` |
|     3040 |  773 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|     2556 |  774 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|     2064 |  775 | `	 \|\| pVm->iResumePc == 0 ){` |
|        - |  776 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|        - |  777 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|        - |  778 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|        - |  779 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|        - |  780 | `		 * never match a real frame. */` |
|      999 |  781 | `		return FALSE;` |
|        - |  782 | `	}` |
|        - |  783 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|        - |  784 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|        - |  785 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|        - |  786 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|        - |  787 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|        - |  788 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|        - |  789 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|        - |  790 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|        - |  791 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|        - |  792 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|        - |  793 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|        - |  794 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|        - |  795 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|        - |  796 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|        - |  797 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|        - |  798 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|        - |  799 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|        - |  800 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|        - |  801 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|        - |  802 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|        - |  803 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|     3484 |  804 | `	while( pVm->pFrame != pEntryFrame` |
|     3965 |  805 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|     2521 |  806 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|      835 |  807 | `		VmLeaveFrame(&(*pVm));` |
|        5 |  808 | `	}` |
|     2051 |  809 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|     2051 |  810 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|        - |  811 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|        - |  812 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|        - |  813 | `	 * point router must not re-fire it after this resume. */` |
|     2051 |  814 | `	pVm->nBoundaryRc = 0;` |
|     2051 |  815 | `	return TRUE;` |
|     1532 |  816 | `}` |
|        - |  817 | `/*` |
|        - |  818 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|        - |  819 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|        - |  820 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|        - |  821 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|        - |  822 | ` * nested try/finally inside a catch body). Each finally runs with` |
|        - |  823 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|        - |  824 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|        - |  825 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|        - |  826 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|        - |  827 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|        - |  828 | ` */` |
|        - |  829 | `/*` |
|        - |  830 | ` * BYTECODE stage 2b — per-activation try state.` |
|        - |  831 | ` *` |
|        - |  832 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|        - |  833 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|        - |  834 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|        - |  835 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|        - |  836 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|        - |  837 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|        - |  838 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|        - |  839 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|        - |  840 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|        - |  841 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|        - |  842 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|        - |  843 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|        - |  844 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|        - |  845 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|        - |  846 | ` * activation.` |
|        - |  847 | ` */` |
|     2776 |  848 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  849 | `{` |
|     2781 |  850 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|     2781 |  851 | `	if( pClone == 0 ){` |
|      ! 0 |  852 | `		return 0;` |
|        - |  853 | `	}` |
|     2781 |  854 | `	*pClone = *pCompiled;` |
|     2781 |  855 | `	pClone->pCompiled = pCompiled;` |
|     2781 |  856 | `	pClone->iFinallyDone = 0;` |
|     2781 |  857 | `	pClone->iInCatch = 0;` |
|     2781 |  858 | `	pClone->pInflight = 0;` |
|     2781 |  859 | `	pClone->pFrame = 0;` |
|     2781 |  860 | `	return pClone;` |
|     1393 |  861 | `}` |
|     5476 |  862 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|        5 |  863 | `{` |
|     5481 |  864 | `	if( pExc && pExc->pCompiled ){` |
|        - |  865 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|        - |  866 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|        - |  867 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|        - |  868 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|     2769 |  869 | `		if( pExc->pInflight ){` |
|      ! 0 |  870 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|      ! 0 |  871 | `			pExc->pInflight = 0;` |
|      ! 0 |  872 | `		}` |
|     2769 |  873 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|     1382 |  874 | `	}` |
|     5481 |  875 | `}` |
|        - |  876 | `/*` |
|        - |  877 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|        - |  878 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|        - |  879 | ` */` |
|      468 |  880 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|        5 |  881 | `{` |
|      473 |  882 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|        5 |  883 | `}` |
|        - |  884 | `/*` |
|        - |  885 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|        - |  886 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|        - |  887 | ` * handlers). The set itself is reset by the caller.` |
|        - |  888 | ` */` |
|      546 |  889 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|        5 |  890 | `{` |
|      551 |  891 | `	sxu32 n = SySetUsed(pSet);` |
|      551 |  892 | `	if( n > 0 ){` |
|      ! 0 |  893 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|        - |  894 | `		sxu32 i;` |
|      ! 0 |  895 | `		for( i = 0; i < n; i++ ){` |
|      ! 0 |  896 | `			VmExcRelease(pVm,ap[i]);` |
|      ! 0 |  897 | `		}` |
|      ! 0 |  898 | `	}` |
|      551 |  899 | `}` |
|        - |  900 | `/*` |
|        - |  901 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|        - |  902 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|        - |  903 | ` * only carries the compiled pointer.` |
|        - |  904 | ` */` |
|       66 |  905 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  906 | `{` |
|       71 |  907 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       71 |  908 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|       71 |  909 | `	while( n > 0 ){` |
|       71 |  910 | `		n--;` |
|       71 |  911 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|       71 |  912 | `			return ap[n];` |
|        - |  913 | `		}` |
|      ! 0 |  914 | `	}` |
|      ! 0 |  915 | `	return 0;` |
|       38 |  916 | `}` |
|   263079 |  917 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |  918 | `{` |
|        - |  919 | `	sxu32 nUsed;` |
|   263084 |  920 | `	sxi32 rcOut = SXRET_OK;` |
|   263148 |  921 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|       68 |  922 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       68 |  923 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|       68 |  924 | `		(void)SySetPop(&pVm->aException);` |
|       68 |  925 | `		pExc->pFrame = 0;` |
|        - |  926 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|        - |  927 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|        - |  928 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|        - |  929 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|        - |  930 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|       68 |  931 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       68 |  932 | `			VmLeaveFrame(&(*pVm));` |
|       32 |  933 | `		}` |
|       92 |  934 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        - |  935 | `			sxi32 rcF;` |
|       52 |  936 | `			pExc->iFinallyDone = 1;` |
|       52 |  937 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|       52 |  938 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|       52 |  939 | `			if( rcF == SXERR_ABORT ){` |
|      ! 0 |  940 | `				return SXERR_ABORT;` |
|        - |  941 | `			}` |
|       52 |  942 | `			if( rcF == PH7_EXCEPTION ){` |
|        - |  943 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|        - |  944 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|        - |  945 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|        5 |  946 | `				rcOut = PH7_EXCEPTION;` |
|        2 |  947 | `			}` |
|       28 |  948 | `		}else{` |
|       19 |  949 | `			VmExcRelease(&(*pVm),pExc);` |
|        - |  950 | `		}` |
|        4 |  951 | `	}` |
|   263084 |  952 | `	return rcOut;` |
|   131648 |  953 | `}` |
|        - |  954 | `/*` |
|        - |  955 | ` * Drop a body frame's pending catch/finally return: clear the flag and release` |
|        - |  956 | ` * the slot value. Safe on a frame with no pending return (the slot is then an` |
|        - |  957 | ` * empty MEMOBJ_NULL value and the release is a no-op).` |
|        - |  958 | ` */` |
|   159338 |  959 | `PH7_PRIVATE void VmClearFrameReturn(VmFrame *pFrame)` |
|        5 |  960 | `{` |
|   159343 |  961 | `	pFrame->bHasRet = 0;` |
|   159343 |  962 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   159343 |  963 | `}` |
|        - |  964 | `/*` |
|        - |  965 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|        - |  966 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|        - |  967 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|        - |  968 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|        - |  969 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|        - |  970 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|        - |  971 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|        - |  972 | ` */` |
|      162 |  973 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|        5 |  974 | `{` |
|      167 |  975 | `	if( pResult ){` |
|      167 |  976 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|       81 |  977 | `	}` |
|      167 |  978 | `	VmClearFrameReturn(pEntryFrame);` |
|      167 |  979 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|      ! 0 |  980 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  981 | `	}` |
|      167 |  982 | `}` |
|        - |  983 | `/*` |
|        - |  984 | ` * Compare two functions signature and return the comparison result.` |
|        - |  985 | ` */` |
|     1182 |  986 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |  987 | `{` |
|     1183 |  988 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|     1183 |  989 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|     1183 |  990 | `	const char *zSin = pSecond->zString;` |
|     1183 |  991 | `	const char *zFin = pFirst->zString;` |
|     1183 |  992 | `	const char *zPtr = zFin;` |
|      591 |  993 | `	for(;;){` |
|     1183 |  994 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      592 |  995 | `			break;` |
|        - |  996 | `		}` |
|      ! 0 |  997 | `		if( zFin[0] != zSin[0] ){` |
|        - |  998 | `			/* mismatch */` |
|      ! 0 |  999 | `			break;` |
|        - | 1000 | `		}` |
|      ! 0 | 1001 | `		zFin++;` |
|      ! 0 | 1002 | `		zSin++;` |
|      ! 0 | 1003 | `	}` |
|     1183 | 1004 | `	return (int)(zFin-zPtr);` |
|        1 | 1005 | `}` |
|        - | 1006 | `/*` |
|        - | 1007 | ` * Select the appropriate VM function for the current call context.` |
|        - | 1008 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - | 1009 | ` * introduced by the version 2 of the PH7 engine.` |
|        - | 1010 | ` * Refer to the official documentation for more information.` |
|        - | 1011 | ` */` |
|      240 | 1012 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|        - | 1013 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1014 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - | 1015 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - | 1016 | `	int nArg             /* Total number of passed arguments  */` |
|        - | 1017 | `	)` |
|        2 | 1018 | `{` |
|        - | 1019 | `	int iTarget,i,j,iCur,iMax;` |
|        - | 1020 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - | 1021 | `	ph7_vm_func *pLink;` |
|        - | 1022 | `	SyString sArgSig;` |
|        - | 1023 | `	SyBlob sSig;` |
|        - | 1024 |  |
|      242 | 1025 | `	pLink = pList;` |
|      242 | 1026 | `	i = 0;` |
|        - | 1027 | `	/* Put functions expecting the same number of passed arguments */` |
|     1452 | 1028 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1422 | 1029 | `		if( pLink == 0 ){` |
|      212 | 1030 | `			break;` |
|        - | 1031 | `		}` |
|     1212 | 1032 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - | 1033 | `			/* Candidate for overloading */` |
|     1212 | 1034 | `			apSet[i++] = pLink;` |
|      605 | 1035 | `		}` |
|        - | 1036 | `		/* Point to the next entry */` |
|     1212 | 1037 | `		pLink = pLink->pNextName;` |
|        2 | 1038 | `	}` |
|      242 | 1039 | `	if( i < 1 ){` |
|        - | 1040 | `		/* No candidates,return the head of the list */` |
|      ! 0 | 1041 | `		return pList;` |
|        - | 1042 | `	}` |
|      242 | 1043 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - | 1044 | `		/* Return the only candidate */` |
|       16 | 1045 | `		return apSet[0];` |
|        - | 1046 | `	}` |
|        - | 1047 | `	/* Calculate function signature */` |
|      227 | 1048 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      453 | 1049 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      227 | 1050 | `		int c = 'n'; /* null */` |
|      227 | 1051 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1052 | `			/* Hashmap */` |
|      ! 0 | 1053 | `			c = 'h';` |
|      227 | 1054 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - | 1055 | `			/* bool */` |
|       85 | 1056 | `			c = 'b';` |
|      185 | 1057 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - | 1058 | `			/* int */` |
|       45 | 1059 | `			c = 'i';` |
|      121 | 1060 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - | 1061 | `			/* String */` |
|       87 | 1062 | `			c = 's';` |
|       56 | 1063 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - | 1064 | `			/* Float */` |
|       11 | 1065 | `			c = 'f';` |
|        8 | 1066 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - | 1067 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|      ! 0 | 1068 | `			int marker = 'o';` |
|      ! 0 | 1069 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 | 1070 | `			SyString *pName = &pClass->sName;` |
|      ! 0 | 1071 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|      ! 0 | 1072 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 | 1073 | `			c = -1;` |
|      ! 0 | 1074 | `		}` |
|      227 | 1075 | `		if( c > 0 ){` |
|      227 | 1076 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      113 | 1077 | `		}` |
|      114 | 1078 | `	}` |
|      227 | 1079 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      227 | 1080 | `	iTarget = 0;` |
|      227 | 1081 | `	iMax = -1;` |
|        - | 1082 | `	/* Select the appropriate function */` |
|     1409 | 1083 | `	for( j = 0 ; j < i ; j++ ){` |
|        - | 1084 | `		/* Compare the two signatures */` |
|     1183 | 1085 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|     1183 | 1086 | `		if( iCur > iMax ){` |
|      227 | 1087 | `			iMax = iCur;` |
|      227 | 1088 | `			iTarget = j;` |
|      113 | 1089 | `		}` |
|      592 | 1090 | `	}` |
|      227 | 1091 | `	SyBlobRelease(&sSig);` |
|        - | 1092 | `	/* Appropriate function for the current call context */` |
|      227 | 1093 | `	return apSet[iTarget];` |
|      122 | 1094 | `}` |
|        - | 1095 | `/* Forward declaration */` |
|        - | 1096 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - | 1097 | `/*` |
|        - | 1098 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|        - | 1099 | ` * safely across a pool reallocation.` |
|        - | 1100 | ` *` |
|        - | 1101 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|        - | 1102 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|        - | 1103 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|        - | 1104 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|        - | 1105 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|        - | 1106 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|        - | 1107 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|        - | 1108 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|        - | 1109 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|        - | 1110 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|        - | 1111 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|        - | 1112 | ` */` |
|    34964 | 1113 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|        5 | 1114 | `{` |
|        - | 1115 | `	ph7_value sVal;` |
|    34969 | 1116 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|        - | 1117 | `	sxi32 rc;` |
|    34969 | 1118 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    34969 | 1119 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|        - | 1120 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|    34969 | 1121 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    34969 | 1122 | `	if( *ppMemObj ){` |
|    34969 | 1123 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|    17482 | 1124 | `	}` |
|    34969 | 1125 | `	PH7_MemObjRelease(&sVal);` |
|    34969 | 1126 | `	return rc;` |
|        5 | 1127 | `}` |
|        - | 1128 | `/*` |
|        - | 1129 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - | 1130 | ` * it can be instanciated from the executed PHP script.` |
|        - | 1131 | ` */` |
|        - | 1132 | `/*` |
|        - | 1133 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - | 1134 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - | 1135 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - | 1136 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - | 1137 | ` * properties register their enforcement slot. It is factored out of` |
|        - | 1138 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - | 1139 | ` * reuse without re-installing the (compile-time) methods.` |
|        - | 1140 | ` */` |
|   707122 | 1141 | `static sxi32 VmMountUserClassAttrs(` |
|        - | 1142 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1143 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - | 1144 | `	)` |
|        5 | 1145 | `{` |
|        - | 1146 | `	ph7_class_attr *pAttr;` |
|        - | 1147 | `	SyHashEntry *pEntry;` |
|        - | 1148 | `	/* Reset the loop cursor */` |
|   707127 | 1149 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - | 1150 | `	/* Process only static and constant attribute */` |
|  2902377 | 1151 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1152 | `		/* Extract the current attribute */` |
|  2195259 | 1153 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2195254 | 1154 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  1439853 | 1155 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   342662 | 1156 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|        - | 1157 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|        - | 1158 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|        - | 1159 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|        - | 1160 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|        - | 1161 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|        - | 1162 | `			 * user catch, and initializers referencing constants of a class` |
|        - | 1163 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|        - | 1164 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|        - | 1165 | `			 * %s as value for class constant" fatal without any access). */` |
|   683565 | 1166 | `			continue;` |
|        - | 1167 | `		}` |
|  1511699 | 1168 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 1169 | `			ph7_value *pMemObj;` |
|    24817 | 1170 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|        - | 1171 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|        - | 1172 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|        - | 1173 | `				 * re-mount pass, so VM reuse still re-evaluates. */` |
|      856 | 1174 | `				continue;` |
|        - | 1175 | `			}` |
|        - | 1176 | `			/* Reserve a memory object for this constant/static attribute */` |
|    23963 | 1177 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    23963 | 1178 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1179 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1180 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 | 1181 | `					&pClass->sName,&pAttr->sName` |
|        - | 1182 | `					);` |
|      ! 0 | 1183 | `				return SXERR_MEM;` |
|        - | 1184 | `			}` |
|    23963 | 1185 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1186 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1187 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|        - | 1188 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|    23955 | 1189 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        - | 1190 | `				sxi32 rcExec;` |
|    23955 | 1191 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    23955 | 1192 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|    23955 | 1193 | `				pVm->nConstEvalDepth++;` |
|    23955 | 1194 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    23955 | 1195 | `				pVm->nConstEvalDepth--;` |
|    23955 | 1196 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|    23955 | 1197 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    23955 | 1198 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1199 | `					/* The initializer raised (self-referencing constant, or a` |
|        - | 1200 | `					 * throwing enum-case reference): park it for the fetch-point` |
|        - | 1201 | `					 * router — user classes mount mid-execution, so the throw` |
|        - | 1202 | `					 * lands catchably at the declaration site. */` |
|      ! 0 | 1203 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|    23955 | 1204 | `				}else if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 1205 | `					/* A nested evaluation detected a self-referencing constant:` |
|        - | 1206 | `					 * raise it at this, the outermost level. */` |
|      ! 0 | 1207 | `					VmBoundaryPark(&(*pVm),VmConstCycleThrow(&(*pVm)));` |
|      ! 0 | 1208 | `				}` |
|        - | 1209 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - | 1210 | `				 * against the declared type. A mismatch is a non-catchable` |
|        - | 1211 | `				 * fatal, raised here at definition time (matching PHP). */` |
|    23950 | 1212 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|    11980 | 1213 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|       35 | 1214 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|       35 | 1215 | `					if( rcType != SXRET_OK ){` |
|        6 | 1216 | `						return rcType;` |
|        - | 1217 | `					}` |
|       14 | 1218 | `				}` |
|    11973 | 1219 | `			}` |
|        - | 1220 | `			/* Record attribute index */` |
|    23959 | 1221 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - | 1222 | `			/* Install static attribute in the reference table */` |
|    23959 | 1223 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1224 | `			/* If this is a typed static property, register the slot so the` |
|        - | 1225 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - | 1226 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - | 1227 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - | 1228 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - | 1229 | `			 * already enforced above, so they need no store-time slot. */` |
|    23954 | 1230 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    12005 | 1231 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       21 | 1232 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       21 | 1233 | `				if( pVmAttrS == 0 ){` |
|      ! 0 | 1234 | `					return SXERR_MEM;` |
|        - | 1235 | `				}` |
|       21 | 1236 | `				pVmAttrS->pAttr = pAttr;` |
|       21 | 1237 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       21 | 1238 | `				pVmAttrS->iState = 0;` |
|       21 | 1239 | `				pVmAttrS->pOwner = pClass;` |
|        - | 1240 | `				/* Static typed property with no default starts uninitialized` |
|        - | 1241 | `				 * (constants are already excluded by the enclosing condition). */` |
|       21 | 1242 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        6 | 1243 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 | 1244 | `				}` |
|       21 | 1245 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 | 1246 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 | 1247 | `					return SXERR_MEM;` |
|        - | 1248 | `				}` |
|        9 | 1249 | `			}` |
|    11977 | 1250 | `		}` |
|        5 | 1251 | `	}` |
|   707123 | 1252 | `	return SXRET_OK;` |
|   353566 | 1253 | `}` |
|   706030 | 1254 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - | 1255 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1256 | `	ph7_class *pClass /* Class to be mounted */` |
|        - | 1257 | `	)` |
|        5 | 1258 | `{` |
|        - | 1259 | `	ph7_class_method *pMeth;` |
|        - | 1260 | `	SyHashEntry *pEntry;` |
|        - | 1261 | `	sxi32 rc;` |
|        - | 1262 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   706035 | 1263 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   706035 | 1264 | `	if( rc != SXRET_OK ){` |
|        6 | 1265 | `		return rc;` |
|        - | 1266 | `	}` |
|        - | 1267 | `	/* Install class methods */` |
|   706031 | 1268 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - | 1269 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - | 1270 | `		 */` |
|   309585 | 1271 | `		return SXRET_OK;` |
|        - | 1272 | `	}` |
|        - | 1273 | `	/* Create constructor alias if not yet done */` |
|   396451 | 1274 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - | 1275 | `		/* User constructor with the same base class name */` |
|    59361 | 1276 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|    59361 | 1277 | `		if( pEntry ){` |
|      ! 0 | 1278 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 1279 | `			/* Create the alias */` |
|      ! 0 | 1280 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 | 1281 | `		}` |
|    29678 | 1282 | `	}` |
|        - | 1283 | `	/* Install the methods now */` |
|   396451 | 1284 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  5938616 | 1285 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5343947 | 1286 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5343947 | 1287 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  5333715 | 1288 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  5333715 | 1289 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1290 | `				return rc;` |
|        - | 1291 | `			}` |
|  2666855 | 1292 | `		}` |
|        5 | 1293 | `	}` |
|        - | 1294 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   396451 | 1295 | `	pClass->bMounted = TRUE;` |
|   396451 | 1296 | `	return SXRET_OK;` |
|   353020 | 1297 | `}` |
|        - | 1298 | `/*` |
|        - | 1299 | ` * Allocate a private frame for attributes of the given` |
|        - | 1300 | ` * class instance (Object in the PHP jargon).` |
|        - | 1301 | ` */` |
|     8824 | 1302 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - | 1303 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 1304 | `	ph7_class_instance *pObj /* Class instance */` |
|        - | 1305 | `	)` |
|        5 | 1306 | `{` |
|     8829 | 1307 | `	ph7_class *pClass = pObj->pClass;` |
|        - | 1308 | `	ph7_class_attr *pAttr;` |
|        - | 1309 | `	SyHashEntry *pEntry;` |
|        - | 1310 | `	sxi32 rc;` |
|        - | 1311 | `	/* Install class attribute in the private frame associated with this instance */` |
|     8829 | 1312 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    46485 | 1313 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1314 | `		VmClassAttr *pVmAttr;` |
|        - | 1315 | `		/* Extract the current attribute */` |
|    37661 | 1316 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    37661 | 1317 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|    37661 | 1318 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1319 | `			return SXERR_MEM;` |
|        - | 1320 | `		}` |
|    37661 | 1321 | `		pVmAttr->pAttr = pAttr;` |
|    37661 | 1322 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - | 1323 | `			ph7_value *pMemObj;` |
|        - | 1324 | `			/* Reserve a memory object for this attribute */` |
|    29601 | 1325 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    29601 | 1326 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1327 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1328 | `				return SXERR_MEM;` |
|        - | 1329 | `			}` |
|    29601 | 1330 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|    29601 | 1331 | `			pVmAttr->iState = 0;` |
|    29601 | 1332 | `			pVmAttr->pOwner = pClass;` |
|    29601 | 1333 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1334 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1335 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|        - | 1336 | `				 * against the declaring class (no method frame here). */` |
|    10793 | 1337 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|    10793 | 1338 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    10793 | 1339 | `				VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    10793 | 1340 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    24207 | 1341 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1342 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - | 1343 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|      403 | 1344 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      199 | 1345 | `			}` |
|    29601 | 1346 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|    29601 | 1347 | `			if( rc != SXRET_OK ){` |
|        - | 1348 | `				VmSlot sSlot;` |
|        - | 1349 | `				/* Restore memory object */` |
|      ! 0 | 1350 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1351 | `				sSlot.pUserData = 0;` |
|      ! 0 | 1352 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1353 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1354 | `				return SXERR_MEM;` |
|        - | 1355 | `			}` |
|        - | 1356 | `			/* Install attribute in the reference table */` |
|    29601 | 1357 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1358 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - | 1359 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - | 1360 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|    29601 | 1361 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      617 | 1362 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      617 | 1363 | `				if( rc != SXRET_OK ){` |
|        - | 1364 | `					VmSlot sSlot;` |
|      ! 0 | 1365 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 | 1366 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1367 | `					sSlot.pUserData = 0;` |
|      ! 0 | 1368 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1369 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1370 | `					return SXERR_MEM;` |
|        - | 1371 | `				}` |
|      306 | 1372 | `			}` |
|    14803 | 1373 | `		}else{` |
|        - | 1374 | `			/* Install static/constant attribute */` |
|     8065 | 1375 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|     8065 | 1376 | `			pVmAttr->iState = 0;` |
|     8065 | 1377 | `			pVmAttr->pOwner = pClass;` |
|     8065 | 1378 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     8065 | 1379 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1380 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1381 | `				return SXERR_MEM;` |
|        - | 1382 | `			}` |
|        - | 1383 | `		}` |
|        5 | 1384 | `	}` |
|     8829 | 1385 | `	return SXRET_OK;` |
|     4417 | 1386 | `}` |
|        - | 1387 | `/*` |
|        - | 1388 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|        - | 1389 | ` * stdClass for now; the future general-dynamic-props work turns` |
|        - | 1390 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|        - | 1391 | ` */` |
|       62 | 1392 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|        2 | 1393 | `{` |
|       64 | 1394 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|        2 | 1395 | `}` |
|        - | 1396 | `/*` |
|        - | 1397 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|        - | 1398 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|        - | 1399 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|        - | 1400 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|        - | 1401 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|        - | 1402 | ` */` |
|        2 | 1403 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|        1 | 1404 | `{` |
|        5 | 1405 | `	while( pClass ){` |
|        3 | 1406 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|        - | 1407 | `		sxu32 n;` |
|        3 | 1408 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|      ! 0 | 1409 | `			if( aAttr[n].sName.nByte == nName` |
|      ! 0 | 1410 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|      ! 0 | 1411 | `				return TRUE;` |
|        - | 1412 | `			}` |
|      ! 0 | 1413 | `		}` |
|        3 | 1414 | `		pClass = pClass->pBase;` |
|        1 | 1415 | `	}` |
|        3 | 1416 | `	return FALSE;` |
|        2 | 1417 | `}` |
|        - | 1418 | `/*` |
|        - | 1419 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|        - | 1420 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|        - | 1421 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|        - | 1422 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|        - | 1423 | ` *` |
|        - | 1424 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|        - | 1425 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|        - | 1426 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|        - | 1427 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|        - | 1428 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|        - | 1429 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|        - | 1430 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|        - | 1431 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|        - | 1432 | ` */` |
|      146 | 1433 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|        4 | 1434 | `{` |
|        - | 1435 | `	ph7_class_attr *pAttr;` |
|      150 | 1436 | `	VmClassAttr *pVmAttr = 0;` |
|      150 | 1437 | `	ph7_value *pMemObj = 0;` |
|        - | 1438 | `	char *zCopy;` |
|        - | 1439 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|      150 | 1440 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|      150 | 1441 | `	if( pAttr == 0 ){` |
|      ! 0 | 1442 | `		return 0;` |
|        - | 1443 | `	}` |
|      150 | 1444 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      150 | 1445 | `	zCopy = (char *)&pAttr[1];` |
|      150 | 1446 | `	if( nName > 0 ){` |
|      150 | 1447 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       73 | 1448 | `	}` |
|      150 | 1449 | `	zCopy[nName] = 0;` |
|      150 | 1450 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|      150 | 1451 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|      150 | 1452 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      150 | 1453 | `	pAttr->pDeclClass = pThis->pClass;` |
|        - | 1454 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|        - | 1455 | `	 * value, never a union. */` |
|      150 | 1456 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      150 | 1457 | `	if( pVmAttr == 0 ){` |
|      ! 0 | 1458 | `		goto fail_attr;` |
|        - | 1459 | `	}` |
|      150 | 1460 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      150 | 1461 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1462 | `		goto fail_vmattr;` |
|        - | 1463 | `	}` |
|      150 | 1464 | `	pVmAttr->pAttr = pAttr;` |
|      150 | 1465 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|      150 | 1466 | `	pVmAttr->iState = 0;` |
|      150 | 1467 | `	pVmAttr->pOwner = pThis->pClass;` |
|        - | 1468 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|        - | 1469 | `	 * property-creation order, matching PHP. */` |
|      150 | 1470 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 1471 | `		goto fail_slot;` |
|        - | 1472 | `	}` |
|        - | 1473 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|      150 | 1474 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      150 | 1475 | `	if( ppAttr ){` |
|       76 | 1476 | `		*ppAttr = pVmAttr;` |
|       37 | 1477 | `	}` |
|      150 | 1478 | `	return pMemObj;` |
|      ! 0 | 1479 | `fail_slot:` |
|        - | 1480 | `	{` |
|        - | 1481 | `		VmSlot sSlot;` |
|      ! 0 | 1482 | `		sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1483 | `		sSlot.pUserData = 0;` |
|      ! 0 | 1484 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1485 | `	}` |
|      ! 0 | 1486 | `fail_vmattr:` |
|      ! 0 | 1487 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1488 | `fail_attr:` |
|      ! 0 | 1489 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|      ! 0 | 1490 | `	return 0;` |
|       77 | 1491 | `}` |
|        - | 1492 | `/*` |
|        - | 1493 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|        - | 1494 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|        - | 1495 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|        - | 1496 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|        - | 1497 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|        - | 1498 | ` */` |
|        6 | 1499 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|        1 | 1500 | `{` |
|        - | 1501 | `	VmClassAttr *pVmAttr;` |
|        - | 1502 | `	ph7_value *pMemObj;` |
|        7 | 1503 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        7 | 1504 | `	if( pVmAttr == 0 ){` |
|      ! 0 | 1505 | `		return;` |
|        - | 1506 | `	}` |
|        7 | 1507 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|        7 | 1508 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1509 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1510 | `		return;` |
|        - | 1511 | `	}` |
|        7 | 1512 | `	pVmAttr->pAttr = pAttr;` |
|        7 | 1513 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|        7 | 1514 | `	pVmAttr->iState = 0;` |
|        7 | 1515 | `	pVmAttr->pOwner = pThis->pClass;` |
|        - | 1516 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|        - | 1517 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|        - | 1518 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|        - | 1519 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|        - | 1520 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|        7 | 1521 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1522 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      ! 0 | 1523 | `	}` |
|        - | 1524 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|        - | 1525 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|        - | 1526 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|        - | 1527 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|        7 | 1528 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|        - | 1529 | `		VmSlot sSlot;` |
|      ! 0 | 1530 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|      ! 0 | 1531 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1532 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1533 | `		return;` |
|        - | 1534 | `	}` |
|        7 | 1535 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        7 | 1536 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1537 | `		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|        - | 1538 | `			VmSlot sSlot;` |
|      ! 0 | 1539 | `			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 | 1540 | `			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|      ! 0 | 1541 | `			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1542 | `			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1543 | `			return;` |
|        - | 1544 | `		}` |
|      ! 0 | 1545 | `	}` |
|        7 | 1546 | `	if( ppAttr ){` |
|        7 | 1547 | `		*ppAttr = pVmAttr;` |
|        3 | 1548 | `	}` |
|        4 | 1549 | `}` |
|        - | 1550 | `/* Forward declaration */` |
|        - | 1551 | `/*` |
|        - | 1552 | ` * Dummy read-only buffer used for slot reservation.` |
|        - | 1553 | ` */` |
|        - | 1554 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - | 1555 | `/*` |
|        - | 1556 | ` * Reserve a constant memory object.` |
|        - | 1557 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 1558 | ` */` |
|  8588098 | 1559 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1560 | `{` |
|        - | 1561 | `	ph7_value *pObj;` |
|        - | 1562 | `	sxi32 rc;` |
|  8588103 | 1563 | `	if( pIndex ){` |
|        - | 1564 | `		/* Object index in the object table */` |
|  8576439 | 1565 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|  4288217 | 1566 | `	}` |
|        - | 1567 | `	/* Reserve a slot for the new object */` |
|  8588103 | 1568 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|  8588103 | 1569 | `	if( rc != SXRET_OK ){` |
|        - | 1570 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1571 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1572 | `		 */` |
|      ! 0 | 1573 | `		return 0;` |
|        - | 1574 | `	}` |
|  8588103 | 1575 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|  8588103 | 1576 | `	return pObj;` |
|  4294054 | 1577 | `}` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * Reserve a memory object.` |
|        - | 1580 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 1581 | ` */` |
|  2237728 | 1582 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1583 | `{` |
|        - | 1584 | `	ph7_value *pObj;` |
|        - | 1585 | `	sxi32 rc;` |
|  2237733 | 1586 | `	if( pIndex ){` |
|        - | 1587 | `		/* Object index in the object table */` |
|  2237733 | 1588 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1118864 | 1589 | `	}` |
|        - | 1590 | `	/* Reserve a slot for the new object */` |
|  2237733 | 1591 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2237733 | 1592 | `	if( rc != SXRET_OK ){` |
|        - | 1593 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1594 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1595 | `		 */` |
|      ! 0 | 1596 | `		return 0;` |
|        - | 1597 | `	}` |
|  2237733 | 1598 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2237733 | 1599 | `	return pObj;` |
|  1118869 | 1600 | `}` |
|        - | 1601 | `/* Forward declaration */` |
|        - | 1602 | `/* Forward declarations for Fiber C functions */` |
|        - | 1603 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - | 1604 | `/* Forward declarations for Generator helpers and C functions */` |
|        - | 1605 | `/*` |
|        - | 1606 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - | 1607 | ` * directly as foreign functions.` |
|        - | 1608 | ` */` |
|        - | 1609 |  |
|        - | 1610 | `/*` |
|        - | 1611 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - | 1612 | ` * start compiling the target PHP program.` |
|        - | 1613 | ` */` |
|     3888 | 1614 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - | 1615 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - | 1616 | `	 ph7 *pEngine /* Master engine */` |
|        - | 1617 | `	 )` |
|        5 | 1618 | `{` |
|        - | 1619 | `	ph7_value *pObj;` |
|        - | 1620 | `	sxi32 rc;` |
|        - | 1621 | `	/* Zero the structure */` |
|     3893 | 1622 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - | 1623 | `	/* Initialize VM fields */` |
|     3893 | 1624 | `	pVm->pEngine = &(*pEngine);` |
|     3893 | 1625 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|     3893 | 1626 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - | 1627 | `	/* Instructions containers */` |
|     3893 | 1628 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3893 | 1629 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3893 | 1630 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - | 1631 | `	/* Object containers */` |
|     3893 | 1632 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3893 | 1633 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - | 1634 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|     3893 | 1635 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|     3893 | 1636 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|     3893 | 1637 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|     3893 | 1638 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|        - | 1639 | `	/* Virtual machine internal containers */` |
|     3893 | 1640 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3893 | 1641 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3893 | 1642 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|     3893 | 1643 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|     3893 | 1644 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3893 | 1645 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3893 | 1646 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3893 | 1647 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3893 | 1648 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3893 | 1649 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3893 | 1650 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3893 | 1651 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3893 | 1652 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3893 | 1653 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3893 | 1654 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3893 | 1655 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3893 | 1656 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3893 | 1657 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3893 | 1658 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3893 | 1659 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|     3893 | 1660 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3893 | 1661 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3893 | 1662 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|     3893 | 1663 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3893 | 1664 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|     3893 | 1665 | `	pVm->nResourceIdNext = 1;` |
|     3893 | 1666 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3893 | 1667 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|     3893 | 1668 | `	pVm->pMagicSetThis = 0;` |
|     3893 | 1669 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|     3893 | 1670 | `	pVm->pHookSetThis = 0;` |
|     3893 | 1671 | `	pVm->pHookSetAttr = 0;` |
|     3893 | 1672 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3893 | 1673 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|     3893 | 1674 | `	pVm->pMagicCallThis = 0;` |
|     3893 | 1675 | `	pVm->pMagicCallClass = 0;` |
|     3893 | 1676 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|     3893 | 1677 | `	pVm->pIdleCallFrames = 0;` |
|     3893 | 1678 | `	pVm->pIdleOperandStacks = 0;` |
|     3893 | 1679 | `	pVm->nIdleOperandStacks = 0;` |
|     3893 | 1680 | `	pVm->pIdleStackNodes = 0;` |
|     3893 | 1681 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|     3893 | 1682 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|     3893 | 1683 | `	pVm->pPendingException = 0;` |
|     3893 | 1684 | `	pVm->pInflightException = 0;` |
|     3893 | 1685 | `	pVm->nInflightExcBase = 0;` |
|     3893 | 1686 | `	pVm->pResumeFrame = 0;` |
|     3893 | 1687 | `	pVm->iResumePc = 0;` |
|     3893 | 1688 | `	pVm->pResumeInstr = 0;` |
|     3893 | 1689 | `	pVm->iResumeStackDepth = 0;` |
|     3893 | 1690 | `	pVm->nBoundaryRc = 0;` |
|     3893 | 1691 | `	pVm->pConstEvalClass = 0;` |
|     3893 | 1692 | `	pVm->nConstEvalDepth = 0;` |
|     3893 | 1693 | `	pVm->pConstCycleAttr = 0;` |
|     3893 | 1694 | `	pVm->pConstCycleClass = 0;` |
|     3893 | 1695 | `	SySetReset(&pVm->aMagicGuard);` |
|     3893 | 1696 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 1697 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 1698 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 1699 | `	}` |
|     3893 | 1700 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|     3893 | 1701 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 1702 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 1703 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 1704 | `	}` |
|     3893 | 1705 | `	pVm->pHookSetAttr = 0;` |
|     3893 | 1706 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3893 | 1707 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 1708 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 1709 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 1710 | `	}` |
|     3893 | 1711 | `	pVm->pMagicCallClass = 0;` |
|     3893 | 1712 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        - | 1713 | `	/* Configuration containers */` |
|     3893 | 1714 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3893 | 1715 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3893 | 1716 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3893 | 1717 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3893 | 1718 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3893 | 1719 | `	pVm->iResponseStatus = 200;` |
|     3893 | 1720 | `	pVm->bHeadersSent = 0;` |
|     3893 | 1721 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - | 1722 | `	/* Error callbacks containers */` |
|     3893 | 1723 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3893 | 1724 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3893 | 1725 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3893 | 1726 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3893 | 1727 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - | 1728 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|        - | 1729 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|        - | 1730 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|        - | 1731 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|        - | 1732 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|        - | 1733 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|        - | 1734 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3893 | 1735 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|     3893 | 1736 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|        - | 1737 | `	                             * usort-in-comparator path overflows at 1024) */` |
|        - | 1738 | `#else` |
|        - | 1739 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|        - | 1740 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|        - | 1741 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|        - | 1742 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|        - | 1743 | `	pVm->nMaxDepth = 512;` |
|        - | 1744 | `	pVm->nMaxNativeDepth = 16;` |
|        - | 1745 | `#endif` |
|        - | 1746 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|        - | 1747 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|        - | 1748 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|     3893 | 1749 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|        - | 1750 | `	/* JSON return status */` |
|     3893 | 1751 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 1752 | `	/* PRNG context */` |
|     3893 | 1753 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - | 1754 | `	/* Install the null constant */` |
|     3893 | 1755 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3893 | 1756 | `	if( pObj == 0 ){` |
|      ! 0 | 1757 | `		rc = SXERR_MEM;` |
|      ! 0 | 1758 | `		goto Err;` |
|        - | 1759 | `	}` |
|     3893 | 1760 | `	PH7_MemObjInit(pVm,pObj);` |
|        - | 1761 | `	/* Install the boolean TRUE constant */` |
|     3893 | 1762 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3893 | 1763 | `	if( pObj == 0 ){` |
|      ! 0 | 1764 | `		rc = SXERR_MEM;` |
|      ! 0 | 1765 | `		goto Err;` |
|        - | 1766 | `	}` |
|     3893 | 1767 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - | 1768 | `	/* Install the boolean FALSE constant */` |
|     3893 | 1769 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3893 | 1770 | `	if( pObj == 0 ){` |
|      ! 0 | 1771 | `		rc = SXERR_MEM;` |
|      ! 0 | 1772 | `		goto Err;` |
|        - | 1773 | `	}` |
|     3893 | 1774 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - | 1775 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - | 1776 | `	 * reuse the same slot rather than allocating a new one.` |
|        - | 1777 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3893 | 1778 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3893 | 1779 | `	if( pObj == 0 ){` |
|      ! 0 | 1780 | `		rc = SXERR_MEM;` |
|      ! 0 | 1781 | `		goto Err;` |
|        - | 1782 | `	}` |
|     3893 | 1783 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - | 1784 | `	/* Create the global frame */` |
|     3893 | 1785 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3893 | 1786 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1787 | `		goto Err;` |
|        - | 1788 | `	}` |
|        - | 1789 | `	/* Initialize the code generator */` |
|     3893 | 1790 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3893 | 1791 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1792 | `		goto Err;` |
|        - | 1793 | `	}` |
|        - | 1794 | `	/* VM correctly initialized,set the magic number */` |
|     3893 | 1795 | `	pVm->nMagic = PH7_VM_INIT;` |
|        - | 1796 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|        - | 1797 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|     3893 | 1798 | `	pVm->bCompilingBuiltin = 1;` |
|        - | 1799 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|     3893 | 1800 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|        - | 1801 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|        - | 1802 | `	 * compiled — its classes are internal too. */` |
|        - | 1803 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3893 | 1804 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - | 1805 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3893 | 1806 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3893 | 1807 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3893 | 1808 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3893 | 1809 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3893 | 1810 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - | 1811 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3893 | 1812 | `	pVm->pCoalesceObj = 0;` |
|     3893 | 1813 | `	pVm->bCoalesceArmed = 0;` |
|     3893 | 1814 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - | 1815 | `	/* Register Fiber internal C functions */` |
|     3893 | 1816 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3893 | 1817 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3893 | 1818 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3893 | 1819 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3893 | 1820 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3893 | 1821 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3893 | 1822 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3893 | 1823 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3893 | 1824 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3893 | 1825 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - | 1826 | `	/* Cache the Closure class pointer (closures are instances of it) */` |
|     3893 | 1827 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|     3893 | 1828 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|     3893 | 1829 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|        - | 1830 | `	/* Closure::bind/bindTo/call/fromCallable native delegates (Increment 2) */` |
|     3893 | 1831 | `	ph7_create_function(pVm,"__closure_bindTo",vm_builtin_Closure_bindTo,0);` |
|     3893 | 1832 | `	ph7_create_function(pVm,"__closure_fromCallable",vm_builtin_Closure_fromCallable,0);` |
|        - | 1833 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|     3893 | 1834 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        - | 1835 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3893 | 1836 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3893 | 1837 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3893 | 1838 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3893 | 1839 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3893 | 1840 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3893 | 1841 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3893 | 1842 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3893 | 1843 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3893 | 1844 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3893 | 1845 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - | 1846 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|        - | 1847 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|        - | 1848 | `	 * internal; the Traversable pointer above must already be cached. */` |
|     3893 | 1849 | `	PH7_VmInstallReflection(&(*pVm));` |
|     3893 | 1850 | `	PH7_VmInstallDateTime(&(*pVm));` |
|     3893 | 1851 | `	PH7_VmInstallSpl(&(*pVm));` |
|     3893 | 1852 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|     3893 | 1853 | `	PH7_VmInstallSession(&(*pVm));` |
|     3893 | 1854 | `	PH7_VmInstallIni(&(*pVm));` |
|        - | 1855 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1856 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|        - | 1857 | `	 * XMLWriter class libraries that build on it. */` |
|     3893 | 1858 | `	PH7_VmInstallLibxml(&(*pVm));` |
|     3893 | 1859 | `	PH7_VmInstallDom(&(*pVm));` |
|     3893 | 1860 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|        - | 1861 | `#endif` |
|     3893 | 1862 | `	pVm->bCompilingBuiltin = 0;` |
|        - | 1863 | `	/* Reset the code generator */` |
|     3893 | 1864 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3893 | 1865 | `	return SXRET_OK;` |
|      ! 0 | 1866 | `Err:` |
|      ! 0 | 1867 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 | 1868 | `	return rc;` |
|     1949 | 1869 | `}` |
|        - | 1870 | `/*` |
|        - | 1871 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - | 1872 | ` * routine which store the output in an internal blob.` |
|        - | 1873 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - | 1874 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - | 1875 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - | 1876 | ` * Refer to the official docurmentation for additional information.` |
|        - | 1877 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - | 1878 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - | 1879 | ` * to finish executing and extracting the output.` |
|        - | 1880 | ` */` |
|       66 | 1881 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - | 1882 | `	const void *pOut,   /* VM Generated output*/` |
|        - | 1883 | `	unsigned int nLen,  /* Generated output length */` |
|        - | 1884 | `	void *pUserData     /* User private data */` |
|        - | 1885 | `	)` |
|      ! 0 | 1886 | `{` |
|        - | 1887 | `	 sxi32 rc;` |
|        - | 1888 | `	 /* Store the output in an internal BLOB */` |
|       66 | 1889 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       66 | 1890 | `	 return rc;` |
|      ! 0 | 1891 | `}` |
|        - | 1892 | `/*` |
|        - | 1893 | ` * Track output length and mark headers as sent when output reaches` |
|        - | 1894 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - | 1895 | ` */` |
|    42822 | 1896 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 | 1897 | `{` |
|    42827 | 1898 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    42827 | 1899 | `	if( xCons != VmObConsumer ){` |
|    12803 | 1900 | `		pVm->nOutputLen += nLen;` |
|    12803 | 1901 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1401 | 1902 | `			pVm->bHeadersSent = 1;` |
|      698 | 1903 | `		}` |
|     6399 | 1904 | `	}` |
|    42827 | 1905 | `}` |
|        - | 1906 | `/*` |
|        - | 1907 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|        - | 1908 | ` *` |
|        - | 1909 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|        - | 1910 | ` * (no instruction pushes more than one net slot), and that is what` |
|        - | 1911 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|        - | 1912 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|        - | 1913 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|        - | 1914 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|        - | 1915 | ` *` |
|        - | 1916 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|        - | 1917 | ` * conservative BY CONSTRUCTION:` |
|        - | 1918 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|        - | 1919 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|        - | 1920 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|        - | 1921 | ` *     pop — makes height go negative, which triggers fallback.` |
|        - | 1922 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|        - | 1923 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|        - | 1924 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|        - | 1925 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|        - | 1926 | ` *     bound. There is no partial/unsafe middle.` |
|        - | 1927 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|        - | 1928 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|        - | 1929 | ` *     instruction-count bound -> fallback.` |
|        - | 1930 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|        - | 1931 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|        - | 1932 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|        - | 1933 | ` *` |
|        - | 1934 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|        - | 1935 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|        - | 1936 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|        - | 1937 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|        - | 1938 | ` *` |
|        - | 1939 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|        - | 1940 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|        - | 1941 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|        - | 1942 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|        - | 1943 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|        - | 1944 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|        - | 1945 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|        - | 1946 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|        - | 1947 | ` * entry here.` |
|        - | 1948 | ` */` |
|        - | 1949 | `/*` |
|        - | 1950 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|        - | 1951 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|        - | 1952 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|        - | 1953 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|        - | 1954 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|        - | 1955 | ` */` |
|    51412 | 1956 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|        5 | 1957 | `{` |
|    51417 | 1958 | `	int push = 0, n = 0;` |
|        - | 1959 | `	sxi32 d;` |
|    51417 | 1960 | `	switch( pI->iOp ){` |
|        - | 1961 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|        - | 1962 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     8119 | 1963 | `	case PH7_OP_LOADC:` |
|        - | 1964 | `	case PH7_OP_DUP:` |
|    16243 | 1965 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|     5798 | 1966 | `	case PH7_OP_LOAD:` |
|    11601 | 1967 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|    11601 | 1968 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|      ! 0 | 1969 | `	case PH7_OP_LOAD_REF:` |
|      ! 0 | 1970 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1971 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|      344 | 1972 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|        - | 1973 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|        - | 1974 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      693 | 1975 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|        - | 1976 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|        - | 1977 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|      269 | 1978 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - | 1979 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|      543 | 1980 | `		if( pI->iP2 == 0 ){` |
|      543 | 1981 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|      274 | 1982 | `		}else{` |
|      ! 0 | 1983 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|      ! 0 | 1984 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|        - | 1985 | `		}` |
|      543 | 1986 | `		break;` |
|        - | 1987 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|        - | 1988 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|      156 | 1989 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|        - | 1990 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|        - | 1991 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|        - | 1992 | `	case PH7_OP_NOOP:` |
|      316 | 1993 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1994 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|        - | 1995 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      818 | 1996 | `	case PH7_OP_STORE:` |
|     1641 | 1997 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|     1641 | 1998 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|        - | 1999 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|     1262 | 2000 | `	case PH7_OP_POP:` |
|        - | 2001 | `	case PH7_OP_CONSUME:` |
|     2529 | 2002 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2003 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|        - | 2004 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|        - | 2005 | `	 * true pop count is a runtime value — never reaches here. */` |
|     1917 | 2006 | `	case PH7_OP_CALL:` |
|     3839 | 2007 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2008 | `	/* Jumps. */` |
|      147 | 2009 | `	case PH7_OP_JMP:` |
|      299 | 2010 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|      774 | 2011 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|     1553 | 2012 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|     1553 | 2013 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|        - | 2014 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|     2975 | 2015 | `	case PH7_OP_DONE:` |
|     5955 | 2016 | `		n = 0; break;` |
|     3127 | 2017 | `	default:` |
|     6259 | 2018 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|        - | 2019 | `	}` |
|    45163 | 2020 | `	*pPush = push; *pN = n;` |
|    45163 | 2021 | `	return 1;` |
|    25711 | 2022 | `}` |
|        - | 2023 | `/*` |
|        - | 2024 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|        - | 2025 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|        - | 2026 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|        - | 2027 | ` */` |
|     8288 | 2028 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|        5 | 2029 | `{` |
|        - | 2030 | `	void *pScratch;` |
|        - | 2031 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|        - | 2032 | `	sxu32 nQ, i, nIter, nCap;` |
|        - | 2033 | `	sxi32 iMax;` |
|        - | 2034 | `	int push, n, k;` |
|        - | 2035 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     8293 | 2036 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|        - | 2037 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|      ! 0 | 2038 | `		return VM_STACK_UNMODELED;` |
|        - | 2039 | `	}` |
|        - | 2040 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|    44833 | 2041 | `	for( i = 0; i < nInstr; i++ ){` |
|    42799 | 2042 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     6259 | 2043 | `			return VM_STACK_UNMODELED;` |
|        - | 2044 | `		}` |
|    18275 | 2045 | `	}` |
|        - | 2046 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|        - | 2047 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|        - | 2048 | `	 * first (the byte array last needs no alignment). */` |
|     2039 | 2049 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|     2039 | 2050 | `	if( pScratch == 0 ){` |
|      ! 0 | 2051 | `		return VM_STACK_UNMODELED;` |
|        - | 2052 | `	}` |
|     2039 | 2053 | `	aH  = (sxi32 *)pScratch;` |
|     2039 | 2054 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|     2039 | 2055 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|    12387 | 2056 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|     2039 | 2057 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|     2039 | 2058 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|    10657 | 2059 | `	while( nQ > 0 ){` |
|     8623 | 2060 | `		sxu32 pc = aQ[--nQ];` |
|        - | 2061 | `		sxi32 h;` |
|     8623 | 2062 | `		aIn[pc] = 0;` |
|     8623 | 2063 | `		h = aH[pc];` |
|     8623 | 2064 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     8623 | 2065 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     8623 | 2066 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     8623 | 2067 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|    15285 | 2068 | `		for( k = 0; k < n; k++ ){` |
|     6667 | 2069 | `			sxi32 hn = h + delta[k];` |
|     6667 | 2070 | `			sxu32 t = succ[k];` |
|     6667 | 2071 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|     6667 | 2072 | `			if( hn > aH[t] ){` |
|     6589 | 2073 | `				aH[t] = hn;` |
|     6589 | 2074 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|     3292 | 2075 | `			}` |
|     3336 | 2076 | `		}` |
|     8623 | 2077 | `		if( iMax < 0 ){ break; }` |
|        5 | 2078 | `	}` |
|     2039 | 2079 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|     2039 | 2080 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|     4149 | 2081 | `}` |
|        - | 2082 | `/*` |
|        - | 2083 | ` * Allocate a new operand stack so that we can start executing` |
|        - | 2084 | ` * our compiled PHP program.` |
|        - | 2085 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - | 2086 | ` * on success. NULL (Fatal error) on failure.` |
|        - | 2087 | ` *` |
|        - | 2088 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|        - | 2089 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|        - | 2090 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|        - | 2091 | ` * eval, coroutine, callbacks) call this directly.` |
|        - | 2092 | ` */` |
|   250154 | 2093 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|        - | 2094 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 2095 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - | 2096 | `	)` |
|        5 | 2097 | `{` |
|        - | 2098 | `	ph7_value *pStack;` |
|        - | 2099 | `  /* No instruction ever pushes more than a single element onto the` |
|        - | 2100 | `  ** stack and the stack never grows on successive executions of the` |
|        - | 2101 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - | 2102 | `  ** on the maximum stack depth required.` |
|        - | 2103 | `  **` |
|        - | 2104 | `  ** Allocation all the stack space we will ever need.` |
|        - | 2105 | `  */` |
|   250159 | 2106 | `	nInstr += VM_STACK_GUARD;` |
|   250159 | 2107 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   250159 | 2108 | `	if( pStack == 0 ){` |
|      ! 0 | 2109 | `		return 0;` |
|        - | 2110 | `	}` |
|        - | 2111 | `	/* Initialize the operand stack */` |
| 24088367 | 2112 | `	while( nInstr > 0 ){` |
| 23838213 | 2113 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
| 23838213 | 2114 | `		--nInstr;` |
|        5 | 2115 | `	}` |
|        - | 2116 | `	/* Ready for bytecode execution */` |
|   250159 | 2117 | `	return pStack;` |
|   125184 | 2118 | `}` |
|        - | 2119 | `/*` |
|        - | 2120 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|        - | 2121 | ` *` |
|        - | 2122 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|        - | 2123 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|        - | 2124 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|        - | 2125 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|        - | 2126 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|        - | 2127 | ` * the per-slot PH7_MemObjInit.` |
|        - | 2128 | ` *` |
|        - | 2129 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|        - | 2130 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|        - | 2131 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|        - | 2132 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|        - | 2133 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|        - | 2134 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|        - | 2135 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|        - | 2136 | ` *` |
|        - | 2137 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|        - | 2138 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|        - | 2139 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|        - | 2140 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|        - | 2141 | ` * recursion case is the one worth the O(1) simplicity.` |
|        - | 2142 | ` */` |
|        - | 2143 | `typedef struct VmIdleStack VmIdleStack;` |
|        - | 2144 | `struct VmIdleStack {` |
|        - | 2145 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|        - | 2146 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|        - | 2147 | `	VmIdleStack *pNext;  /* LIFO link */` |
|        - | 2148 | `};` |
|        - | 2149 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|        - | 2150 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|        - | 2151 | `                                    * (a large fallback-sized stack recursing would` |
|        - | 2152 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|        - | 2153 | `                                    * the tight-sized hot case is far below this) */` |
|        - | 2154 | `/*` |
|        - | 2155 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|        - | 2156 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|        - | 2157 | ` */` |
|   105577 | 2158 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|        5 | 2159 | `{` |
|   105582 | 2160 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   105582 | 2161 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|   105582 | 2162 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    16024 | 2163 | `		ph7_value *pStack = pIdle->pStack;` |
|    16024 | 2164 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    16024 | 2165 | `		pVm->nIdleOperandStacks--;` |
|        - | 2166 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|        - | 2167 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    16024 | 2168 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    16024 | 2169 | `		pVm->pIdleStackNodes = pIdle;` |
|    16024 | 2170 | `		return pStack; /* slots already released -> reusable without re-init */` |
|        - | 2171 | `	}` |
|    89563 | 2172 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    52897 | 2173 | `}` |
|        - | 2174 | `/*` |
|        - | 2175 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|        - | 2176 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|        - | 2177 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|        - | 2178 | ` * live value.` |
|        - | 2179 | ` */` |
|   105167 | 2180 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|        5 | 2181 | `{` |
|        - | 2182 | `	VmIdleStack *pIdle;` |
|        - | 2183 | `	sxu32 i;` |
|   105172 | 2184 | `	if( pStack == 0 ){` |
|      ! 0 | 2185 | `		return;` |
|        - | 2186 | `	}` |
|   105172 | 2187 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|    83245 | 2188 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|    83245 | 2189 | `		return;` |
|        - | 2190 | `	}` |
|        - | 2191 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|        - | 2192 | `	 * pool-allocate only when the spare list is empty. */` |
|    21932 | 2193 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    21932 | 2194 | `	if( pIdle ){` |
|    16024 | 2195 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|     8016 | 2196 | `	}else{` |
|     5913 | 2197 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|     5913 | 2198 | `		if( pIdle == 0 ){` |
|      ! 0 | 2199 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|      ! 0 | 2200 | `			return;` |
|        - | 2201 | `		}` |
|        - | 2202 | `	}` |
|  1312887 | 2203 | `	for( i = 0; i < nCap; i++ ){` |
|  1290960 | 2204 | `		PH7_MemObjRelease(&pStack[i]);` |
|        - | 2205 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|        - | 2206 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|        - | 2207 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|        - | 2208 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|        - | 2209 | `		 * across invocations — cheap defense in depth. */` |
|  1290960 | 2210 | `		pStack[i].nIdx = SXU32_HIGH;` |
|   646063 | 2211 | `	}` |
|    21932 | 2212 | `	pIdle->pStack = pStack;` |
|    21932 | 2213 | `	pIdle->nCap = nCap;` |
|    21932 | 2214 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    21932 | 2215 | `	pVm->pIdleOperandStacks = pIdle;` |
|    21932 | 2216 | `	pVm->nIdleOperandStacks++;` |
|    52692 | 2217 | `}` |
|        - | 2218 | `/* Forward declaration */` |
|        - | 2219 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - | 2220 | `/*` |
|        - | 2221 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - | 2222 | ` * This routine gets called by the PH7 engine after` |
|        - | 2223 | ` * successful compilation of the target PHP program.` |
|        - | 2224 | ` */` |
|     3404 | 2225 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - | 2226 | `	ph7_vm *pVm /* Target VM */` |
|        - | 2227 | `	)` |
|        5 | 2228 | `{` |
|        - | 2229 | `	SyHashEntry *pEntry;` |
|        - | 2230 | `	sxi32 rc;` |
|     3409 | 2231 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - | 2232 | `		/* Initialize your VM first */` |
|      ! 0 | 2233 | `		return SXERR_CORRUPT;` |
|        - | 2234 | `	}` |
|        - | 2235 | `	/* Mark the VM ready for byte-code execution */` |
|     3409 | 2236 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - | 2237 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - | 2238 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - | 2239 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - | 2240 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     3409 | 2241 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - | 2242 | `	/* Emit the DONE instruction */` |
|     3409 | 2243 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     3409 | 2244 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2245 | `		return SXERR_MEM;` |
|        - | 2246 | `	}` |
|        - | 2247 | `	/* Script return value */` |
|     3409 | 2248 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - | 2249 | `	/* Allocate a new operand stack */` |
|     3409 | 2250 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     3409 | 2251 | `	if( pVm->aOps == 0 ){` |
|      ! 0 | 2252 | `		return SXERR_MEM;` |
|        - | 2253 | `	}` |
|        - | 2254 | `	/* Set the default VM output consumer callback and it's` |
|        - | 2255 | `	 * private data. */` |
|     3409 | 2256 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     3409 | 2257 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - | 2258 | `	/* Allocate the reference table */` |
|     3409 | 2259 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     3409 | 2260 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     3409 | 2261 | `	if( pVm->apRefObj == 0 ){` |
|        - | 2262 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2263 | `		return SXERR_MEM;` |
|        - | 2264 | `	}` |
|        - | 2265 | `	/* Zero the reference table */` |
|     3409 | 2266 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - | 2267 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     3409 | 2268 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     3409 | 2269 | `	if( rc != SXRET_OK ){` |
|        - | 2270 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2271 | `		return rc;` |
|        - | 2272 | `	}` |
|        - | 2273 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - | 2274 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - | 2275 | `	 * every object/variable created during execution) is per-exec state that` |
|        - | 2276 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - | 2277 | `	 * below it is compile-time/init state that survives a reset. */` |
|     3409 | 2278 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - | 2279 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     3409 | 2280 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     3409 | 2281 | `	if( rc != SXRET_OK ){` |
|        - | 2282 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2283 | `		return rc;` |
|        - | 2284 | `	}` |
|        - | 2285 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     3409 | 2286 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - | 2287 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|     3409 | 2288 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|        - | 2289 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     3409 | 2290 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - | 2291 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     3409 | 2292 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - | 2293 | `#ifdef PH7_ENABLE_PCRE` |
|        - | 2294 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     3409 | 2295 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     3409 | 2296 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - | 2297 | `#endif` |
|        - | 2298 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2299 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|     3409 | 2300 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|        - | 2301 | `#endif` |
|        - | 2302 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|        - | 2303 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|     3409 | 2304 | `	VmSetBuiltinArity(&(*pVm));` |
|        - | 2305 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|     3409 | 2306 | `	VmSetBuiltinSignatures(&(*pVm));` |
|        - | 2307 | `	/* Initialize and install static and constants class attributes.` |
|        - | 2308 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - | 2309 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - | 2310 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - | 2311 | `	 * that function in sync when changing what is reserved here. */` |
|     3409 | 2312 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   466617 | 2313 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   463215 | 2314 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   463215 | 2315 | `		if( rc != SXRET_OK ){` |
|        3 | 2316 | `			return rc;` |
|        - | 2317 | `		}` |
|        5 | 2318 | `	}` |
|        - | 2319 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     3407 | 2320 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2321 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|     3407 | 2322 | `	pVm->nNextObjId = 1;` |
|        - | 2323 | `	/* VM is ready for bytecode execution */` |
|     3407 | 2324 | `	return SXRET_OK;` |
|     1707 | 2325 | `}` |
|        - | 2326 | `/*` |
|        - | 2327 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - | 2328 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - | 2329 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - | 2330 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - | 2331 | ` * a dangling node pointer in some other object's reference record.` |
|        - | 2332 | ` */` |
|        8 | 2333 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 | 2334 | `{` |
|        - | 2335 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - | 2336 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - | 2337 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      328 | 2338 | `	while( pVm->pRefList ){` |
|      320 | 2339 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 | 2340 | `	}` |
|        8 | 2341 | `}` |
|        - | 2342 | `/*` |
|        - | 2343 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - | 2344 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - | 2345 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - | 2346 | ` */` |
|       56 | 2347 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 | 2348 | `{` |
|       56 | 2349 | `	PH7_MemObjRelease(pObj);` |
|       56 | 2350 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       56 | 2351 | `}` |
|        - | 2352 | `/*` |
|        - | 2353 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - | 2354 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - | 2355 | ` * of statics).` |
|        - | 2356 | ` */` |
|     6760 | 2357 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 | 2358 | `{` |
|     6760 | 2359 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - | 2360 | `	sxu32 k;` |
|     6788 | 2361 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|       28 | 2362 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|       14 | 2363 | `	}` |
|     6760 | 2364 | `}` |
|        - | 2365 | `/*` |
|        - | 2366 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - | 2367 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - | 2368 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - | 2369 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - | 2370 | ` *    captured environment values, its name buffer and its structure (the` |
|        - | 2371 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - | 2372 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - | 2373 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - | 2374 | ` *    has its static sentinels reset.` |
|        - | 2375 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - | 2376 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - | 2377 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - | 2378 | ` */` |
|        8 | 2379 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 | 2380 | `{` |
|        - | 2381 | `	SyHashEntry *pEntry;` |
|        8 | 2382 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     6768 | 2383 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     6760 | 2384 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     6760 | 2385 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 2386 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - | 2387 | `			 * release its captured-by-value environment, then free the entry,` |
|        - | 2388 | `			 * name buffer and structure. */` |
|        4 | 2389 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 | 2390 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - | 2391 | `			sxu32 k;` |
|        4 | 2392 | `			VmResetFuncStatics(pFunc);` |
|        8 | 2393 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 | 2394 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 | 2395 | `			}` |
|        4 | 2396 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - | 2397 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 | 2398 | `			SyHashDeleteEntry2(pEntry);` |
|        4 | 2399 | `			if( zName ){` |
|        4 | 2400 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 | 2401 | `			}` |
|        4 | 2402 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 | 2403 | `			continue;` |
|        - | 2404 | `		}` |
|        - | 2405 | `		/* Named function: reset statics for every overload sharing this name. */` |
|    13512 | 2406 | `		while( pFunc ){` |
|     6756 | 2407 | `			VmResetFuncStatics(pFunc);` |
|     6756 | 2408 | `			pFunc = pFunc->pNextName;` |
|      ! 0 | 2409 | `		}` |
|      ! 0 | 2410 | `	}` |
|        8 | 2411 | `	pVm->closure_cnt = 0;` |
|        8 | 2412 | `}` |
|        - | 2413 | `/*` |
|        - | 2414 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - | 2415 | ` * are already gone (each object's destructor removed its own during the object` |
|        - | 2416 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - | 2417 | ` * the class re-mount registers fresh ones.` |
|        - | 2418 | ` */` |
|        8 | 2419 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 | 2420 | `{` |
|        - | 2421 | `	SyHashEntry *pEntry;` |
|        - | 2422 | `	/* Common case: no class static typed properties — table already empty. */` |
|        8 | 2423 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        4 | 2424 | `		return;` |
|        - | 2425 | `	}` |
|        - | 2426 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - | 2427 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 | 2428 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 | 2429 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 | 2430 | `		if( pEntry->pUserData ){` |
|        4 | 2431 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 | 2432 | `		}` |
|      ! 0 | 2433 | `	}` |
|        4 | 2434 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 | 2435 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        4 | 2436 | `}` |
|        - | 2437 | `/*` |
|        - | 2438 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|        - | 2439 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|        - | 2440 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|        - | 2441 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|        - | 2442 | ` * comparing equal — both used to cast to 1.` |
|        - | 2443 | ` *` |
|        - | 2444 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|        - | 2445 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|        - | 2446 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|        - | 2447 | ` * aborting a cast.` |
|        - | 2448 | ` */` |
|       24 | 2449 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|        2 | 2450 | `{` |
|        - | 2451 | `	SyHashEntry *pEntry;` |
|        - | 2452 | `	phl_res_id *pRec;` |
|       26 | 2453 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|      ! 0 | 2454 | `		return 0;` |
|        - | 2455 | `	}` |
|       26 | 2456 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|       26 | 2457 | `	if( pEntry ){` |
|       20 | 2458 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|        - | 2459 | `	}` |
|        8 | 2460 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        8 | 2461 | `	if( pRec == 0 ){` |
|      ! 0 | 2462 | `		return 0;` |
|        - | 2463 | `	}` |
|        8 | 2464 | `	pRec->pRes = pRes;` |
|        8 | 2465 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        8 | 2466 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|      ! 0 | 2467 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|      ! 0 | 2468 | `		return 0;` |
|        - | 2469 | `	}` |
|        8 | 2470 | `	return pRec->nId;` |
|       14 | 2471 | `}` |
|        - | 2472 | `/*` |
|        - | 2473 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|        - | 2474 | ` * 1 for the next run, matching a fresh php process.` |
|        - | 2475 | ` */` |
|        8 | 2476 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|      ! 0 | 2477 | `{` |
|        - | 2478 | `	SyHashEntry *pEntry;` |
|        8 | 2479 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        8 | 2480 | `		pVm->nResourceIdNext = 1;` |
|        8 | 2481 | `		return;` |
|        - | 2482 | `	}` |
|      ! 0 | 2483 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|      ! 0 | 2484 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|      ! 0 | 2485 | `		if( pEntry->pUserData ){` |
|      ! 0 | 2486 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|      ! 0 | 2487 | `		}` |
|      ! 0 | 2488 | `	}` |
|      ! 0 | 2489 | `	SyHashRelease(&pVm->hResourceId);` |
|      ! 0 | 2490 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      ! 0 | 2491 | `	pVm->nResourceIdNext = 1;` |
|        4 | 2492 | `}` |
|        - | 2493 | `/*` |
|        - | 2494 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - | 2495 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - | 2496 | ` *` |
|        - | 2497 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - | 2498 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - | 2499 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - | 2500 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - | 2501 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - | 2502 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - | 2503 | ` *` |
|        - | 2504 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - | 2505 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - | 2506 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - | 2507 | ` * exception/error-handler state, the reference table and every object/array` |
|        - | 2508 | ` * reserved during the run.` |
|        - | 2509 | ` *` |
|        - | 2510 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - | 2511 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - | 2512 | ` * global-scope destructors never fired.` |
|        - | 2513 | ` */` |
|        8 | 2514 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 | 2515 | `{` |
|        - | 2516 | `	sxu32 nWater,n;` |
|        8 | 2517 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 | 2518 | `		return SXERR_CORRUPT;` |
|        - | 2519 | `	}` |
|        8 | 2520 | `	nWater = pVm->nSuperBaseline;` |
|        - | 2521 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - | 2522 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        8 | 2523 | `	pVm->pGlobal = 0;` |
|        - | 2524 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|        - | 2525 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|        - | 2526 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|        - | 2527 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|        - | 2528 | `	 * object); unref'ing here would race the teardown below. */` |
|        8 | 2529 | `	pVm->pClosureThis = 0;` |
|        8 | 2530 | `	pVm->pClosureScope = 0;` |
|        - | 2531 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - | 2532 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - | 2533 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - | 2534 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        8 | 2535 | `	pVm->bInReset = 1;` |
|        - | 2536 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        8 | 2537 | `	VmResetRefTable(&(*pVm));` |
|        - | 2538 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - | 2539 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - | 2540 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - | 2541 | `	 * re-run define() overwrites the value in place). */` |
|        8 | 2542 | `	VmResetFunctionState(&(*pVm));` |
|        - | 2543 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - | 2544 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      344 | 2545 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      336 | 2546 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      336 | 2547 | `		if( pObj ){` |
|      336 | 2548 | `			PH7_MemObjRelease(pObj);` |
|      168 | 2549 | `		}` |
|      168 | 2550 | `	}` |
|        - | 2551 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - | 2552 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        8 | 2553 | `	VmResetTypedSlots(&(*pVm));` |
|        - | 2554 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|        - | 2555 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        8 | 2556 | `	VmResetResourceIds(&(*pVm));` |
|        - | 2557 | `	/* (5) Unwind any active frames back to none. */` |
|       16 | 2558 | `	while( pVm->pFrame ){` |
|        8 | 2559 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 2560 | `	}` |
|        - | 2561 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        8 | 2562 | `	pVm->bInReset = 0;` |
|        - | 2563 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - | 2564 | `	 * slots (their indices no longer exist). */` |
|        8 | 2565 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        8 | 2566 | `	SySetReset(&pVm->aFreeObj);` |
|        - | 2567 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        8 | 2568 | `	SyHashRelease(&pVm->hSuper);` |
|        8 | 2569 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - | 2570 | `	/* (8) Drain remaining per-exec containers. */` |
|        8 | 2571 | `	SySetReset(&pVm->aSelf);` |
|        - | 2572 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - | 2573 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - | 2574 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        8 | 2575 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 | 2576 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 | 2577 | `		if( pCB ){` |
|        - | 2578 | `			int iArg;` |
|      ! 0 | 2579 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2580 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 | 2581 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 | 2582 | `			}` |
|      ! 0 | 2583 | `		}` |
|      ! 0 | 2584 | `	}` |
|        8 | 2585 | `	SySetReset(&pVm->aShutdown);` |
|        - | 2586 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|        - | 2587 | `	 * aborted program can leave entries behind). */` |
|        8 | 2588 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        8 | 2589 | `	SySetReset(&pVm->aException);` |
|        8 | 2590 | `	SySetReset(&pVm->aFinallyAction);` |
|        8 | 2591 | `	pVm->pPendingException = 0;` |
|        8 | 2592 | `	pVm->pInflightException = 0;` |
|        8 | 2593 | `	pVm->nInflightExcBase = 0;` |
|        8 | 2594 | `	pVm->pResumeFrame = 0;` |
|        8 | 2595 | `	pVm->iResumePc = 0;` |
|        8 | 2596 | `	pVm->pResumeInstr = 0;` |
|        8 | 2597 | `	pVm->iResumeStackDepth = 0;` |
|        8 | 2598 | `	pVm->nBoundaryRc = 0;` |
|        8 | 2599 | `	pVm->pConstEvalClass = 0;` |
|        8 | 2600 | `	pVm->nConstEvalDepth = 0;` |
|        8 | 2601 | `	pVm->pConstCycleAttr = 0;` |
|        8 | 2602 | `	pVm->pConstCycleClass = 0;` |
|        8 | 2603 | `	SySetReset(&pVm->aMagicGuard);` |
|        - | 2604 | `	{` |
|        - | 2605 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|        - | 2606 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        8 | 2607 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        8 | 2608 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|        - | 2609 | `		sxu32 iRmw;` |
|        8 | 2610 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|      ! 0 | 2611 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|      ! 0 | 2612 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|      ! 0 | 2613 | `		}` |
|        8 | 2614 | `		SySetReset(&pVm->aHookRmw);` |
|        - | 2615 | `	}` |
|        8 | 2616 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 2617 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 2618 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 2619 | `	}` |
|        8 | 2620 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        8 | 2621 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 2622 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 2623 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 2624 | `	}` |
|        8 | 2625 | `	pVm->pHookSetAttr = 0;` |
|        8 | 2626 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        8 | 2627 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 2628 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 2629 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 2630 | `	}` |
|        8 | 2631 | `	pVm->pMagicCallClass = 0;` |
|        8 | 2632 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        8 | 2633 | `	pVm->nExceptDepth = 0;` |
|        - | 2634 | `	/* spl_autoload_register() callbacks are per request */` |
|        8 | 2635 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 | 2636 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 | 2637 | `		if( pCB ){` |
|      ! 0 | 2638 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2639 | `		}` |
|      ! 0 | 2640 | `	}` |
|        8 | 2641 | `	SySetReset(&pVm->aAutoload);` |
|        - | 2642 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - | 2643 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        8 | 2644 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 | 2645 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 | 2646 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 | 2647 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      ! 0 | 2648 | `	}` |
|        - | 2649 | `	/* Output buffers */` |
|        8 | 2650 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 | 2651 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 | 2652 | `		if( pOb ){` |
|      ! 0 | 2653 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 | 2654 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 | 2655 | `		}` |
|      ! 0 | 2656 | `	}` |
|        8 | 2657 | `	SySetReset(&pVm->aOB);` |
|        8 | 2658 | `	pVm->nObDepth = 0;` |
|        - | 2659 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - | 2660 | `	{` |
|        8 | 2661 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        8 | 2662 | `		if( rc == SXRET_OK ){` |
|        8 | 2663 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        4 | 2664 | `		}` |
|        8 | 2665 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2666 | `			return rc;` |
|        - | 2667 | `		}` |
|        - | 2668 | `	}` |
|        - | 2669 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|        - | 2670 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|        - | 2671 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|        - | 2672 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|        - | 2673 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|        - | 2674 | `	{` |
|        - | 2675 | `		SyHashEntry *pEntry;` |
|        8 | 2676 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1100 | 2677 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1092 | 2678 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 2679 | `			ph7_class_attr *pAttr;` |
|        - | 2680 | `			SyHashEntry *pAttrEntry;` |
|     1092 | 2681 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     6330 | 2682 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     4692 | 2683 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     4692 | 2684 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     1340 | 2685 | `					pAttr->nIdx = SXU32_HIGH;` |
|     1340 | 2686 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      670 | 2687 | `				}` |
|      ! 0 | 2688 | `			}` |
|      ! 0 | 2689 | `		}` |
|        8 | 2690 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1100 | 2691 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1092 | 2692 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     1092 | 2693 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2694 | `				return rc;` |
|        - | 2695 | `			}` |
|      ! 0 | 2696 | `		}` |
|        - | 2697 | `	}` |
|        - | 2698 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        8 | 2699 | `	SyBlobReset(&pVm->sConsumer);` |
|        8 | 2700 | `	pVm->nOutputLen = 0;` |
|        8 | 2701 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        8 | 2702 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        8 | 2703 | `	pVm->iResponseStatus = 200;` |
|        8 | 2704 | `	pVm->bHeadersSent = 0;` |
|        8 | 2705 | `	pVm->bHttpContext = 0;` |
|        8 | 2706 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        8 | 2707 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        8 | 2708 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        8 | 2709 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        8 | 2710 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        8 | 2711 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 2712 | `#ifdef PH7_ENABLE_PCRE` |
|        8 | 2713 | `	pVm->iPcreLastError = 0;` |
|        - | 2714 | `#endif` |
|        - | 2715 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2716 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        8 | 2717 | `	PH7_LibxmlVmReset(&(*pVm));` |
|        - | 2718 | `#endif` |
|        8 | 2719 | `	pVm->iCmpCallbackExc = 0;` |
|        8 | 2720 | `	pVm->bHaltRequested = 0;` |
|        8 | 2721 | `	pVm->iExitStatus = 0;` |
|        8 | 2722 | `	pVm->nSpreadCallBase = 0;` |
|        8 | 2723 | `	VmSpreadCaptureReset(pVm);` |
|        8 | 2724 | `	pVm->nRecursionDepth = 0;` |
|        8 | 2725 | `	pVm->pActiveCtx = 0;` |
|        8 | 2726 | `	pVm->pCoalesceObj = 0;` |
|        8 | 2727 | `	pVm->bCoalesceArmed = 0;` |
|        8 | 2728 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - | 2729 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        8 | 2730 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2731 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|        - | 2732 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        8 | 2733 | `	pVm->nNextObjId = 1;` |
|        - | 2734 | `	/* Set the ready flag */` |
|        8 | 2735 | `	pVm->nMagic = PH7_VM_RUN;` |
|        8 | 2736 | `	return SXRET_OK;` |
|        4 | 2737 | `}` |
|        - | 2738 | `/*` |
|        - | 2739 | ` * Release a Virtual Machine.` |
|        - | 2740 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - | 2741 | ` */` |
|     3402 | 2742 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 | 2743 | `{` |
|        - | 2744 | `	/* Set the stale magic number */` |
|     3407 | 2745 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - | 2746 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2747 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|        - | 2748 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|     3407 | 2749 | `	PH7_LibxmlVmRelease(pVm);` |
|        - | 2750 | `#endif` |
|        - | 2751 | `	/* Release the private memory subsystem */` |
|     3407 | 2752 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     3407 | 2753 | `	return SXRET_OK;` |
|        5 | 2754 | `}` |
|        - | 2755 | `/*` |
|        - | 2756 | ` * Initialize a foreign function call context.` |
|        - | 2757 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - | 2758 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - | 2759 | ` * functions.` |
|        - | 2760 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - | 2761 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - | 2762 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - | 2763 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - | 2764 | ` */` |
|   938808 | 2765 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|        - | 2766 | `	ph7_context *pOut,    /* Call Context */` |
|        - | 2767 | `	ph7_vm *pVm,          /* Target VM */` |
|        - | 2768 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - | 2769 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - | 2770 | `	sxi32 iFlags          /* Control flags */` |
|        - | 2771 | `	)` |
|        5 | 2772 | `{` |
|   938813 | 2773 | `	pOut->pFunc = pFunc;` |
|   938813 | 2774 | `	pOut->pVm   = pVm;` |
|   938813 | 2775 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   938813 | 2776 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - | 2777 | `	/* Assume a null return value */` |
|   938813 | 2778 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   938813 | 2779 | `	pOut->pRet = pRet;` |
|   938813 | 2780 | `	pOut->iFlags = iFlags;` |
|   938813 | 2781 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|   938813 | 2782 | `	return SXRET_OK;` |
|        5 | 2783 | `}` |
|        - | 2784 | `/*` |
|        - | 2785 | ` * Release a foreign function call context and cleanup the mess` |
|        - | 2786 | ` * left behind.` |
|        - | 2787 | ` */` |
|   938808 | 2788 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 | 2789 | `{` |
|        - | 2790 | `	sxu32 n;` |
|   938813 | 2791 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    11791 | 2792 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    68574 | 2793 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    56788 | 2794 | `			if( apObj[n] == 0 ){` |
|        - | 2795 | `				/* Already released */` |
|      682 | 2796 | `				continue;` |
|        - | 2797 | `			}` |
|    56111 | 2798 | `			PH7_MemObjRelease(apObj[n]);` |
|    56111 | 2799 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    28058 | 2800 | `		}` |
|    11791 | 2801 | `		SySetRelease(&pCtx->sVar);` |
|     5893 | 2802 | `	}` |
|   938813 | 2803 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - | 2804 | `		ph7_aux_data *aAux;` |
|        - | 2805 | `		void *pChunk;` |
|        - | 2806 | `		/* Automatic release of dynamically allocated chunk` |
|        - | 2807 | `		 * using [ph7_context_alloc_chunk()].` |
|        - | 2808 | `		 */` |
|      119 | 2809 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      353 | 2810 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      239 | 2811 | `			pChunk = aAux[n].pAuxData;` |
|        - | 2812 | `			/* Release the chunk */` |
|      239 | 2813 | `			if( pChunk ){` |
|      239 | 2814 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      117 | 2815 | `			}` |
|      122 | 2816 | `		}` |
|      119 | 2817 | `		SySetRelease(&pCtx->sChunk);` |
|       57 | 2818 | `	}` |
|   938813 | 2819 | `}` |
|        - | 2820 | `/*` |
|        - | 2821 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - | 2822 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - | 2823 | ` */` |
|      677 | 2824 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - | 2825 | `	ph7_context *pCtx, /* Call context */` |
|        - | 2826 | `	ph7_value *pValue  /* Release this value */` |
|        - | 2827 | `	)` |
|        5 | 2828 | `{` |
|      682 | 2829 | `	if( pValue == 0 ){` |
|        - | 2830 | `		/* NULL value is a harmless operation */` |
|      ! 0 | 2831 | `		return;` |
|        - | 2832 | `	}` |
|      682 | 2833 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      682 | 2834 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - | 2835 | `		sxu32 n;` |
|     1536 | 2836 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1536 | 2837 | `			if( apObj[n] == pValue ){` |
|      682 | 2838 | `				PH7_MemObjRelease(pValue);` |
|      682 | 2839 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - | 2840 | `				/* Mark as released */` |
|      682 | 2841 | `				apObj[n] = 0;` |
|      682 | 2842 | `				break;` |
|        - | 2843 | `			}` |
|      431 | 2844 | `		}` |
|      338 | 2845 | `	}` |
|      343 | 2846 | `}` |
|        - | 2847 | `/*` |
|        - | 2848 | ` * Pop and release as many memory object from the operand stack.` |
|        - | 2849 | ` */` |
|  5845116 | 2850 | `PH7_PRIVATE void VmPopOperand(` |
|        - | 2851 | `	ph7_value **ppTos, /* Operand stack */` |
|        - | 2852 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - | 2853 | `	)` |
|        5 | 2854 | `{` |
|  5845121 | 2855 | `	ph7_value *pTos = *ppTos;` |
| 12423359 | 2856 | `	while( nPop > 0 ){` |
|  6578243 | 2857 | `		PH7_MemObjRelease(pTos);` |
|  6578243 | 2858 | `		pTos--;` |
|  6578243 | 2859 | `		nPop--;` |
|        5 | 2860 | `	}` |
|        - | 2861 | `	/* Top of the stack */` |
|  5845121 | 2862 | `	*ppTos = pTos;` |
|  5845121 | 2863 | `}` |
|        - | 2864 | `/*` |
|        - | 2865 | ` * Reserve a memory object.` |
|        - | 2866 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 2867 | ` */` |
|  4389191 | 2868 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 | 2869 | `{` |
|  4389196 | 2870 | `	ph7_value *pObj = 0;` |
|        - | 2871 | `	VmSlot *pSlot;` |
|        - | 2872 | `	sxu32 nIdx;` |
|        - | 2873 | `	/* Check for a free slot */` |
|  4389196 | 2874 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  4389196 | 2875 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  4389196 | 2876 | `	if( pSlot ){` |
|  2151494 | 2877 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  2151494 | 2878 | `		nIdx = pSlot->nIdx;` |
|  1076365 | 2879 | `	}` |
|  4389196 | 2880 | `	if( pObj == 0 ){` |
|        - | 2881 | `		/* Reserve a new memory object */` |
|  2237707 | 2882 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2237707 | 2883 | `		if( pObj == 0 ){` |
|      ! 0 | 2884 | `			return 0;` |
|        - | 2885 | `		}` |
|  1118851 | 2886 | `	}` |
|        - | 2887 | `	/* Set a null default value */` |
|  4389196 | 2888 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  4389196 | 2889 | `	pObj->nIdx = nIdx;` |
|  4389196 | 2890 | `	return pObj;` |
|  2195221 | 2891 | `}` |
|        - | 2892 | `/*` |
|        - | 2893 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - | 2894 | ` */` |
|    46254 | 2895 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|        - | 2896 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - | 2897 | `	const char *zKey,  /* Entry key */` |
|        - | 2898 | `	sxu32 nByte,       /* Key length */` |
|        - | 2899 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - | 2900 | `	)` |
|        5 | 2901 | `{` |
|        - | 2902 | `	ph7_value sKey;` |
|        - | 2903 | `	sxi32 rc;` |
|    46259 | 2904 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    46259 | 2905 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - | 2906 | `	/* Perform the insertion */` |
|    46259 | 2907 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    46259 | 2908 | `	PH7_MemObjRelease(&sKey);` |
|    46259 | 2909 | `	return rc;` |
|        5 | 2910 | `}` |
|        - | 2911 | `/*` |
|        - | 2912 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|        - | 2913 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|        - | 2914 | ` * key must create a real global variable — linked into the bottom frame's` |
|        - | 2915 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|        - | 2916 | ` * variable created by top-level code — so later reads and writes alias one` |
|        - | 2917 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|        - | 2918 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|        - | 2919 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|        - | 2920 | ` *     superglobal in place.` |
|        - | 2921 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|        - | 2922 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING` |
|        - | 2923 | ` *     name is rejected with the engine's usual "already exists" diagnostic` |
|        - | 2924 | ` *     (the same limitation OP_STORE_REF has for plain variables).` |
|        - | 2925 | ` */` |
|      152 | 2926 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|        2 | 2927 | `{` |
|      154 | 2928 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 2929 | `	SyHashEntry *pEntry;` |
|        - | 2930 | `	ph7_value *pObj;` |
|        - | 2931 | `	char *zDup;` |
|        - | 2932 | `	sxu32 nIdx;` |
|        - | 2933 | `	sxi32 rc;` |
|        - | 2934 | `	/* Walk down to the global frame */` |
|      158 | 2935 | `	while( pFrame->pParent ){` |
|        5 | 2936 | `		pFrame = pFrame->pParent;` |
|        1 | 2937 | `	}` |
|        - | 2938 | `	/* An existing global (or superglobal) is overwritten in place */` |
|      154 | 2939 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      154 | 2940 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|        - | 2941 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|        - | 2942 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|        - | 2943 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|        - | 2944 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|        - | 2945 | `		 * of $GLOBALS itself). */` |
|        5 | 2946 | `		pEntry = 0;` |
|        2 | 2947 | `	}` |
|      154 | 2948 | `	if( pEntry == 0 ){` |
|      154 | 2949 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       76 | 2950 | `	}` |
|      154 | 2951 | `	if( pEntry ){` |
|        3 | 2952 | `		if( nRefIdx != SXU32_HIGH ){` |
|        - | 2953 | `			SyString sName;` |
|      ! 0 | 2954 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|      ! 0 | 2955 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 | 2956 | `			return SXRET_OK;` |
|        - | 2957 | `		}` |
|        3 | 2958 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|        3 | 2959 | `		if( pObj == 0 ){` |
|      ! 0 | 2960 | `			return SXERR_NOTFOUND;` |
|        - | 2961 | `		}` |
|        3 | 2962 | `		if( pValue ){` |
|        3 | 2963 | `			PH7_MemObjStore(pValue,pObj);` |
|        2 | 2964 | `		}else{` |
|      ! 0 | 2965 | `			PH7_MemObjToNull(pObj);` |
|        - | 2966 | `		}` |
|        3 | 2967 | `		return SXRET_OK;` |
|        - | 2968 | `	}` |
|      152 | 2969 | `	if( nRefIdx == SXU32_HIGH ){` |
|        - | 2970 | `		/* Reserve a fresh slot for the new global */` |
|      150 | 2971 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|      150 | 2972 | `		if( pObj == 0 ){` |
|      ! 0 | 2973 | `			return SXERR_MEM;` |
|        - | 2974 | `		}` |
|      150 | 2975 | `		nIdx = pObj->nIdx;` |
|       76 | 2976 | `	}else{` |
|        - | 2977 | `		/* Reference assignment: bind the name to the existing slot */` |
|        3 | 2978 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|        3 | 2979 | `		if( pObj == 0 ){` |
|      ! 0 | 2980 | `			return SXERR_NOTFOUND;` |
|        - | 2981 | `		}` |
|        3 | 2982 | `		nIdx = nRefIdx;` |
|        - | 2983 | `	}` |
|      152 | 2984 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|      152 | 2985 | `	if( zDup == 0 ){` |
|      ! 0 | 2986 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2987 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|        - | 2988 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|        - | 2989 | `			VmSlot sFree;` |
|      ! 0 | 2990 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 2991 | `			sFree.pUserData = 0;` |
|      ! 0 | 2992 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 2993 | `		}` |
|      ! 0 | 2994 | `		return SXERR_MEM;` |
|        - | 2995 | `	}` |
|      152 | 2996 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|      152 | 2997 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2998 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2999 | `			VmSlot sFree;` |
|      ! 0 | 3000 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 3001 | `			sFree.pUserData = 0;` |
|      ! 0 | 3002 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 3003 | `		}` |
|      ! 0 | 3004 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 3005 | `		return rc;` |
|        - | 3006 | `	}` |
|        - | 3007 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|      152 | 3008 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|      152 | 3009 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      152 | 3010 | `	if( nRefIdx == SXU32_HIGH ){` |
|      150 | 3011 | `		pObj->nIdx = nIdx;` |
|      150 | 3012 | `		if( pValue ){` |
|      150 | 3013 | `			PH7_MemObjStore(pValue,pObj);` |
|       74 | 3014 | `		}` |
|       74 | 3015 | `	}` |
|      152 | 3016 | `	return SXRET_OK;` |
|       78 | 3017 | `}` |
|        - | 3018 | `/*` |
|        - | 3019 | ` * Extract a variable value from the top active VM frame.` |
|        - | 3020 | ` * Return a pointer to the variable value on success.` |
|        - | 3021 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - | 3022 | ` */` |
|  5944905 | 3023 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|        - | 3024 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 3025 | `	const SyString *pName, /* Variable name */` |
|        - | 3026 | `	int bDup,              /* True to duplicate variable name */` |
|        - | 3027 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - | 3028 | `	)` |
|        5 | 3029 | `{` |
|  5944910 | 3030 | `	int bNullify = FALSE;` |
|        - | 3031 | `	SyHashEntry *pEntry;` |
|        - | 3032 | `	VmFrame *pFrame;` |
|        - | 3033 | `	ph7_value *pObj;` |
|        - | 3034 | `	sxu32 nIdx;` |
|        - | 3035 | `	sxi32 rc;` |
|        - | 3036 | `	/* Point to the top active frame */` |
|  5944910 | 3037 | `	pFrame = pVm->pFrame;` |
|  5944910 | 3038 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 3039 | `	/* Perform the lookup */` |
|  5944910 | 3040 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - | 3041 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 | 3042 | `		pName = &sAnnon;` |
|        - | 3043 | `		/* Always nullify the object */` |
|      ! 0 | 3044 | `		bNullify = TRUE;` |
|      ! 0 | 3045 | `		bDup = FALSE;` |
|      ! 0 | 3046 | `	}` |
|        - | 3047 | `	/* Check the superglobals table first */` |
|  5944910 | 3048 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  5944910 | 3049 | `	if( pEntry == 0 ){` |
|        - | 3050 | `		/* Query the top active frame */` |
|  5944486 | 3051 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  5944486 | 3052 | `		if( pEntry == 0 ){` |
|   516189 | 3053 | `			char *zName = (char *)pName->zString;` |
|        - | 3054 | `			VmSlot sLocal;` |
|   516189 | 3055 | `			if( !bCreate ){` |
|        - | 3056 | `				/* Do not create the variable,return NULL instead */` |
|     1191 | 3057 | `				return 0;` |
|        - | 3058 | `			}` |
|        - | 3059 | `			/* No such variable,automatically create a new one and install` |
|        - | 3060 | `			 * it in the current frame.` |
|        - | 3061 | `			 */` |
|   515003 | 3062 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   515003 | 3063 | `			if( pObj == 0 ){` |
|      ! 0 | 3064 | `				return 0;` |
|        - | 3065 | `			}` |
|   515003 | 3066 | `			nIdx = pObj->nIdx;` |
|   515003 | 3067 | `			if( bDup ){` |
|        - | 3068 | `				/* Duplicate name */` |
|      540 | 3069 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      540 | 3070 | `				if( zName == 0 ){` |
|      ! 0 | 3071 | `					return 0;` |
|        - | 3072 | `				}` |
|      268 | 3073 | `			}` |
|        - | 3074 | `			/* Link to the top active VM frame */` |
|   515003 | 3075 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   515003 | 3076 | `			if( rc != SXRET_OK ){` |
|        - | 3077 | `				/* Return the slot to the free pool */` |
|      ! 0 | 3078 | `				sLocal.nIdx = nIdx;` |
|      ! 0 | 3079 | `				sLocal.pUserData = 0;` |
|      ! 0 | 3080 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 | 3081 | `				return 0;` |
|        - | 3082 | `			}` |
|   515003 | 3083 | `			if( pFrame->pParent != 0 ){` |
|        - | 3084 | `				/* Local variable */` |
|   506431 | 3085 | `				sLocal.nIdx = nIdx;` |
|   506431 | 3086 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   253839 | 3087 | `			}else{` |
|        - | 3088 | `				/* Register in the $GLOBALS array */` |
|     8577 | 3089 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - | 3090 | `			}` |
|        - | 3091 | `			/* Install in the reference table */` |
|   515003 | 3092 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - | 3093 | `			/* Save object index */` |
|   515003 | 3094 | `			pObj->nIdx = nIdx;` |
|   258125 | 3095 | `		}else{` |
|        - | 3096 | `			/* Extract variable contents */` |
|  5428302 | 3097 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5428302 | 3098 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  5428302 | 3099 | `			if( bNullify && pObj ){` |
|      ! 0 | 3100 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 | 3101 | `			}` |
|        - | 3102 | `		}` |
|  2974349 | 3103 | `	}else{` |
|        - | 3104 | `		/* Superglobal */` |
|      429 | 3105 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      429 | 3106 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 3107 | `	}` |
|  5943724 | 3108 | `	return pObj;` |
|  2975154 | 3109 | `}` |
|        - | 3110 | `/*` |
|        - | 3111 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - | 3112 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - | 3113 | ` */` |
|    31026 | 3114 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - | 3115 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3116 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - | 3117 | `	sxu32 nByte        /* zName length */` |
|        - | 3118 | `	)` |
|        5 | 3119 | `{` |
|        - | 3120 | `	SyHashEntry *pEntry;` |
|        - | 3121 | `	ph7_value *pValue;` |
|        - | 3122 | `	sxu32 nIdx;` |
|        - | 3123 | `	/* Query the superglobal table */` |
|    31031 | 3124 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    31031 | 3125 | `	if( pEntry == 0 ){` |
|        - | 3126 | `		/* No such entry */` |
|      ! 0 | 3127 | `		return 0;` |
|        - | 3128 | `	}` |
|        - | 3129 | `	/* Extract the superglobal index in the global object pool */` |
|    31031 | 3130 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3131 | `	/* Extract the variable value  */` |
|    31031 | 3132 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    31031 | 3133 | `	return pValue;` |
|    15518 | 3134 | `}` |
|        - | 3135 | `/*` |
|        - | 3136 | ` * Perform a raw hashmap insertion.` |
|        - | 3137 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - | 3138 | ` */` |
|    24308 | 3139 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - | 3140 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - | 3141 | `	const char *zKey,   /* Entry key */` |
|        - | 3142 | `	int nKeylen,        /* zKey length*/` |
|        - | 3143 | `	const char *zData,  /* Entry data */` |
|        - | 3144 | `	int nLen            /* zData length */` |
|        - | 3145 | `	)` |
|        5 | 3146 | `{` |
|        - | 3147 | `	ph7_value sKey,sValue;` |
|        - | 3148 | `	sxi32 rc;` |
|    24313 | 3149 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    24313 | 3150 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|    24313 | 3151 | `	if( zKey ){` |
|    20905 | 3152 | `		if( nKeylen < 0 ){` |
|    20801 | 3153 | `			nKeylen = (int)SyStrlen(zKey);` |
|    10398 | 3154 | `		}` |
|    20905 | 3155 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|    10450 | 3156 | `	}` |
|    24313 | 3157 | `	if( zData ){` |
|    24313 | 3158 | `		if( nLen < 0 ){` |
|        - | 3159 | `			/* Compute length automatically */` |
|    13795 | 3160 | `			nLen = (int)SyStrlen(zData);` |
|     6895 | 3161 | `		}` |
|    24313 | 3162 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|    12154 | 3163 | `	}` |
|        - | 3164 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|        - | 3165 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|        - | 3166 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|        - | 3167 | `	 * every argument under "". */` |
|    24313 | 3168 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|    24313 | 3169 | `	PH7_MemObjRelease(&sKey);` |
|    24313 | 3170 | `	PH7_MemObjRelease(&sValue);` |
|    24313 | 3171 | `	return rc;` |
|        5 | 3172 | `}` |
|        - | 3173 | `/*` |
|        - | 3174 | ` * Configure a working virtual machine instance.` |
|        - | 3175 | ` *` |
|        - | 3176 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - | 3177 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - | 3178 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - | 3179 | ` * The second argument to this function is an integer configuration option` |
|        - | 3180 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - | 3181 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - | 3182 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - | 3183 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - | 3184 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - | 3185 | ` */` |
|    89082 | 3186 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - | 3187 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 3188 | `	sxi32 nOp,   /* Configuration verb */` |
|        - | 3189 | `	va_list ap   /* Subsequent option arguments */` |
|        - | 3190 | `	)` |
|        5 | 3191 | `{` |
|    89087 | 3192 | `	sxi32 rc = SXRET_OK;` |
|    89087 | 3193 | `	switch(nOp){` |
|     1688 | 3194 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     3381 | 3195 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     3381 | 3196 | `		void *pUserData = va_arg(ap,void *);` |
|        - | 3197 | `		/* VM output consumer callback */` |
|        - | 3198 | `#ifdef UNTRUST` |
|        - | 3199 | `		if( xConsumer == 0 ){` |
|        - | 3200 | `			rc = SXERR_CORRUPT;` |
|        - | 3201 | `			break;` |
|        - | 3202 | `		}` |
|        - | 3203 | `#endif` |
|        - | 3204 | `		/* Install the output consumer */` |
|     3381 | 3205 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     3381 | 3206 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     3381 | 3207 | `		break;` |
|        - | 3208 | `							   }` |
|     1701 | 3209 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - | 3210 | `		/* Import path */` |
|        - | 3211 | `		  const char *zPath;` |
|        - | 3212 | `		  SyString sPath;` |
|     3407 | 3213 | `		  zPath = va_arg(ap,const char *);` |
|        - | 3214 | `#if defined(UNTRUST)` |
|        - | 3215 | `		  if( zPath == 0 ){` |
|        - | 3216 | `			  rc = SXERR_EMPTY;` |
|        - | 3217 | `			  break;` |
|        - | 3218 | `		  }` |
|        - | 3219 | `#endif` |
|     3407 | 3220 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - | 3221 | `		  /* Remove trailing slashes and backslashes */` |
|        - | 3222 | `#ifdef __WINNT__` |
|        5 | 3223 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - | 3224 | `#endif` |
|     6809 | 3225 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - | 3226 | `		  /* Remove leading and trailing white spaces */` |
|     3407 | 3227 | `		  SyStringFullTrim(&sPath);` |
|     3407 | 3228 | `		  if( sPath.nByte > 0 ){` |
|        - | 3229 | `			  /* Store the path in the corresponding conatiner */` |
|     3407 | 3230 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1701 | 3231 | `		  }` |
|     3407 | 3232 | `		  break;` |
|        - | 3233 | `									 }` |
|     1704 | 3234 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - | 3235 | `		/* Run-Time Error report */` |
|     3413 | 3236 | `		pVm->bErrReport = 1;` |
|     3413 | 3237 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|     3413 | 3238 | `		break;` |
|        2 | 3239 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - | 3240 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|        - | 3241 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|        - | 3242 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|        - | 3243 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|        - | 3244 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|        - | 3245 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|        - | 3246 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|        - | 3247 | `		 * would otherwise read as an enormous positive cap). */` |
|        5 | 3248 | `		int nDepth = va_arg(ap,int);` |
|        5 | 3249 | `		if( nDepth >= 0 ){` |
|        5 | 3250 | `			pVm->nMaxDepth = nDepth;` |
|        2 | 3251 | `		}` |
|        5 | 3252 | `		break;` |
|        - | 3253 | `									   }` |
|        5 | 3254 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|        - | 3255 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|        - | 3256 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|        - | 3257 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|        - | 3258 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|        - | 3259 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|        - | 3260 | `		 * so it is rejected as a footgun). */` |
|       12 | 3261 | `		int nDepth = va_arg(ap,int);` |
|       12 | 3262 | `		if( nDepth > 1 ){` |
|       12 | 3263 | `			pVm->nMaxNativeDepth = nDepth;` |
|        5 | 3264 | `		}` |
|       12 | 3265 | `		break;` |
|        - | 3266 | `									   }` |
|      ! 0 | 3267 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - | 3268 | `		/* VM output length in bytes */` |
|      ! 0 | 3269 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - | 3270 | `#ifdef UNTRUST` |
|        - | 3271 | `		if( pOut == 0 ){` |
|        - | 3272 | `			rc = SXERR_CORRUPT;` |
|        - | 3273 | `			break;` |
|        - | 3274 | `		}` |
|        - | 3275 | `#endif` |
|      ! 0 | 3276 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 | 3277 | `		break;` |
|        - | 3278 | `							   }` |
|        - | 3279 |  |
|    18748 | 3280 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - | 3281 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - | 3282 | `		/* Create a new superglobal/global variable */` |
|    37501 | 3283 | `		const char *zName = va_arg(ap,const char *);` |
|    37501 | 3284 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - | 3285 | `		SyHashEntry *pEntry;` |
|        - | 3286 | `		ph7_value *pObj;` |
|        - | 3287 | `		sxu32 nByte;` |
|        - | 3288 | `		sxu32 nIdx;` |
|        - | 3289 | `#ifdef UNTRUST` |
|        - | 3290 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - | 3291 | `			rc = SXERR_CORRUPT;` |
|        - | 3292 | `			break;` |
|        - | 3293 | `		}` |
|        - | 3294 | `#endif` |
|    37501 | 3295 | `		nByte = SyStrlen(zName);` |
|    37501 | 3296 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3297 | `			/* Check if the superglobal is already installed */` |
|    34125 | 3298 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    17065 | 3299 | `		}else{` |
|        - | 3300 | `			/* Query the top active VM frame */` |
|     3381 | 3301 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - | 3302 | `		}` |
|    37501 | 3303 | `		if( pEntry ){` |
|        - | 3304 | `			/* Variable already installed */` |
|      ! 0 | 3305 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3306 | `			/* Extract contents */` |
|      ! 0 | 3307 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 | 3308 | `			if( pObj ){` |
|        - | 3309 | `				/* Overwrite old contents */` |
|      ! 0 | 3310 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 | 3311 | `			}` |
|      ! 0 | 3312 | `		}else{` |
|        - | 3313 | `			/* Install a new variable */` |
|    37501 | 3314 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    37501 | 3315 | `			if( pObj == 0 ){` |
|      ! 0 | 3316 | `				rc = SXERR_MEM;` |
|      ! 0 | 3317 | `				break;` |
|        - | 3318 | `			}` |
|    37501 | 3319 | `			nIdx = pObj->nIdx;` |
|        - | 3320 | `			/* Copy value */` |
|    37501 | 3321 | `			PH7_MemObjStore(pValue,pObj);` |
|    37501 | 3322 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3323 | `				/* Install the superglobal */` |
|    34125 | 3324 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    17065 | 3325 | `			}else{` |
|        - | 3326 | `				/* Install in the current frame */` |
|     3381 | 3327 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - | 3328 | `			}` |
|    37501 | 3329 | `			if( rc == SXRET_OK ){` |
|        - | 3330 | `				SyHashEntry *pRef;` |
|    37501 | 3331 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    34125 | 3332 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    17065 | 3333 | `				}else{` |
|     3381 | 3334 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - | 3335 | `				}` |
|        - | 3336 | `				/* Install in the reference table */` |
|    37501 | 3337 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    37501 | 3338 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - | 3339 | `					/* Register in the $GLOBALS array */` |
|    37501 | 3340 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    18748 | 3341 | `				}` |
|    18748 | 3342 | `			}` |
|        - | 3343 | `		}` |
|    37501 | 3344 | `		break;` |
|        - | 3345 | `									}` |
|    10398 | 3346 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - | 3347 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - | 3348 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - | 3349 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - | 3350 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - | 3351 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - | 3352 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|    20801 | 3353 | `		const char *zKey   = va_arg(ap,const char *);` |
|    20801 | 3354 | `		const char *zValue = va_arg(ap,const char *);` |
|    20801 | 3355 | `		int nLen = va_arg(ap,int);` |
|        - | 3356 | `		ph7_hashmap *pMap;` |
|        - | 3357 | `		ph7_value *pValue;` |
|    20801 | 3358 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - | 3359 | `			/* Extract the $_ENV superglobal */` |
|        3 | 3360 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|    20800 | 3361 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - | 3362 | `			/* Extract the $_POST superglobal */` |
|      ! 0 | 3363 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|    20799 | 3364 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - | 3365 | `			/* Extract the $_GET superglobal */` |
|      ! 0 | 3366 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    20799 | 3367 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - | 3368 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 | 3369 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    20799 | 3370 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - | 3371 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 | 3372 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|    20799 | 3373 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - | 3374 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 | 3375 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 | 3376 | `		}else{` |
|        - | 3377 | `			/* Extract the $_SERVER superglobal */` |
|    20799 | 3378 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - | 3379 | `		}` |
|    20801 | 3380 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3381 | `			/* No such entry */` |
|      ! 0 | 3382 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3383 | `			break;` |
|        - | 3384 | `		}` |
|        - | 3385 | `		/* Point to the hashmap */` |
|    20801 | 3386 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3387 | `		/* Perform the insertion */` |
|    20801 | 3388 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|    20801 | 3389 | `		break;` |
|        - | 3390 | `								   }` |
|     1705 | 3391 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - | 3392 | `		/* Script arguments */` |
|     3415 | 3393 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3394 | `		ph7_hashmap *pMap;` |
|        - | 3395 | `		ph7_value *pValue;` |
|        - | 3396 | `		sxu32 n;` |
|     3415 | 3397 | `		if( SX_EMPTY_STR(zValue) ){` |
|        2 | 3398 | `			rc = SXERR_EMPTY;` |
|        2 | 3399 | `			break;` |
|        - | 3400 | `		}` |
|        - | 3401 | `		/* Extract the $argv array */` |
|     3413 | 3402 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3413 | 3403 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3404 | `			/* No such entry */` |
|      ! 0 | 3405 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3406 | `			break;` |
|        - | 3407 | `		}` |
|        - | 3408 | `		/* Point to the hashmap */` |
|     3413 | 3409 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3410 | `		/* Perform the insertion */` |
|     3413 | 3411 | `		n = (sxu32)SyStrlen(zValue);` |
|     3413 | 3412 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|     3413 | 3413 | `		if( rc == SXRET_OK ){` |
|     3413 | 3414 | `			if( pMap->nEntry > 1 ){` |
|        - | 3415 | `				/* Append space separator first */` |
|       37 | 3416 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|       16 | 3417 | `			}` |
|     3413 | 3418 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|     1704 | 3419 | `		}` |
|     3413 | 3420 | `		break;` |
|        - | 3421 | `								  }` |
|     1688 | 3422 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|        - | 3423 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|        - | 3424 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|        - | 3425 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|        - | 3426 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|        - | 3427 | `		ph7_value *pArgv,*pServer;` |
|        - | 3428 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|        - | 3429 | `		ph7_value sArgvVal,sKey,sCount;` |
|     3381 | 3430 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3381 | 3431 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|     3376 | 3432 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|     3381 | 3433 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 3434 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3435 | `			break;` |
|        - | 3436 | `		}` |
|     3381 | 3437 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|     3381 | 3438 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|        - | 3439 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|     3381 | 3440 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|     3381 | 3441 | `		if( pDup == 0 ){` |
|      ! 0 | 3442 | `			rc = SXERR_MEM;` |
|      ! 0 | 3443 | `			break;` |
|        - | 3444 | `		}` |
|     3381 | 3445 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|     3381 | 3446 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|     3381 | 3447 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3381 | 3448 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|     3381 | 3449 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|     3381 | 3450 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|     3381 | 3451 | `		PH7_MemObjRelease(&sKey);` |
|        - | 3452 | `		/* $_SERVER['argc'] = count($argv). */` |
|     3381 | 3453 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|     3381 | 3454 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3381 | 3455 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|     3381 | 3456 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|     3381 | 3457 | `		PH7_MemObjRelease(&sCount);` |
|     3381 | 3458 | `		PH7_MemObjRelease(&sKey);` |
|     3381 | 3459 | `		rc = SXRET_OK;` |
|     3381 | 3460 | `		break;` |
|        - | 3461 | `								  }` |
|       29 | 3462 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|        - | 3463 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|        - | 3464 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|        - | 3465 | `		 * apply immediately so they take effect even if the script never` |
|        - | 3466 | `		 * touches the INI API. */` |
|       62 | 3467 | `		const char *zName = va_arg(ap,const char *);` |
|       62 | 3468 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3469 | `		VmIniEntry sEntry;` |
|        - | 3470 | `		char *zDupN,*zDupV;` |
|        - | 3471 | `		sxu32 nName,nValue;` |
|       62 | 3472 | `		if( SX_EMPTY_STR(zName) ){` |
|      ! 0 | 3473 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3474 | `			break;` |
|        - | 3475 | `		}` |
|       62 | 3476 | `		if( zValue == 0 ){` |
|      ! 0 | 3477 | `			zValue = "";` |
|      ! 0 | 3478 | `		}` |
|       62 | 3479 | `		nName = (sxu32)SyStrlen(zName);` |
|       62 | 3480 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       62 | 3481 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       62 | 3482 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       62 | 3483 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|      ! 0 | 3484 | `			rc = SXERR_MEM;` |
|      ! 0 | 3485 | `			break;` |
|        - | 3486 | `		}` |
|       62 | 3487 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       62 | 3488 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       62 | 3489 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       62 | 3490 | `		if( rc == SXRET_OK ){` |
|       58 | 3491 | `			if( nName == sizeof("error_reporting")-1` |
|       52 | 3492 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|        2 | 3493 | `				sxi64 iLevel = 0;` |
|        2 | 3494 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|        2 | 3495 | `				pVm->bErrReport = iLevel != 0;` |
|       57 | 3496 | `			}else if( nName == sizeof("date.timezone")-1` |
|       28 | 3497 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|      ! 0 | 3498 | `			 && nValue == 3` |
|        4 | 3499 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|      ! 0 | 3500 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|      ! 0 | 3501 | `				pVm->zDefTz[3] = 0;` |
|      ! 0 | 3502 | `				pVm->nDefTz = 3;` |
|       56 | 3503 | `			}else if( nName == sizeof("zend.assertions")-1` |
|       50 | 3504 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|        - | 3505 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|        - | 3506 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|        - | 3507 | `				 * effect even before the INI chunk is seeded. */` |
|       40 | 3508 | `				sxi64 iZend = 0;` |
|       40 | 3509 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|       40 | 3510 | `				if( iZend >= 1 ){` |
|       40 | 3511 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|       22 | 3512 | `				}else{` |
|      ! 0 | 3513 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|        - | 3514 | `				}` |
|       18 | 3515 | `			}` |
|       29 | 3516 | `		}` |
|       62 | 3517 | `		break;` |
|        - | 3518 | `								  }` |
|      ! 0 | 3519 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - | 3520 | `		/* error_log() consumer */` |
|      ! 0 | 3521 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 | 3522 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 | 3523 | `		break;` |
|        - | 3524 | `										}` |
|      ! 0 | 3525 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - | 3526 | `		/* Script return value */` |
|      ! 0 | 3527 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - | 3528 | `#ifdef UNTRUST` |
|        - | 3529 | `		if( ppValue == 0 ){` |
|        - | 3530 | `			rc = SXERR_CORRUPT;` |
|        - | 3531 | `			break;` |
|        - | 3532 | `		}` |
|        - | 3533 | `#endif` |
|      ! 0 | 3534 | `		*ppValue = &pVm->sExec;` |
|      ! 0 | 3535 | `		break;` |
|        - | 3536 | `								   }` |
|     6809 | 3537 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - | 3538 | `		/* Register an IO stream device */` |
|    13623 | 3539 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - | 3540 | `		/* Make sure we are dealing with a valid IO stream */` |
|    13618 | 3541 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|    13623 | 3542 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - | 3543 | `				/* Invalid stream */` |
|      ! 0 | 3544 | `				rc = SXERR_INVALID;` |
|      ! 0 | 3545 | `				break;` |
|        - | 3546 | `		}` |
|    13623 | 3547 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - | 3548 | `			/* Make the 'file://' stream the defaut stream device */` |
|     3409 | 3549 | `			pVm->pDefStream = pStream;` |
|     1702 | 3550 | `		}` |
|        - | 3551 | `		/* Insert in the appropriate container */` |
|    13623 | 3552 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|    13623 | 3553 | `		break;` |
|        - | 3554 | `								  }` |
|       16 | 3555 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - | 3556 | `		/* Point to the VM internal output consumer buffer */` |
|       32 | 3557 | `		const void **ppOut = va_arg(ap,const void **);` |
|       32 | 3558 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - | 3559 | `#ifdef UNTRUST` |
|        - | 3560 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - | 3561 | `			rc = SXERR_CORRUPT;` |
|        - | 3562 | `			break;` |
|        - | 3563 | `		}` |
|        - | 3564 | `#endif` |
|       32 | 3565 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       32 | 3566 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       32 | 3567 | `		break;` |
|        - | 3568 | `									   }` |
|       16 | 3569 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - | 3570 | `		/* Raw HTTP request*/` |
|       32 | 3571 | `		const char *zRequest = va_arg(ap,const char *);` |
|       32 | 3572 | `		int nByte = va_arg(ap,int);` |
|       32 | 3573 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 | 3574 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3575 | `			break;` |
|        - | 3576 | `		}` |
|       32 | 3577 | `		if( nByte < 0 ){` |
|        - | 3578 | `			/* Compute length automatically */` |
|      ! 0 | 3579 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 | 3580 | `		}` |
|        - | 3581 | `		/* Process the request */` |
|       32 | 3582 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - | 3583 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       32 | 3584 | `		if( rc == SXRET_OK ){` |
|       30 | 3585 | `			pVm->bHttpContext = 1;` |
|       15 | 3586 | `		}` |
|       32 | 3587 | `		break;` |
|        - | 3588 | `									}` |
|       16 | 3589 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - | 3590 | `		/* Extract HTTP response status code */` |
|       32 | 3591 | `		int *pStatus = va_arg(ap, int *);` |
|       32 | 3592 | `		if( pStatus ){` |
|       32 | 3593 | `			*pStatus = pVm->iResponseStatus;` |
|       16 | 3594 | `		}` |
|       32 | 3595 | `		break;` |
|        - | 3596 | `										}` |
|       16 | 3597 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - | 3598 | `		/* Iterate response headers via callback */` |
|        - | 3599 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       32 | 3600 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       32 | 3601 | `		void *pUserData = va_arg(ap, void *);` |
|       32 | 3602 | `		if( xCallback ){` |
|       32 | 3603 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       32 | 3604 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       44 | 3605 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 | 3606 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 | 3607 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 | 3608 | `							   pUserData);` |
|       12 | 3609 | `				if( rc != PH7_OK ){` |
|      ! 0 | 3610 | `					break;` |
|        - | 3611 | `				}` |
|        6 | 3612 | `			}` |
|       16 | 3613 | `		}` |
|       32 | 3614 | `		break;` |
|        - | 3615 | `										 }` |
|      ! 0 | 3616 | `	default:` |
|        - | 3617 | `		/* Unknown configuration option */` |
|      ! 0 | 3618 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 | 3619 | `		break;` |
|        - | 3620 | `	}` |
|    89087 | 3621 | `	return rc;` |
|        5 | 3622 | `}` |
|        - | 3623 | `/* Forward declaration */` |
|        - | 3624 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - | 3625 | `/*` |
|        - | 3626 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - | 3627 | ` * format.` |
|        - | 3628 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - | 3629 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - | 3630 | ` * (STDOUT).` |
|        - | 3631 | ` */` |
|        2 | 3632 | `static sxi32 VmByteCodeDump(` |
|        - | 3633 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - | 3634 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - | 3635 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 3636 | `	)` |
|        1 | 3637 | `{` |
|        - | 3638 | `	static const char zDump[] = {` |
|        - | 3639 | `		"====================================================\n"` |
|        - | 3640 | `		"PH7 VM Dump\n"` |
|        - | 3641 | `		"====================================================\n"` |
|        - | 3642 | `	};` |
|        - | 3643 | `	VmInstr *pInstr,*pEnd;` |
|        3 | 3644 | `	sxi32 rc = SXRET_OK;` |
|        - | 3645 | `	sxu32 n;` |
|        - | 3646 | `	/* Point to the PH7 instructions */` |
|        3 | 3647 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 | 3648 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 | 3649 | `	n = 0;` |
|        3 | 3650 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - | 3651 | `	/* Dump instructions */` |
|        7 | 3652 | `	for(;;){` |
|       15 | 3653 | `		if( pInstr >= pEnd ){` |
|        - | 3654 | `			/* No more instructions */` |
|        3 | 3655 | `			break;` |
|        - | 3656 | `		}` |
|        - | 3657 | `		/* Format and call the consumer callback */` |
|       19 | 3658 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|       12 | 3659 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 | 3660 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|       13 | 3661 | `		if( rc != SXRET_OK ){` |
|        - | 3662 | `			/* Consumer routine request an operation abort */` |
|      ! 0 | 3663 | `			return rc;` |
|        - | 3664 | `		}` |
|       13 | 3665 | `		++n;` |
|       13 | 3666 | `		pInstr++; /* Next instruction in the stream */` |
|        1 | 3667 | `	}` |
|        3 | 3668 | `	return rc;` |
|        2 | 3669 | `}` |
|        - | 3670 | `/*` |
|        - | 3671 | ` * Save the execution state of a fiber/generator context.` |
|        - | 3672 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - | 3673 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - | 3674 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - | 3675 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - | 3676 | ` * when VmByteCodeExec returns.` |
|        - | 3677 | ` */` |
|     1646 | 3678 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|        - | 3679 | `	ph7_vm *pVm,` |
|        - | 3680 | `	ph7_exec_ctx *pCtx,` |
|        - | 3681 | `	sxi32 pc,` |
|        - | 3682 | `	sxi32 nTos` |
|        - | 3683 | `	)` |
|        5 | 3684 | `{` |
|      823 | 3685 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|     1651 | 3686 | `	pCtx->pc = pc;` |
|     1651 | 3687 | `	pCtx->nTos = nTos;` |
|     1651 | 3688 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|     1651 | 3689 | `	return PH7_SUSPEND;` |
|        5 | 3690 | `}` |
|        - | 3691 | `/*` |
|        - | 3692 | ` * Resolve named-argument mapping.` |
|        - | 3693 | ` *` |
|        - | 3694 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - | 3695 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - | 3696 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - | 3697 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - | 3698 | ` * every formal parameter that received a value.` |
|        - | 3699 | ` *` |
|        - | 3700 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - | 3701 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|        - | 3702 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|        - | 3703 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|        - | 3704 | ` */` |
|      276 | 3705 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|        - | 3706 | `	ph7_vm *pVm,` |
|        - | 3707 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - | 3708 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - | 3709 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - | 3710 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - | 3711 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - | 3712 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - | 3713 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - | 3714 | `)` |
|        4 | 3715 | `{` |
|      280 | 3716 | `	sxi32 posIdx = 0;` |
|        - | 3717 | `	sxu32 i;` |
|      280 | 3718 | `	int bSeenNamed = 0;` |
|        - | 3719 | `	char zErrMsg[256];` |
|      280 | 3720 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|     1056 | 3721 | `	for( i = 0; i < nActual; i++ ){` |
|      780 | 3722 | `		aSlot[i] = -2;` |
|      392 | 3723 | `	}` |
|     1044 | 3724 | `	for( i = 0; i < nActual; i++ ){` |
|      999 | 3725 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - | 3726 | `			/* Named argument — find formal by name */` |
|      454 | 3727 | `			int found = 0;` |
|      454 | 3728 | `			bSeenNamed = 1;` |
|        - | 3729 | `			sxu32 k;` |
|      704 | 3730 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      602 | 3731 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      585 | 3732 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      560 | 3733 | `						pMap->aNames[i].zString,` |
|      840 | 3734 | `						pMap->aNames[i].nByte) == 0 ){` |
|      356 | 3735 | `					if( aUsed[k] ){` |
|       12 | 3736 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3737 | `							"Named parameter $%.*s overwrites previous argument",` |
|        6 | 3738 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        9 | 3739 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3740 | `					}` |
|      349 | 3741 | `					aSlot[i] = (sxi32)k;` |
|      349 | 3742 | `					aUsed[k] = 1;` |
|      349 | 3743 | `					found = 1;` |
|      349 | 3744 | `					break;` |
|        - | 3745 | `				}` |
|      128 | 3746 | `			}` |
|      448 | 3747 | `			if( !found ){` |
|      101 | 3748 | `				if( iVariadicIdx >= 0 ){` |
|       93 | 3749 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|       47 | 3750 | `				}else{` |
|       11 | 3751 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3752 | `						"Unknown named parameter $%.*s",` |
|        6 | 3753 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        8 | 3754 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3755 | `				}` |
|       46 | 3756 | `			}` |
|      222 | 3757 | `		}else{` |
|        - | 3758 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|        - | 3759 | `			 * named arg (the parser rejects it at compile time), but a call` |
|        - | 3760 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|        - | 3761 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|      329 | 3762 | `			if( bSeenNamed ){` |
|      ! 0 | 3763 | `				return VmThrowNamedArgError(&(*pVm),` |
|        - | 3764 | `					"Cannot use positional argument after named argument",` |
|        - | 3765 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|        - | 3766 | `			}` |
|      329 | 3767 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       51 | 3768 | `				if( aUsed[posIdx] ){` |
|      ! 0 | 3769 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3770 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 | 3771 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 | 3772 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3773 | `				}` |
|       51 | 3774 | `				aSlot[i] = posIdx;` |
|       51 | 3775 | `				aUsed[posIdx] = 1;` |
|      303 | 3776 | `			}else if( iVariadicIdx >= 0 ){` |
|      279 | 3777 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      139 | 3778 | `			}` |
|      329 | 3779 | `			posIdx++;` |
|        - | 3780 | `		}` |
|      386 | 3781 | `	}` |
|      267 | 3782 | `	return SXRET_OK;` |
|      142 | 3783 | `}` |
|        - | 3784 | `/*` |
|        - | 3785 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - | 3786 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - | 3787 | ` */` |
|      322 | 3788 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        5 | 3789 | `{` |
|      327 | 3790 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      314 | 3791 | `		return 0;` |
|        - | 3792 | `	}` |
|       15 | 3793 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|      166 | 3794 | `}` |
|        - | 3795 | `/*` |
|        - | 3796 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - | 3797 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - | 3798 | ` * preserved (later wins), integer keys are renumbered.` |
|        - | 3799 | ` */` |
|       10 | 3800 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3801 | `{` |
|       11 | 3802 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 | 3803 | `	(void)pVm;` |
|       11 | 3804 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 | 3805 | `	return SXRET_OK;` |
|        1 | 3806 | `}` |
|        - | 3807 | `/*` |
|        - | 3808 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - | 3809 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - | 3810 | ` */` |
|        6 | 3811 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3812 | `{` |
|        3 | 3813 | `	(void)pVm; (void)pKey;` |
|        7 | 3814 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 | 3815 | `	return SXRET_OK;` |
|        1 | 3816 | `}` |
|        - | 3817 | `/*` |
|        - | 3818 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|        - | 3819 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|        - | 3820 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|        - | 3821 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|        - | 3822 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|        - | 3823 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|        - | 3824 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|        - | 3825 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|        - | 3826 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|        - | 3827 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|        - | 3828 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|        - | 3829 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|        - | 3830 | ` */` |
|        - | 3831 | `/*` |
|        - | 3832 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|        - | 3833 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|        - | 3834 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|        - | 3835 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|        - | 3836 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|        - | 3837 | ` */` |
|      300 | 3838 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|        4 | 3839 | `{` |
|        - | 3840 | `	VmSpreadRun sRun;` |
|        - | 3841 | `	ph7_hashmap_node *pNode;` |
|        - | 3842 | `	sxu32 i;` |
|      304 | 3843 | `	sRun.pStart = pFirst;` |
|      304 | 3844 | `	sRun.nCount = nCount;` |
|      304 | 3845 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      304 | 3846 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      304 | 3847 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|      ! 0 | 3848 | `		return;` |
|        - | 3849 | `	}` |
|      304 | 3850 | `	pNode = pMap->pFirst;` |
|     2098 | 3851 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|        - | 3852 | `		VmSpreadKey sKey;` |
|     1798 | 3853 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|        - | 3854 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|        - | 3855 | `			 * the source map's release before CALL replays them. */` |
|       95 | 3856 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       95 | 3857 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       95 | 3858 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|       48 | 3859 | `		}else{` |
|        - | 3860 | `			/* Integer key (or empty-string key, treated positionally) */` |
|     1704 | 3861 | `			sKey.nOff = 0;` |
|     1704 | 3862 | `			sKey.nLen = 0;` |
|        - | 3863 | `		}` |
|     1798 | 3864 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|     1798 | 3865 | `		pNode = pNode->pPrev; /* forward link */` |
|      901 | 3866 | `	}` |
|      154 | 3867 | `}` |
|        - | 3868 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|        - | 3869 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        8 | 3870 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|      ! 0 | 3871 | `{` |
|        8 | 3872 | `	SySetReset(&pVm->aSpreadRun);` |
|        8 | 3873 | `	SySetReset(&pVm->aSpreadKey);` |
|        8 | 3874 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        8 | 3875 | `}` |
|        - | 3876 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|        - | 3877 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|        - | 3878 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|        - | 3879 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|        - | 3880 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|        - | 3881 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|        - | 3882 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|        - | 3883 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|        - | 3884 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|        - | 3885 | ` * slot — from being consumed by that nested call. */` |
|      508 | 3886 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|        4 | 3887 | `{` |
|      512 | 3888 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      512 | 3889 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|        - | 3890 | `	VmSpreadRun *aRun;` |
|      512 | 3891 | `	if( rStart >= nRun ){` |
|      224 | 3892 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|        - | 3893 | `	}` |
|      292 | 3894 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      292 | 3895 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      292 | 3896 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      292 | 3897 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|      144 | 3898 | `	}` |
|      292 | 3899 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|      258 | 3900 | `}` |
|        - | 3901 | `/*` |
|        - | 3902 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|        - | 3903 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|        - | 3904 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|        - | 3905 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|        - | 3906 | ` *` |
|        - | 3907 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|        - | 3908 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|        - | 3909 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|        - | 3910 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|        - | 3911 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|        - | 3912 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|        - | 3913 | ` * they are counted only by that call. This replaces the old shared` |
|        - | 3914 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|        - | 3915 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|        - | 3916 | ` *` |
|        - | 3917 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|        - | 3918 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|        - | 3919 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|        - | 3920 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|        - | 3921 | ` */` |
|      316 | 3922 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|        4 | 3923 | `{` |
|      320 | 3924 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 3925 | `	VmSpreadRun *aRun;` |
|      320 | 3926 | `	ph7_value *pEnd = pTos;` |
|      320 | 3927 | `	sxi32 nPos = iP1;` |
|      320 | 3928 | `	sxi32 ri, extra = 0;` |
|      320 | 3929 | `	if( nRun == 0 ){` |
|       15 | 3930 | `		pVm->nSpreadCallBase = 0;` |
|       15 | 3931 | `		return 0;` |
|        - | 3932 | `	}` |
|      306 | 3933 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      306 | 3934 | `	ri = (sxi32)nRun - 1;` |
|      736 | 3935 | `	while( nPos > 0 ){` |
|      434 | 3936 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|        - | 3937 | `			/* A non-empty unpack occupying nCount slots. */` |
|      268 | 3938 | `			pEnd = aRun[ri].pStart;` |
|      268 | 3939 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      268 | 3940 | `			ri--;` |
|      302 | 3941 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|        - | 3942 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       39 | 3943 | `			extra -= 1;` |
|       39 | 3944 | `			ri--;` |
|       21 | 3945 | `		}else{` |
|        - | 3946 | `			/* An ordinary single-slot argument. */` |
|      134 | 3947 | `			pEnd--;` |
|        - | 3948 | `		}` |
|      434 | 3949 | `		nPos--;` |
|        4 | 3950 | `	}` |
|        - | 3951 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|        - | 3952 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      306 | 3953 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      306 | 3954 | `	return extra;` |
|      162 | 3955 | `}` |
|      300 | 3956 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)` |
|        4 | 3957 | `{` |
|      304 | 3958 | `	ph7_value *pTos = *ppTos;` |
|      304 | 3959 | `	sxu32 nEntry = pMap->nEntry;` |
|      304 | 3960 | `	if( nEntry == 0 ){` |
|        - | 3961 | `		/* Nothing to unpack — remove the source from the stack */` |
|       39 | 3962 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       39 | 3963 | `		VmPopOperand(&pTos, 1);` |
|       21 | 3964 | `	}else{` |
|        - | 3965 | `		ph7_hashmap_node *pNode;` |
|        - | 3966 | `		ph7_value *pElem;` |
|        - | 3967 | `		sxu32 i;` |
|        - | 3968 | `		int bTemp;` |
|      268 | 3969 | `		pMap->iRef++;` |
|      268 | 3970 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|        - | 3971 | `		/* Record the run + element keys before any release (nodes still alive).` |
|        - | 3972 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|      268 | 3973 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|        - | 3974 | `		/* Overwrite the source slot with the first element */` |
|      268 | 3975 | `		pNode = pMap->pFirst;` |
|      268 | 3976 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      268 | 3977 | `		PH7_MemObjRelease(pTos);` |
|      268 | 3978 | `		if( pElem ){` |
|      268 | 3979 | `			if( bTemp ){` |
|      135 | 3980 | `				PH7_MemObjStore(pElem, pTos);` |
|       68 | 3981 | `			}else{` |
|      134 | 3982 | `				PH7_MemObjLoad(pElem, pTos);` |
|        - | 3983 | `			}` |
|      132 | 3984 | `		}` |
|      268 | 3985 | `		pTos->nIdx = SXU32_HIGH;` |
|        - | 3986 | `		/* Traverse in insertion order (pPrev is the forward link` |
|        - | 3987 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|      268 | 3988 | `		pNode = pNode->pPrev;` |
|        - | 3989 | `		/* Push the remaining elements */` |
|     1798 | 3990 | `		for( i = 1; i < nEntry; i++ ){` |
|     1534 | 3991 | `			pTos++;` |
|     1534 | 3992 | `			PH7_MemObjInit(pVm, pTos);` |
|     1534 | 3993 | `			pTos->nIdx = SXU32_HIGH;` |
|     1534 | 3994 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|     1534 | 3995 | `			if( pElem ){` |
|     1534 | 3996 | `				if( bTemp ){` |
|     1259 | 3997 | `					PH7_MemObjStore(pElem, pTos);` |
|      630 | 3998 | `				}else{` |
|      276 | 3999 | `					PH7_MemObjLoad(pElem, pTos);` |
|        - | 4000 | `				}` |
|      765 | 4001 | `			}` |
|     1534 | 4002 | `			pNode = pNode->pPrev;` |
|      769 | 4003 | `		}` |
|      268 | 4004 | `		PH7_HashmapUnref(pMap);` |
|        - | 4005 | `	}` |
|      304 | 4006 | `	*ppTos = pTos;` |
|      304 | 4007 | `}` |
|        - | 4008 | `/*` |
|        - | 4009 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|        - | 4010 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|        - | 4011 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|        - | 4012 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|        - | 4013 | ` * element keys, interleaving them with the compile-time names at their real` |
|        - | 4014 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|        - | 4015 | ` *` |
|        - | 4016 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|        - | 4017 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|        - | 4018 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|        - | 4019 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|        - | 4020 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|        - | 4021 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|        - | 4022 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|        - | 4023 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|        - | 4024 | ` * method-name slot pop shifts pArg).` |
|        - | 4025 | ` *` |
|        - | 4026 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|        - | 4027 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|        - | 4028 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|        - | 4029 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|        - | 4030 | ` * which is after this call's synchronous named-arg resolution.` |
|        - | 4031 | ` */` |
|      276 | 4032 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|        - | 4033 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|        4 | 4034 | `{` |
|      280 | 4035 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 4036 | `	VmSpreadRun *aRun;` |
|        - | 4037 | `	VmSpreadKey *aKey;` |
|        - | 4038 | `	const char *zKeyBase;` |
|      280 | 4039 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|      280 | 4040 | `	int bAnyNamed = 0;` |
|        - | 4041 | `	sxu32 ai, ci, ri, rStart;` |
|      280 | 4042 | `	if( nRun == 0 ){` |
|        - | 4043 | `		/* No spread captured at all — the compile map is already aligned. */` |
|      ! 0 | 4044 | `		return 0;` |
|        - | 4045 | `	}` |
|      280 | 4046 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      280 | 4047 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      280 | 4048 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|        - | 4049 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|        - | 4050 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|        - | 4051 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|        - | 4052 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      280 | 4053 | `	ri = pVm->nSpreadCallBase;` |
|      280 | 4054 | `	rStart = ri;` |
|      280 | 4055 | `	if( rStart >= nRun ){` |
|        - | 4056 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|      ! 0 | 4057 | `		return 0;` |
|        - | 4058 | `	}` |
|      280 | 4059 | `	SySetReset(&pVm->aEffArgName);` |
|      280 | 4060 | `	ci = 0;` |
|      280 | 4061 | `	ai = 0;` |
|      654 | 4062 | `	while( ai < nActual ){` |
|        - | 4063 | `		SyString sName;` |
|      378 | 4064 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|        - | 4065 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|        - | 4066 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      390 | 4067 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|       13 | 4068 | `			ci++; ri++;` |
|        1 | 4069 | `		}` |
|      378 | 4070 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|        - | 4071 | `			/* A run of spread elements: one name per element from its key. Keys` |
|        - | 4072 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|        - | 4073 | `			 * run never desyncs the key stream. */` |
|      268 | 4074 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|     2062 | 4075 | `			for( j = 0; j < K; j++ ){` |
|     1798 | 4076 | `				SyZero(&sName, sizeof(sName));` |
|     1798 | 4077 | `				if( aKey[ks + j].nLen > 0 ){` |
|       95 | 4078 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       95 | 4079 | `					bAnyNamed = 1;` |
|       47 | 4080 | `				}` |
|     1798 | 4081 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      901 | 4082 | `			}` |
|      268 | 4083 | `			ai += K;` |
|      268 | 4084 | `			ci++; ri++;` |
|      136 | 4085 | `		}else{` |
|        - | 4086 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      114 | 4087 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|       33 | 4088 | `				sName = pCompile->aNames[ci];` |
|       33 | 4089 | `				bAnyNamed = 1;` |
|       16 | 4090 | `			}` |
|      114 | 4091 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      114 | 4092 | `			ai++;` |
|      114 | 4093 | `			ci++;` |
|        - | 4094 | `		}` |
|        4 | 4095 | `	}` |
|        - | 4096 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|        - | 4097 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|        - | 4098 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|        - | 4099 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      280 | 4100 | `	VmSpreadConsume(pVm);` |
|      280 | 4101 | `	if( !bAnyNamed ){` |
|        - | 4102 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      210 | 4103 | `		return 0;` |
|        - | 4104 | `	}` |
|       71 | 4105 | `	pEff->bHasNamed = 1;` |
|       71 | 4106 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|       71 | 4107 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|       71 | 4108 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|       71 | 4109 | `	pEff->nTotal = nActual;` |
|       71 | 4110 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|       71 | 4111 | `	return 1;` |
|      142 | 4112 | `}` |
|        - | 4113 | `/*` |
|        - | 4114 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|        - | 4115 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|        - | 4116 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|        - | 4117 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|        - | 4118 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|        - | 4119 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|        - | 4120 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|        - | 4121 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|        - | 4122 | ` * pArg must be the site's FINAL argument base.` |
|        - | 4123 | ` */` |
|  1049827 | 4124 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|        - | 4125 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|        5 | 4126 | `{` |
|  1049832 | 4127 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|  1049832 | 4128 | `	if( pInstr->iP2 == 0 ){` |
|  1049530 | 4129 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|        - | 4130 | `	}` |
|      306 | 4131 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|       71 | 4132 | `		return pStorage;` |
|        - | 4133 | `	}` |
|      236 | 4134 | `	VmSpreadConsume(pVm);` |
|      236 | 4135 | `	return pCompile;` |
|   525387 | 4136 | `}` |
|        - | 4137 | `/*` |
|        - | 4138 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|        - | 4139 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|        - | 4140 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — the two paths` |
|        - | 4141 | ` * disagree on which scalar types are silent (positional silences null+bool; keyed silences` |
|        - | 4142 | ` * only null, warning for bool to match PHP) — this only maps the type name and emits.` |
|        - | 4143 | ` */` |
|      ! 0 | 4144 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|      ! 0 | 4145 | `{` |
|      ! 0 | 4146 | `	const char *zType = "unknown";` |
|        - | 4147 | `	char zMsg[64];` |
|      ! 0 | 4148 | `	if( iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 4149 | `		zType = "string";` |
|      ! 0 | 4150 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        - | 4151 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|        - | 4152 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|        - | 4153 | `		 * REAL flag, so it still falls through to the int arm. */` |
|      ! 0 | 4154 | `		zType = "float";` |
|      ! 0 | 4155 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 | 4156 | `		zType = "int";` |
|      ! 0 | 4157 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 4158 | `		zType = "bool";` |
|      ! 0 | 4159 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4160 | `		zType = "object";` |
|      ! 0 | 4161 | `	}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 | 4162 | `		zType = "resource";` |
|      ! 0 | 4163 | `	}` |
|      ! 0 | 4164 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|      ! 0 | 4165 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 | 4166 | `}` |
|        - | 4167 | `/*` |
|        - | 4168 | ` * A member access in isset()/empty() context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY) is a silent` |
|        - | 4169 | ` * lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class instance"` |
|        - | 4170 | ` * warnings, mirroring the array isset/empty path.` |
|        - | 4171 | ` */` |
|     1102 | 4172 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|        5 | 4173 | `{` |
|     1107 | 4174 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY;` |
|        5 | 4175 | `}` |
|        - | 4176 | `/*` |
|        - | 4177 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|        - | 4178 | ` * A __get body reading the SAME property of the SAME instance must not` |
|        - | 4179 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|        - | 4180 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|        - | 4181 | ` * reads (other names / other instances) still dispatch.` |
|        - | 4182 | ` */` |
|     1060 | 4183 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4184 | `{` |
|        - | 4185 | `	VmMagicGuard *aG;` |
|        - | 4186 | `	sxu32 nHash;` |
|        - | 4187 | `	sxu32 n;` |
|     1061 | 4188 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|        - | 4189 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|        - | 4190 | `		 * every hooked-property access consults the guard, often twice. */` |
|      881 | 4191 | `		return FALSE;` |
|        - | 4192 | `	}` |
|      181 | 4193 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|      181 | 4194 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      225 | 4195 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|      181 | 4196 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|      137 | 4197 | `			return TRUE;` |
|        - | 4198 | `		}` |
|       23 | 4199 | `	}` |
|       45 | 4200 | `	return FALSE;` |
|      531 | 4201 | `}` |
|      486 | 4202 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4203 | `{` |
|        - | 4204 | `	VmMagicGuard sG;` |
|      487 | 4205 | `	sG.pThis = pThis;` |
|      487 | 4206 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      487 | 4207 | `	sG.cKind = cKind;` |
|      487 | 4208 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      487 | 4209 | `}` |
|      486 | 4210 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|        1 | 4211 | `{` |
|      487 | 4212 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      487 | 4213 | `}` |
|        - | 4214 | `/*` |
|        - | 4215 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|        - | 4216 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|        - | 4217 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|        - | 4218 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|        - | 4219 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|        - | 4220 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|        - | 4221 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|        - | 4222 | ` * One-token lookahead only.` |
|        - | 4223 | ` */` |
|      708 | 4224 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|        2 | 4225 | `{` |
|      710 | 4226 | `	switch( pNext->iOp ){` |
|       17 | 4227 | `		case PH7_OP_STORE:` |
|       36 | 4228 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|      ! 0 | 4229 | `		case PH7_OP_STORE_REF:` |
|      ! 0 | 4230 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|       11 | 4231 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|        - | 4232 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|        - | 4233 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|        - | 4234 | `		case PH7_OP_CAT_STORE:` |
|        - | 4235 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|        - | 4236 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       23 | 4237 | `			return 1;` |
|      326 | 4238 | `		default:` |
|      654 | 4239 | `			return 0;` |
|        - | 4240 | `	}` |
|      356 | 4241 | `}` |
|        - | 4242 | `/*` |
|        - | 4243 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|        - | 4244 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|        - | 4245 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|        - | 4246 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|        - | 4247 | ` */` |
|      424 | 4248 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|        1 | 4249 | `{` |
|      425 | 4250 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|        1 | 4251 | `}` |
|        - | 4252 | `/*` |
|        - | 4253 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|        - | 4254 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|        - | 4255 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|        - | 4256 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|        - | 4257 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|        - | 4258 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|        - | 4259 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|        - | 4260 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|        - | 4261 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|        - | 4262 | ` * abort path; SXRET_OK otherwise.` |
|        - | 4263 | ` */` |
|       54 | 4264 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|        1 | 4265 | `{` |
|        - | 4266 | `	char zHName[384];` |
|        - | 4267 | `	sxu32 nHName;` |
|        - | 4268 | `	ph7_class_method *pSetHook;` |
|       55 | 4269 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|        - | 4270 | `		/* get-only hooked property: php's read-only Error */` |
|        - | 4271 | `		SyBlob sErrMsg;` |
|        5 | 4272 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 4273 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|        4 | 4274 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|        5 | 4275 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        5 | 4276 | `		return SXRET_OK;` |
|        - | 4277 | `	}` |
|       51 | 4278 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 4279 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|      ! 0 | 4280 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|      ! 0 | 4281 | `		if( rcVis != SXRET_OK ){` |
|      ! 0 | 4282 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|      ! 0 | 4283 | `			return SXRET_OK;` |
|        - | 4284 | `		}` |
|      ! 0 | 4285 | `	}` |
|       51 | 4286 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|       51 | 4287 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|       51 | 4288 | `	if( pSetHook ){` |
|        - | 4289 | `		ph7_value sHookRet;` |
|        - | 4290 | `		ph7_value *apHArg[1];` |
|       51 | 4291 | `		apHArg[0] = pValue;` |
|       51 | 4292 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|       51 | 4293 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|       51 | 4294 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|       51 | 4295 | `		VmMagicGuardPop(pVm);` |
|       50 | 4296 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|       27 | 4297 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|        3 | 4298 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|        3 | 4299 | `			if( rcH == SXRET_OK ){` |
|        3 | 4300 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|        3 | 4301 | `				if( pBack ){` |
|        3 | 4302 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|        2 | 4303 | `				}` |
|        1 | 4304 | `			}else if( rcH == PH7_ABORT ){` |
|      ! 0 | 4305 | `				PH7_MemObjRelease(&sHookRet);` |
|      ! 0 | 4306 | `				return PH7_ABORT;` |
|        - | 4307 | `			}` |
|        - | 4308 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|        - | 4309 | `			 * the store is skipped, execution lands at the fetch point like any` |
|        - | 4310 | `			 * parked throw. */` |
|        1 | 4311 | `		}` |
|       51 | 4312 | `		PH7_MemObjRelease(&sHookRet);` |
|       25 | 4313 | `	}` |
|       51 | 4314 | `	return SXRET_OK;` |
|       28 | 4315 | `}` |
|        - | 4316 | `/*` |
|        - | 4317 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|        - | 4318 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|        - | 4319 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|        - | 4320 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|        - | 4321 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|        - | 4322 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|        - | 4323 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|        - | 4324 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|        - | 4325 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|        - | 4326 | ` */` |
|        - | 4327 | `/*` |
|        - | 4328 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|        - | 4329 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|        - | 4330 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|        - | 4331 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|        - | 4332 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|        - | 4333 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|        - | 4334 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|        - | 4335 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|        - | 4336 | ` * caller reads the raw slot then.` |
|        - | 4337 | ` */` |
|      382 | 4338 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|        4 | 4339 | `{` |
|      386 | 4340 | `	ph7_vm *pVm = pThis->pVm;` |
|        - | 4341 | `	char zHName[384];` |
|        - | 4342 | `	sxu32 nHName;` |
|        - | 4343 | `	ph7_class_method *pGetHook;` |
|        - | 4344 | `	sxi32 rc;` |
|      382 | 4345 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|      260 | 4346 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|      142 | 4347 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|        - | 4348 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|        - | 4349 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|        - | 4350 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|        - | 4351 | `		 * raw values whose output the routed throw then discards. */` |
|      248 | 4352 | `		return SXERR_NOTFOUND;` |
|        - | 4353 | `	}` |
|      139 | 4354 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|      139 | 4355 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|      139 | 4356 | `	if( pGetHook == 0 ){` |
|      ! 0 | 4357 | `		return SXERR_NOTFOUND;` |
|        - | 4358 | `	}` |
|      139 | 4359 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|      139 | 4360 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|      139 | 4361 | `	VmMagicGuardPop(pVm);` |
|      139 | 4362 | `	return rc;` |
|      195 | 4363 | `}` |
|        - | 4364 | `/*` |
|        - | 4365 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|        - | 4366 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|        - | 4367 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|        - | 4368 | ` */` |
|       20 | 4369 | `static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4370 | `{` |
|       21 | 4371 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 4372 | `	VmSlot sFree;` |
|       21 | 4373 | `	if( pScr ){` |
|       21 | 4374 | `		PH7_MemObjRelease(pScr);` |
|       10 | 4375 | `	}` |
|       21 | 4376 | `	sFree.nIdx = nIdx;` |
|       21 | 4377 | `	sFree.pUserData = 0;` |
|       21 | 4378 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       21 | 4379 | `}` |
|        - | 4380 | `/*` |
|        - | 4381 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|        - | 4382 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|        - | 4383 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|        - | 4384 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|        - | 4385 | ` * instance reference.` |
|        - | 4386 | ` */` |
|       16 | 4387 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|        2 | 4388 | `{` |
|       18 | 4389 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       18 | 4390 | `	if( pEnt == 0 ){` |
|        5 | 4391 | `		return;` |
|        - | 4392 | `	}` |
|       13 | 4393 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        5 | 4394 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|        2 | 4395 | `	}` |
|       13 | 4396 | `	SyBlobRelease(&pEnt->sName);` |
|       13 | 4397 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|       13 | 4398 | `	(void)SySetPop(&pVm->aHookRmw);` |
|       10 | 4399 | `}` |
|       16 | 4400 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4401 | `{` |
|        - | 4402 | `	VmHookRmw sEnt;` |
|        - | 4403 | `	VmHookRmw *pEnt;` |
|        - | 4404 | `	ph7_value *pScr;` |
|        - | 4405 | `	ph7_value sVal;` |
|       17 | 4406 | `	sxi32 rc = SXRET_OK;` |
|       17 | 4407 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       17 | 4408 | `	if( pEnt == 0 \|\| pEnt->iKind != VM_HOOK_PEND_RMW \|\| pEnt->nScratchIdx != nIdx ){` |
|      ! 0 | 4409 | `		return SXERR_NOTFOUND;` |
|        - | 4410 | `	}` |
|       17 | 4411 | `	sEnt = *pEnt;` |
|       17 | 4412 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        - | 4413 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|        - | 4414 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|        - | 4415 | `	 * scratch index past this point). */` |
|       17 | 4416 | `	PH7_MemObjInit(pVm,&sVal);` |
|       17 | 4417 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|       17 | 4418 | `	if( pScr ){` |
|       17 | 4419 | `		PH7_MemObjStore(pScr,&sVal);` |
|        8 | 4420 | `	}` |
|       17 | 4421 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|       17 | 4422 | `	sVal.nIdx = SXU32_HIGH;` |
|       17 | 4423 | `	if( pVm->nBoundaryRc == 0 ){` |
|       15 | 4424 | `		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|        7 | 4425 | `	}` |
|       17 | 4426 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 4427 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|       17 | 4428 | `	return rc;` |
|        9 | 4429 | `}` |
|        - | 4430 | `/*` |
|        - | 4431 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|        - | 4432 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|        - | 4433 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|        - | 4434 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|        - | 4435 | ` */` |
|       12 | 4436 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|        1 | 4437 | `{` |
|       13 | 4438 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       13 | 4439 | `	if( pSetMeth ){` |
|        - | 4440 | `		ph7_value sNameVal;` |
|        - | 4441 | `		ph7_value *apSetArg[2];` |
|       13 | 4442 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       13 | 4443 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       13 | 4444 | `		apSetArg[0] = &sNameVal;` |
|       13 | 4445 | `		apSetArg[1] = pValue;` |
|       13 | 4446 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       13 | 4447 | `		PH7_VmCallClassMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       13 | 4448 | `		VmMagicGuardPop(pVm);` |
|       13 | 4449 | `		PH7_MemObjRelease(&sNameVal);` |
|        6 | 4450 | `	}` |
|       13 | 4451 | `}` |
|        - | 4452 | `/*` |
|        - | 4453 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|        - | 4454 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|        - | 4455 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|        - | 4456 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|        - | 4457 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|        - | 4458 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|        - | 4459 | ` * path (the SyHash-layout incident class).` |
|        - | 4460 | ` */` |
|        - | 4461 | `/*` |
|        - | 4462 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|        - | 4463 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|        - | 4464 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|        - | 4465 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|        - | 4466 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|        - | 4467 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|        - | 4468 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|        - | 4469 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|        - | 4470 | ` * never linked (INIT error path).` |
|        - | 4471 | ` */` |
|    23922 | 4472 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|        5 | 4473 | `{` |
|    23927 | 4474 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|    23927 | 4475 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|        - | 4476 | `	sxu32 i;` |
|    26099 | 4477 | `	for( i = 0 ; i < n ; ++i ){` |
|    26099 | 4478 | `		if( apStep[i] == pStep ){` |
|    23935 | 4479 | `			for( ; i + 1 < n ; ++i ){` |
|        9 | 4480 | `				apStep[i] = apStep[i + 1];` |
|        5 | 4481 | `			}` |
|    23927 | 4482 | `			(void)SySetPop(&pInfo->aStep);` |
|    23927 | 4483 | `			return;` |
|        - | 4484 | `		}` |
|     1091 | 4485 | `	}` |
|    11966 | 4486 | `}` |
|      210 | 4487 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|        5 | 4488 | `{` |
|      215 | 4489 | `	if( pStep->pOwner ){` |
|       24 | 4490 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       11 | 4491 | `	}` |
|      215 | 4492 | `	VmForeachStepUnlink(pInfo,pStep);` |
|      215 | 4493 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      215 | 4494 | `	PH7_ClassInstanceUnref(pThis);` |
|      215 | 4495 | `}` |
|        - | 4496 | `/*` |
|        - | 4497 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|        - | 4498 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|        - | 4499 | ` * step stack, then drop the step's map reference. The single home for this` |
|        - | 4500 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|        - | 4501 | ` * load-bearing: a step freed while still registered is walked by the next` |
|        - | 4502 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|        - | 4503 | ` * class), and the unregister must precede the unref in case the step held the` |
|        - | 4504 | ` * map's last reference.` |
|        - | 4505 | ` */` |
|    23686 | 4506 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|        5 | 4507 | `{` |
|    23691 | 4508 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|    23691 | 4509 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|    23691 | 4510 | `	if( bPop ){` |
|        - | 4511 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|        - | 4512 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|    23691 | 4513 | `		VmForeachStepUnlink(pInfo,pStep);` |
|    11843 | 4514 | `	}` |
|    23691 | 4515 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    23691 | 4516 | `	PH7_HashmapUnref(pMap);` |
|    23691 | 4517 | `}` |
|        - | 4518 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|        - | 4519 | `/*` |
|        - | 4520 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 4521 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4522 | ` * See block-comment on that function for additional information.` |
|        - | 4523 | ` */` |
|   142168 | 4524 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 4525 | `{` |
|        - | 4526 | `	ph7_value *pStack;` |
|        - | 4527 | `	sxu32 nCap;` |
|        - | 4528 | `	sxi32 rc;` |
|        - | 4529 | `	/* Allocate a new operand stack */` |
|   142173 | 4530 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   142173 | 4531 | `	if( pStack == 0 ){` |
|      ! 0 | 4532 | `		return SXERR_MEM;` |
|        - | 4533 | `	}` |
|   142173 | 4534 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|        - | 4535 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|        - | 4536 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   142173 | 4537 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|        - | 4538 | `	/* Free the operand stack */` |
|   142173 | 4539 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 4540 | `	/* Execution result */` |
|   142173 | 4541 | `	return rc;` |
|    71089 | 4542 | `}` |
|        - | 4543 | `/*` |
|        - | 4544 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|        - | 4545 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|        - | 4546 | ` * the argument resolve against that class (like php) rather than the reflection` |
|        - | 4547 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|        - | 4548 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|        - | 4549 | ` */` |
|       54 | 4550 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|        1 | 4551 | `{` |
|       55 | 4552 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       55 | 4553 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - | 4554 | `	sxi32 rc;` |
|       55 | 4555 | `	if( pDeclCls ){` |
|       39 | 4556 | `		pVm->pConstEvalClass = pDeclCls;` |
|       39 | 4557 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       19 | 4558 | `	}` |
|       55 | 4559 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       55 | 4560 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       55 | 4561 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       55 | 4562 | `	return rc;` |
|        1 | 4563 | `}` |
|        - | 4564 | `/*` |
|        - | 4565 | ` * Invoke any installed shutdown callbacks.` |
|        - | 4566 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|        - | 4567 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|        - | 4568 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|        - | 4569 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|        - | 4570 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|        - | 4571 | ` *` |
|        - | 4572 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|        - | 4573 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|        - | 4574 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|        - | 4575 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|        - | 4576 | ` * down and restore the default consumer.` |
|        - | 4577 | ` */` |
|     3408 | 4578 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|        5 | 4579 | `{` |
|     3413 | 4580 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - | 4581 | `	sxu32 n,nUsed;` |
|     3413 | 4582 | `	nUsed = SySetUsed(&pVm->aOB);` |
|     3413 | 4583 | `	if( nUsed < 1 ){` |
|     3411 | 4584 | `		return;` |
|        - | 4585 | `	}` |
|        7 | 4586 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4587 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4588 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|        5 | 4589 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|        5 | 4590 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|        2 | 4591 | `		}` |
|        3 | 4592 | `	}` |
|        - | 4593 | `	/* Restore the default consumer and release the buffers. */` |
|        3 | 4594 | `	pCons->xConsumer = pCons->xDef;` |
|        3 | 4595 | `	pCons->pUserData = pCons->pDefData;` |
|        7 | 4596 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4597 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4598 | `		if( pOb ){` |
|        5 | 4599 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|        5 | 4600 | `			SyBlobRelease(&pOb->sOB);` |
|        2 | 4601 | `		}` |
|        3 | 4602 | `	}` |
|        3 | 4603 | `	SySetReset(&pVm->aOB);` |
|        3 | 4604 | `	pVm->nObDepth = 0;` |
|     1709 | 4605 | `}` |
|        - | 4606 | `/*` |
|        - | 4607 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 4608 | ` * or more calls to [register_shutdown_function()].` |
|        - | 4609 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 4610 | ` * execution ends.` |
|        - | 4611 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 4612 | ` * additional information.` |
|        - | 4613 | ` */` |
|     3408 | 4614 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 4615 | `{` |
|        - | 4616 | `	VmShutdownCB *pEntry;` |
|        - | 4617 | `	ph7_value *apArg[10];` |
|        - | 4618 | `	sxu32 n,nEntry;` |
|        - | 4619 | `	int i;` |
|        - | 4620 | `	/* Point to the stack of registered callbacks */` |
|     3413 | 4621 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    37493 | 4622 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    34085 | 4623 | `		apArg[i] = 0;` |
|    17045 | 4624 | `	}` |
|        - | 4625 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 4626 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 4627 | `	 * callbacks, mirroring PHP.` |
|        - | 4628 | `	 */` |
|     3413 | 4629 | `	pVm->bHaltRequested = 0;` |
|     3431 | 4630 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       23 | 4631 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4632 | `		if( pEntry ){` |
|        - | 4633 | `			/* Prepare callback arguments if any */` |
|       23 | 4634 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 4635 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 4636 | `					break;` |
|        - | 4637 | `				}` |
|      ! 0 | 4638 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 4639 | `			}` |
|        - | 4640 | `			/* Invoke the callback */` |
|       23 | 4641 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 4642 | `			/*` |
|        - | 4643 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 4644 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 4645 | `			 */` |
|       23 | 4646 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4647 | `			if( pEntry ){` |
|       23 | 4648 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       23 | 4649 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 4650 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 4651 | `				}` |
|        9 | 4652 | `			}` |
|       23 | 4653 | `			if( pVm->bHaltRequested ){` |
|        - | 4654 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 4655 | `				break;` |
|        - | 4656 | `			}` |
|        9 | 4657 | `		}` |
|       14 | 4658 | `	}` |
|     3413 | 4659 | `	SySetReset(&pVm->aShutdown);` |
|     3413 | 4660 | `}` |
|        - | 4661 | `/*` |
|        - | 4662 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 4663 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4664 | ` * See block-comment on that function for additional information.` |
|        - | 4665 | ` */` |
|     3408 | 4666 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 4667 | `{` |
|        - | 4668 | `	/* Make sure we are ready to execute this program */` |
|     3413 | 4669 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 4670 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 4671 | `	}` |
|        - | 4672 | `	/* Set the execution magic number  */` |
|     3413 | 4673 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 4674 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|        - | 4675 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|        - | 4676 | `	{` |
|     3413 | 4677 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|     3413 | 4678 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|        - | 4679 | `	}` |
|        - | 4680 | `	/* Invoke any shutdown callbacks */` |
|     3413 | 4681 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 4682 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|        - | 4683 | `	 * shutdown callbacks, which may still write into them. */` |
|     3413 | 4684 | `	VmFlushOutputBuffers(&(*pVm));` |
|        - | 4685 | `	/*` |
|        - | 4686 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 4687 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 4688 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 4689 | `	 */` |
|     3413 | 4690 | `	return SXRET_OK;` |
|     1709 | 4691 | `}` |
|        - | 4692 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 4693 | `/*` |
|        - | 4694 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 4695 | ` * the desired message.` |
|        - | 4696 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 4697 | ` * in 'api.c' for additional information.` |
|        - | 4698 | ` */` |
|     2414 | 4699 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 4700 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 4701 | `	SyString *pString /* Message to output */` |
|        - | 4702 | `	)` |
|        5 | 4703 | `{` |
|     2419 | 4704 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     2419 | 4705 | `	sxi32 rc = SXRET_OK;` |
|        - | 4706 | `	/* Call the output consumer */` |
|     2419 | 4707 | `	if( pString->nByte > 0 ){` |
|     2419 | 4708 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     2419 | 4709 | `		VmTrackOutput(pVm, pString->nByte);` |
|     1207 | 4710 | `	}` |
|     2419 | 4711 | `	return rc;` |
|        5 | 4712 | `}` |
|        - | 4713 | `/*` |
|        - | 4714 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 4715 | ` * callback to consume the formatted message.` |
|        - | 4716 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 4717 | ` * in 'api.c' for additional information.` |
|        - | 4718 | ` */` |
|        2 | 4719 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 4720 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 4721 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 4722 | `	va_list ap           /* Variable list of arguments */` |
|        - | 4723 | `	)` |
|        1 | 4724 | `{` |
|        3 | 4725 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 4726 | `	sxi32 rc = SXRET_OK;` |
|        - | 4727 | `	SyBlob sWorker;` |
|        - | 4728 | `	/* Format the message and call the output consumer */` |
|        3 | 4729 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 4730 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 4731 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 4732 | `		/* Consume the formatted message */` |
|        3 | 4733 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 4734 | `	}` |
|        3 | 4735 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 4736 | `	/* Release the working buffer */` |
|        3 | 4737 | `	SyBlobRelease(&sWorker);` |
|        3 | 4738 | `	return rc;` |
|        1 | 4739 | `}` |
|        - | 4740 | `/*` |
|        - | 4741 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 4742 | ` * This function never fail and always return a pointer` |
|        - | 4743 | ` * to a null terminated string.` |
|        - | 4744 | ` */` |
|       12 | 4745 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 4746 | `{` |
|       13 | 4747 | `	const char *zOp = "Unknown     ";` |
|       13 | 4748 | `	switch(nOp){` |
|        3 | 4749 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 4750 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 4751 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 4752 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 4753 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 4754 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 4755 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 4756 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 4757 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 4758 | `	case PH7_OP_LOAD_FCC:` |
|      ! 0 | 4759 | `		                    zOp = "LOAD_FCC   "; break;` |
|      ! 0 | 4760 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 4761 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 4762 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 4763 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 4764 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 4765 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 4766 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 4767 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 4768 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 4769 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 4770 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 4771 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 4772 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 4773 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 4774 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 4775 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 4776 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 4777 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 4778 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 4779 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 4780 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 4781 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 4782 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 4783 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 4784 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 4785 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 4786 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 4787 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 4788 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 4789 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 4790 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 4791 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 4792 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 4793 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 4794 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 4795 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 4796 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 4797 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 4798 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 4799 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 4800 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 4801 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 4802 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 4803 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 4804 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 4805 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 4806 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|      ! 0 | 4807 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 4808 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 4809 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 4810 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 4811 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 4812 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 4813 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 4814 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 4815 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 4816 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 4817 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 4818 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 4819 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 4820 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 4821 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 4822 | `	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;` |
|      ! 0 | 4823 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 4824 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 4825 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 4826 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 4827 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 4828 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 4829 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 4830 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 4831 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 4832 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 4833 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 4834 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 4835 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 4836 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 4837 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 4838 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 4839 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 4840 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|      ! 0 | 4841 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 4842 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 4843 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 4844 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 4845 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 4846 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 4847 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 4848 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 4849 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 4850 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 4851 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 4852 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 4853 | `	default:` |
|      ! 0 | 4854 | `		break;` |
|        - | 4855 | `	}` |
|       13 | 4856 | `	return zOp;` |
|        1 | 4857 | `}` |
|        - | 4858 | `/*` |
|        - | 4859 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 4860 | ` * The xConsumer() callback which is an used defined function` |
|        - | 4861 | ` * is responsible of consuming the generated dump.` |
|        - | 4862 | ` */` |
|        2 | 4863 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 4864 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 4865 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 4866 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 4867 | `	)` |
|        1 | 4868 | `{` |
|        - | 4869 | `	sxi32 rc;` |
|        3 | 4870 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 4871 | `	return rc;` |
|        1 | 4872 | `}` |
|        - | 4873 | `/*` |
|        - | 4874 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 4875 | ` * outside a class body [i.e: global or function scope].` |
|        - | 4876 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 4877 | ` * in 'compile.c' for additional information.` |
|        - | 4878 | ` */` |
|       64 | 4879 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        3 | 4880 | `{` |
|       67 | 4881 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 4882 | `	/* Evaluate and expand constant value */` |
|       67 | 4883 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       67 | 4884 | `}` |
|        - | 4885 | `/*` |
|        - | 4886 | ` * Section:` |
|        - | 4887 | ` *  Function handling functions.` |
|        - | 4888 | ` * Status:` |
|        - | 4889 | ` *    Stable.` |
|        - | 4890 | ` */` |
|        - | 4891 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 4892 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 4893 | `	{ "__phl_magic_call", vm_builtin_magic_call },` |
|        - | 4894 | `	{ "__phl_enum_cases",   vm_builtin_enum_cases },` |
|        - | 4895 | `	{ "__phl_enum_from",    vm_builtin_enum_from },` |
|        - | 4896 | `	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },` |
|        - | 4897 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|        - | 4898 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 4899 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 4900 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 4901 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 4902 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 4903 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 4904 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 4905 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 4906 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 4907 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 4908 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 4909 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 4910 | `	    /* Constants management */` |
|        - | 4911 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 4912 | `	{ "define",   vm_builtin_define               },` |
|        - | 4913 | `	{ "constant", vm_builtin_constant             },` |
|        - | 4914 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 4915 | `	   /* Class/Object functions */` |
|        - | 4916 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 4917 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 4918 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 4919 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 4920 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 4921 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|        - | 4922 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 4923 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 4924 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 4925 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 4926 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 4927 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 4928 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 4929 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 4930 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 4931 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 4932 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 4933 | `	   /* SPL object identity */` |
|        - | 4934 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|        - | 4935 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|        - | 4936 | `	   /* SPL Autoloading */` |
|        - | 4937 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 4938 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 4939 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 4940 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 4941 | `	   /* Random numbers/strings generators */` |
|        - | 4942 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 4943 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 4944 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 4945 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 4946 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 4947 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 4948 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 4949 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 4950 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 4951 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 4952 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 4953 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 4954 | `	   /* Language constructs functions */` |
|        - | 4955 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 4956 | `	{ "print", vm_builtin_print                   },` |
|        - | 4957 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 4958 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 4959 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 4960 | `	  /* Variable handling functions */` |
|        - | 4961 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 4962 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 4963 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 4964 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|        - | 4965 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 4966 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 4967 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 4968 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 4969 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 4970 | `	  /* Ouput control functions */` |
|        - | 4971 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 4972 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 4973 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 4974 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 4975 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 4976 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 4977 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 4978 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 4979 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 4980 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 4981 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 4982 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 4983 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 4984 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 4985 | `	  /* Assertion functions */` |
|        - | 4986 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 4987 | `	  /* Error reporting functions */` |
|        - | 4988 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 4989 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 4990 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 4991 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 4992 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 4993 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 4994 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 4995 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 4996 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|        - | 4997 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|        - | 4998 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 4999 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|        - | 5000 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|        - | 5001 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 5002 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 5003 | `	  /* Release info */` |
|        - | 5004 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 5005 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 5006 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 5007 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 5008 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 5009 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 5010 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 5011 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 5012 | `	  /* hashmap */` |
|        - | 5013 | `	{"compact",          vm_builtin_compact       },` |
|        - | 5014 | `	{"extract",          vm_builtin_extract       },` |
|        - | 5015 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 5016 | `	  /* URL related function */` |
|        - | 5017 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 5018 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 5019 | `	   /* UTF-8 encoding/decoding */` |
|        - | 5020 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 5021 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 5022 | `	   /* Command line processing */` |
|        - | 5023 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 5024 | `	   /* JSON encoding/decoding */` |
|        - | 5025 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 5026 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 5027 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 5028 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 5029 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 5030 | `	{"serialize",      vm_builtin_serialize },` |
|        - | 5031 | `	{"unserialize",    vm_builtin_unserialize },` |
|        - | 5032 | `	   /* Files/URI inclusion facility */` |
|        - | 5033 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 5034 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|        - | 5035 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 5036 | `	{ "include",      vm_builtin_include          },` |
|        - | 5037 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 5038 | `	{ "require",      vm_builtin_require          },` |
|        - | 5039 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 5040 | `};` |
|        - | 5041 | `/*` |
|        - | 5042 | ` * Register the built-in VM functions defined above.` |
|        - | 5043 | ` */` |
|     3404 | 5044 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 5045 | `{` |
|        - | 5046 | `	sxi32 rc;` |
|        - | 5047 | `	sxu32 n;` |
|   428909 | 5048 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 5049 | `		/* Note that these special functions have access` |
|        - | 5050 | `		 * to the underlying virtual machine as their` |
|        - | 5051 | `		 * private data.` |
|        - | 5052 | `		 */` |
|   425505 | 5053 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   425505 | 5054 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 5055 | `			return rc;` |
|        - | 5056 | `		}` |
|   212755 | 5057 | `	}` |
|     3409 | 5058 | `	return SXRET_OK;` |
|     1707 | 5059 | `}` |
|        - | 5060 | `/*` |
|        - | 5061 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 5062 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 5063 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 5064 | ` */` |
|   786180 | 5065 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 5066 | `{` |
|   786185 | 5067 | `	if( !iLoadable ){` |
|   779431 | 5068 | `		return pClass;` |
|        - | 5069 | `	}` |
|     6763 | 5070 | `	while(pClass){` |
|     6759 | 5071 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     6755 | 5072 | `			return pClass;` |
|        - | 5073 | `		}` |
|        5 | 5074 | `		pClass = pClass->pNextName;` |
|        1 | 5075 | `	}` |
|        5 | 5076 | `	return 0;` |
|   393095 | 5077 | `}` |
|        - | 5078 | `/*` |
|        - | 5079 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 5080 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 5081 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 5082 | ` * registered in the VM's class table.` |
|        - | 5083 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 5084 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 5085 | ` */` |
|      318 | 5086 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5087 | `{` |
|        - | 5088 | `	VmAutoloadCB *pEntry;` |
|        - | 5089 | `	ph7_value sArg,sResult;` |
|        - | 5090 | `	SyHashEntry *pHashEntry;` |
|        - | 5091 | `	ph7_class *pClass;` |
|        - | 5092 | `	sxu32 n,nEntry;` |
|      323 | 5093 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|      323 | 5094 | `	if( nEntry < 1 ){` |
|      251 | 5095 | `		return 0;` |
|        - | 5096 | `	}` |
|        - | 5097 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       77 | 5098 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 5099 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 5100 | `	}` |
|        - | 5101 | `	/* Mark this class as being autoloaded */` |
|       75 | 5102 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 5103 | `	/* Prepare the class name argument */` |
|       75 | 5104 | `	PH7_MemObjInit(pVm,&sArg);` |
|       75 | 5105 | `	PH7_MemObjInit(pVm,&sResult);` |
|       75 | 5106 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       75 | 5107 | `	pClass = 0;` |
|      139 | 5108 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 5109 | `		ph7_value *apArg[1];` |
|       85 | 5110 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       85 | 5111 | `		if( pEntry == 0 ){` |
|      ! 0 | 5112 | `			continue;` |
|        - | 5113 | `		}` |
|       85 | 5114 | `		apArg[0] = &sArg;` |
|       85 | 5115 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 5116 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 5117 | `			continue;` |
|        - | 5118 | `		}` |
|        - | 5119 | `		/* Check if the class is now available */` |
|       85 | 5120 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       85 | 5121 | `		if( pHashEntry ){` |
|       21 | 5122 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       21 | 5123 | `			if( pClass ){` |
|       21 | 5124 | `				break;` |
|        - | 5125 | `			}` |
|      ! 0 | 5126 | `		}` |
|       35 | 5127 | `	}` |
|       75 | 5128 | `	PH7_MemObjRelease(&sArg);` |
|       75 | 5129 | `	PH7_MemObjRelease(&sResult);` |
|        - | 5130 | `	/* Remove reentrancy guard */` |
|       75 | 5131 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       75 | 5132 | `	return pClass;` |
|      164 | 5133 | `}` |
|        - | 5134 | `/*` |
|        - | 5135 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 5136 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 5137 | ` */` |
|       38 | 5138 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5139 | `{` |
|       43 | 5140 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 | 5141 | `}` |
|        - | 5142 | `/*` |
|        - | 5143 | ` * Check if the given name refer to an installed class.` |
|        - | 5144 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 5145 | ` */` |
|   786444 | 5146 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 5147 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 5148 | `	const char *zName,  /* Name of the target class */` |
|        - | 5149 | `	sxu32 nByte,        /* zName length */` |
|        - | 5150 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 5151 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 5152 | `						 */` |
|        - | 5153 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 5154 | `	)` |
|        5 | 5155 | `{` |
|        - | 5156 | `	SyHashEntry *pEntry;` |
|        - | 5157 | `	ph7_class *pClass;` |
|   393222 | 5158 | `	SXUNUSED(iNest);` |
|        - | 5159 | `	/* Exact class lookup.` |
|        - | 5160 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 5161 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   786449 | 5162 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   786449 | 5163 | `	if( pEntry == 0 ){` |
|        - | 5164 | `		/* Class not found in hash table — try autoload before giving up */` |
|      285 | 5165 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 5166 | `	}` |
|   786169 | 5167 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   786169 | 5168 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   393227 | 5169 | `}` |
|        - | 5170 | `/*` |
|        - | 5171 | ` * Reference Table Implementation` |
|        - | 5172 | ` * Status: stable <chm@symisc.net>` |
|        - | 5173 | ` * Intro` |
|        - | 5174 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 5175 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 5176 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 5177 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 5178 | ` *  Refer to the official for more information on this powerful` |
|        - | 5179 | ` *  extension.` |
|        - | 5180 | ` */` |
|        - | 5181 | `/*` |
|        - | 5182 | ` * Allocate a new reference entry.` |
|        - | 5183 | ` */` |
|  4385749 | 5184 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 5185 | `{` |
|        - | 5186 | `	VmRefObj *pRef;` |
|        - | 5187 | `	/* Allocate a new instance */` |
|  4385754 | 5188 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  4385754 | 5189 | `	if( pRef == 0 ){` |
|      ! 0 | 5190 | `		return 0;` |
|        - | 5191 | `	}` |
|        - | 5192 | `	/* Zero the structure */` |
|  4385754 | 5193 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 5194 | `	/* Initialize fields */` |
|  4385754 | 5195 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  4385754 | 5196 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  4385754 | 5197 | `	pRef->nIdx = nIdx;` |
|  4385754 | 5198 | `	return pRef;` |
|  2193500 | 5199 | `}` |
|        - | 5200 | `/*` |
|        - | 5201 | ` * Default hash function used by the reference table` |
|        - | 5202 | ` * for lookup/insertion operations.` |
|        - | 5203 | ` */` |
| 23553368 | 5204 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 5205 | `{` |
|        - | 5206 | `	/* Calculate the hash based on the memory object index */` |
| 23553373 | 5207 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 5208 | `}` |
|        - | 5209 | `/*` |
|        - | 5210 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 5211 | ` * in the reference table.` |
|        - | 5212 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 5213 | ` * otherwise.` |
|        - | 5214 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5215 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5216 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5217 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5218 | ` * Refer to the official for more information on this powerful` |
|        - | 5219 | ` * extension.` |
|        - | 5220 | ` */` |
| 13622651 | 5221 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 5222 | `{` |
|        - | 5223 | `	VmRefObj *pRef;` |
|        - | 5224 | `	sxu32 nBucket;` |
|        - | 5225 | `	/* Point to the appropriate bucket */` |
| 13622656 | 5226 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 5227 | `	/* Perform the lookup */` |
| 13622656 | 5228 | `	pRef = pVm->apRefObj[nBucket];` |
| 30050138 | 5229 | `	for(;;){` |
| 60070845 | 5230 | `		if( pRef == 0 ){` |
|  4892054 | 5231 | `			break;` |
|        - | 5232 | `		}` |
| 55178796 | 5233 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 5234 | `			/* Entry found */` |
|  8730607 | 5235 | `			return pRef;` |
|        - | 5236 | `		}` |
|        - | 5237 | `		/* Point to the next entry */` |
| 46448194 | 5238 | `		pRef = pRef->pNextCollide;` |
|        5 | 5239 | `	}` |
|        - | 5240 | `	/* No such entry,return NULL */` |
|  4892054 | 5241 | `	return 0;` |
|  6813192 | 5242 | `}` |
|        - | 5243 | `/*` |
|        - | 5244 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5245 | ` *` |
|        - | 5246 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5247 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5248 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5249 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5250 | ` * Refer to the official for more information on this powerful` |
|        - | 5251 | ` * extension.` |
|        - | 5252 | ` */` |
|  4385749 | 5253 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5254 | `{` |
|        - | 5255 | `	sxu32 nBucket;` |
|  4385754 | 5256 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 5257 | `		VmRefObj **apNew;` |
|        - | 5258 | `		sxu32 nNew;` |
|        - | 5259 | `		/* Allocate a larger table */` |
|    10549 | 5260 | `		nNew = pVm->nRefSize << 1;` |
|    10549 | 5261 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|    10549 | 5262 | `		if( apNew ){` |
|    10549 | 5263 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 5264 | `			sxu32 n;` |
|        - | 5265 | `			/* Zero the structure */` |
|    10549 | 5266 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 5267 | `			/* Rehash all referenced entries */` |
|  2966749 | 5268 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 5269 | `				/* Remove old collision links */` |
|  2956205 | 5270 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 5271 | `				/* Point to the appropriate bucket */` |
|  2956205 | 5272 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 5273 | `				/* Insert the entry  */` |
|  2956205 | 5274 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2956205 | 5275 | `				if( apNew[nBucket] ){` |
|  2306069 | 5276 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1153032 | 5277 | `				}` |
|  2956205 | 5278 | `				apNew[nBucket] = pEntry;` |
|        - | 5279 | `				/* Point to the next entry */` |
|  2956205 | 5280 | `				pEntry = pEntry->pNext;` |
|  1478105 | 5281 | `			}` |
|        - | 5282 | `			/* Release the old table */` |
|    10549 | 5283 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 5284 | `			/* Install the new one */` |
|    10549 | 5285 | `			pVm->apRefObj = apNew;` |
|    10549 | 5286 | `			pVm->nRefSize = nNew;` |
|     5272 | 5287 | `		}` |
|     5272 | 5288 | `	}` |
|        - | 5289 | `	/* Point to the appropriate bucket */` |
|  4385754 | 5290 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 5291 | `	/* Insert the entry */` |
|  4385754 | 5292 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  4385754 | 5293 | `	if( pVm->apRefObj[nBucket] ){` |
|  3378580 | 5294 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1689840 | 5295 | `	}` |
|  4385754 | 5296 | `	pVm->apRefObj[nBucket] = pRef;` |
|  4385754 | 5297 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  4385754 | 5298 | `	pVm->nRefUsed++;` |
|  4385754 | 5299 | `	return SXRET_OK;` |
|        5 | 5300 | `}` |
|        - | 5301 | `/*` |
|        - | 5302 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 5303 | ` * the reference table.` |
|        - | 5304 | ` * This function is invoked when the user perform an unset` |
|        - | 5305 | ` * call [i.e: unset($var); ].` |
|        - | 5306 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5307 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5308 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5309 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5310 | ` * Refer to the official for more information on this powerful` |
|        - | 5311 | ` * extension.` |
|        - | 5312 | ` */` |
|  4252043 | 5313 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5314 | `{` |
|        - | 5315 | `	ph7_hashmap_node **apNode;` |
|        - | 5316 | `	SyHashEntry **apEntry;` |
|        - | 5317 | `	sxu32 n;` |
|        - | 5318 | `	/* Point to the reference table */` |
|  4252048 | 5319 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  4252048 | 5320 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 5321 | `	/* Unlink the entry from the reference table */` |
|  4764418 | 5322 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   512375 | 5323 | `		if( apEntry[n] ){` |
|   506539 | 5324 | `			SyHashDeleteEntry2(apEntry[n]);` |
|   253888 | 5325 | `		}` |
|   256811 | 5326 | `	}` |
|  7975735 | 5327 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3723692 | 5328 | `		if( apNode[n] ){` |
|     1390 | 5329 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|      693 | 5330 | `		}` |
|  1861848 | 5331 | `	}` |
|  4252048 | 5332 | `	if( pRef->pPrevCollide ){` |
|  1663280 | 5333 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   831608 | 5334 | `	}else{` |
|  2588773 | 5335 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 5336 | `	}` |
|  4252048 | 5337 | `	if( pRef->pNextCollide ){` |
|  2265647 | 5338 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|  1133374 | 5339 | `	}` |
|  4252048 | 5340 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 5341 | `	/* Release the node */` |
|  4252048 | 5342 | `	SySetRelease(&pRef->aReference);` |
|  4252048 | 5343 | `	SySetRelease(&pRef->aArrEntries);` |
|  4252048 | 5344 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  4252048 | 5345 | `	pVm->nRefUsed--;` |
|  4252048 | 5346 | `	return SXRET_OK;` |
|        5 | 5347 | `}` |
|        - | 5348 | `/*` |
|        - | 5349 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5350 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5351 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5352 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5353 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5354 | ` * Refer to the official for more information on this powerful` |
|        - | 5355 | ` * extension.` |
|        - | 5356 | ` */` |
|  4432257 | 5357 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 5358 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5359 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5360 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5361 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 5362 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 5363 | `	)` |
|        5 | 5364 | `{` |
|  4432262 | 5365 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 5366 | `	VmRefObj *pRef;` |
|        - | 5367 | `	/* Check if the referenced object already exists */` |
|  4432262 | 5368 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4432262 | 5369 | `	if( pRef == 0 ){` |
|        - | 5370 | `		/* Create a new entry */` |
|  4385754 | 5371 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  4385754 | 5372 | `		if( pRef == 0 ){` |
|      ! 0 | 5373 | `			return SXERR_MEM;` |
|        - | 5374 | `		}` |
|  4385754 | 5375 | `		pRef->iFlags = iFlags;` |
|        - | 5376 | `		/* Install the entry */` |
|  4385754 | 5377 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  2193495 | 5378 | `	}` |
|  4432262 | 5379 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  4432262 | 5380 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 5381 | `		VmSlot sRef;` |
|        - | 5382 | `		/* Local frame,record referenced entry so that it can` |
|        - | 5383 | `		 * be deleted when we leave this frame.` |
|        - | 5384 | `		 */` |
|   506503 | 5385 | `		sRef.nIdx = nIdx;` |
|   506503 | 5386 | `		sRef.pUserData = pEntry;` |
|   506503 | 5387 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 5388 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 5389 | `		}` |
|   253870 | 5390 | `	}` |
|  4432262 | 5391 | `	if( pEntry ){` |
|        - | 5392 | `		/* Address of the hash-entry */` |
|   552753 | 5393 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|   276995 | 5394 | `	}` |
|  4432262 | 5395 | `	if( pMapEntry ){` |
|        - | 5396 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3825540 | 5397 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1912767 | 5398 | `	}` |
|  4432262 | 5399 | `	return SXRET_OK;` |
|  2216754 | 5400 | `}` |
|        - | 5401 | `/*` |
|        - | 5402 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 5403 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5404 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5405 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5406 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5407 | ` * Refer to the official for more information on this powerful` |
|        - | 5408 | ` * extension.` |
|        - | 5409 | ` */` |
|  4222933 | 5410 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 5411 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5412 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5413 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5414 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 5415 | `	)` |
|        5 | 5416 | `{` |
|        - | 5417 | `	VmRefObj *pRef;` |
|        - | 5418 | `	sxu32 n;` |
|        - | 5419 | `	/* Check if the referenced object already exists */` |
|  4222938 | 5420 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4222938 | 5421 | `	if( pRef == 0 ){` |
|        - | 5422 | `		/* Not such entry */` |
|   506305 | 5423 | `		return SXERR_NOTFOUND;` |
|        - | 5424 | `	}` |
|        - | 5425 | `	/* Remove the desired entry */` |
|  3716638 | 5426 | `	if( pEntry ){` |
|        - | 5427 | `		SyHashEntry **apEntry;` |
|       87 | 5428 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 5429 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      185 | 5430 | `			if( apEntry[n] == pEntry ){` |
|        - | 5431 | `				/* Nullify the entry */` |
|       85 | 5432 | `				apEntry[n] = 0;` |
|        - | 5433 | `				/*` |
|        - | 5434 | `				 * NOTE:` |
|        - | 5435 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 5436 | `				 * we avoid wasting spaces.` |
|        - | 5437 | `				 */` |
|       40 | 5438 | `			}` |
|       95 | 5439 | `		}` |
|       41 | 5440 | `	}` |
|  3716638 | 5441 | `	if( pMapEntry ){` |
|        - | 5442 | `		ph7_hashmap_node **apNode;` |
|  3716556 | 5443 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  7433371 | 5444 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3716820 | 5445 | `			if( apNode[n] == pMapEntry ){` |
|        - | 5446 | `				/* nullify the entry */` |
|  3716556 | 5447 | `				apNode[n] = 0;` |
|  1858275 | 5448 | `			}` |
|  1858412 | 5449 | `		}` |
|  1858275 | 5450 | `	}` |
|  3716638 | 5451 | `	return SXRET_OK;` |
|  2112092 | 5452 | `}` |
|        - | 5453 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 5454 | `/*` |
|        - | 5455 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 5456 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 5457 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 5458 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 5459 | ` * For more information on how to register IO stream devices,please` |
|        - | 5460 | ` * refer to the official documentation.` |
|        - | 5461 | ` */` |
|    30032 | 5462 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 5463 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 5464 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 5465 | `	int nByte              /* *pzDevice length*/` |
|        - | 5466 | `	)` |
|        5 | 5467 | `{` |
|        - | 5468 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 5469 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 5470 | `	SyString sDev,sCur;` |
|        - | 5471 | `	sxu32 n,nEntry;` |
|        - | 5472 | `	int rc;` |
|        - | 5473 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    30037 | 5474 | `	zNext = zCur = zIn = *pzDevice;` |
|    30037 | 5475 | `	zEnd = &zIn[nByte];` |
|  1904051 | 5476 | `	while( zIn < zEnd ){` |
|  1874051 | 5477 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 5478 | `			/* Got one */` |
|       35 | 5479 | `			zNext = &zIn[sizeof("://")-1];` |
|       35 | 5480 | `			break;` |
|        - | 5481 | `		}` |
|        - | 5482 | `		/* Advance the cursor */` |
|  1874019 | 5483 | `		zIn++;` |
|        5 | 5484 | `	}` |
|    30037 | 5485 | `	if( zIn >= zEnd ){` |
|        - | 5486 | `		/* No such scheme,return the default stream */` |
|    30005 | 5487 | `		return pVm->pDefStream;` |
|        - | 5488 | `	}` |
|       35 | 5489 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 5490 | `	/* Remove leading and trailing white spaces */` |
|       35 | 5491 | `	SyStringFullTrim(&sDev);` |
|        - | 5492 | `	/* Perform a linear lookup on the installed stream devices */` |
|       35 | 5493 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       35 | 5494 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|       69 | 5495 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       69 | 5496 | `		pStream = apStream[n];` |
|       69 | 5497 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 5498 | `		/* Perfrom a case-insensitive comparison */` |
|       69 | 5499 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|       69 | 5500 | `		if( rc == 0 ){` |
|        - | 5501 | `			/* Stream device found */` |
|       35 | 5502 | `			*pzDevice = zNext;` |
|       35 | 5503 | `			return pStream;` |
|        - | 5504 | `		}` |
|       19 | 5505 | `	}` |
|        - | 5506 | `	/* No such stream,return NULL */` |
|      ! 0 | 5507 | `	return 0;` |
|    15021 | 5508 | `}` |
|        - | 5509 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 5510 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 5511 |  |
