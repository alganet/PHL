# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4837/5485 lines (88.19%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits |  Line | Source |
| -------: | ----: | :--- |
|        - |     1 | `/**` |
|        - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |     5 | ` */` |
|        - |     6 | `#include "ph7int.h"` |
|        - |     7 | `#include <stddef.h>` |
|        - |     8 | `#include <stdlib.h>` |
|        - |     9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |    10 | `#include <math.h>` |
|        - |    11 | `#endif` |
|        - |    12 | `/* Signed 64-bit integer overflow detection lives in ph7int.h as the shared` |
|        - |    13 | ` * PH7_{ADD,SUB,MUL}_OVERFLOW64 macros (GCC/Clang intrinsics, MSVC fallbacks in` |
|        - |    14 | ` * memobj.c). The executor uses them to promote an overflowing integer` |
|        - |    15 | ` * operation to a float, matching PHP. */` |
|        - |    16 | `/*` |
|        - |    17 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    18 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    19 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    20 | ` * statements.` |
|        - |    21 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    22 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    23 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    24 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    25 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    26 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    27 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    28 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    29 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    30 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    31 | ` * and so on.` |
|        - |    32 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    33 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    34 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    35 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    36 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    37 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    38 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    39 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    40 | ` */` |
|        - |    41 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |    42 | `/*` |
|        - |    43 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    44 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    45 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    46 | ` * a new memory object.` |
|        - |    47 | ` */` |
|        - |    48 | `/* VmSlot struct moved to ph7int.h */` |
|        - |    49 | `/*` |
|        - |    50 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    51 | ` * follwoing table.` |
|        - |    52 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    53 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    54 | ` * the reference implementation is consistent,solid and it's` |
|        - |    55 | ` * behavior resemble the C++ reference mechanism.` |
|        - |    56 | ` * Refer to the official for more information on this powerful` |
|        - |    57 | ` * extension.` |
|        - |    58 | ` */` |
|        - |    59 | `/* struct VmRefObj + VM_REF_IDX_KEEP moved to ph7int.h */` |
|        - |    60 | `/*` |
|        - |    61 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    62 | ` * is stored in an instance of the following structure.` |
|        - |    63 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |    64 | ` */` |
|        - |    65 | `/* VmShutdownCB struct moved to ph7int.h */` |
|        - |    66 | `/*` |
|        - |    67 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |    68 | ` * is stored in an instance of the following structure.` |
|        - |    69 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |    70 | ` */` |
|        - |    71 | `/* VmAutoloadCB struct moved to ph7int.h */` |
|        - |    72 |  |
|        - |    73 | `/*` |
|        - |    74 | ` * TRUE when php compares these two operands as UNORDERED -- a NaN against` |
|        - |    75 | ` * something php reads as a NUMBER or as a STRING. php answers 1 for that` |
|        - |    76 | `` * comparison in BOTH directions, which is what makes `==`, `<`, `>`, `<=` and`` |
|        - |    77 | `` * `>=` all false at once while `<=>` is 1 either way round.`` |
|        - |    78 | ` *` |
|        - |    79 | ` * Two rules ride on this predicate and both were wrong without them.` |
|        - |    80 | ` *` |
|        - |    81 | ` * It must be asked BEFORE PH7_MemObjCmp runs: the comparator converts its` |
|        - |    82 | ` * operands IN PLACE, so a NaN that took the string path is a MEMOBJ_STRING by` |
|        - |    83 | ` * the time the answer comes back and the float is gone. That is how` |
|        - |    84 | `` * `NAN == "NAN"` was TRUE here (php: false) and `NAN < "abc"` was TRUE`` |
|        - |    85 | ` * (php: false) -- the screen ran on two strings and saw no NaN at all.` |
|        - |    86 | ` *` |
|        - |    87 | ` * And php's own precedence comes FIRST: a comparison against null, a bool, an` |
|        - |    88 | ``  * array or an object never reaches the numeric/string rule, so `NAN == true` `` |
|        - |    89 | `` * is TRUE (both truthy) and `NAN < []` is TRUE (an array is greater). Those`` |
|        - |    90 | ` * flags are exactly the branches PH7_MemObjCmp answers ahead of its numeric` |
|        - |    91 | ` * one. A RESOURCE is not among them: php reads it as its ID there, so a NaN` |
|        - |    92 | ` * against one is as unordered as a NaN against any other number.` |
|        - |    93 | ` */` |
|  7323846 |    94 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |    95 | `{` |
|  7323846 |    96 | `	if( (pLeft->iFlags \| pRight->iFlags)` |
|  7323851 |    97 | `	  & (MEMOBJ_NULL\|MEMOBJ_BOOL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ) ){` |
|      ! 0 |    98 | `		return FALSE;` |
|        - |    99 | `	}` |
|  7323851 |   100 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|      383 |   101 | `		return TRUE;` |
|        - |   102 | `	}` |
|  7323471 |   103 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|      174 |   104 | `		return TRUE;` |
|        - |   105 | `	}` |
|  7323299 |   106 | `	return FALSE;` |
|  3670730 |   107 | `}` |
|        - |   108 | `/*` |
|        - |   109 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   110 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   111 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   112 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   113 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   114 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   115 | ` * still go through the existing numeric coercion.` |
|        - |   116 | ` */` |
|   955118 |   117 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   118 | `{` |
|        - |   119 | `	SyString sStr;` |
|   955123 |   120 | `	sxu8 bReal = FALSE;` |
|   955123 |   121 | `	const char *zTail = 0;` |
|        - |   122 | `	const char *zEnd;` |
|   955123 |   123 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   955105 |   124 | `		return FALSE;` |
|        - |   125 | `	}` |
|       21 |   126 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       21 |   127 | `	if( sStr.nByte == 0 ){` |
|      ! 0 |   128 | `		return TRUE;` |
|        - |   129 | `	}` |
|       21 |   130 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|        5 |   131 | `		return TRUE;` |
|        - |   132 | `	}` |
|        - |   133 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   134 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   135 | `	 * still takes the Perl path. */` |
|       17 |   136 | `	zEnd = sStr.zString + sStr.nByte;` |
|       17 |   137 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|      ! 0 |   138 | `		zTail++;` |
|      ! 0 |   139 | `	}` |
|       17 |   140 | `	return zTail < zEnd;` |
|   478602 |   141 | `}` |
|        - |   142 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   143 | `/* Constant expander used by define(); used below to recognise user-defined` |
|        - |   144 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|        - |   145 | ` * a define() overwrites them. */` |
|        - |   146 | `/*` |
|        - |   147 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   148 | ` * it can be expanded from the target PHP program.` |
|        - |   149 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   150 | ` * simple and work as follows:` |
|        - |   151 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   152 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   153 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   154 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   155 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   156 | ` * (Windows,Linux,...) and so on.` |
|        - |   157 | ` * Please refer to the official documentation for additional information.` |
|        - |   158 | ` */` |
| 12993589 |   159 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   160 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   161 | `	const SyString *pName,  /* Constant name */` |
|        - |   162 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   163 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   164 | `	)` |
|        5 |   165 | `{` |
| 12993594 |   166 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|        5 |   167 | `}` |
|        - |   168 | `/*` |
|        - |   169 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|        - |   170 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|        - |   171 | ` */` |
| 12993931 |   172 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
|        - |   173 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   174 | `	const SyString *pName,  /* Constant name */` |
|        - |   175 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   176 | `	void *pUserData,        /* Last argument to xExpand() */` |
|        - |   177 | `	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */` |
|        - |   178 | `	sxu32 nLine,            /* Declaration line, 0 = unknown */` |
|        - |   179 | `	int bUser               /* 1 when defined by user code */` |
|        - |   180 | `	)` |
|        5 |   181 | `{` |
|        - |   182 | `	ph7_constant *pCons;` |
|        - |   183 | `	SyHashEntry *pEntry;` |
|        - |   184 | `	char *zDupName;` |
|        - |   185 | `	sxi32 rc;` |
| 12993936 |   186 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
| 12993936 |   187 | `	if( pEntry ){` |
|        - |   188 | `		/* Overwrite the old definition and return immediately */` |
|        3 |   189 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |   190 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|        - |   191 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|        - |   192 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|        2 |   193 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|        3 |   194 | `		 && pCons->pUserData != pUserData ){` |
|        3 |   195 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|        3 |   196 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|        1 |   197 | `		}` |
|        3 |   198 | `		pCons->xExpand = xExpand;` |
|        3 |   199 | `		pCons->pUserData = pUserData;` |
|        3 |   200 | `		if( pFile ){` |
|        3 |   201 | `			SyStringDupPtr(&pCons->sFile,pFile);` |
|        2 |   202 | `		}else{` |
|      ! 0 |   203 | `			SyStringInitFromBuf(&pCons->sFile,0,0);` |
|        - |   204 | `		}` |
|        3 |   205 | `		pCons->nLine = nLine;` |
|        3 |   206 | `		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|        3 |   207 | `		pCons->zDeprecated = 0;     /* ...and its deprecation, which was the old symbol's */` |
|        3 |   208 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|        3 |   209 | `		return SXRET_OK;` |
|        - |   210 | `	}` |
|        - |   211 | `	/* Allocate a new constant instance */` |
| 12993934 |   212 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
| 12993934 |   213 | `	if( pCons == 0 ){` |
|      ! 0 |   214 | `		return 0;` |
|        - |   215 | `	}` |
|        - |   216 | `	/* Duplicate constant name */` |
| 12993934 |   217 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 12993934 |   218 | `	if( zDupName == 0 ){` |
|      ! 0 |   219 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   220 | `		return 0;` |
|        - |   221 | `	}` |
| 12993934 |   222 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
| 12993934 |   223 | `	if( pFile ){` |
|      345 |   224 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|      170 |   225 | `	}` |
| 12993934 |   226 | `	pCons->nLine = nLine;` |
| 12993934 |   227 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
| 12993934 |   228 | `	pCons->zDeprecated = 0;` |
|        - |   229 | `	/* Install the constant */` |
| 12993934 |   230 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
| 12993934 |   231 | `	pCons->xExpand = xExpand;` |
| 12993934 |   232 | `	pCons->pUserData = pUserData;` |
| 12993934 |   233 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
| 12993934 |   234 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|        - |   235 | `	/* A name that was not a constant is one now, so every PH7_OP_LOADC site that` |
|        - |   236 | `	 * remembers what its name resolved to has to ask again -- including one whose` |
|        - |   237 | `	 * namespaced candidate used to MISS and fall through to the global literal. */` |
| 12993934 |   238 | `	pVm->nConstGen++;` |
| 12993934 |   239 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   240 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   241 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   242 | `		return rc;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* All done,constant can be invoked from PHP code */` |
| 12993934 |   245 | `	return SXRET_OK;` |
|  6297310 |   246 | `}` |
|        - |   247 | `/*` |
|        - |   248 | ` * Allocate a new foreign function instance.` |
|        - |   249 | ` * This function return SXRET_OK on success. Any other` |
|        - |   250 | ` * return value indicates failure.` |
|        - |   251 | ` * Please refer to the official documentation for an introduction to` |
|        - |   252 | ` * the foreign function mechanism.` |
|        - |   253 | ` */` |
| 20090180 |   254 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
|        - |   255 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   256 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   257 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   258 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   259 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   260 | `	)` |
|        5 |   261 | `{` |
|        - |   262 | `	ph7_user_func *pFunc;` |
|        - |   263 | `	char *zDup;` |
|        - |   264 | `	/* Allocate a new user function */` |
| 20090185 |   265 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
| 20090185 |   266 | `	if( pFunc == 0 ){` |
|      ! 0 |   267 | `		return SXERR_MEM;` |
|        - |   268 | `	}` |
|        - |   269 | `	/* Duplicate function name */` |
| 20090185 |   270 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 20090185 |   271 | `	if( zDup == 0 ){` |
|      ! 0 |   272 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Zero the structure */` |
| 20090185 |   276 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   277 | `	/* Initialize structure fields */` |
| 20090185 |   278 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
| 20090185 |   279 | `	pFunc->pVm   = pVm;` |
| 20090185 |   280 | `	pFunc->xFunc = xFunc;` |
| 20090185 |   281 | `	pFunc->pUserData = pUserData;` |
| 20090185 |   282 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   283 | `	/* Write a pointer to the new function */` |
| 20090185 |   284 | `	*ppOut = pFunc;` |
| 20090185 |   285 | `	return SXRET_OK;` |
| 10019265 |   286 | `}` |
|        - |   287 | `/*` |
|        - |   288 | ` * Install a foreign function and it's associated callback so that` |
|        - |   289 | ` * it can be invoked from the target PHP code.` |
|        - |   290 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   291 | ` * return value indicates failure.` |
|        - |   292 | ` * Please refer to the official documentation for an introduction to` |
|        - |   293 | ` * the foreign function mechanism.` |
|        - |   294 | ` */` |
|  8931508 |   295 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   296 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   297 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   298 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   299 | `	void *pUserData           /* Foreign function private data */` |
|        - |   300 | `	)` |
|        5 |   301 | `{` |
|        - |   302 | `	ph7_user_func *pFunc;` |
|        - |   303 | `	SyHashEntry *pEntry;` |
|        - |   304 | `	sxi32 rc;` |
|        - |   305 | `	/* Overwrite any previously registered function with the same name */` |
|  8931513 |   306 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  8931513 |   307 | `	if( pEntry ){` |
|      ! 0 |   308 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      ! 0 |   309 | `		pFunc->pUserData = pUserData;` |
|      ! 0 |   310 | `		pFunc->xFunc = xFunc;` |
|      ! 0 |   311 | `		SySetReset(&pFunc->aAux);` |
|        - |   312 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|        - |   313 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|        - |   314 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|        - |   315 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|      ! 0 |   316 | `		pFunc->nMinArg  = 0;` |
|      ! 0 |   317 | `		pFunc->nMaxArg  = 0;` |
|      ! 0 |   318 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|      ! 0 |   319 | `		pFunc->bAtLeast = 0;` |
|      ! 0 |   320 | `		return SXRET_OK;` |
|        - |   321 | `	}` |
|        - |   322 | `	/* Create a new user function */` |
|  8931513 |   323 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  8931513 |   324 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   325 | `		return rc;` |
|        - |   326 | `	}` |
|        - |   327 | `	/* Install the function in the corresponding hashtable */` |
|  8931513 |   328 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  8931513 |   329 | `	pVm->nCallableGen++; /* a name that was not callable may be now (OP_CALL_INIT) */` |
|  8931513 |   330 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   331 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   332 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   333 | `		return rc;` |
|        - |   334 | `	}` |
|        - |   335 | `	/* User function successfully installed */` |
|  8931513 |   336 | `	return SXRET_OK;` |
|  4447673 |   337 | `}` |
|        - |   338 | `/*` |
|        - |   339 | ` * Initialize a VM function.` |
|        - |   340 | ` */` |
| 11357687 |   341 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   342 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   343 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   344 | `	const char *zName,  /* Function name */` |
|        - |   345 | `	sxu32 nByte,        /* zName length */` |
|        - |   346 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   347 | `	void *pUserData     /* Function private data */` |
|        - |   348 | `	)` |
|        5 |   349 | `{` |
|        - |   350 | `	/* Zero the structure */` |
| 11357692 |   351 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   352 | `	/* Initialize structure fields */` |
|        - |   353 | `	/* Arguments container */` |
| 11357692 |   354 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   355 | `	/* Static variable container */` |
| 11357692 |   356 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   357 | `	/* Bytecode container */` |
| 11357692 |   358 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   359 | `    /* Preallocate some instruction slots */` |
| 11357692 |   360 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   361 | `	/* Closure environment */` |
| 11357692 |   362 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   363 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
| 11357692 |   364 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |   365 | `	/* Declared #[...] attributes */` |
| 11357692 |   366 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
| 11357692 |   367 | `	pFunc->iFlags = iFlags;` |
| 11357692 |   368 | `	pFunc->pUserData = pUserData;` |
|        - |   369 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   370 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
| 11357692 |   371 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
| 11357692 |   372 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   373 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
| 11332755 |   374 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|  5658515 |   375 | `	}else{` |
|        - |   376 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|        - |   377 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|        - |   378 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|    24942 |   379 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    24942 |   380 | `		if( pFile ){` |
|    24942 |   381 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|    12347 |   382 | `		}` |
|        - |   383 | `	}` |
| 11357692 |   384 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
| 11357692 |   385 | `	return SXRET_OK;` |
|        5 |   386 | `}` |
|        - |   387 | `/*` |
|        - |   388 | ` * Look a name up in the compiled-function table AS A SCRIPT SPELLS IT.` |
|        - |   389 | ` *` |
|        - |   390 | ` * hFunction is the ENGINE's table, not the script's. Besides the functions a program` |
|        - |   391 | ` * declared it holds every mounted class METHOD -- VmMountUserClassMethods installs each` |
|        - |   392 | `` * one under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c mints -- and`` |
|        - |   393 | `` * every compiled CLOSURE, under `[closure_N]`. Neither is a php function name (no php`` |
|        - |   394 | `` * label may hold a `[`, an `@` or a `]`), and php has no table in which a script can find`` |
|        - |   395 | ` * one.` |
|        - |   396 | ` *` |
|        - |   397 | ` * A plain SyHashGet therefore answered a name that does not exist to every surface that` |
|        - |   398 | ` * asks whether a function does: function_exists(), is_callable() and the whole callback` |
|        - |   399 | `` * screen behind it, ReflectionFunction, `new Fiber(name)` -- and the dispatch itself.`` |
|        - |   400 | ` * Reaching a METHOD that way really did run it: the body assumes the receiver frame the` |
|        - |   401 | `` * plain-function path never builds, so `$n = '[__Foo@bar_...]'; $n();` popped past the`` |
|        - |   402 | ` * bottom of the operand stack (SIGSEGV in the release build, an ASan heap-buffer-overflow` |
|        - |   403 | ` * READ in VmByteCodeExecBody).` |
|        - |   404 | ` *` |
|        - |   405 | ` * bEngineName is for the engine's OWN dispatch of those same entries, which is by name` |
|        - |   406 | `` * too: OP_MEMBER pushes a resolved method's `sVmName` onto the callee slot, the closure`` |
|        - |   407 | `` * machinery unwraps a Closure to its `[closure_N]`, and the two synthetic call builders`` |
|        - |   408 | ` * do both without an OP_MEMBER ahead of them. Each of those marks its own call site` |
|        - |   409 | ` * (MEMOBJ_AUX_MEMBERCALL / MEMOBJ_AUX_ENGINEFN / the OP_CALL local); nothing a program` |
|        - |   410 | ` * wrote ever passes 1.` |
|        - |   411 | ` */` |
|  4005931 |   412 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(` |
|        - |   413 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   414 | `	const void *pName,  /* Function name */` |
|        - |   415 | `	sxu32 nByte,        /* Name length */` |
|        - |   416 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|        - |   417 | `	)` |
|        5 |   418 | `{` |
|  4005936 |   419 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,pName,nByte);` |
|  4005936 |   420 | `	if( pEntry && !bEngineName ){` |
|    57949 |   421 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    57949 |   422 | `		if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|       25 |   423 | `			return 0;` |
|        - |   424 | `		}` |
|    28671 |   425 | `	}` |
|  4005912 |   426 | `	return pEntry;` |
|  2001676 |   427 | `}` |
|        - |   428 | `/*` |
|        - |   429 | ` * Look a name up in the HOST function table as a script spells it -- the twin of` |
|        - |   430 | ` * PH7_VmGetUserFunction above, and for the same reason.` |
|        - |   431 | ` *` |
|        - |   432 | `` * Nine of the engine's host functions are php LANGUAGE CONSTRUCTS: `empty`, `isset`,`` |
|        - |   433 | `` * `unset`, `eval`, `print`, `include`, `include_once`, `require` and `require_once`.`` |
|        - |   434 | ` * php has none of them in its function table -- they are grammar, and the compiler emits` |
|        - |   435 | `` * an opcode -- so `function_exists('empty')` is false there, `is_callable('isset')` is`` |
|        - |   436 | `` * false, `get_defined_functions()` lists neither, and `$f = 'include'; $f($p);` is`` |
|        - |   437 | `` * `Call to undefined function include()`. Here the construct's codegen dispatches each`` |
|        - |   438 | ` * one as an ordinary call to a host function of the same name, so a plain SyHashGet` |
|        - |   439 | ` * answered every one of those doors YES: phpstan's bundled better-reflection enumerates` |
|        - |   440 | `` * the internal function list, wrote `function empty() {}` into a stub, and its own parser`` |
|        - |   441 | ` * refused the file.` |
|        - |   442 | ` *` |
|        - |   443 | ` * bEngineName is the construct codegen's own dispatch, marked at the call SITE with` |
|        - |   444 | ` * PH7_CALL_CONSTRUCT. Nothing a program wrote ever passes 1: all nine names are lexer` |
|        - |   445 | ` * keywords, so the construct compiler is the only thing that can emit an OP_CALL naming` |
|        - |   446 | `` * one. (`exit`, `die` and `clone` are NOT here -- php 8.5 really does have those three as`` |
|        - |   447 | ` * functions.)` |
|        - |   448 | ` */` |
|  2065846 |   449 | `PH7_PRIVATE SyHashEntry * PH7_VmGetHostFunction(` |
|        - |   450 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   451 | `	const void *pName,  /* Function name */` |
|        - |   452 | `	sxu32 nByte,        /* Name length */` |
|        - |   453 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|        - |   454 | `	)` |
|        5 |   455 | `{` |
|  2065851 |   456 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,pName,nByte);` |
|  2065851 |   457 | `	if( pEntry && !bEngineName ){` |
|  1798017 |   458 | `		ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|  1798017 |   459 | `		if( pFunc == 0 \|\| pFunc->bConstruct ){` |
|       71 |   460 | `			return 0;` |
|        - |   461 | `		}` |
|   898245 |   462 | `	}` |
|  2065781 |   463 | `	return pEntry;` |
|  1032202 |   464 | `}` |
|        - |   465 | `/*` |
|        - |   466 | ` * The one copy of a callee name that every call site spelling it shares, made on first` |
|        - |   467 | ` * demand. 0 when it cannot be made, which just costs the caller its cache.` |
|        - |   468 | ` */` |
|    19940 |   469 | `static const char * VmCallNameIntern(ph7_vm *pVm,const SyString *pName)` |
|        5 |   470 | `{` |
|    19945 |   471 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hCallName,pName->zString,pName->nByte);` |
|        - |   472 | `	char *zCopy;` |
|    19945 |   473 | `	if( pEntry ){` |
|    11659 |   474 | `		return (const char *)pEntry->pKey;` |
|        - |   475 | `	}` |
|     8291 |   476 | `	zCopy = (char *)SyMemBackendDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|     8291 |   477 | `	if( zCopy == 0 ){` |
|      ! 0 |   478 | `		return 0;` |
|        - |   479 | `	}` |
|     8291 |   480 | `	if( SyHashInsert(&pVm->hCallName,zCopy,pName->nByte,zCopy) != SXRET_OK ){` |
|      ! 0 |   481 | `		SyMemBackendFree(&pVm->sAllocator,zCopy);` |
|      ! 0 |   482 | `		return 0;` |
|        - |   483 | `	}` |
|     8291 |   484 | `	return zCopy;` |
|     9903 |   485 | `}` |
|        - |   486 | `/*` |
|        - |   487 | ` * Are these two names the same bytes? Equality only -- the guard below never orders.` |
|        - |   488 | ` *` |
|        - |   489 | ` * SyMemcmp is a byte loop (SX_MACRO_FAST_CMP, four bytes unrolled with a branch each)` |
|        - |   490 | ` * behind a call, and THIS ONE SITE walked 1,737,378,747 bytes of callee name on the` |
|        - |   491 | ` * ecosystem gate's phpcs step: 42,994,248 guards averaging forty bytes, because a` |
|        - |   492 | ` * namespaced function name is long. It was the second-largest SyMemcmp caller in the` |
|        - |   493 | ` * engine, above the one inside SyHashGetHashed. Eight bytes at a time turns forty` |
|        - |   494 | ` * comparisons into five and drops the call.` |
|        - |   495 | ` *` |
|        - |   496 | ` * The load is through memcpy rather than a cast: an unaligned sxu64 read through a` |
|        - |   497 | ` * char pointer is what UBSan exists to catch, and every compiler in the matrix folds a` |
|        - |   498 | ` * constant-size memcpy into the one load anyway.` |
|        - |   499 | ` *` |
|        - |   500 | ` * This is NOT a case for widening SyMemcmp itself. It was measured, and widening it read` |
|        - |   501 | ` * neutral, because most of its callers compare short property names where a word loop` |
|        - |   502 | ` * never gets going -- the same reason glibc's memcmp measured 1.3% SLOWER there.` |
|        - |   503 | ` */` |
|  4468132 |   504 | `static int VmCallNameEq(const char *zA,const char *zB,sxu32 nByte)` |
|        5 |   505 | `{` |
|        - |   506 | `	/* Declared and seeded out here for MSVC: /WX turns C4701 ("potentially` |
|        - |   507 | `	 * uninitialized local variable used") into an error, and cl cannot see that the` |
|        - |   508 | `	 * memmove below is what writes them. Both compilers drop the two stores. */` |
|  4468137 |   509 | `	sxu64 a = 0,b = 0;` |
|  7410219 |   510 | `	while( nByte >= sizeof(sxu64) ){` |
|  2942117 |   511 | `		SX_MACRO_FAST_MEMCPY(zA,&a,sizeof(a));` |
|  2942117 |   512 | `		SX_MACRO_FAST_MEMCPY(zB,&b,sizeof(b));` |
|  2942117 |   513 | `		if( a != b ){` |
|       31 |   514 | `			return 0;` |
|        - |   515 | `		}` |
|  2942087 |   516 | `		zA += sizeof(sxu64);` |
|  2942087 |   517 | `		zB += sizeof(sxu64);` |
|  2942087 |   518 | `		nByte -= (sxu32)sizeof(sxu64);` |
|        5 |   519 | `	}` |
| 23275176 |   520 | `	while( nByte-- > 0 ){` |
| 18807780 |   521 | `		if( *zA++ != *zB++ ){` |
|      711 |   522 | `			return 0;` |
|        - |   523 | `		}` |
|        5 |   524 | `	}` |
|  4467401 |   525 | `	return 1;` |
|  2236184 |   526 | `}` |
|        - |   527 | `/*` |
|        - |   528 | ` * The VmCallSite record a PH7_OP_CALL site owns. bClaim says which of the two doors is` |
|        - |   529 | ` * asking: the RECORDING one may create the record, the ASKING one only reads it.` |
|        - |   530 | ` *` |
|        - |   531 | ` * Answers 0 whenever the site has to resolve the long way -- it has no record yet, it` |
|        - |   532 | ` * has been marked dead, it is asking about a name it did not ask about before (which is` |
|        - |   533 | ` * what marks it dead), or the bookkeeping could not be allocated.` |
|        - |   534 | ` */` |
| 11040904 |   535 | `static VmCallSite * VmCallSiteFor(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,` |
|        - |   536 | `	int bEngineName,int bClaim)` |
|        5 |   537 | `{` |
|        - |   538 | `	VmCallSite *pSite;` |
| 11040909 |   539 | `	if( pName->nByte < 1 \|\| pName->zString == 0 ){` |
|      ! 0 |   540 | `		return 0;` |
|        - |   541 | `	}` |
| 11040909 |   542 | `	if( pInstr->nSite == 0 ){` |
|        - |   543 | `		/* No record yet. Claim one only on this site's SECOND execution, because a` |
|        - |   544 | `		 * record costs more memory than a site that runs once can ever save -- a` |
|        - |   545 | `		 * bootstrap, a one-shot branch, a sniff that matches nothing. pInstr->nAux is` |
|        - |   546 | `		 * free on a PH7_OP_CALL (OP_LOAD and OP_CALL_INIT are the only opcodes that` |
|        - |   547 | `		 * use it), so the site counts its own first two executions there.` |
|        - |   548 | `		 *` |
|        - |   549 | `		 * Only the ASKING door counts, and only the RECORDING door claims: both run on` |
|        - |   550 | `		 * one dispatch, so a shared counter would reach two before the first call has` |
|        - |   551 | `		 * finished and the site would pay on its first execution after all. */` |
|        - |   552 | `		VmCallSite sNew;` |
|        - |   553 | `		const char *zCopy;` |
|  6544799 |   554 | `		if( !bClaim ){` |
|  3392471 |   555 | `			if( pInstr->nAux < 2 ){` |
|  3152473 |   556 | `				pInstr->nAux++;` |
|  1575541 |   557 | `			}` |
|  3392471 |   558 | `			return 0;` |
|        - |   559 | `		}` |
|  3152333 |   560 | `		if( pInstr->nAux < 2 ){` |
|  3132393 |   561 | `			return 0;` |
|        - |   562 | `		}` |
|    19945 |   563 | `		zCopy = VmCallNameIntern(&(*pVm),pName);` |
|    19945 |   564 | `		if( zCopy == 0 ){` |
|      ! 0 |   565 | `			return 0;` |
|        - |   566 | `		}` |
|    19945 |   567 | `		sNew.zName = zCopy;` |
|    19945 |   568 | `		sNew.nName = pName->nByte;` |
|    19945 |   569 | `		sNew.pEntry = 0;` |
|    19945 |   570 | `		sNew.nGen = 0;` |
|    19945 |   571 | `		sNew.nNextFree = 0;` |
|    19945 |   572 | `		sNew.bHost = 0;` |
|    19945 |   573 | `		sNew.bEngine = (sxu8)(bEngineName ? 1 : 0);` |
|    19945 |   574 | `		sNew.bDead = 0;` |
|    19945 |   575 | `		if( pVm->nFreeCallSite ){` |
|     5255 |   576 | `			pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pVm->nFreeCallSite - 1);` |
|     5255 |   577 | `			if( pSite ){` |
|     5255 |   578 | `				pInstr->nSite = pVm->nFreeCallSite;` |
|     5255 |   579 | `				pVm->nFreeCallSite = pSite->nNextFree;` |
|     5255 |   580 | `				*pSite = sNew;` |
|     5255 |   581 | `				return pSite;` |
|        - |   582 | `			}` |
|      ! 0 |   583 | `			pVm->nFreeCallSite = 0; /* corrupt link: give up on reuse rather than on the cache */` |
|      ! 0 |   584 | `		}` |
|    14691 |   585 | `		if( SySetPut(&pVm->aCallSite,(const void *)&sNew) != SXRET_OK ){` |
|      ! 0 |   586 | `			return 0; /* the interned name stays; another site may still want it */` |
|        - |   587 | `		}` |
|    14691 |   588 | `		pInstr->nSite = SySetUsed(&pVm->aCallSite); /* index + 1 */` |
|    14691 |   589 | `		return (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|        - |   590 | `	}` |
|  4496115 |   591 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|  4496115 |   592 | `	if( pSite == 0 \|\| pSite->bDead ){` |
|    27751 |   593 | `		return 0;` |
|        - |   594 | `	}` |
|  4468364 |   595 | `	if( pSite->nName != pName->nByte` |
|  4468248 |   596 | `	 \|\| pSite->bEngine != (sxu8)(bEngineName ? 1 : 0)` |
|  4468137 |   597 | `	 \|\| !VmCallNameEq(pSite->zName,pName->zString,pSite->nName) ){` |
|        - |   598 | `		/* A second name at one site: the callee is a variable (or a closure key), and` |
|        - |   599 | `		 * re-interning it on every call would cost more than the lookup it saves. */` |
|      973 |   600 | `		pSite->zName = 0;` |
|      973 |   601 | `		pSite->nName = 0;` |
|      973 |   602 | `		pSite->pEntry = 0;` |
|      973 |   603 | `		pSite->nGen = 0;` |
|      973 |   604 | `		pSite->bDead = 1;` |
|      973 |   605 | `		return 0;` |
|        - |   606 | `	}` |
|  4467401 |   607 | `	return pSite;` |
|  5520913 |   608 | `}` |
|        - |   609 | `/*` |
|        - |   610 | ` * The function-table entry this call site resolved its callee to last time, or 0 if it` |
|        - |   611 | ` * has to be resolved again. *pbHost says which table the answer is in.` |
|        - |   612 | ` */` |
|  7810233 |   613 | `PH7_PRIVATE SyHashEntry * PH7_VmCallSiteAnswer(` |
|        - |   614 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |   615 | `	VmInstr *pInstr,      /* The PH7_OP_CALL being dispatched */` |
|        - |   616 | `	const SyString *pName,/* Callee name, as this dispatch spelled it */` |
|        - |   617 | `	int bEngineName,      /* TRUE when the engine, not the script, spelled it */` |
|        - |   618 | `	int *pbHost           /* OUT: 1 when the answer lives in hHostFunction */` |
|        - |   619 | `	)` |
|        5 |   620 | `{` |
|  7810238 |   621 | `	VmCallSite *pSite = VmCallSiteFor(&(*pVm),pInstr,pName,bEngineName,0);` |
|  7810238 |   622 | `	if( pSite == 0 \|\| pSite->nGen != pVm->nCallableGen ){` |
|  3470814 |   623 | `		return 0;` |
|        - |   624 | `	}` |
|  4339429 |   625 | `	*pbHost = pSite->bHost;` |
|  4339429 |   626 | `	return pSite->pEntry;` |
|  3906408 |   627 | `}` |
|        - |   628 | `/*` |
|        - |   629 | ` * Remember what this call site's callee name resolved to, so the next execution can` |
|        - |   630 | ` * skip the lookups. Silently does nothing for a site that has no record to write to.` |
|        - |   631 | ` */` |
|  5037251 |   632 | `PH7_PRIVATE void PH7_VmCallSiteRecord(` |
|        - |   633 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |   634 | `	VmInstr *pInstr,      /* The PH7_OP_CALL being dispatched */` |
|        - |   635 | `	const SyString *pName,/* Callee name, as this dispatch spelled it */` |
|        - |   636 | `	int bEngineName,      /* TRUE when the engine, not the script, spelled it */` |
|        - |   637 | `	int bHost,            /* TRUE when pEntry lives in hHostFunction */` |
|        - |   638 | `	SyHashEntry *pEntry   /* The entry the name resolved to */` |
|        - |   639 | `	)` |
|        5 |   640 | `{` |
|        - |   641 | `	VmCallSite *pSite;` |
|  5037256 |   642 | `	if( pEntry == 0 ){` |
|  1806585 |   643 | `		return;` |
|        - |   644 | `	}` |
|  3230676 |   645 | `	pSite = VmCallSiteFor(&(*pVm),pInstr,pName,bEngineName,1);` |
|  3230676 |   646 | `	if( pSite == 0 ){` |
|  3146750 |   647 | `		return;` |
|        - |   648 | `	}` |
|    83931 |   649 | `	pSite->pEntry = pEntry;` |
|    83931 |   650 | `	pSite->bHost = (sxu8)(bHost ? 1 : 0);` |
|    83931 |   651 | `	pSite->nGen = pVm->nCallableGen;` |
|  2517396 |   652 | `}` |
|        - |   653 | `/*` |
|        - |   654 | ` * The hConstant entry a PH7_OP_LOADC site resolved its constant to last time, or 0 when` |
|        - |   655 | ` * it has to be resolved the long way.` |
|        - |   656 | ` *` |
|        - |   657 | ` * A LOADC site looks up as many as TWO names on every execution -- the compile-time` |
|        - |   658 | `` * candidate in p3 (a `use const` import's FQN, or `current-namespace\NAME`) and then the`` |
|        - |   659 | ` * bare literal -- and on the ecosystem gate's phpcs step those two were 104M of the` |
|        - |   660 | ` * engine's 724M hash lookups and 3.0 GB of its 7.5 GB of hashed key bytes. The candidate` |
|        - |   661 | ` * alone hashed 2.3 GB to miss 91% of the time, because a namespace-qualified name is` |
|        - |   662 | ` * long and usually is not a constant.` |
|        - |   663 | ` *` |
|        - |   664 | ` * Which of the two wins, and what it resolves to, can only change when a name enters or` |
|        - |   665 | ` * leaves hConstant. So the site stamps the pVm->nConstGen it resolved at, and a site` |
|        - |   666 | ` * whose stamp is current answers without hashing anything.` |
|        - |   667 | ` *` |
|        - |   668 | ` * The record is claimed on the site's SECOND execution, for VmCallSiteFor's reason: a` |
|        - |   669 | ` * bootstrap or a one-shot branch would pay for bookkeeping it never reads. pInstr->nAux` |
|        - |   670 | ` * is free on a PH7_OP_LOADC, so the site counts its first two executions there.` |
|        - |   671 | ` */` |
|   162820 |   672 | `PH7_PRIVATE SyHashEntry * PH7_VmConstSiteAnswer(ph7_vm *pVm,VmInstr *pInstr)` |
|        5 |   673 | `{` |
|        - |   674 | `	VmCallSite *pSite;` |
|   162825 |   675 | `	if( pInstr->nSite == 0 ){` |
|    10310 |   676 | `		if( pInstr->nAux < 2 ){` |
|    10268 |   677 | `			pInstr->nAux++;` |
|     5043 |   678 | `		}` |
|    10310 |   679 | `		return 0;` |
|        - |   680 | `	}` |
|   152520 |   681 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|   152520 |   682 | `	if( pSite == 0 \|\| pSite->nGen != pVm->nConstGen ){` |
|      123 |   683 | `		return 0;` |
|        - |   684 | `	}` |
|   152398 |   685 | `	return pSite->pEntry;` |
|    81277 |   686 | `}` |
|        - |   687 | `/*` |
|        - |   688 | ` * Remember what this PH7_OP_LOADC site's constant name resolved to. Silently does` |
|        - |   689 | ` * nothing when the site has not earned a record yet or one cannot be allocated -- the` |
|        - |   690 | ` * lookup path above is always correct on its own.` |
|        - |   691 | ` */` |
|    10427 |   692 | `PH7_PRIVATE void PH7_VmConstSiteRecord(ph7_vm *pVm,VmInstr *pInstr,SyHashEntry *pEntry)` |
|        5 |   693 | `{` |
|        - |   694 | `	VmCallSite *pSite;` |
|    10432 |   695 | `	if( pEntry == 0 ){` |
|      179 |   696 | `		return;` |
|        - |   697 | `	}` |
|    10258 |   698 | `	if( pInstr->nSite == 0 ){` |
|        - |   699 | `		VmCallSite sNew;` |
|    10136 |   700 | `		if( pInstr->nAux < 2 ){` |
|     9358 |   701 | `			return;` |
|        - |   702 | `		}` |
|      783 |   703 | `		sNew.zName = 0;   /* a LOADC site's name is its instruction; nothing to guard */` |
|      783 |   704 | `		sNew.nName = 0;` |
|      783 |   705 | `		sNew.pEntry = 0;` |
|      783 |   706 | `		sNew.nGen = 0;` |
|      783 |   707 | `		sNew.nNextFree = 0;` |
|      783 |   708 | `		sNew.bHost = 0;` |
|      783 |   709 | `		sNew.bEngine = 0;` |
|      783 |   710 | `		sNew.bDead = 0;` |
|      783 |   711 | `		if( pVm->nFreeCallSite ){` |
|       99 |   712 | `			pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pVm->nFreeCallSite - 1);` |
|       99 |   713 | `			if( pSite ){` |
|       99 |   714 | `				pInstr->nSite = pVm->nFreeCallSite;` |
|       99 |   715 | `				pVm->nFreeCallSite = pSite->nNextFree;` |
|       99 |   716 | `				*pSite = sNew;` |
|       50 |   717 | `			}else{` |
|      ! 0 |   718 | `				pVm->nFreeCallSite = 0; /* corrupt link: give up on reuse, not on the cache */` |
|        - |   719 | `			}` |
|       49 |   720 | `		}` |
|      783 |   721 | `		if( pInstr->nSite == 0 ){` |
|      685 |   722 | `			if( SySetPut(&pVm->aCallSite,(const void *)&sNew) != SXRET_OK ){` |
|      ! 0 |   723 | `				return;` |
|        - |   724 | `			}` |
|      685 |   725 | `			pInstr->nSite = SySetUsed(&pVm->aCallSite); /* index + 1 */` |
|      333 |   726 | `		}` |
|      382 |   727 | `	}` |
|      905 |   728 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|      905 |   729 | `	if( pSite == 0 ){` |
|      ! 0 |   730 | `		return;` |
|        - |   731 | `	}` |
|      905 |   732 | `	pSite->pEntry = pEntry;` |
|      905 |   733 | `	pSite->nGen = pVm->nConstGen;` |
|     5130 |   734 | `}` |
|        - |   735 | `/*` |
|        - |   736 | ` * Give back every site record the instructions in a bytecode container claimed -- a` |
|        - |   737 | ` * PH7_OP_CALL's callee answer and a PH7_OP_LOADC's constant answer alike. Called just` |
|        - |   738 | ` * before the container itself is released -- which happens exactly once, for the chunk an` |
|        - |   739 | ` * eval() or an include compiles -- so that a program evaluating chunks in a loop reuses` |
|        - |   740 | ` * the records instead of accumulating one per chunk for ever.` |
|        - |   741 | ` */` |
|    29565 |   742 | `PH7_PRIVATE void PH7_VmCallSiteReleaseChunk(ph7_vm *pVm,SySet *pByteCode)` |
|        5 |   743 | `{` |
|    29570 |   744 | `	VmInstr *aInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|    29570 |   745 | `	sxu32 n = SySetUsed(pByteCode);` |
|        - |   746 | `	sxu32 i;` |
|    29570 |   747 | `	if( aInstr == 0 ){` |
|      ! 0 |   748 | `		return;` |
|        - |   749 | `	}` |
|   759701 |   750 | `	for( i = 0 ; i < n ; ++i ){` |
|        - |   751 | `		VmCallSite *pSite;` |
|   730136 |   752 | `		sxu32 nSite = aInstr[i].nSite;` |
|   730131 |   753 | `		if( nSite == 0` |
|   367744 |   754 | `		 \|\| (aInstr[i].iOp != PH7_OP_CALL && aInstr[i].iOp != PH7_OP_LOADC) ){` |
|   724778 |   755 | `			continue;` |
|        - |   756 | `		}` |
|     5359 |   757 | `		aInstr[i].nSite = 0;` |
|     5359 |   758 | `		pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,nSite - 1);` |
|     5359 |   759 | `		if( pSite == 0 ){` |
|      ! 0 |   760 | `			continue;` |
|        - |   761 | `		}` |
|     5359 |   762 | `		pSite->zName = 0; /* the name itself is hCallName's, and other sites may share it */` |
|     5359 |   763 | `		pSite->nName = 0;` |
|     5359 |   764 | `		pSite->pEntry = 0;` |
|     5359 |   765 | `		pSite->nGen = 0;` |
|     5359 |   766 | `		pSite->bDead = 0;` |
|     5359 |   767 | `		pSite->nNextFree = pVm->nFreeCallSite;` |
|     5359 |   768 | `		pVm->nFreeCallSite = nSite;` |
|     2680 |   769 | `	}` |
|    14782 |   770 | `}` |
|        - |   771 | `/*` |
|        - |   772 | ` * Namespace-aware function lookup.` |
|        - |   773 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   774 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   775 | ` */` |
|        - |   776 | `/*` |
|        - |   777 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   778 | ` */` |
| 43446019 |   779 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   780 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   781 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   782 | `	SyString *pName     /* Function name */` |
|        - |   783 | `	)` |
|        5 |   784 | `{` |
|        - |   785 | `	SyHashEntry *pEntry;` |
|        - |   786 | `	sxi32 rc;` |
| 43446024 |   787 | `	if( pName == 0 ){` |
|        - |   788 | `		/* Use the built-in name */` |
|   198090 |   789 | `		pName = &pFunc->sName;` |
|    98794 |   790 | `	}` |
|        - |   791 | `	/* Check for duplicates (functions with the same name) first */` |
| 43446024 |   792 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
| 43446024 |   793 | `	if( pEntry ){` |
| 32568000 |   794 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
| 32568000 |   795 | `		if( pLink != pFunc ){` |
|        - |   796 | `			/* Link */` |
|       72 |   797 | `			pFunc->pNextName = pLink;` |
|       72 |   798 | `			pEntry->pUserData = pFunc;` |
|       34 |   799 | `		}` |
| 32568000 |   800 | `		return SXRET_OK;` |
|        - |   801 | `	}` |
|        - |   802 | `	/* First time seen */` |
| 10878029 |   803 | `	pFunc->pNextName = 0;` |
| 10878029 |   804 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
| 10878029 |   805 | `	if( (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) == 0 ){` |
|        - |   806 | `		/* A name a SCRIPT can now call. This table also holds every method and every` |
|        - |   807 | `		 * per-instantiation closure copy -- names PH7_VmGetUserFunction refuses to a` |
|        - |   808 | `		 * script -- and counting those would retire OP_CALL_INIT's screened-at stamps` |
|        - |   809 | `		 * on every closure EXPRESSION a program evaluates, which is most of them. */` |
|   178929 |   810 | `		pVm->nCallableGen++;` |
|    89335 |   811 | `	}` |
| 10878029 |   812 | `	return rc;` |
| 21690228 |   813 | `}` |
|        - |   814 | `/*` |
|        - |   815 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   816 | ` */` |
|  1741353 |   817 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   818 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   819 | `	ph7_class *pClass /* Target Class */` |
|        - |   820 | `	)` |
|        5 |   821 | `{` |
|  1741358 |   822 | `	SyString *pName = &pClass->sName;` |
|        - |   823 | `	SyHashEntry *pEntry;` |
|        - |   824 | `	sxi32 rc;` |
|        - |   825 | `	/* Check for duplicates */` |
|  1741358 |   826 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|  1741358 |   827 | `	if( pEntry ){` |
|        6 |   828 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   829 | `		/* Link entry with the same name */` |
|        6 |   830 | `		pClass->pNextName = pLink;` |
|        6 |   831 | `		pEntry->pUserData = pClass;` |
|        6 |   832 | `		return SXRET_OK;` |
|        - |   833 | `	}` |
|  1741352 |   834 | `	pClass->pNextName = 0;` |
|        - |   835 | `	/* Perform a simple hashtable insertion */` |
|  1741352 |   836 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|  1741352 |   837 | `	return rc;` |
|   869477 |   838 | `}` |
|        - |   839 | `/*` |
|        - |   840 | ` * Instruction builder interface.` |
|        - |   841 | ` */` |
| 17666366 |   842 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   843 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   844 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   845 | `	sxi32 iP1,    /* First operand */` |
|        - |   846 | `	sxu32 iP2,    /* Second operand */` |
|        - |   847 | `	void *p3,     /* Third operand */` |
|        - |   848 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   849 | `	)` |
|        5 |   850 | `{` |
|        - |   851 | `	VmInstr sInstr;` |
| 17666371 |   852 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - |   853 | `	sxi32 rc;` |
|        - |   854 | `	/* Fill the VM instruction */` |
| 17666371 |   855 | `	sInstr.iOp = (sxu8)iOp;` |
| 17666371 |   856 | `	sInstr.iP1 = iP1;` |
| 17666371 |   857 | `	sInstr.iP2 = iP2;` |
| 17666371 |   858 | `	sInstr.p3  = p3;` |
|        - |   859 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|        - |   860 | `	 * compiled (that is how they read its text), so the current token IS this` |
|        - |   861 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|        - |   862 | `	 * between statements, hence the range check. */` |
| 17666371 |   863 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|        - |   864 | `	/* Nothing is discarded until the statement that owns this call says so` |
|        - |   865 | `	 * (GenStateMarkDiscardedCall, after the fact) — but the field must not be` |
|        - |   866 | `	 * this stack frame's leftovers in the meantime. */` |
| 17666371 |   867 | `	sInstr.bDiscard = 0;` |
|        - |   868 | `	/* ...and neither must the reference-source marker: the codegen stamps it on the` |
|        - |   869 | `	 * one OP_MEMBER it belongs to, AFTER this returns. */` |
| 17666371 |   870 | `	sInstr.bRefSrc = 0;` |
| 17666371 |   871 | `	sInstr.nAux = 0;` |
|        - |   872 | `	/* ...nor the call site's cache index: a stale one would point this site at` |
|        - |   873 | `	 * another site's remembered callee. */` |
| 17666371 |   874 | `	sInstr.nSite = 0;` |
| 17666371 |   875 | `	sInstr.nLine = 0;` |
| 17666371 |   876 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|  6357219 |   877 | `		sInstr.nLine = pGen->pIn->nLine;` |
| 14482256 |   878 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|        - |   879 | `		/* Past the end (statement tail): blame the last real token. */` |
| 11215335 |   880 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|  5598071 |   881 | `	}` |
| 17666371 |   882 | `	if( pIndex ){` |
|        - |   883 | `		/* Instruction index in the bytecode array */` |
|  1461476 |   884 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   729705 |   885 | `	}` |
|        - |   886 | `	/* Finally,record the instruction */` |
| 17666371 |   887 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
| 17666371 |   888 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   889 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   890 | `		/* Fall throw */` |
|      ! 0 |   891 | `	}` |
| 17666371 |   892 | `	return rc;` |
|        5 |   893 | `}` |
|        - |   894 | `/*` |
|        - |   895 | ` * Swap the current bytecode container with the given one.` |
|        - |   896 | ` */` |
|   512698 |   897 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |   898 | `{` |
|   512703 |   899 | `	if( pContainer == 0 ){` |
|        - |   900 | `		/* Point to the default container */` |
|      ! 0 |   901 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   902 | `	}else{` |
|        - |   903 | `		/* Change container */` |
|   512703 |   904 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   905 | `	}` |
|   512703 |   906 | `	return SXRET_OK;` |
|        5 |   907 | `}` |
|        - |   908 | `/*` |
|        - |   909 | ` * Return the current bytecode container.` |
|        - |   910 | ` */` |
|  1373155 |   911 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |   912 | `{` |
|  1373160 |   913 | `	return pVm->pByteContainer;` |
|        5 |   914 | `}` |
|        - |   915 | `/*` |
|        - |   916 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   917 | ` */` |
|  1414390 |   918 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |   919 | `{` |
|        - |   920 | `	VmInstr *pInstr;` |
|  1414395 |   921 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|  1414395 |   922 | `	return pInstr;` |
|        5 |   923 | `}` |
|        - |   924 | `/*` |
|        - |   925 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   926 | ` */` |
| 14264825 |   927 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |   928 | `{` |
| 14264830 |   929 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |   930 | `}` |
|        - |   931 | `/*` |
|        - |   932 | ` * Pop the last VM instruction.` |
|        - |   933 | ` */` |
|  1117780 |   934 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |   935 | `{` |
|  1117785 |   936 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |   937 | `}` |
|        - |   938 | `/*` |
|        - |   939 | ` * Peek the last VM instruction.` |
|        - |   940 | ` */` |
|  5823734 |   941 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |   942 | `{` |
|  5823739 |   943 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |   944 | `}` |
|   112516 |   945 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |   946 | `{` |
|        - |   947 | `	VmInstr *aInstr;` |
|        - |   948 | `	sxu32 n;` |
|   112521 |   949 | `	n = SySetUsed(pVm->pByteContainer);` |
|   112521 |   950 | `	if( n < 2 ){` |
|      ! 0 |   951 | `		return 0;` |
|        - |   952 | `	}` |
|   112521 |   953 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|   112521 |   954 | `	return &aInstr[n - 2];` |
|    56184 |   955 | `}` |
|        - |   956 | `/*` |
|        - |   957 | ` * Allocate a new virtual machine frame.` |
|        - |   958 | ` */` |
|  3768635 |   959 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|        - |   960 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   961 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   962 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   963 | `	)` |
|        5 |   964 | `{` |
|        - |   965 | `	VmFrame *pFrame;` |
|        - |   966 | `	/* Allocate a new vm frame */` |
|  3768640 |   967 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|  3768640 |   968 | `	if( pFrame == 0 ){` |
|      ! 0 |   969 | `		return 0;` |
|        - |   970 | `	}` |
|        - |   971 | `	/* Zero the structure */` |
|  3768640 |   972 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   973 | `	/* Initialize frame fields */` |
|  3768640 |   974 | `	pFrame->pUserData = pUserData;` |
|  3768640 |   975 | `	pFrame->pThis = pThis;` |
|  3768640 |   976 | `	pFrame->pVm = pVm;` |
|  3768640 |   977 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|  3768640 |   978 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|  3768640 |   979 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|  3768640 |   980 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|  3768640 |   981 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|        - |   982 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|        - |   983 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|  3768640 |   984 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|  3768640 |   985 | `	return pFrame;` |
|  1884385 |   986 | `}` |
|        - |   987 | `/* Forward declaration */` |
|        - |   988 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|        - |   989 | `/*` |
|        - |   990 | ` * The file the code RUNNING RIGHT NOW is written in -- what php would call the` |
|        - |   991 | ` * executing op array's filename, and what a trace frame records as its call site.` |
|        - |   992 | ` *` |
|        - |   993 | ` * Three answers, in order. An include/require/eval started from the current frame` |
|        - |   994 | ` * means that unit's own top-level code is what is running, so the include stack's` |
|        - |   995 | ` * top is the file (a frame is SHARED with the unit it includes: php gives the unit` |
|        - |   996 | ` * an op array of its own, this engine does not). Otherwise it is the defining file` |
|        - |   997 | ` * of the function whose frame this is -- the include stack is no help there, since` |
|        - |   998 | ` * a call chain spanning files leaves it pointing at the outermost unit. And for` |
|        - |   999 | ` * top-level code with no function at all, the include stack's top again.` |
|        - |  1000 | ` *` |
|        - |  1001 | `` * A `try` block pushes a frame of its own carrying no function, so the search for`` |
|        - |  1002 | ` * the running function looks past those.` |
|        - |  1003 | ` */` |
|  5319474 |  1004 | `PH7_PRIVATE SyString * PH7_VmExecutingUnitFile(ph7_vm *pVm)` |
|        5 |  1005 | `{` |
|  5319479 |  1006 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  1007 | `	sxu32 nInc;` |
| 12814134 |  1008 | `	while( pFrame && pFrame->pParent` |
| 10154235 |  1009 | `	    && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|  2740507 |  1010 | `		pFrame = pFrame->pParent;` |
|        5 |  1011 | `	}` |
|  5319479 |  1012 | `	nInc = SySetUsed(&pVm->aIncFrame);` |
|  5319479 |  1013 | `	if( nInc > 0 ){` |
|   280653 |  1014 | `		VmIncFrame *pInc = (VmIncFrame *)SySetAt(&pVm->aIncFrame,nInc - 1);` |
|   280653 |  1015 | `		if( pInc && pInc->pFrame == (void *)pFrame ){` |
|   164317 |  1016 | `			return (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  1017 | `		}` |
|    58168 |  1018 | `	}` |
|  5155167 |  1019 | `	if( pFrame && pFrame->pUserData ){` |
|  1446739 |  1020 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|  1446739 |  1021 | `		if( SyStringLength(&pFunc->sFile) > 0 ){` |
|  1445550 |  1022 | `			return &pFunc->sFile;` |
|        - |  1023 | `		}` |
|      594 |  1024 | `	}` |
|  3709622 |  1025 | `	return (SyString *)SySetPeek(&pVm->aFiles);` |
|  2659734 |  1026 | `}` |
|        - |  1027 | `/*` |
|        - |  1028 | ` * The name a diagnostic raised from inside this builtin belongs to, php's way:` |
|        - |  1029 | ` * the prelude builtin under whose body it is running when there is one, and` |
|        - |  1030 | `` * otherwise the builtin's own name. `zBuf` is scratch the caller owns -- a`` |
|        - |  1031 | ` * compiled function's name aliases the chunk it was parsed from and is not` |
|        - |  1032 | `` * NUL-terminated, and every one of these messages is built with `%s`.`` |
|        - |  1033 | ` *` |
|        - |  1034 | ` * See PH7_VmPreludeBuiltinFrame for why the outer name is the right one.` |
|        - |  1035 | ` */` |
|      500 |  1036 | `PH7_PRIVATE const char * PH7_CtxDiagFuncName(ph7_context *pCtx,char *zBuf,int nBuf)` |
|        5 |  1037 | `{` |
|      505 |  1038 | `	ph7_vm_func *pFunc = PH7_VmPreludeBuiltinFrame(pCtx->pVm,0,0);` |
|        - |  1039 | `	int nName;` |
|      505 |  1040 | `	if( pFunc == 0 ){` |
|      479 |  1041 | `		return ph7_function_name(pCtx);` |
|        - |  1042 | `	}` |
|       28 |  1043 | `	nName = (int)SyStringLength(&pFunc->sName);` |
|       28 |  1044 | `	if( nName < 1 \|\| nName >= nBuf ){` |
|      ! 0 |  1045 | `		return ph7_function_name(pCtx);` |
|        - |  1046 | `	}` |
|       28 |  1047 | `	SyMemcpy(SyStringData(&pFunc->sName),zBuf,(sxu32)nName);` |
|       28 |  1048 | `	zBuf[nName] = 0;` |
|       28 |  1049 | `	return zBuf;` |
|      254 |  1050 | `}` |
|        - |  1051 | `/*` |
|        - |  1052 | ` * The prelude builtin whose body is RUNNING, or 0 -- and the call site the` |
|        - |  1053 | ` * program wrote for it.` |
|        - |  1054 | ` *` |
|        - |  1055 | ` * ~24 builtins (scandir, glob, tempnam, tmpfile, hex2bin, checkdate, ...) are` |
|        - |  1056 | ` * written as embedded PHP in the builtin chunk, so each one runs in a VM frame` |
|        - |  1057 | ` * of its own. php has no such frame: every one of them is an INTERNAL function` |
|        - |  1058 | ` * there, whose C body is not PHP code and carries no line at all. So everything` |
|        - |  1059 | ` * a diagnostic asks about position has to be answered from the frame BELOW --` |
|        - |  1060 | ` * the file and line of the call the program wrote -- and the builtin has to` |
|        - |  1061 | ` * name ITSELF rather than whichever host builtin it reached for.` |
|        - |  1062 | ` *` |
|        - |  1063 | ` * Neither held. The whole chunk is one source line, so every diagnostic raised` |
|        - |  1064 | `` * anywhere under one of these reported `on line 1`, and so did the getFile()/`` |
|        - |  1065 | `` * getLine() of every exception they throw: `scandir('')` said line 1 where php`` |
|        - |  1066 | `` * says the caller's. And scandir()'s failed open said `opendir(/nope): Failed`` |
|        - |  1067 | `` * to open directory` where php says `scandir(...)`, because the message names`` |
|        - |  1068 | ` * the host builtin that raised it.` |
|        - |  1069 | ` *` |
|        - |  1070 | `` * The walk skips `try`/`catch` frames, which carry no function, and stops at`` |
|        - |  1071 | ` * the first frame that is not a prelude builtin -- a user callback reached from` |
|        - |  1072 | ` * one is ordinary user code and keeps its own position. It walks past a RUN of` |
|        - |  1073 | ` * them (a prelude builtin calling another) so the site is always userland's,` |
|        - |  1074 | ` * while the name reported is the innermost, which is the internal function php` |
|        - |  1075 | ` * would have been inside.` |
|        - |  1076 | ` *` |
|        - |  1077 | ` * Only plain functions qualify. A method declared in a builtin chunk carries` |
|        - |  1078 | ` * VM_FUNC_INTERNAL too, and so does a native class's method shell, but those` |
|        - |  1079 | ` * have a receiver and php gives several of them real PHP frames.` |
|        - |  1080 | ` */` |
|  1504698 |  1081 | `PH7_PRIVATE ph7_vm_func * PH7_VmPreludeBuiltinFrame(` |
|        - |  1082 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  1083 | `	SyString **ppFile,  /* OUT: file of the call the program wrote, or untouched */` |
|        - |  1084 | `	sxu32 *pnLine       /* OUT: its line, or untouched */` |
|        - |  1085 | `	)` |
|        5 |  1086 | `{` |
|  1504703 |  1087 | `	VmFrame *pFrame = pVm->pFrame;` |
|  1504703 |  1088 | `	ph7_vm_func *pInner = 0;` |
|   752491 |  1089 | `	for(;;){` |
|        - |  1090 | `		ph7_vm_func *pFunc;` |
|  3965785 |  1091 | `		while( pFrame && pFrame->pParent` |
|  3258291 |  1092 | `		    && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|   956018 |  1093 | `			pFrame = pFrame->pParent;` |
|        5 |  1094 | `		}` |
|  1504891 |  1095 | `		if( pFrame == 0 \|\| pFrame->pUserData == 0 ){` |
|   433090 |  1096 | `			break;` |
|        - |  1097 | `		}` |
|   638697 |  1098 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   638692 |  1099 | `		if( (pFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE))` |
|   319317 |  1100 | `			!= VM_FUNC_INTERNAL ){` |
|   638509 |  1101 | `			break;` |
|        - |  1102 | `		}` |
|      192 |  1103 | `		if( pInner == 0 ){` |
|      192 |  1104 | `			pInner = pFunc;` |
|       94 |  1105 | `		}` |
|      192 |  1106 | `		if( ppFile && SyStringLength(&pFrame->sCallFile) > 0 ){` |
|      166 |  1107 | `			*ppFile = &pFrame->sCallFile;` |
|       81 |  1108 | `		}` |
|      192 |  1109 | `		if( pnLine ){` |
|      166 |  1110 | `			*pnLine = pFrame->nCallLine;` |
|       81 |  1111 | `		}` |
|      192 |  1112 | `		pFrame = pFrame->pParent;` |
|        4 |  1113 | `	}` |
|  1504703 |  1114 | `	return pInner;` |
|        5 |  1115 | `}` |
|        - |  1116 | `/* Defined with the variable-slot machinery below; VmEnterFrame is what arms it. */` |
|        - |  1117 | `static void VmNumberLocals(VmInstr *aInstr,sxu32 nInstr,sxu16 *pnName);` |
|        - |  1118 | `static void VmFrameNumberBody(VmFrame *pFrame,ph7_vm_func *pFunc);` |
|        - |  1119 | `/*` |
|        - |  1120 | ` * Enter a VM frame.` |
|        - |  1121 | ` */` |
|  3767601 |  1122 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|        - |  1123 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  1124 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |  1125 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |  1126 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |  1127 | `	)` |
|        5 |  1128 | `{` |
|        - |  1129 | `	VmFrame *pFrame;` |
|        - |  1130 | `	/* Allocate a new frame */` |
|  3767606 |  1131 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|  3767606 |  1132 | `	if( pFrame == 0 ){` |
|      ! 0 |  1133 | `		return SXERR_MEM;` |
|        - |  1134 | `	}` |
|  3767606 |  1135 | `	pFrame->pSelfClass = pThis ? pThis->pClass : 0; /* the caller overwrites it for a static call */` |
|  3767606 |  1136 | `	if( pUserData ){` |
|        - |  1137 | `		/* A function frame runs a body whose variables can be numbered; do it once,` |
|        - |  1138 | `		 * here, so every push site inherits it (the OP_CALL trampoline, a generator` |
|        - |  1139 | `		 * or fiber resume, a closure, an engine-dispatched magic method). A frame` |
|        - |  1140 | `		 * with no function -- the global one, a try's -- leaves pCodeBase 0 and its` |
|        - |  1141 | `		 * variables take the hash path. */` |
|   831648 |  1142 | `		VmFrameNumberBody(pFrame,(ph7_vm_func *)pUserData);` |
|   416026 |  1143 | `	}` |
|        - |  1144 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|  3767606 |  1145 | `	pFrame->nCallLine = pVm->nCurLine;` |
|        - |  1146 | `	{` |
|        - |  1147 | `		/* ...and the file that line is in, which has to be read NOW: the include` |
|        - |  1148 | `		 * stack has moved on by the time a backtrace is taken. */` |
|  3767606 |  1149 | `		SyString *pCallFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|  3767606 |  1150 | `		if( pCallFile ){` |
|  3759681 |  1151 | `			pFrame->sCallFile = *pCallFile;` |
|  1879906 |  1152 | `		}` |
|        - |  1153 | `	}` |
|        - |  1154 | `	/* Link to the list of active VM frame */` |
|  3767606 |  1155 | `	pFrame->pParent = pVm->pFrame;` |
|  3767606 |  1156 | `	pVm->pFrame = pFrame;` |
|  3767606 |  1157 | `	if( ppFrame ){` |
|        - |  1158 | `		/* Write a pointer to the new VM frame */` |
|  3759665 |  1159 | `		*ppFrame = pFrame;` |
|  1879898 |  1160 | `	}` |
|  3767606 |  1161 | `	return SXRET_OK;` |
|  1883868 |  1162 | `}` |
|        - |  1163 | `/*` |
|        - |  1164 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |  1165 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |  1166 | ` * information.` |
|        - |  1167 | ` */` |
|     2616 |  1168 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        5 |  1169 | `{` |
|        - |  1170 | `	VmFrame *pTarget,*pGlobal;` |
|        - |  1171 | `	SyHashEntry *pEntry;` |
|        - |  1172 | `	sxi32 rc;` |
|     2621 |  1173 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|        - |  1174 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|        - |  1175 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|        - |  1176 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|        - |  1177 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|        - |  1178 | `	 * writes never reached the real global. */` |
|     2621 |  1179 | `	pGlobal = pTarget;` |
|     6181 |  1180 | `	while( pGlobal->pParent ){` |
|     3565 |  1181 | `		pGlobal = pGlobal->pParent;` |
|        5 |  1182 | `	}` |
|     2621 |  1183 | `	if( pGlobal == pTarget ){` |
|        - |  1184 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|      ! 0 |  1185 | `		return SXRET_OK;` |
|        - |  1186 | `	}` |
|        - |  1187 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|        - |  1188 | `	 * plain global that would shadow it. */` |
|     2621 |  1189 | `	pEntry = PH7_VmSuperGet(&(*pVm),pName->zString,pName->nByte);` |
|     2621 |  1190 | `	if( pEntry == 0 ){` |
|     2619 |  1191 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|     1305 |  1192 | `	}` |
|     2621 |  1193 | `	if( pEntry == 0 ){` |
|        - |  1194 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|        - |  1195 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|        - |  1196 | `		 * the assignment went to a local nobody could read. */` |
|       12 |  1197 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|       12 |  1198 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1199 | `			return rc;` |
|        - |  1200 | `		}` |
|       12 |  1201 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|       12 |  1202 | `		if( pEntry == 0 ){` |
|      ! 0 |  1203 | `			return SXERR_NOTFOUND;` |
|        - |  1204 | `		}` |
|        5 |  1205 | `	}` |
|        - |  1206 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|        - |  1207 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|     3927 |  1208 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|     2616 |  1209 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|     2621 |  1210 | `	return SXRET_OK;` |
|     1311 |  1211 | `}` |
|        - |  1212 | `/*` |
|        - |  1213 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|        - |  1214 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|        - |  1215 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|        - |  1216 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|        - |  1217 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|        - |  1218 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|        - |  1219 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|        - |  1220 | ` */` |
|  3760666 |  1221 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 |  1222 | `{` |
|  3760671 |  1223 | `	if( pVm->pResumeFrame == pFrame ){` |
|      ! 0 |  1224 | `		VmClearResumeTarget(&(*pVm));` |
|      ! 0 |  1225 | `	}` |
|  3760671 |  1226 | `}` |
|        - |  1227 | `/*` |
|        - |  1228 | ` * The four resume fields are ONE record: a frame, the landing pad inside it, the` |
|        - |  1229 | ` * bytecode array that pad indexes, and the operand-stack base to drain to. They` |
|        - |  1230 | ` * were written together but cleared, saved and restored INDIVIDUALLY (only the` |
|        - |  1231 | ` * frame), so a live frame could end up paired with a dead try's pad and depth —` |
|        - |  1232 | ` * which drains the operand stack to a foreign base and lands mid-statement, one` |
|        - |  1233 | ` * slot below the stack. These four functions are the only writers.` |
|        - |  1234 | ` */` |
|  4306910 |  1235 | `PH7_PRIVATE void VmSetResumeTarget(ph7_vm *pVm,VmFrame *pFrame,sxu32 iPc,void *pInstr,sxi32 iStackDepth)` |
|        5 |  1236 | `{` |
|  4306915 |  1237 | `	pVm->pResumeFrame = pFrame;` |
|  4306915 |  1238 | `	pVm->iResumePc = iPc;` |
|  4306915 |  1239 | `	pVm->pResumeInstr = pInstr;` |
|  4306915 |  1240 | `	pVm->iResumeStackDepth = iStackDepth;` |
|  4306915 |  1241 | `}` |
|  2940524 |  1242 | `PH7_PRIVATE void VmClearResumeTarget(ph7_vm *pVm)` |
|        5 |  1243 | `{` |
|  2940529 |  1244 | `	VmSetResumeTarget(&(*pVm),0,0,0,0);` |
|  2940529 |  1245 | `}` |
|  1366970 |  1246 | `PH7_PRIVATE void VmSaveResumeTarget(ph7_vm *pVm,VmResumeTarget *pSave)` |
|        5 |  1247 | `{` |
|  1366975 |  1248 | `	pSave->pFrame = pVm->pResumeFrame;` |
|  1366975 |  1249 | `	pSave->iPc = pVm->iResumePc;` |
|  1366975 |  1250 | `	pSave->pInstr = pVm->pResumeInstr;` |
|  1366975 |  1251 | `	pSave->iStackDepth = pVm->iResumeStackDepth;` |
|  1366975 |  1252 | `}` |
|     1261 |  1253 | `PH7_PRIVATE void VmRestoreResumeTarget(ph7_vm *pVm,const VmResumeTarget *pSave)` |
|        5 |  1254 | `{` |
|     1266 |  1255 | `	VmSetResumeTarget(&(*pVm),pSave->pFrame,pSave->iPc,pSave->pInstr,pSave->iStackDepth);` |
|     1266 |  1256 | `}` |
|        - |  1257 | `/*` |
|        - |  1258 | ` * Leave the top-most active frame.` |
|        - |  1259 | ` */` |
|  3759634 |  1260 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |  1261 | `{` |
|  3759639 |  1262 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|  3759639 |  1263 | `	if( pCurFrame ){` |
|        - |  1264 | `		/* Unlink from the list of active VM frame */` |
|  3759639 |  1265 | `		pVm->pFrame = pCurFrame->pParent;` |
|        - |  1266 | `		/* End the foreach walks this activation never finished, before its locals go:` |
|        - |  1267 | `		 * a step retains its subject, and an object walk holds a cursor registered on` |
|        - |  1268 | `		 * the instance. */` |
|  3759639 |  1269 | `		VmReleaseFrameForeachSteps(&(*pVm),pCurFrame);` |
|  3759639 |  1270 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |  1271 | `			VmSlot  *aSlot;` |
|        - |  1272 | `			sxu32 n;` |
|        - |  1273 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|        - |  1274 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|        - |  1275 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|        - |  1276 | `			 * is about to free). */` |
|   831638 |  1277 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|  2195119 |  1278 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|  1363486 |  1279 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|   684625 |  1280 | `			}` |
|        - |  1281 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|   831638 |  1282 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|  2190917 |  1283 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|  1359284 |  1284 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|        - |  1285 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|        - |  1286 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|        - |  1287 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|        - |  1288 | `					 * down the slot and the reference table took the holders with it, so` |
|        - |  1289 | `					 * the returned array came back one element SHORT. The last holder to` |
|        - |  1290 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|       30 |  1291 | `					continue;` |
|        - |  1292 | `				}` |
|        - |  1293 | `				/* Unset the local variable */` |
|  1359256 |  1294 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|   682512 |  1295 | `			}` |
|   416021 |  1296 | `		}` |
|        - |  1297 | `		/* Release internal containers */` |
|  3759639 |  1298 | `		SyHashRelease(&pCurFrame->hVar);` |
|  3759639 |  1299 | `		SySetRelease(&pCurFrame->sArg);` |
|  3759639 |  1300 | `		SySetRelease(&pCurFrame->sLocal);` |
|  3759639 |  1301 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |  1302 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|        - |  1303 | `		 * containers above — released for every frame, including transparent` |
|        - |  1304 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|  3759639 |  1305 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|        - |  1306 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|  3759639 |  1307 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|        - |  1308 | `		/* This activation no longer needs the function it was running. For a` |
|        - |  1309 | `		 * run-time closure that is one of the two holds on its per-instantiation` |
|        - |  1310 | `		 * copy -- the other is the Closure object -- and the copy goes when both` |
|        - |  1311 | `		 * are gone. pUserData is a ph7_vm_func for a user-function frame and 0 for` |
|        - |  1312 | `		 * every other kind (the global frame, an exception wrapper, a local exec). */` |
|  3759639 |  1313 | `		if( pCurFrame->pUserData ){` |
|   831638 |  1314 | `			PH7_VmClosureFuncUnref(&(*pVm),(ph7_vm_func *)pCurFrame->pUserData);` |
|   416021 |  1315 | `		}` |
|        - |  1316 | `		/* Release the whole structure */` |
|  3759639 |  1317 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|  1879885 |  1318 | `	}` |
|  3759639 |  1319 | `}` |
|        - |  1320 | `/*` |
|        - |  1321 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|        - |  1322 | ` * active frame's local-teardown set records it (walking the parent chain` |
|        - |  1323 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|        - |  1324 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|        - |  1325 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|        - |  1326 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|        - |  1327 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|        - |  1328 | ` */` |
|        - |  1329 | `/*` |
|        - |  1330 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|        - |  1331 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|        - |  1332 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|        - |  1333 | ` *` |
|        - |  1334 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|        - |  1335 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|        - |  1336 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|        - |  1337 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|        - |  1338 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|        - |  1339 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|        - |  1340 | ` */` |
| 12017955 |  1341 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  1342 | `{` |
|        - |  1343 | `	VmFrame *pFrame;` |
| 33522909 |  1344 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
| 21506410 |  1345 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  1346 | `		sxu32 n;` |
| 56418919 |  1347 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
| 34913970 |  1348 | `			if( aSlot[n].nIdx == nIdx ){` |
|        - |  1349 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|     1460 |  1350 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|     1460 |  1351 | `				(void)SySetPop(&pFrame->sLocal);` |
|     1460 |  1352 | `				return TRUE; /* Slot owned by exactly one frame */` |
|        - |  1353 | `			}` |
| 17454440 |  1354 | `		}` |
| 10749195 |  1355 | `	}` |
| 12016504 |  1356 | `	return FALSE;` |
|  6006109 |  1357 | `}` |
|        - |  1358 | `/*` |
|        - |  1359 | ` * The superglobal table, asked the cheap question first.` |
|        - |  1360 | ` *` |
|        - |  1361 | ` * Every variable access consults hSuper before the frame -- php resolves $_SERVER` |
|        - |  1362 | ` * the same in every scope, so the order is the semantics and cannot change -- and` |
|        - |  1363 | ` * for the ~9 names that are superglobals ($GLOBALS and the $_* set) the answer is` |
|        - |  1364 | ` * no. Hashing a whole variable name to learn that was, measured on the ecosystem` |
|        - |  1365 | ` * gate's phpcs step, 101M of the engine's 325M hash-table lookups.` |
|        - |  1366 | ` *` |
|        - |  1367 | ` * aSuperFirst is the set of first bytes any INSTALLED superglobal name starts` |
|        - |  1368 | ` * with, so a name whose first byte is not in it cannot be one and never reaches` |
|        - |  1369 | ` * the table. It is a set and not a fixed 'G'/'_' test because an embedder may` |
|        - |  1370 | ` * install a superglobal of its own (PH7_VM_CONFIG_CREATE_SUPER).` |
|        - |  1371 | ` */` |
|  5784548 |  1372 | `PH7_PRIVATE SyHashEntry * PH7_VmSuperGet(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        5 |  1373 | `{` |
|        - |  1374 | `	unsigned char c;` |
|  5784553 |  1375 | `	if( nByte < 1 \|\| zName == 0 ){` |
|        5 |  1376 | `		return 0;` |
|        - |  1377 | `	}` |
|  5784549 |  1378 | `	c = (unsigned char)zName[0];` |
|  5784549 |  1379 | `	if( (pVm->aSuperFirst[c >> 5] & (1u << (c & 31))) == 0 ){` |
|  5641490 |  1380 | `		return 0;` |
|        - |  1381 | `	}` |
|   143064 |  1382 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|  2894627 |  1383 | `}` |
|        - |  1384 | `/* Record a name just installed in hSuper. Every insertion into that table must` |
|        - |  1385 | ` * come through here, or the lookup above stops finding it. */` |
|    73777 |  1386 | `PH7_PRIVATE void PH7_VmSuperNote(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        5 |  1387 | `{` |
|        - |  1388 | `	VmFrame *pFrame;` |
|        - |  1389 | `	unsigned char c;` |
|    73782 |  1390 | `	if( nByte < 1 \|\| zName == 0 ){` |
|      ! 0 |  1391 | `		return;` |
|        - |  1392 | `	}` |
|    73782 |  1393 | `	c = (unsigned char)zName[0];` |
|    73782 |  1394 | `	pVm->aSuperFirst[c >> 5] \|= (1u << (c & 31));` |
|        - |  1395 | `	/* A name the frames may already have memoized as an ordinary variable now` |
|        - |  1396 | `	 * resolves through hSuper instead, and hSuper is consulted FIRST. Installing a` |
|        - |  1397 | `	 * superglobal is a VM-configuration act with only the global frame live, so the` |
|        - |  1398 | `	 * active chain is every frame there is to correct. */` |
|   147559 |  1399 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|    73782 |  1400 | `		VmVarMemoFlush(pFrame);` |
|    36833 |  1401 | `	}` |
|    36833 |  1402 | `}` |
|        - |  1403 | `/*` |
|        - |  1404 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |  1405 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |  1406 | ` * should be skipped when looking for the real execution context.` |
|        - |  1407 | ` */` |
| 58324477 |  1408 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |  1409 | `{` |
| 71555001 |  1410 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
| 13230524 |  1411 | `		pFrame = pFrame->pParent;` |
|        5 |  1412 | `	}` |
| 58324482 |  1413 | `	return pFrame;` |
|        5 |  1414 | `}` |
|        - |  1415 | `/*` |
|        - |  1416 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|        - |  1417 | ` * may be several frames below the frame that caught the exception — must resume at` |
|        - |  1418 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|        - |  1419 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|        - |  1420 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|        - |  1421 | ` *` |
|        - |  1422 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|        - |  1423 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|        - |  1424 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|        - |  1425 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|        - |  1426 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|        - |  1427 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|        - |  1428 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|        - |  1429 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|        - |  1430 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|        - |  1431 | ` * VmSkipExceptionFrames yields its real body frame.` |
|        - |  1432 | ` *` |
|        - |  1433 | ` * This replaces the older "is there a resumable try frame here" test` |
|        - |  1434 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|        - |  1435 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|        - |  1436 | ` * catching frame was not the nearest try (ROOT B).` |
|        - |  1437 | ` */` |
|  1977166 |  1438 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|        5 |  1439 | `{` |
|  1977171 |  1440 | `	if( pVm->pResumeFrame == 0 ){` |
|       68 |  1441 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|        - |  1442 | `	}` |
|  1977107 |  1443 | `	if( pEntryFrame == 0 ){` |
|        - |  1444 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|        - |  1445 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|        - |  1446 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|        - |  1447 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|        - |  1448 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|        - |  1449 | `		 * NULL and the process dies. */` |
|       23 |  1450 | `		return FALSE;` |
|        - |  1451 | `	}` |
|        - |  1452 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|        - |  1453 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|        - |  1454 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|        - |  1455 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|        - |  1456 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|        - |  1457 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|        - |  1458 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|        - |  1459 | `	 * exec's resume site match and land. */` |
|  1977082 |  1460 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|  1673325 |  1461 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|  1367378 |  1462 | `	 \|\| pVm->iResumePc == 0 ){` |
|        - |  1463 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|        - |  1464 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|        - |  1465 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|        - |  1466 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|        - |  1467 | `		 * never match a real frame. */` |
|   611964 |  1468 | `		return FALSE;` |
|        - |  1469 | `	}` |
|        - |  1470 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|        - |  1471 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|        - |  1472 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|        - |  1473 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|        - |  1474 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|        - |  1475 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|        - |  1476 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|        - |  1477 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|        - |  1478 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|        - |  1479 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|        - |  1480 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|        - |  1481 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|        - |  1482 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|        - |  1483 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|        - |  1484 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|        - |  1485 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|        - |  1486 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|        - |  1487 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|        - |  1488 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|        - |  1489 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|        - |  1490 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly.` |
|        - |  1491 | `	 *` |
|        - |  1492 | `	 * The record is CONSUMED FIRST — snapshotted whole and cleared — because the pop` |
|        - |  1493 | `	 * loop below runs USER CODE: leaving a frame releases its locals, and a local's` |
|        - |  1494 | `	 * last reference dying runs that object's __destruct(). A destructor allocates,` |
|        - |  1495 | `	 * calls, and may throw; a throw re-enters VmThrowException, whose first act is to` |
|        - |  1496 | `	 * invalidate the in-flight resume record. Reading pVm->iResumePc AFTER the loop` |
|        - |  1497 | ``	 * therefore read a ZERO the destructor had left behind, and `iResumePc - 1` handed`` |
|        - |  1498 | `	 * the dispatcher -1, which its pc++ turned into a re-run of the whole body from` |
|        - |  1499 | `	 * index 0: monolog's suite restarted its top-level script forever. The loop's own` |
|        - |  1500 | `	 * landing-pad test has to read the snapshot for the same reason. */` |
|        - |  1501 | `	{` |
|        - |  1502 | `		VmResumeTarget sTarget;` |
|  1365128 |  1503 | `		VmSaveResumeTarget(&(*pVm),&sTarget);` |
|  1365128 |  1504 | `		VmClearResumeTarget(&(*pVm)); /* one-shot consume: the whole record, before any teardown */` |
|  2252905 |  1505 | `		while( pVm->pFrame != pEntryFrame` |
|  2508253 |  1506 | `		    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|  1620415 |  1507 | `		         && pVm->pFrame->iExceptionJump == sTarget.iPc) ){` |
|   410552 |  1508 | `			VmLeaveFrame(&(*pVm));` |
|        5 |  1509 | `		}` |
|  1365128 |  1510 | `		*pResumePc = (sxi32)sTarget.iPc - 1;` |
|        - |  1511 | `	}` |
|        - |  1512 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|        - |  1513 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|        - |  1514 | `	 * point router must not re-fire it after this resume. */` |
|  1365128 |  1515 | `	pVm->nBoundaryRc = 0;` |
|  1365128 |  1516 | `	return TRUE;` |
|   988533 |  1517 | `}` |
|        - |  1518 | `/*` |
|        - |  1519 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|        - |  1520 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|        - |  1521 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|        - |  1522 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|        - |  1523 | ` * nested try/finally inside a catch body). Each finally runs with` |
|        - |  1524 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|        - |  1525 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|        - |  1526 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|        - |  1527 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|        - |  1528 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|        - |  1529 | ` */` |
|        - |  1530 | `/*` |
|        - |  1531 | ` * BYTECODE stage 2b — per-activation try state.` |
|        - |  1532 | ` *` |
|        - |  1533 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|        - |  1534 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|        - |  1535 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|        - |  1536 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|        - |  1537 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|        - |  1538 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|        - |  1539 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|        - |  1540 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|        - |  1541 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|        - |  1542 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|        - |  1543 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|        - |  1544 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|        - |  1545 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|        - |  1546 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|        - |  1547 | ` * activation.` |
|        - |  1548 | ` */` |
|  1482706 |  1549 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  1550 | `{` |
|  1482711 |  1551 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|  1482711 |  1552 | `	if( pClone == 0 ){` |
|      ! 0 |  1553 | `		return 0;` |
|        - |  1554 | `	}` |
|  1482711 |  1555 | `	*pClone = *pCompiled;` |
|  1482711 |  1556 | `	pClone->pCompiled = pCompiled;` |
|  1482711 |  1557 | `	pClone->iFinallyDone = 0;` |
|  1482711 |  1558 | `	pClone->iInCatch = 0;` |
|  1482711 |  1559 | `	pClone->pInflight = 0;` |
|  1482711 |  1560 | `	pClone->pFrame = 0;` |
|  1482711 |  1561 | `	return pClone;` |
|   741249 |  1562 | `}` |
|  2948633 |  1563 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|        5 |  1564 | `{` |
|  2948638 |  1565 | `	if( pExc && pExc->pCompiled ){` |
|        - |  1566 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|        - |  1567 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|        - |  1568 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|        - |  1569 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|  1482699 |  1570 | `		if( pExc->pInflight ){` |
|      ! 0 |  1571 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|      ! 0 |  1572 | `			pExc->pInflight = 0;` |
|      ! 0 |  1573 | `		}` |
|  1482699 |  1574 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|   741238 |  1575 | `	}` |
|  2948638 |  1576 | `}` |
|        - |  1577 | `/*` |
|        - |  1578 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|        - |  1579 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|        - |  1580 | ` */` |
|    11197 |  1581 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|        5 |  1582 | `{` |
|    11202 |  1583 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|        5 |  1584 | `}` |
|        - |  1585 | `/*` |
|        - |  1586 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|        - |  1587 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|        - |  1588 | ` * handlers). The set itself is reset by the caller.` |
|        - |  1589 | ` */` |
|   103362 |  1590 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|        5 |  1591 | `{` |
|   103367 |  1592 | `	sxu32 n = SySetUsed(pSet);` |
|   103367 |  1593 | `	if( n > 0 ){` |
|      ! 0 |  1594 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|        - |  1595 | `		sxu32 i;` |
|      ! 0 |  1596 | `		for( i = 0; i < n; i++ ){` |
|      ! 0 |  1597 | `			VmExcRelease(pVm,ap[i]);` |
|      ! 0 |  1598 | `		}` |
|      ! 0 |  1599 | `	}` |
|   103367 |  1600 | `}` |
|        - |  1601 | `/*` |
|        - |  1602 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|        - |  1603 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|        - |  1604 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|        - |  1605 | ` * down a try belonging to the caller.` |
|        - |  1606 | ` */` |
|       18 |  1607 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|        4 |  1608 | `{` |
|       22 |  1609 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|       22 |  1610 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|       22 |  1611 | `	if( nBase < nFloor ){` |
|      ! 0 |  1612 | `		nBase = nFloor;` |
|      ! 0 |  1613 | `	}` |
|       22 |  1614 | `	return VmDrainFinally(&(*pVm),nBase);` |
|        4 |  1615 | `}` |
|        - |  1616 | `/*` |
|        - |  1617 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|        - |  1618 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|        - |  1619 | ` * only carries the compiled pointer.` |
|        - |  1620 | ` */` |
|       84 |  1621 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|        5 |  1622 | `{` |
|       89 |  1623 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       89 |  1624 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|       89 |  1625 | `	while( n > 0 ){` |
|       89 |  1626 | `		n--;` |
|       89 |  1627 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|       89 |  1628 | `			return ap[n];` |
|        - |  1629 | `		}` |
|      ! 0 |  1630 | `	}` |
|      ! 0 |  1631 | `	return 0;` |
|       47 |  1632 | `}` |
|  4452165 |  1633 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |  1634 | `{` |
|        - |  1635 | `	sxu32 nUsed;` |
|  4452170 |  1636 | `	sxi32 rcOut = SXRET_OK;` |
|  4458516 |  1637 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|     6351 |  1638 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     6351 |  1639 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|     6351 |  1640 | `		(void)SySetPop(&pVm->aException);` |
|     6351 |  1641 | `		pExc->pFrame = 0;` |
|        - |  1642 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|        - |  1643 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|        - |  1644 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|        - |  1645 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|        - |  1646 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|     6351 |  1647 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     6351 |  1648 | `			VmLeaveFrame(&(*pVm));` |
|     3173 |  1649 | `		}` |
|     6385 |  1650 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        - |  1651 | `			sxi32 rcF;` |
|       73 |  1652 | `			pExc->iFinallyDone = 1;` |
|       73 |  1653 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|       73 |  1654 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|       73 |  1655 | `			if( rcF == SXERR_ABORT ){` |
|      ! 0 |  1656 | `				return SXERR_ABORT;` |
|        - |  1657 | `			}` |
|       73 |  1658 | `			if( rcF == PH7_EXCEPTION ){` |
|        - |  1659 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|        - |  1660 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|        - |  1661 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|        5 |  1662 | `				rcOut = PH7_EXCEPTION;` |
|        2 |  1663 | `			}` |
|       39 |  1664 | `		}else{` |
|     6283 |  1665 | `			VmExcRelease(&(*pVm),pExc);` |
|        - |  1666 | `		}` |
|        5 |  1667 | `	}` |
|  4452170 |  1668 | `	return rcOut;` |
|  2226131 |  1669 | `}` |
|        - |  1670 | `/*` |
|        - |  1671 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|        - |  1672 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|        - |  1673 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|        - |  1674 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|        - |  1675 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|        - |  1676 | ` */` |
|  3705898 |  1677 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|        5 |  1678 | `{` |
|  3705903 |  1679 | `	pFrame->bHasRet = 0;` |
|  3705903 |  1680 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|  3705903 |  1681 | `	pFrame->nCatchJmpPc = 0;` |
|  3705903 |  1682 | `}` |
|        - |  1683 | `/*` |
|        - |  1684 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|        - |  1685 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|        - |  1686 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|        - |  1687 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|        - |  1688 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|        - |  1689 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|        - |  1690 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|        - |  1691 | ` */` |
|    23990 |  1692 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|        5 |  1693 | `{` |
|    23995 |  1694 | `	if( pResult ){` |
|    23995 |  1695 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|    11995 |  1696 | `	}` |
|    23995 |  1697 | `	VmClearFramePending(pEntryFrame);` |
|    23995 |  1698 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|      ! 0 |  1699 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  1700 | `	}` |
|    23995 |  1701 | `}` |
|        - |  1702 | `/*` |
|        - |  1703 | ` * Compare two functions signature and return the comparison result.` |
|        - |  1704 | ` */` |
|        4 |  1705 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |  1706 | `{` |
|        5 |  1707 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|        5 |  1708 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|        5 |  1709 | `	const char *zSin = pSecond->zString;` |
|        5 |  1710 | `	const char *zFin = pFirst->zString;` |
|        5 |  1711 | `	const char *zPtr = zFin;` |
|        2 |  1712 | `	for(;;){` |
|        5 |  1713 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|        3 |  1714 | `			break;` |
|        - |  1715 | `		}` |
|      ! 0 |  1716 | `		if( zFin[0] != zSin[0] ){` |
|        - |  1717 | `			/* mismatch */` |
|      ! 0 |  1718 | `			break;` |
|        - |  1719 | `		}` |
|      ! 0 |  1720 | `		zFin++;` |
|      ! 0 |  1721 | `		zSin++;` |
|      ! 0 |  1722 | `	}` |
|        5 |  1723 | `	return (int)(zFin-zPtr);` |
|        1 |  1724 | `}` |
|        - |  1725 | `/*` |
|        - |  1726 | ` * Select the appropriate VM function for the current call context.` |
|        - |  1727 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |  1728 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |  1729 | ` * Refer to the official documentation for more information.` |
|        - |  1730 | ` */` |
|       72 |  1731 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|        - |  1732 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  1733 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |  1734 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |  1735 | `	int nArg             /* Total number of passed arguments  */` |
|        - |  1736 | `	)` |
|        4 |  1737 | `{` |
|        - |  1738 | `	int iTarget,i,j,iCur,iMax;` |
|        - |  1739 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |  1740 | `	ph7_vm_func *pLink;` |
|        - |  1741 | `	SyString sArgSig;` |
|        - |  1742 | `	SyBlob sSig;` |
|        - |  1743 |  |
|       76 |  1744 | `	pLink = pList;` |
|       76 |  1745 | `	i = 0;` |
|        - |  1746 | `	/* Put functions expecting the same number of passed arguments */` |
|      636 |  1747 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      584 |  1748 | `		if( pLink == 0 ){` |
|       24 |  1749 | `			break;` |
|        - |  1750 | `		}` |
|      564 |  1751 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |  1752 | `			/* Candidate for overloading */` |
|      564 |  1753 | `			apSet[i++] = pLink;` |
|      280 |  1754 | `		}` |
|        - |  1755 | `		/* Point to the next entry */` |
|      564 |  1756 | `		pLink = pLink->pNextName;` |
|        4 |  1757 | `	}` |
|       76 |  1758 | `	if( i < 1 ){` |
|        - |  1759 | `		/* No candidates,return the head of the list */` |
|      ! 0 |  1760 | `		return pList;` |
|        - |  1761 | `	}` |
|       76 |  1762 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |  1763 | `		/* Return the only candidate */` |
|       74 |  1764 | `		return apSet[0];` |
|        - |  1765 | `	}` |
|        - |  1766 | `	/* Calculate function signature */` |
|        3 |  1767 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|        5 |  1768 | `	for( j = 0 ; j < nArg ; j++ ){` |
|        3 |  1769 | `		int c = 'n'; /* null */` |
|        3 |  1770 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |  1771 | `			/* Hashmap */` |
|      ! 0 |  1772 | `			c = 'h';` |
|        3 |  1773 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |  1774 | `			/* bool */` |
|      ! 0 |  1775 | `			c = 'b';` |
|        3 |  1776 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |  1777 | `			/* int */` |
|        3 |  1778 | `			c = 'i';` |
|        1 |  1779 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |  1780 | `			/* String */` |
|      ! 0 |  1781 | `			c = 's';` |
|      ! 0 |  1782 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |  1783 | `			/* Float */` |
|      ! 0 |  1784 | `			c = 'f';` |
|      ! 0 |  1785 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |  1786 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|      ! 0 |  1787 | `			int marker = 'o';` |
|      ! 0 |  1788 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |  1789 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |  1790 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|      ! 0 |  1791 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |  1792 | `			c = -1;` |
|      ! 0 |  1793 | `		}` |
|        3 |  1794 | `		if( c > 0 ){` |
|        3 |  1795 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        1 |  1796 | `		}` |
|        2 |  1797 | `	}` |
|        3 |  1798 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|        3 |  1799 | `	iTarget = 0;` |
|        3 |  1800 | `	iMax = -1;` |
|        - |  1801 | `	/* Select the appropriate function */` |
|        7 |  1802 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |  1803 | `		/* Compare the two signatures */` |
|        5 |  1804 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|        5 |  1805 | `		if( iCur > iMax ){` |
|        3 |  1806 | `			iMax = iCur;` |
|        3 |  1807 | `			iTarget = j;` |
|        1 |  1808 | `		}` |
|        3 |  1809 | `	}` |
|        3 |  1810 | `	SyBlobRelease(&sSig);` |
|        - |  1811 | `	/* Appropriate function for the current call context */` |
|        3 |  1812 | `	return apSet[iTarget];` |
|       40 |  1813 | `}` |
|        - |  1814 | `/* Forward declaration */` |
|        - |  1815 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |  1816 | `/*` |
|        - |  1817 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot.` |
|        - |  1818 | ` *` |
|        - |  1819 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|        - |  1820 | ` * end-of-exec. When the pool was one doubling buffer, an initializer that` |
|        - |  1821 | ` * allocated pool memobjs — a large array literal reserves one per element —` |
|        - |  1822 | ` * reallocated and FREED that buffer, so a pResult pointing into it dangled and` |
|        - |  1823 | ` * the final store was a heap use-after-free (confirmed via ASan on a >=~227` |
|        - |  1824 | ` * element class-const array; it is what blocked Composer's autoload class-map).` |
|        - |  1825 | ` * The answer was to evaluate into a stable local and store into the slot` |
|        - |  1826 | ` * re-fetched by its index, which is what this does. Redundant since P1 -- the` |
|        - |  1827 | ` * pool's segments are fixed, so a slot's address never moves. Left for the` |
|        - |  1828 | ` * harvest sweep; the history above is why it was ever needed.` |
|        - |  1829 | ` *` |
|        - |  1830 | ` * On return *ppMemObj points at the valid post-eval slot. PH7_MemObjStore` |
|        - |  1831 | ` * preserves the destination slot's nIdx (excluded from its memcpy), so the slot` |
|        - |  1832 | ` * identity is kept. Mirrors the enum-case backing path.` |
|        - |  1833 | ` */` |
|     4326 |  1834 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|        5 |  1835 | `{` |
|        - |  1836 | `	ph7_value sVal;` |
|     4331 |  1837 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|        - |  1838 | `	sxi32 rc;` |
|     4331 |  1839 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|     4331 |  1840 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|        - |  1841 | `	/* Re-fetch by the reserved index (redundant since P1: see above). */` |
|     4331 |  1842 | `	*ppMemObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     4331 |  1843 | `	if( *ppMemObj ){` |
|     4331 |  1844 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|     2163 |  1845 | `	}` |
|     4331 |  1846 | `	PH7_MemObjRelease(&sVal);` |
|     4331 |  1847 | `	return rc;` |
|        5 |  1848 | `}` |
|        - |  1849 | `/*` |
|        - |  1850 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|        - |  1851 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|        - |  1852 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|        - |  1853 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|        - |  1854 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|        - |  1855 | ` * C caller ever regains control.` |
|        - |  1856 | ` */` |
|        - |  1857 | `typedef struct VmMuteState {` |
|        - |  1858 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|        - |  1859 | `	sxu32 nSaved;` |
|        - |  1860 | `	sxi32 iSaveStatus;` |
|        - |  1861 | `	sxi32 iSaveBoundary;` |
|        - |  1862 | `	VmResumeTarget sSaveResume;` |
|        - |  1863 | `	ph7_class_attr *pSaveCycleAttr;` |
|        - |  1864 | `	ph7_class *pSaveCycleClass;` |
|        - |  1865 | `} VmMuteState;` |
|      634 |  1866 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|        5 |  1867 | `{` |
|      639 |  1868 | `	pSave->apSaved = 0;` |
|      639 |  1869 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|      639 |  1870 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|      639 |  1871 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|      639 |  1872 | `	VmSaveResumeTarget(&(*pVm),&pSave->sSaveResume);` |
|      639 |  1873 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|      639 |  1874 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|      639 |  1875 | `	if( pSave->nSaved > 0 ){` |
|      410 |  1876 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|      204 |  1877 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|      206 |  1878 | `		if( pSave->apSaved ){` |
|      308 |  1879 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|      204 |  1880 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|      206 |  1881 | `			SySetReset(&pVm->aException);` |
|      102 |  1882 | `		}` |
|      102 |  1883 | `	}` |
|      639 |  1884 | `	pVm->nMuteThrow++;` |
|      639 |  1885 | `}` |
|        - |  1886 | `/*` |
|        - |  1887 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|        - |  1888 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|        - |  1889 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|        - |  1890 | ` * TRUE when a throw was actually swallowed.` |
|        - |  1891 | ` */` |
|      634 |  1892 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|        5 |  1893 | `{` |
|        - |  1894 | `	VmFrame *pFrame;` |
|      639 |  1895 | `	pVm->nMuteThrow--;` |
|        - |  1896 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|        - |  1897 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|        - |  1898 | `	 * activation behind: release whatever is there whether or not anything was` |
|        - |  1899 | `	 * hidden. */` |
|      639 |  1900 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|      639 |  1901 | `	SySetReset(&pVm->aException);` |
|      639 |  1902 | `	if( pSave->apSaved ){` |
|        - |  1903 | `		sxu32 k;` |
|      410 |  1904 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|      206 |  1905 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|      104 |  1906 | `		}` |
|      206 |  1907 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|      206 |  1908 | `		pSave->apSaved = 0;` |
|      102 |  1909 | `	}` |
|      639 |  1910 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|      589 |  1911 | `		return FALSE;` |
|        - |  1912 | `	}` |
|       54 |  1913 | `	pFrame = pVm->pFrame;` |
|       54 |  1914 | `	if( pFrame ){` |
|       54 |  1915 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       54 |  1916 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       25 |  1917 | `	}` |
|       54 |  1918 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|       54 |  1919 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|       54 |  1920 | `	VmRestoreResumeTarget(&(*pVm),&pSave->sSaveResume);` |
|       54 |  1921 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|       54 |  1922 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|       54 |  1923 | `	return TRUE;` |
|      322 |  1924 | `}` |
|        - |  1925 | `/*` |
|        - |  1926 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|        - |  1927 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|        - |  1928 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|        - |  1929 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|        - |  1930 | ` *` |
|        - |  1931 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|        - |  1932 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|        - |  1933 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|        - |  1934 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|        - |  1935 | ` */` |
|      198 |  1936 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|        - |  1937 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - |  1938 | `	ph7_class_instance *pThis,   /* Receiver */` |
|        - |  1939 | `	ph7_class_method *pMethod,   /* Method to run */` |
|        - |  1940 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|        - |  1941 | `	int nArg,                    /* Argument count */` |
|        - |  1942 | `	ph7_value **apArg,           /* Arguments */` |
|        - |  1943 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|        - |  1944 | `	)` |
|        1 |  1945 | `{` |
|        - |  1946 | `	VmMuteState sSave;` |
|        - |  1947 | `	sxi32 rc;` |
|      199 |  1948 | `	VmMuteEnter(&(*pVm),&sSave);` |
|      199 |  1949 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|      199 |  1950 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|        7 |  1951 | `		if( pbThrew ){` |
|        7 |  1952 | `			*pbThrew = TRUE;` |
|        3 |  1953 | `		}` |
|        7 |  1954 | `		return SXRET_OK;` |
|        - |  1955 | `	}` |
|      193 |  1956 | `	if( pbThrew ){` |
|      193 |  1957 | `		*pbThrew = FALSE;` |
|       96 |  1958 | `	}` |
|      193 |  1959 | `	return rc;` |
|      100 |  1960 | `}` |
|        - |  1961 | `/*` |
|        - |  1962 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|        - |  1963 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|        - |  1964 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|        - |  1965 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|        - |  1966 | ` * restored on the way out. A throw comes back only as the returned status.` |
|        - |  1967 | ` *` |
|        - |  1968 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|        - |  1969 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|        - |  1970 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|        - |  1971 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|        - |  1972 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|        - |  1973 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|        - |  1974 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|        - |  1975 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|        - |  1976 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|        - |  1977 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|        - |  1978 | ` */` |
|      420 |  1979 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|        5 |  1980 | `{` |
|        - |  1981 | `	VmMuteState sSave;` |
|        - |  1982 | `	sxi32 rc;` |
|      425 |  1983 | `	VmMuteEnter(&(*pVm),&sSave);` |
|      425 |  1984 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|      425 |  1985 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|        - |  1986 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|        - |  1987 | `		 * where it is found — the innermost evaluation only records it for an` |
|        - |  1988 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|        - |  1989 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|        - |  1990 | `		 * records the cycle again and raises it there. */` |
|      ! 0 |  1991 | `		rc = PH7_EXCEPTION;` |
|      ! 0 |  1992 | `	}` |
|        - |  1993 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|        - |  1994 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|        - |  1995 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|        - |  1996 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|        - |  1997 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|        - |  1998 | `	 * detects the cycle again. */` |
|      425 |  1999 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|      425 |  2000 | `	return rc;` |
|        5 |  2001 | `}` |
|        - |  2002 | `/*` |
|        - |  2003 | ` * Run a compiled constant expression only to LOOK at the value it produces, and` |
|        - |  2004 | ` * report whether php's own compiler would have FOLDED it.` |
|        - |  2005 | ` *` |
|        - |  2006 | ` * php folds a parameter default at compile time and keeps the folded zval; what` |
|        - |  2007 | `` * it cannot reduce stays an AST and prints as `<expression>` in the declaration`` |
|        - |  2008 | ` * php renders for an incompatible-override fatal (see PH7_ClassRenderDecl).` |
|        - |  2009 | ``  * "Cannot reduce" is not a syntactic property -- `2 * 1024` folds and `1 / 0` `` |
|        - |  2010 | ` * does not -- so the question is asked by RUNNING the program and watching for` |
|        - |  2011 | ` * anything php's folder would have refused on.` |
|        - |  2012 | ` *` |
|        - |  2013 | ` * The window is doubly sealed, because this runs at CLASS-LINK time: a program` |
|        - |  2014 | ` * php has not reached, in the middle of compiling one it has. VmMuteEnter hides` |
|        - |  2015 | `` * the live try activations and swallows a throw (`1/0`, `"a"+1`), and`` |
|        - |  2016 | ` * nSpeculative drops every diagnostic without running a user error handler or` |
|        - |  2017 | `` * touching error_get_last() (`[1,2][5]`). Either one having happened is exactly`` |
|        - |  2018 | ` * php's "did not fold".` |
|        - |  2019 | ` *` |
|        - |  2020 | ` * Returns TRUE with *pOut holding the value, or FALSE (caller prints` |
|        - |  2021 | `` * `<expression>`). The caller must have SCREENED the program first: only a run`` |
|        - |  2022 | ` * built from literal loads and pure value operators belongs here -- a constant` |
|        - |  2023 | `` * NAME, a class constant and a `new` are all things php keeps unfolded and this`` |
|        - |  2024 | ` * would happily evaluate (or construct).` |
|        - |  2025 | ` */` |
|       16 |  2026 | `PH7_PRIVATE int PH7_VmEvalConstExpr(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut)` |
|        2 |  2027 | `{` |
|        - |  2028 | `	VmMuteState sSave;` |
|        - |  2029 | `	sxu32 nDiag;` |
|        - |  2030 | `	sxi32 rc;` |
|        - |  2031 | `	int bFolded;` |
|       18 |  2032 | `	int bFramePushed = 0;` |
|       18 |  2033 | `	if( pVm->pFrame == 0 ){` |
|        - |  2034 | `		/* CLASS-LINK time: the script has not started, so there is no frame at all --` |
|        - |  2035 | `		 * and the reference table, the array builder and the throw path all read one.` |
|        - |  2036 | `		 * Stand a global-shaped frame up for the duration (pParent == 0, so its` |
|        - |  2037 | `		 * teardown is the global frame's: nothing of the caller's is torn down with` |
|        - |  2038 | `		 * it). Without this an array default segfaulted the compiler. */` |
|      ! 0 |  2039 | `		if( VmEnterFrame(&(*pVm),0,0,0) != SXRET_OK ){` |
|      ! 0 |  2040 | `			return 0;` |
|        - |  2041 | `		}` |
|      ! 0 |  2042 | `		bFramePushed = 1;` |
|      ! 0 |  2043 | `	}` |
|       18 |  2044 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       18 |  2045 | `	pVm->nSpeculative++;` |
|       18 |  2046 | `	nDiag = pVm->nSpecDiag;` |
|       18 |  2047 | `	rc = VmLocalExec(&(*pVm),pByteCode,pOut,FALSE);` |
|       18 |  2048 | `	pVm->nSpeculative--;` |
|       18 |  2049 | `	bFolded = ( rc == SXRET_OK && pVm->nSpecDiag == nDiag );` |
|       18 |  2050 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|      ! 0 |  2051 | `		bFolded = 0; /* a throw was swallowed */` |
|      ! 0 |  2052 | `	}` |
|       18 |  2053 | `	if( bFramePushed ){` |
|      ! 0 |  2054 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2055 | `	}` |
|       18 |  2056 | `	return bFolded;` |
|       10 |  2057 | `}` |
|        - |  2058 | `/*` |
|        - |  2059 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |  2060 | ` * it can be instanciated from the executed PHP script.` |
|        - |  2061 | ` */` |
|        - |  2062 | `/*` |
|        - |  2063 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |  2064 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |  2065 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |  2066 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |  2067 | ` * properties register their enforcement slot. It is factored out of` |
|        - |  2068 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |  2069 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |  2070 | ` */` |
|  4205562 |  2071 | `static sxi32 VmMountUserClassAttrs(` |
|        - |  2072 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  2073 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |  2074 | `	)` |
|        5 |  2075 | `{` |
|        - |  2076 | `	ph7_class_attr *pAttr;` |
|        - |  2077 | `	SyHashEntry *pEntry;` |
|        - |  2078 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|        - |  2079 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|        - |  2080 | `	 * over both tables. */` |
|        - |  2081 | `	SyHash *apMount[2];` |
|        - |  2082 | `	int iMount;` |
|  4205567 |  2083 | `	apMount[0] = &pClass->hAttr;` |
|  4205567 |  2084 | `	apMount[1] = &pClass->hConst;` |
| 12616683 |  2085 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|        - |  2086 | `	/* Reset the loop cursor */` |
|  8411129 |  2087 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|        - |  2088 | `	/* Process only static and constant attribute */` |
| 36254369 |  2089 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|        - |  2090 | `		/* Extract the current attribute */` |
| 27843253 |  2091 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 27843248 |  2092 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
| 19170103 |  2093 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|  5255094 |  2094 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|        - |  2095 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|        - |  2096 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|        - |  2097 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|        - |  2098 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|        - |  2099 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|        - |  2100 | `			 * user catch, and initializers referencing constants of a class` |
|        - |  2101 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|        - |  2102 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|        - |  2103 | `			 * %s as value for class constant" fatal without any access). */` |
| 10520431 |  2104 | `			continue;` |
|        - |  2105 | `		}` |
| 17322827 |  2106 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  2107 | `			ph7_value *pMemObj;` |
|     7997 |  2108 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|        - |  2109 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|        - |  2110 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|        - |  2111 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|        - |  2112 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|        - |  2113 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|        - |  2114 | `				 * access / instantiation throws like php's. */` |
|     7524 |  2115 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     5054 |  2116 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|        - |  2117 | `						/* Its default threw at the other class's mount and is` |
|        - |  2118 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|        - |  2119 | `						 * both static tables). */` |
|        3 |  2120 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|     5052 |  2121 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       20 |  2122 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|       12 |  2123 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       14 |  2124 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|        3 |  2125 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        1 |  2126 | `						}` |
|        6 |  2127 | `					}` |
|     2525 |  2128 | `				}` |
|     7527 |  2129 | `				continue;` |
|        - |  2130 | `			}` |
|        - |  2131 | `			/* Reserve a memory object for this constant/static attribute */` |
|      477 |  2132 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      477 |  2133 | `			if( pMemObj == 0 ){` |
|      ! 0 |  2134 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  2135 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |  2136 | `					&pClass->sDisp,&pAttr->sName` |
|        - |  2137 | `					);` |
|      ! 0 |  2138 | `				return SXERR_MEM;` |
|        - |  2139 | `			}` |
|      477 |  2140 | `			if( pAttr->pNativeValue ){` |
|        - |  2141 | `				/* A native class's literal initializer: no expression to run. */` |
|      ! 0 |  2142 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|      477 |  2143 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |  2144 | `				/* Initialize attribute default value (any complex expression).` |
|        - |  2145 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|        - |  2146 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|      425 |  2147 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      425 |  2148 | `				void *pSaveFrame = pVm->pConstEvalFrame;` |
|      425 |  2149 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|        - |  2150 | `				sxi32 rcExec;` |
|      425 |  2151 | `				pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - |  2152 | `				/* ...and the frame marker is what makes that fallback reachable when a` |
|        - |  2153 | `				 * frame IS current: a class declared inside a METHOD mounts here, and` |
|        - |  2154 | `				 * without the marker PH7_VmPeekDeclaringClass answers that method's` |
|        - |  2155 | ``				 * class instead (`class G { function go(){ eval('class Q { const K=5;`` |
|        - |  2156 | ``				 * public static $s = self::K; }'); } }` read G::K). */`` |
|      425 |  2157 | `				pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      425 |  2158 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|      425 |  2159 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|      425 |  2160 | `				pVm->nConstEvalDepth++;` |
|        - |  2161 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|        - |  2162 | `				 * when the member is first USED, so a throw at declaration time is` |
|        - |  2163 | `				 * not something it can see. What reaches this line is a static` |
|        - |  2164 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|        - |  2165 | `				 * which php does validate here, but only when the initializer` |
|        - |  2166 | ``				 * actually produced a value: `const int A = "x"` and`` |
|        - |  2167 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|        - |  2168 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|      425 |  2169 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|      425 |  2170 | `				pVm->nConstEvalDepth--;` |
|      425 |  2171 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      425 |  2172 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      425 |  2173 | `				pVm->pConstEvalFrame = pSaveFrame;` |
|      425 |  2174 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - |  2175 | `					/* php has not reached this initializer: defer it whole to the` |
|        - |  2176 | `					 * first USE, where the throw is raised at the access site and is` |
|        - |  2177 | `					 * catchable there. The leftover value is null and must NOT be` |
|        - |  2178 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|        - |  2179 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|        - |  2180 | `					 * property keeps its (already reserved) slot and re-runs through` |
|        - |  2181 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|        - |  2182 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|        - |  2183 | `					 * unset nIdx. */` |
|       47 |  2184 | `					if( bStaticProp ){` |
|       41 |  2185 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|       41 |  2186 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|       22 |  2187 | `					}else{` |
|        - |  2188 | `						/* Release before recycling: the value a muted eval that` |
|        - |  2189 | `						 * only recorded a CYCLE left here must go before the` |
|        - |  2190 | `						 * slot's dead nIdx word becomes the free-list link. */` |
|        8 |  2191 | `						PH7_MemObjRelease(pMemObj);` |
|        8 |  2192 | `						VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|        8 |  2193 | `						continue;` |
|        - |  2194 | `					}` |
|      398 |  2195 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|      193 |  2196 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|        - |  2197 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - |  2198 | `					 * against the declared type. A mismatch is a non-catchable` |
|        - |  2199 | `					 * fatal, raised here at definition time (matching PHP). */` |
|       55 |  2200 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|       55 |  2201 | `					if( rcType != SXRET_OK ){` |
|       11 |  2202 | `						return rcType;` |
|        - |  2203 | `					}` |
|       21 |  2204 | `				}` |
|      203 |  2205 | `			}` |
|        - |  2206 | `			/* Record attribute index */` |
|      463 |  2207 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |  2208 | `			/* Install static attribute in the reference table */` |
|      463 |  2209 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |  2210 | `			/* If this is a typed static property, register the slot so the` |
|        - |  2211 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |  2212 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |  2213 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - |  2214 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - |  2215 | `			 * already enforced above, so they need no store-time slot. */` |
|      458 |  2216 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|      302 |  2217 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       99 |  2218 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       99 |  2219 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |  2220 | `					return SXERR_MEM;` |
|        - |  2221 | `				}` |
|       99 |  2222 | `				pVmAttrS->pAttr = pAttr;` |
|       99 |  2223 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       99 |  2224 | `				pVmAttrS->iState = 0;` |
|       99 |  2225 | `				PH7_VmAttrSetClass(pVmAttrS,pClass);   /* the class's own slot: no instance behind it */` |
|        - |  2226 | `				/* Static typed property with no default starts uninitialized` |
|        - |  2227 | `				 * (constants are already excluded by the enclosing condition). */` |
|       99 |  2228 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|       30 |  2229 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       86 |  2230 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|        - |  2231 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|        - |  2232 | `					 * typed static default LAZILY at the first static-property` |
|        - |  2233 | `					 * access / instantiation (a never-touched bad default is` |
|        - |  2234 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|        - |  2235 | `					 * place (int -> float widening, whole-real materialization,` |
|        - |  2236 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|        - |  2237 | `					 * the slot and the class are flagged, and the access sites` |
|        - |  2238 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|        - |  2239 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|        - |  2240 | `					 * the materializer checks it after the re-run. */` |
|       73 |  2241 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|       25 |  2242 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|       25 |  2243 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|       11 |  2244 | `					}` |
|       34 |  2245 | `				}` |
|       99 |  2246 | `				if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |  2247 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |  2248 | `					return SXERR_MEM;` |
|        - |  2249 | `				}` |
|       47 |  2250 | `			}` |
|      229 |  2251 | `		}` |
|        5 |  2252 | `	}` |
|  4200745 |  2253 | `	} /* for iMount */` |
|  4205559 |  2254 | `	return SXRET_OK;` |
|  2100377 |  2255 | `}` |
|        - |  2256 | `/*` |
|        - |  2257 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|        - |  2258 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|        - |  2259 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|        - |  2260 | ` */` |
|  4203368 |  2261 | `static sxi32 VmMountUserClassMethods(` |
|        - |  2262 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  2263 | `	ph7_class *pClass /* Class whose methods are installed */` |
|        - |  2264 | `	)` |
|        5 |  2265 | `{` |
|        - |  2266 | `	ph7_class_method *pMeth;` |
|        - |  2267 | `	SyHashEntry *pEntry;` |
|        - |  2268 | `	sxi32 rc;` |
|        - |  2269 | `	/* Install class methods */` |
|  4203373 |  2270 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |  2271 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |  2272 | `		 */` |
|  1363253 |  2273 | `		return SXRET_OK;` |
|        - |  2274 | `	}` |
|        - |  2275 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|        - |  2276 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|        - |  2277 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|        - |  2278 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|        - |  2279 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|        - |  2280 | `	/* Install the methods now */` |
|  2840125 |  2281 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
| 47568807 |  2282 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
| 43306493 |  2283 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 43306493 |  2284 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
| 43247939 |  2285 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
| 43247939 |  2286 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2287 | `				return rc;` |
|        - |  2288 | `			}` |
| 21591429 |  2289 | `		}` |
|        5 |  2290 | `	}` |
|        - |  2291 | `	/* Mark class as mounted to avoid redundant mounting */` |
|  2840125 |  2292 | `	pClass->bMounted = TRUE;` |
|  2840125 |  2293 | `	return SXRET_OK;` |
|  2099280 |  2294 | `}` |
|  2736195 |  2295 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |  2296 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  2297 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |  2298 | `	)` |
|        5 |  2299 | `{` |
|        - |  2300 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|        - |  2301 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|        - |  2302 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|        - |  2303 | `	 * initializer that throws finds the exception classes ready. */` |
|  2736200 |  2304 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|  2736200 |  2305 | `	if( rc != SXRET_OK ){` |
|        3 |  2306 | `		return rc;` |
|        - |  2307 | `	}` |
|  2736198 |  2308 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|  1366898 |  2309 | `}` |
|        - |  2310 | `/*` |
|        - |  2311 | ` * Allocate a private frame for attributes of the given` |
|        - |  2312 | ` * class instance (Object in the PHP jargon).` |
|        - |  2313 | ` */` |
|  1628479 |  2314 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |  2315 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2316 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |  2317 | `	)` |
|        5 |  2318 | `{` |
|  1628484 |  2319 | `	ph7_class *pClass = pObj->pClass;` |
|        - |  2320 | `	ph7_class_attr *pAttr;` |
|        - |  2321 | `	SyHashEntry *pEntry;` |
|        - |  2322 | `	sxi32 rc;` |
|  1628484 |  2323 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|        - |  2324 | `	                    * at the FIRST bad default; a second registered throw` |
|        - |  2325 | `	                    * would escape the catch as an uncaught fatal. */` |
|        - |  2326 | `	/* Install class attribute in the private frame associated with this instance */` |
|  1628484 |  2327 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
| 12274327 |  2328 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |  2329 | `		VmClassAttr *pVmAttr;` |
|        - |  2330 | `		/* The KEY the class filed it under, not the attribute's own name: an` |
|        - |  2331 | `		 * inherited PRIVATE instance property lives under php's mangled storage` |
|        - |  2332 | ``		 * name (PH7_ClassAttrStorageName), which is what keeps a base's `$q` and a`` |
|        - |  2333 | ``		 * child's `$q` two slots on one object instead of one. */`` |
| 10645848 |  2334 | `		const void *pKey = pEntry->pKey;` |
| 10645848 |  2335 | `		sxu32 nKeyLen = pEntry->nKeyLen;` |
|        - |  2336 | `		/* Extract the current attribute */` |
| 10645848 |  2337 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 10645848 |  2338 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT ){` |
|        - |  2339 | `			/* php's VIRTUAL property: the class declares the name and answers it` |
|        - |  2340 | `			 * from its own state, and the OBJECT has no slot for it at all -- so` |
|        - |  2341 | `			 * every table walk (the (array) cast, get_object_vars, foreach,` |
|        - |  2342 | `			 * json_encode, var_export, serialize) finds nothing, and a read, a` |
|        - |  2343 | `			 * write or an isset() takes the miss path to the class's magic trio.` |
|        - |  2344 | `			 * Nothing ever installs one; unlike the LAZY kind there is no C body` |
|        - |  2345 | `			 * behind it that would. */` |
|   179217 |  2346 | `			continue;` |
|        - |  2347 | `		}` |
| 10466636 |  2348 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY ){` |
|        - |  2349 | `			/* A property php's own object does not HOLD until its constructor` |
|        - |  2350 | `			 * fills it: no slot, no hAttr entry, nothing for a read, an isset()` |
|        - |  2351 | `			 * or a property walk to find. PH7_NativeMaterializeLazy installs the` |
|        - |  2352 | `			 * whole set the first time a C body writes one. */` |
|     9383 |  2353 | `			continue;` |
|        - |  2354 | `		}` |
| 10457256 |  2355 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
| 10457256 |  2356 | `		if( pVmAttr == 0 ){` |
|      ! 0 |  2357 | `			return SXERR_MEM;` |
|        - |  2358 | `		}` |
| 10457256 |  2359 | `		pVmAttr->pAttr = pAttr;` |
| 10457256 |  2360 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |  2361 | `			ph7_value *pMemObj;` |
|        - |  2362 | `			/* Reserve a memory object for this attribute */` |
| 10455904 |  2363 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
| 10455904 |  2364 | `			if( pMemObj == 0 ){` |
|      ! 0 |  2365 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2366 | `				return SXERR_MEM;` |
|        - |  2367 | `			}` |
| 10455904 |  2368 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
| 10455904 |  2369 | `			pVmAttr->iState = 0;` |
| 10455904 |  2370 | `			PH7_VmAttrSetInst(pVmAttr,pObj);` |
| 10455904 |  2371 | `			if( pAttr->pNativeValue ){` |
|        - |  2372 | `				/* Native class, literal default: no initializer to execute, so none` |
|        - |  2373 | `				 * of the throw/typed-default machinery below can apply either — a` |
|        - |  2374 | `				 * literal cannot throw and the builder states the type itself. */` |
|  9972151 |  2375 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|  5469264 |  2376 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |  2377 | `				/* Initialize attribute default value (any complex expression).` |
|        - |  2378 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|        - |  2379 | ``				 * against the declaring class. This runs at `new`, i.e. at an`` |
|        - |  2380 | `				 * arbitrary point in execution -- typically inside some OTHER` |
|        - |  2381 | `				 * class's method, whose frame is still current because` |
|        - |  2382 | `				 * VmLocalExec pushes none of its own. Mark that frame, or` |
|        - |  2383 | `				 * PH7_VmPeekDeclaringClass answers the caller's class and` |
|        - |  2384 | ``				 * `public $v = self::K` read F::K when `new` ran in F::make(). */`` |
|     3367 |  2385 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|     3367 |  2386 | `				void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - |  2387 | `				sxi32 rcExec;` |
|     3367 |  2388 | `				pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|     3367 |  2389 | `				pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|     3367 |  2390 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|     3367 |  2391 | `				pVm->pConstEvalClass = pSaveCtx;` |
|     3367 |  2392 | `				pVm->pConstEvalFrame = pSaveFrame;` |
|     3367 |  2393 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - |  2394 | `					/* The initializer itself threw (undefined constant, throwing` |
|        - |  2395 | `					 * enum case): its exception is already registered — do NOT` |
|        - |  2396 | `					 * also type-check the leftover value (a spurious second` |
|        - |  2397 | `					 * TypeError would escape the user's catch), and throw` |
|        - |  2398 | `					 * nothing further for the remaining attributes.` |
|        - |  2399 | `					 *` |
|        - |  2400 | `					 * PARK the status too, exactly as the typed-default branch` |
|        - |  2401 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|        - |  2402 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|        - |  2403 | `					 * and the throw came back as a status nobody read: this` |
|        - |  2404 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|        - |  2405 | `					 * and the whole statement RESUMED after the catch — php` |
|        - |  2406 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|        - |  2407 | `					 * existing construction-aborted route. */` |
|       25 |  2408 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|       25 |  2409 | `					bDefThrew = 1;` |
|     3355 |  2410 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|        - |  2411 | `					/* Typed property DEFAULT: php validates the computed value` |
|        - |  2412 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|        - |  2413 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|        - |  2414 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|        - |  2415 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|        - |  2416 | `					 * aborts construction) / the fetch-point router. */` |
|      565 |  2417 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|      565 |  2418 | `					if( rcDef != SXRET_OK ){` |
|       13 |  2419 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|       13 |  2420 | `						bDefThrew = 1;` |
|        6 |  2421 | `					}` |
|      285 |  2422 | `				}` |
|   482077 |  2423 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |  2424 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |  2425 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|   477472 |  2426 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|   238693 |  2427 | `			}` |
| 10455904 |  2428 | `			rc = SyHashInsertTail(&pObj->hAttr,pKey,nKeyLen,pVmAttr);` |
| 10455904 |  2429 | `			if( rc != SXRET_OK ){` |
|        - |  2430 | `				/* Restore the reserved (NULL-valued) slot to the free list */` |
|      ! 0 |  2431 | `				VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|      ! 0 |  2432 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2433 | `				return SXERR_MEM;` |
|        - |  2434 | `			}` |
|        - |  2435 | `			/* Install attribute in the reference table */` |
| 10455904 |  2436 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |  2437 | `			/* Register the slot with the store filter -- a declared TYPE to` |
|        - |  2438 | `			 * enforce, a native class's write handler, or both. On failure roll` |
|        - |  2439 | `			 * back the just-installed hAttr entry and the reserved memobj so the` |
|        - |  2440 | `			 * caller sees a consistent instance. */` |
| 10455904 |  2441 | `			rc = PH7_VmStoreFilterRegister(&(*pVm),pVmAttr);` |
| 10455904 |  2442 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2443 | `				SyHashDeleteEntry(&pObj->hAttr,pKey,nKeyLen,0);` |
|      ! 0 |  2444 | `				VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|      ! 0 |  2445 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2446 | `				return SXERR_MEM;` |
|        - |  2447 | `			}` |
|  5227347 |  2448 | `		}else{` |
|        - |  2449 | `			/* Install static/constant attribute */` |
|     1357 |  2450 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|     1357 |  2451 | `			pVmAttr->iState = 0;` |
|     1357 |  2452 | `			PH7_VmAttrSetClass(pVmAttr,pClass);   /* a static slot belongs to the class, not to this object */` |
|     1357 |  2453 | `			rc = SyHashInsertTail(&pObj->hAttr,pKey,nKeyLen,pVmAttr);` |
|     1357 |  2454 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2455 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2456 | `				return SXERR_MEM;` |
|        - |  2457 | `			}` |
|        - |  2458 | `		}` |
|        5 |  2459 | `	}` |
|  1628484 |  2460 | `	return SXRET_OK;` |
|   814077 |  2461 | `}` |
|        - |  2462 | `/*` |
|        - |  2463 | ` * Whether [pClass] permits runtime-created (dynamic) properties: stdClass, and` |
|        - |  2464 | `` * any class php's own `#[AllowDynamicProperties]` opts in (the attribute is`` |
|        - |  2465 | ` * inherited, so the ancestry is walked -- VmClassHasAttributeNamed does that).` |
|        - |  2466 | ` *` |
|        - |  2467 | ` * The attribute half used to be spelled out at each caller, and one of the three` |
|        - |  2468 | ` * did not have it: the by-REFERENCE binder (VmBindPropByRef) asked this alone, so` |
|        - |  2469 | `` * an opted-in class refused `f($o->undeclared)` with the scope policy's `Cannot create dynamic`` |
|        - |  2470 | `` * property` on a write php performs -- while `$o->undeclared = 1` next to it`` |
|        - |  2471 | ` * worked. One decision, one site.` |
|        - |  2472 | ` */` |
|      340 |  2473 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|        5 |  2474 | `{` |
|      345 |  2475 | `	if( pVm->pStdClass != 0 ){` |
|        - |  2476 | `		/* ...and stdClass's own permission is INHERITED, exactly as the attribute` |
|        - |  2477 | ``		 * is: php deprecates nothing for `class C extends stdClass`, so the scope policy must`` |
|        - |  2478 | `		 * refuse nothing there either. Only the class ITSELF was recognized, so a` |
|        - |  2479 | `		 * subclass of the one class php lets a script build freely could not take a` |
|        - |  2480 | `		 * property at all. */` |
|      345 |  2481 | `		ph7_class *pAncestor = pClass;` |
|      437 |  2482 | `		while( pAncestor ){` |
|      357 |  2483 | `			if( pAncestor == pVm->pStdClass ){` |
|      265 |  2484 | `				return TRUE;` |
|        - |  2485 | `			}` |
|       97 |  2486 | `			pAncestor = pAncestor->pBase;` |
|        5 |  2487 | `		}` |
|       40 |  2488 | `	}` |
|       85 |  2489 | `	return VmClassHasAttributeNamed(pClass,"AllowDynamicProperties",` |
|        - |  2490 | `		sizeof("AllowDynamicProperties")-1);` |
|      175 |  2491 | `}` |
|        - |  2492 | `/*` |
|        - |  2493 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|        - |  2494 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|        - |  2495 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|        - |  2496 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|        - |  2497 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|        - |  2498 | ` */` |
|       80 |  2499 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|        5 |  2500 | `{` |
|      141 |  2501 | `	while( pClass ){` |
|       91 |  2502 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|        - |  2503 | `		sxu32 n;` |
|       91 |  2504 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|       30 |  2505 | `			if( aAttr[n].sName.nByte == nName` |
|       33 |  2506 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|       33 |  2507 | `				return TRUE;` |
|        - |  2508 | `			}` |
|      ! 0 |  2509 | `		}` |
|       61 |  2510 | `		pClass = pClass->pBase;` |
|        5 |  2511 | `	}` |
|       55 |  2512 | `	return FALSE;` |
|       45 |  2513 | `}` |
|        - |  2514 | `/*` |
|        - |  2515 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|        - |  2516 | ` * access or method call on such an instance is php's incomplete-object` |
|        - |  2517 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|        - |  2518 | ` * (array), get_object_vars) read its attribute table freely.` |
|        - |  2519 | ` */` |
|   283403 |  2520 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|        5 |  2521 | `{` |
|   283408 |  2522 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|        5 |  2523 | `}` |
|        - |  2524 | `/*` |
|        - |  2525 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|        - |  2526 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|        - |  2527 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|        - |  2528 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|        - |  2529 | ` * had one says "unknown", like php.` |
|        - |  2530 | ` */` |
|       44 |  2531 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|        1 |  2532 | `{` |
|       45 |  2533 | `	const char *zName = "unknown";` |
|       45 |  2534 | `	sxu32 nName = sizeof("unknown")-1;` |
|       45 |  2535 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|        - |  2536 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|       45 |  2537 | `	if( pEntry ){` |
|       45 |  2538 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       45 |  2539 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|       45 |  2540 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|       45 |  2541 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|       45 |  2542 | `			nName = SyBlobLength(&pVal->sBlob);` |
|       22 |  2543 | `		}` |
|       22 |  2544 | `	}` |
|       67 |  2545 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|        - |  2546 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|        - |  2547 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|       22 |  2548 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|       45 |  2549 | `}` |
|        - |  2550 | `/*` |
|        - |  2551 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|        - |  2552 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|        - |  2553 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|        - |  2554 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|        - |  2555 | ` * (property_exists() passes its name in pFuncName).` |
|        - |  2556 | ` */` |
|       16 |  2557 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|        1 |  2558 | `{` |
|        - |  2559 | `	SyBlob sMsg;` |
|       17 |  2560 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  2561 | `	if( pFuncName ){` |
|      ! 0 |  2562 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|      ! 0 |  2563 | `	}else{` |
|       17 |  2564 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|       17 |  2565 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|       17 |  2566 | `		if( pFunc == 0 ){` |
|      ! 0 |  2567 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|      ! 0 |  2568 | `		}else{` |
|       17 |  2569 | `			const char *zDisp = 0;` |
|        - |  2570 | `			int nDisp;` |
|       17 |  2571 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        3 |  2572 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|        3 |  2573 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|        3 |  2574 | `				SyBlobAppend(&sMsg,"::",2);` |
|        1 |  2575 | `			}` |
|       17 |  2576 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|       17 |  2577 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|        - |  2578 | `		}` |
|        - |  2579 | `	}` |
|       17 |  2580 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|       17 |  2581 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|       25 |  2582 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|       16 |  2583 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       17 |  2584 | `	SyBlobRelease(&sMsg);` |
|       17 |  2585 | `}` |
|        - |  2586 | `/*` |
|        - |  2587 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|        - |  2588 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|        - |  2589 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|        - |  2590 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|        - |  2591 | ` *` |
|        - |  2592 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|        - |  2593 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|        - |  2594 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|        - |  2595 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|        - |  2596 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|        - |  2597 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|        - |  2598 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|        - |  2599 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|        - |  2600 | ` */` |
|      638 |  2601 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|        5 |  2602 | `{` |
|        - |  2603 | `	ph7_class_attr *pAttr;` |
|      643 |  2604 | `	VmClassAttr *pVmAttr = 0;` |
|      643 |  2605 | `	ph7_value *pMemObj = 0;` |
|        - |  2606 | `	char *zCopy;` |
|        - |  2607 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|      643 |  2608 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|      643 |  2609 | `	if( pAttr == 0 ){` |
|      ! 0 |  2610 | `		return 0;` |
|        - |  2611 | `	}` |
|      643 |  2612 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      643 |  2613 | `	zCopy = (char *)&pAttr[1];` |
|      643 |  2614 | `	if( nName > 0 ){` |
|      631 |  2615 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|      313 |  2616 | `	}` |
|      643 |  2617 | `	zCopy[nName] = 0;` |
|      643 |  2618 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|      643 |  2619 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|      643 |  2620 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      643 |  2621 | `	pAttr->pDeclClass = pThis->pClass;` |
|        - |  2622 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|        - |  2623 | `	 * value, never a union. */` |
|      643 |  2624 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      643 |  2625 | `	if( pVmAttr == 0 ){` |
|      ! 0 |  2626 | `		goto fail_attr;` |
|        - |  2627 | `	}` |
|      643 |  2628 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      643 |  2629 | `	if( pMemObj == 0 ){` |
|      ! 0 |  2630 | `		goto fail_vmattr;` |
|        - |  2631 | `	}` |
|      643 |  2632 | `	pVmAttr->pAttr = pAttr;` |
|      643 |  2633 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|      643 |  2634 | `	pVmAttr->iState = 0;` |
|      643 |  2635 | `	PH7_VmAttrSetInst(pVmAttr,pThis);` |
|        - |  2636 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|        - |  2637 | `	 * property-creation order, matching PHP. */` |
|      643 |  2638 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|      ! 0 |  2639 | `		goto fail_slot;` |
|        - |  2640 | `	}` |
|        - |  2641 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|      643 |  2642 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |  2643 | ``	/* php walks the LIVE property table: a `foreach`/`array_walk` that has run off`` |
|        - |  2644 | `	 * the end re-arms onto a property the body just created. */` |
|      962 |  2645 | `	PH7_ClassInstanceAttrAppended(pThis,` |
|      638 |  2646 | `		SyHashGet(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)));` |
|      643 |  2647 | `	if( ppAttr ){` |
|      371 |  2648 | `		*ppAttr = pVmAttr;` |
|      183 |  2649 | `	}` |
|      643 |  2650 | `	return pMemObj;` |
|      ! 0 |  2651 | `fail_slot:` |
|      ! 0 |  2652 | `	VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|      ! 0 |  2653 | `fail_vmattr:` |
|      ! 0 |  2654 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2655 | `fail_attr:` |
|      ! 0 |  2656 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |  2657 | `	return 0;` |
|      324 |  2658 | `}` |
|        - |  2659 | `/*` |
|        - |  2660 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|        - |  2661 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|        - |  2662 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|        - |  2663 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|        - |  2664 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|        - |  2665 | ` */` |
|     7396 |  2666 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|        3 |  2667 | `{` |
|        - |  2668 | `	VmClassAttr *pVmAttr;` |
|        - |  2669 | `	ph7_value *pMemObj;` |
|        - |  2670 | `	/* php's storage name: a base's private goes back into the slot it came out` |
|        - |  2671 | `	 * of, beside (not over) a same-named property of the object's own class. */` |
|     7399 |  2672 | `	const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pAttr);` |
|     7399 |  2673 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     7399 |  2674 | `	if( pVmAttr == 0 ){` |
|      ! 0 |  2675 | `		return;` |
|        - |  2676 | `	}` |
|     7399 |  2677 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     7399 |  2678 | `	if( pMemObj == 0 ){` |
|      ! 0 |  2679 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2680 | `		return;` |
|        - |  2681 | `	}` |
|     7399 |  2682 | `	pVmAttr->pAttr = pAttr;` |
|     7399 |  2683 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|     7399 |  2684 | `	pVmAttr->iState = 0;` |
|     7399 |  2685 | `	PH7_VmAttrSetInst(pVmAttr,pThis);` |
|        - |  2686 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|        - |  2687 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|        - |  2688 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|        - |  2689 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|        - |  2690 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|     7399 |  2691 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     1135 |  2692 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      567 |  2693 | `	}` |
|        - |  2694 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|        - |  2695 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|        - |  2696 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|        - |  2697 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|     7399 |  2698 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey),pVmAttr) != SXRET_OK ){` |
|      ! 0 |  2699 | `		VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|      ! 0 |  2700 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2701 | `		return;` |
|        - |  2702 | `	}` |
|     7399 |  2703 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     7399 |  2704 | `	if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttr) != SXRET_OK ){` |
|      ! 0 |  2705 | `		SyHashDeleteEntry(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey),0);` |
|      ! 0 |  2706 | `		VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|      ! 0 |  2707 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  2708 | `		return;` |
|        - |  2709 | `	}` |
|        - |  2710 | `	/* Re-armed only once the entry is here to stay: the rollback above deletes it. */` |
|    11097 |  2711 | `	PH7_ClassInstanceAttrAppended(pThis,` |
|     7396 |  2712 | `		SyHashGet(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey)));` |
|     7399 |  2713 | `	if( ppAttr ){` |
|     7399 |  2714 | `		*ppAttr = pVmAttr;` |
|     3698 |  2715 | `	}` |
|     3701 |  2716 | `}` |
|        - |  2717 | `/* Forward declaration */` |
|        - |  2718 | `/*` |
|        - |  2719 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  2720 | ` */` |
|        - |  2721 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  2722 | `/*` |
|        - |  2723 | ` * Reserve a constant memory object.` |
|        - |  2724 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2725 | ` */` |
|  1970222 |  2726 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  2727 | `{` |
|        - |  2728 | `	ph7_value *pObj;` |
|        - |  2729 | `	sxi32 rc;` |
|  1970227 |  2730 | `	if( pIndex ){` |
|        - |  2731 | `		/* Object index in the object table */` |
|  1946452 |  2732 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   971479 |  2733 | `	}` |
|        - |  2734 | `	/* Reserve a slot for the new object */` |
|  1970227 |  2735 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|  1970227 |  2736 | `	if( rc != SXRET_OK ){` |
|        - |  2737 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  2738 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  2739 | `		 */` |
|      ! 0 |  2740 | `		return 0;` |
|        - |  2741 | `	}` |
|  1970227 |  2742 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|  1970227 |  2743 | `	return pObj;` |
|   983355 |  2744 | `}` |
|        - |  2745 | `/*` |
|        - |  2746 | ` * The segmented memory-object pool. See VmMemPool in ph7int.h.` |
|        - |  2747 | ` * A slot's ADDRESS never moves once it exists, which is the whole point: the` |
|        - |  2748 | ` * engine's standing "a pointer into aMemObj dangles across a reserve" hazard` |
|        - |  2749 | ` * (written down at pointers-die-across-a-user-callback) exists only because the` |
|        - |  2750 | ` * old table reallocated. Growth here appends a VM_MEMPOOL_SEG_SLOTS segment --` |
|        - |  2751 | ` * one allocation, no copy -- and a fully-free trailing segment is returned on` |
|        - |  2752 | ` * truncate, so a reused VM (-S server, in-process .phpt runner) hands the pool` |
|        - |  2753 | ` * back most of what a large run grew.` |
|        - |  2754 | ` */` |
|     7925 |  2755 | `PH7_PRIVATE sxi32 VmMemPoolInit(VmMemPool *pPool,SyMemBackend *pAllocator)` |
|        5 |  2756 | `{` |
|        - |  2757 | `	ph7_value *pSeg;` |
|     7930 |  2758 | `	SyZero(pPool,sizeof(VmMemPool));` |
|     7930 |  2759 | `	pPool->pAllocator = pAllocator;` |
|        - |  2760 | `	/* The first segment up front, mirroring the SySetAlloc(&pVm->aMemObj,0xFF)` |
|        - |  2761 | `	 * the pool replaced: an aMemObj exists the moment the VM does, and at 256` |
|        - |  2762 | `	 * slots it costs the same 16 KB that opening bid did. This allocation IS the` |
|        - |  2763 | `	 * per-VM floor -- see the segment-size note on VmMemPool in ph7int.h before` |
|        - |  2764 | `	 * raising VM_MEMPOOL_SEG_SHIFT. */` |
|     7930 |  2765 | `	pSeg = (ph7_value *)SyMemBackendAlloc(pAllocator,sizeof(ph7_value) * VM_MEMPOOL_SEG_SLOTS);` |
|     7930 |  2766 | `	if( pSeg == 0 ){` |
|      ! 0 |  2767 | `		return SXERR_MEM;` |
|        - |  2768 | `	}` |
|     7930 |  2769 | `	pPool->apSeg = (ph7_value **)SyMemBackendAlloc(pAllocator,sizeof(ph7_value *) * 16);` |
|     7930 |  2770 | `	if( pPool->apSeg == 0 ){` |
|      ! 0 |  2771 | `		SyMemBackendFree(pAllocator,pSeg);` |
|      ! 0 |  2772 | `		return SXERR_MEM;` |
|        - |  2773 | `	}` |
|     7930 |  2774 | `	pPool->apSeg[0] = pSeg;` |
|     7930 |  2775 | `	pPool->nSeg = 1;` |
|     7930 |  2776 | `	pPool->nCap = 16;` |
|     7930 |  2777 | `	pPool->nFreeHead = SXU32_HIGH; /* 0 is a valid slot; the empty-list mark cannot be it */` |
|     7930 |  2778 | `	return SXRET_OK;` |
|     3962 |  2779 | `}` |
|        - |  2780 | `/*` |
|        - |  2781 | ` * Return a freed slot to the pool's intrusive free list. The slot's value must` |
|        - |  2782 | ` * already have been RELEASED by the caller (the sites that push NULL-valued` |
|        - |  2783 | ` * freshly-reserved slots on error have nothing to release): the link is written` |
|        - |  2784 | ` * into the slot's own dead nIdx word, so a slot in the list must be a dead slot.` |
|        - |  2785 | ` * O(1), and zero memory beyond the pool's single head word.` |
|        - |  2786 | ` *` |
|        - |  2787 | ` * Freeing an index that is already on the list is a NO-OP, not a corruption:` |
|        - |  2788 | ` * see MEMOBJ_POOLFREE. That is the one behaviour the old aFreeObj stack had for` |
|        - |  2789 | ` * free and this list does not, so it is bought back explicitly.` |
|        - |  2790 | ` */` |
| 23133875 |  2791 | `PH7_PRIVATE void VmMemPoolFreeSlot(VmMemPool *pPool,sxu32 nIdx)` |
|        5 |  2792 | `{` |
| 23133880 |  2793 | `	ph7_value *pObj = PH7_MemObjAt(pPool,nIdx);` |
| 23133880 |  2794 | `	if( pObj == 0 ){` |
|      ! 0 |  2795 | `		return;   /* stale index -- the caller's own contract, and truncate's */` |
|        - |  2796 | `	}` |
| 23133880 |  2797 | `	if( pObj->iFlags & MEMOBJ_POOLFREE ){` |
|        - |  2798 | `		/* Already on the list. The old aFreeObj stack tolerated a double free by` |
|        - |  2799 | `		 * handing the index out twice and draining; this list would write the head` |
|        - |  2800 | `		 * into the slot the head already names, and every reserve after it would` |
|        - |  2801 | `		 * return that one slot forever. Refusing leaks nothing -- the slot stays` |
|        - |  2802 | `		 * exactly where it already is, on the list. */` |
|      ! 0 |  2803 | `		return;` |
|        - |  2804 | `	}` |
| 23133880 |  2805 | `	pObj->iFlags \|= MEMOBJ_POOLFREE;` |
| 23133880 |  2806 | `	pObj->nIdx = pPool->nFreeHead;` |
| 23133880 |  2807 | `	pPool->nFreeHead = nIdx;` |
| 11566357 |  2808 | `}` |
|        - |  2809 | `/*` |
|        - |  2810 | ` * Reserve a slot at the end of the pool. Returns the raw slot (uninitialized --` |
|        - |  2811 | ` * callers PH7_MemObjInit it, as they did the SySetPeek of the set this replaced)` |
|        - |  2812 | ` * and stores its index. Appending a slot never relocates an existing one.` |
|        - |  2813 | ` */` |
|  3116521 |  2814 | `PH7_PRIVATE ph7_value * VmMemPoolReserve(VmMemPool *pPool,sxu32 *pIndex)` |
|        5 |  2815 | `{` |
|        - |  2816 | `	sxu32 nIdx;` |
|        - |  2817 | `	sxu32 nSeg;` |
|  3116526 |  2818 | `	if( pPool->nUsed >= (pPool->nSeg << VM_MEMPOOL_SEG_SHIFT) ){` |
|        - |  2819 | `		ph7_value *pSeg;` |
|    11396 |  2820 | `		if( pPool->nSeg >= pPool->nCap ){` |
|        - |  2821 | `			/* The segment table itself doubles. It is a few hundred pointers at` |
|        - |  2822 | `			 * the engine's real peaks, so this is cheap and does not touch the` |
|        - |  2823 | `			 * values. */` |
|        - |  2824 | `			ph7_value **apNew;` |
|       47 |  2825 | `			sxu32 nNew = pPool->nCap ? pPool->nCap * 2 : 16;` |
|       47 |  2826 | `			apNew = (ph7_value **)SyMemBackendRealloc(pPool->pAllocator,pPool->apSeg,sizeof(ph7_value *) * nNew);` |
|       47 |  2827 | `			if( apNew == 0 ){` |
|      ! 0 |  2828 | `				return 0;` |
|        - |  2829 | `			}` |
|       47 |  2830 | `			pPool->apSeg = apNew;` |
|       47 |  2831 | `			pPool->nCap = nNew;` |
|       21 |  2832 | `		}` |
|    11396 |  2833 | `		pSeg = (ph7_value *)SyMemBackendAlloc(pPool->pAllocator,sizeof(ph7_value) * VM_MEMPOOL_SEG_SLOTS);` |
|    11396 |  2834 | `		if( pSeg == 0 ){` |
|      ! 0 |  2835 | `			return 0;` |
|        - |  2836 | `		}` |
|    11396 |  2837 | `		pPool->apSeg[pPool->nSeg++] = pSeg;` |
|     5694 |  2838 | `	}` |
|  3116526 |  2839 | `	nIdx = pPool->nUsed;` |
|  3116526 |  2840 | `	pPool->nUsed++;` |
|  3116526 |  2841 | `	nSeg = nIdx >> VM_MEMPOOL_SEG_SHIFT;` |
|  3116526 |  2842 | `	if( pIndex ){` |
|  3116526 |  2843 | `		*pIndex = nIdx;` |
|  1557418 |  2844 | `	}` |
|  3116526 |  2845 | `	return &pPool->apSeg[nSeg][nIdx & VM_MEMPOOL_SEG_MASK];` |
|  1557423 |  2846 | `}` |
|        - |  2847 | `/*` |
|        - |  2848 | ` * Shrink the pool's logical size. Fully-free trailing segments are RETURNED to` |
|        - |  2849 | ` * the allocator; the segment table itself keeps its capacity (a few hundred` |
|        - |  2850 | ` * pointers), so a reset does not realloc the table that describes the pool.` |
|        - |  2851 | ` */` |
|       16 |  2852 | `PH7_PRIVATE sxi32 VmMemPoolTruncate(VmMemPool *pPool,sxu32 nNewSize)` |
|      ! 0 |  2853 | `{` |
|        - |  2854 | `	sxu32 nSegNeed;` |
|        - |  2855 | `	sxu32 nGuard;` |
|        - |  2856 | `	sxu32 nCur;` |
|        - |  2857 | `	sxu32 n;` |
|        - |  2858 | `	/* Abandon the free list: its chain threads through slots that are about to be` |
|        - |  2859 | `	 * truncated away (and through segments about to be freed). The retained` |
|        - |  2860 | `	 * segments' free slots are simply forgotten -- they become fresh reserves,` |
|        - |  2861 | `	 * exactly as the SySetReset(&pVm->aFreeObj) this replaces emptied the old` |
|        - |  2862 | `	 * stack. Walk it FIRST, while nUsed still resolves every link, to take` |
|        - |  2863 | `	 * MEMOBJ_POOLFREE back off the slots that survive: the bit means "on the` |
|        - |  2864 | `	 * list", and a slot still wearing it after the list is gone would refuse the` |
|        - |  2865 | `	 * next legitimate free of that index. nGuard bounds the walk by the slot` |
|        - |  2866 | `	 * count so a chain corrupted from outside cannot spin here. */` |
|       16 |  2867 | `	nCur = pPool->nFreeHead;` |
|       36 |  2868 | `	for( nGuard = pPool->nUsed ; nGuard > 0 && nCur != SXU32_HIGH ; --nGuard ){` |
|       20 |  2869 | `		ph7_value *pFree = PH7_MemObjAt(pPool,nCur);` |
|       20 |  2870 | `		if( pFree == 0 ){` |
|      ! 0 |  2871 | `			break;` |
|        - |  2872 | `		}` |
|       20 |  2873 | `		pFree->iFlags &= ~MEMOBJ_POOLFREE;` |
|       20 |  2874 | `		nCur = pFree->nIdx;` |
|       10 |  2875 | `	}` |
|       16 |  2876 | `	pPool->nFreeHead = SXU32_HIGH;` |
|       16 |  2877 | `	if( nNewSize < pPool->nUsed ){` |
|       16 |  2878 | `		pPool->nUsed = nNewSize;` |
|        8 |  2879 | `	}` |
|        - |  2880 | `	/* Return every segment past the one that still holds a slot. nSeg is kept` |
|        - |  2881 | `	 * rounded UP to cover nUsed, so a slot index already handed out never stops` |
|        - |  2882 | `	 * resolving. */` |
|       16 |  2883 | `	nSegNeed = (pPool->nUsed + VM_MEMPOOL_SEG_SLOTS - 1) >> VM_MEMPOOL_SEG_SHIFT;` |
|       16 |  2884 | `	if( nSegNeed < pPool->nSeg ){` |
|       32 |  2885 | `		for( n = nSegNeed ; n < pPool->nSeg ; ++n ){` |
|       16 |  2886 | `			SyMemBackendFree(pPool->pAllocator,pPool->apSeg[n]);` |
|       16 |  2887 | `			pPool->apSeg[n] = 0;` |
|        8 |  2888 | `		}` |
|       16 |  2889 | `		pPool->nSeg = nSegNeed;` |
|        8 |  2890 | `	}` |
|       16 |  2891 | `	return SXRET_OK;` |
|      ! 0 |  2892 | `}` |
|        - |  2893 | `/*` |
|        - |  2894 | `` * Point an instance property at somebody else's value slot -- php's `$o->p =& $x`,`` |
|        - |  2895 | `` * and the same bind an `R:` back-reference asks for when unserialize() reads a`` |
|        - |  2896 | ` * payload whose two properties shared one reference.` |
|        - |  2897 | ` *` |
|        - |  2898 | ` * The property gives back whatever it was holding first: a slot it was already` |
|        - |  2899 | ` * bound to gets its pin returned (which frees it when this property was the last` |
|        - |  2900 | ` * holder), and its OWN slot is unset outright. Either way the typed-slot` |
|        - |  2901 | ` * enforcement entry goes with the old slot -- a reference-bound property bypasses` |
|        - |  2902 | ` * php's typed coercion -- and the new slot takes a counted pin so no frame` |
|        - |  2903 | ` * teardown recycles it underneath the property.` |
|        - |  2904 | ` */` |
|       58 |  2905 | `PH7_PRIVATE void PH7_VmBindAttrRef(ph7_vm *pVm,VmClassAttr *pVmAttr,sxu32 nSrcIdx)` |
|        1 |  2906 | `{` |
|       59 |  2907 | `	sxu32 nOldIdx = pVmAttr->nIdx;` |
|       59 |  2908 | `	if( nOldIdx == nSrcIdx ){` |
|      ! 0 |  2909 | `		return;` |
|        - |  2910 | `	}` |
|       59 |  2911 | `	if( pVmAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN) ){` |
|        - |  2912 | ``		/* A SOURCE (`$r =& $o->p; $o->p =& $y;`) still owns its declaration, so`` |
|        - |  2913 | `		 * its enforcement entry goes with the repoint. */` |
|        9 |  2914 | `		if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|        3 |  2915 | `			PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|        1 |  2916 | `		}` |
|        9 |  2917 | `		VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|        5 |  2918 | `	}else{` |
|       51 |  2919 | `		PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|       51 |  2920 | `		PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|        - |  2921 | `	}` |
|       59 |  2922 | `	pVmAttr->nIdx = nSrcIdx;` |
|       59 |  2923 | `	pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|       59 |  2924 | `	pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_REFSRCPIN);` |
|       59 |  2925 | `	VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|       30 |  2926 | `}` |
|        - |  2927 | `/*` |
|        - |  2928 | ` * Reserve a memory object.` |
|        - |  2929 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2930 | ` */` |
|  3116521 |  2931 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  2932 | `{` |
|        - |  2933 | `	ph7_value *pObj;` |
|  3116526 |  2934 | `	pObj = VmMemPoolReserve(&pVm->aMemObj,pIndex);` |
|  3116526 |  2935 | `	if( pObj == 0 ){` |
|      ! 0 |  2936 | `		return 0;` |
|        - |  2937 | `	}` |
|        - |  2938 | `	/* The slot this replaced came from a SySetPut of a zeroed filler, so a slot` |
|        - |  2939 | `	 * the caller leaves untouched (a static without an initializer, say) reads as` |
|        - |  2940 | `	 * a null value. Keep that: zero the fresh slot. */` |
|  3116526 |  2941 | `	SyZero(pObj,sizeof(ph7_value));` |
|  3116526 |  2942 | `	return pObj;` |
|  1557423 |  2943 | `}` |
|        - |  2944 | `/* Forward declaration */` |
|        - |  2945 | `/* Forward declarations for Fiber C functions */` |
|        - |  2946 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  2947 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  2948 | `/*` |
|        - |  2949 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  2950 | ` * directly as foreign functions.` |
|        - |  2951 | ` */` |
|        - |  2952 |  |
|        - |  2953 | `/*` |
|        - |  2954 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  2955 | ` * start compiling the target PHP program.` |
|        - |  2956 | ` */` |
|        - |  2957 | `/* Forward declaration */` |
|        - |  2958 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|     7925 |  2959 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  2960 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  2961 | `	 ph7 *pEngine /* Master engine */` |
|        - |  2962 | `	 )` |
|        5 |  2963 | `{` |
|        - |  2964 | `	ph7_value *pObj;` |
|        - |  2965 | `	sxi32 rc;` |
|        - |  2966 | `	/* Zero the structure */` |
|     7930 |  2967 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  2968 | `	/* Initialize VM fields */` |
|     7930 |  2969 | `	pVm->pEngine = &(*pEngine);` |
|     7930 |  2970 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|     7930 |  2971 | `	PH7_GcInit(&(*pVm));` |
|     7930 |  2972 | `	SySetInit(&pVm->aDeadClosure,&pVm->sAllocator,sizeof(ph7_vm_func *));` |
|        - |  2973 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|        - |  2974 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|        - |  2975 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|     7930 |  2976 | `	pVm->iDisplayErrors = PH7_DISPLAY_ERRORS_OFF;` |
|     7930 |  2977 | `	pVm->bLogErrors = 1;` |
|        - |  2978 | `	/* mbstring's substitute character, php's default (the internal encoding` |
|        - |  2979 | `	 * beside it is UTF-8, which is the zero the struct already holds) */` |
|     7930 |  2980 | `	pVm->iMbSubstitute = '?';` |
|     7930 |  2981 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  2982 | `	/* Instructions containers */` |
|     7930 |  2983 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     7930 |  2984 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     7930 |  2985 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  2986 | `	/* Object containers */` |
|     7930 |  2987 | `	rc = VmMemPoolInit(&pVm->aMemObj,&pVm->sAllocator);` |
|     7930 |  2988 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2989 | `		return rc;` |
|        - |  2990 | `	}` |
|        - |  2991 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|     7930 |  2992 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|     7930 |  2993 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|     7930 |  2994 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|     7930 |  2995 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|        - |  2996 | `	/* Virtual machine internal containers */` |
|     7930 |  2997 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     7930 |  2998 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     7930 |  2999 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|     7930 |  3000 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|        - |  3001 | `	/* The http:// wrapper's last response headers (see PH7_HttpPublishHeaders). */` |
|     7930 |  3002 | `	SyBlobInit(&pVm->sHttpRespHdrs,&pVm->sAllocator);` |
|     7930 |  3003 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     7930 |  3004 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|        - |  3005 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|        - |  3006 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|        - |  3007 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|        - |  3008 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|        - |  3009 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|        - |  3010 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|        - |  3011 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|        - |  3012 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|     7930 |  3013 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     7930 |  3014 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|        - |  3015 | `	/* 0 means "this call site has never been screened", so the first generation is 1. */` |
|     7930 |  3016 | `	pVm->nCallableGen = 1;` |
|     7930 |  3017 | `	pVm->nConstGen = 1; /* likewise for a PH7_OP_LOADC site (PH7_VmConstSiteAnswer) */` |
|     7930 |  3018 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     7930 |  3019 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     7930 |  3020 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     7930 |  3021 | `	SyZero(pVm->aSuperFirst,sizeof(pVm->aSuperFirst));` |
|     7930 |  3022 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     7930 |  3023 | `	SySetInit(&pVm->aCallSite,&pVm->sAllocator,sizeof(VmCallSite));` |
|     7930 |  3024 | `	SyHashInit(&pVm->hCallName,&pVm->sAllocator,0,0);` |
|     7930 |  3025 | `	pVm->nFreeCallSite = 0;` |
|     7930 |  3026 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     7930 |  3027 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     7930 |  3028 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|     7930 |  3029 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|     7930 |  3030 | `	SySetInit(&pVm->aPersistSock,&pVm->sAllocator,sizeof(VmPersistSock));` |
|     7930 |  3031 | `	pVm->bIniSeeded = 0;` |
|     7930 |  3032 | `	pVm->pGettext = 0;   /* ext/gettext binds its first domain lazily */` |
|     7930 |  3033 | `	pVm->pPcntl = 0;     /* ext/pcntl allocates its handler table on the first call */` |
|     7930 |  3034 | `	pVm->pSyslog = 0;    /* openlog() allocates the prefix it has to keep alive */` |
|     7930 |  3035 | `	pVm->iPosixErr = 0;  /* ext/posix has seen no failure yet */` |
|     7930 |  3036 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|     7930 |  3037 | `	PH7_MemObjInit(&(*pVm),&pVm->sSessHandler);` |
|     7930 |  3038 | `	SyBlobInit(&pVm->sSessData,&pVm->sAllocator);` |
|     7930 |  3039 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|     7930 |  3040 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|     7930 |  3041 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|     7930 |  3042 | `	SyBlobInit(&pVm->sErrLogPath,&pVm->sAllocator);` |
|     7930 |  3043 | `	SyBlobInit(&pVm->sOutStartFile,&pVm->sAllocator);` |
|     7930 |  3044 | `	SyBlobInit(&pVm->sSessStartFile,&pVm->sAllocator);` |
|     7930 |  3045 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|     7930 |  3046 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     7930 |  3047 | `	SyBlobInit(&pVm->sAutoloadExt,&pVm->sAllocator);` |
|     7930 |  3048 | `	SyBlobAppend(&pVm->sAutoloadExt,PH7_SPL_AUTOLOAD_EXT,sizeof(PH7_SPL_AUTOLOAD_EXT)-1);` |
|     7930 |  3049 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     7930 |  3050 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|     7930 |  3051 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|     7930 |  3052 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     7930 |  3053 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|     7930 |  3054 | `	pVm->nResourceIdNext = 1;` |
|     7930 |  3055 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     7930 |  3056 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|     7930 |  3057 | `	pVm->pMagicSetThis = 0;` |
|     7930 |  3058 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|     7930 |  3059 | `	pVm->pHookSetThis = 0;` |
|     7930 |  3060 | `	pVm->pHookSetAttr = 0;` |
|     7930 |  3061 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     7930 |  3062 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|     7930 |  3063 | `	pVm->pMagicCallThis = 0;` |
|     7930 |  3064 | `	pVm->pMagicCallClass = 0;` |
|     7930 |  3065 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|     7930 |  3066 | `	pVm->pIdleCallFrames = 0;` |
|     7930 |  3067 | `	SyZero(pVm->apIdleOperandStack,sizeof(pVm->apIdleOperandStack));` |
|     7930 |  3068 | `	pVm->nIdleOperandStacks = 0;` |
|     7930 |  3069 | `	pVm->nIdleOperandSlots = 0;` |
|     7930 |  3070 | `	pVm->pIdleStackNodes = 0;` |
|     7930 |  3071 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|     7930 |  3072 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|     7930 |  3073 | `	pVm->pPendingException = 0;` |
|     7930 |  3074 | `	pVm->pInflightException = 0;` |
|     7930 |  3075 | `	pVm->nInflightExcBase = 0;` |
|     7930 |  3076 | `	VmClearResumeTarget(&(*pVm));` |
|     7930 |  3077 | `	pVm->nBoundaryRc = 0;` |
|     7930 |  3078 | `	PH7_CmpRefusalClear(&(*pVm));` |
|     7930 |  3079 | `	pVm->pConstEvalClass = 0;` |
|     7930 |  3080 | `	pVm->nConstEvalDepth = 0;` |
|     7930 |  3081 | `	pVm->pConstCycleAttr = 0;` |
|     7930 |  3082 | `	pVm->pConstCycleClass = 0;` |
|     7930 |  3083 | `	SySetReset(&pVm->aMagicGuard);` |
|     7930 |  3084 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 |  3085 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 |  3086 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 |  3087 | `	}` |
|     7930 |  3088 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|     7930 |  3089 | `	if( pVm->pHookSetThis ){` |
|      ! 0 |  3090 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 |  3091 | `		pVm->pHookSetThis = 0;` |
|      ! 0 |  3092 | `	}` |
|     7930 |  3093 | `	pVm->pHookSetAttr = 0;` |
|     7930 |  3094 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|     7930 |  3095 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 |  3096 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 |  3097 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 |  3098 | `	}` |
|     7930 |  3099 | `	pVm->pMagicCallClass = 0;` |
|     7930 |  3100 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        - |  3101 | `	/* Configuration containers */` |
|     7930 |  3102 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     7930 |  3103 | `	SySetInit(&pVm->aIncFrame,&pVm->sAllocator,sizeof(VmIncFrame));` |
|     7930 |  3104 | `	SyBlobInit(&pVm->sReflectConstName,&pVm->sAllocator);` |
|     7930 |  3105 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     7930 |  3106 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     7930 |  3107 | `	SySetInit(&pVm->aEvalFile,&pVm->sAllocator,sizeof(SyString));` |
|     7930 |  3108 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     7930 |  3109 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|        - |  3110 | `	/* 0 is php's "no code set": a CLI run reads FALSE until something sets` |
|        - |  3111 | `	 * one, and a request-driven run is put at 200 when the request arrives. */` |
|     7930 |  3112 | `	pVm->iResponseStatus = 0;` |
|     7930 |  3113 | `	pVm->bHeadersSent = 0;` |
|     7930 |  3114 | `	SyBlobReset(&pVm->sOutStartFile);` |
|     7930 |  3115 | `	pVm->nOutStartLine = 0;` |
|     7930 |  3116 | `	SyBlobReset(&pVm->sSessStartFile);` |
|     7930 |  3117 | `	pVm->nSessStartLine = 0;` |
|     7930 |  3118 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|     7930 |  3119 | `	SySetInit(&pVm->aSuppressedIo,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  3120 | `	/* Error callbacks containers */` |
|     7930 |  3121 | `	PH7_MemObjInit(&(*pVm),&pVm->sExceptionCB);` |
|     7930 |  3122 | `	PH7_MemObjInit(&(*pVm),&pVm->sErrCB);` |
|     7930 |  3123 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|     7930 |  3124 | `	SySetInit(&pVm->aExceptionCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|     7930 |  3125 | `	SySetInit(&pVm->aErrCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|     7930 |  3126 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  3127 | `	/* Recursion policy. PHP call depth is heap-bound since` |
|        - |  3128 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|        - |  3129 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|        - |  3130 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|        - |  3131 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|        - |  3132 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|        - |  3133 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     7930 |  3134 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|     7930 |  3135 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|        - |  3136 | `	                             * usort-in-comparator path overflows at 1024) */` |
|        - |  3137 | `#else` |
|        - |  3138 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|        - |  3139 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|        - |  3140 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|        - |  3141 | `	 * at max depth). The embedder tunes both via config verbs. */` |
|        - |  3142 | `	pVm->nMaxDepth = 512;` |
|        - |  3143 | `	pVm->nMaxNativeDepth = 16;` |
|        - |  3144 | `#endif` |
|        - |  3145 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|        - |  3146 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|        - |  3147 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|     7930 |  3148 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|        - |  3149 | `	/* JSON return status */` |
|     7930 |  3150 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  3151 | `	/* PRNG context */` |
|     7930 |  3152 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  3153 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|        - |  3154 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|     7930 |  3155 | `	pVm->mtSeeded = FALSE;` |
|        - |  3156 | `	/* Install the null constant */` |
|     7930 |  3157 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     7930 |  3158 | `	if( pObj == 0 ){` |
|      ! 0 |  3159 | `		rc = SXERR_MEM;` |
|      ! 0 |  3160 | `		goto Err;` |
|        - |  3161 | `	}` |
|     7930 |  3162 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  3163 | `	/* Install the boolean TRUE constant */` |
|     7930 |  3164 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     7930 |  3165 | `	if( pObj == 0 ){` |
|      ! 0 |  3166 | `		rc = SXERR_MEM;` |
|      ! 0 |  3167 | `		goto Err;` |
|        - |  3168 | `	}` |
|     7930 |  3169 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  3170 | `	/* Install the boolean FALSE constant */` |
|     7930 |  3171 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     7930 |  3172 | `	if( pObj == 0 ){` |
|      ! 0 |  3173 | `		rc = SXERR_MEM;` |
|      ! 0 |  3174 | `		goto Err;` |
|        - |  3175 | `	}` |
|     7930 |  3176 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  3177 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  3178 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  3179 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     7930 |  3180 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     7930 |  3181 | `	if( pObj == 0 ){` |
|      ! 0 |  3182 | `		rc = SXERR_MEM;` |
|      ! 0 |  3183 | `		goto Err;` |
|        - |  3184 | `	}` |
|     7930 |  3185 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  3186 | `	/* Allocate the reference table. It belongs to VM INIT rather than to` |
|        - |  3187 | `	 * PH7_VmMakeReady because the COMPILER now runs bytecode of its own: the` |
|        - |  3188 | `	 * constant-expression evaluation behind a declaration message` |
|        - |  3189 | `	 * (PH7_VmEvalConstExpr) builds the array a parameter defaults to, and every` |
|        - |  3190 | `	 * hashmap insert installs a reference-table entry. (While the table was a` |
|        - |  3191 | `` 	 * HASH, an unallocated one made that lookup index `apRefObj[hash & (0 - 1)]` `` |
|        - |  3192 | `	 * and segfaulted the compiler; the slot-indexed table answers "no record"` |
|        - |  3193 | `	 * for an out-of-range index instead, so this is now a head start rather than` |
|        - |  3194 | `	 * the thing standing between the compiler and a crash.) */` |
|     7930 |  3195 | `	pVm->nRefSize = 0x10;` |
|     7930 |  3196 | `	pVm->apRefObj = (void **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(void *) * pVm->nRefSize);` |
|     7930 |  3197 | `	if( pVm->apRefObj == 0 ){` |
|      ! 0 |  3198 | `		rc = SXERR_MEM;` |
|      ! 0 |  3199 | `		goto Err;` |
|        - |  3200 | `	}` |
|        - |  3201 | `	/* Zero the reference table */` |
|     7930 |  3202 | `	SyZero(pVm->apRefObj,sizeof(void *) * pVm->nRefSize);` |
|        - |  3203 | `	/* Create the global frame */` |
|     7930 |  3204 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     7930 |  3205 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3206 | `		goto Err;` |
|        - |  3207 | `	}` |
|        - |  3208 | `	/* Initialize the code generator */` |
|     7930 |  3209 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     7930 |  3210 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3211 | `		goto Err;` |
|        - |  3212 | `	}` |
|        - |  3213 | `	/* VM correctly initialized,set the magic number */` |
|     7930 |  3214 | `	pVm->nMagic = PH7_VM_INIT;` |
|        - |  3215 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|        - |  3216 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|     7930 |  3217 | `	pVm->bCompilingBuiltin = 1;` |
|        - |  3218 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|     7930 |  3219 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|        - |  3220 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|        - |  3221 | `	 * compiled — its classes are internal too. */` |
|        - |  3222 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     7930 |  3223 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     7930 |  3224 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     7930 |  3225 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     7930 |  3226 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     7930 |  3227 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - |  3228 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     7930 |  3229 | `	pVm->pCoalesceObj = 0;` |
|     7930 |  3230 | `	pVm->bCoalesceArmed = 0;` |
|     7930 |  3231 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  3232 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|        - |  3233 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|        - |  3234 | ``	 * segfaults the first `new Fiber`. */`` |
|     7930 |  3235 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|     7930 |  3236 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  3237 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|        - |  3238 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|        - |  3239 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|        - |  3240 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|        - |  3241 | `	 * rather than being stamped on afterwards. */` |
|     7930 |  3242 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|     7930 |  3243 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|     7930 |  3244 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|     7930 |  3245 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|        - |  3246 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|     7930 |  3247 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        - |  3248 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|     7930 |  3249 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|        - |  3250 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|        - |  3251 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|        - |  3252 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|        - |  3253 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|        - |  3254 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|     7930 |  3255 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|     7930 |  3256 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|        - |  3257 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|        - |  3258 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|        - |  3259 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|     7930 |  3260 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|        - |  3261 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|        - |  3262 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|        - |  3263 | `	 * internal; the Traversable pointer above must already be cached. */` |
|     7930 |  3264 | `	PH7_VmInstallReflection(&(*pVm));` |
|     7930 |  3265 | `	PH7_VmInstallDateTime(&(*pVm));` |
|     7930 |  3266 | `	PH7_VmInstallSpl(&(*pVm));` |
|        - |  3267 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     7930 |  3268 | `	PH7_VmInstallHashContext(&(*pVm));` |
|        - |  3269 | `#endif` |
|        - |  3270 | `#ifndef PH7_DISABLE_DISK_IO` |
|        - |  3271 | `	/* php_user_filter and StreamBucket: the classes stream_filter_register()` |
|        - |  3272 | `	 * builds its filters out of. */` |
|     7930 |  3273 | `	PH7_VmInstallStreamFilter(&(*pVm));` |
|        - |  3274 | `#endif` |
|     7930 |  3275 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|        - |  3276 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  3277 | `	/* php 8.4's RoundingMode, round()'s declared third argument. It rides the` |
|        - |  3278 | `	 * builtin guard because round() -- and bcround() -- do: a build with no` |
|        - |  3279 | `	 * consumer for the symbol does not ship the symbol. */` |
|     7930 |  3280 | `	PH7_VmInstallRoundingMode(&(*pVm));` |
|        - |  3281 | `	/* Pcntl\QosClass: php registers this pure enum on every platform, even the` |
|        - |  3282 | `	 * ones whose build has no function that reads it. */` |
|     7930 |  3283 | `	PH7_VmInstallPcntl(&(*pVm));` |
|        - |  3284 | `	/* BcMath\Number: after RoundingMode, whose cases its round() reads. */` |
|     7930 |  3285 | `	PH7_VmInstallBcMath(&(*pVm));` |
|        - |  3286 | `	/* php's ext/random object surface. It rides the builtin guard for the same` |
|        - |  3287 | `	 * reason bcmath does: the tiny build ships no consumer for it. */` |
|     7930 |  3288 | `	PH7_VmInstallRandom(&(*pVm));` |
|        - |  3289 | `	/* php's ext/fileinfo: the finfo class. It rides the builtin guard with the` |
|        - |  3290 | `	 * two above -- the tiny build ships none of its six functions. */` |
|     7930 |  3291 | `	PH7_VmInstallFileinfo(&(*pVm));` |
|        - |  3292 | `	/* ext/phar stands on SPL's directory iterators (a Phar IS one) and on` |
|        - |  3293 | `	 * ext/zlib for a compressed entry, so it mounts after both. */` |
|     7930 |  3294 | `	PH7_VmInstallPhar(&(*pVm));` |
|        - |  3295 | `#endif` |
|     7930 |  3296 | `	PH7_VmInstallSession(&(*pVm));` |
|     7930 |  3297 | `	PH7_VmInstallIni(&(*pVm));` |
|        - |  3298 | `#ifdef PH7_ENABLE_LIBXML` |
|        - |  3299 | `	/* libxml2-backed surfaces: shared plumbing first, then the ext/xml push` |
|        - |  3300 | `	 * parser and the DOM and XMLWriter class libraries that build on it. */` |
|     7930 |  3301 | `	PH7_VmInstallLibxml(&(*pVm));` |
|     7930 |  3302 | `	PH7_VmInstallXml(&(*pVm));` |
|     7930 |  3303 | `	PH7_VmInstallDom(&(*pVm));` |
|     7930 |  3304 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|        - |  3305 | `	/* ext/simplexml stands on ext/dom's document shells and hands nodes back to` |
|        - |  3306 | `	 * it (dom_import_simplexml), so it mounts after DOMDocument exists. */` |
|     7930 |  3307 | `	PH7_VmInstallSimpleXml(&(*pVm));` |
|        - |  3308 | `#endif` |
|        - |  3309 | `#ifdef PH7_ENABLE_SQLITE` |
|        - |  3310 | ``	/* ext/pdo's class library first: `Pdo\Sqlite` extends PDO, so the driver's`` |
|        - |  3311 | `	 * installer needs the parent already mounted. */` |
|     7930 |  3312 | `	PH7_VmInstallPdo(&(*pVm));` |
|     7930 |  3313 | `	PH7_VmInstallPdoSqlite(&(*pVm));` |
|        - |  3314 | `	/* ext/sqlite3: php's other sqlite surface, independent of both. */` |
|     7930 |  3315 | `	PH7_VmInstallSqlite3(&(*pVm));` |
|        - |  3316 | `#endif` |
|        - |  3317 | `#ifdef PH7_ENABLE_CURL` |
|        - |  3318 | `	/* ext/curl: the libcurl binding. */` |
|     7930 |  3319 | `	PH7_VmInstallCurl(&(*pVm));` |
|        - |  3320 | `#endif` |
|        - |  3321 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|        - |  3322 | `	/* ext/sockets: the Socket/AddressInfo handle classes and php's BSD socket` |
|        - |  3323 | `	 * API over the descriptors net.c already drives for the stream wrappers. */` |
|        - |  3324 | `	{` |
|        - |  3325 | `		const ph7_builtin_func *aSock;` |
|     7930 |  3326 | `		sxu32 nSock = 0,n;` |
|     7930 |  3327 | `		PH7_VmInstallSockets(&(*pVm));` |
|     7930 |  3328 | `		aSock = PH7_SocketsFuncTable(&nSock);` |
|   301155 |  3329 | `		for( n = 0 ; n < nSock ; ++n ){` |
|   293230 |  3330 | `			ph7_create_function(&(*pVm),aSock[n].zName,aSock[n].xFunc,pVm);` |
|   146414 |  3331 | `		}` |
|        - |  3332 | `	}` |
|        - |  3333 | `#endif` |
|        - |  3334 | `#ifdef PH7_ENABLE_OPENSSL` |
|        - |  3335 | `	/* ext/openssl: the three opaque handle classes and the extension's own` |
|        - |  3336 | `	 * functions, in two units -- the library-wide/cipher half and the` |
|        - |  3337 | `	 * certificate half. */` |
|        - |  3338 | `	{` |
|        - |  3339 | `		const ph7_builtin_func *aSsl;` |
|     7930 |  3340 | `		sxu32 nSsl = 0,n;` |
|     7930 |  3341 | `		PH7_VmInstallOpenSsl(&(*pVm));` |
|     7930 |  3342 | `		aSsl = PH7_OpenSslFuncTable(&nSsl);` |
|   277380 |  3343 | `		for( n = 0 ; n < nSsl ; ++n ){` |
|   269455 |  3344 | `			ph7_create_function(&(*pVm),aSsl[n].zName,aSsl[n].xFunc,pVm);` |
|   134543 |  3345 | `		}` |
|     7930 |  3346 | `		aSsl = PH7_OpenSslX509FuncTable(&nSsl);` |
|   221905 |  3347 | `		for( n = 0 ; n < nSsl ; ++n ){` |
|   213980 |  3348 | `			ph7_create_function(&(*pVm),aSsl[n].zName,aSsl[n].xFunc,pVm);` |
|   106844 |  3349 | `		}` |
|        - |  3350 | `	}` |
|        - |  3351 | `#endif` |
|        - |  3352 | `#ifdef PH7_ENABLE_ZLIB` |
|        - |  3353 | `	/* ext/zlib: the two context classes and the extension's own functions.` |
|        - |  3354 | `	 * Its gz* handle verbs are aliases registered beside the stream functions` |
|        - |  3355 | `	 * they are (vfs.c), and its device beside the other wrappers. */` |
|        - |  3356 | `	{` |
|        - |  3357 | `		const ph7_builtin_func *aZlib;` |
|     7930 |  3358 | `		sxu32 nZlib = 0,n;` |
|     7930 |  3359 | `		PH7_VmInstallZlib(&(*pVm));` |
|     7930 |  3360 | `		aZlib = PH7_ZlibFuncTable(&nZlib);` |
|   158505 |  3361 | `		for( n = 0 ; n < nZlib ; ++n ){` |
|   150580 |  3362 | `			ph7_create_function(&(*pVm),aZlib[n].zName,aZlib[n].xFunc,pVm);` |
|    75188 |  3363 | `		}` |
|        - |  3364 | `	}` |
|        - |  3365 | `	/* ext/zip: the ZipArchive class and the ten deprecated procedural verbs.` |
|        - |  3366 | `	 * It stands ON ext/zlib -- a deflated member is the format's normal case,` |
|        - |  3367 | `	 * and php's own build requires the library for the same reason -- so it` |
|        - |  3368 | `	 * mounts inside that guard and after it. */` |
|        - |  3369 | `	{` |
|        - |  3370 | `		const ph7_builtin_func *aZip;` |
|     7930 |  3371 | `		sxu32 nZip = 0,n;` |
|     7930 |  3372 | `		PH7_VmInstallZip(&(*pVm));` |
|     7930 |  3373 | `		aZip = PH7_ZipFuncTable(&nZip);` |
|    87180 |  3374 | `		for( n = 0 ; n < nZip ; ++n ){` |
|    79255 |  3375 | `			ph7_create_function(&(*pVm),aZip[n].zName,aZip[n].xFunc,pVm);` |
|    39575 |  3376 | `		}` |
|        - |  3377 | `	}` |
|        - |  3378 | `#endif` |
|        - |  3379 | `	/* Register the C builtins, LAST so that the order every name was inserted in` |
|        - |  3380 | `	 * is the order it was inserted in when these ran from PH7_VmMakeReady -- a hash` |
|        - |  3381 | `	 * walk is reverse-insertion, and get_defined_functions() and Reflection's` |
|        - |  3382 | `	 * extension surfaces both read one.` |
|        - |  3383 | `	 *` |
|        - |  3384 | `	 * They run HERE, and not after compilation as they used to, because the` |
|        - |  3385 | `	 * declaration guard has to be able to see them: a program is compiled between` |
|        - |  3386 | ``	 * VM init and PH7_VmMakeReady, and `function strlen(){}` in it silently WON`` |
|        - |  3387 | `	 * while the ~650 core names arrived too late to be found. php has its whole` |
|        - |  3388 | `	 * internal function table before it compiles anything, and now so does this.` |
|        - |  3389 | `	 * Nothing else the compiler does reads hHostFunction.` |
|        - |  3390 | `	 *` |
|        - |  3391 | `	 * What still belongs to PH7_VmMakeReady is everything STAMPED on top of these` |
|        - |  3392 | `	 * registrations -- arity, signatures, the deprecation marks and the` |
|        - |  3393 | `	 * language-construct mark -- which needs every extension's own pass to have run` |
|        - |  3394 | `	 * first, and the constants, which a compile-time constant expression resolves` |
|        - |  3395 | `	 * through its own table. */` |
|     7930 |  3396 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     7930 |  3397 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3398 | `		goto Err;` |
|        - |  3399 | `	}` |
|     7930 |  3400 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|     7930 |  3401 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  3402 | `#ifdef PH7_ENABLE_PCRE` |
|     7930 |  3403 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|        - |  3404 | `#endif` |
|     7930 |  3405 | `	pVm->bCompilingBuiltin = 0;` |
|        - |  3406 | `	/* Reset the code generator */` |
|     7930 |  3407 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     7930 |  3408 | `	return SXRET_OK;` |
|      ! 0 |  3409 | `Err:` |
|      ! 0 |  3410 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  3411 | `	return rc;` |
|     3962 |  3412 | `}` |
|        - |  3413 | `/*` |
|        - |  3414 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  3415 | ` * routine which store the output in an internal blob.` |
|        - |  3416 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  3417 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  3418 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  3419 | ` * Refer to the official docurmentation for additional information.` |
|        - |  3420 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  3421 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  3422 | ` * to finish executing and extracting the output.` |
|        - |  3423 | ` */` |
|      352 |  3424 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  3425 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  3426 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  3427 | `	void *pUserData     /* User private data */` |
|        - |  3428 | `	)` |
|        4 |  3429 | `{` |
|        - |  3430 | `	 sxi32 rc;` |
|        - |  3431 | `	 /* Store the output in an internal BLOB */` |
|      356 |  3432 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      356 |  3433 | `	 return rc;` |
|        4 |  3434 | `}` |
|        - |  3435 | `/*` |
|        - |  3436 | ` * WHERE the response body began -- the file and the line php names in every` |
|        - |  3437 | ` * headers-already-sent diagnostic and hands back through headers_sent()'s two` |
|        - |  3438 | ` * by-ref out-params. Answers 0 while nothing has been emitted, which is php's` |
|        - |  3439 | ` * "" and 0 rather than a missing answer.` |
|        - |  3440 | ` */` |
|       30 |  3441 | `PH7_PRIVATE int PH7_VmOutputOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine)` |
|        4 |  3442 | `{` |
|       34 |  3443 | `	if( pFile ){` |
|       34 |  3444 | `		SyStringInitFromBuf(pFile,(const char *)SyBlobData(&pVm->sOutStartFile),` |
|        - |  3445 | `			SyBlobLength(&pVm->sOutStartFile));` |
|       15 |  3446 | `	}` |
|       34 |  3447 | `	if( pnLine ){` |
|       34 |  3448 | `		*pnLine = pVm->nOutStartLine;` |
|       15 |  3449 | `	}` |
|       34 |  3450 | `	return SyBlobLength(&pVm->sOutStartFile) > 0;` |
|        4 |  3451 | `}` |
|        - |  3452 | `/*` |
|        - |  3453 | ` * WHERE the active session was started, which is the other half php names --` |
|        - |  3454 | ` * a session directive refused because a session is ACTIVE points at the` |
|        - |  3455 | ` * session_start() that opened it.` |
|        - |  3456 | ` */` |
|       12 |  3457 | `PH7_PRIVATE int PH7_VmSessionOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine)` |
|        2 |  3458 | `{` |
|       14 |  3459 | `	if( pFile ){` |
|       14 |  3460 | `		SyStringInitFromBuf(pFile,(const char *)SyBlobData(&pVm->sSessStartFile),` |
|        - |  3461 | `			SyBlobLength(&pVm->sSessStartFile));` |
|        6 |  3462 | `	}` |
|       14 |  3463 | `	if( pnLine ){` |
|       14 |  3464 | `		*pnLine = pVm->nSessStartLine;` |
|        6 |  3465 | `	}` |
|       14 |  3466 | `	return SyBlobLength(&pVm->sSessStartFile) > 0;` |
|        2 |  3467 | `}` |
|        - |  3468 | `/*` |
|        - |  3469 | ` * php's provenance clause, appended to a session refusal: a session that is` |
|        - |  3470 | ` * ACTIVE points at the session_start() that opened it, and a response that has` |
|        - |  3471 | ` * already begun points at the output. Appends nothing when the place is not` |
|        - |  3472 | ` * known, which is the message php prints for a session no script started.` |
|        - |  3473 | ` */` |
|       18 |  3474 | `PH7_PRIVATE void PH7_VmAppendWhere(ph7_vm *pVm,SyBlob *pMsg,int bSessionActive)` |
|        3 |  3475 | `{` |
|        - |  3476 | `	SyString sFile;` |
|       21 |  3477 | `	sxu32 nLine = 0;` |
|        - |  3478 | `	char zTail[64];` |
|       18 |  3479 | `	int bHave = bSessionActive ? PH7_VmSessionOrigin(pVm,&sFile,&nLine)` |
|       12 |  3480 | `	                           : PH7_VmOutputOrigin(pVm,&sFile,&nLine);` |
|       21 |  3481 | `	if( !bHave ){` |
|      ! 0 |  3482 | `		return;` |
|        - |  3483 | `	}` |
|        - |  3484 | `	{` |
|       21 |  3485 | `		const char *zOpen = bSessionActive ? " (started from " : " (sent from ";` |
|       21 |  3486 | `		SyBlobAppend(pMsg,zOpen,(sxu32)SyStrlen(zOpen));` |
|        - |  3487 | `	}` |
|       21 |  3488 | `	SyBlobAppend(pMsg,sFile.zString,sFile.nByte);` |
|       21 |  3489 | `	SyBufferFormat(zTail,sizeof(zTail)," on line %u)",nLine);` |
|       21 |  3490 | `	SyBlobAppend(pMsg,zTail,(sxu32)SyStrlen(zTail));` |
|       12 |  3491 | `}` |
|        - |  3492 | `/*` |
|        - |  3493 | ` * Record where the session now going ACTIVE was started.` |
|        - |  3494 | ` */` |
|       96 |  3495 | `PH7_PRIVATE void PH7_VmSetSessionOrigin(ph7_vm *pVm)` |
|        4 |  3496 | `{` |
|      100 |  3497 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      100 |  3498 | `	SyBlobReset(&pVm->sSessStartFile);` |
|      100 |  3499 | `	if( pFile && pFile->nByte > 0 ){` |
|      100 |  3500 | `		SyBlobAppend(&pVm->sSessStartFile,pFile->zString,pFile->nByte);` |
|       48 |  3501 | `	}` |
|      100 |  3502 | `	pVm->nSessStartLine = pVm->nCurLine;` |
|      100 |  3503 | `}` |
|        - |  3504 | `/*` |
|        - |  3505 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  3506 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  3507 | ` */` |
|   311176 |  3508 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 |  3509 | `{` |
|   311181 |  3510 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|   311181 |  3511 | `	if( xCons != VmObConsumer ){` |
|   101669 |  3512 | `		pVm->nOutputLen += nLen;` |
|   101669 |  3513 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|        - |  3514 | `			/* The ORIGIN of the output is recorded with the flag, once: php's` |
|        - |  3515 | `			 * four headers-sent diagnostics all name the place the response` |
|        - |  3516 | `			 * body began, and headers_sent() hands the same pair back through` |
|        - |  3517 | `			 * its two by-ref out-params. */` |
|     2835 |  3518 | `			SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     2835 |  3519 | `			pVm->bHeadersSent = 1;` |
|     2835 |  3520 | `			SyBlobReset(&pVm->sOutStartFile);` |
|     2835 |  3521 | `			if( pFile && pFile->nByte > 0 ){` |
|     2835 |  3522 | `				SyBlobAppend(&pVm->sOutStartFile,pFile->zString,pFile->nByte);` |
|     1415 |  3523 | `			}` |
|     2835 |  3524 | `			pVm->nOutStartLine = pVm->nCurLine;` |
|     1415 |  3525 | `		}` |
|    49867 |  3526 | `	}` |
|   311181 |  3527 | `}` |
|        - |  3528 | `/*` |
|        - |  3529 | ` * Static operand-stack depth analysis.` |
|        - |  3530 | ` *` |
|        - |  3531 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|        - |  3532 | ` * (no instruction pushes more than one net slot), and that is what` |
|        - |  3533 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|        - |  3534 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|        - |  3535 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|        - |  3536 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|        - |  3537 | ` *` |
|        - |  3538 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|        - |  3539 | ` * conservative BY CONSTRUCTION:` |
|        - |  3540 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|        - |  3541 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|        - |  3542 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|        - |  3543 | ` *     pop — makes height go negative, which triggers fallback.` |
|        - |  3544 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|        - |  3545 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|        - |  3546 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|        - |  3547 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|        - |  3548 | ` *     bound. There is no partial/unsafe middle.` |
|        - |  3549 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|        - |  3550 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|        - |  3551 | ` *     instruction-count bound -> fallback.` |
|        - |  3552 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|        - |  3553 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|        - |  3554 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|        - |  3555 | ` *` |
|        - |  3556 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|        - |  3557 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|        - |  3558 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|        - |  3559 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|        - |  3560 | ` *` |
|        - |  3561 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|        - |  3562 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|        - |  3563 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|        - |  3564 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|        - |  3565 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|        - |  3566 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|        - |  3567 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|        - |  3568 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|        - |  3569 | ` * entry here.` |
|        - |  3570 | ` */` |
|        - |  3571 | `/*` |
|        - |  3572 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|        - |  3573 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|        - |  3574 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|        - |  3575 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|        - |  3576 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|        - |  3577 | ` */` |
|    83834 |  3578 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|        5 |  3579 | `{` |
|    83839 |  3580 | `	int push = 0, n = 0;` |
|        - |  3581 | `	sxi32 d;` |
|    83839 |  3582 | `	switch( pI->iOp ){` |
|        - |  3583 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|        - |  3584 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|    15944 |  3585 | `	case PH7_OP_LOADC:` |
|        - |  3586 | `	case PH7_OP_DUP:` |
|        - |  3587 | `	case PH7_OP_PICK:` |
|    31596 |  3588 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|     6116 |  3589 | `	case PH7_OP_LOAD:` |
|    12209 |  3590 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|    12209 |  3591 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|       10 |  3592 | `	case PH7_OP_LOAD_REF:` |
|       21 |  3593 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - |  3594 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|      676 |  3595 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|        - |  3596 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|        - |  3597 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|     1356 |  3598 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|        - |  3599 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|        - |  3600 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|      297 |  3601 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - |  3602 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|      575 |  3603 | `		if( pI->iP2 == 0 ){` |
|      575 |  3604 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|      278 |  3605 | `		}else{` |
|      ! 0 |  3606 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|      ! 0 |  3607 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|        - |  3608 | `		}` |
|      575 |  3609 | `		break;` |
|        - |  3610 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|        - |  3611 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|      219 |  3612 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|        - |  3613 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|        - |  3614 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|        - |  3615 | `	case PH7_OP_NOOP:` |
|        - |  3616 | `	case PH7_OP_SNAPSHOT:` |
|      441 |  3617 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - |  3618 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|        - |  3619 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      924 |  3620 | `	case PH7_OP_STORE:` |
|     1850 |  3621 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|     1850 |  3622 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|        - |  3623 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|     1820 |  3624 | `	case PH7_OP_POP:` |
|        - |  3625 | `	case PH7_OP_CONSUME:` |
|     3622 |  3626 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - |  3627 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|        - |  3628 | `	 * pushed and none is popped, on any path. */` |
|      ! 0 |  3629 | `	case PH7_OP_ROT_CALLEE:` |
|      ! 0 |  3630 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|        - |  3631 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|        - |  3632 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|        - |  3633 | `	 * true pop count is a runtime value — never reaches here. */` |
|      669 |  3634 | `	case PH7_OP_CALL:` |
|     1288 |  3635 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|        - |  3636 | `	/* Jumps. */` |
|       99 |  3637 | `	case PH7_OP_JMP:` |
|      203 |  3638 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|      227 |  3639 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|      456 |  3640 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|      456 |  3641 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|        - |  3642 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|     4563 |  3643 | `	case PH7_OP_DONE:` |
|     9088 |  3644 | `		n = 0; break;` |
|    10714 |  3645 | `	default:` |
|    21190 |  3646 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|        - |  3647 | `	}` |
|    62654 |  3648 | `	*pPush = push; *pN = n;` |
|    62654 |  3649 | `	return 1;` |
|    41561 |  3650 | `}` |
|        - |  3651 | `/*` |
|        - |  3652 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|        - |  3653 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|        - |  3654 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|        - |  3655 | ` */` |
|    24325 |  3656 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|        5 |  3657 | `{` |
|        - |  3658 | `	void *pScratch;` |
|        - |  3659 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|        - |  3660 | `	sxu32 nQ, i, nIter, nCap;` |
|        - |  3661 | `	sxi32 iMax;` |
|        - |  3662 | `	int push, n, k;` |
|        - |  3663 | `	sxu32 succ[2]; sxi32 delta[2];` |
|    24330 |  3664 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|        - |  3665 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|      ! 0 |  3666 | `		return VM_STACK_UNMODELED;` |
|        - |  3667 | `	}` |
|        - |  3668 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|    77074 |  3669 | `	for( i = 0; i < nInstr; i++ ){` |
|    73934 |  3670 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|    21190 |  3671 | `			return VM_STACK_UNMODELED;` |
|        - |  3672 | `		}` |
|    26180 |  3673 | `	}` |
|        - |  3674 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|        - |  3675 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|        - |  3676 | `	 * first (the byte array last needs no alignment). */` |
|     3145 |  3677 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|     3145 |  3678 | `	if( pScratch == 0 ){` |
|      ! 0 |  3679 | `		return VM_STACK_UNMODELED;` |
|        - |  3680 | `	}` |
|     3145 |  3681 | `	aH  = (sxi32 *)pScratch;` |
|     3145 |  3682 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|     3145 |  3683 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|    15806 |  3684 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|     3145 |  3685 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|     3145 |  3686 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|    13050 |  3687 | `	while( nQ > 0 ){` |
|     9910 |  3688 | `		sxu32 pc = aQ[--nQ];` |
|        - |  3689 | `		sxi32 h;` |
|     9910 |  3690 | `		aIn[pc] = 0;` |
|     9910 |  3691 | `		h = aH[pc];` |
|     9910 |  3692 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     9910 |  3693 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     9910 |  3694 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     9910 |  3695 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|    16713 |  3696 | `		for( k = 0; k < n; k++ ){` |
|     6808 |  3697 | `			sxi32 hn = h + delta[k];` |
|     6808 |  3698 | `			sxu32 t = succ[k];` |
|     6808 |  3699 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|     6808 |  3700 | `			if( hn > aH[t] ){` |
|     6770 |  3701 | `				aH[t] = hn;` |
|     6770 |  3702 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|     3347 |  3703 | `			}` |
|     3371 |  3704 | `		}` |
|     9910 |  3705 | `		if( iMax < 0 ){ break; }` |
|        5 |  3706 | `	}` |
|     3145 |  3707 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|     3145 |  3708 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|    12039 |  3709 | `}` |
|        - |  3710 | `/*` |
|        - |  3711 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  3712 | ` * our compiled PHP program.` |
|        - |  3713 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  3714 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  3715 | ` *` |
|        - |  3716 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|        - |  3717 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|        - |  3718 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|        - |  3719 | ` * eval, coroutine, callbacks) call this directly.` |
|        - |  3720 | ` */` |
|  4553350 |  3721 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|        - |  3722 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  3723 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  3724 | `	)` |
|        5 |  3725 | `{` |
|        - |  3726 | `	ph7_value *pStack;` |
|        - |  3727 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  3728 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  3729 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  3730 | `  ** on the maximum stack depth required.` |
|        - |  3731 | `  **` |
|        - |  3732 | `  ** Allocation all the stack space we will ever need.` |
|        - |  3733 | `  */` |
|  4553355 |  3734 | `	nInstr += VM_STACK_GUARD;` |
|  4553355 |  3735 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|  4553355 |  3736 | `	if( pStack == 0 ){` |
|      ! 0 |  3737 | `		return 0;` |
|        - |  3738 | `	}` |
|        - |  3739 | `	/* Initialize the operand stack. PH7_MemObjInit per slot is one call, one` |
|        - |  3740 | `	 * sizeof(ph7_value) SyZero and one SyBlobInit each; the whole buffer is` |
|        - |  3741 | `	 * contiguous, so zero it ONCE and fill in only the three fields a null value` |
|        - |  3742 | `	 * needs that are not zero. This is a hot path in its own right -- an OP_CALL` |
|        - |  3743 | `	 * that misses the recycling pool inits the callee's whole stack, and the fill` |
|        - |  3744 | `	 * loop below was 11% of a phpcs run. */` |
|  4553355 |  3745 | `	SyZero(pStack,nInstr * sizeof(ph7_value));` |
| 88913015 |  3746 | `	while( nInstr > 0 ){` |
| 84359665 |  3747 | `		ph7_value *pSlot = &pStack[--nInstr];` |
| 84359665 |  3748 | `		pSlot->pVm = pVm;` |
| 84359665 |  3749 | `		pSlot->sBlob.pAllocator = &pVm->sAllocator;` |
| 84359665 |  3750 | `		pSlot->iFlags = MEMOBJ_NULL;` |
|        5 |  3751 | `	}` |
|        - |  3752 | `	/* Ready for bytecode execution */` |
|  4553355 |  3753 | `	return pStack;` |
|  2276407 |  3754 | `}` |
|        - |  3755 | `/*` |
|        - |  3756 | ` * Operand-stack recycling.` |
|        - |  3757 | ` *` |
|        - |  3758 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|        - |  3759 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|        - |  3760 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|        - |  3761 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|        - |  3762 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|        - |  3763 | ` * the per-slot PH7_MemObjInit.` |
|        - |  3764 | ` *` |
|        - |  3765 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|        - |  3766 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|        - |  3767 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|        - |  3768 | ` * cold/suspend/abort paths that never route through here. A buffer is reused` |
|        - |  3769 | ` * only for a request of exactly its size (never over-allocated), and the pool is` |
|        - |  3770 | ` * bounded three ways so it can't grow without end: entry count, per-buffer size,` |
|        - |  3771 | ` * and -- the one that actually bounds the MEMORY -- a total parked-slot budget.` |
|        - |  3772 | ` *` |
|        - |  3773 | ` * Only an EXACT size is reusable, so the size picks the chain: the pool is` |
|        - |  3774 | `` * PH7_STACK_POOL_BUCKETS separate LIFO lists indexed by `nCap & (BUCKETS-1)`, with`` |
|        - |  3775 | ` * the size still checked per node. It used to be ONE list walked end to end. That` |
|        - |  3776 | ` * replaced a head-only match, which was tuned for the design target -- recursion, or` |
|        - |  3777 | ` * a hot loop calling one function: one size, near-total reuse -- and which real code` |
|        - |  3778 | ` * (a dozen differently-sized functions in turn) missed on nearly every call, falling` |
|        - |  3779 | ` * back to a fresh buffer whose every slot had to be initialized: 11% of a phpcs run` |
|        - |  3780 | ` * sat in that init. Walking the whole list fixed the misses and bought its own` |
|        - |  3781 | ` * problem, because the cap that makes the reuse work is 256 buffers and a call` |
|        - |  3782 | ` * compared itself against all of them: 2.2% of the run, nearly all of it against` |
|        - |  3783 | ` * sizes it could never take. Keying by size keeps the hit rate and drops the walk.` |
|        - |  3784 | ` */` |
|        - |  3785 | `typedef struct VmIdleStack VmIdleStack;` |
|        - |  3786 | `struct VmIdleStack {` |
|        - |  3787 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|        - |  3788 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|        - |  3789 | `	VmIdleStack *pNext;  /* LIFO link */` |
|        - |  3790 | `};` |
|        - |  3791 | `#define VM_STACK_POOL_MAX 256      /* max buffers parked at once. A program calls far more` |
|        - |  3792 | `                                    * than 64 distinct-sized functions in its hot loop, and a` |
|        - |  3793 | `                                    * pool that is FULL turns every recycle into a free and` |
|        - |  3794 | `                                    * every call after it into a fresh, freshly-initialized` |
|        - |  3795 | `                                    * buffer: at 64 entries a phpcs run refused 86k of its` |
|        - |  3796 | `                                    * 400k recycles for being full and missed 24% of its` |
|        - |  3797 | `                                    * allocations. The memory this could cost is bounded by` |
|        - |  3798 | `                                    * the slot budget below, not by this count. */` |
|        - |  3799 | `#define VM_STACK_POOL_MAXSLOTS 4096 /* never pool a buffer bigger than this: one outlier` |
|        - |  3800 | `                                    * (an unmodelable body falls back to its whole` |
|        - |  3801 | `                                    * instruction count — 8000+ slots is real) must not sit` |
|        - |  3802 | `                                    * in the pool holding half a megabyte for a size nothing` |
|        - |  3803 | `                                    * asks for again */` |
|        - |  3804 | `#define VM_STACK_POOL_SLOTS 65536  /* total slots parked across the pool: ~4 MB of ph7_values,` |
|        - |  3805 | `                                    * the real bound on what recycling costs. Entry count and` |
|        - |  3806 | `                                    * per-buffer size are shape limits; this is the budget. */` |
|        - |  3807 | `/*` |
|        - |  3808 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|        - |  3809 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|        - |  3810 | ` */` |
|   827023 |  3811 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|        5 |  3812 | `{` |
|   827028 |  3813 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|        - |  3814 | `	/* Only this size's own chain can hold a buffer this call can take. */` |
|   827028 |  3815 | `	VmIdleStack **ppIdle =` |
|   827023 |  3816 | `		(VmIdleStack **)&pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)];` |
|   840210 |  3817 | `	while( *ppIdle ){` |
|   826909 |  3818 | `		VmIdleStack *pIdle = *ppIdle;` |
|   826909 |  3819 | `		if( pIdle->nCap == nCap ){` |
|   813727 |  3820 | `			ph7_value *pStack = pIdle->pStack;` |
|   813727 |  3821 | `			*ppIdle = pIdle->pNext;` |
|   813727 |  3822 | `			pVm->nIdleOperandStacks--;` |
|   813727 |  3823 | `			pVm->nIdleOperandSlots -= nCap;` |
|        - |  3824 | `			/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|        - |  3825 | `			 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|   813727 |  3826 | `			pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|   813727 |  3827 | `			pVm->pIdleStackNodes = pIdle;` |
|   813727 |  3828 | `			return pStack; /* slots already released -> reusable without re-init */` |
|        - |  3829 | `		}` |
|    13185 |  3830 | `		ppIdle = &pIdle->pNext;` |
|        3 |  3831 | `	}` |
|    13306 |  3832 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|   413721 |  3833 | `}` |
|        - |  3834 | `/*` |
|        - |  3835 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|        - |  3836 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size); a parked` |
|        - |  3837 | ` * buffer must leave here with every slot released, so it can be handed to the next` |
|        - |  3838 | ` * call without re-initialization and can never retain a live value.` |
|        - |  3839 | ` *` |
|        - |  3840 | ` * nLive is how many slots the finishing activation could have touched -- its` |
|        - |  3841 | ` * operand-stack WATERMARK, the deepest its top ever reached (VmByteCodeExecBody` |
|        - |  3842 | ` * keeps it; the callee's record carries it here). Above that the buffer is still` |
|        - |  3843 | ` * exactly as it was handed out: clean. Releasing the whole capacity instead was` |
|        - |  3844 | ` * 1,044,870,548 releases on the phpcs step of record -- 38% of every release the` |
|        - |  3845 | ` * engine makes -- of which 34,354 found a value and 1,044,836,194 found a slot` |
|        - |  3846 | ` * that owned nothing. 11.85 million recycles walking 88 slots each, to free 34` |
|        - |  3847 | ` * thousand values, all of which live in the first handful of slots. The watermark` |
|        - |  3848 | ` * bounds the same sweep at 44.5 million slots (-95.7%) and finds every one of` |
|        - |  3849 | ` * those values; both corpora and the phpcs step agree that NOTHING above it is` |
|        - |  3850 | ` * ever dirty (the value-primitive census is what found it).` |
|        - |  3851 | ` *` |
|        - |  3852 | ` * The bound has to be the watermark and not the final top: a call abandons its` |
|        - |  3853 | `` * argument slots by lowering the top past them (`pTos = &pTos[-nCallArgs]`), so a`` |
|        - |  3854 | ` * body's own stack routinely carries dirt ABOVE where its top ends up -- 28,343` |
|        - |  3855 | ` * of those 11.85 million recycles. The watermark is above both by construction.` |
|        - |  3856 | ` */` |
|   827023 |  3857 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap, sxu32 nLive)` |
|        5 |  3858 | `{` |
|        - |  3859 | `	VmIdleStack *pIdle;` |
|        - |  3860 | `	sxu32 i;` |
|   827028 |  3861 | `	if( pStack == 0 ){` |
|      ! 0 |  3862 | `		return;` |
|        - |  3863 | `	}` |
|   827023 |  3864 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX` |
|   823371 |  3865 | `	 \|\| nCap > VM_STACK_POOL_MAXSLOTS` |
|   819724 |  3866 | `	 \|\| pVm->nIdleOperandSlots + nCap > VM_STACK_POOL_SLOTS ){` |
|     7305 |  3867 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|     7305 |  3868 | `		return;` |
|        - |  3869 | `	}` |
|        - |  3870 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|        - |  3871 | `	 * pool-allocate only when the spare list is empty. */` |
|   819724 |  3872 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|   819724 |  3873 | `	if( pIdle ){` |
|   813727 |  3874 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|   407157 |  3875 | `	}else{` |
|     6002 |  3876 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|     6002 |  3877 | `		if( pIdle == 0 ){` |
|      ! 0 |  3878 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|      ! 0 |  3879 | `			return;` |
|        - |  3880 | `		}` |
|        - |  3881 | `	}` |
|   819724 |  3882 | `	if( nLive > nCap ){` |
|      ! 0 |  3883 | `		nLive = nCap;   /* defensive: never walk past the buffer */` |
|      ! 0 |  3884 | `	}` |
|  2820251 |  3885 | `	for( i = 0; i < nLive; i++ ){` |
|  2000532 |  3886 | `		PH7_MemObjRelease(&pStack[i]);` |
|        - |  3887 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|        - |  3888 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|        - |  3889 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|        - |  3890 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|        - |  3891 | `		 * across invocations — cheap defense in depth. The slots ABOVE nLive keep` |
|        - |  3892 | `		 * whatever index they had, which is exactly what a FRESH buffer looks like:` |
|        - |  3893 | `		 * VmNewOperandStack zeroes the array and never writes nIdx, so slot 0's` |
|        - |  3894 | `		 * index is what an untouched slot has always carried. */` |
|  2000532 |  3895 | `		pStack[i].nIdx = SXU32_HIGH;` |
|   998110 |  3896 | `	}` |
|   819724 |  3897 | `	pIdle->pStack = pStack;` |
|   819724 |  3898 | `	pIdle->nCap = nCap;` |
|   819724 |  3899 | `	pIdle->pNext = (VmIdleStack *)pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)];` |
|   819724 |  3900 | `	pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)] = pIdle;` |
|   819724 |  3901 | `	pVm->nIdleOperandStacks++;` |
|   819724 |  3902 | `	pVm->nIdleOperandSlots += nCap;` |
|   413721 |  3903 | `}` |
|        - |  3904 | `/*` |
|        - |  3905 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  3906 | ` * This routine gets called by the PH7 engine after` |
|        - |  3907 | ` * successful compilation of the target PHP program.` |
|        - |  3908 | ` */` |
|     6691 |  3909 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  3910 | `	ph7_vm *pVm /* Target VM */` |
|        - |  3911 | `	)` |
|        5 |  3912 | `{` |
|        - |  3913 | `	SyHashEntry *pEntry;` |
|        - |  3914 | `	sxi32 rc;` |
|     6696 |  3915 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  3916 | `		/* Initialize your VM first */` |
|      ! 0 |  3917 | `		return SXERR_CORRUPT;` |
|        - |  3918 | `	}` |
|        - |  3919 | `	/* Mark the VM ready for byte-code execution */` |
|     6696 |  3920 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  3921 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - |  3922 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - |  3923 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - |  3924 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     6696 |  3925 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - |  3926 | `	/* Emit the DONE instruction */` |
|     6696 |  3927 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     6696 |  3928 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3929 | `		return SXERR_MEM;` |
|        - |  3930 | `	}` |
|        - |  3931 | `	/* Script return value */` |
|     6696 |  3932 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  3933 | `	/* Allocate a new operand stack */` |
|     6696 |  3934 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     6696 |  3935 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  3936 | `		return SXERR_MEM;` |
|        - |  3937 | `	}` |
|        - |  3938 | `	/* Set the default VM output consumer callback and it's` |
|        - |  3939 | `	 * private data. */` |
|     6696 |  3940 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     6696 |  3941 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  3942 | `	/* The host functions themselves -- the special ones, the core builtins, the HTTP` |
|        - |  3943 | `	 * response verbs and PCRE's -- were registered by PH7_VmInit, which runs before` |
|        - |  3944 | `	 * the program compiles; a declaration that shadows one has to be refused where` |
|        - |  3945 | `	 * php refuses it, and that is at compile time. Their constants and the metadata` |
|        - |  3946 | `	 * stamped onto them are still this routine's, below. */` |
|        - |  3947 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  3948 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  3949 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  3950 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  3951 | `	 * below it is compile-time/init state that survives a reset. */` |
|     6696 |  3952 | `	pVm->nSuperBaseline = pVm->aMemObj.nUsed;` |
|        - |  3953 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     6696 |  3954 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     6696 |  3955 | `	if( rc != SXRET_OK ){` |
|        - |  3956 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3957 | `		return rc;` |
|        - |  3958 | `	}` |
|        - |  3959 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     6696 |  3960 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  3961 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|     6696 |  3962 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|        - |  3963 | `	/* Register ext/pcntl's signal, priority and namespace constants (none of` |
|        - |  3964 | `	 * which exist on Windows, where php builds no ext/pcntl either) */` |
|     6696 |  3965 | `	PH7_RegisterPcntlConstants(&(*pVm));` |
|        - |  3966 | `	/* Register the LOG_* constants ext/standard's syslog trio reads */` |
|     6696 |  3967 | `	PH7_RegisterSyslogConstants(&(*pVm));` |
|        - |  3968 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|        - |  3969 | `	/* Register ext/sockets' AF_, SOCK_, SO_ and SOCKET_E family. php builds this` |
|        - |  3970 | `	 * extension on every platform, so unlike pcntl's these are not #ifdef'd` |
|        - |  3971 | `	 * away on Windows -- only the names that platform has no macro for are. */` |
|     6696 |  3972 | `	PH7_RegisterSocketsConstants(&(*pVm));` |
|        - |  3973 | `#endif` |
|        - |  3974 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  3975 | `	/* Register PCRE constants [i.e: PREG_SPLIT_NO_EMPTY, PREG_PATTERN_ORDER, etc.] */` |
|     6696 |  3976 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  3977 | `#endif` |
|        - |  3978 | `#ifdef PH7_ENABLE_LIBXML` |
|        - |  3979 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|     6696 |  3980 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|        - |  3981 | `#endif` |
|        - |  3982 | `#ifdef PH7_ENABLE_CURL` |
|        - |  3983 | `	/* Register the CURLOPT_* / CURLINFO_* / CURLE_* family */` |
|     6696 |  3984 | `	PH7_RegisterCurlConstants(&(*pVm));` |
|        - |  3985 | `#endif` |
|        - |  3986 | `	/* Every extension has registered its own constants by now, so the` |
|        - |  3987 | `	 * deprecation marks can be stamped on the names they cover wherever those` |
|        - |  3988 | `	 * were installed -- a name a build does not carry is simply skipped. */` |
|     6696 |  3989 | `	PH7_MarkDeprecatedConstants(&(*pVm));` |
|        - |  3990 | `	/* Same stamp for the FUNCTIONS and native methods php deprecated; the` |
|        - |  3991 | `	 * classes were installed by PH7_VmInit, well before this runs. */` |
|     6696 |  3992 | `	PH7_MarkDeprecatedFunctions(&(*pVm));` |
|        - |  3993 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|        - |  3994 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|     6696 |  3995 | `	VmSetBuiltinArity(&(*pVm));` |
|        - |  3996 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|     6696 |  3997 | `	VmSetBuiltinSignatures(&(*pVm));` |
|        - |  3998 | `	/* ...and hide the nine registrations that are language CONSTRUCTS rather than` |
|        - |  3999 | `	 * functions, now every table that could hold one has been filled. */` |
|     6696 |  4000 | `	PH7_VmMarkLanguageConstructs(&(*pVm));` |
|        - |  4001 | `	/* Initialize and install static and constants class attributes.` |
|        - |  4002 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  4003 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  4004 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  4005 | `	 * that function in sync when changing what is reserved here. */` |
|        - |  4006 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|        - |  4007 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|        - |  4008 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|        - |  4009 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|        - |  4010 | `	 * static default while the exception classes were still unmounted: the throw` |
|        - |  4011 | `	 * failed to construct its own exception, that failure threw again, and the` |
|        - |  4012 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|        - |  4013 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|        - |  4014 | `	 * The passes are independent — attribute initializers reference constants and` |
|        - |  4015 | `	 * enum cases, which materialize on demand, never a method table. */` |
|     6696 |  4016 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|  1473871 |  4017 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  1467180 |  4018 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|  1467180 |  4019 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4020 | `			return rc;` |
|        - |  4021 | `		}` |
|        5 |  4022 | `	}` |
|     6696 |  4023 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|  1472549 |  4024 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  1465864 |  4025 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|  1465864 |  4026 | `		if( rc != SXRET_OK ){` |
|        8 |  4027 | `			return rc;` |
|        - |  4028 | `		}` |
|        5 |  4029 | `	}` |
|        - |  4030 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|        - |  4031 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|     6690 |  4032 | `	pVm->nNextObjId = 1;` |
|        - |  4033 | `	/* VM is ready for bytecode execution */` |
|     6690 |  4034 | `	return SXRET_OK;` |
|     3345 |  4035 | `}` |
|        - |  4036 | `/*` |
|        - |  4037 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  4038 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  4039 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  4040 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  4041 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  4042 | ` */` |
|       16 |  4043 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  4044 | `{` |
|        - |  4045 | `	/* VmRefSlotUnlink empties the cell it is given and decrements nRefUsed, so a` |
|        - |  4046 | `	 * sweep of the table leaves it empty and nRefUsed at 0 — no extra clearing` |
|        - |  4047 | `	 * needed. The cell array and nRefSize survive.` |
|        - |  4048 | `	 *` |
|        - |  4049 | `	 * Unlinking one cell deletes the names and array nodes it holds, which` |
|        - |  4050 | `	 * releases values, which can unlink OTHER cells — including ones this sweep` |
|        - |  4051 | `	 * has already passed, and (through a destructor) ones it has not created yet.` |
|        - |  4052 | `	 * So the sweep repeats while it is still making progress rather than trusting` |
|        - |  4053 | `	 * one pass, and stops the moment a pass frees nothing so it cannot spin. */` |
|       24 |  4054 | `	for(;;){` |
|       32 |  4055 | `		sxu32 n, nBefore = pVm->nRefUsed;` |
|       32 |  4056 | `		if( nBefore == 0 ){` |
|       16 |  4057 | `			break;` |
|        - |  4058 | `		}` |
|      912 |  4059 | `		for( n = 0 ; n < pVm->nRefSize ; ++n ){` |
|     1404 |  4060 | `			while( pVm->apRefObj[n] ){` |
|      508 |  4061 | `				void *pWord = pVm->apRefObj[n];` |
|      508 |  4062 | `				PH7_VmSlotUnlink(&(*pVm),n);` |
|      508 |  4063 | `				if( pVm->apRefObj[n] == pWord ){` |
|      ! 0 |  4064 | `					break; /* unlink did not clear it: do not spin on this cell */` |
|        - |  4065 | `				}` |
|      ! 0 |  4066 | `			}` |
|      448 |  4067 | `		}` |
|       16 |  4068 | `		if( pVm->nRefUsed >= nBefore ){` |
|      ! 0 |  4069 | `			break;` |
|        - |  4070 | `		}` |
|      ! 0 |  4071 | `	}` |
|       16 |  4072 | `}` |
|        - |  4073 | `/*` |
|        - |  4074 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  4075 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  4076 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  4077 | ` */` |
|       96 |  4078 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  4079 | `{` |
|       96 |  4080 | `	PH7_MemObjRelease(pObj);` |
|       96 |  4081 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       96 |  4082 | `}` |
|        - |  4083 | `/*` |
|        - |  4084 | ` * Empty a set_error_handler()/set_exception_handler() stack, releasing every` |
|        - |  4085 | ` * saved handler. The SySet itself keeps its buffer for the next request; the` |
|        - |  4086 | ` * whole thing dies with the VM allocator either way.` |
|        - |  4087 | ` */` |
|       32 |  4088 | `static void VmReleaseHandlerStack(SySet *pStack)` |
|      ! 0 |  4089 | `{` |
|       32 |  4090 | `	VmHandlerSlot *aSlot = (VmHandlerSlot *)SySetBasePtr(pStack);` |
|        - |  4091 | `	sxu32 n;` |
|       32 |  4092 | `	for( n = 0 ; n < SySetUsed(pStack) ; ++n ){` |
|      ! 0 |  4093 | `		PH7_MemObjRelease(&aSlot[n].sCb);` |
|      ! 0 |  4094 | `	}` |
|       32 |  4095 | `	SySetReset(pStack);` |
|       32 |  4096 | `}` |
|        - |  4097 | `/*` |
|        - |  4098 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  4099 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  4100 | ` * of statics).` |
|        - |  4101 | ` */` |
|    39985 |  4102 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|        5 |  4103 | `{` |
|    39990 |  4104 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  4105 | `	sxu32 k;` |
|    39998 |  4106 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        9 |  4107 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        5 |  4108 | `	}` |
|    39990 |  4109 | `}` |
|        - |  4110 | `/*` |
|        - |  4111 | ` * Tear down one run-time closure's per-instantiation ph7_vm_func: reset its` |
|        - |  4112 | ` * (template-shared) statics, release its captured-by-value environment, then free` |
|        - |  4113 | ` * the name buffer and the structure. The caller owns unlinking the hFunction row.` |
|        - |  4114 | ` */` |
|    18077 |  4115 | `static void VmFreeRuntimeClosure(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        5 |  4116 | `{` |
|    18082 |  4117 | `	ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|    18082 |  4118 | `	const char *zName = SyStringData(&pFunc->sName);` |
|        - |  4119 | `	sxu32 k;` |
|    18082 |  4120 | `	VmResetFuncStatics(pFunc);` |
|    45501 |  4121 | `	for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|    27424 |  4122 | `		PH7_MemObjRelease(&aEnv[k].sValue);` |
|    13573 |  4123 | `	}` |
|    18082 |  4124 | `	SySetRelease(&pFunc->aClosureEnv);` |
|    18082 |  4125 | `	if( zName ){` |
|    18082 |  4126 | `		SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|     8919 |  4127 | `	}` |
|    18082 |  4128 | `	SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|    18082 |  4129 | `}` |
|        - |  4130 | `/*` |
|        - |  4131 | ` * The run-time closure this name belongs to, or NULL when the name is anything` |
|        - |  4132 | ` * else -- a named user function, a host function, a method. Only a` |
|        - |  4133 | ` * per-instantiation copy (VM_FUNC_CLOSURE, minted by OP_LOAD_CLOSURE) is owned by` |
|        - |  4134 | ` * the Closure objects that name it; everything else in hFunction outlives them.` |
|        - |  4135 | ` */` |
|    39213 |  4136 | `PH7_PRIVATE ph7_vm_func * PH7_VmRuntimeClosure(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        5 |  4137 | `{` |
|        - |  4138 | `	SyHashEntry *pEntry;` |
|        - |  4139 | `	ph7_vm_func *pFunc;` |
|    39218 |  4140 | `	if( zName == 0 \|\| nByte < 1 ){` |
|      ! 0 |  4141 | `		return 0;` |
|        - |  4142 | `	}` |
|    39218 |  4143 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nByte);` |
|    39218 |  4144 | `	if( pEntry == 0 ){` |
|      643 |  4145 | `		return 0;` |
|        - |  4146 | `	}` |
|    38580 |  4147 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    38580 |  4148 | `	if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|      681 |  4149 | `		return 0;` |
|        - |  4150 | `	}` |
|    37904 |  4151 | `	return pFunc;` |
|    19370 |  4152 | `}` |
|   852968 |  4153 | `PH7_PRIVATE void PH7_VmClosureFuncRef(ph7_vm_func *pFunc)` |
|        5 |  4154 | `{` |
|   852973 |  4155 | `	if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|    57479 |  4156 | `		pFunc->nRef++;` |
|    28388 |  4157 | `	}` |
|   852973 |  4158 | `}` |
|        - |  4159 | `/*` |
|        - |  4160 | ` * Give back one hold on a run-time closure. At zero nothing can reach it any more` |
|        - |  4161 | ` * -- no Closure object names it and no activation is running it -- so it leaves` |
|        - |  4162 | ` * the function table and the memory goes back.` |
|        - |  4163 | ` */` |
|   852339 |  4164 | `PH7_PRIVATE void PH7_VmClosureFuncUnref(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        5 |  4165 | `{` |
|   852344 |  4166 | `	if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|   795485 |  4167 | `		return;` |
|        - |  4168 | `	}` |
|    56864 |  4169 | `	pFunc->nRef--;` |
|    56864 |  4170 | `	if( pFunc->nRef > 0 \|\| pVm->bInReset \|\| pFunc->bQueued ){` |
|    37921 |  4171 | `		return;` |
|        - |  4172 | `	}` |
|        - |  4173 | `	/* Queued, not freed on the spot. The drop that brings a closure to zero is` |
|        - |  4174 | `	 * usually OP_CALL releasing the Closure OBJECT it just unwrapped -- and the` |
|        - |  4175 | `	 * dispatch that did it is about to look the function up BY NAME. Freeing here` |
|        - |  4176 | ``	 * turned `(function(){ yield 1; })()` into "Call to undefined function`` |
|        - |  4177 | `	 * [closure_6]()". The list is drained at the VM's fetch point, where no dispatch` |
|        - |  4178 | `	 * is half-done; anything that took the function up again in between (the call's` |
|        - |  4179 | `	 * own frame does, one instruction later) is simply dropped from the list. */` |
|    18948 |  4180 | `	pFunc->bQueued = 1;` |
|    18948 |  4181 | `	if( SySetPut(&pVm->aDeadClosure,(const void *)&pFunc) != SXRET_OK ){` |
|      ! 0 |  4182 | `		pFunc->bQueued = 0;` |
|      ! 0 |  4183 | `		return; /* no room to remember it: it goes with the wholesale teardown */` |
|        - |  4184 | `	}` |
|    18948 |  4185 | `	pVm->bClosurePurge = 1;` |
|   426259 |  4186 | `}` |
|        - |  4187 | `/*` |
|        - |  4188 | ` * Free the run-time closures nothing needs any more. Called only from the VM's` |
|        - |  4189 | ` * fetch point, between two instructions, where no dispatch is half-resolved.` |
|        - |  4190 | ` *` |
|        - |  4191 | ` * Drained by POPPING: freeing one releases its captured environment, which can` |
|        - |  4192 | ` * release the last Closure object naming ANOTHER one and queue it mid-drain.` |
|        - |  4193 | ` */` |
|    17518 |  4194 | `PH7_PRIVATE void PH7_VmPurgeDeadClosures(ph7_vm *pVm)` |
|        5 |  4195 | `{` |
|    17523 |  4196 | `	pVm->bClosurePurge = 0;` |
|    26911 |  4197 | `	for(;;){` |
|    35993 |  4198 | `		ph7_vm_func **ppFunc = (ph7_vm_func **)SySetPop(&pVm->aDeadClosure);` |
|        - |  4199 | `		ph7_vm_func *pFunc;` |
|        - |  4200 | `		SyHashEntry *pEntry;` |
|    35993 |  4201 | `		if( ppFunc == 0 ){` |
|    17523 |  4202 | `			break;` |
|        - |  4203 | `		}` |
|    18475 |  4204 | `		pFunc = *ppFunc;` |
|    18475 |  4205 | `		pFunc->bQueued = 0;` |
|    18475 |  4206 | `		if( pFunc->nRef > 0 \|\| pVm->bInReset ){` |
|      402 |  4207 | `			continue; /* taken up again between the drop and here */` |
|        - |  4208 | `		}` |
|    26995 |  4209 | `		pEntry = SyHashGet(&pVm->hFunction,(const void *)SyStringData(&pFunc->sName),` |
|     8917 |  4210 | `			SyStringLength(&pFunc->sName));` |
|    18078 |  4211 | `		if( pEntry == 0 \|\| pEntry->pUserData != (void *)pFunc ){` |
|        - |  4212 | `			/* The name is not this copy's any more (an overload chain, a reset in` |
|        - |  4213 | `			 * flight): leave it to the wholesale teardown rather than guess. */` |
|      ! 0 |  4214 | `			continue;` |
|        - |  4215 | `		}` |
|    18078 |  4216 | `		SyHashDeleteEntry2(pEntry);` |
|    18078 |  4217 | `		VmFreeRuntimeClosure(&(*pVm),pFunc);` |
|        5 |  4218 | `	}` |
|    17523 |  4219 | `}` |
|        - |  4220 | `/*` |
|        - |  4221 | `` * A Closure OBJECT taking or giving back its hold on the function `$__fn` names.`` |
|        - |  4222 | `` * One door for all of them: `function(){}` (OP_LOAD_CLOSURE via VmCreateClosure),`` |
|        - |  4223 | `` * `clone`, and `bindTo`/`bind`, which clones.`` |
|        - |  4224 | ` */` |
|    40355 |  4225 | `PH7_PRIVATE void PH7_VmClosureInstanceRef(ph7_vm *pVm,ph7_class_instance *pObj,int iDelta)` |
|        5 |  4226 | `{` |
|        - |  4227 | `	ph7_value *pFn;` |
|        - |  4228 | `	ph7_vm_func *pFunc;` |
|        - |  4229 | `	SyString sAttr;` |
|    40360 |  4230 | `	if( pObj == 0 \|\| pVm->pClosureClass == 0 \|\| pObj->pClass != pVm->pClosureClass ){` |
|     1804 |  4231 | `		return;` |
|        - |  4232 | `	}` |
|    39218 |  4233 | `	SyStringInitFromBuf(&sAttr,"__fn",4);` |
|    39218 |  4234 | `	pFn = PH7_ClassInstanceFetchAttr(pObj,&sAttr);` |
|    39218 |  4235 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4236 | `		return;` |
|        - |  4237 | `	}` |
|    58583 |  4238 | `	pFunc = PH7_VmRuntimeClosure(&(*pVm),(const char *)SyBlobData(&pFn->sBlob),` |
|    19365 |  4239 | `		SyBlobLength(&pFn->sBlob));` |
|    39218 |  4240 | `	if( pFunc == 0 ){` |
|     1319 |  4241 | `		return;` |
|        - |  4242 | `	}` |
|    37904 |  4243 | `	if( iDelta > 0 ){` |
|    19262 |  4244 | `		PH7_VmClosureFuncRef(pFunc);` |
|     9512 |  4245 | `	}else{` |
|    18647 |  4246 | `		PH7_VmClosureFuncUnref(&(*pVm),pFunc);` |
|        - |  4247 | `	}` |
|    19941 |  4248 | `}` |
|        - |  4249 | `/*` |
|        - |  4250 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  4251 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  4252 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  4253 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  4254 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  4255 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  4256 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  4257 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  4258 | ` *    has its static sentinels reset.` |
|        - |  4259 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  4260 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  4261 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  4262 | ` */` |
|       16 |  4263 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  4264 | `{` |
|        - |  4265 | `	SyHashEntry *pEntry;` |
|       16 |  4266 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|    21928 |  4267 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|    21912 |  4268 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    21912 |  4269 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  4270 | `			/* Whatever run-time closures outlived their objects (one being executed` |
|        - |  4271 | `			 * when its last holder went, one the engine still names) go here. */` |
|        - |  4272 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  4273 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  4274 | `			VmFreeRuntimeClosure(&(*pVm),pFunc);` |
|        4 |  4275 | `			continue;` |
|        - |  4276 | `		}` |
|        - |  4277 | `		/* Named function: reset statics for every overload sharing this name. */` |
|    43816 |  4278 | `		while( pFunc ){` |
|    21908 |  4279 | `			VmResetFuncStatics(pFunc);` |
|    21908 |  4280 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  4281 | `		}` |
|      ! 0 |  4282 | `	}` |
|       16 |  4283 | `	pVm->closure_cnt = 0;` |
|       16 |  4284 | `}` |
|        - |  4285 | `/*` |
|        - |  4286 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  4287 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  4288 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  4289 | ` * the class re-mount registers fresh ones.` |
|        - |  4290 | ` */` |
|       16 |  4291 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  4292 | `{` |
|        - |  4293 | `	SyHashEntry *pEntry;` |
|        - |  4294 | `	/* The bitmap in front of the table goes with it, whether or not the table has` |
|        - |  4295 | `	 * anything left in it: a bit that outlived its entry would send a store into a` |
|        - |  4296 | `	 * lookup that answers nothing, and the class re-mount registers fresh ones. */` |
|       16 |  4297 | `	if( pVm->pFilterBits ){` |
|        4 |  4298 | `		SyZero(pVm->pFilterBits,pVm->nFilterBits >> 3);` |
|        2 |  4299 | `	}` |
|        - |  4300 | `	/* The table is emptied with it, so a bitmap that had been switched off can be` |
|        - |  4301 | `	 * trusted again from here. */` |
|       16 |  4302 | `	pVm->bFilterBitsOff = 0;` |
|        - |  4303 | `	/* Common case: no class static typed properties — table already empty. */` |
|       16 |  4304 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|       12 |  4305 | `		return;` |
|        - |  4306 | `	}` |
|        - |  4307 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  4308 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  4309 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  4310 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  4311 | `		if( pEntry->pUserData ){` |
|        4 |  4312 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  4313 | `		}` |
|      ! 0 |  4314 | `	}` |
|        4 |  4315 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  4316 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        4 |  4317 | `	pVm->nNativeSetSlot = 0;` |
|        8 |  4318 | `}` |
|        - |  4319 | `/*` |
|        - |  4320 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|        - |  4321 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|        - |  4322 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|        - |  4323 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|        - |  4324 | ` * comparing equal — both used to cast to 1.` |
|        - |  4325 | ` *` |
|        - |  4326 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|        - |  4327 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|        - |  4328 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|        - |  4329 | ` * aborting a cast.` |
|        - |  4330 | ` */` |
|      466 |  4331 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|        4 |  4332 | `{` |
|        - |  4333 | `	SyHashEntry *pEntry;` |
|        - |  4334 | `	phl_res_id *pRec;` |
|      470 |  4335 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|      ! 0 |  4336 | `		return 0;` |
|        - |  4337 | `	}` |
|      470 |  4338 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|      470 |  4339 | `	if( pEntry ){` |
|      416 |  4340 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|        - |  4341 | `	}` |
|       58 |  4342 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|       58 |  4343 | `	if( pRec == 0 ){` |
|      ! 0 |  4344 | `		return 0;` |
|        - |  4345 | `	}` |
|       58 |  4346 | `	pRec->pRes = pRes;` |
|       58 |  4347 | `	pRec->nId = pVm->nResourceIdNext++;` |
|       58 |  4348 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|      ! 0 |  4349 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|      ! 0 |  4350 | `		return 0;` |
|        - |  4351 | `	}` |
|       58 |  4352 | `	return pRec->nId;` |
|      237 |  4353 | `}` |
|        - |  4354 | `/*` |
|        - |  4355 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|        - |  4356 | ` * 1 for the next run, matching a fresh php process.` |
|        - |  4357 | ` */` |
|       16 |  4358 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|      ! 0 |  4359 | `{` |
|        - |  4360 | `	SyHashEntry *pEntry;` |
|       16 |  4361 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|       16 |  4362 | `		pVm->nResourceIdNext = 1;` |
|       16 |  4363 | `		return;` |
|        - |  4364 | `	}` |
|      ! 0 |  4365 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|      ! 0 |  4366 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|      ! 0 |  4367 | `		if( pEntry->pUserData ){` |
|      ! 0 |  4368 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|      ! 0 |  4369 | `		}` |
|      ! 0 |  4370 | `	}` |
|      ! 0 |  4371 | `	SyHashRelease(&pVm->hResourceId);` |
|      ! 0 |  4372 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      ! 0 |  4373 | `	pVm->nResourceIdNext = 1;` |
|        8 |  4374 | `}` |
|        - |  4375 | `/*` |
|        - |  4376 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  4377 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  4378 | ` *` |
|        - |  4379 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  4380 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  4381 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  4382 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  4383 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  4384 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  4385 | ` *` |
|        - |  4386 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  4387 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  4388 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  4389 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  4390 | ` * reserved during the run.` |
|        - |  4391 | ` *` |
|        - |  4392 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  4393 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  4394 | ` * global-scope destructors never fired.` |
|        - |  4395 | ` */` |
|       16 |  4396 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  4397 | `{` |
|        - |  4398 | `	sxu32 nWater,n;` |
|       16 |  4399 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  4400 | `		return SXERR_CORRUPT;` |
|        - |  4401 | `	}` |
|       16 |  4402 | `	nWater = pVm->nSuperBaseline;` |
|        - |  4403 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  4404 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|       16 |  4405 | `	pVm->pGlobal = 0;` |
|        - |  4406 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|        - |  4407 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|        - |  4408 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|        - |  4409 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|        - |  4410 | `	 * object); unref'ing here would race the teardown below. */` |
|       16 |  4411 | `	pVm->pClosureThis = 0;` |
|       16 |  4412 | `	pVm->pClosureScope = 0;` |
|        - |  4413 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  4414 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  4415 | `	 * here is unsafe. Engine memory is still reclaimed. Mirrors prior behaviour` |
|        - |  4416 | `	 * (global destructors never ran). */` |
|       16 |  4417 | `	pVm->bInReset = 1;` |
|        - |  4418 | `	/* (0) Forget every buffered cycle root. The object pool is about to go, and a` |
|        - |  4419 | `	 * row that outlived it would name freed memory on the next run. */` |
|       16 |  4420 | `	PH7_GcResetBuffer(&(*pVm));` |
|        - |  4421 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|       16 |  4422 | `	VmResetRefTable(&(*pVm));` |
|        - |  4423 | `	/* (1b) The pending-free list names functions the wholesale teardown below is` |
|        - |  4424 | `	 * about to free anyway; forget it rather than leave rows pointing at them. */` |
|       16 |  4425 | `	SySetReset(&pVm->aDeadClosure);` |
|       16 |  4426 | `	pVm->bClosurePurge = 0;` |
|        - |  4427 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  4428 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  4429 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  4430 | `	 * re-run define() overwrites the value in place). */` |
|       16 |  4431 | `	VmResetFunctionState(&(*pVm));` |
|        - |  4432 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  4433 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      560 |  4434 | `	for( n = nWater ; n < pVm->aMemObj.nUsed ; ++n ){` |
|      544 |  4435 | `		ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,n);` |
|      544 |  4436 | `		if( pObj ){` |
|      544 |  4437 | `			PH7_MemObjRelease(pObj);` |
|      272 |  4438 | `		}` |
|      272 |  4439 | `	}` |
|        - |  4440 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  4441 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|       16 |  4442 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  4443 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|        - |  4444 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|       16 |  4445 | `	VmResetResourceIds(&(*pVm));` |
|        - |  4446 | `	/* (5) Unwind any active frames back to none. */` |
|       32 |  4447 | `	while( pVm->pFrame ){` |
|       16 |  4448 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  4449 | `	}` |
|        - |  4450 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|       16 |  4451 | `	pVm->bInReset = 0;` |
|        - |  4452 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  4453 | `	 * slots (their indices no longer exist). Fully-free trailing segments are` |
|        - |  4454 | `	 * returned by VmMemPoolTruncate. */` |
|       16 |  4455 | `	VmMemPoolTruncate(&pVm->aMemObj,nWater);` |
|        - |  4456 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|       16 |  4457 | `	SyHashRelease(&pVm->hSuper);` |
|       16 |  4458 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|       16 |  4459 | `	SyZero(pVm->aSuperFirst,sizeof(pVm->aSuperFirst));` |
|        - |  4460 | `	/* A reused VM (the -S server, the in-process .phpt runner) starts the next run` |
|        - |  4461 | `	 * with the bytecode of the last one still holding its screened-at stamps. */` |
|       16 |  4462 | `	pVm->nCallableGen++;` |
|       16 |  4463 | `	pVm->nConstGen++;` |
|        - |  4464 | `	/* (8) Drain remaining per-exec containers. */` |
|       16 |  4465 | `	SySetReset(&pVm->aSelf);` |
|        - |  4466 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  4467 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  4468 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|       16 |  4469 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  4470 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  4471 | `		if( pCB ){` |
|        - |  4472 | `			int iArg;` |
|      ! 0 |  4473 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  4474 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  4475 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  4476 | `			}` |
|      ! 0 |  4477 | `		}` |
|      ! 0 |  4478 | `	}` |
|       16 |  4479 | `	SySetReset(&pVm->aShutdown);` |
|        - |  4480 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|        - |  4481 | `	 * aborted program can leave entries behind). */` |
|       16 |  4482 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|       16 |  4483 | `	SySetReset(&pVm->aException);` |
|       16 |  4484 | `	SySetReset(&pVm->aFinallyAction);` |
|       16 |  4485 | `	pVm->pPendingException = 0;` |
|       16 |  4486 | `	pVm->pInflightException = 0;` |
|       16 |  4487 | `	pVm->nInflightExcBase = 0;` |
|       16 |  4488 | `	VmClearResumeTarget(&(*pVm));` |
|       16 |  4489 | `	pVm->nBoundaryRc = 0;` |
|       16 |  4490 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       16 |  4491 | `	pVm->pConstEvalClass = 0;` |
|       16 |  4492 | `	pVm->nConstEvalDepth = 0;` |
|       16 |  4493 | `	pVm->pConstCycleAttr = 0;` |
|       16 |  4494 | `	pVm->pConstCycleClass = 0;` |
|       16 |  4495 | `	SySetReset(&pVm->aMagicGuard);` |
|        - |  4496 | `	{` |
|        - |  4497 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|        - |  4498 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|       16 |  4499 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|       16 |  4500 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|        - |  4501 | `		sxu32 iRmw;` |
|       16 |  4502 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|      ! 0 |  4503 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|      ! 0 |  4504 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|      ! 0 |  4505 | `		}` |
|       16 |  4506 | `		SySetReset(&pVm->aHookRmw);` |
|        - |  4507 | `	}` |
|       16 |  4508 | `	if( pVm->pMagicSetThis ){` |
|      ! 0 |  4509 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|      ! 0 |  4510 | `		pVm->pMagicSetThis = 0;` |
|      ! 0 |  4511 | `	}` |
|       16 |  4512 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|       16 |  4513 | `	if( pVm->pHookSetThis ){` |
|      ! 0 |  4514 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|      ! 0 |  4515 | `		pVm->pHookSetThis = 0;` |
|      ! 0 |  4516 | `	}` |
|       16 |  4517 | `	pVm->pHookSetAttr = 0;` |
|       16 |  4518 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|       16 |  4519 | `	if( pVm->pMagicCallThis ){` |
|      ! 0 |  4520 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|      ! 0 |  4521 | `		pVm->pMagicCallThis = 0;` |
|      ! 0 |  4522 | `	}` |
|       16 |  4523 | `	pVm->pMagicCallClass = 0;` |
|       16 |  4524 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|       16 |  4525 | `	pVm->nExceptDepth = 0;` |
|        - |  4526 | `	/* spl_autoload_register() callbacks are per request */` |
|       16 |  4527 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  4528 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  4529 | `		if( pCB ){` |
|      ! 0 |  4530 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  4531 | `		}` |
|      ! 0 |  4532 | `	}` |
|       16 |  4533 | `	SySetReset(&pVm->aAutoload);` |
|        - |  4534 | `	/* ...and so is the extension list they are searched with. */` |
|       16 |  4535 | `	SyBlobReset(&pVm->sAutoloadExt);` |
|       16 |  4536 | `	SyBlobAppend(&pVm->sAutoloadExt,PH7_SPL_AUTOLOAD_EXT,sizeof(PH7_SPL_AUTOLOAD_EXT)-1);` |
|        - |  4537 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  4538 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|       16 |  4539 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  4540 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  4541 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  4542 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      ! 0 |  4543 | `	}` |
|        - |  4544 | `	/* Output buffers */` |
|       16 |  4545 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  4546 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  4547 | `		if( pOb ){` |
|      ! 0 |  4548 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  4549 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  4550 | `		}` |
|      ! 0 |  4551 | `	}` |
|       16 |  4552 | `	SySetReset(&pVm->aOB);` |
|       16 |  4553 | `	pVm->nObDepth = 0;` |
|        - |  4554 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  4555 | `	{` |
|       16 |  4556 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|       16 |  4557 | `		if( rc == SXRET_OK ){` |
|       16 |  4558 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        8 |  4559 | `		}` |
|       16 |  4560 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4561 | `			return rc;` |
|        - |  4562 | `		}` |
|        - |  4563 | `	}` |
|        - |  4564 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|        - |  4565 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|        - |  4566 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|        - |  4567 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|        - |  4568 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|        - |  4569 | `	{` |
|        - |  4570 | `		SyHashEntry *pEntry;` |
|       16 |  4571 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     3524 |  4572 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     3508 |  4573 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  4574 | `			ph7_class_attr *pAttr;` |
|        - |  4575 | `			SyHashEntry *pAttrEntry;` |
|     3508 |  4576 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|    24178 |  4577 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    18916 |  4578 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|    18916 |  4579 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        4 |  4580 | `					pAttr->nIdx = SXU32_HIGH;` |
|        4 |  4581 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|        2 |  4582 | `				}` |
|      ! 0 |  4583 | `			}` |
|        - |  4584 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|        - |  4585 | `			 * slots too so VM reuse re-evaluates them. */` |
|     3508 |  4586 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|    14628 |  4587 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|    11120 |  4588 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|    11120 |  4589 | `				pAttr->nIdx = SXU32_HIGH;` |
|    11120 |  4590 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      ! 0 |  4591 | `			}` |
|      ! 0 |  4592 | `		}` |
|       16 |  4593 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|     3524 |  4594 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     3508 |  4595 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     3508 |  4596 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4597 | `				return rc;` |
|        - |  4598 | `			}` |
|      ! 0 |  4599 | `		}` |
|        - |  4600 | `	}` |
|        - |  4601 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|       16 |  4602 | `	SyBlobReset(&pVm->sConsumer);` |
|       16 |  4603 | `	pVm->nOutputLen = 0;` |
|       16 |  4604 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|       16 |  4605 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        - |  4606 | `	/* 0 is php's "no code set": a CLI run reads FALSE until something sets` |
|        - |  4607 | `	 * one, and a request-driven run is put at 200 when the request arrives. */` |
|       16 |  4608 | `	pVm->iResponseStatus = 0;` |
|       16 |  4609 | `	pVm->bHeadersSent = 0;` |
|       16 |  4610 | `	SyBlobReset(&pVm->sOutStartFile);` |
|       16 |  4611 | `	pVm->nOutStartLine = 0;` |
|       16 |  4612 | `	SyBlobReset(&pVm->sSessStartFile);` |
|       16 |  4613 | `	pVm->nSessStartLine = 0;` |
|       16 |  4614 | `	pVm->bHttpContext = 0;` |
|       16 |  4615 | `	VmReinitMemObj(&(*pVm),&pVm->sExceptionCB);` |
|       16 |  4616 | `	VmReinitMemObj(&(*pVm),&pVm->sErrCB);` |
|       16 |  4617 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|       16 |  4618 | `	VmReleaseHandlerStack(&pVm->aExceptionCBSaved);` |
|       16 |  4619 | `	VmReleaseHandlerStack(&pVm->aErrCBSaved);` |
|       16 |  4620 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        - |  4621 | `	/* The session's userland save handler belongs to the request that installed` |
|        - |  4622 | `	 * it; a reused VM (the -S server's) must not route the next request's store` |
|        - |  4623 | `	 * through the previous script's object. */` |
|       16 |  4624 | `	VmReinitMemObj(&(*pVm),&pVm->sSessHandler);` |
|       16 |  4625 | `	pVm->bSessOpened = 0;` |
|       16 |  4626 | `	SyBlobReset(&pVm->sSessData);` |
|       16 |  4627 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  4628 | `#ifdef PH7_ENABLE_PCRE` |
|       16 |  4629 | `	pVm->iPcreLastError = 0;` |
|        - |  4630 | `#endif` |
|        - |  4631 | `#ifdef PH7_ENABLE_LIBXML` |
|        - |  4632 | `	/* Drop the libxml error queue and the previous request's documents */` |
|       16 |  4633 | `	PH7_LibxmlVmReset(&(*pVm));` |
|        - |  4634 | `#endif` |
|        - |  4635 | `#ifdef PH7_ENABLE_SQLITE` |
|        - |  4636 | `	/* Close the previous request's databases: a reused VM (the -S server's)` |
|        - |  4637 | `	 * must not answer the next request through a handle that request opened. */` |
|       16 |  4638 | `	PH7_PdoVmReset(&(*pVm));` |
|       16 |  4639 | `	PH7_Sqlite3VmReset(&(*pVm));` |
|        - |  4640 | `#endif` |
|        - |  4641 | `#ifdef PH7_ENABLE_CURL` |
|        - |  4642 | `	/* Same rule for the previous request's curl handles, which hold sockets` |
|        - |  4643 | `	 * and a connection cache of their own. */` |
|       16 |  4644 | `	PH7_CurlVmReset(&(*pVm));` |
|        - |  4645 | `#endif` |
|        - |  4646 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|        - |  4647 | `	/* And every ext/sockets descriptor: a reused VM must not leave the previous` |
|        - |  4648 | `	 * request's listener bound to its port. */` |
|       16 |  4649 | `	PH7_SocketsVmReset(&(*pVm));` |
|        - |  4650 | `#endif` |
|        - |  4651 | `	/* php's "last opened directory stream" is per REQUEST: a reused VM must not` |
|        - |  4652 | `	 * let readdir() with no argument reach the previous one's handle. */` |
|       16 |  4653 | `	pVm->pLastDir = 0;` |
|        - |  4654 | `#ifdef PH7_ENABLE_ZLIB` |
|        - |  4655 | `	/* And its deflate/inflate contexts: a z_stream's window is libz's own` |
|        - |  4656 | `	 * allocation, which the wholesale release below would not reach. */` |
|       16 |  4657 | `	PH7_ZlibVmReset(&(*pVm));` |
|       16 |  4658 | `	PH7_ZipVmReset(&(*pVm));` |
|        - |  4659 | `#endif` |
|        - |  4660 | `#ifdef PH7_ENABLE_OPENSSL` |
|        - |  4661 | `	/* And every certificate, key and signing request still held: each is` |
|        - |  4662 | `	 * OpenSSL's own allocation, outside the backend the release below wipes. */` |
|       16 |  4663 | `	PH7_SslVmReset(&(*pVm));` |
|        - |  4664 | `#endif` |
|        - |  4665 | `	/* And every archive it opened: php's phar cache is per-request too. */` |
|       16 |  4666 | `	PH7_PharVmReset(&(*pVm));` |
|        - |  4667 | `	/* Drop the stream contexts this run created, the default one included: a` |
|        - |  4668 | `	 * reused VM (the -S server's) must not answer the next request from the` |
|        - |  4669 | `	 * previous one's stream_context_set_default(). */` |
|       16 |  4670 | `	PH7_StreamCtxVmReset(&(*pVm));` |
|        - |  4671 | `	/* And every filter INSTANCE it created: a chain that was never removed` |
|        - |  4672 | `	 * still owns memory the next request must not inherit. */` |
|       16 |  4673 | `	PH7_StreamFilterVmReset(&(*pVm));` |
|        - |  4674 | `	/* And the last http:// exchange's response headers, for the same reason:` |
|        - |  4675 | `	 * http_get_last_response_headers() must not answer the previous request's. */` |
|       16 |  4676 | `	PH7_HttpClearResponseHeaders(&(*pVm));` |
|       16 |  4677 | `	pVm->iCmpCallbackExc = 0;` |
|       16 |  4678 | `	pVm->bHaltRequested = 0;` |
|       16 |  4679 | `	pVm->iExitStatus = 0;` |
|       16 |  4680 | `	pVm->nSpreadCallBase = 0;` |
|       16 |  4681 | `	VmSpreadCaptureReset(pVm);` |
|       16 |  4682 | `	pVm->nRecursionDepth = 0;` |
|       16 |  4683 | `	pVm->pActiveCtx = 0;` |
|       16 |  4684 | `	pVm->pCurFiber = 0;` |
|       16 |  4685 | `	pVm->pCoalesceObj = 0;` |
|       16 |  4686 | `	pVm->bCoalesceArmed = 0;` |
|       16 |  4687 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  4688 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|        - |  4689 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|       16 |  4690 | `	pVm->nNextObjId = 1;` |
|        - |  4691 | `	/* Set the ready flag */` |
|       16 |  4692 | `	pVm->nMagic = PH7_VM_RUN;` |
|       16 |  4693 | `	return SXRET_OK;` |
|        8 |  4694 | `}` |
|        - |  4695 | `/*` |
|        - |  4696 | ` * Release a Virtual Machine.` |
|        - |  4697 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  4698 | ` */` |
|     6701 |  4699 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 |  4700 | `{` |
|        - |  4701 | `	/* Set the stale magic number */` |
|     6706 |  4702 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  4703 | `#ifdef PH7_ENABLE_LIBXML` |
|        - |  4704 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|        - |  4705 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|     6706 |  4706 | `	PH7_LibxmlVmRelease(pVm);` |
|        - |  4707 | `#endif` |
|        - |  4708 | `#ifdef PH7_ENABLE_SQLITE` |
|        - |  4709 | `	/* Same rule for the sqlite3 handles behind still-open PDO objects. */` |
|     6706 |  4710 | `	PH7_PdoVmRelease(pVm);` |
|     6706 |  4711 | `	PH7_Sqlite3VmRelease(pVm);` |
|        - |  4712 | `#endif` |
|        - |  4713 | `#ifdef PH7_ENABLE_CURL` |
|        - |  4714 | `	/* Same rule for the libcurl handles behind still-open CurlHandle objects. */` |
|     6706 |  4715 | `	PH7_CurlVmRelease(pVm);` |
|        - |  4716 | `#endif` |
|        - |  4717 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|        - |  4718 | `	/* Same rule for the descriptors behind still-open Socket objects. */` |
|     6706 |  4719 | `	PH7_SocketsVmRelease(pVm);` |
|        - |  4720 | `#endif` |
|        - |  4721 | `#ifdef PH7_ENABLE_ZLIB` |
|        - |  4722 | `	/* Same rule for the z_streams behind still-open Deflate/InflateContexts. */` |
|     6706 |  4723 | `	PH7_ZlibVmRelease(pVm);` |
|        - |  4724 | `	/* ...and for the archives behind still-open ZipArchives, whose entry` |
|        - |  4725 | `	 * tables are this allocator's but whose lifetime is not the object's. */` |
|     6706 |  4726 | `	PH7_ZipVmRelease(pVm);` |
|        - |  4727 | `#endif` |
|        - |  4728 | `#ifdef PH7_ENABLE_OPENSSL` |
|        - |  4729 | `	/* Same rule for the X509/EVP_PKEY handles behind still-open objects. */` |
|     6706 |  4730 | `	PH7_SslVmRelease(pVm);` |
|        - |  4731 | `#endif` |
|        - |  4732 | `	/* ext/pcntl put a C signal handler in front of this VM's handler table;` |
|        - |  4733 | `	 * every disposition it took over goes back to SIG_DFL before the allocator` |
|        - |  4734 | `	 * that table lives in disappears. */` |
|     6706 |  4735 | `	PH7_PcntlVmRelease(pVm);` |
|        - |  4736 | `	/* ...and for the syslog prefix a still-open openlog() points at. */` |
|     6706 |  4737 | `	PH7_SyslogVmRelease(pVm);` |
|     6706 |  4738 | `	PH7_PharVmRelease(pVm);` |
|        - |  4739 | `	/* Same rule for the OS directory streams behind still-open directory` |
|        - |  4740 | `	 * iterators: the DIR lives outside the backend. */` |
|     6706 |  4741 | `	PH7_SplDirVmRelease(pVm);` |
|     6706 |  4742 | `	SySetRelease(&pVm->aDeadClosure);` |
|     6706 |  4743 | `	PH7_GcRelease(pVm);` |
|        - |  4744 | `	/* Release the private memory subsystem */` |
|     6706 |  4745 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     6706 |  4746 | `	return SXRET_OK;` |
|        5 |  4747 | `}` |
|        - |  4748 | `/*` |
|        - |  4749 | ` * Initialize a foreign function call context.` |
|        - |  4750 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  4751 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  4752 | ` * functions.` |
|        - |  4753 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  4754 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  4755 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  4756 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  4757 | ` */` |
|  6745059 |  4758 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|        - |  4759 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  4760 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  4761 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  4762 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  4763 | `	sxi32 iFlags          /* Control flags */` |
|        - |  4764 | `	)` |
|        5 |  4765 | `{` |
|  6745064 |  4766 | `	pOut->pFunc = pFunc;` |
|  6745064 |  4767 | `	pOut->pVm   = pVm;` |
|  6745064 |  4768 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|  6745064 |  4769 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  4770 | `	/* Assume a null return value */` |
|  6745064 |  4771 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|  6745064 |  4772 | `	pOut->pRet = pRet;` |
|  6745064 |  4773 | `	pOut->iFlags = iFlags;` |
|  6745064 |  4774 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|  6745064 |  4775 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|        - |  4776 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|        - |  4777 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|        - |  4778 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|        - |  4779 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|  6745064 |  4780 | `	pOut->pThis = 0;` |
|  6745064 |  4781 | `	pOut->pCalledClass = 0;` |
|  6745064 |  4782 | `	pOut->bThisInit = 0;` |
|        - |  4783 | `	/* Only the scratch context a native PROPERTY handler runs on carries one; every` |
|        - |  4784 | `	 * ordinary call leaves it empty, so a refusal there throws as it always did. */` |
|  6745064 |  4785 | `	pOut->pPropCtx = 0;` |
|  6745064 |  4786 | `	return SXRET_OK;` |
|        5 |  4787 | `}` |
|        - |  4788 | `/*` |
|        - |  4789 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  4790 | ` * left behind.` |
|        - |  4791 | ` */` |
|  6745075 |  4792 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 |  4793 | `{` |
|        - |  4794 | `	sxu32 n;` |
|  6745080 |  4795 | `	if( pCtx->bThisInit ){` |
|        - |  4796 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|        - |  4797 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|        - |  4798 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|        - |  4799 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|     9449 |  4800 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|     9449 |  4801 | `		pCtx->sThis.x.pOther = 0;` |
|     9449 |  4802 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|     9449 |  4803 | `		pCtx->bThisInit = 0;` |
|     4722 |  4804 | `	}` |
|  6745080 |  4805 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|   853091 |  4806 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|  2158520 |  4807 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|  1305434 |  4808 | `			if( apObj[n] == 0 ){` |
|        - |  4809 | `				/* Already released */` |
|    58566 |  4810 | `				continue;` |
|        - |  4811 | `			}` |
|  1246873 |  4812 | `			PH7_MemObjRelease(apObj[n]);` |
|  1246873 |  4813 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|   623340 |  4814 | `		}` |
|   853091 |  4815 | `		SySetRelease(&pCtx->sVar);` |
|   426505 |  4816 | `	}` |
|  6745080 |  4817 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  4818 | `		ph7_aux_data *aAux;` |
|        - |  4819 | `		void *pChunk;` |
|        - |  4820 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  4821 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  4822 | `		 */` |
|     8057 |  4823 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|    31396 |  4824 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|    23344 |  4825 | `			pChunk = aAux[n].pAuxData;` |
|        - |  4826 | `			/* Release the chunk */` |
|    23344 |  4827 | `			if( pChunk ){` |
|    22640 |  4828 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|    11174 |  4829 | `			}` |
|    11531 |  4830 | `		}` |
|     8057 |  4831 | `		SySetRelease(&pCtx->sChunk);` |
|     3978 |  4832 | `	}` |
|  6745080 |  4833 | `}` |
|        - |  4834 | `/*` |
|        - |  4835 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  4836 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  4837 | ` */` |
|    58561 |  4838 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  4839 | `	ph7_context *pCtx, /* Call context */` |
|        - |  4840 | `	ph7_value *pValue  /* Release this value */` |
|        - |  4841 | `	)` |
|        5 |  4842 | `{` |
|    58566 |  4843 | `	if( pValue == 0 ){` |
|        - |  4844 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  4845 | `		return;` |
|        - |  4846 | `	}` |
|    58566 |  4847 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    58566 |  4848 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  4849 | `		sxu32 n;` |
|   946342 |  4850 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|   946342 |  4851 | `			if( apObj[n] == pValue ){` |
|    58566 |  4852 | `				PH7_MemObjRelease(pValue);` |
|    58566 |  4853 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  4854 | `				/* Mark as released */` |
|    58566 |  4855 | `				apObj[n] = 0;` |
|    58566 |  4856 | `				break;` |
|        - |  4857 | `			}` |
|   443217 |  4858 | `		}` |
|    29239 |  4859 | `	}` |
|    29244 |  4860 | `}` |
|        - |  4861 | `/*` |
|        - |  4862 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  4863 | ` */` |
| 28761962 |  4864 | `PH7_PRIVATE void VmPopOperand(` |
|        - |  4865 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  4866 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  4867 | `	)` |
|        5 |  4868 | `{` |
| 28761967 |  4869 | `	ph7_value *pTos = *ppTos;` |
| 62168198 |  4870 | `	while( nPop > 0 ){` |
| 33406236 |  4871 | `		PH7_MemObjRelease(pTos);` |
| 33406236 |  4872 | `		pTos--;` |
| 33406236 |  4873 | `		nPop--;` |
|        5 |  4874 | `	}` |
|        - |  4875 | `	/* Top of the stack */` |
| 28761967 |  4876 | `	*ppTos = pTos;` |
| 28761967 |  4877 | `}` |
|        - |  4878 | `/*` |
|        - |  4879 | ` * Reserve a memory object.` |
|        - |  4880 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  4881 | ` */` |
| 24097156 |  4882 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 |  4883 | `{` |
| 24097161 |  4884 | `	ph7_value *pObj = 0;` |
|        - |  4885 | `	sxu32 nIdx;` |
|        - |  4886 | `	/* Check for a free slot. The head is a slot index, and the freed slot's own` |
|        - |  4887 | `	 * (dead) nIdx word holds the next one -- one load past the bounds test, the` |
|        - |  4888 | `	 * same shape as the SySetPop of the stack this replaced. The PH7_MemObjInit` |
|        - |  4889 | `	 * below is what takes MEMOBJ_POOLFREE back off: every acquire runs it, so the` |
|        - |  4890 | `	 * bit means "on the list" and nothing else. */` |
| 24097161 |  4891 | `	nIdx = SXU32_HIGH; /* cc warning */` |
| 24097161 |  4892 | `	if( pVm->aMemObj.nFreeHead != SXU32_HIGH ){` |
| 20980740 |  4893 | `		nIdx = pVm->aMemObj.nFreeHead;` |
| 20980740 |  4894 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
| 20980740 |  4895 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_POOLFREE) == 0 ){` |
|        - |  4896 | `			/* Stale or corrupted chain (defensive -- truncate clears the head and` |
|        - |  4897 | `			 * clears the bit): abandon it rather than hand out a LIVE slot. */` |
|      ! 0 |  4898 | `			pVm->aMemObj.nFreeHead = SXU32_HIGH;` |
|      ! 0 |  4899 | `			pObj = 0;` |
|      ! 0 |  4900 | `		}else{` |
| 20980740 |  4901 | `			pVm->aMemObj.nFreeHead = pObj->nIdx;` |
|        - |  4902 | `		}` |
| 10489917 |  4903 | `	}` |
| 24097161 |  4904 | `	if( pObj == 0 ){` |
|        - |  4905 | `		/* Reserve a new memory object */` |
|  3116426 |  4906 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  3116426 |  4907 | `		if( pObj == 0 ){` |
|      ! 0 |  4908 | `			return 0;` |
|        - |  4909 | `		}` |
|  1557368 |  4910 | `	}` |
|        - |  4911 | `	/* Set a null default value */` |
| 24097161 |  4912 | `	PH7_MemObjInit(&(*pVm),pObj);` |
| 24097161 |  4913 | `	pObj->nIdx = nIdx;` |
| 24097161 |  4914 | `	return pObj;` |
| 12047290 |  4915 | `}` |
|        - |  4916 | `/*` |
|        - |  4917 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  4918 | ` */` |
|    92771 |  4919 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|        - |  4920 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  4921 | `	const char *zKey,  /* Entry key */` |
|        - |  4922 | `	sxu32 nByte,       /* Key length */` |
|        - |  4923 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  4924 | `	)` |
|        5 |  4925 | `{` |
|        - |  4926 | `	ph7_value sKey;` |
|        - |  4927 | `	sxi32 rc;` |
|    92776 |  4928 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    92776 |  4929 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  4930 | `	/* Perform the insertion */` |
|    92776 |  4931 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    92776 |  4932 | `	PH7_MemObjRelease(&sKey);` |
|    92776 |  4933 | `	return rc;` |
|        5 |  4934 | `}` |
|        - |  4935 | `/*` |
|        - |  4936 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|        - |  4937 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|        - |  4938 | ` * key must create a real global variable — linked into the bottom frame's` |
|        - |  4939 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|        - |  4940 | ` * variable created by top-level code — so later reads and writes alias one` |
|        - |  4941 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|        - |  4942 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|        - |  4943 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|        - |  4944 | ` *     superglobal in place.` |
|        - |  4945 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|        - |  4946 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|        - |  4947 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|        - |  4948 | ` *     a plain variable.` |
|        - |  4949 | ` */` |
|      260 |  4950 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|        5 |  4951 | `{` |
|      265 |  4952 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  4953 | `	SyHashEntry *pEntry;` |
|        - |  4954 | `	ph7_value *pObj;` |
|        - |  4955 | `	char *zDup;` |
|        - |  4956 | `	sxu32 nIdx;` |
|        - |  4957 | `	sxi32 rc;` |
|        - |  4958 | `	/* Walk down to the global frame */` |
|      331 |  4959 | `	while( pFrame->pParent ){` |
|       70 |  4960 | `		pFrame = pFrame->pParent;` |
|        4 |  4961 | `	}` |
|        - |  4962 | `	/* An existing global (or superglobal) is overwritten in place */` |
|      265 |  4963 | `	pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|      265 |  4964 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|        - |  4965 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|        - |  4966 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|        - |  4967 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|        - |  4968 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|        - |  4969 | `		 * of $GLOBALS itself). */` |
|        5 |  4970 | `		pEntry = 0;` |
|        2 |  4971 | `	}` |
|      265 |  4972 | `	if( pEntry == 0 ){` |
|      265 |  4973 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|      130 |  4974 | `	}` |
|      265 |  4975 | `	if( pEntry ){` |
|       42 |  4976 | `		if( nRefIdx != SXU32_HIGH ){` |
|        - |  4977 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|        - |  4978 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|       40 |  4979 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|       40 |  4980 | `			return SXRET_OK;` |
|        - |  4981 | `		}` |
|        3 |  4982 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|        3 |  4983 | `		if( pObj == 0 ){` |
|      ! 0 |  4984 | `			return SXERR_NOTFOUND;` |
|        - |  4985 | `		}` |
|        3 |  4986 | `		if( pValue ){` |
|        3 |  4987 | `			PH7_MemObjStore(pValue,pObj);` |
|        2 |  4988 | `		}else{` |
|      ! 0 |  4989 | `			PH7_MemObjToNull(pObj);` |
|        - |  4990 | `		}` |
|        3 |  4991 | `		return SXRET_OK;` |
|        - |  4992 | `	}` |
|      225 |  4993 | `	if( nRefIdx == SXU32_HIGH ){` |
|        - |  4994 | `		/* Reserve a fresh slot for the new global */` |
|      219 |  4995 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|      219 |  4996 | `		if( pObj == 0 ){` |
|      ! 0 |  4997 | `			return SXERR_MEM;` |
|        - |  4998 | `		}` |
|      219 |  4999 | `		nIdx = pObj->nIdx;` |
|      112 |  5000 | `	}else{` |
|        - |  5001 | `		/* Reference assignment: bind the name to the existing slot */` |
|        7 |  5002 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nRefIdx);` |
|        7 |  5003 | `		if( pObj == 0 ){` |
|      ! 0 |  5004 | `			return SXERR_NOTFOUND;` |
|        - |  5005 | `		}` |
|        7 |  5006 | `		nIdx = nRefIdx;` |
|        - |  5007 | `	}` |
|      225 |  5008 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|      225 |  5009 | `	if( zDup == 0 ){` |
|      ! 0 |  5010 | `		if( nRefIdx == SXU32_HIGH ){` |
|        - |  5011 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|        - |  5012 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|      ! 0 |  5013 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|      ! 0 |  5014 | `		}` |
|      ! 0 |  5015 | `		return SXERR_MEM;` |
|        - |  5016 | `	}` |
|      225 |  5017 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|      225 |  5018 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5019 | `		if( nRefIdx == SXU32_HIGH ){` |
|      ! 0 |  5020 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|      ! 0 |  5021 | `		}` |
|      ! 0 |  5022 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  5023 | `		return rc;` |
|        - |  5024 | `	}` |
|        - |  5025 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|      225 |  5026 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|      225 |  5027 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      225 |  5028 | `	if( nRefIdx == SXU32_HIGH ){` |
|      219 |  5029 | `		pObj->nIdx = nIdx;` |
|      219 |  5030 | `		if( pValue ){` |
|      204 |  5031 | `			PH7_MemObjStore(pValue,pObj);` |
|      100 |  5032 | `		}` |
|      107 |  5033 | `	}` |
|      225 |  5034 | `	return SXRET_OK;` |
|      135 |  5035 | `}` |
|        - |  5036 | `/*` |
|        - |  5037 | ` * Extract a variable value from the top active VM frame.` |
|        - |  5038 | ` * Return a pointer to the variable value on success.` |
|        - |  5039 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  5040 | ` *` |
|        - |  5041 | ` * pnIdx, when given, receives the SLOT the name resolved to -- which the value's own` |
|        - |  5042 | ` * nIdx does not always carry (a superglobal's does not), and which the caller cannot` |
|        - |  5043 | ` * ask for afterwards without repeating the lookup this function just did.` |
|        - |  5044 | ` */` |
|  5654381 |  5045 | `static ph7_value * VmExtractMemObjEx(` |
|        - |  5046 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  5047 | `	const SyString *pName, /* Variable name */` |
|        - |  5048 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  5049 | `	int bCreate,           /* True to create the variable if non-existent */` |
|        - |  5050 | `	sxu32 *pnIdx           /* OUT: the slot the name is bound to (may be NULL) */` |
|        - |  5051 | `	)` |
|        5 |  5052 | `{` |
|  5654386 |  5053 | `	int bNullify = FALSE;` |
|        - |  5054 | `	SyHashEntry *pEntry;` |
|        - |  5055 | `	VmFrame *pFrame;` |
|        - |  5056 | `	ph7_value *pObj;` |
|        - |  5057 | `	sxu32 nIdx;` |
|        - |  5058 | `	sxi32 rc;` |
|        - |  5059 | `	/* Point to the top active frame */` |
|  5654386 |  5060 | `	pFrame = pVm->pFrame;` |
|  5654386 |  5061 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  5062 | `	/* Perform the lookup */` |
|  5654386 |  5063 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  5064 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|       18 |  5065 | `		pName = &sAnnon;` |
|        - |  5066 | `		/* Always nullify the object */` |
|       18 |  5067 | `		bNullify = TRUE;` |
|       18 |  5068 | `		bDup = FALSE;` |
|        8 |  5069 | `	}` |
|        - |  5070 | `	/* Check the superglobals table first */` |
|  5654386 |  5071 | `	pEntry = PH7_VmSuperGet(&(*pVm),pName->zString,pName->nByte);` |
|  5654386 |  5072 | `	if( pEntry == 0 ){` |
|        - |  5073 | `		/* Query the top active frame */` |
|  5652740 |  5074 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  5652740 |  5075 | `		if( pEntry == 0 ){` |
|  1399950 |  5076 | `			char *zName = (char *)pName->zString;` |
|        - |  5077 | `			VmSlot sLocal;` |
|  1399950 |  5078 | `			if( !bCreate ){` |
|        - |  5079 | `				/* Do not create the variable,return NULL instead */` |
|    19186 |  5080 | `				return 0;` |
|        - |  5081 | `			}` |
|        - |  5082 | `			/* No such variable,automatically create a new one and install` |
|        - |  5083 | `			 * it in the current frame.` |
|        - |  5084 | `			 */` |
|  1380769 |  5085 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|  1380769 |  5086 | `			if( pObj == 0 ){` |
|      ! 0 |  5087 | `				return 0;` |
|        - |  5088 | `			}` |
|  1380769 |  5089 | `			nIdx = pObj->nIdx;` |
|  1380769 |  5090 | `			if( bDup ){` |
|        - |  5091 | `				/* Duplicate name */` |
|    14579 |  5092 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|    14579 |  5093 | `				if( zName == 0 ){` |
|      ! 0 |  5094 | `					return 0;` |
|        - |  5095 | `				}` |
|     7247 |  5096 | `			}` |
|        - |  5097 | `			/* Link to the top active VM frame */` |
|  1380769 |  5098 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|  1380769 |  5099 | `			if( rc != SXRET_OK ){` |
|        - |  5100 | `				/* Return the slot to the free pool */` |
|      ! 0 |  5101 | `				VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|      ! 0 |  5102 | `				return 0;` |
|        - |  5103 | `			}` |
|  1380769 |  5104 | `			if( pFrame->pParent != 0 ){` |
|        - |  5105 | `				/* Local variable */` |
|  1361740 |  5106 | `				sLocal.nIdx = nIdx;` |
|  1361740 |  5107 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|   702783 |  5108 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|        - |  5109 | `				/* Register in the $GLOBALS array */` |
|    18751 |  5110 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|     9337 |  5111 | `			}` |
|        - |  5112 | `			/* Install in the reference table */` |
|  1380769 |  5113 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  5114 | `			/* Save object index */` |
|  1380769 |  5115 | `			pObj->nIdx = nIdx;` |
|   693232 |  5116 | `		}else{` |
|        - |  5117 | `			/* Extract variable contents */` |
|  4252795 |  5118 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  4252795 |  5119 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  4252795 |  5120 | `			if( bNullify && pObj ){` |
|        3 |  5121 | `				PH7_MemObjRelease(pObj);` |
|        1 |  5122 | `			}` |
|        - |  5123 | `		}` |
|  2819361 |  5124 | `	}else{` |
|        - |  5125 | `		/* Superglobal */` |
|     1651 |  5126 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|     1651 |  5127 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|        - |  5128 | `	}` |
|  5635205 |  5129 | `	if( pnIdx ){` |
|  2753056 |  5130 | `		*pnIdx = nIdx;` |
|  1379215 |  5131 | `	}` |
|  5635205 |  5132 | `	return pObj;` |
|  2829650 |  5133 | `}` |
|  2900872 |  5134 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|        - |  5135 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  5136 | `	const SyString *pName, /* Variable name */` |
|        - |  5137 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  5138 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  5139 | `	)` |
|        5 |  5140 | `{` |
|  2900877 |  5141 | `	return VmExtractMemObjEx(&(*pVm),pName,bDup,bCreate,0);` |
|        5 |  5142 | `}` |
|        - |  5143 | `/*` |
|        - |  5144 | ` * Number a body's variables, once, from the body itself.` |
|        - |  5145 | ` *` |
|        - |  5146 | ` * Every instruction that names a variable the compiler wrote down -- OP_LOAD and` |
|        - |  5147 | ` * OP_STORE with a p3 -- gets a small NUMBER in its nSite, and the running frame then` |
|        - |  5148 | ` * answers that number out of an array instead of hashing the name (see VmFrame's` |
|        - |  5149 | ` * aLocalSlot). Two instructions naming the same variable get the same number, so the` |
|        - |  5150 | ` * frame holds one entry per NAME and not one per site.` |
|        - |  5151 | ` *` |
|        - |  5152 | ` * Names are compared by ADDRESS, which is exact and not an approximation: the compiler` |
|        - |  5153 | ` * interns every variable name it emits into one VM-lifetime buffer (pGen->hVar), so` |
|        - |  5154 | ` * within a body the same spelling is the same pointer. Two pointers for one spelling` |
|        - |  5155 | ` * would only cost a body two numbers for one name, which stays correct -- both entries` |
|        - |  5156 | ` * hold the same slot and both are emptied together.` |
|        - |  5157 | ` *` |
|        - |  5158 | ` * A body with more distinct names than PH7_VAR_SLOT_MAX numbers its most REFERENCED` |
|        - |  5159 | ` * ones: the pass counts static references first and hands the numbers out in that` |
|        - |  5160 | ` * order, so what a hot loop reads is what fits. The rest keep nSite = 0 and take the` |
|        - |  5161 | ` * hash path, exactly as every site did before this existed.` |
|        - |  5162 | ` *` |
|        - |  5163 | ` * Lazy and self-computing, like nMaxStack: a body that has not been walked yet just` |
|        - |  5164 | ` * walks. There is no path that can produce a WRONG number -- an unwalked body has 0` |
|        - |  5165 | ` * everywhere, which means "ask the table".` |
|        - |  5166 | ` */` |
|        - |  5167 | `#define VM_LOCAL_SCAN_MAX 32   /* distinct names the pass will rank; past this it stops` |
|        - |  5168 | `                                * counting and numbers what it has. A body naming more` |
|        - |  5169 | `                                * than this has long since stopped fitting the frame, and` |
|        - |  5170 | `                                * the three arrays below are C STACK -- 352 bytes at this` |
|        - |  5171 | `                                * width, which matters on a 16-frame embedded target. */` |
|  1389571 |  5172 | `static int VmInstrNamesVar(const VmInstr *pInstr)` |
|        5 |  5173 | `{` |
|  1389576 |  5174 | `	return ( pInstr->iOp == PH7_OP_LOAD \|\| pInstr->iOp == PH7_OP_STORE ) && pInstr->p3 != 0;` |
|        5 |  5175 | `}` |
|    31146 |  5176 | `static void VmNumberLocals(VmInstr *aInstr,sxu32 nInstr,sxu16 *pnName)` |
|        5 |  5177 | `{` |
|        - |  5178 | `	const char *azName[VM_LOCAL_SCAN_MAX];` |
|        - |  5179 | `	sxu16 aRef[VM_LOCAL_SCAN_MAX];   /* references, then re-used as name -> number+1 */` |
|        - |  5180 | `	sxu8 aRank[VM_LOCAL_SCAN_MAX];` |
|    31151 |  5181 | `	sxu32 nName = 0;` |
|        - |  5182 | `	sxu32 i,j,n;` |
|    31151 |  5183 | `	*pnName = 0;` |
|    31151 |  5184 | `	if( aInstr == 0 ){` |
|      ! 0 |  5185 | `		return;` |
|        - |  5186 | `	}` |
|        - |  5187 | `	/* Pass one: the distinct names, and how many instructions reach for each. */` |
|   783089 |  5188 | `	for( i = 0 ; i < nInstr ; ++i ){` |
|        - |  5189 | `		const char *zName;` |
|   751943 |  5190 | `		if( !VmInstrNamesVar(&aInstr[i]) ){` |
|   656809 |  5191 | `			continue;` |
|        - |  5192 | `		}` |
|    95139 |  5193 | `		zName = (const char *)aInstr[i].p3;` |
|        - |  5194 | `		/* The length belongs to the name and not to the execution: measure it here,` |
|        - |  5195 | `		 * once, for the handlers that used to call SyStrlen on every pass. */` |
|    95139 |  5196 | `		if( aInstr[i].nAux == 0 ){` |
|    89351 |  5197 | `			aInstr[i].nAux = (sxu32)SyStrlen(zName);` |
|    44333 |  5198 | `		}` |
|   445578 |  5199 | `		for( j = 0 ; j < nName ; ++j ){` |
|   405206 |  5200 | `			if( azName[j] == zName ){` |
|    54767 |  5201 | `				if( aRef[j] < SXU16_HIGH ){` |
|    54767 |  5202 | `					aRef[j]++;   /* a count that saturates still ranks first */` |
|    27154 |  5203 | `				}` |
|    54767 |  5204 | `				break;` |
|        - |  5205 | `			}` |
|   173490 |  5206 | `		}` |
|    95139 |  5207 | `		if( j == nName ){` |
|    40377 |  5208 | `			if( nName >= VM_LOCAL_SCAN_MAX ){` |
|     1203 |  5209 | `				continue;` |
|        - |  5210 | `			}` |
|    39179 |  5211 | `			azName[nName] = zName;` |
|    39179 |  5212 | `			aRef[nName] = 1;` |
|    39179 |  5213 | `			nName++;` |
|    19462 |  5214 | `		}` |
|    46621 |  5215 | `	}` |
|    31151 |  5216 | `	if( nName < 1 ){` |
|    12047 |  5217 | `		return;` |
|        - |  5218 | `	}` |
|        - |  5219 | `	/* Rank by static reference count, first appearance breaking ties -- an insertion` |
|        - |  5220 | `	 * sort over at most VM_LOCAL_SCAN_MAX entries, run once per body. aRank[k] is the` |
|        - |  5221 | `	 * name that gets number k. */` |
|    58283 |  5222 | `	for( i = 0 ; i < nName ; ++i ){` |
|    65682 |  5223 | `		for( j = i ; j > 0 && aRef[aRank[j-1]] < aRef[i] ; --j ){` |
|    26508 |  5224 | `			aRank[j] = aRank[j-1];` |
|    13117 |  5225 | `		}` |
|        - |  5226 | `		/* aRank holds name INDICES, so VM_LOCAL_SCAN_MAX must fit an sxu8. */` |
|    39179 |  5227 | `		aRank[j] = (sxu8)i;` |
|    19467 |  5228 | `	}` |
|    19109 |  5229 | `	n = nName > PH7_VAR_SLOT_MAX ? PH7_VAR_SLOT_MAX : nName;` |
|        - |  5230 | `	/* aRef is re-used as name -> number+1, so pass two is a single lookup. */` |
|    58283 |  5231 | `	for( i = 0 ; i < nName ; ++i ){` |
|    39179 |  5232 | `		aRef[i] = 0;` |
|    19467 |  5233 | `	}` |
|    58225 |  5234 | `	for( i = 0 ; i < n ; ++i ){` |
|    39121 |  5235 | `		aRef[aRank[i]] = (sxu16)(i + 1);` |
|    19438 |  5236 | `	}` |
|        - |  5237 | `	/* Pass two: stamp the number on every instruction that names one. */` |
|   656742 |  5238 | `	for( i = 0 ; i < nInstr ; ++i ){` |
|        - |  5239 | `		const char *zName;` |
|   637638 |  5240 | `		if( !VmInstrNamesVar(&aInstr[i]) ){` |
|   542504 |  5241 | `			continue;` |
|        - |  5242 | `		}` |
|    95139 |  5243 | `		zName = (const char *)aInstr[i].p3;` |
|   445578 |  5244 | `		for( j = 0 ; j < nName ; ++j ){` |
|   444380 |  5245 | `			if( azName[j] == zName ){` |
|    93941 |  5246 | `				aInstr[i].nSite = aRef[j];` |
|    93941 |  5247 | `				break;` |
|        - |  5248 | `			}` |
|   173490 |  5249 | `		}` |
|    47220 |  5250 | `	}` |
|    19109 |  5251 | `	*pnName = (sxu16)n;` |
|    15444 |  5252 | `}` |
|        - |  5253 | `/*` |
|        - |  5254 | ` * Number a function body if it has not been numbered, and tell the frame about to run` |
|        - |  5255 | ` * it which body its numbers belong to. One branch per activation; the walk itself` |
|        - |  5256 | ` * happens once per function for the life of the VM.` |
|        - |  5257 | ` */` |
|   831643 |  5258 | `static void VmFrameNumberBody(VmFrame *pFrame,ph7_vm_func *pFunc)` |
|        5 |  5259 | `{` |
|   831648 |  5260 | `	if( !pFunc->bNumbered ){` |
|    36733 |  5261 | `		VmNumberLocals((VmInstr *)SySetBasePtr(&pFunc->aByteCode),` |
|    12157 |  5262 | `			SySetUsed(&pFunc->aByteCode),&pFunc->nLocalName);` |
|    24576 |  5263 | `		pFunc->bNumbered = 1;` |
|    12157 |  5264 | `	}` |
|   831648 |  5265 | `	if( pFunc->nLocalName > 0 ){` |
|   200636 |  5266 | `		pFrame->pCodeBase = (const VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|   100616 |  5267 | `	}` |
|   831648 |  5268 | `}` |
|        - |  5269 | `/*` |
|        - |  5270 | ` * Forget where this frame's variables live. Called from the doors that can move a NAME` |
|        - |  5271 | ` * to a different slot; a door that only ever INSTALLS a name the frame did not have` |
|        - |  5272 | ` * does not need it, because a lookup that found nothing is never filed.` |
|        - |  5273 | ` */` |
|    92551 |  5274 | `PH7_PRIVATE void VmVarMemoFlush(VmFrame *pFrame)` |
|        5 |  5275 | `{` |
|        - |  5276 | `	sxu32 i;` |
|  2683984 |  5277 | `	for( i = 0 ; i < PH7_VAR_SLOT_MAX ; ++i ){` |
|  2591433 |  5278 | `		pFrame->aLocalSlot[i] = 0;` |
|  1293829 |  5279 | `	}` |
|    92556 |  5280 | `}` |
|        - |  5281 | `/*` |
|        - |  5282 | ` * VmExtractMemObj for a name the CALLER guarantees outlives the lookup -- a variable` |
|        - |  5283 | ` * name the compiler interned into the bytecode, and nothing else.` |
|        - |  5284 | ` *` |
|        - |  5285 | ` * Every variable access consults the superglobal table and then hashes the name into` |
|        - |  5286 | ` * the frame's symbol table; measured on the ecosystem gate's phpcs step that was the` |
|        - |  5287 | ` * largest single row in the engine's whole name-lookup census, and the hash is over a` |
|        - |  5288 | ` * name whose answer cannot change between two accesses in the same frame unless` |
|        - |  5289 | ` * something re-binds it. So the answer is remembered on the frame, BY NUMBER (see` |
|        - |  5290 | ` * VmFrame's aLocalSlot), and the second and later reads of a variable inside one` |
|        - |  5291 | ` * activation cost an array index.` |
|        - |  5292 | ` *` |
|        - |  5293 | ` * nSlot is the number the body gave this name plus one, and aCode the instruction` |
|        - |  5294 | ` * array it was numbered in -- 0 for a caller that has neither, which then pays the` |
|        - |  5295 | ` * lookup it always did. The aCode compare is what keeps an included unit, an eval and` |
|        - |  5296 | ` * a default-argument mini-program from reading numbers that are not theirs: they share` |
|        - |  5297 | ` * the frame, so their instructions must not index its array.` |
|        - |  5298 | ` *` |
|        - |  5299 | ` * bDup is deliberately absent: a name that has to be COPIED to become a symbol-table` |
|        - |  5300 | ` * key is by definition not one that outlives the lookup.` |
|        - |  5301 | ` */` |
| 17658818 |  5302 | `PH7_PRIVATE ph7_value * PH7_VmExtractVarSlot(` |
|        - |  5303 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  5304 | `	const SyString *pName, /* Variable name -- interned, NUL-terminated, VM-lifetime */` |
|        - |  5305 | `	int bCreate,           /* True to create the variable if non-existent */` |
|        - |  5306 | `	sxu32 nSlot,           /* The body's number for this name, plus one (0 = none) */` |
|        - |  5307 | `	const VmInstr *aCode   /* The instruction array nSlot was numbered in */` |
|        - |  5308 | `	)` |
|        5 |  5309 | `{` |
|        - |  5310 | `	VmFrame *pFrame;` |
|        - |  5311 | `	ph7_value *pObj;` |
|        - |  5312 | `	sxu32 nIdx;` |
| 17658823 |  5313 | `	if( pName->nByte < 1 \|\| pName->zString == 0 ){` |
|      ! 0 |  5314 | `		return VmExtractMemObjEx(&(*pVm),pName,FALSE,bCreate,0);` |
|        - |  5315 | `	}` |
| 17658823 |  5316 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
| 18138541 |  5317 | `	if( nSlot > 0 && pFrame->pCodeBase == aCode ){` |
| 15870552 |  5318 | `		sxu32 nCached = pFrame->aLocalSlot[nSlot - 1];` |
| 15870552 |  5319 | `		if( nCached > 0 ){` |
| 14905314 |  5320 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCached - 1);` |
| 14905314 |  5321 | `			if( pObj ){` |
| 14905314 |  5322 | `				return pObj;` |
|        - |  5323 | `			}` |
|      ! 0 |  5324 | `		}` |
|   485525 |  5325 | `	}else{` |
|  1788276 |  5326 | `		nSlot = 0;` |
|        - |  5327 | `	}` |
|  2753514 |  5328 | `	nIdx = SXU32_HIGH;` |
|  2753514 |  5329 | `	pObj = VmExtractMemObjEx(&(*pVm),pName,FALSE,bCreate,&nIdx);` |
|  2753514 |  5330 | `	if( pObj && nSlot > 0 && nIdx != SXU32_HIGH ){` |
|        - |  5331 | `		/* VmExtractMemObjEx may have grown the frame chain's tables, but never the` |
|        - |  5332 | `		 * chain itself, so the frame the answer belongs to is still this one. */` |
|   964905 |  5333 | `		pFrame->aLocalSlot[nSlot - 1] = nIdx + 1;` |
|   485351 |  5334 | `	}` |
|  2753514 |  5335 | `	return pObj;` |
|  8845865 |  5336 | `}` |
|        - |  5337 | `/*` |
|        - |  5338 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  5339 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  5340 | ` */` |
|    60097 |  5341 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  5342 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  5343 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  5344 | `	sxu32 nByte        /* zName length */` |
|        - |  5345 | `	)` |
|        5 |  5346 | `{` |
|        - |  5347 | `	SyHashEntry *pEntry;` |
|        - |  5348 | `	ph7_value *pValue;` |
|        - |  5349 | `	sxu32 nIdx;` |
|        - |  5350 | `	/* Query the superglobal table */` |
|    60102 |  5351 | `	pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|    60102 |  5352 | `	if( pEntry == 0 ){` |
|        - |  5353 | `		/* No such entry */` |
|      ! 0 |  5354 | `		return 0;` |
|        - |  5355 | `	}` |
|        - |  5356 | `	/* Extract the superglobal index in the global object pool */` |
|    60102 |  5357 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  5358 | `	/* Extract the variable value  */` |
|    60102 |  5359 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|    60102 |  5360 | `	return pValue;` |
|    30004 |  5361 | `}` |
|        - |  5362 | `/*` |
|        - |  5363 | ` * Perform a raw hashmap insertion.` |
|        - |  5364 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  5365 | ` */` |
|    46867 |  5366 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  5367 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  5368 | `	const char *zKey,   /* Entry key */` |
|        - |  5369 | `	int nKeylen,        /* zKey length*/` |
|        - |  5370 | `	const char *zData,  /* Entry data */` |
|        - |  5371 | `	int nLen            /* zData length */` |
|        - |  5372 | `	)` |
|        5 |  5373 | `{` |
|        - |  5374 | `	ph7_value sKey,sValue;` |
|        - |  5375 | `	sxi32 rc;` |
|    46872 |  5376 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    46872 |  5377 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|    46872 |  5378 | `	if( zKey ){` |
|    40267 |  5379 | `		if( nKeylen < 0 ){` |
|    40093 |  5380 | `			nKeylen = (int)SyStrlen(zKey);` |
|    20011 |  5381 | `		}` |
|    40267 |  5382 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|    20098 |  5383 | `	}` |
|    46872 |  5384 | `	if( zData ){` |
|    46872 |  5385 | `		if( nLen < 0 ){` |
|        - |  5386 | `			/* Compute length automatically */` |
|    26533 |  5387 | `			nLen = (int)SyStrlen(zData);` |
|    13242 |  5388 | `		}` |
|    46872 |  5389 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|    23395 |  5390 | `	}` |
|        - |  5391 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|        - |  5392 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|        - |  5393 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|        - |  5394 | `	 * every argument under "". */` |
|    46872 |  5395 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|    46872 |  5396 | `	PH7_MemObjRelease(&sKey);` |
|    46872 |  5397 | `	PH7_MemObjRelease(&sValue);` |
|    46872 |  5398 | `	return rc;` |
|        5 |  5399 | `}` |
|        - |  5400 | `/*` |
|        - |  5401 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|        - |  5402 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|        - |  5403 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|        - |  5404 | ` */` |
|      166 |  5405 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|        4 |  5406 | `{` |
|      170 |  5407 | `	sxi64 iVal = 0;` |
|      170 |  5408 | `	if( nValue == 0 ){` |
|      ! 0 |  5409 | `		return 0;` |
|        - |  5410 | `	}` |
|      166 |  5411 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|      166 |  5412 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|      170 |  5413 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|      ! 0 |  5414 | `		return 1;` |
|        - |  5415 | `	}` |
|      170 |  5416 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|      170 |  5417 | `	return iVal != 0;` |
|       87 |  5418 | `}` |
|        - |  5419 | `/*` |
|        - |  5420 | ` * php's php.ini VALUE grammar, over one directive's value.` |
|        - |  5421 | ` *` |
|        - |  5422 | ` * An ini value is not a literal. zend_ini_parser reads it as an expression over` |
|        - |  5423 | `` * `\|`, `&`, `^`, unary `~` and `!` and parentheses, in which a bare identifier`` |
|        - |  5424 | ` * stands for the constant of that name -- which is what makes` |
|        - |  5425 | `` * `error_reporting = E_ALL & ~E_DEPRECATED` mean 22527 rather than 0. Three`` |
|        - |  5426 | ` * facts about it are not the C ones and were all read off php 8.5:` |
|        - |  5427 | ` *` |
|        - |  5428 | ` *   . The three binary operators share ONE precedence and associate left, so` |
|        - |  5429 | `` *     `1 \| 2 & 4` is `(1\|2) & 4` = 0 where C reads it as 1. The unary pair binds`` |
|        - |  5430 | `` *     tighter and nests (`~~2` is 2).`` |
|        - |  5431 | ` *   . Operands are STRINGS. zend_ini_do_op() runs each side through atoi(),` |
|        - |  5432 | ` *     computes in a 32-bit int, and writes the decimal text of the result back` |
|        - |  5433 | ` *     as the value -- so an undefined constant is its own name (and therefore` |
|        - |  5434 | ` *     0), a quoted "E_ALL" is four letters and not 30719, and adjacent pieces` |
|        - |  5435 | `` *     concatenate: `E_NOTICE E_WARNING` is the string "8 2", which is 8.`` |
|        - |  5436 | ` *   . A value carrying no operator at all keeps its substituted TEXT. That text` |
|        - |  5437 | ` *     is what ini_get() shows and what ini_restore() re-applies, so it is what` |
|        - |  5438 | ` *     gets stored, not the number it happens to read as.` |
|        - |  5439 | ` *` |
|        - |  5440 | `` * The boolean words are a whole-value shape rather than an operand: `On` is 1`` |
|        - |  5441 | `` * even in `On\|E_NOTICE`, where the word itself still commits and the `\|` is`` |
|        - |  5442 | ` * a separate syntax error over the leftover text (see VmIniEvalValue).` |
|        - |  5443 | ` */` |
|        - |  5444 | `#define VM_INI_EXPR_MAX_DEPTH 32` |
|        - |  5445 | `/*` |
|        - |  5446 | ` * What php's ini parser would report over one value, in the pieces its` |
|        - |  5447 | ` * ini_error() prints. php names the token it could not take, and it has three` |
|        - |  5448 | ` * ways of naming one: zTok is the SYMBOL its grammar declares for a token made` |
|        - |  5449 | `` * of more than one shape (`TC_CONSTANT`, `TC_NUMBER`, `TC_STRING`, `TC_RAW`,`` |
|        - |  5450 | `` * `TC_DOLLAR_CURLY`), cChar is the byte itself for the tokens bison prints as`` |
|        - |  5451 | ` * a quoted character, and neither set means the value ran out -- which php` |
|        - |  5452 | ` * names END_OF_LINE and dates to the line after the directive. bExpect adds` |
|        - |  5453 | ` * the token list it appends when the stop happened inside an unclosed '('.` |
|        - |  5454 | ` * bSet is the whole question "was this value a syntax error", which is what` |
|        - |  5455 | ` * decides both the warning and php's abort of the rest of the source.` |
|        - |  5456 | ` */` |
|        - |  5457 | `typedef struct VmIniBad VmIniBad;` |
|        - |  5458 | `struct VmIniBad {` |
|        - |  5459 | `	int bSet;` |
|        - |  5460 | `	const char *zTok;` |
|        - |  5461 | `	int cChar;` |
|        - |  5462 | `	int bExpect;` |
|        - |  5463 | ``	/* An expect-list that is not the value grammar's own: `${` has two of its`` |
|        - |  5464 | `	 * own (TC_VARNAME, and "TC_FALLBACK or '}'"), so the list is text here` |
|        - |  5465 | `	 * rather than the single flag bExpect still carries. */` |
|        - |  5466 | `	const char *zExpect;` |
|        - |  5467 | `	/* How many lines below the directive's own the stop happened. A value is` |
|        - |  5468 | `	 * one scanner input and a double-quoted run inside it crosses newlines,` |
|        - |  5469 | ``	 * so `x = "a\nb")` is dated to the line the `)` is written on and not to`` |
|        - |  5470 | ``	 * the line `x` is. Only the double-quoted run moves it: php's raw-string`` |
|        - |  5471 | `	 * rule is a single match that never touches the line counter. */` |
|        - |  5472 | `	sxu32 nLine;` |
|        - |  5473 | `};` |
|        - |  5474 | `typedef struct VmIniExpr VmIniExpr;` |
|        - |  5475 | `struct VmIniExpr {` |
|        - |  5476 | `	ph7_vm *pVm;` |
|        - |  5477 | `	const char *zCur;` |
|        - |  5478 | `	const char *zEnd;` |
|        - |  5479 | `	/* Newlines a double-quoted run carried the scanner over before the stop --` |
|        - |  5480 | `	 * php's ST_DOUBLE_QUOTES counts every one of them, and dates whatever it` |
|        - |  5481 | `	 * reports next that many lines below the directive. */` |
|        - |  5482 | `	sxu32 nLine;` |
|        - |  5483 | ``	/* A raw string with no closing quote: php's `['][^']*[']` is ONE match, so`` |
|        - |  5484 | `	 * it does not match at all and the scanner runs off the end of the source.` |
|        - |  5485 | `	 * Whatever the value already holds still commits; only a value that had` |
|        - |  5486 | `	 * nothing yet becomes an error, and php names that one end of file. */` |
|        - |  5487 | `	int bRawEof;` |
|        - |  5488 | `	/* Where a failed parse stopped, in the two pieces php's ini_error() prints.` |
|        - |  5489 | `	 * cStop is the byte the grammar could not take, or 0 when the value simply` |
|        - |  5490 | `	 * ran out -- php calls that one END_OF_LINE and dates it to the line AFTER` |
|        - |  5491 | `	 * the directive, because its scanner has already eaten the newline. bExpect` |
|        - |  5492 | `	 * is set at the one place php's parser has a complete expression in hand and` |
|        - |  5493 | `	 * an unclosed '(' behind it, which is the whole of when it appends its` |
|        - |  5494 | ``	 * "expecting '^' or '\|' or '&' or ')'" list -- `(E_ALL` and `(1~2)` both get`` |
|        - |  5495 | ``	 * it, `~(` and `1 ^` do not. Only the FIRST stop is kept: the recursive`` |
|        - |  5496 | `	 * descent unwinds through every caller and php reports one error per parse. */` |
|        - |  5497 | `	int bStop;` |
|        - |  5498 | `	int cStop;` |
|        - |  5499 | `	int bExpect;` |
|        - |  5500 | ``	/* The `${` substitution names its own token and its own expect-list; every`` |
|        - |  5501 | `	 * other stop leaves both 0 and is described by cStop/bExpect alone. */` |
|        - |  5502 | `	const char *zTok;` |
|        - |  5503 | `	const char *zExpect;` |
|        - |  5504 | `};` |
|      108 |  5505 | `static void VmIniExprStop(VmIniExpr *p,int cStop,int bExpect)` |
|      ! 0 |  5506 | `{` |
|      108 |  5507 | `	if( p->bStop ){` |
|       28 |  5508 | `		return;` |
|        - |  5509 | `	}` |
|       80 |  5510 | `	p->bStop = 1;` |
|        - |  5511 | ``	/* `;` opens a comment rather than a token, so a value that stops there ran`` |
|        - |  5512 | `	 * out as far as the grammar is concerned. */` |
|       80 |  5513 | `	p->cStop = cStop == ';' ? 0 : cStop;` |
|       80 |  5514 | `	p->bExpect = bExpect;` |
|       54 |  5515 | `}` |
|        - |  5516 | `/*` |
|        - |  5517 | ` * A stop php names itself rather than by the byte it choked on. zTok is the` |
|        - |  5518 | ` * symbol printed with no quotes ("end of file", "TC_FALLBACK"), cStop the byte` |
|        - |  5519 | ` * when there is one, and zExpect the list appended behind either.` |
|        - |  5520 | ` */` |
|       66 |  5521 | `static void VmIniExprStopAt(VmIniExpr *p,const char *zTok,int cStop,const char *zExpect)` |
|      ! 0 |  5522 | `{` |
|       66 |  5523 | `	if( p->bStop ){` |
|      ! 0 |  5524 | `		return;` |
|        - |  5525 | `	}` |
|       66 |  5526 | `	p->bStop = 1;` |
|       66 |  5527 | `	p->cStop = cStop;` |
|       66 |  5528 | `	p->bExpect = 0;` |
|       66 |  5529 | `	p->zTok = zTok;` |
|       66 |  5530 | `	p->zExpect = zExpect;` |
|       33 |  5531 | `}` |
|     3645 |  5532 | `static void VmIniExprSpace(VmIniExpr *p)` |
|        4 |  5533 | `{` |
|     5034 |  5534 | `	while( p->zCur < p->zEnd && (p->zCur[0] == ' ' \|\| p->zCur[0] == '\t') ){` |
|       45 |  5535 | `		p->zCur++;` |
|        3 |  5536 | `	}` |
|     3649 |  5537 | `}` |
|     1823 |  5538 | `static int VmIniExprIsOp(int c)` |
|        4 |  5539 | `{` |
|     1827 |  5540 | `	return c == '\|' \|\| c == '&' \|\| c == '^';` |
|        4 |  5541 | `}` |
|        - |  5542 | `/*` |
|        - |  5543 | ` * One VALUE_CHARS unit: php's value scanner takes any byte that is not one of` |
|        - |  5544 | `` * its own delimiters, plus a `$` that does NOT open `${` -- which carries the`` |
|        - |  5545 | ` * byte behind it, and one more when that byte is a backslash. Answers how many` |
|        - |  5546 | `` * bytes the unit spends, or 0 when the run stops here. A `$` with NOTHING`` |
|        - |  5547 | ` * behind it matches nothing at all, so it is not a unit and the run stops in` |
|        - |  5548 | `` * front of it -- a value that ends on `$` keeps the newline that followed it,`` |
|        - |  5549 | ` * because the reader hands the run-on over whole rather than trimming it.` |
|        - |  5550 | ` */` |
|      853 |  5551 | `static int VmIniValueCharLen(const char *zCur,const char *zEnd)` |
|        4 |  5552 | `{` |
|      857 |  5553 | `	int c = (unsigned char)zCur[0];` |
|      857 |  5554 | `	if( c == '$' ){` |
|       30 |  5555 | `		if( &zCur[1] >= zEnd \|\| zCur[1] == '{' ){` |
|      ! 0 |  5556 | `			return 0;` |
|        - |  5557 | `		}` |
|       30 |  5558 | `		return zCur[1] == '\\' && &zCur[2] < zEnd ? 3 : 2;` |
|        - |  5559 | `	}` |
|      823 |  5560 | `	if( c == '=' \|\| c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\r' \|\| c == ';'` |
|      813 |  5561 | `	 \|\| c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '~' \|\| c == '(' \|\| c == ')'` |
|      817 |  5562 | `	 \|\| c == '!' \|\| c == '"' \|\| c == '\'' \|\| c == 0 ){` |
|       10 |  5563 | `		return 0;` |
|        - |  5564 | `	}` |
|      817 |  5565 | `	return 1;` |
|      430 |  5566 | `}` |
|        - |  5567 | `/*` |
|        - |  5568 | ` * Name the token php's value scanner would make of the text left standing` |
|        - |  5569 | ` * where the grammar cannot take it -- the two shapes VmIniEvalValue hands back` |
|        - |  5570 | ` * as "committed, but here is what php would separately warn about": a leftover` |
|        - |  5571 | `` * right after a boolean word short-circuits (`On X`, `On\|E_NOTICE`) and one`` |
|        - |  5572 | `` * right after a parenthesised expression closes (`(1)x`, `(1)=`). Blanks are`` |
|        - |  5573 | `` * already behind zCur, and `;` opens a comment rather than a token.`` |
|        - |  5574 | ` *` |
|        - |  5575 | ` * php's ST_VALUE rules compete by longest match and, on a tie, by the order` |
|        - |  5576 | ` * they are written in: TC_CONSTANT, then TC_NUMBER, then the catch-all` |
|        - |  5577 | ` * TC_STRING. Every byte those first two can take is also a VALUE_CHARS byte,` |
|        - |  5578 | ` * so the longest run is always the catch-all's and the tie IS the test --` |
|        - |  5579 | `` * `x1` is a constant, `1x` is a string, `-1` is a number and `-1x` is not.`` |
|        - |  5580 | ` */` |
|     1027 |  5581 | `static void VmIniBadToken(const char *zCur,const char *zEnd,VmIniBad *pBad)` |
|        4 |  5582 | `{` |
|        - |  5583 | `	const char *zWalk;` |
|        - |  5584 | `	sxu32 nVal,nCons,nNum;` |
|        - |  5585 | `	int c;` |
|     1031 |  5586 | `	if( zCur >= zEnd ){` |
|      937 |  5587 | `		return;` |
|        - |  5588 | `	}` |
|       97 |  5589 | `	c = (unsigned char)zCur[0];` |
|       97 |  5590 | `	if( c == ';' ){` |
|      ! 0 |  5591 | `		return;   /* a comment, not a token */` |
|        - |  5592 | `	}` |
|        - |  5593 | `	/* The tokens bison prints as a quoted character: the operators and` |
|        - |  5594 | ``	 * parentheses of the value grammar itself, the `"` that opens a double`` |
|        - |  5595 | ``	 * quoted run, and the `=` php hands back to statement position. */`` |
|       94 |  5596 | `	if( c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '~' \|\| c == '('` |
|       85 |  5597 | `	 \|\| c == ')' \|\| c == '!' \|\| c == '"' \|\| c == '=' ){` |
|       41 |  5598 | `		pBad->bSet = 1;` |
|       41 |  5599 | `		pBad->cChar = c;` |
|       41 |  5600 | `		return;` |
|        - |  5601 | `	}` |
|       56 |  5602 | `	if( c == '\'' ){` |
|        - |  5603 | `		/* A raw string is one token only when it closes, and it needs at least` |
|        - |  5604 | ``		 * one byte inside: `''` matches nothing at all and php reports no`` |
|        - |  5605 | `		 * error over it. */` |
|      110 |  5606 | `		for( zWalk = &zCur[1] ; zWalk < zEnd ; zWalk++ ){` |
|      106 |  5607 | `			if( zWalk[0] == '\'' ){` |
|        6 |  5608 | `				if( zWalk > &zCur[1] ){` |
|        6 |  5609 | `					pBad->bSet = 1;` |
|        6 |  5610 | `					pBad->zTok = "TC_RAW";` |
|        3 |  5611 | `				}` |
|        6 |  5612 | `				return;` |
|        - |  5613 | `			}` |
|       50 |  5614 | `		}` |
|        4 |  5615 | `		return;` |
|        - |  5616 | `	}` |
|       46 |  5617 | `	if( c == '$' && &zCur[1] < zEnd && zCur[1] == '{' ){` |
|        4 |  5618 | `		pBad->bSet = 1;` |
|        4 |  5619 | `		pBad->zTok = "TC_DOLLAR_CURLY";` |
|        4 |  5620 | `		return;` |
|        - |  5621 | `	}` |
|       42 |  5622 | `	nVal = 0;` |
|      130 |  5623 | `	for( zWalk = zCur ; zWalk < zEnd ; ){` |
|       90 |  5624 | `		int nUnit = VmIniValueCharLen(zWalk,zEnd);` |
|       90 |  5625 | `		if( nUnit < 1 ){` |
|        2 |  5626 | `			break;` |
|        - |  5627 | `		}` |
|       88 |  5628 | `		zWalk += nUnit;` |
|       88 |  5629 | `		nVal += (sxu32)nUnit;` |
|      ! 0 |  5630 | `	}` |
|       42 |  5631 | `	if( nVal < 1 ){` |
|      ! 0 |  5632 | `		return;` |
|        - |  5633 | `	}` |
|       42 |  5634 | `	nCons = 0;` |
|       42 |  5635 | `	if( c < 0xc0 && (SyisAlpha(c) \|\| c == '_') ){` |
|       24 |  5636 | `		for( zWalk = zCur ; zWalk < zEnd ; zWalk++ ){` |
|       14 |  5637 | `			if( (unsigned char)zWalk[0] >= 0xc0` |
|       14 |  5638 | `			 \|\| (!SyisAlphaNum((unsigned char)zWalk[0]) && zWalk[0] != '_') ){` |
|      ! 0 |  5639 | `				break;` |
|        - |  5640 | `			}` |
|       14 |  5641 | `			nCons++;` |
|        7 |  5642 | `		}` |
|        5 |  5643 | `	}` |
|        - |  5644 | ``	/* php's NUMBER is `[-]?[0-9]+` or a decimal run with a dot on either side`` |
|        - |  5645 | `	 * of it; the sign belongs to the integer form alone. */` |
|       42 |  5646 | `	nNum = 0;` |
|       58 |  5647 | `	for( zWalk = c == '-' ? &zCur[1] : zCur ; zWalk < zEnd ; zWalk++ ){` |
|       50 |  5648 | `		if( !SyisDigit((unsigned char)zWalk[0]) ){` |
|       34 |  5649 | `			break;` |
|        - |  5650 | `		}` |
|       16 |  5651 | `		nNum++;` |
|        8 |  5652 | `	}` |
|       42 |  5653 | `	if( nNum > 0 && c == '-' ){` |
|        2 |  5654 | `		nNum++;` |
|       41 |  5655 | `	}else if( c != '-' && zWalk < zEnd && zWalk[0] == '.' ){` |
|        6 |  5656 | `		sxu32 nFrac = 0;` |
|        - |  5657 | `		const char *zFrac;` |
|       10 |  5658 | `		for( zFrac = &zWalk[1] ; zFrac < zEnd ; zFrac++ ){` |
|        4 |  5659 | `			if( !SyisDigit((unsigned char)zFrac[0]) ){` |
|      ! 0 |  5660 | `				break;` |
|        - |  5661 | `			}` |
|        4 |  5662 | `			nFrac++;` |
|        2 |  5663 | `		}` |
|        6 |  5664 | `		if( nNum > 0 \|\| nFrac > 0 ){` |
|        6 |  5665 | `			nNum += nFrac + 1;` |
|        3 |  5666 | `		}` |
|        3 |  5667 | `	}` |
|       42 |  5668 | `	pBad->bSet = 1;` |
|       42 |  5669 | `	pBad->zTok = nCons == nVal ? "TC_CONSTANT"` |
|       37 |  5670 | `	           : (nNum == nVal ? "TC_NUMBER" : "TC_STRING");` |
|      517 |  5671 | `}` |
|        - |  5672 | `/*` |
|        - |  5673 | ` * atoi() over an operand: php stops at the first byte that is not part of a` |
|        - |  5674 | ` * number and answers 0 when there is none.` |
|        - |  5675 | ` */` |
|      116 |  5676 | `static sxi32 VmIniExprInt(SyBlob *pVal)` |
|        3 |  5677 | `{` |
|      119 |  5678 | `	sxi32 iVal = 0;` |
|      119 |  5679 | `	if( SyBlobLength(pVal) > 0 ){` |
|      119 |  5680 | `		SyStrToInt32((const char *)SyBlobData(pVal),SyBlobLength(pVal),(void *)&iVal,0);` |
|       58 |  5681 | `	}` |
|      119 |  5682 | `	return iVal;` |
|        3 |  5683 | `}` |
|       38 |  5684 | `static void VmIniExprSetInt(SyBlob *pOut,sxi32 iVal)` |
|        3 |  5685 | `{` |
|        - |  5686 | `	char zBuf[32];` |
|       41 |  5687 | `	int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",(int)iVal);` |
|       41 |  5688 | `	SyBlobReset(pOut);` |
|       41 |  5689 | `	SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|       41 |  5690 | `}` |
|        - |  5691 | `/*` |
|        - |  5692 | `` * php's `${NAME}` substitution, and php 8.5's `${NAME:-fallback}`. The name is`` |
|        - |  5693 | ` * answered in three steps (zend_ini_get_var): a directive ALREADY SET in this` |
|        - |  5694 | `` * source wins -- `precision=77` then `${precision}` is "77" whatever the`` |
|        - |  5695 | ` * environment says -- then the environment, then the fallback, then the empty` |
|        - |  5696 | ` * string. Only names that were set are visible: a directive left at its` |
|        - |  5697 | `` * built-in default is not in the table, so `${memory_limit}` is empty.`` |
|        - |  5698 | ` *` |
|        - |  5699 | ` * The name run is php's LABEL_CHAR: any byte that is not one of the value` |
|        - |  5700 | `` * grammar's own delimiters, `{`/`}`/`[`/`]`, or the `:` of a `:-`. Blanks are`` |
|        - |  5701 | `` * IN the run and trimmed off both ends afterwards, so `${ FOO }` is FOO and`` |
|        - |  5702 | `` * `${F OO}` is the three-word name "F OO".`` |
|        - |  5703 | ` */` |
|    13749 |  5704 | `static int VmIniVarNameStop(int c)` |
|        4 |  5705 | `{` |
|    20615 |  5706 | `	return c == '=' \|\| c == '\n' \|\| c == '\r' \|\| c == '\t' \|\| c == ';'` |
|    13741 |  5707 | `	    \|\| c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '$' \|\| c == '~'` |
|    13727 |  5708 | `	    \|\| c == '(' \|\| c == ')' \|\| c == '{' \|\| c == '}' \|\| c == '!'` |
|    20628 |  5709 | `	    \|\| c == '"' \|\| c == '[' \|\| c == ']' \|\| c == 0;` |
|        4 |  5710 | `}` |
|        - |  5711 | `/*` |
|        - |  5712 | ` * php's eight bool words. Its ini scanner has a rule for each of them, and` |
|        - |  5713 | ` * that rule stands AHEAD of the one that reads a LABEL -- so the same word is` |
|        - |  5714 | `` * a value's `1`/`""`/null and, at a statement position, a token no statement`` |
|        - |  5715 | `` * of php's grammar starts with. `on = 1` is `syntax error, unexpected`` |
|        - |  5716 | `` * BOOL_TRUE` where `onx = 1` is the entry "onx".`` |
|        - |  5717 | ` */` |
|        - |  5718 | `static const struct {` |
|        - |  5719 | `	const char *zWord;` |
|        - |  5720 | `	sxu32 nWord;` |
|        - |  5721 | `	const char *zText;   /* what the word reduces to inside a value */` |
|        - |  5722 | `	const char *zTok;    /* how php's parser names it when it refuses one */` |
|        - |  5723 | `} aVmIniBool[] = {` |
|        - |  5724 | `	{ "on"  ,2,"1","BOOL_TRUE"  }, { "yes"  ,3,"1","BOOL_TRUE"  },` |
|        - |  5725 | `	{ "true",4,"1","BOOL_TRUE"  }, { "off"  ,3,"" ,"BOOL_FALSE" },` |
|        - |  5726 | `	{ "no"  ,2,"" ,"BOOL_FALSE" }, { "false",5,"" ,"BOOL_FALSE" },` |
|        - |  5727 | `	{ "none",4,"" ,"BOOL_FALSE" }, { "null" ,4,"" ,"NULL_NULL"  }` |
|        - |  5728 | `};` |
|        - |  5729 | `/*` |
|        - |  5730 | ` * Screen the text standing where php's scanner reads a directive NAME, for a` |
|        - |  5731 | ` * host that walks a php.ini source itself (PH7_INI_STOP_STMT).` |
|        - |  5732 | ` *` |
|        - |  5733 | `` * php's INITIAL is not "everything up to the `=`": a `{LABEL}` run stops at`` |
|        - |  5734 | ` * every byte its operators, brackets and line ends are made of, and each of` |
|        - |  5735 | `` * those has a rule of its own behind it. A TAB, a `;` and a `[` open another`` |
|        - |  5736 | ` * statement, a comment and an offset, so a name may legitimately be several` |
|        - |  5737 | ` * runs -- but the twelve bytes below are tokens the grammar has no statement` |
|        - |  5738 | ` * for, and meeting one refuses the source from here down whether it opens the` |
|        - |  5739 | ` * name or sits in the middle of it. Three rules compete for the first run and` |
|        - |  5740 | ` * flex ranks them by length, then by the order they are written in:` |
|        - |  5741 | `` * `{LABEL}"["` outruns everything, the bool words come next, and `{LABEL}` is`` |
|        - |  5742 | `` * last. A word has to OPEN the run, and its `{TABS_AND_SPACES}*` tail is what`` |
|        - |  5743 | `` * lets it outrun a LABEL, which stops dead at a TAB -- so `on\t= 1` is`` |
|        - |  5744 | `` * BOOL_TRUE, `on x = 1` is the entry "on x", and `none = 1` is BOOL_FALSE`` |
|        - |  5745 | ` * only because the four-byte word outruns the two-byte one inside it.` |
|        - |  5746 | ` *` |
|        - |  5747 | ` * Answers 0 when php reads a name here, and otherwise the token it refuses` |
|        - |  5748 | ` * the statement under: a bool word by name, or the byte itself in quotes,` |
|        - |  5749 | ` * written into zBuf.` |
|        - |  5750 | ` */` |
|     1281 |  5751 | `static const char * VmIniStmtToken(const char *z,sxu32 nByte,char *zBuf)` |
|        4 |  5752 | `{` |
|     1285 |  5753 | `	const char *zEnd = &z[nByte];` |
|     1285 |  5754 | `	int bFirst = 1;` |
|      640 |  5755 | `	for(;;){` |
|     1285 |  5756 | `		const char *zRun = z;` |
|    14164 |  5757 | `		while( zRun < zEnd && !VmIniVarNameStop((unsigned char)zRun[0]) ){` |
|    12883 |  5758 | `			zRun++;` |
|        4 |  5759 | `		}` |
|     1285 |  5760 | `		if( bFirst && zRun > z && (zRun >= zEnd \|\| zRun[0] != '[') ){` |
|     1203 |  5761 | `			const char *zTok = 0;` |
|     1203 |  5762 | `			int nBest = 0;` |
|        - |  5763 | `			sxu32 i;` |
|    10795 |  5764 | `			for( i = 0 ; i < SX_ARRAYSIZE(aVmIniBool) ; ++i ){` |
|        - |  5765 | `				const char *zTail;` |
|        - |  5766 | `				int nMatch;` |
|     9592 |  5767 | `				if( (sxu32)(zEnd - z) < aVmIniBool[i].nWord` |
|     9476 |  5768 | `				 \|\| SyStrnicmp(z,aVmIniBool[i].zWord,aVmIniBool[i].nWord) != 0 ){` |
|     9572 |  5769 | `					continue;` |
|        - |  5770 | `				}` |
|       24 |  5771 | `				zTail = &z[aVmIniBool[i].nWord];` |
|       30 |  5772 | `				while( zTail < zEnd && (zTail[0] == ' ' \|\| zTail[0] == '\t') ){` |
|        2 |  5773 | `					zTail++;` |
|      ! 0 |  5774 | `				}` |
|       24 |  5775 | `				nMatch = (int)(zTail - z);` |
|       24 |  5776 | `				if( nMatch > nBest ){` |
|       24 |  5777 | `					nBest = nMatch;` |
|       24 |  5778 | `					zTok = aVmIniBool[i].zTok;` |
|       12 |  5779 | `				}` |
|       12 |  5780 | `			}` |
|     1203 |  5781 | `			if( zTok && nBest >= (int)(zRun - z) ){` |
|       18 |  5782 | `				return zTok;` |
|        - |  5783 | `			}` |
|      590 |  5784 | `		}` |
|     1267 |  5785 | `		bFirst = 0;` |
|     1267 |  5786 | `		z = zRun;` |
|     1267 |  5787 | `		if( z >= zEnd \|\| z[0] == 0 ){` |
|     1177 |  5788 | `			return 0;   /* the statement ran out: nothing left to refuse */` |
|        - |  5789 | `		}` |
|       90 |  5790 | `		if( z[0] == '\t' \|\| z[0] == ' ' ){` |
|      ! 0 |  5791 | `			z++;        /* blanks the LABEL rule left behind are thrown away */` |
|      ! 0 |  5792 | `			continue;` |
|        - |  5793 | `		}` |
|       90 |  5794 | `		if( z[0] == ';' \|\| z[0] == '\n' \|\| z[0] == '\r' \|\| z[0] == '[' ){` |
|       50 |  5795 | `			return 0;   /* the comment, the newline and the offset own these */` |
|        - |  5796 | `		}` |
|       40 |  5797 | `		zBuf[0] = '\'';` |
|       40 |  5798 | `		zBuf[1] = z[0];` |
|       40 |  5799 | `		zBuf[2] = '\'';` |
|       40 |  5800 | `		zBuf[3] = 0;` |
|       40 |  5801 | `		return zBuf;` |
|      ! 0 |  5802 | `	}` |
|      644 |  5803 | `}` |
|        - |  5804 | `/*` |
|        - |  5805 | `` * Name the token php's parser finds where an offset statement's `=` has to be.`` |
|        - |  5806 | `` * The `]` that closes an offset pops the scanner back to INITIAL, and every`` |
|        - |  5807 | ` * token INITIAL can make there is a refusal because the grammar has no other` |
|        - |  5808 | `` * statement to build: `foo[bar]]` is `']'`, `foo[bar]x` is TC_LABEL, and`` |
|        - |  5809 | `` * `foo[bar]on` is BOOL_TRUE even though the same word inside a value is a 1.`` |
|        - |  5810 | ` * The three INITIAL runs rank as they do at any other statement position --` |
|        - |  5811 | `` * `{LABEL}"["` first, then the bool words, then `{LABEL}` -- so a second`` |
|        - |  5812 | ` * offset behind the first is TC_OFFSET rather than the label inside it.` |
|        - |  5813 | ` *` |
|        - |  5814 | ` * Answers 0 when the text names nothing, which is php's END_OF_LINE, and` |
|        - |  5815 | ` * otherwise the token: a symbol by name, or the byte in quotes in zBuf.` |
|        - |  5816 | ` */` |
|       24 |  5817 | `static const char * VmIniOffsetToken(const char *z,sxu32 nByte,char *zBuf)` |
|      ! 0 |  5818 | `{` |
|       24 |  5819 | `	const char *zEnd = &z[nByte];` |
|       24 |  5820 | `	const char *zRun,*zTok = 0;` |
|       24 |  5821 | `	int nBest = 0;` |
|        - |  5822 | `	sxu32 i;` |
|       38 |  5823 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|        4 |  5824 | `		z++;   /* every INITIAL rule that can match here eats its own blanks */` |
|      ! 0 |  5825 | `	}` |
|       24 |  5826 | `	if( z >= zEnd \|\| z[0] == '\n' \|\| z[0] == '\r' ){` |
|        8 |  5827 | `		return 0;   /* the newline rule, which is an END_OF_LINE */` |
|        - |  5828 | `	}` |
|       16 |  5829 | `	if( z[0] == ';' ){` |
|        2 |  5830 | `		return 0;   /* so is the comment rule: it ends on the same newline */` |
|        - |  5831 | `	}` |
|       14 |  5832 | `	if( z[0] == '[' ){` |
|        2 |  5833 | `		return "TC_SECTION";` |
|        - |  5834 | `	}` |
|       12 |  5835 | `	zRun = z;` |
|       22 |  5836 | `	while( zRun < zEnd && !VmIniVarNameStop((unsigned char)zRun[0]) ){` |
|       10 |  5837 | `		zRun++;` |
|      ! 0 |  5838 | `	}` |
|       12 |  5839 | `	if( zRun == z ){` |
|        - |  5840 | `		/* one of the bytes no run of php's takes: it reaches the parser as` |
|        - |  5841 | `		 * itself, and bison prints it as a quoted character */` |
|        6 |  5842 | `		zBuf[0] = '\'';` |
|        6 |  5843 | `		zBuf[1] = z[0];` |
|        6 |  5844 | `		zBuf[2] = '\'';` |
|        6 |  5845 | `		zBuf[3] = 0;` |
|        6 |  5846 | `		return zBuf;` |
|        - |  5847 | `	}` |
|        6 |  5848 | `	if( zRun < zEnd && zRun[0] == '[' ){` |
|        2 |  5849 | `		return "TC_OFFSET";` |
|        - |  5850 | `	}` |
|       36 |  5851 | `	for( i = 0 ; i < SX_ARRAYSIZE(aVmIniBool) ; ++i ){` |
|        - |  5852 | `		const char *zTail;` |
|        - |  5853 | `		int nMatch;` |
|       32 |  5854 | `		if( (sxu32)(zEnd - z) < aVmIniBool[i].nWord` |
|       18 |  5855 | `		 \|\| SyStrnicmp(z,aVmIniBool[i].zWord,aVmIniBool[i].nWord) != 0 ){` |
|       30 |  5856 | `			continue;` |
|        - |  5857 | `		}` |
|        2 |  5858 | `		zTail = &z[aVmIniBool[i].nWord];` |
|        2 |  5859 | `		while( zTail < zEnd && (zTail[0] == ' ' \|\| zTail[0] == '\t') ){` |
|      ! 0 |  5860 | `			zTail++;` |
|      ! 0 |  5861 | `		}` |
|        2 |  5862 | `		nMatch = (int)(zTail - z);` |
|        2 |  5863 | `		if( nMatch > nBest ){` |
|        2 |  5864 | `			nBest = nMatch;` |
|        2 |  5865 | `			zTok = aVmIniBool[i].zTok;` |
|        1 |  5866 | `		}` |
|        1 |  5867 | `	}` |
|        4 |  5868 | `	if( zTok && nBest >= (int)(zRun - z) ){` |
|        2 |  5869 | `		return zTok;` |
|        - |  5870 | `	}` |
|        2 |  5871 | `	return "TC_LABEL";` |
|       12 |  5872 | `}` |
|        - |  5873 | `/*` |
|        - |  5874 | ` * The first two steps of that lookup. FALSE means the name is nowhere, which` |
|        - |  5875 | ` * is what hands the question on to a fallback.` |
|        - |  5876 | ` */` |
|       86 |  5877 | `static int VmIniVarLookup(ph7_vm *pVm,const char *zName,sxu32 nName,SyBlob *pOut)` |
|      ! 0 |  5878 | `{` |
|        - |  5879 | `	VmIniEntry *aEntry;` |
|        - |  5880 | `	const char *zEnv;` |
|        - |  5881 | `	char zName0[256];` |
|        - |  5882 | `	sxu32 n;` |
|       86 |  5883 | `	if( nName < 1 ){` |
|        2 |  5884 | `		return 0;` |
|        - |  5885 | `	}` |
|       84 |  5886 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|        - |  5887 | `	/* Backwards: a directive written twice answers with the last one, which is` |
|        - |  5888 | `	 * also the one that stands. */` |
|       84 |  5889 | `	for( n = SySetUsed(&pVm->aIniCli) ; n > 0 ; n-- ){` |
|        4 |  5890 | `		SyString *pName = &aEntry[n-1].sName;` |
|        4 |  5891 | `		if( pName->nByte == nName && SyMemcmp(pName->zString,zName,nName) == 0 ){` |
|        4 |  5892 | `			SyBlobAppend(pOut,aEntry[n-1].sValue.zString,aEntry[n-1].sValue.nByte);` |
|        4 |  5893 | `			return 1;` |
|        - |  5894 | `		}` |
|      ! 0 |  5895 | `	}` |
|        - |  5896 | `	/* getenv() wants a C string and the name is a slice of the ini source. A` |
|        - |  5897 | `	 * name longer than any real environment variable is simply absent.` |
|        - |  5898 | `	 *` |
|        - |  5899 | `	 * It also trips MSVC's C4996 "may be unsafe" deprecation under /WX -- the` |
|        - |  5900 | `	 * same standard function used deliberately, suppressed the same way vfs.c` |
|        - |  5901 | `	 * suppresses it for strerror(), and _MSC_VER-guarded so the GCC build never` |
|        - |  5902 | `	 * sees an unknown pragma. */` |
|       80 |  5903 | `	if( nName >= sizeof(zName0) ){` |
|      ! 0 |  5904 | `		return 0;` |
|        - |  5905 | `	}` |
|       80 |  5906 | `	SyMemcpy(zName,zName0,nName);` |
|       80 |  5907 | `	zName0[nName] = 0;` |
|        - |  5908 | `#if defined(_MSC_VER)` |
|        - |  5909 | `#pragma warning(push)` |
|        - |  5910 | `#pragma warning(disable:4996)` |
|        - |  5911 | `#endif` |
|       80 |  5912 | `	zEnv = getenv(zName0);` |
|        - |  5913 | `#if defined(_MSC_VER)` |
|        - |  5914 | `#pragma warning(pop)` |
|        - |  5915 | `#endif` |
|       80 |  5916 | `	if( zEnv == 0 ){` |
|       62 |  5917 | `		return 0;` |
|        - |  5918 | `	}` |
|       18 |  5919 | `	SyBlobAppend(pOut,zEnv,(sxu32)SyStrlen(zEnv));` |
|       18 |  5920 | `	return 1;` |
|       43 |  5921 | `}` |
|        - |  5922 | `static int VmIniExprVar(VmIniExpr *p,SyBlob *pOut,int nDepth);` |
|        - |  5923 | `/*` |
|        - |  5924 | ` * One quoted run. php collapses exactly three escapes inside a double-quoted` |
|        - |  5925 | ` * run and keeps both bytes of every other one; a single-quoted run carries no` |
|        - |  5926 | `` * escapes at all and no substitution either, so `'${FOO}'` is its own seven`` |
|        - |  5927 | `` * bytes. A tool that has to put a quote in an ini value writes `\"` -- PHPUnit's`` |
|        - |  5928 | ` * job runner does -- and reading that as the end of the run cut the value in` |
|        - |  5929 | ` * half.` |
|        - |  5930 | ` */` |
|       78 |  5931 | `static int VmIniExprQuoted(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|        3 |  5932 | `{` |
|       81 |  5933 | `	int c = (unsigned char)p->zCur[0];` |
|       81 |  5934 | `	p->zCur++;` |
|      633 |  5935 | `	while( p->zCur < p->zEnd && (unsigned char)p->zCur[0] != c ){` |
|      555 |  5936 | `		if( c == '"' && (p->zCur[0] == '\n' \|\| p->zCur[0] == '\r') ){` |
|        - |  5937 | `			/* ST_DOUBLE_QUOTES is the only quoted run that counts what it eats. */` |
|       42 |  5938 | `			if( p->zCur[0] == '\r' && &p->zCur[1] < p->zEnd && p->zCur[1] == '\n' ){` |
|      ! 0 |  5939 | `				SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|      ! 0 |  5940 | `				p->zCur++;` |
|      ! 0 |  5941 | `			}` |
|       42 |  5942 | `			p->nLine++;` |
|       42 |  5943 | `			SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|       42 |  5944 | `			p->zCur++;` |
|       42 |  5945 | `			continue;` |
|        - |  5946 | `		}` |
|      513 |  5947 | `		if( c == '"' && p->zCur[0] == '\\' && &p->zCur[1] < p->zEnd ){` |
|      ! 0 |  5948 | `			int e = (unsigned char)p->zCur[1];` |
|      ! 0 |  5949 | `			if( e == '"' \|\| e == '\\' \|\| e == '$' ){` |
|      ! 0 |  5950 | `				SyBlobAppend(pOut,&p->zCur[1],sizeof(char));` |
|      ! 0 |  5951 | `			}else{` |
|      ! 0 |  5952 | `				SyBlobAppend(pOut,p->zCur,2*sizeof(char));` |
|        - |  5953 | `			}` |
|      ! 0 |  5954 | `			p->zCur += 2;` |
|      ! 0 |  5955 | `			continue;` |
|        - |  5956 | `		}` |
|      513 |  5957 | `		if( c == '"' && p->zCur[0] == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|        - |  5958 | ``			/* ST_DOUBLE_QUOTES takes `${` too. */`` |
|        2 |  5959 | `			if( !VmIniExprVar(p,pOut,nDepth+1) ){` |
|      ! 0 |  5960 | `				return 0;` |
|        - |  5961 | `			}` |
|        2 |  5962 | `			continue;` |
|        - |  5963 | `		}` |
|      511 |  5964 | `		SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|      511 |  5965 | `		p->zCur++;` |
|        3 |  5966 | `	}` |
|       81 |  5967 | `	if( p->zCur >= p->zEnd ){` |
|        - |  5968 | `		/* php's ST_DOUBLE_QUOTES runs to the end of the SOURCE, not to the end` |
|        - |  5969 | `		 * of the line, so a quote with no partner is the end of file and the` |
|        - |  5970 | `		 * whole value is refused. Dropping the quote and keeping the letters` |
|        - |  5971 | ``		 * made `a"b` the two bytes "ab". The caller names the token: this one is`` |
|        - |  5972 | ``		 * also reached from inside `${NAME:-...}`, which has an expect-list of`` |
|        - |  5973 | `		 * its own. */` |
|       14 |  5974 | `		return 0;` |
|        - |  5975 | `	}` |
|       67 |  5976 | `	p->zCur++;    /* the closing quote */` |
|       67 |  5977 | `	return 1;` |
|       42 |  5978 | `}` |
|        - |  5979 | `/*` |
|        - |  5980 | `` * `${` ... `}`, with p->zCur on the `$`. Everything it can refuse refuses the`` |
|        - |  5981 | ` * whole VALUE -- php's parser never reduces the directive -- so the directive` |
|        - |  5982 | ` * keeps its default and the rest of the source is dropped, exactly as any other` |
|        - |  5983 | ` * ini syntax error is.` |
|        - |  5984 | ` *` |
|        - |  5985 | ` * The three refusals are php's own, and the middle one is php's scanner being` |
|        - |  5986 | `` * literal about a one-byte name: `<ST_VARNAME>{LABEL_CHAR}` matches ONE byte and`` |
|        - |  5987 | `` * then looks ahead, and when what follows is `:-` it jumps straight to the`` |
|        - |  5988 | `` * fallback state WITHOUT returning the name it just read. So `${NN:-x}` is`` |
|        - |  5989 | `` * "x" and `${N:-x}` is a syntax error over a token the parser never got.`` |
|        - |  5990 | ` */` |
|      146 |  5991 | `static int VmIniExprVar(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|      ! 0 |  5992 | `{` |
|        - |  5993 | `	const char *zName,*zRaw;` |
|        - |  5994 | `	SyBlob sFallback;` |
|        - |  5995 | `	sxu32 nName,nRaw;` |
|      146 |  5996 | `	int bFallback = 0;` |
|      146 |  5997 | `	if( nDepth > VM_INI_EXPR_MAX_DEPTH ){` |
|      ! 0 |  5998 | `		VmIniExprStopAt(p,"end of file",0,"'}'");` |
|      ! 0 |  5999 | `		return 0;` |
|        - |  6000 | `	}` |
|        - |  6001 | ``	/* Seeded here rather than under the `:-` branch that fills it: MSVC cannot`` |
|        - |  6002 | `	 * prove bFallback gates every use and rejects the blob as possibly` |
|        - |  6003 | `	 * uninitialised under /WX. */` |
|      146 |  6004 | `	SyBlobInit(&sFallback,&p->pVm->sAllocator);` |
|      146 |  6005 | ``	p->zCur += 2;    /* `${` */`` |
|      146 |  6006 | `	zRaw = p->zCur;` |
|      768 |  6007 | `	while( p->zCur < p->zEnd ){` |
|      762 |  6008 | `		int c = (unsigned char)p->zCur[0];` |
|      762 |  6009 | `		if( VmIniVarNameStop(c) ){` |
|       66 |  6010 | `			break;` |
|        - |  6011 | `		}` |
|      696 |  6012 | `		if( c == ':' && &p->zCur[1] < p->zEnd && p->zCur[1] == '-' ){` |
|       74 |  6013 | `			break;` |
|        - |  6014 | `		}` |
|      622 |  6015 | `		p->zCur++;` |
|      ! 0 |  6016 | `	}` |
|      146 |  6017 | `	nRaw = (sxu32)(p->zCur - zRaw);` |
|      146 |  6018 | `	if( nRaw < 1 ){` |
|        - |  6019 | ``		/* Nothing the name rule could take at all. `${}` names the brace;`` |
|        - |  6020 | `		 * anything else -- a delimiter, or the source running out -- reaches` |
|        - |  6021 | `		 * php's scanner with no rule left and reads as the end of file. */` |
|       16 |  6022 | `		if( p->zCur < p->zEnd && p->zCur[0] == '}' ){` |
|        8 |  6023 | `			VmIniExprStopAt(p,0,'}',"TC_VARNAME");` |
|        4 |  6024 | `		}else{` |
|        8 |  6025 | `			VmIniExprStopAt(p,"end of file",0,"TC_VARNAME");` |
|        - |  6026 | `		}` |
|       16 |  6027 | `		SyBlobRelease(&sFallback);` |
|       16 |  6028 | `		return 0;` |
|        - |  6029 | `	}` |
|      130 |  6030 | `	if( nRaw == 1 && p->zCur < p->zEnd && p->zCur[0] == ':' ){` |
|       10 |  6031 | `		VmIniExprStopAt(p,"TC_FALLBACK",0,"TC_VARNAME");` |
|       10 |  6032 | `		SyBlobRelease(&sFallback);` |
|       10 |  6033 | `		return 0;` |
|        - |  6034 | `	}` |
|      120 |  6035 | `	zName = zRaw;` |
|      120 |  6036 | `	nName = nRaw;` |
|      184 |  6037 | `	while( nName > 0 && (zName[0] == ' ' \|\| zName[0] == '\t') ){` |
|        4 |  6038 | `		zName++;` |
|        4 |  6039 | `		nName--;` |
|      ! 0 |  6040 | `	}` |
|      182 |  6041 | `	while( nName > 0 && (zName[nName-1] == ' ' \|\| zName[nName-1] == '\t') ){` |
|        2 |  6042 | `		nName--;` |
|      ! 0 |  6043 | `	}` |
|      120 |  6044 | `	if( p->zCur < p->zEnd && p->zCur[0] == ':' ){` |
|        - |  6045 | ``		/* The fallback state takes text, `\<byte>` pairs kept WHOLE, nested`` |
|        - |  6046 | `		 * substitutions and double-quoted runs -- and nothing else: a newline,` |
|        - |  6047 | ``		 * a `;` or a raw quote leaves php with no rule and it reports the end`` |
|        - |  6048 | ``		 * of file. Blanks here are content, so `${NN:- }` is one space. */`` |
|       64 |  6049 | `		p->zCur += 2;` |
|       64 |  6050 | `		bFallback = 1;` |
|      180 |  6051 | `		while( p->zCur < p->zEnd ){` |
|      178 |  6052 | `			int c = (unsigned char)p->zCur[0];` |
|      178 |  6053 | `			if( c == '}' ){` |
|       40 |  6054 | `				break;` |
|        - |  6055 | `			}` |
|      138 |  6056 | `			if( c == '\\' && &p->zCur[1] < p->zEnd ){` |
|        6 |  6057 | `				SyBlobAppend(&sFallback,p->zCur,2*sizeof(char));` |
|        6 |  6058 | `				p->zCur += 2;` |
|        6 |  6059 | `				continue;` |
|        - |  6060 | `			}` |
|      132 |  6061 | `			if( c == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|        8 |  6062 | `				if( !VmIniExprVar(p,&sFallback,nDepth+1) ){` |
|        2 |  6063 | `					SyBlobRelease(&sFallback);` |
|        2 |  6064 | `					return 0;` |
|        - |  6065 | `				}` |
|        6 |  6066 | `				continue;` |
|        - |  6067 | `			}` |
|      124 |  6068 | `			if( c == '"' ){` |
|       16 |  6069 | `				if( !VmIniExprQuoted(p,&sFallback,nDepth) ){` |
|        - |  6070 | `					/* The run reaches the end of the SOURCE, and what php` |
|        - |  6071 | `					 * names there is its double-quote state's own expect-list` |
|        - |  6072 | ``					 * rather than the `}` the fallback around it still wants:`` |
|        - |  6073 | ``					 * `${A:-"x` and `${A:-x"` both stop under it. */`` |
|        8 |  6074 | `					SyBlobRelease(&sFallback);` |
|        8 |  6075 | `					VmIniExprStopAt(p,"end of file",0,` |
|        - |  6076 | `						"TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'");` |
|        8 |  6077 | `					return 0;` |
|        - |  6078 | `				}` |
|        8 |  6079 | `				continue;` |
|        - |  6080 | `			}` |
|      108 |  6081 | `			if( c == '\n' \|\| c == '\r' \|\| c == ';' \|\| c == '\'' ){` |
|        6 |  6082 | `				break;` |
|        - |  6083 | `			}` |
|       96 |  6084 | `			SyBlobAppend(&sFallback,p->zCur,sizeof(char));` |
|       96 |  6085 | `			p->zCur++;` |
|      ! 0 |  6086 | `		}` |
|       54 |  6087 | `		if( p->zCur >= p->zEnd \|\| p->zCur[0] != '}' ){` |
|       14 |  6088 | `			SyBlobRelease(&sFallback);` |
|       14 |  6089 | `			VmIniExprStopAt(p,"end of file",0,"'}'");` |
|       14 |  6090 | `			return 0;` |
|        - |  6091 | `		}` |
|       20 |  6092 | `	}` |
|       96 |  6093 | `	if( p->zCur >= p->zEnd \|\| p->zCur[0] != '}' ){` |
|       10 |  6094 | `		SyBlobRelease(&sFallback);` |
|       10 |  6095 | `		VmIniExprStopAt(p,"end of file",0,"TC_FALLBACK or '}'");` |
|       10 |  6096 | `		return 0;` |
|        - |  6097 | `	}` |
|       86 |  6098 | ``	p->zCur++;    /* `}` */`` |
|       86 |  6099 | `	if( !VmIniVarLookup(p->pVm,zName,nName,pOut) && bFallback ){` |
|       38 |  6100 | `		SyBlobAppend(pOut,SyBlobData(&sFallback),SyBlobLength(&sFallback));` |
|       19 |  6101 | `	}` |
|       86 |  6102 | `	SyBlobRelease(&sFallback);` |
|       86 |  6103 | `	return 1;` |
|       73 |  6104 | `}` |
|        - |  6105 | `/*` |
|        - |  6106 | `` * One operand: everything up to the next operator, parenthesis or `;` comment,`` |
|        - |  6107 | ` * with each bare identifier replaced by the constant of that name when one is` |
|        - |  6108 | ` * defined and left standing as its own text when none is, and each quoted run` |
|        - |  6109 | ``  * taken literally. Trailing blanks are not part of the token, so `E_ALL ; x` `` |
|        - |  6110 | ` * stores "30719" and not "30719 ".` |
|        - |  6111 | ` */` |
|     1145 |  6112 | `static int VmIniExprOperand(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|        4 |  6113 | `{` |
|     1149 |  6114 | `	const char *zPend = 0;   /* blanks held back: trailing ones are not part of the` |
|        - |  6115 | `	                          * token, interior ones are ("E_NOTICE E_WARNING" is "8 2") */` |
|     1149 |  6116 | `	sxu32 nPend = 0;` |
|     1149 |  6117 | `	int bAny = 0;` |
|     1149 |  6118 | `	SyBlobReset(pOut);` |
|     1149 |  6119 | `	VmIniExprSpace(p);` |
|     2534 |  6120 | `	while( p->zCur < p->zEnd ){` |
|     1597 |  6121 | `		int c = (unsigned char)p->zCur[0];` |
|     1593 |  6122 | `		if( VmIniExprIsOp(c) \|\| c == '(' \|\| c == ')'` |
|     1475 |  6123 | `		 \|\| c == '~' \|\| c == '!' \|\| c == ';' ){` |
|       87 |  6124 | `			break;` |
|        - |  6125 | `		}` |
|     1429 |  6126 | `		if( c == ' ' \|\| c == '\t' ){` |
|       87 |  6127 | `			if( nPend == 0 ){` |
|       87 |  6128 | `				zPend = p->zCur;` |
|       42 |  6129 | `			}` |
|       87 |  6130 | `			nPend++;` |
|       87 |  6131 | `			p->zCur++;` |
|       87 |  6132 | `			continue;` |
|        - |  6133 | `		}` |
|     1345 |  6134 | `		if( nPend > 0 ){` |
|       12 |  6135 | `			SyBlobAppend(pOut,zPend,nPend);` |
|       12 |  6136 | `			nPend = 0;` |
|        5 |  6137 | `		}` |
|     1345 |  6138 | `		if( c == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|       84 |  6139 | `			if( !VmIniExprVar(p,pOut,nDepth) ){` |
|       22 |  6140 | `				return 0;` |
|        - |  6141 | `			}` |
|        - |  6142 | `			/* A substitution that answered nothing still MADE a value: the` |
|        - |  6143 | `			 * directive lands as the empty string rather than keeping its` |
|        - |  6144 | `			 * default. */` |
|       62 |  6145 | `			bAny = 1;` |
|       62 |  6146 | `			continue;` |
|        - |  6147 | `		}` |
|     1261 |  6148 | `		if( c == '\'' ){` |
|       12 |  6149 | `			const char *zQ = &p->zCur[1];` |
|      148 |  6150 | `			while( zQ < p->zEnd && zQ[0] != '\'' ){` |
|      136 |  6151 | `				zQ++;` |
|      ! 0 |  6152 | `			}` |
|       12 |  6153 | `			if( zQ >= p->zEnd ){` |
|        - |  6154 | `				/* php's raw-string rule is one match spanning as many newlines` |
|        - |  6155 | `				 * as it needs; with no closing quote it matches nothing and the` |
|        - |  6156 | `				 * scanner leaves for the end of the source. The operand ends` |
|        - |  6157 | `` 				 * here, and what it already holds is still a value: `x = A'b` `` |
|        - |  6158 | `				 * stores "A" and reports nothing at all. */` |
|        4 |  6159 | `				p->bRawEof = 1;` |
|        4 |  6160 | `				break;` |
|        - |  6161 | `			}` |
|        4 |  6162 | `		}` |
|     1257 |  6163 | `		if( c == '"' \|\| c == '\'' ){` |
|       65 |  6164 | `			if( !VmIniExprQuoted(p,pOut,nDepth) ){` |
|        - |  6165 | `				/* Only the double-quoted run can get here now, and php names its` |
|        - |  6166 | `				 * refusal with the tokens ST_DOUBLE_QUOTES was still willing to` |
|        - |  6167 | `				 * take. */` |
|        6 |  6168 | `				VmIniExprStopAt(p,"end of file",0,` |
|        - |  6169 | `					"TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'");` |
|        6 |  6170 | `				return 0;` |
|        - |  6171 | `			}` |
|       59 |  6172 | `			bAny = 1;` |
|       59 |  6173 | `			continue;` |
|        - |  6174 | `		}` |
|     1195 |  6175 | `		if( c < 0xc0 && (SyisAlpha(c) \|\| c == '_') ){` |
|      432 |  6176 | `			const char *zTok = p->zCur;` |
|        - |  6177 | `			ph7_value sCons;` |
|     1891 |  6178 | `			while( p->zCur < p->zEnd` |
|     2752 |  6179 | `			 && (unsigned char)p->zCur[0] < 0xc0` |
|     4045 |  6180 | `			 && (SyisAlphaNum((unsigned char)p->zCur[0]) \|\| p->zCur[0] == '_') ){` |
|     2502 |  6181 | `				p->zCur++;` |
|        4 |  6182 | `			}` |
|      432 |  6183 | `			PH7_MemObjInit(p->pVm,&sCons);` |
|        - |  6184 | `			/* php reads php.ini before a single extension has registered a` |
|        - |  6185 | ``			 * constant, so only the ENGINE's own answer here: `M_PI`,`` |
|        - |  6186 | ``			 * `SORT_ASC` and `DIRECTORY_SEPARATOR` are ext/standard's and`` |
|        - |  6187 | `			 * store their own NAMES, while the same text through` |
|        - |  6188 | `			 * parse_ini_file() at runtime stores the constant. */` |
|      428 |  6189 | `			if( PH7_VmExtOfConstant(zTok,(int)(p->zCur - zTok)) == PH7_EXT_CORE` |
|      431 |  6190 | `			 && (PH7_ExpandBuiltinConstant(p->pVm,zTok,(sxu32)(p->zCur - zTok),&sCons)` |
|      426 |  6191 | `			  \|\| PH7_VmQueryConstant(p->pVm,zTok,(sxu32)(p->zCur - zTok),&sCons)) ){` |
|       48 |  6192 | `				int nCons = 0;` |
|       48 |  6193 | `				const char *zCons = ph7_value_to_string(&sCons,&nCons);` |
|       48 |  6194 | `				SyBlobAppend(pOut,zCons,(sxu32)nCons);` |
|       26 |  6195 | `			}else{` |
|      388 |  6196 | `				SyBlobAppend(pOut,zTok,(sxu32)(p->zCur - zTok));` |
|        - |  6197 | `			}` |
|      432 |  6198 | `			PH7_MemObjRelease(&sCons);` |
|      432 |  6199 | `			bAny = 1;` |
|      432 |  6200 | `			continue;` |
|        - |  6201 | `		}` |
|        - |  6202 | `		{` |
|        - |  6203 | `			/* Everything else is VALUE_CHARS, and the run spends whole UNITS` |
|        - |  6204 | `			 * of it: a byte that opens no unit is not part of the operand and` |
|        - |  6205 | ``			 * ends it. `=` and a bare newline are two of those, so a value`` |
|        - |  6206 | `			 * cannot walk down into the statement below on its own -- the one` |
|        - |  6207 | ``			 * way through is the `$` unit, which carries whatever byte follows`` |
|        - |  6208 | `			 * it and hands the run-on the next line's text. */` |
|      767 |  6209 | `			int nUnit = VmIniValueCharLen(p->zCur,p->zEnd);` |
|      767 |  6210 | `			if( nUnit < 1 ){` |
|        8 |  6211 | `				break;` |
|        - |  6212 | `			}` |
|      759 |  6213 | `			SyBlobAppend(pOut,p->zCur,(sxu32)nUnit);` |
|      759 |  6214 | `			p->zCur += nUnit;` |
|      759 |  6215 | `			bAny = 1;` |
|        - |  6216 | `		}` |
|        4 |  6217 | `	}` |
|     1121 |  6218 | `	return bAny;` |
|      576 |  6219 | `}` |
|        - |  6220 | `static int VmIniExprEval(VmIniExpr *p,SyBlob *pOut,int nDepth);` |
|     1287 |  6221 | `static int VmIniExprUnary(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|        4 |  6222 | `{` |
|        - |  6223 | `	int c;` |
|     1291 |  6224 | `	if( nDepth > VM_INI_EXPR_MAX_DEPTH ){` |
|      ! 0 |  6225 | `		return 0;` |
|        - |  6226 | `	}` |
|     1291 |  6227 | `	VmIniExprSpace(p);` |
|     1291 |  6228 | `	if( p->zCur >= p->zEnd ){` |
|       46 |  6229 | `		VmIniExprStop(p,0,0);` |
|       46 |  6230 | `		return 0;` |
|        - |  6231 | `	}` |
|     1245 |  6232 | `	c = p->zCur[0];` |
|     1245 |  6233 | `	if( c == '~' \|\| c == '!' ){` |
|       29 |  6234 | `		p->zCur++;` |
|       29 |  6235 | `		if( !VmIniExprUnary(p,pOut,nDepth+1) ){` |
|       12 |  6236 | `			return 0;` |
|        - |  6237 | `		}` |
|       17 |  6238 | `		VmIniExprSetInt(pOut,c == '~' ? ~VmIniExprInt(pOut)` |
|      ! 0 |  6239 | `		                              : (sxi32)(VmIniExprInt(pOut) == 0));` |
|       17 |  6240 | `		return 1;` |
|        - |  6241 | `	}` |
|     1219 |  6242 | `	if( c == '(' ){` |
|       71 |  6243 | `		p->zCur++;` |
|       71 |  6244 | `		if( !VmIniExprEval(p,pOut,nDepth+1) ){` |
|        4 |  6245 | `			return 0;` |
|        - |  6246 | `		}` |
|       67 |  6247 | `		VmIniExprSpace(p);` |
|       67 |  6248 | `		if( p->zCur >= p->zEnd \|\| p->zCur[0] != ')' ){` |
|        - |  6249 | `			/* A complete expression with an unclosed '(' behind it: php's parser` |
|        - |  6250 | `			 * can still take an operator or the ')', and says so. */` |
|       12 |  6251 | `			VmIniExprStop(p,p->zCur >= p->zEnd ? 0 : (unsigned char)p->zCur[0],1);` |
|       12 |  6252 | `			return 0;` |
|        - |  6253 | `		}` |
|       55 |  6254 | `		p->zCur++;` |
|       55 |  6255 | `		return 1;` |
|        - |  6256 | `	}` |
|     1149 |  6257 | `	if( !VmIniExprOperand(p,pOut,nDepth) ){` |
|       52 |  6258 | `		if( p->bRawEof ){` |
|        - |  6259 | `			/* Nothing had reduced yet, so the scanner's leap to the end of the` |
|        - |  6260 | `			 * source is the token php's parser chokes on -- and it is the end of` |
|        - |  6261 | `			 * the FILE, not the end of a line, so it does not move the line the` |
|        - |  6262 | `			 * refusal is dated to. */` |
|        2 |  6263 | `			VmIniExprStopAt(p,"end of file",0,0);` |
|        1 |  6264 | `		}else{` |
|       50 |  6265 | `			VmIniExprStop(p,(unsigned char)p->zCur[0],0);` |
|        - |  6266 | `		}` |
|       52 |  6267 | `		return 0;` |
|        - |  6268 | `	}` |
|     1097 |  6269 | `	return 1;` |
|      647 |  6270 | `}` |
|     1183 |  6271 | `static int VmIniExprEval(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|        4 |  6272 | `{` |
|     1187 |  6273 | `	if( nDepth > VM_INI_EXPR_MAX_DEPTH ){` |
|      ! 0 |  6274 | `		return 0;` |
|        - |  6275 | `	}` |
|     1187 |  6276 | `	if( !VmIniExprUnary(p,pOut,nDepth) ){` |
|       60 |  6277 | `		return 0;` |
|        - |  6278 | `	}` |
|      585 |  6279 | `	for(;;){` |
|        - |  6280 | `		SyBlob sRhs;` |
|        - |  6281 | `		sxi32 iLhs,iRhs,iRes;` |
|        - |  6282 | `		int c;` |
|     1151 |  6283 | `		VmIniExprSpace(p);` |
|     1151 |  6284 | `		if( p->zCur >= p->zEnd \|\| !VmIniExprIsOp(p->zCur[0]) ){` |
|      538 |  6285 | `			break;` |
|        - |  6286 | `		}` |
|       81 |  6287 | `		c = p->zCur[0];` |
|       81 |  6288 | `		p->zCur++;` |
|       81 |  6289 | `		iLhs = VmIniExprInt(pOut);` |
|       81 |  6290 | `		SyBlobInit(&sRhs,&p->pVm->sAllocator);` |
|       81 |  6291 | `		if( !VmIniExprUnary(p,&sRhs,nDepth+1) ){` |
|       54 |  6292 | `			SyBlobRelease(&sRhs);` |
|       54 |  6293 | `			return 0;` |
|        - |  6294 | `		}` |
|       27 |  6295 | `		iRhs = VmIniExprInt(&sRhs);` |
|       27 |  6296 | `		SyBlobRelease(&sRhs);` |
|       27 |  6297 | `		iRes = c == '\|' ? (iLhs \| iRhs) : (c == '&' ? (iLhs & iRhs) : (iLhs ^ iRhs));` |
|       27 |  6298 | `		VmIniExprSetInt(pOut,iRes);` |
|        3 |  6299 | `	}` |
|     1073 |  6300 | `	return 1;` |
|      595 |  6301 | `}` |
|        - |  6302 | `/*` |
|        - |  6303 | ` * Evaluate one php.ini value into the text php would store for it. FALSE means` |
|        - |  6304 | ` * nothing ever reduced to a complete value at all -- a dangling operator or an` |
|        - |  6305 | ` * unmatched '(' -- and php leaves the directive at its default rather than at` |
|        - |  6306 | `` * zero: `error_reporting = E_ALL &` keeps whatever was there before.`` |
|        - |  6307 | ` *` |
|        - |  6308 | ` * A syntax error elsewhere does NOT mean FALSE: php's yacc grammar reduces` |
|        - |  6309 | `` * `string_or_value` (and fires the assignment) as soon as the lookahead byte`` |
|        - |  6310 | ` * cannot extend it further, and only THEN discovers that byte cannot start` |
|        - |  6311 | `` * anything either. `error_reporting = E_ALL)` stores 30719 and separately`` |
|        - |  6312 | ` * warns about the ')' -- the value most be committed even though the whole` |
|        - |  6313 | ` * directive text was not clean.` |
|        - |  6314 | ` *` |
|        - |  6315 | ` * *pBad is what php would REPORT over this value, and it is filled on both` |
|        - |  6316 | ` * paths: a leftover byte over a value that committed, and, when the parse` |
|        - |  6317 | ` * failed outright, wherever the expression walker stopped. Its bSet is the` |
|        - |  6318 | ` * question "was this a syntax error at all" -- the caller prints php's` |
|        - |  6319 | ` * warning over it AND stops feeding the rest of that ini source, which php` |
|        - |  6320 | ` * does because it hands each source to its parser whole (see` |
|        - |  6321 | ` * VmSetIniEntry and PH7_VmApplyEngineIni).` |
|        - |  6322 | ` */` |
|     1173 |  6323 | `static int VmIniEvalValue(ph7_vm *pVm,const char *zVal,sxu32 nVal,SyBlob *pOut,VmIniBad *pBad)` |
|        4 |  6324 | `{` |
|        - |  6325 | `	VmIniExpr sIn;` |
|        - |  6326 | `	sxu32 i;` |
|     1177 |  6327 | `	pBad->bSet = 0;` |
|     1177 |  6328 | `	pBad->zTok = 0;` |
|     1177 |  6329 | `	pBad->cChar = 0;` |
|     1177 |  6330 | `	pBad->bExpect = 0;` |
|     1177 |  6331 | `	pBad->zExpect = 0;` |
|     1177 |  6332 | `	pBad->nLine = 0;` |
|     1177 |  6333 | `	SyBlobReset(pOut);` |
|     1999 |  6334 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){` |
|      236 |  6335 | `		zVal++;` |
|      236 |  6336 | `		nVal--;` |
|      ! 0 |  6337 | `	}` |
|     1781 |  6338 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){` |
|       18 |  6339 | `		nVal--;` |
|      ! 0 |  6340 | `	}` |
|     1177 |  6341 | `	if( nVal == 0 ){` |
|        4 |  6342 | ``		return 1;   /* `-d name=` carries the empty value, not an expression */`` |
|        - |  6343 | `	}` |
|        - |  6344 | `	/* A boolean word is its own token, taken whenever it is the longest match` |
|        - |  6345 | `	 * AT THE FRONT of the value -- i.e. whenever the byte right behind it is` |
|        - |  6346 | `` 	 * not one php's scanner would fold into the same run. `onx`/`ontology` `` |
|        - |  6347 | `	 * are not "on" at all (VALUE_CHARS+ outruns the word there and reads the` |
|        - |  6348 | `` 	 * whole run as one unresolved identifier instead), while `On\|E_NOTICE` `` |
|        - |  6349 | `	 * IS "on" followed by a token the boolean production cannot take -- the` |
|        - |  6350 | `	 * word still commits, the '\|' is a separate, later syntax error. */` |
|    10137 |  6351 | `	for( i = 0 ; i < SX_ARRAYSIZE(aVmIniBool) ; i++ ){` |
|        - |  6352 | `		int c;` |
|     9020 |  6353 | `		if( nVal < aVmIniBool[i].nWord` |
|     6025 |  6354 | `		 \|\| SyStrnicmp(zVal,aVmIniBool[i].zWord,aVmIniBool[i].nWord) != 0 ){` |
|     8966 |  6355 | `			continue;` |
|        - |  6356 | `		}` |
|       60 |  6357 | `		c = nVal == aVmIniBool[i].nWord ? -1 : (unsigned char)zVal[aVmIniBool[i].nWord];` |
|       58 |  6358 | `		if( c < 0 \|\| VmIniExprIsOp(c) \|\| c == '(' \|\| c == ')' \|\| c == '~'` |
|       20 |  6359 | `		 \|\| c == '!' \|\| c == '"' \|\| c == '\'' \|\| c == '$' \|\| c == ';'` |
|       20 |  6360 | `		 \|\| c == ' ' \|\| c == '\t' ){` |
|       58 |  6361 | `			if( c > 0 ){` |
|        - |  6362 | `				/* The word production eats the blanks behind it, so what php` |
|        - |  6363 | `				 * scans next starts at the first non-blank. */` |
|       26 |  6364 | `				const char *zRest = &zVal[aVmIniBool[i].nWord];` |
|       26 |  6365 | `				const char *zStop = &zVal[nVal];` |
|       54 |  6366 | `				while( zRest < zStop && (zRest[0] == ' ' \|\| zRest[0] == '\t') ){` |
|       16 |  6367 | `					zRest++;` |
|      ! 0 |  6368 | `				}` |
|       26 |  6369 | `				VmIniBadToken(zRest,zStop,pBad);` |
|       12 |  6370 | `			}` |
|       58 |  6371 | `			SyBlobAppend(pOut,aVmIniBool[i].zText,(sxu32)SyStrlen(aVmIniBool[i].zText));` |
|       58 |  6372 | `			return 1;` |
|        - |  6373 | `		}` |
|        2 |  6374 | `	}` |
|     1117 |  6375 | `	sIn.pVm = pVm;` |
|     1117 |  6376 | `	sIn.zCur = zVal;` |
|     1117 |  6377 | `	sIn.zEnd = &zVal[nVal];` |
|     1117 |  6378 | `	sIn.nLine = 0;` |
|     1117 |  6379 | `	sIn.bRawEof = 0;` |
|     1117 |  6380 | `	sIn.bStop = 0;` |
|     1117 |  6381 | `	sIn.cStop = 0;` |
|     1117 |  6382 | `	sIn.bExpect = 0;` |
|     1117 |  6383 | `	sIn.zTok = 0;` |
|     1117 |  6384 | `	sIn.zExpect = 0;` |
|     1117 |  6385 | `	if( !VmIniExprEval(&sIn,pOut,0) ){` |
|      110 |  6386 | `		pBad->bSet = 1;` |
|      110 |  6387 | `		pBad->zTok = sIn.zTok;` |
|      110 |  6388 | `		pBad->cChar = sIn.cStop;` |
|      110 |  6389 | `		pBad->bExpect = sIn.bExpect;` |
|      110 |  6390 | `		pBad->zExpect = sIn.zExpect;` |
|      110 |  6391 | `		pBad->nLine = sIn.nLine;` |
|      110 |  6392 | `		return 0;` |
|        - |  6393 | `	}` |
|        - |  6394 | ``	/* Whatever is left -- `)`, `1&&2`'s second `&`, an unresolved `~`, and`` |
|        - |  6395 | ``	 * after a closing paren anything at all (`(1)x`, `(1)0`, `(1)'a'`) -- is a`` |
|        - |  6396 | `	 * separate token the grammar cannot take from here, but the expr already` |
|        - |  6397 | `	 * reduced and its value already stands. */` |
|     1007 |  6398 | `	VmIniBadToken(sIn.zCur,sIn.zEnd,pBad);` |
|     1007 |  6399 | `	pBad->nLine = sIn.nLine;` |
|     1007 |  6400 | `	return 1;` |
|      590 |  6401 | `}` |
|        - |  6402 | `/*` |
|        - |  6403 | `` * php's own unbuffered ini-parser warning: `PHP:  syntax error, unexpected`` |
|        - |  6404 | `` * '<c>' in <file> on line <N>\n`, written straight to the engine's error`` |
|        - |  6405 | ` * consumer with none of error_reporting/display_errors/log_errors in the` |
|        - |  6406 | ` * way. That is php's own rule (zend_ini_parser.c's ini_error(), the` |
|        - |  6407 | ` * ini_parser_unbuffered_errors branch): those three knobs are not` |
|        - |  6408 | ` * trustworthy gates here because the refusal may be setting one of them.` |
|        - |  6409 | ` * Silent when the host never wired PH7_CONFIG_ERR_OUTPUT, or never supplied` |
|        - |  6410 | ` * a file for this entry (an embedder that does not pass one gets nothing,` |
|        - |  6411 | ` * same as it gets nothing from any other early diagnostic).` |
|        - |  6412 | ` */` |
|      360 |  6413 | `static void VmIniSyntaxWarning(ph7_vm *pVm,SyString *pFile,sxu32 nLine,int iStop,const VmIniBad *pBad)` |
|        3 |  6414 | `{` |
|        - |  6415 | `	SyBlob sMsg;` |
|      363 |  6416 | `	ProcConsumer xErr = pVm->pEngine->xConf.xErr;` |
|      363 |  6417 | `	if( xErr == 0 \|\| pFile == 0 \|\| pFile->nByte == 0 ){` |
|      ! 0 |  6418 | `		return;` |
|        - |  6419 | `	}` |
|      363 |  6420 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  6421 | `	/* A value is one scanner input: a double-quoted run inside it may have` |
|        - |  6422 | `	 * carried php's line counter past several newlines before the stop. */` |
|      363 |  6423 | `	nLine += pBad->nLine;` |
|      363 |  6424 | `	SyBlobAppend(&sMsg,"PHP:  syntax error, unexpected ",sizeof("PHP:  syntax error, unexpected ")-1);` |
|      363 |  6425 | `	if( pBad->zTok ){` |
|        - |  6426 | `		/* A token php's grammar declares under a symbol of its own prints as` |
|        - |  6427 | `		 * that symbol, with no quotes around it. */` |
|      220 |  6428 | `		SyBlobAppend(&sMsg,pBad->zTok,(sxu32)SyStrlen(pBad->zTok));` |
|      253 |  6429 | `	}else if( pBad->cChar != 0 ){` |
|       79 |  6430 | `		SyBlobFormat(&sMsg,"'%c'",pBad->cChar);` |
|      102 |  6431 | `	}else if( iStop == PH7_INI_STOP_COMMENT ){` |
|        - |  6432 | ``		/* A value that ran out inside a `;` comment no newline ever closed: the`` |
|        - |  6433 | `		 * comment rule wants that newline, so nothing matches at all and what` |
|        - |  6434 | `		 * the parser is handed is the end of the input rather than a line's end.` |
|        - |  6435 | `		 * It stays on the directive's own line. */` |
|        6 |  6436 | `		SyBlobAppend(&sMsg,"end of file",sizeof("end of file")-1);` |
|        3 |  6437 | `	}else{` |
|        - |  6438 | `		/* A value that ran out: php's scanner has already taken the newline, so` |
|        - |  6439 | `		 * its token is END_OF_LINE and it is dated to the line after the one the` |
|        - |  6440 | `		 * directive was written on -- but only when there WAS a newline to take.` |
|        - |  6441 | `		 * The last line of a source that ends without one is still refused under` |
|        - |  6442 | `		 * END_OF_LINE, and stays where it was written. */` |
|       58 |  6443 | `		SyBlobAppend(&sMsg,"END_OF_LINE",sizeof("END_OF_LINE")-1);` |
|       58 |  6444 | `		if( iStop == PH7_INI_STOP_EOL ){` |
|       42 |  6445 | `			nLine++;` |
|       21 |  6446 | `		}` |
|        - |  6447 | `	}` |
|      363 |  6448 | `	if( pBad->bExpect ){` |
|       12 |  6449 | `		SyBlobAppend(&sMsg,", expecting '^' or '\|' or '&' or ')'",` |
|        - |  6450 | `			sizeof(", expecting '^' or '\|' or '&' or ')'")-1);` |
|      357 |  6451 | `	}else if( pBad->zExpect ){` |
|      130 |  6452 | `		SyBlobFormat(&sMsg,", expecting %s",pBad->zExpect);` |
|       65 |  6453 | `	}` |
|      363 |  6454 | `	SyBlobFormat(&sMsg," in %.*s on line %u\n",` |
|      360 |  6455 | `		(int)pFile->nByte,pFile->zString,(unsigned)nLine);` |
|      363 |  6456 | `	xErr(SyBlobData(&sMsg),SyBlobLength(&sMsg),pVm->pEngine->xConf.pErrData);` |
|      363 |  6457 | `	SyBlobRelease(&sMsg);` |
|      183 |  6458 | `}` |
|        - |  6459 | `/*` |
|        - |  6460 | ` * Apply one php.ini directive to a VM: queue an allocator-owned copy for the INI` |
|        - |  6461 | ` * chunk's lazy seed, then arm the C-side knobs that have to hold whether or not` |
|        - |  6462 | ` * the script ever touches the INI API. Shared by PH7_VM_CONFIG_INI_ENTRY, which` |
|        - |  6463 | ` * hands a directive to a VM that already exists, and by the engine-level replay a` |
|        - |  6464 | ` * fresh VM runs before it compiles anything (PH7_VmApplyEngineIni). zFile/nLine` |
|        - |  6465 | ` * locate the directive for VmIniSyntaxWarning; an empty zFile leaves a refusal` |
|        - |  6466 | ` * silent, matching an embedder that never supplied one.` |
|        - |  6467 | ` */` |
|     2572 |  6468 | `static sxi32 VmSetIniEntry(ph7_vm *pVm,const char *zName,const char *zValue,` |
|        - |  6469 | `	const char *zFile,sxu32 nLine,int iStop,int *pbBad)` |
|        4 |  6470 | `{` |
|     2576 |  6471 | `	sxi32 rc = SXRET_OK;` |
|        - |  6472 | `	VmIniEntry sEntry;` |
|        - |  6473 | `	SyBlob sEval;` |
|        - |  6474 | `	SyString sFile;` |
|     2576 |  6475 | `	int bLevel = 0;` |
|        - |  6476 | `	VmIniBad sBad;` |
|        - |  6477 | `	char *zDupN,*zDupV;` |
|        - |  6478 | `	sxu32 nName,nValue;` |
|     2576 |  6479 | `	if( pbBad ){` |
|     2576 |  6480 | `		*pbBad = 0;` |
|     1285 |  6481 | `	}` |
|     2576 |  6482 | `	if( iStop == PH7_INI_STOP_STMT ){` |
|        - |  6483 | `		/* Not a directive either: the text a host's scanner found where php` |
|        - |  6484 | `		 * reads a directive NAME. php reads a name out of most of it and` |
|        - |  6485 | `		 * needs nothing done -- its php.ini callback ignores a statement` |
|        - |  6486 | `		 * carrying no value, and one carrying a value arrives on its own --` |
|        - |  6487 | `		 * but where its scanner hands the parser a token instead, the source` |
|        - |  6488 | `		 * is refused from here down. The table that decides is the ini value` |
|        - |  6489 | `		 * grammar's own, one file away from here rather than copied into` |
|        - |  6490 | `		 * every host that walks a source. */` |
|        - |  6491 | `		char zTok[8];` |
|     1285 |  6492 | `		const char *zBad = VmIniStmtToken(zName,(sxu32)SyStrlen(zName),zTok);` |
|     1285 |  6493 | `		if( zBad == 0 ){` |
|     1227 |  6494 | `			return SXRET_OK;` |
|        - |  6495 | `		}` |
|       58 |  6496 | `		SyZero(&sBad,sizeof(sBad));` |
|       58 |  6497 | `		sBad.zTok = zBad;` |
|       58 |  6498 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|       58 |  6499 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|       58 |  6500 | `		if( pbBad ){` |
|       58 |  6501 | `			*pbBad = 1;` |
|       29 |  6502 | `		}` |
|       58 |  6503 | `		return SXRET_OK;` |
|        - |  6504 | `	}` |
|     1295 |  6505 | `	if( iStop == PH7_INI_STOP_OFFSET \|\| iStop == PH7_INI_STOP_OFFSET_EOF ){` |
|        - |  6506 | ``		/* Not a directive: an offset statement whose `]` was not followed by`` |
|        - |  6507 | ``		 * the `=` its grammar demands. php refuses the source from here down`` |
|        - |  6508 | `		 * the way any other statement-position token does, and appends the one` |
|        - |  6509 | `		 * thing it could have taken. */` |
|        - |  6510 | `		char zTok[8];` |
|       24 |  6511 | `		const char *zBad = VmIniOffsetToken(zName,(sxu32)SyStrlen(zName),zTok);` |
|       24 |  6512 | `		SyZero(&sBad,sizeof(sBad));` |
|       24 |  6513 | `		if( zBad ){` |
|       14 |  6514 | `			if( zBad == zTok ){` |
|        6 |  6515 | `				sBad.cChar = (unsigned char)zTok[1];` |
|        3 |  6516 | `			}else{` |
|        8 |  6517 | `				sBad.zTok = zBad;` |
|      ! 0 |  6518 | `			}` |
|       17 |  6519 | `		}else if( iStop == PH7_INI_STOP_OFFSET_EOF ){` |
|        - |  6520 | `			/* nothing left to read at all: the newline rule the END_OF_LINE` |
|        - |  6521 | `			 * comes from needs a newline, and there is none */` |
|        2 |  6522 | `			sBad.zTok = "end of file";` |
|        1 |  6523 | `		}else{` |
|        - |  6524 | `			/* END_OF_LINE, and php's newline rule has already counted it */` |
|        8 |  6525 | `			sBad.nLine = 1;` |
|        - |  6526 | `		}` |
|       24 |  6527 | `		sBad.zExpect = "'='";` |
|       24 |  6528 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|       24 |  6529 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|       24 |  6530 | `		if( pbBad ){` |
|       24 |  6531 | `			*pbBad = 1;` |
|       12 |  6532 | `		}` |
|       24 |  6533 | `		return SXRET_OK;` |
|        - |  6534 | `	}` |
|     1271 |  6535 | `	if( iStop == PH7_INI_STOP_SECTION_VAR ){` |
|        - |  6536 | ``		/* A `${` run written inside a section name or an offset. php's`` |
|        - |  6537 | `		 * ST_VARNAME and ST_VAR_FALLBACK are pushed from its section, offset` |
|        - |  6538 | `		 * and value states alike, so the run is read by the value grammar's` |
|        - |  6539 | `		 * own variable rule and refused under the same tokens -- a source that` |
|        - |  6540 | ``		 * writes `[${A:-x}]` is refused for the one-letter name php's scanner`` |
|        - |  6541 | ``		 * loses to its fallback rule, exactly as `p = ${A:-x}` is. Where php`` |
|        - |  6542 | `		 * reads the substitution there is nothing to say and nothing to store:` |
|        - |  6543 | `		 * the host walks a section header for its extent, not for a directive. */` |
|        - |  6544 | `		VmIniExpr sVar;` |
|        - |  6545 | `		SyBlob sOut;` |
|        - |  6546 | `		int bRead;` |
|       52 |  6547 | `		nName = (sxu32)SyStrlen(zName);` |
|       52 |  6548 | `		if( nName < 2 ){` |
|      ! 0 |  6549 | `			return SXRET_OK;` |
|        - |  6550 | `		}` |
|       52 |  6551 | `		SyZero(&sVar,sizeof(sVar));` |
|       52 |  6552 | `		sVar.pVm = pVm;` |
|       52 |  6553 | `		sVar.zCur = zName;` |
|       52 |  6554 | `		sVar.zEnd = &zName[nName];` |
|       52 |  6555 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       52 |  6556 | `		bRead = VmIniExprVar(&sVar,&sOut,0);` |
|       52 |  6557 | `		SyBlobRelease(&sOut);` |
|       52 |  6558 | `		if( bRead ){` |
|       16 |  6559 | `			return SXRET_OK;` |
|        - |  6560 | `		}` |
|       36 |  6561 | `		SyZero(&sBad,sizeof(sBad));` |
|       36 |  6562 | `		sBad.zTok = sVar.zTok;` |
|       36 |  6563 | `		sBad.cChar = sVar.cStop;` |
|       36 |  6564 | `		sBad.bExpect = sVar.bExpect;` |
|       36 |  6565 | `		sBad.zExpect = sVar.zExpect;` |
|       36 |  6566 | `		sBad.nLine = sVar.nLine;` |
|       36 |  6567 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|       36 |  6568 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|       36 |  6569 | `		if( pbBad ){` |
|       36 |  6570 | `			*pbBad = 1;` |
|       18 |  6571 | `		}` |
|       36 |  6572 | `		return SXRET_OK;` |
|        - |  6573 | `	}` |
|     1219 |  6574 | `	if( iStop >= PH7_INI_STOP_SECTION ){` |
|        - |  6575 | ``		/* Not a directive: a `[` the host's scanner never found a `]` for. php's`` |
|        - |  6576 | ``		 * section name is one scanner run that ends at the `]` -- a newline is`` |
|        - |  6577 | `		 * not an end, so the run reaches the end of the FILE and refuses it` |
|        - |  6578 | `		 * there, dated to where the run stopped counting. The directives above` |
|        - |  6579 | `		 * it stand, and everything below is inside a section header that never` |
|        - |  6580 | `		 * closed, so nothing below is ever seen. */` |
|       42 |  6581 | `		SyZero(&sBad,sizeof(sBad));` |
|       42 |  6582 | `		sBad.zTok = "end of file";` |
|       42 |  6583 | `		sBad.zExpect = iStop == PH7_INI_STOP_SECTION_STR` |
|        - |  6584 | `			? "TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'"` |
|       21 |  6585 | `			: "']'";` |
|       42 |  6586 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|       42 |  6587 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|       42 |  6588 | `		if( pbBad ){` |
|       42 |  6589 | `			*pbBad = 1;` |
|       21 |  6590 | `		}` |
|       42 |  6591 | `		return SXRET_OK;` |
|        - |  6592 | `	}` |
|     1177 |  6593 | `	if( SX_EMPTY_STR(zName) ){` |
|      ! 0 |  6594 | `		return SXERR_EMPTY;` |
|        - |  6595 | `	}` |
|     1177 |  6596 | `	if( zValue == 0 ){` |
|      ! 0 |  6597 | `		zValue = "";` |
|      ! 0 |  6598 | `	}` |
|     1177 |  6599 | `	SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|     1177 |  6600 | `	nName = (sxu32)SyStrlen(zName);` |
|     1177 |  6601 | `	nValue = (sxu32)SyStrlen(zValue);` |
|     1177 |  6602 | `	SyBlobInit(&sEval,&pVm->sAllocator);` |
|     1245 |  6603 | `	bLevel = nName == sizeof("error_reporting")-1` |
|     1173 |  6604 | `	      && SyMemcmp(zName,"error_reporting",nName) == 0;` |
|        - |  6605 | `	/* php stores what its ini parser MADE of the value, not what was typed, and` |
|        - |  6606 | `	 * it runs that parser over EVERY directive: ini_get() shows "30711" for` |
|        - |  6607 | ``	 * `E_ALL & ~E_NOTICE` and "1" for `On` whatever the name in front of it, and`` |
|        - |  6608 | `	 * ini_restore() then has that text to re-apply rather than an expression the` |
|        - |  6609 | `	 * runtime setter would read as 0. */` |
|     1177 |  6610 | `	if( !VmIniEvalValue(pVm,zValue,nValue,&sEval,&sBad) ){` |
|        - |  6611 | `		/* php's ini parser calls this a syntax error, and a refused directive` |
|        - |  6612 | `		 * never lands at all -- its default stands, rather than the raw text or` |
|        - |  6613 | `		 * a zero standing in for it. It is still an error it REPORTS, and one` |
|        - |  6614 | ``		 * that stops the rest of the source: `precision = 1 & )` leaves`` |
|        - |  6615 | `		 * precision alone, warns, and takes every later line down with it. */` |
|      110 |  6616 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|      110 |  6617 | `		if( pbBad ){` |
|      110 |  6618 | `			*pbBad = 1;` |
|       55 |  6619 | `		}` |
|      110 |  6620 | `		SyBlobRelease(&sEval);` |
|      110 |  6621 | `		return SXRET_OK;` |
|        - |  6622 | `	}` |
|     1067 |  6623 | `	if( sBad.bSet ){` |
|       93 |  6624 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|       93 |  6625 | `		if( pbBad ){` |
|       93 |  6626 | `			*pbBad = 1;` |
|       45 |  6627 | `		}` |
|       45 |  6628 | `	}` |
|     1067 |  6629 | `	nValue = SyBlobLength(&sEval);` |
|     1067 |  6630 | `	zValue = nValue > 0 ? (const char *)SyBlobData(&sEval) : "";` |
|     1067 |  6631 | `	zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|     1067 |  6632 | `	zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|     1067 |  6633 | `	SyBlobRelease(&sEval);` |
|        - |  6634 | `	/* From here on the evaluated TEXT is the allocator's copy: the blob it was` |
|        - |  6635 | `	 * built in is gone, and every knob armed below reads this value. */` |
|     1067 |  6636 | `	zValue = zDupV ? zDupV : "";` |
|     1067 |  6637 | `	if( zDupN == 0 \|\| zDupV == 0 ){` |
|      ! 0 |  6638 | `		return SXERR_MEM;` |
|        - |  6639 | `	}` |
|     1067 |  6640 | `	SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|     1067 |  6641 | `	SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|     1067 |  6642 | `	sEntry.sFile.zString = 0;` |
|     1067 |  6643 | `	sEntry.sFile.nByte = 0;` |
|     1067 |  6644 | `	sEntry.nLine = 0;` |
|     1067 |  6645 | `	rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|     1067 |  6646 | `	if( rc == SXRET_OK ){` |
|     1067 |  6647 | `		if( bLevel ){` |
|        - |  6648 | `			/* The LEVEL, not an on/off gate. It used to move bErrReport alone, so` |
|        - |  6649 | ``			 * `-d error_reporting=2` left the mask at E_ALL and printed every`` |
|        - |  6650 | `			 * severity it was set to hide. The text read here is the grammar's,` |
|        - |  6651 | `			 * so it is always a number: a value php's ini parser refuses never` |
|        - |  6652 | `			 * reaches this far, and reading one as 0 silenced the whole run over` |
|        - |  6653 | `			 * one stray operator. */` |
|       98 |  6654 | `			sxi64 iLevel = 0;` |
|       98 |  6655 | `			if( nValue > 0 ){` |
|       98 |  6656 | `				SyStrToInt64(zDupV,nValue,(void *)&iLevel,0);` |
|       47 |  6657 | `			}` |
|       98 |  6658 | `			pVm->iErrMask = (sxi32)iLevel;` |
|       98 |  6659 | `			pVm->bErrReport = pVm->iErrMask != 0;` |
|       98 |  6660 | `			pVm->bErrMaskSet = 1;` |
|        - |  6661 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     1020 |  6662 | `		}else if( nName == sizeof("memory_limit")-1` |
|      498 |  6663 | `		 && SyMemcmp(zName,"memory_limit",nName) == 0 ){` |
|        - |  6664 | ``			/* Arm the allocator ceiling now: `-d memory_limit=32M` has to hold`` |
|        - |  6665 | `			 * for the whole run, and the INI chunk that would otherwise carry it` |
|        - |  6666 | `			 * is seeded lazily -- by which time a runaway script has already` |
|        - |  6667 | `			 * taken the box.` |
|        - |  6668 | `			 *` |
|        - |  6669 | `			 * Guarded because the applier lives in vm_builtin_ini.c, which the` |
|        - |  6670 | `			 * tiny build compiles away wholesale: an unguarded call here links` |
|        - |  6671 | ``			 * fine in `full` and fails ONLY in tiny, which is the one build the`` |
|        - |  6672 | `			 * ASan and Windows gates do not cover. */` |
|        4 |  6673 | `			PH7_VmApplyMemoryLimit(&(*pVm),zValue,nValue);` |
|        - |  6674 | `#endif` |
|      967 |  6675 | `		}else if( nName == sizeof("date.timezone")-1` |
|      498 |  6676 | `		 && SyMemcmp(zName,"date.timezone",nName) == 0 ){` |
|        - |  6677 | ``			/* `-d date.timezone=...` takes what the directive takes: a`` |
|        - |  6678 | `			 * tz-database identifier, or UTC/GMT when there is no database.` |
|        - |  6679 | `			 * Anything else leaves the default at UTC, silently, which is` |
|        - |  6680 | `			 * php's answer for an ini value it cannot resolve at startup. */` |
|        - |  6681 | `#ifdef PH7_ENABLE_TZDB` |
|        6 |  6682 | `			if( nValue > 0 && (sxu32)nValue < sizeof(pVm->zDefTz)` |
|        8 |  6683 | `			 && PH7_TzFind(zValue,nValue) >= 0 ){` |
|        8 |  6684 | `				SyMemcpy(zValue,pVm->zDefTz,(sxu32)nValue);` |
|        8 |  6685 | `				pVm->zDefTz[nValue] = 0;` |
|        8 |  6686 | `				pVm->nDefTz = (sxu32)nValue;` |
|        5 |  6687 | `			}else` |
|        - |  6688 | `#endif` |
|      ! 0 |  6689 | `			if( nValue == 3` |
|      ! 0 |  6690 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|      ! 0 |  6691 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|      ! 0 |  6692 | `				pVm->zDefTz[3] = 0;` |
|      ! 0 |  6693 | `				pVm->nDefTz = 3;` |
|      ! 0 |  6694 | `			}` |
|      964 |  6695 | `		}else if( nName == sizeof("zend.assertions")-1` |
|      522 |  6696 | `		 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|        - |  6697 | `			/* zend.assertions is a compile-time switch: 1 makes assert()` |
|        - |  6698 | `			 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|        - |  6699 | `			 * effect even before the INI chunk is seeded. */` |
|       40 |  6700 | `			sxi64 iZend = 0;` |
|       40 |  6701 | `			SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|       40 |  6702 | `			if( iZend >= 1 ){` |
|       40 |  6703 | `				pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|       22 |  6704 | `			}else{` |
|      ! 0 |  6705 | `				pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|        - |  6706 | `			}` |
|      945 |  6707 | `		}else if( nName == sizeof("display_errors")-1` |
|      551 |  6708 | `		 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|        - |  6709 | `			/* Mirror the display_errors DESTINATION C-side so it takes effect` |
|        - |  6710 | `			 * even if the script never touches the INI API (ini_set keeps it in` |
|        - |  6711 | `			 * sync at runtime via __ini_apply_err). */` |
|      176 |  6712 | `			pVm->iDisplayErrors = PH7_VmDisplayErrorsMode(zValue,nValue);` |
|      841 |  6713 | `		}else if( nName == sizeof("log_errors")-1` |
|      575 |  6714 | `		 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|      170 |  6715 | `			pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|      672 |  6716 | `		}else if( nName == sizeof("error_log")-1` |
|      395 |  6717 | `		 && SyMemcmp(zName,"error_log",nName) == 0 ){` |
|        - |  6718 | `			/* The destination has to reach the VM here and not only through the` |
|        - |  6719 | ``			 * INI chunk, whose seed is lazy: `-d error_log=…` is set precisely so`` |
|        - |  6720 | `			 * that the diagnostics of a run that never touches the INI API land` |
|        - |  6721 | `			 * in the file. Empty clears it back to the SAPI logger, which is what` |
|        - |  6722 | `			 * php's unset destination means. */` |
|       68 |  6723 | `			SyBlobReset(&pVm->sErrLogPath);` |
|       68 |  6724 | `			if( nValue > 0 ){` |
|       64 |  6725 | `				SyBlobAppend(&pVm->sErrLogPath,zValue,nValue);` |
|       32 |  6726 | `			}` |
|       68 |  6727 | `			SyBlobNullAppend(&pVm->sErrLogPath);` |
|      551 |  6728 | `		}else if( nName == sizeof("include_path")-1` |
|      266 |  6729 | `		 && SyMemcmp(zName,"include_path",nName) == 0` |
|       16 |  6730 | `		 && nValue > 0 ){` |
|        - |  6731 | `			/* The path SET is the store this directive names, and the INI` |
|        - |  6732 | ``			 * chunk's seed is lazy -- so `-d include_path=…` has to reach it`` |
|        - |  6733 | `			 * here or a script that never touches the INI API keeps looking` |
|        - |  6734 | `			 * in the default directory. Empty is refused, as php's` |
|        - |  6735 | `			 * OnUpdateStringUnempty refuses it. */` |
|       10 |  6736 | `			PH7_VmSetIncludePath(pVm,zValue,nValue);` |
|        4 |  6737 | `		}` |
|      531 |  6738 | `	}` |
|     1067 |  6739 | `	return rc;` |
|     1289 |  6740 | `}` |
|        - |  6741 | `/*` |
|        - |  6742 | ` * Seed a brand-new VM from the ENGINE's configuration: the reporting level, then` |
|        - |  6743 | ` * the php.ini directives the host handed over with PH7_CONFIG_INI_ENTRY. Called` |
|        - |  6744 | ` * before the unit is compiled, because a compile diagnostic owes the same three` |
|        - |  6745 | ` * gates a runtime one does and ph7_compile_file is what creates the VM -- a` |
|        - |  6746 | ` * directive that only ever reached the FINISHED VM arrived after every diagnostic` |
|        - |  6747 | ` * the unit's own compile could raise. The ini replay runs last so` |
|        - |  6748 | `` * `-d error_reporting=0` still lowers what PH7_CONFIG_ERR_REPORT raised.`` |
|        - |  6749 | ` */` |
|     7925 |  6750 | `PH7_PRIVATE void PH7_VmApplyEngineIni(ph7_vm *pVm)` |
|        5 |  6751 | `{` |
|     7930 |  6752 | `	ph7_conf *pConf = &pVm->pEngine->xConf;` |
|        - |  6753 | `	VmIniEntry *aEntry;` |
|        - |  6754 | `	SyString sSkip;` |
|        - |  6755 | `	sxu32 i;` |
|     7930 |  6756 | `	if( pConf->bErrReport ){` |
|     7900 |  6757 | `		pVm->bErrReport = 1;` |
|     7900 |  6758 | `		pVm->iErrMask = PH7_E_ALL_MASK;` |
|     7900 |  6759 | `		pVm->bErrMaskSet = 1;` |
|     3942 |  6760 | `	}` |
|        - |  6761 | `	/* php hands each ini SOURCE to its parser whole -- the php.ini file is one` |
|        - |  6762 | `	 * parse and the CLI's joined -d buffer is another -- so a syntax error does` |
|        - |  6763 | `	 * not just drop its own directive: bison stops, and every directive still to` |
|        - |  6764 | `	 * come in that source is never seen. A bad php.ini line therefore takes the` |
|        - |  6765 | `	 * lines under it down while leaving -d alone, and a bad -d takes the -d's` |
|        - |  6766 | `	 * behind it while leaving php.ini alone. The queue here is per directive, so` |
|        - |  6767 | `	 * the source is the file it came from; an entry with no file is an embedder` |
|        - |  6768 | `	 * handing over one directive at a time rather than a parsed source, and` |
|        - |  6769 | `	 * nothing follows it into the skip. */` |
|     7930 |  6770 | `	sSkip.zString = 0;` |
|     7930 |  6771 | `	sSkip.nByte = 0;` |
|     7930 |  6772 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pConf->aIniEntry);` |
|    10882 |  6773 | `	for( i = 0 ; i < SySetUsed(&pConf->aIniEntry) ; ++i ){` |
|     2956 |  6774 | `		int bBad = 0;` |
|     2952 |  6775 | `		if( sSkip.nByte > 0 && aEntry[i].sFile.nByte == sSkip.nByte` |
|      386 |  6776 | `		 && SyMemcmp(aEntry[i].sFile.zString,sSkip.zString,sSkip.nByte) == 0 ){` |
|      380 |  6777 | `			continue;` |
|        - |  6778 | `		}` |
|     3861 |  6779 | `		VmSetIniEntry(pVm,aEntry[i].sName.zString,aEntry[i].sValue.zString,` |
|     2572 |  6780 | `			aEntry[i].sFile.zString,aEntry[i].nLine,aEntry[i].iStop,&bBad);` |
|     2576 |  6781 | `		if( bBad && aEntry[i].sFile.nByte > 0 ){` |
|      363 |  6782 | `			sSkip = aEntry[i].sFile;` |
|      180 |  6783 | `		}` |
|     1289 |  6784 | `	}` |
|     7930 |  6785 | `}` |
|        - |  6786 | `/*` |
|        - |  6787 | ` * Configure a working virtual machine instance.` |
|        - |  6788 | ` *` |
|        - |  6789 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  6790 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  6791 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  6792 | ` * The second argument to this function is an integer configuration option` |
|        - |  6793 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  6794 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  6795 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  6796 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  6797 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  6798 | ` */` |
|   226086 |  6799 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  6800 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  6801 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  6802 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  6803 | `	)` |
|        5 |  6804 | `{` |
|   226091 |  6805 | `	sxi32 rc = SXRET_OK;` |
|   226091 |  6806 | `	switch(nOp){` |
|     3270 |  6807 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     6534 |  6808 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     6534 |  6809 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  6810 | `		/* VM output consumer callback */` |
|        - |  6811 | `#ifdef UNTRUST` |
|        - |  6812 | `		if( xConsumer == 0 ){` |
|        - |  6813 | `			rc = SXERR_CORRUPT;` |
|        - |  6814 | `			break;` |
|        - |  6815 | `		}` |
|        - |  6816 | `#endif` |
|        - |  6817 | `		/* Install the output consumer */` |
|     6534 |  6818 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     6534 |  6819 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     6534 |  6820 | `		break;` |
|        - |  6821 | `							   }` |
|     3270 |  6822 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|     6534 |  6823 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     6534 |  6824 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  6825 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|        - |  6826 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|        - |  6827 | `		 * instead of the program-output stream. */` |
|        - |  6828 | `#ifdef UNTRUST` |
|        - |  6829 | `		if( xConsumer == 0 ){` |
|        - |  6830 | `			rc = SXERR_CORRUPT;` |
|        - |  6831 | `			break;` |
|        - |  6832 | `		}` |
|        - |  6833 | `#endif` |
|     6534 |  6834 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|     6534 |  6835 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|     6534 |  6836 | `		break;` |
|        - |  6837 | `								   }` |
|     3344 |  6838 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  6839 | `		/* Import path */` |
|        - |  6840 | `		  const char *zPath;` |
|        - |  6841 | `		  SyString sPath;` |
|     6682 |  6842 | `		  zPath = va_arg(ap,const char *);` |
|        - |  6843 | `#if defined(UNTRUST)` |
|        - |  6844 | `		  if( zPath == 0 ){` |
|        - |  6845 | `			  rc = SXERR_EMPTY;` |
|        - |  6846 | `			  break;` |
|        - |  6847 | `		  }` |
|        - |  6848 | `#endif` |
|     6682 |  6849 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  6850 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  6851 | `#ifdef __WINNT__` |
|        5 |  6852 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  6853 | `#endif` |
|    13359 |  6854 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  6855 | `		  /* Remove leading and trailing white spaces */` |
|     6682 |  6856 | `		  SyStringFullTrim(&sPath);` |
|     6682 |  6857 | `		  if( sPath.nByte > 0 ){` |
|        - |  6858 | `			  /* Store the path in the corresponding conatiner */` |
|     6682 |  6859 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     3333 |  6860 | `		  }` |
|     6682 |  6861 | `		  break;` |
|        - |  6862 | `									 }` |
|       23 |  6863 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  6864 | `		/* Run-Time Error report */` |
|       46 |  6865 | `		pVm->bErrReport = 1;` |
|       46 |  6866 | `		pVm->iErrMask = PH7_E_ALL_MASK; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|       46 |  6867 | `		pVm->bErrMaskSet = 1;` |
|       46 |  6868 | `		break;` |
|        2 |  6869 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  6870 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|        - |  6871 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|        - |  6872 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|        - |  6873 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|        - |  6874 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|        - |  6875 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|        - |  6876 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|        - |  6877 | `		 * would otherwise read as an enormous positive cap). */` |
|        5 |  6878 | `		int nDepth = va_arg(ap,int);` |
|        5 |  6879 | `		if( nDepth >= 0 ){` |
|        5 |  6880 | `			pVm->nMaxDepth = nDepth;` |
|        2 |  6881 | `		}` |
|        5 |  6882 | `		break;` |
|        - |  6883 | `									   }` |
|        5 |  6884 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|        - |  6885 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|        - |  6886 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|        - |  6887 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|        - |  6888 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|        - |  6889 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|        - |  6890 | `		 * so it is rejected as a footgun). */` |
|       12 |  6891 | `		int nDepth = va_arg(ap,int);` |
|       12 |  6892 | `		if( nDepth > 1 ){` |
|       12 |  6893 | `			pVm->nMaxNativeDepth = nDepth;` |
|        5 |  6894 | `		}` |
|       12 |  6895 | `		break;` |
|        - |  6896 | `									   }` |
|      ! 0 |  6897 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  6898 | `		/* VM output length in bytes */` |
|      ! 0 |  6899 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  6900 | `#ifdef UNTRUST` |
|        - |  6901 | `		if( pOut == 0 ){` |
|        - |  6902 | `			rc = SXERR_CORRUPT;` |
|        - |  6903 | `			break;` |
|        - |  6904 | `		}` |
|        - |  6905 | `#endif` |
|      ! 0 |  6906 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  6907 | `		break;` |
|        - |  6908 | `							   }` |
|        - |  6909 |  |
|    36860 |  6910 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  6911 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  6912 | `		/* Create a new superglobal/global variable */` |
|    73604 |  6913 | `		const char *zName = va_arg(ap,const char *);` |
|    73604 |  6914 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  6915 | `		SyHashEntry *pEntry;` |
|        - |  6916 | `		ph7_value *pObj;` |
|        - |  6917 | `		sxu32 nByte;` |
|        - |  6918 | `		sxu32 nIdx;` |
|        - |  6919 | `#ifdef UNTRUST` |
|        - |  6920 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  6921 | `			rc = SXERR_CORRUPT;` |
|        - |  6922 | `			break;` |
|        - |  6923 | `		}` |
|        - |  6924 | `#endif` |
|    73604 |  6925 | `		nByte = SyStrlen(zName);` |
|    73604 |  6926 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  6927 | `			/* Check if the superglobal is already installed */` |
|    67075 |  6928 | `			pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|    33485 |  6929 | `		}else{` |
|        - |  6930 | `			/* Query the top active VM frame */` |
|     6534 |  6931 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  6932 | `		}` |
|    73604 |  6933 | `		if( pEntry ){` |
|        - |  6934 | `			/* Variable already installed */` |
|      ! 0 |  6935 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  6936 | `			/* Extract contents */` |
|      ! 0 |  6937 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  6938 | `			if( pObj ){` |
|        - |  6939 | `				/* Overwrite old contents */` |
|      ! 0 |  6940 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  6941 | `			}` |
|      ! 0 |  6942 | `		}else{` |
|        - |  6943 | `			/* Install a new variable */` |
|    73604 |  6944 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    73604 |  6945 | `			if( pObj == 0 ){` |
|      ! 0 |  6946 | `				rc = SXERR_MEM;` |
|      ! 0 |  6947 | `				break;` |
|        - |  6948 | `			}` |
|    73604 |  6949 | `			nIdx = pObj->nIdx;` |
|        - |  6950 | `			/* Copy value */` |
|    73604 |  6951 | `			PH7_MemObjStore(pValue,pObj);` |
|    73604 |  6952 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  6953 | `				/* Install the superglobal */` |
|    67075 |  6954 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    67075 |  6955 | `				if( rc == SXRET_OK ){` |
|    67075 |  6956 | `					PH7_VmSuperNote(&(*pVm),zName,nByte);` |
|    33480 |  6957 | `				}` |
|    33485 |  6958 | `			}else{` |
|        - |  6959 | `				/* Install in the current frame */` |
|     6534 |  6960 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  6961 | `			}` |
|    73604 |  6962 | `			if( rc == SXRET_OK ){` |
|        - |  6963 | `				SyHashEntry *pRef;` |
|    73604 |  6964 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    67075 |  6965 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    33485 |  6966 | `				}else{` |
|     6534 |  6967 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  6968 | `				}` |
|        - |  6969 | `				/* Install in the reference table */` |
|    73604 |  6970 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    73604 |  6971 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  6972 | `					/* Register in the $GLOBALS array */` |
|    73604 |  6973 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    36739 |  6974 | `				}` |
|    36739 |  6975 | `			}` |
|        - |  6976 | `		}` |
|    73604 |  6977 | `		break;` |
|        - |  6978 | `									}` |
|    20077 |  6979 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  6980 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  6981 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  6982 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  6983 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  6984 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  6985 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|    40093 |  6986 | `		const char *zKey   = va_arg(ap,const char *);` |
|    40093 |  6987 | `		const char *zValue = va_arg(ap,const char *);` |
|    40093 |  6988 | `		int nLen = va_arg(ap,int);` |
|        - |  6989 | `		ph7_hashmap *pMap;` |
|        - |  6990 | `		ph7_value *pValue;` |
|    40093 |  6991 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  6992 | `			/* Extract the $_ENV superglobal */` |
|      ! 0 |  6993 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|    40093 |  6994 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  6995 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  6996 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|    40093 |  6997 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  6998 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  6999 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    40093 |  7000 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  7001 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  7002 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    40093 |  7003 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  7004 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  7005 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|    40093 |  7006 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  7007 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  7008 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  7009 | `		}else{` |
|        - |  7010 | `			/* Extract the $_SERVER superglobal */` |
|    40093 |  7011 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  7012 | `		}` |
|    40093 |  7013 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  7014 | `			/* No such entry */` |
|      ! 0 |  7015 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  7016 | `			break;` |
|        - |  7017 | `		}` |
|        - |  7018 | `		/* Point to the hashmap */` |
|    40093 |  7019 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  7020 | `		/* Perform the insertion */` |
|    40093 |  7021 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|    40093 |  7022 | `		break;` |
|        - |  7023 | `								   }` |
|     3308 |  7024 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  7025 | `		/* Script arguments */` |
|     6610 |  7026 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  7027 | `		ph7_hashmap *pMap;` |
|        - |  7028 | `		ph7_value *pValue;` |
|        - |  7029 | `		sxu32 n;` |
|        - |  7030 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|        - |  7031 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|        - |  7032 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|        - |  7033 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|     6610 |  7034 | `		if( zValue == 0 ){` |
|      ! 0 |  7035 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  7036 | `			break;` |
|        - |  7037 | `		}` |
|        - |  7038 | `		/* Extract the $argv array */` |
|     6610 |  7039 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     6610 |  7040 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  7041 | `			/* No such entry */` |
|      ! 0 |  7042 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  7043 | `			break;` |
|        - |  7044 | `		}` |
|        - |  7045 | `		/* Point to the hashmap */` |
|     6610 |  7046 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  7047 | `		/* Perform the insertion */` |
|     6610 |  7048 | `		n = (sxu32)SyStrlen(zValue);` |
|     6610 |  7049 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|     6610 |  7050 | `		break;` |
|        - |  7051 | `								  }` |
|     3270 |  7052 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|        - |  7053 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|        - |  7054 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|        - |  7055 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|        - |  7056 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|        - |  7057 | `		ph7_value *pArgv,*pServer;` |
|        - |  7058 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|        - |  7059 | `		ph7_value sArgvVal,sKey,sCount;` |
|     6534 |  7060 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|     6534 |  7061 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|     6529 |  7062 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|     6534 |  7063 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  7064 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  7065 | `			break;` |
|        - |  7066 | `		}` |
|     6534 |  7067 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|     6534 |  7068 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|        - |  7069 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|     6534 |  7070 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|     6534 |  7071 | `		if( pDup == 0 ){` |
|      ! 0 |  7072 | `			rc = SXERR_MEM;` |
|      ! 0 |  7073 | `			break;` |
|        - |  7074 | `		}` |
|     6534 |  7075 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|     6534 |  7076 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|     6534 |  7077 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     6534 |  7078 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|     6534 |  7079 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|     6534 |  7080 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|     6534 |  7081 | `		PH7_MemObjRelease(&sKey);` |
|        - |  7082 | `		/* $_SERVER['argc'] = count($argv). */` |
|     6534 |  7083 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|     6534 |  7084 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     6534 |  7085 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|     6534 |  7086 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|     6534 |  7087 | `		PH7_MemObjRelease(&sCount);` |
|     6534 |  7088 | `		PH7_MemObjRelease(&sKey);` |
|     6534 |  7089 | `		rc = SXRET_OK;` |
|     6534 |  7090 | `		break;` |
|        - |  7091 | `								  }` |
|      ! 0 |  7092 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|        - |  7093 | `		/* A php.ini directive handed to a VM that already exists (the CLI's -d/-c` |
|        - |  7094 | `		 * queue reaches a fresh one through PH7_CONFIG_INI_ENTRY instead). */` |
|      ! 0 |  7095 | `		const char *zName = va_arg(ap,const char *);` |
|      ! 0 |  7096 | `		const char *zValue = va_arg(ap,const char *);` |
|      ! 0 |  7097 | `		const char *zFile = va_arg(ap,const char *);` |
|      ! 0 |  7098 | `		unsigned int nLine = va_arg(ap,unsigned int);` |
|      ! 0 |  7099 | `		int iStop = va_arg(ap,int);` |
|      ! 0 |  7100 | `		rc = VmSetIniEntry(pVm,zName,zValue,zFile,(sxu32)nLine,iStop,0);` |
|      ! 0 |  7101 | `		break;` |
|        - |  7102 | `								  }` |
|      ! 0 |  7103 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  7104 | `		/* error_log() consumer */` |
|      ! 0 |  7105 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  7106 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  7107 | `		break;` |
|        - |  7108 | `										}` |
|      ! 0 |  7109 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  7110 | `		/* Script return value */` |
|      ! 0 |  7111 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  7112 | `#ifdef UNTRUST` |
|        - |  7113 | `		if( ppValue == 0 ){` |
|        - |  7114 | `			rc = SXERR_CORRUPT;` |
|        - |  7115 | `			break;` |
|        - |  7116 | `		}` |
|        - |  7117 | `#endif` |
|      ! 0 |  7118 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  7119 | `		break;` |
|        - |  7120 | `								   }` |
|    39698 |  7121 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  7122 | `		/* Register an IO stream device */` |
|    79291 |  7123 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  7124 | `		/* Make sure we are dealing with a valid IO stream. A wrapper has to be` |
|        - |  7125 | `		 * able to do ONE of the two things a wrapper does -- open a byte stream` |
|        - |  7126 | `		 * or open a directory. php's glob:// is a dir_opener and nothing else,` |
|        - |  7127 | ``		 * and demanding xOpen here would leave `opendir('glob://…')` with no`` |
|        - |  7128 | `		 * device to reach. */` |
|    83248 |  7129 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|    79286 |  7130 | `			((pStream->xOpen == 0 \|\| pStream->xRead == 0) && pStream->xOpenDir == 0) ){` |
|        - |  7131 | `				/* Invalid stream */` |
|      ! 0 |  7132 | `				rc = SXERR_INVALID;` |
|      ! 0 |  7133 | `				break;` |
|        - |  7134 | `		}` |
|    79291 |  7135 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  7136 | `			/* Make the 'file://' stream the defaut stream device */` |
|     7930 |  7137 | `			pVm->pDefStream = pStream;` |
|     3957 |  7138 | `		}` |
|        - |  7139 | `		/* Insert in the appropriate container */` |
|    79291 |  7140 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|    79291 |  7141 | `		break;` |
|        - |  7142 | `								  }` |
|       23 |  7143 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  7144 | `		/* Point to the VM internal output consumer buffer */` |
|       46 |  7145 | `		const void **ppOut = va_arg(ap,const void **);` |
|       46 |  7146 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  7147 | `#ifdef UNTRUST` |
|        - |  7148 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  7149 | `			rc = SXERR_CORRUPT;` |
|        - |  7150 | `			break;` |
|        - |  7151 | `		}` |
|        - |  7152 | `#endif` |
|       46 |  7153 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       46 |  7154 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       46 |  7155 | `		break;` |
|        - |  7156 | `									   }` |
|       23 |  7157 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  7158 | `		/* Raw HTTP request*/` |
|       46 |  7159 | `		const char *zRequest = va_arg(ap,const char *);` |
|       46 |  7160 | `		int nByte = va_arg(ap,int);` |
|       46 |  7161 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  7162 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  7163 | `			break;` |
|        - |  7164 | `		}` |
|       46 |  7165 | `		if( nByte < 0 ){` |
|        - |  7166 | `			/* Compute length automatically */` |
|      ! 0 |  7167 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  7168 | `		}` |
|        - |  7169 | `		/* Process the request */` |
|       46 |  7170 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  7171 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       46 |  7172 | `		if( rc == SXRET_OK ){` |
|       44 |  7173 | `			pVm->bHttpContext = 1;` |
|       44 |  7174 | `			if( pVm->iResponseStatus == 0 ){` |
|        - |  7175 | `				/* A request-driven run starts at 200, which is what` |
|        - |  7176 | `				 * http_response_code() reads back before anything sets one. */` |
|       44 |  7177 | `				pVm->iResponseStatus = 200;` |
|       22 |  7178 | `			}` |
|       22 |  7179 | `		}` |
|       46 |  7180 | `		break;` |
|        - |  7181 | `									}` |
|       23 |  7182 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  7183 | `		/* Extract HTTP response status code */` |
|       46 |  7184 | `		int *pStatus = va_arg(ap, int *);` |
|       46 |  7185 | `		if( pStatus ){` |
|        - |  7186 | `			/* A response nothing set a code for goes out as 200. */` |
|       46 |  7187 | `			*pStatus = pVm->iResponseStatus ? pVm->iResponseStatus : 200;` |
|       23 |  7188 | `		}` |
|       46 |  7189 | `		break;` |
|        - |  7190 | `										}` |
|       23 |  7191 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  7192 | `		/* Iterate response headers via callback */` |
|        - |  7193 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       46 |  7194 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       46 |  7195 | `		void *pUserData = va_arg(ap, void *);` |
|       46 |  7196 | `		if( xCallback ){` |
|       46 |  7197 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       46 |  7198 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|      112 |  7199 | `			for( k = 0; k < nHdr; k++ ){` |
|       99 |  7200 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       66 |  7201 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|       33 |  7202 | `							   pUserData);` |
|       66 |  7203 | `				if( rc != PH7_OK ){` |
|      ! 0 |  7204 | `					break;` |
|        - |  7205 | `				}` |
|       33 |  7206 | `			}` |
|       23 |  7207 | `		}` |
|       46 |  7208 | `		break;` |
|        - |  7209 | `										 }` |
|      ! 0 |  7210 | `	default:` |
|        - |  7211 | `		/* Unknown configuration option */` |
|      ! 0 |  7212 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  7213 | `		break;` |
|        - |  7214 | `	}` |
|   226091 |  7215 | `	return rc;` |
|        5 |  7216 | `}` |
|        - |  7217 | `/* Forward declaration */` |
|        - |  7218 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  7219 | `/*` |
|        - |  7220 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  7221 | ` * format.` |
|        - |  7222 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  7223 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  7224 | ` * (STDOUT).` |
|        - |  7225 | ` */` |
|        2 |  7226 | `static sxi32 VmByteCodeDump(` |
|        - |  7227 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  7228 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  7229 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  7230 | `	)` |
|        1 |  7231 | `{` |
|        - |  7232 | `	static const char zDump[] = {` |
|        - |  7233 | `		"====================================================\n"` |
|        - |  7234 | `		"PH7 VM Dump\n"` |
|        - |  7235 | `		"====================================================\n"` |
|        - |  7236 | `	};` |
|        - |  7237 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  7238 | `	sxi32 rc = SXRET_OK;` |
|        - |  7239 | `	sxu32 n;` |
|        - |  7240 | `	/* Point to the PH7 instructions */` |
|        3 |  7241 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  7242 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  7243 | `	n = 0;` |
|        3 |  7244 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  7245 | `	/* Dump instructions */` |
|        6 |  7246 | `	for(;;){` |
|       13 |  7247 | `		if( pInstr >= pEnd ){` |
|        - |  7248 | `			/* No more instructions */` |
|        3 |  7249 | `			break;` |
|        - |  7250 | `		}` |
|        - |  7251 | `		/* Format and call the consumer callback */` |
|       16 |  7252 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|       10 |  7253 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  7254 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|       11 |  7255 | `		if( rc != SXRET_OK ){` |
|        - |  7256 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  7257 | `			return rc;` |
|        - |  7258 | `		}` |
|       11 |  7259 | `		++n;` |
|       11 |  7260 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  7261 | `	}` |
|        3 |  7262 | `	return rc;` |
|        2 |  7263 | `}` |
|        - |  7264 | `/*` |
|        - |  7265 | ` * Save the execution state of a fiber/generator context.` |
|        - |  7266 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  7267 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  7268 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  7269 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  7270 | ` * when VmByteCodeExec returns.` |
|        - |  7271 | ` */` |
|     1672 |  7272 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|        - |  7273 | `	ph7_vm *pVm,` |
|        - |  7274 | `	ph7_exec_ctx *pCtx,` |
|        - |  7275 | `	sxi32 pc,` |
|        - |  7276 | `	sxi32 nTos` |
|        - |  7277 | `	)` |
|        5 |  7278 | `{` |
|      836 |  7279 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|     1677 |  7280 | `	pCtx->pc = pc;` |
|     1677 |  7281 | `	pCtx->nTos = nTos;` |
|     1677 |  7282 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|     1677 |  7283 | `	return PH7_SUSPEND;` |
|        5 |  7284 | `}` |
|        - |  7285 | `/*` |
|        - |  7286 | ` * Resolve named-argument mapping.` |
|        - |  7287 | ` *` |
|        - |  7288 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  7289 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  7290 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  7291 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  7292 | ` * every formal parameter that received a value.` |
|        - |  7293 | ` *` |
|        - |  7294 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  7295 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|        - |  7296 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|        - |  7297 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|        - |  7298 | ` */` |
|      510 |  7299 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|        - |  7300 | `	ph7_vm *pVm,` |
|        - |  7301 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  7302 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  7303 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  7304 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  7305 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  7306 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  7307 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  7308 | `)` |
|        5 |  7309 | `{` |
|      515 |  7310 | `	sxi32 posIdx = 0;` |
|        - |  7311 | `	sxu32 i;` |
|      515 |  7312 | `	int bSeenNamed = 0;` |
|        - |  7313 | `	char zErrMsg[256];` |
|      515 |  7314 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|     1661 |  7315 | `	for( i = 0; i < nActual; i++ ){` |
|     1151 |  7316 | `		aSlot[i] = -2;` |
|      578 |  7317 | `	}` |
|     1637 |  7318 | `	for( i = 0; i < nActual; i++ ){` |
|     1525 |  7319 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  7320 | `			/* Named argument — find formal by name */` |
|      773 |  7321 | `			int found = 0;` |
|      773 |  7322 | `			bSeenNamed = 1;` |
|        - |  7323 | `			sxu32 k;` |
|     1219 |  7324 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|     1012 |  7325 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      989 |  7326 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      956 |  7327 | `						pMap->aNames[i].zString,` |
|     1434 |  7328 | `						pMap->aNames[i].nByte) == 0 ){` |
|      571 |  7329 | `					if( aUsed[k] ){` |
|       19 |  7330 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  7331 | `							"Named parameter $%.*s overwrites previous argument",` |
|       10 |  7332 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|       14 |  7333 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - |  7334 | `					}` |
|      561 |  7335 | `					aSlot[i] = (sxi32)k;` |
|      561 |  7336 | `					aUsed[k] = 1;` |
|      561 |  7337 | `					found = 1;` |
|      561 |  7338 | `					break;` |
|        - |  7339 | `				}` |
|      228 |  7340 | `			}` |
|      763 |  7341 | `			if( !found ){` |
|      207 |  7342 | `				if( iVariadicIdx >= 0 ){` |
|      196 |  7343 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|      100 |  7344 | `				}else{` |
|       18 |  7345 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  7346 | `						"Unknown named parameter $%.*s",` |
|       10 |  7347 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|       13 |  7348 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - |  7349 | `				}` |
|       96 |  7350 | `			}` |
|      379 |  7351 | `		}else{` |
|        - |  7352 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|        - |  7353 | `			 * named arg (the parser rejects it at compile time), but a call` |
|        - |  7354 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|        - |  7355 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|      383 |  7356 | `			if( bSeenNamed ){` |
|        - |  7357 | `				/* php has two sentences for the one rule: the argument list an` |
|        - |  7358 | ``				 * UNPACK produced ends with ` during unpacking`, and the one`` |
|        - |  7359 | `				 * call_user_func_array() rebuilt from an array does not. */` |
|        5 |  7360 | `				if( pMap->bFromUnpack ){` |
|        3 |  7361 | `					return VmThrowNamedArgError(&(*pVm),` |
|        - |  7362 | `						"Cannot use positional argument after named argument during unpacking",` |
|        - |  7363 | `						sizeof("Cannot use positional argument after named argument during unpacking") - 1);` |
|        - |  7364 | `				}` |
|        3 |  7365 | `				return VmThrowNamedArgError(&(*pVm),` |
|        - |  7366 | `					"Cannot use positional argument after named argument",` |
|        - |  7367 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|        - |  7368 | `			}` |
|      379 |  7369 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       73 |  7370 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  7371 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  7372 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  7373 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  7374 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        - |  7375 | `				}` |
|       73 |  7376 | `				aSlot[i] = posIdx;` |
|       73 |  7377 | `				aUsed[posIdx] = 1;` |
|      343 |  7378 | `			}else if( iVariadicIdx >= 0 ){` |
|      309 |  7379 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      153 |  7380 | `			}` |
|      379 |  7381 | `			posIdx++;` |
|        - |  7382 | `		}` |
|      566 |  7383 | `	}` |
|      491 |  7384 | `	return SXRET_OK;` |
|      260 |  7385 | `}` |
|        - |  7386 | `/*` |
|        - |  7387 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - |  7388 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - |  7389 | ` */` |
|     1459 |  7390 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        5 |  7391 | `{` |
|     1464 |  7392 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|     1364 |  7393 | `		return 0;` |
|        - |  7394 | `	}` |
|      105 |  7395 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|      687 |  7396 | `}` |
|        - |  7397 | `/*` |
|        - |  7398 | ` * Shared body of the two Traversable-spread steps. An ARRAY source can only hand` |
|        - |  7399 | ` * over the two key types an array holds, but an ITERATOR may answer key() with` |
|        - |  7400 | `` * anything at all, so php screens it: `Keys must be of type int\|string during`` |
|        - |  7401 | `` * {array,argument} unpacking` is an Error raised at the offending element, and it`` |
|        - |  7402 | ` * refuses a float (a WHOLE one included), a bool, a null and a resource as well as` |
|        - |  7403 | ` * the two containers — this is not the offset rule set, which folds all four.` |
|        - |  7404 | ` *` |
|        - |  7405 | ` * What survives the screen follows php's ordinary 8.1 unpack rules, which are the` |
|        - |  7406 | ` * ones an array source already gets: a key that stays a STRING is kept, a key that` |
|        - |  7407 | ` * FOLDS to an integer — a canonical numeric string like "7" among them — is` |
|        - |  7408 | ` * renumbered. PH7_HashmapKeyIsInt answers that fold, and asking it is what keeps` |
|        - |  7409 | `` * `yield "7" => v` off the integer key 7 the raw insert would have written.`` |
|        - |  7410 | ` *` |
|        - |  7411 | ` * On the ARGUMENT path the kept string key is what makes the element a NAMED` |
|        - |  7412 | ` * argument: VmSpreadCaptureRun reads the temp map's node keys, so binding, the` |
|        - |  7413 | ` * unknown-name Error and the duplicate-name Error all come for free. Before this,` |
|        - |  7414 | `` * both steps threw the key away — `[...$gen]` silently renumbered a key php`` |
|        - |  7415 | `` * refuses, and `f(...$gen)` passed a named argument positionally, which is a`` |
|        - |  7416 | ` * DIFFERENT parameter's value with no diagnostic at all.` |
|        - |  7417 | ` */` |
|      104 |  7418 | `static sxi32 VmSpreadKeyedStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue,` |
|        - |  7419 | `                               ph7_hashmap *pMap, int bArgs)` |
|        4 |  7420 | `{` |
|        - |  7421 | `	SyBlob sMsg;` |
|        - |  7422 | `	sxi32 rc;` |
|        - |  7423 | `	int bKeep;` |
|      104 |  7424 | `	if( (pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT)) == 0` |
|       94 |  7425 | `	 \|\| (pKey->iFlags & (MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) ){` |
|       35 |  7426 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  7427 | `		SyBlobFormat(&sMsg,"Keys must be of type int\|string during %s unpacking",` |
|       16 |  7428 | `			bArgs ? "argument" : "array");` |
|       35 |  7429 | `		rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  7430 | `		SyBlobRelease(&sMsg);` |
|       35 |  7431 | `		return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  7432 | `	}` |
|        - |  7433 | `	/* PH7_HashmapKeyIsInt may cast pKey to a string to answer; pKey is the walk's` |
|        - |  7434 | `	 * own temporary, released the moment this step returns. */` |
|       76 |  7435 | `	bKeep = (pKey->iFlags & MEMOBJ_STRING) && !PH7_HashmapKeyIsInt(pKey);` |
|       76 |  7436 | `	if( bKeep && bArgs && PH7_HashmapLookup(pMap,pKey,0) == SXRET_OK ){` |
|        - |  7437 | `		/* Two elements under the same string key are two NAMED arguments with the` |
|        - |  7438 | `		 * same name, which php refuses. The array path lets the later one win (that` |
|        - |  7439 | `		 * IS php's array-unpack rule), but here the collision would silently drop an` |
|        - |  7440 | `		 * argument: the temp map keeps one element, so the callee would be handed a` |
|        - |  7441 | `		 * shorter list with no diagnostic. The message is the binder's own — a` |
|        - |  7442 | `		 * duplicate spread ACROSS two sources still reaches it there. */` |
|        6 |  7443 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 |  7444 | `		SyBlobFormat(&sMsg,"Named parameter $%.*s overwrites previous argument",` |
|        4 |  7445 | `			(int)SyBlobLength(&pKey->sBlob),(const char *)SyBlobData(&pKey->sBlob));` |
|        6 |  7446 | `		rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        6 |  7447 | `		SyBlobRelease(&sMsg);` |
|        6 |  7448 | `		return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  7449 | `	}` |
|       72 |  7450 | `	PH7_HashmapInsert(pMap, bKeep ? pKey : 0 /* auto-index */, pValue);` |
|       72 |  7451 | `	return SXRET_OK;` |
|       56 |  7452 | `}` |
|        - |  7453 | `/*` |
|        - |  7454 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - |  7455 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - |  7456 | ` * preserved (later wins), integer keys are renumbered.` |
|        - |  7457 | ` */` |
|       46 |  7458 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        2 |  7459 | `{` |
|       48 |  7460 | `	return VmSpreadKeyedStep(pVm,pKey,pValue,(ph7_hashmap *)pUserData,0);` |
|        2 |  7461 | `}` |
|        - |  7462 | `/*` |
|        - |  7463 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - |  7464 | ` * collect the elements into a temp array, keeping a string key so the CALL` |
|        - |  7465 | ` * replays it as a named argument.` |
|        - |  7466 | ` */` |
|       58 |  7467 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        4 |  7468 | `{` |
|       62 |  7469 | `	return VmSpreadKeyedStep(pVm,pKey,pValue,(ph7_hashmap *)pUserData,1);` |
|        4 |  7470 | `}` |
|        - |  7471 | `/*` |
|        - |  7472 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|        - |  7473 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|        - |  7474 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|        - |  7475 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|        - |  7476 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|        - |  7477 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|        - |  7478 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|        - |  7479 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|        - |  7480 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|        - |  7481 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|        - |  7482 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|        - |  7483 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|        - |  7484 | ` */` |
|        - |  7485 | `/*` |
|        - |  7486 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|        - |  7487 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|        - |  7488 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|        - |  7489 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|        - |  7490 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|        - |  7491 | ` */` |
|     1311 |  7492 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|        5 |  7493 | `{` |
|        - |  7494 | `	VmSpreadRun sRun;` |
|        - |  7495 | `	ph7_hashmap_node *pNode;` |
|        - |  7496 | `	sxu32 i;` |
|     1316 |  7497 | `	sRun.pStart = pFirst;` |
|     1316 |  7498 | `	sRun.nCount = nCount;` |
|     1316 |  7499 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|     1316 |  7500 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|     1316 |  7501 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|      ! 0 |  7502 | `		return;` |
|        - |  7503 | `	}` |
|     1316 |  7504 | `	pNode = pMap->pFirst;` |
|     4465 |  7505 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|        - |  7506 | `		VmSpreadKey sKey;` |
|     3154 |  7507 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|        - |  7508 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|        - |  7509 | `			 * the source map's release before CALL replays them. */` |
|      129 |  7510 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      129 |  7511 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|      129 |  7512 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|       66 |  7513 | `		}else{` |
|        - |  7514 | `			/* Integer key (or empty-string key, treated positionally) */` |
|     3028 |  7515 | `			sKey.nOff = 0;` |
|     3028 |  7516 | `			sKey.nLen = 0;` |
|        - |  7517 | `		}` |
|     3154 |  7518 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|     3154 |  7519 | `		pNode = pNode->pPrev; /* forward link */` |
|     1467 |  7520 | `	}` |
|      613 |  7521 | `}` |
|        - |  7522 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|        - |  7523 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|       16 |  7524 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|      ! 0 |  7525 | `{` |
|       16 |  7526 | `	SySetReset(&pVm->aSpreadRun);` |
|       16 |  7527 | `	SySetReset(&pVm->aSpreadKey);` |
|       16 |  7528 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|       16 |  7529 | `}` |
|        - |  7530 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|        - |  7531 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|        - |  7532 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|        - |  7533 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|        - |  7534 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|        - |  7535 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|        - |  7536 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|        - |  7537 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|        - |  7538 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|        - |  7539 | ` * slot — from being consumed by that nested call. */` |
|   261692 |  7540 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|        5 |  7541 | `{` |
|   261697 |  7542 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|   261697 |  7543 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|        - |  7544 | `	VmSpreadRun *aRun;` |
|   261697 |  7545 | `	if( rStart >= nRun ){` |
|   260404 |  7546 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|        - |  7547 | `	}` |
|     1298 |  7548 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|     1298 |  7549 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|     1298 |  7550 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|     1298 |  7551 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|      599 |  7552 | `	}` |
|     1298 |  7553 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|   130719 |  7554 | `}` |
|        - |  7555 | `/*` |
|        - |  7556 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|        - |  7557 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|        - |  7558 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|        - |  7559 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|        - |  7560 | ` *` |
|        - |  7561 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|        - |  7562 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|        - |  7563 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|        - |  7564 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|        - |  7565 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|        - |  7566 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|        - |  7567 | ` * they are counted only by that call. This replaces the old shared` |
|        - |  7568 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|        - |  7569 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|        - |  7570 | ` *` |
|        - |  7571 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|        - |  7572 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|        - |  7573 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|        - |  7574 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|        - |  7575 | ` */` |
|     3068 |  7576 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|        5 |  7577 | `{` |
|     3073 |  7578 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - |  7579 | `	VmSpreadRun *aRun;` |
|     3073 |  7580 | `	ph7_value *pEnd = pTos;` |
|     3073 |  7581 | `	sxi32 nPos = iP1;` |
|     3073 |  7582 | `	sxi32 ri, extra = 0;` |
|     3073 |  7583 | `	if( nRun == 0 ){` |
|       17 |  7584 | `		pVm->nSpreadCallBase = 0;` |
|       17 |  7585 | `		return 0;` |
|        - |  7586 | `	}` |
|     3057 |  7587 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|     3057 |  7588 | `	ri = (sxi32)nRun - 1;` |
|     9229 |  7589 | `	while( nPos > 0 ){` |
|     6177 |  7590 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|        - |  7591 | `			/* A non-empty unpack occupying nCount slots. */` |
|     2553 |  7592 | `			pEnd = aRun[ri].pStart;` |
|     2553 |  7593 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|     2553 |  7594 | `			ri--;` |
|     4807 |  7595 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|        - |  7596 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|      514 |  7597 | `			extra -= 1;` |
|      514 |  7598 | `			ri--;` |
|      258 |  7599 | `		}else{` |
|        - |  7600 | `			/* An ordinary single-slot argument. */` |
|     3116 |  7601 | `			pEnd--;` |
|        - |  7602 | `		}` |
|     6177 |  7603 | `		nPos--;` |
|        5 |  7604 | `	}` |
|        - |  7605 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|        - |  7606 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|     3057 |  7607 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|     3057 |  7608 | `	return extra;` |
|     1444 |  7609 | `}` |
|     1311 |  7610 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|        5 |  7611 | `{` |
|     1316 |  7612 | `	ph7_value *pTos = *ppTos;` |
|     1316 |  7613 | `	sxu32 nEntry = pMap->nEntry;` |
|     1316 |  7614 | `	if( nEntry == 0 ){` |
|        - |  7615 | `		/* Nothing to unpack — remove the source from the stack */` |
|      214 |  7616 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|      214 |  7617 | `		VmPopOperand(&pTos, 1);` |
|      108 |  7618 | `	}else{` |
|        - |  7619 | `		ph7_hashmap_node *pNode;` |
|        - |  7620 | `		ph7_value *pElem;` |
|        - |  7621 | `		sxu32 i;` |
|        - |  7622 | `		int bTemp;` |
|        - |  7623 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|        - |  7624 | `		 * so a source array that other variables share has to separate first — otherwise` |
|        - |  7625 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|        - |  7626 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|        - |  7627 | `		 * ordinary spread pays nothing for this. */` |
|     1099 |  7628 | `		if( bVarSource` |
|     1002 |  7629 | `		 && pMap != pVm->pGlobal` |
|      905 |  7630 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|      910 |  7631 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|        - |  7632 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|        - |  7633 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|        - |  7634 | `			 * object. */` |
|      910 |  7635 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|      910 |  7636 | `			if( pSep ){` |
|      910 |  7637 | `				pMap = pSep;` |
|      910 |  7638 | `				nEntry = pMap->nEntry;` |
|      405 |  7639 | `			}` |
|      405 |  7640 | `		}` |
|     1104 |  7641 | `		pMap->iRef++;` |
|     1104 |  7642 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|        - |  7643 | `		/* Record the run + element keys before any release (nodes still alive).` |
|        - |  7644 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|     1104 |  7645 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|        - |  7646 | `		/* Overwrite the source slot with the first element */` |
|     1104 |  7647 | `		pNode = pMap->pFirst;` |
|     1104 |  7648 | `		pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pNode->nValIdx);` |
|     1104 |  7649 | `		PH7_MemObjRelease(pTos);` |
|     1104 |  7650 | `		if( pElem ){` |
|     1104 |  7651 | `			if( bTemp ){` |
|      190 |  7652 | `				PH7_MemObjStore(pElem, pTos);` |
|       97 |  7653 | `			}else{` |
|      918 |  7654 | `				PH7_MemObjLoad(pElem, pTos);` |
|        - |  7655 | `			}` |
|      502 |  7656 | `		}` |
|        - |  7657 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|        - |  7658 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|        - |  7659 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|        - |  7660 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|        - |  7661 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|        - |  7662 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|        - |  7663 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|        - |  7664 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|     1104 |  7665 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|     1104 |  7666 | `		if( !bVarSource \|\| bTemp ){` |
|      198 |  7667 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       97 |  7668 | `		}` |
|        - |  7669 | `		/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7670 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|     1104 |  7671 | `		pNode = pNode->pPrev;` |
|        - |  7672 | `		/* Push the remaining elements */` |
|     3154 |  7673 | `		for( i = 1; i < nEntry; i++ ){` |
|     2055 |  7674 | `			pTos++;` |
|     2055 |  7675 | `			PH7_MemObjInit(pVm, pTos);` |
|     2055 |  7676 | `			pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pNode->nValIdx);` |
|     2055 |  7677 | `			if( pElem ){` |
|     2055 |  7678 | `				if( bTemp ){` |
|     1307 |  7679 | `					PH7_MemObjStore(pElem, pTos);` |
|      655 |  7680 | `				}else{` |
|      750 |  7681 | `					PH7_MemObjLoad(pElem, pTos);` |
|        - |  7682 | `				}` |
|      960 |  7683 | `			}` |
|     2055 |  7684 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|     2055 |  7685 | `			if( !bVarSource \|\| bTemp ){` |
|     1307 |  7686 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|      652 |  7687 | `			}` |
|     2055 |  7688 | `			pNode = pNode->pPrev;` |
|      965 |  7689 | `		}` |
|     1104 |  7690 | `		PH7_HashmapUnref(pMap);` |
|        - |  7691 | `	}` |
|     1316 |  7692 | `	*ppTos = pTos;` |
|     1316 |  7693 | `}` |
|        - |  7694 | `/*` |
|        - |  7695 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|        - |  7696 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|        - |  7697 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|        - |  7698 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|        - |  7699 | ` * element keys, interleaving them with the compile-time names at their real` |
|        - |  7700 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|        - |  7701 | ` *` |
|        - |  7702 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|        - |  7703 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|        - |  7704 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|        - |  7705 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|        - |  7706 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|        - |  7707 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|        - |  7708 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|        - |  7709 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|        - |  7710 | ` * method-name slot pop shifts pArg).` |
|        - |  7711 | ` *` |
|        - |  7712 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|        - |  7713 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|        - |  7714 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|        - |  7715 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|        - |  7716 | ` * which is after this call's synchronous named-arg resolution.` |
|        - |  7717 | ` */` |
|   260483 |  7718 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|        - |  7719 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|        5 |  7720 | `{` |
|   260488 |  7721 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|        - |  7722 | `	VmSpreadRun *aRun;` |
|        - |  7723 | `	VmSpreadKey *aKey;` |
|        - |  7724 | `	const char *zKeyBase;` |
|   260488 |  7725 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|   260488 |  7726 | `	int bAnyNamed = 0;` |
|        - |  7727 | `	sxu32 ai, ci, ri, rStart;` |
|   260488 |  7728 | `	if( nRun == 0 ){` |
|        - |  7729 | `		/* No spread captured at all — the compile map is already aligned. */` |
|   259229 |  7730 | `		return 0;` |
|        - |  7731 | `	}` |
|     1264 |  7732 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|     1264 |  7733 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|     1264 |  7734 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|        - |  7735 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|        - |  7736 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|        - |  7737 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|        - |  7738 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|     1264 |  7739 | `	ri = pVm->nSpreadCallBase;` |
|     1264 |  7740 | `	rStart = ri;` |
|     1264 |  7741 | `	if( rStart >= nRun ){` |
|        - |  7742 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|      ! 0 |  7743 | `		return 0;` |
|        - |  7744 | `	}` |
|     1264 |  7745 | `	SySetReset(&pVm->aEffArgName);` |
|     1264 |  7746 | `	ci = 0;` |
|     1264 |  7747 | `	ai = 0;` |
|     3570 |  7748 | `	while( ai < nActual ){` |
|        - |  7749 | `		SyString sName;` |
|     2311 |  7750 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|        - |  7751 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|        - |  7752 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|     2323 |  7753 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|       13 |  7754 | `			ci++; ri++;` |
|        1 |  7755 | `		}` |
|     2311 |  7756 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|        - |  7757 | `			/* A run of spread elements: one name per element from its key. Keys` |
|        - |  7758 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|        - |  7759 | `			 * run never desyncs the key stream. */` |
|     1104 |  7760 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|     4253 |  7761 | `			for( j = 0; j < K; j++ ){` |
|     3154 |  7762 | `				SyZero(&sName, sizeof(sName));` |
|     3154 |  7763 | `				if( aKey[ks + j].nLen > 0 ){` |
|      129 |  7764 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|      129 |  7765 | `					bAnyNamed = 1;` |
|       63 |  7766 | `				}` |
|     3154 |  7767 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|     1467 |  7768 | `			}` |
|     1104 |  7769 | `			ai += K;` |
|     1104 |  7770 | `			ci++; ri++;` |
|      507 |  7771 | `		}else{` |
|        - |  7772 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|     1211 |  7773 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|       38 |  7774 | `				sName = pCompile->aNames[ci];` |
|       38 |  7775 | `				bAnyNamed = 1;` |
|       18 |  7776 | `			}` |
|     1211 |  7777 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|     1211 |  7778 | `			ai++;` |
|     1211 |  7779 | `			ci++;` |
|        - |  7780 | `		}` |
|        5 |  7781 | `	}` |
|        - |  7782 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|        - |  7783 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|        - |  7784 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|        - |  7785 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|     1264 |  7786 | `	VmSpreadConsume(pVm);` |
|     1264 |  7787 | `	if( !bAnyNamed ){` |
|        - |  7788 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|     1164 |  7789 | `		return 0;` |
|        - |  7790 | `	}` |
|      103 |  7791 | `	pEff->bHasNamed = 1;` |
|      103 |  7792 | ``	pEff->bFromUnpack = 1; /* picks php's ` during unpacking` refusal wording */`` |
|      103 |  7793 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|      103 |  7794 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|      103 |  7795 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|        - |  7796 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|        - |  7797 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|        - |  7798 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|        - |  7799 | `	 * is the caller's stack local. */` |
|      103 |  7800 | `	pEff->bArgShapes = 0;` |
|      103 |  7801 | `	pEff->nNonLvalMask = 0;` |
|      103 |  7802 | `	pEff->nTempCallMask = 0;` |
|      103 |  7803 | `	if( pCompile ){` |
|       42 |  7804 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|       22 |  7805 | `	}else{` |
|       63 |  7806 | `		pEff->sAssertSrc.zString = 0;` |
|       63 |  7807 | `		pEff->sAssertSrc.nByte = 0;` |
|        - |  7808 | `	}` |
|      103 |  7809 | `	pEff->nTotal = nActual;` |
|      103 |  7810 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|      103 |  7811 | `	return 1;` |
|   130162 |  7812 | `}` |
|        - |  7813 | `/*` |
|        - |  7814 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|        - |  7815 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|        - |  7816 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|        - |  7817 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|        - |  7818 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|        - |  7819 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|        - |  7820 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|        - |  7821 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|        - |  7822 | ` * pArg must be the site's FINAL argument base.` |
|        - |  7823 | ` */` |
|  8995258 |  7824 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|        - |  7825 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|        5 |  7826 | `{` |
|  8995263 |  7827 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|  8995263 |  7828 | `	if( pInstr->iP2 == 0 ){` |
|  8734730 |  7829 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|        - |  7830 | `	}` |
|   260538 |  7831 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|      103 |  7832 | `		return pStorage;` |
|        - |  7833 | `	}` |
|   260438 |  7834 | `	VmSpreadConsume(pVm);` |
|   260438 |  7835 | `	return pCompile;` |
|  4498909 |  7836 | `}` |
|        - |  7837 | `/*` |
|        - |  7838 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|        - |  7839 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|        - |  7840 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|        - |  7841 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|        - |  7842 | ` */` |
|       38 |  7843 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|        3 |  7844 | `{` |
|       41 |  7845 | `	const char *zType = "unknown";` |
|        - |  7846 | `	char zMsg[64];` |
|       41 |  7847 | `	if( iFlags & MEMOBJ_STRING ){` |
|       10 |  7848 | `		zType = "string";` |
|       37 |  7849 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        - |  7850 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|        - |  7851 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|        - |  7852 | `		 * REAL flag, so it still falls through to the int arm. */` |
|      ! 0 |  7853 | `		zType = "float";` |
|       33 |  7854 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       31 |  7855 | `		zType = "int";` |
|       17 |  7856 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|        3 |  7857 | `		zType = "bool";` |
|        1 |  7858 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7859 | `		zType = "object";` |
|      ! 0 |  7860 | `	}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  7861 | `		zType = "resource";` |
|      ! 0 |  7862 | `	}` |
|       41 |  7863 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|       41 |  7864 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       41 |  7865 | `}` |
|        - |  7866 | `/*` |
|        - |  7867 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|        - |  7868 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|        - |  7869 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|        - |  7870 | ` * per context — see VmMemberCtxWantsValue.` |
|        - |  7871 | ` */` |
|   104912 |  7872 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|        5 |  7873 | `{` |
|   104917 |  7874 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|        5 |  7875 | `}` |
|        - |  7876 | `/*` |
|        - |  7877 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|        - |  7878 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|        - |  7879 | ` * isset() stops at the truth.` |
|        - |  7880 | ` */` |
|       56 |  7881 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|        3 |  7882 | `{` |
|       59 |  7883 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|        3 |  7884 | `}` |
|        - |  7885 | `/*` |
|        - |  7886 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|        - |  7887 | ` * A __get body reading the SAME property of the SAME instance must not` |
|        - |  7888 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|        - |  7889 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|        - |  7890 | ` * reads (other names / other instances) still dispatch.` |
|        - |  7891 | ` */` |
|     1364 |  7892 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        4 |  7893 | `{` |
|        - |  7894 | `	VmMagicGuard *aG;` |
|        - |  7895 | `	sxu32 nHash;` |
|        - |  7896 | `	sxu32 n;` |
|     1368 |  7897 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|        - |  7898 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|        - |  7899 | `		 * every hooked-property access consults the guard, often twice. */` |
|     1180 |  7900 | `		return FALSE;` |
|        - |  7901 | `	}` |
|      191 |  7902 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|      191 |  7903 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      235 |  7904 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|      191 |  7905 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|      147 |  7906 | `			return TRUE;` |
|        - |  7907 | `		}` |
|       23 |  7908 | `	}` |
|       45 |  7909 | `	return FALSE;` |
|      686 |  7910 | `}` |
|      580 |  7911 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|        4 |  7912 | `{` |
|        - |  7913 | `	VmMagicGuard sG;` |
|      584 |  7914 | `	sG.pThis = pThis;` |
|      584 |  7915 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      584 |  7916 | `	sG.cKind = cKind;` |
|      584 |  7917 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      584 |  7918 | `}` |
|      580 |  7919 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|        4 |  7920 | `{` |
|      584 |  7921 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      584 |  7922 | `}` |
|        - |  7923 | `/*` |
|        - |  7924 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|        - |  7925 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|        - |  7926 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|        - |  7927 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|        - |  7928 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|        - |  7929 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|        - |  7930 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|        - |  7931 | ` * One-token lookahead only.` |
|        - |  7932 | ` */` |
|    13445 |  7933 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|        5 |  7934 | `{` |
|    13450 |  7935 | `	switch( pNext->iOp ){` |
|      603 |  7936 | `		case PH7_OP_STORE:` |
|     1211 |  7937 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|       30 |  7938 | `		case PH7_OP_STORE_REF:` |
|       62 |  7939 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|     1280 |  7940 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|        - |  7941 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|        - |  7942 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|        - |  7943 | `		case PH7_OP_CAT_STORE:` |
|        - |  7944 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|        - |  7945 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|     2565 |  7946 | `			return 1;` |
|     4809 |  7947 | `		default:` |
|     9624 |  7948 | `			return 0;` |
|        - |  7949 | `	}` |
|     6728 |  7950 | `}` |
|        - |  7951 | `/*` |
|        - |  7952 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|        - |  7953 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|        - |  7954 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|        - |  7955 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|        - |  7956 | ` * treat them as one.` |
|        - |  7957 | ` */` |
|     7027 |  7958 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|        5 |  7959 | `{` |
|    10511 |  7960 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|    10524 |  7961 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|        5 |  7962 | `}` |
|        - |  7963 | `/*` |
|        - |  7964 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|        - |  7965 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|        - |  7966 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|        - |  7967 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|        - |  7968 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|        - |  7969 | `` * offsetSet where `$o['n']++` only notices.`` |
|        - |  7970 | ` */` |
|     7277 |  7971 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|        5 |  7972 | `{` |
|     7282 |  7973 | `	switch( pNext->iOp ){` |
|       65 |  7974 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|        - |  7975 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|        - |  7976 | `		case PH7_OP_CAT_STORE:` |
|        - |  7977 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|        - |  7978 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|      132 |  7979 | `			return 1;` |
|     3573 |  7980 | `		default:` |
|     7152 |  7981 | `			return 0;` |
|        - |  7982 | `	}` |
|     3644 |  7983 | `}` |
|        - |  7984 | `/*` |
|        - |  7985 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|        - |  7986 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|        - |  7987 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|        - |  7988 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|        - |  7989 | ` */` |
|      574 |  7990 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|        4 |  7991 | `{` |
|      578 |  7992 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|        4 |  7993 | `}` |
|        - |  7994 | `/*` |
|        - |  7995 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|        - |  7996 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|        - |  7997 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|        - |  7998 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|        - |  7999 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|        - |  8000 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|        - |  8001 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|        - |  8002 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|        - |  8003 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|        - |  8004 | ` * abort path; SXRET_OK otherwise.` |
|        - |  8005 | ` */` |
|       76 |  8006 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|        3 |  8007 | `{` |
|        - |  8008 | `	char zHName[384];` |
|        - |  8009 | `	sxu32 nHName;` |
|        - |  8010 | `	ph7_class_method *pSetHook;` |
|       79 |  8011 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|        - |  8012 | `		/* get-only hooked property: php's read-only Error */` |
|        - |  8013 | `		SyBlob sErrMsg;` |
|        5 |  8014 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 |  8015 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|        4 |  8016 | `			&pHThis->pClass->sDisp,&pHAttr->sName);` |
|        5 |  8017 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        5 |  8018 | `		return SXRET_OK;` |
|        - |  8019 | `	}` |
|       75 |  8020 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - |  8021 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|      ! 0 |  8022 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|      ! 0 |  8023 | `		if( rcVis != SXRET_OK ){` |
|      ! 0 |  8024 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|      ! 0 |  8025 | `			return SXRET_OK;` |
|        - |  8026 | `		}` |
|      ! 0 |  8027 | `	}` |
|       75 |  8028 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|       75 |  8029 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|       75 |  8030 | `	if( pSetHook ){` |
|        - |  8031 | `		ph7_value sHookRet;` |
|        - |  8032 | `		ph7_value *apHArg[1];` |
|       75 |  8033 | `		apHArg[0] = pValue;` |
|       75 |  8034 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|       75 |  8035 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|       75 |  8036 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|       75 |  8037 | `		VmMagicGuardPop(pVm);` |
|       72 |  8038 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|       41 |  8039 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|        6 |  8040 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|        6 |  8041 | `			if( rcH == SXRET_OK ){` |
|        6 |  8042 | `				ph7_value *pBack = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nBackIdx);` |
|        6 |  8043 | `				if( pBack ){` |
|        6 |  8044 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|        4 |  8045 | `				}` |
|        2 |  8046 | `			}else if( rcH == PH7_ABORT ){` |
|      ! 0 |  8047 | `				PH7_MemObjRelease(&sHookRet);` |
|      ! 0 |  8048 | `				return PH7_ABORT;` |
|        - |  8049 | `			}` |
|        - |  8050 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|        - |  8051 | `			 * the store is skipped, execution lands at the fetch point like any` |
|        - |  8052 | `			 * parked throw. */` |
|        2 |  8053 | `		}` |
|       75 |  8054 | `		PH7_MemObjRelease(&sHookRet);` |
|       36 |  8055 | `	}` |
|       75 |  8056 | `	return SXRET_OK;` |
|       41 |  8057 | `}` |
|        - |  8058 | `/*` |
|        - |  8059 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|        - |  8060 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|        - |  8061 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|        - |  8062 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|        - |  8063 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|        - |  8064 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|        - |  8065 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|        - |  8066 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|        - |  8067 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|        - |  8068 | ` */` |
|        - |  8069 | `/*` |
|        - |  8070 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|        - |  8071 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|        - |  8072 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|        - |  8073 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|        - |  8074 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|        - |  8075 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|        - |  8076 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|        - |  8077 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|        - |  8078 | ` * caller reads the raw slot then.` |
|        - |  8079 | ` */` |
|     1184 |  8080 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|        5 |  8081 | `{` |
|     1189 |  8082 | `	ph7_vm *pVm = pThis->pVm;` |
|        - |  8083 | `	char zHName[384];` |
|        - |  8084 | `	sxu32 nHName;` |
|        - |  8085 | `	ph7_class_method *pGetHook;` |
|        - |  8086 | `	sxi32 rc;` |
|     1184 |  8087 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|      693 |  8088 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|      207 |  8089 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|        - |  8090 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|        - |  8091 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|        - |  8092 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|        - |  8093 | `		 * raw values whose output the routed throw then discards. */` |
|      987 |  8094 | `		return SXERR_NOTFOUND;` |
|        - |  8095 | `	}` |
|      206 |  8096 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|      206 |  8097 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|      206 |  8098 | `	if( pGetHook == 0 ){` |
|      ! 0 |  8099 | `		return SXERR_NOTFOUND;` |
|        - |  8100 | `	}` |
|      206 |  8101 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|      206 |  8102 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|      206 |  8103 | `	VmMagicGuardPop(pVm);` |
|      206 |  8104 | `	return rc;` |
|      597 |  8105 | `}` |
|        - |  8106 | `/*` |
|        - |  8107 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|        - |  8108 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|        - |  8109 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|        - |  8110 | ` */` |
|      158 |  8111 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|        2 |  8112 | `{` |
|      160 |  8113 | `	ph7_value *pScr = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|      160 |  8114 | `	if( pScr ){` |
|      160 |  8115 | `		PH7_MemObjRelease(pScr);` |
|       79 |  8116 | `	}` |
|      160 |  8117 | `	VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|      160 |  8118 | `}` |
|        - |  8119 | `/*` |
|        - |  8120 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|        - |  8121 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|        - |  8122 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|        - |  8123 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|        - |  8124 | ` * instance reference.` |
|        - |  8125 | ` */` |
|       24 |  8126 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|        2 |  8127 | `{` |
|       26 |  8128 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       26 |  8129 | `	if( pEnt == 0 ){` |
|        5 |  8130 | `		return;` |
|        - |  8131 | `	}` |
|       21 |  8132 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|       13 |  8133 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|        6 |  8134 | `	}` |
|       21 |  8135 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|        - |  8136 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|        - |  8137 | `		 * property's backing store — this entry owns it. */` |
|        5 |  8138 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|        2 |  8139 | `	}` |
|       21 |  8140 | `	SyBlobRelease(&pEnt->sName);` |
|       21 |  8141 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|       21 |  8142 | `	(void)SySetPop(&pVm->aHookRmw);` |
|       14 |  8143 | `}` |
|       92 |  8144 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|        2 |  8145 | `{` |
|        - |  8146 | `	VmHookRmw sEnt;` |
|        - |  8147 | `	VmHookRmw *pEnt;` |
|        - |  8148 | `	ph7_value *pScr;` |
|        - |  8149 | `	ph7_value sVal;` |
|        - |  8150 | `	ph7_value sKey;` |
|       94 |  8151 | `	sxi32 rc = SXRET_OK;` |
|       94 |  8152 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       94 |  8153 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|      ! 0 |  8154 | `		return SXERR_NOTFOUND;` |
|        - |  8155 | `	}` |
|       94 |  8156 | `	sEnt = *pEnt;` |
|       94 |  8157 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        - |  8158 | `	/* Copy the computed value out of the scratch slot, then free the slot:` |
|        - |  8159 | `	 * once it is back on the pool's free list the next reserve may hand it to` |
|        - |  8160 | `	 * someone else, so nothing may read the scratch index past this point. The` |
|        - |  8161 | `	 * DIM kind's KEY slot goes the same way, for the same reason. (The pool` |
|        - |  8162 | `	 * itself no longer MOVES -- P1 -- but a freed index is still a freed index.) */` |
|       94 |  8163 | `	PH7_MemObjInit(pVm,&sVal);` |
|       94 |  8164 | `	pScr = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|       94 |  8165 | `	if( pScr ){` |
|       94 |  8166 | `		PH7_MemObjStore(pScr,&sVal);` |
|       46 |  8167 | `	}` |
|       94 |  8168 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|       94 |  8169 | `	sVal.nIdx = SXU32_HIGH;` |
|       94 |  8170 | `	PH7_MemObjInit(pVm,&sKey);` |
|       94 |  8171 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|       52 |  8172 | `		ph7_value *pKeySlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|       52 |  8173 | `		if( pKeySlot ){` |
|       52 |  8174 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|       25 |  8175 | `		}` |
|       52 |  8176 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|       52 |  8177 | `		sKey.nIdx = SXU32_HIGH;` |
|       25 |  8178 | `	}` |
|       94 |  8179 | `	if( pVm->nBoundaryRc == 0 ){` |
|       94 |  8180 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|        - |  8181 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|        - |  8182 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|        - |  8183 | `			SyString sPropName;` |
|       29 |  8184 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|       29 |  8185 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|       80 |  8186 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|        - |  8187 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|        - |  8188 | `			 * back through offsetSet($key, $value). */` |
|       52 |  8189 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|        - |  8190 | `				"offsetSet",sizeof("offsetSet")-1);` |
|       52 |  8191 | `			if( pSet ){` |
|        - |  8192 | `				ph7_value *apArg[2];` |
|       43 |  8193 | `				apArg[0] = &sKey;` |
|       43 |  8194 | `				apArg[1] = &sVal;` |
|       43 |  8195 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|       22 |  8196 | `			}else{` |
|        - |  8197 | `				/* A container that answers a READ and no ArrayAccess: its own` |
|        - |  8198 | `				 * dimension handler gets the computed value first -- php's` |
|        - |  8199 | `` 				 * SimpleXMLElement stores it, which is what makes `$x['a'] .= 'x'` `` |
|        - |  8200 | `				 * work there. A handler that stores nothing (DOMNodeList, PDORow)` |
|        - |  8201 | `				 * leaves the write, and php's read-then-write pair then ends in the` |
|        - |  8202 | ``				 * plain store's Error, so `$list[9] .= 'x'` says what`` |
|        - |  8203 | ``				 * `$list[9] = 'x'` says. Parked: this runs at an arithmetic op's`` |
|        - |  8204 | `				 * tail, not at a throw boundary. */` |
|        - |  8205 | `				PH7_NativeDimCtx sDim;` |
|       10 |  8206 | `				if( PH7_ClassNativeDimStore(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|        - |  8207 | `					&sKey,&sVal,&sDim) ){` |
|        6 |  8208 | `					if( sDim.zThrowClass ){` |
|        4 |  8209 | `						VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),sDim.zThrowClass,` |
|        2 |  8210 | `							sDim.zThrowMsg,(sxu32)SyStrlen(sDim.zThrowMsg)));` |
|        1 |  8211 | `					}` |
|        4 |  8212 | `				}else{` |
|        - |  8213 | `					char zMsg[256];` |
|        7 |  8214 | `					sxu32 nMsg = PH7_ClassNativeDimRefusal(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|        2 |  8215 | `						zMsg,sizeof(zMsg));` |
|        5 |  8216 | `					VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg));` |
|        - |  8217 | `				}` |
|        - |  8218 | `			}` |
|       27 |  8219 | `		}else{` |
|       15 |  8220 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|        - |  8221 | `		}` |
|       46 |  8222 | `	}` |
|       94 |  8223 | `	SyBlobRelease(&sEnt.sName);` |
|       94 |  8224 | `	PH7_MemObjRelease(&sKey);` |
|       94 |  8225 | `	PH7_MemObjRelease(&sVal);` |
|       94 |  8226 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|       94 |  8227 | `	return rc;` |
|       48 |  8228 | `}` |
|        - |  8229 | `/*` |
|        - |  8230 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|        - |  8231 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|        - |  8232 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|        - |  8233 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|        - |  8234 | ` */` |
|      814 |  8235 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|        4 |  8236 | `{` |
|        - |  8237 | `	ph7_class_method *pSetMeth;` |
|      818 |  8238 | `	if( PH7_ClassNativePropOwns(pSetThis,pName) ){` |
|        - |  8239 | `		/* php's write_property handler for a name the class's own table carries:` |
|        - |  8240 | ``		 * it answers BEFORE the standard path, so a subclass's `__set` never sees`` |
|        - |  8241 | `		 * a DOM property and the handler's refusal is the one a program catches.` |
|        - |  8242 | `		 * Every overloaded write funnels through here -- the plain store, the` |
|        - |  8243 | ``		 * compound assign's write-back, the `??=` and Reflection -- so this is the`` |
|        - |  8244 | `		 * one door the handler needs. The refusal is PARKED: this runs at an` |
|        - |  8245 | `		 * opcode's tail rather than at a throw boundary. */` |
|        - |  8246 | `		PH7_NativePropCtx sNat;` |
|      762 |  8247 | `		if( PH7_ClassNativePropAsk(pSetThis,&sNat,PH7_NATIVE_PROP_STORE,pName,pValue)` |
|      765 |  8248 | `		 && sNat.zThrowClass ){` |
|      246 |  8249 | `			VmBoundaryPark(&(*pVm),VmThrowFixedErrorCode(&(*pVm),sNat.zThrowClass,` |
|       81 |  8250 | `				sNat.iThrowCode,sNat.zThrowMsg));` |
|       81 |  8251 | `		}` |
|      765 |  8252 | `		return;` |
|        - |  8253 | `	}` |
|       54 |  8254 | `	pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       54 |  8255 | `	if( pSetMeth ){` |
|        - |  8256 | `		ph7_value sNameVal;` |
|        - |  8257 | `		ph7_value *apSetArg[2];` |
|       54 |  8258 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       54 |  8259 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       54 |  8260 | `		apSetArg[0] = &sNameVal;` |
|       54 |  8261 | `		apSetArg[1] = pValue;` |
|       54 |  8262 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       54 |  8263 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       54 |  8264 | `		VmMagicGuardPop(pVm);` |
|       54 |  8265 | `		PH7_MemObjRelease(&sNameVal);` |
|       26 |  8266 | `	}` |
|      411 |  8267 | `}` |
|        - |  8268 | `/*` |
|        - |  8269 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|        - |  8270 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|        - |  8271 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|        - |  8272 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|        - |  8273 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|        - |  8274 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|        - |  8275 | ` * path (the SyHash-layout incident class).` |
|        - |  8276 | ` */` |
|        - |  8277 | `/*` |
|        - |  8278 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|        - |  8279 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|        - |  8280 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|        - |  8281 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|        - |  8282 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|        - |  8283 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|        - |  8284 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|        - |  8285 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|        - |  8286 | ` * never linked (INIT error path).` |
|        - |  8287 | ` */` |
|    43757 |  8288 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|        5 |  8289 | `{` |
|    43762 |  8290 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|    43762 |  8291 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|        - |  8292 | `	sxu32 i;` |
|        - |  8293 | `	/* Drop the owning activation's claim on it first: the frame list is what` |
|        - |  8294 | `	 * guarantees a step cannot outlive the frame that made it, so it has to be` |
|        - |  8295 | `	 * left in step with aStep by the same door. A step that never made it onto` |
|        - |  8296 | `	 * aStep never made it onto the frame list either (INIT links both together,` |
|        - |  8297 | `	 * after the SySetPut), so the walk below simply finds nothing. */` |
|    43762 |  8298 | `	if( pStep->pFrame ){` |
|    43762 |  8299 | `		ph7_foreach_step **ppLink = &pStep->pFrame->pForeachSteps;` |
|    43788 |  8300 | `		while( *ppLink ){` |
|    43480 |  8301 | `			if( *ppLink == pStep ){` |
|    43454 |  8302 | `				*ppLink = pStep->pNextFrameStep;` |
|    43454 |  8303 | `				break;` |
|        - |  8304 | `			}` |
|       29 |  8305 | `			ppLink = &(*ppLink)->pNextFrameStep;` |
|        3 |  8306 | `		}` |
|    43762 |  8307 | `		pStep->pNextFrameStep = 0;` |
|    21844 |  8308 | `	}` |
|    46616 |  8309 | `	for( i = 0 ; i < n ; ++i ){` |
|    46616 |  8310 | `		if( apStep[i] == pStep ){` |
|    43774 |  8311 | `			for( ; i + 1 < n ; ++i ){` |
|       13 |  8312 | `				apStep[i] = apStep[i + 1];` |
|        7 |  8313 | `			}` |
|    43762 |  8314 | `			(void)SySetPop(&pInfo->aStep);` |
|    43762 |  8315 | `			return;` |
|        - |  8316 | `		}` |
|     1432 |  8317 | `	}` |
|    21849 |  8318 | `}` |
|        - |  8319 | `/*` |
|        - |  8320 | ` * End every foreach walk this activation still owns, because the activation is` |
|        - |  8321 | ` * about to die.` |
|        - |  8322 | ` *` |
|        - |  8323 | `` * A loop left through `break`, `return`, `goto` or an exception never reaches the`` |
|        - |  8324 | ` * "no more entries" arm that frees its step. OP_FOREACH_INIT reclaims such a` |
|        - |  8325 | ` * leftover, but only one whose owning frame is the frame running INIT -- so a step` |
|        - |  8326 | ` * belonging to an activation that had already returned stayed on the per-STATEMENT` |
|        - |  8327 | ` * aStep for the life of the VM, holding ~140 bytes and a retain of the subject, and` |
|        - |  8328 | ` * INIT's reclaim scan walked past all of them on every single iteration of every` |
|        - |  8329 | ` * enclosing loop. That is quadratic in the number of broken loops a program runs:` |
|        - |  8330 | ` * phpcs over one 318-line file reached 3600 dead steps and spent 60% of its time in` |
|        - |  8331 | ` * that scan.` |
|        - |  8332 | ` *` |
|        - |  8333 | ` * The frame that made a step is the one that can always end it. Called from both` |
|        - |  8334 | ` * VmFrame free sites (VmLeaveFrame and VmFreeDetachedFrame), after the frame has` |
|        - |  8335 | ` * left the active chain and before its locals are torn down -- the same point, and` |
|        - |  8336 | ` * the same order, the loop's own last iteration would have released it at.` |
|        - |  8337 | ` */` |
|  3760666 |  8338 | `PH7_PRIVATE void VmReleaseFrameForeachSteps(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 |  8339 | `{` |
|  3760671 |  8340 | `	if( pFrame == 0 ){` |
|      ! 0 |  8341 | `		return;` |
|        - |  8342 | `	}` |
|  3760979 |  8343 | `	while( pFrame->pForeachSteps ){` |
|      313 |  8344 | `		ph7_foreach_step *pStep = pFrame->pForeachSteps;` |
|        - |  8345 | `		/* Detach BEFORE releasing rather than letting the release do it. The release` |
|        - |  8346 | `		 * runs teardown that can re-enter the VM (an instance losing its last retain` |
|        - |  8347 | `		 * runs __destruct), and a head that is still linked when that happens is a` |
|        - |  8348 | `		 * step whose frame is dying being handed back out. It also makes the loop` |
|        - |  8349 | `		 * unconditionally terminate: nothing here depends on the release finding this` |
|        - |  8350 | `		 * step to unlink, which VmForeachStepUnlink then simply doesn't. */` |
|      313 |  8351 | `		pFrame->pForeachSteps = pStep->pNextFrameStep;` |
|      313 |  8352 | `		pStep->pNextFrameStep = 0;` |
|      313 |  8353 | `		if( pStep->pInfo == 0 ){` |
|        - |  8354 | `			/* Never linked to a statement, so nothing else can free it. */` |
|      ! 0 |  8355 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  8356 | `			continue;` |
|        - |  8357 | `		}` |
|      313 |  8358 | `		VmForeachStepRelease(&(*pVm),pStep->pInfo,pStep);` |
|        5 |  8359 | `	}` |
|  1880406 |  8360 | `}` |
|      760 |  8361 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|        5 |  8362 | `{` |
|      765 |  8363 | `	if( pStep->pOwner ){` |
|      263 |  8364 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|      130 |  8365 | `	}` |
|      765 |  8366 | `	VmForeachStepUnlink(pInfo,pStep);` |
|      765 |  8367 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      765 |  8368 | `	PH7_ClassInstanceUnref(pThis);` |
|      765 |  8369 | `}` |
|        - |  8370 | `/*` |
|        - |  8371 | ` * Release ONE foreach step of any kind — the single door OP_FOREACH_INIT uses to` |
|        - |  8372 | ` * reclaim a step its loop never exhausted.` |
|        - |  8373 | ` *` |
|        - |  8374 | `` * A `foreach` that leaves through `break`, `return`, `goto` or an exception never`` |
|        - |  8375 | ` * reaches the "no more entries" arm, so its step stayed on pInfo->aStep forever` |
|        - |  8376 | ` * (~140 bytes and one retain of the subject per execution: 200k broken loops leaked` |
|        - |  8377 | ` * 28 MB). Worse for an OBJECT loop, whose cursor is REGISTERED on the instance —` |
|        - |  8378 | ` * every abandoned walk left an entry that each later property add/remove had to` |
|        - |  8379 | ` * walk past. A step for THIS pInfo whose owning frame is the running one cannot be` |
|        - |  8380 | ` * mid-loop when INIT runs again (the frame executes one instruction at a time), so` |
|        - |  8381 | ` * INIT reclaims it before pushing its own.` |
|        - |  8382 | ` */` |
|      324 |  8383 | `PH7_PRIVATE void VmForeachStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|        5 |  8384 | `{` |
|        - |  8385 | `	ph7_class_instance *pThis;` |
|      329 |  8386 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      315 |  8387 | `		VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|      315 |  8388 | `		return;` |
|        - |  8389 | `	}` |
|        - |  8390 | `	/* Object-shaped step (plain attribute walk or the Iterator protocol): both` |
|        - |  8391 | `	 * retain xIter.pThis, and only the plain one holds a registered cursor. */` |
|       22 |  8392 | `	pThis = (pStep->iFlags & (PH7_4EACH_STEP_OBJECT\|PH7_4EACH_STEP_ITERATOR))` |
|       14 |  8393 | `		? pStep->xIter.pThis : 0;` |
|       15 |  8394 | `	if( pStep->iFlags & PH7_4EACH_STEP_OBJECT ){` |
|      ! 0 |  8395 | `		PH7_ClassInstanceIterClose(pStep->xIter.pThis,&pStep->sAttrIter);` |
|      ! 0 |  8396 | `	}` |
|       15 |  8397 | `	if( pStep->pOwner ){` |
|      ! 0 |  8398 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|      ! 0 |  8399 | `	}` |
|       15 |  8400 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       15 |  8401 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       15 |  8402 | `	if( pThis ){` |
|       15 |  8403 | `		PH7_ClassInstanceUnref(pThis);` |
|        7 |  8404 | `	}` |
|      167 |  8405 | `}` |
|        - |  8406 | `/*` |
|        - |  8407 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|        - |  8408 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|        - |  8409 | ` * step stack, then drop the step's map reference. The single home for this` |
|        - |  8410 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|        - |  8411 | ` * load-bearing: a step freed while still registered is walked by the next` |
|        - |  8412 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|        - |  8413 | ` * class), and the unregister must precede the unref in case the step held the` |
|        - |  8414 | ` * map's last reference.` |
|        - |  8415 | ` */` |
|    42877 |  8416 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|        5 |  8417 | `{` |
|    42882 |  8418 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|    42882 |  8419 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|    42882 |  8420 | `	if( bPop ){` |
|        - |  8421 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|        - |  8422 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|    42882 |  8423 | `		VmForeachStepUnlink(pInfo,pStep);` |
|    21404 |  8424 | `	}` |
|    42882 |  8425 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    42882 |  8426 | `	PH7_HashmapUnref(pMap);` |
|    42882 |  8427 | `}` |
|        - |  8428 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|        - |  8429 | `/*` |
|        - |  8430 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8431 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8432 | ` * See block-comment on that function for additional information.` |
|        - |  8433 | ` */` |
|  1519689 |  8434 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 |  8435 | `{` |
|        - |  8436 | `	ph7_value *pStack;` |
|        - |  8437 | `	sxu32 nCap;` |
|        - |  8438 | `	sxi32 rc;` |
|        - |  8439 | `	/* Allocate a new operand stack */` |
|  1519694 |  8440 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|  1519694 |  8441 | `	if( pStack == 0 ){` |
|      ! 0 |  8442 | `		return SXERR_MEM;` |
|        - |  8443 | `	}` |
|  1519694 |  8444 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|        - |  8445 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|        - |  8446 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|  1519694 |  8447 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|        - |  8448 | `	/* Free the operand stack */` |
|  1519694 |  8449 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8450 | `	/* Execution result */` |
|  1519694 |  8451 | `	return rc;` |
|   759802 |  8452 | `}` |
|        - |  8453 | `/*` |
|        - |  8454 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|        - |  8455 | ` * statement must honour?` |
|        - |  8456 | ` *` |
|        - |  8457 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|        - |  8458 | ` * property default) is compiled into its own bytecode container but shares the` |
|        - |  8459 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|        - |  8460 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|        - |  8461 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|        - |  8462 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|        - |  8463 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|        - |  8464 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|        - |  8465 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|        - |  8466 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|        - |  8467 | ` * drift apart.` |
|        - |  8468 | ` *` |
|        - |  8469 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|        - |  8470 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|        - |  8471 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|        - |  8472 | ` * this opcode did not create as its own throw.` |
|        - |  8473 | ` *` |
|        - |  8474 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|        - |  8475 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|        - |  8476 | ` */` |
|        - |  8477 | `/*` |
|        - |  8478 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|        - |  8479 | ` *` |
|        - |  8480 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|        - |  8481 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|        - |  8482 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|        - |  8483 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|        - |  8484 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|        - |  8485 | ` * captured value writes over the superglobal's own slot: calling` |
|        - |  8486 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|        - |  8487 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|        - |  8488 | ` * invisible to every reader in the program. extract() already screens for the` |
|        - |  8489 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|        - |  8490 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|        - |  8491 | ` *` |
|        - |  8492 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|        - |  8493 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|        - |  8494 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|        - |  8495 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|        - |  8496 | ` */` |
|     7580 |  8497 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|        5 |  8498 | `{` |
|        - |  8499 | `	static const char *const azAuto[] = {` |
|        - |  8500 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|        - |  8501 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|        - |  8502 | `	};` |
|        - |  8503 | `	sxu32 n;` |
|    82767 |  8504 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|    75249 |  8505 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|    75249 |  8506 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|       65 |  8507 | `			return 1;` |
|        - |  8508 | `		}` |
|    37346 |  8509 | `	}` |
|     7523 |  8510 | `	return 0;` |
|     3770 |  8511 | `}` |
|    30941 |  8512 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|        5 |  8513 | `{` |
|    29114 |  8514 | `	return rc == PH7_EXCEPTION` |
|    29120 |  8515 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|    44596 |  8516 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|        5 |  8517 | `}` |
|        - |  8518 | `/*` |
|        - |  8519 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|        - |  8520 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|        - |  8521 | ` * the argument resolve against that class (like php) rather than the reflection` |
|        - |  8522 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|        - |  8523 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|        - |  8524 | ` */` |
|      182 |  8525 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|        3 |  8526 | `{` |
|      185 |  8527 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      185 |  8528 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - |  8529 | `	sxi32 rc;` |
|      185 |  8530 | `	if( pDeclCls ){` |
|      169 |  8531 | `		pVm->pConstEvalClass = pDeclCls;` |
|      169 |  8532 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       83 |  8533 | `	}` |
|      185 |  8534 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|      185 |  8535 | `	pVm->pConstEvalClass = pSaveCtx;` |
|      185 |  8536 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|      185 |  8537 | `	return rc;` |
|        3 |  8538 | `}` |
|        - |  8539 | `/*` |
|        - |  8540 | ` * Flush every still-open output buffer at the end of execution. php implicitly` |
|        - |  8541 | ` * ends+flushes all ob_start() levels on shutdown (normal end, exit()/die(), or` |
|        - |  8542 | ` * fatal); PHL used to DISCARD them, so a script that never called ob_end_flush()` |
|        - |  8543 | ` * — e.g. PHPUnit, which buffers its result summary and then exit()s with a` |
|        - |  8544 | ` * non-zero status — lost that output entirely.` |
|        - |  8545 | ` *` |
|        - |  8546 | ` * PH7_VmObFlushAll() does it the way php does: one FINAL operation per buffer,` |
|        - |  8547 | ` * innermost first, so each handler's answer is what the buffer under it is` |
|        - |  8548 | ` * handed.` |
|        - |  8549 | ` */` |
|     6591 |  8550 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|        5 |  8551 | `{` |
|     6596 |  8552 | `	PH7_VmObFlushAll(&(*pVm));` |
|     6596 |  8553 | `}` |
|        - |  8554 | `/*` |
|        - |  8555 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8556 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8557 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8558 | ` * execution ends.` |
|        - |  8559 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8560 | ` * additional information.` |
|        - |  8561 | ` */` |
|     6591 |  8562 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 |  8563 | `{` |
|        - |  8564 | `	VmShutdownCB *pEntry;` |
|        - |  8565 | `	ph7_value *apArg[10];` |
|        - |  8566 | `	sxu32 n,nEntry;` |
|     6596 |  8567 | `	sxi32 rc = SXRET_OK;` |
|        - |  8568 | `	int i;` |
|        - |  8569 | `	/* Point to the stack of registered callbacks */` |
|     6596 |  8570 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    72506 |  8571 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    65915 |  8572 | `		apArg[i] = 0;` |
|    32905 |  8573 | `	}` |
|        - |  8574 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - |  8575 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - |  8576 | `	 * callbacks, mirroring PHP.` |
|        - |  8577 | `	 */` |
|     6596 |  8578 | `	pVm->bHaltRequested = 0;` |
|     6620 |  8579 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       31 |  8580 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       31 |  8581 | `		if( pEntry ){` |
|        - |  8582 | `			/* Prepare callback arguments if any */` |
|       31 |  8583 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8584 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8585 | `					break;` |
|        - |  8586 | `				}` |
|      ! 0 |  8587 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8588 | `			}` |
|        - |  8589 | `			/* Invoke the callback */` |
|       31 |  8590 | `			rc = PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8591 | `			/*` |
|        - |  8592 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8593 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8594 | `			 */` |
|       31 |  8595 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       31 |  8596 | `			if( pEntry ){` |
|       31 |  8597 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       31 |  8598 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8599 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8600 | `				}` |
|       13 |  8601 | `			}` |
|       31 |  8602 | `			if( pVm->bHaltRequested \|\| rc == SXERR_ABORT ){` |
|        - |  8603 | `				/* exit() inside the callback, or a throwable it never caught: php` |
|        - |  8604 | `				 * abandons the remaining callbacks either way (the bailout leaves` |
|        - |  8605 | `				 * php_call_shutdown_functions), and goes on to the destructors. */` |
|        2 |  8606 | `				break;` |
|        - |  8607 | `			}` |
|       12 |  8608 | `		}` |
|       17 |  8609 | `	}` |
|     6596 |  8610 | `	SySetReset(&pVm->aShutdown);` |
|     6596 |  8611 | `}` |
|        - |  8612 | `/*` |
|        - |  8613 | ` * One name of the global symbol table, snapshotted for the shutdown pass below.` |
|        - |  8614 | ` * Held as an offset into a private blob rather than a pointer: a destructor is` |
|        - |  8615 | ` * arbitrary PHP and may unset any global, which frees the key the table owns.` |
|        - |  8616 | ` */` |
|        - |  8617 | `typedef struct VmShutdownName VmShutdownName;` |
|        - |  8618 | `struct VmShutdownName` |
|        - |  8619 | `{` |
|        - |  8620 | `	sxu32 nOfft;  /* Offset of the name in the caller's snapshot blob */` |
|        - |  8621 | `	sxu32 nByte;  /* Its length */` |
|        - |  8622 | `};` |
|        - |  8623 | `/*` |
|        - |  8624 | ` * TRUE when this slot is held by exactly ONE name and nothing else -- the state php` |
|        - |  8625 | `` * spells `Z_TYPE_P(zv) == IS_OBJECT` with a refcount of 1 on a symbol-table entry.`` |
|        - |  8626 | ` *` |
|        - |  8627 | `` * php's symbol-table pass tests the ZVAL, and a name written with `&` is not an object`` |
|        - |  8628 | `` * zval at all: `$g = new T; $r = &$g;` makes both entries IS_REFERENCE, which the test`` |
|        - |  8629 | ` * rejects outright and leaves to the object-store pass. This engine has no separate` |
|        - |  8630 | ` * reference cell -- the two names simply share one slot -- so the equivalent question` |
|        - |  8631 | ` * is how many names the slot's reference record still lists, plus whether anything the` |
|        - |  8632 | `` * record cannot name pins it (a `use (&$x)` capture, a static, a reference-bound`` |
|        - |  8633 | ` * property), which php would also be carrying as a reference.` |
|        - |  8634 | ` *` |
|        - |  8635 | ` * The $GLOBALS entry for the name is not a holder for this purpose: it is how this` |
|        - |  8636 | ` * engine spells the symbol table, not a second reference to the value.` |
|        - |  8637 | ` */` |
|     3649 |  8638 | `static int VmSlotHeldByOneName(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  8639 | `{` |
|     3654 |  8640 | `	if( PH7_VmSlotSelfPinned(&(*pVm),nIdx) ){` |
|       48 |  8641 | `		return 0;` |
|        - |  8642 | `	}` |
|     3610 |  8643 | `	return PH7_VmSlotEntryCount(&(*pVm),nIdx) == 1;` |
|     1826 |  8644 | `}` |
|        - |  8645 | `/*` |
|        - |  8646 | ` * php's shutdown destructor phase, first half: the GLOBAL SYMBOL TABLE.` |
|        - |  8647 | ` *` |
|        - |  8648 | `` * `shutdown_destructors()` walks the symbol table in REVERSE and drops every entry`` |
|        - |  8649 | ` * holding an object nothing else refers to, repeating the walk while the table keeps` |
|        - |  8650 | `` * shrinking. That reverse walk is observable -- `$a = new T; $b = new T;` destructs`` |
|        - |  8651 | ` * $b before $a -- and it is the half that actually FREES its objects, which is why a` |
|        - |  8652 | ` * destructor here sees the rest of the program's globals still standing.` |
|        - |  8653 | ` *` |
|        - |  8654 | `` * `iRef == 1` plus VmSlotHeldByOneName is this engine's spelling of php's`` |
|        - |  8655 | `` * `Z_TYPE_P(zv) == IS_OBJECT && Z_REFCOUNT_P(zv) == 1`: exactly one memory object holds`` |
|        - |  8656 | ` * the instance and exactly one name holds that, so dropping the name ends it. Everything` |
|        - |  8657 | ` * else -- an object two names share, one an array or a property also holds, one a name` |
|        - |  8658 | `` * written with `&` reaches -- is left to the second half.`` |
|        - |  8659 | ` */` |
|     6591 |  8660 | `static void VmShutdownGlobalPass(ph7_vm *pVm)` |
|        5 |  8661 | `{` |
|        - |  8662 | `	VmFrame *pFrame;` |
|     6634 |  8663 | `	for( pFrame = pVm->pFrame ; pFrame && pFrame->pParent ; pFrame = pFrame->pParent ){}` |
|     6596 |  8664 | `	if( pFrame == 0 ){` |
|      ! 0 |  8665 | `		return;` |
|        - |  8666 | `	}` |
|     4330 |  8667 | `	for(;;){` |
|        - |  8668 | `		ph7_hashmap_node *pNode;` |
|        - |  8669 | `		VmShutdownName *aName;` |
|        - |  8670 | `		SyBlob sNames;` |
|        - |  8671 | `		SySet aEntry;` |
|        - |  8672 | `		sxu32 n;` |
|     7636 |  8673 | `		int bDropped = 0;` |
|     7636 |  8674 | `		if( pVm->pGlobal == 0 \|\| pVm->pGlobal->nEntry < 1 ){` |
|      ! 0 |  8675 | `			return;` |
|        - |  8676 | `		}` |
|        - |  8677 | `		/* Snapshot the names, last-declared first. The map's insertion list runs` |
|        - |  8678 | `		 * pFirst -> pPrev -> ... -> pLast, so walking it BACKWARDS is pLast and the` |
|        - |  8679 | `		 * pNext chain (the two link names read the other way round here). */` |
|     7636 |  8680 | `		SyBlobInit(&sNames,&pVm->sAllocator);` |
|     7636 |  8681 | `		SySetInit(&aEntry,&pVm->sAllocator,sizeof(VmShutdownName));` |
|   110848 |  8682 | `		for( pNode = pVm->pGlobal->pLast ; pNode ; pNode = pNode->pNext ){` |
|        - |  8683 | `			VmShutdownName sName;` |
|   103217 |  8684 | `			if( pNode->iType != HASHMAP_BLOB_NODE \|\| SyBlobLength(&pNode->xKey.sKey) < 1 ){` |
|      ! 0 |  8685 | `				continue;` |
|        - |  8686 | `			}` |
|   103217 |  8687 | `			sName.nOfft = SyBlobLength(&sNames);` |
|   103217 |  8688 | `			sName.nByte = SyBlobLength(&pNode->xKey.sKey);` |
|   103212 |  8689 | `			if( SyBlobAppend(&sNames,SyBlobData(&pNode->xKey.sKey),sName.nByte) != SXRET_OK` |
|   103217 |  8690 | `			 \|\| SySetPut(&aEntry,(const void *)&sName) != SXRET_OK ){` |
|        - |  8691 | `				/* Out of memory: go on with the names already gathered. */` |
|      ! 0 |  8692 | `				break;` |
|        - |  8693 | `			}` |
|    51476 |  8694 | `		}` |
|     7636 |  8695 | `		aName = (VmShutdownName *)SySetBasePtr(&aEntry);` |
|   110796 |  8696 | `		for( n = 0 ; n < SySetUsed(&aEntry) ; ++n ){` |
|   103169 |  8697 | `			const char *zName = (const char *)SyBlobData(&sNames) + aName[n].nOfft;` |
|        - |  8698 | `			ph7_class_instance *pThis;` |
|        - |  8699 | `			SyHashEntry *pHash;` |
|        - |  8700 | `			ph7_value *pObj;` |
|   103169 |  8701 | `			pHash = SyHashGet(&pFrame->hVar,(const void *)zName,aName[n].nByte);` |
|   103169 |  8702 | `			if( pHash == 0 ){` |
|    76275 |  8703 | `				continue;  /* An earlier destructor already dropped this one */` |
|        - |  8704 | `			}` |
|    26899 |  8705 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pHash->pUserData));` |
|    26899 |  8706 | `			if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    22430 |  8707 | `				continue;` |
|        - |  8708 | `			}` |
|     4474 |  8709 | `			pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     4469 |  8710 | `			if( pThis->iRef != 1` |
|     4064 |  8711 | `			 \|\| !VmSlotHeldByOneName(&(*pVm),(sxu32)SX_PTR_TO_INT(pHash->pUserData)) ){` |
|      889 |  8712 | `				continue;` |
|        - |  8713 | `			}` |
|     3590 |  8714 | `			VmUnsetVarByNameEx(&(*pVm),pFrame,zName,aName[n].nByte,FALSE);` |
|     3590 |  8715 | `			bDropped = 1;` |
|     3590 |  8716 | `			if( pVm->bHaltRequested \|\| pVm->bShutdownAborted ){` |
|        4 |  8717 | `				break;` |
|        - |  8718 | `			}` |
|     1792 |  8719 | `		}` |
|     7636 |  8720 | `		SySetRelease(&aEntry);` |
|     7636 |  8721 | `		SyBlobRelease(&sNames);` |
|     7636 |  8722 | `		if( !bDropped \|\| pVm->bHaltRequested \|\| pVm->bShutdownAborted ){` |
|     6596 |  8723 | `			return;` |
|        - |  8724 | `		}` |
|        5 |  8725 | `	}` |
|     3295 |  8726 | `}` |
|        - |  8727 | `/*` |
|        - |  8728 | ` * Order the collected instances by their object handle -- php's object store is` |
|        - |  8729 | ` * walked front to back, and a handle is handed out in creation order, so this is` |
|        - |  8730 | ` * "oldest object first". Shell sort: no allocation, no recursion, and the array is` |
|        - |  8731 | ` * the objects a finished program left alive.` |
|        - |  8732 | ` */` |
|      207 |  8733 | `static void VmSortByObjId(ph7_class_instance **apObj,sxu32 nUsed)` |
|        5 |  8734 | `{` |
|        - |  8735 | `	static const sxu32 aGap[] = { 701, 301, 132, 57, 23, 10, 4, 1 };` |
|        - |  8736 | `	sxu32 g;` |
|     1868 |  8737 | `	for( g = 0 ; g < SX_ARRAYSIZE(aGap) ; ++g ){` |
|     1661 |  8738 | `		sxu32 nGap = aGap[g], i;` |
|     9996 |  8739 | `		for( i = nGap ; i < nUsed ; ++i ){` |
|     8340 |  8740 | `			ph7_class_instance *pCur = apObj[i];` |
|     8340 |  8741 | `			sxu32 j = i;` |
|    14185 |  8742 | `			while( j >= nGap && apObj[j-nGap]->nObjId > pCur->nObjId ){` |
|     5850 |  8743 | `				apObj[j] = apObj[j-nGap];` |
|     5850 |  8744 | `				j -= nGap;` |
|        5 |  8745 | `			}` |
|     8340 |  8746 | `			apObj[j] = pCur;` |
|     4169 |  8747 | `		}` |
|      829 |  8748 | `	}` |
|      212 |  8749 | `}` |
|        - |  8750 | `/*` |
|        - |  8751 | ` * php's shutdown destructor phase, second half: the OBJECT STORE.` |
|        - |  8752 | ` *` |
|        - |  8753 | `` * `zend_objects_store_call_destructors()` reaches every object still alive after the`` |
|        - |  8754 | ` * symbol-table pass -- one a class static, a function static, an array or another` |
|        - |  8755 | ` * object holds, and one that is only part of a cycle -- and calls its destructor in` |
|        - |  8756 | ` * CREATION order without freeing it. The free comes later, from the teardown proper,` |
|        - |  8757 | ` * which is why the destructor is flagged as already run (CLASS_INSTANCE_DTOR_CALLED).` |
|        - |  8758 | ` *` |
|        - |  8759 | ` * Every object is reachable from the memory-object pool, so that pool is the store.` |
|        - |  8760 | ` * A destructor may create objects of its own (php destructs those too), so the sweep` |
|        - |  8761 | ` * repeats until a round finds nothing new.` |
|        - |  8762 | ` */` |
|     6591 |  8763 | `static void VmShutdownObjectPass(ph7_vm *pVm)` |
|        5 |  8764 | `{` |
|     6803 |  8765 | `	while( !pVm->bHaltRequested && !pVm->bShutdownAborted ){` |
|        - |  8766 | `		ph7_class_instance **apObj;` |
|        - |  8767 | `		SySet aObj;` |
|        - |  8768 | `		sxu32 n,nUsed;` |
|     6799 |  8769 | `		SySetInit(&aObj,&pVm->sAllocator,sizeof(ph7_class_instance *));` |
|  5976544 |  8770 | `		for( n = 0 ; n < pVm->aMemObj.nUsed ; ++n ){` |
|  5969750 |  8771 | `			ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,n);` |
|  5969750 |  8772 | `			if( pObj && (pObj->iFlags & MEMOBJ_OBJ) && pObj->x.pOther ){` |
|     3659 |  8773 | `				ph7_class_instance *pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     3659 |  8774 | `				if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){` |
|     1835 |  8775 | `					SySetPut(&aObj,(const void *)&pThis);` |
|      912 |  8776 | `				}` |
|     1821 |  8777 | `			}` |
|  2983829 |  8778 | `		}` |
|     6799 |  8779 | `		nUsed = SySetUsed(&aObj);` |
|     6799 |  8780 | `		if( nUsed < 1 ){` |
|     6592 |  8781 | `			SySetRelease(&aObj);` |
|     6592 |  8782 | `			return;` |
|        - |  8783 | `		}` |
|      212 |  8784 | `		apObj = (ph7_class_instance **)SySetBasePtr(&aObj);` |
|      212 |  8785 | `		VmSortByObjId(apObj,nUsed);` |
|        - |  8786 | `		/* Pin every one of them BEFORE the first body runs: a destructor is free to` |
|        - |  8787 | `		 * unset whatever holds another object on this list, and the release that` |
|        - |  8788 | `		 * follows would free a pointer still to be visited. php pins the same way,` |
|        - |  8789 | `		 * one at a time, because its store can tell a dead bucket from a live one. */` |
|     2042 |  8790 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|     1835 |  8791 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|      365 |  8792 | `				continue;  /* Several names for one object: sorted, so duplicates adjoin */` |
|        - |  8793 | `			}` |
|     1475 |  8794 | `			apObj[n]->iRef++;` |
|      737 |  8795 | `		}` |
|     2042 |  8796 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|     1835 |  8797 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|      365 |  8798 | `				continue;` |
|        - |  8799 | `			}` |
|     1475 |  8800 | `			if( !pVm->bHaltRequested && !pVm->bShutdownAborted ){` |
|        - |  8801 | `				/* A body that leaves an uncaught throwable raises bShutdownAborted` |
|        - |  8802 | `				 * itself, which is what stops this loop and the sweep around it:` |
|        - |  8803 | `				 * php abandons the whole phase on the first one, leaving every` |
|        - |  8804 | `				 * remaining object undestructed. */` |
|     1475 |  8805 | `				PH7_ClassInstanceCallDestructor(apObj[n]);` |
|      732 |  8806 | `			}` |
|      737 |  8807 | `		}` |
|     2042 |  8808 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|     1835 |  8809 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|      365 |  8810 | `				continue;` |
|        - |  8811 | `			}` |
|     1475 |  8812 | `			PH7_ClassInstanceUnref(apObj[n]);` |
|      737 |  8813 | `		}` |
|      212 |  8814 | `		SySetRelease(&aObj);` |
|        5 |  8815 | `	}` |
|     3295 |  8816 | `}` |
|        - |  8817 | `/*` |
|        - |  8818 | ` * Run every destructor a finished program still owes, between the shutdown callbacks` |
|        - |  8819 | `` * and the output-buffer flush -- php's `zend_call_destructors()`, in that same slot of`` |
|        - |  8820 | `` * `php_request_shutdown()`, which is why a destructor's own echo still lands inside an`` |
|        - |  8821 | ` * open output buffer.` |
|        - |  8822 | ` *` |
|        - |  8823 | ` * Before this existed, an object a program left in a global (or a static, or any` |
|        - |  8824 | ` * container) was torn down by PH7_VmReset with user destructors suppressed, so a` |
|        - |  8825 | ` * destructor that closes a file, flushes a buffer or commits a transaction simply` |
|        - |  8826 | ` * never fired. The two passes below are php's two, in php's order.` |
|        - |  8827 | ` */` |
|     6591 |  8828 | `static void VmCallShutdownDestructors(ph7_vm *pVm)` |
|        5 |  8829 | `{` |
|     6596 |  8830 | `	if( pVm->bInReset ){` |
|      ! 0 |  8831 | `		return;` |
|        - |  8832 | `	}` |
|        - |  8833 | `	/* A halt is consumed the same way the shutdown callbacks consume theirs: php runs` |
|        - |  8834 | `	 * the destructor phase after an exit(), and after a shutdown callback that threw. */` |
|     6596 |  8835 | `	pVm->bHaltRequested = 0;` |
|     6596 |  8836 | `	pVm->bShutdownAborted = 0;` |
|     6596 |  8837 | `	pVm->bInShutdownDtor = 1;` |
|     6596 |  8838 | `	VmShutdownGlobalPass(&(*pVm));` |
|     6596 |  8839 | `	VmShutdownObjectPass(&(*pVm));` |
|     6596 |  8840 | `	pVm->bInShutdownDtor = 0;` |
|     3295 |  8841 | `}` |
|        - |  8842 | `/*` |
|        - |  8843 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8844 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8845 | ` * See block-comment on that function for additional information.` |
|        - |  8846 | ` */` |
|     6575 |  8847 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 |  8848 | `{` |
|        - |  8849 | `	/* Make sure we are ready to execute this program */` |
|     6580 |  8850 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8851 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8852 | `	}` |
|        - |  8853 | `	/* Set the execution magic number  */` |
|     6580 |  8854 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8855 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|        - |  8856 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|        - |  8857 | `	{` |
|     6580 |  8858 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|        - |  8859 | `		/* Top-level code is a body like any other, and the global frame is the one` |
|        - |  8860 | `		 * frame that was pushed long before there was anything to number. Number it` |
|        - |  8861 | `		 * here, where the program about to run is finally known. */` |
|     6580 |  8862 | `		if( pVm->pFrame && pVm->pFrame->pCodeBase == 0 ){` |
|     6580 |  8863 | `			sxu16 nMainName = 0;` |
|     9862 |  8864 | `			VmNumberLocals((VmInstr *)SySetBasePtr(pVm->pByteContainer),` |
|     6575 |  8865 | `				SySetUsed(pVm->pByteContainer),&nMainName);` |
|     6580 |  8866 | `			if( nMainName > 0 ){` |
|     2293 |  8867 | `				pVm->pFrame->pCodeBase = (const VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     1139 |  8868 | `			}` |
|     3282 |  8869 | `		}` |
|     6580 |  8870 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|        - |  8871 | `	}` |
|        - |  8872 | `	/* Invoke any shutdown callbacks */` |
|     6588 |  8873 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8874 | `	/* Then every destructor the program still owes: php's zend_call_destructors(),` |
|        - |  8875 | `	 * which sits exactly here -- after the shutdown callbacks, before the buffers. */` |
|     6588 |  8876 | `	VmCallShutdownDestructors(&(*pVm));` |
|        - |  8877 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|        - |  8878 | `	 * shutdown callbacks, which may still write into them. */` |
|     6588 |  8879 | `	VmFlushOutputBuffers(&(*pVm));` |
|        - |  8880 | `	/* An open session is written back LAST, from php's own module shutdown: after` |
|        - |  8881 | `	 * the script's shutdown callbacks (which may still write to $_SESSION) and` |
|        - |  8882 | `	 * after the buffers are flushed (which is why its diagnostics land outside` |
|        - |  8883 | `	 * them). */` |
|     6588 |  8884 | `	PH7_VmSessionShutdown(&(*pVm));` |
|        - |  8885 | `	/*` |
|        - |  8886 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8887 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8888 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8889 | `	 */` |
|     6588 |  8890 | `	return SXRET_OK;` |
|     3287 |  8891 | `}` |
|        - |  8892 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8893 | `/*` |
|        - |  8894 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8895 | ` * the desired message.` |
|        - |  8896 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8897 | ` * in 'api.c' for additional information.` |
|        - |  8898 | ` */` |
|   143065 |  8899 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8900 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8901 | `	SyString *pString /* Message to output */` |
|        - |  8902 | `	)` |
|        5 |  8903 | `{` |
|   143070 |  8904 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|   143070 |  8905 | `	sxi32 rc = SXRET_OK;` |
|        - |  8906 | `	/* Call the output consumer */` |
|   143070 |  8907 | `	if( pString->nByte > 0 ){` |
|   143070 |  8908 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|   143070 |  8909 | `		VmTrackOutput(pVm, pString->nByte);` |
|    70639 |  8910 | `	}` |
|   143070 |  8911 | `	return rc;` |
|        5 |  8912 | `}` |
|        - |  8913 | `/*` |
|        - |  8914 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8915 | ` * callback to consume the formatted message.` |
|        - |  8916 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8917 | ` * in 'api.c' for additional information.` |
|        - |  8918 | ` */` |
|       30 |  8919 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8920 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8921 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8922 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8923 | `	)` |
|        2 |  8924 | `{` |
|       32 |  8925 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|       32 |  8926 | `	sxi32 rc = SXRET_OK;` |
|        - |  8927 | `	SyBlob sWorker;` |
|        - |  8928 | `	/* Format the message and call the output consumer */` |
|       32 |  8929 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       32 |  8930 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|       32 |  8931 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8932 | `		/* Consume the formatted message */` |
|       32 |  8933 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|       15 |  8934 | `	}` |
|       32 |  8935 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8936 | `	/* Release the working buffer */` |
|       32 |  8937 | `	SyBlobRelease(&sWorker);` |
|       32 |  8938 | `	return rc;` |
|        2 |  8939 | `}` |
|        - |  8940 | `/*` |
|        - |  8941 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8942 | ` * This function never fail and always return a pointer` |
|        - |  8943 | ` * to a null terminated string.` |
|        - |  8944 | ` */` |
|       10 |  8945 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8946 | `{` |
|       11 |  8947 | `	const char *zOp = "Unknown     ";` |
|       11 |  8948 | `	switch(nOp){` |
|        3 |  8949 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8950 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8951 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8952 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8953 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8954 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8955 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8956 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8957 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8958 | `	case PH7_OP_LOAD_FCC:` |
|      ! 0 |  8959 | `		                    zOp = "LOAD_FCC   "; break;` |
|      ! 0 |  8960 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8961 | `	case PH7_OP_SNAPSHOT:   zOp = "SNAPSHOT   "; break;` |
|      ! 0 |  8962 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8963 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8964 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8965 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8966 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8967 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8968 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8969 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8970 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8971 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|      ! 0 |  8972 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|      ! 0 |  8973 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8974 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8975 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8976 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8977 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8978 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8979 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8980 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8981 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8982 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8983 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8984 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8985 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8986 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8987 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8988 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8989 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8990 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8991 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8992 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8993 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8994 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8995 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8996 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8997 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8998 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8999 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9000 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9001 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9002 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9003 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9004 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|      ! 0 |  9005 | `	case PH7_OP_PICK:       zOp = "PICK       "; break;` |
|      ! 0 |  9006 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9007 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9008 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|      ! 0 |  9009 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9010 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9011 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9012 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 |  9013 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9014 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 |  9015 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9016 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9017 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9018 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9019 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9020 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9021 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9022 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9023 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9024 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9025 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9026 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9027 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9028 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9029 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9030 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9031 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9032 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9033 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9034 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9035 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9036 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9037 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9038 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9039 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9040 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9041 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|      ! 0 |  9042 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9043 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9044 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 |  9045 | `	case PH7_OP_FUNC_DECL:  zOp = "FUNC_DECL  "; break;` |
|      ! 0 |  9046 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|      ! 0 |  9047 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  9048 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  9049 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  9050 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  9051 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  9052 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  9053 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  9054 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  9055 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  9056 | `	default:` |
|      ! 0 |  9057 | `		break;` |
|        - |  9058 | `	}` |
|       11 |  9059 | `	return zOp;` |
|        1 |  9060 | `}` |
|        - |  9061 | `/*` |
|        - |  9062 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  9063 | ` * The xConsumer() callback which is an used defined function` |
|        - |  9064 | ` * is responsible of consuming the generated dump.` |
|        - |  9065 | ` */` |
|        2 |  9066 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  9067 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  9068 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  9069 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  9070 | `	)` |
|        1 |  9071 | `{` |
|        - |  9072 | `	sxi32 rc;` |
|        3 |  9073 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  9074 | `	return rc;` |
|        1 |  9075 | `}` |
|        - |  9076 | `/*` |
|        - |  9077 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  9078 | ` * outside a class body [i.e: global or function scope].` |
|        - |  9079 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  9080 | ` * in 'compile.c' for additional information.` |
|        - |  9081 | ` */` |
|        4 |  9082 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  9083 | `{` |
|        5 |  9084 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  9085 | `	/* Evaluate and expand constant value */` |
|        5 |  9086 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|        5 |  9087 | `}` |
|        - |  9088 | `/*` |
|        - |  9089 | ` * Section:` |
|        - |  9090 | ` *  Function handling functions.` |
|        - |  9091 | ` * Status:` |
|        - |  9092 | ` *    Stable.` |
|        - |  9093 | ` */` |
|        - |  9094 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  9095 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9096 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|        - |  9097 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9098 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9099 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9100 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9101 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9102 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9103 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9104 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9105 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9106 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9107 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9108 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9109 | `	    /* Constants management */` |
|        - |  9110 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9111 | `	{ "define",   vm_builtin_define               },` |
|        - |  9112 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9113 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9114 | `	   /* Class/Object functions */` |
|        - |  9115 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9116 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9117 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9118 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9119 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9120 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|        - |  9121 | `	{ "class_parents",   vm_builtin_class_parents     },` |
|        - |  9122 | `	{ "class_implements",vm_builtin_class_implements  },` |
|        - |  9123 | `	{ "class_uses",      vm_builtin_class_uses        },` |
|        - |  9124 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9125 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9126 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9127 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9128 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9129 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - |  9130 | `	{ "get_declared_traits",     vm_builtin_get_declared_traits    },` |
|        - |  9131 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - |  9132 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - |  9133 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - |  9134 | `	{ "get_mangled_object_vars", vm_builtin_get_mangled_object_vars },` |
|        - |  9135 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - |  9136 | `	{ "is_a", vm_builtin_is_a },` |
|        - |  9137 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|        - |  9138 | `	{ "clone",           vm_builtin_clone             },` |
|        - |  9139 | `	   /* SPL object identity */` |
|        - |  9140 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|        - |  9141 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|        - |  9142 | `	   /* SPL Autoloading */` |
|        - |  9143 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - |  9144 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - |  9145 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - |  9146 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - |  9147 | `	{ "spl_autoload_extensions", vm_builtin_spl_autoload_extensions },` |
|        - |  9148 | `	{ "spl_autoload_call",       vm_builtin_spl_autoload_call       },` |
|        - |  9149 | `	{ "spl_classes",             vm_builtin_spl_classes             },` |
|        - |  9150 | `	   /* Random numbers/strings generators */` |
|        - |  9151 | `	{ "rand",          vm_builtin_rand            },` |
|        - |  9152 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - |  9153 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - |  9154 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - |  9155 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - |  9156 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - |  9157 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - |  9158 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9159 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9160 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - |  9161 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9162 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9163 | `	   /* Language constructs functions.` |
|        - |  9164 | `	    *` |
|        - |  9165 | ``	    * `echo` is not among them: it is the one construct whose codegen emits an`` |
|        - |  9166 | `	    * OPCODE and no call at all (PH7_OP_CONSUME, PH7_CompileEcho), so the name was` |
|        - |  9167 | `	    * a registration nothing could ever dispatch -- and one more row php has no` |
|        - |  9168 | `	    * function for. The rest stay, hidden (PH7_VmGetHostFunction). */` |
|        - |  9169 | `	{ "print", vm_builtin_print                   },` |
|        - |  9170 | `	{ "exit",  vm_builtin_exit                    },` |
|        - |  9171 | `	{ "die",   vm_builtin_exit                    },` |
|        - |  9172 | `	{ "eval",  vm_builtin_eval                    },` |
|        - |  9173 | `	  /* Variable handling functions */` |
|        - |  9174 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - |  9175 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - |  9176 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|        - |  9177 | `	{ "settype",   vm_builtin_settype              },` |
|        - |  9178 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - |  9179 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|        - |  9180 | `	{ "isset",     vm_builtin_isset                },` |
|        - |  9181 | `	{ "unset",     vm_builtin_unset                },` |
|        - |  9182 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - |  9183 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - |  9184 | `	{ "var_export",vm_builtin_var_export           },` |
|        - |  9185 | `	  /* Ouput control functions */` |
|        - |  9186 | `	{ "flush",        vm_builtin_flush             },` |
|        - |  9187 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - |  9188 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - |  9189 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - |  9190 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - |  9191 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - |  9192 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - |  9193 | `	{ "ob_get_flush",    vm_builtin_ob_get_flush   },` |
|        - |  9194 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - |  9195 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - |  9196 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - |  9197 | `	{ "ob_get_status",     vm_builtin_ob_get_status },` |
|        - |  9198 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - |  9199 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - |  9200 | `	  /* Assertion functions */` |
|        - |  9201 | `	{ "assert",          vm_builtin_assert         },` |
|        - |  9202 | `	  /* Error reporting functions */` |
|        - |  9203 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - |  9204 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - |  9205 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - |  9206 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - |  9207 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - |  9208 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - |  9209 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - |  9210 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - |  9211 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|        - |  9212 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|        - |  9213 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - |  9214 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|        - |  9215 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|        - |  9216 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - |  9217 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - |  9218 | `	  /* Release info */` |
|        - |  9219 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - |  9220 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - |  9221 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|        - |  9222 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|        - |  9223 | `	{"get_extension_funcs",   vm_builtin_get_extension_funcs   },` |
|        - |  9224 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - |  9225 | `	{"php_ini_loaded_file",   vm_builtin_php_ini_loaded_file },` |
|        - |  9226 | `	{"php_ini_scanned_files", vm_builtin_php_ini_scanned_files },` |
|        - |  9227 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - |  9228 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - |  9229 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - |  9230 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - |  9231 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - |  9232 | `	  /* hashmap */` |
|        - |  9233 | `	{"compact",          vm_builtin_compact       },` |
|        - |  9234 | `	{"extract",          vm_builtin_extract       },` |
|        - |  9235 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - |  9236 | `	  /* URL related function */` |
|        - |  9237 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - |  9238 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - |  9239 | `	   /* Command line processing */` |
|        - |  9240 | `	{"getopt",         vm_builtin_getopt     },` |
|        - |  9241 | `	   /* JSON encoding/decoding */` |
|        - |  9242 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - |  9243 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - |  9244 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - |  9245 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - |  9246 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - |  9247 | `	{"serialize",      vm_builtin_serialize },` |
|        - |  9248 | `	{"unserialize",    vm_builtin_unserialize },` |
|        - |  9249 | `	   /* Files/URI inclusion facility */` |
|        - |  9250 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - |  9251 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|        - |  9252 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - |  9253 | ``	/* php's alias: the same list under the name a `require` reader`` |
|        - |  9254 | `	 * reaches for. */` |
|        - |  9255 | `	{ "get_required_files",vm_builtin_get_included_files},` |
|        - |  9256 | `	{ "include",      vm_builtin_include          },` |
|        - |  9257 | `	{ "include_once", vm_builtin_include_once     },` |
|        - |  9258 | `	{ "require",      vm_builtin_require          },` |
|        - |  9259 | `	{ "require_once", vm_builtin_require_once     },` |
|        - |  9260 | `};` |
|        - |  9261 | `/*` |
|        - |  9262 | ` * Register the built-in VM functions defined above.` |
|        - |  9263 | ` */` |
|     7925 |  9264 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 |  9265 | `{` |
|        - |  9266 | `	sxi32 rc;` |
|        - |  9267 | `	sxu32 n;` |
|  1077805 |  9268 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - |  9269 | `		/* Note that these special functions have access` |
|        - |  9270 | `		 * to the underlying virtual machine as their` |
|        - |  9271 | `		 * private data.` |
|        - |  9272 | `		 */` |
|  1069880 |  9273 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|  1069880 |  9274 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  9275 | `			return rc;` |
|        - |  9276 | `		}` |
|   534200 |  9277 | `	}` |
|     7930 |  9278 | `	return SXRET_OK;` |
|     3962 |  9279 | `}` |
|        - |  9280 | `/*` |
|        - |  9281 | ` * The nine host functions that are php LANGUAGE CONSTRUCTS. Their implementations stay` |
|        - |  9282 | ` * registered -- each construct's codegen dispatches an OP_CALL to the name -- and the` |
|        - |  9283 | ` * mark hides the name from every door a SCRIPT can ask through (PH7_VmGetHostFunction).` |
|        - |  9284 | ` *` |
|        - |  9285 | `` * `echo` is NOT here: it is the one construct with no call at all (PH7_CompileEcho emits`` |
|        - |  9286 | ``  * OP_CONSUME), so its host function was simply dropped rather than hidden. `exit`, `die` `` |
|        - |  9287 | `` * and `clone` are not here either -- php 8.5 has all three as real internal functions.`` |
|        - |  9288 | ` */` |
|        - |  9289 | `static const char * const azLangConstruct[] = {` |
|        - |  9290 | `	"print", "isset", "unset", "empty", "eval",` |
|        - |  9291 | `	"include", "include_once", "require", "require_once"` |
|        - |  9292 | `};` |
|        - |  9293 | `/*` |
|        - |  9294 | ` * Stamp the mark. Runs once at VM init, after every registration pass: the nine live in` |
|        - |  9295 | `` * two different tables (`empty` in builtin.c's, the rest in this file's), and one list`` |
|        - |  9296 | ` * walked at the end covers both without either table having to know.` |
|        - |  9297 | ` */` |
|     6691 |  9298 | `PH7_PRIVATE void PH7_VmMarkLanguageConstructs(ph7_vm *pVm)` |
|        5 |  9299 | `{` |
|        - |  9300 | `	sxu32 n;` |
|    66915 |  9301 | `	for( n = 0 ; n < SX_ARRAYSIZE(azLangConstruct) ; ++n ){` |
|   120443 |  9302 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|    60219 |  9303 | `			(const void *)azLangConstruct[n],SyStrlen(azLangConstruct[n]));` |
|    60224 |  9304 | `		if( pEntry && pEntry->pUserData ){` |
|    60224 |  9305 | `			((ph7_user_func *)pEntry->pUserData)->bConstruct = 1;` |
|    30060 |  9306 | `		}` |
|    30065 |  9307 | `	}` |
|     6696 |  9308 | `}` |
|        - |  9309 | `/*` |
|        - |  9310 | ` * The internal function names this engine carries that php has none of. They matter` |
|        - |  9311 | ` * to exactly one question -- may a user function take this name? -- and the answer` |
|        - |  9312 | ` * php gives is "no, the name is already taken", which is only true of a name php` |
|        - |  9313 | ` * itself has. Shadowing one of these RUNS under php, so it has to run here.` |
|        - |  9314 | ` *` |
|        - |  9315 | ` * Each is a PH7 legacy verb kept for the embedding API, an engine introspection` |
|        - |  9316 | ` * hook, or a name php REMOVED (each(), fgetss(), import_request_variables()) -- and` |
|        - |  9317 | ` * every one of them is a name real code declares: size_format() is WordPress's, and` |
|        - |  9318 | ` * each() is the classic php-5 polyfill.` |
|        - |  9319 | ` *` |
|        - |  9320 | ` * Re-derive after adding or removing a builtin, by diffing the internal half of` |
|        - |  9321 | ` * get_defined_functions() between this engine and the oracle:` |
|        - |  9322 | ` *` |
|        - |  9323 | ` *   echo '<?php $f=get_defined_functions()["internal"]; sort($f);` |
|        - |  9324 | ` *         echo implode("\n",$f),"\n";' > names.php` |
|        - |  9325 | ` *   phl names.php \| LC_ALL=C sort -u > phl.names` |
|        - |  9326 | ` *   XDEBUG_MODE=off php names.php \| LC_ALL=C sort -u > php.names` |
|        - |  9327 | ` *   LC_ALL=C comm -23 phl.names php.names` |
|        - |  9328 | ` *` |
|        - |  9329 | ` * The nine LANGUAGE CONSTRUCTS above are not here and do not need to be: every one` |
|        - |  9330 | `` * of them is a reserved word, so `function print(){}` never reaches this question --`` |
|        - |  9331 | ` * the parser refuses it first, under php and under this.` |
|        - |  9332 | ` *` |
|        - |  9333 | ` * __tempnam_in() is not here either, and is the one name this list still owes php:` |
|        - |  9334 | ` * it is a builtin this engine writes in PHP, so it lives in the USER table, and` |
|        - |  9335 | ` * letting the declaration through is not enough -- a call compiled against the` |
|        - |  9336 | ` * engine's copy stays bound to it, so the user function installs, answers` |
|        - |  9337 | ` * Reflection, and never runs. It needs the call site rebound, not a carve-out.` |
|        - |  9338 | ` */` |
|        - |  9339 | `static const char * const azEngineOnlyFunc[] = {` |
|        - |  9340 | `	"array_copy", "array_erase", "array_same", "debug_string_backtrace",` |
|        - |  9341 | `	"delete", "each", "fgetss", "func_get_args_byref", "get_defined_classes",` |
|        - |  9342 | `	"getgid", "getpid", "getuid", "implode_recursive",` |
|        - |  9343 | `	"import_request_variables", "join_recursive", "ph7_info", "ph7_uname",` |
|        - |  9344 | `	"ph7copyright", "ph7credits", "ph7info", "ph7version", "rand_str",` |
|        - |  9345 | `	"setenv", "size_format", "strglob"` |
|        - |  9346 | `};` |
|        - |  9347 | `/*` |
|        - |  9348 | ` */` |
|        - |  9349 | `/*` |
|        - |  9350 | ` * Is this name already an INTERNAL function, in the sense php's redeclaration fatal` |
|        - |  9351 | ` * means it? Both doors of a function declaration ask it -- the compiler for a` |
|        - |  9352 | ` * top-level one, OP_FUNC_DECL for a conditional one -- so they refuse the same set.` |
|        - |  9353 | ` *` |
|        - |  9354 | ` * The engine's own extras above are excluded; everything else in hHostFunction is a` |
|        - |  9355 | ` * name php has too, because the table this engine registers IS php's list minus what` |
|        - |  9356 | ` * is not built yet. A name php has that this engine has not implemented is simply` |
|        - |  9357 | ` * not here, and shadowing it stays allowed -- that is the capability gap, not this.` |
|        - |  9358 | ` */` |
|       14 |  9359 | `static int VmNameIsEngineOnlyFunc(const char *zName,sxu32 nByte)` |
|        4 |  9360 | `{` |
|        - |  9361 | `	sxu32 n;` |
|      322 |  9362 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEngineOnlyFunc) ; ++n ){` |
|      310 |  9363 | `		if( SyStrlen(azEngineOnlyFunc[n]) == nByte` |
|      175 |  9364 | `		 && SyStrnicmp(azEngineOnlyFunc[n],zName,nByte) == 0 ){` |
|        7 |  9365 | `			return 1;` |
|        - |  9366 | `		}` |
|      156 |  9367 | `	}` |
|       11 |  9368 | `	return 0;` |
|       11 |  9369 | `}` |
|     4506 |  9370 | `PH7_PRIVATE int PH7_VmNameIsInternalFunc(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        5 |  9371 | `{` |
|     4511 |  9372 | `	if( nByte < 1 ){` |
|      ! 0 |  9373 | `		return 0;` |
|        - |  9374 | `	}` |
|     4511 |  9375 | `	if( SyHashGet(&pVm->hHostFunction,(const void *)zName,nByte) == 0 ){` |
|     4497 |  9376 | `		return 0;` |
|        - |  9377 | `	}` |
|       18 |  9378 | `	return !VmNameIsEngineOnlyFunc(zName,nByte);` |
|     2252 |  9379 | `}` |
|        - |  9380 | `/*` |
|        - |  9381 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - |  9382 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - |  9383 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - |  9384 | ` */` |
|  9584974 |  9385 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 |  9386 | `{` |
|  9584979 |  9387 | `	if( !iLoadable ){` |
|  6979049 |  9388 | `		return pClass;` |
|        - |  9389 | `	}` |
|  2605951 |  9390 | `	while(pClass){` |
|  2605935 |  9391 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|  2605919 |  9392 | `			return pClass;` |
|        - |  9393 | `		}` |
|       17 |  9394 | `		pClass = pClass->pNextName;` |
|        1 |  9395 | `	}` |
|       17 |  9396 | `	return 0;` |
|  4790521 |  9397 | `}` |
|        - |  9398 | `/*` |
|        - |  9399 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - |  9400 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - |  9401 | ` * with the class name. After each callback, checks if the class is now` |
|        - |  9402 | ` * registered in the VM's class table.` |
|        - |  9403 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - |  9404 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - |  9405 | ` */` |
|      590 |  9406 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 |  9407 | `{` |
|        - |  9408 | `	VmAutoloadCB *pEntry;` |
|        - |  9409 | `	ph7_value sArg,sResult;` |
|        - |  9410 | `	SyHashEntry *pHashEntry;` |
|        - |  9411 | `	ph7_class *pClass;` |
|        - |  9412 | `	sxu32 n,nEntry;` |
|      595 |  9413 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|      595 |  9414 | `	if( nEntry < 1 ){` |
|      359 |  9415 | `		return 0;` |
|        - |  9416 | `	}` |
|        - |  9417 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|      241 |  9418 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 |  9419 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - |  9420 | `	}` |
|        - |  9421 | `	/* Mark this class as being autoloaded */` |
|      239 |  9422 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - |  9423 | `	/* Prepare the class name argument */` |
|      239 |  9424 | `	PH7_MemObjInit(pVm,&sArg);` |
|      239 |  9425 | `	PH7_MemObjInit(pVm,&sResult);` |
|      239 |  9426 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|      239 |  9427 | `	pClass = 0;` |
|      521 |  9428 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - |  9429 | `		ph7_value *apArg[1];` |
|      363 |  9430 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      363 |  9431 | `		if( pEntry == 0 ){` |
|      ! 0 |  9432 | `			continue;` |
|        - |  9433 | `		}` |
|      363 |  9434 | `		apArg[0] = &sArg;` |
|        - |  9435 | `		/* NOT PH7_VmCallUserFunction: that wrapper marks the dispatch as an internal` |
|        - |  9436 | `		 * function reaching for a callback, which binds the argument weakly. php's` |
|        - |  9437 | `		 * autoload call is the one such dispatch that is NOT weak -- it reads the` |
|        - |  9438 | `		 * strict_types of the code whose class reference triggered it, and says so in` |
|        - |  9439 | `		 * its own diagnostic ("called in <that file> on line <that line>"). An` |
|        - |  9440 | ``		 * autoloader declaring anything but `string` therefore RAISES under a strict`` |
|        - |  9441 | ``		 * caller, where PHL coerced the class name (`bool $c` got true) and ran the`` |
|        - |  9442 | `		 * loader on a value that no longer named anything. */` |
|      363 |  9443 | `		if( PH7_VmCallUserFunctionWithMap(pVm,&pEntry->sCallback,1,apArg,&sResult,0) != SXRET_OK ){` |
|        - |  9444 | `			/* Callback could not be invoked — skip to next autoloader */` |
|       26 |  9445 | `			continue;` |
|        - |  9446 | `		}` |
|        - |  9447 | `		/* Check if the class is now available */` |
|      339 |  9448 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|      339 |  9449 | `		if( pHashEntry ){` |
|       81 |  9450 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       81 |  9451 | `			if( pClass ){` |
|       81 |  9452 | `				break;` |
|        - |  9453 | `			}` |
|      ! 0 |  9454 | `		}` |
|      133 |  9455 | `	}` |
|      239 |  9456 | `	PH7_MemObjRelease(&sArg);` |
|      239 |  9457 | `	PH7_MemObjRelease(&sResult);` |
|        - |  9458 | `	/* Remove reentrancy guard */` |
|      239 |  9459 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|      239 |  9460 | `	return pClass;` |
|      300 |  9461 | `}` |
|        - |  9462 | `/*` |
|        - |  9463 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - |  9464 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - |  9465 | ` */` |
|       66 |  9466 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 |  9467 | `{` |
|       71 |  9468 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 |  9469 | `}` |
|        - |  9470 | `/*` |
|        - |  9471 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|        - |  9472 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|        - |  9473 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|        - |  9474 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|        - |  9475 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|        - |  9476 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|        - |  9477 | ` * only turn a failing lookup into a match, never break an existing one.` |
|        - |  9478 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|        - |  9479 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|        - |  9480 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|        - |  9481 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|        - |  9482 | ` */` |
| 10113349 |  9483 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|        5 |  9484 | `{` |
| 10113354 |  9485 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|       87 |  9486 | `		(*pzName)++;` |
|       87 |  9487 | `		(*pnByte)--;` |
|       41 |  9488 | `	}` |
| 10113354 |  9489 | `}` |
|        - |  9490 | `/*` |
|        - |  9491 | ` * Check if the given name refer to an installed class.` |
|        - |  9492 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - |  9493 | ` */` |
|  9585432 |  9494 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - |  9495 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |  9496 | `	const char *zName,  /* Name of the target class */` |
|        - |  9497 | `	sxu32 nByte,        /* zName length */` |
|        - |  9498 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - |  9499 | `						 * [i.e: no abstract classes or interfaces]` |
|        - |  9500 | `						 */` |
|        - |  9501 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - |  9502 | `	)` |
|        5 |  9503 | `{` |
|        - |  9504 | `	SyHashEntry *pEntry;` |
|        - |  9505 | `	ph7_class *pClass;` |
|  4790745 |  9506 | `	SXUNUSED(iNest);` |
|        - |  9507 | `	/* Exact class lookup.` |
|        - |  9508 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  9509 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|        - |  9510 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|        - |  9511 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|  9585437 |  9512 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|        - |  9513 | `	/* An empty stripped name names no class: neither a truly empty "" nor a lone` |
|        - |  9514 | `	 * "\" is looked up or handed to the autoloader (php 8.5.11, GH-23232). */` |
|  9585437 |  9515 | `	if( nByte < 1 ){` |
|       12 |  9516 | `		return 0;` |
|        - |  9517 | `	}` |
|  9585427 |  9518 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|  9585427 |  9519 | `	if( pEntry == 0 ){` |
|        - |  9520 | `		/* Class not found in hash table — try autoload before giving up */` |
|      529 |  9521 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - |  9522 | `	}` |
|  9584903 |  9523 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|  9584903 |  9524 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|  4790750 |  9525 | `}` |
|        - |  9526 | `/*` |
|        - |  9527 | ` * Reference Table Implementation` |
|        - |  9528 | ` * Status: stable <chm@symisc.net>` |
|        - |  9529 | ` * Intro` |
|        - |  9530 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - |  9531 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - |  9532 | ` *  the reference implementation is consistent,solid and it's` |
|        - |  9533 | ` *  behavior resemble the C++ reference mechanism.` |
|        - |  9534 | ` *  Refer to the official for more information on this powerful` |
|        - |  9535 | ` *  extension.` |
|        - |  9536 | ` */` |
|        - |  9537 | `/*` |
|        - |  9538 | ` * ---------------------------------------------------------------------------` |
|        - |  9539 | ` * The reference table.` |
|        - |  9540 | ` *` |
|        - |  9541 | ` * One TAGGED WORD per memory-object slot (pVm->apRefObj[nIdx]; see VM_REF_TAG_*` |
|        - |  9542 | ` * in ph7int.h for the five shapes). The table answers one question -- who still` |
|        - |  9543 | ` * holds this slot -- and that answer decides when a value is freed, so every` |
|        - |  9544 | ` * accessor below is asked BY SLOT INDEX: a slot whose answer fits in its word has` |
|        - |  9545 | ` * no record for a caller to hold on to.` |
|        - |  9546 | ` *` |
|        - |  9547 | ` * A record (VmRefObj) is the fallback for the answers a word cannot carry: two or` |
|        - |  9548 | ` * more names, two or more nodes, or a pin standing beside a named holder.` |
|        - |  9549 | ` * ---------------------------------------------------------------------------` |
|        - |  9550 | ` */` |
|        - |  9551 | `/* The word for a slot the table has been grown to cover; 0 otherwise. */` |
| 42292054 |  9552 | `static void * VmRefWord(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9553 | `{` |
| 42292059 |  9554 | `	if( nIdx >= pVm->nRefSize ){` |
|        2 |  9555 | `		return 0;` |
|        - |  9556 | `	}` |
| 42292057 |  9557 | `	return pVm->apRefObj[nIdx];` |
| 21144963 |  9558 | `}` |
|        - |  9559 | `/*` |
|        - |  9560 | ` * Grow the table to cover a slot. Doubling keeps the growth amortized and, unlike` |
|        - |  9561 | ` * the hash table this replaced, nothing has to be MOVED: the cells that exist keep` |
|        - |  9562 | ` * their index, so the copy is one memcpy and the tail is zeroed.` |
|        - |  9563 | ` */` |
| 24293997 |  9564 | `static sxi32 VmRefTableGrow(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9565 | `{` |
|        - |  9566 | `	void **apNew;` |
|        - |  9567 | `	sxu32 nNew;` |
| 24294002 |  9568 | `	if( nIdx < pVm->nRefSize ){` |
| 24285130 |  9569 | `		return SXRET_OK;` |
|        - |  9570 | `	}` |
|     8877 |  9571 | `	nNew = pVm->nRefSize ? pVm->nRefSize : 0x10;` |
|    17751 |  9572 | `	while( nIdx >= nNew ){` |
|     8879 |  9573 | `		nNew <<= 1;` |
|        5 |  9574 | `	}` |
|     8877 |  9575 | `	apNew = (void **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(void *) * nNew);` |
|     8877 |  9576 | `	if( apNew == 0 ){` |
|      ! 0 |  9577 | `		return SXERR_MEM;` |
|        - |  9578 | `	}` |
|     8877 |  9579 | `	if( pVm->nRefSize > 0 ){` |
|     8877 |  9580 | `		SyMemcpy((const void *)pVm->apRefObj,(void *)apNew,pVm->nRefSize * sizeof(void *));` |
|     4422 |  9581 | `	}` |
|     8877 |  9582 | `	SyZero((void *)&apNew[pVm->nRefSize],(nNew - pVm->nRefSize) * sizeof(void *));` |
|     8877 |  9583 | `	SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|     8877 |  9584 | `	pVm->apRefObj = apNew;` |
|     8877 |  9585 | `	pVm->nRefSize = nNew;` |
|     8877 |  9586 | `	return SXRET_OK;` |
| 12145529 |  9587 | `}` |
|        - |  9588 | `/*` |
|        - |  9589 | ` * Store a word. The ONE place nRefUsed moves: it counts FILLED CELLS, so a cell` |
|        - |  9590 | ` * that goes from one shape to another (a name dropped to a bare mark, a word` |
|        - |  9591 | ` * promoted to a record) does not move it. The caller must already have grown the` |
|        - |  9592 | ` * table -- this cannot fail, which is what lets the callers below commit a state` |
|        - |  9593 | ` * change and a holder in the same breath.` |
|        - |  9594 | ` */` |
| 70441132 |  9595 | `static void VmRefWordSet(ph7_vm *pVm,sxu32 nIdx,void *pWord)` |
|        5 |  9596 | `{` |
| 70441137 |  9597 | `	void *pOld = pVm->apRefObj[nIdx];` |
| 70441137 |  9598 | `	if( pOld == 0 && pWord != 0 ){` |
| 24089432 |  9599 | `		pVm->nRefUsed++;` |
| 58395150 |  9600 | `	}else if( pOld != 0 && pWord == 0 ){` |
| 23134188 |  9601 | `		pVm->nRefUsed--;` |
| 11566506 |  9602 | `	}` |
| 70441137 |  9603 | `	pVm->apRefObj[nIdx] = pWord;` |
| 70441137 |  9604 | `}` |
|        - |  9605 | `/* A holder pointer carried in a word, with its tag taken back off. */` |
|        - |  9606 | `#define VM_REF_UNTAG(W,T) ((void *)&((char *)(W))[-(T)])` |
|        - |  9607 | `/*` |
|        - |  9608 | ` * May this pointer be tagged? The pool allocator keeps every chunk 8-aligned (the` |
|        - |  9609 | ` * C library's own alignment, a SyMemBlock that is a multiple of 8, and a` |
|        - |  9610 | ` * pointer-sized SyMemHeader -- see the alignment note on sxmem.c's OS methods), so` |
|        - |  9611 | ` * this is true everywhere it is asked -- but a word whose low bits are not free` |
|        - |  9612 | ` * would read back as another shape entirely, so the question is asked rather than` |
|        - |  9613 | ` * assumed and a stray pointer simply takes the record path.` |
|        - |  9614 | ` */` |
| 13624363 |  9615 | `static int VmRefTaggable(void *pPtr)` |
|        5 |  9616 | `{` |
| 13624368 |  9617 | `	return pPtr != 0 && (SX_PTR_TO_INT(pPtr) & VM_REF_TAG_MASK) == 0;` |
|        5 |  9618 | `}` |
|        - |  9619 | `/* Build a MARK word out of a pin count and the flags. */` |
| 33583451 |  9620 | `static void * VmRefMarkWord(sxu32 nPin,sxi32 iFlags)` |
|        5 |  9621 | `{` |
| 33583456 |  9622 | `	int iWord = VM_REF_TAG_MARK;` |
| 33583456 |  9623 | `	if( iFlags & VM_REF_IDX_KEEP ){` |
| 10465389 |  9624 | `		iWord \|= VM_REF_MARK_KEEP;` |
|  5232082 |  9625 | `	}` |
| 33583456 |  9626 | `	iWord \|= (int)(nPin * VM_REF_MARK_PIN);` |
| 33583456 |  9627 | `	return SX_INT_TO_PTR(iWord);` |
|        5 |  9628 | `}` |
| 22355361 |  9629 | `static sxu32 VmRefMarkPin(void *pWord)` |
|        5 |  9630 | `{` |
| 22355366 |  9631 | `	return ((sxu32)SX_PTR_TO_INT(pWord)) / VM_REF_MARK_PIN;` |
|        5 |  9632 | `}` |
| 11122572 |  9633 | `static int VmRefMarkKeep(void *pWord)` |
|        5 |  9634 | `{` |
| 11122577 |  9635 | `	return (SX_PTR_TO_INT(pWord) & VM_REF_MARK_KEEP) != 0;` |
|        5 |  9636 | `}` |
|        - |  9637 | `/*` |
|        - |  9638 | ` * Allocate a new reference record.` |
|        - |  9639 | ` *` |
|        - |  9640 | ` * Reached only by a slot whose holders will not fit in its word -- two names, two` |
|        - |  9641 | ` * nodes, or a pin beside a named holder. It used to be allocated for EVERY variable` |
|        - |  9642 | ` * a frame binds and EVERY element an array inserts, which made its size the engine's` |
|        - |  9643 | ` * per-value memory overhead.` |
|        - |  9644 | ` */` |
|    99135 |  9645 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9646 | `{` |
|        - |  9647 | `	VmRefObj *pRef;` |
|    99140 |  9648 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|    99140 |  9649 | `	if( pRef == 0 ){` |
|      ! 0 |  9650 | `		return 0;` |
|        - |  9651 | `	}` |
|        - |  9652 | `	/* Zero the structure */` |
|    99140 |  9653 | `	SyZero(pRef,sizeof(VmRefObj));` |
|    99140 |  9654 | `	pRef->nIdx = nIdx;` |
|    99140 |  9655 | `	return pRef;` |
|    49476 |  9656 | `}` |
|        - |  9657 | `/*` |
|        - |  9658 | ` * The spill sets of a record that has just been given a second holder, created on` |
|        - |  9659 | ` * demand. NULL on OOM, in which case the caller drops the row -- the same` |
|        - |  9660 | ` * degradation SySetPut's own failure already produced.` |
|        - |  9661 | ` */` |
|     3422 |  9662 | `static VmRefSpill * VmRefSpillGet(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 |  9663 | `{` |
|        - |  9664 | `	VmRefSpill *pSpill;` |
|     3427 |  9665 | `	if( pRef->pSpill ){` |
|     2700 |  9666 | `		return pRef->pSpill;` |
|        - |  9667 | `	}` |
|      732 |  9668 | `	pSpill = (VmRefSpill *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefSpill));` |
|      732 |  9669 | `	if( pSpill == 0 ){` |
|      ! 0 |  9670 | `		return 0;` |
|        - |  9671 | `	}` |
|      732 |  9672 | `	SySetInit(&pSpill->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|      732 |  9673 | `	SySetInit(&pSpill->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|      732 |  9674 | `	pRef->pSpill = pSpill;` |
|      732 |  9675 | `	return pSpill;` |
|     1714 |  9676 | `}` |
|        - |  9677 | `/*` |
|        - |  9678 | ` * File one holder on a record, ignoring a name that is already on it -- a name can be` |
|        - |  9679 | ` * RE-BOUND to the same slot any number of times, and a table that only ever grew made` |
|        - |  9680 | ` * both the install and the holder count O(rows).` |
|        - |  9681 | ` */` |
|    26900 |  9682 | `static void VmRefAddEntry(ph7_vm *pVm,VmRefObj *pRef,SyHashEntry *pEntry)` |
|        5 |  9683 | `{` |
|        - |  9684 | `	VmRefSpill *pSpill;` |
|        - |  9685 | `	SyHashEntry **apEntry;` |
|    26905 |  9686 | `	sxu32 n, nFree = SXU32_HIGH;` |
|    26905 |  9687 | `	if( pRef->pEntry0 == pEntry ){` |
|      ! 0 |  9688 | `		return;` |
|        - |  9689 | `	}` |
|    26905 |  9690 | `	if( pRef->pEntry0 == 0 ){` |
|    24123 |  9691 | `		pRef->pEntry0 = pEntry;` |
|    24123 |  9692 | `		return;` |
|        - |  9693 | `	}` |
|     2787 |  9694 | `	pSpill = VmRefSpillGet(&(*pVm),pRef);` |
|     2787 |  9695 | `	if( pSpill == 0 ){` |
|      ! 0 |  9696 | `		return;` |
|        - |  9697 | `	}` |
|     2787 |  9698 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pSpill->aReference);` |
|     5446 |  9699 | `	for( n = 0 ; n < SySetUsed(&pSpill->aReference) ; ++n ){` |
|     2664 |  9700 | `		if( apEntry[n] == pEntry ){` |
|      ! 0 |  9701 | `			return; /* already recorded: never file one holder twice */` |
|        - |  9702 | `		}` |
|     2664 |  9703 | `		if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|     2452 |  9704 | `			nFree = n; /* a row a dead holder left behind */` |
|     1222 |  9705 | `		}` |
|     1333 |  9706 | `	}` |
|     2787 |  9707 | `	if( nFree != SXU32_HIGH ){` |
|     2452 |  9708 | `		apEntry[nFree] = pEntry;` |
|     1227 |  9709 | `	}else{` |
|      340 |  9710 | `		SySetPut(&pSpill->aReference,(const void *)&pEntry);` |
|        - |  9711 | `	}` |
|    13417 |  9712 | `}` |
|    74395 |  9713 | `static void VmRefAddNode(ph7_vm *pVm,VmRefObj *pRef,ph7_hashmap_node *pNode)` |
|        5 |  9714 | `{` |
|        - |  9715 | `	VmRefSpill *pSpill;` |
|        - |  9716 | `	ph7_hashmap_node **apNode;` |
|    74400 |  9717 | `	sxu32 n, nFree = SXU32_HIGH;` |
|    74400 |  9718 | `	if( pRef->pNode0 == pNode ){` |
|      ! 0 |  9719 | `		return;` |
|        - |  9720 | `	}` |
|    74400 |  9721 | `	if( pRef->pNode0 == 0 ){` |
|    73760 |  9722 | `		pRef->pNode0 = pNode;` |
|    73760 |  9723 | `		return;` |
|        - |  9724 | `	}` |
|      645 |  9725 | `	pSpill = VmRefSpillGet(&(*pVm),pRef);` |
|      645 |  9726 | `	if( pSpill == 0 ){` |
|      ! 0 |  9727 | `		return;` |
|        - |  9728 | `	}` |
|      645 |  9729 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pSpill->aArrEntries);` |
|      945 |  9730 | `	for( n = 0 ; n < SySetUsed(&pSpill->aArrEntries) ; ++n ){` |
|      302 |  9731 | `		if( apNode[n] == pNode ){` |
|      ! 0 |  9732 | `			return;` |
|        - |  9733 | `		}` |
|      302 |  9734 | `		if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|       19 |  9735 | `			nFree = n;` |
|        9 |  9736 | `		}` |
|      152 |  9737 | `	}` |
|      645 |  9738 | `	if( nFree != SXU32_HIGH ){` |
|       19 |  9739 | `		apNode[nFree] = pNode;` |
|       10 |  9740 | `	}else{` |
|      627 |  9741 | `		SySetPut(&pSpill->aArrEntries,(const void *)&pNode);` |
|        - |  9742 | `	}` |
|    37142 |  9743 | `}` |
|        - |  9744 | `/*` |
|        - |  9745 | ` * Drop one holder from a record. Every row that names it goes, inline or spilled:` |
|        - |  9746 | ` * the table has never promised a holder appears once, and the count below reads` |
|        - |  9747 | ` * whatever is left.` |
|        - |  9748 | ` */` |
|    19874 |  9749 | `static void VmRefDropEntry(VmRefObj *pRef,SyHashEntry *pEntry)` |
|        5 |  9750 | `{` |
|    19879 |  9751 | `	if( pRef->pEntry0 == pEntry ){` |
|    17069 |  9752 | `		pRef->pEntry0 = 0;` |
|     8527 |  9753 | `	}` |
|    19879 |  9754 | `	if( pRef->pSpill ){` |
|     3207 |  9755 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->pSpill->aReference);` |
|        - |  9756 | `		sxu32 n;` |
|     6385 |  9757 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aReference) ; ++n ){` |
|     3183 |  9758 | `			if( apEntry[n] == pEntry ){` |
|     2769 |  9759 | `				apEntry[n] = 0;` |
|     1380 |  9760 | `			}` |
|     1592 |  9761 | `		}` |
|     1599 |  9762 | `	}` |
|    19879 |  9763 | `}` |
|    16060 |  9764 | `static void VmRefDropNode(VmRefObj *pRef,ph7_hashmap_node *pNode)` |
|        5 |  9765 | `{` |
|    16065 |  9766 | `	if( pRef->pNode0 == pNode ){` |
|    15579 |  9767 | `		pRef->pNode0 = 0;` |
|     7782 |  9768 | `	}` |
|    16065 |  9769 | `	if( pRef->pSpill ){` |
|      889 |  9770 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->pSpill->aArrEntries);` |
|        - |  9771 | `		sxu32 n;` |
|     2263 |  9772 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aArrEntries) ; ++n ){` |
|     1377 |  9773 | `			if( apNode[n] == pNode ){` |
|      489 |  9774 | `				apNode[n] = 0;` |
|      243 |  9775 | `			}` |
|      690 |  9776 | `		}` |
|      443 |  9777 | `	}` |
|    16065 |  9778 | `}` |
|        - |  9779 | `/*` |
|        - |  9780 | ` * How many LIVE holders of each kind a RECORD still carries. A node counts only while` |
|        - |  9781 | ` * it still points HERE -- a slot index travels through the free list, so a record can` |
|        - |  9782 | ` * outlive the node that filed the row.` |
|        - |  9783 | ` */` |
|    25043 |  9784 | `static sxu32 VmRefEntryCount(VmRefObj *pRef)` |
|        5 |  9785 | `{` |
|    25048 |  9786 | `	sxu32 n, nLive = pRef->pEntry0 ? 1 : 0;` |
|    25048 |  9787 | `	if( pRef->pSpill ){` |
|     1406 |  9788 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->pSpill->aReference);` |
|     1756 |  9789 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aReference) ; ++n ){` |
|      353 |  9790 | `			if( apEntry[n] ){` |
|       72 |  9791 | `				nLive++;` |
|       35 |  9792 | `			}` |
|      178 |  9793 | `		}` |
|      701 |  9794 | `	}` |
|    25048 |  9795 | `	return nLive;` |
|        5 |  9796 | `}` |
|    21342 |  9797 | `static sxu32 VmRefNodeCount(VmRefObj *pRef,sxu32 nIdx)` |
|        5 |  9798 | `{` |
|    21347 |  9799 | `	sxu32 n, nLive = (pRef->pNode0 && pRef->pNode0->nValIdx == nIdx) ? 1 : 0;` |
|    21347 |  9800 | `	if( pRef->pSpill ){` |
|     1278 |  9801 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->pSpill->aArrEntries);` |
|     2978 |  9802 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aArrEntries) ; ++n ){` |
|     1704 |  9803 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|      830 |  9804 | `				nLive++;` |
|      413 |  9805 | `			}` |
|      854 |  9806 | `		}` |
|      637 |  9807 | `	}` |
|    21347 |  9808 | `	return nLive;` |
|        5 |  9809 | `}` |
|        - |  9810 | `/*` |
|        - |  9811 | ` * The RECORD behind a slot, or 0 when the slot's answer is in its word (which,` |
|        - |  9812 | ` * unlike the old VmRefObjExtract, does NOT mean the slot is unheld). Nothing` |
|        - |  9813 | ` * outside this section may hold one: every question is asked by index below.` |
|        - |  9814 | ` */` |
|      208 |  9815 | `static VmRefObj * VmRefFull(ph7_vm *pVm,sxu32 nIdx)` |
|        2 |  9816 | `{` |
|      210 |  9817 | `	void *pWord = VmRefWord(&(*pVm),nIdx);` |
|      210 |  9818 | `	if( pWord == 0 \|\| VM_REF_TAGOF(pWord) != VM_REF_TAG_FULL ){` |
|      ! 0 |  9819 | `		return 0;` |
|        - |  9820 | `	}` |
|      210 |  9821 | `	return (VmRefObj *)pWord;` |
|      106 |  9822 | `}` |
|        - |  9823 | `/*` |
|        - |  9824 | ` * Promote whatever a slot has into a real record, creating one if the slot has` |
|        - |  9825 | ` * nothing yet. 0 on OOM (the table cannot be grown, or the record cannot be` |
|        - |  9826 | ` * allocated), which every caller degrades to "the slot records nothing", exactly` |
|        - |  9827 | ` * as a failed install always has.` |
|        - |  9828 | ` */` |
|   103275 |  9829 | `static VmRefObj * VmRefMaterialize(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9830 | `{` |
|        - |  9831 | `	void *pWord;` |
|        - |  9832 | `	VmRefObj *pRef;` |
|        - |  9833 | `	int iTag;` |
|   103280 |  9834 | `	if( VmRefTableGrow(&(*pVm),nIdx) != SXRET_OK ){` |
|      ! 0 |  9835 | `		return 0;` |
|        - |  9836 | `	}` |
|   103280 |  9837 | `	pWord = pVm->apRefObj[nIdx];` |
|   103280 |  9838 | `	iTag = pWord ? VM_REF_TAGOF(pWord) : VM_REF_TAG_MARK;` |
|   103280 |  9839 | `	if( pWord != 0 && iTag == VM_REF_TAG_FULL ){` |
|     4145 |  9840 | `		return (VmRefObj *)pWord;` |
|        - |  9841 | `	}` |
|    99140 |  9842 | `	pRef = VmNewRefObj(&(*pVm),nIdx);` |
|    99140 |  9843 | `	if( pRef == 0 ){` |
|      ! 0 |  9844 | `		return 0;` |
|        - |  9845 | `	}` |
|    99140 |  9846 | `	if( pWord != 0 ){` |
|    99140 |  9847 | `		switch( iTag ){` |
|    37571 |  9848 | `		case VM_REF_TAG_NAME:` |
|    75026 |  9849 | `			pRef->pEntry0 = (SyHashEntry *)VM_REF_UNTAG(pWord,VM_REF_TAG_NAME);` |
|    75026 |  9850 | `			break;` |
|    12004 |  9851 | `		case VM_REF_TAG_NODE:` |
|    23941 |  9852 | `			pRef->pNode0 = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|    23941 |  9853 | `			break;` |
|       89 |  9854 | `		default: /* VM_REF_TAG_MARK */` |
|      182 |  9855 | `			pRef->nPin = VmRefMarkPin(pWord);` |
|      182 |  9856 | `			pRef->iFlags = VmRefMarkKeep(pWord) ? VM_REF_IDX_KEEP : 0;` |
|      178 |  9857 | `			break;` |
|        - |  9858 | `		}` |
|    49471 |  9859 | `	}` |
|    99140 |  9860 | `	VmRefWordSet(&(*pVm),nIdx,(void *)pRef);` |
|    99140 |  9861 | `	return pRef;` |
|    51540 |  9862 | `}` |
|        - |  9863 | `/*` |
|        - |  9864 | ` * ---------------------------------------------------------------------------` |
|        - |  9865 | ` * The questions, all asked by slot index.` |
|        - |  9866 | ` * ---------------------------------------------------------------------------` |
|        - |  9867 | ` */` |
|        - |  9868 | `/* Has anything ever been registered against this slot? A slot that answers NO has` |
|        - |  9869 | ` * never been in the table at all, which is what keeps it out of the free pool. */` |
|  1642397 |  9870 | `PH7_PRIVATE int PH7_VmSlotRegistered(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9871 | `{` |
|  1642402 |  9872 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  9873 | `		return 0;` |
|        - |  9874 | `	}` |
|  1642402 |  9875 | `	return VmRefWord(&(*pVm),nIdx) != 0;` |
|   820650 |  9876 | `}` |
|        - |  9877 | `/*` |
|        - |  9878 | ` * The BARE MARK -- the one word that decides a slot's whole teardown by itself.` |
|        - |  9879 | ` *` |
|        - |  9880 | ` * VM_REF_TAG_MARK with nothing above the tag means "registered, held by nothing,` |
|        - |  9881 | ` * pinned by nothing", which is what a dropped holder leaves behind and therefore` |
|        - |  9882 | ` * what an array element's slot looks like once its node has been unlinked. For` |
|        - |  9883 | ` * that word all three of the questions below are already answered -- registered` |
|        - |  9884 | ` * yes, keep no, holders none -- and PH7_VmSlotUnlink has nothing to unlink but the` |
|        - |  9885 | ` * cell itself, so this empties it and says it did.` |
|        - |  9886 | ` *` |
|        - |  9887 | ` * A throwaway counter build over the ecosystem gate's phpcs step (built with the` |
|        - |  9888 | ` * counter, read once, then reverted) says it is not an edge case:` |
|        - |  9889 | ` * 22,831,962 of the 23,016,949 slots PH7_VmReleaseUnheldSlot is handed carry` |
|        - |  9890 | ` * exactly this word -- 99.20% -- and none of them carry a name or a node. The` |
|        - |  9891 | ` * teardown of one of those used to ask the same cell seven separate loads across` |
|        - |  9892 | ` * eight out-of-line calls.` |
|        - |  9893 | ` *` |
|        - |  9894 | ` * It cannot be confused with a real holder: a name or a node word is a pointer the` |
|        - |  9895 | ` * allocator has kept 4-aligned, tagged with 1 or 2, so its low two bits are never` |
|        - |  9896 | ` * 3; a record pointer's are 0; and a mark carrying a pin or a keep has bits set` |
|        - |  9897 | ` * above the tag. Only the bare mark is the integer 3.` |
|        - |  9898 | ` */` |
| 23133725 |  9899 | `PH7_PRIVATE int PH7_VmSlotDropIfBare(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9900 | `{` |
| 23133725 |  9901 | `	if( nIdx >= pVm->nRefSize` |
| 23133730 |  9902 | `	 \|\| SX_PTR_TO_INT(pVm->apRefObj[nIdx]) != VM_REF_TAG_MARK ){` |
|    15671 |  9903 | `		return 0;` |
|        - |  9904 | `	}` |
| 23118064 |  9905 | `	VmRefWordSet(&(*pVm),nIdx,0);` |
| 23118064 |  9906 | `	return 1;` |
| 11566282 |  9907 | `}` |
|        - |  9908 | `/* The names bound to the slot. */` |
|  1598494 |  9909 | `PH7_PRIVATE sxu32 PH7_VmSlotEntryCount(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9910 | `{` |
|        - |  9911 | `	void *pWord;` |
|  1598499 |  9912 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  9913 | `		return 0;` |
|        - |  9914 | `	}` |
|  1598499 |  9915 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  1598499 |  9916 | `	if( pWord == 0 ){` |
|      ! 0 |  9917 | `		return 0;` |
|        - |  9918 | `	}` |
|  1598499 |  9919 | `	switch( VM_REF_TAGOF(pWord) ){` |
|      ! 0 |  9920 | `	case VM_REF_TAG_NAME: return 1;` |
|   797929 |  9921 | `	case VM_REF_TAG_NODE: /* fall through */` |
|  1594785 |  9922 | `	case VM_REF_TAG_MARK: return 0;` |
|     3718 |  9923 | `	default:              return VmRefEntryCount((VmRefObj *)pWord);` |
|        - |  9924 | `	}` |
|   798710 |  9925 | `}` |
|        - |  9926 | `/* The array nodes still pointing HERE -- a node that has moved on does not count. */` |
|  1474870 |  9927 | `PH7_PRIVATE sxu32 PH7_VmSlotNodeCount(ph7_vm *pVm,sxu32 nIdx)` |
|        3 |  9928 | `{` |
|        - |  9929 | `	void *pWord;` |
|  1474873 |  9930 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  9931 | `		return 0;` |
|        - |  9932 | `	}` |
|  1474873 |  9933 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  1474873 |  9934 | `	if( pWord == 0 ){` |
|      ! 0 |  9935 | `		return 0;` |
|        - |  9936 | `	}` |
|  1474873 |  9937 | `	switch( VM_REF_TAGOF(pWord) ){` |
|        3 |  9938 | `	case VM_REF_TAG_NODE: {` |
|        7 |  9939 | `		ph7_hashmap_node *pNode = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|        7 |  9940 | `		return pNode->nValIdx == nIdx ? 1 : 0;` |
|        - |  9941 | `	}` |
|   737426 |  9942 | `	case VM_REF_TAG_NAME: /* fall through */` |
|  1474855 |  9943 | `	case VM_REF_TAG_MARK: return 0;` |
|       13 |  9944 | `	default:              return VmRefNodeCount((VmRefObj *)pWord,nIdx);` |
|        - |  9945 | `	}` |
|   737438 |  9946 | `}` |
|        - |  9947 | `/* The counted pins (a reference-bound property, one per binding). */` |
|  1600860 |  9948 | `PH7_PRIVATE sxu32 PH7_VmSlotPinCount(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9949 | `{` |
|        - |  9950 | `	void *pWord;` |
|  1600865 |  9951 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  9952 | `		return 0;` |
|        - |  9953 | `	}` |
|  1600865 |  9954 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  1600865 |  9955 | `	if( pWord == 0 ){` |
|      ! 0 |  9956 | `		return 0;` |
|        - |  9957 | `	}` |
|  1600865 |  9958 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|  1477113 |  9959 | `		return VmRefMarkPin(pWord);` |
|        - |  9960 | `	}` |
|   123757 |  9961 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|     3834 |  9962 | `		return ((VmRefObj *)pWord)->nPin;` |
|        - |  9963 | `	}` |
|   119927 |  9964 | `	return 0;` |
|   799893 |  9965 | `}` |
|        - |  9966 | `/* The permanent pin (VM_REF_IDX_KEEP): a use(&$x) capture, a static, an enum case,` |
|        - |  9967 | ` * and the hold a declared property has on its own value slot. */` |
|   162468 |  9968 | `PH7_PRIVATE int PH7_VmSlotKeepPinned(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  9969 | `{` |
|        - |  9970 | `	void *pWord;` |
|   162473 |  9971 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  9972 | `		return 0;` |
|        - |  9973 | `	}` |
|   162473 |  9974 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   162473 |  9975 | `	if( pWord == 0 ){` |
|      ! 0 |  9976 | `		return 0;` |
|        - |  9977 | `	}` |
|   162473 |  9978 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|     2319 |  9979 | `		return VmRefMarkKeep(pWord);` |
|        - |  9980 | `	}` |
|   160159 |  9981 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|    40236 |  9982 | `		return (((VmRefObj *)pWord)->iFlags & VM_REF_IDX_KEEP) != 0;` |
|        - |  9983 | `	}` |
|   119927 |  9984 | `	return 0;` |
|    80687 |  9985 | `}` |
|        - |  9986 | `/*` |
|        - |  9987 | ` * Is pNode the FIRST node filed against this slot, and the only live one? The cycle` |
|        - |  9988 | ` * collector's "is this element held by its own array and nothing else" test; the` |
|        - |  9989 | ` * first-filed row is the one an ordinary insert leaves, so any other answer means` |
|        - |  9990 | ` * somebody else is pointing at the same slot.` |
|        - |  9991 | ` */` |
|   119979 |  9992 | `PH7_PRIVATE int PH7_VmSlotSoleNodeIs(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)` |
|        4 |  9993 | `{` |
|        - |  9994 | `	void *pWord;` |
|   119983 |  9995 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  9996 | `		return 0;` |
|        - |  9997 | `	}` |
|   119983 |  9998 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   119983 |  9999 | `	if( pWord == 0 ){` |
|      ! 0 | 10000 | `		return 0;` |
|        - | 10001 | `	}` |
|   119983 | 10002 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_NODE ){` |
|   179350 | 10003 | `		return VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pNode` |
|   119923 | 10004 | `			&& pNode->nValIdx == nIdx;` |
|        - | 10005 | `	}` |
|       57 | 10006 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|       57 | 10007 | `		VmRefObj *pRef = (VmRefObj *)pWord;` |
|       57 | 10008 | `		return pRef->pNode0 == pNode && VmRefNodeCount(pRef,nIdx) == 1;` |
|        - | 10009 | `	}` |
|      ! 0 | 10010 | `	return 0;` |
|    59455 | 10011 | `}` |
|        - | 10012 | `/*` |
|        - | 10013 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|        - | 10014 | ` * names bound to it plus the array nodes pointing at it, plus the holders the` |
|        - | 10015 | ` * table cannot name. php refcounts a reference set and keeps the VALUE alive` |
|        - | 10016 | ` * while any holder remains, so this is the count every "may I release this` |
|        - | 10017 | ` * slot?" decision asks for.` |
|        - | 10018 | ` */` |
| 12520063 | 10019 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 10020 | `{` |
|        - | 10021 | `	void *pWord;` |
| 12520068 | 10022 | `	sxu32 nLive = 0;` |
| 12520068 | 10023 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 10024 | `		return 0;` |
|        - | 10025 | `	}` |
| 12520068 | 10026 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
| 12520068 | 10027 | `	if( pWord == 0 ){` |
|        3 | 10028 | `		return 0;` |
|        - | 10029 | `	}` |
| 12520066 | 10030 | `	switch( VM_REF_TAGOF(pWord) ){` |
|      ! 0 | 10031 | `	case VM_REF_TAG_NAME:` |
|      ! 0 | 10032 | `		return 1;` |
|   689342 | 10033 | `	case VM_REF_TAG_NODE: {` |
|  1378652 | 10034 | `		ph7_hashmap_node *pNode = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|  1378652 | 10035 | `		return pNode->nValIdx == nIdx ? 1 : 0;` |
|        - | 10036 | `	}` |
|  5557758 | 10037 | `	case VM_REF_TAG_MARK: {` |
| 11120089 | 10038 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
| 11120089 | 10039 | `		if( nPin > 0 ){` |
|        5 | 10040 | `			return nPin;` |
|        - | 10041 | `		}` |
|        - | 10042 | ``		/* A permanent pin -- a `use (&$x)` capture, a static, an enum case. It is the`` |
|        - | 10043 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|        - | 10044 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|        - | 10045 | `		 * php's refcount does. */` |
| 11120085 | 10046 | `		return VmRefMarkKeep(pWord) ? 1 : 0;` |
|        - | 10047 | `	}` |
|    10670 | 10048 | `	default: {` |
|    21335 | 10049 | `		VmRefObj *pRef = (VmRefObj *)pWord;` |
|    21335 | 10050 | `		if( pRef->nPin > 0 ){` |
|      112 | 10051 | `			nLive += pRef->nPin;` |
|    21280 | 10052 | `		}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|        8 | 10053 | `			nLive++;` |
|        3 | 10054 | `		}` |
|    21335 | 10055 | `		nLive += VmRefEntryCount(pRef);` |
|    21335 | 10056 | `		nLive += VmRefNodeCount(pRef,nIdx);` |
|    21335 | 10057 | `		return nLive;` |
|        - | 10058 | `	}` |
|        - | 10059 | `	}` |
|  6262297 | 10060 | `}` |
|        - | 10061 | `/*` |
|        - | 10062 | ` * Does this slot's holder count include the OWNER's own hold?` |
|        - | 10063 | ` *` |
|        - | 10064 | ` * A property's slot is installed in the reference table with a permanent pin` |
|        - | 10065 | ` * (VM_REF_IDX_KEEP) when the property was created dynamically or re-created after` |
|        - | 10066 | ` * unset(), with a COUNTED pin when the property was BOUND to somebody else's slot` |
|        - | 10067 | `` * (`$o->p =& $x`), and with nothing at all when it came straight from the class`` |
|        - | 10068 | ` * declaration. PH7_VmSlotHolderCount counts those pins as holders, so a renderer` |
|        - | 10069 | ` * asking "is this value a REFERENCE" has to subtract the one hold that is the` |
|        - | 10070 | ` * property itself -- an array ELEMENT, which is its own first holder in the table,` |
|        - | 10071 | ` * asks the same question with a threshold of two.` |
|        - | 10072 | ` */` |
|     5989 | 10073 | `PH7_PRIVATE int PH7_VmSlotSelfPinned(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 10074 | `{` |
|     5994 | 10075 | `	return PH7_VmSlotPinCount(&(*pVm),nIdx) > 0 \|\| PH7_VmSlotKeepPinned(&(*pVm),nIdx);` |
|        5 | 10076 | `}` |
|        - | 10077 | `/*` |
|        - | 10078 | ` * Delete every holder a slot has and empty its cell.` |
|        - | 10079 | ` *` |
|        - | 10080 | ` * Each row is cleared BEFORE the call that acts on it: unlinking a node runs the` |
|        - | 10081 | ` * value's release, which can re-enter this table for the same slot, and a row still` |
|        - | 10082 | ` * filled when that happens is a holder being handed out twice. The base pointer is` |
|        - | 10083 | ` * re-read per row for the same reason -- a re-entrant install may have grown the set` |
|        - | 10084 | ` * out from under it.` |
|        - | 10085 | ` */` |
|    16124 | 10086 | `PH7_PRIVATE void PH7_VmSlotUnlink(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 10087 | `{` |
|        - | 10088 | `	VmRefSpill *pSpill;` |
|        - | 10089 | `	VmRefObj *pRef;` |
|        - | 10090 | `	void *pWord;` |
|        - | 10091 | `	sxu32 n;` |
|    16129 | 10092 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|    16129 | 10093 | `	if( pWord == 0 ){` |
|      ! 0 | 10094 | `		return;` |
|        - | 10095 | `	}` |
|        - | 10096 | `	/* Empty the cell first: everything below can re-enter the table for this slot. */` |
|    16129 | 10097 | `	VmRefWordSet(&(*pVm),nIdx,0);` |
|    16129 | 10098 | `	switch( VM_REF_TAGOF(pWord) ){` |
|      ! 0 | 10099 | `	case VM_REF_TAG_NAME:` |
|      ! 0 | 10100 | `		SyHashDeleteEntry2((SyHashEntry *)VM_REF_UNTAG(pWord,VM_REF_TAG_NAME));` |
|      ! 0 | 10101 | `		return;` |
|      168 | 10102 | `	case VM_REF_TAG_NODE:` |
|      336 | 10103 | `		PH7_HashmapUnlinkNode((ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE),FALSE);` |
|      336 | 10104 | `		return;` |
|       33 | 10105 | `	case VM_REF_TAG_MARK:` |
|       67 | 10106 | `		return; /* pins and flags name nothing to delete */` |
|     7866 | 10107 | `	default:` |
|    15722 | 10108 | `		break;` |
|        - | 10109 | `	}` |
|    15727 | 10110 | `	pRef = (VmRefObj *)pWord;` |
|    15727 | 10111 | `	if( pRef->pEntry0 ){` |
|      167 | 10112 | `		SyHashEntry *pEntry = pRef->pEntry0;` |
|      167 | 10113 | `		pRef->pEntry0 = 0;` |
|      167 | 10114 | `		SyHashDeleteEntry2(pEntry);` |
|       83 | 10115 | `	}` |
|    15727 | 10116 | `	if( pRef->pNode0 ){` |
|      167 | 10117 | `		ph7_hashmap_node *pNode = pRef->pNode0;` |
|      167 | 10118 | `		pRef->pNode0 = 0;` |
|      167 | 10119 | `		PH7_HashmapUnlinkNode(pNode,FALSE);` |
|       83 | 10120 | `	}` |
|    15727 | 10121 | `	pSpill = pRef->pSpill;` |
|    15727 | 10122 | `	if( pSpill ){` |
|      573 | 10123 | `		for( n = 0 ; n < SySetUsed(&pSpill->aReference) ; n++ ){` |
|      171 | 10124 | `			SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pSpill->aReference);` |
|      171 | 10125 | `			SyHashEntry *pEntry = apEntry[n];` |
|      171 | 10126 | `			if( pEntry ){` |
|      ! 0 | 10127 | `				apEntry[n] = 0;` |
|      ! 0 | 10128 | `				SyHashDeleteEntry2(pEntry);` |
|      ! 0 | 10129 | `			}` |
|       87 | 10130 | `		}` |
|      707 | 10131 | `		for(n = 0 ; n < SySetUsed(&pSpill->aArrEntries) ; ++n ){` |
|      304 | 10132 | `			ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pSpill->aArrEntries);` |
|      304 | 10133 | `			ph7_hashmap_node *pNode = apNode[n];` |
|      304 | 10134 | `			if( pNode ){` |
|      ! 0 | 10135 | `				apNode[n] = 0;` |
|      ! 0 | 10136 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|      ! 0 | 10137 | `			}` |
|      153 | 10138 | `		}` |
|      405 | 10139 | `		SySetRelease(&pRef->pSpill->aReference);` |
|      405 | 10140 | `		SySetRelease(&pRef->pSpill->aArrEntries);` |
|      405 | 10141 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRef->pSpill);` |
|      405 | 10142 | `		pRef->pSpill = 0;` |
|      201 | 10143 | `	}` |
|    15727 | 10144 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|     8062 | 10145 | `}` |
|        - | 10146 | `/*` |
|        - | 10147 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10148 | ` *` |
|        - | 10149 | ` * iFlags is applied only when the slot has NOTHING registered against it yet; a slot` |
|        - | 10150 | ` * already in the table keeps the flags it has (VmPinMemObjSlot is the door that adds` |
|        - | 10151 | ` * one). That has always been the rule -- it is now spelled out because the word for a` |
|        - | 10152 | ` * fresh slot is chosen from it.` |
|        - | 10153 | ` *` |
|        - | 10154 | ` * The implementation of the reference mechanism in the PH7 engine differ greatly from` |
|        - | 10155 | ` * the one used by the zend engine. That is, the reference implementation is` |
|        - | 10156 | ` * consistent,solid and it's behavior resemble the C++ reference mechanism.` |
|        - | 10157 | ` */` |
| 24190722 | 10158 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10159 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10160 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10161 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10162 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10163 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10164 | `	)` |
|        5 | 10165 | `{` |
|        - | 10166 | `	VmFrame *pFrame;` |
|        - | 10167 | `	VmRefObj *pRef;` |
|        - | 10168 | `	void *pWord;` |
|        - | 10169 | `	/* Cover the slot up front: everything below commits without a way to fail, and a` |
|        - | 10170 | `	 * table that cannot be grown records nothing at all -- the same degradation a` |
|        - | 10171 | `	 * failed record allocation has always produced. */` |
| 24190727 | 10172 | `	if( VmRefTableGrow(&(*pVm),nIdx) != SXRET_OK ){` |
|      ! 0 | 10173 | `		return SXERR_MEM;` |
|        - | 10174 | `	}` |
| 24190727 | 10175 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
| 24190727 | 10176 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10177 | `		VmSlot sRef;` |
|        - | 10178 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10179 | `		 * be deleted when we leave this frame.` |
|        - | 10180 | `		 */` |
|  1369368 | 10181 | `		sRef.nIdx = nIdx;` |
|  1369368 | 10182 | `		sRef.pUserData = pEntry;` |
|  1369368 | 10183 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10184 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10185 | `		}` |
|   687561 | 10186 | `	}` |
| 24190727 | 10187 | `	pWord = pVm->apRefObj[nIdx];` |
| 24190727 | 10188 | `	if( pWord == 0 ){` |
|        - | 10189 | `		/* A slot nothing has claimed yet. This is the common case by three orders of` |
|        - | 10190 | `		 * magnitude, and every shape of it fits in the word. */` |
| 24089432 | 10191 | `		if( iFlags == 0 && pEntry != 0 && pMapEntry == 0 && VmRefTaggable(pEntry) ){` |
|  1435622 | 10192 | `			VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pEntry)[VM_REF_TAG_NAME]);` |
|  1435622 | 10193 | `			return SXRET_OK;` |
|        - | 10194 | `		}` |
| 22653815 | 10195 | `		if( iFlags == 0 && pMapEntry != 0 && pEntry == 0 && VmRefTaggable(pMapEntry) ){` |
| 12188751 | 10196 | `			VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pMapEntry)[VM_REF_TAG_NODE]);` |
| 12188751 | 10197 | `			return SXRET_OK;` |
|        - | 10198 | `		}` |
| 10465069 | 10199 | `		if( pEntry == 0 && pMapEntry == 0 && (iFlags & ~VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10200 | `			/* A pin with no named holder -- every declared property's own hold on its` |
|        - | 10201 | `			 * value slot -- and the empty registration a dropped holder leaves behind. */` |
| 10465069 | 10202 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,iFlags));` |
| 10465069 | 10203 | `			return SXRET_OK;` |
|        - | 10204 | `		}` |
|      ! 0 | 10205 | `	}else{` |
|   101300 | 10206 | `		switch( VM_REF_TAGOF(pWord) ){` |
|    36905 | 10207 | `		case VM_REF_TAG_NAME:` |
|        - | 10208 | `			/* A name RE-BOUND to the slot it already names changes nothing, which is` |
|        - | 10209 | `			 * the frame-local variable written to in a loop. */` |
|    73689 | 10210 | `			if( pMapEntry == 0` |
|    36818 | 10211 | `			 && (pEntry == 0 \|\| VM_REF_UNTAG(pWord,VM_REF_TAG_NAME) == (void *)pEntry) ){` |
|      ! 0 | 10212 | `				return SXRET_OK;` |
|        - | 10213 | `			}` |
|    73694 | 10214 | `			break;` |
|    11989 | 10215 | `		case VM_REF_TAG_NODE:` |
|    23906 | 10216 | `			if( pEntry == 0` |
|    12034 | 10217 | `			 && (pMapEntry == 0 \|\| VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pMapEntry) ){` |
|      ! 0 | 10218 | `				return SXRET_OK;` |
|        - | 10219 | `			}` |
|    23911 | 10220 | `			break;` |
|       89 | 10221 | `		case VM_REF_TAG_MARK:` |
|      182 | 10222 | `			if( pEntry == 0 && pMapEntry == 0 ){` |
|      ! 0 | 10223 | `				return SXRET_OK; /* iFlags is ignored on a slot already registered */` |
|        - | 10224 | `			}` |
|      182 | 10225 | `			if( SX_PTR_TO_INT(pWord) == VM_REF_TAG_MARK ){` |
|        - | 10226 | `				/* A bare mark says "registered, held by nothing"; a holder on top of it` |
|        - | 10227 | `				 * says exactly what the pointer words say. */` |
|      ! 0 | 10228 | `				if( pMapEntry == 0 && VmRefTaggable(pEntry) ){` |
|      ! 0 | 10229 | `					VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pEntry)[VM_REF_TAG_NAME]);` |
|      ! 0 | 10230 | `					return SXRET_OK;` |
|        - | 10231 | `				}` |
|      ! 0 | 10232 | `				if( pEntry == 0 && VmRefTaggable(pMapEntry) ){` |
|      ! 0 | 10233 | `					VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pMapEntry)[VM_REF_TAG_NODE]);` |
|      ! 0 | 10234 | `					return SXRET_OK;` |
|        - | 10235 | `				}` |
|      ! 0 | 10236 | `			}` |
|      178 | 10237 | `			break;` |
|     1763 | 10238 | `		default:` |
|     3522 | 10239 | `			break;` |
|        - | 10240 | `		}` |
|        - | 10241 | `	}` |
|        - | 10242 | `	/* More than one word can say. */` |
|   101300 | 10243 | `	pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|   101300 | 10244 | `	if( pRef == 0 ){` |
|      ! 0 | 10245 | `		return SXERR_MEM;` |
|        - | 10246 | `	}` |
|   101300 | 10247 | `	if( pWord == 0 ){` |
|      ! 0 | 10248 | `		pRef->iFlags = iFlags;` |
|      ! 0 | 10249 | `	}` |
|   101300 | 10250 | `	if( pEntry ){` |
|        - | 10251 | `		/* The name bound to this slot */` |
|    26905 | 10252 | `		VmRefAddEntry(&(*pVm),pRef,pEntry);` |
|    13412 | 10253 | `	}` |
|   101300 | 10254 | `	if( pMapEntry ){` |
|        - | 10255 | `		/* The hashmap node [i.e: Array entry] pointing at it */` |
|    74400 | 10256 | `		VmRefAddNode(&(*pVm),pRef,pMapEntry);` |
|    37137 | 10257 | `	}` |
|   101300 | 10258 | `	return SXRET_OK;` |
| 12093994 | 10259 | `}` |
|        - | 10260 | `/*` |
|        - | 10261 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10262 | ` *` |
|        - | 10263 | ` * The slot stays REGISTERED with no holders (a bare mark) rather than leaving the` |
|        - | 10264 | ` * table: that is what still returns the index to the free pool when the value goes.` |
|        - | 10265 | ` */` |
| 13396334 | 10266 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10267 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10268 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10269 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10270 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10271 | `	)` |
|        5 | 10272 | `{` |
|        - | 10273 | `	VmRefObj *pRef;` |
|        - | 10274 | `	void *pWord;` |
| 13396339 | 10275 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
| 13396339 | 10276 | `	if( pWord == 0 ){` |
|        - | 10277 | `		/* Not such entry */` |
|        5 | 10278 | `		return SXERR_NOTFOUND;` |
|        - | 10279 | `	}` |
| 13396335 | 10280 | `	switch( VM_REF_TAGOF(pWord) ){` |
|   677273 | 10281 | `	case VM_REF_TAG_NAME:` |
|  1360314 | 10282 | `		if( pEntry != 0 && VM_REF_UNTAG(pWord,VM_REF_TAG_NAME) == (void *)pEntry ){` |
|  1360314 | 10283 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|   683036 | 10284 | `		}` |
|  1360314 | 10285 | `		return SXRET_OK;` |
|  6002908 | 10286 | `	case VM_REF_TAG_NODE:` |
| 12000092 | 10287 | `		if( pMapEntry != 0 && VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pMapEntry ){` |
| 12000092 | 10288 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|  5997179 | 10289 | `		}` |
| 12000092 | 10290 | `		return SXRET_OK;` |
|      ! 0 | 10291 | `	case VM_REF_TAG_MARK:` |
|      ! 0 | 10292 | `		return SXRET_OK; /* nothing named to drop */` |
|    17979 | 10293 | `	default:` |
|    35934 | 10294 | `		break;` |
|        - | 10295 | `	}` |
|    35939 | 10296 | `	pRef = (VmRefObj *)pWord;` |
|        - | 10297 | `	/* Remove the desired entry */` |
|    35939 | 10298 | `	if( pEntry ){` |
|    19879 | 10299 | `		VmRefDropEntry(pRef,pEntry);` |
|     9930 | 10300 | `	}` |
|    35939 | 10301 | `	if( pMapEntry ){` |
|    16065 | 10302 | `		VmRefDropNode(pRef,pMapEntry);` |
|     8025 | 10303 | `	}` |
|    35939 | 10304 | `	return SXRET_OK;` |
|  6698177 | 10305 | `}` |
|        - | 10306 | `/*` |
|        - | 10307 | `` * Pin a slot past its frame: a `use (&$x)` capture, a static, an enum case, a`` |
|        - | 10308 | ` * reference-bound property. A pin is a holder the table cannot NAME.` |
|        - | 10309 | ` */` |
|     2192 | 10310 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 10311 | `{` |
|        - | 10312 | `	void *pWord;` |
|        - | 10313 | `	VmRefObj *pRef;` |
|     2197 | 10314 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|     2197 | 10315 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|     2197 | 10316 | `	if( pWord != 0 && VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|      154 | 10317 | `		VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(VmRefMarkPin(pWord),VM_REF_IDX_KEEP));` |
|      154 | 10318 | `		return;` |
|        - | 10319 | `	}` |
|     2045 | 10320 | `	if( pWord == 0 ){` |
|        - | 10321 | `		/* No record yet -- a pin on a slot nothing refers to was silently a NO-OP, so the` |
|        - | 10322 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|       64 | 10323 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|       64 | 10324 | `		return;` |
|        - | 10325 | `	}` |
|     1985 | 10326 | `	pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|     1985 | 10327 | `	if( pRef ){` |
|     1985 | 10328 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|      986 | 10329 | `	}` |
|     1097 | 10330 | `}` |
|        - | 10331 | `/*` |
|        - | 10332 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|        - | 10333 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|        - | 10334 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|        - | 10335 | ` */` |
|      208 | 10336 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10337 | `{` |
|        - | 10338 | `	void *pWord;` |
|        - | 10339 | `	VmRefObj *pRef;` |
|      210 | 10340 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|      210 | 10341 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|      210 | 10342 | `	if( pWord != 0 && VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|      156 | 10343 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|      156 | 10344 | `		if( nPin < VM_REF_MARK_PINMAX ){` |
|      156 | 10345 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(nPin + 1,VM_REF_IDX_KEEP));` |
|      156 | 10346 | `			return;` |
|        - | 10347 | `		}` |
|      ! 0 | 10348 | `	}` |
|       55 | 10349 | `	pRef = VmRefFull(&(*pVm),nIdx);` |
|       55 | 10350 | `	if( pRef == 0 && pWord != 0 ){` |
|      ! 0 | 10351 | `		pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|      ! 0 | 10352 | `	}` |
|       55 | 10353 | `	if( pRef ){` |
|       55 | 10354 | `		pRef->nPin++;` |
|       27 | 10355 | `	}` |
|      106 | 10356 | `}` |
|        - | 10357 | `/*` |
|        - | 10358 | ` * Give back a counted pin. The slot goes when it was the last holder -- without this the` |
|        - | 10359 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|        - | 10360 | ` * used to be a flag, so nothing could tell one holder from two).` |
|        - | 10361 | ` */` |
|      198 | 10362 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10363 | `{` |
|      200 | 10364 | `	void *pWord = VmRefWord(&(*pVm),nIdx);` |
|        - | 10365 | `	VmRefObj *pRef;` |
|      200 | 10366 | `	if( pWord == 0 ){` |
|      ! 0 | 10367 | `		return;` |
|        - | 10368 | `	}` |
|      200 | 10369 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|       45 | 10370 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|       45 | 10371 | `		if( nPin < 1 ){` |
|      ! 0 | 10372 | `			return;` |
|        - | 10373 | `		}` |
|       45 | 10374 | `		nPin--;` |
|       45 | 10375 | `		VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(nPin,nPin < 1 ? 0 : VM_REF_IDX_KEEP));` |
|       45 | 10376 | `		if( nPin < 1 ){` |
|       31 | 10377 | `			PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|       15 | 10378 | `		}` |
|       45 | 10379 | `		return;` |
|        - | 10380 | `	}` |
|      156 | 10381 | `	pRef = VmRefFull(&(*pVm),nIdx);` |
|      156 | 10382 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|      ! 0 | 10383 | `		return;` |
|        - | 10384 | `	}` |
|      156 | 10385 | `	pRef->nPin--;` |
|      156 | 10386 | `	if( pRef->nPin < 1 ){` |
|      146 | 10387 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|      146 | 10388 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|       72 | 10389 | `	}` |
|      101 | 10390 | `}` |
|        - | 10391 | `/*` |
|        - | 10392 | ` * Give up the OWNER's own hold on a slot, and say whether anybody else still has one.` |
|        - | 10393 | ` *` |
|        - | 10394 | `` * A property that is released -- with its object, or by `unset($o->p)` -- used to`` |
|        - | 10395 | ` * take its VALUE SLOT with it unconditionally, which is right only while the` |
|        - | 10396 | ` * property is the one thing naming it. php's refcount keeps the value alive for` |
|        - | 10397 | `` * whoever else holds a reference to it (`$r =& $o->p; unset($o->p);` leaves $r`` |
|        - | 10398 | `` * holding the value, and `$a[] =& $o->p` leaves the element), where unlinking the`` |
|        - | 10399 | ` * slot here dropped the array element and left the VARIABLE undefined.` |
|        - | 10400 | ` *` |
|        - | 10401 | ` * Returns TRUE when the caller must NOT free the slot. The owner's own hold is the` |
|        - | 10402 | ` * permanent pin a dynamically created / re-created property carries; a declared one` |
|        - | 10403 | ` * holds nothing at all, so there is nothing to give back for it.` |
|        - | 10404 | ` */` |
|  9757659 | 10405 | `PH7_PRIVATE int PH7_VmSlotDropOwnerHold(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 10406 | `{` |
|        - | 10407 | `	void *pWord;` |
|        - | 10408 | `	VmRefObj *pRef;` |
|  9757664 | 10409 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 10410 | `		return 0;` |
|        - | 10411 | `	}` |
|  9757664 | 10412 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  9757664 | 10413 | `	if( pWord == 0 ){` |
|      ! 0 | 10414 | `		return 0;` |
|        - | 10415 | `	}` |
|  9757664 | 10416 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|  9757646 | 10417 | `		if( VmRefMarkPin(pWord) == 0 ){` |
|  9757646 | 10418 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|  4878228 | 10419 | `		}` |
|  4878242 | 10420 | `	}else if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|       19 | 10421 | `		pRef = (VmRefObj *)pWord;` |
|       19 | 10422 | `		if( pRef->nPin == 0 ){` |
|       19 | 10423 | `			pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|        9 | 10424 | `		}` |
|        9 | 10425 | `	}` |
|        - | 10426 | `	/* A word carrying a name or a node has no pin to give back; its holder is the` |
|        - | 10427 | `	 * one the count below reports. */` |
|  9757664 | 10428 | `	return PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0;` |
|  4878237 | 10429 | `}` |
|        - | 10430 | `/*` |
|        - | 10431 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|        - | 10432 | ` * holds it (php frees the value with the last reference, not with the first one` |
|        - | 10433 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|        - | 10434 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|        - | 10435 | ` */` |
| 12021253 | 10436 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 10437 | `{` |
| 12021258 | 10438 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 10439 | `		return;` |
|        - | 10440 | `	}` |
| 12021253 | 10441 | `	if( nIdx < pVm->nRefSize` |
| 12021258 | 10442 | `	 && SX_PTR_TO_INT(pVm->apRefObj[nIdx]) == VM_REF_TAG_MARK ){` |
|        - | 10443 | `		/* A bare mark (PH7_VmSlotDropIfBare): registered, unpinned, unheld. The three` |
|        - | 10444 | `		 * calls below would load this same word three more times to say so, and this` |
|        - | 10445 | `		 * is 99.2% of the door. Fall through to the release. */` |
|  6018278 | 10446 | `	}else if( PH7_VmSlotRegistered(&(*pVm),nIdx) ){` |
|    20977 | 10447 | `		if( PH7_VmSlotKeepPinned(&(*pVm),nIdx) ){` |
|      128 | 10448 | `			return; /* pinned past its frame — its holder is not in the table */` |
|        - | 10449 | `		}` |
|    20851 | 10450 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|     5369 | 10451 | `			return; /* somebody still holds it */` |
|        - | 10452 | `		}` |
|     7735 | 10453 | `	}` |
|        - | 10454 | `	/* Nothing registered at all means nothing was ever recorded against the slot,` |
|        - | 10455 | `	 * which is the same answer as a count of zero — release it (this is what every` |
|        - | 10456 | `	 * caller did unconditionally before the holder rule). */` |
| 12015766 | 10457 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - | 10458 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|        - | 10459 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
| 12015766 | 10460 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|  6007762 | 10461 | `}` |
|        - | 10462 | `/*` |
|        - | 10463 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|        - | 10464 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|        - | 10465 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|        - | 10466 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|        - | 10467 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|        - | 10468 | ` */` |
|    15674 | 10469 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|        5 | 10470 | `{` |
|        - | 10471 | `	VmFrame *pFrame;` |
|    37935 | 10472 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|    22261 | 10473 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    22261 | 10474 | `		sxu32 n = 0;` |
|    43903 | 10475 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|    21644 | 10476 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|        - | 10477 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|        - | 10478 | `				 * same index — it now holds the row swapped in from the tail. */` |
|     4852 | 10479 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|     4852 | 10480 | `				(void)SySetPop(&pFrame->sRef);` |
|     4852 | 10481 | `				continue;` |
|        - | 10482 | `			}` |
|    16794 | 10483 | `			n++;` |
|        2 | 10484 | `		}` |
|    11128 | 10485 | `	}` |
|    15679 | 10486 | `}` |
|        - | 10487 | `/*` |
|        - | 10488 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|        - | 10489 | ` *` |
|        - | 10490 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|        - | 10491 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|        - | 10492 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|        - | 10493 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|        - | 10494 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|        - | 10495 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|        - | 10496 | ` */` |
|    31919 | 10497 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|        5 | 10498 | `{` |
|    31924 | 10499 | `	return nByte > 0 && zName[0] == '[';` |
|        5 | 10500 | `}` |
|        - | 10501 | `/*` |
|        - | 10502 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|        - | 10503 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|        - | 10504 | ` * its value variable on every step, and it goes through the reference table like any` |
|        - | 10505 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|        - | 10506 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|        - | 10507 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|        - | 10508 | ` */` |
|     2948 | 10509 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|        - | 10510 | `	sxu32 nIdx)` |
|        5 | 10511 | `{` |
|     2953 | 10512 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|     2953 | 10513 | `	if( pEntry ){` |
|      116 | 10514 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|      116 | 10515 | `		return;` |
|        - | 10516 | `	}` |
|     2841 | 10517 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|      ! 0 | 10518 | `		return;` |
|        - | 10519 | `	}` |
|        - | 10520 | `	/* The name may already be memoized against the slot it had before this frame` |
|        - | 10521 | ``	 * installed it -- a by-reference `foreach` re-binds its value variable on every`` |
|        - | 10522 | `	 * step, and the first step is an INSERT. */` |
|     2841 | 10523 | `	VmVarMemoFlush(pFrame);` |
|     2841 | 10524 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|        - | 10525 | `		/* A global is also an entry of the $GLOBALS view */` |
|       42 | 10526 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       19 | 10527 | `	}` |
|     2841 | 10528 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|     1477 | 10529 | `}` |
|        - | 10530 | `/*` |
|        - | 10531 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|        - | 10532 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|        - | 10533 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|        - | 10534 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|        - | 10535 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|        - | 10536 | ` * and left the OLD binding standing, so every later write through the name went to` |
|        - | 10537 | ` * the wrong variable.` |
|        - | 10538 | ` *` |
|        - | 10539 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|        - | 10540 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|        - | 10541 | ` * compares it by identity.` |
|        - | 10542 | ` */` |
|     4840 | 10543 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|        - | 10544 | `	ph7_vm *pVm,          /* Target VM */` |
|        - | 10545 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|        - | 10546 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|        - | 10547 | `	const char *zName,    /* Variable name */` |
|        - | 10548 | `	sxu32 nByte,          /* Name length */` |
|        - | 10549 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|        - | 10550 | `	)` |
|        5 | 10551 | `{` |
|     4845 | 10552 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|     4845 | 10553 | `	if( nOld == nIdx ){` |
|        - | 10554 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|        5 | 10555 | `		return;` |
|        - | 10556 | `	}` |
|        - | 10557 | `	/* This name now means another slot; every memo this frame holds goes. */` |
|     4840 | 10558 | `	VmVarMemoFlush(pFrame);` |
|        - | 10559 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|        - | 10560 | `	 * "release this reference at exit" set */` |
|     4840 | 10561 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|     4840 | 10562 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|     4840 | 10563 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|     4840 | 10564 | `	if( pFrame->pParent == 0 ){` |
|        - | 10565 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|        - | 10566 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|        - | 10567 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|        - | 10568 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|      150 | 10569 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|      146 | 10570 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|      150 | 10571 | `		 && pGlobalNode ){` |
|      150 | 10572 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|      150 | 10573 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|      150 | 10574 | `				pGlobalNode->nValIdx = nIdx;` |
|      150 | 10575 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|       73 | 10576 | `			}` |
|       77 | 10577 | `		}else{` |
|        - | 10578 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|      ! 0 | 10579 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        - | 10580 | `		}` |
|       73 | 10581 | `	}` |
|     4840 | 10582 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|        - | 10583 | `	/* The old value dies with its last holder — and only then */` |
|     4840 | 10584 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|     2425 | 10585 | `}` |
|        - | 10586 | `/*` |
|        - | 10587 | ` * Is there a SCHEME at the front of this name, and how long is it?` |
|        - | 10588 | ` *` |
|        - | 10589 | ` * php reads one only at the START, and only as a URL scheme: a run of` |
|        - | 10590 | ` * [A-Za-z0-9+.-] at least TWO characters long, followed immediately by "://".` |
|        - | 10591 | ` * PHL used to hunt for the first "://" ANYWHERE in the name and then trim` |
|        - | 10592 | ` * whitespace off whatever preceded it, which made four ordinary FILENAMES into` |
|        - | 10593 | ` * URLs: " php://memory" and "php ://memory" opened the memory stream php opens` |
|        - | 10594 | ` * a file called that, "./sub://z" and "a b://c" were looked up as schemes` |
|        - | 10595 | ` * "./sub" and "a b". The two-character minimum is php's, and it is what keeps a` |
|        - | 10596 | ` * Windows drive letter ("C://tmp") a path rather than a "C" scheme.` |
|        - | 10597 | ` */` |
|   296103 | 10598 | `static int VmUrlScheme(const char *zIn,int nByte,int *pnScheme)` |
|        5 | 10599 | `{` |
|   296108 | 10600 | `	int i = 0;` |
|  1612391 | 10601 | `	while( i < nByte ){` |
|  1612219 | 10602 | `		int c = zIn[i];` |
|  1612214 | 10603 | `		if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9')` |
|   297745 | 10604 | `		 \|\| c == '+' \|\| c == '-' \|\| c == '.' ){` |
|  1316288 | 10605 | `			i++;` |
|  1316288 | 10606 | `			continue;` |
|        - | 10607 | `		}` |
|   295936 | 10608 | `		break;` |
|      ! 0 | 10609 | `	}` |
|        - | 10610 | `	/* php also accepts a scheme with NOTHING after it ("zzz://" is an unknown` |
|        - | 10611 | `	 * wrapper, not a file called "zzz://"), which the old scan refused. */` |
|   296108 | 10612 | `	if( i > 1 && i + 2 < nByte && zIn[i] == ':' && zIn[i+1] == '/' && zIn[i+2] == '/' ){` |
|     2721 | 10613 | `		*pnScheme = i;` |
|     2721 | 10614 | `		return 1;` |
|        - | 10615 | `	}` |
|   293392 | 10616 | `	return 0;` |
|   148089 | 10617 | `}` |
|        - | 10618 | `/*` |
|        - | 10619 | ` * The same question from outside vm.c: how long is the scheme, or 0 for a name` |
|        - | 10620 | ` * that has none. stream_resolve_include_path() asks it to decide whether a name` |
|        - | 10621 | ` * is walkable at all.` |
|        - | 10622 | ` */` |
|       82 | 10623 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte)` |
|        3 | 10624 | `{` |
|       85 | 10625 | `	int nScheme = 0;` |
|       85 | 10626 | `	if( zIn == 0 ){` |
|      ! 0 | 10627 | `		return 0;` |
|        - | 10628 | `	}` |
|       85 | 10629 | `	if( nByte < 0 ){` |
|       55 | 10630 | `		nByte = (int)SyStrlen(zIn);` |
|       22 | 10631 | `	}` |
|       85 | 10632 | `	return VmUrlScheme(zIn,nByte,&nScheme) ? nScheme : 0;` |
|       40 | 10633 | `}` |
|        - | 10634 | `/*` |
|        - | 10635 | ` * The bytes the FILE wrapper is handed for a file:// URL.` |
|        - | 10636 | ` *` |
|        - | 10637 | ` * A file:// URL has an AUTHORITY, and php only accepts two of them: an empty` |
|        - | 10638 | `` * one and `localhost` (case-insensitively, and only with its slash). Anything`` |
|        - | 10639 | ` * else is a remote host it refuses to reach -- where PHL stripped exactly` |
|        - | 10640 | `` * "file://" and opened whatever was left, so `file://tmp/passwd` silently read`` |
|        - | 10641 | ` * the RELATIVE path tmp/passwd. What survives the strip is the LAST slash of` |
|        - | 10642 | `` * the leading run, so `file:////x` is /x, `file://localhost//x` is /x, and`` |
|        - | 10643 | `` * `file://` on its own is the root directory.`` |
|        - | 10644 | ` *` |
|        - | 10645 | ` * Returns 0 for an authority php will not reach; the caller answers "no` |
|        - | 10646 | ` * wrapper", as php does.` |
|        - | 10647 | ` */` |
|      156 | 10648 | `static int VmFileUrlPath(const char *zIn,int nByte,int nScheme,const char **pzPath)` |
|        4 | 10649 | `{` |
|        - | 10650 | `	static const char zLocal[] = "file://localhost/";` |
|      160 | 10651 | `	const char *zPath = &zIn[nScheme+1]; /* the first slash of "://" */` |
|      160 | 10652 | `	const char *zEnd = &zIn[nByte];` |
|      156 | 10653 | `	if( nScheme + 3 < nByte && zIn[nScheme+3] != '/'` |
|        - | 10654 | `#ifdef __WINNT__` |
|        - | 10655 | ``	 /* php's own Windows allowance: `file://C:/x` is a DRIVE, not a host. */`` |
|        4 | 10656 | `	 && !(nScheme + 4 < nByte && zIn[nScheme+4] == ':')` |
|        - | 10657 | `#endif` |
|        - | 10658 | `	){` |
|       94 | 10659 | `		if( nByte < (int)sizeof(zLocal)-1` |
|       97 | 10660 | `		 \|\| SyStrnicmp(zIn,zLocal,(sxu32)sizeof(zLocal)-1) != 0 ){` |
|       93 | 10661 | `			return 0; /* a host this build (and php) will not fetch from */` |
|        - | 10662 | `		}` |
|        4 | 10663 | `		zPath = &zIn[nScheme+3+sizeof("localhost")-1];` |
|        2 | 10664 | `	}` |
|      191 | 10665 | `	while( &zPath[1] < zEnd && zPath[1] == '/' ){` |
|      125 | 10666 | `		zPath++;` |
|        3 | 10667 | `	}` |
|       69 | 10668 | `	*pzPath = zPath;` |
|       69 | 10669 | `	return 1;` |
|       82 | 10670 | `}` |
|        - | 10671 | `/*` |
|        - | 10672 | ` * The same rule for the VFS side, which stats and unlinks a name without ever` |
|        - | 10673 | ` * going through a stream device. It carried a second, shorter copy of the` |
|        - | 10674 | `` * strip -- no slash-run collapse and nothing for a bare `file://` -- so`` |
|        - | 10675 | `` * `is_dir('file://')` was false where php names the root. A host this build`` |
|        - | 10676 | ` * will not reach is handed back UNCHANGED: the syscall then fails on a name` |
|        - | 10677 | ` * that is not a path, which is the FALSE php answers for it.` |
|        - | 10678 | ` */` |
|    88870 | 10679 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath)` |
|        5 | 10680 | `{` |
|        - | 10681 | `	const char *zOut;` |
|    88875 | 10682 | `	int nByte,nScheme = 0;` |
|    88875 | 10683 | `	if( zPath == 0 ){` |
|      ! 0 | 10684 | `		return zPath;` |
|        - | 10685 | `	}` |
|    88875 | 10686 | `	nByte = (int)SyStrlen(zPath);` |
|    88870 | 10687 | `	if( !VmUrlScheme(zPath,nByte,&nScheme)` |
|    44647 | 10688 | `	 \|\| nScheme != (int)sizeof("file")-1` |
|       78 | 10689 | `	 \|\| SyStrnicmp(zPath,"file",sizeof("file")-1) != 0 ){` |
|    88855 | 10690 | `		return zPath;` |
|        - | 10691 | `	}` |
|       22 | 10692 | `	if( !VmFileUrlPath(zPath,nByte,nScheme,&zOut) ){` |
|        3 | 10693 | `		return zPath;` |
|        - | 10694 | `	}` |
|        - | 10695 | `#ifdef __WINNT__` |
|        - | 10696 | `	/* The one piece that is only true here: the leading slash php's own strip` |
|        - | 10697 | `	 * leaves in front of a DRIVE is not part of a Windows path, so` |
|        - | 10698 | `	 * file:///C:/x and file://localhost/C:/x both name C:/x. */` |
|        2 | 10699 | `	if( zOut[0] == '/' && zOut[1] != 0 && zOut[2] == ':' ){` |
|        2 | 10700 | `		zOut++;` |
|        - | 10701 | `	}` |
|        - | 10702 | `#endif` |
|       20 | 10703 | `	return zOut;` |
|    44594 | 10704 | `}` |
|        - | 10705 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10706 | `/*` |
|        - | 10707 | ` * Has a script taken this device out of service with stream_wrapper_unregister()?` |
|        - | 10708 | ` */` |
|   207945 | 10709 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|        5 | 10710 | `{` |
|   207950 | 10711 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|        - | 10712 | `	sxu32 n;` |
|   210322 | 10713 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|     2432 | 10714 | `		if( apOff[n] == pStream ){` |
|       58 | 10715 | `			return 1;` |
|        - | 10716 | `		}` |
|     1190 | 10717 | `	}` |
|   207894 | 10718 | `	return 0;` |
|   103855 | 10719 | `}` |
|        - | 10720 | `/*` |
|        - | 10721 | ` * The device currently answering to a scheme name, or NULL. The scan runs to` |
|        - | 10722 | ` * the END rather than stopping at the first hit: once a built-in has been` |
|        - | 10723 | ` * unregistered a userland wrapper can be registered under the same name, both` |
|        - | 10724 | ` * sit in the list, and the LIVE one is the later of the two.` |
|        - | 10725 | ` */` |
|   206921 | 10726 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName)` |
|        5 | 10727 | `{` |
|   206926 | 10728 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|   206926 | 10729 | `	ph7_io_stream *pHit = 0;` |
|   206926 | 10730 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|   206926 | 10731 | `	if( nName < 0 ){` |
|      ! 0 | 10732 | `		nName = (int)SyStrlen(zName);` |
|      ! 0 | 10733 | `	}` |
|  2279578 | 10734 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|  2072657 | 10735 | `		ph7_io_stream *pStream = apStream[n];` |
|  2072652 | 10736 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|  1551603 | 10737 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|  1865876 | 10738 | `			continue;` |
|        - | 10739 | `		}` |
|   206786 | 10740 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|       19 | 10741 | `			continue;` |
|        - | 10742 | `		}` |
|   206768 | 10743 | `		pHit = pStream;` |
|   103269 | 10744 | `	}` |
|   206926 | 10745 | `	return pHit;` |
|        5 | 10746 | `}` |
|        - | 10747 | `/*` |
|        - | 10748 | ` * Is this scheme one the build HAS but the script has switched off? php words` |
|        - | 10749 | ` * that differently from a scheme nothing was ever registered under -- but only` |
|        - | 10750 | ` * for file://, whose plain-files fallback is the branch that reports it.` |
|        - | 10751 | ` */` |
|       90 | 10752 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName)` |
|        4 | 10753 | `{` |
|       94 | 10754 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       94 | 10755 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|       94 | 10756 | `	int bSeen = 0;` |
|       94 | 10757 | `	if( nName < 0 ){` |
|      ! 0 | 10758 | `		nName = (int)SyStrlen(zName);` |
|      ! 0 | 10759 | `	}` |
|      820 | 10760 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|      818 | 10761 | `		ph7_io_stream *pStream = apStream[n];` |
|      814 | 10762 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|      636 | 10763 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|      728 | 10764 | `			continue;` |
|        - | 10765 | `		}` |
|       94 | 10766 | `		if( !PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|       92 | 10767 | `			return 0; /* it is live */` |
|        - | 10768 | `		}` |
|        3 | 10769 | `		bSeen = 1;` |
|        2 | 10770 | `	}` |
|        3 | 10771 | `	return bSeen;` |
|       49 | 10772 | `}` |
|        - | 10773 | `/*` |
|        - | 10774 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10775 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10776 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10777 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10778 | ` * For more information on how to register IO stream devices,please` |
|        - | 10779 | ` * refer to the official documentation.` |
|        - | 10780 | ` */` |
|   206877 | 10781 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10782 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10783 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10784 | `	int nByte              /* *pzDevice length*/` |
|        - | 10785 | `	)` |
|        5 | 10786 | `{` |
|        - | 10787 | `	const char *zIn,*zNext;` |
|        - | 10788 | `	ph7_io_stream *pStream;` |
|   206882 | 10789 | `	int nScheme = 0;` |
|        - | 10790 | `	/* A failed open names the URI the SCRIPT wrote, and every caller from here` |
|        - | 10791 | `	 * on holds only what is left after the scheme -- so both halves are` |
|        - | 10792 | `	 * remembered as the scheme comes off, and forgotten on every arm that does` |
|        - | 10793 | `	 * not take one off (see VfsThrowOpenWarning). */` |
|   206882 | 10794 | `	if( pVm->nOpenDepth < 1 ){` |
|   206734 | 10795 | `		pVm->zOpenUri = 0;` |
|   206734 | 10796 | `		pVm->zOpenUriTail = 0;` |
|   206734 | 10797 | `		pVm->nOpenUri = 0;` |
|        - | 10798 | `		/* The reason goes with them: a caller that resolves a device and then` |
|        - | 10799 | `		 * declines to open it (dom asks for xRead/xWrite first) must not report` |
|        - | 10800 | `		 * the PREVIOUS open's wrapper reason. */` |
|   206734 | 10801 | `		pVm->zOpenErr = 0;` |
|   103247 | 10802 | `	}` |
|        - | 10803 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|   206882 | 10804 | `	zIn = *pzDevice;` |
|   206882 | 10805 | `	if( !VmUrlScheme(zIn,nByte,&nScheme) ){` |
|        - | 10806 | `		/* No scheme: php's default is the plain-files wrapper, and it is the` |
|        - | 10807 | `		 * SAME slot file:// names -- so a script that unregisters file:// loses` |
|        - | 10808 | `		 * the bare-path open too, and one that registers its own wrapper over` |
|        - | 10809 | `		 * file:// gets bare paths routed through it. Looking the name up rather` |
|        - | 10810 | `		 * than answering pDefStream is what makes both true. */` |
|   204562 | 10811 | `		return PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|        - | 10812 | `	}` |
|     2325 | 10813 | `	zNext = &zIn[nScheme+sizeof("://")-1];` |
|        - | 10814 | `	/* php applies the file:// authority rules by the SCHEME NAME, before it` |
|        - | 10815 | `	 * cares who is registered under it -- a userland wrapper that replaced` |
|        - | 10816 | `	 * file:// is handed the stripped path too. */` |
|     2325 | 10817 | `	if( nScheme == (int)sizeof("file")-1 && SyStrnicmp(zIn,"file",sizeof("file")-1) == 0 ){` |
|       96 | 10818 | `		if( !VmFileUrlPath(zIn,nByte,nScheme,&zNext) ){` |
|       47 | 10819 | `			return 0;` |
|        - | 10820 | `		}` |
|       24 | 10821 | `	}` |
|     2281 | 10822 | `	pStream = PH7_VmFindStreamDevice(pVm,zIn,nScheme);` |
|     2281 | 10823 | `	if( pStream == 0 ){` |
|        - | 10824 | `		/* No such stream -- or one a script has taken out of service. */` |
|      161 | 10825 | `		return 0;` |
|        - | 10826 | `	}` |
|     2125 | 10827 | `	*pzDevice = zNext;` |
|     2125 | 10828 | `	if( pVm->nOpenDepth < 1 ){` |
|     2123 | 10829 | `		pVm->zOpenUri = zIn;` |
|     2123 | 10830 | `		pVm->nOpenUri = nByte;` |
|     2123 | 10831 | `		pVm->zOpenUriTail = zNext;` |
|     1042 | 10832 | `	}` |
|     2125 | 10833 | `	return pStream;` |
|   103326 | 10834 | `}` |
|        - | 10835 | `/*` |
|        - | 10836 | ` * Why did PH7_VmGetStreamDevice() answer nothing for this name? php raises a` |
|        - | 10837 | ` * REASON of its own before the operation's own failure, and the two a caller` |
|        - | 10838 | ` * can hit here are different sentences. Re-derived from the name rather than` |
|        - | 10839 | ` * threaded out of the lookup, so every call site stays one line.` |
|        - | 10840 | ` *` |
|        - | 10841 | ` * Answers TRUE for the file:// authority php will not reach; otherwise FALSE` |
|        - | 10842 | ` * with *pnScheme set to the length of the scheme that has no wrapper.` |
|        - | 10843 | ` */` |
|      274 | 10844 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme)` |
|        5 | 10845 | `{` |
|        - | 10846 | `	const char *zPath;` |
|      279 | 10847 | `	int nScheme = 0;` |
|      279 | 10848 | `	if( nByte < 0 ){` |
|      151 | 10849 | `		nByte = (int)SyStrlen(zUri);` |
|       73 | 10850 | `	}` |
|      279 | 10851 | `	if( !VmUrlScheme(zUri,nByte,&nScheme) ){` |
|        5 | 10852 | `		*pnScheme = 0;` |
|        5 | 10853 | `		return 0;` |
|        - | 10854 | `	}` |
|      275 | 10855 | `	*pnScheme = nScheme;` |
|      299 | 10856 | `	return nScheme == (int)sizeof("file")-1` |
|      159 | 10857 | `		&& SyStrnicmp(zUri,"file",sizeof("file")-1) == 0` |
|      294 | 10858 | `		&& !VmFileUrlPath(zUri,nByte,nScheme,&zPath);` |
|      142 | 10859 | `}` |
|        - | 10860 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10861 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10862 |  |
