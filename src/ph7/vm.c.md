# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6712/8589 lines (78.15%)

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
|   917474 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   917476 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   917442 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   917432 |   148 | `	return FALSE;` |
|   458761 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335474 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335476 |   162 | `	sxu8 bReal = FALSE;` |
|   335476 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335476 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335406 |   166 | `		return FALSE;` |
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
|   167761 |   183 |  |
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
|   278246 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   278248 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   278248 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   278248 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   278248 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   278248 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   278248 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   278248 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   278248 |   362 | `	pFunc->iFlags = iFlags;` |
|   278248 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   278248 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   278248 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   278248 |   368 | `	return SXRET_OK;` |
|        2 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1457150 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        2 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1457152 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    41900 |   388 | `		pName = &pFunc->sName;` |
|    20949 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1457152 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1457152 |   392 | `	if( pEntry ){` |
|  1260832 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1260832 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1260832 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   196322 |   402 | `	pFunc->pNextName = 0;` |
|   196322 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   196322 |   404 | `	return rc;` |
|   728577 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   120336 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        2 |   413 |  |
|   120338 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   120338 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120338 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   120308 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   120308 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120308 |   429 | `	return rc;` |
|    60170 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4258498 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4258500 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4258500 |   447 | `	sInstr.iP1 = iP1;` |
|  4258500 |   448 | `	sInstr.iP2 = iP2;` |
|  4258500 |   449 | `	sInstr.p3  = p3;` |
|  4258500 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   231244 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115621 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4258500 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4258500 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4258500 |   460 | `	return rc;` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   552612 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   466 |  |
|   552614 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   552614 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   552614 |   474 | `	return SXRET_OK;` |
|        2 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   276306 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   480 |  |
|   276308 |   481 | `	return pVm->pByteContainer;` |
|        2 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   228026 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   228028 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   228028 |   490 | `	return pInstr;` |
|        2 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1279016 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   496 |  |
|  1279018 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   210980 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   503 |  |
|   210982 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   838630 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   510 |  |
|   838632 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
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
|    22416 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        2 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    22418 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22418 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    22418 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    22418 |   542 | `	pFrame->pUserData = pUserData;` |
|    22418 |   543 | `	pFrame->pThis = pThis;` |
|    22418 |   544 | `	pFrame->pVm = pVm;` |
|    22418 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22418 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22418 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22418 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22418 |   549 | `	return pFrame;` |
|    11210 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    22370 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        2 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    22372 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22372 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    22372 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    22372 |   571 | `	pVm->pFrame = pFrame;` |
|    22372 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    19226 |   574 | `		*ppFrame = pFrame;` |
|     9612 |   575 | `	}` |
|    22372 |   576 | `	return SXRET_OK;` |
|    11187 |   577 |  |
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
|    19220 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   622 |  |
|    19222 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19222 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    19222 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19222 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18848 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   124218 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   105372 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52687 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    18848 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   124292 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   105446 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52724 |   640 | `			}` |
|     9423 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    19222 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19222 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    19222 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19222 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    19222 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9610 |   649 | `	}` |
|    19222 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7117846 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   657 |  |
|  7120068 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2222 |   659 | `		pFrame = pFrame->pParent;` |
|        2 |   660 | `	}` |
|  7117848 |   661 | `	return pFrame;` |
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
|   353946 |   809 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   810 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   811 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   812 | `	)` |
|        2 |   813 |  |
|        - |   814 | `	ph7_class_attr *pAttr;` |
|        - |   815 | `	SyHashEntry *pEntry;` |
|        - |   816 | `	/* Reset the loop cursor */` |
|   353948 |   817 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   818 | `	/* Process only static and constant attribute */` |
|  1400533 |   819 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   820 | `		/* Extract the current attribute */` |
|   869614 |   821 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   869614 |   822 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   353948 |   866 | `	return SXRET_OK;` |
|   176975 |   867 |  |
|   353714 |   868 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   869 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   870 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   871 | `	)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_class_method *pMeth;` |
|        - |   874 | `	SyHashEntry *pEntry;` |
|        - |   875 | `	sxi32 rc;` |
|        - |   876 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   353716 |   877 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   353716 |   878 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   879 | `		return rc;` |
|        - |   880 | `	}` |
|        - |   881 | `	/* Install class methods */` |
|   353716 |   882 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   883 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   884 | `		 */` |
|   191778 |   885 | `		return SXRET_OK;` |
|        - |   886 | `	}` |
|        - |   887 | `	/* Create constructor alias if not yet done */` |
|   161940 |   888 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   889 | `		/* User constructor with the same base class name */` |
|     6676 |   890 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6676 |   891 | `		if( pEntry ){` |
|      ! 0 |   892 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   893 | `			/* Create the alias */` |
|      ! 0 |   894 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   895 | `		}` |
|     3337 |   896 | `	}` |
|        - |   897 | `	/* Install the methods now */` |
|   161940 |   898 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1658169 |   899 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1415262 |   900 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1415262 |   901 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1415254 |   902 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1415254 |   903 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   904 | `				return rc;` |
|        - |   905 | `			}` |
|   707626 |   906 | `		}` |
|        2 |   907 | `	}` |
|        - |   908 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   161940 |   909 | `	pClass->bMounted = TRUE;` |
|   161940 |   910 | `	return SXRET_OK;` |
|   176859 |   911 |  |
|        - |   912 | `/*` |
|        - |   913 | ` * Allocate a private frame for attributes of the given` |
|        - |   914 | ` * class instance (Object in the PHP jargon).` |
|        - |   915 | ` */` |
|     2104 |   916 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   917 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   918 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   919 | `	)` |
|        2 |   920 |  |
|     2106 |   921 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   922 | `	ph7_class_attr *pAttr;` |
|        - |   923 | `	SyHashEntry *pEntry;` |
|        - |   924 | `	sxi32 rc;` |
|        - |   925 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2106 |   926 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8724 |   927 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   928 | `		VmClassAttr *pVmAttr;` |
|        - |   929 | `		/* Extract the current attribute */` |
|     6620 |   930 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6620 |   931 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6620 |   932 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   933 | `			return SXERR_MEM;` |
|        - |   934 | `		}` |
|     6620 |   935 | `		pVmAttr->pAttr = pAttr;` |
|     6620 |   936 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   937 | `			ph7_value *pMemObj;` |
|        - |   938 | `			/* Reserve a memory object for this attribute */` |
|     6594 |   939 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6594 |   940 | `			if( pMemObj == 0 ){` |
|      ! 0 |   941 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   942 | `				return SXERR_MEM;` |
|        - |   943 | `			}` |
|     6594 |   944 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6594 |   945 | `			pVmAttr->iState = 0;` |
|     6594 |   946 | `			pVmAttr->pOwner = pClass;` |
|     6594 |   947 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   948 | `				/* Initialize attribute default value (any complex expression) */` |
|     2272 |   949 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5459 |   950 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   951 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   952 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   953 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   954 | `			}` |
|     6594 |   955 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6594 |   956 | `			if( rc != SXRET_OK ){` |
|        - |   957 | `				VmSlot sSlot;` |
|        - |   958 | `				/* Restore memory object */` |
|      ! 0 |   959 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   960 | `				sSlot.pUserData = 0;` |
|      ! 0 |   961 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   962 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   963 | `				return SXERR_MEM;` |
|        - |   964 | `			}` |
|        - |   965 | `			/* Install attribute in the reference table */` |
|     6594 |   966 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   967 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   968 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   969 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6594 |   970 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      178 |   971 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      178 |   972 | `				if( rc != SXRET_OK ){` |
|        - |   973 | `					VmSlot sSlot;` |
|      ! 0 |   974 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   975 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   976 | `					sSlot.pUserData = 0;` |
|      ! 0 |   977 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   978 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   979 | `					return SXERR_MEM;` |
|        - |   980 | `				}` |
|       88 |   981 | `			}` |
|     3298 |   982 | `		}else{` |
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
|     2106 |   994 | `	return SXRET_OK;` |
|     1054 |   995 |  |
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
|   455994 |  1007 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1008 |  |
|        - |  1009 | `	ph7_value *pObj;` |
|        - |  1010 | `	sxi32 rc;` |
|   455996 |  1011 | `	if( pIndex ){` |
|        - |  1012 | `		/* Object index in the object table */` |
|   446576 |  1013 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   223287 |  1014 | `	}` |
|        - |  1015 | `	/* Reserve a slot for the new object */` |
|   455996 |  1016 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   455996 |  1017 | `	if( rc != SXRET_OK ){` |
|        - |  1018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1020 | `		 */` |
|      ! 0 |  1021 | `		return 0;` |
|        - |  1022 | `	}` |
|   455996 |  1023 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   455996 |  1024 | `	return pObj;` |
|   227999 |  1025 |  |
|        - |  1026 | `/*` |
|        - |  1027 | ` * Reserve a memory object.` |
|        - |  1028 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1029 | ` */` |
|  2151966 |  1030 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1031 |  |
|        - |  1032 | `	ph7_value *pObj;` |
|        - |  1033 | `	sxi32 rc;` |
|  2151968 |  1034 | `	if( pIndex ){` |
|        - |  1035 | `		/* Object index in the object table */` |
|  2151968 |  1036 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075983 |  1037 | `	}` |
|        - |  1038 | `	/* Reserve a slot for the new object */` |
|  2151968 |  1039 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151968 |  1040 | `	if( rc != SXRET_OK ){` |
|        - |  1041 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1042 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1043 | `		 */` |
|      ! 0 |  1044 | `		return 0;` |
|        - |  1045 | `	}` |
|  2151968 |  1046 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151968 |  1047 | `	return pObj;` |
|  1075985 |  1048 |  |
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
|    20606 |  1735 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1736 |  |
|    20608 |  1737 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20608 |  1738 | `	if( xCons != VmObConsumer ){` |
|     8222 |  1739 | `		pVm->nOutputLen += nLen;` |
|     8222 |  1740 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1024 |  1741 | `			pVm->bHeadersSent = 1;` |
|      511 |  1742 | `		}` |
|     4110 |  1743 | `	}` |
|    20608 |  1744 |  |
|        - |  1745 | `#define VM_STACK_GUARD 16` |
|        - |  1746 | `/*` |
|        - |  1747 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1748 | ` * our compiled PHP program.` |
|        - |  1749 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1750 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1751 | ` */` |
|    45034 |  1752 | `static ph7_value * VmNewOperandStack(` |
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
|    45036 |  1765 | `	nInstr += VM_STACK_GUARD;` |
|    45036 |  1766 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    45036 |  1767 | `	if( pStack == 0 ){` |
|      ! 0 |  1768 | `		return 0;` |
|        - |  1769 | `	}` |
|        - |  1770 | `	/* Initialize the operand stack */` |
|  3035104 |  1771 | `	while( nInstr > 0 ){` |
|  2990070 |  1772 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2990070 |  1773 | `		--nInstr;` |
|        2 |  1774 | `	}` |
|        - |  1775 | `	/* Ready for bytecode execution */` |
|    45036 |  1776 | `	return pStack;` |
|    22519 |  1777 |  |
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
|   696996 |  2166 | `static sxi32 VmInitCallContext(` |
|        - |  2167 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2168 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2169 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2170 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2171 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2172 | `	)` |
|        2 |  2173 |  |
|   696998 |  2174 | `	pOut->pFunc = pFunc;` |
|   696998 |  2175 | `	pOut->pVm   = pVm;` |
|   696998 |  2176 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   696998 |  2177 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2178 | `	/* Assume a null return value */` |
|   696998 |  2179 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   696998 |  2180 | `	pOut->pRet = pRet;` |
|   696998 |  2181 | `	pOut->iFlags = iFlags;` |
|   696998 |  2182 | `	return SXRET_OK;` |
|        2 |  2183 |  |
|        - |  2184 | `/*` |
|        - |  2185 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2186 | ` * left behind.` |
|        - |  2187 | ` */` |
|   696996 |  2188 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  2189 |  |
|        - |  2190 | `	sxu32 n;` |
|   696998 |  2191 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8632 |  2192 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25216 |  2193 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16586 |  2194 | `			if( apObj[n] == 0 ){` |
|        - |  2195 | `				/* Already released */` |
|      384 |  2196 | `				continue;` |
|        - |  2197 | `			}` |
|    16204 |  2198 | `			PH7_MemObjRelease(apObj[n]);` |
|    16204 |  2199 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8103 |  2200 | `		}` |
|     8632 |  2201 | `		SySetRelease(&pCtx->sVar);` |
|     4315 |  2202 | `	}` |
|   696998 |  2203 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   696998 |  2219 |  |
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
|  3950146 |  2250 | `static void VmPopOperand(` |
|        - |  2251 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2252 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2253 | `	)` |
|        2 |  2254 |  |
|  3950148 |  2255 | `	ph7_value *pTos = *ppTos;` |
|  8416254 |  2256 | `	while( nPop > 0 ){` |
|  4466108 |  2257 | `		PH7_MemObjRelease(pTos);` |
|  4466108 |  2258 | `		pTos--;` |
|  4466108 |  2259 | `		nPop--;` |
|        2 |  2260 | `	}` |
|        - |  2261 | `	/* Top of the stack */` |
|  3950148 |  2262 | `	*ppTos = pTos;` |
|  3950148 |  2263 |  |
|        - |  2264 | `/*` |
|        - |  2265 | ` * Reserve a memory object.` |
|        - |  2266 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2267 | ` */` |
|  3205928 |  2268 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  2269 |  |
|  3205930 |  2270 | `	ph7_value *pObj = 0;` |
|        - |  2271 | `	VmSlot *pSlot;` |
|        - |  2272 | `	sxu32 nIdx;` |
|        - |  2273 | `	/* Check for a free slot */` |
|  3205930 |  2274 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3205930 |  2275 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3205930 |  2276 | `	if( pSlot ){` |
|  1053970 |  2277 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1053970 |  2278 | `		nIdx = pSlot->nIdx;` |
|   526984 |  2279 | `	}` |
|  3205930 |  2280 | `	if( pObj == 0 ){` |
|        - |  2281 | `		/* Reserve a new memory object */` |
|  2151962 |  2282 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151962 |  2283 | `		if( pObj == 0 ){` |
|      ! 0 |  2284 | `			return 0;` |
|        - |  2285 | `		}` |
|  1075980 |  2286 | `	}` |
|        - |  2287 | `	/* Set a null default value */` |
|  3205930 |  2288 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3205930 |  2289 | `	pObj->nIdx = nIdx;` |
|  3205930 |  2290 | `	return pObj;` |
|  1602966 |  2291 |  |
|        - |  2292 | `/*` |
|        - |  2293 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2294 | ` */` |
|    35378 |  2295 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2296 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2297 | `	const char *zKey,  /* Entry key */` |
|        - |  2298 | `	sxu32 nByte,       /* Key length */` |
|        - |  2299 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2300 | `	)` |
|        2 |  2301 |  |
|        - |  2302 | `	ph7_value sKey;` |
|        - |  2303 | `	sxi32 rc;` |
|    35380 |  2304 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35380 |  2305 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2306 | `	/* Perform the insertion */` |
|    35380 |  2307 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35380 |  2308 | `	PH7_MemObjRelease(&sKey);` |
|    35380 |  2309 | `	return rc;` |
|        2 |  2310 |  |
|        - |  2311 | `/*` |
|        - |  2312 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2313 | ` * Return a pointer to the variable value on success.` |
|        - |  2314 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2315 | ` */` |
|  3670630 |  2316 | `static ph7_value * VmExtractMemObj(` |
|        - |  2317 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2318 | `	const SyString *pName, /* Variable name */` |
|        - |  2319 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2320 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2321 | `	)` |
|        2 |  2322 |  |
|  3670632 |  2323 | `	int bNullify = FALSE;` |
|        - |  2324 | `	SyHashEntry *pEntry;` |
|        - |  2325 | `	VmFrame *pFrame;` |
|        - |  2326 | `	ph7_value *pObj;` |
|        - |  2327 | `	sxu32 nIdx;` |
|        - |  2328 | `	sxi32 rc;` |
|        - |  2329 | `	/* Point to the top active frame */` |
|  3670632 |  2330 | `	pFrame = pVm->pFrame;` |
|  3670632 |  2331 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2332 | `	/* Perform the lookup */` |
|  3670632 |  2333 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2334 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2335 | `		pName = &sAnnon;` |
|        - |  2336 | `		/* Always nullify the object */` |
|      ! 0 |  2337 | `		bNullify = TRUE;` |
|      ! 0 |  2338 | `		bDup = FALSE;` |
|      ! 0 |  2339 | `	}` |
|        - |  2340 | `	/* Check the superglobals table first */` |
|  3670632 |  2341 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3670632 |  2342 | `	if( pEntry == 0 ){` |
|        - |  2343 | `		/* Query the top active frame */` |
|  3670586 |  2344 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3670586 |  2345 | `		if( pEntry == 0 ){` |
|   113430 |  2346 | `			char *zName = (char *)pName->zString;` |
|        - |  2347 | `			VmSlot sLocal;` |
|   113430 |  2348 | `			if( !bCreate ){` |
|        - |  2349 | `				/* Do not create the variable,return NULL instead */` |
|      986 |  2350 | `				return 0;` |
|        - |  2351 | `			}` |
|        - |  2352 | `			/* No such variable,automatically create a new one and install` |
|        - |  2353 | `			 * it in the current frame.` |
|        - |  2354 | `			 */` |
|   112446 |  2355 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   112446 |  2356 | `			if( pObj == 0 ){` |
|      ! 0 |  2357 | `				return 0;` |
|        - |  2358 | `			}` |
|   112446 |  2359 | `			nIdx = pObj->nIdx;` |
|   112446 |  2360 | `			if( bDup ){` |
|        - |  2361 | `				/* Duplicate name */` |
|      230 |  2362 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2363 | `				if( zName == 0 ){` |
|      ! 0 |  2364 | `					return 0;` |
|        - |  2365 | `				}` |
|      114 |  2366 | `			}` |
|        - |  2367 | `			/* Link to the top active VM frame */` |
|   112446 |  2368 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   112446 |  2369 | `			if( rc != SXRET_OK ){` |
|        - |  2370 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2371 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2372 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2373 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2374 | `				return 0;` |
|        - |  2375 | `			}` |
|   112446 |  2376 | `			if( pFrame->pParent != 0 ){` |
|        - |  2377 | `				/* Local variable */` |
|   105420 |  2378 | `				sLocal.nIdx = nIdx;` |
|   105420 |  2379 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52711 |  2380 | `			}else{` |
|        - |  2381 | `				/* Register in the $GLOBALS array */` |
|     7028 |  2382 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2383 | `			}` |
|        - |  2384 | `			/* Install in the reference table */` |
|   112446 |  2385 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2386 | `			/* Save object index */` |
|   112446 |  2387 | `			pObj->nIdx = nIdx;` |
|    56224 |  2388 | `		}else{` |
|        - |  2389 | `			/* Extract variable contents */` |
|  3557158 |  2390 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3557158 |  2391 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3557158 |  2392 | `			if( bNullify && pObj ){` |
|      ! 0 |  2393 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2394 | `			}` |
|        - |  2395 | `		}` |
|  1834912 |  2396 | `	}else{` |
|        - |  2397 | `		/* Superglobal */` |
|       48 |  2398 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       48 |  2399 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2400 | `	}` |
|  3669648 |  2401 | `	return pObj;` |
|  1835427 |  2402 |  |
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
|       40 |  3065 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        2 |  3066 |  |
|        - |  3067 | `	ph7_class *pClass;` |
|       42 |  3068 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  3069 | `	ph7_class_instance *pThis;` |
|        - |  3070 | `	ph7_class_method *pCons;` |
|        - |  3071 | `	ph7_value sArg;` |
|        - |  3072 | `	ph7_value *apArg[1];` |
|        - |  3073 | `	SyBlob sMsg;` |
|        - |  3074 | `	SyString sMsgStr;` |
|        - |  3075 | `	VmFrame *pFrame;` |
|        - |  3076 | `	sxi32 rc;` |
|       42 |  3077 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       42 |  3078 | `	if( pClass == 0 ){` |
|      ! 0 |  3079 | `		return PH7_ABORT;` |
|        - |  3080 | `	}` |
|       42 |  3081 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       42 |  3082 | `	if( pThis == 0 ){` |
|      ! 0 |  3083 | `		return PH7_ABORT;` |
|        - |  3084 | `	}` |
|       42 |  3085 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3086 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3087 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  3088 | `	{` |
|       42 |  3089 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       42 |  3090 | `		if( pOwner ){` |
|       42 |  3091 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       20 |  3092 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       22 |  3093 | `		}else{` |
|      ! 0 |  3094 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3095 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3096 | `		}` |
|        - |  3097 | `	}` |
|       42 |  3098 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       42 |  3099 | `	if( pCons ){` |
|       42 |  3100 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       42 |  3101 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       42 |  3102 | `		apArg[0] = &sArg;` |
|       42 |  3103 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       42 |  3104 | `		PH7_MemObjRelease(&sArg);` |
|       20 |  3105 | `	}` |
|       42 |  3106 | `	SyBlobRelease(&sMsg);` |
|       42 |  3107 | `	pFrame = pVm->pFrame;` |
|       42 |  3108 | `	if( pFrame ){` |
|       42 |  3109 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       42 |  3110 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       20 |  3111 | `	}` |
|       42 |  3112 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       42 |  3113 | `	PH7_ClassInstanceUnref(pThis);` |
|       42 |  3114 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3115 | `		return PH7_ABORT;` |
|        - |  3116 | `	}` |
|       42 |  3117 | `	return PH7_EXCEPTION;` |
|       22 |  3118 |  |
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
|       22 |  3186 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  3187 |  |
|        - |  3188 | `	const char *z, *zEnd, *zTail;` |
|        - |  3189 | `	sxu32 n;` |
|        - |  3190 | `	sxu8 bReal;` |
|        - |  3191 | `	sxi32 rc;` |
|       24 |  3192 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3193 | `		return 0;` |
|        - |  3194 | `	}` |
|       24 |  3195 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       24 |  3196 | `	n = SyBlobLength(&pValue->sBlob);` |
|       24 |  3197 | `	zEnd = z + n;` |
|       24 |  3198 | `	if( n == 0 ){` |
|      ! 0 |  3199 | `		return 0;` |
|        - |  3200 | `	}` |
|       24 |  3201 | `	zTail = 0;` |
|       24 |  3202 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       24 |  3203 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  3204 | `		return 0;` |
|        - |  3205 | `	}` |
|        - |  3206 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       18 |  3207 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3208 | `		zTail++;` |
|      ! 0 |  3209 | `	}` |
|       18 |  3210 | `	return zTail == zEnd ? 1 : 0;` |
|       13 |  3211 |  |
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
|     6426 |  3445 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3446 |  |
|        - |  3447 | `	SyHashEntry *pSlot;` |
|        - |  3448 | `	VmClassAttr *pVmAttr;` |
|        - |  3449 | `	ph7_class_attr *pAttr;` |
|     6428 |  3450 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6428 |  3451 | `	if( pSlot == 0 ){` |
|     6216 |  3452 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3453 | `	}` |
|      214 |  3454 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      214 |  3455 | `	pAttr = pVmAttr->pAttr;` |
|      214 |  3456 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3457 | `		return SXRET_OK;` |
|        - |  3458 | `	}` |
|        - |  3459 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3460 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3461 | `	 * matching PHP's documented behavior. */` |
|      214 |  3462 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
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
|      200 |  3478 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3479 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3480 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3481 | `			return SXRET_OK;` |
|        - |  3482 | `		}` |
|        3 |  3483 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3484 | `	}` |
|        - |  3485 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3486 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3487 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      188 |  3488 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3489 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3490 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3491 | `			return SXRET_OK;` |
|        - |  3492 | `		}` |
|        7 |  3493 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3494 | `	}` |
|      178 |  3495 | `	if( pAttr->nType == SXU32_HIGH ){` |
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
|      154 |  3528 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3529 | `		char zBuf[128];` |
|       10 |  3530 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3531 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3532 | `	}` |
|      148 |  3533 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       28 |  3534 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       28 |  3535 | `		if( xCast ){` |
|        - |  3536 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       28 |  3537 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3538 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3539 | `			}` |
|       26 |  3540 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3541 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3542 | `			}` |
|        - |  3543 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3544 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3545 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       29 |  3546 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       19 |  3547 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       21 |  3548 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|       12 |  3549 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3550 | `			}` |
|       12 |  3551 | `			xCast(pValue);` |
|        5 |  3552 | `		}` |
|        5 |  3553 | `	}` |
|      132 |  3554 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      132 |  3555 | `	return SXRET_OK;` |
|     3215 |  3556 |  |
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
|      870 |  4038 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  4039 |  |
|      872 |  4040 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  4041 | `		if( pVm->pCoalesceObj ){` |
|        7 |  4042 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4043 | `		}` |
|        7 |  4044 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  4045 | `		pVm->pCoalesceObj = 0;` |
|        7 |  4046 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4047 | `	}` |
|      872 |  4048 |  |
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
|    45138 |  4323 | `static sxi32 VmByteCodeExec(` |
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
|    45140 |  4342 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    45140 |  4343 | `	if( nTos < 0 ){` |
|    41964 |  4344 | `		pTos = &pStack[-1];` |
|    20983 |  4345 | `	}else{` |
|     3178 |  4346 | `		pTos = &pStack[nTos];` |
|        - |  4347 | `	}` |
|    45140 |  4348 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    45140 |  4349 | `	pc = nPc;` |
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
|  5907461 |  4370 | `	for(;;){` |
|        - |  4371 | `		/* Fetch the instruction to execute */` |
| 11814220 |  4372 | `		pInstr = &aInstr[pc];` |
| 11814220 |  4373 | `		rc = SXRET_OK;` |
|        - |  4374 | `/*` |
|        - |  4375 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4376 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4377 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4378 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4379 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4380 | ` */` |
| 11814220 |  4381 | `		switch(pInstr->iOp){` |
|        - |  4382 | `/*` |
|        - |  4383 | ` * DONE: P1 * *` |
|        - |  4384 | ` *` |
|        - |  4385 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4386 | ` * and return immediately.` |
|        - |  4387 | ` */` |
|    22191 |  4388 | `case PH7_OP_DONE:` |
|        - |  4389 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4390 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4391 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4392 | `	 * callback trampolines, and the main script. */` |
|    44382 |  4393 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
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
|    44378 |  4419 | `	if( pInstr->iP1 ){` |
|        - |  4420 | `#ifdef UNTRUST` |
|        - |  4421 | `		if( pTos < pStack ){` |
|        - |  4422 | `			goto Abort;` |
|        - |  4423 | `		}` |
|        - |  4424 | `#endif` |
|    26984 |  4425 | `		if( pLastRef ){` |
|    16418 |  4426 | `			*pLastRef = pTos->nIdx;` |
|     8208 |  4427 | `		}` |
|    26984 |  4428 | `		if( pResult ){` |
|        - |  4429 | `			/* Execution result */` |
|    25482 |  4430 | `			PH7_MemObjStore(pTos,pResult);` |
|    12740 |  4431 | `		}` |
|    26984 |  4432 | `		VmPopOperand(&pTos,1);` |
|    30887 |  4433 | `	}else if( pLastRef ){` |
|        - |  4434 | `		/* Nothing referenced */` |
|     1990 |  4435 | `		*pLastRef = SXU32_HIGH;` |
|      994 |  4436 | `	}` |
|        - |  4437 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4438 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4439 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4440 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4441 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4442 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4443 | `	 * block can override it.` |
|        - |  4444 | `	 */` |
|    44380 |  4445 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    44378 |  4460 | `	goto Done;` |
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
|   251655 |  4505 | `case PH7_OP_JMP:` |
|   503356 |  4506 | `	pc = pInstr->iP2 - 1;` |
|   503356 |  4507 | `	break;` |
|        - |  4508 | `/*` |
|        - |  4509 | ` * JZ: P1 P2 *` |
|        - |  4510 | ` *` |
|        - |  4511 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4512 | ` * entry in the stack if P1 is zero.` |
|        - |  4513 | ` */` |
|   597528 |  4514 | `case PH7_OP_JZ:` |
|        - |  4515 | `#ifdef UNTRUST` |
|        - |  4516 | `	if( pTos < pStack ){` |
|        - |  4517 | `		goto Abort;` |
|        - |  4518 | `	}` |
|        - |  4519 | `#endif` |
|        - |  4520 | `	/* Get a boolean value */` |
|  1195146 |  4521 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4522 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4523 | `	}` |
|  1195146 |  4524 | `	if( !pTos->x.iVal ){` |
|        - |  4525 | `		/* Take the jump */` |
|   615018 |  4526 | `		pc = pInstr->iP2 - 1;` |
|   307508 |  4527 | `	}` |
|  1195146 |  4528 | `	if( !pInstr->iP1 ){` |
|   946970 |  4529 | `		VmPopOperand(&pTos,1);` |
|   473506 |  4530 | `	}` |
|  1195146 |  4531 | `	break;` |
|        - |  4532 | `/*` |
|        - |  4533 | ` * JNZ: P1 P2 *` |
|        - |  4534 | ` *` |
|        - |  4535 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4536 | ` * entry in the stack if P1 is zero.` |
|        - |  4537 | ` */` |
|    61368 |  4538 | `case PH7_OP_JNZ:` |
|        - |  4539 | `#ifdef UNTRUST` |
|        - |  4540 | `	if( pTos < pStack ){` |
|        - |  4541 | `		goto Abort;` |
|        - |  4542 | `	}` |
|        - |  4543 | `#endif` |
|        - |  4544 | `	/* Get a boolean value */` |
|   122738 |  4545 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4546 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4547 | `	}` |
|   122738 |  4548 | `	if( pTos->x.iVal ){` |
|        - |  4549 | `		/* Take the jump */` |
|     5598 |  4550 | `		pc = pInstr->iP2 - 1;` |
|     2798 |  4551 | `	}` |
|   122738 |  4552 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4553 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4554 | `	}` |
|   122738 |  4555 | `	break;` |
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
|   463189 |  4569 | `case PH7_OP_POP: {` |
|   926424 |  4570 | `	sxi32 n = pInstr->iP1;` |
|   926424 |  4571 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4572 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4573 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4574 | `	}` |
|   926424 |  4575 | `	VmPopOperand(&pTos,n);` |
|   926424 |  4576 | `	break;` |
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
|     7830 |  4599 | `case PH7_OP_NSSWITCH:` |
|    15662 |  4600 | `	SyBlobReset(&pVm->sNamespace);` |
|    15662 |  4601 | `	if( pInstr->p3 ){` |
|      100 |  4602 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4603 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4604 | `	}` |
|        - |  4605 | `	/* Clear namespace-scoped use-const imports */` |
|    15662 |  4606 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15662 |  4607 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15662 |  4608 | `	break;` |
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
|    16067 |  4752 | `case PH7_OP_ERR_CTRL:` |
|        - |  4753 | `	/*` |
|        - |  4754 | `	 * TICKET 1433-038:` |
|        - |  4755 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4756 | `	 * use the public API,to control error output.` |
|        - |  4757 | `	 */` |
|    32134 |  4758 | `	break;` |
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
|  1016600 |  4818 | `case PH7_OP_LOADC: {` |
|        - |  4819 | `	ph7_value *pObj;` |
|        - |  4820 | `	/* Reserve a room */` |
|  2033246 |  4821 | `	pTos++;` |
|  3040007 |  4822 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2033246 |  4823 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4824 | `			SyHashEntry *pEntry;` |
|        - |  4825 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4826 | `			{` |
|        - |  4827 | `				SyHashEntry *pConstImport;` |
|    29657 |  4828 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19770 |  4829 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19772 |  4830 | `				if( pConstImport ){` |
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
|    19762 |  4845 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19762 |  4846 | `			if( pEntry ){` |
|    19756 |  4847 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4848 | `				/* Set a NULL default value */` |
|    19756 |  4849 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19756 |  4850 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4851 | `				/* Invoke the callback and deal with the expanded value */` |
|    19756 |  4852 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4853 | `				/* Mark as constant */` |
|    19756 |  4854 | `				pTos->nIdx = SXU32_HIGH;` |
|    19756 |  4855 | `				break;` |
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
|  2013480 |  4904 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1006763 |  4905 | `	}else{` |
|        - |  4906 | `		/* Set a NULL value */` |
|      ! 0 |  4907 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4908 | `	}` |
|  1006718 |  4909 | `LoadC_Done:` |
|        - |  4910 | `	/* Mark as constant */` |
|  2013482 |  4911 | `	pTos->nIdx = SXU32_HIGH;` |
|  2013482 |  4912 | `	break;` |
|        - |  4913 | `				  }` |
|        - |  4914 | `/*` |
|        - |  4915 | ` * LOAD: P1 * P3` |
|        - |  4916 | ` *` |
|        - |  4917 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4918 | ` * from the P3 operand.` |
|        - |  4919 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4920 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4921 | ` */` |
|  1577104 |  4922 | `case PH7_OP_LOAD:{` |
|        - |  4923 | `	ph7_value *pObj;` |
|        - |  4924 | `	SyString sName;` |
|  3154430 |  4925 | `	if( pInstr->p3 == 0 ){` |
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
|  3154412 |  4938 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4939 | `		/* Reserve a room for the target object */` |
|  3154412 |  4940 | `		pTos++;` |
|        - |  4941 | `	}` |
|        - |  4942 | `	/* Extract the requested memory object */` |
|  3154430 |  4943 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3154430 |  4944 | `	if( pObj == 0 ){` |
|      858 |  4945 | `		if( pInstr->iP1 ){` |
|        - |  4946 | `			/* Variable not found,load NULL */` |
|      858 |  4947 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4948 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4949 | `			}else{` |
|      858 |  4950 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4951 | `			}` |
|      858 |  4952 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1577534 |  4953 | `			break;` |
|      ! 0 |  4954 | `		}else{` |
|        - |  4955 | `			/* Fatal error */` |
|      ! 0 |  4956 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4957 | `			goto Abort;` |
|        - |  4958 | `		}` |
|        - |  4959 | `	}` |
|        - |  4960 | `	/* Load variable contents */` |
|  3153574 |  4961 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3153574 |  4962 | `	pTos->nIdx = pObj->nIdx;` |
|  3153574 |  4963 | `	break;` |
|        - |  4964 | `				   }` |
|        - |  4965 | `/*` |
|        - |  4966 | ` * LOAD_MAP P1 * *` |
|        - |  4967 | ` *` |
|        - |  4968 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4969 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4970 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4971 | ` */` |
|    22820 |  4972 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4973 | `	ph7_hashmap *pMap;` |
|        - |  4974 | `	/* Allocate a new hashmap instance */` |
|    45642 |  4975 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45642 |  4976 | `	if( pMap == 0 ){` |
|      ! 0 |  4977 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4978 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4979 | `		goto Abort;` |
|        - |  4980 | `	}` |
|    45642 |  4981 | `	if( pInstr->iP1 > 0 ){` |
|     2788 |  4982 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2788 |  4983 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4984 | `		/* Perform the insertion */` |
|     8514 |  4985 | `		while( pEntry < pTos ){` |
|     5744 |  4986 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
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
|     5702 |  5006 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5007 | `				/* Insertion by reference */` |
|      151 |  5008 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5009 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5010 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5011 | `					);` |
|       51 |  5012 | `			}else{` |
|        - |  5013 | `				/* Standard insertion */` |
|     8363 |  5014 | `				PH7_HashmapInsert(pMap,` |
|     5574 |  5015 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2787 |  5016 | `					&pEntry[1]` |
|        - |  5017 | `				);` |
|        - |  5018 | `			}` |
|        - |  5019 | `			/* Next pair on the stack */` |
|     5728 |  5020 | `			pEntry += 2;` |
|        2 |  5021 | `		}` |
|        - |  5022 | `		/* Pop P1 elements */` |
|     2788 |  5023 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2788 |  5024 | `		if( rcSpread != SXRET_OK ){` |
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
|     1385 |  5039 | `	}` |
|        - |  5040 | `	/* Push the hashmap */` |
|    45626 |  5041 | `	pTos++;` |
|    45626 |  5042 | `	pTos->nIdx = SXU32_HIGH;` |
|    45626 |  5043 | `	pTos->x.pOther = pMap;` |
|    45626 |  5044 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45626 |  5045 | `	break;` |
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
|   250737 |  5134 | `case PH7_OP_LOAD_IDX: {` |
|   501520 |  5135 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   501520 |  5136 | `	ph7_hashmap *pMap = 0;` |
|        - |  5137 | `	ph7_value *pIdx;` |
|   501520 |  5138 | `	pIdx = 0;` |
|   501520 |  5139 | `	if( pInstr->iP1 == 0 ){` |
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
|   501520 |  5156 | `		pIdx = pTos;` |
|   501520 |  5157 | `		pTos--;` |
|        - |  5158 | `	}` |
|   501520 |  5159 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
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
|   113806 |  5184 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
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
|   113682 |  5345 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5346 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5347 | `			ph7_value *pObj;` |
|        3 |  5348 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5349 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5350 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5351 | `			}` |
|        1 |  5352 | `		}` |
|        1 |  5353 | `	}` |
|   113682 |  5354 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   113682 |  5355 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   113682 |  5356 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5357 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5358 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5359 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5360 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5361 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5362 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      896 |  5363 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5364 | `		}` |
|        - |  5365 | `		/* Point to the hashmap */` |
|   113682 |  5366 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   113682 |  5367 | `		if( pIdx ){` |
|        - |  5368 | `			/* Load the desired entry */` |
|   113682 |  5369 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56840 |  5370 | `		}` |
|   113682 |  5371 | `		if( pInstr->iP2 == 3 ){` |
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
|   113682 |  5399 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5400 | `			/* Create a new empty entry */` |
|      273 |  5401 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5402 | `			if( rc == SXRET_OK ){` |
|        - |  5403 | `				/* Point to the last inserted entry */` |
|      273 |  5404 | `				pNode = pMap->pLast;` |
|      136 |  5405 | `			}` |
|      136 |  5406 | `		}` |
|    56840 |  5407 | `	}` |
|   113682 |  5408 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5409 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5410 | `		char zMsg[128];` |
|      ! 0 |  5411 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5412 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5413 | `		}` |
|      ! 0 |  5414 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5415 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5416 | `	}` |
|   113682 |  5417 | `	if( pIdx ){` |
|   113682 |  5418 | `		PH7_MemObjRelease(pIdx);` |
|    56840 |  5419 | `	}` |
|   113682 |  5420 | `	if( rc == SXRET_OK ){` |
|        - |  5421 | `		/* Load entry contents */` |
|    50404 |  5422 | `		if( pMap->iRef < 2 ){` |
|        - |  5423 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5424 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5425 | `			 */` |
|       28 |  5426 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5427 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5428 | `		}else{` |
|    50378 |  5429 | `			pTos->nIdx = pNode->nValIdx;` |
|    50378 |  5430 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50378 |  5431 | `			PH7_HashmapUnref(pMap);` |
|        - |  5432 | `		}` |
|    25203 |  5433 | `	}else{` |
|        - |  5434 | `		/* No such entry,load NULL */` |
|    63280 |  5435 | `		PH7_MemObjRelease(pTos);` |
|    63280 |  5436 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5437 | `	}` |
|   113682 |  5438 | `	break;` |
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
|   146254 |  5518 | `case PH7_OP_STORE: {` |
|        - |  5519 | `	ph7_value *pObj;` |
|        - |  5520 | `	SyString sName;` |
|        - |  5521 | `#ifdef UNTRUST` |
|        - |  5522 | `	if( pTos < pStack ){` |
|        - |  5523 | `		goto Abort;` |
|        - |  5524 | `	}` |
|        - |  5525 | `#endif` |
|   292510 |  5526 | `	if( pInstr->iP2 ){` |
|        - |  5527 | `		sxu32 nIdx;` |
|        - |  5528 | `		sxi32 rcT;` |
|        - |  5529 | `		/* Member store operation */` |
|     5280 |  5530 | `		nIdx = pTos->nIdx;` |
|     5280 |  5531 | `		VmPopOperand(&pTos,1);` |
|     5280 |  5532 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5533 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5534 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5535 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5536 | `		}else{` |
|        - |  5537 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5538 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5276 |  5539 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5276 |  5540 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5541 | `				goto Abort;` |
|        - |  5542 | `			}` |
|     5276 |  5543 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5544 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5545 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5546 | `				 * propagate out of the VM loop. */` |
|       37 |  5547 | `				VmPopOperand(&pTos,1);` |
|        - |  5548 | `				{` |
|       37 |  5549 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5550 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5551 | `						pc = pFrm2->iExceptionJump - 1;` |
|   146273 |  5552 | `						break;` |
|        - |  5553 | `					}` |
|        - |  5554 | `				}` |
|      ! 0 |  5555 | `				goto Exception;` |
|        - |  5556 | `			}` |
|        - |  5557 | `			/* Point to the desired memory object */` |
|     5240 |  5558 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5240 |  5559 | `			if( pObj ){` |
|        - |  5560 | `				/* Perform the store operation */` |
|     5240 |  5561 | `				PH7_MemObjStore(pTos,pObj);` |
|     2619 |  5562 | `			}` |
|        - |  5563 | `		}` |
|     5244 |  5564 | `		break;` |
|   287232 |  5565 | `	}else if( pInstr->p3 == 0 ){` |
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
|   287226 |  5579 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5580 | `	}` |
|        - |  5581 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   287232 |  5582 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   287232 |  5583 | `	if( pObj == 0 ){` |
|      ! 0 |  5584 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5585 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5586 | `		goto Abort;` |
|        - |  5587 | `	}` |
|   287232 |  5588 | `	if( !pInstr->p3 ){` |
|        7 |  5589 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5590 | `	}` |
|        - |  5591 | `	/* Perform the store operation */` |
|   287232 |  5592 | `	PH7_MemObjStore(pTos,pObj);` |
|   287232 |  5593 | `	break;` |
|        - |  5594 | `				   }` |
|        - |  5595 | `/*` |
|        - |  5596 | ` * STORE_IDX:   P1 * P3` |
|        - |  5597 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5598 | ` *` |
|        - |  5599 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5600 | ` */` |
|    97156 |  5601 | `case PH7_OP_STORE_IDX:` |
|        - |  5602 | `case PH7_OP_STORE_IDX_REF: {` |
|   194314 |  5603 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5604 | `	ph7_value *pKey;` |
|        - |  5605 | `	sxu32 nIdx;` |
|   194314 |  5606 | `	if( pInstr->iP1 ){` |
|        - |  5607 | `		/* Key is next on stack */` |
|    63352 |  5608 | `		pKey = pTos;` |
|    63352 |  5609 | `		pTos--;` |
|    31677 |  5610 | `	}else{` |
|   130964 |  5611 | `		pKey = 0;` |
|        - |  5612 | `	}` |
|   194314 |  5613 | `	nIdx = pTos->nIdx;` |
|        - |  5614 | `	{` |
|        - |  5615 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5616 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5617 | `		 * the backing variable slot at nIdx. */` |
|   194314 |  5618 | `		ph7_class_instance *pInst = 0;` |
|   194314 |  5619 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5620 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   194298 |  5621 | `		}else if( nIdx != SXU32_HIGH ){` |
|   194282 |  5622 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   194282 |  5623 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5624 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5625 | `			}` |
|    97140 |  5626 | `		}` |
|   194314 |  5627 | `		if( pInst ){` |
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
|   194282 |  5681 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5682 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5683 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5684 | `		 * checking true sharing count, then re-add after separation. */` |
|   194230 |  5685 | `		if( nIdx != SXU32_HIGH ){` |
|   194230 |  5686 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   291344 |  5687 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   194230 |  5688 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5689 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5690 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5691 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5692 | `				 * refcounts if the backing array was already separated. */` |
|   194230 |  5693 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   194230 |  5694 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   194230 |  5695 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   194230 |  5696 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   194230 |  5697 | `					pTos->x.pOther = pMap;` |
|    97116 |  5698 | `				}else{` |
|        - |  5699 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5700 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5701 | `					pMap = pCur;` |
|        - |  5702 | `				}` |
|    97116 |  5703 | `			}else{` |
|      ! 0 |  5704 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5705 | `			}` |
|    97116 |  5706 | `		}else{` |
|      ! 0 |  5707 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5708 | `		}` |
|   194230 |  5709 | `		if( pMap->iRef < 2 ){` |
|        - |  5710 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5711 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5712 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5713 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5714 | `			pMap->iRef = 2;` |
|      ! 0 |  5715 | `		}` |
|    97116 |  5716 | `	}else{` |
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
|   194230 |  5771 | `	VmPopOperand(&pTos,1);` |
|        - |  5772 | `	/* Phase#2: Perform the insertion */` |
|   194230 |  5773 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5774 | `		/* Insertion by reference */` |
|       15 |  5775 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5776 | `	}else{` |
|   194216 |  5777 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5778 | `	}` |
|   194230 |  5779 | `	if( pKey ){` |
|    63276 |  5780 | `		PH7_MemObjRelease(pKey);` |
|    31637 |  5781 | `	}` |
|   194230 |  5782 | `	break;` |
|        - |  5783 | `					   }` |
|        - |  5784 | `/*` |
|        - |  5785 | ` * INCR: P1 * *` |
|        - |  5786 | ` *` |
|        - |  5787 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5788 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5789 | ` * the stack and increment after that.` |
|        - |  5790 | ` */` |
|   167702 |  5791 | `case PH7_OP_INCR:` |
|        - |  5792 | `#ifdef UNTRUST` |
|        - |  5793 | `	if( pTos < pStack ){` |
|        - |  5794 | `		goto Abort;` |
|        - |  5795 | `	}` |
|        - |  5796 | `#endif` |
|   335450 |  5797 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335450 |  5798 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5799 | `			ph7_value *pObj;` |
|   335450 |  5800 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335450 |  5801 | `				if( VmStringWantsPerlIncr(pObj) ){` |
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
|   335402 |  5821 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5822 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5823 | `					}` |
|        - |  5824 | `					/* Force a numeric cast on the variable */` |
|   335402 |  5825 | `					PH7_MemObjToNumeric(pObj);` |
|   335402 |  5826 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5827 | `						pObj->rVal++;` |
|        - |  5828 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5829 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5830 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5831 | `						 * integer-valued real. */` |
|        9 |  5832 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5833 | `					}else{` |
|   335394 |  5834 | `						pObj->x.iVal++;` |
|        - |  5835 | `					}` |
|   335402 |  5836 | `					if( pInstr->iP1 ){` |
|        - |  5837 | `						/* Pre-increment: result is the new value. */` |
|       83 |  5838 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  5839 | `					}` |
|        - |  5840 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5841 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5842 | `				}` |
|   167746 |  5843 | `			}` |
|   167748 |  5844 | `		}else{` |
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
|   167746 |  5863 | `	}` |
|   335450 |  5864 | `	break;` |
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
|    29747 |  5944 | `case PH7_OP_UMINUS:` |
|        - |  5945 | `#ifdef UNTRUST` |
|        - |  5946 | `	if( pTos < pStack ){` |
|        - |  5947 | `		goto Abort;` |
|        - |  5948 | `	}` |
|        - |  5949 | `#endif` |
|        - |  5950 | `	/* Force a numeric (integer,real or both) cast */` |
|    59496 |  5951 | `	PH7_MemObjToNumeric(pTos);` |
|    59496 |  5952 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5953 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5954 | `	}` |
|    59496 |  5955 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59466 |  5956 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29732 |  5957 | `	}` |
|    59496 |  5958 | `	break;` |
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
|    44858 |  5985 | `case PH7_OP_LNOT:` |
|        - |  5986 | `#ifdef UNTRUST` |
|        - |  5987 | `	if( pTos < pStack ){` |
|        - |  5988 | `		goto Abort;` |
|        - |  5989 | `	}` |
|        - |  5990 | `#endif` |
|        - |  5991 | `	/* Force a boolean cast */` |
|    89762 |  5992 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5993 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5994 | `	}` |
|    89762 |  5995 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89762 |  5996 | `	break;` |
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
|    71911 |  6728 | `case PH7_OP_CAT:{` |
|        - |  6729 | `	ph7_value *pNos,*pCur;` |
|   143824 |  6730 | `	if( pInstr->iP1 < 1 ){` |
|   116338 |  6731 | `		pNos = &pTos[-1];` |
|    58170 |  6732 | `	}else{` |
|    27488 |  6733 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6734 | `	}` |
|        - |  6735 | `#ifdef UNTRUST` |
|        - |  6736 | `	if( pNos < pStack ){` |
|        - |  6737 | `		goto Abort;` |
|        - |  6738 | `	}` |
|        - |  6739 | `#endif` |
|        - |  6740 | `	/* Force a string cast */` |
|   143824 |  6741 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6742 | `		PH7_MemObjToString(pNos);` |
|      835 |  6743 | `	}` |
|   143824 |  6744 | `	pCur = &pNos[1];` |
|   290376 |  6745 | `	while( pCur <= pTos ){` |
|   146554 |  6746 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50962 |  6747 | `			PH7_MemObjToString(pCur);` |
|    25480 |  6748 | `		}` |
|        - |  6749 | `		/* Perform the concatenation */` |
|   146554 |  6750 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146510 |  6751 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6752 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6753 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6754 | `				goto Abort;` |
|        - |  6755 | `			}` |
|    73254 |  6756 | `		}` |
|   146554 |  6757 | `		SyBlobRelease(&pCur->sBlob);` |
|   146554 |  6758 | `		pCur++;` |
|        2 |  6759 | `	}` |
|   143824 |  6760 | `	pTos = pNos;` |
|   143824 |  6761 | `	break;` |
|        - |  6762 | `				}` |
|        - |  6763 | `/*  CAT_STORE: * * *` |
|        - |  6764 | ` *` |
|        - |  6765 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6766 | ` * back.` |
|        - |  6767 | ` */` |
|     4142 |  6768 | `case PH7_OP_CAT_STORE:{` |
|     8286 |  6769 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6770 | `	ph7_value *pObj;` |
|        - |  6771 | `	sxu32 nIdx;` |
|        - |  6772 | `#ifdef UNTRUST` |
|        - |  6773 | `	if( pNos < pStack ){` |
|        - |  6774 | `		goto Abort;` |
|        - |  6775 | `	}` |
|        - |  6776 | `#endif` |
|        - |  6777 | `	/* The right operand must be a string to append it */` |
|     8286 |  6778 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6779 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6780 | `	}` |
|     8286 |  6781 | `	nIdx = pTos->nIdx;` |
|        - |  6782 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  6783 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  6784 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  6785 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  6786 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  6787 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  6788 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  6789 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  6790 | `	 * must run before any mutation (left to the slow path).` |
|        - |  6791 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  6792 | `	 * and remains O(n^2) by design. */` |
|     8287 |  6793 | `	if( nIdx != SXU32_HIGH` |
|     8284 |  6794 | `	 && nIdx != pNos->nIdx` |
|     8280 |  6795 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8278 |  6796 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4141 |  6797 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8272 |  6798 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6799 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  6800 | `			PH7_MemObjToString(pObj);` |
|        1 |  6801 | `		}` |
|     8272 |  6802 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8270 |  6803 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6804 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  6805 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  6806 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6807 | `				goto Abort;` |
|        - |  6808 | `			}` |
|     4134 |  6809 | `		}` |
|        - |  6810 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  6811 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  6812 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  6813 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  6814 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  6815 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  6816 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  6817 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  6818 | `		 * the same slot is appended to again later in the statement` |
|        - |  6819 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  6820 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  6821 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8272 |  6822 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  6823 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  6824 | `		}` |
|     8272 |  6825 | `		pNos->nIdx = SXU32_HIGH;` |
|     8272 |  6826 | `		VmPopOperand(&pTos,1);` |
|     8279 |  6827 | `		break;` |
|        - |  6828 | `	}` |
|        - |  6829 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  6830 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6831 | `		/* Force a string cast */` |
|        6 |  6832 | `		PH7_MemObjToString(pTos);` |
|        2 |  6833 | `	}` |
|        - |  6834 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  6835 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  6836 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6837 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  6838 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  6839 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6840 | `			goto Abort;` |
|        - |  6841 | `		}` |
|        7 |  6842 | `	}` |
|        - |  6843 | `	/* Perform the store operation */` |
|       16 |  6844 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6845 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  6846 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  6847 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  6848 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  6849 | `	}` |
|       11 |  6850 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  6851 | `	VmPopOperand(&pTos,1);` |
|       11 |  6852 | `	break;` |
|        - |  6853 | `				}` |
|        - |  6854 | `/* OP_AND: * * *` |
|        - |  6855 | ` *` |
|        - |  6856 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6857 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6858 | ` * stack.` |
|        - |  6859 | ` */` |
|        - |  6860 | `/* OP_OR: * * *` |
|        - |  6861 | ` *` |
|        - |  6862 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6863 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6864 | ` * stack.` |
|        - |  6865 | ` */` |
|   108258 |  6866 | `case PH7_OP_LAND:` |
|        - |  6867 | `case PH7_OP_LOR: {` |
|   216562 |  6868 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6869 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6870 | `#ifdef UNTRUST` |
|        - |  6871 | `	if( pNos < pStack ){` |
|        - |  6872 | `		goto Abort;` |
|        - |  6873 | `	}` |
|        - |  6874 | `#endif` |
|        - |  6875 | `	/* Force a boolean cast */` |
|   216562 |  6876 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6877 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6878 | `	}` |
|   216562 |  6879 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6880 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6881 | `	}` |
|   216562 |  6882 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216562 |  6883 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216562 |  6884 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6885 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99424 |  6886 | `		v1 = and_logic[v1*3+v2];` |
|    49735 |  6887 | `	}else{` |
|        - |  6888 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117140 |  6889 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6890 | `	}` |
|   216562 |  6891 | `	if( v1 == 2 ){` |
|      ! 0 |  6892 | `		v1 = 1;` |
|      ! 0 |  6893 | `	}` |
|   216562 |  6894 | `	VmPopOperand(&pTos,1);` |
|   216562 |  6895 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216562 |  6896 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216562 |  6897 | `	break;` |
|        - |  6898 | `				 }` |
|        - |  6899 | `/*` |
|        - |  6900 | ` * OP_NULLC: * * *` |
|        - |  6901 | ` * Null coalescing operator '??'.` |
|        - |  6902 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6903 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6904 | ` */` |
|        - |  6905 | `/*` |
|        - |  6906 | ` * OP_NULLC: * P2 *` |
|        - |  6907 | ` * Short-circuit null coalescing '??'.` |
|        - |  6908 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6909 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6910 | ` */` |
|       99 |  6911 | `case PH7_OP_NULLC: {` |
|        - |  6912 | `#ifdef UNTRUST` |
|        - |  6913 | `	if( pTos < pStack ){` |
|        - |  6914 | `		goto Abort;` |
|        - |  6915 | `	}` |
|        - |  6916 | `#endif` |
|      200 |  6917 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6918 | `		/* Left is not null — keep it and skip the RHS */` |
|      120 |  6919 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       61 |  6920 | `	}else{` |
|        - |  6921 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       82 |  6922 | `		VmPopOperand(&pTos, 1);` |
|        - |  6923 | `	}` |
|      200 |  6924 | `	break;` |
|        - |  6925 |  |
|        - |  6926 | `/*` |
|        - |  6927 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6928 | ` * Null coalescing assignment short-circuit.` |
|        - |  6929 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6930 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6931 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6932 | ` */` |
|       28 |  6933 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6934 | `#ifdef UNTRUST` |
|        - |  6935 | `	if( pTos < pStack ){` |
|        - |  6936 | `		goto Abort;` |
|        - |  6937 | `	}` |
|        - |  6938 | `#endif` |
|       58 |  6939 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6940 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6941 | `	}` |
|       58 |  6942 | `	break;` |
|        - |  6943 |  |
|        - |  6944 | `/*` |
|        - |  6945 | ` * OP_NULLC_STORE: * * *` |
|        - |  6946 | ` * Null coalescing assignment store.` |
|        - |  6947 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6948 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6949 | ` * expression result.` |
|        - |  6950 | ` */` |
|        - |  6951 | `/*` |
|        - |  6952 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6953 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6954 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6955 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6956 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6957 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6958 | ` */` |
|       51 |  6959 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6960 | `#ifdef UNTRUST` |
|        - |  6961 | `	if( pTos < pStack ){` |
|        - |  6962 | `		goto Abort;` |
|        - |  6963 | `	}` |
|        - |  6964 | `#endif` |
|      104 |  6965 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6966 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6967 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6968 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6969 | `	}` |
|      104 |  6970 | `	break;` |
|        - |  6971 |  |
|       17 |  6972 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6973 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6974 | `	ph7_value *pObj;` |
|        - |  6975 | `	sxu32 nIdx;` |
|        - |  6976 | `#ifdef UNTRUST` |
|        - |  6977 | `	if( pNos < pStack ){` |
|        - |  6978 | `		goto Abort;` |
|        - |  6979 | `	}` |
|        - |  6980 | `#endif` |
|        - |  6981 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6982 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6983 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6984 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6985 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6986 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6987 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6988 | `		ph7_value *apArg[2];` |
|        5 |  6989 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6990 | `		apArg[1] = pTos;` |
|        5 |  6991 | `		if( pSet ){` |
|        5 |  6992 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6993 | `		}` |
|        - |  6994 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6995 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6996 | `		VmPopOperand(&pTos,1);` |
|        - |  6997 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6998 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6999 | `		break;` |
|        - |  7000 | `	}` |
|       32 |  7001 | `	nIdx = pNos->nIdx;` |
|       32 |  7002 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7003 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7004 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7005 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7006 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7007 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7008 | `	}` |
|       32 |  7009 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7010 | `	VmPopOperand(&pTos,1);` |
|       32 |  7011 | `	break;` |
|        - |  7012 |  |
|        - |  7013 | `/*` |
|        - |  7014 | ` * OP_SPREAD: * * *` |
|        - |  7015 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7016 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7017 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7018 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7019 | ` */` |
|        9 |  7020 | `case PH7_OP_SPREAD: {` |
|        - |  7021 | `#ifdef UNTRUST` |
|        - |  7022 | `	if( pTos < pStack ){` |
|        - |  7023 | `		goto Abort;` |
|        - |  7024 | `	}` |
|        - |  7025 | `#endif` |
|       20 |  7026 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  7027 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  7028 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  7029 | `		if( nEntry == 0 ){` |
|        - |  7030 | `			/* Empty array — remove from stack */` |
|        3 |  7031 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7032 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  7033 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7034 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7035 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7036 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7037 | `				VM_STACK_GUARD);` |
|      ! 0 |  7038 | `		}else{` |
|        - |  7039 | `			ph7_hashmap_node *pNode2;` |
|        - |  7040 | `			ph7_value *pElem;` |
|        - |  7041 | `			sxu32 i;` |
|        - |  7042 | `			/* Overwrite TOS with first element */` |
|       18 |  7043 | `			pNode2 = pMap->pFirst;` |
|       18 |  7044 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  7045 | `			PH7_MemObjRelease(pTos);` |
|       18 |  7046 | `			if( pElem ){` |
|       18 |  7047 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7048 | `			}` |
|       18 |  7049 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7050 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7051 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  7052 | `			pNode2 = pNode2->pPrev;` |
|        - |  7053 | `			/* Push remaining elements */` |
|       44 |  7054 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  7055 | `				pTos++;` |
|       28 |  7056 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  7057 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  7058 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  7059 | `				if( pElem ){` |
|       28 |  7060 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7061 | `				}` |
|       28 |  7062 | `				pNode2 = pNode2->pPrev;` |
|       15 |  7063 | `			}` |
|       18 |  7064 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7065 | `		}` |
|        9 |  7066 | `	}` |
|        - |  7067 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  7068 | `	break;` |
|        - |  7069 |  |
|        - |  7070 | `/*` |
|        - |  7071 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7072 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7073 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7074 | ` */` |
|       34 |  7075 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7076 | `#ifdef UNTRUST` |
|        - |  7077 | `	if( pTos < pStack ){` |
|        - |  7078 | `		goto Abort;` |
|        - |  7079 | `	}` |
|        - |  7080 | `#endif` |
|       70 |  7081 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  7082 | `	break;` |
|        - |  7083 |  |
|        - |  7084 | `/* OP_LXOR: * * *` |
|        - |  7085 | ` *` |
|        - |  7086 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7087 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7088 | ` * stack.` |
|        - |  7089 | ` * According to the PHP language reference manual:` |
|        - |  7090 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7091 | ` *  TRUE,but not both.` |
|        - |  7092 | ` */` |
|        5 |  7093 | `case PH7_OP_LXOR:{` |
|       11 |  7094 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7095 | `	sxi32 v = 0;` |
|        - |  7096 | `#ifdef UNTRUST` |
|        - |  7097 | `	if( pNos < pStack ){` |
|        - |  7098 | `		goto Abort;` |
|        - |  7099 | `	}` |
|        - |  7100 | `#endif` |
|        - |  7101 | `	/* Force a boolean cast */` |
|       11 |  7102 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7103 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7104 | `	}` |
|       11 |  7105 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7106 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7107 | `	}` |
|       11 |  7108 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7109 | `		v = 1;` |
|        3 |  7110 | `	}` |
|       11 |  7111 | `	VmPopOperand(&pTos,1);` |
|       11 |  7112 | `	pTos->x.iVal = v;` |
|       11 |  7113 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7114 | `	break;` |
|        - |  7115 | `				 }` |
|        - |  7116 | `/* OP_EQ P1 P2 P3` |
|        - |  7117 | ` *` |
|        - |  7118 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7119 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7120 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7121 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7122 | ` */` |
|        - |  7123 | `/* OP_NEQ P1 P2 P3` |
|        - |  7124 | ` *` |
|        - |  7125 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7126 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7127 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7128 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7129 | ` */` |
|     4584 |  7130 | `case PH7_OP_EQ:` |
|        - |  7131 | `case PH7_OP_NEQ: {` |
|     9170 |  7132 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7133 | `	/* Perform the comparison and act accordingly */` |
|        - |  7134 | `#ifdef UNTRUST` |
|        - |  7135 | `	if( pNos < pStack ){` |
|        - |  7136 | `		goto Abort;` |
|        - |  7137 | `	}` |
|        - |  7138 | `#endif` |
|     9170 |  7139 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9170 |  7140 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7141 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9161 |  7142 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9126 |  7143 | `		rc = rc == 0;` |
|     4564 |  7144 | `	}else{` |
|       28 |  7145 | `		rc = rc != 0;` |
|        - |  7146 | `	}` |
|     9170 |  7147 | `	VmPopOperand(&pTos,1);` |
|     9170 |  7148 | `	if( !pInstr->iP2 ){` |
|        - |  7149 | `		/* Push comparison result without taking the jump */` |
|     9170 |  7150 | `		PH7_MemObjRelease(pTos);` |
|     9170 |  7151 | `		pTos->x.iVal = rc;` |
|        - |  7152 | `		/* Invalidate any prior representation */` |
|     9170 |  7153 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4586 |  7154 | `	}else{` |
|      ! 0 |  7155 | `		if( rc ){` |
|        - |  7156 | `			/* Jump to the desired location */` |
|      ! 0 |  7157 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7158 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7159 | `		}` |
|        - |  7160 | `	}` |
|     9170 |  7161 | `	break;` |
|        - |  7162 | `				 }` |
|        - |  7163 | `/* OP_TEQ P1 P2 *` |
|        - |  7164 | ` *` |
|        - |  7165 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7166 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7167 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7168 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7169 | ` */` |
|   161654 |  7170 | `case PH7_OP_TEQ: {` |
|   323310 |  7171 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7172 | `	/* Perform the comparison and act accordingly */` |
|        - |  7173 | `#ifdef UNTRUST` |
|        - |  7174 | `	if( pNos < pStack ){` |
|        - |  7175 | `		goto Abort;` |
|        - |  7176 | `	}` |
|        - |  7177 | `#endif` |
|   323310 |  7178 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   323310 |  7179 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7180 | `		rc = 0;` |
|        2 |  7181 | `	}else{` |
|   323308 |  7182 | `		rc = rc == 0;` |
|        - |  7183 | `	}` |
|   323310 |  7184 | `	VmPopOperand(&pTos,1);` |
|   323310 |  7185 | `	if( !pInstr->iP2 ){` |
|        - |  7186 | `		/* Push comparison result without taking the jump */` |
|   323310 |  7187 | `		PH7_MemObjRelease(pTos);` |
|   323310 |  7188 | `		pTos->x.iVal = rc;` |
|        - |  7189 | `		/* Invalidate any prior representation */` |
|   323310 |  7190 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   161656 |  7191 | `	}else{` |
|      ! 0 |  7192 | `		if( rc ){` |
|        - |  7193 | `			/* Jump to the desired location */` |
|      ! 0 |  7194 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7195 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7196 | `		}` |
|        - |  7197 | `	}` |
|   323310 |  7198 | `	break;` |
|        - |  7199 | `				 }` |
|        - |  7200 | `/* OP_TNE P1 P2 *` |
|        - |  7201 | ` *` |
|        - |  7202 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7203 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7204 | ` * instruction.` |
|        - |  7205 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7206 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7207 | ` *` |
|        - |  7208 | ` */` |
|   124371 |  7209 | `case PH7_OP_TNE: {` |
|   248744 |  7210 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7211 | `	/* Perform the comparison and act accordingly */` |
|        - |  7212 | `#ifdef UNTRUST` |
|        - |  7213 | `	if( pNos < pStack ){` |
|        - |  7214 | `		goto Abort;` |
|        - |  7215 | `	}` |
|        - |  7216 | `#endif` |
|   248744 |  7217 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   248744 |  7218 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7219 | `		rc = 1;` |
|        2 |  7220 | `	}else{` |
|   248742 |  7221 | `		rc = rc != 0;` |
|        - |  7222 | `	}` |
|   248744 |  7223 | `	VmPopOperand(&pTos,1);` |
|   248744 |  7224 | `	if( !pInstr->iP2 ){` |
|        - |  7225 | `		/* Push comparison result without taking the jump */` |
|   248744 |  7226 | `		PH7_MemObjRelease(pTos);` |
|   248744 |  7227 | `		pTos->x.iVal = rc;` |
|        - |  7228 | `		/* Invalidate any prior representation */` |
|   248744 |  7229 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124373 |  7230 | `	}else{` |
|      ! 0 |  7231 | `		if( rc ){` |
|        - |  7232 | `			/* Jump to the desired location */` |
|      ! 0 |  7233 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7234 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7235 | `		}` |
|        - |  7236 | `	}` |
|   248744 |  7237 | `	break;` |
|        - |  7238 | `				 }` |
|        - |  7239 | `/* OP_LT P1 P2 P3` |
|        - |  7240 | ` *` |
|        - |  7241 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7242 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7243 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7244 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7245 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7246 | ` *` |
|        - |  7247 | ` */` |
|        - |  7248 | `/* OP_LE P1 P2 P3` |
|        - |  7249 | ` *` |
|        - |  7250 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7251 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7252 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7253 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7254 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7255 | ` *` |
|        - |  7256 | ` */` |
|   112427 |  7257 | `case PH7_OP_LT:` |
|        - |  7258 | `case PH7_OP_LE: {` |
|   224900 |  7259 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7260 | `	/* Perform the comparison and act accordingly */` |
|        - |  7261 | `#ifdef UNTRUST` |
|        - |  7262 | `	if( pNos < pStack ){` |
|        - |  7263 | `		goto Abort;` |
|        - |  7264 | `	}` |
|        - |  7265 | `#endif` |
|   224900 |  7266 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224900 |  7267 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7268 | `		rc = 0;` |
|   224896 |  7269 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  7270 | `		rc = rc < 1;` |
|      805 |  7271 | `	}else{` |
|   223286 |  7272 | `		rc = rc < 0;` |
|        - |  7273 | `	}` |
|   224900 |  7274 | `	VmPopOperand(&pTos,1);` |
|   224900 |  7275 | `	if( !pInstr->iP2 ){` |
|        - |  7276 | `		/* Push comparison result without taking the jump */` |
|   224900 |  7277 | `		PH7_MemObjRelease(pTos);` |
|   224900 |  7278 | `		pTos->x.iVal = rc;` |
|        - |  7279 | `		/* Invalidate any prior representation */` |
|   224900 |  7280 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112473 |  7281 | `	}else{` |
|      ! 0 |  7282 | `		if( rc ){` |
|        - |  7283 | `			/* Jump to the desired location */` |
|      ! 0 |  7284 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7285 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7286 | `		}` |
|        - |  7287 | `	}` |
|   224900 |  7288 | `	break;` |
|        - |  7289 | `				}` |
|        - |  7290 | `/* OP_GT P1 P2 P3` |
|        - |  7291 | ` *` |
|        - |  7292 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7293 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7294 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7295 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7296 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7297 | ` *` |
|        - |  7298 | ` */` |
|        - |  7299 | `/* OP_GE P1 P2 P3` |
|        - |  7300 | ` *` |
|        - |  7301 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7302 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7303 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7304 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7305 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7306 | ` *` |
|        - |  7307 | ` */` |
|    55654 |  7308 | `case PH7_OP_GT:` |
|        - |  7309 | `case PH7_OP_GE: {` |
|   111310 |  7310 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7311 | `	/* Perform the comparison and act accordingly */` |
|        - |  7312 | `#ifdef UNTRUST` |
|        - |  7313 | `	if( pNos < pStack ){` |
|        - |  7314 | `		goto Abort;` |
|        - |  7315 | `	}` |
|        - |  7316 | `#endif` |
|   111310 |  7317 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111310 |  7318 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7319 | `		rc = 0;` |
|   111306 |  7320 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110878 |  7321 | `		rc = rc >= 0;` |
|    55440 |  7322 | `	}else{` |
|      426 |  7323 | `		rc = rc > 0;` |
|        - |  7324 | `	}` |
|   111310 |  7325 | `	VmPopOperand(&pTos,1);` |
|   111310 |  7326 | `	if( !pInstr->iP2 ){` |
|        - |  7327 | `		/* Push comparison result without taking the jump */` |
|   111310 |  7328 | `		PH7_MemObjRelease(pTos);` |
|   111310 |  7329 | `		pTos->x.iVal = rc;` |
|        - |  7330 | `		/* Invalidate any prior representation */` |
|   111310 |  7331 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55656 |  7332 | `	}else{` |
|      ! 0 |  7333 | `		if( rc ){` |
|        - |  7334 | `			/* Jump to the desired location */` |
|      ! 0 |  7335 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7336 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7337 | `		}` |
|        - |  7338 | `	}` |
|   111310 |  7339 | `	break;` |
|        - |  7340 | `				}` |
|        - |  7341 | `/* OP_SPACESHIP * * *` |
|        - |  7342 | ` *` |
|        - |  7343 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7344 | ` *   -1 if left < right` |
|        - |  7345 | ` *    0 if left == right` |
|        - |  7346 | ` *    1 if left > right` |
|        - |  7347 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7348 | ` */` |
|       25 |  7349 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7350 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7351 | `#ifdef UNTRUST` |
|        - |  7352 | `	if( pNos < pStack ){` |
|        - |  7353 | `		goto Abort;` |
|        - |  7354 | `	}` |
|        - |  7355 | `#endif` |
|       51 |  7356 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7357 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7358 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7359 | `		rc = 1;` |
|        4 |  7360 | `	}else{` |
|        - |  7361 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7362 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7363 | `	}` |
|       51 |  7364 | `	VmPopOperand(&pTos,1);` |
|       51 |  7365 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7366 | `	pTos->x.iVal = rc;` |
|       51 |  7367 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7368 | `	break;` |
|        - |  7369 | `				}` |
|        - |  7370 | `/* OP_SEQ P1 P2 *` |
|        - |  7371 | ` * Strict string comparison.` |
|        - |  7372 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7373 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7374 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7375 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7376 | ` * use PH7_OP_EQ.` |
|        - |  7377 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7378 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7379 | ` */` |
|        - |  7380 | `/* OP_SNE P1 P2 *` |
|        - |  7381 | ` * Strict string comparison.` |
|        - |  7382 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7383 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7384 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7385 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7386 | ` * use PH7_OP_EQ.` |
|        - |  7387 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7388 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7389 | ` */` |
|       18 |  7390 | `case PH7_OP_SEQ:` |
|        - |  7391 | `case PH7_OP_SNE: {` |
|       38 |  7392 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7393 | `	SyString s1,s2;` |
|        - |  7394 | `	/* Perform the comparison and act accordingly */` |
|        - |  7395 | `#ifdef UNTRUST` |
|        - |  7396 | `	if( pNos < pStack ){` |
|        - |  7397 | `		goto Abort;` |
|        - |  7398 | `	}` |
|        - |  7399 | `#endif` |
|        - |  7400 | `	/* Force a string cast */` |
|       38 |  7401 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7402 | `		PH7_MemObjToString(pTos);` |
|        2 |  7403 | `	}` |
|       38 |  7404 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7405 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7406 | `	}` |
|       38 |  7407 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7408 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7409 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7410 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7411 | `		rc = rc != 0;` |
|      ! 0 |  7412 | `	}else{` |
|       38 |  7413 | `		rc = rc == 0;` |
|        - |  7414 | `	}` |
|       38 |  7415 | `	VmPopOperand(&pTos,1);` |
|       38 |  7416 | `	if( !pInstr->iP2 ){` |
|        - |  7417 | `		/* Push comparison result without taking the jump */` |
|       38 |  7418 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7419 | `		pTos->x.iVal = rc;` |
|        - |  7420 | `		/* Invalidate any prior representation */` |
|       38 |  7421 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7422 | `	}else{` |
|      ! 0 |  7423 | `		if( rc ){` |
|        - |  7424 | `			/* Jump to the desired location */` |
|      ! 0 |  7425 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7426 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7427 | `		}` |
|        - |  7428 | `	}` |
|       38 |  7429 | `	break;` |
|        - |  7430 | `				 }` |
|        - |  7431 | `/*` |
|        - |  7432 | ` * OP_LOAD_REF * * *` |
|        - |  7433 | ` * Push the index of a referenced object on the stack.` |
|        - |  7434 | ` */` |
|       60 |  7435 | `case PH7_OP_LOAD_REF: {` |
|        - |  7436 | `	sxu32 nIdx;` |
|        - |  7437 | `#ifdef UNTRUST` |
|        - |  7438 | `	if( pTos < pStack ){` |
|        - |  7439 | `		goto Abort;` |
|        - |  7440 | `	}` |
|        - |  7441 | `#endif` |
|        - |  7442 | `	/* Extract memory object index */` |
|      121 |  7443 | `	nIdx = pTos->nIdx;` |
|      121 |  7444 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7445 | `		/* Nullify the object */` |
|      101 |  7446 | `		PH7_MemObjRelease(pTos);` |
|        - |  7447 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7448 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7449 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7450 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7451 | `	}` |
|      121 |  7452 | `	break;` |
|        - |  7453 | `					  }` |
|        - |  7454 | `/*` |
|        - |  7455 | ` * OP_STORE_REF * * P3` |
|        - |  7456 | ` * Perform an assignment operation by reference.` |
|        - |  7457 | ` */` |
|       18 |  7458 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7459 | `	 SyString sName = { 0 , 0 };` |
|        - |  7460 | `	 VmFrame *pFrameLocal;` |
|        - |  7461 | `	SyHashEntry *pEntry;` |
|        - |  7462 | `	sxu32 nIdx;` |
|        - |  7463 | `#ifdef UNTRUST` |
|        - |  7464 | `	if( pTos < pStack ){` |
|        - |  7465 | `		goto Abort;` |
|        - |  7466 | `	}` |
|        - |  7467 | `#endif` |
|       38 |  7468 | `	if( pInstr->p3 == 0 ){` |
|        - |  7469 | `		char *zName;` |
|        - |  7470 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7471 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7472 | `			/* Force a string cast */` |
|      ! 0 |  7473 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7474 | `		}` |
|      ! 0 |  7475 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7476 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7477 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7478 | `			if( zName ){` |
|      ! 0 |  7479 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7480 | `			}` |
|      ! 0 |  7481 | `		}` |
|      ! 0 |  7482 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7483 | `		pTos--;` |
|      ! 0 |  7484 | `	}else{` |
|       38 |  7485 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7486 | `	}` |
|       38 |  7487 | `	nIdx = pTos->nIdx;` |
|       38 |  7488 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7489 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7490 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7491 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7492 | `		}else{` |
|        - |  7493 | `			ph7_value *pObj;` |
|        - |  7494 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7495 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7496 | `			if( pObj == 0 ){` |
|      ! 0 |  7497 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7498 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7499 | `				goto Abort;` |
|        - |  7500 | `			}` |
|        - |  7501 | `			/* Perform the store operation */` |
|      ! 0 |  7502 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7503 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7504 | `		}` |
|       38 |  7505 | `	}else if( sName.nByte > 0){` |
|       38 |  7506 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7507 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7508 | `		}else{` |
|       38 |  7509 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  7510 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7511 | `			/* Query the local frame */` |
|       38 |  7512 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  7513 | `			if( pEntry ){` |
|      ! 0 |  7514 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7515 | `			}else{` |
|       38 |  7516 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  7517 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7518 | `					/* Insert in the $GLOBALS array */` |
|       34 |  7519 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  7520 | `				}` |
|       38 |  7521 | `				if( rc == SXRET_OK ){` |
|       38 |  7522 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  7523 | `				}` |
|        - |  7524 | `			}` |
|        - |  7525 | `		}` |
|       18 |  7526 | `	}` |
|       38 |  7527 | `	break;` |
|        - |  7528 | `				 }` |
|        - |  7529 | `/*` |
|        - |  7530 | ` * OP_UPLINK P1 * *` |
|        - |  7531 | ` * Link a variable to the top active VM frame.` |
|        - |  7532 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7533 | ` */` |
|       30 |  7534 | `case PH7_OP_UPLINK: {` |
|       62 |  7535 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7536 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7537 | `		SyString sName;` |
|        - |  7538 | `		/* Perform the link */` |
|      132 |  7539 | `		while( pLink <= pTos ){` |
|       72 |  7540 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7541 | `				/* Force a string cast */` |
|      ! 0 |  7542 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7543 | `			}` |
|       72 |  7544 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7545 | `			if( sName.nByte > 0 ){` |
|       72 |  7546 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7547 | `			}` |
|       72 |  7548 | `			pLink++;` |
|        2 |  7549 | `		}` |
|       30 |  7550 | `	}` |
|       62 |  7551 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7552 | `	break;` |
|        - |  7553 | `					}` |
|        - |  7554 | `/*` |
|        - |  7555 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7556 | ` * Push an exception in the corresponding container so that` |
|        - |  7557 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7558 | ` */` |
|      184 |  7559 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      370 |  7560 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7561 | `	VmFrame *pFrameLocal;` |
|        - |  7562 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      370 |  7563 | `	pException->iFinallyDone = 0;` |
|      370 |  7564 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7565 | `	/* Create the exception frame */` |
|      370 |  7566 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      370 |  7567 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7568 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7569 | `		goto Abort;` |
|        - |  7570 | `	}` |
|        - |  7571 | `	/* Mark the special frame */` |
|      370 |  7572 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      370 |  7573 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7574 | `	/* Point to the frame that trigger the exception */` |
|      370 |  7575 | `	pFrameLocal = pFrameLocal->pParent;` |
|      370 |  7576 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      370 |  7577 | `	pException->pFrame = pFrameLocal;` |
|      370 |  7578 | `	break;` |
|        - |  7579 | `							}` |
|        - |  7580 | `/*` |
|        - |  7581 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7582 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7583 | ` */` |
|      183 |  7584 | `case PH7_OP_POP_EXCEPTION: {` |
|      368 |  7585 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      368 |  7586 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7587 | `		ph7_exception **apException;` |
|        - |  7588 | `		/* Pop the loaded exception */` |
|       32 |  7589 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7590 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7591 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7592 | `		}` |
|       15 |  7593 | `	}` |
|      368 |  7594 | `	pException->pFrame = 0;` |
|        - |  7595 | `	/* Leave the exception frame */` |
|      368 |  7596 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7597 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      368 |  7598 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7599 | `		sxi32 rcFinally;` |
|       20 |  7600 | `		pException->iFinallyDone = 1;` |
|       20 |  7601 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7602 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7603 | `			goto Abort;` |
|        - |  7604 | `		}` |
|        9 |  7605 | `	}` |
|      368 |  7606 | `	break;` |
|        - |  7607 | `							}` |
|        - |  7608 |  |
|        - |  7609 | `/*` |
|        - |  7610 | ` * OP_THROW * P2 *` |
|        - |  7611 | ` * Throw an user exception.` |
|        - |  7612 | ` */` |
|       78 |  7613 | `case PH7_OP_THROW: {` |
|      158 |  7614 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      158 |  7615 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7616 | `#ifdef UNTRUST` |
|        - |  7617 | `	if( pTos < pStack ){` |
|        - |  7618 | `		goto Abort;` |
|        - |  7619 | `	}` |
|        - |  7620 | `#endif` |
|      158 |  7621 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7622 | `	/* Tell the upper layer that an exception was thrown */` |
|      158 |  7623 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      158 |  7624 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      158 |  7625 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7626 | `		ph7_class *pThrowable;` |
|        - |  7627 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      158 |  7628 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      159 |  7629 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7630 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7631 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7632 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7633 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7634 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7635 | `			if( pErrorClass ){` |
|        3 |  7636 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7637 | `			}` |
|        3 |  7638 | `			if( pErrInst ){` |
|        - |  7639 | `				ph7_class_method *pCons;` |
|        3 |  7640 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7641 | `				if( pCons ){` |
|        - |  7642 | `					ph7_value sArg;` |
|        - |  7643 | `					ph7_value *apArg[1];` |
|        - |  7644 | `					SyString sMsgStr;` |
|        - |  7645 | `					static const char zErrMsg[] =` |
|        - |  7646 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7647 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7648 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7649 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7650 | `					apArg[0] = &sArg;` |
|        3 |  7651 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7652 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7653 | `				}` |
|        3 |  7654 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7655 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7656 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7657 | `					goto Abort;` |
|        - |  7658 | `				}` |
|        2 |  7659 | `			}else{` |
|        - |  7660 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7661 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7662 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7663 | `					goto Abort;` |
|        - |  7664 | `				}` |
|        - |  7665 | `			}` |
|        2 |  7666 | `		}else{` |
|        - |  7667 | `			/* Throw the exception */` |
|      156 |  7668 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      156 |  7669 | `			if( rc == SXERR_ABORT ){` |
|        - |  7670 | `				/* Abort processing immediately */` |
|       11 |  7671 | `				goto Abort;` |
|        - |  7672 | `			}` |
|        - |  7673 | `		}` |
|       75 |  7674 | `	}else{` |
|        - |  7675 | `		/* Expecting a class instance */` |
|      ! 0 |  7676 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7677 | `		if( rc == SXERR_ABORT ){` |
|        - |  7678 | `			/* Abort processing immediately */` |
|      ! 0 |  7679 | `			goto Abort;` |
|        - |  7680 | `		}` |
|        - |  7681 | `	}` |
|        - |  7682 | `	/* Pop the top entry */` |
|      148 |  7683 | `	VmPopOperand(&pTos,1);` |
|        - |  7684 | `	/* Perform an unconditional jump */` |
|      148 |  7685 | `	pc = nJump - 1;` |
|      148 |  7686 | `	break;` |
|        - |  7687 | `				   }` |
|        - |  7688 | `/*` |
|        - |  7689 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7690 | ` * Prepare a foreach step.` |
|        - |  7691 | ` */` |
|     6181 |  7692 | `case PH7_OP_FOREACH_INIT: {` |
|    12364 |  7693 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7694 | `	void *pName;` |
|        - |  7695 | `#ifdef UNTRUST` |
|        - |  7696 | `	if( pTos < pStack ){` |
|        - |  7697 | `		goto Abort;` |
|        - |  7698 | `	}` |
|        - |  7699 | `#endif` |
|    12364 |  7700 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7701 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7702 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7703 | `			/* Force a string cast */` |
|      ! 0 |  7704 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7705 | `		}` |
|        - |  7706 | `		/* Duplicate name */` |
|      ! 0 |  7707 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7708 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7709 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7710 | `		}` |
|      ! 0 |  7711 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7712 | `	}` |
|    12364 |  7713 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7714 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7715 | `			/* Force a string cast */` |
|      ! 0 |  7716 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7717 | `		}` |
|        - |  7718 | `		/* Duplicate name */` |
|      ! 0 |  7719 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7720 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7721 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7722 | `		}` |
|      ! 0 |  7723 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7724 | `	}` |
|        - |  7725 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12364 |  7726 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7727 | `		/* Jump out of the loop */` |
|      ! 0 |  7728 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7729 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7730 | `		}` |
|      ! 0 |  7731 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7732 | `	}else{` |
|        - |  7733 | `		ph7_foreach_step *pStep;` |
|    12364 |  7734 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12364 |  7735 | `		if( pStep == 0 ){` |
|      ! 0 |  7736 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7737 | `			/* Jump out of the loop */` |
|      ! 0 |  7738 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7739 | `		}else{` |
|        - |  7740 | `			/* Zero the structure */` |
|    12364 |  7741 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7742 | `			/* Prepare the step */` |
|    12364 |  7743 | `			pStep->iFlags = pInfo->iFlags;` |
|    12364 |  7744 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7745 | `				ph7_hashmap *pMap;` |
|        - |  7746 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7747 | `				 * source array so mutations don't affect other sharers. */` |
|    12330 |  7748 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7749 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7750 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7751 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7752 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7753 | `						 * variable still points at the same hashmap as` |
|        - |  7754 | `						 * the stack value. */` |
|        9 |  7755 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7756 | `							pCur->iRef--;` |
|        9 |  7757 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7758 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7759 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7760 | `						}` |
|        4 |  7761 | `					}` |
|        4 |  7762 | `				}` |
|    12330 |  7763 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7764 | `				/* Reset the internal loop cursor */` |
|    12330 |  7765 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7766 | `				/* Mark the step */` |
|    12330 |  7767 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12330 |  7768 | `				pStep->xIter.pMap = pMap;` |
|    12330 |  7769 | `				pMap->iRef++;` |
|     6166 |  7770 | `			}else{` |
|       36 |  7771 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7772 | `				ph7_class *pIteratorClass;` |
|        - |  7773 | `				/* Check if the object implements Iterator */` |
|       36 |  7774 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7775 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7776 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7777 | `					ph7_class_method *pRewind;` |
|       24 |  7778 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7779 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7780 | `					pThis->iRef++;` |
|       24 |  7781 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7782 | `					if( pRewind ){` |
|       24 |  7783 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7784 | `					}` |
|       13 |  7785 | `				}else{` |
|        - |  7786 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7787 | `					ph7_class *pIterAggClass;` |
|       14 |  7788 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7789 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7790 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7791 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7792 | `						ph7_class_method *pGetIter;` |
|        3 |  7793 | `						int iterAggOk = 0;` |
|        3 |  7794 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7795 | `						if( pGetIter ){` |
|        - |  7796 | `							ph7_value sResult;` |
|        3 |  7797 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7798 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7799 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7800 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7801 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7802 | `									ph7_class_method *pRewind;` |
|        3 |  7803 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7804 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7805 | `									pIterObj->iRef++;` |
|        - |  7806 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7807 | `									pStep->pOwner = pThis;` |
|        3 |  7808 | `									pThis->iRef++;` |
|        3 |  7809 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7810 | `									if( pRewind ){` |
|        3 |  7811 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7812 | `									}` |
|        3 |  7813 | `									iterAggOk = 1;` |
|        1 |  7814 | `								}` |
|        1 |  7815 | `							}` |
|        3 |  7816 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7817 | `						}` |
|        3 |  7818 | `						if( !iterAggOk ){` |
|        - |  7819 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7820 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7821 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7822 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7823 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7824 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7825 | `						}` |
|        2 |  7826 | `					}else{` |
|        - |  7827 | `						/* Plain object iteration via hAttr */` |
|       12 |  7828 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7829 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7830 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7831 | `						pThis->iRef++;` |
|        - |  7832 | `					}` |
|        - |  7833 | `				}` |
|        - |  7834 | `			}` |
|        - |  7835 | `		}` |
|    12364 |  7836 | `		if( pStep ){` |
|    12364 |  7837 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7838 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7839 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7840 | `				/* Jump out of the loop */` |
|      ! 0 |  7841 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7842 | `			}` |
|     6181 |  7843 | `		}` |
|        - |  7844 | `	}` |
|    12364 |  7845 | `	VmPopOperand(&pTos,1);` |
|    12364 |  7846 | `	break;` |
|        - |  7847 | `						  }` |
|        - |  7848 | `/*` |
|        - |  7849 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7850 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7851 | ` */` |
|   101558 |  7852 | `case PH7_OP_FOREACH_STEP: {` |
|   203118 |  7853 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7854 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7855 | `	ph7_value *pValue;` |
|        - |  7856 | `	VmFrame *pFrameLocal;` |
|        - |  7857 | `	/* Peek the last step */` |
|   203118 |  7858 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   203118 |  7859 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   203118 |  7860 | `	pFrameLocal = pVm->pFrame;` |
|   203118 |  7861 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   203118 |  7862 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   202984 |  7863 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7864 | `		ph7_hashmap_node *pNode;` |
|        - |  7865 | `		/* Extract the current node value */` |
|   202984 |  7866 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   202984 |  7867 | `		if( pNode == 0 ){` |
|        - |  7868 | `			/* No more entry to process */` |
|    12328 |  7869 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12328 |  7870 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7871 | `				/* Break the reference with the last element */` |
|        7 |  7872 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7873 | `			}` |
|        - |  7874 | `			/* Automatically reset the loop cursor */` |
|    12328 |  7875 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7876 | `			/* Cleanup the mess left behind */` |
|    12328 |  7877 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12328 |  7878 | `			SySetPop(&pInfo->aStep);` |
|    12328 |  7879 | `			PH7_HashmapUnref(pMap);` |
|     6165 |  7880 | `		}else{` |
|   190658 |  7881 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7882 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7883 | `				if( pKey ){` |
|      528 |  7884 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7885 | `				}` |
|      263 |  7886 | `			}` |
|   190658 |  7887 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7888 | `				SyHashEntry *pEntry;` |
|        - |  7889 | `				/* Pass by reference */` |
|       23 |  7890 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7891 | `				if( pEntry ){` |
|       21 |  7892 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7893 | `				}else{` |
|        4 |  7894 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7895 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7896 | `				}` |
|       12 |  7897 | `			}else{` |
|        - |  7898 | `				/* Make a copy of the entry value */` |
|   190636 |  7899 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   190636 |  7900 | `				if( pValue ){` |
|   190636 |  7901 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95317 |  7902 | `				}` |
|        - |  7903 | `			}` |
|        2 |  7904 | `		}` |
|   101627 |  7905 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7906 | `		/* Iterator-based iteration.` |
|        - |  7907 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7908 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7909 | `		 */` |
|      106 |  7910 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7911 | `		ph7_class_method *pMethod;` |
|        - |  7912 | `		ph7_value sResult;` |
|      106 |  7913 | `		int isValid = 0;` |
|        - |  7914 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7915 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7916 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7917 | `		}else{` |
|       82 |  7918 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7919 | `			if( pMethod ){` |
|       82 |  7920 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7921 | `			}` |
|        - |  7922 | `		}` |
|        - |  7923 | `		/* Call valid() */` |
|      106 |  7924 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7925 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7926 | `		if( pMethod ){` |
|      106 |  7927 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7928 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7929 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7930 | `		}` |
|      106 |  7931 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7932 | `		if( !isValid ){` |
|        - |  7933 | `			/* Iterator exhausted */` |
|       24 |  7934 | `			pc = pInstr->iP2 - 1;` |
|        - |  7935 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7936 | `			if( pStep->pOwner ){` |
|        3 |  7937 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7938 | `			}` |
|       24 |  7939 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7940 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7941 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7942 | `		}else{` |
|        - |  7943 | `			/* Call current() to get value */` |
|       84 |  7944 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7945 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7946 | `			if( pMethod ){` |
|       84 |  7947 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7948 | `			}` |
|       84 |  7949 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7950 | `			if( pValue ){` |
|       84 |  7951 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7952 | `			}` |
|       84 |  7953 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7954 | `			/* Call key() if needed */` |
|       84 |  7955 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7956 | `				ph7_value sKey;` |
|       35 |  7957 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7958 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7959 | `				if( pMethod ){` |
|       35 |  7960 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7961 | `				}` |
|       35 |  7962 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7963 | `				if( pValue ){` |
|       35 |  7964 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7965 | `				}` |
|       35 |  7966 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7967 | `			}` |
|        - |  7968 | `		}` |
|       54 |  7969 | `	}else{` |
|       32 |  7970 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7971 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7972 | `		SyHashEntry *pEntry;` |
|        - |  7973 | `		/* Point to the next attribute */` |
|       36 |  7974 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7975 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7976 | `			/* Check access permission */` |
|       38 |  7977 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7978 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7979 | `					break; /* Access is granted */` |
|        - |  7980 | `			}` |
|        1 |  7981 | `		}` |
|       32 |  7982 | `		if( pEntry == 0 ){` |
|        - |  7983 | `			/* Clean up the mess left behind */` |
|       12 |  7984 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7985 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7986 | `				/* Break the reference with the last element */` |
|        3 |  7987 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7988 | `			}` |
|       12 |  7989 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7990 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7991 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7992 | `		}else{` |
|       22 |  7993 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7994 | `			ph7_value *pAttrValue;` |
|       22 |  7995 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7996 | `				/* Fill with the current attribute name */` |
|       22 |  7997 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7998 | `				if( pKey ){` |
|       22 |  7999 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8000 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8001 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8002 | `				}` |
|       10 |  8003 | `			}` |
|        - |  8004 | `			/* Extract attribute value */` |
|       22 |  8005 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8006 | `			if( pAttrValue ){` |
|       22 |  8007 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8008 | `					/* Pass by reference */` |
|        3 |  8009 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8010 | `					if( pEntry ){` |
|        3 |  8011 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8012 | `					}else{` |
|      ! 0 |  8013 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8014 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8015 | `					}` |
|        2 |  8016 | `				}else{` |
|        - |  8017 | `					/* Make a copy of the attribute value */` |
|       20 |  8018 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8019 | `					if( pValue ){` |
|       20 |  8020 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8021 | `					}` |
|        - |  8022 | `				}` |
|       10 |  8023 | `			}` |
|        - |  8024 | `		}` |
|        - |  8025 | `	}` |
|   203118 |  8026 | `	break;` |
|        - |  8027 | `						  }` |
|        - |  8028 | `/*` |
|        - |  8029 | ` * OP_MEMBER P1 P2` |
|        - |  8030 | ` * Load class attribute/method on the stack.` |
|        - |  8031 | ` */` |
|     4051 |  8032 | `case PH7_OP_MEMBER: {` |
|        - |  8033 | `	ph7_class_instance *pThis;` |
|        - |  8034 | `	ph7_value *pNos;` |
|        - |  8035 | `	SyString sName;` |
|     8104 |  8036 | `	if( !pInstr->iP1 ){` |
|     7864 |  8037 | `		pNos = &pTos[-1];` |
|        - |  8038 | `#ifdef UNTRUST` |
|        - |  8039 | `		if( pNos < pStack ){` |
|        - |  8040 | `			goto Abort;` |
|        - |  8041 | `		}` |
|        - |  8042 | `#endif` |
|     7864 |  8043 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8044 | `			ph7_class *pClass;` |
|        - |  8045 | `			/* Class already instantiated */` |
|     7862 |  8046 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8047 | `			/* Point to the instantiated class */` |
|     7862 |  8048 | `			pClass = pThis->pClass;` |
|        - |  8049 | `			/* Extract attribute name first */` |
|     7862 |  8050 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7862 |  8051 | `			if( pInstr->iP2 ){` |
|        - |  8052 | `				/* Method call */` |
|      786 |  8053 | `				ph7_class_method *pMeth = 0;` |
|      786 |  8054 | `				if( sName.nByte > 0 ){` |
|        - |  8055 | `					/* Extract the target method */` |
|      786 |  8056 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      392 |  8057 | `				}` |
|      786 |  8058 | `				if( pMeth == 0 ){` |
|      ! 0 |  8059 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8060 | `						&pClass->sName,&sName` |
|        - |  8061 | `						);` |
|        - |  8062 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8063 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8064 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8065 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8066 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8067 | `				}else{` |
|        - |  8068 | `					/* Push method name on the stack */` |
|      786 |  8069 | `					PH7_MemObjRelease(pTos);` |
|      786 |  8070 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      786 |  8071 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8072 | `				}` |
|      786 |  8073 | `				pTos->nIdx = SXU32_HIGH;` |
|      394 |  8074 | `			}else{` |
|        - |  8075 | `				/* Attribute access */` |
|     7078 |  8076 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8077 | `				SyHashEntry *pEntry;` |
|        - |  8078 | `				/* Extract the target attribute */` |
|     7078 |  8079 | `				if( sName.nByte > 0 ){` |
|     7078 |  8080 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7078 |  8081 | `					if( pEntry ){` |
|        - |  8082 | `						/* Point to the attribute value */` |
|     7076 |  8083 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3537 |  8084 | `					}` |
|     3538 |  8085 | `				}` |
|     7078 |  8086 | `				if( pObjAttr == 0 ){` |
|        - |  8087 | `					/* No such attribute,load null */` |
|        4 |  8088 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8089 | `						&pClass->sName,&sName);` |
|        - |  8090 | `					/* Call the __get magic method if available */` |
|        3 |  8091 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8092 | `				}` |
|     7078 |  8093 | `				VmPopOperand(&pTos,1);` |
|        - |  8094 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8095 | `				 * This is due to the following case:` |
|        - |  8096 | `				 *     (new TestClass())->foo;` |
|        - |  8097 | `				 */` |
|     7078 |  8098 | `				pThis->iRef++;` |
|     7078 |  8099 | `				PH7_MemObjRelease(pTos);` |
|     7078 |  8100 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7078 |  8101 | `				if( pObjAttr ){` |
|     7076 |  8102 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8103 | `					/* Check attribute access */` |
|     7076 |  8104 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8105 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8106 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8107 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8108 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8109 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7074 |  8110 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3579 |  8111 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  8112 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  8113 | `							int bIsLhs = 0;` |
|       82 |  8114 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  8115 | `								bIsLhs = 1;` |
|       39 |  8116 | `							}` |
|       82 |  8117 | `							if( !bIsLhs ){` |
|        3 |  8118 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8119 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8120 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8121 | `									goto Abort;` |
|        - |  8122 | `								}` |
|        - |  8123 | `								{` |
|        3 |  8124 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8125 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8126 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4051 |  8127 | `										break;` |
|        - |  8128 | `									}` |
|        - |  8129 | `								}` |
|      ! 0 |  8130 | `								goto Exception;` |
|        - |  8131 | `							}` |
|       39 |  8132 | `						}` |
|        - |  8133 | `						/* Load attribute */` |
|     7074 |  8134 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7074 |  8135 | `						if( pValue ){` |
|     7074 |  8136 | `							if( pThis->iRef < 2 ){` |
|        - |  8137 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8138 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8139 | `								 */` |
|        7 |  8140 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8141 | `							}else{` |
|        - |  8142 | `								/* Simple load */` |
|     7068 |  8143 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8144 | `							}` |
|     7074 |  8145 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7072 |  8146 | `								if( pThis->iRef > 1 ){` |
|        - |  8147 | `									/* Load attribute index */` |
|     7066 |  8148 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3532 |  8149 | `								}` |
|     3535 |  8150 | `							}` |
|     3536 |  8151 | `						}` |
|     3538 |  8152 | `					}else{` |
|        - |  8153 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8154 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8155 | `						char zMsg[256];` |
|      ! 0 |  8156 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8157 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8158 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8159 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8160 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8161 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8162 | `						goto Abort;` |
|        - |  8163 | `					}` |
|     3536 |  8164 | `				}` |
|        - |  8165 | `				/* Safely unreference the object */` |
|     7076 |  8166 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8167 | `			}` |
|     3931 |  8168 | `		}else{` |
|        3 |  8169 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8170 | `			VmPopOperand(&pTos,1);` |
|        3 |  8171 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8172 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8173 | `		}` |
|     3932 |  8174 | `	}else{` |
|        - |  8175 | `		/* Static member access using class name */` |
|      242 |  8176 | `		pNos = pTos;` |
|      242 |  8177 | `		pThis = 0;` |
|      242 |  8178 | `		if( !pInstr->p3 ){` |
|      192 |  8179 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  8180 | `			pNos--;` |
|        - |  8181 | `#ifdef UNTRUST` |
|        - |  8182 | `			if( pNos < pStack ){` |
|        - |  8183 | `				goto Abort;` |
|        - |  8184 | `			}` |
|        - |  8185 | `#endif` |
|       97 |  8186 | `		}else{` |
|        - |  8187 | `			/* Attribute name already computed */` |
|       52 |  8188 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8189 | `		}` |
|      242 |  8190 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      242 |  8191 | `			ph7_class *pClass = 0;` |
|      242 |  8192 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8193 | `				/* Class already instantiated */` |
|        5 |  8194 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8195 | `				pClass = pThis->pClass;` |
|        5 |  8196 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8197 | `			}else{` |
|        - |  8198 | `				/* Try to extract the target class */` |
|      238 |  8199 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      238 |  8200 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      238 |  8201 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8202 | `					/* Handle self/static/parent keywords */` |
|      238 |  8203 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  8204 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  8205 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8206 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  8207 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  8208 | `						}` |
|      208 |  8209 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  8210 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      178 |  8211 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  8212 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  8213 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  8214 | `							pClass = pSelf->pBase;` |
|       13 |  8215 | `						}` |
|       15 |  8216 | `					}else{` |
|      126 |  8217 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8218 | `					}` |
|      118 |  8219 | `				}` |
|        - |  8220 | `			}` |
|      242 |  8221 | `			if( pClass == 0 ){` |
|        - |  8222 | `				/* Undefined class */` |
|      ! 0 |  8223 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8224 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8225 | `					);` |
|      ! 0 |  8226 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8227 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8228 | `				}` |
|      ! 0 |  8229 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8230 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8231 | `			}else{` |
|      242 |  8232 | `				if( pInstr->iP2 ){` |
|        - |  8233 | `					/* Method call */` |
|       86 |  8234 | `					ph7_class_method *pMeth = 0;` |
|       86 |  8235 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8236 | `						/* Extract the target method */` |
|       86 |  8237 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8238 | `					}` |
|       86 |  8239 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8240 | `						if( pMeth ){` |
|      ! 0 |  8241 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8242 | `								&pClass->sName,&sName` |
|        - |  8243 | `								);` |
|      ! 0 |  8244 | `						}else{` |
|      ! 0 |  8245 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8246 | `								&pClass->sName,&sName` |
|        - |  8247 | `								);` |
|        - |  8248 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8249 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8250 | `						}` |
|        - |  8251 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8252 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8253 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8254 | `						}` |
|      ! 0 |  8255 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8256 | `					}else{` |
|        - |  8257 | `						/* Push method name on the stack */` |
|       86 |  8258 | `						PH7_MemObjRelease(pTos);` |
|       86 |  8259 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  8260 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8261 | `					}` |
|       86 |  8262 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  8263 | `				}else{` |
|        - |  8264 | `					/* Attribute access */` |
|      158 |  8265 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8266 | `					/* Check for special ::class pseudo-constant */` |
|      204 |  8267 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8268 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8269 | `						/* ::class returns the fully qualified class name */` |
|        - |  8270 | `						/* Pop the attribute name from the stack */` |
|       60 |  8271 | `						if( !pInstr->p3 ){` |
|       60 |  8272 | `							VmPopOperand(&pTos,1);` |
|       29 |  8273 | `						}` |
|       60 |  8274 | `						PH7_MemObjRelease(pTos);` |
|        - |  8275 | `						/* Load the class name */` |
|       60 |  8276 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  8277 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  8278 | `					}else{` |
|        - |  8279 | `						/* Extract the target attribute */` |
|      100 |  8280 | `						if( sName.nByte > 0 ){` |
|      100 |  8281 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8282 | `						}` |
|      100 |  8283 | `						if( pAttr == 0 ){` |
|        - |  8284 | `							/* No such attribute,load null */` |
|      ! 0 |  8285 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8286 | `								&pClass->sName,&sName);` |
|        - |  8287 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8288 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8289 | `						}` |
|        - |  8290 | `						/* Pop the attribute name from the stack */` |
|      100 |  8291 | `						if( !pInstr->p3 ){` |
|       50 |  8292 | `							VmPopOperand(&pTos,1);` |
|       24 |  8293 | `						}` |
|      100 |  8294 | `						PH7_MemObjRelease(pTos);` |
|      100 |  8295 | `						pTos->nIdx = SXU32_HIGH;` |
|      100 |  8296 | `						if( pAttr ){` |
|      100 |  8297 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8298 | `								/* Access to a non static attribute */` |
|      ! 0 |  8299 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8300 | `									&pClass->sName,&pAttr->sName` |
|        - |  8301 | `									);` |
|      ! 0 |  8302 | `							}else{` |
|        - |  8303 | `								ph7_value *pValue;` |
|        - |  8304 | `								/* Check if the access to the attribute is allowed */` |
|      100 |  8305 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8306 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8307 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8308 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       68 |  8309 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       59 |  8310 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8311 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       40 |  8312 | `										if( pS ){` |
|       40 |  8313 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       40 |  8314 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8315 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8316 | `												int bIsLhs = 0;` |
|        8 |  8317 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8318 | `													bIsLhs = 1;` |
|        2 |  8319 | `												}` |
|        8 |  8320 | `												if( !bIsLhs ){` |
|        3 |  8321 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8322 | `													if( pThis ){` |
|      ! 0 |  8323 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8324 | `													}` |
|        3 |  8325 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8326 | `														goto Abort;` |
|        - |  8327 | `													}` |
|        - |  8328 | `													{` |
|        3 |  8329 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8330 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8331 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8332 | `															break;` |
|        - |  8333 | `														}` |
|        - |  8334 | `													}` |
|      ! 0 |  8335 | `													goto Exception;` |
|        - |  8336 | `												}` |
|        2 |  8337 | `											}` |
|       18 |  8338 | `										}` |
|       18 |  8339 | `									}` |
|        - |  8340 | `									/* Load the desired attribute */` |
|       94 |  8341 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       94 |  8342 | `									if( pValue ){` |
|       94 |  8343 | `										PH7_MemObjLoad(pValue,pTos);` |
|       94 |  8344 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8345 | `											/* Load index number */` |
|       50 |  8346 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8347 | `										}` |
|       46 |  8348 | `									}` |
|       48 |  8349 | `								}else{` |
|        - |  8350 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8351 | `									char zMsg[256];` |
|        5 |  8352 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8353 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8354 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8355 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8356 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8357 | `									}else{` |
|      ! 0 |  8358 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8359 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8360 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8361 | `									}` |
|        5 |  8362 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8363 | `									goto Abort;` |
|        - |  8364 | `								}` |
|        - |  8365 | `							}` |
|       46 |  8366 | `						}` |
|        - |  8367 | `					}` |
|        - |  8368 | `				}` |
|      236 |  8369 | `				if( pThis ){` |
|        - |  8370 | `					/* Safely unreference the object */` |
|        5 |  8371 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8372 | `				}` |
|        - |  8373 | `			}` |
|      119 |  8374 | `		}else{` |
|        - |  8375 | `			/* Pop operands */` |
|      ! 0 |  8376 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8377 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8378 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8379 | `			}` |
|      ! 0 |  8380 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8381 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8382 | `		}` |
|        - |  8383 | `	}` |
|     8096 |  8384 | `	break;` |
|        - |  8385 | `					}` |
|        - |  8386 | `/*` |
|        - |  8387 | ` * OP_NEW P1 * * *` |
|        - |  8388 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8389 | ` */` |
|      664 |  8390 | `case PH7_OP_NEW: {` |
|     1330 |  8391 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1330 |  8392 | `	ph7_class *pClass = 0;` |
|        - |  8393 | `	ph7_class_instance *pNew;` |
|     1330 |  8394 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8395 | `		/* Try to extract the desired class */` |
|     1994 |  8396 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1328 |  8397 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      664 |  8398 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8399 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8400 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8401 | `	}` |
|     1330 |  8402 | `	if( pClass == 0 ){` |
|        - |  8403 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8404 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8405 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8406 | `			);` |
|        - |  8407 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8408 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8409 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8410 | `			/* Pop given arguments */` |
|      ! 0 |  8411 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8412 | `		}` |
|      ! 0 |  8413 | `		goto Abort;` |
|      ! 0 |  8414 | `	}else{` |
|        - |  8415 | `		ph7_class_method *pCons;` |
|        - |  8416 | `		/* Create a new class instance */` |
|     1330 |  8417 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1330 |  8418 | `		if( pNew == 0 ){` |
|      ! 0 |  8419 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8420 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8421 | `				&pClass->sName` |
|        - |  8422 | `			);` |
|      ! 0 |  8423 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8424 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8425 | `				/* Pop given arguments */` |
|      ! 0 |  8426 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8427 | `			}` |
|      ! 0 |  8428 | `			break;` |
|        - |  8429 | `		}` |
|        - |  8430 | `		/* Check if a constructor is available */` |
|     1330 |  8431 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1330 |  8432 | `		if( pCons == 0 ){` |
|      934 |  8433 | `			SyString *pName = &pClass->sName;` |
|        - |  8434 | `			/* Check for a constructor with the same base class name */` |
|      934 |  8435 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      466 |  8436 | `		}` |
|     1330 |  8437 | `		if( pCons ){` |
|        - |  8438 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8439 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8440 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8441 | `			 * (including variadic string-key packing). */` |
|      398 |  8442 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8443 | `			sxi32 rcCons;` |
|      398 |  8444 | `			SySetReset(&aArg);` |
|      778 |  8445 | `			while( pArg < pTos ){` |
|      382 |  8446 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      382 |  8447 | `				pArg++;` |
|        2 |  8448 | `			}` |
|      398 |  8449 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8450 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8451 | `				sxu32 n;` |
|      114 |  8452 | `				n = SySetUsed(&aArg);` |
|        - |  8453 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8454 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8455 | `				 * after resolution). */` |
|      222 |  8456 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8457 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8458 | `					if( pFuncArg ){` |
|      110 |  8459 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8460 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8461 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8462 | `						}` |
|       54 |  8463 | `					}` |
|      110 |  8464 | `					n++;` |
|        2 |  8465 | `				}` |
|       56 |  8466 | `			}` |
|      398 |  8467 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8468 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      398 |  8469 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8470 | `				pNew->iRef = 1;` |
|      ! 0 |  8471 | `			}` |
|      398 |  8472 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8473 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8474 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8475 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8476 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8477 | `				sxi32 iResumePc;` |
|        5 |  8478 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8479 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8480 | `					goto Abort;` |
|        - |  8481 | `				}` |
|        5 |  8482 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8483 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8484 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8485 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8486 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8487 | `					}` |
|        5 |  8488 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8489 | `					pc = iResumePc;` |
|        5 |  8490 | `					break;` |
|        - |  8491 | `				}` |
|      ! 0 |  8492 | `				goto Exception;` |
|        - |  8493 | `			}` |
|      196 |  8494 | `		}` |
|     1326 |  8495 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8496 | `			/* Pop given arguments */` |
|      312 |  8497 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      155 |  8498 | `		}` |
|     1326 |  8499 | `		PH7_MemObjRelease(pTos);` |
|     1326 |  8500 | `		pTos->x.pOther = pNew;` |
|     1326 |  8501 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8502 | `	}` |
|     1326 |  8503 | `	break;` |
|        - |  8504 | `				 }` |
|        - |  8505 | `/*` |
|        - |  8506 | ` * OP_CLONE * * *` |
|        - |  8507 | ` * Perfome a clone operation.` |
|        - |  8508 | ` */` |
|       24 |  8509 | `case PH7_OP_CLONE: {` |
|        - |  8510 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8511 | `#ifdef UNTRUST` |
|        - |  8512 | `	if( pTos < pStack ){` |
|        - |  8513 | `		goto Abort;` |
|        - |  8514 | `	}` |
|        - |  8515 | `#endif` |
|        - |  8516 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8517 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8518 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8519 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8520 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8521 | `		break;` |
|        - |  8522 | `	}` |
|        - |  8523 | `	/* Point to the source */` |
|       46 |  8524 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8525 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8526 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8527 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8528 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8529 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8530 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8531 | `		break;` |
|        - |  8532 | `	}` |
|        - |  8533 | `	/* Perform the clone operation */` |
|       46 |  8534 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8535 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8536 | `	if( pClone == 0 ){` |
|      ! 0 |  8537 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8538 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8539 | `	}else{` |
|        - |  8540 | `		/* Load the cloned object */` |
|       46 |  8541 | `		pTos->x.pOther = pClone;` |
|       46 |  8542 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8543 | `	}` |
|       46 |  8544 | `	break;` |
|        - |  8545 | `				   }` |
|        - |  8546 | `/*` |
|        - |  8547 | ` * OP_SWITCH * * P3` |
|        - |  8548 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8549 | ` */` |
|       26 |  8550 | `case PH7_OP_SWITCH: {` |
|       54 |  8551 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8552 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8553 | `	ph7_value sValue,sCaseValue;` |
|        - |  8554 | `	sxu32 n,nEntry;` |
|        - |  8555 | `#ifdef UNTRUST` |
|        - |  8556 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8557 | `		goto Abort;` |
|        - |  8558 | `	}` |
|        - |  8559 | `#endif` |
|        - |  8560 | `	/* Point to the case table  */` |
|       54 |  8561 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8562 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8563 | `	/* Select the appropriate case block to execute */` |
|       54 |  8564 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8565 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8566 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8567 | `		pCase = &aCase[n];` |
|      130 |  8568 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8569 | `		/* Execute the case expression first */` |
|      130 |  8570 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8571 | `		/* Compare the two expression */` |
|      130 |  8572 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8573 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8574 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8575 | `		if( rc == 0 ){` |
|        - |  8576 | `			/* Value match,jump to this block */` |
|       52 |  8577 | `			pc = pCase->nStart - 1;` |
|       52 |  8578 | `			break;` |
|        - |  8579 | `		}` |
|       41 |  8580 | `	}` |
|       54 |  8581 | `	VmPopOperand(&pTos,1);` |
|       54 |  8582 | `	if( n >= nEntry ){` |
|        - |  8583 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8584 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8585 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8586 | `		}else{` |
|        - |  8587 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8588 | `			pc = pSwitch->nOut - 1;` |
|        - |  8589 | `		}` |
|        1 |  8590 | `	}` |
|       54 |  8591 | `	break;` |
|        - |  8592 | `					}` |
|        - |  8593 | `/*` |
|        - |  8594 | ` * OP_MATCH * * P3` |
|        - |  8595 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8596 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8597 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8598 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8599 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8600 | ` */` |
|       54 |  8601 | `case PH7_OP_MATCH: {` |
|      110 |  8602 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8603 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8604 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8605 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8606 | `	int matched = 0;` |
|        - |  8607 | `#ifdef UNTRUST` |
|        - |  8608 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8609 | `		goto Abort;` |
|        - |  8610 | `	}` |
|        - |  8611 | `#endif` |
|      110 |  8612 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8613 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8614 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8615 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8616 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8617 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8618 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8619 | `		pArm = &aArm[i];` |
|      240 |  8620 | `		if( pArm->bDefault ){` |
|       13 |  8621 | `			pDefault = pArm;` |
|       13 |  8622 | `			continue;` |
|        - |  8623 | `		}` |
|      228 |  8624 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8625 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8626 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8627 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8628 | `				continue;` |
|        - |  8629 | `			}` |
|      260 |  8630 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8631 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8632 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8633 | `			if( rc == 0 ){` |
|       93 |  8634 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8635 | `				matched = 1;` |
|       93 |  8636 | `				break;` |
|        - |  8637 | `			}` |
|       85 |  8638 | `		}` |
|      115 |  8639 | `	}` |
|      110 |  8640 | `	if( !matched && pDefault ){` |
|       13 |  8641 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8642 | `		matched = 1;` |
|        6 |  8643 | `	}` |
|      110 |  8644 | `	if( !matched ){` |
|        5 |  8645 | `		const char *zType = "unknown";` |
|        - |  8646 | `		char zMsg[128];` |
|        - |  8647 | `		sxu32 nMsg;` |
|        5 |  8648 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8649 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8650 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8651 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8652 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8653 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8654 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8655 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8656 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8657 | `		default: break;` |
|        - |  8658 | `		}` |
|        7 |  8659 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8660 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8661 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8662 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8663 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8664 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8665 | `		goto Abort;` |
|        - |  8666 | `	}` |
|      105 |  8667 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8668 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8669 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8670 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8671 | `	break;` |
|        - |  8672 | `					}` |
|        - |  8673 | `/*` |
|        - |  8674 | ` * OP_YIELD P1 P2 *` |
|        - |  8675 | ` *  Yield a value from a generator function.` |
|        - |  8676 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8677 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8678 | ` */` |
|       34 |  8679 | `case PH7_OP_YIELD: {` |
|        - |  8680 | `	ph7_generator *pGen;` |
|       70 |  8681 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8682 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8683 | `		goto Abort;` |
|        - |  8684 | `	}` |
|       70 |  8685 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8686 | `	if( pInstr->iP2 ){` |
|        - |  8687 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8688 | `#ifdef UNTRUST` |
|        - |  8689 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8690 | `#endif` |
|        7 |  8691 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8692 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8693 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8694 | `		VmPopOperand(&pTos, 1);` |
|        - |  8695 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8696 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8697 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8698 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8699 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8700 | `			}` |
|        1 |  8701 | `		}` |
|       67 |  8702 | `	}else if( pInstr->iP1 ){` |
|        - |  8703 | `		/* yield $value */` |
|        - |  8704 | `#ifdef UNTRUST` |
|        - |  8705 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8706 | `#endif` |
|       64 |  8707 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8708 | `		VmPopOperand(&pTos, 1);` |
|        - |  8709 | `		/* Auto-increment key */` |
|       64 |  8710 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8711 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8712 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8713 | `	}else{` |
|        - |  8714 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8715 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8716 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8717 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8718 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8719 | `	}` |
|        - |  8720 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8721 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8722 | `	goto Suspend;` |
|        - |  8723 |  |
|        - |  8724 | `/*` |
|        - |  8725 | ` * OP_CALL P1 * *` |
|        - |  8726 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8727 | ` *  function on the stack.` |
|        - |  8728 | ` */` |
|   357792 |  8729 | `case PH7_OP_CALL: {` |
|   715630 |  8730 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8731 | `	ph7_value *pArg;` |
|   715630 |  8732 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   715630 |  8733 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8734 | `	SyHashEntry *pEntry;` |
|        - |  8735 | `	SyString sName;` |
|        - |  8736 | `	/* Extract function name */` |
|   715630 |  8737 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8738 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8739 | `			ph7_value sResult;` |
|        - |  8740 | `			sxi32 rcArr;` |
|        3 |  8741 | `			SySetReset(&aArg);` |
|        3 |  8742 | `			while( pArg < pTos ){` |
|      ! 0 |  8743 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8744 | `				pArg++;` |
|      ! 0 |  8745 | `			}` |
|        3 |  8746 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8747 | `			/* May be a class instance and it's static method */` |
|        3 |  8748 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8749 | `			SySetReset(&aArg);` |
|        - |  8750 | `			/* Pop given arguments */` |
|        3 |  8751 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8752 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8753 | `			}` |
|        3 |  8754 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8755 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8756 | `				goto Abort;` |
|        - |  8757 | `			}` |
|        3 |  8758 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8759 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8760 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8761 | `				sxi32 iResumePc;` |
|        3 |  8762 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8763 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8764 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8765 | `					pc = iResumePc;` |
|        3 |  8766 | `					break;` |
|        - |  8767 | `				}` |
|      ! 0 |  8768 | `				goto Exception;` |
|        - |  8769 | `			}` |
|        - |  8770 | `			/* Copy result */` |
|      ! 0 |  8771 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8772 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8773 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8774 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8775 | `			ph7_value sResult;` |
|        - |  8776 | `			sxi32 rcInv;` |
|       84 |  8777 | `			SySetReset(&aArg);` |
|      200 |  8778 | `			while( pArg < pTos ){` |
|      118 |  8779 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8780 | `				pArg++;` |
|        2 |  8781 | `			}` |
|       84 |  8782 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8783 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8784 | `				(int)SySetUsed(&aArg),` |
|       82 |  8785 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8786 | `				&sResult,` |
|       82 |  8787 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8788 | `			SySetReset(&aArg);` |
|       84 |  8789 | `			if( nCallArgs > 0 ){` |
|       76 |  8790 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8791 | `			}` |
|       84 |  8792 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8793 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8794 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8795 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8796 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8797 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8798 | `				pThis->iRef++;` |
|       13 |  8799 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8800 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8801 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8802 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8803 | `					goto Abort;` |
|        - |  8804 | `				}` |
|        - |  8805 | `				{` |
|       13 |  8806 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8807 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8808 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8809 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8810 | `						break;` |
|        - |  8811 | `					}` |
|        - |  8812 | `				}` |
|      ! 0 |  8813 | `				goto Exception;` |
|        - |  8814 | `			}` |
|       72 |  8815 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8816 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8817 | `				goto Abort;` |
|        - |  8818 | `			}` |
|       72 |  8819 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8820 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8821 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8822 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8823 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8824 | `				sxi32 iResumePc;` |
|        7 |  8825 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8826 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8827 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8828 | `					pc = iResumePc;` |
|        5 |  8829 | `					break;` |
|        - |  8830 | `				}` |
|        3 |  8831 | `				goto Exception;` |
|        - |  8832 | `			}` |
|       66 |  8833 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8834 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8835 | `		}else{` |
|        - |  8836 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8837 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8838 | `			/* Pop given arguments */` |
|      ! 0 |  8839 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8840 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8841 | `			}` |
|        - |  8842 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8843 | `			PH7_MemObjRelease(pTos);` |
|        - |  8844 | `		}` |
|       66 |  8845 | `		break;` |
|        - |  8846 | `	}` |
|   715546 |  8847 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8848 | `	/* Check for a compiled function first.` |
|        - |  8849 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8850 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   715546 |  8851 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8852 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8853 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8854 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8855 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8856 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8857 | `	{` |
|   715546 |  8858 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   715546 |  8859 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8860 | `		const char *zFunc;` |
|        - |  8861 | `		const char *zEnd;` |
|        - |  8862 | `		const char *z;` |
|        - |  8863 | `		SyString sGlobal;` |
|       22 |  8864 | `		zFunc = sName.zString;` |
|       22 |  8865 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8866 | `		z = zEnd;` |
|        - |  8867 | `		/* Find last namespace separator */` |
|      194 |  8868 | `		while( z > zFunc ){` |
|      194 |  8869 | `			if( z[-1] == '\\' ){` |
|       22 |  8870 | `				break;` |
|        - |  8871 | `			}` |
|      174 |  8872 | `			z--;` |
|        2 |  8873 | `		}` |
|       22 |  8874 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8875 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8876 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8877 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8878 | `		}` |
|       10 |  8879 | `	}` |
|        - |  8880 | `	} /* end VmCallArgMap namespace scope */` |
|   715546 |  8881 | `	if( pEntry ){` |
|        - |  8882 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8883 | `		ph7_class_instance *pThis;` |
|        - |  8884 | `		ph7_value *pFrameStack;` |
|        - |  8885 | `		ph7_vm_func *pVmFunc;` |
|        - |  8886 | `		ph7_class *pSelf;` |
|        - |  8887 | `		VmFrame *pFrame;` |
|        - |  8888 | `		ph7_value *pObj;` |
|        - |  8889 | `		VmSlot sArg;` |
|        - |  8890 | `		sxu32 n;` |
|        - |  8891 | `		/* initialize fields */` |
|    18546 |  8892 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18546 |  8893 | `		pThis = 0;` |
|    18546 |  8894 | `		pSelf = 0;` |
|    18546 |  8895 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8896 | `			ph7_class_method *pMeth;` |
|        - |  8897 | `			/* Class method call */` |
|     3352 |  8898 | `			ph7_value *pTarget = &pTos[-1];` |
|     3352 |  8899 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8900 | `				/* Extract the 'this' pointer */` |
|     3352 |  8901 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8902 | `					/* Instance already loaded */` |
|     3262 |  8903 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3262 |  8904 | `					pThis->iRef++;` |
|     3262 |  8905 | `					pSelf = pThis->pClass;` |
|     1630 |  8906 | `				}` |
|     3352 |  8907 | `				if( pSelf == 0 ){` |
|       92 |  8908 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8909 | `						/* "Late Static Binding" class name */` |
|      128 |  8910 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8911 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8912 | `					}` |
|       92 |  8913 | `					if( pSelf == 0 ){` |
|       21 |  8914 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8915 | `					}` |
|       45 |  8916 | `				}` |
|     3352 |  8917 | `				if( pThis == 0  ){` |
|       92 |  8918 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8919 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8920 | `					if( pFrameLocal->pParent ){` |
|        - |  8921 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8922 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8923 | `						if( pThis ){` |
|       21 |  8924 | `							pThis->iRef++;` |
|       10 |  8925 | `						}` |
|       32 |  8926 | `					}` |
|       45 |  8927 | `				}` |
|     3352 |  8928 | `				VmPopOperand(&pTos,1);` |
|     3352 |  8929 | `				PH7_MemObjRelease(pTos);` |
|        - |  8930 | `				/* Synchronize pointers */` |
|     3352 |  8931 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8932 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8933 | `				 * user have already computed the random generated unique class method name` |
|        - |  8934 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8935 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8936 | `				 */` |
|     3352 |  8937 | `				while( pArg < pStack ){` |
|      ! 0 |  8938 | `					pArg++;` |
|      ! 0 |  8939 | `				}` |
|     3352 |  8940 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8941 | `					/* Check if the call is allowed */` |
|     3352 |  8942 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3352 |  8943 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8944 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8945 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8946 | `							char zMsg[256];` |
|      ! 0 |  8947 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8948 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8949 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8950 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8951 | `							/* Pop given arguments */` |
|      ! 0 |  8952 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8953 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8954 | `							}` |
|      ! 0 |  8955 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8956 | `							goto Abort;` |
|        - |  8957 | `						}` |
|        6 |  8958 | `					}` |
|     1675 |  8959 | `				}` |
|     1675 |  8960 | `			}` |
|     1675 |  8961 | `		}` |
|        - |  8962 | `		/* Check The recursion limit */` |
|    18546 |  8963 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8964 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8965 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8966 | `				&pVmFunc->sName);` |
|        - |  8967 | `			/* Pop given arguments */` |
|        3 |  8968 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8969 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8970 | `			}` |
|        - |  8971 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8972 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8973 | `			break;` |
|        - |  8974 | `		}` |
|    18544 |  8975 | `		if( pVmFunc->pNextName ){` |
|        - |  8976 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8977 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8978 | `		}` |
|    18544 |  8979 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8980 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8981 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8982 | `			ph7_generator *pGenerator;` |
|        - |  8983 | `			ph7_class_instance *pGenObj;` |
|        - |  8984 | `			ph7_value *pCtxAttr;` |
|        - |  8985 | `			SyString sAttrName;` |
|        - |  8986 | `			ph7_value **apCallArgs;` |
|        - |  8987 | `			int nGenArgs, iArg;` |
|        - |  8988 | `			/* Collect arguments from the operand stack */` |
|       24 |  8989 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8990 | `			apCallArgs = 0;` |
|       24 |  8991 | `			if( nGenArgs > 0 ){` |
|       14 |  8992 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8993 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8994 | `				if( apCallArgs == 0 ){` |
|        - |  8995 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8996 | `					nGenArgs = 0;` |
|      ! 0 |  8997 | `				}else{` |
|       10 |  8998 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8999 | `					int didReorder = 0;` |
|       10 |  9000 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9001 | `						/* Named-argument reordering for generator */` |
|        5 |  9002 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9003 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9004 | `						sxu32 nNV = nF;` |
|        5 |  9005 | `						sxi32 iVIdx = -1;` |
|        - |  9006 | `						sxi32 *aGSlot;` |
|        - |  9007 | `						sxu8 *aGUsed;` |
|        - |  9008 | `						sxu32 gi;` |
|       13 |  9009 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9010 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9011 | `						}` |
|        7 |  9012 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9013 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9014 | `						if( aGSlot ){` |
|        5 |  9015 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9016 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9017 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9018 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9019 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9020 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9021 | `								goto Abort;` |
|        - |  9022 | `							}` |
|        - |  9023 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9024 | `							 * append overflow (variadic / positional beyond` |
|        - |  9025 | `							 * formals) so downstream sees every argument. */` |
|        - |  9026 | `							{` |
|        5 |  9027 | `								int nOut = 0;` |
|       13 |  9028 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9029 | `									sxu32 gj;` |
|       13 |  9030 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9031 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9032 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9033 | `											break;` |
|        - |  9034 | `										}` |
|        3 |  9035 | `									}` |
|        5 |  9036 | `								}` |
|       13 |  9037 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9038 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9039 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9040 | `									}` |
|        5 |  9041 | `								}` |
|        5 |  9042 | `								nGenArgs = nOut;` |
|        - |  9043 | `							}` |
|        5 |  9044 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9045 | `							didReorder = 1;` |
|        2 |  9046 | `						}` |
|        - |  9047 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9048 | `						 * positional fill below — preserves arg order rather` |
|        - |  9049 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9050 | `					}` |
|       10 |  9051 | `					if( !didReorder ){` |
|       12 |  9052 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9053 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9054 | `						}` |
|        2 |  9055 | `					}` |
|        - |  9056 | `				}` |
|        4 |  9057 | `			}` |
|        - |  9058 | `			/* Create execution context and generator wrapper */` |
|       24 |  9059 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  9060 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9061 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9062 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9063 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9064 | `				break;` |
|        - |  9065 | `			}` |
|       24 |  9066 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  9067 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9068 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9069 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9070 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9071 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9072 | `				break;` |
|        - |  9073 | `			}` |
|        - |  9074 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  9075 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  9076 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  9077 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  9078 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  9079 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  9080 | `			if( apCallArgs ){` |
|       10 |  9081 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9082 | `			}` |
|       24 |  9083 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9084 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9085 | `				if( pThis ){` |
|      ! 0 |  9086 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9087 | `				}` |
|      ! 0 |  9088 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9089 | `					goto Abort;` |
|        - |  9090 | `				}` |
|      ! 0 |  9091 | `				break;` |
|        - |  9092 | `			}` |
|        - |  9093 | `			/* Create Generator class instance */` |
|       24 |  9094 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  9095 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9096 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9097 | `				break;` |
|        - |  9098 | `			}` |
|        - |  9099 | `			/* Store generator in __ctx attribute */` |
|       24 |  9100 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  9101 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  9102 | `			if( pCtxAttr ){` |
|       24 |  9103 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  9104 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  9105 | `			}` |
|        - |  9106 | `			/* Pop args and function name, push Generator object */` |
|       24 |  9107 | `			PH7_MemObjRelease(pTos);` |
|       24 |  9108 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  9109 | `			pTos->x.pOther = pGenObj;` |
|       24 |  9110 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  9111 | `			pGenObj->iRef++;` |
|       24 |  9112 | `			if( pThis ){` |
|      ! 0 |  9113 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9114 | `			}` |
|       24 |  9115 | `			break;` |
|        - |  9116 | `		}` |
|        - |  9117 | `		/* Extract the formal argument set */` |
|    18522 |  9118 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9119 | `		/* Create a new VM frame  */` |
|    18522 |  9120 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18522 |  9121 | `		if( rc != SXRET_OK ){` |
|        - |  9122 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9123 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9124 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9125 | `				&pVmFunc->sName);` |
|        - |  9126 | `			/* Pop given arguments */` |
|      ! 0 |  9127 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9128 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9129 | `			}` |
|        - |  9130 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9131 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9132 | `			break;` |
|        - |  9133 | `		}` |
|    18522 |  9134 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9135 | `			/* Install the '$this' variable */` |
|        - |  9136 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3280 |  9137 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3280 |  9138 | `			if( pObj ){` |
|        - |  9139 | `				/* Reflect the change */` |
|     3280 |  9140 | `				pObj->x.pOther = pThis;` |
|     3280 |  9141 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1639 |  9142 | `			}` |
|     1639 |  9143 | `		}` |
|    18522 |  9144 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9145 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9146 | `			/* Install static variables */` |
|        6 |  9147 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       12 |  9148 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|        6 |  9149 | `				pStatic = &aStatic[n];` |
|        6 |  9150 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9151 | `					/* Initialize the static variables */` |
|        6 |  9152 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        6 |  9153 | `					if( pObj ){` |
|        - |  9154 | `						/* Assume a NULL initialization value */` |
|        6 |  9155 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        6 |  9156 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9157 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        6 |  9158 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|        3 |  9159 | `						}` |
|        6 |  9160 | `						pObj->nIdx = pStatic->nIdx;` |
|        3 |  9161 | `					}else{` |
|      ! 0 |  9162 | `						continue;` |
|        - |  9163 | `					}` |
|        3 |  9164 | `				}` |
|        - |  9165 | `				/* Install in the current frame */` |
|        9 |  9166 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|        6 |  9167 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        3 |  9168 | `			}` |
|        3 |  9169 | `		}` |
|        - |  9170 | `		/* Push arguments in the local frame */` |
|        - |  9171 | `		{` |
|    18522 |  9172 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9173 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9174 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18522 |  9175 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18522 |  9176 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9177 | `			/* ============================================================` |
|        - |  9178 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9179 | `			 *` |
|        - |  9180 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9181 | `			 * or position, then install them in the frame.` |
|        - |  9182 | `			 * ============================================================ */` |
|       96 |  9183 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  9184 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  9185 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9186 | `			sxu32 nNonVariadic;` |
|        - |  9187 | `			sxi32 *aSlot;` |
|        - |  9188 | `			sxu8  *aUsed;` |
|        - |  9189 | `			sxu32 i;` |
|        - |  9190 | `			/* Find variadic parameter index */` |
|      292 |  9191 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  9192 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9193 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9194 | `					break;` |
|        - |  9195 | `				}` |
|      100 |  9196 | `			}` |
|       96 |  9197 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9198 | `			/* Allocate mapping arrays */` |
|      143 |  9199 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9200 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  9201 | `			if( aSlot == 0 ){` |
|      ! 0 |  9202 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9203 | `				goto Abort;` |
|        - |  9204 | `			}` |
|       96 |  9205 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9206 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  9207 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9208 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  9209 | `			if( rc == PH7_ABORT ){` |
|        7 |  9210 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  9211 | `				goto Abort;` |
|        - |  9212 | `			}` |
|        - |  9213 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9214 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9215 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9216 | `				sxi32 iSrc = -1;` |
|      309 |  9217 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9218 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9219 | `						iSrc = (sxi32)i;` |
|      169 |  9220 | `						break;` |
|        - |  9221 | `					}` |
|       62 |  9222 | `				}` |
|      187 |  9223 | `				if( iSrc >= 0 ){` |
|        - |  9224 | `					/* Argument was provided — install with type checking */` |
|      169 |  9225 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9226 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9227 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9228 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9229 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  9230 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9231 | `					}` |
|        - |  9232 | `					/* Type checking: union types */` |
|      169 |  9233 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9234 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9235 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9236 | `							bCallIsStrict);` |
|       13 |  9237 | `						if( rcU != SXRET_OK ){` |
|        - |  9238 | `							const char *zGiven;` |
|      ! 0 |  9239 | `							const char *zExpected = "union";` |
|        - |  9240 | `							char zBuf[128];` |
|        - |  9241 | `							char zTypeBuf[128];` |
|      ! 0 |  9242 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9243 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9244 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9245 | `								zGiven = "null";` |
|      ! 0 |  9246 | `							}else{` |
|      ! 0 |  9247 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9248 | `							}` |
|      ! 0 |  9249 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9250 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9251 | `							}` |
|      ! 0 |  9252 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9253 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9254 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9255 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9256 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9257 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9258 | `							pFrameStack = 0;` |
|      ! 0 |  9259 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9260 | `							goto SkipFuncBody;` |
|        - |  9261 | `						}` |
|      171 |  9262 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9263 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9264 | `						/* Scalar/class type checking */` |
|       17 |  9265 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9266 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  9267 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9268 | `							if( pClass ){` |
|      ! 0 |  9269 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9270 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9271 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9272 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9273 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9274 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9275 | `									}` |
|      ! 0 |  9276 | `								}else{` |
|      ! 0 |  9277 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9278 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9279 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9280 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9281 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9282 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9283 | `									}` |
|        - |  9284 | `								}` |
|      ! 0 |  9285 | `							}` |
|       17 |  9286 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9287 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9288 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9289 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9290 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9291 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9292 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9293 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9294 | `								pFrameStack = 0;` |
|      ! 0 |  9295 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9296 | `								goto SkipFuncBody;` |
|        7 |  9297 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9298 | `								char zTypeBuf[128];` |
|      ! 0 |  9299 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9300 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9301 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9302 | `									ph7_type_name(pVal));` |
|      ! 0 |  9303 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9304 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9305 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9306 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9307 | `								pFrameStack = 0;` |
|      ! 0 |  9308 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9309 | `								goto SkipFuncBody;` |
|        - |  9310 | `							}` |
|        3 |  9311 | `						}` |
|        8 |  9312 | `					}` |
|        - |  9313 | `					/* Install: by reference or by value */` |
|      169 |  9314 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9315 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9316 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9317 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9318 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9319 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9320 | `							}` |
|      ! 0 |  9321 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9322 | `						}else{` |
|        7 |  9323 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9324 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9325 | `							if( pRefEntry == 0 ){` |
|        7 |  9326 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9327 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9328 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9329 | `								sArg.pUserData = 0;` |
|        5 |  9330 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9331 | `							}` |
|        5 |  9332 | `							pObj = 0;` |
|        - |  9333 | `						}` |
|        3 |  9334 | `					}else{` |
|      165 |  9335 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9336 | `					}` |
|      169 |  9337 | `					if( pObj ){` |
|      165 |  9338 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9339 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9340 | `						sArg.pUserData = 0;` |
|      165 |  9341 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9342 | `					}` |
|       85 |  9343 | `				}else{` |
|        - |  9344 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9345 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9346 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9347 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9348 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9349 | `						if( pObj ){` |
|       19 |  9350 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9351 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9352 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9353 | `							sArg.pUserData = 0;` |
|       19 |  9354 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9355 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9356 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9357 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9358 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9359 | `							}` |
|        9 |  9360 | `						}` |
|        9 |  9361 | `					}` |
|        - |  9362 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9363 | `				}` |
|       94 |  9364 | `			}` |
|        - |  9365 | `			/* Handle variadic parameter */` |
|       89 |  9366 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9367 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9368 | `				if( pObj ){` |
|        9 |  9369 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9370 | `					{` |
|        9 |  9371 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9372 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9373 | `							if( aSlot[i] == -1 ){` |
|       16 |  9374 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9375 | `									/* Named variadic entry: insert with string key */` |
|        - |  9376 | `									ph7_value sKey;` |
|       11 |  9377 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9378 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9379 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9380 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9381 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9382 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9383 | `								}else{` |
|        - |  9384 | `									/* Positional variadic entry */` |
|      ! 0 |  9385 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9386 | `								}` |
|        5 |  9387 | `							}` |
|       12 |  9388 | `						}` |
|        - |  9389 | `					}` |
|        9 |  9390 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9391 | `					sArg.pUserData = 0;` |
|        9 |  9392 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9393 | `				}` |
|        5 |  9394 | `			}else{` |
|        - |  9395 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9396 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9397 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9398 | `				 * the positional-only path's behavior. */` |
|       81 |  9399 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9400 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9401 | `					if( aSlot[i] == -2 ){` |
|        - |  9402 | `						char zAnonBuf[32];` |
|        - |  9403 | `						SyString sAnonName;` |
|      ! 0 |  9404 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9405 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9406 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9407 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9408 | `						if( pObj ){` |
|      ! 0 |  9409 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9410 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9411 | `							sArg.pUserData = 0;` |
|      ! 0 |  9412 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9413 | `						}` |
|      ! 0 |  9414 | `						nAnon++;` |
|      ! 0 |  9415 | `					}` |
|       79 |  9416 | `				}` |
|        - |  9417 | `			}` |
|        - |  9418 | `			/* Release all stack arguments */` |
|      267 |  9419 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9420 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9421 | `			}` |
|       89 |  9422 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9423 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9424 | `			n = nFormal;` |
|       45 |  9425 | `		}else{` |
|        - |  9426 | `		/* ============================================================` |
|        - |  9427 | `		 * Positional-only matching path (original)` |
|        - |  9428 | `		 * ============================================================ */` |
|    18428 |  9429 | `		n = 0;` |
|    49060 |  9430 | `		while( pArg < pTos ){` |
|    30706 |  9431 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9432 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9433 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9434 | `				if( pObj ){` |
|        - |  9435 | `					/* Initialize as empty array */` |
|       40 |  9436 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9437 | `					{` |
|       40 |  9438 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9439 | `						while( pArg < pTos ){` |
|        - |  9440 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9441 | `							 *` |
|        - |  9442 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9443 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9444 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9445 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9446 | `							 * fixing both wants a separate counter for elements` |
|        - |  9447 | `							 * already packed into the variadic array. */` |
|      114 |  9448 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9449 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9450 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9451 | `									bCallIsStrict);` |
|       16 |  9452 | `								if( rcU != SXRET_OK ){` |
|        - |  9453 | `									const char *zGiven;` |
|        3 |  9454 | `									const char *zExpected = "union";` |
|        - |  9455 | `									char zBuf[128];` |
|        - |  9456 | `									char zTypeBuf[128];` |
|        3 |  9457 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9458 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9459 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9460 | `										zGiven = "null";` |
|      ! 0 |  9461 | `									}else{` |
|        3 |  9462 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9463 | `									}` |
|        3 |  9464 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9465 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9466 | `									}` |
|        4 |  9467 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9468 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9469 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9470 | `										goto Abort;` |
|        - |  9471 | `									}` |
|        3 |  9472 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9473 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9474 | `									pFrameStack = 0;` |
|        3 |  9475 | `									rc = PH7_EXCEPTION;` |
|        3 |  9476 | `									goto SkipFuncBody;` |
|        - |  9477 | `								}` |
|       14 |  9478 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9479 | `								pArg++;` |
|       14 |  9480 | `								continue;` |
|        - |  9481 | `							}` |
|        - |  9482 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9483 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9484 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9485 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9486 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9487 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9488 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9489 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9490 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9491 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9492 | `										goto Abort;` |
|        - |  9493 | `									}` |
|        - |  9494 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9495 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9496 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9497 | `									pFrameStack = 0;` |
|      ! 0 |  9498 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9499 | `									goto SkipFuncBody;` |
|       13 |  9500 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9501 | `									char zTypeBuf[128];` |
|      ! 0 |  9502 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9503 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9504 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9505 | `										ph7_type_name(pArg));` |
|      ! 0 |  9506 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9507 | `										goto Abort;` |
|        - |  9508 | `									}` |
|      ! 0 |  9509 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9510 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9511 | `									pFrameStack = 0;` |
|      ! 0 |  9512 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9513 | `									goto SkipFuncBody;` |
|        - |  9514 | `								}` |
|        6 |  9515 | `							}` |
|      100 |  9516 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9517 | `							pArg++;` |
|        2 |  9518 | `						}` |
|        - |  9519 | `					}` |
|       38 |  9520 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9521 | `					sArg.pUserData = 0;` |
|       38 |  9522 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9523 | `				}` |
|       38 |  9524 | `				break; /* All remaining args consumed */` |
|        - |  9525 | `			}` |
|    30668 |  9526 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30450 |  9527 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9528 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9529 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9530 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9531 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9532 | `						goto Abort;` |
|        - |  9533 | `					}` |
|      ! 0 |  9534 | `				}` |
|        - |  9535 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30452 |  9536 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9537 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9538 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9539 | `						bCallIsStrict);` |
|       60 |  9540 | `					if( rcU != SXRET_OK ){` |
|        - |  9541 | `						const char *zGiven;` |
|       19 |  9542 | `						const char *zExpected = "union";` |
|        - |  9543 | `						char zBuf[128];` |
|        - |  9544 | `						char zTypeBuf[128];` |
|       19 |  9545 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9546 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9547 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9548 | `							zGiven = "null";` |
|        5 |  9549 | `						}else{` |
|        5 |  9550 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9551 | `						}` |
|       19 |  9552 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9553 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9554 | `						}` |
|       28 |  9555 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9556 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9557 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9558 | `							goto Abort;` |
|        - |  9559 | `						}` |
|       19 |  9560 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9561 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9562 | `						pFrameStack = 0;` |
|       19 |  9563 | `						rc = PH7_EXCEPTION;` |
|       19 |  9564 | `						goto SkipFuncBody;` |
|        - |  9565 | `					}` |
|       21 |  9566 | `				}else` |
|        - |  9567 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9568 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30418 |  9569 | `				if( aFormalArg[n].nType > 0` |
|    15913 |  9570 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9571 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9572 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9573 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9574 | `						ph7_class *pClass;` |
|        - |  9575 | `						/* Try to extract the desired class */` |
|       26 |  9576 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9577 | `						if( pClass ){` |
|       22 |  9578 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9579 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9580 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9581 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9582 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9583 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9584 | `								}` |
|      ! 0 |  9585 | `							}else{` |
|        - |  9586 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9587 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9588 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9589 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9590 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9591 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9592 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9593 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9594 | `								}` |
|        - |  9595 | `							}` |
|       12 |  9596 | `						}` |
|     1394 |  9597 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9598 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9599 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9600 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9601 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9602 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9603 | `								goto Abort;` |
|        - |  9604 | `							}` |
|        - |  9605 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9606 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9607 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9608 | `							pFrameStack = 0;` |
|       11 |  9609 | `							rc = PH7_EXCEPTION;` |
|       11 |  9610 | `							goto SkipFuncBody;` |
|       16 |  9611 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9612 | `							char zTypeBuf[128];` |
|       11 |  9613 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9614 | `								&aFormalArg[n].sName,` |
|        6 |  9615 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9616 | `								ph7_type_name(pArg));` |
|        8 |  9617 | `							if( rc == PH7_ABORT ){` |
|        5 |  9618 | `								goto Abort;` |
|        - |  9619 | `							}` |
|        3 |  9620 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9621 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9622 | `							pFrameStack = 0;` |
|        3 |  9623 | `							rc = PH7_EXCEPTION;` |
|        3 |  9624 | `							goto SkipFuncBody;` |
|        - |  9625 | `						}` |
|        4 |  9626 | `					}` |
|      694 |  9627 | `				}` |
|    30418 |  9628 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9629 | `					/* Pass by reference */` |
|       58 |  9630 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9631 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9632 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9633 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9634 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9635 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9636 | `						}` |
|        - |  9637 | `						/* Switch to pass by value */` |
|      ! 0 |  9638 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9639 | `					}else{` |
|        - |  9640 | `						SyHashEntry *pRefEntry;` |
|        - |  9641 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9642 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9643 | `						if( pRefEntry == 0 ){` |
|       86 |  9644 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9645 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9646 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9647 | `							sArg.pUserData = 0;` |
|       58 |  9648 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9649 | `						}` |
|       58 |  9650 | `						pObj = 0;` |
|        - |  9651 | `					}` |
|       30 |  9652 | `				}else{` |
|        - |  9653 | `					/* Pass by value,make a copy of the given argument */` |
|    30362 |  9654 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9655 | `				}` |
|    15210 |  9656 | `			}else{` |
|        - |  9657 | `				char zName[32];` |
|        - |  9658 | `				SyString sArgName;` |
|        - |  9659 | `				/* Set a dummy name */` |
|      218 |  9660 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9661 | `				sArgName.zString = zName;` |
|        - |  9662 | `				/* Annonymous argument */` |
|      218 |  9663 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9664 | `			}` |
|    30634 |  9665 | `			if( pObj ){` |
|    30578 |  9666 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9667 | `				/* Insert argument index  */` |
|    30578 |  9668 | `				sArg.nIdx = pObj->nIdx;` |
|    30578 |  9669 | `				sArg.pUserData = 0;` |
|    30578 |  9670 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15288 |  9671 | `			}` |
|    30634 |  9672 | `			PH7_MemObjRelease(pArg);` |
|    30634 |  9673 | `			pArg++;` |
|    30634 |  9674 | `			++n;` |
|        2 |  9675 | `		}` |
|        - |  9676 | `		} /* end named vs positional branch */` |
|        - |  9677 | `		/* Set up closure environment */` |
|    18480 |  9678 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9679 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9680 | `			ph7_value *pValue;` |
|        - |  9681 | `			sxu32 iEnv;` |
|      184 |  9682 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 |  9683 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 |  9684 | `				pEnv = &aEnv[iEnv];` |
|      252 |  9685 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9686 | `					/* Do not install null value */` |
|      178 |  9687 | `					continue;` |
|        - |  9688 | `				}` |
|       76 |  9689 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9690 | `				if( pValue == 0 ){` |
|      ! 0 |  9691 | `					continue;` |
|        - |  9692 | `				}` |
|        - |  9693 | `				/* Invalidate any prior representation */` |
|       76 |  9694 | `				PH7_MemObjRelease(pValue);` |
|        - |  9695 | `				/* Duplicate bound variable value */` |
|       76 |  9696 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9697 | `			}` |
|       91 |  9698 | `		}` |
|        - |  9699 | `		/* Process default values for remaining formal parameters */` |
|    21372 |  9700 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2940 |  9701 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9702 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9703 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9704 | `				if( pObj ){` |
|       48 |  9705 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9706 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9707 | `					sArg.pUserData = 0;` |
|       48 |  9708 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9709 | `				}` |
|       48 |  9710 | `				n++;` |
|       48 |  9711 | `				break; /* Variadic is always last */` |
|        - |  9712 | `			}` |
|     2894 |  9713 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2888 |  9714 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2888 |  9715 | `				if( pObj ){` |
|        - |  9716 | `					/* Evaluate the default value and extract it's result */` |
|     2888 |  9717 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2888 |  9718 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9719 | `						goto Abort;` |
|        - |  9720 | `					}` |
|        - |  9721 | `					/* Insert argument index */` |
|     2888 |  9722 | `					sArg.nIdx = pObj->nIdx;` |
|     2888 |  9723 | `					sArg.pUserData = 0;` |
|     2888 |  9724 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9725 | `					/* Make sure the default argument is of the correct type */` |
|     2886 |  9726 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1866 |  9727 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9728 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9729 | `						/* Cast to the desired type */` |
|        3 |  9730 | `						xCast(pObj);` |
|        1 |  9731 | `					}` |
|     1443 |  9732 | `				}` |
|     1443 |  9733 | `			}` |
|     2894 |  9734 | `			++n;` |
|        2 |  9735 | `		}` |
|        - |  9736 | `		} /* end VmCallArgMap scope */` |
|        - |  9737 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9738 | `		 * does not return anything.` |
|        - |  9739 | `		 */` |
|    18480 |  9740 | `		PH7_MemObjRelease(pTos);` |
|    18480 |  9741 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9742 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18480 |  9743 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18480 |  9744 | `		if( pFrameStack == 0 ){` |
|        - |  9745 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9746 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9747 | `				&pVmFunc->sName);` |
|      ! 0 |  9748 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9749 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9750 | `			}` |
|      ! 0 |  9751 | `			break;` |
|        - |  9752 | `		}` |
|     9239 |  9753 | `SkipFuncBody:` |
|    18512 |  9754 | `		if( pSelf ){` |
|        - |  9755 | `			/* Push class name */` |
|     3350 |  9756 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1674 |  9757 | `		}` |
|        - |  9758 | `		/* Increment nesting level */` |
|    18512 |  9759 | `		pVm->nRecursionDepth++;` |
|    18512 |  9760 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9761 | `			/* Execute function body */` |
|    27719 |  9762 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18478 |  9763 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9239 |  9764 | `		}` |
|        - |  9765 | `		/* Decrement nesting level */` |
|    18512 |  9766 | `		pVm->nRecursionDepth--;` |
|    18512 |  9767 | `		if( pSelf ){` |
|        - |  9768 | `			/* Pop class name */` |
|     3350 |  9769 | `			(void)SySetPop(&pVm->aSelf);` |
|     1674 |  9770 | `		}` |
|        - |  9771 | `		/* Cleanup the mess left behind */` |
|    18512 |  9772 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9773 | `			/* Return by reference,reflect that */` |
|        9 |  9774 | `			if( n != SXU32_HIGH ){` |
|        9 |  9775 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9776 | `				sxu32 i;` |
|        - |  9777 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9778 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9779 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9780 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9781 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9782 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9783 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9784 | `								&pVmFunc->sName);` |
|      ! 0 |  9785 | `						}` |
|      ! 0 |  9786 | `						n = SXU32_HIGH;` |
|      ! 0 |  9787 | `						break;` |
|        - |  9788 | `					}` |
|        3 |  9789 | `				}` |
|        5 |  9790 | `			}else{` |
|      ! 0 |  9791 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9792 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9793 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9794 | `						&pVmFunc->sName);` |
|      ! 0 |  9795 | `				}` |
|        - |  9796 | `			}` |
|        9 |  9797 | `			pTos->nIdx = n;` |
|        4 |  9798 | `		}` |
|        - |  9799 | `		/* Cleanup the mess left behind */` |
|    18512 |  9800 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9801 | `			/* An exception was throw in this frame */` |
|      100 |  9802 | `			pFrame = pFrame->pParent;` |
|      100 |  9803 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9804 | `				/* Pop the resutlt */` |
|       62 |  9805 | `				VmPopOperand(&pTos,1);` |
|        - |  9806 | `				/* Jump to this destination */` |
|       62 |  9807 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9808 | `				rc = PH7_OK;` |
|       32 |  9809 | `			}else{` |
|       39 |  9810 | `				if( pFrame->pParent ){` |
|       39 |  9811 | `					rc = PH7_EXCEPTION;` |
|       20 |  9812 | `				}else{` |
|        - |  9813 | `					/* Continue normal execution */` |
|      ! 0 |  9814 | `					rc = PH7_OK;` |
|        - |  9815 | `				}` |
|        - |  9816 | `			}` |
|       49 |  9817 | `		}` |
|        - |  9818 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18512 |  9819 | `		if( pFrameStack ){` |
|    18480 |  9820 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9239 |  9821 | `		}` |
|        - |  9822 | `		/* Leave the frame */` |
|    18512 |  9823 | `		VmLeaveFrame(&(*pVm));` |
|    18512 |  9824 | `		if( rc == PH7_ABORT ){` |
|        - |  9825 | `			/* Abort processing immeditaley */` |
|       17 |  9826 | `			goto Abort;` |
|    18496 |  9827 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9828 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9829 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9830 | `			 * overwriting the state saved by the inner level.` |
|        - |  9831 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9832 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9833 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9834 | `			goto Suspend;` |
|    18458 |  9835 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9836 | `			goto Exception;` |
|        - |  9837 | `		}` |
|     9211 |  9838 | `	}else{` |
|        - |  9839 | `		ph7_user_func *pFunc;` |
|        - |  9840 | `		ph7_context sCtx;` |
|        - |  9841 | `		ph7_value sRet;` |
|        - |  9842 | `		/* Look for an installed foreign function.` |
|        - |  9843 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9844 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9845 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9846 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   697002 |  9847 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9848 | `		{` |
|   697002 |  9849 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   697002 |  9850 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9851 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9852 | `			const char *zShort = sName.zString;` |
|        - |  9853 | `			sxu32 i;` |
|      334 |  9854 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9855 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9856 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9857 | `				}` |
|      158 |  9858 | `			}` |
|       22 |  9859 | `			if( zShort != sName.zString ){` |
|       22 |  9860 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9861 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9862 | `			}` |
|       10 |  9863 | `		}` |
|        - |  9864 | `		} /* end VmCallArgMap namespace scope */` |
|   697002 |  9865 | `		if( pEntry == 0 ){` |
|        - |  9866 | `			/* Call to undefined function */` |
|        5 |  9867 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9868 | `			/* Pop given arguments */` |
|        5 |  9869 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9870 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9871 | `			}` |
|        - |  9872 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9873 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9874 | `			break;` |
|        - |  9875 | `		}` |
|   696998 |  9876 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9877 | `		/* Start collecting function arguments */` |
|   696998 |  9878 | `		SySetReset(&aArg);` |
|  1879326 |  9879 | `		while( pArg < pTos ){` |
|  1182330 |  9880 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1182330 |  9881 | `			pArg++;` |
|        2 |  9882 | `		}` |
|        - |  9883 | `		/* Assume a null return value */` |
|   696998 |  9884 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9885 | `		/* Init the call context */` |
|   696998 |  9886 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9887 | `		/* Call the foreign function */` |
|   696998 |  9888 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9889 | `		/* Release the call context */` |
|   696998 |  9890 | `		VmReleaseCallContext(&sCtx);` |
|   696998 |  9891 | `		if( rc == PH7_ABORT ){` |
|        - |  9892 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - |  9893 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - |  9894 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      497 |  9895 | `			PH7_MemObjRelease(&sRet);` |
|      497 |  9896 | `			goto Abort;` |
|   696502 |  9897 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9898 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9899 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9900 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9901 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9902 | `				goto Exception;` |
|        - |  9903 | `			}` |
|        - |  9904 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9905 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9906 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9907 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9908 | `			}` |
|        - |  9909 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9910 | `			VmPopOperand(&pTos,1);` |
|        - |  9911 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9912 | `			pFrm = pVm->pFrame;` |
|      108 |  9913 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9914 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9915 | `			}` |
|      108 |  9916 | `			break;` |
|        - |  9917 | `		}` |
|   696392 |  9918 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9919 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9920 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9921 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9922 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9923 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9924 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9925 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9926 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9927 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9928 | `			}` |
|        - |  9929 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9930 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9931 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9932 | `			goto Suspend;` |
|        - |  9933 | `		}` |
|   696354 |  9934 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9935 | `			/* Pop function name and arguments */` |
|   674358 |  9936 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   337200 |  9937 | `		}` |
|        - |  9938 | `		/* Save foreign function return value */` |
|   696354 |  9939 | `		PH7_MemObjStore(&sRet,pTos);` |
|   696354 |  9940 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9941 | `	}` |
|   714772 |  9942 | `	break;` |
|        - |  9943 | `				  }` |
|        - |  9944 | `/*` |
|        - |  9945 | ` * OP_CONSUME: P1 * *` |
|        - |  9946 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9947 | ` */` |
|    15999 |  9948 | `case PH7_OP_CONSUME: {` |
|    32000 |  9949 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32000 |  9950 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9951 |  |
|    32000 |  9952 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32000 |  9953 | `	pCur = pOut;` |
|        - |  9954 | `	/* Start the consume process  */` |
|    64040 |  9955 | `	while( pOut <= pTos ){` |
|        - |  9956 | `		/* Force a string cast */` |
|    32042 |  9957 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1052 |  9958 | `			PH7_MemObjToString(pOut);` |
|      525 |  9959 | `		}` |
|    32042 |  9960 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9961 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9962 | `			/* Invoke the output consumer callback */` |
|    19624 |  9963 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19624 |  9964 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19624 |  9965 | `			SyBlobRelease(&pOut->sBlob);` |
|    19624 |  9966 | `			if( rc == SXERR_ABORT ){` |
|        - |  9967 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9968 | `				goto Abort;` |
|        - |  9969 | `			}` |
|     9811 |  9970 | `		}` |
|    32042 |  9971 | `		pOut++;` |
|        2 |  9972 | `	}` |
|    32000 |  9973 | `	pTos = &pCur[-1];` |
|    31998 |  9974 | `	break;` |
|        - |  9975 | `					 }` |
|        - |  9976 |  |
|        - |  9977 | `		} /* Switch() */` |
| 11769082 |  9978 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9979 | `	} /* For(;;) */` |
|    22188 |  9980 | `Done:` |
|    44378 |  9981 | `	SySetRelease(&aArg);` |
|    44378 |  9982 | `	return SXRET_OK;` |
|       72 |  9983 | `Suspend:` |
|      146 |  9984 | `	SySetRelease(&aArg);` |
|      146 |  9985 | `	return PH7_SUSPEND;` |
|      280 |  9986 | `Abort:` |
|      561 |  9987 | `	SySetRelease(&aArg);` |
|     1875 |  9988 | `	while( pTos >= pStack ){` |
|     1315 |  9989 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9990 | `		pTos--;` |
|        1 |  9991 | `	}` |
|      561 |  9992 | `	return PH7_ABORT;` |
|       29 |  9993 | `Exception:` |
|       60 |  9994 | `	SySetRelease(&aArg);` |
|      112 |  9995 | `	while( pTos >= pStack ){` |
|       54 |  9996 | `		PH7_MemObjRelease(pTos);` |
|       54 |  9997 | `		pTos--;` |
|        2 |  9998 | `	}` |
|       60 |  9999 | `	return PH7_EXCEPTION;` |
|    22571 | 10000 |  |
|        - | 10001 | `/*` |
|        - | 10002 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10003 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10004 | ` * See block-comment on that function for additional information.` |
|        - | 10005 | ` */` |
|    20606 | 10006 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 | 10007 |  |
|        - | 10008 | `	ph7_value *pStack;` |
|        - | 10009 | `	sxi32 rc;` |
|        - | 10010 | `	/* Allocate a new operand stack */` |
|    20608 | 10011 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20608 | 10012 | `	if( pStack == 0 ){` |
|      ! 0 | 10013 | `		return SXERR_MEM;` |
|        - | 10014 | `	}` |
|        - | 10015 | `	/* Execute the program */` |
|    20608 | 10016 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - | 10017 | `	/* Free the operand stack */` |
|    20608 | 10018 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10019 | `	/* Execution result */` |
|    20608 | 10020 | `	return rc;` |
|    10305 | 10021 |  |
|        - | 10022 | `/*` |
|        - | 10023 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10024 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10025 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10026 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10027 | ` * execution ends.` |
|        - | 10028 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10029 | ` * additional information.` |
|        - | 10030 | ` */` |
|     2832 | 10031 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 | 10032 |  |
|        - | 10033 | `	VmShutdownCB *pEntry;` |
|        - | 10034 | `	ph7_value *apArg[10];` |
|        - | 10035 | `	sxu32 n,nEntry;` |
|        - | 10036 | `	int i;` |
|        - | 10037 | `	/* Point to the stack of registered callbacks */` |
|     2834 | 10038 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31154 | 10039 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28322 | 10040 | `		apArg[i] = 0;` |
|    14162 | 10041 | `	}` |
|        - | 10042 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10043 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10044 | `	 * callbacks, mirroring PHP.` |
|        - | 10045 | `	 */` |
|     2834 | 10046 | `	pVm->bHaltRequested = 0;` |
|     2844 | 10047 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 | 10048 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 | 10049 | `		if( pEntry ){` |
|        - | 10050 | `			/* Prepare callback arguments if any */` |
|       12 | 10051 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10052 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10053 | `					break;` |
|        - | 10054 | `				}` |
|      ! 0 | 10055 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10056 | `			}` |
|        - | 10057 | `			/* Invoke the callback */` |
|       12 | 10058 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10059 | `			/*` |
|        - | 10060 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10061 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10062 | `			 */` |
|       12 | 10063 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 | 10064 | `			if( pEntry ){` |
|       12 | 10065 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 | 10066 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10067 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10068 | `				}` |
|        5 | 10069 | `			}` |
|       12 | 10070 | `			if( pVm->bHaltRequested ){` |
|        - | 10071 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10072 | `				break;` |
|        - | 10073 | `			}` |
|        5 | 10074 | `		}` |
|        7 | 10075 | `	}` |
|     2834 | 10076 | `	SySetReset(&pVm->aShutdown);` |
|     2834 | 10077 |  |
|        - | 10078 | `/*` |
|        - | 10079 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10080 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10081 | ` * See block-comment on that function for additional information.` |
|        - | 10082 | ` */` |
|     2832 | 10083 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 | 10084 |  |
|        - | 10085 | `	/* Make sure we are ready to execute this program */` |
|     2834 | 10086 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10087 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10088 | `	}` |
|        - | 10089 | `	/* Set the execution magic number  */` |
|     2834 | 10090 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10091 | `	/* Execute the program */` |
|     2834 | 10092 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - | 10093 | `	/* Invoke any shutdown callbacks */` |
|     2834 | 10094 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10095 | `	/*` |
|        - | 10096 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10097 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10098 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10099 | `	 */` |
|     2834 | 10100 | `	return SXRET_OK;` |
|     1418 | 10101 |  |
|        - | 10102 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10103 | `/*` |
|        - | 10104 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10105 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10106 | ` */` |
|       46 | 10107 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 | 10108 |  |
|        - | 10109 | `	ph7_exec_ctx *pCtx;` |
|        - | 10110 | `	ph7_value *pStack;` |
|        - | 10111 | `	VmFrame *pFrame;` |
|       48 | 10112 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 | 10113 | `	if( pCtx == 0 ){` |
|      ! 0 | 10114 | `		return 0;` |
|        - | 10115 | `	}` |
|       48 | 10116 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 | 10117 | `	pCtx->pVm = pVm;` |
|       48 | 10118 | `	pCtx->pFunc = pFunc;` |
|       48 | 10119 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 | 10120 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 | 10121 | `	pCtx->pc = 0;` |
|       48 | 10122 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 | 10123 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10124 | `	/* Allocate a private operand stack */` |
|       48 | 10125 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 | 10126 | `	if( pStack == 0 ){` |
|      ! 0 | 10127 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10128 | `		return 0;` |
|        - | 10129 | `	}` |
|       48 | 10130 | `	pCtx->pStack = pStack;` |
|        - | 10131 | `	/* Create a detached frame for the fiber */` |
|       48 | 10132 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 | 10133 | `	if( pFrame == 0 ){` |
|      ! 0 | 10134 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10135 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10136 | `		return 0;` |
|        - | 10137 | `	}` |
|       48 | 10138 | `	pCtx->pFrame = pFrame;` |
|       48 | 10139 | `	return pCtx;` |
|       25 | 10140 |  |
|        - | 10141 | `/*` |
|        - | 10142 | ` * Start executing a fiber context for the first time.` |
|        - | 10143 | ` */` |
|       46 | 10144 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 | 10145 |  |
|        - | 10146 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10147 | `	sxi32 rc;` |
|       48 | 10148 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10149 | `		return SXERR_INVALID;` |
|        - | 10150 | `	}` |
|        - | 10151 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 | 10152 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 | 10153 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10154 | `	/* Save and set the active context */` |
|       48 | 10155 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 | 10156 | `	pVm->pActiveCtx = pCtx;` |
|       48 | 10157 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 | 10158 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 | 10159 | `	pVm->nRecursionDepth++;` |
|        - | 10160 | `	/* Execute from the beginning */` |
|       48 | 10161 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 | 10162 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 | 10163 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 | 10164 | `	pVm->nRecursionDepth--;` |
|        - | 10165 | `	/* Restore the previous context */` |
|       48 | 10166 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 | 10167 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10168 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 | 10169 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 | 10170 | `		pCtx->pFrame->pParent = 0;` |
|       46 | 10171 | `		if( pResult ){` |
|       24 | 10172 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10173 | `		}` |
|       46 | 10174 | `		return SXRET_OK;` |
|        - | 10175 | `	}` |
|        - | 10176 | `	/* Detach frame */` |
|        3 | 10177 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 | 10178 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 | 10179 | `		pCtx->pFrame->pParent = 0;` |
|        1 | 10180 | `	}` |
|        3 | 10181 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10182 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10183 | `		return PH7_ABORT;` |
|        - | 10184 | `	}` |
|        3 | 10185 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10186 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10187 | `		return PH7_EXCEPTION;` |
|        - | 10188 | `	}` |
|        - | 10189 | `	/* Normal completion */` |
|        3 | 10190 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 | 10191 | `	if( pResult ){` |
|        3 | 10192 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10193 | `	}` |
|        3 | 10194 | `	return SXRET_OK;` |
|       25 | 10195 |  |
|        - | 10196 | `/*` |
|        - | 10197 | ` * Resume a suspended fiber context.` |
|        - | 10198 | ` */` |
|       98 | 10199 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 | 10200 |  |
|        - | 10201 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10202 | `	sxi32 rc;` |
|      100 | 10203 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10204 | `		return SXERR_INVALID;` |
|        - | 10205 | `	}` |
|        - | 10206 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10207 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10208 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 | 10209 | `	if( pResumeValue ){` |
|       40 | 10210 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 | 10211 | `	}else{` |
|       62 | 10212 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10213 | `	}` |
|      100 | 10214 | `	pCtx->nTos++;` |
|        - | 10215 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 | 10216 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 | 10217 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10218 | `	/* Save and set the active context */` |
|      100 | 10219 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 | 10220 | `	pVm->pActiveCtx = pCtx;` |
|      100 | 10221 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 | 10222 | `	pVm->nRecursionDepth++;` |
|        - | 10223 | `	/* Resume execution from saved PC */` |
|      100 | 10224 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 | 10225 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 | 10226 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 | 10227 | `	pVm->nRecursionDepth--;` |
|        - | 10228 | `	/* Restore the previous context */` |
|      100 | 10229 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 | 10230 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10231 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 | 10232 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 | 10233 | `		pCtx->pFrame->pParent = 0;` |
|       64 | 10234 | `		if( pResult ){` |
|       18 | 10235 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10236 | `		}` |
|       64 | 10237 | `		return SXRET_OK;` |
|        - | 10238 | `	}` |
|        - | 10239 | `	/* Detach frame */` |
|       38 | 10240 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 | 10241 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 | 10242 | `		pCtx->pFrame->pParent = 0;` |
|       18 | 10243 | `	}` |
|       38 | 10244 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10245 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10246 | `		return PH7_ABORT;` |
|        - | 10247 | `	}` |
|       38 | 10248 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10249 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10250 | `		return PH7_EXCEPTION;` |
|        - | 10251 | `	}` |
|        - | 10252 | `	/* Normal completion */` |
|       38 | 10253 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 | 10254 | `	if( pResult ){` |
|       20 | 10255 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10256 | `	}` |
|       38 | 10257 | `	return SXRET_OK;` |
|       51 | 10258 |  |
|        - | 10259 | `/*` |
|        - | 10260 | ` * Release an execution context and all its resources.` |
|        - | 10261 | ` */` |
|        4 | 10262 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10263 |  |
|        5 | 10264 | `	if( pCtx == 0 ){` |
|      ! 0 | 10265 | `		return;` |
|        - | 10266 | `	}` |
|        5 | 10267 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10268 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10269 | `		return;` |
|        - | 10270 | `	}` |
|        5 | 10271 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10272 | `	/* Release values */` |
|        5 | 10273 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10274 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10275 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10276 | `	if( pCtx->pFrame ){` |
|        - | 10277 | `		VmSlot *aSlot;` |
|        - | 10278 | `		sxu32 n;` |
|        - | 10279 | `		/* Free local variables */` |
|        5 | 10280 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10281 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10282 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10283 | `		}` |
|        - | 10284 | `		/* Remove local references */` |
|        5 | 10285 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10286 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10287 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10288 | `		}` |
|        5 | 10289 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10290 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10291 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10292 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10293 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10294 | `		pCtx->pFrame = 0;` |
|        2 | 10295 | `	}` |
|        - | 10296 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10297 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10298 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10299 | `	if( pCtx->pStack ){` |
|        5 | 10300 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10301 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10302 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10303 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10304 | `				pTos--;` |
|        1 | 10305 | `			}` |
|        2 | 10306 | `		}` |
|        5 | 10307 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10308 | `		pCtx->pStack = 0;` |
|        2 | 10309 | `	}` |
|        - | 10310 | `	/* Free the context itself */` |
|        5 | 10311 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10312 |  |
|        - | 10313 | `/*` |
|        - | 10314 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10315 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10316 | ` */` |
|       90 | 10317 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 | 10318 |  |
|        - | 10319 | `	ph7_class_instance *pThis;` |
|        - | 10320 | `	SyString sAttr;` |
|        - | 10321 | `	ph7_value *pAttr;` |
|       92 | 10322 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10323 | `		return 0;` |
|        - | 10324 | `	}` |
|       92 | 10325 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 | 10326 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10327 | `		return 0;` |
|        - | 10328 | `	}` |
|       92 | 10329 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 | 10330 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 | 10331 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 | 10332 | `		return 0;` |
|        - | 10333 | `	}` |
|       62 | 10334 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 | 10335 |  |
|        - | 10336 | `/*` |
|        - | 10337 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10338 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10339 | ` */` |
|       38 | 10340 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10341 |  |
|       40 | 10342 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 | 10343 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10344 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10345 | `			"Cannot suspend outside of a fiber");` |
|        - | 10346 | `	}` |
|       40 | 10347 | `	if( nArg > 0 ){` |
|       40 | 10348 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10349 | `	}else{` |
|      ! 0 | 10350 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10351 | `	}` |
|       40 | 10352 | `	return PH7_SUSPEND;` |
|       21 | 10353 |  |
|        - | 10354 | `/*` |
|        - | 10355 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10356 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10357 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10358 | ` */` |
|       24 | 10359 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10360 |  |
|        - | 10361 | `	ph7_class_instance *pThis;` |
|        - | 10362 | `	ph7_value *pAttr;` |
|        - | 10363 | `	SyString sAttrName;` |
|       26 | 10364 | `	if( nArg < 2 ){` |
|      ! 0 | 10365 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10366 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10367 | `	}` |
|       26 | 10368 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10369 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10370 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10371 | `	}` |
|       26 | 10372 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10373 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10374 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10375 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10376 | `	}` |
|        - | 10377 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10378 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10379 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10380 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10381 | `	}` |
|        - | 10382 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10383 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10384 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10385 | `	if( pAttr ){` |
|       26 | 10386 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10387 | `	}` |
|       26 | 10388 | `	return PH7_OK;` |
|       14 | 10389 |  |
|        - | 10390 | `/*` |
|        - | 10391 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10392 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10393 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10394 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10395 | ` */` |
|       24 | 10396 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10397 | `	ph7_class_instance **ppThis)` |
|        2 | 10398 |  |
|       26 | 10399 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10400 | `	ph7_value *pCallable;` |
|        - | 10401 | `	SyString sAttrName;` |
|       26 | 10402 | `	*ppThis = 0;` |
|       26 | 10403 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10404 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10405 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10406 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10407 | `		return 0;` |
|        - | 10408 | `	}` |
|       26 | 10409 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10410 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10411 | `		SyString sName;` |
|        - | 10412 | `		SyHashEntry *pEntry;` |
|        - | 10413 | `		ph7_vm_func *pFunc;` |
|       26 | 10414 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10415 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10416 | `		if( pEntry == 0 ){` |
|      ! 0 | 10417 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10418 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10419 | `			return 0;` |
|        - | 10420 | `		}` |
|       26 | 10421 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10422 | `		return pFunc;` |
|      ! 0 | 10423 | `	}else{` |
|        - | 10424 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10425 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10426 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10427 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10428 | `		if( pMethod == 0 ){` |
|      ! 0 | 10429 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10430 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10431 | `			return 0;` |
|        - | 10432 | `		}` |
|      ! 0 | 10433 | `		*ppThis = pClosure;` |
|      ! 0 | 10434 | `		return &pMethod->sFunc;` |
|        - | 10435 | `	}` |
|       14 | 10436 |  |
|        - | 10437 | `/*` |
|        - | 10438 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10439 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10440 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10441 | ` */` |
|       46 | 10442 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10443 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10444 |  |
|       48 | 10445 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10446 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10447 | `	sxu32 nFormal, n;` |
|        - | 10448 | `	VmSlot sSlot;` |
|        - | 10449 | `	sxi32 rc;` |
|        - | 10450 | `	/* Install $this for closure/method callables */` |
|       48 | 10451 | `	if( pClosureThis ){` |
|        - | 10452 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10453 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10454 | `		if( pObj ){` |
|      ! 0 | 10455 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10456 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10457 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10458 | `		}` |
|      ! 0 | 10459 | `	}` |
|        - | 10460 | `	/* Install static variables */` |
|       48 | 10461 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10462 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10463 | `		ph7_value *pVal;` |
|      ! 0 | 10464 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10465 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10466 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10467 | `			if( pVal ){` |
|      ! 0 | 10468 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10469 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10470 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10471 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10472 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10473 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10474 | `				}` |
|      ! 0 | 10475 | `			}` |
|      ! 0 | 10476 | `		}` |
|      ! 0 | 10477 | `	}` |
|        - | 10478 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10479 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10480 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10481 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10482 | `		ph7_value *pObj;` |
|       20 | 10483 | `		if( n < (sxu32)nArg ){` |
|        - | 10484 | `			/* Argument provided — install with type casting */` |
|       20 | 10485 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10486 | `			if( pObj ){` |
|       20 | 10487 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10488 | `				/* Type casting */` |
|       20 | 10489 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10490 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10491 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10492 | `						if( xCast ){` |
|      ! 0 | 10493 | `							xCast(pObj);` |
|      ! 0 | 10494 | `						}` |
|      ! 0 | 10495 | `					}` |
|      ! 0 | 10496 | `				}` |
|       20 | 10497 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10498 | `				sSlot.pUserData = 0;` |
|       20 | 10499 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10500 | `			}` |
|        9 | 10501 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10502 | `			/* Default value */` |
|      ! 0 | 10503 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10504 | `			if( pObj ){` |
|      ! 0 | 10505 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10506 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10507 | `					return rc;` |
|        - | 10508 | `				}` |
|      ! 0 | 10509 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10510 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10511 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10512 | `						if( xCast ){` |
|      ! 0 | 10513 | `							xCast(pObj);` |
|      ! 0 | 10514 | `						}` |
|      ! 0 | 10515 | `					}` |
|      ! 0 | 10516 | `				}` |
|      ! 0 | 10517 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10518 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10519 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10520 | `			}` |
|      ! 0 | 10521 | `		}` |
|       11 | 10522 | `	}` |
|        - | 10523 | `	/* Install closure environment (captured variables) */` |
|       48 | 10524 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10525 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10526 | `		ph7_value *pValue;` |
|        - | 10527 | `		sxu32 iEnv;` |
|        3 | 10528 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10529 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10530 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10531 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10532 | `				continue;` |
|        - | 10533 | `			}` |
|        5 | 10534 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10535 | `			if( pValue == 0 ){` |
|      ! 0 | 10536 | `				continue;` |
|        - | 10537 | `			}` |
|        5 | 10538 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10539 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10540 | `		}` |
|        1 | 10541 | `	}` |
|       48 | 10542 | `	return SXRET_OK;` |
|       25 | 10543 |  |
|        - | 10544 | `/*` |
|        - | 10545 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10546 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10547 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10548 | ` */` |
|       26 | 10549 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10550 |  |
|       28 | 10551 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10552 | `	ph7_class_instance *pThis;` |
|        - | 10553 | `	ph7_class_instance *pClosureThis;` |
|        - | 10554 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10555 | `	ph7_vm_func *pFunc;` |
|        - | 10556 | `	ph7_value sResult;` |
|        - | 10557 | `	ph7_value *pCtxAttr;` |
|        - | 10558 | `	SyString sAttrName;` |
|        - | 10559 | `	sxi32 rc;` |
|       28 | 10560 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10561 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10562 | `	}` |
|       28 | 10563 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10564 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10565 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10566 | `	if( pExecCtx != 0 ){` |
|        3 | 10567 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10568 | `			"Cannot start a fiber that has already been started");` |
|        - | 10569 | `	}` |
|        - | 10570 | `	/* Resolve callable */` |
|       26 | 10571 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10572 | `	if( pFunc == 0 ){` |
|      ! 0 | 10573 | `		return PH7_EXCEPTION;` |
|        - | 10574 | `	}` |
|        - | 10575 | `	/* Create execution context now that we know the function */` |
|       26 | 10576 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10577 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10578 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10579 | `			"Fiber::start(): out of memory");` |
|        - | 10580 | `	}` |
|        - | 10581 | `	/* Store context in $this->__ctx */` |
|       26 | 10582 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10583 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10584 | `	if( pCtxAttr ){` |
|       26 | 10585 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10586 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10587 | `	}` |
|        - | 10588 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10589 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10590 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10591 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10592 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10593 | `	/* Unpack the args array and install into the frame */` |
|        - | 10594 | `	{` |
|       26 | 10595 | `		ph7_value **apValues = 0;` |
|       26 | 10596 | `		int nActual = 0;` |
|       26 | 10597 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10598 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10599 | `			ph7_hashmap_node *pNode;` |
|       26 | 10600 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10601 | `			if( nCount > 0 ){` |
|        3 | 10602 | `				sxu32 idx = 0;` |
|        4 | 10603 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10604 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10605 | `				if( apValues ){` |
|        3 | 10606 | `					pNode = pMap->pFirst;` |
|        7 | 10607 | `					while( pNode && idx < nCount ){` |
|        5 | 10608 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10609 | `						idx++;` |
|        5 | 10610 | `						pNode = pNode->pPrev;` |
|        1 | 10611 | `					}` |
|        3 | 10612 | `					nActual = (int)idx;` |
|        1 | 10613 | `				}` |
|        1 | 10614 | `			}` |
|       12 | 10615 | `		}` |
|       26 | 10616 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10617 | `		if( apValues ){` |
|        3 | 10618 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10619 | `		}` |
|        - | 10620 | `	}` |
|        - | 10621 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10622 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10623 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10624 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10625 | `		return PH7_ABORT;` |
|        - | 10626 | `	}` |
|       26 | 10627 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10628 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10629 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10630 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10631 | `		return PH7_ABORT;` |
|        - | 10632 | `	}` |
|       26 | 10633 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10634 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10635 | `		return PH7_EXCEPTION;` |
|        - | 10636 | `	}` |
|       26 | 10637 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10638 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10639 | `	return PH7_OK;` |
|       15 | 10640 |  |
|        - | 10641 | `/*` |
|        - | 10642 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10643 | ` */` |
|       36 | 10644 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10645 |  |
|       38 | 10646 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10647 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10648 | `	ph7_value sResult;` |
|        - | 10649 | `	ph7_value *pResumeVal;` |
|        - | 10650 | `	sxi32 rc;` |
|       38 | 10651 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10652 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10653 | `		return PH7_OK;` |
|        - | 10654 | `	}` |
|       38 | 10655 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10656 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10657 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10658 | `		return PH7_OK;` |
|        - | 10659 | `	}` |
|       38 | 10660 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10661 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10662 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10663 | `	}` |
|       36 | 10664 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10665 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10666 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10667 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10668 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10669 | `		return PH7_ABORT;` |
|        - | 10670 | `	}` |
|       36 | 10671 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10672 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10673 | `		return PH7_EXCEPTION;` |
|        - | 10674 | `	}` |
|       36 | 10675 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10676 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10677 | `	return PH7_OK;` |
|       20 | 10678 |  |
|        - | 10679 | `/*` |
|        - | 10680 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10681 | ` */` |
|        6 | 10682 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10683 |  |
|        8 | 10684 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10685 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10686 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10687 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10688 | `		return PH7_OK;` |
|        - | 10689 | `	}` |
|        8 | 10690 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10691 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10692 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10693 | `		return PH7_OK;` |
|        - | 10694 | `	}` |
|        8 | 10695 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10696 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10697 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10698 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10699 | `		}` |
|      ! 0 | 10700 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10701 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10702 | `	}` |
|        8 | 10703 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10704 | `	return PH7_OK;` |
|        5 | 10705 |  |
|        - | 10706 | `/*` |
|        - | 10707 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10708 | ` */` |
|        6 | 10709 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10710 |  |
|        - | 10711 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10712 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10713 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10714 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10715 | `	return PH7_OK;` |
|        4 | 10716 |  |
|      ! 0 | 10717 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10718 |  |
|        - | 10719 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10720 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10721 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10722 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10723 | `	return PH7_OK;` |
|      ! 0 | 10724 |  |
|        6 | 10725 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10726 |  |
|        - | 10727 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10728 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10729 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10730 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10731 | `	return PH7_OK;` |
|        4 | 10732 |  |
|        6 | 10733 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10734 |  |
|        - | 10735 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10736 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10737 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10738 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10739 | `	return PH7_OK;` |
|        4 | 10740 |  |
|        - | 10741 | `/*` |
|        - | 10742 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10743 | ` */` |
|        4 | 10744 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10745 |  |
|        5 | 10746 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10747 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10748 | `	if( nArg < 1 ){` |
|      ! 0 | 10749 | `		return PH7_OK;` |
|        - | 10750 | `	}` |
|        5 | 10751 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10752 | `	if( pExecCtx ){` |
|        5 | 10753 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10754 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10755 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10756 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10757 | `			SyString sAttrName;` |
|        - | 10758 | `			ph7_value *pAttr;` |
|        5 | 10759 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10760 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10761 | `			if( pAttr ){` |
|        5 | 10762 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10763 | `			}` |
|        2 | 10764 | `		}` |
|        2 | 10765 | `	}` |
|        5 | 10766 | `	return PH7_OK;` |
|        3 | 10767 |  |
|        - | 10768 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10769 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10770 |  |
|        - | 10771 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10772 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10773 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10774 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10775 |  |
|      ! 0 | 10776 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10777 |  |
|        - | 10778 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10779 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10780 | `	ph7_exec_ctx *pCtx;` |
|        - | 10781 | `	ph7_vm_func *pFunc;` |
|        - | 10782 | `	ph7_value *pCallable;` |
|        - | 10783 | `	ph7_value *pCtxAttr;` |
|        - | 10784 | `	SyString sAttrName;` |
|        - | 10785 | `	/* Must not already be started */` |
|      ! 0 | 10786 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10787 | `	if( pCtx != 0 ){` |
|      ! 0 | 10788 | `		return SXERR_INVALID;` |
|        - | 10789 | `	}` |
|      ! 0 | 10790 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10791 | `		return SXERR_INVALID;` |
|        - | 10792 | `	}` |
|      ! 0 | 10793 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10794 | `	/* Get the callable */` |
|      ! 0 | 10795 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10796 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10797 | `	if( pCallable == 0 ){` |
|      ! 0 | 10798 | `		return SXERR_INVALID;` |
|        - | 10799 | `	}` |
|        - | 10800 | `	/* Resolve callable */` |
|      ! 0 | 10801 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10802 | `		SyString sName;` |
|        - | 10803 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10804 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10805 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10806 | `		if( pEntry == 0 ){` |
|      ! 0 | 10807 | `			return SXERR_NOTFOUND;` |
|        - | 10808 | `		}` |
|      ! 0 | 10809 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10810 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10811 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10812 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10813 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10814 | `		if( pMethod == 0 ){` |
|      ! 0 | 10815 | `			return SXERR_INVALID;` |
|        - | 10816 | `		}` |
|      ! 0 | 10817 | `		pClosureThis = pClosure;` |
|      ! 0 | 10818 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10819 | `	}else{` |
|      ! 0 | 10820 | `		return SXERR_INVALID;` |
|        - | 10821 | `	}` |
|        - | 10822 | `	/* Create context */` |
|      ! 0 | 10823 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10824 | `	if( pCtx == 0 ){` |
|      ! 0 | 10825 | `		return SXERR_MEM;` |
|        - | 10826 | `	}` |
|        - | 10827 | `	/* Store in __ctx */` |
|      ! 0 | 10828 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10829 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10830 | `	if( pCtxAttr ){` |
|      ! 0 | 10831 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10832 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10833 | `	}` |
|        - | 10834 | `	/* Set up frame with args */` |
|      ! 0 | 10835 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10836 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10837 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10838 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10839 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10840 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10841 |  |
|      ! 0 | 10842 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10843 |  |
|      ! 0 | 10844 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10845 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10846 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10847 |  |
|      ! 0 | 10848 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10849 |  |
|      ! 0 | 10850 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10851 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10852 |  |
|      ! 0 | 10853 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10854 |  |
|      ! 0 | 10855 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10856 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10857 |  |
|      ! 0 | 10858 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10859 |  |
|      ! 0 | 10860 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10861 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10862 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10863 |  |
|        - | 10864 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10865 | `/*` |
|        - | 10866 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10867 | ` */` |
|       22 | 10868 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10869 |  |
|        - | 10870 | `	ph7_generator *pGen;` |
|       24 | 10871 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10872 | `	if( pGen == 0 ){` |
|      ! 0 | 10873 | `		return 0;` |
|        - | 10874 | `	}` |
|       24 | 10875 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10876 | `	pGen->pCtx = pCtx;` |
|       24 | 10877 | `	pGen->iImplicitKey = 0;` |
|       24 | 10878 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10879 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10880 | `	/* Link the generator back to the exec context */` |
|       24 | 10881 | `	pCtx->pPrivate = pGen;` |
|       24 | 10882 | `	return pGen;` |
|       13 | 10883 |  |
|        - | 10884 | `/*` |
|        - | 10885 | ` * Release a generator and its execution context.` |
|        - | 10886 | ` */` |
|      ! 0 | 10887 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10888 |  |
|      ! 0 | 10889 | `	if( pGen == 0 ){` |
|      ! 0 | 10890 | `		return;` |
|        - | 10891 | `	}` |
|      ! 0 | 10892 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10893 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10894 | `	if( pGen->pCtx ){` |
|      ! 0 | 10895 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10896 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10897 | `		pGen->pCtx = 0;` |
|      ! 0 | 10898 | `	}` |
|      ! 0 | 10899 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10900 |  |
|        - | 10901 | `/*` |
|        - | 10902 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10903 | ` */` |
|      236 | 10904 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10905 |  |
|        - | 10906 | `	ph7_class_instance *pThis;` |
|        - | 10907 | `	SyString sAttr;` |
|        - | 10908 | `	ph7_value *pAttr;` |
|      238 | 10909 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10910 | `		return 0;` |
|        - | 10911 | `	}` |
|      238 | 10912 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10913 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10914 | `		return 0;` |
|        - | 10915 | `	}` |
|      238 | 10916 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10917 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10918 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10919 | `		return 0;` |
|        - | 10920 | `	}` |
|      238 | 10921 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10922 |  |
|        - | 10923 | `/*` |
|        - | 10924 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10925 | ` */` |
|       22 | 10926 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10927 |  |
|        - | 10928 | `	ph7_generator *pGen;` |
|        - | 10929 | `	sxi32 rc;` |
|       24 | 10930 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10931 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10932 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10933 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10934 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10935 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10936 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10937 | `	}` |
|       24 | 10938 | `	return PH7_OK;` |
|       13 | 10939 |  |
|        - | 10940 | `/*` |
|        - | 10941 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10942 | ` */` |
|       68 | 10943 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10944 |  |
|        - | 10945 | `	ph7_generator *pGen;` |
|       70 | 10946 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10947 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10948 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10949 | `	return PH7_OK;` |
|       36 | 10950 |  |
|        - | 10951 | `/*` |
|        - | 10952 | ` * Generator::current() — return the last yielded value.` |
|        - | 10953 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10954 | ` */` |
|       68 | 10955 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10956 |  |
|        - | 10957 | `	ph7_generator *pGen;` |
|        - | 10958 | `	sxi32 rc;` |
|       70 | 10959 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10960 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10961 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10962 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10963 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10964 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10965 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10966 | `	}` |
|       70 | 10967 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10968 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10969 | `	}else{` |
|      ! 0 | 10970 | `		ph7_result_null(pCtx);` |
|        - | 10971 | `	}` |
|       70 | 10972 | `	return PH7_OK;` |
|       36 | 10973 |  |
|        - | 10974 | `/*` |
|        - | 10975 | ` * Generator::key() — return the last yielded key.` |
|        - | 10976 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10977 | ` */` |
|       12 | 10978 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10979 |  |
|        - | 10980 | `	ph7_generator *pGen;` |
|        - | 10981 | `	sxi32 rc;` |
|       13 | 10982 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10983 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10984 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10985 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10986 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10987 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10988 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10989 | `	}` |
|       13 | 10990 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10991 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10992 | `	}else{` |
|      ! 0 | 10993 | `		ph7_result_null(pCtx);` |
|        - | 10994 | `	}` |
|       13 | 10995 | `	return PH7_OK;` |
|        7 | 10996 |  |
|        - | 10997 | `/*` |
|        - | 10998 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10999 | ` */` |
|       60 | 11000 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11001 |  |
|        - | 11002 | `	ph7_generator *pGen;` |
|        - | 11003 | `	sxi32 rc;` |
|       62 | 11004 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 11005 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 11006 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 11007 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11008 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 11009 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 11010 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 11011 | `	}else{` |
|      ! 0 | 11012 | `		return PH7_OK;` |
|        - | 11013 | `	}` |
|       62 | 11014 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 11015 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 11016 | `	return PH7_OK;` |
|       32 | 11017 |  |
|        - | 11018 | `/*` |
|        - | 11019 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11020 | ` */` |
|        4 | 11021 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11022 |  |
|        - | 11023 | `	ph7_generator *pGen;` |
|        - | 11024 | `	ph7_value *pSendVal;` |
|        - | 11025 | `	sxi32 rc;` |
|        5 | 11026 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11027 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11028 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11029 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11030 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11031 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11032 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11033 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11034 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11035 | `	}else{` |
|      ! 0 | 11036 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11037 | `		return PH7_OK;` |
|        - | 11038 | `	}` |
|        5 | 11039 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11040 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11041 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11042 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11043 | `	}else{` |
|        3 | 11044 | `		ph7_result_null(pCtx);` |
|        - | 11045 | `	}` |
|        5 | 11046 | `	return PH7_OK;` |
|        3 | 11047 |  |
|        - | 11048 | `/*` |
|        - | 11049 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11050 | ` *` |
|        - | 11051 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11052 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11053 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11054 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11055 | ` * the exception to the caller.` |
|        - | 11056 | ` */` |
|      ! 0 | 11057 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11058 |  |
|        - | 11059 | `	ph7_generator *pGen;` |
|        - | 11060 | `	const char *zMsg;` |
|        - | 11061 | `	int nLen;` |
|      ! 0 | 11062 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11063 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11064 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11065 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11066 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11067 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11068 | `			"Cannot throw into a closed generator");` |
|        - | 11069 | `	}` |
|        - | 11070 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11071 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11072 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11073 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11074 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11075 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11076 | `	nLen = 0;` |
|      ! 0 | 11077 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11078 | `		/* Try to get the exception's message */` |
|        - | 11079 | `		SyString sAttr;` |
|        - | 11080 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11081 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11082 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11083 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11084 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11085 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11086 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11087 | `		}` |
|      ! 0 | 11088 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11089 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11090 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11091 | `	}` |
|      ! 0 | 11092 | `	(void)nLen;` |
|      ! 0 | 11093 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11094 |  |
|        - | 11095 | `/*` |
|        - | 11096 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11097 | ` */` |
|        2 | 11098 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11099 |  |
|        - | 11100 | `	ph7_generator *pGen;` |
|        3 | 11101 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11102 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11103 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11104 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11105 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11106 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11107 | `	}` |
|        3 | 11108 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11109 | `	return PH7_OK;` |
|        2 | 11110 |  |
|        - | 11111 | `/*` |
|        - | 11112 | ` * Generator::__destruct() — clean up.` |
|        - | 11113 | ` */` |
|      ! 0 | 11114 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11115 |  |
|        - | 11116 | `	ph7_generator *pGen;` |
|      ! 0 | 11117 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11118 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11119 | `	if( pGen ){` |
|      ! 0 | 11120 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11121 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11122 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11123 | `			SyString sAttrName;` |
|        - | 11124 | `			ph7_value *pAttr;` |
|      ! 0 | 11125 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11126 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11127 | `			if( pAttr ){` |
|      ! 0 | 11128 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11129 | `			}` |
|      ! 0 | 11130 | `		}` |
|      ! 0 | 11131 | `	}` |
|      ! 0 | 11132 | `	return PH7_OK;` |
|      ! 0 | 11133 |  |
|        - | 11134 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11135 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11136 | `/*` |
|        - | 11137 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11138 | ` * the desired message.` |
|        - | 11139 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11140 | ` * in 'api.c' for additional information.` |
|        - | 11141 | ` */` |
|      370 | 11142 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11143 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11144 | `	SyString *pString /* Message to output */` |
|        - | 11145 | `	)` |
|        2 | 11146 |  |
|      372 | 11147 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 11148 | `	sxi32 rc = SXRET_OK;` |
|        - | 11149 | `	/* Call the output consumer */` |
|      372 | 11150 | `	if( pString->nByte > 0 ){` |
|      372 | 11151 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 11152 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11153 | `	}` |
|      372 | 11154 | `	return rc;` |
|        2 | 11155 |  |
|        - | 11156 | `/*` |
|        - | 11157 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11158 | ` * callback to consume the formatted message.` |
|        - | 11159 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11160 | ` * in 'api.c' for additional information.` |
|        - | 11161 | ` */` |
|        2 | 11162 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11163 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11164 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11165 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11166 | `	)` |
|        1 | 11167 |  |
|        3 | 11168 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11169 | `	sxi32 rc = SXRET_OK;` |
|        - | 11170 | `	SyBlob sWorker;` |
|        - | 11171 | `	/* Format the message and call the output consumer */` |
|        3 | 11172 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11173 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11174 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11175 | `		/* Consume the formatted message */` |
|        3 | 11176 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11177 | `	}` |
|        3 | 11178 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11179 | `	/* Release the working buffer */` |
|        3 | 11180 | `	SyBlobRelease(&sWorker);` |
|        3 | 11181 | `	return rc;` |
|        1 | 11182 |  |
|        - | 11183 | `/*` |
|        - | 11184 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11185 | ` * This function never fail and always return a pointer` |
|        - | 11186 | ` * to a null terminated string.` |
|        - | 11187 | ` */` |
|       12 | 11188 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11189 |  |
|       13 | 11190 | `	const char *zOp = "Unknown     ";` |
|       13 | 11191 | `	switch(nOp){` |
|        3 | 11192 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11193 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11194 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11195 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11196 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11197 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11198 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11199 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11200 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11201 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11202 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11203 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11204 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11205 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11206 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11207 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11208 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11209 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11210 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11211 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11212 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11213 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11214 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11215 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11216 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11217 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11218 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11219 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11220 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11221 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11222 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11223 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11224 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11225 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11226 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11227 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11228 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11229 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11230 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11231 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11232 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11233 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11234 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11235 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11236 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11237 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11238 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11239 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11240 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11241 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11242 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11243 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11244 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11245 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11246 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11247 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11248 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11249 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11250 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11251 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11252 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11253 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11254 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11255 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11256 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11257 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11258 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11259 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11260 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11261 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11262 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11263 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11264 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11265 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11266 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11267 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11268 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11269 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11270 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11271 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11272 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11273 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11274 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11275 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11276 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11277 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11278 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11279 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11280 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11281 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11282 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11283 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11284 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11285 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11286 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11287 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11288 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11289 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11290 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11291 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11292 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11293 | `	default:` |
|      ! 0 | 11294 | `		break;` |
|        - | 11295 | `	}` |
|       13 | 11296 | `	return zOp;` |
|        1 | 11297 |  |
|        - | 11298 | `/*` |
|        - | 11299 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11300 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11301 | ` * is responsible of consuming the generated dump.` |
|        - | 11302 | ` */` |
|        2 | 11303 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11304 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11305 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11306 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11307 | `	)` |
|        1 | 11308 |  |
|        - | 11309 | `	sxi32 rc;` |
|        3 | 11310 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11311 | `	return rc;` |
|        1 | 11312 |  |
|        - | 11313 | `/*` |
|        - | 11314 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11315 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11316 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11317 | ` * in 'compile.c' for additional information.` |
|        - | 11318 | ` */` |
|       14 | 11319 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11320 |  |
|       15 | 11321 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11322 | `	/* Evaluate and expand constant value */` |
|       15 | 11323 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 11324 |  |
|        - | 11325 | `/*` |
|        - | 11326 | ` * Section:` |
|        - | 11327 | ` *  Function handling functions.` |
|        - | 11328 | ` * Status:` |
|        - | 11329 | ` *    Stable.` |
|        - | 11330 | ` */` |
|        - | 11331 | `/*` |
|        - | 11332 | ` * int func_num_args(void)` |
|        - | 11333 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11334 | ` * Parameters` |
|        - | 11335 | ` *   None.` |
|        - | 11336 | ` * Return` |
|        - | 11337 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11338 | ` *  or -1 if called from the globe scope.` |
|        - | 11339 | ` */` |
|      980 | 11340 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11341 |  |
|        - | 11342 | `	VmFrame *pFrame;` |
|        - | 11343 | `	ph7_vm *pVm;` |
|        - | 11344 | `	/* Point to the target VM */` |
|      982 | 11345 | `	pVm = pCtx->pVm;` |
|        - | 11346 | `	/* Current frame */` |
|      982 | 11347 | `	pFrame = pVm->pFrame;` |
|      982 | 11348 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 11349 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11350 | `		SXUNUSED(nArg);` |
|      ! 0 | 11351 | `		SXUNUSED(apArg);` |
|        - | 11352 | `		/* Global frame,return -1 */` |
|      ! 0 | 11353 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11354 | `		return SXRET_OK;` |
|        - | 11355 | `	}` |
|        - | 11356 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 11357 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 11358 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 11359 | `	return SXRET_OK;` |
|      492 | 11360 |  |
|        - | 11361 | `/*` |
|        - | 11362 | ` * value func_get_arg(int $arg_num)` |
|        - | 11363 | ` *   Return an item from the argument list.` |
|        - | 11364 | ` * Parameters` |
|        - | 11365 | ` *  Argument number(index start from zero).` |
|        - | 11366 | ` * Return` |
|        - | 11367 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11368 | ` */` |
|       22 | 11369 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11370 |  |
|       24 | 11371 | `	ph7_value *pObj = 0;` |
|       24 | 11372 | `	VmSlot *pSlot = 0;` |
|        - | 11373 | `	VmFrame *pFrame;` |
|        - | 11374 | `	ph7_vm *pVm;` |
|        - | 11375 | `	/* Point to the target VM */` |
|       24 | 11376 | `	pVm = pCtx->pVm;` |
|        - | 11377 | `	/* Current frame */` |
|       24 | 11378 | `	pFrame = pVm->pFrame;` |
|       24 | 11379 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11380 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11381 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11382 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11383 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11384 | `		return SXRET_OK;` |
|        - | 11385 | `	}` |
|        - | 11386 | `	/* Extract the desired index */` |
|       21 | 11387 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11388 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11389 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11390 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11391 | `		return SXRET_OK;` |
|        - | 11392 | `	}` |
|        - | 11393 | `	/* Extract the desired argument */` |
|       21 | 11394 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11395 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11396 | `			/* Return the desired argument */` |
|       21 | 11397 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11398 | `		}else{` |
|        - | 11399 | `			/* No such argument,return false */` |
|      ! 0 | 11400 | `			ph7_result_bool(pCtx,0);` |
|        - | 11401 | `		}` |
|       11 | 11402 | `	}else{` |
|        - | 11403 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11404 | `		ph7_result_bool(pCtx,0);` |
|        - | 11405 | `	}` |
|       21 | 11406 | `	return SXRET_OK;` |
|       13 | 11407 |  |
|        - | 11408 | `/*` |
|        - | 11409 | ` * array func_get_args_byref(void)` |
|        - | 11410 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11411 | ` * Parameters` |
|        - | 11412 | ` *  None.` |
|        - | 11413 | ` * Return` |
|        - | 11414 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11415 | ` *  member of the current user-defined function's argument list.` |
|        - | 11416 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11417 | ` * NOTE:` |
|        - | 11418 | ` *  Arguments are returned to the array by reference.` |
|        - | 11419 | ` */` |
|        2 | 11420 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11421 |  |
|        - | 11422 | `	ph7_value *pArray;` |
|        - | 11423 | `	VmFrame *pFrame;` |
|        - | 11424 | `	VmSlot *aSlot;` |
|        - | 11425 | `	sxu32 n;` |
|        - | 11426 | `	/* Point to the current frame */` |
|        3 | 11427 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11428 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11429 | `	if( pFrame->pParent == 0 ){` |
|        - | 11430 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11431 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11432 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11433 | `		return SXRET_OK;` |
|        - | 11434 | `	}` |
|        - | 11435 | `	/* Create a new array */` |
|        3 | 11436 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11437 | `	if( pArray == 0 ){` |
|      ! 0 | 11438 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11439 | `		SXUNUSED(apArg);` |
|      ! 0 | 11440 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11441 | `		return SXRET_OK;` |
|        - | 11442 | `	}` |
|        - | 11443 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11444 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11445 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11446 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11447 | `	}` |
|        - | 11448 | `	/* Return the freshly created array */` |
|        3 | 11449 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11450 | `	return SXRET_OK;` |
|        2 | 11451 |  |
|        - | 11452 | `/*` |
|        - | 11453 | ` * array func_get_args(void)` |
|        - | 11454 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11455 | ` * Parameters` |
|        - | 11456 | ` *  None.` |
|        - | 11457 | ` * Return` |
|        - | 11458 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11459 | ` *  member of the current user-defined function's argument list.` |
|        - | 11460 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11461 | ` */` |
|       88 | 11462 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11463 |  |
|       90 | 11464 | `	ph7_value *pObj = 0;` |
|        - | 11465 | `	ph7_value *pArray;` |
|        - | 11466 | `	VmFrame *pFrame;` |
|        - | 11467 | `	VmSlot *aSlot;` |
|        - | 11468 | `	sxu32 n;` |
|        - | 11469 | `	/* Point to the current frame */` |
|       90 | 11470 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11471 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11472 | `	if( pFrame->pParent == 0 ){` |
|        - | 11473 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11474 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11475 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11476 | `		return SXRET_OK;` |
|        - | 11477 | `	}` |
|        - | 11478 | `	/* Create a new array */` |
|       90 | 11479 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11480 | `	if( pArray == 0 ){` |
|      ! 0 | 11481 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11482 | `		SXUNUSED(apArg);` |
|      ! 0 | 11483 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11484 | `		return SXRET_OK;` |
|        - | 11485 | `	}` |
|        - | 11486 | `	/* Start filling the array with the given arguments */` |
|       90 | 11487 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11488 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11489 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11490 | `		if( pObj ){` |
|      134 | 11491 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11492 | `		}` |
|       68 | 11493 | `	}` |
|        - | 11494 | `	/* Return the freshly created array */` |
|       90 | 11495 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11496 | `	return SXRET_OK;` |
|       46 | 11497 |  |
|        - | 11498 | `/*` |
|        - | 11499 | ` * bool function_exists(string $name)` |
|        - | 11500 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11501 | ` * Parameters` |
|        - | 11502 | ` *  The name of the desired function.` |
|        - | 11503 | ` * Return` |
|        - | 11504 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11505 | ` */` |
|     1742 | 11506 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11507 |  |
|        - | 11508 | `	const char *zName;` |
|        - | 11509 | `	ph7_vm *pVm;` |
|        - | 11510 | `	int nLen;` |
|        - | 11511 | `	int res;` |
|     1744 | 11512 | `	if( nArg < 1 ){` |
|        - | 11513 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11514 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11515 | `		return SXRET_OK;` |
|        - | 11516 | `	}` |
|        - | 11517 | `	/* Point to the target VM */` |
|     1744 | 11518 | `	pVm = pCtx->pVm;` |
|        - | 11519 | `	/* Extract the function name */` |
|     1744 | 11520 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11521 | `	/* Assume the function is not defined */` |
|     1744 | 11522 | `	res = 0;` |
|        - | 11523 | `	/* Perform the lookup */` |
|     2613 | 11524 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1738 | 11525 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11526 | `			/* Function is defined */` |
|      266 | 11527 | `			res = 1;` |
|      132 | 11528 | `	}` |
|     1744 | 11529 | `	ph7_result_bool(pCtx,res);` |
|     1744 | 11530 | `	return SXRET_OK;` |
|      873 | 11531 |  |
|        - | 11532 | `/*` |
|        - | 11533 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11534 | ` * [i.e: Whether it is callable or not].` |
|        - | 11535 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11536 | ` */` |
|    23864 | 11537 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11538 |  |
|    23866 | 11539 | `	int res = 0;` |
|    23866 | 11540 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11541 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11542 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11543 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11544 | `		 * standard PHP behavior. */` |
|       20 | 11545 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11546 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11547 | `			res = 1;` |
|       10 | 11548 | `		}` |
|        9 | 11549 | `		(void)CallInvoke;` |
|    23857 | 11550 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11551 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11552 | `		if( pMap->nEntry == 2 ){` |
|        - | 11553 | `			ph7_class *pClass;` |
|        - | 11554 | `			ph7_value *pV;` |
|        - | 11555 | `			/* Extract the target class */` |
|       12 | 11556 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11557 | `			if( pV ){` |
|       12 | 11558 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11559 | `				if( pClass ){` |
|        - | 11560 | `					ph7_class_method *pMethod;` |
|        - | 11561 | `					/* Extract the target method */` |
|       10 | 11562 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11563 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11564 | `						/* Perform the lookup */` |
|       10 | 11565 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11566 | `						if( pMethod ){` |
|        - | 11567 | `							/* Method is callable */` |
|        5 | 11568 | `							res = 1;` |
|        2 | 11569 | `						}` |
|        4 | 11570 | `					}` |
|        4 | 11571 | `				}` |
|        5 | 11572 | `			}` |
|        7 | 11573 | `		}` |
|    23835 | 11574 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11575 | `		const char *zName;` |
|        - | 11576 | `		int nLen;` |
|        - | 11577 | `		/* Extract the name */` |
|     5872 | 11578 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11579 | `		/* Perform the lookup */` |
|     5887 | 11580 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11581 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11582 | `				/* Function is callable */` |
|     5854 | 11583 | `				res = 1;` |
|     2926 | 11584 | `		}` |
|     2935 | 11585 | `	}` |
|    23866 | 11586 | `	return res;` |
|        2 | 11587 |  |
|        - | 11588 | `/*` |
|        - | 11589 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11590 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11591 | ` * Parameters` |
|        - | 11592 | ` * $name` |
|        - | 11593 | ` *    The callback function to check` |
|        - | 11594 | ` * $syntax_only` |
|        - | 11595 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11596 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11597 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11598 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11599 | ` *    a string.` |
|        - | 11600 | ` * Return` |
|        - | 11601 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11602 | ` */` |
|       20 | 11603 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11604 |  |
|        - | 11605 | `	ph7_vm *pVm;` |
|        - | 11606 | `	int res;` |
|       21 | 11607 | `	if( nArg < 1 ){` |
|        - | 11608 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11609 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11610 | `		return SXRET_OK;` |
|        - | 11611 | `	}` |
|        - | 11612 | `	/* Point to the target VM */` |
|       21 | 11613 | `	pVm = pCtx->pVm;` |
|        - | 11614 | `	/* Perform the requested operation */` |
|       21 | 11615 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11616 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11617 | `	return SXRET_OK;` |
|       11 | 11618 |  |
|        - | 11619 | `/*` |
|        - | 11620 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11621 | ` * defined below.` |
|        - | 11622 | ` */` |
|     1306 | 11623 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11624 |  |
|     1307 | 11625 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11626 | `	ph7_value sName;` |
|        - | 11627 | `	sxi32 rc;` |
|        - | 11628 | `	/* Prepare the function name for insertion */` |
|     1307 | 11629 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11630 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11631 | `	/* Perform the insertion */` |
|     1307 | 11632 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11633 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11634 | `	return rc;` |
|        1 | 11635 |  |
|        - | 11636 | `/*` |
|        - | 11637 | ` * array get_defined_functions(void)` |
|        - | 11638 | ` *  Returns an array of all defined functions.` |
|        - | 11639 | ` * Parameter` |
|        - | 11640 | ` *  None.` |
|        - | 11641 | ` * Return` |
|        - | 11642 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11643 | ` *  both built-in (internal) and user-defined.` |
|        - | 11644 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11645 | ` *  defined ones using $arr["user"].` |
|        - | 11646 | ` * Note:` |
|        - | 11647 | ` *  NULL is returned on failure.` |
|        - | 11648 | ` */` |
|        2 | 11649 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11650 |  |
|        - | 11651 | `	ph7_value *pArray,*pEntry;` |
|        - | 11652 | `	/* NOTE:` |
|        - | 11653 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11654 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11655 | `	 */` |
|        3 | 11656 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11657 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11658 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11659 | `		SXUNUSED(apArg);` |
|        - | 11660 | `		/* Return NULL */` |
|      ! 0 | 11661 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11662 | `		return SXRET_OK;` |
|        - | 11663 | `	}` |
|        3 | 11664 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11665 | `	if( pEntry == 0 ){` |
|        - | 11666 | `		/* Return NULL */` |
|      ! 0 | 11667 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11668 | `		return SXRET_OK;` |
|        - | 11669 | `	}` |
|        - | 11670 | `	/* Fill with the appropriate information */` |
|        3 | 11671 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11672 | `	/* Create the 'internal' index */` |
|        3 | 11673 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11674 | `	/* Create the user-func array */` |
|        3 | 11675 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11676 | `	if( pEntry == 0 ){` |
|        - | 11677 | `		/* Return NULL */` |
|      ! 0 | 11678 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11679 | `		return SXRET_OK;` |
|        - | 11680 | `	}` |
|        - | 11681 | `	/* Fill with the appropriate information */` |
|        3 | 11682 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11683 | `	/* Create the 'user' index */` |
|        3 | 11684 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11685 | `	/* Return the multi-dimensional array */` |
|        3 | 11686 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11687 | `	return SXRET_OK;` |
|        2 | 11688 |  |
|        - | 11689 | `/*` |
|        - | 11690 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11691 | ` *  Register a function for execution on shutdown.` |
|        - | 11692 | ` * Note` |
|        - | 11693 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11694 | ` *  be called in the same order as they were registered.` |
|        - | 11695 | ` * Parameters` |
|        - | 11696 | ` *  $callback` |
|        - | 11697 | ` *   The shutdown callback to register.` |
|        - | 11698 | ` * $param` |
|        - | 11699 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11700 | ` * Return` |
|        - | 11701 | ` *  Nothing.` |
|        - | 11702 | ` */` |
|       10 | 11703 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11704 |  |
|        - | 11705 | `	VmShutdownCB sEntry;` |
|        - | 11706 | `	int i,j;` |
|       12 | 11707 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11708 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11709 | `		return PH7_OK;` |
|        - | 11710 | `	}` |
|        - | 11711 | `	/* Zero the Entry */` |
|       12 | 11712 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11713 | `	/* Initialize fields */` |
|       12 | 11714 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11715 | `	/* Save the callback name for later invocation name */` |
|       12 | 11716 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11717 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11718 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11719 | `	}` |
|        - | 11720 | `	/* Copy arguments */` |
|       12 | 11721 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11722 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11723 | `			/* Limit reached */` |
|      ! 0 | 11724 | `			break;` |
|        - | 11725 | `		}` |
|      ! 0 | 11726 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11727 | `	}` |
|       12 | 11728 | `	sEntry.nArg = j;` |
|        - | 11729 | `	/* Install the callback */` |
|       12 | 11730 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11731 | `	return PH7_OK;` |
|        7 | 11732 |  |
|        - | 11733 | `/*` |
|        - | 11734 | ` * Section:` |
|        - | 11735 | ` *  Class handling functions.` |
|        - | 11736 | ` * Status:` |
|        - | 11737 | ` *    Stable.` |
|        - | 11738 | ` */` |
|        - | 11739 | `/*` |
|        - | 11740 | ` * Extract the top active class. NULL is returned` |
|        - | 11741 | ` * if the class stack is empty.` |
|        - | 11742 | ` */` |
|      986 | 11743 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11744 |  |
|      988 | 11745 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11746 | `	ph7_class **apClass;` |
|      988 | 11747 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11748 | `		/* Empty stack,return NULL */` |
|       15 | 11749 | `		return 0;` |
|        - | 11750 | `	}` |
|        - | 11751 | `	/* Peek the last entry */` |
|      974 | 11752 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      974 | 11753 | `	return apClass[pSet->nUsed - 1];` |
|      495 | 11754 |  |
|        - | 11755 | `/*` |
|        - | 11756 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11757 | ` *   Get the class that declared the currently executing method.` |
|        - | 11758 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11759 | ` *` |
|        - | 11760 | ` * Parameters` |
|        - | 11761 | ` *   pVm: Target VM` |
|        - | 11762 | ` *` |
|        - | 11763 | ` * Return` |
|        - | 11764 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11765 | ` *   - Not executing within a class method` |
|        - | 11766 | ` *` |
|        - | 11767 | ` * Note` |
|        - | 11768 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11769 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11770 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11771 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11772 | ` *   declaring class.` |
|        - | 11773 | ` */` |
|       98 | 11774 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11775 |  |
|      100 | 11776 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11777 | `	ph7_vm_func *pVmFunc;` |
|        - | 11778 |  |
|        - | 11779 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11780 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11781 |  |
|        - | 11782 | `	/* Check if we're in a method context */` |
|      100 | 11783 | `	if( pFrame->pParent ){` |
|       96 | 11784 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11785 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11786 | `			/* Return the declaring class */` |
|       96 | 11787 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11788 | `		}` |
|      ! 0 | 11789 | `	}` |
|        - | 11790 |  |
|        5 | 11791 | `	return 0;` |
|       51 | 11792 |  |
|        - | 11793 |  |
|        - | 11794 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11795 | `/*` |
|        - | 11796 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11797 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11798 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11799 | ` * return value indicates failure.` |
|        - | 11800 | ` */` |
|        - | 11801 | `/*` |
|        - | 11802 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11803 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11804 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11805 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11806 | ` */` |
|     2482 | 11807 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11808 | `	ph7_vm *pVm,` |
|        - | 11809 | `	ph7_class_instance *pThis,` |
|        - | 11810 | `	ph7_class_method *pMethod,` |
|        - | 11811 | `	ph7_value *pResult,` |
|        - | 11812 | `	int nArg,` |
|        - | 11813 | `	ph7_value **apArg,` |
|        - | 11814 | `	VmCallArgMap *pMap` |
|        - | 11815 | `	)` |
|        2 | 11816 |  |
|        - | 11817 | `	ph7_value *aStack;` |
|        - | 11818 | `	VmInstr aInstr[2];` |
|        - | 11819 | `	int iCursor;` |
|        - | 11820 | `	int i;` |
|        - | 11821 | `	sxi32 rc;` |
|     2484 | 11822 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2484 | 11823 | `	if( aStack == 0 ){` |
|      ! 0 | 11824 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11825 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11826 | `		return SXERR_MEM;` |
|        - | 11827 | `	}` |
|     4028 | 11828 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1546 | 11829 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1546 | 11830 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      774 | 11831 | `	}` |
|     2484 | 11832 | `	iCursor = nArg + 1;` |
|     2484 | 11833 | `	if( pThis ){` |
|     2478 | 11834 | `		pThis->iRef++;` |
|     2478 | 11835 | `		aStack[i].x.pOther = pThis;` |
|     2478 | 11836 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1238 | 11837 | `	}` |
|     2484 | 11838 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2484 | 11839 | `	i++;` |
|     2484 | 11840 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2484 | 11841 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2484 | 11842 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2484 | 11843 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2484 | 11844 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2484 | 11845 | `	aInstr[0].iP1 = nArg;` |
|     2484 | 11846 | `	aInstr[0].iP2 = 0;` |
|     2484 | 11847 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2484 | 11848 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2484 | 11849 | `	aInstr[1].iP1 = 1;` |
|     2484 | 11850 | `	aInstr[1].iP2 = 0;` |
|     2484 | 11851 | `	aInstr[1].p3  = 0;` |
|     2484 | 11852 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2484 | 11853 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11854 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11855 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2484 | 11856 | `	return rc;` |
|     1243 | 11857 |  |
|     1924 | 11858 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11859 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11860 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11861 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11862 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11863 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11864 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11865 | `	)` |
|        2 | 11866 |  |
|     1926 | 11867 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11868 |  |
|        - | 11869 | `/*` |
|        - | 11870 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11871 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11872 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11873 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11874 | ` *` |
|        - | 11875 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11876 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11877 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11878 | ` *` |
|        - | 11879 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11880 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11881 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11882 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11883 | ` *` |
|        - | 11884 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11885 | ` */` |
|      174 | 11886 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11887 | `	ph7_vm *pVm,` |
|        - | 11888 | `	ph7_class_instance *pThis,` |
|        - | 11889 | `	int nArg,` |
|        - | 11890 | `	ph7_value **apArg,` |
|        - | 11891 | `	ph7_value *pResult,` |
|        - | 11892 | `	VmCallArgMap *pMap` |
|        - | 11893 | `	)` |
|        2 | 11894 |  |
|        - | 11895 | `	ph7_class_method *pMethod;` |
|      176 | 11896 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11897 | `	if( pMethod == 0 ){` |
|       13 | 11898 | `		if( pResult ){` |
|       13 | 11899 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11900 | `		}` |
|       13 | 11901 | `		return SXERR_INVALID;` |
|        - | 11902 | `	}` |
|      164 | 11903 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11904 |  |
|        - | 11905 | `/*` |
|        - | 11906 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11907 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11908 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11909 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11910 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11911 | ` * lookup or 'goto Exception').` |
|        - | 11912 | ` *` |
|        - | 11913 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11914 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11915 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11916 | ` * reported.` |
|        - | 11917 | ` */` |
|       12 | 11918 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11919 |  |
|        - | 11920 | `	ph7_class *pErrorClass;` |
|       13 | 11921 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11922 | `	ph7_class_method *pCons;` |
|        - | 11923 | `	VmFrame *pThrowFrame;` |
|        - | 11924 | `	char zMsg[256];` |
|        - | 11925 | `	int nMsg;` |
|        - | 11926 | `	sxi32 rc;` |
|       25 | 11927 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11928 | `		"Object of type %.*s is not callable",` |
|       12 | 11929 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11930 | `		pThis->pClass->sName.zString);` |
|       13 | 11931 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11932 | `	if( pErrorClass ){` |
|       13 | 11933 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11934 | `	}` |
|       13 | 11935 | `	if( pErrInst == 0 ){` |
|        - | 11936 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11937 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11938 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11939 | `		 * visible to the user. */` |
|      ! 0 | 11940 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11941 | `		return SXERR_ABORT;` |
|        - | 11942 | `	}` |
|       13 | 11943 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11944 | `	if( pCons ){` |
|        - | 11945 | `		ph7_value sArg;` |
|        - | 11946 | `		ph7_value *apMsg[1];` |
|        - | 11947 | `		SyString sMsgStr;` |
|       13 | 11948 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11949 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11950 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11951 | `		apMsg[0] = &sArg;` |
|       13 | 11952 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11953 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11954 | `	}` |
|        - | 11955 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11956 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11957 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11958 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11959 | `	if( pThrowFrame ){` |
|       13 | 11960 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11961 | `	}` |
|       13 | 11962 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11963 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11964 | `	return rc;` |
|        7 | 11965 |  |
|        - | 11966 | `/*` |
|        - | 11967 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11968 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11969 | ` * in the apArg[] array.` |
|        - | 11970 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11971 | ` * return value indicates failure.` |
|        - | 11972 | ` */` |
|     1212 | 11973 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11974 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11975 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11976 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11977 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11978 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11979 | `	)` |
|        2 | 11980 |  |
|        - | 11981 | `	ph7_value *aStack;` |
|        - | 11982 | `	VmInstr aInstr[2];` |
|        - | 11983 | `	int i;` |
|     1214 | 11984 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11985 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11986 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11987 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 11988 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 11989 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 11990 | `			nArg,apArg,pResult,0);` |
|        - | 11991 | `	}` |
|     1122 | 11992 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11993 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11994 | `		if( pResult ){` |
|        - | 11995 | `			/* Assume a null return value */` |
|      ! 0 | 11996 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11997 | `		}` |
|      511 | 11998 | `		return SXERR_INVALID;` |
|        - | 11999 | `	}` |
|      612 | 12000 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12001 | `		/* Class method */` |
|       15 | 12002 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12003 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12004 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12005 | `		ph7_class *pClass = 0;` |
|        - | 12006 | `		ph7_value *pValue;` |
|        - | 12007 | `		sxi32 rc;` |
|       15 | 12008 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12009 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12010 | `			if( pResult ){` |
|        - | 12011 | `				/* Assume a null return value */` |
|      ! 0 | 12012 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12013 | `			}` |
|      ! 0 | 12014 | `			return SXRET_OK;` |
|        - | 12015 | `		}` |
|        - | 12016 | `		/* Extract the class name or an instance of it */` |
|       15 | 12017 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12018 | `		if( pValue ){` |
|       15 | 12019 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12020 | `		}` |
|       15 | 12021 | `		if( pClass == 0 ){` |
|        - | 12022 | `			/* No such class,return NULL */` |
|      ! 0 | 12023 | `			if( pResult ){` |
|      ! 0 | 12024 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12025 | `			}` |
|      ! 0 | 12026 | `			return SXRET_OK;` |
|        - | 12027 | `		}` |
|       15 | 12028 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12029 | `			/* Point to the class instance */` |
|        9 | 12030 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12031 | `		}` |
|        - | 12032 | `		/* Try to extract the method */` |
|       15 | 12033 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12034 | `		if( pValue ){` |
|       15 | 12035 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12036 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12037 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12038 | `			}` |
|        7 | 12039 | `		}` |
|       15 | 12040 | `		if( pMethod == 0 ){` |
|        - | 12041 | `			/* No such method,return NULL */` |
|      ! 0 | 12042 | `			if( pResult ){` |
|      ! 0 | 12043 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12044 | `			}` |
|      ! 0 | 12045 | `			return SXRET_OK;` |
|        - | 12046 | `		}` |
|        - | 12047 | `		/* Call the class method */` |
|       15 | 12048 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12049 | `		return rc;` |
|        - | 12050 | `	}` |
|        - | 12051 | `	/* Create a new operand stack */` |
|      598 | 12052 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 12053 | `	if( aStack == 0 ){` |
|      ! 0 | 12054 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12055 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12056 | `		if( pResult ){` |
|        - | 12057 | `			/* Assume a null return value */` |
|      ! 0 | 12058 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12059 | `		}` |
|      ! 0 | 12060 | `		return SXERR_MEM;` |
|        - | 12061 | `	}` |
|        - | 12062 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 12063 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 12064 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12065 | `		/*` |
|        - | 12066 | `		 * Symisc eXtension:` |
|        - | 12067 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12068 | `		 */` |
|     1304 | 12069 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 12070 | `	}` |
|        - | 12071 | `	/* Push the function name */` |
|      598 | 12072 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 12073 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12074 | `	/* Emit the CALL istruction */` |
|      598 | 12075 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 12076 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 12077 | `	aInstr[0].iP2 = 0;` |
|      598 | 12078 | `	aInstr[0].p3  = 0;` |
|        - | 12079 | `	/* Emit the DONE instruction */` |
|      598 | 12080 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 12081 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 12082 | `	aInstr[1].iP2 = 0;` |
|      598 | 12083 | `	aInstr[1].p3  = 0;` |
|        - | 12084 | `	/* Execute the function body (if available) */` |
|        - | 12085 | `	{` |
|        - | 12086 | `		sxi32 rcExec;` |
|      598 | 12087 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 12088 | `		/* Clean up the mess left behind */` |
|      598 | 12089 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12090 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 12091 | `		return rcExec;` |
|        - | 12092 | `	}` |
|      608 | 12093 |  |
|        - | 12094 | `/*` |
|        - | 12095 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12096 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12097 | ` * parameter.` |
|        - | 12098 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12099 | ` * return value indicates failure.` |
|        - | 12100 | ` */` |
|      240 | 12101 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12102 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12103 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12104 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12105 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12106 | `	)` |
|        1 | 12107 |  |
|        - | 12108 | `	ph7_value *pArg;` |
|        - | 12109 | `	SySet aArg;` |
|        - | 12110 | `	va_list ap;` |
|        - | 12111 | `	sxi32 rc;` |
|      241 | 12112 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12113 | `	/* Copy arguments one after one */` |
|      241 | 12114 | `	va_start(ap,pResult);` |
|      399 | 12115 | `	for(;;){` |
|      799 | 12116 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12117 | `		if( pArg == 0 ){` |
|      241 | 12118 | `			break;` |
|        - | 12119 | `		}` |
|      559 | 12120 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12121 | `	}` |
|        - | 12122 | `	/* Call the core routine */` |
|      241 | 12123 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12124 | `	/* Cleanup */` |
|      241 | 12125 | `	SySetRelease(&aArg);` |
|      241 | 12126 | `	return rc;` |
|        1 | 12127 |  |
|        - | 12128 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12129 | `/*` |
|        - | 12130 | ` * bool defined(string $name)` |
|        - | 12131 | ` *  Checks whether a given named constant exists.` |
|        - | 12132 | ` * Parameter:` |
|        - | 12133 | ` *  Name of the desired constant.` |
|        - | 12134 | ` * Return` |
|        - | 12135 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12136 | ` */` |
|       26 | 12137 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12138 |  |
|        - | 12139 | `	const char *zName;` |
|       28 | 12140 | `	int nLen = 0;` |
|       28 | 12141 | `	int res = 0;` |
|       28 | 12142 | `	if( nArg < 1 ){` |
|        - | 12143 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12144 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12145 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12146 | `		return SXRET_OK;` |
|        - | 12147 | `	}` |
|        - | 12148 | `	/* Extract constant name */` |
|       28 | 12149 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12150 | `	/* Perform the lookup */` |
|       28 | 12151 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12152 | `		/* Already defined */` |
|       26 | 12153 | `		res = 1;` |
|       12 | 12154 | `	}` |
|       28 | 12155 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12156 | `	return SXRET_OK;` |
|       15 | 12157 |  |
|        - | 12158 | `/*` |
|        - | 12159 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12160 | ` * below.` |
|        - | 12161 | ` */` |
|       16 | 12162 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 12163 |  |
|       18 | 12164 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12165 | `	/* Expand constant value */` |
|       18 | 12166 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       18 | 12167 |  |
|        - | 12168 | `/*` |
|        - | 12169 | ` * bool define(string $constant_name,expression value)` |
|        - | 12170 | ` *  Defines a named constant at runtime.` |
|        - | 12171 | ` * Parameter:` |
|        - | 12172 | ` *  $constant_name` |
|        - | 12173 | ` *   The name of the constant` |
|        - | 12174 | ` *  $value` |
|        - | 12175 | ` *   Constant value` |
|        - | 12176 | ` * Return:` |
|        - | 12177 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12178 | ` */` |
|       14 | 12179 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12180 |  |
|        - | 12181 | `	const char *zName;  /* Constant name */` |
|        - | 12182 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       16 | 12183 | `	int nLen = 0;       /* Name length */` |
|        - | 12184 | `	sxi32 rc;` |
|       16 | 12185 | `	if( nArg < 2 ){` |
|        - | 12186 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12187 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12188 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12189 | `		return SXRET_OK;` |
|        - | 12190 | `	}` |
|       16 | 12191 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12192 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12193 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12194 | `		return SXRET_OK;` |
|        - | 12195 | `	}` |
|        - | 12196 | `	/* Extract constant name */` |
|       16 | 12197 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       16 | 12198 | `	if( nLen < 1 ){` |
|      ! 0 | 12199 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12200 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12201 | `		return SXRET_OK;` |
|        - | 12202 | `	}` |
|        - | 12203 | `	/* Duplicate constant value */` |
|       16 | 12204 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       16 | 12205 | `	if( pValue == 0 ){` |
|      ! 0 | 12206 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12207 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12208 | `		return SXRET_OK;` |
|        - | 12209 | `	}` |
|        - | 12210 | `	/* Initialize the memory object */` |
|       16 | 12211 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12212 | `	/* Register the constant */` |
|       16 | 12213 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       16 | 12214 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12215 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12216 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12217 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12218 | `		return SXRET_OK;` |
|        - | 12219 | `	}` |
|        - | 12220 | `	/* Duplicate constant value */` |
|       16 | 12221 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       16 | 12222 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12223 | `		/* Lower case the constant name */` |
|      ! 0 | 12224 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12225 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12226 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12227 | `				/* UTF-8 stream */` |
|      ! 0 | 12228 | `				zCur++;` |
|      ! 0 | 12229 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12230 | `					zCur++;` |
|      ! 0 | 12231 | `				}` |
|      ! 0 | 12232 | `				continue;` |
|        - | 12233 | `			}` |
|      ! 0 | 12234 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12235 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12236 | `				zCur[0] = (char)c;` |
|      ! 0 | 12237 | `			}` |
|      ! 0 | 12238 | `			zCur++;` |
|      ! 0 | 12239 | `		}` |
|        - | 12240 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12241 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12242 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12243 | `		{` |
|      ! 0 | 12244 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12245 | `			if( pAlias ){` |
|      ! 0 | 12246 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12247 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12248 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12249 | `			}` |
|        - | 12250 | `		}` |
|      ! 0 | 12251 | `	}` |
|        - | 12252 | `	/* All done,return TRUE */` |
|       16 | 12253 | `	ph7_result_bool(pCtx,1);` |
|       16 | 12254 | `	return SXRET_OK;` |
|        9 | 12255 |  |
|        - | 12256 | `/*` |
|        - | 12257 | ` * value constant(string $name)` |
|        - | 12258 | ` *  Returns the value of a constant` |
|        - | 12259 | ` * Parameter` |
|        - | 12260 | ` *  $name` |
|        - | 12261 | ` *    Name of the constant.` |
|        - | 12262 | ` * Return` |
|        - | 12263 | ` *  Constant value or NULL if not defined.` |
|        - | 12264 | ` */` |
|        8 | 12265 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12266 |  |
|        - | 12267 | `	SyHashEntry *pEntry;` |
|        - | 12268 | `	ph7_constant *pCons;` |
|        - | 12269 | `	const char *zName; /* Constant name */` |
|        - | 12270 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12271 | `	int nLen;` |
|       10 | 12272 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12273 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12274 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12275 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12276 | `		return SXRET_OK;` |
|        - | 12277 | `	}` |
|        - | 12278 | `	/* Extract the constant name */` |
|       10 | 12279 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12280 | `	/* Perform the query */` |
|       10 | 12281 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 12282 | `	if( pEntry == 0 ){` |
|        3 | 12283 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12284 | `		ph7_result_null(pCtx);` |
|        3 | 12285 | `		return SXRET_OK;` |
|        - | 12286 | `	}` |
|        8 | 12287 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12288 | `	/* Point to the structure that describe the constant */` |
|        8 | 12289 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12290 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 12291 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12292 | `	/* Return that value */` |
|        8 | 12293 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12294 | `	/* Cleanup */` |
|        8 | 12295 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 12296 | `	return SXRET_OK;` |
|        6 | 12297 |  |
|        - | 12298 | `/*` |
|        - | 12299 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12300 | ` * defined below.` |
|        - | 12301 | ` */` |
|      466 | 12302 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12303 |  |
|      467 | 12304 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12305 | `	ph7_value sName;` |
|        - | 12306 | `	sxi32 rc;` |
|        - | 12307 | `	/* Prepare the constant name for insertion */` |
|      467 | 12308 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12309 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12310 | `	/* Perform the insertion */` |
|      467 | 12311 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12312 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12313 | `	return rc;` |
|        1 | 12314 |  |
|        - | 12315 | `/*` |
|        - | 12316 | ` * array get_defined_constants(void)` |
|        - | 12317 | ` *  Returns an associative array with the names of all defined` |
|        - | 12318 | ` *  constants.` |
|        - | 12319 | ` * Parameters` |
|        - | 12320 | ` *  NONE.` |
|        - | 12321 | ` * Returns` |
|        - | 12322 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12323 | ` */` |
|        2 | 12324 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12325 |  |
|        - | 12326 | `	ph7_value *pArray;` |
|        - | 12327 | `	/* Create the array first*/` |
|        3 | 12328 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12329 | `	if( pArray == 0 ){` |
|      ! 0 | 12330 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12331 | `		SXUNUSED(apArg);` |
|        - | 12332 | `		/* Return NULL */` |
|      ! 0 | 12333 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12334 | `		return SXRET_OK;` |
|        - | 12335 | `	}` |
|        - | 12336 | `	/* Fill the array with the defined constants */` |
|        3 | 12337 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12338 | `	/* Return the created array */` |
|        3 | 12339 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12340 | `	return SXRET_OK;` |
|        2 | 12341 |  |
|        - | 12342 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12343 | `/*` |
|        - | 12344 | ` * Section:` |
|        - | 12345 | ` *  Random numbers/string generators.` |
|        - | 12346 | ` * Status:` |
|        - | 12347 | ` *    Stable.` |
|        - | 12348 | ` */` |
|        - | 12349 | `/*` |
|        - | 12350 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12351 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12352 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12353 | ` */` |
|     2903 | 12354 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 12355 |  |
|        - | 12356 | `	sxu32 iNum;` |
|     2905 | 12357 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2905 | 12358 | `	return iNum;` |
|        2 | 12359 |  |
|        - | 12360 | `/*` |
|        - | 12361 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12362 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12363 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12364 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12365 | ` */` |
|   236484 | 12366 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12367 |  |
|        - | 12368 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12369 | `	int i;` |
|        - | 12370 | `	/* Generate a binary string first */` |
|   236486 | 12371 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12372 | `	/* Turn the binary string into english based alphabet */` |
|  2601494 | 12373 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2365010 | 12374 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1182506 | 12375 | `	 }` |
|   236486 | 12376 |  |
|        - | 12377 | `/*` |
|        - | 12378 | ` * int rand()` |
|        - | 12379 | ` * int mt_rand()` |
|        - | 12380 | ` * int rand(int $min,int $max)` |
|        - | 12381 | ` * int mt_rand(int $min,int $max)` |
|        - | 12382 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12383 | ` * Parameter` |
|        - | 12384 | ` *  $min` |
|        - | 12385 | ` *    The lowest value to return (default: 0)` |
|        - | 12386 | ` *  $max` |
|        - | 12387 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12388 | ` * Return` |
|        - | 12389 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12390 | ` * Note:` |
|        - | 12391 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12392 | ` *  by te SQLite3 library.` |
|        - | 12393 | ` */` |
|       20 | 12394 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12395 |  |
|        - | 12396 | `	sxu32 iNum;` |
|        - | 12397 | `	/* Generate the random number */` |
|       21 | 12398 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12399 | `	if( nArg > 1 ){` |
|        - | 12400 | `		sxu32 iMin,iMax;` |
|        3 | 12401 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12402 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12403 | `		if( iMin < iMax ){` |
|        3 | 12404 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12405 | `			if( iDiv > 0 ){` |
|        3 | 12406 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12407 | `			}` |
|        1 | 12408 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12409 | `			iNum %= iMax;` |
|      ! 0 | 12410 | `		}` |
|        1 | 12411 | `	}` |
|        - | 12412 | `	/* Return the number */` |
|       21 | 12413 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12414 | `	return SXRET_OK;` |
|        1 | 12415 |  |
|        - | 12416 | `/*` |
|        - | 12417 | ` * int getrandmax(void)` |
|        - | 12418 | ` * int mt_getrandmax(void)` |
|        - | 12419 | ` * int rc4_getrandmax(void)` |
|        - | 12420 | ` *   Show largest possible random value` |
|        - | 12421 | ` * Return` |
|        - | 12422 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12423 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12424 | ` * Note:` |
|        - | 12425 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12426 | ` *  by te SQLite3 library.` |
|        - | 12427 | ` */` |
|        4 | 12428 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12429 |  |
|        2 | 12430 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12431 | `	SXUNUSED(apArg);` |
|        5 | 12432 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12433 | `	return SXRET_OK;` |
|        1 | 12434 |  |
|        - | 12435 | `/*` |
|        - | 12436 | ` * string rand_str()` |
|        - | 12437 | ` * string rand_str(int $len)` |
|        - | 12438 | ` *  Generate a random string (English alphabet).` |
|        - | 12439 | ` * Parameter` |
|        - | 12440 | ` *  $len` |
|        - | 12441 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12442 | ` * Return` |
|        - | 12443 | ` *   A pseudo random string.` |
|        - | 12444 | ` * Note:` |
|        - | 12445 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12446 | ` *  by te SQLite3 library.` |
|        - | 12447 | ` *  This function is a symisc extension.` |
|        - | 12448 | ` */` |
|      120 | 12449 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12450 |  |
|        - | 12451 | `	char zString[1024];` |
|      122 | 12452 | `	int iLen = 0x10;` |
|      122 | 12453 | `	if( nArg > 0 ){` |
|        - | 12454 | `		/* Get the desired length */` |
|      122 | 12455 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12456 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12457 | `			/* Default length */` |
|        3 | 12458 | `			iLen = 0x10;` |
|        1 | 12459 | `		}` |
|       60 | 12460 | `	}` |
|        - | 12461 | `	/* Generate the random string */` |
|      122 | 12462 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12463 | `	/* Return the generated string */` |
|      122 | 12464 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12465 | `	return SXRET_OK;` |
|        2 | 12466 |  |
|        - | 12467 | `/*` |
|        - | 12468 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12469 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12470 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12471 | ` */` |
|      488 | 12472 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12473 |  |
|      488 | 12474 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12475 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12476 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12477 | `			"TypeError",` |
|        - | 12478 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12479 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12480 | `			ph7_type_name(pArg)` |
|        - | 12481 | `			);` |
|        - | 12482 | `	}` |
|      483 | 12483 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12484 | `		int len;` |
|        9 | 12485 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12486 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12487 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12488 | `				"TypeError",` |
|        - | 12489 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12490 | `				zFunc,iArgPos,zParamName` |
|        - | 12491 | `				);` |
|        - | 12492 | `		}` |
|        2 | 12493 | `	}` |
|      479 | 12494 | `	return SXRET_OK;` |
|      245 | 12495 |  |
|        - | 12496 | `/*` |
|        - | 12497 | ` * int random_int(int $min, int $max)` |
|        - | 12498 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12499 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12500 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12501 | ` *  power-of-two mask covering the range.` |
|        - | 12502 | ` */` |
|      242 | 12503 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12504 |  |
|        - | 12505 | `	sxi64 iMin,iMax;` |
|        - | 12506 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12507 | `	unsigned int nAttempt;` |
|        - | 12508 | `	int rc;` |
|      243 | 12509 | `	if( nArg != 2 ){` |
|       10 | 12510 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12511 | `			"ArgumentCountError",` |
|        - | 12512 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12513 | `			nArg` |
|        - | 12514 | `			);` |
|        - | 12515 | `	}` |
|      237 | 12516 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12517 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12518 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12519 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12520 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12521 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12522 | `	if( iMin > iMax ){` |
|        3 | 12523 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12524 | `			"ValueError",` |
|        - | 12525 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12526 | `			);` |
|        - | 12527 | `	}` |
|      229 | 12528 | `	if( iMin == iMax ){` |
|        5 | 12529 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12530 | `		return SXRET_OK;` |
|        - | 12531 | `	}` |
|      225 | 12532 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12533 | `	uMask = uRange;` |
|      225 | 12534 | `	uMask \|= uMask >> 1;` |
|      225 | 12535 | `	uMask \|= uMask >> 2;` |
|      225 | 12536 | `	uMask \|= uMask >> 4;` |
|      225 | 12537 | `	uMask \|= uMask >> 8;` |
|      225 | 12538 | `	uMask \|= uMask >> 16;` |
|      225 | 12539 | `	uMask \|= uMask >> 32;` |
|      225 | 12540 | `	uResult = 0;` |
|      341 | 12541 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12542 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12543 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12544 | `		 * and the low-half mask would always read 0). */` |
|        - | 12545 | `		sxu64 uDraw;` |
|      341 | 12546 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12547 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12548 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12549 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12550 | `				"Exception",` |
|        - | 12551 | `				"Cannot gather sufficient random data"` |
|        - | 12552 | `				);` |
|        - | 12553 | `		}` |
|      341 | 12554 | `		uDraw &= uMask;` |
|      341 | 12555 | `		if( uDraw <= uRange ){` |
|      225 | 12556 | `			uResult = uDraw;` |
|      225 | 12557 | `			break;` |
|        - | 12558 | `		}` |
|       44 | 12559 | `	}` |
|      225 | 12560 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12561 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12562 | `			"Exception",` |
|        - | 12563 | `			"Cannot gather sufficient random data"` |
|        - | 12564 | `			);` |
|        - | 12565 | `	}` |
|      225 | 12566 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12567 | `	return SXRET_OK;` |
|      122 | 12568 |  |
|        - | 12569 | `/*` |
|        - | 12570 | ` * string random_bytes(int $length)` |
|        - | 12571 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12572 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12573 | ` */` |
|       24 | 12574 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12575 |  |
|        - | 12576 | `	sxi64 iLen;` |
|        - | 12577 | `	unsigned char zStack[256];` |
|        - | 12578 | `	void *pBuf;` |
|        - | 12579 | `	int rc;` |
|       25 | 12580 | `	int bHeap = 0;` |
|       25 | 12581 | `	if( nArg != 1 ){` |
|        7 | 12582 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12583 | `			"ArgumentCountError",` |
|        - | 12584 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12585 | `			nArg` |
|        - | 12586 | `			);` |
|        - | 12587 | `	}` |
|       21 | 12588 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12589 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12590 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12591 | `	if( iLen < 1 ){` |
|        5 | 12592 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12593 | `			"ValueError",` |
|        - | 12594 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12595 | `			);` |
|        - | 12596 | `	}` |
|        - | 12597 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12598 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12599 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12600 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12601 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12602 | `			"ValueError",` |
|        - | 12603 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12604 | `			);` |
|        - | 12605 | `	}` |
|       13 | 12606 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12607 | `		pBuf = zStack;` |
|        7 | 12608 | `	}else{` |
|      ! 0 | 12609 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12610 | `		if( pBuf == 0 ){` |
|      ! 0 | 12611 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12612 | `				"Exception",` |
|        - | 12613 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12614 | `				iLen` |
|        - | 12615 | `				);` |
|        - | 12616 | `		}` |
|      ! 0 | 12617 | `		bHeap = 1;` |
|        - | 12618 | `	}` |
|       13 | 12619 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12620 | `		if( bHeap ){` |
|      ! 0 | 12621 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12622 | `		}` |
|      ! 0 | 12623 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12624 | `			"Exception",` |
|        - | 12625 | `			"Cannot gather sufficient random data"` |
|        - | 12626 | `			);` |
|        - | 12627 | `	}` |
|       13 | 12628 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12629 | `	if( bHeap ){` |
|      ! 0 | 12630 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12631 | `	}` |
|       13 | 12632 | `	return SXRET_OK;` |
|       13 | 12633 |  |
|        - | 12634 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12635 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12636 | `/* Unique ID private data */` |
|        - | 12637 | `struct unique_id_data` |
|        - | 12638 |  |
|        - | 12639 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12640 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12641 | `};` |
|        - | 12642 | `/*` |
|        - | 12643 | ` * Binary to hex consumer callback.` |
|        - | 12644 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12645 | ` * defined below.` |
|        - | 12646 | ` */` |
|      192 | 12647 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12648 |  |
|      193 | 12649 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12650 | `	sxu32 nBuflen;` |
|        - | 12651 | `	/* Extract result buffer length */` |
|      193 | 12652 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12653 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12654 | `			/*` |
|        - | 12655 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12656 | `			 * string will be 13 characters long` |
|        - | 12657 | `			 */` |
|       25 | 12658 | `		return SXERR_ABORT;` |
|        - | 12659 | `	}` |
|      169 | 12660 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12661 | `		return SXERR_ABORT;` |
|        - | 12662 | `	}` |
|        - | 12663 | `	/* Safely Consume the hex stream */` |
|      169 | 12664 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12665 | `	return SXRET_OK;` |
|       97 | 12666 |  |
|        - | 12667 | `/*` |
|        - | 12668 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12669 | ` *  Generate a unique ID` |
|        - | 12670 | ` * Parameter` |
|        - | 12671 | ` * $prefix` |
|        - | 12672 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12673 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12674 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12675 | ` * $more_entropy` |
|        - | 12676 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12677 | ` *  that the result will be unique.` |
|        - | 12678 | ` * Return` |
|        - | 12679 | ` *  Returns the unique identifier, as a string.` |
|        - | 12680 | ` */` |
|       24 | 12681 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12682 |  |
|        - | 12683 | `	struct unique_id_data sUniq;` |
|        - | 12684 | `	unsigned char zDigest[20];` |
|       25 | 12685 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12686 | `	const char *zPrefix;` |
|        - | 12687 | `	SHA1Context sCtx;` |
|        - | 12688 | `	char zRandom[7];` |
|        - | 12689 | `	int nPrefix;` |
|        - | 12690 | `	int entropy;` |
|        - | 12691 | `	/* Generate a random string first */` |
|       25 | 12692 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12693 | `	/* Initialize fields */` |
|       25 | 12694 | `	zPrefix = 0;` |
|       25 | 12695 | `	nPrefix = 0;` |
|       25 | 12696 | `	entropy = 0;` |
|       25 | 12697 | `	if( nArg > 0 ){` |
|        - | 12698 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12699 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12700 | `		if( nArg > 1 ){` |
|      ! 0 | 12701 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12702 | `		}` |
|      ! 0 | 12703 | `	}` |
|       25 | 12704 | `	SHA1Init(&sCtx);` |
|        - | 12705 | `	/* Generate the random ID */` |
|       25 | 12706 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12707 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12708 | `	}` |
|        - | 12709 | `	/* Append the random ID */` |
|       25 | 12710 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12711 | `	/* Append the random string */` |
|       25 | 12712 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12713 | `	/* Increment the number */` |
|       25 | 12714 | `	pVm->unique_id++;` |
|       25 | 12715 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12716 | `	/* Hexify the digest */` |
|       25 | 12717 | `	sUniq.pCtx = pCtx;` |
|       25 | 12718 | `	sUniq.entropy = entropy;` |
|       25 | 12719 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12720 | `	/* All done */` |
|       25 | 12721 | `	return PH7_OK;` |
|        1 | 12722 |  |
|        - | 12723 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12724 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12725 | `/*` |
|        - | 12726 | ` * Section:` |
|        - | 12727 | ` *  Language construct implementation as foreign functions.` |
|        - | 12728 | ` * Status:` |
|        - | 12729 | ` *    Stable.` |
|        - | 12730 | ` */` |
|        - | 12731 | `/*` |
|        - | 12732 | ` * void echo($string...)` |
|        - | 12733 | ` *  Output one or more messages.` |
|        - | 12734 | ` * Parameters` |
|        - | 12735 | ` *  $string` |
|        - | 12736 | ` *   Message to output.` |
|        - | 12737 | ` * Return` |
|        - | 12738 | ` *  NULL.` |
|        - | 12739 | ` */` |
|      ! 0 | 12740 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12741 |  |
|        - | 12742 | `	const char *zData;` |
|      ! 0 | 12743 | `	int nDataLen = 0;` |
|        - | 12744 | `	ph7_vm *pVm;` |
|        - | 12745 | `	int i,rc;` |
|        - | 12746 | `	/* Point to the target VM */` |
|      ! 0 | 12747 | `	pVm = pCtx->pVm;` |
|        - | 12748 | `	/* Output */` |
|      ! 0 | 12749 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12750 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12751 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12752 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12753 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12754 | `			if( rc == SXERR_ABORT ){` |
|        - | 12755 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12756 | `				return PH7_ABORT;` |
|        - | 12757 | `			}` |
|      ! 0 | 12758 | `		}` |
|      ! 0 | 12759 | `	}` |
|      ! 0 | 12760 | `	return SXRET_OK;` |
|      ! 0 | 12761 |  |
|        - | 12762 | `/*` |
|        - | 12763 | ` * int print($string...)` |
|        - | 12764 | ` *  Output one or more messages.` |
|        - | 12765 | ` * Parameters` |
|        - | 12766 | ` *  $string` |
|        - | 12767 | ` *   Message to output.` |
|        - | 12768 | ` * Return` |
|        - | 12769 | ` *  1 always.` |
|        - | 12770 | ` */` |
|        2 | 12771 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12772 |  |
|        - | 12773 | `	const char *zData;` |
|        3 | 12774 | `	int nDataLen = 0;` |
|        - | 12775 | `	ph7_vm *pVm;` |
|        - | 12776 | `	int i,rc;` |
|        - | 12777 | `	/* Point to the target VM */` |
|        3 | 12778 | `	pVm = pCtx->pVm;` |
|        - | 12779 | `	/* Output */` |
|        5 | 12780 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12781 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12782 | `		if( nDataLen > 0 ){` |
|        3 | 12783 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12784 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12785 | `			if( rc == SXERR_ABORT ){` |
|        - | 12786 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12787 | `				return PH7_ABORT;` |
|        - | 12788 | `			}` |
|        1 | 12789 | `		}` |
|        2 | 12790 | `	}` |
|        - | 12791 | `	/* Return 1 */` |
|        3 | 12792 | `	ph7_result_int(pCtx,1);` |
|        3 | 12793 | `	return SXRET_OK;` |
|        2 | 12794 |  |
|        - | 12795 | `/*` |
|        - | 12796 | ` * void exit(string $msg)` |
|        - | 12797 | ` * void exit(int $status)` |
|        - | 12798 | ` * void die(string $ms)` |
|        - | 12799 | ` * void die(int $status)` |
|        - | 12800 | ` *   Output a message and terminate program execution.` |
|        - | 12801 | ` * Parameter` |
|        - | 12802 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12803 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12804 | ` *  and not printed` |
|        - | 12805 | ` * Return` |
|        - | 12806 | ` *  NULL` |
|        - | 12807 | ` */` |
|      ! 0 | 12808 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12809 |  |
|      ! 0 | 12810 | `	if( nArg > 0 ){` |
|      ! 0 | 12811 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12812 | `			const char *zData;` |
|      ! 0 | 12813 | `			int iLen = 0;` |
|        - | 12814 | `			/* Print exit message */` |
|      ! 0 | 12815 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12816 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12817 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12818 | `			sxi32 iExitStatus;` |
|        - | 12819 | `			/* Record exit status code */` |
|      ! 0 | 12820 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12821 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12822 | `		}` |
|      ! 0 | 12823 | `	}` |
|        - | 12824 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12825 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12826 | `	 */` |
|      ! 0 | 12827 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12828 | `	return PH7_ABORT;` |
|      ! 0 | 12829 |  |
|        - | 12830 | `/*` |
|        - | 12831 | ` * bool isset($var,...)` |
|        - | 12832 | ` *  Finds out whether a variable is set.` |
|        - | 12833 | ` * Parameters` |
|        - | 12834 | ` *  One or more variable to check.` |
|        - | 12835 | ` * Return` |
|        - | 12836 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12837 | ` */` |
|    92744 | 12838 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12839 |  |
|        - | 12840 | `	ph7_value *pObj;` |
|    92746 | 12841 | `	int res = 0;` |
|        - | 12842 | `	int i;` |
|    92746 | 12843 | `	if( nArg < 1 ){` |
|        - | 12844 | `		/* Missing arguments,return false */` |
|      ! 0 | 12845 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12846 | `		return SXRET_OK;` |
|        - | 12847 | `	}` |
|        - | 12848 | `	/* Iterate over available arguments */` |
|   121226 | 12849 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92756 | 12850 | `		pObj = apArg[i];` |
|    92756 | 12851 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12852 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12853 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12854 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63300 | 12855 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12856 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12857 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12858 | `			}` |
|    31649 | 12859 | `		}` |
|    92756 | 12860 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92756 | 12861 | `		if( !res ){` |
|        - | 12862 | `			/* Variable not set,return FALSE */` |
|    64276 | 12863 | `			ph7_result_bool(pCtx,0);` |
|    64276 | 12864 | `			return SXRET_OK;` |
|        - | 12865 | `		}` |
|    14242 | 12866 | `	}` |
|        - | 12867 | `	/* All given variable are set,return TRUE */` |
|    28472 | 12868 | `	ph7_result_bool(pCtx,1);` |
|    28472 | 12869 | `	return SXRET_OK;` |
|    46374 | 12870 |  |
|        - | 12871 | `/*` |
|        - | 12872 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12873 | ` * frame,the reference table and discard it's contents.` |
|        - | 12874 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12875 | ` */` |
|  3161648 | 12876 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12877 |  |
|        - | 12878 | `	ph7_value *pObj;` |
|        - | 12879 | `	VmRefObj *pRef;` |
|  3161650 | 12880 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3161650 | 12881 | `	if( pObj ){` |
|        - | 12882 | `		/* Release the object */` |
|  3161650 | 12883 | `		PH7_MemObjRelease(pObj);` |
|  1580824 | 12884 | `	}` |
|        - | 12885 | `	/* Remove old reference links */` |
|  3161650 | 12886 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3161650 | 12887 | `	if( pRef ){` |
|  3161644 | 12888 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12889 | `		/* Unlink from the reference table */` |
|  3161644 | 12890 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3161644 | 12891 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12892 | `			VmSlot sFree;` |
|        - | 12893 | `			/* Restore to the free list */` |
|  3161636 | 12894 | `			sFree.nIdx = nObjIdx;` |
|  3161636 | 12895 | `			sFree.pUserData = 0;` |
|  3161636 | 12896 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1580817 | 12897 | `		}` |
|  1580821 | 12898 | `	}` |
|  3161650 | 12899 | `	return SXRET_OK;` |
|        2 | 12900 |  |
|        - | 12901 | `/*` |
|        - | 12902 | ` * void unset($var,...)` |
|        - | 12903 | ` *   Unset one or more given variable.` |
|        - | 12904 | ` * Parameters` |
|        - | 12905 | ` *  One or more variable to unset.` |
|        - | 12906 | ` * Return` |
|        - | 12907 | ` *  Nothing.` |
|        - | 12908 | ` */` |
|     7554 | 12909 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12910 |  |
|        - | 12911 | `	ph7_value *pObj;` |
|        - | 12912 | `	ph7_vm *pVm;` |
|        - | 12913 | `	int i;` |
|        - | 12914 | `	/* Point to the target VM */` |
|     7556 | 12915 | `	pVm = pCtx->pVm;` |
|        - | 12916 | `	/* Iterate and unset */` |
|    15110 | 12917 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7556 | 12918 | `		pObj = apArg[i];` |
|     7556 | 12919 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      840 | 12920 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12921 | `				/* Throw an error */` |
|      ! 0 | 12922 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12923 | `			}` |
|      421 | 12924 | `		}else{` |
|     6718 | 12925 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12926 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6718 | 12927 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6712 | 12928 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3355 | 12929 | `			}` |
|        - | 12930 | `		}` |
|     3779 | 12931 | `	}` |
|     7556 | 12932 | `	return SXRET_OK;` |
|        2 | 12933 |  |
|        - | 12934 | `/*` |
|        - | 12935 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12936 | ` */` |
|      116 | 12937 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12938 |  |
|      117 | 12939 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12940 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12941 | `	ph7_value *pObj;` |
|        - | 12942 | `	sxu32 nIdx;` |
|        - | 12943 | `	/* Extract the memory object */` |
|      117 | 12944 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12945 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12946 | `	if( pObj ){` |
|      117 | 12947 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12948 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12949 | `				SyString sName;` |
|        - | 12950 | `				ph7_value sKey;` |
|        - | 12951 | `				/* Perform the insertion */` |
|      115 | 12952 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12953 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12954 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12955 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12956 | `			}` |
|       57 | 12957 | `		}` |
|       58 | 12958 | `	}` |
|      117 | 12959 | `	return SXRET_OK;` |
|        1 | 12960 |  |
|        - | 12961 | `/*` |
|        - | 12962 | ` * array get_defined_vars(void)` |
|        - | 12963 | ` *  Returns an array of all defined variables.` |
|        - | 12964 | ` * Parameter` |
|        - | 12965 | ` *  None` |
|        - | 12966 | ` * Return` |
|        - | 12967 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12968 | ` */` |
|        2 | 12969 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12970 |  |
|        3 | 12971 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12972 | `	ph7_value *pArray;` |
|        - | 12973 | `	/* Create a new array */` |
|        3 | 12974 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12975 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12976 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12977 | `		SXUNUSED(apArg);` |
|        - | 12978 | `		/* Return NULL */` |
|      ! 0 | 12979 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12980 | `		return SXRET_OK;` |
|        - | 12981 | `	}` |
|        - | 12982 | `	/* Superglobals first */` |
|        3 | 12983 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12984 | `	/* Then variable defined in the current frame */` |
|        3 | 12985 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12986 | `	/* Finally,return the created array */` |
|        3 | 12987 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12988 | `	return SXRET_OK;` |
|        2 | 12989 |  |
|        - | 12990 | `/*` |
|        - | 12991 | ` * bool gettype($var)` |
|        - | 12992 | ` *  Get the type of a variable` |
|        - | 12993 | ` * Parameters` |
|        - | 12994 | ` *   $var` |
|        - | 12995 | ` *    The variable being type checked.` |
|        - | 12996 | ` * Return` |
|        - | 12997 | ` *   String representation of the given variable type.` |
|        - | 12998 | ` */` |
|       32 | 12999 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13000 |  |
|       34 | 13001 | `	const char *zType = "Empty";` |
|       34 | 13002 | `	if( nArg > 0 ){` |
|       34 | 13003 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 13004 | `	}` |
|        - | 13005 | `	/* Return the variable type */` |
|       34 | 13006 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 13007 | `	return SXRET_OK;` |
|        2 | 13008 |  |
|        - | 13009 | `/*` |
|        - | 13010 | ` * string get_resource_type(resource $handle)` |
|        - | 13011 | ` *  This function gets the type of the given resource.` |
|        - | 13012 | ` * Parameters` |
|        - | 13013 | ` *  $handle` |
|        - | 13014 | ` *  The evaluated resource handle.` |
|        - | 13015 | ` * Return` |
|        - | 13016 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13017 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13018 | ` *  the return value will be the string Unknown.` |
|        - | 13019 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13020 | ` *  is not a resource.` |
|        - | 13021 | ` */` |
|        2 | 13022 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13023 |  |
|        3 | 13024 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13025 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13026 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13027 | `		return PH7_OK;` |
|        - | 13028 | `	}` |
|        3 | 13029 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13030 | `	return SXRET_OK;` |
|        2 | 13031 |  |
|        - | 13032 | `/*` |
|        - | 13033 | ` * void var_dump(expression,....)` |
|        - | 13034 | ` *   var_dump � Dumps information about a variable` |
|        - | 13035 | ` * Parameters` |
|        - | 13036 | ` *   One or more expression to dump.` |
|        - | 13037 | ` * Returns` |
|        - | 13038 | ` *  Nothing.` |
|        - | 13039 | ` */` |
|      218 | 13040 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13041 |  |
|        - | 13042 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13043 | `	int i;` |
|      220 | 13044 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13045 | `	/* Dump one or more expressions */` |
|      444 | 13046 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 13047 | `		ph7_value *pObj = apArg[i];` |
|        - | 13048 | `		/* Reset the working buffer */` |
|      226 | 13049 | `		SyBlobReset(&sDump);` |
|        - | 13050 | `		/* Dump the given expression */` |
|      226 | 13051 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13052 | `		/* Output */` |
|      226 | 13053 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 13054 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13055 | `		}` |
|      114 | 13056 | `	}` |
|        - | 13057 | `	/* Release the working buffer */` |
|      220 | 13058 | `	SyBlobRelease(&sDump);` |
|      220 | 13059 | `	return SXRET_OK;` |
|        2 | 13060 |  |
|        - | 13061 | `/*` |
|        - | 13062 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13063 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13064 | ` * Parameters` |
|        - | 13065 | ` *   expression: Expression to dump` |
|        - | 13066 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13067 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13068 | ` *            print_r() will return the information rather than print it.` |
|        - | 13069 | ` * Return` |
|        - | 13070 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13071 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13072 | ` */` |
|       16 | 13073 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13074 |  |
|       17 | 13075 | `	int ret_string = 0;` |
|        - | 13076 | `	SyBlob sDump;` |
|       17 | 13077 | `	if( nArg < 1 ){` |
|        - | 13078 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13079 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13080 | `		return SXRET_OK;` |
|        - | 13081 | `	}` |
|       17 | 13082 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13083 | `	if ( nArg > 1 ){` |
|        - | 13084 | `		/* Where to redirect output */` |
|       11 | 13085 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13086 | `	}` |
|        - | 13087 | `	/* Generate dump */` |
|       17 | 13088 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13089 | `	if( !ret_string ){` |
|        - | 13090 | `		/* Output dump */` |
|        7 | 13091 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13092 | `		/* Return true */` |
|        7 | 13093 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13094 | `	}else{` |
|        - | 13095 | `		/* Generated dump as return value */` |
|       11 | 13096 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13097 | `	}` |
|        - | 13098 | `	/* Release the working buffer */` |
|       17 | 13099 | `	SyBlobRelease(&sDump);` |
|       17 | 13100 | `	return SXRET_OK;` |
|        9 | 13101 |  |
|        - | 13102 | `/*` |
|        - | 13103 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13104 | ` * Same job as print_r. (see coment above)` |
|        - | 13105 | ` */` |
|        2 | 13106 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13107 |  |
|        3 | 13108 | `	int ret_string = 0;` |
|        - | 13109 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13110 | `	if( nArg < 1 ){` |
|        - | 13111 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13112 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13113 | `		return SXRET_OK;` |
|        - | 13114 | `	}` |
|        3 | 13115 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13116 | `	if ( nArg > 1 ){` |
|        - | 13117 | `		/* Where to redirect output */` |
|        3 | 13118 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13119 | `	}` |
|        - | 13120 | `	/* Generate dump */` |
|        3 | 13121 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13122 | `	if( !ret_string ){` |
|        - | 13123 | `		/* Output dump */` |
|      ! 0 | 13124 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13125 | `		/* Return NULL */` |
|      ! 0 | 13126 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13127 | `	}else{` |
|        - | 13128 | `		/* Generated dump as return value */` |
|        3 | 13129 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13130 | `	}` |
|        - | 13131 | `	/* Release the working buffer */` |
|        3 | 13132 | `	SyBlobRelease(&sDump);` |
|        3 | 13133 | `	return SXRET_OK;` |
|        2 | 13134 |  |
|        - | 13135 | `/*` |
|        - | 13136 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13137 | ` *  Set/get the various assert flags.` |
|        - | 13138 | ` * Parameter` |
|        - | 13139 | ` * $what` |
|        - | 13140 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13141 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13142 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13143 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13144 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13145 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13146 | ` * $value` |
|        - | 13147 | ` *   An optional new value for the option.` |
|        - | 13148 | ` * Return` |
|        - | 13149 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13150 | ` */` |
|       28 | 13151 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13152 |  |
|       30 | 13153 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13154 | `	int iOption;` |
|        - | 13155 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 13156 | `	if( nArg < 1 ){` |
|        3 | 13157 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13158 | `			"ArgumentCountError",` |
|        - | 13159 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13160 | `			);` |
|        - | 13161 | `	}` |
|        - | 13162 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13163 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 13164 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13165 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13166 | `			"TypeError",` |
|        - | 13167 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13168 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13169 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13170 | `			);` |
|        - | 13171 | `	}` |
|       28 | 13172 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13173 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13174 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13175 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 13176 | `	switch( iOption ){` |
|        5 | 13177 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13178 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13179 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13180 | `		if( nArg > 1 ){` |
|        5 | 13181 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13182 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13183 | `			}else{` |
|        3 | 13184 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13185 | `			}` |
|        2 | 13186 | `		}` |
|       12 | 13187 | `		break;` |
|        1 | 13188 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13189 | `		/* Return old callback or null */` |
|        3 | 13190 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13191 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13192 | `		}else{` |
|        3 | 13193 | `			ph7_result_null(pCtx);` |
|        - | 13194 | `		}` |
|        3 | 13195 | `		if( nArg > 1 ){` |
|      ! 0 | 13196 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13197 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13198 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13199 | `			}else{` |
|      ! 0 | 13200 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13201 | `			}` |
|      ! 0 | 13202 | `		}` |
|        3 | 13203 | `		break;` |
|        5 | 13204 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13205 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13206 | `		if( nArg > 1 ){` |
|        5 | 13207 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13208 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13209 | `			}else{` |
|        3 | 13210 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13211 | `			}` |
|        2 | 13212 | `		}` |
|       11 | 13213 | `		break;` |
|      ! 0 | 13214 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13215 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13216 | `		break;` |
|        1 | 13217 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13218 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13219 | `		break;` |
|      ! 0 | 13220 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13221 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13222 | `		break;` |
|        1 | 13223 | `	default:` |
|        - | 13224 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13225 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13226 | `			"ValueError",` |
|        - | 13227 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13228 | `			);` |
|        - | 13229 | `	}` |
|       26 | 13230 | `	return PH7_OK;` |
|       16 | 13231 |  |
|        - | 13232 | `/*` |
|        - | 13233 | ` * bool assert(mixed $assertion)` |
|        - | 13234 | ` *  Checks if assertion is FALSE.` |
|        - | 13235 | ` * Parameter` |
|        - | 13236 | ` *  $assertion` |
|        - | 13237 | ` *    The assertion to test.` |
|        - | 13238 | ` * Return` |
|        - | 13239 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13240 | ` */` |
|       24 | 13241 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13242 |  |
|       26 | 13243 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13244 | `	int iFlags,iResult;` |
|        - | 13245 | `	const char *zDesc;` |
|        - | 13246 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 13247 | `	if( nArg < 1 ){` |
|        3 | 13248 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13249 | `			"ArgumentCountError",` |
|        - | 13250 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13251 | `			);` |
|        - | 13252 | `	}` |
|       24 | 13253 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 13254 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13255 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13256 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13257 | `		return PH7_OK;` |
|        - | 13258 | `	}` |
|        - | 13259 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 13260 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 13261 | `	if( !iResult ){` |
|        - | 13262 | `		/* Assertion failed */` |
|        - | 13263 | `		/* Extract optional description */` |
|       13 | 13264 | `		zDesc = 0;` |
|       13 | 13265 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13266 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13267 | `		}` |
|       13 | 13268 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13269 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13270 | `			ph7_value sFile,sLine;` |
|        - | 13271 | `			ph7_value *apCbArg[3];` |
|        - | 13272 | `			SyString *pFile;` |
|        - | 13273 | `			/* Extract the processed script */` |
|      ! 0 | 13274 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13275 | `			if( pFile == 0 ){` |
|      ! 0 | 13276 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13277 | `			}` |
|        - | 13278 | `			/* Invoke the callback */` |
|      ! 0 | 13279 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13280 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13281 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13282 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13283 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13284 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13285 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13286 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13287 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13288 | `		}` |
|       13 | 13289 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13290 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13291 | `			return PH7_ABORT;` |
|        - | 13292 | `		}` |
|        - | 13293 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 13294 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13295 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13296 | `				"AssertionError",` |
|        - | 13297 | `				"%s",` |
|        1 | 13298 | `				zDesc` |
|        - | 13299 | `				);` |
|      ! 0 | 13300 | `		}else{` |
|       11 | 13301 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13302 | `				"AssertionError",` |
|        - | 13303 | `				"assert(false)"` |
|        - | 13304 | `				);` |
|        - | 13305 | `		}` |
|        - | 13306 | `	}` |
|        - | 13307 | `	/* Assertion passed */` |
|       11 | 13308 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13309 | `	return PH7_OK;` |
|       14 | 13310 |  |
|        - | 13311 | `/*` |
|        - | 13312 | ` * Section:` |
|        - | 13313 | ` *  Error reporting functions.` |
|        - | 13314 | ` * Status:` |
|        - | 13315 | ` *    Stable.` |
|        - | 13316 | ` */` |
|        - | 13317 | `/*` |
|        - | 13318 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13319 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13320 | ` * Parameters` |
|        - | 13321 | ` *  $error_msg` |
|        - | 13322 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13323 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13324 | ` * $error_type` |
|        - | 13325 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13326 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13327 | ` * Return` |
|        - | 13328 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13329 | ` */` |
|       12 | 13330 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13331 |  |
|       14 | 13332 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 13333 | `	int rc = PH7_OK;` |
|       14 | 13334 | `	if( nArg > 0 ){` |
|        - | 13335 | `		const char *zErr;` |
|        - | 13336 | `		int nLen;` |
|        - | 13337 | `		/* Extract the error message */` |
|       12 | 13338 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 13339 | `		if( nArg > 1 ){` |
|        - | 13340 | `			/* Extract the error type */` |
|       12 | 13341 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 13342 | `			switch( nErr ){` |
|        1 | 13343 | `			case 1:   /* E_ERROR */` |
|        - | 13344 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13345 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13346 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13347 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13348 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13349 | `				break;` |
|        1 | 13350 | `			case 2:   /* E_WARNING */` |
|        - | 13351 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13352 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13353 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13354 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13355 | `				break;` |
|        3 | 13356 | `			default:` |
|        8 | 13357 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13358 | `				break;` |
|        - | 13359 | `			}` |
|        5 | 13360 | `		}` |
|        - | 13361 | `		/* Report error */` |
|       12 | 13362 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13363 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13364 | `			return rc;` |
|        - | 13365 | `		}` |
|        - | 13366 | `		/* Return true */` |
|       12 | 13367 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13368 | `	}else{` |
|        - | 13369 | `		/* Missing arguments,return FALSE */` |
|        3 | 13370 | `		ph7_result_bool(pCtx,0);` |
|        - | 13371 | `	}` |
|       14 | 13372 | `	return rc;` |
|        8 | 13373 |  |
|        - | 13374 | `/*` |
|        - | 13375 | ` * int error_reporting([int $level])` |
|        - | 13376 | ` *  Sets which PHP errors are reported.` |
|        - | 13377 | ` * Parameters` |
|        - | 13378 | ` *  $level` |
|        - | 13379 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13380 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13381 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13382 | ` *   levels will not always behave as expected.` |
|        - | 13383 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13384 | ` *   in the predefined constants.` |
|        - | 13385 | ` * Return` |
|        - | 13386 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13387 | ` *   parameter is given.` |
|        - | 13388 | ` */` |
|       32 | 13389 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13390 |  |
|       34 | 13391 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13392 | `	int nOld;` |
|        - | 13393 | `	/* Extract the old reporting level */` |
|       34 | 13394 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13395 | `	if( nArg > 0 ){` |
|        - | 13396 | `		int nNew;` |
|        - | 13397 | `		/* Extract the desired error reporting level */` |
|       28 | 13398 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13399 | `		if( !nNew ){` |
|        - | 13400 | `			/* Do not report errors at all */` |
|        5 | 13401 | `			pVm->bErrReport = 0;` |
|        3 | 13402 | `		}else{` |
|        - | 13403 | `			/* Report all errors */` |
|       24 | 13404 | `			pVm->bErrReport = 1;` |
|        - | 13405 | `		}` |
|       13 | 13406 | `	}` |
|        - | 13407 | `	/* Return the old level */` |
|       34 | 13408 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13409 | `	return PH7_OK;` |
|        2 | 13410 |  |
|        - | 13411 | `/*` |
|        - | 13412 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13413 | ` *  Send an error message somewhere.` |
|        - | 13414 | ` * Parameter` |
|        - | 13415 | ` *  $message` |
|        - | 13416 | ` *   The error message that should be logged.` |
|        - | 13417 | ` *  $message_type` |
|        - | 13418 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13419 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13420 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13421 | ` *       This is the default option.` |
|        - | 13422 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13423 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13424 | ` *    2  No longer an option.` |
|        - | 13425 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13426 | ` *       to the end of the message string.` |
|        - | 13427 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13428 | ` *  $destination` |
|        - | 13429 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13430 | ` *  $extra_headers` |
|        - | 13431 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13432 | ` * Return` |
|        - | 13433 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13434 | ` * NOTE:` |
|        - | 13435 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13436 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13437 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13438 | ` *  Otherwise this function is no-op.` |
|        - | 13439 | ` */` |
|        4 | 13440 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13441 |  |
|        - | 13442 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13443 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13444 | `	int iType = 0;` |
|        5 | 13445 | `	if( nArg < 1 ){` |
|        - | 13446 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13447 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13448 | `		return PH7_OK;` |
|        - | 13449 | `	}` |
|        5 | 13450 | `	if( pVm->xErrLog  ){` |
|        - | 13451 | `		/* Invoke the user callback */` |
|      ! 0 | 13452 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13453 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13454 | `		if( nArg > 1 ){` |
|      ! 0 | 13455 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13456 | `			if( nArg > 2 ){` |
|      ! 0 | 13457 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13458 | `				if( nArg > 3 ){` |
|      ! 0 | 13459 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13460 | `				}` |
|      ! 0 | 13461 | `			}` |
|      ! 0 | 13462 | `		}` |
|      ! 0 | 13463 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13464 | `	}` |
|        - | 13465 | `	/* Retun TRUE */` |
|        5 | 13466 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13467 | `	return PH7_OK;` |
|        3 | 13468 |  |
|        - | 13469 | `/*` |
|        - | 13470 | ` * bool restore_exception_handler(void)` |
|        - | 13471 | ` *  Restores the previously defined exception handler function.` |
|        - | 13472 | ` * Parameter` |
|        - | 13473 | ` *  None` |
|        - | 13474 | ` * Return` |
|        - | 13475 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13476 | ` */` |
|        4 | 13477 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13478 |  |
|        5 | 13479 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13480 | `	ph7_value *pOld,*pNew;` |
|        - | 13481 | `	/* Point to the old and the new handler */` |
|        5 | 13482 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13483 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13484 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13485 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13486 | `		SXUNUSED(apArg);` |
|        - | 13487 | `		/* No installed handler,return FALSE */` |
|        5 | 13488 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13489 | `		return PH7_OK;` |
|        - | 13490 | `	}` |
|        - | 13491 | `	/* Copy the old handler */` |
|      ! 0 | 13492 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13493 | `	PH7_MemObjRelease(pOld);` |
|        - | 13494 | `	/* Return TRUE */` |
|      ! 0 | 13495 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13496 | `	return PH7_OK;` |
|        3 | 13497 |  |
|        - | 13498 | `/*` |
|        - | 13499 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13500 | ` *  Sets a user-defined exception handler function.` |
|        - | 13501 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13502 | ` * NOTE` |
|        - | 13503 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13504 | ` *  the satndard PHP engine.` |
|        - | 13505 | ` * Parameters` |
|        - | 13506 | ` *  $exception_handler` |
|        - | 13507 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13508 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13509 | ` *   that was thrown.` |
|        - | 13510 | ` *  Note:` |
|        - | 13511 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13512 | ` * Return` |
|        - | 13513 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13514 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13515 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13516 | ` */` |
|        4 | 13517 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13518 |  |
|        6 | 13519 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13520 | `	ph7_value *pOld,*pNew;` |
|        - | 13521 | `	/* Point to the old and the new handler */` |
|        6 | 13522 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13523 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13524 | `	/* Return the old handler */` |
|        6 | 13525 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13526 | `	if( nArg > 0 ){` |
|        6 | 13527 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13528 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13529 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13530 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13531 | `		}else{` |
|        6 | 13532 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13533 | `			/* Install the new handler */` |
|        6 | 13534 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13535 | `		}` |
|        2 | 13536 | `	}` |
|        6 | 13537 | `	return PH7_OK;` |
|        2 | 13538 |  |
|        - | 13539 | `/*` |
|        - | 13540 | ` * bool restore_error_handler(void)` |
|        - | 13541 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13542 | ` * Parameters:` |
|        - | 13543 | ` *  None.` |
|        - | 13544 | ` * Return` |
|        - | 13545 | ` *  Always TRUE.` |
|        - | 13546 | ` */` |
|        6 | 13547 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13548 |  |
|        7 | 13549 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13550 | `	ph7_value *pOld,*pNew;` |
|        - | 13551 | `	/* Point to the old and the new handler */` |
|        7 | 13552 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13553 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13554 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13555 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13556 | `		SXUNUSED(apArg);` |
|        - | 13557 | `		/* No installed callback,return FALSE */` |
|        7 | 13558 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13559 | `		return PH7_OK;` |
|        - | 13560 | `	}` |
|        - | 13561 | `	/* Copy the old callback */` |
|      ! 0 | 13562 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13563 | `	PH7_MemObjRelease(pOld);` |
|        - | 13564 | `	/* Return TRUE */` |
|      ! 0 | 13565 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13566 | `	return PH7_OK;` |
|        4 | 13567 |  |
|        - | 13568 | `/*` |
|        - | 13569 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13570 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13571 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13572 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13573 | ` *  Sets a user-defined error handler function.` |
|        - | 13574 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13575 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13576 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13577 | ` *  conditions (using trigger_error()).` |
|        - | 13578 | ` * Parameters` |
|        - | 13579 | ` *  $error_handler` |
|        - | 13580 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13581 | ` *   describing the error.` |
|        - | 13582 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13583 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13584 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13585 | ` *   The function can be shown as:` |
|        - | 13586 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13587 | ` *     errno` |
|        - | 13588 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13589 | ` *   errstr` |
|        - | 13590 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13591 | ` *   errfile` |
|        - | 13592 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13593 | ` *     was raised in, as a string.` |
|        - | 13594 | ` *  Note:` |
|        - | 13595 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13596 | ` * Return` |
|        - | 13597 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13598 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13599 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13600 | ` */` |
|    10860 | 13601 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13602 |  |
|    10862 | 13603 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13604 | `	ph7_value *pOld,*pNew;` |
|        - | 13605 | `	/* Point to the old and the new handler */` |
|    10862 | 13606 | `	pOld = &pVm->aErrCB[0];` |
|    10862 | 13607 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13608 | `	/* Return the old handler */` |
|    10862 | 13609 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10862 | 13610 | `	if( nArg > 0 ){` |
|    10862 | 13611 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13612 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5425 | 13613 | `			PH7_MemObjRelease(pNew);` |
|     5425 | 13614 | `			ph7_result_bool(pCtx,1);` |
|     2713 | 13615 | `		}else{` |
|     5438 | 13616 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13617 | `			/* Install the new handler */` |
|     5438 | 13618 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13619 | `		}` |
|     5430 | 13620 | `	}` |
|    10862 | 13621 | `	return PH7_OK;` |
|        2 | 13622 |  |
|        - | 13623 | `/*` |
|        - | 13624 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13625 | ` *  Generates a backtrace.` |
|        - | 13626 | ` * Paramaeter` |
|        - | 13627 | ` *  $options` |
|        - | 13628 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13629 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13630 | ` *   all the function/method arguments, to save memory.` |
|        - | 13631 | ` * $limit` |
|        - | 13632 | ` *   (Not Used)` |
|        - | 13633 | ` * Return` |
|        - | 13634 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13635 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13636 | ` *          Name        Type      Description` |
|        - | 13637 | ` *          ------      ------     -----------` |
|        - | 13638 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13639 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13640 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13641 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13642 | ` *          object      object    The current object.` |
|        - | 13643 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13644 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13645 | ` */` |
|      928 | 13646 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13647 |  |
|      930 | 13648 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13649 | `	ph7_value *pArray;` |
|        - | 13650 | `	ph7_class *pClass;` |
|        - | 13651 | `	ph7_value *pValue;` |
|        - | 13652 | `	SyString *pFile;` |
|        - | 13653 | `	/* Create a new array */` |
|      930 | 13654 | `	pArray = ph7_context_new_array(pCtx);` |
|      930 | 13655 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      930 | 13656 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13657 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13658 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13659 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13660 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13661 | `		SXUNUSED(apArg);` |
|      ! 0 | 13662 | `		return PH7_OK;` |
|        - | 13663 | `	}` |
|        - | 13664 | `	/* Dump running function name and it's arguments  */` |
|      930 | 13665 | `	if( pVm->pFrame->pParent ){` |
|      930 | 13666 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13667 | `		ph7_vm_func *pFunc;` |
|        - | 13668 | `		ph7_value *pArg;` |
|      930 | 13669 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      930 | 13670 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      930 | 13671 | `		if( pFrame->pParent && pFunc ){` |
|      930 | 13672 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      930 | 13673 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      930 | 13674 | `			ph7_value_reset_string_cursor(pValue);` |
|      464 | 13675 | `		}` |
|        - | 13676 | `		/* Function arguments */` |
|      930 | 13677 | `		pArg = ph7_context_new_array(pCtx);` |
|      930 | 13678 | `		if( pArg  ){` |
|        - | 13679 | `			ph7_value *pObj;` |
|        - | 13680 | `			VmSlot *aSlot;` |
|        - | 13681 | `			sxu32 n;` |
|        - | 13682 | `			/* Start filling the array with the given arguments */` |
|      930 | 13683 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3718 | 13684 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2790 | 13685 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2790 | 13686 | `				if( pObj ){` |
|     2790 | 13687 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1394 | 13688 | `				}` |
|     1396 | 13689 | `			}` |
|        - | 13690 | `			/* Save the array */` |
|      930 | 13691 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      464 | 13692 | `		}` |
|      464 | 13693 | `	}` |
|      930 | 13694 | `	ph7_value_int(pValue,1);` |
|        - | 13695 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13696 | `	 * line numbers at run-time. )` |
|        - | 13697 | `	 */` |
|      930 | 13698 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13699 | `	/* Current processed script */` |
|      930 | 13700 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      930 | 13701 | `	if( pFile ){` |
|      930 | 13702 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      930 | 13703 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      930 | 13704 | `		ph7_value_reset_string_cursor(pValue);` |
|      464 | 13705 | `	}` |
|        - | 13706 | `	/* Top class */` |
|      930 | 13707 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      930 | 13708 | `	if( pClass ){` |
|      926 | 13709 | `		ph7_value_reset_string_cursor(pValue);` |
|      926 | 13710 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      926 | 13711 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      462 | 13712 | `	}` |
|        - | 13713 | `	/* Return the freshly created array */` |
|      930 | 13714 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13715 | `	/*` |
|        - | 13716 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13717 | `	 * as soon we return from this function.` |
|        - | 13718 | `	 */` |
|      930 | 13719 | `	return PH7_OK;` |
|      466 | 13720 |  |
|        - | 13721 | `/*` |
|        - | 13722 | ` * Generate a small backtrace.` |
|        - | 13723 | ` * Store the generated dump in the given BLOB` |
|        - | 13724 | ` */` |
|        4 | 13725 | `static int VmMiniBacktrace(` |
|        - | 13726 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13727 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13728 | `	)` |
|        1 | 13729 |  |
|        5 | 13730 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13731 | `	ph7_vm_func *pFunc;` |
|        - | 13732 | `	ph7_class *pClass;` |
|        - | 13733 | `	SyString *pFile;` |
|        - | 13734 | `	/* Called function */` |
|        5 | 13735 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13736 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13737 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13738 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13739 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13740 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13741 | `	}else{` |
|      ! 0 | 13742 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13743 | `	}` |
|        5 | 13744 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13745 | `	/* Current processed script */` |
|        5 | 13746 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13747 | `	if( pFile ){` |
|        5 | 13748 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13749 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13750 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13751 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13752 | `	}` |
|        - | 13753 | `	/* Top class */` |
|        5 | 13754 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13755 | `	if( pClass ){` |
|      ! 0 | 13756 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13757 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13758 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13759 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13760 | `	}` |
|        5 | 13761 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13762 | `	/* All done */` |
|        5 | 13763 | `	return SXRET_OK;` |
|        1 | 13764 |  |
|        - | 13765 | `/*` |
|        - | 13766 | ` * void debug_print_backtrace()` |
|        - | 13767 | ` *  Prints a backtrace` |
|        - | 13768 | ` * Parameters` |
|        - | 13769 | ` * None` |
|        - | 13770 | ` * Return` |
|        - | 13771 | ` * NULL` |
|        - | 13772 | ` */` |
|        2 | 13773 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13774 |  |
|        3 | 13775 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13776 | `	SyBlob sDump;` |
|        3 | 13777 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13778 | `	/* Generate the backtrace */` |
|        3 | 13779 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13780 | `	/* Output backtrace */` |
|        3 | 13781 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13782 | `	/* All done,cleanup */` |
|        3 | 13783 | `	SyBlobRelease(&sDump);` |
|        1 | 13784 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13785 | `	SXUNUSED(apArg);` |
|        3 | 13786 | `	return PH7_OK;` |
|        1 | 13787 |  |
|        - | 13788 | `/*` |
|        - | 13789 | ` * string debug_string_backtrace()` |
|        - | 13790 | ` *  Generate a backtrace` |
|        - | 13791 | ` * Parameters` |
|        - | 13792 | ` * None` |
|        - | 13793 | ` * Return` |
|        - | 13794 | ` *  A mini backtrace().` |
|        - | 13795 | ` * Note that this is a symisc extension.` |
|        - | 13796 | ` */` |
|        2 | 13797 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13798 |  |
|        3 | 13799 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13800 | `	SyBlob sDump;` |
|        3 | 13801 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13802 | `	/* Generate the backtrace */` |
|        3 | 13803 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13804 | `	/* Return the backtrace */` |
|        3 | 13805 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13806 | `	/* All done,cleanup */` |
|        3 | 13807 | `	SyBlobRelease(&sDump);` |
|        1 | 13808 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13809 | `	SXUNUSED(apArg);` |
|        3 | 13810 | `	return PH7_OK;` |
|        1 | 13811 |  |
|        - | 13812 | `/*` |
|        - | 13813 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13814 | ` * exception is triggered.` |
|        - | 13815 | ` */` |
|      512 | 13816 | `static sxi32 VmUncaughtException(` |
|        - | 13817 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13818 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13819 | `	)` |
|        1 | 13820 |  |
|        - | 13821 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13822 | `	int nArg = 1;` |
|        - | 13823 | `	sxi32 rc;` |
|      513 | 13824 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13825 | `		/* Nesting limit reached */` |
|      ! 0 | 13826 | `		return SXRET_OK;` |
|        - | 13827 | `	}` |
|        - | 13828 | `	/* Call any exception handler if available */` |
|      513 | 13829 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13830 | `	if( pThis ){` |
|        - | 13831 | `		/* Load the exception instance */` |
|      513 | 13832 | `		sArg.x.pOther = pThis;` |
|      513 | 13833 | `		pThis->iRef++;` |
|      513 | 13834 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13835 | `	}else{` |
|      ! 0 | 13836 | `		nArg = 0;` |
|        - | 13837 | `	}` |
|      513 | 13838 | `	apArg[0] = &sArg;` |
|        - | 13839 | `	/* Call the exception handler if available */` |
|      513 | 13840 | `	pVm->nExceptDepth++;` |
|      513 | 13841 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13842 | `	pVm->nExceptDepth--;` |
|      513 | 13843 | `	if( rc != SXRET_OK ){` |
|        - | 13844 | `		SyBlob sMsgBuf;` |
|      511 | 13845 | `		const char *zClass = "Exception";` |
|      511 | 13846 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13847 | `		const char *zMsg;` |
|        - | 13848 | `		sxu32 nMsg;` |
|        - | 13849 | `		const char *zFuncName;` |
|        - | 13850 | `		int nFuncLen;` |
|      511 | 13851 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13852 | `		if( pThis ){` |
|        - | 13853 | `			ph7_class_method *pGetMessage;` |
|        - | 13854 | `			ph7_value sMsg;` |
|        - | 13855 | `			const char *zTmp;` |
|        - | 13856 | `			int nTmp;` |
|      511 | 13857 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13858 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13859 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13860 | `			if( pGetMessage ){` |
|      511 | 13861 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13862 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13863 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13864 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13865 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13866 | `					}` |
|      255 | 13867 | `				}` |
|      511 | 13868 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13869 | `			}` |
|      255 | 13870 | `		}` |
|      511 | 13871 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13872 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13873 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13874 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13875 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13876 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13877 | `		rc = SXERR_ABORT;` |
|      255 | 13878 | `	}` |
|      513 | 13879 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13880 | `	return rc;` |
|      257 | 13881 |  |
|        - | 13882 | `/*` |
|        - | 13883 | ` * Throw a user exception.` |
|        - | 13884 | ` *` |
|        - | 13885 | ` * Exception dispatch follows this sequence:` |
|        - | 13886 | ` *` |
|        - | 13887 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13888 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13889 | ` *` |
|        - | 13890 | ` * 2. If NO catch matches:` |
|        - | 13891 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13892 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13893 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13894 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13895 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13896 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13897 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13898 | ` *` |
|        - | 13899 | ` * 3. If a catch DOES match:` |
|        - | 13900 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13901 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13902 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13903 | ` *       finally block.` |
|        - | 13904 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13905 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13906 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13907 | ` *       in pPendingException (step 2c).` |
|        - | 13908 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13909 | ` *    d. Run finally (if present).` |
|        - | 13910 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13911 | ` *       that handlers are restored and finally has run.` |
|        - | 13912 | ` */` |
|      858 | 13913 | `static sxi32 VmThrowException(` |
|        - | 13914 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13915 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13916 | `	)` |
|        2 | 13917 |  |
|        - | 13918 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13919 | `	ph7_exception **apException;` |
|        - | 13920 | `	ph7_exception *pException;` |
|        - | 13921 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13922 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13923 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      860 | 13924 | `	VmCoalesceDisarm(pVm);` |
|        - | 13925 | `	/* Point to the stack of loaded exceptions */` |
|      860 | 13926 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      860 | 13927 | `	pException = 0;` |
|      860 | 13928 | `	pCatch = 0;` |
|      860 | 13929 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13930 | `		ph7_exception_block *aCatch;` |
|        - | 13931 | `		ph7_class *pClass;` |
|        - | 13932 | `		SyString *aNames;` |
|        - | 13933 | `		sxu32 nNames;` |
|        - | 13934 | `		int matched;` |
|        - | 13935 | `		sxu32 j,k;` |
|        - | 13936 | `		/* Locate the appropriate block to execute */` |
|      340 | 13937 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      340 | 13938 | `		(void)SySetPop(&pVm->aException);` |
|      340 | 13939 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      348 | 13940 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13941 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      346 | 13942 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      346 | 13943 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      346 | 13944 | `			matched = 0;` |
|      372 | 13945 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13946 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13947 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13948 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      364 | 13949 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      364 | 13950 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13951 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13952 | `					continue;` |
|        - | 13953 | `				}` |
|      364 | 13954 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      338 | 13955 | `					matched = 1;` |
|      338 | 13956 | `					break;` |
|        - | 13957 | `				}` |
|       14 | 13958 | `			}` |
|      346 | 13959 | `			if( matched ){` |
|        - | 13960 | `				/* Catch block found,break immediately */` |
|      338 | 13961 | `				pCatch = &aCatch[j];` |
|      338 | 13962 | `				break;` |
|        - | 13963 | `			}` |
|        5 | 13964 | `		}` |
|      169 | 13965 | `	}` |
|        - | 13966 | `	/* Execute the cached block if available */` |
|      860 | 13967 | `	if( pCatch == 0 ){` |
|        - | 13968 | `		sxi32 rc;` |
|        - | 13969 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13970 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13971 | `			pException->iFinallyDone = 1;` |
|        3 | 13972 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13973 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13974 | `				return SXERR_ABORT;` |
|        - | 13975 | `			}` |
|        1 | 13976 | `		}` |
|        - | 13977 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13978 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13979 | `			/* Re-throw to the outer handler */` |
|        3 | 13980 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13981 | `		}` |
|        - | 13982 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13983 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13984 | `		 * exception instead of reporting it uncaught.` |
|        - | 13985 | `		 */` |
|      522 | 13986 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13987 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13988 | `			 * by looking for a catch frame on the stack.` |
|        - | 13989 | `			 */` |
|      522 | 13990 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13991 | `			int inCatch = 0;` |
|     1050 | 13992 | `			while( pF ){` |
|      538 | 13993 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13994 | `					inCatch = 1;` |
|        9 | 13995 | `					break;` |
|        - | 13996 | `				}` |
|      529 | 13997 | `				pF = pF->pParent;` |
|        1 | 13998 | `			}` |
|      522 | 13999 | `			if( inCatch ){` |
|        - | 14000 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 14001 | `				pThis->iRef++;` |
|        9 | 14002 | `				pVm->pPendingException = pThis;` |
|        9 | 14003 | `				return SXRET_OK;` |
|        - | 14004 | `			}` |
|      256 | 14005 | `		}` |
|        - | 14006 | `		/* Truly uncaught */` |
|      513 | 14007 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 14008 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14009 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14010 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14011 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14012 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14013 | `			}` |
|      ! 0 | 14014 | `		}` |
|      513 | 14015 | `		return rc;` |
|      ! 0 | 14016 | `	}else{` |
|      338 | 14017 | `		VmFrame *pFrame = pVm->pFrame;` |
|      338 | 14018 | `		ph7_exception **apSaved = 0;` |
|        - | 14019 | `		sxu32 nSavedCount;` |
|        - | 14020 | `		sxi32 rc;` |
|      338 | 14021 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      338 | 14022 | `		if( pException->pFrame == pFrame ){` |
|      238 | 14023 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      118 | 14024 | `		}` |
|        - | 14025 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14026 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14027 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14028 | `		 */` |
|      338 | 14029 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      338 | 14030 | `		if( nSavedCount > 0 ){` |
|       16 | 14031 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 14032 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14033 | `			if( apSaved ){` |
|       16 | 14034 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 14035 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14036 | `				SySetReset(&pVm->aException);` |
|        5 | 14037 | `			}` |
|        5 | 14038 | `		}` |
|        - | 14039 | `		/* Create a private frame first */` |
|      338 | 14040 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      338 | 14041 | `		if( rc == SXRET_OK ){` |
|      338 | 14042 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      338 | 14043 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      338 | 14044 | `			if( pObj ){` |
|      338 | 14045 | `				pThis->iRef++;` |
|      338 | 14046 | `				pObj->x.pOther = pThis;` |
|      338 | 14047 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      168 | 14048 | `			}` |
|        - | 14049 | `			/* Execute the catch block */` |
|      338 | 14050 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 14051 | `			/* Leave the frame */` |
|      338 | 14052 | `			VmLeaveFrame(&(*pVm));` |
|      168 | 14053 | `		}` |
|        - | 14054 | `		/* Restore the outer exception handlers */` |
|      338 | 14055 | `		if( apSaved ){` |
|        - | 14056 | `			sxu32 k;` |
|        - | 14057 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14058 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14059 | `			 * Restore the original outer entries.` |
|        - | 14060 | `			 */` |
|       11 | 14061 | `			SySetReset(&pVm->aException);` |
|       21 | 14062 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 14063 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 14064 | `			}` |
|       11 | 14065 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 14066 | `		}` |
|        - | 14067 | `		/* Execute the finally block after catch */` |
|      338 | 14068 | `		if( pException->iHasFinally ){` |
|       16 | 14069 | `			pException->iFinallyDone = 1;` |
|        - | 14070 | `			{` |
|       16 | 14071 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 14072 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14073 | `					return SXERR_ABORT;` |
|        - | 14074 | `				}` |
|        - | 14075 | `			}` |
|        7 | 14076 | `		}` |
|      338 | 14077 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14078 | `			return SXERR_ABORT;` |
|        - | 14079 | `		}` |
|        - | 14080 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14081 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14082 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 14083 | `		 */` |
|      338 | 14084 | `		if( pVm->pPendingException ){` |
|        9 | 14085 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14086 | `			pVm->pPendingException = 0;` |
|        9 | 14087 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14088 | `		}` |
|        - | 14089 | `	}` |
|        - | 14090 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14091 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14092 | `	 */` |
|      330 | 14093 | `	return SXRET_OK;` |
|      431 | 14094 |  |
|        - | 14095 | `/*` |
|        - | 14096 | ` * Section:` |
|        - | 14097 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14098 | ` * Status:` |
|        - | 14099 | ` *    Stable.` |
|        - | 14100 | ` */` |
|        - | 14101 | `/*` |
|        - | 14102 | ` * string ph7version(void)` |
|        - | 14103 | ` *  Returns the running version of the PH7 version.` |
|        - | 14104 | ` * Parameters` |
|        - | 14105 | ` *  None` |
|        - | 14106 | ` * Return` |
|        - | 14107 | ` * Current PH7 version.` |
|        - | 14108 | ` */` |
|        2 | 14109 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14110 |  |
|        1 | 14111 | `	SXUNUSED(nArg);` |
|        1 | 14112 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14113 | `	/* Current engine version */` |
|        3 | 14114 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14115 | `	return PH7_OK;` |
|        1 | 14116 |  |
|        - | 14117 | `/*` |
|        - | 14118 | ` * string phpversion([ string $extension ])` |
|        - | 14119 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14120 | ` * Parameters` |
|        - | 14121 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14122 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14123 | ` * Return` |
|        - | 14124 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14125 | ` */` |
|        4 | 14126 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14127 |  |
|        2 | 14128 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14129 | `	if( nArg > 0 ){` |
|      ! 0 | 14130 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14131 | `		return PH7_OK;` |
|        - | 14132 | `	}` |
|        5 | 14133 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14134 | `	return PH7_OK;` |
|        3 | 14135 |  |
|        - | 14136 | `/*` |
|        - | 14137 | ` * string php_sapi_name(void)` |
|        - | 14138 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14139 | ` * Parameters` |
|        - | 14140 | ` *  None` |
|        - | 14141 | ` * Return` |
|        - | 14142 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14143 | ` */` |
|        2 | 14144 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14145 |  |
|        3 | 14146 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14147 | `	SXUNUSED(nArg);` |
|        1 | 14148 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14149 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14150 | `	return PH7_OK;` |
|        1 | 14151 |  |
|        - | 14152 | `/*` |
|        - | 14153 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14154 | ` */` |
|        - | 14155 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14156 | ` "<html><head>"\` |
|        - | 14157 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14158 | ` "<style type=\"text/css\">"\` |
|        - | 14159 | ` "div {"\` |
|        - | 14160 | `     "border: 1px solid #cccccc;"\` |
|        - | 14161 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14162 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14163 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14164 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14165 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14166 | `     "-o-border-radius: 10px;"\` |
|        - | 14167 | `     "border-radius: 10px;"\` |
|        - | 14168 | `     "padding-left: 2em;"\` |
|        - | 14169 | `     "background-color: white;"\` |
|        - | 14170 | `     "margin-left: auto;"\` |
|        - | 14171 | `     "font-family: verdana;"\` |
|        - | 14172 | `     "padding-right: 2em;"\` |
|        - | 14173 | `     "margin-right: auto;"\` |
|        - | 14174 | `     "}"\` |
|        - | 14175 | `     "body {"\` |
|        - | 14176 | `     "padding: 0.2em;"\` |
|        - | 14177 | `     "font-style: normal;"\` |
|        - | 14178 | `     "font-size: medium;"\` |
|        - | 14179 | `     "background-color: #f2f2f2;"\` |
|        - | 14180 | `     "}"\` |
|        - | 14181 | `     "hr {"\` |
|        - | 14182 | `     "border-style: solid none none;"\` |
|        - | 14183 | `     "border-width: 1px medium medium;"\` |
|        - | 14184 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14185 | `     "height: 1px;"\` |
|        - | 14186 | `     "}"\` |
|        - | 14187 | `     "a {"\` |
|        - | 14188 | `     "color: #3366cc;"\` |
|        - | 14189 | `     "text-decoration: none;"\` |
|        - | 14190 | `     "}"\` |
|        - | 14191 | `     "a:hover {"\` |
|        - | 14192 | `     "color: #999999;"\` |
|        - | 14193 | `     "}"\` |
|        - | 14194 | `     "a:active {"\` |
|        - | 14195 | `     "color: #663399;"\` |
|        - | 14196 | `     "}"\` |
|        - | 14197 | `     "h1 {"\` |
|        - | 14198 | `     "margin: 0;"\` |
|        - | 14199 | `     "padding: 0;"\` |
|        - | 14200 | `     "font-family: Verdana;"\` |
|        - | 14201 | `     "font-weight: bold;"\` |
|        - | 14202 | `     "font-style: normal;"\` |
|        - | 14203 | `     "font-size: medium;"\` |
|        - | 14204 | `     "text-transform: capitalize;"\` |
|        - | 14205 | `     "color: #0a328c;"\` |
|        - | 14206 | `     "}"\` |
|        - | 14207 | `     "p {"\` |
|        - | 14208 | `     "margin: 0 auto;"\` |
|        - | 14209 | `     "font-size: medium;"\` |
|        - | 14210 | `     "font-style: normal;"\` |
|        - | 14211 | `     "font-family: verdana;"\` |
|        - | 14212 | `     "}"\` |
|        - | 14213 | `"</style></head><body>"\` |
|        - | 14214 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14215 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14216 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14217 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14218 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14219 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14220 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14221 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14222 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14223 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14224 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14225 |  |
|        - | 14226 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14227 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14228 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14229 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14230 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14231 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14232 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14233 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14234 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14235 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14236 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14237 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14238 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14239 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14240 |  |
|        - | 14241 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14242 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14243 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14244 | `"&nbsp;*<br>"\` |
|        - | 14245 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14246 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14247 | `"&nbsp;* are met:<br>"\` |
|        - | 14248 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14249 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14250 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14251 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14252 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14253 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14254 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14255 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14256 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14257 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14258 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14259 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14260 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14261 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14262 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14263 | `"&nbsp;*<br>"\` |
|        - | 14264 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14265 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14266 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14267 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14268 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14269 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14270 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14271 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14272 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14273 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14274 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14275 | `"&nbsp;*/<br>"\` |
|        - | 14276 | `"</span></small></small></p>"\` |
|        - | 14277 | `"</div></body></html>"` |
|        - | 14278 | `/*` |
|        - | 14279 | ` * bool ph7credits(void)` |
|        - | 14280 | ` * bool ph7info(void)` |
|        - | 14281 | ` * bool ph7copyright(void)` |
|        - | 14282 | ` *  Prints out the credits for PH7 engine` |
|        - | 14283 | ` * Parameters` |
|        - | 14284 | ` *  None` |
|        - | 14285 | ` * Return` |
|        - | 14286 | ` *  Always TRUE` |
|        - | 14287 | ` */` |
|        2 | 14288 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14289 |  |
|        3 | 14290 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14291 | `	/* Expand the HTML page above*/` |
|        3 | 14292 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14293 | `	ph7_context_output_format(` |
|        1 | 14294 | `		pCtx,` |
|        - | 14295 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14296 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14297 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14298 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14299 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14300 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14301 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14302 | `#ifdef __WINNT__` |
|        - | 14303 | `		"Windows NT"` |
|        - | 14304 | `#elif defined(__UNIXES__)` |
|        - | 14305 | `		"UNIX-Like"` |
|        - | 14306 | `#else` |
|        - | 14307 | `		"Other OS"` |
|        - | 14308 | `#endif` |
|        - | 14309 | `		);` |
|        3 | 14310 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14311 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14312 | `	SXUNUSED(apArg);` |
|        - | 14313 | `	/* Return TRUE */` |
|        - | 14314 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14315 | `	return PH7_OK;` |
|        1 | 14316 |  |
|        - | 14317 | `/*` |
|        - | 14318 | ` * Section:` |
|        - | 14319 | ` *    URL related routines.` |
|        - | 14320 | ` * Status:` |
|        - | 14321 | ` *    Stable.` |
|        - | 14322 | ` */` |
|        - | 14323 | `/*` |
|        - | 14324 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14325 | ` *  Parse a URL and return its fields.` |
|        - | 14326 | ` * Parameters` |
|        - | 14327 | ` *  $url` |
|        - | 14328 | ` *   The URL to parse.` |
|        - | 14329 | ` * $component` |
|        - | 14330 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14331 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14332 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14333 | ` *  in which case the return value will be an integer).` |
|        - | 14334 | ` * Return` |
|        - | 14335 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14336 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14337 | ` *  this array are:` |
|        - | 14338 | ` *   scheme - e.g. http` |
|        - | 14339 | ` *   host` |
|        - | 14340 | ` *   port` |
|        - | 14341 | ` *   user` |
|        - | 14342 | ` *   pass` |
|        - | 14343 | ` *   path` |
|        - | 14344 | ` *   query - after the question mark ?` |
|        - | 14345 | ` *   fragment - after the hashmark #` |
|        - | 14346 | ` * Note:` |
|        - | 14347 | ` *  FALSE is returned on failure.` |
|        - | 14348 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14349 | ` *  with the standard PHP engine.` |
|        - | 14350 | ` */` |
|       28 | 14351 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14352 |  |
|        - | 14353 | `	const char *zStr; /* Input string */` |
|        - | 14354 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14355 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14356 | `	int nLen;` |
|        - | 14357 | `	sxi32 rc;` |
|       29 | 14358 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14359 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14360 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14361 | `		return PH7_OK;` |
|        - | 14362 | `	}` |
|        - | 14363 | `	/* Extract the given URI */` |
|       29 | 14364 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14365 | `	if( nLen < 1 ){` |
|        - | 14366 | `		/* Nothing to process,return FALSE */` |
|        3 | 14367 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14368 | `		return PH7_OK;` |
|        - | 14369 | `	}` |
|        - | 14370 | `	/* Get a parse */` |
|       27 | 14371 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14372 | `	if( rc != SXRET_OK ){` |
|        - | 14373 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14374 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14375 | `		return PH7_OK;` |
|        - | 14376 | `	}` |
|       27 | 14377 | `	if( nArg > 1 ){` |
|      ! 0 | 14378 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14379 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14380 | `		switch(nComponent){` |
|      ! 0 | 14381 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14382 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14383 | `			if( pComp->nByte < 1 ){` |
|        - | 14384 | `				/* No available value,return NULL */` |
|      ! 0 | 14385 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14386 | `			}else{` |
|      ! 0 | 14387 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14388 | `			}` |
|      ! 0 | 14389 | `			break;` |
|      ! 0 | 14390 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14391 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14392 | `			if( pComp->nByte < 1 ){` |
|        - | 14393 | `				/* No available value,return NULL */` |
|      ! 0 | 14394 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14395 | `			}else{` |
|      ! 0 | 14396 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14397 | `			}` |
|      ! 0 | 14398 | `			break;` |
|      ! 0 | 14399 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14400 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14401 | `			if( pComp->nByte < 1 ){` |
|        - | 14402 | `				/* No available value,return NULL */` |
|      ! 0 | 14403 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14404 | `			}else{` |
|      ! 0 | 14405 | `				int iPort = 0;` |
|        - | 14406 | `				/* Cast the value to integer */` |
|      ! 0 | 14407 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14408 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14409 | `			}` |
|      ! 0 | 14410 | `			break;` |
|      ! 0 | 14411 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14412 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14413 | `			if( pComp->nByte < 1 ){` |
|        - | 14414 | `				/* No available value,return NULL */` |
|      ! 0 | 14415 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14416 | `			}else{` |
|      ! 0 | 14417 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14418 | `			}` |
|      ! 0 | 14419 | `			break;` |
|      ! 0 | 14420 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14421 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14422 | `			if( pComp->nByte < 1 ){` |
|        - | 14423 | `				/* No available value,return NULL */` |
|      ! 0 | 14424 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14425 | `			}else{` |
|      ! 0 | 14426 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14427 | `			}` |
|      ! 0 | 14428 | `			break;` |
|      ! 0 | 14429 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14430 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14431 | `			if( pComp->nByte < 1 ){` |
|        - | 14432 | `				/* No available value,return NULL */` |
|      ! 0 | 14433 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14434 | `			}else{` |
|      ! 0 | 14435 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14436 | `			}` |
|      ! 0 | 14437 | `			break;` |
|      ! 0 | 14438 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14439 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14440 | `			if( pComp->nByte < 1 ){` |
|        - | 14441 | `				/* No available value,return NULL */` |
|      ! 0 | 14442 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14443 | `			}else{` |
|      ! 0 | 14444 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14445 | `			}` |
|      ! 0 | 14446 | `			break;` |
|      ! 0 | 14447 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14448 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14449 | `			if( pComp->nByte < 1 ){` |
|        - | 14450 | `				/* No available value,return NULL */` |
|      ! 0 | 14451 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14452 | `			}else{` |
|      ! 0 | 14453 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14454 | `			}` |
|      ! 0 | 14455 | `			break;` |
|      ! 0 | 14456 | `		default:` |
|        - | 14457 | `			/* No such entry,return NULL */` |
|      ! 0 | 14458 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14459 | `			break;` |
|        - | 14460 | `		}` |
|      ! 0 | 14461 | `	}else{` |
|        - | 14462 | `		ph7_value *pArray,*pValue;` |
|        - | 14463 | `		/* Return an associative array */` |
|       27 | 14464 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14465 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14466 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14467 | `			/* Out of memory */` |
|      ! 0 | 14468 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14469 | `			/* Return false */` |
|      ! 0 | 14470 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14471 | `			return PH7_OK;` |
|        - | 14472 | `		}` |
|        - | 14473 | `		/* Fill the array */` |
|       27 | 14474 | `		pComp = &sURI.sScheme;` |
|       27 | 14475 | `		if( pComp->nByte > 0 ){` |
|       19 | 14476 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14477 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14478 | `		}` |
|        - | 14479 | `		/* Reset the string cursor */` |
|       27 | 14480 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14481 | `		pComp = &sURI.sHost;` |
|       27 | 14482 | `		if( pComp->nByte > 0 ){` |
|       25 | 14483 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14484 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14485 | `		}` |
|        - | 14486 | `		/* Reset the string cursor */` |
|       27 | 14487 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14488 | `		pComp = &sURI.sPort;` |
|       27 | 14489 | `		if( pComp->nByte > 0 ){` |
|       11 | 14490 | `			int iPort = 0;/* cc warning */` |
|        - | 14491 | `			/* Convert to integer */` |
|       11 | 14492 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14493 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14494 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14495 | `		}` |
|        - | 14496 | `		/* Reset the string cursor */` |
|       27 | 14497 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14498 | `		pComp = &sURI.sUser;` |
|       27 | 14499 | `		if( pComp->nByte > 0 ){` |
|        7 | 14500 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14501 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14502 | `		}` |
|        - | 14503 | `		/* Reset the string cursor */` |
|       27 | 14504 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14505 | `		pComp = &sURI.sPass;` |
|       27 | 14506 | `		if( pComp->nByte > 0 ){` |
|        7 | 14507 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14508 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14509 | `		}` |
|        - | 14510 | `		/* Reset the string cursor */` |
|       27 | 14511 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14512 | `		pComp = &sURI.sPath;` |
|       27 | 14513 | `		if( pComp->nByte > 0 ){` |
|       17 | 14514 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14515 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14516 | `		}` |
|        - | 14517 | `		/* Reset the string cursor */` |
|       27 | 14518 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14519 | `		pComp = &sURI.sQuery;` |
|       27 | 14520 | `		if( pComp->nByte > 0 ){` |
|        5 | 14521 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14522 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14523 | `		}` |
|        - | 14524 | `		/* Reset the string cursor */` |
|       27 | 14525 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14526 | `		pComp = &sURI.sFragment;` |
|       27 | 14527 | `		if( pComp->nByte > 0 ){` |
|        5 | 14528 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14529 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14530 | `		}` |
|        - | 14531 | `		/* Return the created array */` |
|       27 | 14532 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14533 | `		/* NOTE:` |
|        - | 14534 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14535 | `		 * automatically as soon we return from this function.` |
|        - | 14536 | `		 */` |
|        - | 14537 | `	}` |
|        - | 14538 | `	/* All done */` |
|       27 | 14539 | `	return PH7_OK;` |
|       15 | 14540 |  |
|        - | 14541 | `/*` |
|        - | 14542 | ` * Section:` |
|        - | 14543 | ` *   Array related routines.` |
|        - | 14544 | ` * Status:` |
|        - | 14545 | ` *    Stable.` |
|        - | 14546 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14547 | ` *  Array related functions that need access to the underlying` |
|        - | 14548 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14549 | ` */` |
|        - | 14550 | `/*` |
|        - | 14551 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14552 | ` * of the following structure.` |
|        - | 14553 | ` */` |
|        - | 14554 | `struct compact_data` |
|        - | 14555 |  |
|        - | 14556 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14557 | `	int nRecCount;      /* Recursion count */` |
|        - | 14558 | `};` |
|        - | 14559 | `/*` |
|        - | 14560 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14561 | ` */` |
|      ! 0 | 14562 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14563 |  |
|      ! 0 | 14564 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14565 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14566 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14567 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14568 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14569 | `		SyString sVar;` |
|      ! 0 | 14570 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14571 | `		if( sVar.nByte > 0 ){` |
|        - | 14572 | `			/* Query the current frame */` |
|      ! 0 | 14573 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14574 | `			/* ^` |
|        - | 14575 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14576 | `			 */` |
|      ! 0 | 14577 | `			if( pKey ){` |
|        - | 14578 | `				/* Perform the insertion */` |
|      ! 0 | 14579 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14580 | `			}` |
|      ! 0 | 14581 | `		}` |
|      ! 0 | 14582 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14583 | `		int rc;` |
|        - | 14584 | `		/* Recursively traverse this array */` |
|      ! 0 | 14585 | `		pData->nRecCount++;` |
|      ! 0 | 14586 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14587 | `		pData->nRecCount--;` |
|      ! 0 | 14588 | `		return rc;` |
|        - | 14589 | `	}` |
|      ! 0 | 14590 | `	return SXRET_OK;` |
|      ! 0 | 14591 |  |
|        - | 14592 | `/*` |
|        - | 14593 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14594 | ` *  Create array containing variables and their values.` |
|        - | 14595 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14596 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14597 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14598 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14599 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14600 | ` * Parameters` |
|        - | 14601 | ` *  $varname` |
|        - | 14602 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14603 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14604 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14605 | ` *   it recursively.` |
|        - | 14606 | ` * Return` |
|        - | 14607 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14608 | ` */` |
|        2 | 14609 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14610 |  |
|        - | 14611 | `	ph7_value *pArray,*pObj;` |
|        3 | 14612 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14613 | `	const char *zName;` |
|        - | 14614 | `	SyString sVar;` |
|        - | 14615 | `	int i,nLen;` |
|        3 | 14616 | `	if( nArg < 1 ){` |
|        - | 14617 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14618 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14619 | `		return PH7_OK;` |
|        - | 14620 | `	}` |
|        - | 14621 | `	/* Create the array */` |
|        3 | 14622 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14623 | `	if( pArray == 0 ){` |
|        - | 14624 | `		/* Out of memory */` |
|      ! 0 | 14625 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14626 | `		/* Return NULL */` |
|      ! 0 | 14627 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14628 | `		return PH7_OK;` |
|        - | 14629 | `	}` |
|        - | 14630 | `	/* Perform the requested operation */` |
|        7 | 14631 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14632 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14633 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14634 | `				struct compact_data sData;` |
|      ! 0 | 14635 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14636 | `				/* Recursively walk the array */` |
|      ! 0 | 14637 | `				sData.nRecCount = 0;` |
|      ! 0 | 14638 | `				sData.pArray = pArray;` |
|      ! 0 | 14639 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14640 | `			}` |
|      ! 0 | 14641 | `		}else{` |
|        - | 14642 | `			/* Extract variable name */` |
|        5 | 14643 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14644 | `			if( nLen > 0 ){` |
|        5 | 14645 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14646 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14647 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14648 | `				if( pObj ){` |
|        5 | 14649 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14650 | `				}` |
|        2 | 14651 | `			}` |
|        - | 14652 | `		}` |
|        3 | 14653 | `	}` |
|        - | 14654 | `	/* Return the array */` |
|        3 | 14655 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14656 | `	return PH7_OK;` |
|        2 | 14657 |  |
|        - | 14658 | `/*` |
|        - | 14659 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14660 | ` * of the following structure.` |
|        - | 14661 | ` */` |
|        - | 14662 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14663 | `struct extract_aux_data` |
|        - | 14664 |  |
|        - | 14665 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14666 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14667 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14668 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14669 | `	int iFlags;           /* Control flags */` |
|        - | 14670 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14671 | `};` |
|        - | 14672 | `/* Forward declaration */` |
|        - | 14673 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14674 | `/*` |
|        - | 14675 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14676 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14677 | ` * Parameters` |
|        - | 14678 | ` * $var_array` |
|        - | 14679 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14680 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14681 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14682 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14683 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14684 | ` * $extract_type` |
|        - | 14685 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14686 | ` *  It can be one of the following values:` |
|        - | 14687 | ` *   EXTR_OVERWRITE` |
|        - | 14688 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14689 | ` *   EXTR_SKIP` |
|        - | 14690 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14691 | ` *   EXTR_PREFIX_SAME` |
|        - | 14692 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14693 | ` *   EXTR_PREFIX_ALL` |
|        - | 14694 | ` *       Prefix all variable names with prefix.` |
|        - | 14695 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14696 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14697 | ` *   EXTR_IF_EXISTS` |
|        - | 14698 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14699 | ` *       otherwise do nothing.` |
|        - | 14700 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14701 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14702 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14703 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14704 | ` *      the current symbol table.` |
|        - | 14705 | ` * $prefix` |
|        - | 14706 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14707 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14708 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14709 | ` *  underscore character.` |
|        - | 14710 | ` * Return` |
|        - | 14711 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14712 | ` */` |
|        4 | 14713 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14714 |  |
|        - | 14715 | `	extract_aux_data sAux;` |
|        - | 14716 | `	ph7_hashmap *pMap;` |
|        5 | 14717 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14718 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14719 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14720 | `		return PH7_OK;` |
|        - | 14721 | `	}` |
|        - | 14722 | `	/* Point to the target hashmap */` |
|        5 | 14723 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14724 | `	if( pMap->nEntry < 1 ){` |
|        - | 14725 | `		/* Empty map,return  0 */` |
|      ! 0 | 14726 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14727 | `		return PH7_OK;` |
|        - | 14728 | `	}` |
|        - | 14729 | `	/* Prepare the aux data */` |
|        5 | 14730 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14731 | `	if( nArg > 1 ){` |
|        3 | 14732 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14733 | `		if( nArg > 2 ){` |
|      ! 0 | 14734 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14735 | `		}` |
|        1 | 14736 | `	}` |
|        5 | 14737 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14738 | `	/* Invoke the worker callback */` |
|        5 | 14739 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14740 | `	/* Number of variables successfully imported */` |
|        5 | 14741 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14742 | `	return PH7_OK;` |
|        3 | 14743 |  |
|        - | 14744 | `/*` |
|        - | 14745 | ` * Worker callback for the [extract()] function defined` |
|        - | 14746 | ` * below.` |
|        - | 14747 | ` */` |
|        8 | 14748 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14749 |  |
|        9 | 14750 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14751 | `	int iFlags = pAux->iFlags;` |
|        9 | 14752 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14753 | `	ph7_value *pObj;` |
|        - | 14754 | `	SyString sVar;` |
|        9 | 14755 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14756 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14757 | `	}` |
|        - | 14758 | `	/* Perform a string cast */` |
|        9 | 14759 | `	PH7_MemObjToString(pKey);` |
|        9 | 14760 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14761 | `		/* Unavailable variable name */` |
|      ! 0 | 14762 | `		return SXRET_OK;` |
|        - | 14763 | `	}` |
|        9 | 14764 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14765 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14766 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14767 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14768 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14769 | `			);` |
|      ! 0 | 14770 | `	}else{` |
|       13 | 14771 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14772 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14773 | `	}` |
|        9 | 14774 | `	sVar.zString = pAux->zWorker;` |
|        - | 14775 | `	/* Try to extract the variable */` |
|        9 | 14776 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14777 | `	if( pObj ){` |
|        - | 14778 | `		/* Collision */` |
|        5 | 14779 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14780 | `			return SXRET_OK;` |
|        - | 14781 | `		}` |
|        5 | 14782 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14783 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14784 | `				/* Already prefixed */` |
|      ! 0 | 14785 | `				return SXRET_OK;` |
|        - | 14786 | `			}` |
|      ! 0 | 14787 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14788 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14789 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14790 | `				);` |
|      ! 0 | 14791 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14792 | `		}` |
|        3 | 14793 | `	}else{` |
|        - | 14794 | `		/* Create the variable */` |
|        5 | 14795 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14796 | `	}` |
|        9 | 14797 | `	if( pObj ){` |
|        - | 14798 | `		/* Overwrite the old value */` |
|        9 | 14799 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14800 | `		/* Increment counter */` |
|        9 | 14801 | `		pAux->iCount++;` |
|        4 | 14802 | `	}` |
|        9 | 14803 | `	return SXRET_OK;` |
|        5 | 14804 |  |
|        - | 14805 | `/*` |
|        - | 14806 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14807 | ` * defined below.` |
|        - | 14808 | ` */` |
|        2 | 14809 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14810 |  |
|        3 | 14811 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14812 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14813 | `	ph7_value *pObj;` |
|        - | 14814 | `	SyString sVar;` |
|        - | 14815 | `	/* Perform a string cast */` |
|        3 | 14816 | `	PH7_MemObjToString(pKey);` |
|        3 | 14817 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14818 | `		/* Unavailable variable name */` |
|      ! 0 | 14819 | `		return SXRET_OK;` |
|        - | 14820 | `	}` |
|        3 | 14821 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14822 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14823 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14824 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14825 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14826 | `			);` |
|        2 | 14827 | `	}else{` |
|      ! 0 | 14828 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14829 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14830 | `	}` |
|        3 | 14831 | `	sVar.zString = pAux->zWorker;` |
|        - | 14832 | `	/* Extract the variable */` |
|        3 | 14833 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14834 | `	if( pObj ){` |
|        3 | 14835 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14836 | `	}` |
|        3 | 14837 | `	return SXRET_OK;` |
|        2 | 14838 |  |
|        - | 14839 | `/*` |
|        - | 14840 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14841 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14842 | ` * Parameters` |
|        - | 14843 | ` * $types` |
|        - | 14844 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14845 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14846 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14847 | ` *  POST includes the POST uploaded file information.` |
|        - | 14848 | ` *  Note:` |
|        - | 14849 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14850 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14851 | ` * $prefix` |
|        - | 14852 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14853 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14854 | ` *  variable named $pref_userid.` |
|        - | 14855 | ` * Return` |
|        - | 14856 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14857 | ` */` |
|        2 | 14858 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14859 |  |
|        - | 14860 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14861 | `	extract_aux_data sAux;` |
|        - | 14862 | `	int nLen,nPrefixLen;` |
|        - | 14863 | `	ph7_value *pSuper;` |
|        - | 14864 | `	ph7_vm *pVm;` |
|        - | 14865 | `	/* By default import only $_GET variables  */` |
|        3 | 14866 | `	zImport = "G";` |
|        3 | 14867 | `	nLen = (int)sizeof(char);` |
|        3 | 14868 | `	zPrefix = 0;` |
|        3 | 14869 | `	nPrefixLen = 0;` |
|        3 | 14870 | `	if( nArg > 0 ){` |
|        3 | 14871 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14872 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14873 | `		}` |
|        3 | 14874 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14875 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14876 | `		}` |
|        1 | 14877 | `	}` |
|        - | 14878 | `	/* Point to the underlying VM */` |
|        3 | 14879 | `	pVm = pCtx->pVm;` |
|        - | 14880 | `	/* Initialize the aux data */` |
|        3 | 14881 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14882 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14883 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14884 | `	sAux.pVm = pVm;` |
|        - | 14885 | `	/* Extract */` |
|        3 | 14886 | `	zEnd = &zImport[nLen];` |
|        5 | 14887 | `	while( zImport < zEnd ){` |
|        3 | 14888 | `		int c = zImport[0];` |
|        3 | 14889 | `		pSuper = 0;` |
|        3 | 14890 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14891 | `			/* Import $_GET variables */` |
|        3 | 14892 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14893 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14894 | `			/* Import $_POST variables */` |
|      ! 0 | 14895 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14896 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14897 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14898 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14899 | `		}` |
|        3 | 14900 | `		if( pSuper ){` |
|        - | 14901 | `			/* Iterate throw array entries */` |
|        3 | 14902 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14903 | `		}` |
|        - | 14904 | `		/* Advance the cursor */` |
|        3 | 14905 | `		zImport++;` |
|        1 | 14906 | `	}` |
|        - | 14907 | `	/* All done,return TRUE*/` |
|        3 | 14908 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14909 | `	return PH7_OK;` |
|        1 | 14910 |  |
|        - | 14911 | `/*` |
|        - | 14912 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14913 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14914 | ` * information.` |
|        - | 14915 | ` */` |
|    12730 | 14916 | `static sxi32 VmEvalChunk(` |
|        - | 14917 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14918 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14919 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14920 | `	int iFlags,         /* Compile flag */` |
|        - | 14921 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14922 | `	)` |
|        2 | 14923 |  |
|        - | 14924 | `	SySet *pByteCode,aByteCode;` |
|        - | 14925 | `	SyBlob sSavedNs;` |
|    12732 | 14926 | `	ProcConsumer xErr = 0;` |
|    12732 | 14927 | `	void *pErrData = 0;` |
|        - | 14928 | `	/* Initialize bytecode container */` |
|    12732 | 14929 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12732 | 14930 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14931 | `	/* Reset the code generator */` |
|    12732 | 14932 | `	if( bTrueReturn ){` |
|        - | 14933 | `		/* Included file,log compile-time errors */` |
|     9570 | 14934 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9570 | 14935 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4784 | 14936 | `	}` |
|    12732 | 14937 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14938 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14939 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14940 | `	 * the caller's namespace is restored. */` |
|    12732 | 14941 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12732 | 14942 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12732 | 14943 | `	if( bTrueReturn ){` |
|        - | 14944 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9570 | 14945 | `		SyBlobReset(&pVm->sNamespace);` |
|     4784 | 14946 | `	}` |
|        - | 14947 | `	/* Swap bytecode container */` |
|    12732 | 14948 | `	pByteCode = pVm->pByteContainer;` |
|    12732 | 14949 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14950 | `	/* Compile the chunk */` |
|    12732 | 14951 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19097 | 14952 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14953 | `		/* Compilation error,return false */` |
|        3 | 14954 | `		if( pCtx ){` |
|        3 | 14955 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14956 | `		}` |
|        2 | 14957 | `	}else{` |
|        - | 14958 | `		/* Mount any newly defined classes */` |
|        - | 14959 | `		SyHashEntry *pEntry;` |
|        - | 14960 | `		ph7_class *pClass;` |
|        - | 14961 | `		ph7_value sResult; /* Return value */` |
|        - | 14962 | `		sxi32 rc;` |
|    12730 | 14963 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   967816 | 14964 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   948724 | 14965 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14966 | `			/* Only mount classes that haven't been mounted yet */` |
|   948724 | 14967 | `			if( !pClass->bMounted ){` |
|   245982 | 14968 | `				rc = VmMountUserClass(pVm,pClass);` |
|   245982 | 14969 | `				if( rc != SXRET_OK ){` |
|        - | 14970 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14971 | `					if( pCtx ){` |
|      ! 0 | 14972 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14973 | `					}` |
|      ! 0 | 14974 | `					goto Cleanup;` |
|        - | 14975 | `				}` |
|   122990 | 14976 | `			}` |
|        2 | 14977 | `		}` |
|    12730 | 14978 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14979 | `			/* Out of memory */` |
|      ! 0 | 14980 | `			if( pCtx ){` |
|      ! 0 | 14981 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14982 | `			}` |
|      ! 0 | 14983 | `			goto Cleanup;` |
|        - | 14984 | `		}` |
|    12730 | 14985 | `		if( bTrueReturn ){` |
|        - | 14986 | `			/* Assume a boolean true return value */` |
|     9570 | 14987 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4786 | 14988 | `		}else{` |
|        - | 14989 | `			/* Assume a null return value */` |
|     3162 | 14990 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14991 | `		}` |
|        - | 14992 | `		/* Execute the compiled chunk */` |
|    12730 | 14993 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12730 | 14994 | `		if( pCtx ){` |
|        - | 14995 | `			/* Set the execution result */` |
|     9590 | 14996 | `			ph7_result_value(pCtx,&sResult);` |
|     4794 | 14997 | `		}` |
|    12730 | 14998 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14999 | `	}` |
|     6365 | 15000 | `Cleanup:` |
|        - | 15001 | `	/* Cleanup the mess left behind */` |
|    12732 | 15002 | `	pVm->pByteContainer = pByteCode;` |
|    12732 | 15003 | `	SySetRelease(&aByteCode);` |
|        - | 15004 | `	/* Restore caller's namespace state */` |
|    12732 | 15005 | `	SyBlobReset(&pVm->sNamespace);` |
|    12732 | 15006 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12732 | 15007 | `	SyBlobRelease(&sSavedNs);` |
|    12732 | 15008 | `	return SXRET_OK;` |
|        2 | 15009 |  |
|        - | 15010 | `/*` |
|        - | 15011 | ` * value eval(string $code)` |
|        - | 15012 | ` *   Evaluate a string as PHP code.` |
|        - | 15013 | ` * Parameter` |
|        - | 15014 | ` *  code: PHP code to evaluate.` |
|        - | 15015 | ` * Return` |
|        - | 15016 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15017 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15018 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15019 | ` */` |
|       24 | 15020 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15021 |  |
|        - | 15022 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 15023 | `	if( nArg < 1 ){` |
|        - | 15024 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15025 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15026 | `		return SXRET_OK;` |
|        - | 15027 | `	}` |
|        - | 15028 | `	/* Chunk to evaluate */` |
|       26 | 15029 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 15030 | `	if( sChunk.nByte < 1 ){` |
|        - | 15031 | `		/* Empty string,return NULL */` |
|        3 | 15032 | `		ph7_result_null(pCtx);` |
|        3 | 15033 | `		return SXRET_OK;` |
|        - | 15034 | `	}` |
|        - | 15035 | `	/* Eval the chunk */` |
|       24 | 15036 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 15037 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15038 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 15039 | `		return PH7_ABORT;` |
|        - | 15040 | `	}` |
|       22 | 15041 | `	return SXRET_OK;` |
|       14 | 15042 |  |
|        - | 15043 | `/*` |
|        - | 15044 | ` * Check if a file path is already included.` |
|        - | 15045 | ` */` |
|    19134 | 15046 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 15047 |  |
|        - | 15048 | `	SyString *aEntries;` |
|        - | 15049 | `	sxu32 n;` |
|    19136 | 15050 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15051 | `	/* Perform a linear search */` |
| 91337154 | 15052 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 91318030 | 15053 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15054 | `			/* Already included */` |
|       11 | 15055 | `			return TRUE;` |
|        - | 15056 | `		}` |
| 45659011 | 15057 | `	}` |
|    19126 | 15058 | `	return FALSE;` |
|     9569 | 15059 |  |
|        - | 15060 | `/*` |
|        - | 15061 | ` * Push a file path in the appropriate VM container.` |
|        - | 15062 | ` */` |
|    22266 | 15063 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 15064 |  |
|        - | 15065 | `	SyString sPath;` |
|        - | 15066 | `	char *zDup;` |
|        - | 15067 | `#ifdef __WINNT__` |
|        - | 15068 | `	char *zCur;` |
|        - | 15069 | `#endif` |
|        - | 15070 | `	sxi32 rc;` |
|    22268 | 15071 | `	if( nLen < 0 ){` |
|     3134 | 15072 | `		nLen = SyStrlen(zPath);` |
|     1566 | 15073 | `	}` |
|        - | 15074 | `	/* Duplicate the file path first */` |
|    22268 | 15075 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22268 | 15076 | `	if( zDup == 0 ){` |
|      ! 0 | 15077 | `		return SXERR_MEM;` |
|        - | 15078 | `	}` |
|        - | 15079 | `#ifdef __WINNT__` |
|        - | 15080 | `	/* Normalize path on windows` |
|        - | 15081 | `	 * Example:` |
|        - | 15082 | `	 *    Path/To/File.php` |
|        - | 15083 | `	 * becomes` |
|        - | 15084 | `	 *   path\to\file.php` |
|        - | 15085 | `	 */` |
|        2 | 15086 | `	zCur = zDup;` |
|        2 | 15087 | `	while( zCur[0] != 0 ){` |
|        2 | 15088 | `		if( zCur[0] == '/' ){` |
|        2 | 15089 | `			zCur[0] = '\\';` |
|        2 | 15090 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15091 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15092 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15093 | `		}` |
|        2 | 15094 | `		zCur++;` |
|        2 | 15095 | `	}` |
|        - | 15096 | `#endif` |
|        - | 15097 | `	/* Install the file path */` |
|    22268 | 15098 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22268 | 15099 | `	if( !bMain ){` |
|    19136 | 15100 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15101 | `			/* Already included */` |
|       11 | 15102 | `			*pNew = 0;` |
|        6 | 15103 | `		}else{` |
|        - | 15104 | `			/* Insert in the corresponding container */` |
|    19126 | 15105 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19126 | 15106 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15107 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15108 | `				return rc;` |
|        - | 15109 | `			}` |
|    19126 | 15110 | `			*pNew = 1;` |
|        - | 15111 | `		}` |
|     9567 | 15112 | `	}` |
|    22268 | 15113 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22268 | 15114 | `	return SXRET_OK;` |
|    11135 | 15115 |  |
|        - | 15116 | `/*` |
|        - | 15117 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15118 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15119 | ` * indicates failure.` |
|        - | 15120 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15121 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15122 | ` * operations.` |
|        - | 15123 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15124 | ` * this function is a no-op.` |
|        - | 15125 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15126 | ` * constructs for more information.` |
|        - | 15127 | ` */` |
|     9582 | 15128 | `static sxi32 VmExecIncludedFile(` |
|        - | 15129 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15130 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15131 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15132 | `	 )` |
|        2 | 15133 |  |
|        - | 15134 | `	sxi32 rc;` |
|        - | 15135 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15136 | `	const ph7_io_stream *pStream;` |
|        - | 15137 | `	SyBlob sContents;` |
|        - | 15138 | `	void *pHandle;` |
|        - | 15139 | `	ph7_vm *pVm;` |
|        - | 15140 | `	int isNew;` |
|        - | 15141 | `	/* Initialize fields */` |
|     9584 | 15142 | `	pVm = pCtx->pVm;` |
|     9584 | 15143 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9584 | 15144 | `	isNew = 0;` |
|        - | 15145 | `	/* Extract the associated stream */` |
|     9584 | 15146 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15147 | `	/*` |
|        - | 15148 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15149 | `	 * in a read-only mode.` |
|        - | 15150 | `	 */` |
|     9584 | 15151 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9584 | 15152 | `	if( pHandle == 0 ){` |
|        8 | 15153 | `		return SXERR_IO;` |
|        - | 15154 | `	}` |
|     9578 | 15155 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9578 | 15156 | `	if( IncludeOnce && !isNew ){` |
|        - | 15157 | `		/* Already included */` |
|        9 | 15158 | `		rc = SXERR_EXISTS;` |
|        5 | 15159 | `	}else{` |
|        - | 15160 | `		/* Read the whole file contents */` |
|     9570 | 15161 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9570 | 15162 | `		if( rc == SXRET_OK ){` |
|        - | 15163 | `			SyString sScript;` |
|        - | 15164 | `			/* Compile and execute the script */` |
|     9570 | 15165 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9570 | 15166 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4784 | 15167 | `		}` |
|        - | 15168 | `	}` |
|        - | 15169 | `	/* Pop from the set of included file */` |
|     9578 | 15170 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15171 | `	/* Close the handle */` |
|     9578 | 15172 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15173 | `	/* Release the working buffer */` |
|     9578 | 15174 | `	SyBlobRelease(&sContents);` |
|        - | 15175 | `#else` |
|        - | 15176 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15177 | `	SXUNUSED(pPath);` |
|        - | 15178 | `	SXUNUSED(IncludeOnce);` |
|        - | 15179 | `	rc = SXERR_IO;` |
|        - | 15180 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9578 | 15181 | `	return rc;` |
|     4793 | 15182 |  |
|        - | 15183 | `/*` |
|        - | 15184 | ` * string get_include_path(void)` |
|        - | 15185 | ` *  Gets the current include_path configuration option.` |
|        - | 15186 | ` * Parameter` |
|        - | 15187 | ` *  None` |
|        - | 15188 | ` * Return` |
|        - | 15189 | ` *  Included paths as a string` |
|        - | 15190 | ` */` |
|        2 | 15191 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15192 |  |
|        3 | 15193 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15194 | `	SyString *aEntry;` |
|        - | 15195 | `	int dir_sep;` |
|        - | 15196 | `	sxu32 n;` |
|        - | 15197 | `#ifdef __WINNT__` |
|        1 | 15198 | `	dir_sep = ';';` |
|        - | 15199 | `#else` |
|        - | 15200 | `	/* Assume UNIX path separator */` |
|        2 | 15201 | `	dir_sep = ':';` |
|        - | 15202 | `#endif` |
|        1 | 15203 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15204 | `	SXUNUSED(apArg);` |
|        - | 15205 | `	/* Point to the list of import paths */` |
|        3 | 15206 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15207 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15208 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15209 | `		if( n > 0 ){` |
|        - | 15210 | `			/* Append dir seprator */` |
|      ! 0 | 15211 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15212 | `		}` |
|        - | 15213 | `		/* Append path */` |
|        3 | 15214 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15215 | `	}` |
|        3 | 15216 | `	return PH7_OK;` |
|        1 | 15217 |  |
|        - | 15218 | `/*` |
|        - | 15219 | ` * string get_get_included_files(void)` |
|        - | 15220 | ` *  Gets the current include_path configuration option.` |
|        - | 15221 | ` * Parameter` |
|        - | 15222 | ` *  None` |
|        - | 15223 | ` * Return` |
|        - | 15224 | ` *  Included paths as a string` |
|        - | 15225 | ` */` |
|        2 | 15226 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15227 |  |
|        3 | 15228 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15229 | `	ph7_value *pArray,*pWorker;` |
|        - | 15230 | `	SyString *pEntry;` |
|        - | 15231 | `	int c,d;` |
|        - | 15232 | `	/* Create an array and a working value */` |
|        3 | 15233 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15234 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15235 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15236 | `		/* Out of memory,return null */` |
|      ! 0 | 15237 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15238 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15239 | `		SXUNUSED(apArg);` |
|      ! 0 | 15240 | `		return PH7_OK;` |
|        - | 15241 | `	}` |
|        3 | 15242 | `	c = d = '/';` |
|        - | 15243 | `#ifdef __WINNT__` |
|        1 | 15244 | `	d = '\\';` |
|        - | 15245 | `#endif` |
|        - | 15246 | `	/* Iterate throw entries */` |
|        3 | 15247 | `	SySetResetCursor(pFiles);` |
|     3917 | 15248 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15249 | `		const char *zBase,*zEnd;` |
|        - | 15250 | `		int iLen;` |
|        - | 15251 | `		/* reset the string cursor */` |
|     3915 | 15252 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15253 | `		/* Extract base name */` |
|     3915 | 15254 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15255 | `		/* Ignore trailing '/' */` |
|     5872 | 15256 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15257 | `			zEnd--;` |
|      ! 0 | 15258 | `		}` |
|     3915 | 15259 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15260 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15261 | `			zEnd--;` |
|        1 | 15262 | `		}` |
|     3915 | 15263 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15264 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15265 | `		/* Copy entry name */` |
|     3915 | 15266 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15267 | `		/* Perform the insertion */` |
|     3915 | 15268 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15269 | `	}` |
|        - | 15270 | `	/* All done,return the created array */` |
|        3 | 15271 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15272 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15273 | `	 * by the engine as soon we return from this foreign` |
|        - | 15274 | `	 * function.` |
|        - | 15275 | `	 */` |
|        3 | 15276 | `	return PH7_OK;` |
|        2 | 15277 |  |
|        - | 15278 | `/*` |
|        - | 15279 | ` * include:` |
|        - | 15280 | ` * According to the PHP reference manual.` |
|        - | 15281 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15282 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15283 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15284 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15285 | ` *  and the current working directory before failing. The include()` |
|        - | 15286 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15287 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15288 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15289 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15290 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15291 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15292 | ` *  directory to find the requested file.` |
|        - | 15293 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15294 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15295 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15296 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15297 | ` */` |
|     9558 | 15298 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15299 |  |
|        - | 15300 | `	SyString sFile;` |
|        - | 15301 | `	sxi32 rc;` |
|     9560 | 15302 | `	if( nArg < 1 ){` |
|        - | 15303 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15304 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15305 | `		return SXRET_OK;` |
|        - | 15306 | `	}` |
|        - | 15307 | `	/* File to include */` |
|     9560 | 15308 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9560 | 15309 | `	if( sFile.nByte < 1 ){` |
|        - | 15310 | `		/* Empty string,return NULL */` |
|      ! 0 | 15311 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15312 | `		return SXRET_OK;` |
|        - | 15313 | `	}` |
|        - | 15314 | `	/* Open,compile and execute the desired script */` |
|     9560 | 15315 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9560 | 15316 | `	if( rc != SXRET_OK ){` |
|        - | 15317 | `		/* Emit a warning and return false */` |
|        3 | 15318 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15319 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15320 | `	}` |
|     9560 | 15321 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15322 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 15323 | `		return PH7_ABORT;` |
|        - | 15324 | `	}` |
|     9556 | 15325 | `	return SXRET_OK;` |
|     4781 | 15326 |  |
|        - | 15327 | `/*` |
|        - | 15328 | ` * include_once:` |
|        - | 15329 | ` *  According to the PHP reference manual.` |
|        - | 15330 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15331 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15332 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15333 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15334 | ` *   just once.` |
|        - | 15335 | ` */` |
|       10 | 15336 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15337 |  |
|        - | 15338 | `	SyString sFile;` |
|        - | 15339 | `	sxi32 rc;` |
|       11 | 15340 | `	if( nArg < 1 ){` |
|        - | 15341 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15342 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15343 | `		return SXRET_OK;` |
|        - | 15344 | `	}` |
|        - | 15345 | `	/* File to include */` |
|       11 | 15346 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15347 | `	if( sFile.nByte < 1 ){` |
|        - | 15348 | `		/* Empty string,return NULL */` |
|      ! 0 | 15349 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15350 | `		return SXRET_OK;` |
|        - | 15351 | `	}` |
|        - | 15352 | `	/* Open,compile and execute the desired script */` |
|       11 | 15353 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15354 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15355 | `		/* File already included,return TRUE */` |
|        7 | 15356 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15357 | `		return SXRET_OK;` |
|        - | 15358 | `	}` |
|        5 | 15359 | `	if( rc != SXRET_OK ){` |
|        - | 15360 | `		/* Emit a warning and return false */` |
|      ! 0 | 15361 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15362 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15363 | ` 	}` |
|        5 | 15364 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15365 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15366 | `		return PH7_ABORT;` |
|        - | 15367 | `	}` |
|        5 | 15368 | `	return SXRET_OK;` |
|        6 | 15369 |  |
|        - | 15370 | `/*` |
|        - | 15371 | ` * require.` |
|        - | 15372 | ` *  According to the PHP reference manual.` |
|        - | 15373 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15374 | ` *   also produce a fatal level error.` |
|        - | 15375 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15376 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15377 | ` */` |
|        6 | 15378 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15379 |  |
|        - | 15380 | `	SyString sFile;` |
|        - | 15381 | `	sxi32 rc;` |
|        8 | 15382 | `	if( nArg < 1 ){` |
|        - | 15383 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15384 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15385 | `		return SXRET_OK;` |
|        - | 15386 | `	}` |
|        - | 15387 | `	/* File to include */` |
|        8 | 15388 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15389 | `	if( sFile.nByte < 1 ){` |
|        - | 15390 | `		/* Empty string,return NULL */` |
|      ! 0 | 15391 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15392 | `		return SXRET_OK;` |
|        - | 15393 | `	}` |
|        - | 15394 | `	/* Open,compile and execute the desired script */` |
|        8 | 15395 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15396 | `	if( rc != SXRET_OK ){` |
|        - | 15397 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15398 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15399 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15400 | `		return PH7_ABORT;` |
|        - | 15401 | `	}` |
|        8 | 15402 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15403 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15404 | `		return PH7_ABORT;` |
|        - | 15405 | `	}` |
|        8 | 15406 | `	return SXRET_OK;` |
|        5 | 15407 |  |
|        - | 15408 | `/*` |
|        - | 15409 | ` * require_once:` |
|        - | 15410 | ` *  According to the PHP reference manual.` |
|        - | 15411 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15412 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15413 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15414 | ` *   and how it differs from its non _once siblings.` |
|        - | 15415 | ` */` |
|        4 | 15416 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15417 |  |
|        - | 15418 | `	SyString sFile;` |
|        - | 15419 | `	sxi32 rc;` |
|        5 | 15420 | `	if( nArg < 1 ){` |
|        - | 15421 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15422 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15423 | `		return SXRET_OK;` |
|        - | 15424 | `	}` |
|        - | 15425 | `	/* File to include */` |
|        5 | 15426 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15427 | `	if( sFile.nByte < 1 ){` |
|        - | 15428 | `		/* Empty string,return NULL */` |
|      ! 0 | 15429 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15430 | `		return SXRET_OK;` |
|        - | 15431 | `	}` |
|        - | 15432 | `	/* Open,compile and execute the desired script */` |
|        5 | 15433 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15434 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15435 | `		/* File already included,return TRUE */` |
|        3 | 15436 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15437 | `		return SXRET_OK;` |
|        - | 15438 | `	}` |
|        3 | 15439 | `	if( rc != SXRET_OK ){` |
|        - | 15440 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15441 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15442 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15443 | `		return PH7_ABORT;` |
|        - | 15444 | `	}` |
|        3 | 15445 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15446 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15447 | `		return PH7_ABORT;` |
|        - | 15448 | `	}` |
|        3 | 15449 | `	return SXRET_OK;` |
|        3 | 15450 |  |
|        - | 15451 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15452 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15453 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15454 | `/*` |
|        - | 15455 | ` * Section:` |
|        - | 15456 | ` *  SPL Autoloading functions.` |
|        - | 15457 | ` * Status:` |
|        - | 15458 | ` *  Stable.` |
|        - | 15459 | ` */` |
|        - | 15460 | `/*` |
|        - | 15461 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15462 | ` *  Register given function as __autoload() implementation.` |
|        - | 15463 | ` * Parameters` |
|        - | 15464 | ` *  callback` |
|        - | 15465 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15466 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15467 | ` *  throw` |
|        - | 15468 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15469 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15470 | ` *  prepend` |
|        - | 15471 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15472 | ` *   autoload stack instead of appending it.` |
|        - | 15473 | ` * Return` |
|        - | 15474 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15475 | ` */` |
|       34 | 15476 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15477 |  |
|        - | 15478 | `	VmAutoloadCB sEntry;` |
|       36 | 15479 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15480 | `	int iPrepend = 0;` |
|        - | 15481 | `	sxu32 n;` |
|       36 | 15482 | `	if( nArg < 1 ){` |
|        - | 15483 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15484 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15485 | `		/* Check for duplicates first */` |
|        9 | 15486 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15487 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15488 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15489 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15490 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15491 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15492 | `				return SXRET_OK;` |
|        - | 15493 | `			}` |
|      ! 0 | 15494 | `		}` |
|        5 | 15495 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15496 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15497 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15498 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15499 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15500 | `		return SXRET_OK;` |
|        - | 15501 | `	}` |
|        - | 15502 | `	/* Validate that the callback is callable */` |
|       28 | 15503 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15504 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15505 | `		if( nArg >= 2 ){` |
|      ! 0 | 15506 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15507 | `		}` |
|      ! 0 | 15508 | `		if( iThrow ){` |
|      ! 0 | 15509 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15510 | `				"Argument is not callable");` |
|      ! 0 | 15511 | `		}` |
|      ! 0 | 15512 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15513 | `		return SXRET_OK;` |
|        - | 15514 | `	}` |
|        - | 15515 | `	/* Check for duplicates */` |
|       46 | 15516 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15517 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15518 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15519 | `			/* Already registered */` |
|      ! 0 | 15520 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15521 | `			return SXRET_OK;` |
|        - | 15522 | `		}` |
|       11 | 15523 | `	}` |
|        - | 15524 | `	/* Check prepend flag */` |
|       28 | 15525 | `	if( nArg >= 3 ){` |
|        3 | 15526 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15527 | `	}` |
|        - | 15528 | `	/* Store the callback */` |
|       28 | 15529 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15530 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15531 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15532 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15533 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15534 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15535 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15536 | `		VmAutoloadCB *aBase;` |
|        3 | 15537 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15538 | `		/* Rotate: move last entry to front */` |
|        3 | 15539 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15540 | `		if( aBase ){` |
|        - | 15541 | `			VmAutoloadCB sTemp;` |
|        - | 15542 | `			sxu32 i;` |
|        3 | 15543 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15544 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15545 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15546 | `			}` |
|        3 | 15547 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15548 | `		}` |
|        2 | 15549 | `	}else{` |
|       26 | 15550 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15551 | `	}` |
|       28 | 15552 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15553 | `	return SXRET_OK;` |
|       19 | 15554 |  |
|        - | 15555 | `/*` |
|        - | 15556 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15557 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15558 | ` * Parameters` |
|        - | 15559 | ` *  callback` |
|        - | 15560 | ` *   The autoload function being unregistered.` |
|        - | 15561 | ` * Return` |
|        - | 15562 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15563 | ` */` |
|       32 | 15564 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15565 |  |
|       34 | 15566 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15567 | `	sxu32 n,nEntry;` |
|       34 | 15568 | `	if( nArg < 1 ){` |
|      ! 0 | 15569 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15570 | `		return SXRET_OK;` |
|        - | 15571 | `	}` |
|       34 | 15572 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15573 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15574 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15575 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15576 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15577 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15578 | `			sxu32 i;` |
|       32 | 15579 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15580 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15581 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15582 | `			}` |
|        - | 15583 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15584 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15585 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15586 | `			return SXRET_OK;` |
|        - | 15587 | `		}` |
|        3 | 15588 | `	}` |
|        3 | 15589 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15590 | `	return SXRET_OK;` |
|       18 | 15591 |  |
|        - | 15592 | `/*` |
|        - | 15593 | ` * array spl_autoload_functions(void)` |
|        - | 15594 | ` *  Return all registered __autoload() functions.` |
|        - | 15595 | ` * Return` |
|        - | 15596 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15597 | ` *  an empty array is returned.` |
|        - | 15598 | ` */` |
|       20 | 15599 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15600 |  |
|       21 | 15601 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15602 | `	ph7_value *pArray;` |
|        - | 15603 | `	sxu32 n,nEntry;` |
|       10 | 15604 | `	SXUNUSED(nArg);` |
|       10 | 15605 | `	SXUNUSED(apArg);` |
|       21 | 15606 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15607 | `	if( pArray == 0 ){` |
|      ! 0 | 15608 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15609 | `		return SXRET_OK;` |
|        - | 15610 | `	}` |
|       21 | 15611 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15612 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15613 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15614 | `		if( pEntry ){` |
|       15 | 15615 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15616 | `		}` |
|        8 | 15617 | `	}` |
|       21 | 15618 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15619 | `	return SXRET_OK;` |
|       11 | 15620 |  |
|        - | 15621 | `/*` |
|        - | 15622 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15623 | ` *  Default implementation of __autoload().` |
|        - | 15624 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15625 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15626 | ` * Parameters` |
|        - | 15627 | ` *  class` |
|        - | 15628 | ` *   The class name being searched.` |
|        - | 15629 | ` *  file_extensions` |
|        - | 15630 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15631 | ` */` |
|        2 | 15632 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15633 |  |
|        - | 15634 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15635 | `	SyBlob sPath;` |
|        - | 15636 | `	int nClass;` |
|        - | 15637 | `	sxi32 rc;` |
|        3 | 15638 | `	if( nArg < 1 ){` |
|      ! 0 | 15639 | `		return SXRET_OK;` |
|        - | 15640 | `	}` |
|        3 | 15641 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15642 | `	if( nClass < 1 ){` |
|      ! 0 | 15643 | `		return SXRET_OK;` |
|        - | 15644 | `	}` |
|        - | 15645 | `	/* Default extensions */` |
|        3 | 15646 | `	zExt = ".php,.inc";` |
|        3 | 15647 | `	if( nArg >= 2 ){` |
|        - | 15648 | `		int nExt;` |
|      ! 0 | 15649 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15650 | `		if( nExt < 1 ){` |
|      ! 0 | 15651 | `			zExt = ".php,.inc";` |
|      ! 0 | 15652 | `		}` |
|      ! 0 | 15653 | `	}` |
|        3 | 15654 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15655 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15656 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15657 | `	zCur = zExt;` |
|        7 | 15658 | `	while( zCur < zEnd ){` |
|        - | 15659 | `		const char *zComma;` |
|        - | 15660 | `		SyString sFile;` |
|        - | 15661 | `		int i;` |
|        - | 15662 | `		/* Find next comma or end */` |
|        5 | 15663 | `		zComma = zCur;` |
|       21 | 15664 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15665 | `			zComma++;` |
|        1 | 15666 | `		}` |
|        - | 15667 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15668 | `		SyBlobReset(&sPath);` |
|       69 | 15669 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15670 | `			char c = zClass[i];` |
|       65 | 15671 | `			if( c == '\\' ){` |
|      ! 0 | 15672 | `				c = '/';` |
|       65 | 15673 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15674 | `				c = c + ('a' - 'A');` |
|        6 | 15675 | `			}` |
|       65 | 15676 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15677 | `		}` |
|        - | 15678 | `		/* Append extension */` |
|        5 | 15679 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15680 | `		/* Try to include the file */` |
|        5 | 15681 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15682 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15683 | `		if( rc == SXRET_OK ){` |
|        - | 15684 | `			/* File included successfully */` |
|      ! 0 | 15685 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15686 | `			return SXRET_OK;` |
|        - | 15687 | `		}` |
|        - | 15688 | `		/* Move past the comma */` |
|        5 | 15689 | `		zCur = zComma;` |
|        5 | 15690 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15691 | `			zCur++;` |
|        1 | 15692 | `		}` |
|        1 | 15693 | `	}` |
|        3 | 15694 | `	SyBlobRelease(&sPath);` |
|        3 | 15695 | `	return SXRET_OK;` |
|        2 | 15696 |  |
|        - | 15697 | `/* Table of built-in VM functions. */` |
|        - | 15698 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15699 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15700 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15701 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15702 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15703 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15704 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15705 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15706 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15707 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15708 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15709 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15710 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15711 | `	    /* Constants management */` |
|        - | 15712 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15713 | `	{ "define",   vm_builtin_define               },` |
|        - | 15714 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15715 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15716 | `	   /* Class/Object functions */` |
|        - | 15717 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15718 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15719 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15720 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15721 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15722 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15723 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15724 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15725 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15726 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15727 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15728 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15729 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15730 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15731 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15732 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15733 | `	   /* SPL Autoloading */` |
|        - | 15734 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15735 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15736 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15737 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15738 | `	   /* Random numbers/strings generators */` |
|        - | 15739 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15740 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15741 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15742 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15743 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15744 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15745 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15746 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15747 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15748 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15749 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15750 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15751 | `	   /* Language constructs functions */` |
|        - | 15752 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15753 | `	{ "print", vm_builtin_print                   },` |
|        - | 15754 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15755 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15756 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15757 | `	  /* Variable handling functions */` |
|        - | 15758 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15759 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15760 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15761 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15762 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15763 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15764 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15765 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15766 | `	  /* Ouput control functions */` |
|        - | 15767 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15768 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15769 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15770 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15771 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15772 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15773 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15774 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15775 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15776 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15777 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15778 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15779 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15780 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15781 | `	  /* Assertion functions */` |
|        - | 15782 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15783 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15784 | `	  /* Error reporting functions */` |
|        - | 15785 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15786 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15787 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15788 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15789 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15790 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15791 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15792 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15793 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15794 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15795 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15796 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15797 | `	  /* Release info */` |
|        - | 15798 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15799 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15800 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15801 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15802 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15803 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15804 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15805 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15806 | `	  /* hashmap */` |
|        - | 15807 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15808 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15809 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15810 | `	  /* URL related function */` |
|        - | 15811 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15812 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15813 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15814 | `	   /* XML processing functions */` |
|        - | 15815 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15816 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15817 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15818 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15819 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15820 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15821 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15822 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15823 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15824 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15825 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15826 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15827 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15828 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15829 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15830 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15831 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15832 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15833 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15834 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15835 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15836 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15837 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15838 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15839 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15840 | `	   /* Command line processing */` |
|        - | 15841 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15842 | `	   /* JSON encoding/decoding */` |
|        - | 15843 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15844 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15845 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15846 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15847 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15848 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15849 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15850 | `	   /* Files/URI inclusion facility */` |
|        - | 15851 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15852 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15853 | `	{ "include",      vm_builtin_include          },` |
|        - | 15854 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15855 | `	{ "require",      vm_builtin_require          },` |
|        - | 15856 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15857 | `};` |
|        - | 15858 | `/*` |
|        - | 15859 | ` * Register the built-in VM functions defined above.` |
|        - | 15860 | ` */` |
|     2826 | 15861 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15862 |  |
|        - | 15863 | `	sxi32 rc;` |
|        - | 15864 | `	sxu32 n;` |
|   381512 | 15865 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15866 | `		/* Note that these special functions have access` |
|        - | 15867 | `		 * to the underlying virtual machine as their` |
|        - | 15868 | `		 * private data.` |
|        - | 15869 | `		 */` |
|   378686 | 15870 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   378686 | 15871 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15872 | `			return rc;` |
|        - | 15873 | `		}` |
|   189344 | 15874 | `	}` |
|     2828 | 15875 | `	return SXRET_OK;` |
|     1415 | 15876 |  |
|        - | 15877 | `/*` |
|        - | 15878 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15879 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15880 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15881 | ` */` |
|   182382 | 15882 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15883 |  |
|   182384 | 15884 | `	if( !iLoadable ){` |
|   180296 | 15885 | `		return pClass;` |
|        - | 15886 | `	}` |
|     2094 | 15887 | `	while(pClass){` |
|     2090 | 15888 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2086 | 15889 | `			return pClass;` |
|        - | 15890 | `		}` |
|        5 | 15891 | `		pClass = pClass->pNextName;` |
|        1 | 15892 | `	}` |
|        5 | 15893 | `	return 0;` |
|    91193 | 15894 |  |
|        - | 15895 | `/*` |
|        - | 15896 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15897 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15898 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15899 | ` * registered in the VM's class table.` |
|        - | 15900 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15901 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15902 | ` */` |
|       38 | 15903 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15904 |  |
|        - | 15905 | `	VmAutoloadCB *pEntry;` |
|        - | 15906 | `	ph7_value sArg,sResult;` |
|        - | 15907 | `	SyHashEntry *pHashEntry;` |
|        - | 15908 | `	ph7_class *pClass;` |
|        - | 15909 | `	sxu32 n,nEntry;` |
|       40 | 15910 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15911 | `	if( nEntry < 1 ){` |
|       26 | 15912 | `		return 0;` |
|        - | 15913 | `	}` |
|        - | 15914 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15915 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15916 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15917 | `	}` |
|        - | 15918 | `	/* Mark this class as being autoloaded */` |
|       14 | 15919 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15920 | `	/* Prepare the class name argument */` |
|       14 | 15921 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15922 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15923 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15924 | `	pClass = 0;` |
|       28 | 15925 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15926 | `		ph7_value *apArg[1];` |
|       24 | 15927 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15928 | `		if( pEntry == 0 ){` |
|      ! 0 | 15929 | `			continue;` |
|        - | 15930 | `		}` |
|       24 | 15931 | `		apArg[0] = &sArg;` |
|       24 | 15932 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15933 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15934 | `			continue;` |
|        - | 15935 | `		}` |
|        - | 15936 | `		/* Check if the class is now available */` |
|       24 | 15937 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15938 | `		if( pHashEntry ){` |
|       10 | 15939 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15940 | `			if( pClass ){` |
|       10 | 15941 | `				break;` |
|        - | 15942 | `			}` |
|      ! 0 | 15943 | `		}` |
|        9 | 15944 | `	}` |
|       14 | 15945 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15946 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15947 | `	/* Remove reentrancy guard */` |
|       14 | 15948 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15949 | `	return pClass;` |
|       21 | 15950 |  |
|        - | 15951 | `/*` |
|        - | 15952 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15953 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15954 | ` */` |
|       18 | 15955 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15956 |  |
|       20 | 15957 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15958 |  |
|        - | 15959 | `/*` |
|        - | 15960 | ` * Check if the given name refer to an installed class.` |
|        - | 15961 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15962 | ` */` |
|   182394 | 15963 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15964 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15965 | `	const char *zName,  /* Name of the target class */` |
|        - | 15966 | `	sxu32 nByte,        /* zName length */` |
|        - | 15967 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15968 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15969 | `						 */` |
|        - | 15970 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15971 | `	)` |
|        2 | 15972 |  |
|        - | 15973 | `	SyHashEntry *pEntry;` |
|        - | 15974 | `	ph7_class *pClass;` |
|    91197 | 15975 | `	SXUNUSED(iNest);` |
|        - | 15976 | `	/* Exact class lookup.` |
|        - | 15977 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15978 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   182396 | 15979 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   182396 | 15980 | `	if( pEntry == 0 ){` |
|        - | 15981 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15982 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15983 | `	}` |
|   182376 | 15984 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   182376 | 15985 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    91199 | 15986 |  |
|        - | 15987 | `/*` |
|        - | 15988 | ` * Reference Table Implementation` |
|        - | 15989 | ` * Status: stable <chm@symisc.net>` |
|        - | 15990 | ` * Intro` |
|        - | 15991 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15992 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15993 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15994 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15995 | ` *  Refer to the official for more information on this powerful` |
|        - | 15996 | ` *  extension.` |
|        - | 15997 | ` */` |
|        - | 15998 | `/*` |
|        - | 15999 | ` * Allocate a new reference entry.` |
|        - | 16000 | ` */` |
|  3203096 | 16001 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 16002 |  |
|        - | 16003 | `	VmRefObj *pRef;` |
|        - | 16004 | `	/* Allocate a new instance */` |
|  3203098 | 16005 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3203098 | 16006 | `	if( pRef == 0 ){` |
|      ! 0 | 16007 | `		return 0;` |
|        - | 16008 | `	}` |
|        - | 16009 | `	/* Zero the structure */` |
|  3203098 | 16010 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16011 | `	/* Initialize fields */` |
|  3203098 | 16012 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3203098 | 16013 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3203098 | 16014 | `	pRef->nIdx = nIdx;` |
|  3203098 | 16015 | `	return pRef;` |
|  1601550 | 16016 |  |
|        - | 16017 | `/*` |
|        - | 16018 | ` * Default hash function used by the reference table` |
|        - | 16019 | ` * for lookup/insertion operations.` |
|        - | 16020 | ` */` |
| 17544138 | 16021 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 16022 |  |
|        - | 16023 | `	/* Calculate the hash based on the memory object index */` |
| 17544140 | 16024 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 16025 |  |
|        - | 16026 | `/*` |
|        - | 16027 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16028 | ` * in the reference table.` |
|        - | 16029 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16030 | ` * otherwise.` |
|        - | 16031 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16032 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16033 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16034 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16035 | ` * Refer to the official for more information on this powerful` |
|        - | 16036 | ` * extension.` |
|        - | 16037 | ` */` |
|  9549328 | 16038 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 16039 |  |
|        - | 16040 | `	VmRefObj *pRef;` |
|        - | 16041 | `	sxu32 nBucket;` |
|        - | 16042 | `	/* Point to the appropriate bucket */` |
|  9549330 | 16043 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16044 | `	/* Perform the lookup */` |
|  9549330 | 16045 | `	pRef = pVm->apRefObj[nBucket];` |
| 20995179 | 16046 | `	for(;;){` |
| 41980859 | 16047 | `		if( pRef == 0 ){` |
|  3308482 | 16048 | `			break;` |
|        - | 16049 | `		}` |
| 38672379 | 16050 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16051 | `			/* Entry found */` |
|  6240850 | 16052 | `			return pRef;` |
|        - | 16053 | `		}` |
|        - | 16054 | `		/* Point to the next entry */` |
| 32431531 | 16055 | `		pRef = pRef->pNextCollide;` |
|        2 | 16056 | `	}` |
|        - | 16057 | `	/* No such entry,return NULL */` |
|  3308482 | 16058 | `	return 0;` |
|  4774666 | 16059 |  |
|        - | 16060 | `/*` |
|        - | 16061 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16062 | ` *` |
|        - | 16063 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16064 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16065 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16066 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16067 | ` * Refer to the official for more information on this powerful` |
|        - | 16068 | ` * extension.` |
|        - | 16069 | ` */` |
|  3203096 | 16070 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16071 |  |
|        - | 16072 | `	sxu32 nBucket;` |
|  3203098 | 16073 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16074 | `		VmRefObj **apNew;` |
|        - | 16075 | `		sxu32 nNew;` |
|        - | 16076 | `		/* Allocate a larger table */` |
|     4484 | 16077 | `		nNew = pVm->nRefSize << 1;` |
|     4484 | 16078 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4484 | 16079 | `		if( apNew ){` |
|     4484 | 16080 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16081 | `			sxu32 n;` |
|        - | 16082 | `			/* Zero the structure */` |
|     4484 | 16083 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16084 | `			/* Rehash all referenced entries */` |
|  2848110 | 16085 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16086 | `				/* Remove old collision links */` |
|  2843628 | 16087 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16088 | `				/* Point to the appropriate bucket */` |
|  2843628 | 16089 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16090 | `				/* Insert the entry  */` |
|  2843628 | 16091 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843628 | 16092 | `				if( apNew[nBucket] ){` |
|  2301116 | 16093 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16094 | `				}` |
|  2843628 | 16095 | `				apNew[nBucket] = pEntry;` |
|        - | 16096 | `				/* Point to the next entry */` |
|  2843628 | 16097 | `				pEntry = pEntry->pNext;` |
|  1421815 | 16098 | `			}` |
|        - | 16099 | `			/* Release the old table */` |
|     4484 | 16100 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16101 | `			/* Install the new one */` |
|     4484 | 16102 | `			pVm->apRefObj = apNew;` |
|     4484 | 16103 | `			pVm->nRefSize = nNew;` |
|     2241 | 16104 | `		}` |
|     2241 | 16105 | `	}` |
|        - | 16106 | `	/* Point to the appropriate bucket */` |
|  3203098 | 16107 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16108 | `	/* Insert the entry */` |
|  3203098 | 16109 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3203098 | 16110 | `	if( pVm->apRefObj[nBucket] ){` |
|  2614677 | 16111 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307346 | 16112 | `	}` |
|  3203098 | 16113 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3203098 | 16114 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3203098 | 16115 | `	pVm->nRefUsed++;` |
|  3203098 | 16116 | `	return SXRET_OK;` |
|        2 | 16117 |  |
|        - | 16118 | `/*` |
|        - | 16119 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16120 | ` * the reference table.` |
|        - | 16121 | ` * This function is invoked when the user perform an unset` |
|        - | 16122 | ` * call [i.e: unset($var); ].` |
|        - | 16123 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16124 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16125 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16126 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16127 | ` * Refer to the official for more information on this powerful` |
|        - | 16128 | ` * extension.` |
|        - | 16129 | ` */` |
|  3161840 | 16130 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16131 |  |
|        - | 16132 | `	ph7_hashmap_node **apNode;` |
|        - | 16133 | `	SyHashEntry **apEntry;` |
|        - | 16134 | `	sxu32 n;` |
|        - | 16135 | `	/* Point to the reference table */` |
|  3161842 | 16136 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3161842 | 16137 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16138 | `	/* Unlink the entry from the reference table */` |
|  3273194 | 16139 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   111354 | 16140 | `		if( apEntry[n] ){` |
|   111304 | 16141 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55651 | 16142 | `		}` |
|    55678 | 16143 | `	}` |
|  6212404 | 16144 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3050564 | 16145 | `		if( apNode[n] ){` |
|     7042 | 16146 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3520 | 16147 | `		}` |
|  1525283 | 16148 | `	}` |
|  3161842 | 16149 | `	if( pRef->pPrevCollide ){` |
|  1213754 | 16150 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   606598 | 16151 | `	}else{` |
|  1948090 | 16152 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16153 | `	}` |
|  3161842 | 16154 | `	if( pRef->pNextCollide ){` |
|  1801708 | 16155 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   900850 | 16156 | `	}` |
|  3161842 | 16157 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16158 | `	/* Release the node */` |
|  3161842 | 16159 | `	SySetRelease(&pRef->aReference);` |
|  3161842 | 16160 | `	SySetRelease(&pRef->aArrEntries);` |
|  3161842 | 16161 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3161842 | 16162 | `	pVm->nRefUsed--;` |
|  3161842 | 16163 | `	return SXRET_OK;` |
|        2 | 16164 |  |
|        - | 16165 | `/*` |
|        - | 16166 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16167 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16168 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16169 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16170 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16171 | ` * Refer to the official for more information on this powerful` |
|        - | 16172 | ` * extension.` |
|        - | 16173 | ` */` |
|  3238702 | 16174 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16175 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16176 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16177 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16178 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16179 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16180 | `	)` |
|        2 | 16181 |  |
|  3238704 | 16182 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16183 | `	VmRefObj *pRef;` |
|        - | 16184 | `	/* Check if the referenced object already exists */` |
|  3238704 | 16185 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3238704 | 16186 | `	if( pRef == 0 ){` |
|        - | 16187 | `		/* Create a new entry */` |
|  3203098 | 16188 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3203098 | 16189 | `		if( pRef == 0 ){` |
|      ! 0 | 16190 | `			return SXERR_MEM;` |
|        - | 16191 | `		}` |
|  3203098 | 16192 | `		pRef->iFlags = iFlags;` |
|        - | 16193 | `		/* Install the entry */` |
|  3203098 | 16194 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1601548 | 16195 | `	}` |
|  3238704 | 16196 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3238704 | 16197 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16198 | `		VmSlot sRef;` |
|        - | 16199 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16200 | `		 * be deleted when we leave this frame.` |
|        - | 16201 | `		 */` |
|   105494 | 16202 | `		sRef.nIdx = nIdx;` |
|   105494 | 16203 | `		sRef.pUserData = pEntry;` |
|   105494 | 16204 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16205 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16206 | `		}` |
|    52746 | 16207 | `	}` |
|  3238704 | 16208 | `	if( pEntry ){` |
|        - | 16209 | `		/* Address of the hash-entry */` |
|   140872 | 16210 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70435 | 16211 | `	}` |
|  3238704 | 16212 | `	if( pMapEntry ){` |
|        - | 16213 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3089416 | 16214 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1544707 | 16215 | `	}` |
|  3238704 | 16216 | `	return SXRET_OK;` |
|  1619353 | 16217 |  |
|        - | 16218 | `/*` |
|        - | 16219 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16220 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16221 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16222 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16223 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16224 | ` * Refer to the official for more information on this powerful` |
|        - | 16225 | ` * extension.` |
|        - | 16226 | ` */` |
|  3148978 | 16227 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16228 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16229 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16230 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16231 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16232 | `	)` |
|        2 | 16233 |  |
|        - | 16234 | `	VmRefObj *pRef;` |
|        - | 16235 | `	sxu32 n;` |
|        - | 16236 | `	/* Check if the referenced object already exists */` |
|  3148980 | 16237 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3148980 | 16238 | `	if( pRef == 0 ){` |
|        - | 16239 | `		/* Not such entry */` |
|   105380 | 16240 | `		return SXERR_NOTFOUND;` |
|        - | 16241 | `	}` |
|        - | 16242 | `	/* Remove the desired entry */` |
|  3043602 | 16243 | `	if( pEntry ){` |
|        - | 16244 | `		SyHashEntry **apEntry;` |
|       74 | 16245 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 16246 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 16247 | `			if( apEntry[n] == pEntry ){` |
|        - | 16248 | `				/* Nullify the entry */` |
|       74 | 16249 | `				apEntry[n] = 0;` |
|        - | 16250 | `				/*` |
|        - | 16251 | `				 * NOTE:` |
|        - | 16252 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16253 | `				 * we avoid wasting spaces.` |
|        - | 16254 | `				 */` |
|       36 | 16255 | `			}` |
|       97 | 16256 | `		}` |
|       36 | 16257 | `	}` |
|  3043602 | 16258 | `	if( pMapEntry ){` |
|        - | 16259 | `		ph7_hashmap_node **apNode;` |
|  3043530 | 16260 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6087152 | 16261 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3043624 | 16262 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16263 | `				/* nullify the entry */` |
|  3043530 | 16264 | `				apNode[n] = 0;` |
|  1521764 | 16265 | `			}` |
|  1521813 | 16266 | `		}` |
|  1521764 | 16267 | `	}` |
|  3043602 | 16268 | `	return SXRET_OK;` |
|  1574491 | 16269 |  |
|        - | 16270 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16271 | `/*` |
|        - | 16272 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16273 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16274 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16275 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16276 | ` * For more information on how to register IO stream devices,please` |
|        - | 16277 | ` * refer to the official documentation.` |
|        - | 16278 | ` */` |
|    29116 | 16279 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16280 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16281 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16282 | `	int nByte              /* *pzDevice length*/` |
|        - | 16283 | `	)` |
|        2 | 16284 |  |
|        - | 16285 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16286 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16287 | `	SyString sDev,sCur;` |
|        - | 16288 | `	sxu32 n,nEntry;` |
|        - | 16289 | `	int rc;` |
|        - | 16290 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29118 | 16291 | `	zNext = zCur = zIn = *pzDevice;` |
|    29118 | 16292 | `	zEnd = &zIn[nByte];` |
|  1859823 | 16293 | `	while( zIn < zEnd ){` |
|  1830709 | 16294 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16295 | `			/* Got one */` |
|        3 | 16296 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16297 | `			break;` |
|        - | 16298 | `		}` |
|        - | 16299 | `		/* Advance the cursor */` |
|  1830707 | 16300 | `		zIn++;` |
|        2 | 16301 | `	}` |
|    29118 | 16302 | `	if( zIn >= zEnd ){` |
|        - | 16303 | `		/* No such scheme,return the default stream */` |
|    29116 | 16304 | `		return pVm->pDefStream;` |
|        - | 16305 | `	}` |
|        3 | 16306 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16307 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16308 | `	SyStringFullTrim(&sDev);` |
|        - | 16309 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16310 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16311 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16312 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16313 | `		pStream = apStream[n];` |
|        3 | 16314 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16315 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16316 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16317 | `		if( rc == 0 ){` |
|        - | 16318 | `			/* Stream device found */` |
|        3 | 16319 | `			*pzDevice = zNext;` |
|        3 | 16320 | `			return pStream;` |
|        - | 16321 | `		}` |
|      ! 0 | 16322 | `	}` |
|        - | 16323 | `	/* No such stream,return NULL */` |
|      ! 0 | 16324 | `	return 0;` |
|    14560 | 16325 |  |
|        - | 16326 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16327 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16328 |  |
