# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2955/3436 lines (86.00%)

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
|         - |   74 | ` * TRUE when php compares these two operands as UNORDERED -- a NaN against` |
|         - |   75 | ` * something php reads as a NUMBER or as a STRING. php answers 1 for that` |
|         - |   76 | `` * comparison in BOTH directions, which is what makes `==`, `<`, `>`, `<=` and`` |
|         - |   77 | `` * `>=` all false at once while `<=>` is 1 either way round.`` |
|         - |   78 | ` *` |
|         - |   79 | ` * Two rules ride on this predicate and both were wrong without them.` |
|         - |   80 | ` *` |
|         - |   81 | ` * It must be asked BEFORE PH7_MemObjCmp runs: the comparator converts its` |
|         - |   82 | ` * operands IN PLACE, so a NaN that took the string path is a MEMOBJ_STRING by` |
|         - |   83 | ` * the time the answer comes back and the float is gone. That is how` |
|         - |   84 | `` * `NAN == "NAN"` was TRUE here (php: false) and `NAN < "abc"` was TRUE`` |
|         - |   85 | ` * (php: false) -- the screen ran on two strings and saw no NaN at all.` |
|         - |   86 | ` *` |
|         - |   87 | ` * And php's own precedence comes FIRST: a comparison against null, a bool, an` |
|         - |   88 | ``  * array or an object never reaches the numeric/string rule, so `NAN == true` `` |
|         - |   89 | `` * is TRUE (both truthy) and `NAN < []` is TRUE (an array is greater). Those`` |
|         - |   90 | ` * flags are exactly the branches PH7_MemObjCmp answers ahead of its numeric` |
|         - |   91 | ` * one. A RESOURCE is not among them: php reads it as its ID there, so a NaN` |
|         - |   92 | ` * against one is as unordered as a NaN against any other number.` |
|         - |   93 | ` */` |
|   6105046 |   94 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   95 | `{` |
|   6105046 |   96 | `	if( (pLeft->iFlags \| pRight->iFlags)` |
|   6105051 |   97 | `	  & (MEMOBJ_NULL\|MEMOBJ_BOOL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ) ){` |
|       ! 0 |   98 | `		return FALSE;` |
|         - |   99 | `	}` |
|   6105051 |  100 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       334 |  101 | `		return TRUE;` |
|         - |  102 | `	}` |
|   6104719 |  103 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       145 |  104 | `		return TRUE;` |
|         - |  105 | `	}` |
|   6104575 |  106 | `	return FALSE;` |
|   3057155 |  107 | `}` |
|         - |  108 | `/*` |
|         - |  109 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |  110 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |  111 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |  112 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |  113 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |  114 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |  115 | ` * still go through the existing numeric coercion.` |
|         - |  116 | ` */` |
|    729633 |  117 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |  118 | `{` |
|         - |  119 | `	SyString sStr;` |
|    729638 |  120 | `	sxu8 bReal = FALSE;` |
|    729638 |  121 | `	const char *zTail = 0;` |
|         - |  122 | `	const char *zEnd;` |
|    729638 |  123 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    729620 |  124 | `		return FALSE;` |
|         - |  125 | `	}` |
|        21 |  126 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        21 |  127 | `	if( sStr.nByte == 0 ){` |
|       ! 0 |  128 | `		return TRUE;` |
|         - |  129 | `	}` |
|        21 |  130 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|         5 |  131 | `		return TRUE;` |
|         - |  132 | `	}` |
|         - |  133 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|         - |  134 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|         - |  135 | `	 * still takes the Perl path. */` |
|        17 |  136 | `	zEnd = sStr.zString + sStr.nByte;` |
|        17 |  137 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|       ! 0 |  138 | `		zTail++;` |
|       ! 0 |  139 | `	}` |
|        17 |  140 | `	return zTail < zEnd;` |
|    365382 |  141 | `}` |
|         - |  142 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|         - |  143 | `/* Constant expander used by define(); used below to recognise user-defined` |
|         - |  144 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|         - |  145 | ` * a define() overwrites them. */` |
|         - |  146 | `/*` |
|         - |  147 | ` * Register a constant and it's associated expansion callback so that` |
|         - |  148 | ` * it can be expanded from the target PHP program.` |
|         - |  149 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|         - |  150 | ` * simple and work as follows:` |
|         - |  151 | ` * Each registered constant have a C procedure associated with it.` |
|         - |  152 | ` * This procedure known as the constant expansion callback is responsible` |
|         - |  153 | ` * of expanding the invoked constant to the desired value,for example:` |
|         - |  154 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|         - |  155 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|         - |  156 | ` * (Windows,Linux,...) and so on.` |
|         - |  157 | ` * Please refer to the official documentation for additional information.` |
|         - |  158 | ` */` |
|   6526960 |  159 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  160 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  161 | `	const SyString *pName,  /* Constant name */` |
|         - |  162 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  163 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  164 | `	)` |
|         5 |  165 | `{` |
|   6526965 |  166 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  167 | `}` |
|         - |  168 | `/*` |
|         - |  169 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  170 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  171 | ` */` |
|   6527264 |  172 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
|         - |  173 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  174 | `	const SyString *pName,  /* Constant name */` |
|         - |  175 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  176 | `	void *pUserData,        /* Last argument to xExpand() */` |
|         - |  177 | `	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */` |
|         - |  178 | `	sxu32 nLine,            /* Declaration line, 0 = unknown */` |
|         - |  179 | `	int bUser               /* 1 when defined by user code */` |
|         - |  180 | `	)` |
|         5 |  181 | `{` |
|         - |  182 | `	ph7_constant *pCons;` |
|         - |  183 | `	SyHashEntry *pEntry;` |
|         - |  184 | `	char *zDupName;` |
|         - |  185 | `	sxi32 rc;` |
|   6527269 |  186 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   6527269 |  187 | `	if( pEntry ){` |
|         - |  188 | `		/* Overwrite the old definition and return immediately */` |
|         3 |  189 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  190 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|         - |  191 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|         - |  192 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|         2 |  193 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|         3 |  194 | `		 && pCons->pUserData != pUserData ){` |
|         3 |  195 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|         3 |  196 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|         1 |  197 | `		}` |
|         3 |  198 | `		pCons->xExpand = xExpand;` |
|         3 |  199 | `		pCons->pUserData = pUserData;` |
|         3 |  200 | `		if( pFile ){` |
|         3 |  201 | `			SyStringDupPtr(&pCons->sFile,pFile);` |
|         2 |  202 | `		}else{` |
|       ! 0 |  203 | `			SyStringInitFromBuf(&pCons->sFile,0,0);` |
|         - |  204 | `		}` |
|         3 |  205 | `		pCons->nLine = nLine;` |
|         3 |  206 | `		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         3 |  207 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|         3 |  208 | `		return SXRET_OK;` |
|         - |  209 | `	}` |
|         - |  210 | `	/* Allocate a new constant instance */` |
|   6527267 |  211 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   6527267 |  212 | `	if( pCons == 0 ){` |
|       ! 0 |  213 | `		return 0;` |
|         - |  214 | `	}` |
|         - |  215 | `	/* Duplicate constant name */` |
|   6527267 |  216 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   6527267 |  217 | `	if( zDupName == 0 ){` |
|       ! 0 |  218 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  219 | `		return 0;` |
|         - |  220 | `	}` |
|   6527267 |  221 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|   6527267 |  222 | `	if( pFile ){` |
|       307 |  223 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       151 |  224 | `	}` |
|   6527267 |  225 | `	pCons->nLine = nLine;` |
|   6527267 |  226 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         - |  227 | `	/* Install the constant */` |
|   6527267 |  228 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   6527267 |  229 | `	pCons->xExpand = xExpand;` |
|   6527267 |  230 | `	pCons->pUserData = pUserData;` |
|   6527267 |  231 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   6527267 |  232 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   6527267 |  233 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  234 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  235 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  236 | `		return rc;` |
|         - |  237 | `	}` |
|         - |  238 | `	/* All done,constant can be invoked from PHP code */` |
|   6527267 |  239 | `	return SXRET_OK;` |
|   3263637 |  240 | `}` |
|         - |  241 | `/*` |
|         - |  242 | ` * Allocate a new foreign function instance.` |
|         - |  243 | ` * This function return SXRET_OK on success. Any other` |
|         - |  244 | ` * return value indicates failure.` |
|         - |  245 | ` * Please refer to the official documentation for an introduction to` |
|         - |  246 | ` * the foreign function mechanism.` |
|         - |  247 | ` */` |
|  11023958 |  248 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
|         - |  249 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  250 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  251 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  252 | `	void *pUserData,          /* Foreign function private data */` |
|         - |  253 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|         - |  254 | `	)` |
|         5 |  255 | `{` |
|         - |  256 | `	ph7_user_func *pFunc;` |
|         - |  257 | `	char *zDup;` |
|         - |  258 | `	/* Allocate a new user function */` |
|  11023963 |  259 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  11023963 |  260 | `	if( pFunc == 0 ){` |
|       ! 0 |  261 | `		return SXERR_MEM;` |
|         - |  262 | `	}` |
|         - |  263 | `	/* Duplicate function name */` |
|  11023963 |  264 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  11023963 |  265 | `	if( zDup == 0 ){` |
|       ! 0 |  266 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  267 | `		return SXERR_MEM;` |
|         - |  268 | `	}` |
|         - |  269 | `	/* Zero the structure */` |
|  11023963 |  270 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  271 | `	/* Initialize structure fields */` |
|  11023963 |  272 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  11023963 |  273 | `	pFunc->pVm   = pVm;` |
|  11023963 |  274 | `	pFunc->xFunc = xFunc;` |
|  11023963 |  275 | `	pFunc->pUserData = pUserData;` |
|  11023963 |  276 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  277 | `	/* Write a pointer to the new function */` |
|  11023963 |  278 | `	*ppOut = pFunc;` |
|  11023963 |  279 | `	return SXRET_OK;` |
|   5511984 |  280 | `}` |
|         - |  281 | `/*` |
|         - |  282 | ` * Install a foreign function and it's associated callback so that` |
|         - |  283 | ` * it can be invoked from the target PHP code.` |
|         - |  284 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  285 | ` * return value indicates failure.` |
|         - |  286 | ` * Please refer to the official documentation for an introduction to` |
|         - |  287 | ` * the foreign function mechanism.` |
|         - |  288 | ` */` |
|   4273490 |  289 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|         - |  290 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  291 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  292 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  293 | `	void *pUserData           /* Foreign function private data */` |
|         - |  294 | `	)` |
|         5 |  295 | `{` |
|         - |  296 | `	ph7_user_func *pFunc;` |
|         - |  297 | `	SyHashEntry *pEntry;` |
|         - |  298 | `	sxi32 rc;` |
|         - |  299 | `	/* Overwrite any previously registered function with the same name */` |
|   4273495 |  300 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   4273495 |  301 | `	if( pEntry ){` |
|       ! 0 |  302 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|       ! 0 |  303 | `		pFunc->pUserData = pUserData;` |
|       ! 0 |  304 | `		pFunc->xFunc = xFunc;` |
|       ! 0 |  305 | `		SySetReset(&pFunc->aAux);` |
|         - |  306 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  307 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  308 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  309 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|       ! 0 |  310 | `		pFunc->nMinArg  = 0;` |
|       ! 0 |  311 | `		pFunc->nMaxArg  = 0;` |
|       ! 0 |  312 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|       ! 0 |  313 | `		pFunc->bAtLeast = 0;` |
|       ! 0 |  314 | `		return SXRET_OK;` |
|         - |  315 | `	}` |
|         - |  316 | `	/* Create a new user function */` |
|   4273495 |  317 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   4273495 |  318 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  319 | `		return rc;` |
|         - |  320 | `	}` |
|         - |  321 | `	/* Install the function in the corresponding hashtable */` |
|   4273495 |  322 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   4273495 |  323 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  324 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  325 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  326 | `		return rc;` |
|         - |  327 | `	}` |
|         - |  328 | `	/* User function successfully installed */` |
|   4273495 |  329 | `	return SXRET_OK;` |
|   2136750 |  330 | `}` |
|         - |  331 | `/*` |
|         - |  332 | ` * Initialize a VM function.` |
|         - |  333 | ` */` |
|   6923304 |  334 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  335 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  336 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  337 | `	const char *zName,  /* Function name */` |
|         - |  338 | `	sxu32 nByte,        /* zName length */` |
|         - |  339 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  340 | `	void *pUserData     /* Function private data */` |
|         - |  341 | `	)` |
|         5 |  342 | `{` |
|         - |  343 | `	/* Zero the structure */` |
|   6923309 |  344 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  345 | `	/* Initialize structure fields */` |
|         - |  346 | `	/* Arguments container */` |
|   6923309 |  347 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  348 | `	/* Static variable container */` |
|   6923309 |  349 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  350 | `	/* Bytecode container */` |
|   6923309 |  351 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  352 | `    /* Preallocate some instruction slots */` |
|   6923309 |  353 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  354 | `	/* Closure environment */` |
|   6923309 |  355 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  356 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   6923309 |  357 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  358 | `	/* Declared #[...] attributes */` |
|   6923309 |  359 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   6923309 |  360 | `	pFunc->iFlags = iFlags;` |
|   6923309 |  361 | `	pFunc->pUserData = pUserData;` |
|         - |  362 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  363 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   6923309 |  364 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   6923309 |  365 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  366 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   6905225 |  367 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   3452615 |  368 | `	}else{` |
|         - |  369 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  370 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  371 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     18089 |  372 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     18089 |  373 | `		if( pFile ){` |
|     18089 |  374 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|      9042 |  375 | `		}` |
|         - |  376 | `	}` |
|   6923309 |  377 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   6923309 |  378 | `	return SXRET_OK;` |
|         5 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Look a name up in the compiled-function table AS A SCRIPT SPELLS IT.` |
|         - |  382 | ` *` |
|         - |  383 | ` * hFunction is the ENGINE's table, not the script's. Besides the functions a program` |
|         - |  384 | ` * declared it holds every mounted class METHOD -- VmMountUserClassMethods installs each` |
|         - |  385 | `` * one under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c mints -- and`` |
|         - |  386 | `` * every compiled CLOSURE, under `[closure_N]`. Neither is a php function name (no php`` |
|         - |  387 | `` * label may hold a `[`, an `@` or a `]`), and php has no table in which a script can find`` |
|         - |  388 | ` * one.` |
|         - |  389 | ` *` |
|         - |  390 | ` * A plain SyHashGet therefore answered a name that does not exist to every surface that` |
|         - |  391 | ` * asks whether a function does: function_exists(), is_callable() and the whole callback` |
|         - |  392 | `` * screen behind it, ReflectionFunction, `new Fiber(name)` -- and the dispatch itself.`` |
|         - |  393 | ` * Reaching a METHOD that way really did run it: the body assumes the receiver frame the` |
|         - |  394 | `` * plain-function path never builds, so `$n = '[__Foo@bar_...]'; $n();` popped past the`` |
|         - |  395 | ` * bottom of the operand stack (SIGSEGV in the release build, an ASan heap-buffer-overflow` |
|         - |  396 | ` * READ in VmByteCodeExecBody).` |
|         - |  397 | ` *` |
|         - |  398 | ` * bEngineName is for the engine's OWN dispatch of those same entries, which is by name` |
|         - |  399 | `` * too: OP_MEMBER pushes a resolved method's `sVmName` onto the callee slot, the closure`` |
|         - |  400 | `` * machinery unwraps a Closure to its `[closure_N]`, and the two synthetic call builders`` |
|         - |  401 | ` * do both without an OP_MEMBER ahead of them. Each of those marks its own call site` |
|         - |  402 | ` * (MEMOBJ_AUX_MEMBERCALL / MEMOBJ_AUX_ENGINEFN / the OP_CALL local); nothing a program` |
|         - |  403 | ` * wrote ever passes 1.` |
|         - |  404 | ` */` |
|  10908914 |  405 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(` |
|         - |  406 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  407 | `	const void *pName,  /* Function name */` |
|         - |  408 | `	sxu32 nByte,        /* Name length */` |
|         - |  409 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|         - |  410 | `	)` |
|         5 |  411 | `{` |
|  10908919 |  412 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,pName,nByte);` |
|  10908919 |  413 | `	if( pEntry && !bEngineName ){` |
|    415037 |  414 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    415037 |  415 | `		if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|        25 |  416 | `			return 0;` |
|         - |  417 | `		}` |
|    207944 |  418 | `	}` |
|  10908895 |  419 | `	return pEntry;` |
|   5457962 |  420 | `}` |
|         - |  421 | `/*` |
|         - |  422 | ` * Namespace-aware function lookup.` |
|         - |  423 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  424 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  425 | ` */` |
|         - |  426 | `/*` |
|         - |  427 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  428 | ` */` |
|  27954136 |  429 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  430 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  431 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  432 | `	SyString *pName     /* Function name */` |
|         - |  433 | `	)` |
|         5 |  434 | `{` |
|         - |  435 | `	SyHashEntry *pEntry;` |
|         - |  436 | `	sxi32 rc;` |
|  27954141 |  437 | `	if( pName == 0 ){` |
|         - |  438 | `		/* Use the built-in name */` |
|    170645 |  439 | `		pName = &pFunc->sName;` |
|     85320 |  440 | `	}` |
|         - |  441 | `	/* Check for duplicates (functions with the same name) first */` |
|  27954141 |  442 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  27954141 |  443 | `	if( pEntry ){` |
|  21327733 |  444 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  21327733 |  445 | `		if( pLink != pFunc ){` |
|         - |  446 | `			/* Link */` |
|        91 |  447 | `			pFunc->pNextName = pLink;` |
|        91 |  448 | `			pEntry->pUserData = pFunc;` |
|        43 |  449 | `		}` |
|  21327733 |  450 | `		return SXRET_OK;` |
|         - |  451 | `	}` |
|         - |  452 | `	/* First time seen */` |
|   6626413 |  453 | `	pFunc->pNextName = 0;` |
|   6626413 |  454 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   6626413 |  455 | `	return rc;` |
|  13977073 |  456 | `}` |
|         - |  457 | `/*` |
|         - |  458 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  459 | ` */` |
|   1135192 |  460 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  461 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  462 | `	ph7_class *pClass /* Target Class */` |
|         - |  463 | `	)` |
|         5 |  464 | `{` |
|   1135197 |  465 | `	SyString *pName = &pClass->sName;` |
|         - |  466 | `	SyHashEntry *pEntry;` |
|         - |  467 | `	sxi32 rc;` |
|         - |  468 | `	/* Check for duplicates */` |
|   1135197 |  469 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   1135197 |  470 | `	if( pEntry ){` |
|         3 |  471 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  472 | `		/* Link entry with the same name */` |
|         3 |  473 | `		pClass->pNextName = pLink;` |
|         3 |  474 | `		pEntry->pUserData = pClass;` |
|         3 |  475 | `		return SXRET_OK;` |
|         - |  476 | `	}` |
|   1135195 |  477 | `	pClass->pNextName = 0;` |
|         - |  478 | `	/* Perform a simple hashtable insertion */` |
|   1135195 |  479 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   1135195 |  480 | `	return rc;` |
|    567601 |  481 | `}` |
|         - |  482 | `/*` |
|         - |  483 | ` * Instruction builder interface.` |
|         - |  484 | ` */` |
|  14089656 |  485 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  486 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  487 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  488 | `	sxi32 iP1,    /* First operand */` |
|         - |  489 | `	sxu32 iP2,    /* Second operand */` |
|         - |  490 | `	void *p3,     /* Third operand */` |
|         - |  491 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  492 | `	)` |
|         5 |  493 | `{` |
|         - |  494 | `	VmInstr sInstr;` |
|  14089661 |  495 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  496 | `	sxi32 rc;` |
|         - |  497 | `	/* Fill the VM instruction */` |
|  14089661 |  498 | `	sInstr.iOp = (sxu8)iOp;` |
|  14089661 |  499 | `	sInstr.iP1 = iP1;` |
|  14089661 |  500 | `	sInstr.iP2 = iP2;` |
|  14089661 |  501 | `	sInstr.p3  = p3;` |
|         - |  502 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  503 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  504 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  505 | `	 * between statements, hence the range check. */` |
|  14089661 |  506 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|         - |  507 | `	/* Nothing is discarded until the statement that owns this call says so` |
|         - |  508 | `	 * (GenStateMarkDiscardedCall, after the fact) — but the field must not be` |
|         - |  509 | `	 * this stack frame's leftovers in the meantime. */` |
|  14089661 |  510 | `	sInstr.bDiscard = 0;` |
|  14089661 |  511 | `	sInstr.nLine = 0;` |
|  14089661 |  512 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|   4989763 |  513 | `		sInstr.nLine = pGen->pIn->nLine;` |
|  11594782 |  514 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  515 | `		/* Past the end (statement tail): blame the last real token. */` |
|   9035551 |  516 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|   4517773 |  517 | `	}` |
|  14089661 |  518 | `	if( pIndex ){` |
|         - |  519 | `		/* Instruction index in the bytecode array */` |
|   1140411 |  520 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    570203 |  521 | `	}` |
|         - |  522 | `	/* Finally,record the instruction */` |
|  14089661 |  523 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  14089661 |  524 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  525 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  526 | `		/* Fall throw */` |
|       ! 0 |  527 | `	}` |
|  14089661 |  528 | `	return rc;` |
|         5 |  529 | `}` |
|         - |  530 | `/*` |
|         - |  531 | ` * Swap the current bytecode container with the given one.` |
|         - |  532 | ` */` |
|    533796 |  533 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  534 | `{` |
|    533801 |  535 | `	if( pContainer == 0 ){` |
|         - |  536 | `		/* Point to the default container */` |
|       ! 0 |  537 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  538 | `	}else{` |
|         - |  539 | `		/* Change container */` |
|    533801 |  540 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  541 | `	}` |
|    533801 |  542 | `	return SXRET_OK;` |
|         5 |  543 | `}` |
|         - |  544 | `/*` |
|         - |  545 | ` * Return the current bytecode container.` |
|         - |  546 | ` */` |
|   1133282 |  547 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  548 | `{` |
|   1133287 |  549 | `	return pVm->pByteContainer;` |
|         5 |  550 | `}` |
|         - |  551 | `/*` |
|         - |  552 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  553 | ` */` |
|    346888 |  554 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  555 | `{` |
|         - |  556 | `	VmInstr *pInstr;` |
|    346893 |  557 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    346893 |  558 | `	return pInstr;` |
|         5 |  559 | `}` |
|         - |  560 | `/*` |
|         - |  561 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  562 | ` */` |
|   7043602 |  563 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  564 | `{` |
|   7043607 |  565 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  566 | `}` |
|         - |  567 | `/*` |
|         - |  568 | ` * Pop the last VM instruction.` |
|         - |  569 | ` */` |
|   1007160 |  570 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  571 | `{` |
|   1007165 |  572 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  573 | `}` |
|         - |  574 | `/*` |
|         - |  575 | ` * Peek the last VM instruction.` |
|         - |  576 | ` */` |
|   3741718 |  577 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  578 | `{` |
|   3741723 |  579 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  580 | `}` |
|     88160 |  581 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  582 | `{` |
|         - |  583 | `	VmInstr *aInstr;` |
|         - |  584 | `	sxu32 n;` |
|     88165 |  585 | `	n = SySetUsed(pVm->pByteContainer);` |
|     88165 |  586 | `	if( n < 2 ){` |
|       ! 0 |  587 | `		return 0;` |
|         - |  588 | `	}` |
|     88165 |  589 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     88165 |  590 | `	return &aInstr[n - 2];` |
|     44085 |  591 | `}` |
|         - |  592 | `/*` |
|         - |  593 | ` * Allocate a new virtual machine frame.` |
|         - |  594 | ` */` |
|   3719332 |  595 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  596 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  597 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  598 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  599 | `	)` |
|         5 |  600 | `{` |
|         - |  601 | `	VmFrame *pFrame;` |
|         - |  602 | `	/* Allocate a new vm frame */` |
|   3719337 |  603 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   3719337 |  604 | `	if( pFrame == 0 ){` |
|       ! 0 |  605 | `		return 0;` |
|         - |  606 | `	}` |
|         - |  607 | `	/* Zero the structure */` |
|   3719337 |  608 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  609 | `	/* Initialize frame fields */` |
|   3719337 |  610 | `	pFrame->pUserData = pUserData;` |
|   3719337 |  611 | `	pFrame->pThis = pThis;` |
|   3719337 |  612 | `	pFrame->pVm = pVm;` |
|   3719337 |  613 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   3719337 |  614 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   3719337 |  615 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   3719337 |  616 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   3719337 |  617 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  618 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  619 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   3719337 |  620 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   3719337 |  621 | `	return pFrame;` |
|   1859886 |  622 | `}` |
|         - |  623 | `/* Forward declaration */` |
|         - |  624 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  625 | `/*` |
|         - |  626 | ` * Enter a VM frame.` |
|         - |  627 | ` */` |
|   3718584 |  628 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  629 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  630 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  631 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  632 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  633 | `	)` |
|         5 |  634 | `{` |
|         - |  635 | `	VmFrame *pFrame;` |
|         - |  636 | `	/* Allocate a new frame */` |
|   3718589 |  637 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   3718589 |  638 | `	if( pFrame == 0 ){` |
|       ! 0 |  639 | `		return SXERR_MEM;` |
|         - |  640 | `	}` |
|         - |  641 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   3718589 |  642 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  643 | `	/* Link to the list of active VM frame */` |
|   3718589 |  644 | `	pFrame->pParent = pVm->pFrame;` |
|   3718589 |  645 | `	pVm->pFrame = pFrame;` |
|   3718589 |  646 | `	if( ppFrame ){` |
|         - |  647 | `		/* Write a pointer to the new VM frame */` |
|   3712833 |  648 | `		*ppFrame = pFrame;` |
|   1856629 |  649 | `	}` |
|   3718589 |  650 | `	return SXRET_OK;` |
|   1859512 |  651 | `}` |
|         - |  652 | `/*` |
|         - |  653 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  654 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  655 | ` * information.` |
|         - |  656 | ` */` |
|       462 |  657 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  658 | `{` |
|         - |  659 | `	VmFrame *pTarget,*pGlobal;` |
|         - |  660 | `	SyHashEntry *pEntry;` |
|         - |  661 | `	sxi32 rc;` |
|       467 |  662 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|         - |  663 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|         - |  664 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|         - |  665 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|         - |  666 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|         - |  667 | `	 * writes never reached the real global. */` |
|       467 |  668 | `	pGlobal = pTarget;` |
|       967 |  669 | `	while( pGlobal->pParent ){` |
|       505 |  670 | `		pGlobal = pGlobal->pParent;` |
|         5 |  671 | `	}` |
|       467 |  672 | `	if( pGlobal == pTarget ){` |
|         - |  673 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|       ! 0 |  674 | `		return SXRET_OK;` |
|         - |  675 | `	}` |
|         - |  676 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|         - |  677 | `	 * plain global that would shadow it. */` |
|       467 |  678 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|       467 |  679 | `	if( pEntry == 0 ){` |
|       465 |  680 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|       230 |  681 | `	}` |
|       467 |  682 | `	if( pEntry == 0 ){` |
|         - |  683 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|         - |  684 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|         - |  685 | `		 * the assignment went to a local nobody could read. */` |
|        12 |  686 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|        12 |  687 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  688 | `			return rc;` |
|         - |  689 | `		}` |
|        12 |  690 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        12 |  691 | `		if( pEntry == 0 ){` |
|       ! 0 |  692 | `			return SXERR_NOTFOUND;` |
|         - |  693 | `		}` |
|         5 |  694 | `	}` |
|         - |  695 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|         - |  696 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|       698 |  697 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|       462 |  698 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|       467 |  699 | `	return SXRET_OK;` |
|       236 |  700 | `}` |
|         - |  701 | `/*` |
|         - |  702 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  703 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  704 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  705 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  706 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  707 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  708 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  709 | ` */` |
|   3713382 |  710 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  711 | `{` |
|   3713387 |  712 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 |  713 | `		pVm->pResumeFrame = 0;` |
|       ! 0 |  714 | `	}` |
|   3713387 |  715 | `}` |
|         - |  716 | `/*` |
|         - |  717 | ` * Leave the top-most active frame.` |
|         - |  718 | ` */` |
|   3712618 |  719 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  720 | `{` |
|   3712623 |  721 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   3712623 |  722 | `	if( pCurFrame ){` |
|         - |  723 | `		/* Unlink from the list of active VM frame */` |
|   3712623 |  724 | `		pVm->pFrame = pCurFrame->pParent;` |
|   3712623 |  725 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  726 | `			VmSlot  *aSlot;` |
|         - |  727 | `			sxu32 n;` |
|         - |  728 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|         - |  729 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|         - |  730 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|         - |  731 | `			 * is about to free). */` |
|    793227 |  732 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   2018758 |  733 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   1225536 |  734 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    614068 |  735 | `			}` |
|         - |  736 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    793227 |  737 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   2017854 |  738 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|   1224632 |  739 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|         - |  740 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|         - |  741 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|         - |  742 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|         - |  743 | `					 * down the slot and the reference table took the holders with it, so` |
|         - |  744 | `					 * the returned array came back one element SHORT. The last holder to` |
|         - |  745 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|        24 |  746 | `					continue;` |
|         - |  747 | `				}` |
|         - |  748 | `				/* Unset the local variable */` |
|   1224610 |  749 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    613605 |  750 | `			}` |
|    396826 |  751 | `		}` |
|         - |  752 | `		/* Release internal containers */` |
|   3712623 |  753 | `		SyHashRelease(&pCurFrame->hVar);` |
|   3712623 |  754 | `		SySetRelease(&pCurFrame->sArg);` |
|   3712623 |  755 | `		SySetRelease(&pCurFrame->sLocal);` |
|   3712623 |  756 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  757 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  758 | `		 * containers above — released for every frame, including transparent` |
|         - |  759 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   3712623 |  760 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  761 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   3712623 |  762 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  763 | `		/* Release the whole structure */` |
|   3712623 |  764 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   1856524 |  765 | `	}` |
|   3712623 |  766 | `}` |
|         - |  767 | `/*` |
|         - |  768 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  769 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  770 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  771 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  772 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  773 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  774 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  775 | ` */` |
|         - |  776 | `/*` |
|         - |  777 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  778 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  779 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  780 | ` *` |
|         - |  781 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  782 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  783 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  784 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  785 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  786 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  787 | ` */` |
|  10812799 |  788 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  789 | `{` |
|         - |  790 | `	VmFrame *pFrame;` |
|  30365881 |  791 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|  19553430 |  792 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  793 | `		sxu32 n;` |
|  52190380 |  794 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|  32637303 |  795 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  796 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       351 |  797 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       351 |  798 | `				(void)SySetPop(&pFrame->sLocal);` |
|       351 |  799 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  800 | `			}` |
|  16318477 |  801 | `		}` |
|   9776565 |  802 | `	}` |
|  10812456 |  803 | `	return FALSE;` |
|   5406429 |  804 | `}` |
|       674 |  805 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  806 | `{` |
|         - |  807 | `	VmRefObj *pRef;` |
|       679 |  808 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       679 |  809 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       679 |  810 | `	if( pRef ){` |
|       643 |  811 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       324 |  812 | `	}else{` |
|         - |  813 | `		/* No record yet — a pin on a slot nothing refers to was silently a NO-OP, so the` |
|         - |  814 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|        38 |  815 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  816 | `	}` |
|       679 |  817 | `}` |
|         - |  818 | `/*` |
|         - |  819 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|         - |  820 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|         - |  821 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|         - |  822 | ` */` |
|        46 |  823 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  824 | `{` |
|         - |  825 | `	VmRefObj *pRef;` |
|        47 |  826 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|        47 |  827 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        47 |  828 | `	if( pRef ){` |
|        47 |  829 | `		pRef->nPin++;` |
|        23 |  830 | `	}` |
|        47 |  831 | `}` |
|         - |  832 | `/*` |
|         - |  833 | ` * Give back a counted pin. The slot goes when it was the last holder — without this the` |
|         - |  834 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|         - |  835 | ` * used to be a flag, so nothing could tell one holder from two).` |
|         - |  836 | ` */` |
|        36 |  837 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  838 | `{` |
|        37 |  839 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        37 |  840 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|       ! 0 |  841 | `		return;` |
|         - |  842 | `	}` |
|        37 |  843 | `	pRef->nPin--;` |
|        37 |  844 | `	if( pRef->nPin < 1 ){` |
|        31 |  845 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|        31 |  846 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        15 |  847 | `	}` |
|        19 |  848 | `}` |
|         - |  849 | `/*` |
|         - |  850 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  851 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  852 | ` * should be skipped when looking for the real execution context.` |
|         - |  853 | ` */` |
|  52028867 |  854 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  855 | `{` |
|  65716267 |  856 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  13687400 |  857 | `		pFrame = pFrame->pParent;` |
|         5 |  858 | `	}` |
|  52028872 |  859 | `	return pFrame;` |
|         5 |  860 | `}` |
|         - |  861 | `/*` |
|         - |  862 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  863 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  864 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  865 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  866 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  867 | ` *` |
|         - |  868 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  869 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  870 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  871 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  872 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  873 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  874 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  875 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  876 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  877 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  878 | ` *` |
|         - |  879 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  880 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  881 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  882 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  883 | ` */` |
|   1972720 |  884 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  885 | `{` |
|   1972725 |  886 | `	if( pVm->pResumeFrame == 0 ){` |
|        23 |  887 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  888 | `	}` |
|   1972705 |  889 | `	if( pEntryFrame == 0 ){` |
|         - |  890 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - |  891 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - |  892 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - |  893 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - |  894 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - |  895 | `		 * NULL and the process dies. */` |
|        15 |  896 | `		return FALSE;` |
|         - |  897 | `	}` |
|         - |  898 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  899 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  900 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  901 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  902 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  903 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  904 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  905 | `	 * exec's resume site match and land. */` |
|   1972688 |  906 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1669977 |  907 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1365016 |  908 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  909 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  910 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  911 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  912 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  913 | `		 * never match a real frame. */` |
|    609936 |  914 | `		return FALSE;` |
|         - |  915 | `	}` |
|         - |  916 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  917 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  918 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  919 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  920 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  921 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  922 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  923 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  924 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  925 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  926 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  927 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  928 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  929 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  930 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  931 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  932 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  933 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  934 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  935 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  936 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|   2198626 |  937 | `	while( pVm->pFrame != pEntryFrame` |
|   2403196 |  938 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1567321 |  939 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|    308987 |  940 | `		VmLeaveFrame(&(*pVm));` |
|         5 |  941 | `	}` |
|   1362762 |  942 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|   1362762 |  943 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|         - |  944 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  945 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  946 | `	 * point router must not re-fire it after this resume. */` |
|   1362762 |  947 | `	pVm->nBoundaryRc = 0;` |
|   1362762 |  948 | `	return TRUE;` |
|    986364 |  949 | `}` |
|         - |  950 | `/*` |
|         - |  951 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  952 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  953 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  954 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  955 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  956 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  957 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  958 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  959 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  960 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  961 | ` */` |
|         - |  962 | `/*` |
|         - |  963 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  964 | ` *` |
|         - |  965 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  966 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  967 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  968 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  969 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  970 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  971 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  972 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  973 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  974 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  975 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  976 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  977 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  978 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  979 | ` * activation.` |
|         - |  980 | ` */` |
|   1476469 |  981 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  982 | `{` |
|   1476474 |  983 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1476474 |  984 | `	if( pClone == 0 ){` |
|       ! 0 |  985 | `		return 0;` |
|         - |  986 | `	}` |
|   1476474 |  987 | `	*pClone = *pCompiled;` |
|   1476474 |  988 | `	pClone->pCompiled = pCompiled;` |
|   1476474 |  989 | `	pClone->iFinallyDone = 0;` |
|   1476474 |  990 | `	pClone->iInCatch = 0;` |
|   1476474 |  991 | `	pClone->pInflight = 0;` |
|   1476474 |  992 | `	pClone->pFrame = 0;` |
|   1476474 |  993 | `	return pClone;` |
|    738240 |  994 | `}` |
|   2939982 |  995 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  996 | `{` |
|   2939987 |  997 | `	if( pExc && pExc->pCompiled ){` |
|         - |  998 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  999 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - | 1000 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - | 1001 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1476456 | 1002 | `		if( pExc->pInflight ){` |
|       ! 0 | 1003 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 | 1004 | `			pExc->pInflight = 0;` |
|       ! 0 | 1005 | `		}` |
|   1476456 | 1006 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    738226 | 1007 | `	}` |
|   2939987 | 1008 | `}` |
|         - | 1009 | `/*` |
|         - | 1010 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - | 1011 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - | 1012 | ` */` |
|      7356 | 1013 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 | 1014 | `{` |
|      7361 | 1015 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 | 1016 | `}` |
|         - | 1017 | `/*` |
|         - | 1018 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - | 1019 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - | 1020 | ` * handlers). The set itself is reset by the caller.` |
|         - | 1021 | ` */` |
|    101344 | 1022 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 | 1023 | `{` |
|    101349 | 1024 | `	sxu32 n = SySetUsed(pSet);` |
|    101349 | 1025 | `	if( n > 0 ){` |
|       ! 0 | 1026 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - | 1027 | `		sxu32 i;` |
|       ! 0 | 1028 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 | 1029 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 | 1030 | `		}` |
|       ! 0 | 1031 | `	}` |
|    101349 | 1032 | `}` |
|         - | 1033 | `/*` |
|         - | 1034 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - | 1035 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - | 1036 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - | 1037 | ` * down a try belonging to the caller.` |
|         - | 1038 | ` */` |
|        18 | 1039 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         3 | 1040 | `{` |
|        21 | 1041 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        21 | 1042 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        21 | 1043 | `	if( nBase < nFloor ){` |
|       ! 0 | 1044 | `		nBase = nFloor;` |
|       ! 0 | 1045 | `	}` |
|        21 | 1046 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         3 | 1047 | `}` |
|         - | 1048 | `/*` |
|         - | 1049 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - | 1050 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - | 1051 | ` * only carries the compiled pointer.` |
|         - | 1052 | ` */` |
|        78 | 1053 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 | 1054 | `{` |
|        83 | 1055 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        83 | 1056 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        83 | 1057 | `	while( n > 0 ){` |
|        83 | 1058 | `		n--;` |
|        83 | 1059 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        83 | 1060 | `			return ap[n];` |
|         - | 1061 | `		}` |
|       ! 0 | 1062 | `	}` |
|       ! 0 | 1063 | `	return 0;` |
|        44 | 1064 | `}` |
|   4389531 | 1065 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 | 1066 | `{` |
|         - | 1067 | `	sxu32 nUsed;` |
|   4389536 | 1068 | `	sxi32 rcOut = SXRET_OK;` |
|   4395864 | 1069 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|      6333 | 1070 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      6333 | 1071 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|      6333 | 1072 | `		(void)SySetPop(&pVm->aException);` |
|      6333 | 1073 | `		pExc->pFrame = 0;` |
|         - | 1074 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - | 1075 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - | 1076 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - | 1077 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - | 1078 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|      6333 | 1079 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      6333 | 1080 | `			VmLeaveFrame(&(*pVm));` |
|      3164 | 1081 | `		}` |
|      6365 | 1082 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - | 1083 | `			sxi32 rcF;` |
|        69 | 1084 | `			pExc->iFinallyDone = 1;` |
|        69 | 1085 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        69 | 1086 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        69 | 1087 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 | 1088 | `				return SXERR_ABORT;` |
|         - | 1089 | `			}` |
|        69 | 1090 | `			if( rcF == PH7_EXCEPTION ){` |
|         - | 1091 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - | 1092 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - | 1093 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 | 1094 | `				rcOut = PH7_EXCEPTION;` |
|         2 | 1095 | `			}` |
|        37 | 1096 | `		}else{` |
|      6268 | 1097 | `			VmExcRelease(&(*pVm),pExc);` |
|         - | 1098 | `		}` |
|         5 | 1099 | `	}` |
|   4389536 | 1100 | `	return rcOut;` |
|   2194982 | 1101 | `}` |
|         - | 1102 | `/*` |
|         - | 1103 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - | 1104 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - | 1105 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - | 1106 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - | 1107 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - | 1108 | ` */` |
|   3778903 | 1109 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 | 1110 | `{` |
|   3778908 | 1111 | `	pFrame->bHasRet = 0;` |
|   3778908 | 1112 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   3778908 | 1113 | `	pFrame->nCatchJmpPc = 0;` |
|   3778908 | 1114 | `}` |
|         - | 1115 | `/*` |
|         - | 1116 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - | 1117 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - | 1118 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - | 1119 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - | 1120 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - | 1121 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - | 1122 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - | 1123 | ` */` |
|     23944 | 1124 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 | 1125 | `{` |
|     23949 | 1126 | `	if( pResult ){` |
|     23949 | 1127 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     11972 | 1128 | `	}` |
|     23949 | 1129 | `	VmClearFramePending(pEntryFrame);` |
|     23953 | 1130 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|         5 | 1131 | `		VmLeaveFrame(&(*pVm));` |
|         1 | 1132 | `	}` |
|     23949 | 1133 | `}` |
|         - | 1134 | `/*` |
|         - | 1135 | ` * Compare two functions signature and return the comparison result.` |
|         - | 1136 | ` */` |
|      1186 | 1137 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         2 | 1138 | `{` |
|      1188 | 1139 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      1188 | 1140 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      1188 | 1141 | `	const char *zSin = pSecond->zString;` |
|      1188 | 1142 | `	const char *zFin = pFirst->zString;` |
|      1188 | 1143 | `	const char *zPtr = zFin;` |
|       593 | 1144 | `	for(;;){` |
|      1188 | 1145 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|       595 | 1146 | `			break;` |
|         - | 1147 | `		}` |
|       ! 0 | 1148 | `		if( zFin[0] != zSin[0] ){` |
|         - | 1149 | `			/* mismatch */` |
|       ! 0 | 1150 | `			break;` |
|         - | 1151 | `		}` |
|       ! 0 | 1152 | `		zFin++;` |
|       ! 0 | 1153 | `		zSin++;` |
|       ! 0 | 1154 | `	}` |
|      1188 | 1155 | `	return (int)(zFin-zPtr);` |
|         2 | 1156 | `}` |
|         - | 1157 | `/*` |
|         - | 1158 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1159 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1160 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1161 | ` * Refer to the official documentation for more information.` |
|         - | 1162 | ` */` |
|       284 | 1163 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1164 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1165 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1166 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1167 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1168 | `	)` |
|         5 | 1169 | `{` |
|         - | 1170 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1171 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1172 | `	ph7_vm_func *pLink;` |
|         - | 1173 | `	SyString sArgSig;` |
|         - | 1174 | `	SyBlob sSig;` |
|         - | 1175 |  |
|       289 | 1176 | `	pLink = pList;` |
|       289 | 1177 | `	i = 0;` |
|         - | 1178 | `	/* Put functions expecting the same number of passed arguments */` |
|      1891 | 1179 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      1823 | 1180 | `		if( pLink == 0 ){` |
|       220 | 1181 | `			break;` |
|         - | 1182 | `		}` |
|      1607 | 1183 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1184 | `			/* Candidate for overloading */` |
|      1607 | 1185 | `			apSet[i++] = pLink;` |
|       801 | 1186 | `		}` |
|         - | 1187 | `		/* Point to the next entry */` |
|      1607 | 1188 | `		pLink = pLink->pNextName;` |
|         5 | 1189 | `	}` |
|       289 | 1190 | `	if( i < 1 ){` |
|         - | 1191 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1192 | `		return pList;` |
|         - | 1193 | `	}` |
|       289 | 1194 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1195 | `		/* Return the only candidate */` |
|        61 | 1196 | `		return apSet[0];` |
|         - | 1197 | `	}` |
|         - | 1198 | `	/* Calculate function signature */` |
|       230 | 1199 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|       458 | 1200 | `	for( j = 0 ; j < nArg ; j++ ){` |
|       230 | 1201 | `		int c = 'n'; /* null */` |
|       230 | 1202 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1203 | `			/* Hashmap */` |
|       ! 0 | 1204 | `			c = 'h';` |
|       230 | 1205 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1206 | `			/* bool */` |
|        85 | 1207 | `			c = 'b';` |
|       188 | 1208 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1209 | `			/* int */` |
|        48 | 1210 | `			c = 'i';` |
|       122 | 1211 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1212 | `			/* String */` |
|        87 | 1213 | `			c = 's';` |
|        56 | 1214 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1215 | `			/* Float */` |
|        11 | 1216 | `			c = 'f';` |
|         8 | 1217 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1218 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1219 | `			int marker = 'o';` |
|       ! 0 | 1220 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1221 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1222 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1223 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1224 | `			c = -1;` |
|       ! 0 | 1225 | `		}` |
|       230 | 1226 | `		if( c > 0 ){` |
|       230 | 1227 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       114 | 1228 | `		}` |
|       116 | 1229 | `	}` |
|       230 | 1230 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|       230 | 1231 | `	iTarget = 0;` |
|       230 | 1232 | `	iMax = -1;` |
|         - | 1233 | `	/* Select the appropriate function */` |
|      1416 | 1234 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1235 | `		/* Compare the two signatures */` |
|      1188 | 1236 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      1188 | 1237 | `		if( iCur > iMax ){` |
|       230 | 1238 | `			iMax = iCur;` |
|       230 | 1239 | `			iTarget = j;` |
|       114 | 1240 | `		}` |
|       595 | 1241 | `	}` |
|       230 | 1242 | `	SyBlobRelease(&sSig);` |
|         - | 1243 | `	/* Appropriate function for the current call context */` |
|       230 | 1244 | `	return apSet[iTarget];` |
|       147 | 1245 | `}` |
|         - | 1246 | `/* Forward declaration */` |
|         - | 1247 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1248 | `/*` |
|         - | 1249 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|         - | 1250 | ` * safely across a pool reallocation.` |
|         - | 1251 | ` *` |
|         - | 1252 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1253 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|         - | 1254 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|         - | 1255 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|         - | 1256 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|         - | 1257 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|         - | 1258 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|         - | 1259 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|         - | 1260 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|         - | 1261 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|         - | 1262 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|         - | 1263 | ` */` |
|      3168 | 1264 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1265 | `{` |
|         - | 1266 | `	ph7_value sVal;` |
|      3173 | 1267 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1268 | `	sxi32 rc;` |
|      3173 | 1269 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|      3173 | 1270 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1271 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|      3173 | 1272 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      3173 | 1273 | `	if( *ppMemObj ){` |
|      3173 | 1274 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|      1584 | 1275 | `	}` |
|      3173 | 1276 | `	PH7_MemObjRelease(&sVal);` |
|      3173 | 1277 | `	return rc;` |
|         5 | 1278 | `}` |
|         - | 1279 | `/*` |
|         - | 1280 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|         - | 1281 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|         - | 1282 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|         - | 1283 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|         - | 1284 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|         - | 1285 | ` * C caller ever regains control.` |
|         - | 1286 | ` */` |
|         - | 1287 | `typedef struct VmMuteState {` |
|         - | 1288 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|         - | 1289 | `	sxu32 nSaved;` |
|         - | 1290 | `	sxi32 iSaveStatus;` |
|         - | 1291 | `	sxi32 iSaveBoundary;` |
|         - | 1292 | `	VmFrame *pSaveResume;` |
|         - | 1293 | `	ph7_class_attr *pSaveCycleAttr;` |
|         - | 1294 | `	ph7_class *pSaveCycleClass;` |
|         - | 1295 | `} VmMuteState;` |
|       472 | 1296 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|         5 | 1297 | `{` |
|       477 | 1298 | `	pSave->apSaved = 0;` |
|       477 | 1299 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|       477 | 1300 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|       477 | 1301 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|       477 | 1302 | `	pSave->pSaveResume = pVm->pResumeFrame;` |
|       477 | 1303 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|       477 | 1304 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|       477 | 1305 | `	if( pSave->nSaved > 0 ){` |
|       406 | 1306 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       202 | 1307 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|       204 | 1308 | `		if( pSave->apSaved ){` |
|       305 | 1309 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|       202 | 1310 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|       204 | 1311 | `			SySetReset(&pVm->aException);` |
|       101 | 1312 | `		}` |
|       101 | 1313 | `	}` |
|       477 | 1314 | `	pVm->nMuteThrow++;` |
|       477 | 1315 | `}` |
|         - | 1316 | `/*` |
|         - | 1317 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|         - | 1318 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|         - | 1319 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|         - | 1320 | ` * TRUE when a throw was actually swallowed.` |
|         - | 1321 | ` */` |
|       472 | 1322 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|         5 | 1323 | `{` |
|         - | 1324 | `	VmFrame *pFrame;` |
|       477 | 1325 | `	pVm->nMuteThrow--;` |
|         - | 1326 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|         - | 1327 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|         - | 1328 | `	 * activation behind: release whatever is there whether or not anything was` |
|         - | 1329 | `	 * hidden. */` |
|       477 | 1330 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|       477 | 1331 | `	SySetReset(&pVm->aException);` |
|       477 | 1332 | `	if( pSave->apSaved ){` |
|         - | 1333 | `		sxu32 k;` |
|       406 | 1334 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|       204 | 1335 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|       103 | 1336 | `		}` |
|       204 | 1337 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|       204 | 1338 | `		pSave->apSaved = 0;` |
|       101 | 1339 | `	}` |
|       477 | 1340 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|       427 | 1341 | `		return FALSE;` |
|         - | 1342 | `	}` |
|        53 | 1343 | `	pFrame = pVm->pFrame;` |
|        53 | 1344 | `	if( pFrame ){` |
|        53 | 1345 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        53 | 1346 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        25 | 1347 | `	}` |
|        53 | 1348 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|        53 | 1349 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|        53 | 1350 | `	pVm->pResumeFrame = pSave->pSaveResume;` |
|        53 | 1351 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|        53 | 1352 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|        53 | 1353 | `	return TRUE;` |
|       241 | 1354 | `}` |
|         - | 1355 | `/*` |
|         - | 1356 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|         - | 1357 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|         - | 1358 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|         - | 1359 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|         - | 1360 | ` *` |
|         - | 1361 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|         - | 1362 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|         - | 1363 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|         - | 1364 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|         - | 1365 | ` */` |
|       198 | 1366 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|         - | 1367 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 1368 | `	ph7_class_instance *pThis,   /* Receiver */` |
|         - | 1369 | `	ph7_class_method *pMethod,   /* Method to run */` |
|         - | 1370 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|         - | 1371 | `	int nArg,                    /* Argument count */` |
|         - | 1372 | `	ph7_value **apArg,           /* Arguments */` |
|         - | 1373 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|         - | 1374 | `	)` |
|         1 | 1375 | `{` |
|         - | 1376 | `	VmMuteState sSave;` |
|         - | 1377 | `	sxi32 rc;` |
|       199 | 1378 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       199 | 1379 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       199 | 1380 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         7 | 1381 | `		if( pbThrew ){` |
|         7 | 1382 | `			*pbThrew = TRUE;` |
|         3 | 1383 | `		}` |
|         7 | 1384 | `		return SXRET_OK;` |
|         - | 1385 | `	}` |
|       193 | 1386 | `	if( pbThrew ){` |
|       193 | 1387 | `		*pbThrew = FALSE;` |
|        96 | 1388 | `	}` |
|       193 | 1389 | `	return rc;` |
|       100 | 1390 | `}` |
|         - | 1391 | `/*` |
|         - | 1392 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - | 1393 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - | 1394 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - | 1395 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - | 1396 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - | 1397 | ` *` |
|         - | 1398 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - | 1399 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - | 1400 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - | 1401 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - | 1402 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - | 1403 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - | 1404 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - | 1405 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - | 1406 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - | 1407 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 1408 | ` */` |
|       274 | 1409 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 | 1410 | `{` |
|         - | 1411 | `	VmMuteState sSave;` |
|         - | 1412 | `	sxi32 rc;` |
|       279 | 1413 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       279 | 1414 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|       279 | 1415 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|         - | 1416 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - | 1417 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - | 1418 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - | 1419 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - | 1420 | `		 * records the cycle again and raises it there. */` |
|       ! 0 | 1421 | `		rc = PH7_EXCEPTION;` |
|       ! 0 | 1422 | `	}` |
|         - | 1423 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|         - | 1424 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|         - | 1425 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|         - | 1426 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|         - | 1427 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|         - | 1428 | `	 * detects the cycle again. */` |
|       279 | 1429 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|       279 | 1430 | `	return rc;` |
|         5 | 1431 | `}` |
|         - | 1432 | `/*` |
|         - | 1433 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1434 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1435 | ` */` |
|         - | 1436 | `/*` |
|         - | 1437 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1438 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1439 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1440 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1441 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1442 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1443 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1444 | ` */` |
|   2924552 | 1445 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1446 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1447 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1448 | `	)` |
|         5 | 1449 | `{` |
|         - | 1450 | `	ph7_class_attr *pAttr;` |
|         - | 1451 | `	SyHashEntry *pEntry;` |
|         - | 1452 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1453 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1454 | `	 * over both tables. */` |
|         - | 1455 | `	SyHash *apMount[2];` |
|         - | 1456 | `	int iMount;` |
|   2924557 | 1457 | `	apMount[0] = &pClass->hAttr;` |
|   2924557 | 1458 | `	apMount[1] = &pClass->hConst;` |
|   8773655 | 1459 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1460 | `	/* Reset the loop cursor */` |
|   5849109 | 1461 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1462 | `	/* Process only static and constant attribute */` |
|  19481519 | 1463 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1464 | `		/* Extract the current attribute */` |
|  13632421 | 1465 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  13632416 | 1466 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   9541064 | 1467 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   2725427 | 1468 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1469 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1470 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1471 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1472 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1473 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1474 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1475 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1476 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1477 | `			 * %s as value for class constant" fatal without any access). */` |
|   5448555 | 1478 | `			continue;` |
|         - | 1479 | `		}` |
|   8183871 | 1480 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1481 | `			ph7_value *pMemObj;` |
|      1441 | 1482 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1483 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1484 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1485 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1486 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - | 1487 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - | 1488 | `				 * access / instantiation throws like php's. */` |
|      1135 | 1489 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        27 | 1490 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - | 1491 | `						/* Its default threw at the other class's mount and is` |
|         - | 1492 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - | 1493 | `						 * both static tables). */` |
|         3 | 1494 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        26 | 1495 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        11 | 1496 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|         6 | 1497 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|         8 | 1498 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1499 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|         1 | 1500 | `						}` |
|         3 | 1501 | `					}` |
|        12 | 1502 | `				}` |
|      1138 | 1503 | `				continue;` |
|         - | 1504 | `			}` |
|         - | 1505 | `			/* Reserve a memory object for this constant/static attribute */` |
|       309 | 1506 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       309 | 1507 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1508 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 1509 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 1510 | `					&pClass->sName,&pAttr->sName` |
|         - | 1511 | `					);` |
|       ! 0 | 1512 | `				return SXERR_MEM;` |
|         - | 1513 | `			}` |
|       309 | 1514 | `			if( pAttr->pNativeValue ){` |
|         - | 1515 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 | 1516 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|       309 | 1517 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1518 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1519 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 1520 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|       279 | 1521 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       279 | 1522 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - | 1523 | `				sxi32 rcExec;` |
|       279 | 1524 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       279 | 1525 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|       279 | 1526 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|       279 | 1527 | `				pVm->nConstEvalDepth++;` |
|         - | 1528 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - | 1529 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - | 1530 | `				 * not something it can see. What reaches this line is a static` |
|         - | 1531 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - | 1532 | `				 * which php does validate here, but only when the initializer` |
|         - | 1533 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - | 1534 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - | 1535 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|       279 | 1536 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|       279 | 1537 | `				pVm->nConstEvalDepth--;` |
|       279 | 1538 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       279 | 1539 | `				pVm->pConstEvalClass = pSaveCtx;` |
|       279 | 1540 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1541 | `					/* php has not reached this initializer: defer it whole to the` |
|         - | 1542 | `					 * first USE, where the throw is raised at the access site and is` |
|         - | 1543 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - | 1544 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - | 1545 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - | 1546 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - | 1547 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - | 1548 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - | 1549 | `					 * unset nIdx. */` |
|        46 | 1550 | `					if( bStaticProp ){` |
|        40 | 1551 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        40 | 1552 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        24 | 1553 | `					}else{` |
|         - | 1554 | `						VmSlot sSlot;` |
|         - | 1555 | `						/* Release before recycling: PH7_ReserveMemObj re-inits a` |
|         - | 1556 | `						 * reused slot without releasing it, and a muted eval that` |
|         - | 1557 | `						 * only recorded a CYCLE still left its value here. */` |
|         8 | 1558 | `						sSlot.nIdx = pMemObj->nIdx;` |
|         8 | 1559 | `						sSlot.pUserData = 0;` |
|         8 | 1560 | `						PH7_MemObjRelease(pMemObj);` |
|         8 | 1561 | `						SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|         8 | 1562 | `						continue;` |
|         - | 1563 | `					}` |
|       251 | 1564 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|       120 | 1565 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1566 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 1567 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - | 1568 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        42 | 1569 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        42 | 1570 | `					if( rcType != SXRET_OK ){` |
|         9 | 1571 | `						return rcType;` |
|         - | 1572 | `					}` |
|        16 | 1573 | `				}` |
|       131 | 1574 | `			}` |
|         - | 1575 | `			/* Record attribute index */` |
|       297 | 1576 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 1577 | `			/* Install static attribute in the reference table */` |
|       297 | 1578 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1579 | `			/* If this is a typed static property, register the slot so the` |
|         - | 1580 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 1581 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 1582 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 1583 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 1584 | `			 * already enforced above, so they need no store-time slot. */` |
|       292 | 1585 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       206 | 1586 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        83 | 1587 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        83 | 1588 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 1589 | `					return SXERR_MEM;` |
|         - | 1590 | `				}` |
|        83 | 1591 | `				pVmAttrS->pAttr = pAttr;` |
|        83 | 1592 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        83 | 1593 | `				pVmAttrS->iState = 0;` |
|        83 | 1594 | `				pVmAttrS->pOwner = pClass;` |
|        83 | 1595 | `				pVmAttrS->pInst = 0;   /* the class's own slot: no instance behind it */` |
|         - | 1596 | `				/* Static typed property with no default starts uninitialized` |
|         - | 1597 | `				 * (constants are already excluded by the enclosing condition). */` |
|        83 | 1598 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        18 | 1599 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        75 | 1600 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - | 1601 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 1602 | `					 * typed static default LAZILY at the first static-property` |
|         - | 1603 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 1604 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 1605 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 1606 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 1607 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 1608 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - | 1609 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - | 1610 | `					 * the materializer checks it after the re-run. */` |
|        68 | 1611 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        25 | 1612 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        25 | 1613 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        11 | 1614 | `					}` |
|        32 | 1615 | `				}` |
|        83 | 1616 | `				if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 1617 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 1618 | `					return SXERR_MEM;` |
|         - | 1619 | `				}` |
|        39 | 1620 | `			}` |
|       146 | 1621 | `		}` |
|         5 | 1622 | `	}` |
|   2924554 | 1623 | `	} /* for iMount */` |
|   2924551 | 1624 | `	return SXRET_OK;` |
|   1462281 | 1625 | `}` |
|         - | 1626 | `/*` |
|         - | 1627 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - | 1628 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - | 1629 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - | 1630 | ` */` |
|   2922184 | 1631 | `static sxi32 VmMountUserClassMethods(` |
|         - | 1632 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1633 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - | 1634 | `	)` |
|         5 | 1635 | `{` |
|         - | 1636 | `	ph7_class_method *pMeth;` |
|         - | 1637 | `	SyHashEntry *pEntry;` |
|         - | 1638 | `	sxi32 rc;` |
|         - | 1639 | `	/* Install class methods */` |
|   2922189 | 1640 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 1641 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 1642 | `		 */` |
|   1077769 | 1643 | `		return SXRET_OK;` |
|         - | 1644 | `	}` |
|         - | 1645 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 1646 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 1647 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 1648 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 1649 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 1650 | `	/* Install the methods now */` |
|   1844425 | 1651 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  30592989 | 1652 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  27826359 | 1653 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  27826359 | 1654 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  27783501 | 1655 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  27783501 | 1656 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1657 | `				return rc;` |
|         - | 1658 | `			}` |
|  13891748 | 1659 | `		}` |
|         5 | 1660 | `	}` |
|         - | 1661 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   1844425 | 1662 | `	pClass->bMounted = TRUE;` |
|   1844425 | 1663 | `	return SXRET_OK;` |
|   1461097 | 1664 | `}` |
|   1943136 | 1665 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 1666 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1667 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 1668 | `	)` |
|         5 | 1669 | `{` |
|         - | 1670 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - | 1671 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - | 1672 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - | 1673 | `	 * initializer that throws finds the exception classes ready. */` |
|   1943141 | 1674 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   1943141 | 1675 | `	if( rc != SXRET_OK ){` |
|         3 | 1676 | `		return rc;` |
|         - | 1677 | `	}` |
|   1943139 | 1678 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|    971573 | 1679 | `}` |
|         - | 1680 | `/*` |
|         - | 1681 | ` * Allocate a private frame for attributes of the given` |
|         - | 1682 | ` * class instance (Object in the PHP jargon).` |
|         - | 1683 | ` */` |
|   1606267 | 1684 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 1685 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 1686 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 1687 | `	)` |
|         5 | 1688 | `{` |
|   1606272 | 1689 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 1690 | `	ph7_class_attr *pAttr;` |
|         - | 1691 | `	SyHashEntry *pEntry;` |
|         - | 1692 | `	sxi32 rc;` |
|   1606272 | 1693 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 1694 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 1695 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 1696 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1606272 | 1697 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  12002677 | 1698 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 1699 | `		VmClassAttr *pVmAttr;` |
|         - | 1700 | `		/* Extract the current attribute */` |
|  10396410 | 1701 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  10396410 | 1702 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY ){` |
|         - | 1703 | `			/* A property php's own object does not HOLD until its constructor` |
|         - | 1704 | `			 * fills it: no slot, no hAttr entry, nothing for a read, an isset()` |
|         - | 1705 | `			 * or a property walk to find. PH7_NativeMaterializeLazy installs the` |
|         - | 1706 | `			 * whole set the first time a C body writes one. */` |
|      7617 | 1707 | `			continue;` |
|         - | 1708 | `		}` |
|  10388796 | 1709 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|  10388796 | 1710 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 1711 | `			return SXERR_MEM;` |
|         - | 1712 | `		}` |
|  10388796 | 1713 | `		pVmAttr->pAttr = pAttr;` |
|  10388796 | 1714 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 1715 | `			ph7_value *pMemObj;` |
|         - | 1716 | `			/* Reserve a memory object for this attribute */` |
|  10388622 | 1717 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|  10388622 | 1718 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1719 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1720 | `				return SXERR_MEM;` |
|         - | 1721 | `			}` |
|  10388622 | 1722 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|  10388622 | 1723 | `			pVmAttr->iState = 0;` |
|  10388622 | 1724 | `			pVmAttr->pOwner = pClass;` |
|  10388622 | 1725 | `			pVmAttr->pInst = pObj;` |
|  10388622 | 1726 | `			if( pAttr->pNativeValue ){` |
|         - | 1727 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - | 1728 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - | 1729 | `				 * literal cannot throw and the builder states the type itself. */` |
|   9915271 | 1730 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   5430986 | 1731 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1732 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1733 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 1734 | `				 * against the declaring class (no method frame here). */` |
|      2467 | 1735 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1736 | `				sxi32 rcExec;` |
|      2467 | 1737 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      2467 | 1738 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      2467 | 1739 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      2467 | 1740 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1741 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 1742 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 1743 | `					 * also type-check the leftover value (a spurious second` |
|         - | 1744 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 1745 | `					 * nothing further for the remaining attributes.` |
|         - | 1746 | `					 *` |
|         - | 1747 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - | 1748 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - | 1749 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - | 1750 | `					 * and the throw came back as a status nobody read: this` |
|         - | 1751 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - | 1752 | `					 * and the whole statement RESUMED after the catch — php` |
|         - | 1753 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - | 1754 | `					 * existing construction-aborted route. */` |
|        24 | 1755 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        24 | 1756 | `					bDefThrew = 1;` |
|      2456 | 1757 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1758 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 1759 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 1760 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 1761 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 1762 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 1763 | `					 * aborts construction) / the fetch-point router. */` |
|       367 | 1764 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       367 | 1765 | `					if( rcDef != SXRET_OK ){` |
|        13 | 1766 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 1767 | `						bDefThrew = 1;` |
|         6 | 1768 | `					}` |
|       186 | 1769 | `				}` |
|    472125 | 1770 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 1771 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 1772 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|    469730 | 1773 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|    234862 | 1774 | `			}` |
|  10388622 | 1775 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|  10388622 | 1776 | `			if( rc != SXRET_OK ){` |
|         - | 1777 | `				VmSlot sSlot;` |
|         - | 1778 | `				/* Restore memory object */` |
|       ! 0 | 1779 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1780 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1781 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1782 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1783 | `				return SXERR_MEM;` |
|         - | 1784 | `			}` |
|         - | 1785 | `			/* Install attribute in the reference table */` |
|  10388622 | 1786 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1787 | `			/* Register the slot with the store filter -- a declared TYPE to` |
|         - | 1788 | `			 * enforce, a native class's write handler, or both. On failure roll` |
|         - | 1789 | `			 * back the just-installed hAttr entry and the reserved memobj so the` |
|         - | 1790 | `			 * caller sees a consistent instance. */` |
|  10388622 | 1791 | `			rc = PH7_VmStoreFilterRegister(&(*pVm),pVmAttr);` |
|  10388622 | 1792 | `			if( rc != SXRET_OK ){` |
|         - | 1793 | `				VmSlot sSlot;` |
|       ! 0 | 1794 | `				SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1795 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1796 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1797 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1798 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1799 | `				return SXERR_MEM;` |
|         - | 1800 | `			}` |
|   5194310 | 1801 | `		}else{` |
|         - | 1802 | `			/* Install static/constant attribute */` |
|       179 | 1803 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       179 | 1804 | `			pVmAttr->iState = 0;` |
|       179 | 1805 | `			pVmAttr->pOwner = pClass;` |
|       179 | 1806 | `			pVmAttr->pInst = 0;   /* a static slot belongs to the class, not to this object */` |
|       179 | 1807 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       179 | 1808 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1809 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1810 | `				return SXERR_MEM;` |
|         - | 1811 | `			}` |
|         - | 1812 | `		}` |
|         5 | 1813 | `	}` |
|   1606272 | 1814 | `	return SXRET_OK;` |
|    803138 | 1815 | `}` |
|         - | 1816 | `/*` |
|         - | 1817 | ` * Whether [pClass] permits runtime-created (dynamic) properties: stdClass, and` |
|         - | 1818 | `` * any class php's own `#[AllowDynamicProperties]` opts in (the attribute is`` |
|         - | 1819 | ` * inherited, so the ancestry is walked -- VmClassHasAttributeNamed does that).` |
|         - | 1820 | ` *` |
|         - | 1821 | ` * The attribute half used to be spelled out at each caller, and one of the three` |
|         - | 1822 | ` * did not have it: the by-REFERENCE binder (VmBindPropByRef) asked this alone, so` |
|         - | 1823 | `` * an opted-in class refused `f($o->undeclared)` with §10's `Cannot create dynamic`` |
|         - | 1824 | `` * property` on a write php performs -- while `$o->undeclared = 1` next to it`` |
|         - | 1825 | ` * worked. One decision, one site.` |
|         - | 1826 | ` */` |
|       230 | 1827 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1828 | `{` |
|       235 | 1829 | `	if( pVm->pStdClass != 0 && pClass == pVm->pStdClass ){` |
|       179 | 1830 | `		return TRUE;` |
|         - | 1831 | `	}` |
|        60 | 1832 | `	return VmClassHasAttributeNamed(pClass,"AllowDynamicProperties",` |
|         - | 1833 | `		sizeof("AllowDynamicProperties")-1);` |
|       120 | 1834 | `}` |
|         - | 1835 | `/*` |
|         - | 1836 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 1837 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 1838 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 1839 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 1840 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 1841 | ` */` |
|        56 | 1842 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         4 | 1843 | `{` |
|       100 | 1844 | `	while( pClass ){` |
|        62 | 1845 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 1846 | `		sxu32 n;` |
|        62 | 1847 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|        18 | 1848 | `			if( aAttr[n].sName.nByte == nName` |
|        20 | 1849 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|        20 | 1850 | `				return TRUE;` |
|         - | 1851 | `			}` |
|       ! 0 | 1852 | `		}` |
|        44 | 1853 | `		pClass = pClass->pBase;` |
|         4 | 1854 | `	}` |
|        42 | 1855 | `	return FALSE;` |
|        32 | 1856 | `}` |
|         - | 1857 | `/*` |
|         - | 1858 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|         - | 1859 | ` * access or method call on such an instance is php's incomplete-object` |
|         - | 1860 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|         - | 1861 | ` * (array), get_object_vars) read its attribute table freely.` |
|         - | 1862 | ` */` |
|    262612 | 1863 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1864 | `{` |
|    262617 | 1865 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|         5 | 1866 | `}` |
|         - | 1867 | `/*` |
|         - | 1868 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|         - | 1869 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|         - | 1870 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|         - | 1871 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|         - | 1872 | ` * had one says "unknown", like php.` |
|         - | 1873 | ` */` |
|        44 | 1874 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|         1 | 1875 | `{` |
|        45 | 1876 | `	const char *zName = "unknown";` |
|        45 | 1877 | `	sxu32 nName = sizeof("unknown")-1;` |
|        45 | 1878 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|         - | 1879 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|        45 | 1880 | `	if( pEntry ){` |
|        45 | 1881 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        45 | 1882 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|        45 | 1883 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|        45 | 1884 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|        45 | 1885 | `			nName = SyBlobLength(&pVal->sBlob);` |
|        22 | 1886 | `		}` |
|        22 | 1887 | `	}` |
|        67 | 1888 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|         - | 1889 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|         - | 1890 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|        22 | 1891 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|        45 | 1892 | `}` |
|         - | 1893 | `/*` |
|         - | 1894 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|         - | 1895 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|         - | 1896 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|         - | 1897 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|         - | 1898 | ` * (property_exists() passes its name in pFuncName).` |
|         - | 1899 | ` */` |
|        16 | 1900 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|         1 | 1901 | `{` |
|         - | 1902 | `	SyBlob sMsg;` |
|        17 | 1903 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        17 | 1904 | `	if( pFuncName ){` |
|       ! 0 | 1905 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|       ! 0 | 1906 | `	}else{` |
|        17 | 1907 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|        17 | 1908 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|        17 | 1909 | `		if( pFunc == 0 ){` |
|       ! 0 | 1910 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|       ! 0 | 1911 | `		}else{` |
|        17 | 1912 | `			const char *zDisp = 0;` |
|         - | 1913 | `			int nDisp;` |
|        17 | 1914 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 | 1915 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         3 | 1916 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|         3 | 1917 | `				SyBlobAppend(&sMsg,"::",2);` |
|         1 | 1918 | `			}` |
|        17 | 1919 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|        17 | 1920 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|         - | 1921 | `		}` |
|         - | 1922 | `	}` |
|        17 | 1923 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|        17 | 1924 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|        25 | 1925 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|        16 | 1926 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        17 | 1927 | `	SyBlobRelease(&sMsg);` |
|        17 | 1928 | `}` |
|         - | 1929 | `/*` |
|         - | 1930 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 1931 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 1932 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 1933 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 1934 | ` *` |
|         - | 1935 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 1936 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 1937 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 1938 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 1939 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 1940 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 1941 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 1942 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 1943 | ` */` |
|       520 | 1944 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         5 | 1945 | `{` |
|         - | 1946 | `	ph7_class_attr *pAttr;` |
|       525 | 1947 | `	VmClassAttr *pVmAttr = 0;` |
|       525 | 1948 | `	ph7_value *pMemObj = 0;` |
|         - | 1949 | `	char *zCopy;` |
|         - | 1950 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       525 | 1951 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       525 | 1952 | `	if( pAttr == 0 ){` |
|       ! 0 | 1953 | `		return 0;` |
|         - | 1954 | `	}` |
|       525 | 1955 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       525 | 1956 | `	zCopy = (char *)&pAttr[1];` |
|       525 | 1957 | `	if( nName > 0 ){` |
|       517 | 1958 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       256 | 1959 | `	}` |
|       525 | 1960 | `	zCopy[nName] = 0;` |
|       525 | 1961 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       525 | 1962 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       525 | 1963 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       525 | 1964 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 1965 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 1966 | `	 * value, never a union. */` |
|       525 | 1967 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       525 | 1968 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1969 | `		goto fail_attr;` |
|         - | 1970 | `	}` |
|       525 | 1971 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       525 | 1972 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1973 | `		goto fail_vmattr;` |
|         - | 1974 | `	}` |
|       525 | 1975 | `	pVmAttr->pAttr = pAttr;` |
|       525 | 1976 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       525 | 1977 | `	pVmAttr->iState = 0;` |
|       525 | 1978 | `	pVmAttr->pOwner = pThis->pClass;` |
|       525 | 1979 | `	pVmAttr->pInst = pThis;` |
|         - | 1980 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 1981 | `	 * property-creation order, matching PHP. */` |
|       525 | 1982 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 1983 | `		goto fail_slot;` |
|         - | 1984 | `	}` |
|         - | 1985 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       525 | 1986 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|       525 | 1987 | `	if( ppAttr ){` |
|       187 | 1988 | `		*ppAttr = pVmAttr;` |
|        91 | 1989 | `	}` |
|       525 | 1990 | `	return pMemObj;` |
|       ! 0 | 1991 | `fail_slot:` |
|         - | 1992 | `	{` |
|         - | 1993 | `		VmSlot sSlot;` |
|       ! 0 | 1994 | `		sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1995 | `		sSlot.pUserData = 0;` |
|       ! 0 | 1996 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1997 | `	}` |
|       ! 0 | 1998 | `fail_vmattr:` |
|       ! 0 | 1999 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2000 | `fail_attr:` |
|       ! 0 | 2001 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 2002 | `	return 0;` |
|       265 | 2003 | `}` |
|         - | 2004 | `/*` |
|         - | 2005 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 2006 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 2007 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 2008 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 2009 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 2010 | ` */` |
|      5778 | 2011 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         3 | 2012 | `{` |
|         - | 2013 | `	VmClassAttr *pVmAttr;` |
|         - | 2014 | `	ph7_value *pMemObj;` |
|      5781 | 2015 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      5781 | 2016 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 2017 | `		return;` |
|         - | 2018 | `	}` |
|      5781 | 2019 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      5781 | 2020 | `	if( pMemObj == 0 ){` |
|       ! 0 | 2021 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2022 | `		return;` |
|         - | 2023 | `	}` |
|      5781 | 2024 | `	pVmAttr->pAttr = pAttr;` |
|      5781 | 2025 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|      5781 | 2026 | `	pVmAttr->iState = 0;` |
|      5781 | 2027 | `	pVmAttr->pOwner = pThis->pClass;` |
|      5781 | 2028 | `	pVmAttr->pInst = pThis;` |
|         - | 2029 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 2030 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 2031 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 2032 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 2033 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|      5781 | 2034 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      1107 | 2035 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       553 | 2036 | `	}` |
|         - | 2037 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 2038 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 2039 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 2040 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|      5781 | 2041 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|         - | 2042 | `		VmSlot sSlot;` |
|       ! 0 | 2043 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 2044 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 2045 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2046 | `		return;` |
|         - | 2047 | `	}` |
|      5781 | 2048 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      5781 | 2049 | `	if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttr) != SXRET_OK ){` |
|         - | 2050 | `		VmSlot sSlot;` |
|       ! 0 | 2051 | `		SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 2052 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 2053 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 2054 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2055 | `		return;` |
|         - | 2056 | `	}` |
|      5781 | 2057 | `	if( ppAttr ){` |
|      5781 | 2058 | `		*ppAttr = pVmAttr;` |
|      2889 | 2059 | `	}` |
|      2892 | 2060 | `}` |
|         - | 2061 | `/* Forward declaration */` |
|         - | 2062 | `/*` |
|         - | 2063 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 2064 | ` */` |
|         - | 2065 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 2066 | `/*` |
|         - | 2067 | ` * Reserve a constant memory object.` |
|         - | 2068 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2069 | ` */` |
|   1478528 | 2070 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2071 | `{` |
|         - | 2072 | `	ph7_value *pObj;` |
|         - | 2073 | `	sxi32 rc;` |
|   1478533 | 2074 | `	if( pIndex ){` |
|         - | 2075 | `		/* Object index in the object table */` |
|   1461313 | 2076 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    730654 | 2077 | `	}` |
|         - | 2078 | `	/* Reserve a slot for the new object */` |
|   1478533 | 2079 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   1478533 | 2080 | `	if( rc != SXRET_OK ){` |
|         - | 2081 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2082 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2083 | `		 */` |
|       ! 0 | 2084 | `		return 0;` |
|         - | 2085 | `	}` |
|   1478533 | 2086 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   1478533 | 2087 | `	return pObj;` |
|    739269 | 2088 | `}` |
|         - | 2089 | `/*` |
|         - | 2090 | ` * Reserve a memory object.` |
|         - | 2091 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2092 | ` */` |
|   3033675 | 2093 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2094 | `{` |
|         - | 2095 | `	ph7_value *pObj;` |
|         - | 2096 | `	sxi32 rc;` |
|   3033680 | 2097 | `	if( pIndex ){` |
|         - | 2098 | `		/* Object index in the object table */` |
|   3033680 | 2099 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|   1516832 | 2100 | `	}` |
|         - | 2101 | `	/* Reserve a slot for the new object */` |
|   3033680 | 2102 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|   3033680 | 2103 | `	if( rc != SXRET_OK ){` |
|         - | 2104 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2105 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2106 | `		 */` |
|       ! 0 | 2107 | `		return 0;` |
|         - | 2108 | `	}` |
|   3033680 | 2109 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|   3033680 | 2110 | `	return pObj;` |
|   1516837 | 2111 | `}` |
|         - | 2112 | `/* Forward declaration */` |
|         - | 2113 | `/* Forward declarations for Fiber C functions */` |
|         - | 2114 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 2115 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 2116 | `/*` |
|         - | 2117 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 2118 | ` * directly as foreign functions.` |
|         - | 2119 | ` */` |
|         - | 2120 |  |
|         - | 2121 | `/*` |
|         - | 2122 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 2123 | ` * start compiling the target PHP program.` |
|         - | 2124 | ` */` |
|      5740 | 2125 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 2126 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 2127 | `	 ph7 *pEngine /* Master engine */` |
|         - | 2128 | `	 )` |
|         5 | 2129 | `{` |
|         - | 2130 | `	ph7_value *pObj;` |
|         - | 2131 | `	sxi32 rc;` |
|         - | 2132 | `	/* Zero the structure */` |
|      5745 | 2133 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 2134 | `	/* Initialize VM fields */` |
|      5745 | 2135 | `	pVm->pEngine = &(*pEngine);` |
|      5745 | 2136 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|         - | 2137 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 2138 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 2139 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      5745 | 2140 | `	pVm->bDisplayErrors = 0;` |
|      5745 | 2141 | `	pVm->bLogErrors = 1;` |
|         - | 2142 | `	/* mbstring's substitute character, php's default (the internal encoding` |
|         - | 2143 | `	 * beside it is UTF-8, which is the zero the struct already holds) */` |
|      5745 | 2144 | `	pVm->iMbSubstitute = '?';` |
|      5745 | 2145 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 2146 | `	/* Instructions containers */` |
|      5745 | 2147 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      5745 | 2148 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      5745 | 2149 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 2150 | `	/* Object containers */` |
|      5745 | 2151 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      5745 | 2152 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|         - | 2153 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      5745 | 2154 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      5745 | 2155 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      5745 | 2156 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      5745 | 2157 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 2158 | `	/* Virtual machine internal containers */` |
|      5745 | 2159 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      5745 | 2160 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      5745 | 2161 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      5745 | 2162 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|      5745 | 2163 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      5745 | 2164 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 2165 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 2166 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 2167 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 2168 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 2169 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 2170 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 2171 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 2172 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      5745 | 2173 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5745 | 2174 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5745 | 2175 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5745 | 2176 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      5745 | 2177 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      5745 | 2178 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      5745 | 2179 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|      5745 | 2180 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      5745 | 2181 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      5745 | 2182 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      5745 | 2183 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|      5745 | 2184 | `	SySetInit(&pVm->aPersistSock,&pVm->sAllocator,sizeof(VmPersistSock));` |
|      5745 | 2185 | `	pVm->bIniSeeded = 0;` |
|      5745 | 2186 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|      5745 | 2187 | `	PH7_MemObjInit(&(*pVm),&pVm->sSessHandler);` |
|      5745 | 2188 | `	SyBlobInit(&pVm->sSessData,&pVm->sAllocator);` |
|      5745 | 2189 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|      5745 | 2190 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|      5745 | 2191 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|      5745 | 2192 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|      5745 | 2193 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      5745 | 2194 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      5745 | 2195 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      5745 | 2196 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|      5745 | 2197 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      5745 | 2198 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      5745 | 2199 | `	pVm->nResourceIdNext = 1;` |
|      5745 | 2200 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      5745 | 2201 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      5745 | 2202 | `	pVm->pMagicSetThis = 0;` |
|      5745 | 2203 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      5745 | 2204 | `	pVm->pHookSetThis = 0;` |
|      5745 | 2205 | `	pVm->pHookSetAttr = 0;` |
|      5745 | 2206 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      5745 | 2207 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      5745 | 2208 | `	pVm->pMagicCallThis = 0;` |
|      5745 | 2209 | `	pVm->pMagicCallClass = 0;` |
|      5745 | 2210 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      5745 | 2211 | `	pVm->pIdleCallFrames = 0;` |
|      5745 | 2212 | `	pVm->pIdleOperandStacks = 0;` |
|      5745 | 2213 | `	pVm->nIdleOperandStacks = 0;` |
|      5745 | 2214 | `	pVm->pIdleStackNodes = 0;` |
|      5745 | 2215 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      5745 | 2216 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      5745 | 2217 | `	pVm->pPendingException = 0;` |
|      5745 | 2218 | `	pVm->pInflightException = 0;` |
|      5745 | 2219 | `	pVm->nInflightExcBase = 0;` |
|      5745 | 2220 | `	pVm->pResumeFrame = 0;` |
|      5745 | 2221 | `	pVm->iResumePc = 0;` |
|      5745 | 2222 | `	pVm->pResumeInstr = 0;` |
|      5745 | 2223 | `	pVm->iResumeStackDepth = 0;` |
|      5745 | 2224 | `	pVm->nBoundaryRc = 0;` |
|      5745 | 2225 | `	PH7_CmpRefusalClear(&(*pVm));` |
|      5745 | 2226 | `	pVm->pConstEvalClass = 0;` |
|      5745 | 2227 | `	pVm->nConstEvalDepth = 0;` |
|      5745 | 2228 | `	pVm->pConstCycleAttr = 0;` |
|      5745 | 2229 | `	pVm->pConstCycleClass = 0;` |
|      5745 | 2230 | `	SySetReset(&pVm->aMagicGuard);` |
|      5745 | 2231 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2232 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2233 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2234 | `	}` |
|      5745 | 2235 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      5745 | 2236 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2237 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2238 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2239 | `	}` |
|      5745 | 2240 | `	pVm->pHookSetAttr = 0;` |
|      5745 | 2241 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      5745 | 2242 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2243 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2244 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2245 | `	}` |
|      5745 | 2246 | `	pVm->pMagicCallClass = 0;` |
|      5745 | 2247 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 2248 | `	/* Configuration containers */` |
|      5745 | 2249 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      5745 | 2250 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      5745 | 2251 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      5745 | 2252 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      5745 | 2253 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|      5745 | 2254 | `	pVm->iResponseStatus = 200;` |
|      5745 | 2255 | `	pVm->bHeadersSent = 0;` |
|      5745 | 2256 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|      5745 | 2257 | `	SySetInit(&pVm->aSuppressedIo,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 2258 | `	/* Error callbacks containers */` |
|      5745 | 2259 | `	PH7_MemObjInit(&(*pVm),&pVm->sExceptionCB);` |
|      5745 | 2260 | `	PH7_MemObjInit(&(*pVm),&pVm->sErrCB);` |
|      5745 | 2261 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|      5745 | 2262 | `	SySetInit(&pVm->aExceptionCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      5745 | 2263 | `	SySetInit(&pVm->aErrCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      5745 | 2264 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 2265 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 2266 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 2267 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 2268 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 2269 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 2270 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 2271 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      5745 | 2272 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      5745 | 2273 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 2274 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 2275 | `#else` |
|         - | 2276 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 2277 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 2278 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 2279 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 2280 | `	pVm->nMaxDepth = 512;` |
|         - | 2281 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 2282 | `#endif` |
|         - | 2283 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 2284 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 2285 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      5745 | 2286 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 2287 | `	/* JSON return status */` |
|      5745 | 2288 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2289 | `	/* PRNG context */` |
|      5745 | 2290 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 2291 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 2292 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      5745 | 2293 | `	pVm->mtSeeded = FALSE;` |
|         - | 2294 | `	/* Install the null constant */` |
|      5745 | 2295 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5745 | 2296 | `	if( pObj == 0 ){` |
|       ! 0 | 2297 | `		rc = SXERR_MEM;` |
|       ! 0 | 2298 | `		goto Err;` |
|         - | 2299 | `	}` |
|      5745 | 2300 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 2301 | `	/* Install the boolean TRUE constant */` |
|      5745 | 2302 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5745 | 2303 | `	if( pObj == 0 ){` |
|       ! 0 | 2304 | `		rc = SXERR_MEM;` |
|       ! 0 | 2305 | `		goto Err;` |
|         - | 2306 | `	}` |
|      5745 | 2307 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 2308 | `	/* Install the boolean FALSE constant */` |
|      5745 | 2309 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5745 | 2310 | `	if( pObj == 0 ){` |
|       ! 0 | 2311 | `		rc = SXERR_MEM;` |
|       ! 0 | 2312 | `		goto Err;` |
|         - | 2313 | `	}` |
|      5745 | 2314 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 2315 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 2316 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 2317 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      5745 | 2318 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      5745 | 2319 | `	if( pObj == 0 ){` |
|       ! 0 | 2320 | `		rc = SXERR_MEM;` |
|       ! 0 | 2321 | `		goto Err;` |
|         - | 2322 | `	}` |
|      5745 | 2323 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 2324 | `	/* Create the global frame */` |
|      5745 | 2325 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      5745 | 2326 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2327 | `		goto Err;` |
|         - | 2328 | `	}` |
|         - | 2329 | `	/* Initialize the code generator */` |
|      5745 | 2330 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      5745 | 2331 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2332 | `		goto Err;` |
|         - | 2333 | `	}` |
|         - | 2334 | `	/* VM correctly initialized,set the magic number */` |
|      5745 | 2335 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 2336 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 2337 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      5745 | 2338 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 2339 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      5745 | 2340 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 2341 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 2342 | `	 * compiled — its classes are internal too. */` |
|         - | 2343 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      5745 | 2344 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      5745 | 2345 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      5745 | 2346 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      5745 | 2347 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      5745 | 2348 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 2349 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      5745 | 2350 | `	pVm->pCoalesceObj = 0;` |
|      5745 | 2351 | `	pVm->bCoalesceArmed = 0;` |
|      5745 | 2352 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 2353 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - | 2354 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - | 2355 | ``	 * segfaults the first `new Fiber`. */`` |
|      5745 | 2356 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      5745 | 2357 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 2358 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|         - | 2359 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|         - | 2360 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|         - | 2361 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|         - | 2362 | `	 * rather than being stamped on afterwards. */` |
|      5745 | 2363 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|      5745 | 2364 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      5745 | 2365 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      5745 | 2366 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 2367 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      5745 | 2368 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 2369 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|      5745 | 2370 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|         - | 2371 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|         - | 2372 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - | 2373 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - | 2374 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - | 2375 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      5745 | 2376 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      5745 | 2377 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - | 2378 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|         - | 2379 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|         - | 2380 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|      5745 | 2381 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|         - | 2382 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 2383 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 2384 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      5745 | 2385 | `	PH7_VmInstallReflection(&(*pVm));` |
|      5745 | 2386 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      5745 | 2387 | `	PH7_VmInstallSpl(&(*pVm));` |
|         - | 2388 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      5745 | 2389 | `	PH7_VmInstallHashContext(&(*pVm));` |
|         - | 2390 | `#endif` |
|         - | 2391 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2392 | `	/* php_user_filter and StreamBucket: the classes stream_filter_register()` |
|         - | 2393 | `	 * builds its filters out of. */` |
|      5745 | 2394 | `	PH7_VmInstallStreamFilter(&(*pVm));` |
|         - | 2395 | `#endif` |
|      5745 | 2396 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|         - | 2397 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 2398 | `	/* php 8.4's RoundingMode, round()'s declared third argument. It rides the` |
|         - | 2399 | `	 * builtin guard because round() -- and bcround() -- do: a build with no` |
|         - | 2400 | `	 * consumer for the symbol does not ship the symbol. */` |
|      5745 | 2401 | `	PH7_VmInstallRoundingMode(&(*pVm));` |
|         - | 2402 | `	/* BcMath\Number: after RoundingMode, whose cases its round() reads. */` |
|      5745 | 2403 | `	PH7_VmInstallBcMath(&(*pVm));` |
|         - | 2404 | `	/* php's ext/random object surface. It rides the builtin guard for the same` |
|         - | 2405 | `	 * reason bcmath does: the tiny build ships no consumer for it. */` |
|      5745 | 2406 | `	PH7_VmInstallRandom(&(*pVm));` |
|         - | 2407 | `#endif` |
|      5745 | 2408 | `	PH7_VmInstallSession(&(*pVm));` |
|      5745 | 2409 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 2410 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2411 | `	/* libxml2-backed surfaces: shared plumbing first, then the ext/xml push` |
|         - | 2412 | `	 * parser and the DOM and XMLWriter class libraries that build on it. */` |
|      5745 | 2413 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      5745 | 2414 | `	PH7_VmInstallXml(&(*pVm));` |
|      5745 | 2415 | `	PH7_VmInstallDom(&(*pVm));` |
|      5745 | 2416 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 2417 | `#endif` |
|         - | 2418 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 2419 | ``	/* ext/pdo's class library first: `Pdo\Sqlite` extends PDO, so the driver's`` |
|         - | 2420 | `	 * installer needs the parent already mounted. */` |
|      5745 | 2421 | `	PH7_VmInstallPdo(&(*pVm));` |
|      5745 | 2422 | `	PH7_VmInstallPdoSqlite(&(*pVm));` |
|         - | 2423 | `#endif` |
|         - | 2424 | `#ifdef PH7_ENABLE_CURL` |
|         - | 2425 | `	/* ext/curl: the libcurl binding. */` |
|      5745 | 2426 | `	PH7_VmInstallCurl(&(*pVm));` |
|         - | 2427 | `#endif` |
|      5745 | 2428 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 2429 | `	/* Reset the code generator */` |
|      5745 | 2430 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      5745 | 2431 | `	return SXRET_OK;` |
|       ! 0 | 2432 | `Err:` |
|       ! 0 | 2433 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 2434 | `	return rc;` |
|      2875 | 2435 | `}` |
|         - | 2436 | `/*` |
|         - | 2437 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 2438 | ` * routine which store the output in an internal blob.` |
|         - | 2439 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 2440 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 2441 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 2442 | ` * Refer to the official docurmentation for additional information.` |
|         - | 2443 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 2444 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 2445 | ` * to finish executing and extracting the output.` |
|         - | 2446 | ` */` |
|       348 | 2447 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 2448 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 2449 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 2450 | `	void *pUserData     /* User private data */` |
|         - | 2451 | `	)` |
|         4 | 2452 | `{` |
|         - | 2453 | `	 sxi32 rc;` |
|         - | 2454 | `	 /* Store the output in an internal BLOB */` |
|       352 | 2455 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       352 | 2456 | `	 return rc;` |
|         4 | 2457 | `}` |
|         - | 2458 | `/*` |
|         - | 2459 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 2460 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 2461 | ` */` |
|    213214 | 2462 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 2463 | `{` |
|    213219 | 2464 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    213219 | 2465 | `	if( xCons != VmObConsumer ){` |
|     40971 | 2466 | `		pVm->nOutputLen += nLen;` |
|     40971 | 2467 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      1711 | 2468 | `			pVm->bHeadersSent = 1;` |
|       853 | 2469 | `		}` |
|     20483 | 2470 | `	}` |
|    213219 | 2471 | `}` |
|         - | 2472 | `/*` |
|         - | 2473 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 2474 | ` *` |
|         - | 2475 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 2476 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 2477 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 2478 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 2479 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 2480 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 2481 | ` *` |
|         - | 2482 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 2483 | ` * conservative BY CONSTRUCTION:` |
|         - | 2484 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 2485 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 2486 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 2487 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 2488 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 2489 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 2490 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 2491 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 2492 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 2493 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 2494 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 2495 | ` *     instruction-count bound -> fallback.` |
|         - | 2496 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 2497 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 2498 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 2499 | ` *` |
|         - | 2500 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 2501 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 2502 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 2503 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 2504 | ` *` |
|         - | 2505 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 2506 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 2507 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 2508 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 2509 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 2510 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 2511 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 2512 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 2513 | ` * entry here.` |
|         - | 2514 | ` */` |
|         - | 2515 | `/*` |
|         - | 2516 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 2517 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 2518 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 2519 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 2520 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 2521 | ` */` |
|     61864 | 2522 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 2523 | `{` |
|     61869 | 2524 | `	int push = 0, n = 0;` |
|         - | 2525 | `	sxi32 d;` |
|     61869 | 2526 | `	switch( pI->iOp ){` |
|         - | 2527 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 2528 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     11684 | 2529 | `	case PH7_OP_LOADC:` |
|         - | 2530 | `	case PH7_OP_DUP:` |
|     23373 | 2531 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      4333 | 2532 | `	case PH7_OP_LOAD:` |
|      8671 | 2533 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|      8671 | 2534 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         9 | 2535 | `	case PH7_OP_LOAD_REF:` |
|        19 | 2536 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2537 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       649 | 2538 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 2539 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 2540 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1303 | 2541 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 2542 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 2543 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       275 | 2544 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 2545 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       555 | 2546 | `		if( pI->iP2 == 0 ){` |
|       555 | 2547 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       280 | 2548 | `		}else{` |
|       ! 0 | 2549 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 2550 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 2551 | `		}` |
|       555 | 2552 | `		break;` |
|         - | 2553 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 2554 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       164 | 2555 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 2556 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 2557 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 2558 | `	case PH7_OP_NOOP:` |
|       333 | 2559 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2560 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 2561 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|       733 | 2562 | `	case PH7_OP_STORE:` |
|      1471 | 2563 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      1471 | 2564 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 2565 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1483 | 2566 | `	case PH7_OP_POP:` |
|         - | 2567 | `	case PH7_OP_CONSUME:` |
|      2971 | 2568 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2569 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|         - | 2570 | `	 * pushed and none is popped, on any path. */` |
|       ! 0 | 2571 | `	case PH7_OP_ROT_CALLEE:` |
|       ! 0 | 2572 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2573 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 2574 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 2575 | `	 * true pop count is a runtime value — never reaches here. */` |
|       409 | 2576 | `	case PH7_OP_CALL:` |
|       823 | 2577 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2578 | `	/* Jumps. */` |
|       103 | 2579 | `	case PH7_OP_JMP:` |
|       211 | 2580 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       185 | 2581 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|       375 | 2582 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|       375 | 2583 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 2584 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      3629 | 2585 | `	case PH7_OP_DONE:` |
|      7263 | 2586 | `		n = 0; break;` |
|      7276 | 2587 | `	default:` |
|     14557 | 2588 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 2589 | `	}` |
|     47317 | 2590 | `	*pPush = push; *pN = n;` |
|     47317 | 2591 | `	return 1;` |
|     30937 | 2592 | `}` |
|         - | 2593 | `/*` |
|         - | 2594 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 2595 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 2596 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 2597 | ` */` |
|     17062 | 2598 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 2599 | `{` |
|         - | 2600 | `	void *pScratch;` |
|         - | 2601 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 2602 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 2603 | `	sxi32 iMax;` |
|         - | 2604 | `	int push, n, k;` |
|         - | 2605 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     17067 | 2606 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 2607 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 2608 | `		return VM_STACK_UNMODELED;` |
|         - | 2609 | `	}` |
|         - | 2610 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     56279 | 2611 | `	for( i = 0; i < nInstr; i++ ){` |
|     53769 | 2612 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     14557 | 2613 | `			return VM_STACK_UNMODELED;` |
|         - | 2614 | `		}` |
|     19611 | 2615 | `	}` |
|         - | 2616 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 2617 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 2618 | `	 * first (the byte array last needs no alignment). */` |
|      2515 | 2619 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      2515 | 2620 | `	if( pScratch == 0 ){` |
|       ! 0 | 2621 | `		return VM_STACK_UNMODELED;` |
|         - | 2622 | `	}` |
|      2515 | 2623 | `	aH  = (sxi32 *)pScratch;` |
|      2515 | 2624 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      2515 | 2625 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     12831 | 2626 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      2515 | 2627 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      2515 | 2628 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|     10615 | 2629 | `	while( nQ > 0 ){` |
|      8105 | 2630 | `		sxu32 pc = aQ[--nQ];` |
|         - | 2631 | `		sxi32 h;` |
|      8105 | 2632 | `		aIn[pc] = 0;` |
|      8105 | 2633 | `		h = aH[pc];` |
|      8105 | 2634 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|      8105 | 2635 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|      8105 | 2636 | `		if( h + push > iMax ){ iMax = h + push; }` |
|      8105 | 2637 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     13715 | 2638 | `		for( k = 0; k < n; k++ ){` |
|      5615 | 2639 | `			sxi32 hn = h + delta[k];` |
|      5615 | 2640 | `			sxu32 t = succ[k];` |
|      5615 | 2641 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      5615 | 2642 | `			if( hn > aH[t] ){` |
|      5595 | 2643 | `				aH[t] = hn;` |
|      5595 | 2644 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      2795 | 2645 | `			}` |
|      2810 | 2646 | `		}` |
|      8105 | 2647 | `		if( iMax < 0 ){ break; }` |
|         5 | 2648 | `	}` |
|      2515 | 2649 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      2515 | 2650 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|      8536 | 2651 | `}` |
|         - | 2652 | `/*` |
|         - | 2653 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 2654 | ` * our compiled PHP program.` |
|         - | 2655 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 2656 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 2657 | ` *` |
|         - | 2658 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 2659 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 2660 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 2661 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 2662 | ` */` |
|   4709683 | 2663 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 2664 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2665 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 2666 | `	)` |
|         5 | 2667 | `{` |
|         - | 2668 | `	ph7_value *pStack;` |
|         - | 2669 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 2670 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 2671 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 2672 | `  ** on the maximum stack depth required.` |
|         - | 2673 | `  **` |
|         - | 2674 | `  ** Allocation all the stack space we will ever need.` |
|         - | 2675 | `  */` |
|   4709688 | 2676 | `	nInstr += VM_STACK_GUARD;` |
|   4709688 | 2677 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   4709688 | 2678 | `	if( pStack == 0 ){` |
|       ! 0 | 2679 | `		return 0;` |
|         - | 2680 | `	}` |
|         - | 2681 | `	/* Initialize the operand stack */` |
|  98228031 | 2682 | `	while( nInstr > 0 ){` |
|  93518348 | 2683 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  93518348 | 2684 | `		--nInstr;` |
|         5 | 2685 | `	}` |
|         - | 2686 | `	/* Ready for bytecode execution */` |
|   4709688 | 2687 | `	return pStack;` |
|   2354892 | 2688 | `}` |
|         - | 2689 | `/*` |
|         - | 2690 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 2691 | ` *` |
|         - | 2692 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 2693 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 2694 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 2695 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 2696 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 2697 | ` * the per-slot PH7_MemObjInit.` |
|         - | 2698 | ` *` |
|         - | 2699 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 2700 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 2701 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 2702 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|         - | 2703 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|         - | 2704 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|         - | 2705 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|         - | 2706 | ` *` |
|         - | 2707 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|         - | 2708 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|         - | 2709 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|         - | 2710 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|         - | 2711 | ` * recursion case is the one worth the O(1) simplicity.` |
|         - | 2712 | ` */` |
|         - | 2713 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 2714 | `struct VmIdleStack {` |
|         - | 2715 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 2716 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 2717 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 2718 | `};` |
|         - | 2719 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|         - | 2720 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|         - | 2721 | `                                    * (a large fallback-sized stack recursing would` |
|         - | 2722 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|         - | 2723 | `                                    * the tight-sized hot case is far below this) */` |
|         - | 2724 | `/*` |
|         - | 2725 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 2726 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 2727 | ` */` |
|    788944 | 2728 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 2729 | `{` |
|    788949 | 2730 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    788949 | 2731 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|    788949 | 2732 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    692318 | 2733 | `		ph7_value *pStack = pIdle->pStack;` |
|    692318 | 2734 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    692318 | 2735 | `		pVm->nIdleOperandStacks--;` |
|         - | 2736 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 2737 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    692318 | 2738 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    692318 | 2739 | `		pVm->pIdleStackNodes = pIdle;` |
|    692318 | 2740 | `		return pStack; /* slots already released -> reusable without re-init */` |
|         - | 2741 | `	}` |
|     96636 | 2742 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    394692 | 2743 | `}` |
|         - | 2744 | `/*` |
|         - | 2745 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 2746 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|         - | 2747 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|         - | 2748 | ` * live value.` |
|         - | 2749 | ` */` |
|    788744 | 2750 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|         5 | 2751 | `{` |
|         - | 2752 | `	VmIdleStack *pIdle;` |
|         - | 2753 | `	sxu32 i;` |
|    788749 | 2754 | `	if( pStack == 0 ){` |
|       ! 0 | 2755 | `		return;` |
|         - | 2756 | `	}` |
|    788749 | 2757 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|     91138 | 2758 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|     91138 | 2759 | `		return;` |
|         - | 2760 | `	}` |
|         - | 2761 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 2762 | `	 * pool-allocate only when the spare list is empty. */` |
|    697616 | 2763 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    697616 | 2764 | `	if( pIdle ){` |
|    692318 | 2765 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    346327 | 2766 | `	}else{` |
|      5303 | 2767 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      5303 | 2768 | `		if( pIdle == 0 ){` |
|       ! 0 | 2769 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 2770 | `			return;` |
|         - | 2771 | `		}` |
|         - | 2772 | `	}` |
|  46020518 | 2773 | `	for( i = 0; i < nCap; i++ ){` |
|  45322907 | 2774 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 2775 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 2776 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 2777 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 2778 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 2779 | `		 * across invocations — cheap defense in depth. */` |
|  45322907 | 2780 | `		pStack[i].nIdx = SXU32_HIGH;` |
|  22730989 | 2781 | `	}` |
|    697616 | 2782 | `	pIdle->pStack = pStack;` |
|    697616 | 2783 | `	pIdle->nCap = nCap;` |
|    697616 | 2784 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    697616 | 2785 | `	pVm->pIdleOperandStacks = pIdle;` |
|    697616 | 2786 | `	pVm->nIdleOperandStacks++;` |
|    394592 | 2787 | `}` |
|         - | 2788 | `/* Forward declaration */` |
|         - | 2789 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 2790 | `/*` |
|         - | 2791 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 2792 | ` * This routine gets called by the PH7 engine after` |
|         - | 2793 | ` * successful compilation of the target PHP program.` |
|         - | 2794 | ` */` |
|      4962 | 2795 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 2796 | `	ph7_vm *pVm /* Target VM */` |
|         - | 2797 | `	)` |
|         5 | 2798 | `{` |
|         - | 2799 | `	SyHashEntry *pEntry;` |
|         - | 2800 | `	sxi32 rc;` |
|      4967 | 2801 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 2802 | `		/* Initialize your VM first */` |
|       ! 0 | 2803 | `		return SXERR_CORRUPT;` |
|         - | 2804 | `	}` |
|         - | 2805 | `	/* Mark the VM ready for byte-code execution */` |
|      4967 | 2806 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 2807 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 2808 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 2809 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 2810 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      4967 | 2811 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 2812 | `	/* Emit the DONE instruction */` |
|      4967 | 2813 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      4967 | 2814 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2815 | `		return SXERR_MEM;` |
|         - | 2816 | `	}` |
|         - | 2817 | `	/* Script return value */` |
|      4967 | 2818 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 2819 | `	/* Allocate a new operand stack */` |
|      4967 | 2820 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      4967 | 2821 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 2822 | `		return SXERR_MEM;` |
|         - | 2823 | `	}` |
|         - | 2824 | `	/* Set the default VM output consumer callback and it's` |
|         - | 2825 | `	 * private data. */` |
|      4967 | 2826 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      4967 | 2827 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 2828 | `	/* Allocate the reference table */` |
|      4967 | 2829 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      4967 | 2830 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      4967 | 2831 | `	if( pVm->apRefObj == 0 ){` |
|         - | 2832 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2833 | `		return SXERR_MEM;` |
|         - | 2834 | `	}` |
|         - | 2835 | `	/* Zero the reference table */` |
|      4967 | 2836 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|         - | 2837 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      4967 | 2838 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      4967 | 2839 | `	if( rc != SXRET_OK ){` |
|         - | 2840 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2841 | `		return rc;` |
|         - | 2842 | `	}` |
|         - | 2843 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 2844 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 2845 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 2846 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 2847 | `	 * below it is compile-time/init state that survives a reset. */` |
|      4967 | 2848 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|         - | 2849 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      4967 | 2850 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      4967 | 2851 | `	if( rc != SXRET_OK ){` |
|         - | 2852 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2853 | `		return rc;` |
|         - | 2854 | `	}` |
|         - | 2855 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      4967 | 2856 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 2857 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      4967 | 2858 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 2859 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      4967 | 2860 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 2861 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      4967 | 2862 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 2863 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 2864 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      4967 | 2865 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      4967 | 2866 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 2867 | `#endif` |
|         - | 2868 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2869 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      4967 | 2870 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 2871 | `#endif` |
|         - | 2872 | `#ifdef PH7_ENABLE_CURL` |
|         - | 2873 | `	/* Register the CURLOPT_* / CURLINFO_* / CURLE_* family */` |
|      4967 | 2874 | `	PH7_RegisterCurlConstants(&(*pVm));` |
|         - | 2875 | `#endif` |
|         - | 2876 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 2877 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      4967 | 2878 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 2879 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      4967 | 2880 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 2881 | `	/* Initialize and install static and constants class attributes.` |
|         - | 2882 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 2883 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 2884 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 2885 | `	 * that function in sync when changing what is reserved here. */` |
|         - | 2886 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - | 2887 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - | 2888 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - | 2889 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - | 2890 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - | 2891 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - | 2892 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - | 2893 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 2894 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - | 2895 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      4967 | 2896 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    984017 | 2897 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    979055 | 2898 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    979055 | 2899 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2900 | `			return rc;` |
|         - | 2901 | `		}` |
|         5 | 2902 | `	}` |
|      4967 | 2903 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    983223 | 2904 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    978265 | 2905 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    978265 | 2906 | `		if( rc != SXRET_OK ){` |
|         6 | 2907 | `			return rc;` |
|         - | 2908 | `		}` |
|         5 | 2909 | `	}` |
|         - | 2910 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      4963 | 2911 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2912 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      4963 | 2913 | `	pVm->nNextObjId = 1;` |
|         - | 2914 | `	/* VM is ready for bytecode execution */` |
|      4963 | 2915 | `	return SXRET_OK;` |
|      2486 | 2916 | `}` |
|         - | 2917 | `/*` |
|         - | 2918 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 2919 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 2920 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 2921 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 2922 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 2923 | ` */` |
|        16 | 2924 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 2925 | `{` |
|         - | 2926 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|         - | 2927 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|         - | 2928 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|       540 | 2929 | `	while( pVm->pRefList ){` |
|       524 | 2930 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|       ! 0 | 2931 | `	}` |
|        16 | 2932 | `}` |
|         - | 2933 | `/*` |
|         - | 2934 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 2935 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 2936 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 2937 | ` */` |
|        96 | 2938 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 2939 | `{` |
|        96 | 2940 | `	PH7_MemObjRelease(pObj);` |
|        96 | 2941 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        96 | 2942 | `}` |
|         - | 2943 | `/*` |
|         - | 2944 | ` * Empty a set_error_handler()/set_exception_handler() stack, releasing every` |
|         - | 2945 | ` * saved handler. The SySet itself keeps its buffer for the next request; the` |
|         - | 2946 | ` * whole thing dies with the VM allocator either way.` |
|         - | 2947 | ` */` |
|        32 | 2948 | `static void VmReleaseHandlerStack(SySet *pStack)` |
|       ! 0 | 2949 | `{` |
|        32 | 2950 | `	VmHandlerSlot *aSlot = (VmHandlerSlot *)SySetBasePtr(pStack);` |
|         - | 2951 | `	sxu32 n;` |
|        32 | 2952 | `	for( n = 0 ; n < SySetUsed(pStack) ; ++n ){` |
|       ! 0 | 2953 | `		PH7_MemObjRelease(&aSlot[n].sCb);` |
|       ! 0 | 2954 | `	}` |
|        32 | 2955 | `	SySetReset(pStack);` |
|        32 | 2956 | `}` |
|         - | 2957 | `/*` |
|         - | 2958 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 2959 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 2960 | ` * of statics).` |
|         - | 2961 | ` */` |
|     18424 | 2962 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|       ! 0 | 2963 | `{` |
|     18424 | 2964 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 2965 | `	sxu32 k;` |
|     18428 | 2966 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|         4 | 2967 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|         2 | 2968 | `	}` |
|     18424 | 2969 | `}` |
|         - | 2970 | `/*` |
|         - | 2971 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 2972 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 2973 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 2974 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 2975 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 2976 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 2977 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 2978 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 2979 | ` *    has its static sentinels reset.` |
|         - | 2980 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 2981 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 2982 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 2983 | ` */` |
|        16 | 2984 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 2985 | `{` |
|         - | 2986 | `	SyHashEntry *pEntry;` |
|        16 | 2987 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     18440 | 2988 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     18424 | 2989 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     18424 | 2990 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 2991 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|         - | 2992 | `			 * release its captured-by-value environment, then free the entry,` |
|         - | 2993 | `			 * name buffer and structure. */` |
|         4 | 2994 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|         4 | 2995 | `			const char *zName = SyStringData(&pFunc->sName);` |
|         - | 2996 | `			sxu32 k;` |
|         4 | 2997 | `			VmResetFuncStatics(pFunc);` |
|         8 | 2998 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|         4 | 2999 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|         2 | 3000 | `			}` |
|         4 | 3001 | `			SySetRelease(&pFunc->aClosureEnv);` |
|         - | 3002 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 3003 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 3004 | `			if( zName ){` |
|         4 | 3005 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|         2 | 3006 | `			}` |
|         4 | 3007 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|         4 | 3008 | `			continue;` |
|         - | 3009 | `		}` |
|         - | 3010 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     36840 | 3011 | `		while( pFunc ){` |
|     18420 | 3012 | `			VmResetFuncStatics(pFunc);` |
|     18420 | 3013 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 3014 | `		}` |
|       ! 0 | 3015 | `	}` |
|        16 | 3016 | `	pVm->closure_cnt = 0;` |
|        16 | 3017 | `}` |
|         - | 3018 | `/*` |
|         - | 3019 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 3020 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 3021 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 3022 | ` * the class re-mount registers fresh ones.` |
|         - | 3023 | ` */` |
|        16 | 3024 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 3025 | `{` |
|         - | 3026 | `	SyHashEntry *pEntry;` |
|         - | 3027 | `	/* Common case: no class static typed properties — table already empty. */` |
|        16 | 3028 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        12 | 3029 | `		return;` |
|         - | 3030 | `	}` |
|         - | 3031 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 3032 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 3033 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 3034 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 3035 | `		if( pEntry->pUserData ){` |
|         4 | 3036 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 3037 | `		}` |
|       ! 0 | 3038 | `	}` |
|         4 | 3039 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 3040 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 | 3041 | `	pVm->nNativeSetSlot = 0;` |
|         8 | 3042 | `}` |
|         - | 3043 | `/*` |
|         - | 3044 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 3045 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 3046 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 3047 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 3048 | ` * comparing equal — both used to cast to 1.` |
|         - | 3049 | ` *` |
|         - | 3050 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 3051 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 3052 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 3053 | ` * aborting a cast.` |
|         - | 3054 | ` */` |
|       390 | 3055 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         3 | 3056 | `{` |
|         - | 3057 | `	SyHashEntry *pEntry;` |
|         - | 3058 | `	phl_res_id *pRec;` |
|       393 | 3059 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 3060 | `		return 0;` |
|         - | 3061 | `	}` |
|       393 | 3062 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|       393 | 3063 | `	if( pEntry ){` |
|       340 | 3064 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 3065 | `	}` |
|        55 | 3066 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        55 | 3067 | `	if( pRec == 0 ){` |
|       ! 0 | 3068 | `		return 0;` |
|         - | 3069 | `	}` |
|        55 | 3070 | `	pRec->pRes = pRes;` |
|        55 | 3071 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        55 | 3072 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 3073 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 3074 | `		return 0;` |
|         - | 3075 | `	}` |
|        55 | 3076 | `	return pRec->nId;` |
|       198 | 3077 | `}` |
|         - | 3078 | `/*` |
|         - | 3079 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 3080 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 3081 | ` */` |
|        16 | 3082 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 3083 | `{` |
|         - | 3084 | `	SyHashEntry *pEntry;` |
|        16 | 3085 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        16 | 3086 | `		pVm->nResourceIdNext = 1;` |
|        16 | 3087 | `		return;` |
|         - | 3088 | `	}` |
|       ! 0 | 3089 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 3090 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 3091 | `		if( pEntry->pUserData ){` |
|       ! 0 | 3092 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 3093 | `		}` |
|       ! 0 | 3094 | `	}` |
|       ! 0 | 3095 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 3096 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 3097 | `	pVm->nResourceIdNext = 1;` |
|         8 | 3098 | `}` |
|         - | 3099 | `/*` |
|         - | 3100 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 3101 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 3102 | ` *` |
|         - | 3103 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 3104 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 3105 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 3106 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 3107 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 3108 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 3109 | ` *` |
|         - | 3110 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 3111 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 3112 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 3113 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 3114 | ` * reserved during the run.` |
|         - | 3115 | ` *` |
|         - | 3116 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 3117 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 3118 | ` * global-scope destructors never fired.` |
|         - | 3119 | ` */` |
|        16 | 3120 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 3121 | `{` |
|         - | 3122 | `	sxu32 nWater,n;` |
|        16 | 3123 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 3124 | `		return SXERR_CORRUPT;` |
|         - | 3125 | `	}` |
|        16 | 3126 | `	nWater = pVm->nSuperBaseline;` |
|         - | 3127 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 3128 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        16 | 3129 | `	pVm->pGlobal = 0;` |
|         - | 3130 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 3131 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 3132 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 3133 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 3134 | `	 * object); unref'ing here would race the teardown below. */` |
|        16 | 3135 | `	pVm->pClosureThis = 0;` |
|        16 | 3136 | `	pVm->pClosureScope = 0;` |
|         - | 3137 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 3138 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 3139 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|         - | 3140 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        16 | 3141 | `	pVm->bInReset = 1;` |
|         - | 3142 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        16 | 3143 | `	VmResetRefTable(&(*pVm));` |
|         - | 3144 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 3145 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 3146 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 3147 | `	 * re-run define() overwrites the value in place). */` |
|        16 | 3148 | `	VmResetFunctionState(&(*pVm));` |
|         - | 3149 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 3150 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       560 | 3151 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|       544 | 3152 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|       544 | 3153 | `		if( pObj ){` |
|       544 | 3154 | `			PH7_MemObjRelease(pObj);` |
|       272 | 3155 | `		}` |
|       272 | 3156 | `	}` |
|         - | 3157 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 3158 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        16 | 3159 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 3160 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 3161 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        16 | 3162 | `	VmResetResourceIds(&(*pVm));` |
|         - | 3163 | `	/* (5) Unwind any active frames back to none. */` |
|        32 | 3164 | `	while( pVm->pFrame ){` |
|        16 | 3165 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 3166 | `	}` |
|         - | 3167 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        16 | 3168 | `	pVm->bInReset = 0;` |
|         - | 3169 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 3170 | `	 * slots (their indices no longer exist). */` |
|        16 | 3171 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        16 | 3172 | `	SySetReset(&pVm->aFreeObj);` |
|         - | 3173 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        16 | 3174 | `	SyHashRelease(&pVm->hSuper);` |
|        16 | 3175 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|         - | 3176 | `	/* (8) Drain remaining per-exec containers. */` |
|        16 | 3177 | `	SySetReset(&pVm->aSelf);` |
|         - | 3178 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 3179 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 3180 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        16 | 3181 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 3182 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 3183 | `		if( pCB ){` |
|         - | 3184 | `			int iArg;` |
|       ! 0 | 3185 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3186 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 3187 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 3188 | `			}` |
|       ! 0 | 3189 | `		}` |
|       ! 0 | 3190 | `	}` |
|        16 | 3191 | `	SySetReset(&pVm->aShutdown);` |
|         - | 3192 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 3193 | `	 * aborted program can leave entries behind). */` |
|        16 | 3194 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        16 | 3195 | `	SySetReset(&pVm->aException);` |
|        16 | 3196 | `	SySetReset(&pVm->aFinallyAction);` |
|        16 | 3197 | `	pVm->pPendingException = 0;` |
|        16 | 3198 | `	pVm->pInflightException = 0;` |
|        16 | 3199 | `	pVm->nInflightExcBase = 0;` |
|        16 | 3200 | `	pVm->pResumeFrame = 0;` |
|        16 | 3201 | `	pVm->iResumePc = 0;` |
|        16 | 3202 | `	pVm->pResumeInstr = 0;` |
|        16 | 3203 | `	pVm->iResumeStackDepth = 0;` |
|        16 | 3204 | `	pVm->nBoundaryRc = 0;` |
|        16 | 3205 | `	PH7_CmpRefusalClear(&(*pVm));` |
|        16 | 3206 | `	pVm->pConstEvalClass = 0;` |
|        16 | 3207 | `	pVm->nConstEvalDepth = 0;` |
|        16 | 3208 | `	pVm->pConstCycleAttr = 0;` |
|        16 | 3209 | `	pVm->pConstCycleClass = 0;` |
|        16 | 3210 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 3211 | `	{` |
|         - | 3212 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 3213 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        16 | 3214 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        16 | 3215 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 3216 | `		sxu32 iRmw;` |
|        16 | 3217 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 3218 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 3219 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 3220 | `		}` |
|        16 | 3221 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 3222 | `	}` |
|        16 | 3223 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 3224 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 3225 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 3226 | `	}` |
|        16 | 3227 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        16 | 3228 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 3229 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 3230 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 3231 | `	}` |
|        16 | 3232 | `	pVm->pHookSetAttr = 0;` |
|        16 | 3233 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        16 | 3234 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 3235 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 3236 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 3237 | `	}` |
|        16 | 3238 | `	pVm->pMagicCallClass = 0;` |
|        16 | 3239 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        16 | 3240 | `	pVm->nExceptDepth = 0;` |
|         - | 3241 | `	/* spl_autoload_register() callbacks are per request */` |
|        16 | 3242 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 3243 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 3244 | `		if( pCB ){` |
|       ! 0 | 3245 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3246 | `		}` |
|       ! 0 | 3247 | `	}` |
|        16 | 3248 | `	SySetReset(&pVm->aAutoload);` |
|         - | 3249 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 3250 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        16 | 3251 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 3252 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 3253 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 3254 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 3255 | `	}` |
|         - | 3256 | `	/* Output buffers */` |
|        16 | 3257 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 3258 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 3259 | `		if( pOb ){` |
|       ! 0 | 3260 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 3261 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 3262 | `		}` |
|       ! 0 | 3263 | `	}` |
|        16 | 3264 | `	SySetReset(&pVm->aOB);` |
|        16 | 3265 | `	pVm->nObDepth = 0;` |
|         - | 3266 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 3267 | `	{` |
|        16 | 3268 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        16 | 3269 | `		if( rc == SXRET_OK ){` |
|        16 | 3270 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         8 | 3271 | `		}` |
|        16 | 3272 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 3273 | `			return rc;` |
|         - | 3274 | `		}` |
|         - | 3275 | `	}` |
|         - | 3276 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 3277 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 3278 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 3279 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 3280 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 3281 | `	{` |
|         - | 3282 | `		SyHashEntry *pEntry;` |
|        16 | 3283 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      3172 | 3284 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      3156 | 3285 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 3286 | `			ph7_class_attr *pAttr;` |
|         - | 3287 | `			SyHashEntry *pAttrEntry;` |
|      3156 | 3288 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     16946 | 3289 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     12212 | 3290 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     12212 | 3291 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|         4 | 3292 | `					pAttr->nIdx = SXU32_HIGH;` |
|         4 | 3293 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|         2 | 3294 | `				}` |
|       ! 0 | 3295 | `			}` |
|         - | 3296 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 3297 | `			 * slots too so VM reuse re-evaluates them. */` |
|      3156 | 3298 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|     10804 | 3299 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|      7648 | 3300 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      7648 | 3301 | `				pAttr->nIdx = SXU32_HIGH;` |
|      7648 | 3302 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 3303 | `			}` |
|       ! 0 | 3304 | `		}` |
|        16 | 3305 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      3172 | 3306 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      3156 | 3307 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      3156 | 3308 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 3309 | `				return rc;` |
|         - | 3310 | `			}` |
|       ! 0 | 3311 | `		}` |
|         - | 3312 | `	}` |
|         - | 3313 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        16 | 3314 | `	SyBlobReset(&pVm->sConsumer);` |
|        16 | 3315 | `	pVm->nOutputLen = 0;` |
|        16 | 3316 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        16 | 3317 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        16 | 3318 | `	pVm->iResponseStatus = 200;` |
|        16 | 3319 | `	pVm->bHeadersSent = 0;` |
|        16 | 3320 | `	pVm->bHttpContext = 0;` |
|        16 | 3321 | `	VmReinitMemObj(&(*pVm),&pVm->sExceptionCB);` |
|        16 | 3322 | `	VmReinitMemObj(&(*pVm),&pVm->sErrCB);` |
|        16 | 3323 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|        16 | 3324 | `	VmReleaseHandlerStack(&pVm->aExceptionCBSaved);` |
|        16 | 3325 | `	VmReleaseHandlerStack(&pVm->aErrCBSaved);` |
|        16 | 3326 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         - | 3327 | `	/* The session's userland save handler belongs to the request that installed` |
|         - | 3328 | `	 * it; a reused VM (the -S server's) must not route the next request's store` |
|         - | 3329 | `	 * through the previous script's object. */` |
|        16 | 3330 | `	VmReinitMemObj(&(*pVm),&pVm->sSessHandler);` |
|        16 | 3331 | `	pVm->bSessOpened = 0;` |
|        16 | 3332 | `	SyBlobReset(&pVm->sSessData);` |
|        16 | 3333 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 3334 | `#ifdef PH7_ENABLE_PCRE` |
|        16 | 3335 | `	pVm->iPcreLastError = 0;` |
|         - | 3336 | `#endif` |
|         - | 3337 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3338 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        16 | 3339 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 3340 | `#endif` |
|         - | 3341 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 3342 | `	/* Close the previous request's databases: a reused VM (the -S server's)` |
|         - | 3343 | `	 * must not answer the next request through a handle that request opened. */` |
|        16 | 3344 | `	PH7_PdoVmReset(&(*pVm));` |
|         - | 3345 | `#endif` |
|         - | 3346 | `#ifdef PH7_ENABLE_CURL` |
|         - | 3347 | `	/* Same rule for the previous request's curl handles, which hold sockets` |
|         - | 3348 | `	 * and a connection cache of their own. */` |
|        16 | 3349 | `	PH7_CurlVmReset(&(*pVm));` |
|         - | 3350 | `#endif` |
|         - | 3351 | `	/* Drop the stream contexts this run created, the default one included: a` |
|         - | 3352 | `	 * reused VM (the -S server's) must not answer the next request from the` |
|         - | 3353 | `	 * previous one's stream_context_set_default(). */` |
|        16 | 3354 | `	PH7_StreamCtxVmReset(&(*pVm));` |
|         - | 3355 | `	/* And every filter INSTANCE it created: a chain that was never removed` |
|         - | 3356 | `	 * still owns memory the next request must not inherit. */` |
|        16 | 3357 | `	PH7_StreamFilterVmReset(&(*pVm));` |
|        16 | 3358 | `	pVm->iCmpCallbackExc = 0;` |
|        16 | 3359 | `	pVm->bHaltRequested = 0;` |
|        16 | 3360 | `	pVm->iExitStatus = 0;` |
|        16 | 3361 | `	pVm->nSpreadCallBase = 0;` |
|        16 | 3362 | `	VmSpreadCaptureReset(pVm);` |
|        16 | 3363 | `	pVm->nRecursionDepth = 0;` |
|        16 | 3364 | `	pVm->pActiveCtx = 0;` |
|        16 | 3365 | `	pVm->pCoalesceObj = 0;` |
|        16 | 3366 | `	pVm->bCoalesceArmed = 0;` |
|        16 | 3367 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 3368 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        16 | 3369 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 3370 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 3371 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        16 | 3372 | `	pVm->nNextObjId = 1;` |
|         - | 3373 | `	/* Set the ready flag */` |
|        16 | 3374 | `	pVm->nMagic = PH7_VM_RUN;` |
|        16 | 3375 | `	return SXRET_OK;` |
|         8 | 3376 | `}` |
|         - | 3377 | `/*` |
|         - | 3378 | ` * Release a Virtual Machine.` |
|         - | 3379 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 3380 | ` */` |
|      4958 | 3381 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 3382 | `{` |
|         - | 3383 | `	/* Set the stale magic number */` |
|      4963 | 3384 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 3385 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3386 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 3387 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      4963 | 3388 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 3389 | `#endif` |
|         - | 3390 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 3391 | `	/* Same rule for the sqlite3 handles behind still-open PDO objects. */` |
|      4963 | 3392 | `	PH7_PdoVmRelease(pVm);` |
|         - | 3393 | `#endif` |
|         - | 3394 | `#ifdef PH7_ENABLE_CURL` |
|         - | 3395 | `	/* Same rule for the libcurl handles behind still-open CurlHandle objects. */` |
|      4963 | 3396 | `	PH7_CurlVmRelease(pVm);` |
|         - | 3397 | `#endif` |
|         - | 3398 | `	/* Same rule for the OS directory streams behind still-open directory` |
|         - | 3399 | `	 * iterators: the DIR lives outside the backend. */` |
|      4963 | 3400 | `	PH7_SplDirVmRelease(pVm);` |
|         - | 3401 | `	/* Release the private memory subsystem */` |
|      4963 | 3402 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      4963 | 3403 | `	return SXRET_OK;` |
|         5 | 3404 | `}` |
|         - | 3405 | `/*` |
|         - | 3406 | ` * Initialize a foreign function call context.` |
|         - | 3407 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 3408 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 3409 | ` * functions.` |
|         - | 3410 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 3411 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 3412 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 3413 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 3414 | ` */` |
|   6045915 | 3415 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 3416 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 3417 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 3418 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 3419 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 3420 | `	sxi32 iFlags          /* Control flags */` |
|         - | 3421 | `	)` |
|         5 | 3422 | `{` |
|   6045920 | 3423 | `	pOut->pFunc = pFunc;` |
|   6045920 | 3424 | `	pOut->pVm   = pVm;` |
|   6045920 | 3425 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   6045920 | 3426 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 3427 | `	/* Assume a null return value */` |
|   6045920 | 3428 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   6045920 | 3429 | `	pOut->pRet = pRet;` |
|   6045920 | 3430 | `	pOut->iFlags = iFlags;` |
|   6045920 | 3431 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   6045920 | 3432 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - | 3433 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - | 3434 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - | 3435 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - | 3436 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|   6045920 | 3437 | `	pOut->pThis = 0;` |
|   6045920 | 3438 | `	pOut->pCalledClass = 0;` |
|   6045920 | 3439 | `	pOut->bThisInit = 0;` |
|   6045920 | 3440 | `	return SXRET_OK;` |
|         5 | 3441 | `}` |
|         - | 3442 | `/*` |
|         - | 3443 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 3444 | ` * left behind.` |
|         - | 3445 | ` */` |
|   6045915 | 3446 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 3447 | `{` |
|         - | 3448 | `	sxu32 n;` |
|   6045920 | 3449 | `	if( pCtx->bThisInit ){` |
|         - | 3450 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - | 3451 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - | 3452 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - | 3453 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|      7027 | 3454 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|      7027 | 3455 | `		pCtx->sThis.x.pOther = 0;` |
|      7027 | 3456 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|      7027 | 3457 | `		pCtx->bThisInit = 0;` |
|      3511 | 3458 | `	}` |
|   6045920 | 3459 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    811857 | 3460 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|   2051533 | 3461 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|   1239681 | 3462 | `			if( apObj[n] == 0 ){` |
|         - | 3463 | `				/* Already released */` |
|     15657 | 3464 | `				continue;` |
|         - | 3465 | `			}` |
|   1224029 | 3466 | `			PH7_MemObjRelease(apObj[n]);` |
|   1224029 | 3467 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    612017 | 3468 | `		}` |
|    811857 | 3469 | `		SySetRelease(&pCtx->sVar);` |
|    405926 | 3470 | `	}` |
|   6045920 | 3471 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 3472 | `		ph7_aux_data *aAux;` |
|         - | 3473 | `		void *pChunk;` |
|         - | 3474 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 3475 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 3476 | `		 */` |
|      3689 | 3477 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     14247 | 3478 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     10563 | 3479 | `			pChunk = aAux[n].pAuxData;` |
|         - | 3480 | `			/* Release the chunk */` |
|     10563 | 3481 | `			if( pChunk ){` |
|      9887 | 3482 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      4941 | 3483 | `			}` |
|      5284 | 3484 | `		}` |
|      3689 | 3485 | `		SySetRelease(&pCtx->sChunk);` |
|      1842 | 3486 | `	}` |
|   6045920 | 3487 | `}` |
|         - | 3488 | `/*` |
|         - | 3489 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 3490 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 3491 | ` */` |
|     15652 | 3492 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 3493 | `	ph7_context *pCtx, /* Call context */` |
|         - | 3494 | `	ph7_value *pValue  /* Release this value */` |
|         - | 3495 | `	)` |
|         5 | 3496 | `{` |
|     15657 | 3497 | `	if( pValue == 0 ){` |
|         - | 3498 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 3499 | `		return;` |
|         - | 3500 | `	}` |
|     15657 | 3501 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     15657 | 3502 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 3503 | `		sxu32 n;` |
|    463431 | 3504 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    463431 | 3505 | `			if( apObj[n] == pValue ){` |
|     15657 | 3506 | `				PH7_MemObjRelease(pValue);` |
|     15657 | 3507 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 3508 | `				/* Mark as released */` |
|     15657 | 3509 | `				apObj[n] = 0;` |
|     15657 | 3510 | `				break;` |
|         - | 3511 | `			}` |
|    223892 | 3512 | `		}` |
|      7826 | 3513 | `	}` |
|      7831 | 3514 | `}` |
|         - | 3515 | `/*` |
|         - | 3516 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 3517 | ` */` |
|  25143944 | 3518 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 3519 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 3520 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 3521 | `	)` |
|         5 | 3522 | `{` |
|  25143949 | 3523 | `	ph7_value *pTos = *ppTos;` |
|  54184798 | 3524 | `	while( nPop > 0 ){` |
|  29040854 | 3525 | `		PH7_MemObjRelease(pTos);` |
|  29040854 | 3526 | `		pTos--;` |
|  29040854 | 3527 | `		nPop--;` |
|         5 | 3528 | `	}` |
|         - | 3529 | `	/* Top of the stack */` |
|  25143949 | 3530 | `	*ppTos = pTos;` |
|  25143949 | 3531 | `}` |
|         - | 3532 | `/*` |
|         - | 3533 | ` * Reserve a memory object.` |
|         - | 3534 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 3535 | ` */` |
|  22608106 | 3536 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 3537 | `{` |
|  22608111 | 3538 | `	ph7_value *pObj = 0;` |
|         - | 3539 | `	VmSlot *pSlot;` |
|         - | 3540 | `	sxu32 nIdx;` |
|         - | 3541 | `	/* Check for a free slot */` |
|  22608111 | 3542 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  22608111 | 3543 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  22608111 | 3544 | `	if( pSlot ){` |
|  19574508 | 3545 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  19574508 | 3546 | `		nIdx = pSlot->nIdx;` |
|   9788566 | 3547 | `	}` |
|  22608111 | 3548 | `	if( pObj == 0 ){` |
|         - | 3549 | `		/* Reserve a new memory object */` |
|   3033608 | 3550 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   3033608 | 3551 | `		if( pObj == 0 ){` |
|       ! 0 | 3552 | `			return 0;` |
|         - | 3553 | `		}` |
|   1516796 | 3554 | `	}` |
|         - | 3555 | `	/* Set a null default value */` |
|  22608111 | 3556 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  22608111 | 3557 | `	pObj->nIdx = nIdx;` |
|  22608111 | 3558 | `	return pObj;` |
|  11305367 | 3559 | `}` |
|         - | 3560 | `/*` |
|         - | 3561 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 3562 | ` */` |
|     69196 | 3563 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 3564 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 3565 | `	const char *zKey,  /* Entry key */` |
|         - | 3566 | `	sxu32 nByte,       /* Key length */` |
|         - | 3567 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 3568 | `	)` |
|         5 | 3569 | `{` |
|         - | 3570 | `	ph7_value sKey;` |
|         - | 3571 | `	sxi32 rc;` |
|     69201 | 3572 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     69201 | 3573 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 3574 | `	/* Perform the insertion */` |
|     69201 | 3575 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     69201 | 3576 | `	PH7_MemObjRelease(&sKey);` |
|     69201 | 3577 | `	return rc;` |
|         5 | 3578 | `}` |
|         - | 3579 | `/*` |
|         - | 3580 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 3581 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 3582 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 3583 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 3584 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 3585 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 3586 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 3587 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 3588 | ` *     superglobal in place.` |
|         - | 3589 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 3590 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|         - | 3591 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|         - | 3592 | ` *     a plain variable.` |
|         - | 3593 | ` */` |
|       196 | 3594 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         4 | 3595 | `{` |
|       200 | 3596 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 3597 | `	SyHashEntry *pEntry;` |
|         - | 3598 | `	ph7_value *pObj;` |
|         - | 3599 | `	char *zDup;` |
|         - | 3600 | `	sxu32 nIdx;` |
|         - | 3601 | `	sxi32 rc;` |
|         - | 3602 | `	/* Walk down to the global frame */` |
|       228 | 3603 | `	while( pFrame->pParent ){` |
|        30 | 3604 | `		pFrame = pFrame->pParent;` |
|         2 | 3605 | `	}` |
|         - | 3606 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       200 | 3607 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|       200 | 3608 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 3609 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 3610 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 3611 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 3612 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 3613 | `		 * of $GLOBALS itself). */` |
|         5 | 3614 | `		pEntry = 0;` |
|         2 | 3615 | `	}` |
|       200 | 3616 | `	if( pEntry == 0 ){` |
|       200 | 3617 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|        98 | 3618 | `	}` |
|       200 | 3619 | `	if( pEntry ){` |
|         5 | 3620 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 3621 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|         - | 3622 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|         3 | 3623 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|         3 | 3624 | `			return SXRET_OK;` |
|         - | 3625 | `		}` |
|         3 | 3626 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 3627 | `		if( pObj == 0 ){` |
|       ! 0 | 3628 | `			return SXERR_NOTFOUND;` |
|         - | 3629 | `		}` |
|         3 | 3630 | `		if( pValue ){` |
|         3 | 3631 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 3632 | `		}else{` |
|       ! 0 | 3633 | `			PH7_MemObjToNull(pObj);` |
|         - | 3634 | `		}` |
|         3 | 3635 | `		return SXRET_OK;` |
|         - | 3636 | `	}` |
|       196 | 3637 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 3638 | `		/* Reserve a fresh slot for the new global */` |
|       194 | 3639 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       194 | 3640 | `		if( pObj == 0 ){` |
|       ! 0 | 3641 | `			return SXERR_MEM;` |
|         - | 3642 | `		}` |
|       194 | 3643 | `		nIdx = pObj->nIdx;` |
|        99 | 3644 | `	}else{` |
|         - | 3645 | `		/* Reference assignment: bind the name to the existing slot */` |
|         3 | 3646 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|         3 | 3647 | `		if( pObj == 0 ){` |
|       ! 0 | 3648 | `			return SXERR_NOTFOUND;` |
|         - | 3649 | `		}` |
|         3 | 3650 | `		nIdx = nRefIdx;` |
|         - | 3651 | `	}` |
|       196 | 3652 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       196 | 3653 | `	if( zDup == 0 ){` |
|       ! 0 | 3654 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3655 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 3656 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|         - | 3657 | `			VmSlot sFree;` |
|       ! 0 | 3658 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3659 | `			sFree.pUserData = 0;` |
|       ! 0 | 3660 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3661 | `		}` |
|       ! 0 | 3662 | `		return SXERR_MEM;` |
|         - | 3663 | `	}` |
|       196 | 3664 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       196 | 3665 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3666 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3667 | `			VmSlot sFree;` |
|       ! 0 | 3668 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3669 | `			sFree.pUserData = 0;` |
|       ! 0 | 3670 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3671 | `		}` |
|       ! 0 | 3672 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 3673 | `		return rc;` |
|         - | 3674 | `	}` |
|         - | 3675 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       196 | 3676 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       196 | 3677 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       196 | 3678 | `	if( nRefIdx == SXU32_HIGH ){` |
|       194 | 3679 | `		pObj->nIdx = nIdx;` |
|       194 | 3680 | `		if( pValue ){` |
|       179 | 3681 | `			PH7_MemObjStore(pValue,pObj);` |
|        88 | 3682 | `		}` |
|        95 | 3683 | `	}` |
|       196 | 3684 | `	return SXRET_OK;` |
|       102 | 3685 | `}` |
|         - | 3686 | `/*` |
|         - | 3687 | ` * Extract a variable value from the top active VM frame.` |
|         - | 3688 | ` * Return a pointer to the variable value on success.` |
|         - | 3689 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 3690 | ` */` |
|  17322418 | 3691 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 3692 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 3693 | `	const SyString *pName, /* Variable name */` |
|         - | 3694 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 3695 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 3696 | `	)` |
|         5 | 3697 | `{` |
|  17322423 | 3698 | `	int bNullify = FALSE;` |
|         - | 3699 | `	SyHashEntry *pEntry;` |
|         - | 3700 | `	VmFrame *pFrame;` |
|         - | 3701 | `	ph7_value *pObj;` |
|         - | 3702 | `	sxu32 nIdx;` |
|         - | 3703 | `	sxi32 rc;` |
|         - | 3704 | `	/* Point to the top active frame */` |
|  17322423 | 3705 | `	pFrame = pVm->pFrame;` |
|  17322423 | 3706 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 3707 | `	/* Perform the lookup */` |
|  17322423 | 3708 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 3709 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        18 | 3710 | `		pName = &sAnnon;` |
|         - | 3711 | `		/* Always nullify the object */` |
|        18 | 3712 | `		bNullify = TRUE;` |
|        18 | 3713 | `		bDup = FALSE;` |
|         8 | 3714 | `	}` |
|         - | 3715 | `	/* Check the superglobals table first */` |
|  17322423 | 3716 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  17322423 | 3717 | `	if( pEntry == 0 ){` |
|         - | 3718 | `		/* Query the top active frame */` |
|  17319999 | 3719 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  17319999 | 3720 | `		if( pEntry == 0 ){` |
|   1252294 | 3721 | `			char *zName = (char *)pName->zString;` |
|         - | 3722 | `			VmSlot sLocal;` |
|   1252294 | 3723 | `			if( !bCreate ){` |
|         - | 3724 | `				/* Do not create the variable,return NULL instead */` |
|     12155 | 3725 | `				return 0;` |
|         - | 3726 | `			}` |
|         - | 3727 | `			/* No such variable,automatically create a new one and install` |
|         - | 3728 | `			 * it in the current frame.` |
|         - | 3729 | `			 */` |
|   1240144 | 3730 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1240144 | 3731 | `			if( pObj == 0 ){` |
|       ! 0 | 3732 | `				return 0;` |
|         - | 3733 | `			}` |
|   1240144 | 3734 | `			nIdx = pObj->nIdx;` |
|   1240144 | 3735 | `			if( bDup ){` |
|         - | 3736 | `				/* Duplicate name */` |
|      9675 | 3737 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      9675 | 3738 | `				if( zName == 0 ){` |
|       ! 0 | 3739 | `					return 0;` |
|         - | 3740 | `				}` |
|      4826 | 3741 | `			}` |
|         - | 3742 | `			/* Link to the top active VM frame */` |
|   1240144 | 3743 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   1240144 | 3744 | `			if( rc != SXRET_OK ){` |
|         - | 3745 | `				/* Return the slot to the free pool */` |
|       ! 0 | 3746 | `				sLocal.nIdx = nIdx;` |
|       ! 0 | 3747 | `				sLocal.pUserData = 0;` |
|       ! 0 | 3748 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|       ! 0 | 3749 | `				return 0;` |
|         - | 3750 | `			}` |
|   1240144 | 3751 | `			if( pFrame->pParent != 0 ){` |
|         - | 3752 | `				/* Local variable */` |
|   1225746 | 3753 | `				sLocal.nIdx = nIdx;` |
|   1225746 | 3754 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    628571 | 3755 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|         - | 3756 | `				/* Register in the $GLOBALS array */` |
|     14189 | 3757 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|      7092 | 3758 | `			}` |
|         - | 3759 | `			/* Install in the reference table */` |
|   1240144 | 3760 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 3761 | `			/* Save object index */` |
|   1240144 | 3762 | `			pObj->nIdx = nIdx;` |
|    621372 | 3763 | `		}else{` |
|         - | 3764 | `			/* Extract variable contents */` |
|  16067710 | 3765 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  16067710 | 3766 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  16067710 | 3767 | `			if( bNullify && pObj ){` |
|         3 | 3768 | `				PH7_MemObjRelease(pObj);` |
|         1 | 3769 | `			}` |
|         - | 3770 | `		}` |
|   8664719 | 3771 | `	}else{` |
|         - | 3772 | `		/* Superglobal */` |
|      2429 | 3773 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      2429 | 3774 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 3775 | `	}` |
|  17310273 | 3776 | `	return pObj;` |
|   8672006 | 3777 | `}` |
|         - | 3778 | `/*` |
|         - | 3779 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 3780 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 3781 | ` */` |
|     45538 | 3782 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 3783 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 3784 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 3785 | `	sxu32 nByte        /* zName length */` |
|         - | 3786 | `	)` |
|         5 | 3787 | `{` |
|         - | 3788 | `	SyHashEntry *pEntry;` |
|         - | 3789 | `	ph7_value *pValue;` |
|         - | 3790 | `	sxu32 nIdx;` |
|         - | 3791 | `	/* Query the superglobal table */` |
|     45543 | 3792 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     45543 | 3793 | `	if( pEntry == 0 ){` |
|         - | 3794 | `		/* No such entry */` |
|       ! 0 | 3795 | `		return 0;` |
|         - | 3796 | `	}` |
|         - | 3797 | `	/* Extract the superglobal index in the global object pool */` |
|     45543 | 3798 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3799 | `	/* Extract the variable value  */` |
|     45543 | 3800 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     45543 | 3801 | `	return pValue;` |
|     22774 | 3802 | `}` |
|         - | 3803 | `/*` |
|         - | 3804 | ` * Perform a raw hashmap insertion.` |
|         - | 3805 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 3806 | ` */` |
|     35520 | 3807 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 3808 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 3809 | `	const char *zKey,   /* Entry key */` |
|         - | 3810 | `	int nKeylen,        /* zKey length*/` |
|         - | 3811 | `	const char *zData,  /* Entry data */` |
|         - | 3812 | `	int nLen            /* zData length */` |
|         - | 3813 | `	)` |
|         5 | 3814 | `{` |
|         - | 3815 | `	ph7_value sKey,sValue;` |
|         - | 3816 | `	sxi32 rc;` |
|     35525 | 3817 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     35525 | 3818 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     35525 | 3819 | `	if( zKey ){` |
|     30523 | 3820 | `		if( nKeylen < 0 ){` |
|     30349 | 3821 | `			nKeylen = (int)SyStrlen(zKey);` |
|     15172 | 3822 | `		}` |
|     30523 | 3823 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     15259 | 3824 | `	}` |
|     35525 | 3825 | `	if( zData ){` |
|     35525 | 3826 | `		if( nLen < 0 ){` |
|         - | 3827 | `			/* Compute length automatically */` |
|     20121 | 3828 | `			nLen = (int)SyStrlen(zData);` |
|     10058 | 3829 | `		}` |
|     35525 | 3830 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     17760 | 3831 | `	}` |
|         - | 3832 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 3833 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 3834 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 3835 | `	 * every argument under "". */` |
|     35525 | 3836 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     35525 | 3837 | `	PH7_MemObjRelease(&sKey);` |
|     35525 | 3838 | `	PH7_MemObjRelease(&sValue);` |
|     35525 | 3839 | `	return rc;` |
|         5 | 3840 | `}` |
|         - | 3841 | `/*` |
|         - | 3842 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 3843 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 3844 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 3845 | ` */` |
|        34 | 3846 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 3847 | `{` |
|        38 | 3848 | `	sxi64 iVal = 0;` |
|        38 | 3849 | `	if( nValue == 0 ){` |
|       ! 0 | 3850 | `		return 0;` |
|         - | 3851 | `	}` |
|        34 | 3852 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 3853 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 3854 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 3855 | `		return 1;` |
|         - | 3856 | `	}` |
|        38 | 3857 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 3858 | `	return iVal != 0;` |
|        21 | 3859 | `}` |
|         - | 3860 | `/*` |
|         - | 3861 | ` * Configure a working virtual machine instance.` |
|         - | 3862 | ` *` |
|         - | 3863 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 3864 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 3865 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 3866 | ` * The second argument to this function is an integer configuration option` |
|         - | 3867 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 3868 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 3869 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 3870 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 3871 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 3872 | ` */` |
|    139898 | 3873 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 3874 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3875 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 3876 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 3877 | `	)` |
|         5 | 3878 | `{` |
|    139903 | 3879 | `	sxi32 rc = SXRET_OK;` |
|    139903 | 3880 | `	switch(nOp){` |
|      2463 | 3881 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      4931 | 3882 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4931 | 3883 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3884 | `		/* VM output consumer callback */` |
|         - | 3885 | `#ifdef UNTRUST` |
|         - | 3886 | `		if( xConsumer == 0 ){` |
|         - | 3887 | `			rc = SXERR_CORRUPT;` |
|         - | 3888 | `			break;` |
|         - | 3889 | `		}` |
|         - | 3890 | `#endif` |
|         - | 3891 | `		/* Install the output consumer */` |
|      4931 | 3892 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      4931 | 3893 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      4931 | 3894 | `		break;` |
|         - | 3895 | `							   }` |
|      2463 | 3896 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      4931 | 3897 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4931 | 3898 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3899 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 3900 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 3901 | `		 * instead of the program-output stream. */` |
|         - | 3902 | `#ifdef UNTRUST` |
|         - | 3903 | `		if( xConsumer == 0 ){` |
|         - | 3904 | `			rc = SXERR_CORRUPT;` |
|         - | 3905 | `			break;` |
|         - | 3906 | `		}` |
|         - | 3907 | `#endif` |
|      4931 | 3908 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      4931 | 3909 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      4931 | 3910 | `		break;` |
|         - | 3911 | `								   }` |
|      2479 | 3912 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 3913 | `		/* Import path */` |
|         - | 3914 | `		  const char *zPath;` |
|         - | 3915 | `		  SyString sPath;` |
|      4963 | 3916 | `		  zPath = va_arg(ap,const char *);` |
|         - | 3917 | `#if defined(UNTRUST)` |
|         - | 3918 | `		  if( zPath == 0 ){` |
|         - | 3919 | `			  rc = SXERR_EMPTY;` |
|         - | 3920 | `			  break;` |
|         - | 3921 | `		  }` |
|         - | 3922 | `#endif` |
|      4963 | 3923 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 3924 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 3925 | `#ifdef __WINNT__` |
|         5 | 3926 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 3927 | `#endif` |
|      9921 | 3928 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 3929 | `		  /* Remove leading and trailing white spaces */` |
|      4963 | 3930 | `		  SyStringFullTrim(&sPath);` |
|      4963 | 3931 | `		  if( sPath.nByte > 0 ){` |
|         - | 3932 | `			  /* Store the path in the corresponding conatiner */` |
|      4963 | 3933 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      2479 | 3934 | `		  }` |
|      4963 | 3935 | `		  break;` |
|         - | 3936 | `									 }` |
|      2486 | 3937 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 3938 | `		/* Run-Time Error report */` |
|      4977 | 3939 | `		pVm->bErrReport = 1;` |
|      4977 | 3940 | `		pVm->iErrMask = PH7_E_ALL_MASK; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      4977 | 3941 | `		break;` |
|         2 | 3942 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 3943 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 3944 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 3945 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 3946 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 3947 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 3948 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 3949 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 3950 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 3951 | `		int nDepth = va_arg(ap,int);` |
|         5 | 3952 | `		if( nDepth >= 0 ){` |
|         5 | 3953 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 3954 | `		}` |
|         5 | 3955 | `		break;` |
|         - | 3956 | `									   }` |
|         5 | 3957 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 3958 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 3959 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 3960 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 3961 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 3962 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 3963 | `		 * so it is rejected as a footgun). */` |
|        12 | 3964 | `		int nDepth = va_arg(ap,int);` |
|        12 | 3965 | `		if( nDepth > 1 ){` |
|        12 | 3966 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 3967 | `		}` |
|        12 | 3968 | `		break;` |
|         - | 3969 | `									   }` |
|       ! 0 | 3970 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 3971 | `		/* VM output length in bytes */` |
|       ! 0 | 3972 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 3973 | `#ifdef UNTRUST` |
|         - | 3974 | `		if( pOut == 0 ){` |
|         - | 3975 | `			rc = SXERR_CORRUPT;` |
|         - | 3976 | `			break;` |
|         - | 3977 | `		}` |
|         - | 3978 | `#endif` |
|       ! 0 | 3979 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 3980 | `		break;` |
|         - | 3981 | `							   }` |
|         - | 3982 |  |
|     27353 | 3983 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 3984 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 3985 | `		/* Create a new superglobal/global variable */` |
|     54711 | 3986 | `		const char *zName = va_arg(ap,const char *);` |
|     54711 | 3987 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 3988 | `		SyHashEntry *pEntry;` |
|         - | 3989 | `		ph7_value *pObj;` |
|         - | 3990 | `		sxu32 nByte;` |
|         - | 3991 | `		sxu32 nIdx;` |
|         - | 3992 | `#ifdef UNTRUST` |
|         - | 3993 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 3994 | `			rc = SXERR_CORRUPT;` |
|         - | 3995 | `			break;` |
|         - | 3996 | `		}` |
|         - | 3997 | `#endif` |
|     54711 | 3998 | `		nByte = SyStrlen(zName);` |
|     54711 | 3999 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 4000 | `			/* Check if the superglobal is already installed */` |
|     49785 | 4001 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     24895 | 4002 | `		}else{` |
|         - | 4003 | `			/* Query the top active VM frame */` |
|      4931 | 4004 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 4005 | `		}` |
|     54711 | 4006 | `		if( pEntry ){` |
|         - | 4007 | `			/* Variable already installed */` |
|       ! 0 | 4008 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 4009 | `			/* Extract contents */` |
|       ! 0 | 4010 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 4011 | `			if( pObj ){` |
|         - | 4012 | `				/* Overwrite old contents */` |
|       ! 0 | 4013 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 4014 | `			}` |
|       ! 0 | 4015 | `		}else{` |
|         - | 4016 | `			/* Install a new variable */` |
|     54711 | 4017 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     54711 | 4018 | `			if( pObj == 0 ){` |
|       ! 0 | 4019 | `				rc = SXERR_MEM;` |
|       ! 0 | 4020 | `				break;` |
|         - | 4021 | `			}` |
|     54711 | 4022 | `			nIdx = pObj->nIdx;` |
|         - | 4023 | `			/* Copy value */` |
|     54711 | 4024 | `			PH7_MemObjStore(pValue,pObj);` |
|     54711 | 4025 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 4026 | `				/* Install the superglobal */` |
|     49785 | 4027 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     24895 | 4028 | `			}else{` |
|         - | 4029 | `				/* Install in the current frame */` |
|      4931 | 4030 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 4031 | `			}` |
|     54711 | 4032 | `			if( rc == SXRET_OK ){` |
|         - | 4033 | `				SyHashEntry *pRef;` |
|     54711 | 4034 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     49785 | 4035 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     24895 | 4036 | `				}else{` |
|      4931 | 4037 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 4038 | `				}` |
|         - | 4039 | `				/* Install in the reference table */` |
|     54711 | 4040 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     54711 | 4041 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 4042 | `					/* Register in the $GLOBALS array */` |
|     54711 | 4043 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     27353 | 4044 | `				}` |
|     27353 | 4045 | `			}` |
|         - | 4046 | `		}` |
|     54711 | 4047 | `		break;` |
|         - | 4048 | `									}` |
|     15172 | 4049 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 4050 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 4051 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 4052 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 4053 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 4054 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 4055 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     30349 | 4056 | `		const char *zKey   = va_arg(ap,const char *);` |
|     30349 | 4057 | `		const char *zValue = va_arg(ap,const char *);` |
|     30349 | 4058 | `		int nLen = va_arg(ap,int);` |
|         - | 4059 | `		ph7_hashmap *pMap;` |
|         - | 4060 | `		ph7_value *pValue;` |
|     30349 | 4061 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 4062 | `			/* Extract the $_ENV superglobal */` |
|       ! 0 | 4063 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     30349 | 4064 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 4065 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 4066 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     30349 | 4067 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 4068 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 4069 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     30349 | 4070 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 4071 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 4072 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     30349 | 4073 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 4074 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 4075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     30349 | 4076 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 4077 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 4078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 4079 | `		}else{` |
|         - | 4080 | `			/* Extract the $_SERVER superglobal */` |
|     30349 | 4081 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 4082 | `		}` |
|     30349 | 4083 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 4084 | `			/* No such entry */` |
|       ! 0 | 4085 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4086 | `			break;` |
|         - | 4087 | `		}` |
|         - | 4088 | `		/* Point to the hashmap */` |
|     30349 | 4089 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 4090 | `		/* Perform the insertion */` |
|     30349 | 4091 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     30349 | 4092 | `		break;` |
|         - | 4093 | `								   }` |
|      2501 | 4094 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 4095 | `		/* Script arguments */` |
|      5007 | 4096 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 4097 | `		ph7_hashmap *pMap;` |
|         - | 4098 | `		ph7_value *pValue;` |
|         - | 4099 | `		sxu32 n;` |
|         - | 4100 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 4101 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 4102 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 4103 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      5007 | 4104 | `		if( zValue == 0 ){` |
|       ! 0 | 4105 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4106 | `			break;` |
|         - | 4107 | `		}` |
|         - | 4108 | `		/* Extract the $argv array */` |
|      5007 | 4109 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      5007 | 4110 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 4111 | `			/* No such entry */` |
|       ! 0 | 4112 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4113 | `			break;` |
|         - | 4114 | `		}` |
|         - | 4115 | `		/* Point to the hashmap */` |
|      5007 | 4116 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 4117 | `		/* Perform the insertion */` |
|      5007 | 4118 | `		n = (sxu32)SyStrlen(zValue);` |
|      5007 | 4119 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      5007 | 4120 | `		break;` |
|         - | 4121 | `								  }` |
|      2463 | 4122 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 4123 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 4124 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 4125 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 4126 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 4127 | `		ph7_value *pArgv,*pServer;` |
|         - | 4128 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 4129 | `		ph7_value sArgvVal,sKey,sCount;` |
|      4931 | 4130 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4931 | 4131 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      4926 | 4132 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      4931 | 4133 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 4134 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4135 | `			break;` |
|         - | 4136 | `		}` |
|      4931 | 4137 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      4931 | 4138 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 4139 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      4931 | 4140 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      4931 | 4141 | `		if( pDup == 0 ){` |
|       ! 0 | 4142 | `			rc = SXERR_MEM;` |
|       ! 0 | 4143 | `			break;` |
|         - | 4144 | `		}` |
|      4931 | 4145 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      4931 | 4146 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      4931 | 4147 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4931 | 4148 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      4931 | 4149 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      4931 | 4150 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      4931 | 4151 | `		PH7_MemObjRelease(&sKey);` |
|         - | 4152 | `		/* $_SERVER['argc'] = count($argv). */` |
|      4931 | 4153 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      4931 | 4154 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4931 | 4155 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      4931 | 4156 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      4931 | 4157 | `		PH7_MemObjRelease(&sCount);` |
|      4931 | 4158 | `		PH7_MemObjRelease(&sKey);` |
|      4931 | 4159 | `		rc = SXRET_OK;` |
|      4931 | 4160 | `		break;` |
|         - | 4161 | `								  }` |
|        54 | 4162 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 4163 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 4164 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 4165 | `		 * apply immediately so they take effect even if the script never` |
|         - | 4166 | `		 * touches the INI API. */` |
|       112 | 4167 | `		const char *zName = va_arg(ap,const char *);` |
|       112 | 4168 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 4169 | `		VmIniEntry sEntry;` |
|         - | 4170 | `		char *zDupN,*zDupV;` |
|         - | 4171 | `		sxu32 nName,nValue;` |
|       112 | 4172 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 4173 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4174 | `			break;` |
|         - | 4175 | `		}` |
|       112 | 4176 | `		if( zValue == 0 ){` |
|       ! 0 | 4177 | `			zValue = "";` |
|       ! 0 | 4178 | `		}` |
|       112 | 4179 | `		nName = (sxu32)SyStrlen(zName);` |
|       112 | 4180 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       112 | 4181 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       112 | 4182 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       112 | 4183 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 4184 | `			rc = SXERR_MEM;` |
|       ! 0 | 4185 | `			break;` |
|         - | 4186 | `		}` |
|       112 | 4187 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       112 | 4188 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       112 | 4189 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       112 | 4190 | `		if( rc == SXRET_OK ){` |
|       108 | 4191 | `			if( nName == sizeof("error_reporting")-1` |
|        80 | 4192 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         6 | 4193 | `				sxi64 iLevel = 0;` |
|         6 | 4194 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         6 | 4195 | `				pVm->bErrReport = iLevel != 0;` |
|       105 | 4196 | `			}else if( nName == sizeof("date.timezone")-1` |
|        51 | 4197 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|       ! 0 | 4198 | `			 && nValue == 3` |
|         4 | 4199 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 4200 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 4201 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 4202 | `				pVm->nDefTz = 3;` |
|       102 | 4203 | `			}else if( nName == sizeof("zend.assertions")-1` |
|        92 | 4204 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 4205 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 4206 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 4207 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 4208 | `				sxi64 iZend = 0;` |
|        40 | 4209 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 4210 | `				if( iZend >= 1 ){` |
|        40 | 4211 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 4212 | `				}else{` |
|       ! 0 | 4213 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 4214 | `				}` |
|        88 | 4215 | `			}else if( nName == sizeof("display_errors")-1` |
|        46 | 4216 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 4217 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 4218 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 4219 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 4220 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        61 | 4221 | `			}else if( nName == sizeof("log_errors")-1` |
|        36 | 4222 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 4223 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|        44 | 4224 | `			}else if( nName == sizeof("include_path")-1` |
|        26 | 4225 | `			 && SyMemcmp(zName,"include_path",nName) == 0` |
|        16 | 4226 | `			 && nValue > 0 ){` |
|         - | 4227 | `				/* The path SET is the store this directive names, and the INI` |
|         - | 4228 | ``				 * chunk's seed is lazy -- so `-d include_path=…` has to reach it`` |
|         - | 4229 | `				 * here or a script that never touches the INI API keeps looking` |
|         - | 4230 | `				 * in the default directory. Empty is refused, as php's` |
|         - | 4231 | `				 * OnUpdateStringUnempty refuses it. */` |
|         9 | 4232 | `				PH7_VmSetIncludePath(pVm,zValue,nValue);` |
|         4 | 4233 | `			}` |
|        54 | 4234 | `		}` |
|       112 | 4235 | `		break;` |
|         - | 4236 | `								  }` |
|       ! 0 | 4237 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 4238 | `		/* error_log() consumer */` |
|       ! 0 | 4239 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 4240 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 4241 | `		break;` |
|         - | 4242 | `										}` |
|       ! 0 | 4243 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 4244 | `		/* Script return value */` |
|       ! 0 | 4245 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 4246 | `#ifdef UNTRUST` |
|         - | 4247 | `		if( ppValue == 0 ){` |
|         - | 4248 | `			rc = SXERR_CORRUPT;` |
|         - | 4249 | `			break;` |
|         - | 4250 | `		}` |
|         - | 4251 | `#endif` |
|       ! 0 | 4252 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 4253 | `		break;` |
|         - | 4254 | `								   }` |
|     12416 | 4255 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 4256 | `		/* Register an IO stream device */` |
|     24837 | 4257 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 4258 | `		/* Make sure we are dealing with a valid IO stream. A wrapper has to be` |
|         - | 4259 | `		 * able to do ONE of the two things a wrapper does -- open a byte stream` |
|         - | 4260 | `		 * or open a directory. php's glob:// is a dir_opener and nothing else,` |
|         - | 4261 | ``		 * and demanding xOpen here would leave `opendir('glob://…')` with no`` |
|         - | 4262 | `		 * device to reach. */` |
|     27318 | 4263 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     24832 | 4264 | `			((pStream->xOpen == 0 \|\| pStream->xRead == 0) && pStream->xOpenDir == 0) ){` |
|         - | 4265 | `				/* Invalid stream */` |
|       ! 0 | 4266 | `				rc = SXERR_INVALID;` |
|       ! 0 | 4267 | `				break;` |
|         - | 4268 | `		}` |
|     24837 | 4269 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 4270 | `			/* Make the 'file://' stream the defaut stream device */` |
|      4967 | 4271 | `			pVm->pDefStream = pStream;` |
|      2481 | 4272 | `		}` |
|         - | 4273 | `		/* Insert in the appropriate container */` |
|     24837 | 4274 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     24837 | 4275 | `		break;` |
|         - | 4276 | `								  }` |
|        23 | 4277 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 4278 | `		/* Point to the VM internal output consumer buffer */` |
|        46 | 4279 | `		const void **ppOut = va_arg(ap,const void **);` |
|        46 | 4280 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 4281 | `#ifdef UNTRUST` |
|         - | 4282 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 4283 | `			rc = SXERR_CORRUPT;` |
|         - | 4284 | `			break;` |
|         - | 4285 | `		}` |
|         - | 4286 | `#endif` |
|        46 | 4287 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        46 | 4288 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        46 | 4289 | `		break;` |
|         - | 4290 | `									   }` |
|        23 | 4291 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 4292 | `		/* Raw HTTP request*/` |
|        46 | 4293 | `		const char *zRequest = va_arg(ap,const char *);` |
|        46 | 4294 | `		int nByte = va_arg(ap,int);` |
|        46 | 4295 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 4296 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4297 | `			break;` |
|         - | 4298 | `		}` |
|        46 | 4299 | `		if( nByte < 0 ){` |
|         - | 4300 | `			/* Compute length automatically */` |
|       ! 0 | 4301 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 4302 | `		}` |
|         - | 4303 | `		/* Process the request */` |
|        46 | 4304 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 4305 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        46 | 4306 | `		if( rc == SXRET_OK ){` |
|        44 | 4307 | `			pVm->bHttpContext = 1;` |
|        22 | 4308 | `		}` |
|        46 | 4309 | `		break;` |
|         - | 4310 | `									}` |
|        23 | 4311 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 4312 | `		/* Extract HTTP response status code */` |
|        46 | 4313 | `		int *pStatus = va_arg(ap, int *);` |
|        46 | 4314 | `		if( pStatus ){` |
|        46 | 4315 | `			*pStatus = pVm->iResponseStatus;` |
|        23 | 4316 | `		}` |
|        46 | 4317 | `		break;` |
|         - | 4318 | `										}` |
|        23 | 4319 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 4320 | `		/* Iterate response headers via callback */` |
|         - | 4321 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        46 | 4322 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        46 | 4323 | `		void *pUserData = va_arg(ap, void *);` |
|        46 | 4324 | `		if( xCallback ){` |
|        46 | 4325 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        46 | 4326 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       112 | 4327 | `			for( k = 0; k < nHdr; k++ ){` |
|        99 | 4328 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        66 | 4329 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        33 | 4330 | `							   pUserData);` |
|        66 | 4331 | `				if( rc != PH7_OK ){` |
|       ! 0 | 4332 | `					break;` |
|         - | 4333 | `				}` |
|        33 | 4334 | `			}` |
|        23 | 4335 | `		}` |
|        46 | 4336 | `		break;` |
|         - | 4337 | `										 }` |
|       ! 0 | 4338 | `	default:` |
|         - | 4339 | `		/* Unknown configuration option */` |
|       ! 0 | 4340 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 4341 | `		break;` |
|         - | 4342 | `	}` |
|    139903 | 4343 | `	return rc;` |
|         5 | 4344 | `}` |
|         - | 4345 | `/* Forward declaration */` |
|         - | 4346 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 4347 | `/*` |
|         - | 4348 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 4349 | ` * format.` |
|         - | 4350 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 4351 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 4352 | ` * (STDOUT).` |
|         - | 4353 | ` */` |
|         2 | 4354 | `static sxi32 VmByteCodeDump(` |
|         - | 4355 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 4356 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 4357 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 4358 | `	)` |
|         1 | 4359 | `{` |
|         - | 4360 | `	static const char zDump[] = {` |
|         - | 4361 | `		"====================================================\n"` |
|         - | 4362 | `		"PH7 VM Dump\n"` |
|         - | 4363 | `		"====================================================\n"` |
|         - | 4364 | `	};` |
|         - | 4365 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 4366 | `	sxi32 rc = SXRET_OK;` |
|         - | 4367 | `	sxu32 n;` |
|         - | 4368 | `	/* Point to the PH7 instructions */` |
|         3 | 4369 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 4370 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 4371 | `	n = 0;` |
|         3 | 4372 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 4373 | `	/* Dump instructions */` |
|         6 | 4374 | `	for(;;){` |
|        13 | 4375 | `		if( pInstr >= pEnd ){` |
|         - | 4376 | `			/* No more instructions */` |
|         3 | 4377 | `			break;` |
|         - | 4378 | `		}` |
|         - | 4379 | `		/* Format and call the consumer callback */` |
|        16 | 4380 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 | 4381 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 | 4382 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 | 4383 | `		if( rc != SXRET_OK ){` |
|         - | 4384 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 4385 | `			return rc;` |
|         - | 4386 | `		}` |
|        11 | 4387 | `		++n;` |
|        11 | 4388 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 4389 | `	}` |
|         3 | 4390 | `	return rc;` |
|         2 | 4391 | `}` |
|         - | 4392 | `/*` |
|         - | 4393 | ` * Save the execution state of a fiber/generator context.` |
|         - | 4394 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 4395 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 4396 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 4397 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 4398 | ` * when VmByteCodeExec returns.` |
|         - | 4399 | ` */` |
|      1768 | 4400 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 4401 | `	ph7_vm *pVm,` |
|         - | 4402 | `	ph7_exec_ctx *pCtx,` |
|         - | 4403 | `	sxi32 pc,` |
|         - | 4404 | `	sxi32 nTos` |
|         - | 4405 | `	)` |
|         5 | 4406 | `{` |
|       884 | 4407 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1773 | 4408 | `	pCtx->pc = pc;` |
|      1773 | 4409 | `	pCtx->nTos = nTos;` |
|      1773 | 4410 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1773 | 4411 | `	return PH7_SUSPEND;` |
|         5 | 4412 | `}` |
|         - | 4413 | `/*` |
|         - | 4414 | ` * Resolve named-argument mapping.` |
|         - | 4415 | ` *` |
|         - | 4416 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 4417 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 4418 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 4419 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 4420 | ` * every formal parameter that received a value.` |
|         - | 4421 | ` *` |
|         - | 4422 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 4423 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 4424 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 4425 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 4426 | ` */` |
|       398 | 4427 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 4428 | `	ph7_vm *pVm,` |
|         - | 4429 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 4430 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 4431 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 4432 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 4433 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 4434 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 4435 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 4436 | `)` |
|         5 | 4437 | `{` |
|       403 | 4438 | `	sxi32 posIdx = 0;` |
|         - | 4439 | `	sxu32 i;` |
|       403 | 4440 | `	int bSeenNamed = 0;` |
|         - | 4441 | `	char zErrMsg[256];` |
|       403 | 4442 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1401 | 4443 | `	for( i = 0; i < nActual; i++ ){` |
|      1003 | 4444 | `		aSlot[i] = -2;` |
|       504 | 4445 | `	}` |
|      1389 | 4446 | `	for( i = 0; i < nActual; i++ ){` |
|      1314 | 4447 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 4448 | `			/* Named argument — find formal by name */` |
|       639 | 4449 | `			int found = 0;` |
|       639 | 4450 | `			bSeenNamed = 1;` |
|         - | 4451 | `			sxu32 k;` |
|       949 | 4452 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       748 | 4453 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       726 | 4454 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       694 | 4455 | `						pMap->aNames[i].zString,` |
|      1041 | 4456 | `						pMap->aNames[i].nByte) == 0 ){` |
|       443 | 4457 | `					if( aUsed[k] ){` |
|        12 | 4458 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4459 | `							"Named parameter $%.*s overwrites previous argument",` |
|         6 | 4460 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         9 | 4461 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4462 | `					}` |
|       437 | 4463 | `					aSlot[i] = (sxi32)k;` |
|       437 | 4464 | `					aUsed[k] = 1;` |
|       437 | 4465 | `					found = 1;` |
|       437 | 4466 | `					break;` |
|         - | 4467 | `				}` |
|       160 | 4468 | `			}` |
|       633 | 4469 | `			if( !found ){` |
|       201 | 4470 | `				if( iVariadicIdx >= 0 ){` |
|       194 | 4471 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        99 | 4472 | `				}else{` |
|        11 | 4473 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4474 | `						"Unknown named parameter $%.*s",` |
|         6 | 4475 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         8 | 4476 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4477 | `				}` |
|        95 | 4478 | `			}` |
|       316 | 4479 | `		}else{` |
|         - | 4480 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 4481 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 4482 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 4483 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       368 | 4484 | `			if( bSeenNamed ){` |
|       ! 0 | 4485 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 4486 | `					"Cannot use positional argument after named argument",` |
|         - | 4487 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 4488 | `			}` |
|       368 | 4489 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        62 | 4490 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 4491 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4492 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 4493 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 4494 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4495 | `				}` |
|        62 | 4496 | `				aSlot[i] = posIdx;` |
|        62 | 4497 | `				aUsed[posIdx] = 1;` |
|       338 | 4498 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 4499 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 4500 | `			}` |
|       368 | 4501 | `			posIdx++;` |
|         - | 4502 | `		}` |
|       498 | 4503 | `	}` |
|       391 | 4504 | `	return SXRET_OK;` |
|       204 | 4505 | `}` |
|         - | 4506 | `/*` |
|         - | 4507 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 4508 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 4509 | ` */` |
|      1158 | 4510 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         5 | 4511 | `{` |
|      1163 | 4512 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      1149 | 4513 | `		return 0;` |
|         - | 4514 | `	}` |
|        17 | 4515 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       584 | 4516 | `}` |
|         - | 4517 | `/*` |
|         - | 4518 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 4519 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 4520 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 4521 | ` */` |
|        10 | 4522 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4523 | `{` |
|        11 | 4524 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|         5 | 4525 | `	(void)pVm;` |
|        11 | 4526 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|        11 | 4527 | `	return SXRET_OK;` |
|         1 | 4528 | `}` |
|         - | 4529 | `/*` |
|         - | 4530 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 4531 | ` * collect values positionally (keys ignored) into a temp array.` |
|         - | 4532 | ` */` |
|         6 | 4533 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4534 | `{` |
|         3 | 4535 | `	(void)pVm; (void)pKey;` |
|         7 | 4536 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|         7 | 4537 | `	return SXRET_OK;` |
|         1 | 4538 | `}` |
|         - | 4539 | `/*` |
|         - | 4540 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 4541 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 4542 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 4543 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 4544 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 4545 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 4546 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 4547 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 4548 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 4549 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 4550 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 4551 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 4552 | ` */` |
|         - | 4553 | `/*` |
|         - | 4554 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 4555 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 4556 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 4557 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 4558 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 4559 | ` */` |
|      1134 | 4560 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         4 | 4561 | `{` |
|         - | 4562 | `	VmSpreadRun sRun;` |
|         - | 4563 | `	ph7_hashmap_node *pNode;` |
|         - | 4564 | `	sxu32 i;` |
|      1138 | 4565 | `	sRun.pStart = pFirst;` |
|      1138 | 4566 | `	sRun.nCount = nCount;` |
|      1138 | 4567 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      1138 | 4568 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      1138 | 4569 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 4570 | `		return;` |
|         - | 4571 | `	}` |
|      1138 | 4572 | `	pNode = pMap->pFirst;` |
|      3958 | 4573 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 4574 | `		VmSpreadKey sKey;` |
|      2824 | 4575 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 4576 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 4577 | `			 * the source map's release before CALL replays them. */` |
|       101 | 4578 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       101 | 4579 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       101 | 4580 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        51 | 4581 | `		}else{` |
|         - | 4582 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      2724 | 4583 | `			sKey.nOff = 0;` |
|      2724 | 4584 | `			sKey.nLen = 0;` |
|         - | 4585 | `		}` |
|      2824 | 4586 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      2824 | 4587 | `		pNode = pNode->pPrev; /* forward link */` |
|      1414 | 4588 | `	}` |
|       571 | 4589 | `}` |
|         - | 4590 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 4591 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        16 | 4592 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 4593 | `{` |
|        16 | 4594 | `	SySetReset(&pVm->aSpreadRun);` |
|        16 | 4595 | `	SySetReset(&pVm->aSpreadKey);` |
|        16 | 4596 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        16 | 4597 | `}` |
|         - | 4598 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 4599 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 4600 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 4601 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 4602 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 4603 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 4604 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 4605 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 4606 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 4607 | ` * slot — from being consumed by that nested call. */` |
|      2154 | 4608 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         4 | 4609 | `{` |
|      2158 | 4610 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      2158 | 4611 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 4612 | `	VmSpreadRun *aRun;` |
|      2158 | 4613 | `	if( rStart >= nRun ){` |
|      1038 | 4614 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 4615 | `	}` |
|      1124 | 4616 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1124 | 4617 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      1124 | 4618 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      1124 | 4619 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       560 | 4620 | `	}` |
|      1124 | 4621 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|      1081 | 4622 | `}` |
|         - | 4623 | `/*` |
|         - | 4624 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 4625 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 4626 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 4627 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 4628 | ` *` |
|         - | 4629 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 4630 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 4631 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 4632 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 4633 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 4634 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 4635 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 4636 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 4637 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 4638 | ` *` |
|         - | 4639 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 4640 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 4641 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 4642 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 4643 | ` */` |
|      2736 | 4644 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         4 | 4645 | `{` |
|      2740 | 4646 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4647 | `	VmSpreadRun *aRun;` |
|      2740 | 4648 | `	ph7_value *pEnd = pTos;` |
|      2740 | 4649 | `	sxi32 nPos = iP1;` |
|      2740 | 4650 | `	sxi32 ri, extra = 0;` |
|      2740 | 4651 | `	if( nRun == 0 ){` |
|        15 | 4652 | `		pVm->nSpreadCallBase = 0;` |
|        15 | 4653 | `		return 0;` |
|         - | 4654 | `	}` |
|      2726 | 4655 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      2726 | 4656 | `	ri = (sxi32)nRun - 1;` |
|      8368 | 4657 | `	while( nPos > 0 ){` |
|      5646 | 4658 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 4659 | `			/* A non-empty unpack occupying nCount slots. */` |
|      2236 | 4660 | `			pEnd = aRun[ri].pStart;` |
|      2236 | 4661 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      2236 | 4662 | `			ri--;` |
|      4530 | 4663 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 4664 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       495 | 4665 | `			extra -= 1;` |
|       495 | 4666 | `			ri--;` |
|       249 | 4667 | `		}else{` |
|         - | 4668 | `			/* An ordinary single-slot argument. */` |
|      2922 | 4669 | `			pEnd--;` |
|         - | 4670 | `		}` |
|      5646 | 4671 | `		nPos--;` |
|         4 | 4672 | `	}` |
|         - | 4673 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 4674 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      2726 | 4675 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      2726 | 4676 | `	return extra;` |
|      1372 | 4677 | `}` |
|      1134 | 4678 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|         4 | 4679 | `{` |
|      1138 | 4680 | `	ph7_value *pTos = *ppTos;` |
|      1138 | 4681 | `	sxu32 nEntry = pMap->nEntry;` |
|      1138 | 4682 | `	if( nEntry == 0 ){` |
|         - | 4683 | `		/* Nothing to unpack — remove the source from the stack */` |
|       205 | 4684 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       205 | 4685 | `		VmPopOperand(&pTos, 1);` |
|       104 | 4686 | `	}else{` |
|         - | 4687 | `		ph7_hashmap_node *pNode;` |
|         - | 4688 | `		ph7_value *pElem;` |
|         - | 4689 | `		sxu32 i;` |
|         - | 4690 | `		int bTemp;` |
|         - | 4691 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|         - | 4692 | `		 * so a source array that other variables share has to separate first — otherwise` |
|         - | 4693 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|         - | 4694 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|         - | 4695 | `		 * ordinary spread pays nothing for this. */` |
|       932 | 4696 | `		if( bVarSource` |
|       852 | 4697 | `		 && pMap != pVm->pGlobal` |
|       772 | 4698 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|       776 | 4699 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|         - | 4700 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|         - | 4701 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|         - | 4702 | `			 * object. */` |
|       776 | 4703 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|       776 | 4704 | `			if( pSep ){` |
|       776 | 4705 | `				pMap = pSep;` |
|       776 | 4706 | `				nEntry = pMap->nEntry;` |
|       386 | 4707 | `			}` |
|       386 | 4708 | `		}` |
|       936 | 4709 | `		pMap->iRef++;` |
|       936 | 4710 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 4711 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 4712 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|       936 | 4713 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 4714 | `		/* Overwrite the source slot with the first element */` |
|       936 | 4715 | `		pNode = pMap->pFirst;` |
|       936 | 4716 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|       936 | 4717 | `		PH7_MemObjRelease(pTos);` |
|       936 | 4718 | `		if( pElem ){` |
|       936 | 4719 | `			if( bTemp ){` |
|       154 | 4720 | `				PH7_MemObjStore(pElem, pTos);` |
|        78 | 4721 | `			}else{` |
|       784 | 4722 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 4723 | `			}` |
|       466 | 4724 | `		}` |
|         - | 4725 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|         - | 4726 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|         - | 4727 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|         - | 4728 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|         - | 4729 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|         - | 4730 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|         - | 4731 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|         - | 4732 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|       936 | 4733 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|       936 | 4734 | `		if( !bVarSource \|\| bTemp ){` |
|       162 | 4735 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|        80 | 4736 | `		}` |
|         - | 4737 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 4738 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|       936 | 4739 | `		pNode = pNode->pPrev;` |
|         - | 4740 | `		/* Push the remaining elements */` |
|      2824 | 4741 | `		for( i = 1; i < nEntry; i++ ){` |
|      1892 | 4742 | `			pTos++;` |
|      1892 | 4743 | `			PH7_MemObjInit(pVm, pTos);` |
|      1892 | 4744 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1892 | 4745 | `			if( pElem ){` |
|      1892 | 4746 | `				if( bTemp ){` |
|      1289 | 4747 | `					PH7_MemObjStore(pElem, pTos);` |
|       645 | 4748 | `				}else{` |
|       604 | 4749 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 4750 | `				}` |
|       944 | 4751 | `			}` |
|      1892 | 4752 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|      1892 | 4753 | `			if( !bVarSource \|\| bTemp ){` |
|      1289 | 4754 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       644 | 4755 | `			}` |
|      1892 | 4756 | `			pNode = pNode->pPrev;` |
|       948 | 4757 | `		}` |
|       936 | 4758 | `		PH7_HashmapUnref(pMap);` |
|         - | 4759 | `	}` |
|      1138 | 4760 | `	*ppTos = pTos;` |
|      1138 | 4761 | `}` |
|         - | 4762 | `/*` |
|         - | 4763 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 4764 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 4765 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 4766 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 4767 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 4768 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 4769 | ` *` |
|         - | 4770 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 4771 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 4772 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 4773 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 4774 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 4775 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 4776 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 4777 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 4778 | ` * method-name slot pop shifts pArg).` |
|         - | 4779 | ` *` |
|         - | 4780 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 4781 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 4782 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 4783 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 4784 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 4785 | ` */` |
|      1096 | 4786 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 4787 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         4 | 4788 | `{` |
|      1100 | 4789 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4790 | `	VmSpreadRun *aRun;` |
|         - | 4791 | `	VmSpreadKey *aKey;` |
|         - | 4792 | `	const char *zKeyBase;` |
|      1100 | 4793 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|      1100 | 4794 | `	int bAnyNamed = 0;` |
|         - | 4795 | `	sxu32 ai, ci, ri, rStart;` |
|      1100 | 4796 | `	if( nRun == 0 ){` |
|         - | 4797 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 4798 | `		return 0;` |
|         - | 4799 | `	}` |
|      1100 | 4800 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1100 | 4801 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      1100 | 4802 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 4803 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 4804 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 4805 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 4806 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      1100 | 4807 | `	ri = pVm->nSpreadCallBase;` |
|      1100 | 4808 | `	rStart = ri;` |
|      1100 | 4809 | `	if( rStart >= nRun ){` |
|         - | 4810 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 4811 | `		return 0;` |
|         - | 4812 | `	}` |
|      1100 | 4813 | `	SySetReset(&pVm->aEffArgName);` |
|      1100 | 4814 | `	ci = 0;` |
|      1100 | 4815 | `	ai = 0;` |
|      3140 | 4816 | `	while( ai < nActual ){` |
|         - | 4817 | `		SyString sName;` |
|      2044 | 4818 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 4819 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 4820 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      2056 | 4821 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 4822 | `			ci++; ri++;` |
|         1 | 4823 | `		}` |
|      2044 | 4824 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 4825 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 4826 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 4827 | `			 * run never desyncs the key stream. */` |
|       936 | 4828 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      3756 | 4829 | `			for( j = 0; j < K; j++ ){` |
|      2824 | 4830 | `				SyZero(&sName, sizeof(sName));` |
|      2824 | 4831 | `				if( aKey[ks + j].nLen > 0 ){` |
|       101 | 4832 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       101 | 4833 | `					bAnyNamed = 1;` |
|        50 | 4834 | `				}` |
|      2824 | 4835 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1414 | 4836 | `			}` |
|       936 | 4837 | `			ai += K;` |
|       936 | 4838 | `			ci++; ri++;` |
|       470 | 4839 | `		}else{` |
|         - | 4840 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      1112 | 4841 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        35 | 4842 | `				sName = pCompile->aNames[ci];` |
|        35 | 4843 | `				bAnyNamed = 1;` |
|        17 | 4844 | `			}` |
|      1112 | 4845 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1112 | 4846 | `			ai++;` |
|      1112 | 4847 | `			ci++;` |
|         - | 4848 | `		}` |
|         4 | 4849 | `	}` |
|         - | 4850 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 4851 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 4852 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 4853 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      1100 | 4854 | `	VmSpreadConsume(pVm);` |
|      1100 | 4855 | `	if( !bAnyNamed ){` |
|         - | 4856 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      1024 | 4857 | `		return 0;` |
|         - | 4858 | `	}` |
|        77 | 4859 | `	pEff->bHasNamed = 1;` |
|        77 | 4860 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|        77 | 4861 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|        77 | 4862 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|         - | 4863 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|         - | 4864 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|         - | 4865 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|         - | 4866 | `	 * is the caller's stack local. */` |
|        77 | 4867 | `	pEff->bArgShapes = 0;` |
|        77 | 4868 | `	pEff->nNonLvalMask = 0;` |
|        77 | 4869 | `	pEff->nTempCallMask = 0;` |
|        77 | 4870 | `	if( pCompile ){` |
|        37 | 4871 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        19 | 4872 | `	}else{` |
|        41 | 4873 | `		pEff->sAssertSrc.zString = 0;` |
|        41 | 4874 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 4875 | `	}` |
|        77 | 4876 | `	pEff->nTotal = nActual;` |
|        77 | 4877 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|        77 | 4878 | `	return 1;` |
|       552 | 4879 | `}` |
|         - | 4880 | `/*` |
|         - | 4881 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 4882 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 4883 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 4884 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 4885 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 4886 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 4887 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 4888 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 4889 | ` * pArg must be the site's FINAL argument base.` |
|         - | 4890 | ` */` |
|   8357447 | 4891 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 4892 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 4893 | `{` |
|   8357452 | 4894 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   8357452 | 4895 | `	if( pInstr->iP2 == 0 ){` |
|   8356318 | 4896 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 4897 | `	}` |
|      1138 | 4898 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|        77 | 4899 | `		return pStorage;` |
|         - | 4900 | `	}` |
|      1062 | 4901 | `	VmSpreadConsume(pVm);` |
|      1062 | 4902 | `	return pCompile;` |
|   4180477 | 4903 | `}` |
|         - | 4904 | `/*` |
|         - | 4905 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 4906 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 4907 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 4908 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 4909 | ` */` |
|        14 | 4910 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         2 | 4911 | `{` |
|        16 | 4912 | `	const char *zType = "unknown";` |
|         - | 4913 | `	char zMsg[64];` |
|        16 | 4914 | `	if( iFlags & MEMOBJ_STRING ){` |
|         6 | 4915 | `		zType = "string";` |
|        14 | 4916 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 4917 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 4918 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 4919 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 4920 | `		zType = "float";` |
|        12 | 4921 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        10 | 4922 | `		zType = "int";` |
|         7 | 4923 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 4924 | `		zType = "bool";` |
|         1 | 4925 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 4926 | `		zType = "object";` |
|       ! 0 | 4927 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 4928 | `		zType = "resource";` |
|       ! 0 | 4929 | `	}` |
|        16 | 4930 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        16 | 4931 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        16 | 4932 | `}` |
|         - | 4933 | `/*` |
|         - | 4934 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - | 4935 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - | 4936 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - | 4937 | ` * per context — see VmMemberCtxWantsValue.` |
|         - | 4938 | ` */` |
|     14366 | 4939 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 4940 | `{` |
|     14371 | 4941 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 | 4942 | `}` |
|         - | 4943 | `/*` |
|         - | 4944 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - | 4945 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - | 4946 | ` * isset() stops at the truth.` |
|         - | 4947 | ` */` |
|       160 | 4948 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 | 4949 | `{` |
|       163 | 4950 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 | 4951 | `}` |
|         - | 4952 | `/*` |
|         - | 4953 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 4954 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 4955 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 4956 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 4957 | ` * reads (other names / other instances) still dispatch.` |
|         - | 4958 | ` */` |
|      9667 | 4959 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4960 | `{` |
|         - | 4961 | `	VmMagicGuard *aG;` |
|         - | 4962 | `	sxu32 nHash;` |
|         - | 4963 | `	sxu32 n;` |
|      9672 | 4964 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 4965 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 4966 | `		 * every hooked-property access consults the guard, often twice. */` |
|      9484 | 4967 | `		return FALSE;` |
|         - | 4968 | `	}` |
|       190 | 4969 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       190 | 4970 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       234 | 4971 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       190 | 4972 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       146 | 4973 | `			return TRUE;` |
|         - | 4974 | `		}` |
|        23 | 4975 | `	}` |
|        45 | 4976 | `	return FALSE;` |
|      4839 | 4977 | `}` |
|      6815 | 4978 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4979 | `{` |
|         - | 4980 | `	VmMagicGuard sG;` |
|      6820 | 4981 | `	sG.pThis = pThis;` |
|      6820 | 4982 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      6820 | 4983 | `	sG.cKind = cKind;` |
|      6820 | 4984 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      6820 | 4985 | `}` |
|      6815 | 4986 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         5 | 4987 | `{` |
|      6820 | 4988 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      6820 | 4989 | `}` |
|         - | 4990 | `/*` |
|         - | 4991 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 4992 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 4993 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 4994 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 4995 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 4996 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 4997 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 4998 | ` * One-token lookahead only.` |
|         - | 4999 | ` */` |
|     14584 | 5000 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 | 5001 | `{` |
|     14589 | 5002 | `	switch( pNext->iOp ){` |
|       567 | 5003 | `		case PH7_OP_STORE:` |
|      1139 | 5004 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|         6 | 5005 | `		case PH7_OP_STORE_REF:` |
|        13 | 5006 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|       248 | 5007 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 5008 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 5009 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 5010 | `		case PH7_OP_CAT_STORE:` |
|         - | 5011 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 5012 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       501 | 5013 | `			return 1;` |
|      6470 | 5014 | `		default:` |
|     12947 | 5015 | `			return 0;` |
|         - | 5016 | `	}` |
|      7298 | 5017 | `}` |
|         - | 5018 | `/*` |
|         - | 5019 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|         - | 5020 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|         - | 5021 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|         - | 5022 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|         - | 5023 | ` * treat them as one.` |
|         - | 5024 | ` */` |
|       504 | 5025 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|         5 | 5026 | `{` |
|       728 | 5027 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|       741 | 5028 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|         5 | 5029 | `}` |
|         - | 5030 | `/*` |
|         - | 5031 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|         - | 5032 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|         - | 5033 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|         - | 5034 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|         - | 5035 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|         - | 5036 | `` * offsetSet where `$o['n']++` only notices.`` |
|         - | 5037 | ` */` |
|       754 | 5038 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|         5 | 5039 | `{` |
|       759 | 5040 | `	switch( pNext->iOp ){` |
|        60 | 5041 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 5042 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 5043 | `		case PH7_OP_CAT_STORE:` |
|         - | 5044 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 5045 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       121 | 5046 | `			return 1;` |
|       317 | 5047 | `		default:` |
|       639 | 5048 | `			return 0;` |
|         - | 5049 | `	}` |
|       382 | 5050 | `}` |
|         - | 5051 | `/*` |
|         - | 5052 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 5053 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 5054 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 5055 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 5056 | ` */` |
|       562 | 5057 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         5 | 5058 | `{` |
|       567 | 5059 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         5 | 5060 | `}` |
|         - | 5061 | `/*` |
|         - | 5062 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 5063 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 5064 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 5065 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 5066 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 5067 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 5068 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 5069 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 5070 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 5071 | ` * abort path; SXRET_OK otherwise.` |
|         - | 5072 | ` */` |
|        76 | 5073 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         4 | 5074 | `{` |
|         - | 5075 | `	char zHName[384];` |
|         - | 5076 | `	sxu32 nHName;` |
|         - | 5077 | `	ph7_class_method *pSetHook;` |
|        80 | 5078 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 5079 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 5080 | `		SyBlob sErrMsg;` |
|         5 | 5081 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 5082 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 5083 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 5084 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 5085 | `		return SXRET_OK;` |
|         - | 5086 | `	}` |
|        76 | 5087 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 5088 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 5089 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 5090 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 5091 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 5092 | `			return SXRET_OK;` |
|         - | 5093 | `		}` |
|       ! 0 | 5094 | `	}` |
|        76 | 5095 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        76 | 5096 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        76 | 5097 | `	if( pSetHook ){` |
|         - | 5098 | `		ph7_value sHookRet;` |
|         - | 5099 | `		ph7_value *apHArg[1];` |
|        76 | 5100 | `		apHArg[0] = pValue;` |
|        76 | 5101 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        76 | 5102 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        76 | 5103 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        76 | 5104 | `		VmMagicGuardPop(pVm);` |
|        72 | 5105 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        42 | 5106 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         6 | 5107 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         6 | 5108 | `			if( rcH == SXRET_OK ){` |
|         6 | 5109 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|         6 | 5110 | `				if( pBack ){` |
|         6 | 5111 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         4 | 5112 | `				}` |
|         2 | 5113 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 5114 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 5115 | `				return PH7_ABORT;` |
|         - | 5116 | `			}` |
|         - | 5117 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 5118 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 5119 | `			 * parked throw. */` |
|         2 | 5120 | `		}` |
|        76 | 5121 | `		PH7_MemObjRelease(&sHookRet);` |
|        36 | 5122 | `	}` |
|        76 | 5123 | `	return SXRET_OK;` |
|        42 | 5124 | `}` |
|         - | 5125 | `/*` |
|         - | 5126 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 5127 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 5128 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 5129 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 5130 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 5131 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 5132 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 5133 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 5134 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 5135 | ` */` |
|         - | 5136 | `/*` |
|         - | 5137 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 5138 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 5139 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 5140 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 5141 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 5142 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 5143 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 5144 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 5145 | ` * caller reads the raw slot then.` |
|         - | 5146 | ` */` |
|      1056 | 5147 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 | 5148 | `{` |
|      1061 | 5149 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 5150 | `	char zHName[384];` |
|         - | 5151 | `	sxu32 nHName;` |
|         - | 5152 | `	ph7_class_method *pGetHook;` |
|         - | 5153 | `	sxi32 rc;` |
|      1056 | 5154 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       626 | 5155 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       201 | 5156 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 5157 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 5158 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 5159 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 5160 | `		 * raw values whose output the routed throw then discards. */` |
|       865 | 5161 | `		return SXERR_NOTFOUND;` |
|         - | 5162 | `	}` |
|       201 | 5163 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       201 | 5164 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       201 | 5165 | `	if( pGetHook == 0 ){` |
|       ! 0 | 5166 | `		return SXERR_NOTFOUND;` |
|         - | 5167 | `	}` |
|       201 | 5168 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       201 | 5169 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       201 | 5170 | `	VmMagicGuardPop(pVm);` |
|       201 | 5171 | `	return rc;` |
|       533 | 5172 | `}` |
|         - | 5173 | `/*` |
|         - | 5174 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 5175 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 5176 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 5177 | ` */` |
|       150 | 5178 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5179 | `{` |
|       151 | 5180 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 5181 | `	VmSlot sFree;` |
|       151 | 5182 | `	if( pScr ){` |
|       151 | 5183 | `		PH7_MemObjRelease(pScr);` |
|        75 | 5184 | `	}` |
|       151 | 5185 | `	sFree.nIdx = nIdx;` |
|       151 | 5186 | `	sFree.pUserData = 0;` |
|       151 | 5187 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       151 | 5188 | `}` |
|         - | 5189 | `/*` |
|         - | 5190 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 5191 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 5192 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 5193 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 5194 | ` * instance reference.` |
|         - | 5195 | ` */` |
|        24 | 5196 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 5197 | `{` |
|        26 | 5198 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        26 | 5199 | `	if( pEnt == 0 ){` |
|         5 | 5200 | `		return;` |
|         - | 5201 | `	}` |
|        21 | 5202 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        13 | 5203 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         6 | 5204 | `	}` |
|        21 | 5205 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|         - | 5206 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|         - | 5207 | `		 * property's backing store — this entry owns it. */` |
|         5 | 5208 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|         2 | 5209 | `	}` |
|        21 | 5210 | `	SyBlobRelease(&pEnt->sName);` |
|        21 | 5211 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        21 | 5212 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        14 | 5213 | `}` |
|        86 | 5214 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5215 | `{` |
|         - | 5216 | `	VmHookRmw sEnt;` |
|         - | 5217 | `	VmHookRmw *pEnt;` |
|         - | 5218 | `	ph7_value *pScr;` |
|         - | 5219 | `	ph7_value sVal;` |
|         - | 5220 | `	ph7_value sKey;` |
|        87 | 5221 | `	sxi32 rc = SXRET_OK;` |
|        87 | 5222 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        87 | 5223 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 5224 | `		return SXERR_NOTFOUND;` |
|         - | 5225 | `	}` |
|        87 | 5226 | `	sEnt = *pEnt;` |
|        87 | 5227 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 5228 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|         - | 5229 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|         - | 5230 | `	 * scratch index past this point). The DIM kind's KEY slot goes the same` |
|         - | 5231 | `	 * way, for the same reason: reserving relocates the aMemObj set. */` |
|        87 | 5232 | `	PH7_MemObjInit(pVm,&sVal);` |
|        87 | 5233 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        87 | 5234 | `	if( pScr ){` |
|        87 | 5235 | `		PH7_MemObjStore(pScr,&sVal);` |
|        43 | 5236 | `	}` |
|        87 | 5237 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        87 | 5238 | `	sVal.nIdx = SXU32_HIGH;` |
|        87 | 5239 | `	PH7_MemObjInit(pVm,&sKey);` |
|        87 | 5240 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|        49 | 5241 | `		ph7_value *pKeySlot = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|        49 | 5242 | `		if( pKeySlot ){` |
|        49 | 5243 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|        24 | 5244 | `		}` |
|        49 | 5245 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|        49 | 5246 | `		sKey.nIdx = SXU32_HIGH;` |
|        24 | 5247 | `	}` |
|        87 | 5248 | `	if( pVm->nBoundaryRc == 0 ){` |
|        87 | 5249 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|         - | 5250 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|         - | 5251 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|         - | 5252 | `			SyString sPropName;` |
|        25 | 5253 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|        25 | 5254 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|        75 | 5255 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|         - | 5256 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|         - | 5257 | `			 * back through offsetSet($key, $value). */` |
|        49 | 5258 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|         - | 5259 | `				"offsetSet",sizeof("offsetSet")-1);` |
|        49 | 5260 | `			if( pSet ){` |
|         - | 5261 | `				ph7_value *apArg[2];` |
|        43 | 5262 | `				apArg[0] = &sKey;` |
|        43 | 5263 | `				apArg[1] = &sVal;` |
|        43 | 5264 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|        22 | 5265 | `			}else{` |
|         - | 5266 | `				/* A container that answers a READ and has nowhere to put the write:` |
|         - | 5267 | `				 * a class carrying a native dimension handler (ph7_class::xDim) and` |
|         - | 5268 | `				 * no ArrayAccess, which is php's DOMNodeList. php's read-then-write` |
|         - | 5269 | ``				 * pair ends in the plain store's Error, so `$list[9] .= 'x'` says`` |
|         - | 5270 | ``				 * what `$list[9] = 'x'` says. Parked: this runs at an arithmetic`` |
|         - | 5271 | `				 * op's tail, not at a throw boundary. */` |
|         - | 5272 | `				char zMsg[256];` |
|        10 | 5273 | `				sxu32 nMsg = PH7_ClassNativeDimRefusal(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|         3 | 5274 | `					zMsg,sizeof(zMsg));` |
|         7 | 5275 | `				VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg));` |
|         - | 5276 | `			}` |
|        25 | 5277 | `		}else{` |
|        15 | 5278 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         - | 5279 | `		}` |
|        43 | 5280 | `	}` |
|        87 | 5281 | `	SyBlobRelease(&sEnt.sName);` |
|        87 | 5282 | `	PH7_MemObjRelease(&sKey);` |
|        87 | 5283 | `	PH7_MemObjRelease(&sVal);` |
|        87 | 5284 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        87 | 5285 | `	return rc;` |
|        44 | 5286 | `}` |
|         - | 5287 | `/*` |
|         - | 5288 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 5289 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 5290 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 5291 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 5292 | ` */` |
|       400 | 5293 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         3 | 5294 | `{` |
|       403 | 5295 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       403 | 5296 | `	if( pSetMeth ){` |
|         - | 5297 | `		ph7_value sNameVal;` |
|         - | 5298 | `		ph7_value *apSetArg[2];` |
|       403 | 5299 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       403 | 5300 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       403 | 5301 | `		apSetArg[0] = &sNameVal;` |
|       403 | 5302 | `		apSetArg[1] = pValue;` |
|       403 | 5303 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       403 | 5304 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       403 | 5305 | `		VmMagicGuardPop(pVm);` |
|       403 | 5306 | `		PH7_MemObjRelease(&sNameVal);` |
|       200 | 5307 | `	}` |
|       403 | 5308 | `}` |
|         - | 5309 | `/*` |
|         - | 5310 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 5311 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 5312 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 5313 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 5314 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 5315 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 5316 | ` * path (the SyHash-layout incident class).` |
|         - | 5317 | ` */` |
|         - | 5318 | `/*` |
|         - | 5319 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 5320 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 5321 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 5322 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 5323 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 5324 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 5325 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 5326 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 5327 | ` * never linked (INIT error path).` |
|         - | 5328 | ` */` |
|     35250 | 5329 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 5330 | `{` |
|     35255 | 5331 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     35255 | 5332 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 5333 | `	sxu32 i;` |
|     37773 | 5334 | `	for( i = 0 ; i < n ; ++i ){` |
|     37773 | 5335 | `		if( apStep[i] == pStep ){` |
|     35263 | 5336 | `			for( ; i + 1 < n ; ++i ){` |
|         9 | 5337 | `				apStep[i] = apStep[i + 1];` |
|         5 | 5338 | `			}` |
|     35255 | 5339 | `			(void)SySetPop(&pInfo->aStep);` |
|     35255 | 5340 | `			return;` |
|         - | 5341 | `		}` |
|      1264 | 5342 | `	}` |
|     17630 | 5343 | `}` |
|       598 | 5344 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 5345 | `{` |
|       603 | 5346 | `	if( pStep->pOwner ){` |
|       243 | 5347 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       120 | 5348 | `	}` |
|       603 | 5349 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       603 | 5350 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       603 | 5351 | `	PH7_ClassInstanceUnref(pThis);` |
|       603 | 5352 | `}` |
|         - | 5353 | `/*` |
|         - | 5354 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 5355 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 5356 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 5357 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 5358 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 5359 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 5360 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 5361 | ` * map's last reference.` |
|         - | 5362 | ` */` |
|     34584 | 5363 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 5364 | `{` |
|     34589 | 5365 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     34589 | 5366 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     34589 | 5367 | `	if( bPop ){` |
|         - | 5368 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 5369 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     34589 | 5370 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     17292 | 5371 | `	}` |
|     34589 | 5372 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     34589 | 5373 | `	PH7_HashmapUnref(pMap);` |
|     34589 | 5374 | `}` |
|         - | 5375 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 5376 | `/*` |
|         - | 5377 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 5378 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5379 | ` * See block-comment on that function for additional information.` |
|         - | 5380 | ` */` |
|   1510007 | 5381 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 5382 | `{` |
|         - | 5383 | `	ph7_value *pStack;` |
|         - | 5384 | `	sxu32 nCap;` |
|         - | 5385 | `	sxi32 rc;` |
|         - | 5386 | `	/* Allocate a new operand stack */` |
|   1510012 | 5387 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   1510012 | 5388 | `	if( pStack == 0 ){` |
|       ! 0 | 5389 | `		return SXERR_MEM;` |
|         - | 5390 | `	}` |
|   1510012 | 5391 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 5392 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 5393 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   1510012 | 5394 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 5395 | `	/* Free the operand stack */` |
|   1510012 | 5396 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 5397 | `	/* Execution result */` |
|   1510012 | 5398 | `	return rc;` |
|    755008 | 5399 | `}` |
|         - | 5400 | `/*` |
|         - | 5401 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - | 5402 | ` * statement must honour?` |
|         - | 5403 | ` *` |
|         - | 5404 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - | 5405 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - | 5406 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - | 5407 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - | 5408 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - | 5409 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - | 5410 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - | 5411 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - | 5412 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - | 5413 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - | 5414 | ` * drift apart.` |
|         - | 5415 | ` *` |
|         - | 5416 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - | 5417 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - | 5418 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - | 5419 | ` * this opcode did not create as its own throw.` |
|         - | 5420 | ` *` |
|         - | 5421 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - | 5422 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - | 5423 | ` */` |
|         - | 5424 | `/*` |
|         - | 5425 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - | 5426 | ` *` |
|         - | 5427 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - | 5428 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - | 5429 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - | 5430 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - | 5431 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - | 5432 | ` * captured value writes over the superglobal's own slot: calling` |
|         - | 5433 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - | 5434 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - | 5435 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - | 5436 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - | 5437 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - | 5438 | ` *` |
|         - | 5439 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - | 5440 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - | 5441 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - | 5442 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - | 5443 | ` */` |
|      4882 | 5444 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 | 5445 | `{` |
|         - | 5446 | `	static const char *const azAuto[] = {` |
|         - | 5447 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - | 5448 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - | 5449 | `	};` |
|         - | 5450 | `	sxu32 n;` |
|     53109 | 5451 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|     48287 | 5452 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|     48287 | 5453 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|        63 | 5454 | `			return 1;` |
|         - | 5455 | `		}` |
|     24116 | 5456 | `	}` |
|      4827 | 5457 | `	return 0;` |
|      2446 | 5458 | `}` |
|     27708 | 5459 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 | 5460 | `{` |
|     25893 | 5461 | `	return rc == PH7_EXCEPTION` |
|     25888 | 5462 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|     39742 | 5463 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 | 5464 | `}` |
|         - | 5465 | `/*` |
|         - | 5466 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 5467 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 5468 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 5469 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 5470 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 5471 | ` */` |
|       144 | 5472 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         2 | 5473 | `{` |
|       146 | 5474 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       146 | 5475 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 5476 | `	sxi32 rc;` |
|       146 | 5477 | `	if( pDeclCls ){` |
|       130 | 5478 | `		pVm->pConstEvalClass = pDeclCls;` |
|       130 | 5479 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        64 | 5480 | `	}` |
|       146 | 5481 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       146 | 5482 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       146 | 5483 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       146 | 5484 | `	return rc;` |
|         2 | 5485 | `}` |
|         - | 5486 | `/*` |
|         - | 5487 | ` * Flush every still-open output buffer at the end of execution. php implicitly` |
|         - | 5488 | ` * ends+flushes all ob_start() levels on shutdown (normal end, exit()/die(), or` |
|         - | 5489 | ` * fatal); PHL used to DISCARD them, so a script that never called ob_end_flush()` |
|         - | 5490 | ` * — e.g. PHPUnit, which buffers its result summary and then exit()s with a` |
|         - | 5491 | ` * non-zero status — lost that output entirely.` |
|         - | 5492 | ` *` |
|         - | 5493 | ` * PH7_VmObFlushAll() does it the way php does: one FINAL operation per buffer,` |
|         - | 5494 | ` * innermost first, so each handler's answer is what the buffer under it is` |
|         - | 5495 | ` * handed.` |
|         - | 5496 | ` */` |
|      4972 | 5497 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 5498 | `{` |
|      4977 | 5499 | `	PH7_VmObFlushAll(&(*pVm));` |
|      4977 | 5500 | `}` |
|         - | 5501 | `/*` |
|         - | 5502 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 5503 | ` * or more calls to [register_shutdown_function()].` |
|         - | 5504 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 5505 | ` * execution ends.` |
|         - | 5506 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 5507 | ` * additional information.` |
|         - | 5508 | ` */` |
|      4972 | 5509 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 5510 | `{` |
|         - | 5511 | `	VmShutdownCB *pEntry;` |
|         - | 5512 | `	ph7_value *apArg[10];` |
|         - | 5513 | `	sxu32 n,nEntry;` |
|         - | 5514 | `	int i;` |
|         - | 5515 | `	/* Point to the stack of registered callbacks */` |
|      4977 | 5516 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     54697 | 5517 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     49725 | 5518 | `		apArg[i] = 0;` |
|     24865 | 5519 | `	}` |
|         - | 5520 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 5521 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 5522 | `	 * callbacks, mirroring PHP.` |
|         - | 5523 | `	 */` |
|      4977 | 5524 | `	pVm->bHaltRequested = 0;` |
|      4999 | 5525 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        27 | 5526 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        27 | 5527 | `		if( pEntry ){` |
|         - | 5528 | `			/* Prepare callback arguments if any */` |
|        27 | 5529 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 5530 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 5531 | `					break;` |
|         - | 5532 | `				}` |
|       ! 0 | 5533 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 5534 | `			}` |
|         - | 5535 | `			/* Invoke the callback */` |
|        27 | 5536 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 5537 | `			/*` |
|         - | 5538 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 5539 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 5540 | `			 */` |
|        27 | 5541 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        27 | 5542 | `			if( pEntry ){` |
|        27 | 5543 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        27 | 5544 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 5545 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 5546 | `				}` |
|        11 | 5547 | `			}` |
|        27 | 5548 | `			if( pVm->bHaltRequested ){` |
|         - | 5549 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|       ! 0 | 5550 | `				break;` |
|         - | 5551 | `			}` |
|        11 | 5552 | `		}` |
|        16 | 5553 | `	}` |
|      4977 | 5554 | `	SySetReset(&pVm->aShutdown);` |
|      4977 | 5555 | `}` |
|         - | 5556 | `/*` |
|         - | 5557 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 5558 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5559 | ` * See block-comment on that function for additional information.` |
|         - | 5560 | ` */` |
|      4972 | 5561 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 5562 | `{` |
|         - | 5563 | `	/* Make sure we are ready to execute this program */` |
|      4977 | 5564 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 5565 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 5566 | `	}` |
|         - | 5567 | `	/* Set the execution magic number  */` |
|      4977 | 5568 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 5569 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 5570 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 5571 | `	{` |
|      4977 | 5572 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|      4977 | 5573 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 5574 | `	}` |
|         - | 5575 | `	/* Invoke any shutdown callbacks */` |
|      4977 | 5576 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 5577 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 5578 | `	 * shutdown callbacks, which may still write into them. */` |
|      4977 | 5579 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 5580 | `	/* An open session is written back LAST, from php's own module shutdown: after` |
|         - | 5581 | `	 * the script's shutdown callbacks (which may still write to $_SESSION) and` |
|         - | 5582 | `	 * after the buffers are flushed (which is why its diagnostics land outside` |
|         - | 5583 | `	 * them). */` |
|      4977 | 5584 | `	PH7_VmSessionShutdown(&(*pVm));` |
|         - | 5585 | `	/*` |
|         - | 5586 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 5587 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 5588 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 5589 | `	 */` |
|      4977 | 5590 | `	return SXRET_OK;` |
|      2491 | 5591 | `}` |
|         - | 5592 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 5593 | `/*` |
|         - | 5594 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 5595 | ` * the desired message.` |
|         - | 5596 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 5597 | ` * in 'api.c' for additional information.` |
|         - | 5598 | ` */` |
|     87958 | 5599 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 5600 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 5601 | `	SyString *pString /* Message to output */` |
|         - | 5602 | `	)` |
|         5 | 5603 | `{` |
|     87963 | 5604 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     87963 | 5605 | `	sxi32 rc = SXRET_OK;` |
|         - | 5606 | `	/* Call the output consumer */` |
|     87963 | 5607 | `	if( pString->nByte > 0 ){` |
|     87963 | 5608 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     87963 | 5609 | `		VmTrackOutput(pVm, pString->nByte);` |
|     43979 | 5610 | `	}` |
|     87963 | 5611 | `	return rc;` |
|         5 | 5612 | `}` |
|         - | 5613 | `/*` |
|         - | 5614 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 5615 | ` * callback to consume the formatted message.` |
|         - | 5616 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 5617 | ` * in 'api.c' for additional information.` |
|         - | 5618 | ` */` |
|        30 | 5619 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 5620 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 5621 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 5622 | `	va_list ap           /* Variable list of arguments */` |
|         - | 5623 | `	)` |
|         2 | 5624 | `{` |
|        32 | 5625 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        32 | 5626 | `	sxi32 rc = SXRET_OK;` |
|         - | 5627 | `	SyBlob sWorker;` |
|         - | 5628 | `	/* Format the message and call the output consumer */` |
|        32 | 5629 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        32 | 5630 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        32 | 5631 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 5632 | `		/* Consume the formatted message */` |
|        32 | 5633 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        15 | 5634 | `	}` |
|        32 | 5635 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 5636 | `	/* Release the working buffer */` |
|        32 | 5637 | `	SyBlobRelease(&sWorker);` |
|        32 | 5638 | `	return rc;` |
|         2 | 5639 | `}` |
|         - | 5640 | `/*` |
|         - | 5641 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 5642 | ` * This function never fail and always return a pointer` |
|         - | 5643 | ` * to a null terminated string.` |
|         - | 5644 | ` */` |
|        10 | 5645 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 5646 | `{` |
|        11 | 5647 | `	const char *zOp = "Unknown     ";` |
|        11 | 5648 | `	switch(nOp){` |
|         3 | 5649 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 5650 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 5651 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 5652 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 5653 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 5654 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 5655 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 5656 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 5657 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 5658 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 5659 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 5660 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 5661 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 5662 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 5663 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 5664 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 5665 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 5666 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 5667 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 5668 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 5669 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 5670 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|       ! 0 | 5671 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|       ! 0 | 5672 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 5673 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 5674 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 5675 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 5676 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 5677 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 5678 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 5679 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 5680 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 5681 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 5682 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 5683 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 5684 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 5685 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 5686 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 5687 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 5688 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 5689 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 5690 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 5691 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 5692 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 5693 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 5694 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 5695 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 5696 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 5697 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 5698 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 5699 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 5700 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 5701 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 5702 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 5703 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 | 5704 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 5705 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 5706 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 5707 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 5708 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 5709 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 5710 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 5711 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 5712 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 5713 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 5714 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 5715 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 5716 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 5717 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 5718 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 5719 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 5720 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 5721 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 5722 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 5723 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 5724 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 5725 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 5726 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 5727 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 5728 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 5729 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 5730 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 5731 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 5732 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 5733 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 5734 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 5735 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 5736 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 5737 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 5738 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 5739 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 5740 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 5741 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 5742 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 5743 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 5744 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 5745 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 5746 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 5747 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 5748 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 5749 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 5750 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 5751 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 5752 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 5753 | `	default:` |
|       ! 0 | 5754 | `		break;` |
|         - | 5755 | `	}` |
|        11 | 5756 | `	return zOp;` |
|         1 | 5757 | `}` |
|         - | 5758 | `/*` |
|         - | 5759 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 5760 | ` * The xConsumer() callback which is an used defined function` |
|         - | 5761 | ` * is responsible of consuming the generated dump.` |
|         - | 5762 | ` */` |
|         2 | 5763 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 5764 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 5765 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 5766 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 5767 | `	)` |
|         1 | 5768 | `{` |
|         - | 5769 | `	sxi32 rc;` |
|         3 | 5770 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 5771 | `	return rc;` |
|         1 | 5772 | `}` |
|         - | 5773 | `/*` |
|         - | 5774 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 5775 | ` * outside a class body [i.e: global or function scope].` |
|         - | 5776 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 5777 | ` * in 'compile.c' for additional information.` |
|         - | 5778 | ` */` |
|         2 | 5779 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         1 | 5780 | `{` |
|         3 | 5781 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 5782 | `	/* Evaluate and expand constant value */` |
|         3 | 5783 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|         3 | 5784 | `}` |
|         - | 5785 | `/*` |
|         - | 5786 | ` * Section:` |
|         - | 5787 | ` *  Function handling functions.` |
|         - | 5788 | ` * Status:` |
|         - | 5789 | ` *    Stable.` |
|         - | 5790 | ` */` |
|         - | 5791 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 5792 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 5793 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 5794 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 5795 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 5796 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 5797 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 5798 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 5799 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 5800 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 5801 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 5802 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 5803 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 5804 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 5805 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 5806 | `	    /* Constants management */` |
|         - | 5807 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 5808 | `	{ "define",   vm_builtin_define               },` |
|         - | 5809 | `	{ "constant", vm_builtin_constant             },` |
|         - | 5810 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 5811 | `	   /* Class/Object functions */` |
|         - | 5812 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 5813 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 5814 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 5815 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 5816 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 5817 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 5818 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 5819 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 5820 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 5821 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 5822 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 5823 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 5824 | `	{ "get_declared_traits",     vm_builtin_get_declared_traits    },` |
|         - | 5825 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 5826 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 5827 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 5828 | `	{ "get_mangled_object_vars", vm_builtin_get_mangled_object_vars },` |
|         - | 5829 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 5830 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 5831 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|         - | 5832 | `	{ "clone",           vm_builtin_clone             },` |
|         - | 5833 | `	   /* SPL object identity */` |
|         - | 5834 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 5835 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 5836 | `	   /* SPL Autoloading */` |
|         - | 5837 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 5838 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 5839 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 5840 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 5841 | `	   /* Random numbers/strings generators */` |
|         - | 5842 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 5843 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 5844 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 5845 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 5846 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 5847 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 5848 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 5849 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5850 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 5851 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 5852 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 5853 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5854 | `	   /* Language constructs functions */` |
|         - | 5855 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 5856 | `	{ "print", vm_builtin_print                   },` |
|         - | 5857 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 5858 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 5859 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 5860 | `	  /* Variable handling functions */` |
|         - | 5861 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 5862 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 5863 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|         - | 5864 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 5865 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 5866 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 5867 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 5868 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 5869 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 5870 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 5871 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 5872 | `	  /* Ouput control functions */` |
|         - | 5873 | `	{ "flush",        vm_builtin_flush             },` |
|         - | 5874 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 5875 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 5876 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 5877 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 5878 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 5879 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 5880 | `	{ "ob_get_flush",    vm_builtin_ob_get_flush   },` |
|         - | 5881 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 5882 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 5883 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 5884 | `	{ "ob_get_status",     vm_builtin_ob_get_status },` |
|         - | 5885 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 5886 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 5887 | `	  /* Assertion functions */` |
|         - | 5888 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 5889 | `	  /* Error reporting functions */` |
|         - | 5890 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 5891 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 5892 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 5893 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 5894 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 5895 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 5896 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 5897 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 5898 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 5899 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 5900 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 5901 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 5902 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 5903 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 5904 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 5905 | `	  /* Release info */` |
|         - | 5906 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 5907 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 5908 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|         - | 5909 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|         - | 5910 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 5911 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 5912 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 5913 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 5914 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 5915 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 5916 | `	  /* hashmap */` |
|         - | 5917 | `	{"compact",          vm_builtin_compact       },` |
|         - | 5918 | `	{"extract",          vm_builtin_extract       },` |
|         - | 5919 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 5920 | `	  /* URL related function */` |
|         - | 5921 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 5922 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 5923 | `	   /* Command line processing */` |
|         - | 5924 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 5925 | `	   /* JSON encoding/decoding */` |
|         - | 5926 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 5927 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 5928 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 5929 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 5930 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 5931 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 5932 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 5933 | `	   /* Files/URI inclusion facility */` |
|         - | 5934 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 5935 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 5936 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 5937 | `	{ "include",      vm_builtin_include          },` |
|         - | 5938 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 5939 | `	{ "require",      vm_builtin_require          },` |
|         - | 5940 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 5941 | `};` |
|         - | 5942 | `/*` |
|         - | 5943 | ` * Register the built-in VM functions defined above.` |
|         - | 5944 | ` */` |
|      4962 | 5945 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 5946 | `{` |
|         - | 5947 | `	sxi32 rc;` |
|         - | 5948 | `	sxu32 n;` |
|    630179 | 5949 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 5950 | `		/* Note that these special functions have access` |
|         - | 5951 | `		 * to the underlying virtual machine as their` |
|         - | 5952 | `		 * private data.` |
|         - | 5953 | `		 */` |
|    625217 | 5954 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    625217 | 5955 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 5956 | `			return rc;` |
|         - | 5957 | `		}` |
|    312611 | 5958 | `	}` |
|      4967 | 5959 | `	return SXRET_OK;` |
|      2486 | 5960 | `}` |
|         - | 5961 | `/*` |
|         - | 5962 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 5963 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 5964 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 5965 | ` */` |
|   8463811 | 5966 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 5967 | `{` |
|   8463816 | 5968 | `	if( !iLoadable ){` |
|   5872561 | 5969 | `		return pClass;` |
|         - | 5970 | `	}` |
|   2591276 | 5971 | `	while(pClass){` |
|   2591260 | 5972 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   2591244 | 5973 | `			return pClass;` |
|         - | 5974 | `		}` |
|        17 | 5975 | `		pClass = pClass->pNextName;` |
|         1 | 5976 | `	}` |
|        17 | 5977 | `	return 0;` |
|   4231921 | 5978 | `}` |
|         - | 5979 | `/*` |
|         - | 5980 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 5981 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 5982 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 5983 | ` * registered in the VM's class table.` |
|         - | 5984 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 5985 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 5986 | ` */` |
|       472 | 5987 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5988 | `{` |
|         - | 5989 | `	VmAutoloadCB *pEntry;` |
|         - | 5990 | `	ph7_value sArg,sResult;` |
|         - | 5991 | `	SyHashEntry *pHashEntry;` |
|         - | 5992 | `	ph7_class *pClass;` |
|         - | 5993 | `	sxu32 n,nEntry;` |
|       477 | 5994 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       477 | 5995 | `	if( nEntry < 1 ){` |
|       321 | 5996 | `		return 0;` |
|         - | 5997 | `	}` |
|         - | 5998 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       161 | 5999 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 6000 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 6001 | `	}` |
|         - | 6002 | `	/* Mark this class as being autoloaded */` |
|       159 | 6003 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 6004 | `	/* Prepare the class name argument */` |
|       159 | 6005 | `	PH7_MemObjInit(pVm,&sArg);` |
|       159 | 6006 | `	PH7_MemObjInit(pVm,&sResult);` |
|       159 | 6007 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       159 | 6008 | `	pClass = 0;` |
|       295 | 6009 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 6010 | `		ph7_value *apArg[1];` |
|       169 | 6011 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       169 | 6012 | `		if( pEntry == 0 ){` |
|       ! 0 | 6013 | `			continue;` |
|         - | 6014 | `		}` |
|       169 | 6015 | `		apArg[0] = &sArg;` |
|       169 | 6016 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|         - | 6017 | `			/* Callback could not be invoked — skip to next autoloader */` |
|        24 | 6018 | `			continue;` |
|         - | 6019 | `		}` |
|         - | 6020 | `		/* Check if the class is now available */` |
|       147 | 6021 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       147 | 6022 | `		if( pHashEntry ){` |
|        33 | 6023 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        33 | 6024 | `			if( pClass ){` |
|        33 | 6025 | `				break;` |
|         - | 6026 | `			}` |
|       ! 0 | 6027 | `		}` |
|        61 | 6028 | `	}` |
|       159 | 6029 | `	PH7_MemObjRelease(&sArg);` |
|       159 | 6030 | `	PH7_MemObjRelease(&sResult);` |
|         - | 6031 | `	/* Remove reentrancy guard */` |
|       159 | 6032 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       159 | 6033 | `	return pClass;` |
|       241 | 6034 | `}` |
|         - | 6035 | `/*` |
|         - | 6036 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 6037 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 6038 | ` */` |
|        46 | 6039 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 6040 | `{` |
|        51 | 6041 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         5 | 6042 | `}` |
|         - | 6043 | `/*` |
|         - | 6044 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 6045 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 6046 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 6047 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 6048 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 6049 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 6050 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 6051 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 6052 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 6053 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 6054 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 6055 | ` */` |
|  12290534 | 6056 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 6057 | `{` |
|  12290539 | 6058 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        72 | 6059 | `		(*pzName)++;` |
|        72 | 6060 | `		(*pnByte)--;` |
|        34 | 6061 | `	}` |
|  12290539 | 6062 | `}` |
|         - | 6063 | `/*` |
|         - | 6064 | ` * Check if the given name refer to an installed class.` |
|         - | 6065 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 6066 | ` */` |
|   8464219 | 6067 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 6068 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 6069 | `	const char *zName,  /* Name of the target class */` |
|         - | 6070 | `	sxu32 nByte,        /* zName length */` |
|         - | 6071 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 6072 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 6073 | `						 */` |
|         - | 6074 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 6075 | `	)` |
|         5 | 6076 | `{` |
|         - | 6077 | `	SyHashEntry *pEntry;` |
|         - | 6078 | `	ph7_class *pClass;` |
|   4232120 | 6079 | `	SXUNUSED(iNest);` |
|         - | 6080 | `	/* Exact class lookup.` |
|         - | 6081 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 6082 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 6083 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 6084 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   8464224 | 6085 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 6086 | `	/* An empty stripped name names no class: neither a truly empty "" nor a lone` |
|         - | 6087 | `	 * "\" is looked up or handed to the autoloader (php 8.5.11, GH-23232). */` |
|   8464224 | 6088 | `	if( nByte < 1 ){` |
|        12 | 6089 | `		return 0;` |
|         - | 6090 | `	}` |
|   8464214 | 6091 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   8464214 | 6092 | `	if( pEntry == 0 ){` |
|         - | 6093 | `		/* Class not found in hash table — try autoload before giving up */` |
|       431 | 6094 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         - | 6095 | `	}` |
|   8463788 | 6096 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   8463788 | 6097 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   4232125 | 6098 | `}` |
|         - | 6099 | `/*` |
|         - | 6100 | ` * Reference Table Implementation` |
|         - | 6101 | ` * Status: stable <chm@symisc.net>` |
|         - | 6102 | ` * Intro` |
|         - | 6103 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 6104 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 6105 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 6106 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 6107 | ` *  Refer to the official for more information on this powerful` |
|         - | 6108 | ` *  extension.` |
|         - | 6109 | ` */` |
|         - | 6110 | `/*` |
|         - | 6111 | ` * Allocate a new reference entry.` |
|         - | 6112 | ` */` |
|  22602556 | 6113 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6114 | `{` |
|         - | 6115 | `	VmRefObj *pRef;` |
|         - | 6116 | `	/* Allocate a new instance */` |
|  22602561 | 6117 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  22602561 | 6118 | `	if( pRef == 0 ){` |
|       ! 0 | 6119 | `		return 0;` |
|         - | 6120 | `	}` |
|         - | 6121 | `	/* Zero the structure */` |
|  22602561 | 6122 | `	SyZero(pRef,sizeof(VmRefObj));` |
|         - | 6123 | `	/* Initialize fields */` |
|  22602561 | 6124 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  22602561 | 6125 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  22602561 | 6126 | `	pRef->nIdx = nIdx;` |
|  22602561 | 6127 | `	return pRef;` |
|  11302592 | 6128 | `}` |
|         - | 6129 | `/*` |
|         - | 6130 | ` * Default hash function used by the reference table` |
|         - | 6131 | ` * for lookup/insertion operations.` |
|         - | 6132 | ` */` |
| 126777605 | 6133 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|         5 | 6134 | `{` |
|         - | 6135 | `	/* Calculate the hash based on the memory object index */` |
| 126777610 | 6136 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|         5 | 6137 | `}` |
|         - | 6138 | `/*` |
|         - | 6139 | ` * Check if a memory object [i.e: a variable] is already installed` |
|         - | 6140 | ` * in the reference table.` |
|         - | 6141 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|         - | 6142 | ` * otherwise.` |
|         - | 6143 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6144 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6145 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6146 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6147 | ` * Refer to the official for more information on this powerful` |
|         - | 6148 | ` * extension.` |
|         - | 6149 | ` */` |
|  80488725 | 6150 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|         5 | 6151 | `{` |
|         - | 6152 | `	VmRefObj *pRef;` |
|         - | 6153 | `	sxu32 nBucket;` |
|         - | 6154 | `	/* Point to the appropriate bucket */` |
|  80488730 | 6155 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|         - | 6156 | `	/* Perform the lookup */` |
|  80488730 | 6157 | `	pRef = pVm->apRefObj[nBucket];` |
| 224892681 | 6158 | `	for(;;){` |
| 449774862 | 6159 | `		if( pRef == 0 ){` |
|  22602691 | 6160 | `			break;` |
|         - | 6161 | `		}` |
| 427172176 | 6162 | `		if( pRef->nIdx == nObjIdx ){` |
|         - | 6163 | `			/* Entry found */` |
|  57886044 | 6164 | `			return pRef;` |
|         - | 6165 | `		}` |
|         - | 6166 | `		/* Point to the next entry */` |
| 369286137 | 6167 | `		pRef = pRef->pNextCollide;` |
|         5 | 6168 | `	}` |
|         - | 6169 | `	/* No such entry,return NULL */` |
|  22602691 | 6170 | `	return 0;` |
|  40249671 | 6171 | `}` |
|         - | 6172 | `/*` |
|         - | 6173 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6174 | ` *` |
|         - | 6175 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6176 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6177 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6178 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6179 | ` * Refer to the official for more information on this powerful` |
|         - | 6180 | ` * extension.` |
|         - | 6181 | ` */` |
|  22602556 | 6182 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6183 | `{` |
|         - | 6184 | `	sxu32 nBucket;` |
|  22602561 | 6185 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|         - | 6186 | `		VmRefObj **apNew;` |
|         - | 6187 | `		sxu32 nNew;` |
|         - | 6188 | `		/* Allocate a larger table */` |
|     12696 | 6189 | `		nNew = pVm->nRefSize << 1;` |
|     12696 | 6190 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     12696 | 6191 | `		if( apNew ){` |
|     12696 | 6192 | `			VmRefObj *pEntry = pVm->pRefList;` |
|         - | 6193 | `			sxu32 n;` |
|         - | 6194 | `			/* Zero the structure */` |
|     12696 | 6195 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|         - | 6196 | `			/* Rehash all referenced entries */` |
|   4408923 | 6197 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|         - | 6198 | `				/* Remove old collision links */` |
|   4396232 | 6199 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 6200 | `				/* Point to the appropriate bucket */` |
|   4396232 | 6201 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|         - | 6202 | `				/* Insert the entry  */` |
|   4396232 | 6203 | `				pEntry->pNextCollide = apNew[nBucket];` |
|   4396232 | 6204 | `				if( apNew[nBucket] ){` |
|   3585243 | 6205 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|   1792619 | 6206 | `				}` |
|   4396232 | 6207 | `				apNew[nBucket] = pEntry;` |
|         - | 6208 | `				/* Point to the next entry */` |
|   4396232 | 6209 | `				pEntry = pEntry->pNext;` |
|   2198097 | 6210 | `			}` |
|         - | 6211 | `			/* Release the old table */` |
|     12696 | 6212 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|         - | 6213 | `			/* Install the new one */` |
|     12696 | 6214 | `			pVm->apRefObj = apNew;` |
|     12696 | 6215 | `			pVm->nRefSize = nNew;` |
|      6345 | 6216 | `		}` |
|      6345 | 6217 | `	}` |
|         - | 6218 | `	/* Point to the appropriate bucket */` |
|  22602561 | 6219 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|         - | 6220 | `	/* Insert the entry */` |
|  22602561 | 6221 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  22602561 | 6222 | `	if( pVm->apRefObj[nBucket] ){` |
|  17801217 | 6223 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   8900710 | 6224 | `	}` |
|  22602561 | 6225 | `	pVm->apRefObj[nBucket] = pRef;` |
|  22602561 | 6226 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  22602561 | 6227 | `	pVm->nRefUsed++;` |
|  22602561 | 6228 | `	return SXRET_OK;` |
|         5 | 6229 | `}` |
|         - | 6230 | `/*` |
|         - | 6231 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|         - | 6232 | ` * the reference table.` |
|         - | 6233 | ` * This function is invoked when the user perform an unset` |
|         - | 6234 | ` * call [i.e: unset($var); ].` |
|         - | 6235 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6236 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6237 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6238 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6239 | ` * Refer to the official for more information on this powerful` |
|         - | 6240 | ` * extension.` |
|         - | 6241 | ` */` |
|  21655535 | 6242 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6243 | `{` |
|         - | 6244 | `	ph7_hashmap_node **apNode;` |
|         - | 6245 | `	SyHashEntry **apEntry;` |
|         - | 6246 | `	sxu32 n;` |
|         - | 6247 | `	/* Point to the reference table */` |
|  21655540 | 6248 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  21655540 | 6249 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|         - | 6250 | `	/* Unlink the entry from the reference table */` |
|  22890899 | 6251 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1235364 | 6252 | `		if( apEntry[n] ){` |
|       177 | 6253 | `			SyHashDeleteEntry2(apEntry[n]);` |
|        88 | 6254 | `		}` |
|    618982 | 6255 | `	}` |
|  32468373 | 6256 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  10812838 | 6257 | `		if( apNode[n] ){` |
|       511 | 6258 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|       255 | 6259 | `		}` |
|   5406446 | 6260 | `	}` |
|  21655540 | 6261 | `	if( pRef->pPrevCollide ){` |
|   2365443 | 6262 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   1182279 | 6263 | `	}else{` |
|  19290102 | 6264 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|         - | 6265 | `	}` |
|  21655540 | 6266 | `	if( pRef->pNextCollide ){` |
|  15909342 | 6267 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   7954783 | 6268 | `	}` |
|  21655540 | 6269 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|         - | 6270 | `	/* Release the node */` |
|  21655540 | 6271 | `	SySetRelease(&pRef->aReference);` |
|  21655540 | 6272 | `	SySetRelease(&pRef->aArrEntries);` |
|  21655540 | 6273 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  21655540 | 6274 | `	pVm->nRefUsed--;` |
|  21655540 | 6275 | `	return SXRET_OK;` |
|         5 | 6276 | `}` |
|         - | 6277 | `/*` |
|         - | 6278 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6279 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6280 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6281 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6282 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6283 | ` * Refer to the official for more information on this powerful` |
|         - | 6284 | ` * extension.` |
|         - | 6285 | ` */` |
|  22676042 | 6286 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 6287 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6288 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6289 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6290 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 6291 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 6292 | `	)` |
|         5 | 6293 | `{` |
|  22676047 | 6294 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 6295 | `	VmRefObj *pRef;` |
|         - | 6296 | `	/* Check if the referenced object already exists */` |
|  22676047 | 6297 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  22676047 | 6298 | `	if( pRef == 0 ){` |
|         - | 6299 | `		/* Create a new entry */` |
|  22602561 | 6300 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  22602561 | 6301 | `		if( pRef == 0 ){` |
|       ! 0 | 6302 | `			return SXERR_MEM;` |
|         - | 6303 | `		}` |
|  22602561 | 6304 | `		pRef->iFlags = iFlags;` |
|         - | 6305 | `		/* Install the entry */` |
|  22602561 | 6306 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  11302587 | 6307 | `	}` |
|  22676047 | 6308 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  22676047 | 6309 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 6310 | `		VmSlot sRef;` |
|         - | 6311 | `		/* Local frame,record referenced entry so that it can` |
|         - | 6312 | `		 * be deleted when we leave this frame.` |
|         - | 6313 | `		 */` |
|   1229440 | 6314 | `		sRef.nIdx = nIdx;` |
|   1229440 | 6315 | `		sRef.pUserData = pEntry;` |
|   1229440 | 6316 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 6317 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 6318 | `		}` |
|    616015 | 6319 | `	}` |
|  22676047 | 6320 | `	if( pEntry ){` |
|         - | 6321 | `		/* Address of the hash-entry (into a row a dead holder left behind — a name can` |
|         - | 6322 | `		 * be RE-BOUND to the same slot any number of times, and a set that only ever` |
|         - | 6323 | `		 * grew made both the install and the holder count O(rows)) */` |
|   1298942 | 6324 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   1298942 | 6325 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   1299922 | 6326 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|       985 | 6327 | `			if( apEntry[n] == pEntry ){` |
|       ! 0 | 6328 | `				nFree = SXU32_HIGH; /* already recorded: never file one holder twice */` |
|       ! 0 | 6329 | `				break;` |
|         - | 6330 | `			}` |
|       985 | 6331 | `			if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|       421 | 6332 | `				nFree = n;` |
|       209 | 6333 | `			}` |
|       495 | 6334 | `		}` |
|   1298942 | 6335 | `		if( n >= SySetUsed(&pRef->aReference) ){` |
|   1298942 | 6336 | `			if( nFree != SXU32_HIGH ){` |
|       421 | 6337 | `				apEntry[nFree] = pEntry;` |
|       212 | 6338 | `			}else{` |
|   1298524 | 6339 | `				SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|         - | 6340 | `			}` |
|    650766 | 6341 | `		}` |
|    650766 | 6342 | `	}` |
|  22676047 | 6343 | `	if( pMapEntry ){` |
|         - | 6344 | `		/* Address of the hashmap node [i.e: Array entry] — same row reuse */` |
|  10981391 | 6345 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  10981391 | 6346 | `		sxu32 n, nFree = SXU32_HIGH;` |
|  10982085 | 6347 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|       699 | 6348 | `			if( apNode[n] == pMapEntry ){` |
|       ! 0 | 6349 | `				nFree = SXU32_HIGH;` |
|       ! 0 | 6350 | `				break;` |
|         - | 6351 | `			}` |
|       699 | 6352 | `			if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|         3 | 6353 | `				nFree = n;` |
|         1 | 6354 | `			}` |
|       352 | 6355 | `		}` |
|  10981391 | 6356 | `		if( n >= SySetUsed(&pRef->aArrEntries) ){` |
|  10981391 | 6357 | `			if( nFree != SXU32_HIGH ){` |
|         3 | 6358 | `				apNode[nFree] = pMapEntry;` |
|         2 | 6359 | `			}else{` |
|  10981389 | 6360 | `				SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|         - | 6361 | `			}` |
|   5490708 | 6362 | `		}` |
|   5490708 | 6363 | `	}` |
|  22676047 | 6364 | `	return SXRET_OK;` |
|  11339335 | 6365 | `}` |
|         - | 6366 | `/*` |
|         - | 6367 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 6368 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6369 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6370 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6371 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6372 | ` * Refer to the official for more information on this powerful` |
|         - | 6373 | ` * extension.` |
|         - | 6374 | ` */` |
|  12035242 | 6375 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 6376 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6377 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6378 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6379 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 6380 | `	)` |
|         5 | 6381 | `{` |
|         - | 6382 | `	VmRefObj *pRef;` |
|         - | 6383 | `	sxu32 n;` |
|         - | 6384 | `	/* Check if the referenced object already exists */` |
|  12035247 | 6385 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  12035247 | 6386 | `	if( pRef == 0 ){` |
|         - | 6387 | `		/* Not such entry */` |
|         5 | 6388 | `		return SXERR_NOTFOUND;` |
|         - | 6389 | `	}` |
|         - | 6390 | `	/* Remove the desired entry */` |
|  12035243 | 6391 | `	if( pEntry ){` |
|         - | 6392 | `		SyHashEntry **apEntry;` |
|   1229396 | 6393 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   2459333 | 6394 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1229942 | 6395 | `			if( apEntry[n] == pEntry ){` |
|         - | 6396 | `				/* Nullify the entry */` |
|   1229394 | 6397 | `				apEntry[n] = 0;` |
|         - | 6398 | `				/*` |
|         - | 6399 | `				 * NOTE:` |
|         - | 6400 | `				 * In future releases,think to add a free pool of entries,so that` |
|         - | 6401 | `				 * we avoid wasting spaces.` |
|         - | 6402 | `				 */` |
|    615992 | 6403 | `			}` |
|    616271 | 6404 | `		}` |
|    615993 | 6405 | `	}` |
|  12035243 | 6406 | `	if( pMapEntry ){` |
|         - | 6407 | `		ph7_hashmap_node **apNode;` |
|  10805852 | 6408 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  21612579 | 6409 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  10806732 | 6410 | `			if( apNode[n] == pMapEntry ){` |
|         - | 6411 | `				/* nullify the entry */` |
|  10805852 | 6412 | `				apNode[n] = 0;` |
|   5402948 | 6413 | `			}` |
|   5403393 | 6414 | `		}` |
|   5402948 | 6415 | `	}` |
|  12035243 | 6416 | `	return SXRET_OK;` |
|   6018948 | 6417 | `}` |
|         - | 6418 | `/*` |
|         - | 6419 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|         - | 6420 | ` * names bound to it plus the array nodes pointing at it. php refcounts a` |
|         - | 6421 | ` * reference set and keeps the VALUE alive while any holder remains, so this is` |
|         - | 6422 | ` * the count every "may I release this slot?" decision asks for.` |
|         - | 6423 | ` *` |
|         - | 6424 | ` * A row is only a holder while it is non-NULL (every holder's death nullifies` |
|         - | 6425 | ` * its own row through PH7_VmRefObjRemove) and, for a node, while it still points` |
|         - | 6426 | ` * HERE — a slot index travels through the free list, so a record can outlive the` |
|         - | 6427 | ` * node that filed the row (the same filter VmUnsetVarByName applies).` |
|         - | 6428 | ` */` |
|  13299211 | 6429 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6430 | `{` |
|         - | 6431 | `	ph7_hashmap_node **apNode;` |
|         - | 6432 | `	SyHashEntry **apEntry;` |
|         - | 6433 | `	VmRefObj *pRef;` |
|  13299216 | 6434 | `	sxu32 n, nLive = 0;` |
|  13299216 | 6435 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6436 | `		return 0;` |
|         - | 6437 | `	}` |
|  13299216 | 6438 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  13299216 | 6439 | `	if( pRef == 0 ){` |
|         3 | 6440 | `		return 0;` |
|         - | 6441 | `	}` |
|  13299214 | 6442 | `	if( pRef->nPin > 0 ){` |
|         - | 6443 | `		/* Holders the table cannot name, counted: reference-bound properties. */` |
|        13 | 6444 | `		nLive += pRef->nPin;` |
|  13299208 | 6445 | `	}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|         - | 6446 | ``		/* A permanent pin — a `use (&$x)` capture, a static, an enum case. It is the`` |
|         - | 6447 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|         - | 6448 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|         - | 6449 | `		 * php's refcount does. */` |
|         5 | 6450 | `		nLive++;` |
|         2 | 6451 | `	}` |
|  13299214 | 6452 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|  14538037 | 6453 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|   1238828 | 6454 | `		if( apEntry[n] ){` |
|       253 | 6455 | `			nLive++;` |
|       124 | 6456 | `		}` |
|    620714 | 6457 | `	}` |
|  13299214 | 6458 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  25374384 | 6459 | `	for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  12075175 | 6460 | `		if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|   1262178 | 6461 | `			nLive++;` |
|    631094 | 6462 | `		}` |
|   6037622 | 6463 | `	}` |
|  13299214 | 6464 | `	return nLive;` |
|   6650940 | 6465 | `}` |
|         - | 6466 | `/*` |
|         - | 6467 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|         - | 6468 | ` * holds it (php frees the value with the last reference, not with the first one` |
|         - | 6469 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|         - | 6470 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|         - | 6471 | ` */` |
|  10815703 | 6472 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6473 | `{` |
|         - | 6474 | `	VmRefObj *pRef;` |
|  10815708 | 6475 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6476 | `		return;` |
|         - | 6477 | `	}` |
|  10815708 | 6478 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  10815708 | 6479 | `	if( pRef ){` |
|  10815664 | 6480 | `		if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|        61 | 6481 | `			return; /* pinned past its frame — its holder is not in the table */` |
|         - | 6482 | `		}` |
|  10815604 | 6483 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|      3523 | 6484 | `			return; /* somebody still holds it */` |
|         - | 6485 | `		}` |
|   5406064 | 6486 | `	}` |
|         - | 6487 | `	/* No record at all means nothing was ever registered against the slot, which is` |
|         - | 6488 | `	 * the same answer as a count of zero — release it (this is what every caller did` |
|         - | 6489 | `	 * unconditionally before the holder rule). */` |
|  10812128 | 6490 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|         - | 6491 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|         - | 6492 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
|  10812128 | 6493 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|   5407881 | 6494 | `}` |
|         - | 6495 | `/*` |
|         - | 6496 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|         - | 6497 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|         - | 6498 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|         - | 6499 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|         - | 6500 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|         - | 6501 | ` */` |
|      9882 | 6502 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|         5 | 6503 | `{` |
|         - | 6504 | `	VmFrame *pFrame;` |
|     22913 | 6505 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     13031 | 6506 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     13031 | 6507 | `		sxu32 n = 0;` |
|     25425 | 6508 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|     12396 | 6509 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|         - | 6510 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|         - | 6511 | `				 * same index — it now holds the row swapped in from the tail. */` |
|      3110 | 6512 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|      3110 | 6513 | `				(void)SySetPop(&pFrame->sRef);` |
|      3110 | 6514 | `				continue;` |
|         - | 6515 | `			}` |
|      9288 | 6516 | `			n++;` |
|         2 | 6517 | `		}` |
|      6518 | 6518 | `	}` |
|      9887 | 6519 | `}` |
|         - | 6520 | `/*` |
|         - | 6521 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|         - | 6522 | ` *` |
|         - | 6523 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|         - | 6524 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|         - | 6525 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|         - | 6526 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|         - | 6527 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|         - | 6528 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|         - | 6529 | ` */` |
|     25690 | 6530 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|         5 | 6531 | `{` |
|     25695 | 6532 | `	return nByte > 0 && zName[0] == '[';` |
|         5 | 6533 | `}` |
|         - | 6534 | `/*` |
|         - | 6535 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|         - | 6536 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|         - | 6537 | ` * its value variable on every step, and it goes through the reference table like any` |
|         - | 6538 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|         - | 6539 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|         - | 6540 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|         - | 6541 | ` */` |
|       722 | 6542 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 6543 | `	sxu32 nIdx)` |
|         5 | 6544 | `{` |
|       727 | 6545 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       727 | 6546 | `	if( pEntry ){` |
|       102 | 6547 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|       102 | 6548 | `		return;` |
|         - | 6549 | `	}` |
|       629 | 6550 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|       ! 0 | 6551 | `		return;` |
|         - | 6552 | `	}` |
|       629 | 6553 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|         - | 6554 | `		/* A global is also an entry of the $GLOBALS view */` |
|        42 | 6555 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        19 | 6556 | `	}` |
|       629 | 6557 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       366 | 6558 | `}` |
|         - | 6559 | `/*` |
|         - | 6560 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|         - | 6561 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|         - | 6562 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|         - | 6563 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|         - | 6564 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|         - | 6565 | ` * and left the OLD binding standing, so every later write through the name went to` |
|         - | 6566 | ` * the wrong variable.` |
|         - | 6567 | ` *` |
|         - | 6568 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|         - | 6569 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|         - | 6570 | ` * compares it by identity.` |
|         - | 6571 | ` */` |
|      3184 | 6572 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|         - | 6573 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 6574 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|         - | 6575 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|         - | 6576 | `	const char *zName,    /* Variable name */` |
|         - | 6577 | `	sxu32 nByte,          /* Name length */` |
|         - | 6578 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|         - | 6579 | `	)` |
|         4 | 6580 | `{` |
|      3188 | 6581 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      3188 | 6582 | `	if( nOld == nIdx ){` |
|         - | 6583 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|       ! 0 | 6584 | `		return;` |
|         - | 6585 | `	}` |
|         - | 6586 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|         - | 6587 | `	 * "release this reference at exit" set */` |
|      3188 | 6588 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|      3188 | 6589 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|      3188 | 6590 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|      3188 | 6591 | `	if( pFrame->pParent == 0 ){` |
|         - | 6592 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|         - | 6593 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|         - | 6594 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|         - | 6595 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|       108 | 6596 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|       104 | 6597 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|       108 | 6598 | `		 && pGlobalNode ){` |
|       108 | 6599 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|       108 | 6600 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|       108 | 6601 | `				pGlobalNode->nValIdx = nIdx;` |
|       108 | 6602 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|        52 | 6603 | `			}` |
|        56 | 6604 | `		}else{` |
|         - | 6605 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|       ! 0 | 6606 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|         - | 6607 | `		}` |
|        52 | 6608 | `	}` |
|      3188 | 6609 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|         - | 6610 | `	/* The old value dies with its last holder — and only then */` |
|      3188 | 6611 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|      1596 | 6612 | `}` |
|         - | 6613 | `/*` |
|         - | 6614 | ` * Is there a SCHEME at the front of this name, and how long is it?` |
|         - | 6615 | ` *` |
|         - | 6616 | ` * php reads one only at the START, and only as a URL scheme: a run of` |
|         - | 6617 | ` * [A-Za-z0-9+.-] at least TWO characters long, followed immediately by "://".` |
|         - | 6618 | ` * PHL used to hunt for the first "://" ANYWHERE in the name and then trim` |
|         - | 6619 | ` * whitespace off whatever preceded it, which made four ordinary FILENAMES into` |
|         - | 6620 | ` * URLs: " php://memory" and "php ://memory" opened the memory stream php opens` |
|         - | 6621 | ` * a file called that, "./sub://z" and "a b://c" were looked up as schemes` |
|         - | 6622 | ` * "./sub" and "a b". The two-character minimum is php's, and it is what keeps a` |
|         - | 6623 | ` * Windows drive letter ("C://tmp") a path rather than a "C" scheme.` |
|         - | 6624 | ` */` |
|    114731 | 6625 | `static int VmUrlScheme(const char *zIn,int nByte,int *pnScheme)` |
|         5 | 6626 | `{` |
|    114736 | 6627 | `	int i = 0;` |
|    647584 | 6628 | `	while( i < nByte ){` |
|    647540 | 6629 | `		int c = zIn[i];` |
|    647535 | 6630 | `		if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9')` |
|    115203 | 6631 | `		 \|\| c == '+' \|\| c == '-' \|\| c == '.' ){` |
|    532853 | 6632 | `			i++;` |
|    532853 | 6633 | `			continue;` |
|         - | 6634 | `		}` |
|    114692 | 6635 | `		break;` |
|       ! 0 | 6636 | `	}` |
|         - | 6637 | `	/* php also accepts a scheme with NOTHING after it ("zzz://" is an unknown` |
|         - | 6638 | `	 * wrapper, not a file called "zzz://"), which the old scan refused. */` |
|    114736 | 6639 | `	if( i > 1 && i + 2 < nByte && zIn[i] == ':' && zIn[i+1] == '/' && zIn[i+2] == '/' ){` |
|       783 | 6640 | `		*pnScheme = i;` |
|       783 | 6641 | `		return 1;` |
|         - | 6642 | `	}` |
|    113958 | 6643 | `	return 0;` |
|     57389 | 6644 | `}` |
|         - | 6645 | `/*` |
|         - | 6646 | ` * The same question from outside vm.c: how long is the scheme, or 0 for a name` |
|         - | 6647 | ` * that has none. stream_resolve_include_path() asks it to decide whether a name` |
|         - | 6648 | ` * is walkable at all.` |
|         - | 6649 | ` */` |
|        52 | 6650 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte)` |
|         2 | 6651 | `{` |
|        54 | 6652 | `	int nScheme = 0;` |
|        54 | 6653 | `	if( zIn == 0 ){` |
|       ! 0 | 6654 | `		return 0;` |
|         - | 6655 | `	}` |
|        54 | 6656 | `	if( nByte < 0 ){` |
|        25 | 6657 | `		nByte = (int)SyStrlen(zIn);` |
|        12 | 6658 | `	}` |
|        54 | 6659 | `	return VmUrlScheme(zIn,nByte,&nScheme) ? nScheme : 0;` |
|        28 | 6660 | `}` |
|         - | 6661 | `/*` |
|         - | 6662 | ` * The bytes the FILE wrapper is handed for a file:// URL.` |
|         - | 6663 | ` *` |
|         - | 6664 | ` * A file:// URL has an AUTHORITY, and php only accepts two of them: an empty` |
|         - | 6665 | `` * one and `localhost` (case-insensitively, and only with its slash). Anything`` |
|         - | 6666 | ` * else is a remote host it refuses to reach -- where PHL stripped exactly` |
|         - | 6667 | `` * "file://" and opened whatever was left, so `file://tmp/passwd` silently read`` |
|         - | 6668 | ` * the RELATIVE path tmp/passwd. What survives the strip is the LAST slash of` |
|         - | 6669 | `` * the leading run, so `file:////x` is /x, `file://localhost//x` is /x, and`` |
|         - | 6670 | `` * `file://` on its own is the root directory.`` |
|         - | 6671 | ` *` |
|         - | 6672 | ` * Returns 0 for an authority php will not reach; the caller answers "no` |
|         - | 6673 | ` * wrapper", as php does.` |
|         - | 6674 | ` */` |
|        62 | 6675 | `static int VmFileUrlPath(const char *zIn,int nByte,int nScheme,const char **pzPath)` |
|         4 | 6676 | `{` |
|         - | 6677 | `	static const char zLocal[] = "file://localhost/";` |
|        66 | 6678 | `	const char *zPath = &zIn[nScheme+1]; /* the first slash of "://" */` |
|        66 | 6679 | `	const char *zEnd = &zIn[nByte];` |
|        62 | 6680 | `	if( nScheme + 3 < nByte && zIn[nScheme+3] != '/'` |
|         - | 6681 | `#ifdef __WINNT__` |
|         - | 6682 | ``	 /* php's own Windows allowance: `file://C:/x` is a DRIVE, not a host. */`` |
|         4 | 6683 | `	 && !(nScheme + 4 < nByte && zIn[nScheme+4] == ':')` |
|         - | 6684 | `#endif` |
|         - | 6685 | `	){` |
|        32 | 6686 | `		if( nByte < (int)sizeof(zLocal)-1` |
|        35 | 6687 | `		 \|\| SyStrnicmp(zIn,zLocal,(sxu32)sizeof(zLocal)-1) != 0 ){` |
|        31 | 6688 | `			return 0; /* a host this build (and php) will not fetch from */` |
|         - | 6689 | `		}` |
|         4 | 6690 | `		zPath = &zIn[nScheme+3+sizeof("localhost")-1];` |
|         2 | 6691 | `	}` |
|        99 | 6692 | `	while( &zPath[1] < zEnd && zPath[1] == '/' ){` |
|        65 | 6693 | `		zPath++;` |
|         3 | 6694 | `	}` |
|        37 | 6695 | `	*pzPath = zPath;` |
|        37 | 6696 | `	return 1;` |
|        35 | 6697 | `}` |
|         - | 6698 | `/*` |
|         - | 6699 | ` * The same rule for the VFS side, which stats and unlinks a name without ever` |
|         - | 6700 | ` * going through a stream device. It carried a second, shorter copy of the` |
|         - | 6701 | `` * strip -- no slash-run collapse and nothing for a bare `file://` -- so`` |
|         - | 6702 | `` * `is_dir('file://')` was false where php names the root. A host this build`` |
|         - | 6703 | ` * will not reach is handed back UNCHANGED: the syscall then fails on a name` |
|         - | 6704 | ` * that is not a path, which is the FALSE php answers for it.` |
|         - | 6705 | ` */` |
|     73713 | 6706 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath)` |
|         5 | 6707 | `{` |
|         - | 6708 | `	const char *zOut;` |
|     73718 | 6709 | `	int nByte,nScheme = 0;` |
|     73718 | 6710 | `	if( zPath == 0 ){` |
|       ! 0 | 6711 | `		return zPath;` |
|         - | 6712 | `	}` |
|     73718 | 6713 | `	nByte = (int)SyStrlen(zPath);` |
|     73713 | 6714 | `	if( !VmUrlScheme(zPath,nByte,&nScheme)` |
|     36902 | 6715 | `	 \|\| nScheme != (int)sizeof("file")-1` |
|        46 | 6716 | `	 \|\| SyStrnicmp(zPath,"file",sizeof("file")-1) != 0 ){` |
|     73696 | 6717 | `		return zPath;` |
|         - | 6718 | `	}` |
|        25 | 6719 | `	if( !VmFileUrlPath(zPath,nByte,nScheme,&zOut) ){` |
|         6 | 6720 | `		return zPath;` |
|         - | 6721 | `	}` |
|         - | 6722 | `#ifdef __WINNT__` |
|         - | 6723 | `	/* The one piece that is only true here: the leading slash php's own strip` |
|         - | 6724 | `	 * leaves in front of a DRIVE is not part of a Windows path, so` |
|         - | 6725 | `	 * file:///C:/x and file://localhost/C:/x both name C:/x. */` |
|         2 | 6726 | `	if( zOut[0] == '/' && zOut[1] != 0 && zOut[2] == ':' ){` |
|         2 | 6727 | `		zOut++;` |
|         - | 6728 | `	}` |
|         - | 6729 | `#endif` |
|        20 | 6730 | `	return zOut;` |
|     36880 | 6731 | `}` |
|         - | 6732 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 6733 | `/*` |
|         - | 6734 | ` * Has a script taken this device out of service with stream_wrapper_unregister()?` |
|         - | 6735 | ` */` |
|     41220 | 6736 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|         5 | 6737 | `{` |
|     41225 | 6738 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|         - | 6739 | `	sxu32 n;` |
|     41337 | 6740 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|       156 | 6741 | `		if( apOff[n] == pStream ){` |
|        43 | 6742 | `			return 1;` |
|         - | 6743 | `		}` |
|        58 | 6744 | `	}` |
|     41183 | 6745 | `	return 0;` |
|     20615 | 6746 | `}` |
|         - | 6747 | `/*` |
|         - | 6748 | ` * The device currently answering to a scheme name, or NULL. The scan runs to` |
|         - | 6749 | ` * the END rather than stopping at the first hit: once a built-in has been` |
|         - | 6750 | ` * unregistered a userland wrapper can be registered under the same name, both` |
|         - | 6751 | ` * sit in the list, and the LIVE one is the later of the two.` |
|         - | 6752 | ` */` |
|     40930 | 6753 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName)` |
|         5 | 6754 | `{` |
|     40935 | 6755 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     40935 | 6756 | `	ph7_io_stream *pHit = 0;` |
|     40935 | 6757 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|     40935 | 6758 | `	if( nName < 0 ){` |
|       ! 0 | 6759 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 6760 | `	}` |
|    245717 | 6761 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|    204787 | 6762 | `		ph7_io_stream *pStream = apStream[n];` |
|    204782 | 6763 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|    163528 | 6764 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|    163861 | 6765 | `			continue;` |
|         - | 6766 | `		}` |
|     40931 | 6767 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|        15 | 6768 | `			continue;` |
|         - | 6769 | `		}` |
|     40917 | 6770 | `		pHit = pStream;` |
|     20461 | 6771 | `	}` |
|     40935 | 6772 | `	return pHit;` |
|         5 | 6773 | `}` |
|         - | 6774 | `/*` |
|         - | 6775 | ` * Is this scheme one the build HAS but the script has switched off? php words` |
|         - | 6776 | ` * that differently from a scheme nothing was ever registered under -- but only` |
|         - | 6777 | ` * for file://, whose plain-files fallback is the branch that reports it.` |
|         - | 6778 | ` */` |
|         2 | 6779 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName)` |
|         1 | 6780 | `{` |
|         3 | 6781 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|         3 | 6782 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|         3 | 6783 | `	int bSeen = 0;` |
|         3 | 6784 | `	if( nName < 0 ){` |
|       ! 0 | 6785 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 6786 | `	}` |
|        15 | 6787 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        13 | 6788 | `		ph7_io_stream *pStream = apStream[n];` |
|        12 | 6789 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|        10 | 6790 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|        11 | 6791 | `			continue;` |
|         - | 6792 | `		}` |
|         3 | 6793 | `		if( !PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|       ! 0 | 6794 | `			return 0; /* it is live */` |
|         - | 6795 | `		}` |
|         3 | 6796 | `		bSeen = 1;` |
|         2 | 6797 | `	}` |
|         3 | 6798 | `	return bSeen;` |
|         2 | 6799 | `}` |
|         - | 6800 | `/*` |
|         - | 6801 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 6802 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 6803 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 6804 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 6805 | ` * For more information on how to register IO stream devices,please` |
|         - | 6806 | ` * refer to the official documentation.` |
|         - | 6807 | ` */` |
|     40942 | 6808 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 6809 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 6810 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 6811 | `	int nByte              /* *pzDevice length*/` |
|         - | 6812 | `	)` |
|         5 | 6813 | `{` |
|         - | 6814 | `	const char *zIn,*zNext;` |
|         - | 6815 | `	ph7_io_stream *pStream;` |
|     40947 | 6816 | `	int nScheme = 0;` |
|         - | 6817 | `	/* A failed open names the URI the SCRIPT wrote, and every caller from here` |
|         - | 6818 | `	 * on holds only what is left after the scheme -- so both halves are` |
|         - | 6819 | `	 * remembered as the scheme comes off, and forgotten on every arm that does` |
|         - | 6820 | `	 * not take one off (see VfsThrowOpenWarning). */` |
|     40947 | 6821 | `	if( pVm->nOpenDepth < 1 ){` |
|     40901 | 6822 | `		pVm->zOpenUri = 0;` |
|     40901 | 6823 | `		pVm->zOpenUriTail = 0;` |
|     40901 | 6824 | `		pVm->nOpenUri = 0;` |
|         - | 6825 | `		/* The reason goes with them: a caller that resolves a device and then` |
|         - | 6826 | `		 * declines to open it (dom asks for xRead/xWrite first) must not report` |
|         - | 6827 | `		 * the PREVIOUS open's wrapper reason. */` |
|     40901 | 6828 | `		pVm->zOpenErr = 0;` |
|     20448 | 6829 | `	}` |
|         - | 6830 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|     40947 | 6831 | `	zIn = *pzDevice;` |
|     40947 | 6832 | `	if( !VmUrlScheme(zIn,nByte,&nScheme) ){` |
|         - | 6833 | `		/* No scheme: php's default is the plain-files wrapper, and it is the` |
|         - | 6834 | `		 * SAME slot file:// names -- so a script that unregisters file:// loses` |
|         - | 6835 | `		 * the bare-path open too, and one that registers its own wrapper over` |
|         - | 6836 | `		 * file:// gets bare paths routed through it. Looking the name up rather` |
|         - | 6837 | `		 * than answering pDefStream is what makes both true. */` |
|     40257 | 6838 | `		return PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|         - | 6839 | `	}` |
|       695 | 6840 | `	zNext = &zIn[nScheme+sizeof("://")-1];` |
|         - | 6841 | `	/* php applies the file:// authority rules by the SCHEME NAME, before it` |
|         - | 6842 | `	 * cares who is registered under it -- a userland wrapper that replaced` |
|         - | 6843 | `	 * file:// is handed the stripped path too. */` |
|       695 | 6844 | `	if( nScheme == (int)sizeof("file")-1 && SyStrnicmp(zIn,"file",sizeof("file")-1) == 0 ){` |
|        31 | 6845 | `		if( !VmFileUrlPath(zIn,nByte,nScheme,&zNext) ){` |
|        13 | 6846 | `			return 0;` |
|         - | 6847 | `		}` |
|         8 | 6848 | `	}` |
|       683 | 6849 | `	pStream = PH7_VmFindStreamDevice(pVm,zIn,nScheme);` |
|       683 | 6850 | `	if( pStream == 0 ){` |
|         - | 6851 | `		/* No such stream -- or one a script has taken out of service. */` |
|        19 | 6852 | `		return 0;` |
|         - | 6853 | `	}` |
|       667 | 6854 | `	*pzDevice = zNext;` |
|       667 | 6855 | `	if( pVm->nOpenDepth < 1 ){` |
|       665 | 6856 | `		pVm->zOpenUri = zIn;` |
|       665 | 6857 | `		pVm->nOpenUri = nByte;` |
|       665 | 6858 | `		pVm->zOpenUriTail = zNext;` |
|       330 | 6859 | `	}` |
|       667 | 6860 | `	return pStream;` |
|     20476 | 6861 | `}` |
|         - | 6862 | `/*` |
|         - | 6863 | ` * Why did PH7_VmGetStreamDevice() answer nothing for this name? php raises a` |
|         - | 6864 | ` * REASON of its own before the operation's own failure, and the two a caller` |
|         - | 6865 | ` * can hit here are different sentences. Re-derived from the name rather than` |
|         - | 6866 | ` * threaded out of the lookup, so every call site stays one line.` |
|         - | 6867 | ` *` |
|         - | 6868 | ` * Answers TRUE for the file:// authority php will not reach; otherwise FALSE` |
|         - | 6869 | ` * with *pnScheme set to the length of the scheme that has no wrapper.` |
|         - | 6870 | ` */` |
|        24 | 6871 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme)` |
|         2 | 6872 | `{` |
|         - | 6873 | `	const char *zPath;` |
|        26 | 6874 | `	int nScheme = 0;` |
|        26 | 6875 | `	if( nByte < 0 ){` |
|        26 | 6876 | `		nByte = (int)SyStrlen(zUri);` |
|        12 | 6877 | `	}` |
|        26 | 6878 | `	if( !VmUrlScheme(zUri,nByte,&nScheme) ){` |
|         3 | 6879 | `		*pnScheme = 0;` |
|         3 | 6880 | `		return 0;` |
|         - | 6881 | `	}` |
|        24 | 6882 | `	*pnScheme = nScheme;` |
|        31 | 6883 | `	return nScheme == (int)sizeof("file")-1` |
|        18 | 6884 | `		&& SyStrnicmp(zUri,"file",sizeof("file")-1) == 0` |
|        29 | 6885 | `		&& !VmFileUrlPath(zUri,nByte,nScheme,&zPath);` |
|        14 | 6886 | `}` |
|         - | 6887 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 6888 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 6889 |  |
