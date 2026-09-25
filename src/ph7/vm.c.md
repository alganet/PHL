# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2917/3397 lines (85.87%)

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
|   2154927 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   77 | `{` |
|   2154932 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|        42 |   79 | `		return TRUE;` |
|         - |   80 | `	}` |
|   2154892 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        11 |   82 | `		return TRUE;` |
|         - |   83 | `	}` |
|   2154882 |   84 | `	return FALSE;` |
|   1079185 |   85 | `}` |
|         - |   86 | `/*` |
|         - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |   93 | ` * still go through the existing numeric coercion.` |
|         - |   94 | ` */` |
|    696856 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |   96 | `{` |
|         - |   97 | `	SyString sStr;` |
|    696861 |   98 | `	sxu8 bReal = FALSE;` |
|    696861 |   99 | `	const char *zTail = 0;` |
|         - |  100 | `	const char *zEnd;` |
|    696861 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    696843 |  102 | `		return FALSE;` |
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
|    348864 |  119 | `}` |
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
|   2485386 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  139 | `	const SyString *pName,  /* Constant name */` |
|         - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  142 | `	)` |
|         5 |  143 | `{` |
|   2485391 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  145 | `}` |
|         - |  146 | `/*` |
|         - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  149 | ` */` |
|   2485682 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|   2485687 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   2485687 |  165 | `	if( pEntry ){` |
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
|   2485685 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   2485685 |  190 | `	if( pCons == 0 ){` |
|       ! 0 |  191 | `		return 0;` |
|         - |  192 | `	}` |
|         - |  193 | `	/* Duplicate constant name */` |
|   2485685 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   2485685 |  195 | `	if( zDupName == 0 ){` |
|       ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  197 | `		return 0;` |
|         - |  198 | `	}` |
|   2485685 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|   2485685 |  200 | `	if( pFile ){` |
|       299 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       147 |  202 | `	}` |
|   2485685 |  203 | `	pCons->nLine = nLine;` |
|   2485685 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         - |  205 | `	/* Install the constant */` |
|   2485685 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   2485685 |  207 | `	pCons->xExpand = xExpand;` |
|   2485685 |  208 | `	pCons->pUserData = pUserData;` |
|   2485685 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   2485685 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   2485685 |  211 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  214 | `		return rc;` |
|         - |  215 | `	}` |
|         - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|   2485685 |  217 | `	return SXRET_OK;` |
|   1242846 |  218 | `}` |
|         - |  219 | `/*` |
|         - |  220 | ` * Allocate a new foreign function instance.` |
|         - |  221 | ` * This function return SXRET_OK on success. Any other` |
|         - |  222 | ` * return value indicates failure.` |
|         - |  223 | ` * Please refer to the official documentation for an introduction to` |
|         - |  224 | ` * the foreign function mechanism.` |
|         - |  225 | ` */` |
|   7328364 |  226 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
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
|   7328369 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   7328369 |  238 | `	if( pFunc == 0 ){` |
|       ! 0 |  239 | `		return SXERR_MEM;` |
|         - |  240 | `	}` |
|         - |  241 | `	/* Duplicate function name */` |
|   7328369 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   7328369 |  243 | `	if( zDup == 0 ){` |
|       ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  245 | `		return SXERR_MEM;` |
|         - |  246 | `	}` |
|         - |  247 | `	/* Zero the structure */` |
|   7328369 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  249 | `	/* Initialize structure fields */` |
|   7328369 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   7328369 |  251 | `	pFunc->pVm   = pVm;` |
|   7328369 |  252 | `	pFunc->xFunc = xFunc;` |
|   7328369 |  253 | `	pFunc->pUserData = pUserData;` |
|   7328369 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  255 | `	/* Write a pointer to the new function */` |
|   7328369 |  256 | `	*ppOut = pFunc;` |
|   7328369 |  257 | `	return SXRET_OK;` |
|   3664187 |  258 | `}` |
|         - |  259 | `/*` |
|         - |  260 | ` * Install a foreign function and it's associated callback so that` |
|         - |  261 | ` * it can be invoked from the target PHP code.` |
|         - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  263 | ` * return value indicates failure.` |
|         - |  264 | ` * Please refer to the official documentation for an introduction to` |
|         - |  265 | ` * the foreign function mechanism.` |
|         - |  266 | ` */` |
|   3175332 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   3175337 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   3175337 |  279 | `	if( pEntry ){` |
|       ! 0 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|       ! 0 |  281 | `		pFunc->pUserData = pUserData;` |
|       ! 0 |  282 | `		pFunc->xFunc = xFunc;` |
|       ! 0 |  283 | `		SySetReset(&pFunc->aAux);` |
|         - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|       ! 0 |  288 | `		pFunc->nMinArg  = 0;` |
|       ! 0 |  289 | `		pFunc->nMaxArg  = 0;` |
|       ! 0 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|       ! 0 |  291 | `		pFunc->bAtLeast = 0;` |
|       ! 0 |  292 | `		return SXRET_OK;` |
|         - |  293 | `	}` |
|         - |  294 | `	/* Create a new user function */` |
|   3175337 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   3175337 |  296 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  297 | `		return rc;` |
|         - |  298 | `	}` |
|         - |  299 | `	/* Install the function in the corresponding hashtable */` |
|   3175337 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   3175337 |  301 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  304 | `		return rc;` |
|         - |  305 | `	}` |
|         - |  306 | `	/* User function successfully installed */` |
|   3175337 |  307 | `	return SXRET_OK;` |
|   1587671 |  308 | `}` |
|         - |  309 | `/*` |
|         - |  310 | ` * Initialize a VM function.` |
|         - |  311 | ` */` |
|   4310538 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  315 | `	const char *zName,  /* Function name */` |
|         - |  316 | `	sxu32 nByte,        /* zName length */` |
|         - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  318 | `	void *pUserData     /* Function private data */` |
|         - |  319 | `	)` |
|         5 |  320 | `{` |
|         - |  321 | `	/* Zero the structure */` |
|   4310543 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  323 | `	/* Initialize structure fields */` |
|         - |  324 | `	/* Arguments container */` |
|   4310543 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  326 | `	/* Static variable container */` |
|   4310543 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  328 | `	/* Bytecode container */` |
|   4310543 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  330 | `    /* Preallocate some instruction slots */` |
|   4310543 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  332 | `	/* Closure environment */` |
|   4310543 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   4310543 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  336 | `	/* Declared #[...] attributes */` |
|   4310543 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   4310543 |  338 | `	pFunc->iFlags = iFlags;` |
|   4310543 |  339 | `	pFunc->pUserData = pUserData;` |
|         - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   4310543 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   4310543 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   4296915 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   2148460 |  346 | `	}else{` |
|         - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     13633 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     13633 |  351 | `		if( pFile ){` |
|     13633 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|      6814 |  353 | `		}` |
|         - |  354 | `	}` |
|   4310543 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   4310543 |  356 | `	return SXRET_OK;` |
|         5 |  357 | `}` |
|         - |  358 | `/*` |
|         - |  359 | ` * Look a name up in the compiled-function table AS A SCRIPT SPELLS IT.` |
|         - |  360 | ` *` |
|         - |  361 | ` * hFunction is the ENGINE's table, not the script's. Besides the functions a program` |
|         - |  362 | ` * declared it holds every mounted class METHOD -- VmMountUserClassMethods installs each` |
|         - |  363 | `` * one under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c mints -- and`` |
|         - |  364 | `` * every compiled CLOSURE, under `[closure_N]`. Neither is a php function name (no php`` |
|         - |  365 | `` * label may hold a `[`, an `@` or a `]`), and php has no table in which a script can find`` |
|         - |  366 | ` * one.` |
|         - |  367 | ` *` |
|         - |  368 | ` * A plain SyHashGet therefore answered a name that does not exist to every surface that` |
|         - |  369 | ` * asks whether a function does: function_exists(), is_callable() and the whole callback` |
|         - |  370 | `` * screen behind it, ReflectionFunction, `new Fiber(name)` -- and the dispatch itself.`` |
|         - |  371 | ` * Reaching a METHOD that way really did run it: the body assumes the receiver frame the` |
|         - |  372 | `` * plain-function path never builds, so `$n = '[__Foo@bar_...]'; $n();` popped past the`` |
|         - |  373 | ` * bottom of the operand stack (SIGSEGV in the release build, an ASan heap-buffer-overflow` |
|         - |  374 | ` * READ in VmByteCodeExecBody).` |
|         - |  375 | ` *` |
|         - |  376 | ` * bEngineName is for the engine's OWN dispatch of those same entries, which is by name` |
|         - |  377 | `` * too: OP_MEMBER pushes a resolved method's `sVmName` onto the callee slot, the closure`` |
|         - |  378 | `` * machinery unwraps a Closure to its `[closure_N]`, and the two synthetic call builders`` |
|         - |  379 | ` * do both without an OP_MEMBER ahead of them. Each of those marks its own call site` |
|         - |  380 | ` * (MEMOBJ_AUX_MEMBERCALL / MEMOBJ_AUX_ENGINEFN / the OP_CALL local); nothing a program` |
|         - |  381 | ` * wrote ever passes 1.` |
|         - |  382 | ` */` |
|   5345638 |  383 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(` |
|         - |  384 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  385 | `	const void *pName,  /* Function name */` |
|         - |  386 | `	sxu32 nByte,        /* Name length */` |
|         - |  387 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|         - |  388 | `	)` |
|         5 |  389 | `{` |
|   5345643 |  390 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,pName,nByte);` |
|   5345643 |  391 | `	if( pEntry && !bEngineName ){` |
|    379384 |  392 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    379384 |  393 | `		if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|        25 |  394 | `			return 0;` |
|         - |  395 | `		}` |
|    190122 |  396 | `	}` |
|   5345619 |  397 | `	return pEntry;` |
|   2674918 |  398 | `}` |
|         - |  399 | `/*` |
|         - |  400 | ` * Namespace-aware function lookup.` |
|         - |  401 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  402 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  403 | ` */` |
|         - |  404 | `/*` |
|         - |  405 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  406 | ` */` |
|  16377186 |  407 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  408 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  409 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  410 | `	SyString *pName     /* Function name */` |
|         - |  411 | `	)` |
|         5 |  412 | `{` |
|         - |  413 | `	SyHashEntry *pEntry;` |
|         - |  414 | `	sxi32 rc;` |
|  16377191 |  415 | `	if( pName == 0 ){` |
|         - |  416 | `		/* Use the built-in name */` |
|    154915 |  417 | `		pName = &pFunc->sName;` |
|     77455 |  418 | `	}` |
|         - |  419 | `	/* Check for duplicates (functions with the same name) first */` |
|  16377191 |  420 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  16377191 |  421 | `	if( pEntry ){` |
|  12292189 |  422 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  12292189 |  423 | `		if( pLink != pFunc ){` |
|         - |  424 | `			/* Link */` |
|        91 |  425 | `			pFunc->pNextName = pLink;` |
|        91 |  426 | `			pEntry->pUserData = pFunc;` |
|        43 |  427 | `		}` |
|  12292189 |  428 | `		return SXRET_OK;` |
|         - |  429 | `	}` |
|         - |  430 | `	/* First time seen */` |
|   4085007 |  431 | `	pFunc->pNextName = 0;` |
|   4085007 |  432 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   4085007 |  433 | `	return rc;` |
|   8188598 |  434 | `}` |
|         - |  435 | `/*` |
|         - |  436 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  437 | ` */` |
|    739688 |  438 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  439 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  440 | `	ph7_class *pClass /* Target Class */` |
|         - |  441 | `	)` |
|         5 |  442 | `{` |
|    739693 |  443 | `	SyString *pName = &pClass->sName;` |
|         - |  444 | `	SyHashEntry *pEntry;` |
|         - |  445 | `	sxi32 rc;` |
|         - |  446 | `	/* Check for duplicates */` |
|    739693 |  447 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    739693 |  448 | `	if( pEntry ){` |
|         3 |  449 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  450 | `		/* Link entry with the same name */` |
|         3 |  451 | `		pClass->pNextName = pLink;` |
|         3 |  452 | `		pEntry->pUserData = pClass;` |
|         3 |  453 | `		return SXRET_OK;` |
|         - |  454 | `	}` |
|    739691 |  455 | `	pClass->pNextName = 0;` |
|         - |  456 | `	/* Perform a simple hashtable insertion */` |
|    739691 |  457 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    739691 |  458 | `	return rc;` |
|    369849 |  459 | `}` |
|         - |  460 | `/*` |
|         - |  461 | ` * Instruction builder interface.` |
|         - |  462 | ` */` |
|  11690114 |  463 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  464 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  465 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  466 | `	sxi32 iP1,    /* First operand */` |
|         - |  467 | `	sxu32 iP2,    /* Second operand */` |
|         - |  468 | `	void *p3,     /* Third operand */` |
|         - |  469 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  470 | `	)` |
|         5 |  471 | `{` |
|         - |  472 | `	VmInstr sInstr;` |
|  11690119 |  473 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  474 | `	sxi32 rc;` |
|         - |  475 | `	/* Fill the VM instruction */` |
|  11690119 |  476 | `	sInstr.iOp = (sxu8)iOp;` |
|  11690119 |  477 | `	sInstr.iP1 = iP1;` |
|  11690119 |  478 | `	sInstr.iP2 = iP2;` |
|  11690119 |  479 | `	sInstr.p3  = p3;` |
|         - |  480 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  481 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  482 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  483 | `	 * between statements, hence the range check. */` |
|  11690119 |  484 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|  11690119 |  485 | `	sInstr.nLine = 0;` |
|  11690119 |  486 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|   4217589 |  487 | `		sInstr.nLine = pGen->pIn->nLine;` |
|   9581327 |  488 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  489 | `		/* Past the end (statement tail): blame the last real token. */` |
|   7421917 |  490 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|   3710956 |  491 | `	}` |
|  11690119 |  492 | `	if( pIndex ){` |
|         - |  493 | `		/* Instruction index in the bytecode array */` |
|    951915 |  494 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    475955 |  495 | `	}` |
|         - |  496 | `	/* Finally,record the instruction */` |
|  11690119 |  497 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  11690119 |  498 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  499 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  500 | `		/* Fall throw */` |
|       ! 0 |  501 | `	}` |
|  11690119 |  502 | `	return rc;` |
|         5 |  503 | `}` |
|         - |  504 | `/*` |
|         - |  505 | ` * Swap the current bytecode container with the given one.` |
|         - |  506 | ` */` |
|    482636 |  507 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  508 | `{` |
|    482641 |  509 | `	if( pContainer == 0 ){` |
|         - |  510 | `		/* Point to the default container */` |
|       ! 0 |  511 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  512 | `	}else{` |
|         - |  513 | `		/* Change container */` |
|    482641 |  514 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  515 | `	}` |
|    482641 |  516 | `	return SXRET_OK;` |
|         5 |  517 | `}` |
|         - |  518 | `/*` |
|         - |  519 | ` * Return the current bytecode container.` |
|         - |  520 | ` */` |
|    953000 |  521 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  522 | `{` |
|    953005 |  523 | `	return pVm->pByteContainer;` |
|         5 |  524 | `}` |
|         - |  525 | `/*` |
|         - |  526 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  527 | ` */` |
|    308734 |  528 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  529 | `{` |
|         - |  530 | `	VmInstr *pInstr;` |
|    308739 |  531 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    308739 |  532 | `	return pInstr;` |
|         5 |  533 | `}` |
|         - |  534 | `/*` |
|         - |  535 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  536 | ` */` |
|   5867566 |  537 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  538 | `{` |
|   5867571 |  539 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  540 | `}` |
|         - |  541 | `/*` |
|         - |  542 | ` * Pop the last VM instruction.` |
|         - |  543 | ` */` |
|    846164 |  544 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  545 | `{` |
|    846169 |  546 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  547 | `}` |
|         - |  548 | `/*` |
|         - |  549 | ` * Peek the last VM instruction.` |
|         - |  550 | ` */` |
|   2241788 |  551 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  552 | `{` |
|   2241793 |  553 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  554 | `}` |
|     82732 |  555 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  556 | `{` |
|         - |  557 | `	VmInstr *aInstr;` |
|         - |  558 | `	sxu32 n;` |
|     82737 |  559 | `	n = SySetUsed(pVm->pByteContainer);` |
|     82737 |  560 | `	if( n < 2 ){` |
|       ! 0 |  561 | `		return 0;` |
|         - |  562 | `	}` |
|     82737 |  563 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     82737 |  564 | `	return &aInstr[n - 2];` |
|     41371 |  565 | `}` |
|         - |  566 | `/*` |
|         - |  567 | ` * Allocate a new virtual machine frame.` |
|         - |  568 | ` */` |
|   3663764 |  569 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  570 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  571 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  572 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  573 | `	)` |
|         5 |  574 | `{` |
|         - |  575 | `	VmFrame *pFrame;` |
|         - |  576 | `	/* Allocate a new vm frame */` |
|   3663769 |  577 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   3663769 |  578 | `	if( pFrame == 0 ){` |
|       ! 0 |  579 | `		return 0;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* Zero the structure */` |
|   3663769 |  582 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  583 | `	/* Initialize frame fields */` |
|   3663769 |  584 | `	pFrame->pUserData = pUserData;` |
|   3663769 |  585 | `	pFrame->pThis = pThis;` |
|   3663769 |  586 | `	pFrame->pVm = pVm;` |
|   3663769 |  587 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   3663769 |  588 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   3663769 |  589 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   3663769 |  590 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   3663769 |  591 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  592 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  593 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   3663769 |  594 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   3663769 |  595 | `	return pFrame;` |
|   1832109 |  596 | `}` |
|         - |  597 | `/* Forward declaration */` |
|         - |  598 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  599 | `/*` |
|         - |  600 | ` * Enter a VM frame.` |
|         - |  601 | ` */` |
|   3663070 |  602 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  603 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  604 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  605 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  606 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  607 | `	)` |
|         5 |  608 | `{` |
|         - |  609 | `	VmFrame *pFrame;` |
|         - |  610 | `	/* Allocate a new frame */` |
|   3663075 |  611 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   3663075 |  612 | `	if( pFrame == 0 ){` |
|       ! 0 |  613 | `		return SXERR_MEM;` |
|         - |  614 | `	}` |
|         - |  615 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   3663075 |  616 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  617 | `	/* Link to the list of active VM frame */` |
|   3663075 |  618 | `	pFrame->pParent = pVm->pFrame;` |
|   3663075 |  619 | `	pVm->pFrame = pFrame;` |
|   3663075 |  620 | `	if( ppFrame ){` |
|         - |  621 | `		/* Write a pointer to the new VM frame */` |
|   3657913 |  622 | `		*ppFrame = pFrame;` |
|   1829176 |  623 | `	}` |
|   3663075 |  624 | `	return SXRET_OK;` |
|   1831762 |  625 | `}` |
|         - |  626 | `/*` |
|         - |  627 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  628 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  629 | ` * information.` |
|         - |  630 | ` */` |
|       230 |  631 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  632 | `{` |
|         - |  633 | `	VmFrame *pTarget,*pGlobal;` |
|         - |  634 | `	SyHashEntry *pEntry;` |
|         - |  635 | `	sxi32 rc;` |
|       235 |  636 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|         - |  637 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|         - |  638 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|         - |  639 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|         - |  640 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|         - |  641 | `	 * writes never reached the real global. */` |
|       235 |  642 | `	pGlobal = pTarget;` |
|       501 |  643 | `	while( pGlobal->pParent ){` |
|       271 |  644 | `		pGlobal = pGlobal->pParent;` |
|         5 |  645 | `	}` |
|       235 |  646 | `	if( pGlobal == pTarget ){` |
|         - |  647 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|       ! 0 |  648 | `		return SXRET_OK;` |
|         - |  649 | `	}` |
|         - |  650 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|         - |  651 | `	 * plain global that would shadow it. */` |
|       235 |  652 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|       235 |  653 | `	if( pEntry == 0 ){` |
|       233 |  654 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|       114 |  655 | `	}` |
|       235 |  656 | `	if( pEntry == 0 ){` |
|         - |  657 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|         - |  658 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|         - |  659 | `		 * the assignment went to a local nobody could read. */` |
|        12 |  660 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|        12 |  661 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  662 | `			return rc;` |
|         - |  663 | `		}` |
|        12 |  664 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        12 |  665 | `		if( pEntry == 0 ){` |
|       ! 0 |  666 | `			return SXERR_NOTFOUND;` |
|         - |  667 | `		}` |
|         5 |  668 | `	}` |
|         - |  669 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|         - |  670 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|       350 |  671 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|       230 |  672 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|       235 |  673 | `	return SXRET_OK;` |
|       120 |  674 | `}` |
|         - |  675 | `/*` |
|         - |  676 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  677 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  678 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  679 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  680 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  681 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  682 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  683 | ` */` |
|   3658408 |  684 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  685 | `{` |
|   3658413 |  686 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 |  687 | `		pVm->pResumeFrame = 0;` |
|       ! 0 |  688 | `	}` |
|   3658413 |  689 | `}` |
|         - |  690 | `/*` |
|         - |  691 | ` * Leave the top-most active frame.` |
|         - |  692 | ` */` |
|   3657698 |  693 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  694 | `{` |
|   3657703 |  695 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   3657703 |  696 | `	if( pCurFrame ){` |
|         - |  697 | `		/* Unlink from the list of active VM frame */` |
|   3657703 |  698 | `		pVm->pFrame = pCurFrame->pParent;` |
|   3657703 |  699 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  700 | `			VmSlot  *aSlot;` |
|         - |  701 | `			sxu32 n;` |
|         - |  702 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|         - |  703 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|         - |  704 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|         - |  705 | `			 * is about to free). */` |
|    763319 |  706 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   1900863 |  707 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   1137549 |  708 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    570109 |  709 | `			}` |
|         - |  710 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    763319 |  711 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   1900305 |  712 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|   1136991 |  713 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|         - |  714 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|         - |  715 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|         - |  716 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|         - |  717 | `					 * down the slot and the reference table took the holders with it, so` |
|         - |  718 | `					 * the returned array came back one element SHORT. The last holder to` |
|         - |  719 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|        24 |  720 | `					continue;` |
|         - |  721 | `				}` |
|         - |  722 | `				/* Unset the local variable */` |
|   1136969 |  723 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    569819 |  724 | `			}` |
|    381879 |  725 | `		}` |
|         - |  726 | `		/* Release internal containers */` |
|   3657703 |  727 | `		SyHashRelease(&pCurFrame->hVar);` |
|   3657703 |  728 | `		SySetRelease(&pCurFrame->sArg);` |
|   3657703 |  729 | `		SySetRelease(&pCurFrame->sLocal);` |
|   3657703 |  730 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  731 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  732 | `		 * containers above — released for every frame, including transparent` |
|         - |  733 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   3657703 |  734 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  735 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   3657703 |  736 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  737 | `		/* Release the whole structure */` |
|   3657703 |  738 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   1829071 |  739 | `	}` |
|   3657703 |  740 | `}` |
|         - |  741 | `/*` |
|         - |  742 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  743 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  744 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  745 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  746 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  747 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  748 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  749 | ` */` |
|         - |  750 | `/*` |
|         - |  751 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  752 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  753 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  754 | ` *` |
|         - |  755 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  756 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  757 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  758 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  759 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  760 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  761 | ` */` |
|   7686922 |  762 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  763 | `{` |
|         - |  764 | `	VmFrame *pFrame;` |
|  20827271 |  765 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|  13140615 |  766 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  767 | `		sxu32 n;` |
|  27871427 |  768 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|  14731083 |  769 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  770 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       269 |  771 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       269 |  772 | `				(void)SySetPop(&pFrame->sLocal);` |
|       269 |  773 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  774 | `			}` |
|   7365411 |  775 | `		}` |
|   6570163 |  776 | `	}` |
|   7686661 |  777 | `	return FALSE;` |
|   3843452 |  778 | `}` |
|       540 |  779 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  780 | `{` |
|         - |  781 | `	VmRefObj *pRef;` |
|       545 |  782 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       545 |  783 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       545 |  784 | `	if( pRef ){` |
|       521 |  785 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       263 |  786 | `	}else{` |
|         - |  787 | `		/* No record yet — a pin on a slot nothing refers to was silently a NO-OP, so the` |
|         - |  788 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|        27 |  789 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  790 | `	}` |
|       545 |  791 | `}` |
|         - |  792 | `/*` |
|         - |  793 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|         - |  794 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|         - |  795 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|         - |  796 | ` */` |
|        42 |  797 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  798 | `{` |
|         - |  799 | `	VmRefObj *pRef;` |
|        43 |  800 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|        43 |  801 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        43 |  802 | `	if( pRef ){` |
|        43 |  803 | `		pRef->nPin++;` |
|        21 |  804 | `	}` |
|        43 |  805 | `}` |
|         - |  806 | `/*` |
|         - |  807 | ` * Give back a counted pin. The slot goes when it was the last holder — without this the` |
|         - |  808 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|         - |  809 | ` * used to be a flag, so nothing could tell one holder from two).` |
|         - |  810 | ` */` |
|        36 |  811 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  812 | `{` |
|        37 |  813 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        37 |  814 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|       ! 0 |  815 | `		return;` |
|         - |  816 | `	}` |
|        37 |  817 | `	pRef->nPin--;` |
|        37 |  818 | `	if( pRef->nPin < 1 ){` |
|        31 |  819 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|        31 |  820 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        15 |  821 | `	}` |
|        19 |  822 | `}` |
|         - |  823 | `/*` |
|         - |  824 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  825 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  826 | ` * should be skipped when looking for the real execution context.` |
|         - |  827 | ` */` |
|  42656265 |  828 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  829 | `{` |
|  56104072 |  830 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  13447807 |  831 | `		pFrame = pFrame->pParent;` |
|         5 |  832 | `	}` |
|  42656270 |  833 | `	return pFrame;` |
|         5 |  834 | `}` |
|         - |  835 | `/*` |
|         - |  836 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  837 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  838 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  839 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  840 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  841 | ` *` |
|         - |  842 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  843 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  844 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  845 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  846 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  847 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  848 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  849 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  850 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  851 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  852 | ` *` |
|         - |  853 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  854 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  855 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  856 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  857 | ` */` |
|   1959076 |  858 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  859 | `{` |
|   1959081 |  860 | `	if( pVm->pResumeFrame == 0 ){` |
|        23 |  861 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  862 | `	}` |
|   1959061 |  863 | `	if( pEntryFrame == 0 ){` |
|         - |  864 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - |  865 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - |  866 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - |  867 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - |  868 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - |  869 | `		 * NULL and the process dies. */` |
|        12 |  870 | `		return FALSE;` |
|         - |  871 | `	}` |
|         - |  872 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  873 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  874 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  875 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  876 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  877 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  878 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  879 | `	 * exec's resume site match and land. */` |
|   1959046 |  880 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1657277 |  881 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1355318 |  882 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  883 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  884 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  885 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  886 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  887 | `		 * never match a real frame. */` |
|    603933 |  888 | `		return FALSE;` |
|         - |  889 | `	}` |
|         - |  890 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  891 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  892 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  893 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  894 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  895 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  896 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  897 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  898 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  899 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  900 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  901 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  902 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  903 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  904 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  905 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  906 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  907 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  908 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  909 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  910 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|   2186409 |  911 | `	while( pVm->pFrame != pEntryFrame` |
|   2390218 |  912 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1558921 |  913 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|    307469 |  914 | `		VmLeaveFrame(&(*pVm));` |
|         5 |  915 | `	}` |
|   1355123 |  916 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|   1355123 |  917 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|         - |  918 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  919 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  920 | `	 * point router must not re-fire it after this resume. */` |
|   1355123 |  921 | `	pVm->nBoundaryRc = 0;` |
|   1355123 |  922 | `	return TRUE;` |
|    979543 |  923 | `}` |
|         - |  924 | `/*` |
|         - |  925 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  926 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  927 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  928 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  929 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  930 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  931 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  932 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  933 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  934 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  935 | ` */` |
|         - |  936 | `/*` |
|         - |  937 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  938 | ` *` |
|         - |  939 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  940 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  941 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  942 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  943 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  944 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  945 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  946 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  947 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  948 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  949 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  950 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  951 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  952 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  953 | ` * activation.` |
|         - |  954 | ` */` |
|   1459096 |  955 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  956 | `{` |
|   1459101 |  957 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1459101 |  958 | `	if( pClone == 0 ){` |
|       ! 0 |  959 | `		return 0;` |
|         - |  960 | `	}` |
|   1459101 |  961 | `	*pClone = *pCompiled;` |
|   1459101 |  962 | `	pClone->pCompiled = pCompiled;` |
|   1459101 |  963 | `	pClone->iFinallyDone = 0;` |
|   1459101 |  964 | `	pClone->iInCatch = 0;` |
|   1459101 |  965 | `	pClone->pInflight = 0;` |
|   1459101 |  966 | `	pClone->pFrame = 0;` |
|   1459101 |  967 | `	return pClone;` |
|    729553 |  968 | `}` |
|   2914956 |  969 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  970 | `{` |
|   2914961 |  971 | `	if( pExc && pExc->pCompiled ){` |
|         - |  972 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  973 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - |  974 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - |  975 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1459083 |  976 | `		if( pExc->pInflight ){` |
|       ! 0 |  977 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 |  978 | `			pExc->pInflight = 0;` |
|       ! 0 |  979 | `		}` |
|   1459083 |  980 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    729539 |  981 | `	}` |
|   2914961 |  982 | `}` |
|         - |  983 | `/*` |
|         - |  984 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - |  985 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - |  986 | ` */` |
|      3762 |  987 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 |  988 | `{` |
|      3767 |  989 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 |  990 | `}` |
|         - |  991 | `/*` |
|         - |  992 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - |  993 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - |  994 | ` * handlers). The set itself is reset by the caller.` |
|         - |  995 | ` */` |
|    100986 |  996 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 |  997 | `{` |
|    100991 |  998 | `	sxu32 n = SySetUsed(pSet);` |
|    100991 |  999 | `	if( n > 0 ){` |
|       ! 0 | 1000 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - | 1001 | `		sxu32 i;` |
|       ! 0 | 1002 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 | 1003 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 | 1004 | `		}` |
|       ! 0 | 1005 | `	}` |
|    100991 | 1006 | `}` |
|         - | 1007 | `/*` |
|         - | 1008 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - | 1009 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - | 1010 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - | 1011 | ` * down a try belonging to the caller.` |
|         - | 1012 | ` */` |
|        18 | 1013 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         3 | 1014 | `{` |
|        21 | 1015 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        21 | 1016 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        21 | 1017 | `	if( nBase < nFloor ){` |
|       ! 0 | 1018 | `		nBase = nFloor;` |
|       ! 0 | 1019 | `	}` |
|        21 | 1020 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         3 | 1021 | `}` |
|         - | 1022 | `/*` |
|         - | 1023 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - | 1024 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - | 1025 | ` * only carries the compiled pointer.` |
|         - | 1026 | ` */` |
|        76 | 1027 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 | 1028 | `{` |
|        81 | 1029 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        81 | 1030 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        81 | 1031 | `	while( n > 0 ){` |
|        81 | 1032 | `		n--;` |
|        81 | 1033 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        81 | 1034 | `			return ap[n];` |
|         - | 1035 | `		}` |
|       ! 0 | 1036 | `	}` |
|       ! 0 | 1037 | `	return 0;` |
|        43 | 1038 | `}` |
|   3136111 | 1039 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 | 1040 | `{` |
|         - | 1041 | `	sxu32 nUsed;` |
|   3136116 | 1042 | `	sxi32 rcOut = SXRET_OK;` |
|   3136196 | 1043 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|        85 | 1044 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        85 | 1045 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|        85 | 1046 | `		(void)SySetPop(&pVm->aException);` |
|        85 | 1047 | `		pExc->pFrame = 0;` |
|         - | 1048 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - | 1049 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - | 1050 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - | 1051 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - | 1052 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|        85 | 1053 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        85 | 1054 | `			VmLeaveFrame(&(*pVm));` |
|        40 | 1055 | `		}` |
|       117 | 1056 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - | 1057 | `			sxi32 rcF;` |
|        69 | 1058 | `			pExc->iFinallyDone = 1;` |
|        69 | 1059 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        69 | 1060 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        69 | 1061 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 | 1062 | `				return SXERR_ABORT;` |
|         - | 1063 | `			}` |
|        69 | 1064 | `			if( rcF == PH7_EXCEPTION ){` |
|         - | 1065 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - | 1066 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - | 1067 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 | 1068 | `				rcOut = PH7_EXCEPTION;` |
|         2 | 1069 | `			}` |
|        37 | 1070 | `		}else{` |
|        20 | 1071 | `			VmExcRelease(&(*pVm),pExc);` |
|         - | 1072 | `		}` |
|         5 | 1073 | `	}` |
|   3136116 | 1074 | `	return rcOut;` |
|   1568280 | 1075 | `}` |
|         - | 1076 | `/*` |
|         - | 1077 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - | 1078 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - | 1079 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - | 1080 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - | 1081 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - | 1082 | ` */` |
|   2551717 | 1083 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 | 1084 | `{` |
|   2551722 | 1085 | `	pFrame->bHasRet = 0;` |
|   2551722 | 1086 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   2551722 | 1087 | `	pFrame->nCatchJmpPc = 0;` |
|   2551722 | 1088 | `}` |
|         - | 1089 | `/*` |
|         - | 1090 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - | 1091 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - | 1092 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - | 1093 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - | 1094 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - | 1095 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - | 1096 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - | 1097 | ` */` |
|     20256 | 1098 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 | 1099 | `{` |
|     20261 | 1100 | `	if( pResult ){` |
|     20261 | 1101 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     10128 | 1102 | `	}` |
|     20261 | 1103 | `	VmClearFramePending(pEntryFrame);` |
|     20265 | 1104 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|         5 | 1105 | `		VmLeaveFrame(&(*pVm));` |
|         1 | 1106 | `	}` |
|     20261 | 1107 | `}` |
|         - | 1108 | `/*` |
|         - | 1109 | ` * Compare two functions signature and return the comparison result.` |
|         - | 1110 | ` */` |
|      1186 | 1111 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         2 | 1112 | `{` |
|      1188 | 1113 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      1188 | 1114 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      1188 | 1115 | `	const char *zSin = pSecond->zString;` |
|      1188 | 1116 | `	const char *zFin = pFirst->zString;` |
|      1188 | 1117 | `	const char *zPtr = zFin;` |
|       593 | 1118 | `	for(;;){` |
|      1188 | 1119 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|       595 | 1120 | `			break;` |
|         - | 1121 | `		}` |
|       ! 0 | 1122 | `		if( zFin[0] != zSin[0] ){` |
|         - | 1123 | `			/* mismatch */` |
|       ! 0 | 1124 | `			break;` |
|         - | 1125 | `		}` |
|       ! 0 | 1126 | `		zFin++;` |
|       ! 0 | 1127 | `		zSin++;` |
|       ! 0 | 1128 | `	}` |
|      1188 | 1129 | `	return (int)(zFin-zPtr);` |
|         2 | 1130 | `}` |
|         - | 1131 | `/*` |
|         - | 1132 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1133 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1134 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1135 | ` * Refer to the official documentation for more information.` |
|         - | 1136 | ` */` |
|       284 | 1137 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1138 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1139 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1140 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1141 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1142 | `	)` |
|         5 | 1143 | `{` |
|         - | 1144 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1145 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1146 | `	ph7_vm_func *pLink;` |
|         - | 1147 | `	SyString sArgSig;` |
|         - | 1148 | `	SyBlob sSig;` |
|         - | 1149 |  |
|       289 | 1150 | `	pLink = pList;` |
|       289 | 1151 | `	i = 0;` |
|         - | 1152 | `	/* Put functions expecting the same number of passed arguments */` |
|      1891 | 1153 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      1823 | 1154 | `		if( pLink == 0 ){` |
|       220 | 1155 | `			break;` |
|         - | 1156 | `		}` |
|      1607 | 1157 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1158 | `			/* Candidate for overloading */` |
|      1607 | 1159 | `			apSet[i++] = pLink;` |
|       801 | 1160 | `		}` |
|         - | 1161 | `		/* Point to the next entry */` |
|      1607 | 1162 | `		pLink = pLink->pNextName;` |
|         5 | 1163 | `	}` |
|       289 | 1164 | `	if( i < 1 ){` |
|         - | 1165 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1166 | `		return pList;` |
|         - | 1167 | `	}` |
|       289 | 1168 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1169 | `		/* Return the only candidate */` |
|        61 | 1170 | `		return apSet[0];` |
|         - | 1171 | `	}` |
|         - | 1172 | `	/* Calculate function signature */` |
|       230 | 1173 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|       458 | 1174 | `	for( j = 0 ; j < nArg ; j++ ){` |
|       230 | 1175 | `		int c = 'n'; /* null */` |
|       230 | 1176 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1177 | `			/* Hashmap */` |
|       ! 0 | 1178 | `			c = 'h';` |
|       230 | 1179 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1180 | `			/* bool */` |
|        85 | 1181 | `			c = 'b';` |
|       188 | 1182 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1183 | `			/* int */` |
|        48 | 1184 | `			c = 'i';` |
|       122 | 1185 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1186 | `			/* String */` |
|        87 | 1187 | `			c = 's';` |
|        56 | 1188 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1189 | `			/* Float */` |
|        11 | 1190 | `			c = 'f';` |
|         8 | 1191 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1192 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1193 | `			int marker = 'o';` |
|       ! 0 | 1194 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1195 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1196 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1197 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1198 | `			c = -1;` |
|       ! 0 | 1199 | `		}` |
|       230 | 1200 | `		if( c > 0 ){` |
|       230 | 1201 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       114 | 1202 | `		}` |
|       116 | 1203 | `	}` |
|       230 | 1204 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|       230 | 1205 | `	iTarget = 0;` |
|       230 | 1206 | `	iMax = -1;` |
|         - | 1207 | `	/* Select the appropriate function */` |
|      1416 | 1208 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1209 | `		/* Compare the two signatures */` |
|      1188 | 1210 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      1188 | 1211 | `		if( iCur > iMax ){` |
|       230 | 1212 | `			iMax = iCur;` |
|       230 | 1213 | `			iTarget = j;` |
|       114 | 1214 | `		}` |
|       595 | 1215 | `	}` |
|       230 | 1216 | `	SyBlobRelease(&sSig);` |
|         - | 1217 | `	/* Appropriate function for the current call context */` |
|       230 | 1218 | `	return apSet[iTarget];` |
|       147 | 1219 | `}` |
|         - | 1220 | `/* Forward declaration */` |
|         - | 1221 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1222 | `/*` |
|         - | 1223 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|         - | 1224 | ` * safely across a pool reallocation.` |
|         - | 1225 | ` *` |
|         - | 1226 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1227 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|         - | 1228 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|         - | 1229 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|         - | 1230 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|         - | 1231 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|         - | 1232 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|         - | 1233 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|         - | 1234 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|         - | 1235 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|         - | 1236 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|         - | 1237 | ` */` |
|      2868 | 1238 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1239 | `{` |
|         - | 1240 | `	ph7_value sVal;` |
|      2873 | 1241 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1242 | `	sxi32 rc;` |
|      2873 | 1243 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|      2873 | 1244 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1245 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|      2873 | 1246 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      2873 | 1247 | `	if( *ppMemObj ){` |
|      2873 | 1248 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|      1434 | 1249 | `	}` |
|      2873 | 1250 | `	PH7_MemObjRelease(&sVal);` |
|      2873 | 1251 | `	return rc;` |
|         5 | 1252 | `}` |
|         - | 1253 | `/*` |
|         - | 1254 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|         - | 1255 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|         - | 1256 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|         - | 1257 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|         - | 1258 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|         - | 1259 | ` * C caller ever regains control.` |
|         - | 1260 | ` */` |
|         - | 1261 | `typedef struct VmMuteState {` |
|         - | 1262 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|         - | 1263 | `	sxu32 nSaved;` |
|         - | 1264 | `	sxi32 iSaveStatus;` |
|         - | 1265 | `	sxi32 iSaveBoundary;` |
|         - | 1266 | `	VmFrame *pSaveResume;` |
|         - | 1267 | `	ph7_class_attr *pSaveCycleAttr;` |
|         - | 1268 | `	ph7_class *pSaveCycleClass;` |
|         - | 1269 | `} VmMuteState;` |
|       280 | 1270 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|         5 | 1271 | `{` |
|       285 | 1272 | `	pSave->apSaved = 0;` |
|       285 | 1273 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|       285 | 1274 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|       285 | 1275 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|       285 | 1276 | `	pSave->pSaveResume = pVm->pResumeFrame;` |
|       285 | 1277 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|       285 | 1278 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|       285 | 1279 | `	if( pSave->nSaved > 0 ){` |
|        34 | 1280 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        16 | 1281 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|        18 | 1282 | `		if( pSave->apSaved ){` |
|        26 | 1283 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|        16 | 1284 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|        18 | 1285 | `			SySetReset(&pVm->aException);` |
|         8 | 1286 | `		}` |
|         8 | 1287 | `	}` |
|       285 | 1288 | `	pVm->nMuteThrow++;` |
|       285 | 1289 | `}` |
|         - | 1290 | `/*` |
|         - | 1291 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|         - | 1292 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|         - | 1293 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|         - | 1294 | ` * TRUE when a throw was actually swallowed.` |
|         - | 1295 | ` */` |
|       280 | 1296 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|         5 | 1297 | `{` |
|         - | 1298 | `	VmFrame *pFrame;` |
|       285 | 1299 | `	pVm->nMuteThrow--;` |
|         - | 1300 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|         - | 1301 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|         - | 1302 | `	 * activation behind: release whatever is there whether or not anything was` |
|         - | 1303 | `	 * hidden. */` |
|       285 | 1304 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|       285 | 1305 | `	SySetReset(&pVm->aException);` |
|       285 | 1306 | `	if( pSave->apSaved ){` |
|         - | 1307 | `		sxu32 k;` |
|        34 | 1308 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|        18 | 1309 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|        10 | 1310 | `		}` |
|        18 | 1311 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|        18 | 1312 | `		pSave->apSaved = 0;` |
|         8 | 1313 | `	}` |
|       285 | 1314 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|       239 | 1315 | `		return FALSE;` |
|         - | 1316 | `	}` |
|        50 | 1317 | `	pFrame = pVm->pFrame;` |
|        50 | 1318 | `	if( pFrame ){` |
|        50 | 1319 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        50 | 1320 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        23 | 1321 | `	}` |
|        50 | 1322 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|        50 | 1323 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|        50 | 1324 | `	pVm->pResumeFrame = pSave->pSaveResume;` |
|        50 | 1325 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|        50 | 1326 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|        50 | 1327 | `	return TRUE;` |
|       145 | 1328 | `}` |
|         - | 1329 | `/*` |
|         - | 1330 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|         - | 1331 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|         - | 1332 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|         - | 1333 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|         - | 1334 | ` *` |
|         - | 1335 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|         - | 1336 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|         - | 1337 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|         - | 1338 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|         - | 1339 | ` */` |
|        12 | 1340 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|         - | 1341 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 1342 | `	ph7_class_instance *pThis,   /* Receiver */` |
|         - | 1343 | `	ph7_class_method *pMethod,   /* Method to run */` |
|         - | 1344 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|         - | 1345 | `	int nArg,                    /* Argument count */` |
|         - | 1346 | `	ph7_value **apArg,           /* Arguments */` |
|         - | 1347 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|         - | 1348 | `	)` |
|         1 | 1349 | `{` |
|         - | 1350 | `	VmMuteState sSave;` |
|         - | 1351 | `	sxi32 rc;` |
|        13 | 1352 | `	VmMuteEnter(&(*pVm),&sSave);` |
|        13 | 1353 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|        13 | 1354 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         3 | 1355 | `		if( pbThrew ){` |
|         3 | 1356 | `			*pbThrew = TRUE;` |
|         1 | 1357 | `		}` |
|         3 | 1358 | `		return SXRET_OK;` |
|         - | 1359 | `	}` |
|        11 | 1360 | `	if( pbThrew ){` |
|        11 | 1361 | `		*pbThrew = FALSE;` |
|         5 | 1362 | `	}` |
|        11 | 1363 | `	return rc;` |
|         7 | 1364 | `}` |
|         - | 1365 | `/*` |
|         - | 1366 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - | 1367 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - | 1368 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - | 1369 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - | 1370 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - | 1371 | ` *` |
|         - | 1372 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - | 1373 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - | 1374 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - | 1375 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - | 1376 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - | 1377 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - | 1378 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - | 1379 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - | 1380 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - | 1381 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 1382 | ` */` |
|       268 | 1383 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 | 1384 | `{` |
|         - | 1385 | `	VmMuteState sSave;` |
|         - | 1386 | `	sxi32 rc;` |
|       273 | 1387 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       273 | 1388 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|       273 | 1389 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|         - | 1390 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - | 1391 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - | 1392 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - | 1393 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - | 1394 | `		 * records the cycle again and raises it there. */` |
|       ! 0 | 1395 | `		rc = PH7_EXCEPTION;` |
|       ! 0 | 1396 | `	}` |
|         - | 1397 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|         - | 1398 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|         - | 1399 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|         - | 1400 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|         - | 1401 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|         - | 1402 | `	 * detects the cycle again. */` |
|       273 | 1403 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|       273 | 1404 | `	return rc;` |
|         5 | 1405 | `}` |
|         - | 1406 | `/*` |
|         - | 1407 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1408 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1409 | ` */` |
|         - | 1410 | `/*` |
|         - | 1411 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1412 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1413 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1414 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1415 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1416 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1417 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1418 | ` */` |
|   1743212 | 1419 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1420 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1421 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1422 | `	)` |
|         5 | 1423 | `{` |
|         - | 1424 | `	ph7_class_attr *pAttr;` |
|         - | 1425 | `	SyHashEntry *pEntry;` |
|         - | 1426 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1427 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1428 | `	 * over both tables. */` |
|         - | 1429 | `	SyHash *apMount[2];` |
|         - | 1430 | `	int iMount;` |
|   1743217 | 1431 | `	apMount[0] = &pClass->hAttr;` |
|   1743217 | 1432 | `	apMount[1] = &pClass->hConst;` |
|   5229635 | 1433 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1434 | `	/* Reset the loop cursor */` |
|   3486429 | 1435 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1436 | `	/* Process only static and constant attribute */` |
|  10385817 | 1437 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1438 | `		/* Extract the current attribute */` |
|   6899399 | 1439 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   6899394 | 1440 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   4308605 | 1441 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    859436 | 1442 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1443 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1444 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1445 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1446 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1447 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1448 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1449 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1450 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1451 | `			 * %s as value for class constant" fatal without any access). */` |
|   1716745 | 1452 | `			continue;` |
|         - | 1453 | `		}` |
|   5182659 | 1454 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1455 | `			ph7_value *pMemObj;` |
|      1343 | 1456 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1457 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1458 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1459 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1460 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - | 1461 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - | 1462 | `				 * access / instantiation throws like php's. */` |
|      1048 | 1463 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        26 | 1464 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - | 1465 | `						/* Its default threw at the other class's mount and is` |
|         - | 1466 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - | 1467 | `						 * both static tables). */` |
|         3 | 1468 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1469 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        11 | 1470 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|         6 | 1471 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|         8 | 1472 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1473 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|         1 | 1474 | `						}` |
|         3 | 1475 | `					}` |
|        11 | 1476 | `				}` |
|      1051 | 1477 | `				continue;` |
|         - | 1478 | `			}` |
|         - | 1479 | `			/* Reserve a memory object for this constant/static attribute */` |
|       299 | 1480 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       299 | 1481 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1482 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 1483 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 1484 | `					&pClass->sName,&pAttr->sName` |
|         - | 1485 | `					);` |
|       ! 0 | 1486 | `				return SXERR_MEM;` |
|         - | 1487 | `			}` |
|       299 | 1488 | `			if( pAttr->pNativeValue ){` |
|         - | 1489 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 | 1490 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|       299 | 1491 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1492 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1493 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 1494 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|       273 | 1495 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       273 | 1496 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - | 1497 | `				sxi32 rcExec;` |
|       273 | 1498 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       273 | 1499 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|       273 | 1500 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|       273 | 1501 | `				pVm->nConstEvalDepth++;` |
|         - | 1502 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - | 1503 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - | 1504 | `				 * not something it can see. What reaches this line is a static` |
|         - | 1505 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - | 1506 | `				 * which php does validate here, but only when the initializer` |
|         - | 1507 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - | 1508 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - | 1509 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|       273 | 1510 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|       273 | 1511 | `				pVm->nConstEvalDepth--;` |
|       273 | 1512 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       273 | 1513 | `				pVm->pConstEvalClass = pSaveCtx;` |
|       273 | 1514 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1515 | `					/* php has not reached this initializer: defer it whole to the` |
|         - | 1516 | `					 * first USE, where the throw is raised at the access site and is` |
|         - | 1517 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - | 1518 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - | 1519 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - | 1520 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - | 1521 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - | 1522 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - | 1523 | `					 * unset nIdx. */` |
|        47 | 1524 | `					if( bStaticProp ){` |
|        41 | 1525 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        41 | 1526 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1527 | `					}else{` |
|         - | 1528 | `						VmSlot sSlot;` |
|         - | 1529 | `						/* Release before recycling: PH7_ReserveMemObj re-inits a` |
|         - | 1530 | `						 * reused slot without releasing it, and a muted eval that` |
|         - | 1531 | `						 * only recorded a CYCLE still left its value here. */` |
|         8 | 1532 | `						sSlot.nIdx = pMemObj->nIdx;` |
|         8 | 1533 | `						sSlot.pUserData = 0;` |
|         8 | 1534 | `						PH7_MemObjRelease(pMemObj);` |
|         8 | 1535 | `						SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|         8 | 1536 | `						continue;` |
|         - | 1537 | `					}` |
|       246 | 1538 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|       117 | 1539 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1540 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 1541 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - | 1542 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        42 | 1543 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        42 | 1544 | `					if( rcType != SXRET_OK ){` |
|         9 | 1545 | `						return rcType;` |
|         - | 1546 | `					}` |
|        16 | 1547 | `				}` |
|       128 | 1548 | `			}` |
|         - | 1549 | `			/* Record attribute index */` |
|       287 | 1550 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 1551 | `			/* Install static attribute in the reference table */` |
|       287 | 1552 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1553 | `			/* If this is a typed static property, register the slot so the` |
|         - | 1554 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 1555 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 1556 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 1557 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 1558 | `			 * already enforced above, so they need no store-time slot. */` |
|       282 | 1559 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       201 | 1560 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        83 | 1561 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        83 | 1562 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 1563 | `					return SXERR_MEM;` |
|         - | 1564 | `				}` |
|        83 | 1565 | `				pVmAttrS->pAttr = pAttr;` |
|        83 | 1566 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        83 | 1567 | `				pVmAttrS->iState = 0;` |
|        83 | 1568 | `				pVmAttrS->pOwner = pClass;` |
|         - | 1569 | `				/* Static typed property with no default starts uninitialized` |
|         - | 1570 | `				 * (constants are already excluded by the enclosing condition). */` |
|        83 | 1571 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        18 | 1572 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        76 | 1573 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - | 1574 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 1575 | `					 * typed static default LAZILY at the first static-property` |
|         - | 1576 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 1577 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 1578 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 1579 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 1580 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 1581 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - | 1582 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - | 1583 | `					 * the materializer checks it after the re-run. */` |
|        69 | 1584 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        24 | 1585 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        24 | 1586 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        11 | 1587 | `					}` |
|        32 | 1588 | `				}` |
|        83 | 1589 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 1590 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 1591 | `					return SXERR_MEM;` |
|         - | 1592 | `				}` |
|        39 | 1593 | `			}` |
|       141 | 1594 | `		}` |
|         5 | 1595 | `	}` |
|   1743214 | 1596 | `	} /* for iMount */` |
|   1743211 | 1597 | `	return SXRET_OK;` |
|    871611 | 1598 | `}` |
|         - | 1599 | `/*` |
|         - | 1600 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - | 1601 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - | 1602 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - | 1603 | ` */` |
|   1741492 | 1604 | `static sxi32 VmMountUserClassMethods(` |
|         - | 1605 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1606 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - | 1607 | `	)` |
|         5 | 1608 | `{` |
|         - | 1609 | `	ph7_class_method *pMeth;` |
|         - | 1610 | `	SyHashEntry *pEntry;` |
|         - | 1611 | `	sxi32 rc;` |
|         - | 1612 | `	/* Install class methods */` |
|   1741497 | 1613 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 1614 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 1615 | `		 */` |
|    555079 | 1616 | `		return SXRET_OK;` |
|         - | 1617 | `	}` |
|         - | 1618 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 1619 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 1620 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 1621 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 1622 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 1623 | `	/* Install the methods now */` |
|   1186423 | 1624 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  18040746 | 1625 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  16261119 | 1626 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  16261119 | 1627 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  16222281 | 1628 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  16222281 | 1629 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1630 | `				return rc;` |
|         - | 1631 | `			}` |
|   8111138 | 1632 | `		}` |
|         5 | 1633 | `	}` |
|         - | 1634 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   1186423 | 1635 | `	pClass->bMounted = TRUE;` |
|   1186423 | 1636 | `	return SXRET_OK;` |
|    870751 | 1637 | `}` |
|   1089204 | 1638 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 1639 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1640 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 1641 | `	)` |
|         5 | 1642 | `{` |
|         - | 1643 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - | 1644 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - | 1645 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - | 1646 | `	 * initializer that throws finds the exception classes ready. */` |
|   1089209 | 1647 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   1089209 | 1648 | `	if( rc != SXRET_OK ){` |
|         3 | 1649 | `		return rc;` |
|         - | 1650 | `	}` |
|   1089207 | 1651 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|    544607 | 1652 | `}` |
|         - | 1653 | `/*` |
|         - | 1654 | ` * Allocate a private frame for attributes of the given` |
|         - | 1655 | ` * class instance (Object in the PHP jargon).` |
|         - | 1656 | ` */` |
|   1577106 | 1657 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 1658 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 1659 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 1660 | `	)` |
|         5 | 1661 | `{` |
|   1577111 | 1662 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 1663 | `	ph7_class_attr *pAttr;` |
|         - | 1664 | `	SyHashEntry *pEntry;` |
|         - | 1665 | `	sxi32 rc;` |
|   1577111 | 1666 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 1667 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 1668 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 1669 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1577111 | 1670 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  11821537 | 1671 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 1672 | `		VmClassAttr *pVmAttr;` |
|         - | 1673 | `		/* Extract the current attribute */` |
|  10244431 | 1674 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  10244431 | 1675 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|  10244431 | 1676 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 1677 | `			return SXERR_MEM;` |
|         - | 1678 | `		}` |
|  10244431 | 1679 | `		pVmAttr->pAttr = pAttr;` |
|  10244431 | 1680 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 1681 | `			ph7_value *pMemObj;` |
|         - | 1682 | `			/* Reserve a memory object for this attribute */` |
|  10244271 | 1683 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|  10244271 | 1684 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1685 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1686 | `				return SXERR_MEM;` |
|         - | 1687 | `			}` |
|  10244271 | 1688 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|  10244271 | 1689 | `			pVmAttr->iState = 0;` |
|  10244271 | 1690 | `			pVmAttr->pOwner = pClass;` |
|  10244271 | 1691 | `			if( pAttr->pNativeValue ){` |
|         - | 1692 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - | 1693 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - | 1694 | `				 * literal cannot throw and the builder states the type itself. */` |
|   9782095 | 1695 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   5353226 | 1696 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1697 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1698 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 1699 | `				 * against the declaring class (no method frame here). */` |
|      2177 | 1700 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1701 | `				sxi32 rcExec;` |
|      2177 | 1702 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      2177 | 1703 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      2177 | 1704 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      2177 | 1705 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1706 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 1707 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 1708 | `					 * also type-check the leftover value (a spurious second` |
|         - | 1709 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 1710 | `					 * nothing further for the remaining attributes.` |
|         - | 1711 | `					 *` |
|         - | 1712 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - | 1713 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - | 1714 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - | 1715 | `					 * and the throw came back as a status nobody read: this` |
|         - | 1716 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - | 1717 | `					 * and the whole statement RESUMED after the catch — php` |
|         - | 1718 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - | 1719 | `					 * existing construction-aborted route. */` |
|        24 | 1720 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        24 | 1721 | `					bDefThrew = 1;` |
|      2166 | 1722 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1723 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 1724 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 1725 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 1726 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 1727 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 1728 | `					 * aborts construction) / the fetch-point router. */` |
|       327 | 1729 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       327 | 1730 | `					if( rcDef != SXRET_OK ){` |
|        13 | 1731 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 1732 | `						bDefThrew = 1;` |
|         6 | 1733 | `					}` |
|       166 | 1734 | `				}` |
|    461095 | 1735 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 1736 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 1737 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|    459059 | 1738 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|    229527 | 1739 | `			}` |
|  10244271 | 1740 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|  10244271 | 1741 | `			if( rc != SXRET_OK ){` |
|         - | 1742 | `				VmSlot sSlot;` |
|         - | 1743 | `				/* Restore memory object */` |
|       ! 0 | 1744 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1745 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1746 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1747 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1748 | `				return SXERR_MEM;` |
|         - | 1749 | `			}` |
|         - | 1750 | `			/* Install attribute in the reference table */` |
|  10244271 | 1751 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1752 | `			/* Register typed property slot for assignment-time enforcement.` |
|         - | 1753 | `			 * On failure roll back the just-installed hAttr entry and the` |
|         - | 1754 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|  10244271 | 1755 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|   7285423 | 1756 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|   7285423 | 1757 | `				if( rc != SXRET_OK ){` |
|         - | 1758 | `					VmSlot sSlot;` |
|       ! 0 | 1759 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1760 | `					sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1761 | `					sSlot.pUserData = 0;` |
|       ! 0 | 1762 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1763 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1764 | `					return SXERR_MEM;` |
|         - | 1765 | `				}` |
|   3642709 | 1766 | `			}` |
|   5122138 | 1767 | `		}else{` |
|         - | 1768 | `			/* Install static/constant attribute */` |
|       165 | 1769 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       165 | 1770 | `			pVmAttr->iState = 0;` |
|       165 | 1771 | `			pVmAttr->pOwner = pClass;` |
|       165 | 1772 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       165 | 1773 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1774 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1775 | `				return SXERR_MEM;` |
|         - | 1776 | `			}` |
|         - | 1777 | `		}` |
|         5 | 1778 | `	}` |
|   1577111 | 1779 | `	return SXRET_OK;` |
|    788558 | 1780 | `}` |
|         - | 1781 | `/*` |
|         - | 1782 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|         - | 1783 | ` * stdClass for now; the future general-dynamic-props work turns` |
|         - | 1784 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|         - | 1785 | ` */` |
|       162 | 1786 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1787 | `{` |
|       167 | 1788 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|         5 | 1789 | `}` |
|         - | 1790 | `/*` |
|         - | 1791 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 1792 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 1793 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 1794 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 1795 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 1796 | ` */` |
|        22 | 1797 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         4 | 1798 | `{` |
|        36 | 1799 | `	while( pClass ){` |
|        26 | 1800 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 1801 | `		sxu32 n;` |
|        26 | 1802 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|        12 | 1803 | `			if( aAttr[n].sName.nByte == nName` |
|        13 | 1804 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|        13 | 1805 | `				return TRUE;` |
|         - | 1806 | `			}` |
|       ! 0 | 1807 | `		}` |
|        13 | 1808 | `		pClass = pClass->pBase;` |
|         3 | 1809 | `	}` |
|        13 | 1810 | `	return FALSE;` |
|        15 | 1811 | `}` |
|         - | 1812 | `/*` |
|         - | 1813 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|         - | 1814 | ` * access or method call on such an instance is php's incomplete-object` |
|         - | 1815 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|         - | 1816 | ` * (array), get_object_vars) read its attribute table freely.` |
|         - | 1817 | ` */` |
|    223477 | 1818 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1819 | `{` |
|    223482 | 1820 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|         5 | 1821 | `}` |
|         - | 1822 | `/*` |
|         - | 1823 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|         - | 1824 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|         - | 1825 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|         - | 1826 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|         - | 1827 | ` * had one says "unknown", like php.` |
|         - | 1828 | ` */` |
|        44 | 1829 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|         1 | 1830 | `{` |
|        45 | 1831 | `	const char *zName = "unknown";` |
|        45 | 1832 | `	sxu32 nName = sizeof("unknown")-1;` |
|        45 | 1833 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|         - | 1834 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|        45 | 1835 | `	if( pEntry ){` |
|        45 | 1836 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        45 | 1837 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|        45 | 1838 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|        45 | 1839 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|        45 | 1840 | `			nName = SyBlobLength(&pVal->sBlob);` |
|        22 | 1841 | `		}` |
|        22 | 1842 | `	}` |
|        67 | 1843 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|         - | 1844 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|         - | 1845 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|        22 | 1846 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|        45 | 1847 | `}` |
|         - | 1848 | `/*` |
|         - | 1849 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|         - | 1850 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|         - | 1851 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|         - | 1852 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|         - | 1853 | ` * (property_exists() passes its name in pFuncName).` |
|         - | 1854 | ` */` |
|        16 | 1855 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|         1 | 1856 | `{` |
|         - | 1857 | `	SyBlob sMsg;` |
|        17 | 1858 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        17 | 1859 | `	if( pFuncName ){` |
|       ! 0 | 1860 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|       ! 0 | 1861 | `	}else{` |
|        17 | 1862 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|        17 | 1863 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|        17 | 1864 | `		if( pFunc == 0 ){` |
|       ! 0 | 1865 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|       ! 0 | 1866 | `		}else{` |
|        17 | 1867 | `			const char *zDisp = 0;` |
|         - | 1868 | `			int nDisp;` |
|        17 | 1869 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 | 1870 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         3 | 1871 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|         3 | 1872 | `				SyBlobAppend(&sMsg,"::",2);` |
|         1 | 1873 | `			}` |
|        17 | 1874 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|        17 | 1875 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|         - | 1876 | `		}` |
|         - | 1877 | `	}` |
|        17 | 1878 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|        17 | 1879 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|        25 | 1880 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|        16 | 1881 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        17 | 1882 | `	SyBlobRelease(&sMsg);` |
|        17 | 1883 | `}` |
|         - | 1884 | `/*` |
|         - | 1885 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 1886 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 1887 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 1888 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 1889 | ` *` |
|         - | 1890 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 1891 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 1892 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 1893 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 1894 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 1895 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 1896 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 1897 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 1898 | ` */` |
|       368 | 1899 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         4 | 1900 | `{` |
|         - | 1901 | `	ph7_class_attr *pAttr;` |
|       372 | 1902 | `	VmClassAttr *pVmAttr = 0;` |
|       372 | 1903 | `	ph7_value *pMemObj = 0;` |
|         - | 1904 | `	char *zCopy;` |
|         - | 1905 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       372 | 1906 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       372 | 1907 | `	if( pAttr == 0 ){` |
|       ! 0 | 1908 | `		return 0;` |
|         - | 1909 | `	}` |
|       372 | 1910 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       372 | 1911 | `	zCopy = (char *)&pAttr[1];` |
|       372 | 1912 | `	if( nName > 0 ){` |
|       364 | 1913 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       180 | 1914 | `	}` |
|       372 | 1915 | `	zCopy[nName] = 0;` |
|       372 | 1916 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       372 | 1917 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       372 | 1918 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       372 | 1919 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 1920 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 1921 | `	 * value, never a union. */` |
|       372 | 1922 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       372 | 1923 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1924 | `		goto fail_attr;` |
|         - | 1925 | `	}` |
|       372 | 1926 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       372 | 1927 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1928 | `		goto fail_vmattr;` |
|         - | 1929 | `	}` |
|       372 | 1930 | `	pVmAttr->pAttr = pAttr;` |
|       372 | 1931 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       372 | 1932 | `	pVmAttr->iState = 0;` |
|       372 | 1933 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1934 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 1935 | `	 * property-creation order, matching PHP. */` |
|       372 | 1936 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 1937 | `		goto fail_slot;` |
|         - | 1938 | `	}` |
|         - | 1939 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       372 | 1940 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|       372 | 1941 | `	if( ppAttr ){` |
|       144 | 1942 | `		*ppAttr = pVmAttr;` |
|        70 | 1943 | `	}` |
|       372 | 1944 | `	return pMemObj;` |
|       ! 0 | 1945 | `fail_slot:` |
|         - | 1946 | `	{` |
|         - | 1947 | `		VmSlot sSlot;` |
|       ! 0 | 1948 | `		sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1949 | `		sSlot.pUserData = 0;` |
|       ! 0 | 1950 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1951 | `	}` |
|       ! 0 | 1952 | `fail_vmattr:` |
|       ! 0 | 1953 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1954 | `fail_attr:` |
|       ! 0 | 1955 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 1956 | `	return 0;` |
|       188 | 1957 | `}` |
|         - | 1958 | `/*` |
|         - | 1959 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 1960 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 1961 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 1962 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 1963 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 1964 | ` */` |
|        10 | 1965 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         2 | 1966 | `{` |
|         - | 1967 | `	VmClassAttr *pVmAttr;` |
|         - | 1968 | `	ph7_value *pMemObj;` |
|        12 | 1969 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        12 | 1970 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1971 | `		return;` |
|         - | 1972 | `	}` |
|        12 | 1973 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|        12 | 1974 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1975 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1976 | `		return;` |
|         - | 1977 | `	}` |
|        12 | 1978 | `	pVmAttr->pAttr = pAttr;` |
|        12 | 1979 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|        12 | 1980 | `	pVmAttr->iState = 0;` |
|        12 | 1981 | `	pVmAttr->pOwner = pThis->pClass;` |
|         - | 1982 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 1983 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 1984 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 1985 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 1986 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|        12 | 1987 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         3 | 1988 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|         1 | 1989 | `	}` |
|         - | 1990 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 1991 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 1992 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 1993 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|        12 | 1994 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|         - | 1995 | `		VmSlot sSlot;` |
|       ! 0 | 1996 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 1997 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1998 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1999 | `		return;` |
|         - | 2000 | `	}` |
|        12 | 2001 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        12 | 2002 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         3 | 2003 | `		if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|         - | 2004 | `			VmSlot sSlot;` |
|       ! 0 | 2005 | `			SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 2006 | `			sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 2007 | `			SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 2008 | `			SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2009 | `			return;` |
|         - | 2010 | `		}` |
|         1 | 2011 | `	}` |
|        12 | 2012 | `	if( ppAttr ){` |
|        12 | 2013 | `		*ppAttr = pVmAttr;` |
|         5 | 2014 | `	}` |
|         7 | 2015 | `}` |
|         - | 2016 | `/* Forward declaration */` |
|         - | 2017 | `/*` |
|         - | 2018 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 2019 | ` */` |
|         - | 2020 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 2021 | `/*` |
|         - | 2022 | ` * Reserve a constant memory object.` |
|         - | 2023 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2024 | ` */` |
|   1359610 | 2025 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2026 | `{` |
|         - | 2027 | `	ph7_value *pObj;` |
|         - | 2028 | `	sxi32 rc;` |
|   1359615 | 2029 | `	if( pIndex ){` |
|         - | 2030 | `		/* Object index in the object table */` |
|   1344177 | 2031 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    672086 | 2032 | `	}` |
|         - | 2033 | `	/* Reserve a slot for the new object */` |
|   1359615 | 2034 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   1359615 | 2035 | `	if( rc != SXRET_OK ){` |
|         - | 2036 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2037 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2038 | `		 */` |
|       ! 0 | 2039 | `		return 0;` |
|         - | 2040 | `	}` |
|   1359615 | 2041 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   1359615 | 2042 | `	return pObj;` |
|    679810 | 2043 | `}` |
|         - | 2044 | `/*` |
|         - | 2045 | ` * Reserve a memory object.` |
|         - | 2046 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2047 | ` */` |
|   2973124 | 2048 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2049 | `{` |
|         - | 2050 | `	ph7_value *pObj;` |
|         - | 2051 | `	sxi32 rc;` |
|   2973129 | 2052 | `	if( pIndex ){` |
|         - | 2053 | `		/* Object index in the object table */` |
|   2973129 | 2054 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|   1486560 | 2055 | `	}` |
|         - | 2056 | `	/* Reserve a slot for the new object */` |
|   2973129 | 2057 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|   2973129 | 2058 | `	if( rc != SXRET_OK ){` |
|         - | 2059 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2060 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2061 | `		 */` |
|       ! 0 | 2062 | `		return 0;` |
|         - | 2063 | `	}` |
|   2973129 | 2064 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|   2973129 | 2065 | `	return pObj;` |
|   1486565 | 2066 | `}` |
|         - | 2067 | `/* Forward declaration */` |
|         - | 2068 | `/* Forward declarations for Fiber C functions */` |
|         - | 2069 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 2070 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 2071 | `/*` |
|         - | 2072 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 2073 | ` * directly as foreign functions.` |
|         - | 2074 | ` */` |
|         - | 2075 |  |
|         - | 2076 | `/*` |
|         - | 2077 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 2078 | ` * start compiling the target PHP program.` |
|         - | 2079 | ` */` |
|      5146 | 2080 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 2081 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 2082 | `	 ph7 *pEngine /* Master engine */` |
|         - | 2083 | `	 )` |
|         5 | 2084 | `{` |
|         - | 2085 | `	ph7_value *pObj;` |
|         - | 2086 | `	sxi32 rc;` |
|         - | 2087 | `	/* Zero the structure */` |
|      5151 | 2088 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 2089 | `	/* Initialize VM fields */` |
|      5151 | 2090 | `	pVm->pEngine = &(*pEngine);` |
|      5151 | 2091 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|         - | 2092 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 2093 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 2094 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      5151 | 2095 | `	pVm->bDisplayErrors = 0;` |
|      5151 | 2096 | `	pVm->bLogErrors = 1;` |
|         - | 2097 | `	/* mbstring's substitute character, php's default (the internal encoding` |
|         - | 2098 | `	 * beside it is UTF-8, which is the zero the struct already holds) */` |
|      5151 | 2099 | `	pVm->iMbSubstitute = '?';` |
|      5151 | 2100 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 2101 | `	/* Instructions containers */` |
|      5151 | 2102 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      5151 | 2103 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      5151 | 2104 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 2105 | `	/* Object containers */` |
|      5151 | 2106 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      5151 | 2107 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|         - | 2108 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      5151 | 2109 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      5151 | 2110 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      5151 | 2111 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      5151 | 2112 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 2113 | `	/* Virtual machine internal containers */` |
|      5151 | 2114 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      5151 | 2115 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      5151 | 2116 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      5151 | 2117 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|      5151 | 2118 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      5151 | 2119 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 2120 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 2121 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 2122 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 2123 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 2124 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 2125 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 2126 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 2127 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      5151 | 2128 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5151 | 2129 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5151 | 2130 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5151 | 2131 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      5151 | 2132 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      5151 | 2133 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      5151 | 2134 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|      5151 | 2135 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      5151 | 2136 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      5151 | 2137 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      5151 | 2138 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|      5151 | 2139 | `	SySetInit(&pVm->aPersistSock,&pVm->sAllocator,sizeof(VmPersistSock));` |
|      5151 | 2140 | `	pVm->bIniSeeded = 0;` |
|      5151 | 2141 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|      5151 | 2142 | `	PH7_MemObjInit(&(*pVm),&pVm->sSessHandler);` |
|      5151 | 2143 | `	SyBlobInit(&pVm->sSessData,&pVm->sAllocator);` |
|      5151 | 2144 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|      5151 | 2145 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|      5151 | 2146 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|      5151 | 2147 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|      5151 | 2148 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      5151 | 2149 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      5151 | 2150 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      5151 | 2151 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|      5151 | 2152 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      5151 | 2153 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      5151 | 2154 | `	pVm->nResourceIdNext = 1;` |
|      5151 | 2155 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      5151 | 2156 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      5151 | 2157 | `	pVm->pMagicSetThis = 0;` |
|      5151 | 2158 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      5151 | 2159 | `	pVm->pHookSetThis = 0;` |
|      5151 | 2160 | `	pVm->pHookSetAttr = 0;` |
|      5151 | 2161 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      5151 | 2162 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      5151 | 2163 | `	pVm->pMagicCallThis = 0;` |
|      5151 | 2164 | `	pVm->pMagicCallClass = 0;` |
|      5151 | 2165 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      5151 | 2166 | `	pVm->pIdleCallFrames = 0;` |
|      5151 | 2167 | `	pVm->pIdleOperandStacks = 0;` |
|      5151 | 2168 | `	pVm->nIdleOperandStacks = 0;` |
|      5151 | 2169 | `	pVm->pIdleStackNodes = 0;` |
|      5151 | 2170 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      5151 | 2171 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      5151 | 2172 | `	pVm->pPendingException = 0;` |
|      5151 | 2173 | `	pVm->pInflightException = 0;` |
|      5151 | 2174 | `	pVm->nInflightExcBase = 0;` |
|      5151 | 2175 | `	pVm->pResumeFrame = 0;` |
|      5151 | 2176 | `	pVm->iResumePc = 0;` |
|      5151 | 2177 | `	pVm->pResumeInstr = 0;` |
|      5151 | 2178 | `	pVm->iResumeStackDepth = 0;` |
|      5151 | 2179 | `	pVm->nBoundaryRc = 0;` |
|      5151 | 2180 | `	pVm->pConstEvalClass = 0;` |
|      5151 | 2181 | `	pVm->nConstEvalDepth = 0;` |
|      5151 | 2182 | `	pVm->pConstCycleAttr = 0;` |
|      5151 | 2183 | `	pVm->pConstCycleClass = 0;` |
|      5151 | 2184 | `	SySetReset(&pVm->aMagicGuard);` |
|      5151 | 2185 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2186 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2187 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2188 | `	}` |
|      5151 | 2189 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      5151 | 2190 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2191 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2192 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2193 | `	}` |
|      5151 | 2194 | `	pVm->pHookSetAttr = 0;` |
|      5151 | 2195 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      5151 | 2196 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2197 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2198 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2199 | `	}` |
|      5151 | 2200 | `	pVm->pMagicCallClass = 0;` |
|      5151 | 2201 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 2202 | `	/* Configuration containers */` |
|      5151 | 2203 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      5151 | 2204 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      5151 | 2205 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      5151 | 2206 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      5151 | 2207 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|      5151 | 2208 | `	pVm->iResponseStatus = 200;` |
|      5151 | 2209 | `	pVm->bHeadersSent = 0;` |
|      5151 | 2210 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|      5151 | 2211 | `	SySetInit(&pVm->aSuppressedIo,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 2212 | `	/* Error callbacks containers */` |
|      5151 | 2213 | `	PH7_MemObjInit(&(*pVm),&pVm->sExceptionCB);` |
|      5151 | 2214 | `	PH7_MemObjInit(&(*pVm),&pVm->sErrCB);` |
|      5151 | 2215 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|      5151 | 2216 | `	SySetInit(&pVm->aExceptionCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      5151 | 2217 | `	SySetInit(&pVm->aErrCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      5151 | 2218 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 2219 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 2220 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 2221 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 2222 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 2223 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 2224 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 2225 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      5151 | 2226 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      5151 | 2227 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 2228 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 2229 | `#else` |
|         - | 2230 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 2231 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 2232 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 2233 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 2234 | `	pVm->nMaxDepth = 512;` |
|         - | 2235 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 2236 | `#endif` |
|         - | 2237 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 2238 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 2239 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      5151 | 2240 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 2241 | `	/* JSON return status */` |
|      5151 | 2242 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2243 | `	/* PRNG context */` |
|      5151 | 2244 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 2245 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 2246 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      5151 | 2247 | `	pVm->mtSeeded = FALSE;` |
|         - | 2248 | `	/* Install the null constant */` |
|      5151 | 2249 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5151 | 2250 | `	if( pObj == 0 ){` |
|       ! 0 | 2251 | `		rc = SXERR_MEM;` |
|       ! 0 | 2252 | `		goto Err;` |
|         - | 2253 | `	}` |
|      5151 | 2254 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 2255 | `	/* Install the boolean TRUE constant */` |
|      5151 | 2256 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5151 | 2257 | `	if( pObj == 0 ){` |
|       ! 0 | 2258 | `		rc = SXERR_MEM;` |
|       ! 0 | 2259 | `		goto Err;` |
|         - | 2260 | `	}` |
|      5151 | 2261 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 2262 | `	/* Install the boolean FALSE constant */` |
|      5151 | 2263 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5151 | 2264 | `	if( pObj == 0 ){` |
|       ! 0 | 2265 | `		rc = SXERR_MEM;` |
|       ! 0 | 2266 | `		goto Err;` |
|         - | 2267 | `	}` |
|      5151 | 2268 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 2269 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 2270 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 2271 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      5151 | 2272 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      5151 | 2273 | `	if( pObj == 0 ){` |
|       ! 0 | 2274 | `		rc = SXERR_MEM;` |
|       ! 0 | 2275 | `		goto Err;` |
|         - | 2276 | `	}` |
|      5151 | 2277 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 2278 | `	/* Create the global frame */` |
|      5151 | 2279 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      5151 | 2280 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2281 | `		goto Err;` |
|         - | 2282 | `	}` |
|         - | 2283 | `	/* Initialize the code generator */` |
|      5151 | 2284 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      5151 | 2285 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2286 | `		goto Err;` |
|         - | 2287 | `	}` |
|         - | 2288 | `	/* VM correctly initialized,set the magic number */` |
|      5151 | 2289 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 2290 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 2291 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      5151 | 2292 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 2293 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      5151 | 2294 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 2295 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 2296 | `	 * compiled — its classes are internal too. */` |
|         - | 2297 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      5151 | 2298 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      5151 | 2299 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      5151 | 2300 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      5151 | 2301 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      5151 | 2302 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 2303 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      5151 | 2304 | `	pVm->pCoalesceObj = 0;` |
|      5151 | 2305 | `	pVm->bCoalesceArmed = 0;` |
|      5151 | 2306 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 2307 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - | 2308 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - | 2309 | ``	 * segfaults the first `new Fiber`. */`` |
|      5151 | 2310 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      5151 | 2311 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 2312 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|         - | 2313 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|         - | 2314 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|         - | 2315 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|         - | 2316 | `	 * rather than being stamped on afterwards. */` |
|      5151 | 2317 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|      5151 | 2318 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      5151 | 2319 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      5151 | 2320 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 2321 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      5151 | 2322 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 2323 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|      5151 | 2324 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|         - | 2325 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|         - | 2326 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - | 2327 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - | 2328 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - | 2329 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      5151 | 2330 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      5151 | 2331 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - | 2332 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|         - | 2333 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|         - | 2334 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|      5151 | 2335 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|         - | 2336 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 2337 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 2338 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      5151 | 2339 | `	PH7_VmInstallReflection(&(*pVm));` |
|      5151 | 2340 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      5151 | 2341 | `	PH7_VmInstallSpl(&(*pVm));` |
|         - | 2342 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      5151 | 2343 | `	PH7_VmInstallHashContext(&(*pVm));` |
|         - | 2344 | `#endif` |
|         - | 2345 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2346 | `	/* php_user_filter and StreamBucket: the classes stream_filter_register()` |
|         - | 2347 | `	 * builds its filters out of. */` |
|      5151 | 2348 | `	PH7_VmInstallStreamFilter(&(*pVm));` |
|         - | 2349 | `#endif` |
|      5151 | 2350 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|      5151 | 2351 | `	PH7_VmInstallSession(&(*pVm));` |
|      5151 | 2352 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 2353 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2354 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|         - | 2355 | `	 * XMLWriter class libraries that build on it. */` |
|      5151 | 2356 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      5151 | 2357 | `	PH7_VmInstallDom(&(*pVm));` |
|      5151 | 2358 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 2359 | `#endif` |
|      5151 | 2360 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 2361 | `	/* Reset the code generator */` |
|      5151 | 2362 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      5151 | 2363 | `	return SXRET_OK;` |
|       ! 0 | 2364 | `Err:` |
|       ! 0 | 2365 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 2366 | `	return rc;` |
|      2578 | 2367 | `}` |
|         - | 2368 | `/*` |
|         - | 2369 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 2370 | ` * routine which store the output in an internal blob.` |
|         - | 2371 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 2372 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 2373 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 2374 | ` * Refer to the official docurmentation for additional information.` |
|         - | 2375 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 2376 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 2377 | ` * to finish executing and extracting the output.` |
|         - | 2378 | ` */` |
|       348 | 2379 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 2380 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 2381 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 2382 | `	void *pUserData     /* User private data */` |
|         - | 2383 | `	)` |
|         4 | 2384 | `{` |
|         - | 2385 | `	 sxi32 rc;` |
|         - | 2386 | `	 /* Store the output in an internal BLOB */` |
|       352 | 2387 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       352 | 2388 | `	 return rc;` |
|         4 | 2389 | `}` |
|         - | 2390 | `/*` |
|         - | 2391 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 2392 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 2393 | ` */` |
|    112924 | 2394 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 2395 | `{` |
|    112929 | 2396 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    112929 | 2397 | `	if( xCons != VmObConsumer ){` |
|     35707 | 2398 | `		pVm->nOutputLen += nLen;` |
|     35707 | 2399 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      1531 | 2400 | `			pVm->bHeadersSent = 1;` |
|       763 | 2401 | `		}` |
|     17851 | 2402 | `	}` |
|    112929 | 2403 | `}` |
|         - | 2404 | `/*` |
|         - | 2405 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 2406 | ` *` |
|         - | 2407 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 2408 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 2409 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 2410 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 2411 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 2412 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 2413 | ` *` |
|         - | 2414 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 2415 | ` * conservative BY CONSTRUCTION:` |
|         - | 2416 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 2417 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 2418 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 2419 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 2420 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 2421 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 2422 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 2423 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 2424 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 2425 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 2426 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 2427 | ` *     instruction-count bound -> fallback.` |
|         - | 2428 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 2429 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 2430 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 2431 | ` *` |
|         - | 2432 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 2433 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 2434 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 2435 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 2436 | ` *` |
|         - | 2437 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 2438 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 2439 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 2440 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 2441 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 2442 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 2443 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 2444 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 2445 | ` * entry here.` |
|         - | 2446 | ` */` |
|         - | 2447 | `/*` |
|         - | 2448 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 2449 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 2450 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 2451 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 2452 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 2453 | ` */` |
|     46610 | 2454 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 2455 | `{` |
|     46615 | 2456 | `	int push = 0, n = 0;` |
|         - | 2457 | `	sxi32 d;` |
|     46615 | 2458 | `	switch( pI->iOp ){` |
|         - | 2459 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 2460 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|      8782 | 2461 | `	case PH7_OP_LOADC:` |
|         - | 2462 | `	case PH7_OP_DUP:` |
|     17568 | 2463 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      3074 | 2464 | `	case PH7_OP_LOAD:` |
|      6153 | 2465 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|      6153 | 2466 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         9 | 2467 | `	case PH7_OP_LOAD_REF:` |
|        19 | 2468 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2469 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       571 | 2470 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 2471 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 2472 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1147 | 2473 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 2474 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 2475 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       201 | 2476 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 2477 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       407 | 2478 | `		if( pI->iP2 == 0 ){` |
|       407 | 2479 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       206 | 2480 | `		}else{` |
|       ! 0 | 2481 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 2482 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 2483 | `		}` |
|       407 | 2484 | `		break;` |
|         - | 2485 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 2486 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       143 | 2487 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 2488 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 2489 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 2490 | `	case PH7_OP_NOOP:` |
|       291 | 2491 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2492 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 2493 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|       587 | 2494 | `	case PH7_OP_STORE:` |
|      1179 | 2495 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      1179 | 2496 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 2497 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1190 | 2498 | `	case PH7_OP_POP:` |
|         - | 2499 | `	case PH7_OP_CONSUME:` |
|      2385 | 2500 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2501 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|         - | 2502 | `	 * pushed and none is popped, on any path. */` |
|       ! 0 | 2503 | `	case PH7_OP_ROT_CALLEE:` |
|       ! 0 | 2504 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2505 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 2506 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 2507 | `	 * true pop count is a runtime value — never reaches here. */` |
|       239 | 2508 | `	case PH7_OP_CALL:` |
|       483 | 2509 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2510 | `	/* Jumps. */` |
|        90 | 2511 | `	case PH7_OP_JMP:` |
|       184 | 2512 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       142 | 2513 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|       289 | 2514 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|       289 | 2515 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 2516 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      3105 | 2517 | `	case PH7_OP_DONE:` |
|      6215 | 2518 | `		n = 0; break;` |
|      5173 | 2519 | `	default:` |
|     10350 | 2520 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 2521 | `	}` |
|     36270 | 2522 | `	*pPush = push; *pN = n;` |
|     36270 | 2523 | `	return 1;` |
|     23309 | 2524 | `}` |
|         - | 2525 | `/*` |
|         - | 2526 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 2527 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 2528 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 2529 | ` */` |
|     12503 | 2530 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 2531 | `{` |
|         - | 2532 | `	void *pScratch;` |
|         - | 2533 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 2534 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 2535 | `	sxi32 iMax;` |
|         - | 2536 | `	int push, n, k;` |
|         - | 2537 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     12508 | 2538 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 2539 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 2540 | `		return VM_STACK_UNMODELED;` |
|         - | 2541 | `	}` |
|         - | 2542 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     41799 | 2543 | `	for( i = 0; i < nInstr; i++ ){` |
|     39641 | 2544 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     10350 | 2545 | `			return VM_STACK_UNMODELED;` |
|         - | 2546 | `		}` |
|     14650 | 2547 | `	}` |
|         - | 2548 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 2549 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 2550 | `	 * first (the byte array last needs no alignment). */` |
|      2163 | 2551 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      2163 | 2552 | `	if( pScratch == 0 ){` |
|       ! 0 | 2553 | `		return VM_STACK_UNMODELED;` |
|         - | 2554 | `	}` |
|      2163 | 2555 | `	aH  = (sxi32 *)pScratch;` |
|      2163 | 2556 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      2163 | 2557 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     11035 | 2558 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      2163 | 2559 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      2163 | 2560 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|      9137 | 2561 | `	while( nQ > 0 ){` |
|      6979 | 2562 | `		sxu32 pc = aQ[--nQ];` |
|         - | 2563 | `		sxi32 h;` |
|      6979 | 2564 | `		aIn[pc] = 0;` |
|      6979 | 2565 | `		h = aH[pc];` |
|      6979 | 2566 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|      6979 | 2567 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|      6979 | 2568 | `		if( h + push > iMax ){ iMax = h + push; }` |
|      6979 | 2569 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     11809 | 2570 | `		for( k = 0; k < n; k++ ){` |
|      4835 | 2571 | `			sxi32 hn = h + delta[k];` |
|      4835 | 2572 | `			sxu32 t = succ[k];` |
|      4835 | 2573 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      4835 | 2574 | `			if( hn > aH[t] ){` |
|      4821 | 2575 | `				aH[t] = hn;` |
|      4821 | 2576 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      2408 | 2577 | `			}` |
|      2420 | 2578 | `		}` |
|      6979 | 2579 | `		if( iMax < 0 ){ break; }` |
|         5 | 2580 | `	}` |
|      2163 | 2581 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      2163 | 2582 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|      6256 | 2583 | `}` |
|         - | 2584 | `/*` |
|         - | 2585 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 2586 | ` * our compiled PHP program.` |
|         - | 2587 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 2588 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 2589 | ` *` |
|         - | 2590 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 2591 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 2592 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 2593 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 2594 | ` */` |
|   3452907 | 2595 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 2596 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2597 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 2598 | `	)` |
|         5 | 2599 | `{` |
|         - | 2600 | `	ph7_value *pStack;` |
|         - | 2601 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 2602 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 2603 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 2604 | `  ** on the maximum stack depth required.` |
|         - | 2605 | `  **` |
|         - | 2606 | `  ** Allocation all the stack space we will ever need.` |
|         - | 2607 | `  */` |
|   3452912 | 2608 | `	nInstr += VM_STACK_GUARD;` |
|   3452912 | 2609 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   3452912 | 2610 | `	if( pStack == 0 ){` |
|       ! 0 | 2611 | `		return 0;` |
|         - | 2612 | `	}` |
|         - | 2613 | `	/* Initialize the operand stack */` |
|  72819590 | 2614 | `	while( nInstr > 0 ){` |
|  69366683 | 2615 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  69366683 | 2616 | `		--nInstr;` |
|         5 | 2617 | `	}` |
|         - | 2618 | `	/* Ready for bytecode execution */` |
|   3452912 | 2619 | `	return pStack;` |
|   1726511 | 2620 | `}` |
|         - | 2621 | `/*` |
|         - | 2622 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 2623 | ` *` |
|         - | 2624 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 2625 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 2626 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 2627 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 2628 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 2629 | ` * the per-slot PH7_MemObjInit.` |
|         - | 2630 | ` *` |
|         - | 2631 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 2632 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 2633 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 2634 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|         - | 2635 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|         - | 2636 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|         - | 2637 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|         - | 2638 | ` *` |
|         - | 2639 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|         - | 2640 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|         - | 2641 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|         - | 2642 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|         - | 2643 | ` * recursion case is the one worth the O(1) simplicity.` |
|         - | 2644 | ` */` |
|         - | 2645 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 2646 | `struct VmIdleStack {` |
|         - | 2647 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 2648 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 2649 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 2650 | `};` |
|         - | 2651 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|         - | 2652 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|         - | 2653 | `                                    * (a large fallback-sized stack recursing would` |
|         - | 2654 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|         - | 2655 | `                                    * the tight-sized hot case is far below this) */` |
|         - | 2656 | `/*` |
|         - | 2657 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 2658 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 2659 | ` */` |
|    759036 | 2660 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 2661 | `{` |
|    759041 | 2662 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    759041 | 2663 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|    759041 | 2664 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    689597 | 2665 | `		ph7_value *pStack = pIdle->pStack;` |
|    689597 | 2666 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    689597 | 2667 | `		pVm->nIdleOperandStacks--;` |
|         - | 2668 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 2669 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    689597 | 2670 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    689597 | 2671 | `		pVm->pIdleStackNodes = pIdle;` |
|    689597 | 2672 | `		return pStack; /* slots already released -> reusable without re-init */` |
|         - | 2673 | `	}` |
|     69449 | 2674 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    379745 | 2675 | `}` |
|         - | 2676 | `/*` |
|         - | 2677 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 2678 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|         - | 2679 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|         - | 2680 | ` * live value.` |
|         - | 2681 | ` */` |
|    758836 | 2682 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|         5 | 2683 | `{` |
|         - | 2684 | `	VmIdleStack *pIdle;` |
|         - | 2685 | `	sxu32 i;` |
|    758841 | 2686 | `	if( pStack == 0 ){` |
|       ! 0 | 2687 | `		return;` |
|         - | 2688 | `	}` |
|    758841 | 2689 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|     64435 | 2690 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|     64435 | 2691 | `		return;` |
|         - | 2692 | `	}` |
|         - | 2693 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 2694 | `	 * pool-allocate only when the spare list is empty. */` |
|    694411 | 2695 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    694411 | 2696 | `	if( pIdle ){` |
|    689597 | 2697 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    344968 | 2698 | `	}else{` |
|      4819 | 2699 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      4819 | 2700 | `		if( pIdle == 0 ){` |
|       ! 0 | 2701 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 2702 | `			return;` |
|         - | 2703 | `		}` |
|         - | 2704 | `	}` |
|  45452517 | 2705 | `	for( i = 0; i < nCap; i++ ){` |
|  44758111 | 2706 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 2707 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 2708 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 2709 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 2710 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 2711 | `		 * across invocations — cheap defense in depth. */` |
|  44758111 | 2712 | `		pStack[i].nIdx = SXU32_HIGH;` |
|  22448754 | 2713 | `	}` |
|    694411 | 2714 | `	pIdle->pStack = pStack;` |
|    694411 | 2715 | `	pIdle->nCap = nCap;` |
|    694411 | 2716 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    694411 | 2717 | `	pVm->pIdleOperandStacks = pIdle;` |
|    694411 | 2718 | `	pVm->nIdleOperandStacks++;` |
|    379645 | 2719 | `}` |
|         - | 2720 | `/* Forward declaration */` |
|         - | 2721 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 2722 | `/*` |
|         - | 2723 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 2724 | ` * This routine gets called by the PH7 engine after` |
|         - | 2725 | ` * successful compilation of the target PHP program.` |
|         - | 2726 | ` */` |
|      4552 | 2727 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 2728 | `	ph7_vm *pVm /* Target VM */` |
|         - | 2729 | `	)` |
|         5 | 2730 | `{` |
|         - | 2731 | `	SyHashEntry *pEntry;` |
|         - | 2732 | `	sxi32 rc;` |
|      4557 | 2733 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 2734 | `		/* Initialize your VM first */` |
|       ! 0 | 2735 | `		return SXERR_CORRUPT;` |
|         - | 2736 | `	}` |
|         - | 2737 | `	/* Mark the VM ready for byte-code execution */` |
|      4557 | 2738 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 2739 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 2740 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 2741 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 2742 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      4557 | 2743 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 2744 | `	/* Emit the DONE instruction */` |
|      4557 | 2745 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      4557 | 2746 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2747 | `		return SXERR_MEM;` |
|         - | 2748 | `	}` |
|         - | 2749 | `	/* Script return value */` |
|      4557 | 2750 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 2751 | `	/* Allocate a new operand stack */` |
|      4557 | 2752 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      4557 | 2753 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 2754 | `		return SXERR_MEM;` |
|         - | 2755 | `	}` |
|         - | 2756 | `	/* Set the default VM output consumer callback and it's` |
|         - | 2757 | `	 * private data. */` |
|      4557 | 2758 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      4557 | 2759 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 2760 | `	/* Allocate the reference table */` |
|      4557 | 2761 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      4557 | 2762 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      4557 | 2763 | `	if( pVm->apRefObj == 0 ){` |
|         - | 2764 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2765 | `		return SXERR_MEM;` |
|         - | 2766 | `	}` |
|         - | 2767 | `	/* Zero the reference table */` |
|      4557 | 2768 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|         - | 2769 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      4557 | 2770 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      4557 | 2771 | `	if( rc != SXRET_OK ){` |
|         - | 2772 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2773 | `		return rc;` |
|         - | 2774 | `	}` |
|         - | 2775 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 2776 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 2777 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 2778 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 2779 | `	 * below it is compile-time/init state that survives a reset. */` |
|      4557 | 2780 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|         - | 2781 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      4557 | 2782 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      4557 | 2783 | `	if( rc != SXRET_OK ){` |
|         - | 2784 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2785 | `		return rc;` |
|         - | 2786 | `	}` |
|         - | 2787 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      4557 | 2788 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 2789 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      4557 | 2790 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 2791 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      4557 | 2792 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 2793 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      4557 | 2794 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 2795 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 2796 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      4557 | 2797 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      4557 | 2798 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 2799 | `#endif` |
|         - | 2800 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2801 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      4557 | 2802 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 2803 | `#endif` |
|         - | 2804 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 2805 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      4557 | 2806 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 2807 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      4557 | 2808 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 2809 | `	/* Initialize and install static and constants class attributes.` |
|         - | 2810 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 2811 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 2812 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 2813 | `	 * that function in sync when changing what is reserved here. */` |
|         - | 2814 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - | 2815 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - | 2816 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - | 2817 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - | 2818 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - | 2819 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - | 2820 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - | 2821 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 2822 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - | 2823 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      4557 | 2824 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    656847 | 2825 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    652295 | 2826 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    652295 | 2827 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2828 | `			return rc;` |
|         - | 2829 | `		}` |
|         5 | 2830 | `	}` |
|      4557 | 2831 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    656269 | 2832 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    651721 | 2833 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    651721 | 2834 | `		if( rc != SXRET_OK ){` |
|         6 | 2835 | `			return rc;` |
|         - | 2836 | `		}` |
|         5 | 2837 | `	}` |
|         - | 2838 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      4553 | 2839 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2840 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      4553 | 2841 | `	pVm->nNextObjId = 1;` |
|         - | 2842 | `	/* VM is ready for bytecode execution */` |
|      4553 | 2843 | `	return SXRET_OK;` |
|      2281 | 2844 | `}` |
|         - | 2845 | `/*` |
|         - | 2846 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 2847 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 2848 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 2849 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 2850 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 2851 | ` */` |
|        16 | 2852 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 2853 | `{` |
|         - | 2854 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|         - | 2855 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|         - | 2856 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|       540 | 2857 | `	while( pVm->pRefList ){` |
|       524 | 2858 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|       ! 0 | 2859 | `	}` |
|        16 | 2860 | `}` |
|         - | 2861 | `/*` |
|         - | 2862 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 2863 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 2864 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 2865 | ` */` |
|        96 | 2866 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 2867 | `{` |
|        96 | 2868 | `	PH7_MemObjRelease(pObj);` |
|        96 | 2869 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        96 | 2870 | `}` |
|         - | 2871 | `/*` |
|         - | 2872 | ` * Empty a set_error_handler()/set_exception_handler() stack, releasing every` |
|         - | 2873 | ` * saved handler. The SySet itself keeps its buffer for the next request; the` |
|         - | 2874 | ` * whole thing dies with the VM allocator either way.` |
|         - | 2875 | ` */` |
|        32 | 2876 | `static void VmReleaseHandlerStack(SySet *pStack)` |
|       ! 0 | 2877 | `{` |
|        32 | 2878 | `	VmHandlerSlot *aSlot = (VmHandlerSlot *)SySetBasePtr(pStack);` |
|         - | 2879 | `	sxu32 n;` |
|        32 | 2880 | `	for( n = 0 ; n < SySetUsed(pStack) ; ++n ){` |
|       ! 0 | 2881 | `		PH7_MemObjRelease(&aSlot[n].sCb);` |
|       ! 0 | 2882 | `	}` |
|        32 | 2883 | `	SySetReset(pStack);` |
|        32 | 2884 | `}` |
|         - | 2885 | `/*` |
|         - | 2886 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 2887 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 2888 | ` * of statics).` |
|         - | 2889 | ` */` |
|     12664 | 2890 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|       ! 0 | 2891 | `{` |
|     12664 | 2892 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 2893 | `	sxu32 k;` |
|     12668 | 2894 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|         4 | 2895 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|         2 | 2896 | `	}` |
|     12664 | 2897 | `}` |
|         - | 2898 | `/*` |
|         - | 2899 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 2900 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 2901 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 2902 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 2903 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 2904 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 2905 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 2906 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 2907 | ` *    has its static sentinels reset.` |
|         - | 2908 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 2909 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 2910 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 2911 | ` */` |
|        16 | 2912 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 2913 | `{` |
|         - | 2914 | `	SyHashEntry *pEntry;` |
|        16 | 2915 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     12680 | 2916 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     12664 | 2917 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     12664 | 2918 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 2919 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|         - | 2920 | `			 * release its captured-by-value environment, then free the entry,` |
|         - | 2921 | `			 * name buffer and structure. */` |
|         4 | 2922 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|         4 | 2923 | `			const char *zName = SyStringData(&pFunc->sName);` |
|         - | 2924 | `			sxu32 k;` |
|         4 | 2925 | `			VmResetFuncStatics(pFunc);` |
|         8 | 2926 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|         4 | 2927 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|         2 | 2928 | `			}` |
|         4 | 2929 | `			SySetRelease(&pFunc->aClosureEnv);` |
|         - | 2930 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 2931 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 2932 | `			if( zName ){` |
|         4 | 2933 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|         2 | 2934 | `			}` |
|         4 | 2935 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|         4 | 2936 | `			continue;` |
|         - | 2937 | `		}` |
|         - | 2938 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     25320 | 2939 | `		while( pFunc ){` |
|     12660 | 2940 | `			VmResetFuncStatics(pFunc);` |
|     12660 | 2941 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 2942 | `		}` |
|       ! 0 | 2943 | `	}` |
|        16 | 2944 | `	pVm->closure_cnt = 0;` |
|        16 | 2945 | `}` |
|         - | 2946 | `/*` |
|         - | 2947 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 2948 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 2949 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 2950 | ` * the class re-mount registers fresh ones.` |
|         - | 2951 | ` */` |
|        16 | 2952 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 2953 | `{` |
|         - | 2954 | `	SyHashEntry *pEntry;` |
|         - | 2955 | `	/* Common case: no class static typed properties — table already empty. */` |
|        16 | 2956 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        12 | 2957 | `		return;` |
|         - | 2958 | `	}` |
|         - | 2959 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 2960 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 2961 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 2962 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 2963 | `		if( pEntry->pUserData ){` |
|         4 | 2964 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 2965 | `		}` |
|       ! 0 | 2966 | `	}` |
|         4 | 2967 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 2968 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         8 | 2969 | `}` |
|         - | 2970 | `/*` |
|         - | 2971 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 2972 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 2973 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 2974 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 2975 | ` * comparing equal — both used to cast to 1.` |
|         - | 2976 | ` *` |
|         - | 2977 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 2978 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 2979 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 2980 | ` * aborting a cast.` |
|         - | 2981 | ` */` |
|        72 | 2982 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         3 | 2983 | `{` |
|         - | 2984 | `	SyHashEntry *pEntry;` |
|         - | 2985 | `	phl_res_id *pRec;` |
|        75 | 2986 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 2987 | `		return 0;` |
|         - | 2988 | `	}` |
|        75 | 2989 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|        75 | 2990 | `	if( pEntry ){` |
|        45 | 2991 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 2992 | `	}` |
|        33 | 2993 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        33 | 2994 | `	if( pRec == 0 ){` |
|       ! 0 | 2995 | `		return 0;` |
|         - | 2996 | `	}` |
|        33 | 2997 | `	pRec->pRes = pRes;` |
|        33 | 2998 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        33 | 2999 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 3000 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 3001 | `		return 0;` |
|         - | 3002 | `	}` |
|        33 | 3003 | `	return pRec->nId;` |
|        39 | 3004 | `}` |
|         - | 3005 | `/*` |
|         - | 3006 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 3007 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 3008 | ` */` |
|        16 | 3009 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 3010 | `{` |
|         - | 3011 | `	SyHashEntry *pEntry;` |
|        16 | 3012 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        16 | 3013 | `		pVm->nResourceIdNext = 1;` |
|        16 | 3014 | `		return;` |
|         - | 3015 | `	}` |
|       ! 0 | 3016 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 3017 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 3018 | `		if( pEntry->pUserData ){` |
|       ! 0 | 3019 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 3020 | `		}` |
|       ! 0 | 3021 | `	}` |
|       ! 0 | 3022 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 3023 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 3024 | `	pVm->nResourceIdNext = 1;` |
|         8 | 3025 | `}` |
|         - | 3026 | `/*` |
|         - | 3027 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 3028 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 3029 | ` *` |
|         - | 3030 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 3031 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 3032 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 3033 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 3034 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 3035 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 3036 | ` *` |
|         - | 3037 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 3038 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 3039 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 3040 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 3041 | ` * reserved during the run.` |
|         - | 3042 | ` *` |
|         - | 3043 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 3044 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 3045 | ` * global-scope destructors never fired.` |
|         - | 3046 | ` */` |
|        16 | 3047 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 3048 | `{` |
|         - | 3049 | `	sxu32 nWater,n;` |
|        16 | 3050 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 3051 | `		return SXERR_CORRUPT;` |
|         - | 3052 | `	}` |
|        16 | 3053 | `	nWater = pVm->nSuperBaseline;` |
|         - | 3054 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 3055 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        16 | 3056 | `	pVm->pGlobal = 0;` |
|         - | 3057 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 3058 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 3059 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 3060 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 3061 | `	 * object); unref'ing here would race the teardown below. */` |
|        16 | 3062 | `	pVm->pClosureThis = 0;` |
|        16 | 3063 | `	pVm->pClosureScope = 0;` |
|         - | 3064 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 3065 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 3066 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|         - | 3067 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        16 | 3068 | `	pVm->bInReset = 1;` |
|         - | 3069 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        16 | 3070 | `	VmResetRefTable(&(*pVm));` |
|         - | 3071 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 3072 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 3073 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 3074 | `	 * re-run define() overwrites the value in place). */` |
|        16 | 3075 | `	VmResetFunctionState(&(*pVm));` |
|         - | 3076 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 3077 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       560 | 3078 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|       544 | 3079 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|       544 | 3080 | `		if( pObj ){` |
|       544 | 3081 | `			PH7_MemObjRelease(pObj);` |
|       272 | 3082 | `		}` |
|       272 | 3083 | `	}` |
|         - | 3084 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 3085 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        16 | 3086 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 3087 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 3088 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        16 | 3089 | `	VmResetResourceIds(&(*pVm));` |
|         - | 3090 | `	/* (5) Unwind any active frames back to none. */` |
|        32 | 3091 | `	while( pVm->pFrame ){` |
|        16 | 3092 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 3093 | `	}` |
|         - | 3094 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        16 | 3095 | `	pVm->bInReset = 0;` |
|         - | 3096 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 3097 | `	 * slots (their indices no longer exist). */` |
|        16 | 3098 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        16 | 3099 | `	SySetReset(&pVm->aFreeObj);` |
|         - | 3100 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        16 | 3101 | `	SyHashRelease(&pVm->hSuper);` |
|        16 | 3102 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|         - | 3103 | `	/* (8) Drain remaining per-exec containers. */` |
|        16 | 3104 | `	SySetReset(&pVm->aSelf);` |
|         - | 3105 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 3106 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 3107 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        16 | 3108 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 3109 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 3110 | `		if( pCB ){` |
|         - | 3111 | `			int iArg;` |
|       ! 0 | 3112 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3113 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 3114 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 3115 | `			}` |
|       ! 0 | 3116 | `		}` |
|       ! 0 | 3117 | `	}` |
|        16 | 3118 | `	SySetReset(&pVm->aShutdown);` |
|         - | 3119 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 3120 | `	 * aborted program can leave entries behind). */` |
|        16 | 3121 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        16 | 3122 | `	SySetReset(&pVm->aException);` |
|        16 | 3123 | `	SySetReset(&pVm->aFinallyAction);` |
|        16 | 3124 | `	pVm->pPendingException = 0;` |
|        16 | 3125 | `	pVm->pInflightException = 0;` |
|        16 | 3126 | `	pVm->nInflightExcBase = 0;` |
|        16 | 3127 | `	pVm->pResumeFrame = 0;` |
|        16 | 3128 | `	pVm->iResumePc = 0;` |
|        16 | 3129 | `	pVm->pResumeInstr = 0;` |
|        16 | 3130 | `	pVm->iResumeStackDepth = 0;` |
|        16 | 3131 | `	pVm->nBoundaryRc = 0;` |
|        16 | 3132 | `	pVm->pConstEvalClass = 0;` |
|        16 | 3133 | `	pVm->nConstEvalDepth = 0;` |
|        16 | 3134 | `	pVm->pConstCycleAttr = 0;` |
|        16 | 3135 | `	pVm->pConstCycleClass = 0;` |
|        16 | 3136 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 3137 | `	{` |
|         - | 3138 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 3139 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        16 | 3140 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        16 | 3141 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 3142 | `		sxu32 iRmw;` |
|        16 | 3143 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 3144 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 3145 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 3146 | `		}` |
|        16 | 3147 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 3148 | `	}` |
|        16 | 3149 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 3150 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 3151 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 3152 | `	}` |
|        16 | 3153 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        16 | 3154 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 3155 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 3156 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 3157 | `	}` |
|        16 | 3158 | `	pVm->pHookSetAttr = 0;` |
|        16 | 3159 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        16 | 3160 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 3161 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 3162 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 3163 | `	}` |
|        16 | 3164 | `	pVm->pMagicCallClass = 0;` |
|        16 | 3165 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        16 | 3166 | `	pVm->nExceptDepth = 0;` |
|         - | 3167 | `	/* spl_autoload_register() callbacks are per request */` |
|        16 | 3168 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 3169 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 3170 | `		if( pCB ){` |
|       ! 0 | 3171 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3172 | `		}` |
|       ! 0 | 3173 | `	}` |
|        16 | 3174 | `	SySetReset(&pVm->aAutoload);` |
|         - | 3175 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 3176 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        16 | 3177 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 3178 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 3179 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 3180 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 3181 | `	}` |
|         - | 3182 | `	/* Output buffers */` |
|        16 | 3183 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 3184 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 3185 | `		if( pOb ){` |
|       ! 0 | 3186 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 3187 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 3188 | `		}` |
|       ! 0 | 3189 | `	}` |
|        16 | 3190 | `	SySetReset(&pVm->aOB);` |
|        16 | 3191 | `	pVm->nObDepth = 0;` |
|         - | 3192 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 3193 | `	{` |
|        16 | 3194 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        16 | 3195 | `		if( rc == SXRET_OK ){` |
|        16 | 3196 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         8 | 3197 | `		}` |
|        16 | 3198 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 3199 | `			return rc;` |
|         - | 3200 | `		}` |
|         - | 3201 | `	}` |
|         - | 3202 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 3203 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 3204 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 3205 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 3206 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 3207 | `	{` |
|         - | 3208 | `		SyHashEntry *pEntry;` |
|        16 | 3209 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      2308 | 3210 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      2292 | 3211 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 3212 | `			ph7_class_attr *pAttr;` |
|         - | 3213 | `			SyHashEntry *pAttrEntry;` |
|      2292 | 3214 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     11970 | 3215 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      8532 | 3216 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      8532 | 3217 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|         4 | 3218 | `					pAttr->nIdx = SXU32_HIGH;` |
|         4 | 3219 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|         2 | 3220 | `				}` |
|       ! 0 | 3221 | `			}` |
|         - | 3222 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 3223 | `			 * slots too so VM reuse re-evaluates them. */` |
|      2292 | 3224 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|      4852 | 3225 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|      2560 | 3226 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      2560 | 3227 | `				pAttr->nIdx = SXU32_HIGH;` |
|      2560 | 3228 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 3229 | `			}` |
|       ! 0 | 3230 | `		}` |
|        16 | 3231 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      2308 | 3232 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      2292 | 3233 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      2292 | 3234 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 3235 | `				return rc;` |
|         - | 3236 | `			}` |
|       ! 0 | 3237 | `		}` |
|         - | 3238 | `	}` |
|         - | 3239 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        16 | 3240 | `	SyBlobReset(&pVm->sConsumer);` |
|        16 | 3241 | `	pVm->nOutputLen = 0;` |
|        16 | 3242 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        16 | 3243 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        16 | 3244 | `	pVm->iResponseStatus = 200;` |
|        16 | 3245 | `	pVm->bHeadersSent = 0;` |
|        16 | 3246 | `	pVm->bHttpContext = 0;` |
|        16 | 3247 | `	VmReinitMemObj(&(*pVm),&pVm->sExceptionCB);` |
|        16 | 3248 | `	VmReinitMemObj(&(*pVm),&pVm->sErrCB);` |
|        16 | 3249 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|        16 | 3250 | `	VmReleaseHandlerStack(&pVm->aExceptionCBSaved);` |
|        16 | 3251 | `	VmReleaseHandlerStack(&pVm->aErrCBSaved);` |
|        16 | 3252 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         - | 3253 | `	/* The session's userland save handler belongs to the request that installed` |
|         - | 3254 | `	 * it; a reused VM (the -S server's) must not route the next request's store` |
|         - | 3255 | `	 * through the previous script's object. */` |
|        16 | 3256 | `	VmReinitMemObj(&(*pVm),&pVm->sSessHandler);` |
|        16 | 3257 | `	pVm->bSessOpened = 0;` |
|        16 | 3258 | `	SyBlobReset(&pVm->sSessData);` |
|        16 | 3259 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 3260 | `#ifdef PH7_ENABLE_PCRE` |
|        16 | 3261 | `	pVm->iPcreLastError = 0;` |
|         - | 3262 | `#endif` |
|         - | 3263 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3264 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        16 | 3265 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 3266 | `#endif` |
|         - | 3267 | `	/* Drop the stream contexts this run created, the default one included: a` |
|         - | 3268 | `	 * reused VM (the -S server's) must not answer the next request from the` |
|         - | 3269 | `	 * previous one's stream_context_set_default(). */` |
|        16 | 3270 | `	PH7_StreamCtxVmReset(&(*pVm));` |
|         - | 3271 | `	/* And every filter INSTANCE it created: a chain that was never removed` |
|         - | 3272 | `	 * still owns memory the next request must not inherit. */` |
|        16 | 3273 | `	PH7_StreamFilterVmReset(&(*pVm));` |
|        16 | 3274 | `	pVm->iCmpCallbackExc = 0;` |
|        16 | 3275 | `	pVm->bHaltRequested = 0;` |
|        16 | 3276 | `	pVm->iExitStatus = 0;` |
|        16 | 3277 | `	pVm->nSpreadCallBase = 0;` |
|        16 | 3278 | `	VmSpreadCaptureReset(pVm);` |
|        16 | 3279 | `	pVm->nRecursionDepth = 0;` |
|        16 | 3280 | `	pVm->pActiveCtx = 0;` |
|        16 | 3281 | `	pVm->pCoalesceObj = 0;` |
|        16 | 3282 | `	pVm->bCoalesceArmed = 0;` |
|        16 | 3283 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 3284 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        16 | 3285 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 3286 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 3287 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        16 | 3288 | `	pVm->nNextObjId = 1;` |
|         - | 3289 | `	/* Set the ready flag */` |
|        16 | 3290 | `	pVm->nMagic = PH7_VM_RUN;` |
|        16 | 3291 | `	return SXRET_OK;` |
|         8 | 3292 | `}` |
|         - | 3293 | `/*` |
|         - | 3294 | ` * Release a Virtual Machine.` |
|         - | 3295 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 3296 | ` */` |
|      4548 | 3297 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 3298 | `{` |
|         - | 3299 | `	/* Set the stale magic number */` |
|      4553 | 3300 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 3301 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3302 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 3303 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      4553 | 3304 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 3305 | `#endif` |
|         - | 3306 | `	/* Release the private memory subsystem */` |
|      4553 | 3307 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      4553 | 3308 | `	return SXRET_OK;` |
|         5 | 3309 | `}` |
|         - | 3310 | `/*` |
|         - | 3311 | ` * Initialize a foreign function call context.` |
|         - | 3312 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 3313 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 3314 | ` * functions.` |
|         - | 3315 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 3316 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 3317 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 3318 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 3319 | ` */` |
|   2866968 | 3320 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 3321 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 3322 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 3323 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 3324 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 3325 | `	sxi32 iFlags          /* Control flags */` |
|         - | 3326 | `	)` |
|         5 | 3327 | `{` |
|   2866973 | 3328 | `	pOut->pFunc = pFunc;` |
|   2866973 | 3329 | `	pOut->pVm   = pVm;` |
|   2866973 | 3330 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   2866973 | 3331 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 3332 | `	/* Assume a null return value */` |
|   2866973 | 3333 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   2866973 | 3334 | `	pOut->pRet = pRet;` |
|   2866973 | 3335 | `	pOut->iFlags = iFlags;` |
|   2866973 | 3336 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   2866973 | 3337 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - | 3338 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - | 3339 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - | 3340 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - | 3341 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|   2866973 | 3342 | `	pOut->pThis = 0;` |
|   2866973 | 3343 | `	pOut->pCalledClass = 0;` |
|   2866973 | 3344 | `	pOut->bThisInit = 0;` |
|   2866973 | 3345 | `	return SXRET_OK;` |
|         5 | 3346 | `}` |
|         - | 3347 | `/*` |
|         - | 3348 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 3349 | ` * left behind.` |
|         - | 3350 | ` */` |
|   2866968 | 3351 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 3352 | `{` |
|         - | 3353 | `	sxu32 n;` |
|   2866973 | 3354 | `	if( pCtx->bThisInit ){` |
|         - | 3355 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - | 3356 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - | 3357 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - | 3358 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|      5589 | 3359 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|      5589 | 3360 | `		pCtx->sThis.x.pOther = 0;` |
|      5589 | 3361 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|      5589 | 3362 | `		pCtx->bThisInit = 0;` |
|      2792 | 3363 | `	}` |
|   2866973 | 3364 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     16493 | 3365 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|     51457 | 3366 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     34969 | 3367 | `			if( apObj[n] == 0 ){` |
|         - | 3368 | `				/* Already released */` |
|      3381 | 3369 | `				continue;` |
|         - | 3370 | `			}` |
|     31593 | 3371 | `			PH7_MemObjRelease(apObj[n]);` |
|     31593 | 3372 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     15799 | 3373 | `		}` |
|     16493 | 3374 | `		SySetRelease(&pCtx->sVar);` |
|      8244 | 3375 | `	}` |
|   2866973 | 3376 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 3377 | `		ph7_aux_data *aAux;` |
|         - | 3378 | `		void *pChunk;` |
|         - | 3379 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 3380 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 3381 | `		 */` |
|      1611 | 3382 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      5265 | 3383 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      3659 | 3384 | `			pChunk = aAux[n].pAuxData;` |
|         - | 3385 | `			/* Release the chunk */` |
|      3659 | 3386 | `			if( pChunk ){` |
|      3651 | 3387 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      1823 | 3388 | `			}` |
|      1832 | 3389 | `		}` |
|      1611 | 3390 | `		SySetRelease(&pCtx->sChunk);` |
|       803 | 3391 | `	}` |
|   2866973 | 3392 | `}` |
|         - | 3393 | `/*` |
|         - | 3394 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 3395 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 3396 | ` */` |
|      3376 | 3397 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 3398 | `	ph7_context *pCtx, /* Call context */` |
|         - | 3399 | `	ph7_value *pValue  /* Release this value */` |
|         - | 3400 | `	)` |
|         5 | 3401 | `{` |
|      3381 | 3402 | `	if( pValue == 0 ){` |
|         - | 3403 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 3404 | `		return;` |
|         - | 3405 | `	}` |
|      3381 | 3406 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      3381 | 3407 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 3408 | `		sxu32 n;` |
|    368659 | 3409 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    368659 | 3410 | `			if( apObj[n] == pValue ){` |
|      3381 | 3411 | `				PH7_MemObjRelease(pValue);` |
|      3381 | 3412 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 3413 | `				/* Mark as released */` |
|      3381 | 3414 | `				apObj[n] = 0;` |
|      3381 | 3415 | `				break;` |
|         - | 3416 | `			}` |
|    182644 | 3417 | `		}` |
|      1688 | 3418 | `	}` |
|      1693 | 3419 | `}` |
|         - | 3420 | `/*` |
|         - | 3421 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 3422 | ` */` |
|  16393716 | 3423 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 3424 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 3425 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 3426 | `	)` |
|         5 | 3427 | `{` |
|  16393721 | 3428 | `	ph7_value *pTos = *ppTos;` |
|  33925539 | 3429 | `	while( nPop > 0 ){` |
|  17531823 | 3430 | `		PH7_MemObjRelease(pTos);` |
|  17531823 | 3431 | `		pTos--;` |
|  17531823 | 3432 | `		nPop--;` |
|         5 | 3433 | `	}` |
|         - | 3434 | `	/* Top of the stack */` |
|  16393721 | 3435 | `	*ppTos = pTos;` |
|  16393721 | 3436 | `}` |
|         - | 3437 | `/*` |
|         - | 3438 | ` * Reserve a memory object.` |
|         - | 3439 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 3440 | ` */` |
|  19215894 | 3441 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 3442 | `{` |
|  19215899 | 3443 | `	ph7_value *pObj = 0;` |
|         - | 3444 | `	VmSlot *pSlot;` |
|         - | 3445 | `	sxu32 nIdx;` |
|         - | 3446 | `	/* Check for a free slot */` |
|  19215899 | 3447 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  19215899 | 3448 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  19215899 | 3449 | `	if( pSlot ){` |
|  16242839 | 3450 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  16242839 | 3451 | `		nIdx = pSlot->nIdx;` |
|   8122731 | 3452 | `	}` |
|  19215899 | 3453 | `	if( pObj == 0 ){` |
|         - | 3454 | `		/* Reserve a new memory object */` |
|   2973065 | 3455 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   2973065 | 3456 | `		if( pObj == 0 ){` |
|       ! 0 | 3457 | `			return 0;` |
|         - | 3458 | `		}` |
|   1486528 | 3459 | `	}` |
|         - | 3460 | `	/* Set a null default value */` |
|  19215899 | 3461 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  19215899 | 3462 | `	pObj->nIdx = nIdx;` |
|  19215899 | 3463 | `	return pObj;` |
|   9609264 | 3464 | `}` |
|         - | 3465 | `/*` |
|         - | 3466 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 3467 | ` */` |
|     62812 | 3468 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 3469 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 3470 | `	const char *zKey,  /* Entry key */` |
|         - | 3471 | `	sxu32 nByte,       /* Key length */` |
|         - | 3472 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 3473 | `	)` |
|         5 | 3474 | `{` |
|         - | 3475 | `	ph7_value sKey;` |
|         - | 3476 | `	sxi32 rc;` |
|     62817 | 3477 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     62817 | 3478 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 3479 | `	/* Perform the insertion */` |
|     62817 | 3480 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     62817 | 3481 | `	PH7_MemObjRelease(&sKey);` |
|     62817 | 3482 | `	return rc;` |
|         5 | 3483 | `}` |
|         - | 3484 | `/*` |
|         - | 3485 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 3486 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 3487 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 3488 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 3489 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 3490 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 3491 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 3492 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 3493 | ` *     superglobal in place.` |
|         - | 3494 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 3495 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|         - | 3496 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|         - | 3497 | ` *     a plain variable.` |
|         - | 3498 | ` */` |
|       186 | 3499 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         5 | 3500 | `{` |
|       191 | 3501 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 3502 | `	SyHashEntry *pEntry;` |
|         - | 3503 | `	ph7_value *pObj;` |
|         - | 3504 | `	char *zDup;` |
|         - | 3505 | `	sxu32 nIdx;` |
|         - | 3506 | `	sxi32 rc;` |
|         - | 3507 | `	/* Walk down to the global frame */` |
|       213 | 3508 | `	while( pFrame->pParent ){` |
|        25 | 3509 | `		pFrame = pFrame->pParent;` |
|         3 | 3510 | `	}` |
|         - | 3511 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       191 | 3512 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|       191 | 3513 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 3514 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 3515 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 3516 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 3517 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 3518 | `		 * of $GLOBALS itself). */` |
|         5 | 3519 | `		pEntry = 0;` |
|         2 | 3520 | `	}` |
|       191 | 3521 | `	if( pEntry == 0 ){` |
|       191 | 3522 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|        93 | 3523 | `	}` |
|       191 | 3524 | `	if( pEntry ){` |
|         5 | 3525 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 3526 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|         - | 3527 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|         3 | 3528 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|         3 | 3529 | `			return SXRET_OK;` |
|         - | 3530 | `		}` |
|         3 | 3531 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 3532 | `		if( pObj == 0 ){` |
|       ! 0 | 3533 | `			return SXERR_NOTFOUND;` |
|         - | 3534 | `		}` |
|         3 | 3535 | `		if( pValue ){` |
|         3 | 3536 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 3537 | `		}else{` |
|       ! 0 | 3538 | `			PH7_MemObjToNull(pObj);` |
|         - | 3539 | `		}` |
|         3 | 3540 | `		return SXRET_OK;` |
|         - | 3541 | `	}` |
|       187 | 3542 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 3543 | `		/* Reserve a fresh slot for the new global */` |
|       185 | 3544 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       185 | 3545 | `		if( pObj == 0 ){` |
|       ! 0 | 3546 | `			return SXERR_MEM;` |
|         - | 3547 | `		}` |
|       185 | 3548 | `		nIdx = pObj->nIdx;` |
|        95 | 3549 | `	}else{` |
|         - | 3550 | `		/* Reference assignment: bind the name to the existing slot */` |
|         3 | 3551 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|         3 | 3552 | `		if( pObj == 0 ){` |
|       ! 0 | 3553 | `			return SXERR_NOTFOUND;` |
|         - | 3554 | `		}` |
|         3 | 3555 | `		nIdx = nRefIdx;` |
|         - | 3556 | `	}` |
|       187 | 3557 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       187 | 3558 | `	if( zDup == 0 ){` |
|       ! 0 | 3559 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3560 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 3561 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|         - | 3562 | `			VmSlot sFree;` |
|       ! 0 | 3563 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3564 | `			sFree.pUserData = 0;` |
|       ! 0 | 3565 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3566 | `		}` |
|       ! 0 | 3567 | `		return SXERR_MEM;` |
|         - | 3568 | `	}` |
|       187 | 3569 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       187 | 3570 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3571 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3572 | `			VmSlot sFree;` |
|       ! 0 | 3573 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3574 | `			sFree.pUserData = 0;` |
|       ! 0 | 3575 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3576 | `		}` |
|       ! 0 | 3577 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 3578 | `		return rc;` |
|         - | 3579 | `	}` |
|         - | 3580 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       187 | 3581 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       187 | 3582 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       187 | 3583 | `	if( nRefIdx == SXU32_HIGH ){` |
|       185 | 3584 | `		pObj->nIdx = nIdx;` |
|       185 | 3585 | `		if( pValue ){` |
|       171 | 3586 | `			PH7_MemObjStore(pValue,pObj);` |
|        84 | 3587 | `		}` |
|        90 | 3588 | `	}` |
|       187 | 3589 | `	return SXRET_OK;` |
|        98 | 3590 | `}` |
|         - | 3591 | `/*` |
|         - | 3592 | ` * Extract a variable value from the top active VM frame.` |
|         - | 3593 | ` * Return a pointer to the variable value on success.` |
|         - | 3594 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 3595 | ` */` |
|  11554413 | 3596 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 3597 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 3598 | `	const SyString *pName, /* Variable name */` |
|         - | 3599 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 3600 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 3601 | `	)` |
|         5 | 3602 | `{` |
|  11554418 | 3603 | `	int bNullify = FALSE;` |
|         - | 3604 | `	SyHashEntry *pEntry;` |
|         - | 3605 | `	VmFrame *pFrame;` |
|         - | 3606 | `	ph7_value *pObj;` |
|         - | 3607 | `	sxu32 nIdx;` |
|         - | 3608 | `	sxi32 rc;` |
|         - | 3609 | `	/* Point to the top active frame */` |
|  11554418 | 3610 | `	pFrame = pVm->pFrame;` |
|  11554418 | 3611 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 3612 | `	/* Perform the lookup */` |
|  11554418 | 3613 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 3614 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        18 | 3615 | `		pName = &sAnnon;` |
|         - | 3616 | `		/* Always nullify the object */` |
|        18 | 3617 | `		bNullify = TRUE;` |
|        18 | 3618 | `		bDup = FALSE;` |
|         8 | 3619 | `	}` |
|         - | 3620 | `	/* Check the superglobals table first */` |
|  11554418 | 3621 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  11554418 | 3622 | `	if( pEntry == 0 ){` |
|         - | 3623 | `		/* Query the top active frame */` |
|  11553710 | 3624 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  11553710 | 3625 | `		if( pEntry == 0 ){` |
|   1158583 | 3626 | `			char *zName = (char *)pName->zString;` |
|         - | 3627 | `			VmSlot sLocal;` |
|   1158583 | 3628 | `			if( !bCreate ){` |
|         - | 3629 | `				/* Do not create the variable,return NULL instead */` |
|      8139 | 3630 | `				return 0;` |
|         - | 3631 | `			}` |
|         - | 3632 | `			/* No such variable,automatically create a new one and install` |
|         - | 3633 | `			 * it in the current frame.` |
|         - | 3634 | `			 */` |
|   1150449 | 3635 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1150449 | 3636 | `			if( pObj == 0 ){` |
|       ! 0 | 3637 | `				return 0;` |
|         - | 3638 | `			}` |
|   1150449 | 3639 | `			nIdx = pObj->nIdx;` |
|   1150449 | 3640 | `			if( bDup ){` |
|         - | 3641 | `				/* Duplicate name */` |
|      3013 | 3642 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      3013 | 3643 | `				if( zName == 0 ){` |
|       ! 0 | 3644 | `					return 0;` |
|         - | 3645 | `				}` |
|      1504 | 3646 | `			}` |
|         - | 3647 | `			/* Link to the top active VM frame */` |
|   1150449 | 3648 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   1150449 | 3649 | `			if( rc != SXRET_OK ){` |
|         - | 3650 | `				/* Return the slot to the free pool */` |
|       ! 0 | 3651 | `				sLocal.nIdx = nIdx;` |
|       ! 0 | 3652 | `				sLocal.pUserData = 0;` |
|       ! 0 | 3653 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|       ! 0 | 3654 | `				return 0;` |
|         - | 3655 | `			}` |
|   1150449 | 3656 | `			if( pFrame->pParent != 0 ){` |
|         - | 3657 | `				/* Local variable */` |
|   1137975 | 3658 | `				sLocal.nIdx = nIdx;` |
|   1137975 | 3659 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    582796 | 3660 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|         - | 3661 | `				/* Register in the $GLOBALS array */` |
|     12341 | 3662 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|      6168 | 3663 | `			}` |
|         - | 3664 | `			/* Install in the reference table */` |
|   1150449 | 3665 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 3666 | `			/* Save object index */` |
|   1150449 | 3667 | `			pObj->nIdx = nIdx;` |
|    576559 | 3668 | `		}else{` |
|         - | 3669 | `			/* Extract variable contents */` |
|  10395132 | 3670 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10395132 | 3671 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  10395132 | 3672 | `			if( bNullify && pObj ){` |
|         3 | 3673 | `				PH7_MemObjRelease(pObj);` |
|         1 | 3674 | `			}` |
|         - | 3675 | `		}` |
|   5780512 | 3676 | `	}else{` |
|         - | 3677 | `		/* Superglobal */` |
|       713 | 3678 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       713 | 3679 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 3680 | `	}` |
|  11546284 | 3681 | `	return pObj;` |
|   5784933 | 3682 | `}` |
|         - | 3683 | `/*` |
|         - | 3684 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 3685 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 3686 | ` */` |
|     41848 | 3687 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 3688 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 3689 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 3690 | `	sxu32 nByte        /* zName length */` |
|         - | 3691 | `	)` |
|         5 | 3692 | `{` |
|         - | 3693 | `	SyHashEntry *pEntry;` |
|         - | 3694 | `	ph7_value *pValue;` |
|         - | 3695 | `	sxu32 nIdx;` |
|         - | 3696 | `	/* Query the superglobal table */` |
|     41853 | 3697 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     41853 | 3698 | `	if( pEntry == 0 ){` |
|         - | 3699 | `		/* No such entry */` |
|       ! 0 | 3700 | `		return 0;` |
|         - | 3701 | `	}` |
|         - | 3702 | `	/* Extract the superglobal index in the global object pool */` |
|     41853 | 3703 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3704 | `	/* Extract the variable value  */` |
|     41853 | 3705 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     41853 | 3706 | `	return pValue;` |
|     20929 | 3707 | `}` |
|         - | 3708 | `/*` |
|         - | 3709 | ` * Perform a raw hashmap insertion.` |
|         - | 3710 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 3711 | ` */` |
|     32650 | 3712 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 3713 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 3714 | `	const char *zKey,   /* Entry key */` |
|         - | 3715 | `	int nKeylen,        /* zKey length*/` |
|         - | 3716 | `	const char *zData,  /* Entry data */` |
|         - | 3717 | `	int nLen            /* zData length */` |
|         - | 3718 | `	)` |
|         5 | 3719 | `{` |
|         - | 3720 | `	ph7_value sKey,sValue;` |
|         - | 3721 | `	sxi32 rc;` |
|     32655 | 3722 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     32655 | 3723 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     32655 | 3724 | `	if( zKey ){` |
|     28063 | 3725 | `		if( nKeylen < 0 ){` |
|     27889 | 3726 | `			nKeylen = (int)SyStrlen(zKey);` |
|     13942 | 3727 | `		}` |
|     28063 | 3728 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     14029 | 3729 | `	}` |
|     32655 | 3730 | `	if( zData ){` |
|     32655 | 3731 | `		if( nLen < 0 ){` |
|         - | 3732 | `			/* Compute length automatically */` |
|     18481 | 3733 | `			nLen = (int)SyStrlen(zData);` |
|      9238 | 3734 | `		}` |
|     32655 | 3735 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     16325 | 3736 | `	}` |
|         - | 3737 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 3738 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 3739 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 3740 | `	 * every argument under "". */` |
|     32655 | 3741 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     32655 | 3742 | `	PH7_MemObjRelease(&sKey);` |
|     32655 | 3743 | `	PH7_MemObjRelease(&sValue);` |
|     32655 | 3744 | `	return rc;` |
|         5 | 3745 | `}` |
|         - | 3746 | `/*` |
|         - | 3747 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 3748 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 3749 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 3750 | ` */` |
|        34 | 3751 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 3752 | `{` |
|        38 | 3753 | `	sxi64 iVal = 0;` |
|        38 | 3754 | `	if( nValue == 0 ){` |
|       ! 0 | 3755 | `		return 0;` |
|         - | 3756 | `	}` |
|        34 | 3757 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 3758 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 3759 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 3760 | `		return 1;` |
|         - | 3761 | `	}` |
|        38 | 3762 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 3763 | `	return iVal != 0;` |
|        21 | 3764 | `}` |
|         - | 3765 | `/*` |
|         - | 3766 | ` * Configure a working virtual machine instance.` |
|         - | 3767 | ` *` |
|         - | 3768 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 3769 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 3770 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 3771 | ` * The second argument to this function is an integer configuration option` |
|         - | 3772 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 3773 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 3774 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 3775 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 3776 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 3777 | ` */` |
|    123862 | 3778 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 3779 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3780 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 3781 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 3782 | `	)` |
|         5 | 3783 | `{` |
|    123867 | 3784 | `	sxi32 rc = SXRET_OK;` |
|    123867 | 3785 | `	switch(nOp){` |
|      2258 | 3786 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      4521 | 3787 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4521 | 3788 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3789 | `		/* VM output consumer callback */` |
|         - | 3790 | `#ifdef UNTRUST` |
|         - | 3791 | `		if( xConsumer == 0 ){` |
|         - | 3792 | `			rc = SXERR_CORRUPT;` |
|         - | 3793 | `			break;` |
|         - | 3794 | `		}` |
|         - | 3795 | `#endif` |
|         - | 3796 | `		/* Install the output consumer */` |
|      4521 | 3797 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      4521 | 3798 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      4521 | 3799 | `		break;` |
|         - | 3800 | `							   }` |
|      2258 | 3801 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      4521 | 3802 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4521 | 3803 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3804 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 3805 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 3806 | `		 * instead of the program-output stream. */` |
|         - | 3807 | `#ifdef UNTRUST` |
|         - | 3808 | `		if( xConsumer == 0 ){` |
|         - | 3809 | `			rc = SXERR_CORRUPT;` |
|         - | 3810 | `			break;` |
|         - | 3811 | `		}` |
|         - | 3812 | `#endif` |
|      4521 | 3813 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      4521 | 3814 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      4521 | 3815 | `		break;` |
|         - | 3816 | `								   }` |
|      2274 | 3817 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 3818 | `		/* Import path */` |
|         - | 3819 | `		  const char *zPath;` |
|         - | 3820 | `		  SyString sPath;` |
|      4553 | 3821 | `		  zPath = va_arg(ap,const char *);` |
|         - | 3822 | `#if defined(UNTRUST)` |
|         - | 3823 | `		  if( zPath == 0 ){` |
|         - | 3824 | `			  rc = SXERR_EMPTY;` |
|         - | 3825 | `			  break;` |
|         - | 3826 | `		  }` |
|         - | 3827 | `#endif` |
|      4553 | 3828 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 3829 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 3830 | `#ifdef __WINNT__` |
|         5 | 3831 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 3832 | `#endif` |
|      9101 | 3833 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 3834 | `		  /* Remove leading and trailing white spaces */` |
|      4553 | 3835 | `		  SyStringFullTrim(&sPath);` |
|      4553 | 3836 | `		  if( sPath.nByte > 0 ){` |
|         - | 3837 | `			  /* Store the path in the corresponding conatiner */` |
|      4553 | 3838 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      2274 | 3839 | `		  }` |
|      4553 | 3840 | `		  break;` |
|         - | 3841 | `									 }` |
|      2281 | 3842 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 3843 | `		/* Run-Time Error report */` |
|      4567 | 3844 | `		pVm->bErrReport = 1;` |
|      4567 | 3845 | `		pVm->iErrMask = PH7_E_ALL_MASK; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      4567 | 3846 | `		break;` |
|         2 | 3847 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 3848 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 3849 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 3850 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 3851 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 3852 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 3853 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 3854 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 3855 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 3856 | `		int nDepth = va_arg(ap,int);` |
|         5 | 3857 | `		if( nDepth >= 0 ){` |
|         5 | 3858 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 3859 | `		}` |
|         5 | 3860 | `		break;` |
|         - | 3861 | `									   }` |
|         5 | 3862 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 3863 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 3864 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 3865 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 3866 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 3867 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 3868 | `		 * so it is rejected as a footgun). */` |
|        11 | 3869 | `		int nDepth = va_arg(ap,int);` |
|        11 | 3870 | `		if( nDepth > 1 ){` |
|        11 | 3871 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 3872 | `		}` |
|        11 | 3873 | `		break;` |
|         - | 3874 | `									   }` |
|       ! 0 | 3875 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 3876 | `		/* VM output length in bytes */` |
|       ! 0 | 3877 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 3878 | `#ifdef UNTRUST` |
|         - | 3879 | `		if( pOut == 0 ){` |
|         - | 3880 | `			rc = SXERR_CORRUPT;` |
|         - | 3881 | `			break;` |
|         - | 3882 | `		}` |
|         - | 3883 | `#endif` |
|       ! 0 | 3884 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 3885 | `		break;` |
|         - | 3886 | `							   }` |
|         - | 3887 |  |
|     25098 | 3888 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 3889 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 3890 | `		/* Create a new superglobal/global variable */` |
|     50201 | 3891 | `		const char *zName = va_arg(ap,const char *);` |
|     50201 | 3892 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 3893 | `		SyHashEntry *pEntry;` |
|         - | 3894 | `		ph7_value *pObj;` |
|         - | 3895 | `		sxu32 nByte;` |
|         - | 3896 | `		sxu32 nIdx;` |
|         - | 3897 | `#ifdef UNTRUST` |
|         - | 3898 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 3899 | `			rc = SXERR_CORRUPT;` |
|         - | 3900 | `			break;` |
|         - | 3901 | `		}` |
|         - | 3902 | `#endif` |
|     50201 | 3903 | `		nByte = SyStrlen(zName);` |
|     50201 | 3904 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3905 | `			/* Check if the superglobal is already installed */` |
|     45685 | 3906 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     22845 | 3907 | `		}else{` |
|         - | 3908 | `			/* Query the top active VM frame */` |
|      4521 | 3909 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 3910 | `		}` |
|     50201 | 3911 | `		if( pEntry ){` |
|         - | 3912 | `			/* Variable already installed */` |
|       ! 0 | 3913 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3914 | `			/* Extract contents */` |
|       ! 0 | 3915 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 3916 | `			if( pObj ){` |
|         - | 3917 | `				/* Overwrite old contents */` |
|       ! 0 | 3918 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 3919 | `			}` |
|       ! 0 | 3920 | `		}else{` |
|         - | 3921 | `			/* Install a new variable */` |
|     50201 | 3922 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     50201 | 3923 | `			if( pObj == 0 ){` |
|       ! 0 | 3924 | `				rc = SXERR_MEM;` |
|       ! 0 | 3925 | `				break;` |
|         - | 3926 | `			}` |
|     50201 | 3927 | `			nIdx = pObj->nIdx;` |
|         - | 3928 | `			/* Copy value */` |
|     50201 | 3929 | `			PH7_MemObjStore(pValue,pObj);` |
|     50201 | 3930 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3931 | `				/* Install the superglobal */` |
|     45685 | 3932 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     22845 | 3933 | `			}else{` |
|         - | 3934 | `				/* Install in the current frame */` |
|      4521 | 3935 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 3936 | `			}` |
|     50201 | 3937 | `			if( rc == SXRET_OK ){` |
|         - | 3938 | `				SyHashEntry *pRef;` |
|     50201 | 3939 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     45685 | 3940 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     22845 | 3941 | `				}else{` |
|      4521 | 3942 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 3943 | `				}` |
|         - | 3944 | `				/* Install in the reference table */` |
|     50201 | 3945 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     50201 | 3946 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 3947 | `					/* Register in the $GLOBALS array */` |
|     50201 | 3948 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     25098 | 3949 | `				}` |
|     25098 | 3950 | `			}` |
|         - | 3951 | `		}` |
|     50201 | 3952 | `		break;` |
|         - | 3953 | `									}` |
|     13942 | 3954 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 3955 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 3956 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 3957 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 3958 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 3959 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 3960 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     27889 | 3961 | `		const char *zKey   = va_arg(ap,const char *);` |
|     27889 | 3962 | `		const char *zValue = va_arg(ap,const char *);` |
|     27889 | 3963 | `		int nLen = va_arg(ap,int);` |
|         - | 3964 | `		ph7_hashmap *pMap;` |
|         - | 3965 | `		ph7_value *pValue;` |
|     27889 | 3966 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 3967 | `			/* Extract the $_ENV superglobal */` |
|       ! 0 | 3968 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     27889 | 3969 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 3970 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 3971 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     27889 | 3972 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 3973 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 3974 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     27889 | 3975 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 3976 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 3977 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     27889 | 3978 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 3979 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 3980 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     27889 | 3981 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 3982 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 3983 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 3984 | `		}else{` |
|         - | 3985 | `			/* Extract the $_SERVER superglobal */` |
|     27889 | 3986 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 3987 | `		}` |
|     27889 | 3988 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 3989 | `			/* No such entry */` |
|       ! 0 | 3990 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 3991 | `			break;` |
|         - | 3992 | `		}` |
|         - | 3993 | `		/* Point to the hashmap */` |
|     27889 | 3994 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 3995 | `		/* Perform the insertion */` |
|     27889 | 3996 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     27889 | 3997 | `		break;` |
|         - | 3998 | `								   }` |
|      2296 | 3999 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 4000 | `		/* Script arguments */` |
|      4597 | 4001 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 4002 | `		ph7_hashmap *pMap;` |
|         - | 4003 | `		ph7_value *pValue;` |
|         - | 4004 | `		sxu32 n;` |
|         - | 4005 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 4006 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 4007 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 4008 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      4597 | 4009 | `		if( zValue == 0 ){` |
|       ! 0 | 4010 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4011 | `			break;` |
|         - | 4012 | `		}` |
|         - | 4013 | `		/* Extract the $argv array */` |
|      4597 | 4014 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4597 | 4015 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 4016 | `			/* No such entry */` |
|       ! 0 | 4017 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4018 | `			break;` |
|         - | 4019 | `		}` |
|         - | 4020 | `		/* Point to the hashmap */` |
|      4597 | 4021 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 4022 | `		/* Perform the insertion */` |
|      4597 | 4023 | `		n = (sxu32)SyStrlen(zValue);` |
|      4597 | 4024 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      4597 | 4025 | `		break;` |
|         - | 4026 | `								  }` |
|      2258 | 4027 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 4028 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 4029 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 4030 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 4031 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 4032 | `		ph7_value *pArgv,*pServer;` |
|         - | 4033 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 4034 | `		ph7_value sArgvVal,sKey,sCount;` |
|      4521 | 4035 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4521 | 4036 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      4516 | 4037 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      4521 | 4038 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 4039 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4040 | `			break;` |
|         - | 4041 | `		}` |
|      4521 | 4042 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      4521 | 4043 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 4044 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      4521 | 4045 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      4521 | 4046 | `		if( pDup == 0 ){` |
|       ! 0 | 4047 | `			rc = SXERR_MEM;` |
|       ! 0 | 4048 | `			break;` |
|         - | 4049 | `		}` |
|      4521 | 4050 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      4521 | 4051 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      4521 | 4052 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4521 | 4053 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      4521 | 4054 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      4521 | 4055 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      4521 | 4056 | `		PH7_MemObjRelease(&sKey);` |
|         - | 4057 | `		/* $_SERVER['argc'] = count($argv). */` |
|      4521 | 4058 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      4521 | 4059 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4521 | 4060 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      4521 | 4061 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      4521 | 4062 | `		PH7_MemObjRelease(&sCount);` |
|      4521 | 4063 | `		PH7_MemObjRelease(&sKey);` |
|      4521 | 4064 | `		rc = SXRET_OK;` |
|      4521 | 4065 | `		break;` |
|         - | 4066 | `								  }` |
|        53 | 4067 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 4068 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 4069 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 4070 | `		 * apply immediately so they take effect even if the script never` |
|         - | 4071 | `		 * touches the INI API. */` |
|       110 | 4072 | `		const char *zName = va_arg(ap,const char *);` |
|       110 | 4073 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 4074 | `		VmIniEntry sEntry;` |
|         - | 4075 | `		char *zDupN,*zDupV;` |
|         - | 4076 | `		sxu32 nName,nValue;` |
|       110 | 4077 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 4078 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4079 | `			break;` |
|         - | 4080 | `		}` |
|       110 | 4081 | `		if( zValue == 0 ){` |
|       ! 0 | 4082 | `			zValue = "";` |
|       ! 0 | 4083 | `		}` |
|       110 | 4084 | `		nName = (sxu32)SyStrlen(zName);` |
|       110 | 4085 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       110 | 4086 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       110 | 4087 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       110 | 4088 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 4089 | `			rc = SXERR_MEM;` |
|       ! 0 | 4090 | `			break;` |
|         - | 4091 | `		}` |
|       110 | 4092 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       110 | 4093 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       110 | 4094 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       110 | 4095 | `		if( rc == SXRET_OK ){` |
|       106 | 4096 | `			if( nName == sizeof("error_reporting")-1` |
|        78 | 4097 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         4 | 4098 | `				sxi64 iLevel = 0;` |
|         4 | 4099 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         4 | 4100 | `				pVm->bErrReport = iLevel != 0;` |
|       104 | 4101 | `			}else if( nName == sizeof("date.timezone")-1` |
|        51 | 4102 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|       ! 0 | 4103 | `			 && nValue == 3` |
|         4 | 4104 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 4105 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 4106 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 4107 | `				pVm->nDefTz = 3;` |
|       102 | 4108 | `			}else if( nName == sizeof("zend.assertions")-1` |
|        92 | 4109 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 4110 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 4111 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 4112 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 4113 | `				sxi64 iZend = 0;` |
|        40 | 4114 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 4115 | `				if( iZend >= 1 ){` |
|        40 | 4116 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 4117 | `				}else{` |
|       ! 0 | 4118 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 4119 | `				}` |
|        88 | 4120 | `			}else if( nName == sizeof("display_errors")-1` |
|        46 | 4121 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 4122 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 4123 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 4124 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 4125 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        61 | 4126 | `			}else if( nName == sizeof("log_errors")-1` |
|        36 | 4127 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 4128 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|        44 | 4129 | `			}else if( nName == sizeof("include_path")-1` |
|        26 | 4130 | `			 && SyMemcmp(zName,"include_path",nName) == 0` |
|        17 | 4131 | `			 && nValue > 0 ){` |
|         - | 4132 | `				/* The path SET is the store this directive names, and the INI` |
|         - | 4133 | ``				 * chunk's seed is lazy -- so `-d include_path=…` has to reach it`` |
|         - | 4134 | `				 * here or a script that never touches the INI API keeps looking` |
|         - | 4135 | `				 * in the default directory. Empty is refused, as php's` |
|         - | 4136 | `				 * OnUpdateStringUnempty refuses it. */` |
|        10 | 4137 | `				PH7_VmSetIncludePath(pVm,zValue,nValue);` |
|         4 | 4138 | `			}` |
|        53 | 4139 | `		}` |
|       110 | 4140 | `		break;` |
|         - | 4141 | `								  }` |
|       ! 0 | 4142 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 4143 | `		/* error_log() consumer */` |
|       ! 0 | 4144 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 4145 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 4146 | `		break;` |
|         - | 4147 | `										}` |
|       ! 0 | 4148 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 4149 | `		/* Script return value */` |
|       ! 0 | 4150 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 4151 | `#ifdef UNTRUST` |
|         - | 4152 | `		if( ppValue == 0 ){` |
|         - | 4153 | `			rc = SXERR_CORRUPT;` |
|         - | 4154 | `			break;` |
|         - | 4155 | `		}` |
|         - | 4156 | `#endif` |
|       ! 0 | 4157 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 4158 | `		break;` |
|         - | 4159 | `								   }` |
|      9114 | 4160 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 4161 | `		/* Register an IO stream device */` |
|     18233 | 4162 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 4163 | `		/* Make sure we are dealing with a valid IO stream */` |
|     18228 | 4164 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     18233 | 4165 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|         - | 4166 | `				/* Invalid stream */` |
|       ! 0 | 4167 | `				rc = SXERR_INVALID;` |
|       ! 0 | 4168 | `				break;` |
|         - | 4169 | `		}` |
|     18233 | 4170 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 4171 | `			/* Make the 'file://' stream the defaut stream device */` |
|      4557 | 4172 | `			pVm->pDefStream = pStream;` |
|      2276 | 4173 | `		}` |
|         - | 4174 | `		/* Insert in the appropriate container */` |
|     18233 | 4175 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     18233 | 4176 | `		break;` |
|         - | 4177 | `								  }` |
|        23 | 4178 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 4179 | `		/* Point to the VM internal output consumer buffer */` |
|        46 | 4180 | `		const void **ppOut = va_arg(ap,const void **);` |
|        46 | 4181 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 4182 | `#ifdef UNTRUST` |
|         - | 4183 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 4184 | `			rc = SXERR_CORRUPT;` |
|         - | 4185 | `			break;` |
|         - | 4186 | `		}` |
|         - | 4187 | `#endif` |
|        46 | 4188 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        46 | 4189 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        46 | 4190 | `		break;` |
|         - | 4191 | `									   }` |
|        23 | 4192 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 4193 | `		/* Raw HTTP request*/` |
|        46 | 4194 | `		const char *zRequest = va_arg(ap,const char *);` |
|        46 | 4195 | `		int nByte = va_arg(ap,int);` |
|        46 | 4196 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 4197 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4198 | `			break;` |
|         - | 4199 | `		}` |
|        46 | 4200 | `		if( nByte < 0 ){` |
|         - | 4201 | `			/* Compute length automatically */` |
|       ! 0 | 4202 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 4203 | `		}` |
|         - | 4204 | `		/* Process the request */` |
|        46 | 4205 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 4206 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        46 | 4207 | `		if( rc == SXRET_OK ){` |
|        44 | 4208 | `			pVm->bHttpContext = 1;` |
|        22 | 4209 | `		}` |
|        46 | 4210 | `		break;` |
|         - | 4211 | `									}` |
|        23 | 4212 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 4213 | `		/* Extract HTTP response status code */` |
|        46 | 4214 | `		int *pStatus = va_arg(ap, int *);` |
|        46 | 4215 | `		if( pStatus ){` |
|        46 | 4216 | `			*pStatus = pVm->iResponseStatus;` |
|        23 | 4217 | `		}` |
|        46 | 4218 | `		break;` |
|         - | 4219 | `										}` |
|        23 | 4220 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 4221 | `		/* Iterate response headers via callback */` |
|         - | 4222 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        46 | 4223 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        46 | 4224 | `		void *pUserData = va_arg(ap, void *);` |
|        46 | 4225 | `		if( xCallback ){` |
|        46 | 4226 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        46 | 4227 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       112 | 4228 | `			for( k = 0; k < nHdr; k++ ){` |
|        99 | 4229 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        66 | 4230 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        33 | 4231 | `							   pUserData);` |
|        66 | 4232 | `				if( rc != PH7_OK ){` |
|       ! 0 | 4233 | `					break;` |
|         - | 4234 | `				}` |
|        33 | 4235 | `			}` |
|        23 | 4236 | `		}` |
|        46 | 4237 | `		break;` |
|         - | 4238 | `										 }` |
|       ! 0 | 4239 | `	default:` |
|         - | 4240 | `		/* Unknown configuration option */` |
|       ! 0 | 4241 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 4242 | `		break;` |
|         - | 4243 | `	}` |
|    123867 | 4244 | `	return rc;` |
|         5 | 4245 | `}` |
|         - | 4246 | `/* Forward declaration */` |
|         - | 4247 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 4248 | `/*` |
|         - | 4249 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 4250 | ` * format.` |
|         - | 4251 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 4252 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 4253 | ` * (STDOUT).` |
|         - | 4254 | ` */` |
|         2 | 4255 | `static sxi32 VmByteCodeDump(` |
|         - | 4256 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 4257 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 4258 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 4259 | `	)` |
|         1 | 4260 | `{` |
|         - | 4261 | `	static const char zDump[] = {` |
|         - | 4262 | `		"====================================================\n"` |
|         - | 4263 | `		"PH7 VM Dump\n"` |
|         - | 4264 | `		"====================================================\n"` |
|         - | 4265 | `	};` |
|         - | 4266 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 4267 | `	sxi32 rc = SXRET_OK;` |
|         - | 4268 | `	sxu32 n;` |
|         - | 4269 | `	/* Point to the PH7 instructions */` |
|         3 | 4270 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 4271 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 4272 | `	n = 0;` |
|         3 | 4273 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 4274 | `	/* Dump instructions */` |
|         6 | 4275 | `	for(;;){` |
|        13 | 4276 | `		if( pInstr >= pEnd ){` |
|         - | 4277 | `			/* No more instructions */` |
|         3 | 4278 | `			break;` |
|         - | 4279 | `		}` |
|         - | 4280 | `		/* Format and call the consumer callback */` |
|        16 | 4281 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 | 4282 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 | 4283 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 | 4284 | `		if( rc != SXRET_OK ){` |
|         - | 4285 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 4286 | `			return rc;` |
|         - | 4287 | `		}` |
|        11 | 4288 | `		++n;` |
|        11 | 4289 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 4290 | `	}` |
|         3 | 4291 | `	return rc;` |
|         2 | 4292 | `}` |
|         - | 4293 | `/*` |
|         - | 4294 | ` * Save the execution state of a fiber/generator context.` |
|         - | 4295 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 4296 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 4297 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 4298 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 4299 | ` * when VmByteCodeExec returns.` |
|         - | 4300 | ` */` |
|      1684 | 4301 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 4302 | `	ph7_vm *pVm,` |
|         - | 4303 | `	ph7_exec_ctx *pCtx,` |
|         - | 4304 | `	sxi32 pc,` |
|         - | 4305 | `	sxi32 nTos` |
|         - | 4306 | `	)` |
|         5 | 4307 | `{` |
|       842 | 4308 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1689 | 4309 | `	pCtx->pc = pc;` |
|      1689 | 4310 | `	pCtx->nTos = nTos;` |
|      1689 | 4311 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1689 | 4312 | `	return PH7_SUSPEND;` |
|         5 | 4313 | `}` |
|         - | 4314 | `/*` |
|         - | 4315 | ` * Resolve named-argument mapping.` |
|         - | 4316 | ` *` |
|         - | 4317 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 4318 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 4319 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 4320 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 4321 | ` * every formal parameter that received a value.` |
|         - | 4322 | ` *` |
|         - | 4323 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 4324 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 4325 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 4326 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 4327 | ` */` |
|       392 | 4328 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 4329 | `	ph7_vm *pVm,` |
|         - | 4330 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 4331 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 4332 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 4333 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 4334 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 4335 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 4336 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 4337 | `)` |
|         5 | 4338 | `{` |
|       397 | 4339 | `	sxi32 posIdx = 0;` |
|         - | 4340 | `	sxu32 i;` |
|       397 | 4341 | `	int bSeenNamed = 0;` |
|         - | 4342 | `	char zErrMsg[256];` |
|       397 | 4343 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1355 | 4344 | `	for( i = 0; i < nActual; i++ ){` |
|       963 | 4345 | `		aSlot[i] = -2;` |
|       484 | 4346 | `	}` |
|      1343 | 4347 | `	for( i = 0; i < nActual; i++ ){` |
|      1254 | 4348 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 4349 | `			/* Named argument — find formal by name */` |
|       599 | 4350 | `			int found = 0;` |
|       599 | 4351 | `			bSeenNamed = 1;` |
|         - | 4352 | `			sxu32 k;` |
|       903 | 4353 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       734 | 4354 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       714 | 4355 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       684 | 4356 | `						pMap->aNames[i].zString,` |
|      1026 | 4357 | `						pMap->aNames[i].nByte) == 0 ){` |
|       435 | 4358 | `					if( aUsed[k] ){` |
|        12 | 4359 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4360 | `							"Named parameter $%.*s overwrites previous argument",` |
|         6 | 4361 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         9 | 4362 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4363 | `					}` |
|       428 | 4364 | `					aSlot[i] = (sxi32)k;` |
|       428 | 4365 | `					aUsed[k] = 1;` |
|       428 | 4366 | `					found = 1;` |
|       428 | 4367 | `					break;` |
|         - | 4368 | `				}` |
|       157 | 4369 | `			}` |
|       593 | 4370 | `			if( !found ){` |
|       169 | 4371 | `				if( iVariadicIdx >= 0 ){` |
|       163 | 4372 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        84 | 4373 | `				}else{` |
|        11 | 4374 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4375 | `						"Unknown named parameter $%.*s",` |
|         6 | 4376 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         8 | 4377 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4378 | `				}` |
|        79 | 4379 | `			}` |
|       296 | 4380 | `		}else{` |
|         - | 4381 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 4382 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 4383 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 4384 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       368 | 4385 | `			if( bSeenNamed ){` |
|       ! 0 | 4386 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 4387 | `					"Cannot use positional argument after named argument",` |
|         - | 4388 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 4389 | `			}` |
|       368 | 4390 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        62 | 4391 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 4392 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4393 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 4394 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 4395 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4396 | `				}` |
|        62 | 4397 | `				aSlot[i] = posIdx;` |
|        62 | 4398 | `				aUsed[posIdx] = 1;` |
|       338 | 4399 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 4400 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 4401 | `			}` |
|       368 | 4402 | `			posIdx++;` |
|         - | 4403 | `		}` |
|       478 | 4404 | `	}` |
|       385 | 4405 | `	return SXRET_OK;` |
|       201 | 4406 | `}` |
|         - | 4407 | `/*` |
|         - | 4408 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 4409 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 4410 | ` */` |
|       634 | 4411 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         4 | 4412 | `{` |
|       638 | 4413 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       626 | 4414 | `		return 0;` |
|         - | 4415 | `	}` |
|        15 | 4416 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       321 | 4417 | `}` |
|         - | 4418 | `/*` |
|         - | 4419 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 4420 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 4421 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 4422 | ` */` |
|        10 | 4423 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4424 | `{` |
|        11 | 4425 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|         5 | 4426 | `	(void)pVm;` |
|        11 | 4427 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|        11 | 4428 | `	return SXRET_OK;` |
|         1 | 4429 | `}` |
|         - | 4430 | `/*` |
|         - | 4431 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 4432 | ` * collect values positionally (keys ignored) into a temp array.` |
|         - | 4433 | ` */` |
|         6 | 4434 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4435 | `{` |
|         3 | 4436 | `	(void)pVm; (void)pKey;` |
|         7 | 4437 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|         7 | 4438 | `	return SXRET_OK;` |
|         1 | 4439 | `}` |
|         - | 4440 | `/*` |
|         - | 4441 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 4442 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 4443 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 4444 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 4445 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 4446 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 4447 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 4448 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 4449 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 4450 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 4451 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 4452 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 4453 | ` */` |
|         - | 4454 | `/*` |
|         - | 4455 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 4456 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 4457 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 4458 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 4459 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 4460 | ` */` |
|       612 | 4461 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         4 | 4462 | `{` |
|         - | 4463 | `	VmSpreadRun sRun;` |
|         - | 4464 | `	ph7_hashmap_node *pNode;` |
|         - | 4465 | `	sxu32 i;` |
|       616 | 4466 | `	sRun.pStart = pFirst;` |
|       616 | 4467 | `	sRun.nCount = nCount;` |
|       616 | 4468 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|       616 | 4469 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       616 | 4470 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 4471 | `		return;` |
|         - | 4472 | `	}` |
|       616 | 4473 | `	pNode = pMap->pFirst;` |
|      2870 | 4474 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 4475 | `		VmSpreadKey sKey;` |
|      2258 | 4476 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 4477 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 4478 | `			 * the source map's release before CALL replays them. */` |
|       101 | 4479 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       101 | 4480 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       101 | 4481 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        51 | 4482 | `		}else{` |
|         - | 4483 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      2158 | 4484 | `			sKey.nOff = 0;` |
|      2158 | 4485 | `			sKey.nLen = 0;` |
|         - | 4486 | `		}` |
|      2258 | 4487 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      2258 | 4488 | `		pNode = pNode->pPrev; /* forward link */` |
|      1131 | 4489 | `	}` |
|       310 | 4490 | `}` |
|         - | 4491 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 4492 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        16 | 4493 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 4494 | `{` |
|        16 | 4495 | `	SySetReset(&pVm->aSpreadRun);` |
|        16 | 4496 | `	SySetReset(&pVm->aSpreadKey);` |
|        16 | 4497 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        16 | 4498 | `}` |
|         - | 4499 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 4500 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 4501 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 4502 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 4503 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 4504 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 4505 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 4506 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 4507 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 4508 | ` * slot — from being consumed by that nested call. */` |
|      1112 | 4509 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         4 | 4510 | `{` |
|      1116 | 4511 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      1116 | 4512 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 4513 | `	VmSpreadRun *aRun;` |
|      1116 | 4514 | `	if( rStart >= nRun ){` |
|       518 | 4515 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 4516 | `	}` |
|       602 | 4517 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       602 | 4518 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|       602 | 4519 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|       602 | 4520 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       299 | 4521 | `	}` |
|       602 | 4522 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|       560 | 4523 | `}` |
|         - | 4524 | `/*` |
|         - | 4525 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 4526 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 4527 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 4528 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 4529 | ` *` |
|         - | 4530 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 4531 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 4532 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 4533 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 4534 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 4535 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 4536 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 4537 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 4538 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 4539 | ` *` |
|         - | 4540 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 4541 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 4542 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 4543 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 4544 | ` */` |
|      1232 | 4545 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         4 | 4546 | `{` |
|      1236 | 4547 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4548 | `	VmSpreadRun *aRun;` |
|      1236 | 4549 | `	ph7_value *pEnd = pTos;` |
|      1236 | 4550 | `	sxi32 nPos = iP1;` |
|      1236 | 4551 | `	sxi32 ri, extra = 0;` |
|      1236 | 4552 | `	if( nRun == 0 ){` |
|        15 | 4553 | `		pVm->nSpreadCallBase = 0;` |
|        15 | 4554 | `		return 0;` |
|         - | 4555 | `	}` |
|      1222 | 4556 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1222 | 4557 | `	ri = (sxi32)nRun - 1;` |
|      3166 | 4558 | `	while( nPos > 0 ){` |
|      1948 | 4559 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 4560 | `			/* A non-empty unpack occupying nCount slots. */` |
|      1096 | 4561 | `			pEnd = aRun[ri].pStart;` |
|      1096 | 4562 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      1096 | 4563 | `			ri--;` |
|      1402 | 4564 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 4565 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       135 | 4566 | `			extra -= 1;` |
|       135 | 4567 | `			ri--;` |
|        69 | 4568 | `		}else{` |
|         - | 4569 | `			/* An ordinary single-slot argument. */` |
|       724 | 4570 | `			pEnd--;` |
|         - | 4571 | `		}` |
|      1948 | 4572 | `		nPos--;` |
|         4 | 4573 | `	}` |
|         - | 4574 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 4575 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      1222 | 4576 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      1222 | 4577 | `	return extra;` |
|       620 | 4578 | `}` |
|       612 | 4579 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|         4 | 4580 | `{` |
|       616 | 4581 | `	ph7_value *pTos = *ppTos;` |
|       616 | 4582 | `	sxu32 nEntry = pMap->nEntry;` |
|       616 | 4583 | `	if( nEntry == 0 ){` |
|         - | 4584 | `		/* Nothing to unpack — remove the source from the stack */` |
|        69 | 4585 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|        69 | 4586 | `		VmPopOperand(&pTos, 1);` |
|        36 | 4587 | `	}else{` |
|         - | 4588 | `		ph7_hashmap_node *pNode;` |
|         - | 4589 | `		ph7_value *pElem;` |
|         - | 4590 | `		sxu32 i;` |
|         - | 4591 | `		int bTemp;` |
|         - | 4592 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|         - | 4593 | `		 * so a source array that other variables share has to separate first — otherwise` |
|         - | 4594 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|         - | 4595 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|         - | 4596 | `		 * ordinary spread pays nothing for this. */` |
|       546 | 4597 | `		if( bVarSource` |
|       467 | 4598 | `		 && pMap != pVm->pGlobal` |
|       388 | 4599 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|       392 | 4600 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|         - | 4601 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|         - | 4602 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|         - | 4603 | `			 * object. */` |
|       392 | 4604 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|       392 | 4605 | `			if( pSep ){` |
|       392 | 4606 | `				pMap = pSep;` |
|       392 | 4607 | `				nEntry = pMap->nEntry;` |
|       194 | 4608 | `			}` |
|       194 | 4609 | `		}` |
|       550 | 4610 | `		pMap->iRef++;` |
|       550 | 4611 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 4612 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 4613 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|       550 | 4614 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 4615 | `		/* Overwrite the source slot with the first element */` |
|       550 | 4616 | `		pNode = pMap->pFirst;` |
|       550 | 4617 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|       550 | 4618 | `		PH7_MemObjRelease(pTos);` |
|       550 | 4619 | `		if( pElem ){` |
|       550 | 4620 | `			if( bTemp ){` |
|       152 | 4621 | `				PH7_MemObjStore(pElem, pTos);` |
|        77 | 4622 | `			}else{` |
|       400 | 4623 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 4624 | `			}` |
|       273 | 4625 | `		}` |
|         - | 4626 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|         - | 4627 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|         - | 4628 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|         - | 4629 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|         - | 4630 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|         - | 4631 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|         - | 4632 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|         - | 4633 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|       550 | 4634 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|       550 | 4635 | `		if( !bVarSource \|\| bTemp ){` |
|       160 | 4636 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|        79 | 4637 | `		}` |
|         - | 4638 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 4639 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|       550 | 4640 | `		pNode = pNode->pPrev;` |
|         - | 4641 | `		/* Push the remaining elements */` |
|      2258 | 4642 | `		for( i = 1; i < nEntry; i++ ){` |
|      1712 | 4643 | `			pTos++;` |
|      1712 | 4644 | `			PH7_MemObjInit(pVm, pTos);` |
|      1712 | 4645 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1712 | 4646 | `			if( pElem ){` |
|      1712 | 4647 | `				if( bTemp ){` |
|      1289 | 4648 | `					PH7_MemObjStore(pElem, pTos);` |
|       645 | 4649 | `				}else{` |
|       424 | 4650 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 4651 | `				}` |
|       854 | 4652 | `			}` |
|      1712 | 4653 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|      1712 | 4654 | `			if( !bVarSource \|\| bTemp ){` |
|      1289 | 4655 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       644 | 4656 | `			}` |
|      1712 | 4657 | `			pNode = pNode->pPrev;` |
|       858 | 4658 | `		}` |
|       550 | 4659 | `		PH7_HashmapUnref(pMap);` |
|         - | 4660 | `	}` |
|       616 | 4661 | `	*ppTos = pTos;` |
|       616 | 4662 | `}` |
|         - | 4663 | `/*` |
|         - | 4664 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 4665 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 4666 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 4667 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 4668 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 4669 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 4670 | ` *` |
|         - | 4671 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 4672 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 4673 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 4674 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 4675 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 4676 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 4677 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 4678 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 4679 | ` * method-name slot pop shifts pArg).` |
|         - | 4680 | ` *` |
|         - | 4681 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 4682 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 4683 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 4684 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 4685 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 4686 | ` */` |
|       576 | 4687 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 4688 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         4 | 4689 | `{` |
|       580 | 4690 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4691 | `	VmSpreadRun *aRun;` |
|         - | 4692 | `	VmSpreadKey *aKey;` |
|         - | 4693 | `	const char *zKeyBase;` |
|       580 | 4694 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|       580 | 4695 | `	int bAnyNamed = 0;` |
|         - | 4696 | `	sxu32 ai, ci, ri, rStart;` |
|       580 | 4697 | `	if( nRun == 0 ){` |
|         - | 4698 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 4699 | `		return 0;` |
|         - | 4700 | `	}` |
|       580 | 4701 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       580 | 4702 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|       580 | 4703 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 4704 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 4705 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 4706 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 4707 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|       580 | 4708 | `	ri = pVm->nSpreadCallBase;` |
|       580 | 4709 | `	rStart = ri;` |
|       580 | 4710 | `	if( rStart >= nRun ){` |
|         - | 4711 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 4712 | `		return 0;` |
|         - | 4713 | `	}` |
|       580 | 4714 | `	SySetReset(&pVm->aEffArgName);` |
|       580 | 4715 | `	ci = 0;` |
|       580 | 4716 | `	ai = 0;` |
|      1472 | 4717 | `	while( ai < nActual ){` |
|         - | 4718 | `		SyString sName;` |
|       896 | 4719 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 4720 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 4721 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|       908 | 4722 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 4723 | `			ci++; ri++;` |
|         1 | 4724 | `		}` |
|       896 | 4725 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 4726 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 4727 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 4728 | `			 * run never desyncs the key stream. */` |
|       550 | 4729 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      2804 | 4730 | `			for( j = 0; j < K; j++ ){` |
|      2258 | 4731 | `				SyZero(&sName, sizeof(sName));` |
|      2258 | 4732 | `				if( aKey[ks + j].nLen > 0 ){` |
|       101 | 4733 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       101 | 4734 | `					bAnyNamed = 1;` |
|        50 | 4735 | `				}` |
|      2258 | 4736 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1131 | 4737 | `			}` |
|       550 | 4738 | `			ai += K;` |
|       550 | 4739 | `			ci++; ri++;` |
|       277 | 4740 | `		}else{` |
|         - | 4741 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|       350 | 4742 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        35 | 4743 | `				sName = pCompile->aNames[ci];` |
|        35 | 4744 | `				bAnyNamed = 1;` |
|        17 | 4745 | `			}` |
|       350 | 4746 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       350 | 4747 | `			ai++;` |
|       350 | 4748 | `			ci++;` |
|         - | 4749 | `		}` |
|         4 | 4750 | `	}` |
|         - | 4751 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 4752 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 4753 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 4754 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|       580 | 4755 | `	VmSpreadConsume(pVm);` |
|       580 | 4756 | `	if( !bAnyNamed ){` |
|         - | 4757 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|       504 | 4758 | `		return 0;` |
|         - | 4759 | `	}` |
|        77 | 4760 | `	pEff->bHasNamed = 1;` |
|        77 | 4761 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|        77 | 4762 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|        77 | 4763 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|         - | 4764 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|         - | 4765 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|         - | 4766 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|         - | 4767 | `	 * is the caller's stack local. */` |
|        77 | 4768 | `	pEff->bArgShapes = 0;` |
|        77 | 4769 | `	pEff->nNonLvalMask = 0;` |
|        77 | 4770 | `	pEff->nTempCallMask = 0;` |
|        77 | 4771 | `	if( pCompile ){` |
|        37 | 4772 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        19 | 4773 | `	}else{` |
|        41 | 4774 | `		pEff->sAssertSrc.zString = 0;` |
|        41 | 4775 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 4776 | `	}` |
|        77 | 4777 | `	pEff->nTotal = nActual;` |
|        77 | 4778 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|        77 | 4779 | `	return 1;` |
|       292 | 4780 | `}` |
|         - | 4781 | `/*` |
|         - | 4782 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 4783 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 4784 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 4785 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 4786 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 4787 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 4788 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 4789 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 4790 | ` * pArg must be the site's FINAL argument base.` |
|         - | 4791 | ` */` |
|   5140346 | 4792 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 4793 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 4794 | `{` |
|   5140351 | 4795 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   5140351 | 4796 | `	if( pInstr->iP2 == 0 ){` |
|   5139739 | 4797 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 4798 | `	}` |
|       616 | 4799 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|        77 | 4800 | `		return pStorage;` |
|         - | 4801 | `	}` |
|       540 | 4802 | `	VmSpreadConsume(pVm);` |
|       540 | 4803 | `	return pCompile;` |
|   2571225 | 4804 | `}` |
|         - | 4805 | `/*` |
|         - | 4806 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 4807 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 4808 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 4809 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 4810 | ` */` |
|        14 | 4811 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         3 | 4812 | `{` |
|        17 | 4813 | `	const char *zType = "unknown";` |
|         - | 4814 | `	char zMsg[64];` |
|        17 | 4815 | `	if( iFlags & MEMOBJ_STRING ){` |
|         6 | 4816 | `		zType = "string";` |
|        14 | 4817 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 4818 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 4819 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 4820 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 4821 | `		zType = "float";` |
|        12 | 4822 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        10 | 4823 | `		zType = "int";` |
|         7 | 4824 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 4825 | `		zType = "bool";` |
|         1 | 4826 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 4827 | `		zType = "object";` |
|       ! 0 | 4828 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 4829 | `		zType = "resource";` |
|       ! 0 | 4830 | `	}` |
|        17 | 4831 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        17 | 4832 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        17 | 4833 | `}` |
|         - | 4834 | `/*` |
|         - | 4835 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - | 4836 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - | 4837 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - | 4838 | ` * per context — see VmMemberCtxWantsValue.` |
|         - | 4839 | ` */` |
|      1888 | 4840 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 4841 | `{` |
|      1893 | 4842 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 | 4843 | `}` |
|         - | 4844 | `/*` |
|         - | 4845 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - | 4846 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - | 4847 | ` * isset() stops at the truth.` |
|         - | 4848 | ` */` |
|        44 | 4849 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 | 4850 | `{` |
|        47 | 4851 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 | 4852 | `}` |
|         - | 4853 | `/*` |
|         - | 4854 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 4855 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 4856 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 4857 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 4858 | ` * reads (other names / other instances) still dispatch.` |
|         - | 4859 | ` */` |
|      1626 | 4860 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4861 | `{` |
|         - | 4862 | `	VmMagicGuard *aG;` |
|         - | 4863 | `	sxu32 nHash;` |
|         - | 4864 | `	sxu32 n;` |
|      1631 | 4865 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 4866 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 4867 | `		 * every hooked-property access consults the guard, often twice. */` |
|      1447 | 4868 | `		return FALSE;` |
|         - | 4869 | `	}` |
|       186 | 4870 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       186 | 4871 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       230 | 4872 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       186 | 4873 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       142 | 4874 | `			return TRUE;` |
|         - | 4875 | `		}` |
|        23 | 4876 | `	}` |
|        45 | 4877 | `	return FALSE;` |
|       818 | 4878 | `}` |
|       800 | 4879 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4880 | `{` |
|         - | 4881 | `	VmMagicGuard sG;` |
|       805 | 4882 | `	sG.pThis = pThis;` |
|       805 | 4883 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       805 | 4884 | `	sG.cKind = cKind;` |
|       805 | 4885 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|       805 | 4886 | `}` |
|       800 | 4887 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         5 | 4888 | `{` |
|       805 | 4889 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|       805 | 4890 | `}` |
|         - | 4891 | `/*` |
|         - | 4892 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 4893 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 4894 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 4895 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 4896 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 4897 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 4898 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 4899 | ` * One-token lookahead only.` |
|         - | 4900 | ` */` |
|      1190 | 4901 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 | 4902 | `{` |
|      1195 | 4903 | `	switch( pNext->iOp ){` |
|        44 | 4904 | `		case PH7_OP_STORE:` |
|        92 | 4905 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|         5 | 4906 | `		case PH7_OP_STORE_REF:` |
|        11 | 4907 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|        18 | 4908 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 4909 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4910 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4911 | `		case PH7_OP_CAT_STORE:` |
|         - | 4912 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4913 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|        37 | 4914 | `			return 1;` |
|       528 | 4915 | `		default:` |
|      1061 | 4916 | `			return 0;` |
|         - | 4917 | `	}` |
|       600 | 4918 | `}` |
|         - | 4919 | `/*` |
|         - | 4920 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|         - | 4921 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|         - | 4922 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|         - | 4923 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|         - | 4924 | ` * treat them as one.` |
|         - | 4925 | ` */` |
|       318 | 4926 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|         5 | 4927 | `{` |
|       453 | 4928 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|       464 | 4929 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|         5 | 4930 | `}` |
|         - | 4931 | `/*` |
|         - | 4932 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|         - | 4933 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|         - | 4934 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|         - | 4935 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|         - | 4936 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|         - | 4937 | `` * offsetSet where `$o['n']++` only notices.`` |
|         - | 4938 | ` */` |
|       520 | 4939 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|         5 | 4940 | `{` |
|       525 | 4941 | `	switch( pNext->iOp ){` |
|        52 | 4942 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4943 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4944 | `		case PH7_OP_CAT_STORE:` |
|         - | 4945 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4946 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       105 | 4947 | `			return 1;` |
|       208 | 4948 | `		default:` |
|       421 | 4949 | `			return 0;` |
|         - | 4950 | `	}` |
|       265 | 4951 | `}` |
|         - | 4952 | `/*` |
|         - | 4953 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 4954 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 4955 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 4956 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 4957 | ` */` |
|       546 | 4958 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         5 | 4959 | `{` |
|       551 | 4960 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         5 | 4961 | `}` |
|         - | 4962 | `/*` |
|         - | 4963 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 4964 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 4965 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 4966 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 4967 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 4968 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 4969 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 4970 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 4971 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 4972 | ` * abort path; SXRET_OK otherwise.` |
|         - | 4973 | ` */` |
|        76 | 4974 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         4 | 4975 | `{` |
|         - | 4976 | `	char zHName[384];` |
|         - | 4977 | `	sxu32 nHName;` |
|         - | 4978 | `	ph7_class_method *pSetHook;` |
|        80 | 4979 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 4980 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 4981 | `		SyBlob sErrMsg;` |
|         5 | 4982 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 4983 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 4984 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 4985 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 4986 | `		return SXRET_OK;` |
|         - | 4987 | `	}` |
|        76 | 4988 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 4989 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 4990 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 4991 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 4992 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 4993 | `			return SXRET_OK;` |
|         - | 4994 | `		}` |
|       ! 0 | 4995 | `	}` |
|        76 | 4996 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        76 | 4997 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        76 | 4998 | `	if( pSetHook ){` |
|         - | 4999 | `		ph7_value sHookRet;` |
|         - | 5000 | `		ph7_value *apHArg[1];` |
|        76 | 5001 | `		apHArg[0] = pValue;` |
|        76 | 5002 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        76 | 5003 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        76 | 5004 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        76 | 5005 | `		VmMagicGuardPop(pVm);` |
|        72 | 5006 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        42 | 5007 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         6 | 5008 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         6 | 5009 | `			if( rcH == SXRET_OK ){` |
|         6 | 5010 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|         6 | 5011 | `				if( pBack ){` |
|         6 | 5012 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         4 | 5013 | `				}` |
|         2 | 5014 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 5015 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 5016 | `				return PH7_ABORT;` |
|         - | 5017 | `			}` |
|         - | 5018 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 5019 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 5020 | `			 * parked throw. */` |
|         2 | 5021 | `		}` |
|        76 | 5022 | `		PH7_MemObjRelease(&sHookRet);` |
|        36 | 5023 | `	}` |
|        76 | 5024 | `	return SXRET_OK;` |
|        42 | 5025 | `}` |
|         - | 5026 | `/*` |
|         - | 5027 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 5028 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 5029 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 5030 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 5031 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 5032 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 5033 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 5034 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 5035 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 5036 | ` */` |
|         - | 5037 | `/*` |
|         - | 5038 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 5039 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 5040 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 5041 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 5042 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 5043 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 5044 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 5045 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 5046 | ` * caller reads the raw slot then.` |
|         - | 5047 | ` */` |
|       512 | 5048 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 | 5049 | `{` |
|       517 | 5050 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 5051 | `	char zHName[384];` |
|         - | 5052 | `	sxu32 nHName;` |
|         - | 5053 | `	ph7_class_method *pGetHook;` |
|         - | 5054 | `	sxi32 rc;` |
|       512 | 5055 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       351 | 5056 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       195 | 5057 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 5058 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 5059 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 5060 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 5061 | `		 * raw values whose output the routed throw then discards. */` |
|       327 | 5062 | `		return SXERR_NOTFOUND;` |
|         - | 5063 | `	}` |
|       195 | 5064 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       195 | 5065 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       195 | 5066 | `	if( pGetHook == 0 ){` |
|       ! 0 | 5067 | `		return SXERR_NOTFOUND;` |
|         - | 5068 | `	}` |
|       195 | 5069 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       195 | 5070 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       195 | 5071 | `	VmMagicGuardPop(pVm);` |
|       195 | 5072 | `	return rc;` |
|       261 | 5073 | `}` |
|         - | 5074 | `/*` |
|         - | 5075 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 5076 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 5077 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 5078 | ` */` |
|       130 | 5079 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5080 | `{` |
|       131 | 5081 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 5082 | `	VmSlot sFree;` |
|       131 | 5083 | `	if( pScr ){` |
|       131 | 5084 | `		PH7_MemObjRelease(pScr);` |
|        65 | 5085 | `	}` |
|       131 | 5086 | `	sFree.nIdx = nIdx;` |
|       131 | 5087 | `	sFree.pUserData = 0;` |
|       131 | 5088 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       131 | 5089 | `}` |
|         - | 5090 | `/*` |
|         - | 5091 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 5092 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 5093 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 5094 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 5095 | ` * instance reference.` |
|         - | 5096 | ` */` |
|        22 | 5097 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 5098 | `{` |
|        24 | 5099 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        24 | 5100 | `	if( pEnt == 0 ){` |
|         5 | 5101 | `		return;` |
|         - | 5102 | `	}` |
|        19 | 5103 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        11 | 5104 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         5 | 5105 | `	}` |
|        19 | 5106 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|         - | 5107 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|         - | 5108 | `		 * property's backing store — this entry owns it. */` |
|         3 | 5109 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|         1 | 5110 | `	}` |
|        19 | 5111 | `	SyBlobRelease(&pEnt->sName);` |
|        19 | 5112 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        19 | 5113 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        13 | 5114 | `}` |
|        78 | 5115 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5116 | `{` |
|         - | 5117 | `	VmHookRmw sEnt;` |
|         - | 5118 | `	VmHookRmw *pEnt;` |
|         - | 5119 | `	ph7_value *pScr;` |
|         - | 5120 | `	ph7_value sVal;` |
|         - | 5121 | `	ph7_value sKey;` |
|        79 | 5122 | `	sxi32 rc = SXRET_OK;` |
|        79 | 5123 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        79 | 5124 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 5125 | `		return SXERR_NOTFOUND;` |
|         - | 5126 | `	}` |
|        79 | 5127 | `	sEnt = *pEnt;` |
|        79 | 5128 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 5129 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|         - | 5130 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|         - | 5131 | `	 * scratch index past this point). The DIM kind's KEY slot goes the same` |
|         - | 5132 | `	 * way, for the same reason: reserving relocates the aMemObj set. */` |
|        79 | 5133 | `	PH7_MemObjInit(pVm,&sVal);` |
|        79 | 5134 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        79 | 5135 | `	if( pScr ){` |
|        79 | 5136 | `		PH7_MemObjStore(pScr,&sVal);` |
|        39 | 5137 | `	}` |
|        79 | 5138 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        79 | 5139 | `	sVal.nIdx = SXU32_HIGH;` |
|        79 | 5140 | `	PH7_MemObjInit(pVm,&sKey);` |
|        79 | 5141 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|        41 | 5142 | `		ph7_value *pKeySlot = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|        41 | 5143 | `		if( pKeySlot ){` |
|        41 | 5144 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|        20 | 5145 | `		}` |
|        41 | 5146 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|        41 | 5147 | `		sKey.nIdx = SXU32_HIGH;` |
|        20 | 5148 | `	}` |
|        79 | 5149 | `	if( pVm->nBoundaryRc == 0 ){` |
|        79 | 5150 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|         - | 5151 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|         - | 5152 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|         - | 5153 | `			SyString sPropName;` |
|        25 | 5154 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|        25 | 5155 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|        67 | 5156 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|         - | 5157 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|         - | 5158 | `			 * back through offsetSet($key, $value). */` |
|        41 | 5159 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|         - | 5160 | `				"offsetSet",sizeof("offsetSet")-1);` |
|        41 | 5161 | `			if( pSet ){` |
|         - | 5162 | `				ph7_value *apArg[2];` |
|        41 | 5163 | `				apArg[0] = &sKey;` |
|        41 | 5164 | `				apArg[1] = &sVal;` |
|        41 | 5165 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|        20 | 5166 | `			}` |
|        21 | 5167 | `		}else{` |
|        15 | 5168 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         - | 5169 | `		}` |
|        39 | 5170 | `	}` |
|        79 | 5171 | `	SyBlobRelease(&sEnt.sName);` |
|        79 | 5172 | `	PH7_MemObjRelease(&sKey);` |
|        79 | 5173 | `	PH7_MemObjRelease(&sVal);` |
|        79 | 5174 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        79 | 5175 | `	return rc;` |
|        40 | 5176 | `}` |
|         - | 5177 | `/*` |
|         - | 5178 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 5179 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 5180 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 5181 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 5182 | ` */` |
|        54 | 5183 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         3 | 5184 | `{` |
|        57 | 5185 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|        57 | 5186 | `	if( pSetMeth ){` |
|         - | 5187 | `		ph7_value sNameVal;` |
|         - | 5188 | `		ph7_value *apSetArg[2];` |
|        57 | 5189 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|        57 | 5190 | `		sNameVal.nIdx = SXU32_HIGH;` |
|        57 | 5191 | `		apSetArg[0] = &sNameVal;` |
|        57 | 5192 | `		apSetArg[1] = pValue;` |
|        57 | 5193 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|        57 | 5194 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|        57 | 5195 | `		VmMagicGuardPop(pVm);` |
|        57 | 5196 | `		PH7_MemObjRelease(&sNameVal);` |
|        27 | 5197 | `	}` |
|        57 | 5198 | `}` |
|         - | 5199 | `/*` |
|         - | 5200 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 5201 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 5202 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 5203 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 5204 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 5205 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 5206 | ` * path (the SyHash-layout incident class).` |
|         - | 5207 | ` */` |
|         - | 5208 | `/*` |
|         - | 5209 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 5210 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 5211 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 5212 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 5213 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 5214 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 5215 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 5216 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 5217 | ` * never linked (INIT error path).` |
|         - | 5218 | ` */` |
|     30368 | 5219 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 5220 | `{` |
|     30373 | 5221 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     30373 | 5222 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 5223 | `	sxu32 i;` |
|     32721 | 5224 | `	for( i = 0 ; i < n ; ++i ){` |
|     32721 | 5225 | `		if( apStep[i] == pStep ){` |
|     30381 | 5226 | `			for( ; i + 1 < n ; ++i ){` |
|         9 | 5227 | `				apStep[i] = apStep[i + 1];` |
|         5 | 5228 | `			}` |
|     30373 | 5229 | `			(void)SySetPop(&pInfo->aStep);` |
|     30373 | 5230 | `			return;` |
|         - | 5231 | `		}` |
|      1179 | 5232 | `	}` |
|     15189 | 5233 | `}` |
|       366 | 5234 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 5235 | `{` |
|       371 | 5236 | `	if( pStep->pOwner ){` |
|        77 | 5237 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|        37 | 5238 | `	}` |
|       371 | 5239 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       371 | 5240 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       371 | 5241 | `	PH7_ClassInstanceUnref(pThis);` |
|       371 | 5242 | `}` |
|         - | 5243 | `/*` |
|         - | 5244 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 5245 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 5246 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 5247 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 5248 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 5249 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 5250 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 5251 | ` * map's last reference.` |
|         - | 5252 | ` */` |
|     29942 | 5253 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 5254 | `{` |
|     29947 | 5255 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     29947 | 5256 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     29947 | 5257 | `	if( bPop ){` |
|         - | 5258 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 5259 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     29947 | 5260 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     14971 | 5261 | `	}` |
|     29947 | 5262 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     29947 | 5263 | `	PH7_HashmapUnref(pMap);` |
|     29947 | 5264 | `}` |
|         - | 5265 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 5266 | `/*` |
|         - | 5267 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 5268 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5269 | ` * See block-comment on that function for additional information.` |
|         - | 5270 | ` */` |
|   1488866 | 5271 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 5272 | `{` |
|         - | 5273 | `	ph7_value *pStack;` |
|         - | 5274 | `	sxu32 nCap;` |
|         - | 5275 | `	sxi32 rc;` |
|         - | 5276 | `	/* Allocate a new operand stack */` |
|   1488871 | 5277 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   1488871 | 5278 | `	if( pStack == 0 ){` |
|       ! 0 | 5279 | `		return SXERR_MEM;` |
|         - | 5280 | `	}` |
|   1488871 | 5281 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 5282 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 5283 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   1488871 | 5284 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 5285 | `	/* Free the operand stack */` |
|   1488871 | 5286 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 5287 | `	/* Execution result */` |
|   1488871 | 5288 | `	return rc;` |
|    744437 | 5289 | `}` |
|         - | 5290 | `/*` |
|         - | 5291 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - | 5292 | ` * statement must honour?` |
|         - | 5293 | ` *` |
|         - | 5294 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - | 5295 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - | 5296 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - | 5297 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - | 5298 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - | 5299 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - | 5300 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - | 5301 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - | 5302 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - | 5303 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - | 5304 | ` * drift apart.` |
|         - | 5305 | ` *` |
|         - | 5306 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - | 5307 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - | 5308 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - | 5309 | ` * this opcode did not create as its own throw.` |
|         - | 5310 | ` *` |
|         - | 5311 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - | 5312 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - | 5313 | ` */` |
|         - | 5314 | `/*` |
|         - | 5315 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - | 5316 | ` *` |
|         - | 5317 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - | 5318 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - | 5319 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - | 5320 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - | 5321 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - | 5322 | ` * captured value writes over the superglobal's own slot: calling` |
|         - | 5323 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - | 5324 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - | 5325 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - | 5326 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - | 5327 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - | 5328 | ` *` |
|         - | 5329 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - | 5330 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - | 5331 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - | 5332 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - | 5333 | ` */` |
|      2488 | 5334 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 | 5335 | `{` |
|         - | 5336 | `	static const char *const azAuto[] = {` |
|         - | 5337 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - | 5338 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - | 5339 | `	};` |
|         - | 5340 | `	sxu32 n;` |
|     26875 | 5341 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|     24437 | 5342 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|     24437 | 5343 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|        53 | 5344 | `			return 1;` |
|         - | 5345 | `		}` |
|     12196 | 5346 | `	}` |
|      2443 | 5347 | `	return 0;` |
|      1249 | 5348 | `}` |
|     16276 | 5349 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 | 5350 | `{` |
|     16258 | 5351 | `	return rc == PH7_EXCEPTION` |
|     16253 | 5352 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|     24391 | 5353 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 | 5354 | `}` |
|         - | 5355 | `/*` |
|         - | 5356 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 5357 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 5358 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 5359 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 5360 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 5361 | ` */` |
|       140 | 5362 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         1 | 5363 | `{` |
|       141 | 5364 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       141 | 5365 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 5366 | `	sxi32 rc;` |
|       141 | 5367 | `	if( pDeclCls ){` |
|       125 | 5368 | `		pVm->pConstEvalClass = pDeclCls;` |
|       125 | 5369 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        62 | 5370 | `	}` |
|       141 | 5371 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       141 | 5372 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       141 | 5373 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       141 | 5374 | `	return rc;` |
|         1 | 5375 | `}` |
|         - | 5376 | `/*` |
|         - | 5377 | ` * Flush every still-open output buffer at the end of execution. php implicitly` |
|         - | 5378 | ` * ends+flushes all ob_start() levels on shutdown (normal end, exit()/die(), or` |
|         - | 5379 | ` * fatal); PHL used to DISCARD them, so a script that never called ob_end_flush()` |
|         - | 5380 | ` * — e.g. PHPUnit, which buffers its result summary and then exit()s with a` |
|         - | 5381 | ` * non-zero status — lost that output entirely.` |
|         - | 5382 | ` *` |
|         - | 5383 | ` * PH7_VmObFlushAll() does it the way php does: one FINAL operation per buffer,` |
|         - | 5384 | ` * innermost first, so each handler's answer is what the buffer under it is` |
|         - | 5385 | ` * handed.` |
|         - | 5386 | ` */` |
|      4562 | 5387 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 5388 | `{` |
|      4567 | 5389 | `	PH7_VmObFlushAll(&(*pVm));` |
|      4567 | 5390 | `}` |
|         - | 5391 | `/*` |
|         - | 5392 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 5393 | ` * or more calls to [register_shutdown_function()].` |
|         - | 5394 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 5395 | ` * execution ends.` |
|         - | 5396 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 5397 | ` * additional information.` |
|         - | 5398 | ` */` |
|      4562 | 5399 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 5400 | `{` |
|         - | 5401 | `	VmShutdownCB *pEntry;` |
|         - | 5402 | `	ph7_value *apArg[10];` |
|         - | 5403 | `	sxu32 n,nEntry;` |
|         - | 5404 | `	int i;` |
|         - | 5405 | `	/* Point to the stack of registered callbacks */` |
|      4567 | 5406 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     50187 | 5407 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     45625 | 5408 | `		apArg[i] = 0;` |
|     22815 | 5409 | `	}` |
|         - | 5410 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 5411 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 5412 | `	 * callbacks, mirroring PHP.` |
|         - | 5413 | `	 */` |
|      4567 | 5414 | `	pVm->bHaltRequested = 0;` |
|      4589 | 5415 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        27 | 5416 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        27 | 5417 | `		if( pEntry ){` |
|         - | 5418 | `			/* Prepare callback arguments if any */` |
|        27 | 5419 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 5420 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 5421 | `					break;` |
|         - | 5422 | `				}` |
|       ! 0 | 5423 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 5424 | `			}` |
|         - | 5425 | `			/* Invoke the callback */` |
|        27 | 5426 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 5427 | `			/*` |
|         - | 5428 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 5429 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 5430 | `			 */` |
|        27 | 5431 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        27 | 5432 | `			if( pEntry ){` |
|        27 | 5433 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        27 | 5434 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 5435 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 5436 | `				}` |
|        11 | 5437 | `			}` |
|        27 | 5438 | `			if( pVm->bHaltRequested ){` |
|         - | 5439 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|       ! 0 | 5440 | `				break;` |
|         - | 5441 | `			}` |
|        11 | 5442 | `		}` |
|        16 | 5443 | `	}` |
|      4567 | 5444 | `	SySetReset(&pVm->aShutdown);` |
|      4567 | 5445 | `}` |
|         - | 5446 | `/*` |
|         - | 5447 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 5448 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5449 | ` * See block-comment on that function for additional information.` |
|         - | 5450 | ` */` |
|      4562 | 5451 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 5452 | `{` |
|         - | 5453 | `	/* Make sure we are ready to execute this program */` |
|      4567 | 5454 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 5455 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 5456 | `	}` |
|         - | 5457 | `	/* Set the execution magic number  */` |
|      4567 | 5458 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 5459 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 5460 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 5461 | `	{` |
|      4567 | 5462 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|      4567 | 5463 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 5464 | `	}` |
|         - | 5465 | `	/* Invoke any shutdown callbacks */` |
|      4567 | 5466 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 5467 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 5468 | `	 * shutdown callbacks, which may still write into them. */` |
|      4567 | 5469 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 5470 | `	/* An open session is written back LAST, from php's own module shutdown: after` |
|         - | 5471 | `	 * the script's shutdown callbacks (which may still write to $_SESSION) and` |
|         - | 5472 | `	 * after the buffers are flushed (which is why its diagnostics land outside` |
|         - | 5473 | `	 * them). */` |
|      4567 | 5474 | `	PH7_VmSessionShutdown(&(*pVm));` |
|         - | 5475 | `	/*` |
|         - | 5476 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 5477 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 5478 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 5479 | `	 */` |
|      4567 | 5480 | `	return SXRET_OK;` |
|      2286 | 5481 | `}` |
|         - | 5482 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 5483 | `/*` |
|         - | 5484 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 5485 | ` * the desired message.` |
|         - | 5486 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 5487 | ` * in 'api.c' for additional information.` |
|         - | 5488 | ` */` |
|     18762 | 5489 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 5490 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 5491 | `	SyString *pString /* Message to output */` |
|         - | 5492 | `	)` |
|         5 | 5493 | `{` |
|     18767 | 5494 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     18767 | 5495 | `	sxi32 rc = SXRET_OK;` |
|         - | 5496 | `	/* Call the output consumer */` |
|     18767 | 5497 | `	if( pString->nByte > 0 ){` |
|     18767 | 5498 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     18767 | 5499 | `		VmTrackOutput(pVm, pString->nByte);` |
|      9381 | 5500 | `	}` |
|     18767 | 5501 | `	return rc;` |
|         5 | 5502 | `}` |
|         - | 5503 | `/*` |
|         - | 5504 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 5505 | ` * callback to consume the formatted message.` |
|         - | 5506 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 5507 | ` * in 'api.c' for additional information.` |
|         - | 5508 | ` */` |
|         2 | 5509 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 5510 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 5511 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 5512 | `	va_list ap           /* Variable list of arguments */` |
|         - | 5513 | `	)` |
|         1 | 5514 | `{` |
|         3 | 5515 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|         3 | 5516 | `	sxi32 rc = SXRET_OK;` |
|         - | 5517 | `	SyBlob sWorker;` |
|         - | 5518 | `	/* Format the message and call the output consumer */` |
|         3 | 5519 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|         3 | 5520 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|         3 | 5521 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 5522 | `		/* Consume the formatted message */` |
|         3 | 5523 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|         1 | 5524 | `	}` |
|         3 | 5525 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 5526 | `	/* Release the working buffer */` |
|         3 | 5527 | `	SyBlobRelease(&sWorker);` |
|         3 | 5528 | `	return rc;` |
|         1 | 5529 | `}` |
|         - | 5530 | `/*` |
|         - | 5531 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 5532 | ` * This function never fail and always return a pointer` |
|         - | 5533 | ` * to a null terminated string.` |
|         - | 5534 | ` */` |
|        10 | 5535 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 5536 | `{` |
|        11 | 5537 | `	const char *zOp = "Unknown     ";` |
|        11 | 5538 | `	switch(nOp){` |
|         3 | 5539 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 5540 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 5541 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 5542 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 5543 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 5544 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 5545 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 5546 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 5547 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 5548 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 5549 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 5550 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 5551 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 5552 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 5553 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 5554 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 5555 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 5556 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 5557 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 5558 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 5559 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 5560 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|       ! 0 | 5561 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|       ! 0 | 5562 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 5563 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 5564 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 5565 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 5566 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 5567 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 5568 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 5569 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 5570 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 5571 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 5572 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 5573 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 5574 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 5575 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 5576 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 5577 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 5578 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 5579 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 5580 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 5581 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 5582 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 5583 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 5584 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 5585 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 5586 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 5587 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 5588 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 5589 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 5590 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 5591 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 5592 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 5593 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 | 5594 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 5595 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 5596 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 5597 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 5598 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 5599 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 5600 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 5601 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 5602 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 5603 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 5604 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 5605 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 5606 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 5607 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 5608 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 5609 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 5610 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 5611 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 5612 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 5613 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 5614 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 5615 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 5616 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 5617 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 5618 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 5619 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 5620 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 5621 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 5622 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 5623 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 5624 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 5625 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 5626 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 5627 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 5628 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 5629 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 5630 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 5631 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 5632 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 5633 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 5634 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 5635 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 5636 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 5637 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 5638 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 5639 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 5640 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 5641 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 5642 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 5643 | `	default:` |
|       ! 0 | 5644 | `		break;` |
|         - | 5645 | `	}` |
|        11 | 5646 | `	return zOp;` |
|         1 | 5647 | `}` |
|         - | 5648 | `/*` |
|         - | 5649 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 5650 | ` * The xConsumer() callback which is an used defined function` |
|         - | 5651 | ` * is responsible of consuming the generated dump.` |
|         - | 5652 | ` */` |
|         2 | 5653 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 5654 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 5655 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 5656 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 5657 | `	)` |
|         1 | 5658 | `{` |
|         - | 5659 | `	sxi32 rc;` |
|         3 | 5660 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 5661 | `	return rc;` |
|         1 | 5662 | `}` |
|         - | 5663 | `/*` |
|         - | 5664 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 5665 | ` * outside a class body [i.e: global or function scope].` |
|         - | 5666 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 5667 | ` * in 'compile.c' for additional information.` |
|         - | 5668 | ` */` |
|         2 | 5669 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         1 | 5670 | `{` |
|         3 | 5671 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 5672 | `	/* Evaluate and expand constant value */` |
|         3 | 5673 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|         3 | 5674 | `}` |
|         - | 5675 | `/*` |
|         - | 5676 | ` * Section:` |
|         - | 5677 | ` *  Function handling functions.` |
|         - | 5678 | ` * Status:` |
|         - | 5679 | ` *    Stable.` |
|         - | 5680 | ` */` |
|         - | 5681 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 5682 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 5683 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 5684 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 5685 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 5686 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 5687 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 5688 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 5689 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 5690 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 5691 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 5692 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 5693 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 5694 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 5695 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 5696 | `	    /* Constants management */` |
|         - | 5697 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 5698 | `	{ "define",   vm_builtin_define               },` |
|         - | 5699 | `	{ "constant", vm_builtin_constant             },` |
|         - | 5700 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 5701 | `	   /* Class/Object functions */` |
|         - | 5702 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 5703 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 5704 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 5705 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 5706 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 5707 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 5708 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 5709 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 5710 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 5711 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 5712 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 5713 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 5714 | `	{ "get_declared_traits",     vm_builtin_get_declared_traits    },` |
|         - | 5715 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 5716 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 5717 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 5718 | `	{ "get_mangled_object_vars", vm_builtin_get_mangled_object_vars },` |
|         - | 5719 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 5720 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 5721 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|         - | 5722 | `	{ "clone",           vm_builtin_clone             },` |
|         - | 5723 | `	   /* SPL object identity */` |
|         - | 5724 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 5725 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 5726 | `	   /* SPL Autoloading */` |
|         - | 5727 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 5728 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 5729 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 5730 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 5731 | `	   /* Random numbers/strings generators */` |
|         - | 5732 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 5733 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 5734 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 5735 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 5736 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 5737 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 5738 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 5739 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5740 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 5741 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 5742 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 5743 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5744 | `	   /* Language constructs functions */` |
|         - | 5745 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 5746 | `	{ "print", vm_builtin_print                   },` |
|         - | 5747 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 5748 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 5749 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 5750 | `	  /* Variable handling functions */` |
|         - | 5751 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 5752 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 5753 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|         - | 5754 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 5755 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 5756 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 5757 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 5758 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 5759 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 5760 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 5761 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 5762 | `	  /* Ouput control functions */` |
|         - | 5763 | `	{ "flush",        vm_builtin_flush             },` |
|         - | 5764 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 5765 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 5766 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 5767 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 5768 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 5769 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 5770 | `	{ "ob_get_flush",    vm_builtin_ob_get_flush   },` |
|         - | 5771 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 5772 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 5773 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 5774 | `	{ "ob_get_status",     vm_builtin_ob_get_status },` |
|         - | 5775 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 5776 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 5777 | `	  /* Assertion functions */` |
|         - | 5778 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 5779 | `	  /* Error reporting functions */` |
|         - | 5780 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 5781 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 5782 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 5783 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 5784 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 5785 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 5786 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 5787 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 5788 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 5789 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 5790 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 5791 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 5792 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 5793 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 5794 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 5795 | `	  /* Release info */` |
|         - | 5796 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 5797 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 5798 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|         - | 5799 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|         - | 5800 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 5801 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 5802 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 5803 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 5804 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 5805 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 5806 | `	  /* hashmap */` |
|         - | 5807 | `	{"compact",          vm_builtin_compact       },` |
|         - | 5808 | `	{"extract",          vm_builtin_extract       },` |
|         - | 5809 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 5810 | `	  /* URL related function */` |
|         - | 5811 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 5812 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 5813 | `	   /* Command line processing */` |
|         - | 5814 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 5815 | `	   /* JSON encoding/decoding */` |
|         - | 5816 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 5817 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 5818 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 5819 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 5820 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 5821 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 5822 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 5823 | `	   /* Files/URI inclusion facility */` |
|         - | 5824 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 5825 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 5826 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 5827 | `	{ "include",      vm_builtin_include          },` |
|         - | 5828 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 5829 | `	{ "require",      vm_builtin_require          },` |
|         - | 5830 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 5831 | `};` |
|         - | 5832 | `/*` |
|         - | 5833 | ` * Register the built-in VM functions defined above.` |
|         - | 5834 | ` */` |
|      4552 | 5835 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 5836 | `{` |
|         - | 5837 | `	sxi32 rc;` |
|         - | 5838 | `	sxu32 n;` |
|    578109 | 5839 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 5840 | `		/* Note that these special functions have access` |
|         - | 5841 | `		 * to the underlying virtual machine as their` |
|         - | 5842 | `		 * private data.` |
|         - | 5843 | `		 */` |
|    573557 | 5844 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    573557 | 5845 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 5846 | `			return rc;` |
|         - | 5847 | `		}` |
|    286781 | 5848 | `	}` |
|      4557 | 5849 | `	return SXRET_OK;` |
|      2281 | 5850 | `}` |
|         - | 5851 | `/*` |
|         - | 5852 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 5853 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 5854 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 5855 | ` */` |
|   7780908 | 5856 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 5857 | `{` |
|   7780913 | 5858 | `	if( !iLoadable ){` |
|   5212015 | 5859 | `		return pClass;` |
|         - | 5860 | `	}` |
|   2568919 | 5861 | `	while(pClass){` |
|   2568903 | 5862 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   2568887 | 5863 | `			return pClass;` |
|         - | 5864 | `		}` |
|        17 | 5865 | `		pClass = pClass->pNextName;` |
|         1 | 5866 | `	}` |
|        17 | 5867 | `	return 0;` |
|   3890459 | 5868 | `}` |
|         - | 5869 | `/*` |
|         - | 5870 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 5871 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 5872 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 5873 | ` * registered in the VM's class table.` |
|         - | 5874 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 5875 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 5876 | ` */` |
|       446 | 5877 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5878 | `{` |
|         - | 5879 | `	VmAutoloadCB *pEntry;` |
|         - | 5880 | `	ph7_value sArg,sResult;` |
|         - | 5881 | `	SyHashEntry *pHashEntry;` |
|         - | 5882 | `	ph7_class *pClass;` |
|         - | 5883 | `	sxu32 n,nEntry;` |
|       451 | 5884 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       451 | 5885 | `	if( nEntry < 1 ){` |
|       313 | 5886 | `		return 0;` |
|         - | 5887 | `	}` |
|         - | 5888 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       143 | 5889 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 5890 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 5891 | `	}` |
|         - | 5892 | `	/* Mark this class as being autoloaded */` |
|       141 | 5893 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 5894 | `	/* Prepare the class name argument */` |
|       141 | 5895 | `	PH7_MemObjInit(pVm,&sArg);` |
|       141 | 5896 | `	PH7_MemObjInit(pVm,&sResult);` |
|       141 | 5897 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       141 | 5898 | `	pClass = 0;` |
|       259 | 5899 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 5900 | `		ph7_value *apArg[1];` |
|       151 | 5901 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       151 | 5902 | `		if( pEntry == 0 ){` |
|       ! 0 | 5903 | `			continue;` |
|         - | 5904 | `		}` |
|       151 | 5905 | `		apArg[0] = &sArg;` |
|       151 | 5906 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|         - | 5907 | `			/* Callback could not be invoked — skip to next autoloader */` |
|        25 | 5908 | `			continue;` |
|         - | 5909 | `		}` |
|         - | 5910 | `		/* Check if the class is now available */` |
|       129 | 5911 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       129 | 5912 | `		if( pHashEntry ){` |
|        33 | 5913 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        33 | 5914 | `			if( pClass ){` |
|        33 | 5915 | `				break;` |
|         - | 5916 | `			}` |
|       ! 0 | 5917 | `		}` |
|        53 | 5918 | `	}` |
|       141 | 5919 | `	PH7_MemObjRelease(&sArg);` |
|       141 | 5920 | `	PH7_MemObjRelease(&sResult);` |
|         - | 5921 | `	/* Remove reentrancy guard */` |
|       141 | 5922 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       141 | 5923 | `	return pClass;` |
|       228 | 5924 | `}` |
|         - | 5925 | `/*` |
|         - | 5926 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 5927 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 5928 | ` */` |
|        46 | 5929 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5930 | `{` |
|        51 | 5931 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         5 | 5932 | `}` |
|         - | 5933 | `/*` |
|         - | 5934 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 5935 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 5936 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 5937 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 5938 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 5939 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 5940 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 5941 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 5942 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 5943 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 5944 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 5945 | ` */` |
|   9254354 | 5946 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 5947 | `{` |
|   9254359 | 5948 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        72 | 5949 | `		(*pzName)++;` |
|        72 | 5950 | `		(*pnByte)--;` |
|        34 | 5951 | `	}` |
|   9254359 | 5952 | `}` |
|         - | 5953 | `/*` |
|         - | 5954 | ` * Check if the given name refer to an installed class.` |
|         - | 5955 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 5956 | ` */` |
|   7781286 | 5957 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 5958 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 5959 | `	const char *zName,  /* Name of the target class */` |
|         - | 5960 | `	sxu32 nByte,        /* zName length */` |
|         - | 5961 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 5962 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 5963 | `						 */` |
|         - | 5964 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 5965 | `	)` |
|         5 | 5966 | `{` |
|         - | 5967 | `	SyHashEntry *pEntry;` |
|         - | 5968 | `	ph7_class *pClass;` |
|   3890643 | 5969 | `	SXUNUSED(iNest);` |
|         - | 5970 | `	/* Exact class lookup.` |
|         - | 5971 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 5972 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 5973 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 5974 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   7781291 | 5975 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 5976 | `	/* An empty stripped name names no class: neither a truly empty "" nor a lone` |
|         - | 5977 | `	 * "\" is looked up or handed to the autoloader (php 8.5.11, GH-23232). */` |
|   7781291 | 5978 | `	if( nByte < 1 ){` |
|         8 | 5979 | `		return 0;` |
|         - | 5980 | `	}` |
|   7781285 | 5981 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   7781285 | 5982 | `	if( pEntry == 0 ){` |
|         - | 5983 | `		/* Class not found in hash table — try autoload before giving up */` |
|       405 | 5984 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         - | 5985 | `	}` |
|   7780885 | 5986 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   7780885 | 5987 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   3890648 | 5988 | `}` |
|         - | 5989 | `/*` |
|         - | 5990 | ` * Reference Table Implementation` |
|         - | 5991 | ` * Status: stable <chm@symisc.net>` |
|         - | 5992 | ` * Intro` |
|         - | 5993 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 5994 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 5995 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 5996 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 5997 | ` *  Refer to the official for more information on this powerful` |
|         - | 5998 | ` *  extension.` |
|         - | 5999 | ` */` |
|         - | 6000 | `/*` |
|         - | 6001 | ` * Allocate a new reference entry.` |
|         - | 6002 | ` */` |
|  19211002 | 6003 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6004 | `{` |
|         - | 6005 | `	VmRefObj *pRef;` |
|         - | 6006 | `	/* Allocate a new instance */` |
|  19211007 | 6007 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  19211007 | 6008 | `	if( pRef == 0 ){` |
|       ! 0 | 6009 | `		return 0;` |
|         - | 6010 | `	}` |
|         - | 6011 | `	/* Zero the structure */` |
|  19211007 | 6012 | `	SyZero(pRef,sizeof(VmRefObj));` |
|         - | 6013 | `	/* Initialize fields */` |
|  19211007 | 6014 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  19211007 | 6015 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  19211007 | 6016 | `	pRef->nIdx = nIdx;` |
|  19211007 | 6017 | `	return pRef;` |
|   9606818 | 6018 | `}` |
|         - | 6019 | `/*` |
|         - | 6020 | ` * Default hash function used by the reference table` |
|         - | 6021 | ` * for lookup/insertion operations.` |
|         - | 6022 | ` */` |
| 103863120 | 6023 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|         5 | 6024 | `{` |
|         - | 6025 | `	/* Calculate the hash based on the memory object index */` |
| 103863125 | 6026 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|         5 | 6027 | `}` |
|         - | 6028 | `/*` |
|         - | 6029 | ` * Check if a memory object [i.e: a variable] is already installed` |
|         - | 6030 | ` * in the reference table.` |
|         - | 6031 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|         - | 6032 | ` * otherwise.` |
|         - | 6033 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6034 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6035 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6036 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6037 | ` * Refer to the official for more information on this powerful` |
|         - | 6038 | ` * extension.` |
|         - | 6039 | ` */` |
|  63836158 | 6040 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|         5 | 6041 | `{` |
|         - | 6042 | `	VmRefObj *pRef;` |
|         - | 6043 | `	sxu32 nBucket;` |
|         - | 6044 | `	/* Point to the appropriate bucket */` |
|  63836163 | 6045 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|         - | 6046 | `	/* Perform the lookup */` |
|  63836163 | 6047 | `	pRef = pVm->apRefObj[nBucket];` |
| 202921127 | 6048 | `	for(;;){` |
| 405855681 | 6049 | `		if( pRef == 0 ){` |
|  19211125 | 6050 | `			break;` |
|         - | 6051 | `		}` |
| 386644561 | 6052 | `		if( pRef->nIdx == nObjIdx ){` |
|         - | 6053 | `			/* Entry found */` |
|  44625043 | 6054 | `			return pRef;` |
|         - | 6055 | `		}` |
|         - | 6056 | `		/* Point to the next entry */` |
| 342019523 | 6057 | `		pRef = pRef->pNextCollide;` |
|         5 | 6058 | `	}` |
|         - | 6059 | `	/* No such entry,return NULL */` |
|  19211125 | 6060 | `	return 0;` |
|  31923336 | 6061 | `}` |
|         - | 6062 | `/*` |
|         - | 6063 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6064 | ` *` |
|         - | 6065 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6066 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6067 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6068 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6069 | ` * Refer to the official for more information on this powerful` |
|         - | 6070 | ` * extension.` |
|         - | 6071 | ` */` |
|  19211002 | 6072 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6073 | `{` |
|         - | 6074 | `	sxu32 nBucket;` |
|  19211007 | 6075 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|         - | 6076 | `		VmRefObj **apNew;` |
|         - | 6077 | `		sxu32 nNew;` |
|         - | 6078 | `		/* Allocate a larger table */` |
|     11671 | 6079 | `		nNew = pVm->nRefSize << 1;` |
|     11671 | 6080 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     11671 | 6081 | `		if( apNew ){` |
|     11671 | 6082 | `			VmRefObj *pEntry = pVm->pRefList;` |
|         - | 6083 | `			sxu32 n;` |
|         - | 6084 | `			/* Zero the structure */` |
|     11671 | 6085 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|         - | 6086 | `			/* Rehash all referenced entries */` |
|   4388565 | 6087 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|         - | 6088 | `				/* Remove old collision links */` |
|   4376899 | 6089 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 6090 | `				/* Point to the appropriate bucket */` |
|   4376899 | 6091 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|         - | 6092 | `				/* Insert the entry  */` |
|   4376899 | 6093 | `				pEntry->pNextCollide = apNew[nBucket];` |
|   4376899 | 6094 | `				if( apNew[nBucket] ){` |
|   3580803 | 6095 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|   1790399 | 6096 | `				}` |
|   4376899 | 6097 | `				apNew[nBucket] = pEntry;` |
|         - | 6098 | `				/* Point to the next entry */` |
|   4376899 | 6099 | `				pEntry = pEntry->pNext;` |
|   2188452 | 6100 | `			}` |
|         - | 6101 | `			/* Release the old table */` |
|     11671 | 6102 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|         - | 6103 | `			/* Install the new one */` |
|     11671 | 6104 | `			pVm->apRefObj = apNew;` |
|     11671 | 6105 | `			pVm->nRefSize = nNew;` |
|      5833 | 6106 | `		}` |
|      5833 | 6107 | `	}` |
|         - | 6108 | `	/* Point to the appropriate bucket */` |
|  19211007 | 6109 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|         - | 6110 | `	/* Insert the entry */` |
|  19211007 | 6111 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  19211007 | 6112 | `	if( pVm->apRefObj[nBucket] ){` |
|  17031590 | 6113 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   8516787 | 6114 | `	}` |
|  19211007 | 6115 | `	pVm->apRefObj[nBucket] = pRef;` |
|  19211007 | 6116 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  19211007 | 6117 | `	pVm->nRefUsed++;` |
|  19211007 | 6118 | `	return SXRET_OK;` |
|         5 | 6119 | `}` |
|         - | 6120 | `/*` |
|         - | 6121 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|         - | 6122 | ` * the reference table.` |
|         - | 6123 | ` * This function is invoked when the user perform an unset` |
|         - | 6124 | ` * call [i.e: unset($var); ].` |
|         - | 6125 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6126 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6127 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6128 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6129 | ` * Refer to the official for more information on this powerful` |
|         - | 6130 | ` * extension.` |
|         - | 6131 | ` */` |
|  18335826 | 6132 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6133 | `{` |
|         - | 6134 | `	ph7_hashmap_node **apNode;` |
|         - | 6135 | `	SyHashEntry **apEntry;` |
|         - | 6136 | `	sxu32 n;` |
|         - | 6137 | `	/* Point to the reference table */` |
|  18335831 | 6138 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  18335831 | 6139 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|         - | 6140 | `	/* Unlink the entry from the reference table */` |
|  19483267 | 6141 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1147441 | 6142 | `		if( apEntry[n] ){` |
|       175 | 6143 | `			SyHashDeleteEntry2(apEntry[n]);` |
|        87 | 6144 | `		}` |
|    575055 | 6145 | `	}` |
|  26022925 | 6146 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   7687099 | 6147 | `		if( apNode[n] ){` |
|       509 | 6148 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|       254 | 6149 | `		}` |
|   3843538 | 6150 | `	}` |
|  18335831 | 6151 | `	if( pRef->pPrevCollide ){` |
|   1896765 | 6152 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|    948191 | 6153 | `	}else{` |
|  16439071 | 6154 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|         - | 6155 | `	}` |
|  18335831 | 6156 | `	if( pRef->pNextCollide ){` |
|  15218165 | 6157 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   7610077 | 6158 | `	}` |
|  18335831 | 6159 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|         - | 6160 | `	/* Release the node */` |
|  18335831 | 6161 | `	SySetRelease(&pRef->aReference);` |
|  18335831 | 6162 | `	SySetRelease(&pRef->aArrEntries);` |
|  18335831 | 6163 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  18335831 | 6164 | `	pVm->nRefUsed--;` |
|  18335831 | 6165 | `	return SXRET_OK;` |
|         5 | 6166 | `}` |
|         - | 6167 | `/*` |
|         - | 6168 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6169 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6170 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6171 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6172 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6173 | ` * Refer to the official for more information on this powerful` |
|         - | 6174 | ` * extension.` |
|         - | 6175 | ` */` |
|  19277808 | 6176 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 6177 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6178 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6179 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6180 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 6181 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 6182 | `	)` |
|         5 | 6183 | `{` |
|  19277813 | 6184 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 6185 | `	VmRefObj *pRef;` |
|         - | 6186 | `	/* Check if the referenced object already exists */` |
|  19277813 | 6187 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  19277813 | 6188 | `	if( pRef == 0 ){` |
|         - | 6189 | `		/* Create a new entry */` |
|  19211007 | 6190 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  19211007 | 6191 | `		if( pRef == 0 ){` |
|       ! 0 | 6192 | `			return SXERR_MEM;` |
|         - | 6193 | `		}` |
|  19211007 | 6194 | `		pRef->iFlags = iFlags;` |
|         - | 6195 | `		/* Install the entry */` |
|  19211007 | 6196 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   9606813 | 6197 | `	}` |
|  19277813 | 6198 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  19277813 | 6199 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 6200 | `		VmSlot sRef;` |
|         - | 6201 | `		/* Local frame,record referenced entry so that it can` |
|         - | 6202 | `		 * be deleted when we leave this frame.` |
|         - | 6203 | `		 */` |
|   1141389 | 6204 | `		sRef.nIdx = nIdx;` |
|   1141389 | 6205 | `		sRef.pUserData = pEntry;` |
|   1141389 | 6206 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 6207 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 6208 | `		}` |
|    572024 | 6209 | `	}` |
|  19277813 | 6210 | `	if( pEntry ){` |
|         - | 6211 | `		/* Address of the hash-entry (into a row a dead holder left behind — a name can` |
|         - | 6212 | `		 * be RE-BOUND to the same slot any number of times, and a set that only ever` |
|         - | 6213 | `		 * grew made both the install and the holder count O(rows)) */` |
|   1204435 | 6214 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   1204435 | 6215 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   1204937 | 6216 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|       507 | 6217 | `			if( apEntry[n] == pEntry ){` |
|       ! 0 | 6218 | `				nFree = SXU32_HIGH; /* already recorded: never file one holder twice */` |
|       ! 0 | 6219 | `				break;` |
|         - | 6220 | `			}` |
|       507 | 6221 | `			if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|       182 | 6222 | `				nFree = n;` |
|        89 | 6223 | `			}` |
|       256 | 6224 | `		}` |
|   1204435 | 6225 | `		if( n >= SySetUsed(&pRef->aReference) ){` |
|   1204435 | 6226 | `			if( nFree != SXU32_HIGH ){` |
|       182 | 6227 | `				apEntry[nFree] = pEntry;` |
|        93 | 6228 | `			}else{` |
|   1204257 | 6229 | `				SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|         - | 6230 | `			}` |
|    603547 | 6231 | `		}` |
|    603547 | 6232 | `	}` |
|  19277813 | 6233 | `	if( pMapEntry ){` |
|         - | 6234 | `		/* Address of the hashmap node [i.e: Array entry] — same row reuse */` |
|   7827993 | 6235 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   7827993 | 6236 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   7828673 | 6237 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|       685 | 6238 | `			if( apNode[n] == pMapEntry ){` |
|       ! 0 | 6239 | `				nFree = SXU32_HIGH;` |
|       ! 0 | 6240 | `				break;` |
|         - | 6241 | `			}` |
|       685 | 6242 | `			if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|         3 | 6243 | `				nFree = n;` |
|         1 | 6244 | `			}` |
|       345 | 6245 | `		}` |
|   7827993 | 6246 | `		if( n >= SySetUsed(&pRef->aArrEntries) ){` |
|   7827993 | 6247 | `			if( nFree != SXU32_HIGH ){` |
|         3 | 6248 | `				apNode[nFree] = pMapEntry;` |
|         2 | 6249 | `			}else{` |
|   7827991 | 6250 | `				SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|         - | 6251 | `			}` |
|   3913974 | 6252 | `		}` |
|   3913974 | 6253 | `	}` |
|  19277813 | 6254 | `	return SXRET_OK;` |
|   9640221 | 6255 | `}` |
|         - | 6256 | `/*` |
|         - | 6257 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 6258 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6259 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6260 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6261 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6262 | ` * Refer to the official for more information on this powerful` |
|         - | 6263 | ` * extension.` |
|         - | 6264 | ` */` |
|   8821660 | 6265 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 6266 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6267 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6268 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6269 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 6270 | `	)` |
|         5 | 6271 | `{` |
|         - | 6272 | `	VmRefObj *pRef;` |
|         - | 6273 | `	sxu32 n;` |
|         - | 6274 | `	/* Check if the referenced object already exists */` |
|   8821665 | 6275 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   8821665 | 6276 | `	if( pRef == 0 ){` |
|         - | 6277 | `		/* Not such entry */` |
|         5 | 6278 | `		return SXERR_NOTFOUND;` |
|         - | 6279 | `	}` |
|         - | 6280 | `	/* Remove the desired entry */` |
|   8821661 | 6281 | `	if( pEntry ){` |
|         - | 6282 | `		SyHashEntry **apEntry;` |
|   1141345 | 6283 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   2282997 | 6284 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1141657 | 6285 | `			if( apEntry[n] == pEntry ){` |
|         - | 6286 | `				/* Nullify the entry */` |
|   1141343 | 6287 | `				apEntry[n] = 0;` |
|         - | 6288 | `				/*` |
|         - | 6289 | `				 * NOTE:` |
|         - | 6290 | `				 * In future releases,think to add a free pool of entries,so that` |
|         - | 6291 | `				 * we avoid wasting spaces.` |
|         - | 6292 | `				 */` |
|    572001 | 6293 | `			}` |
|    572163 | 6294 | `		}` |
|    572002 | 6295 | `	}` |
|   8821661 | 6296 | `	if( pMapEntry ){` |
|         - | 6297 | `		ph7_hashmap_node **apNode;` |
|   7680321 | 6298 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  15361517 | 6299 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   7681201 | 6300 | `			if( apNode[n] == pMapEntry ){` |
|         - | 6301 | `				/* nullify the entry */` |
|   7680321 | 6302 | `				apNode[n] = 0;` |
|   3840144 | 6303 | `			}` |
|   3840589 | 6304 | `		}` |
|   3840144 | 6305 | `	}` |
|   8821661 | 6306 | `	return SXRET_OK;` |
|   4412153 | 6307 | `}` |
|         - | 6308 | `/*` |
|         - | 6309 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|         - | 6310 | ` * names bound to it plus the array nodes pointing at it. php refcounts a` |
|         - | 6311 | ` * reference set and keeps the VALUE alive while any holder remains, so this is` |
|         - | 6312 | ` * the count every "may I release this slot?" decision asks for.` |
|         - | 6313 | ` *` |
|         - | 6314 | ` * A row is only a holder while it is non-NULL (every holder's death nullifies` |
|         - | 6315 | ` * its own row through PH7_VmRefObjRemove) and, for a node, while it still points` |
|         - | 6316 | ` * HERE — a slot index travels through the free list, so a record can outlive the` |
|         - | 6317 | ` * node that filed the row (the same filter VmUnsetVarByName applies).` |
|         - | 6318 | ` */` |
|   9704286 | 6319 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6320 | `{` |
|         - | 6321 | `	ph7_hashmap_node **apNode;` |
|         - | 6322 | `	SyHashEntry **apEntry;` |
|         - | 6323 | `	VmRefObj *pRef;` |
|   9704291 | 6324 | `	sxu32 n, nLive = 0;` |
|   9704291 | 6325 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6326 | `		return 0;` |
|         - | 6327 | `	}` |
|   9704291 | 6328 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   9704291 | 6329 | `	if( pRef == 0 ){` |
|         3 | 6330 | `		return 0;` |
|         - | 6331 | `	}` |
|   9704289 | 6332 | `	if( pRef->nPin > 0 ){` |
|         - | 6333 | `		/* Holders the table cannot name, counted: reference-bound properties. */` |
|        13 | 6334 | `		nLive += pRef->nPin;` |
|   9704283 | 6335 | `	}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|         - | 6336 | ``		/* A permanent pin — a `use (&$x)` capture, a static, an enum case. It is the`` |
|         - | 6337 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|         - | 6338 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|         - | 6339 | `		 * php's refcount does. */` |
|         5 | 6340 | `		nLive++;` |
|         2 | 6341 | `	}` |
|   9704289 | 6342 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|  10855177 | 6343 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|   1150893 | 6344 | `		if( apEntry[n] ){` |
|       248 | 6345 | `			nLive++;` |
|       122 | 6346 | `		}` |
|    576781 | 6347 | `	}` |
|   9704289 | 6348 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  18272233 | 6349 | `	for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   8567949 | 6350 | `		if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|    880689 | 6351 | `			nLive++;` |
|    440342 | 6352 | `		}` |
|   4283963 | 6353 | `	}` |
|   9704289 | 6354 | `	return nLive;` |
|   4853466 | 6355 | `}` |
|         - | 6356 | `/*` |
|         - | 6357 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|         - | 6358 | ` * holds it (php frees the value with the last reference, not with the first one` |
|         - | 6359 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|         - | 6360 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|         - | 6361 | ` */` |
|   7689946 | 6362 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6363 | `{` |
|         - | 6364 | `	VmRefObj *pRef;` |
|   7689951 | 6365 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6366 | `		return;` |
|         - | 6367 | `	}` |
|   7689951 | 6368 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   7689951 | 6369 | `	if( pRef ){` |
|   7689907 | 6370 | `		if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|        57 | 6371 | `			return; /* pinned past its frame — its holder is not in the table */` |
|         - | 6372 | `		}` |
|   7689851 | 6373 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|      3514 | 6374 | `			return; /* somebody still holds it */` |
|         - | 6375 | `		}` |
|   3843154 | 6376 | `	}` |
|         - | 6377 | `	/* No record at all means nothing was ever registered against the slot, which is` |
|         - | 6378 | `	 * the same answer as a count of zero — release it (this is what every caller did` |
|         - | 6379 | `	 * unconditionally before the holder rule). */` |
|   7686385 | 6380 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|         - | 6381 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|         - | 6382 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
|   7686385 | 6383 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|   3844964 | 6384 | `}` |
|         - | 6385 | `/*` |
|         - | 6386 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|         - | 6387 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|         - | 6388 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|         - | 6389 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|         - | 6390 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|         - | 6391 | ` */` |
|      9652 | 6392 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|         5 | 6393 | `{` |
|         - | 6394 | `	VmFrame *pFrame;` |
|     22405 | 6395 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     12753 | 6396 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     12753 | 6397 | `		sxu32 n = 0;` |
|     25073 | 6398 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|     12322 | 6399 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|         - | 6400 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|         - | 6401 | `				 * same index — it now holds the row swapped in from the tail. */` |
|      3094 | 6402 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|      3094 | 6403 | `				(void)SySetPop(&pFrame->sRef);` |
|      3094 | 6404 | `				continue;` |
|         - | 6405 | `			}` |
|      9230 | 6406 | `			n++;` |
|         2 | 6407 | `		}` |
|      6379 | 6408 | `	}` |
|      9657 | 6409 | `}` |
|         - | 6410 | `/*` |
|         - | 6411 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|         - | 6412 | ` *` |
|         - | 6413 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|         - | 6414 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|         - | 6415 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|         - | 6416 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|         - | 6417 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|         - | 6418 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|         - | 6419 | ` */` |
|     19736 | 6420 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|         5 | 6421 | `{` |
|     19741 | 6422 | `	return nByte > 0 && zName[0] == '[';` |
|         5 | 6423 | `}` |
|         - | 6424 | `/*` |
|         - | 6425 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|         - | 6426 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|         - | 6427 | ` * its value variable on every step, and it goes through the reference table like any` |
|         - | 6428 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|         - | 6429 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|         - | 6430 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|         - | 6431 | ` */` |
|       448 | 6432 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 6433 | `	sxu32 nIdx)` |
|         5 | 6434 | `{` |
|       453 | 6435 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       453 | 6436 | `	if( pEntry ){` |
|        96 | 6437 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|        96 | 6438 | `		return;` |
|         - | 6439 | `	}` |
|       361 | 6440 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|       ! 0 | 6441 | `		return;` |
|         - | 6442 | `	}` |
|       361 | 6443 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|         - | 6444 | `		/* A global is also an entry of the $GLOBALS view */` |
|        42 | 6445 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        19 | 6446 | `	}` |
|       361 | 6447 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       229 | 6448 | `}` |
|         - | 6449 | `/*` |
|         - | 6450 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|         - | 6451 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|         - | 6452 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|         - | 6453 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|         - | 6454 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|         - | 6455 | ` * and left the OLD binding standing, so every later write through the name went to` |
|         - | 6456 | ` * the wrong variable.` |
|         - | 6457 | ` *` |
|         - | 6458 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|         - | 6459 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|         - | 6460 | ` * compares it by identity.` |
|         - | 6461 | ` */` |
|      3176 | 6462 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|         - | 6463 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 6464 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|         - | 6465 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|         - | 6466 | `	const char *zName,    /* Variable name */` |
|         - | 6467 | `	sxu32 nByte,          /* Name length */` |
|         - | 6468 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|         - | 6469 | `	)` |
|         4 | 6470 | `{` |
|      3180 | 6471 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      3180 | 6472 | `	if( nOld == nIdx ){` |
|         - | 6473 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|       ! 0 | 6474 | `		return;` |
|         - | 6475 | `	}` |
|         - | 6476 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|         - | 6477 | `	 * "release this reference at exit" set */` |
|      3180 | 6478 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|      3180 | 6479 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|      3180 | 6480 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|      3180 | 6481 | `	if( pFrame->pParent == 0 ){` |
|         - | 6482 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|         - | 6483 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|         - | 6484 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|         - | 6485 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|       106 | 6486 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|       102 | 6487 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|       106 | 6488 | `		 && pGlobalNode ){` |
|       106 | 6489 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|       106 | 6490 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|       106 | 6491 | `				pGlobalNode->nValIdx = nIdx;` |
|       106 | 6492 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|        51 | 6493 | `			}` |
|        55 | 6494 | `		}else{` |
|         - | 6495 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|       ! 0 | 6496 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|         - | 6497 | `		}` |
|        51 | 6498 | `	}` |
|      3180 | 6499 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|         - | 6500 | `	/* The old value dies with its last holder — and only then */` |
|      3180 | 6501 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|      1592 | 6502 | `}` |
|         - | 6503 | `/*` |
|         - | 6504 | ` * Is there a SCHEME at the front of this name, and how long is it?` |
|         - | 6505 | ` *` |
|         - | 6506 | ` * php reads one only at the START, and only as a URL scheme: a run of` |
|         - | 6507 | ` * [A-Za-z0-9+.-] at least TWO characters long, followed immediately by "://".` |
|         - | 6508 | ` * PHL used to hunt for the first "://" ANYWHERE in the name and then trim` |
|         - | 6509 | ` * whitespace off whatever preceded it, which made four ordinary FILENAMES into` |
|         - | 6510 | ` * URLs: " php://memory" and "php ://memory" opened the memory stream php opens` |
|         - | 6511 | ` * a file called that, "./sub://z" and "a b://c" were looked up as schemes` |
|         - | 6512 | ` * "./sub" and "a b". The two-character minimum is php's, and it is what keeps a` |
|         - | 6513 | ` * Windows drive letter ("C://tmp") a path rather than a "C" scheme.` |
|         - | 6514 | ` */` |
|    102651 | 6515 | `static int VmUrlScheme(const char *zIn,int nByte,int *pnScheme)` |
|         5 | 6516 | `{` |
|    102656 | 6517 | `	int i = 0;` |
|    595868 | 6518 | `	while( i < nByte ){` |
|    595826 | 6519 | `		int c = zIn[i];` |
|    595821 | 6520 | `		if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9')` |
|    103026 | 6521 | `		 \|\| c == '+' \|\| c == '-' \|\| c == '.' ){` |
|    493217 | 6522 | `			i++;` |
|    493217 | 6523 | `			continue;` |
|         - | 6524 | `		}` |
|    102614 | 6525 | `		break;` |
|       ! 0 | 6526 | `	}` |
|         - | 6527 | `	/* php also accepts a scheme with NOTHING after it ("zzz://" is an unknown` |
|         - | 6528 | `	 * wrapper, not a file called "zzz://"), which the old scan refused. */` |
|    102656 | 6529 | `	if( i > 1 && i + 2 < nByte && zIn[i] == ':' && zIn[i+1] == '/' && zIn[i+2] == '/' ){` |
|       623 | 6530 | `		*pnScheme = i;` |
|       623 | 6531 | `		return 1;` |
|         - | 6532 | `	}` |
|    102038 | 6533 | `	return 0;` |
|     51347 | 6534 | `}` |
|         - | 6535 | `/*` |
|         - | 6536 | ` * The same question from outside vm.c: how long is the scheme, or 0 for a name` |
|         - | 6537 | ` * that has none. stream_resolve_include_path() asks it to decide whether a name` |
|         - | 6538 | ` * is walkable at all.` |
|         - | 6539 | ` */` |
|        52 | 6540 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte)` |
|         2 | 6541 | `{` |
|        54 | 6542 | `	int nScheme = 0;` |
|        54 | 6543 | `	if( zIn == 0 ){` |
|       ! 0 | 6544 | `		return 0;` |
|         - | 6545 | `	}` |
|        54 | 6546 | `	if( nByte < 0 ){` |
|        25 | 6547 | `		nByte = (int)SyStrlen(zIn);` |
|        12 | 6548 | `	}` |
|        54 | 6549 | `	return VmUrlScheme(zIn,nByte,&nScheme) ? nScheme : 0;` |
|        28 | 6550 | `}` |
|         - | 6551 | `/*` |
|         - | 6552 | ` * The bytes the FILE wrapper is handed for a file:// URL.` |
|         - | 6553 | ` *` |
|         - | 6554 | ` * A file:// URL has an AUTHORITY, and php only accepts two of them: an empty` |
|         - | 6555 | `` * one and `localhost` (case-insensitively, and only with its slash). Anything`` |
|         - | 6556 | ` * else is a remote host it refuses to reach -- where PHL stripped exactly` |
|         - | 6557 | `` * "file://" and opened whatever was left, so `file://tmp/passwd` silently read`` |
|         - | 6558 | ` * the RELATIVE path tmp/passwd. What survives the strip is the LAST slash of` |
|         - | 6559 | `` * the leading run, so `file:////x` is /x, `file://localhost//x` is /x, and`` |
|         - | 6560 | `` * `file://` on its own is the root directory.`` |
|         - | 6561 | ` *` |
|         - | 6562 | ` * Returns 0 for an authority php will not reach; the caller answers "no` |
|         - | 6563 | ` * wrapper", as php does.` |
|         - | 6564 | ` */` |
|        56 | 6565 | `static int VmFileUrlPath(const char *zIn,int nByte,int nScheme,const char **pzPath)` |
|         4 | 6566 | `{` |
|         - | 6567 | `	static const char zLocal[] = "file://localhost/";` |
|        60 | 6568 | `	const char *zPath = &zIn[nScheme+1]; /* the first slash of "://" */` |
|        60 | 6569 | `	const char *zEnd = &zIn[nByte];` |
|        56 | 6570 | `	if( nScheme + 3 < nByte && zIn[nScheme+3] != '/'` |
|         - | 6571 | `#ifdef __WINNT__` |
|         - | 6572 | ``	 /* php's own Windows allowance: `file://C:/x` is a DRIVE, not a host. */`` |
|         4 | 6573 | `	 && !(nScheme + 4 < nByte && zIn[nScheme+4] == ':')` |
|         - | 6574 | `#endif` |
|         - | 6575 | `	){` |
|        32 | 6576 | `		if( nByte < (int)sizeof(zLocal)-1` |
|        35 | 6577 | `		 \|\| SyStrnicmp(zIn,zLocal,(sxu32)sizeof(zLocal)-1) != 0 ){` |
|        31 | 6578 | `			return 0; /* a host this build (and php) will not fetch from */` |
|         - | 6579 | `		}` |
|         4 | 6580 | `		zPath = &zIn[nScheme+3+sizeof("localhost")-1];` |
|         2 | 6581 | `	}` |
|        80 | 6582 | `	while( &zPath[1] < zEnd && zPath[1] == '/' ){` |
|        52 | 6583 | `		zPath++;` |
|         2 | 6584 | `	}` |
|        30 | 6585 | `	*pzPath = zPath;` |
|        30 | 6586 | `	return 1;` |
|        32 | 6587 | `}` |
|         - | 6588 | `/*` |
|         - | 6589 | ` * The same rule for the VFS side, which stats and unlinks a name without ever` |
|         - | 6590 | ` * going through a stream device. It carried a second, shorter copy of the` |
|         - | 6591 | `` * strip -- no slash-run collapse and nothing for a bare `file://` -- so`` |
|         - | 6592 | `` * `is_dir('file://')` was false where php names the root. A host this build`` |
|         - | 6593 | ` * will not reach is handed back UNCHANGED: the syscall then fails on a name` |
|         - | 6594 | ` * that is not a path, which is the FALSE php answers for it.` |
|         - | 6595 | ` */` |
|     65836 | 6596 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath)` |
|         5 | 6597 | `{` |
|         - | 6598 | `	const char *zOut;` |
|     65841 | 6599 | `	int nByte,nScheme = 0;` |
|     65841 | 6600 | `	if( zPath == 0 ){` |
|       ! 0 | 6601 | `		return zPath;` |
|         - | 6602 | `	}` |
|     65841 | 6603 | `	nByte = (int)SyStrlen(zPath);` |
|     65836 | 6604 | `	if( !VmUrlScheme(zPath,nByte,&nScheme)` |
|     32948 | 6605 | `	 \|\| nScheme != (int)sizeof("file")-1` |
|        30 | 6606 | `	 \|\| SyStrnicmp(zPath,"file",sizeof("file")-1) != 0 ){` |
|     65819 | 6607 | `		return zPath;` |
|         - | 6608 | `	}` |
|        25 | 6609 | `	if( !VmFileUrlPath(zPath,nByte,nScheme,&zOut) ){` |
|         6 | 6610 | `		return zPath;` |
|         - | 6611 | `	}` |
|         - | 6612 | `#ifdef __WINNT__` |
|         - | 6613 | `	/* The one piece that is only true here: the leading slash php's own strip` |
|         - | 6614 | `	 * leaves in front of a DRIVE is not part of a Windows path, so` |
|         - | 6615 | `	 * file:///C:/x and file://localhost/C:/x both name C:/x. */` |
|         2 | 6616 | `	if( zOut[0] == '/' && zOut[1] != 0 && zOut[2] == ':' ){` |
|         2 | 6617 | `		zOut++;` |
|         - | 6618 | `	}` |
|         - | 6619 | `#endif` |
|        20 | 6620 | `	return zOut;` |
|     32940 | 6621 | `}` |
|         - | 6622 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 6623 | `/*` |
|         - | 6624 | ` * Has a script taken this device out of service with stream_wrapper_unregister()?` |
|         - | 6625 | ` */` |
|     36955 | 6626 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|         5 | 6627 | `{` |
|     36960 | 6628 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|         - | 6629 | `	sxu32 n;` |
|     37050 | 6630 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|       133 | 6631 | `		if( apOff[n] == pStream ){` |
|        43 | 6632 | `			return 1;` |
|         - | 6633 | `		}` |
|        46 | 6634 | `	}` |
|     36918 | 6635 | `	return 0;` |
|     18482 | 6636 | `}` |
|         - | 6637 | `/*` |
|         - | 6638 | ` * The device currently answering to a scheme name, or NULL. The scan runs to` |
|         - | 6639 | ` * the END rather than stopping at the first hit: once a built-in has been` |
|         - | 6640 | ` * unregistered a userland wrapper can be registered under the same name, both` |
|         - | 6641 | ` * sit in the list, and the LIVE one is the later of the two.` |
|         - | 6642 | ` */` |
|     36727 | 6643 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName)` |
|         5 | 6644 | `{` |
|     36732 | 6645 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     36732 | 6646 | `	ph7_io_stream *pHit = 0;` |
|     36732 | 6647 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|     36732 | 6648 | `	if( nName < 0 ){` |
|       ! 0 | 6649 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 6650 | `	}` |
|    183764 | 6651 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|    147037 | 6652 | `		ph7_io_stream *pStream = apStream[n];` |
|    147032 | 6653 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|    110248 | 6654 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|    110310 | 6655 | `			continue;` |
|         - | 6656 | `		}` |
|     36732 | 6657 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|        15 | 6658 | `			continue;` |
|         - | 6659 | `		}` |
|     36718 | 6660 | `		pHit = pStream;` |
|     18361 | 6661 | `	}` |
|     36732 | 6662 | `	return pHit;` |
|         5 | 6663 | `}` |
|         - | 6664 | `/*` |
|         - | 6665 | ` * Is this scheme one the build HAS but the script has switched off? php words` |
|         - | 6666 | ` * that differently from a scheme nothing was ever registered under -- but only` |
|         - | 6667 | ` * for file://, whose plain-files fallback is the branch that reports it.` |
|         - | 6668 | ` */` |
|         2 | 6669 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName)` |
|         1 | 6670 | `{` |
|         3 | 6671 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|         3 | 6672 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|         3 | 6673 | `	int bSeen = 0;` |
|         3 | 6674 | `	if( nName < 0 ){` |
|       ! 0 | 6675 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 6676 | `	}` |
|        13 | 6677 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        11 | 6678 | `		ph7_io_stream *pStream = apStream[n];` |
|        10 | 6679 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|         8 | 6680 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|         9 | 6681 | `			continue;` |
|         - | 6682 | `		}` |
|         3 | 6683 | `		if( !PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|       ! 0 | 6684 | `			return 0; /* it is live */` |
|         - | 6685 | `		}` |
|         3 | 6686 | `		bSeen = 1;` |
|         2 | 6687 | `	}` |
|         3 | 6688 | `	return bSeen;` |
|         2 | 6689 | `}` |
|         - | 6690 | `/*` |
|         - | 6691 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 6692 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 6693 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 6694 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 6695 | ` * For more information on how to register IO stream devices,please` |
|         - | 6696 | ` * refer to the official documentation.` |
|         - | 6697 | ` */` |
|     36739 | 6698 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 6699 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 6700 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 6701 | `	int nByte              /* *pzDevice length*/` |
|         - | 6702 | `	)` |
|         5 | 6703 | `{` |
|         - | 6704 | `	const char *zIn,*zNext;` |
|         - | 6705 | `	ph7_io_stream *pStream;` |
|     36744 | 6706 | `	int nScheme = 0;` |
|         - | 6707 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|     36744 | 6708 | `	zIn = *pzDevice;` |
|     36744 | 6709 | `	if( !VmUrlScheme(zIn,nByte,&nScheme) ){` |
|         - | 6710 | `		/* No scheme: php's default is the plain-files wrapper, and it is the` |
|         - | 6711 | `		 * SAME slot file:// names -- so a script that unregisters file:// loses` |
|         - | 6712 | `		 * the bare-path open too, and one that registers its own wrapper over` |
|         - | 6713 | `		 * file:// gets bare paths routed through it. Looking the name up rather` |
|         - | 6714 | `		 * than answering pDefStream is what makes both true. */` |
|     36186 | 6715 | `		return PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|         - | 6716 | `	}` |
|       563 | 6717 | `	zNext = &zIn[nScheme+sizeof("://")-1];` |
|         - | 6718 | `	/* php applies the file:// authority rules by the SCHEME NAME, before it` |
|         - | 6719 | `	 * cares who is registered under it -- a userland wrapper that replaced` |
|         - | 6720 | `	 * file:// is handed the stripped path too. */` |
|       563 | 6721 | `	if( nScheme == (int)sizeof("file")-1 && SyStrnicmp(zIn,"file",sizeof("file")-1) == 0 ){` |
|        24 | 6722 | `		if( !VmFileUrlPath(zIn,nByte,nScheme,&zNext) ){` |
|        13 | 6723 | `			return 0;` |
|         - | 6724 | `		}` |
|         5 | 6725 | `	}` |
|       551 | 6726 | `	pStream = PH7_VmFindStreamDevice(pVm,zIn,nScheme);` |
|       551 | 6727 | `	if( pStream == 0 ){` |
|         - | 6728 | `		/* No such stream -- or one a script has taken out of service. */` |
|        15 | 6729 | `		return 0;` |
|         - | 6730 | `	}` |
|       539 | 6731 | `	*pzDevice = zNext;` |
|       539 | 6732 | `	return pStream;` |
|     18374 | 6733 | `}` |
|         - | 6734 | `/*` |
|         - | 6735 | ` * Why did PH7_VmGetStreamDevice() answer nothing for this name? php raises a` |
|         - | 6736 | ` * REASON of its own before the operation's own failure, and the two a caller` |
|         - | 6737 | ` * can hit here are different sentences. Re-derived from the name rather than` |
|         - | 6738 | ` * threaded out of the lookup, so every call site stays one line.` |
|         - | 6739 | ` *` |
|         - | 6740 | ` * Answers TRUE for the file:// authority php will not reach; otherwise FALSE` |
|         - | 6741 | ` * with *pnScheme set to the length of the scheme that has no wrapper.` |
|         - | 6742 | ` */` |
|        24 | 6743 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme)` |
|         2 | 6744 | `{` |
|         - | 6745 | `	const char *zPath;` |
|        26 | 6746 | `	int nScheme = 0;` |
|        26 | 6747 | `	if( nByte < 0 ){` |
|        26 | 6748 | `		nByte = (int)SyStrlen(zUri);` |
|        12 | 6749 | `	}` |
|        26 | 6750 | `	if( !VmUrlScheme(zUri,nByte,&nScheme) ){` |
|         3 | 6751 | `		*pnScheme = 0;` |
|         3 | 6752 | `		return 0;` |
|         - | 6753 | `	}` |
|        24 | 6754 | `	*pnScheme = nScheme;` |
|        31 | 6755 | `	return nScheme == (int)sizeof("file")-1` |
|        18 | 6756 | `		&& SyStrnicmp(zUri,"file",sizeof("file")-1) == 0` |
|        29 | 6757 | `		&& !VmFileUrlPath(zUri,nByte,nScheme,&zPath);` |
|        14 | 6758 | `}` |
|         - | 6759 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 6760 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 6761 |  |
