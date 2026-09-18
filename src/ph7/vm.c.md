# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2377/2841 lines (83.67%)

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
|  1336147 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   77 | `{` |
|  1336152 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       40 |   79 | `		return TRUE;` |
|        - |   80 | `	}` |
|  1336114 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   82 | `		return TRUE;` |
|        - |   83 | `	}` |
|  1336104 |   84 | `	return FALSE;` |
|   668604 |   85 | `}` |
|        - |   86 | `/*` |
|        - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   93 | ` * still go through the existing numeric coercion.` |
|        - |   94 | ` */` |
|   375477 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   96 | `{` |
|        - |   97 | `	SyString sStr;` |
|   375482 |   98 | `	sxu8 bReal = FALSE;` |
|   375482 |   99 | `	const char *zTail = 0;` |
|        - |  100 | `	const char *zEnd;` |
|   375482 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   375468 |  102 | `		return FALSE;` |
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
|   187849 |  119 | `}` |
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
|  1511044 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  139 | `	const SyString *pName,  /* Constant name */` |
|        - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |  142 | `	)` |
|        5 |  143 | `{` |
|  1511049 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|        5 |  145 | `}` |
|        - |  146 | `/*` |
|        - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|        - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|        - |  149 | ` */` |
|  1511114 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|  1511119 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|  1511119 |  165 | `	if( pEntry ){` |
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
|  1511117 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|  1511117 |  190 | `	if( pCons == 0 ){` |
|      ! 0 |  191 | `		return 0;` |
|        - |  192 | `	}` |
|        - |  193 | `	/* Duplicate constant name */` |
|  1511117 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1511117 |  195 | `	if( zDupName == 0 ){` |
|      ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  197 | `		return 0;` |
|        - |  198 | `	}` |
|  1511117 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|  1511117 |  200 | `	if( pFile ){` |
|       73 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       34 |  202 | `	}` |
|  1511117 |  203 | `	pCons->nLine = nLine;` |
|  1511117 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|        - |  205 | `	/* Install the constant */` |
|  1511117 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|  1511117 |  207 | `	pCons->xExpand = xExpand;` |
|  1511117 |  208 | `	pCons->pUserData = pUserData;` |
|  1511117 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1511117 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|  1511117 |  211 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  214 | `		return rc;` |
|        - |  215 | `	}` |
|        - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|  1511117 |  217 | `	return SXRET_OK;` |
|   755562 |  218 | `}` |
|        - |  219 | `/*` |
|        - |  220 | ` * Allocate a new foreign function instance.` |
|        - |  221 | ` * This function return SXRET_OK on success. Any other` |
|        - |  222 | ` * return value indicates failure.` |
|        - |  223 | ` * Please refer to the official documentation for an introduction to` |
|        - |  224 | ` * the foreign function mechanism.` |
|        - |  225 | ` */` |
|  2213836 |  226 | `static sxi32 PH7_NewForeignFunction(` |
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
|  2213841 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  2213841 |  238 | `	if( pFunc == 0 ){` |
|      ! 0 |  239 | `		return SXERR_MEM;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate function name */` |
|  2213841 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2213841 |  243 | `	if( zDup == 0 ){` |
|      ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  245 | `		return SXERR_MEM;` |
|        - |  246 | `	}` |
|        - |  247 | `	/* Zero the structure */` |
|  2213841 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |  249 | `	/* Initialize structure fields */` |
|  2213841 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  2213841 |  251 | `	pFunc->pVm   = pVm;` |
|  2213841 |  252 | `	pFunc->xFunc = xFunc;` |
|  2213841 |  253 | `	pFunc->pUserData = pUserData;` |
|  2213841 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  255 | `	/* Write a pointer to the new function */` |
|  2213841 |  256 | `	*ppOut = pFunc;` |
|  2213841 |  257 | `	return SXRET_OK;` |
|  1106923 |  258 | `}` |
|        - |  259 | `/*` |
|        - |  260 | ` * Install a foreign function and it's associated callback so that` |
|        - |  261 | ` * it can be invoked from the target PHP code.` |
|        - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |  263 | ` * return value indicates failure.` |
|        - |  264 | ` * Please refer to the official documentation for an introduction to` |
|        - |  265 | ` * the foreign function mechanism.` |
|        - |  266 | ` */` |
|  2217224 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  2217229 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  2217229 |  279 | `	if( pEntry ){` |
|     3393 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     3393 |  281 | `		pFunc->pUserData = pUserData;` |
|     3393 |  282 | `		pFunc->xFunc = xFunc;` |
|     3393 |  283 | `		SySetReset(&pFunc->aAux);` |
|        - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|        - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|        - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|        - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|     3393 |  288 | `		pFunc->nMinArg  = 0;` |
|     3393 |  289 | `		pFunc->nMaxArg  = 0;` |
|     3393 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|     3393 |  291 | `		pFunc->bAtLeast = 0;` |
|     3393 |  292 | `		return SXRET_OK;` |
|        - |  293 | `	}` |
|        - |  294 | `	/* Create a new user function */` |
|  2213841 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  2213841 |  296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  297 | `		return rc;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* Install the function in the corresponding hashtable */` |
|  2213841 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  2213841 |  301 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  304 | `		return rc;` |
|        - |  305 | `	}` |
|        - |  306 | `	/* User function successfully installed */` |
|  2213841 |  307 | `	return SXRET_OK;` |
|  1108617 |  308 | `}` |
|        - |  309 | `/*` |
|        - |  310 | ` * Initialize a VM function.` |
|        - |  311 | ` */` |
|  3408442 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |  315 | `	const char *zName,  /* Function name */` |
|        - |  316 | `	sxu32 nByte,        /* zName length */` |
|        - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |  318 | `	void *pUserData     /* Function private data */` |
|        - |  319 | `	)` |
|        5 |  320 | `{` |
|        - |  321 | `	/* Zero the structure */` |
|  3408447 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |  323 | `	/* Initialize structure fields */` |
|        - |  324 | `	/* Arguments container */` |
|  3408447 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |  326 | `	/* Static variable container */` |
|  3408447 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |  328 | `	/* Bytecode container */` |
|  3408447 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |  330 | `    /* Preallocate some instruction slots */` |
|  3408447 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |  332 | `	/* Closure environment */` |
|  3408447 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|  3408447 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  336 | `	/* Declared #[...] attributes */` |
|  3408447 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  3408447 |  338 | `	pFunc->iFlags = iFlags;` |
|  3408447 |  339 | `	pFunc->pUserData = pUserData;` |
|        - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|  3408447 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|  3408447 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|  3403845 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|  1701925 |  346 | `	}else{` |
|        - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|        - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|        - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     4607 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     4607 |  351 | `		if( pFile ){` |
|     4607 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|     2301 |  353 | `		}` |
|        - |  354 | `	}` |
|  3408447 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|  3408447 |  356 | `	return SXRET_OK;` |
|        5 |  357 | `}` |
|        - |  358 | `/*` |
|        - |  359 | ` * Namespace-aware function lookup.` |
|        - |  360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |  361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |  362 | ` */` |
|        - |  363 | `/*` |
|        - |  364 | ` * Install a user defined function in the corresponding VM container.` |
|        - |  365 | ` */` |
|  5829538 |  366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |  367 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  368 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |  369 | `	SyString *pName     /* Function name */` |
|        - |  370 | `	)` |
|        5 |  371 | `{` |
|        - |  372 | `	SyHashEntry *pEntry;` |
|        - |  373 | `	sxi32 rc;` |
|  5829543 |  374 | `	if( pName == 0 ){` |
|        - |  375 | `		/* Use the built-in name */` |
|   520903 |  376 | `		pName = &pFunc->sName;` |
|   260449 |  377 | `	}` |
|        - |  378 | `	/* Check for duplicates (functions with the same name) first */` |
|  5829543 |  379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  5829543 |  380 | `	if( pEntry ){` |
|  2902737 |  381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  2902737 |  382 | `		if( pLink != pFunc ){` |
|        - |  383 | `			/* Link */` |
|       32 |  384 | `			pFunc->pNextName = pLink;` |
|       32 |  385 | `			pEntry->pUserData = pFunc;` |
|       15 |  386 | `		}` |
|  2902737 |  387 | `		return SXRET_OK;` |
|        - |  388 | `	}` |
|        - |  389 | `	/* First time seen */` |
|  2926811 |  390 | `	pFunc->pNextName = 0;` |
|  2926811 |  391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|  2926811 |  392 | `	return rc;` |
|  2914774 |  393 | `}` |
|        - |  394 | `/*` |
|        - |  395 | ` * Install a user defined class in the corresponding VM container.` |
|        - |  396 | ` */` |
|   528238 |  397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |  398 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |  399 | `	ph7_class *pClass /* Target Class */` |
|        - |  400 | `	)` |
|        5 |  401 | `{` |
|   528243 |  402 | `	SyString *pName = &pClass->sName;` |
|        - |  403 | `	SyHashEntry *pEntry;` |
|        - |  404 | `	sxi32 rc;` |
|        - |  405 | `	/* Check for duplicates */` |
|   528243 |  406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   528243 |  407 | `	if( pEntry ){` |
|        3 |  408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |  409 | `		/* Link entry with the same name */` |
|        3 |  410 | `		pClass->pNextName = pLink;` |
|        3 |  411 | `		pEntry->pUserData = pClass;` |
|        3 |  412 | `		return SXRET_OK;` |
|        - |  413 | `	}` |
|   528241 |  414 | `	pClass->pNextName = 0;` |
|        - |  415 | `	/* Perform a simple hashtable insertion */` |
|   528241 |  416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   528241 |  417 | `	return rc;` |
|   264124 |  418 | `}` |
|        - |  419 | `/*` |
|        - |  420 | ` * Instruction builder interface.` |
|        - |  421 | ` */` |
| 98100092 |  422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |  423 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |  424 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |  425 | `	sxi32 iP1,    /* First operand */` |
|        - |  426 | `	sxu32 iP2,    /* Second operand */` |
|        - |  427 | `	void *p3,     /* Third operand */` |
|        - |  428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |  429 | `	)` |
|        5 |  430 | `{` |
|        - |  431 | `	VmInstr sInstr;` |
| 98100097 |  432 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - |  433 | `	sxi32 rc;` |
|        - |  434 | `	/* Fill the VM instruction */` |
| 98100097 |  435 | `	sInstr.iOp = (sxu8)iOp;` |
| 98100097 |  436 | `	sInstr.iP1 = iP1;` |
| 98100097 |  437 | `	sInstr.iP2 = iP2;` |
| 98100097 |  438 | `	sInstr.p3  = p3;` |
|        - |  439 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|        - |  440 | `	 * compiled (that is how they read its text), so the current token IS this` |
|        - |  441 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|        - |  442 | `	 * between statements, hence the range check. */` |
| 98100097 |  443 | `	sInstr.nLine = 0;` |
| 98100097 |  444 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
| 41796519 |  445 | `		sInstr.nLine = pGen->pIn->nLine;` |
| 77201840 |  446 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|        - |  447 | `		/* Past the end (statement tail): blame the last real token. */` |
| 56090089 |  448 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
| 28045042 |  449 | `	}` |
| 98100097 |  450 | `	if( pIndex ){` |
|        - |  451 | `		/* Instruction index in the bytecode array */` |
|  6640349 |  452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|  3320172 |  453 | `	}` |
|        - |  454 | `	/* Finally,record the instruction */` |
| 98100097 |  455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 98100097 |  456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |  458 | `		/* Fall throw */` |
|      ! 0 |  459 | `	}` |
| 98100097 |  460 | `	return rc;` |
|        5 |  461 | `}` |
|        - |  462 | `/*` |
|        - |  463 | ` * Swap the current bytecode container with the given one.` |
|        - |  464 | ` */` |
|  9755700 |  465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |  466 | `{` |
|  9755705 |  467 | `	if( pContainer == 0 ){` |
|        - |  468 | `		/* Point to the default container */` |
|      ! 0 |  469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |  470 | `	}else{` |
|        - |  471 | `		/* Change container */` |
|  9755705 |  472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |  473 | `	}` |
|  9755705 |  474 | `	return SXRET_OK;` |
|        5 |  475 | `}` |
|        - |  476 | `/*` |
|        - |  477 | ` * Return the current bytecode container.` |
|        - |  478 | ` */` |
|  4877850 |  479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |  480 | `{` |
|  4877855 |  481 | `	return pVm->pByteContainer;` |
|        5 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |  485 | ` */` |
|  6845828 |  486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |  487 | `{` |
|        - |  488 | `	VmInstr *pInstr;` |
|  6845833 |  489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|  6845833 |  490 | `	return pInstr;` |
|        5 |  491 | `}` |
|        - |  492 | `/*` |
|        - |  493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |  494 | ` */` |
| 54021032 |  495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |  496 | `{` |
| 54021037 |  497 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |  498 | `}` |
|        - |  499 | `/*` |
|        - |  500 | ` * Pop the last VM instruction.` |
|        - |  501 | ` */` |
|  5383944 |  502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |  503 | `{` |
|  5383949 |  504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |  505 | `}` |
|        - |  506 | `/*` |
|        - |  507 | ` * Peek the last VM instruction.` |
|        - |  508 | ` */` |
| 20394972 |  509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |  510 | `{` |
| 20394977 |  511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |  512 | `}` |
|  1707524 |  513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |  514 | `{` |
|        - |  515 | `	VmInstr *aInstr;` |
|        - |  516 | `	sxu32 n;` |
|  1707529 |  517 | `	n = SySetUsed(pVm->pByteContainer);` |
|  1707529 |  518 | `	if( n < 2 ){` |
|      ! 0 |  519 | `		return 0;` |
|        - |  520 | `	}` |
|  1707529 |  521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|  1707529 |  522 | `	return &aInstr[n - 2];` |
|   853767 |  523 | `}` |
|        - |  524 | `/*` |
|        - |  525 | ` * Allocate a new virtual machine frame.` |
|        - |  526 | ` */` |
|   115876 |  527 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|        - |  528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |  530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  531 | `	)` |
|        5 |  532 | `{` |
|        - |  533 | `	VmFrame *pFrame;` |
|        - |  534 | `	/* Allocate a new vm frame */` |
|   115881 |  535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   115881 |  536 | `	if( pFrame == 0 ){` |
|      ! 0 |  537 | `		return 0;` |
|        - |  538 | `	}` |
|        - |  539 | `	/* Zero the structure */` |
|   115881 |  540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |  541 | `	/* Initialize frame fields */` |
|   115881 |  542 | `	pFrame->pUserData = pUserData;` |
|   115881 |  543 | `	pFrame->pThis = pThis;` |
|   115881 |  544 | `	pFrame->pVm = pVm;` |
|   115881 |  545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   115881 |  546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   115881 |  547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   115881 |  548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   115881 |  549 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|        - |  550 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|        - |  551 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   115881 |  552 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   115881 |  553 | `	return pFrame;` |
|    58048 |  554 | `}` |
|        - |  555 | `/* Forward declaration */` |
|        - |  556 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|        - |  557 | `/*` |
|        - |  558 | ` * Enter a VM frame.` |
|        - |  559 | ` */` |
|   115280 |  560 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|        - |  561 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  562 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |  563 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  564 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |  565 | `	)` |
|        5 |  566 | `{` |
|        - |  567 | `	VmFrame *pFrame;` |
|        - |  568 | `	/* Allocate a new frame */` |
|   115285 |  569 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   115285 |  570 | `	if( pFrame == 0 ){` |
|      ! 0 |  571 | `		return SXERR_MEM;` |
|        - |  572 | `	}` |
|        - |  573 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   115285 |  574 | `	pFrame->nCallLine = pVm->nCurLine;` |
|        - |  575 | `	/* Link to the list of active VM frame */` |
|   115285 |  576 | `	pFrame->pParent = pVm->pFrame;` |
|   115285 |  577 | `	pVm->pFrame = pFrame;` |
|   115285 |  578 | `	if( ppFrame ){` |
|        - |  579 | `		/* Write a pointer to the new VM frame */` |
|   111409 |  580 | `		*ppFrame = pFrame;` |
|    55807 |  581 | `	}` |
|   115285 |  582 | `	return SXRET_OK;` |
|    57750 |  583 | `}` |
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
|   111834 |  633 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 |  634 | `{` |
|   111839 |  635 | `	if( pVm->pResumeFrame == pFrame ){` |
|        3 |  636 | `		pVm->pResumeFrame = 0;` |
|        1 |  637 | `	}` |
|   111839 |  638 | `}` |
|        - |  639 | `/*` |
|        - |  640 | ` * Leave the top-most active frame.` |
|        - |  641 | ` */` |
|   110982 |  642 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |  643 | `{` |
|   110987 |  644 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   110987 |  645 | `	if( pCurFrame ){` |
|        - |  646 | `		/* Unlink from the list of active VM frame */` |
|   110987 |  647 | `		pVm->pFrame = pCurFrame->pParent;` |
|   110987 |  648 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  649 | `			VmSlot  *aSlot;` |
|        - |  650 | `			sxu32 n;` |
|        - |  651 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   105949 |  652 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   615161 |  653 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |  654 | `				/* Unset the local variable */` |
|   509217 |  655 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   255241 |  656 | `			}` |
|        - |  657 | `			/* Remove local reference */` |
|   105949 |  658 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   615245 |  659 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   509301 |  660 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   255283 |  661 | `			}` |
|    53077 |  662 | `		}` |
|        - |  663 | `		/* Release internal containers */` |
|   110987 |  664 | `		SyHashRelease(&pCurFrame->hVar);` |
|   110987 |  665 | `		SySetRelease(&pCurFrame->sArg);` |
|   110987 |  666 | `		SySetRelease(&pCurFrame->sLocal);` |
|   110987 |  667 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |  668 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|        - |  669 | `		 * containers above — released for every frame, including transparent` |
|        - |  670 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   110987 |  671 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|        - |  672 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   110987 |  673 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|        - |  674 | `		/* Release the whole structure */` |
|   110987 |  675 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    55596 |  676 | `	}` |
|   110987 |  677 | `}` |
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
|     6904 |  699 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        4 |  700 | `{` |
|        - |  701 | `	VmFrame *pFrame;` |
|    13842 |  702 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     6950 |  703 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  704 | `		sxu32 n;` |
|     7010 |  705 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|       76 |  706 | `			if( aSlot[n].nIdx == nIdx ){` |
|        - |  707 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       13 |  708 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       13 |  709 | `				(void)SySetPop(&pFrame->sLocal);` |
|       13 |  710 | `				return TRUE; /* Slot owned by exactly one frame */` |
|        - |  711 | `			}` |
|       34 |  712 | `		}` |
|     3471 |  713 | `	}` |
|     6896 |  714 | `	return FALSE;` |
|     3456 |  715 | `}` |
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
| 10717449 |  730 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |  731 | `{` |
| 10743930 |  732 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|    26481 |  733 | `		pFrame = pFrame->pParent;` |
|        5 |  734 | `	}` |
| 10717454 |  735 | `	return pFrame;` |
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
|     3068 |  760 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|        5 |  761 | `{` |
|     3073 |  762 | `	if( pVm->pResumeFrame == 0 ){` |
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
|     3054 |  773 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|     2570 |  774 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|     2078 |  775 | `	 \|\| pVm->iResumePc == 0 ){` |
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
|     3505 |  804 | `	while( pVm->pFrame != pEntryFrame` |
|     3986 |  805 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|     2535 |  806 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|      835 |  807 | `		VmLeaveFrame(&(*pVm));` |
|        5 |  808 | `	}` |
|     2065 |  809 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|     2065 |  810 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|        - |  811 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|        - |  812 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|        - |  813 | `	 * point router must not re-fire it after this resume. */` |
|     2065 |  814 | `	pVm->nBoundaryRc = 0;` |
|     2065 |  815 | `	return TRUE;` |
|     1539 |  816 | `}` |
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
|     2790 |  848 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  849 | `{` |
|     2795 |  850 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|     2795 |  851 | `	if( pClone == 0 ){` |
|      ! 0 |  852 | `		return 0;` |
|        - |  853 | `	}` |
|     2795 |  854 | `	*pClone = *pCompiled;` |
|     2795 |  855 | `	pClone->pCompiled = pCompiled;` |
|     2795 |  856 | `	pClone->iFinallyDone = 0;` |
|     2795 |  857 | `	pClone->iInCatch = 0;` |
|     2795 |  858 | `	pClone->pInflight = 0;` |
|     2795 |  859 | `	pClone->pFrame = 0;` |
|     2795 |  860 | `	return pClone;` |
|     1400 |  861 | `}` |
|     5504 |  862 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|        5 |  863 | `{` |
|     5509 |  864 | `	if( pExc && pExc->pCompiled ){` |
|        - |  865 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|        - |  866 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|        - |  867 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|        - |  868 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|     2783 |  869 | `		if( pExc->pInflight ){` |
|      ! 0 |  870 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|      ! 0 |  871 | `			pExc->pInflight = 0;` |
|      ! 0 |  872 | `		}` |
|     2783 |  873 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|     1389 |  874 | `	}` |
|     5509 |  875 | `}` |
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
|   263132 |  917 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |  918 | `{` |
|        - |  919 | `	sxu32 nUsed;` |
|   263137 |  920 | `	sxi32 rcOut = SXRET_OK;` |
|   263201 |  921 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
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
|   263137 |  952 | `	return rcOut;` |
|   131676 |  953 | `}` |
|        - |  954 | `/*` |
|        - |  955 | ` * Drop a body frame's pending catch/finally return: clear the flag and release` |
|        - |  956 | ` * the slot value. Safe on a frame with no pending return (the slot is then an` |
|        - |  957 | ` * empty MEMOBJ_NULL value and the release is a no-op).` |
|        - |  958 | ` */` |
|   158734 |  959 | `PH7_PRIVATE void VmClearFrameReturn(VmFrame *pFrame)` |
|        5 |  960 | `{` |
|   158739 |  961 | `	pFrame->bHasRet = 0;` |
|   158739 |  962 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   158739 |  963 | `}` |
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
|    34880 | 1113 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|        5 | 1114 | `{` |
|        - | 1115 | `	ph7_value sVal;` |
|    34885 | 1116 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|        - | 1117 | `	sxi32 rc;` |
|    34885 | 1118 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    34885 | 1119 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|        - | 1120 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|    34885 | 1121 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    34885 | 1122 | `	if( *ppMemObj ){` |
|    34885 | 1123 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|    17440 | 1124 | `	}` |
|    34885 | 1125 | `	PH7_MemObjRelease(&sVal);` |
|    34885 | 1126 | `	return rc;` |
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
|   703230 | 1141 | `static sxi32 VmMountUserClassAttrs(` |
|        - | 1142 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1143 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - | 1144 | `	)` |
|        5 | 1145 | `{` |
|        - | 1146 | `	ph7_class_attr *pAttr;` |
|        - | 1147 | `	SyHashEntry *pEntry;` |
|        - | 1148 | `	/* Reset the loop cursor */` |
|   703235 | 1149 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - | 1150 | `	/* Process only static and constant attribute */` |
|  2887489 | 1151 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1152 | `		/* Extract the current attribute */` |
|  2184263 | 1153 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2184258 | 1154 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  1432498 | 1155 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   340805 | 1156 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|        - | 1157 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|        - | 1158 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|        - | 1159 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|        - | 1160 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|        - | 1161 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|        - | 1162 | `			 * user catch, and initializers referencing constants of a class` |
|        - | 1163 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|        - | 1164 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|        - | 1165 | `			 * %s as value for class constant" fatal without any access). */` |
|   679851 | 1166 | `			continue;` |
|        - | 1167 | `		}` |
|  1504417 | 1168 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 1169 | `			ph7_value *pMemObj;` |
|    24705 | 1170 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|        - | 1171 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|        - | 1172 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|        - | 1173 | `				 * re-mount pass, so VM reuse still re-evaluates. */` |
|      856 | 1174 | `				continue;` |
|        - | 1175 | `			}` |
|        - | 1176 | `			/* Reserve a memory object for this constant/static attribute */` |
|    23851 | 1177 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    23851 | 1178 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1179 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1180 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 | 1181 | `					&pClass->sName,&pAttr->sName` |
|        - | 1182 | `					);` |
|      ! 0 | 1183 | `				return SXERR_MEM;` |
|        - | 1184 | `			}` |
|    23851 | 1185 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1186 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1187 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|        - | 1188 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|    23843 | 1189 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        - | 1190 | `				sxi32 rcExec;` |
|    23843 | 1191 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    23843 | 1192 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|    23843 | 1193 | `				pVm->nConstEvalDepth++;` |
|    23843 | 1194 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    23843 | 1195 | `				pVm->nConstEvalDepth--;` |
|    23843 | 1196 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|    23843 | 1197 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    23843 | 1198 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1199 | `					/* The initializer raised (self-referencing constant, or a` |
|        - | 1200 | `					 * throwing enum-case reference): park it for the fetch-point` |
|        - | 1201 | `					 * router — user classes mount mid-execution, so the throw` |
|        - | 1202 | `					 * lands catchably at the declaration site. */` |
|      ! 0 | 1203 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|    23843 | 1204 | `				}else if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 1205 | `					/* A nested evaluation detected a self-referencing constant:` |
|        - | 1206 | `					 * raise it at this, the outermost level. */` |
|      ! 0 | 1207 | `					VmBoundaryPark(&(*pVm),VmConstCycleThrow(&(*pVm)));` |
|      ! 0 | 1208 | `				}` |
|        - | 1209 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - | 1210 | `				 * against the declared type. A mismatch is a non-catchable` |
|        - | 1211 | `				 * fatal, raised here at definition time (matching PHP). */` |
|    23838 | 1212 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|    11924 | 1213 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|       35 | 1214 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|       35 | 1215 | `					if( rcType != SXRET_OK ){` |
|        6 | 1216 | `						return rcType;` |
|        - | 1217 | `					}` |
|       14 | 1218 | `				}` |
|    11917 | 1219 | `			}` |
|        - | 1220 | `			/* Record attribute index */` |
|    23847 | 1221 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - | 1222 | `			/* Install static attribute in the reference table */` |
|    23847 | 1223 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1224 | `			/* If this is a typed static property, register the slot so the` |
|        - | 1225 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - | 1226 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - | 1227 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - | 1228 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - | 1229 | `			 * already enforced above, so they need no store-time slot. */` |
|    23842 | 1230 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    11949 | 1231 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
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
|    11921 | 1250 | `		}` |
|        5 | 1251 | `	}` |
|   703231 | 1252 | `	return SXRET_OK;` |
|   351620 | 1253 | `}` |
|   702138 | 1254 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - | 1255 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1256 | `	ph7_class *pClass /* Class to be mounted */` |
|        - | 1257 | `	)` |
|        5 | 1258 | `{` |
|        - | 1259 | `	ph7_class_method *pMeth;` |
|        - | 1260 | `	SyHashEntry *pEntry;` |
|        - | 1261 | `	sxi32 rc;` |
|        - | 1262 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   702143 | 1263 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   702143 | 1264 | `	if( rc != SXRET_OK ){` |
|        6 | 1265 | `		return rc;` |
|        - | 1266 | `	}` |
|        - | 1267 | `	/* Install class methods */` |
|   702139 | 1268 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - | 1269 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - | 1270 | `		 */` |
|   307531 | 1271 | `		return SXRET_OK;` |
|        - | 1272 | `	}` |
|        - | 1273 | `	/* Create constructor alias if not yet done */` |
|   394613 | 1274 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - | 1275 | `		/* User constructor with the same base class name */` |
|    59107 | 1276 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|    59107 | 1277 | `		if( pEntry ){` |
|      ! 0 | 1278 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 1279 | `			/* Create the alias */` |
|      ! 0 | 1280 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 | 1281 | `		}` |
|    29551 | 1282 | `	}` |
|        - | 1283 | `	/* Install the methods now */` |
|   394613 | 1284 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  5910741 | 1285 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5318829 | 1286 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5318829 | 1287 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  5308645 | 1288 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  5308645 | 1289 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1290 | `				return rc;` |
|        - | 1291 | `			}` |
|  2654320 | 1292 | `		}` |
|        5 | 1293 | `	}` |
|        - | 1294 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   394613 | 1295 | `	pClass->bMounted = TRUE;` |
|   394613 | 1296 | `	return SXRET_OK;` |
|   351074 | 1297 | `}` |
|        - | 1298 | `/*` |
|        - | 1299 | ` * Allocate a private frame for attributes of the given` |
|        - | 1300 | ` * class instance (Object in the PHP jargon).` |
|        - | 1301 | ` */` |
|     8866 | 1302 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - | 1303 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 1304 | `	ph7_class_instance *pObj /* Class instance */` |
|        - | 1305 | `	)` |
|        5 | 1306 | `{` |
|     8871 | 1307 | `	ph7_class *pClass = pObj->pClass;` |
|        - | 1308 | `	ph7_class_attr *pAttr;` |
|        - | 1309 | `	SyHashEntry *pEntry;` |
|        - | 1310 | `	sxi32 rc;` |
|        - | 1311 | `	/* Install class attribute in the private frame associated with this instance */` |
|     8871 | 1312 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    46617 | 1313 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1314 | `		VmClassAttr *pVmAttr;` |
|        - | 1315 | `		/* Extract the current attribute */` |
|    37751 | 1316 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    37751 | 1317 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|    37751 | 1318 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1319 | `			return SXERR_MEM;` |
|        - | 1320 | `		}` |
|    37751 | 1321 | `		pVmAttr->pAttr = pAttr;` |
|    37751 | 1322 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - | 1323 | `			ph7_value *pMemObj;` |
|        - | 1324 | `			/* Reserve a memory object for this attribute */` |
|    29691 | 1325 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    29691 | 1326 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1327 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1328 | `				return SXERR_MEM;` |
|        - | 1329 | `			}` |
|    29691 | 1330 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|    29691 | 1331 | `			pVmAttr->iState = 0;` |
|    29691 | 1332 | `			pVmAttr->pOwner = pClass;` |
|    29691 | 1333 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1334 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1335 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|        - | 1336 | `				 * against the declaring class (no method frame here). */` |
|    10821 | 1337 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|    10821 | 1338 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    10821 | 1339 | `				VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    10821 | 1340 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    24283 | 1341 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1342 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - | 1343 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|      403 | 1344 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      199 | 1345 | `			}` |
|    29691 | 1346 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|    29691 | 1347 | `			if( rc != SXRET_OK ){` |
|        - | 1348 | `				VmSlot sSlot;` |
|        - | 1349 | `				/* Restore memory object */` |
|      ! 0 | 1350 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1351 | `				sSlot.pUserData = 0;` |
|      ! 0 | 1352 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1353 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1354 | `				return SXERR_MEM;` |
|        - | 1355 | `			}` |
|        - | 1356 | `			/* Install attribute in the reference table */` |
|    29691 | 1357 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1358 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - | 1359 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - | 1360 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|    29691 | 1361 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|    14848 | 1373 | `		}else{` |
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
|     8871 | 1385 | `	return SXRET_OK;` |
|     4438 | 1386 | `}` |
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
|  8544528 | 1559 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1560 | `{` |
|        - | 1561 | `	ph7_value *pObj;` |
|        - | 1562 | `	sxi32 rc;` |
|  8544533 | 1563 | `	if( pIndex ){` |
|        - | 1564 | `		/* Object index in the object table */` |
|  8532929 | 1565 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|  4266462 | 1566 | `	}` |
|        - | 1567 | `	/* Reserve a slot for the new object */` |
|  8544533 | 1568 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|  8544533 | 1569 | `	if( rc != SXRET_OK ){` |
|        - | 1570 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1571 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1572 | `		 */` |
|      ! 0 | 1573 | `		return 0;` |
|        - | 1574 | `	}` |
|  8544533 | 1575 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|  8544533 | 1576 | `	return pObj;` |
|  4272269 | 1577 | `}` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * Reserve a memory object.` |
|        - | 1580 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 1581 | ` */` |
|  2237324 | 1582 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1583 | `{` |
|        - | 1584 | `	ph7_value *pObj;` |
|        - | 1585 | `	sxi32 rc;` |
|  2237329 | 1586 | `	if( pIndex ){` |
|        - | 1587 | `		/* Object index in the object table */` |
|  2237329 | 1588 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1118662 | 1589 | `	}` |
|        - | 1590 | `	/* Reserve a slot for the new object */` |
|  2237329 | 1591 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2237329 | 1592 | `	if( rc != SXRET_OK ){` |
|        - | 1593 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1594 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1595 | `		 */` |
|      ! 0 | 1596 | `		return 0;` |
|        - | 1597 | `	}` |
|  2237329 | 1598 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2237329 | 1599 | `	return pObj;` |
|  1118667 | 1600 | `}` |
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
|     3868 | 1614 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - | 1615 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - | 1616 | `	 ph7 *pEngine /* Master engine */` |
|        - | 1617 | `	 )` |
|        5 | 1618 | `{` |
|        - | 1619 | `	ph7_value *pObj;` |
|        - | 1620 | `	sxi32 rc;` |
|        - | 1621 | `	/* Zero the structure */` |
|     3873 | 1622 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - | 1623 | `	/* Initialize VM fields */` |
|     3873 | 1624 | `	pVm->pEngine = &(*pEngine);` |
|     3873 | 1625 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|     3873 | 1626 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - | 1627 | `	/* Instructions containers */` |
|     3873 | 1628 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3873 | 1629 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3873 | 1630 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - | 1631 | `	/* Object containers */` |
|     3873 | 1632 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3873 | 1633 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - | 1634 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|     3873 | 1635 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|     3873 | 1636 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|     3873 | 1637 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|     3873 | 1638 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|        - | 1639 | `	/* Virtual machine internal containers */` |
|     3873 | 1640 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3873 | 1641 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3873 | 1642 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|     3873 | 1643 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|     3873 | 1644 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3873 | 1645 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3873 | 1646 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3873 | 1647 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3873 | 1648 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3873 | 1649 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3873 | 1650 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3873 | 1651 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3873 | 1652 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3873 | 1653 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3873 | 1654 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3873 | 1655 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3873 | 1656 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3873 | 1657 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3873 | 1658 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3873 | 1659 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|     3873 | 1660 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3873 | 1661 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3873 | 1662 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|     3873 | 1663 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3873 | 1664 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|     3873 | 1665 | `	pVm->nResourceIdNext = 1;` |
|     3873 | 1666 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3873 | 1667 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|     3873 | 1668 | `	pVm->pMagicSetThis = 0;` |
|     3873 | 1669 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|     3873 | 1670 | `	pVm->pHookSetThis = 0;` |
|     3873 | 1671 | `	pVm->pHookSetAttr = 0;` |
|     3873 | 1672 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3873 | 1673 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|     3873 | 1674 | `	pVm->pMagicCallThis = 0;` |
|     3873 | 1675 | `	pVm->pMagicCallClass = 0;` |
|     3873 | 1676 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|     3873 | 1677 | `	pVm->pIdleCallFrames = 0;` |
|     3873 | 1678 | `	pVm->pIdleOperandStacks = 0;` |
|     3873 | 1679 | `	pVm->nIdleOperandStacks = 0;` |
|     3873 | 1680 | `	pVm->pIdleStackNodes = 0;` |
|     3873 | 1681 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|     3873 | 1682 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|     3873 | 1683 | `	pVm->pPendingException = 0;` |
|     3873 | 1684 | `	pVm->pInflightException = 0;` |
|     3873 | 1685 | `	pVm->nInflightExcBase = 0;` |
|     3873 | 1686 | `	pVm->pResumeFrame = 0;` |
|     3873 | 1687 | `	pVm->iResumePc = 0;` |
|     3873 | 1688 | `	pVm->pResumeInstr = 0;` |
|     3873 | 1689 | `	pVm->iResumeStackDepth = 0;` |
|     3873 | 1690 | `	pVm->nBoundaryRc = 0;` |
|     3873 | 1691 | `	pVm->pConstEvalClass = 0;` |
|     3873 | 1692 | `	pVm->nConstEvalDepth = 0;` |
|     3873 | 1693 | `	pVm->pConstCycleAttr = 0;` |
|     3873 | 1694 | `	pVm->pConstCycleClass = 0;` |
|     3873 | 1695 | `	SySetReset(&pVm->aMagicGuard);` |
|     3873 | 1696 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 1697 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 1698 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 1699 | `	}` |
|     3873 | 1700 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|     3873 | 1701 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 1702 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 1703 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 1704 | `	}` |
|     3873 | 1705 | `	pVm->pHookSetAttr = 0;` |
|     3873 | 1706 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3873 | 1707 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 1708 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 1709 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 1710 | `	}` |
|     3873 | 1711 | `	pVm->pMagicCallClass = 0;` |
|     3873 | 1712 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        - | 1713 | `	/* Configuration containers */` |
|     3873 | 1714 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3873 | 1715 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3873 | 1716 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3873 | 1717 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3873 | 1718 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3873 | 1719 | `	pVm->iResponseStatus = 200;` |
|     3873 | 1720 | `	pVm->bHeadersSent = 0;` |
|     3873 | 1721 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - | 1722 | `	/* Error callbacks containers */` |
|     3873 | 1723 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3873 | 1724 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3873 | 1725 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3873 | 1726 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3873 | 1727 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - | 1728 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|        - | 1729 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|        - | 1730 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|        - | 1731 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|        - | 1732 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|        - | 1733 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|        - | 1734 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3873 | 1735 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|     3873 | 1736 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
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
|     3873 | 1749 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|        - | 1750 | `	/* JSON return status */` |
|     3873 | 1751 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 1752 | `	/* PRNG context */` |
|     3873 | 1753 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - | 1754 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|        - | 1755 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|     3873 | 1756 | `	pVm->mtSeeded = FALSE;` |
|        - | 1757 | `	/* Install the null constant */` |
|     3873 | 1758 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3873 | 1759 | `	if( pObj == 0 ){` |
|      ! 0 | 1760 | `		rc = SXERR_MEM;` |
|      ! 0 | 1761 | `		goto Err;` |
|        - | 1762 | `	}` |
|     3873 | 1763 | `	PH7_MemObjInit(pVm,pObj);` |
|        - | 1764 | `	/* Install the boolean TRUE constant */` |
|     3873 | 1765 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3873 | 1766 | `	if( pObj == 0 ){` |
|      ! 0 | 1767 | `		rc = SXERR_MEM;` |
|      ! 0 | 1768 | `		goto Err;` |
|        - | 1769 | `	}` |
|     3873 | 1770 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - | 1771 | `	/* Install the boolean FALSE constant */` |
|     3873 | 1772 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3873 | 1773 | `	if( pObj == 0 ){` |
|      ! 0 | 1774 | `		rc = SXERR_MEM;` |
|      ! 0 | 1775 | `		goto Err;` |
|        - | 1776 | `	}` |
|     3873 | 1777 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - | 1778 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - | 1779 | `	 * reuse the same slot rather than allocating a new one.` |
|        - | 1780 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3873 | 1781 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3873 | 1782 | `	if( pObj == 0 ){` |
|      ! 0 | 1783 | `		rc = SXERR_MEM;` |
|      ! 0 | 1784 | `		goto Err;` |
|        - | 1785 | `	}` |
|     3873 | 1786 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - | 1787 | `	/* Create the global frame */` |
|     3873 | 1788 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3873 | 1789 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1790 | `		goto Err;` |
|        - | 1791 | `	}` |
|        - | 1792 | `	/* Initialize the code generator */` |
|     3873 | 1793 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3873 | 1794 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1795 | `		goto Err;` |
|        - | 1796 | `	}` |
|        - | 1797 | `	/* VM correctly initialized,set the magic number */` |
|     3873 | 1798 | `	pVm->nMagic = PH7_VM_INIT;` |
|        - | 1799 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|        - | 1800 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|     3873 | 1801 | `	pVm->bCompilingBuiltin = 1;` |
|        - | 1802 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|     3873 | 1803 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|        - | 1804 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|        - | 1805 | `	 * compiled — its classes are internal too. */` |
|        - | 1806 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3873 | 1807 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - | 1808 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3873 | 1809 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3873 | 1810 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3873 | 1811 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3873 | 1812 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3873 | 1813 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - | 1814 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3873 | 1815 | `	pVm->pCoalesceObj = 0;` |
|     3873 | 1816 | `	pVm->bCoalesceArmed = 0;` |
|     3873 | 1817 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - | 1818 | `	/* Register Fiber internal C functions */` |
|     3873 | 1819 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3873 | 1820 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3873 | 1821 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3873 | 1822 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3873 | 1823 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3873 | 1824 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3873 | 1825 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3873 | 1826 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3873 | 1827 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3873 | 1828 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - | 1829 | `	/* Cache the Closure class pointer (closures are instances of it) */` |
|     3873 | 1830 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|     3873 | 1831 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|     3873 | 1832 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|        - | 1833 | `	/* Closure::bind/bindTo/call/fromCallable native delegates (Increment 2) */` |
|     3873 | 1834 | `	ph7_create_function(pVm,"__closure_bindTo",vm_builtin_Closure_bindTo,0);` |
|     3873 | 1835 | `	ph7_create_function(pVm,"__closure_fromCallable",vm_builtin_Closure_fromCallable,0);` |
|        - | 1836 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|     3873 | 1837 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        - | 1838 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3873 | 1839 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3873 | 1840 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3873 | 1841 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3873 | 1842 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3873 | 1843 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3873 | 1844 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3873 | 1845 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3873 | 1846 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3873 | 1847 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3873 | 1848 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - | 1849 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|        - | 1850 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|        - | 1851 | `	 * internal; the Traversable pointer above must already be cached. */` |
|     3873 | 1852 | `	PH7_VmInstallReflection(&(*pVm));` |
|     3873 | 1853 | `	PH7_VmInstallDateTime(&(*pVm));` |
|     3873 | 1854 | `	PH7_VmInstallSpl(&(*pVm));` |
|     3873 | 1855 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|     3873 | 1856 | `	PH7_VmInstallSession(&(*pVm));` |
|     3873 | 1857 | `	PH7_VmInstallIni(&(*pVm));` |
|        - | 1858 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1859 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|        - | 1860 | `	 * XMLWriter class libraries that build on it. */` |
|     3873 | 1861 | `	PH7_VmInstallLibxml(&(*pVm));` |
|     3873 | 1862 | `	PH7_VmInstallDom(&(*pVm));` |
|     3873 | 1863 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|        - | 1864 | `#endif` |
|     3873 | 1865 | `	pVm->bCompilingBuiltin = 0;` |
|        - | 1866 | `	/* Reset the code generator */` |
|     3873 | 1867 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3873 | 1868 | `	return SXRET_OK;` |
|      ! 0 | 1869 | `Err:` |
|      ! 0 | 1870 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 | 1871 | `	return rc;` |
|     1939 | 1872 | `}` |
|        - | 1873 | `/*` |
|        - | 1874 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - | 1875 | ` * routine which store the output in an internal blob.` |
|        - | 1876 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - | 1877 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - | 1878 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - | 1879 | ` * Refer to the official docurmentation for additional information.` |
|        - | 1880 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - | 1881 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - | 1882 | ` * to finish executing and extracting the output.` |
|        - | 1883 | ` */` |
|       66 | 1884 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - | 1885 | `	const void *pOut,   /* VM Generated output*/` |
|        - | 1886 | `	unsigned int nLen,  /* Generated output length */` |
|        - | 1887 | `	void *pUserData     /* User private data */` |
|        - | 1888 | `	)` |
|      ! 0 | 1889 | `{` |
|        - | 1890 | `	 sxi32 rc;` |
|        - | 1891 | `	 /* Store the output in an internal BLOB */` |
|       66 | 1892 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       66 | 1893 | `	 return rc;` |
|      ! 0 | 1894 | `}` |
|        - | 1895 | `/*` |
|        - | 1896 | ` * Track output length and mark headers as sent when output reaches` |
|        - | 1897 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - | 1898 | ` */` |
|    43234 | 1899 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 | 1900 | `{` |
|    43239 | 1901 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    43239 | 1902 | `	if( xCons != VmObConsumer ){` |
|    12971 | 1903 | `		pVm->nOutputLen += nLen;` |
|    12971 | 1904 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1413 | 1905 | `			pVm->bHeadersSent = 1;` |
|      704 | 1906 | `		}` |
|     6483 | 1907 | `	}` |
|    43239 | 1908 | `}` |
|        - | 1909 | `/*` |
|        - | 1910 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|        - | 1911 | ` *` |
|        - | 1912 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|        - | 1913 | ` * (no instruction pushes more than one net slot), and that is what` |
|        - | 1914 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|        - | 1915 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|        - | 1916 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|        - | 1917 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|        - | 1918 | ` *` |
|        - | 1919 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|        - | 1920 | ` * conservative BY CONSTRUCTION:` |
|        - | 1921 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|        - | 1922 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|        - | 1923 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|        - | 1924 | ` *     pop — makes height go negative, which triggers fallback.` |
|        - | 1925 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|        - | 1926 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|        - | 1927 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|        - | 1928 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|        - | 1929 | ` *     bound. There is no partial/unsafe middle.` |
|        - | 1930 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|        - | 1931 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|        - | 1932 | ` *     instruction-count bound -> fallback.` |
|        - | 1933 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|        - | 1934 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|        - | 1935 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|        - | 1936 | ` *` |
|        - | 1937 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|        - | 1938 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|        - | 1939 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|        - | 1940 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|        - | 1941 | ` *` |
|        - | 1942 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|        - | 1943 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|        - | 1944 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|        - | 1945 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|        - | 1946 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|        - | 1947 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|        - | 1948 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|        - | 1949 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|        - | 1950 | ` * entry here.` |
|        - | 1951 | ` */` |
|        - | 1952 | `/*` |
|        - | 1953 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|        - | 1954 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|        - | 1955 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|        - | 1956 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|        - | 1957 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|        - | 1958 | ` */` |
|    51464 | 1959 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|        5 | 1960 | `{` |
|    51469 | 1961 | `	int push = 0, n = 0;` |
|        - | 1962 | `	sxi32 d;` |
|    51469 | 1963 | `	switch( pI->iOp ){` |
|        - | 1964 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|        - | 1965 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     8127 | 1966 | `	case PH7_OP_LOADC:` |
|        - | 1967 | `	case PH7_OP_DUP:` |
|    16259 | 1968 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|     5807 | 1969 | `	case PH7_OP_LOAD:` |
|    11619 | 1970 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|    11619 | 1971 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|      ! 0 | 1972 | `	case PH7_OP_LOAD_REF:` |
|      ! 0 | 1973 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1974 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|      344 | 1975 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|        - | 1976 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|        - | 1977 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      693 | 1978 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|        - | 1979 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|        - | 1980 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|      269 | 1981 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - | 1982 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|      543 | 1983 | `		if( pI->iP2 == 0 ){` |
|      543 | 1984 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|      274 | 1985 | `		}else{` |
|      ! 0 | 1986 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|      ! 0 | 1987 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|        - | 1988 | `		}` |
|      543 | 1989 | `		break;` |
|        - | 1990 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|        - | 1991 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|      156 | 1992 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|        - | 1993 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|        - | 1994 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|        - | 1995 | `	case PH7_OP_NOOP:` |
|      316 | 1996 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1997 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|        - | 1998 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      818 | 1999 | `	case PH7_OP_STORE:` |
|     1641 | 2000 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|     1641 | 2001 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|        - | 2002 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|     1262 | 2003 | `	case PH7_OP_POP:` |
|        - | 2004 | `	case PH7_OP_CONSUME:` |
|     2529 | 2005 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2006 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|        - | 2007 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|        - | 2008 | `	 * true pop count is a runtime value — never reaches here. */` |
|     1919 | 2009 | `	case PH7_OP_CALL:` |
|     3843 | 2010 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2011 | `	/* Jumps. */` |
|      147 | 2012 | `	case PH7_OP_JMP:` |
|      299 | 2013 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|      776 | 2014 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|     1557 | 2015 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|     1557 | 2016 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|        - | 2017 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|     2975 | 2018 | `	case PH7_OP_DONE:` |
|     5955 | 2019 | `		n = 0; break;` |
|     3132 | 2020 | `	default:` |
|     6269 | 2021 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|        - | 2022 | `	}` |
|    45205 | 2023 | `	*pPush = push; *pN = n;` |
|    45205 | 2024 | `	return 1;` |
|    25737 | 2025 | `}` |
|        - | 2026 | `/*` |
|        - | 2027 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|        - | 2028 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|        - | 2029 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|        - | 2030 | ` */` |
|     8298 | 2031 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|        5 | 2032 | `{` |
|        - | 2033 | `	void *pScratch;` |
|        - | 2034 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|        - | 2035 | `	sxu32 nQ, i, nIter, nCap;` |
|        - | 2036 | `	sxi32 iMax;` |
|        - | 2037 | `	int push, n, k;` |
|        - | 2038 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     8303 | 2039 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|        - | 2040 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|      ! 0 | 2041 | `		return VM_STACK_UNMODELED;` |
|        - | 2042 | `	}` |
|        - | 2043 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|    44885 | 2044 | `	for( i = 0; i < nInstr; i++ ){` |
|    42851 | 2045 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     6269 | 2046 | `			return VM_STACK_UNMODELED;` |
|        - | 2047 | `		}` |
|    18296 | 2048 | `	}` |
|        - | 2049 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|        - | 2050 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|        - | 2051 | `	 * first (the byte array last needs no alignment). */` |
|     2039 | 2052 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|     2039 | 2053 | `	if( pScratch == 0 ){` |
|      ! 0 | 2054 | `		return VM_STACK_UNMODELED;` |
|        - | 2055 | `	}` |
|     2039 | 2056 | `	aH  = (sxi32 *)pScratch;` |
|     2039 | 2057 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|     2039 | 2058 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|    12387 | 2059 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|     2039 | 2060 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|     2039 | 2061 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|    10657 | 2062 | `	while( nQ > 0 ){` |
|     8623 | 2063 | `		sxu32 pc = aQ[--nQ];` |
|        - | 2064 | `		sxi32 h;` |
|     8623 | 2065 | `		aIn[pc] = 0;` |
|     8623 | 2066 | `		h = aH[pc];` |
|     8623 | 2067 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     8623 | 2068 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     8623 | 2069 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     8623 | 2070 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|    15285 | 2071 | `		for( k = 0; k < n; k++ ){` |
|     6667 | 2072 | `			sxi32 hn = h + delta[k];` |
|     6667 | 2073 | `			sxu32 t = succ[k];` |
|     6667 | 2074 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|     6667 | 2075 | `			if( hn > aH[t] ){` |
|     6589 | 2076 | `				aH[t] = hn;` |
|     6589 | 2077 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|     3292 | 2078 | `			}` |
|     3336 | 2079 | `		}` |
|     8623 | 2080 | `		if( iMax < 0 ){ break; }` |
|        5 | 2081 | `	}` |
|     2039 | 2082 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|     2039 | 2083 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|     4154 | 2084 | `}` |
|        - | 2085 | `/*` |
|        - | 2086 | ` * Allocate a new operand stack so that we can start executing` |
|        - | 2087 | ` * our compiled PHP program.` |
|        - | 2088 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - | 2089 | ` * on success. NULL (Fatal error) on failure.` |
|        - | 2090 | ` *` |
|        - | 2091 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|        - | 2092 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|        - | 2093 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|        - | 2094 | ` * eval, coroutine, callbacks) call this directly.` |
|        - | 2095 | ` */` |
|   250193 | 2096 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|        - | 2097 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 2098 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - | 2099 | `	)` |
|        5 | 2100 | `{` |
|        - | 2101 | `	ph7_value *pStack;` |
|        - | 2102 | `  /* No instruction ever pushes more than a single element onto the` |
|        - | 2103 | `  ** stack and the stack never grows on successive executions of the` |
|        - | 2104 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - | 2105 | `  ** on the maximum stack depth required.` |
|        - | 2106 | `  **` |
|        - | 2107 | `  ** Allocation all the stack space we will ever need.` |
|        - | 2108 | `  */` |
|   250198 | 2109 | `	nInstr += VM_STACK_GUARD;` |
|   250198 | 2110 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   250198 | 2111 | `	if( pStack == 0 ){` |
|      ! 0 | 2112 | `		return 0;` |
|        - | 2113 | `	}` |
|        - | 2114 | `	/* Initialize the operand stack */` |
| 24320637 | 2115 | `	while( nInstr > 0 ){` |
| 24070444 | 2116 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
| 24070444 | 2117 | `		--nInstr;` |
|        5 | 2118 | `	}` |
|        - | 2119 | `	/* Ready for bytecode execution */` |
|   250198 | 2120 | `	return pStack;` |
|   125205 | 2121 | `}` |
|        - | 2122 | `/*` |
|        - | 2123 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|        - | 2124 | ` *` |
|        - | 2125 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|        - | 2126 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|        - | 2127 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|        - | 2128 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|        - | 2129 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|        - | 2130 | ` * the per-slot PH7_MemObjInit.` |
|        - | 2131 | ` *` |
|        - | 2132 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|        - | 2133 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|        - | 2134 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|        - | 2135 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|        - | 2136 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|        - | 2137 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|        - | 2138 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|        - | 2139 | ` *` |
|        - | 2140 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|        - | 2141 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|        - | 2142 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|        - | 2143 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|        - | 2144 | ` * recursion case is the one worth the O(1) simplicity.` |
|        - | 2145 | ` */` |
|        - | 2146 | `typedef struct VmIdleStack VmIdleStack;` |
|        - | 2147 | `struct VmIdleStack {` |
|        - | 2148 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|        - | 2149 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|        - | 2150 | `	VmIdleStack *pNext;  /* LIFO link */` |
|        - | 2151 | `};` |
|        - | 2152 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|        - | 2153 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|        - | 2154 | `                                    * (a large fallback-sized stack recursing would` |
|        - | 2155 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|        - | 2156 | `                                    * the tight-sized hot case is far below this) */` |
|        - | 2157 | `/*` |
|        - | 2158 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|        - | 2159 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|        - | 2160 | ` */` |
|   106220 | 2161 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|        5 | 2162 | `{` |
|   106225 | 2163 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   106225 | 2164 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|   106225 | 2165 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    16038 | 2166 | `		ph7_value *pStack = pIdle->pStack;` |
|    16038 | 2167 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    16038 | 2168 | `		pVm->nIdleOperandStacks--;` |
|        - | 2169 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|        - | 2170 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    16038 | 2171 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    16038 | 2172 | `		pVm->pIdleStackNodes = pIdle;` |
|    16038 | 2173 | `		return pStack; /* slots already released -> reusable without re-init */` |
|        - | 2174 | `	}` |
|    90192 | 2175 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    53220 | 2176 | `}` |
|        - | 2177 | `/*` |
|        - | 2178 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|        - | 2179 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|        - | 2180 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|        - | 2181 | ` * live value.` |
|        - | 2182 | ` */` |
|   105810 | 2183 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|        5 | 2184 | `{` |
|        - | 2185 | `	VmIdleStack *pIdle;` |
|        - | 2186 | `	sxu32 i;` |
|   105815 | 2187 | `	if( pStack == 0 ){` |
|      ! 0 | 2188 | `		return;` |
|        - | 2189 | `	}` |
|   105815 | 2190 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|    83856 | 2191 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|    83856 | 2192 | `		return;` |
|        - | 2193 | `	}` |
|        - | 2194 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|        - | 2195 | `	 * pool-allocate only when the spare list is empty. */` |
|    21964 | 2196 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    21964 | 2197 | `	if( pIdle ){` |
|    16038 | 2198 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|     8023 | 2199 | `	}else{` |
|     5931 | 2200 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|     5931 | 2201 | `		if( pIdle == 0 ){` |
|      ! 0 | 2202 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|      ! 0 | 2203 | `			return;` |
|        - | 2204 | `		}` |
|        - | 2205 | `	}` |
|  1314941 | 2206 | `	for( i = 0; i < nCap; i++ ){` |
|  1292982 | 2207 | `		PH7_MemObjRelease(&pStack[i]);` |
|        - | 2208 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|        - | 2209 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|        - | 2210 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|        - | 2211 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|        - | 2212 | `		 * across invocations — cheap defense in depth. */` |
|  1292982 | 2213 | `		pStack[i].nIdx = SXU32_HIGH;` |
|   647074 | 2214 | `	}` |
|    21964 | 2215 | `	pIdle->pStack = pStack;` |
|    21964 | 2216 | `	pIdle->nCap = nCap;` |
|    21964 | 2217 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    21964 | 2218 | `	pVm->pIdleOperandStacks = pIdle;` |
|    21964 | 2219 | `	pVm->nIdleOperandStacks++;` |
|    53015 | 2220 | `}` |
|        - | 2221 | `/* Forward declaration */` |
|        - | 2222 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - | 2223 | `/*` |
|        - | 2224 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - | 2225 | ` * This routine gets called by the PH7 engine after` |
|        - | 2226 | ` * successful compilation of the target PHP program.` |
|        - | 2227 | ` */` |
|     3388 | 2228 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - | 2229 | `	ph7_vm *pVm /* Target VM */` |
|        - | 2230 | `	)` |
|        5 | 2231 | `{` |
|        - | 2232 | `	SyHashEntry *pEntry;` |
|        - | 2233 | `	sxi32 rc;` |
|     3393 | 2234 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - | 2235 | `		/* Initialize your VM first */` |
|      ! 0 | 2236 | `		return SXERR_CORRUPT;` |
|        - | 2237 | `	}` |
|        - | 2238 | `	/* Mark the VM ready for byte-code execution */` |
|     3393 | 2239 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - | 2240 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - | 2241 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - | 2242 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - | 2243 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     3393 | 2244 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - | 2245 | `	/* Emit the DONE instruction */` |
|     3393 | 2246 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     3393 | 2247 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2248 | `		return SXERR_MEM;` |
|        - | 2249 | `	}` |
|        - | 2250 | `	/* Script return value */` |
|     3393 | 2251 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - | 2252 | `	/* Allocate a new operand stack */` |
|     3393 | 2253 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     3393 | 2254 | `	if( pVm->aOps == 0 ){` |
|      ! 0 | 2255 | `		return SXERR_MEM;` |
|        - | 2256 | `	}` |
|        - | 2257 | `	/* Set the default VM output consumer callback and it's` |
|        - | 2258 | `	 * private data. */` |
|     3393 | 2259 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     3393 | 2260 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - | 2261 | `	/* Allocate the reference table */` |
|     3393 | 2262 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     3393 | 2263 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     3393 | 2264 | `	if( pVm->apRefObj == 0 ){` |
|        - | 2265 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2266 | `		return SXERR_MEM;` |
|        - | 2267 | `	}` |
|        - | 2268 | `	/* Zero the reference table */` |
|     3393 | 2269 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - | 2270 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     3393 | 2271 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     3393 | 2272 | `	if( rc != SXRET_OK ){` |
|        - | 2273 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2274 | `		return rc;` |
|        - | 2275 | `	}` |
|        - | 2276 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - | 2277 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - | 2278 | `	 * every object/variable created during execution) is per-exec state that` |
|        - | 2279 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - | 2280 | `	 * below it is compile-time/init state that survives a reset. */` |
|     3393 | 2281 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - | 2282 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     3393 | 2283 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     3393 | 2284 | `	if( rc != SXRET_OK ){` |
|        - | 2285 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2286 | `		return rc;` |
|        - | 2287 | `	}` |
|        - | 2288 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     3393 | 2289 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - | 2290 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|     3393 | 2291 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|        - | 2292 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     3393 | 2293 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - | 2294 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     3393 | 2295 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - | 2296 | `#ifdef PH7_ENABLE_PCRE` |
|        - | 2297 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     3393 | 2298 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     3393 | 2299 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - | 2300 | `#endif` |
|        - | 2301 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2302 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|     3393 | 2303 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|        - | 2304 | `#endif` |
|        - | 2305 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|        - | 2306 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|     3393 | 2307 | `	VmSetBuiltinArity(&(*pVm));` |
|        - | 2308 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|     3393 | 2309 | `	VmSetBuiltinSignatures(&(*pVm));` |
|        - | 2310 | `	/* Initialize and install static and constants class attributes.` |
|        - | 2311 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - | 2312 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - | 2313 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - | 2314 | `	 * that function in sync when changing what is reserved here. */` |
|     3393 | 2315 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   464453 | 2316 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   461067 | 2317 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   461067 | 2318 | `		if( rc != SXRET_OK ){` |
|        3 | 2319 | `			return rc;` |
|        - | 2320 | `		}` |
|        5 | 2321 | `	}` |
|        - | 2322 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     3391 | 2323 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2324 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|     3391 | 2325 | `	pVm->nNextObjId = 1;` |
|        - | 2326 | `	/* VM is ready for bytecode execution */` |
|     3391 | 2327 | `	return SXRET_OK;` |
|     1699 | 2328 | `}` |
|        - | 2329 | `/*` |
|        - | 2330 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - | 2331 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - | 2332 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - | 2333 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - | 2334 | ` * a dangling node pointer in some other object's reference record.` |
|        - | 2335 | ` */` |
|        8 | 2336 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 | 2337 | `{` |
|        - | 2338 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - | 2339 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - | 2340 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      328 | 2341 | `	while( pVm->pRefList ){` |
|      320 | 2342 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 | 2343 | `	}` |
|        8 | 2344 | `}` |
|        - | 2345 | `/*` |
|        - | 2346 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - | 2347 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - | 2348 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - | 2349 | ` */` |
|       56 | 2350 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 | 2351 | `{` |
|       56 | 2352 | `	PH7_MemObjRelease(pObj);` |
|       56 | 2353 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       56 | 2354 | `}` |
|        - | 2355 | `/*` |
|        - | 2356 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - | 2357 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - | 2358 | ` * of statics).` |
|        - | 2359 | ` */` |
|     6760 | 2360 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 | 2361 | `{` |
|     6760 | 2362 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - | 2363 | `	sxu32 k;` |
|     6788 | 2364 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|       28 | 2365 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|       14 | 2366 | `	}` |
|     6760 | 2367 | `}` |
|        - | 2368 | `/*` |
|        - | 2369 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - | 2370 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - | 2371 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - | 2372 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - | 2373 | ` *    captured environment values, its name buffer and its structure (the` |
|        - | 2374 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - | 2375 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - | 2376 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - | 2377 | ` *    has its static sentinels reset.` |
|        - | 2378 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - | 2379 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - | 2380 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - | 2381 | ` */` |
|        8 | 2382 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 | 2383 | `{` |
|        - | 2384 | `	SyHashEntry *pEntry;` |
|        8 | 2385 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     6768 | 2386 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     6760 | 2387 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     6760 | 2388 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 2389 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - | 2390 | `			 * release its captured-by-value environment, then free the entry,` |
|        - | 2391 | `			 * name buffer and structure. */` |
|        4 | 2392 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 | 2393 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - | 2394 | `			sxu32 k;` |
|        4 | 2395 | `			VmResetFuncStatics(pFunc);` |
|        8 | 2396 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 | 2397 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 | 2398 | `			}` |
|        4 | 2399 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - | 2400 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 | 2401 | `			SyHashDeleteEntry2(pEntry);` |
|        4 | 2402 | `			if( zName ){` |
|        4 | 2403 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 | 2404 | `			}` |
|        4 | 2405 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 | 2406 | `			continue;` |
|        - | 2407 | `		}` |
|        - | 2408 | `		/* Named function: reset statics for every overload sharing this name. */` |
|    13512 | 2409 | `		while( pFunc ){` |
|     6756 | 2410 | `			VmResetFuncStatics(pFunc);` |
|     6756 | 2411 | `			pFunc = pFunc->pNextName;` |
|      ! 0 | 2412 | `		}` |
|      ! 0 | 2413 | `	}` |
|        8 | 2414 | `	pVm->closure_cnt = 0;` |
|        8 | 2415 | `}` |
|        - | 2416 | `/*` |
|        - | 2417 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - | 2418 | ` * are already gone (each object's destructor removed its own during the object` |
|        - | 2419 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - | 2420 | ` * the class re-mount registers fresh ones.` |
|        - | 2421 | ` */` |
|        8 | 2422 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 | 2423 | `{` |
|        - | 2424 | `	SyHashEntry *pEntry;` |
|        - | 2425 | `	/* Common case: no class static typed properties — table already empty. */` |
|        8 | 2426 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        4 | 2427 | `		return;` |
|        - | 2428 | `	}` |
|        - | 2429 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - | 2430 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 | 2431 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 | 2432 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 | 2433 | `		if( pEntry->pUserData ){` |
|        4 | 2434 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 | 2435 | `		}` |
|      ! 0 | 2436 | `	}` |
|        4 | 2437 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 | 2438 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        4 | 2439 | `}` |
|        - | 2440 | `/*` |
|        - | 2441 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|        - | 2442 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|        - | 2443 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|        - | 2444 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|        - | 2445 | ` * comparing equal — both used to cast to 1.` |
|        - | 2446 | ` *` |
|        - | 2447 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|        - | 2448 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|        - | 2449 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|        - | 2450 | ` * aborting a cast.` |
|        - | 2451 | ` */` |
|       24 | 2452 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|        2 | 2453 | `{` |
|        - | 2454 | `	SyHashEntry *pEntry;` |
|        - | 2455 | `	phl_res_id *pRec;` |
|       26 | 2456 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|      ! 0 | 2457 | `		return 0;` |
|        - | 2458 | `	}` |
|       26 | 2459 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|       26 | 2460 | `	if( pEntry ){` |
|       20 | 2461 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|        - | 2462 | `	}` |
|        8 | 2463 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        8 | 2464 | `	if( pRec == 0 ){` |
|      ! 0 | 2465 | `		return 0;` |
|        - | 2466 | `	}` |
|        8 | 2467 | `	pRec->pRes = pRes;` |
|        8 | 2468 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        8 | 2469 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|      ! 0 | 2470 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|      ! 0 | 2471 | `		return 0;` |
|        - | 2472 | `	}` |
|        8 | 2473 | `	return pRec->nId;` |
|       14 | 2474 | `}` |
|        - | 2475 | `/*` |
|        - | 2476 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|        - | 2477 | ` * 1 for the next run, matching a fresh php process.` |
|        - | 2478 | ` */` |
|        8 | 2479 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|      ! 0 | 2480 | `{` |
|        - | 2481 | `	SyHashEntry *pEntry;` |
|        8 | 2482 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        8 | 2483 | `		pVm->nResourceIdNext = 1;` |
|        8 | 2484 | `		return;` |
|        - | 2485 | `	}` |
|      ! 0 | 2486 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|      ! 0 | 2487 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|      ! 0 | 2488 | `		if( pEntry->pUserData ){` |
|      ! 0 | 2489 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|      ! 0 | 2490 | `		}` |
|      ! 0 | 2491 | `	}` |
|      ! 0 | 2492 | `	SyHashRelease(&pVm->hResourceId);` |
|      ! 0 | 2493 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      ! 0 | 2494 | `	pVm->nResourceIdNext = 1;` |
|        4 | 2495 | `}` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - | 2498 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - | 2499 | ` *` |
|        - | 2500 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - | 2501 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - | 2502 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - | 2503 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - | 2504 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - | 2505 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - | 2506 | ` *` |
|        - | 2507 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - | 2508 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - | 2509 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - | 2510 | ` * exception/error-handler state, the reference table and every object/array` |
|        - | 2511 | ` * reserved during the run.` |
|        - | 2512 | ` *` |
|        - | 2513 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - | 2514 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - | 2515 | ` * global-scope destructors never fired.` |
|        - | 2516 | ` */` |
|        8 | 2517 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 | 2518 | `{` |
|        - | 2519 | `	sxu32 nWater,n;` |
|        8 | 2520 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 | 2521 | `		return SXERR_CORRUPT;` |
|        - | 2522 | `	}` |
|        8 | 2523 | `	nWater = pVm->nSuperBaseline;` |
|        - | 2524 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - | 2525 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        8 | 2526 | `	pVm->pGlobal = 0;` |
|        - | 2527 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|        - | 2528 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|        - | 2529 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|        - | 2530 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|        - | 2531 | `	 * object); unref'ing here would race the teardown below. */` |
|        8 | 2532 | `	pVm->pClosureThis = 0;` |
|        8 | 2533 | `	pVm->pClosureScope = 0;` |
|        - | 2534 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - | 2535 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - | 2536 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - | 2537 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        8 | 2538 | `	pVm->bInReset = 1;` |
|        - | 2539 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        8 | 2540 | `	VmResetRefTable(&(*pVm));` |
|        - | 2541 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - | 2542 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - | 2543 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - | 2544 | `	 * re-run define() overwrites the value in place). */` |
|        8 | 2545 | `	VmResetFunctionState(&(*pVm));` |
|        - | 2546 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - | 2547 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      344 | 2548 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      336 | 2549 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      336 | 2550 | `		if( pObj ){` |
|      336 | 2551 | `			PH7_MemObjRelease(pObj);` |
|      168 | 2552 | `		}` |
|      168 | 2553 | `	}` |
|        - | 2554 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - | 2555 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        8 | 2556 | `	VmResetTypedSlots(&(*pVm));` |
|        - | 2557 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|        - | 2558 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        8 | 2559 | `	VmResetResourceIds(&(*pVm));` |
|        - | 2560 | `	/* (5) Unwind any active frames back to none. */` |
|       16 | 2561 | `	while( pVm->pFrame ){` |
|        8 | 2562 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 2563 | `	}` |
|        - | 2564 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        8 | 2565 | `	pVm->bInReset = 0;` |
|        - | 2566 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - | 2567 | `	 * slots (their indices no longer exist). */` |
|        8 | 2568 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        8 | 2569 | `	SySetReset(&pVm->aFreeObj);` |
|        - | 2570 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        8 | 2571 | `	SyHashRelease(&pVm->hSuper);` |
|        8 | 2572 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - | 2573 | `	/* (8) Drain remaining per-exec containers. */` |
|        8 | 2574 | `	SySetReset(&pVm->aSelf);` |
|        - | 2575 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - | 2576 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - | 2577 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        8 | 2578 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 | 2579 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 | 2580 | `		if( pCB ){` |
|        - | 2581 | `			int iArg;` |
|      ! 0 | 2582 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2583 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 | 2584 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 | 2585 | `			}` |
|      ! 0 | 2586 | `		}` |
|      ! 0 | 2587 | `	}` |
|        8 | 2588 | `	SySetReset(&pVm->aShutdown);` |
|        - | 2589 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|        - | 2590 | `	 * aborted program can leave entries behind). */` |
|        8 | 2591 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        8 | 2592 | `	SySetReset(&pVm->aException);` |
|        8 | 2593 | `	SySetReset(&pVm->aFinallyAction);` |
|        8 | 2594 | `	pVm->pPendingException = 0;` |
|        8 | 2595 | `	pVm->pInflightException = 0;` |
|        8 | 2596 | `	pVm->nInflightExcBase = 0;` |
|        8 | 2597 | `	pVm->pResumeFrame = 0;` |
|        8 | 2598 | `	pVm->iResumePc = 0;` |
|        8 | 2599 | `	pVm->pResumeInstr = 0;` |
|        8 | 2600 | `	pVm->iResumeStackDepth = 0;` |
|        8 | 2601 | `	pVm->nBoundaryRc = 0;` |
|        8 | 2602 | `	pVm->pConstEvalClass = 0;` |
|        8 | 2603 | `	pVm->nConstEvalDepth = 0;` |
|        8 | 2604 | `	pVm->pConstCycleAttr = 0;` |
|        8 | 2605 | `	pVm->pConstCycleClass = 0;` |
|        8 | 2606 | `	SySetReset(&pVm->aMagicGuard);` |
|        - | 2607 | `	{` |
|        - | 2608 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|        - | 2609 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        8 | 2610 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        8 | 2611 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|        - | 2612 | `		sxu32 iRmw;` |
|        8 | 2613 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|      ! 0 | 2614 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|      ! 0 | 2615 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|      ! 0 | 2616 | `		}` |
|        8 | 2617 | `		SySetReset(&pVm->aHookRmw);` |
|        - | 2618 | `	}` |
|        8 | 2619 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 2620 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 2621 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 2622 | `	}` |
|        8 | 2623 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        8 | 2624 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 2625 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 2626 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 2627 | `	}` |
|        8 | 2628 | `	pVm->pHookSetAttr = 0;` |
|        8 | 2629 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        8 | 2630 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 2631 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 2632 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 2633 | `	}` |
|        8 | 2634 | `	pVm->pMagicCallClass = 0;` |
|        8 | 2635 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        8 | 2636 | `	pVm->nExceptDepth = 0;` |
|        - | 2637 | `	/* spl_autoload_register() callbacks are per request */` |
|        8 | 2638 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 | 2639 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 | 2640 | `		if( pCB ){` |
|      ! 0 | 2641 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2642 | `		}` |
|      ! 0 | 2643 | `	}` |
|        8 | 2644 | `	SySetReset(&pVm->aAutoload);` |
|        - | 2645 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - | 2646 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        8 | 2647 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 | 2648 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 | 2649 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 | 2650 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      ! 0 | 2651 | `	}` |
|        - | 2652 | `	/* Output buffers */` |
|        8 | 2653 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 | 2654 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 | 2655 | `		if( pOb ){` |
|      ! 0 | 2656 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 | 2657 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 | 2658 | `		}` |
|      ! 0 | 2659 | `	}` |
|        8 | 2660 | `	SySetReset(&pVm->aOB);` |
|        8 | 2661 | `	pVm->nObDepth = 0;` |
|        - | 2662 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - | 2663 | `	{` |
|        8 | 2664 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        8 | 2665 | `		if( rc == SXRET_OK ){` |
|        8 | 2666 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        4 | 2667 | `		}` |
|        8 | 2668 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2669 | `			return rc;` |
|        - | 2670 | `		}` |
|        - | 2671 | `	}` |
|        - | 2672 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|        - | 2673 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|        - | 2674 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|        - | 2675 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|        - | 2676 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|        - | 2677 | `	{` |
|        - | 2678 | `		SyHashEntry *pEntry;` |
|        8 | 2679 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1100 | 2680 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1092 | 2681 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 2682 | `			ph7_class_attr *pAttr;` |
|        - | 2683 | `			SyHashEntry *pAttrEntry;` |
|     1092 | 2684 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     6330 | 2685 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     4692 | 2686 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     4692 | 2687 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     1340 | 2688 | `					pAttr->nIdx = SXU32_HIGH;` |
|     1340 | 2689 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      670 | 2690 | `				}` |
|      ! 0 | 2691 | `			}` |
|      ! 0 | 2692 | `		}` |
|        8 | 2693 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1100 | 2694 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1092 | 2695 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     1092 | 2696 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2697 | `				return rc;` |
|        - | 2698 | `			}` |
|      ! 0 | 2699 | `		}` |
|        - | 2700 | `	}` |
|        - | 2701 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        8 | 2702 | `	SyBlobReset(&pVm->sConsumer);` |
|        8 | 2703 | `	pVm->nOutputLen = 0;` |
|        8 | 2704 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        8 | 2705 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        8 | 2706 | `	pVm->iResponseStatus = 200;` |
|        8 | 2707 | `	pVm->bHeadersSent = 0;` |
|        8 | 2708 | `	pVm->bHttpContext = 0;` |
|        8 | 2709 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        8 | 2710 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        8 | 2711 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        8 | 2712 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        8 | 2713 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        8 | 2714 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 2715 | `#ifdef PH7_ENABLE_PCRE` |
|        8 | 2716 | `	pVm->iPcreLastError = 0;` |
|        - | 2717 | `#endif` |
|        - | 2718 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2719 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        8 | 2720 | `	PH7_LibxmlVmReset(&(*pVm));` |
|        - | 2721 | `#endif` |
|        8 | 2722 | `	pVm->iCmpCallbackExc = 0;` |
|        8 | 2723 | `	pVm->bHaltRequested = 0;` |
|        8 | 2724 | `	pVm->iExitStatus = 0;` |
|        8 | 2725 | `	pVm->nSpreadCallBase = 0;` |
|        8 | 2726 | `	VmSpreadCaptureReset(pVm);` |
|        8 | 2727 | `	pVm->nRecursionDepth = 0;` |
|        8 | 2728 | `	pVm->pActiveCtx = 0;` |
|        8 | 2729 | `	pVm->pCoalesceObj = 0;` |
|        8 | 2730 | `	pVm->bCoalesceArmed = 0;` |
|        8 | 2731 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - | 2732 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        8 | 2733 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2734 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|        - | 2735 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        8 | 2736 | `	pVm->nNextObjId = 1;` |
|        - | 2737 | `	/* Set the ready flag */` |
|        8 | 2738 | `	pVm->nMagic = PH7_VM_RUN;` |
|        8 | 2739 | `	return SXRET_OK;` |
|        4 | 2740 | `}` |
|        - | 2741 | `/*` |
|        - | 2742 | ` * Release a Virtual Machine.` |
|        - | 2743 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - | 2744 | ` */` |
|     3386 | 2745 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 | 2746 | `{` |
|        - | 2747 | `	/* Set the stale magic number */` |
|     3391 | 2748 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - | 2749 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2750 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|        - | 2751 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|     3391 | 2752 | `	PH7_LibxmlVmRelease(pVm);` |
|        - | 2753 | `#endif` |
|        - | 2754 | `	/* Release the private memory subsystem */` |
|     3391 | 2755 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     3391 | 2756 | `	return SXRET_OK;` |
|        5 | 2757 | `}` |
|        - | 2758 | `/*` |
|        - | 2759 | ` * Initialize a foreign function call context.` |
|        - | 2760 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - | 2761 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - | 2762 | ` * functions.` |
|        - | 2763 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - | 2764 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - | 2765 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - | 2766 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - | 2767 | ` */` |
|   941478 | 2768 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|        - | 2769 | `	ph7_context *pOut,    /* Call Context */` |
|        - | 2770 | `	ph7_vm *pVm,          /* Target VM */` |
|        - | 2771 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - | 2772 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - | 2773 | `	sxi32 iFlags          /* Control flags */` |
|        - | 2774 | `	)` |
|        5 | 2775 | `{` |
|   941483 | 2776 | `	pOut->pFunc = pFunc;` |
|   941483 | 2777 | `	pOut->pVm   = pVm;` |
|   941483 | 2778 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   941483 | 2779 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - | 2780 | `	/* Assume a null return value */` |
|   941483 | 2781 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   941483 | 2782 | `	pOut->pRet = pRet;` |
|   941483 | 2783 | `	pOut->iFlags = iFlags;` |
|   941483 | 2784 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|   941483 | 2785 | `	return SXRET_OK;` |
|        5 | 2786 | `}` |
|        - | 2787 | `/*` |
|        - | 2788 | ` * Release a foreign function call context and cleanup the mess` |
|        - | 2789 | ` * left behind.` |
|        - | 2790 | ` */` |
|   941478 | 2791 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 | 2792 | `{` |
|        - | 2793 | `	sxu32 n;` |
|   941483 | 2794 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    11829 | 2795 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    68668 | 2796 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    56844 | 2797 | `			if( apObj[n] == 0 ){` |
|        - | 2798 | `				/* Already released */` |
|      688 | 2799 | `				continue;` |
|        - | 2800 | `			}` |
|    56161 | 2801 | `			PH7_MemObjRelease(apObj[n]);` |
|    56161 | 2802 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    28083 | 2803 | `		}` |
|    11829 | 2804 | `		SySetRelease(&pCtx->sVar);` |
|     5912 | 2805 | `	}` |
|   941483 | 2806 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - | 2807 | `		ph7_aux_data *aAux;` |
|        - | 2808 | `		void *pChunk;` |
|        - | 2809 | `		/* Automatic release of dynamically allocated chunk` |
|        - | 2810 | `		 * using [ph7_context_alloc_chunk()].` |
|        - | 2811 | `		 */` |
|      115 | 2812 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      341 | 2813 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      231 | 2814 | `			pChunk = aAux[n].pAuxData;` |
|        - | 2815 | `			/* Release the chunk */` |
|      231 | 2816 | `			if( pChunk ){` |
|      231 | 2817 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      113 | 2818 | `			}` |
|      118 | 2819 | `		}` |
|      115 | 2820 | `		SySetRelease(&pCtx->sChunk);` |
|       55 | 2821 | `	}` |
|   941483 | 2822 | `}` |
|        - | 2823 | `/*` |
|        - | 2824 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - | 2825 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - | 2826 | ` */` |
|      683 | 2827 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - | 2828 | `	ph7_context *pCtx, /* Call context */` |
|        - | 2829 | `	ph7_value *pValue  /* Release this value */` |
|        - | 2830 | `	)` |
|        5 | 2831 | `{` |
|      688 | 2832 | `	if( pValue == 0 ){` |
|        - | 2833 | `		/* NULL value is a harmless operation */` |
|      ! 0 | 2834 | `		return;` |
|        - | 2835 | `	}` |
|      688 | 2836 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      688 | 2837 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - | 2838 | `		sxu32 n;` |
|     1554 | 2839 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1554 | 2840 | `			if( apObj[n] == pValue ){` |
|      688 | 2841 | `				PH7_MemObjRelease(pValue);` |
|      688 | 2842 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - | 2843 | `				/* Mark as released */` |
|      688 | 2844 | `				apObj[n] = 0;` |
|      688 | 2845 | `				break;` |
|        - | 2846 | `			}` |
|      437 | 2847 | `		}` |
|      341 | 2848 | `	}` |
|      346 | 2849 | `}` |
|        - | 2850 | `/*` |
|        - | 2851 | ` * Pop and release as many memory object from the operand stack.` |
|        - | 2852 | ` */` |
|  5867222 | 2853 | `PH7_PRIVATE void VmPopOperand(` |
|        - | 2854 | `	ph7_value **ppTos, /* Operand stack */` |
|        - | 2855 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - | 2856 | `	)` |
|        5 | 2857 | `{` |
|  5867227 | 2858 | `	ph7_value *pTos = *ppTos;` |
| 12469107 | 2859 | `	while( nPop > 0 ){` |
|  6601885 | 2860 | `		PH7_MemObjRelease(pTos);` |
|  6601885 | 2861 | `		pTos--;` |
|  6601885 | 2862 | `		nPop--;` |
|        5 | 2863 | `	}` |
|        - | 2864 | `	/* Top of the stack */` |
|  5867227 | 2865 | `	*ppTos = pTos;` |
|  5867227 | 2866 | `}` |
|        - | 2867 | `/*` |
|        - | 2868 | ` * Reserve a memory object.` |
|        - | 2869 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 2870 | ` */` |
|  4392291 | 2871 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 | 2872 | `{` |
|  4392296 | 2873 | `	ph7_value *pObj = 0;` |
|        - | 2874 | `	VmSlot *pSlot;` |
|        - | 2875 | `	sxu32 nIdx;` |
|        - | 2876 | `	/* Check for a free slot */` |
|  4392296 | 2877 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  4392296 | 2878 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  4392296 | 2879 | `	if( pSlot ){` |
|  2154998 | 2880 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  2154998 | 2881 | `		nIdx = pSlot->nIdx;` |
|  1078126 | 2882 | `	}` |
|  4392296 | 2883 | `	if( pObj == 0 ){` |
|        - | 2884 | `		/* Reserve a new memory object */` |
|  2237303 | 2885 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2237303 | 2886 | `		if( pObj == 0 ){` |
|      ! 0 | 2887 | `			return 0;` |
|        - | 2888 | `		}` |
|  1118649 | 2889 | `	}` |
|        - | 2890 | `	/* Set a null default value */` |
|  4392296 | 2891 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  4392296 | 2892 | `	pObj->nIdx = nIdx;` |
|  4392296 | 2893 | `	return pObj;` |
|  2196780 | 2894 | `}` |
|        - | 2895 | `/*` |
|        - | 2896 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - | 2897 | ` */` |
|    46088 | 2898 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|        - | 2899 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - | 2900 | `	const char *zKey,  /* Entry key */` |
|        - | 2901 | `	sxu32 nByte,       /* Key length */` |
|        - | 2902 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - | 2903 | `	)` |
|        5 | 2904 | `{` |
|        - | 2905 | `	ph7_value sKey;` |
|        - | 2906 | `	sxi32 rc;` |
|    46093 | 2907 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    46093 | 2908 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - | 2909 | `	/* Perform the insertion */` |
|    46093 | 2910 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    46093 | 2911 | `	PH7_MemObjRelease(&sKey);` |
|    46093 | 2912 | `	return rc;` |
|        5 | 2913 | `}` |
|        - | 2914 | `/*` |
|        - | 2915 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|        - | 2916 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|        - | 2917 | ` * key must create a real global variable — linked into the bottom frame's` |
|        - | 2918 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|        - | 2919 | ` * variable created by top-level code — so later reads and writes alias one` |
|        - | 2920 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|        - | 2921 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|        - | 2922 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|        - | 2923 | ` *     superglobal in place.` |
|        - | 2924 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|        - | 2925 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING` |
|        - | 2926 | ` *     name is rejected with the engine's usual "already exists" diagnostic` |
|        - | 2927 | ` *     (the same limitation OP_STORE_REF has for plain variables).` |
|        - | 2928 | ` */` |
|      148 | 2929 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|        2 | 2930 | `{` |
|      150 | 2931 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 2932 | `	SyHashEntry *pEntry;` |
|        - | 2933 | `	ph7_value *pObj;` |
|        - | 2934 | `	char *zDup;` |
|        - | 2935 | `	sxu32 nIdx;` |
|        - | 2936 | `	sxi32 rc;` |
|        - | 2937 | `	/* Walk down to the global frame */` |
|      154 | 2938 | `	while( pFrame->pParent ){` |
|        5 | 2939 | `		pFrame = pFrame->pParent;` |
|        1 | 2940 | `	}` |
|        - | 2941 | `	/* An existing global (or superglobal) is overwritten in place */` |
|      150 | 2942 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      150 | 2943 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|        - | 2944 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|        - | 2945 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|        - | 2946 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|        - | 2947 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|        - | 2948 | `		 * of $GLOBALS itself). */` |
|        5 | 2949 | `		pEntry = 0;` |
|        2 | 2950 | `	}` |
|      150 | 2951 | `	if( pEntry == 0 ){` |
|      150 | 2952 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       74 | 2953 | `	}` |
|      150 | 2954 | `	if( pEntry ){` |
|        3 | 2955 | `		if( nRefIdx != SXU32_HIGH ){` |
|        - | 2956 | `			SyString sName;` |
|      ! 0 | 2957 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|      ! 0 | 2958 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 | 2959 | `			return SXRET_OK;` |
|        - | 2960 | `		}` |
|        3 | 2961 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|        3 | 2962 | `		if( pObj == 0 ){` |
|      ! 0 | 2963 | `			return SXERR_NOTFOUND;` |
|        - | 2964 | `		}` |
|        3 | 2965 | `		if( pValue ){` |
|        3 | 2966 | `			PH7_MemObjStore(pValue,pObj);` |
|        2 | 2967 | `		}else{` |
|      ! 0 | 2968 | `			PH7_MemObjToNull(pObj);` |
|        - | 2969 | `		}` |
|        3 | 2970 | `		return SXRET_OK;` |
|        - | 2971 | `	}` |
|      148 | 2972 | `	if( nRefIdx == SXU32_HIGH ){` |
|        - | 2973 | `		/* Reserve a fresh slot for the new global */` |
|      146 | 2974 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|      146 | 2975 | `		if( pObj == 0 ){` |
|      ! 0 | 2976 | `			return SXERR_MEM;` |
|        - | 2977 | `		}` |
|      146 | 2978 | `		nIdx = pObj->nIdx;` |
|       74 | 2979 | `	}else{` |
|        - | 2980 | `		/* Reference assignment: bind the name to the existing slot */` |
|        3 | 2981 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|        3 | 2982 | `		if( pObj == 0 ){` |
|      ! 0 | 2983 | `			return SXERR_NOTFOUND;` |
|        - | 2984 | `		}` |
|        3 | 2985 | `		nIdx = nRefIdx;` |
|        - | 2986 | `	}` |
|      148 | 2987 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|      148 | 2988 | `	if( zDup == 0 ){` |
|      ! 0 | 2989 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2990 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|        - | 2991 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|        - | 2992 | `			VmSlot sFree;` |
|      ! 0 | 2993 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 2994 | `			sFree.pUserData = 0;` |
|      ! 0 | 2995 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 2996 | `		}` |
|      ! 0 | 2997 | `		return SXERR_MEM;` |
|        - | 2998 | `	}` |
|      148 | 2999 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|      148 | 3000 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3001 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 3002 | `			VmSlot sFree;` |
|      ! 0 | 3003 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 3004 | `			sFree.pUserData = 0;` |
|      ! 0 | 3005 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 3006 | `		}` |
|      ! 0 | 3007 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 3008 | `		return rc;` |
|        - | 3009 | `	}` |
|        - | 3010 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|      148 | 3011 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|      148 | 3012 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      148 | 3013 | `	if( nRefIdx == SXU32_HIGH ){` |
|      146 | 3014 | `		pObj->nIdx = nIdx;` |
|      146 | 3015 | `		if( pValue ){` |
|      146 | 3016 | `			PH7_MemObjStore(pValue,pObj);` |
|       72 | 3017 | `		}` |
|       72 | 3018 | `	}` |
|      148 | 3019 | `	return SXRET_OK;` |
|       76 | 3020 | `}` |
|        - | 3021 | `/*` |
|        - | 3022 | ` * Extract a variable value from the top active VM frame.` |
|        - | 3023 | ` * Return a pointer to the variable value on success.` |
|        - | 3024 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - | 3025 | ` */` |
|  5970422 | 3026 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|        - | 3027 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 3028 | `	const SyString *pName, /* Variable name */` |
|        - | 3029 | `	int bDup,              /* True to duplicate variable name */` |
|        - | 3030 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - | 3031 | `	)` |
|        5 | 3032 | `{` |
|  5970427 | 3033 | `	int bNullify = FALSE;` |
|        - | 3034 | `	SyHashEntry *pEntry;` |
|        - | 3035 | `	VmFrame *pFrame;` |
|        - | 3036 | `	ph7_value *pObj;` |
|        - | 3037 | `	sxu32 nIdx;` |
|        - | 3038 | `	sxi32 rc;` |
|        - | 3039 | `	/* Point to the top active frame */` |
|  5970427 | 3040 | `	pFrame = pVm->pFrame;` |
|  5970427 | 3041 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 3042 | `	/* Perform the lookup */` |
|  5970427 | 3043 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - | 3044 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 | 3045 | `		pName = &sAnnon;` |
|        - | 3046 | `		/* Always nullify the object */` |
|      ! 0 | 3047 | `		bNullify = TRUE;` |
|      ! 0 | 3048 | `		bDup = FALSE;` |
|      ! 0 | 3049 | `	}` |
|        - | 3050 | `	/* Check the superglobals table first */` |
|  5970427 | 3051 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  5970427 | 3052 | `	if( pEntry == 0 ){` |
|        - | 3053 | `		/* Query the top active frame */` |
|  5970011 | 3054 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  5970011 | 3055 | `		if( pEntry == 0 ){` |
|   519971 | 3056 | `			char *zName = (char *)pName->zString;` |
|        - | 3057 | `			VmSlot sLocal;` |
|   519971 | 3058 | `			if( !bCreate ){` |
|        - | 3059 | `				/* Do not create the variable,return NULL instead */` |
|     1193 | 3060 | `				return 0;` |
|        - | 3061 | `			}` |
|        - | 3062 | `			/* No such variable,automatically create a new one and install` |
|        - | 3063 | `			 * it in the current frame.` |
|        - | 3064 | `			 */` |
|   518783 | 3065 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   518783 | 3066 | `			if( pObj == 0 ){` |
|      ! 0 | 3067 | `				return 0;` |
|        - | 3068 | `			}` |
|   518783 | 3069 | `			nIdx = pObj->nIdx;` |
|   518783 | 3070 | `			if( bDup ){` |
|        - | 3071 | `				/* Duplicate name */` |
|      541 | 3072 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      541 | 3073 | `				if( zName == 0 ){` |
|      ! 0 | 3074 | `					return 0;` |
|        - | 3075 | `				}` |
|      268 | 3076 | `			}` |
|        - | 3077 | `			/* Link to the top active VM frame */` |
|   518783 | 3078 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   518783 | 3079 | `			if( rc != SXRET_OK ){` |
|        - | 3080 | `				/* Return the slot to the free pool */` |
|      ! 0 | 3081 | `				sLocal.nIdx = nIdx;` |
|      ! 0 | 3082 | `				sLocal.pUserData = 0;` |
|      ! 0 | 3083 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 | 3084 | `				return 0;` |
|        - | 3085 | `			}` |
|   518783 | 3086 | `			if( pFrame->pParent != 0 ){` |
|        - | 3087 | `				/* Local variable */` |
|   510197 | 3088 | `				sLocal.nIdx = nIdx;` |
|   510197 | 3089 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   255731 | 3090 | `			}else{` |
|        - | 3091 | `				/* Register in the $GLOBALS array */` |
|     8591 | 3092 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - | 3093 | `			}` |
|        - | 3094 | `			/* Install in the reference table */` |
|   518783 | 3095 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - | 3096 | `			/* Save object index */` |
|   518783 | 3097 | `			pObj->nIdx = nIdx;` |
|   260024 | 3098 | `		}else{` |
|        - | 3099 | `			/* Extract variable contents */` |
|  5450045 | 3100 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5450045 | 3101 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  5450045 | 3102 | `			if( bNullify && pObj ){` |
|      ! 0 | 3103 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 | 3104 | `			}` |
|        - | 3105 | `		}` |
|  2987157 | 3106 | `	}else{` |
|        - | 3107 | `		/* Superglobal */` |
|      421 | 3108 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      421 | 3109 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 3110 | `	}` |
|  5969239 | 3111 | `	return pObj;` |
|  2987959 | 3112 | `}` |
|        - | 3113 | `/*` |
|        - | 3114 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - | 3115 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - | 3116 | ` */` |
|    30882 | 3117 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - | 3118 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3119 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - | 3120 | `	sxu32 nByte        /* zName length */` |
|        - | 3121 | `	)` |
|        5 | 3122 | `{` |
|        - | 3123 | `	SyHashEntry *pEntry;` |
|        - | 3124 | `	ph7_value *pValue;` |
|        - | 3125 | `	sxu32 nIdx;` |
|        - | 3126 | `	/* Query the superglobal table */` |
|    30887 | 3127 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    30887 | 3128 | `	if( pEntry == 0 ){` |
|        - | 3129 | `		/* No such entry */` |
|      ! 0 | 3130 | `		return 0;` |
|        - | 3131 | `	}` |
|        - | 3132 | `	/* Extract the superglobal index in the global object pool */` |
|    30887 | 3133 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3134 | `	/* Extract the variable value  */` |
|    30887 | 3135 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    30887 | 3136 | `	return pValue;` |
|    15446 | 3137 | `}` |
|        - | 3138 | `/*` |
|        - | 3139 | ` * Perform a raw hashmap insertion.` |
|        - | 3140 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - | 3141 | ` */` |
|    24196 | 3142 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - | 3143 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - | 3144 | `	const char *zKey,   /* Entry key */` |
|        - | 3145 | `	int nKeylen,        /* zKey length*/` |
|        - | 3146 | `	const char *zData,  /* Entry data */` |
|        - | 3147 | `	int nLen            /* zData length */` |
|        - | 3148 | `	)` |
|        5 | 3149 | `{` |
|        - | 3150 | `	ph7_value sKey,sValue;` |
|        - | 3151 | `	sxi32 rc;` |
|    24201 | 3152 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    24201 | 3153 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|    24201 | 3154 | `	if( zKey ){` |
|    20809 | 3155 | `		if( nKeylen < 0 ){` |
|    20705 | 3156 | `			nKeylen = (int)SyStrlen(zKey);` |
|    10350 | 3157 | `		}` |
|    20809 | 3158 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|    10402 | 3159 | `	}` |
|    24201 | 3160 | `	if( zData ){` |
|    24201 | 3161 | `		if( nLen < 0 ){` |
|        - | 3162 | `			/* Compute length automatically */` |
|    13731 | 3163 | `			nLen = (int)SyStrlen(zData);` |
|     6863 | 3164 | `		}` |
|    24201 | 3165 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|    12098 | 3166 | `	}` |
|        - | 3167 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|        - | 3168 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|        - | 3169 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|        - | 3170 | `	 * every argument under "". */` |
|    24201 | 3171 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|    24201 | 3172 | `	PH7_MemObjRelease(&sKey);` |
|    24201 | 3173 | `	PH7_MemObjRelease(&sValue);` |
|    24201 | 3174 | `	return rc;` |
|        5 | 3175 | `}` |
|        - | 3176 | `/*` |
|        - | 3177 | ` * Configure a working virtual machine instance.` |
|        - | 3178 | ` *` |
|        - | 3179 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - | 3180 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - | 3181 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - | 3182 | ` * The second argument to this function is an integer configuration option` |
|        - | 3183 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - | 3184 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - | 3185 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - | 3186 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - | 3187 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - | 3188 | ` */` |
|    88666 | 3189 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - | 3190 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 3191 | `	sxi32 nOp,   /* Configuration verb */` |
|        - | 3192 | `	va_list ap   /* Subsequent option arguments */` |
|        - | 3193 | `	)` |
|        5 | 3194 | `{` |
|    88671 | 3195 | `	sxi32 rc = SXRET_OK;` |
|    88671 | 3196 | `	switch(nOp){` |
|     1680 | 3197 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     3365 | 3198 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     3365 | 3199 | `		void *pUserData = va_arg(ap,void *);` |
|        - | 3200 | `		/* VM output consumer callback */` |
|        - | 3201 | `#ifdef UNTRUST` |
|        - | 3202 | `		if( xConsumer == 0 ){` |
|        - | 3203 | `			rc = SXERR_CORRUPT;` |
|        - | 3204 | `			break;` |
|        - | 3205 | `		}` |
|        - | 3206 | `#endif` |
|        - | 3207 | `		/* Install the output consumer */` |
|     3365 | 3208 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     3365 | 3209 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     3365 | 3210 | `		break;` |
|        - | 3211 | `							   }` |
|     1693 | 3212 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - | 3213 | `		/* Import path */` |
|        - | 3214 | `		  const char *zPath;` |
|        - | 3215 | `		  SyString sPath;` |
|     3391 | 3216 | `		  zPath = va_arg(ap,const char *);` |
|        - | 3217 | `#if defined(UNTRUST)` |
|        - | 3218 | `		  if( zPath == 0 ){` |
|        - | 3219 | `			  rc = SXERR_EMPTY;` |
|        - | 3220 | `			  break;` |
|        - | 3221 | `		  }` |
|        - | 3222 | `#endif` |
|     3391 | 3223 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - | 3224 | `		  /* Remove trailing slashes and backslashes */` |
|        - | 3225 | `#ifdef __WINNT__` |
|        5 | 3226 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - | 3227 | `#endif` |
|     6777 | 3228 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - | 3229 | `		  /* Remove leading and trailing white spaces */` |
|     3391 | 3230 | `		  SyStringFullTrim(&sPath);` |
|     3391 | 3231 | `		  if( sPath.nByte > 0 ){` |
|        - | 3232 | `			  /* Store the path in the corresponding conatiner */` |
|     3391 | 3233 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1693 | 3234 | `		  }` |
|     3391 | 3235 | `		  break;` |
|        - | 3236 | `									 }` |
|     1696 | 3237 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - | 3238 | `		/* Run-Time Error report */` |
|     3397 | 3239 | `		pVm->bErrReport = 1;` |
|     3397 | 3240 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|     3397 | 3241 | `		break;` |
|        2 | 3242 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - | 3243 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|        - | 3244 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|        - | 3245 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|        - | 3246 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|        - | 3247 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|        - | 3248 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|        - | 3249 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|        - | 3250 | `		 * would otherwise read as an enormous positive cap). */` |
|        5 | 3251 | `		int nDepth = va_arg(ap,int);` |
|        5 | 3252 | `		if( nDepth >= 0 ){` |
|        5 | 3253 | `			pVm->nMaxDepth = nDepth;` |
|        2 | 3254 | `		}` |
|        5 | 3255 | `		break;` |
|        - | 3256 | `									   }` |
|        5 | 3257 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|        - | 3258 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|        - | 3259 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|        - | 3260 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|        - | 3261 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|        - | 3262 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|        - | 3263 | `		 * so it is rejected as a footgun). */` |
|       12 | 3264 | `		int nDepth = va_arg(ap,int);` |
|       12 | 3265 | `		if( nDepth > 1 ){` |
|       12 | 3266 | `			pVm->nMaxNativeDepth = nDepth;` |
|        5 | 3267 | `		}` |
|       12 | 3268 | `		break;` |
|        - | 3269 | `									   }` |
|      ! 0 | 3270 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - | 3271 | `		/* VM output length in bytes */` |
|      ! 0 | 3272 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - | 3273 | `#ifdef UNTRUST` |
|        - | 3274 | `		if( pOut == 0 ){` |
|        - | 3275 | `			rc = SXERR_CORRUPT;` |
|        - | 3276 | `			break;` |
|        - | 3277 | `		}` |
|        - | 3278 | `#endif` |
|      ! 0 | 3279 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 | 3280 | `		break;` |
|        - | 3281 | `							   }` |
|        - | 3282 |  |
|    18660 | 3283 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - | 3284 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - | 3285 | `		/* Create a new superglobal/global variable */` |
|    37325 | 3286 | `		const char *zName = va_arg(ap,const char *);` |
|    37325 | 3287 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - | 3288 | `		SyHashEntry *pEntry;` |
|        - | 3289 | `		ph7_value *pObj;` |
|        - | 3290 | `		sxu32 nByte;` |
|        - | 3291 | `		sxu32 nIdx;` |
|        - | 3292 | `#ifdef UNTRUST` |
|        - | 3293 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - | 3294 | `			rc = SXERR_CORRUPT;` |
|        - | 3295 | `			break;` |
|        - | 3296 | `		}` |
|        - | 3297 | `#endif` |
|    37325 | 3298 | `		nByte = SyStrlen(zName);` |
|    37325 | 3299 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3300 | `			/* Check if the superglobal is already installed */` |
|    33965 | 3301 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    16985 | 3302 | `		}else{` |
|        - | 3303 | `			/* Query the top active VM frame */` |
|     3365 | 3304 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - | 3305 | `		}` |
|    37325 | 3306 | `		if( pEntry ){` |
|        - | 3307 | `			/* Variable already installed */` |
|      ! 0 | 3308 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3309 | `			/* Extract contents */` |
|      ! 0 | 3310 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 | 3311 | `			if( pObj ){` |
|        - | 3312 | `				/* Overwrite old contents */` |
|      ! 0 | 3313 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 | 3314 | `			}` |
|      ! 0 | 3315 | `		}else{` |
|        - | 3316 | `			/* Install a new variable */` |
|    37325 | 3317 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    37325 | 3318 | `			if( pObj == 0 ){` |
|      ! 0 | 3319 | `				rc = SXERR_MEM;` |
|      ! 0 | 3320 | `				break;` |
|        - | 3321 | `			}` |
|    37325 | 3322 | `			nIdx = pObj->nIdx;` |
|        - | 3323 | `			/* Copy value */` |
|    37325 | 3324 | `			PH7_MemObjStore(pValue,pObj);` |
|    37325 | 3325 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3326 | `				/* Install the superglobal */` |
|    33965 | 3327 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    16985 | 3328 | `			}else{` |
|        - | 3329 | `				/* Install in the current frame */` |
|     3365 | 3330 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - | 3331 | `			}` |
|    37325 | 3332 | `			if( rc == SXRET_OK ){` |
|        - | 3333 | `				SyHashEntry *pRef;` |
|    37325 | 3334 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    33965 | 3335 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    16985 | 3336 | `				}else{` |
|     3365 | 3337 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - | 3338 | `				}` |
|        - | 3339 | `				/* Install in the reference table */` |
|    37325 | 3340 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    37325 | 3341 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - | 3342 | `					/* Register in the $GLOBALS array */` |
|    37325 | 3343 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    18660 | 3344 | `				}` |
|    18660 | 3345 | `			}` |
|        - | 3346 | `		}` |
|    37325 | 3347 | `		break;` |
|        - | 3348 | `									}` |
|    10350 | 3349 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - | 3350 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - | 3351 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - | 3352 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - | 3353 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - | 3354 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - | 3355 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|    20705 | 3356 | `		const char *zKey   = va_arg(ap,const char *);` |
|    20705 | 3357 | `		const char *zValue = va_arg(ap,const char *);` |
|    20705 | 3358 | `		int nLen = va_arg(ap,int);` |
|        - | 3359 | `		ph7_hashmap *pMap;` |
|        - | 3360 | `		ph7_value *pValue;` |
|    20705 | 3361 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - | 3362 | `			/* Extract the $_ENV superglobal */` |
|        3 | 3363 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|    20704 | 3364 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - | 3365 | `			/* Extract the $_POST superglobal */` |
|      ! 0 | 3366 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|    20703 | 3367 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - | 3368 | `			/* Extract the $_GET superglobal */` |
|      ! 0 | 3369 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    20703 | 3370 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - | 3371 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 | 3372 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    20703 | 3373 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - | 3374 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 | 3375 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|    20703 | 3376 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - | 3377 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 | 3378 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 | 3379 | `		}else{` |
|        - | 3380 | `			/* Extract the $_SERVER superglobal */` |
|    20703 | 3381 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - | 3382 | `		}` |
|    20705 | 3383 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3384 | `			/* No such entry */` |
|      ! 0 | 3385 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3386 | `			break;` |
|        - | 3387 | `		}` |
|        - | 3388 | `		/* Point to the hashmap */` |
|    20705 | 3389 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3390 | `		/* Perform the insertion */` |
|    20705 | 3391 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|    20705 | 3392 | `		break;` |
|        - | 3393 | `								   }` |
|     1697 | 3394 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - | 3395 | `		/* Script arguments */` |
|     3399 | 3396 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3397 | `		ph7_hashmap *pMap;` |
|        - | 3398 | `		ph7_value *pValue;` |
|        - | 3399 | `		sxu32 n;` |
|     3399 | 3400 | `		if( SX_EMPTY_STR(zValue) ){` |
|        2 | 3401 | `			rc = SXERR_EMPTY;` |
|        2 | 3402 | `			break;` |
|        - | 3403 | `		}` |
|        - | 3404 | `		/* Extract the $argv array */` |
|     3397 | 3405 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3397 | 3406 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3407 | `			/* No such entry */` |
|      ! 0 | 3408 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3409 | `			break;` |
|        - | 3410 | `		}` |
|        - | 3411 | `		/* Point to the hashmap */` |
|     3397 | 3412 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3413 | `		/* Perform the insertion */` |
|     3397 | 3414 | `		n = (sxu32)SyStrlen(zValue);` |
|     3397 | 3415 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|     3397 | 3416 | `		if( rc == SXRET_OK ){` |
|     3397 | 3417 | `			if( pMap->nEntry > 1 ){` |
|        - | 3418 | `				/* Append space separator first */` |
|       37 | 3419 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|       16 | 3420 | `			}` |
|     3397 | 3421 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|     1696 | 3422 | `		}` |
|     3397 | 3423 | `		break;` |
|        - | 3424 | `								  }` |
|     1680 | 3425 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|        - | 3426 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|        - | 3427 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|        - | 3428 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|        - | 3429 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|        - | 3430 | `		ph7_value *pArgv,*pServer;` |
|        - | 3431 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|        - | 3432 | `		ph7_value sArgvVal,sKey,sCount;` |
|     3365 | 3433 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3365 | 3434 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|     3360 | 3435 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|     3365 | 3436 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 3437 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3438 | `			break;` |
|        - | 3439 | `		}` |
|     3365 | 3440 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|     3365 | 3441 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|        - | 3442 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|     3365 | 3443 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|     3365 | 3444 | `		if( pDup == 0 ){` |
|      ! 0 | 3445 | `			rc = SXERR_MEM;` |
|      ! 0 | 3446 | `			break;` |
|        - | 3447 | `		}` |
|     3365 | 3448 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|     3365 | 3449 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|     3365 | 3450 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3365 | 3451 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|     3365 | 3452 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|     3365 | 3453 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|     3365 | 3454 | `		PH7_MemObjRelease(&sKey);` |
|        - | 3455 | `		/* $_SERVER['argc'] = count($argv). */` |
|     3365 | 3456 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|     3365 | 3457 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3365 | 3458 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|     3365 | 3459 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|     3365 | 3460 | `		PH7_MemObjRelease(&sCount);` |
|     3365 | 3461 | `		PH7_MemObjRelease(&sKey);` |
|     3365 | 3462 | `		rc = SXRET_OK;` |
|     3365 | 3463 | `		break;` |
|        - | 3464 | `								  }` |
|       29 | 3465 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|        - | 3466 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|        - | 3467 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|        - | 3468 | `		 * apply immediately so they take effect even if the script never` |
|        - | 3469 | `		 * touches the INI API. */` |
|       62 | 3470 | `		const char *zName = va_arg(ap,const char *);` |
|       62 | 3471 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3472 | `		VmIniEntry sEntry;` |
|        - | 3473 | `		char *zDupN,*zDupV;` |
|        - | 3474 | `		sxu32 nName,nValue;` |
|       62 | 3475 | `		if( SX_EMPTY_STR(zName) ){` |
|      ! 0 | 3476 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3477 | `			break;` |
|        - | 3478 | `		}` |
|       62 | 3479 | `		if( zValue == 0 ){` |
|      ! 0 | 3480 | `			zValue = "";` |
|      ! 0 | 3481 | `		}` |
|       62 | 3482 | `		nName = (sxu32)SyStrlen(zName);` |
|       62 | 3483 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       62 | 3484 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       62 | 3485 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       62 | 3486 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|      ! 0 | 3487 | `			rc = SXERR_MEM;` |
|      ! 0 | 3488 | `			break;` |
|        - | 3489 | `		}` |
|       62 | 3490 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       62 | 3491 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       62 | 3492 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       62 | 3493 | `		if( rc == SXRET_OK ){` |
|       58 | 3494 | `			if( nName == sizeof("error_reporting")-1` |
|       52 | 3495 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|        2 | 3496 | `				sxi64 iLevel = 0;` |
|        2 | 3497 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|        2 | 3498 | `				pVm->bErrReport = iLevel != 0;` |
|       57 | 3499 | `			}else if( nName == sizeof("date.timezone")-1` |
|       28 | 3500 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|      ! 0 | 3501 | `			 && nValue == 3` |
|        4 | 3502 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|      ! 0 | 3503 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|      ! 0 | 3504 | `				pVm->zDefTz[3] = 0;` |
|      ! 0 | 3505 | `				pVm->nDefTz = 3;` |
|       56 | 3506 | `			}else if( nName == sizeof("zend.assertions")-1` |
|       50 | 3507 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|        - | 3508 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|        - | 3509 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|        - | 3510 | `				 * effect even before the INI chunk is seeded. */` |
|       40 | 3511 | `				sxi64 iZend = 0;` |
|       40 | 3512 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|       40 | 3513 | `				if( iZend >= 1 ){` |
|       40 | 3514 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|       22 | 3515 | `				}else{` |
|      ! 0 | 3516 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|        - | 3517 | `				}` |
|       18 | 3518 | `			}` |
|       29 | 3519 | `		}` |
|       62 | 3520 | `		break;` |
|        - | 3521 | `								  }` |
|      ! 0 | 3522 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - | 3523 | `		/* error_log() consumer */` |
|      ! 0 | 3524 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 | 3525 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 | 3526 | `		break;` |
|        - | 3527 | `										}` |
|      ! 0 | 3528 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - | 3529 | `		/* Script return value */` |
|      ! 0 | 3530 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - | 3531 | `#ifdef UNTRUST` |
|        - | 3532 | `		if( ppValue == 0 ){` |
|        - | 3533 | `			rc = SXERR_CORRUPT;` |
|        - | 3534 | `			break;` |
|        - | 3535 | `		}` |
|        - | 3536 | `#endif` |
|      ! 0 | 3537 | `		*ppValue = &pVm->sExec;` |
|      ! 0 | 3538 | `		break;` |
|        - | 3539 | `								   }` |
|     6777 | 3540 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - | 3541 | `		/* Register an IO stream device */` |
|    13559 | 3542 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - | 3543 | `		/* Make sure we are dealing with a valid IO stream */` |
|    13554 | 3544 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|    13559 | 3545 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - | 3546 | `				/* Invalid stream */` |
|      ! 0 | 3547 | `				rc = SXERR_INVALID;` |
|      ! 0 | 3548 | `				break;` |
|        - | 3549 | `		}` |
|    13559 | 3550 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - | 3551 | `			/* Make the 'file://' stream the defaut stream device */` |
|     3393 | 3552 | `			pVm->pDefStream = pStream;` |
|     1694 | 3553 | `		}` |
|        - | 3554 | `		/* Insert in the appropriate container */` |
|    13559 | 3555 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|    13559 | 3556 | `		break;` |
|        - | 3557 | `								  }` |
|       16 | 3558 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - | 3559 | `		/* Point to the VM internal output consumer buffer */` |
|       32 | 3560 | `		const void **ppOut = va_arg(ap,const void **);` |
|       32 | 3561 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - | 3562 | `#ifdef UNTRUST` |
|        - | 3563 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - | 3564 | `			rc = SXERR_CORRUPT;` |
|        - | 3565 | `			break;` |
|        - | 3566 | `		}` |
|        - | 3567 | `#endif` |
|       32 | 3568 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       32 | 3569 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       32 | 3570 | `		break;` |
|        - | 3571 | `									   }` |
|       16 | 3572 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - | 3573 | `		/* Raw HTTP request*/` |
|       32 | 3574 | `		const char *zRequest = va_arg(ap,const char *);` |
|       32 | 3575 | `		int nByte = va_arg(ap,int);` |
|       32 | 3576 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 | 3577 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3578 | `			break;` |
|        - | 3579 | `		}` |
|       32 | 3580 | `		if( nByte < 0 ){` |
|        - | 3581 | `			/* Compute length automatically */` |
|      ! 0 | 3582 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 | 3583 | `		}` |
|        - | 3584 | `		/* Process the request */` |
|       32 | 3585 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - | 3586 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       32 | 3587 | `		if( rc == SXRET_OK ){` |
|       30 | 3588 | `			pVm->bHttpContext = 1;` |
|       15 | 3589 | `		}` |
|       32 | 3590 | `		break;` |
|        - | 3591 | `									}` |
|       16 | 3592 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - | 3593 | `		/* Extract HTTP response status code */` |
|       32 | 3594 | `		int *pStatus = va_arg(ap, int *);` |
|       32 | 3595 | `		if( pStatus ){` |
|       32 | 3596 | `			*pStatus = pVm->iResponseStatus;` |
|       16 | 3597 | `		}` |
|       32 | 3598 | `		break;` |
|        - | 3599 | `										}` |
|       16 | 3600 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - | 3601 | `		/* Iterate response headers via callback */` |
|        - | 3602 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       32 | 3603 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       32 | 3604 | `		void *pUserData = va_arg(ap, void *);` |
|       32 | 3605 | `		if( xCallback ){` |
|       32 | 3606 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       32 | 3607 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       44 | 3608 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 | 3609 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 | 3610 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 | 3611 | `							   pUserData);` |
|       12 | 3612 | `				if( rc != PH7_OK ){` |
|      ! 0 | 3613 | `					break;` |
|        - | 3614 | `				}` |
|        6 | 3615 | `			}` |
|       16 | 3616 | `		}` |
|       32 | 3617 | `		break;` |
|        - | 3618 | `										 }` |
|      ! 0 | 3619 | `	default:` |
|        - | 3620 | `		/* Unknown configuration option */` |
|      ! 0 | 3621 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 | 3622 | `		break;` |
|        - | 3623 | `	}` |
|    88671 | 3624 | `	return rc;` |
|        5 | 3625 | `}` |
|        - | 3626 | `/* Forward declaration */` |
|        - | 3627 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - | 3628 | `/*` |
|        - | 3629 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - | 3630 | ` * format.` |
|        - | 3631 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - | 3632 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - | 3633 | ` * (STDOUT).` |
|        - | 3634 | ` */` |
|        2 | 3635 | `static sxi32 VmByteCodeDump(` |
|        - | 3636 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - | 3637 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - | 3638 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 3639 | `	)` |
|        1 | 3640 | `{` |
|        - | 3641 | `	static const char zDump[] = {` |
|        - | 3642 | `		"====================================================\n"` |
|        - | 3643 | `		"PH7 VM Dump\n"` |
|        - | 3644 | `		"====================================================\n"` |
|        - | 3645 | `	};` |
|        - | 3646 | `	VmInstr *pInstr,*pEnd;` |
|        3 | 3647 | `	sxi32 rc = SXRET_OK;` |
|        - | 3648 | `	sxu32 n;` |
|        - | 3649 | `	/* Point to the PH7 instructions */` |
|        3 | 3650 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 | 3651 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 | 3652 | `	n = 0;` |
|        3 | 3653 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - | 3654 | `	/* Dump instructions */` |
|        7 | 3655 | `	for(;;){` |
|       15 | 3656 | `		if( pInstr >= pEnd ){` |
|        - | 3657 | `			/* No more instructions */` |
|        3 | 3658 | `			break;` |
|        - | 3659 | `		}` |
|        - | 3660 | `		/* Format and call the consumer callback */` |
|       19 | 3661 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|       12 | 3662 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 | 3663 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|       13 | 3664 | `		if( rc != SXRET_OK ){` |
|        - | 3665 | `			/* Consumer routine request an operation abort */` |
|      ! 0 | 3666 | `			return rc;` |
|        - | 3667 | `		}` |
|       13 | 3668 | `		++n;` |
|       13 | 3669 | `		pInstr++; /* Next instruction in the stream */` |
|        1 | 3670 | `	}` |
|        3 | 3671 | `	return rc;` |
|        2 | 3672 | `}` |
|        - | 3673 | `/*` |
|        - | 3674 | ` * Save the execution state of a fiber/generator context.` |
|        - | 3675 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - | 3676 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - | 3677 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - | 3678 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - | 3679 | ` * when VmByteCodeExec returns.` |
|        - | 3680 | ` */` |
|     1646 | 3681 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|        - | 3682 | `	ph7_vm *pVm,` |
|        - | 3683 | `	ph7_exec_ctx *pCtx,` |
|        - | 3684 | `	sxi32 pc,` |
|        - | 3685 | `	sxi32 nTos` |
|        - | 3686 | `	)` |
|        5 | 3687 | `{` |
|      823 | 3688 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|     1651 | 3689 | `	pCtx->pc = pc;` |
|     1651 | 3690 | `	pCtx->nTos = nTos;` |
|     1651 | 3691 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|     1651 | 3692 | `	return PH7_SUSPEND;` |
|        5 | 3693 | `}` |
|        - | 3694 | `/*` |
|        - | 3695 | ` * Resolve named-argument mapping.` |
|        - | 3696 | ` *` |
|        - | 3697 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - | 3698 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - | 3699 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - | 3700 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - | 3701 | ` * every formal parameter that received a value.` |
|        - | 3702 | ` *` |
|        - | 3703 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - | 3704 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|        - | 3705 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|        - | 3706 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|        - | 3707 | ` */` |
|      276 | 3708 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|        - | 3709 | `	ph7_vm *pVm,` |
|        - | 3710 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - | 3711 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - | 3712 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - | 3713 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - | 3714 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - | 3715 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - | 3716 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - | 3717 | `)` |
|        4 | 3718 | `{` |
|      280 | 3719 | `	sxi32 posIdx = 0;` |
|        - | 3720 | `	sxu32 i;` |
|      280 | 3721 | `	int bSeenNamed = 0;` |
|        - | 3722 | `	char zErrMsg[256];` |
|      280 | 3723 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|     1056 | 3724 | `	for( i = 0; i < nActual; i++ ){` |
|      780 | 3725 | `		aSlot[i] = -2;` |
|      392 | 3726 | `	}` |
|     1044 | 3727 | `	for( i = 0; i < nActual; i++ ){` |
|      999 | 3728 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - | 3729 | `			/* Named argument — find formal by name */` |
|      454 | 3730 | `			int found = 0;` |
|      454 | 3731 | `			bSeenNamed = 1;` |
|        - | 3732 | `			sxu32 k;` |
|      704 | 3733 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      602 | 3734 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      585 | 3735 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      560 | 3736 | `						pMap->aNames[i].zString,` |
|      840 | 3737 | `						pMap->aNames[i].nByte) == 0 ){` |
|      356 | 3738 | `					if( aUsed[k] ){` |
|       12 | 3739 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3740 | `							"Named parameter $%.*s overwrites previous argument",` |
|        6 | 3741 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        9 | 3742 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3743 | `					}` |
|      349 | 3744 | `					aSlot[i] = (sxi32)k;` |
|      349 | 3745 | `					aUsed[k] = 1;` |
|      349 | 3746 | `					found = 1;` |
|      349 | 3747 | `					break;` |
|        - | 3748 | `				}` |
|      128 | 3749 | `			}` |
|      448 | 3750 | `			if( !found ){` |
|      101 | 3751 | `				if( iVariadicIdx >= 0 ){` |
|       93 | 3752 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|       47 | 3753 | `				}else{` |
|       11 | 3754 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3755 | `						"Unknown named parameter $%.*s",` |
|        6 | 3756 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        8 | 3757 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3758 | `				}` |
|       46 | 3759 | `			}` |
|      222 | 3760 | `		}else{` |
|        - | 3761 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|        - | 3762 | `			 * named arg (the parser rejects it at compile time), but a call` |
|        - | 3763 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|        - | 3764 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|      329 | 3765 | `			if( bSeenNamed ){` |
|      ! 0 | 3766 | `				return VmThrowNamedArgError(&(*pVm),` |
|        - | 3767 | `					"Cannot use positional argument after named argument",` |
|        - | 3768 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|        - | 3769 | `			}` |
|      329 | 3770 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       51 | 3771 | `				if( aUsed[posIdx] ){` |
|      ! 0 | 3772 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3773 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 | 3774 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 | 3775 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3776 | `				}` |
|       51 | 3777 | `				aSlot[i] = posIdx;` |
|       51 | 3778 | `				aUsed[posIdx] = 1;` |
|      303 | 3779 | `			}else if( iVariadicIdx >= 0 ){` |
|      279 | 3780 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      139 | 3781 | `			}` |
|      329 | 3782 | `			posIdx++;` |
|        - | 3783 | `		}` |
|      386 | 3784 | `	}` |
|      267 | 3785 | `	return SXRET_OK;` |
|      142 | 3786 | `}` |
|        - | 3787 | `/*` |
|        - | 3788 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - | 3789 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - | 3790 | ` */` |
|      322 | 3791 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        5 | 3792 | `{` |
|      327 | 3793 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      314 | 3794 | `		return 0;` |
|        - | 3795 | `	}` |
|       15 | 3796 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|      166 | 3797 | `}` |
|        - | 3798 | `/*` |
|        - | 3799 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - | 3800 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - | 3801 | ` * preserved (later wins), integer keys are renumbered.` |
|        - | 3802 | ` */` |
|       10 | 3803 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3804 | `{` |
|       11 | 3805 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 | 3806 | `	(void)pVm;` |
|       11 | 3807 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 | 3808 | `	return SXRET_OK;` |
|        1 | 3809 | `}` |
|        - | 3810 | `/*` |
|        - | 3811 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - | 3812 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - | 3813 | ` */` |
|        6 | 3814 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3815 | `{` |
|        3 | 3816 | `	(void)pVm; (void)pKey;` |
|        7 | 3817 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 | 3818 | `	return SXRET_OK;` |
|        1 | 3819 | `}` |
|        - | 3820 | `/*` |
|        - | 3821 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|        - | 3822 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|        - | 3823 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|        - | 3824 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|        - | 3825 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|        - | 3826 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|        - | 3827 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|        - | 3828 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|        - | 3829 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|        - | 3830 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|        - | 3831 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|        - | 3832 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|        - | 3833 | ` */` |
|        - | 3834 | `/*` |
|        - | 3835 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|        - | 3836 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|        - | 3837 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|        - | 3838 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|        - | 3839 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|        - | 3840 | ` */` |
|      300 | 3841 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|        4 | 3842 | `{` |
|        - | 3843 | `	VmSpreadRun sRun;` |
|        - | 3844 | `	ph7_hashmap_node *pNode;` |
|        - | 3845 | `	sxu32 i;` |
|      304 | 3846 | `	sRun.pStart = pFirst;` |
|      304 | 3847 | `	sRun.nCount = nCount;` |
|      304 | 3848 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      304 | 3849 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      304 | 3850 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|      ! 0 | 3851 | `		return;` |
|        - | 3852 | `	}` |
|      304 | 3853 | `	pNode = pMap->pFirst;` |
|     2098 | 3854 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|        - | 3855 | `		VmSpreadKey sKey;` |
|     1798 | 3856 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|        - | 3857 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|        - | 3858 | `			 * the source map's release before CALL replays them. */` |
|       95 | 3859 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       95 | 3860 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       95 | 3861 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|       48 | 3862 | `		}else{` |
|        - | 3863 | `			/* Integer key (or empty-string key, treated positionally) */` |
|     1704 | 3864 | `			sKey.nOff = 0;` |
|     1704 | 3865 | `			sKey.nLen = 0;` |
|        - | 3866 | `		}` |
|     1798 | 3867 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|     1798 | 3868 | `		pNode = pNode->pPrev; /* forward link */` |
|      901 | 3869 | `	}` |
|      154 | 3870 | `}` |
|        - | 3871 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|        - | 3872 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        8 | 3873 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|      ! 0 | 3874 | `{` |
|        8 | 3875 | `	SySetReset(&pVm->aSpreadRun);` |
|        8 | 3876 | `	SySetReset(&pVm->aSpreadKey);` |
|        8 | 3877 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        8 | 3878 | `}` |
|        - | 3879 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|        - | 3880 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|        - | 3881 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|        - | 3882 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|        - | 3883 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|        - | 3884 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|        - | 3885 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|        - | 3886 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|        - | 3887 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|        - | 3888 | ` * slot — from being consumed by that nested call. */` |
|      508 | 3889 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|        4 | 3890 | `{` |
|      512 | 3891 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      512 | 3892 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|        - | 3893 | `	VmSpreadRun *aRun;` |
|      512 | 3894 | `	if( rStart >= nRun ){` |
|      224 | 3895 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|        - | 3896 | `	}` |
|      292 | 3897 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      292 | 3898 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      292 | 3899 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      292 | 3900 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|      144 | 3901 | `	}` |
|      292 | 3902 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|      258 | 3903 | `}` |
|        - | 3904 | `/*` |
|        - | 3905 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|        - | 3906 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|        - | 3907 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|        - | 3908 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|        - | 3909 | ` *` |
|        - | 3910 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|        - | 3911 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|        - | 3912 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|        - | 3913 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|        - | 3914 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|        - | 3915 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|        - | 3916 | ` * they are counted only by that call. This replaces the old shared` |
|        - | 3917 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|        - | 3918 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|        - | 3919 | ` *` |
|        - | 3920 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|        - | 3921 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|        - | 3922 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|        - | 3923 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|        - | 3924 | ` */` |
|      316 | 3925 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|        4 | 3926 | `{` |
|      320 | 3927 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 3928 | `	VmSpreadRun *aRun;` |
|      320 | 3929 | `	ph7_value *pEnd = pTos;` |
|      320 | 3930 | `	sxi32 nPos = iP1;` |
|      320 | 3931 | `	sxi32 ri, extra = 0;` |
|      320 | 3932 | `	if( nRun == 0 ){` |
|       15 | 3933 | `		pVm->nSpreadCallBase = 0;` |
|       15 | 3934 | `		return 0;` |
|        - | 3935 | `	}` |
|      306 | 3936 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      306 | 3937 | `	ri = (sxi32)nRun - 1;` |
|      736 | 3938 | `	while( nPos > 0 ){` |
|      434 | 3939 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|        - | 3940 | `			/* A non-empty unpack occupying nCount slots. */` |
|      268 | 3941 | `			pEnd = aRun[ri].pStart;` |
|      268 | 3942 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      268 | 3943 | `			ri--;` |
|      302 | 3944 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|        - | 3945 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       39 | 3946 | `			extra -= 1;` |
|       39 | 3947 | `			ri--;` |
|       21 | 3948 | `		}else{` |
|        - | 3949 | `			/* An ordinary single-slot argument. */` |
|      134 | 3950 | `			pEnd--;` |
|        - | 3951 | `		}` |
|      434 | 3952 | `		nPos--;` |
|        4 | 3953 | `	}` |
|        - | 3954 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|        - | 3955 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      306 | 3956 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      306 | 3957 | `	return extra;` |
|      162 | 3958 | `}` |
|      300 | 3959 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)` |
|        4 | 3960 | `{` |
|      304 | 3961 | `	ph7_value *pTos = *ppTos;` |
|      304 | 3962 | `	sxu32 nEntry = pMap->nEntry;` |
|      304 | 3963 | `	if( nEntry == 0 ){` |
|        - | 3964 | `		/* Nothing to unpack — remove the source from the stack */` |
|       39 | 3965 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       39 | 3966 | `		VmPopOperand(&pTos, 1);` |
|       21 | 3967 | `	}else{` |
|        - | 3968 | `		ph7_hashmap_node *pNode;` |
|        - | 3969 | `		ph7_value *pElem;` |
|        - | 3970 | `		sxu32 i;` |
|        - | 3971 | `		int bTemp;` |
|      268 | 3972 | `		pMap->iRef++;` |
|      268 | 3973 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|        - | 3974 | `		/* Record the run + element keys before any release (nodes still alive).` |
|        - | 3975 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|      268 | 3976 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|        - | 3977 | `		/* Overwrite the source slot with the first element */` |
|      268 | 3978 | `		pNode = pMap->pFirst;` |
|      268 | 3979 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      268 | 3980 | `		PH7_MemObjRelease(pTos);` |
|      268 | 3981 | `		if( pElem ){` |
|      268 | 3982 | `			if( bTemp ){` |
|      135 | 3983 | `				PH7_MemObjStore(pElem, pTos);` |
|       68 | 3984 | `			}else{` |
|      134 | 3985 | `				PH7_MemObjLoad(pElem, pTos);` |
|        - | 3986 | `			}` |
|      132 | 3987 | `		}` |
|      268 | 3988 | `		pTos->nIdx = SXU32_HIGH;` |
|        - | 3989 | `		/* Traverse in insertion order (pPrev is the forward link` |
|        - | 3990 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|      268 | 3991 | `		pNode = pNode->pPrev;` |
|        - | 3992 | `		/* Push the remaining elements */` |
|     1798 | 3993 | `		for( i = 1; i < nEntry; i++ ){` |
|     1534 | 3994 | `			pTos++;` |
|     1534 | 3995 | `			PH7_MemObjInit(pVm, pTos);` |
|     1534 | 3996 | `			pTos->nIdx = SXU32_HIGH;` |
|     1534 | 3997 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|     1534 | 3998 | `			if( pElem ){` |
|     1534 | 3999 | `				if( bTemp ){` |
|     1259 | 4000 | `					PH7_MemObjStore(pElem, pTos);` |
|      630 | 4001 | `				}else{` |
|      276 | 4002 | `					PH7_MemObjLoad(pElem, pTos);` |
|        - | 4003 | `				}` |
|      765 | 4004 | `			}` |
|     1534 | 4005 | `			pNode = pNode->pPrev;` |
|      769 | 4006 | `		}` |
|      268 | 4007 | `		PH7_HashmapUnref(pMap);` |
|        - | 4008 | `	}` |
|      304 | 4009 | `	*ppTos = pTos;` |
|      304 | 4010 | `}` |
|        - | 4011 | `/*` |
|        - | 4012 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|        - | 4013 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|        - | 4014 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|        - | 4015 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|        - | 4016 | ` * element keys, interleaving them with the compile-time names at their real` |
|        - | 4017 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|        - | 4018 | ` *` |
|        - | 4019 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|        - | 4020 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|        - | 4021 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|        - | 4022 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|        - | 4023 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|        - | 4024 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|        - | 4025 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|        - | 4026 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|        - | 4027 | ` * method-name slot pop shifts pArg).` |
|        - | 4028 | ` *` |
|        - | 4029 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|        - | 4030 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|        - | 4031 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|        - | 4032 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|        - | 4033 | ` * which is after this call's synchronous named-arg resolution.` |
|        - | 4034 | ` */` |
|      276 | 4035 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|        - | 4036 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|        4 | 4037 | `{` |
|      280 | 4038 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 4039 | `	VmSpreadRun *aRun;` |
|        - | 4040 | `	VmSpreadKey *aKey;` |
|        - | 4041 | `	const char *zKeyBase;` |
|      280 | 4042 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|      280 | 4043 | `	int bAnyNamed = 0;` |
|        - | 4044 | `	sxu32 ai, ci, ri, rStart;` |
|      280 | 4045 | `	if( nRun == 0 ){` |
|        - | 4046 | `		/* No spread captured at all — the compile map is already aligned. */` |
|      ! 0 | 4047 | `		return 0;` |
|        - | 4048 | `	}` |
|      280 | 4049 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      280 | 4050 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      280 | 4051 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|        - | 4052 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|        - | 4053 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|        - | 4054 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|        - | 4055 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      280 | 4056 | `	ri = pVm->nSpreadCallBase;` |
|      280 | 4057 | `	rStart = ri;` |
|      280 | 4058 | `	if( rStart >= nRun ){` |
|        - | 4059 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|      ! 0 | 4060 | `		return 0;` |
|        - | 4061 | `	}` |
|      280 | 4062 | `	SySetReset(&pVm->aEffArgName);` |
|      280 | 4063 | `	ci = 0;` |
|      280 | 4064 | `	ai = 0;` |
|      654 | 4065 | `	while( ai < nActual ){` |
|        - | 4066 | `		SyString sName;` |
|      378 | 4067 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|        - | 4068 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|        - | 4069 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      390 | 4070 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|       13 | 4071 | `			ci++; ri++;` |
|        1 | 4072 | `		}` |
|      378 | 4073 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|        - | 4074 | `			/* A run of spread elements: one name per element from its key. Keys` |
|        - | 4075 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|        - | 4076 | `			 * run never desyncs the key stream. */` |
|      268 | 4077 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|     2062 | 4078 | `			for( j = 0; j < K; j++ ){` |
|     1798 | 4079 | `				SyZero(&sName, sizeof(sName));` |
|     1798 | 4080 | `				if( aKey[ks + j].nLen > 0 ){` |
|       95 | 4081 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       95 | 4082 | `					bAnyNamed = 1;` |
|       47 | 4083 | `				}` |
|     1798 | 4084 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      901 | 4085 | `			}` |
|      268 | 4086 | `			ai += K;` |
|      268 | 4087 | `			ci++; ri++;` |
|      136 | 4088 | `		}else{` |
|        - | 4089 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      114 | 4090 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|       33 | 4091 | `				sName = pCompile->aNames[ci];` |
|       33 | 4092 | `				bAnyNamed = 1;` |
|       16 | 4093 | `			}` |
|      114 | 4094 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      114 | 4095 | `			ai++;` |
|      114 | 4096 | `			ci++;` |
|        - | 4097 | `		}` |
|        4 | 4098 | `	}` |
|        - | 4099 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|        - | 4100 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|        - | 4101 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|        - | 4102 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      280 | 4103 | `	VmSpreadConsume(pVm);` |
|      280 | 4104 | `	if( !bAnyNamed ){` |
|        - | 4105 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      210 | 4106 | `		return 0;` |
|        - | 4107 | `	}` |
|       71 | 4108 | `	pEff->bHasNamed = 1;` |
|       71 | 4109 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|       71 | 4110 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|       71 | 4111 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|       71 | 4112 | `	pEff->nTotal = nActual;` |
|       71 | 4113 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|       71 | 4114 | `	return 1;` |
|      142 | 4115 | `}` |
|        - | 4116 | `/*` |
|        - | 4117 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|        - | 4118 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|        - | 4119 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|        - | 4120 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|        - | 4121 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|        - | 4122 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|        - | 4123 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|        - | 4124 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|        - | 4125 | ` * pArg must be the site's FINAL argument base.` |
|        - | 4126 | ` */` |
|  1053168 | 4127 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|        - | 4128 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|        5 | 4129 | `{` |
|  1053173 | 4130 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|  1053173 | 4131 | `	if( pInstr->iP2 == 0 ){` |
|  1052871 | 4132 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|        - | 4133 | `	}` |
|      306 | 4134 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|       71 | 4135 | `		return pStorage;` |
|        - | 4136 | `	}` |
|      236 | 4137 | `	VmSpreadConsume(pVm);` |
|      236 | 4138 | `	return pCompile;` |
|   527067 | 4139 | `}` |
|        - | 4140 | `/*` |
|        - | 4141 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|        - | 4142 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|        - | 4143 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — the two paths` |
|        - | 4144 | ` * disagree on which scalar types are silent (positional silences null+bool; keyed silences` |
|        - | 4145 | ` * only null, warning for bool to match PHP) — this only maps the type name and emits.` |
|        - | 4146 | ` */` |
|        6 | 4147 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|        2 | 4148 | `{` |
|        8 | 4149 | `	const char *zType = "unknown";` |
|        - | 4150 | `	char zMsg[64];` |
|        8 | 4151 | `	if( iFlags & MEMOBJ_STRING ){` |
|        6 | 4152 | `		zType = "string";` |
|        5 | 4153 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        - | 4154 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|        - | 4155 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|        - | 4156 | `		 * REAL flag, so it still falls through to the int arm. */` |
|      ! 0 | 4157 | `		zType = "float";` |
|        3 | 4158 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        3 | 4159 | `		zType = "int";` |
|        1 | 4160 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 4161 | `		zType = "bool";` |
|      ! 0 | 4162 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4163 | `		zType = "object";` |
|      ! 0 | 4164 | `	}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 | 4165 | `		zType = "resource";` |
|      ! 0 | 4166 | `	}` |
|        8 | 4167 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        8 | 4168 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        8 | 4169 | `}` |
|        - | 4170 | `/*` |
|        - | 4171 | ` * A member access in isset()/empty() context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY) is a silent` |
|        - | 4172 | ` * lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class instance"` |
|        - | 4173 | ` * warnings, mirroring the array isset/empty path.` |
|        - | 4174 | ` */` |
|     1102 | 4175 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|        5 | 4176 | `{` |
|     1107 | 4177 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY;` |
|        5 | 4178 | `}` |
|        - | 4179 | `/*` |
|        - | 4180 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|        - | 4181 | ` * A __get body reading the SAME property of the SAME instance must not` |
|        - | 4182 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|        - | 4183 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|        - | 4184 | ` * reads (other names / other instances) still dispatch.` |
|        - | 4185 | ` */` |
|     1060 | 4186 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4187 | `{` |
|        - | 4188 | `	VmMagicGuard *aG;` |
|        - | 4189 | `	sxu32 nHash;` |
|        - | 4190 | `	sxu32 n;` |
|     1061 | 4191 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|        - | 4192 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|        - | 4193 | `		 * every hooked-property access consults the guard, often twice. */` |
|      881 | 4194 | `		return FALSE;` |
|        - | 4195 | `	}` |
|      181 | 4196 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|      181 | 4197 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      225 | 4198 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|      181 | 4199 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|      137 | 4200 | `			return TRUE;` |
|        - | 4201 | `		}` |
|       23 | 4202 | `	}` |
|       45 | 4203 | `	return FALSE;` |
|      531 | 4204 | `}` |
|      486 | 4205 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4206 | `{` |
|        - | 4207 | `	VmMagicGuard sG;` |
|      487 | 4208 | `	sG.pThis = pThis;` |
|      487 | 4209 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      487 | 4210 | `	sG.cKind = cKind;` |
|      487 | 4211 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      487 | 4212 | `}` |
|      486 | 4213 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|        1 | 4214 | `{` |
|      487 | 4215 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      487 | 4216 | `}` |
|        - | 4217 | `/*` |
|        - | 4218 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|        - | 4219 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|        - | 4220 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|        - | 4221 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|        - | 4222 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|        - | 4223 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|        - | 4224 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|        - | 4225 | ` * One-token lookahead only.` |
|        - | 4226 | ` */` |
|      708 | 4227 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|        2 | 4228 | `{` |
|      710 | 4229 | `	switch( pNext->iOp ){` |
|       17 | 4230 | `		case PH7_OP_STORE:` |
|       36 | 4231 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|      ! 0 | 4232 | `		case PH7_OP_STORE_REF:` |
|      ! 0 | 4233 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|       11 | 4234 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|        - | 4235 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|        - | 4236 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|        - | 4237 | `		case PH7_OP_CAT_STORE:` |
|        - | 4238 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|        - | 4239 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       23 | 4240 | `			return 1;` |
|      326 | 4241 | `		default:` |
|      654 | 4242 | `			return 0;` |
|        - | 4243 | `	}` |
|      356 | 4244 | `}` |
|        - | 4245 | `/*` |
|        - | 4246 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|        - | 4247 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|        - | 4248 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|        - | 4249 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|        - | 4250 | ` */` |
|      424 | 4251 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|        1 | 4252 | `{` |
|      425 | 4253 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|        1 | 4254 | `}` |
|        - | 4255 | `/*` |
|        - | 4256 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|        - | 4257 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|        - | 4258 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|        - | 4259 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|        - | 4260 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|        - | 4261 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|        - | 4262 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|        - | 4263 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|        - | 4264 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|        - | 4265 | ` * abort path; SXRET_OK otherwise.` |
|        - | 4266 | ` */` |
|       54 | 4267 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|        1 | 4268 | `{` |
|        - | 4269 | `	char zHName[384];` |
|        - | 4270 | `	sxu32 nHName;` |
|        - | 4271 | `	ph7_class_method *pSetHook;` |
|       55 | 4272 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|        - | 4273 | `		/* get-only hooked property: php's read-only Error */` |
|        - | 4274 | `		SyBlob sErrMsg;` |
|        5 | 4275 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 4276 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|        4 | 4277 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|        5 | 4278 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        5 | 4279 | `		return SXRET_OK;` |
|        - | 4280 | `	}` |
|       51 | 4281 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 4282 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|      ! 0 | 4283 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|      ! 0 | 4284 | `		if( rcVis != SXRET_OK ){` |
|      ! 0 | 4285 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|      ! 0 | 4286 | `			return SXRET_OK;` |
|        - | 4287 | `		}` |
|      ! 0 | 4288 | `	}` |
|       51 | 4289 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|       51 | 4290 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|       51 | 4291 | `	if( pSetHook ){` |
|        - | 4292 | `		ph7_value sHookRet;` |
|        - | 4293 | `		ph7_value *apHArg[1];` |
|       51 | 4294 | `		apHArg[0] = pValue;` |
|       51 | 4295 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|       51 | 4296 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|       51 | 4297 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|       51 | 4298 | `		VmMagicGuardPop(pVm);` |
|       50 | 4299 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|       27 | 4300 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|        3 | 4301 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|        3 | 4302 | `			if( rcH == SXRET_OK ){` |
|        3 | 4303 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|        3 | 4304 | `				if( pBack ){` |
|        3 | 4305 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|        2 | 4306 | `				}` |
|        1 | 4307 | `			}else if( rcH == PH7_ABORT ){` |
|      ! 0 | 4308 | `				PH7_MemObjRelease(&sHookRet);` |
|      ! 0 | 4309 | `				return PH7_ABORT;` |
|        - | 4310 | `			}` |
|        - | 4311 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|        - | 4312 | `			 * the store is skipped, execution lands at the fetch point like any` |
|        - | 4313 | `			 * parked throw. */` |
|        1 | 4314 | `		}` |
|       51 | 4315 | `		PH7_MemObjRelease(&sHookRet);` |
|       25 | 4316 | `	}` |
|       51 | 4317 | `	return SXRET_OK;` |
|       28 | 4318 | `}` |
|        - | 4319 | `/*` |
|        - | 4320 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|        - | 4321 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|        - | 4322 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|        - | 4323 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|        - | 4324 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|        - | 4325 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|        - | 4326 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|        - | 4327 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|        - | 4328 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|        - | 4329 | ` */` |
|        - | 4330 | `/*` |
|        - | 4331 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|        - | 4332 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|        - | 4333 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|        - | 4334 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|        - | 4335 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|        - | 4336 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|        - | 4337 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|        - | 4338 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|        - | 4339 | ` * caller reads the raw slot then.` |
|        - | 4340 | ` */` |
|      382 | 4341 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|        4 | 4342 | `{` |
|      386 | 4343 | `	ph7_vm *pVm = pThis->pVm;` |
|        - | 4344 | `	char zHName[384];` |
|        - | 4345 | `	sxu32 nHName;` |
|        - | 4346 | `	ph7_class_method *pGetHook;` |
|        - | 4347 | `	sxi32 rc;` |
|      382 | 4348 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|      260 | 4349 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|      142 | 4350 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|        - | 4351 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|        - | 4352 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|        - | 4353 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|        - | 4354 | `		 * raw values whose output the routed throw then discards. */` |
|      248 | 4355 | `		return SXERR_NOTFOUND;` |
|        - | 4356 | `	}` |
|      139 | 4357 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|      139 | 4358 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|      139 | 4359 | `	if( pGetHook == 0 ){` |
|      ! 0 | 4360 | `		return SXERR_NOTFOUND;` |
|        - | 4361 | `	}` |
|      139 | 4362 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|      139 | 4363 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|      139 | 4364 | `	VmMagicGuardPop(pVm);` |
|      139 | 4365 | `	return rc;` |
|      195 | 4366 | `}` |
|        - | 4367 | `/*` |
|        - | 4368 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|        - | 4369 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|        - | 4370 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|        - | 4371 | ` */` |
|       20 | 4372 | `static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4373 | `{` |
|       21 | 4374 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 4375 | `	VmSlot sFree;` |
|       21 | 4376 | `	if( pScr ){` |
|       21 | 4377 | `		PH7_MemObjRelease(pScr);` |
|       10 | 4378 | `	}` |
|       21 | 4379 | `	sFree.nIdx = nIdx;` |
|       21 | 4380 | `	sFree.pUserData = 0;` |
|       21 | 4381 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       21 | 4382 | `}` |
|        - | 4383 | `/*` |
|        - | 4384 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|        - | 4385 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|        - | 4386 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|        - | 4387 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|        - | 4388 | ` * instance reference.` |
|        - | 4389 | ` */` |
|       16 | 4390 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|        2 | 4391 | `{` |
|       18 | 4392 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       18 | 4393 | `	if( pEnt == 0 ){` |
|        5 | 4394 | `		return;` |
|        - | 4395 | `	}` |
|       13 | 4396 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        5 | 4397 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|        2 | 4398 | `	}` |
|       13 | 4399 | `	SyBlobRelease(&pEnt->sName);` |
|       13 | 4400 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|       13 | 4401 | `	(void)SySetPop(&pVm->aHookRmw);` |
|       10 | 4402 | `}` |
|       16 | 4403 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4404 | `{` |
|        - | 4405 | `	VmHookRmw sEnt;` |
|        - | 4406 | `	VmHookRmw *pEnt;` |
|        - | 4407 | `	ph7_value *pScr;` |
|        - | 4408 | `	ph7_value sVal;` |
|       17 | 4409 | `	sxi32 rc = SXRET_OK;` |
|       17 | 4410 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       17 | 4411 | `	if( pEnt == 0 \|\| pEnt->iKind != VM_HOOK_PEND_RMW \|\| pEnt->nScratchIdx != nIdx ){` |
|      ! 0 | 4412 | `		return SXERR_NOTFOUND;` |
|        - | 4413 | `	}` |
|       17 | 4414 | `	sEnt = *pEnt;` |
|       17 | 4415 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        - | 4416 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|        - | 4417 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|        - | 4418 | `	 * scratch index past this point). */` |
|       17 | 4419 | `	PH7_MemObjInit(pVm,&sVal);` |
|       17 | 4420 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|       17 | 4421 | `	if( pScr ){` |
|       17 | 4422 | `		PH7_MemObjStore(pScr,&sVal);` |
|        8 | 4423 | `	}` |
|       17 | 4424 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|       17 | 4425 | `	sVal.nIdx = SXU32_HIGH;` |
|       17 | 4426 | `	if( pVm->nBoundaryRc == 0 ){` |
|       15 | 4427 | `		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|        7 | 4428 | `	}` |
|       17 | 4429 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 4430 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|       17 | 4431 | `	return rc;` |
|        9 | 4432 | `}` |
|        - | 4433 | `/*` |
|        - | 4434 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|        - | 4435 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|        - | 4436 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|        - | 4437 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|        - | 4438 | ` */` |
|       12 | 4439 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|        1 | 4440 | `{` |
|       13 | 4441 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       13 | 4442 | `	if( pSetMeth ){` |
|        - | 4443 | `		ph7_value sNameVal;` |
|        - | 4444 | `		ph7_value *apSetArg[2];` |
|       13 | 4445 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       13 | 4446 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       13 | 4447 | `		apSetArg[0] = &sNameVal;` |
|       13 | 4448 | `		apSetArg[1] = pValue;` |
|       13 | 4449 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       13 | 4450 | `		PH7_VmCallClassMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       13 | 4451 | `		VmMagicGuardPop(pVm);` |
|       13 | 4452 | `		PH7_MemObjRelease(&sNameVal);` |
|        6 | 4453 | `	}` |
|       13 | 4454 | `}` |
|        - | 4455 | `/*` |
|        - | 4456 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|        - | 4457 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|        - | 4458 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|        - | 4459 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|        - | 4460 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|        - | 4461 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|        - | 4462 | ` * path (the SyHash-layout incident class).` |
|        - | 4463 | ` */` |
|        - | 4464 | `/*` |
|        - | 4465 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|        - | 4466 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|        - | 4467 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|        - | 4468 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|        - | 4469 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|        - | 4470 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|        - | 4471 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|        - | 4472 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|        - | 4473 | ` * never linked (INIT error path).` |
|        - | 4474 | ` */` |
|    23914 | 4475 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|        5 | 4476 | `{` |
|    23919 | 4477 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|    23919 | 4478 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|        - | 4479 | `	sxu32 i;` |
|    26095 | 4480 | `	for( i = 0 ; i < n ; ++i ){` |
|    26095 | 4481 | `		if( apStep[i] == pStep ){` |
|    23927 | 4482 | `			for( ; i + 1 < n ; ++i ){` |
|        9 | 4483 | `				apStep[i] = apStep[i + 1];` |
|        5 | 4484 | `			}` |
|    23919 | 4485 | `			(void)SySetPop(&pInfo->aStep);` |
|    23919 | 4486 | `			return;` |
|        - | 4487 | `		}` |
|     1093 | 4488 | `	}` |
|    11962 | 4489 | `}` |
|      210 | 4490 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|        5 | 4491 | `{` |
|      215 | 4492 | `	if( pStep->pOwner ){` |
|       24 | 4493 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       11 | 4494 | `	}` |
|      215 | 4495 | `	VmForeachStepUnlink(pInfo,pStep);` |
|      215 | 4496 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      215 | 4497 | `	PH7_ClassInstanceUnref(pThis);` |
|      215 | 4498 | `}` |
|        - | 4499 | `/*` |
|        - | 4500 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|        - | 4501 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|        - | 4502 | ` * step stack, then drop the step's map reference. The single home for this` |
|        - | 4503 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|        - | 4504 | ` * load-bearing: a step freed while still registered is walked by the next` |
|        - | 4505 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|        - | 4506 | ` * class), and the unregister must precede the unref in case the step held the` |
|        - | 4507 | ` * map's last reference.` |
|        - | 4508 | ` */` |
|    23678 | 4509 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|        5 | 4510 | `{` |
|    23683 | 4511 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|    23683 | 4512 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|    23683 | 4513 | `	if( bPop ){` |
|        - | 4514 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|        - | 4515 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|    23683 | 4516 | `		VmForeachStepUnlink(pInfo,pStep);` |
|    11839 | 4517 | `	}` |
|    23683 | 4518 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    23683 | 4519 | `	PH7_HashmapUnref(pMap);` |
|    23683 | 4520 | `}` |
|        - | 4521 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|        - | 4522 | `/*` |
|        - | 4523 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 4524 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4525 | ` * See block-comment on that function for additional information.` |
|        - | 4526 | ` */` |
|   141566 | 4527 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 4528 | `{` |
|        - | 4529 | `	ph7_value *pStack;` |
|        - | 4530 | `	sxu32 nCap;` |
|        - | 4531 | `	sxi32 rc;` |
|        - | 4532 | `	/* Allocate a new operand stack */` |
|   141571 | 4533 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   141571 | 4534 | `	if( pStack == 0 ){` |
|      ! 0 | 4535 | `		return SXERR_MEM;` |
|        - | 4536 | `	}` |
|   141571 | 4537 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|        - | 4538 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|        - | 4539 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   141571 | 4540 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|        - | 4541 | `	/* Free the operand stack */` |
|   141571 | 4542 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 4543 | `	/* Execution result */` |
|   141571 | 4544 | `	return rc;` |
|    70788 | 4545 | `}` |
|        - | 4546 | `/*` |
|        - | 4547 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|        - | 4548 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|        - | 4549 | ` * the argument resolve against that class (like php) rather than the reflection` |
|        - | 4550 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|        - | 4551 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|        - | 4552 | ` */` |
|       54 | 4553 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|        1 | 4554 | `{` |
|       55 | 4555 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       55 | 4556 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - | 4557 | `	sxi32 rc;` |
|       55 | 4558 | `	if( pDeclCls ){` |
|       39 | 4559 | `		pVm->pConstEvalClass = pDeclCls;` |
|       39 | 4560 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       19 | 4561 | `	}` |
|       55 | 4562 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       55 | 4563 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       55 | 4564 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       55 | 4565 | `	return rc;` |
|        1 | 4566 | `}` |
|        - | 4567 | `/*` |
|        - | 4568 | ` * Invoke any installed shutdown callbacks.` |
|        - | 4569 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|        - | 4570 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|        - | 4571 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|        - | 4572 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|        - | 4573 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|        - | 4574 | ` *` |
|        - | 4575 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|        - | 4576 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|        - | 4577 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|        - | 4578 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|        - | 4579 | ` * down and restore the default consumer.` |
|        - | 4580 | ` */` |
|     3392 | 4581 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|        5 | 4582 | `{` |
|     3397 | 4583 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - | 4584 | `	sxu32 n,nUsed;` |
|     3397 | 4585 | `	nUsed = SySetUsed(&pVm->aOB);` |
|     3397 | 4586 | `	if( nUsed < 1 ){` |
|     3395 | 4587 | `		return;` |
|        - | 4588 | `	}` |
|        7 | 4589 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4590 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4591 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|        5 | 4592 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|        5 | 4593 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|        2 | 4594 | `		}` |
|        3 | 4595 | `	}` |
|        - | 4596 | `	/* Restore the default consumer and release the buffers. */` |
|        3 | 4597 | `	pCons->xConsumer = pCons->xDef;` |
|        3 | 4598 | `	pCons->pUserData = pCons->pDefData;` |
|        7 | 4599 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4600 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4601 | `		if( pOb ){` |
|        5 | 4602 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|        5 | 4603 | `			SyBlobRelease(&pOb->sOB);` |
|        2 | 4604 | `		}` |
|        3 | 4605 | `	}` |
|        3 | 4606 | `	SySetReset(&pVm->aOB);` |
|        3 | 4607 | `	pVm->nObDepth = 0;` |
|     1701 | 4608 | `}` |
|        - | 4609 | `/*` |
|        - | 4610 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 4611 | ` * or more calls to [register_shutdown_function()].` |
|        - | 4612 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 4613 | ` * execution ends.` |
|        - | 4614 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 4615 | ` * additional information.` |
|        - | 4616 | ` */` |
|     3392 | 4617 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 4618 | `{` |
|        - | 4619 | `	VmShutdownCB *pEntry;` |
|        - | 4620 | `	ph7_value *apArg[10];` |
|        - | 4621 | `	sxu32 n,nEntry;` |
|        - | 4622 | `	int i;` |
|        - | 4623 | `	/* Point to the stack of registered callbacks */` |
|     3397 | 4624 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    37317 | 4625 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    33925 | 4626 | `		apArg[i] = 0;` |
|    16965 | 4627 | `	}` |
|        - | 4628 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 4629 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 4630 | `	 * callbacks, mirroring PHP.` |
|        - | 4631 | `	 */` |
|     3397 | 4632 | `	pVm->bHaltRequested = 0;` |
|     3415 | 4633 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       23 | 4634 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4635 | `		if( pEntry ){` |
|        - | 4636 | `			/* Prepare callback arguments if any */` |
|       23 | 4637 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 4638 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 4639 | `					break;` |
|        - | 4640 | `				}` |
|      ! 0 | 4641 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 4642 | `			}` |
|        - | 4643 | `			/* Invoke the callback */` |
|       23 | 4644 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 4645 | `			/*` |
|        - | 4646 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 4647 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 4648 | `			 */` |
|       23 | 4649 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4650 | `			if( pEntry ){` |
|       23 | 4651 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       23 | 4652 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 4653 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 4654 | `				}` |
|        9 | 4655 | `			}` |
|       23 | 4656 | `			if( pVm->bHaltRequested ){` |
|        - | 4657 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 4658 | `				break;` |
|        - | 4659 | `			}` |
|        9 | 4660 | `		}` |
|       14 | 4661 | `	}` |
|     3397 | 4662 | `	SySetReset(&pVm->aShutdown);` |
|     3397 | 4663 | `}` |
|        - | 4664 | `/*` |
|        - | 4665 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 4666 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4667 | ` * See block-comment on that function for additional information.` |
|        - | 4668 | ` */` |
|     3392 | 4669 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 4670 | `{` |
|        - | 4671 | `	/* Make sure we are ready to execute this program */` |
|     3397 | 4672 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 4673 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 4674 | `	}` |
|        - | 4675 | `	/* Set the execution magic number  */` |
|     3397 | 4676 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 4677 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|        - | 4678 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|        - | 4679 | `	{` |
|     3397 | 4680 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|     3397 | 4681 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|        - | 4682 | `	}` |
|        - | 4683 | `	/* Invoke any shutdown callbacks */` |
|     3397 | 4684 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 4685 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|        - | 4686 | `	 * shutdown callbacks, which may still write into them. */` |
|     3397 | 4687 | `	VmFlushOutputBuffers(&(*pVm));` |
|        - | 4688 | `	/*` |
|        - | 4689 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 4690 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 4691 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 4692 | `	 */` |
|     3397 | 4693 | `	return SXRET_OK;` |
|     1701 | 4694 | `}` |
|        - | 4695 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 4696 | `/*` |
|        - | 4697 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 4698 | ` * the desired message.` |
|        - | 4699 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 4700 | ` * in 'api.c' for additional information.` |
|        - | 4701 | ` */` |
|     2552 | 4702 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 4703 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 4704 | `	SyString *pString /* Message to output */` |
|        - | 4705 | `	)` |
|        5 | 4706 | `{` |
|     2557 | 4707 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     2557 | 4708 | `	sxi32 rc = SXRET_OK;` |
|        - | 4709 | `	/* Call the output consumer */` |
|     2557 | 4710 | `	if( pString->nByte > 0 ){` |
|     2557 | 4711 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     2557 | 4712 | `		VmTrackOutput(pVm, pString->nByte);` |
|     1276 | 4713 | `	}` |
|     2557 | 4714 | `	return rc;` |
|        5 | 4715 | `}` |
|        - | 4716 | `/*` |
|        - | 4717 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 4718 | ` * callback to consume the formatted message.` |
|        - | 4719 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 4720 | ` * in 'api.c' for additional information.` |
|        - | 4721 | ` */` |
|        2 | 4722 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 4723 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 4724 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 4725 | `	va_list ap           /* Variable list of arguments */` |
|        - | 4726 | `	)` |
|        1 | 4727 | `{` |
|        3 | 4728 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 4729 | `	sxi32 rc = SXRET_OK;` |
|        - | 4730 | `	SyBlob sWorker;` |
|        - | 4731 | `	/* Format the message and call the output consumer */` |
|        3 | 4732 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 4733 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 4734 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 4735 | `		/* Consume the formatted message */` |
|        3 | 4736 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 4737 | `	}` |
|        3 | 4738 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 4739 | `	/* Release the working buffer */` |
|        3 | 4740 | `	SyBlobRelease(&sWorker);` |
|        3 | 4741 | `	return rc;` |
|        1 | 4742 | `}` |
|        - | 4743 | `/*` |
|        - | 4744 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 4745 | ` * This function never fail and always return a pointer` |
|        - | 4746 | ` * to a null terminated string.` |
|        - | 4747 | ` */` |
|       12 | 4748 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 4749 | `{` |
|       13 | 4750 | `	const char *zOp = "Unknown     ";` |
|       13 | 4751 | `	switch(nOp){` |
|        3 | 4752 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 4753 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 4754 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 4755 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 4756 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 4757 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 4758 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 4759 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 4760 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 4761 | `	case PH7_OP_LOAD_FCC:` |
|      ! 0 | 4762 | `		                    zOp = "LOAD_FCC   "; break;` |
|      ! 0 | 4763 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 4764 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 4765 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 4766 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 4767 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 4768 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 4769 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 4770 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 4771 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 4772 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 4773 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 4774 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 4775 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 4776 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 4777 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 4778 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 4779 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 4780 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 4781 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 4782 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 4783 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 4784 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 4785 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 4786 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 4787 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 4788 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 4789 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 4790 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 4791 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 4792 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 4793 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 4794 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 4795 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 4796 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 4797 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 4798 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 4799 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 4800 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 4801 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 4802 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 4803 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 4804 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 4805 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 4806 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 4807 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 4808 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 4809 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|      ! 0 | 4810 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 4811 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 4812 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 4813 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 4814 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 4815 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 4816 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 4817 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 4818 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 4819 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 4820 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 4821 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 4822 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 4823 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 4824 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 4825 | `	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;` |
|      ! 0 | 4826 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 4827 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 4828 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 4829 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 4830 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 4831 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 4832 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 4833 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 4834 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 4835 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 4836 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 4837 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 4838 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 4839 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 4840 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 4841 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 4842 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 4843 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|      ! 0 | 4844 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 4845 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 4846 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 4847 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 4848 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 4849 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 4850 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 4851 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 4852 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 4853 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 4854 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 4855 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 4856 | `	default:` |
|      ! 0 | 4857 | `		break;` |
|        - | 4858 | `	}` |
|       13 | 4859 | `	return zOp;` |
|        1 | 4860 | `}` |
|        - | 4861 | `/*` |
|        - | 4862 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 4863 | ` * The xConsumer() callback which is an used defined function` |
|        - | 4864 | ` * is responsible of consuming the generated dump.` |
|        - | 4865 | ` */` |
|        2 | 4866 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 4867 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 4868 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 4869 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 4870 | `	)` |
|        1 | 4871 | `{` |
|        - | 4872 | `	sxi32 rc;` |
|        3 | 4873 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 4874 | `	return rc;` |
|        1 | 4875 | `}` |
|        - | 4876 | `/*` |
|        - | 4877 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 4878 | ` * outside a class body [i.e: global or function scope].` |
|        - | 4879 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 4880 | ` * in 'compile.c' for additional information.` |
|        - | 4881 | ` */` |
|       64 | 4882 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        3 | 4883 | `{` |
|       67 | 4884 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 4885 | `	/* Evaluate and expand constant value */` |
|       67 | 4886 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       67 | 4887 | `}` |
|        - | 4888 | `/*` |
|        - | 4889 | ` * Section:` |
|        - | 4890 | ` *  Function handling functions.` |
|        - | 4891 | ` * Status:` |
|        - | 4892 | ` *    Stable.` |
|        - | 4893 | ` */` |
|        - | 4894 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 4895 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 4896 | `	{ "__phl_magic_call", vm_builtin_magic_call },` |
|        - | 4897 | `	{ "__phl_enum_cases",   vm_builtin_enum_cases },` |
|        - | 4898 | `	{ "__phl_enum_from",    vm_builtin_enum_from },` |
|        - | 4899 | `	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },` |
|        - | 4900 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|        - | 4901 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 4902 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 4903 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 4904 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 4905 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 4906 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 4907 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 4908 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 4909 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 4910 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 4911 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 4912 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 4913 | `	    /* Constants management */` |
|        - | 4914 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 4915 | `	{ "define",   vm_builtin_define               },` |
|        - | 4916 | `	{ "constant", vm_builtin_constant             },` |
|        - | 4917 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 4918 | `	   /* Class/Object functions */` |
|        - | 4919 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 4920 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 4921 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 4922 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 4923 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 4924 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|        - | 4925 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 4926 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 4927 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 4928 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 4929 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 4930 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 4931 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 4932 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 4933 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 4934 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 4935 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 4936 | `	   /* SPL object identity */` |
|        - | 4937 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|        - | 4938 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|        - | 4939 | `	   /* SPL Autoloading */` |
|        - | 4940 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 4941 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 4942 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 4943 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 4944 | `	   /* Random numbers/strings generators */` |
|        - | 4945 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 4946 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 4947 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 4948 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 4949 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 4950 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 4951 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 4952 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 4953 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 4954 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 4955 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 4956 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 4957 | `	   /* Language constructs functions */` |
|        - | 4958 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 4959 | `	{ "print", vm_builtin_print                   },` |
|        - | 4960 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 4961 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 4962 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 4963 | `	  /* Variable handling functions */` |
|        - | 4964 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 4965 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 4966 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 4967 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|        - | 4968 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 4969 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 4970 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 4971 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 4972 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 4973 | `	  /* Ouput control functions */` |
|        - | 4974 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 4975 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 4976 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 4977 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 4978 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 4979 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 4980 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 4981 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 4982 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 4983 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 4984 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 4985 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 4986 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 4987 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 4988 | `	  /* Assertion functions */` |
|        - | 4989 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 4990 | `	  /* Error reporting functions */` |
|        - | 4991 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 4992 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 4993 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 4994 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 4995 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 4996 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 4997 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 4998 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 4999 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|        - | 5000 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|        - | 5001 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 5002 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|        - | 5003 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|        - | 5004 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 5005 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 5006 | `	  /* Release info */` |
|        - | 5007 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 5008 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 5009 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 5010 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 5011 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 5012 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 5013 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 5014 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 5015 | `	  /* hashmap */` |
|        - | 5016 | `	{"compact",          vm_builtin_compact       },` |
|        - | 5017 | `	{"extract",          vm_builtin_extract       },` |
|        - | 5018 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 5019 | `	  /* URL related function */` |
|        - | 5020 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 5021 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
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
|     3388 | 5044 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 5045 | `{` |
|        - | 5046 | `	sxi32 rc;` |
|        - | 5047 | `	sxu32 n;` |
|   420117 | 5048 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 5049 | `		/* Note that these special functions have access` |
|        - | 5050 | `		 * to the underlying virtual machine as their` |
|        - | 5051 | `		 * private data.` |
|        - | 5052 | `		 */` |
|   416729 | 5053 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   416729 | 5054 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 5055 | `			return rc;` |
|        - | 5056 | `		}` |
|   208367 | 5057 | `	}` |
|     3393 | 5058 | `	return SXRET_OK;` |
|     1699 | 5059 | `}` |
|        - | 5060 | `/*` |
|        - | 5061 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 5062 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 5063 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 5064 | ` */` |
|   782464 | 5065 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 5066 | `{` |
|   782469 | 5067 | `	if( !iLoadable ){` |
|   775675 | 5068 | `		return pClass;` |
|        - | 5069 | `	}` |
|     6803 | 5070 | `	while(pClass){` |
|     6799 | 5071 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     6795 | 5072 | `			return pClass;` |
|        - | 5073 | `		}` |
|        5 | 5074 | `		pClass = pClass->pNextName;` |
|        1 | 5075 | `	}` |
|        5 | 5076 | `	return 0;` |
|   391237 | 5077 | `}` |
|        - | 5078 | `/*` |
|        - | 5079 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 5080 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 5081 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 5082 | ` * registered in the VM's class table.` |
|        - | 5083 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 5084 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 5085 | ` */` |
|      334 | 5086 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5087 | `{` |
|        - | 5088 | `	VmAutoloadCB *pEntry;` |
|        - | 5089 | `	ph7_value sArg,sResult;` |
|        - | 5090 | `	SyHashEntry *pHashEntry;` |
|        - | 5091 | `	ph7_class *pClass;` |
|        - | 5092 | `	sxu32 n,nEntry;` |
|      339 | 5093 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|      339 | 5094 | `	if( nEntry < 1 ){` |
|      263 | 5095 | `		return 0;` |
|        - | 5096 | `	}` |
|        - | 5097 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       81 | 5098 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 5099 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 5100 | `	}` |
|        - | 5101 | `	/* Mark this class as being autoloaded */` |
|       79 | 5102 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 5103 | `	/* Prepare the class name argument */` |
|       79 | 5104 | `	PH7_MemObjInit(pVm,&sArg);` |
|       79 | 5105 | `	PH7_MemObjInit(pVm,&sResult);` |
|       79 | 5106 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       79 | 5107 | `	pClass = 0;` |
|      147 | 5108 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 5109 | `		ph7_value *apArg[1];` |
|       89 | 5110 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       89 | 5111 | `		if( pEntry == 0 ){` |
|      ! 0 | 5112 | `			continue;` |
|        - | 5113 | `		}` |
|       89 | 5114 | `		apArg[0] = &sArg;` |
|       89 | 5115 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 5116 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 5117 | `			continue;` |
|        - | 5118 | `		}` |
|        - | 5119 | `		/* Check if the class is now available */` |
|       89 | 5120 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       89 | 5121 | `		if( pHashEntry ){` |
|       21 | 5122 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       21 | 5123 | `			if( pClass ){` |
|       21 | 5124 | `				break;` |
|        - | 5125 | `			}` |
|      ! 0 | 5126 | `		}` |
|       38 | 5127 | `	}` |
|       79 | 5128 | `	PH7_MemObjRelease(&sArg);` |
|       79 | 5129 | `	PH7_MemObjRelease(&sResult);` |
|        - | 5130 | `	/* Remove reentrancy guard */` |
|       79 | 5131 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       79 | 5132 | `	return pClass;` |
|      172 | 5133 | `}` |
|        - | 5134 | `/*` |
|        - | 5135 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 5136 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 5137 | ` */` |
|       42 | 5138 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5139 | `{` |
|       47 | 5140 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 | 5141 | `}` |
|        - | 5142 | `/*` |
|        - | 5143 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|        - | 5144 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|        - | 5145 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|        - | 5146 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|        - | 5147 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|        - | 5148 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|        - | 5149 | ` * only turn a failing lookup into a match, never break an existing one.` |
|        - | 5150 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|        - | 5151 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|        - | 5152 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|        - | 5153 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|        - | 5154 | ` */` |
|   787950 | 5155 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|        5 | 5156 | `{` |
|   787955 | 5157 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|       59 | 5158 | `		(*pzName)++;` |
|       59 | 5159 | `		(*pnByte)--;` |
|       28 | 5160 | `	}` |
|   787955 | 5161 | `}` |
|        - | 5162 | `/*` |
|        - | 5163 | ` * Check if the given name refer to an installed class.` |
|        - | 5164 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 5165 | ` */` |
|   782740 | 5166 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 5167 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 5168 | `	const char *zName,  /* Name of the target class */` |
|        - | 5169 | `	sxu32 nByte,        /* zName length */` |
|        - | 5170 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 5171 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 5172 | `						 */` |
|        - | 5173 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 5174 | `	)` |
|        5 | 5175 | `{` |
|        - | 5176 | `	SyHashEntry *pEntry;` |
|        - | 5177 | `	ph7_class *pClass;` |
|   782745 | 5178 | `	sxu32 nOrig = nByte;` |
|   391370 | 5179 | `	SXUNUSED(iNest);` |
|        - | 5180 | `	/* Exact class lookup.` |
|        - | 5181 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 5182 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|        - | 5183 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|        - | 5184 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   782745 | 5185 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|        - | 5186 | `	/* An empty stripped name never matches a stored key (none is empty); skip the` |
|        - | 5187 | `	 * hash probe. But php still fires the autoloader when the ORIGINAL name was` |
|        - | 5188 | `	 * non-empty — a lone "\" autoloads with the empty stripped name, whereas a` |
|        - | 5189 | `	 * truly empty "" does not. Gate autoload on nOrig, pass the stripped name. */` |
|   782745 | 5190 | `	pEntry = nByte > 0 ? SyHashGet(&pVm->hClass,(const void *)zName,nByte) : 0;` |
|   782745 | 5191 | `	if( pEntry == 0 ){` |
|        - | 5192 | `		/* Class not found in hash table — try autoload before giving up */` |
|      297 | 5193 | `		return nOrig > 0 ? VmTriggerAutoload(pVm,zName,nByte,iLoadable) : 0;` |
|        - | 5194 | `	}` |
|   782453 | 5195 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   782453 | 5196 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   391375 | 5197 | `}` |
|        - | 5198 | `/*` |
|        - | 5199 | ` * Reference Table Implementation` |
|        - | 5200 | ` * Status: stable <chm@symisc.net>` |
|        - | 5201 | ` * Intro` |
|        - | 5202 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 5203 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 5204 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 5205 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 5206 | ` *  Refer to the official for more information on this powerful` |
|        - | 5207 | ` *  extension.` |
|        - | 5208 | ` */` |
|        - | 5209 | `/*` |
|        - | 5210 | ` * Allocate a new reference entry.` |
|        - | 5211 | ` */` |
|  4388865 | 5212 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 5213 | `{` |
|        - | 5214 | `	VmRefObj *pRef;` |
|        - | 5215 | `	/* Allocate a new instance */` |
|  4388870 | 5216 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  4388870 | 5217 | `	if( pRef == 0 ){` |
|      ! 0 | 5218 | `		return 0;` |
|        - | 5219 | `	}` |
|        - | 5220 | `	/* Zero the structure */` |
|  4388870 | 5221 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 5222 | `	/* Initialize fields */` |
|  4388870 | 5223 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  4388870 | 5224 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  4388870 | 5225 | `	pRef->nIdx = nIdx;` |
|  4388870 | 5226 | `	return pRef;` |
|  2195067 | 5227 | `}` |
|        - | 5228 | `/*` |
|        - | 5229 | ` * Default hash function used by the reference table` |
|        - | 5230 | ` * for lookup/insertion operations.` |
|        - | 5231 | ` */` |
| 23568806 | 5232 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 5233 | `{` |
|        - | 5234 | `	/* Calculate the hash based on the memory object index */` |
| 23568811 | 5235 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 5236 | `}` |
|        - | 5237 | `/*` |
|        - | 5238 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 5239 | ` * in the reference table.` |
|        - | 5240 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 5241 | ` * otherwise.` |
|        - | 5242 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5243 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5244 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5245 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5246 | ` * Refer to the official for more information on this powerful` |
|        - | 5247 | ` * extension.` |
|        - | 5248 | ` */` |
| 13631251 | 5249 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 5250 | `{` |
|        - | 5251 | `	VmRefObj *pRef;` |
|        - | 5252 | `	sxu32 nBucket;` |
|        - | 5253 | `	/* Point to the appropriate bucket */` |
| 13631256 | 5254 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 5255 | `	/* Perform the lookup */` |
| 13631256 | 5256 | `	pRef = pVm->apRefObj[nBucket];` |
| 30053276 | 5257 | `	for(;;){` |
| 60077777 | 5258 | `		if( pRef == 0 ){` |
|  4898936 | 5259 | `			break;` |
|        - | 5260 | `		}` |
| 55178846 | 5261 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 5262 | `			/* Entry found */` |
|  8732325 | 5263 | `			return pRef;` |
|        - | 5264 | `		}` |
|        - | 5265 | `		/* Point to the next entry */` |
| 46446526 | 5266 | `		pRef = pRef->pNextCollide;` |
|        5 | 5267 | `	}` |
|        - | 5268 | `	/* No such entry,return NULL */` |
|  4898936 | 5269 | `	return 0;` |
|  6817519 | 5270 | `}` |
|        - | 5271 | `/*` |
|        - | 5272 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5273 | ` *` |
|        - | 5274 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5275 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5276 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5277 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5278 | ` * Refer to the official for more information on this powerful` |
|        - | 5279 | ` * extension.` |
|        - | 5280 | ` */` |
|  4388865 | 5281 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5282 | `{` |
|        - | 5283 | `	sxu32 nBucket;` |
|  4388870 | 5284 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 5285 | `		VmRefObj **apNew;` |
|        - | 5286 | `		sxu32 nNew;` |
|        - | 5287 | `		/* Allocate a larger table */` |
|    10503 | 5288 | `		nNew = pVm->nRefSize << 1;` |
|    10503 | 5289 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|    10503 | 5290 | `		if( apNew ){` |
|    10503 | 5291 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 5292 | `			sxu32 n;` |
|        - | 5293 | `			/* Zero the structure */` |
|    10503 | 5294 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 5295 | `			/* Rehash all referenced entries */` |
|  2966165 | 5296 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 5297 | `				/* Remove old collision links */` |
|  2955667 | 5298 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 5299 | `				/* Point to the appropriate bucket */` |
|  2955667 | 5300 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 5301 | `				/* Insert the entry  */` |
|  2955667 | 5302 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2955667 | 5303 | `				if( apNew[nBucket] ){` |
|  2306069 | 5304 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1153032 | 5305 | `				}` |
|  2955667 | 5306 | `				apNew[nBucket] = pEntry;` |
|        - | 5307 | `				/* Point to the next entry */` |
|  2955667 | 5308 | `				pEntry = pEntry->pNext;` |
|  1477836 | 5309 | `			}` |
|        - | 5310 | `			/* Release the old table */` |
|    10503 | 5311 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 5312 | `			/* Install the new one */` |
|    10503 | 5313 | `			pVm->apRefObj = apNew;` |
|    10503 | 5314 | `			pVm->nRefSize = nNew;` |
|     5249 | 5315 | `		}` |
|     5249 | 5316 | `	}` |
|        - | 5317 | `	/* Point to the appropriate bucket */` |
|  4388870 | 5318 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 5319 | `	/* Insert the entry */` |
|  4388870 | 5320 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  4388870 | 5321 | `	if( pVm->apRefObj[nBucket] ){` |
|  3381668 | 5322 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1691390 | 5323 | `	}` |
|  4388870 | 5324 | `	pVm->apRefObj[nBucket] = pRef;` |
|  4388870 | 5325 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  4388870 | 5326 | `	pVm->nRefUsed++;` |
|  4388870 | 5327 | `	return SXRET_OK;` |
|        5 | 5328 | `}` |
|        - | 5329 | `/*` |
|        - | 5330 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 5331 | ` * the reference table.` |
|        - | 5332 | ` * This function is invoked when the user perform an unset` |
|        - | 5333 | ` * call [i.e: unset($var); ].` |
|        - | 5334 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5335 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5336 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5337 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5338 | ` * Refer to the official for more information on this powerful` |
|        - | 5339 | ` * extension.` |
|        - | 5340 | ` */` |
|  4255563 | 5341 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5342 | `{` |
|        - | 5343 | `	ph7_hashmap_node **apNode;` |
|        - | 5344 | `	SyHashEntry **apEntry;` |
|        - | 5345 | `	sxu32 n;` |
|        - | 5346 | `	/* Point to the reference table */` |
|  4255568 | 5347 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  4255568 | 5348 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 5349 | `	/* Unlink the entry from the reference table */` |
|  4771692 | 5350 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   516129 | 5351 | `		if( apEntry[n] ){` |
|   510301 | 5352 | `			SyHashDeleteEntry2(apEntry[n]);` |
|   255778 | 5353 | `		}` |
|   258697 | 5354 | `	}` |
|  7978961 | 5355 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3723398 | 5356 | `		if( apNode[n] ){` |
|     1386 | 5357 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|      691 | 5358 | `		}` |
|  1861701 | 5359 | `	}` |
|  4255568 | 5360 | `	if( pRef->pPrevCollide ){` |
|  1662540 | 5361 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   831202 | 5362 | `	}else{` |
|  2593033 | 5363 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 5364 | `	}` |
|  4255568 | 5365 | `	if( pRef->pNextCollide ){` |
|  2268707 | 5366 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|  1134911 | 5367 | `	}` |
|  4255568 | 5368 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 5369 | `	/* Release the node */` |
|  4255568 | 5370 | `	SySetRelease(&pRef->aReference);` |
|  4255568 | 5371 | `	SySetRelease(&pRef->aArrEntries);` |
|  4255568 | 5372 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  4255568 | 5373 | `	pVm->nRefUsed--;` |
|  4255568 | 5374 | `	return SXRET_OK;` |
|        5 | 5375 | `}` |
|        - | 5376 | `/*` |
|        - | 5377 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5378 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5379 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5380 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5381 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5382 | ` * Refer to the official for more information on this powerful` |
|        - | 5383 | ` * extension.` |
|        - | 5384 | ` */` |
|  4435207 | 5385 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 5386 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5387 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5388 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5389 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 5390 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 5391 | `	)` |
|        5 | 5392 | `{` |
|  4435212 | 5393 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 5394 | `	VmRefObj *pRef;` |
|        - | 5395 | `	/* Check if the referenced object already exists */` |
|  4435212 | 5396 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4435212 | 5397 | `	if( pRef == 0 ){` |
|        - | 5398 | `		/* Create a new entry */` |
|  4388870 | 5399 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  4388870 | 5400 | `		if( pRef == 0 ){` |
|      ! 0 | 5401 | `			return SXERR_MEM;` |
|        - | 5402 | `		}` |
|  4388870 | 5403 | `		pRef->iFlags = iFlags;` |
|        - | 5404 | `		/* Install the entry */` |
|  4388870 | 5405 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  2195062 | 5406 | `	}` |
|  4435212 | 5407 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  4435212 | 5408 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 5409 | `		VmSlot sRef;` |
|        - | 5410 | `		/* Local frame,record referenced entry so that it can` |
|        - | 5411 | `		 * be deleted when we leave this frame.` |
|        - | 5412 | `		 */` |
|   510269 | 5413 | `		sRef.nIdx = nIdx;` |
|   510269 | 5414 | `		sRef.pUserData = pEntry;` |
|   510269 | 5415 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 5416 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 5417 | `		}` |
|   255762 | 5418 | `	}` |
|  4435212 | 5419 | `	if( pEntry ){` |
|        - | 5420 | `		/* Address of the hash-entry */` |
|   556353 | 5421 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|   278804 | 5422 | `	}` |
|  4435212 | 5423 | `	if( pMapEntry ){` |
|        - | 5424 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3824912 | 5425 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1912453 | 5426 | `	}` |
|  4435212 | 5427 | `	return SXRET_OK;` |
|  2218238 | 5428 | `}` |
|        - | 5429 | `/*` |
|        - | 5430 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 5431 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5432 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5433 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5434 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5435 | ` * Refer to the official for more information on this powerful` |
|        - | 5436 | ` * extension.` |
|        - | 5437 | ` */` |
|  4226417 | 5438 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 5439 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5440 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5441 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5442 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 5443 | `	)` |
|        5 | 5444 | `{` |
|        - | 5445 | `	VmRefObj *pRef;` |
|        - | 5446 | `	sxu32 n;` |
|        - | 5447 | `	/* Check if the referenced object already exists */` |
|  4226422 | 5448 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4226422 | 5449 | `	if( pRef == 0 ){` |
|        - | 5450 | `		/* Not such entry */` |
|   510071 | 5451 | `		return SXERR_NOTFOUND;` |
|        - | 5452 | `	}` |
|        - | 5453 | `	/* Remove the desired entry */` |
|  3716356 | 5454 | `	if( pEntry ){` |
|        - | 5455 | `		SyHashEntry **apEntry;` |
|       87 | 5456 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 5457 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      185 | 5458 | `			if( apEntry[n] == pEntry ){` |
|        - | 5459 | `				/* Nullify the entry */` |
|       85 | 5460 | `				apEntry[n] = 0;` |
|        - | 5461 | `				/*` |
|        - | 5462 | `				 * NOTE:` |
|        - | 5463 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 5464 | `				 * we avoid wasting spaces.` |
|        - | 5465 | `				 */` |
|       40 | 5466 | `			}` |
|       95 | 5467 | `		}` |
|       41 | 5468 | `	}` |
|  3716356 | 5469 | `	if( pMapEntry ){` |
|        - | 5470 | `		ph7_hashmap_node **apNode;` |
|  3716274 | 5471 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  7432807 | 5472 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3716538 | 5473 | `			if( apNode[n] == pMapEntry ){` |
|        - | 5474 | `				/* nullify the entry */` |
|  3716274 | 5475 | `				apNode[n] = 0;` |
|  1858134 | 5476 | `			}` |
|  1858271 | 5477 | `		}` |
|  1858134 | 5478 | `	}` |
|  3716356 | 5479 | `	return SXRET_OK;` |
|  2113843 | 5480 | `}` |
|        - | 5481 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 5482 | `/*` |
|        - | 5483 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 5484 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 5485 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 5486 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 5487 | ` * For more information on how to register IO stream devices,please` |
|        - | 5488 | ` * refer to the official documentation.` |
|        - | 5489 | ` */` |
|    29862 | 5490 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 5491 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 5492 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 5493 | `	int nByte              /* *pzDevice length*/` |
|        - | 5494 | `	)` |
|        5 | 5495 | `{` |
|        - | 5496 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 5497 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 5498 | `	SyString sDev,sCur;` |
|        - | 5499 | `	sxu32 n,nEntry;` |
|        - | 5500 | `	int rc;` |
|        - | 5501 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29867 | 5502 | `	zNext = zCur = zIn = *pzDevice;` |
|    29867 | 5503 | `	zEnd = &zIn[nByte];` |
|  1892556 | 5504 | `	while( zIn < zEnd ){` |
|  1862726 | 5505 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 5506 | `			/* Got one */` |
|       35 | 5507 | `			zNext = &zIn[sizeof("://")-1];` |
|       35 | 5508 | `			break;` |
|        - | 5509 | `		}` |
|        - | 5510 | `		/* Advance the cursor */` |
|  1862694 | 5511 | `		zIn++;` |
|        5 | 5512 | `	}` |
|    29867 | 5513 | `	if( zIn >= zEnd ){` |
|        - | 5514 | `		/* No such scheme,return the default stream */` |
|    29835 | 5515 | `		return pVm->pDefStream;` |
|        - | 5516 | `	}` |
|       35 | 5517 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 5518 | `	/* Remove leading and trailing white spaces */` |
|       35 | 5519 | `	SyStringFullTrim(&sDev);` |
|        - | 5520 | `	/* Perform a linear lookup on the installed stream devices */` |
|       35 | 5521 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       35 | 5522 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|       69 | 5523 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       69 | 5524 | `		pStream = apStream[n];` |
|       69 | 5525 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 5526 | `		/* Perfrom a case-insensitive comparison */` |
|       69 | 5527 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|       69 | 5528 | `		if( rc == 0 ){` |
|        - | 5529 | `			/* Stream device found */` |
|       35 | 5530 | `			*pzDevice = zNext;` |
|       35 | 5531 | `			return pStream;` |
|        - | 5532 | `		}` |
|       19 | 5533 | `	}` |
|        - | 5534 | `	/* No such stream,return NULL */` |
|      ! 0 | 5535 | `	return 0;` |
|    14936 | 5536 | `}` |
|        - | 5537 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 5538 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 5539 |  |
