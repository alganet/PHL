# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2433/2892 lines (84.13%)

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
|   1592695 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   77 | `{` |
|   1592700 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|        40 |   79 | `		return TRUE;` |
|         - |   80 | `	}` |
|   1592662 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        11 |   82 | `		return TRUE;` |
|         - |   83 | `	}` |
|   1592652 |   84 | `	return FALSE;` |
|    797181 |   85 | `}` |
|         - |   86 | `/*` |
|         - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |   93 | ` * still go through the existing numeric coercion.` |
|         - |   94 | ` */` |
|    542201 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |   96 | `{` |
|         - |   97 | `	SyString sStr;` |
|    542206 |   98 | `	sxu8 bReal = FALSE;` |
|    542206 |   99 | `	const char *zTail = 0;` |
|         - |  100 | `	const char *zEnd;` |
|    542206 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    542192 |  102 | `		return FALSE;` |
|         - |  103 | `	}` |
|        17 |  104 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        17 |  105 | `	if( sStr.nByte == 0 ){` |
|       ! 0 |  106 | `		return TRUE;` |
|         - |  107 | `	}` |
|        17 |  108 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|         5 |  109 | `		return TRUE;` |
|         - |  110 | `	}` |
|         - |  111 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|         - |  112 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|         - |  113 | `	 * still takes the Perl path. */` |
|        13 |  114 | `	zEnd = sStr.zString + sStr.nByte;` |
|        13 |  115 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|       ! 0 |  116 | `		zTail++;` |
|       ! 0 |  117 | `	}` |
|        13 |  118 | `	return zTail < zEnd;` |
|    271276 |  119 | `}` |
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
|   1629758 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  139 | `	const SyString *pName,  /* Constant name */` |
|         - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  142 | `	)` |
|         5 |  143 | `{` |
|   1629763 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  145 | `}` |
|         - |  146 | `/*` |
|         - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  149 | ` */` |
|   1629876 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|   1629881 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   1629881 |  165 | `	if( pEntry ){` |
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
|   1629879 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   1629879 |  190 | `	if( pCons == 0 ){` |
|       ! 0 |  191 | `		return 0;` |
|         - |  192 | `	}` |
|         - |  193 | `	/* Duplicate constant name */` |
|   1629879 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   1629879 |  195 | `	if( zDupName == 0 ){` |
|       ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  197 | `		return 0;` |
|         - |  198 | `	}` |
|   1629879 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|   1629879 |  200 | `	if( pFile ){` |
|       121 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|        58 |  202 | `	}` |
|   1629879 |  203 | `	pCons->nLine = nLine;` |
|   1629879 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         - |  205 | `	/* Install the constant */` |
|   1629879 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   1629879 |  207 | `	pCons->xExpand = xExpand;` |
|   1629879 |  208 | `	pCons->pUserData = pUserData;` |
|   1629879 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   1629879 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   1629879 |  211 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  214 | `		return rc;` |
|         - |  215 | `	}` |
|         - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|   1629879 |  217 | `	return SXRET_OK;` |
|    814943 |  218 | `}` |
|         - |  219 | `/*` |
|         - |  220 | ` * Allocate a new foreign function instance.` |
|         - |  221 | ` * This function return SXRET_OK on success. Any other` |
|         - |  222 | ` * return value indicates failure.` |
|         - |  223 | ` * Please refer to the official documentation for an introduction to` |
|         - |  224 | ` * the foreign function mechanism.` |
|         - |  225 | ` */` |
|   2387592 |  226 | `static sxi32 PH7_NewForeignFunction(` |
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
|   2387597 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   2387597 |  238 | `	if( pFunc == 0 ){` |
|       ! 0 |  239 | `		return SXERR_MEM;` |
|         - |  240 | `	}` |
|         - |  241 | `	/* Duplicate function name */` |
|   2387597 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   2387597 |  243 | `	if( zDup == 0 ){` |
|       ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  245 | `		return SXERR_MEM;` |
|         - |  246 | `	}` |
|         - |  247 | `	/* Zero the structure */` |
|   2387597 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  249 | `	/* Initialize structure fields */` |
|   2387597 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   2387597 |  251 | `	pFunc->pVm   = pVm;` |
|   2387597 |  252 | `	pFunc->xFunc = xFunc;` |
|   2387597 |  253 | `	pFunc->pUserData = pUserData;` |
|   2387597 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  255 | `	/* Write a pointer to the new function */` |
|   2387597 |  256 | `	*ppOut = pFunc;` |
|   2387597 |  257 | `	return SXRET_OK;` |
|   1193801 |  258 | `}` |
|         - |  259 | `/*` |
|         - |  260 | ` * Install a foreign function and it's associated callback so that` |
|         - |  261 | ` * it can be invoked from the target PHP code.` |
|         - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  263 | ` * return value indicates failure.` |
|         - |  264 | ` * Please refer to the official documentation for an introduction to` |
|         - |  265 | ` * the foreign function mechanism.` |
|         - |  266 | ` */` |
|   2391238 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   2391243 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   2391243 |  279 | `	if( pEntry ){` |
|      3651 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      3651 |  281 | `		pFunc->pUserData = pUserData;` |
|      3651 |  282 | `		pFunc->xFunc = xFunc;` |
|      3651 |  283 | `		SySetReset(&pFunc->aAux);` |
|         - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|      3651 |  288 | `		pFunc->nMinArg  = 0;` |
|      3651 |  289 | `		pFunc->nMaxArg  = 0;` |
|      3651 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|      3651 |  291 | `		pFunc->bAtLeast = 0;` |
|      3651 |  292 | `		return SXRET_OK;` |
|         - |  293 | `	}` |
|         - |  294 | `	/* Create a new user function */` |
|   2387597 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   2387597 |  296 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  297 | `		return rc;` |
|         - |  298 | `	}` |
|         - |  299 | `	/* Install the function in the corresponding hashtable */` |
|   2387597 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   2387597 |  301 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  304 | `		return rc;` |
|         - |  305 | `	}` |
|         - |  306 | `	/* User function successfully installed */` |
|   2387597 |  307 | `	return SXRET_OK;` |
|   1195624 |  308 | `}` |
|         - |  309 | `/*` |
|         - |  310 | ` * Initialize a VM function.` |
|         - |  311 | ` */` |
|   3648540 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  315 | `	const char *zName,  /* Function name */` |
|         - |  316 | `	sxu32 nByte,        /* zName length */` |
|         - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  318 | `	void *pUserData     /* Function private data */` |
|         - |  319 | `	)` |
|         5 |  320 | `{` |
|         - |  321 | `	/* Zero the structure */` |
|   3648545 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  323 | `	/* Initialize structure fields */` |
|         - |  324 | `	/* Arguments container */` |
|   3648545 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  326 | `	/* Static variable container */` |
|   3648545 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  328 | `	/* Bytecode container */` |
|   3648545 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  330 | `    /* Preallocate some instruction slots */` |
|   3648545 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  332 | `	/* Closure environment */` |
|   3648545 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   3648545 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  336 | `	/* Declared #[...] attributes */` |
|   3648545 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   3648545 |  338 | `	pFunc->iFlags = iFlags;` |
|   3648545 |  339 | `	pFunc->pUserData = pUserData;` |
|         - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   3648545 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   3648545 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   3643205 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   1821605 |  346 | `	}else{` |
|         - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|      5345 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      5345 |  351 | `		if( pFile ){` |
|      5345 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|      2670 |  353 | `		}` |
|         - |  354 | `	}` |
|   3648545 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   3648545 |  356 | `	return SXRET_OK;` |
|         5 |  357 | `}` |
|         - |  358 | `/*` |
|         - |  359 | ` * Namespace-aware function lookup.` |
|         - |  360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  362 | ` */` |
|         - |  363 | `/*` |
|         - |  364 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  365 | ` */` |
|   6271122 |  366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  367 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  368 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  369 | `	SyString *pName     /* Function name */` |
|         - |  370 | `	)` |
|         5 |  371 | `{` |
|         - |  372 | `	SyHashEntry *pEntry;` |
|         - |  373 | `	sxi32 rc;` |
|   6271127 |  374 | `	if( pName == 0 ){` |
|         - |  375 | `		/* Use the built-in name */` |
|    557935 |  376 | `		pName = &pFunc->sName;` |
|    278965 |  377 | `	}` |
|         - |  378 | `	/* Check for duplicates (functions with the same name) first */` |
|   6271127 |  379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   6271127 |  380 | `	if( pEntry ){` |
|   3123903 |  381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   3123903 |  382 | `		if( pLink != pFunc ){` |
|         - |  383 | `			/* Link */` |
|        50 |  384 | `			pFunc->pNextName = pLink;` |
|        50 |  385 | `			pEntry->pUserData = pFunc;` |
|        23 |  386 | `		}` |
|   3123903 |  387 | `		return SXRET_OK;` |
|         - |  388 | `	}` |
|         - |  389 | `	/* First time seen */` |
|   3147229 |  390 | `	pFunc->pNextName = 0;` |
|   3147229 |  391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   3147229 |  392 | `	return rc;` |
|   3135566 |  393 | `}` |
|         - |  394 | `/*` |
|         - |  395 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  396 | ` */` |
|    565538 |  397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  398 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  399 | `	ph7_class *pClass /* Target Class */` |
|         - |  400 | `	)` |
|         5 |  401 | `{` |
|    565543 |  402 | `	SyString *pName = &pClass->sName;` |
|         - |  403 | `	SyHashEntry *pEntry;` |
|         - |  404 | `	sxi32 rc;` |
|         - |  405 | `	/* Check for duplicates */` |
|    565543 |  406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    565543 |  407 | `	if( pEntry ){` |
|         3 |  408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  409 | `		/* Link entry with the same name */` |
|         3 |  410 | `		pClass->pNextName = pLink;` |
|         3 |  411 | `		pEntry->pUserData = pClass;` |
|         3 |  412 | `		return SXRET_OK;` |
|         - |  413 | `	}` |
|    565541 |  414 | `	pClass->pNextName = 0;` |
|         - |  415 | `	/* Perform a simple hashtable insertion */` |
|    565541 |  416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    565541 |  417 | `	return rc;` |
|    282774 |  418 | `}` |
|         - |  419 | `/*` |
|         - |  420 | ` * Instruction builder interface.` |
|         - |  421 | ` */` |
| 105631896 |  422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  423 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  424 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  425 | `	sxi32 iP1,    /* First operand */` |
|         - |  426 | `	sxu32 iP2,    /* Second operand */` |
|         - |  427 | `	void *p3,     /* Third operand */` |
|         - |  428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  429 | `	)` |
|         5 |  430 | `{` |
|         - |  431 | `	VmInstr sInstr;` |
| 105631901 |  432 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  433 | `	sxi32 rc;` |
|         - |  434 | `	/* Fill the VM instruction */` |
| 105631901 |  435 | `	sInstr.iOp = (sxu8)iOp;` |
| 105631901 |  436 | `	sInstr.iP1 = iP1;` |
| 105631901 |  437 | `	sInstr.iP2 = iP2;` |
| 105631901 |  438 | `	sInstr.p3  = p3;` |
|         - |  439 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  440 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  441 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  442 | `	 * between statements, hence the range check. */` |
| 105631901 |  443 | `	sInstr.nLine = 0;` |
| 105631901 |  444 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|  44985323 |  445 | `		sInstr.nLine = pGen->pIn->nLine;` |
|  83139242 |  446 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  447 | `		/* Past the end (statement tail): blame the last real token. */` |
|  60419655 |  448 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|  30209825 |  449 | `	}` |
| 105631901 |  450 | `	if( pIndex ){` |
|         - |  451 | `		/* Instruction index in the bytecode array */` |
|   7194213 |  452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   3597104 |  453 | `	}` |
|         - |  454 | `	/* Finally,record the instruction */` |
| 105631901 |  455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 105631901 |  456 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  458 | `		/* Fall throw */` |
|       ! 0 |  459 | `	}` |
| 105631901 |  460 | `	return rc;` |
|         5 |  461 | `}` |
|         - |  462 | `/*` |
|         - |  463 | ` * Swap the current bytecode container with the given one.` |
|         - |  464 | ` */` |
|  10443700 |  465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  466 | `{` |
|  10443705 |  467 | `	if( pContainer == 0 ){` |
|         - |  468 | `		/* Point to the default container */` |
|       ! 0 |  469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  470 | `	}else{` |
|         - |  471 | `		/* Change container */` |
|  10443705 |  472 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  473 | `	}` |
|  10443705 |  474 | `	return SXRET_OK;` |
|         5 |  475 | `}` |
|         - |  476 | `/*` |
|         - |  477 | ` * Return the current bytecode container.` |
|         - |  478 | ` */` |
|   5221850 |  479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  480 | `{` |
|   5221855 |  481 | `	return pVm->pByteContainer;` |
|         5 |  482 | `}` |
|         - |  483 | `/*` |
|         - |  484 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  485 | ` */` |
|   7409998 |  486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  487 | `{` |
|         - |  488 | `	VmInstr *pInstr;` |
|   7410003 |  489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   7410003 |  490 | `	return pInstr;` |
|         5 |  491 | `}` |
|         - |  492 | `/*` |
|         - |  493 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  494 | ` */` |
|  58151126 |  495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  496 | `{` |
|  58151131 |  497 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  498 | `}` |
|         - |  499 | `/*` |
|         - |  500 | ` * Pop the last VM instruction.` |
|         - |  501 | ` */` |
|   5770940 |  502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  503 | `{` |
|   5770945 |  504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  505 | `}` |
|         - |  506 | `/*` |
|         - |  507 | ` * Peek the last VM instruction.` |
|         - |  508 | ` */` |
|  21902254 |  509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  510 | `{` |
|  21902259 |  511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  512 | `}` |
|   1835732 |  513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  514 | `{` |
|         - |  515 | `	VmInstr *aInstr;` |
|         - |  516 | `	sxu32 n;` |
|   1835737 |  517 | `	n = SySetUsed(pVm->pByteContainer);` |
|   1835737 |  518 | `	if( n < 2 ){` |
|       ! 0 |  519 | `		return 0;` |
|         - |  520 | `	}` |
|   1835737 |  521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|   1835737 |  522 | `	return &aInstr[n - 2];` |
|    917871 |  523 | `}` |
|         - |  524 | `/*` |
|         - |  525 | ` * Allocate a new virtual machine frame.` |
|         - |  526 | ` */` |
|   5059733 |  527 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  528 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  529 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  531 | `	)` |
|         5 |  532 | `{` |
|         - |  533 | `	VmFrame *pFrame;` |
|         - |  534 | `	/* Allocate a new vm frame */` |
|   5059738 |  535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   5059738 |  536 | `	if( pFrame == 0 ){` |
|       ! 0 |  537 | `		return 0;` |
|         - |  538 | `	}` |
|         - |  539 | `	/* Zero the structure */` |
|   5059738 |  540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  541 | `	/* Initialize frame fields */` |
|   5059738 |  542 | `	pFrame->pUserData = pUserData;` |
|   5059738 |  543 | `	pFrame->pThis = pThis;` |
|   5059738 |  544 | `	pFrame->pVm = pVm;` |
|   5059738 |  545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   5059738 |  546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   5059738 |  547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   5059738 |  548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   5059738 |  549 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  550 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  551 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   5059738 |  552 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   5059738 |  553 | `	return pFrame;` |
|   2530034 |  554 | `}` |
|         - |  555 | `/* Forward declaration */` |
|         - |  556 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  557 | `/*` |
|         - |  558 | ` * Enter a VM frame.` |
|         - |  559 | ` */` |
|   5059113 |  560 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  561 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  562 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  563 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  564 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  565 | `	)` |
|         5 |  566 | `{` |
|         - |  567 | `	VmFrame *pFrame;` |
|         - |  568 | `	/* Allocate a new frame */` |
|   5059118 |  569 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   5059118 |  570 | `	if( pFrame == 0 ){` |
|       ! 0 |  571 | `		return SXERR_MEM;` |
|         - |  572 | `	}` |
|         - |  573 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   5059118 |  574 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  575 | `	/* Link to the list of active VM frame */` |
|   5059118 |  576 | `	pFrame->pParent = pVm->pFrame;` |
|   5059118 |  577 | `	pVm->pFrame = pFrame;` |
|   5059118 |  578 | `	if( ppFrame ){` |
|         - |  579 | `		/* Write a pointer to the new VM frame */` |
|   5054970 |  580 | `		*ppFrame = pFrame;` |
|   2527645 |  581 | `	}` |
|   5059118 |  582 | `	return SXRET_OK;` |
|   2529724 |  583 | `}` |
|         - |  584 | `/*` |
|         - |  585 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  586 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  587 | ` * information.` |
|         - |  588 | ` */` |
|        68 |  589 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  590 | `{` |
|         - |  591 | `	VmFrame *pTarget,*pFrame;` |
|        73 |  592 | `	SyHashEntry *pEntry = 0;` |
|         - |  593 | `	sxi32 rc;` |
|         - |  594 | `	/* Point to the upper frame */` |
|        73 |  595 | `	pFrame = pVm->pFrame;` |
|        73 |  596 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        73 |  597 | `	pTarget = pFrame;` |
|        73 |  598 | `	pFrame = pTarget->pParent;` |
|        73 |  599 | `	while( pFrame ){` |
|        73 |  600 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  601 | `			/* Query the current frame */` |
|        73 |  602 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|        73 |  603 | `			if( pEntry ){` |
|         - |  604 | `				/* Variable found */` |
|        73 |  605 | `				break;` |
|         - |  606 | `			}` |
|       ! 0 |  607 | `		}` |
|         - |  608 | `		/* Point to the upper frame */` |
|       ! 0 |  609 | `		pFrame = pFrame->pParent;` |
|       ! 0 |  610 | `	}` |
|        73 |  611 | `	if( pEntry == 0 ){` |
|         - |  612 | `		/* Inexistant variable */` |
|       ! 0 |  613 | `		return SXERR_NOTFOUND;` |
|         - |  614 | `	}` |
|         - |  615 | `	/* Link to the current frame */` |
|        73 |  616 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|        73 |  617 | `	if( rc == SXRET_OK ){` |
|         - |  618 | `		sxu32 nIdx;` |
|        73 |  619 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        73 |  620 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|        34 |  621 | `	}` |
|        73 |  622 | `	return rc;` |
|        39 |  623 | `}` |
|         - |  624 | `/*` |
|         - |  625 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  626 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  627 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  628 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  629 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  630 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  631 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  632 | ` */` |
|   5055415 |  633 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  634 | `{` |
|   5055420 |  635 | `	if( pVm->pResumeFrame == pFrame ){` |
|         3 |  636 | `		pVm->pResumeFrame = 0;` |
|         1 |  637 | `	}` |
|   5055420 |  638 | `}` |
|         - |  639 | `/*` |
|         - |  640 | ` * Leave the top-most active frame.` |
|         - |  641 | ` */` |
|   5054543 |  642 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  643 | `{` |
|   5054548 |  644 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   5054548 |  645 | `	if( pCurFrame ){` |
|         - |  646 | `		/* Unlink from the list of active VM frame */` |
|   5054548 |  647 | `		pVm->pFrame = pCurFrame->pParent;` |
|   5054548 |  648 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  649 | `			VmSlot  *aSlot;` |
|         - |  650 | `			sxu32 n;` |
|         - |  651 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   2180210 |  652 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   8928274 |  653 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|         - |  654 | `				/* Unset the local variable */` |
|   6748069 |  655 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   3375012 |  656 | `			}` |
|         - |  657 | `			/* Remove local reference */` |
|   2180210 |  658 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   8928372 |  659 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   6748167 |  660 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   3375061 |  661 | `			}` |
|   1090265 |  662 | `		}` |
|         - |  663 | `		/* Release internal containers */` |
|   5054548 |  664 | `		SyHashRelease(&pCurFrame->hVar);` |
|   5054548 |  665 | `		SySetRelease(&pCurFrame->sArg);` |
|   5054548 |  666 | `		SySetRelease(&pCurFrame->sLocal);` |
|   5054548 |  667 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  668 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  669 | `		 * containers above — released for every frame, including transparent` |
|         - |  670 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   5054548 |  671 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  672 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   5054548 |  673 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  674 | `		/* Release the whole structure */` |
|   5054548 |  675 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   2527434 |  676 | `	}` |
|   5054548 |  677 | `}` |
|         - |  678 | `/*` |
|         - |  679 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  680 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  681 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  682 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  683 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  684 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  685 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  686 | ` */` |
|         - |  687 | `/*` |
|         - |  688 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  689 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  690 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  691 | ` *` |
|         - |  692 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  693 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  694 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  695 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  696 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  697 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  698 | ` */` |
|      7076 |  699 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  700 | `{` |
|         - |  701 | `	VmFrame *pFrame;` |
|     14275 |  702 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|      7221 |  703 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  704 | `		sxu32 n;` |
|      7955 |  705 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|       760 |  706 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  707 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|        24 |  708 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|        24 |  709 | `				(void)SySetPop(&pFrame->sLocal);` |
|        24 |  710 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  711 | `			}` |
|       371 |  712 | `		}` |
|      3602 |  713 | `	}` |
|      7059 |  714 | `	return FALSE;` |
|      3543 |  715 | `}` |
|       104 |  716 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         3 |  717 | `{` |
|         - |  718 | `	VmRefObj *pRef;` |
|       107 |  719 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       107 |  720 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       107 |  721 | `	if( pRef ){` |
|       107 |  722 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|        52 |  723 | `	}` |
|       107 |  724 | `}` |
|         - |  725 | `/*` |
|         - |  726 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  727 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  728 | ` * should be skipped when looking for the real execution context.` |
|         - |  729 | ` */` |
|  58478258 |  730 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  731 | `{` |
|  71103593 |  732 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  12625335 |  733 | `		pFrame = pFrame->pParent;` |
|         5 |  734 | `	}` |
|  58478263 |  735 | `	return pFrame;` |
|         5 |  736 | `}` |
|         - |  737 | `/*` |
|         - |  738 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  739 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  740 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  741 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  742 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  743 | ` *` |
|         - |  744 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  745 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  746 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  747 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  748 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  749 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  750 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  751 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  752 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  753 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  754 | ` *` |
|         - |  755 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  756 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  757 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  758 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  759 | ` */` |
|   1947914 |  760 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  761 | `{` |
|   1947919 |  762 | `	if( pVm->pResumeFrame == 0 ){` |
|        17 |  763 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  764 | `	}` |
|         - |  765 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  766 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  767 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  768 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  769 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  770 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  771 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  772 | `	 * exec's resume site match and land. */` |
|   1947900 |  773 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1647300 |  774 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1346689 |  775 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  776 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  777 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  778 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  779 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  780 | `		 * never match a real frame. */` |
|    601237 |  781 | `		return FALSE;` |
|         - |  782 | `	}` |
|         - |  783 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  784 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  785 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  786 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  787 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  788 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  789 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  790 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  791 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  792 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  793 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  794 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  795 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  796 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  797 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  798 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  799 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  800 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  801 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  802 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  803 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|   2170567 |  804 | `	while( pVm->pFrame != pEntryFrame` |
|   2371204 |  805 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1547299 |  806 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|    301135 |  807 | `		VmLeaveFrame(&(*pVm));` |
|         5 |  808 | `	}` |
|   1346673 |  809 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|   1346673 |  810 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|         - |  811 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  812 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  813 | `	 * point router must not re-fire it after this resume. */` |
|   1346673 |  814 | `	pVm->nBoundaryRc = 0;` |
|   1346673 |  815 | `	return TRUE;` |
|    973962 |  816 | `}` |
|         - |  817 | `/*` |
|         - |  818 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  819 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  820 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  821 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  822 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  823 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  824 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  825 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  826 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  827 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  828 | ` */` |
|         - |  829 | `/*` |
|         - |  830 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  831 | ` *` |
|         - |  832 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  833 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  834 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  835 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  836 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  837 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  838 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  839 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  840 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  841 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  842 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  843 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  844 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  845 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  846 | ` * activation.` |
|         - |  847 | ` */` |
|   1447504 |  848 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  849 | `{` |
|   1447509 |  850 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1447509 |  851 | `	if( pClone == 0 ){` |
|       ! 0 |  852 | `		return 0;` |
|         - |  853 | `	}` |
|   1447509 |  854 | `	*pClone = *pCompiled;` |
|   1447509 |  855 | `	pClone->pCompiled = pCompiled;` |
|   1447509 |  856 | `	pClone->iFinallyDone = 0;` |
|   1447509 |  857 | `	pClone->iInCatch = 0;` |
|   1447509 |  858 | `	pClone->pInflight = 0;` |
|   1447509 |  859 | `	pClone->pFrame = 0;` |
|   1447509 |  860 | `	return pClone;` |
|    723757 |  861 | `}` |
|   2894850 |  862 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  863 | `{` |
|   2894855 |  864 | `	if( pExc && pExc->pCompiled ){` |
|         - |  865 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  866 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - |  867 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - |  868 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1447497 |  869 | `		if( pExc->pInflight ){` |
|       ! 0 |  870 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 |  871 | `			pExc->pInflight = 0;` |
|       ! 0 |  872 | `		}` |
|   1447497 |  873 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    723746 |  874 | `	}` |
|   2894855 |  875 | `}` |
|         - |  876 | `/*` |
|         - |  877 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - |  878 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - |  879 | ` */` |
|       566 |  880 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 |  881 | `{` |
|       571 |  882 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 |  883 | `}` |
|         - |  884 | `/*` |
|         - |  885 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - |  886 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - |  887 | ` * handlers). The set itself is reset by the caller.` |
|         - |  888 | ` */` |
|    100570 |  889 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 |  890 | `{` |
|    100575 |  891 | `	sxu32 n = SySetUsed(pSet);` |
|    100575 |  892 | `	if( n > 0 ){` |
|       ! 0 |  893 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - |  894 | `		sxu32 i;` |
|       ! 0 |  895 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 |  896 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 |  897 | `		}` |
|       ! 0 |  898 | `	}` |
|    100575 |  899 | `}` |
|         - |  900 | `/*` |
|         - |  901 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - |  902 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - |  903 | ` * only carries the compiled pointer.` |
|         - |  904 | ` */` |
|        70 |  905 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  906 | `{` |
|        75 |  907 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        75 |  908 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        75 |  909 | `	while( n > 0 ){` |
|        75 |  910 | `		n--;` |
|        75 |  911 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        75 |  912 | `			return ap[n];` |
|         - |  913 | `		}` |
|       ! 0 |  914 | `	}` |
|       ! 0 |  915 | `	return 0;` |
|        40 |  916 | `}` |
|  10518779 |  917 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 |  918 | `{` |
|         - |  919 | `	sxu32 nUsed;` |
|  10518784 |  920 | `	sxi32 rcOut = SXRET_OK;` |
|  10518852 |  921 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|        72 |  922 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        72 |  923 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|        72 |  924 | `		(void)SySetPop(&pVm->aException);` |
|        72 |  925 | `		pExc->pFrame = 0;` |
|         - |  926 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - |  927 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - |  928 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - |  929 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - |  930 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|        72 |  931 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        72 |  932 | `			VmLeaveFrame(&(*pVm));` |
|        34 |  933 | `		}` |
|        97 |  934 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - |  935 | `			sxi32 rcF;` |
|        54 |  936 | `			pExc->iFinallyDone = 1;` |
|        54 |  937 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        54 |  938 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        54 |  939 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 |  940 | `				return SXERR_ABORT;` |
|         - |  941 | `			}` |
|        54 |  942 | `			if( rcF == PH7_EXCEPTION ){` |
|         - |  943 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - |  944 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - |  945 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 |  946 | `				rcOut = PH7_EXCEPTION;` |
|         2 |  947 | `			}` |
|        29 |  948 | `		}else{` |
|        21 |  949 | `			VmExcRelease(&(*pVm),pExc);` |
|         - |  950 | `		}` |
|         4 |  951 | `	}` |
|  10518784 |  952 | `	return rcOut;` |
|   5259557 |  953 | `}` |
|         - |  954 | `/*` |
|         - |  955 | ` * Drop a body frame's pending catch/finally return: clear the flag and release` |
|         - |  956 | ` * the slot value. Safe on a frame with no pending return (the slot is then an` |
|         - |  957 | ` * empty MEMOBJ_NULL value and the release is a no-op).` |
|         - |  958 | ` */` |
|   8516122 |  959 | `PH7_PRIVATE void VmClearFrameReturn(VmFrame *pFrame)` |
|         5 |  960 | `{` |
|   8516127 |  961 | `	pFrame->bHasRet = 0;` |
|   8516127 |  962 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   8516127 |  963 | `}` |
|         - |  964 | `/*` |
|         - |  965 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - |  966 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - |  967 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - |  968 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - |  969 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - |  970 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - |  971 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - |  972 | ` */` |
|     20184 |  973 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 |  974 | `{` |
|     20189 |  975 | `	if( pResult ){` |
|     20189 |  976 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     10092 |  977 | `	}` |
|     20189 |  978 | `	VmClearFrameReturn(pEntryFrame);` |
|     20189 |  979 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|       ! 0 |  980 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 |  981 | `	}` |
|     20189 |  982 | `}` |
|         - |  983 | `/*` |
|         - |  984 | ` * Compare two functions signature and return the comparison result.` |
|         - |  985 | ` */` |
|      1186 |  986 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         2 |  987 | `{` |
|      1188 |  988 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      1188 |  989 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      1188 |  990 | `	const char *zSin = pSecond->zString;` |
|      1188 |  991 | `	const char *zFin = pFirst->zString;` |
|      1188 |  992 | `	const char *zPtr = zFin;` |
|       593 |  993 | `	for(;;){` |
|      1188 |  994 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|       595 |  995 | `			break;` |
|         - |  996 | `		}` |
|       ! 0 |  997 | `		if( zFin[0] != zSin[0] ){` |
|         - |  998 | `			/* mismatch */` |
|       ! 0 |  999 | `			break;` |
|         - | 1000 | `		}` |
|       ! 0 | 1001 | `		zFin++;` |
|       ! 0 | 1002 | `		zSin++;` |
|       ! 0 | 1003 | `	}` |
|      1188 | 1004 | `	return (int)(zFin-zPtr);` |
|         2 | 1005 | `}` |
|         - | 1006 | `/*` |
|         - | 1007 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1008 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1009 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1010 | ` * Refer to the official documentation for more information.` |
|         - | 1011 | ` */` |
|       264 | 1012 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1013 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1014 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1015 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1016 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1017 | `	)` |
|         4 | 1018 | `{` |
|         - | 1019 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1020 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1021 | `	ph7_vm_func *pLink;` |
|         - | 1022 | `	SyString sArgSig;` |
|         - | 1023 | `	SyBlob sSig;` |
|         - | 1024 |  |
|       268 | 1025 | `	pLink = pList;` |
|       268 | 1026 | `	i = 0;` |
|         - | 1027 | `	/* Put functions expecting the same number of passed arguments */` |
|      1670 | 1028 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      1622 | 1029 | `		if( pLink == 0 ){` |
|       219 | 1030 | `			break;` |
|         - | 1031 | `		}` |
|      1406 | 1032 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1033 | `			/* Candidate for overloading */` |
|      1406 | 1034 | `			apSet[i++] = pLink;` |
|       701 | 1035 | `		}` |
|         - | 1036 | `		/* Point to the next entry */` |
|      1406 | 1037 | `		pLink = pLink->pNextName;` |
|         4 | 1038 | `	}` |
|       268 | 1039 | `	if( i < 1 ){` |
|         - | 1040 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1041 | `		return pList;` |
|         - | 1042 | `	}` |
|       268 | 1043 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1044 | `		/* Return the only candidate */` |
|        40 | 1045 | `		return apSet[0];` |
|         - | 1046 | `	}` |
|         - | 1047 | `	/* Calculate function signature */` |
|       230 | 1048 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|       458 | 1049 | `	for( j = 0 ; j < nArg ; j++ ){` |
|       230 | 1050 | `		int c = 'n'; /* null */` |
|       230 | 1051 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1052 | `			/* Hashmap */` |
|       ! 0 | 1053 | `			c = 'h';` |
|       230 | 1054 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1055 | `			/* bool */` |
|        85 | 1056 | `			c = 'b';` |
|       188 | 1057 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1058 | `			/* int */` |
|        48 | 1059 | `			c = 'i';` |
|       122 | 1060 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1061 | `			/* String */` |
|        87 | 1062 | `			c = 's';` |
|        56 | 1063 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1064 | `			/* Float */` |
|        11 | 1065 | `			c = 'f';` |
|         8 | 1066 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1067 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1068 | `			int marker = 'o';` |
|       ! 0 | 1069 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1070 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1071 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1072 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1073 | `			c = -1;` |
|       ! 0 | 1074 | `		}` |
|       230 | 1075 | `		if( c > 0 ){` |
|       230 | 1076 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       114 | 1077 | `		}` |
|       116 | 1078 | `	}` |
|       230 | 1079 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|       230 | 1080 | `	iTarget = 0;` |
|       230 | 1081 | `	iMax = -1;` |
|         - | 1082 | `	/* Select the appropriate function */` |
|      1416 | 1083 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1084 | `		/* Compare the two signatures */` |
|      1188 | 1085 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      1188 | 1086 | `		if( iCur > iMax ){` |
|       230 | 1087 | `			iMax = iCur;` |
|       230 | 1088 | `			iTarget = j;` |
|       114 | 1089 | `		}` |
|       595 | 1090 | `	}` |
|       230 | 1091 | `	SyBlobRelease(&sSig);` |
|         - | 1092 | `	/* Appropriate function for the current call context */` |
|       230 | 1093 | `	return apSet[iTarget];` |
|       136 | 1094 | `}` |
|         - | 1095 | `/* Forward declaration */` |
|         - | 1096 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1097 | `/*` |
|         - | 1098 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|         - | 1099 | ` * safely across a pool reallocation.` |
|         - | 1100 | ` *` |
|         - | 1101 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1102 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|         - | 1103 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|         - | 1104 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|         - | 1105 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|         - | 1106 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|         - | 1107 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|         - | 1108 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|         - | 1109 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|         - | 1110 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|         - | 1111 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|         - | 1112 | ` */` |
|   2926208 | 1113 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1114 | `{` |
|         - | 1115 | `	ph7_value sVal;` |
|   2926213 | 1116 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1117 | `	sxi32 rc;` |
|   2926213 | 1118 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   2926213 | 1119 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1120 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|   2926213 | 1121 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   2926213 | 1122 | `	if( *ppMemObj ){` |
|   2926213 | 1123 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|   1463104 | 1124 | `	}` |
|   2926213 | 1125 | `	PH7_MemObjRelease(&sVal);` |
|   2926213 | 1126 | `	return rc;` |
|         5 | 1127 | `}` |
|         - | 1128 | `/*` |
|         - | 1129 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1130 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1131 | ` */` |
|         - | 1132 | `/*` |
|         - | 1133 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1134 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1135 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1136 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1137 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1138 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1139 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1140 | ` */` |
|    752930 | 1141 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1142 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1143 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1144 | `	)` |
|         5 | 1145 | `{` |
|         - | 1146 | `	ph7_class_attr *pAttr;` |
|         - | 1147 | `	SyHashEntry *pEntry;` |
|         - | 1148 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1149 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1150 | `	 * over both tables. */` |
|         - | 1151 | `	SyHash *apMount[2];` |
|         - | 1152 | `	int iMount;` |
|    752935 | 1153 | `	apMount[0] = &pClass->hAttr;` |
|    752935 | 1154 | `	apMount[1] = &pClass->hConst;` |
|   2258791 | 1155 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1156 | `	/* Reset the loop cursor */` |
|   1505865 | 1157 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1158 | `	/* Process only static and constant attribute */` |
|   3851463 | 1159 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1160 | `		/* Extract the current attribute */` |
|   2345607 | 1161 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   2345602 | 1162 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   1538061 | 1163 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    365697 | 1164 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1165 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1166 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1167 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1168 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1169 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1170 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1171 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1172 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1173 | `			 * %s as value for class constant" fatal without any access). */` |
|    729631 | 1174 | `			continue;` |
|         - | 1175 | `		}` |
|   1615981 | 1176 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1177 | `			ph7_value *pMemObj;` |
|     26557 | 1178 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1179 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1180 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1181 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1182 | `				 * deferred static-default type failure to THIS class too, so a` |
|         - | 1183 | `				 * subclass's static access / instantiation throws like php's. */` |
|       858 | 1184 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       858 | 1185 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         4 | 1186 | `					SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|         2 | 1187 | `						(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|         3 | 1188 | `					if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1189 | `						pClass->iFlags \|= PH7_CLASS_STATIC_TYPE_DEFER;` |
|         1 | 1190 | `					}` |
|         1 | 1191 | `				}` |
|       861 | 1192 | `				continue;` |
|         - | 1193 | `			}` |
|         - | 1194 | `			/* Reserve a memory object for this constant/static attribute */` |
|     25699 | 1195 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     25699 | 1196 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1197 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 1198 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 1199 | `					&pClass->sName,&pAttr->sName` |
|         - | 1200 | `					);` |
|       ! 0 | 1201 | `				return SXERR_MEM;` |
|         - | 1202 | `			}` |
|     25699 | 1203 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1204 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1205 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 1206 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|     25687 | 1207 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1208 | `				sxi32 rcExec;` |
|     25687 | 1209 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|     25687 | 1210 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|     25687 | 1211 | `				pVm->nConstEvalDepth++;` |
|     25687 | 1212 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|     25687 | 1213 | `				pVm->nConstEvalDepth--;` |
|     25687 | 1214 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|     25687 | 1215 | `				pVm->pConstEvalClass = pSaveCtx;` |
|     25687 | 1216 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1217 | `					/* The initializer raised (self-referencing constant, or a` |
|         - | 1218 | `					 * throwing enum-case reference): park it for the fetch-point` |
|         - | 1219 | `					 * router — user classes mount mid-execution, so the throw` |
|         - | 1220 | `					 * lands catchably at the declaration site. */` |
|       ! 0 | 1221 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|     25687 | 1222 | `				}else if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|         - | 1223 | `					/* A nested evaluation detected a self-referencing constant:` |
|         - | 1224 | `					 * raise it at this, the outermost level. */` |
|       ! 0 | 1225 | `					VmBoundaryPark(&(*pVm),VmConstCycleThrow(&(*pVm)));` |
|       ! 0 | 1226 | `				}` |
|         - | 1227 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 1228 | `				 * against the declared type. A mismatch is a non-catchable` |
|         - | 1229 | `				 * fatal, raised here at definition time (matching PHP). */` |
|     25682 | 1230 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|     12846 | 1231 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|        37 | 1232 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|        37 | 1233 | `					if( rcType != SXRET_OK ){` |
|         6 | 1234 | `						return rcType;` |
|         - | 1235 | `					}` |
|        15 | 1236 | `				}` |
|     12839 | 1237 | `			}` |
|         - | 1238 | `			/* Record attribute index */` |
|     25695 | 1239 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 1240 | `			/* Install static attribute in the reference table */` |
|     25695 | 1241 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1242 | `			/* If this is a typed static property, register the slot so the` |
|         - | 1243 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 1244 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 1245 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 1246 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 1247 | `			 * already enforced above, so they need no store-time slot. */` |
|     25690 | 1248 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     12888 | 1249 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        51 | 1250 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        51 | 1251 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 1252 | `					return SXERR_MEM;` |
|         - | 1253 | `				}` |
|        51 | 1254 | `				pVmAttrS->pAttr = pAttr;` |
|        51 | 1255 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        51 | 1256 | `				pVmAttrS->iState = 0;` |
|        51 | 1257 | `				pVmAttrS->pOwner = pClass;` |
|         - | 1258 | `				/* Static typed property with no default starts uninitialized` |
|         - | 1259 | `				 * (constants are already excluded by the enclosing condition). */` |
|        51 | 1260 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        11 | 1261 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|         7 | 1262 | `				}else{` |
|         - | 1263 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 1264 | `					 * typed static default LAZILY at the first static-property` |
|         - | 1265 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 1266 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 1267 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 1268 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 1269 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 1270 | `					 * throw via VmThrowDeferredStaticType. */` |
|        41 | 1271 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        19 | 1272 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        19 | 1273 | `						pClass->iFlags \|= PH7_CLASS_STATIC_TYPE_DEFER;` |
|         9 | 1274 | `					}` |
|         - | 1275 | `				}` |
|        51 | 1276 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 1277 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 1278 | `					return SXERR_MEM;` |
|         - | 1279 | `				}` |
|        23 | 1280 | `			}` |
|     12845 | 1281 | `		}` |
|         5 | 1282 | `	}` |
|    752933 | 1283 | `	} /* for iMount */` |
|    752931 | 1284 | `	return SXRET_OK;` |
|    376470 | 1285 | `}` |
|    751838 | 1286 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 1287 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1288 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 1289 | `	)` |
|         5 | 1290 | `{` |
|         - | 1291 | `	ph7_class_method *pMeth;` |
|         - | 1292 | `	SyHashEntry *pEntry;` |
|         - | 1293 | `	sxi32 rc;` |
|         - | 1294 | `	/* Reserve/initialize the static and constant attribute slots */` |
|    751843 | 1295 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|    751843 | 1296 | `	if( rc != SXRET_OK ){` |
|         6 | 1297 | `		return rc;` |
|         - | 1298 | `	}` |
|         - | 1299 | `	/* Install class methods */` |
|    751839 | 1300 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 1301 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 1302 | `		 */` |
|    327047 | 1303 | `		return SXRET_OK;` |
|         - | 1304 | `	}` |
|         - | 1305 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 1306 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 1307 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 1308 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 1309 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 1310 | `	/* Install the methods now */` |
|    424797 | 1311 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   6361349 | 1312 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   5724161 | 1313 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   5724161 | 1314 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   5713197 | 1315 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   5713197 | 1316 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1317 | `				return rc;` |
|         - | 1318 | `			}` |
|   2856596 | 1319 | `		}` |
|         5 | 1320 | `	}` |
|         - | 1321 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    424797 | 1322 | `	pClass->bMounted = TRUE;` |
|    424797 | 1323 | `	return SXRET_OK;` |
|    375924 | 1324 | `}` |
|         - | 1325 | `/*` |
|         - | 1326 | ` * Allocate a private frame for attributes of the given` |
|         - | 1327 | ` * class instance (Object in the PHP jargon).` |
|         - | 1328 | ` */` |
|   1554210 | 1329 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 1330 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 1331 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 1332 | `	)` |
|         5 | 1333 | `{` |
|   1554215 | 1334 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 1335 | `	ph7_class_attr *pAttr;` |
|         - | 1336 | `	SyHashEntry *pEntry;` |
|         - | 1337 | `	sxi32 rc;` |
|   1554215 | 1338 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 1339 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 1340 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 1341 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1554215 | 1342 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  10253283 | 1343 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 1344 | `		VmClassAttr *pVmAttr;` |
|         - | 1345 | `		/* Extract the current attribute */` |
|   8699073 | 1346 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   8699073 | 1347 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|   8699073 | 1348 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 1349 | `			return SXERR_MEM;` |
|         - | 1350 | `		}` |
|   8699073 | 1351 | `		pVmAttr->pAttr = pAttr;` |
|   8699073 | 1352 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 1353 | `			ph7_value *pMemObj;` |
|         - | 1354 | `			/* Reserve a memory object for this attribute */` |
|   8698811 | 1355 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|   8698811 | 1356 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1357 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1358 | `				return SXERR_MEM;` |
|         - | 1359 | `			}` |
|   8698811 | 1360 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|   8698811 | 1361 | `			pVmAttr->iState = 0;` |
|   8698811 | 1362 | `			pVmAttr->pOwner = pClass;` |
|   8698811 | 1363 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1364 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1365 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 1366 | `				 * against the declaring class (no method frame here). */` |
|   2900195 | 1367 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1368 | `				sxi32 rcExec;` |
|   2900195 | 1369 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   2900195 | 1370 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|   2900195 | 1371 | `				pVm->pConstEvalClass = pSaveCtx;` |
|   2900195 | 1372 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1373 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 1374 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 1375 | `					 * also type-check the leftover value (a spurious second` |
|         - | 1376 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 1377 | `					 * nothing further for the remaining attributes. */` |
|         3 | 1378 | `					bDefThrew = 1;` |
|   2900194 | 1379 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1380 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 1381 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 1382 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 1383 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 1384 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 1385 | `					 * aborts construction) / the fetch-point router. */` |
|       255 | 1386 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       255 | 1387 | `					if( rcDef != SXRET_OK ){` |
|        13 | 1388 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 1389 | `						bDefThrew = 1;` |
|         6 | 1390 | `					}` |
|       130 | 1391 | `				}` |
|   7248716 | 1392 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 1393 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 1394 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       445 | 1395 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       220 | 1396 | `			}` |
|   8698811 | 1397 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|   8698811 | 1398 | `			if( rc != SXRET_OK ){` |
|         - | 1399 | `				VmSlot sSlot;` |
|         - | 1400 | `				/* Restore memory object */` |
|       ! 0 | 1401 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1402 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1403 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1404 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1405 | `				return SXERR_MEM;` |
|         - | 1406 | `			}` |
|         - | 1407 | `			/* Install attribute in the reference table */` |
|   8698811 | 1408 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1409 | `			/* Register typed property slot for assignment-time enforcement.` |
|         - | 1410 | `			 * On failure roll back the just-installed hAttr entry and the` |
|         - | 1411 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|   8698811 | 1412 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       699 | 1413 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|       699 | 1414 | `				if( rc != SXRET_OK ){` |
|         - | 1415 | `					VmSlot sSlot;` |
|       ! 0 | 1416 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1417 | `					sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1418 | `					sSlot.pUserData = 0;` |
|       ! 0 | 1419 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1420 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1421 | `					return SXERR_MEM;` |
|         - | 1422 | `				}` |
|       347 | 1423 | `			}` |
|   4349408 | 1424 | `		}else{` |
|         - | 1425 | `			/* Install static/constant attribute */` |
|       265 | 1426 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       265 | 1427 | `			pVmAttr->iState = 0;` |
|       265 | 1428 | `			pVmAttr->pOwner = pClass;` |
|       265 | 1429 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       265 | 1430 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1431 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1432 | `				return SXERR_MEM;` |
|         - | 1433 | `			}` |
|         - | 1434 | `		}` |
|         5 | 1435 | `	}` |
|   1554215 | 1436 | `	return SXRET_OK;` |
|    777110 | 1437 | `}` |
|         - | 1438 | `/*` |
|         - | 1439 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|         - | 1440 | ` * stdClass for now; the future general-dynamic-props work turns` |
|         - | 1441 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|         - | 1442 | ` */` |
|        70 | 1443 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         4 | 1444 | `{` |
|        74 | 1445 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|         4 | 1446 | `}` |
|         - | 1447 | `/*` |
|         - | 1448 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 1449 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 1450 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 1451 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 1452 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 1453 | ` */` |
|         4 | 1454 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         2 | 1455 | `{` |
|        10 | 1456 | `	while( pClass ){` |
|         6 | 1457 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 1458 | `		sxu32 n;` |
|         6 | 1459 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|       ! 0 | 1460 | `			if( aAttr[n].sName.nByte == nName` |
|       ! 0 | 1461 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|       ! 0 | 1462 | `				return TRUE;` |
|         - | 1463 | `			}` |
|       ! 0 | 1464 | `		}` |
|         6 | 1465 | `		pClass = pClass->pBase;` |
|         2 | 1466 | `	}` |
|         6 | 1467 | `	return FALSE;` |
|         4 | 1468 | `}` |
|         - | 1469 | `/*` |
|         - | 1470 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 1471 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 1472 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 1473 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 1474 | ` *` |
|         - | 1475 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 1476 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 1477 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 1478 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 1479 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 1480 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 1481 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 1482 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 1483 | ` */` |
|       158 | 1484 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         4 | 1485 | `{` |
|         - | 1486 | `	ph7_class_attr *pAttr;` |
|       162 | 1487 | `	VmClassAttr *pVmAttr = 0;` |
|       162 | 1488 | `	ph7_value *pMemObj = 0;` |
|         - | 1489 | `	char *zCopy;` |
|         - | 1490 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       162 | 1491 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       162 | 1492 | `	if( pAttr == 0 ){` |
|       ! 0 | 1493 | `		return 0;` |
|         - | 1494 | `	}` |
|       162 | 1495 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       162 | 1496 | `	zCopy = (char *)&pAttr[1];` |
|       162 | 1497 | `	if( nName > 0 ){` |
|       162 | 1498 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|        79 | 1499 | `	}` |
|       162 | 1500 | `	zCopy[nName] = 0;` |
|       162 | 1501 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       162 | 1502 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       162 | 1503 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       162 | 1504 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 1505 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 1506 | `	 * value, never a union. */` |
|       162 | 1507 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       162 | 1508 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1509 | `		goto fail_attr;` |
|         - | 1510 | `	}` |
|       162 | 1511 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       162 | 1512 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1513 | `		goto fail_vmattr;` |
|         - | 1514 | `	}` |
|       162 | 1515 | `	pVmAttr->pAttr = pAttr;` |
|       162 | 1516 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       162 | 1517 | `	pVmAttr->iState = 0;` |
|       162 | 1518 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1519 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 1520 | `	 * property-creation order, matching PHP. */` |
|       162 | 1521 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 1522 | `		goto fail_slot;` |
|         - | 1523 | `	}` |
|         - | 1524 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       162 | 1525 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|       162 | 1526 | `	if( ppAttr ){` |
|        83 | 1527 | `		*ppAttr = pVmAttr;` |
|        40 | 1528 | `	}` |
|       162 | 1529 | `	return pMemObj;` |
|       ! 0 | 1530 | `fail_slot:` |
|         - | 1531 | `	{` |
|         - | 1532 | `		VmSlot sSlot;` |
|       ! 0 | 1533 | `		sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1534 | `		sSlot.pUserData = 0;` |
|       ! 0 | 1535 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1536 | `	}` |
|       ! 0 | 1537 | `fail_vmattr:` |
|       ! 0 | 1538 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1539 | `fail_attr:` |
|       ! 0 | 1540 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 1541 | `	return 0;` |
|        83 | 1542 | `}` |
|         - | 1543 | `/*` |
|         - | 1544 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 1545 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 1546 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 1547 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 1548 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 1549 | ` */` |
|         6 | 1550 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         1 | 1551 | `{` |
|         - | 1552 | `	VmClassAttr *pVmAttr;` |
|         - | 1553 | `	ph7_value *pMemObj;` |
|         7 | 1554 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|         7 | 1555 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1556 | `		return;` |
|         - | 1557 | `	}` |
|         7 | 1558 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|         7 | 1559 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1560 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1561 | `		return;` |
|         - | 1562 | `	}` |
|         7 | 1563 | `	pVmAttr->pAttr = pAttr;` |
|         7 | 1564 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|         7 | 1565 | `	pVmAttr->iState = 0;` |
|         7 | 1566 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1567 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 1568 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 1569 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 1570 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 1571 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|         7 | 1572 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       ! 0 | 1573 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       ! 0 | 1574 | `	}` |
|         - | 1575 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 1576 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 1577 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 1578 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|         7 | 1579 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|         - | 1580 | `		VmSlot sSlot;` |
|       ! 0 | 1581 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1582 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1583 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1584 | `		return;` |
|         - | 1585 | `	}` |
|         7 | 1586 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         7 | 1587 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       ! 0 | 1588 | `		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|         - | 1589 | `			VmSlot sSlot;` |
|       ! 0 | 1590 | `			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1591 | `			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1592 | `			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1593 | `			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1594 | `			return;` |
|         - | 1595 | `		}` |
|       ! 0 | 1596 | `	}` |
|         7 | 1597 | `	if( ppAttr ){` |
|         7 | 1598 | `		*ppAttr = pVmAttr;` |
|         3 | 1599 | `	}` |
|         4 | 1600 | `}` |
|         - | 1601 | `/* Forward declaration */` |
|         - | 1602 | `/*` |
|         - | 1603 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 1604 | ` */` |
|         - | 1605 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 1606 | `/*` |
|         - | 1607 | ` * Reserve a constant memory object.` |
|         - | 1608 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 1609 | ` */` |
|   9232772 | 1610 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 1611 | `{` |
|         - | 1612 | `	ph7_value *pObj;` |
|         - | 1613 | `	sxi32 rc;` |
|   9232777 | 1614 | `	if( pIndex ){` |
|         - | 1615 | `		/* Object index in the object table */` |
|   9220357 | 1616 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   4610176 | 1617 | `	}` |
|         - | 1618 | `	/* Reserve a slot for the new object */` |
|   9232777 | 1619 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   9232777 | 1620 | `	if( rc != SXRET_OK ){` |
|         - | 1621 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1622 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1623 | `		 */` |
|       ! 0 | 1624 | `		return 0;` |
|         - | 1625 | `	}` |
|   9232777 | 1626 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   9232777 | 1627 | `	return pObj;` |
|   4616391 | 1628 | `}` |
|         - | 1629 | `/*` |
|         - | 1630 | ` * Reserve a memory object.` |
|         - | 1631 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 1632 | ` */` |
|   2849950 | 1633 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 1634 | `{` |
|         - | 1635 | `	ph7_value *pObj;` |
|         - | 1636 | `	sxi32 rc;` |
|   2849955 | 1637 | `	if( pIndex ){` |
|         - | 1638 | `		/* Object index in the object table */` |
|   2849955 | 1639 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|   1424975 | 1640 | `	}` |
|         - | 1641 | `	/* Reserve a slot for the new object */` |
|   2849955 | 1642 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|   2849955 | 1643 | `	if( rc != SXRET_OK ){` |
|         - | 1644 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1645 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1646 | `		 */` |
|       ! 0 | 1647 | `		return 0;` |
|         - | 1648 | `	}` |
|   2849955 | 1649 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|   2849955 | 1650 | `	return pObj;` |
|   1424980 | 1651 | `}` |
|         - | 1652 | `/* Forward declaration */` |
|         - | 1653 | `/* Forward declarations for Fiber C functions */` |
|         - | 1654 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 1655 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 1656 | `/*` |
|         - | 1657 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 1658 | ` * directly as foreign functions.` |
|         - | 1659 | ` */` |
|         - | 1660 |  |
|         - | 1661 | `/*` |
|         - | 1662 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 1663 | ` * start compiling the target PHP program.` |
|         - | 1664 | ` */` |
|      4140 | 1665 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 1666 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 1667 | `	 ph7 *pEngine /* Master engine */` |
|         - | 1668 | `	 )` |
|         5 | 1669 | `{` |
|         - | 1670 | `	ph7_value *pObj;` |
|         - | 1671 | `	sxi32 rc;` |
|         - | 1672 | `	/* Zero the structure */` |
|      4145 | 1673 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 1674 | `	/* Initialize VM fields */` |
|      4145 | 1675 | `	pVm->pEngine = &(*pEngine);` |
|      4145 | 1676 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|         - | 1677 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 1678 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 1679 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      4145 | 1680 | `	pVm->bDisplayErrors = 0;` |
|      4145 | 1681 | `	pVm->bLogErrors = 1;` |
|      4145 | 1682 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 1683 | `	/* Instructions containers */` |
|      4145 | 1684 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      4145 | 1685 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      4145 | 1686 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 1687 | `	/* Object containers */` |
|      4145 | 1688 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      4145 | 1689 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|         - | 1690 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      4145 | 1691 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      4145 | 1692 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      4145 | 1693 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      4145 | 1694 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 1695 | `	/* Virtual machine internal containers */` |
|      4145 | 1696 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      4145 | 1697 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      4145 | 1698 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      4145 | 1699 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|      4145 | 1700 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      4145 | 1701 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 1702 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 1703 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 1704 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 1705 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 1706 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 1707 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 1708 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 1709 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      4145 | 1710 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4145 | 1711 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4145 | 1712 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|      4145 | 1713 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|      4145 | 1714 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|      4145 | 1715 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      4145 | 1716 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      4145 | 1717 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      4145 | 1718 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      4145 | 1719 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|      4145 | 1720 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      4145 | 1721 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      4145 | 1722 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      4145 | 1723 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      4145 | 1724 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      4145 | 1725 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      4145 | 1726 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      4145 | 1727 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      4145 | 1728 | `	pVm->nResourceIdNext = 1;` |
|      4145 | 1729 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      4145 | 1730 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      4145 | 1731 | `	pVm->pMagicSetThis = 0;` |
|      4145 | 1732 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      4145 | 1733 | `	pVm->pHookSetThis = 0;` |
|      4145 | 1734 | `	pVm->pHookSetAttr = 0;` |
|      4145 | 1735 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      4145 | 1736 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      4145 | 1737 | `	pVm->pMagicCallThis = 0;` |
|      4145 | 1738 | `	pVm->pMagicCallClass = 0;` |
|      4145 | 1739 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      4145 | 1740 | `	pVm->pIdleCallFrames = 0;` |
|      4145 | 1741 | `	pVm->pIdleOperandStacks = 0;` |
|      4145 | 1742 | `	pVm->nIdleOperandStacks = 0;` |
|      4145 | 1743 | `	pVm->pIdleStackNodes = 0;` |
|      4145 | 1744 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      4145 | 1745 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      4145 | 1746 | `	pVm->pPendingException = 0;` |
|      4145 | 1747 | `	pVm->pInflightException = 0;` |
|      4145 | 1748 | `	pVm->nInflightExcBase = 0;` |
|      4145 | 1749 | `	pVm->pResumeFrame = 0;` |
|      4145 | 1750 | `	pVm->iResumePc = 0;` |
|      4145 | 1751 | `	pVm->pResumeInstr = 0;` |
|      4145 | 1752 | `	pVm->iResumeStackDepth = 0;` |
|      4145 | 1753 | `	pVm->nBoundaryRc = 0;` |
|      4145 | 1754 | `	pVm->pConstEvalClass = 0;` |
|      4145 | 1755 | `	pVm->nConstEvalDepth = 0;` |
|      4145 | 1756 | `	pVm->pConstCycleAttr = 0;` |
|      4145 | 1757 | `	pVm->pConstCycleClass = 0;` |
|      4145 | 1758 | `	SySetReset(&pVm->aMagicGuard);` |
|      4145 | 1759 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 1760 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 1761 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 1762 | `	}` |
|      4145 | 1763 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      4145 | 1764 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 1765 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 1766 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 1767 | `	}` |
|      4145 | 1768 | `	pVm->pHookSetAttr = 0;` |
|      4145 | 1769 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      4145 | 1770 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 1771 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 1772 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 1773 | `	}` |
|      4145 | 1774 | `	pVm->pMagicCallClass = 0;` |
|      4145 | 1775 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 1776 | `	/* Configuration containers */` |
|      4145 | 1777 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      4145 | 1778 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      4145 | 1779 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      4145 | 1780 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      4145 | 1781 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|      4145 | 1782 | `	pVm->iResponseStatus = 200;` |
|      4145 | 1783 | `	pVm->bHeadersSent = 0;` |
|      4145 | 1784 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 1785 | `	/* Error callbacks containers */` |
|      4145 | 1786 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|      4145 | 1787 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|      4145 | 1788 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|      4145 | 1789 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|      4145 | 1790 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 1791 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 1792 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 1793 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 1794 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 1795 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 1796 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 1797 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      4145 | 1798 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      4145 | 1799 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 1800 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 1801 | `#else` |
|         - | 1802 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 1803 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 1804 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 1805 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 1806 | `	pVm->nMaxDepth = 512;` |
|         - | 1807 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 1808 | `#endif` |
|         - | 1809 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 1810 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 1811 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      4145 | 1812 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 1813 | `	/* JSON return status */` |
|      4145 | 1814 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 1815 | `	/* PRNG context */` |
|      4145 | 1816 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 1817 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 1818 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      4145 | 1819 | `	pVm->mtSeeded = FALSE;` |
|         - | 1820 | `	/* Install the null constant */` |
|      4145 | 1821 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4145 | 1822 | `	if( pObj == 0 ){` |
|       ! 0 | 1823 | `		rc = SXERR_MEM;` |
|       ! 0 | 1824 | `		goto Err;` |
|         - | 1825 | `	}` |
|      4145 | 1826 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 1827 | `	/* Install the boolean TRUE constant */` |
|      4145 | 1828 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4145 | 1829 | `	if( pObj == 0 ){` |
|       ! 0 | 1830 | `		rc = SXERR_MEM;` |
|       ! 0 | 1831 | `		goto Err;` |
|         - | 1832 | `	}` |
|      4145 | 1833 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 1834 | `	/* Install the boolean FALSE constant */` |
|      4145 | 1835 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      4145 | 1836 | `	if( pObj == 0 ){` |
|       ! 0 | 1837 | `		rc = SXERR_MEM;` |
|       ! 0 | 1838 | `		goto Err;` |
|         - | 1839 | `	}` |
|      4145 | 1840 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 1841 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 1842 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 1843 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      4145 | 1844 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      4145 | 1845 | `	if( pObj == 0 ){` |
|       ! 0 | 1846 | `		rc = SXERR_MEM;` |
|       ! 0 | 1847 | `		goto Err;` |
|         - | 1848 | `	}` |
|      4145 | 1849 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 1850 | `	/* Create the global frame */` |
|      4145 | 1851 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      4145 | 1852 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1853 | `		goto Err;` |
|         - | 1854 | `	}` |
|         - | 1855 | `	/* Initialize the code generator */` |
|      4145 | 1856 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      4145 | 1857 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1858 | `		goto Err;` |
|         - | 1859 | `	}` |
|         - | 1860 | `	/* VM correctly initialized,set the magic number */` |
|      4145 | 1861 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 1862 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 1863 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      4145 | 1864 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 1865 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      4145 | 1866 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 1867 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 1868 | `	 * compiled — its classes are internal too. */` |
|         - | 1869 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|      4145 | 1870 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 1871 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      4145 | 1872 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      4145 | 1873 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      4145 | 1874 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      4145 | 1875 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      4145 | 1876 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 1877 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      4145 | 1878 | `	pVm->pCoalesceObj = 0;` |
|      4145 | 1879 | `	pVm->bCoalesceArmed = 0;` |
|      4145 | 1880 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 1881 | `	/* Register Fiber internal C functions */` |
|      4145 | 1882 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|      4145 | 1883 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|      4145 | 1884 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|      4145 | 1885 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|      4145 | 1886 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|      4145 | 1887 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|      4145 | 1888 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|      4145 | 1889 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|      4145 | 1890 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|      4145 | 1891 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|         - | 1892 | `	/* Cache the Closure class pointer (closures are instances of it) */` |
|      4145 | 1893 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      4145 | 1894 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      4145 | 1895 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 1896 | `	/* Closure::bind/bindTo/call/fromCallable native delegates (Increment 2) */` |
|      4145 | 1897 | `	ph7_create_function(pVm,"__closure_bindTo",vm_builtin_Closure_bindTo,0);` |
|      4145 | 1898 | `	ph7_create_function(pVm,"__closure_fromCallable",vm_builtin_Closure_fromCallable,0);` |
|         - | 1899 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      4145 | 1900 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 1901 | `	/* Cache the Generator class pointer and register generator functions */` |
|      4145 | 1902 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|      4145 | 1903 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|      4145 | 1904 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|      4145 | 1905 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|      4145 | 1906 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|      4145 | 1907 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|      4145 | 1908 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|      4145 | 1909 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|      4145 | 1910 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|      4145 | 1911 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|         - | 1912 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 1913 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 1914 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      4145 | 1915 | `	PH7_VmInstallReflection(&(*pVm));` |
|      4145 | 1916 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      4145 | 1917 | `	PH7_VmInstallSpl(&(*pVm));` |
|      4145 | 1918 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|      4145 | 1919 | `	PH7_VmInstallSession(&(*pVm));` |
|      4145 | 1920 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 1921 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 1922 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|         - | 1923 | `	 * XMLWriter class libraries that build on it. */` |
|      4145 | 1924 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      4145 | 1925 | `	PH7_VmInstallDom(&(*pVm));` |
|      4145 | 1926 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 1927 | `#endif` |
|      4145 | 1928 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 1929 | `	/* Reset the code generator */` |
|      4145 | 1930 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      4145 | 1931 | `	return SXRET_OK;` |
|       ! 0 | 1932 | `Err:` |
|       ! 0 | 1933 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 1934 | `	return rc;` |
|      2075 | 1935 | `}` |
|         - | 1936 | `/*` |
|         - | 1937 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 1938 | ` * routine which store the output in an internal blob.` |
|         - | 1939 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 1940 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 1941 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 1942 | ` * Refer to the official docurmentation for additional information.` |
|         - | 1943 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 1944 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 1945 | ` * to finish executing and extracting the output.` |
|         - | 1946 | ` */` |
|        68 | 1947 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 1948 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 1949 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 1950 | `	void *pUserData     /* User private data */` |
|         - | 1951 | `	)` |
|       ! 0 | 1952 | `{` |
|         - | 1953 | `	 sxi32 rc;` |
|         - | 1954 | `	 /* Store the output in an internal BLOB */` |
|        68 | 1955 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|        68 | 1956 | `	 return rc;` |
|       ! 0 | 1957 | `}` |
|         - | 1958 | `/*` |
|         - | 1959 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 1960 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 1961 | ` */` |
|     47826 | 1962 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 1963 | `{` |
|     47831 | 1964 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|     47831 | 1965 | `	if( xCons != VmObConsumer ){` |
|     14811 | 1966 | `		pVm->nOutputLen += nLen;` |
|     14811 | 1967 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|       991 | 1968 | `			pVm->bHeadersSent = 1;` |
|       493 | 1969 | `		}` |
|      7403 | 1970 | `	}` |
|     47831 | 1971 | `}` |
|         - | 1972 | `/*` |
|         - | 1973 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 1974 | ` *` |
|         - | 1975 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 1976 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 1977 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 1978 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 1979 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 1980 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 1981 | ` *` |
|         - | 1982 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 1983 | ` * conservative BY CONSTRUCTION:` |
|         - | 1984 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 1985 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 1986 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 1987 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 1988 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 1989 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 1990 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 1991 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 1992 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 1993 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 1994 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 1995 | ` *     instruction-count bound -> fallback.` |
|         - | 1996 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 1997 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 1998 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 1999 | ` *` |
|         - | 2000 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 2001 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 2002 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 2003 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 2004 | ` *` |
|         - | 2005 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 2006 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 2007 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 2008 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 2009 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 2010 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 2011 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 2012 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 2013 | ` * entry here.` |
|         - | 2014 | ` */` |
|         - | 2015 | `/*` |
|         - | 2016 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 2017 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 2018 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 2019 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 2020 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 2021 | ` */` |
|     59226 | 2022 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 2023 | `{` |
|     59231 | 2024 | `	int push = 0, n = 0;` |
|         - | 2025 | `	sxi32 d;` |
|     59231 | 2026 | `	switch( pI->iOp ){` |
|         - | 2027 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 2028 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|      9531 | 2029 | `	case PH7_OP_LOADC:` |
|         - | 2030 | `	case PH7_OP_DUP:` |
|     19067 | 2031 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      6471 | 2032 | `	case PH7_OP_LOAD:` |
|     12947 | 2033 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|     12947 | 2034 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|       ! 0 | 2035 | `	case PH7_OP_LOAD_REF:` |
|       ! 0 | 2036 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2037 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       387 | 2038 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 2039 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 2040 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|       779 | 2041 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 2042 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 2043 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       307 | 2044 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 2045 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       619 | 2046 | `		if( pI->iP2 == 0 ){` |
|       619 | 2047 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       312 | 2048 | `		}else{` |
|       ! 0 | 2049 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 2050 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 2051 | `		}` |
|       619 | 2052 | `		break;` |
|         - | 2053 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 2054 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       193 | 2055 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 2056 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 2057 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 2058 | `	case PH7_OP_NOOP:` |
|       391 | 2059 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2060 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 2061 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|       882 | 2062 | `	case PH7_OP_STORE:` |
|      1769 | 2063 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      1769 | 2064 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 2065 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1396 | 2066 | `	case PH7_OP_POP:` |
|         - | 2067 | `	case PH7_OP_CONSUME:` |
|      2797 | 2068 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2069 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 2070 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 2071 | `	 * true pop count is a runtime value — never reaches here. */` |
|      2336 | 2072 | `	case PH7_OP_CALL:` |
|      4677 | 2073 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2074 | `	/* Jumps. */` |
|       165 | 2075 | `	case PH7_OP_JMP:` |
|       335 | 2076 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       909 | 2077 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|      1823 | 2078 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|      1823 | 2079 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 2080 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      3648 | 2081 | `	case PH7_OP_DONE:` |
|      7301 | 2082 | `		n = 0; break;` |
|      3388 | 2083 | `	default:` |
|      6781 | 2084 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 2085 | `	}` |
|     52455 | 2086 | `	*pPush = push; *pN = n;` |
|     52455 | 2087 | `	return 1;` |
|     29618 | 2088 | `}` |
|         - | 2089 | `/*` |
|         - | 2090 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 2091 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 2092 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 2093 | ` */` |
|      9244 | 2094 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 2095 | `{` |
|         - | 2096 | `	void *pScratch;` |
|         - | 2097 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 2098 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 2099 | `	sxi32 iMax;` |
|         - | 2100 | `	int push, n, k;` |
|         - | 2101 | `	sxu32 succ[2]; sxi32 delta[2];` |
|      9249 | 2102 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 2103 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 2104 | `		return VM_STACK_UNMODELED;` |
|         - | 2105 | `	}` |
|         - | 2106 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     50683 | 2107 | `	for( i = 0; i < nInstr; i++ ){` |
|     48215 | 2108 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|      6781 | 2109 | `			return VM_STACK_UNMODELED;` |
|         - | 2110 | `		}` |
|     20722 | 2111 | `	}` |
|         - | 2112 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 2113 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 2114 | `	 * first (the byte array last needs no alignment). */` |
|      2473 | 2115 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      2473 | 2116 | `	if( pScratch == 0 ){` |
|       ! 0 | 2117 | `		return VM_STACK_UNMODELED;` |
|         - | 2118 | `	}` |
|      2473 | 2119 | `	aH  = (sxi32 *)pScratch;` |
|      2473 | 2120 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      2473 | 2121 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     15587 | 2122 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      2473 | 2123 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      2473 | 2124 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|     13489 | 2125 | `	while( nQ > 0 ){` |
|     11021 | 2126 | `		sxu32 pc = aQ[--nQ];` |
|         - | 2127 | `		sxi32 h;` |
|     11021 | 2128 | `		aIn[pc] = 0;` |
|     11021 | 2129 | `		h = aH[pc];` |
|     11021 | 2130 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     11021 | 2131 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     11021 | 2132 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     11021 | 2133 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     19663 | 2134 | `		for( k = 0; k < n; k++ ){` |
|      8647 | 2135 | `			sxi32 hn = h + delta[k];` |
|      8647 | 2136 | `			sxu32 t = succ[k];` |
|      8647 | 2137 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      8647 | 2138 | `			if( hn > aH[t] ){` |
|      8553 | 2139 | `				aH[t] = hn;` |
|      8553 | 2140 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      4274 | 2141 | `			}` |
|      4326 | 2142 | `		}` |
|     11021 | 2143 | `		if( iMax < 0 ){ break; }` |
|         5 | 2144 | `	}` |
|      2473 | 2145 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      2473 | 2146 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|      4627 | 2147 | `}` |
|         - | 2148 | `/*` |
|         - | 2149 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 2150 | ` * our compiled PHP program.` |
|         - | 2151 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 2152 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 2153 | ` *` |
|         - | 2154 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 2155 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 2156 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 2157 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 2158 | ` */` |
|  10260634 | 2159 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 2160 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2161 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 2162 | `	)` |
|         5 | 2163 | `{` |
|         - | 2164 | `	ph7_value *pStack;` |
|         - | 2165 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 2166 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 2167 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 2168 | `  ** on the maximum stack depth required.` |
|         - | 2169 | `  **` |
|         - | 2170 | `  ** Allocation all the stack space we will ever need.` |
|         - | 2171 | `  */` |
|  10260639 | 2172 | `	nInstr += VM_STACK_GUARD;` |
|  10260639 | 2173 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|  10260639 | 2174 | `	if( pStack == 0 ){` |
|       ! 0 | 2175 | `		return 0;` |
|         - | 2176 | `	}` |
|         - | 2177 | `	/* Initialize the operand stack */` |
| 237685307 | 2178 | `	while( nInstr > 0 ){` |
| 227424673 | 2179 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
| 227424673 | 2180 | `		--nInstr;` |
|         5 | 2181 | `	}` |
|         - | 2182 | `	/* Ready for bytecode execution */` |
|  10260639 | 2183 | `	return pStack;` |
|   5130483 | 2184 | `}` |
|         - | 2185 | `/*` |
|         - | 2186 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 2187 | ` *` |
|         - | 2188 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 2189 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 2190 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 2191 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 2192 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 2193 | ` * the per-slot PH7_MemObjInit.` |
|         - | 2194 | ` *` |
|         - | 2195 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 2196 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 2197 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 2198 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|         - | 2199 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|         - | 2200 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|         - | 2201 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|         - | 2202 | ` *` |
|         - | 2203 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|         - | 2204 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|         - | 2205 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|         - | 2206 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|         - | 2207 | ` * recursion case is the one worth the O(1) simplicity.` |
|         - | 2208 | ` */` |
|         - | 2209 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 2210 | `struct VmIdleStack {` |
|         - | 2211 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 2212 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 2213 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 2214 | `};` |
|         - | 2215 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|         - | 2216 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|         - | 2217 | `                                    * (a large fallback-sized stack recursing would` |
|         - | 2218 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|         - | 2219 | `                                    * the tight-sized hot case is far below this) */` |
|         - | 2220 | `/*` |
|         - | 2221 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 2222 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 2223 | ` */` |
|   2180377 | 2224 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 2225 | `{` |
|   2180382 | 2226 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   2180382 | 2227 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|   2180382 | 2228 | `	if( pIdle && pIdle->nCap == nCap ){` |
|   1261518 | 2229 | `		ph7_value *pStack = pIdle->pStack;` |
|   1261518 | 2230 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|   1261518 | 2231 | `		pVm->nIdleOperandStacks--;` |
|         - | 2232 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 2233 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|   1261518 | 2234 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|   1261518 | 2235 | `		pVm->pIdleStackNodes = pIdle;` |
|   1261518 | 2236 | `		return pStack; /* slots already released -> reusable without re-init */` |
|         - | 2237 | `	}` |
|    918869 | 2238 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|   1090356 | 2239 | `}` |
|         - | 2240 | `/*` |
|         - | 2241 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 2242 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|         - | 2243 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|         - | 2244 | ` * live value.` |
|         - | 2245 | ` */` |
|   2179967 | 2246 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|         5 | 2247 | `{` |
|         - | 2248 | `	VmIdleStack *pIdle;` |
|         - | 2249 | `	sxu32 i;` |
|   2179972 | 2250 | `	if( pStack == 0 ){` |
|       ! 0 | 2251 | `		return;` |
|         - | 2252 | `	}` |
|   2179972 | 2253 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|    911115 | 2254 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|    911115 | 2255 | `		return;` |
|         - | 2256 | `	}` |
|         - | 2257 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 2258 | `	 * pool-allocate only when the spare list is empty. */` |
|   1268862 | 2259 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|   1268862 | 2260 | `	if( pIdle ){` |
|   1261518 | 2261 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    630763 | 2262 | `	}else{` |
|      7349 | 2263 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      7349 | 2264 | `		if( pIdle == 0 ){` |
|       ! 0 | 2265 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 2266 | `			return;` |
|         - | 2267 | `		}` |
|         - | 2268 | `	}` |
|  42985483 | 2269 | `	for( i = 0; i < nCap; i++ ){` |
|  41716626 | 2270 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 2271 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 2272 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 2273 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 2274 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 2275 | `		 * across invocations — cheap defense in depth. */` |
|  41716626 | 2276 | `		pStack[i].nIdx = SXU32_HIGH;` |
|  20858896 | 2277 | `	}` |
|   1268862 | 2278 | `	pIdle->pStack = pStack;` |
|   1268862 | 2279 | `	pIdle->nCap = nCap;` |
|   1268862 | 2280 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   1268862 | 2281 | `	pVm->pIdleOperandStacks = pIdle;` |
|   1268862 | 2282 | `	pVm->nIdleOperandStacks++;` |
|   1090151 | 2283 | `}` |
|         - | 2284 | `/* Forward declaration */` |
|         - | 2285 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 2286 | `/*` |
|         - | 2287 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 2288 | ` * This routine gets called by the PH7 engine after` |
|         - | 2289 | ` * successful compilation of the target PHP program.` |
|         - | 2290 | ` */` |
|      3646 | 2291 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 2292 | `	ph7_vm *pVm /* Target VM */` |
|         - | 2293 | `	)` |
|         5 | 2294 | `{` |
|         - | 2295 | `	SyHashEntry *pEntry;` |
|         - | 2296 | `	sxi32 rc;` |
|      3651 | 2297 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 2298 | `		/* Initialize your VM first */` |
|       ! 0 | 2299 | `		return SXERR_CORRUPT;` |
|         - | 2300 | `	}` |
|         - | 2301 | `	/* Mark the VM ready for byte-code execution */` |
|      3651 | 2302 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 2303 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 2304 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 2305 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 2306 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      3651 | 2307 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 2308 | `	/* Emit the DONE instruction */` |
|      3651 | 2309 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      3651 | 2310 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2311 | `		return SXERR_MEM;` |
|         - | 2312 | `	}` |
|         - | 2313 | `	/* Script return value */` |
|      3651 | 2314 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 2315 | `	/* Allocate a new operand stack */` |
|      3651 | 2316 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      3651 | 2317 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 2318 | `		return SXERR_MEM;` |
|         - | 2319 | `	}` |
|         - | 2320 | `	/* Set the default VM output consumer callback and it's` |
|         - | 2321 | `	 * private data. */` |
|      3651 | 2322 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      3651 | 2323 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 2324 | `	/* Allocate the reference table */` |
|      3651 | 2325 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      3651 | 2326 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      3651 | 2327 | `	if( pVm->apRefObj == 0 ){` |
|         - | 2328 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2329 | `		return SXERR_MEM;` |
|         - | 2330 | `	}` |
|         - | 2331 | `	/* Zero the reference table */` |
|      3651 | 2332 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|         - | 2333 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      3651 | 2334 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      3651 | 2335 | `	if( rc != SXRET_OK ){` |
|         - | 2336 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2337 | `		return rc;` |
|         - | 2338 | `	}` |
|         - | 2339 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 2340 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 2341 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 2342 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 2343 | `	 * below it is compile-time/init state that survives a reset. */` |
|      3651 | 2344 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|         - | 2345 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      3651 | 2346 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      3651 | 2347 | `	if( rc != SXRET_OK ){` |
|         - | 2348 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2349 | `		return rc;` |
|         - | 2350 | `	}` |
|         - | 2351 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      3651 | 2352 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 2353 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      3651 | 2354 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 2355 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      3651 | 2356 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 2357 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      3651 | 2358 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 2359 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 2360 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      3651 | 2361 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      3651 | 2362 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 2363 | `#endif` |
|         - | 2364 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2365 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      3651 | 2366 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 2367 | `#endif` |
|         - | 2368 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 2369 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      3651 | 2370 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 2371 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      3651 | 2372 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 2373 | `	/* Initialize and install static and constants class attributes.` |
|         - | 2374 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 2375 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 2376 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 2377 | `	 * that function in sync when changing what is reserved here. */` |
|      3651 | 2378 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    500033 | 2379 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    496389 | 2380 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    496389 | 2381 | `		if( rc != SXRET_OK ){` |
|         3 | 2382 | `			return rc;` |
|         - | 2383 | `		}` |
|         5 | 2384 | `	}` |
|         - | 2385 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      3649 | 2386 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2387 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      3649 | 2388 | `	pVm->nNextObjId = 1;` |
|         - | 2389 | `	/* VM is ready for bytecode execution */` |
|      3649 | 2390 | `	return SXRET_OK;` |
|      1828 | 2391 | `}` |
|         - | 2392 | `/*` |
|         - | 2393 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 2394 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 2395 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 2396 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 2397 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 2398 | ` */` |
|         8 | 2399 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 2400 | `{` |
|         - | 2401 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|         - | 2402 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|         - | 2403 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|       328 | 2404 | `	while( pVm->pRefList ){` |
|       320 | 2405 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|       ! 0 | 2406 | `	}` |
|         8 | 2407 | `}` |
|         - | 2408 | `/*` |
|         - | 2409 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 2410 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 2411 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 2412 | ` */` |
|        56 | 2413 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 2414 | `{` |
|        56 | 2415 | `	PH7_MemObjRelease(pObj);` |
|        56 | 2416 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        56 | 2417 | `}` |
|         - | 2418 | `/*` |
|         - | 2419 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 2420 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 2421 | ` * of statics).` |
|         - | 2422 | ` */` |
|      6760 | 2423 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|       ! 0 | 2424 | `{` |
|      6760 | 2425 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 2426 | `	sxu32 k;` |
|      6788 | 2427 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        28 | 2428 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        14 | 2429 | `	}` |
|      6760 | 2430 | `}` |
|         - | 2431 | `/*` |
|         - | 2432 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 2433 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 2434 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 2435 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 2436 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 2437 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 2438 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 2439 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 2440 | ` *    has its static sentinels reset.` |
|         - | 2441 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 2442 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 2443 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 2444 | ` */` |
|         8 | 2445 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 2446 | `{` |
|         - | 2447 | `	SyHashEntry *pEntry;` |
|         8 | 2448 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      6768 | 2449 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      6760 | 2450 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      6760 | 2451 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 2452 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|         - | 2453 | `			 * release its captured-by-value environment, then free the entry,` |
|         - | 2454 | `			 * name buffer and structure. */` |
|         4 | 2455 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|         4 | 2456 | `			const char *zName = SyStringData(&pFunc->sName);` |
|         - | 2457 | `			sxu32 k;` |
|         4 | 2458 | `			VmResetFuncStatics(pFunc);` |
|         8 | 2459 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|         4 | 2460 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|         2 | 2461 | `			}` |
|         4 | 2462 | `			SySetRelease(&pFunc->aClosureEnv);` |
|         - | 2463 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 2464 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 2465 | `			if( zName ){` |
|         4 | 2466 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|         2 | 2467 | `			}` |
|         4 | 2468 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|         4 | 2469 | `			continue;` |
|         - | 2470 | `		}` |
|         - | 2471 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     13512 | 2472 | `		while( pFunc ){` |
|      6756 | 2473 | `			VmResetFuncStatics(pFunc);` |
|      6756 | 2474 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 2475 | `		}` |
|       ! 0 | 2476 | `	}` |
|         8 | 2477 | `	pVm->closure_cnt = 0;` |
|         8 | 2478 | `}` |
|         - | 2479 | `/*` |
|         - | 2480 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 2481 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 2482 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 2483 | ` * the class re-mount registers fresh ones.` |
|         - | 2484 | ` */` |
|         8 | 2485 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 2486 | `{` |
|         - | 2487 | `	SyHashEntry *pEntry;` |
|         - | 2488 | `	/* Common case: no class static typed properties — table already empty. */` |
|         8 | 2489 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|         4 | 2490 | `		return;` |
|         - | 2491 | `	}` |
|         - | 2492 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 2493 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 2494 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 2495 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 2496 | `		if( pEntry->pUserData ){` |
|         4 | 2497 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 2498 | `		}` |
|       ! 0 | 2499 | `	}` |
|         4 | 2500 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 2501 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 | 2502 | `}` |
|         - | 2503 | `/*` |
|         - | 2504 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 2505 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 2506 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 2507 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 2508 | ` * comparing equal — both used to cast to 1.` |
|         - | 2509 | ` *` |
|         - | 2510 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 2511 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 2512 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 2513 | ` * aborting a cast.` |
|         - | 2514 | ` */` |
|        24 | 2515 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         2 | 2516 | `{` |
|         - | 2517 | `	SyHashEntry *pEntry;` |
|         - | 2518 | `	phl_res_id *pRec;` |
|        26 | 2519 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 2520 | `		return 0;` |
|         - | 2521 | `	}` |
|        26 | 2522 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|        26 | 2523 | `	if( pEntry ){` |
|        20 | 2524 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 2525 | `	}` |
|         8 | 2526 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|         8 | 2527 | `	if( pRec == 0 ){` |
|       ! 0 | 2528 | `		return 0;` |
|         - | 2529 | `	}` |
|         8 | 2530 | `	pRec->pRes = pRes;` |
|         8 | 2531 | `	pRec->nId = pVm->nResourceIdNext++;` |
|         8 | 2532 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 2533 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 2534 | `		return 0;` |
|         - | 2535 | `	}` |
|         8 | 2536 | `	return pRec->nId;` |
|        14 | 2537 | `}` |
|         - | 2538 | `/*` |
|         - | 2539 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 2540 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 2541 | ` */` |
|         8 | 2542 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 2543 | `{` |
|         - | 2544 | `	SyHashEntry *pEntry;` |
|         8 | 2545 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|         8 | 2546 | `		pVm->nResourceIdNext = 1;` |
|         8 | 2547 | `		return;` |
|         - | 2548 | `	}` |
|       ! 0 | 2549 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 2550 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 2551 | `		if( pEntry->pUserData ){` |
|       ! 0 | 2552 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 2553 | `		}` |
|       ! 0 | 2554 | `	}` |
|       ! 0 | 2555 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 2556 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 2557 | `	pVm->nResourceIdNext = 1;` |
|         4 | 2558 | `}` |
|         - | 2559 | `/*` |
|         - | 2560 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 2561 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 2562 | ` *` |
|         - | 2563 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 2564 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 2565 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 2566 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 2567 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 2568 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 2569 | ` *` |
|         - | 2570 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 2571 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 2572 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 2573 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 2574 | ` * reserved during the run.` |
|         - | 2575 | ` *` |
|         - | 2576 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 2577 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 2578 | ` * global-scope destructors never fired.` |
|         - | 2579 | ` */` |
|         8 | 2580 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 2581 | `{` |
|         - | 2582 | `	sxu32 nWater,n;` |
|         8 | 2583 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 2584 | `		return SXERR_CORRUPT;` |
|         - | 2585 | `	}` |
|         8 | 2586 | `	nWater = pVm->nSuperBaseline;` |
|         - | 2587 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 2588 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|         8 | 2589 | `	pVm->pGlobal = 0;` |
|         - | 2590 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 2591 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 2592 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 2593 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 2594 | `	 * object); unref'ing here would race the teardown below. */` |
|         8 | 2595 | `	pVm->pClosureThis = 0;` |
|         8 | 2596 | `	pVm->pClosureScope = 0;` |
|         - | 2597 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 2598 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 2599 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|         - | 2600 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|         8 | 2601 | `	pVm->bInReset = 1;` |
|         - | 2602 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|         8 | 2603 | `	VmResetRefTable(&(*pVm));` |
|         - | 2604 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 2605 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 2606 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 2607 | `	 * re-run define() overwrites the value in place). */` |
|         8 | 2608 | `	VmResetFunctionState(&(*pVm));` |
|         - | 2609 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 2610 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       344 | 2611 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|       336 | 2612 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|       336 | 2613 | `		if( pObj ){` |
|       336 | 2614 | `			PH7_MemObjRelease(pObj);` |
|       168 | 2615 | `		}` |
|       168 | 2616 | `	}` |
|         - | 2617 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 2618 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|         8 | 2619 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 2620 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 2621 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|         8 | 2622 | `	VmResetResourceIds(&(*pVm));` |
|         - | 2623 | `	/* (5) Unwind any active frames back to none. */` |
|        16 | 2624 | `	while( pVm->pFrame ){` |
|         8 | 2625 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 2626 | `	}` |
|         - | 2627 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|         8 | 2628 | `	pVm->bInReset = 0;` |
|         - | 2629 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 2630 | `	 * slots (their indices no longer exist). */` |
|         8 | 2631 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|         8 | 2632 | `	SySetReset(&pVm->aFreeObj);` |
|         - | 2633 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|         8 | 2634 | `	SyHashRelease(&pVm->hSuper);` |
|         8 | 2635 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|         - | 2636 | `	/* (8) Drain remaining per-exec containers. */` |
|         8 | 2637 | `	SySetReset(&pVm->aSelf);` |
|         - | 2638 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 2639 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 2640 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|         8 | 2641 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 2642 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 2643 | `		if( pCB ){` |
|         - | 2644 | `			int iArg;` |
|       ! 0 | 2645 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 2646 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 2647 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 2648 | `			}` |
|       ! 0 | 2649 | `		}` |
|       ! 0 | 2650 | `	}` |
|         8 | 2651 | `	SySetReset(&pVm->aShutdown);` |
|         - | 2652 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 2653 | `	 * aborted program can leave entries behind). */` |
|         8 | 2654 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|         8 | 2655 | `	SySetReset(&pVm->aException);` |
|         8 | 2656 | `	SySetReset(&pVm->aFinallyAction);` |
|         8 | 2657 | `	pVm->pPendingException = 0;` |
|         8 | 2658 | `	pVm->pInflightException = 0;` |
|         8 | 2659 | `	pVm->nInflightExcBase = 0;` |
|         8 | 2660 | `	pVm->pResumeFrame = 0;` |
|         8 | 2661 | `	pVm->iResumePc = 0;` |
|         8 | 2662 | `	pVm->pResumeInstr = 0;` |
|         8 | 2663 | `	pVm->iResumeStackDepth = 0;` |
|         8 | 2664 | `	pVm->nBoundaryRc = 0;` |
|         8 | 2665 | `	pVm->pConstEvalClass = 0;` |
|         8 | 2666 | `	pVm->nConstEvalDepth = 0;` |
|         8 | 2667 | `	pVm->pConstCycleAttr = 0;` |
|         8 | 2668 | `	pVm->pConstCycleClass = 0;` |
|         8 | 2669 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 2670 | `	{` |
|         - | 2671 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 2672 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|         8 | 2673 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|         8 | 2674 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 2675 | `		sxu32 iRmw;` |
|         8 | 2676 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 2677 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 2678 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 2679 | `		}` |
|         8 | 2680 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 2681 | `	}` |
|         8 | 2682 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2683 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2684 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2685 | `	}` |
|         8 | 2686 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|         8 | 2687 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2688 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2689 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2690 | `	}` |
|         8 | 2691 | `	pVm->pHookSetAttr = 0;` |
|         8 | 2692 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|         8 | 2693 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2694 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2695 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2696 | `	}` |
|         8 | 2697 | `	pVm->pMagicCallClass = 0;` |
|         8 | 2698 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         8 | 2699 | `	pVm->nExceptDepth = 0;` |
|         - | 2700 | `	/* spl_autoload_register() callbacks are per request */` |
|         8 | 2701 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 2702 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 2703 | `		if( pCB ){` |
|       ! 0 | 2704 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 2705 | `		}` |
|       ! 0 | 2706 | `	}` |
|         8 | 2707 | `	SySetReset(&pVm->aAutoload);` |
|         - | 2708 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 2709 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|         8 | 2710 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 2711 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 2712 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 2713 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 2714 | `	}` |
|         - | 2715 | `	/* Output buffers */` |
|         8 | 2716 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 2717 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 2718 | `		if( pOb ){` |
|       ! 0 | 2719 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 2720 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 2721 | `		}` |
|       ! 0 | 2722 | `	}` |
|         8 | 2723 | `	SySetReset(&pVm->aOB);` |
|         8 | 2724 | `	pVm->nObDepth = 0;` |
|         - | 2725 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 2726 | `	{` |
|         8 | 2727 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|         8 | 2728 | `		if( rc == SXRET_OK ){` |
|         8 | 2729 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         4 | 2730 | `		}` |
|         8 | 2731 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2732 | `			return rc;` |
|         - | 2733 | `		}` |
|         - | 2734 | `	}` |
|         - | 2735 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 2736 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 2737 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 2738 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 2739 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 2740 | `	{` |
|         - | 2741 | `		SyHashEntry *pEntry;` |
|         8 | 2742 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      1100 | 2743 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      1092 | 2744 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 2745 | `			ph7_class_attr *pAttr;` |
|         - | 2746 | `			SyHashEntry *pAttrEntry;` |
|      1092 | 2747 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|      5050 | 2748 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      3412 | 2749 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      3412 | 2750 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        60 | 2751 | `					pAttr->nIdx = SXU32_HIGH;` |
|        60 | 2752 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|        30 | 2753 | `				}` |
|       ! 0 | 2754 | `			}` |
|         - | 2755 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 2756 | `			 * slots too so VM reuse re-evaluates them. */` |
|      1092 | 2757 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|      2372 | 2758 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|      1280 | 2759 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      1280 | 2760 | `				pAttr->nIdx = SXU32_HIGH;` |
|      1280 | 2761 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 2762 | `			}` |
|       ! 0 | 2763 | `		}` |
|         8 | 2764 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      1100 | 2765 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      1092 | 2766 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      1092 | 2767 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2768 | `				return rc;` |
|         - | 2769 | `			}` |
|       ! 0 | 2770 | `		}` |
|         - | 2771 | `	}` |
|         - | 2772 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|         8 | 2773 | `	SyBlobReset(&pVm->sConsumer);` |
|         8 | 2774 | `	pVm->nOutputLen = 0;` |
|         8 | 2775 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|         8 | 2776 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|         8 | 2777 | `	pVm->iResponseStatus = 200;` |
|         8 | 2778 | `	pVm->bHeadersSent = 0;` |
|         8 | 2779 | `	pVm->bHttpContext = 0;` |
|         8 | 2780 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|         8 | 2781 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|         8 | 2782 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|         8 | 2783 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|         8 | 2784 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         8 | 2785 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2786 | `#ifdef PH7_ENABLE_PCRE` |
|         8 | 2787 | `	pVm->iPcreLastError = 0;` |
|         - | 2788 | `#endif` |
|         - | 2789 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2790 | `	/* Drop the libxml error queue and the previous request's documents */` |
|         8 | 2791 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 2792 | `#endif` |
|         8 | 2793 | `	pVm->iCmpCallbackExc = 0;` |
|         8 | 2794 | `	pVm->bHaltRequested = 0;` |
|         8 | 2795 | `	pVm->iExitStatus = 0;` |
|         8 | 2796 | `	pVm->nSpreadCallBase = 0;` |
|         8 | 2797 | `	VmSpreadCaptureReset(pVm);` |
|         8 | 2798 | `	pVm->nRecursionDepth = 0;` |
|         8 | 2799 | `	pVm->pActiveCtx = 0;` |
|         8 | 2800 | `	pVm->pCoalesceObj = 0;` |
|         8 | 2801 | `	pVm->bCoalesceArmed = 0;` |
|         8 | 2802 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 2803 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|         8 | 2804 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2805 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 2806 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|         8 | 2807 | `	pVm->nNextObjId = 1;` |
|         - | 2808 | `	/* Set the ready flag */` |
|         8 | 2809 | `	pVm->nMagic = PH7_VM_RUN;` |
|         8 | 2810 | `	return SXRET_OK;` |
|         4 | 2811 | `}` |
|         - | 2812 | `/*` |
|         - | 2813 | ` * Release a Virtual Machine.` |
|         - | 2814 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 2815 | ` */` |
|      3644 | 2816 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 2817 | `{` |
|         - | 2818 | `	/* Set the stale magic number */` |
|      3649 | 2819 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 2820 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2821 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 2822 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      3649 | 2823 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 2824 | `#endif` |
|         - | 2825 | `	/* Release the private memory subsystem */` |
|      3649 | 2826 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      3649 | 2827 | `	return SXRET_OK;` |
|         5 | 2828 | `}` |
|         - | 2829 | `/*` |
|         - | 2830 | ` * Initialize a foreign function call context.` |
|         - | 2831 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 2832 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 2833 | ` * functions.` |
|         - | 2834 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 2835 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 2836 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 2837 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 2838 | ` */` |
|   3937083 | 2839 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 2840 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 2841 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 2842 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 2843 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 2844 | `	sxi32 iFlags          /* Control flags */` |
|         - | 2845 | `	)` |
|         5 | 2846 | `{` |
|   3937088 | 2847 | `	pOut->pFunc = pFunc;` |
|   3937088 | 2848 | `	pOut->pVm   = pVm;` |
|   3937088 | 2849 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   3937088 | 2850 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 2851 | `	/* Assume a null return value */` |
|   3937088 | 2852 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   3937088 | 2853 | `	pOut->pRet = pRet;` |
|   3937088 | 2854 | `	pOut->iFlags = iFlags;` |
|   3937088 | 2855 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   3937088 | 2856 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|   3937088 | 2857 | `	return SXRET_OK;` |
|         5 | 2858 | `}` |
|         - | 2859 | `/*` |
|         - | 2860 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 2861 | ` * left behind.` |
|         - | 2862 | ` */` |
|   3937083 | 2863 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 2864 | `{` |
|         - | 2865 | `	sxu32 n;` |
|   3937088 | 2866 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     12589 | 2867 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|     72228 | 2868 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     59644 | 2869 | `			if( apObj[n] == 0 ){` |
|         - | 2870 | `				/* Already released */` |
|       856 | 2871 | `				continue;` |
|         - | 2872 | `			}` |
|     58793 | 2873 | `			PH7_MemObjRelease(apObj[n]);` |
|     58793 | 2874 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     29399 | 2875 | `		}` |
|     12589 | 2876 | `		SySetRelease(&pCtx->sVar);` |
|      6292 | 2877 | `	}` |
|   3937088 | 2878 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 2879 | `		ph7_aux_data *aAux;` |
|         - | 2880 | `		void *pChunk;` |
|         - | 2881 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 2882 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 2883 | `		 */` |
|       137 | 2884 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       413 | 2885 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       281 | 2886 | `			pChunk = aAux[n].pAuxData;` |
|         - | 2887 | `			/* Release the chunk */` |
|       281 | 2888 | `			if( pChunk ){` |
|       281 | 2889 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       138 | 2890 | `			}` |
|       143 | 2891 | `		}` |
|       137 | 2892 | `		SySetRelease(&pCtx->sChunk);` |
|        66 | 2893 | `	}` |
|   3937088 | 2894 | `}` |
|         - | 2895 | `/*` |
|         - | 2896 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 2897 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 2898 | ` */` |
|       851 | 2899 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 2900 | `	ph7_context *pCtx, /* Call context */` |
|         - | 2901 | `	ph7_value *pValue  /* Release this value */` |
|         - | 2902 | `	)` |
|         5 | 2903 | `{` |
|       856 | 2904 | `	if( pValue == 0 ){` |
|         - | 2905 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 2906 | `		return;` |
|         - | 2907 | `	}` |
|       856 | 2908 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|       856 | 2909 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 2910 | `		sxu32 n;` |
|      2114 | 2911 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      2114 | 2912 | `			if( apObj[n] == pValue ){` |
|       856 | 2913 | `				PH7_MemObjRelease(pValue);` |
|       856 | 2914 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 2915 | `				/* Mark as released */` |
|       856 | 2916 | `				apObj[n] = 0;` |
|       856 | 2917 | `				break;` |
|         - | 2918 | `			}` |
|       633 | 2919 | `		}` |
|       425 | 2920 | `	}` |
|       430 | 2921 | `}` |
|         - | 2922 | `/*` |
|         - | 2923 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 2924 | ` */` |
|  33127960 | 2925 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 2926 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 2927 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 2928 | `	)` |
|         5 | 2929 | `{` |
|  33127965 | 2930 | `	ph7_value *pTos = *ppTos;` |
|  67074376 | 2931 | `	while( nPop > 0 ){` |
|  33946416 | 2932 | `		PH7_MemObjRelease(pTos);` |
|  33946416 | 2933 | `		pTos--;` |
|  33946416 | 2934 | `		nPop--;` |
|         5 | 2935 | `	}` |
|         - | 2936 | `	/* Top of the stack */` |
|  33127965 | 2937 | `	*ppTos = pTos;` |
|  33127965 | 2938 | `}` |
|         - | 2939 | `/*` |
|         - | 2940 | ` * Reserve a memory object.` |
|         - | 2941 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2942 | ` */` |
|  23073069 | 2943 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 2944 | `{` |
|  23073074 | 2945 | `	ph7_value *pObj = 0;` |
|         - | 2946 | `	VmSlot *pSlot;` |
|         - | 2947 | `	sxu32 nIdx;` |
|         - | 2948 | `	/* Check for a free slot */` |
|  23073074 | 2949 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  23073074 | 2950 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  23073074 | 2951 | `	if( pSlot ){` |
|  20223160 | 2952 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  20223160 | 2953 | `		nIdx = pSlot->nIdx;` |
|  10112552 | 2954 | `	}` |
|  23073074 | 2955 | `	if( pObj == 0 ){` |
|         - | 2956 | `		/* Reserve a new memory object */` |
|   2849919 | 2957 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   2849919 | 2958 | `		if( pObj == 0 ){` |
|       ! 0 | 2959 | `			return 0;` |
|         - | 2960 | `		}` |
|   1424957 | 2961 | `	}` |
|         - | 2962 | `	/* Set a null default value */` |
|  23073074 | 2963 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  23073074 | 2964 | `	pObj->nIdx = nIdx;` |
|  23073074 | 2965 | `	return pObj;` |
|  11537514 | 2966 | `}` |
|         - | 2967 | `/*` |
|         - | 2968 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 2969 | ` */` |
|     49600 | 2970 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 2971 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2972 | `	const char *zKey,  /* Entry key */` |
|         - | 2973 | `	sxu32 nByte,       /* Key length */` |
|         - | 2974 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 2975 | `	)` |
|         5 | 2976 | `{` |
|         - | 2977 | `	ph7_value sKey;` |
|         - | 2978 | `	sxi32 rc;` |
|     49605 | 2979 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     49605 | 2980 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 2981 | `	/* Perform the insertion */` |
|     49605 | 2982 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     49605 | 2983 | `	PH7_MemObjRelease(&sKey);` |
|     49605 | 2984 | `	return rc;` |
|         5 | 2985 | `}` |
|         - | 2986 | `/*` |
|         - | 2987 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 2988 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 2989 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 2990 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 2991 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 2992 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 2993 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 2994 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 2995 | ` *     superglobal in place.` |
|         - | 2996 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 2997 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING` |
|         - | 2998 | ` *     name is rejected with the engine's usual "already exists" diagnostic` |
|         - | 2999 | ` *     (the same limitation OP_STORE_REF has for plain variables).` |
|         - | 3000 | ` */` |
|       162 | 3001 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         2 | 3002 | `{` |
|       164 | 3003 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 3004 | `	SyHashEntry *pEntry;` |
|         - | 3005 | `	ph7_value *pObj;` |
|         - | 3006 | `	char *zDup;` |
|         - | 3007 | `	sxu32 nIdx;` |
|         - | 3008 | `	sxi32 rc;` |
|         - | 3009 | `	/* Walk down to the global frame */` |
|       168 | 3010 | `	while( pFrame->pParent ){` |
|         5 | 3011 | `		pFrame = pFrame->pParent;` |
|         1 | 3012 | `	}` |
|         - | 3013 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       164 | 3014 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|       164 | 3015 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 3016 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 3017 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 3018 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 3019 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 3020 | `		 * of $GLOBALS itself). */` |
|         5 | 3021 | `		pEntry = 0;` |
|         2 | 3022 | `	}` |
|       164 | 3023 | `	if( pEntry == 0 ){` |
|       164 | 3024 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|        81 | 3025 | `	}` |
|       164 | 3026 | `	if( pEntry ){` |
|         3 | 3027 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 3028 | `			SyString sName;` |
|       ! 0 | 3029 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|       ! 0 | 3030 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|       ! 0 | 3031 | `			return SXRET_OK;` |
|         - | 3032 | `		}` |
|         3 | 3033 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 3034 | `		if( pObj == 0 ){` |
|       ! 0 | 3035 | `			return SXERR_NOTFOUND;` |
|         - | 3036 | `		}` |
|         3 | 3037 | `		if( pValue ){` |
|         3 | 3038 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 3039 | `		}else{` |
|       ! 0 | 3040 | `			PH7_MemObjToNull(pObj);` |
|         - | 3041 | `		}` |
|         3 | 3042 | `		return SXRET_OK;` |
|         - | 3043 | `	}` |
|       162 | 3044 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 3045 | `		/* Reserve a fresh slot for the new global */` |
|       160 | 3046 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       160 | 3047 | `		if( pObj == 0 ){` |
|       ! 0 | 3048 | `			return SXERR_MEM;` |
|         - | 3049 | `		}` |
|       160 | 3050 | `		nIdx = pObj->nIdx;` |
|        81 | 3051 | `	}else{` |
|         - | 3052 | `		/* Reference assignment: bind the name to the existing slot */` |
|         3 | 3053 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|         3 | 3054 | `		if( pObj == 0 ){` |
|       ! 0 | 3055 | `			return SXERR_NOTFOUND;` |
|         - | 3056 | `		}` |
|         3 | 3057 | `		nIdx = nRefIdx;` |
|         - | 3058 | `	}` |
|       162 | 3059 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       162 | 3060 | `	if( zDup == 0 ){` |
|       ! 0 | 3061 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3062 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 3063 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|         - | 3064 | `			VmSlot sFree;` |
|       ! 0 | 3065 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3066 | `			sFree.pUserData = 0;` |
|       ! 0 | 3067 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3068 | `		}` |
|       ! 0 | 3069 | `		return SXERR_MEM;` |
|         - | 3070 | `	}` |
|       162 | 3071 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       162 | 3072 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3073 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3074 | `			VmSlot sFree;` |
|       ! 0 | 3075 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3076 | `			sFree.pUserData = 0;` |
|       ! 0 | 3077 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3078 | `		}` |
|       ! 0 | 3079 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 3080 | `		return rc;` |
|         - | 3081 | `	}` |
|         - | 3082 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       162 | 3083 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       162 | 3084 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       162 | 3085 | `	if( nRefIdx == SXU32_HIGH ){` |
|       160 | 3086 | `		pObj->nIdx = nIdx;` |
|       160 | 3087 | `		if( pValue ){` |
|       160 | 3088 | `			PH7_MemObjStore(pValue,pObj);` |
|        79 | 3089 | `		}` |
|        79 | 3090 | `	}` |
|       162 | 3091 | `	return SXRET_OK;` |
|        83 | 3092 | `}` |
|         - | 3093 | `/*` |
|         - | 3094 | ` * Extract a variable value from the top active VM frame.` |
|         - | 3095 | ` * Return a pointer to the variable value on success.` |
|         - | 3096 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 3097 | ` */` |
|  23746321 | 3098 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 3099 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 3100 | `	const SyString *pName, /* Variable name */` |
|         - | 3101 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 3102 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 3103 | `	)` |
|         5 | 3104 | `{` |
|  23746326 | 3105 | `	int bNullify = FALSE;` |
|         - | 3106 | `	SyHashEntry *pEntry;` |
|         - | 3107 | `	VmFrame *pFrame;` |
|         - | 3108 | `	ph7_value *pObj;` |
|         - | 3109 | `	sxu32 nIdx;` |
|         - | 3110 | `	sxi32 rc;` |
|         - | 3111 | `	/* Point to the top active frame */` |
|  23746326 | 3112 | `	pFrame = pVm->pFrame;` |
|  23746326 | 3113 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 3114 | `	/* Perform the lookup */` |
|  23746326 | 3115 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 3116 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        16 | 3117 | `		pName = &sAnnon;` |
|         - | 3118 | `		/* Always nullify the object */` |
|        16 | 3119 | `		bNullify = TRUE;` |
|        16 | 3120 | `		bDup = FALSE;` |
|         7 | 3121 | `	}` |
|         - | 3122 | `	/* Check the superglobals table first */` |
|  23746326 | 3123 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  23746326 | 3124 | `	if( pEntry == 0 ){` |
|         - | 3125 | `		/* Query the top active frame */` |
|  23745866 | 3126 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  23745866 | 3127 | `		if( pEntry == 0 ){` |
|   6759921 | 3128 | `			char *zName = (char *)pName->zString;` |
|         - | 3129 | `			VmSlot sLocal;` |
|   6759921 | 3130 | `			if( !bCreate ){` |
|         - | 3131 | `				/* Do not create the variable,return NULL instead */` |
|      1607 | 3132 | `				return 0;` |
|         - | 3133 | `			}` |
|         - | 3134 | `			/* No such variable,automatically create a new one and install` |
|         - | 3135 | `			 * it in the current frame.` |
|         - | 3136 | `			 */` |
|   6758319 | 3137 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   6758319 | 3138 | `			if( pObj == 0 ){` |
|       ! 0 | 3139 | `				return 0;` |
|         - | 3140 | `			}` |
|   6758319 | 3141 | `			nIdx = pObj->nIdx;` |
|   6758319 | 3142 | `			if( bDup ){` |
|         - | 3143 | `				/* Duplicate name */` |
|       963 | 3144 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|       963 | 3145 | `				if( zName == 0 ){` |
|       ! 0 | 3146 | `					return 0;` |
|         - | 3147 | `				}` |
|       479 | 3148 | `			}` |
|         - | 3149 | `			/* Link to the top active VM frame */` |
|   6758319 | 3150 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   6758319 | 3151 | `			if( rc != SXRET_OK ){` |
|         - | 3152 | `				/* Return the slot to the free pool */` |
|       ! 0 | 3153 | `				sLocal.nIdx = nIdx;` |
|       ! 0 | 3154 | `				sLocal.pUserData = 0;` |
|       ! 0 | 3155 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|       ! 0 | 3156 | `				return 0;` |
|         - | 3157 | `			}` |
|   6758319 | 3158 | `			if( pFrame->pParent != 0 ){` |
|         - | 3159 | `				/* Local variable */` |
|   6749073 | 3160 | `				sLocal.nIdx = nIdx;` |
|   6749073 | 3161 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   3375514 | 3162 | `			}else{` |
|         - | 3163 | `				/* Register in the $GLOBALS array */` |
|      9251 | 3164 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|         - | 3165 | `			}` |
|         - | 3166 | `			/* Install in the reference table */` |
|   6758319 | 3167 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 3168 | `			/* Save object index */` |
|   6758319 | 3169 | `			pObj->nIdx = nIdx;` |
|   3380137 | 3170 | `		}else{` |
|         - | 3171 | `			/* Extract variable contents */` |
|  16985950 | 3172 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  16985950 | 3173 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  16985950 | 3174 | `			if( bNullify && pObj ){` |
|         3 | 3175 | `				PH7_MemObjRelease(pObj);` |
|         1 | 3176 | `			}` |
|         - | 3177 | `		}` |
|  11876483 | 3178 | `	}else{` |
|         - | 3179 | `		/* Superglobal */` |
|       465 | 3180 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       465 | 3181 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 3182 | `	}` |
|  23744724 | 3183 | `	return pObj;` |
|  11877514 | 3184 | `}` |
|         - | 3185 | `/*` |
|         - | 3186 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 3187 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 3188 | ` */` |
|     33260 | 3189 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 3190 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 3191 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 3192 | `	sxu32 nByte        /* zName length */` |
|         - | 3193 | `	)` |
|         5 | 3194 | `{` |
|         - | 3195 | `	SyHashEntry *pEntry;` |
|         - | 3196 | `	ph7_value *pValue;` |
|         - | 3197 | `	sxu32 nIdx;` |
|         - | 3198 | `	/* Query the superglobal table */` |
|     33265 | 3199 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     33265 | 3200 | `	if( pEntry == 0 ){` |
|         - | 3201 | `		/* No such entry */` |
|       ! 0 | 3202 | `		return 0;` |
|         - | 3203 | `	}` |
|         - | 3204 | `	/* Extract the superglobal index in the global object pool */` |
|     33265 | 3205 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3206 | `	/* Extract the variable value  */` |
|     33265 | 3207 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     33265 | 3208 | `	return pValue;` |
|     16635 | 3209 | `}` |
|         - | 3210 | `/*` |
|         - | 3211 | ` * Perform a raw hashmap insertion.` |
|         - | 3212 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 3213 | ` */` |
|     26046 | 3214 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 3215 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 3216 | `	const char *zKey,   /* Entry key */` |
|         - | 3217 | `	int nKeylen,        /* zKey length*/` |
|         - | 3218 | `	const char *zData,  /* Entry data */` |
|         - | 3219 | `	int nLen            /* zData length */` |
|         - | 3220 | `	)` |
|         5 | 3221 | `{` |
|         - | 3222 | `	ph7_value sKey,sValue;` |
|         - | 3223 | `	sxi32 rc;` |
|     26051 | 3224 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     26051 | 3225 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     26051 | 3226 | `	if( zKey ){` |
|     22357 | 3227 | `		if( nKeylen < 0 ){` |
|     22253 | 3228 | `			nKeylen = (int)SyStrlen(zKey);` |
|     11124 | 3229 | `		}` |
|     22357 | 3230 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     11176 | 3231 | `	}` |
|     26051 | 3232 | `	if( zData ){` |
|     26051 | 3233 | `		if( nLen < 0 ){` |
|         - | 3234 | `			/* Compute length automatically */` |
|     14763 | 3235 | `			nLen = (int)SyStrlen(zData);` |
|      7379 | 3236 | `		}` |
|     26051 | 3237 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     13023 | 3238 | `	}` |
|         - | 3239 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 3240 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 3241 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 3242 | `	 * every argument under "". */` |
|     26051 | 3243 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     26051 | 3244 | `	PH7_MemObjRelease(&sKey);` |
|     26051 | 3245 | `	PH7_MemObjRelease(&sValue);` |
|     26051 | 3246 | `	return rc;` |
|         5 | 3247 | `}` |
|         - | 3248 | `/*` |
|         - | 3249 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 3250 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 3251 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 3252 | ` */` |
|        34 | 3253 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 3254 | `{` |
|        38 | 3255 | `	sxi64 iVal = 0;` |
|        38 | 3256 | `	if( nValue == 0 ){` |
|       ! 0 | 3257 | `		return 0;` |
|         - | 3258 | `	}` |
|        34 | 3259 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 3260 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 3261 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 3262 | `		return 1;` |
|         - | 3263 | `	}` |
|        38 | 3264 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 3265 | `	return iVal != 0;` |
|        21 | 3266 | `}` |
|         - | 3267 | `/*` |
|         - | 3268 | ` * Configure a working virtual machine instance.` |
|         - | 3269 | ` *` |
|         - | 3270 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 3271 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 3272 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 3273 | ` * The second argument to this function is an integer configuration option` |
|         - | 3274 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 3275 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 3276 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 3277 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 3278 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 3279 | ` */` |
|     99066 | 3280 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 3281 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3282 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 3283 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 3284 | `	)` |
|         5 | 3285 | `{` |
|     99071 | 3286 | `	sxi32 rc = SXRET_OK;` |
|     99071 | 3287 | `	switch(nOp){` |
|      1809 | 3288 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      3623 | 3289 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      3623 | 3290 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3291 | `		/* VM output consumer callback */` |
|         - | 3292 | `#ifdef UNTRUST` |
|         - | 3293 | `		if( xConsumer == 0 ){` |
|         - | 3294 | `			rc = SXERR_CORRUPT;` |
|         - | 3295 | `			break;` |
|         - | 3296 | `		}` |
|         - | 3297 | `#endif` |
|         - | 3298 | `		/* Install the output consumer */` |
|      3623 | 3299 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      3623 | 3300 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      3623 | 3301 | `		break;` |
|         - | 3302 | `							   }` |
|      1809 | 3303 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      3623 | 3304 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      3623 | 3305 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3306 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 3307 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 3308 | `		 * instead of the program-output stream. */` |
|         - | 3309 | `#ifdef UNTRUST` |
|         - | 3310 | `		if( xConsumer == 0 ){` |
|         - | 3311 | `			rc = SXERR_CORRUPT;` |
|         - | 3312 | `			break;` |
|         - | 3313 | `		}` |
|         - | 3314 | `#endif` |
|      3623 | 3315 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      3623 | 3316 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      3623 | 3317 | `		break;` |
|         - | 3318 | `								   }` |
|      1822 | 3319 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 3320 | `		/* Import path */` |
|         - | 3321 | `		  const char *zPath;` |
|         - | 3322 | `		  SyString sPath;` |
|      3649 | 3323 | `		  zPath = va_arg(ap,const char *);` |
|         - | 3324 | `#if defined(UNTRUST)` |
|         - | 3325 | `		  if( zPath == 0 ){` |
|         - | 3326 | `			  rc = SXERR_EMPTY;` |
|         - | 3327 | `			  break;` |
|         - | 3328 | `		  }` |
|         - | 3329 | `#endif` |
|      3649 | 3330 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 3331 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 3332 | `#ifdef __WINNT__` |
|         5 | 3333 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 3334 | `#endif` |
|      7293 | 3335 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 3336 | `		  /* Remove leading and trailing white spaces */` |
|      3649 | 3337 | `		  SyStringFullTrim(&sPath);` |
|      3649 | 3338 | `		  if( sPath.nByte > 0 ){` |
|         - | 3339 | `			  /* Store the path in the corresponding conatiner */` |
|      3649 | 3340 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      1822 | 3341 | `		  }` |
|      3649 | 3342 | `		  break;` |
|         - | 3343 | `									 }` |
|      1825 | 3344 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 3345 | `		/* Run-Time Error report */` |
|      3655 | 3346 | `		pVm->bErrReport = 1;` |
|      3655 | 3347 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      3655 | 3348 | `		break;` |
|         2 | 3349 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 3350 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 3351 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 3352 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 3353 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 3354 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 3355 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 3356 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 3357 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 3358 | `		int nDepth = va_arg(ap,int);` |
|         5 | 3359 | `		if( nDepth >= 0 ){` |
|         5 | 3360 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 3361 | `		}` |
|         5 | 3362 | `		break;` |
|         - | 3363 | `									   }` |
|         5 | 3364 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 3365 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 3366 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 3367 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 3368 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 3369 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 3370 | `		 * so it is rejected as a footgun). */` |
|        11 | 3371 | `		int nDepth = va_arg(ap,int);` |
|        11 | 3372 | `		if( nDepth > 1 ){` |
|        11 | 3373 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 3374 | `		}` |
|        11 | 3375 | `		break;` |
|         - | 3376 | `									   }` |
|       ! 0 | 3377 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 3378 | `		/* VM output length in bytes */` |
|       ! 0 | 3379 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 3380 | `#ifdef UNTRUST` |
|         - | 3381 | `		if( pOut == 0 ){` |
|         - | 3382 | `			rc = SXERR_CORRUPT;` |
|         - | 3383 | `			break;` |
|         - | 3384 | `		}` |
|         - | 3385 | `#endif` |
|       ! 0 | 3386 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 3387 | `		break;` |
|         - | 3388 | `							   }` |
|         - | 3389 |  |
|     20079 | 3390 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 3391 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 3392 | `		/* Create a new superglobal/global variable */` |
|     40163 | 3393 | `		const char *zName = va_arg(ap,const char *);` |
|     40163 | 3394 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 3395 | `		SyHashEntry *pEntry;` |
|         - | 3396 | `		ph7_value *pObj;` |
|         - | 3397 | `		sxu32 nByte;` |
|         - | 3398 | `		sxu32 nIdx;` |
|         - | 3399 | `#ifdef UNTRUST` |
|         - | 3400 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 3401 | `			rc = SXERR_CORRUPT;` |
|         - | 3402 | `			break;` |
|         - | 3403 | `		}` |
|         - | 3404 | `#endif` |
|     40163 | 3405 | `		nByte = SyStrlen(zName);` |
|     40163 | 3406 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3407 | `			/* Check if the superglobal is already installed */` |
|     36545 | 3408 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     18275 | 3409 | `		}else{` |
|         - | 3410 | `			/* Query the top active VM frame */` |
|      3623 | 3411 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 3412 | `		}` |
|     40163 | 3413 | `		if( pEntry ){` |
|         - | 3414 | `			/* Variable already installed */` |
|       ! 0 | 3415 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3416 | `			/* Extract contents */` |
|       ! 0 | 3417 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 3418 | `			if( pObj ){` |
|         - | 3419 | `				/* Overwrite old contents */` |
|       ! 0 | 3420 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 3421 | `			}` |
|       ! 0 | 3422 | `		}else{` |
|         - | 3423 | `			/* Install a new variable */` |
|     40163 | 3424 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     40163 | 3425 | `			if( pObj == 0 ){` |
|       ! 0 | 3426 | `				rc = SXERR_MEM;` |
|       ! 0 | 3427 | `				break;` |
|         - | 3428 | `			}` |
|     40163 | 3429 | `			nIdx = pObj->nIdx;` |
|         - | 3430 | `			/* Copy value */` |
|     40163 | 3431 | `			PH7_MemObjStore(pValue,pObj);` |
|     40163 | 3432 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3433 | `				/* Install the superglobal */` |
|     36545 | 3434 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     18275 | 3435 | `			}else{` |
|         - | 3436 | `				/* Install in the current frame */` |
|      3623 | 3437 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 3438 | `			}` |
|     40163 | 3439 | `			if( rc == SXRET_OK ){` |
|         - | 3440 | `				SyHashEntry *pRef;` |
|     40163 | 3441 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     36545 | 3442 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     18275 | 3443 | `				}else{` |
|      3623 | 3444 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 3445 | `				}` |
|         - | 3446 | `				/* Install in the reference table */` |
|     40163 | 3447 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     40163 | 3448 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 3449 | `					/* Register in the $GLOBALS array */` |
|     40163 | 3450 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     20079 | 3451 | `				}` |
|     20079 | 3452 | `			}` |
|         - | 3453 | `		}` |
|     40163 | 3454 | `		break;` |
|         - | 3455 | `									}` |
|     11124 | 3456 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 3457 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 3458 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 3459 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 3460 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 3461 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 3462 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     22253 | 3463 | `		const char *zKey   = va_arg(ap,const char *);` |
|     22253 | 3464 | `		const char *zValue = va_arg(ap,const char *);` |
|     22253 | 3465 | `		int nLen = va_arg(ap,int);` |
|         - | 3466 | `		ph7_hashmap *pMap;` |
|         - | 3467 | `		ph7_value *pValue;` |
|     22253 | 3468 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 3469 | `			/* Extract the $_ENV superglobal */` |
|         3 | 3470 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     22252 | 3471 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 3472 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 3473 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     22251 | 3474 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 3475 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 3476 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     22251 | 3477 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 3478 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 3479 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     22251 | 3480 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 3481 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 3482 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     22251 | 3483 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 3484 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 3485 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 3486 | `		}else{` |
|         - | 3487 | `			/* Extract the $_SERVER superglobal */` |
|     22251 | 3488 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 3489 | `		}` |
|     22253 | 3490 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3491 | `			/* No such entry */` |
|       ! 0 | 3492 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3493 | `			break;` |
|         - | 3494 | `		}` |
|         - | 3495 | `		/* Point to the hashmap */` |
|     22253 | 3496 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3497 | `		/* Perform the insertion */` |
|     22253 | 3498 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     22253 | 3499 | `		break;` |
|         - | 3500 | `								   }` |
|      1847 | 3501 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 3502 | `		/* Script arguments */` |
|      3699 | 3503 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 3504 | `		ph7_hashmap *pMap;` |
|         - | 3505 | `		ph7_value *pValue;` |
|         - | 3506 | `		sxu32 n;` |
|         - | 3507 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 3508 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 3509 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 3510 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      3699 | 3511 | `		if( zValue == 0 ){` |
|       ! 0 | 3512 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3513 | `			break;` |
|         - | 3514 | `		}` |
|         - | 3515 | `		/* Extract the $argv array */` |
|      3699 | 3516 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      3699 | 3517 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3518 | `			/* No such entry */` |
|       ! 0 | 3519 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3520 | `			break;` |
|         - | 3521 | `		}` |
|         - | 3522 | `		/* Point to the hashmap */` |
|      3699 | 3523 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3524 | `		/* Perform the insertion */` |
|      3699 | 3525 | `		n = (sxu32)SyStrlen(zValue);` |
|      3699 | 3526 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      3699 | 3527 | `		break;` |
|         - | 3528 | `								  }` |
|      1809 | 3529 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 3530 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 3531 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 3532 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 3533 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 3534 | `		ph7_value *pArgv,*pServer;` |
|         - | 3535 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 3536 | `		ph7_value sArgvVal,sKey,sCount;` |
|      3623 | 3537 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      3623 | 3538 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      3618 | 3539 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      3623 | 3540 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 3541 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3542 | `			break;` |
|         - | 3543 | `		}` |
|      3623 | 3544 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      3623 | 3545 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 3546 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      3623 | 3547 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      3623 | 3548 | `		if( pDup == 0 ){` |
|       ! 0 | 3549 | `			rc = SXERR_MEM;` |
|       ! 0 | 3550 | `			break;` |
|         - | 3551 | `		}` |
|      3623 | 3552 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      3623 | 3553 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      3623 | 3554 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      3623 | 3555 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      3623 | 3556 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      3623 | 3557 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      3623 | 3558 | `		PH7_MemObjRelease(&sKey);` |
|         - | 3559 | `		/* $_SERVER['argc'] = count($argv). */` |
|      3623 | 3560 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      3623 | 3561 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      3623 | 3562 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      3623 | 3563 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      3623 | 3564 | `		PH7_MemObjRelease(&sCount);` |
|      3623 | 3565 | `		PH7_MemObjRelease(&sKey);` |
|      3623 | 3566 | `		rc = SXRET_OK;` |
|      3623 | 3567 | `		break;` |
|         - | 3568 | `								  }` |
|        45 | 3569 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 3570 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 3571 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 3572 | `		 * apply immediately so they take effect even if the script never` |
|         - | 3573 | `		 * touches the INI API. */` |
|        94 | 3574 | `		const char *zName = va_arg(ap,const char *);` |
|        94 | 3575 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 3576 | `		VmIniEntry sEntry;` |
|         - | 3577 | `		char *zDupN,*zDupV;` |
|         - | 3578 | `		sxu32 nName,nValue;` |
|        94 | 3579 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 3580 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3581 | `			break;` |
|         - | 3582 | `		}` |
|        94 | 3583 | `		if( zValue == 0 ){` |
|       ! 0 | 3584 | `			zValue = "";` |
|       ! 0 | 3585 | `		}` |
|        94 | 3586 | `		nName = (sxu32)SyStrlen(zName);` |
|        94 | 3587 | `		nValue = (sxu32)SyStrlen(zValue);` |
|        94 | 3588 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|        94 | 3589 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|        94 | 3590 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 3591 | `			rc = SXERR_MEM;` |
|       ! 0 | 3592 | `			break;` |
|         - | 3593 | `		}` |
|        94 | 3594 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|        94 | 3595 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|        94 | 3596 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|        94 | 3597 | `		if( rc == SXRET_OK ){` |
|        90 | 3598 | `			if( nName == sizeof("error_reporting")-1` |
|        68 | 3599 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         2 | 3600 | `				sxi64 iLevel = 0;` |
|         2 | 3601 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         2 | 3602 | `				pVm->bErrReport = iLevel != 0;` |
|        89 | 3603 | `			}else if( nName == sizeof("date.timezone")-1` |
|        44 | 3604 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|       ! 0 | 3605 | `			 && nValue == 3` |
|         4 | 3606 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 3607 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 3608 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 3609 | `				pVm->nDefTz = 3;` |
|        88 | 3610 | `			}else if( nName == sizeof("zend.assertions")-1` |
|        84 | 3611 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 3612 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 3613 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 3614 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 3615 | `				sxi64 iZend = 0;` |
|        40 | 3616 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 3617 | `				if( iZend >= 1 ){` |
|        40 | 3618 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 3619 | `				}else{` |
|       ! 0 | 3620 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 3621 | `				}` |
|        74 | 3622 | `			}else if( nName == sizeof("display_errors")-1` |
|        39 | 3623 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 3624 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 3625 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 3626 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 3627 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        47 | 3628 | `			}else if( nName == sizeof("log_errors")-1` |
|        29 | 3629 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 3630 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|         8 | 3631 | `			}` |
|        45 | 3632 | `		}` |
|        94 | 3633 | `		break;` |
|         - | 3634 | `								  }` |
|       ! 0 | 3635 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 3636 | `		/* error_log() consumer */` |
|       ! 0 | 3637 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 3638 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 3639 | `		break;` |
|         - | 3640 | `										}` |
|       ! 0 | 3641 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 3642 | `		/* Script return value */` |
|       ! 0 | 3643 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 3644 | `#ifdef UNTRUST` |
|         - | 3645 | `		if( ppValue == 0 ){` |
|         - | 3646 | `			rc = SXERR_CORRUPT;` |
|         - | 3647 | `			break;` |
|         - | 3648 | `		}` |
|         - | 3649 | `#endif` |
|       ! 0 | 3650 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 3651 | `		break;` |
|         - | 3652 | `								   }` |
|      7293 | 3653 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 3654 | `		/* Register an IO stream device */` |
|     14591 | 3655 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 3656 | `		/* Make sure we are dealing with a valid IO stream */` |
|     14586 | 3657 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     14591 | 3658 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|         - | 3659 | `				/* Invalid stream */` |
|       ! 0 | 3660 | `				rc = SXERR_INVALID;` |
|       ! 0 | 3661 | `				break;` |
|         - | 3662 | `		}` |
|     14591 | 3663 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 3664 | `			/* Make the 'file://' stream the defaut stream device */` |
|      3651 | 3665 | `			pVm->pDefStream = pStream;` |
|      1823 | 3666 | `		}` |
|         - | 3667 | `		/* Insert in the appropriate container */` |
|     14591 | 3668 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     14591 | 3669 | `		break;` |
|         - | 3670 | `								  }` |
|        16 | 3671 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 3672 | `		/* Point to the VM internal output consumer buffer */` |
|        32 | 3673 | `		const void **ppOut = va_arg(ap,const void **);` |
|        32 | 3674 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 3675 | `#ifdef UNTRUST` |
|         - | 3676 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 3677 | `			rc = SXERR_CORRUPT;` |
|         - | 3678 | `			break;` |
|         - | 3679 | `		}` |
|         - | 3680 | `#endif` |
|        32 | 3681 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        32 | 3682 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        32 | 3683 | `		break;` |
|         - | 3684 | `									   }` |
|        16 | 3685 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 3686 | `		/* Raw HTTP request*/` |
|        32 | 3687 | `		const char *zRequest = va_arg(ap,const char *);` |
|        32 | 3688 | `		int nByte = va_arg(ap,int);` |
|        32 | 3689 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 3690 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 3691 | `			break;` |
|         - | 3692 | `		}` |
|        32 | 3693 | `		if( nByte < 0 ){` |
|         - | 3694 | `			/* Compute length automatically */` |
|       ! 0 | 3695 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 3696 | `		}` |
|         - | 3697 | `		/* Process the request */` |
|        32 | 3698 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 3699 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        32 | 3700 | `		if( rc == SXRET_OK ){` |
|        30 | 3701 | `			pVm->bHttpContext = 1;` |
|        15 | 3702 | `		}` |
|        32 | 3703 | `		break;` |
|         - | 3704 | `									}` |
|        16 | 3705 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 3706 | `		/* Extract HTTP response status code */` |
|        32 | 3707 | `		int *pStatus = va_arg(ap, int *);` |
|        32 | 3708 | `		if( pStatus ){` |
|        32 | 3709 | `			*pStatus = pVm->iResponseStatus;` |
|        16 | 3710 | `		}` |
|        32 | 3711 | `		break;` |
|         - | 3712 | `										}` |
|        16 | 3713 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 3714 | `		/* Iterate response headers via callback */` |
|         - | 3715 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        32 | 3716 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        32 | 3717 | `		void *pUserData = va_arg(ap, void *);` |
|        32 | 3718 | `		if( xCallback ){` |
|        32 | 3719 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        32 | 3720 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|        44 | 3721 | `			for( k = 0; k < nHdr; k++ ){` |
|        18 | 3722 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        12 | 3723 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|         6 | 3724 | `							   pUserData);` |
|        12 | 3725 | `				if( rc != PH7_OK ){` |
|       ! 0 | 3726 | `					break;` |
|         - | 3727 | `				}` |
|         6 | 3728 | `			}` |
|        16 | 3729 | `		}` |
|        32 | 3730 | `		break;` |
|         - | 3731 | `										 }` |
|       ! 0 | 3732 | `	default:` |
|         - | 3733 | `		/* Unknown configuration option */` |
|       ! 0 | 3734 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 3735 | `		break;` |
|         - | 3736 | `	}` |
|     99071 | 3737 | `	return rc;` |
|         5 | 3738 | `}` |
|         - | 3739 | `/* Forward declaration */` |
|         - | 3740 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 3741 | `/*` |
|         - | 3742 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 3743 | ` * format.` |
|         - | 3744 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 3745 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 3746 | ` * (STDOUT).` |
|         - | 3747 | ` */` |
|         2 | 3748 | `static sxi32 VmByteCodeDump(` |
|         - | 3749 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 3750 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 3751 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 3752 | `	)` |
|         1 | 3753 | `{` |
|         - | 3754 | `	static const char zDump[] = {` |
|         - | 3755 | `		"====================================================\n"` |
|         - | 3756 | `		"PH7 VM Dump\n"` |
|         - | 3757 | `		"====================================================\n"` |
|         - | 3758 | `	};` |
|         - | 3759 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 3760 | `	sxi32 rc = SXRET_OK;` |
|         - | 3761 | `	sxu32 n;` |
|         - | 3762 | `	/* Point to the PH7 instructions */` |
|         3 | 3763 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 3764 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 3765 | `	n = 0;` |
|         3 | 3766 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 3767 | `	/* Dump instructions */` |
|         7 | 3768 | `	for(;;){` |
|        15 | 3769 | `		if( pInstr >= pEnd ){` |
|         - | 3770 | `			/* No more instructions */` |
|         3 | 3771 | `			break;` |
|         - | 3772 | `		}` |
|         - | 3773 | `		/* Format and call the consumer callback */` |
|        19 | 3774 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        12 | 3775 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        12 | 3776 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        13 | 3777 | `		if( rc != SXRET_OK ){` |
|         - | 3778 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 3779 | `			return rc;` |
|         - | 3780 | `		}` |
|        13 | 3781 | `		++n;` |
|        13 | 3782 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 3783 | `	}` |
|         3 | 3784 | `	return rc;` |
|         2 | 3785 | `}` |
|         - | 3786 | `/*` |
|         - | 3787 | ` * Save the execution state of a fiber/generator context.` |
|         - | 3788 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 3789 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 3790 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 3791 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 3792 | ` * when VmByteCodeExec returns.` |
|         - | 3793 | ` */` |
|      1666 | 3794 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 3795 | `	ph7_vm *pVm,` |
|         - | 3796 | `	ph7_exec_ctx *pCtx,` |
|         - | 3797 | `	sxi32 pc,` |
|         - | 3798 | `	sxi32 nTos` |
|         - | 3799 | `	)` |
|         5 | 3800 | `{` |
|       833 | 3801 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1671 | 3802 | `	pCtx->pc = pc;` |
|      1671 | 3803 | `	pCtx->nTos = nTos;` |
|      1671 | 3804 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1671 | 3805 | `	return PH7_SUSPEND;` |
|         5 | 3806 | `}` |
|         - | 3807 | `/*` |
|         - | 3808 | ` * Resolve named-argument mapping.` |
|         - | 3809 | ` *` |
|         - | 3810 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 3811 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 3812 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 3813 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 3814 | ` * every formal parameter that received a value.` |
|         - | 3815 | ` *` |
|         - | 3816 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 3817 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 3818 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 3819 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 3820 | ` */` |
|       330 | 3821 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 3822 | `	ph7_vm *pVm,` |
|         - | 3823 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 3824 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 3825 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 3826 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 3827 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 3828 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 3829 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 3830 | `)` |
|         4 | 3831 | `{` |
|       334 | 3832 | `	sxi32 posIdx = 0;` |
|         - | 3833 | `	sxu32 i;` |
|       334 | 3834 | `	int bSeenNamed = 0;` |
|         - | 3835 | `	char zErrMsg[256];` |
|       334 | 3836 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1216 | 3837 | `	for( i = 0; i < nActual; i++ ){` |
|       886 | 3838 | `		aSlot[i] = -2;` |
|       445 | 3839 | `	}` |
|      1204 | 3840 | `	for( i = 0; i < nActual; i++ ){` |
|      1140 | 3841 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 3842 | `			/* Named argument — find formal by name */` |
|       524 | 3843 | `			int found = 0;` |
|       524 | 3844 | `			bSeenNamed = 1;` |
|         - | 3845 | `			sxu32 k;` |
|       794 | 3846 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       632 | 3847 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       613 | 3848 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       586 | 3849 | `						pMap->aNames[i].zString,` |
|       879 | 3850 | `						pMap->aNames[i].nByte) == 0 ){` |
|       366 | 3851 | `					if( aUsed[k] ){` |
|        12 | 3852 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 3853 | `							"Named parameter $%.*s overwrites previous argument",` |
|         6 | 3854 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         9 | 3855 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 3856 | `					}` |
|       359 | 3857 | `					aSlot[i] = (sxi32)k;` |
|       359 | 3858 | `					aUsed[k] = 1;` |
|       359 | 3859 | `					found = 1;` |
|       359 | 3860 | `					break;` |
|         - | 3861 | `				}` |
|       139 | 3862 | `			}` |
|       518 | 3863 | `			if( !found ){` |
|       162 | 3864 | `				if( iVariadicIdx >= 0 ){` |
|       156 | 3865 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        80 | 3866 | `				}else{` |
|        11 | 3867 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 3868 | `						"Unknown named parameter $%.*s",` |
|         6 | 3869 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         8 | 3870 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 3871 | `				}` |
|        76 | 3872 | `			}` |
|       258 | 3873 | `		}else{` |
|         - | 3874 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 3875 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 3876 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 3877 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       366 | 3878 | `			if( bSeenNamed ){` |
|       ! 0 | 3879 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 3880 | `					"Cannot use positional argument after named argument",` |
|         - | 3881 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 3882 | `			}` |
|       366 | 3883 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        60 | 3884 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 3885 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 3886 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 3887 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 3888 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 3889 | `				}` |
|        60 | 3890 | `				aSlot[i] = posIdx;` |
|        60 | 3891 | `				aUsed[posIdx] = 1;` |
|       337 | 3892 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 3893 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 3894 | `			}` |
|       366 | 3895 | `			posIdx++;` |
|         - | 3896 | `		}` |
|       439 | 3897 | `	}` |
|       322 | 3898 | `	return SXRET_OK;` |
|       169 | 3899 | `}` |
|         - | 3900 | `/*` |
|         - | 3901 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 3902 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 3903 | ` */` |
|       324 | 3904 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         4 | 3905 | `{` |
|       328 | 3906 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       316 | 3907 | `		return 0;` |
|         - | 3908 | `	}` |
|        15 | 3909 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       166 | 3910 | `}` |
|         - | 3911 | `/*` |
|         - | 3912 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 3913 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 3914 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 3915 | ` */` |
|        10 | 3916 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 3917 | `{` |
|        11 | 3918 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|         5 | 3919 | `	(void)pVm;` |
|        11 | 3920 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|        11 | 3921 | `	return SXRET_OK;` |
|         1 | 3922 | `}` |
|         - | 3923 | `/*` |
|         - | 3924 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 3925 | ` * collect values positionally (keys ignored) into a temp array.` |
|         - | 3926 | ` */` |
|         6 | 3927 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 3928 | `{` |
|         3 | 3929 | `	(void)pVm; (void)pKey;` |
|         7 | 3930 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|         7 | 3931 | `	return SXRET_OK;` |
|         1 | 3932 | `}` |
|         - | 3933 | `/*` |
|         - | 3934 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 3935 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 3936 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 3937 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 3938 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 3939 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 3940 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 3941 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 3942 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 3943 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 3944 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 3945 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 3946 | ` */` |
|         - | 3947 | `/*` |
|         - | 3948 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 3949 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 3950 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 3951 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 3952 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 3953 | ` */` |
|       302 | 3954 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         4 | 3955 | `{` |
|         - | 3956 | `	VmSpreadRun sRun;` |
|         - | 3957 | `	ph7_hashmap_node *pNode;` |
|         - | 3958 | `	sxu32 i;` |
|       306 | 3959 | `	sRun.pStart = pFirst;` |
|       306 | 3960 | `	sRun.nCount = nCount;` |
|       306 | 3961 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|       306 | 3962 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       306 | 3963 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 3964 | `		return;` |
|         - | 3965 | `	}` |
|       306 | 3966 | `	pNode = pMap->pFirst;` |
|      2104 | 3967 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 3968 | `		VmSpreadKey sKey;` |
|      1802 | 3969 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 3970 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 3971 | `			 * the source map's release before CALL replays them. */` |
|        95 | 3972 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|        95 | 3973 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|        95 | 3974 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        48 | 3975 | `		}else{` |
|         - | 3976 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      1708 | 3977 | `			sKey.nOff = 0;` |
|      1708 | 3978 | `			sKey.nLen = 0;` |
|         - | 3979 | `		}` |
|      1802 | 3980 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      1802 | 3981 | `		pNode = pNode->pPrev; /* forward link */` |
|       903 | 3982 | `	}` |
|       155 | 3983 | `}` |
|         - | 3984 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 3985 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|         8 | 3986 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 3987 | `{` |
|         8 | 3988 | `	SySetReset(&pVm->aSpreadRun);` |
|         8 | 3989 | `	SySetReset(&pVm->aSpreadKey);` |
|         8 | 3990 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|         8 | 3991 | `}` |
|         - | 3992 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 3993 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 3994 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 3995 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 3996 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 3997 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 3998 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 3999 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 4000 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 4001 | ` * slot — from being consumed by that nested call. */` |
|       510 | 4002 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         4 | 4003 | `{` |
|       514 | 4004 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|       514 | 4005 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 4006 | `	VmSpreadRun *aRun;` |
|       514 | 4007 | `	if( rStart >= nRun ){` |
|       224 | 4008 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 4009 | `	}` |
|       294 | 4010 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       294 | 4011 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|       294 | 4012 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|       294 | 4013 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       145 | 4014 | `	}` |
|       294 | 4015 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|       259 | 4016 | `}` |
|         - | 4017 | `/*` |
|         - | 4018 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 4019 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 4020 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 4021 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 4022 | ` *` |
|         - | 4023 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 4024 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 4025 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 4026 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 4027 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 4028 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 4029 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 4030 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 4031 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 4032 | ` *` |
|         - | 4033 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 4034 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 4035 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 4036 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 4037 | ` */` |
|       318 | 4038 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         4 | 4039 | `{` |
|       322 | 4040 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4041 | `	VmSpreadRun *aRun;` |
|       322 | 4042 | `	ph7_value *pEnd = pTos;` |
|       322 | 4043 | `	sxi32 nPos = iP1;` |
|       322 | 4044 | `	sxi32 ri, extra = 0;` |
|       322 | 4045 | `	if( nRun == 0 ){` |
|        15 | 4046 | `		pVm->nSpreadCallBase = 0;` |
|        15 | 4047 | `		return 0;` |
|         - | 4048 | `	}` |
|       308 | 4049 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       308 | 4050 | `	ri = (sxi32)nRun - 1;` |
|       740 | 4051 | `	while( nPos > 0 ){` |
|       436 | 4052 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 4053 | `			/* A non-empty unpack occupying nCount slots. */` |
|       270 | 4054 | `			pEnd = aRun[ri].pStart;` |
|       270 | 4055 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|       270 | 4056 | `			ri--;` |
|       303 | 4057 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 4058 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|        39 | 4059 | `			extra -= 1;` |
|        39 | 4060 | `			ri--;` |
|        21 | 4061 | `		}else{` |
|         - | 4062 | `			/* An ordinary single-slot argument. */` |
|       134 | 4063 | `			pEnd--;` |
|         - | 4064 | `		}` |
|       436 | 4065 | `		nPos--;` |
|         4 | 4066 | `	}` |
|         - | 4067 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 4068 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|       308 | 4069 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|       308 | 4070 | `	return extra;` |
|       163 | 4071 | `}` |
|       302 | 4072 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)` |
|         4 | 4073 | `{` |
|       306 | 4074 | `	ph7_value *pTos = *ppTos;` |
|       306 | 4075 | `	sxu32 nEntry = pMap->nEntry;` |
|       306 | 4076 | `	if( nEntry == 0 ){` |
|         - | 4077 | `		/* Nothing to unpack — remove the source from the stack */` |
|        39 | 4078 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|        39 | 4079 | `		VmPopOperand(&pTos, 1);` |
|        21 | 4080 | `	}else{` |
|         - | 4081 | `		ph7_hashmap_node *pNode;` |
|         - | 4082 | `		ph7_value *pElem;` |
|         - | 4083 | `		sxu32 i;` |
|         - | 4084 | `		int bTemp;` |
|       270 | 4085 | `		pMap->iRef++;` |
|       270 | 4086 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 4087 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 4088 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|       270 | 4089 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 4090 | `		/* Overwrite the source slot with the first element */` |
|       270 | 4091 | `		pNode = pMap->pFirst;` |
|       270 | 4092 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|       270 | 4093 | `		PH7_MemObjRelease(pTos);` |
|       270 | 4094 | `		if( pElem ){` |
|       270 | 4095 | `			if( bTemp ){` |
|       137 | 4096 | `				PH7_MemObjStore(pElem, pTos);` |
|        69 | 4097 | `			}else{` |
|       134 | 4098 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 4099 | `			}` |
|       133 | 4100 | `		}` |
|       270 | 4101 | `		pTos->nIdx = SXU32_HIGH;` |
|         - | 4102 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 4103 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|       270 | 4104 | `		pNode = pNode->pPrev;` |
|         - | 4105 | `		/* Push the remaining elements */` |
|      1802 | 4106 | `		for( i = 1; i < nEntry; i++ ){` |
|      1536 | 4107 | `			pTos++;` |
|      1536 | 4108 | `			PH7_MemObjInit(pVm, pTos);` |
|      1536 | 4109 | `			pTos->nIdx = SXU32_HIGH;` |
|      1536 | 4110 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1536 | 4111 | `			if( pElem ){` |
|      1536 | 4112 | `				if( bTemp ){` |
|      1261 | 4113 | `					PH7_MemObjStore(pElem, pTos);` |
|       631 | 4114 | `				}else{` |
|       276 | 4115 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 4116 | `				}` |
|       766 | 4117 | `			}` |
|      1536 | 4118 | `			pNode = pNode->pPrev;` |
|       770 | 4119 | `		}` |
|       270 | 4120 | `		PH7_HashmapUnref(pMap);` |
|         - | 4121 | `	}` |
|       306 | 4122 | `	*ppTos = pTos;` |
|       306 | 4123 | `}` |
|         - | 4124 | `/*` |
|         - | 4125 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 4126 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 4127 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 4128 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 4129 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 4130 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 4131 | ` *` |
|         - | 4132 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 4133 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 4134 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 4135 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 4136 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 4137 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 4138 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 4139 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 4140 | ` * method-name slot pop shifts pArg).` |
|         - | 4141 | ` *` |
|         - | 4142 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 4143 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 4144 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 4145 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 4146 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 4147 | ` */` |
|       276 | 4148 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 4149 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         4 | 4150 | `{` |
|       280 | 4151 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4152 | `	VmSpreadRun *aRun;` |
|         - | 4153 | `	VmSpreadKey *aKey;` |
|         - | 4154 | `	const char *zKeyBase;` |
|       280 | 4155 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|       280 | 4156 | `	int bAnyNamed = 0;` |
|         - | 4157 | `	sxu32 ai, ci, ri, rStart;` |
|       280 | 4158 | `	if( nRun == 0 ){` |
|         - | 4159 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 4160 | `		return 0;` |
|         - | 4161 | `	}` |
|       280 | 4162 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       280 | 4163 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|       280 | 4164 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 4165 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 4166 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 4167 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 4168 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|       280 | 4169 | `	ri = pVm->nSpreadCallBase;` |
|       280 | 4170 | `	rStart = ri;` |
|       280 | 4171 | `	if( rStart >= nRun ){` |
|         - | 4172 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 4173 | `		return 0;` |
|         - | 4174 | `	}` |
|       280 | 4175 | `	SySetReset(&pVm->aEffArgName);` |
|       280 | 4176 | `	ci = 0;` |
|       280 | 4177 | `	ai = 0;` |
|       654 | 4178 | `	while( ai < nActual ){` |
|         - | 4179 | `		SyString sName;` |
|       378 | 4180 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 4181 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 4182 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|       390 | 4183 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 4184 | `			ci++; ri++;` |
|         1 | 4185 | `		}` |
|       378 | 4186 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 4187 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 4188 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 4189 | `			 * run never desyncs the key stream. */` |
|       268 | 4190 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      2062 | 4191 | `			for( j = 0; j < K; j++ ){` |
|      1798 | 4192 | `				SyZero(&sName, sizeof(sName));` |
|      1798 | 4193 | `				if( aKey[ks + j].nLen > 0 ){` |
|        95 | 4194 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|        95 | 4195 | `					bAnyNamed = 1;` |
|        47 | 4196 | `				}` |
|      1798 | 4197 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       901 | 4198 | `			}` |
|       268 | 4199 | `			ai += K;` |
|       268 | 4200 | `			ci++; ri++;` |
|       136 | 4201 | `		}else{` |
|         - | 4202 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|       114 | 4203 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        33 | 4204 | `				sName = pCompile->aNames[ci];` |
|        33 | 4205 | `				bAnyNamed = 1;` |
|        16 | 4206 | `			}` |
|       114 | 4207 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       114 | 4208 | `			ai++;` |
|       114 | 4209 | `			ci++;` |
|         - | 4210 | `		}` |
|         4 | 4211 | `	}` |
|         - | 4212 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 4213 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 4214 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 4215 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|       280 | 4216 | `	VmSpreadConsume(pVm);` |
|       280 | 4217 | `	if( !bAnyNamed ){` |
|         - | 4218 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|       210 | 4219 | `		return 0;` |
|         - | 4220 | `	}` |
|        71 | 4221 | `	pEff->bHasNamed = 1;` |
|        71 | 4222 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|        71 | 4223 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|        71 | 4224 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|        71 | 4225 | `	if( pCompile ){` |
|        33 | 4226 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        17 | 4227 | `	}else{` |
|        39 | 4228 | `		pEff->sAssertSrc.zString = 0;` |
|        39 | 4229 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 4230 | `	}` |
|        71 | 4231 | `	pEff->nTotal = nActual;` |
|        71 | 4232 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|        71 | 4233 | `	return 1;` |
|       142 | 4234 | `}` |
|         - | 4235 | `/*` |
|         - | 4236 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 4237 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 4238 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 4239 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 4240 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 4241 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 4242 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 4243 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 4244 | ` * pArg must be the site's FINAL argument base.` |
|         - | 4245 | ` */` |
|   7623446 | 4246 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 4247 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 4248 | `{` |
|   7623451 | 4249 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   7623451 | 4250 | `	if( pInstr->iP2 == 0 ){` |
|   7623149 | 4251 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 4252 | `	}` |
|       306 | 4253 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|        71 | 4254 | `		return pStorage;` |
|         - | 4255 | `	}` |
|       236 | 4256 | `	VmSpreadConsume(pVm);` |
|       236 | 4257 | `	return pCompile;` |
|   3812493 | 4258 | `}` |
|         - | 4259 | `/*` |
|         - | 4260 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 4261 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 4262 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 4263 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 4264 | ` */` |
|        14 | 4265 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         3 | 4266 | `{` |
|        17 | 4267 | `	const char *zType = "unknown";` |
|         - | 4268 | `	char zMsg[64];` |
|        17 | 4269 | `	if( iFlags & MEMOBJ_STRING ){` |
|         6 | 4270 | `		zType = "string";` |
|        14 | 4271 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 4272 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 4273 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 4274 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 4275 | `		zType = "float";` |
|        12 | 4276 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        10 | 4277 | `		zType = "int";` |
|         7 | 4278 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 4279 | `		zType = "bool";` |
|         1 | 4280 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 4281 | `		zType = "object";` |
|       ! 0 | 4282 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 4283 | `		zType = "resource";` |
|       ! 0 | 4284 | `	}` |
|        17 | 4285 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        17 | 4286 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        17 | 4287 | `}` |
|         - | 4288 | `/*` |
|         - | 4289 | ` * A member access in isset()/empty() context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY) is a silent` |
|         - | 4290 | ` * lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class instance"` |
|         - | 4291 | ` * warnings, mirroring the array isset/empty path.` |
|         - | 4292 | ` */` |
|      1148 | 4293 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 4294 | `{` |
|      1153 | 4295 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY;` |
|         5 | 4296 | `}` |
|         - | 4297 | `/*` |
|         - | 4298 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 4299 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 4300 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 4301 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 4302 | ` * reads (other names / other instances) still dispatch.` |
|         - | 4303 | ` */` |
|      1060 | 4304 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         1 | 4305 | `{` |
|         - | 4306 | `	VmMagicGuard *aG;` |
|         - | 4307 | `	sxu32 nHash;` |
|         - | 4308 | `	sxu32 n;` |
|      1061 | 4309 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 4310 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 4311 | `		 * every hooked-property access consults the guard, often twice. */` |
|       881 | 4312 | `		return FALSE;` |
|         - | 4313 | `	}` |
|       181 | 4314 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       181 | 4315 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       225 | 4316 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       181 | 4317 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       137 | 4318 | `			return TRUE;` |
|         - | 4319 | `		}` |
|        23 | 4320 | `	}` |
|        45 | 4321 | `	return FALSE;` |
|       531 | 4322 | `}` |
|       486 | 4323 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         1 | 4324 | `{` |
|         - | 4325 | `	VmMagicGuard sG;` |
|       487 | 4326 | `	sG.pThis = pThis;` |
|       487 | 4327 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       487 | 4328 | `	sG.cKind = cKind;` |
|       487 | 4329 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|       487 | 4330 | `}` |
|       486 | 4331 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         1 | 4332 | `{` |
|       487 | 4333 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|       487 | 4334 | `}` |
|         - | 4335 | `/*` |
|         - | 4336 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 4337 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 4338 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 4339 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 4340 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 4341 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 4342 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 4343 | ` * One-token lookahead only.` |
|         - | 4344 | ` */` |
|       790 | 4345 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         3 | 4346 | `{` |
|       793 | 4347 | `	switch( pNext->iOp ){` |
|        18 | 4348 | `		case PH7_OP_STORE:` |
|        39 | 4349 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|       ! 0 | 4350 | `		case PH7_OP_STORE_REF:` |
|       ! 0 | 4351 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|        11 | 4352 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 4353 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4354 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4355 | `		case PH7_OP_CAT_STORE:` |
|         - | 4356 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4357 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|        23 | 4358 | `			return 1;` |
|       366 | 4359 | `		default:` |
|       734 | 4360 | `			return 0;` |
|         - | 4361 | `	}` |
|       398 | 4362 | `}` |
|         - | 4363 | `/*` |
|         - | 4364 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 4365 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 4366 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 4367 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 4368 | ` */` |
|       424 | 4369 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         1 | 4370 | `{` |
|       425 | 4371 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         1 | 4372 | `}` |
|         - | 4373 | `/*` |
|         - | 4374 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 4375 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 4376 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 4377 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 4378 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 4379 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 4380 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 4381 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 4382 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 4383 | ` * abort path; SXRET_OK otherwise.` |
|         - | 4384 | ` */` |
|        54 | 4385 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         1 | 4386 | `{` |
|         - | 4387 | `	char zHName[384];` |
|         - | 4388 | `	sxu32 nHName;` |
|         - | 4389 | `	ph7_class_method *pSetHook;` |
|        55 | 4390 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 4391 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 4392 | `		SyBlob sErrMsg;` |
|         5 | 4393 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 4394 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 4395 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 4396 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 4397 | `		return SXRET_OK;` |
|         - | 4398 | `	}` |
|        51 | 4399 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 4400 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 4401 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 4402 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 4403 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 4404 | `			return SXRET_OK;` |
|         - | 4405 | `		}` |
|       ! 0 | 4406 | `	}` |
|        51 | 4407 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        51 | 4408 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        51 | 4409 | `	if( pSetHook ){` |
|         - | 4410 | `		ph7_value sHookRet;` |
|         - | 4411 | `		ph7_value *apHArg[1];` |
|        51 | 4412 | `		apHArg[0] = pValue;` |
|        51 | 4413 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        51 | 4414 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        51 | 4415 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        51 | 4416 | `		VmMagicGuardPop(pVm);` |
|        50 | 4417 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        27 | 4418 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         3 | 4419 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         3 | 4420 | `			if( rcH == SXRET_OK ){` |
|         3 | 4421 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|         3 | 4422 | `				if( pBack ){` |
|         3 | 4423 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         2 | 4424 | `				}` |
|         1 | 4425 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 4426 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 4427 | `				return PH7_ABORT;` |
|         - | 4428 | `			}` |
|         - | 4429 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 4430 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 4431 | `			 * parked throw. */` |
|         1 | 4432 | `		}` |
|        51 | 4433 | `		PH7_MemObjRelease(&sHookRet);` |
|        25 | 4434 | `	}` |
|        51 | 4435 | `	return SXRET_OK;` |
|        28 | 4436 | `}` |
|         - | 4437 | `/*` |
|         - | 4438 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 4439 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 4440 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 4441 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 4442 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 4443 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 4444 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 4445 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 4446 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 4447 | ` */` |
|         - | 4448 | `/*` |
|         - | 4449 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 4450 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 4451 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 4452 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 4453 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 4454 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 4455 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 4456 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 4457 | ` * caller reads the raw slot then.` |
|         - | 4458 | ` */` |
|       382 | 4459 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         4 | 4460 | `{` |
|       386 | 4461 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 4462 | `	char zHName[384];` |
|         - | 4463 | `	sxu32 nHName;` |
|         - | 4464 | `	ph7_class_method *pGetHook;` |
|         - | 4465 | `	sxi32 rc;` |
|       382 | 4466 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       260 | 4467 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       142 | 4468 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 4469 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 4470 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 4471 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 4472 | `		 * raw values whose output the routed throw then discards. */` |
|       248 | 4473 | `		return SXERR_NOTFOUND;` |
|         - | 4474 | `	}` |
|       139 | 4475 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       139 | 4476 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       139 | 4477 | `	if( pGetHook == 0 ){` |
|       ! 0 | 4478 | `		return SXERR_NOTFOUND;` |
|         - | 4479 | `	}` |
|       139 | 4480 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       139 | 4481 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       139 | 4482 | `	VmMagicGuardPop(pVm);` |
|       139 | 4483 | `	return rc;` |
|       195 | 4484 | `}` |
|         - | 4485 | `/*` |
|         - | 4486 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 4487 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 4488 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 4489 | ` */` |
|        20 | 4490 | `static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 4491 | `{` |
|        21 | 4492 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 4493 | `	VmSlot sFree;` |
|        21 | 4494 | `	if( pScr ){` |
|        21 | 4495 | `		PH7_MemObjRelease(pScr);` |
|        10 | 4496 | `	}` |
|        21 | 4497 | `	sFree.nIdx = nIdx;` |
|        21 | 4498 | `	sFree.pUserData = 0;` |
|        21 | 4499 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|        21 | 4500 | `}` |
|         - | 4501 | `/*` |
|         - | 4502 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 4503 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 4504 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 4505 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 4506 | ` * instance reference.` |
|         - | 4507 | ` */` |
|        16 | 4508 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 4509 | `{` |
|        18 | 4510 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        18 | 4511 | `	if( pEnt == 0 ){` |
|         5 | 4512 | `		return;` |
|         - | 4513 | `	}` |
|        13 | 4514 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|         5 | 4515 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         2 | 4516 | `	}` |
|        13 | 4517 | `	SyBlobRelease(&pEnt->sName);` |
|        13 | 4518 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        13 | 4519 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        10 | 4520 | `}` |
|        16 | 4521 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 4522 | `{` |
|         - | 4523 | `	VmHookRmw sEnt;` |
|         - | 4524 | `	VmHookRmw *pEnt;` |
|         - | 4525 | `	ph7_value *pScr;` |
|         - | 4526 | `	ph7_value sVal;` |
|        17 | 4527 | `	sxi32 rc = SXRET_OK;` |
|        17 | 4528 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        17 | 4529 | `	if( pEnt == 0 \|\| pEnt->iKind != VM_HOOK_PEND_RMW \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 4530 | `		return SXERR_NOTFOUND;` |
|         - | 4531 | `	}` |
|        17 | 4532 | `	sEnt = *pEnt;` |
|        17 | 4533 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 4534 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|         - | 4535 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|         - | 4536 | `	 * scratch index past this point). */` |
|        17 | 4537 | `	PH7_MemObjInit(pVm,&sVal);` |
|        17 | 4538 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        17 | 4539 | `	if( pScr ){` |
|        17 | 4540 | `		PH7_MemObjStore(pScr,&sVal);` |
|         8 | 4541 | `	}` |
|        17 | 4542 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        17 | 4543 | `	sVal.nIdx = SXU32_HIGH;` |
|        17 | 4544 | `	if( pVm->nBoundaryRc == 0 ){` |
|        15 | 4545 | `		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         7 | 4546 | `	}` |
|        17 | 4547 | `	PH7_MemObjRelease(&sVal);` |
|        17 | 4548 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        17 | 4549 | `	return rc;` |
|         9 | 4550 | `}` |
|         - | 4551 | `/*` |
|         - | 4552 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 4553 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 4554 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 4555 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 4556 | ` */` |
|        12 | 4557 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         1 | 4558 | `{` |
|        13 | 4559 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|        13 | 4560 | `	if( pSetMeth ){` |
|         - | 4561 | `		ph7_value sNameVal;` |
|         - | 4562 | `		ph7_value *apSetArg[2];` |
|        13 | 4563 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|        13 | 4564 | `		sNameVal.nIdx = SXU32_HIGH;` |
|        13 | 4565 | `		apSetArg[0] = &sNameVal;` |
|        13 | 4566 | `		apSetArg[1] = pValue;` |
|        13 | 4567 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|        13 | 4568 | `		PH7_VmCallClassMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|        13 | 4569 | `		VmMagicGuardPop(pVm);` |
|        13 | 4570 | `		PH7_MemObjRelease(&sNameVal);` |
|         6 | 4571 | `	}` |
|        13 | 4572 | `}` |
|         - | 4573 | `/*` |
|         - | 4574 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 4575 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 4576 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 4577 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 4578 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 4579 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 4580 | ` * path (the SyHash-layout incident class).` |
|         - | 4581 | ` */` |
|         - | 4582 | `/*` |
|         - | 4583 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 4584 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 4585 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 4586 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 4587 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 4588 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 4589 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 4590 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 4591 | ` * never linked (INIT error path).` |
|         - | 4592 | ` */` |
|     25252 | 4593 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 4594 | `{` |
|     25257 | 4595 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     25257 | 4596 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 4597 | `	sxu32 i;` |
|     27493 | 4598 | `	for( i = 0 ; i < n ; ++i ){` |
|     27493 | 4599 | `		if( apStep[i] == pStep ){` |
|     25265 | 4600 | `			for( ; i + 1 < n ; ++i ){` |
|         9 | 4601 | `				apStep[i] = apStep[i + 1];` |
|         5 | 4602 | `			}` |
|     25257 | 4603 | `			(void)SySetPop(&pInfo->aStep);` |
|     25257 | 4604 | `			return;` |
|         - | 4605 | `		}` |
|      1123 | 4606 | `	}` |
|     12631 | 4607 | `}` |
|       230 | 4608 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 4609 | `{` |
|       235 | 4610 | `	if( pStep->pOwner ){` |
|        24 | 4611 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|        11 | 4612 | `	}` |
|       235 | 4613 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       235 | 4614 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       235 | 4615 | `	PH7_ClassInstanceUnref(pThis);` |
|       235 | 4616 | `}` |
|         - | 4617 | `/*` |
|         - | 4618 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 4619 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 4620 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 4621 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 4622 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 4623 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 4624 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 4625 | ` * map's last reference.` |
|         - | 4626 | ` */` |
|     24996 | 4627 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 4628 | `{` |
|     25001 | 4629 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     25001 | 4630 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     25001 | 4631 | `	if( bPop ){` |
|         - | 4632 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 4633 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     25001 | 4634 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     12498 | 4635 | `	}` |
|     25001 | 4636 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     25001 | 4637 | `	PH7_HashmapUnref(pMap);` |
|     25001 | 4638 | `}` |
|         - | 4639 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 4640 | `/*` |
|         - | 4641 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 4642 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 4643 | ` * See block-comment on that function for additional information.` |
|         - | 4644 | ` */` |
|   7477428 | 4645 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 4646 | `{` |
|         - | 4647 | `	ph7_value *pStack;` |
|         - | 4648 | `	sxu32 nCap;` |
|         - | 4649 | `	sxi32 rc;` |
|         - | 4650 | `	/* Allocate a new operand stack */` |
|   7477433 | 4651 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   7477433 | 4652 | `	if( pStack == 0 ){` |
|       ! 0 | 4653 | `		return SXERR_MEM;` |
|         - | 4654 | `	}` |
|   7477433 | 4655 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 4656 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 4657 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   7477433 | 4658 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 4659 | `	/* Free the operand stack */` |
|   7477433 | 4660 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 4661 | `	/* Execution result */` |
|   7477433 | 4662 | `	return rc;` |
|   3738719 | 4663 | `}` |
|         - | 4664 | `/*` |
|         - | 4665 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 4666 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 4667 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 4668 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 4669 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 4670 | ` */` |
|        54 | 4671 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         1 | 4672 | `{` |
|        55 | 4673 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        55 | 4674 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 4675 | `	sxi32 rc;` |
|        55 | 4676 | `	if( pDeclCls ){` |
|        39 | 4677 | `		pVm->pConstEvalClass = pDeclCls;` |
|        39 | 4678 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        19 | 4679 | `	}` |
|        55 | 4680 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|        55 | 4681 | `	pVm->pConstEvalClass = pSaveCtx;` |
|        55 | 4682 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|        55 | 4683 | `	return rc;` |
|         1 | 4684 | `}` |
|         - | 4685 | `/*` |
|         - | 4686 | ` * Invoke any installed shutdown callbacks.` |
|         - | 4687 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|         - | 4688 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|         - | 4689 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|         - | 4690 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|         - | 4691 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|         - | 4692 | ` *` |
|         - | 4693 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|         - | 4694 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|         - | 4695 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|         - | 4696 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|         - | 4697 | ` * down and restore the default consumer.` |
|         - | 4698 | ` */` |
|      3650 | 4699 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 4700 | `{` |
|      3655 | 4701 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         - | 4702 | `	sxu32 n,nUsed;` |
|      3655 | 4703 | `	nUsed = SySetUsed(&pVm->aOB);` |
|      3655 | 4704 | `	if( nUsed < 1 ){` |
|      3653 | 4705 | `		return;` |
|         - | 4706 | `	}` |
|         7 | 4707 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|         5 | 4708 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|         5 | 4709 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|         5 | 4710 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|         5 | 4711 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|         2 | 4712 | `		}` |
|         3 | 4713 | `	}` |
|         - | 4714 | `	/* Restore the default consumer and release the buffers. */` |
|         3 | 4715 | `	pCons->xConsumer = pCons->xDef;` |
|         3 | 4716 | `	pCons->pUserData = pCons->pDefData;` |
|         7 | 4717 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|         5 | 4718 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|         5 | 4719 | `		if( pOb ){` |
|         5 | 4720 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|         5 | 4721 | `			SyBlobRelease(&pOb->sOB);` |
|         2 | 4722 | `		}` |
|         3 | 4723 | `	}` |
|         3 | 4724 | `	SySetReset(&pVm->aOB);` |
|         3 | 4725 | `	pVm->nObDepth = 0;` |
|      1830 | 4726 | `}` |
|         - | 4727 | `/*` |
|         - | 4728 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 4729 | ` * or more calls to [register_shutdown_function()].` |
|         - | 4730 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 4731 | ` * execution ends.` |
|         - | 4732 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 4733 | ` * additional information.` |
|         - | 4734 | ` */` |
|      3650 | 4735 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 4736 | `{` |
|         - | 4737 | `	VmShutdownCB *pEntry;` |
|         - | 4738 | `	ph7_value *apArg[10];` |
|         - | 4739 | `	sxu32 n,nEntry;` |
|         - | 4740 | `	int i;` |
|         - | 4741 | `	/* Point to the stack of registered callbacks */` |
|      3655 | 4742 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     40155 | 4743 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     36505 | 4744 | `		apArg[i] = 0;` |
|     18255 | 4745 | `	}` |
|         - | 4746 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 4747 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 4748 | `	 * callbacks, mirroring PHP.` |
|         - | 4749 | `	 */` |
|      3655 | 4750 | `	pVm->bHaltRequested = 0;` |
|      3673 | 4751 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        23 | 4752 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        23 | 4753 | `		if( pEntry ){` |
|         - | 4754 | `			/* Prepare callback arguments if any */` |
|        23 | 4755 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 4756 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 4757 | `					break;` |
|         - | 4758 | `				}` |
|       ! 0 | 4759 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 4760 | `			}` |
|         - | 4761 | `			/* Invoke the callback */` |
|        23 | 4762 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 4763 | `			/*` |
|         - | 4764 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 4765 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 4766 | `			 */` |
|        23 | 4767 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        23 | 4768 | `			if( pEntry ){` |
|        23 | 4769 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        23 | 4770 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 4771 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 4772 | `				}` |
|         9 | 4773 | `			}` |
|        23 | 4774 | `			if( pVm->bHaltRequested ){` |
|         - | 4775 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|       ! 0 | 4776 | `				break;` |
|         - | 4777 | `			}` |
|         9 | 4778 | `		}` |
|        14 | 4779 | `	}` |
|      3655 | 4780 | `	SySetReset(&pVm->aShutdown);` |
|      3655 | 4781 | `}` |
|         - | 4782 | `/*` |
|         - | 4783 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 4784 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 4785 | ` * See block-comment on that function for additional information.` |
|         - | 4786 | ` */` |
|      3650 | 4787 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 4788 | `{` |
|         - | 4789 | `	/* Make sure we are ready to execute this program */` |
|      3655 | 4790 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 4791 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 4792 | `	}` |
|         - | 4793 | `	/* Set the execution magic number  */` |
|      3655 | 4794 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 4795 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 4796 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 4797 | `	{` |
|      3655 | 4798 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|      3655 | 4799 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 4800 | `	}` |
|         - | 4801 | `	/* Invoke any shutdown callbacks */` |
|      3655 | 4802 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 4803 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 4804 | `	 * shutdown callbacks, which may still write into them. */` |
|      3655 | 4805 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 4806 | `	/*` |
|         - | 4807 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 4808 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 4809 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 4810 | `	 */` |
|      3655 | 4811 | `	return SXRET_OK;` |
|      1830 | 4812 | `}` |
|         - | 4813 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 4814 | `/*` |
|         - | 4815 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 4816 | ` * the desired message.` |
|         - | 4817 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 4818 | ` * in 'api.c' for additional information.` |
|         - | 4819 | ` */` |
|      3358 | 4820 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 4821 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 4822 | `	SyString *pString /* Message to output */` |
|         - | 4823 | `	)` |
|         5 | 4824 | `{` |
|      3363 | 4825 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      3363 | 4826 | `	sxi32 rc = SXRET_OK;` |
|         - | 4827 | `	/* Call the output consumer */` |
|      3363 | 4828 | `	if( pString->nByte > 0 ){` |
|      3363 | 4829 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      3363 | 4830 | `		VmTrackOutput(pVm, pString->nByte);` |
|      1679 | 4831 | `	}` |
|      3363 | 4832 | `	return rc;` |
|         5 | 4833 | `}` |
|         - | 4834 | `/*` |
|         - | 4835 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 4836 | ` * callback to consume the formatted message.` |
|         - | 4837 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 4838 | ` * in 'api.c' for additional information.` |
|         - | 4839 | ` */` |
|         2 | 4840 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 4841 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 4842 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 4843 | `	va_list ap           /* Variable list of arguments */` |
|         - | 4844 | `	)` |
|         1 | 4845 | `{` |
|         3 | 4846 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         3 | 4847 | `	sxi32 rc = SXRET_OK;` |
|         - | 4848 | `	SyBlob sWorker;` |
|         - | 4849 | `	/* Format the message and call the output consumer */` |
|         3 | 4850 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|         3 | 4851 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|         3 | 4852 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 4853 | `		/* Consume the formatted message */` |
|         3 | 4854 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|         1 | 4855 | `	}` |
|         3 | 4856 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 4857 | `	/* Release the working buffer */` |
|         3 | 4858 | `	SyBlobRelease(&sWorker);` |
|         3 | 4859 | `	return rc;` |
|         1 | 4860 | `}` |
|         - | 4861 | `/*` |
|         - | 4862 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 4863 | ` * This function never fail and always return a pointer` |
|         - | 4864 | ` * to a null terminated string.` |
|         - | 4865 | ` */` |
|        12 | 4866 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 4867 | `{` |
|        13 | 4868 | `	const char *zOp = "Unknown     ";` |
|        13 | 4869 | `	switch(nOp){` |
|         3 | 4870 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 4871 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 4872 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 4873 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 4874 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 4875 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 4876 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 4877 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 4878 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 4879 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 4880 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 4881 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 4882 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 4883 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 4884 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 4885 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 4886 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 4887 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 4888 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 4889 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 4890 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 4891 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 4892 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 4893 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 4894 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 4895 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 4896 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 4897 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 4898 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 4899 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 4900 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 4901 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 4902 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 4903 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 4904 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 4905 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 4906 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 4907 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 4908 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 4909 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 4910 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 4911 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 4912 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 4913 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 4914 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 4915 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 4916 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 4917 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 4918 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 4919 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 4920 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 4921 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 4922 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|         3 | 4923 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|       ! 0 | 4924 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|       ! 0 | 4925 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 4926 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 4927 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 4928 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 4929 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 4930 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 4931 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 4932 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 4933 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 4934 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 4935 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 4936 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 4937 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 4938 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 4939 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 4940 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 4941 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 4942 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 4943 | `	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;` |
|       ! 0 | 4944 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 4945 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 4946 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 4947 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 4948 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 4949 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 4950 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 4951 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 4952 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 4953 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 4954 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 4955 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 4956 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 4957 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 4958 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 4959 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 4960 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 4961 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 4962 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 4963 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 4964 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 4965 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 4966 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 4967 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 4968 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 4969 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 4970 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 4971 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 4972 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 4973 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 4974 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 4975 | `	default:` |
|       ! 0 | 4976 | `		break;` |
|         - | 4977 | `	}` |
|        13 | 4978 | `	return zOp;` |
|         1 | 4979 | `}` |
|         - | 4980 | `/*` |
|         - | 4981 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 4982 | ` * The xConsumer() callback which is an used defined function` |
|         - | 4983 | ` * is responsible of consuming the generated dump.` |
|         - | 4984 | ` */` |
|         2 | 4985 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 4986 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 4987 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 4988 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 4989 | `	)` |
|         1 | 4990 | `{` |
|         - | 4991 | `	sxi32 rc;` |
|         3 | 4992 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 4993 | `	return rc;` |
|         1 | 4994 | `}` |
|         - | 4995 | `/*` |
|         - | 4996 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 4997 | ` * outside a class body [i.e: global or function scope].` |
|         - | 4998 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 4999 | ` * in 'compile.c' for additional information.` |
|         - | 5000 | ` */` |
|    100136 | 5001 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         5 | 5002 | `{` |
|    100141 | 5003 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 5004 | `	/* Evaluate and expand constant value */` |
|    100141 | 5005 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|    100141 | 5006 | `}` |
|         - | 5007 | `/*` |
|         - | 5008 | ` * Section:` |
|         - | 5009 | ` *  Function handling functions.` |
|         - | 5010 | ` * Status:` |
|         - | 5011 | ` *    Stable.` |
|         - | 5012 | ` */` |
|         - | 5013 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 5014 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 5015 | `	{ "__phl_magic_call", vm_builtin_magic_call },` |
|         - | 5016 | `	{ "__phl_enum_cases",   vm_builtin_enum_cases },` |
|         - | 5017 | `	{ "__phl_enum_from",    vm_builtin_enum_from },` |
|         - | 5018 | `	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },` |
|         - | 5019 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 5020 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 5021 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 5022 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 5023 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 5024 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 5025 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 5026 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 5027 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 5028 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 5029 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 5030 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 5031 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 5032 | `	    /* Constants management */` |
|         - | 5033 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 5034 | `	{ "define",   vm_builtin_define               },` |
|         - | 5035 | `	{ "constant", vm_builtin_constant             },` |
|         - | 5036 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 5037 | `	   /* Class/Object functions */` |
|         - | 5038 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 5039 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 5040 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 5041 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 5042 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 5043 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 5044 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 5045 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 5046 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 5047 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 5048 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 5049 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 5050 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 5051 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 5052 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 5053 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 5054 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 5055 | `	   /* SPL object identity */` |
|         - | 5056 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 5057 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 5058 | `	   /* SPL Autoloading */` |
|         - | 5059 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 5060 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 5061 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 5062 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 5063 | `	   /* Random numbers/strings generators */` |
|         - | 5064 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 5065 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 5066 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 5067 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 5068 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 5069 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 5070 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 5071 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5072 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 5073 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 5074 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 5075 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5076 | `	   /* Language constructs functions */` |
|         - | 5077 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 5078 | `	{ "print", vm_builtin_print                   },` |
|         - | 5079 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 5080 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 5081 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 5082 | `	  /* Variable handling functions */` |
|         - | 5083 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 5084 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 5085 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 5086 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 5087 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 5088 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 5089 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 5090 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 5091 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 5092 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 5093 | `	  /* Ouput control functions */` |
|         - | 5094 | `	{ "flush",        vm_builtin_ob_flush          },` |
|         - | 5095 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 5096 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 5097 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 5098 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 5099 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 5100 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 5101 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|         - | 5102 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 5103 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 5104 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 5105 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|         - | 5106 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 5107 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 5108 | `	  /* Assertion functions */` |
|         - | 5109 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 5110 | `	  /* Error reporting functions */` |
|         - | 5111 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 5112 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 5113 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 5114 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 5115 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 5116 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 5117 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 5118 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 5119 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 5120 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 5121 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 5122 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 5123 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 5124 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 5125 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 5126 | `	  /* Release info */` |
|         - | 5127 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 5128 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 5129 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 5130 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 5131 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 5132 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 5133 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 5134 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 5135 | `	  /* hashmap */` |
|         - | 5136 | `	{"compact",          vm_builtin_compact       },` |
|         - | 5137 | `	{"extract",          vm_builtin_extract       },` |
|         - | 5138 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 5139 | `	  /* URL related function */` |
|         - | 5140 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 5141 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 5142 | `	   /* Command line processing */` |
|         - | 5143 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 5144 | `	   /* JSON encoding/decoding */` |
|         - | 5145 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 5146 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 5147 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 5148 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 5149 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 5150 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 5151 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 5152 | `	   /* Files/URI inclusion facility */` |
|         - | 5153 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 5154 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 5155 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 5156 | `	{ "include",      vm_builtin_include          },` |
|         - | 5157 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 5158 | `	{ "require",      vm_builtin_require          },` |
|         - | 5159 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 5160 | `};` |
|         - | 5161 | `/*` |
|         - | 5162 | ` * Register the built-in VM functions defined above.` |
|         - | 5163 | ` */` |
|      3646 | 5164 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 5165 | `{` |
|         - | 5166 | `	sxi32 rc;` |
|         - | 5167 | `	sxu32 n;` |
|    455755 | 5168 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 5169 | `		/* Note that these special functions have access` |
|         - | 5170 | `		 * to the underlying virtual machine as their` |
|         - | 5171 | `		 * private data.` |
|         - | 5172 | `		 */` |
|    452109 | 5173 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    452109 | 5174 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 5175 | `			return rc;` |
|         - | 5176 | `		}` |
|    226057 | 5177 | `	}` |
|      3651 | 5178 | `	return SXRET_OK;` |
|      1828 | 5179 | `}` |
|         - | 5180 | `/*` |
|         - | 5181 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 5182 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 5183 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 5184 | ` */` |
|   7073736 | 5185 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 5186 | `{` |
|   7073741 | 5187 | `	if( !iLoadable ){` |
|   5522009 | 5188 | `		return pClass;` |
|         - | 5189 | `	}` |
|   1551741 | 5190 | `	while(pClass){` |
|   1551737 | 5191 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   1551733 | 5192 | `			return pClass;` |
|         - | 5193 | `		}` |
|         5 | 5194 | `		pClass = pClass->pNextName;` |
|         1 | 5195 | `	}` |
|         5 | 5196 | `	return 0;` |
|   3536873 | 5197 | `}` |
|         - | 5198 | `/*` |
|         - | 5199 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 5200 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 5201 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 5202 | ` * registered in the VM's class table.` |
|         - | 5203 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 5204 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 5205 | ` */` |
|       412 | 5206 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5207 | `{` |
|         - | 5208 | `	VmAutoloadCB *pEntry;` |
|         - | 5209 | `	ph7_value sArg,sResult;` |
|         - | 5210 | `	SyHashEntry *pHashEntry;` |
|         - | 5211 | `	ph7_class *pClass;` |
|         - | 5212 | `	sxu32 n,nEntry;` |
|       417 | 5213 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       417 | 5214 | `	if( nEntry < 1 ){` |
|       331 | 5215 | `		return 0;` |
|         - | 5216 | `	}` |
|         - | 5217 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|        91 | 5218 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 5219 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 5220 | `	}` |
|         - | 5221 | `	/* Mark this class as being autoloaded */` |
|        89 | 5222 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 5223 | `	/* Prepare the class name argument */` |
|        89 | 5224 | `	PH7_MemObjInit(pVm,&sArg);` |
|        89 | 5225 | `	PH7_MemObjInit(pVm,&sResult);` |
|        89 | 5226 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|        89 | 5227 | `	pClass = 0;` |
|       157 | 5228 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 5229 | `		ph7_value *apArg[1];` |
|        99 | 5230 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        99 | 5231 | `		if( pEntry == 0 ){` |
|       ! 0 | 5232 | `			continue;` |
|         - | 5233 | `		}` |
|        99 | 5234 | `		apArg[0] = &sArg;` |
|        99 | 5235 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|         - | 5236 | `			/* Callback could not be invoked — skip to next autoloader */` |
|       ! 0 | 5237 | `			continue;` |
|         - | 5238 | `		}` |
|         - | 5239 | `		/* Check if the class is now available */` |
|        99 | 5240 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        99 | 5241 | `		if( pHashEntry ){` |
|        31 | 5242 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        31 | 5243 | `			if( pClass ){` |
|        31 | 5244 | `				break;` |
|         - | 5245 | `			}` |
|       ! 0 | 5246 | `		}` |
|        38 | 5247 | `	}` |
|        89 | 5248 | `	PH7_MemObjRelease(&sArg);` |
|        89 | 5249 | `	PH7_MemObjRelease(&sResult);` |
|         - | 5250 | `	/* Remove reentrancy guard */` |
|        89 | 5251 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        89 | 5252 | `	return pClass;` |
|       211 | 5253 | `}` |
|         - | 5254 | `/*` |
|         - | 5255 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 5256 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 5257 | ` */` |
|        42 | 5258 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5259 | `{` |
|        47 | 5260 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         5 | 5261 | `}` |
|         - | 5262 | `/*` |
|         - | 5263 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 5264 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 5265 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 5266 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 5267 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 5268 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 5269 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 5270 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 5271 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 5272 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 5273 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 5274 | ` */` |
|   7079454 | 5275 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 5276 | `{` |
|   7079459 | 5277 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        59 | 5278 | `		(*pzName)++;` |
|        59 | 5279 | `		(*pnByte)--;` |
|        28 | 5280 | `	}` |
|   7079459 | 5281 | `}` |
|         - | 5282 | `/*` |
|         - | 5283 | ` * Check if the given name refer to an installed class.` |
|         - | 5284 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 5285 | ` */` |
|   7074080 | 5286 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 5287 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 5288 | `	const char *zName,  /* Name of the target class */` |
|         - | 5289 | `	sxu32 nByte,        /* zName length */` |
|         - | 5290 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 5291 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 5292 | `						 */` |
|         - | 5293 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 5294 | `	)` |
|         5 | 5295 | `{` |
|         - | 5296 | `	SyHashEntry *pEntry;` |
|         - | 5297 | `	ph7_class *pClass;` |
|   7074085 | 5298 | `	sxu32 nOrig = nByte;` |
|   3537040 | 5299 | `	SXUNUSED(iNest);` |
|         - | 5300 | `	/* Exact class lookup.` |
|         - | 5301 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 5302 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 5303 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 5304 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   7074085 | 5305 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 5306 | `	/* An empty stripped name never matches a stored key (none is empty); skip the` |
|         - | 5307 | `	 * hash probe. But php still fires the autoloader when the ORIGINAL name was` |
|         - | 5308 | `	 * non-empty — a lone "\" autoloads with the empty stripped name, whereas a` |
|         - | 5309 | `	 * truly empty "" does not. Gate autoload on nOrig, pass the stripped name. */` |
|   7074085 | 5310 | `	pEntry = nByte > 0 ? SyHashGet(&pVm->hClass,(const void *)zName,nByte) : 0;` |
|   7074085 | 5311 | `	if( pEntry == 0 ){` |
|         - | 5312 | `		/* Class not found in hash table — try autoload before giving up */` |
|       375 | 5313 | `		return nOrig > 0 ? VmTriggerAutoload(pVm,zName,nByte,iLoadable) : 0;` |
|         - | 5314 | `	}` |
|   7073715 | 5315 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   7073715 | 5316 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   3537045 | 5317 | `}` |
|         - | 5318 | `/*` |
|         - | 5319 | ` * Reference Table Implementation` |
|         - | 5320 | ` * Status: stable <chm@symisc.net>` |
|         - | 5321 | ` * Intro` |
|         - | 5322 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 5323 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 5324 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 5325 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 5326 | ` *  Refer to the official for more information on this powerful` |
|         - | 5327 | ` *  extension.` |
|         - | 5328 | ` */` |
|         - | 5329 | `/*` |
|         - | 5330 | ` * Allocate a new reference entry.` |
|         - | 5331 | ` */` |
|  23069385 | 5332 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 5333 | `{` |
|         - | 5334 | `	VmRefObj *pRef;` |
|         - | 5335 | `	/* Allocate a new instance */` |
|  23069390 | 5336 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  23069390 | 5337 | `	if( pRef == 0 ){` |
|       ! 0 | 5338 | `		return 0;` |
|         - | 5339 | `	}` |
|         - | 5340 | `	/* Zero the structure */` |
|  23069390 | 5341 | `	SyZero(pRef,sizeof(VmRefObj));` |
|         - | 5342 | `	/* Initialize fields */` |
|  23069390 | 5343 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  23069390 | 5344 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  23069390 | 5345 | `	pRef->nIdx = nIdx;` |
|  23069390 | 5346 | `	return pRef;` |
|  11535672 | 5347 | `}` |
|         - | 5348 | `/*` |
|         - | 5349 | ` * Default hash function used by the reference table` |
|         - | 5350 | ` * for lookup/insertion operations.` |
|         - | 5351 | ` */` |
| 107763409 | 5352 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|         5 | 5353 | `{` |
|         - | 5354 | `	/* Calculate the hash based on the memory object index */` |
| 107763414 | 5355 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|         5 | 5356 | `}` |
|         - | 5357 | `/*` |
|         - | 5358 | ` * Check if a memory object [i.e: a variable] is already installed` |
|         - | 5359 | ` * in the reference table.` |
|         - | 5360 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|         - | 5361 | ` * otherwise.` |
|         - | 5362 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5363 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5364 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5365 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5366 | ` * Refer to the official for more information on this powerful` |
|         - | 5367 | ` * extension.` |
|         - | 5368 | ` */` |
|  60420975 | 5369 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|         5 | 5370 | `{` |
|         - | 5371 | `	VmRefObj *pRef;` |
|         - | 5372 | `	sxu32 nBucket;` |
|         - | 5373 | `	/* Point to the appropriate bucket */` |
|  60420980 | 5374 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|         - | 5375 | `	/* Perform the lookup */` |
|  60420980 | 5376 | `	pRef = pVm->apRefObj[nBucket];` |
| 305345015 | 5377 | `	for(;;){` |
| 610684496 | 5378 | `		if( pRef == 0 ){` |
|  29818332 | 5379 | `			break;` |
|         - | 5380 | `		}` |
| 580866169 | 5381 | `		if( pRef->nIdx == nObjIdx ){` |
|         - | 5382 | `			/* Entry found */` |
|  30602653 | 5383 | `			return pRef;` |
|         - | 5384 | `		}` |
|         - | 5385 | `		/* Point to the next entry */` |
| 550263521 | 5386 | `		pRef = pRef->pNextCollide;` |
|         5 | 5387 | `	}` |
|         - | 5388 | `	/* No such entry,return NULL */` |
|  29818332 | 5389 | `	return 0;` |
|  30213416 | 5390 | `}` |
|         - | 5391 | `/*` |
|         - | 5392 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 5393 | ` *` |
|         - | 5394 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5395 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5396 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5397 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5398 | ` * Refer to the official for more information on this powerful` |
|         - | 5399 | ` * extension.` |
|         - | 5400 | ` */` |
|  23069385 | 5401 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 5402 | `{` |
|         - | 5403 | `	sxu32 nBucket;` |
|  23069390 | 5404 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|         - | 5405 | `		VmRefObj **apNew;` |
|         - | 5406 | `		sxu32 nNew;` |
|         - | 5407 | `		/* Allocate a larger table */` |
|     11397 | 5408 | `		nNew = pVm->nRefSize << 1;` |
|     11397 | 5409 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     11397 | 5410 | `		if( apNew ){` |
|     11397 | 5411 | `			VmRefObj *pEntry = pVm->pRefList;` |
|         - | 5412 | `			sxu32 n;` |
|         - | 5413 | `			/* Zero the structure */` |
|     11397 | 5414 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|         - | 5415 | `			/* Rehash all referenced entries */` |
|   3681597 | 5416 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|         - | 5417 | `				/* Remove old collision links */` |
|   3670205 | 5418 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 5419 | `				/* Point to the appropriate bucket */` |
|   3670205 | 5420 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|         - | 5421 | `				/* Insert the entry  */` |
|   3670205 | 5422 | `				pEntry->pNextCollide = apNew[nBucket];` |
|   3670205 | 5423 | `				if( apNew[nBucket] ){` |
|   2965535 | 5424 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|   1482765 | 5425 | `				}` |
|   3670205 | 5426 | `				apNew[nBucket] = pEntry;` |
|         - | 5427 | `				/* Point to the next entry */` |
|   3670205 | 5428 | `				pEntry = pEntry->pNext;` |
|   1835105 | 5429 | `			}` |
|         - | 5430 | `			/* Release the old table */` |
|     11397 | 5431 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|         - | 5432 | `			/* Install the new one */` |
|     11397 | 5433 | `			pVm->apRefObj = apNew;` |
|     11397 | 5434 | `			pVm->nRefSize = nNew;` |
|      5696 | 5435 | `		}` |
|      5696 | 5436 | `	}` |
|         - | 5437 | `	/* Point to the appropriate bucket */` |
|  23069390 | 5438 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|         - | 5439 | `	/* Insert the entry */` |
|  23069390 | 5440 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  23069390 | 5441 | `	if( pVm->apRefObj[nBucket] ){` |
|  20869409 | 5442 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  10435352 | 5443 | `	}` |
|  23069390 | 5444 | `	pVm->apRefObj[nBucket] = pRef;` |
|  23069390 | 5445 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  23069390 | 5446 | `	pVm->nRefUsed++;` |
|  23069390 | 5447 | `	return SXRET_OK;` |
|         5 | 5448 | `}` |
|         - | 5449 | `/*` |
|         - | 5450 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|         - | 5451 | ` * the reference table.` |
|         - | 5452 | ` * This function is invoked when the user perform an unset` |
|         - | 5453 | ` * call [i.e: unset($var); ].` |
|         - | 5454 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5455 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5456 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5457 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5458 | ` * Refer to the official for more information on this powerful` |
|         - | 5459 | ` * extension.` |
|         - | 5460 | ` */` |
|  22323173 | 5461 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 5462 | `{` |
|         - | 5463 | `	ph7_hashmap_node **apNode;` |
|         - | 5464 | `	SyHashEntry **apEntry;` |
|         - | 5465 | `	sxu32 n;` |
|         - | 5466 | `	/* Point to the reference table */` |
|  22323178 | 5467 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  22323178 | 5468 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|         - | 5469 | `	/* Unlink the entry from the reference table */` |
|  29078238 | 5470 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   6755065 | 5471 | `		if( apEntry[n] ){` |
|   6749181 | 5472 | `			SyHashDeleteEntry2(apEntry[n]);` |
|   3375563 | 5473 | `		}` |
|   3378510 | 5474 | `	}` |
|  29807611 | 5475 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   7484438 | 5476 | `		if( apNode[n] ){` |
|      1500 | 5477 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|       748 | 5478 | `		}` |
|   3742221 | 5479 | `	}` |
|  22323178 | 5480 | `	if( pRef->pPrevCollide ){` |
|   1720329 | 5481 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|    859839 | 5482 | `	}else{` |
|  20602854 | 5483 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|         - | 5484 | `	}` |
|  22323178 | 5485 | `	if( pRef->pNextCollide ){` |
|  19176507 | 5486 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   9588901 | 5487 | `	}` |
|  22323178 | 5488 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|         - | 5489 | `	/* Release the node */` |
|  22323178 | 5490 | `	SySetRelease(&pRef->aReference);` |
|  22323178 | 5491 | `	SySetRelease(&pRef->aArrEntries);` |
|  22323178 | 5492 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  22323178 | 5493 | `	pVm->nRefUsed--;` |
|  22323178 | 5494 | `	return SXRET_OK;` |
|         5 | 5495 | `}` |
|         - | 5496 | `/*` |
|         - | 5497 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 5498 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5499 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5500 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5501 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5502 | ` * Refer to the official for more information on this powerful` |
|         - | 5503 | ` * extension.` |
|         - | 5504 | ` */` |
|  23119259 | 5505 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 5506 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 5507 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 5508 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 5509 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 5510 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 5511 | `	)` |
|         5 | 5512 | `{` |
|  23119264 | 5513 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 5514 | `	VmRefObj *pRef;` |
|         - | 5515 | `	/* Check if the referenced object already exists */` |
|  23119264 | 5516 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  23119264 | 5517 | `	if( pRef == 0 ){` |
|         - | 5518 | `		/* Create a new entry */` |
|  23069390 | 5519 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  23069390 | 5520 | `		if( pRef == 0 ){` |
|       ! 0 | 5521 | `			return SXERR_MEM;` |
|         - | 5522 | `		}` |
|  23069390 | 5523 | `		pRef->iFlags = iFlags;` |
|         - | 5524 | `		/* Install the entry */` |
|  23069390 | 5525 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  11535667 | 5526 | `	}` |
|  23119264 | 5527 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  23119264 | 5528 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 5529 | `		VmSlot sRef;` |
|         - | 5530 | `		/* Local frame,record referenced entry so that it can` |
|         - | 5531 | `		 * be deleted when we leave this frame.` |
|         - | 5532 | `		 */` |
|   6749149 | 5533 | `		sRef.nIdx = nIdx;` |
|   6749149 | 5534 | `		sRef.pUserData = pEntry;` |
|   6749149 | 5535 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 5536 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 5537 | `		}` |
|   3375547 | 5538 | `	}` |
|  23119264 | 5539 | `	if( pEntry ){` |
|         - | 5540 | `		/* Address of the hash-entry */` |
|   6798745 | 5541 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|   3400345 | 5542 | `	}` |
|  23119264 | 5543 | `	if( pMapEntry ){` |
|         - | 5544 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|   7595470 | 5545 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|   3797732 | 5546 | `	}` |
|  23119264 | 5547 | `	return SXRET_OK;` |
|  11560609 | 5548 | `}` |
|         - | 5549 | `/*` |
|         - | 5550 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 5551 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 5552 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 5553 | ` * the reference implementation is consistent,solid and it's` |
|         - | 5554 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 5555 | ` * Refer to the official for more information on this powerful` |
|         - | 5556 | ` * extension.` |
|         - | 5557 | ` */` |
|  14226179 | 5558 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 5559 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 5560 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 5561 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 5562 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 5563 | `	)` |
|         5 | 5564 | `{` |
|         - | 5565 | `	VmRefObj *pRef;` |
|         - | 5566 | `	sxu32 n;` |
|         - | 5567 | `	/* Check if the referenced object already exists */` |
|  14226184 | 5568 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  14226184 | 5569 | `	if( pRef == 0 ){` |
|         - | 5570 | `		/* Not such entry */` |
|   6748947 | 5571 | `		return SXERR_NOTFOUND;` |
|         - | 5572 | `	}` |
|         - | 5573 | `	/* Remove the desired entry */` |
|   7477242 | 5574 | `	if( pEntry ){` |
|         - | 5575 | `		SyHashEntry **apEntry;` |
|        91 | 5576 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|       279 | 5577 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       193 | 5578 | `			if( apEntry[n] == pEntry ){` |
|         - | 5579 | `				/* Nullify the entry */` |
|        89 | 5580 | `				apEntry[n] = 0;` |
|         - | 5581 | `				/*` |
|         - | 5582 | `				 * NOTE:` |
|         - | 5583 | `				 * In future releases,think to add a free pool of entries,so that` |
|         - | 5584 | `				 * we avoid wasting spaces.` |
|         - | 5585 | `				 */` |
|        42 | 5586 | `			}` |
|        99 | 5587 | `		}` |
|        43 | 5588 | `	}` |
|   7477242 | 5589 | `	if( pMapEntry ){` |
|         - | 5590 | `		ph7_hashmap_node **apNode;` |
|   7477156 | 5591 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  14954571 | 5592 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   7477420 | 5593 | `			if( apNode[n] == pMapEntry ){` |
|         - | 5594 | `				/* nullify the entry */` |
|   7477156 | 5595 | `				apNode[n] = 0;` |
|   3738575 | 5596 | `			}` |
|   3738712 | 5597 | `		}` |
|   3738575 | 5598 | `	}` |
|   7477242 | 5599 | `	return SXRET_OK;` |
|   7114069 | 5600 | `}` |
|         - | 5601 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 5602 | `/*` |
|         - | 5603 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 5604 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 5605 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 5606 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 5607 | ` * For more information on how to register IO stream devices,please` |
|         - | 5608 | ` * refer to the official documentation.` |
|         - | 5609 | ` */` |
|     31090 | 5610 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 5611 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 5612 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 5613 | `	int nByte              /* *pzDevice length*/` |
|         - | 5614 | `	)` |
|         5 | 5615 | `{` |
|         - | 5616 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|         - | 5617 | `	ph7_io_stream **apStream,*pStream;` |
|         - | 5618 | `	SyString sDev,sCur;` |
|         - | 5619 | `	sxu32 n,nEntry;` |
|         - | 5620 | `	int rc;` |
|         - | 5621 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|     31095 | 5622 | `	zNext = zCur = zIn = *pzDevice;` |
|     31095 | 5623 | `	zEnd = &zIn[nByte];` |
|   1972985 | 5624 | `	while( zIn < zEnd ){` |
|   1941951 | 5625 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|         - | 5626 | `			/* Got one */` |
|        59 | 5627 | `			zNext = &zIn[sizeof("://")-1];` |
|        59 | 5628 | `			break;` |
|         - | 5629 | `		}` |
|         - | 5630 | `		/* Advance the cursor */` |
|   1941895 | 5631 | `		zIn++;` |
|         5 | 5632 | `	}` |
|     31095 | 5633 | `	if( zIn >= zEnd ){` |
|         - | 5634 | `		/* No such scheme,return the default stream */` |
|     31039 | 5635 | `		return pVm->pDefStream;` |
|         - | 5636 | `	}` |
|        59 | 5637 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|         - | 5638 | `	/* Remove leading and trailing white spaces */` |
|        59 | 5639 | `	SyStringFullTrim(&sDev);` |
|         - | 5640 | `	/* Perform a linear lookup on the installed stream devices */` |
|        59 | 5641 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        59 | 5642 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        93 | 5643 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        93 | 5644 | `		pStream = apStream[n];` |
|        93 | 5645 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|         - | 5646 | `		/* Perfrom a case-insensitive comparison */` |
|        93 | 5647 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        93 | 5648 | `		if( rc == 0 ){` |
|         - | 5649 | `			/* Stream device found */` |
|        59 | 5650 | `			*pzDevice = zNext;` |
|        59 | 5651 | `			return pStream;` |
|         - | 5652 | `		}` |
|        19 | 5653 | `	}` |
|         - | 5654 | `	/* No such stream,return NULL */` |
|       ! 0 | 5655 | `	return 0;` |
|     15550 | 5656 | `}` |
|         - | 5657 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 5658 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 5659 |  |
