# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6694/8569 lines (78.12%)

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
|        - |    12 | `/* Signed 64-bit multiplication with overflow detection. GCC/Clang expose` |
|        - |    13 | ` * __builtin_mul_overflow; MSVC does not, so we fall back to a portable` |
|        - |    14 | ` * UB-free bound-check implementation. Sets *pR to the wrapped product and` |
|        - |    15 | ` * returns non-zero on overflow. The fallback never divides a potentially-` |
|        - |    16 | ` * overflowed intermediate: all divisions are of compile-time constants` |
|        - |    17 | ` * (LARGEST_INT64/SMALLEST_INT64) by a factor already proven not to be -1,` |
|        - |    18 | ` * and the product itself is computed via unsigned multiplication to avoid` |
|        - |    19 | ` * signed-overflow UB. */` |
|        - |    20 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|        - |    21 | `#define VmMulOverflow64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|        - |    22 | `#else` |
|        - |    23 | `static int VmMulOverflow64(sxi64 a, sxi64 b, sxi64 *pR)` |
|        1 |    24 |  |
|        1 |    25 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        - |    26 | `	/* Factors of 0 or ±1 never overflow; handle up front so the divisions` |
|        - |    27 | `	 * below are guaranteed safe (no SMALLEST_INT64 / -1, no /0). */` |
|        1 |    28 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        1 |    29 | `		return 0;` |
|        - |    30 | `	}` |
|        1 |    31 | `	if( a == -1 ){` |
|      ! 0 |    32 | `		return b == SMALLEST_INT64;` |
|        - |    33 | `	}` |
|        1 |    34 | `	if( b == -1 ){` |
|      ! 0 |    35 | `		return a == SMALLEST_INT64;` |
|        - |    36 | `	}` |
|        - |    37 | `	/* \|a\|,\|b\| >= 2 and neither is -1.  Bound check against the MAX/MIN` |
|        - |    38 | `	 * thresholds.  No division by -1 is possible here, and the quotients` |
|        - |    39 | `	 * of compile-time constants by {a,b} always fit in sxi64. */` |
|        1 |    40 | `	if( a > 0 ){` |
|        1 |    41 | `		if( b > 0 ){` |
|        1 |    42 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |    43 | `		}else{` |
|      ! 0 |    44 | `			return b < SMALLEST_INT64 / a;` |
|        - |    45 | `		}` |
|      ! 0 |    46 | `	}else{` |
|        1 |    47 | `		if( b > 0 ){` |
|        1 |    48 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |    49 | `		}else{` |
|        1 |    50 | `			return b < LARGEST_INT64 / a;` |
|        - |    51 | `		}` |
|        - |    52 | `	}` |
|        1 |    53 |  |
|        - |    54 | `#endif` |
|        - |    55 | `/*` |
|        - |    56 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    57 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    58 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    59 | ` * statements.` |
|        - |    60 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    61 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    62 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    63 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    64 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    65 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    66 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    67 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    68 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    69 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    70 | ` * and so on.` |
|        - |    71 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    72 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    73 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    74 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    75 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    76 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    77 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    78 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    79 | ` */` |
|        - |    80 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |    81 | `/*` |
|        - |    82 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    83 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    84 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    85 | ` * a new memory object.` |
|        - |    86 | ` */` |
|        - |    87 | `typedef struct VmSlot VmSlot;` |
|        - |    88 | `struct VmSlot` |
|        - |    89 |  |
|        - |    90 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    91 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    92 | `};` |
|        - |    93 | `/*` |
|        - |    94 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    95 | ` * follwoing table.` |
|        - |    96 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    97 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    98 | ` * the reference implementation is consistent,solid and it's` |
|        - |    99 | ` * behavior resemble the C++ reference mechanism.` |
|        - |   100 | ` * Refer to the official for more information on this powerful` |
|        - |   101 | ` * extension.` |
|        - |   102 | ` */` |
|        - |   103 | `struct VmRefObj` |
|        - |   104 |  |
|        - |   105 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |   106 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |   107 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |   108 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |   109 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |   110 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |   111 | `};` |
|        - |   112 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |   113 | `/* VmObEntry struct moved to ph7int.h */` |
|        - |   114 | `/*` |
|        - |   115 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |   116 | ` * is stored in an instance of the following structure.` |
|        - |   117 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   118 | ` */` |
|        - |   119 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   120 | `struct VmShutdownCB` |
|        - |   121 |  |
|        - |   122 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   123 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   124 | `	int nArg;             /* Total number of given arguments */` |
|        - |   125 | `};` |
|        - |   126 | `/*` |
|        - |   127 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |   128 | ` * is stored in an instance of the following structure.` |
|        - |   129 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|        - |   132 | `struct VmAutoloadCB` |
|        - |   133 |  |
|        - |   134 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|        - |   135 | `};` |
|        - |   136 |  |
|        - |   137 | `/*` |
|        - |   138 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   139 | ` */` |
|   917164 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   917166 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   917132 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   917122 |   148 | `	return FALSE;` |
|   458606 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335464 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335466 |   162 | `	sxu8 bReal = FALSE;` |
|   335466 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335466 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335396 |   166 | `		return FALSE;` |
|        - |   167 | `	}` |
|       71 |   168 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       71 |   169 | `	if( sStr.nByte == 0 ){` |
|        5 |   170 | `		return TRUE;` |
|        - |   171 | `	}` |
|       67 |   172 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       47 |   173 | `		return TRUE;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   176 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   177 | `	 * still takes the Perl path. */` |
|       21 |   178 | `	zEnd = sStr.zString + sStr.nByte;` |
|       25 |   179 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   180 | `		zTail++;` |
|        1 |   181 | `	}` |
|       21 |   182 | `	return zTail < zEnd;` |
|   167756 |   183 |  |
|        - |   184 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   185 | `/* Constant expander used by define(); used below to recognise user-defined` |
|        - |   186 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|        - |   187 | ` * a define() overwrites them. */` |
|        - |   188 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData);` |
|        - |   189 | `/*` |
|        - |   190 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   191 | ` * it can be expanded from the target PHP program.` |
|        - |   192 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   193 | ` * simple and work as follows:` |
|        - |   194 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   195 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   196 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   197 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   198 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   199 | ` * (Windows,Linux,...) and so on.` |
|        - |   200 | ` * Please refer to the official documentation for additional information.` |
|        - |   201 | ` */` |
|   633036 |   202 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   203 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   204 | `	const SyString *pName,  /* Constant name */` |
|        - |   205 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   206 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   207 | `	)` |
|        2 |   208 |  |
|        - |   209 | `	ph7_constant *pCons;` |
|        - |   210 | `	SyHashEntry *pEntry;` |
|        - |   211 | `	char *zDupName;` |
|        - |   212 | `	sxi32 rc;` |
|   633038 |   213 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   633038 |   214 | `	if( pEntry ){` |
|        - |   215 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   216 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |   217 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|        - |   218 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|        - |   219 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|        4 |   220 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|        4 |   221 | `		 && pCons->pUserData != pUserData ){` |
|        3 |   222 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|        3 |   223 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|        1 |   224 | `		}` |
|        6 |   225 | `		pCons->xExpand = xExpand;` |
|        6 |   226 | `		pCons->pUserData = pUserData;` |
|        6 |   227 | `		return SXRET_OK;` |
|        - |   228 | `	}` |
|        - |   229 | `	/* Allocate a new constant instance */` |
|   633034 |   230 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   633034 |   231 | `	if( pCons == 0 ){` |
|      ! 0 |   232 | `		return 0;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate constant name */` |
|   633034 |   235 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   633034 |   236 | `	if( zDupName == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return 0;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Install the constant */` |
|   633034 |   241 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   633034 |   242 | `	pCons->xExpand = xExpand;` |
|   633034 |   243 | `	pCons->pUserData = pUserData;` |
|   633034 |   244 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   633034 |   245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   246 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   247 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   248 | `		return rc;` |
|        - |   249 | `	}` |
|        - |   250 | `	/* All done,constant can be invoked from PHP code */` |
|   633034 |   251 | `	return SXRET_OK;` |
|   316520 |   252 |  |
|        - |   253 | `/*` |
|        - |   254 | ` * Allocate a new foreign function instance.` |
|        - |   255 | ` * This function return SXRET_OK on success. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|  1390706 |   260 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   261 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   262 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   263 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   264 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   265 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   266 | `	)` |
|        2 |   267 |  |
|        - |   268 | `	ph7_user_func *pFunc;` |
|        - |   269 | `	char *zDup;` |
|        - |   270 | `	/* Allocate a new user function */` |
|  1390708 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1390708 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1390708 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1390708 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1390708 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1390708 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1390708 |   285 | `	pFunc->pVm   = pVm;` |
|  1390708 |   286 | `	pFunc->xFunc = xFunc;` |
|  1390708 |   287 | `	pFunc->pUserData = pUserData;` |
|  1390708 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1390708 |   290 | `	*ppOut = pFunc;` |
|  1390708 |   291 | `	return SXRET_OK;` |
|   695355 |   292 |  |
|        - |   293 | `/*` |
|        - |   294 | ` * Install a foreign function and it's associated callback so that` |
|        - |   295 | ` * it can be invoked from the target PHP code.` |
|        - |   296 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   297 | ` * return value indicates failure.` |
|        - |   298 | ` * Please refer to the official documentation for an introduction to` |
|        - |   299 | ` * the foreign function mechanism.` |
|        - |   300 | ` */` |
|  1393532 |   301 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   302 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   303 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   304 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   305 | `	void *pUserData           /* Foreign function private data */` |
|        - |   306 | `	)` |
|        2 |   307 |  |
|        - |   308 | `	ph7_user_func *pFunc;` |
|        - |   309 | `	SyHashEntry *pEntry;` |
|        - |   310 | `	sxi32 rc;` |
|        - |   311 | `	/* Overwrite any previously registered function with the same name */` |
|  1393534 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1393534 |   313 | `	if( pEntry ){` |
|     2828 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2828 |   315 | `		pFunc->pUserData = pUserData;` |
|     2828 |   316 | `		pFunc->xFunc = xFunc;` |
|     2828 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2828 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1390708 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1390708 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1390708 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1390708 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1390708 |   333 | `	return SXRET_OK;` |
|   696768 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   278244 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   278246 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   278246 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   278246 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   278246 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   278246 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   278246 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   278246 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   278246 |   362 | `	pFunc->iFlags = iFlags;` |
|   278246 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   278246 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   278246 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   278246 |   368 | `	return SXRET_OK;` |
|        2 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1457148 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        2 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1457150 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    41898 |   388 | `		pName = &pFunc->sName;` |
|    20948 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1457150 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1457150 |   392 | `	if( pEntry ){` |
|  1260832 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1260832 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1260832 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   196320 |   402 | `	pFunc->pNextName = 0;` |
|   196320 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   196320 |   404 | `	return rc;` |
|   728576 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   120332 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        2 |   413 |  |
|   120334 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   120334 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120334 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   120304 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   120304 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120304 |   429 | `	return rc;` |
|    60168 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4257720 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   435 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   436 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   437 | `	sxi32 iP1,    /* First operand */` |
|        - |   438 | `	sxu32 iP2,    /* Second operand */` |
|        - |   439 | `	void *p3,     /* Third operand */` |
|        - |   440 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   441 | `	)` |
|        2 |   442 |  |
|        - |   443 | `	VmInstr sInstr;` |
|        - |   444 | `	sxi32 rc;` |
|        - |   445 | `	/* Fill the VM instruction */` |
|  4257722 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4257722 |   447 | `	sInstr.iP1 = iP1;` |
|  4257722 |   448 | `	sInstr.iP2 = iP2;` |
|  4257722 |   449 | `	sInstr.p3  = p3;` |
|  4257722 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   231240 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115619 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4257722 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4257722 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4257722 |   460 | `	return rc;` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   552592 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   466 |  |
|   552594 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   552594 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   552594 |   474 | `	return SXRET_OK;` |
|        2 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   276296 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   480 |  |
|   276298 |   481 | `	return pVm->pByteContainer;` |
|        2 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   228022 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   228024 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   228024 |   490 | `	return pInstr;` |
|        2 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1278990 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   496 |  |
|  1278992 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   210934 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   503 |  |
|   210936 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   838534 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   510 |  |
|   838536 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   512 |  |
|    33476 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    33478 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33478 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    33478 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33478 |   522 | `	return &aInstr[n - 2];` |
|    16740 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    22380 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        2 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    22382 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22382 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    22382 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    22382 |   542 | `	pFrame->pUserData = pUserData;` |
|    22382 |   543 | `	pFrame->pThis = pThis;` |
|    22382 |   544 | `	pFrame->pVm = pVm;` |
|    22382 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22382 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22382 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22382 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22382 |   549 | `	return pFrame;` |
|    11192 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    22334 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        2 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    22336 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22336 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    22336 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    22336 |   571 | `	pVm->pFrame = pFrame;` |
|    22336 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    19190 |   574 | `		*ppFrame = pFrame;` |
|     9594 |   575 | `	}` |
|    22336 |   576 | `	return SXRET_OK;` |
|    11169 |   577 |  |
|        - |   578 | `/*` |
|        - |   579 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   580 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   581 | ` * information.` |
|        - |   582 | ` */` |
|       70 |   583 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   584 |  |
|        - |   585 | `	VmFrame *pTarget,*pFrame;` |
|       72 |   586 | `	SyHashEntry *pEntry = 0;` |
|        - |   587 | `	sxi32 rc;` |
|        - |   588 | `	/* Point to the upper frame */` |
|       72 |   589 | `	pFrame = pVm->pFrame;` |
|       72 |   590 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       72 |   591 | `	pTarget = pFrame;` |
|       72 |   592 | `	pFrame = pTarget->pParent;` |
|       72 |   593 | `	while( pFrame ){` |
|       72 |   594 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   595 | `			/* Query the current frame */` |
|       72 |   596 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       72 |   597 | `			if( pEntry ){` |
|        - |   598 | `				/* Variable found */` |
|       72 |   599 | `				break;` |
|        - |   600 | `			}` |
|      ! 0 |   601 | `		}` |
|        - |   602 | `		/* Point to the upper frame */` |
|      ! 0 |   603 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   604 | `	}` |
|       72 |   605 | `	if( pEntry == 0 ){` |
|        - |   606 | `		/* Inexistant variable */` |
|      ! 0 |   607 | `		return SXERR_NOTFOUND;` |
|        - |   608 | `	}` |
|        - |   609 | `	/* Link to the current frame */` |
|       72 |   610 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       72 |   611 | `	if( rc == SXRET_OK ){` |
|        - |   612 | `		sxu32 nIdx;` |
|       72 |   613 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       72 |   614 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       35 |   615 | `	}` |
|       72 |   616 | `	return rc;` |
|       37 |   617 |  |
|        - |   618 | `/*` |
|        - |   619 | ` * Leave the top-most active frame.` |
|        - |   620 | ` */` |
|    19184 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   622 |  |
|    19186 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19186 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    19186 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19186 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18814 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   124100 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   105288 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52645 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    18814 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   124174 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   105362 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52682 |   640 | `			}` |
|     9406 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    19186 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19186 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    19186 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19186 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    19186 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9592 |   649 | `	}` |
|    19186 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7115802 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   657 |  |
|  7118006 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2204 |   659 | `		pFrame = pFrame->pParent;` |
|        2 |   660 | `	}` |
|  7115804 |   661 | `	return pFrame;` |
|        2 |   662 |  |
|        - |   663 | `/*` |
|        - |   664 | ` * After a callee invoked from an OP_CALL site (object __invoke, an array` |
|        - |   665 | ` * callable, or a NEW constructor) returns PH7_EXCEPTION, decide how the current` |
|        - |   666 | ` * frame unwinds. The catch body, if any, already ran in-place inside` |
|        - |   667 | ` * VmThrowException. Returns TRUE and sets *pResumePc to the post-try resume` |
|        - |   668 | ` * point when THIS frame's own try caught the exception; returns FALSE to signal` |
|        - |   669 | ` * the caller to propagate (goto Exception) so the exception unwinds through` |
|        - |   670 | ` * intermediate frames that have no local handler.` |
|        - |   671 | ` */` |
|       12 |   672 | `static int VmCalleeExceptionResume(ph7_vm *pVm,sxi32 *pResumePc)` |
|        1 |   673 |  |
|       13 |   674 | `	VmFrame *pFrame = pVm->pFrame;` |
|       12 |   675 | `	if( (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0` |
|       11 |   676 | `	 && !(pFrame->iFlags & VM_FRAME_THROW) ){` |
|       11 |   677 | `		*pResumePc = (sxi32)pFrame->iExceptionJump - 1;` |
|       11 |   678 | `		return TRUE;` |
|        - |   679 | `	}` |
|        3 |   680 | `	return FALSE;` |
|        7 |   681 |  |
|        - |   682 | `/*` |
|        - |   683 | ` * Compare two functions signature and return the comparison result.` |
|        - |   684 | ` */` |
|      836 |   685 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   686 |  |
|      837 |   687 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   688 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   689 | `	const char *zSin = pSecond->zString;` |
|      837 |   690 | `	const char *zFin = pFirst->zString;` |
|      837 |   691 | `	const char *zPtr = zFin;` |
|      421 |   692 | `	for(;;){` |
|      843 |   693 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   694 | `			break;` |
|        - |   695 | `		}` |
|       19 |   696 | `		if( zFin[0] != zSin[0] ){` |
|        - |   697 | `			/* mismatch */` |
|       13 |   698 | `			break;` |
|        - |   699 | `		}` |
|        7 |   700 | `		zFin++;` |
|        7 |   701 | `		zSin++;` |
|        1 |   702 | `	}` |
|      837 |   703 | `	return (int)(zFin-zPtr);` |
|        1 |   704 |  |
|        - |   705 | `/*` |
|        - |   706 | ` * Select the appropriate VM function for the current call context.` |
|        - |   707 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   708 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   709 | ` * Refer to the official documentation for more information.` |
|        - |   710 | ` */` |
|      138 |   711 | `static ph7_vm_func * VmOverload(` |
|        - |   712 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   713 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   714 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   715 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   716 | `	)` |
|        2 |   717 |  |
|        - |   718 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   719 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   720 | `	ph7_vm_func *pLink;` |
|        - |   721 | `	SyString sArgSig;` |
|        - |   722 | `	SyBlob sSig;` |
|        - |   723 |  |
|      140 |   724 | `	pLink = pList;` |
|      140 |   725 | `	i = 0;` |
|        - |   726 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   727 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   728 | `		if( pLink == 0 ){` |
|       78 |   729 | `			break;` |
|        - |   730 | `		}` |
|      948 |   731 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   732 | `			/* Candidate for overloading */` |
|      902 |   733 | `			apSet[i++] = pLink;` |
|      450 |   734 | `		}` |
|        - |   735 | `		/* Point to the next entry */` |
|      948 |   736 | `		pLink = pLink->pNextName;` |
|        2 |   737 | `	}` |
|      140 |   738 | `	if( i < 1 ){` |
|        - |   739 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   740 | `		return pList;` |
|        - |   741 | `	}` |
|      140 |   742 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   743 | `		/* Return the only candidate */` |
|       32 |   744 | `		return apSet[0];` |
|        - |   745 | `	}` |
|        - |   746 | `	/* Calculate function signature */` |
|      109 |   747 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   748 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   749 | `		int c = 'n'; /* null */` |
|      259 |   750 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   751 | `			/* Hashmap */` |
|       45 |   752 | `			c = 'h';` |
|      237 |   753 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   754 | `			/* bool */` |
|      ! 0 |   755 | `			c = 'b';` |
|      215 |   756 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   757 | `			/* int */` |
|        7 |   758 | `			c = 'i';` |
|      212 |   759 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   760 | `			/* String */` |
|      107 |   761 | `			c = 's';` |
|      156 |   762 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   763 | `			/* Float */` |
|      ! 0 |   764 | `			c = 'f';` |
|      103 |   765 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   766 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   767 | `			int marker = 'o';` |
|        3 |   768 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   769 | `			SyString *pName = &pClass->sName;` |
|        3 |   770 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   771 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   772 | `			c = -1;` |
|        1 |   773 | `		}` |
|      259 |   774 | `		if( c > 0 ){` |
|      257 |   775 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   776 | `		}` |
|      130 |   777 | `	}` |
|      109 |   778 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   779 | `	iTarget = 0;` |
|      109 |   780 | `	iMax = -1;` |
|        - |   781 | `	/* Select the appropriate function */` |
|      945 |   782 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   783 | `		/* Compare the two signatures */` |
|      837 |   784 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   785 | `		if( iCur > iMax ){` |
|      113 |   786 | `			iMax = iCur;` |
|      113 |   787 | `			iTarget = j;` |
|       56 |   788 | `		}` |
|      419 |   789 | `	}` |
|      109 |   790 | `	SyBlobRelease(&sSig);` |
|        - |   791 | `	/* Appropriate function for the current call context */` |
|      109 |   792 | `	return apSet[iTarget];` |
|       71 |   793 |  |
|        - |   794 | `/* Forward declaration */` |
|        - |   795 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   796 | `/*` |
|        - |   797 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   798 | ` * it can be instanciated from the executed PHP script.` |
|        - |   799 | ` */` |
|        - |   800 | `/*` |
|        - |   801 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |   802 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |   803 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |   804 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |   805 | ` * properties register their enforcement slot. It is factored out of` |
|        - |   806 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |   807 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |   808 | ` */` |
|   353882 |   809 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   810 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   811 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   812 | `	)` |
|        2 |   813 |  |
|        - |   814 | `	ph7_class_attr *pAttr;` |
|        - |   815 | `	SyHashEntry *pEntry;` |
|        - |   816 | `	/* Reset the loop cursor */` |
|   353884 |   817 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   818 | `	/* Process only static and constant attribute */` |
|  1400431 |   819 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   820 | `		/* Extract the current attribute */` |
|   869608 |   821 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   869608 |   822 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   823 | `			ph7_value *pMemObj;` |
|        - |   824 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1828 |   825 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1828 |   826 | `			if( pMemObj == 0 ){` |
|      ! 0 |   827 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   828 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   829 | `					&pClass->sName,&pAttr->sName` |
|        - |   830 | `					);` |
|      ! 0 |   831 | `				return SXERR_MEM;` |
|        - |   832 | `			}` |
|     1828 |   833 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   834 | `				/* Initialize attribute default value (any complex expression) */` |
|     1824 |   835 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      911 |   836 | `			}` |
|        - |   837 | `			/* Record attribute index */` |
|     1828 |   838 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   839 | `			/* Install static attribute in the reference table */` |
|     1828 |   840 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   841 | `			/* If this is a typed static property, register the slot so the` |
|        - |   842 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   843 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   844 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1828 |   845 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       16 |   846 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       16 |   847 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   848 | `					return SXERR_MEM;` |
|        - |   849 | `				}` |
|       16 |   850 | `				pVmAttrS->pAttr = pAttr;` |
|       16 |   851 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       16 |   852 | `				pVmAttrS->iState = 0;` |
|       16 |   853 | `				pVmAttrS->pOwner = pClass;` |
|        - |   854 | `				/* Static typed property with no default starts uninitialized */` |
|       14 |   855 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|       11 |   856 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   857 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   858 | `				}` |
|       16 |   859 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   860 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   861 | `					return SXERR_MEM;` |
|        - |   862 | `				}` |
|        7 |   863 | `			}` |
|      913 |   864 | `		}` |
|        2 |   865 | `	}` |
|   353884 |   866 | `	return SXRET_OK;` |
|   176943 |   867 |  |
|   353650 |   868 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   869 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   870 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   871 | `	)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_class_method *pMeth;` |
|        - |   874 | `	SyHashEntry *pEntry;` |
|        - |   875 | `	sxi32 rc;` |
|        - |   876 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   353652 |   877 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   353652 |   878 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   879 | `		return rc;` |
|        - |   880 | `	}` |
|        - |   881 | `	/* Install class methods */` |
|   353652 |   882 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   883 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   884 | `		 */` |
|   191718 |   885 | `		return SXRET_OK;` |
|        - |   886 | `	}` |
|        - |   887 | `	/* Create constructor alias if not yet done */` |
|   161936 |   888 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   889 | `		/* User constructor with the same base class name */` |
|     6672 |   890 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6672 |   891 | `		if( pEntry ){` |
|      ! 0 |   892 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   893 | `			/* Create the alias */` |
|      ! 0 |   894 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   895 | `		}` |
|     3335 |   896 | `	}` |
|        - |   897 | `	/* Install the methods now */` |
|   161936 |   898 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1658163 |   899 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1415262 |   900 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1415262 |   901 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1415254 |   902 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1415254 |   903 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   904 | `				return rc;` |
|        - |   905 | `			}` |
|   707626 |   906 | `		}` |
|        2 |   907 | `	}` |
|        - |   908 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   161936 |   909 | `	pClass->bMounted = TRUE;` |
|   161936 |   910 | `	return SXRET_OK;` |
|   176827 |   911 |  |
|        - |   912 | `/*` |
|        - |   913 | ` * Allocate a private frame for attributes of the given` |
|        - |   914 | ` * class instance (Object in the PHP jargon).` |
|        - |   915 | ` */` |
|     2096 |   916 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   917 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   918 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   919 | `	)` |
|        2 |   920 |  |
|     2098 |   921 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   922 | `	ph7_class_attr *pAttr;` |
|        - |   923 | `	SyHashEntry *pEntry;` |
|        - |   924 | `	sxi32 rc;` |
|        - |   925 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2098 |   926 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8694 |   927 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   928 | `		VmClassAttr *pVmAttr;` |
|        - |   929 | `		/* Extract the current attribute */` |
|     6598 |   930 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6598 |   931 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6598 |   932 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   933 | `			return SXERR_MEM;` |
|        - |   934 | `		}` |
|     6598 |   935 | `		pVmAttr->pAttr = pAttr;` |
|     6598 |   936 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   937 | `			ph7_value *pMemObj;` |
|        - |   938 | `			/* Reserve a memory object for this attribute */` |
|     6572 |   939 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6572 |   940 | `			if( pMemObj == 0 ){` |
|      ! 0 |   941 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   942 | `				return SXERR_MEM;` |
|        - |   943 | `			}` |
|     6572 |   944 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6572 |   945 | `			pVmAttr->iState = 0;` |
|     6572 |   946 | `			pVmAttr->pOwner = pClass;` |
|     6572 |   947 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   948 | `				/* Initialize attribute default value (any complex expression) */` |
|     2258 |   949 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5444 |   950 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   951 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   952 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   953 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   954 | `			}` |
|     6572 |   955 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6572 |   956 | `			if( rc != SXRET_OK ){` |
|        - |   957 | `				VmSlot sSlot;` |
|        - |   958 | `				/* Restore memory object */` |
|      ! 0 |   959 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   960 | `				sSlot.pUserData = 0;` |
|      ! 0 |   961 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   962 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   963 | `				return SXERR_MEM;` |
|        - |   964 | `			}` |
|        - |   965 | `			/* Install attribute in the reference table */` |
|     6572 |   966 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   967 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   968 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   969 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6572 |   970 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      170 |   971 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      170 |   972 | `				if( rc != SXRET_OK ){` |
|        - |   973 | `					VmSlot sSlot;` |
|      ! 0 |   974 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   975 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   976 | `					sSlot.pUserData = 0;` |
|      ! 0 |   977 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   978 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   979 | `					return SXERR_MEM;` |
|        - |   980 | `				}` |
|       84 |   981 | `			}` |
|     3287 |   982 | `		}else{` |
|        - |   983 | `			/* Install static/constant attribute */` |
|       28 |   984 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       28 |   985 | `			pVmAttr->iState = 0;` |
|       28 |   986 | `			pVmAttr->pOwner = pClass;` |
|       28 |   987 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       28 |   988 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   989 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   990 | `				return SXERR_MEM;` |
|        - |   991 | `			}` |
|        - |   992 | `		}` |
|        2 |   993 | `	}` |
|     2098 |   994 | `	return SXRET_OK;` |
|     1050 |   995 |  |
|        - |   996 | `/* Forward declaration */` |
|        - |   997 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   998 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   999 | `/*` |
|        - |  1000 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  1001 | ` */` |
|        - |  1002 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  1003 | `/*` |
|        - |  1004 | ` * Reserve a constant memory object.` |
|        - |  1005 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1006 | ` */` |
|   455872 |  1007 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1008 |  |
|        - |  1009 | `	ph7_value *pObj;` |
|        - |  1010 | `	sxi32 rc;` |
|   455874 |  1011 | `	if( pIndex ){` |
|        - |  1012 | `		/* Object index in the object table */` |
|   446454 |  1013 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   223226 |  1014 | `	}` |
|        - |  1015 | `	/* Reserve a slot for the new object */` |
|   455874 |  1016 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   455874 |  1017 | `	if( rc != SXRET_OK ){` |
|        - |  1018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1020 | `		 */` |
|      ! 0 |  1021 | `		return 0;` |
|        - |  1022 | `	}` |
|   455874 |  1023 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   455874 |  1024 | `	return pObj;` |
|   227938 |  1025 |  |
|        - |  1026 | `/*` |
|        - |  1027 | ` * Reserve a memory object.` |
|        - |  1028 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1029 | ` */` |
|  2151964 |  1030 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1031 |  |
|        - |  1032 | `	ph7_value *pObj;` |
|        - |  1033 | `	sxi32 rc;` |
|  2151966 |  1034 | `	if( pIndex ){` |
|        - |  1035 | `		/* Object index in the object table */` |
|  2151966 |  1036 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075982 |  1037 | `	}` |
|        - |  1038 | `	/* Reserve a slot for the new object */` |
|  2151966 |  1039 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151966 |  1040 | `	if( rc != SXRET_OK ){` |
|        - |  1041 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1042 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1043 | `		 */` |
|      ! 0 |  1044 | `		return 0;` |
|        - |  1045 | `	}` |
|  2151966 |  1046 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151966 |  1047 | `	return pObj;` |
|  1075984 |  1048 |  |
|        - |  1049 | `/* Forward declaration */` |
|        - |  1050 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1051 | `/* Forward declarations for Fiber C functions */` |
|        - |  1052 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1053 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1054 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1055 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1056 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1057 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1058 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1059 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1060 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1061 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1062 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1063 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1064 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1065 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1066 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1067 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1068 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1069 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1070 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1071 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1072 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1073 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1074 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1075 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1076 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1077 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1078 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1079 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1080 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1081 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1082 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1083 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1084 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1085 | `/*` |
|        - |  1086 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1087 | ` * directly as foreign functions.` |
|        - |  1088 | ` */` |
|        - |  1089 | `#define PH7_BUILTIN_LIB \` |
|        - |  1090 | `	"interface Throwable {"\` |
|        - |  1091 | `	"public function getMessage();"\` |
|        - |  1092 | `	"public function getCode();"\` |
|        - |  1093 | `	"public function getFile();"\` |
|        - |  1094 | `	"public function getLine();"\` |
|        - |  1095 | `	"public function getTrace();"\` |
|        - |  1096 | `	"public function getTraceAsString();"\` |
|        - |  1097 | `	"public function getPrevious();"\` |
|        - |  1098 | `	"public function __toString();"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"interface Traversable {}"\` |
|        - |  1101 | `	"interface ArrayAccess {"\` |
|        - |  1102 | `	"public function offsetExists($offset);"\` |
|        - |  1103 | `	"public function offsetGet($offset);"\` |
|        - |  1104 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1105 | `	"public function offsetUnset($offset);"\` |
|        - |  1106 | `	"}"\` |
|        - |  1107 | `	"interface Countable {"\` |
|        - |  1108 | `	"public function count();"\` |
|        - |  1109 | `	"}"\` |
|        - |  1110 | `	"interface Stringable {"\` |
|        - |  1111 | `	"public function __toString();"\` |
|        - |  1112 | `	"}"\` |
|        - |  1113 | `	"interface JsonSerializable {"\` |
|        - |  1114 | `	"public function jsonSerialize();"\` |
|        - |  1115 | `	"}"\` |
|        - |  1116 | `	"interface UnitEnum {"\` |
|        - |  1117 | `	"public static function cases();"\` |
|        - |  1118 | `	"}"\` |
|        - |  1119 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1120 | `	"public static function from($value);"\` |
|        - |  1121 | `	"public static function tryFrom($value);"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"class Exception implements Throwable { "\` |
|        - |  1124 | `    "protected $message = '';"\` |
|        - |  1125 | `    "protected $code = 0;"\` |
|        - |  1126 | `    "protected $file;"\` |
|        - |  1127 | `    "protected $line;"\` |
|        - |  1128 | `    "protected $trace;"\` |
|        - |  1129 | `    "protected $previous;"\` |
|        - |  1130 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1131 | `	"   if( isset($message) ){"\` |
|        - |  1132 | `	"	  $this->message = $message;"\` |
|        - |  1133 | `	"   }"\` |
|        - |  1134 | `	"   $this->code = $code;"\` |
|        - |  1135 | `	"   $this->file = __FILE__;"\` |
|        - |  1136 | `	"   $this->line = __LINE__;"\` |
|        - |  1137 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1138 | `	"   if( isset($previous) ){"\` |
|        - |  1139 | `	"     $this->previous = $previous;"\` |
|        - |  1140 | `	"   }"\` |
|        - |  1141 | `	"}"\` |
|        - |  1142 | `	"public function getMessage(){"\` |
|        - |  1143 | `	"   return $this->message;"\` |
|        - |  1144 | `	"}"\` |
|        - |  1145 | `	" public function getCode(){"\` |
|        - |  1146 | `	"  return $this->code;"\` |
|        - |  1147 | `	"}"\` |
|        - |  1148 | `	"public function getFile(){"\` |
|        - |  1149 | `	"  return $this->file;"\` |
|        - |  1150 | `	"}"\` |
|        - |  1151 | `	"public function getLine(){"\` |
|        - |  1152 | `	"  return $this->line;"\` |
|        - |  1153 | `	"}"\` |
|        - |  1154 | `	"public function getTrace(){"\` |
|        - |  1155 | `	"   return $this->trace;"\` |
|        - |  1156 | `	"}"\` |
|        - |  1157 | `	"public function getTraceAsString(){"\` |
|        - |  1158 | `	"  return debug_string_backtrace();"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"public function getPrevious(){"\` |
|        - |  1161 | `	"    return $this->previous;"\` |
|        - |  1162 | `	"}"\` |
|        - |  1163 | `	"public function __toString(){"\` |
|        - |  1164 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1165 | `    "}"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"class Error implements Throwable { "\` |
|        - |  1168 | `    "protected $message = '';"\` |
|        - |  1169 | `    "protected $code = 0;"\` |
|        - |  1170 | `    "protected $file;"\` |
|        - |  1171 | `    "protected $line;"\` |
|        - |  1172 | `    "protected $trace;"\` |
|        - |  1173 | `    "protected $previous;"\` |
|        - |  1174 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1175 | `	"   if( isset($message) ){"\` |
|        - |  1176 | `	"	  $this->message = $message;"\` |
|        - |  1177 | `	"   }"\` |
|        - |  1178 | `	"   $this->code = $code;"\` |
|        - |  1179 | `	"   $this->file = __FILE__;"\` |
|        - |  1180 | `	"   $this->line = __LINE__;"\` |
|        - |  1181 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1182 | `	"   if( isset($previous) ){"\` |
|        - |  1183 | `	"     $this->previous = $previous;"\` |
|        - |  1184 | `	"   }"\` |
|        - |  1185 | `	"}"\` |
|        - |  1186 | `	"public function getMessage(){"\` |
|        - |  1187 | `	"   return $this->message;"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"public function getCode(){"\` |
|        - |  1190 | `	"  return $this->code;"\` |
|        - |  1191 | `	"}"\` |
|        - |  1192 | `	"public function getFile(){"\` |
|        - |  1193 | `	"  return $this->file;"\` |
|        - |  1194 | `	"}"\` |
|        - |  1195 | `	"public function getLine(){"\` |
|        - |  1196 | `	"  return $this->line;"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"public function getTrace(){"\` |
|        - |  1199 | `	"   return $this->trace;"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `	"public function getTraceAsString(){"\` |
|        - |  1202 | `	"  return debug_string_backtrace();"\` |
|        - |  1203 | `	"}"\` |
|        - |  1204 | `	"public function getPrevious(){"\` |
|        - |  1205 | `	"    return $this->previous;"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"public function __toString(){"\` |
|        - |  1208 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `	"class TypeError extends Error { }"\` |
|        - |  1212 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1213 | `	"class ValueError extends Error { }"\` |
|        - |  1214 | `	"class FiberError extends Error { }"\` |
|        - |  1215 | `	"class AssertionError extends Error { }"\` |
|        - |  1216 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1217 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1218 | `	"class ErrorException extends Exception { "\` |
|        - |  1219 | `	"protected $severity;"\` |
|        - |  1220 | `	"public function __construct(string $message = null,"\` |
|        - |  1221 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1222 | `	"   if( isset($message) ){"\` |
|        - |  1223 | `	"	  $this->message = $message;"\` |
|        - |  1224 | `	"   }"\` |
|        - |  1225 | `	"   $this->severity = $severity;"\` |
|        - |  1226 | `	"   $this->code = $code;"\` |
|        - |  1227 | `	"   $this->file = $filename;"\` |
|        - |  1228 | `	"   $this->line = $lineno;"\` |
|        - |  1229 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1230 | `	"   if( isset($previous) ){"\` |
|        - |  1231 | `	"     $this->previous = $previous;"\` |
|        - |  1232 | `	"   }"\` |
|        - |  1233 | `	"}"\` |
|        - |  1234 | `	"public function getSeverity(){"\` |
|        - |  1235 | `	"   return $this->severity;"\` |
|        - |  1236 | `    "}"\` |
|        - |  1237 | `	"}"\` |
|        - |  1238 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1239 | `	"class LogicException extends Exception { }"\` |
|        - |  1240 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1241 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1242 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1243 | `	"class DomainException extends LogicException { }"\` |
|        - |  1244 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1245 | `	"class LengthException extends LogicException { }"\` |
|        - |  1246 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1247 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1248 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1249 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1250 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1251 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1252 | `	"interface Iterator extends Traversable {"\` |
|        - |  1253 | `	"public function current();"\` |
|        - |  1254 | `	"public function key();"\` |
|        - |  1255 | `	"public function next();"\` |
|        - |  1256 | `	"public function rewind();"\` |
|        - |  1257 | `	"public function valid();"\` |
|        - |  1258 | `	"}"\` |
|        - |  1259 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1260 | `	"public function getIterator();"\` |
|        - |  1261 | `	"}"\` |
|        - |  1262 | `	"interface Serializable {"\` |
|        - |  1263 | `	"public function serialize();"\` |
|        - |  1264 | `	"public function unserialize(string $serialized);"\` |
|        - |  1265 | `	"}"\` |
|        - |  1266 | `	"/* Directory releated IO */"\` |
|        - |  1267 | `	"class Directory {"\` |
|        - |  1268 | `	"public $handle = null;"\` |
|        - |  1269 | `	"public $path  = null;"\` |
|        - |  1270 | `	"public function __construct(string $path)"\` |
|        - |  1271 | `	"{"\` |
|        - |  1272 | `	"   $this->handle = opendir($path);"\` |
|        - |  1273 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1274 | `	"      $this->path = $path;"\` |
|        - |  1275 | `	"   }"\` |
|        - |  1276 | `	"}"\` |
|        - |  1277 | `	"public function __destruct()"\` |
|        - |  1278 | `	"{"\` |
|        - |  1279 | `	"  if( $this->handle != null ){"\` |
|        - |  1280 | `	"       closedir($this->handle);"\` |
|        - |  1281 | `	"  }"\` |
|        - |  1282 | `	"}"\` |
|        - |  1283 | `	"public function read()"\` |
|        - |  1284 | `	"{"\` |
|        - |  1285 | `	"    return readdir($this->handle);"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"public function rewind()"\` |
|        - |  1288 | `	"{"\` |
|        - |  1289 | `	"    rewinddir($this->handle);"\` |
|        - |  1290 | `	"}"\` |
|        - |  1291 | `	"public function close()"\` |
|        - |  1292 | `	"{"\` |
|        - |  1293 | `	"    closedir($this->handle);"\` |
|        - |  1294 | `	"    $this->handle = null;"\` |
|        - |  1295 | `	"}"\` |
|        - |  1296 | `	"}"\` |
|        - |  1297 | `	"class Fiber {"\` |
|        - |  1298 | `	"  private $__ctx;"\` |
|        - |  1299 | `	"  private $__callable;"\` |
|        - |  1300 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1301 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1302 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1303 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1304 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1305 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1306 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1307 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1308 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1309 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1310 | `	"}"\` |
|        - |  1311 | `	"class Generator implements Iterator {"\` |
|        - |  1312 | `	"  private $__ctx;"\` |
|        - |  1313 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1314 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1315 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1316 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1317 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1318 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1319 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1320 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1321 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1322 | `	"}"\` |
|        - |  1323 | `	"class stdClass{"\` |
|        - |  1324 | `	"  public $value;"\` |
|        - |  1325 | `	" /* Magic methods */"\` |
|        - |  1326 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1327 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1328 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1329 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1330 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1331 | `	"}"\` |
|        - |  1332 | `	"function dir(string $path){"\` |
|        - |  1333 | `	"   return new Directory($path);"\` |
|        - |  1334 | `	"}"\` |
|        - |  1335 | `	"function Dir(string $path){"\` |
|        - |  1336 | `	"   return new Directory($path);"\` |
|        - |  1337 | `	"}"\` |
|        - |  1338 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1339 | `    "{"\` |
|        - |  1340 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1341 | `	"  $aDir = array();"\` |
|        - |  1342 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1343 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1344 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1345 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1346 | `	"   }"\` |
|        - |  1347 | `	"  closedir($pHandle);"\` |
|        - |  1348 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1349 | `	"      rsort($aDir);"\` |
|        - |  1350 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1351 | `	"      sort($aDir);"\` |
|        - |  1352 | `	"  }"\` |
|        - |  1353 | `	"  return $aDir;"\` |
|        - |  1354 | `	"}"\` |
|        - |  1355 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1356 | `	"/* Open the target directory */"\` |
|        - |  1357 | `	"$zDir = dirname($pattern);"\` |
|        - |  1358 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1359 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1360 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1361 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1362 | `	"	return FALSE;"\` |
|        - |  1363 | `	"}"\` |
|        - |  1364 | `	"$pattern = basename($pattern);"\` |
|        - |  1365 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1366 | `	"/* Loop throw available entries */"\` |
|        - |  1367 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1368 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1369 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1370 | `	"	if( $rc ){"\` |
|        - |  1371 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1372 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1373 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1374 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1375 | `	"		  }"\` |
|        - |  1376 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1377 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1378 | `	"		 continue;"\` |
|        - |  1379 | `	"	   }"\` |
|        - |  1380 | `	"	   /* Add the entry */"\` |
|        - |  1381 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1382 | `	"	}"\` |
|        - |  1383 | `	" }"\` |
|        - |  1384 | `	"/* Close the handle */"\` |
|        - |  1385 | `	"closedir($pHandle);"\` |
|        - |  1386 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1387 | `	"  /* Sort the array */"\` |
|        - |  1388 | `	"  sort($pArray);"\` |
|        - |  1389 | `	"}"\` |
|        - |  1390 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1391 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1392 | `	"  $pArray[] = $pattern;"\` |
|        - |  1393 | `	"}"\` |
|        - |  1394 | `	"/* Return the created array */"\` |
|        - |  1395 | `	"return $pArray;"\` |
|        - |  1396 | `   "}"\` |
|        - |  1397 | `   "/* Creates a temporary file */"\` |
|        - |  1398 | `   "function tmpfile(){"\` |
|        - |  1399 | `   "  /* Extract the temp directory */"\` |
|        - |  1400 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1401 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1402 | `   "    /* Use the current dir */"\` |
|        - |  1403 | `   "    $zTempDir = '.';"\` |
|        - |  1404 | `   "  }"\` |
|        - |  1405 | `   "  /* Create the file */"\` |
|        - |  1406 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1407 | `   "  return $pHandle;"\` |
|        - |  1408 | `   "}"\` |
|        - |  1409 | `   "/* Creates a temporary filename */"\` |
|        - |  1410 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1411 | `   "{"\` |
|        - |  1412 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1413 | `   "}"\` |
|        - |  1414 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1415 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1416 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1417 | `   "/* Copy arguments */"\` |
|        - |  1418 | `   "$nArgs = func_num_args();"\` |
|        - |  1419 | `   "$pNew = array();"\` |
|        - |  1420 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1421 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1422 | `    "}"\` |
|        - |  1423 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1424 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1425 | `	"/* Erase */"\` |
|        - |  1426 | `	"array_erase($pArray);"\` |
|        - |  1427 | `	"/* Unshift */"\` |
|        - |  1428 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1429 | `	"return sizeof($pArray);"\` |
|        - |  1430 | `    "}"\` |
|        - |  1431 | `	"function array_merge_recursive(){"\` |
|        - |  1432 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1433 | `    "$arrays = func_get_args();"\` |
|        - |  1434 | `    "$narrays = count($arrays);"\` |
|        - |  1435 | `    "$ret = array();"\` |
|        - |  1436 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1437 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1438 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1439 | `	 " }"\` |
|        - |  1440 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1441 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1442 | `     "  if( $keyIsInt ) {"\` |
|        - |  1443 | `     "   $ret[] = $value;"\` |
|        - |  1444 | `     "  } else {"\` |
|        - |  1445 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1446 | `     "    $cur = $ret[$key];"\` |
|        - |  1447 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1448 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1449 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1450 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1451 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1452 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1453 | `     "    } else {"\` |
|        - |  1454 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1455 | `     "    }"\` |
|        - |  1456 | `     "   } else {"\` |
|        - |  1457 | `     "    $ret[$key] = $value;"\` |
|        - |  1458 | `     "   }"\` |
|        - |  1459 | `     "  }"\` |
|        - |  1460 | `     " }"\` |
|        - |  1461 | `	 " }"\` |
|        - |  1462 | `	 " return $ret;"\` |
|        - |  1463 | `    "}"\` |
|        - |  1464 | `	"function max(){"\` |
|        - |  1465 | `    "  $pArgs = func_get_args();"\` |
|        - |  1466 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1467 | `	"  return null;"\` |
|        - |  1468 | `    " }"\` |
|        - |  1469 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1470 | `    " $pArg = $pArgs[0];"\` |
|        - |  1471 | `	" if( !is_array($pArg) ){"\` |
|        - |  1472 | `	"   return $pArg; "\` |
|        - |  1473 | `	" }"\` |
|        - |  1474 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1475 | `	"   return null;"\` |
|        - |  1476 | `	" }"\` |
|        - |  1477 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1478 | `	" reset($pArg);"\` |
|        - |  1479 | `	" $max = current($pArg);"\` |
|        - |  1480 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1481 | `	"   if( $val > $max ){"\` |
|        - |  1482 | `	"     $max = $val;"\` |
|        - |  1483 | `    " }"\` |
|        - |  1484 | `	" }"\` |
|        - |  1485 | `	" return $max;"\` |
|        - |  1486 | `    " }"\` |
|        - |  1487 | `    " $max = $pArgs[0];"\` |
|        - |  1488 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1489 | `    " $val = $pArgs[$i];"\` |
|        - |  1490 | `	"if( $val > $max ){"\` |
|        - |  1491 | `	" $max = $val;"\` |
|        - |  1492 | `	"}"\` |
|        - |  1493 | `    " }"\` |
|        - |  1494 | `	" return $max;"\` |
|        - |  1495 | `    "}"\` |
|        - |  1496 | `	"function min(){"\` |
|        - |  1497 | `    "  $pArgs = func_get_args();"\` |
|        - |  1498 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1499 | `	"  return null;"\` |
|        - |  1500 | `    " }"\` |
|        - |  1501 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1502 | `    " $pArg = $pArgs[0];"\` |
|        - |  1503 | `	" if( !is_array($pArg) ){"\` |
|        - |  1504 | `	"   return $pArg; "\` |
|        - |  1505 | `	" }"\` |
|        - |  1506 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1507 | `	"   return null;"\` |
|        - |  1508 | `	" }"\` |
|        - |  1509 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1510 | `	" reset($pArg);"\` |
|        - |  1511 | `	" $min = current($pArg);"\` |
|        - |  1512 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1513 | `	"   if( $val < $min ){"\` |
|        - |  1514 | `	"     $min = $val;"\` |
|        - |  1515 | `    " }"\` |
|        - |  1516 | `	" }"\` |
|        - |  1517 | `	" return $min;"\` |
|        - |  1518 | `    " }"\` |
|        - |  1519 | `    " $min = $pArgs[0];"\` |
|        - |  1520 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1521 | `    " $val = $pArgs[$i];"\` |
|        - |  1522 | `	"if( $val < $min ){"\` |
|        - |  1523 | `	" $min = $val;"\` |
|        - |  1524 | `	" }"\` |
|        - |  1525 | `    " }"\` |
|        - |  1526 | `	" return $min;"\` |
|        - |  1527 | `	"}"\` |
|        - |  1528 | `	"function fileowner(string $file){"\` |
|        - |  1529 | `    " $a = stat($file);"\` |
|        - |  1530 | `	" if( !is_array($a) ){"\` |
|        - |  1531 | `	"	return false;"\` |
|        - |  1532 | `	" }"\` |
|        - |  1533 | `	" return $a['uid'];"\` |
|        - |  1534 | `    "}"\` |
|        - |  1535 | `    "function filegroup(string $file){"\` |
|        - |  1536 | `	" $a = stat($file);"\` |
|        - |  1537 | `	" if( !is_array($a) ){"\` |
|        - |  1538 | `	"	return false;"\` |
|        - |  1539 | `	" }"\` |
|        - |  1540 | `	" return $a['gid'];"\` |
|        - |  1541 | `    "}"\` |
|        - |  1542 | `	 "function fileinode(string $file){"\` |
|        - |  1543 | `	" $a = stat($file);"\` |
|        - |  1544 | `	" if( !is_array($a) ){"\` |
|        - |  1545 | `	"	return false;"\` |
|        - |  1546 | `	" }"\` |
|        - |  1547 | `	" return $a['ino'];"\` |
|        - |  1548 | `    "}"` |
|        - |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1552 | ` * start compiling the target PHP program.` |
|        - |  1553 | ` */` |
|     3140 |  1554 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1555 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1556 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1557 | `	 )` |
|        2 |  1558 |  |
|        - |  1559 | `	SyString sBuiltin;` |
|        - |  1560 | `	ph7_value *pObj;` |
|        - |  1561 | `	sxi32 rc;` |
|        - |  1562 | `	/* Zero the structure */` |
|     3142 |  1563 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1564 | `	/* Initialize VM fields */` |
|     3142 |  1565 | `	pVm->pEngine = &(*pEngine);` |
|     3142 |  1566 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1567 | `	/* Instructions containers */` |
|     3142 |  1568 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3142 |  1569 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3142 |  1570 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1571 | `	/* Object containers */` |
|     3142 |  1572 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3142 |  1573 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1574 | `	/* Virtual machine internal containers */` |
|     3142 |  1575 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3142 |  1576 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3142 |  1577 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3142 |  1578 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3142 |  1579 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3142 |  1580 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3142 |  1581 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3142 |  1582 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3142 |  1583 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3142 |  1584 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3142 |  1585 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3142 |  1586 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3142 |  1587 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3142 |  1588 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3142 |  1589 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3142 |  1590 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3142 |  1591 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3142 |  1592 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3142 |  1593 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3142 |  1594 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3142 |  1595 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3142 |  1596 | `	pVm->pPendingException = 0;` |
|        - |  1597 | `	/* Configuration containers */` |
|     3142 |  1598 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3142 |  1599 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3142 |  1600 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3142 |  1601 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3142 |  1602 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3142 |  1603 | `	pVm->iResponseStatus = 200;` |
|     3142 |  1604 | `	pVm->bHeadersSent = 0;` |
|     3142 |  1605 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1606 | `	/* Error callbacks containers */` |
|     3142 |  1607 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3142 |  1608 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3142 |  1609 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3142 |  1610 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3142 |  1611 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1612 | `	/* Set a default recursion limit */` |
|        - |  1613 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3142 |  1614 | `	pVm->nMaxDepth = 32;` |
|        - |  1615 | `#else` |
|        - |  1616 | `	pVm->nMaxDepth = 16;` |
|        - |  1617 | `#endif` |
|        - |  1618 | `	/* Default assertion flags */` |
|     3142 |  1619 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1620 | `	/* JSON return status */` |
|     3142 |  1621 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1622 | `	/* PRNG context */` |
|     3142 |  1623 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1624 | `	/* Install the null constant */` |
|     3142 |  1625 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3142 |  1626 | `	if( pObj == 0 ){` |
|      ! 0 |  1627 | `		rc = SXERR_MEM;` |
|      ! 0 |  1628 | `		goto Err;` |
|        - |  1629 | `	}` |
|     3142 |  1630 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1631 | `	/* Install the boolean TRUE constant */` |
|     3142 |  1632 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3142 |  1633 | `	if( pObj == 0 ){` |
|      ! 0 |  1634 | `		rc = SXERR_MEM;` |
|      ! 0 |  1635 | `		goto Err;` |
|        - |  1636 | `	}` |
|     3142 |  1637 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1638 | `	/* Install the boolean FALSE constant */` |
|     3142 |  1639 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3142 |  1640 | `	if( pObj == 0 ){` |
|      ! 0 |  1641 | `		rc = SXERR_MEM;` |
|      ! 0 |  1642 | `		goto Err;` |
|        - |  1643 | `	}` |
|     3142 |  1644 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1645 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1646 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1647 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3142 |  1648 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3142 |  1649 | `	if( pObj == 0 ){` |
|      ! 0 |  1650 | `		rc = SXERR_MEM;` |
|      ! 0 |  1651 | `		goto Err;` |
|        - |  1652 | `	}` |
|     3142 |  1653 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1654 | `	/* Create the global frame */` |
|     3142 |  1655 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3142 |  1656 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1657 | `		goto Err;` |
|        - |  1658 | `	}` |
|        - |  1659 | `	/* Initialize the code generator */` |
|     3142 |  1660 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3142 |  1661 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1662 | `		goto Err;` |
|        - |  1663 | `	}` |
|        - |  1664 | `	/* VM correctly initialized,set the magic number */` |
|     3142 |  1665 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3142 |  1666 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1667 | `	/* Compile the built-in library */` |
|     3142 |  1668 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1669 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3142 |  1670 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1671 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3142 |  1672 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3142 |  1673 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3142 |  1674 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3142 |  1675 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|        - |  1676 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3142 |  1677 | `	pVm->pCoalesceObj = 0;` |
|     3142 |  1678 | `	pVm->bCoalesceArmed = 0;` |
|     3142 |  1679 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1680 | `	/* Register Fiber internal C functions */` |
|     3142 |  1681 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3142 |  1682 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3142 |  1683 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3142 |  1684 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3142 |  1685 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3142 |  1686 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3142 |  1687 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3142 |  1688 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3142 |  1689 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3142 |  1690 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1691 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3142 |  1692 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3142 |  1693 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3142 |  1694 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3142 |  1695 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3142 |  1696 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3142 |  1697 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3142 |  1698 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3142 |  1699 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3142 |  1700 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3142 |  1701 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1702 | `	/* Reset the code generator */` |
|     3142 |  1703 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3142 |  1704 | `	return SXRET_OK;` |
|      ! 0 |  1705 | `Err:` |
|      ! 0 |  1706 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1707 | `	return rc;` |
|     1572 |  1708 |  |
|        - |  1709 | `/*` |
|        - |  1710 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1711 | ` * routine which store the output in an internal blob.` |
|        - |  1712 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1713 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1714 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1715 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1716 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1717 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1718 | ` * to finish executing and extracting the output.` |
|        - |  1719 | ` */` |
|       56 |  1720 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1721 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1722 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1723 | `	void *pUserData     /* User private data */` |
|        - |  1724 | `	)` |
|      ! 0 |  1725 |  |
|        - |  1726 | `	 sxi32 rc;` |
|        - |  1727 | `	 /* Store the output in an internal BLOB */` |
|       56 |  1728 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       56 |  1729 | `	 return rc;` |
|      ! 0 |  1730 |  |
|        - |  1731 | `/*` |
|        - |  1732 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1733 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1734 | ` */` |
|    20488 |  1735 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1736 |  |
|    20490 |  1737 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20490 |  1738 | `	if( xCons != VmObConsumer ){` |
|     8220 |  1739 | `		pVm->nOutputLen += nLen;` |
|     8220 |  1740 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1024 |  1741 | `			pVm->bHeadersSent = 1;` |
|      511 |  1742 | `		}` |
|     4109 |  1743 | `	}` |
|    20490 |  1744 |  |
|        - |  1745 | `#define VM_STACK_GUARD 16` |
|        - |  1746 | `/*` |
|        - |  1747 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1748 | ` * our compiled PHP program.` |
|        - |  1749 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1750 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1751 | ` */` |
|    44976 |  1752 | `static ph7_value * VmNewOperandStack(` |
|        - |  1753 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1754 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1755 | `	)` |
|        2 |  1756 |  |
|        - |  1757 | `	ph7_value *pStack;` |
|        - |  1758 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1759 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1760 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1761 | `  ** on the maximum stack depth required.` |
|        - |  1762 | `  **` |
|        - |  1763 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1764 | `  */` |
|    44978 |  1765 | `	nInstr += VM_STACK_GUARD;` |
|    44978 |  1766 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    44978 |  1767 | `	if( pStack == 0 ){` |
|      ! 0 |  1768 | `		return 0;` |
|        - |  1769 | `	}` |
|        - |  1770 | `	/* Initialize the operand stack */` |
|  3032798 |  1771 | `	while( nInstr > 0 ){` |
|  2987822 |  1772 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2987822 |  1773 | `		--nInstr;` |
|        2 |  1774 | `	}` |
|        - |  1775 | `	/* Ready for bytecode execution */` |
|    44978 |  1776 | `	return pStack;` |
|    22490 |  1777 |  |
|        - |  1778 | `/* Forward declaration */` |
|        - |  1779 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1780 | `/*` |
|        - |  1781 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1782 | ` * This routine gets called by the PH7 engine after` |
|        - |  1783 | ` * successful compilation of the target PHP program.` |
|        - |  1784 | ` */` |
|     2826 |  1785 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1786 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1787 | `	)` |
|        2 |  1788 |  |
|        - |  1789 | `	SyHashEntry *pEntry;` |
|        - |  1790 | `	sxi32 rc;` |
|     2828 |  1791 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1792 | `		/* Initialize your VM first */` |
|      ! 0 |  1793 | `		return SXERR_CORRUPT;` |
|        - |  1794 | `	}` |
|        - |  1795 | `	/* Mark the VM ready for byte-code execution */` |
|     2828 |  1796 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1797 | `	/* Release the code generator now we have compiled our program */` |
|     2828 |  1798 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1799 | `	/* Emit the DONE instruction */` |
|     2828 |  1800 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2828 |  1801 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1802 | `		return SXERR_MEM;` |
|        - |  1803 | `	}` |
|        - |  1804 | `	/* Script return value */` |
|     2828 |  1805 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1806 | `	/* Allocate a new operand stack */` |
|     2828 |  1807 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2828 |  1808 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1809 | `		return SXERR_MEM;` |
|        - |  1810 | `	}` |
|        - |  1811 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1812 | `	 * private data. */` |
|     2828 |  1813 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2828 |  1814 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1815 | `	/* Allocate the reference table */` |
|     2828 |  1816 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2828 |  1817 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2828 |  1818 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1819 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1820 | `		return SXERR_MEM;` |
|        - |  1821 | `	}` |
|        - |  1822 | `	/* Zero the reference table */` |
|     2828 |  1823 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1824 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2828 |  1825 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2828 |  1826 | `	if( rc != SXRET_OK ){` |
|        - |  1827 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1828 | `		return rc;` |
|        - |  1829 | `	}` |
|        - |  1830 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  1831 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  1832 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  1833 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  1834 | `	 * below it is compile-time/init state that survives a reset. */` |
|     2828 |  1835 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - |  1836 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2828 |  1837 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2828 |  1838 | `	if( rc != SXRET_OK ){` |
|        - |  1839 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1840 | `		return rc;` |
|        - |  1841 | `	}` |
|        - |  1842 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2828 |  1843 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1844 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2828 |  1845 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1846 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2828 |  1847 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1848 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1849 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2828 |  1850 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2828 |  1851 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1852 | `#endif` |
|        - |  1853 | `	/* Initialize and install static and constants class attributes.` |
|        - |  1854 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  1855 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  1856 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  1857 | `	 * that function in sync when changing what is reserved here. */` |
|     2828 |  1858 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   110562 |  1859 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   107736 |  1860 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   107736 |  1861 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1862 | `			return rc;` |
|        - |  1863 | `		}` |
|        2 |  1864 | `	}` |
|        - |  1865 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2828 |  1866 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1867 | `	/* VM is ready for bytecode execution */` |
|     2828 |  1868 | `	return SXRET_OK;` |
|     1415 |  1869 |  |
|        - |  1870 | `/*` |
|        - |  1871 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  1872 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  1873 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  1874 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  1875 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  1876 | ` */` |
|        6 |  1877 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  1878 |  |
|        - |  1879 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - |  1880 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - |  1881 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      204 |  1882 | `	while( pVm->pRefList ){` |
|      198 |  1883 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 |  1884 | `	}` |
|        6 |  1885 |  |
|        - |  1886 | `/*` |
|        - |  1887 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  1888 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  1889 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  1890 | ` */` |
|       42 |  1891 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  1892 |  |
|       42 |  1893 | `	PH7_MemObjRelease(pObj);` |
|       42 |  1894 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       42 |  1895 |  |
|        - |  1896 | `/*` |
|        - |  1897 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  1898 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  1899 | ` * of statics).` |
|        - |  1900 | ` */` |
|      380 |  1901 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 |  1902 |  |
|      380 |  1903 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  1904 | `	sxu32 k;` |
|      384 |  1905 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        4 |  1906 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        2 |  1907 | `	}` |
|      380 |  1908 |  |
|        - |  1909 | `/*` |
|        - |  1910 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  1911 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  1912 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  1913 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  1914 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  1915 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  1916 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  1917 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  1918 | ` *    has its static sentinels reset.` |
|        - |  1919 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  1920 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  1921 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  1922 | ` */` |
|        6 |  1923 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  1924 |  |
|        - |  1925 | `	SyHashEntry *pEntry;` |
|        6 |  1926 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      386 |  1927 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      380 |  1928 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      380 |  1929 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  1930 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - |  1931 | `			 * release its captured-by-value environment, then free the entry,` |
|        - |  1932 | `			 * name buffer and structure. */` |
|        4 |  1933 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 |  1934 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - |  1935 | `			sxu32 k;` |
|        4 |  1936 | `			VmResetFuncStatics(pFunc);` |
|        8 |  1937 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 |  1938 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 |  1939 | `			}` |
|        4 |  1940 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - |  1941 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  1942 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  1943 | `			if( zName ){` |
|        4 |  1944 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 |  1945 | `			}` |
|        4 |  1946 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 |  1947 | `			continue;` |
|        - |  1948 | `		}` |
|        - |  1949 | `		/* Named function: reset statics for every overload sharing this name. */` |
|      752 |  1950 | `		while( pFunc ){` |
|      376 |  1951 | `			VmResetFuncStatics(pFunc);` |
|      376 |  1952 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  1953 | `		}` |
|      ! 0 |  1954 | `	}` |
|        6 |  1955 | `	pVm->closure_cnt = 0;` |
|        6 |  1956 |  |
|        - |  1957 | `/*` |
|        - |  1958 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  1959 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  1960 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  1961 | ` * the class re-mount registers fresh ones.` |
|        - |  1962 | ` */` |
|        6 |  1963 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  1964 |  |
|        - |  1965 | `	SyHashEntry *pEntry;` |
|        - |  1966 | `	/* Common case: no class static typed properties — table already empty. */` |
|        6 |  1967 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        2 |  1968 | `		return;` |
|        - |  1969 | `	}` |
|        - |  1970 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  1971 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  1972 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  1973 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  1974 | `		if( pEntry->pUserData ){` |
|        4 |  1975 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  1976 | `		}` |
|      ! 0 |  1977 | `	}` |
|        4 |  1978 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  1979 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        3 |  1980 |  |
|        - |  1981 | `/*` |
|        - |  1982 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  1983 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  1984 | ` *` |
|        - |  1985 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  1986 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  1987 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  1988 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  1989 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  1990 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  1991 | ` *` |
|        - |  1992 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  1993 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  1994 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  1995 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  1996 | ` * reserved during the run.` |
|        - |  1997 | ` *` |
|        - |  1998 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  1999 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  2000 | ` * global-scope destructors never fired.` |
|        - |  2001 | ` */` |
|        6 |  2002 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  2003 |  |
|        - |  2004 | `	sxu32 nWater,n;` |
|        6 |  2005 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  2006 | `		return SXERR_CORRUPT;` |
|        - |  2007 | `	}` |
|        6 |  2008 | `	nWater = pVm->nSuperBaseline;` |
|        - |  2009 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  2010 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        6 |  2011 | `	pVm->pGlobal = 0;` |
|        - |  2012 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  2013 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  2014 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - |  2015 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        6 |  2016 | `	pVm->bInReset = 1;` |
|        - |  2017 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        6 |  2018 | `	VmResetRefTable(&(*pVm));` |
|        - |  2019 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  2020 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  2021 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  2022 | `	 * re-run define() overwrites the value in place). */` |
|        6 |  2023 | `	VmResetFunctionState(&(*pVm));` |
|        - |  2024 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  2025 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      218 |  2026 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      212 |  2027 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      212 |  2028 | `		if( pObj ){` |
|      212 |  2029 | `			PH7_MemObjRelease(pObj);` |
|      106 |  2030 | `		}` |
|      106 |  2031 | `	}` |
|        - |  2032 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  2033 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        6 |  2034 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  2035 | `	/* (5) Unwind any active frames back to none. */` |
|       12 |  2036 | `	while( pVm->pFrame ){` |
|        6 |  2037 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2038 | `	}` |
|        - |  2039 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        6 |  2040 | `	pVm->bInReset = 0;` |
|        - |  2041 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  2042 | `	 * slots (their indices no longer exist). */` |
|        6 |  2043 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        6 |  2044 | `	SySetReset(&pVm->aFreeObj);` |
|        - |  2045 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        6 |  2046 | `	SyHashRelease(&pVm->hSuper);` |
|        6 |  2047 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - |  2048 | `	/* (8) Drain remaining per-exec containers. */` |
|        6 |  2049 | `	SySetReset(&pVm->aSelf);` |
|        - |  2050 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  2051 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  2052 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        6 |  2053 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  2054 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  2055 | `		if( pCB ){` |
|        - |  2056 | `			int iArg;` |
|      ! 0 |  2057 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2058 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  2059 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  2060 | `			}` |
|      ! 0 |  2061 | `		}` |
|      ! 0 |  2062 | `	}` |
|        6 |  2063 | `	SySetReset(&pVm->aShutdown);` |
|        6 |  2064 | `	SySetReset(&pVm->aException);` |
|        6 |  2065 | `	pVm->pPendingException = 0;` |
|        6 |  2066 | `	pVm->nExceptDepth = 0;` |
|        - |  2067 | `	/* spl_autoload_register() callbacks are per request */` |
|        6 |  2068 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  2069 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  2070 | `		if( pCB ){` |
|      ! 0 |  2071 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2072 | `		}` |
|      ! 0 |  2073 | `	}` |
|        6 |  2074 | `	SySetReset(&pVm->aAutoload);` |
|        - |  2075 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  2076 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        6 |  2077 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  2078 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  2079 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  2080 | `	}` |
|        - |  2081 | `	/* Output buffers */` |
|        6 |  2082 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  2083 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  2084 | `		if( pOb ){` |
|      ! 0 |  2085 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  2086 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  2087 | `		}` |
|      ! 0 |  2088 | `	}` |
|        6 |  2089 | `	SySetReset(&pVm->aOB);` |
|        6 |  2090 | `	pVm->nObDepth = 0;` |
|        - |  2091 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  2092 | `	{` |
|        6 |  2093 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        6 |  2094 | `		if( rc == SXRET_OK ){` |
|        6 |  2095 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        3 |  2096 | `		}` |
|        6 |  2097 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  2098 | `			return rc;` |
|        - |  2099 | `		}` |
|        - |  2100 | `	}` |
|        - |  2101 | `	/* (10) Re-mount the static/const attribute slots of every class. */` |
|        - |  2102 | `	{` |
|        - |  2103 | `		SyHashEntry *pEntry;` |
|        6 |  2104 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      238 |  2105 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      232 |  2106 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      232 |  2107 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2108 | `				return rc;` |
|        - |  2109 | `			}` |
|      ! 0 |  2110 | `		}` |
|        - |  2111 | `	}` |
|        - |  2112 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        6 |  2113 | `	SyBlobReset(&pVm->sConsumer);` |
|        6 |  2114 | `	pVm->nOutputLen = 0;` |
|        6 |  2115 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        6 |  2116 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        6 |  2117 | `	pVm->iResponseStatus = 200;` |
|        6 |  2118 | `	pVm->bHeadersSent = 0;` |
|        6 |  2119 | `	pVm->bHttpContext = 0;` |
|        6 |  2120 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        6 |  2121 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        6 |  2122 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        6 |  2123 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        6 |  2124 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        6 |  2125 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  2126 | `#ifdef PH7_ENABLE_PCRE` |
|        6 |  2127 | `	pVm->iPcreLastError = 0;` |
|        - |  2128 | `#endif` |
|        6 |  2129 | `	pVm->iCmpCallbackExc = 0;` |
|        6 |  2130 | `	pVm->bHaltRequested = 0;` |
|        6 |  2131 | `	pVm->iExitStatus = 0;` |
|        6 |  2132 | `	pVm->iSpreadExtra = 0;` |
|        6 |  2133 | `	pVm->nRecursionDepth = 0;` |
|        6 |  2134 | `	pVm->pActiveCtx = 0;` |
|        6 |  2135 | `	pVm->pCoalesceObj = 0;` |
|        6 |  2136 | `	pVm->bCoalesceArmed = 0;` |
|        6 |  2137 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  2138 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        6 |  2139 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  2140 | `	/* Set the ready flag */` |
|        6 |  2141 | `	pVm->nMagic = PH7_VM_RUN;` |
|        6 |  2142 | `	return SXRET_OK;` |
|        3 |  2143 |  |
|        - |  2144 | `/*` |
|        - |  2145 | ` * Release a Virtual Machine.` |
|        - |  2146 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  2147 | ` */` |
|     2826 |  2148 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  2149 |  |
|        - |  2150 | `	/* Set the stale magic number */` |
|     2828 |  2151 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  2152 | `	/* Release the private memory subsystem */` |
|     2828 |  2153 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2828 |  2154 | `	return SXRET_OK;` |
|        2 |  2155 |  |
|        - |  2156 | `/*` |
|        - |  2157 | ` * Initialize a foreign function call context.` |
|        - |  2158 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  2159 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  2160 | ` * functions.` |
|        - |  2161 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  2162 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  2163 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  2164 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  2165 | ` */` |
|   696658 |  2166 | `static sxi32 VmInitCallContext(` |
|        - |  2167 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2168 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2169 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2170 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2171 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2172 | `	)` |
|        2 |  2173 |  |
|   696660 |  2174 | `	pOut->pFunc = pFunc;` |
|   696660 |  2175 | `	pOut->pVm   = pVm;` |
|   696660 |  2176 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   696660 |  2177 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2178 | `	/* Assume a null return value */` |
|   696660 |  2179 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   696660 |  2180 | `	pOut->pRet = pRet;` |
|   696660 |  2181 | `	pOut->iFlags = iFlags;` |
|   696660 |  2182 | `	return SXRET_OK;` |
|        2 |  2183 |  |
|        - |  2184 | `/*` |
|        - |  2185 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2186 | ` * left behind.` |
|        - |  2187 | ` */` |
|   696658 |  2188 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  2189 |  |
|        - |  2190 | `	sxu32 n;` |
|   696660 |  2191 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8628 |  2192 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25202 |  2193 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16576 |  2194 | `			if( apObj[n] == 0 ){` |
|        - |  2195 | `				/* Already released */` |
|      384 |  2196 | `				continue;` |
|        - |  2197 | `			}` |
|    16194 |  2198 | `			PH7_MemObjRelease(apObj[n]);` |
|    16194 |  2199 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8098 |  2200 | `		}` |
|     8628 |  2201 | `		SySetRelease(&pCtx->sVar);` |
|     4313 |  2202 | `	}` |
|   696660 |  2203 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  2204 | `		ph7_aux_data *aAux;` |
|        - |  2205 | `		void *pChunk;` |
|        - |  2206 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  2207 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  2208 | `		 */` |
|        9 |  2209 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  2210 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  2211 | `			pChunk = aAux[n].pAuxData;` |
|        - |  2212 | `			/* Release the chunk */` |
|       25 |  2213 | `			if( pChunk ){` |
|       25 |  2214 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  2215 | `			}` |
|       13 |  2216 | `		}` |
|        9 |  2217 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  2218 | `	}` |
|   696660 |  2219 |  |
|        - |  2220 | `/*` |
|        - |  2221 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  2222 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  2223 | ` */` |
|      382 |  2224 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  2225 | `	ph7_context *pCtx, /* Call context */` |
|        - |  2226 | `	ph7_value *pValue  /* Release this value */` |
|        - |  2227 | `	)` |
|        2 |  2228 |  |
|      384 |  2229 | `	if( pValue == 0 ){` |
|        - |  2230 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  2231 | `		return;` |
|        - |  2232 | `	}` |
|      384 |  2233 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      384 |  2234 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  2235 | `		sxu32 n;` |
|     1282 |  2236 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1282 |  2237 | `			if( apObj[n] == pValue ){` |
|      384 |  2238 | `				PH7_MemObjRelease(pValue);` |
|      384 |  2239 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  2240 | `				/* Mark as released */` |
|      384 |  2241 | `				apObj[n] = 0;` |
|      384 |  2242 | `				break;` |
|        - |  2243 | `			}` |
|      451 |  2244 | `		}` |
|      191 |  2245 | `	}` |
|      193 |  2246 |  |
|        - |  2247 | `/*` |
|        - |  2248 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  2249 | ` */` |
|  3948476 |  2250 | `static void VmPopOperand(` |
|        - |  2251 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2252 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2253 | `	)` |
|        2 |  2254 |  |
|  3948478 |  2255 | `	ph7_value *pTos = *ppTos;` |
|  8412554 |  2256 | `	while( nPop > 0 ){` |
|  4464078 |  2257 | `		PH7_MemObjRelease(pTos);` |
|  4464078 |  2258 | `		pTos--;` |
|  4464078 |  2259 | `		nPop--;` |
|        2 |  2260 | `	}` |
|        - |  2261 | `	/* Top of the stack */` |
|  3948478 |  2262 | `	*ppTos = pTos;` |
|  3948478 |  2263 |  |
|        - |  2264 | `/*` |
|        - |  2265 | ` * Reserve a memory object.` |
|        - |  2266 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2267 | ` */` |
|  3205490 |  2268 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  2269 |  |
|  3205492 |  2270 | `	ph7_value *pObj = 0;` |
|        - |  2271 | `	VmSlot *pSlot;` |
|        - |  2272 | `	sxu32 nIdx;` |
|        - |  2273 | `	/* Check for a free slot */` |
|  3205492 |  2274 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3205492 |  2275 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3205492 |  2276 | `	if( pSlot ){` |
|  1053534 |  2277 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1053534 |  2278 | `		nIdx = pSlot->nIdx;` |
|   526766 |  2279 | `	}` |
|  3205492 |  2280 | `	if( pObj == 0 ){` |
|        - |  2281 | `		/* Reserve a new memory object */` |
|  2151960 |  2282 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151960 |  2283 | `		if( pObj == 0 ){` |
|      ! 0 |  2284 | `			return 0;` |
|        - |  2285 | `		}` |
|  1075979 |  2286 | `	}` |
|        - |  2287 | `	/* Set a null default value */` |
|  3205492 |  2288 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3205492 |  2289 | `	pObj->nIdx = nIdx;` |
|  3205492 |  2290 | `	return pObj;` |
|  1602747 |  2291 |  |
|        - |  2292 | `/*` |
|        - |  2293 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2294 | ` */` |
|    35346 |  2295 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2296 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2297 | `	const char *zKey,  /* Entry key */` |
|        - |  2298 | `	sxu32 nByte,       /* Key length */` |
|        - |  2299 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2300 | `	)` |
|        2 |  2301 |  |
|        - |  2302 | `	ph7_value sKey;` |
|        - |  2303 | `	sxi32 rc;` |
|    35348 |  2304 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35348 |  2305 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2306 | `	/* Perform the insertion */` |
|    35348 |  2307 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35348 |  2308 | `	PH7_MemObjRelease(&sKey);` |
|    35348 |  2309 | `	return rc;` |
|        2 |  2310 |  |
|        - |  2311 | `/*` |
|        - |  2312 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2313 | ` * Return a pointer to the variable value on success.` |
|        - |  2314 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2315 | ` */` |
|  3669234 |  2316 | `static ph7_value * VmExtractMemObj(` |
|        - |  2317 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2318 | `	const SyString *pName, /* Variable name */` |
|        - |  2319 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2320 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2321 | `	)` |
|        2 |  2322 |  |
|  3669236 |  2323 | `	int bNullify = FALSE;` |
|        - |  2324 | `	SyHashEntry *pEntry;` |
|        - |  2325 | `	VmFrame *pFrame;` |
|        - |  2326 | `	ph7_value *pObj;` |
|        - |  2327 | `	sxu32 nIdx;` |
|        - |  2328 | `	sxi32 rc;` |
|        - |  2329 | `	/* Point to the top active frame */` |
|  3669236 |  2330 | `	pFrame = pVm->pFrame;` |
|  3669236 |  2331 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2332 | `	/* Perform the lookup */` |
|  3669236 |  2333 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2334 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2335 | `		pName = &sAnnon;` |
|        - |  2336 | `		/* Always nullify the object */` |
|      ! 0 |  2337 | `		bNullify = TRUE;` |
|      ! 0 |  2338 | `		bDup = FALSE;` |
|      ! 0 |  2339 | `	}` |
|        - |  2340 | `	/* Check the superglobals table first */` |
|  3669236 |  2341 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3669236 |  2342 | `	if( pEntry == 0 ){` |
|        - |  2343 | `		/* Query the top active frame */` |
|  3669190 |  2344 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3669190 |  2345 | `		if( pEntry == 0 ){` |
|   113314 |  2346 | `			char *zName = (char *)pName->zString;` |
|        - |  2347 | `			VmSlot sLocal;` |
|   113314 |  2348 | `			if( !bCreate ){` |
|        - |  2349 | `				/* Do not create the variable,return NULL instead */` |
|      982 |  2350 | `				return 0;` |
|        - |  2351 | `			}` |
|        - |  2352 | `			/* No such variable,automatically create a new one and install` |
|        - |  2353 | `			 * it in the current frame.` |
|        - |  2354 | `			 */` |
|   112334 |  2355 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   112334 |  2356 | `			if( pObj == 0 ){` |
|      ! 0 |  2357 | `				return 0;` |
|        - |  2358 | `			}` |
|   112334 |  2359 | `			nIdx = pObj->nIdx;` |
|   112334 |  2360 | `			if( bDup ){` |
|        - |  2361 | `				/* Duplicate name */` |
|      230 |  2362 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2363 | `				if( zName == 0 ){` |
|      ! 0 |  2364 | `					return 0;` |
|        - |  2365 | `				}` |
|      114 |  2366 | `			}` |
|        - |  2367 | `			/* Link to the top active VM frame */` |
|   112334 |  2368 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   112334 |  2369 | `			if( rc != SXRET_OK ){` |
|        - |  2370 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2371 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2372 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2373 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2374 | `				return 0;` |
|        - |  2375 | `			}` |
|   112334 |  2376 | `			if( pFrame->pParent != 0 ){` |
|        - |  2377 | `				/* Local variable */` |
|   105336 |  2378 | `				sLocal.nIdx = nIdx;` |
|   105336 |  2379 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52669 |  2380 | `			}else{` |
|        - |  2381 | `				/* Register in the $GLOBALS array */` |
|     7000 |  2382 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2383 | `			}` |
|        - |  2384 | `			/* Install in the reference table */` |
|   112334 |  2385 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2386 | `			/* Save object index */` |
|   112334 |  2387 | `			pObj->nIdx = nIdx;` |
|    56168 |  2388 | `		}else{` |
|        - |  2389 | `			/* Extract variable contents */` |
|  3555878 |  2390 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3555878 |  2391 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3555878 |  2392 | `			if( bNullify && pObj ){` |
|      ! 0 |  2393 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2394 | `			}` |
|        - |  2395 | `		}` |
|  1834216 |  2396 | `	}else{` |
|        - |  2397 | `		/* Superglobal */` |
|       48 |  2398 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       48 |  2399 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2400 | `	}` |
|  3668256 |  2401 | `	return pObj;` |
|  1834729 |  2402 |  |
|        - |  2403 | `/*` |
|        - |  2404 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2405 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2406 | ` */` |
|     3256 |  2407 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2408 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2409 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2410 | `	sxu32 nByte        /* zName length */` |
|        - |  2411 | `	)` |
|        2 |  2412 |  |
|        - |  2413 | `	SyHashEntry *pEntry;` |
|        - |  2414 | `	ph7_value *pValue;` |
|        - |  2415 | `	sxu32 nIdx;` |
|        - |  2416 | `	/* Query the superglobal table */` |
|     3258 |  2417 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3258 |  2418 | `	if( pEntry == 0 ){` |
|        - |  2419 | `		/* No such entry */` |
|      ! 0 |  2420 | `		return 0;` |
|        - |  2421 | `	}` |
|        - |  2422 | `	/* Extract the superglobal index in the global object pool */` |
|     3258 |  2423 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2424 | `	/* Extract the variable value  */` |
|     3258 |  2425 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3258 |  2426 | `	return pValue;` |
|     1630 |  2427 |  |
|        - |  2428 | `/*` |
|        - |  2429 | ` * Perform a raw hashmap insertion.` |
|        - |  2430 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2431 | ` */` |
|     3298 |  2432 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2433 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2434 | `	const char *zKey,   /* Entry key */` |
|        - |  2435 | `	int nKeylen,        /* zKey length*/` |
|        - |  2436 | `	const char *zData,  /* Entry data */` |
|        - |  2437 | `	int nLen            /* zData length */` |
|        - |  2438 | `	)` |
|        2 |  2439 |  |
|        - |  2440 | `	ph7_value sKey,sValue;` |
|        - |  2441 | `	sxi32 rc;` |
|     3300 |  2442 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3300 |  2443 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3300 |  2444 | `	if( zKey ){` |
|     3278 |  2445 | `		if( nKeylen < 0 ){` |
|     3196 |  2446 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1597 |  2447 | `		}` |
|     3278 |  2448 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1638 |  2449 | `	}` |
|     3300 |  2450 | `	if( zData ){` |
|     3300 |  2451 | `		if( nLen < 0 ){` |
|        - |  2452 | `			/* Compute length automatically */` |
|      198 |  2453 | `			nLen = (int)SyStrlen(zData);` |
|       99 |  2454 | `		}` |
|     3300 |  2455 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1649 |  2456 | `	}` |
|        - |  2457 | `	/* Perform the insertion */` |
|     3300 |  2458 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3300 |  2459 | `	PH7_MemObjRelease(&sKey);` |
|     3300 |  2460 | `	PH7_MemObjRelease(&sValue);` |
|     3300 |  2461 | `	return rc;` |
|        2 |  2462 |  |
|        - |  2463 | `/*` |
|        - |  2464 | ` * Configure a working virtual machine instance.` |
|        - |  2465 | ` *` |
|        - |  2466 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2467 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2468 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2469 | ` * The second argument to this function is an integer configuration option` |
|        - |  2470 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2471 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2472 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2473 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2474 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2475 | ` */` |
|    45744 |  2476 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2477 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2478 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2479 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2480 | `	)` |
|        2 |  2481 |  |
|    45746 |  2482 | `	sxi32 rc = SXRET_OK;` |
|    45746 |  2483 | `	switch(nOp){` |
|     1405 |  2484 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2812 |  2485 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2812 |  2486 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2487 | `		/* VM output consumer callback */` |
|        - |  2488 | `#ifdef UNTRUST` |
|        - |  2489 | `		if( xConsumer == 0 ){` |
|        - |  2490 | `			rc = SXERR_CORRUPT;` |
|        - |  2491 | `			break;` |
|        - |  2492 | `		}` |
|        - |  2493 | `#endif` |
|        - |  2494 | `		/* Install the output consumer */` |
|     2812 |  2495 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2812 |  2496 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2812 |  2497 | `		break;` |
|        - |  2498 | `							   }` |
|     1413 |  2499 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2500 | `		/* Import path */` |
|        - |  2501 | `		  const char *zPath;` |
|        - |  2502 | `		  SyString sPath;` |
|     2828 |  2503 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2504 | `#if defined(UNTRUST)` |
|        - |  2505 | `		  if( zPath == 0 ){` |
|        - |  2506 | `			  rc = SXERR_EMPTY;` |
|        - |  2507 | `			  break;` |
|        - |  2508 | `		  }` |
|        - |  2509 | `#endif` |
|     2828 |  2510 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2511 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2512 | `#ifdef __WINNT__` |
|        2 |  2513 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2514 | `#endif` |
|     5654 |  2515 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2516 | `		  /* Remove leading and trailing white spaces */` |
|     2828 |  2517 | `		  SyStringFullTrim(&sPath);` |
|     2828 |  2518 | `		  if( sPath.nByte > 0 ){` |
|        - |  2519 | `			  /* Store the path in the corresponding conatiner */` |
|     2828 |  2520 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1413 |  2521 | `		  }` |
|     2828 |  2522 | `		  break;` |
|        - |  2523 | `									 }` |
|     1416 |  2524 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2525 | `		/* Run-Time Error report */` |
|     2834 |  2526 | `		pVm->bErrReport = 1;` |
|     2834 |  2527 | `		break;` |
|      ! 0 |  2528 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2529 | `		/* Recursion depth */` |
|      ! 0 |  2530 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2531 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2532 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2533 | `		}` |
|      ! 0 |  2534 | `		break;` |
|        - |  2535 | `									   }` |
|      ! 0 |  2536 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2537 | `		/* VM output length in bytes */` |
|      ! 0 |  2538 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2539 | `#ifdef UNTRUST` |
|        - |  2540 | `		if( pOut == 0 ){` |
|        - |  2541 | `			rc = SXERR_CORRUPT;` |
|        - |  2542 | `			break;` |
|        - |  2543 | `		}` |
|        - |  2544 | `#endif` |
|      ! 0 |  2545 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2546 | `		break;` |
|        - |  2547 | `							   }` |
|        - |  2548 |  |
|    14160 |  2549 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2550 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2551 | `		/* Create a new superglobal/global variable */` |
|    28322 |  2552 | `		const char *zName = va_arg(ap,const char *);` |
|    28322 |  2553 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2554 | `		SyHashEntry *pEntry;` |
|        - |  2555 | `		ph7_value *pObj;` |
|        - |  2556 | `		sxu32 nByte;` |
|        - |  2557 | `		sxu32 nIdx;` |
|        - |  2558 | `#ifdef UNTRUST` |
|        - |  2559 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2560 | `			rc = SXERR_CORRUPT;` |
|        - |  2561 | `			break;` |
|        - |  2562 | `		}` |
|        - |  2563 | `#endif` |
|    28322 |  2564 | `		nByte = SyStrlen(zName);` |
|    28322 |  2565 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2566 | `			/* Check if the superglobal is already installed */` |
|    28322 |  2567 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14162 |  2568 | `		}else{` |
|        - |  2569 | `			/* Query the top active VM frame */` |
|      ! 0 |  2570 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2571 | `		}` |
|    28322 |  2572 | `		if( pEntry ){` |
|        - |  2573 | `			/* Variable already installed */` |
|      ! 0 |  2574 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2575 | `			/* Extract contents */` |
|      ! 0 |  2576 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2577 | `			if( pObj ){` |
|        - |  2578 | `				/* Overwrite old contents */` |
|      ! 0 |  2579 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2580 | `			}` |
|      ! 0 |  2581 | `		}else{` |
|        - |  2582 | `			/* Install a new variable */` |
|    28322 |  2583 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28322 |  2584 | `			if( pObj == 0 ){` |
|      ! 0 |  2585 | `				rc = SXERR_MEM;` |
|      ! 0 |  2586 | `				break;` |
|        - |  2587 | `			}` |
|    28322 |  2588 | `			nIdx = pObj->nIdx;` |
|        - |  2589 | `			/* Copy value */` |
|    28322 |  2590 | `			PH7_MemObjStore(pValue,pObj);` |
|    28322 |  2591 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2592 | `				/* Install the superglobal */` |
|    28322 |  2593 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14162 |  2594 | `			}else{` |
|        - |  2595 | `				/* Install in the current frame */` |
|      ! 0 |  2596 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2597 | `			}` |
|    28322 |  2598 | `			if( rc == SXRET_OK ){` |
|        - |  2599 | `				SyHashEntry *pRef;` |
|    28322 |  2600 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28322 |  2601 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14162 |  2602 | `				}else{` |
|      ! 0 |  2603 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2604 | `				}` |
|        - |  2605 | `				/* Install in the reference table */` |
|    28322 |  2606 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28322 |  2607 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2608 | `					/* Register in the $GLOBALS array */` |
|    28322 |  2609 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14160 |  2610 | `				}` |
|    14160 |  2611 | `			}` |
|        - |  2612 | `		}` |
|    28322 |  2613 | `		break;` |
|        - |  2614 | `									}` |
|     1597 |  2615 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2616 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2617 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2618 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2619 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2620 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2621 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3196 |  2622 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3196 |  2623 | `		const char *zValue = va_arg(ap,const char *);` |
|     3196 |  2624 | `		int nLen = va_arg(ap,int);` |
|        - |  2625 | `		ph7_hashmap *pMap;` |
|        - |  2626 | `		ph7_value *pValue;` |
|     3196 |  2627 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2628 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2629 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3195 |  2630 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2631 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2632 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3194 |  2633 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2634 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2635 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3194 |  2636 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2637 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2638 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3194 |  2639 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2640 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2641 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3194 |  2642 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2643 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2644 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2645 | `		}else{` |
|        - |  2646 | `			/* Extract the $_SERVER superglobal */` |
|     3194 |  2647 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2648 | `		}` |
|     3196 |  2649 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2650 | `			/* No such entry */` |
|      ! 0 |  2651 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2652 | `			break;` |
|        - |  2653 | `		}` |
|        - |  2654 | `		/* Point to the hashmap */` |
|     3196 |  2655 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2656 | `		/* Perform the insertion */` |
|     3196 |  2657 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3196 |  2658 | `		break;` |
|        - |  2659 | `								   }` |
|       11 |  2660 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2661 | `		/* Script arguments */` |
|       24 |  2662 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2663 | `		ph7_hashmap *pMap;` |
|        - |  2664 | `		ph7_value *pValue;` |
|        - |  2665 | `		sxu32 n;` |
|       24 |  2666 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2667 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2668 | `			break;` |
|        - |  2669 | `		}` |
|        - |  2670 | `		/* Extract the $argv array */` |
|       24 |  2671 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2672 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2673 | `			/* No such entry */` |
|      ! 0 |  2674 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2675 | `			break;` |
|        - |  2676 | `		}` |
|        - |  2677 | `		/* Point to the hashmap */` |
|       24 |  2678 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2679 | `		/* Perform the insertion */` |
|       24 |  2680 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2681 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2682 | `		if( rc == SXRET_OK ){` |
|       24 |  2683 | `			if( pMap->nEntry > 1 ){` |
|        - |  2684 | `				/* Append space separator first */` |
|       18 |  2685 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2686 | `			}` |
|       24 |  2687 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2688 | `		}` |
|       24 |  2689 | `		break;` |
|        - |  2690 | `								  }` |
|      ! 0 |  2691 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2692 | `		/* error_log() consumer */` |
|      ! 0 |  2693 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2694 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2695 | `		break;` |
|        - |  2696 | `										}` |
|      ! 0 |  2697 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2698 | `		/* Script return value */` |
|      ! 0 |  2699 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2700 | `#ifdef UNTRUST` |
|        - |  2701 | `		if( ppValue == 0 ){` |
|        - |  2702 | `			rc = SXERR_CORRUPT;` |
|        - |  2703 | `			break;` |
|        - |  2704 | `		}` |
|        - |  2705 | `#endif` |
|      ! 0 |  2706 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2707 | `		break;` |
|        - |  2708 | `								   }` |
|     2826 |  2709 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2710 | `		/* Register an IO stream device */` |
|     5654 |  2711 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2712 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8478 |  2713 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5654 |  2714 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2715 | `				/* Invalid stream */` |
|      ! 0 |  2716 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2717 | `				break;` |
|        - |  2718 | `		}` |
|     5654 |  2719 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2720 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2828 |  2721 | `			pVm->pDefStream = pStream;` |
|     1413 |  2722 | `		}` |
|        - |  2723 | `		/* Insert in the appropriate container */` |
|     5654 |  2724 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5654 |  2725 | `		break;` |
|        - |  2726 | `								  }` |
|       11 |  2727 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2728 | `		/* Point to the VM internal output consumer buffer */` |
|       22 |  2729 | `		const void **ppOut = va_arg(ap,const void **);` |
|       22 |  2730 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2731 | `#ifdef UNTRUST` |
|        - |  2732 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2733 | `			rc = SXERR_CORRUPT;` |
|        - |  2734 | `			break;` |
|        - |  2735 | `		}` |
|        - |  2736 | `#endif` |
|       22 |  2737 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       22 |  2738 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       22 |  2739 | `		break;` |
|        - |  2740 | `									   }` |
|       11 |  2741 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2742 | `		/* Raw HTTP request*/` |
|       22 |  2743 | `		const char *zRequest = va_arg(ap,const char *);` |
|       22 |  2744 | `		int nByte = va_arg(ap,int);` |
|       22 |  2745 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2746 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2747 | `			break;` |
|        - |  2748 | `		}` |
|       22 |  2749 | `		if( nByte < 0 ){` |
|        - |  2750 | `			/* Compute length automatically */` |
|      ! 0 |  2751 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2752 | `		}` |
|        - |  2753 | `		/* Process the request */` |
|       22 |  2754 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2755 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       22 |  2756 | `		if( rc == SXRET_OK ){` |
|       22 |  2757 | `			pVm->bHttpContext = 1;` |
|       11 |  2758 | `		}` |
|       22 |  2759 | `		break;` |
|        - |  2760 | `									}` |
|       11 |  2761 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2762 | `		/* Extract HTTP response status code */` |
|       22 |  2763 | `		int *pStatus = va_arg(ap, int *);` |
|       22 |  2764 | `		if( pStatus ){` |
|       22 |  2765 | `			*pStatus = pVm->iResponseStatus;` |
|       11 |  2766 | `		}` |
|       22 |  2767 | `		break;` |
|        - |  2768 | `										}` |
|       11 |  2769 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2770 | `		/* Iterate response headers via callback */` |
|        - |  2771 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       22 |  2772 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       22 |  2773 | `		void *pUserData = va_arg(ap, void *);` |
|       22 |  2774 | `		if( xCallback ){` |
|       22 |  2775 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       22 |  2776 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       34 |  2777 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2778 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2779 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2780 | `							   pUserData);` |
|       12 |  2781 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2782 | `					break;` |
|        - |  2783 | `				}` |
|        6 |  2784 | `			}` |
|       11 |  2785 | `		}` |
|       22 |  2786 | `		break;` |
|        - |  2787 | `										 }` |
|      ! 0 |  2788 | `	default:` |
|        - |  2789 | `		/* Unknown configuration option */` |
|      ! 0 |  2790 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2791 | `		break;` |
|        - |  2792 | `	}` |
|    45746 |  2793 | `	return rc;` |
|        2 |  2794 |  |
|        - |  2795 | `/* Forward declaration */` |
|        - |  2796 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2797 | `/*` |
|        - |  2798 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2799 | ` * format.` |
|        - |  2800 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2801 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2802 | ` * (STDOUT).` |
|        - |  2803 | ` */` |
|        2 |  2804 | `static sxi32 VmByteCodeDump(` |
|        - |  2805 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2806 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2807 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2808 | `	)` |
|        1 |  2809 |  |
|        - |  2810 | `	static const char zDump[] = {` |
|        - |  2811 | `		"====================================================\n"` |
|        - |  2812 | `		"PH7 VM Dump\n"` |
|        - |  2813 | `		"====================================================\n"` |
|        - |  2814 | `	};` |
|        - |  2815 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2816 | `	sxi32 rc = SXRET_OK;` |
|        - |  2817 | `	sxu32 n;` |
|        - |  2818 | `	/* Point to the PH7 instructions */` |
|        3 |  2819 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2820 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2821 | `	n = 0;` |
|        3 |  2822 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2823 | `	/* Dump instructions */` |
|        7 |  2824 | `	for(;;){` |
|       15 |  2825 | `		if( pInstr >= pEnd ){` |
|        - |  2826 | `			/* No more instructions */` |
|        3 |  2827 | `			break;` |
|        - |  2828 | `		}` |
|        - |  2829 | `		/* Format and call the consumer callback */` |
|       19 |  2830 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2831 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2832 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2833 | `		if( rc != SXRET_OK ){` |
|        - |  2834 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2835 | `			return rc;` |
|        - |  2836 | `		}` |
|       13 |  2837 | `		++n;` |
|       13 |  2838 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2839 | `	}` |
|        3 |  2840 | `	return rc;` |
|        2 |  2841 |  |
|        - |  2842 | `/* Forward declaration */` |
|        - |  2843 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2844 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2845 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2846 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2847 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2848 | `/*` |
|        - |  2849 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2850 | ` * consumer callback.` |
|        - |  2851 | ` */` |
|      600 |  2852 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2853 |  |
|      601 |  2854 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      601 |  2855 | `	sxi32 rc = SXRET_OK;` |
|        - |  2856 | `	/* Append a new line */` |
|        - |  2857 | `#ifdef __WINNT__` |
|        1 |  2858 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2859 | `#else` |
|      600 |  2860 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2861 | `#endif` |
|        - |  2862 | `	/* Invoke the output consumer callback */` |
|      601 |  2863 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      601 |  2864 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      601 |  2865 | `	return rc;` |
|        1 |  2866 |  |
|        - |  2867 | `/*` |
|        - |  2868 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2869 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2870 | ` * information.` |
|        - |  2871 | ` */` |
|      148 |  2872 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2873 |  |
|      150 |  2874 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2875 | `		ph7_value apArg[4];` |
|        - |  2876 | `		ph7_value *apArgPtr[4];` |
|        - |  2877 | `		ph7_value sResult;` |
|        - |  2878 | `		SyString sErr;` |
|        - |  2879 | `		/* Prepare arguments */` |
|       76 |  2880 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2881 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2882 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2883 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2884 | `		if( pFile ){` |
|       76 |  2885 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2886 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2887 | `		}else{` |
|      ! 0 |  2888 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2889 | `		}` |
|       76 |  2890 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2891 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2892 | `		/* Set up pointer array */` |
|       76 |  2893 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2894 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2895 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2896 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2897 | `		/* Call the handler */` |
|       76 |  2898 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2899 | `		/* Check return value */` |
|       76 |  2900 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2901 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2902 | `		}` |
|        - |  2903 | `		/* Release */` |
|       76 |  2904 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2905 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2906 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2907 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2908 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2909 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2910 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2911 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2912 | `	}` |
|        - |  2913 | `	/* No handler, always call error handler */` |
|       75 |  2914 | `	return TRUE;` |
|       76 |  2915 |  |
|      110 |  2916 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2917 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2918 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2919 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2920 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2921 | `	)` |
|        2 |  2922 |  |
|      112 |  2923 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2924 | `	SyString *pFile;` |
|        - |  2925 | `	char *zErr;` |
|      112 |  2926 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2927 | `	if( !pVm->bErrReport ){` |
|        - |  2928 | `		/* Don't bother reporting errors */` |
|        3 |  2929 | `		return SXRET_OK;` |
|        - |  2930 | `	}` |
|        - |  2931 | `	/* Reset the working buffer */` |
|      110 |  2932 | `	SyBlobReset(pWorker);` |
|        - |  2933 | `	/* Peek the processed file if available */` |
|      110 |  2934 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2935 | `	if( pFile ){` |
|        - |  2936 | `		/* Append file name */` |
|      110 |  2937 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2938 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2939 | `	}` |
|        - |  2940 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2941 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2942 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2943 | `	 * E_DEPRECATED). */` |
|      110 |  2944 | `	zErr = "Error:  ";` |
|      110 |  2945 | `	switch(iErr){` |
|       19 |  2946 | `	case PH7_CTX_WARNING:` |
|       40 |  2947 | `		zErr = "Warning:  ";` |
|       40 |  2948 | `		break;` |
|        6 |  2949 | `	case PH7_CTX_NOTICE:` |
|       14 |  2950 | `		zErr = "Notice:  ";` |
|       12 |  2951 | `		break;` |
|       29 |  2952 | `	default:` |
|        - |  2953 | `		/* keep iErr unchanged */` |
|       58 |  2954 | `		break;` |
|        - |  2955 | `	}` |
|      110 |  2956 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2957 | `	if( pFuncName ){` |
|        - |  2958 | `		/* Append function name first */` |
|       23 |  2959 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2960 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2961 | `	}` |
|      110 |  2962 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2963 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2964 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2965 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2966 | `	}` |
|      110 |  2967 | `	return rc;` |
|       57 |  2968 |  |
|        - |  2969 | `/*` |
|        - |  2970 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  2971 | ` *` |
|        - |  2972 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  2973 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  2974 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  2975 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  2976 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  2977 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  2978 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  2979 | ` * calling it from a VM op.` |
|        - |  2980 | ` */` |
|      ! 0 |  2981 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  2982 |  |
|      ! 0 |  2983 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  2984 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  2985 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  2986 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  2987 | `	return PH7_ABORT;` |
|      ! 0 |  2988 |  |
|        - |  2989 | `/*` |
|        - |  2990 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  2991 | ` */` |
|      ! 0 |  2992 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  2993 |  |
|      ! 0 |  2994 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  2995 |  |
|        - |  2996 | `/*` |
|        - |  2997 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2998 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2999 | ` * information.` |
|        - |  3000 | ` */` |
|       40 |  3001 | `static sxi32 VmThrowErrorAp(` |
|        - |  3002 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3003 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  3004 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  3005 | `	const char *zFormat, /* Format message */` |
|        - |  3006 | `	va_list ap           /* Variable list of arguments */` |
|        - |  3007 | `	)` |
|        2 |  3008 |  |
|       42 |  3009 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  3010 | `	SyBlob sMsg;` |
|        - |  3011 | `	SyString *pFile;` |
|        - |  3012 | `	char *zErr;` |
|       42 |  3013 | `	sxi32 rc = SXRET_OK;` |
|       42 |  3014 | `	if( !pVm->bErrReport ){` |
|        - |  3015 | `		/* Don't bother reporting errors */` |
|      ! 0 |  3016 | `		return SXRET_OK;` |
|        - |  3017 | `	}` |
|        - |  3018 | `	/* Reset the working buffer */` |
|       42 |  3019 | `	SyBlobReset(pWorker);` |
|        - |  3020 | `	/* Peek the processed file if available */` |
|       42 |  3021 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  3022 | `	if( pFile ){` |
|        - |  3023 | `		/* Append file name */` |
|       42 |  3024 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  3025 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  3026 | `	}` |
|        - |  3027 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  3028 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  3029 | `	 * the correct errno value. */` |
|       42 |  3030 | `	zErr = "Error:  ";` |
|       42 |  3031 | `	switch(iErr){` |
|        4 |  3032 | `	case PH7_CTX_WARNING:` |
|        9 |  3033 | `		zErr = "Warning:  ";` |
|        9 |  3034 | `		break;` |
|        3 |  3035 | `	case PH7_CTX_NOTICE:` |
|        7 |  3036 | `		zErr = "Notice:  ";` |
|        6 |  3037 | `		break;` |
|       13 |  3038 | `	default:` |
|        - |  3039 | `		/* do not change iErr */` |
|       26 |  3040 | `		break;` |
|        - |  3041 | `	}` |
|       42 |  3042 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  3043 | `	if( pFuncName ){` |
|        - |  3044 | `		/* Append function name first */` |
|       26 |  3045 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  3046 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  3047 | `	}` |
|        - |  3048 | `	/* Format the raw message */` |
|       42 |  3049 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  3050 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  3051 | `	/* Check if a user error handler is installed */` |
|       42 |  3052 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  3053 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  3054 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  3055 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  3056 | `	}` |
|       42 |  3057 | `	SyBlobRelease(&sMsg);` |
|       42 |  3058 | `	return rc;` |
|       22 |  3059 |  |
|        - |  3060 | `/*` |
|        - |  3061 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  3062 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  3063 | ` * possible.` |
|        - |  3064 | ` */` |
|       38 |  3065 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  3066 |  |
|        - |  3067 | `	ph7_class *pClass;` |
|       39 |  3068 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  3069 | `	ph7_class_instance *pThis;` |
|        - |  3070 | `	ph7_class_method *pCons;` |
|        - |  3071 | `	ph7_value sArg;` |
|        - |  3072 | `	ph7_value *apArg[1];` |
|        - |  3073 | `	SyBlob sMsg;` |
|        - |  3074 | `	SyString sMsgStr;` |
|        - |  3075 | `	VmFrame *pFrame;` |
|        - |  3076 | `	sxi32 rc;` |
|       39 |  3077 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  3078 | `	if( pClass == 0 ){` |
|      ! 0 |  3079 | `		return PH7_ABORT;` |
|        - |  3080 | `	}` |
|       39 |  3081 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  3082 | `	if( pThis == 0 ){` |
|      ! 0 |  3083 | `		return PH7_ABORT;` |
|        - |  3084 | `	}` |
|       39 |  3085 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3086 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3087 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  3088 | `	{` |
|       39 |  3089 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  3090 | `		if( pOwner ){` |
|       39 |  3091 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  3092 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  3093 | `		}else{` |
|      ! 0 |  3094 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3095 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3096 | `		}` |
|        - |  3097 | `	}` |
|       39 |  3098 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  3099 | `	if( pCons ){` |
|       39 |  3100 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  3101 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  3102 | `		apArg[0] = &sArg;` |
|       39 |  3103 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  3104 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  3105 | `	}` |
|       39 |  3106 | `	SyBlobRelease(&sMsg);` |
|       39 |  3107 | `	pFrame = pVm->pFrame;` |
|       39 |  3108 | `	if( pFrame ){` |
|       39 |  3109 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  3110 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  3111 | `	}` |
|       39 |  3112 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  3113 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  3114 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3115 | `		return PH7_ABORT;` |
|        - |  3116 | `	}` |
|       39 |  3117 | `	return PH7_EXCEPTION;` |
|       20 |  3118 |  |
|        - |  3119 |  |
|        - |  3120 | `/*` |
|        - |  3121 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  3122 | ` */` |
|        4 |  3123 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  3124 |  |
|        - |  3125 | `	ph7_class *pErrClass;` |
|        - |  3126 | `	ph7_class_instance *pThis;` |
|        - |  3127 | `	ph7_class_method *pCons;` |
|        - |  3128 | `	ph7_value sArg;` |
|        - |  3129 | `	ph7_value *apArg[1];` |
|        - |  3130 | `	SyBlob sMsg;` |
|        - |  3131 | `	SyString sMsgStr;` |
|        - |  3132 | `	VmFrame *pFrame;` |
|        - |  3133 | `	sxi32 rc;` |
|        5 |  3134 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  3135 | `	if( pErrClass == 0 ){` |
|      ! 0 |  3136 | `		return PH7_ABORT;` |
|        - |  3137 | `	}` |
|        5 |  3138 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  3139 | `	if( pThis == 0 ){` |
|      ! 0 |  3140 | `		return PH7_ABORT;` |
|        - |  3141 | `	}` |
|        5 |  3142 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3143 | `	{` |
|        5 |  3144 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  3145 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  3146 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  3147 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  3148 | `	}` |
|        5 |  3149 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3150 | `	if( pCons ){` |
|        5 |  3151 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  3152 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  3153 | `		apArg[0] = &sArg;` |
|        5 |  3154 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  3155 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3156 | `	}` |
|        5 |  3157 | `	SyBlobRelease(&sMsg);` |
|        5 |  3158 | `	pFrame = pVm->pFrame;` |
|        5 |  3159 | `	if( pFrame ){` |
|        5 |  3160 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3161 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3162 | `	}` |
|        5 |  3163 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  3164 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3165 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3166 | `		return PH7_ABORT;` |
|        - |  3167 | `	}` |
|        5 |  3168 | `	return PH7_EXCEPTION;` |
|        3 |  3169 |  |
|        - |  3170 |  |
|        - |  3171 | `/*` |
|        - |  3172 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  3173 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  3174 | ` * For class types, instanceof is verified.` |
|        - |  3175 | ` *` |
|        - |  3176 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  3177 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  3178 | ` */` |
|        - |  3179 | `/*` |
|        - |  3180 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  3181 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  3182 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  3183 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  3184 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  3185 | ` */` |
|       20 |  3186 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  3187 |  |
|        - |  3188 | `	const char *z, *zEnd, *zTail;` |
|        - |  3189 | `	sxu32 n;` |
|        - |  3190 | `	sxu8 bReal;` |
|        - |  3191 | `	sxi32 rc;` |
|       22 |  3192 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3193 | `		return 0;` |
|        - |  3194 | `	}` |
|       22 |  3195 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  3196 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  3197 | `	zEnd = z + n;` |
|       22 |  3198 | `	if( n == 0 ){` |
|      ! 0 |  3199 | `		return 0;` |
|        - |  3200 | `	}` |
|       22 |  3201 | `	zTail = 0;` |
|       22 |  3202 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  3203 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  3204 | `		return 0;` |
|        - |  3205 | `	}` |
|        - |  3206 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  3207 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3208 | `		zTail++;` |
|      ! 0 |  3209 | `	}` |
|       16 |  3210 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  3211 |  |
|        - |  3212 |  |
|        - |  3213 | `/*` |
|        - |  3214 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  3215 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  3216 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  3217 | ` *   0 if it's not strictly numeric.` |
|        - |  3218 | ` */` |
|       16 |  3219 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  3220 |  |
|        - |  3221 | `	const char *z, *zEnd, *zTail;` |
|        - |  3222 | `	sxu32 n;` |
|       18 |  3223 | `	sxu8 bReal = 0;` |
|        - |  3224 | `	sxi32 rc;` |
|       18 |  3225 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3226 | `		return 0;` |
|        - |  3227 | `	}` |
|       18 |  3228 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  3229 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  3230 | `	zEnd = z + n;` |
|       18 |  3231 | `	if( n == 0 ) return 0;` |
|       18 |  3232 | `	zTail = 0;` |
|       18 |  3233 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  3234 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  3235 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  3236 | `	if( zTail != zEnd ) return 0;` |
|       15 |  3237 | `	return bReal ? 2 : 1;` |
|       10 |  3238 |  |
|        - |  3239 |  |
|        - |  3240 | `/*` |
|        - |  3241 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  3242 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  3243 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  3244 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  3245 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  3246 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  3247 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  3248 | ` * throw.` |
|        - |  3249 | ` *` |
|        - |  3250 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  3251 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  3252 | ` */` |
|       98 |  3253 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  3254 |  |
|        - |  3255 | `	sxu32 i;` |
|        - |  3256 | `	ph7_type_alt *aAlts;` |
|        - |  3257 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  3258 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  3259 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3260 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  3261 | `	}` |
|       88 |  3262 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  3263 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  3264 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  3265 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  3266 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  3267 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  3268 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  3269 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  3270 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  3271 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  3272 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  3273 | `	}` |
|        - |  3274 | `	/* Object handling */` |
|       88 |  3275 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  3276 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  3277 | `		if( bHasClassAlt ){` |
|       14 |  3278 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  3279 | `			ph7_class *pSelfNow = 0;` |
|       14 |  3280 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  3281 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  3282 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  3283 | `			}` |
|       26 |  3284 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  3285 | `				ph7_class *pExpected;` |
|        - |  3286 | `				SyString *pCN;` |
|       22 |  3287 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  3288 | `				pCN = &aAlts[i].sClass;` |
|       22 |  3289 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3290 | `					pExpected = pSelfNow;` |
|       22 |  3291 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3292 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3293 | `				}else{` |
|       22 |  3294 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3295 | `				}` |
|       22 |  3296 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  3297 | `					return SXRET_OK;` |
|        - |  3298 | `				}` |
|        8 |  3299 | `			}` |
|        2 |  3300 | `		}` |
|        9 |  3301 | `		return SXERR_INVALID;` |
|        - |  3302 | `	}` |
|        - |  3303 | `	/* Array handling */` |
|       72 |  3304 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3305 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3306 | `	}` |
|        - |  3307 | `	/* Scalar handling — exact match first */` |
|       66 |  3308 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  3309 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3310 | `	}` |
|       42 |  3311 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3312 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3313 | `	}` |
|       38 |  3314 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  3315 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3316 | `	}` |
|       18 |  3317 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3318 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3319 | `	}` |
|       18 |  3320 | `	if( bStrict ){` |
|        - |  3321 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3322 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3323 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3324 | `			return SXRET_OK;` |
|        - |  3325 | `		}` |
|      ! 0 |  3326 | `		return SXERR_INVALID;` |
|        - |  3327 | `	}` |
|        - |  3328 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3329 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3330 | `	 * to match PHP's union RFC. */` |
|        - |  3331 | `	{` |
|       18 |  3332 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3333 | `		if( bHasInt ){` |
|        - |  3334 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3335 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3336 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3337 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3338 | `				return SXRET_OK;` |
|        - |  3339 | `			}` |
|       18 |  3340 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3341 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3342 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3343 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3344 | `					return SXRET_OK;` |
|        - |  3345 | `				}` |
|      ! 0 |  3346 | `			}` |
|       18 |  3347 | `			if( kind == 1 ){` |
|        9 |  3348 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3349 | `				return SXRET_OK;` |
|        - |  3350 | `			}` |
|        4 |  3351 | `		}` |
|       10 |  3352 | `		if( bHasFloat ){` |
|       10 |  3353 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3354 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3355 | `				return SXRET_OK;` |
|        - |  3356 | `			}` |
|       10 |  3357 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3358 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3359 | `				return SXRET_OK;` |
|        - |  3360 | `			}` |
|        1 |  3361 | `		}` |
|        3 |  3362 | `		if( bHasString ){` |
|      ! 0 |  3363 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3364 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3365 | `				return SXRET_OK;` |
|        - |  3366 | `			}` |
|      ! 0 |  3367 | `		}` |
|        3 |  3368 | `		if( bHasBool ){` |
|      ! 0 |  3369 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3370 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3371 | `				return SXRET_OK;` |
|        - |  3372 | `			}` |
|      ! 0 |  3373 | `		}` |
|        - |  3374 | `	}` |
|        3 |  3375 | `	return SXERR_INVALID;` |
|       51 |  3376 |  |
|        - |  3377 |  |
|        - |  3378 | `/*` |
|        - |  3379 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3380 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3381 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3382 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3383 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3384 | ` */` |
|       36 |  3385 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3386 |  |
|       38 |  3387 | `	if( bStrict ){` |
|        - |  3388 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3389 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3390 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3391 | `			return SXRET_OK;` |
|        - |  3392 | `		}` |
|       10 |  3393 | `		return SXERR_INVALID;` |
|        - |  3394 | `	}` |
|        - |  3395 | `	{` |
|       28 |  3396 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3397 | `		if( xCast ) xCast(pVal);` |
|        - |  3398 | `	}` |
|       28 |  3399 | `	return SXRET_OK;` |
|       20 |  3400 |  |
|        - |  3401 |  |
|        - |  3402 | `/*` |
|        - |  3403 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3404 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3405 | ` *` |
|        - |  3406 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3407 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3408 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3409 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3410 | ` */` |
|       10 |  3411 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3412 |  |
|       12 |  3413 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       12 |  3414 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       12 |  3415 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       12 |  3416 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       12 |  3417 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        5 |  3418 | `		}` |
|       12 |  3419 | `		zBuf[nCopy] = 0;` |
|       12 |  3420 | `		return zBuf;` |
|        - |  3421 | `	}` |
|      ! 0 |  3422 | `	switch( nType ){` |
|      ! 0 |  3423 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3424 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3425 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3426 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3427 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3428 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3429 | `		default:             return "scalar";` |
|        - |  3430 | `	}` |
|        7 |  3431 |  |
|        - |  3432 |  |
|        - |  3433 | `/*` |
|        - |  3434 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3435 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3436 | ` */` |
|       18 |  3437 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3438 |  |
|       19 |  3439 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3440 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3441 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3442 | `	return zBuf;` |
|        1 |  3443 |  |
|        - |  3444 |  |
|    14644 |  3445 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3446 |  |
|        - |  3447 | `	SyHashEntry *pSlot;` |
|        - |  3448 | `	VmClassAttr *pVmAttr;` |
|        - |  3449 | `	ph7_class_attr *pAttr;` |
|    14646 |  3450 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14646 |  3451 | `	if( pSlot == 0 ){` |
|    14438 |  3452 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3453 | `	}` |
|      210 |  3454 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3455 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3456 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3457 | `		return SXRET_OK;` |
|        - |  3458 | `	}` |
|        - |  3459 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3460 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3461 | `	 * matching PHP's documented behavior. */` |
|      210 |  3462 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3463 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3464 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3465 |  |
|       16 |  3466 | `		if( rc == SXRET_OK ){` |
|        9 |  3467 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3468 | `			return SXRET_OK;` |
|        - |  3469 | `		}` |
|        7 |  3470 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3471 | `			char zBuf[128];` |
|        4 |  3472 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3473 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3474 | `		}` |
|        5 |  3475 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3476 | `	}` |
|        - |  3477 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3478 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3479 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3480 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3481 | `			return SXRET_OK;` |
|        - |  3482 | `		}` |
|        3 |  3483 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3484 | `	}` |
|        - |  3485 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3486 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3487 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3488 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3489 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3490 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3491 | `			return SXRET_OK;` |
|        - |  3492 | `		}` |
|        7 |  3493 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3494 | `	}` |
|      174 |  3495 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3496 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3497 | `		 * currently active on the self-stack. */` |
|       26 |  3498 | `		ph7_class *pExpected = 0;` |
|       26 |  3499 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3500 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3501 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3502 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3503 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3504 | `		}` |
|       26 |  3505 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3506 | `			pExpected = pSelfNow;` |
|       24 |  3507 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3508 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3509 | `		}else{` |
|       22 |  3510 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3511 | `		}` |
|       26 |  3512 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3513 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3514 | `		}` |
|       26 |  3515 | `		if( pExpected ){` |
|       22 |  3516 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3517 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3518 | `				char zBuf[128];` |
|        7 |  3519 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3520 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3521 | `			}` |
|        8 |  3522 | `		}` |
|       22 |  3523 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3524 | `		return SXRET_OK;` |
|        - |  3525 | `	}` |
|        - |  3526 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3527 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3528 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3529 | `		char zBuf[128];` |
|       10 |  3530 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3531 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3532 | `	}` |
|      144 |  3533 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3534 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3535 | `		if( xCast ){` |
|        - |  3536 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3537 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3538 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3539 | `			}` |
|       24 |  3540 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3541 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3542 | `			}` |
|        - |  3543 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3544 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3545 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3546 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3547 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3548 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3549 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3550 | `			}` |
|       12 |  3551 | `			xCast(pValue);` |
|        5 |  3552 | `		}` |
|        5 |  3553 | `	}` |
|      130 |  3554 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3555 | `	return SXRET_OK;` |
|     7324 |  3556 |  |
|        - |  3557 |  |
|        - |  3558 | `/*` |
|        - |  3559 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3560 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3561 | ` * information.` |
|        - |  3562 | ` * ------------------------------------` |
|        - |  3563 | ` * Simple boring wrapper function.` |
|        - |  3564 | ` * ------------------------------------` |
|        - |  3565 | ` */` |
|       16 |  3566 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3567 |  |
|        - |  3568 | `	va_list ap;` |
|        - |  3569 | `	sxi32 rc;` |
|       17 |  3570 | `	va_start(ap,zFormat);` |
|       17 |  3571 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3572 | `	va_end(ap);` |
|       17 |  3573 | `	return rc;` |
|        1 |  3574 |  |
|        - |  3575 | `/*` |
|        - |  3576 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3577 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3578 | ` */` |
|       36 |  3579 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3580 |  |
|        - |  3581 | `	ph7_class *pClass;` |
|        - |  3582 | `	ph7_class_instance *pThis;` |
|        - |  3583 | `	ph7_class_method *pCons;` |
|        - |  3584 | `	ph7_value sArg;` |
|        - |  3585 | `	ph7_value *apArg[1];` |
|        - |  3586 | `	SyBlob sMsg;` |
|        - |  3587 | `	SyString sMsgStr;` |
|        - |  3588 | `	VmFrame *pFrame;` |
|        - |  3589 | `	sxi32 rc;` |
|       38 |  3590 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       38 |  3591 | `	if( pClass == 0 ){` |
|      ! 0 |  3592 | `		return PH7_ABORT;` |
|        - |  3593 | `	}` |
|       38 |  3594 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       38 |  3595 | `	if( pThis == 0 ){` |
|      ! 0 |  3596 | `		return PH7_ABORT;` |
|        - |  3597 | `	}` |
|       38 |  3598 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       38 |  3599 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       18 |  3600 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       38 |  3601 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       38 |  3602 | `	if( pCons ){` |
|       38 |  3603 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       38 |  3604 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       38 |  3605 | `		apArg[0] = &sArg;` |
|       38 |  3606 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       38 |  3607 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  3608 | `	}` |
|       38 |  3609 | `	SyBlobRelease(&sMsg);` |
|       38 |  3610 | `	pFrame = pVm->pFrame;` |
|       38 |  3611 | `	if( pFrame ){` |
|       38 |  3612 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  3613 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  3614 | `	}` |
|       38 |  3615 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  3616 | `	PH7_ClassInstanceUnref(pThis);` |
|       38 |  3617 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3618 | `		return PH7_ABORT;` |
|        - |  3619 | `	}` |
|       34 |  3620 | `	return PH7_EXCEPTION;` |
|       20 |  3621 |  |
|        - |  3622 | `/*` |
|        - |  3623 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3624 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3625 | ` */` |
|        6 |  3626 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3627 |  |
|        - |  3628 | `	ph7_class *pClass;` |
|        - |  3629 | `	ph7_class_instance *pThis;` |
|        - |  3630 | `	ph7_class_method *pCons;` |
|        - |  3631 | `	ph7_value sArg;` |
|        - |  3632 | `	ph7_value *apArg[1];` |
|        - |  3633 | `	SyBlob sMsg;` |
|        - |  3634 | `	SyString sMsgStr;` |
|        - |  3635 | `	VmFrame *pFrame;` |
|        - |  3636 | `	sxi32 rc;` |
|        7 |  3637 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3638 | `	if( pClass == 0 ){` |
|      ! 0 |  3639 | `		return PH7_ABORT;` |
|        - |  3640 | `	}` |
|        7 |  3641 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3642 | `	if( pThis == 0 ){` |
|      ! 0 |  3643 | `		return PH7_ABORT;` |
|        - |  3644 | `	}` |
|        7 |  3645 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3646 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3647 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3648 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3649 | `	if( pCons ){` |
|        7 |  3650 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3651 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3652 | `		apArg[0] = &sArg;` |
|        7 |  3653 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3654 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3655 | `	}` |
|        7 |  3656 | `	SyBlobRelease(&sMsg);` |
|        7 |  3657 | `	pFrame = pVm->pFrame;` |
|        7 |  3658 | `	if( pFrame ){` |
|        7 |  3659 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3660 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3661 | `	}` |
|        7 |  3662 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3663 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3664 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3665 | `		return PH7_ABORT;` |
|        - |  3666 | `	}` |
|      ! 0 |  3667 | `	return PH7_EXCEPTION;` |
|        4 |  3668 |  |
|        - |  3669 | `/*` |
|        - |  3670 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3671 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3672 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3673 | ` */` |
|       16 |  3674 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3675 |  |
|       17 |  3676 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3677 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3678 | `	}` |
|       13 |  3679 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3680 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3681 | `		if( pThis && pThis->pClass ){` |
|        5 |  3682 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3683 | `			sxu32 n = pName->nByte;` |
|        5 |  3684 | `			if( n >= nBuf ){` |
|      ! 0 |  3685 | `				n = nBuf - 1;` |
|      ! 0 |  3686 | `			}` |
|        5 |  3687 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3688 | `			zBuf[n] = 0;` |
|        5 |  3689 | `			return zBuf;` |
|        - |  3690 | `		}` |
|      ! 0 |  3691 | `		return "object";` |
|        - |  3692 | `	}` |
|        9 |  3693 | `	return ph7_type_name(pVal);` |
|        9 |  3694 |  |
|        - |  3695 | `/*` |
|        - |  3696 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3697 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3698 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3699 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3700 | ` */` |
|       16 |  3701 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3702 |  |
|        - |  3703 | `	ph7_class *pClass;` |
|        - |  3704 | `	ph7_class_instance *pThis;` |
|        - |  3705 | `	ph7_class_method *pCons;` |
|        - |  3706 | `	ph7_value sArg;` |
|        - |  3707 | `	ph7_value *apArg[1];` |
|        - |  3708 | `	SyBlob sMsg;` |
|        - |  3709 | `	SyString sMsgStr;` |
|        - |  3710 | `	VmFrame *pFrame;` |
|        - |  3711 | `	sxi32 rc;` |
|       17 |  3712 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3713 | `	char zNameBuf[64];` |
|       17 |  3714 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3715 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3716 | `	if( pClass == 0 ){` |
|      ! 0 |  3717 | `		return PH7_ABORT;` |
|        - |  3718 | `	}` |
|       17 |  3719 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3720 | `	if( pThis == 0 ){` |
|      ! 0 |  3721 | `		return PH7_ABORT;` |
|        - |  3722 | `	}` |
|       17 |  3723 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3724 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3725 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3726 | `	if( pCons ){` |
|       17 |  3727 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3728 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3729 | `		apArg[0] = &sArg;` |
|       17 |  3730 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3731 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3732 | `	}` |
|       17 |  3733 | `	SyBlobRelease(&sMsg);` |
|       17 |  3734 | `	pFrame = pVm->pFrame;` |
|       17 |  3735 | `	if( pFrame ){` |
|       17 |  3736 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3737 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3738 | `	}` |
|       17 |  3739 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3740 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3741 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3742 | `		return PH7_ABORT;` |
|        - |  3743 | `	}` |
|       17 |  3744 | `	return PH7_EXCEPTION;` |
|        9 |  3745 |  |
|        - |  3746 | `/*` |
|        - |  3747 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3748 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3749 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3750 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3751 | ` */` |
|        - |  3752 | `/*` |
|        - |  3753 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3754 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3755 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3756 | ` */` |
|       24 |  3757 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3758 |  |
|        - |  3759 | `	sxu32 nCopy;` |
|       26 |  3760 | `	if( nBuf == 0 ) return "";` |
|       26 |  3761 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3762 | `		zBuf[0] = 0;` |
|      ! 0 |  3763 | `		return zBuf;` |
|        - |  3764 | `	}` |
|       26 |  3765 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3766 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3767 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3768 | `	zBuf[nCopy] = 0;` |
|       26 |  3769 | `	return zBuf;` |
|       14 |  3770 |  |
|        - |  3771 |  |
|      396 |  3772 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3773 |  |
|      398 |  3774 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3775 | `	const char *zGiven;` |
|        - |  3776 | `	char zBuf[128];` |
|        - |  3777 | `	char zTypeBuf[128];` |
|        - |  3778 | `	/* Untyped function: no enforcement. */` |
|      398 |  3779 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3780 | `		return SXRET_OK;` |
|        - |  3781 | `	}` |
|        - |  3782 | `	/* void return type: the function must not produce a value. */` |
|      398 |  3783 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3784 | `		if( pValue == 0 ){` |
|      134 |  3785 | `			return SXRET_OK;` |
|        - |  3786 | `		}` |
|        - |  3787 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3788 | `		 * still counts as "returned a value" here. */` |
|        3 |  3789 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3790 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3791 | `	}` |
|        - |  3792 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3793 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3794 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      264 |  3795 | `	if( pValue == 0 ){` |
|      ! 0 |  3796 | `		const char *zExpected = "value";` |
|      ! 0 |  3797 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3798 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3799 | `		}` |
|      ! 0 |  3800 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3801 | `	}` |
|        - |  3802 | ``	/* `mixed` accepts any explicitly returned value, including null. It is`` |
|        - |  3803 | `	 * parsed as a class-name atom (SXU32_HIGH, sReturnClass = "mixed") since` |
|        - |  3804 | `	 * it is not a scalar keyword, so short-circuit it here before the null /` |
|        - |  3805 | `	 * class-type checks below — which would otherwise demand an object. */` |
|      272 |  3806 | `	if( pFunc->nReturnType == SXU32_HIGH` |
|      143 |  3807 | `	 && pFunc->sReturnClass.nByte == 5` |
|       24 |  3808 | `	 && SyStrnicmp(pFunc->sReturnClass.zString,"mixed",5) == 0 ){` |
|       21 |  3809 | `		return SXRET_OK;` |
|        - |  3810 | `	}` |
|        - |  3811 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3812 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3813 | `	 * bNullable=0 here. */` |
|      244 |  3814 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3815 | `		sxi32 rcU;` |
|      ! 0 |  3816 | `		int bNullable = 0;` |
|      ! 0 |  3817 | `		const char *zExpected = "union";` |
|        - |  3818 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3819 | `		{` |
|        - |  3820 | `			sxu32 i;` |
|      ! 0 |  3821 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3822 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3823 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3824 | `			}` |
|        - |  3825 | `		}` |
|      ! 0 |  3826 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3827 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3828 | `			return SXRET_OK;` |
|        - |  3829 | `		}` |
|      ! 0 |  3830 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3831 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3832 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3833 | `			zGiven = "null";` |
|      ! 0 |  3834 | `		}else{` |
|      ! 0 |  3835 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3836 | `		}` |
|      ! 0 |  3837 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3838 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3839 | `		}` |
|      ! 0 |  3840 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3841 | `	}` |
|        - |  3842 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3843 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3844 | `	 * it into the TypeError message. */` |
|      244 |  3845 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3846 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3847 | `		const char *zExpected;` |
|        - |  3848 | `		ph7_class *pExpected;` |
|        6 |  3849 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3850 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3851 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3852 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3853 | `		}` |
|        6 |  3854 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3855 | `			pExpected = pSelfNow;` |
|        4 |  3856 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3857 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3858 | `		}else{` |
|        3 |  3859 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3860 | `		}` |
|        6 |  3861 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3862 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3863 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3864 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3865 | `		}` |
|        6 |  3866 | `		if( pExpected ){` |
|        6 |  3867 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3868 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3869 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3870 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3871 | `			}` |
|        2 |  3872 | `		}` |
|        6 |  3873 | `		return SXRET_OK;` |
|        - |  3874 | `	}` |
|        - |  3875 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3876 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3877 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3878 | `	 * via the type-text leading '?'. */` |
|      240 |  3879 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3880 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3881 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3882 | `			return SXRET_OK;` |
|        - |  3883 | `		}` |
|      ! 0 |  3884 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3885 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3886 | `			"null");` |
|        - |  3887 | `	}` |
|        - |  3888 | `	/* Exact match? Done. */` |
|      234 |  3889 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3890 | `		return SXRET_OK;` |
|        - |  3891 | `	}` |
|        - |  3892 | `	/* Object->scalar is never compatible. */` |
|        8 |  3893 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3894 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3895 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3896 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3897 | `			zGiven);` |
|        - |  3898 | `	}` |
|        - |  3899 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3900 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3901 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3902 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3903 | `			ph7_type_name(pValue));` |
|        - |  3904 | `	}` |
|        - |  3905 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3906 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3907 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3908 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3909 | `	if( !bStrict` |
|        5 |  3910 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3911 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3912 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3913 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3914 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3915 | `			"string");` |
|        - |  3916 | `	}` |
|        6 |  3917 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3918 | `		return SXRET_OK;` |
|        - |  3919 | `	}` |
|        4 |  3920 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3921 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3922 | `		ph7_type_name(pValue));` |
|      200 |  3923 |  |
|        - |  3924 | `/*` |
|        - |  3925 | ` * Report a fatal named-argument error.` |
|        - |  3926 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3927 | ` */` |
|        6 |  3928 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3929 |  |
|        7 |  3930 | `	const char *zFunc = 0;` |
|        7 |  3931 | `	int nFunc = 0;` |
|        7 |  3932 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3933 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3934 |  |
|        - |  3935 | `/*` |
|        - |  3936 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3937 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3938 | ` * information.` |
|        - |  3939 | ` * ------------------------------------` |
|        - |  3940 | ` * Simple boring wrapper function.` |
|        - |  3941 | ` * ------------------------------------` |
|        - |  3942 | ` */` |
|       24 |  3943 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3944 |  |
|        - |  3945 | `	sxi32 rc;` |
|       26 |  3946 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3947 | `	return rc;` |
|        2 |  3948 |  |
|        - |  3949 | `/*` |
|        - |  3950 | ` * Resolve function context from the current frame.` |
|        - |  3951 | ` */` |
|     1018 |  3952 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3953 |  |
|        - |  3954 | `	VmFrame *pFrame;` |
|        - |  3955 | `	ph7_vm_func *pFunc;` |
|     1019 |  3956 | `	*pzFuncName = 0;` |
|     1019 |  3957 | `	*pnFuncLen = 0;` |
|     1019 |  3958 | `	pFrame = pVm->pFrame;` |
|     1019 |  3959 | `	if( pFrame == 0 ){` |
|      ! 0 |  3960 | `		return;` |
|        - |  3961 | `	}` |
|     1019 |  3962 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3963 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3964 | `		return;` |
|        - |  3965 | `	}` |
|       25 |  3966 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3967 | `	if( pFunc == 0 ){` |
|      ! 0 |  3968 | `		return;` |
|        - |  3969 | `	}` |
|       25 |  3970 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3971 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3972 |  |
|        - |  3973 | `/*` |
|        - |  3974 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3975 | ` */` |
|      524 |  3976 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3977 |  |
|        - |  3978 | `	SyBlob sOut;` |
|        - |  3979 | `	SyString *pFile;` |
|      525 |  3980 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3981 | `		return PH7_OK;` |
|        - |  3982 | `	}` |
|      525 |  3983 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3984 | `		zClass = "Exception";` |
|      ! 0 |  3985 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3986 | `	}` |
|      525 |  3987 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3988 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3989 | `	}` |
|      525 |  3990 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3991 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3992 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3993 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3994 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3995 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3996 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3997 | `	}` |
|      525 |  3998 | `	if( pFile ){` |
|      525 |  3999 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  4000 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4001 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  4002 | `	}` |
|      525 |  4003 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  4004 | `	if( pFile ){` |
|      525 |  4005 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  4006 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4007 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  4008 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  4009 | `		}else{` |
|      501 |  4010 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  4011 | `		}` |
|      262 |  4012 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  4013 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  4014 | `	}else{` |
|      ! 0 |  4015 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  4016 | `	}` |
|      525 |  4017 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  4018 | `	if( pFile ){` |
|      525 |  4019 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  4020 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  4021 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4022 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  4023 | `	}` |
|      525 |  4024 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  4025 | `	SyBlobRelease(&sOut);` |
|      525 |  4026 | `	return PH7_ABORT;` |
|      263 |  4027 |  |
|        - |  4028 | `/*` |
|        - |  4029 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  4030 | ` *` |
|        - |  4031 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  4032 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  4033 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  4034 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  4035 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  4036 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  4037 | ` */` |
|      868 |  4038 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  4039 |  |
|      870 |  4040 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  4041 | `		if( pVm->pCoalesceObj ){` |
|        7 |  4042 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4043 | `		}` |
|        7 |  4044 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  4045 | `		pVm->pCoalesceObj = 0;` |
|        7 |  4046 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4047 | `	}` |
|      870 |  4048 |  |
|        - |  4049 | `/*` |
|        - |  4050 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  4051 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  4052 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  4053 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  4054 | ` *` |
|        - |  4055 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  4056 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  4057 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  4058 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  4059 | ` */` |
|        4 |  4060 | `static sxi32 VmThrowFromVm(` |
|        - |  4061 | `	ph7_vm *pVm,` |
|        - |  4062 | `	const char *zClass,` |
|        - |  4063 | `	const char *zMsg,` |
|        - |  4064 | `	sxu32 nMsg` |
|        1 |  4065 | `){` |
|        - |  4066 | `	ph7_class *pClass;` |
|        - |  4067 | `	ph7_class_instance *pThis;` |
|        - |  4068 | `	ph7_class_method *pCons;` |
|        - |  4069 | `	VmFrame *pFrame;` |
|        - |  4070 | `	sxi32 rc;` |
|        5 |  4071 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  4072 | `	if( pClass == 0 ){` |
|      ! 0 |  4073 | `		return SXERR_ABORT;` |
|        - |  4074 | `	}` |
|        5 |  4075 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  4076 | `	if( pThis == 0 ){` |
|      ! 0 |  4077 | `		return SXERR_ABORT;` |
|        - |  4078 | `	}` |
|        5 |  4079 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  4080 | `	if( pCons ){` |
|        - |  4081 | `		ph7_value sArg;` |
|        - |  4082 | `		ph7_value *apArg[1];` |
|        - |  4083 | `		SyString sMsgStr;` |
|        5 |  4084 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  4085 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  4086 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  4087 | `		apArg[0] = &sArg;` |
|        5 |  4088 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  4089 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  4090 | `	}` |
|        5 |  4091 | `	pFrame = pVm->pFrame;` |
|        5 |  4092 | `	if( pFrame ){` |
|        5 |  4093 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  4094 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  4095 | `	}` |
|        5 |  4096 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  4097 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  4098 | `	return rc;` |
|        3 |  4099 |  |
|        - |  4100 | `/*` |
|        - |  4101 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  4102 | ` */` |
|      574 |  4103 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  4104 |  |
|        - |  4105 | `	ph7_vm *pVm;` |
|        - |  4106 | `	ph7_class *pClass;` |
|        - |  4107 | `	ph7_class_instance *pThis;` |
|        - |  4108 | `	ph7_class_method *pCons;` |
|        - |  4109 | `	ph7_value sArg;` |
|        - |  4110 | `	ph7_value *apArg[1];` |
|        - |  4111 | `	SyBlob sMsg;` |
|        - |  4112 | `	SyString sMsgStr;` |
|        - |  4113 | `	VmFrame *pFrame;` |
|        - |  4114 | `	va_list ap;` |
|        - |  4115 | `	sxi32 rc;` |
|        - |  4116 |  |
|      576 |  4117 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4118 | `		return PH7_ABORT;` |
|        - |  4119 | `	}` |
|      576 |  4120 | `	pVm = pCtx->pVm;` |
|      576 |  4121 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4122 | `		zClass = "Error";` |
|      ! 0 |  4123 | `	}` |
|      576 |  4124 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  4125 | `	if( pClass == 0 ){` |
|      ! 0 |  4126 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4127 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  4128 | `			zClass` |
|        - |  4129 | `			);` |
|        - |  4130 | `	}` |
|      576 |  4131 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  4132 | `	if( pThis == 0 ){` |
|      ! 0 |  4133 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4134 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  4135 | `			);` |
|        - |  4136 | `	}` |
|        - |  4137 |  |
|      576 |  4138 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  4139 | `	va_start(ap,zFormat);` |
|      576 |  4140 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  4141 | `	va_end(ap);` |
|        - |  4142 |  |
|      576 |  4143 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  4144 | `	if( pCons ){` |
|      576 |  4145 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  4146 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  4147 | `		apArg[0] = &sArg;` |
|      576 |  4148 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  4149 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  4150 | `	}` |
|      576 |  4151 | `	SyBlobRelease(&sMsg);` |
|        - |  4152 |  |
|      576 |  4153 | `	pFrame = pVm->pFrame;` |
|      576 |  4154 | `	if( pFrame ){` |
|      576 |  4155 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  4156 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  4157 | `	}` |
|      576 |  4158 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  4159 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  4160 | `	if( rc == SXERR_ABORT ){` |
|      491 |  4161 | `		return PH7_ABORT;` |
|        - |  4162 | `	}` |
|       86 |  4163 | `	return PH7_EXCEPTION;` |
|      289 |  4164 |  |
|        - |  4165 | `/*` |
|        - |  4166 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  4167 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  4168 | ` */` |
|      ! 0 |  4169 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  4170 |  |
|        - |  4171 | `	ph7_vm *pVm;` |
|        - |  4172 | `	SyBlob sMsg;` |
|      ! 0 |  4173 | `	const char *zFuncName = 0;` |
|      ! 0 |  4174 | `	int nFuncLen = 0;` |
|        - |  4175 | `	va_list ap;` |
|        - |  4176 | `	sxi32 rc;` |
|        - |  4177 |  |
|      ! 0 |  4178 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4179 | `		return PH7_OK;` |
|        - |  4180 | `	}` |
|      ! 0 |  4181 | `	pVm = pCtx->pVm;` |
|      ! 0 |  4182 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4183 | `		zClass = "Error";` |
|      ! 0 |  4184 | `	}` |
|        - |  4185 |  |
|      ! 0 |  4186 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  4187 |  |
|      ! 0 |  4188 | `	va_start(ap,zFormat);` |
|      ! 0 |  4189 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  4190 | `	va_end(ap);` |
|        - |  4191 |  |
|      ! 0 |  4192 | `	if( pCtx->pFunc ){` |
|      ! 0 |  4193 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  4194 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  4195 | `	}` |
|      ! 0 |  4196 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  4197 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  4198 | `	}` |
|      ! 0 |  4199 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  4200 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  4201 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  4202 | `	return rc;` |
|      ! 0 |  4203 |  |
|        - |  4204 | `/*` |
|        - |  4205 | ` * Save the execution state of a fiber/generator context.` |
|        - |  4206 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  4207 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  4208 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  4209 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  4210 | ` * when VmByteCodeExec returns.` |
|        - |  4211 | ` */` |
|      144 |  4212 | `static sxi32 VmSuspendCtx(` |
|        - |  4213 | `	ph7_vm *pVm,` |
|        - |  4214 | `	ph7_exec_ctx *pCtx,` |
|        - |  4215 | `	sxi32 pc,` |
|        - |  4216 | `	sxi32 nTos` |
|        - |  4217 | `	)` |
|        2 |  4218 |  |
|       72 |  4219 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  4220 | `	pCtx->pc = pc;` |
|      146 |  4221 | `	pCtx->nTos = nTos;` |
|      146 |  4222 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  4223 | `	return PH7_SUSPEND;` |
|        2 |  4224 |  |
|        - |  4225 | `/*` |
|        - |  4226 | ` * Resolve named-argument mapping.` |
|        - |  4227 | ` *` |
|        - |  4228 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  4229 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  4230 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  4231 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  4232 | ` * every formal parameter that received a value.` |
|        - |  4233 | ` *` |
|        - |  4234 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  4235 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  4236 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  4237 | ` */` |
|       98 |  4238 | `static sxi32 VmResolveNamedArgs(` |
|        - |  4239 | `	ph7_vm *pVm,` |
|        - |  4240 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  4241 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  4242 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  4243 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  4244 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  4245 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  4246 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  4247 |  |
|        2 |  4248 |  |
|      100 |  4249 | `	sxi32 posIdx = 0;` |
|        - |  4250 | `	sxu32 i;` |
|        - |  4251 | `	char zErrMsg[256];` |
|      100 |  4252 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  4253 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  4254 | `		aSlot[i] = -2;` |
|      100 |  4255 | `	}` |
|      290 |  4256 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  4257 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  4258 | `			/* Named argument — find formal by name */` |
|      184 |  4259 | `			int found = 0;` |
|        - |  4260 | `			sxu32 k;` |
|      304 |  4261 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  4262 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  4263 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  4264 | `						pMap->aNames[i].zString,` |
|      402 |  4265 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  4266 | `					if( aUsed[k] ){` |
|        7 |  4267 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4268 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  4269 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  4270 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  4271 | `						return PH7_ABORT;` |
|        - |  4272 | `					}` |
|      168 |  4273 | `					aSlot[i] = (sxi32)k;` |
|      168 |  4274 | `					aUsed[k] = 1;` |
|      168 |  4275 | `					found = 1;` |
|      168 |  4276 | `					break;` |
|        - |  4277 | `				}` |
|       62 |  4278 | `			}` |
|      180 |  4279 | `			if( !found ){` |
|       14 |  4280 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  4281 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  4282 | `				}else{` |
|        4 |  4283 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4284 | `						"Unknown named parameter $%.*s",` |
|        2 |  4285 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  4286 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  4287 | `					return PH7_ABORT;` |
|        - |  4288 | `				}` |
|        5 |  4289 | `			}` |
|       90 |  4290 | `		}else{` |
|        - |  4291 | `			/* Positional argument */` |
|       16 |  4292 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  4293 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  4294 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4295 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  4296 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  4297 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4298 | `					return PH7_ABORT;` |
|        - |  4299 | `				}` |
|       16 |  4300 | `				aSlot[i] = posIdx;` |
|       16 |  4301 | `				aUsed[posIdx] = 1;` |
|        7 |  4302 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4303 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4304 | `			}` |
|       16 |  4305 | `			posIdx++;` |
|        - |  4306 | `		}` |
|       97 |  4307 | `	}` |
|       93 |  4308 | `	return SXRET_OK;` |
|       51 |  4309 |  |
|        - |  4310 | `/*` |
|        - |  4311 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4312 | ` *` |
|        - |  4313 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4314 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4315 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4316 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4317 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4318 | ` * then the program execution is halted.` |
|        - |  4319 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4320 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4321 | ` * or to reset the VM to it's initial state.` |
|        - |  4322 | ` */` |
|    45080 |  4323 | `static sxi32 VmByteCodeExec(` |
|        - |  4324 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4325 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4326 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4327 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4328 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4329 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4330 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4331 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4332 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4333 | `	)` |
|        2 |  4334 |  |
|        - |  4335 | `	VmInstr *pInstr;` |
|        - |  4336 | `	ph7_value *pTos;` |
|        - |  4337 | `	SySet aArg;` |
|        - |  4338 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4339 | `	sxi32 pc;` |
|        - |  4340 | `	sxi32 rc;` |
|        - |  4341 | `	/* Argument container */` |
|    45082 |  4342 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    45082 |  4343 | `	if( nTos < 0 ){` |
|    41908 |  4344 | `		pTos = &pStack[-1];` |
|    20955 |  4345 | `	}else{` |
|     3176 |  4346 | `		pTos = &pStack[nTos];` |
|        - |  4347 | `	}` |
|    45082 |  4348 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    45082 |  4349 | `	pc = nPc;` |
|        - |  4350 | `/*` |
|        - |  4351 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4352 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4353 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4354 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4355 | ` */` |
|        - |  4356 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4357 | `	{ \` |
|        - |  4358 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4359 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4360 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4361 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4362 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4363 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4364 | `				break; \` |
|        - |  4365 | `			} \` |
|        - |  4366 | `			goto Exception; \` |
|        - |  4367 | `		} \` |
|        - |  4368 | `	}` |
|        - |  4369 | `	/* Execute as much as we can */` |
|  5904951 |  4370 | `	for(;;){` |
|        - |  4371 | `		/* Fetch the instruction to execute */` |
| 11809200 |  4372 | `		pInstr = &aInstr[pc];` |
| 11809200 |  4373 | `		rc = SXRET_OK;` |
|        - |  4374 | `/*` |
|        - |  4375 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4376 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4377 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4378 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4379 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4380 | ` */` |
| 11809200 |  4381 | `		switch(pInstr->iOp){` |
|        - |  4382 | `/*` |
|        - |  4383 | ` * DONE: P1 * *` |
|        - |  4384 | ` *` |
|        - |  4385 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4386 | ` * and return immediately.` |
|        - |  4387 | ` */` |
|    22162 |  4388 | `case PH7_OP_DONE:` |
|        - |  4389 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4390 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4391 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4392 | `	 * callback trampolines, and the main script. */` |
|    44324 |  4393 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      402 |  4394 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4395 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4396 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4397 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4398 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4399 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4400 | `		 * exception. */` |
|      398 |  4401 | `		ph7_value *pRetVal = 0;` |
|      398 |  4402 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      266 |  4403 | `			pRetVal = pTos;` |
|      132 |  4404 | `		}` |
|      398 |  4405 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      398 |  4406 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      392 |  4407 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4408 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4409 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4410 | `				pTos--;` |
|      ! 0 |  4411 | `			}` |
|      ! 0 |  4412 | `			goto Exception;` |
|        - |  4413 | `		}` |
|        - |  4414 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4415 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4416 | `		 * defensively we clear the pointer after a successful check). */` |
|      392 |  4417 | `		pEnforceRetFunc = 0;` |
|      195 |  4418 | `	}` |
|    44320 |  4419 | `	if( pInstr->iP1 ){` |
|        - |  4420 | `#ifdef UNTRUST` |
|        - |  4421 | `		if( pTos < pStack ){` |
|        - |  4422 | `			goto Abort;` |
|        - |  4423 | `		}` |
|        - |  4424 | `#endif` |
|    26962 |  4425 | `		if( pLastRef ){` |
|    16416 |  4426 | `			*pLastRef = pTos->nIdx;` |
|     8207 |  4427 | `		}` |
|    26962 |  4428 | `		if( pResult ){` |
|        - |  4429 | `			/* Execution result */` |
|    25462 |  4430 | `			PH7_MemObjStore(pTos,pResult);` |
|    12730 |  4431 | `		}` |
|    26962 |  4432 | `		VmPopOperand(&pTos,1);` |
|    30840 |  4433 | `	}else if( pLastRef ){` |
|        - |  4434 | `		/* Nothing referenced */` |
|     1960 |  4435 | `		*pLastRef = SXU32_HIGH;` |
|      979 |  4436 | `	}` |
|        - |  4437 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4438 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4439 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4440 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4441 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4442 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4443 | `	 * block can override it.` |
|        - |  4444 | `	 */` |
|    44322 |  4445 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4446 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4447 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4448 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4449 | `		pExc->pFrame = 0;` |
|        3 |  4450 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4451 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4452 | `			pExc->iFinallyDone = 1;` |
|        - |  4453 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4454 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4455 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4456 | `				goto Abort;` |
|        - |  4457 | `			}` |
|        1 |  4458 | `		}` |
|        1 |  4459 | `	}` |
|    44320 |  4460 | `	goto Done;` |
|        - |  4461 | `/*` |
|        - |  4462 | ` * HALT: P1 * *` |
|        - |  4463 | ` *` |
|        - |  4464 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4465 | ` * and abort immediately.` |
|        - |  4466 | ` */` |
|        7 |  4467 | `case PH7_OP_HALT:` |
|       15 |  4468 | `	if( pInstr->iP1 ){` |
|        - |  4469 | `#ifdef UNTRUST` |
|        - |  4470 | `		if( pTos < pStack ){` |
|        - |  4471 | `			goto Abort;` |
|        - |  4472 | `		}` |
|        - |  4473 | `#endif` |
|       15 |  4474 | `		if( pLastRef ){` |
|        3 |  4475 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4476 | `		}` |
|       15 |  4477 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4478 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4479 | `				/* Output the exit message */` |
|       16 |  4480 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4481 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4482 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4483 | `			}` |
|       10 |  4484 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4485 | `			/* Record exit status */` |
|        5 |  4486 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4487 | `		}` |
|       15 |  4488 | `		VmPopOperand(&pTos,1);` |
|        7 |  4489 | `	}else if( pLastRef ){` |
|        - |  4490 | `		/* Nothing referenced */` |
|      ! 0 |  4491 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4492 | `	}` |
|        - |  4493 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4494 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4495 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4496 | `	 */` |
|       15 |  4497 | `	pVm->bHaltRequested = 1;` |
|       15 |  4498 | `	goto Abort;` |
|        - |  4499 | `/*` |
|        - |  4500 | ` * JMP: * P2 *` |
|        - |  4501 | ` *` |
|        - |  4502 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4503 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4504 | ` */` |
|   251558 |  4505 | `case PH7_OP_JMP:` |
|   503162 |  4506 | `	pc = pInstr->iP2 - 1;` |
|   503162 |  4507 | `	break;` |
|        - |  4508 | `/*` |
|        - |  4509 | ` * JZ: P1 P2 *` |
|        - |  4510 | ` *` |
|        - |  4511 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4512 | ` * entry in the stack if P1 is zero.` |
|        - |  4513 | ` */` |
|   597271 |  4514 | `case PH7_OP_JZ:` |
|        - |  4515 | `#ifdef UNTRUST` |
|        - |  4516 | `	if( pTos < pStack ){` |
|        - |  4517 | `		goto Abort;` |
|        - |  4518 | `	}` |
|        - |  4519 | `#endif` |
|        - |  4520 | `	/* Get a boolean value */` |
|  1194632 |  4521 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4522 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4523 | `	}` |
|  1194632 |  4524 | `	if( !pTos->x.iVal ){` |
|        - |  4525 | `		/* Take the jump */` |
|   614706 |  4526 | `		pc = pInstr->iP2 - 1;` |
|   307352 |  4527 | `	}` |
|  1194632 |  4528 | `	if( !pInstr->iP1 ){` |
|   946614 |  4529 | `		VmPopOperand(&pTos,1);` |
|   473328 |  4530 | `	}` |
|  1194632 |  4531 | `	break;` |
|        - |  4532 | `/*` |
|        - |  4533 | ` * JNZ: P1 P2 *` |
|        - |  4534 | ` *` |
|        - |  4535 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4536 | ` * entry in the stack if P1 is zero.` |
|        - |  4537 | ` */` |
|    61366 |  4538 | `case PH7_OP_JNZ:` |
|        - |  4539 | `#ifdef UNTRUST` |
|        - |  4540 | `	if( pTos < pStack ){` |
|        - |  4541 | `		goto Abort;` |
|        - |  4542 | `	}` |
|        - |  4543 | `#endif` |
|        - |  4544 | `	/* Get a boolean value */` |
|   122734 |  4545 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4546 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4547 | `	}` |
|   122734 |  4548 | `	if( pTos->x.iVal ){` |
|        - |  4549 | `		/* Take the jump */` |
|     5596 |  4550 | `		pc = pInstr->iP2 - 1;` |
|     2797 |  4551 | `	}` |
|   122734 |  4552 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4553 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4554 | `	}` |
|   122734 |  4555 | `	break;` |
|        - |  4556 | `/*` |
|        - |  4557 | ` * NOOP: * * *` |
|        - |  4558 | ` *` |
|        - |  4559 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4560 | ` * destination.` |
|        - |  4561 | ` */` |
|      ! 0 |  4562 | `case PH7_OP_NOOP:` |
|      ! 0 |  4563 | `	break;` |
|        - |  4564 | `/*` |
|        - |  4565 | ` * POP: P1 * *` |
|        - |  4566 | ` *` |
|        - |  4567 | ` * Pop P1 elements from the operand stack.` |
|        - |  4568 | ` */` |
|   462980 |  4569 | `case PH7_OP_POP: {` |
|   926006 |  4570 | `	sxi32 n = pInstr->iP1;` |
|   926006 |  4571 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4572 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4573 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4574 | `	}` |
|   926006 |  4575 | `	VmPopOperand(&pTos,n);` |
|   926006 |  4576 | `	break;` |
|        - |  4577 | `				 }` |
|        - |  4578 | `/*` |
|        - |  4579 | ` * DUP: * * *` |
|        - |  4580 | ` *` |
|        - |  4581 | ` * Duplicate the top of the stack.` |
|        - |  4582 | ` */` |
|       41 |  4583 | `case PH7_OP_DUP:` |
|        - |  4584 | `#ifdef UNTRUST` |
|        - |  4585 | `	if( pTos < pStack ){` |
|        - |  4586 | `		goto Abort;` |
|        - |  4587 | `	}` |
|        - |  4588 | `#endif` |
|       84 |  4589 | `	pTos++;` |
|       84 |  4590 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4591 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4592 | `	break;` |
|        - |  4593 | `/*` |
|        - |  4594 | ` * NSSWITCH: * * P3` |
|        - |  4595 | ` *` |
|        - |  4596 | ` * Switch the active namespace at runtime.` |
|        - |  4597 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4598 | ` */` |
|     7828 |  4599 | `case PH7_OP_NSSWITCH:` |
|    15658 |  4600 | `	SyBlobReset(&pVm->sNamespace);` |
|    15658 |  4601 | `	if( pInstr->p3 ){` |
|      100 |  4602 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4603 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4604 | `	}` |
|        - |  4605 | `	/* Clear namespace-scoped use-const imports */` |
|    15658 |  4606 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15658 |  4607 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15658 |  4608 | `	break;` |
|        - |  4609 | `/* OP_USECONST P1 * P3` |
|        - |  4610 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4611 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4612 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4613 | ` */` |
|        7 |  4614 | `case PH7_OP_USECONST: {` |
|       16 |  4615 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4616 | `	if( azPair ){` |
|       16 |  4617 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4618 | `	}` |
|       16 |  4619 | `	break;` |
|        - |  4620 | `				}` |
|        - |  4621 | `/*` |
|        - |  4622 | ` * CVT_INT: * * *` |
|        - |  4623 | ` *` |
|        - |  4624 | ` * Force the top of the stack to be an integer.` |
|        - |  4625 | ` */` |
|       80 |  4626 | `case PH7_OP_CVT_INT:` |
|        - |  4627 | `#ifdef UNTRUST` |
|        - |  4628 | `	if( pTos < pStack ){` |
|        - |  4629 | `		goto Abort;` |
|        - |  4630 | `	}` |
|        - |  4631 | `#endif` |
|      162 |  4632 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4633 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4634 | `	}` |
|        - |  4635 | `	/* Invalidate any prior representation */` |
|      162 |  4636 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4637 | `	break;` |
|        - |  4638 | `/*` |
|        - |  4639 | ` * CVT_REAL: * * *` |
|        - |  4640 | ` *` |
|        - |  4641 | ` * Force the top of the stack to be a real.` |
|        - |  4642 | ` */` |
|        5 |  4643 | `case PH7_OP_CVT_REAL:` |
|        - |  4644 | `#ifdef UNTRUST` |
|        - |  4645 | `	if( pTos < pStack ){` |
|        - |  4646 | `		goto Abort;` |
|        - |  4647 | `	}` |
|        - |  4648 | `#endif` |
|       11 |  4649 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4650 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4651 | `	}` |
|        - |  4652 | `	/* Invalidate any prior representation */` |
|       11 |  4653 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4654 | `	break;` |
|        - |  4655 | `/*` |
|        - |  4656 | ` * CVT_STR: * * *` |
|        - |  4657 | ` *` |
|        - |  4658 | ` * Force the top of the stack to be a string.` |
|        - |  4659 | ` */` |
|      163 |  4660 | `case PH7_OP_CVT_STR:` |
|        - |  4661 | `#ifdef UNTRUST` |
|        - |  4662 | `	if( pTos < pStack ){` |
|        - |  4663 | `		goto Abort;` |
|        - |  4664 | `	}` |
|        - |  4665 | `#endif` |
|      328 |  4666 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4667 | `		PH7_MemObjToString(pTos);` |
|      153 |  4668 | `	}` |
|      328 |  4669 | `	break;` |
|        - |  4670 | `/*` |
|        - |  4671 | ` * CVT_BOOL: * * *` |
|        - |  4672 | ` *` |
|        - |  4673 | ` * Force the top of the stack to be a boolean.` |
|        - |  4674 | ` */` |
|        5 |  4675 | `case PH7_OP_CVT_BOOL:` |
|        - |  4676 | `#ifdef UNTRUST` |
|        - |  4677 | `	if( pTos < pStack ){` |
|        - |  4678 | `		goto Abort;` |
|        - |  4679 | `	}` |
|        - |  4680 | `#endif` |
|       11 |  4681 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4682 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4683 | `	}` |
|       11 |  4684 | `	break;` |
|        - |  4685 | `/*` |
|        - |  4686 | ` * CVT_NULL: * * *` |
|        - |  4687 | ` *` |
|        - |  4688 | ` * Nullify the top of the stack.` |
|        - |  4689 | ` */` |
|        3 |  4690 | `case PH7_OP_CVT_NULL:` |
|        - |  4691 | `#ifdef UNTRUST` |
|        - |  4692 | `	if( pTos < pStack ){` |
|        - |  4693 | `		goto Abort;` |
|        - |  4694 | `	}` |
|        - |  4695 | `#endif` |
|        7 |  4696 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4697 | `	break;` |
|        - |  4698 | `/*` |
|        - |  4699 | ` * CVT_NUMC: * * *` |
|        - |  4700 | ` *` |
|        - |  4701 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4702 | ` */` |
|      ! 0 |  4703 | `case PH7_OP_CVT_NUMC:` |
|        - |  4704 | `#ifdef UNTRUST` |
|        - |  4705 | `	if( pTos < pStack ){` |
|        - |  4706 | `		goto Abort;` |
|        - |  4707 | `	}` |
|        - |  4708 | `#endif` |
|        - |  4709 | `	/* Force a numeric cast */` |
|      ! 0 |  4710 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4711 | `	break;` |
|        - |  4712 | `/*` |
|        - |  4713 | ` * CVT_ARRAY: * * *` |
|        - |  4714 | ` *` |
|        - |  4715 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4716 | ` */` |
|       10 |  4717 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4718 | `#ifdef UNTRUST` |
|        - |  4719 | `	if( pTos < pStack ){` |
|        - |  4720 | `		goto Abort;` |
|        - |  4721 | `	}` |
|        - |  4722 | `#endif` |
|        - |  4723 | `	/* Force a hashmap cast */` |
|       21 |  4724 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4725 | `	if( rc != SXRET_OK ){` |
|        - |  4726 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4727 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4728 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4729 | `	}` |
|       21 |  4730 | `	break;` |
|        - |  4731 | `/*` |
|        - |  4732 | ` * CVT_OBJ: * * *` |
|        - |  4733 | ` *` |
|        - |  4734 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4735 | ` */` |
|        8 |  4736 | `case PH7_OP_CVT_OBJ:` |
|        - |  4737 | `#ifdef UNTRUST` |
|        - |  4738 | `	if( pTos < pStack ){` |
|        - |  4739 | `		goto Abort;` |
|        - |  4740 | `	}` |
|        - |  4741 | `#endif` |
|       17 |  4742 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4743 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4744 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4745 | `	}` |
|       17 |  4746 | `	break;` |
|        - |  4747 | `/*` |
|        - |  4748 | ` * ERR_CTRL * * *` |
|        - |  4749 | ` *` |
|        - |  4750 | ` * Error control operator.` |
|        - |  4751 | ` */` |
|    16061 |  4752 | `case PH7_OP_ERR_CTRL:` |
|        - |  4753 | `	/*` |
|        - |  4754 | `	 * TICKET 1433-038:` |
|        - |  4755 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4756 | `	 * use the public API,to control error output.` |
|        - |  4757 | `	 */` |
|    32122 |  4758 | `	break;` |
|        - |  4759 | `/*` |
|        - |  4760 | ` * IS_A * * *` |
|        - |  4761 | ` *` |
|        - |  4762 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4763 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4764 | ` * holding a class name or an object).` |
|        - |  4765 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4766 | ` */` |
|       75 |  4767 | `case PH7_OP_IS_A:{` |
|      152 |  4768 | `	ph7_value *pNos = &pTos[-1];` |
|      152 |  4769 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4770 | `#ifdef UNTRUST` |
|        - |  4771 | `	if( pNos < pStack ){` |
|        - |  4772 | `		goto Abort;` |
|        - |  4773 | `	}` |
|        - |  4774 | `#endif` |
|      152 |  4775 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      150 |  4776 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      150 |  4777 | `		ph7_class *pClass = 0;` |
|        - |  4778 | `		/* Extract the target class */` |
|      150 |  4779 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4780 | `			/* Instance already loaded */` |
|      ! 0 |  4781 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      150 |  4782 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      150 |  4783 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      150 |  4784 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4785 | `			/* Handle self/static/parent keywords */` |
|      150 |  4786 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4787 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      148 |  4788 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4789 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      147 |  4790 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4791 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4792 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4793 | `					pClass = pSelf->pBase;` |
|        2 |  4794 | `				}` |
|        3 |  4795 | `			}else{` |
|      140 |  4796 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4797 | `			}` |
|       74 |  4798 | `		}` |
|      150 |  4799 | `		if( pClass ){` |
|        - |  4800 | `			/* Perform the query */` |
|      150 |  4801 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       74 |  4802 | `		}` |
|       74 |  4803 | `	}` |
|        - |  4804 | `	/* Push result */` |
|      152 |  4805 | `	VmPopOperand(&pTos,1);` |
|      152 |  4806 | `	PH7_MemObjRelease(pTos);` |
|      152 |  4807 | `	pTos->x.iVal = iRes;` |
|      152 |  4808 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      152 |  4809 | `	break;` |
|        - |  4810 | `				 }` |
|        - |  4811 |  |
|        - |  4812 | `/*` |
|        - |  4813 | ` * LOADC P1 P2 *` |
|        - |  4814 | ` *` |
|        - |  4815 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4816 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4817 | ` */` |
|  1015963 |  4818 | `case PH7_OP_LOADC: {` |
|        - |  4819 | `	ph7_value *pObj;` |
|        - |  4820 | `	/* Reserve a room */` |
|  2031972 |  4821 | `	pTos++;` |
|  3038099 |  4822 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2031972 |  4823 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4824 | `			SyHashEntry *pEntry;` |
|        - |  4825 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4826 | `			{` |
|        - |  4827 | `				SyHashEntry *pConstImport;` |
|    29648 |  4828 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19764 |  4829 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19766 |  4830 | `				if( pConstImport ){` |
|       11 |  4831 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4832 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4833 | `					if( pEntry ){` |
|       11 |  4834 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4835 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4836 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4837 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4838 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4839 | `						break;` |
|        - |  4840 | `					}` |
|        - |  4841 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4842 | `				}` |
|        - |  4843 | `			}` |
|        - |  4844 | `			/* Candidate for expansion via user defined callbacks */` |
|    19756 |  4845 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19756 |  4846 | `			if( pEntry ){` |
|    19750 |  4847 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4848 | `				/* Set a NULL default value */` |
|    19750 |  4849 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19750 |  4850 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4851 | `				/* Invoke the callback and deal with the expanded value */` |
|    19750 |  4852 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4853 | `				/* Mark as constant */` |
|    19750 |  4854 | `				pTos->nIdx = SXU32_HIGH;` |
|    19750 |  4855 | `				break;` |
|        - |  4856 | `			}` |
|        - |  4857 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4858 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4859 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4860 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4861 | `			{` |
|        8 |  4862 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4863 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4864 | `				sxu32 j;` |
|        8 |  4865 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4866 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4867 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4868 | `				}` |
|        8 |  4869 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4870 | `					/* Try current_namespace\name */` |
|      ! 0 |  4871 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4872 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4873 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4874 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4875 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4876 | `					if( pEntry ){` |
|      ! 0 |  4877 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4878 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4879 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4880 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4881 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4882 | `						break;` |
|        - |  4883 | `					}` |
|        - |  4884 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4885 | `				}` |
|        8 |  4886 | `				if( isQualified ){` |
|        - |  4887 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4888 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4889 | `					SyBlob sErr;` |
|        3 |  4890 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4891 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4892 | `					if( pErrFile ){` |
|        3 |  4893 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4894 | `					}` |
|        3 |  4895 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4896 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4897 | `					SyBlobRelease(&sErr);` |
|        3 |  4898 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4899 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4900 | `					goto LoadC_Done;` |
|        - |  4901 | `				}` |
|        - |  4902 | `			}` |
|        2 |  4903 | `		}` |
|  2012212 |  4904 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1006129 |  4905 | `	}else{` |
|        - |  4906 | `		/* Set a NULL value */` |
|      ! 0 |  4907 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4908 | `	}` |
|  1006084 |  4909 | `LoadC_Done:` |
|        - |  4910 | `	/* Mark as constant */` |
|  2012214 |  4911 | `	pTos->nIdx = SXU32_HIGH;` |
|  2012214 |  4912 | `	break;` |
|        - |  4913 | `				  }` |
|        - |  4914 | `/*` |
|        - |  4915 | ` * LOAD: P1 * P3` |
|        - |  4916 | ` *` |
|        - |  4917 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4918 | ` * from the P3 operand.` |
|        - |  4919 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4920 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4921 | ` */` |
|  1576586 |  4922 | `case PH7_OP_LOAD:{` |
|        - |  4923 | `	ph7_value *pObj;` |
|        - |  4924 | `	SyString sName;` |
|  3153394 |  4925 | `	if( pInstr->p3 == 0 ){` |
|        - |  4926 | `		/* Take the variable name from the top of the stack */` |
|        - |  4927 | `#ifdef UNTRUST` |
|        - |  4928 | `		if( pTos < pStack ){` |
|        - |  4929 | `			goto Abort;` |
|        - |  4930 | `		}` |
|        - |  4931 | `#endif` |
|        - |  4932 | `		/* Force a string cast */` |
|       19 |  4933 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4934 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4935 | `		}` |
|       19 |  4936 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4937 | `	}else{` |
|  3153376 |  4938 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4939 | `		/* Reserve a room for the target object */` |
|  3153376 |  4940 | `		pTos++;` |
|        - |  4941 | `	}` |
|        - |  4942 | `	/* Extract the requested memory object */` |
|  3153394 |  4943 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3153394 |  4944 | `	if( pObj == 0 ){` |
|      854 |  4945 | `		if( pInstr->iP1 ){` |
|        - |  4946 | `			/* Variable not found,load NULL */` |
|      854 |  4947 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4948 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4949 | `			}else{` |
|      854 |  4950 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4951 | `			}` |
|      854 |  4952 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1577014 |  4953 | `			break;` |
|      ! 0 |  4954 | `		}else{` |
|        - |  4955 | `			/* Fatal error */` |
|      ! 0 |  4956 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4957 | `			goto Abort;` |
|        - |  4958 | `		}` |
|        - |  4959 | `	}` |
|        - |  4960 | `	/* Load variable contents */` |
|  3152542 |  4961 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3152542 |  4962 | `	pTos->nIdx = pObj->nIdx;` |
|  3152542 |  4963 | `	break;` |
|        - |  4964 | `				   }` |
|        - |  4965 | `/*` |
|        - |  4966 | ` * LOAD_MAP P1 * *` |
|        - |  4967 | ` *` |
|        - |  4968 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4969 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4970 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4971 | ` */` |
|    22811 |  4972 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4973 | `	ph7_hashmap *pMap;` |
|        - |  4974 | `	/* Allocate a new hashmap instance */` |
|    45624 |  4975 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45624 |  4976 | `	if( pMap == 0 ){` |
|      ! 0 |  4977 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4978 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4979 | `		goto Abort;` |
|        - |  4980 | `	}` |
|    45624 |  4981 | `	if( pInstr->iP1 > 0 ){` |
|     2786 |  4982 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2786 |  4983 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4984 | `		/* Perform the insertion */` |
|     8510 |  4985 | `		while( pEntry < pTos ){` |
|     5742 |  4986 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4987 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4988 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4989 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  4990 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  4991 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  4992 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4993 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4994 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4995 | `						 * map dangling. */` |
|      ! 0 |  4996 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4997 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4998 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4999 | `						break;` |
|        - |  5000 | `					}` |
|       27 |  5001 | `				}else{` |
|        - |  5002 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  5003 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  5004 | `					break;` |
|        1 |  5005 | `				}` |
|     5700 |  5006 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5007 | `				/* Insertion by reference */` |
|      151 |  5008 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5009 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5010 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5011 | `					);` |
|       51 |  5012 | `			}else{` |
|        - |  5013 | `				/* Standard insertion */` |
|     8360 |  5014 | `				PH7_HashmapInsert(pMap,` |
|     5572 |  5015 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2786 |  5016 | `					&pEntry[1]` |
|        - |  5017 | `				);` |
|        - |  5018 | `			}` |
|        - |  5019 | `			/* Next pair on the stack */` |
|     5726 |  5020 | `			pEntry += 2;` |
|        2 |  5021 | `		}` |
|        - |  5022 | `		/* Pop P1 elements */` |
|     2786 |  5023 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2786 |  5024 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5025 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  5026 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  5027 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5028 | `				goto Abort;` |
|        - |  5029 | `			}` |
|        - |  5030 | `			{` |
|       17 |  5031 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  5032 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  5033 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  5034 | `					break;` |
|        - |  5035 | `				}` |
|        - |  5036 | `			}` |
|       15 |  5037 | `			goto Exception;` |
|        - |  5038 | `		}` |
|     1384 |  5039 | `	}` |
|        - |  5040 | `	/* Push the hashmap */` |
|    45608 |  5041 | `	pTos++;` |
|    45608 |  5042 | `	pTos->nIdx = SXU32_HIGH;` |
|    45608 |  5043 | `	pTos->x.pOther = pMap;` |
|    45608 |  5044 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45608 |  5045 | `	break;` |
|        - |  5046 | `					  }` |
|        - |  5047 | `/*` |
|        - |  5048 | ` * LOAD_LIST: P1 * *` |
|        - |  5049 | ` *` |
|        - |  5050 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5051 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5052 | ` * Caveats:` |
|        - |  5053 | ` *  This implementation support only a single nesting level.` |
|        - |  5054 | ` */` |
|       48 |  5055 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5056 | `	ph7_value *pEntry;` |
|       98 |  5057 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5058 | `		/* Empty list,break immediately */` |
|      ! 0 |  5059 | `		break;` |
|        - |  5060 | `	}` |
|       98 |  5061 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5062 | `#ifdef UNTRUST` |
|        - |  5063 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5064 | `		goto Abort;` |
|        - |  5065 | `	}` |
|        - |  5066 | `#endif` |
|       98 |  5067 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5068 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5069 | `		ph7_hashmap_node *pNode;` |
|        - |  5070 | `		ph7_value sKey,*pObj;` |
|        - |  5071 | `		/* Start Copying */` |
|       91 |  5072 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5073 | `		while( pEntry <= pTos ){` |
|      193 |  5074 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5075 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5076 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5077 | `					if( rc == SXRET_OK ){` |
|        - |  5078 | `						/* Store node value */` |
|      165 |  5079 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5080 | `					}else{` |
|        - |  5081 | `						/* Undefined array key */` |
|        - |  5082 | `						char zMsg[128];` |
|      ! 0 |  5083 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5084 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5085 | `						PH7_MemObjRelease(pObj);` |
|        - |  5086 | `					}` |
|       82 |  5087 | `				}` |
|       82 |  5088 | `			}` |
|      193 |  5089 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5090 | `			pEntry++;` |
|        1 |  5091 | `		}` |
|       46 |  5092 | `	}else{` |
|        - |  5093 | `		/* Source is not an array */` |
|        - |  5094 | `		ph7_value *pObj;` |
|       18 |  5095 | `		while( pEntry <= pTos ){` |
|       12 |  5096 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5097 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5098 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5099 | `				}` |
|        5 |  5100 | `			}` |
|       12 |  5101 | `			pEntry++;` |
|        2 |  5102 | `		}` |
|        8 |  5103 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5104 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5105 | `			const char *zType = "unknown";` |
|        3 |  5106 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5107 | `			char zMsg[256];` |
|        3 |  5108 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5109 | `				zType = "string";` |
|        1 |  5110 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5111 | `				zType = "int";` |
|      ! 0 |  5112 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5113 | `				zType = "float";` |
|      ! 0 |  5114 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5115 | `				zType = "object";` |
|      ! 0 |  5116 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5117 | `				zType = "resource";` |
|      ! 0 |  5118 | `			}` |
|        3 |  5119 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5120 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5121 | `		}` |
|        - |  5122 | `	}` |
|       98 |  5123 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5124 | `	break;` |
|        - |  5125 | `					   }` |
|        - |  5126 | `/*` |
|        - |  5127 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5128 | ` *` |
|        - |  5129 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5130 | ` * from the stack.` |
|        - |  5131 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5132 | ` * instead.` |
|        - |  5133 | ` */` |
|   250716 |  5134 | `case PH7_OP_LOAD_IDX: {` |
|   501478 |  5135 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   501478 |  5136 | `	ph7_hashmap *pMap = 0;` |
|        - |  5137 | `	ph7_value *pIdx;` |
|   501478 |  5138 | `	pIdx = 0;` |
|   501478 |  5139 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5140 | `		if( !pInstr->iP2){` |
|        - |  5141 | `			/* No available index,load NULL */` |
|      ! 0 |  5142 | `			if( pTos >= pStack ){` |
|      ! 0 |  5143 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5144 | `			}else{` |
|        - |  5145 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5146 | `				pTos++;` |
|      ! 0 |  5147 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5148 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5149 | `			}` |
|        - |  5150 | `			/* Emit a notice */` |
|      ! 0 |  5151 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5152 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5153 | `			break;` |
|        - |  5154 | `		}` |
|      ! 0 |  5155 | `	}else{` |
|   501478 |  5156 | `		pIdx = pTos;` |
|   501478 |  5157 | `		pTos--;` |
|        - |  5158 | `	}` |
|   501478 |  5159 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5160 | `		/* String access */` |
|   387716 |  5161 | `		if( pIdx ){` |
|        - |  5162 | `			sxu32 nOfft;` |
|   387716 |  5163 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5164 | `				/* Force an int cast */` |
|      ! 0 |  5165 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5166 | `			}` |
|   387716 |  5167 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  5168 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5169 | `				/* Invalid offset,load null */` |
|      ! 0 |  5170 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5171 | `			}else{` |
|   387716 |  5172 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  5173 | `				int c = zData[nOfft];` |
|   387716 |  5174 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  5175 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  5176 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5177 | `			}` |
|   193881 |  5178 | `		}else{` |
|        - |  5179 | `			/* No available index,load NULL */` |
|      ! 0 |  5180 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5181 | `		}` |
|   387716 |  5182 | `		break;` |
|        - |  5183 | `	}` |
|   113764 |  5184 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5185 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5186 | `		 * iP2 codes:` |
|        - |  5187 | `		 *   0 = read       → offsetGet` |
|        - |  5188 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5189 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5190 | `		 *   4 = isset()    → offsetExists` |
|        - |  5191 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5192 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  5193 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  5194 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  5195 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5196 | `			ph7_class_method *pMeth;` |
|        - |  5197 | `			ph7_value sResult;` |
|        - |  5198 | `			ph7_value *apArg[1];` |
|      124 |  5199 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5200 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5201 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5202 | `					"Cannot use [] for reading");` |
|      ! 0 |  5203 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5204 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5205 | `				break;` |
|        - |  5206 | `			}` |
|      124 |  5207 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  5208 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5209 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  5210 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5211 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  5212 | `				apArg[0] = pIdx;` |
|       51 |  5213 | `				if( pMeth ){` |
|       51 |  5214 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  5215 | `				}` |
|       99 |  5216 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  5217 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5218 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  5219 | `				apArg[0] = pIdx;` |
|        9 |  5220 | `				if( pMeth ){` |
|        9 |  5221 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5222 | `				}` |
|        5 |  5223 | `			}else{` |
|       66 |  5224 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5225 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  5226 | `				apArg[0] = pIdx;` |
|       66 |  5227 | `				if( pMeth ){` |
|       66 |  5228 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5229 | `				}` |
|        - |  5230 | `			}` |
|      124 |  5231 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5232 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5233 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5234 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  5235 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  5236 | `				PH7_MemObjRelease(pTos);` |
|       33 |  5237 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  5238 | `				if( bExists ){` |
|       17 |  5239 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  5240 | `					pTos->x.iVal = 1;` |
|        9 |  5241 | `				}else{` |
|       17 |  5242 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  5243 | `				}` |
|      108 |  5244 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5245 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5246 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  5247 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5248 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5249 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  5250 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5251 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5252 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5253 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5254 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5255 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5256 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5257 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5258 | `				if( !bExists ){` |
|        3 |  5259 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5260 | `				}else{` |
|        9 |  5261 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5262 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5263 | `					ph7_value sValue;` |
|        9 |  5264 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5265 | `					apArg[0] = pIdx;` |
|        9 |  5266 | `					if( pGet ){` |
|        9 |  5267 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5268 | `					}` |
|        9 |  5269 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5270 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5271 | `				}` |
|       11 |  5272 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5273 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  5274 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5275 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5276 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5277 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5278 | `				 *     and push NULL.` |
|        - |  5279 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  5280 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  5281 | `				int bShouldArm = !bExists;` |
|        - |  5282 | `				ph7_value sValue;` |
|        9 |  5283 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5284 | `				/* Reset any prior arming defensively */` |
|        9 |  5285 | `				VmCoalesceDisarm(pVm);` |
|        9 |  5286 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5287 | `				if( bExists ){` |
|        5 |  5288 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5289 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5290 | `					apArg[0] = pIdx;` |
|        5 |  5291 | `					if( pGet ){` |
|        5 |  5292 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5293 | `					}` |
|        5 |  5294 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5295 | `						bShouldArm = 1;` |
|        1 |  5296 | `					}` |
|        2 |  5297 | `				}` |
|        9 |  5298 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5299 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5300 | `				if( bShouldArm ){` |
|        - |  5301 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5302 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5303 | `					 * intervening expression evaluation. */` |
|        7 |  5304 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  5305 | `					if( pIdx ){` |
|        7 |  5306 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5307 | `					}` |
|        7 |  5308 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  5309 | `					pInst->iRef++;` |
|        7 |  5310 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  5311 | `				}else{` |
|        3 |  5312 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5313 | `				}` |
|        9 |  5314 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  5315 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  5316 | `				break;` |
|      ! 0 |  5317 | `			}else{` |
|        - |  5318 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  5319 | `				PH7_MemObjRelease(pTos);` |
|       66 |  5320 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  5321 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5322 | `			}` |
|      106 |  5323 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  5324 | `			if( pIdx ){` |
|      106 |  5325 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5326 | `			}` |
|      106 |  5327 | `			break;` |
|        - |  5328 | `		}` |
|        - |  5329 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5330 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5331 | `		if( pInst ){` |
|        - |  5332 | `			char zMsg[256];` |
|        3 |  5333 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5334 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5335 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5336 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5337 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5338 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5339 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5340 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5341 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5342 | `			break;` |
|        - |  5343 | `		}` |
|      ! 0 |  5344 | `	}` |
|   113640 |  5345 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5346 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5347 | `			ph7_value *pObj;` |
|        3 |  5348 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5349 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5350 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5351 | `			}` |
|        1 |  5352 | `		}` |
|        1 |  5353 | `	}` |
|   113640 |  5354 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   113640 |  5355 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   113640 |  5356 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5357 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5358 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5359 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5360 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5361 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5362 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  5363 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  5364 | `		}` |
|        - |  5365 | `		/* Point to the hashmap */` |
|   113640 |  5366 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   113640 |  5367 | `		if( pIdx ){` |
|        - |  5368 | `			/* Load the desired entry */` |
|   113640 |  5369 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56819 |  5370 | `		}` |
|   113640 |  5371 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5372 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5373 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5374 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5375 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5376 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5377 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5378 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5379 | `			 * correct for the outermost write. */` |
|       19 |  5380 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5381 | `			if( !needWrite && pNode ){` |
|       13 |  5382 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5383 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5384 | `					needWrite = 1;` |
|        3 |  5385 | `				}` |
|        6 |  5386 | `			}` |
|       19 |  5387 | `			if( needWrite ){` |
|       13 |  5388 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5389 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5390 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5391 | `					 * into the new map's storage. */` |
|        7 |  5392 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5393 | `					if( pIdx ){` |
|        7 |  5394 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5395 | `					}` |
|        3 |  5396 | `				}` |
|        6 |  5397 | `			}` |
|        9 |  5398 | `		}` |
|   113640 |  5399 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5400 | `			/* Create a new empty entry */` |
|      273 |  5401 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5402 | `			if( rc == SXRET_OK ){` |
|        - |  5403 | `				/* Point to the last inserted entry */` |
|      273 |  5404 | `				pNode = pMap->pLast;` |
|      136 |  5405 | `			}` |
|      136 |  5406 | `		}` |
|    56819 |  5407 | `	}` |
|   113640 |  5408 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5409 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5410 | `		char zMsg[128];` |
|      ! 0 |  5411 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5412 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5413 | `		}` |
|      ! 0 |  5414 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5415 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5416 | `	}` |
|   113640 |  5417 | `	if( pIdx ){` |
|   113640 |  5418 | `		PH7_MemObjRelease(pIdx);` |
|    56819 |  5419 | `	}` |
|   113640 |  5420 | `	if( rc == SXRET_OK ){` |
|        - |  5421 | `		/* Load entry contents */` |
|    50386 |  5422 | `		if( pMap->iRef < 2 ){` |
|        - |  5423 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5424 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5425 | `			 */` |
|       28 |  5426 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5427 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5428 | `		}else{` |
|    50360 |  5429 | `			pTos->nIdx = pNode->nValIdx;` |
|    50360 |  5430 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50360 |  5431 | `			PH7_HashmapUnref(pMap);` |
|        - |  5432 | `		}` |
|    25194 |  5433 | `	}else{` |
|        - |  5434 | `		/* No such entry,load NULL */` |
|    63256 |  5435 | `		PH7_MemObjRelease(pTos);` |
|    63256 |  5436 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5437 | `	}` |
|   113640 |  5438 | `	break;` |
|        - |  5439 | `					  }` |
|        - |  5440 | `/*` |
|        - |  5441 | ` * LOAD_CLOSURE * * P3` |
|        - |  5442 | ` *` |
|        - |  5443 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5444 | ` * name in the stack.` |
|        - |  5445 | ` */` |
|       64 |  5446 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5447 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5448 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5449 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5450 | `		ph7_vm_func *pClosure;` |
|        - |  5451 | `		char *zName;` |
|        - |  5452 | `		sxu32 mLen;` |
|        - |  5453 | `		sxu32 n;` |
|        - |  5454 | `		/* Create a new VM function */` |
|      130 |  5455 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5456 | `		/* Generate an unique closure name */` |
|      130 |  5457 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5458 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5459 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5460 | `			goto Abort;` |
|        - |  5461 | `		}` |
|      130 |  5462 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5463 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5464 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5465 | `		}` |
|        - |  5466 | `		/* Zero the stucture */` |
|      130 |  5467 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5468 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5469 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5470 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5471 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5472 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5473 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5474 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5475 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5476 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5477 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5478 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5479 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5480 | `		/* Register the closure */` |
|      130 |  5481 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5482 | `		/* Set up closure environment */` |
|      130 |  5483 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5484 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5485 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5486 | `			ph7_value *pValue;` |
|      196 |  5487 | `			pEnv = &aEnv[n];` |
|      196 |  5488 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5489 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5490 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5491 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5492 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5493 | `				/* Pass by reference */` |
|      ! 0 |  5494 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5495 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5496 | `					);` |
|      ! 0 |  5497 | `			}` |
|        - |  5498 | `			/* Standard pass by value */` |
|      196 |  5499 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5500 | `			if( pValue ){` |
|        - |  5501 | `				/* Copy imported value */` |
|       72 |  5502 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5503 | `			}` |
|        - |  5504 | `			/* Insert the imported variable */` |
|      196 |  5505 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5506 | `		}` |
|        - |  5507 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5508 | `		pTos++;` |
|      130 |  5509 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5510 | `	}` |
|      130 |  5511 | `	break;` |
|        - |  5512 | `						 }` |
|        - |  5513 | `/*` |
|        - |  5514 | ` * STORE * P2 P3` |
|        - |  5515 | ` *` |
|        - |  5516 | ` * Perform a store (Assignment) operation.` |
|        - |  5517 | ` */` |
|   146183 |  5518 | `case PH7_OP_STORE: {` |
|        - |  5519 | `	ph7_value *pObj;` |
|        - |  5520 | `	SyString sName;` |
|        - |  5521 | `#ifdef UNTRUST` |
|        - |  5522 | `	if( pTos < pStack ){` |
|        - |  5523 | `		goto Abort;` |
|        - |  5524 | `	}` |
|        - |  5525 | `#endif` |
|   292368 |  5526 | `	if( pInstr->iP2 ){` |
|        - |  5527 | `		sxu32 nIdx;` |
|        - |  5528 | `		sxi32 rcT;` |
|        - |  5529 | `		/* Member store operation */` |
|     5270 |  5530 | `		nIdx = pTos->nIdx;` |
|     5270 |  5531 | `		VmPopOperand(&pTos,1);` |
|     5270 |  5532 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5533 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5534 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5535 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5536 | `		}else{` |
|        - |  5537 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5538 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5266 |  5539 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5266 |  5540 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5541 | `				goto Abort;` |
|        - |  5542 | `			}` |
|     5266 |  5543 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5544 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5545 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5546 | `				 * propagate out of the VM loop. */` |
|       37 |  5547 | `				VmPopOperand(&pTos,1);` |
|        - |  5548 | `				{` |
|       37 |  5549 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5550 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5551 | `						pc = pFrm2->iExceptionJump - 1;` |
|   146202 |  5552 | `						break;` |
|        - |  5553 | `					}` |
|        - |  5554 | `				}` |
|      ! 0 |  5555 | `				goto Exception;` |
|        - |  5556 | `			}` |
|        - |  5557 | `			/* Point to the desired memory object */` |
|     5230 |  5558 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5230 |  5559 | `			if( pObj ){` |
|        - |  5560 | `				/* Perform the store operation */` |
|     5230 |  5561 | `				PH7_MemObjStore(pTos,pObj);` |
|     2614 |  5562 | `			}` |
|        - |  5563 | `		}` |
|     5234 |  5564 | `		break;` |
|   287100 |  5565 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5566 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5567 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5568 | `			/* Force a string cast */` |
|      ! 0 |  5569 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5570 | `		}` |
|        7 |  5571 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5572 | `		pTos--;` |
|        - |  5573 | `#ifdef UNTRUST` |
|        - |  5574 | `		if( pTos < pStack  ){` |
|        - |  5575 | `			goto Abort;` |
|        - |  5576 | `		}` |
|        - |  5577 | `#endif` |
|        4 |  5578 | `	}else{` |
|   287094 |  5579 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5580 | `	}` |
|        - |  5581 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   287100 |  5582 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   287100 |  5583 | `	if( pObj == 0 ){` |
|      ! 0 |  5584 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5585 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5586 | `		goto Abort;` |
|        - |  5587 | `	}` |
|   287100 |  5588 | `	if( !pInstr->p3 ){` |
|        7 |  5589 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5590 | `	}` |
|        - |  5591 | `	/* Perform the store operation */` |
|   287100 |  5592 | `	PH7_MemObjStore(pTos,pObj);` |
|   287100 |  5593 | `	break;` |
|        - |  5594 | `				   }` |
|        - |  5595 | `/*` |
|        - |  5596 | ` * STORE_IDX:   P1 * P3` |
|        - |  5597 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5598 | ` *` |
|        - |  5599 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5600 | ` */` |
|    97086 |  5601 | `case PH7_OP_STORE_IDX:` |
|        - |  5602 | `case PH7_OP_STORE_IDX_REF: {` |
|   194174 |  5603 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5604 | `	ph7_value *pKey;` |
|        - |  5605 | `	sxu32 nIdx;` |
|   194174 |  5606 | `	if( pInstr->iP1 ){` |
|        - |  5607 | `		/* Key is next on stack */` |
|    63340 |  5608 | `		pKey = pTos;` |
|    63340 |  5609 | `		pTos--;` |
|    31671 |  5610 | `	}else{` |
|   130836 |  5611 | `		pKey = 0;` |
|        - |  5612 | `	}` |
|   194174 |  5613 | `	nIdx = pTos->nIdx;` |
|        - |  5614 | `	{` |
|        - |  5615 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5616 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5617 | `		 * the backing variable slot at nIdx. */` |
|   194174 |  5618 | `		ph7_class_instance *pInst = 0;` |
|   194174 |  5619 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5620 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   194158 |  5621 | `		}else if( nIdx != SXU32_HIGH ){` |
|   194142 |  5622 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   194142 |  5623 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5624 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5625 | `			}` |
|    97070 |  5626 | `		}` |
|   194174 |  5627 | `		if( pInst ){` |
|       34 |  5628 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5629 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5630 | `				ph7_class_method *pMeth;` |
|        - |  5631 | `				ph7_value sNullKey;` |
|        - |  5632 | `				ph7_value *apArg[2];` |
|       32 |  5633 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5634 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5635 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5636 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5637 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5638 | `					break;` |
|        - |  5639 | `				}` |
|       32 |  5640 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5641 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5642 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5643 | `				VmPopOperand(&pTos,1);` |
|       32 |  5644 | `				if( pKey == 0 ){` |
|        7 |  5645 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5646 | `					apArg[0] = &sNullKey;` |
|        4 |  5647 | `				}else{` |
|       26 |  5648 | `					apArg[0] = pKey;` |
|        - |  5649 | `				}` |
|       32 |  5650 | `				apArg[1] = pTos;` |
|       32 |  5651 | `				if( pMeth ){` |
|       32 |  5652 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5653 | `				}` |
|       32 |  5654 | `				if( pKey ){` |
|       26 |  5655 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5656 | `				}else{` |
|        7 |  5657 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5658 | `				}` |
|        - |  5659 | `				/* Pop the value */` |
|       32 |  5660 | `				VmPopOperand(&pTos,1);` |
|       32 |  5661 | `				break;` |
|        - |  5662 | `			}` |
|        - |  5663 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5664 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5665 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5666 | `			 * a few lines below). Match PHP. */` |
|        - |  5667 | `			{` |
|        - |  5668 | `				char zMsg[256];` |
|        3 |  5669 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5670 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5671 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5672 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5673 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5674 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5675 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5676 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5677 | `				break;` |
|        - |  5678 | `			}` |
|        - |  5679 | `		}` |
|        - |  5680 | `	}` |
|   194142 |  5681 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5682 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5683 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5684 | `		 * checking true sharing count, then re-add after separation. */` |
|   194090 |  5685 | `		if( nIdx != SXU32_HIGH ){` |
|   194090 |  5686 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   291134 |  5687 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   194090 |  5688 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5689 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5690 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5691 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5692 | `				 * refcounts if the backing array was already separated. */` |
|   194090 |  5693 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   194090 |  5694 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   194090 |  5695 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   194090 |  5696 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   194090 |  5697 | `					pTos->x.pOther = pMap;` |
|    97046 |  5698 | `				}else{` |
|        - |  5699 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5700 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5701 | `					pMap = pCur;` |
|        - |  5702 | `				}` |
|    97046 |  5703 | `			}else{` |
|      ! 0 |  5704 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5705 | `			}` |
|    97046 |  5706 | `		}else{` |
|      ! 0 |  5707 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5708 | `		}` |
|   194090 |  5709 | `		if( pMap->iRef < 2 ){` |
|        - |  5710 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5711 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5712 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5713 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5714 | `			pMap->iRef = 2;` |
|      ! 0 |  5715 | `		}` |
|    97046 |  5716 | `	}else{` |
|        - |  5717 | `		ph7_value *pObj;` |
|       53 |  5718 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5719 | `		if( pObj == 0 ){` |
|      ! 0 |  5720 | `			if( pKey ){` |
|      ! 0 |  5721 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5722 | `			}` |
|      ! 0 |  5723 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5724 | `			break;` |
|        - |  5725 | `		}` |
|        - |  5726 | `		/* Phase#1: Load the array */` |
|       53 |  5727 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5728 | `			VmPopOperand(&pTos,1);` |
|       53 |  5729 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5730 | `				/* Force a string cast */` |
|      ! 0 |  5731 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5732 | `			}` |
|       53 |  5733 | `			if( pKey == 0 ){` |
|        - |  5734 | `				/* Append string */` |
|        3 |  5735 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5736 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5737 | `				}` |
|        2 |  5738 | `			}else{` |
|        - |  5739 | `				sxu32 nOfft;` |
|       51 |  5740 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5741 | `					/* Force an int cast */` |
|       51 |  5742 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5743 | `				}` |
|       51 |  5744 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5745 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5746 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5747 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5748 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5749 | `				}else{` |
|      ! 0 |  5750 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5751 | `						/* Perform an append operation */` |
|      ! 0 |  5752 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5753 | `					}` |
|        - |  5754 | `				}` |
|        - |  5755 | `			}` |
|       53 |  5756 | `			if( pKey ){` |
|       51 |  5757 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5758 | `			}` |
|       53 |  5759 | `			break;` |
|      ! 0 |  5760 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5761 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5762 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5763 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5764 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5765 | `				goto Abort;` |
|        - |  5766 | `			}` |
|      ! 0 |  5767 | `		}` |
|        - |  5768 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5769 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5770 | `	}` |
|   194090 |  5771 | `	VmPopOperand(&pTos,1);` |
|        - |  5772 | `	/* Phase#2: Perform the insertion */` |
|   194090 |  5773 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5774 | `		/* Insertion by reference */` |
|       15 |  5775 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5776 | `	}else{` |
|   194076 |  5777 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5778 | `	}` |
|   194090 |  5779 | `	if( pKey ){` |
|    63264 |  5780 | `		PH7_MemObjRelease(pKey);` |
|    31631 |  5781 | `	}` |
|   194090 |  5782 | `	break;` |
|        - |  5783 | `					   }` |
|        - |  5784 | `/*` |
|        - |  5785 | ` * INCR: P1 * *` |
|        - |  5786 | ` *` |
|        - |  5787 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5788 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5789 | ` * the stack and increment after that.` |
|        - |  5790 | ` */` |
|   167697 |  5791 | `case PH7_OP_INCR:` |
|        - |  5792 | `#ifdef UNTRUST` |
|        - |  5793 | `	if( pTos < pStack ){` |
|        - |  5794 | `		goto Abort;` |
|        - |  5795 | `	}` |
|        - |  5796 | `#endif` |
|   335440 |  5797 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335440 |  5798 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5799 | `			ph7_value *pObj;` |
|   335440 |  5800 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335440 |  5801 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5802 | `					/* Perl-style string increment.` |
|        - |  5803 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5804 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5805 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5806 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5807 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5808 | `					}` |
|       49 |  5809 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5810 | `					if( pInstr->iP1 ){` |
|        - |  5811 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5812 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5813 | `					}` |
|       25 |  5814 | `				}else{` |
|        - |  5815 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5816 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5817 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5818 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5819 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5820 | `					 * so its old-value view survives the coercion. */` |
|   335392 |  5821 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5822 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5823 | `					}` |
|        - |  5824 | `					/* Force a numeric cast on the variable */` |
|   335392 |  5825 | `					PH7_MemObjToNumeric(pObj);` |
|   335392 |  5826 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5827 | `						pObj->rVal++;` |
|        - |  5828 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5829 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5830 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5831 | `						 * integer-valued real. */` |
|        9 |  5832 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5833 | `					}else{` |
|   335384 |  5834 | `						pObj->x.iVal++;` |
|        - |  5835 | `					}` |
|   335392 |  5836 | `					if( pInstr->iP1 ){` |
|        - |  5837 | `						/* Pre-increment: result is the new value. */` |
|       83 |  5838 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  5839 | `					}` |
|        - |  5840 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5841 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5842 | `				}` |
|   167741 |  5843 | `			}` |
|   167743 |  5844 | `		}else{` |
|      ! 0 |  5845 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5846 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5847 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5848 | `				}else{` |
|        - |  5849 | `					/* Force a numeric cast */` |
|      ! 0 |  5850 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5851 | `					/* Pre-increment */` |
|      ! 0 |  5852 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5853 | `						pTos->rVal++;` |
|        - |  5854 | `						/* Try to get an integer representation */` |
|      ! 0 |  5855 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5856 | `					}else{` |
|      ! 0 |  5857 | `						pTos->x.iVal++;` |
|      ! 0 |  5858 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5859 | `					}` |
|        - |  5860 | `				}` |
|      ! 0 |  5861 | `			}` |
|        - |  5862 | `		}` |
|   167741 |  5863 | `	}` |
|   335440 |  5864 | `	break;` |
|        - |  5865 | `/*` |
|        - |  5866 | ` * DECR: P1 * *` |
|        - |  5867 | ` *` |
|        - |  5868 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5869 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5870 | ` * and decrement after that.` |
|        - |  5871 | ` */` |
|       14 |  5872 | `case PH7_OP_DECR:` |
|        - |  5873 | `#ifdef UNTRUST` |
|        - |  5874 | `	if( pTos < pStack ){` |
|        - |  5875 | `		goto Abort;` |
|        - |  5876 | `	}` |
|        - |  5877 | `#endif` |
|        - |  5878 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5879 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5880 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5881 | `			ph7_value *pObj;` |
|       27 |  5882 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5883 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5884 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5885 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5886 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5887 | `					if( pInstr->iP1 ){` |
|        - |  5888 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5889 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5890 | `					}` |
|        - |  5891 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5892 | `				}else{` |
|        - |  5893 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5894 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5895 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5896 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5897 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5898 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5899 | `					}` |
|       21 |  5900 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5901 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5902 | `						pObj->rVal--;` |
|        - |  5903 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5904 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5905 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5906 | `						 * integer-valued real. */` |
|        9 |  5907 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5908 | `					}else{` |
|       13 |  5909 | `						pObj->x.iVal--;` |
|        - |  5910 | `					}` |
|       21 |  5911 | `					if( pInstr->iP1 ){` |
|        - |  5912 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5913 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5914 | `					}` |
|        - |  5915 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5916 | `				}` |
|       13 |  5917 | `			}` |
|       14 |  5918 | `		}else{` |
|      ! 0 |  5919 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5920 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5921 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5922 | `				}else{` |
|        - |  5923 | `					/* Force a numeric cast */` |
|      ! 0 |  5924 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5925 | `					/* Pre-decrement */` |
|      ! 0 |  5926 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5927 | `						pTos->rVal--;` |
|        - |  5928 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5929 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5930 | `					}else{` |
|      ! 0 |  5931 | `						pTos->x.iVal--;` |
|      ! 0 |  5932 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5933 | `					}` |
|        - |  5934 | `				}` |
|      ! 0 |  5935 | `			}` |
|        - |  5936 | `		}` |
|       13 |  5937 | `	}` |
|       29 |  5938 | `	break;` |
|        - |  5939 | `/*` |
|        - |  5940 | ` * UMINUS: * * *` |
|        - |  5941 | ` *` |
|        - |  5942 | ` * Perform a unary minus operation.` |
|        - |  5943 | ` */` |
|    29735 |  5944 | `case PH7_OP_UMINUS:` |
|        - |  5945 | `#ifdef UNTRUST` |
|        - |  5946 | `	if( pTos < pStack ){` |
|        - |  5947 | `		goto Abort;` |
|        - |  5948 | `	}` |
|        - |  5949 | `#endif` |
|        - |  5950 | `	/* Force a numeric (integer,real or both) cast */` |
|    59472 |  5951 | `	PH7_MemObjToNumeric(pTos);` |
|    59472 |  5952 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5953 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5954 | `	}` |
|    59472 |  5955 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59442 |  5956 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29720 |  5957 | `	}` |
|    59472 |  5958 | `	break;` |
|        - |  5959 | `/*` |
|        - |  5960 | ` * UPLUS: * * *` |
|        - |  5961 | ` *` |
|        - |  5962 | ` * Perform a unary plus operation.` |
|        - |  5963 | ` */` |
|       18 |  5964 | `case PH7_OP_UPLUS:` |
|        - |  5965 | `#ifdef UNTRUST` |
|        - |  5966 | `	if( pTos < pStack ){` |
|        - |  5967 | `		goto Abort;` |
|        - |  5968 | `	}` |
|        - |  5969 | `#endif` |
|        - |  5970 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5971 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5972 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5973 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5974 | `	}` |
|       37 |  5975 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5976 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5977 | `	}` |
|       37 |  5978 | `	break;` |
|        - |  5979 | `/*` |
|        - |  5980 | ` * OP_LNOT: * * *` |
|        - |  5981 | ` *` |
|        - |  5982 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5983 | ` * with its complement.` |
|        - |  5984 | ` */` |
|    44855 |  5985 | `case PH7_OP_LNOT:` |
|        - |  5986 | `#ifdef UNTRUST` |
|        - |  5987 | `	if( pTos < pStack ){` |
|        - |  5988 | `		goto Abort;` |
|        - |  5989 | `	}` |
|        - |  5990 | `#endif` |
|        - |  5991 | `	/* Force a boolean cast */` |
|    89756 |  5992 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5993 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5994 | `	}` |
|    89756 |  5995 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89756 |  5996 | `	break;` |
|        - |  5997 | `/*` |
|        - |  5998 | ` * OP_BITNOT: * * *` |
|        - |  5999 | ` *` |
|        - |  6000 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6001 | ` * with its ones-complement.` |
|        - |  6002 | ` */` |
|       14 |  6003 | `case PH7_OP_BITNOT:` |
|        - |  6004 | `#ifdef UNTRUST` |
|        - |  6005 | `	if( pTos < pStack ){` |
|        - |  6006 | `		goto Abort;` |
|        - |  6007 | `	}` |
|        - |  6008 | `#endif` |
|        - |  6009 | `	/* Force an integer cast */` |
|       30 |  6010 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6011 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6012 | `	}` |
|       30 |  6013 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  6014 | `	break;` |
|        - |  6015 | `/* OP_MUL * * *` |
|        - |  6016 | ` * OP_MUL_STORE * * *` |
|        - |  6017 | ` *` |
|        - |  6018 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6019 | ` * and push the result back onto the stack.` |
|        - |  6020 | ` */` |
|     1290 |  6021 | `case PH7_OP_MUL:` |
|        - |  6022 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  6023 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6024 | `	/* Force the operand to be numeric */` |
|        - |  6025 | `#ifdef UNTRUST` |
|        - |  6026 | `	if( pNos < pStack ){` |
|        - |  6027 | `		goto Abort;` |
|        - |  6028 | `	}` |
|        - |  6029 | `#endif` |
|     2582 |  6030 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  6031 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6032 | `	/* Perform the requested operation */` |
|     2582 |  6033 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6034 | `		/* Floating point arithemic */` |
|        - |  6035 | `		ph7_real a,b,r;` |
|       21 |  6036 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6037 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6038 | `		}` |
|       21 |  6039 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6040 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6041 | `		}` |
|       21 |  6042 | `		a = pNos->rVal;` |
|       21 |  6043 | `		b = pTos->rVal;` |
|       21 |  6044 | `		r = a * b;` |
|        - |  6045 | `		/* Push the result */` |
|       21 |  6046 | `		pNos->rVal = r;` |
|       21 |  6047 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6048 | `		/* Try to get an integer representation */` |
|       21 |  6049 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6050 | `	}else{` |
|        - |  6051 | `		/* Integer arithmetic */` |
|        - |  6052 | `		sxi64 a,b,r;` |
|     2562 |  6053 | `		a = pNos->x.iVal;` |
|     2562 |  6054 | `		b = pTos->x.iVal;` |
|     2562 |  6055 | `		r = a * b;` |
|        - |  6056 | `		/* Push the result */` |
|     2562 |  6057 | `		pNos->x.iVal = r;` |
|     2562 |  6058 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6059 | `	}` |
|     2582 |  6060 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6061 | `		ph7_value *pObj;` |
|       32 |  6062 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6063 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6064 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6065 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6066 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6067 | `		}` |
|       15 |  6068 | `	}` |
|     2582 |  6069 | `	VmPopOperand(&pTos,1);` |
|     2582 |  6070 | `	break;` |
|        - |  6071 | `				 }` |
|        - |  6072 | `/* OP_POW * * *` |
|        - |  6073 | ` * OP_POW_STORE * * *` |
|        - |  6074 | ` *` |
|        - |  6075 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6076 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6077 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6078 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6079 | ` */` |
|       67 |  6080 | `case PH7_OP_POW:` |
|        - |  6081 | `case PH7_OP_POW_STORE: {` |
|      135 |  6082 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6083 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6084 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6085 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6086 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6087 | `	 */` |
|      135 |  6088 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6089 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6090 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6091 | `	int bBothInt;` |
|      135 |  6092 | `	int usedInt = 0;` |
|        - |  6093 | `	ph7_real a, b, r;` |
|        - |  6094 | `#endif` |
|      135 |  6095 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6096 | `#ifdef UNTRUST` |
|        - |  6097 | `	if( pNos < pStack ){` |
|        - |  6098 | `		goto Abort;` |
|        - |  6099 | `	}` |
|        - |  6100 | `#endif` |
|      135 |  6101 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6102 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6103 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6104 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6105 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6106 | `	if( bBothInt ){` |
|      123 |  6107 | `		base_i = pBase->x.iVal;` |
|      123 |  6108 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6109 | `	}` |
|      135 |  6110 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6111 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6112 | `	}` |
|      135 |  6113 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6114 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6115 | `	}` |
|      135 |  6116 | `	a = pBase->rVal;` |
|      135 |  6117 | `	b = pExp->rVal;` |
|      135 |  6118 | `	r = pow(a, b);` |
|        - |  6119 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6120 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6121 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6122 | `	 * representable as double but not as signed int64. */` |
|      135 |  6123 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6124 | `		sxi64 result_i = 1;` |
|      117 |  6125 | `		sxi64 cur_base = base_i;` |
|      117 |  6126 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6127 | `		int overflow = 0;` |
|      401 |  6128 | `		while( cur_exp > 0 ){` |
|      289 |  6129 | `			if( cur_exp & 1 ){` |
|      189 |  6130 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6131 | `					overflow = 1;` |
|        3 |  6132 | `					break;` |
|        - |  6133 | `				}` |
|       93 |  6134 | `			}` |
|      287 |  6135 | `			cur_exp >>= 1;` |
|      287 |  6136 | `			if( cur_exp > 0 ){` |
|      181 |  6137 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6138 | `					overflow = 1;` |
|        3 |  6139 | `					break;` |
|        - |  6140 | `				}` |
|       89 |  6141 | `			}` |
|        1 |  6142 | `		}` |
|      117 |  6143 | `		if( !overflow ){` |
|      113 |  6144 | `			pNos->x.iVal = result_i;` |
|      113 |  6145 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6146 | `			usedInt = 1;` |
|       56 |  6147 | `		}` |
|       58 |  6148 | `	}` |
|      135 |  6149 | `	if( !usedInt ){` |
|       23 |  6150 | `		pNos->rVal = r;` |
|       23 |  6151 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6152 | `	}` |
|        - |  6153 | `#else` |
|        - |  6154 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6155 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6156 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6157 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6158 | `	 * represented. */` |
|        - |  6159 | `	base_i = pBase->x.iVal;` |
|        - |  6160 | `	exp_i  = pExp->x.iVal;` |
|        - |  6161 | `	{` |
|        - |  6162 | `		sxi64 result_i = 1;` |
|        - |  6163 | `		sxi64 cur_base = base_i;` |
|        - |  6164 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6165 | `		if( cur_exp < 0 ){` |
|        - |  6166 | `			result_i = 0;` |
|        - |  6167 | `		}else{` |
|        - |  6168 | `			while( cur_exp > 0 ){` |
|        - |  6169 | `				if( cur_exp & 1 ){` |
|        - |  6170 | `					result_i *= cur_base;` |
|        - |  6171 | `				}` |
|        - |  6172 | `				cur_exp >>= 1;` |
|        - |  6173 | `				if( cur_exp > 0 ){` |
|        - |  6174 | `					cur_base *= cur_base;` |
|        - |  6175 | `				}` |
|        - |  6176 | `			}` |
|        - |  6177 | `		}` |
|        - |  6178 | `		pNos->x.iVal = result_i;` |
|        - |  6179 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6180 | `	}` |
|        - |  6181 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6182 | `	if( bStore ){` |
|        - |  6183 | `		ph7_value *pObj;` |
|       23 |  6184 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6185 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6186 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6187 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6188 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6189 | `		}` |
|       11 |  6190 | `	}` |
|      135 |  6191 | `	VmPopOperand(&pTos,1);` |
|      135 |  6192 | `	break;` |
|        - |  6193 | `				 }` |
|        - |  6194 | `/* OP_ADD * * *` |
|        - |  6195 | ` *` |
|        - |  6196 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6197 | ` * and push the result back onto the stack.` |
|        - |  6198 | ` */` |
|      535 |  6199 | `case PH7_OP_ADD:{` |
|     1072 |  6200 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6201 | `#ifdef UNTRUST` |
|        - |  6202 | `	if( pNos < pStack ){` |
|        - |  6203 | `		goto Abort;` |
|        - |  6204 | `	}` |
|        - |  6205 | `#endif` |
|        - |  6206 | `	/* Perform the addition */` |
|     1072 |  6207 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1072 |  6208 | `	VmPopOperand(&pTos,1);` |
|     1072 |  6209 | `	break;` |
|        - |  6210 | `				}` |
|        - |  6211 | `/*` |
|        - |  6212 | ` * OP_ADD_STORE * * *` |
|        - |  6213 | ` *` |
|        - |  6214 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6215 | ` * and push the result back onto the stack.` |
|        - |  6216 | ` */` |
|      502 |  6217 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  6218 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6219 | `	ph7_value *pObj;` |
|        - |  6220 | `	sxu32 nIdx;` |
|        - |  6221 | `#ifdef UNTRUST` |
|        - |  6222 | `	if( pNos < pStack ){` |
|        - |  6223 | `		goto Abort;` |
|        - |  6224 | `	}` |
|        - |  6225 | `#endif` |
|        - |  6226 | `	/* Perform the addition */` |
|     1006 |  6227 | `	nIdx = pTos->nIdx;` |
|     1006 |  6228 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6229 | `	/* Peform the store operation */` |
|     1006 |  6230 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6231 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  6232 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  6233 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  6234 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6235 | `	}` |
|        - |  6236 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  6237 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  6238 | `	VmPopOperand(&pTos,1);` |
|     1006 |  6239 | `	break;` |
|        - |  6240 | `				}` |
|        - |  6241 | `/* OP_SUB * * *` |
|        - |  6242 | ` *` |
|        - |  6243 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6244 | ` * first (what was next on the stack) from the second (the` |
|        - |  6245 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6246 | ` */` |
|      349 |  6247 | `case PH7_OP_SUB: {` |
|      700 |  6248 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6249 | `#ifdef UNTRUST` |
|        - |  6250 | `	if( pNos < pStack ){` |
|        - |  6251 | `		goto Abort;` |
|        - |  6252 | `	}` |
|        - |  6253 | `#endif` |
|      700 |  6254 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6255 | `		/* Floating point arithemic */` |
|        - |  6256 | `		ph7_real a,b,r;` |
|       97 |  6257 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6258 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6259 | `		}` |
|       97 |  6260 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6261 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6262 | `		}` |
|       97 |  6263 | `		a = pNos->rVal;` |
|       97 |  6264 | `		b = pTos->rVal;` |
|       97 |  6265 | `		r = a - b;` |
|        - |  6266 | `		/* Push the result */` |
|       97 |  6267 | `		pNos->rVal = r;` |
|       97 |  6268 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6269 | `		/* Try to get an integer representation */` |
|       97 |  6270 | `		PH7_MemObjTryInteger(pNos);` |
|       49 |  6271 | `	}else{` |
|        - |  6272 | `		/* Integer arithmetic */` |
|        - |  6273 | `		sxi64 a,b,r;` |
|      604 |  6274 | `		a = pNos->x.iVal;` |
|      604 |  6275 | `		b = pTos->x.iVal;` |
|      604 |  6276 | `		r = a - b;` |
|        - |  6277 | `		/* Push the result */` |
|      604 |  6278 | `		pNos->x.iVal = r;` |
|      604 |  6279 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6280 | `	}` |
|      700 |  6281 | `	VmPopOperand(&pTos,1);` |
|      700 |  6282 | `	break;` |
|        - |  6283 | `				 }` |
|        - |  6284 | `/* OP_SUB_STORE * * *` |
|        - |  6285 | ` *` |
|        - |  6286 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6287 | ` * first (what was next on the stack) from the second (the` |
|        - |  6288 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6289 | ` */` |
|        4 |  6290 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6291 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6292 | `	ph7_value *pObj;` |
|        - |  6293 | `#ifdef UNTRUST` |
|        - |  6294 | `	if( pNos < pStack ){` |
|        - |  6295 | `		goto Abort;` |
|        - |  6296 | `	}` |
|        - |  6297 | `#endif` |
|       10 |  6298 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6299 | `		/* Floating point arithemic */` |
|        - |  6300 | `		ph7_real a,b,r;` |
|      ! 0 |  6301 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6302 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6303 | `		}` |
|      ! 0 |  6304 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6305 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6306 | `		}` |
|      ! 0 |  6307 | `		a = pTos->rVal;` |
|      ! 0 |  6308 | `		b = pNos->rVal;` |
|      ! 0 |  6309 | `		r = a - b;` |
|        - |  6310 | `		/* Push the result */` |
|      ! 0 |  6311 | `		pNos->rVal = r;` |
|      ! 0 |  6312 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6313 | `		/* Try to get an integer representation */` |
|      ! 0 |  6314 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6315 | `	}else{` |
|        - |  6316 | `		/* Integer arithmetic */` |
|        - |  6317 | `		sxi64 a,b,r;` |
|       10 |  6318 | `		a = pTos->x.iVal;` |
|       10 |  6319 | `		b = pNos->x.iVal;` |
|       10 |  6320 | `		r = a - b;` |
|        - |  6321 | `		/* Push the result */` |
|       10 |  6322 | `		pNos->x.iVal = r;` |
|       10 |  6323 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6324 | `	}` |
|       10 |  6325 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6326 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6327 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6328 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6329 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6330 | `	}` |
|       10 |  6331 | `	VmPopOperand(&pTos,1);` |
|       10 |  6332 | `	break;` |
|        - |  6333 | `				 }` |
|        - |  6334 |  |
|        - |  6335 | `/*` |
|        - |  6336 | ` * OP_MOD * * *` |
|        - |  6337 | ` *` |
|        - |  6338 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6339 | ` * first (what was next on the stack) from the second (the` |
|        - |  6340 | ` * top of the stack) and push the remainder after division` |
|        - |  6341 | ` * onto the stack.` |
|        - |  6342 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6343 | ` */` |
|      309 |  6344 | `case PH7_OP_MOD:{` |
|      620 |  6345 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6346 | `	sxi64 a,b,r;` |
|        - |  6347 | `#ifdef UNTRUST` |
|        - |  6348 | `	if( pNos < pStack ){` |
|        - |  6349 | `		goto Abort;` |
|        - |  6350 | `	}` |
|        - |  6351 | `#endif` |
|        - |  6352 | `	/* Force the operands to be integer */` |
|      620 |  6353 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6354 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6355 | `	}` |
|      620 |  6356 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6357 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6358 | `	}` |
|        - |  6359 | `	/* Perform the requested operation */` |
|      620 |  6360 | `	a = pNos->x.iVal;` |
|      620 |  6361 | `	b = pTos->x.iVal;` |
|      620 |  6362 | `	if( b == 0 ){` |
|        3 |  6363 | `		r = 0;` |
|        3 |  6364 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6365 | `		/* goto Abort; */` |
|        2 |  6366 | `	}else{` |
|      617 |  6367 | `		r = a%b;` |
|        - |  6368 | `	}` |
|        - |  6369 | `	/* Push the result */` |
|      620 |  6370 | `	pNos->x.iVal = r;` |
|      620 |  6371 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      620 |  6372 | `	VmPopOperand(&pTos,1);` |
|      620 |  6373 | `	break;` |
|        - |  6374 | `				}` |
|        - |  6375 | `/*` |
|        - |  6376 | ` * OP_MOD_STORE * * *` |
|        - |  6377 | ` *` |
|        - |  6378 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6379 | ` * first (what was next on the stack) from the second (the` |
|        - |  6380 | ` * top of the stack) and push the remainder after division` |
|        - |  6381 | ` * onto the stack.` |
|        - |  6382 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6383 | ` */` |
|        1 |  6384 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6385 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6386 | `	ph7_value *pObj;` |
|        - |  6387 | `	sxi64 a,b,r;` |
|        - |  6388 | `#ifdef UNTRUST` |
|        - |  6389 | `	if( pNos < pStack ){` |
|        - |  6390 | `		goto Abort;` |
|        - |  6391 | `	}` |
|        - |  6392 | `#endif` |
|        - |  6393 | `	/* Force the operands to be integer */` |
|        3 |  6394 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6395 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6396 | `	}` |
|        3 |  6397 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6398 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6399 | `	}` |
|        - |  6400 | `	/* Perform the requested operation */` |
|        3 |  6401 | `	a = pTos->x.iVal;` |
|        3 |  6402 | `	b = pNos->x.iVal;` |
|        3 |  6403 | `	if( b == 0 ){` |
|      ! 0 |  6404 | `		r = 0;` |
|      ! 0 |  6405 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6406 | `		/* goto Abort; */` |
|      ! 0 |  6407 | `	}else{` |
|        3 |  6408 | `		r = a%b;` |
|        - |  6409 | `	}` |
|        - |  6410 | `	/* Push the result */` |
|        3 |  6411 | `	pNos->x.iVal = r;` |
|        3 |  6412 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6413 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6414 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6415 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6416 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6417 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6418 | `	}` |
|        3 |  6419 | `	VmPopOperand(&pTos,1);` |
|        3 |  6420 | `	break;` |
|        - |  6421 | `				}` |
|        - |  6422 | `/*` |
|        - |  6423 | ` * OP_DIV * * *` |
|        - |  6424 | ` *` |
|        - |  6425 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6426 | ` * first (what was next on the stack) from the second (the` |
|        - |  6427 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6428 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6429 | ` */` |
|       33 |  6430 | `case PH7_OP_DIV:{` |
|       68 |  6431 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6432 | `	ph7_real a,b,r;` |
|        - |  6433 | `#ifdef UNTRUST` |
|        - |  6434 | `	if( pNos < pStack ){` |
|        - |  6435 | `		goto Abort;` |
|        - |  6436 | `	}` |
|        - |  6437 | `#endif` |
|        - |  6438 | `	/* Force the operands to be real */` |
|       68 |  6439 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6440 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6441 | `	}` |
|       68 |  6442 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6443 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6444 | `	}` |
|        - |  6445 | `	/* Perform the requested operation */` |
|       68 |  6446 | `	a = pNos->rVal;` |
|       68 |  6447 | `	b = pTos->rVal;` |
|       68 |  6448 | `	if( b == 0 ){` |
|        - |  6449 | `		/* Division by zero */` |
|        3 |  6450 | `		pNos->rVal = 0;` |
|        3 |  6451 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6452 | `		/* goto Abort; */` |
|        2 |  6453 | `	}else{` |
|       65 |  6454 | `		r = a/b;` |
|        - |  6455 | `		/* Push the result */` |
|       65 |  6456 | `		pNos->rVal = r;` |
|       65 |  6457 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6458 | `		/* Try to get an integer representation */` |
|       65 |  6459 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6460 | `	}` |
|       68 |  6461 | `	VmPopOperand(&pTos,1);` |
|       68 |  6462 | `	break;` |
|        - |  6463 | `				}` |
|        - |  6464 | `/*` |
|        - |  6465 | ` * OP_DIV_STORE * * *` |
|        - |  6466 | ` *` |
|        - |  6467 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6468 | ` * first (what was next on the stack) from the second (the` |
|        - |  6469 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6470 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6471 | ` */` |
|        2 |  6472 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6473 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6474 | `	ph7_value *pObj;` |
|        - |  6475 | `	ph7_real a,b,r;` |
|        - |  6476 | `#ifdef UNTRUST` |
|        - |  6477 | `	if( pNos < pStack ){` |
|        - |  6478 | `		goto Abort;` |
|        - |  6479 | `	}` |
|        - |  6480 | `#endif` |
|        - |  6481 | `	/* Force the operands to be real */` |
|        5 |  6482 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6483 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6484 | `	}` |
|        5 |  6485 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6486 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6487 | `	}` |
|        - |  6488 | `	/* Perform the requested operation */` |
|        5 |  6489 | `	a = pTos->rVal;` |
|        5 |  6490 | `	b = pNos->rVal;` |
|        5 |  6491 | `	if( b == 0 ){` |
|        - |  6492 | `		/* Division by zero */` |
|      ! 0 |  6493 | `		r = 0;` |
|      ! 0 |  6494 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6495 | `		/* goto Abort; */` |
|      ! 0 |  6496 | `	}else{` |
|        5 |  6497 | `		r = a/b;` |
|        - |  6498 | `		/* Push the result */` |
|        5 |  6499 | `		pNos->rVal = r;` |
|        5 |  6500 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6501 | `		/* Try to get an integer representation */` |
|        5 |  6502 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6503 | `	}` |
|        5 |  6504 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6505 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6506 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6507 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6508 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6509 | `	}` |
|        5 |  6510 | `	VmPopOperand(&pTos,1);` |
|        5 |  6511 | `	break;` |
|        - |  6512 | `				}` |
|        - |  6513 | `/* OP_BAND * * *` |
|        - |  6514 | ` *` |
|        - |  6515 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6516 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6517 | ` * two elements.` |
|        - |  6518 | `*/` |
|        - |  6519 | `/* OP_BOR * * *` |
|        - |  6520 | ` *` |
|        - |  6521 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6522 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6523 | ` * two elements.` |
|        - |  6524 | ` */` |
|        - |  6525 | `/* OP_BXOR * * *` |
|        - |  6526 | ` *` |
|        - |  6527 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6528 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6529 | ` * two elements.` |
|        - |  6530 | ` */` |
|       43 |  6531 | `case PH7_OP_BAND:` |
|        - |  6532 | `case PH7_OP_BOR:` |
|        - |  6533 | `case PH7_OP_BXOR:{` |
|       88 |  6534 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6535 | `	sxi64 a,b,r;` |
|        - |  6536 | `#ifdef UNTRUST` |
|        - |  6537 | `	if( pNos < pStack ){` |
|        - |  6538 | `		goto Abort;` |
|        - |  6539 | `	}` |
|        - |  6540 | `#endif` |
|        - |  6541 | `	/* Force the operands to be integer */` |
|       88 |  6542 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6543 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6544 | `	}` |
|       88 |  6545 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6546 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6547 | `	}` |
|        - |  6548 | `	/* Perform the requested operation */` |
|       88 |  6549 | `	a = pNos->x.iVal;` |
|       88 |  6550 | `	b = pTos->x.iVal;` |
|       88 |  6551 | `	switch(pInstr->iOp){` |
|        7 |  6552 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6553 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6554 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6555 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6556 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6557 | `	case PH7_OP_BAND:` |
|       60 |  6558 | `	default:          r = a&b; break;` |
|        - |  6559 | `	}` |
|        - |  6560 | `	/* Push the result */` |
|       88 |  6561 | `	pNos->x.iVal = r;` |
|       88 |  6562 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6563 | `	VmPopOperand(&pTos,1);` |
|       88 |  6564 | `	break;` |
|        - |  6565 | `				 }` |
|        - |  6566 | `/* OP_BAND_STORE * * *` |
|        - |  6567 | ` *` |
|        - |  6568 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6569 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6570 | ` * two elements.` |
|        - |  6571 | `*/` |
|        - |  6572 | `/* OP_BOR_STORE * * *` |
|        - |  6573 | ` *` |
|        - |  6574 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6575 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6576 | ` * two elements.` |
|        - |  6577 | ` */` |
|        - |  6578 | `/* OP_BXOR_STORE * * *` |
|        - |  6579 | ` *` |
|        - |  6580 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6581 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6582 | ` * two elements.` |
|        - |  6583 | ` */` |
|       10 |  6584 | `case PH7_OP_BAND_STORE:` |
|        - |  6585 | `case PH7_OP_BOR_STORE:` |
|        - |  6586 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6587 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6588 | `	ph7_value *pObj;` |
|        - |  6589 | `	sxi64 a,b,r;` |
|        - |  6590 | `#ifdef UNTRUST` |
|        - |  6591 | `	if( pNos < pStack ){` |
|        - |  6592 | `		goto Abort;` |
|        - |  6593 | `	}` |
|        - |  6594 | `#endif` |
|        - |  6595 | `	/* Force the operands to be integer */` |
|       21 |  6596 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6597 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6598 | `	}` |
|       21 |  6599 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6600 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6601 | `	}` |
|        - |  6602 | `	/* Perform the requested operation */` |
|       21 |  6603 | `	a = pTos->x.iVal;` |
|       21 |  6604 | `	b = pNos->x.iVal;` |
|       21 |  6605 | `	switch(pInstr->iOp){` |
|        3 |  6606 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6607 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6608 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6609 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6610 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6611 | `	case PH7_OP_BAND:` |
|        7 |  6612 | `	default:          r = a&b; break;` |
|        - |  6613 | `	}` |
|        - |  6614 | `	/* Push the result */` |
|       21 |  6615 | `	pNos->x.iVal = r;` |
|       21 |  6616 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6617 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6618 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6619 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6620 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6621 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6622 | `	}` |
|       21 |  6623 | `	VmPopOperand(&pTos,1);` |
|       21 |  6624 | `	break;` |
|        - |  6625 | `				 }` |
|        - |  6626 | `/* OP_SHL * * *` |
|        - |  6627 | ` *` |
|        - |  6628 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6629 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6630 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6631 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6632 | ` */` |
|        - |  6633 | `/* OP_SHR * * *` |
|        - |  6634 | ` *` |
|        - |  6635 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6636 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6637 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6638 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6639 | ` */` |
|       12 |  6640 | `case PH7_OP_SHL:` |
|        - |  6641 | `case PH7_OP_SHR: {` |
|       25 |  6642 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6643 | `	sxi64 a,r;` |
|        - |  6644 | `	sxi32 b;` |
|        - |  6645 | `#ifdef UNTRUST` |
|        - |  6646 | `	if( pNos < pStack ){` |
|        - |  6647 | `		goto Abort;` |
|        - |  6648 | `	}` |
|        - |  6649 | `#endif` |
|        - |  6650 | `	/* Force the operands to be integer */` |
|       25 |  6651 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6652 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6653 | `	}` |
|       25 |  6654 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6655 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6656 | `	}` |
|        - |  6657 | `	/* Perform the requested operation */` |
|       25 |  6658 | `	a = pNos->x.iVal;` |
|       25 |  6659 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6660 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6661 | `		r = a << b;` |
|        8 |  6662 | `	}else{` |
|       11 |  6663 | `		r = a >> b;` |
|        - |  6664 | `	}` |
|        - |  6665 | `	/* Push the result */` |
|       25 |  6666 | `	pNos->x.iVal = r;` |
|       25 |  6667 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6668 | `	VmPopOperand(&pTos,1);` |
|       25 |  6669 | `	break;` |
|        - |  6670 | `				 }` |
|        - |  6671 | `/*  OP_SHL_STORE * * *` |
|        - |  6672 | ` *` |
|        - |  6673 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6674 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6675 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6676 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6677 | ` */` |
|        - |  6678 | `/* OP_SHR_STORE * * *` |
|        - |  6679 | ` *` |
|        - |  6680 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6681 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6682 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6683 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6684 | ` */` |
|        9 |  6685 | `case PH7_OP_SHL_STORE:` |
|        - |  6686 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6687 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6688 | `	ph7_value *pObj;` |
|        - |  6689 | `	sxi64 a,r;` |
|        - |  6690 | `	sxi32 b;` |
|        - |  6691 | `#ifdef UNTRUST` |
|        - |  6692 | `	if( pNos < pStack ){` |
|        - |  6693 | `		goto Abort;` |
|        - |  6694 | `	}` |
|        - |  6695 | `#endif` |
|        - |  6696 | `	/* Force the operands to be integer */` |
|       19 |  6697 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6698 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6699 | `	}` |
|       19 |  6700 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6701 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6702 | `	}` |
|        - |  6703 | `	/* Perform the requested operation */` |
|       19 |  6704 | `	a = pTos->x.iVal;` |
|       19 |  6705 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6706 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6707 | `		r = a << b;` |
|        5 |  6708 | `	}else{` |
|       11 |  6709 | `		r = a >> b;` |
|        - |  6710 | `	}` |
|        - |  6711 | `	/* Push the result */` |
|       19 |  6712 | `	pNos->x.iVal = r;` |
|       19 |  6713 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6714 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6715 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6716 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6717 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6718 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6719 | `	}` |
|       19 |  6720 | `	VmPopOperand(&pTos,1);` |
|       19 |  6721 | `	break;` |
|        - |  6722 | `				 }` |
|        - |  6723 | `/* CAT:  P1 * *` |
|        - |  6724 | ` *` |
|        - |  6725 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6726 | ` * back.` |
|        - |  6727 | ` */` |
|    71880 |  6728 | `case PH7_OP_CAT:{` |
|        - |  6729 | `	ph7_value *pNos,*pCur;` |
|   143762 |  6730 | `	if( pInstr->iP1 < 1 ){` |
|   116276 |  6731 | `		pNos = &pTos[-1];` |
|    58139 |  6732 | `	}else{` |
|    27488 |  6733 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6734 | `	}` |
|        - |  6735 | `#ifdef UNTRUST` |
|        - |  6736 | `	if( pNos < pStack ){` |
|        - |  6737 | `		goto Abort;` |
|        - |  6738 | `	}` |
|        - |  6739 | `#endif` |
|        - |  6740 | `	/* Force a string cast */` |
|   143762 |  6741 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6742 | `		PH7_MemObjToString(pNos);` |
|      835 |  6743 | `	}` |
|   143762 |  6744 | `	pCur = &pNos[1];` |
|   290252 |  6745 | `	while( pCur <= pTos ){` |
|   146492 |  6746 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50952 |  6747 | `			PH7_MemObjToString(pCur);` |
|    25475 |  6748 | `		}` |
|        - |  6749 | `		/* Perform the concatenation */` |
|   146492 |  6750 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146448 |  6751 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6752 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6753 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6754 | `				goto Abort;` |
|        - |  6755 | `			}` |
|    73223 |  6756 | `		}` |
|   146492 |  6757 | `		SyBlobRelease(&pCur->sBlob);` |
|   146492 |  6758 | `		pCur++;` |
|        2 |  6759 | `	}` |
|   143762 |  6760 | `	pTos = pNos;` |
|   143762 |  6761 | `	break;` |
|        - |  6762 | `				}` |
|        - |  6763 | `/*  CAT_STORE: * * *` |
|        - |  6764 | ` *` |
|        - |  6765 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6766 | ` * back.` |
|        - |  6767 | ` */` |
|     4121 |  6768 | `case PH7_OP_CAT_STORE:{` |
|     8244 |  6769 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6770 | `	ph7_value *pObj;` |
|        - |  6771 | `#ifdef UNTRUST` |
|        - |  6772 | `	if( pNos < pStack ){` |
|        - |  6773 | `		goto Abort;` |
|        - |  6774 | `	}` |
|        - |  6775 | `#endif` |
|     8244 |  6776 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6777 | `		/* Force a string cast */` |
|        3 |  6778 | `		PH7_MemObjToString(pTos);` |
|        1 |  6779 | `	}` |
|     8244 |  6780 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6781 | `		/* Force a string cast */` |
|      ! 0 |  6782 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6783 | `	}` |
|        - |  6784 | `	/* Perform the concatenation (Reverse order) */` |
|     8244 |  6785 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8244 |  6786 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6787 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  6788 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  6789 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6790 | `			goto Abort;` |
|        - |  6791 | `		}` |
|     4121 |  6792 | `	}` |
|        - |  6793 | `	/* Perform the store operation */` |
|     8244 |  6794 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6795 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8244 |  6796 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8244 |  6797 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8242 |  6798 | `		PH7_MemObjStore(pTos,pObj);` |
|     4120 |  6799 | `	}` |
|     8242 |  6800 | `	PH7_MemObjStore(pTos,pNos);` |
|     8242 |  6801 | `	VmPopOperand(&pTos,1);` |
|     8242 |  6802 | `	break;` |
|        - |  6803 | `				}` |
|        - |  6804 | `/* OP_AND: * * *` |
|        - |  6805 | ` *` |
|        - |  6806 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6807 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6808 | ` * stack.` |
|        - |  6809 | ` */` |
|        - |  6810 | `/* OP_OR: * * *` |
|        - |  6811 | ` *` |
|        - |  6812 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6813 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6814 | ` * stack.` |
|        - |  6815 | ` */` |
|   108249 |  6816 | `case PH7_OP_LAND:` |
|        - |  6817 | `case PH7_OP_LOR: {` |
|   216544 |  6818 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6819 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6820 | `#ifdef UNTRUST` |
|        - |  6821 | `	if( pNos < pStack ){` |
|        - |  6822 | `		goto Abort;` |
|        - |  6823 | `	}` |
|        - |  6824 | `#endif` |
|        - |  6825 | `	/* Force a boolean cast */` |
|   216544 |  6826 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6827 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6828 | `	}` |
|   216544 |  6829 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6830 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6831 | `	}` |
|   216544 |  6832 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216544 |  6833 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216544 |  6834 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6835 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99408 |  6836 | `		v1 = and_logic[v1*3+v2];` |
|    49727 |  6837 | `	}else{` |
|        - |  6838 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117138 |  6839 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6840 | `	}` |
|   216544 |  6841 | `	if( v1 == 2 ){` |
|      ! 0 |  6842 | `		v1 = 1;` |
|      ! 0 |  6843 | `	}` |
|   216544 |  6844 | `	VmPopOperand(&pTos,1);` |
|   216544 |  6845 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216544 |  6846 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216544 |  6847 | `	break;` |
|        - |  6848 | `				 }` |
|        - |  6849 | `/*` |
|        - |  6850 | ` * OP_NULLC: * * *` |
|        - |  6851 | ` * Null coalescing operator '??'.` |
|        - |  6852 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6853 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6854 | ` */` |
|        - |  6855 | `/*` |
|        - |  6856 | ` * OP_NULLC: * P2 *` |
|        - |  6857 | ` * Short-circuit null coalescing '??'.` |
|        - |  6858 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6859 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6860 | ` */` |
|       99 |  6861 | `case PH7_OP_NULLC: {` |
|        - |  6862 | `#ifdef UNTRUST` |
|        - |  6863 | `	if( pTos < pStack ){` |
|        - |  6864 | `		goto Abort;` |
|        - |  6865 | `	}` |
|        - |  6866 | `#endif` |
|      200 |  6867 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6868 | `		/* Left is not null — keep it and skip the RHS */` |
|      120 |  6869 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       61 |  6870 | `	}else{` |
|        - |  6871 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       82 |  6872 | `		VmPopOperand(&pTos, 1);` |
|        - |  6873 | `	}` |
|      200 |  6874 | `	break;` |
|        - |  6875 |  |
|        - |  6876 | `/*` |
|        - |  6877 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6878 | ` * Null coalescing assignment short-circuit.` |
|        - |  6879 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6880 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6881 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6882 | ` */` |
|       28 |  6883 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6884 | `#ifdef UNTRUST` |
|        - |  6885 | `	if( pTos < pStack ){` |
|        - |  6886 | `		goto Abort;` |
|        - |  6887 | `	}` |
|        - |  6888 | `#endif` |
|       58 |  6889 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6890 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6891 | `	}` |
|       58 |  6892 | `	break;` |
|        - |  6893 |  |
|        - |  6894 | `/*` |
|        - |  6895 | ` * OP_NULLC_STORE: * * *` |
|        - |  6896 | ` * Null coalescing assignment store.` |
|        - |  6897 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6898 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6899 | ` * expression result.` |
|        - |  6900 | ` */` |
|        - |  6901 | `/*` |
|        - |  6902 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6903 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6904 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6905 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6906 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6907 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6908 | ` */` |
|       51 |  6909 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6910 | `#ifdef UNTRUST` |
|        - |  6911 | `	if( pTos < pStack ){` |
|        - |  6912 | `		goto Abort;` |
|        - |  6913 | `	}` |
|        - |  6914 | `#endif` |
|      104 |  6915 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6916 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6917 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6918 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6919 | `	}` |
|      104 |  6920 | `	break;` |
|        - |  6921 |  |
|       17 |  6922 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6923 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6924 | `	ph7_value *pObj;` |
|        - |  6925 | `	sxu32 nIdx;` |
|        - |  6926 | `#ifdef UNTRUST` |
|        - |  6927 | `	if( pNos < pStack ){` |
|        - |  6928 | `		goto Abort;` |
|        - |  6929 | `	}` |
|        - |  6930 | `#endif` |
|        - |  6931 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6932 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6933 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6934 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6935 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6936 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6937 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6938 | `		ph7_value *apArg[2];` |
|        5 |  6939 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6940 | `		apArg[1] = pTos;` |
|        5 |  6941 | `		if( pSet ){` |
|        5 |  6942 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6943 | `		}` |
|        - |  6944 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6945 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6946 | `		VmPopOperand(&pTos,1);` |
|        - |  6947 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6948 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6949 | `		break;` |
|        - |  6950 | `	}` |
|       32 |  6951 | `	nIdx = pNos->nIdx;` |
|       32 |  6952 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6953 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6954 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6955 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6956 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6957 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6958 | `	}` |
|       32 |  6959 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6960 | `	VmPopOperand(&pTos,1);` |
|       32 |  6961 | `	break;` |
|        - |  6962 |  |
|        - |  6963 | `/*` |
|        - |  6964 | ` * OP_SPREAD: * * *` |
|        - |  6965 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6966 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6967 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6968 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6969 | ` */` |
|        9 |  6970 | `case PH7_OP_SPREAD: {` |
|        - |  6971 | `#ifdef UNTRUST` |
|        - |  6972 | `	if( pTos < pStack ){` |
|        - |  6973 | `		goto Abort;` |
|        - |  6974 | `	}` |
|        - |  6975 | `#endif` |
|       20 |  6976 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6977 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6978 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6979 | `		if( nEntry == 0 ){` |
|        - |  6980 | `			/* Empty array — remove from stack */` |
|        3 |  6981 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6982 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6983 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6984 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6985 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6986 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6987 | `				VM_STACK_GUARD);` |
|      ! 0 |  6988 | `		}else{` |
|        - |  6989 | `			ph7_hashmap_node *pNode2;` |
|        - |  6990 | `			ph7_value *pElem;` |
|        - |  6991 | `			sxu32 i;` |
|        - |  6992 | `			/* Overwrite TOS with first element */` |
|       18 |  6993 | `			pNode2 = pMap->pFirst;` |
|       18 |  6994 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6995 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6996 | `			if( pElem ){` |
|       18 |  6997 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6998 | `			}` |
|       18 |  6999 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7000 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7001 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  7002 | `			pNode2 = pNode2->pPrev;` |
|        - |  7003 | `			/* Push remaining elements */` |
|       44 |  7004 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  7005 | `				pTos++;` |
|       28 |  7006 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  7007 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  7008 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  7009 | `				if( pElem ){` |
|       28 |  7010 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7011 | `				}` |
|       28 |  7012 | `				pNode2 = pNode2->pPrev;` |
|       15 |  7013 | `			}` |
|       18 |  7014 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7015 | `		}` |
|        9 |  7016 | `	}` |
|        - |  7017 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  7018 | `	break;` |
|        - |  7019 |  |
|        - |  7020 | `/*` |
|        - |  7021 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7022 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7023 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7024 | ` */` |
|       34 |  7025 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7026 | `#ifdef UNTRUST` |
|        - |  7027 | `	if( pTos < pStack ){` |
|        - |  7028 | `		goto Abort;` |
|        - |  7029 | `	}` |
|        - |  7030 | `#endif` |
|       70 |  7031 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  7032 | `	break;` |
|        - |  7033 |  |
|        - |  7034 | `/* OP_LXOR: * * *` |
|        - |  7035 | ` *` |
|        - |  7036 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7037 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7038 | ` * stack.` |
|        - |  7039 | ` * According to the PHP language reference manual:` |
|        - |  7040 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7041 | ` *  TRUE,but not both.` |
|        - |  7042 | ` */` |
|        5 |  7043 | `case PH7_OP_LXOR:{` |
|       11 |  7044 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7045 | `	sxi32 v = 0;` |
|        - |  7046 | `#ifdef UNTRUST` |
|        - |  7047 | `	if( pNos < pStack ){` |
|        - |  7048 | `		goto Abort;` |
|        - |  7049 | `	}` |
|        - |  7050 | `#endif` |
|        - |  7051 | `	/* Force a boolean cast */` |
|       11 |  7052 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7053 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7054 | `	}` |
|       11 |  7055 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7056 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7057 | `	}` |
|       11 |  7058 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7059 | `		v = 1;` |
|        3 |  7060 | `	}` |
|       11 |  7061 | `	VmPopOperand(&pTos,1);` |
|       11 |  7062 | `	pTos->x.iVal = v;` |
|       11 |  7063 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7064 | `	break;` |
|        - |  7065 | `				 }` |
|        - |  7066 | `/* OP_EQ P1 P2 P3` |
|        - |  7067 | ` *` |
|        - |  7068 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7069 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7070 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7071 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7072 | ` */` |
|        - |  7073 | `/* OP_NEQ P1 P2 P3` |
|        - |  7074 | ` *` |
|        - |  7075 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7076 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7077 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7078 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7079 | ` */` |
|     4583 |  7080 | `case PH7_OP_EQ:` |
|        - |  7081 | `case PH7_OP_NEQ: {` |
|     9168 |  7082 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7083 | `	/* Perform the comparison and act accordingly */` |
|        - |  7084 | `#ifdef UNTRUST` |
|        - |  7085 | `	if( pNos < pStack ){` |
|        - |  7086 | `		goto Abort;` |
|        - |  7087 | `	}` |
|        - |  7088 | `#endif` |
|     9168 |  7089 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9168 |  7090 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7091 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9159 |  7092 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9124 |  7093 | `		rc = rc == 0;` |
|     4563 |  7094 | `	}else{` |
|       28 |  7095 | `		rc = rc != 0;` |
|        - |  7096 | `	}` |
|     9168 |  7097 | `	VmPopOperand(&pTos,1);` |
|     9168 |  7098 | `	if( !pInstr->iP2 ){` |
|        - |  7099 | `		/* Push comparison result without taking the jump */` |
|     9168 |  7100 | `		PH7_MemObjRelease(pTos);` |
|     9168 |  7101 | `		pTos->x.iVal = rc;` |
|        - |  7102 | `		/* Invalidate any prior representation */` |
|     9168 |  7103 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4585 |  7104 | `	}else{` |
|      ! 0 |  7105 | `		if( rc ){` |
|        - |  7106 | `			/* Jump to the desired location */` |
|      ! 0 |  7107 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7108 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7109 | `		}` |
|        - |  7110 | `	}` |
|     9168 |  7111 | `	break;` |
|        - |  7112 | `				 }` |
|        - |  7113 | `/* OP_TEQ P1 P2 *` |
|        - |  7114 | ` *` |
|        - |  7115 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7116 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7117 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7118 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7119 | ` */` |
|   161574 |  7120 | `case PH7_OP_TEQ: {` |
|   323150 |  7121 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7122 | `	/* Perform the comparison and act accordingly */` |
|        - |  7123 | `#ifdef UNTRUST` |
|        - |  7124 | `	if( pNos < pStack ){` |
|        - |  7125 | `		goto Abort;` |
|        - |  7126 | `	}` |
|        - |  7127 | `#endif` |
|   323150 |  7128 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   323150 |  7129 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7130 | `		rc = 0;` |
|        2 |  7131 | `	}else{` |
|   323148 |  7132 | `		rc = rc == 0;` |
|        - |  7133 | `	}` |
|   323150 |  7134 | `	VmPopOperand(&pTos,1);` |
|   323150 |  7135 | `	if( !pInstr->iP2 ){` |
|        - |  7136 | `		/* Push comparison result without taking the jump */` |
|   323150 |  7137 | `		PH7_MemObjRelease(pTos);` |
|   323150 |  7138 | `		pTos->x.iVal = rc;` |
|        - |  7139 | `		/* Invalidate any prior representation */` |
|   323150 |  7140 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   161576 |  7141 | `	}else{` |
|      ! 0 |  7142 | `		if( rc ){` |
|        - |  7143 | `			/* Jump to the desired location */` |
|      ! 0 |  7144 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7145 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7146 | `		}` |
|        - |  7147 | `	}` |
|   323150 |  7148 | `	break;` |
|        - |  7149 | `				 }` |
|        - |  7150 | `/* OP_TNE P1 P2 *` |
|        - |  7151 | ` *` |
|        - |  7152 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7153 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7154 | ` * instruction.` |
|        - |  7155 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7156 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7157 | ` *` |
|        - |  7158 | ` */` |
|   124301 |  7159 | `case PH7_OP_TNE: {` |
|   248604 |  7160 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7161 | `	/* Perform the comparison and act accordingly */` |
|        - |  7162 | `#ifdef UNTRUST` |
|        - |  7163 | `	if( pNos < pStack ){` |
|        - |  7164 | `		goto Abort;` |
|        - |  7165 | `	}` |
|        - |  7166 | `#endif` |
|   248604 |  7167 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   248604 |  7168 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7169 | `		rc = 1;` |
|        2 |  7170 | `	}else{` |
|   248602 |  7171 | `		rc = rc != 0;` |
|        - |  7172 | `	}` |
|   248604 |  7173 | `	VmPopOperand(&pTos,1);` |
|   248604 |  7174 | `	if( !pInstr->iP2 ){` |
|        - |  7175 | `		/* Push comparison result without taking the jump */` |
|   248604 |  7176 | `		PH7_MemObjRelease(pTos);` |
|   248604 |  7177 | `		pTos->x.iVal = rc;` |
|        - |  7178 | `		/* Invalidate any prior representation */` |
|   248604 |  7179 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124303 |  7180 | `	}else{` |
|      ! 0 |  7181 | `		if( rc ){` |
|        - |  7182 | `			/* Jump to the desired location */` |
|      ! 0 |  7183 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7184 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7185 | `		}` |
|        - |  7186 | `	}` |
|   248604 |  7187 | `	break;` |
|        - |  7188 | `				 }` |
|        - |  7189 | `/* OP_LT P1 P2 P3` |
|        - |  7190 | ` *` |
|        - |  7191 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7192 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7193 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7194 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7195 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7196 | ` *` |
|        - |  7197 | ` */` |
|        - |  7198 | `/* OP_LE P1 P2 P3` |
|        - |  7199 | ` *` |
|        - |  7200 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7201 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7202 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7203 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7204 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7205 | ` *` |
|        - |  7206 | ` */` |
|   112423 |  7207 | `case PH7_OP_LT:` |
|        - |  7208 | `case PH7_OP_LE: {` |
|   224892 |  7209 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7210 | `	/* Perform the comparison and act accordingly */` |
|        - |  7211 | `#ifdef UNTRUST` |
|        - |  7212 | `	if( pNos < pStack ){` |
|        - |  7213 | `		goto Abort;` |
|        - |  7214 | `	}` |
|        - |  7215 | `#endif` |
|   224892 |  7216 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224892 |  7217 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7218 | `		rc = 0;` |
|   224888 |  7219 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  7220 | `		rc = rc < 1;` |
|      805 |  7221 | `	}else{` |
|   223278 |  7222 | `		rc = rc < 0;` |
|        - |  7223 | `	}` |
|   224892 |  7224 | `	VmPopOperand(&pTos,1);` |
|   224892 |  7225 | `	if( !pInstr->iP2 ){` |
|        - |  7226 | `		/* Push comparison result without taking the jump */` |
|   224892 |  7227 | `		PH7_MemObjRelease(pTos);` |
|   224892 |  7228 | `		pTos->x.iVal = rc;` |
|        - |  7229 | `		/* Invalidate any prior representation */` |
|   224892 |  7230 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112469 |  7231 | `	}else{` |
|      ! 0 |  7232 | `		if( rc ){` |
|        - |  7233 | `			/* Jump to the desired location */` |
|      ! 0 |  7234 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7235 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7236 | `		}` |
|        - |  7237 | `	}` |
|   224892 |  7238 | `	break;` |
|        - |  7239 | `				}` |
|        - |  7240 | `/* OP_GT P1 P2 P3` |
|        - |  7241 | ` *` |
|        - |  7242 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7243 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7244 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7245 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7246 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7247 | ` *` |
|        - |  7248 | ` */` |
|        - |  7249 | `/* OP_GE P1 P2 P3` |
|        - |  7250 | ` *` |
|        - |  7251 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7252 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7253 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7254 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7255 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7256 | ` *` |
|        - |  7257 | ` */` |
|    55654 |  7258 | `case PH7_OP_GT:` |
|        - |  7259 | `case PH7_OP_GE: {` |
|   111310 |  7260 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7261 | `	/* Perform the comparison and act accordingly */` |
|        - |  7262 | `#ifdef UNTRUST` |
|        - |  7263 | `	if( pNos < pStack ){` |
|        - |  7264 | `		goto Abort;` |
|        - |  7265 | `	}` |
|        - |  7266 | `#endif` |
|   111310 |  7267 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111310 |  7268 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7269 | `		rc = 0;` |
|   111306 |  7270 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110878 |  7271 | `		rc = rc >= 0;` |
|    55440 |  7272 | `	}else{` |
|      426 |  7273 | `		rc = rc > 0;` |
|        - |  7274 | `	}` |
|   111310 |  7275 | `	VmPopOperand(&pTos,1);` |
|   111310 |  7276 | `	if( !pInstr->iP2 ){` |
|        - |  7277 | `		/* Push comparison result without taking the jump */` |
|   111310 |  7278 | `		PH7_MemObjRelease(pTos);` |
|   111310 |  7279 | `		pTos->x.iVal = rc;` |
|        - |  7280 | `		/* Invalidate any prior representation */` |
|   111310 |  7281 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55656 |  7282 | `	}else{` |
|      ! 0 |  7283 | `		if( rc ){` |
|        - |  7284 | `			/* Jump to the desired location */` |
|      ! 0 |  7285 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7286 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7287 | `		}` |
|        - |  7288 | `	}` |
|   111310 |  7289 | `	break;` |
|        - |  7290 | `				}` |
|        - |  7291 | `/* OP_SPACESHIP * * *` |
|        - |  7292 | ` *` |
|        - |  7293 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7294 | ` *   -1 if left < right` |
|        - |  7295 | ` *    0 if left == right` |
|        - |  7296 | ` *    1 if left > right` |
|        - |  7297 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7298 | ` */` |
|       25 |  7299 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7300 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7301 | `#ifdef UNTRUST` |
|        - |  7302 | `	if( pNos < pStack ){` |
|        - |  7303 | `		goto Abort;` |
|        - |  7304 | `	}` |
|        - |  7305 | `#endif` |
|       51 |  7306 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7307 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7308 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7309 | `		rc = 1;` |
|        4 |  7310 | `	}else{` |
|        - |  7311 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7312 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7313 | `	}` |
|       51 |  7314 | `	VmPopOperand(&pTos,1);` |
|       51 |  7315 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7316 | `	pTos->x.iVal = rc;` |
|       51 |  7317 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7318 | `	break;` |
|        - |  7319 | `				}` |
|        - |  7320 | `/* OP_SEQ P1 P2 *` |
|        - |  7321 | ` * Strict string comparison.` |
|        - |  7322 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7323 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7324 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7325 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7326 | ` * use PH7_OP_EQ.` |
|        - |  7327 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7328 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7329 | ` */` |
|        - |  7330 | `/* OP_SNE P1 P2 *` |
|        - |  7331 | ` * Strict string comparison.` |
|        - |  7332 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7333 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7334 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7335 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7336 | ` * use PH7_OP_EQ.` |
|        - |  7337 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7338 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7339 | ` */` |
|       18 |  7340 | `case PH7_OP_SEQ:` |
|        - |  7341 | `case PH7_OP_SNE: {` |
|       38 |  7342 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7343 | `	SyString s1,s2;` |
|        - |  7344 | `	/* Perform the comparison and act accordingly */` |
|        - |  7345 | `#ifdef UNTRUST` |
|        - |  7346 | `	if( pNos < pStack ){` |
|        - |  7347 | `		goto Abort;` |
|        - |  7348 | `	}` |
|        - |  7349 | `#endif` |
|        - |  7350 | `	/* Force a string cast */` |
|       38 |  7351 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7352 | `		PH7_MemObjToString(pTos);` |
|        2 |  7353 | `	}` |
|       38 |  7354 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7355 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7356 | `	}` |
|       38 |  7357 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7358 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7359 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7360 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7361 | `		rc = rc != 0;` |
|      ! 0 |  7362 | `	}else{` |
|       38 |  7363 | `		rc = rc == 0;` |
|        - |  7364 | `	}` |
|       38 |  7365 | `	VmPopOperand(&pTos,1);` |
|       38 |  7366 | `	if( !pInstr->iP2 ){` |
|        - |  7367 | `		/* Push comparison result without taking the jump */` |
|       38 |  7368 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7369 | `		pTos->x.iVal = rc;` |
|        - |  7370 | `		/* Invalidate any prior representation */` |
|       38 |  7371 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7372 | `	}else{` |
|      ! 0 |  7373 | `		if( rc ){` |
|        - |  7374 | `			/* Jump to the desired location */` |
|      ! 0 |  7375 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7376 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7377 | `		}` |
|        - |  7378 | `	}` |
|       38 |  7379 | `	break;` |
|        - |  7380 | `				 }` |
|        - |  7381 | `/*` |
|        - |  7382 | ` * OP_LOAD_REF * * *` |
|        - |  7383 | ` * Push the index of a referenced object on the stack.` |
|        - |  7384 | ` */` |
|       60 |  7385 | `case PH7_OP_LOAD_REF: {` |
|        - |  7386 | `	sxu32 nIdx;` |
|        - |  7387 | `#ifdef UNTRUST` |
|        - |  7388 | `	if( pTos < pStack ){` |
|        - |  7389 | `		goto Abort;` |
|        - |  7390 | `	}` |
|        - |  7391 | `#endif` |
|        - |  7392 | `	/* Extract memory object index */` |
|      121 |  7393 | `	nIdx = pTos->nIdx;` |
|      121 |  7394 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7395 | `		/* Nullify the object */` |
|      101 |  7396 | `		PH7_MemObjRelease(pTos);` |
|        - |  7397 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7398 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7399 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7400 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7401 | `	}` |
|      121 |  7402 | `	break;` |
|        - |  7403 | `					  }` |
|        - |  7404 | `/*` |
|        - |  7405 | ` * OP_STORE_REF * * P3` |
|        - |  7406 | ` * Perform an assignment operation by reference.` |
|        - |  7407 | ` */` |
|       16 |  7408 | ` case PH7_OP_STORE_REF: {` |
|       34 |  7409 | `	 SyString sName = { 0 , 0 };` |
|        - |  7410 | `	 VmFrame *pFrameLocal;` |
|        - |  7411 | `	SyHashEntry *pEntry;` |
|        - |  7412 | `	sxu32 nIdx;` |
|        - |  7413 | `#ifdef UNTRUST` |
|        - |  7414 | `	if( pTos < pStack ){` |
|        - |  7415 | `		goto Abort;` |
|        - |  7416 | `	}` |
|        - |  7417 | `#endif` |
|       34 |  7418 | `	if( pInstr->p3 == 0 ){` |
|        - |  7419 | `		char *zName;` |
|        - |  7420 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7421 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7422 | `			/* Force a string cast */` |
|      ! 0 |  7423 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7424 | `		}` |
|      ! 0 |  7425 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7426 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7427 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7428 | `			if( zName ){` |
|      ! 0 |  7429 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7430 | `			}` |
|      ! 0 |  7431 | `		}` |
|      ! 0 |  7432 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7433 | `		pTos--;` |
|      ! 0 |  7434 | `	}else{` |
|       34 |  7435 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7436 | `	}` |
|       34 |  7437 | `	nIdx = pTos->nIdx;` |
|       34 |  7438 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7439 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7440 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7441 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7442 | `		}else{` |
|        - |  7443 | `			ph7_value *pObj;` |
|        - |  7444 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7445 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7446 | `			if( pObj == 0 ){` |
|      ! 0 |  7447 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7448 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7449 | `				goto Abort;` |
|        - |  7450 | `			}` |
|        - |  7451 | `			/* Perform the store operation */` |
|      ! 0 |  7452 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7453 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7454 | `		}` |
|       34 |  7455 | `	}else if( sName.nByte > 0){` |
|       34 |  7456 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7457 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7458 | `		}else{` |
|       34 |  7459 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7460 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7461 | `			/* Query the local frame */` |
|       34 |  7462 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7463 | `			if( pEntry ){` |
|      ! 0 |  7464 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7465 | `			}else{` |
|       34 |  7466 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7467 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7468 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7469 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7470 | `				}` |
|       34 |  7471 | `				if( rc == SXRET_OK ){` |
|       34 |  7472 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7473 | `				}` |
|        - |  7474 | `			}` |
|        - |  7475 | `		}` |
|       16 |  7476 | `	}` |
|       34 |  7477 | `	break;` |
|        - |  7478 | `				 }` |
|        - |  7479 | `/*` |
|        - |  7480 | ` * OP_UPLINK P1 * *` |
|        - |  7481 | ` * Link a variable to the top active VM frame.` |
|        - |  7482 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7483 | ` */` |
|       30 |  7484 | `case PH7_OP_UPLINK: {` |
|       62 |  7485 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7486 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7487 | `		SyString sName;` |
|        - |  7488 | `		/* Perform the link */` |
|      132 |  7489 | `		while( pLink <= pTos ){` |
|       72 |  7490 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7491 | `				/* Force a string cast */` |
|      ! 0 |  7492 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7493 | `			}` |
|       72 |  7494 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7495 | `			if( sName.nByte > 0 ){` |
|       72 |  7496 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7497 | `			}` |
|       72 |  7498 | `			pLink++;` |
|        2 |  7499 | `		}` |
|       30 |  7500 | `	}` |
|       62 |  7501 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7502 | `	break;` |
|        - |  7503 | `					}` |
|        - |  7504 | `/*` |
|        - |  7505 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7506 | ` * Push an exception in the corresponding container so that` |
|        - |  7507 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7508 | ` */` |
|      183 |  7509 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      368 |  7510 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7511 | `	VmFrame *pFrameLocal;` |
|        - |  7512 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      368 |  7513 | `	pException->iFinallyDone = 0;` |
|      368 |  7514 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7515 | `	/* Create the exception frame */` |
|      368 |  7516 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      368 |  7517 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7518 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7519 | `		goto Abort;` |
|        - |  7520 | `	}` |
|        - |  7521 | `	/* Mark the special frame */` |
|      368 |  7522 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      368 |  7523 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7524 | `	/* Point to the frame that trigger the exception */` |
|      368 |  7525 | `	pFrameLocal = pFrameLocal->pParent;` |
|      368 |  7526 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      368 |  7527 | `	pException->pFrame = pFrameLocal;` |
|      368 |  7528 | `	break;` |
|        - |  7529 | `							}` |
|        - |  7530 | `/*` |
|        - |  7531 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7532 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7533 | ` */` |
|      182 |  7534 | `case PH7_OP_POP_EXCEPTION: {` |
|      366 |  7535 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      366 |  7536 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7537 | `		ph7_exception **apException;` |
|        - |  7538 | `		/* Pop the loaded exception */` |
|       32 |  7539 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7540 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7541 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7542 | `		}` |
|       15 |  7543 | `	}` |
|      366 |  7544 | `	pException->pFrame = 0;` |
|        - |  7545 | `	/* Leave the exception frame */` |
|      366 |  7546 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7547 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      366 |  7548 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7549 | `		sxi32 rcFinally;` |
|       20 |  7550 | `		pException->iFinallyDone = 1;` |
|       20 |  7551 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7552 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7553 | `			goto Abort;` |
|        - |  7554 | `		}` |
|        9 |  7555 | `	}` |
|      366 |  7556 | `	break;` |
|        - |  7557 | `							}` |
|        - |  7558 |  |
|        - |  7559 | `/*` |
|        - |  7560 | ` * OP_THROW * P2 *` |
|        - |  7561 | ` * Throw an user exception.` |
|        - |  7562 | ` */` |
|       78 |  7563 | `case PH7_OP_THROW: {` |
|      158 |  7564 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      158 |  7565 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7566 | `#ifdef UNTRUST` |
|        - |  7567 | `	if( pTos < pStack ){` |
|        - |  7568 | `		goto Abort;` |
|        - |  7569 | `	}` |
|        - |  7570 | `#endif` |
|      158 |  7571 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7572 | `	/* Tell the upper layer that an exception was thrown */` |
|      158 |  7573 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      158 |  7574 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      158 |  7575 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7576 | `		ph7_class *pThrowable;` |
|        - |  7577 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      158 |  7578 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      159 |  7579 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7580 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7581 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7582 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7583 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7584 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7585 | `			if( pErrorClass ){` |
|        3 |  7586 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7587 | `			}` |
|        3 |  7588 | `			if( pErrInst ){` |
|        - |  7589 | `				ph7_class_method *pCons;` |
|        3 |  7590 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7591 | `				if( pCons ){` |
|        - |  7592 | `					ph7_value sArg;` |
|        - |  7593 | `					ph7_value *apArg[1];` |
|        - |  7594 | `					SyString sMsgStr;` |
|        - |  7595 | `					static const char zErrMsg[] =` |
|        - |  7596 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7597 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7598 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7599 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7600 | `					apArg[0] = &sArg;` |
|        3 |  7601 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7602 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7603 | `				}` |
|        3 |  7604 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7605 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7606 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7607 | `					goto Abort;` |
|        - |  7608 | `				}` |
|        2 |  7609 | `			}else{` |
|        - |  7610 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7611 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7612 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7613 | `					goto Abort;` |
|        - |  7614 | `				}` |
|        - |  7615 | `			}` |
|        2 |  7616 | `		}else{` |
|        - |  7617 | `			/* Throw the exception */` |
|      156 |  7618 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      156 |  7619 | `			if( rc == SXERR_ABORT ){` |
|        - |  7620 | `				/* Abort processing immediately */` |
|       11 |  7621 | `				goto Abort;` |
|        - |  7622 | `			}` |
|        - |  7623 | `		}` |
|       75 |  7624 | `	}else{` |
|        - |  7625 | `		/* Expecting a class instance */` |
|      ! 0 |  7626 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7627 | `		if( rc == SXERR_ABORT ){` |
|        - |  7628 | `			/* Abort processing immediately */` |
|      ! 0 |  7629 | `			goto Abort;` |
|        - |  7630 | `		}` |
|        - |  7631 | `	}` |
|        - |  7632 | `	/* Pop the top entry */` |
|      148 |  7633 | `	VmPopOperand(&pTos,1);` |
|        - |  7634 | `	/* Perform an unconditional jump */` |
|      148 |  7635 | `	pc = nJump - 1;` |
|      148 |  7636 | `	break;` |
|        - |  7637 | `				   }` |
|        - |  7638 | `/*` |
|        - |  7639 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7640 | ` * Prepare a foreach step.` |
|        - |  7641 | ` */` |
|     6179 |  7642 | `case PH7_OP_FOREACH_INIT: {` |
|    12360 |  7643 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7644 | `	void *pName;` |
|        - |  7645 | `#ifdef UNTRUST` |
|        - |  7646 | `	if( pTos < pStack ){` |
|        - |  7647 | `		goto Abort;` |
|        - |  7648 | `	}` |
|        - |  7649 | `#endif` |
|    12360 |  7650 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7651 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7652 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7653 | `			/* Force a string cast */` |
|      ! 0 |  7654 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7655 | `		}` |
|        - |  7656 | `		/* Duplicate name */` |
|      ! 0 |  7657 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7658 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7659 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7660 | `		}` |
|      ! 0 |  7661 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7662 | `	}` |
|    12360 |  7663 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7664 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7665 | `			/* Force a string cast */` |
|      ! 0 |  7666 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7667 | `		}` |
|        - |  7668 | `		/* Duplicate name */` |
|      ! 0 |  7669 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7670 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7671 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7672 | `		}` |
|      ! 0 |  7673 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7674 | `	}` |
|        - |  7675 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12360 |  7676 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7677 | `		/* Jump out of the loop */` |
|      ! 0 |  7678 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7679 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7680 | `		}` |
|      ! 0 |  7681 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7682 | `	}else{` |
|        - |  7683 | `		ph7_foreach_step *pStep;` |
|    12360 |  7684 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12360 |  7685 | `		if( pStep == 0 ){` |
|      ! 0 |  7686 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7687 | `			/* Jump out of the loop */` |
|      ! 0 |  7688 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7689 | `		}else{` |
|        - |  7690 | `			/* Zero the structure */` |
|    12360 |  7691 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7692 | `			/* Prepare the step */` |
|    12360 |  7693 | `			pStep->iFlags = pInfo->iFlags;` |
|    12360 |  7694 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7695 | `				ph7_hashmap *pMap;` |
|        - |  7696 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7697 | `				 * source array so mutations don't affect other sharers. */` |
|    12326 |  7698 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7699 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7700 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7701 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7702 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7703 | `						 * variable still points at the same hashmap as` |
|        - |  7704 | `						 * the stack value. */` |
|        9 |  7705 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7706 | `							pCur->iRef--;` |
|        9 |  7707 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7708 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7709 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7710 | `						}` |
|        4 |  7711 | `					}` |
|        4 |  7712 | `				}` |
|    12326 |  7713 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7714 | `				/* Reset the internal loop cursor */` |
|    12326 |  7715 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7716 | `				/* Mark the step */` |
|    12326 |  7717 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12326 |  7718 | `				pStep->xIter.pMap = pMap;` |
|    12326 |  7719 | `				pMap->iRef++;` |
|     6164 |  7720 | `			}else{` |
|       36 |  7721 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7722 | `				ph7_class *pIteratorClass;` |
|        - |  7723 | `				/* Check if the object implements Iterator */` |
|       36 |  7724 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7725 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7726 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7727 | `					ph7_class_method *pRewind;` |
|       24 |  7728 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7729 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7730 | `					pThis->iRef++;` |
|       24 |  7731 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7732 | `					if( pRewind ){` |
|       24 |  7733 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7734 | `					}` |
|       13 |  7735 | `				}else{` |
|        - |  7736 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7737 | `					ph7_class *pIterAggClass;` |
|       14 |  7738 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7739 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7740 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7741 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7742 | `						ph7_class_method *pGetIter;` |
|        3 |  7743 | `						int iterAggOk = 0;` |
|        3 |  7744 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7745 | `						if( pGetIter ){` |
|        - |  7746 | `							ph7_value sResult;` |
|        3 |  7747 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7748 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7749 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7750 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7751 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7752 | `									ph7_class_method *pRewind;` |
|        3 |  7753 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7754 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7755 | `									pIterObj->iRef++;` |
|        - |  7756 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7757 | `									pStep->pOwner = pThis;` |
|        3 |  7758 | `									pThis->iRef++;` |
|        3 |  7759 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7760 | `									if( pRewind ){` |
|        3 |  7761 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7762 | `									}` |
|        3 |  7763 | `									iterAggOk = 1;` |
|        1 |  7764 | `								}` |
|        1 |  7765 | `							}` |
|        3 |  7766 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7767 | `						}` |
|        3 |  7768 | `						if( !iterAggOk ){` |
|        - |  7769 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7770 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7771 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7772 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7773 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7774 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7775 | `						}` |
|        2 |  7776 | `					}else{` |
|        - |  7777 | `						/* Plain object iteration via hAttr */` |
|       12 |  7778 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7779 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7780 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7781 | `						pThis->iRef++;` |
|        - |  7782 | `					}` |
|        - |  7783 | `				}` |
|        - |  7784 | `			}` |
|        - |  7785 | `		}` |
|    12360 |  7786 | `		if( pStep ){` |
|    12360 |  7787 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7788 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7789 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7790 | `				/* Jump out of the loop */` |
|      ! 0 |  7791 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7792 | `			}` |
|     6179 |  7793 | `		}` |
|        - |  7794 | `	}` |
|    12360 |  7795 | `	VmPopOperand(&pTos,1);` |
|    12360 |  7796 | `	break;` |
|        - |  7797 | `						  }` |
|        - |  7798 | `/*` |
|        - |  7799 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7800 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7801 | ` */` |
|   101477 |  7802 | `case PH7_OP_FOREACH_STEP: {` |
|   202956 |  7803 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7804 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7805 | `	ph7_value *pValue;` |
|        - |  7806 | `	VmFrame *pFrameLocal;` |
|        - |  7807 | `	/* Peek the last step */` |
|   202956 |  7808 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   202956 |  7809 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   202956 |  7810 | `	pFrameLocal = pVm->pFrame;` |
|   202956 |  7811 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   202956 |  7812 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   202822 |  7813 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7814 | `		ph7_hashmap_node *pNode;` |
|        - |  7815 | `		/* Extract the current node value */` |
|   202822 |  7816 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   202822 |  7817 | `		if( pNode == 0 ){` |
|        - |  7818 | `			/* No more entry to process */` |
|    12324 |  7819 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12324 |  7820 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7821 | `				/* Break the reference with the last element */` |
|        7 |  7822 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7823 | `			}` |
|        - |  7824 | `			/* Automatically reset the loop cursor */` |
|    12324 |  7825 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7826 | `			/* Cleanup the mess left behind */` |
|    12324 |  7827 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12324 |  7828 | `			SySetPop(&pInfo->aStep);` |
|    12324 |  7829 | `			PH7_HashmapUnref(pMap);` |
|     6163 |  7830 | `		}else{` |
|   190500 |  7831 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7832 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7833 | `				if( pKey ){` |
|      528 |  7834 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7835 | `				}` |
|      263 |  7836 | `			}` |
|   190500 |  7837 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7838 | `				SyHashEntry *pEntry;` |
|        - |  7839 | `				/* Pass by reference */` |
|       23 |  7840 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7841 | `				if( pEntry ){` |
|       21 |  7842 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7843 | `				}else{` |
|        4 |  7844 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7845 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7846 | `				}` |
|       12 |  7847 | `			}else{` |
|        - |  7848 | `				/* Make a copy of the entry value */` |
|   190478 |  7849 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   190478 |  7850 | `				if( pValue ){` |
|   190478 |  7851 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95238 |  7852 | `				}` |
|        - |  7853 | `			}` |
|        2 |  7854 | `		}` |
|   101546 |  7855 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7856 | `		/* Iterator-based iteration.` |
|        - |  7857 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7858 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7859 | `		 */` |
|      106 |  7860 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7861 | `		ph7_class_method *pMethod;` |
|        - |  7862 | `		ph7_value sResult;` |
|      106 |  7863 | `		int isValid = 0;` |
|        - |  7864 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7865 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7866 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7867 | `		}else{` |
|       82 |  7868 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7869 | `			if( pMethod ){` |
|       82 |  7870 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7871 | `			}` |
|        - |  7872 | `		}` |
|        - |  7873 | `		/* Call valid() */` |
|      106 |  7874 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7875 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7876 | `		if( pMethod ){` |
|      106 |  7877 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7878 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7879 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7880 | `		}` |
|      106 |  7881 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7882 | `		if( !isValid ){` |
|        - |  7883 | `			/* Iterator exhausted */` |
|       24 |  7884 | `			pc = pInstr->iP2 - 1;` |
|        - |  7885 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7886 | `			if( pStep->pOwner ){` |
|        3 |  7887 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7888 | `			}` |
|       24 |  7889 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7890 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7891 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7892 | `		}else{` |
|        - |  7893 | `			/* Call current() to get value */` |
|       84 |  7894 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7895 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7896 | `			if( pMethod ){` |
|       84 |  7897 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7898 | `			}` |
|       84 |  7899 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7900 | `			if( pValue ){` |
|       84 |  7901 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7902 | `			}` |
|       84 |  7903 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7904 | `			/* Call key() if needed */` |
|       84 |  7905 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7906 | `				ph7_value sKey;` |
|       35 |  7907 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7908 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7909 | `				if( pMethod ){` |
|       35 |  7910 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7911 | `				}` |
|       35 |  7912 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7913 | `				if( pValue ){` |
|       35 |  7914 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7915 | `				}` |
|       35 |  7916 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7917 | `			}` |
|        - |  7918 | `		}` |
|       54 |  7919 | `	}else{` |
|       32 |  7920 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7921 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7922 | `		SyHashEntry *pEntry;` |
|        - |  7923 | `		/* Point to the next attribute */` |
|       36 |  7924 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7925 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7926 | `			/* Check access permission */` |
|       38 |  7927 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7928 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7929 | `					break; /* Access is granted */` |
|        - |  7930 | `			}` |
|        1 |  7931 | `		}` |
|       32 |  7932 | `		if( pEntry == 0 ){` |
|        - |  7933 | `			/* Clean up the mess left behind */` |
|       12 |  7934 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7935 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7936 | `				/* Break the reference with the last element */` |
|        3 |  7937 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7938 | `			}` |
|       12 |  7939 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7940 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7941 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7942 | `		}else{` |
|       22 |  7943 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7944 | `			ph7_value *pAttrValue;` |
|       22 |  7945 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7946 | `				/* Fill with the current attribute name */` |
|       22 |  7947 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7948 | `				if( pKey ){` |
|       22 |  7949 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7950 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7951 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7952 | `				}` |
|       10 |  7953 | `			}` |
|        - |  7954 | `			/* Extract attribute value */` |
|       22 |  7955 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7956 | `			if( pAttrValue ){` |
|       22 |  7957 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7958 | `					/* Pass by reference */` |
|        3 |  7959 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7960 | `					if( pEntry ){` |
|        3 |  7961 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7962 | `					}else{` |
|      ! 0 |  7963 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7964 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7965 | `					}` |
|        2 |  7966 | `				}else{` |
|        - |  7967 | `					/* Make a copy of the attribute value */` |
|       20 |  7968 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7969 | `					if( pValue ){` |
|       20 |  7970 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7971 | `					}` |
|        - |  7972 | `				}` |
|       10 |  7973 | `			}` |
|        - |  7974 | `		}` |
|        - |  7975 | `	}` |
|   202956 |  7976 | `	break;` |
|        - |  7977 | `						  }` |
|        - |  7978 | `/*` |
|        - |  7979 | ` * OP_MEMBER P1 P2` |
|        - |  7980 | ` * Load class attribute/method on the stack.` |
|        - |  7981 | ` */` |
|     4041 |  7982 | `case PH7_OP_MEMBER: {` |
|        - |  7983 | `	ph7_class_instance *pThis;` |
|        - |  7984 | `	ph7_value *pNos;` |
|        - |  7985 | `	SyString sName;` |
|     8084 |  7986 | `	if( !pInstr->iP1 ){` |
|     7844 |  7987 | `		pNos = &pTos[-1];` |
|        - |  7988 | `#ifdef UNTRUST` |
|        - |  7989 | `		if( pNos < pStack ){` |
|        - |  7990 | `			goto Abort;` |
|        - |  7991 | `		}` |
|        - |  7992 | `#endif` |
|     7844 |  7993 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7994 | `			ph7_class *pClass;` |
|        - |  7995 | `			/* Class already instantiated */` |
|     7842 |  7996 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7997 | `			/* Point to the instantiated class */` |
|     7842 |  7998 | `			pClass = pThis->pClass;` |
|        - |  7999 | `			/* Extract attribute name first */` |
|     7842 |  8000 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7842 |  8001 | `			if( pInstr->iP2 ){` |
|        - |  8002 | `				/* Method call */` |
|      786 |  8003 | `				ph7_class_method *pMeth = 0;` |
|      786 |  8004 | `				if( sName.nByte > 0 ){` |
|        - |  8005 | `					/* Extract the target method */` |
|      786 |  8006 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      392 |  8007 | `				}` |
|      786 |  8008 | `				if( pMeth == 0 ){` |
|      ! 0 |  8009 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8010 | `						&pClass->sName,&sName` |
|        - |  8011 | `						);` |
|        - |  8012 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8013 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8014 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8015 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8016 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8017 | `				}else{` |
|        - |  8018 | `					/* Push method name on the stack */` |
|      786 |  8019 | `					PH7_MemObjRelease(pTos);` |
|      786 |  8020 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      786 |  8021 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8022 | `				}` |
|      786 |  8023 | `				pTos->nIdx = SXU32_HIGH;` |
|      394 |  8024 | `			}else{` |
|        - |  8025 | `				/* Attribute access */` |
|     7058 |  8026 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8027 | `				SyHashEntry *pEntry;` |
|        - |  8028 | `				/* Extract the target attribute */` |
|     7058 |  8029 | `				if( sName.nByte > 0 ){` |
|     7058 |  8030 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7058 |  8031 | `					if( pEntry ){` |
|        - |  8032 | `						/* Point to the attribute value */` |
|     7056 |  8033 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3527 |  8034 | `					}` |
|     3528 |  8035 | `				}` |
|     7058 |  8036 | `				if( pObjAttr == 0 ){` |
|        - |  8037 | `					/* No such attribute,load null */` |
|        4 |  8038 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8039 | `						&pClass->sName,&sName);` |
|        - |  8040 | `					/* Call the __get magic method if available */` |
|        3 |  8041 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8042 | `				}` |
|     7058 |  8043 | `				VmPopOperand(&pTos,1);` |
|        - |  8044 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8045 | `				 * This is due to the following case:` |
|        - |  8046 | `				 *     (new TestClass())->foo;` |
|        - |  8047 | `				 */` |
|     7058 |  8048 | `				pThis->iRef++;` |
|     7058 |  8049 | `				PH7_MemObjRelease(pTos);` |
|     7058 |  8050 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7058 |  8051 | `				if( pObjAttr ){` |
|     7056 |  8052 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8053 | `					/* Check attribute access */` |
|     7056 |  8054 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8055 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8056 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8057 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8058 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8059 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7054 |  8060 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3569 |  8061 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  8062 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  8063 | `							int bIsLhs = 0;` |
|       82 |  8064 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  8065 | `								bIsLhs = 1;` |
|       39 |  8066 | `							}` |
|       82 |  8067 | `							if( !bIsLhs ){` |
|        3 |  8068 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8069 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8070 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8071 | `									goto Abort;` |
|        - |  8072 | `								}` |
|        - |  8073 | `								{` |
|        3 |  8074 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8075 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8076 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4041 |  8077 | `										break;` |
|        - |  8078 | `									}` |
|        - |  8079 | `								}` |
|      ! 0 |  8080 | `								goto Exception;` |
|        - |  8081 | `							}` |
|       39 |  8082 | `						}` |
|        - |  8083 | `						/* Load attribute */` |
|     7054 |  8084 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7054 |  8085 | `						if( pValue ){` |
|     7054 |  8086 | `							if( pThis->iRef < 2 ){` |
|        - |  8087 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8088 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8089 | `								 */` |
|        7 |  8090 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8091 | `							}else{` |
|        - |  8092 | `								/* Simple load */` |
|     7048 |  8093 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8094 | `							}` |
|     7054 |  8095 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7052 |  8096 | `								if( pThis->iRef > 1 ){` |
|        - |  8097 | `									/* Load attribute index */` |
|     7046 |  8098 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3522 |  8099 | `								}` |
|     3525 |  8100 | `							}` |
|     3526 |  8101 | `						}` |
|     3528 |  8102 | `					}else{` |
|        - |  8103 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8104 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8105 | `						char zMsg[256];` |
|      ! 0 |  8106 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8107 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8108 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8109 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8110 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8111 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8112 | `						goto Abort;` |
|        - |  8113 | `					}` |
|     3526 |  8114 | `				}` |
|        - |  8115 | `				/* Safely unreference the object */` |
|     7056 |  8116 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8117 | `			}` |
|     3921 |  8118 | `		}else{` |
|        3 |  8119 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8120 | `			VmPopOperand(&pTos,1);` |
|        3 |  8121 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8122 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8123 | `		}` |
|     3922 |  8124 | `	}else{` |
|        - |  8125 | `		/* Static member access using class name */` |
|      242 |  8126 | `		pNos = pTos;` |
|      242 |  8127 | `		pThis = 0;` |
|      242 |  8128 | `		if( !pInstr->p3 ){` |
|      192 |  8129 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  8130 | `			pNos--;` |
|        - |  8131 | `#ifdef UNTRUST` |
|        - |  8132 | `			if( pNos < pStack ){` |
|        - |  8133 | `				goto Abort;` |
|        - |  8134 | `			}` |
|        - |  8135 | `#endif` |
|       97 |  8136 | `		}else{` |
|        - |  8137 | `			/* Attribute name already computed */` |
|       52 |  8138 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8139 | `		}` |
|      242 |  8140 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      242 |  8141 | `			ph7_class *pClass = 0;` |
|      242 |  8142 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8143 | `				/* Class already instantiated */` |
|        5 |  8144 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8145 | `				pClass = pThis->pClass;` |
|        5 |  8146 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8147 | `			}else{` |
|        - |  8148 | `				/* Try to extract the target class */` |
|      238 |  8149 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      238 |  8150 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      238 |  8151 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8152 | `					/* Handle self/static/parent keywords */` |
|      238 |  8153 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  8154 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  8155 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8156 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  8157 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  8158 | `						}` |
|      208 |  8159 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  8160 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      178 |  8161 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  8162 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  8163 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  8164 | `							pClass = pSelf->pBase;` |
|       13 |  8165 | `						}` |
|       15 |  8166 | `					}else{` |
|      126 |  8167 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8168 | `					}` |
|      118 |  8169 | `				}` |
|        - |  8170 | `			}` |
|      242 |  8171 | `			if( pClass == 0 ){` |
|        - |  8172 | `				/* Undefined class */` |
|      ! 0 |  8173 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8174 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8175 | `					);` |
|      ! 0 |  8176 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8177 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8178 | `				}` |
|      ! 0 |  8179 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8180 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8181 | `			}else{` |
|      242 |  8182 | `				if( pInstr->iP2 ){` |
|        - |  8183 | `					/* Method call */` |
|       86 |  8184 | `					ph7_class_method *pMeth = 0;` |
|       86 |  8185 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8186 | `						/* Extract the target method */` |
|       86 |  8187 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8188 | `					}` |
|       86 |  8189 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8190 | `						if( pMeth ){` |
|      ! 0 |  8191 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8192 | `								&pClass->sName,&sName` |
|        - |  8193 | `								);` |
|      ! 0 |  8194 | `						}else{` |
|      ! 0 |  8195 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8196 | `								&pClass->sName,&sName` |
|        - |  8197 | `								);` |
|        - |  8198 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8199 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8200 | `						}` |
|        - |  8201 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8202 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8203 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8204 | `						}` |
|      ! 0 |  8205 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8206 | `					}else{` |
|        - |  8207 | `						/* Push method name on the stack */` |
|       86 |  8208 | `						PH7_MemObjRelease(pTos);` |
|       86 |  8209 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  8210 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8211 | `					}` |
|       86 |  8212 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  8213 | `				}else{` |
|        - |  8214 | `					/* Attribute access */` |
|      158 |  8215 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8216 | `					/* Check for special ::class pseudo-constant */` |
|      204 |  8217 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8218 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8219 | `						/* ::class returns the fully qualified class name */` |
|        - |  8220 | `						/* Pop the attribute name from the stack */` |
|       60 |  8221 | `						if( !pInstr->p3 ){` |
|       60 |  8222 | `							VmPopOperand(&pTos,1);` |
|       29 |  8223 | `						}` |
|       60 |  8224 | `						PH7_MemObjRelease(pTos);` |
|        - |  8225 | `						/* Load the class name */` |
|       60 |  8226 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  8227 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  8228 | `					}else{` |
|        - |  8229 | `						/* Extract the target attribute */` |
|      100 |  8230 | `						if( sName.nByte > 0 ){` |
|      100 |  8231 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8232 | `						}` |
|      100 |  8233 | `						if( pAttr == 0 ){` |
|        - |  8234 | `							/* No such attribute,load null */` |
|      ! 0 |  8235 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8236 | `								&pClass->sName,&sName);` |
|        - |  8237 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8238 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8239 | `						}` |
|        - |  8240 | `						/* Pop the attribute name from the stack */` |
|      100 |  8241 | `						if( !pInstr->p3 ){` |
|       50 |  8242 | `							VmPopOperand(&pTos,1);` |
|       24 |  8243 | `						}` |
|      100 |  8244 | `						PH7_MemObjRelease(pTos);` |
|      100 |  8245 | `						pTos->nIdx = SXU32_HIGH;` |
|      100 |  8246 | `						if( pAttr ){` |
|      100 |  8247 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8248 | `								/* Access to a non static attribute */` |
|      ! 0 |  8249 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8250 | `									&pClass->sName,&pAttr->sName` |
|        - |  8251 | `									);` |
|      ! 0 |  8252 | `							}else{` |
|        - |  8253 | `								ph7_value *pValue;` |
|        - |  8254 | `								/* Check if the access to the attribute is allowed */` |
|      100 |  8255 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8256 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8257 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8258 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       68 |  8259 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       59 |  8260 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8261 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       40 |  8262 | `										if( pS ){` |
|       40 |  8263 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       40 |  8264 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8265 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8266 | `												int bIsLhs = 0;` |
|        8 |  8267 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8268 | `													bIsLhs = 1;` |
|        2 |  8269 | `												}` |
|        8 |  8270 | `												if( !bIsLhs ){` |
|        3 |  8271 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8272 | `													if( pThis ){` |
|      ! 0 |  8273 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8274 | `													}` |
|        3 |  8275 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8276 | `														goto Abort;` |
|        - |  8277 | `													}` |
|        - |  8278 | `													{` |
|        3 |  8279 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8280 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8281 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8282 | `															break;` |
|        - |  8283 | `														}` |
|        - |  8284 | `													}` |
|      ! 0 |  8285 | `													goto Exception;` |
|        - |  8286 | `												}` |
|        2 |  8287 | `											}` |
|       18 |  8288 | `										}` |
|       18 |  8289 | `									}` |
|        - |  8290 | `									/* Load the desired attribute */` |
|       94 |  8291 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       94 |  8292 | `									if( pValue ){` |
|       94 |  8293 | `										PH7_MemObjLoad(pValue,pTos);` |
|       94 |  8294 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8295 | `											/* Load index number */` |
|       50 |  8296 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8297 | `										}` |
|       46 |  8298 | `									}` |
|       48 |  8299 | `								}else{` |
|        - |  8300 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8301 | `									char zMsg[256];` |
|        5 |  8302 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8303 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8304 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8305 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8306 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8307 | `									}else{` |
|      ! 0 |  8308 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8309 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8310 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8311 | `									}` |
|        5 |  8312 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8313 | `									goto Abort;` |
|        - |  8314 | `								}` |
|        - |  8315 | `							}` |
|       46 |  8316 | `						}` |
|        - |  8317 | `					}` |
|        - |  8318 | `				}` |
|      236 |  8319 | `				if( pThis ){` |
|        - |  8320 | `					/* Safely unreference the object */` |
|        5 |  8321 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8322 | `				}` |
|        - |  8323 | `			}` |
|      119 |  8324 | `		}else{` |
|        - |  8325 | `			/* Pop operands */` |
|      ! 0 |  8326 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8327 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8328 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8329 | `			}` |
|      ! 0 |  8330 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8331 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8332 | `		}` |
|        - |  8333 | `	}` |
|     8076 |  8334 | `	break;` |
|        - |  8335 | `					}` |
|        - |  8336 | `/*` |
|        - |  8337 | ` * OP_NEW P1 * * *` |
|        - |  8338 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8339 | ` */` |
|      661 |  8340 | `case PH7_OP_NEW: {` |
|     1324 |  8341 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1324 |  8342 | `	ph7_class *pClass = 0;` |
|        - |  8343 | `	ph7_class_instance *pNew;` |
|     1324 |  8344 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8345 | `		/* Try to extract the desired class */` |
|     1985 |  8346 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1322 |  8347 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      661 |  8348 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8349 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8350 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8351 | `	}` |
|     1324 |  8352 | `	if( pClass == 0 ){` |
|        - |  8353 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8354 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8355 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8356 | `			);` |
|        - |  8357 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8358 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8359 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8360 | `			/* Pop given arguments */` |
|      ! 0 |  8361 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8362 | `		}` |
|      ! 0 |  8363 | `		goto Abort;` |
|      ! 0 |  8364 | `	}else{` |
|        - |  8365 | `		ph7_class_method *pCons;` |
|        - |  8366 | `		/* Create a new class instance */` |
|     1324 |  8367 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1324 |  8368 | `		if( pNew == 0 ){` |
|      ! 0 |  8369 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8370 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8371 | `				&pClass->sName` |
|        - |  8372 | `			);` |
|      ! 0 |  8373 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8374 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8375 | `				/* Pop given arguments */` |
|      ! 0 |  8376 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8377 | `			}` |
|      ! 0 |  8378 | `			break;` |
|        - |  8379 | `		}` |
|        - |  8380 | `		/* Check if a constructor is available */` |
|     1324 |  8381 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1324 |  8382 | `		if( pCons == 0 ){` |
|      928 |  8383 | `			SyString *pName = &pClass->sName;` |
|        - |  8384 | `			/* Check for a constructor with the same base class name */` |
|      928 |  8385 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      463 |  8386 | `		}` |
|     1324 |  8387 | `		if( pCons ){` |
|        - |  8388 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8389 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8390 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8391 | `			 * (including variadic string-key packing). */` |
|      398 |  8392 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8393 | `			sxi32 rcCons;` |
|      398 |  8394 | `			SySetReset(&aArg);` |
|      778 |  8395 | `			while( pArg < pTos ){` |
|      382 |  8396 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      382 |  8397 | `				pArg++;` |
|        2 |  8398 | `			}` |
|      398 |  8399 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8400 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8401 | `				sxu32 n;` |
|      114 |  8402 | `				n = SySetUsed(&aArg);` |
|        - |  8403 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8404 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8405 | `				 * after resolution). */` |
|      222 |  8406 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8407 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8408 | `					if( pFuncArg ){` |
|      110 |  8409 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8410 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8411 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8412 | `						}` |
|       54 |  8413 | `					}` |
|      110 |  8414 | `					n++;` |
|        2 |  8415 | `				}` |
|       56 |  8416 | `			}` |
|      398 |  8417 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8418 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      398 |  8419 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8420 | `				pNew->iRef = 1;` |
|      ! 0 |  8421 | `			}` |
|      398 |  8422 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8423 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8424 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8425 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8426 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8427 | `				sxi32 iResumePc;` |
|        5 |  8428 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8429 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8430 | `					goto Abort;` |
|        - |  8431 | `				}` |
|        5 |  8432 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8433 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8434 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8435 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8436 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8437 | `					}` |
|        5 |  8438 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8439 | `					pc = iResumePc;` |
|        5 |  8440 | `					break;` |
|        - |  8441 | `				}` |
|      ! 0 |  8442 | `				goto Exception;` |
|        - |  8443 | `			}` |
|      196 |  8444 | `		}` |
|     1320 |  8445 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8446 | `			/* Pop given arguments */` |
|      312 |  8447 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      155 |  8448 | `		}` |
|     1320 |  8449 | `		PH7_MemObjRelease(pTos);` |
|     1320 |  8450 | `		pTos->x.pOther = pNew;` |
|     1320 |  8451 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8452 | `	}` |
|     1320 |  8453 | `	break;` |
|        - |  8454 | `				 }` |
|        - |  8455 | `/*` |
|        - |  8456 | ` * OP_CLONE * * *` |
|        - |  8457 | ` * Perfome a clone operation.` |
|        - |  8458 | ` */` |
|       24 |  8459 | `case PH7_OP_CLONE: {` |
|        - |  8460 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8461 | `#ifdef UNTRUST` |
|        - |  8462 | `	if( pTos < pStack ){` |
|        - |  8463 | `		goto Abort;` |
|        - |  8464 | `	}` |
|        - |  8465 | `#endif` |
|        - |  8466 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8467 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8468 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8469 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8470 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8471 | `		break;` |
|        - |  8472 | `	}` |
|        - |  8473 | `	/* Point to the source */` |
|       46 |  8474 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8475 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8476 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8477 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8478 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8479 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8480 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8481 | `		break;` |
|        - |  8482 | `	}` |
|        - |  8483 | `	/* Perform the clone operation */` |
|       46 |  8484 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8485 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8486 | `	if( pClone == 0 ){` |
|      ! 0 |  8487 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8488 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8489 | `	}else{` |
|        - |  8490 | `		/* Load the cloned object */` |
|       46 |  8491 | `		pTos->x.pOther = pClone;` |
|       46 |  8492 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8493 | `	}` |
|       46 |  8494 | `	break;` |
|        - |  8495 | `				   }` |
|        - |  8496 | `/*` |
|        - |  8497 | ` * OP_SWITCH * * P3` |
|        - |  8498 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8499 | ` */` |
|       26 |  8500 | `case PH7_OP_SWITCH: {` |
|       54 |  8501 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8502 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8503 | `	ph7_value sValue,sCaseValue;` |
|        - |  8504 | `	sxu32 n,nEntry;` |
|        - |  8505 | `#ifdef UNTRUST` |
|        - |  8506 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8507 | `		goto Abort;` |
|        - |  8508 | `	}` |
|        - |  8509 | `#endif` |
|        - |  8510 | `	/* Point to the case table  */` |
|       54 |  8511 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8512 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8513 | `	/* Select the appropriate case block to execute */` |
|       54 |  8514 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8515 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8516 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8517 | `		pCase = &aCase[n];` |
|      130 |  8518 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8519 | `		/* Execute the case expression first */` |
|      130 |  8520 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8521 | `		/* Compare the two expression */` |
|      130 |  8522 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8523 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8524 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8525 | `		if( rc == 0 ){` |
|        - |  8526 | `			/* Value match,jump to this block */` |
|       52 |  8527 | `			pc = pCase->nStart - 1;` |
|       52 |  8528 | `			break;` |
|        - |  8529 | `		}` |
|       41 |  8530 | `	}` |
|       54 |  8531 | `	VmPopOperand(&pTos,1);` |
|       54 |  8532 | `	if( n >= nEntry ){` |
|        - |  8533 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8534 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8535 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8536 | `		}else{` |
|        - |  8537 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8538 | `			pc = pSwitch->nOut - 1;` |
|        - |  8539 | `		}` |
|        1 |  8540 | `	}` |
|       54 |  8541 | `	break;` |
|        - |  8542 | `					}` |
|        - |  8543 | `/*` |
|        - |  8544 | ` * OP_MATCH * * P3` |
|        - |  8545 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8546 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8547 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8548 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8549 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8550 | ` */` |
|       54 |  8551 | `case PH7_OP_MATCH: {` |
|      110 |  8552 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8553 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8554 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8555 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8556 | `	int matched = 0;` |
|        - |  8557 | `#ifdef UNTRUST` |
|        - |  8558 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8559 | `		goto Abort;` |
|        - |  8560 | `	}` |
|        - |  8561 | `#endif` |
|      110 |  8562 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8563 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8564 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8565 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8566 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8567 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8568 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8569 | `		pArm = &aArm[i];` |
|      240 |  8570 | `		if( pArm->bDefault ){` |
|       13 |  8571 | `			pDefault = pArm;` |
|       13 |  8572 | `			continue;` |
|        - |  8573 | `		}` |
|      228 |  8574 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8575 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8576 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8577 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8578 | `				continue;` |
|        - |  8579 | `			}` |
|      260 |  8580 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8581 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8582 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8583 | `			if( rc == 0 ){` |
|       93 |  8584 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8585 | `				matched = 1;` |
|       93 |  8586 | `				break;` |
|        - |  8587 | `			}` |
|       85 |  8588 | `		}` |
|      115 |  8589 | `	}` |
|      110 |  8590 | `	if( !matched && pDefault ){` |
|       13 |  8591 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8592 | `		matched = 1;` |
|        6 |  8593 | `	}` |
|      110 |  8594 | `	if( !matched ){` |
|        5 |  8595 | `		const char *zType = "unknown";` |
|        - |  8596 | `		char zMsg[128];` |
|        - |  8597 | `		sxu32 nMsg;` |
|        5 |  8598 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8599 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8600 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8601 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8602 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8603 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8604 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8605 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8606 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8607 | `		default: break;` |
|        - |  8608 | `		}` |
|        7 |  8609 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8610 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8611 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8612 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8613 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8614 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8615 | `		goto Abort;` |
|        - |  8616 | `	}` |
|      105 |  8617 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8618 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8619 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8620 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8621 | `	break;` |
|        - |  8622 | `					}` |
|        - |  8623 | `/*` |
|        - |  8624 | ` * OP_YIELD P1 P2 *` |
|        - |  8625 | ` *  Yield a value from a generator function.` |
|        - |  8626 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8627 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8628 | ` */` |
|       34 |  8629 | `case PH7_OP_YIELD: {` |
|        - |  8630 | `	ph7_generator *pGen;` |
|       70 |  8631 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8632 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8633 | `		goto Abort;` |
|        - |  8634 | `	}` |
|       70 |  8635 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8636 | `	if( pInstr->iP2 ){` |
|        - |  8637 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8638 | `#ifdef UNTRUST` |
|        - |  8639 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8640 | `#endif` |
|        7 |  8641 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8642 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8643 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8644 | `		VmPopOperand(&pTos, 1);` |
|        - |  8645 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8646 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8647 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8648 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8649 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8650 | `			}` |
|        1 |  8651 | `		}` |
|       67 |  8652 | `	}else if( pInstr->iP1 ){` |
|        - |  8653 | `		/* yield $value */` |
|        - |  8654 | `#ifdef UNTRUST` |
|        - |  8655 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8656 | `#endif` |
|       64 |  8657 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8658 | `		VmPopOperand(&pTos, 1);` |
|        - |  8659 | `		/* Auto-increment key */` |
|       64 |  8660 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8661 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8662 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8663 | `	}else{` |
|        - |  8664 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8665 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8666 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8667 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8668 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8669 | `	}` |
|        - |  8670 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8671 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8672 | `	goto Suspend;` |
|        - |  8673 |  |
|        - |  8674 | `/*` |
|        - |  8675 | ` * OP_CALL P1 * *` |
|        - |  8676 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8677 | ` *  function on the stack.` |
|        - |  8678 | ` */` |
|   357607 |  8679 | `case PH7_OP_CALL: {` |
|   715260 |  8680 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8681 | `	ph7_value *pArg;` |
|   715260 |  8682 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   715260 |  8683 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8684 | `	SyHashEntry *pEntry;` |
|        - |  8685 | `	SyString sName;` |
|        - |  8686 | `	/* Extract function name */` |
|   715260 |  8687 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8688 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8689 | `			ph7_value sResult;` |
|        - |  8690 | `			sxi32 rcArr;` |
|        3 |  8691 | `			SySetReset(&aArg);` |
|        3 |  8692 | `			while( pArg < pTos ){` |
|      ! 0 |  8693 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8694 | `				pArg++;` |
|      ! 0 |  8695 | `			}` |
|        3 |  8696 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8697 | `			/* May be a class instance and it's static method */` |
|        3 |  8698 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8699 | `			SySetReset(&aArg);` |
|        - |  8700 | `			/* Pop given arguments */` |
|        3 |  8701 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8702 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8703 | `			}` |
|        3 |  8704 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8705 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8706 | `				goto Abort;` |
|        - |  8707 | `			}` |
|        3 |  8708 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8709 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8710 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8711 | `				sxi32 iResumePc;` |
|        3 |  8712 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8713 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8714 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8715 | `					pc = iResumePc;` |
|        3 |  8716 | `					break;` |
|        - |  8717 | `				}` |
|      ! 0 |  8718 | `				goto Exception;` |
|        - |  8719 | `			}` |
|        - |  8720 | `			/* Copy result */` |
|      ! 0 |  8721 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8722 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8723 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8724 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8725 | `			ph7_value sResult;` |
|        - |  8726 | `			sxi32 rcInv;` |
|       84 |  8727 | `			SySetReset(&aArg);` |
|      200 |  8728 | `			while( pArg < pTos ){` |
|      118 |  8729 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8730 | `				pArg++;` |
|        2 |  8731 | `			}` |
|       84 |  8732 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8733 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8734 | `				(int)SySetUsed(&aArg),` |
|       82 |  8735 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8736 | `				&sResult,` |
|       82 |  8737 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8738 | `			SySetReset(&aArg);` |
|       84 |  8739 | `			if( nCallArgs > 0 ){` |
|       76 |  8740 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8741 | `			}` |
|       84 |  8742 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8743 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8744 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8745 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8746 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8747 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8748 | `				pThis->iRef++;` |
|       13 |  8749 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8750 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8751 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8752 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8753 | `					goto Abort;` |
|        - |  8754 | `				}` |
|        - |  8755 | `				{` |
|       13 |  8756 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8757 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8758 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8759 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8760 | `						break;` |
|        - |  8761 | `					}` |
|        - |  8762 | `				}` |
|      ! 0 |  8763 | `				goto Exception;` |
|        - |  8764 | `			}` |
|       72 |  8765 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8766 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8767 | `				goto Abort;` |
|        - |  8768 | `			}` |
|       72 |  8769 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8770 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8771 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8772 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8773 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8774 | `				sxi32 iResumePc;` |
|        7 |  8775 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8776 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8777 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8778 | `					pc = iResumePc;` |
|        5 |  8779 | `					break;` |
|        - |  8780 | `				}` |
|        3 |  8781 | `				goto Exception;` |
|        - |  8782 | `			}` |
|       66 |  8783 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8784 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8785 | `		}else{` |
|        - |  8786 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8787 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8788 | `			/* Pop given arguments */` |
|      ! 0 |  8789 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8790 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8791 | `			}` |
|        - |  8792 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8793 | `			PH7_MemObjRelease(pTos);` |
|        - |  8794 | `		}` |
|       66 |  8795 | `		break;` |
|        - |  8796 | `	}` |
|   715176 |  8797 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8798 | `	/* Check for a compiled function first.` |
|        - |  8799 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8800 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   715176 |  8801 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8802 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8803 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8804 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8805 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8806 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8807 | `	{` |
|   715176 |  8808 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   715176 |  8809 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8810 | `		const char *zFunc;` |
|        - |  8811 | `		const char *zEnd;` |
|        - |  8812 | `		const char *z;` |
|        - |  8813 | `		SyString sGlobal;` |
|       22 |  8814 | `		zFunc = sName.zString;` |
|       22 |  8815 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8816 | `		z = zEnd;` |
|        - |  8817 | `		/* Find last namespace separator */` |
|      194 |  8818 | `		while( z > zFunc ){` |
|      194 |  8819 | `			if( z[-1] == '\\' ){` |
|       22 |  8820 | `				break;` |
|        - |  8821 | `			}` |
|      174 |  8822 | `			z--;` |
|        2 |  8823 | `		}` |
|       22 |  8824 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8825 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8826 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8827 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8828 | `		}` |
|       10 |  8829 | `	}` |
|        - |  8830 | `	} /* end VmCallArgMap namespace scope */` |
|   715176 |  8831 | `	if( pEntry ){` |
|        - |  8832 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8833 | `		ph7_class_instance *pThis;` |
|        - |  8834 | `		ph7_value *pFrameStack;` |
|        - |  8835 | `		ph7_vm_func *pVmFunc;` |
|        - |  8836 | `		ph7_class *pSelf;` |
|        - |  8837 | `		VmFrame *pFrame;` |
|        - |  8838 | `		ph7_value *pObj;` |
|        - |  8839 | `		VmSlot sArg;` |
|        - |  8840 | `		sxu32 n;` |
|        - |  8841 | `		/* initialize fields */` |
|    18514 |  8842 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18514 |  8843 | `		pThis = 0;` |
|    18514 |  8844 | `		pSelf = 0;` |
|    18514 |  8845 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8846 | `			ph7_class_method *pMeth;` |
|        - |  8847 | `			/* Class method call */` |
|     3350 |  8848 | `			ph7_value *pTarget = &pTos[-1];` |
|     3350 |  8849 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8850 | `				/* Extract the 'this' pointer */` |
|     3350 |  8851 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8852 | `					/* Instance already loaded */` |
|     3260 |  8853 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3260 |  8854 | `					pThis->iRef++;` |
|     3260 |  8855 | `					pSelf = pThis->pClass;` |
|     1629 |  8856 | `				}` |
|     3350 |  8857 | `				if( pSelf == 0 ){` |
|       92 |  8858 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8859 | `						/* "Late Static Binding" class name */` |
|      128 |  8860 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8861 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8862 | `					}` |
|       92 |  8863 | `					if( pSelf == 0 ){` |
|       21 |  8864 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8865 | `					}` |
|       45 |  8866 | `				}` |
|     3350 |  8867 | `				if( pThis == 0  ){` |
|       92 |  8868 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8869 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8870 | `					if( pFrameLocal->pParent ){` |
|        - |  8871 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8872 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8873 | `						if( pThis ){` |
|       21 |  8874 | `							pThis->iRef++;` |
|       10 |  8875 | `						}` |
|       32 |  8876 | `					}` |
|       45 |  8877 | `				}` |
|     3350 |  8878 | `				VmPopOperand(&pTos,1);` |
|     3350 |  8879 | `				PH7_MemObjRelease(pTos);` |
|        - |  8880 | `				/* Synchronize pointers */` |
|     3350 |  8881 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8882 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8883 | `				 * user have already computed the random generated unique class method name` |
|        - |  8884 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8885 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8886 | `				 */` |
|     3350 |  8887 | `				while( pArg < pStack ){` |
|      ! 0 |  8888 | `					pArg++;` |
|      ! 0 |  8889 | `				}` |
|     3350 |  8890 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8891 | `					/* Check if the call is allowed */` |
|     3350 |  8892 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3350 |  8893 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8894 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8895 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8896 | `							char zMsg[256];` |
|      ! 0 |  8897 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8898 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8899 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8900 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8901 | `							/* Pop given arguments */` |
|      ! 0 |  8902 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8903 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8904 | `							}` |
|      ! 0 |  8905 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8906 | `							goto Abort;` |
|        - |  8907 | `						}` |
|        6 |  8908 | `					}` |
|     1674 |  8909 | `				}` |
|     1674 |  8910 | `			}` |
|     1674 |  8911 | `		}` |
|        - |  8912 | `		/* Check The recursion limit */` |
|    18514 |  8913 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8914 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8915 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8916 | `				&pVmFunc->sName);` |
|        - |  8917 | `			/* Pop given arguments */` |
|        3 |  8918 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8919 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8920 | `			}` |
|        - |  8921 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8922 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8923 | `			break;` |
|        - |  8924 | `		}` |
|    18512 |  8925 | `		if( pVmFunc->pNextName ){` |
|        - |  8926 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8927 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8928 | `		}` |
|    18512 |  8929 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8930 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8931 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8932 | `			ph7_generator *pGenerator;` |
|        - |  8933 | `			ph7_class_instance *pGenObj;` |
|        - |  8934 | `			ph7_value *pCtxAttr;` |
|        - |  8935 | `			SyString sAttrName;` |
|        - |  8936 | `			ph7_value **apCallArgs;` |
|        - |  8937 | `			int nGenArgs, iArg;` |
|        - |  8938 | `			/* Collect arguments from the operand stack */` |
|       24 |  8939 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8940 | `			apCallArgs = 0;` |
|       24 |  8941 | `			if( nGenArgs > 0 ){` |
|       14 |  8942 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8943 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8944 | `				if( apCallArgs == 0 ){` |
|        - |  8945 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8946 | `					nGenArgs = 0;` |
|      ! 0 |  8947 | `				}else{` |
|       10 |  8948 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8949 | `					int didReorder = 0;` |
|       10 |  8950 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8951 | `						/* Named-argument reordering for generator */` |
|        5 |  8952 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8953 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8954 | `						sxu32 nNV = nF;` |
|        5 |  8955 | `						sxi32 iVIdx = -1;` |
|        - |  8956 | `						sxi32 *aGSlot;` |
|        - |  8957 | `						sxu8 *aGUsed;` |
|        - |  8958 | `						sxu32 gi;` |
|       13 |  8959 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8960 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8961 | `						}` |
|        7 |  8962 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8963 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8964 | `						if( aGSlot ){` |
|        5 |  8965 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8966 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8967 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8968 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8969 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8970 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8971 | `								goto Abort;` |
|        - |  8972 | `							}` |
|        - |  8973 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8974 | `							 * append overflow (variadic / positional beyond` |
|        - |  8975 | `							 * formals) so downstream sees every argument. */` |
|        - |  8976 | `							{` |
|        5 |  8977 | `								int nOut = 0;` |
|       13 |  8978 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8979 | `									sxu32 gj;` |
|       13 |  8980 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8981 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8982 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8983 | `											break;` |
|        - |  8984 | `										}` |
|        3 |  8985 | `									}` |
|        5 |  8986 | `								}` |
|       13 |  8987 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8988 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8989 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8990 | `									}` |
|        5 |  8991 | `								}` |
|        5 |  8992 | `								nGenArgs = nOut;` |
|        - |  8993 | `							}` |
|        5 |  8994 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8995 | `							didReorder = 1;` |
|        2 |  8996 | `						}` |
|        - |  8997 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8998 | `						 * positional fill below — preserves arg order rather` |
|        - |  8999 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9000 | `					}` |
|       10 |  9001 | `					if( !didReorder ){` |
|       12 |  9002 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9003 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9004 | `						}` |
|        2 |  9005 | `					}` |
|        - |  9006 | `				}` |
|        4 |  9007 | `			}` |
|        - |  9008 | `			/* Create execution context and generator wrapper */` |
|       24 |  9009 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  9010 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9011 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9012 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9013 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9014 | `				break;` |
|        - |  9015 | `			}` |
|       24 |  9016 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  9017 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9018 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9019 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9020 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9021 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9022 | `				break;` |
|        - |  9023 | `			}` |
|        - |  9024 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  9025 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  9026 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  9027 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  9028 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  9029 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  9030 | `			if( apCallArgs ){` |
|       10 |  9031 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9032 | `			}` |
|       24 |  9033 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9034 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9035 | `				if( pThis ){` |
|      ! 0 |  9036 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9037 | `				}` |
|      ! 0 |  9038 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9039 | `					goto Abort;` |
|        - |  9040 | `				}` |
|      ! 0 |  9041 | `				break;` |
|        - |  9042 | `			}` |
|        - |  9043 | `			/* Create Generator class instance */` |
|       24 |  9044 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  9045 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9046 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9047 | `				break;` |
|        - |  9048 | `			}` |
|        - |  9049 | `			/* Store generator in __ctx attribute */` |
|       24 |  9050 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  9051 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  9052 | `			if( pCtxAttr ){` |
|       24 |  9053 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  9054 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  9055 | `			}` |
|        - |  9056 | `			/* Pop args and function name, push Generator object */` |
|       24 |  9057 | `			PH7_MemObjRelease(pTos);` |
|       24 |  9058 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  9059 | `			pTos->x.pOther = pGenObj;` |
|       24 |  9060 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  9061 | `			pGenObj->iRef++;` |
|       24 |  9062 | `			if( pThis ){` |
|      ! 0 |  9063 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9064 | `			}` |
|       24 |  9065 | `			break;` |
|        - |  9066 | `		}` |
|        - |  9067 | `		/* Extract the formal argument set */` |
|    18490 |  9068 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9069 | `		/* Create a new VM frame  */` |
|    18490 |  9070 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18490 |  9071 | `		if( rc != SXRET_OK ){` |
|        - |  9072 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9073 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9074 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9075 | `				&pVmFunc->sName);` |
|        - |  9076 | `			/* Pop given arguments */` |
|      ! 0 |  9077 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9078 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9079 | `			}` |
|        - |  9080 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9081 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9082 | `			break;` |
|        - |  9083 | `		}` |
|    18490 |  9084 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9085 | `			/* Install the '$this' variable */` |
|        - |  9086 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3278 |  9087 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3278 |  9088 | `			if( pObj ){` |
|        - |  9089 | `				/* Reflect the change */` |
|     3278 |  9090 | `				pObj->x.pOther = pThis;` |
|     3278 |  9091 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1638 |  9092 | `			}` |
|     1638 |  9093 | `		}` |
|    18490 |  9094 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9095 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9096 | `			/* Install static variables */` |
|        6 |  9097 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       12 |  9098 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|        6 |  9099 | `				pStatic = &aStatic[n];` |
|        6 |  9100 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9101 | `					/* Initialize the static variables */` |
|        6 |  9102 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        6 |  9103 | `					if( pObj ){` |
|        - |  9104 | `						/* Assume a NULL initialization value */` |
|        6 |  9105 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        6 |  9106 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9107 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        6 |  9108 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|        3 |  9109 | `						}` |
|        6 |  9110 | `						pObj->nIdx = pStatic->nIdx;` |
|        3 |  9111 | `					}else{` |
|      ! 0 |  9112 | `						continue;` |
|        - |  9113 | `					}` |
|        3 |  9114 | `				}` |
|        - |  9115 | `				/* Install in the current frame */` |
|        9 |  9116 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|        6 |  9117 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        3 |  9118 | `			}` |
|        3 |  9119 | `		}` |
|        - |  9120 | `		/* Push arguments in the local frame */` |
|        - |  9121 | `		{` |
|    18490 |  9122 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9123 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9124 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18490 |  9125 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18490 |  9126 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9127 | `			/* ============================================================` |
|        - |  9128 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9129 | `			 *` |
|        - |  9130 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9131 | `			 * or position, then install them in the frame.` |
|        - |  9132 | `			 * ============================================================ */` |
|       96 |  9133 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  9134 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  9135 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9136 | `			sxu32 nNonVariadic;` |
|        - |  9137 | `			sxi32 *aSlot;` |
|        - |  9138 | `			sxu8  *aUsed;` |
|        - |  9139 | `			sxu32 i;` |
|        - |  9140 | `			/* Find variadic parameter index */` |
|      292 |  9141 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  9142 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9143 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9144 | `					break;` |
|        - |  9145 | `				}` |
|      100 |  9146 | `			}` |
|       96 |  9147 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9148 | `			/* Allocate mapping arrays */` |
|      143 |  9149 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9150 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  9151 | `			if( aSlot == 0 ){` |
|      ! 0 |  9152 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9153 | `				goto Abort;` |
|        - |  9154 | `			}` |
|       96 |  9155 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9156 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  9157 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9158 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  9159 | `			if( rc == PH7_ABORT ){` |
|        7 |  9160 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  9161 | `				goto Abort;` |
|        - |  9162 | `			}` |
|        - |  9163 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9164 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9165 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9166 | `				sxi32 iSrc = -1;` |
|      309 |  9167 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9168 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9169 | `						iSrc = (sxi32)i;` |
|      169 |  9170 | `						break;` |
|        - |  9171 | `					}` |
|       62 |  9172 | `				}` |
|      187 |  9173 | `				if( iSrc >= 0 ){` |
|        - |  9174 | `					/* Argument was provided — install with type checking */` |
|      169 |  9175 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9176 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9177 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9178 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9179 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  9180 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9181 | `					}` |
|        - |  9182 | `					/* Type checking: union types */` |
|      169 |  9183 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9184 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9185 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9186 | `							bCallIsStrict);` |
|       13 |  9187 | `						if( rcU != SXRET_OK ){` |
|        - |  9188 | `							const char *zGiven;` |
|      ! 0 |  9189 | `							const char *zExpected = "union";` |
|        - |  9190 | `							char zBuf[128];` |
|        - |  9191 | `							char zTypeBuf[128];` |
|      ! 0 |  9192 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9193 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9194 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9195 | `								zGiven = "null";` |
|      ! 0 |  9196 | `							}else{` |
|      ! 0 |  9197 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9198 | `							}` |
|      ! 0 |  9199 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9200 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9201 | `							}` |
|      ! 0 |  9202 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9203 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9204 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9205 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9206 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9207 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9208 | `							pFrameStack = 0;` |
|      ! 0 |  9209 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9210 | `							goto SkipFuncBody;` |
|        - |  9211 | `						}` |
|      171 |  9212 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9213 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9214 | `						/* Scalar/class type checking */` |
|       17 |  9215 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9216 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  9217 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9218 | `							if( pClass ){` |
|      ! 0 |  9219 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9220 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9221 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9222 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9223 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9224 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9225 | `									}` |
|      ! 0 |  9226 | `								}else{` |
|      ! 0 |  9227 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9228 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9229 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9230 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9231 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9232 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9233 | `									}` |
|        - |  9234 | `								}` |
|      ! 0 |  9235 | `							}` |
|       17 |  9236 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9237 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9238 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9239 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9240 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9241 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9242 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9243 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9244 | `								pFrameStack = 0;` |
|      ! 0 |  9245 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9246 | `								goto SkipFuncBody;` |
|        7 |  9247 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9248 | `								char zTypeBuf[128];` |
|      ! 0 |  9249 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9250 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9251 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9252 | `									ph7_type_name(pVal));` |
|      ! 0 |  9253 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9254 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9255 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9256 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9257 | `								pFrameStack = 0;` |
|      ! 0 |  9258 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9259 | `								goto SkipFuncBody;` |
|        - |  9260 | `							}` |
|        3 |  9261 | `						}` |
|        8 |  9262 | `					}` |
|        - |  9263 | `					/* Install: by reference or by value */` |
|      169 |  9264 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9265 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9266 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9267 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9268 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9269 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9270 | `							}` |
|      ! 0 |  9271 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9272 | `						}else{` |
|        7 |  9273 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9274 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9275 | `							if( pRefEntry == 0 ){` |
|        7 |  9276 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9277 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9278 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9279 | `								sArg.pUserData = 0;` |
|        5 |  9280 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9281 | `							}` |
|        5 |  9282 | `							pObj = 0;` |
|        - |  9283 | `						}` |
|        3 |  9284 | `					}else{` |
|      165 |  9285 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9286 | `					}` |
|      169 |  9287 | `					if( pObj ){` |
|      165 |  9288 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9289 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9290 | `						sArg.pUserData = 0;` |
|      165 |  9291 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9292 | `					}` |
|       85 |  9293 | `				}else{` |
|        - |  9294 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9295 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9296 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9297 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9298 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9299 | `						if( pObj ){` |
|       19 |  9300 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9301 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9302 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9303 | `							sArg.pUserData = 0;` |
|       19 |  9304 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9305 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9306 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9307 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9308 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9309 | `							}` |
|        9 |  9310 | `						}` |
|        9 |  9311 | `					}` |
|        - |  9312 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9313 | `				}` |
|       94 |  9314 | `			}` |
|        - |  9315 | `			/* Handle variadic parameter */` |
|       89 |  9316 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9317 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9318 | `				if( pObj ){` |
|        9 |  9319 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9320 | `					{` |
|        9 |  9321 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9322 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9323 | `							if( aSlot[i] == -1 ){` |
|       16 |  9324 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9325 | `									/* Named variadic entry: insert with string key */` |
|        - |  9326 | `									ph7_value sKey;` |
|       11 |  9327 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9328 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9329 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9330 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9331 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9332 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9333 | `								}else{` |
|        - |  9334 | `									/* Positional variadic entry */` |
|      ! 0 |  9335 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9336 | `								}` |
|        5 |  9337 | `							}` |
|       12 |  9338 | `						}` |
|        - |  9339 | `					}` |
|        9 |  9340 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9341 | `					sArg.pUserData = 0;` |
|        9 |  9342 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9343 | `				}` |
|        5 |  9344 | `			}else{` |
|        - |  9345 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9346 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9347 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9348 | `				 * the positional-only path's behavior. */` |
|       81 |  9349 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9350 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9351 | `					if( aSlot[i] == -2 ){` |
|        - |  9352 | `						char zAnonBuf[32];` |
|        - |  9353 | `						SyString sAnonName;` |
|      ! 0 |  9354 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9355 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9356 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9357 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9358 | `						if( pObj ){` |
|      ! 0 |  9359 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9360 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9361 | `							sArg.pUserData = 0;` |
|      ! 0 |  9362 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9363 | `						}` |
|      ! 0 |  9364 | `						nAnon++;` |
|      ! 0 |  9365 | `					}` |
|       79 |  9366 | `				}` |
|        - |  9367 | `			}` |
|        - |  9368 | `			/* Release all stack arguments */` |
|      267 |  9369 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9370 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9371 | `			}` |
|       89 |  9372 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9373 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9374 | `			n = nFormal;` |
|       45 |  9375 | `		}else{` |
|        - |  9376 | `		/* ============================================================` |
|        - |  9377 | `		 * Positional-only matching path (original)` |
|        - |  9378 | `		 * ============================================================ */` |
|    18396 |  9379 | `		n = 0;` |
|    48966 |  9380 | `		while( pArg < pTos ){` |
|    30644 |  9381 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9382 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9383 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9384 | `				if( pObj ){` |
|        - |  9385 | `					/* Initialize as empty array */` |
|       40 |  9386 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9387 | `					{` |
|       40 |  9388 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9389 | `						while( pArg < pTos ){` |
|        - |  9390 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9391 | `							 *` |
|        - |  9392 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9393 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9394 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9395 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9396 | `							 * fixing both wants a separate counter for elements` |
|        - |  9397 | `							 * already packed into the variadic array. */` |
|      114 |  9398 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9399 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9400 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9401 | `									bCallIsStrict);` |
|       16 |  9402 | `								if( rcU != SXRET_OK ){` |
|        - |  9403 | `									const char *zGiven;` |
|        3 |  9404 | `									const char *zExpected = "union";` |
|        - |  9405 | `									char zBuf[128];` |
|        - |  9406 | `									char zTypeBuf[128];` |
|        3 |  9407 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9408 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9409 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9410 | `										zGiven = "null";` |
|      ! 0 |  9411 | `									}else{` |
|        3 |  9412 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9413 | `									}` |
|        3 |  9414 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9415 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9416 | `									}` |
|        4 |  9417 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9418 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9419 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9420 | `										goto Abort;` |
|        - |  9421 | `									}` |
|        3 |  9422 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9423 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9424 | `									pFrameStack = 0;` |
|        3 |  9425 | `									rc = PH7_EXCEPTION;` |
|        3 |  9426 | `									goto SkipFuncBody;` |
|        - |  9427 | `								}` |
|       14 |  9428 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9429 | `								pArg++;` |
|       14 |  9430 | `								continue;` |
|        - |  9431 | `							}` |
|        - |  9432 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9433 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9434 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9435 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9436 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9437 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9438 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9439 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9440 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9441 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9442 | `										goto Abort;` |
|        - |  9443 | `									}` |
|        - |  9444 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9445 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9446 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9447 | `									pFrameStack = 0;` |
|      ! 0 |  9448 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9449 | `									goto SkipFuncBody;` |
|       13 |  9450 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9451 | `									char zTypeBuf[128];` |
|      ! 0 |  9452 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9453 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9454 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9455 | `										ph7_type_name(pArg));` |
|      ! 0 |  9456 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9457 | `										goto Abort;` |
|        - |  9458 | `									}` |
|      ! 0 |  9459 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9460 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9461 | `									pFrameStack = 0;` |
|      ! 0 |  9462 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9463 | `									goto SkipFuncBody;` |
|        - |  9464 | `								}` |
|        6 |  9465 | `							}` |
|      100 |  9466 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9467 | `							pArg++;` |
|        2 |  9468 | `						}` |
|        - |  9469 | `					}` |
|       38 |  9470 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9471 | `					sArg.pUserData = 0;` |
|       38 |  9472 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9473 | `				}` |
|       38 |  9474 | `				break; /* All remaining args consumed */` |
|        - |  9475 | `			}` |
|    30606 |  9476 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30388 |  9477 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9478 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9479 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9480 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9481 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9482 | `						goto Abort;` |
|        - |  9483 | `					}` |
|      ! 0 |  9484 | `				}` |
|        - |  9485 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30390 |  9486 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9487 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9488 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9489 | `						bCallIsStrict);` |
|       60 |  9490 | `					if( rcU != SXRET_OK ){` |
|        - |  9491 | `						const char *zGiven;` |
|       19 |  9492 | `						const char *zExpected = "union";` |
|        - |  9493 | `						char zBuf[128];` |
|        - |  9494 | `						char zTypeBuf[128];` |
|       19 |  9495 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9496 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9497 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9498 | `							zGiven = "null";` |
|        5 |  9499 | `						}else{` |
|        5 |  9500 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9501 | `						}` |
|       19 |  9502 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9503 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9504 | `						}` |
|       28 |  9505 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9506 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9507 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9508 | `							goto Abort;` |
|        - |  9509 | `						}` |
|       19 |  9510 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9511 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9512 | `						pFrameStack = 0;` |
|       19 |  9513 | `						rc = PH7_EXCEPTION;` |
|       19 |  9514 | `						goto SkipFuncBody;` |
|        - |  9515 | `					}` |
|       21 |  9516 | `				}else` |
|        - |  9517 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9518 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30356 |  9519 | `				if( aFormalArg[n].nType > 0` |
|    15882 |  9520 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9521 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9522 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9523 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9524 | `						ph7_class *pClass;` |
|        - |  9525 | `						/* Try to extract the desired class */` |
|       26 |  9526 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9527 | `						if( pClass ){` |
|       22 |  9528 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9529 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9530 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9531 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9532 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9533 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9534 | `								}` |
|      ! 0 |  9535 | `							}else{` |
|        - |  9536 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9537 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9538 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9539 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9540 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9541 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9542 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9543 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9544 | `								}` |
|        - |  9545 | `							}` |
|       12 |  9546 | `						}` |
|     1394 |  9547 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9548 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9549 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9550 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9551 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9552 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9553 | `								goto Abort;` |
|        - |  9554 | `							}` |
|        - |  9555 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9556 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9557 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9558 | `							pFrameStack = 0;` |
|       11 |  9559 | `							rc = PH7_EXCEPTION;` |
|       11 |  9560 | `							goto SkipFuncBody;` |
|       16 |  9561 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9562 | `							char zTypeBuf[128];` |
|       11 |  9563 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9564 | `								&aFormalArg[n].sName,` |
|        6 |  9565 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9566 | `								ph7_type_name(pArg));` |
|        8 |  9567 | `							if( rc == PH7_ABORT ){` |
|        5 |  9568 | `								goto Abort;` |
|        - |  9569 | `							}` |
|        3 |  9570 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9571 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9572 | `							pFrameStack = 0;` |
|        3 |  9573 | `							rc = PH7_EXCEPTION;` |
|        3 |  9574 | `							goto SkipFuncBody;` |
|        - |  9575 | `						}` |
|        4 |  9576 | `					}` |
|      694 |  9577 | `				}` |
|    30356 |  9578 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9579 | `					/* Pass by reference */` |
|       58 |  9580 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9581 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9582 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9583 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9584 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9585 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9586 | `						}` |
|        - |  9587 | `						/* Switch to pass by value */` |
|      ! 0 |  9588 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9589 | `					}else{` |
|        - |  9590 | `						SyHashEntry *pRefEntry;` |
|        - |  9591 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9592 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9593 | `						if( pRefEntry == 0 ){` |
|       86 |  9594 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9595 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9596 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9597 | `							sArg.pUserData = 0;` |
|       58 |  9598 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9599 | `						}` |
|       58 |  9600 | `						pObj = 0;` |
|        - |  9601 | `					}` |
|       30 |  9602 | `				}else{` |
|        - |  9603 | `					/* Pass by value,make a copy of the given argument */` |
|    30300 |  9604 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9605 | `				}` |
|    15179 |  9606 | `			}else{` |
|        - |  9607 | `				char zName[32];` |
|        - |  9608 | `				SyString sArgName;` |
|        - |  9609 | `				/* Set a dummy name */` |
|      218 |  9610 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9611 | `				sArgName.zString = zName;` |
|        - |  9612 | `				/* Annonymous argument */` |
|      218 |  9613 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9614 | `			}` |
|    30572 |  9615 | `			if( pObj ){` |
|    30516 |  9616 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9617 | `				/* Insert argument index  */` |
|    30516 |  9618 | `				sArg.nIdx = pObj->nIdx;` |
|    30516 |  9619 | `				sArg.pUserData = 0;` |
|    30516 |  9620 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15257 |  9621 | `			}` |
|    30572 |  9622 | `			PH7_MemObjRelease(pArg);` |
|    30572 |  9623 | `			pArg++;` |
|    30572 |  9624 | `			++n;` |
|        2 |  9625 | `		}` |
|        - |  9626 | `		} /* end named vs positional branch */` |
|        - |  9627 | `		/* Set up closure environment */` |
|    18448 |  9628 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9629 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9630 | `			ph7_value *pValue;` |
|        - |  9631 | `			sxu32 iEnv;` |
|      184 |  9632 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 |  9633 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 |  9634 | `				pEnv = &aEnv[iEnv];` |
|      252 |  9635 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9636 | `					/* Do not install null value */` |
|      178 |  9637 | `					continue;` |
|        - |  9638 | `				}` |
|       76 |  9639 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9640 | `				if( pValue == 0 ){` |
|      ! 0 |  9641 | `					continue;` |
|        - |  9642 | `				}` |
|        - |  9643 | `				/* Invalidate any prior representation */` |
|       76 |  9644 | `				PH7_MemObjRelease(pValue);` |
|        - |  9645 | `				/* Duplicate bound variable value */` |
|       76 |  9646 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9647 | `			}` |
|       91 |  9648 | `		}` |
|        - |  9649 | `		/* Process default values for remaining formal parameters */` |
|    21336 |  9650 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2936 |  9651 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9652 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9653 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9654 | `				if( pObj ){` |
|       48 |  9655 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9656 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9657 | `					sArg.pUserData = 0;` |
|       48 |  9658 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9659 | `				}` |
|       48 |  9660 | `				n++;` |
|       48 |  9661 | `				break; /* Variadic is always last */` |
|        - |  9662 | `			}` |
|     2890 |  9663 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2884 |  9664 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2884 |  9665 | `				if( pObj ){` |
|        - |  9666 | `					/* Evaluate the default value and extract it's result */` |
|     2884 |  9667 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2884 |  9668 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9669 | `						goto Abort;` |
|        - |  9670 | `					}` |
|        - |  9671 | `					/* Insert argument index */` |
|     2884 |  9672 | `					sArg.nIdx = pObj->nIdx;` |
|     2884 |  9673 | `					sArg.pUserData = 0;` |
|     2884 |  9674 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9675 | `					/* Make sure the default argument is of the correct type */` |
|     2882 |  9676 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1864 |  9677 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9678 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9679 | `						/* Cast to the desired type */` |
|        3 |  9680 | `						xCast(pObj);` |
|        1 |  9681 | `					}` |
|     1441 |  9682 | `				}` |
|     1441 |  9683 | `			}` |
|     2890 |  9684 | `			++n;` |
|        2 |  9685 | `		}` |
|        - |  9686 | `		} /* end VmCallArgMap scope */` |
|        - |  9687 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9688 | `		 * does not return anything.` |
|        - |  9689 | `		 */` |
|    18448 |  9690 | `		PH7_MemObjRelease(pTos);` |
|    18448 |  9691 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9692 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18448 |  9693 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18448 |  9694 | `		if( pFrameStack == 0 ){` |
|        - |  9695 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9696 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9697 | `				&pVmFunc->sName);` |
|      ! 0 |  9698 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9699 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9700 | `			}` |
|      ! 0 |  9701 | `			break;` |
|        - |  9702 | `		}` |
|     9223 |  9703 | `SkipFuncBody:` |
|    18480 |  9704 | `		if( pSelf ){` |
|        - |  9705 | `			/* Push class name */` |
|     3348 |  9706 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1673 |  9707 | `		}` |
|        - |  9708 | `		/* Increment nesting level */` |
|    18480 |  9709 | `		pVm->nRecursionDepth++;` |
|    18480 |  9710 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9711 | `			/* Execute function body */` |
|    27671 |  9712 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18446 |  9713 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9223 |  9714 | `		}` |
|        - |  9715 | `		/* Decrement nesting level */` |
|    18480 |  9716 | `		pVm->nRecursionDepth--;` |
|    18480 |  9717 | `		if( pSelf ){` |
|        - |  9718 | `			/* Pop class name */` |
|     3348 |  9719 | `			(void)SySetPop(&pVm->aSelf);` |
|     1673 |  9720 | `		}` |
|        - |  9721 | `		/* Cleanup the mess left behind */` |
|    18480 |  9722 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9723 | `			/* Return by reference,reflect that */` |
|        9 |  9724 | `			if( n != SXU32_HIGH ){` |
|        9 |  9725 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9726 | `				sxu32 i;` |
|        - |  9727 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9728 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9729 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9730 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9731 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9732 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9733 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9734 | `								&pVmFunc->sName);` |
|      ! 0 |  9735 | `						}` |
|      ! 0 |  9736 | `						n = SXU32_HIGH;` |
|      ! 0 |  9737 | `						break;` |
|        - |  9738 | `					}` |
|        3 |  9739 | `				}` |
|        5 |  9740 | `			}else{` |
|      ! 0 |  9741 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9742 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9743 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9744 | `						&pVmFunc->sName);` |
|      ! 0 |  9745 | `				}` |
|        - |  9746 | `			}` |
|        9 |  9747 | `			pTos->nIdx = n;` |
|        4 |  9748 | `		}` |
|        - |  9749 | `		/* Cleanup the mess left behind */` |
|    18480 |  9750 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9751 | `			/* An exception was throw in this frame */` |
|      100 |  9752 | `			pFrame = pFrame->pParent;` |
|      100 |  9753 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9754 | `				/* Pop the resutlt */` |
|       62 |  9755 | `				VmPopOperand(&pTos,1);` |
|        - |  9756 | `				/* Jump to this destination */` |
|       62 |  9757 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9758 | `				rc = PH7_OK;` |
|       32 |  9759 | `			}else{` |
|       39 |  9760 | `				if( pFrame->pParent ){` |
|       39 |  9761 | `					rc = PH7_EXCEPTION;` |
|       20 |  9762 | `				}else{` |
|        - |  9763 | `					/* Continue normal execution */` |
|      ! 0 |  9764 | `					rc = PH7_OK;` |
|        - |  9765 | `				}` |
|        - |  9766 | `			}` |
|       49 |  9767 | `		}` |
|        - |  9768 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18480 |  9769 | `		if( pFrameStack ){` |
|    18448 |  9770 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9223 |  9771 | `		}` |
|        - |  9772 | `		/* Leave the frame */` |
|    18480 |  9773 | `		VmLeaveFrame(&(*pVm));` |
|    18480 |  9774 | `		if( rc == PH7_ABORT ){` |
|        - |  9775 | `			/* Abort processing immeditaley */` |
|       17 |  9776 | `			goto Abort;` |
|    18464 |  9777 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9778 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9779 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9780 | `			 * overwriting the state saved by the inner level.` |
|        - |  9781 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9782 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9783 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9784 | `			goto Suspend;` |
|    18426 |  9785 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9786 | `			goto Exception;` |
|        - |  9787 | `		}` |
|     9195 |  9788 | `	}else{` |
|        - |  9789 | `		ph7_user_func *pFunc;` |
|        - |  9790 | `		ph7_context sCtx;` |
|        - |  9791 | `		ph7_value sRet;` |
|        - |  9792 | `		/* Look for an installed foreign function.` |
|        - |  9793 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9794 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9795 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9796 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   696664 |  9797 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9798 | `		{` |
|   696664 |  9799 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   696664 |  9800 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9801 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9802 | `			const char *zShort = sName.zString;` |
|        - |  9803 | `			sxu32 i;` |
|      334 |  9804 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9805 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9806 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9807 | `				}` |
|      158 |  9808 | `			}` |
|       22 |  9809 | `			if( zShort != sName.zString ){` |
|       22 |  9810 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9811 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9812 | `			}` |
|       10 |  9813 | `		}` |
|        - |  9814 | `		} /* end VmCallArgMap namespace scope */` |
|   696664 |  9815 | `		if( pEntry == 0 ){` |
|        - |  9816 | `			/* Call to undefined function */` |
|        5 |  9817 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9818 | `			/* Pop given arguments */` |
|        5 |  9819 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9820 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9821 | `			}` |
|        - |  9822 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9823 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9824 | `			break;` |
|        - |  9825 | `		}` |
|   696660 |  9826 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9827 | `		/* Start collecting function arguments */` |
|   696660 |  9828 | `		SySetReset(&aArg);` |
|  1878302 |  9829 | `		while( pArg < pTos ){` |
|  1181644 |  9830 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1181644 |  9831 | `			pArg++;` |
|        2 |  9832 | `		}` |
|        - |  9833 | `		/* Assume a null return value */` |
|   696660 |  9834 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9835 | `		/* Init the call context */` |
|   696660 |  9836 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9837 | `		/* Call the foreign function */` |
|   696660 |  9838 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9839 | `		/* Release the call context */` |
|   696660 |  9840 | `		VmReleaseCallContext(&sCtx);` |
|   696660 |  9841 | `		if( rc == PH7_ABORT ){` |
|        - |  9842 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - |  9843 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - |  9844 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      497 |  9845 | `			PH7_MemObjRelease(&sRet);` |
|      497 |  9846 | `			goto Abort;` |
|   696164 |  9847 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9848 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9849 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9850 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9851 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9852 | `				goto Exception;` |
|        - |  9853 | `			}` |
|        - |  9854 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9855 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9856 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9857 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9858 | `			}` |
|        - |  9859 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9860 | `			VmPopOperand(&pTos,1);` |
|        - |  9861 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9862 | `			pFrm = pVm->pFrame;` |
|      108 |  9863 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9864 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9865 | `			}` |
|      108 |  9866 | `			break;` |
|        - |  9867 | `		}` |
|   696054 |  9868 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9869 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9870 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9871 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9872 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9873 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9874 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9875 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9876 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9877 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9878 | `			}` |
|        - |  9879 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9880 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9881 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9882 | `			goto Suspend;` |
|        - |  9883 | `		}` |
|   696016 |  9884 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9885 | `			/* Pop function name and arguments */` |
|   674030 |  9886 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   337036 |  9887 | `		}` |
|        - |  9888 | `		/* Save foreign function return value */` |
|   696016 |  9889 | `		PH7_MemObjStore(&sRet,pTos);` |
|   696016 |  9890 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9891 | `	}` |
|   714402 |  9892 | `	break;` |
|        - |  9893 | `				  }` |
|        - |  9894 | `/*` |
|        - |  9895 | ` * OP_CONSUME: P1 * *` |
|        - |  9896 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9897 | ` */` |
|    15938 |  9898 | `case PH7_OP_CONSUME: {` |
|    31878 |  9899 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    31878 |  9900 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9901 |  |
|    31878 |  9902 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    31878 |  9903 | `	pCur = pOut;` |
|        - |  9904 | `	/* Start the consume process  */` |
|    63796 |  9905 | `	while( pOut <= pTos ){` |
|        - |  9906 | `		/* Force a string cast */` |
|    31920 |  9907 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1052 |  9908 | `			PH7_MemObjToString(pOut);` |
|      525 |  9909 | `		}` |
|    31920 |  9910 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9911 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9912 | `			/* Invoke the output consumer callback */` |
|    19506 |  9913 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19506 |  9914 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19506 |  9915 | `			SyBlobRelease(&pOut->sBlob);` |
|    19506 |  9916 | `			if( rc == SXERR_ABORT ){` |
|        - |  9917 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9918 | `				goto Abort;` |
|        - |  9919 | `			}` |
|     9752 |  9920 | `		}` |
|    31920 |  9921 | `		pOut++;` |
|        2 |  9922 | `	}` |
|    31878 |  9923 | `	pTos = &pCur[-1];` |
|    31876 |  9924 | `	break;` |
|        - |  9925 | `					 }` |
|        - |  9926 |  |
|        - |  9927 | `		} /* Switch() */` |
| 11764120 |  9928 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9929 | `	} /* For(;;) */` |
|    22159 |  9930 | `Done:` |
|    44320 |  9931 | `	SySetRelease(&aArg);` |
|    44320 |  9932 | `	return SXRET_OK;` |
|       72 |  9933 | `Suspend:` |
|      146 |  9934 | `	SySetRelease(&aArg);` |
|      146 |  9935 | `	return PH7_SUSPEND;` |
|      280 |  9936 | `Abort:` |
|      561 |  9937 | `	SySetRelease(&aArg);` |
|     1875 |  9938 | `	while( pTos >= pStack ){` |
|     1315 |  9939 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9940 | `		pTos--;` |
|        1 |  9941 | `	}` |
|      561 |  9942 | `	return PH7_ABORT;` |
|       29 |  9943 | `Exception:` |
|       60 |  9944 | `	SySetRelease(&aArg);` |
|      112 |  9945 | `	while( pTos >= pStack ){` |
|       54 |  9946 | `		PH7_MemObjRelease(pTos);` |
|       54 |  9947 | `		pTos--;` |
|        2 |  9948 | `	}` |
|       60 |  9949 | `	return PH7_EXCEPTION;` |
|    22542 |  9950 |  |
|        - |  9951 | `/*` |
|        - |  9952 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9953 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9954 | ` * See block-comment on that function for additional information.` |
|        - |  9955 | ` */` |
|    20582 |  9956 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9957 |  |
|        - |  9958 | `	ph7_value *pStack;` |
|        - |  9959 | `	sxi32 rc;` |
|        - |  9960 | `	/* Allocate a new operand stack */` |
|    20584 |  9961 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20584 |  9962 | `	if( pStack == 0 ){` |
|      ! 0 |  9963 | `		return SXERR_MEM;` |
|        - |  9964 | `	}` |
|        - |  9965 | `	/* Execute the program */` |
|    20584 |  9966 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9967 | `	/* Free the operand stack */` |
|    20584 |  9968 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9969 | `	/* Execution result */` |
|    20584 |  9970 | `	return rc;` |
|    10293 |  9971 |  |
|        - |  9972 | `/*` |
|        - |  9973 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9974 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9975 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9976 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9977 | ` * execution ends.` |
|        - |  9978 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9979 | ` * additional information.` |
|        - |  9980 | ` */` |
|     2832 |  9981 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9982 |  |
|        - |  9983 | `	VmShutdownCB *pEntry;` |
|        - |  9984 | `	ph7_value *apArg[10];` |
|        - |  9985 | `	sxu32 n,nEntry;` |
|        - |  9986 | `	int i;` |
|        - |  9987 | `	/* Point to the stack of registered callbacks */` |
|     2834 |  9988 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31154 |  9989 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28322 |  9990 | `		apArg[i] = 0;` |
|    14162 |  9991 | `	}` |
|        - |  9992 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - |  9993 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - |  9994 | `	 * callbacks, mirroring PHP.` |
|        - |  9995 | `	 */` |
|     2834 |  9996 | `	pVm->bHaltRequested = 0;` |
|     2844 |  9997 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 |  9998 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9999 | `		if( pEntry ){` |
|        - | 10000 | `			/* Prepare callback arguments if any */` |
|       12 | 10001 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10002 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10003 | `					break;` |
|        - | 10004 | `				}` |
|      ! 0 | 10005 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10006 | `			}` |
|        - | 10007 | `			/* Invoke the callback */` |
|       12 | 10008 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10009 | `			/*` |
|        - | 10010 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10011 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10012 | `			 */` |
|       12 | 10013 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 | 10014 | `			if( pEntry ){` |
|       12 | 10015 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 | 10016 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10017 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10018 | `				}` |
|        5 | 10019 | `			}` |
|       12 | 10020 | `			if( pVm->bHaltRequested ){` |
|        - | 10021 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10022 | `				break;` |
|        - | 10023 | `			}` |
|        5 | 10024 | `		}` |
|        7 | 10025 | `	}` |
|     2834 | 10026 | `	SySetReset(&pVm->aShutdown);` |
|     2834 | 10027 |  |
|        - | 10028 | `/*` |
|        - | 10029 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10030 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10031 | ` * See block-comment on that function for additional information.` |
|        - | 10032 | ` */` |
|     2832 | 10033 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 | 10034 |  |
|        - | 10035 | `	/* Make sure we are ready to execute this program */` |
|     2834 | 10036 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10037 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10038 | `	}` |
|        - | 10039 | `	/* Set the execution magic number  */` |
|     2834 | 10040 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10041 | `	/* Execute the program */` |
|     2834 | 10042 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - | 10043 | `	/* Invoke any shutdown callbacks */` |
|     2834 | 10044 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10045 | `	/*` |
|        - | 10046 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10047 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10048 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10049 | `	 */` |
|     2834 | 10050 | `	return SXRET_OK;` |
|     1418 | 10051 |  |
|        - | 10052 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10053 | `/*` |
|        - | 10054 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10055 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10056 | ` */` |
|       46 | 10057 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 | 10058 |  |
|        - | 10059 | `	ph7_exec_ctx *pCtx;` |
|        - | 10060 | `	ph7_value *pStack;` |
|        - | 10061 | `	VmFrame *pFrame;` |
|       48 | 10062 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 | 10063 | `	if( pCtx == 0 ){` |
|      ! 0 | 10064 | `		return 0;` |
|        - | 10065 | `	}` |
|       48 | 10066 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 | 10067 | `	pCtx->pVm = pVm;` |
|       48 | 10068 | `	pCtx->pFunc = pFunc;` |
|       48 | 10069 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 | 10070 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 | 10071 | `	pCtx->pc = 0;` |
|       48 | 10072 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 | 10073 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10074 | `	/* Allocate a private operand stack */` |
|       48 | 10075 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 | 10076 | `	if( pStack == 0 ){` |
|      ! 0 | 10077 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10078 | `		return 0;` |
|        - | 10079 | `	}` |
|       48 | 10080 | `	pCtx->pStack = pStack;` |
|        - | 10081 | `	/* Create a detached frame for the fiber */` |
|       48 | 10082 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 | 10083 | `	if( pFrame == 0 ){` |
|      ! 0 | 10084 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10085 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10086 | `		return 0;` |
|        - | 10087 | `	}` |
|       48 | 10088 | `	pCtx->pFrame = pFrame;` |
|       48 | 10089 | `	return pCtx;` |
|       25 | 10090 |  |
|        - | 10091 | `/*` |
|        - | 10092 | ` * Start executing a fiber context for the first time.` |
|        - | 10093 | ` */` |
|       46 | 10094 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 | 10095 |  |
|        - | 10096 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10097 | `	sxi32 rc;` |
|       48 | 10098 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10099 | `		return SXERR_INVALID;` |
|        - | 10100 | `	}` |
|        - | 10101 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 | 10102 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 | 10103 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10104 | `	/* Save and set the active context */` |
|       48 | 10105 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 | 10106 | `	pVm->pActiveCtx = pCtx;` |
|       48 | 10107 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 | 10108 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 | 10109 | `	pVm->nRecursionDepth++;` |
|        - | 10110 | `	/* Execute from the beginning */` |
|       48 | 10111 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 | 10112 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 | 10113 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 | 10114 | `	pVm->nRecursionDepth--;` |
|        - | 10115 | `	/* Restore the previous context */` |
|       48 | 10116 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 | 10117 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10118 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 | 10119 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 | 10120 | `		pCtx->pFrame->pParent = 0;` |
|       46 | 10121 | `		if( pResult ){` |
|       24 | 10122 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10123 | `		}` |
|       46 | 10124 | `		return SXRET_OK;` |
|        - | 10125 | `	}` |
|        - | 10126 | `	/* Detach frame */` |
|        3 | 10127 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 | 10128 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 | 10129 | `		pCtx->pFrame->pParent = 0;` |
|        1 | 10130 | `	}` |
|        3 | 10131 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10132 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10133 | `		return PH7_ABORT;` |
|        - | 10134 | `	}` |
|        3 | 10135 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10136 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10137 | `		return PH7_EXCEPTION;` |
|        - | 10138 | `	}` |
|        - | 10139 | `	/* Normal completion */` |
|        3 | 10140 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 | 10141 | `	if( pResult ){` |
|        3 | 10142 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10143 | `	}` |
|        3 | 10144 | `	return SXRET_OK;` |
|       25 | 10145 |  |
|        - | 10146 | `/*` |
|        - | 10147 | ` * Resume a suspended fiber context.` |
|        - | 10148 | ` */` |
|       98 | 10149 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 | 10150 |  |
|        - | 10151 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10152 | `	sxi32 rc;` |
|      100 | 10153 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10154 | `		return SXERR_INVALID;` |
|        - | 10155 | `	}` |
|        - | 10156 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10157 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10158 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 | 10159 | `	if( pResumeValue ){` |
|       40 | 10160 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 | 10161 | `	}else{` |
|       62 | 10162 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10163 | `	}` |
|      100 | 10164 | `	pCtx->nTos++;` |
|        - | 10165 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 | 10166 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 | 10167 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10168 | `	/* Save and set the active context */` |
|      100 | 10169 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 | 10170 | `	pVm->pActiveCtx = pCtx;` |
|      100 | 10171 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 | 10172 | `	pVm->nRecursionDepth++;` |
|        - | 10173 | `	/* Resume execution from saved PC */` |
|      100 | 10174 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 | 10175 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 | 10176 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 | 10177 | `	pVm->nRecursionDepth--;` |
|        - | 10178 | `	/* Restore the previous context */` |
|      100 | 10179 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 | 10180 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10181 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 | 10182 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 | 10183 | `		pCtx->pFrame->pParent = 0;` |
|       64 | 10184 | `		if( pResult ){` |
|       18 | 10185 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10186 | `		}` |
|       64 | 10187 | `		return SXRET_OK;` |
|        - | 10188 | `	}` |
|        - | 10189 | `	/* Detach frame */` |
|       38 | 10190 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 | 10191 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 | 10192 | `		pCtx->pFrame->pParent = 0;` |
|       18 | 10193 | `	}` |
|       38 | 10194 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10195 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10196 | `		return PH7_ABORT;` |
|        - | 10197 | `	}` |
|       38 | 10198 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10199 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10200 | `		return PH7_EXCEPTION;` |
|        - | 10201 | `	}` |
|        - | 10202 | `	/* Normal completion */` |
|       38 | 10203 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 | 10204 | `	if( pResult ){` |
|       20 | 10205 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10206 | `	}` |
|       38 | 10207 | `	return SXRET_OK;` |
|       51 | 10208 |  |
|        - | 10209 | `/*` |
|        - | 10210 | ` * Release an execution context and all its resources.` |
|        - | 10211 | ` */` |
|        4 | 10212 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10213 |  |
|        5 | 10214 | `	if( pCtx == 0 ){` |
|      ! 0 | 10215 | `		return;` |
|        - | 10216 | `	}` |
|        5 | 10217 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10218 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10219 | `		return;` |
|        - | 10220 | `	}` |
|        5 | 10221 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10222 | `	/* Release values */` |
|        5 | 10223 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10224 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10225 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10226 | `	if( pCtx->pFrame ){` |
|        - | 10227 | `		VmSlot *aSlot;` |
|        - | 10228 | `		sxu32 n;` |
|        - | 10229 | `		/* Free local variables */` |
|        5 | 10230 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10231 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10232 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10233 | `		}` |
|        - | 10234 | `		/* Remove local references */` |
|        5 | 10235 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10236 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10237 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10238 | `		}` |
|        5 | 10239 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10240 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10241 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10242 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10243 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10244 | `		pCtx->pFrame = 0;` |
|        2 | 10245 | `	}` |
|        - | 10246 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10247 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10248 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10249 | `	if( pCtx->pStack ){` |
|        5 | 10250 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10251 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10252 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10253 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10254 | `				pTos--;` |
|        1 | 10255 | `			}` |
|        2 | 10256 | `		}` |
|        5 | 10257 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10258 | `		pCtx->pStack = 0;` |
|        2 | 10259 | `	}` |
|        - | 10260 | `	/* Free the context itself */` |
|        5 | 10261 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10262 |  |
|        - | 10263 | `/*` |
|        - | 10264 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10265 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10266 | ` */` |
|       90 | 10267 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 | 10268 |  |
|        - | 10269 | `	ph7_class_instance *pThis;` |
|        - | 10270 | `	SyString sAttr;` |
|        - | 10271 | `	ph7_value *pAttr;` |
|       92 | 10272 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10273 | `		return 0;` |
|        - | 10274 | `	}` |
|       92 | 10275 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 | 10276 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10277 | `		return 0;` |
|        - | 10278 | `	}` |
|       92 | 10279 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 | 10280 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 | 10281 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 | 10282 | `		return 0;` |
|        - | 10283 | `	}` |
|       62 | 10284 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 | 10285 |  |
|        - | 10286 | `/*` |
|        - | 10287 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10288 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10289 | ` */` |
|       38 | 10290 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10291 |  |
|       40 | 10292 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 | 10293 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10294 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10295 | `			"Cannot suspend outside of a fiber");` |
|        - | 10296 | `	}` |
|       40 | 10297 | `	if( nArg > 0 ){` |
|       40 | 10298 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10299 | `	}else{` |
|      ! 0 | 10300 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10301 | `	}` |
|       40 | 10302 | `	return PH7_SUSPEND;` |
|       21 | 10303 |  |
|        - | 10304 | `/*` |
|        - | 10305 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10306 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10307 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10308 | ` */` |
|       24 | 10309 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10310 |  |
|        - | 10311 | `	ph7_class_instance *pThis;` |
|        - | 10312 | `	ph7_value *pAttr;` |
|        - | 10313 | `	SyString sAttrName;` |
|       26 | 10314 | `	if( nArg < 2 ){` |
|      ! 0 | 10315 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10316 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10317 | `	}` |
|       26 | 10318 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10319 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10320 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10321 | `	}` |
|       26 | 10322 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10323 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10324 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10325 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10326 | `	}` |
|        - | 10327 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10328 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10329 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10330 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10331 | `	}` |
|        - | 10332 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10333 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10334 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10335 | `	if( pAttr ){` |
|       26 | 10336 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10337 | `	}` |
|       26 | 10338 | `	return PH7_OK;` |
|       14 | 10339 |  |
|        - | 10340 | `/*` |
|        - | 10341 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10342 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10343 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10344 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10345 | ` */` |
|       24 | 10346 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10347 | `	ph7_class_instance **ppThis)` |
|        2 | 10348 |  |
|       26 | 10349 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10350 | `	ph7_value *pCallable;` |
|        - | 10351 | `	SyString sAttrName;` |
|       26 | 10352 | `	*ppThis = 0;` |
|       26 | 10353 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10354 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10355 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10356 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10357 | `		return 0;` |
|        - | 10358 | `	}` |
|       26 | 10359 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10360 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10361 | `		SyString sName;` |
|        - | 10362 | `		SyHashEntry *pEntry;` |
|        - | 10363 | `		ph7_vm_func *pFunc;` |
|       26 | 10364 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10365 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10366 | `		if( pEntry == 0 ){` |
|      ! 0 | 10367 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10368 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10369 | `			return 0;` |
|        - | 10370 | `		}` |
|       26 | 10371 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10372 | `		return pFunc;` |
|      ! 0 | 10373 | `	}else{` |
|        - | 10374 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10375 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10376 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10377 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10378 | `		if( pMethod == 0 ){` |
|      ! 0 | 10379 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10380 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10381 | `			return 0;` |
|        - | 10382 | `		}` |
|      ! 0 | 10383 | `		*ppThis = pClosure;` |
|      ! 0 | 10384 | `		return &pMethod->sFunc;` |
|        - | 10385 | `	}` |
|       14 | 10386 |  |
|        - | 10387 | `/*` |
|        - | 10388 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10389 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10390 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10391 | ` */` |
|       46 | 10392 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10393 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10394 |  |
|       48 | 10395 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10396 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10397 | `	sxu32 nFormal, n;` |
|        - | 10398 | `	VmSlot sSlot;` |
|        - | 10399 | `	sxi32 rc;` |
|        - | 10400 | `	/* Install $this for closure/method callables */` |
|       48 | 10401 | `	if( pClosureThis ){` |
|        - | 10402 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10403 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10404 | `		if( pObj ){` |
|      ! 0 | 10405 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10406 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10407 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10408 | `		}` |
|      ! 0 | 10409 | `	}` |
|        - | 10410 | `	/* Install static variables */` |
|       48 | 10411 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10412 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10413 | `		ph7_value *pVal;` |
|      ! 0 | 10414 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10415 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10416 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10417 | `			if( pVal ){` |
|      ! 0 | 10418 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10419 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10420 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10421 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10422 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10423 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10424 | `				}` |
|      ! 0 | 10425 | `			}` |
|      ! 0 | 10426 | `		}` |
|      ! 0 | 10427 | `	}` |
|        - | 10428 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10429 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10430 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10431 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10432 | `		ph7_value *pObj;` |
|       20 | 10433 | `		if( n < (sxu32)nArg ){` |
|        - | 10434 | `			/* Argument provided — install with type casting */` |
|       20 | 10435 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10436 | `			if( pObj ){` |
|       20 | 10437 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10438 | `				/* Type casting */` |
|       20 | 10439 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10440 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10441 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10442 | `						if( xCast ){` |
|      ! 0 | 10443 | `							xCast(pObj);` |
|      ! 0 | 10444 | `						}` |
|      ! 0 | 10445 | `					}` |
|      ! 0 | 10446 | `				}` |
|       20 | 10447 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10448 | `				sSlot.pUserData = 0;` |
|       20 | 10449 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10450 | `			}` |
|        9 | 10451 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10452 | `			/* Default value */` |
|      ! 0 | 10453 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10454 | `			if( pObj ){` |
|      ! 0 | 10455 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10456 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10457 | `					return rc;` |
|        - | 10458 | `				}` |
|      ! 0 | 10459 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10460 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10461 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10462 | `						if( xCast ){` |
|      ! 0 | 10463 | `							xCast(pObj);` |
|      ! 0 | 10464 | `						}` |
|      ! 0 | 10465 | `					}` |
|      ! 0 | 10466 | `				}` |
|      ! 0 | 10467 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10468 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10469 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10470 | `			}` |
|      ! 0 | 10471 | `		}` |
|       11 | 10472 | `	}` |
|        - | 10473 | `	/* Install closure environment (captured variables) */` |
|       48 | 10474 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10475 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10476 | `		ph7_value *pValue;` |
|        - | 10477 | `		sxu32 iEnv;` |
|        3 | 10478 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10479 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10480 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10481 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10482 | `				continue;` |
|        - | 10483 | `			}` |
|        5 | 10484 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10485 | `			if( pValue == 0 ){` |
|      ! 0 | 10486 | `				continue;` |
|        - | 10487 | `			}` |
|        5 | 10488 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10489 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10490 | `		}` |
|        1 | 10491 | `	}` |
|       48 | 10492 | `	return SXRET_OK;` |
|       25 | 10493 |  |
|        - | 10494 | `/*` |
|        - | 10495 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10496 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10497 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10498 | ` */` |
|       26 | 10499 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10500 |  |
|       28 | 10501 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10502 | `	ph7_class_instance *pThis;` |
|        - | 10503 | `	ph7_class_instance *pClosureThis;` |
|        - | 10504 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10505 | `	ph7_vm_func *pFunc;` |
|        - | 10506 | `	ph7_value sResult;` |
|        - | 10507 | `	ph7_value *pCtxAttr;` |
|        - | 10508 | `	SyString sAttrName;` |
|        - | 10509 | `	sxi32 rc;` |
|       28 | 10510 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10511 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10512 | `	}` |
|       28 | 10513 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10514 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10515 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10516 | `	if( pExecCtx != 0 ){` |
|        3 | 10517 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10518 | `			"Cannot start a fiber that has already been started");` |
|        - | 10519 | `	}` |
|        - | 10520 | `	/* Resolve callable */` |
|       26 | 10521 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10522 | `	if( pFunc == 0 ){` |
|      ! 0 | 10523 | `		return PH7_EXCEPTION;` |
|        - | 10524 | `	}` |
|        - | 10525 | `	/* Create execution context now that we know the function */` |
|       26 | 10526 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10527 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10528 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10529 | `			"Fiber::start(): out of memory");` |
|        - | 10530 | `	}` |
|        - | 10531 | `	/* Store context in $this->__ctx */` |
|       26 | 10532 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10533 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10534 | `	if( pCtxAttr ){` |
|       26 | 10535 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10536 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10537 | `	}` |
|        - | 10538 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10539 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10540 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10541 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10542 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10543 | `	/* Unpack the args array and install into the frame */` |
|        - | 10544 | `	{` |
|       26 | 10545 | `		ph7_value **apValues = 0;` |
|       26 | 10546 | `		int nActual = 0;` |
|       26 | 10547 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10548 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10549 | `			ph7_hashmap_node *pNode;` |
|       26 | 10550 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10551 | `			if( nCount > 0 ){` |
|        3 | 10552 | `				sxu32 idx = 0;` |
|        4 | 10553 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10554 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10555 | `				if( apValues ){` |
|        3 | 10556 | `					pNode = pMap->pFirst;` |
|        7 | 10557 | `					while( pNode && idx < nCount ){` |
|        5 | 10558 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10559 | `						idx++;` |
|        5 | 10560 | `						pNode = pNode->pPrev;` |
|        1 | 10561 | `					}` |
|        3 | 10562 | `					nActual = (int)idx;` |
|        1 | 10563 | `				}` |
|        1 | 10564 | `			}` |
|       12 | 10565 | `		}` |
|       26 | 10566 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10567 | `		if( apValues ){` |
|        3 | 10568 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10569 | `		}` |
|        - | 10570 | `	}` |
|        - | 10571 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10572 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10573 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10574 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10575 | `		return PH7_ABORT;` |
|        - | 10576 | `	}` |
|       26 | 10577 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10578 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10579 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10580 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10581 | `		return PH7_ABORT;` |
|        - | 10582 | `	}` |
|       26 | 10583 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10584 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10585 | `		return PH7_EXCEPTION;` |
|        - | 10586 | `	}` |
|       26 | 10587 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10588 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10589 | `	return PH7_OK;` |
|       15 | 10590 |  |
|        - | 10591 | `/*` |
|        - | 10592 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10593 | ` */` |
|       36 | 10594 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10595 |  |
|       38 | 10596 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10597 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10598 | `	ph7_value sResult;` |
|        - | 10599 | `	ph7_value *pResumeVal;` |
|        - | 10600 | `	sxi32 rc;` |
|       38 | 10601 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10602 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10603 | `		return PH7_OK;` |
|        - | 10604 | `	}` |
|       38 | 10605 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10606 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10607 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10608 | `		return PH7_OK;` |
|        - | 10609 | `	}` |
|       38 | 10610 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10611 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10612 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10613 | `	}` |
|       36 | 10614 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10615 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10616 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10617 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10618 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10619 | `		return PH7_ABORT;` |
|        - | 10620 | `	}` |
|       36 | 10621 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10622 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10623 | `		return PH7_EXCEPTION;` |
|        - | 10624 | `	}` |
|       36 | 10625 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10626 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10627 | `	return PH7_OK;` |
|       20 | 10628 |  |
|        - | 10629 | `/*` |
|        - | 10630 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10631 | ` */` |
|        6 | 10632 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10633 |  |
|        8 | 10634 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10635 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10636 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10637 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10638 | `		return PH7_OK;` |
|        - | 10639 | `	}` |
|        8 | 10640 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10641 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10642 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10643 | `		return PH7_OK;` |
|        - | 10644 | `	}` |
|        8 | 10645 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10646 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10647 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10648 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10649 | `		}` |
|      ! 0 | 10650 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10651 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10652 | `	}` |
|        8 | 10653 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10654 | `	return PH7_OK;` |
|        5 | 10655 |  |
|        - | 10656 | `/*` |
|        - | 10657 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10658 | ` */` |
|        6 | 10659 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10660 |  |
|        - | 10661 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10662 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10663 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10664 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10665 | `	return PH7_OK;` |
|        4 | 10666 |  |
|      ! 0 | 10667 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10668 |  |
|        - | 10669 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10670 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10671 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10672 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10673 | `	return PH7_OK;` |
|      ! 0 | 10674 |  |
|        6 | 10675 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10676 |  |
|        - | 10677 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10678 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10679 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10680 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10681 | `	return PH7_OK;` |
|        4 | 10682 |  |
|        6 | 10683 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10684 |  |
|        - | 10685 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10686 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10687 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10688 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10689 | `	return PH7_OK;` |
|        4 | 10690 |  |
|        - | 10691 | `/*` |
|        - | 10692 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10693 | ` */` |
|        4 | 10694 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10695 |  |
|        5 | 10696 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10697 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10698 | `	if( nArg < 1 ){` |
|      ! 0 | 10699 | `		return PH7_OK;` |
|        - | 10700 | `	}` |
|        5 | 10701 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10702 | `	if( pExecCtx ){` |
|        5 | 10703 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10704 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10705 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10706 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10707 | `			SyString sAttrName;` |
|        - | 10708 | `			ph7_value *pAttr;` |
|        5 | 10709 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10710 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10711 | `			if( pAttr ){` |
|        5 | 10712 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10713 | `			}` |
|        2 | 10714 | `		}` |
|        2 | 10715 | `	}` |
|        5 | 10716 | `	return PH7_OK;` |
|        3 | 10717 |  |
|        - | 10718 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10719 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10720 |  |
|        - | 10721 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10722 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10723 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10724 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10725 |  |
|      ! 0 | 10726 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10727 |  |
|        - | 10728 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10729 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10730 | `	ph7_exec_ctx *pCtx;` |
|        - | 10731 | `	ph7_vm_func *pFunc;` |
|        - | 10732 | `	ph7_value *pCallable;` |
|        - | 10733 | `	ph7_value *pCtxAttr;` |
|        - | 10734 | `	SyString sAttrName;` |
|        - | 10735 | `	/* Must not already be started */` |
|      ! 0 | 10736 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10737 | `	if( pCtx != 0 ){` |
|      ! 0 | 10738 | `		return SXERR_INVALID;` |
|        - | 10739 | `	}` |
|      ! 0 | 10740 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10741 | `		return SXERR_INVALID;` |
|        - | 10742 | `	}` |
|      ! 0 | 10743 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10744 | `	/* Get the callable */` |
|      ! 0 | 10745 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10746 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10747 | `	if( pCallable == 0 ){` |
|      ! 0 | 10748 | `		return SXERR_INVALID;` |
|        - | 10749 | `	}` |
|        - | 10750 | `	/* Resolve callable */` |
|      ! 0 | 10751 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10752 | `		SyString sName;` |
|        - | 10753 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10754 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10755 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10756 | `		if( pEntry == 0 ){` |
|      ! 0 | 10757 | `			return SXERR_NOTFOUND;` |
|        - | 10758 | `		}` |
|      ! 0 | 10759 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10760 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10761 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10762 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10763 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10764 | `		if( pMethod == 0 ){` |
|      ! 0 | 10765 | `			return SXERR_INVALID;` |
|        - | 10766 | `		}` |
|      ! 0 | 10767 | `		pClosureThis = pClosure;` |
|      ! 0 | 10768 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10769 | `	}else{` |
|      ! 0 | 10770 | `		return SXERR_INVALID;` |
|        - | 10771 | `	}` |
|        - | 10772 | `	/* Create context */` |
|      ! 0 | 10773 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10774 | `	if( pCtx == 0 ){` |
|      ! 0 | 10775 | `		return SXERR_MEM;` |
|        - | 10776 | `	}` |
|        - | 10777 | `	/* Store in __ctx */` |
|      ! 0 | 10778 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10779 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10780 | `	if( pCtxAttr ){` |
|      ! 0 | 10781 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10782 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10783 | `	}` |
|        - | 10784 | `	/* Set up frame with args */` |
|      ! 0 | 10785 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10786 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10787 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10788 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10789 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10790 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10791 |  |
|      ! 0 | 10792 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10793 |  |
|      ! 0 | 10794 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10795 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10796 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10797 |  |
|      ! 0 | 10798 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10799 |  |
|      ! 0 | 10800 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10801 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10802 |  |
|      ! 0 | 10803 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10804 |  |
|      ! 0 | 10805 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10806 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10807 |  |
|      ! 0 | 10808 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10809 |  |
|      ! 0 | 10810 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10811 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10812 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10813 |  |
|        - | 10814 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10815 | `/*` |
|        - | 10816 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10817 | ` */` |
|       22 | 10818 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10819 |  |
|        - | 10820 | `	ph7_generator *pGen;` |
|       24 | 10821 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10822 | `	if( pGen == 0 ){` |
|      ! 0 | 10823 | `		return 0;` |
|        - | 10824 | `	}` |
|       24 | 10825 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10826 | `	pGen->pCtx = pCtx;` |
|       24 | 10827 | `	pGen->iImplicitKey = 0;` |
|       24 | 10828 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10829 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10830 | `	/* Link the generator back to the exec context */` |
|       24 | 10831 | `	pCtx->pPrivate = pGen;` |
|       24 | 10832 | `	return pGen;` |
|       13 | 10833 |  |
|        - | 10834 | `/*` |
|        - | 10835 | ` * Release a generator and its execution context.` |
|        - | 10836 | ` */` |
|      ! 0 | 10837 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10838 |  |
|      ! 0 | 10839 | `	if( pGen == 0 ){` |
|      ! 0 | 10840 | `		return;` |
|        - | 10841 | `	}` |
|      ! 0 | 10842 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10843 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10844 | `	if( pGen->pCtx ){` |
|      ! 0 | 10845 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10846 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10847 | `		pGen->pCtx = 0;` |
|      ! 0 | 10848 | `	}` |
|      ! 0 | 10849 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10850 |  |
|        - | 10851 | `/*` |
|        - | 10852 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10853 | ` */` |
|      236 | 10854 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10855 |  |
|        - | 10856 | `	ph7_class_instance *pThis;` |
|        - | 10857 | `	SyString sAttr;` |
|        - | 10858 | `	ph7_value *pAttr;` |
|      238 | 10859 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10860 | `		return 0;` |
|        - | 10861 | `	}` |
|      238 | 10862 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10863 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10864 | `		return 0;` |
|        - | 10865 | `	}` |
|      238 | 10866 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10867 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10868 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10869 | `		return 0;` |
|        - | 10870 | `	}` |
|      238 | 10871 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10872 |  |
|        - | 10873 | `/*` |
|        - | 10874 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10875 | ` */` |
|       22 | 10876 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10877 |  |
|        - | 10878 | `	ph7_generator *pGen;` |
|        - | 10879 | `	sxi32 rc;` |
|       24 | 10880 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10881 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10882 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10883 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10884 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10885 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10886 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10887 | `	}` |
|       24 | 10888 | `	return PH7_OK;` |
|       13 | 10889 |  |
|        - | 10890 | `/*` |
|        - | 10891 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10892 | ` */` |
|       68 | 10893 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10894 |  |
|        - | 10895 | `	ph7_generator *pGen;` |
|       70 | 10896 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10897 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10898 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10899 | `	return PH7_OK;` |
|       36 | 10900 |  |
|        - | 10901 | `/*` |
|        - | 10902 | ` * Generator::current() — return the last yielded value.` |
|        - | 10903 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10904 | ` */` |
|       68 | 10905 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10906 |  |
|        - | 10907 | `	ph7_generator *pGen;` |
|        - | 10908 | `	sxi32 rc;` |
|       70 | 10909 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10910 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10911 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10912 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10913 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10914 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10915 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10916 | `	}` |
|       70 | 10917 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10918 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10919 | `	}else{` |
|      ! 0 | 10920 | `		ph7_result_null(pCtx);` |
|        - | 10921 | `	}` |
|       70 | 10922 | `	return PH7_OK;` |
|       36 | 10923 |  |
|        - | 10924 | `/*` |
|        - | 10925 | ` * Generator::key() — return the last yielded key.` |
|        - | 10926 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10927 | ` */` |
|       12 | 10928 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10929 |  |
|        - | 10930 | `	ph7_generator *pGen;` |
|        - | 10931 | `	sxi32 rc;` |
|       13 | 10932 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10933 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10934 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10935 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10936 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10937 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10938 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10939 | `	}` |
|       13 | 10940 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10941 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10942 | `	}else{` |
|      ! 0 | 10943 | `		ph7_result_null(pCtx);` |
|        - | 10944 | `	}` |
|       13 | 10945 | `	return PH7_OK;` |
|        7 | 10946 |  |
|        - | 10947 | `/*` |
|        - | 10948 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10949 | ` */` |
|       60 | 10950 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10951 |  |
|        - | 10952 | `	ph7_generator *pGen;` |
|        - | 10953 | `	sxi32 rc;` |
|       62 | 10954 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10955 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10956 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10957 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10958 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10959 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10960 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10961 | `	}else{` |
|      ! 0 | 10962 | `		return PH7_OK;` |
|        - | 10963 | `	}` |
|       62 | 10964 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10965 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10966 | `	return PH7_OK;` |
|       32 | 10967 |  |
|        - | 10968 | `/*` |
|        - | 10969 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10970 | ` */` |
|        4 | 10971 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10972 |  |
|        - | 10973 | `	ph7_generator *pGen;` |
|        - | 10974 | `	ph7_value *pSendVal;` |
|        - | 10975 | `	sxi32 rc;` |
|        5 | 10976 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10977 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10978 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10979 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10980 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10981 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10982 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10983 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10984 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10985 | `	}else{` |
|      ! 0 | 10986 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10987 | `		return PH7_OK;` |
|        - | 10988 | `	}` |
|        5 | 10989 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10990 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10991 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10992 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10993 | `	}else{` |
|        3 | 10994 | `		ph7_result_null(pCtx);` |
|        - | 10995 | `	}` |
|        5 | 10996 | `	return PH7_OK;` |
|        3 | 10997 |  |
|        - | 10998 | `/*` |
|        - | 10999 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11000 | ` *` |
|        - | 11001 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11002 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11003 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11004 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11005 | ` * the exception to the caller.` |
|        - | 11006 | ` */` |
|      ! 0 | 11007 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11008 |  |
|        - | 11009 | `	ph7_generator *pGen;` |
|        - | 11010 | `	const char *zMsg;` |
|        - | 11011 | `	int nLen;` |
|      ! 0 | 11012 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11013 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11014 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11015 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11016 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11017 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11018 | `			"Cannot throw into a closed generator");` |
|        - | 11019 | `	}` |
|        - | 11020 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11021 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11022 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11023 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11024 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11025 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11026 | `	nLen = 0;` |
|      ! 0 | 11027 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11028 | `		/* Try to get the exception's message */` |
|        - | 11029 | `		SyString sAttr;` |
|        - | 11030 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11031 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11032 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11033 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11034 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11035 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11036 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11037 | `		}` |
|      ! 0 | 11038 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11039 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11040 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11041 | `	}` |
|      ! 0 | 11042 | `	(void)nLen;` |
|      ! 0 | 11043 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11044 |  |
|        - | 11045 | `/*` |
|        - | 11046 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11047 | ` */` |
|        2 | 11048 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11049 |  |
|        - | 11050 | `	ph7_generator *pGen;` |
|        3 | 11051 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11052 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11053 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11054 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11055 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11056 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11057 | `	}` |
|        3 | 11058 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11059 | `	return PH7_OK;` |
|        2 | 11060 |  |
|        - | 11061 | `/*` |
|        - | 11062 | ` * Generator::__destruct() — clean up.` |
|        - | 11063 | ` */` |
|      ! 0 | 11064 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11065 |  |
|        - | 11066 | `	ph7_generator *pGen;` |
|      ! 0 | 11067 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11068 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11069 | `	if( pGen ){` |
|      ! 0 | 11070 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11071 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11072 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11073 | `			SyString sAttrName;` |
|        - | 11074 | `			ph7_value *pAttr;` |
|      ! 0 | 11075 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11076 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11077 | `			if( pAttr ){` |
|      ! 0 | 11078 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11079 | `			}` |
|      ! 0 | 11080 | `		}` |
|      ! 0 | 11081 | `	}` |
|      ! 0 | 11082 | `	return PH7_OK;` |
|      ! 0 | 11083 |  |
|        - | 11084 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11085 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11086 | `/*` |
|        - | 11087 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11088 | ` * the desired message.` |
|        - | 11089 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11090 | ` * in 'api.c' for additional information.` |
|        - | 11091 | ` */` |
|      370 | 11092 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11093 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11094 | `	SyString *pString /* Message to output */` |
|        - | 11095 | `	)` |
|        2 | 11096 |  |
|      372 | 11097 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 11098 | `	sxi32 rc = SXRET_OK;` |
|        - | 11099 | `	/* Call the output consumer */` |
|      372 | 11100 | `	if( pString->nByte > 0 ){` |
|      372 | 11101 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 11102 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11103 | `	}` |
|      372 | 11104 | `	return rc;` |
|        2 | 11105 |  |
|        - | 11106 | `/*` |
|        - | 11107 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11108 | ` * callback to consume the formatted message.` |
|        - | 11109 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11110 | ` * in 'api.c' for additional information.` |
|        - | 11111 | ` */` |
|        2 | 11112 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11113 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11114 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11115 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11116 | `	)` |
|        1 | 11117 |  |
|        3 | 11118 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11119 | `	sxi32 rc = SXRET_OK;` |
|        - | 11120 | `	SyBlob sWorker;` |
|        - | 11121 | `	/* Format the message and call the output consumer */` |
|        3 | 11122 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11123 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11124 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11125 | `		/* Consume the formatted message */` |
|        3 | 11126 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11127 | `	}` |
|        3 | 11128 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11129 | `	/* Release the working buffer */` |
|        3 | 11130 | `	SyBlobRelease(&sWorker);` |
|        3 | 11131 | `	return rc;` |
|        1 | 11132 |  |
|        - | 11133 | `/*` |
|        - | 11134 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11135 | ` * This function never fail and always return a pointer` |
|        - | 11136 | ` * to a null terminated string.` |
|        - | 11137 | ` */` |
|       12 | 11138 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11139 |  |
|       13 | 11140 | `	const char *zOp = "Unknown     ";` |
|       13 | 11141 | `	switch(nOp){` |
|        3 | 11142 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11143 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11144 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11145 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11146 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11147 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11148 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11149 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11150 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11151 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11152 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11153 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11154 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11155 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11156 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11157 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11158 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11159 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11160 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11161 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11162 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11163 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11164 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11165 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11166 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11167 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11168 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11169 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11170 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11171 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11172 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11173 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11174 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11175 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11176 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11177 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11178 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11179 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11180 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11181 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11182 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11183 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11184 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11185 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11186 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11187 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11188 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11189 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11190 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11191 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11192 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11193 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11194 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11195 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11196 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11197 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11198 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11199 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11200 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11201 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11202 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11203 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11204 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11205 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11206 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11207 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11208 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11209 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11210 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11211 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11212 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11213 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11214 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11215 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11216 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11217 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11218 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11219 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11220 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11221 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11222 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11223 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11224 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11225 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11226 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11227 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11228 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11229 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11230 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11231 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11232 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11233 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11234 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11235 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11236 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11237 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11238 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11239 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11240 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11241 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11242 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11243 | `	default:` |
|      ! 0 | 11244 | `		break;` |
|        - | 11245 | `	}` |
|       13 | 11246 | `	return zOp;` |
|        1 | 11247 |  |
|        - | 11248 | `/*` |
|        - | 11249 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11250 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11251 | ` * is responsible of consuming the generated dump.` |
|        - | 11252 | ` */` |
|        2 | 11253 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11254 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11255 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11256 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11257 | `	)` |
|        1 | 11258 |  |
|        - | 11259 | `	sxi32 rc;` |
|        3 | 11260 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11261 | `	return rc;` |
|        1 | 11262 |  |
|        - | 11263 | `/*` |
|        - | 11264 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11265 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11266 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11267 | ` * in 'compile.c' for additional information.` |
|        - | 11268 | ` */` |
|       14 | 11269 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11270 |  |
|       15 | 11271 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11272 | `	/* Evaluate and expand constant value */` |
|       15 | 11273 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 11274 |  |
|        - | 11275 | `/*` |
|        - | 11276 | ` * Section:` |
|        - | 11277 | ` *  Function handling functions.` |
|        - | 11278 | ` * Status:` |
|        - | 11279 | ` *    Stable.` |
|        - | 11280 | ` */` |
|        - | 11281 | `/*` |
|        - | 11282 | ` * int func_num_args(void)` |
|        - | 11283 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11284 | ` * Parameters` |
|        - | 11285 | ` *   None.` |
|        - | 11286 | ` * Return` |
|        - | 11287 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11288 | ` *  or -1 if called from the globe scope.` |
|        - | 11289 | ` */` |
|      980 | 11290 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11291 |  |
|        - | 11292 | `	VmFrame *pFrame;` |
|        - | 11293 | `	ph7_vm *pVm;` |
|        - | 11294 | `	/* Point to the target VM */` |
|      982 | 11295 | `	pVm = pCtx->pVm;` |
|        - | 11296 | `	/* Current frame */` |
|      982 | 11297 | `	pFrame = pVm->pFrame;` |
|      982 | 11298 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 11299 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11300 | `		SXUNUSED(nArg);` |
|      ! 0 | 11301 | `		SXUNUSED(apArg);` |
|        - | 11302 | `		/* Global frame,return -1 */` |
|      ! 0 | 11303 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11304 | `		return SXRET_OK;` |
|        - | 11305 | `	}` |
|        - | 11306 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 11307 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 11308 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 11309 | `	return SXRET_OK;` |
|      492 | 11310 |  |
|        - | 11311 | `/*` |
|        - | 11312 | ` * value func_get_arg(int $arg_num)` |
|        - | 11313 | ` *   Return an item from the argument list.` |
|        - | 11314 | ` * Parameters` |
|        - | 11315 | ` *  Argument number(index start from zero).` |
|        - | 11316 | ` * Return` |
|        - | 11317 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11318 | ` */` |
|       22 | 11319 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11320 |  |
|       24 | 11321 | `	ph7_value *pObj = 0;` |
|       24 | 11322 | `	VmSlot *pSlot = 0;` |
|        - | 11323 | `	VmFrame *pFrame;` |
|        - | 11324 | `	ph7_vm *pVm;` |
|        - | 11325 | `	/* Point to the target VM */` |
|       24 | 11326 | `	pVm = pCtx->pVm;` |
|        - | 11327 | `	/* Current frame */` |
|       24 | 11328 | `	pFrame = pVm->pFrame;` |
|       24 | 11329 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11330 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11331 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11332 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11333 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11334 | `		return SXRET_OK;` |
|        - | 11335 | `	}` |
|        - | 11336 | `	/* Extract the desired index */` |
|       21 | 11337 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11338 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11339 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11340 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11341 | `		return SXRET_OK;` |
|        - | 11342 | `	}` |
|        - | 11343 | `	/* Extract the desired argument */` |
|       21 | 11344 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11345 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11346 | `			/* Return the desired argument */` |
|       21 | 11347 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11348 | `		}else{` |
|        - | 11349 | `			/* No such argument,return false */` |
|      ! 0 | 11350 | `			ph7_result_bool(pCtx,0);` |
|        - | 11351 | `		}` |
|       11 | 11352 | `	}else{` |
|        - | 11353 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11354 | `		ph7_result_bool(pCtx,0);` |
|        - | 11355 | `	}` |
|       21 | 11356 | `	return SXRET_OK;` |
|       13 | 11357 |  |
|        - | 11358 | `/*` |
|        - | 11359 | ` * array func_get_args_byref(void)` |
|        - | 11360 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11361 | ` * Parameters` |
|        - | 11362 | ` *  None.` |
|        - | 11363 | ` * Return` |
|        - | 11364 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11365 | ` *  member of the current user-defined function's argument list.` |
|        - | 11366 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11367 | ` * NOTE:` |
|        - | 11368 | ` *  Arguments are returned to the array by reference.` |
|        - | 11369 | ` */` |
|        2 | 11370 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11371 |  |
|        - | 11372 | `	ph7_value *pArray;` |
|        - | 11373 | `	VmFrame *pFrame;` |
|        - | 11374 | `	VmSlot *aSlot;` |
|        - | 11375 | `	sxu32 n;` |
|        - | 11376 | `	/* Point to the current frame */` |
|        3 | 11377 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11378 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11379 | `	if( pFrame->pParent == 0 ){` |
|        - | 11380 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11381 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11382 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11383 | `		return SXRET_OK;` |
|        - | 11384 | `	}` |
|        - | 11385 | `	/* Create a new array */` |
|        3 | 11386 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11387 | `	if( pArray == 0 ){` |
|      ! 0 | 11388 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11389 | `		SXUNUSED(apArg);` |
|      ! 0 | 11390 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11391 | `		return SXRET_OK;` |
|        - | 11392 | `	}` |
|        - | 11393 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11394 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11395 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11396 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11397 | `	}` |
|        - | 11398 | `	/* Return the freshly created array */` |
|        3 | 11399 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11400 | `	return SXRET_OK;` |
|        2 | 11401 |  |
|        - | 11402 | `/*` |
|        - | 11403 | ` * array func_get_args(void)` |
|        - | 11404 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11405 | ` * Parameters` |
|        - | 11406 | ` *  None.` |
|        - | 11407 | ` * Return` |
|        - | 11408 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11409 | ` *  member of the current user-defined function's argument list.` |
|        - | 11410 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11411 | ` */` |
|       88 | 11412 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11413 |  |
|       90 | 11414 | `	ph7_value *pObj = 0;` |
|        - | 11415 | `	ph7_value *pArray;` |
|        - | 11416 | `	VmFrame *pFrame;` |
|        - | 11417 | `	VmSlot *aSlot;` |
|        - | 11418 | `	sxu32 n;` |
|        - | 11419 | `	/* Point to the current frame */` |
|       90 | 11420 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11421 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11422 | `	if( pFrame->pParent == 0 ){` |
|        - | 11423 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11424 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11425 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11426 | `		return SXRET_OK;` |
|        - | 11427 | `	}` |
|        - | 11428 | `	/* Create a new array */` |
|       90 | 11429 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11430 | `	if( pArray == 0 ){` |
|      ! 0 | 11431 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11432 | `		SXUNUSED(apArg);` |
|      ! 0 | 11433 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11434 | `		return SXRET_OK;` |
|        - | 11435 | `	}` |
|        - | 11436 | `	/* Start filling the array with the given arguments */` |
|       90 | 11437 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11438 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11439 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11440 | `		if( pObj ){` |
|      134 | 11441 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11442 | `		}` |
|       68 | 11443 | `	}` |
|        - | 11444 | `	/* Return the freshly created array */` |
|       90 | 11445 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11446 | `	return SXRET_OK;` |
|       46 | 11447 |  |
|        - | 11448 | `/*` |
|        - | 11449 | ` * bool function_exists(string $name)` |
|        - | 11450 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11451 | ` * Parameters` |
|        - | 11452 | ` *  The name of the desired function.` |
|        - | 11453 | ` * Return` |
|        - | 11454 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11455 | ` */` |
|     1742 | 11456 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11457 |  |
|        - | 11458 | `	const char *zName;` |
|        - | 11459 | `	ph7_vm *pVm;` |
|        - | 11460 | `	int nLen;` |
|        - | 11461 | `	int res;` |
|     1744 | 11462 | `	if( nArg < 1 ){` |
|        - | 11463 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11464 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11465 | `		return SXRET_OK;` |
|        - | 11466 | `	}` |
|        - | 11467 | `	/* Point to the target VM */` |
|     1744 | 11468 | `	pVm = pCtx->pVm;` |
|        - | 11469 | `	/* Extract the function name */` |
|     1744 | 11470 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11471 | `	/* Assume the function is not defined */` |
|     1744 | 11472 | `	res = 0;` |
|        - | 11473 | `	/* Perform the lookup */` |
|     2613 | 11474 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1738 | 11475 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11476 | `			/* Function is defined */` |
|      266 | 11477 | `			res = 1;` |
|      132 | 11478 | `	}` |
|     1744 | 11479 | `	ph7_result_bool(pCtx,res);` |
|     1744 | 11480 | `	return SXRET_OK;` |
|      873 | 11481 |  |
|        - | 11482 | `/*` |
|        - | 11483 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11484 | ` * [i.e: Whether it is callable or not].` |
|        - | 11485 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11486 | ` */` |
|    23744 | 11487 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11488 |  |
|    23746 | 11489 | `	int res = 0;` |
|    23746 | 11490 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11491 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11492 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11493 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11494 | `		 * standard PHP behavior. */` |
|       20 | 11495 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11496 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11497 | `			res = 1;` |
|       10 | 11498 | `		}` |
|        9 | 11499 | `		(void)CallInvoke;` |
|    23737 | 11500 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11501 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11502 | `		if( pMap->nEntry == 2 ){` |
|        - | 11503 | `			ph7_class *pClass;` |
|        - | 11504 | `			ph7_value *pV;` |
|        - | 11505 | `			/* Extract the target class */` |
|       12 | 11506 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11507 | `			if( pV ){` |
|       12 | 11508 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11509 | `				if( pClass ){` |
|        - | 11510 | `					ph7_class_method *pMethod;` |
|        - | 11511 | `					/* Extract the target method */` |
|       10 | 11512 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11513 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11514 | `						/* Perform the lookup */` |
|       10 | 11515 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11516 | `						if( pMethod ){` |
|        - | 11517 | `							/* Method is callable */` |
|        5 | 11518 | `							res = 1;` |
|        2 | 11519 | `						}` |
|        4 | 11520 | `					}` |
|        4 | 11521 | `				}` |
|        5 | 11522 | `			}` |
|        7 | 11523 | `		}` |
|    23715 | 11524 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11525 | `		const char *zName;` |
|        - | 11526 | `		int nLen;` |
|        - | 11527 | `		/* Extract the name */` |
|     5870 | 11528 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11529 | `		/* Perform the lookup */` |
|     5885 | 11530 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11531 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11532 | `				/* Function is callable */` |
|     5852 | 11533 | `				res = 1;` |
|     2925 | 11534 | `		}` |
|     2934 | 11535 | `	}` |
|    23746 | 11536 | `	return res;` |
|        2 | 11537 |  |
|        - | 11538 | `/*` |
|        - | 11539 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11540 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11541 | ` * Parameters` |
|        - | 11542 | ` * $name` |
|        - | 11543 | ` *    The callback function to check` |
|        - | 11544 | ` * $syntax_only` |
|        - | 11545 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11546 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11547 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11548 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11549 | ` *    a string.` |
|        - | 11550 | ` * Return` |
|        - | 11551 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11552 | ` */` |
|       20 | 11553 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11554 |  |
|        - | 11555 | `	ph7_vm *pVm;` |
|        - | 11556 | `	int res;` |
|       21 | 11557 | `	if( nArg < 1 ){` |
|        - | 11558 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11559 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11560 | `		return SXRET_OK;` |
|        - | 11561 | `	}` |
|        - | 11562 | `	/* Point to the target VM */` |
|       21 | 11563 | `	pVm = pCtx->pVm;` |
|        - | 11564 | `	/* Perform the requested operation */` |
|       21 | 11565 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11566 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11567 | `	return SXRET_OK;` |
|       11 | 11568 |  |
|        - | 11569 | `/*` |
|        - | 11570 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11571 | ` * defined below.` |
|        - | 11572 | ` */` |
|     1306 | 11573 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11574 |  |
|     1307 | 11575 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11576 | `	ph7_value sName;` |
|        - | 11577 | `	sxi32 rc;` |
|        - | 11578 | `	/* Prepare the function name for insertion */` |
|     1307 | 11579 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11580 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11581 | `	/* Perform the insertion */` |
|     1307 | 11582 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11583 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11584 | `	return rc;` |
|        1 | 11585 |  |
|        - | 11586 | `/*` |
|        - | 11587 | ` * array get_defined_functions(void)` |
|        - | 11588 | ` *  Returns an array of all defined functions.` |
|        - | 11589 | ` * Parameter` |
|        - | 11590 | ` *  None.` |
|        - | 11591 | ` * Return` |
|        - | 11592 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11593 | ` *  both built-in (internal) and user-defined.` |
|        - | 11594 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11595 | ` *  defined ones using $arr["user"].` |
|        - | 11596 | ` * Note:` |
|        - | 11597 | ` *  NULL is returned on failure.` |
|        - | 11598 | ` */` |
|        2 | 11599 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11600 |  |
|        - | 11601 | `	ph7_value *pArray,*pEntry;` |
|        - | 11602 | `	/* NOTE:` |
|        - | 11603 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11604 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11605 | `	 */` |
|        3 | 11606 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11607 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11608 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11609 | `		SXUNUSED(apArg);` |
|        - | 11610 | `		/* Return NULL */` |
|      ! 0 | 11611 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11612 | `		return SXRET_OK;` |
|        - | 11613 | `	}` |
|        3 | 11614 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11615 | `	if( pEntry == 0 ){` |
|        - | 11616 | `		/* Return NULL */` |
|      ! 0 | 11617 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11618 | `		return SXRET_OK;` |
|        - | 11619 | `	}` |
|        - | 11620 | `	/* Fill with the appropriate information */` |
|        3 | 11621 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11622 | `	/* Create the 'internal' index */` |
|        3 | 11623 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11624 | `	/* Create the user-func array */` |
|        3 | 11625 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11626 | `	if( pEntry == 0 ){` |
|        - | 11627 | `		/* Return NULL */` |
|      ! 0 | 11628 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11629 | `		return SXRET_OK;` |
|        - | 11630 | `	}` |
|        - | 11631 | `	/* Fill with the appropriate information */` |
|        3 | 11632 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11633 | `	/* Create the 'user' index */` |
|        3 | 11634 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11635 | `	/* Return the multi-dimensional array */` |
|        3 | 11636 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11637 | `	return SXRET_OK;` |
|        2 | 11638 |  |
|        - | 11639 | `/*` |
|        - | 11640 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11641 | ` *  Register a function for execution on shutdown.` |
|        - | 11642 | ` * Note` |
|        - | 11643 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11644 | ` *  be called in the same order as they were registered.` |
|        - | 11645 | ` * Parameters` |
|        - | 11646 | ` *  $callback` |
|        - | 11647 | ` *   The shutdown callback to register.` |
|        - | 11648 | ` * $param` |
|        - | 11649 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11650 | ` * Return` |
|        - | 11651 | ` *  Nothing.` |
|        - | 11652 | ` */` |
|       10 | 11653 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11654 |  |
|        - | 11655 | `	VmShutdownCB sEntry;` |
|        - | 11656 | `	int i,j;` |
|       12 | 11657 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11658 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11659 | `		return PH7_OK;` |
|        - | 11660 | `	}` |
|        - | 11661 | `	/* Zero the Entry */` |
|       12 | 11662 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11663 | `	/* Initialize fields */` |
|       12 | 11664 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11665 | `	/* Save the callback name for later invocation name */` |
|       12 | 11666 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11667 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11668 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11669 | `	}` |
|        - | 11670 | `	/* Copy arguments */` |
|       12 | 11671 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11672 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11673 | `			/* Limit reached */` |
|      ! 0 | 11674 | `			break;` |
|        - | 11675 | `		}` |
|      ! 0 | 11676 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11677 | `	}` |
|       12 | 11678 | `	sEntry.nArg = j;` |
|        - | 11679 | `	/* Install the callback */` |
|       12 | 11680 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11681 | `	return PH7_OK;` |
|        7 | 11682 |  |
|        - | 11683 | `/*` |
|        - | 11684 | ` * Section:` |
|        - | 11685 | ` *  Class handling functions.` |
|        - | 11686 | ` * Status:` |
|        - | 11687 | ` *    Stable.` |
|        - | 11688 | ` */` |
|        - | 11689 | `/*` |
|        - | 11690 | ` * Extract the top active class. NULL is returned` |
|        - | 11691 | ` * if the class stack is empty.` |
|        - | 11692 | ` */` |
|      984 | 11693 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11694 |  |
|      986 | 11695 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11696 | `	ph7_class **apClass;` |
|      986 | 11697 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11698 | `		/* Empty stack,return NULL */` |
|       15 | 11699 | `		return 0;` |
|        - | 11700 | `	}` |
|        - | 11701 | `	/* Peek the last entry */` |
|      972 | 11702 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      972 | 11703 | `	return apClass[pSet->nUsed - 1];` |
|      494 | 11704 |  |
|        - | 11705 | `/*` |
|        - | 11706 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11707 | ` *   Get the class that declared the currently executing method.` |
|        - | 11708 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11709 | ` *` |
|        - | 11710 | ` * Parameters` |
|        - | 11711 | ` *   pVm: Target VM` |
|        - | 11712 | ` *` |
|        - | 11713 | ` * Return` |
|        - | 11714 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11715 | ` *   - Not executing within a class method` |
|        - | 11716 | ` *` |
|        - | 11717 | ` * Note` |
|        - | 11718 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11719 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11720 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11721 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11722 | ` *   declaring class.` |
|        - | 11723 | ` */` |
|       98 | 11724 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11725 |  |
|      100 | 11726 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11727 | `	ph7_vm_func *pVmFunc;` |
|        - | 11728 |  |
|        - | 11729 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11730 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11731 |  |
|        - | 11732 | `	/* Check if we're in a method context */` |
|      100 | 11733 | `	if( pFrame->pParent ){` |
|       96 | 11734 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11735 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11736 | `			/* Return the declaring class */` |
|       96 | 11737 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11738 | `		}` |
|      ! 0 | 11739 | `	}` |
|        - | 11740 |  |
|        5 | 11741 | `	return 0;` |
|       51 | 11742 |  |
|        - | 11743 |  |
|        - | 11744 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11745 | `/*` |
|        - | 11746 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11747 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11748 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11749 | ` * return value indicates failure.` |
|        - | 11750 | ` */` |
|        - | 11751 | `/*` |
|        - | 11752 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11753 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11754 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11755 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11756 | ` */` |
|     2480 | 11757 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11758 | `	ph7_vm *pVm,` |
|        - | 11759 | `	ph7_class_instance *pThis,` |
|        - | 11760 | `	ph7_class_method *pMethod,` |
|        - | 11761 | `	ph7_value *pResult,` |
|        - | 11762 | `	int nArg,` |
|        - | 11763 | `	ph7_value **apArg,` |
|        - | 11764 | `	VmCallArgMap *pMap` |
|        - | 11765 | `	)` |
|        2 | 11766 |  |
|        - | 11767 | `	ph7_value *aStack;` |
|        - | 11768 | `	VmInstr aInstr[2];` |
|        - | 11769 | `	int iCursor;` |
|        - | 11770 | `	int i;` |
|        - | 11771 | `	sxi32 rc;` |
|     2482 | 11772 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2482 | 11773 | `	if( aStack == 0 ){` |
|      ! 0 | 11774 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11775 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11776 | `		return SXERR_MEM;` |
|        - | 11777 | `	}` |
|     4024 | 11778 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1544 | 11779 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1544 | 11780 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      773 | 11781 | `	}` |
|     2482 | 11782 | `	iCursor = nArg + 1;` |
|     2482 | 11783 | `	if( pThis ){` |
|     2476 | 11784 | `		pThis->iRef++;` |
|     2476 | 11785 | `		aStack[i].x.pOther = pThis;` |
|     2476 | 11786 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1237 | 11787 | `	}` |
|     2482 | 11788 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2482 | 11789 | `	i++;` |
|     2482 | 11790 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2482 | 11791 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2482 | 11792 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2482 | 11793 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2482 | 11794 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2482 | 11795 | `	aInstr[0].iP1 = nArg;` |
|     2482 | 11796 | `	aInstr[0].iP2 = 0;` |
|     2482 | 11797 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2482 | 11798 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2482 | 11799 | `	aInstr[1].iP1 = 1;` |
|     2482 | 11800 | `	aInstr[1].iP2 = 0;` |
|     2482 | 11801 | `	aInstr[1].p3  = 0;` |
|     2482 | 11802 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2482 | 11803 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11804 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11805 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2482 | 11806 | `	return rc;` |
|     1242 | 11807 |  |
|     1922 | 11808 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11809 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11810 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11811 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11812 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11813 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11814 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11815 | `	)` |
|        2 | 11816 |  |
|     1924 | 11817 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11818 |  |
|        - | 11819 | `/*` |
|        - | 11820 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11821 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11822 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11823 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11824 | ` *` |
|        - | 11825 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11826 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11827 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11828 | ` *` |
|        - | 11829 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11830 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11831 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11832 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11833 | ` *` |
|        - | 11834 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11835 | ` */` |
|      174 | 11836 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11837 | `	ph7_vm *pVm,` |
|        - | 11838 | `	ph7_class_instance *pThis,` |
|        - | 11839 | `	int nArg,` |
|        - | 11840 | `	ph7_value **apArg,` |
|        - | 11841 | `	ph7_value *pResult,` |
|        - | 11842 | `	VmCallArgMap *pMap` |
|        - | 11843 | `	)` |
|        2 | 11844 |  |
|        - | 11845 | `	ph7_class_method *pMethod;` |
|      176 | 11846 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11847 | `	if( pMethod == 0 ){` |
|       13 | 11848 | `		if( pResult ){` |
|       13 | 11849 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11850 | `		}` |
|       13 | 11851 | `		return SXERR_INVALID;` |
|        - | 11852 | `	}` |
|      164 | 11853 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11854 |  |
|        - | 11855 | `/*` |
|        - | 11856 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11857 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11858 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11859 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11860 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11861 | ` * lookup or 'goto Exception').` |
|        - | 11862 | ` *` |
|        - | 11863 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11864 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11865 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11866 | ` * reported.` |
|        - | 11867 | ` */` |
|       12 | 11868 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11869 |  |
|        - | 11870 | `	ph7_class *pErrorClass;` |
|       13 | 11871 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11872 | `	ph7_class_method *pCons;` |
|        - | 11873 | `	VmFrame *pThrowFrame;` |
|        - | 11874 | `	char zMsg[256];` |
|        - | 11875 | `	int nMsg;` |
|        - | 11876 | `	sxi32 rc;` |
|       25 | 11877 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11878 | `		"Object of type %.*s is not callable",` |
|       12 | 11879 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11880 | `		pThis->pClass->sName.zString);` |
|       13 | 11881 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11882 | `	if( pErrorClass ){` |
|       13 | 11883 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11884 | `	}` |
|       13 | 11885 | `	if( pErrInst == 0 ){` |
|        - | 11886 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11887 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11888 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11889 | `		 * visible to the user. */` |
|      ! 0 | 11890 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11891 | `		return SXERR_ABORT;` |
|        - | 11892 | `	}` |
|       13 | 11893 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11894 | `	if( pCons ){` |
|        - | 11895 | `		ph7_value sArg;` |
|        - | 11896 | `		ph7_value *apMsg[1];` |
|        - | 11897 | `		SyString sMsgStr;` |
|       13 | 11898 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11899 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11900 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11901 | `		apMsg[0] = &sArg;` |
|       13 | 11902 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11903 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11904 | `	}` |
|        - | 11905 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11906 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11907 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11908 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11909 | `	if( pThrowFrame ){` |
|       13 | 11910 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11911 | `	}` |
|       13 | 11912 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11913 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11914 | `	return rc;` |
|        7 | 11915 |  |
|        - | 11916 | `/*` |
|        - | 11917 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11918 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11919 | ` * in the apArg[] array.` |
|        - | 11920 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11921 | ` * return value indicates failure.` |
|        - | 11922 | ` */` |
|     1212 | 11923 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11924 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11925 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11926 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11927 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11928 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11929 | `	)` |
|        2 | 11930 |  |
|        - | 11931 | `	ph7_value *aStack;` |
|        - | 11932 | `	VmInstr aInstr[2];` |
|        - | 11933 | `	int i;` |
|     1214 | 11934 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11935 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11936 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11937 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 11938 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 11939 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 11940 | `			nArg,apArg,pResult,0);` |
|        - | 11941 | `	}` |
|     1122 | 11942 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11943 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11944 | `		if( pResult ){` |
|        - | 11945 | `			/* Assume a null return value */` |
|      ! 0 | 11946 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11947 | `		}` |
|      511 | 11948 | `		return SXERR_INVALID;` |
|        - | 11949 | `	}` |
|      612 | 11950 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11951 | `		/* Class method */` |
|       15 | 11952 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 11953 | `		ph7_class_method *pMethod = 0;` |
|       15 | 11954 | `		ph7_class_instance *pThis = 0;` |
|       15 | 11955 | `		ph7_class *pClass = 0;` |
|        - | 11956 | `		ph7_value *pValue;` |
|        - | 11957 | `		sxi32 rc;` |
|       15 | 11958 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11959 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11960 | `			if( pResult ){` |
|        - | 11961 | `				/* Assume a null return value */` |
|      ! 0 | 11962 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11963 | `			}` |
|      ! 0 | 11964 | `			return SXRET_OK;` |
|        - | 11965 | `		}` |
|        - | 11966 | `		/* Extract the class name or an instance of it */` |
|       15 | 11967 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 11968 | `		if( pValue ){` |
|       15 | 11969 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 11970 | `		}` |
|       15 | 11971 | `		if( pClass == 0 ){` |
|        - | 11972 | `			/* No such class,return NULL */` |
|      ! 0 | 11973 | `			if( pResult ){` |
|      ! 0 | 11974 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11975 | `			}` |
|      ! 0 | 11976 | `			return SXRET_OK;` |
|        - | 11977 | `		}` |
|       15 | 11978 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11979 | `			/* Point to the class instance */` |
|        9 | 11980 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 11981 | `		}` |
|        - | 11982 | `		/* Try to extract the method */` |
|       15 | 11983 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 11984 | `		if( pValue ){` |
|       15 | 11985 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 11986 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 11987 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 11988 | `			}` |
|        7 | 11989 | `		}` |
|       15 | 11990 | `		if( pMethod == 0 ){` |
|        - | 11991 | `			/* No such method,return NULL */` |
|      ! 0 | 11992 | `			if( pResult ){` |
|      ! 0 | 11993 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11994 | `			}` |
|      ! 0 | 11995 | `			return SXRET_OK;` |
|        - | 11996 | `		}` |
|        - | 11997 | `		/* Call the class method */` |
|       15 | 11998 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 11999 | `		return rc;` |
|        - | 12000 | `	}` |
|        - | 12001 | `	/* Create a new operand stack */` |
|      598 | 12002 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 12003 | `	if( aStack == 0 ){` |
|      ! 0 | 12004 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12005 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12006 | `		if( pResult ){` |
|        - | 12007 | `			/* Assume a null return value */` |
|      ! 0 | 12008 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12009 | `		}` |
|      ! 0 | 12010 | `		return SXERR_MEM;` |
|        - | 12011 | `	}` |
|        - | 12012 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 12013 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 12014 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12015 | `		/*` |
|        - | 12016 | `		 * Symisc eXtension:` |
|        - | 12017 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12018 | `		 */` |
|     1304 | 12019 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 12020 | `	}` |
|        - | 12021 | `	/* Push the function name */` |
|      598 | 12022 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 12023 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12024 | `	/* Emit the CALL istruction */` |
|      598 | 12025 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 12026 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 12027 | `	aInstr[0].iP2 = 0;` |
|      598 | 12028 | `	aInstr[0].p3  = 0;` |
|        - | 12029 | `	/* Emit the DONE instruction */` |
|      598 | 12030 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 12031 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 12032 | `	aInstr[1].iP2 = 0;` |
|      598 | 12033 | `	aInstr[1].p3  = 0;` |
|        - | 12034 | `	/* Execute the function body (if available) */` |
|        - | 12035 | `	{` |
|        - | 12036 | `		sxi32 rcExec;` |
|      598 | 12037 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 12038 | `		/* Clean up the mess left behind */` |
|      598 | 12039 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12040 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 12041 | `		return rcExec;` |
|        - | 12042 | `	}` |
|      608 | 12043 |  |
|        - | 12044 | `/*` |
|        - | 12045 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12046 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12047 | ` * parameter.` |
|        - | 12048 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12049 | ` * return value indicates failure.` |
|        - | 12050 | ` */` |
|      240 | 12051 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12052 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12053 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12054 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12055 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12056 | `	)` |
|        1 | 12057 |  |
|        - | 12058 | `	ph7_value *pArg;` |
|        - | 12059 | `	SySet aArg;` |
|        - | 12060 | `	va_list ap;` |
|        - | 12061 | `	sxi32 rc;` |
|      241 | 12062 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12063 | `	/* Copy arguments one after one */` |
|      241 | 12064 | `	va_start(ap,pResult);` |
|      399 | 12065 | `	for(;;){` |
|      799 | 12066 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12067 | `		if( pArg == 0 ){` |
|      241 | 12068 | `			break;` |
|        - | 12069 | `		}` |
|      559 | 12070 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12071 | `	}` |
|        - | 12072 | `	/* Call the core routine */` |
|      241 | 12073 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12074 | `	/* Cleanup */` |
|      241 | 12075 | `	SySetRelease(&aArg);` |
|      241 | 12076 | `	return rc;` |
|        1 | 12077 |  |
|        - | 12078 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12079 | `/*` |
|        - | 12080 | ` * bool defined(string $name)` |
|        - | 12081 | ` *  Checks whether a given named constant exists.` |
|        - | 12082 | ` * Parameter:` |
|        - | 12083 | ` *  Name of the desired constant.` |
|        - | 12084 | ` * Return` |
|        - | 12085 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12086 | ` */` |
|       26 | 12087 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12088 |  |
|        - | 12089 | `	const char *zName;` |
|       28 | 12090 | `	int nLen = 0;` |
|       28 | 12091 | `	int res = 0;` |
|       28 | 12092 | `	if( nArg < 1 ){` |
|        - | 12093 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12094 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12095 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12096 | `		return SXRET_OK;` |
|        - | 12097 | `	}` |
|        - | 12098 | `	/* Extract constant name */` |
|       28 | 12099 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12100 | `	/* Perform the lookup */` |
|       28 | 12101 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12102 | `		/* Already defined */` |
|       26 | 12103 | `		res = 1;` |
|       12 | 12104 | `	}` |
|       28 | 12105 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12106 | `	return SXRET_OK;` |
|       15 | 12107 |  |
|        - | 12108 | `/*` |
|        - | 12109 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12110 | ` * below.` |
|        - | 12111 | ` */` |
|       16 | 12112 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 12113 |  |
|       18 | 12114 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12115 | `	/* Expand constant value */` |
|       18 | 12116 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       18 | 12117 |  |
|        - | 12118 | `/*` |
|        - | 12119 | ` * bool define(string $constant_name,expression value)` |
|        - | 12120 | ` *  Defines a named constant at runtime.` |
|        - | 12121 | ` * Parameter:` |
|        - | 12122 | ` *  $constant_name` |
|        - | 12123 | ` *   The name of the constant` |
|        - | 12124 | ` *  $value` |
|        - | 12125 | ` *   Constant value` |
|        - | 12126 | ` * Return:` |
|        - | 12127 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12128 | ` */` |
|       14 | 12129 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12130 |  |
|        - | 12131 | `	const char *zName;  /* Constant name */` |
|        - | 12132 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       16 | 12133 | `	int nLen = 0;       /* Name length */` |
|        - | 12134 | `	sxi32 rc;` |
|       16 | 12135 | `	if( nArg < 2 ){` |
|        - | 12136 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12137 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12138 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12139 | `		return SXRET_OK;` |
|        - | 12140 | `	}` |
|       16 | 12141 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12142 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12143 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12144 | `		return SXRET_OK;` |
|        - | 12145 | `	}` |
|        - | 12146 | `	/* Extract constant name */` |
|       16 | 12147 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       16 | 12148 | `	if( nLen < 1 ){` |
|      ! 0 | 12149 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12150 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12151 | `		return SXRET_OK;` |
|        - | 12152 | `	}` |
|        - | 12153 | `	/* Duplicate constant value */` |
|       16 | 12154 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       16 | 12155 | `	if( pValue == 0 ){` |
|      ! 0 | 12156 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12157 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12158 | `		return SXRET_OK;` |
|        - | 12159 | `	}` |
|        - | 12160 | `	/* Initialize the memory object */` |
|       16 | 12161 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12162 | `	/* Register the constant */` |
|       16 | 12163 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       16 | 12164 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12165 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12166 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12167 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12168 | `		return SXRET_OK;` |
|        - | 12169 | `	}` |
|        - | 12170 | `	/* Duplicate constant value */` |
|       16 | 12171 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       16 | 12172 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12173 | `		/* Lower case the constant name */` |
|      ! 0 | 12174 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12175 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12176 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12177 | `				/* UTF-8 stream */` |
|      ! 0 | 12178 | `				zCur++;` |
|      ! 0 | 12179 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12180 | `					zCur++;` |
|      ! 0 | 12181 | `				}` |
|      ! 0 | 12182 | `				continue;` |
|        - | 12183 | `			}` |
|      ! 0 | 12184 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12185 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12186 | `				zCur[0] = (char)c;` |
|      ! 0 | 12187 | `			}` |
|      ! 0 | 12188 | `			zCur++;` |
|      ! 0 | 12189 | `		}` |
|        - | 12190 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12191 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12192 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12193 | `		{` |
|      ! 0 | 12194 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12195 | `			if( pAlias ){` |
|      ! 0 | 12196 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12197 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12198 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12199 | `			}` |
|        - | 12200 | `		}` |
|      ! 0 | 12201 | `	}` |
|        - | 12202 | `	/* All done,return TRUE */` |
|       16 | 12203 | `	ph7_result_bool(pCtx,1);` |
|       16 | 12204 | `	return SXRET_OK;` |
|        9 | 12205 |  |
|        - | 12206 | `/*` |
|        - | 12207 | ` * value constant(string $name)` |
|        - | 12208 | ` *  Returns the value of a constant` |
|        - | 12209 | ` * Parameter` |
|        - | 12210 | ` *  $name` |
|        - | 12211 | ` *    Name of the constant.` |
|        - | 12212 | ` * Return` |
|        - | 12213 | ` *  Constant value or NULL if not defined.` |
|        - | 12214 | ` */` |
|        8 | 12215 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12216 |  |
|        - | 12217 | `	SyHashEntry *pEntry;` |
|        - | 12218 | `	ph7_constant *pCons;` |
|        - | 12219 | `	const char *zName; /* Constant name */` |
|        - | 12220 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12221 | `	int nLen;` |
|       10 | 12222 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12223 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12224 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12225 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12226 | `		return SXRET_OK;` |
|        - | 12227 | `	}` |
|        - | 12228 | `	/* Extract the constant name */` |
|       10 | 12229 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12230 | `	/* Perform the query */` |
|       10 | 12231 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 12232 | `	if( pEntry == 0 ){` |
|        3 | 12233 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12234 | `		ph7_result_null(pCtx);` |
|        3 | 12235 | `		return SXRET_OK;` |
|        - | 12236 | `	}` |
|        8 | 12237 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12238 | `	/* Point to the structure that describe the constant */` |
|        8 | 12239 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12240 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 12241 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12242 | `	/* Return that value */` |
|        8 | 12243 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12244 | `	/* Cleanup */` |
|        8 | 12245 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 12246 | `	return SXRET_OK;` |
|        6 | 12247 |  |
|        - | 12248 | `/*` |
|        - | 12249 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12250 | ` * defined below.` |
|        - | 12251 | ` */` |
|      466 | 12252 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12253 |  |
|      467 | 12254 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12255 | `	ph7_value sName;` |
|        - | 12256 | `	sxi32 rc;` |
|        - | 12257 | `	/* Prepare the constant name for insertion */` |
|      467 | 12258 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12259 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12260 | `	/* Perform the insertion */` |
|      467 | 12261 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12262 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12263 | `	return rc;` |
|        1 | 12264 |  |
|        - | 12265 | `/*` |
|        - | 12266 | ` * array get_defined_constants(void)` |
|        - | 12267 | ` *  Returns an associative array with the names of all defined` |
|        - | 12268 | ` *  constants.` |
|        - | 12269 | ` * Parameters` |
|        - | 12270 | ` *  NONE.` |
|        - | 12271 | ` * Returns` |
|        - | 12272 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12273 | ` */` |
|        2 | 12274 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12275 |  |
|        - | 12276 | `	ph7_value *pArray;` |
|        - | 12277 | `	/* Create the array first*/` |
|        3 | 12278 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12279 | `	if( pArray == 0 ){` |
|      ! 0 | 12280 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12281 | `		SXUNUSED(apArg);` |
|        - | 12282 | `		/* Return NULL */` |
|      ! 0 | 12283 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12284 | `		return SXRET_OK;` |
|        - | 12285 | `	}` |
|        - | 12286 | `	/* Fill the array with the defined constants */` |
|        3 | 12287 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12288 | `	/* Return the created array */` |
|        3 | 12289 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12290 | `	return SXRET_OK;` |
|        2 | 12291 |  |
|        - | 12292 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12293 | `/*` |
|        - | 12294 | ` * Section:` |
|        - | 12295 | ` *  Random numbers/string generators.` |
|        - | 12296 | ` * Status:` |
|        - | 12297 | ` *    Stable.` |
|        - | 12298 | ` */` |
|        - | 12299 | `/*` |
|        - | 12300 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12301 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12302 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12303 | ` */` |
|     2904 | 12304 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 12305 |  |
|        - | 12306 | `	sxu32 iNum;` |
|     2906 | 12307 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2906 | 12308 | `	return iNum;` |
|        2 | 12309 |  |
|        - | 12310 | `/*` |
|        - | 12311 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12312 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12313 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12314 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12315 | ` */` |
|   236484 | 12316 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12317 |  |
|        - | 12318 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12319 | `	int i;` |
|        - | 12320 | `	/* Generate a binary string first */` |
|   236486 | 12321 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12322 | `	/* Turn the binary string into english based alphabet */` |
|  2601494 | 12323 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2365010 | 12324 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1182506 | 12325 | `	 }` |
|   236486 | 12326 |  |
|        - | 12327 | `/*` |
|        - | 12328 | ` * int rand()` |
|        - | 12329 | ` * int mt_rand()` |
|        - | 12330 | ` * int rand(int $min,int $max)` |
|        - | 12331 | ` * int mt_rand(int $min,int $max)` |
|        - | 12332 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12333 | ` * Parameter` |
|        - | 12334 | ` *  $min` |
|        - | 12335 | ` *    The lowest value to return (default: 0)` |
|        - | 12336 | ` *  $max` |
|        - | 12337 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12338 | ` * Return` |
|        - | 12339 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12340 | ` * Note:` |
|        - | 12341 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12342 | ` *  by te SQLite3 library.` |
|        - | 12343 | ` */` |
|       20 | 12344 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12345 |  |
|        - | 12346 | `	sxu32 iNum;` |
|        - | 12347 | `	/* Generate the random number */` |
|       21 | 12348 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12349 | `	if( nArg > 1 ){` |
|        - | 12350 | `		sxu32 iMin,iMax;` |
|        3 | 12351 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12352 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12353 | `		if( iMin < iMax ){` |
|        3 | 12354 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12355 | `			if( iDiv > 0 ){` |
|        3 | 12356 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12357 | `			}` |
|        1 | 12358 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12359 | `			iNum %= iMax;` |
|      ! 0 | 12360 | `		}` |
|        1 | 12361 | `	}` |
|        - | 12362 | `	/* Return the number */` |
|       21 | 12363 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12364 | `	return SXRET_OK;` |
|        1 | 12365 |  |
|        - | 12366 | `/*` |
|        - | 12367 | ` * int getrandmax(void)` |
|        - | 12368 | ` * int mt_getrandmax(void)` |
|        - | 12369 | ` * int rc4_getrandmax(void)` |
|        - | 12370 | ` *   Show largest possible random value` |
|        - | 12371 | ` * Return` |
|        - | 12372 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12373 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12374 | ` * Note:` |
|        - | 12375 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12376 | ` *  by te SQLite3 library.` |
|        - | 12377 | ` */` |
|        4 | 12378 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12379 |  |
|        2 | 12380 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12381 | `	SXUNUSED(apArg);` |
|        5 | 12382 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12383 | `	return SXRET_OK;` |
|        1 | 12384 |  |
|        - | 12385 | `/*` |
|        - | 12386 | ` * string rand_str()` |
|        - | 12387 | ` * string rand_str(int $len)` |
|        - | 12388 | ` *  Generate a random string (English alphabet).` |
|        - | 12389 | ` * Parameter` |
|        - | 12390 | ` *  $len` |
|        - | 12391 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12392 | ` * Return` |
|        - | 12393 | ` *   A pseudo random string.` |
|        - | 12394 | ` * Note:` |
|        - | 12395 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12396 | ` *  by te SQLite3 library.` |
|        - | 12397 | ` *  This function is a symisc extension.` |
|        - | 12398 | ` */` |
|      120 | 12399 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12400 |  |
|        - | 12401 | `	char zString[1024];` |
|      122 | 12402 | `	int iLen = 0x10;` |
|      122 | 12403 | `	if( nArg > 0 ){` |
|        - | 12404 | `		/* Get the desired length */` |
|      122 | 12405 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12406 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12407 | `			/* Default length */` |
|        3 | 12408 | `			iLen = 0x10;` |
|        1 | 12409 | `		}` |
|       60 | 12410 | `	}` |
|        - | 12411 | `	/* Generate the random string */` |
|      122 | 12412 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12413 | `	/* Return the generated string */` |
|      122 | 12414 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12415 | `	return SXRET_OK;` |
|        2 | 12416 |  |
|        - | 12417 | `/*` |
|        - | 12418 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12419 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12420 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12421 | ` */` |
|      488 | 12422 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12423 |  |
|      488 | 12424 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12425 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12426 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12427 | `			"TypeError",` |
|        - | 12428 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12429 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12430 | `			ph7_type_name(pArg)` |
|        - | 12431 | `			);` |
|        - | 12432 | `	}` |
|      483 | 12433 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12434 | `		int len;` |
|        9 | 12435 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12436 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12437 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12438 | `				"TypeError",` |
|        - | 12439 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12440 | `				zFunc,iArgPos,zParamName` |
|        - | 12441 | `				);` |
|        - | 12442 | `		}` |
|        2 | 12443 | `	}` |
|      479 | 12444 | `	return SXRET_OK;` |
|      245 | 12445 |  |
|        - | 12446 | `/*` |
|        - | 12447 | ` * int random_int(int $min, int $max)` |
|        - | 12448 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12449 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12450 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12451 | ` *  power-of-two mask covering the range.` |
|        - | 12452 | ` */` |
|      242 | 12453 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12454 |  |
|        - | 12455 | `	sxi64 iMin,iMax;` |
|        - | 12456 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12457 | `	unsigned int nAttempt;` |
|        - | 12458 | `	int rc;` |
|      243 | 12459 | `	if( nArg != 2 ){` |
|       10 | 12460 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12461 | `			"ArgumentCountError",` |
|        - | 12462 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12463 | `			nArg` |
|        - | 12464 | `			);` |
|        - | 12465 | `	}` |
|      237 | 12466 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12467 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12468 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12469 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12470 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12471 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12472 | `	if( iMin > iMax ){` |
|        3 | 12473 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12474 | `			"ValueError",` |
|        - | 12475 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12476 | `			);` |
|        - | 12477 | `	}` |
|      229 | 12478 | `	if( iMin == iMax ){` |
|        5 | 12479 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12480 | `		return SXRET_OK;` |
|        - | 12481 | `	}` |
|      225 | 12482 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12483 | `	uMask = uRange;` |
|      225 | 12484 | `	uMask \|= uMask >> 1;` |
|      225 | 12485 | `	uMask \|= uMask >> 2;` |
|      225 | 12486 | `	uMask \|= uMask >> 4;` |
|      225 | 12487 | `	uMask \|= uMask >> 8;` |
|      225 | 12488 | `	uMask \|= uMask >> 16;` |
|      225 | 12489 | `	uMask \|= uMask >> 32;` |
|      225 | 12490 | `	uResult = 0;` |
|      339 | 12491 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12492 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12493 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12494 | `		 * and the low-half mask would always read 0). */` |
|        - | 12495 | `		sxu64 uDraw;` |
|      339 | 12496 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12497 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12498 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12499 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12500 | `				"Exception",` |
|        - | 12501 | `				"Cannot gather sufficient random data"` |
|        - | 12502 | `				);` |
|        - | 12503 | `		}` |
|      339 | 12504 | `		uDraw &= uMask;` |
|      339 | 12505 | `		if( uDraw <= uRange ){` |
|      225 | 12506 | `			uResult = uDraw;` |
|      225 | 12507 | `			break;` |
|        - | 12508 | `		}` |
|       58 | 12509 | `	}` |
|      225 | 12510 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12511 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12512 | `			"Exception",` |
|        - | 12513 | `			"Cannot gather sufficient random data"` |
|        - | 12514 | `			);` |
|        - | 12515 | `	}` |
|      225 | 12516 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12517 | `	return SXRET_OK;` |
|      122 | 12518 |  |
|        - | 12519 | `/*` |
|        - | 12520 | ` * string random_bytes(int $length)` |
|        - | 12521 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12522 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12523 | ` */` |
|       24 | 12524 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12525 |  |
|        - | 12526 | `	sxi64 iLen;` |
|        - | 12527 | `	unsigned char zStack[256];` |
|        - | 12528 | `	void *pBuf;` |
|        - | 12529 | `	int rc;` |
|       25 | 12530 | `	int bHeap = 0;` |
|       25 | 12531 | `	if( nArg != 1 ){` |
|        7 | 12532 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12533 | `			"ArgumentCountError",` |
|        - | 12534 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12535 | `			nArg` |
|        - | 12536 | `			);` |
|        - | 12537 | `	}` |
|       21 | 12538 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12539 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12540 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12541 | `	if( iLen < 1 ){` |
|        5 | 12542 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12543 | `			"ValueError",` |
|        - | 12544 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12545 | `			);` |
|        - | 12546 | `	}` |
|        - | 12547 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12548 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12549 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12550 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12551 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12552 | `			"ValueError",` |
|        - | 12553 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12554 | `			);` |
|        - | 12555 | `	}` |
|       13 | 12556 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12557 | `		pBuf = zStack;` |
|        7 | 12558 | `	}else{` |
|      ! 0 | 12559 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12560 | `		if( pBuf == 0 ){` |
|      ! 0 | 12561 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12562 | `				"Exception",` |
|        - | 12563 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12564 | `				iLen` |
|        - | 12565 | `				);` |
|        - | 12566 | `		}` |
|      ! 0 | 12567 | `		bHeap = 1;` |
|        - | 12568 | `	}` |
|       13 | 12569 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12570 | `		if( bHeap ){` |
|      ! 0 | 12571 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12572 | `		}` |
|      ! 0 | 12573 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12574 | `			"Exception",` |
|        - | 12575 | `			"Cannot gather sufficient random data"` |
|        - | 12576 | `			);` |
|        - | 12577 | `	}` |
|       13 | 12578 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12579 | `	if( bHeap ){` |
|      ! 0 | 12580 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12581 | `	}` |
|       13 | 12582 | `	return SXRET_OK;` |
|       13 | 12583 |  |
|        - | 12584 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12585 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12586 | `/* Unique ID private data */` |
|        - | 12587 | `struct unique_id_data` |
|        - | 12588 |  |
|        - | 12589 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12590 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12591 | `};` |
|        - | 12592 | `/*` |
|        - | 12593 | ` * Binary to hex consumer callback.` |
|        - | 12594 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12595 | ` * defined below.` |
|        - | 12596 | ` */` |
|      192 | 12597 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12598 |  |
|      193 | 12599 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12600 | `	sxu32 nBuflen;` |
|        - | 12601 | `	/* Extract result buffer length */` |
|      193 | 12602 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12603 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12604 | `			/*` |
|        - | 12605 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12606 | `			 * string will be 13 characters long` |
|        - | 12607 | `			 */` |
|       25 | 12608 | `		return SXERR_ABORT;` |
|        - | 12609 | `	}` |
|      169 | 12610 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12611 | `		return SXERR_ABORT;` |
|        - | 12612 | `	}` |
|        - | 12613 | `	/* Safely Consume the hex stream */` |
|      169 | 12614 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12615 | `	return SXRET_OK;` |
|       97 | 12616 |  |
|        - | 12617 | `/*` |
|        - | 12618 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12619 | ` *  Generate a unique ID` |
|        - | 12620 | ` * Parameter` |
|        - | 12621 | ` * $prefix` |
|        - | 12622 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12623 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12624 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12625 | ` * $more_entropy` |
|        - | 12626 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12627 | ` *  that the result will be unique.` |
|        - | 12628 | ` * Return` |
|        - | 12629 | ` *  Returns the unique identifier, as a string.` |
|        - | 12630 | ` */` |
|       24 | 12631 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12632 |  |
|        - | 12633 | `	struct unique_id_data sUniq;` |
|        - | 12634 | `	unsigned char zDigest[20];` |
|       25 | 12635 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12636 | `	const char *zPrefix;` |
|        - | 12637 | `	SHA1Context sCtx;` |
|        - | 12638 | `	char zRandom[7];` |
|        - | 12639 | `	int nPrefix;` |
|        - | 12640 | `	int entropy;` |
|        - | 12641 | `	/* Generate a random string first */` |
|       25 | 12642 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12643 | `	/* Initialize fields */` |
|       25 | 12644 | `	zPrefix = 0;` |
|       25 | 12645 | `	nPrefix = 0;` |
|       25 | 12646 | `	entropy = 0;` |
|       25 | 12647 | `	if( nArg > 0 ){` |
|        - | 12648 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12649 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12650 | `		if( nArg > 1 ){` |
|      ! 0 | 12651 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12652 | `		}` |
|      ! 0 | 12653 | `	}` |
|       25 | 12654 | `	SHA1Init(&sCtx);` |
|        - | 12655 | `	/* Generate the random ID */` |
|       25 | 12656 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12657 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12658 | `	}` |
|        - | 12659 | `	/* Append the random ID */` |
|       25 | 12660 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12661 | `	/* Append the random string */` |
|       25 | 12662 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12663 | `	/* Increment the number */` |
|       25 | 12664 | `	pVm->unique_id++;` |
|       25 | 12665 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12666 | `	/* Hexify the digest */` |
|       25 | 12667 | `	sUniq.pCtx = pCtx;` |
|       25 | 12668 | `	sUniq.entropy = entropy;` |
|       25 | 12669 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12670 | `	/* All done */` |
|       25 | 12671 | `	return PH7_OK;` |
|        1 | 12672 |  |
|        - | 12673 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12674 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12675 | `/*` |
|        - | 12676 | ` * Section:` |
|        - | 12677 | ` *  Language construct implementation as foreign functions.` |
|        - | 12678 | ` * Status:` |
|        - | 12679 | ` *    Stable.` |
|        - | 12680 | ` */` |
|        - | 12681 | `/*` |
|        - | 12682 | ` * void echo($string...)` |
|        - | 12683 | ` *  Output one or more messages.` |
|        - | 12684 | ` * Parameters` |
|        - | 12685 | ` *  $string` |
|        - | 12686 | ` *   Message to output.` |
|        - | 12687 | ` * Return` |
|        - | 12688 | ` *  NULL.` |
|        - | 12689 | ` */` |
|      ! 0 | 12690 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12691 |  |
|        - | 12692 | `	const char *zData;` |
|      ! 0 | 12693 | `	int nDataLen = 0;` |
|        - | 12694 | `	ph7_vm *pVm;` |
|        - | 12695 | `	int i,rc;` |
|        - | 12696 | `	/* Point to the target VM */` |
|      ! 0 | 12697 | `	pVm = pCtx->pVm;` |
|        - | 12698 | `	/* Output */` |
|      ! 0 | 12699 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12700 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12701 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12702 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12703 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12704 | `			if( rc == SXERR_ABORT ){` |
|        - | 12705 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12706 | `				return PH7_ABORT;` |
|        - | 12707 | `			}` |
|      ! 0 | 12708 | `		}` |
|      ! 0 | 12709 | `	}` |
|      ! 0 | 12710 | `	return SXRET_OK;` |
|      ! 0 | 12711 |  |
|        - | 12712 | `/*` |
|        - | 12713 | ` * int print($string...)` |
|        - | 12714 | ` *  Output one or more messages.` |
|        - | 12715 | ` * Parameters` |
|        - | 12716 | ` *  $string` |
|        - | 12717 | ` *   Message to output.` |
|        - | 12718 | ` * Return` |
|        - | 12719 | ` *  1 always.` |
|        - | 12720 | ` */` |
|        2 | 12721 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12722 |  |
|        - | 12723 | `	const char *zData;` |
|        3 | 12724 | `	int nDataLen = 0;` |
|        - | 12725 | `	ph7_vm *pVm;` |
|        - | 12726 | `	int i,rc;` |
|        - | 12727 | `	/* Point to the target VM */` |
|        3 | 12728 | `	pVm = pCtx->pVm;` |
|        - | 12729 | `	/* Output */` |
|        5 | 12730 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12731 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12732 | `		if( nDataLen > 0 ){` |
|        3 | 12733 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12734 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12735 | `			if( rc == SXERR_ABORT ){` |
|        - | 12736 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12737 | `				return PH7_ABORT;` |
|        - | 12738 | `			}` |
|        1 | 12739 | `		}` |
|        2 | 12740 | `	}` |
|        - | 12741 | `	/* Return 1 */` |
|        3 | 12742 | `	ph7_result_int(pCtx,1);` |
|        3 | 12743 | `	return SXRET_OK;` |
|        2 | 12744 |  |
|        - | 12745 | `/*` |
|        - | 12746 | ` * void exit(string $msg)` |
|        - | 12747 | ` * void exit(int $status)` |
|        - | 12748 | ` * void die(string $ms)` |
|        - | 12749 | ` * void die(int $status)` |
|        - | 12750 | ` *   Output a message and terminate program execution.` |
|        - | 12751 | ` * Parameter` |
|        - | 12752 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12753 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12754 | ` *  and not printed` |
|        - | 12755 | ` * Return` |
|        - | 12756 | ` *  NULL` |
|        - | 12757 | ` */` |
|      ! 0 | 12758 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12759 |  |
|      ! 0 | 12760 | `	if( nArg > 0 ){` |
|      ! 0 | 12761 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12762 | `			const char *zData;` |
|      ! 0 | 12763 | `			int iLen = 0;` |
|        - | 12764 | `			/* Print exit message */` |
|      ! 0 | 12765 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12766 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12767 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12768 | `			sxi32 iExitStatus;` |
|        - | 12769 | `			/* Record exit status code */` |
|      ! 0 | 12770 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12771 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12772 | `		}` |
|      ! 0 | 12773 | `	}` |
|        - | 12774 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12775 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12776 | `	 */` |
|      ! 0 | 12777 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12778 | `	return PH7_ABORT;` |
|      ! 0 | 12779 |  |
|        - | 12780 | `/*` |
|        - | 12781 | ` * bool isset($var,...)` |
|        - | 12782 | ` *  Finds out whether a variable is set.` |
|        - | 12783 | ` * Parameters` |
|        - | 12784 | ` *  One or more variable to check.` |
|        - | 12785 | ` * Return` |
|        - | 12786 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12787 | ` */` |
|    92708 | 12788 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12789 |  |
|        - | 12790 | `	ph7_value *pObj;` |
|    92710 | 12791 | `	int res = 0;` |
|        - | 12792 | `	int i;` |
|    92710 | 12793 | `	if( nArg < 1 ){` |
|        - | 12794 | `		/* Missing arguments,return false */` |
|      ! 0 | 12795 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12796 | `		return SXRET_OK;` |
|        - | 12797 | `	}` |
|        - | 12798 | `	/* Iterate over available arguments */` |
|   121180 | 12799 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92720 | 12800 | `		pObj = apArg[i];` |
|    92720 | 12801 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12802 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12803 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12804 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63276 | 12805 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12806 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12807 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12808 | `			}` |
|    31637 | 12809 | `		}` |
|    92720 | 12810 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92720 | 12811 | `		if( !res ){` |
|        - | 12812 | `			/* Variable not set,return FALSE */` |
|    64250 | 12813 | `			ph7_result_bool(pCtx,0);` |
|    64250 | 12814 | `			return SXRET_OK;` |
|        - | 12815 | `		}` |
|    14237 | 12816 | `	}` |
|        - | 12817 | `	/* All given variable are set,return TRUE */` |
|    28462 | 12818 | `	ph7_result_bool(pCtx,1);` |
|    28462 | 12819 | `	return SXRET_OK;` |
|    46356 | 12820 |  |
|        - | 12821 | `/*` |
|        - | 12822 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12823 | ` * frame,the reference table and discard it's contents.` |
|        - | 12824 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12825 | ` */` |
|  3161212 | 12826 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12827 |  |
|        - | 12828 | `	ph7_value *pObj;` |
|        - | 12829 | `	VmRefObj *pRef;` |
|  3161214 | 12830 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3161214 | 12831 | `	if( pObj ){` |
|        - | 12832 | `		/* Release the object */` |
|  3161214 | 12833 | `		PH7_MemObjRelease(pObj);` |
|  1580606 | 12834 | `	}` |
|        - | 12835 | `	/* Remove old reference links */` |
|  3161214 | 12836 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3161214 | 12837 | `	if( pRef ){` |
|  3161208 | 12838 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12839 | `		/* Unlink from the reference table */` |
|  3161208 | 12840 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3161208 | 12841 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12842 | `			VmSlot sFree;` |
|        - | 12843 | `			/* Restore to the free list */` |
|  3161200 | 12844 | `			sFree.nIdx = nObjIdx;` |
|  3161200 | 12845 | `			sFree.pUserData = 0;` |
|  3161200 | 12846 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1580599 | 12847 | `		}` |
|  1580603 | 12848 | `	}` |
|  3161214 | 12849 | `	return SXRET_OK;` |
|        2 | 12850 |  |
|        - | 12851 | `/*` |
|        - | 12852 | ` * void unset($var,...)` |
|        - | 12853 | ` *   Unset one or more given variable.` |
|        - | 12854 | ` * Parameters` |
|        - | 12855 | ` *  One or more variable to unset.` |
|        - | 12856 | ` * Return` |
|        - | 12857 | ` *  Nothing.` |
|        - | 12858 | ` */` |
|     7522 | 12859 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12860 |  |
|        - | 12861 | `	ph7_value *pObj;` |
|        - | 12862 | `	ph7_vm *pVm;` |
|        - | 12863 | `	int i;` |
|        - | 12864 | `	/* Point to the target VM */` |
|     7524 | 12865 | `	pVm = pCtx->pVm;` |
|        - | 12866 | `	/* Iterate and unset */` |
|    15046 | 12867 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7524 | 12868 | `		pObj = apArg[i];` |
|     7524 | 12869 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      836 | 12870 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12871 | `				/* Throw an error */` |
|      ! 0 | 12872 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12873 | `			}` |
|      419 | 12874 | `		}else{` |
|     6690 | 12875 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12876 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6690 | 12877 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6684 | 12878 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3341 | 12879 | `			}` |
|        - | 12880 | `		}` |
|     3763 | 12881 | `	}` |
|     7524 | 12882 | `	return SXRET_OK;` |
|        2 | 12883 |  |
|        - | 12884 | `/*` |
|        - | 12885 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12886 | ` */` |
|      116 | 12887 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12888 |  |
|      117 | 12889 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12890 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12891 | `	ph7_value *pObj;` |
|        - | 12892 | `	sxu32 nIdx;` |
|        - | 12893 | `	/* Extract the memory object */` |
|      117 | 12894 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12895 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12896 | `	if( pObj ){` |
|      117 | 12897 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12898 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12899 | `				SyString sName;` |
|        - | 12900 | `				ph7_value sKey;` |
|        - | 12901 | `				/* Perform the insertion */` |
|      115 | 12902 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12903 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12904 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12905 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12906 | `			}` |
|       57 | 12907 | `		}` |
|       58 | 12908 | `	}` |
|      117 | 12909 | `	return SXRET_OK;` |
|        1 | 12910 |  |
|        - | 12911 | `/*` |
|        - | 12912 | ` * array get_defined_vars(void)` |
|        - | 12913 | ` *  Returns an array of all defined variables.` |
|        - | 12914 | ` * Parameter` |
|        - | 12915 | ` *  None` |
|        - | 12916 | ` * Return` |
|        - | 12917 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12918 | ` */` |
|        2 | 12919 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12920 |  |
|        3 | 12921 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12922 | `	ph7_value *pArray;` |
|        - | 12923 | `	/* Create a new array */` |
|        3 | 12924 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12925 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12926 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12927 | `		SXUNUSED(apArg);` |
|        - | 12928 | `		/* Return NULL */` |
|      ! 0 | 12929 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12930 | `		return SXRET_OK;` |
|        - | 12931 | `	}` |
|        - | 12932 | `	/* Superglobals first */` |
|        3 | 12933 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12934 | `	/* Then variable defined in the current frame */` |
|        3 | 12935 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12936 | `	/* Finally,return the created array */` |
|        3 | 12937 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12938 | `	return SXRET_OK;` |
|        2 | 12939 |  |
|        - | 12940 | `/*` |
|        - | 12941 | ` * bool gettype($var)` |
|        - | 12942 | ` *  Get the type of a variable` |
|        - | 12943 | ` * Parameters` |
|        - | 12944 | ` *   $var` |
|        - | 12945 | ` *    The variable being type checked.` |
|        - | 12946 | ` * Return` |
|        - | 12947 | ` *   String representation of the given variable type.` |
|        - | 12948 | ` */` |
|       32 | 12949 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12950 |  |
|       34 | 12951 | `	const char *zType = "Empty";` |
|       34 | 12952 | `	if( nArg > 0 ){` |
|       34 | 12953 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12954 | `	}` |
|        - | 12955 | `	/* Return the variable type */` |
|       34 | 12956 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12957 | `	return SXRET_OK;` |
|        2 | 12958 |  |
|        - | 12959 | `/*` |
|        - | 12960 | ` * string get_resource_type(resource $handle)` |
|        - | 12961 | ` *  This function gets the type of the given resource.` |
|        - | 12962 | ` * Parameters` |
|        - | 12963 | ` *  $handle` |
|        - | 12964 | ` *  The evaluated resource handle.` |
|        - | 12965 | ` * Return` |
|        - | 12966 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12967 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12968 | ` *  the return value will be the string Unknown.` |
|        - | 12969 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12970 | ` *  is not a resource.` |
|        - | 12971 | ` */` |
|        2 | 12972 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12973 |  |
|        3 | 12974 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12975 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12976 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12977 | `		return PH7_OK;` |
|        - | 12978 | `	}` |
|        3 | 12979 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12980 | `	return SXRET_OK;` |
|        2 | 12981 |  |
|        - | 12982 | `/*` |
|        - | 12983 | ` * void var_dump(expression,....)` |
|        - | 12984 | ` *   var_dump � Dumps information about a variable` |
|        - | 12985 | ` * Parameters` |
|        - | 12986 | ` *   One or more expression to dump.` |
|        - | 12987 | ` * Returns` |
|        - | 12988 | ` *  Nothing.` |
|        - | 12989 | ` */` |
|      218 | 12990 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12991 |  |
|        - | 12992 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12993 | `	int i;` |
|      220 | 12994 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12995 | `	/* Dump one or more expressions */` |
|      444 | 12996 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12997 | `		ph7_value *pObj = apArg[i];` |
|        - | 12998 | `		/* Reset the working buffer */` |
|      226 | 12999 | `		SyBlobReset(&sDump);` |
|        - | 13000 | `		/* Dump the given expression */` |
|      226 | 13001 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13002 | `		/* Output */` |
|      226 | 13003 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 13004 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13005 | `		}` |
|      114 | 13006 | `	}` |
|        - | 13007 | `	/* Release the working buffer */` |
|      220 | 13008 | `	SyBlobRelease(&sDump);` |
|      220 | 13009 | `	return SXRET_OK;` |
|        2 | 13010 |  |
|        - | 13011 | `/*` |
|        - | 13012 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13013 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13014 | ` * Parameters` |
|        - | 13015 | ` *   expression: Expression to dump` |
|        - | 13016 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13017 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13018 | ` *            print_r() will return the information rather than print it.` |
|        - | 13019 | ` * Return` |
|        - | 13020 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13021 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13022 | ` */` |
|       16 | 13023 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13024 |  |
|       17 | 13025 | `	int ret_string = 0;` |
|        - | 13026 | `	SyBlob sDump;` |
|       17 | 13027 | `	if( nArg < 1 ){` |
|        - | 13028 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13029 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13030 | `		return SXRET_OK;` |
|        - | 13031 | `	}` |
|       17 | 13032 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13033 | `	if ( nArg > 1 ){` |
|        - | 13034 | `		/* Where to redirect output */` |
|       11 | 13035 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13036 | `	}` |
|        - | 13037 | `	/* Generate dump */` |
|       17 | 13038 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13039 | `	if( !ret_string ){` |
|        - | 13040 | `		/* Output dump */` |
|        7 | 13041 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13042 | `		/* Return true */` |
|        7 | 13043 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13044 | `	}else{` |
|        - | 13045 | `		/* Generated dump as return value */` |
|       11 | 13046 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13047 | `	}` |
|        - | 13048 | `	/* Release the working buffer */` |
|       17 | 13049 | `	SyBlobRelease(&sDump);` |
|       17 | 13050 | `	return SXRET_OK;` |
|        9 | 13051 |  |
|        - | 13052 | `/*` |
|        - | 13053 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13054 | ` * Same job as print_r. (see coment above)` |
|        - | 13055 | ` */` |
|        2 | 13056 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13057 |  |
|        3 | 13058 | `	int ret_string = 0;` |
|        - | 13059 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13060 | `	if( nArg < 1 ){` |
|        - | 13061 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13062 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13063 | `		return SXRET_OK;` |
|        - | 13064 | `	}` |
|        3 | 13065 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13066 | `	if ( nArg > 1 ){` |
|        - | 13067 | `		/* Where to redirect output */` |
|        3 | 13068 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13069 | `	}` |
|        - | 13070 | `	/* Generate dump */` |
|        3 | 13071 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13072 | `	if( !ret_string ){` |
|        - | 13073 | `		/* Output dump */` |
|      ! 0 | 13074 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13075 | `		/* Return NULL */` |
|      ! 0 | 13076 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13077 | `	}else{` |
|        - | 13078 | `		/* Generated dump as return value */` |
|        3 | 13079 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13080 | `	}` |
|        - | 13081 | `	/* Release the working buffer */` |
|        3 | 13082 | `	SyBlobRelease(&sDump);` |
|        3 | 13083 | `	return SXRET_OK;` |
|        2 | 13084 |  |
|        - | 13085 | `/*` |
|        - | 13086 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13087 | ` *  Set/get the various assert flags.` |
|        - | 13088 | ` * Parameter` |
|        - | 13089 | ` * $what` |
|        - | 13090 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13091 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13092 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13093 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13094 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13095 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13096 | ` * $value` |
|        - | 13097 | ` *   An optional new value for the option.` |
|        - | 13098 | ` * Return` |
|        - | 13099 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13100 | ` */` |
|       28 | 13101 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13102 |  |
|       30 | 13103 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13104 | `	int iOption;` |
|        - | 13105 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 13106 | `	if( nArg < 1 ){` |
|        3 | 13107 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13108 | `			"ArgumentCountError",` |
|        - | 13109 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13110 | `			);` |
|        - | 13111 | `	}` |
|        - | 13112 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13113 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 13114 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13115 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13116 | `			"TypeError",` |
|        - | 13117 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13118 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13119 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13120 | `			);` |
|        - | 13121 | `	}` |
|       28 | 13122 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13123 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13124 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13125 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 13126 | `	switch( iOption ){` |
|        5 | 13127 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13128 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13129 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13130 | `		if( nArg > 1 ){` |
|        5 | 13131 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13132 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13133 | `			}else{` |
|        3 | 13134 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13135 | `			}` |
|        2 | 13136 | `		}` |
|       12 | 13137 | `		break;` |
|        1 | 13138 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13139 | `		/* Return old callback or null */` |
|        3 | 13140 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13141 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13142 | `		}else{` |
|        3 | 13143 | `			ph7_result_null(pCtx);` |
|        - | 13144 | `		}` |
|        3 | 13145 | `		if( nArg > 1 ){` |
|      ! 0 | 13146 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13147 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13148 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13149 | `			}else{` |
|      ! 0 | 13150 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13151 | `			}` |
|      ! 0 | 13152 | `		}` |
|        3 | 13153 | `		break;` |
|        5 | 13154 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13155 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13156 | `		if( nArg > 1 ){` |
|        5 | 13157 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13158 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13159 | `			}else{` |
|        3 | 13160 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13161 | `			}` |
|        2 | 13162 | `		}` |
|       11 | 13163 | `		break;` |
|      ! 0 | 13164 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13165 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13166 | `		break;` |
|        1 | 13167 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13168 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13169 | `		break;` |
|      ! 0 | 13170 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13171 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13172 | `		break;` |
|        1 | 13173 | `	default:` |
|        - | 13174 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13175 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13176 | `			"ValueError",` |
|        - | 13177 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13178 | `			);` |
|        - | 13179 | `	}` |
|       26 | 13180 | `	return PH7_OK;` |
|       16 | 13181 |  |
|        - | 13182 | `/*` |
|        - | 13183 | ` * bool assert(mixed $assertion)` |
|        - | 13184 | ` *  Checks if assertion is FALSE.` |
|        - | 13185 | ` * Parameter` |
|        - | 13186 | ` *  $assertion` |
|        - | 13187 | ` *    The assertion to test.` |
|        - | 13188 | ` * Return` |
|        - | 13189 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13190 | ` */` |
|       24 | 13191 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13192 |  |
|       26 | 13193 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13194 | `	int iFlags,iResult;` |
|        - | 13195 | `	const char *zDesc;` |
|        - | 13196 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 13197 | `	if( nArg < 1 ){` |
|        3 | 13198 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13199 | `			"ArgumentCountError",` |
|        - | 13200 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13201 | `			);` |
|        - | 13202 | `	}` |
|       24 | 13203 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 13204 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13205 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13206 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13207 | `		return PH7_OK;` |
|        - | 13208 | `	}` |
|        - | 13209 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 13210 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 13211 | `	if( !iResult ){` |
|        - | 13212 | `		/* Assertion failed */` |
|        - | 13213 | `		/* Extract optional description */` |
|       13 | 13214 | `		zDesc = 0;` |
|       13 | 13215 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13216 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13217 | `		}` |
|       13 | 13218 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13219 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13220 | `			ph7_value sFile,sLine;` |
|        - | 13221 | `			ph7_value *apCbArg[3];` |
|        - | 13222 | `			SyString *pFile;` |
|        - | 13223 | `			/* Extract the processed script */` |
|      ! 0 | 13224 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13225 | `			if( pFile == 0 ){` |
|      ! 0 | 13226 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13227 | `			}` |
|        - | 13228 | `			/* Invoke the callback */` |
|      ! 0 | 13229 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13230 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13231 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13232 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13233 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13234 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13235 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13236 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13237 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13238 | `		}` |
|       13 | 13239 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13240 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13241 | `			return PH7_ABORT;` |
|        - | 13242 | `		}` |
|        - | 13243 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 13244 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13245 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13246 | `				"AssertionError",` |
|        - | 13247 | `				"%s",` |
|        1 | 13248 | `				zDesc` |
|        - | 13249 | `				);` |
|      ! 0 | 13250 | `		}else{` |
|       11 | 13251 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13252 | `				"AssertionError",` |
|        - | 13253 | `				"assert(false)"` |
|        - | 13254 | `				);` |
|        - | 13255 | `		}` |
|        - | 13256 | `	}` |
|        - | 13257 | `	/* Assertion passed */` |
|       11 | 13258 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13259 | `	return PH7_OK;` |
|       14 | 13260 |  |
|        - | 13261 | `/*` |
|        - | 13262 | ` * Section:` |
|        - | 13263 | ` *  Error reporting functions.` |
|        - | 13264 | ` * Status:` |
|        - | 13265 | ` *    Stable.` |
|        - | 13266 | ` */` |
|        - | 13267 | `/*` |
|        - | 13268 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13269 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13270 | ` * Parameters` |
|        - | 13271 | ` *  $error_msg` |
|        - | 13272 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13273 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13274 | ` * $error_type` |
|        - | 13275 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13276 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13277 | ` * Return` |
|        - | 13278 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13279 | ` */` |
|       12 | 13280 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13281 |  |
|       14 | 13282 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 13283 | `	int rc = PH7_OK;` |
|       14 | 13284 | `	if( nArg > 0 ){` |
|        - | 13285 | `		const char *zErr;` |
|        - | 13286 | `		int nLen;` |
|        - | 13287 | `		/* Extract the error message */` |
|       12 | 13288 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 13289 | `		if( nArg > 1 ){` |
|        - | 13290 | `			/* Extract the error type */` |
|       12 | 13291 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 13292 | `			switch( nErr ){` |
|        1 | 13293 | `			case 1:   /* E_ERROR */` |
|        - | 13294 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13295 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13296 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13297 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13298 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13299 | `				break;` |
|        1 | 13300 | `			case 2:   /* E_WARNING */` |
|        - | 13301 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13302 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13303 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13304 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13305 | `				break;` |
|        3 | 13306 | `			default:` |
|        8 | 13307 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13308 | `				break;` |
|        - | 13309 | `			}` |
|        5 | 13310 | `		}` |
|        - | 13311 | `		/* Report error */` |
|       12 | 13312 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13313 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13314 | `			return rc;` |
|        - | 13315 | `		}` |
|        - | 13316 | `		/* Return true */` |
|       12 | 13317 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13318 | `	}else{` |
|        - | 13319 | `		/* Missing arguments,return FALSE */` |
|        3 | 13320 | `		ph7_result_bool(pCtx,0);` |
|        - | 13321 | `	}` |
|       14 | 13322 | `	return rc;` |
|        8 | 13323 |  |
|        - | 13324 | `/*` |
|        - | 13325 | ` * int error_reporting([int $level])` |
|        - | 13326 | ` *  Sets which PHP errors are reported.` |
|        - | 13327 | ` * Parameters` |
|        - | 13328 | ` *  $level` |
|        - | 13329 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13330 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13331 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13332 | ` *   levels will not always behave as expected.` |
|        - | 13333 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13334 | ` *   in the predefined constants.` |
|        - | 13335 | ` * Return` |
|        - | 13336 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13337 | ` *   parameter is given.` |
|        - | 13338 | ` */` |
|       32 | 13339 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13340 |  |
|       34 | 13341 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13342 | `	int nOld;` |
|        - | 13343 | `	/* Extract the old reporting level */` |
|       34 | 13344 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13345 | `	if( nArg > 0 ){` |
|        - | 13346 | `		int nNew;` |
|        - | 13347 | `		/* Extract the desired error reporting level */` |
|       28 | 13348 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13349 | `		if( !nNew ){` |
|        - | 13350 | `			/* Do not report errors at all */` |
|        5 | 13351 | `			pVm->bErrReport = 0;` |
|        3 | 13352 | `		}else{` |
|        - | 13353 | `			/* Report all errors */` |
|       24 | 13354 | `			pVm->bErrReport = 1;` |
|        - | 13355 | `		}` |
|       13 | 13356 | `	}` |
|        - | 13357 | `	/* Return the old level */` |
|       34 | 13358 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13359 | `	return PH7_OK;` |
|        2 | 13360 |  |
|        - | 13361 | `/*` |
|        - | 13362 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13363 | ` *  Send an error message somewhere.` |
|        - | 13364 | ` * Parameter` |
|        - | 13365 | ` *  $message` |
|        - | 13366 | ` *   The error message that should be logged.` |
|        - | 13367 | ` *  $message_type` |
|        - | 13368 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13369 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13370 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13371 | ` *       This is the default option.` |
|        - | 13372 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13373 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13374 | ` *    2  No longer an option.` |
|        - | 13375 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13376 | ` *       to the end of the message string.` |
|        - | 13377 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13378 | ` *  $destination` |
|        - | 13379 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13380 | ` *  $extra_headers` |
|        - | 13381 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13382 | ` * Return` |
|        - | 13383 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13384 | ` * NOTE:` |
|        - | 13385 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13386 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13387 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13388 | ` *  Otherwise this function is no-op.` |
|        - | 13389 | ` */` |
|        4 | 13390 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13391 |  |
|        - | 13392 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13393 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13394 | `	int iType = 0;` |
|        5 | 13395 | `	if( nArg < 1 ){` |
|        - | 13396 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13397 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13398 | `		return PH7_OK;` |
|        - | 13399 | `	}` |
|        5 | 13400 | `	if( pVm->xErrLog  ){` |
|        - | 13401 | `		/* Invoke the user callback */` |
|      ! 0 | 13402 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13403 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13404 | `		if( nArg > 1 ){` |
|      ! 0 | 13405 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13406 | `			if( nArg > 2 ){` |
|      ! 0 | 13407 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13408 | `				if( nArg > 3 ){` |
|      ! 0 | 13409 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13410 | `				}` |
|      ! 0 | 13411 | `			}` |
|      ! 0 | 13412 | `		}` |
|      ! 0 | 13413 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13414 | `	}` |
|        - | 13415 | `	/* Retun TRUE */` |
|        5 | 13416 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13417 | `	return PH7_OK;` |
|        3 | 13418 |  |
|        - | 13419 | `/*` |
|        - | 13420 | ` * bool restore_exception_handler(void)` |
|        - | 13421 | ` *  Restores the previously defined exception handler function.` |
|        - | 13422 | ` * Parameter` |
|        - | 13423 | ` *  None` |
|        - | 13424 | ` * Return` |
|        - | 13425 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13426 | ` */` |
|        4 | 13427 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13428 |  |
|        5 | 13429 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13430 | `	ph7_value *pOld,*pNew;` |
|        - | 13431 | `	/* Point to the old and the new handler */` |
|        5 | 13432 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13433 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13434 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13435 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13436 | `		SXUNUSED(apArg);` |
|        - | 13437 | `		/* No installed handler,return FALSE */` |
|        5 | 13438 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13439 | `		return PH7_OK;` |
|        - | 13440 | `	}` |
|        - | 13441 | `	/* Copy the old handler */` |
|      ! 0 | 13442 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13443 | `	PH7_MemObjRelease(pOld);` |
|        - | 13444 | `	/* Return TRUE */` |
|      ! 0 | 13445 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13446 | `	return PH7_OK;` |
|        3 | 13447 |  |
|        - | 13448 | `/*` |
|        - | 13449 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13450 | ` *  Sets a user-defined exception handler function.` |
|        - | 13451 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13452 | ` * NOTE` |
|        - | 13453 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13454 | ` *  the satndard PHP engine.` |
|        - | 13455 | ` * Parameters` |
|        - | 13456 | ` *  $exception_handler` |
|        - | 13457 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13458 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13459 | ` *   that was thrown.` |
|        - | 13460 | ` *  Note:` |
|        - | 13461 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13462 | ` * Return` |
|        - | 13463 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13464 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13465 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13466 | ` */` |
|        4 | 13467 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13468 |  |
|        6 | 13469 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13470 | `	ph7_value *pOld,*pNew;` |
|        - | 13471 | `	/* Point to the old and the new handler */` |
|        6 | 13472 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13473 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13474 | `	/* Return the old handler */` |
|        6 | 13475 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13476 | `	if( nArg > 0 ){` |
|        6 | 13477 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13478 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13479 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13480 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13481 | `		}else{` |
|        6 | 13482 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13483 | `			/* Install the new handler */` |
|        6 | 13484 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13485 | `		}` |
|        2 | 13486 | `	}` |
|        6 | 13487 | `	return PH7_OK;` |
|        2 | 13488 |  |
|        - | 13489 | `/*` |
|        - | 13490 | ` * bool restore_error_handler(void)` |
|        - | 13491 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13492 | ` * Parameters:` |
|        - | 13493 | ` *  None.` |
|        - | 13494 | ` * Return` |
|        - | 13495 | ` *  Always TRUE.` |
|        - | 13496 | ` */` |
|        6 | 13497 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13498 |  |
|        7 | 13499 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13500 | `	ph7_value *pOld,*pNew;` |
|        - | 13501 | `	/* Point to the old and the new handler */` |
|        7 | 13502 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13503 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13504 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13505 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13506 | `		SXUNUSED(apArg);` |
|        - | 13507 | `		/* No installed callback,return FALSE */` |
|        7 | 13508 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13509 | `		return PH7_OK;` |
|        - | 13510 | `	}` |
|        - | 13511 | `	/* Copy the old callback */` |
|      ! 0 | 13512 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13513 | `	PH7_MemObjRelease(pOld);` |
|        - | 13514 | `	/* Return TRUE */` |
|      ! 0 | 13515 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13516 | `	return PH7_OK;` |
|        4 | 13517 |  |
|        - | 13518 | `/*` |
|        - | 13519 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13520 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13521 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13522 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13523 | ` *  Sets a user-defined error handler function.` |
|        - | 13524 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13525 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13526 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13527 | ` *  conditions (using trigger_error()).` |
|        - | 13528 | ` * Parameters` |
|        - | 13529 | ` *  $error_handler` |
|        - | 13530 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13531 | ` *   describing the error.` |
|        - | 13532 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13533 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13534 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13535 | ` *   The function can be shown as:` |
|        - | 13536 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13537 | ` *     errno` |
|        - | 13538 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13539 | ` *   errstr` |
|        - | 13540 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13541 | ` *   errfile` |
|        - | 13542 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13543 | ` *     was raised in, as a string.` |
|        - | 13544 | ` *  Note:` |
|        - | 13545 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13546 | ` * Return` |
|        - | 13547 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13548 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13549 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13550 | ` */` |
|    10856 | 13551 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13552 |  |
|    10858 | 13553 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13554 | `	ph7_value *pOld,*pNew;` |
|        - | 13555 | `	/* Point to the old and the new handler */` |
|    10858 | 13556 | `	pOld = &pVm->aErrCB[0];` |
|    10858 | 13557 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13558 | `	/* Return the old handler */` |
|    10858 | 13559 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10858 | 13560 | `	if( nArg > 0 ){` |
|    10858 | 13561 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13562 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5423 | 13563 | `			PH7_MemObjRelease(pNew);` |
|     5423 | 13564 | `			ph7_result_bool(pCtx,1);` |
|     2712 | 13565 | `		}else{` |
|     5436 | 13566 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13567 | `			/* Install the new handler */` |
|     5436 | 13568 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13569 | `		}` |
|     5428 | 13570 | `	}` |
|    10858 | 13571 | `	return PH7_OK;` |
|        2 | 13572 |  |
|        - | 13573 | `/*` |
|        - | 13574 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13575 | ` *  Generates a backtrace.` |
|        - | 13576 | ` * Paramaeter` |
|        - | 13577 | ` *  $options` |
|        - | 13578 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13579 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13580 | ` *   all the function/method arguments, to save memory.` |
|        - | 13581 | ` * $limit` |
|        - | 13582 | ` *   (Not Used)` |
|        - | 13583 | ` * Return` |
|        - | 13584 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13585 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13586 | ` *          Name        Type      Description` |
|        - | 13587 | ` *          ------      ------     -----------` |
|        - | 13588 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13589 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13590 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13591 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13592 | ` *          object      object    The current object.` |
|        - | 13593 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13594 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13595 | ` */` |
|      926 | 13596 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13597 |  |
|      928 | 13598 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13599 | `	ph7_value *pArray;` |
|        - | 13600 | `	ph7_class *pClass;` |
|        - | 13601 | `	ph7_value *pValue;` |
|        - | 13602 | `	SyString *pFile;` |
|        - | 13603 | `	/* Create a new array */` |
|      928 | 13604 | `	pArray = ph7_context_new_array(pCtx);` |
|      928 | 13605 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      928 | 13606 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13607 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13608 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13609 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13610 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13611 | `		SXUNUSED(apArg);` |
|      ! 0 | 13612 | `		return PH7_OK;` |
|        - | 13613 | `	}` |
|        - | 13614 | `	/* Dump running function name and it's arguments  */` |
|      928 | 13615 | `	if( pVm->pFrame->pParent ){` |
|      928 | 13616 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13617 | `		ph7_vm_func *pFunc;` |
|        - | 13618 | `		ph7_value *pArg;` |
|      928 | 13619 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      928 | 13620 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      928 | 13621 | `		if( pFrame->pParent && pFunc ){` |
|      928 | 13622 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      928 | 13623 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      928 | 13624 | `			ph7_value_reset_string_cursor(pValue);` |
|      463 | 13625 | `		}` |
|        - | 13626 | `		/* Function arguments */` |
|      928 | 13627 | `		pArg = ph7_context_new_array(pCtx);` |
|      928 | 13628 | `		if( pArg  ){` |
|        - | 13629 | `			ph7_value *pObj;` |
|        - | 13630 | `			VmSlot *aSlot;` |
|        - | 13631 | `			sxu32 n;` |
|        - | 13632 | `			/* Start filling the array with the given arguments */` |
|      928 | 13633 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3710 | 13634 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2784 | 13635 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2784 | 13636 | `				if( pObj ){` |
|     2784 | 13637 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1391 | 13638 | `				}` |
|     1393 | 13639 | `			}` |
|        - | 13640 | `			/* Save the array */` |
|      928 | 13641 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      463 | 13642 | `		}` |
|      463 | 13643 | `	}` |
|      928 | 13644 | `	ph7_value_int(pValue,1);` |
|        - | 13645 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13646 | `	 * line numbers at run-time. )` |
|        - | 13647 | `	 */` |
|      928 | 13648 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13649 | `	/* Current processed script */` |
|      928 | 13650 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      928 | 13651 | `	if( pFile ){` |
|      928 | 13652 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      928 | 13653 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      928 | 13654 | `		ph7_value_reset_string_cursor(pValue);` |
|      463 | 13655 | `	}` |
|        - | 13656 | `	/* Top class */` |
|      928 | 13657 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      928 | 13658 | `	if( pClass ){` |
|      924 | 13659 | `		ph7_value_reset_string_cursor(pValue);` |
|      924 | 13660 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      924 | 13661 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      461 | 13662 | `	}` |
|        - | 13663 | `	/* Return the freshly created array */` |
|      928 | 13664 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13665 | `	/*` |
|        - | 13666 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13667 | `	 * as soon we return from this function.` |
|        - | 13668 | `	 */` |
|      928 | 13669 | `	return PH7_OK;` |
|      465 | 13670 |  |
|        - | 13671 | `/*` |
|        - | 13672 | ` * Generate a small backtrace.` |
|        - | 13673 | ` * Store the generated dump in the given BLOB` |
|        - | 13674 | ` */` |
|        4 | 13675 | `static int VmMiniBacktrace(` |
|        - | 13676 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13677 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13678 | `	)` |
|        1 | 13679 |  |
|        5 | 13680 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13681 | `	ph7_vm_func *pFunc;` |
|        - | 13682 | `	ph7_class *pClass;` |
|        - | 13683 | `	SyString *pFile;` |
|        - | 13684 | `	/* Called function */` |
|        5 | 13685 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13686 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13687 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13688 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13689 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13690 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13691 | `	}else{` |
|      ! 0 | 13692 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13693 | `	}` |
|        5 | 13694 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13695 | `	/* Current processed script */` |
|        5 | 13696 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13697 | `	if( pFile ){` |
|        5 | 13698 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13699 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13700 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13701 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13702 | `	}` |
|        - | 13703 | `	/* Top class */` |
|        5 | 13704 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13705 | `	if( pClass ){` |
|      ! 0 | 13706 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13707 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13708 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13709 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13710 | `	}` |
|        5 | 13711 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13712 | `	/* All done */` |
|        5 | 13713 | `	return SXRET_OK;` |
|        1 | 13714 |  |
|        - | 13715 | `/*` |
|        - | 13716 | ` * void debug_print_backtrace()` |
|        - | 13717 | ` *  Prints a backtrace` |
|        - | 13718 | ` * Parameters` |
|        - | 13719 | ` * None` |
|        - | 13720 | ` * Return` |
|        - | 13721 | ` * NULL` |
|        - | 13722 | ` */` |
|        2 | 13723 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13724 |  |
|        3 | 13725 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13726 | `	SyBlob sDump;` |
|        3 | 13727 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13728 | `	/* Generate the backtrace */` |
|        3 | 13729 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13730 | `	/* Output backtrace */` |
|        3 | 13731 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13732 | `	/* All done,cleanup */` |
|        3 | 13733 | `	SyBlobRelease(&sDump);` |
|        1 | 13734 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13735 | `	SXUNUSED(apArg);` |
|        3 | 13736 | `	return PH7_OK;` |
|        1 | 13737 |  |
|        - | 13738 | `/*` |
|        - | 13739 | ` * string debug_string_backtrace()` |
|        - | 13740 | ` *  Generate a backtrace` |
|        - | 13741 | ` * Parameters` |
|        - | 13742 | ` * None` |
|        - | 13743 | ` * Return` |
|        - | 13744 | ` *  A mini backtrace().` |
|        - | 13745 | ` * Note that this is a symisc extension.` |
|        - | 13746 | ` */` |
|        2 | 13747 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13748 |  |
|        3 | 13749 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13750 | `	SyBlob sDump;` |
|        3 | 13751 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13752 | `	/* Generate the backtrace */` |
|        3 | 13753 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13754 | `	/* Return the backtrace */` |
|        3 | 13755 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13756 | `	/* All done,cleanup */` |
|        3 | 13757 | `	SyBlobRelease(&sDump);` |
|        1 | 13758 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13759 | `	SXUNUSED(apArg);` |
|        3 | 13760 | `	return PH7_OK;` |
|        1 | 13761 |  |
|        - | 13762 | `/*` |
|        - | 13763 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13764 | ` * exception is triggered.` |
|        - | 13765 | ` */` |
|      512 | 13766 | `static sxi32 VmUncaughtException(` |
|        - | 13767 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13768 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13769 | `	)` |
|        1 | 13770 |  |
|        - | 13771 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13772 | `	int nArg = 1;` |
|        - | 13773 | `	sxi32 rc;` |
|      513 | 13774 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13775 | `		/* Nesting limit reached */` |
|      ! 0 | 13776 | `		return SXRET_OK;` |
|        - | 13777 | `	}` |
|        - | 13778 | `	/* Call any exception handler if available */` |
|      513 | 13779 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13780 | `	if( pThis ){` |
|        - | 13781 | `		/* Load the exception instance */` |
|      513 | 13782 | `		sArg.x.pOther = pThis;` |
|      513 | 13783 | `		pThis->iRef++;` |
|      513 | 13784 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13785 | `	}else{` |
|      ! 0 | 13786 | `		nArg = 0;` |
|        - | 13787 | `	}` |
|      513 | 13788 | `	apArg[0] = &sArg;` |
|        - | 13789 | `	/* Call the exception handler if available */` |
|      513 | 13790 | `	pVm->nExceptDepth++;` |
|      513 | 13791 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13792 | `	pVm->nExceptDepth--;` |
|      513 | 13793 | `	if( rc != SXRET_OK ){` |
|        - | 13794 | `		SyBlob sMsgBuf;` |
|      511 | 13795 | `		const char *zClass = "Exception";` |
|      511 | 13796 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13797 | `		const char *zMsg;` |
|        - | 13798 | `		sxu32 nMsg;` |
|        - | 13799 | `		const char *zFuncName;` |
|        - | 13800 | `		int nFuncLen;` |
|      511 | 13801 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13802 | `		if( pThis ){` |
|        - | 13803 | `			ph7_class_method *pGetMessage;` |
|        - | 13804 | `			ph7_value sMsg;` |
|        - | 13805 | `			const char *zTmp;` |
|        - | 13806 | `			int nTmp;` |
|      511 | 13807 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13808 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13809 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13810 | `			if( pGetMessage ){` |
|      511 | 13811 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13812 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13813 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13814 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13815 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13816 | `					}` |
|      255 | 13817 | `				}` |
|      511 | 13818 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13819 | `			}` |
|      255 | 13820 | `		}` |
|      511 | 13821 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13822 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13823 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13824 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13825 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13826 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13827 | `		rc = SXERR_ABORT;` |
|      255 | 13828 | `	}` |
|      513 | 13829 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13830 | `	return rc;` |
|      257 | 13831 |  |
|        - | 13832 | `/*` |
|        - | 13833 | ` * Throw a user exception.` |
|        - | 13834 | ` *` |
|        - | 13835 | ` * Exception dispatch follows this sequence:` |
|        - | 13836 | ` *` |
|        - | 13837 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13838 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13839 | ` *` |
|        - | 13840 | ` * 2. If NO catch matches:` |
|        - | 13841 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13842 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13843 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13844 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13845 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13846 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13847 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13848 | ` *` |
|        - | 13849 | ` * 3. If a catch DOES match:` |
|        - | 13850 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13851 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13852 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13853 | ` *       finally block.` |
|        - | 13854 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13855 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13856 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13857 | ` *       in pPendingException (step 2c).` |
|        - | 13858 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13859 | ` *    d. Run finally (if present).` |
|        - | 13860 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13861 | ` *       that handlers are restored and finally has run.` |
|        - | 13862 | ` */` |
|      856 | 13863 | `static sxi32 VmThrowException(` |
|        - | 13864 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13865 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13866 | `	)` |
|        2 | 13867 |  |
|        - | 13868 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13869 | `	ph7_exception **apException;` |
|        - | 13870 | `	ph7_exception *pException;` |
|        - | 13871 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13872 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13873 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      858 | 13874 | `	VmCoalesceDisarm(pVm);` |
|        - | 13875 | `	/* Point to the stack of loaded exceptions */` |
|      858 | 13876 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      858 | 13877 | `	pException = 0;` |
|      858 | 13878 | `	pCatch = 0;` |
|      858 | 13879 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13880 | `		ph7_exception_block *aCatch;` |
|        - | 13881 | `		ph7_class *pClass;` |
|        - | 13882 | `		SyString *aNames;` |
|        - | 13883 | `		sxu32 nNames;` |
|        - | 13884 | `		int matched;` |
|        - | 13885 | `		sxu32 j,k;` |
|        - | 13886 | `		/* Locate the appropriate block to execute */` |
|      338 | 13887 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      338 | 13888 | `		(void)SySetPop(&pVm->aException);` |
|      338 | 13889 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      346 | 13890 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13891 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      344 | 13892 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      344 | 13893 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      344 | 13894 | `			matched = 0;` |
|      370 | 13895 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13896 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13897 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13898 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      362 | 13899 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      362 | 13900 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13901 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13902 | `					continue;` |
|        - | 13903 | `				}` |
|      362 | 13904 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      336 | 13905 | `					matched = 1;` |
|      336 | 13906 | `					break;` |
|        - | 13907 | `				}` |
|       14 | 13908 | `			}` |
|      344 | 13909 | `			if( matched ){` |
|        - | 13910 | `				/* Catch block found,break immediately */` |
|      336 | 13911 | `				pCatch = &aCatch[j];` |
|      336 | 13912 | `				break;` |
|        - | 13913 | `			}` |
|        5 | 13914 | `		}` |
|      168 | 13915 | `	}` |
|        - | 13916 | `	/* Execute the cached block if available */` |
|      858 | 13917 | `	if( pCatch == 0 ){` |
|        - | 13918 | `		sxi32 rc;` |
|        - | 13919 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13920 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13921 | `			pException->iFinallyDone = 1;` |
|        3 | 13922 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13923 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13924 | `				return SXERR_ABORT;` |
|        - | 13925 | `			}` |
|        1 | 13926 | `		}` |
|        - | 13927 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13928 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13929 | `			/* Re-throw to the outer handler */` |
|        3 | 13930 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13931 | `		}` |
|        - | 13932 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13933 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13934 | `		 * exception instead of reporting it uncaught.` |
|        - | 13935 | `		 */` |
|      522 | 13936 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13937 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13938 | `			 * by looking for a catch frame on the stack.` |
|        - | 13939 | `			 */` |
|      522 | 13940 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13941 | `			int inCatch = 0;` |
|     1050 | 13942 | `			while( pF ){` |
|      538 | 13943 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13944 | `					inCatch = 1;` |
|        9 | 13945 | `					break;` |
|        - | 13946 | `				}` |
|      529 | 13947 | `				pF = pF->pParent;` |
|        1 | 13948 | `			}` |
|      522 | 13949 | `			if( inCatch ){` |
|        - | 13950 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13951 | `				pThis->iRef++;` |
|        9 | 13952 | `				pVm->pPendingException = pThis;` |
|        9 | 13953 | `				return SXRET_OK;` |
|        - | 13954 | `			}` |
|      256 | 13955 | `		}` |
|        - | 13956 | `		/* Truly uncaught */` |
|      513 | 13957 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13958 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13959 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13960 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13961 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13962 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13963 | `			}` |
|      ! 0 | 13964 | `		}` |
|      513 | 13965 | `		return rc;` |
|      ! 0 | 13966 | `	}else{` |
|      336 | 13967 | `		VmFrame *pFrame = pVm->pFrame;` |
|      336 | 13968 | `		ph7_exception **apSaved = 0;` |
|        - | 13969 | `		sxu32 nSavedCount;` |
|        - | 13970 | `		sxi32 rc;` |
|      336 | 13971 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      336 | 13972 | `		if( pException->pFrame == pFrame ){` |
|      236 | 13973 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      117 | 13974 | `		}` |
|        - | 13975 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13976 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13977 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13978 | `		 */` |
|      336 | 13979 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      336 | 13980 | `		if( nSavedCount > 0 ){` |
|       16 | 13981 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13982 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13983 | `			if( apSaved ){` |
|       16 | 13984 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13985 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13986 | `				SySetReset(&pVm->aException);` |
|        5 | 13987 | `			}` |
|        5 | 13988 | `		}` |
|        - | 13989 | `		/* Create a private frame first */` |
|      336 | 13990 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      336 | 13991 | `		if( rc == SXRET_OK ){` |
|      336 | 13992 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      336 | 13993 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      336 | 13994 | `			if( pObj ){` |
|      336 | 13995 | `				pThis->iRef++;` |
|      336 | 13996 | `				pObj->x.pOther = pThis;` |
|      336 | 13997 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      167 | 13998 | `			}` |
|        - | 13999 | `			/* Execute the catch block */` |
|      336 | 14000 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 14001 | `			/* Leave the frame */` |
|      336 | 14002 | `			VmLeaveFrame(&(*pVm));` |
|      167 | 14003 | `		}` |
|        - | 14004 | `		/* Restore the outer exception handlers */` |
|      336 | 14005 | `		if( apSaved ){` |
|        - | 14006 | `			sxu32 k;` |
|        - | 14007 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14008 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14009 | `			 * Restore the original outer entries.` |
|        - | 14010 | `			 */` |
|       11 | 14011 | `			SySetReset(&pVm->aException);` |
|       21 | 14012 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 14013 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 14014 | `			}` |
|       11 | 14015 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 14016 | `		}` |
|        - | 14017 | `		/* Execute the finally block after catch */` |
|      336 | 14018 | `		if( pException->iHasFinally ){` |
|       16 | 14019 | `			pException->iFinallyDone = 1;` |
|        - | 14020 | `			{` |
|       16 | 14021 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 14022 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14023 | `					return SXERR_ABORT;` |
|        - | 14024 | `				}` |
|        - | 14025 | `			}` |
|        7 | 14026 | `		}` |
|      336 | 14027 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14028 | `			return SXERR_ABORT;` |
|        - | 14029 | `		}` |
|        - | 14030 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14031 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14032 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 14033 | `		 */` |
|      336 | 14034 | `		if( pVm->pPendingException ){` |
|        9 | 14035 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14036 | `			pVm->pPendingException = 0;` |
|        9 | 14037 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14038 | `		}` |
|        - | 14039 | `	}` |
|        - | 14040 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14041 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14042 | `	 */` |
|      328 | 14043 | `	return SXRET_OK;` |
|      430 | 14044 |  |
|        - | 14045 | `/*` |
|        - | 14046 | ` * Section:` |
|        - | 14047 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14048 | ` * Status:` |
|        - | 14049 | ` *    Stable.` |
|        - | 14050 | ` */` |
|        - | 14051 | `/*` |
|        - | 14052 | ` * string ph7version(void)` |
|        - | 14053 | ` *  Returns the running version of the PH7 version.` |
|        - | 14054 | ` * Parameters` |
|        - | 14055 | ` *  None` |
|        - | 14056 | ` * Return` |
|        - | 14057 | ` * Current PH7 version.` |
|        - | 14058 | ` */` |
|        2 | 14059 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14060 |  |
|        1 | 14061 | `	SXUNUSED(nArg);` |
|        1 | 14062 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14063 | `	/* Current engine version */` |
|        3 | 14064 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14065 | `	return PH7_OK;` |
|        1 | 14066 |  |
|        - | 14067 | `/*` |
|        - | 14068 | ` * string phpversion([ string $extension ])` |
|        - | 14069 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14070 | ` * Parameters` |
|        - | 14071 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14072 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14073 | ` * Return` |
|        - | 14074 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14075 | ` */` |
|        4 | 14076 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14077 |  |
|        2 | 14078 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14079 | `	if( nArg > 0 ){` |
|      ! 0 | 14080 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14081 | `		return PH7_OK;` |
|        - | 14082 | `	}` |
|        5 | 14083 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14084 | `	return PH7_OK;` |
|        3 | 14085 |  |
|        - | 14086 | `/*` |
|        - | 14087 | ` * string php_sapi_name(void)` |
|        - | 14088 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14089 | ` * Parameters` |
|        - | 14090 | ` *  None` |
|        - | 14091 | ` * Return` |
|        - | 14092 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14093 | ` */` |
|        2 | 14094 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14095 |  |
|        3 | 14096 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14097 | `	SXUNUSED(nArg);` |
|        1 | 14098 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14099 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14100 | `	return PH7_OK;` |
|        1 | 14101 |  |
|        - | 14102 | `/*` |
|        - | 14103 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14104 | ` */` |
|        - | 14105 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14106 | ` "<html><head>"\` |
|        - | 14107 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14108 | ` "<style type=\"text/css\">"\` |
|        - | 14109 | ` "div {"\` |
|        - | 14110 | `     "border: 1px solid #cccccc;"\` |
|        - | 14111 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14112 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14113 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14114 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14115 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14116 | `     "-o-border-radius: 10px;"\` |
|        - | 14117 | `     "border-radius: 10px;"\` |
|        - | 14118 | `     "padding-left: 2em;"\` |
|        - | 14119 | `     "background-color: white;"\` |
|        - | 14120 | `     "margin-left: auto;"\` |
|        - | 14121 | `     "font-family: verdana;"\` |
|        - | 14122 | `     "padding-right: 2em;"\` |
|        - | 14123 | `     "margin-right: auto;"\` |
|        - | 14124 | `     "}"\` |
|        - | 14125 | `     "body {"\` |
|        - | 14126 | `     "padding: 0.2em;"\` |
|        - | 14127 | `     "font-style: normal;"\` |
|        - | 14128 | `     "font-size: medium;"\` |
|        - | 14129 | `     "background-color: #f2f2f2;"\` |
|        - | 14130 | `     "}"\` |
|        - | 14131 | `     "hr {"\` |
|        - | 14132 | `     "border-style: solid none none;"\` |
|        - | 14133 | `     "border-width: 1px medium medium;"\` |
|        - | 14134 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14135 | `     "height: 1px;"\` |
|        - | 14136 | `     "}"\` |
|        - | 14137 | `     "a {"\` |
|        - | 14138 | `     "color: #3366cc;"\` |
|        - | 14139 | `     "text-decoration: none;"\` |
|        - | 14140 | `     "}"\` |
|        - | 14141 | `     "a:hover {"\` |
|        - | 14142 | `     "color: #999999;"\` |
|        - | 14143 | `     "}"\` |
|        - | 14144 | `     "a:active {"\` |
|        - | 14145 | `     "color: #663399;"\` |
|        - | 14146 | `     "}"\` |
|        - | 14147 | `     "h1 {"\` |
|        - | 14148 | `     "margin: 0;"\` |
|        - | 14149 | `     "padding: 0;"\` |
|        - | 14150 | `     "font-family: Verdana;"\` |
|        - | 14151 | `     "font-weight: bold;"\` |
|        - | 14152 | `     "font-style: normal;"\` |
|        - | 14153 | `     "font-size: medium;"\` |
|        - | 14154 | `     "text-transform: capitalize;"\` |
|        - | 14155 | `     "color: #0a328c;"\` |
|        - | 14156 | `     "}"\` |
|        - | 14157 | `     "p {"\` |
|        - | 14158 | `     "margin: 0 auto;"\` |
|        - | 14159 | `     "font-size: medium;"\` |
|        - | 14160 | `     "font-style: normal;"\` |
|        - | 14161 | `     "font-family: verdana;"\` |
|        - | 14162 | `     "}"\` |
|        - | 14163 | `"</style></head><body>"\` |
|        - | 14164 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14165 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14166 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14167 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14168 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14169 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14170 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14171 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14172 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14173 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14174 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14175 |  |
|        - | 14176 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14177 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14178 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14179 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14180 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14181 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14182 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14183 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14184 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14185 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14186 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14187 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14188 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14189 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14190 |  |
|        - | 14191 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14192 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14193 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14194 | `"&nbsp;*<br>"\` |
|        - | 14195 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14196 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14197 | `"&nbsp;* are met:<br>"\` |
|        - | 14198 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14199 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14200 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14201 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14202 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14203 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14204 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14205 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14206 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14207 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14208 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14209 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14210 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14211 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14212 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14213 | `"&nbsp;*<br>"\` |
|        - | 14214 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14215 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14216 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14217 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14218 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14219 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14220 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14221 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14222 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14223 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14224 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14225 | `"&nbsp;*/<br>"\` |
|        - | 14226 | `"</span></small></small></p>"\` |
|        - | 14227 | `"</div></body></html>"` |
|        - | 14228 | `/*` |
|        - | 14229 | ` * bool ph7credits(void)` |
|        - | 14230 | ` * bool ph7info(void)` |
|        - | 14231 | ` * bool ph7copyright(void)` |
|        - | 14232 | ` *  Prints out the credits for PH7 engine` |
|        - | 14233 | ` * Parameters` |
|        - | 14234 | ` *  None` |
|        - | 14235 | ` * Return` |
|        - | 14236 | ` *  Always TRUE` |
|        - | 14237 | ` */` |
|        2 | 14238 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14239 |  |
|        3 | 14240 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14241 | `	/* Expand the HTML page above*/` |
|        3 | 14242 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14243 | `	ph7_context_output_format(` |
|        1 | 14244 | `		pCtx,` |
|        - | 14245 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14246 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14247 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14248 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14249 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14250 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14251 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14252 | `#ifdef __WINNT__` |
|        - | 14253 | `		"Windows NT"` |
|        - | 14254 | `#elif defined(__UNIXES__)` |
|        - | 14255 | `		"UNIX-Like"` |
|        - | 14256 | `#else` |
|        - | 14257 | `		"Other OS"` |
|        - | 14258 | `#endif` |
|        - | 14259 | `		);` |
|        3 | 14260 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14261 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14262 | `	SXUNUSED(apArg);` |
|        - | 14263 | `	/* Return TRUE */` |
|        - | 14264 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14265 | `	return PH7_OK;` |
|        1 | 14266 |  |
|        - | 14267 | `/*` |
|        - | 14268 | ` * Section:` |
|        - | 14269 | ` *    URL related routines.` |
|        - | 14270 | ` * Status:` |
|        - | 14271 | ` *    Stable.` |
|        - | 14272 | ` */` |
|        - | 14273 | `/*` |
|        - | 14274 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14275 | ` *  Parse a URL and return its fields.` |
|        - | 14276 | ` * Parameters` |
|        - | 14277 | ` *  $url` |
|        - | 14278 | ` *   The URL to parse.` |
|        - | 14279 | ` * $component` |
|        - | 14280 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14281 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14282 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14283 | ` *  in which case the return value will be an integer).` |
|        - | 14284 | ` * Return` |
|        - | 14285 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14286 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14287 | ` *  this array are:` |
|        - | 14288 | ` *   scheme - e.g. http` |
|        - | 14289 | ` *   host` |
|        - | 14290 | ` *   port` |
|        - | 14291 | ` *   user` |
|        - | 14292 | ` *   pass` |
|        - | 14293 | ` *   path` |
|        - | 14294 | ` *   query - after the question mark ?` |
|        - | 14295 | ` *   fragment - after the hashmark #` |
|        - | 14296 | ` * Note:` |
|        - | 14297 | ` *  FALSE is returned on failure.` |
|        - | 14298 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14299 | ` *  with the standard PHP engine.` |
|        - | 14300 | ` */` |
|       28 | 14301 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14302 |  |
|        - | 14303 | `	const char *zStr; /* Input string */` |
|        - | 14304 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14305 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14306 | `	int nLen;` |
|        - | 14307 | `	sxi32 rc;` |
|       29 | 14308 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14309 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14310 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14311 | `		return PH7_OK;` |
|        - | 14312 | `	}` |
|        - | 14313 | `	/* Extract the given URI */` |
|       29 | 14314 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14315 | `	if( nLen < 1 ){` |
|        - | 14316 | `		/* Nothing to process,return FALSE */` |
|        3 | 14317 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14318 | `		return PH7_OK;` |
|        - | 14319 | `	}` |
|        - | 14320 | `	/* Get a parse */` |
|       27 | 14321 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14322 | `	if( rc != SXRET_OK ){` |
|        - | 14323 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14324 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14325 | `		return PH7_OK;` |
|        - | 14326 | `	}` |
|       27 | 14327 | `	if( nArg > 1 ){` |
|      ! 0 | 14328 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14329 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14330 | `		switch(nComponent){` |
|      ! 0 | 14331 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14332 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14333 | `			if( pComp->nByte < 1 ){` |
|        - | 14334 | `				/* No available value,return NULL */` |
|      ! 0 | 14335 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14336 | `			}else{` |
|      ! 0 | 14337 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14338 | `			}` |
|      ! 0 | 14339 | `			break;` |
|      ! 0 | 14340 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14341 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14342 | `			if( pComp->nByte < 1 ){` |
|        - | 14343 | `				/* No available value,return NULL */` |
|      ! 0 | 14344 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14345 | `			}else{` |
|      ! 0 | 14346 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14347 | `			}` |
|      ! 0 | 14348 | `			break;` |
|      ! 0 | 14349 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14350 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14351 | `			if( pComp->nByte < 1 ){` |
|        - | 14352 | `				/* No available value,return NULL */` |
|      ! 0 | 14353 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14354 | `			}else{` |
|      ! 0 | 14355 | `				int iPort = 0;` |
|        - | 14356 | `				/* Cast the value to integer */` |
|      ! 0 | 14357 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14358 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14359 | `			}` |
|      ! 0 | 14360 | `			break;` |
|      ! 0 | 14361 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14362 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14363 | `			if( pComp->nByte < 1 ){` |
|        - | 14364 | `				/* No available value,return NULL */` |
|      ! 0 | 14365 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14366 | `			}else{` |
|      ! 0 | 14367 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14368 | `			}` |
|      ! 0 | 14369 | `			break;` |
|      ! 0 | 14370 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14371 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14372 | `			if( pComp->nByte < 1 ){` |
|        - | 14373 | `				/* No available value,return NULL */` |
|      ! 0 | 14374 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14375 | `			}else{` |
|      ! 0 | 14376 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14377 | `			}` |
|      ! 0 | 14378 | `			break;` |
|      ! 0 | 14379 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14380 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14381 | `			if( pComp->nByte < 1 ){` |
|        - | 14382 | `				/* No available value,return NULL */` |
|      ! 0 | 14383 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14384 | `			}else{` |
|      ! 0 | 14385 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14386 | `			}` |
|      ! 0 | 14387 | `			break;` |
|      ! 0 | 14388 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14389 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14390 | `			if( pComp->nByte < 1 ){` |
|        - | 14391 | `				/* No available value,return NULL */` |
|      ! 0 | 14392 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14393 | `			}else{` |
|      ! 0 | 14394 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14395 | `			}` |
|      ! 0 | 14396 | `			break;` |
|      ! 0 | 14397 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14398 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14399 | `			if( pComp->nByte < 1 ){` |
|        - | 14400 | `				/* No available value,return NULL */` |
|      ! 0 | 14401 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14402 | `			}else{` |
|      ! 0 | 14403 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14404 | `			}` |
|      ! 0 | 14405 | `			break;` |
|      ! 0 | 14406 | `		default:` |
|        - | 14407 | `			/* No such entry,return NULL */` |
|      ! 0 | 14408 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14409 | `			break;` |
|        - | 14410 | `		}` |
|      ! 0 | 14411 | `	}else{` |
|        - | 14412 | `		ph7_value *pArray,*pValue;` |
|        - | 14413 | `		/* Return an associative array */` |
|       27 | 14414 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14415 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14416 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14417 | `			/* Out of memory */` |
|      ! 0 | 14418 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14419 | `			/* Return false */` |
|      ! 0 | 14420 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14421 | `			return PH7_OK;` |
|        - | 14422 | `		}` |
|        - | 14423 | `		/* Fill the array */` |
|       27 | 14424 | `		pComp = &sURI.sScheme;` |
|       27 | 14425 | `		if( pComp->nByte > 0 ){` |
|       19 | 14426 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14427 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14428 | `		}` |
|        - | 14429 | `		/* Reset the string cursor */` |
|       27 | 14430 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14431 | `		pComp = &sURI.sHost;` |
|       27 | 14432 | `		if( pComp->nByte > 0 ){` |
|       25 | 14433 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14434 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14435 | `		}` |
|        - | 14436 | `		/* Reset the string cursor */` |
|       27 | 14437 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14438 | `		pComp = &sURI.sPort;` |
|       27 | 14439 | `		if( pComp->nByte > 0 ){` |
|       11 | 14440 | `			int iPort = 0;/* cc warning */` |
|        - | 14441 | `			/* Convert to integer */` |
|       11 | 14442 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14443 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14444 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14445 | `		}` |
|        - | 14446 | `		/* Reset the string cursor */` |
|       27 | 14447 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14448 | `		pComp = &sURI.sUser;` |
|       27 | 14449 | `		if( pComp->nByte > 0 ){` |
|        7 | 14450 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14451 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14452 | `		}` |
|        - | 14453 | `		/* Reset the string cursor */` |
|       27 | 14454 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14455 | `		pComp = &sURI.sPass;` |
|       27 | 14456 | `		if( pComp->nByte > 0 ){` |
|        7 | 14457 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14458 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14459 | `		}` |
|        - | 14460 | `		/* Reset the string cursor */` |
|       27 | 14461 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14462 | `		pComp = &sURI.sPath;` |
|       27 | 14463 | `		if( pComp->nByte > 0 ){` |
|       17 | 14464 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14465 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14466 | `		}` |
|        - | 14467 | `		/* Reset the string cursor */` |
|       27 | 14468 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14469 | `		pComp = &sURI.sQuery;` |
|       27 | 14470 | `		if( pComp->nByte > 0 ){` |
|        5 | 14471 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14472 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14473 | `		}` |
|        - | 14474 | `		/* Reset the string cursor */` |
|       27 | 14475 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14476 | `		pComp = &sURI.sFragment;` |
|       27 | 14477 | `		if( pComp->nByte > 0 ){` |
|        5 | 14478 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14479 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14480 | `		}` |
|        - | 14481 | `		/* Return the created array */` |
|       27 | 14482 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14483 | `		/* NOTE:` |
|        - | 14484 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14485 | `		 * automatically as soon we return from this function.` |
|        - | 14486 | `		 */` |
|        - | 14487 | `	}` |
|        - | 14488 | `	/* All done */` |
|       27 | 14489 | `	return PH7_OK;` |
|       15 | 14490 |  |
|        - | 14491 | `/*` |
|        - | 14492 | ` * Section:` |
|        - | 14493 | ` *   Array related routines.` |
|        - | 14494 | ` * Status:` |
|        - | 14495 | ` *    Stable.` |
|        - | 14496 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14497 | ` *  Array related functions that need access to the underlying` |
|        - | 14498 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14499 | ` */` |
|        - | 14500 | `/*` |
|        - | 14501 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14502 | ` * of the following structure.` |
|        - | 14503 | ` */` |
|        - | 14504 | `struct compact_data` |
|        - | 14505 |  |
|        - | 14506 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14507 | `	int nRecCount;      /* Recursion count */` |
|        - | 14508 | `};` |
|        - | 14509 | `/*` |
|        - | 14510 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14511 | ` */` |
|      ! 0 | 14512 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14513 |  |
|      ! 0 | 14514 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14515 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14516 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14517 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14518 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14519 | `		SyString sVar;` |
|      ! 0 | 14520 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14521 | `		if( sVar.nByte > 0 ){` |
|        - | 14522 | `			/* Query the current frame */` |
|      ! 0 | 14523 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14524 | `			/* ^` |
|        - | 14525 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14526 | `			 */` |
|      ! 0 | 14527 | `			if( pKey ){` |
|        - | 14528 | `				/* Perform the insertion */` |
|      ! 0 | 14529 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14530 | `			}` |
|      ! 0 | 14531 | `		}` |
|      ! 0 | 14532 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14533 | `		int rc;` |
|        - | 14534 | `		/* Recursively traverse this array */` |
|      ! 0 | 14535 | `		pData->nRecCount++;` |
|      ! 0 | 14536 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14537 | `		pData->nRecCount--;` |
|      ! 0 | 14538 | `		return rc;` |
|        - | 14539 | `	}` |
|      ! 0 | 14540 | `	return SXRET_OK;` |
|      ! 0 | 14541 |  |
|        - | 14542 | `/*` |
|        - | 14543 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14544 | ` *  Create array containing variables and their values.` |
|        - | 14545 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14546 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14547 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14548 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14549 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14550 | ` * Parameters` |
|        - | 14551 | ` *  $varname` |
|        - | 14552 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14553 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14554 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14555 | ` *   it recursively.` |
|        - | 14556 | ` * Return` |
|        - | 14557 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14558 | ` */` |
|        2 | 14559 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14560 |  |
|        - | 14561 | `	ph7_value *pArray,*pObj;` |
|        3 | 14562 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14563 | `	const char *zName;` |
|        - | 14564 | `	SyString sVar;` |
|        - | 14565 | `	int i,nLen;` |
|        3 | 14566 | `	if( nArg < 1 ){` |
|        - | 14567 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14568 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14569 | `		return PH7_OK;` |
|        - | 14570 | `	}` |
|        - | 14571 | `	/* Create the array */` |
|        3 | 14572 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14573 | `	if( pArray == 0 ){` |
|        - | 14574 | `		/* Out of memory */` |
|      ! 0 | 14575 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14576 | `		/* Return NULL */` |
|      ! 0 | 14577 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14578 | `		return PH7_OK;` |
|        - | 14579 | `	}` |
|        - | 14580 | `	/* Perform the requested operation */` |
|        7 | 14581 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14582 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14583 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14584 | `				struct compact_data sData;` |
|      ! 0 | 14585 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14586 | `				/* Recursively walk the array */` |
|      ! 0 | 14587 | `				sData.nRecCount = 0;` |
|      ! 0 | 14588 | `				sData.pArray = pArray;` |
|      ! 0 | 14589 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14590 | `			}` |
|      ! 0 | 14591 | `		}else{` |
|        - | 14592 | `			/* Extract variable name */` |
|        5 | 14593 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14594 | `			if( nLen > 0 ){` |
|        5 | 14595 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14596 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14597 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14598 | `				if( pObj ){` |
|        5 | 14599 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14600 | `				}` |
|        2 | 14601 | `			}` |
|        - | 14602 | `		}` |
|        3 | 14603 | `	}` |
|        - | 14604 | `	/* Return the array */` |
|        3 | 14605 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14606 | `	return PH7_OK;` |
|        2 | 14607 |  |
|        - | 14608 | `/*` |
|        - | 14609 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14610 | ` * of the following structure.` |
|        - | 14611 | ` */` |
|        - | 14612 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14613 | `struct extract_aux_data` |
|        - | 14614 |  |
|        - | 14615 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14616 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14617 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14618 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14619 | `	int iFlags;           /* Control flags */` |
|        - | 14620 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14621 | `};` |
|        - | 14622 | `/* Forward declaration */` |
|        - | 14623 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14624 | `/*` |
|        - | 14625 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14626 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14627 | ` * Parameters` |
|        - | 14628 | ` * $var_array` |
|        - | 14629 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14630 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14631 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14632 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14633 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14634 | ` * $extract_type` |
|        - | 14635 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14636 | ` *  It can be one of the following values:` |
|        - | 14637 | ` *   EXTR_OVERWRITE` |
|        - | 14638 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14639 | ` *   EXTR_SKIP` |
|        - | 14640 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14641 | ` *   EXTR_PREFIX_SAME` |
|        - | 14642 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14643 | ` *   EXTR_PREFIX_ALL` |
|        - | 14644 | ` *       Prefix all variable names with prefix.` |
|        - | 14645 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14646 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14647 | ` *   EXTR_IF_EXISTS` |
|        - | 14648 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14649 | ` *       otherwise do nothing.` |
|        - | 14650 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14651 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14652 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14653 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14654 | ` *      the current symbol table.` |
|        - | 14655 | ` * $prefix` |
|        - | 14656 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14657 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14658 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14659 | ` *  underscore character.` |
|        - | 14660 | ` * Return` |
|        - | 14661 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14662 | ` */` |
|        4 | 14663 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14664 |  |
|        - | 14665 | `	extract_aux_data sAux;` |
|        - | 14666 | `	ph7_hashmap *pMap;` |
|        5 | 14667 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14668 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14669 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14670 | `		return PH7_OK;` |
|        - | 14671 | `	}` |
|        - | 14672 | `	/* Point to the target hashmap */` |
|        5 | 14673 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14674 | `	if( pMap->nEntry < 1 ){` |
|        - | 14675 | `		/* Empty map,return  0 */` |
|      ! 0 | 14676 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14677 | `		return PH7_OK;` |
|        - | 14678 | `	}` |
|        - | 14679 | `	/* Prepare the aux data */` |
|        5 | 14680 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14681 | `	if( nArg > 1 ){` |
|        3 | 14682 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14683 | `		if( nArg > 2 ){` |
|      ! 0 | 14684 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14685 | `		}` |
|        1 | 14686 | `	}` |
|        5 | 14687 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14688 | `	/* Invoke the worker callback */` |
|        5 | 14689 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14690 | `	/* Number of variables successfully imported */` |
|        5 | 14691 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14692 | `	return PH7_OK;` |
|        3 | 14693 |  |
|        - | 14694 | `/*` |
|        - | 14695 | ` * Worker callback for the [extract()] function defined` |
|        - | 14696 | ` * below.` |
|        - | 14697 | ` */` |
|        8 | 14698 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14699 |  |
|        9 | 14700 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14701 | `	int iFlags = pAux->iFlags;` |
|        9 | 14702 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14703 | `	ph7_value *pObj;` |
|        - | 14704 | `	SyString sVar;` |
|        9 | 14705 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14706 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14707 | `	}` |
|        - | 14708 | `	/* Perform a string cast */` |
|        9 | 14709 | `	PH7_MemObjToString(pKey);` |
|        9 | 14710 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14711 | `		/* Unavailable variable name */` |
|      ! 0 | 14712 | `		return SXRET_OK;` |
|        - | 14713 | `	}` |
|        9 | 14714 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14715 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14716 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14717 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14718 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14719 | `			);` |
|      ! 0 | 14720 | `	}else{` |
|       13 | 14721 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14722 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14723 | `	}` |
|        9 | 14724 | `	sVar.zString = pAux->zWorker;` |
|        - | 14725 | `	/* Try to extract the variable */` |
|        9 | 14726 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14727 | `	if( pObj ){` |
|        - | 14728 | `		/* Collision */` |
|        5 | 14729 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14730 | `			return SXRET_OK;` |
|        - | 14731 | `		}` |
|        5 | 14732 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14733 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14734 | `				/* Already prefixed */` |
|      ! 0 | 14735 | `				return SXRET_OK;` |
|        - | 14736 | `			}` |
|      ! 0 | 14737 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14738 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14739 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14740 | `				);` |
|      ! 0 | 14741 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14742 | `		}` |
|        3 | 14743 | `	}else{` |
|        - | 14744 | `		/* Create the variable */` |
|        5 | 14745 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14746 | `	}` |
|        9 | 14747 | `	if( pObj ){` |
|        - | 14748 | `		/* Overwrite the old value */` |
|        9 | 14749 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14750 | `		/* Increment counter */` |
|        9 | 14751 | `		pAux->iCount++;` |
|        4 | 14752 | `	}` |
|        9 | 14753 | `	return SXRET_OK;` |
|        5 | 14754 |  |
|        - | 14755 | `/*` |
|        - | 14756 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14757 | ` * defined below.` |
|        - | 14758 | ` */` |
|        2 | 14759 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14760 |  |
|        3 | 14761 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14762 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14763 | `	ph7_value *pObj;` |
|        - | 14764 | `	SyString sVar;` |
|        - | 14765 | `	/* Perform a string cast */` |
|        3 | 14766 | `	PH7_MemObjToString(pKey);` |
|        3 | 14767 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14768 | `		/* Unavailable variable name */` |
|      ! 0 | 14769 | `		return SXRET_OK;` |
|        - | 14770 | `	}` |
|        3 | 14771 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14772 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14773 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14774 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14775 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14776 | `			);` |
|        2 | 14777 | `	}else{` |
|      ! 0 | 14778 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14779 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14780 | `	}` |
|        3 | 14781 | `	sVar.zString = pAux->zWorker;` |
|        - | 14782 | `	/* Extract the variable */` |
|        3 | 14783 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14784 | `	if( pObj ){` |
|        3 | 14785 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14786 | `	}` |
|        3 | 14787 | `	return SXRET_OK;` |
|        2 | 14788 |  |
|        - | 14789 | `/*` |
|        - | 14790 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14791 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14792 | ` * Parameters` |
|        - | 14793 | ` * $types` |
|        - | 14794 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14795 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14796 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14797 | ` *  POST includes the POST uploaded file information.` |
|        - | 14798 | ` *  Note:` |
|        - | 14799 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14800 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14801 | ` * $prefix` |
|        - | 14802 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14803 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14804 | ` *  variable named $pref_userid.` |
|        - | 14805 | ` * Return` |
|        - | 14806 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14807 | ` */` |
|        2 | 14808 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14809 |  |
|        - | 14810 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14811 | `	extract_aux_data sAux;` |
|        - | 14812 | `	int nLen,nPrefixLen;` |
|        - | 14813 | `	ph7_value *pSuper;` |
|        - | 14814 | `	ph7_vm *pVm;` |
|        - | 14815 | `	/* By default import only $_GET variables  */` |
|        3 | 14816 | `	zImport = "G";` |
|        3 | 14817 | `	nLen = (int)sizeof(char);` |
|        3 | 14818 | `	zPrefix = 0;` |
|        3 | 14819 | `	nPrefixLen = 0;` |
|        3 | 14820 | `	if( nArg > 0 ){` |
|        3 | 14821 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14822 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14823 | `		}` |
|        3 | 14824 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14825 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14826 | `		}` |
|        1 | 14827 | `	}` |
|        - | 14828 | `	/* Point to the underlying VM */` |
|        3 | 14829 | `	pVm = pCtx->pVm;` |
|        - | 14830 | `	/* Initialize the aux data */` |
|        3 | 14831 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14832 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14833 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14834 | `	sAux.pVm = pVm;` |
|        - | 14835 | `	/* Extract */` |
|        3 | 14836 | `	zEnd = &zImport[nLen];` |
|        5 | 14837 | `	while( zImport < zEnd ){` |
|        3 | 14838 | `		int c = zImport[0];` |
|        3 | 14839 | `		pSuper = 0;` |
|        3 | 14840 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14841 | `			/* Import $_GET variables */` |
|        3 | 14842 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14843 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14844 | `			/* Import $_POST variables */` |
|      ! 0 | 14845 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14846 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14847 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14848 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14849 | `		}` |
|        3 | 14850 | `		if( pSuper ){` |
|        - | 14851 | `			/* Iterate throw array entries */` |
|        3 | 14852 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14853 | `		}` |
|        - | 14854 | `		/* Advance the cursor */` |
|        3 | 14855 | `		zImport++;` |
|        1 | 14856 | `	}` |
|        - | 14857 | `	/* All done,return TRUE*/` |
|        3 | 14858 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14859 | `	return PH7_OK;` |
|        1 | 14860 |  |
|        - | 14861 | `/*` |
|        - | 14862 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14863 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14864 | ` * information.` |
|        - | 14865 | ` */` |
|    12726 | 14866 | `static sxi32 VmEvalChunk(` |
|        - | 14867 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14868 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14869 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14870 | `	int iFlags,         /* Compile flag */` |
|        - | 14871 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14872 | `	)` |
|        2 | 14873 |  |
|        - | 14874 | `	SySet *pByteCode,aByteCode;` |
|        - | 14875 | `	SyBlob sSavedNs;` |
|    12728 | 14876 | `	ProcConsumer xErr = 0;` |
|    12728 | 14877 | `	void *pErrData = 0;` |
|        - | 14878 | `	/* Initialize bytecode container */` |
|    12728 | 14879 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12728 | 14880 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14881 | `	/* Reset the code generator */` |
|    12728 | 14882 | `	if( bTrueReturn ){` |
|        - | 14883 | `		/* Included file,log compile-time errors */` |
|     9566 | 14884 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9566 | 14885 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4782 | 14886 | `	}` |
|    12728 | 14887 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14888 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14889 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14890 | `	 * the caller's namespace is restored. */` |
|    12728 | 14891 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12728 | 14892 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12728 | 14893 | `	if( bTrueReturn ){` |
|        - | 14894 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9566 | 14895 | `		SyBlobReset(&pVm->sNamespace);` |
|     4782 | 14896 | `	}` |
|        - | 14897 | `	/* Swap bytecode container */` |
|    12728 | 14898 | `	pByteCode = pVm->pByteContainer;` |
|    12728 | 14899 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14900 | `	/* Compile the chunk */` |
|    12728 | 14901 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19091 | 14902 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14903 | `		/* Compilation error,return false */` |
|        3 | 14904 | `		if( pCtx ){` |
|        3 | 14905 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14906 | `		}` |
|        2 | 14907 | `	}else{` |
|        - | 14908 | `		/* Mount any newly defined classes */` |
|        - | 14909 | `		SyHashEntry *pEntry;` |
|        - | 14910 | `		ph7_class *pClass;` |
|        - | 14911 | `		ph7_value sResult; /* Return value */` |
|        - | 14912 | `		sxi32 rc;` |
|    12726 | 14913 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   962334 | 14914 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   943248 | 14915 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14916 | `			/* Only mount classes that haven't been mounted yet */` |
|   943248 | 14917 | `			if( !pClass->bMounted ){` |
|   245918 | 14918 | `				rc = VmMountUserClass(pVm,pClass);` |
|   245918 | 14919 | `				if( rc != SXRET_OK ){` |
|        - | 14920 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14921 | `					if( pCtx ){` |
|      ! 0 | 14922 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14923 | `					}` |
|      ! 0 | 14924 | `					goto Cleanup;` |
|        - | 14925 | `				}` |
|   122958 | 14926 | `			}` |
|        2 | 14927 | `		}` |
|    12726 | 14928 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14929 | `			/* Out of memory */` |
|      ! 0 | 14930 | `			if( pCtx ){` |
|      ! 0 | 14931 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14932 | `			}` |
|      ! 0 | 14933 | `			goto Cleanup;` |
|        - | 14934 | `		}` |
|    12726 | 14935 | `		if( bTrueReturn ){` |
|        - | 14936 | `			/* Assume a boolean true return value */` |
|     9566 | 14937 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4784 | 14938 | `		}else{` |
|        - | 14939 | `			/* Assume a null return value */` |
|     3162 | 14940 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14941 | `		}` |
|        - | 14942 | `		/* Execute the compiled chunk */` |
|    12726 | 14943 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12726 | 14944 | `		if( pCtx ){` |
|        - | 14945 | `			/* Set the execution result */` |
|     9586 | 14946 | `			ph7_result_value(pCtx,&sResult);` |
|     4792 | 14947 | `		}` |
|    12726 | 14948 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14949 | `	}` |
|     6363 | 14950 | `Cleanup:` |
|        - | 14951 | `	/* Cleanup the mess left behind */` |
|    12728 | 14952 | `	pVm->pByteContainer = pByteCode;` |
|    12728 | 14953 | `	SySetRelease(&aByteCode);` |
|        - | 14954 | `	/* Restore caller's namespace state */` |
|    12728 | 14955 | `	SyBlobReset(&pVm->sNamespace);` |
|    12728 | 14956 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12728 | 14957 | `	SyBlobRelease(&sSavedNs);` |
|    12728 | 14958 | `	return SXRET_OK;` |
|        2 | 14959 |  |
|        - | 14960 | `/*` |
|        - | 14961 | ` * value eval(string $code)` |
|        - | 14962 | ` *   Evaluate a string as PHP code.` |
|        - | 14963 | ` * Parameter` |
|        - | 14964 | ` *  code: PHP code to evaluate.` |
|        - | 14965 | ` * Return` |
|        - | 14966 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14967 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14968 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14969 | ` */` |
|       24 | 14970 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14971 |  |
|        - | 14972 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 14973 | `	if( nArg < 1 ){` |
|        - | 14974 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14975 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14976 | `		return SXRET_OK;` |
|        - | 14977 | `	}` |
|        - | 14978 | `	/* Chunk to evaluate */` |
|       26 | 14979 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 14980 | `	if( sChunk.nByte < 1 ){` |
|        - | 14981 | `		/* Empty string,return NULL */` |
|        3 | 14982 | `		ph7_result_null(pCtx);` |
|        3 | 14983 | `		return SXRET_OK;` |
|        - | 14984 | `	}` |
|        - | 14985 | `	/* Eval the chunk */` |
|       24 | 14986 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 14987 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14988 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 14989 | `		return PH7_ABORT;` |
|        - | 14990 | `	}` |
|       22 | 14991 | `	return SXRET_OK;` |
|       14 | 14992 |  |
|        - | 14993 | `/*` |
|        - | 14994 | ` * Check if a file path is already included.` |
|        - | 14995 | ` */` |
|    19126 | 14996 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14997 |  |
|        - | 14998 | `	SyString *aEntries;` |
|        - | 14999 | `	sxu32 n;` |
|    19128 | 15000 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15001 | `	/* Perform a linear search */` |
| 91260726 | 15002 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 91241610 | 15003 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15004 | `			/* Already included */` |
|       11 | 15005 | `			return TRUE;` |
|        - | 15006 | `		}` |
| 45620801 | 15007 | `	}` |
|    19118 | 15008 | `	return FALSE;` |
|     9565 | 15009 |  |
|        - | 15010 | `/*` |
|        - | 15011 | ` * Push a file path in the appropriate VM container.` |
|        - | 15012 | ` */` |
|    22258 | 15013 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 15014 |  |
|        - | 15015 | `	SyString sPath;` |
|        - | 15016 | `	char *zDup;` |
|        - | 15017 | `#ifdef __WINNT__` |
|        - | 15018 | `	char *zCur;` |
|        - | 15019 | `#endif` |
|        - | 15020 | `	sxi32 rc;` |
|    22260 | 15021 | `	if( nLen < 0 ){` |
|     3134 | 15022 | `		nLen = SyStrlen(zPath);` |
|     1566 | 15023 | `	}` |
|        - | 15024 | `	/* Duplicate the file path first */` |
|    22260 | 15025 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22260 | 15026 | `	if( zDup == 0 ){` |
|      ! 0 | 15027 | `		return SXERR_MEM;` |
|        - | 15028 | `	}` |
|        - | 15029 | `#ifdef __WINNT__` |
|        - | 15030 | `	/* Normalize path on windows` |
|        - | 15031 | `	 * Example:` |
|        - | 15032 | `	 *    Path/To/File.php` |
|        - | 15033 | `	 * becomes` |
|        - | 15034 | `	 *   path\to\file.php` |
|        - | 15035 | `	 */` |
|        2 | 15036 | `	zCur = zDup;` |
|        2 | 15037 | `	while( zCur[0] != 0 ){` |
|        2 | 15038 | `		if( zCur[0] == '/' ){` |
|        2 | 15039 | `			zCur[0] = '\\';` |
|        2 | 15040 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15041 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15042 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15043 | `		}` |
|        2 | 15044 | `		zCur++;` |
|        2 | 15045 | `	}` |
|        - | 15046 | `#endif` |
|        - | 15047 | `	/* Install the file path */` |
|    22260 | 15048 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22260 | 15049 | `	if( !bMain ){` |
|    19128 | 15050 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15051 | `			/* Already included */` |
|       11 | 15052 | `			*pNew = 0;` |
|        6 | 15053 | `		}else{` |
|        - | 15054 | `			/* Insert in the corresponding container */` |
|    19118 | 15055 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19118 | 15056 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15057 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15058 | `				return rc;` |
|        - | 15059 | `			}` |
|    19118 | 15060 | `			*pNew = 1;` |
|        - | 15061 | `		}` |
|     9563 | 15062 | `	}` |
|    22260 | 15063 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22260 | 15064 | `	return SXRET_OK;` |
|    11131 | 15065 |  |
|        - | 15066 | `/*` |
|        - | 15067 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15068 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15069 | ` * indicates failure.` |
|        - | 15070 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15071 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15072 | ` * operations.` |
|        - | 15073 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15074 | ` * this function is a no-op.` |
|        - | 15075 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15076 | ` * constructs for more information.` |
|        - | 15077 | ` */` |
|     9578 | 15078 | `static sxi32 VmExecIncludedFile(` |
|        - | 15079 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15080 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15081 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15082 | `	 )` |
|        2 | 15083 |  |
|        - | 15084 | `	sxi32 rc;` |
|        - | 15085 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15086 | `	const ph7_io_stream *pStream;` |
|        - | 15087 | `	SyBlob sContents;` |
|        - | 15088 | `	void *pHandle;` |
|        - | 15089 | `	ph7_vm *pVm;` |
|        - | 15090 | `	int isNew;` |
|        - | 15091 | `	/* Initialize fields */` |
|     9580 | 15092 | `	pVm = pCtx->pVm;` |
|     9580 | 15093 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9580 | 15094 | `	isNew = 0;` |
|        - | 15095 | `	/* Extract the associated stream */` |
|     9580 | 15096 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15097 | `	/*` |
|        - | 15098 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15099 | `	 * in a read-only mode.` |
|        - | 15100 | `	 */` |
|     9580 | 15101 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9580 | 15102 | `	if( pHandle == 0 ){` |
|        8 | 15103 | `		return SXERR_IO;` |
|        - | 15104 | `	}` |
|     9574 | 15105 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9574 | 15106 | `	if( IncludeOnce && !isNew ){` |
|        - | 15107 | `		/* Already included */` |
|        9 | 15108 | `		rc = SXERR_EXISTS;` |
|        5 | 15109 | `	}else{` |
|        - | 15110 | `		/* Read the whole file contents */` |
|     9566 | 15111 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9566 | 15112 | `		if( rc == SXRET_OK ){` |
|        - | 15113 | `			SyString sScript;` |
|        - | 15114 | `			/* Compile and execute the script */` |
|     9566 | 15115 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9566 | 15116 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4782 | 15117 | `		}` |
|        - | 15118 | `	}` |
|        - | 15119 | `	/* Pop from the set of included file */` |
|     9574 | 15120 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15121 | `	/* Close the handle */` |
|     9574 | 15122 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15123 | `	/* Release the working buffer */` |
|     9574 | 15124 | `	SyBlobRelease(&sContents);` |
|        - | 15125 | `#else` |
|        - | 15126 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15127 | `	SXUNUSED(pPath);` |
|        - | 15128 | `	SXUNUSED(IncludeOnce);` |
|        - | 15129 | `	rc = SXERR_IO;` |
|        - | 15130 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9574 | 15131 | `	return rc;` |
|     4791 | 15132 |  |
|        - | 15133 | `/*` |
|        - | 15134 | ` * string get_include_path(void)` |
|        - | 15135 | ` *  Gets the current include_path configuration option.` |
|        - | 15136 | ` * Parameter` |
|        - | 15137 | ` *  None` |
|        - | 15138 | ` * Return` |
|        - | 15139 | ` *  Included paths as a string` |
|        - | 15140 | ` */` |
|        2 | 15141 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15142 |  |
|        3 | 15143 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15144 | `	SyString *aEntry;` |
|        - | 15145 | `	int dir_sep;` |
|        - | 15146 | `	sxu32 n;` |
|        - | 15147 | `#ifdef __WINNT__` |
|        1 | 15148 | `	dir_sep = ';';` |
|        - | 15149 | `#else` |
|        - | 15150 | `	/* Assume UNIX path separator */` |
|        2 | 15151 | `	dir_sep = ':';` |
|        - | 15152 | `#endif` |
|        1 | 15153 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15154 | `	SXUNUSED(apArg);` |
|        - | 15155 | `	/* Point to the list of import paths */` |
|        3 | 15156 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15157 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15158 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15159 | `		if( n > 0 ){` |
|        - | 15160 | `			/* Append dir seprator */` |
|      ! 0 | 15161 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15162 | `		}` |
|        - | 15163 | `		/* Append path */` |
|        3 | 15164 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15165 | `	}` |
|        3 | 15166 | `	return PH7_OK;` |
|        1 | 15167 |  |
|        - | 15168 | `/*` |
|        - | 15169 | ` * string get_get_included_files(void)` |
|        - | 15170 | ` *  Gets the current include_path configuration option.` |
|        - | 15171 | ` * Parameter` |
|        - | 15172 | ` *  None` |
|        - | 15173 | ` * Return` |
|        - | 15174 | ` *  Included paths as a string` |
|        - | 15175 | ` */` |
|        2 | 15176 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15177 |  |
|        3 | 15178 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15179 | `	ph7_value *pArray,*pWorker;` |
|        - | 15180 | `	SyString *pEntry;` |
|        - | 15181 | `	int c,d;` |
|        - | 15182 | `	/* Create an array and a working value */` |
|        3 | 15183 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15184 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15185 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15186 | `		/* Out of memory,return null */` |
|      ! 0 | 15187 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15188 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15189 | `		SXUNUSED(apArg);` |
|      ! 0 | 15190 | `		return PH7_OK;` |
|        - | 15191 | `	}` |
|        3 | 15192 | `	c = d = '/';` |
|        - | 15193 | `#ifdef __WINNT__` |
|        1 | 15194 | `	d = '\\';` |
|        - | 15195 | `#endif` |
|        - | 15196 | `	/* Iterate throw entries */` |
|        3 | 15197 | `	SySetResetCursor(pFiles);` |
|     3917 | 15198 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15199 | `		const char *zBase,*zEnd;` |
|        - | 15200 | `		int iLen;` |
|        - | 15201 | `		/* reset the string cursor */` |
|     3915 | 15202 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15203 | `		/* Extract base name */` |
|     3915 | 15204 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15205 | `		/* Ignore trailing '/' */` |
|     5872 | 15206 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15207 | `			zEnd--;` |
|      ! 0 | 15208 | `		}` |
|     3915 | 15209 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15210 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15211 | `			zEnd--;` |
|        1 | 15212 | `		}` |
|     3915 | 15213 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15214 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15215 | `		/* Copy entry name */` |
|     3915 | 15216 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15217 | `		/* Perform the insertion */` |
|     3915 | 15218 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15219 | `	}` |
|        - | 15220 | `	/* All done,return the created array */` |
|        3 | 15221 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15222 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15223 | `	 * by the engine as soon we return from this foreign` |
|        - | 15224 | `	 * function.` |
|        - | 15225 | `	 */` |
|        3 | 15226 | `	return PH7_OK;` |
|        2 | 15227 |  |
|        - | 15228 | `/*` |
|        - | 15229 | ` * include:` |
|        - | 15230 | ` * According to the PHP reference manual.` |
|        - | 15231 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15232 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15233 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15234 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15235 | ` *  and the current working directory before failing. The include()` |
|        - | 15236 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15237 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15238 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15239 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15240 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15241 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15242 | ` *  directory to find the requested file.` |
|        - | 15243 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15244 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15245 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15246 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15247 | ` */` |
|     9554 | 15248 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15249 |  |
|        - | 15250 | `	SyString sFile;` |
|        - | 15251 | `	sxi32 rc;` |
|     9556 | 15252 | `	if( nArg < 1 ){` |
|        - | 15253 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15254 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15255 | `		return SXRET_OK;` |
|        - | 15256 | `	}` |
|        - | 15257 | `	/* File to include */` |
|     9556 | 15258 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9556 | 15259 | `	if( sFile.nByte < 1 ){` |
|        - | 15260 | `		/* Empty string,return NULL */` |
|      ! 0 | 15261 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15262 | `		return SXRET_OK;` |
|        - | 15263 | `	}` |
|        - | 15264 | `	/* Open,compile and execute the desired script */` |
|     9556 | 15265 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9556 | 15266 | `	if( rc != SXRET_OK ){` |
|        - | 15267 | `		/* Emit a warning and return false */` |
|        3 | 15268 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15269 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15270 | `	}` |
|     9556 | 15271 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15272 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 15273 | `		return PH7_ABORT;` |
|        - | 15274 | `	}` |
|     9552 | 15275 | `	return SXRET_OK;` |
|     4779 | 15276 |  |
|        - | 15277 | `/*` |
|        - | 15278 | ` * include_once:` |
|        - | 15279 | ` *  According to the PHP reference manual.` |
|        - | 15280 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15281 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15282 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15283 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15284 | ` *   just once.` |
|        - | 15285 | ` */` |
|       10 | 15286 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15287 |  |
|        - | 15288 | `	SyString sFile;` |
|        - | 15289 | `	sxi32 rc;` |
|       11 | 15290 | `	if( nArg < 1 ){` |
|        - | 15291 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15292 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15293 | `		return SXRET_OK;` |
|        - | 15294 | `	}` |
|        - | 15295 | `	/* File to include */` |
|       11 | 15296 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15297 | `	if( sFile.nByte < 1 ){` |
|        - | 15298 | `		/* Empty string,return NULL */` |
|      ! 0 | 15299 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15300 | `		return SXRET_OK;` |
|        - | 15301 | `	}` |
|        - | 15302 | `	/* Open,compile and execute the desired script */` |
|       11 | 15303 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15304 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15305 | `		/* File already included,return TRUE */` |
|        7 | 15306 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15307 | `		return SXRET_OK;` |
|        - | 15308 | `	}` |
|        5 | 15309 | `	if( rc != SXRET_OK ){` |
|        - | 15310 | `		/* Emit a warning and return false */` |
|      ! 0 | 15311 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15312 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15313 | ` 	}` |
|        5 | 15314 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15315 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15316 | `		return PH7_ABORT;` |
|        - | 15317 | `	}` |
|        5 | 15318 | `	return SXRET_OK;` |
|        6 | 15319 |  |
|        - | 15320 | `/*` |
|        - | 15321 | ` * require.` |
|        - | 15322 | ` *  According to the PHP reference manual.` |
|        - | 15323 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15324 | ` *   also produce a fatal level error.` |
|        - | 15325 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15326 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15327 | ` */` |
|        6 | 15328 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15329 |  |
|        - | 15330 | `	SyString sFile;` |
|        - | 15331 | `	sxi32 rc;` |
|        8 | 15332 | `	if( nArg < 1 ){` |
|        - | 15333 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15334 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15335 | `		return SXRET_OK;` |
|        - | 15336 | `	}` |
|        - | 15337 | `	/* File to include */` |
|        8 | 15338 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15339 | `	if( sFile.nByte < 1 ){` |
|        - | 15340 | `		/* Empty string,return NULL */` |
|      ! 0 | 15341 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15342 | `		return SXRET_OK;` |
|        - | 15343 | `	}` |
|        - | 15344 | `	/* Open,compile and execute the desired script */` |
|        8 | 15345 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15346 | `	if( rc != SXRET_OK ){` |
|        - | 15347 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15348 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15349 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15350 | `		return PH7_ABORT;` |
|        - | 15351 | `	}` |
|        8 | 15352 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15353 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15354 | `		return PH7_ABORT;` |
|        - | 15355 | `	}` |
|        8 | 15356 | `	return SXRET_OK;` |
|        5 | 15357 |  |
|        - | 15358 | `/*` |
|        - | 15359 | ` * require_once:` |
|        - | 15360 | ` *  According to the PHP reference manual.` |
|        - | 15361 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15362 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15363 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15364 | ` *   and how it differs from its non _once siblings.` |
|        - | 15365 | ` */` |
|        4 | 15366 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15367 |  |
|        - | 15368 | `	SyString sFile;` |
|        - | 15369 | `	sxi32 rc;` |
|        5 | 15370 | `	if( nArg < 1 ){` |
|        - | 15371 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15372 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15373 | `		return SXRET_OK;` |
|        - | 15374 | `	}` |
|        - | 15375 | `	/* File to include */` |
|        5 | 15376 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15377 | `	if( sFile.nByte < 1 ){` |
|        - | 15378 | `		/* Empty string,return NULL */` |
|      ! 0 | 15379 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15380 | `		return SXRET_OK;` |
|        - | 15381 | `	}` |
|        - | 15382 | `	/* Open,compile and execute the desired script */` |
|        5 | 15383 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15384 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15385 | `		/* File already included,return TRUE */` |
|        3 | 15386 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15387 | `		return SXRET_OK;` |
|        - | 15388 | `	}` |
|        3 | 15389 | `	if( rc != SXRET_OK ){` |
|        - | 15390 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15391 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15392 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15393 | `		return PH7_ABORT;` |
|        - | 15394 | `	}` |
|        3 | 15395 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15396 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15397 | `		return PH7_ABORT;` |
|        - | 15398 | `	}` |
|        3 | 15399 | `	return SXRET_OK;` |
|        3 | 15400 |  |
|        - | 15401 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15402 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15403 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15404 | `/*` |
|        - | 15405 | ` * Section:` |
|        - | 15406 | ` *  SPL Autoloading functions.` |
|        - | 15407 | ` * Status:` |
|        - | 15408 | ` *  Stable.` |
|        - | 15409 | ` */` |
|        - | 15410 | `/*` |
|        - | 15411 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15412 | ` *  Register given function as __autoload() implementation.` |
|        - | 15413 | ` * Parameters` |
|        - | 15414 | ` *  callback` |
|        - | 15415 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15416 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15417 | ` *  throw` |
|        - | 15418 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15419 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15420 | ` *  prepend` |
|        - | 15421 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15422 | ` *   autoload stack instead of appending it.` |
|        - | 15423 | ` * Return` |
|        - | 15424 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15425 | ` */` |
|       34 | 15426 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15427 |  |
|        - | 15428 | `	VmAutoloadCB sEntry;` |
|       36 | 15429 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15430 | `	int iPrepend = 0;` |
|        - | 15431 | `	sxu32 n;` |
|       36 | 15432 | `	if( nArg < 1 ){` |
|        - | 15433 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15434 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15435 | `		/* Check for duplicates first */` |
|        9 | 15436 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15437 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15438 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15439 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15440 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15441 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15442 | `				return SXRET_OK;` |
|        - | 15443 | `			}` |
|      ! 0 | 15444 | `		}` |
|        5 | 15445 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15446 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15447 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15448 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15449 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15450 | `		return SXRET_OK;` |
|        - | 15451 | `	}` |
|        - | 15452 | `	/* Validate that the callback is callable */` |
|       28 | 15453 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15454 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15455 | `		if( nArg >= 2 ){` |
|      ! 0 | 15456 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15457 | `		}` |
|      ! 0 | 15458 | `		if( iThrow ){` |
|      ! 0 | 15459 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15460 | `				"Argument is not callable");` |
|      ! 0 | 15461 | `		}` |
|      ! 0 | 15462 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15463 | `		return SXRET_OK;` |
|        - | 15464 | `	}` |
|        - | 15465 | `	/* Check for duplicates */` |
|       46 | 15466 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15467 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15468 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15469 | `			/* Already registered */` |
|      ! 0 | 15470 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15471 | `			return SXRET_OK;` |
|        - | 15472 | `		}` |
|       11 | 15473 | `	}` |
|        - | 15474 | `	/* Check prepend flag */` |
|       28 | 15475 | `	if( nArg >= 3 ){` |
|        3 | 15476 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15477 | `	}` |
|        - | 15478 | `	/* Store the callback */` |
|       28 | 15479 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15480 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15481 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15482 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15483 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15484 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15485 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15486 | `		VmAutoloadCB *aBase;` |
|        3 | 15487 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15488 | `		/* Rotate: move last entry to front */` |
|        3 | 15489 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15490 | `		if( aBase ){` |
|        - | 15491 | `			VmAutoloadCB sTemp;` |
|        - | 15492 | `			sxu32 i;` |
|        3 | 15493 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15494 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15495 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15496 | `			}` |
|        3 | 15497 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15498 | `		}` |
|        2 | 15499 | `	}else{` |
|       26 | 15500 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15501 | `	}` |
|       28 | 15502 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15503 | `	return SXRET_OK;` |
|       19 | 15504 |  |
|        - | 15505 | `/*` |
|        - | 15506 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15507 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15508 | ` * Parameters` |
|        - | 15509 | ` *  callback` |
|        - | 15510 | ` *   The autoload function being unregistered.` |
|        - | 15511 | ` * Return` |
|        - | 15512 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15513 | ` */` |
|       32 | 15514 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15515 |  |
|       34 | 15516 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15517 | `	sxu32 n,nEntry;` |
|       34 | 15518 | `	if( nArg < 1 ){` |
|      ! 0 | 15519 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15520 | `		return SXRET_OK;` |
|        - | 15521 | `	}` |
|       34 | 15522 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15523 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15524 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15525 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15526 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15527 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15528 | `			sxu32 i;` |
|       32 | 15529 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15530 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15531 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15532 | `			}` |
|        - | 15533 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15534 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15535 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15536 | `			return SXRET_OK;` |
|        - | 15537 | `		}` |
|        3 | 15538 | `	}` |
|        3 | 15539 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15540 | `	return SXRET_OK;` |
|       18 | 15541 |  |
|        - | 15542 | `/*` |
|        - | 15543 | ` * array spl_autoload_functions(void)` |
|        - | 15544 | ` *  Return all registered __autoload() functions.` |
|        - | 15545 | ` * Return` |
|        - | 15546 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15547 | ` *  an empty array is returned.` |
|        - | 15548 | ` */` |
|       20 | 15549 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15550 |  |
|       21 | 15551 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15552 | `	ph7_value *pArray;` |
|        - | 15553 | `	sxu32 n,nEntry;` |
|       10 | 15554 | `	SXUNUSED(nArg);` |
|       10 | 15555 | `	SXUNUSED(apArg);` |
|       21 | 15556 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15557 | `	if( pArray == 0 ){` |
|      ! 0 | 15558 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15559 | `		return SXRET_OK;` |
|        - | 15560 | `	}` |
|       21 | 15561 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15562 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15563 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15564 | `		if( pEntry ){` |
|       15 | 15565 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15566 | `		}` |
|        8 | 15567 | `	}` |
|       21 | 15568 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15569 | `	return SXRET_OK;` |
|       11 | 15570 |  |
|        - | 15571 | `/*` |
|        - | 15572 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15573 | ` *  Default implementation of __autoload().` |
|        - | 15574 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15575 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15576 | ` * Parameters` |
|        - | 15577 | ` *  class` |
|        - | 15578 | ` *   The class name being searched.` |
|        - | 15579 | ` *  file_extensions` |
|        - | 15580 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15581 | ` */` |
|        2 | 15582 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15583 |  |
|        - | 15584 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15585 | `	SyBlob sPath;` |
|        - | 15586 | `	int nClass;` |
|        - | 15587 | `	sxi32 rc;` |
|        3 | 15588 | `	if( nArg < 1 ){` |
|      ! 0 | 15589 | `		return SXRET_OK;` |
|        - | 15590 | `	}` |
|        3 | 15591 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15592 | `	if( nClass < 1 ){` |
|      ! 0 | 15593 | `		return SXRET_OK;` |
|        - | 15594 | `	}` |
|        - | 15595 | `	/* Default extensions */` |
|        3 | 15596 | `	zExt = ".php,.inc";` |
|        3 | 15597 | `	if( nArg >= 2 ){` |
|        - | 15598 | `		int nExt;` |
|      ! 0 | 15599 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15600 | `		if( nExt < 1 ){` |
|      ! 0 | 15601 | `			zExt = ".php,.inc";` |
|      ! 0 | 15602 | `		}` |
|      ! 0 | 15603 | `	}` |
|        3 | 15604 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15605 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15606 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15607 | `	zCur = zExt;` |
|        7 | 15608 | `	while( zCur < zEnd ){` |
|        - | 15609 | `		const char *zComma;` |
|        - | 15610 | `		SyString sFile;` |
|        - | 15611 | `		int i;` |
|        - | 15612 | `		/* Find next comma or end */` |
|        5 | 15613 | `		zComma = zCur;` |
|       21 | 15614 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15615 | `			zComma++;` |
|        1 | 15616 | `		}` |
|        - | 15617 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15618 | `		SyBlobReset(&sPath);` |
|       69 | 15619 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15620 | `			char c = zClass[i];` |
|       65 | 15621 | `			if( c == '\\' ){` |
|      ! 0 | 15622 | `				c = '/';` |
|       65 | 15623 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15624 | `				c = c + ('a' - 'A');` |
|        6 | 15625 | `			}` |
|       65 | 15626 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15627 | `		}` |
|        - | 15628 | `		/* Append extension */` |
|        5 | 15629 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15630 | `		/* Try to include the file */` |
|        5 | 15631 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15632 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15633 | `		if( rc == SXRET_OK ){` |
|        - | 15634 | `			/* File included successfully */` |
|      ! 0 | 15635 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15636 | `			return SXRET_OK;` |
|        - | 15637 | `		}` |
|        - | 15638 | `		/* Move past the comma */` |
|        5 | 15639 | `		zCur = zComma;` |
|        5 | 15640 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15641 | `			zCur++;` |
|        1 | 15642 | `		}` |
|        1 | 15643 | `	}` |
|        3 | 15644 | `	SyBlobRelease(&sPath);` |
|        3 | 15645 | `	return SXRET_OK;` |
|        2 | 15646 |  |
|        - | 15647 | `/* Table of built-in VM functions. */` |
|        - | 15648 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15649 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15650 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15651 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15652 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15653 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15654 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15655 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15656 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15657 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15658 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15659 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15660 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15661 | `	    /* Constants management */` |
|        - | 15662 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15663 | `	{ "define",   vm_builtin_define               },` |
|        - | 15664 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15665 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15666 | `	   /* Class/Object functions */` |
|        - | 15667 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15668 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15669 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15670 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15671 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15672 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15673 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15674 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15675 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15676 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15677 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15678 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15679 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15680 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15681 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15682 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15683 | `	   /* SPL Autoloading */` |
|        - | 15684 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15685 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15686 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15687 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15688 | `	   /* Random numbers/strings generators */` |
|        - | 15689 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15690 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15691 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15692 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15693 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15694 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15695 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15696 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15697 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15698 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15699 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15700 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15701 | `	   /* Language constructs functions */` |
|        - | 15702 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15703 | `	{ "print", vm_builtin_print                   },` |
|        - | 15704 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15705 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15706 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15707 | `	  /* Variable handling functions */` |
|        - | 15708 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15709 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15710 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15711 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15712 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15713 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15714 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15715 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15716 | `	  /* Ouput control functions */` |
|        - | 15717 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15718 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15719 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15720 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15721 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15722 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15723 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15724 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15725 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15726 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15727 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15728 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15729 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15730 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15731 | `	  /* Assertion functions */` |
|        - | 15732 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15733 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15734 | `	  /* Error reporting functions */` |
|        - | 15735 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15736 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15737 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15738 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15739 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15740 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15741 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15742 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15743 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15744 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15745 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15746 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15747 | `	  /* Release info */` |
|        - | 15748 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15749 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15750 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15751 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15752 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15753 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15754 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15755 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15756 | `	  /* hashmap */` |
|        - | 15757 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15758 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15759 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15760 | `	  /* URL related function */` |
|        - | 15761 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15762 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15763 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15764 | `	   /* XML processing functions */` |
|        - | 15765 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15766 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15767 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15768 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15769 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15770 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15771 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15772 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15773 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15774 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15775 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15776 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15777 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15778 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15779 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15780 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15781 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15782 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15783 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15784 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15785 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15786 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15787 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15788 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15789 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15790 | `	   /* Command line processing */` |
|        - | 15791 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15792 | `	   /* JSON encoding/decoding */` |
|        - | 15793 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15794 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15795 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15796 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15797 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15798 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15799 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15800 | `	   /* Files/URI inclusion facility */` |
|        - | 15801 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15802 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15803 | `	{ "include",      vm_builtin_include          },` |
|        - | 15804 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15805 | `	{ "require",      vm_builtin_require          },` |
|        - | 15806 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15807 | `};` |
|        - | 15808 | `/*` |
|        - | 15809 | ` * Register the built-in VM functions defined above.` |
|        - | 15810 | ` */` |
|     2826 | 15811 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15812 |  |
|        - | 15813 | `	sxi32 rc;` |
|        - | 15814 | `	sxu32 n;` |
|   381512 | 15815 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15816 | `		/* Note that these special functions have access` |
|        - | 15817 | `		 * to the underlying virtual machine as their` |
|        - | 15818 | `		 * private data.` |
|        - | 15819 | `		 */` |
|   378686 | 15820 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   378686 | 15821 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15822 | `			return rc;` |
|        - | 15823 | `		}` |
|   189344 | 15824 | `	}` |
|     2828 | 15825 | `	return SXRET_OK;` |
|     1415 | 15826 |  |
|        - | 15827 | `/*` |
|        - | 15828 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15829 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15830 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15831 | ` */` |
|   182372 | 15832 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15833 |  |
|   182374 | 15834 | `	if( !iLoadable ){` |
|   180294 | 15835 | `		return pClass;` |
|        - | 15836 | `	}` |
|     2086 | 15837 | `	while(pClass){` |
|     2082 | 15838 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2078 | 15839 | `			return pClass;` |
|        - | 15840 | `		}` |
|        5 | 15841 | `		pClass = pClass->pNextName;` |
|        1 | 15842 | `	}` |
|        5 | 15843 | `	return 0;` |
|    91188 | 15844 |  |
|        - | 15845 | `/*` |
|        - | 15846 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15847 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15848 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15849 | ` * registered in the VM's class table.` |
|        - | 15850 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15851 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15852 | ` */` |
|       38 | 15853 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15854 |  |
|        - | 15855 | `	VmAutoloadCB *pEntry;` |
|        - | 15856 | `	ph7_value sArg,sResult;` |
|        - | 15857 | `	SyHashEntry *pHashEntry;` |
|        - | 15858 | `	ph7_class *pClass;` |
|        - | 15859 | `	sxu32 n,nEntry;` |
|       40 | 15860 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15861 | `	if( nEntry < 1 ){` |
|       26 | 15862 | `		return 0;` |
|        - | 15863 | `	}` |
|        - | 15864 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15865 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15866 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15867 | `	}` |
|        - | 15868 | `	/* Mark this class as being autoloaded */` |
|       14 | 15869 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15870 | `	/* Prepare the class name argument */` |
|       14 | 15871 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15872 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15873 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15874 | `	pClass = 0;` |
|       28 | 15875 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15876 | `		ph7_value *apArg[1];` |
|       24 | 15877 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15878 | `		if( pEntry == 0 ){` |
|      ! 0 | 15879 | `			continue;` |
|        - | 15880 | `		}` |
|       24 | 15881 | `		apArg[0] = &sArg;` |
|       24 | 15882 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15883 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15884 | `			continue;` |
|        - | 15885 | `		}` |
|        - | 15886 | `		/* Check if the class is now available */` |
|       24 | 15887 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15888 | `		if( pHashEntry ){` |
|       10 | 15889 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15890 | `			if( pClass ){` |
|       10 | 15891 | `				break;` |
|        - | 15892 | `			}` |
|      ! 0 | 15893 | `		}` |
|        9 | 15894 | `	}` |
|       14 | 15895 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15896 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15897 | `	/* Remove reentrancy guard */` |
|       14 | 15898 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15899 | `	return pClass;` |
|       21 | 15900 |  |
|        - | 15901 | `/*` |
|        - | 15902 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15903 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15904 | ` */` |
|       18 | 15905 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15906 |  |
|       20 | 15907 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15908 |  |
|        - | 15909 | `/*` |
|        - | 15910 | ` * Check if the given name refer to an installed class.` |
|        - | 15911 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15912 | ` */` |
|   182384 | 15913 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15914 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15915 | `	const char *zName,  /* Name of the target class */` |
|        - | 15916 | `	sxu32 nByte,        /* zName length */` |
|        - | 15917 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15918 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15919 | `						 */` |
|        - | 15920 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15921 | `	)` |
|        2 | 15922 |  |
|        - | 15923 | `	SyHashEntry *pEntry;` |
|        - | 15924 | `	ph7_class *pClass;` |
|    91192 | 15925 | `	SXUNUSED(iNest);` |
|        - | 15926 | `	/* Exact class lookup.` |
|        - | 15927 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15928 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   182386 | 15929 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   182386 | 15930 | `	if( pEntry == 0 ){` |
|        - | 15931 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15932 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15933 | `	}` |
|   182366 | 15934 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   182366 | 15935 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    91194 | 15936 |  |
|        - | 15937 | `/*` |
|        - | 15938 | ` * Reference Table Implementation` |
|        - | 15939 | ` * Status: stable <chm@symisc.net>` |
|        - | 15940 | ` * Intro` |
|        - | 15941 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15942 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15943 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15944 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15945 | ` *  Refer to the official for more information on this powerful` |
|        - | 15946 | ` *  extension.` |
|        - | 15947 | ` */` |
|        - | 15948 | `/*` |
|        - | 15949 | ` * Allocate a new reference entry.` |
|        - | 15950 | ` */` |
|  3202658 | 15951 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15952 |  |
|        - | 15953 | `	VmRefObj *pRef;` |
|        - | 15954 | `	/* Allocate a new instance */` |
|  3202660 | 15955 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3202660 | 15956 | `	if( pRef == 0 ){` |
|      ! 0 | 15957 | `		return 0;` |
|        - | 15958 | `	}` |
|        - | 15959 | `	/* Zero the structure */` |
|  3202660 | 15960 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15961 | `	/* Initialize fields */` |
|  3202660 | 15962 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3202660 | 15963 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3202660 | 15964 | `	pRef->nIdx = nIdx;` |
|  3202660 | 15965 | `	return pRef;` |
|  1601331 | 15966 |  |
|        - | 15967 | `/*` |
|        - | 15968 | ` * Default hash function used by the reference table` |
|        - | 15969 | ` * for lookup/insertion operations.` |
|        - | 15970 | ` */` |
| 17541965 | 15971 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15972 |  |
|        - | 15973 | `	/* Calculate the hash based on the memory object index */` |
| 17541967 | 15974 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15975 |  |
|        - | 15976 | `/*` |
|        - | 15977 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15978 | ` * in the reference table.` |
|        - | 15979 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15980 | ` * otherwise.` |
|        - | 15981 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15982 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15983 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15984 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15985 | ` * Refer to the official for more information on this powerful` |
|        - | 15986 | ` * extension.` |
|        - | 15987 | ` */` |
|  9548032 | 15988 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15989 |  |
|        - | 15990 | `	VmRefObj *pRef;` |
|        - | 15991 | `	sxu32 nBucket;` |
|        - | 15992 | `	/* Point to the appropriate bucket */` |
|  9548034 | 15993 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15994 | `	/* Perform the lookup */` |
|  9548034 | 15995 | `	pRef = pVm->apRefObj[nBucket];` |
| 20996621 | 15996 | `	for(;;){` |
| 41979746 | 15997 | `		if( pRef == 0 ){` |
|  3307960 | 15998 | `			break;` |
|        - | 15999 | `		}` |
| 38671788 | 16000 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16001 | `			/* Entry found */` |
|  6240076 | 16002 | `			return pRef;` |
|        - | 16003 | `		}` |
|        - | 16004 | `		/* Point to the next entry */` |
| 32431714 | 16005 | `		pRef = pRef->pNextCollide;` |
|        2 | 16006 | `	}` |
|        - | 16007 | `	/* No such entry,return NULL */` |
|  3307960 | 16008 | `	return 0;` |
|  4774018 | 16009 |  |
|        - | 16010 | `/*` |
|        - | 16011 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16012 | ` *` |
|        - | 16013 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16014 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16015 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16016 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16017 | ` * Refer to the official for more information on this powerful` |
|        - | 16018 | ` * extension.` |
|        - | 16019 | ` */` |
|  3202658 | 16020 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16021 |  |
|        - | 16022 | `	sxu32 nBucket;` |
|  3202660 | 16023 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16024 | `		VmRefObj **apNew;` |
|        - | 16025 | `		sxu32 nNew;` |
|        - | 16026 | `		/* Allocate a larger table */` |
|     4484 | 16027 | `		nNew = pVm->nRefSize << 1;` |
|     4484 | 16028 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4484 | 16029 | `		if( apNew ){` |
|     4484 | 16030 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16031 | `			sxu32 n;` |
|        - | 16032 | `			/* Zero the structure */` |
|     4484 | 16033 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16034 | `			/* Rehash all referenced entries */` |
|  2848110 | 16035 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16036 | `				/* Remove old collision links */` |
|  2843628 | 16037 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16038 | `				/* Point to the appropriate bucket */` |
|  2843628 | 16039 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16040 | `				/* Insert the entry  */` |
|  2843628 | 16041 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843628 | 16042 | `				if( apNew[nBucket] ){` |
|  2301116 | 16043 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16044 | `				}` |
|  2843628 | 16045 | `				apNew[nBucket] = pEntry;` |
|        - | 16046 | `				/* Point to the next entry */` |
|  2843628 | 16047 | `				pEntry = pEntry->pNext;` |
|  1421815 | 16048 | `			}` |
|        - | 16049 | `			/* Release the old table */` |
|     4484 | 16050 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16051 | `			/* Install the new one */` |
|     4484 | 16052 | `			pVm->apRefObj = apNew;` |
|     4484 | 16053 | `			pVm->nRefSize = nNew;` |
|     2241 | 16054 | `		}` |
|     2241 | 16055 | `	}` |
|        - | 16056 | `	/* Point to the appropriate bucket */` |
|  3202660 | 16057 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16058 | `	/* Insert the entry */` |
|  3202660 | 16059 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3202660 | 16060 | `	if( pVm->apRefObj[nBucket] ){` |
|  2614665 | 16061 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307340 | 16062 | `	}` |
|  3202660 | 16063 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3202660 | 16064 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3202660 | 16065 | `	pVm->nRefUsed++;` |
|  3202660 | 16066 | `	return SXRET_OK;` |
|        2 | 16067 |  |
|        - | 16068 | `/*` |
|        - | 16069 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16070 | ` * the reference table.` |
|        - | 16071 | ` * This function is invoked when the user perform an unset` |
|        - | 16072 | ` * call [i.e: unset($var); ].` |
|        - | 16073 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16074 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16075 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16076 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16077 | ` * Refer to the official for more information on this powerful` |
|        - | 16078 | ` * extension.` |
|        - | 16079 | ` */` |
|  3161404 | 16080 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16081 |  |
|        - | 16082 | `	ph7_hashmap_node **apNode;` |
|        - | 16083 | `	SyHashEntry **apEntry;` |
|        - | 16084 | `	sxu32 n;` |
|        - | 16085 | `	/* Point to the reference table */` |
|  3161406 | 16086 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3161406 | 16087 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16088 | `	/* Unlink the entry from the reference table */` |
|  3272642 | 16089 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   111238 | 16090 | `		if( apEntry[n] ){` |
|   111188 | 16091 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55593 | 16092 | `		}` |
|    55620 | 16093 | `	}` |
|  6211634 | 16094 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3050230 | 16095 | `		if( apNode[n] ){` |
|     7010 | 16096 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3504 | 16097 | `		}` |
|  1525116 | 16098 | `	}` |
|  3161406 | 16099 | `	if( pRef->pPrevCollide ){` |
|  1213757 | 16100 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   606599 | 16101 | `	}else{` |
|  1947651 | 16102 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16103 | `	}` |
|  3161406 | 16104 | `	if( pRef->pNextCollide ){` |
|  1801704 | 16105 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   900848 | 16106 | `	}` |
|  3161406 | 16107 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16108 | `	/* Release the node */` |
|  3161406 | 16109 | `	SySetRelease(&pRef->aReference);` |
|  3161406 | 16110 | `	SySetRelease(&pRef->aArrEntries);` |
|  3161406 | 16111 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3161406 | 16112 | `	pVm->nRefUsed--;` |
|  3161406 | 16113 | `	return SXRET_OK;` |
|        2 | 16114 |  |
|        - | 16115 | `/*` |
|        - | 16116 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16117 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16118 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16119 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16120 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16121 | ` * Refer to the official for more information on this powerful` |
|        - | 16122 | ` * extension.` |
|        - | 16123 | ` */` |
|  3238228 | 16124 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16125 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16126 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16127 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16128 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16129 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16130 | `	)` |
|        2 | 16131 |  |
|  3238230 | 16132 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16133 | `	VmRefObj *pRef;` |
|        - | 16134 | `	/* Check if the referenced object already exists */` |
|  3238230 | 16135 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3238230 | 16136 | `	if( pRef == 0 ){` |
|        - | 16137 | `		/* Create a new entry */` |
|  3202660 | 16138 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3202660 | 16139 | `		if( pRef == 0 ){` |
|      ! 0 | 16140 | `			return SXERR_MEM;` |
|        - | 16141 | `		}` |
|  3202660 | 16142 | `		pRef->iFlags = iFlags;` |
|        - | 16143 | `		/* Install the entry */` |
|  3202660 | 16144 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1601329 | 16145 | `	}` |
|  3238230 | 16146 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3238230 | 16147 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16148 | `		VmSlot sRef;` |
|        - | 16149 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16150 | `		 * be deleted when we leave this frame.` |
|        - | 16151 | `		 */` |
|   105410 | 16152 | `		sRef.nIdx = nIdx;` |
|   105410 | 16153 | `		sRef.pUserData = pEntry;` |
|   105410 | 16154 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16155 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16156 | `		}` |
|    52704 | 16157 | `	}` |
|  3238230 | 16158 | `	if( pEntry ){` |
|        - | 16159 | `		/* Address of the hash-entry */` |
|   140756 | 16160 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70377 | 16161 | `	}` |
|  3238230 | 16162 | `	if( pMapEntry ){` |
|        - | 16163 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3089080 | 16164 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1544539 | 16165 | `	}` |
|  3238230 | 16166 | `	return SXRET_OK;` |
|  1619116 | 16167 |  |
|        - | 16168 | `/*` |
|        - | 16169 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16170 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16171 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16172 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16173 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16174 | ` * Refer to the official for more information on this powerful` |
|        - | 16175 | ` * extension.` |
|        - | 16176 | ` */` |
|  3148592 | 16177 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16178 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16179 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16180 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16181 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16182 | `	)` |
|        2 | 16183 |  |
|        - | 16184 | `	VmRefObj *pRef;` |
|        - | 16185 | `	sxu32 n;` |
|        - | 16186 | `	/* Check if the referenced object already exists */` |
|  3148594 | 16187 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3148594 | 16188 | `	if( pRef == 0 ){` |
|        - | 16189 | `		/* Not such entry */` |
|   105296 | 16190 | `		return SXERR_NOTFOUND;` |
|        - | 16191 | `	}` |
|        - | 16192 | `	/* Remove the desired entry */` |
|  3043300 | 16193 | `	if( pEntry ){` |
|        - | 16194 | `		SyHashEntry **apEntry;` |
|       74 | 16195 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 16196 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 16197 | `			if( apEntry[n] == pEntry ){` |
|        - | 16198 | `				/* Nullify the entry */` |
|       74 | 16199 | `				apEntry[n] = 0;` |
|        - | 16200 | `				/*` |
|        - | 16201 | `				 * NOTE:` |
|        - | 16202 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16203 | `				 * we avoid wasting spaces.` |
|        - | 16204 | `				 */` |
|       36 | 16205 | `			}` |
|       97 | 16206 | `		}` |
|       36 | 16207 | `	}` |
|  3043300 | 16208 | `	if( pMapEntry ){` |
|        - | 16209 | `		ph7_hashmap_node **apNode;` |
|  3043228 | 16210 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6086548 | 16211 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3043322 | 16212 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16213 | `				/* nullify the entry */` |
|  3043228 | 16214 | `				apNode[n] = 0;` |
|  1521613 | 16215 | `			}` |
|  1521662 | 16216 | `		}` |
|  1521613 | 16217 | `	}` |
|  3043300 | 16218 | `	return SXRET_OK;` |
|  1574298 | 16219 |  |
|        - | 16220 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16221 | `/*` |
|        - | 16222 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16223 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16224 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16225 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16226 | ` * For more information on how to register IO stream devices,please` |
|        - | 16227 | ` * refer to the official documentation.` |
|        - | 16228 | ` */` |
|    29106 | 16229 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16230 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16231 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16232 | `	int nByte              /* *pzDevice length*/` |
|        - | 16233 | `	)` |
|        2 | 16234 |  |
|        - | 16235 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16236 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16237 | `	SyString sDev,sCur;` |
|        - | 16238 | `	sxu32 n,nEntry;` |
|        - | 16239 | `	int rc;` |
|        - | 16240 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29108 | 16241 | `	zNext = zCur = zIn = *pzDevice;` |
|    29108 | 16242 | `	zEnd = &zIn[nByte];` |
|  1859275 | 16243 | `	while( zIn < zEnd ){` |
|  1830171 | 16244 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16245 | `			/* Got one */` |
|        3 | 16246 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16247 | `			break;` |
|        - | 16248 | `		}` |
|        - | 16249 | `		/* Advance the cursor */` |
|  1830169 | 16250 | `		zIn++;` |
|        2 | 16251 | `	}` |
|    29108 | 16252 | `	if( zIn >= zEnd ){` |
|        - | 16253 | `		/* No such scheme,return the default stream */` |
|    29106 | 16254 | `		return pVm->pDefStream;` |
|        - | 16255 | `	}` |
|        3 | 16256 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16257 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16258 | `	SyStringFullTrim(&sDev);` |
|        - | 16259 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16260 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16261 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16262 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16263 | `		pStream = apStream[n];` |
|        3 | 16264 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16265 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16266 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16267 | `		if( rc == 0 ){` |
|        - | 16268 | `			/* Stream device found */` |
|        3 | 16269 | `			*pzDevice = zNext;` |
|        3 | 16270 | `			return pStream;` |
|        - | 16271 | `		}` |
|      ! 0 | 16272 | `	}` |
|        - | 16273 | `	/* No such stream,return NULL */` |
|      ! 0 | 16274 | `	return 0;` |
|    14555 | 16275 |  |
|        - | 16276 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16277 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16278 |  |
