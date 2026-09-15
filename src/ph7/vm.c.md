# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2334/2796 lines (83.48%)

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
|  1280793 |   76 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   77 | `{` |
|  1280798 |   78 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       40 |   79 | `		return TRUE;` |
|        - |   80 | `	}` |
|  1280760 |   81 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   82 | `		return TRUE;` |
|        - |   83 | `	}` |
|  1280750 |   84 | `	return FALSE;` |
|   640914 |   85 | `}` |
|        - |   86 | `/*` |
|        - |   87 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   88 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   89 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   90 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   91 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   92 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   93 | ` * still go through the existing numeric coercion.` |
|        - |   94 | ` */` |
|   367035 |   95 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   96 | `{` |
|        - |   97 | `	SyString sStr;` |
|   367040 |   98 | `	sxu8 bReal = FALSE;` |
|   367040 |   99 | `	const char *zTail = 0;` |
|        - |  100 | `	const char *zEnd;` |
|   367040 |  101 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   367026 |  102 | `		return FALSE;` |
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
|   183625 |  119 | `}` |
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
|  1519964 |  137 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |  138 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  139 | `	const SyString *pName,  /* Constant name */` |
|        - |  140 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |  141 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |  142 | `	)` |
|        5 |  143 | `{` |
|  1519969 |  144 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|        5 |  145 | `}` |
|        - |  146 | `/*` |
|        - |  147 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|        - |  148 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|        - |  149 | ` */` |
|  1520016 |  150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|  1520021 |  164 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|  1520021 |  165 | `	if( pEntry ){` |
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
|  1520019 |  189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|  1520019 |  190 | `	if( pCons == 0 ){` |
|      ! 0 |  191 | `		return 0;` |
|        - |  192 | `	}` |
|        - |  193 | `	/* Duplicate constant name */` |
|  1520019 |  194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1520019 |  195 | `	if( zDupName == 0 ){` |
|      ! 0 |  196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  197 | `		return 0;` |
|        - |  198 | `	}` |
|  1520019 |  199 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|  1520019 |  200 | `	if( pFile ){` |
|       54 |  201 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       25 |  202 | `	}` |
|  1520019 |  203 | `	pCons->nLine = nLine;` |
|  1520019 |  204 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|        - |  205 | `	/* Install the constant */` |
|  1520019 |  206 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|  1520019 |  207 | `	pCons->xExpand = xExpand;` |
|  1520019 |  208 | `	pCons->pUserData = pUserData;` |
|  1520019 |  209 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1520019 |  210 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|  1520019 |  211 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  212 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |  213 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |  214 | `		return rc;` |
|        - |  215 | `	}` |
|        - |  216 | `	/* All done,constant can be invoked from PHP code */` |
|  1520019 |  217 | `	return SXRET_OK;` |
|   760013 |  218 | `}` |
|        - |  219 | `/*` |
|        - |  220 | ` * Allocate a new foreign function instance.` |
|        - |  221 | ` * This function return SXRET_OK on success. Any other` |
|        - |  222 | ` * return value indicates failure.` |
|        - |  223 | ` * Please refer to the official documentation for an introduction to` |
|        - |  224 | ` * the foreign function mechanism.` |
|        - |  225 | ` */` |
|  2225184 |  226 | `static sxi32 PH7_NewForeignFunction(` |
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
|  2225189 |  237 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  2225189 |  238 | `	if( pFunc == 0 ){` |
|      ! 0 |  239 | `		return SXERR_MEM;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate function name */` |
|  2225189 |  242 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2225189 |  243 | `	if( zDup == 0 ){` |
|      ! 0 |  244 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  245 | `		return SXERR_MEM;` |
|        - |  246 | `	}` |
|        - |  247 | `	/* Zero the structure */` |
|  2225189 |  248 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |  249 | `	/* Initialize structure fields */` |
|  2225189 |  250 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  2225189 |  251 | `	pFunc->pVm   = pVm;` |
|  2225189 |  252 | `	pFunc->xFunc = xFunc;` |
|  2225189 |  253 | `	pFunc->pUserData = pUserData;` |
|  2225189 |  254 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  255 | `	/* Write a pointer to the new function */` |
|  2225189 |  256 | `	*ppOut = pFunc;` |
|  2225189 |  257 | `	return SXRET_OK;` |
|  1112597 |  258 | `}` |
|        - |  259 | `/*` |
|        - |  260 | ` * Install a foreign function and it's associated callback so that` |
|        - |  261 | ` * it can be invoked from the target PHP code.` |
|        - |  262 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |  263 | ` * return value indicates failure.` |
|        - |  264 | ` * Please refer to the official documentation for an introduction to` |
|        - |  265 | ` * the foreign function mechanism.` |
|        - |  266 | ` */` |
|  2228592 |  267 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  2228597 |  278 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  2228597 |  279 | `	if( pEntry ){` |
|     3413 |  280 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     3413 |  281 | `		pFunc->pUserData = pUserData;` |
|     3413 |  282 | `		pFunc->xFunc = xFunc;` |
|     3413 |  283 | `		SySetReset(&pFunc->aAux);` |
|        - |  284 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|        - |  285 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|        - |  286 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|        - |  287 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|     3413 |  288 | `		pFunc->nMinArg  = 0;` |
|     3413 |  289 | `		pFunc->nMaxArg  = 0;` |
|     3413 |  290 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|     3413 |  291 | `		pFunc->bAtLeast = 0;` |
|     3413 |  292 | `		return SXRET_OK;` |
|        - |  293 | `	}` |
|        - |  294 | `	/* Create a new user function */` |
|  2225189 |  295 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  2225189 |  296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  297 | `		return rc;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* Install the function in the corresponding hashtable */` |
|  2225189 |  300 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  2225189 |  301 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  302 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |  303 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |  304 | `		return rc;` |
|        - |  305 | `	}` |
|        - |  306 | `	/* User function successfully installed */` |
|  2225189 |  307 | `	return SXRET_OK;` |
|  1114301 |  308 | `}` |
|        - |  309 | `/*` |
|        - |  310 | ` * Initialize a VM function.` |
|        - |  311 | ` */` |
|  3415428 |  312 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |  313 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  314 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |  315 | `	const char *zName,  /* Function name */` |
|        - |  316 | `	sxu32 nByte,        /* zName length */` |
|        - |  317 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |  318 | `	void *pUserData     /* Function private data */` |
|        - |  319 | `	)` |
|        5 |  320 | `{` |
|        - |  321 | `	/* Zero the structure */` |
|  3415433 |  322 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |  323 | `	/* Initialize structure fields */` |
|        - |  324 | `	/* Arguments container */` |
|  3415433 |  325 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |  326 | `	/* Static variable container */` |
|  3415433 |  327 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |  328 | `	/* Bytecode container */` |
|  3415433 |  329 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |  330 | `    /* Preallocate some instruction slots */` |
|  3415433 |  331 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |  332 | `	/* Closure environment */` |
|  3415433 |  333 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |  334 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|  3415433 |  335 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  336 | `	/* Declared #[...] attributes */` |
|  3415433 |  337 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  3415433 |  338 | `	pFunc->iFlags = iFlags;` |
|  3415433 |  339 | `	pFunc->pUserData = pUserData;` |
|        - |  340 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |  341 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|  3415433 |  342 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|  3415433 |  343 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  344 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|  3410885 |  345 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|  1705445 |  346 | `	}else{` |
|        - |  347 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|        - |  348 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|        - |  349 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     4553 |  350 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     4553 |  351 | `		if( pFile ){` |
|     4553 |  352 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|     2274 |  353 | `		}` |
|        - |  354 | `	}` |
|  3415433 |  355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|  3415433 |  356 | `	return SXRET_OK;` |
|        5 |  357 | `}` |
|        - |  358 | `/*` |
|        - |  359 | ` * Namespace-aware function lookup.` |
|        - |  360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |  361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |  362 | ` */` |
|        - |  363 | `/*` |
|        - |  364 | ` * Install a user defined function in the corresponding VM container.` |
|        - |  365 | ` */` |
|  5800592 |  366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |  367 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  368 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |  369 | `	SyString *pName     /* Function name */` |
|        - |  370 | `	)` |
|        5 |  371 | `{` |
|        - |  372 | `	SyHashEntry *pEntry;` |
|        - |  373 | `	sxi32 rc;` |
|  5800597 |  374 | `	if( pName == 0 ){` |
|        - |  375 | `		/* Use the built-in name */` |
|   521931 |  376 | `		pName = &pFunc->sName;` |
|   260963 |  377 | `	}` |
|        - |  378 | `	/* Check for duplicates (functions with the same name) first */` |
|  5800597 |  379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  5800597 |  380 | `	if( pEntry ){` |
|  2858569 |  381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  2858569 |  382 | `		if( pLink != pFunc ){` |
|        - |  383 | `			/* Link */` |
|       35 |  384 | `			pFunc->pNextName = pLink;` |
|       35 |  385 | `			pEntry->pUserData = pFunc;` |
|       16 |  386 | `		}` |
|  2858569 |  387 | `		return SXRET_OK;` |
|        - |  388 | `	}` |
|        - |  389 | `	/* First time seen */` |
|  2942033 |  390 | `	pFunc->pNextName = 0;` |
|  2942033 |  391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|  2942033 |  392 | `	return rc;` |
|  2900301 |  393 | `}` |
|        - |  394 | `/*` |
|        - |  395 | ` * Install a user defined class in the corresponding VM container.` |
|        - |  396 | ` */` |
|   521530 |  397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |  398 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |  399 | `	ph7_class *pClass /* Target Class */` |
|        - |  400 | `	)` |
|        5 |  401 | `{` |
|   521535 |  402 | `	SyString *pName = &pClass->sName;` |
|        - |  403 | `	SyHashEntry *pEntry;` |
|        - |  404 | `	sxi32 rc;` |
|        - |  405 | `	/* Check for duplicates */` |
|   521535 |  406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   521535 |  407 | `	if( pEntry ){` |
|        3 |  408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |  409 | `		/* Link entry with the same name */` |
|        3 |  410 | `		pClass->pNextName = pLink;` |
|        3 |  411 | `		pEntry->pUserData = pClass;` |
|        3 |  412 | `		return SXRET_OK;` |
|        - |  413 | `	}` |
|   521533 |  414 | `	pClass->pNextName = 0;` |
|        - |  415 | `	/* Perform a simple hashtable insertion */` |
|   521533 |  416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   521533 |  417 | `	return rc;` |
|   260770 |  418 | `}` |
|        - |  419 | `/*` |
|        - |  420 | ` * Instruction builder interface.` |
|        - |  421 | ` */` |
| 98298838 |  422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |  423 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |  424 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |  425 | `	sxi32 iP1,    /* First operand */` |
|        - |  426 | `	sxu32 iP2,    /* Second operand */` |
|        - |  427 | `	void *p3,     /* Third operand */` |
|        - |  428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |  429 | `	)` |
|        5 |  430 | `{` |
|        - |  431 | `	VmInstr sInstr;` |
| 98298843 |  432 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - |  433 | `	sxi32 rc;` |
|        - |  434 | `	/* Fill the VM instruction */` |
| 98298843 |  435 | `	sInstr.iOp = (sxu8)iOp;` |
| 98298843 |  436 | `	sInstr.iP1 = iP1;` |
| 98298843 |  437 | `	sInstr.iP2 = iP2;` |
| 98298843 |  438 | `	sInstr.p3  = p3;` |
|        - |  439 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|        - |  440 | `	 * compiled (that is how they read its text), so the current token IS this` |
|        - |  441 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|        - |  442 | `	 * between statements, hence the range check. */` |
| 98298843 |  443 | `	sInstr.nLine = 0;` |
| 98298843 |  444 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
| 41880347 |  445 | `		sInstr.nLine = pGen->pIn->nLine;` |
| 77358672 |  446 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|        - |  447 | `		/* Past the end (statement tail): blame the last real token. */` |
| 56203997 |  448 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
| 28101996 |  449 | `	}` |
| 98298843 |  450 | `	if( pIndex ){` |
|        - |  451 | `		/* Instruction index in the bytecode array */` |
|  6654271 |  452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|  3327133 |  453 | `	}` |
|        - |  454 | `	/* Finally,record the instruction */` |
| 98298843 |  455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 98298843 |  456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |  458 | `		/* Fall throw */` |
|      ! 0 |  459 | `	}` |
| 98298843 |  460 | `	return rc;` |
|        5 |  461 | `}` |
|        - |  462 | `/*` |
|        - |  463 | ` * Swap the current bytecode container with the given one.` |
|        - |  464 | ` */` |
|  9775548 |  465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |  466 | `{` |
|  9775553 |  467 | `	if( pContainer == 0 ){` |
|        - |  468 | `		/* Point to the default container */` |
|      ! 0 |  469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |  470 | `	}else{` |
|        - |  471 | `		/* Change container */` |
|  9775553 |  472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |  473 | `	}` |
|  9775553 |  474 | `	return SXRET_OK;` |
|        5 |  475 | `}` |
|        - |  476 | `/*` |
|        - |  477 | ` * Return the current bytecode container.` |
|        - |  478 | ` */` |
|  4887774 |  479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |  480 | `{` |
|  4887779 |  481 | `	return pVm->pByteContainer;` |
|        5 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |  485 | ` */` |
|  6860170 |  486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |  487 | `{` |
|        - |  488 | `	VmInstr *pInstr;` |
|  6860175 |  489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|  6860175 |  490 | `	return pInstr;` |
|        5 |  491 | `}` |
|        - |  492 | `/*` |
|        - |  493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |  494 | ` */` |
| 54132240 |  495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |  496 | `{` |
| 54132245 |  497 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |  498 | `}` |
|        - |  499 | `/*` |
|        - |  500 | ` * Pop the last VM instruction.` |
|        - |  501 | ` */` |
|  5394944 |  502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |  503 | `{` |
|  5394949 |  504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |  505 | `}` |
|        - |  506 | `/*` |
|        - |  507 | ` * Peek the last VM instruction.` |
|        - |  508 | ` */` |
| 20436136 |  509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |  510 | `{` |
| 20436141 |  511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |  512 | `}` |
|  1711028 |  513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |  514 | `{` |
|        - |  515 | `	VmInstr *aInstr;` |
|        - |  516 | `	sxu32 n;` |
|  1711033 |  517 | `	n = SySetUsed(pVm->pByteContainer);` |
|  1711033 |  518 | `	if( n < 2 ){` |
|      ! 0 |  519 | `		return 0;` |
|        - |  520 | `	}` |
|  1711033 |  521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|  1711033 |  522 | `	return &aInstr[n - 2];` |
|   855519 |  523 | `}` |
|        - |  524 | `/*` |
|        - |  525 | ` * Allocate a new virtual machine frame.` |
|        - |  526 | ` */` |
|   114035 |  527 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|        - |  528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |  530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  531 | `	)` |
|        5 |  532 | `{` |
|        - |  533 | `	VmFrame *pFrame;` |
|        - |  534 | `	/* Allocate a new vm frame */` |
|   114040 |  535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   114040 |  536 | `	if( pFrame == 0 ){` |
|      ! 0 |  537 | `		return 0;` |
|        - |  538 | `	}` |
|        - |  539 | `	/* Zero the structure */` |
|   114040 |  540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |  541 | `	/* Initialize frame fields */` |
|   114040 |  542 | `	pFrame->pUserData = pUserData;` |
|   114040 |  543 | `	pFrame->pThis = pThis;` |
|   114040 |  544 | `	pFrame->pVm = pVm;` |
|   114040 |  545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   114040 |  546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   114040 |  547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   114040 |  548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   114040 |  549 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|        - |  550 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|        - |  551 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   114040 |  552 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   114040 |  553 | `	return pFrame;` |
|    57125 |  554 | `}` |
|        - |  555 | `/* Forward declaration */` |
|        - |  556 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|        - |  557 | `/*` |
|        - |  558 | ` * Enter a VM frame.` |
|        - |  559 | ` */` |
|   113439 |  560 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|        - |  561 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  562 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |  563 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  564 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |  565 | `	)` |
|        5 |  566 | `{` |
|        - |  567 | `	VmFrame *pFrame;` |
|        - |  568 | `	/* Allocate a new frame */` |
|   113444 |  569 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   113444 |  570 | `	if( pFrame == 0 ){` |
|      ! 0 |  571 | `		return SXERR_MEM;` |
|        - |  572 | `	}` |
|        - |  573 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   113444 |  574 | `	pFrame->nCallLine = pVm->nCurLine;` |
|        - |  575 | `	/* Link to the list of active VM frame */` |
|   113444 |  576 | `	pFrame->pParent = pVm->pFrame;` |
|   113444 |  577 | `	pVm->pFrame = pFrame;` |
|   113444 |  578 | `	if( ppFrame ){` |
|        - |  579 | `		/* Write a pointer to the new VM frame */` |
|   109560 |  580 | `		*ppFrame = pFrame;` |
|    54880 |  581 | `	}` |
|   113444 |  582 | `	return SXRET_OK;` |
|    56827 |  583 | `}` |
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
|   109985 |  633 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 |  634 | `{` |
|   109990 |  635 | `	if( pVm->pResumeFrame == pFrame ){` |
|        3 |  636 | `		pVm->pResumeFrame = 0;` |
|        1 |  637 | `	}` |
|   109990 |  638 | `}` |
|        - |  639 | `/*` |
|        - |  640 | ` * Leave the top-most active frame.` |
|        - |  641 | ` */` |
|   109133 |  642 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |  643 | `{` |
|   109138 |  644 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   109138 |  645 | `	if( pCurFrame ){` |
|        - |  646 | `		/* Unlink from the list of active VM frame */` |
|   109138 |  647 | `		pVm->pFrame = pCurFrame->pParent;` |
|   109138 |  648 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  649 | `			VmSlot  *aSlot;` |
|        - |  650 | `			sxu32 n;` |
|        - |  651 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   104266 |  652 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   592496 |  653 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |  654 | `				/* Unset the local variable */` |
|   488235 |  655 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   244735 |  656 | `			}` |
|        - |  657 | `			/* Remove local reference */` |
|   104266 |  658 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   592580 |  659 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   488319 |  660 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   244777 |  661 | `			}` |
|    52233 |  662 | `		}` |
|        - |  663 | `		/* Release internal containers */` |
|   109138 |  664 | `		SyHashRelease(&pCurFrame->hVar);` |
|   109138 |  665 | `		SySetRelease(&pCurFrame->sArg);` |
|   109138 |  666 | `		SySetRelease(&pCurFrame->sLocal);` |
|   109138 |  667 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |  668 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|        - |  669 | `		 * containers above — released for every frame, including transparent` |
|        - |  670 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   109138 |  671 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|        - |  672 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   109138 |  673 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|        - |  674 | `		/* Release the whole structure */` |
|   109138 |  675 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    54669 |  676 | `	}` |
|   109138 |  677 | `}` |
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
|     6920 |  699 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  700 | `{` |
|        - |  701 | `	VmFrame *pFrame;` |
|    13875 |  702 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     6967 |  703 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  704 | `		sxu32 n;` |
|     7027 |  705 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|       76 |  706 | `			if( aSlot[n].nIdx == nIdx ){` |
|        - |  707 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       13 |  708 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       13 |  709 | `				(void)SySetPop(&pFrame->sLocal);` |
|       13 |  710 | `				return TRUE; /* Slot owned by exactly one frame */` |
|        - |  711 | `			}` |
|       34 |  712 | `		}` |
|     3480 |  713 | `	}` |
|     6913 |  714 | `	return FALSE;` |
|     3465 |  715 | `}` |
|       78 |  716 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        2 |  717 | `{` |
|        - |  718 | `	VmRefObj *pRef;` |
|       80 |  719 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       80 |  720 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       80 |  721 | `	if( pRef ){` |
|       80 |  722 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       39 |  723 | `	}` |
|       80 |  724 | `}` |
|        - |  725 | `/*` |
|        - |  726 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |  727 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |  728 | ` * should be skipped when looking for the real execution context.` |
|        - |  729 | ` */` |
| 10550594 |  730 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |  731 | `{` |
| 10576013 |  732 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|    25419 |  733 | `		pFrame = pFrame->pParent;` |
|        5 |  734 | `	}` |
| 10550599 |  735 | `	return pFrame;` |
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
|     2960 |  760 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|        5 |  761 | `{` |
|     2965 |  762 | `	if( pVm->pResumeFrame == 0 ){` |
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
|     2946 |  773 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|     2471 |  774 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|     1988 |  775 | `	 \|\| pVm->iResumePc == 0 ){` |
|        - |  776 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|        - |  777 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|        - |  778 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|        - |  779 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|        - |  780 | `		 * never match a real frame. */` |
|      981 |  781 | `		return FALSE;` |
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
|     3361 |  804 | `	while( pVm->pFrame != pEntryFrame` |
|     3833 |  805 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|     2436 |  806 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|      817 |  807 | `		VmLeaveFrame(&(*pVm));` |
|        5 |  808 | `	}` |
|     1975 |  809 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|     1975 |  810 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|        - |  811 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|        - |  812 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|        - |  813 | `	 * point router must not re-fire it after this resume. */` |
|     1975 |  814 | `	pVm->nBoundaryRc = 0;` |
|     1975 |  815 | `	return TRUE;` |
|     1485 |  816 | `}` |
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
|     2706 |  848 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  849 | `{` |
|     2711 |  850 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|     2711 |  851 | `	if( pClone == 0 ){` |
|      ! 0 |  852 | `		return 0;` |
|        - |  853 | `	}` |
|     2711 |  854 | `	*pClone = *pCompiled;` |
|     2711 |  855 | `	pClone->pCompiled = pCompiled;` |
|     2711 |  856 | `	pClone->iFinallyDone = 0;` |
|     2711 |  857 | `	pClone->iInCatch = 0;` |
|     2711 |  858 | `	pClone->pInflight = 0;` |
|     2711 |  859 | `	pClone->pFrame = 0;` |
|     2711 |  860 | `	return pClone;` |
|     1358 |  861 | `}` |
|     5336 |  862 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|        5 |  863 | `{` |
|     5341 |  864 | `	if( pExc && pExc->pCompiled ){` |
|        - |  865 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|        - |  866 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|        - |  867 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|        - |  868 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|     2699 |  869 | `		if( pExc->pInflight ){` |
|      ! 0 |  870 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|      ! 0 |  871 | `			pExc->pInflight = 0;` |
|      ! 0 |  872 | `		}` |
|     2699 |  873 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|     1347 |  874 | `	}` |
|     5341 |  875 | `}` |
|        - |  876 | `/*` |
|        - |  877 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|        - |  878 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|        - |  879 | ` */` |
|      466 |  880 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|        5 |  881 | `{` |
|      471 |  882 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
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
|   257455 |  917 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |  918 | `{` |
|        - |  919 | `	sxu32 nUsed;` |
|   257460 |  920 | `	sxi32 rcOut = SXRET_OK;` |
|   257524 |  921 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
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
|   257460 |  952 | `	return rcOut;` |
|   128835 |  953 | `}` |
|        - |  954 | `/*` |
|        - |  955 | ` * Drop a body frame's pending catch/finally return: clear the flag and release` |
|        - |  956 | ` * the slot value. Safe on a frame with no pending return (the slot is then an` |
|        - |  957 | ` * empty MEMOBJ_NULL value and the release is a no-op).` |
|        - |  958 | ` */` |
|   154784 |  959 | `PH7_PRIVATE void VmClearFrameReturn(VmFrame *pFrame)` |
|        5 |  960 | `{` |
|   154789 |  961 | `	pFrame->bHasRet = 0;` |
|   154789 |  962 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   154789 |  963 | `}` |
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
|      242 | 1012 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|        - | 1013 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1014 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - | 1015 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - | 1016 | `	int nArg             /* Total number of passed arguments  */` |
|        - | 1017 | `	)` |
|        3 | 1018 | `{` |
|        - | 1019 | `	int iTarget,i,j,iCur,iMax;` |
|        - | 1020 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - | 1021 | `	ph7_vm_func *pLink;` |
|        - | 1022 | `	SyString sArgSig;` |
|        - | 1023 | `	SyBlob sSig;` |
|        - | 1024 |  |
|      245 | 1025 | `	pLink = pList;` |
|      245 | 1026 | `	i = 0;` |
|        - | 1027 | `	/* Put functions expecting the same number of passed arguments */` |
|     1459 | 1028 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1429 | 1029 | `		if( pLink == 0 ){` |
|      215 | 1030 | `			break;` |
|        - | 1031 | `		}` |
|     1217 | 1032 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - | 1033 | `			/* Candidate for overloading */` |
|     1217 | 1034 | `			apSet[i++] = pLink;` |
|      607 | 1035 | `		}` |
|        - | 1036 | `		/* Point to the next entry */` |
|     1217 | 1037 | `		pLink = pLink->pNextName;` |
|        3 | 1038 | `	}` |
|      245 | 1039 | `	if( i < 1 ){` |
|        - | 1040 | `		/* No candidates,return the head of the list */` |
|      ! 0 | 1041 | `		return pList;` |
|        - | 1042 | `	}` |
|      245 | 1043 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - | 1044 | `		/* Return the only candidate */` |
|       19 | 1045 | `		return apSet[0];` |
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
|      124 | 1094 | `}` |
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
|    34832 | 1113 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|        5 | 1114 | `{` |
|        - | 1115 | `	ph7_value sVal;` |
|    34837 | 1116 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|        - | 1117 | `	sxi32 rc;` |
|    34837 | 1118 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    34837 | 1119 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|        - | 1120 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|    34837 | 1121 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    34837 | 1122 | `	if( *ppMemObj ){` |
|    34837 | 1123 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|    17416 | 1124 | `	}` |
|    34837 | 1125 | `	PH7_MemObjRelease(&sVal);` |
|    34837 | 1126 | `	return rc;` |
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
|   703024 | 1141 | `static sxi32 VmMountUserClassAttrs(` |
|        - | 1142 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1143 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - | 1144 | `	)` |
|        5 | 1145 | `{` |
|        - | 1146 | `	ph7_class_attr *pAttr;` |
|        - | 1147 | `	SyHashEntry *pEntry;` |
|        - | 1148 | `	/* Reset the loop cursor */` |
|   703029 | 1149 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - | 1150 | `	/* Process only static and constant attribute */` |
|  2861339 | 1151 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1152 | `		/* Extract the current attribute */` |
|  2158319 | 1153 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2158314 | 1154 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  1422314 | 1155 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   343600 | 1156 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|        - | 1157 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|        - | 1158 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|        - | 1159 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|        - | 1160 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|        - | 1161 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|        - | 1162 | `			 * user catch, and initializers referencing constants of a class` |
|        - | 1163 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|        - | 1164 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|        - | 1165 | `			 * %s as value for class constant" fatal without any access). */` |
|   685413 | 1166 | `			continue;` |
|        - | 1167 | `		}` |
|  1472911 | 1168 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 1169 | `			ph7_value *pMemObj;` |
|    24859 | 1170 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|        - | 1171 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|        - | 1172 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|        - | 1173 | `				 * re-mount pass, so VM reuse still re-evaluates. */` |
|      870 | 1174 | `				continue;` |
|        - | 1175 | `			}` |
|        - | 1176 | `			/* Reserve a memory object for this constant/static attribute */` |
|    23991 | 1177 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    23991 | 1178 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1179 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1180 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 | 1181 | `					&pClass->sName,&pAttr->sName` |
|        - | 1182 | `					);` |
|      ! 0 | 1183 | `				return SXERR_MEM;` |
|        - | 1184 | `			}` |
|    23991 | 1185 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1186 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1187 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|        - | 1188 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|    23983 | 1189 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        - | 1190 | `				sxi32 rcExec;` |
|    23983 | 1191 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    23983 | 1192 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|    23983 | 1193 | `				pVm->nConstEvalDepth++;` |
|    23983 | 1194 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    23983 | 1195 | `				pVm->nConstEvalDepth--;` |
|    23983 | 1196 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|    23983 | 1197 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    23983 | 1198 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1199 | `					/* The initializer raised (self-referencing constant, or a` |
|        - | 1200 | `					 * throwing enum-case reference): park it for the fetch-point` |
|        - | 1201 | `					 * router — user classes mount mid-execution, so the throw` |
|        - | 1202 | `					 * lands catchably at the declaration site. */` |
|      ! 0 | 1203 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|    23983 | 1204 | `				}else if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 1205 | `					/* A nested evaluation detected a self-referencing constant:` |
|        - | 1206 | `					 * raise it at this, the outermost level. */` |
|      ! 0 | 1207 | `					VmBoundaryPark(&(*pVm),VmConstCycleThrow(&(*pVm)));` |
|      ! 0 | 1208 | `				}` |
|        - | 1209 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - | 1210 | `				 * against the declared type. A mismatch is a non-catchable` |
|        - | 1211 | `				 * fatal, raised here at definition time (matching PHP). */` |
|    23978 | 1212 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|    11994 | 1213 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|       35 | 1214 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|       35 | 1215 | `					if( rcType != SXRET_OK ){` |
|        6 | 1216 | `						return rcType;` |
|        - | 1217 | `					}` |
|       14 | 1218 | `				}` |
|    11987 | 1219 | `			}` |
|        - | 1220 | `			/* Record attribute index */` |
|    23987 | 1221 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - | 1222 | `			/* Install static attribute in the reference table */` |
|    23987 | 1223 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1224 | `			/* If this is a typed static property, register the slot so the` |
|        - | 1225 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - | 1226 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - | 1227 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - | 1228 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - | 1229 | `			 * already enforced above, so they need no store-time slot. */` |
|    23982 | 1230 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    12019 | 1231 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
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
|    11991 | 1250 | `		}` |
|        5 | 1251 | `	}` |
|   703025 | 1252 | `	return SXRET_OK;` |
|   351517 | 1253 | `}` |
|   701948 | 1254 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - | 1255 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 1256 | `	ph7_class *pClass /* Class to be mounted */` |
|        - | 1257 | `	)` |
|        5 | 1258 | `{` |
|        - | 1259 | `	ph7_class_method *pMeth;` |
|        - | 1260 | `	SyHashEntry *pEntry;` |
|        - | 1261 | `	sxi32 rc;` |
|        - | 1262 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   701953 | 1263 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   701953 | 1264 | `	if( rc != SXRET_OK ){` |
|        6 | 1265 | `		return rc;` |
|        - | 1266 | `	}` |
|        - | 1267 | `	/* Install class methods */` |
|   701949 | 1268 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - | 1269 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - | 1270 | `		 */` |
|   311861 | 1271 | `		return SXRET_OK;` |
|        - | 1272 | `	}` |
|        - | 1273 | `	/* Create constructor alias if not yet done */` |
|   390093 | 1274 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - | 1275 | `		/* User constructor with the same base class name */` |
|    59419 | 1276 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|    59419 | 1277 | `		if( pEntry ){` |
|      ! 0 | 1278 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 1279 | `			/* Create the alias */` |
|      ! 0 | 1280 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 | 1281 | `		}` |
|    29707 | 1282 | `	}` |
|        - | 1283 | `	/* Install the methods now */` |
|   390093 | 1284 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  5874049 | 1285 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5288917 | 1286 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5288917 | 1287 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  5278671 | 1288 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  5278671 | 1289 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1290 | `				return rc;` |
|        - | 1291 | `			}` |
|  2639333 | 1292 | `		}` |
|        5 | 1293 | `	}` |
|        - | 1294 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   390093 | 1295 | `	pClass->bMounted = TRUE;` |
|   390093 | 1296 | `	return SXRET_OK;` |
|   350979 | 1297 | `}` |
|        - | 1298 | `/*` |
|        - | 1299 | ` * Allocate a private frame for attributes of the given` |
|        - | 1300 | ` * class instance (Object in the PHP jargon).` |
|        - | 1301 | ` */` |
|     8714 | 1302 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - | 1303 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 1304 | `	ph7_class_instance *pObj /* Class instance */` |
|        - | 1305 | `	)` |
|        5 | 1306 | `{` |
|     8719 | 1307 | `	ph7_class *pClass = pObj->pClass;` |
|        - | 1308 | `	ph7_class_attr *pAttr;` |
|        - | 1309 | `	SyHashEntry *pEntry;` |
|        - | 1310 | `	sxi32 rc;` |
|        - | 1311 | `	/* Install class attribute in the private frame associated with this instance */` |
|     8719 | 1312 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    45845 | 1313 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - | 1314 | `		VmClassAttr *pVmAttr;` |
|        - | 1315 | `		/* Extract the current attribute */` |
|    37131 | 1316 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    37131 | 1317 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|    37131 | 1318 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1319 | `			return SXERR_MEM;` |
|        - | 1320 | `		}` |
|    37131 | 1321 | `		pVmAttr->pAttr = pAttr;` |
|    37131 | 1322 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - | 1323 | `			ph7_value *pMemObj;` |
|        - | 1324 | `			/* Reserve a memory object for this attribute */` |
|    29073 | 1325 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    29073 | 1326 | `			if( pMemObj == 0 ){` |
|      ! 0 | 1327 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1328 | `				return SXERR_MEM;` |
|        - | 1329 | `			}` |
|    29073 | 1330 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|    29073 | 1331 | `			pVmAttr->iState = 0;` |
|    29073 | 1332 | `			pVmAttr->pOwner = pClass;` |
|    29073 | 1333 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1334 | `				/* Initialize attribute default value (any complex expression).` |
|        - | 1335 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|        - | 1336 | `				 * against the declaring class (no method frame here). */` |
|    10647 | 1337 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|    10647 | 1338 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    10647 | 1339 | `				VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    10647 | 1340 | `				pVm->pConstEvalClass = pSaveCtx;` |
|    23752 | 1341 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1342 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - | 1343 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|      403 | 1344 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      199 | 1345 | `			}` |
|    29073 | 1346 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|    29073 | 1347 | `			if( rc != SXRET_OK ){` |
|        - | 1348 | `				VmSlot sSlot;` |
|        - | 1349 | `				/* Restore memory object */` |
|      ! 0 | 1350 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 | 1351 | `				sSlot.pUserData = 0;` |
|      ! 0 | 1352 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 | 1353 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1354 | `				return SXERR_MEM;` |
|        - | 1355 | `			}` |
|        - | 1356 | `			/* Install attribute in the reference table */` |
|    29073 | 1357 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - | 1358 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - | 1359 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - | 1360 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|    29073 | 1361 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|    14539 | 1373 | `		}else{` |
|        - | 1374 | `			/* Install static/constant attribute */` |
|     8063 | 1375 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|     8063 | 1376 | `			pVmAttr->iState = 0;` |
|     8063 | 1377 | `			pVmAttr->pOwner = pClass;` |
|     8063 | 1378 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     8063 | 1379 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1380 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 | 1381 | `				return SXERR_MEM;` |
|        - | 1382 | `			}` |
|        - | 1383 | `		}` |
|        5 | 1384 | `	}` |
|     8719 | 1385 | `	return SXRET_OK;` |
|     4362 | 1386 | `}` |
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
|  8561398 | 1559 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1560 | `{` |
|        - | 1561 | `	ph7_value *pObj;` |
|        - | 1562 | `	sxi32 rc;` |
|  8561403 | 1563 | `	if( pIndex ){` |
|        - | 1564 | `		/* Object index in the object table */` |
|  8549775 | 1565 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|  4274885 | 1566 | `	}` |
|        - | 1567 | `	/* Reserve a slot for the new object */` |
|  8561403 | 1568 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|  8561403 | 1569 | `	if( rc != SXRET_OK ){` |
|        - | 1570 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1571 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1572 | `		 */` |
|      ! 0 | 1573 | `		return 0;` |
|        - | 1574 | `	}` |
|  8561403 | 1575 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|  8561403 | 1576 | `	return pObj;` |
|  4280704 | 1577 | `}` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * Reserve a memory object.` |
|        - | 1580 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 1581 | ` */` |
|  2237598 | 1582 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 | 1583 | `{` |
|        - | 1584 | `	ph7_value *pObj;` |
|        - | 1585 | `	sxi32 rc;` |
|  2237603 | 1586 | `	if( pIndex ){` |
|        - | 1587 | `		/* Object index in the object table */` |
|  2237603 | 1588 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1118799 | 1589 | `	}` |
|        - | 1590 | `	/* Reserve a slot for the new object */` |
|  2237603 | 1591 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2237603 | 1592 | `	if( rc != SXRET_OK ){` |
|        - | 1593 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1594 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1595 | `		 */` |
|      ! 0 | 1596 | `		return 0;` |
|        - | 1597 | `	}` |
|  2237603 | 1598 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2237603 | 1599 | `	return pObj;` |
|  1118804 | 1600 | `}` |
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
|     3876 | 1614 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - | 1615 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - | 1616 | `	 ph7 *pEngine /* Master engine */` |
|        - | 1617 | `	 )` |
|        5 | 1618 | `{` |
|        - | 1619 | `	ph7_value *pObj;` |
|        - | 1620 | `	sxi32 rc;` |
|        - | 1621 | `	/* Zero the structure */` |
|     3881 | 1622 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - | 1623 | `	/* Initialize VM fields */` |
|     3881 | 1624 | `	pVm->pEngine = &(*pEngine);` |
|     3881 | 1625 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|     3881 | 1626 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - | 1627 | `	/* Instructions containers */` |
|     3881 | 1628 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3881 | 1629 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3881 | 1630 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - | 1631 | `	/* Object containers */` |
|     3881 | 1632 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3881 | 1633 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - | 1634 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|     3881 | 1635 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|     3881 | 1636 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|     3881 | 1637 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|     3881 | 1638 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|        - | 1639 | `	/* Virtual machine internal containers */` |
|     3881 | 1640 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3881 | 1641 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3881 | 1642 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|     3881 | 1643 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|     3881 | 1644 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3881 | 1645 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3881 | 1646 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3881 | 1647 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3881 | 1648 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3881 | 1649 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3881 | 1650 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3881 | 1651 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3881 | 1652 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3881 | 1653 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3881 | 1654 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3881 | 1655 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3881 | 1656 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3881 | 1657 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3881 | 1658 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3881 | 1659 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|     3881 | 1660 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3881 | 1661 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3881 | 1662 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|     3881 | 1663 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3881 | 1664 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3881 | 1665 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|     3881 | 1666 | `	pVm->pMagicSetThis = 0;` |
|     3881 | 1667 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|     3881 | 1668 | `	pVm->pHookSetThis = 0;` |
|     3881 | 1669 | `	pVm->pHookSetAttr = 0;` |
|     3881 | 1670 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3881 | 1671 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|     3881 | 1672 | `	pVm->pMagicCallThis = 0;` |
|     3881 | 1673 | `	pVm->pMagicCallClass = 0;` |
|     3881 | 1674 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|     3881 | 1675 | `	pVm->pIdleCallFrames = 0;` |
|     3881 | 1676 | `	pVm->pIdleOperandStacks = 0;` |
|     3881 | 1677 | `	pVm->nIdleOperandStacks = 0;` |
|     3881 | 1678 | `	pVm->pIdleStackNodes = 0;` |
|     3881 | 1679 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|     3881 | 1680 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|     3881 | 1681 | `	pVm->pPendingException = 0;` |
|     3881 | 1682 | `	pVm->pInflightException = 0;` |
|     3881 | 1683 | `	pVm->nInflightExcBase = 0;` |
|     3881 | 1684 | `	pVm->pResumeFrame = 0;` |
|     3881 | 1685 | `	pVm->iResumePc = 0;` |
|     3881 | 1686 | `	pVm->pResumeInstr = 0;` |
|     3881 | 1687 | `	pVm->iResumeStackDepth = 0;` |
|     3881 | 1688 | `	pVm->nBoundaryRc = 0;` |
|     3881 | 1689 | `	pVm->pConstEvalClass = 0;` |
|     3881 | 1690 | `	pVm->nConstEvalDepth = 0;` |
|     3881 | 1691 | `	pVm->pConstCycleAttr = 0;` |
|     3881 | 1692 | `	pVm->pConstCycleClass = 0;` |
|     3881 | 1693 | `	SySetReset(&pVm->aMagicGuard);` |
|     3881 | 1694 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 1695 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 1696 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 1697 | `	}` |
|     3881 | 1698 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|     3881 | 1699 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 1700 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 1701 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 1702 | `	}` |
|     3881 | 1703 | `	pVm->pHookSetAttr = 0;` |
|     3881 | 1704 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     3881 | 1705 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 1706 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 1707 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 1708 | `	}` |
|     3881 | 1709 | `	pVm->pMagicCallClass = 0;` |
|     3881 | 1710 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        - | 1711 | `	/* Configuration containers */` |
|     3881 | 1712 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3881 | 1713 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3881 | 1714 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3881 | 1715 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3881 | 1716 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3881 | 1717 | `	pVm->iResponseStatus = 200;` |
|     3881 | 1718 | `	pVm->bHeadersSent = 0;` |
|     3881 | 1719 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - | 1720 | `	/* Error callbacks containers */` |
|     3881 | 1721 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3881 | 1722 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3881 | 1723 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3881 | 1724 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3881 | 1725 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - | 1726 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|        - | 1727 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|        - | 1728 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|        - | 1729 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|        - | 1730 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|        - | 1731 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|        - | 1732 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3881 | 1733 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|     3881 | 1734 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|        - | 1735 | `	                             * usort-in-comparator path overflows at 1024) */` |
|        - | 1736 | `#else` |
|        - | 1737 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|        - | 1738 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|        - | 1739 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|        - | 1740 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|        - | 1741 | `	pVm->nMaxDepth = 512;` |
|        - | 1742 | `	pVm->nMaxNativeDepth = 16;` |
|        - | 1743 | `#endif` |
|        - | 1744 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|        - | 1745 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|        - | 1746 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|     3881 | 1747 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|        - | 1748 | `	/* JSON return status */` |
|     3881 | 1749 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 1750 | `	/* PRNG context */` |
|     3881 | 1751 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - | 1752 | `	/* Install the null constant */` |
|     3881 | 1753 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3881 | 1754 | `	if( pObj == 0 ){` |
|      ! 0 | 1755 | `		rc = SXERR_MEM;` |
|      ! 0 | 1756 | `		goto Err;` |
|        - | 1757 | `	}` |
|     3881 | 1758 | `	PH7_MemObjInit(pVm,pObj);` |
|        - | 1759 | `	/* Install the boolean TRUE constant */` |
|     3881 | 1760 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3881 | 1761 | `	if( pObj == 0 ){` |
|      ! 0 | 1762 | `		rc = SXERR_MEM;` |
|      ! 0 | 1763 | `		goto Err;` |
|        - | 1764 | `	}` |
|     3881 | 1765 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - | 1766 | `	/* Install the boolean FALSE constant */` |
|     3881 | 1767 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3881 | 1768 | `	if( pObj == 0 ){` |
|      ! 0 | 1769 | `		rc = SXERR_MEM;` |
|      ! 0 | 1770 | `		goto Err;` |
|        - | 1771 | `	}` |
|     3881 | 1772 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - | 1773 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - | 1774 | `	 * reuse the same slot rather than allocating a new one.` |
|        - | 1775 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3881 | 1776 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3881 | 1777 | `	if( pObj == 0 ){` |
|      ! 0 | 1778 | `		rc = SXERR_MEM;` |
|      ! 0 | 1779 | `		goto Err;` |
|        - | 1780 | `	}` |
|     3881 | 1781 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - | 1782 | `	/* Create the global frame */` |
|     3881 | 1783 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3881 | 1784 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1785 | `		goto Err;` |
|        - | 1786 | `	}` |
|        - | 1787 | `	/* Initialize the code generator */` |
|     3881 | 1788 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3881 | 1789 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1790 | `		goto Err;` |
|        - | 1791 | `	}` |
|        - | 1792 | `	/* VM correctly initialized,set the magic number */` |
|     3881 | 1793 | `	pVm->nMagic = PH7_VM_INIT;` |
|        - | 1794 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|        - | 1795 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|     3881 | 1796 | `	pVm->bCompilingBuiltin = 1;` |
|        - | 1797 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|     3881 | 1798 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|        - | 1799 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|        - | 1800 | `	 * compiled — its classes are internal too. */` |
|        - | 1801 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3881 | 1802 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - | 1803 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3881 | 1804 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3881 | 1805 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3881 | 1806 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3881 | 1807 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3881 | 1808 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - | 1809 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3881 | 1810 | `	pVm->pCoalesceObj = 0;` |
|     3881 | 1811 | `	pVm->bCoalesceArmed = 0;` |
|     3881 | 1812 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - | 1813 | `	/* Register Fiber internal C functions */` |
|     3881 | 1814 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3881 | 1815 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3881 | 1816 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3881 | 1817 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3881 | 1818 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3881 | 1819 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3881 | 1820 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3881 | 1821 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3881 | 1822 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3881 | 1823 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - | 1824 | `	/* Cache the Closure class pointer (closures are instances of it) */` |
|     3881 | 1825 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|     3881 | 1826 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|     3881 | 1827 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|        - | 1828 | `	/* Closure::bind/bindTo/call/fromCallable native delegates (Increment 2) */` |
|     3881 | 1829 | `	ph7_create_function(pVm,"__closure_bindTo",vm_builtin_Closure_bindTo,0);` |
|     3881 | 1830 | `	ph7_create_function(pVm,"__closure_fromCallable",vm_builtin_Closure_fromCallable,0);` |
|        - | 1831 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|     3881 | 1832 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        - | 1833 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3881 | 1834 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3881 | 1835 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3881 | 1836 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3881 | 1837 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3881 | 1838 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3881 | 1839 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3881 | 1840 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3881 | 1841 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3881 | 1842 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3881 | 1843 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - | 1844 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|        - | 1845 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|        - | 1846 | `	 * internal; the Traversable pointer above must already be cached. */` |
|     3881 | 1847 | `	PH7_VmInstallReflection(&(*pVm));` |
|     3881 | 1848 | `	PH7_VmInstallDateTime(&(*pVm));` |
|     3881 | 1849 | `	PH7_VmInstallSpl(&(*pVm));` |
|     3881 | 1850 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|     3881 | 1851 | `	PH7_VmInstallSession(&(*pVm));` |
|     3881 | 1852 | `	PH7_VmInstallIni(&(*pVm));` |
|        - | 1853 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1854 | `	/* libxml2-backed surfaces: shared plumbing first, then the DOM and` |
|        - | 1855 | `	 * XMLWriter class libraries that build on it. */` |
|     3881 | 1856 | `	PH7_VmInstallLibxml(&(*pVm));` |
|     3881 | 1857 | `	PH7_VmInstallDom(&(*pVm));` |
|     3881 | 1858 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|        - | 1859 | `#endif` |
|     3881 | 1860 | `	pVm->bCompilingBuiltin = 0;` |
|        - | 1861 | `	/* Reset the code generator */` |
|     3881 | 1862 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3881 | 1863 | `	return SXRET_OK;` |
|      ! 0 | 1864 | `Err:` |
|      ! 0 | 1865 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 | 1866 | `	return rc;` |
|     1943 | 1867 | `}` |
|        - | 1868 | `/*` |
|        - | 1869 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - | 1870 | ` * routine which store the output in an internal blob.` |
|        - | 1871 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - | 1872 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - | 1873 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - | 1874 | ` * Refer to the official docurmentation for additional information.` |
|        - | 1875 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - | 1876 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - | 1877 | ` * to finish executing and extracting the output.` |
|        - | 1878 | ` */` |
|       66 | 1879 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - | 1880 | `	const void *pOut,   /* VM Generated output*/` |
|        - | 1881 | `	unsigned int nLen,  /* Generated output length */` |
|        - | 1882 | `	void *pUserData     /* User private data */` |
|        - | 1883 | `	)` |
|      ! 0 | 1884 | `{` |
|        - | 1885 | `	 sxi32 rc;` |
|        - | 1886 | `	 /* Store the output in an internal BLOB */` |
|       66 | 1887 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       66 | 1888 | `	 return rc;` |
|      ! 0 | 1889 | `}` |
|        - | 1890 | `/*` |
|        - | 1891 | ` * Track output length and mark headers as sent when output reaches` |
|        - | 1892 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - | 1893 | ` */` |
|    41484 | 1894 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 | 1895 | `{` |
|    41489 | 1896 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    41489 | 1897 | `	if( xCons != VmObConsumer ){` |
|    12653 | 1898 | `		pVm->nOutputLen += nLen;` |
|    12653 | 1899 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1397 | 1900 | `			pVm->bHeadersSent = 1;` |
|      696 | 1901 | `		}` |
|     6324 | 1902 | `	}` |
|    41489 | 1903 | `}` |
|        - | 1904 | `/*` |
|        - | 1905 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|        - | 1906 | ` *` |
|        - | 1907 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|        - | 1908 | ` * (no instruction pushes more than one net slot), and that is what` |
|        - | 1909 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|        - | 1910 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|        - | 1911 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|        - | 1912 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|        - | 1913 | ` *` |
|        - | 1914 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|        - | 1915 | ` * conservative BY CONSTRUCTION:` |
|        - | 1916 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|        - | 1917 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|        - | 1918 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|        - | 1919 | ` *     pop — makes height go negative, which triggers fallback.` |
|        - | 1920 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|        - | 1921 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|        - | 1922 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|        - | 1923 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|        - | 1924 | ` *     bound. There is no partial/unsafe middle.` |
|        - | 1925 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|        - | 1926 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|        - | 1927 | ` *     instruction-count bound -> fallback.` |
|        - | 1928 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|        - | 1929 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|        - | 1930 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|        - | 1931 | ` *` |
|        - | 1932 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|        - | 1933 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|        - | 1934 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|        - | 1935 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|        - | 1936 | ` *` |
|        - | 1937 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|        - | 1938 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|        - | 1939 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|        - | 1940 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|        - | 1941 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|        - | 1942 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|        - | 1943 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|        - | 1944 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|        - | 1945 | ` * entry here.` |
|        - | 1946 | ` */` |
|        - | 1947 | `/*` |
|        - | 1948 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|        - | 1949 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|        - | 1950 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|        - | 1951 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|        - | 1952 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|        - | 1953 | ` */` |
|    51134 | 1954 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|        5 | 1955 | `{` |
|    51139 | 1956 | `	int push = 0, n = 0;` |
|        - | 1957 | `	sxi32 d;` |
|    51139 | 1958 | `	switch( pI->iOp ){` |
|        - | 1959 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|        - | 1960 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     8082 | 1961 | `	case PH7_OP_LOADC:` |
|        - | 1962 | `	case PH7_OP_DUP:` |
|    16169 | 1963 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|     5754 | 1964 | `	case PH7_OP_LOAD:` |
|    11513 | 1965 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|    11513 | 1966 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|      ! 0 | 1967 | `	case PH7_OP_LOAD_REF:` |
|      ! 0 | 1968 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1969 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|      340 | 1970 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|        - | 1971 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|        - | 1972 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      685 | 1973 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|        - | 1974 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|        - | 1975 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|      269 | 1976 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - | 1977 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|      543 | 1978 | `		if( pI->iP2 == 0 ){` |
|      543 | 1979 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|      274 | 1980 | `		}else{` |
|      ! 0 | 1981 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|      ! 0 | 1982 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|        - | 1983 | `		}` |
|      543 | 1984 | `		break;` |
|        - | 1985 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|        - | 1986 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|      155 | 1987 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|        - | 1988 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|        - | 1989 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|        - | 1990 | `	case PH7_OP_NOOP:` |
|      315 | 1991 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - | 1992 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|        - | 1993 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      815 | 1994 | `	case PH7_OP_STORE:` |
|     1635 | 1995 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|     1635 | 1996 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|        - | 1997 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|     1258 | 1998 | `	case PH7_OP_POP:` |
|        - | 1999 | `	case PH7_OP_CONSUME:` |
|     2521 | 2000 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2001 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|        - | 2002 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|        - | 2003 | `	 * true pop count is a runtime value — never reaches here. */` |
|     1910 | 2004 | `	case PH7_OP_CALL:` |
|     3825 | 2005 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - | 2006 | `	/* Jumps. */` |
|      147 | 2007 | `	case PH7_OP_JMP:` |
|      299 | 2008 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|      770 | 2009 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|     1545 | 2010 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|     1545 | 2011 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|        - | 2012 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|     2962 | 2013 | `	case PH7_OP_DONE:` |
|     5929 | 2014 | `		n = 0; break;` |
|     3105 | 2015 | `	default:` |
|     6215 | 2016 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|        - | 2017 | `	}` |
|    44929 | 2018 | `	*pPush = push; *pN = n;` |
|    44929 | 2019 | `	return 1;` |
|    25572 | 2020 | `}` |
|        - | 2021 | `/*` |
|        - | 2022 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|        - | 2023 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|        - | 2024 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|        - | 2025 | ` */` |
|     8236 | 2026 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|        5 | 2027 | `{` |
|        - | 2028 | `	void *pScratch;` |
|        - | 2029 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|        - | 2030 | `	sxu32 nQ, i, nIter, nCap;` |
|        - | 2031 | `	sxi32 iMax;` |
|        - | 2032 | `	int push, n, k;` |
|        - | 2033 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     8241 | 2034 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|        - | 2035 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|      ! 0 | 2036 | `		return VM_STACK_UNMODELED;` |
|        - | 2037 | `	}` |
|        - | 2038 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|    44571 | 2039 | `	for( i = 0; i < nInstr; i++ ){` |
|    42545 | 2040 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     6215 | 2041 | `			return VM_STACK_UNMODELED;` |
|        - | 2042 | `		}` |
|    18170 | 2043 | `	}` |
|        - | 2044 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|        - | 2045 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|        - | 2046 | `	 * first (the byte array last needs no alignment). */` |
|     2031 | 2047 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|     2031 | 2048 | `	if( pScratch == 0 ){` |
|      ! 0 | 2049 | `		return VM_STACK_UNMODELED;` |
|        - | 2050 | `	}` |
|     2031 | 2051 | `	aH  = (sxi32 *)pScratch;` |
|     2031 | 2052 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|     2031 | 2053 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|    12347 | 2054 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|     2031 | 2055 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|     2031 | 2056 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|    10625 | 2057 | `	while( nQ > 0 ){` |
|     8599 | 2058 | `		sxu32 pc = aQ[--nQ];` |
|        - | 2059 | `		sxi32 h;` |
|     8599 | 2060 | `		aIn[pc] = 0;` |
|     8599 | 2061 | `		h = aH[pc];` |
|     8599 | 2062 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     8599 | 2063 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     8599 | 2064 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     8599 | 2065 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|    15245 | 2066 | `		for( k = 0; k < n; k++ ){` |
|     6651 | 2067 | `			sxi32 hn = h + delta[k];` |
|     6651 | 2068 | `			sxu32 t = succ[k];` |
|     6651 | 2069 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|     6651 | 2070 | `			if( hn > aH[t] ){` |
|     6573 | 2071 | `				aH[t] = hn;` |
|     6573 | 2072 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|     3284 | 2073 | `			}` |
|     3328 | 2074 | `		}` |
|     8599 | 2075 | `		if( iMax < 0 ){ break; }` |
|        5 | 2076 | `	}` |
|     2031 | 2077 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|     2031 | 2078 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|     4123 | 2079 | `}` |
|        - | 2080 | `/*` |
|        - | 2081 | ` * Allocate a new operand stack so that we can start executing` |
|        - | 2082 | ` * our compiled PHP program.` |
|        - | 2083 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - | 2084 | ` * on success. NULL (Fatal error) on failure.` |
|        - | 2085 | ` *` |
|        - | 2086 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|        - | 2087 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|        - | 2088 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|        - | 2089 | ` * eval, coroutine, callbacks) call this directly.` |
|        - | 2090 | ` */` |
|   241982 | 2091 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|        - | 2092 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 2093 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - | 2094 | `	)` |
|        5 | 2095 | `{` |
|        - | 2096 | `	ph7_value *pStack;` |
|        - | 2097 | `  /* No instruction ever pushes more than a single element onto the` |
|        - | 2098 | `  ** stack and the stack never grows on successive executions of the` |
|        - | 2099 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - | 2100 | `  ** on the maximum stack depth required.` |
|        - | 2101 | `  **` |
|        - | 2102 | `  ** Allocation all the stack space we will ever need.` |
|        - | 2103 | `  */` |
|   241987 | 2104 | `	nInstr += VM_STACK_GUARD;` |
|   241987 | 2105 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   241987 | 2106 | `	if( pStack == 0 ){` |
|      ! 0 | 2107 | `		return 0;` |
|        - | 2108 | `	}` |
|        - | 2109 | `	/* Initialize the operand stack */` |
| 23082135 | 2110 | `	while( nInstr > 0 ){` |
| 22840153 | 2111 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
| 22840153 | 2112 | `		--nInstr;` |
|        5 | 2113 | `	}` |
|        - | 2114 | `	/* Ready for bytecode execution */` |
|   241987 | 2115 | `	return pStack;` |
|   121098 | 2116 | `}` |
|        - | 2117 | `/*` |
|        - | 2118 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|        - | 2119 | ` *` |
|        - | 2120 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|        - | 2121 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|        - | 2122 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|        - | 2123 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|        - | 2124 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|        - | 2125 | ` * the per-slot PH7_MemObjInit.` |
|        - | 2126 | ` *` |
|        - | 2127 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|        - | 2128 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|        - | 2129 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|        - | 2130 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|        - | 2131 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|        - | 2132 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|        - | 2133 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|        - | 2134 | ` *` |
|        - | 2135 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|        - | 2136 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|        - | 2137 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|        - | 2138 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|        - | 2139 | ` * recursion case is the one worth the O(1) simplicity.` |
|        - | 2140 | ` */` |
|        - | 2141 | `typedef struct VmIdleStack VmIdleStack;` |
|        - | 2142 | `struct VmIdleStack {` |
|        - | 2143 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|        - | 2144 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|        - | 2145 | `	VmIdleStack *pNext;  /* LIFO link */` |
|        - | 2146 | `};` |
|        - | 2147 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|        - | 2148 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|        - | 2149 | `                                    * (a large fallback-sized stack recursing would` |
|        - | 2150 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|        - | 2151 | `                                    * the tight-sized hot case is far below this) */` |
|        - | 2152 | `/*` |
|        - | 2153 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|        - | 2154 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|        - | 2155 | ` */` |
|   104537 | 2156 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|        5 | 2157 | `{` |
|   104542 | 2158 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|   104542 | 2159 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|   104542 | 2160 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    18552 | 2161 | `		ph7_value *pStack = pIdle->pStack;` |
|    18552 | 2162 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    18552 | 2163 | `		pVm->nIdleOperandStacks--;` |
|        - | 2164 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|        - | 2165 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    18552 | 2166 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    18552 | 2167 | `		pVm->pIdleStackNodes = pIdle;` |
|    18552 | 2168 | `		return pStack; /* slots already released -> reusable without re-init */` |
|        - | 2169 | `	}` |
|    85995 | 2170 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    52376 | 2171 | `}` |
|        - | 2172 | `/*` |
|        - | 2173 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|        - | 2174 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|        - | 2175 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|        - | 2176 | ` * live value.` |
|        - | 2177 | ` */` |
|   104127 | 2178 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|        5 | 2179 | `{` |
|        - | 2180 | `	VmIdleStack *pIdle;` |
|        - | 2181 | `	sxu32 i;` |
|   104132 | 2182 | `	if( pStack == 0 ){` |
|      ! 0 | 2183 | `		return;` |
|        - | 2184 | `	}` |
|   104132 | 2185 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|    79743 | 2186 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|    79743 | 2187 | `		return;` |
|        - | 2188 | `	}` |
|        - | 2189 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|        - | 2190 | `	 * pool-allocate only when the spare list is empty. */` |
|    24394 | 2191 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    24394 | 2192 | `	if( pIdle ){` |
|    18552 | 2193 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|     9279 | 2194 | `	}else{` |
|     5847 | 2195 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|     5847 | 2196 | `		if( pIdle == 0 ){` |
|      ! 0 | 2197 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|      ! 0 | 2198 | `			return;` |
|        - | 2199 | `		}` |
|        - | 2200 | `	}` |
|  1681937 | 2201 | `	for( i = 0; i < nCap; i++ ){` |
|  1657548 | 2202 | `		PH7_MemObjRelease(&pStack[i]);` |
|        - | 2203 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|        - | 2204 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|        - | 2205 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|        - | 2206 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|        - | 2207 | `		 * across invocations — cheap defense in depth. */` |
|  1657548 | 2208 | `		pStack[i].nIdx = SXU32_HIGH;` |
|   828970 | 2209 | `	}` |
|    24394 | 2210 | `	pIdle->pStack = pStack;` |
|    24394 | 2211 | `	pIdle->nCap = nCap;` |
|    24394 | 2212 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    24394 | 2213 | `	pVm->pIdleOperandStacks = pIdle;` |
|    24394 | 2214 | `	pVm->nIdleOperandStacks++;` |
|    52171 | 2215 | `}` |
|        - | 2216 | `/* Forward declaration */` |
|        - | 2217 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - | 2218 | `/*` |
|        - | 2219 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - | 2220 | ` * This routine gets called by the PH7 engine after` |
|        - | 2221 | ` * successful compilation of the target PHP program.` |
|        - | 2222 | ` */` |
|     3408 | 2223 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - | 2224 | `	ph7_vm *pVm /* Target VM */` |
|        - | 2225 | `	)` |
|        5 | 2226 | `{` |
|        - | 2227 | `	SyHashEntry *pEntry;` |
|        - | 2228 | `	sxi32 rc;` |
|     3413 | 2229 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - | 2230 | `		/* Initialize your VM first */` |
|      ! 0 | 2231 | `		return SXERR_CORRUPT;` |
|        - | 2232 | `	}` |
|        - | 2233 | `	/* Mark the VM ready for byte-code execution */` |
|     3413 | 2234 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - | 2235 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - | 2236 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - | 2237 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - | 2238 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     3413 | 2239 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - | 2240 | `	/* Emit the DONE instruction */` |
|     3413 | 2241 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     3413 | 2242 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2243 | `		return SXERR_MEM;` |
|        - | 2244 | `	}` |
|        - | 2245 | `	/* Script return value */` |
|     3413 | 2246 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - | 2247 | `	/* Allocate a new operand stack */` |
|     3413 | 2248 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     3413 | 2249 | `	if( pVm->aOps == 0 ){` |
|      ! 0 | 2250 | `		return SXERR_MEM;` |
|        - | 2251 | `	}` |
|        - | 2252 | `	/* Set the default VM output consumer callback and it's` |
|        - | 2253 | `	 * private data. */` |
|     3413 | 2254 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     3413 | 2255 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - | 2256 | `	/* Allocate the reference table */` |
|     3413 | 2257 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     3413 | 2258 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     3413 | 2259 | `	if( pVm->apRefObj == 0 ){` |
|        - | 2260 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2261 | `		return SXERR_MEM;` |
|        - | 2262 | `	}` |
|        - | 2263 | `	/* Zero the reference table */` |
|     3413 | 2264 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - | 2265 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     3413 | 2266 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     3413 | 2267 | `	if( rc != SXRET_OK ){` |
|        - | 2268 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2269 | `		return rc;` |
|        - | 2270 | `	}` |
|        - | 2271 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - | 2272 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - | 2273 | `	 * every object/variable created during execution) is per-exec state that` |
|        - | 2274 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - | 2275 | `	 * below it is compile-time/init state that survives a reset. */` |
|     3413 | 2276 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - | 2277 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     3413 | 2278 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     3413 | 2279 | `	if( rc != SXRET_OK ){` |
|        - | 2280 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2281 | `		return rc;` |
|        - | 2282 | `	}` |
|        - | 2283 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     3413 | 2284 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - | 2285 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|     3413 | 2286 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|        - | 2287 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     3413 | 2288 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - | 2289 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     3413 | 2290 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - | 2291 | `#ifdef PH7_ENABLE_PCRE` |
|        - | 2292 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     3413 | 2293 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     3413 | 2294 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - | 2295 | `#endif` |
|        - | 2296 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2297 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|     3413 | 2298 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|        - | 2299 | `#endif` |
|        - | 2300 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|        - | 2301 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|     3413 | 2302 | `	VmSetBuiltinArity(&(*pVm));` |
|        - | 2303 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|     3413 | 2304 | `	VmSetBuiltinSignatures(&(*pVm));` |
|        - | 2305 | `	/* Initialize and install static and constants class attributes.` |
|        - | 2306 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - | 2307 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - | 2308 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - | 2309 | `	 * that function in sync when changing what is reserved here. */` |
|     3413 | 2310 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   460347 | 2311 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   456941 | 2312 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   456941 | 2313 | `		if( rc != SXRET_OK ){` |
|        3 | 2314 | `			return rc;` |
|        - | 2315 | `		}` |
|        5 | 2316 | `	}` |
|        - | 2317 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     3411 | 2318 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2319 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|     3411 | 2320 | `	pVm->nNextObjId = 1;` |
|        - | 2321 | `	/* VM is ready for bytecode execution */` |
|     3411 | 2322 | `	return SXRET_OK;` |
|     1709 | 2323 | `}` |
|        - | 2324 | `/*` |
|        - | 2325 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - | 2326 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - | 2327 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - | 2328 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - | 2329 | ` * a dangling node pointer in some other object's reference record.` |
|        - | 2330 | ` */` |
|        8 | 2331 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 | 2332 | `{` |
|        - | 2333 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - | 2334 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - | 2335 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      328 | 2336 | `	while( pVm->pRefList ){` |
|      320 | 2337 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 | 2338 | `	}` |
|        8 | 2339 | `}` |
|        - | 2340 | `/*` |
|        - | 2341 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - | 2342 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - | 2343 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - | 2344 | ` */` |
|       56 | 2345 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 | 2346 | `{` |
|       56 | 2347 | `	PH7_MemObjRelease(pObj);` |
|       56 | 2348 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       56 | 2349 | `}` |
|        - | 2350 | `/*` |
|        - | 2351 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - | 2352 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - | 2353 | ` * of statics).` |
|        - | 2354 | ` */` |
|     6760 | 2355 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 | 2356 | `{` |
|     6760 | 2357 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - | 2358 | `	sxu32 k;` |
|     6788 | 2359 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|       28 | 2360 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|       14 | 2361 | `	}` |
|     6760 | 2362 | `}` |
|        - | 2363 | `/*` |
|        - | 2364 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - | 2365 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - | 2366 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - | 2367 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - | 2368 | ` *    captured environment values, its name buffer and its structure (the` |
|        - | 2369 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - | 2370 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - | 2371 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - | 2372 | ` *    has its static sentinels reset.` |
|        - | 2373 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - | 2374 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - | 2375 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - | 2376 | ` */` |
|        8 | 2377 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 | 2378 | `{` |
|        - | 2379 | `	SyHashEntry *pEntry;` |
|        8 | 2380 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     6768 | 2381 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     6760 | 2382 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     6760 | 2383 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 2384 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - | 2385 | `			 * release its captured-by-value environment, then free the entry,` |
|        - | 2386 | `			 * name buffer and structure. */` |
|        4 | 2387 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 | 2388 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - | 2389 | `			sxu32 k;` |
|        4 | 2390 | `			VmResetFuncStatics(pFunc);` |
|        8 | 2391 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 | 2392 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 | 2393 | `			}` |
|        4 | 2394 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - | 2395 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 | 2396 | `			SyHashDeleteEntry2(pEntry);` |
|        4 | 2397 | `			if( zName ){` |
|        4 | 2398 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 | 2399 | `			}` |
|        4 | 2400 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 | 2401 | `			continue;` |
|        - | 2402 | `		}` |
|        - | 2403 | `		/* Named function: reset statics for every overload sharing this name. */` |
|    13512 | 2404 | `		while( pFunc ){` |
|     6756 | 2405 | `			VmResetFuncStatics(pFunc);` |
|     6756 | 2406 | `			pFunc = pFunc->pNextName;` |
|      ! 0 | 2407 | `		}` |
|      ! 0 | 2408 | `	}` |
|        8 | 2409 | `	pVm->closure_cnt = 0;` |
|        8 | 2410 | `}` |
|        - | 2411 | `/*` |
|        - | 2412 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - | 2413 | ` * are already gone (each object's destructor removed its own during the object` |
|        - | 2414 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - | 2415 | ` * the class re-mount registers fresh ones.` |
|        - | 2416 | ` */` |
|        8 | 2417 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 | 2418 | `{` |
|        - | 2419 | `	SyHashEntry *pEntry;` |
|        - | 2420 | `	/* Common case: no class static typed properties — table already empty. */` |
|        8 | 2421 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        4 | 2422 | `		return;` |
|        - | 2423 | `	}` |
|        - | 2424 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - | 2425 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 | 2426 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 | 2427 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 | 2428 | `		if( pEntry->pUserData ){` |
|        4 | 2429 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 | 2430 | `		}` |
|      ! 0 | 2431 | `	}` |
|        4 | 2432 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 | 2433 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        4 | 2434 | `}` |
|        - | 2435 | `/*` |
|        - | 2436 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - | 2437 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - | 2438 | ` *` |
|        - | 2439 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - | 2440 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - | 2441 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - | 2442 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - | 2443 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - | 2444 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - | 2445 | ` *` |
|        - | 2446 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - | 2447 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - | 2448 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - | 2449 | ` * exception/error-handler state, the reference table and every object/array` |
|        - | 2450 | ` * reserved during the run.` |
|        - | 2451 | ` *` |
|        - | 2452 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - | 2453 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - | 2454 | ` * global-scope destructors never fired.` |
|        - | 2455 | ` */` |
|        8 | 2456 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 | 2457 | `{` |
|        - | 2458 | `	sxu32 nWater,n;` |
|        8 | 2459 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 | 2460 | `		return SXERR_CORRUPT;` |
|        - | 2461 | `	}` |
|        8 | 2462 | `	nWater = pVm->nSuperBaseline;` |
|        - | 2463 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - | 2464 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        8 | 2465 | `	pVm->pGlobal = 0;` |
|        - | 2466 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|        - | 2467 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|        - | 2468 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|        - | 2469 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|        - | 2470 | `	 * object); unref'ing here would race the teardown below. */` |
|        8 | 2471 | `	pVm->pClosureThis = 0;` |
|        8 | 2472 | `	pVm->pClosureScope = 0;` |
|        - | 2473 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - | 2474 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - | 2475 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - | 2476 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        8 | 2477 | `	pVm->bInReset = 1;` |
|        - | 2478 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        8 | 2479 | `	VmResetRefTable(&(*pVm));` |
|        - | 2480 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - | 2481 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - | 2482 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - | 2483 | `	 * re-run define() overwrites the value in place). */` |
|        8 | 2484 | `	VmResetFunctionState(&(*pVm));` |
|        - | 2485 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - | 2486 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      344 | 2487 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      336 | 2488 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      336 | 2489 | `		if( pObj ){` |
|      336 | 2490 | `			PH7_MemObjRelease(pObj);` |
|      168 | 2491 | `		}` |
|      168 | 2492 | `	}` |
|        - | 2493 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - | 2494 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        8 | 2495 | `	VmResetTypedSlots(&(*pVm));` |
|        - | 2496 | `	/* (5) Unwind any active frames back to none. */` |
|       16 | 2497 | `	while( pVm->pFrame ){` |
|        8 | 2498 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 2499 | `	}` |
|        - | 2500 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        8 | 2501 | `	pVm->bInReset = 0;` |
|        - | 2502 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - | 2503 | `	 * slots (their indices no longer exist). */` |
|        8 | 2504 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        8 | 2505 | `	SySetReset(&pVm->aFreeObj);` |
|        - | 2506 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        8 | 2507 | `	SyHashRelease(&pVm->hSuper);` |
|        8 | 2508 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - | 2509 | `	/* (8) Drain remaining per-exec containers. */` |
|        8 | 2510 | `	SySetReset(&pVm->aSelf);` |
|        - | 2511 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - | 2512 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - | 2513 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        8 | 2514 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 | 2515 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 | 2516 | `		if( pCB ){` |
|        - | 2517 | `			int iArg;` |
|      ! 0 | 2518 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2519 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 | 2520 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 | 2521 | `			}` |
|      ! 0 | 2522 | `		}` |
|      ! 0 | 2523 | `	}` |
|        8 | 2524 | `	SySetReset(&pVm->aShutdown);` |
|        - | 2525 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|        - | 2526 | `	 * aborted program can leave entries behind). */` |
|        8 | 2527 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        8 | 2528 | `	SySetReset(&pVm->aException);` |
|        8 | 2529 | `	SySetReset(&pVm->aFinallyAction);` |
|        8 | 2530 | `	pVm->pPendingException = 0;` |
|        8 | 2531 | `	pVm->pInflightException = 0;` |
|        8 | 2532 | `	pVm->nInflightExcBase = 0;` |
|        8 | 2533 | `	pVm->pResumeFrame = 0;` |
|        8 | 2534 | `	pVm->iResumePc = 0;` |
|        8 | 2535 | `	pVm->pResumeInstr = 0;` |
|        8 | 2536 | `	pVm->iResumeStackDepth = 0;` |
|        8 | 2537 | `	pVm->nBoundaryRc = 0;` |
|        8 | 2538 | `	pVm->pConstEvalClass = 0;` |
|        8 | 2539 | `	pVm->nConstEvalDepth = 0;` |
|        8 | 2540 | `	pVm->pConstCycleAttr = 0;` |
|        8 | 2541 | `	pVm->pConstCycleClass = 0;` |
|        8 | 2542 | `	SySetReset(&pVm->aMagicGuard);` |
|        - | 2543 | `	{` |
|        - | 2544 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|        - | 2545 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        8 | 2546 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        8 | 2547 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|        - | 2548 | `		sxu32 iRmw;` |
|        8 | 2549 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|      ! 0 | 2550 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|      ! 0 | 2551 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|      ! 0 | 2552 | `		}` |
|        8 | 2553 | `		SySetReset(&pVm->aHookRmw);` |
|        - | 2554 | `	}` |
|        8 | 2555 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 | 2556 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 | 2557 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 | 2558 | `	}` |
|        8 | 2559 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        8 | 2560 | `	if( pVm->pHookSetThis ){` |
|      ! 0 | 2561 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 | 2562 | `		pVm->pHookSetThis = 0;` |
|      ! 0 | 2563 | `	}` |
|        8 | 2564 | `	pVm->pHookSetAttr = 0;` |
|        8 | 2565 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        8 | 2566 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 | 2567 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 | 2568 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 | 2569 | `	}` |
|        8 | 2570 | `	pVm->pMagicCallClass = 0;` |
|        8 | 2571 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        8 | 2572 | `	pVm->nExceptDepth = 0;` |
|        - | 2573 | `	/* spl_autoload_register() callbacks are per request */` |
|        8 | 2574 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 | 2575 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 | 2576 | `		if( pCB ){` |
|      ! 0 | 2577 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 | 2578 | `		}` |
|      ! 0 | 2579 | `	}` |
|        8 | 2580 | `	SySetReset(&pVm->aAutoload);` |
|        - | 2581 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - | 2582 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        8 | 2583 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 | 2584 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 | 2585 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 | 2586 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      ! 0 | 2587 | `	}` |
|        - | 2588 | `	/* Output buffers */` |
|        8 | 2589 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 | 2590 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 | 2591 | `		if( pOb ){` |
|      ! 0 | 2592 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 | 2593 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 | 2594 | `		}` |
|      ! 0 | 2595 | `	}` |
|        8 | 2596 | `	SySetReset(&pVm->aOB);` |
|        8 | 2597 | `	pVm->nObDepth = 0;` |
|        - | 2598 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - | 2599 | `	{` |
|        8 | 2600 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        8 | 2601 | `		if( rc == SXRET_OK ){` |
|        8 | 2602 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        4 | 2603 | `		}` |
|        8 | 2604 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2605 | `			return rc;` |
|        - | 2606 | `		}` |
|        - | 2607 | `	}` |
|        - | 2608 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|        - | 2609 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|        - | 2610 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|        - | 2611 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|        - | 2612 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|        - | 2613 | `	{` |
|        - | 2614 | `		SyHashEntry *pEntry;` |
|        8 | 2615 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1084 | 2616 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1076 | 2617 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 2618 | `			ph7_class_attr *pAttr;` |
|        - | 2619 | `			SyHashEntry *pAttrEntry;` |
|     1076 | 2620 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     6210 | 2621 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     4596 | 2622 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     4596 | 2623 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     1340 | 2624 | `					pAttr->nIdx = SXU32_HIGH;` |
|     1340 | 2625 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      670 | 2626 | `				}` |
|      ! 0 | 2627 | `			}` |
|      ! 0 | 2628 | `		}` |
|        8 | 2629 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     1084 | 2630 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     1076 | 2631 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     1076 | 2632 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2633 | `				return rc;` |
|        - | 2634 | `			}` |
|      ! 0 | 2635 | `		}` |
|        - | 2636 | `	}` |
|        - | 2637 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        8 | 2638 | `	SyBlobReset(&pVm->sConsumer);` |
|        8 | 2639 | `	pVm->nOutputLen = 0;` |
|        8 | 2640 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        8 | 2641 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        8 | 2642 | `	pVm->iResponseStatus = 200;` |
|        8 | 2643 | `	pVm->bHeadersSent = 0;` |
|        8 | 2644 | `	pVm->bHttpContext = 0;` |
|        8 | 2645 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        8 | 2646 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        8 | 2647 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        8 | 2648 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        8 | 2649 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        8 | 2650 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 2651 | `#ifdef PH7_ENABLE_PCRE` |
|        8 | 2652 | `	pVm->iPcreLastError = 0;` |
|        - | 2653 | `#endif` |
|        - | 2654 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2655 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        8 | 2656 | `	PH7_LibxmlVmReset(&(*pVm));` |
|        - | 2657 | `#endif` |
|        8 | 2658 | `	pVm->iCmpCallbackExc = 0;` |
|        8 | 2659 | `	pVm->bHaltRequested = 0;` |
|        8 | 2660 | `	pVm->iExitStatus = 0;` |
|        8 | 2661 | `	pVm->nSpreadCallBase = 0;` |
|        8 | 2662 | `	VmSpreadCaptureReset(pVm);` |
|        8 | 2663 | `	pVm->nRecursionDepth = 0;` |
|        8 | 2664 | `	pVm->pActiveCtx = 0;` |
|        8 | 2665 | `	pVm->pCoalesceObj = 0;` |
|        8 | 2666 | `	pVm->bCoalesceArmed = 0;` |
|        8 | 2667 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - | 2668 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        8 | 2669 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - | 2670 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|        - | 2671 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        8 | 2672 | `	pVm->nNextObjId = 1;` |
|        - | 2673 | `	/* Set the ready flag */` |
|        8 | 2674 | `	pVm->nMagic = PH7_VM_RUN;` |
|        8 | 2675 | `	return SXRET_OK;` |
|        4 | 2676 | `}` |
|        - | 2677 | `/*` |
|        - | 2678 | ` * Release a Virtual Machine.` |
|        - | 2679 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - | 2680 | ` */` |
|     3406 | 2681 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 | 2682 | `{` |
|        - | 2683 | `	/* Set the stale magic number */` |
|     3411 | 2684 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - | 2685 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 2686 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|        - | 2687 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|     3411 | 2688 | `	PH7_LibxmlVmRelease(pVm);` |
|        - | 2689 | `#endif` |
|        - | 2690 | `	/* Release the private memory subsystem */` |
|     3411 | 2691 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     3411 | 2692 | `	return SXRET_OK;` |
|        5 | 2693 | `}` |
|        - | 2694 | `/*` |
|        - | 2695 | ` * Initialize a foreign function call context.` |
|        - | 2696 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - | 2697 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - | 2698 | ` * functions.` |
|        - | 2699 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - | 2700 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - | 2701 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - | 2702 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - | 2703 | ` */` |
|   933848 | 2704 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|        - | 2705 | `	ph7_context *pOut,    /* Call Context */` |
|        - | 2706 | `	ph7_vm *pVm,          /* Target VM */` |
|        - | 2707 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - | 2708 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - | 2709 | `	sxi32 iFlags          /* Control flags */` |
|        - | 2710 | `	)` |
|        5 | 2711 | `{` |
|   933853 | 2712 | `	pOut->pFunc = pFunc;` |
|   933853 | 2713 | `	pOut->pVm   = pVm;` |
|   933853 | 2714 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   933853 | 2715 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - | 2716 | `	/* Assume a null return value */` |
|   933853 | 2717 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   933853 | 2718 | `	pOut->pRet = pRet;` |
|   933853 | 2719 | `	pOut->iFlags = iFlags;` |
|   933853 | 2720 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|   933853 | 2721 | `	return SXRET_OK;` |
|        5 | 2722 | `}` |
|        - | 2723 | `/*` |
|        - | 2724 | ` * Release a foreign function call context and cleanup the mess` |
|        - | 2725 | ` * left behind.` |
|        - | 2726 | ` */` |
|   933848 | 2727 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 | 2728 | `{` |
|        - | 2729 | `	sxu32 n;` |
|   933853 | 2730 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    11705 | 2731 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    68338 | 2732 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    56638 | 2733 | `			if( apObj[n] == 0 ){` |
|        - | 2734 | `				/* Already released */` |
|      682 | 2735 | `				continue;` |
|        - | 2736 | `			}` |
|    55961 | 2737 | `			PH7_MemObjRelease(apObj[n]);` |
|    55961 | 2738 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    27983 | 2739 | `		}` |
|    11705 | 2740 | `		SySetRelease(&pCtx->sVar);` |
|     5850 | 2741 | `	}` |
|   933853 | 2742 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - | 2743 | `		ph7_aux_data *aAux;` |
|        - | 2744 | `		void *pChunk;` |
|        - | 2745 | `		/* Automatic release of dynamically allocated chunk` |
|        - | 2746 | `		 * using [ph7_context_alloc_chunk()].` |
|        - | 2747 | `		 */` |
|      115 | 2748 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      337 | 2749 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      227 | 2750 | `			pChunk = aAux[n].pAuxData;` |
|        - | 2751 | `			/* Release the chunk */` |
|      227 | 2752 | `			if( pChunk ){` |
|      227 | 2753 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      111 | 2754 | `			}` |
|      116 | 2755 | `		}` |
|      115 | 2756 | `		SySetRelease(&pCtx->sChunk);` |
|       55 | 2757 | `	}` |
|   933853 | 2758 | `}` |
|        - | 2759 | `/*` |
|        - | 2760 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - | 2761 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - | 2762 | ` */` |
|      677 | 2763 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - | 2764 | `	ph7_context *pCtx, /* Call context */` |
|        - | 2765 | `	ph7_value *pValue  /* Release this value */` |
|        - | 2766 | `	)` |
|        5 | 2767 | `{` |
|      682 | 2768 | `	if( pValue == 0 ){` |
|        - | 2769 | `		/* NULL value is a harmless operation */` |
|      ! 0 | 2770 | `		return;` |
|        - | 2771 | `	}` |
|      682 | 2772 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      682 | 2773 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - | 2774 | `		sxu32 n;` |
|     1536 | 2775 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1536 | 2776 | `			if( apObj[n] == pValue ){` |
|      682 | 2777 | `				PH7_MemObjRelease(pValue);` |
|      682 | 2778 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - | 2779 | `				/* Mark as released */` |
|      682 | 2780 | `				apObj[n] = 0;` |
|      682 | 2781 | `				break;` |
|        - | 2782 | `			}` |
|      431 | 2783 | `		}` |
|      338 | 2784 | `	}` |
|      343 | 2785 | `}` |
|        - | 2786 | `/*` |
|        - | 2787 | ` * Pop and release as many memory object from the operand stack.` |
|        - | 2788 | ` */` |
|  5675745 | 2789 | `PH7_PRIVATE void VmPopOperand(` |
|        - | 2790 | `	ph7_value **ppTos, /* Operand stack */` |
|        - | 2791 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - | 2792 | `	)` |
|        5 | 2793 | `{` |
|  5675750 | 2794 | `	ph7_value *pTos = *ppTos;` |
| 12079522 | 2795 | `	while( nPop > 0 ){` |
|  6403777 | 2796 | `		PH7_MemObjRelease(pTos);` |
|  6403777 | 2797 | `		pTos--;` |
|  6403777 | 2798 | `		nPop--;` |
|        5 | 2799 | `	}` |
|        - | 2800 | `	/* Top of the stack */` |
|  5675750 | 2801 | `	*ppTos = pTos;` |
|  5675750 | 2802 | `}` |
|        - | 2803 | `/*` |
|        - | 2804 | ` * Reserve a memory object.` |
|        - | 2805 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - | 2806 | ` */` |
|  4368023 | 2807 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 | 2808 | `{` |
|  4368028 | 2809 | `	ph7_value *pObj = 0;` |
|        - | 2810 | `	VmSlot *pSlot;` |
|        - | 2811 | `	sxu32 nIdx;` |
|        - | 2812 | `	/* Check for a free slot */` |
|  4368028 | 2813 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  4368028 | 2814 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  4368028 | 2815 | `	if( pSlot ){` |
|  2130456 | 2816 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  2130456 | 2817 | `		nIdx = pSlot->nIdx;` |
|  1065840 | 2818 | `	}` |
|  4368028 | 2819 | `	if( pObj == 0 ){` |
|        - | 2820 | `		/* Reserve a new memory object */` |
|  2237577 | 2821 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2237577 | 2822 | `		if( pObj == 0 ){` |
|      ! 0 | 2823 | `			return 0;` |
|        - | 2824 | `		}` |
|  1118786 | 2825 | `	}` |
|        - | 2826 | `	/* Set a null default value */` |
|  4368028 | 2827 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  4368028 | 2828 | `	pObj->nIdx = nIdx;` |
|  4368028 | 2829 | `	return pObj;` |
|  2184631 | 2830 | `}` |
|        - | 2831 | `/*` |
|        - | 2832 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - | 2833 | ` */` |
|    46290 | 2834 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|        - | 2835 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - | 2836 | `	const char *zKey,  /* Entry key */` |
|        - | 2837 | `	sxu32 nByte,       /* Key length */` |
|        - | 2838 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - | 2839 | `	)` |
|        5 | 2840 | `{` |
|        - | 2841 | `	ph7_value sKey;` |
|        - | 2842 | `	sxi32 rc;` |
|    46295 | 2843 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    46295 | 2844 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - | 2845 | `	/* Perform the insertion */` |
|    46295 | 2846 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    46295 | 2847 | `	PH7_MemObjRelease(&sKey);` |
|    46295 | 2848 | `	return rc;` |
|        5 | 2849 | `}` |
|        - | 2850 | `/*` |
|        - | 2851 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|        - | 2852 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|        - | 2853 | ` * key must create a real global variable — linked into the bottom frame's` |
|        - | 2854 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|        - | 2855 | ` * variable created by top-level code — so later reads and writes alias one` |
|        - | 2856 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|        - | 2857 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|        - | 2858 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|        - | 2859 | ` *     superglobal in place.` |
|        - | 2860 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|        - | 2861 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). Rebinding an EXISTING` |
|        - | 2862 | ` *     name is rejected with the engine's usual "already exists" diagnostic` |
|        - | 2863 | ` *     (the same limitation OP_STORE_REF has for plain variables).` |
|        - | 2864 | ` */` |
|      142 | 2865 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|        2 | 2866 | `{` |
|      144 | 2867 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 2868 | `	SyHashEntry *pEntry;` |
|        - | 2869 | `	ph7_value *pObj;` |
|        - | 2870 | `	char *zDup;` |
|        - | 2871 | `	sxu32 nIdx;` |
|        - | 2872 | `	sxi32 rc;` |
|        - | 2873 | `	/* Walk down to the global frame */` |
|      148 | 2874 | `	while( pFrame->pParent ){` |
|        5 | 2875 | `		pFrame = pFrame->pParent;` |
|        1 | 2876 | `	}` |
|        - | 2877 | `	/* An existing global (or superglobal) is overwritten in place */` |
|      144 | 2878 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      144 | 2879 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|        - | 2880 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|        - | 2881 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|        - | 2882 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|        - | 2883 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|        - | 2884 | `		 * of $GLOBALS itself). */` |
|        5 | 2885 | `		pEntry = 0;` |
|        2 | 2886 | `	}` |
|      144 | 2887 | `	if( pEntry == 0 ){` |
|      144 | 2888 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       71 | 2889 | `	}` |
|      144 | 2890 | `	if( pEntry ){` |
|        3 | 2891 | `		if( nRefIdx != SXU32_HIGH ){` |
|        - | 2892 | `			SyString sName;` |
|      ! 0 | 2893 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|      ! 0 | 2894 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 | 2895 | `			return SXRET_OK;` |
|        - | 2896 | `		}` |
|        3 | 2897 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|        3 | 2898 | `		if( pObj == 0 ){` |
|      ! 0 | 2899 | `			return SXERR_NOTFOUND;` |
|        - | 2900 | `		}` |
|        3 | 2901 | `		if( pValue ){` |
|        3 | 2902 | `			PH7_MemObjStore(pValue,pObj);` |
|        2 | 2903 | `		}else{` |
|      ! 0 | 2904 | `			PH7_MemObjToNull(pObj);` |
|        - | 2905 | `		}` |
|        3 | 2906 | `		return SXRET_OK;` |
|        - | 2907 | `	}` |
|      142 | 2908 | `	if( nRefIdx == SXU32_HIGH ){` |
|        - | 2909 | `		/* Reserve a fresh slot for the new global */` |
|      140 | 2910 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|      140 | 2911 | `		if( pObj == 0 ){` |
|      ! 0 | 2912 | `			return SXERR_MEM;` |
|        - | 2913 | `		}` |
|      140 | 2914 | `		nIdx = pObj->nIdx;` |
|       71 | 2915 | `	}else{` |
|        - | 2916 | `		/* Reference assignment: bind the name to the existing slot */` |
|        3 | 2917 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|        3 | 2918 | `		if( pObj == 0 ){` |
|      ! 0 | 2919 | `			return SXERR_NOTFOUND;` |
|        - | 2920 | `		}` |
|        3 | 2921 | `		nIdx = nRefIdx;` |
|        - | 2922 | `	}` |
|      142 | 2923 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|      142 | 2924 | `	if( zDup == 0 ){` |
|      ! 0 | 2925 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2926 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|        - | 2927 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|        - | 2928 | `			VmSlot sFree;` |
|      ! 0 | 2929 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 2930 | `			sFree.pUserData = 0;` |
|      ! 0 | 2931 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 2932 | `		}` |
|      ! 0 | 2933 | `		return SXERR_MEM;` |
|        - | 2934 | `	}` |
|      142 | 2935 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|      142 | 2936 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2937 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - | 2938 | `			VmSlot sFree;` |
|      ! 0 | 2939 | `			sFree.nIdx = nIdx;` |
|      ! 0 | 2940 | `			sFree.pUserData = 0;` |
|      ! 0 | 2941 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|      ! 0 | 2942 | `		}` |
|      ! 0 | 2943 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 2944 | `		return rc;` |
|        - | 2945 | `	}` |
|        - | 2946 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|      142 | 2947 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|      142 | 2948 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      142 | 2949 | `	if( nRefIdx == SXU32_HIGH ){` |
|      140 | 2950 | `		pObj->nIdx = nIdx;` |
|      140 | 2951 | `		if( pValue ){` |
|      140 | 2952 | `			PH7_MemObjStore(pValue,pObj);` |
|       69 | 2953 | `		}` |
|       69 | 2954 | `	}` |
|      142 | 2955 | `	return SXRET_OK;` |
|       73 | 2956 | `}` |
|        - | 2957 | `/*` |
|        - | 2958 | ` * Extract a variable value from the top active VM frame.` |
|        - | 2959 | ` * Return a pointer to the variable value on success.` |
|        - | 2960 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - | 2961 | ` */` |
|  5811455 | 2962 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|        - | 2963 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 2964 | `	const SyString *pName, /* Variable name */` |
|        - | 2965 | `	int bDup,              /* True to duplicate variable name */` |
|        - | 2966 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - | 2967 | `	)` |
|        5 | 2968 | `{` |
|  5811460 | 2969 | `	int bNullify = FALSE;` |
|        - | 2970 | `	SyHashEntry *pEntry;` |
|        - | 2971 | `	VmFrame *pFrame;` |
|        - | 2972 | `	ph7_value *pObj;` |
|        - | 2973 | `	sxu32 nIdx;` |
|        - | 2974 | `	sxi32 rc;` |
|        - | 2975 | `	/* Point to the top active frame */` |
|  5811460 | 2976 | `	pFrame = pVm->pFrame;` |
|  5811460 | 2977 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 2978 | `	/* Perform the lookup */` |
|  5811460 | 2979 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - | 2980 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 | 2981 | `		pName = &sAnnon;` |
|        - | 2982 | `		/* Always nullify the object */` |
|      ! 0 | 2983 | `		bNullify = TRUE;` |
|      ! 0 | 2984 | `		bDup = FALSE;` |
|      ! 0 | 2985 | `	}` |
|        - | 2986 | `	/* Check the superglobals table first */` |
|  5811460 | 2987 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  5811460 | 2988 | `	if( pEntry == 0 ){` |
|        - | 2989 | `		/* Query the top active frame */` |
|  5811056 | 2990 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  5811056 | 2991 | `		if( pEntry == 0 ){` |
|   498945 | 2992 | `			char *zName = (char *)pName->zString;` |
|        - | 2993 | `			VmSlot sLocal;` |
|   498945 | 2994 | `			if( !bCreate ){` |
|        - | 2995 | `				/* Do not create the variable,return NULL instead */` |
|     1161 | 2996 | `				return 0;` |
|        - | 2997 | `			}` |
|        - | 2998 | `			/* No such variable,automatically create a new one and install` |
|        - | 2999 | `			 * it in the current frame.` |
|        - | 3000 | `			 */` |
|   497789 | 3001 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   497789 | 3002 | `			if( pObj == 0 ){` |
|      ! 0 | 3003 | `				return 0;` |
|        - | 3004 | `			}` |
|   497789 | 3005 | `			nIdx = pObj->nIdx;` |
|   497789 | 3006 | `			if( bDup ){` |
|        - | 3007 | `				/* Duplicate name */` |
|      524 | 3008 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      524 | 3009 | `				if( zName == 0 ){` |
|      ! 0 | 3010 | `					return 0;` |
|        - | 3011 | `				}` |
|      260 | 3012 | `			}` |
|        - | 3013 | `			/* Link to the top active VM frame */` |
|   497789 | 3014 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   497789 | 3015 | `			if( rc != SXRET_OK ){` |
|        - | 3016 | `				/* Return the slot to the free pool */` |
|      ! 0 | 3017 | `				sLocal.nIdx = nIdx;` |
|      ! 0 | 3018 | `				sLocal.pUserData = 0;` |
|      ! 0 | 3019 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 | 3020 | `				return 0;` |
|        - | 3021 | `			}` |
|   497789 | 3022 | `			if( pFrame->pParent != 0 ){` |
|        - | 3023 | `				/* Local variable */` |
|   489215 | 3024 | `				sLocal.nIdx = nIdx;` |
|   489215 | 3025 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   245225 | 3026 | `			}else{` |
|        - | 3027 | `				/* Register in the $GLOBALS array */` |
|     8579 | 3028 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - | 3029 | `			}` |
|        - | 3030 | `			/* Install in the reference table */` |
|   497789 | 3031 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - | 3032 | `			/* Save object index */` |
|   497789 | 3033 | `			pObj->nIdx = nIdx;` |
|   249512 | 3034 | `		}else{` |
|        - | 3035 | `			/* Extract variable contents */` |
|  5312116 | 3036 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5312116 | 3037 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  5312116 | 3038 | `			if( bNullify && pObj ){` |
|      ! 0 | 3039 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 | 3040 | `			}` |
|        - | 3041 | `		}` |
|  2907626 | 3042 | `	}else{` |
|        - | 3043 | `		/* Superglobal */` |
|      409 | 3044 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      409 | 3045 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 3046 | `	}` |
|  5810304 | 3047 | `	return pObj;` |
|  2908406 | 3048 | `}` |
|        - | 3049 | `/*` |
|        - | 3050 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - | 3051 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - | 3052 | ` */` |
|    31062 | 3053 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - | 3054 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3055 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - | 3056 | `	sxu32 nByte        /* zName length */` |
|        - | 3057 | `	)` |
|        5 | 3058 | `{` |
|        - | 3059 | `	SyHashEntry *pEntry;` |
|        - | 3060 | `	ph7_value *pValue;` |
|        - | 3061 | `	sxu32 nIdx;` |
|        - | 3062 | `	/* Query the superglobal table */` |
|    31067 | 3063 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    31067 | 3064 | `	if( pEntry == 0 ){` |
|        - | 3065 | `		/* No such entry */` |
|      ! 0 | 3066 | `		return 0;` |
|        - | 3067 | `	}` |
|        - | 3068 | `	/* Extract the superglobal index in the global object pool */` |
|    31067 | 3069 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3070 | `	/* Extract the variable value  */` |
|    31067 | 3071 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    31067 | 3072 | `	return pValue;` |
|    15536 | 3073 | `}` |
|        - | 3074 | `/*` |
|        - | 3075 | ` * Perform a raw hashmap insertion.` |
|        - | 3076 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - | 3077 | ` */` |
|    24336 | 3078 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - | 3079 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - | 3080 | `	const char *zKey,   /* Entry key */` |
|        - | 3081 | `	int nKeylen,        /* zKey length*/` |
|        - | 3082 | `	const char *zData,  /* Entry data */` |
|        - | 3083 | `	int nLen            /* zData length */` |
|        - | 3084 | `	)` |
|        5 | 3085 | `{` |
|        - | 3086 | `	ph7_value sKey,sValue;` |
|        - | 3087 | `	sxi32 rc;` |
|    24341 | 3088 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    24341 | 3089 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|    24341 | 3090 | `	if( zKey ){` |
|    20929 | 3091 | `		if( nKeylen < 0 ){` |
|    20825 | 3092 | `			nKeylen = (int)SyStrlen(zKey);` |
|    10410 | 3093 | `		}` |
|    20929 | 3094 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|    10462 | 3095 | `	}` |
|    24341 | 3096 | `	if( zData ){` |
|    24341 | 3097 | `		if( nLen < 0 ){` |
|        - | 3098 | `			/* Compute length automatically */` |
|    13811 | 3099 | `			nLen = (int)SyStrlen(zData);` |
|     6903 | 3100 | `		}` |
|    24341 | 3101 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|    12168 | 3102 | `	}` |
|        - | 3103 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|        - | 3104 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|        - | 3105 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|        - | 3106 | `	 * every argument under "". */` |
|    24341 | 3107 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|    24341 | 3108 | `	PH7_MemObjRelease(&sKey);` |
|    24341 | 3109 | `	PH7_MemObjRelease(&sValue);` |
|    24341 | 3110 | `	return rc;` |
|        5 | 3111 | `}` |
|        - | 3112 | `/*` |
|        - | 3113 | ` * Configure a working virtual machine instance.` |
|        - | 3114 | ` *` |
|        - | 3115 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - | 3116 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - | 3117 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - | 3118 | ` * The second argument to this function is an integer configuration option` |
|        - | 3119 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - | 3120 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - | 3121 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - | 3122 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - | 3123 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - | 3124 | ` */` |
|    89186 | 3125 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - | 3126 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 3127 | `	sxi32 nOp,   /* Configuration verb */` |
|        - | 3128 | `	va_list ap   /* Subsequent option arguments */` |
|        - | 3129 | `	)` |
|        5 | 3130 | `{` |
|    89191 | 3131 | `	sxi32 rc = SXRET_OK;` |
|    89191 | 3132 | `	switch(nOp){` |
|     1690 | 3133 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     3385 | 3134 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     3385 | 3135 | `		void *pUserData = va_arg(ap,void *);` |
|        - | 3136 | `		/* VM output consumer callback */` |
|        - | 3137 | `#ifdef UNTRUST` |
|        - | 3138 | `		if( xConsumer == 0 ){` |
|        - | 3139 | `			rc = SXERR_CORRUPT;` |
|        - | 3140 | `			break;` |
|        - | 3141 | `		}` |
|        - | 3142 | `#endif` |
|        - | 3143 | `		/* Install the output consumer */` |
|     3385 | 3144 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     3385 | 3145 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     3385 | 3146 | `		break;` |
|        - | 3147 | `							   }` |
|     1703 | 3148 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - | 3149 | `		/* Import path */` |
|        - | 3150 | `		  const char *zPath;` |
|        - | 3151 | `		  SyString sPath;` |
|     3411 | 3152 | `		  zPath = va_arg(ap,const char *);` |
|        - | 3153 | `#if defined(UNTRUST)` |
|        - | 3154 | `		  if( zPath == 0 ){` |
|        - | 3155 | `			  rc = SXERR_EMPTY;` |
|        - | 3156 | `			  break;` |
|        - | 3157 | `		  }` |
|        - | 3158 | `#endif` |
|     3411 | 3159 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - | 3160 | `		  /* Remove trailing slashes and backslashes */` |
|        - | 3161 | `#ifdef __WINNT__` |
|        5 | 3162 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - | 3163 | `#endif` |
|     6817 | 3164 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - | 3165 | `		  /* Remove leading and trailing white spaces */` |
|     3411 | 3166 | `		  SyStringFullTrim(&sPath);` |
|     3411 | 3167 | `		  if( sPath.nByte > 0 ){` |
|        - | 3168 | `			  /* Store the path in the corresponding conatiner */` |
|     3411 | 3169 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1703 | 3170 | `		  }` |
|     3411 | 3171 | `		  break;` |
|        - | 3172 | `									 }` |
|     1706 | 3173 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - | 3174 | `		/* Run-Time Error report */` |
|     3417 | 3175 | `		pVm->bErrReport = 1;` |
|     3417 | 3176 | `		pVm->iErrMask = 30719; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|     3417 | 3177 | `		break;` |
|        2 | 3178 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - | 3179 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|        - | 3180 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|        - | 3181 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|        - | 3182 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|        - | 3183 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|        - | 3184 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|        - | 3185 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|        - | 3186 | `		 * would otherwise read as an enormous positive cap). */` |
|        5 | 3187 | `		int nDepth = va_arg(ap,int);` |
|        5 | 3188 | `		if( nDepth >= 0 ){` |
|        5 | 3189 | `			pVm->nMaxDepth = nDepth;` |
|        2 | 3190 | `		}` |
|        5 | 3191 | `		break;` |
|        - | 3192 | `									   }` |
|        5 | 3193 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|        - | 3194 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|        - | 3195 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|        - | 3196 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|        - | 3197 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|        - | 3198 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|        - | 3199 | `		 * so it is rejected as a footgun). */` |
|       12 | 3200 | `		int nDepth = va_arg(ap,int);` |
|       12 | 3201 | `		if( nDepth > 1 ){` |
|       12 | 3202 | `			pVm->nMaxNativeDepth = nDepth;` |
|        5 | 3203 | `		}` |
|       12 | 3204 | `		break;` |
|        - | 3205 | `									   }` |
|      ! 0 | 3206 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - | 3207 | `		/* VM output length in bytes */` |
|      ! 0 | 3208 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - | 3209 | `#ifdef UNTRUST` |
|        - | 3210 | `		if( pOut == 0 ){` |
|        - | 3211 | `			rc = SXERR_CORRUPT;` |
|        - | 3212 | `			break;` |
|        - | 3213 | `		}` |
|        - | 3214 | `#endif` |
|      ! 0 | 3215 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 | 3216 | `		break;` |
|        - | 3217 | `							   }` |
|        - | 3218 |  |
|    18770 | 3219 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - | 3220 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - | 3221 | `		/* Create a new superglobal/global variable */` |
|    37545 | 3222 | `		const char *zName = va_arg(ap,const char *);` |
|    37545 | 3223 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - | 3224 | `		SyHashEntry *pEntry;` |
|        - | 3225 | `		ph7_value *pObj;` |
|        - | 3226 | `		sxu32 nByte;` |
|        - | 3227 | `		sxu32 nIdx;` |
|        - | 3228 | `#ifdef UNTRUST` |
|        - | 3229 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - | 3230 | `			rc = SXERR_CORRUPT;` |
|        - | 3231 | `			break;` |
|        - | 3232 | `		}` |
|        - | 3233 | `#endif` |
|    37545 | 3234 | `		nByte = SyStrlen(zName);` |
|    37545 | 3235 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3236 | `			/* Check if the superglobal is already installed */` |
|    34165 | 3237 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    17085 | 3238 | `		}else{` |
|        - | 3239 | `			/* Query the top active VM frame */` |
|     3385 | 3240 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - | 3241 | `		}` |
|    37545 | 3242 | `		if( pEntry ){` |
|        - | 3243 | `			/* Variable already installed */` |
|      ! 0 | 3244 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - | 3245 | `			/* Extract contents */` |
|      ! 0 | 3246 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 | 3247 | `			if( pObj ){` |
|        - | 3248 | `				/* Overwrite old contents */` |
|      ! 0 | 3249 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 | 3250 | `			}` |
|      ! 0 | 3251 | `		}else{` |
|        - | 3252 | `			/* Install a new variable */` |
|    37545 | 3253 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    37545 | 3254 | `			if( pObj == 0 ){` |
|      ! 0 | 3255 | `				rc = SXERR_MEM;` |
|      ! 0 | 3256 | `				break;` |
|        - | 3257 | `			}` |
|    37545 | 3258 | `			nIdx = pObj->nIdx;` |
|        - | 3259 | `			/* Copy value */` |
|    37545 | 3260 | `			PH7_MemObjStore(pValue,pObj);` |
|    37545 | 3261 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - | 3262 | `				/* Install the superglobal */` |
|    34165 | 3263 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    17085 | 3264 | `			}else{` |
|        - | 3265 | `				/* Install in the current frame */` |
|     3385 | 3266 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - | 3267 | `			}` |
|    37545 | 3268 | `			if( rc == SXRET_OK ){` |
|        - | 3269 | `				SyHashEntry *pRef;` |
|    37545 | 3270 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    34165 | 3271 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    17085 | 3272 | `				}else{` |
|     3385 | 3273 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - | 3274 | `				}` |
|        - | 3275 | `				/* Install in the reference table */` |
|    37545 | 3276 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    37545 | 3277 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - | 3278 | `					/* Register in the $GLOBALS array */` |
|    37545 | 3279 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    18770 | 3280 | `				}` |
|    18770 | 3281 | `			}` |
|        - | 3282 | `		}` |
|    37545 | 3283 | `		break;` |
|        - | 3284 | `									}` |
|    10410 | 3285 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - | 3286 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - | 3287 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - | 3288 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - | 3289 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - | 3290 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - | 3291 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|    20825 | 3292 | `		const char *zKey   = va_arg(ap,const char *);` |
|    20825 | 3293 | `		const char *zValue = va_arg(ap,const char *);` |
|    20825 | 3294 | `		int nLen = va_arg(ap,int);` |
|        - | 3295 | `		ph7_hashmap *pMap;` |
|        - | 3296 | `		ph7_value *pValue;` |
|    20825 | 3297 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - | 3298 | `			/* Extract the $_ENV superglobal */` |
|        3 | 3299 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|    20824 | 3300 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - | 3301 | `			/* Extract the $_POST superglobal */` |
|      ! 0 | 3302 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|    20823 | 3303 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - | 3304 | `			/* Extract the $_GET superglobal */` |
|      ! 0 | 3305 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    20823 | 3306 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - | 3307 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 | 3308 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    20823 | 3309 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - | 3310 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 | 3311 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|    20823 | 3312 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - | 3313 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 | 3314 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 | 3315 | `		}else{` |
|        - | 3316 | `			/* Extract the $_SERVER superglobal */` |
|    20823 | 3317 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - | 3318 | `		}` |
|    20825 | 3319 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3320 | `			/* No such entry */` |
|      ! 0 | 3321 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3322 | `			break;` |
|        - | 3323 | `		}` |
|        - | 3324 | `		/* Point to the hashmap */` |
|    20825 | 3325 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3326 | `		/* Perform the insertion */` |
|    20825 | 3327 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|    20825 | 3328 | `		break;` |
|        - | 3329 | `								   }` |
|     1707 | 3330 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - | 3331 | `		/* Script arguments */` |
|     3419 | 3332 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3333 | `		ph7_hashmap *pMap;` |
|        - | 3334 | `		ph7_value *pValue;` |
|        - | 3335 | `		sxu32 n;` |
|     3419 | 3336 | `		if( SX_EMPTY_STR(zValue) ){` |
|        2 | 3337 | `			rc = SXERR_EMPTY;` |
|        2 | 3338 | `			break;` |
|        - | 3339 | `		}` |
|        - | 3340 | `		/* Extract the $argv array */` |
|     3417 | 3341 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3417 | 3342 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 3343 | `			/* No such entry */` |
|      ! 0 | 3344 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3345 | `			break;` |
|        - | 3346 | `		}` |
|        - | 3347 | `		/* Point to the hashmap */` |
|     3417 | 3348 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 3349 | `		/* Perform the insertion */` |
|     3417 | 3350 | `		n = (sxu32)SyStrlen(zValue);` |
|     3417 | 3351 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|     3417 | 3352 | `		if( rc == SXRET_OK ){` |
|     3417 | 3353 | `			if( pMap->nEntry > 1 ){` |
|        - | 3354 | `				/* Append space separator first */` |
|       37 | 3355 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|       16 | 3356 | `			}` |
|     3417 | 3357 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|     1706 | 3358 | `		}` |
|     3417 | 3359 | `		break;` |
|        - | 3360 | `								  }` |
|     1690 | 3361 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|        - | 3362 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|        - | 3363 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|        - | 3364 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|        - | 3365 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|        - | 3366 | `		ph7_value *pArgv,*pServer;` |
|        - | 3367 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|        - | 3368 | `		ph7_value sArgvVal,sKey,sCount;` |
|     3385 | 3369 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     3385 | 3370 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|     3380 | 3371 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|     3385 | 3372 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 | 3373 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 | 3374 | `			break;` |
|        - | 3375 | `		}` |
|     3385 | 3376 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|     3385 | 3377 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|        - | 3378 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|     3385 | 3379 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|     3385 | 3380 | `		if( pDup == 0 ){` |
|      ! 0 | 3381 | `			rc = SXERR_MEM;` |
|      ! 0 | 3382 | `			break;` |
|        - | 3383 | `		}` |
|     3385 | 3384 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|     3385 | 3385 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|     3385 | 3386 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3385 | 3387 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|     3385 | 3388 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|     3385 | 3389 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|     3385 | 3390 | `		PH7_MemObjRelease(&sKey);` |
|        - | 3391 | `		/* $_SERVER['argc'] = count($argv). */` |
|     3385 | 3392 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|     3385 | 3393 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     3385 | 3394 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|     3385 | 3395 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|     3385 | 3396 | `		PH7_MemObjRelease(&sCount);` |
|     3385 | 3397 | `		PH7_MemObjRelease(&sKey);` |
|     3385 | 3398 | `		rc = SXRET_OK;` |
|     3385 | 3399 | `		break;` |
|        - | 3400 | `								  }` |
|       29 | 3401 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|        - | 3402 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|        - | 3403 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|        - | 3404 | `		 * apply immediately so they take effect even if the script never` |
|        - | 3405 | `		 * touches the INI API. */` |
|       62 | 3406 | `		const char *zName = va_arg(ap,const char *);` |
|       62 | 3407 | `		const char *zValue = va_arg(ap,const char *);` |
|        - | 3408 | `		VmIniEntry sEntry;` |
|        - | 3409 | `		char *zDupN,*zDupV;` |
|        - | 3410 | `		sxu32 nName,nValue;` |
|       62 | 3411 | `		if( SX_EMPTY_STR(zName) ){` |
|      ! 0 | 3412 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3413 | `			break;` |
|        - | 3414 | `		}` |
|       62 | 3415 | `		if( zValue == 0 ){` |
|      ! 0 | 3416 | `			zValue = "";` |
|      ! 0 | 3417 | `		}` |
|       62 | 3418 | `		nName = (sxu32)SyStrlen(zName);` |
|       62 | 3419 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       62 | 3420 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       62 | 3421 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       62 | 3422 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|      ! 0 | 3423 | `			rc = SXERR_MEM;` |
|      ! 0 | 3424 | `			break;` |
|        - | 3425 | `		}` |
|       62 | 3426 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       62 | 3427 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       62 | 3428 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       62 | 3429 | `		if( rc == SXRET_OK ){` |
|       58 | 3430 | `			if( nName == sizeof("error_reporting")-1` |
|       52 | 3431 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|        2 | 3432 | `				sxi64 iLevel = 0;` |
|        2 | 3433 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|        2 | 3434 | `				pVm->bErrReport = iLevel != 0;` |
|       57 | 3435 | `			}else if( nName == sizeof("date.timezone")-1` |
|       28 | 3436 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|      ! 0 | 3437 | `			 && nValue == 3` |
|        4 | 3438 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|      ! 0 | 3439 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|      ! 0 | 3440 | `				pVm->zDefTz[3] = 0;` |
|      ! 0 | 3441 | `				pVm->nDefTz = 3;` |
|       56 | 3442 | `			}else if( nName == sizeof("zend.assertions")-1` |
|       50 | 3443 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|        - | 3444 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|        - | 3445 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|        - | 3446 | `				 * effect even before the INI chunk is seeded. */` |
|       40 | 3447 | `				sxi64 iZend = 0;` |
|       40 | 3448 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|       40 | 3449 | `				if( iZend >= 1 ){` |
|       40 | 3450 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|       22 | 3451 | `				}else{` |
|      ! 0 | 3452 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|        - | 3453 | `				}` |
|       18 | 3454 | `			}` |
|       29 | 3455 | `		}` |
|       62 | 3456 | `		break;` |
|        - | 3457 | `								  }` |
|      ! 0 | 3458 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - | 3459 | `		/* error_log() consumer */` |
|      ! 0 | 3460 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 | 3461 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 | 3462 | `		break;` |
|        - | 3463 | `										}` |
|      ! 0 | 3464 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - | 3465 | `		/* Script return value */` |
|      ! 0 | 3466 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - | 3467 | `#ifdef UNTRUST` |
|        - | 3468 | `		if( ppValue == 0 ){` |
|        - | 3469 | `			rc = SXERR_CORRUPT;` |
|        - | 3470 | `			break;` |
|        - | 3471 | `		}` |
|        - | 3472 | `#endif` |
|      ! 0 | 3473 | `		*ppValue = &pVm->sExec;` |
|      ! 0 | 3474 | `		break;` |
|        - | 3475 | `								   }` |
|     6817 | 3476 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - | 3477 | `		/* Register an IO stream device */` |
|    13639 | 3478 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - | 3479 | `		/* Make sure we are dealing with a valid IO stream */` |
|    13634 | 3480 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|    13639 | 3481 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - | 3482 | `				/* Invalid stream */` |
|      ! 0 | 3483 | `				rc = SXERR_INVALID;` |
|      ! 0 | 3484 | `				break;` |
|        - | 3485 | `		}` |
|    13639 | 3486 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - | 3487 | `			/* Make the 'file://' stream the defaut stream device */` |
|     3413 | 3488 | `			pVm->pDefStream = pStream;` |
|     1704 | 3489 | `		}` |
|        - | 3490 | `		/* Insert in the appropriate container */` |
|    13639 | 3491 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|    13639 | 3492 | `		break;` |
|        - | 3493 | `								  }` |
|       16 | 3494 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - | 3495 | `		/* Point to the VM internal output consumer buffer */` |
|       32 | 3496 | `		const void **ppOut = va_arg(ap,const void **);` |
|       32 | 3497 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - | 3498 | `#ifdef UNTRUST` |
|        - | 3499 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - | 3500 | `			rc = SXERR_CORRUPT;` |
|        - | 3501 | `			break;` |
|        - | 3502 | `		}` |
|        - | 3503 | `#endif` |
|       32 | 3504 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       32 | 3505 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       32 | 3506 | `		break;` |
|        - | 3507 | `									   }` |
|       16 | 3508 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - | 3509 | `		/* Raw HTTP request*/` |
|       32 | 3510 | `		const char *zRequest = va_arg(ap,const char *);` |
|       32 | 3511 | `		int nByte = va_arg(ap,int);` |
|       32 | 3512 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 | 3513 | `			rc = SXERR_EMPTY;` |
|      ! 0 | 3514 | `			break;` |
|        - | 3515 | `		}` |
|       32 | 3516 | `		if( nByte < 0 ){` |
|        - | 3517 | `			/* Compute length automatically */` |
|      ! 0 | 3518 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 | 3519 | `		}` |
|        - | 3520 | `		/* Process the request */` |
|       32 | 3521 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - | 3522 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       32 | 3523 | `		if( rc == SXRET_OK ){` |
|       30 | 3524 | `			pVm->bHttpContext = 1;` |
|       15 | 3525 | `		}` |
|       32 | 3526 | `		break;` |
|        - | 3527 | `									}` |
|       16 | 3528 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - | 3529 | `		/* Extract HTTP response status code */` |
|       32 | 3530 | `		int *pStatus = va_arg(ap, int *);` |
|       32 | 3531 | `		if( pStatus ){` |
|       32 | 3532 | `			*pStatus = pVm->iResponseStatus;` |
|       16 | 3533 | `		}` |
|       32 | 3534 | `		break;` |
|        - | 3535 | `										}` |
|       16 | 3536 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - | 3537 | `		/* Iterate response headers via callback */` |
|        - | 3538 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       32 | 3539 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       32 | 3540 | `		void *pUserData = va_arg(ap, void *);` |
|       32 | 3541 | `		if( xCallback ){` |
|       32 | 3542 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       32 | 3543 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       44 | 3544 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 | 3545 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 | 3546 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 | 3547 | `							   pUserData);` |
|       12 | 3548 | `				if( rc != PH7_OK ){` |
|      ! 0 | 3549 | `					break;` |
|        - | 3550 | `				}` |
|        6 | 3551 | `			}` |
|       16 | 3552 | `		}` |
|       32 | 3553 | `		break;` |
|        - | 3554 | `										 }` |
|      ! 0 | 3555 | `	default:` |
|        - | 3556 | `		/* Unknown configuration option */` |
|      ! 0 | 3557 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 | 3558 | `		break;` |
|        - | 3559 | `	}` |
|    89191 | 3560 | `	return rc;` |
|        5 | 3561 | `}` |
|        - | 3562 | `/* Forward declaration */` |
|        - | 3563 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - | 3564 | `/*` |
|        - | 3565 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - | 3566 | ` * format.` |
|        - | 3567 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - | 3568 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - | 3569 | ` * (STDOUT).` |
|        - | 3570 | ` */` |
|        2 | 3571 | `static sxi32 VmByteCodeDump(` |
|        - | 3572 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - | 3573 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - | 3574 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 3575 | `	)` |
|        1 | 3576 | `{` |
|        - | 3577 | `	static const char zDump[] = {` |
|        - | 3578 | `		"====================================================\n"` |
|        - | 3579 | `		"PH7 VM Dump\n"` |
|        - | 3580 | `		"====================================================\n"` |
|        - | 3581 | `	};` |
|        - | 3582 | `	VmInstr *pInstr,*pEnd;` |
|        3 | 3583 | `	sxi32 rc = SXRET_OK;` |
|        - | 3584 | `	sxu32 n;` |
|        - | 3585 | `	/* Point to the PH7 instructions */` |
|        3 | 3586 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 | 3587 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 | 3588 | `	n = 0;` |
|        3 | 3589 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - | 3590 | `	/* Dump instructions */` |
|        7 | 3591 | `	for(;;){` |
|       15 | 3592 | `		if( pInstr >= pEnd ){` |
|        - | 3593 | `			/* No more instructions */` |
|        3 | 3594 | `			break;` |
|        - | 3595 | `		}` |
|        - | 3596 | `		/* Format and call the consumer callback */` |
|       19 | 3597 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|       12 | 3598 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 | 3599 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|       13 | 3600 | `		if( rc != SXRET_OK ){` |
|        - | 3601 | `			/* Consumer routine request an operation abort */` |
|      ! 0 | 3602 | `			return rc;` |
|        - | 3603 | `		}` |
|       13 | 3604 | `		++n;` |
|       13 | 3605 | `		pInstr++; /* Next instruction in the stream */` |
|        1 | 3606 | `	}` |
|        3 | 3607 | `	return rc;` |
|        2 | 3608 | `}` |
|        - | 3609 | `/*` |
|        - | 3610 | ` * Save the execution state of a fiber/generator context.` |
|        - | 3611 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - | 3612 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - | 3613 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - | 3614 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - | 3615 | ` * when VmByteCodeExec returns.` |
|        - | 3616 | ` */` |
|     1646 | 3617 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|        - | 3618 | `	ph7_vm *pVm,` |
|        - | 3619 | `	ph7_exec_ctx *pCtx,` |
|        - | 3620 | `	sxi32 pc,` |
|        - | 3621 | `	sxi32 nTos` |
|        - | 3622 | `	)` |
|        5 | 3623 | `{` |
|      823 | 3624 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|     1651 | 3625 | `	pCtx->pc = pc;` |
|     1651 | 3626 | `	pCtx->nTos = nTos;` |
|     1651 | 3627 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|     1651 | 3628 | `	return PH7_SUSPEND;` |
|        5 | 3629 | `}` |
|        - | 3630 | `/*` |
|        - | 3631 | ` * Resolve named-argument mapping.` |
|        - | 3632 | ` *` |
|        - | 3633 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - | 3634 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - | 3635 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - | 3636 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - | 3637 | ` * every formal parameter that received a value.` |
|        - | 3638 | ` *` |
|        - | 3639 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - | 3640 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|        - | 3641 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|        - | 3642 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|        - | 3643 | ` */` |
|      276 | 3644 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|        - | 3645 | `	ph7_vm *pVm,` |
|        - | 3646 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - | 3647 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - | 3648 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - | 3649 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - | 3650 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - | 3651 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - | 3652 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - | 3653 | `)` |
|        4 | 3654 | `{` |
|      280 | 3655 | `	sxi32 posIdx = 0;` |
|        - | 3656 | `	sxu32 i;` |
|      280 | 3657 | `	int bSeenNamed = 0;` |
|        - | 3658 | `	char zErrMsg[256];` |
|      280 | 3659 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|     1056 | 3660 | `	for( i = 0; i < nActual; i++ ){` |
|      780 | 3661 | `		aSlot[i] = -2;` |
|      392 | 3662 | `	}` |
|     1044 | 3663 | `	for( i = 0; i < nActual; i++ ){` |
|      999 | 3664 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - | 3665 | `			/* Named argument — find formal by name */` |
|      454 | 3666 | `			int found = 0;` |
|      454 | 3667 | `			bSeenNamed = 1;` |
|        - | 3668 | `			sxu32 k;` |
|      704 | 3669 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      602 | 3670 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      585 | 3671 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      560 | 3672 | `						pMap->aNames[i].zString,` |
|      840 | 3673 | `						pMap->aNames[i].nByte) == 0 ){` |
|      356 | 3674 | `					if( aUsed[k] ){` |
|       12 | 3675 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3676 | `							"Named parameter $%.*s overwrites previous argument",` |
|        6 | 3677 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        9 | 3678 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3679 | `					}` |
|      349 | 3680 | `					aSlot[i] = (sxi32)k;` |
|      349 | 3681 | `					aUsed[k] = 1;` |
|      349 | 3682 | `					found = 1;` |
|      349 | 3683 | `					break;` |
|        - | 3684 | `				}` |
|      128 | 3685 | `			}` |
|      448 | 3686 | `			if( !found ){` |
|      101 | 3687 | `				if( iVariadicIdx >= 0 ){` |
|       93 | 3688 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|       47 | 3689 | `				}else{` |
|       11 | 3690 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3691 | `						"Unknown named parameter $%.*s",` |
|        6 | 3692 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        8 | 3693 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3694 | `				}` |
|       46 | 3695 | `			}` |
|      222 | 3696 | `		}else{` |
|        - | 3697 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|        - | 3698 | `			 * named arg (the parser rejects it at compile time), but a call` |
|        - | 3699 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|        - | 3700 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|      329 | 3701 | `			if( bSeenNamed ){` |
|      ! 0 | 3702 | `				return VmThrowNamedArgError(&(*pVm),` |
|        - | 3703 | `					"Cannot use positional argument after named argument",` |
|        - | 3704 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|        - | 3705 | `			}` |
|      329 | 3706 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       51 | 3707 | `				if( aUsed[posIdx] ){` |
|      ! 0 | 3708 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - | 3709 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 | 3710 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 | 3711 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - | 3712 | `				}` |
|       51 | 3713 | `				aSlot[i] = posIdx;` |
|       51 | 3714 | `				aUsed[posIdx] = 1;` |
|      303 | 3715 | `			}else if( iVariadicIdx >= 0 ){` |
|      279 | 3716 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      139 | 3717 | `			}` |
|      329 | 3718 | `			posIdx++;` |
|        - | 3719 | `		}` |
|      386 | 3720 | `	}` |
|      267 | 3721 | `	return SXRET_OK;` |
|      142 | 3722 | `}` |
|        - | 3723 | `/*` |
|        - | 3724 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - | 3725 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - | 3726 | ` */` |
|      302 | 3727 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        4 | 3728 | `{` |
|      306 | 3729 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      294 | 3730 | `		return 0;` |
|        - | 3731 | `	}` |
|       15 | 3732 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|      155 | 3733 | `}` |
|        - | 3734 | `/*` |
|        - | 3735 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - | 3736 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - | 3737 | ` * preserved (later wins), integer keys are renumbered.` |
|        - | 3738 | ` */` |
|       10 | 3739 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3740 | `{` |
|       11 | 3741 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 | 3742 | `	(void)pVm;` |
|       11 | 3743 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 | 3744 | `	return SXRET_OK;` |
|        1 | 3745 | `}` |
|        - | 3746 | `/*` |
|        - | 3747 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - | 3748 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - | 3749 | ` */` |
|        6 | 3750 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 | 3751 | `{` |
|        3 | 3752 | `	(void)pVm; (void)pKey;` |
|        7 | 3753 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 | 3754 | `	return SXRET_OK;` |
|        1 | 3755 | `}` |
|        - | 3756 | `/*` |
|        - | 3757 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|        - | 3758 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|        - | 3759 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|        - | 3760 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|        - | 3761 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|        - | 3762 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|        - | 3763 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|        - | 3764 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|        - | 3765 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|        - | 3766 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|        - | 3767 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|        - | 3768 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|        - | 3769 | ` */` |
|        - | 3770 | `/*` |
|        - | 3771 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|        - | 3772 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|        - | 3773 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|        - | 3774 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|        - | 3775 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|        - | 3776 | ` */` |
|      280 | 3777 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|        4 | 3778 | `{` |
|        - | 3779 | `	VmSpreadRun sRun;` |
|        - | 3780 | `	ph7_hashmap_node *pNode;` |
|        - | 3781 | `	sxu32 i;` |
|      284 | 3782 | `	sRun.pStart = pFirst;` |
|      284 | 3783 | `	sRun.nCount = nCount;` |
|      284 | 3784 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      284 | 3785 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      284 | 3786 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|      ! 0 | 3787 | `		return;` |
|        - | 3788 | `	}` |
|      284 | 3789 | `	pNode = pMap->pFirst;` |
|     2040 | 3790 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|        - | 3791 | `		VmSpreadKey sKey;` |
|     1760 | 3792 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|        - | 3793 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|        - | 3794 | `			 * the source map's release before CALL replays them. */` |
|       95 | 3795 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       95 | 3796 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       95 | 3797 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|       48 | 3798 | `		}else{` |
|        - | 3799 | `			/* Integer key (or empty-string key, treated positionally) */` |
|     1666 | 3800 | `			sKey.nOff = 0;` |
|     1666 | 3801 | `			sKey.nLen = 0;` |
|        - | 3802 | `		}` |
|     1760 | 3803 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|     1760 | 3804 | `		pNode = pNode->pPrev; /* forward link */` |
|      882 | 3805 | `	}` |
|      144 | 3806 | `}` |
|        - | 3807 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|        - | 3808 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        8 | 3809 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|      ! 0 | 3810 | `{` |
|        8 | 3811 | `	SySetReset(&pVm->aSpreadRun);` |
|        8 | 3812 | `	SySetReset(&pVm->aSpreadKey);` |
|        8 | 3813 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        8 | 3814 | `}` |
|        - | 3815 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|        - | 3816 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|        - | 3817 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|        - | 3818 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|        - | 3819 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|        - | 3820 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|        - | 3821 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|        - | 3822 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|        - | 3823 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|        - | 3824 | ` * slot — from being consumed by that nested call. */` |
|      468 | 3825 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|        4 | 3826 | `{` |
|      472 | 3827 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      472 | 3828 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|        - | 3829 | `	VmSpreadRun *aRun;` |
|      472 | 3830 | `	if( rStart >= nRun ){` |
|      204 | 3831 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|        - | 3832 | `	}` |
|      272 | 3833 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      272 | 3834 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      272 | 3835 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      272 | 3836 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|      134 | 3837 | `	}` |
|      272 | 3838 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|      238 | 3839 | `}` |
|        - | 3840 | `/*` |
|        - | 3841 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|        - | 3842 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|        - | 3843 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|        - | 3844 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|        - | 3845 | ` *` |
|        - | 3846 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|        - | 3847 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|        - | 3848 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|        - | 3849 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|        - | 3850 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|        - | 3851 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|        - | 3852 | ` * they are counted only by that call. This replaces the old shared` |
|        - | 3853 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|        - | 3854 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|        - | 3855 | ` *` |
|        - | 3856 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|        - | 3857 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|        - | 3858 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|        - | 3859 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|        - | 3860 | ` */` |
|      296 | 3861 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|        4 | 3862 | `{` |
|      300 | 3863 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 3864 | `	VmSpreadRun *aRun;` |
|      300 | 3865 | `	ph7_value *pEnd = pTos;` |
|      300 | 3866 | `	sxi32 nPos = iP1;` |
|      300 | 3867 | `	sxi32 ri, extra = 0;` |
|      300 | 3868 | `	if( nRun == 0 ){` |
|       15 | 3869 | `		pVm->nSpreadCallBase = 0;` |
|       15 | 3870 | `		return 0;` |
|        - | 3871 | `	}` |
|      286 | 3872 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      286 | 3873 | `	ri = (sxi32)nRun - 1;` |
|      696 | 3874 | `	while( nPos > 0 ){` |
|      414 | 3875 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|        - | 3876 | `			/* A non-empty unpack occupying nCount slots. */` |
|      248 | 3877 | `			pEnd = aRun[ri].pStart;` |
|      248 | 3878 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      248 | 3879 | `			ri--;` |
|      292 | 3880 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|        - | 3881 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       39 | 3882 | `			extra -= 1;` |
|       39 | 3883 | `			ri--;` |
|       21 | 3884 | `		}else{` |
|        - | 3885 | `			/* An ordinary single-slot argument. */` |
|      134 | 3886 | `			pEnd--;` |
|        - | 3887 | `		}` |
|      414 | 3888 | `		nPos--;` |
|        4 | 3889 | `	}` |
|        - | 3890 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|        - | 3891 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      286 | 3892 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      286 | 3893 | `	return extra;` |
|      152 | 3894 | `}` |
|      280 | 3895 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap)` |
|        4 | 3896 | `{` |
|      284 | 3897 | `	ph7_value *pTos = *ppTos;` |
|      284 | 3898 | `	sxu32 nEntry = pMap->nEntry;` |
|      284 | 3899 | `	if( nEntry == 0 ){` |
|        - | 3900 | `		/* Nothing to unpack — remove the source from the stack */` |
|       39 | 3901 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       39 | 3902 | `		VmPopOperand(&pTos, 1);` |
|       21 | 3903 | `	}else{` |
|        - | 3904 | `		ph7_hashmap_node *pNode;` |
|        - | 3905 | `		ph7_value *pElem;` |
|        - | 3906 | `		sxu32 i;` |
|        - | 3907 | `		int bTemp;` |
|      248 | 3908 | `		pMap->iRef++;` |
|      248 | 3909 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|        - | 3910 | `		/* Record the run + element keys before any release (nodes still alive).` |
|        - | 3911 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|      248 | 3912 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|        - | 3913 | `		/* Overwrite the source slot with the first element */` |
|      248 | 3914 | `		pNode = pMap->pFirst;` |
|      248 | 3915 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      248 | 3916 | `		PH7_MemObjRelease(pTos);` |
|      248 | 3917 | `		if( pElem ){` |
|      248 | 3918 | `			if( bTemp ){` |
|      135 | 3919 | `				PH7_MemObjStore(pElem, pTos);` |
|       68 | 3920 | `			}else{` |
|      114 | 3921 | `				PH7_MemObjLoad(pElem, pTos);` |
|        - | 3922 | `			}` |
|      122 | 3923 | `		}` |
|      248 | 3924 | `		pTos->nIdx = SXU32_HIGH;` |
|        - | 3925 | `		/* Traverse in insertion order (pPrev is the forward link` |
|        - | 3926 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|      248 | 3927 | `		pNode = pNode->pPrev;` |
|        - | 3928 | `		/* Push the remaining elements */` |
|     1760 | 3929 | `		for( i = 1; i < nEntry; i++ ){` |
|     1516 | 3930 | `			pTos++;` |
|     1516 | 3931 | `			PH7_MemObjInit(pVm, pTos);` |
|     1516 | 3932 | `			pTos->nIdx = SXU32_HIGH;` |
|     1516 | 3933 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|     1516 | 3934 | `			if( pElem ){` |
|     1516 | 3935 | `				if( bTemp ){` |
|     1259 | 3936 | `					PH7_MemObjStore(pElem, pTos);` |
|      630 | 3937 | `				}else{` |
|      258 | 3938 | `					PH7_MemObjLoad(pElem, pTos);` |
|        - | 3939 | `				}` |
|      756 | 3940 | `			}` |
|     1516 | 3941 | `			pNode = pNode->pPrev;` |
|      760 | 3942 | `		}` |
|      248 | 3943 | `		PH7_HashmapUnref(pMap);` |
|        - | 3944 | `	}` |
|      284 | 3945 | `	*ppTos = pTos;` |
|      284 | 3946 | `}` |
|        - | 3947 | `/*` |
|        - | 3948 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|        - | 3949 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|        - | 3950 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|        - | 3951 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|        - | 3952 | ` * element keys, interleaving them with the compile-time names at their real` |
|        - | 3953 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|        - | 3954 | ` *` |
|        - | 3955 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|        - | 3956 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|        - | 3957 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|        - | 3958 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|        - | 3959 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|        - | 3960 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|        - | 3961 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|        - | 3962 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|        - | 3963 | ` * method-name slot pop shifts pArg).` |
|        - | 3964 | ` *` |
|        - | 3965 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|        - | 3966 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|        - | 3967 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|        - | 3968 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|        - | 3969 | ` * which is after this call's synchronous named-arg resolution.` |
|        - | 3970 | ` */` |
|      256 | 3971 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|        - | 3972 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|        4 | 3973 | `{` |
|      260 | 3974 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - | 3975 | `	VmSpreadRun *aRun;` |
|        - | 3976 | `	VmSpreadKey *aKey;` |
|        - | 3977 | `	const char *zKeyBase;` |
|      260 | 3978 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|      260 | 3979 | `	int bAnyNamed = 0;` |
|        - | 3980 | `	sxu32 ai, ci, ri, rStart;` |
|      260 | 3981 | `	if( nRun == 0 ){` |
|        - | 3982 | `		/* No spread captured at all — the compile map is already aligned. */` |
|      ! 0 | 3983 | `		return 0;` |
|        - | 3984 | `	}` |
|      260 | 3985 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      260 | 3986 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      260 | 3987 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|        - | 3988 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|        - | 3989 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|        - | 3990 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|        - | 3991 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      260 | 3992 | `	ri = pVm->nSpreadCallBase;` |
|      260 | 3993 | `	rStart = ri;` |
|      260 | 3994 | `	if( rStart >= nRun ){` |
|        - | 3995 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|      ! 0 | 3996 | `		return 0;` |
|        - | 3997 | `	}` |
|      260 | 3998 | `	SySetReset(&pVm->aEffArgName);` |
|      260 | 3999 | `	ci = 0;` |
|      260 | 4000 | `	ai = 0;` |
|      614 | 4001 | `	while( ai < nActual ){` |
|        - | 4002 | `		SyString sName;` |
|      358 | 4003 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|        - | 4004 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|        - | 4005 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      370 | 4006 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|       13 | 4007 | `			ci++; ri++;` |
|        1 | 4008 | `		}` |
|      358 | 4009 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|        - | 4010 | `			/* A run of spread elements: one name per element from its key. Keys` |
|        - | 4011 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|        - | 4012 | `			 * run never desyncs the key stream. */` |
|      248 | 4013 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|     2004 | 4014 | `			for( j = 0; j < K; j++ ){` |
|     1760 | 4015 | `				SyZero(&sName, sizeof(sName));` |
|     1760 | 4016 | `				if( aKey[ks + j].nLen > 0 ){` |
|       95 | 4017 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       95 | 4018 | `					bAnyNamed = 1;` |
|       47 | 4019 | `				}` |
|     1760 | 4020 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      882 | 4021 | `			}` |
|      248 | 4022 | `			ai += K;` |
|      248 | 4023 | `			ci++; ri++;` |
|      126 | 4024 | `		}else{` |
|        - | 4025 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      114 | 4026 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|       33 | 4027 | `				sName = pCompile->aNames[ci];` |
|       33 | 4028 | `				bAnyNamed = 1;` |
|       16 | 4029 | `			}` |
|      114 | 4030 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      114 | 4031 | `			ai++;` |
|      114 | 4032 | `			ci++;` |
|        - | 4033 | `		}` |
|        4 | 4034 | `	}` |
|        - | 4035 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|        - | 4036 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|        - | 4037 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|        - | 4038 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      260 | 4039 | `	VmSpreadConsume(pVm);` |
|      260 | 4040 | `	if( !bAnyNamed ){` |
|        - | 4041 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      190 | 4042 | `		return 0;` |
|        - | 4043 | `	}` |
|       71 | 4044 | `	pEff->bHasNamed = 1;` |
|       71 | 4045 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|       71 | 4046 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|       71 | 4047 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|       71 | 4048 | `	pEff->nTotal = nActual;` |
|       71 | 4049 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|       71 | 4050 | `	return 1;` |
|      132 | 4051 | `}` |
|        - | 4052 | `/*` |
|        - | 4053 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|        - | 4054 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|        - | 4055 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|        - | 4056 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|        - | 4057 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|        - | 4058 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|        - | 4059 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|        - | 4060 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|        - | 4061 | ` * pArg must be the site's FINAL argument base.` |
|        - | 4062 | ` */` |
|  1043821 | 4063 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|        - | 4064 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|        5 | 4065 | `{` |
|  1043826 | 4066 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|  1043826 | 4067 | `	if( pInstr->iP2 == 0 ){` |
|  1043544 | 4068 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|        - | 4069 | `	}` |
|      286 | 4070 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|       71 | 4071 | `		return pStorage;` |
|        - | 4072 | `	}` |
|      216 | 4073 | `	VmSpreadConsume(pVm);` |
|      216 | 4074 | `	return pCompile;` |
|   522381 | 4075 | `}` |
|        - | 4076 | `/*` |
|        - | 4077 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|        - | 4078 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|        - | 4079 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — the two paths` |
|        - | 4080 | ` * disagree on which scalar types are silent (positional silences null+bool; keyed silences` |
|        - | 4081 | ` * only null, warning for bool to match PHP) — this only maps the type name and emits.` |
|        - | 4082 | ` */` |
|      ! 0 | 4083 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|      ! 0 | 4084 | `{` |
|      ! 0 | 4085 | `	const char *zType = "unknown";` |
|        - | 4086 | `	char zMsg[64];` |
|      ! 0 | 4087 | `	if( iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 4088 | `		zType = "string";` |
|      ! 0 | 4089 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        - | 4090 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|        - | 4091 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|        - | 4092 | `		 * REAL flag, so it still falls through to the int arm. */` |
|      ! 0 | 4093 | `		zType = "float";` |
|      ! 0 | 4094 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 | 4095 | `		zType = "int";` |
|      ! 0 | 4096 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 4097 | `		zType = "bool";` |
|      ! 0 | 4098 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4099 | `		zType = "object";` |
|      ! 0 | 4100 | `	}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 | 4101 | `		zType = "resource";` |
|      ! 0 | 4102 | `	}` |
|      ! 0 | 4103 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|      ! 0 | 4104 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 | 4105 | `}` |
|        - | 4106 | `/*` |
|        - | 4107 | ` * A member access in isset()/empty() context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY) is a silent` |
|        - | 4108 | ` * lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class instance"` |
|        - | 4109 | ` * warnings, mirroring the array isset/empty path.` |
|        - | 4110 | ` */` |
|     1102 | 4111 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|        5 | 4112 | `{` |
|     1107 | 4113 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY;` |
|        5 | 4114 | `}` |
|        - | 4115 | `/*` |
|        - | 4116 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|        - | 4117 | ` * A __get body reading the SAME property of the SAME instance must not` |
|        - | 4118 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|        - | 4119 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|        - | 4120 | ` * reads (other names / other instances) still dispatch.` |
|        - | 4121 | ` */` |
|     1060 | 4122 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4123 | `{` |
|        - | 4124 | `	VmMagicGuard *aG;` |
|        - | 4125 | `	sxu32 nHash;` |
|        - | 4126 | `	sxu32 n;` |
|     1061 | 4127 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|        - | 4128 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|        - | 4129 | `		 * every hooked-property access consults the guard, often twice. */` |
|      881 | 4130 | `		return FALSE;` |
|        - | 4131 | `	}` |
|      181 | 4132 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|      181 | 4133 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      225 | 4134 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|      181 | 4135 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|      137 | 4136 | `			return TRUE;` |
|        - | 4137 | `		}` |
|       23 | 4138 | `	}` |
|       45 | 4139 | `	return FALSE;` |
|      531 | 4140 | `}` |
|      486 | 4141 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        1 | 4142 | `{` |
|        - | 4143 | `	VmMagicGuard sG;` |
|      487 | 4144 | `	sG.pThis = pThis;` |
|      487 | 4145 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      487 | 4146 | `	sG.cKind = cKind;` |
|      487 | 4147 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      487 | 4148 | `}` |
|      486 | 4149 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|        1 | 4150 | `{` |
|      487 | 4151 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      487 | 4152 | `}` |
|        - | 4153 | `/*` |
|        - | 4154 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|        - | 4155 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|        - | 4156 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|        - | 4157 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|        - | 4158 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|        - | 4159 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|        - | 4160 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|        - | 4161 | ` * One-token lookahead only.` |
|        - | 4162 | ` */` |
|      708 | 4163 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|        3 | 4164 | `{` |
|      711 | 4165 | `	switch( pNext->iOp ){` |
|       17 | 4166 | `		case PH7_OP_STORE:` |
|       37 | 4167 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|      ! 0 | 4168 | `		case PH7_OP_STORE_REF:` |
|      ! 0 | 4169 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|       11 | 4170 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|        - | 4171 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|        - | 4172 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|        - | 4173 | `		case PH7_OP_CAT_STORE:` |
|        - | 4174 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|        - | 4175 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       23 | 4176 | `			return 1;` |
|      326 | 4177 | `		default:` |
|      654 | 4178 | `			return 0;` |
|        - | 4179 | `	}` |
|      357 | 4180 | `}` |
|        - | 4181 | `/*` |
|        - | 4182 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|        - | 4183 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|        - | 4184 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|        - | 4185 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|        - | 4186 | ` */` |
|      424 | 4187 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|        1 | 4188 | `{` |
|      425 | 4189 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|        1 | 4190 | `}` |
|        - | 4191 | `/*` |
|        - | 4192 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|        - | 4193 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|        - | 4194 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|        - | 4195 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|        - | 4196 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|        - | 4197 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|        - | 4198 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|        - | 4199 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|        - | 4200 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|        - | 4201 | ` * abort path; SXRET_OK otherwise.` |
|        - | 4202 | ` */` |
|       54 | 4203 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|        1 | 4204 | `{` |
|        - | 4205 | `	char zHName[384];` |
|        - | 4206 | `	sxu32 nHName;` |
|        - | 4207 | `	ph7_class_method *pSetHook;` |
|       55 | 4208 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|        - | 4209 | `		/* get-only hooked property: php's read-only Error */` |
|        - | 4210 | `		SyBlob sErrMsg;` |
|        5 | 4211 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 4212 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|        4 | 4213 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|        5 | 4214 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        5 | 4215 | `		return SXRET_OK;` |
|        - | 4216 | `	}` |
|       51 | 4217 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 4218 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|      ! 0 | 4219 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|      ! 0 | 4220 | `		if( rcVis != SXRET_OK ){` |
|      ! 0 | 4221 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|      ! 0 | 4222 | `			return SXRET_OK;` |
|        - | 4223 | `		}` |
|      ! 0 | 4224 | `	}` |
|       51 | 4225 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|       51 | 4226 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|       51 | 4227 | `	if( pSetHook ){` |
|        - | 4228 | `		ph7_value sHookRet;` |
|        - | 4229 | `		ph7_value *apHArg[1];` |
|       51 | 4230 | `		apHArg[0] = pValue;` |
|       51 | 4231 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|       51 | 4232 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|       51 | 4233 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|       51 | 4234 | `		VmMagicGuardPop(pVm);` |
|       50 | 4235 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|       27 | 4236 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|        3 | 4237 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|        3 | 4238 | `			if( rcH == SXRET_OK ){` |
|        3 | 4239 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|        3 | 4240 | `				if( pBack ){` |
|        3 | 4241 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|        2 | 4242 | `				}` |
|        1 | 4243 | `			}else if( rcH == PH7_ABORT ){` |
|      ! 0 | 4244 | `				PH7_MemObjRelease(&sHookRet);` |
|      ! 0 | 4245 | `				return PH7_ABORT;` |
|        - | 4246 | `			}` |
|        - | 4247 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|        - | 4248 | `			 * the store is skipped, execution lands at the fetch point like any` |
|        - | 4249 | `			 * parked throw. */` |
|        1 | 4250 | `		}` |
|       51 | 4251 | `		PH7_MemObjRelease(&sHookRet);` |
|       25 | 4252 | `	}` |
|       51 | 4253 | `	return SXRET_OK;` |
|       28 | 4254 | `}` |
|        - | 4255 | `/*` |
|        - | 4256 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|        - | 4257 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|        - | 4258 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|        - | 4259 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|        - | 4260 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|        - | 4261 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|        - | 4262 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|        - | 4263 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|        - | 4264 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|        - | 4265 | ` */` |
|        - | 4266 | `/*` |
|        - | 4267 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|        - | 4268 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|        - | 4269 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|        - | 4270 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|        - | 4271 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|        - | 4272 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|        - | 4273 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|        - | 4274 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|        - | 4275 | ` * caller reads the raw slot then.` |
|        - | 4276 | ` */` |
|      382 | 4277 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|        4 | 4278 | `{` |
|      386 | 4279 | `	ph7_vm *pVm = pThis->pVm;` |
|        - | 4280 | `	char zHName[384];` |
|        - | 4281 | `	sxu32 nHName;` |
|        - | 4282 | `	ph7_class_method *pGetHook;` |
|        - | 4283 | `	sxi32 rc;` |
|      382 | 4284 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|      260 | 4285 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|      142 | 4286 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|        - | 4287 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|        - | 4288 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|        - | 4289 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|        - | 4290 | `		 * raw values whose output the routed throw then discards. */` |
|      248 | 4291 | `		return SXERR_NOTFOUND;` |
|        - | 4292 | `	}` |
|      139 | 4293 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|      139 | 4294 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|      139 | 4295 | `	if( pGetHook == 0 ){` |
|      ! 0 | 4296 | `		return SXERR_NOTFOUND;` |
|        - | 4297 | `	}` |
|      139 | 4298 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|      139 | 4299 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|      139 | 4300 | `	VmMagicGuardPop(pVm);` |
|      139 | 4301 | `	return rc;` |
|      195 | 4302 | `}` |
|        - | 4303 | `/*` |
|        - | 4304 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|        - | 4305 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|        - | 4306 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|        - | 4307 | ` */` |
|       20 | 4308 | `static void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4309 | `{` |
|       21 | 4310 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - | 4311 | `	VmSlot sFree;` |
|       21 | 4312 | `	if( pScr ){` |
|       21 | 4313 | `		PH7_MemObjRelease(pScr);` |
|       10 | 4314 | `	}` |
|       21 | 4315 | `	sFree.nIdx = nIdx;` |
|       21 | 4316 | `	sFree.pUserData = 0;` |
|       21 | 4317 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       21 | 4318 | `}` |
|        - | 4319 | `/*` |
|        - | 4320 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|        - | 4321 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|        - | 4322 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|        - | 4323 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|        - | 4324 | ` * instance reference.` |
|        - | 4325 | ` */` |
|       16 | 4326 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|        2 | 4327 | `{` |
|       18 | 4328 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       18 | 4329 | `	if( pEnt == 0 ){` |
|        5 | 4330 | `		return;` |
|        - | 4331 | `	}` |
|       13 | 4332 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        5 | 4333 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|        2 | 4334 | `	}` |
|       13 | 4335 | `	SyBlobRelease(&pEnt->sName);` |
|       13 | 4336 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|       13 | 4337 | `	(void)SySetPop(&pVm->aHookRmw);` |
|       10 | 4338 | `}` |
|       16 | 4339 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 4340 | `{` |
|        - | 4341 | `	VmHookRmw sEnt;` |
|        - | 4342 | `	VmHookRmw *pEnt;` |
|        - | 4343 | `	ph7_value *pScr;` |
|        - | 4344 | `	ph7_value sVal;` |
|       17 | 4345 | `	sxi32 rc = SXRET_OK;` |
|       17 | 4346 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       17 | 4347 | `	if( pEnt == 0 \|\| pEnt->iKind != VM_HOOK_PEND_RMW \|\| pEnt->nScratchIdx != nIdx ){` |
|      ! 0 | 4348 | `		return SXERR_NOTFOUND;` |
|        - | 4349 | `	}` |
|       17 | 4350 | `	sEnt = *pEnt;` |
|       17 | 4351 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        - | 4352 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|        - | 4353 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|        - | 4354 | `	 * scratch index past this point). */` |
|       17 | 4355 | `	PH7_MemObjInit(pVm,&sVal);` |
|       17 | 4356 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|       17 | 4357 | `	if( pScr ){` |
|       17 | 4358 | `		PH7_MemObjStore(pScr,&sVal);` |
|        8 | 4359 | `	}` |
|       17 | 4360 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|       17 | 4361 | `	sVal.nIdx = SXU32_HIGH;` |
|       17 | 4362 | `	if( pVm->nBoundaryRc == 0 ){` |
|       15 | 4363 | `		rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|        7 | 4364 | `	}` |
|       17 | 4365 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 4366 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|       17 | 4367 | `	return rc;` |
|        9 | 4368 | `}` |
|        - | 4369 | `/*` |
|        - | 4370 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|        - | 4371 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|        - | 4372 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|        - | 4373 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|        - | 4374 | ` */` |
|       12 | 4375 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|        1 | 4376 | `{` |
|       13 | 4377 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       13 | 4378 | `	if( pSetMeth ){` |
|        - | 4379 | `		ph7_value sNameVal;` |
|        - | 4380 | `		ph7_value *apSetArg[2];` |
|       13 | 4381 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       13 | 4382 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       13 | 4383 | `		apSetArg[0] = &sNameVal;` |
|       13 | 4384 | `		apSetArg[1] = pValue;` |
|       13 | 4385 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       13 | 4386 | `		PH7_VmCallClassMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       13 | 4387 | `		VmMagicGuardPop(pVm);` |
|       13 | 4388 | `		PH7_MemObjRelease(&sNameVal);` |
|        6 | 4389 | `	}` |
|       13 | 4390 | `}` |
|        - | 4391 | `/*` |
|        - | 4392 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|        - | 4393 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|        - | 4394 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|        - | 4395 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|        - | 4396 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|        - | 4397 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|        - | 4398 | ` * path (the SyHash-layout incident class).` |
|        - | 4399 | ` */` |
|        - | 4400 | `/*` |
|        - | 4401 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|        - | 4402 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|        - | 4403 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|        - | 4404 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|        - | 4405 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|        - | 4406 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|        - | 4407 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|        - | 4408 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|        - | 4409 | ` * never linked (INIT error path).` |
|        - | 4410 | ` */` |
|    23792 | 4411 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|        5 | 4412 | `{` |
|    23797 | 4413 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|    23797 | 4414 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|        - | 4415 | `	sxu32 i;` |
|    25973 | 4416 | `	for( i = 0 ; i < n ; ++i ){` |
|    25973 | 4417 | `		if( apStep[i] == pStep ){` |
|    23805 | 4418 | `			for( ; i + 1 < n ; ++i ){` |
|        9 | 4419 | `				apStep[i] = apStep[i + 1];` |
|        5 | 4420 | `			}` |
|    23797 | 4421 | `			(void)SySetPop(&pInfo->aStep);` |
|    23797 | 4422 | `			return;` |
|        - | 4423 | `		}` |
|     1093 | 4424 | `	}` |
|    11901 | 4425 | `}` |
|      210 | 4426 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|        5 | 4427 | `{` |
|      215 | 4428 | `	if( pStep->pOwner ){` |
|       24 | 4429 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       11 | 4430 | `	}` |
|      215 | 4431 | `	VmForeachStepUnlink(pInfo,pStep);` |
|      215 | 4432 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      215 | 4433 | `	PH7_ClassInstanceUnref(pThis);` |
|      215 | 4434 | `}` |
|        - | 4435 | `/*` |
|        - | 4436 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|        - | 4437 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|        - | 4438 | ` * step stack, then drop the step's map reference. The single home for this` |
|        - | 4439 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|        - | 4440 | ` * load-bearing: a step freed while still registered is walked by the next` |
|        - | 4441 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|        - | 4442 | ` * class), and the unregister must precede the unref in case the step held the` |
|        - | 4443 | ` * map's last reference.` |
|        - | 4444 | ` */` |
|    23556 | 4445 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|        5 | 4446 | `{` |
|    23561 | 4447 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|    23561 | 4448 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|    23561 | 4449 | `	if( bPop ){` |
|        - | 4450 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|        - | 4451 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|    23561 | 4452 | `		VmForeachStepUnlink(pInfo,pStep);` |
|    11778 | 4453 | `	}` |
|    23561 | 4454 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    23561 | 4455 | `	PH7_HashmapUnref(pMap);` |
|    23561 | 4456 | `}` |
|        - | 4457 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|        - | 4458 | `/*` |
|        - | 4459 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 4460 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4461 | ` * See block-comment on that function for additional information.` |
|        - | 4462 | ` */` |
|   137652 | 4463 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 4464 | `{` |
|        - | 4465 | `	ph7_value *pStack;` |
|        - | 4466 | `	sxu32 nCap;` |
|        - | 4467 | `	sxi32 rc;` |
|        - | 4468 | `	/* Allocate a new operand stack */` |
|   137657 | 4469 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   137657 | 4470 | `	if( pStack == 0 ){` |
|      ! 0 | 4471 | `		return SXERR_MEM;` |
|        - | 4472 | `	}` |
|   137657 | 4473 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|        - | 4474 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|        - | 4475 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   137657 | 4476 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|        - | 4477 | `	/* Free the operand stack */` |
|   137657 | 4478 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 4479 | `	/* Execution result */` |
|   137657 | 4480 | `	return rc;` |
|    68831 | 4481 | `}` |
|        - | 4482 | `/*` |
|        - | 4483 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|        - | 4484 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|        - | 4485 | ` * the argument resolve against that class (like php) rather than the reflection` |
|        - | 4486 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|        - | 4487 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|        - | 4488 | ` */` |
|       54 | 4489 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|        1 | 4490 | `{` |
|       55 | 4491 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       55 | 4492 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - | 4493 | `	sxi32 rc;` |
|       55 | 4494 | `	if( pDeclCls ){` |
|       39 | 4495 | `		pVm->pConstEvalClass = pDeclCls;` |
|       39 | 4496 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       19 | 4497 | `	}` |
|       55 | 4498 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       55 | 4499 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       55 | 4500 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       55 | 4501 | `	return rc;` |
|        1 | 4502 | `}` |
|        - | 4503 | `/*` |
|        - | 4504 | ` * Invoke any installed shutdown callbacks.` |
|        - | 4505 | ` * Flush every still-open output buffer to the real output consumer at the end` |
|        - | 4506 | ` * of execution. php implicitly ends+flushes all ob_start() levels on shutdown` |
|        - | 4507 | ` * (normal end, exit()/die(), or fatal); PHL used to DISCARD them, so a script` |
|        - | 4508 | ` * that never called ob_end_flush() — e.g. PHPUnit, which buffers its result` |
|        - | 4509 | ` * summary and then exit()s with a non-zero status — lost that output entirely.` |
|        - | 4510 | ` *` |
|        - | 4511 | ` * Buffer content is already callback-transformed (VmObConsumer applies handlers` |
|        - | 4512 | ` * at write time), and new output always lands in the topmost buffer, so the` |
|        - | 4513 | ` * stack holds finished text with aOB[0] the earliest/outermost. Concatenate in` |
|        - | 4514 | ` * that order to the default consumer (sVmConsumer.xDef), then tear the stack` |
|        - | 4515 | ` * down and restore the default consumer.` |
|        - | 4516 | ` */` |
|     3412 | 4517 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|        5 | 4518 | `{` |
|     3417 | 4519 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - | 4520 | `	sxu32 n,nUsed;` |
|     3417 | 4521 | `	nUsed = SySetUsed(&pVm->aOB);` |
|     3417 | 4522 | `	if( nUsed < 1 ){` |
|     3415 | 4523 | `		return;` |
|        - | 4524 | `	}` |
|        7 | 4525 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4526 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4527 | `		if( pOb && SyBlobLength(&pOb->sOB) > 0 && pCons->xDef ){` |
|        5 | 4528 | `			pCons->xDef(SyBlobData(&pOb->sOB),SyBlobLength(&pOb->sOB),pCons->pDefData);` |
|        5 | 4529 | `			pVm->nOutputLen += SyBlobLength(&pOb->sOB);` |
|        2 | 4530 | `		}` |
|        3 | 4531 | `	}` |
|        - | 4532 | `	/* Restore the default consumer and release the buffers. */` |
|        3 | 4533 | `	pCons->xConsumer = pCons->xDef;` |
|        3 | 4534 | `	pCons->pUserData = pCons->pDefData;` |
|        7 | 4535 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|        5 | 4536 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|        5 | 4537 | `		if( pOb ){` |
|        5 | 4538 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|        5 | 4539 | `			SyBlobRelease(&pOb->sOB);` |
|        2 | 4540 | `		}` |
|        3 | 4541 | `	}` |
|        3 | 4542 | `	SySetReset(&pVm->aOB);` |
|        3 | 4543 | `	pVm->nObDepth = 0;` |
|     1711 | 4544 | `}` |
|        - | 4545 | `/*` |
|        - | 4546 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 4547 | ` * or more calls to [register_shutdown_function()].` |
|        - | 4548 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 4549 | ` * execution ends.` |
|        - | 4550 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 4551 | ` * additional information.` |
|        - | 4552 | ` */` |
|     3412 | 4553 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 4554 | `{` |
|        - | 4555 | `	VmShutdownCB *pEntry;` |
|        - | 4556 | `	ph7_value *apArg[10];` |
|        - | 4557 | `	sxu32 n,nEntry;` |
|        - | 4558 | `	int i;` |
|        - | 4559 | `	/* Point to the stack of registered callbacks */` |
|     3417 | 4560 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    37537 | 4561 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    34125 | 4562 | `		apArg[i] = 0;` |
|    17065 | 4563 | `	}` |
|        - | 4564 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 4565 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 4566 | `	 * callbacks, mirroring PHP.` |
|        - | 4567 | `	 */` |
|     3417 | 4568 | `	pVm->bHaltRequested = 0;` |
|     3435 | 4569 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       23 | 4570 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4571 | `		if( pEntry ){` |
|        - | 4572 | `			/* Prepare callback arguments if any */` |
|       23 | 4573 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 4574 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 4575 | `					break;` |
|        - | 4576 | `				}` |
|      ! 0 | 4577 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 4578 | `			}` |
|        - | 4579 | `			/* Invoke the callback */` |
|       23 | 4580 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 4581 | `			/*` |
|        - | 4582 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 4583 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 4584 | `			 */` |
|       23 | 4585 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       23 | 4586 | `			if( pEntry ){` |
|       23 | 4587 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       23 | 4588 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 4589 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 4590 | `				}` |
|        9 | 4591 | `			}` |
|       23 | 4592 | `			if( pVm->bHaltRequested ){` |
|        - | 4593 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 4594 | `				break;` |
|        - | 4595 | `			}` |
|        9 | 4596 | `		}` |
|       14 | 4597 | `	}` |
|     3417 | 4598 | `	SySetReset(&pVm->aShutdown);` |
|     3417 | 4599 | `}` |
|        - | 4600 | `/*` |
|        - | 4601 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 4602 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 4603 | ` * See block-comment on that function for additional information.` |
|        - | 4604 | ` */` |
|     3412 | 4605 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 4606 | `{` |
|        - | 4607 | `	/* Make sure we are ready to execute this program */` |
|     3417 | 4608 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 4609 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 4610 | `	}` |
|        - | 4611 | `	/* Set the execution magic number  */` |
|     3417 | 4612 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 4613 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|        - | 4614 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|        - | 4615 | `	{` |
|     3417 | 4616 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|     3417 | 4617 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|        - | 4618 | `	}` |
|        - | 4619 | `	/* Invoke any shutdown callbacks */` |
|     3417 | 4620 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 4621 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|        - | 4622 | `	 * shutdown callbacks, which may still write into them. */` |
|     3417 | 4623 | `	VmFlushOutputBuffers(&(*pVm));` |
|        - | 4624 | `	/*` |
|        - | 4625 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 4626 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 4627 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 4628 | `	 */` |
|     3417 | 4629 | `	return SXRET_OK;` |
|     1711 | 4630 | `}` |
|        - | 4631 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 4632 | `/*` |
|        - | 4633 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 4634 | ` * the desired message.` |
|        - | 4635 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 4636 | ` * in 'api.c' for additional information.` |
|        - | 4637 | ` */` |
|     2218 | 4638 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 4639 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 4640 | `	SyString *pString /* Message to output */` |
|        - | 4641 | `	)` |
|        5 | 4642 | `{` |
|     2223 | 4643 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     2223 | 4644 | `	sxi32 rc = SXRET_OK;` |
|        - | 4645 | `	/* Call the output consumer */` |
|     2223 | 4646 | `	if( pString->nByte > 0 ){` |
|     2223 | 4647 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     2223 | 4648 | `		VmTrackOutput(pVm, pString->nByte);` |
|     1109 | 4649 | `	}` |
|     2223 | 4650 | `	return rc;` |
|        5 | 4651 | `}` |
|        - | 4652 | `/*` |
|        - | 4653 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 4654 | ` * callback to consume the formatted message.` |
|        - | 4655 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 4656 | ` * in 'api.c' for additional information.` |
|        - | 4657 | ` */` |
|        2 | 4658 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 4659 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 4660 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 4661 | `	va_list ap           /* Variable list of arguments */` |
|        - | 4662 | `	)` |
|        1 | 4663 | `{` |
|        3 | 4664 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 4665 | `	sxi32 rc = SXRET_OK;` |
|        - | 4666 | `	SyBlob sWorker;` |
|        - | 4667 | `	/* Format the message and call the output consumer */` |
|        3 | 4668 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 4669 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 4670 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 4671 | `		/* Consume the formatted message */` |
|        3 | 4672 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 4673 | `	}` |
|        3 | 4674 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 4675 | `	/* Release the working buffer */` |
|        3 | 4676 | `	SyBlobRelease(&sWorker);` |
|        3 | 4677 | `	return rc;` |
|        1 | 4678 | `}` |
|        - | 4679 | `/*` |
|        - | 4680 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 4681 | ` * This function never fail and always return a pointer` |
|        - | 4682 | ` * to a null terminated string.` |
|        - | 4683 | ` */` |
|       12 | 4684 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 4685 | `{` |
|       13 | 4686 | `	const char *zOp = "Unknown     ";` |
|       13 | 4687 | `	switch(nOp){` |
|        3 | 4688 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 4689 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 4690 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 4691 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 4692 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 4693 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 4694 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 4695 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 4696 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 4697 | `	case PH7_OP_LOAD_FCC:` |
|      ! 0 | 4698 | `		                    zOp = "LOAD_FCC   "; break;` |
|      ! 0 | 4699 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 4700 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 4701 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 4702 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 4703 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 4704 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 4705 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 4706 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 4707 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 4708 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 4709 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 4710 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 4711 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 4712 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 4713 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 4714 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 4715 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 4716 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 4717 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 4718 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 4719 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 4720 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 4721 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 4722 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 4723 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 4724 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 4725 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 4726 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 4727 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 4728 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 4729 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 4730 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 4731 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 4732 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 4733 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 4734 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 4735 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 4736 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 4737 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 4738 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 4739 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 4740 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 4741 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 4742 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 4743 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 4744 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 4745 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|      ! 0 | 4746 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 4747 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 4748 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 4749 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 4750 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 4751 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 4752 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 4753 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 4754 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 4755 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 4756 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 4757 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 4758 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 4759 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 4760 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 4761 | `	case PH7_OP_CLONE_APPLY: zOp = "CLONE_APPLY"; break;` |
|      ! 0 | 4762 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 4763 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 4764 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 4765 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 4766 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 4767 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 4768 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 4769 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 4770 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 4771 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 4772 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 4773 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 4774 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 4775 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 4776 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 4777 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 4778 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 4779 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|      ! 0 | 4780 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 4781 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 4782 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 4783 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 4784 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 4785 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 4786 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 4787 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 4788 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 4789 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 4790 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 4791 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 4792 | `	default:` |
|      ! 0 | 4793 | `		break;` |
|        - | 4794 | `	}` |
|       13 | 4795 | `	return zOp;` |
|        1 | 4796 | `}` |
|        - | 4797 | `/*` |
|        - | 4798 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 4799 | ` * The xConsumer() callback which is an used defined function` |
|        - | 4800 | ` * is responsible of consuming the generated dump.` |
|        - | 4801 | ` */` |
|        2 | 4802 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 4803 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 4804 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 4805 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 4806 | `	)` |
|        1 | 4807 | `{` |
|        - | 4808 | `	sxi32 rc;` |
|        3 | 4809 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 4810 | `	return rc;` |
|        1 | 4811 | `}` |
|        - | 4812 | `/*` |
|        - | 4813 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 4814 | ` * outside a class body [i.e: global or function scope].` |
|        - | 4815 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 4816 | ` * in 'compile.c' for additional information.` |
|        - | 4817 | ` */` |
|       42 | 4818 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        2 | 4819 | `{` |
|       44 | 4820 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 4821 | `	/* Evaluate and expand constant value */` |
|       44 | 4822 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       44 | 4823 | `}` |
|        - | 4824 | `/*` |
|        - | 4825 | ` * Section:` |
|        - | 4826 | ` *  Function handling functions.` |
|        - | 4827 | ` * Status:` |
|        - | 4828 | ` *    Stable.` |
|        - | 4829 | ` */` |
|        - | 4830 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 4831 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 4832 | `	{ "__phl_magic_call", vm_builtin_magic_call },` |
|        - | 4833 | `	{ "__phl_enum_cases",   vm_builtin_enum_cases },` |
|        - | 4834 | `	{ "__phl_enum_from",    vm_builtin_enum_from },` |
|        - | 4835 | `	{ "__phl_enum_tryfrom", vm_builtin_enum_tryfrom },` |
|        - | 4836 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|        - | 4837 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 4838 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 4839 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 4840 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 4841 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 4842 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 4843 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 4844 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 4845 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 4846 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 4847 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 4848 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 4849 | `	    /* Constants management */` |
|        - | 4850 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 4851 | `	{ "define",   vm_builtin_define               },` |
|        - | 4852 | `	{ "constant", vm_builtin_constant             },` |
|        - | 4853 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 4854 | `	   /* Class/Object functions */` |
|        - | 4855 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 4856 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 4857 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 4858 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 4859 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 4860 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|        - | 4861 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 4862 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 4863 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 4864 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 4865 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 4866 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 4867 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 4868 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 4869 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 4870 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 4871 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 4872 | `	   /* SPL object identity */` |
|        - | 4873 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|        - | 4874 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|        - | 4875 | `	   /* SPL Autoloading */` |
|        - | 4876 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 4877 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 4878 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 4879 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 4880 | `	   /* Random numbers/strings generators */` |
|        - | 4881 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 4882 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 4883 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 4884 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 4885 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 4886 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 4887 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 4888 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 4889 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 4890 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 4891 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 4892 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 4893 | `	   /* Language constructs functions */` |
|        - | 4894 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 4895 | `	{ "print", vm_builtin_print                   },` |
|        - | 4896 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 4897 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 4898 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 4899 | `	  /* Variable handling functions */` |
|        - | 4900 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 4901 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 4902 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 4903 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 4904 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 4905 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 4906 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 4907 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 4908 | `	  /* Ouput control functions */` |
|        - | 4909 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 4910 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 4911 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 4912 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 4913 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 4914 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 4915 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 4916 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 4917 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 4918 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 4919 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 4920 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 4921 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 4922 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 4923 | `	  /* Assertion functions */` |
|        - | 4924 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 4925 | `	  /* Error reporting functions */` |
|        - | 4926 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 4927 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 4928 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 4929 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 4930 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 4931 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 4932 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 4933 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 4934 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|        - | 4935 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|        - | 4936 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 4937 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|        - | 4938 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|        - | 4939 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 4940 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 4941 | `	  /* Release info */` |
|        - | 4942 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 4943 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 4944 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 4945 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 4946 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 4947 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 4948 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 4949 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 4950 | `	  /* hashmap */` |
|        - | 4951 | `	{"compact",          vm_builtin_compact       },` |
|        - | 4952 | `	{"extract",          vm_builtin_extract       },` |
|        - | 4953 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 4954 | `	  /* URL related function */` |
|        - | 4955 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 4956 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 4957 | `	   /* UTF-8 encoding/decoding */` |
|        - | 4958 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 4959 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 4960 | `	   /* Command line processing */` |
|        - | 4961 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 4962 | `	   /* JSON encoding/decoding */` |
|        - | 4963 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 4964 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 4965 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 4966 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 4967 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 4968 | `	{"serialize",      vm_builtin_serialize },` |
|        - | 4969 | `	{"unserialize",    vm_builtin_unserialize },` |
|        - | 4970 | `	   /* Files/URI inclusion facility */` |
|        - | 4971 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 4972 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|        - | 4973 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 4974 | `	{ "include",      vm_builtin_include          },` |
|        - | 4975 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 4976 | `	{ "require",      vm_builtin_require          },` |
|        - | 4977 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 4978 | `};` |
|        - | 4979 | `/*` |
|        - | 4980 | ` * Register the built-in VM functions defined above.` |
|        - | 4981 | ` */` |
|     3408 | 4982 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 4983 | `{` |
|        - | 4984 | `	sxi32 rc;` |
|        - | 4985 | `	sxu32 n;` |
|   426005 | 4986 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 4987 | `		/* Note that these special functions have access` |
|        - | 4988 | `		 * to the underlying virtual machine as their` |
|        - | 4989 | `		 * private data.` |
|        - | 4990 | `		 */` |
|   422597 | 4991 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   422597 | 4992 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 4993 | `			return rc;` |
|        - | 4994 | `		}` |
|   211301 | 4995 | `	}` |
|     3413 | 4996 | `	return SXRET_OK;` |
|     1709 | 4997 | `}` |
|        - | 4998 | `/*` |
|        - | 4999 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 5000 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 5001 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 5002 | ` */` |
|   768038 | 5003 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 5004 | `{` |
|   768043 | 5005 | `	if( !iLoadable ){` |
|   761365 | 5006 | `		return pClass;` |
|        - | 5007 | `	}` |
|     6687 | 5008 | `	while(pClass){` |
|     6683 | 5009 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     6679 | 5010 | `			return pClass;` |
|        - | 5011 | `		}` |
|        5 | 5012 | `		pClass = pClass->pNextName;` |
|        1 | 5013 | `	}` |
|        5 | 5014 | `	return 0;` |
|   384024 | 5015 | `}` |
|        - | 5016 | `/*` |
|        - | 5017 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 5018 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 5019 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 5020 | ` * registered in the VM's class table.` |
|        - | 5021 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 5022 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 5023 | ` */` |
|      300 | 5024 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 5025 | `{` |
|        - | 5026 | `	VmAutoloadCB *pEntry;` |
|        - | 5027 | `	ph7_value sArg,sResult;` |
|        - | 5028 | `	SyHashEntry *pHashEntry;` |
|        - | 5029 | `	ph7_class *pClass;` |
|        - | 5030 | `	sxu32 n,nEntry;` |
|      305 | 5031 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|      305 | 5032 | `	if( nEntry < 1 ){` |
|      233 | 5033 | `		return 0;` |
|        - | 5034 | `	}` |
|        - | 5035 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       77 | 5036 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 5037 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 5038 | `	}` |
|        - | 5039 | `	/* Mark this class as being autoloaded */` |
|       75 | 5040 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 5041 | `	/* Prepare the class name argument */` |
|       75 | 5042 | `	PH7_MemObjInit(pVm,&sArg);` |
|       75 | 5043 | `	PH7_MemObjInit(pVm,&sResult);` |
|       75 | 5044 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       75 | 5045 | `	pClass = 0;` |
|      139 | 5046 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 5047 | `		ph7_value *apArg[1];` |
|       85 | 5048 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       85 | 5049 | `		if( pEntry == 0 ){` |
|      ! 0 | 5050 | `			continue;` |
|        - | 5051 | `		}` |
|       85 | 5052 | `		apArg[0] = &sArg;` |
|       85 | 5053 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 5054 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 5055 | `			continue;` |
|        - | 5056 | `		}` |
|        - | 5057 | `		/* Check if the class is now available */` |
|       85 | 5058 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       85 | 5059 | `		if( pHashEntry ){` |
|       20 | 5060 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       20 | 5061 | `			if( pClass ){` |
|       20 | 5062 | `				break;` |
|        - | 5063 | `			}` |
|      ! 0 | 5064 | `		}` |
|       35 | 5065 | `	}` |
|       75 | 5066 | `	PH7_MemObjRelease(&sArg);` |
|       75 | 5067 | `	PH7_MemObjRelease(&sResult);` |
|        - | 5068 | `	/* Remove reentrancy guard */` |
|       75 | 5069 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       75 | 5070 | `	return pClass;` |
|      155 | 5071 | `}` |
|        - | 5072 | `/*` |
|        - | 5073 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 5074 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 5075 | ` */` |
|       38 | 5076 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        4 | 5077 | `{` |
|       42 | 5078 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        4 | 5079 | `}` |
|        - | 5080 | `/*` |
|        - | 5081 | ` * Check if the given name refer to an installed class.` |
|        - | 5082 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 5083 | ` */` |
|   768284 | 5084 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 5085 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 5086 | `	const char *zName,  /* Name of the target class */` |
|        - | 5087 | `	sxu32 nByte,        /* zName length */` |
|        - | 5088 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 5089 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 5090 | `						 */` |
|        - | 5091 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 5092 | `	)` |
|        5 | 5093 | `{` |
|        - | 5094 | `	SyHashEntry *pEntry;` |
|        - | 5095 | `	ph7_class *pClass;` |
|   384142 | 5096 | `	SXUNUSED(iNest);` |
|        - | 5097 | `	/* Exact class lookup.` |
|        - | 5098 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 5099 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   768289 | 5100 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   768289 | 5101 | `	if( pEntry == 0 ){` |
|        - | 5102 | `		/* Class not found in hash table — try autoload before giving up */` |
|      267 | 5103 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 5104 | `	}` |
|   768027 | 5105 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   768027 | 5106 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   384147 | 5107 | `}` |
|        - | 5108 | `/*` |
|        - | 5109 | ` * Reference Table Implementation` |
|        - | 5110 | ` * Status: stable <chm@symisc.net>` |
|        - | 5111 | ` * Intro` |
|        - | 5112 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 5113 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 5114 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 5115 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 5116 | ` *  Refer to the official for more information on this powerful` |
|        - | 5117 | ` *  extension.` |
|        - | 5118 | ` */` |
|        - | 5119 | `/*` |
|        - | 5120 | ` * Allocate a new reference entry.` |
|        - | 5121 | ` */` |
|  4364577 | 5122 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 5123 | `{` |
|        - | 5124 | `	VmRefObj *pRef;` |
|        - | 5125 | `	/* Allocate a new instance */` |
|  4364582 | 5126 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  4364582 | 5127 | `	if( pRef == 0 ){` |
|      ! 0 | 5128 | `		return 0;` |
|        - | 5129 | `	}` |
|        - | 5130 | `	/* Zero the structure */` |
|  4364582 | 5131 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 5132 | `	/* Initialize fields */` |
|  4364582 | 5133 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  4364582 | 5134 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  4364582 | 5135 | `	pRef->nIdx = nIdx;` |
|  4364582 | 5136 | `	return pRef;` |
|  2182908 | 5137 | `}` |
|        - | 5138 | `/*` |
|        - | 5139 | ` * Default hash function used by the reference table` |
|        - | 5140 | ` * for lookup/insertion operations.` |
|        - | 5141 | ` */` |
| 23448714 | 5142 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 5143 | `{` |
|        - | 5144 | `	/* Calculate the hash based on the memory object index */` |
| 23448719 | 5145 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 5146 | `}` |
|        - | 5147 | `/*` |
|        - | 5148 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 5149 | ` * in the reference table.` |
|        - | 5150 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 5151 | ` * otherwise.` |
|        - | 5152 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5153 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5154 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5155 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5156 | ` * Refer to the official for more information on this powerful` |
|        - | 5157 | ` * extension.` |
|        - | 5158 | ` */` |
| 13559697 | 5159 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 5160 | `{` |
|        - | 5161 | `	VmRefObj *pRef;` |
|        - | 5162 | `	sxu32 nBucket;` |
|        - | 5163 | `	/* Point to the appropriate bucket */` |
| 13559702 | 5164 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 5165 | `	/* Perform the lookup */` |
| 13559702 | 5166 | `	pRef = pVm->apRefObj[nBucket];` |
| 29859926 | 5167 | `	for(;;){` |
| 59714255 | 5168 | `		if( pRef == 0 ){` |
|  4853666 | 5169 | `			break;` |
|        - | 5170 | `		}` |
| 54860594 | 5171 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 5172 | `			/* Entry found */` |
|  8706041 | 5173 | `			return pRef;` |
|        - | 5174 | `		}` |
|        - | 5175 | `		/* Point to the next entry */` |
| 46154558 | 5176 | `		pRef = pRef->pNextCollide;` |
|        5 | 5177 | `	}` |
|        - | 5178 | `	/* No such entry,return NULL */` |
|  4853666 | 5179 | `	return 0;` |
|  6781697 | 5180 | `}` |
|        - | 5181 | `/*` |
|        - | 5182 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5183 | ` *` |
|        - | 5184 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5185 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5186 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5187 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5188 | ` * Refer to the official for more information on this powerful` |
|        - | 5189 | ` * extension.` |
|        - | 5190 | ` */` |
|  4364577 | 5191 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5192 | `{` |
|        - | 5193 | `	sxu32 nBucket;` |
|  4364582 | 5194 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 5195 | `		VmRefObj **apNew;` |
|        - | 5196 | `		sxu32 nNew;` |
|        - | 5197 | `		/* Allocate a larger table */` |
|    10557 | 5198 | `		nNew = pVm->nRefSize << 1;` |
|    10557 | 5199 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|    10557 | 5200 | `		if( apNew ){` |
|    10557 | 5201 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 5202 | `			sxu32 n;` |
|        - | 5203 | `			/* Zero the structure */` |
|    10557 | 5204 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 5205 | `			/* Rehash all referenced entries */` |
|  2966741 | 5206 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 5207 | `				/* Remove old collision links */` |
|  2956189 | 5208 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 5209 | `				/* Point to the appropriate bucket */` |
|  2956189 | 5210 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 5211 | `				/* Insert the entry  */` |
|  2956189 | 5212 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2956189 | 5213 | `				if( apNew[nBucket] ){` |
|  2306069 | 5214 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1153032 | 5215 | `				}` |
|  2956189 | 5216 | `				apNew[nBucket] = pEntry;` |
|        - | 5217 | `				/* Point to the next entry */` |
|  2956189 | 5218 | `				pEntry = pEntry->pNext;` |
|  1478097 | 5219 | `			}` |
|        - | 5220 | `			/* Release the old table */` |
|    10557 | 5221 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 5222 | `			/* Install the new one */` |
|    10557 | 5223 | `			pVm->apRefObj = apNew;` |
|    10557 | 5224 | `			pVm->nRefSize = nNew;` |
|     5276 | 5225 | `		}` |
|     5276 | 5226 | `	}` |
|        - | 5227 | `	/* Point to the appropriate bucket */` |
|  4364582 | 5228 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 5229 | `	/* Insert the entry */` |
|  4364582 | 5230 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  4364582 | 5231 | `	if( pVm->apRefObj[nBucket] ){` |
|  3360045 | 5232 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1680559 | 5233 | `	}` |
|  4364582 | 5234 | `	pVm->apRefObj[nBucket] = pRef;` |
|  4364582 | 5235 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  4364582 | 5236 | `	pVm->nRefUsed++;` |
|  4364582 | 5237 | `	return SXRET_OK;` |
|        5 | 5238 | `}` |
|        - | 5239 | `/*` |
|        - | 5240 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 5241 | ` * the reference table.` |
|        - | 5242 | ` * This function is invoked when the user perform an unset` |
|        - | 5243 | ` * call [i.e: unset($var); ].` |
|        - | 5244 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5245 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5246 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5247 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5248 | ` * Refer to the official for more information on this powerful` |
|        - | 5249 | ` * extension.` |
|        - | 5250 | ` */` |
|  4230921 | 5251 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 5252 | `{` |
|        - | 5253 | `	ph7_hashmap_node **apNode;` |
|        - | 5254 | `	SyHashEntry **apEntry;` |
|        - | 5255 | `	sxu32 n;` |
|        - | 5256 | `	/* Point to the reference table */` |
|  4230926 | 5257 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  4230926 | 5258 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 5259 | `	/* Unlink the entry from the reference table */` |
|  4726108 | 5260 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   495187 | 5261 | `		if( apEntry[n] ){` |
|   489313 | 5262 | `			SyHashDeleteEntry2(apEntry[n]);` |
|   245269 | 5263 | `		}` |
|   248211 | 5264 | `	}` |
|  7951117 | 5265 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3720196 | 5266 | `		if( apNode[n] ){` |
|     1380 | 5267 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|      688 | 5268 | `		}` |
|  1860100 | 5269 | `	}` |
|  4230926 | 5270 | `	if( pRef->pPrevCollide ){` |
|  1662670 | 5271 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   830875 | 5272 | `	}else{` |
|  2568261 | 5273 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 5274 | `	}` |
|  4230926 | 5275 | `	if( pRef->pNextCollide ){` |
|  2247633 | 5276 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|  1124364 | 5277 | `	}` |
|  4230926 | 5278 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 5279 | `	/* Release the node */` |
|  4230926 | 5280 | `	SySetRelease(&pRef->aReference);` |
|  4230926 | 5281 | `	SySetRelease(&pRef->aArrEntries);` |
|  4230926 | 5282 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  4230926 | 5283 | `	pVm->nRefUsed--;` |
|  4230926 | 5284 | `	return SXRET_OK;` |
|        5 | 5285 | `}` |
|        - | 5286 | `/*` |
|        - | 5287 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 5288 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5289 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5290 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5291 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5292 | ` * Refer to the official for more information on this powerful` |
|        - | 5293 | ` * extension.` |
|        - | 5294 | ` */` |
|  4411121 | 5295 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 5296 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5297 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5298 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5299 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 5300 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 5301 | `	)` |
|        5 | 5302 | `{` |
|  4411126 | 5303 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 5304 | `	VmRefObj *pRef;` |
|        - | 5305 | `	/* Check if the referenced object already exists */` |
|  4411126 | 5306 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4411126 | 5307 | `	if( pRef == 0 ){` |
|        - | 5308 | `		/* Create a new entry */` |
|  4364582 | 5309 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  4364582 | 5310 | `		if( pRef == 0 ){` |
|      ! 0 | 5311 | `			return SXERR_MEM;` |
|        - | 5312 | `		}` |
|  4364582 | 5313 | `		pRef->iFlags = iFlags;` |
|        - | 5314 | `		/* Install the entry */` |
|  4364582 | 5315 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  2182903 | 5316 | `	}` |
|  4411126 | 5317 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  4411126 | 5318 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 5319 | `		VmSlot sRef;` |
|        - | 5320 | `		/* Local frame,record referenced entry so that it can` |
|        - | 5321 | `		 * be deleted when we leave this frame.` |
|        - | 5322 | `		 */` |
|   489287 | 5323 | `		sRef.nIdx = nIdx;` |
|   489287 | 5324 | `		sRef.pUserData = pEntry;` |
|   489287 | 5325 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 5326 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 5327 | `		}` |
|   245256 | 5328 | `	}` |
|  4411126 | 5329 | `	if( pEntry ){` |
|        - | 5330 | `		/* Address of the hash-entry */` |
|   535573 | 5331 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|   268399 | 5332 | `	}` |
|  4411126 | 5333 | `	if( pMapEntry ){` |
|        - | 5334 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3822098 | 5335 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1911046 | 5336 | `	}` |
|  4411126 | 5337 | `	return SXRET_OK;` |
|  2206180 | 5338 | `}` |
|        - | 5339 | `/*` |
|        - | 5340 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 5341 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 5342 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 5343 | ` * the reference implementation is consistent,solid and it's` |
|        - | 5344 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 5345 | ` * Refer to the official for more information on this powerful` |
|        - | 5346 | ` * extension.` |
|        - | 5347 | ` */` |
|  4202193 | 5348 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 5349 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 5350 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 5351 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 5352 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 5353 | `	)` |
|        5 | 5354 | `{` |
|        - | 5355 | `	VmRefObj *pRef;` |
|        - | 5356 | `	sxu32 n;` |
|        - | 5357 | `	/* Check if the referenced object already exists */` |
|  4202198 | 5358 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  4202198 | 5359 | `	if( pRef == 0 ){` |
|        - | 5360 | `		/* Not such entry */` |
|   489089 | 5361 | `		return SXERR_NOTFOUND;` |
|        - | 5362 | `	}` |
|        - | 5363 | `	/* Remove the desired entry */` |
|  3713114 | 5364 | `	if( pEntry ){` |
|        - | 5365 | `		SyHashEntry **apEntry;` |
|       87 | 5366 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 5367 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      185 | 5368 | `			if( apEntry[n] == pEntry ){` |
|        - | 5369 | `				/* Nullify the entry */` |
|       85 | 5370 | `				apEntry[n] = 0;` |
|        - | 5371 | `				/*` |
|        - | 5372 | `				 * NOTE:` |
|        - | 5373 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 5374 | `				 * we avoid wasting spaces.` |
|        - | 5375 | `				 */` |
|       40 | 5376 | `			}` |
|       95 | 5377 | `		}` |
|       41 | 5378 | `	}` |
|  3713114 | 5379 | `	if( pMapEntry ){` |
|        - | 5380 | `		ph7_hashmap_node **apNode;` |
|  3713032 | 5381 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  7426323 | 5382 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3713296 | 5383 | `			if( apNode[n] == pMapEntry ){` |
|        - | 5384 | `				/* nullify the entry */` |
|  3713032 | 5385 | `				apNode[n] = 0;` |
|  1856513 | 5386 | `			}` |
|  1856650 | 5387 | `		}` |
|  1856513 | 5388 | `	}` |
|  3713114 | 5389 | `	return SXRET_OK;` |
|  2101716 | 5390 | `}` |
|        - | 5391 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 5392 | `/*` |
|        - | 5393 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 5394 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 5395 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 5396 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 5397 | ` * For more information on how to register IO stream devices,please` |
|        - | 5398 | ` * refer to the official documentation.` |
|        - | 5399 | ` */` |
|    30158 | 5400 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 5401 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 5402 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 5403 | `	int nByte              /* *pzDevice length*/` |
|        - | 5404 | `	)` |
|        5 | 5405 | `{` |
|        - | 5406 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 5407 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 5408 | `	SyString sDev,sCur;` |
|        - | 5409 | `	sxu32 n,nEntry;` |
|        - | 5410 | `	int rc;` |
|        - | 5411 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    30163 | 5412 | `	zNext = zCur = zIn = *pzDevice;` |
|    30163 | 5413 | `	zEnd = &zIn[nByte];` |
|  1912148 | 5414 | `	while( zIn < zEnd ){` |
|  1882020 | 5415 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 5416 | `			/* Got one */` |
|       32 | 5417 | `			zNext = &zIn[sizeof("://")-1];` |
|       32 | 5418 | `			break;` |
|        - | 5419 | `		}` |
|        - | 5420 | `		/* Advance the cursor */` |
|  1881990 | 5421 | `		zIn++;` |
|        5 | 5422 | `	}` |
|    30163 | 5423 | `	if( zIn >= zEnd ){` |
|        - | 5424 | `		/* No such scheme,return the default stream */` |
|    30133 | 5425 | `		return pVm->pDefStream;` |
|        - | 5426 | `	}` |
|       32 | 5427 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 5428 | `	/* Remove leading and trailing white spaces */` |
|       32 | 5429 | `	SyStringFullTrim(&sDev);` |
|        - | 5430 | `	/* Perform a linear lookup on the installed stream devices */` |
|       32 | 5431 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       32 | 5432 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|       66 | 5433 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       66 | 5434 | `		pStream = apStream[n];` |
|       66 | 5435 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 5436 | `		/* Perfrom a case-insensitive comparison */` |
|       66 | 5437 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|       66 | 5438 | `		if( rc == 0 ){` |
|        - | 5439 | `			/* Stream device found */` |
|       32 | 5440 | `			*pzDevice = zNext;` |
|       32 | 5441 | `			return pStream;` |
|        - | 5442 | `		}` |
|       19 | 5443 | `	}` |
|        - | 5444 | `	/* No such stream,return NULL */` |
|      ! 0 | 5445 | `	return 0;` |
|    15084 | 5446 | `}` |
|        - | 5447 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 5448 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 5449 |  |
